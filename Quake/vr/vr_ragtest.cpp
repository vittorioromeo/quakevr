// vr_ragtest.cpp -- the active ragdolls' tests: see vr_ragtest.hpp.
//
// Adding a behaviour (a new stage's: a backpedal, a pull-up from a ledge, a hop on one leg): a row in `actions` below,
// its Fire (how it is set going on the monster: the QC's test aids, vr_test_kill / vr_test_cut / vr_test_hit /
// VR_Knockdown_Test, each taking the target alone through QC's vr_test_only; a place found; a chance forced through
// `force`) and its Began (what of ragactive::State says it is under way). The menu row, the console name and the headless
// test (Misc/quakevr/ragdoll/ragtest_test.sh, which reads this table) follow.

#include "vr_ragtest.hpp"
#include "vr_trace.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_hitreact.hpp"
#include "vr_progs.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr const char* prefix = "vr_ragdoll_act "; // (each action's command: its name after it)
constexpr float wallSearch = 384.f;  // units: a wall this near his chest he is moved against
constexpr float wallGap = 20.f;      // units: his box this far off it (env_test.sh's wall case: 24)
constexpr float ledgeSearch = 512.f; // units: a ledge's lip this near (his way to it clear)
constexpr float ledgeDrop = 96.f;    // units: a drop at least this deep past it
constexpr float stepUp = 18.f;       // units: a floor within this of his feet is his floor
constexpr float stumbleLeast = 160.f; // units/s: a test's stumble at least this fast (1.5 times Stumble From)
constexpr double forceFor = 20.0;    // s: a test's forcing lasts at most (a swim's: its strokes)
constexpr double getUpAfter = 2.5;   // s: a get-up's ask after its knockdown
constexpr double placeSettle = 0.15; // s: moved to a wall or a ledge (or turned), killed after this
constexpr float throwHealth = 0.2f; // a throw's monster left with at most this share of its full health
constexpr float catchPush = 2.f;    // Killing Blows for a catch's shot in the back
constexpr float twoPi = 6.2831853f;

struct Run;
using Fire = bool (*)(edict_t* target, Run& run); // set going (or its next step); false: it couldn't (said why)
using Began = bool (*)(const Run& run, const ragactive::State* s, const char*& detail); // whether it began, and how

struct Action
{
    const char* command; // "vr_ragdoll_act <name>": the menu row's command
    const char* group;
    const char* label;
    const char* help;
    const char* needs; // the settings it needs on (cvar names, space-separated)
    Fire fire;
    Began began;
    float within; // s: it should have begun by then
    bool zombie{false}; // a zombie can (it dies only beheaded and never crawls: its deaths' tests pass it by)
};

struct Run
{
    const Action* action{nullptr};
    int num{0};
    double fired{0.0};
    Fire then{nullptr}; // a later step (a get-up after its knockdown), at `thenAt` (server time)
    double thenAt{0.0};
    bool done{false};
};

// The tests under way and the forcings they set; how many were fired, began, missed (`vr_ragdoll_act status`); the
// server time last looked at (less now: a new map).
struct Tests
{
    za::Vector<Run> runs;
    za::Vector<ragtest::Force> forces;
    int fired{0}, began{0}, missed{0};
    double last{0.0};
};
Tests tests;

// A setting changed for a call's length (a test's: Knockdowns' Chance, Killing Blows), set back as it was after.
[[nodiscard]] glm::vec3 vec(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

[[nodiscard]] const char* nameOf(const Action& a)
{
    return a.command + strlen(prefix);
}

[[nodiscard]] const char* classOf(int num)
{
    return PR_GetString(EDICT_NUM(num)->v.classname);
}

[[nodiscard]] float yawOf(const glm::vec3& dir)
{
    return glm::degrees(za::atan2(dir.y, dir.x));
}

// QC function `name` run now, self the first player, its first parameter an entity (`ent`, null: world) or a float.
// False: no such function in the progs.
enum class Parm
{
    None,
    Entity,
    Float
};
bool callQc(const char* name, Parm kind = Parm::None, edict_t* ent = nullptr, float value = 0.f)
{
    const func_t fn = progs::findFunction(name);
    if(!fn)
    {
        Con_Printf("ragact: no %s in the progs (another build's?)\n", name);
        return false;
    }
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
    if(kind == Parm::Entity)
    {
        edict_t* const parm = ent ? ent : EDICT_NUM(0);
        G_INT(OFS_PARM0) = EDICT_TO_PROG(parm);
    }
    else if(kind == Parm::Float)
    {
        G_FLOAT(OFS_PARM0) = value;
    }
    PR_ExecuteProgram(fn);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return true;
}

// A QC test aid on `target` alone (QC's vr_test_only): `frame` (VR_TestKill_Frame, ...) run now with its cvar `cvar` at
// `value` (it sets it back to 0).
bool qcTest(cvar_t& cvar, const char* frame, edict_t* target, float value)
{
    callQc("VR_TestOnly", Parm::Entity, target);
    Cvar_SetValueQuick(&cvar, value);
    const bool ok = callQc(frame);
    Cvar_SetValueQuick(&cvar, 0.f); // (if it didn't)
    callQc("VR_TestOnly", Parm::Entity, nullptr);
    return ok;
}

// QC's VR_Knockdown_Test `mode` (the engine's vr_knockdown_test) on `target` alone.
bool knockdownTest(edict_t* target, float mode)
{
    callQc("VR_TestOnly", Parm::Entity, target);
    const bool ok = callQc("VR_Knockdown_Test", Parm::Float, nullptr, mode);
    callQc("VR_TestOnly", Parm::Entity, nullptr);
    return ok;
}

ragtest::Force& force(int num)
{
    for(ragtest::Force& f : tests.forces)
    {
        if(f.num == num)
        {
            f.until = sv.qcvm.time + forceFor;
            return f;
        }
    }
    tests.forces.pushBack(ragtest::Force{});
    ragtest::Force& f = tests.forces.back();
    f.num = num;
    f.until = sv.qcvm.time + forceFor;
    return f;
}

// ---- Places (a wall behind him, a ledge's lip) ----

// The floor under `at` within `depth` units (its height), or -1e30: none.
[[nodiscard]] float floorUnder(const glm::vec3& at, float depth, edict_t* pass)
{
    const trace_t tr = worldtrace::serverLine(at, at - glm::vec3{0.f, 0.f, depth}, pass);
    return tr.fraction < 1.f && !tr.startsolid ? tr.endpos[2] : -1e30f;
}

// `ent` standing at `at` level (its feet on the floor there, within stepUp of `feet`), if its box has room there: its
// origin then in `out`.
[[nodiscard]] bool standAt(edict_t* ent, glm::vec3 at, float feet, glm::vec3& out)
{
    const float f = floorUnder(glm::vec3{at.x, at.y, feet + stepUp}, 2.f * stepUp, ent);
    if(f < -1e29f)
    {
        return false;
    }
    at.z = f - ent->v.mins[2] + 1.f;
    vec3_t p{at.x, at.y, at.z};
    const trace_t tr = SV_Move(p, ent->v.mins, ent->v.maxs, p, MOVE_NOMONSTERS, ent);
    out = at;
    return !tr.startsolid && !tr.allsolid;
}

void moveTo(edict_t* ent, const glm::vec3& at, float yaw)
{
    ent->v.origin[0] = at.x;
    ent->v.origin[1] = at.y;
    ent->v.origin[2] = at.z;
    ent->v.angles[1] = yaw;
    ent->v.ideal_yaw = yaw;
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    SV_LinkEdict(ent, false);
    force(NUM_FOR_EDICT(ent)).still = true; // (moved, not walking: no stumble from the jump)
}

// `ent` moved to stand with its back to the nearest wall round it (wallGap off it), facing away; false: none.
bool toWall(edict_t* ent)
{
    const glm::vec3 o = vec(ent->v.origin);
    const float half = za::max(ent->v.maxs[0], ent->v.maxs[1]);
    const float feet = o.z + ent->v.mins[2];
    const glm::vec3 chest{o.x, o.y, feet + 0.7f * (ent->v.maxs[2] - ent->v.mins[2])};
    float bestDist = 1e30f;
    glm::vec3 bestAt{0.f}, bestN{0.f};
    for(int i = 0; i < 16; i++)
    {
        const float a = twoPi * static_cast<float>(i) / 16.f;
        const glm::vec3 dir{za::cos(a), za::sin(a), 0.f};
        const trace_t tr = worldtrace::serverLine(chest, chest + dir * wallSearch, ent);
        if(tr.fraction >= 1.f || tr.startsolid || za::abs(tr.plane.normal[2]) > 0.3f)
        {
            continue;
        }
        const glm::vec3 n = glm::normalize(glm::vec3{tr.plane.normal[0], tr.plane.normal[1], 0.f});
        glm::vec3 at{0.f};
        if(tr.fraction * wallSearch < bestDist && standAt(ent, vec(tr.endpos) + n * (half + wallGap), feet, at))
        {
            bestDist = tr.fraction * wallSearch;
            bestAt = at;
            bestN = n;
        }
    }
    if(bestDist > 1e29f)
    {
        return false;
    }
    moveTo(ent, bestAt, yawOf(bestN));
    return true;
}

// `ent` moved to the nearest ledge's lip round it (its way there clear, a drop of ledgeDrop past it), its back to the
// drop, its box a little over it; false: none.
bool toLedge(edict_t* ent)
{
    const glm::vec3 o = vec(ent->v.origin);
    const float half = za::max(ent->v.maxs[0], ent->v.maxs[1]);
    const float feet = o.z + ent->v.mins[2];
    float bestDist = 1e30f;
    glm::vec3 bestAt{0.f}, bestDir{0.f};
    for(int i = 0; i < 16; i++)
    {
        const float a = twoPi * static_cast<float>(i) / 16.f;
        const glm::vec3 dir{za::cos(a), za::sin(a), 0.f};
        float ground = -1.f;
        for(float s = 8.f; s <= ledgeSearch; s += 8.f)
        {
            const glm::vec3 q = o + dir * s;
            if(worldtrace::serverLine(o + dir * (s - 8.f), q, ent).fraction < 1.f)
            {
                break; // (a wall)
            }
            const float f = floorUnder(q, (o.z - feet) + ledgeDrop + 64.f, ent);
            if(f > -1e29f && za::abs(f - feet) <= stepUp)
            {
                ground = s;
                continue;
            }
            glm::vec3 at{0.f};
            if(ground > 0.f && (f < -1e29f || feet - f >= ledgeDrop) && ground < bestDist &&
                standAt(ent, o + dir * (ground + 10.f - half), feet, at))
            {
                bestDist = ground;
                bestAt = at;
                bestDir = dir;
            }
            break; // (a step, or the lip)
        }
    }
    if(bestDist > 1e29f)
    {
        return false;
    }
    moveTo(ent, bestAt, yawOf(-bestDir));
    return true;
}

// ---- Fires ----

bool kill(edict_t* t, float part)
{
    qcTest(vr_test_kill, "VR_TestKill_Frame", t, part);
    if(t->v.health > 0.f)
    {
        Con_Printf("ragact: %s (%d) wasn't killed\n", PR_GetString(t->v.classname), NUM_FOR_EDICT(t));
        return false;
    }
    return true;
}

bool fireBody(edict_t* t, Run&)
{
    return kill(t, 1.f);
}

bool fireHead(edict_t* t, Run&)
{
    return kill(t, 2.f);
}

bool fireArm(edict_t* t, Run&)
{
    return kill(t, 3.f);
}

bool fireLeg(edict_t* t, Run&)
{
    return kill(t, 4.f);
}

bool fireWound(edict_t* t, Run& run)
{
    force(run.num).wound = true;
    return kill(t, 1.f);
}

bool fireStumble(edict_t* t, Run& run)
{
    // Killed as if running at you (his facing), no kill push: carried on, a leg stepping ahead.
    const float yaw = glm::radians(t->v.angles[1]);
    const float speed = za::max(1.5f * vr_ragdoll_active_stumble.value, stumbleLeast);
    force(run.num).walk = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f} * speed;
    const cvars::Override push{vr_kill_push, 0.f};
    return kill(t, 1.f);
}

// `t` turned away from the first player where it stands (shot in the back then, it falls forwards).
void turnAway(edict_t* t)
{
    moveTo(t, vec(t->v.origin), yawOf(vec(t->v.origin) - vec(EDICT_NUM(1)->v.origin)));
}

bool killBody(edict_t* t, Run&); // (below)

bool fireCrawl(edict_t* t, Run& run)
{
    // Turned away and shot in the back: he lands face down, as a crawl begins (on his back he must roll over first,
    // and may not: a tall body propped on its arm).
    force(run.num).crawl = true;
    turnAway(t);
    run.then = killBody;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

bool cut(edict_t* t, float n)
{
    qcTest(vr_test_cut, "VR_TestCut_Frame", t, n);
    if(t->v.health > 0.f)
    {
        Con_Printf("ragact: %s (%d) wasn't cut down (Gore > Limb Gore off?)\n", PR_GetString(t->v.classname), NUM_FOR_EDICT(t));
        return false;
    }
    return true;
}

bool fireCutArm(edict_t* t, Run&)
{
    return cut(t, 1.f);
}

bool fireCutForearm(edict_t* t, Run&)
{
    return cut(t, 6.f);
}

bool fireCutLeg(edict_t* t, Run&)
{
    return cut(t, 3.f);
}

bool fireCutShin(edict_t* t, Run&)
{
    return cut(t, 7.f);
}

bool fireCutLegs(edict_t* t, Run&)
{
    return cut(t, 5.f);
}

bool fireCutHead(edict_t* t, Run&)
{
    return cut(t, 8.f);
}

[[nodiscard]] ragactive::Liquid liquidOf(int contents)
{
    return contents == CONTENTS_WATER ? ragactive::Liquid::Water :
           contents == CONTENTS_SLIME ? ragactive::Liquid::Slime :
           contents == CONTENTS_LAVA  ? ragactive::Liquid::Lava :
                                        ragactive::Liquid::None;
}

// Killed in liquid `want`: the one he stands in, else forced on dry land (his drive feels it everywhere; no buoyancy).
bool swim(edict_t* t, Run& run, ragactive::Liquid want)
{
    vec3_t feet{t->v.origin[0], t->v.origin[1], t->v.origin[2] + t->v.mins[2] + 4.f};
    if(liquidOf(SV_PointContents(feet)) != want && liquidOf(SV_PointContents(t->v.origin)) != want)
    {
        force(run.num).liquid = want;
        Con_Printf("ragact: no such liquid where he stands: felt on dry land (his strokes alone, no buoyancy)\n");
    }
    return kill(t, 1.f);
}

bool fireWater(edict_t* t, Run& run)
{
    return swim(t, run, ragactive::Liquid::Water);
}

bool fireSlime(edict_t* t, Run& run)
{
    return swim(t, run, ragactive::Liquid::Slime);
}

bool fireLava(edict_t* t, Run& run)
{
    return swim(t, run, ragactive::Liquid::Lava);
}

// (Moved first, killed a moment later: his place and facing drawn and settled before his ragdoll is made from them.)
bool killBody(edict_t* t, Run&)
{
    return kill(t, 1.f);
}

bool killNoPush(edict_t* t, Run&)
{
    const cvars::Override push{vr_kill_push, 0.f}; // (he topples over the lip by his death animation, as env_test.sh's)
    return kill(t, 1.f);
}

bool killHardPush(edict_t* t, Run& run)
{
    // (Thrown forwards: harder, his pelvis not pulled along his death animation's, which may fall back.)
    force(run.num).noPin = true;
    const cvars::Override push{vr_kill_push, catchPush};
    return kill(t, 1.f);
}

bool fireLean(edict_t* t, Run& run)
{
    if(!toWall(t))
    {
        Con_Printf("ragact: no wall within %.0f units of him with room to stand there\n", wallSearch);
        return false;
    }
    run.then = killBody;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

bool fireLedge(edict_t* t, Run& run)
{
    if(!toLedge(t))
    {
        Con_Printf("ragact: no ledge (a drop of %.0f units or more) within %.0f units of him, his way to it clear\n", ledgeDrop,
            ledgeSearch);
        return false;
    }
    run.then = killNoPush;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

bool fireCatch(edict_t* t, Run& run)
{
    // Turned away from you and shot in the back, hard: he falls forwards, onto his hands.
    turnAway(t);
    run.then = killHardPush;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

bool hit(edict_t* t, float part)
{
    qcTest(vr_test_hit, "VR_TestHit_Frame", t, part);
    return true;
}

bool fireHitBody(edict_t* t, Run&)
{
    return hit(t, 1.f);
}

bool fireHitHead(edict_t* t, Run&)
{
    return hit(t, 2.f);
}

bool fireHitArm(edict_t* t, Run&)
{
    return hit(t, 3.f);
}

bool fireHitLeg(edict_t* t, Run&)
{
    return hit(t, 4.f);
}

bool fireStagger(edict_t* t, Run&)
{
    const cvars::Override chance{vr_knockdown_chance, 0.01f}; // (its balance all but full: it recovers)
    return knockdownTest(t, 21.f);
}

bool fireStaggerFall(edict_t* t, Run&)
{
    const cvars::Override chance{vr_knockdown_chance, 100.f};
    return knockdownTest(t, 22.f);
}

bool fireThrow(edict_t* t, Run& run)
{
    // (Hurt first: a death knight or an ogre is thrown only below Hurt Below, Combat > Holding Enemies.)
    t->v.health = za::max(za::min(t->v.health, throwHealth * t->v.max_health), 1.f);
    Cbuf_InsertText(va("vr_foegrab_throw_test 0 0 %d\n", run.num));
    return true;
}

bool getUpNow(edict_t* t, Run&)
{
    return knockdownTest(t, 1.f);
}

bool fireGetUp(edict_t* t, Run& run)
{
    run.then = getUpNow;
    run.thenAt = sv.qcvm.time + getUpAfter;
    return knockdownTest(t, 0.f);
}

// Drowning (vr_ragdoll_active_drown): out of breath now (QC's VR_Knockdown_Drown, its chance made sure): knocked down,
// its living swim frantic. Its head not under water: felt on dry land.
bool fireDrown(edict_t* t, Run& run)
{
    vec3_t head{t->v.origin[0], t->v.origin[1], t->v.origin[2] + t->v.maxs[2] - 4.f};
    if(liquidOf(SV_PointContents(head)) != ragactive::Liquid::Water)
    {
        force(run.num).liquid = ragactive::Liquid::Water;
        Con_Printf("ragact: his head not under water: felt on dry land (his strokes alone, no buoyancy)\n");
    }
    const cvars::Override chance{vr_ragdoll_active_drown, 1.f};
    return callQc("VR_Knockdown_Drown", Parm::Entity, t);
}

// (At the lip, his back to the drop: knocked down with a little push back over it.)
bool toppleOver(edict_t* t, Run&)
{
    const float yaw = glm::radians(t->v.angles[1]);
    t->v.velocity[0] = -za::cos(yaw) * 70.f;
    t->v.velocity[1] = -za::sin(yaw) * 70.f;
    t->v.velocity[2] = 30.f;
    const cvars::Override push{vr_knockdown_push, 1.f};
    return callQc("VR_Knockdown_Start", Parm::Entity, t);
}

// A ledge pull-up (vr_ragdoll_active_pullup, by its chance): moved to the nearest lip, his back to
// the drop, knocked down over it.
bool firePullUp(edict_t* t, Run& run)
{
    if(!toLedge(t))
    {
        Con_Printf("ragact: no ledge (a drop of %.0f units or more) within %.0f units of him, his way to it clear\n", ledgeDrop,
            ledgeSearch);
        return false;
    }
    run.then = toppleOver;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

// Grabbing another (vr_ragdoll_active_grab): the monster nearest him put just behind him (away from you), then he is
// shot: falling back, he grabs at it (its chance made sure for this death).
bool fireGrab(edict_t* t, Run& run)
{
    const glm::vec3 o = vec(t->v.origin);
    edict_t* other = nullptr;
    float best = 1024.f;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e == t || e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f || e->v.takedamage == 0.f)
        {
            continue;
        }
        const float d = glm::distance(vec(e->v.origin), o);
        if(d < best)
        {
            best = d;
            other = e;
        }
    }
    if(!other)
    {
        Con_Printf("ragact: no other monster within 1024 units of him to grab (spawn one)\n");
        return false;
    }
    glm::vec3 away = o - vec(EDICT_NUM(1)->v.origin);
    away.z = 0.f;
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 at{0.f};
    const float gap = za::max(t->v.maxs[0], 16.f) + za::max(other->v.maxs[0], 16.f) + 6.f;
    if(!standAt(other, o + away * gap, o.z + t->v.mins[2], at))
    {
        Con_Printf("ragact: no room behind him for %s\n", PR_GetString(other->v.classname));
        return false;
    }
    moveTo(other, at, yawOf(-away));
    force(run.num).grab = true; // (its chance made sure for this death)
    run.then = killBody;
    run.thenAt = sv.qcvm.time + placeSettle;
    return true;
}

// Guarding the head (vr_ragdoll_active_guard): as from a swing of yours (QC's vr_test_guard on him alone).
bool fireGuard(edict_t* t, Run&)
{
    return qcTest(vr_test_guard, "VR_Guard_Frame", t, 1.f);
}

// ---- Began ----

[[nodiscard]] int handMode(const ragactive::State* s, ragactive::Grip g)
{
    return s->env.hand[0].mode == g ? 0 : s->env.hand[1].mode == g ? 1 : -1;
}

bool deathBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    if(!s || !s->death.on)
    {
        return false;
    }
    const ragactive::Death& d = s->death;
    detail = d.collapse ? "a collapse" : d.stumble ? "stumbling" : d.woundArm >= 0 ? "a hand on the wound" :
             d.crawl ? "to crawl" : "his death animation, fading";
    return true;
}

bool collapseBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "a collapse";
    return s && s->death.on && s->death.collapse;
}

bool woundBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = s && s->death.woundArm == 0 ? "the left hand on the wound" : "the right hand on the wound";
    return s && s->death.on && s->death.woundArm >= 0;
}

bool stumbleBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "stumbling on, a leg stepping ahead";
    return s && s->death.on && s->death.stumble;
}

bool crawlBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "crawling";
    return s && s->death.on && s->death.crawl && s->death.crawlFrom >= 0.0;
}

bool stumpBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "a hand to the stump";
    return s && s->dismember.on && s->dismember.holdArm >= 0;
}

bool cutBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    if(!s || !s->dismember.on)
    {
        return false;
    }
    detail = s->dismember.standLeg >= 0 ? "balancing on one leg" : s->dismember.holdArm >= 0 ? "a hand to the stump" :
                                                                                               "writhing";
    return true;
}

bool balanceBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "balancing on one leg";
    return s && s->dismember.on && s->dismember.standLeg >= 0;
}

bool leglessBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "to crawl on his arms";
    return s && s->dismember.on && s->death.crawl;
}

bool beheadBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    // (A beheading collapses him, or makes him limp at once: no death animation to drive.)
    detail = s && s->on ? "a collapse" : "limp at once";
    return s && (!s->on || s->death.collapse);
}

template <ragactive::Liquid L>
bool swimBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = L == ragactive::Liquid::Water ? "paddling, his head kept up" : "flailing";
    return s && s->swim.liquid == L && s->swim.from >= 0.0;
}

bool leanBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "a hand on the wall, sliding down it";
    return s && (s->env.leaned || handMode(s, ragactive::Grip::Lean) >= 0);
}

bool ledgeBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "hanging from the lip";
    return s && (s->env.hung || handMode(s, ragactive::Grip::Ledge) >= 0);
}

bool catchBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "his arms out to the floor";
    return s && (s->env.catches > 0 || handMode(s, ragactive::Grip::Catch) >= 0);
}

bool flinchBegan(const Run& run, const ragactive::State*, const char*& detail)
{
    detail = "flinching (its AI going on)";
    return run.num < cl.num_entities && hitreact::reacting(&cl_entities[run.num]);
}

bool staggerBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "staggering, its feet stepping after it";
    return s && s->stagger.on;
}

bool fallBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "going down, its arms bracing";
    return s && s->fall.on;
}

bool throwBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "thrown: slack where held, curling, its arms bracing";
    return s && s->fall.on;
}

bool riseBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "rising through its get-up";
    return s && s->rise.on;
}

bool drownBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "struggling: frantic strokes, its head up";
    return s && s->swim.living && s->swim.panic > 0.f;
}

bool pullUpBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    if(!s || (!s->climb.on && s->climb.ended == nullptr))
    {
        return false;
    }
    detail = !s->climb.on ? s->climb.ended : s->climb.phase == ragactive::ClimbPhase::Hang ? "hanging from the lip" :
             s->climb.phase == ragactive::ClimbPhase::Pull                             ? "pulling itself up" :
                                                                                         "going over the lip";
    return true;
}

bool grabBegan(const Run&, const ragactive::State* s, const char*& detail)
{
    detail = "a hand grabbing at the other";
    return s && (s->env.grabs > 0 || handMode(s, ragactive::Grip::Grab) >= 0);
}

bool guardBegan(const Run& run, const ragactive::State* s, const char*& detail)
{
    if(s && s->guard.on)
    {
        detail = "its body's arms up before its face";
        return true;
    }
    detail = "its arms up before its face (standing: a hit reaction's guard)";
    return run.num < cl.num_entities && hitreact::reacting(&cl_entities[run.num]);
}

// ---- The actions (the menu's rows, in order) ----

constexpr const char* deaths = "vr_ragdoll vr_ragdoll_max vr_ragdoll_active";
constexpr const char* limbs = "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_dismember";

const Action actions[] = {
    {"vr_ragdoll_act death_body", "Deaths", "Shot in the Body",
        "The nearest grunt, enforcer, knight, death knight, ogre or zombie standing killed by a body shot of yours: his "
        "ragdoll plays his death animation as his strength fades, legs first (Gibs and Corpses > Ragdolls: Active Deaths). "
        "The console says when each behaviour began (ragact:).",
        deaths, fireBody, deathBegan, 3.f},
    {"vr_ragdoll_act death_head", "Deaths", "Shot in the Head",
        "A headshot: a collapse, almost no strength from the start (Headshot Strength).", deaths, fireHead, collapseBegan, 3.f},
    {"vr_ragdoll_act death_arm", "Deaths", "Shot in the Arm", "A shot in his arm: his death, a hand perhaps on the wound.",
        deaths, fireArm, deathBegan, 3.f},
    {"vr_ragdoll_act death_leg", "Deaths", "Shot in the Leg", "A shot in his leg: his death, a hand perhaps on the wound.",
        deaths, fireLeg, deathBegan, 3.f},
    {"vr_ragdoll_act wound", "Deaths", "Holding the Wound",
        "A body shot, Hold the Wound's chance made sure for this death: a free hand on the wound as he goes down.",
        deaths, fireWound, woundBegan, 3.f},
    {"vr_ragdoll_act stumble", "Deaths", "Stumbling On",
        "Killed as if running at you (at least 160 units/s, 1.5 times Stumble From), no kill push: carried on a moment, a "
        "leg stepping ahead.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_stumble", fireStumble, stumbleBegan, 3.f},
    {"vr_ragdoll_act crawl", "Deaths", "Crawling Away",
        "A body shot, Crawl Away's chance made sure for this death: once down he rolls onto his front and crawls (Crawl "
        "Time, Crawl Pull). Not a zombie.",
        deaths, fireCrawl, crawlBegan, 8.f},
    {"vr_ragdoll_act cut_arm", "Lost Limbs", "Arm Cut Off",
        "A sword's cut kills him at his left arm: his right hand to the stump, a writhe once down (React to Lost Limbs, "
        "Writhing).",
        limbs, fireCutArm, stumpBegan, 3.f, true},
    {"vr_ragdoll_act cut_forearm", "Lost Limbs", "Forearm Cut Off", "The cut at his left elbow.", limbs, fireCutForearm,
        cutBegan, 3.f, true},
    {"vr_ragdoll_act cut_leg", "Lost Limbs", "Leg Cut Off: One-Leg Balance",
        "The cut at his left hip: he tries to keep his balance on the other leg (Balance on One Leg, Falls Past), then "
        "falls and holds the stump.",
        limbs, fireCutLeg, balanceBegan, 3.f, true},
    {"vr_ragdoll_act cut_shin", "Lost Limbs", "Shin Cut Off", "The cut at his left knee.", limbs, fireCutShin, cutBegan, 3.f, true},
    {"vr_ragdoll_act cut_legs", "Lost Limbs", "Both Legs Cut Off: Crawl",
        "Both legs cut at the hips: he crawls on his arms.", limbs, fireCutLegs, leglessBegan, 4.f},
    {"vr_ragdoll_act cut_head", "Lost Limbs", "Beheaded", "His head cut off: a collapse, or limp at once.",
        "vr_ragdoll vr_ragdoll_max", fireCutHead, beheadBegan, 3.f, true},
    {"vr_ragdoll_act swim_water", "Liquids", "Drowning: Paddle",
        "Killed in water: he paddles and kicks, his head kept up, weakening (Paddle Time). Not standing in water, it is "
        "felt on dry land: his strokes alone, no buoyancy.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_liquids", fireWater,
        swimBegan<ragactive::Liquid::Water>, 4.f},
    {"vr_ragdoll_act swim_lava", "Liquids", "Lava: Flail",
        "Killed in lava: he flails, short and violent (Flail Time). Not standing in lava, it is felt on dry land.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_liquids", fireLava,
        swimBegan<ragactive::Liquid::Lava>, 4.f},
    {"vr_ragdoll_act swim_slime", "Liquids", "Slime: Flail",
        "Killed in slime: he flails for 1.4 times Flail Time. Not standing in slime, it is felt on dry land.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_liquids", fireSlime,
        swimBegan<ragactive::Liquid::Slime>, 4.f},
    {"vr_ragdoll_act lean", "Walls and Ledges", "Lean on a Wall and Slide",
        "He is moved to the nearest wall (within 384 units), his back to it, and shot: a hand braced on it, he slumps "
        "down it (Lean on Walls, Grip).",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_env vr_ragdoll_active_env_lean", fireLean, leanBegan,
        3.f},
    {"vr_ragdoll_act ledge", "Walls and Ledges", "Grab a Ledge and Hang",
        "He is moved to the nearest ledge's lip (a drop of 96 units or more within 512), his back to the drop, and shot "
        "with no kill push: he topples over it, grabs the lip and hangs (Grab Ledges, Hang Time).",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_env vr_ragdoll_active_env_ledge", fireLedge,
        ledgeBegan, 4.f},
    {"vr_ragdoll_act catch", "Walls and Ledges", "Catch the Fall",
        "He is turned away from you and shot in the back, hard: falling forwards, his arms go out to the floor (Catch the "
        "Fall). About half his deaths (the grunt's second) fall back whatever: no catch then (the console says so); press "
        "again.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_env vr_ragdoll_active_env_brace", fireCatch,
        catchBegan, 3.f},
    {"vr_ragdoll_act hit_body", "Living Monsters", "Flinch: Body",
        "A shot in his body that doesn't kill (vr_test_hit_damage): he flinches and fights on (Hit Reactions).",
        "vr_hit_react", fireHitBody, flinchBegan, 1.5f, true},
    {"vr_ragdoll_act hit_head", "Living Monsters", "Flinch: Head Snaps Back", "A shot in his head that doesn't kill.",
        "vr_hit_react", fireHitHead, flinchBegan, 1.5f, true},
    {"vr_ragdoll_act hit_arm", "Living Monsters", "Flinch: Arm Swings", "A shot in his arm that doesn't kill.", "vr_hit_react",
        fireHitArm, flinchBegan, 1.5f, true},
    {"vr_ragdoll_act hit_leg", "Living Monsters", "Knee Buckle and Limp",
        "A shot in his leg that doesn't kill: the knee buckles, the torso leans over it, his steps shorter a moment with a "
        "hitch to that side (Hits Slow Them).",
        "vr_hit_react", fireHitLeg, flinchBegan, 1.5f, true},
    {"vr_ragdoll_act stagger", "Living Monsters", "Stagger and Recover",
        "Shoved with one hand, Knockdowns' Chance 0.01 for this shove: he staggers, his feet stepping after him, and "
        "recovers (Active Shoves and Throws).",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_knockdown vr_ragdoll_active_stagger", fireStagger, staggerBegan, 2.f, true},
    {"vr_ragdoll_act stagger_fall", "Living Monsters", "Stagger and Fall",
        "Shoved with two hands, Knockdowns' Chance 100 for this shove: he staggers a step or two and goes over, his arms "
        "bracing for the floor (Stagger Falls Past).",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_knockdown vr_ragdoll_active_stagger", fireStaggerFall, fallBegan, 3.f, true},
    {"vr_ragdoll_act throw", "Living Monsters", "Two-Hand Throw (Judo)",
        "Thrown to your left as both hands' turn would (vr_foegrab_throw_test): slack where held, curling as he goes over.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_stagger", fireThrow, throwBegan, 2.f, true},
    {"vr_ragdoll_act getup", "Living Monsters", "Knocked Down, Then Get Up",
        "Knocked down, then asked to get up 2.5 s later: he rolls over if he must, pulls himself into his get-up and rises "
        "through it (Active Get-Ups).",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_knockdown vr_ragdoll_active_getup", fireGetUp, riseBegan, 7.f, true},
    {"vr_ragdoll_act drown", "Living Monsters", "Drowning Struggle",
        "Out of breath under water now (Drowning Struggle's chance made sure): knocked down, he paddles frantically for "
        "the surface, his head up, towards where he can stand. His head not under water, it is felt on dry land.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_knockdown vr_ragdoll_active_liquids vr_ragdoll_active_drown",
        fireDrown, drownBegan, 2.f, true},
    {"vr_ragdoll_act pullup", "Living Monsters", "Fall Off a Ledge, Pull Up",
        "He is moved to the nearest ledge's lip (a drop of 96 units or more within 512), his back to the drop, and knocked "
        "down over it: he grabs the lip, hangs, and (Pull Up From Ledges' chance) pulls himself up onto the top.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_knockdown vr_ragdoll_active_env vr_ragdoll_active_env_ledge "
        "vr_ragdoll_active_pullup",
        firePullUp, pullUpBegan, 4.f, true},
    {"vr_ragdoll_act grab", "Living Monsters", "Grab Another Falling",
        "The monster nearest him is put just behind him (away from you) and he is shot: falling back, he grabs at it (its "
        "chance made sure: Grab Others Falling), a hand on it a moment. Needs a second monster.",
        "vr_ragdoll vr_ragdoll_max vr_ragdoll_active vr_ragdoll_active_env vr_ragdoll_active_grab", fireGrab, grabBegan, 3.f},
    {"vr_ragdoll_act guard", "Living Monsters", "Guard the Head",
        "As when your hand swings at his head: his arms up before his face for 0.6 s (standing: Hit Reactions on Limbs; "
        "staggering, down or rising: his body's arms).",
        "vr_ragdoll_active_guard", fireGuard, guardBegan, 1.f, true},
};
constexpr int actionCount = static_cast<int>(za::getArraySize(actions));

[[nodiscard]] const Action* actionNamed(const char* name)
{
    for(const Action& a : actions)
    {
        if(!q_strcasecmp(nameOf(a), name))
        {
            return &a;
        }
    }
    return nullptr;
}

// The first setting of `needs` (cvar names) that is off, or null.
[[nodiscard]] const cvar_t* offSetting(const char* needs)
{
    char name[64];
    for(const char* p = needs; *p;)
    {
        size_t n = 0;
        while(p[n] && p[n] != ' ' && n + 1 < sizeof(name))
        {
            name[n] = p[n];
            n++;
        }
        name[n] = '\0';
        p += n;
        while(*p == ' ')
        {
            p++;
        }
        const cvar_t* var = Cvar_FindVar(name);
        if(var && var->value == 0.f)
        {
            return var;
        }
    }
    return nullptr;
}

// Whether action `a` can be tried on `e`: a live monster of a supported kind standing (not a ragdoll now; a zombie only for
// the actions it can do).
[[nodiscard]] bool takes(const Action& a, edict_t* e)
{
    return !e->free && (static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f && box3d::activeKind(e) &&
           !box3d::isRagdoll(NUM_FOR_EDICT(e)) && (a.zombie || strcmp(PR_GetString(e->v.classname), "monster_zombie"));
}

// The nearest monster `a` takes to the first player; its distance.
[[nodiscard]] edict_t* nearestTarget(const Action& a, float& dist)
{
    const glm::vec3 from = vec(EDICT_NUM(1)->v.origin);
    edict_t* best = nullptr;
    dist = 1e30f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(!takes(a, e))
        {
            continue;
        }
        const float d = glm::distance(vec(e->v.origin), from);
        if(d < dist)
        {
            dist = d;
            best = e;
        }
    }
    return best;
}

void list()
{
    Con_Printf("vr_ragdoll_act <name> [entity]: on the nearest grunt, enforcer, knight, death knight, ogre or zombie standing "
               "(or that entity); status: how many began. Names:\n");
    const char* group = "";
    for(const Action& a : actions)
    {
        if(strcmp(group, a.group))
        {
            group = a.group;
            Con_Printf("  %s:\n", group);
        }
        Con_Printf("    %-14s %s\n", nameOf(a), a.label);
    }
}

void act_f()
{
    if(Cmd_Argc() < 2)
    {
        list();
        return;
    }
    if(!q_strcasecmp(Cmd_Argv(1), "status"))
    {
        int going = 0;
        for(const Run& r : tests.runs)
        {
            going += r.done ? 0 : 1;
        }
        Con_Printf("ragact: status: %d fired, %d began, %d missed, %d under way\n", tests.fired, tests.began, tests.missed,
            going);
        return;
    }
    const Action* a = actionNamed(Cmd_Argv(1));
    if(!a)
    {
        Con_Printf("ragact: no action %s (vr_ragdoll_act: the list)\n", Cmd_Argv(1));
        return;
    }
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("ragact: %s: not in a game\n", nameOf(*a));
        return;
    }
    const progs::ServerVm vm;
    if(EDICT_NUM(1)->free)
    {
        Con_Printf("ragact: %s: no player\n", nameOf(*a));
        return;
    }
    tests.fired++;
    if(const cvar_t* off = offSetting(a->needs))
    {
        Con_Printf("ragact: %s: not begun: it needs %s on (it is %s)\n", nameOf(*a), off->name, off->string);
        tests.missed++;
        return;
    }
    float dist = 0.f;
    edict_t* t = nullptr;
    if(Cmd_Argc() > 2)
    {
        const int n = Q_atoi(Cmd_Argv(2));
        t = n > svs.maxclients && n < qcvm->num_edicts ? EDICT_NUM(n) : nullptr;
        t = t && takes(*a, t) ? t : nullptr;
        dist = t ? glm::distance(vec(t->v.origin), vec(EDICT_NUM(1)->v.origin)) : 0.f;
    }
    else
    {
        t = nearestTarget(*a, dist);
    }
    if(!t)
    {
        Con_Printf("ragact: %s: not begun: no grunt, enforcer, knight, death knight, ogre%s standing (Debug > Tests > Ragdoll "
                   "Behaviours: A Grunt Ahead)\n",
            nameOf(*a), a->zombie ? " or zombie" : " (a zombie dies only beheaded)");
        tests.missed++;
        return;
    }
    // (Fired in the server's frame, as QC's own test aids are: what it sends the clients, a blow's flinch, its blood, would
    // be cleared before it was sent from a console command's.)
    Run run;
    run.action = a;
    run.num = NUM_FOR_EDICT(t);
    run.fired = -1.0;
    run.then = a->fire;
    za::vectorEraseIf(tests.forces, [&](const ragtest::Force& f) { return f.num == run.num; }); // (an earlier test's)
    Con_Printf("ragact: %s on %s (%d), %.0f units away\n", nameOf(*a), classOf(run.num), run.num, dist);
    tests.runs.pushBack(run);
}

} // namespace

const ragtest::Force* ragtest::forced(int num)
{
    for(const Force& f : tests.forces)
    {
        if(f.num == num && sv.qcvm.time < f.until)
        {
            return &f;
        }
    }
    return nullptr;
}

void ragtest::frame()
{
    if(tests.runs.empty() && tests.forces.empty())
    {
        return;
    }
    if(!sv.active || sv.qcvm.time < tests.last)
    {
        tests.runs.clear();
        tests.forces.clear();
        tests.last = sv.active ? sv.qcvm.time : 0.0;
        return;
    }
    tests.last = sv.qcvm.time;
    const progs::ServerVm vm;
    for(Run& run : tests.runs)
    {
        const Action& a = *run.action;
        edict_t* ent = run.num < qcvm->num_edicts ? EDICT_NUM(run.num) : nullptr;
        if(!ent || ent->free)
        {
            Con_Printf("ragact: %s (%d): not begun: it was removed\n", nameOf(a), run.num);
            tests.missed++;
            run.done = true;
            continue;
        }
        if(run.then && sv.qcvm.time >= run.thenAt)
        {
            const Fire then = run.then;
            const bool first = run.fired < 0.0;
            run.then = nullptr;
            run.fired = first ? sv.qcvm.time : run.fired;
            if(!then(ent, run))
            {
                Con_Printf("ragact: %s on %s (%d): not begun (see above)\n", nameOf(a), classOf(run.num), run.num);
                tests.missed++;
                run.done = true;
                continue;
            }
        }
        const char* detail = "";
        const ragactive::State* s = box3d::activeState(run.num);
        if(a.began(run, s, detail))
        {
            Con_Printf("ragact: %s on %s (%d): began after %.2f s: %s\n", nameOf(a), classOf(run.num), run.num,
                sv.qcvm.time - run.fired, detail);
            tests.began++;
            run.done = true;
        }
        else if(sv.qcvm.time - run.fired > static_cast<double>(a.within))
        {
            Con_Printf("ragact: %s on %s (%d): NOT begun in %.1f s (%s)\n", nameOf(a), classOf(run.num), run.num, a.within,
                a.began == flinchBegan ? "no hit reaction drawn: no blow sent, or vr_hit_react off" // (no ragdoll's)
                : !s                   ? "no ragdoll"
                : !s->on               ? "its ragdoll not driven"
                                       : "driven, but not this");
            tests.missed++;
            run.done = true;
        }
    }
    za::vectorEraseIf(tests.runs, [](const Run& r) { return r.done; });
    // (A forcing ends with its time, or its entity removed: its number may be another monster's next.)
    za::vectorEraseIf(tests.forces, [](const ragtest::Force& f) {
        return sv.qcvm.time >= f.until || f.num >= qcvm->num_edicts || EDICT_NUM(f.num)->free;
    });
}

void ragtest::registerCommands()
{
    Cmd_AddCommand("vr_ragdoll_act", act_f);
}

int ragtest::rowCount()
{
    return actionCount;
}

ragtest::Row ragtest::row(int i)
{
    const Action& a = actions[za::clamp(i, 0, actionCount - 1)];
    return Row{a.group, a.label, a.command, a.help};
}
