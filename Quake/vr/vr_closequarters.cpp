// vr_closequarters.cpp -- close-quarters slowdown (see vr_closequarters.hpp). His voice note, 2026-11-01 18:14: "if the
// player walks into an enemy full speed, then it becomes really messy ... it should still be possible for a player to
// get right into an enemy's face, but ... the player speed slowing down if they're very close to an enemy".
//
// SV_AirMove's wish velocity (the stick's, after the most speed's caps), walking: the live enemy (FL_MONSTER, alive,
// damageable, solid) whose box is nearest the player's centre horizontally, at the player's height, within Start Radius
// (vr_closequarters_start, metres): its part towards that enemy times a factor from 1 at Start Radius down to Min Factor
// (vr_closequarters_min) at End Radius (vr_closequarters_end) and nearer, the less the further the move turns from
// straight at it (all of it within vr_closequarters_angle degrees, none past vr_closequarters_angle_falloff); its
// sideways part times 1 .. Strafe Factor (vr_closequarters_strafe) on the same ramp. Moving away: only the sideways part.
//
// Brake Momentum (vr_closequarters_brake; his note 2026-11-02 00:53: "if I run full speed close to the enemy and then
// release the thumbstick, my inertia can still make me basically get into the enemy too quickly"): the player's own
// velocity's part towards that enemy, above the slowed wish's part towards it (0 with the stick let go), shed at Braking
// (vr_closequarters_braking, m/s^2) times the same slowdown's weight on the velocity's own direction (its ramp and angle:
// none past Start Radius or Angle Falloff, all of it at End Radius straight at it), never below the slowed wish's part.
// Sideways untouched. Only his own momentum: on the ground, not sliding from an enemy's shove (QC .vr_eshove_speed) nor
// within pushGrace of a hit's knockback (QC .vr_deathdoll_pushtime); a jump is off the ground.

#include "vr_closequarters.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

using namespace qvr;

namespace
{

// The last walking move a live enemy slowed (or the one that found none), and the approach's extremes since
// vr_closequarters_test start: what the test reads.
struct LastMove
{
    int moves{0};         // walking moves seen
    int slowed{0};        // ... with an enemy within Start Radius
    float gap{0.f};       // the last slowed one's: metres to the enemy's box
    float toward{1.f};    // its factors
    float side{1.f};
    float speedIn{0.f};   // its wish speed before and after (units/s)
    float speedOut{0.f};
    float leastToward{1.f}; // the least toward factor since start
    float leastGap{1e9f};   // the least gap since start
    float farSlowed{0.f};   // the most toward slowdown (1 - factor) at a gap past Start Radius (must stay 0)
    int foe{0};             // the last slowed move's enemy (its edict number)
    float firstGap{-1.f};   // the first slowed move's gap since start (metres)
};
LastMove lastMove;

[[nodiscard]] float startRadius()
{
    return za::max(vr_closequarters_start.value, 0.f);
}

[[nodiscard]] float endRadius()
{
    return za::clamp(vr_closequarters_end.value, 0.f, startRadius());
}

// A live enemy one can walk into: a monster alive, damageable and solid; with Only Live Foes (vr_closequarters_live) not
// a ragdoll either (knocked down, staggering, held up by a hand, getting up through its body: active ragdoll form).
[[nodiscard]] bool liveEnemy(edict_t* e)
{
    const int solid = static_cast<int>(e->v.solid);
    if(!((static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f &&
           static_cast<int>(e->v.takedamage) != DAMAGE_NO && solid != SOLID_NOT && solid != SOLID_TRIGGER))
    {
        return false;
    }
    return vr_closequarters_live.value == 0.f ||
           (!box3d::isRagdoll(NUM_FOR_EDICT(e)) && progs::fieldFloatOr(e, progs::fields().vr_knockdown, 0.f) != 1.f);
}

// The horizontal distance in units from `at` to the box (`lo`, `hi`): 0 inside it.
[[nodiscard]] float boxGap(const glm::vec3& at, const float* lo, const float* hi)
{
    const float dx = za::max(za::max(lo[0] - at.x, at.x - hi[0]), 0.f);
    const float dy = za::max(za::max(lo[1] - at.y, at.y - hi[1]), 0.f);
    return glm::sqrt(dx * dx + dy * dy);
}

// The record of a slowed move (LastMove).
void noteSlowed(const float speedIn, const float speedOut, const float gapM, const closequarters::Factors& f, const int foe)
{
    LastMove& l = lastMove;
    l.slowed++;
    l.gap = gapM;
    l.toward = f.toward;
    l.side = f.side;
    l.speedIn = speedIn;
    l.speedOut = speedOut;
    l.leastToward = za::min(l.leastToward, f.toward);
    l.leastGap = za::min(l.leastGap, gapM);
    l.foe = foe;
    l.firstGap = l.firstGap < 0.f ? gapM : l.firstGap;
    if(gapM >= startRadius())
    {
        l.farSlowed = za::max(l.farSlowed, 1.f - f.toward);
    }
}

constexpr float pushGrace = 0.5f; // s after a hit's knockback pushed the player that his velocity isn't braked

// Whether `ent`'s velocity is its own momentum, for Brake Momentum: on the ground, not sliding from an enemy's shove, no
// hit's knockback within pushGrace.
[[nodiscard]] bool ownMomentum(edict_t* ent)
{
    if(!(static_cast<int>(ent->v.flags) & FL_ONGROUND) ||
        progs::fieldFloatOr(ent, progs::fields().vr_eshove_speed, 0.f) > 0.f)
    {
        return false;
    }
    const float pushed = progs::fieldFloatOr(ent, progs::fields().vr_deathdoll_pushtime, 0.f);
    return pushed <= 0.f || static_cast<float>(qcvm->time) - pushed >= pushGrace;
}

// Brake Momentum: `ent`'s velocity's part towards the enemy (`toEnemy`, its box `gapM` metres off) above the slowed wish
// `out`'s part towards it, shed at Braking times the slowdown's weight on the velocity's direction. Returns the speed
// shed this frame (units/s).
float brakeMomentum(edict_t* ent, const glm::vec3& toEnemy, const float gapM, const glm::vec3& out)
{
    const glm::vec3 vel{ent->v.velocity[0], ent->v.velocity[1], 0.f};
    const float along = glm::dot(vel, toEnemy);
    const float cap = za::max(glm::dot(out, toEnemy), 0.f);
    if(along <= cap)
    {
        return 0.f;
    }
    const closequarters::Factors f = closequarters::factorsFor(vel, toEnemy, gapM);
    const float rate = za::max(vr_closequarters_braking.value, 0.f) * units::metresToUnits() * f.weight;
    const float to = za::max(cap, along - rate * static_cast<float>(host_frametime));
    ent->v.velocity[0] += toEnemy.x * (to - along);
    ent->v.velocity[1] += toEnemy.y * (to - along);
    return along - to;
}

// vr_closequarters_test coast (after "start" and "approach"): leg 1 runs from where "start" stood straight at the grunt
// with Brake Momentum off, lets go of the stick at Start Radius and coasts to a stop; leg 2 the same with it on; leg 3
// strafes beside it (strafeFrames) and runs "end". Each step waits on its own condition (Start Radius reached, stopped,
// frames counted); giveUpFrames in a leg: FAIL "timed out".
struct Coast
{
    int leg{0};                   // 0 none; 1 brake off, 2 brake on, 3 the strafe
    int phase{0};                 // 0 running at it, 1 coasting (the stick let go)
    int frames{0};                // frames in this leg
    glm::vec3 from{0.f};          // where "start" stood
    float entry[2]{0.f, 0.f};     // each leg's speed (units/s) as it let go at Start Radius
    float least[2]{1e9f, 1e9f};   // and the least gap its coast came to (m)
    float brakeWas{1.f};          // vr_closequarters_brake before
    int braked{0};                // leg 2's frames the brake shed speed
    float strafeShed{0.f};        // speed the brake shed in the strafe (units/s; stays 0)
    int foe{0};                   // the grunt "approach" walked into (its edict number)
};
Coast coast;
constexpr int strafeFrames = 12;
constexpr int giveUpFrames = 72 * 6;
constexpr float touchGap = 0.62f; // m: the player's body against a grunt's (16 units)
testkit::Tally cqTally;

// `p` put at `at`, dropped onto the floor below (from 64 units over it), still.
void putAt(edict_t* p, const glm::vec3& at)
{
    vec3_t from{at.x, at.y, at.z + 64.f}, to{at.x, at.y, at.z - 64.f};
    const trace_t tr = SV_Move(from, p->v.mins, p->v.maxs, to, MOVE_NOMONSTERS, p);
    const float z = tr.startsolid ? at.z : tr.endpos[2];
    p->v.origin[0] = at.x;
    p->v.origin[1] = at.y;
    p->v.origin[2] = z;
    p->v.oldorigin[0] = at.x;
    p->v.oldorigin[1] = at.y;
    p->v.oldorigin[2] = z;
    p->v.velocity[0] = p->v.velocity[1] = p->v.velocity[2] = 0.f;
    SV_LinkEdict(p, false);
}

void coastNextLeg(edict_t* ent)
{
    Coast& c = coast;
    c.frames = 0;
    c.phase = 0;
    if(c.leg == 1)
    {
        c.leg = 2;
        Cvar_SetValueQuick(&vr_closequarters_brake, 1.f);
        putAt(ent, c.from);
        Cbuf_AddText("+forward\n");
        return;
    }
    Cvar_SetValueQuick(&vr_closequarters_brake, c.brakeWas);
    constexpr const char* name = "vr_closequarters_test";
    Con_Printf("%s: coast: let go at Start Radius at %.0f u/s (brake off), %.0f (on); stopped at %.2f m (off), %.2f m "
               "(on; braked %d frames, Braking %.0f m/s2)\n",
        name, c.entry[0], c.entry[1], c.least[0], c.least[1], c.braked, vr_closequarters_braking.value);
    cqTally.check(c.entry[1] > 200.f, name, va("the braked leg let go running (%.0f u/s)", c.entry[1]));
    cqTally.check(c.braked > 0, name, "the brake ran");
    cqTally.check(c.least[1] > c.least[0] + 0.1f, name,
        va("braked, it stopped shorter (%.2f m, unbraked %.2f m)", c.least[1], c.least[0]));
    cqTally.check(c.least[1] > touchGap, name, va("braked, it stopped short of the grunt (%.2f m, touching %.2f)", c.least[1], touchGap));
    c.leg = 3;
    Cbuf_AddText("+moveright\n");
}

// Each walking move while the coast runs: `gapM` to the nearest live enemy (huge: none), `shed` what the brake shed.
void coastFrame(edict_t* ent, const float gapM, const float shed)
{
    Coast& c = coast;
    if(c.leg == 0)
    {
        return;
    }
    c.frames++;
    if(c.leg == 3)
    {
        c.strafeShed += shed;
        if(c.frames >= strafeFrames)
        {
            c.leg = 0;
            Cbuf_AddText("-moveright\nvr_closequarters_test end\n");
        }
        return;
    }
    const int i = c.leg - 1;
    c.braked += (i == 1 && shed > 0.f) ? 1 : 0;
    const float speed = glm::length(glm::vec2{ent->v.velocity[0], ent->v.velocity[1]});
    if(c.phase == 0 && gapM <= startRadius())
    {
        c.entry[i] = speed;
        c.phase = 1;
        Cbuf_AddText("-forward\n");
    }
    else if(c.phase == 1)
    {
        c.least[i] = za::min(c.least[i], gapM);
        if(speed < 1.f)
        {
            coastNextLeg(ent);
            return;
        }
    }
    if(c.frames > giveUpFrames)
    {
        cqTally.check(false, "vr_closequarters_test", va("coast leg %d timed out", c.leg));
        Cvar_SetValueQuick(&vr_closequarters_brake, c.brakeWas);
        c.leg = 0;
        Cbuf_AddText("-forward\nvr_closequarters_test end\n");
    }
}

} // namespace

namespace qvr::closequarters
{

Factors factorsFor(const glm::vec3& wish, const glm::vec3& toEnemy, const float gap)
{
    Factors f;
    const float start = startRadius();
    const float end = endRadius();
    if(gap >= start)
    {
        return f;
    }
    const float ramp = start > end ? za::clamp((start - gap) / (start - end), 0.f, 1.f) : 1.f;
    f.side = 1.f - ramp * (1.f - za::clamp(vr_closequarters_strafe.value, 0.f, 1.f));
    const float speed = glm::length(wish);
    const float along = speed > 0.f ? glm::dot(wish, toEnemy) / speed : 0.f;
    if(along <= 0.f)
    {
        return f; // moving away (or still): its part towards nothing
    }
    const float angle = glm::degrees(glm::acos(za::clamp(along, -1.f, 1.f)));
    const float full = za::clamp(vr_closequarters_angle.value, 0.f, 90.f);
    const float none = za::clamp(vr_closequarters_angle_falloff.value, full, 90.f);
    const float weight = angle <= full ? 1.f : (none > full ? za::clamp((none - angle) / (none - full), 0.f, 1.f) : 0.f);
    f.weight = ramp * weight;
    f.toward = 1.f - f.weight * (1.f - za::clamp(vr_closequarters_min.value, 0.f, 1.f));
    return f;
}

} // namespace qvr::closequarters

// SV_AirMove (walking, not noclip): the wish velocity near a live enemy, slowed straight at it (vr_closequarters).
extern "C" void VR_CloseQuartersMove(edict_t* ent, float* wishvel)
{
    if(!ent || vr_closequarters.value == 0.f || static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        return;
    }
    const glm::vec3 wish{wishvel[0], wishvel[1], 0.f};
    const float speedIn = glm::length(wish);
    const bool brake = vr_closequarters_brake.value != 0.f && ownMomentum(ent);
    if(speedIn <= 0.f && !brake && coast.leg == 0)
    {
        return;
    }
    lastMove.moves += speedIn > 0.f ? 1 : 0;
    const float m2u = units::metresToUnits();
    const float start = startRadius() * m2u;
    const glm::vec3 at{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const float zLo = ent->v.absmin[2], zHi = ent->v.absmax[2];
    float gap = start;
    edict_t* const foe = progs::nearestBy(1, start, [&](edict_t* e) {
        if(e == ent || !liveEnemy(e) || e->v.absmax[2] <= zLo || e->v.absmin[2] >= zHi)
        {
            return start;
        }
        return boxGap(at, e->v.absmin, e->v.absmax);
    }, &gap);
    glm::vec3 toEnemy{0.f};
    if(foe)
    {
        toEnemy = glm::vec3{(foe->v.absmin[0] + foe->v.absmax[0]) * 0.5f - at.x, (foe->v.absmin[1] + foe->v.absmax[1]) * 0.5f - at.y, 0.f};
    }
    const float len = glm::length(toEnemy);
    if(len <= 0.f)
    {
        coastFrame(ent, 1e9f, 0.f);
        return;
    }
    toEnemy /= len;
    const float gapM = gap / m2u;
    glm::vec3 out = wish;
    if(speedIn > 0.f)
    {
        const closequarters::Factors f = closequarters::factorsFor(wish, toEnemy, gapM);
        const float along = glm::dot(wish, toEnemy);
        const glm::vec3 towardPart = toEnemy * za::max(along, 0.f);
        out = towardPart * f.toward + (wish - towardPart) * f.side;
        wishvel[0] = out.x;
        wishvel[1] = out.y;
        noteSlowed(speedIn, glm::length(out), gapM, f, NUM_FOR_EDICT(foe));
    }
    coastFrame(ent, gapM, brake ? brakeMomentum(ent, toEnemy, gapM, out) : 0.f);
}

namespace
{

// vr_closequarters_test start | approach | end: "start" clears the record; the script walks straight at a grunt (spawned
// 120 units ahead) and runs "approach": it was slowed only within Start Radius, down to Min Factor at its face; then it
// strafes beside it and runs "end": the sideways move kept Strafe Factor of its speed (no part towards it slowed), and
// the factors' rule on the settings (none past Start Radius, Min Factor at End Radius straight at it, between them
// halfway, Strafe Factor sideways, moving away only the sideways part). "coast" (Coast) runs the brake's legs and the
// strafe itself, then "end". Prints "vr_closequarters_test: N passed, M failed".

void test_f()
{
    constexpr const char* name = "vr_closequarters_test";
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    LastMove& l = lastMove;
    const float minF = za::clamp(vr_closequarters_min.value, 0.f, 1.f);
    const float strafe = za::clamp(vr_closequarters_strafe.value, 0.f, 1.f);
    if(!strcmp(step, "start"))
    {
        l = LastMove{};
        cqTally = testkit::Tally{};
        coast = Coast{};
        if(const edict_t* const p = progs::localPlayer())
        {
            coast.from = glm::vec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
        }
        return;
    }
    if(!strcmp(step, "coast"))
    {
        edict_t* const p = progs::localPlayer();
        if(!p)
        {
            return;
        }
        const progs::ServerVm vm;
        // The legs' start: runUp units short of the grunt "approach" walked into, on the way from the start, on the
        // floor the player stands on now (a map's start may be a step up: off it he flies, and nothing brakes that).
        const edict_t* const foe = coast.foe > 0 && coast.foe < qcvm->num_edicts ? EDICT_NUM(coast.foe) : nullptr;
        glm::vec3 from = coast.from;
        if(foe)
        {
            constexpr float runUp = 140.f;
            const glm::vec3 mid{(foe->v.absmin[0] + foe->v.absmax[0]) * 0.5f, (foe->v.absmin[1] + foe->v.absmax[1]) * 0.5f, p->v.origin[2]};
            glm::vec3 way{mid.x - from.x, mid.y - from.y, 0.f};
            way = glm::length(way) > 0.f ? glm::normalize(way) : glm::vec3{1.f, 0.f, 0.f};
            from = mid - way * (runUp + (foe->v.absmax[0] - foe->v.absmin[0]) * 0.5f);
        }
        coast = Coast{};
        coast.from = from;
        coast.leg = 1;
        coast.brakeWas = vr_closequarters_brake.value;
        Cvar_SetValueQuick(&vr_closequarters_brake, 0.f);
        putAt(p, from);
        Cbuf_AddText("+forward\n");
        return;
    }
    if(!strcmp(step, "approach"))
    {
        if(!sv.active)
        {
            return;
        }
        const progs::ServerVm vm;
        const edict_t* const foe = l.foe > 0 && l.foe < qcvm->num_edicts ? EDICT_NUM(l.foe) : nullptr;
        Con_Printf("%s: approach: %d walking moves, %d slowed (%s #%d, first at %.2f m); nearest %.2f m, least toward "
                   "factor %.3f (Min Factor %.2f)\n",
            name, l.moves, l.slowed, foe ? PR_GetString(foe->v.classname) : "none", l.foe, l.firstGap, l.leastGap,
            l.leastToward, minF);
        cqTally.check(l.moves > l.slowed && l.slowed > 0, name, "the approach walked unslowed far off, slowed near it");
        cqTally.check(l.farSlowed == 0.f, name, "nothing slowed past Start Radius");
        cqTally.check(l.leastGap <= endRadius(), name, va("it reached End Radius (%.2f m, want <= %.2f)", l.leastGap, endRadius()));
        cqTally.check(za::abs(l.leastToward - minF) < 0.01f, name,
            va("straight at its face: slowed to Min Factor (%.3f, want %.2f)", l.leastToward, minF));
        coast.foe = l.foe;
        l = LastMove{};
        return;
    }
    if(!strcmp(step, "live"))
    {
        // Only Live Foes (vr_closequarters_live): the grunt standing counts; knocked down (its ragdoll made, alive) it
        // doesn't, and does again with the option off.
        if(!sv.active)
        {
            return;
        }
        const progs::ServerVm vm;
        edict_t* grunt = nullptr;
        for(int i = 1; i < qcvm->num_edicts && !grunt; i++)
        {
            edict_t* const e = EDICT_NUM(i);
            grunt = !e->free && !strcmp(PR_GetString(e->v.classname), "monster_army") && e->v.health > 0.f ? e : nullptr;
        }
        cqTally.check(grunt != nullptr && liveEnemy(grunt), name, "the grunt standing: a live foe");
        // (the knockdown's mark, .vr_knockdown 1: a living ragdoll's, QC vr_knockdown.qc; the test's grunts here are
        // spawned as targets a sweep does not knock down, so the mark is set for the check and restored)
        const int kd = progs::fields().vr_knockdown;
        if(!grunt || kd < 0)
        {
            cqTally.check(false, name, "a grunt with the knockdown field");
            return;
        }
        const float mark = progs::fieldFloat(grunt, kd);
        progs::fieldFloat(grunt, kd) = 1.f;
        const float was = vr_closequarters_live.value;
        Cvar_SetValueQuick(&vr_closequarters_live, 1.f);
        const bool liveOn = liveEnemy(grunt);
        Cvar_SetValueQuick(&vr_closequarters_live, 0.f);
        const bool liveOff = liveEnemy(grunt);
        Cvar_SetValueQuick(&vr_closequarters_live, was);
        progs::fieldFloat(grunt, kd) = mark;
        Con_Printf("%s: live: knocked down (marked): a foe with Only Live Foes %d, without %d\n", name, liveOn ? 1 : 0,
            liveOff ? 1 : 0);
        cqTally.check(!liveOn, name, "knocked down: no foe with Only Live Foes");
        cqTally.check(liveOff, name, "knocked down: a foe again with Only Live Foes off");
        return;
    }
    Con_Printf("%s: strafe: %d slowed moves, last at %.2f m: toward %.3f, side %.3f, speed %.0f -> %.0f\n", name, l.slowed,
        l.gap, l.toward, l.side, l.speedIn, l.speedOut);
    cqTally.check(l.slowed > 0, name, "the strafe was beside it");
    cqTally.check(l.speedIn > 0.f && l.speedOut >= l.speedIn * (strafe - 0.02f), name,
        va("strafing past it kept its speed (%.0f of %.0f, Strafe Factor %.2f)", l.speedOut, l.speedIn, strafe));
    cqTally.check(coast.strafeShed < 2.f, name, va("strafing past it wasn't braked (%.1f u/s shed)", coast.strafeShed));
    // The rule on the settings, straight, sideways and away.
    const glm::vec3 way{1.f, 0.f, 0.f};
    const float s = startRadius(), e = endRadius();
    using closequarters::factorsFor;
    const closequarters::Factors past = factorsFor(way * 320.f, way, s + 0.05f);
    cqTally.check(past.toward == 1.f && past.side == 1.f, name, "past Start Radius: unchanged");
    const closequarters::Factors atEnd = factorsFor(way * 320.f, way, e);
    cqTally.check(za::abs(atEnd.toward - minF) < 0.001f, name, va("straight at End Radius: Min Factor (%.3f)", atEnd.toward));
    const closequarters::Factors mid = factorsFor(way * 320.f, way, (s + e) * 0.5f);
    cqTally.check(za::abs(mid.toward - (1.f + minF) * 0.5f) < 0.01f, name, va("halfway: halfway (%.3f)", mid.toward));
    const closequarters::Factors sideways = factorsFor(glm::vec3{0.f, 320.f, 0.f}, way, e);
    cqTally.check(za::abs(sideways.side - strafe) < 0.001f && (sideways.toward == 1.f || vr_closequarters_angle_falloff.value >= 90.f),
        name, va("sideways at End Radius: Strafe Factor (%.3f), nothing towards (%.3f)", sideways.side, sideways.toward));
    const closequarters::Factors away = factorsFor(way * -320.f, way, e);
    cqTally.check(away.toward == 1.f, name, "moving away: not slowed");
    cqTally.verdict(name);
}

} // namespace

namespace qvr::closequarters
{

void init()
{
    Cmd_AddCommand("vr_closequarters_test", test_f);
}

} // namespace qvr::closequarters
