// vr_flatmove.cpp -- flatscreen play's slide and lean (vr_flatmove.hpp; docs/vr-port/FLATPLAY.md, "Sliding" and "Leaning").

#include "vr_flatmove.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_hull.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

extern "C" qboolean keydown[MAX_KEYS];             // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c
extern "C" cvar_t sv_maxspeed;                      // sv_user.c

namespace qvr::flatmove
{

namespace
{

constexpr int slots = 17;              // the server's client numbers 1..16 (edicts 1..maxclients)
constexpr float jumpedUp = 60.f;       // u/s up off the ground: a jump (it ends a slide), not a step off a ledge
constexpr float leanMinHit = 6.f;      // units the head must stand off his box's middle before the leant body is boxed
constexpr float leanDistMost = 30.f;
constexpr float leanAngleMost = 35.f;

// ------------------------------------------------------------------------------------------------------------- server

// A client's slide (the server's, one per client number).
struct Slide
{
    bool on{false};          // sliding (his ground move is the slide's)
    bool held{false};        // his crouch key, last move
    bool grounded{false};    // on the ground, last move
    bool airborne{false};    // left the ground while sliding, not by a jump: the slide goes on when he lands
    double cooldownUntil{0.0};
    double lastTime{-1.0};   // the server's time at the last move (a new map: the clock goes back)
    int starts{0};
    int groundSteps{0};      // this slide's moves on the ground and off it (the tests)
    int airSteps{0};
    float startSpeed{0.f};
};

struct ServerState
{
    Slide slide[slots];
};

ServerState server;
bool leanHitOff = false; // vr_flatlean_test: the same shot with his box alone

[[nodiscard]] int clientSlot(const edict_t* ent)
{
    if(!sv.active || !ent || ent->free)
    {
        return -1;
    }
    const int num = NUM_FOR_EDICT(const_cast<edict_t*>(ent));
    return num >= 1 && num <= svs.maxclients && num < slots ? num : -1;
}

[[nodiscard]] bool slideOn()
{
    return vr_flat_slide.value != 0.f;
}

// The floor's normal under `ent`, no more than 4 units down (straight up when none is found; `found` says).
[[nodiscard]] glm::vec3 floorNormal(edict_t* ent, bool& found)
{
    found = false;
    vec3_t start, end;
    VectorCopy(ent->v.origin, start);
    VectorCopy(ent->v.origin, end);
    end[2] -= 4.f;
    const trace_t tr = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    if(tr.fraction < 1.f && !tr.startsolid && tr.plane.normal[2] > 0.5f)
    {
        found = true;
        return glm::vec3{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
    }
    return glm::vec3{0.f, 0.f, 1.f};
}

void endSlide(Slide& s, double now)
{
    if(s.on)
    {
        s.cooldownUntil = now + za::clamp(vr_flat_slide_cooldown.value, 0.f, 3.f);
    }
    s.on = false;
    s.airborne = false;
}

// -------------------------------------------------------------------------------------------------------------- client

struct Lean
{
    bool keyL{false};      // the keys, last frame
    bool keyR{false};
    bool cmdL{false};      // +flatleanleft / +flatleanright held (the console's, the tests')
    bool cmdR{false};
    int toggled{0};        // vr_flat_lean_toggle 1: the side asked for (-1, 0, 1)
    float lin{0.f};        // the signed share's even progress -1..1
    float dist{0.f};       // the eye's move sideways now (signed units, + right): the clamp in it
    float roll{0.f};       // the view's roll now (degrees, + right)
    glm::vec3 shift{0.f};  // the eye's move in the world this frame
    double lastTime{-1e9};
};

struct SlideView
{
    float lin{0.f};        // the slide pose's even progress 0..1
    float share{0.f};      // smoothstepped
    double lastTime{-1e9};
};

// vr_flatslide_test / vr_flatlean_test: frame-driven runs, one verdict line each.
struct SlideTest
{
    int phase{0};          // 0 off
    double phaseAt{0.0};
    testkit::Tally tally;
    glm::vec3 from{0.f};
    float peak{0.f};
    float startSpeed{0.f};
    float jumpSpeed{0.f};
    bool seenSlide{false};
    double slideAt{0.0};
};

struct LeanTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    float leantSpeed{0.f};
    float wallSide{-1.f};
};

struct ClientState
{
    Lean lean;
    SlideView slide;
    SlideTest slideTest;
    LeanTest leanTest;
};

ClientState client;

[[nodiscard]] bool keyHeld(const char* bindName)
{
    const int k = Key_StringToKeynum(bindName);
    return key_dest == key_game && k > 0 && k < MAX_KEYS && keydown[k] != 0;
}

[[nodiscard]] glm::vec3 levelRight(float yawDeg)
{
    const float y = glm::radians(yawDeg);
    return glm::vec3{za::sin(y), -za::cos(y), 0.f};
}

[[nodiscard]] glm::vec3 levelFwd(float yawDeg)
{
    const float y = glm::radians(yawDeg);
    return glm::vec3{za::cos(y), za::sin(y), 0.f};
}

// The eye's drop as it swings out `d` units round a point vr_flat_lean_pivot below it.
[[nodiscard]] float leanDropFor(float d)
{
    const float p = za::clamp(vr_flat_lean_pivot.value, 10.f, 60.f);
    d = za::min(za::abs(d), p);
    return p - za::sqrt(p * p - d * d);
}

// The lean that fits at his side (units, 0..dist): lines from the eye, under it and from the weapon's place ahead,
// sideways to vr_flat_lean_margin past the lean, each stopped short of what it meets by the margin.
[[nodiscard]] float leanRoom(const glm::vec3& eye, float yaw, float side, float dist)
{
    const float margin = za::clamp(vr_flat_lean_margin.value, 0.f, 16.f);
    const glm::vec3 way = levelRight(yaw) * side;
    const glm::vec3 fwd = levelFwd(yaw);
    float room = dist;
    for(const glm::vec3& p : {eye, eye - glm::vec3{0.f, 0.f, 8.f}, eye + fwd * 20.f - glm::vec3{0.f, 0.f, 6.f}})
    {
        const trace_t tr = worldtrace::world(p, p + way * (dist + margin));
        const float got = tr.startsolid ? 0.f : tr.fraction * (dist + margin) - margin;
        room = za::min(room, za::max(0.f, got));
    }
    return room;
}

[[nodiscard]] bool deadOrAway()
{
    return cls.signon != SIGNONS || cl.stats[STAT_HEALTH] <= 0 || !cl.viewentity;
}

void stepLean(bool on, float dt)
{
    Lean& l = client.lean;
    const bool kl = on && keyHeld(vr_flat_key_lean_left.string);
    const bool kr = on && keyHeld(vr_flat_key_lean_right.string);
    int want = 0;
    if(vr_flat_lean_toggle.value != 0.f)
    {
        if(kl && !l.keyL)
        {
            l.toggled = l.toggled == -1 ? 0 : -1;
        }
        if(kr && !l.keyR)
        {
            l.toggled = l.toggled == 1 ? 0 : 1;
        }
        want = l.toggled;
    }
    else
    {
        want = ((kr || l.cmdR) ? 1 : 0) - ((kl || l.cmdL) ? 1 : 0);
    }
    if(vr_flat_lean_toggle.value != 0.f && (l.cmdL || l.cmdR))
    {
        want = (l.cmdR ? 1 : 0) - (l.cmdL ? 1 : 0);
    }
    l.keyL = kl;
    l.keyR = kr;
    // Upright and crouched only: not lying down, not sliding, not dead, not swimming.
    const edict_t* local = progs::localPlayer();
    const bool swimming = local && local->v.waterlevel >= 2;
    const bool allowed =
        on && vr_flat_lean.value != 0.f && flatplay::proneRatio() < 0.01f && !sliding() && !deadOrAway() && !swimming;
    if(!allowed)
    {
        want = 0;
        if(!on || vr_flat_lean.value == 0.f)
        {
            l.toggled = 0;
        }
    }
    const float time = za::clamp(vr_flat_lean_time.value, 0.05f, 1.f);
    if(dt > 0.f)
    {
        const float step = dt / time;
        l.lin += za::clamp(static_cast<float>(want) - l.lin, -step, step);
    }
    const float share = l.lin < 0.f ? -geom::smoothstep(0.f, 1.f, -l.lin) : geom::smoothstep(0.f, 1.f, l.lin);
    const float full = za::clamp(vr_flat_lean_dist.value, 0.f, leanDistMost);
    const float asked = share * full;
    float target = asked;
    if(on && target != 0.f && cl.viewentity)
    {
        // The wall at his side: the eye never goes past what fits, at once (it would show the inside of the wall),
        // and comes back out no faster than the lean's own speed.
        const entity_t& pl = cl_entities[cl.viewentity];
        const glm::vec3 eye{pl.origin[0], pl.origin[1], pl.origin[2] + cl.viewheight - flatplay::crouchDrop()};
        const float side = target > 0.f ? 1.f : -1.f;
        const float room = leanRoom(eye, cl.viewangles[YAW], side, za::abs(target));
        target = side * za::min(za::abs(target), room);
    }
    // Toward the target at the lean's own speed (the key's ease already shapes it: this only keeps a wall's clamp from
    // springing back at once); cut by a wall, never past what fits, that frame.
    const float most = dt > 0.f ? 2.f * full * dt / time : 0.f;
    float next = l.dist + za::clamp(target - l.dist, -most, most);
    const bool cut = za::abs(target) < za::abs(asked) - 1e-3f;
    if(cut && (next > 0.f) == (target > 0.f) && za::abs(next) > za::abs(target))
    {
        next = target;
    }
    l.dist = next;
    if(!on)
    {
        l = Lean{};
    }
    const float angle = za::clamp(vr_flat_lean_angle.value, 0.f, leanAngleMost);
    l.roll = share * angle;
    const float yaw = cl.viewangles[YAW];
    l.shift = levelRight(yaw) * l.dist - glm::vec3{0.f, 0.f, leanDropFor(l.dist)};
}

void stepSlideView(bool on, float dt)
{
    SlideView& v = client.slide;
    const float time = za::clamp(vr_flat_slide_body_time.value, 0.05f, 1.f);
    const float want = on && sliding() ? 1.f : 0.f;
    if(dt > 0.f)
    {
        v.lin += za::clamp(want - v.lin, -dt / time, dt / time);
    }
    if(!on)
    {
        v.lin = 0.f;
    }
    v.share = geom::smoothstep(0.f, 1.f, v.lin);
}

// ---------------------------------------------------------------------------------------------------------------- tests

void testCheck(testkit::Tally& tally, const char* tag, bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", tag, what);
    }
    tally.check(ok, tag, what);
}

[[nodiscard]] float groundSpeed(const edict_t* e)
{
    return za::sqrt(e->v.velocity[0] * e->v.velocity[0] + e->v.velocity[1] * e->v.velocity[1]);
}

// vr_flatslide_test: the script runs him forward (+forward, cl_alwaysrun 1) on open level ground. Up to his run's
// speed, the crouch (+flatcrouch) starts a slide: faster than the run by the boost, further than a crouched walk would
// go, ending in a crouch; let go, run again, slide again and jump in it: the speed along the floor kept in the air.
void slideTestFrame()
{
    SlideTest& t = client.slideTest;
    if(t.phase == 0)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    const double now = qcvm->time;
    const char* tag = "flatslide";
    if(!pl)
    {
        return;
    }
    const float speed = groundSpeed(pl);
    const Slide& s = server.slide[1];
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    if(inPhase > 8.0 && t.phase != 99)
    {
        char line[160];
        q_snprintf(line, sizeof(line), "timed out (step %d: speed %.0f, ground %d, sliding %d, crouch held %d)", t.phase, speed,
            ground ? 1 : 0, s.on ? 1 : 0, hull::flatCrouchHeld(pl) ? 1 : 0);
        testCheck(t.tally, tag, false, line);
        next(99);
    }
    switch(t.phase)
    {
        case 1: // running up to speed
            if(ground && speed > 300.f)
            {
                t.startSpeed = speed;
                t.from = glm::vec3{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
                t.peak = 0.f;
                t.seenSlide = false;
                Cbuf_InsertText("+flatcrouch\n");
                next(2);
            }
            break;
        case 2: // the slide
            t.peak = za::max(t.peak, speed);
            if(s.on && !t.seenSlide)
            {
                t.seenSlide = true;
                t.slideAt = now;
            }
            if(t.seenSlide && !s.on)
            {
                const float secs = static_cast<float>(now - t.slideAt);
                const glm::vec3 at{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
                const float dist = glm::length(glm::vec2{at - t.from});
                const float boost = za::clamp(vr_flat_slide_boost.value, 0.f, 200.f);
                char line[256];
                q_snprintf(line, sizeof(line), "slid from %.0f u/s, peak %.0f (boost %.0f)", t.startSpeed, t.peak, boost);
                testCheck(t.tally, tag, t.peak > t.startSpeed + boost * 0.7f, line);
                // On level ground it loses vr_flat_slide_friction a second: from its peak to vr_flat_slide_end in that time.
                const float friction = za::max(1.f, vr_flat_slide_friction.value);
                const float endSpeed = za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
                const float want = (t.peak - endSpeed) / friction;
                q_snprintf(line, sizeof(line),
                    "slide %.2f s (friction's %.2f; %d moves on the ground, %d off), %.0f units (%.0f from the start), ends at %.0f u/s",
                    secs, want, s.groundSteps, s.airSteps, dist, 0.5f * (t.peak + endSpeed) * want, speed);
                testCheck(t.tally, tag, za::abs(secs - want) < 0.1f + 0.1f * want && dist > 0.4f * (t.peak + endSpeed) * want, line);
                q_snprintf(line, sizeof(line), "ends into a crouch (box crouched %d, key held %d)", hull::isCrouched(pl) ? 1 : 0,
                    hull::flatCrouchHeld(pl) ? 1 : 0);
                testCheck(t.tally, tag, hull::isCrouched(pl) && hull::flatCrouchHeld(pl), line);
                Cbuf_InsertText("-flatcrouch\n");
                next(3);
            }
            break;
        case 3: // up again, running, the cooldown past
            if(!s.on && ground && speed > 300.f && now > s.cooldownUntil && !hull::flatCrouchHeld(pl))
            {
                Cbuf_InsertText("+flatcrouch\n");
                next(4);
            }
            break;
        case 4: // sliding: jump in it
            if(s.on && now - t.phaseAt > 0.15)
            {
                t.jumpSpeed = speed;
                Cbuf_InsertText("+jump\n");
                next(5);
            }
            break;
        case 5: // in the air
            if(!ground && pl->v.velocity[2] > 0.f && inPhase > 0.2)
            {
                char line[256];
                q_snprintf(line, sizeof(line), "a jump in the slide keeps its speed: %.0f u/s in the air of %.0f", speed, t.jumpSpeed);
                testCheck(t.tally, tag, speed > 0.9f * t.jumpSpeed && t.jumpSpeed > 300.f, line);
                q_snprintf(line, sizeof(line), "the jump ended the slide (sliding %d)", s.on ? 1 : 0);
                testCheck(t.tally, tag, !s.on, line);
                Cbuf_InsertText("-jump\n-flatcrouch\n");
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = SlideTest{};
            break;
    }
}

// vr_flatlean_test: standing in the open, then against a wall at his side (the script places him): the lean's eye
// move and roll, the wall's clamp, the hit box out of his box, the slower move.
void leanTestFrame()
{
    LeanTest& t = client.leanTest;
    if(t.phase == 0)
    {
        return;
    }
    const char* tag = "flatlean";
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    const float full = za::clamp(vr_flat_lean_dist.value, 0.f, leanDistMost);
    const float time = za::clamp(vr_flat_lean_time.value, 0.05f, 1.f);
    char line[256];
    if(inPhase > 8.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d)", t.phase);
        testCheck(t.tally, tag, false, line);
        next(99);
    }
    switch(t.phase)
    {
        case 1:
            client.lean.cmdR = true;
            next(2);
            break;
        case 2: // a full lean right, in the open
            if(inPhase > time + 0.1)
            {
                const hands::State& s = hands::current();
                const entity_t& pl = cl_entities[cl.viewentity];
                const glm::vec3 straight{pl.origin[0], pl.origin[1], pl.origin[2] + cl.viewheight - flatplay::crouchDrop()};
                const float side = glm::dot(s.head - straight, levelRight(cl.viewangles[YAW]));
                q_snprintf(line, sizeof(line), "leant right: the eye %.1f units right (of %.0f), roll %.1f (of %.0f), down %.1f",
                    side, full, client.lean.roll, vr_flat_lean_angle.value, straight.z - s.head.z);
                testCheck(t.tally, tag,
                    za::abs(side - full) < 0.5f && za::abs(client.lean.roll - vr_flat_lean_angle.value) < 0.1f, line);
                // The server: a line from his right past his box's side, at the leant head's height, meets him.
                progs::ServerVm vm;
                edict_t* e = progs::localPlayer();
                bool hit = false;
                bool missStraight = false;
                if(e)
                {
                    const glm::vec3 o{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
                    const glm::vec3 r = levelRight(cl.viewangles[YAW]);
                    const glm::vec3 f = levelFwd(cl.viewangles[YAW]);
                    const glm::vec3 at = o + r * (full + 4.f) + glm::vec3{0.f, 0.f, s.head.z - o.z - 2.f};
                    const trace_t tr = worldtrace::serverLine(at + f * 64.f, at - f * 64.f, nullptr, MOVE_NORMAL);
                    hit = tr.ent == e;
                    leanHitOff = true;
                    const trace_t tr2 = worldtrace::serverLine(at + f * 64.f, at - f * 64.f, nullptr, MOVE_NORMAL);
                    leanHitOff = false;
                    missStraight = tr2.ent != e;
                }
                q_snprintf(line, sizeof(line), "a shot across the leant head %.0f units off his middle hits him (%d), his box alone not (%d)",
                    full + 4.f, hit ? 1 : 0, missStraight ? 1 : 0);
                testCheck(t.tally, tag, hit && missStraight, line);
                Cbuf_InsertText("+forward\n");
                next(3);
            }
            break;
        case 3: // walking forward leant
            if(inPhase > 0.8)
            {
                const float v = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
                Cbuf_InsertText("-forward\n");
                client.lean.cmdR = false;
                t.leantSpeed = v;
                next(4);
            }
            break;
        case 4: // straight again, the speed back
            if(inPhase > 0.3 && client.lean.dist == 0.f)
            {
                Cbuf_InsertText("+forward\n");
                next(5);
            }
            break;
        case 5:
            if(inPhase > 0.8)
            {
                const float v = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
                Cbuf_InsertText("-forward\n");
                const float want = za::clamp(vr_flat_lean_speed.value, 0.2f, 1.f);
                q_snprintf(line, sizeof(line), "walking leant %.0f u/s, upright %.0f: %.2f of it (vr_flat_lean_speed %.2f)",
                    t.leantSpeed, v, v > 0.f ? t.leantSpeed / v : 0.f, want);
                testCheck(t.tally, tag, v > 0.f && za::abs(t.leantSpeed / v - want) < 0.06f, line);
                next(6);
            }
            break;
        case 6: // stopped: put him 18 units (his box's half and 2) off the nearer wall at his side, then lean into it
            if(inPhase > 0.3)
            {
                const entity_t& pl = cl_entities[cl.viewentity];
                const glm::vec3 o{pl.origin[0], pl.origin[1], pl.origin[2]};
                const glm::vec3 r = levelRight(cl.viewangles[YAW]);
                const trace_t left = worldtrace::world(o, o - r * 2048.f);
                const trace_t right = worldtrace::world(o, o + r * 2048.f);
                const bool toLeft = left.fraction <= right.fraction;
                const float frac = toLeft ? left.fraction : right.fraction;
                if(frac >= 1.f)
                {
                    testCheck(t.tally, tag, false, "no wall at either side");
                    next(99);
                    break;
                }
                t.wallSide = toLeft ? -1.f : 1.f;
                const glm::vec3 to = o + r * (t.wallSide * (frac * 2048.f - 17.f));
                q_snprintf(line, sizeof(line), "setpos %.1f %.1f %.1f\n", to.x, to.y, to.z);
                Cbuf_InsertText(line);
                (toLeft ? client.lean.cmdL : client.lean.cmdR) = true;
                next(7);
            }
            break;
        case 7:
            if(inPhase > time + 0.2)
            {
                const hands::State& s = hands::current();
                const glm::vec3 r = levelRight(cl.viewangles[YAW]);
                const trace_t tr = worldtrace::world(s.head, s.head + r * (t.wallSide * 64.f));
                const float gap = tr.fraction * 64.f;
                const float margin = za::clamp(vr_flat_lean_margin.value, 0.f, 16.f);
                const float leant = t.wallSide * client.lean.dist;
                q_snprintf(line, sizeof(line), "leant %s into a wall: the eye %.1f units off it (margin %.0f; upright %.1f), lean %.1f of %.0f",
                    t.wallSide < 0.f ? "left" : "right", gap, margin, gap + leant, leant, full);
                testCheck(t.tally, tag, gap > margin - 0.6f && leant > 0.f && leant < full - 1.f, line);
                client.lean.cmdL = client.lean.cmdR = false;
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = LeanTest{};
            break;
    }
}

// vr_flatslide_selftest: the slide's step on made-up floors, 72 steps a second.
void slideSelftest_f()
{
    testkit::Tally tally;
    const char* tag = "flatslide_selftest";
    const float dt = 1.f / 72.f;
    const auto run = [&](const glm::vec3& normal, float v0, float seconds, float& out, bool& ended) {
        SlideStep st{glm::vec2{v0, 0.f}, false};
        ended = false;
        for(int i = 0; i < static_cast<int>(seconds * 72.f) && !st.ended; i++)
        {
            st = slideStep(st.vel, normal, glm::vec2{0.f}, dt);
        }
        ended = st.ended;
        out = glm::length(st.vel);
    };
    char line[256];
    float v = 0.f;
    bool ended = false;
    run({0.f, 0.f, 1.f}, 380.f, 0.5f, v, ended);
    const float fr = za::clamp(vr_flat_slide_friction.value, 0.f, 1000.f);
    q_snprintf(line, sizeof(line), "flat: 380 u/s for half a second leaves %.0f (friction %.0f: %.0f)", v, fr, 380.f - fr * 0.5f);
    testCheck(tally, tag, za::abs(v - (380.f - fr * 0.5f)) < 3.f, line);
    run({0.f, 0.f, 1.f}, 380.f, 5.f, v, ended);
    q_snprintf(line, sizeof(line), "flat: it ends (%d) at %.0f u/s, under vr_flat_slide_end", ended ? 1 : 0, v);
    testCheck(tally, tag, ended, line);
    // Downhill 25 degrees (the floor's normal leaning forward, +x): faster, up to the top speed.
    const float a = glm::radians(25.f);
    run({za::sin(a), 0.f, za::cos(a)}, 380.f, 0.5f, v, ended);
    q_snprintf(line, sizeof(line), "downhill 25 deg: 380 u/s for half a second goes to %.0f", v);
    testCheck(tally, tag, v > 400.f && !ended, line);
    run({za::sin(a), 0.f, za::cos(a)}, 380.f, 6.f, v, ended);
    const float top = za::clamp(vr_flat_slide_max.value, 200.f, 1000.f);
    q_snprintf(line, sizeof(line), "downhill 25 deg: six seconds reach the top speed %.0f (%.0f)", top, v);
    testCheck(tally, tag, za::abs(v - top) < 1.f && !ended, line);
    // Uphill 15 degrees: slower than on the flat, and it stops.
    float vFlat = 0.f;
    run({0.f, 0.f, 1.f}, 380.f, 0.4f, vFlat, ended);
    const float b = glm::radians(15.f);
    run({-za::sin(b), 0.f, za::cos(b)}, 380.f, 0.4f, v, ended);
    q_snprintf(line, sizeof(line), "uphill 15 deg: %.0f u/s after 0.4 s, the flat's %.0f", v, vFlat);
    testCheck(tally, tag, v < vFlat - 20.f, line);
    run({-za::sin(b), 0.f, za::cos(b)}, 380.f, 5.f, v, ended);
    q_snprintf(line, sizeof(line), "uphill 15 deg: it ends (%d)", ended ? 1 : 0);
    testCheck(tally, tag, ended, line);
    tally.verdict(tag);
}

void slideTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatslide_test: flat-screen play (-flatscreenmode) in a game you host\n");
        return;
    }
    client.slideTest = SlideTest{};
    client.slideTest.phase = 1;
    progs::ServerVm vm;
    client.slideTest.phaseAt = qcvm->time;
}

void leanTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatlean_test: flat-screen play (-flatscreenmode) in a game you host\n");
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "wall"))
    {
        if(client.leanTest.phase == 6)
        {
            client.lean.cmdL = true;
            client.leanTest.phase = 7;
            client.leanTest.phaseAt = vr_gametime;
        }
        return;
    }
    client.leanTest = LeanTest{};
    client.leanTest.phase = 1;
    client.leanTest.phaseAt = vr_gametime;
}

void status_f()
{
    Con_Printf("flatmove: lean %.2f (eye %.1f %.1f %.1f, roll %.1f), sliding %d (pose %.2f, drop %.1f)",
        leanShare(), client.lean.shift.x, client.lean.shift.y, client.lean.shift.z, client.lean.roll, sliding() ? 1 : 0,
        client.slide.share, slideDrop());
    if(sv.active)
    {
        const Slide& s = server.slide[1];
        Con_Printf(", server: slides %d, last from %.0f u/s", s.starts, s.startSpeed);
    }
    Con_Printf("\n");
}

void leanLeftDown_f() { client.lean.cmdL = true; }
void leanLeftUp_f() { client.lean.cmdL = false; }
void leanRightDown_f() { client.lean.cmdR = true; }
void leanRightUp_f() { client.lean.cmdR = false; }

} // namespace

SlideStep slideStep(const glm::vec2& vel, const glm::vec3& normal, const glm::vec2& wish, float dt)
{
    SlideStep out{vel, false};
    // Down the slope: the floor's normal leans toward the way down; its level part is the slope's sine.
    const glm::vec2 down{normal.x, normal.y};
    const float sine = glm::length(down);
    if(sine > 0.02f)
    {
        out.vel += down * (za::clamp(vr_flat_slide_slope.value, 0.f, 2000.f) * dt);
    }
    float speed = glm::length(out.vel);
    // The move keys turn it a little.
    const float steer = glm::radians(za::clamp(vr_flat_slide_steer.value, 0.f, 180.f)) * dt;
    if(steer > 0.f && speed > 1.f && glm::length(wish) > 0.1f)
    {
        const float have = za::atan2(out.vel.y, out.vel.x);
        float want = za::atan2(wish.y, wish.x) - have;
        while(want > glm::pi<float>())
        {
            want -= 2.f * glm::pi<float>();
        }
        while(want < -glm::pi<float>())
        {
            want += 2.f * glm::pi<float>();
        }
        // (Only toward a way ahead of it: a key held back is no brake and no turn round.)
        if(za::abs(want) < 0.5f * glm::pi<float>())
        {
            const float turn = have + za::clamp(want, -steer, steer);
            out.vel = glm::vec2{za::cos(turn), za::sin(turn)} * speed;
        }
    }
    const float friction = za::clamp(vr_flat_slide_friction.value, 0.f, 1000.f);
    const float top = za::clamp(vr_flat_slide_max.value, 200.f, 1000.f);
    const float newSpeed = za::min(za::max(0.f, speed - friction * dt), top);
    out.vel = speed > 1e-3f ? out.vel * (newSpeed / speed) : glm::vec2{0.f};
    speed = newSpeed;
    out.ended = speed < za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
    return out;
}

void init()
{
    Cmd_AddCommand("+flatleanleft", leanLeftDown_f); // the lean keys' holds, the console's way (a bind or a script)
    Cmd_AddCommand("-flatleanleft", leanLeftUp_f);
    Cmd_AddCommand("+flatleanright", leanRightDown_f);
    Cmd_AddCommand("-flatleanright", leanRightUp_f);
    Cmd_AddCommand("vr_flatslide_selftest", slideSelftest_f);
    Cmd_AddCommand("vr_flatslide_test", slideTest_f);
    Cmd_AddCommand("vr_flatlean_test", leanTest_f);
    Cmd_AddCommand("vr_flatmove_status", status_f);
}

void frame(bool on)
{
    double dt = client.lean.lastTime > -1e8 ? vr_gametime - client.lean.lastTime : 0.0;
    client.lean.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    stepLean(on, static_cast<float>(dt));
    client.lean.lastTime = vr_gametime;
    stepSlideView(on, static_cast<float>(dt));
    if(on)
    {
        slideTestFrame();
        leanTestFrame();
    }
}

glm::vec3 leanShift()
{
    return flatplay::active() ? client.lean.shift : glm::vec3{0.f};
}

float leanRoll()
{
    return flatplay::active() ? client.lean.roll : 0.f;
}

float leanShare()
{
    const float full = za::clamp(vr_flat_lean_dist.value, 0.f, leanDistMost);
    return full > 0.f ? client.lean.dist / full : 0.f;
}

bool sliding()
{
    if(!flatplay::active())
    {
        return false;
    }
    if(sv.active && svs.maxclients >= 1)
    {
        return server.slide[1].on && !server.slide[1].airborne;
    }
    // A client of another server: his crouch held, on the ground, at a slide's speed.
    const float speed = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
    return slideOn() && flatplay::crouchHeld() && cl.onground && speed > za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
}

float slideShare()
{
    return flatplay::active() ? client.slide.share : 0.f;
}

float slideNeckBack()
{
    return flatplay::active() ? za::clamp(vr_flat_slide_neck_back.value, 0.f, 20.f) : 0.f;
}

float slideDrop()
{
    return slideShare() * za::clamp(vr_flat_slide_drop.value, 0.f, 14.f);
}

} // namespace qvr::flatmove

using namespace qvr;

extern "C" int VR_FlatSlideMove(edict_t* ent, int onground, const float* wishdir, float wishspeed, float* velocity)
{
    const int num = flatmove::clientSlot(ent);
    if(num < 0)
    {
        return 0;
    }
    flatmove::Slide& s = flatmove::server.slide[num];
    const double now = qcvm->time;
    if(now < s.lastTime)
    {
        s = flatmove::Slide{}; // a new map (the clock starts again)
    }
    s.lastTime = now;
    const bool held = hull::flatCrouchHeld(ent);
    const bool pressed = held && !s.held;
    // On the ground: Quake's flag, or the floor right under him and not going up (moving fast, the flag is off every
    // other move: the walk's clip down to the floor sets it only on the moves that meet it).
    bool floorFound = false;
    const glm::vec3 normal = flatmove::floorNormal(ent, floorFound);
    const bool ground = onground || (floorFound && velocity[2] <= flatmove::jumpedUp);
    const bool landed = ground && !s.grounded;
    s.held = held;
    s.grounded = ground;
    const glm::vec2 vel{velocity[0], velocity[1]};
    const float speed = glm::length(vel);

    if(!flatmove::slideOn() || !held || !hull::isFlat(ent) || hull::isProne(ent) || ent->v.waterlevel >= 2 ||
       static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        flatmove::endSlide(s, now);
        return 0;
    }
    if(s.on && !ground)
    {
        if(velocity[2] > flatmove::jumpedUp && !s.airborne)
        {
            flatmove::endSlide(s, now); // a jump: Quake's air move keeps the speed
        }
        else
        {
            s.airborne = true; // off a ledge: on again when he lands
            s.airSteps++;
        }
        return 0;
    }
    if(s.on && s.airborne && ground)
    {
        s.airborne = false;
    }
    if(!s.on)
    {
        const float least = za::clamp(vr_flat_slide_min.value, 0.f, 400.f);
        if(!ground || (!pressed && !landed) || speed < least || now < s.cooldownUntil)
        {
            return 0;
        }
        s.on = true;
        s.starts++;
        s.groundSteps = s.airSteps = 0;
        s.startSpeed = speed;
        const float top = za::clamp(vr_flat_slide_max.value, 200.f, 1000.f);
        const float boosted = za::max(speed, za::min(speed + za::clamp(vr_flat_slide_boost.value, 0.f, 200.f), top));
        velocity[0] *= boosted / speed;
        velocity[1] *= boosted / speed;
        return 1;
    }
    const glm::vec2 wish = wishspeed > 1.f ? glm::vec2{wishdir[0], wishdir[1]} : glm::vec2{0.f};
    s.groundSteps++;
    const flatmove::SlideStep st =
        flatmove::slideStep(vel, normal, wish, static_cast<float>(host_frametime));
    velocity[0] = st.vel.x;
    velocity[1] = st.vel.y;
    if(st.ended)
    {
        flatmove::endSlide(s, now); // into the crouch: the walk's friction from the next move
    }
    return 1;
}

extern "C" int VR_LeanHitBoxes(edict_t* touch, float (*boxes)[6])
{
    if(vr_flat_lean_hit.value == 0.f || flatmove::leanHitOff || flatmove::clientSlot(touch) < 0 || !hull::isFlat(touch) || touch->v.health <= 0.f ||
       static_cast<int>(touch->v.solid) == SOLID_NOT)
    {
        return 0;
    }
    const int ofs = progs::fields().headpos;
    if(ofs < 0)
    {
        return 0;
    }
    const glm::vec3 head = progs::fieldVec(touch, ofs);
    const glm::vec3 o{touch->v.origin[0], touch->v.origin[1], touch->v.origin[2]};
    if(head == glm::vec3{0.f} || glm::length(glm::vec2{head - o}) < flatmove::leanMinHit)
    {
        return 0;
    }
    // The head, and the chest half way down from it to the hips (his origin: the middle of his box).
    const glm::vec3 centres[2] = {head + glm::vec3{0.f, 0.f, -2.f}, o + (head - o) * 0.55f};
    const glm::vec3 halves[2] = {glm::vec3{7.f, 7.f, 8.f}, glm::vec3{8.f, 8.f, 10.f}};
    for(int i = 0; i < 2; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            boxes[i][k] = centres[i][k] - halves[i][k];
            boxes[i][k + 3] = centres[i][k] + halves[i][k];
        }
    }
    return 2;
}

extern "C" int VR_FlatKeyOwned(int key)
{
    if(!flatplay::active() || vr_flat_keys_own.value == 0.f || key <= 0 || key >= MAX_KEYS)
    {
        return 0;
    }
    for(const cvar_t* c : {&vr_flat_key_lean_left, &vr_flat_key_lean_right, &vr_flat_key_swap, &vr_flat_key_grab, &vr_flat_key_grab_alt,
            &vr_flat_key_prone, &vr_flat_key_bullettime, &vr_flat_key_flycam, &vr_flat_key_toolgun, &vr_flat_key_toolgun_menu})
    {
        if(c->string[0] && Key_StringToKeynum(c->string) == key)
        {
            return 1;
        }
    }
    return key == 'f' || key == 'g' || key == 'v' ? 1 : 0;
}

extern "C" void VR_FlatMoveScale(float* forwardmove, float* sidemove)
{
    const float share = za::abs(flatmove::leanShare());
    if(share <= 0.f)
    {
        return;
    }
    float scale = 1.f + (za::clamp(vr_flat_lean_speed.value, 0.2f, 1.f) - 1.f) * za::min(share, 1.f);
    // (Of the speed the move gets: the keys ask for more than the server's most, sv_maxspeed, which it cuts them to.)
    const float len = za::sqrt(*forwardmove * *forwardmove + *sidemove * *sidemove);
    if(len > sv_maxspeed.value && len > 0.f)
    {
        scale *= sv_maxspeed.value / len;
    }
    *forwardmove *= scale;
    *sidemove *= scale;
}

extern "C" void VR_FlatViewLean(float* vieworg, float* viewangles)
{
    if(!flatplay::active() || chase_active.value)
    {
        return;
    }
    const glm::vec3 shift = flatmove::leanShift();
    vieworg[0] += shift.x;
    vieworg[1] += shift.y;
    vieworg[2] += shift.z;
    viewangles[ROLL] += flatmove::leanRoll();
}
