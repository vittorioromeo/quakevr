// vr_flatmove.cpp -- flatscreen play's slide and lean (vr_flatmove.hpp; docs/vr-port/FLATPLAY.md, "Sliding" and "Leaning").

#include "vr_flatmove.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_climb.hpp"
#include "vr_protocol.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
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
constexpr float jumpedUp = 60.f;       // u/s up off the ground, the jump key held: a jump (it ends a slide)
constexpr float steerBack = 0.35f;     // radians (20 degrees) round straight back where the move keys don't steer a slide
constexpr float leanMinHit = 6.f;      // units the head must stand off his box's middle before the leant body is boxed
constexpr float leanDistMost = 30.f;
constexpr float leanAngleMost = 35.f;
constexpr float leanGunAhead = 20.f;   // leanRoom: the weapon's place this far ahead of the eye, where it is open
constexpr float leanAheadKeep = 2.f;   // and this far short of a wall ahead

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
    int jumpEnds{0};         // slides a jump ended (the tests: a ramp or a step is no jump)
    const char* endWhy{""};  // why the last one ended (the tests, vr_flatmove_status)
    float lastZ{0.f};        // his height at the last move (a step up's rise)
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
    float full{0.f};       // a full lean's eye move now (leanFullDist: smaller lying down)
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

// vr_flatprone_test: lying down, its lean; the crouch out of it; the gun flush on the floor looking down.
struct ProneTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    glm::vec3 handWas{0.f};  // the main hand off the eye in the view's frame, last frame
    float biggestStep{0.f};  // the largest frame's move of it, the crouch out of lying down
    bool pitchHeld{false};   // the look held at `pitch` each frame
    float pitch{0.f};
};

// vr_flatmantle_test (maps/vrmantle.bsp): a jump at each block's face, the jump key held, looking up or level; mantled
// onto it or not (vr_climb.cpp flatMantle).
struct MantleTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int index{0};         // the case
    int frames{0};        // frames settled (on the ground, before the jump; landed, after it)
    bool airborne{false}; // the jump has left the ground
    int mantlesBefore{0};
    float pitch{0.f};
    int only{-1};      // vr_flatmantle_test <case>: that case alone
    int onLedge{0};    // the hands drawn on the ledge while it mantled (STAT_QVR_CLIMB's bits: 1 the off hand, 2 the main)
    int empty{-1};     // and the hands empty as it started (climb::handFree), the same bits
};

// vr_flatslide_rough_test (maps/vrtesthall.bsp): slides up its stairs and its ramp, then curves with A alone and D alone,
// the view turned 10 degrees off the slide's way so the side key asks a little behind it.
struct RoughTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int index{0};         // the case
    int frames{0};
    float z0{0.f};        // the slide's start height
    float zTop{0.f};      // the highest it got while sliding
    float heading0{0.f};  // the slide's way when the side key went down (degrees)
    int jumpsBefore{0};
};

// vr_flatslide_ramp_test: slides down and up e1m1's ramps, the view's, the gun's and the body's shake measured frame by frame.
struct RampTest
{
    static constexpr int most = 160;
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int frames{0};
    bool verbose{false};
    int run{0};               // which of rampRuns
    int n{0};                 // samples taken while sliding along the ramp
    int air{0};               // of them, off the ground (Quake's flag)
    float eye[most]{};        // the view's height, world
    float feet[most]{};       // his origin's height
    bool slope[most]{};       // on the slope (his origin going up or down)
    glm::vec3 gun[most]{};    // the drawn main hand from the view, world axes (his yaw stays put)
    glm::vec3 shoulder[most]{}; // the drawn body's main shoulder from the view
    glm::vec3 pelvis[most]{};   // the drawn body's pelvis from the view
};

struct ClientState
{
    Lean lean;
    SlideView slide;
    SlideTest slideTest;
    RoughTest roughTest;
    RampTest rampTest;
    LeanTest leanTest;
    ProneTest proneTest;
    MantleTest mantleTest;
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
// (The weapon's place ahead is only as far ahead as is open: facing a wall he stands against, 20 units ahead was inside
// it, that line started in the solid and the lean got no room at all -- the view rolled and the eye stayed (his report,
// 2026-11-08). The gun there is the wall's to pull back and tilt (vr_flatsight.cpp); the lean only needs the side open.
// A line from under the eye that starts in something -- a ledge, a step -- is not a side wall and is not counted.)
[[nodiscard]] float leanRoom(const glm::vec3& eye, float yaw, float side, float dist)
{
    const float margin = za::clamp(vr_flat_lean_margin.value, 0.f, 16.f);
    const glm::vec3 way = levelRight(yaw) * side;
    const glm::vec3 fwd = levelFwd(yaw);
    const glm::vec3 low{0.f, 0.f, 8.f};
    const glm::vec3 gunDrop{0.f, 0.f, 6.f};
    const trace_t open = worldtrace::world(eye - gunDrop, eye - gunDrop + fwd * leanGunAhead);
    const float ahead = open.startsolid ? 0.f : za::max(0.f, open.fraction * leanGunAhead - leanAheadKeep);
    float room = dist;
    const glm::vec3 from[3] = {eye, eye - low, eye - gunDrop + fwd * ahead};
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3& p = from[i];
        const trace_t tr = worldtrace::world(p, p + way * (dist + margin));
        if(tr.startsolid && i > 0)
        {
            continue;
        }
        const float got = tr.startsolid ? 0.f : tr.fraction * (dist + margin) - margin;
        room = za::min(room, za::max(0.f, got));
    }
    return room;
}

// The lean's full eye move and roll now: lying down (vr_flat_key_prone), by how far down he is, its own smaller ones
// (vr_flat_lean_prone_dist / _angle: the lying body rolls in place).
[[nodiscard]] float leanFullDist()
{
    const float stand = za::clamp(vr_flat_lean_dist.value, 0.f, leanDistMost);
    const float prone = za::clamp(vr_flat_lean_prone_dist.value, 0.f, leanDistMost);
    return stand + (prone - stand) * flatplay::proneRatio();
}

[[nodiscard]] float leanFullAngle()
{
    const float stand = za::clamp(vr_flat_lean_angle.value, 0.f, leanAngleMost);
    const float prone = za::clamp(vr_flat_lean_prone_angle.value, 0.f, leanAngleMost);
    return stand + (prone - stand) * flatplay::proneRatio();
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
    // Upright, crouched and lying down (its own smaller lean: leanFullDist / leanFullAngle); not sliding, not dead, not
    // swimming.
    const edict_t* local = progs::localPlayer();
    const bool swimming = local && local->v.waterlevel >= 2;
    const bool allowed = on && vr_flat_lean.value != 0.f && !slidePose() && !deadOrAway() && !swimming;
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
    const float full = leanFullDist();
    l.full = full;
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
    l.roll = share * leanFullAngle();
    const float yaw = cl.viewangles[YAW];
    l.shift = levelRight(yaw) * l.dist - glm::vec3{0.f, 0.f, leanDropFor(l.dist)};
}

void stepSlideView(bool on, float dt)
{
    SlideView& v = client.slide;
    const float time = za::clamp(vr_flat_slide_body_time.value, 0.05f, 1.f);
    const float want = on && slidePose() ? 1.f : 0.f;
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
        case 5: // in the air: read a few moves after the jump, still rising (a jump's 270 u/s up, not a step off). Read
                // 0.2 s in, the second slide's jump met vrtesthall's north wall first (y 344) and the read was the 30 u/s
                // the air move gives after the wall took the speed.
            if(!ground && pl->v.velocity[2] > 150.f && inPhase > 0.04)
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

// vr_flatslide_rough_test: each case places him (setpos), runs him its way (+forward), crouches at speed and watches the
// slide. 0: east at vrtesthall's stairs (16 high, 16 deep): it climbs at least three steps sliding. 1: north up its
// 22 degree ramp: it goes up the ramp sliding and no jump ends it. 2, 3: east in the open, the forward key let go, the
// view turned 10 degrees left (right) of the slide's way, A (D) alone held 0.5 s: it turns left (right) at least half
// of vr_flat_slide_steer's degrees in that time.
void roughTestFrame()
{
    RoughTest& t = client.roughTest;
    if(t.phase == 0)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    const char* tag = "flatslide_rough";
    const double now = qcvm->time;
    const Slide& s = server.slide[1];
    const float speed = groundSpeed(pl);
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    const glm::vec3 at{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frames = 0;
    };
    struct Case
    {
        float x, y, yaw; // the start (on the floor) and the run's way
        float go;        // crouch once this far along the run's way (x east, y north)
        int side;        // 0: none; 1 A (left), -1 D (right)
    };
    static constexpr Case cases[] = {
        {-50.f, 592.f, 0.f, -20.f, 0},   // the stairs (x 96.. rising east, y 544..640)
        {-160.f, 300.f, 90.f, 380.f, 0}, // the ramp (x -240..-80, y 460..620 rising north)
        {-580.f, -440.f, 0.f, -520.f, 1},
        {-580.f, -440.f, 0.f, -520.f, -1},
    };
    static constexpr int caseCount = static_cast<int>(sizeof(cases) / sizeof(cases[0]));
    const Case& c = cases[t.index < caseCount ? t.index : 0];
    const float along = c.yaw == 0.f ? at.x : at.y;
    const float heading = glm::degrees(za::atan2(pl->v.velocity[1], pl->v.velocity[0]));
    char line[256];
    if(inPhase > 6.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "case %d timed out (step %d: speed %.0f, ground %d, sliding %d, crouch held %d, starts %d)", t.index,
            t.phase, speed, ground ? 1 : 0, s.on ? 1 : 0, hull::flatCrouchHeld(pl) ? 1 : 0, s.starts);
        testCheck(t.tally, tag, false, line);
        Cbuf_InsertText("-forward\n-moveleft\n-moveright\n-flatcrouch\n");
        t.index++;
        next(t.index < caseCount ? 1 : 99);
        return;
    }
    switch(t.phase)
    {
        case 1: // place him, the keys let go (not setpos: it turns noclip on)
            Cbuf_InsertText("-forward\n-moveleft\n-moveright\n-flatcrouch\n");
            pl->v.origin[0] = c.x;
            pl->v.origin[1] = c.y;
            pl->v.origin[2] = 24.f;
            pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
            SV_LinkEdict(pl, false);
            next(2);
            break;
        case 2: // settled on the floor, the last slide's cooldown past: run
            cl.viewangles[YAW] = c.yaw;
            cl.viewangles[PITCH] = 0.f;
            if(++t.frames > 10 && ground && !s.on && now > s.cooldownUntil)
            {
                Cbuf_InsertText("+forward\n");
                next(3);
            }
            break;
        case 3: // at speed, past the mark: crouch
            cl.viewangles[YAW] = c.yaw;
            if(ground && speed > 300.f && along > c.go)
            {
                t.z0 = t.zTop = at.z;
                t.jumpsBefore = s.jumpEnds;
                Cbuf_InsertText("+flatcrouch\n");
                next(4);
            }
            break;
        case 4: // sliding
            if(!s.on)
            {
                break;
            }
            t.zTop = za::max(t.zTop, at.z);
            if(c.side != 0 && inPhase > 0.15)
            {
                cl.viewangles[YAW] = anglemod(heading + 10.f * static_cast<float>(c.side));
                t.heading0 = heading;
                Cbuf_InsertText(c.side > 0 ? "-forward\n+moveleft\n" : "-forward\n+moveright\n");
                next(5);
            }
            else if(c.side == 0 && inPhase > 0.1)
            {
                next(6);
            }
            break;
        case 5: // the side key held: the view kept 10 degrees off the slide's way it started from
            cl.viewangles[YAW] = anglemod(t.heading0 + 10.f * static_cast<float>(c.side));
            if(inPhase >= 0.5 || !s.on)
            {
                float turned = heading - t.heading0;
                turned = turned > 180.f ? turned - 360.f : (turned < -180.f ? turned + 360.f : turned);
                const float want = 0.5f * za::clamp(vr_flat_slide_steer.value, 0.f, 180.f) * static_cast<float>(inPhase);
                q_snprintf(line, sizeof(line), "%s alone, the view 10 degrees %s of the slide: turned %.1f degrees in %.2f s (at least %.1f), sliding %d",
                    c.side > 0 ? "A" : "D", c.side > 0 ? "left" : "right", turned, inPhase, want, s.on ? 1 : 0);
                testCheck(t.tally, tag, turned * static_cast<float>(c.side) >= want && want > 0.f, line);
                t.index++;
                next(t.index < caseCount ? 1 : 99);
            }
            break;
        case 6: // up the stairs or the ramp, until it ends
            if(s.on)
            {
                t.zTop = za::max(t.zTop, at.z);
                break;
            }
            {
                const float rose = t.zTop - t.z0;
                const int jumps = s.jumpEnds - t.jumpsBefore;
                if(t.index == 0)
                {
                    q_snprintf(line, sizeof(line), "the stairs: it rose %.0f units sliding (at least 48: three steps), %d jump ends, ends at %.0f u/s (%s)",
                        rose, jumps, speed, s.endWhy);
                    testCheck(t.tally, tag, rose >= 48.f && jumps == 0, line);
                }
                else
                {
                    q_snprintf(line, sizeof(line), "the ramp: it rose %.0f units sliding (at least 16), %d jump ends, ends at %.0f u/s (%s)", rose,
                        jumps, speed, s.endWhy);
                    testCheck(t.tally, tag, rose >= 16.f && jumps == 0, line);
                }
                t.index++;
                next(t.index < caseCount ? 1 : 99);
            }
            break;
        default:
            Cbuf_InsertText("-forward\n-moveleft\n-moveright\n-flatcrouch\n");
            t.tally.verdict(tag);
            t = RoughTest{};
            break;
    }
}

// The largest second difference of a run of samples: a line (even motion, or an even fall) gives 0, a shake its size.
[[nodiscard]] float largestBend(const float* v, int n)
{
    float most = 0.f;
    for(int i = 1; i + 1 < n; ++i)
    {
        most = za::max(most, za::abs(v[i + 1] - 2.f * v[i] + v[i - 1]));
    }
    return most;
}

// vr_flatslide_ramp_test (e1m1, his coordinates): slides along e1m1's ramps, down and up (rampRuns): placed before the
// ramp, he runs along it and crouches at speed; while he slides, every frame: the view's height, his origin's, the drawn
// main hand from the view and the drawn body's main shoulder and pelvis from the view. Checks, each run: the slide went
// along the ramp, on the ground throughout; on the slope, the view's and the origin's heights bend by at most
// rampBendMost a frame (a shake bends them by a unit or more); the whole slide, the gun and the body from the view by at
// most rampGunBendMost on any axis (2026-11-09: the shoulders and the gun jumped 3.7 units forward where the slide's
// body share and the crouched trunk's squat floor switched: vr_avatar_pitch.cpp, solveTorso).
// `vr_flatslide_ramp_test 1` prints every frame.
constexpr float rampBendMost = 0.5f; // (the crouch and slide drop easing in bend the view by 0.3: smooth; a bounce off the ramp bends it by a unit and more)
constexpr float rampGunBendMost = 0.5f;

struct RampRun
{
    const char* name;
    glm::vec3 start; // placed here (his origin), at rest
    float yaw;       // runs this way
    float crouchAt;  // crouches this far along, at speed
    float length;    // samples until this far along (short of the floor past the ramp's far end), or the slide's end
    bool grounded;   // on the ground throughout (a ramp met at speed from a level floor throws him off its lip: Quake's own)
};
// The spiral room's ramp, its first flight (x 896..1168 at y 1968..2128, 13 degrees down east; a button at its foot),
// and the ramp under the bridge's buttons (x -185..128 at y 656..720, 14 degrees down south: met at speed off the level
// floor, he flies off its lip and lands past it). Not up a ramp: a slide up 13 degrees ends at once ("slowed"). (The
// spiral's second flight, down north from its landing, bent the gun 0.56 where a slide begins at the lip, in the air:
// ROUND24.md, "Ramp slide jitter".)
constexpr RampRun rampRuns[] = {
    {"spiral ramp, down east", {900.f, 2048.f, -150.f}, 0.f, 20.f, 200.f, true},
    {"button ramp, down south", {-50.f, 800.f, -198.f}, 270.f, 20.f, 220.f, false},
};
constexpr int rampRunCount = static_cast<int>(sizeof(rampRuns) / sizeof(rampRuns[0]));

// The largest bend of a run of points, on any axis.
[[nodiscard]] float largestBend3(const glm::vec3* v, int n)
{
    float axis[RampTest::most];
    float most = 0.f;
    for(int a = 0; a < 3; ++a)
    {
        for(int i = 0; i < n; ++i)
        {
            axis[i] = v[i][a];
        }
        most = za::max(most, largestBend(axis, n));
    }
    return most;
}

// The largest second difference of a run of samples, over the ones whose neighbours are on the slope too.
[[nodiscard]] float largestSlopeBend(const float* v, const bool* slope, int n)
{
    float most = 0.f;
    for(int i = 1; i + 1 < n; ++i)
    {
        if(slope[i - 1] && slope[i] && slope[i + 1])
        {
            most = za::max(most, za::abs(v[i + 1] - 2.f * v[i] + v[i - 1]));
        }
    }
    return most;
}

void rampTestFrame()
{
    RampTest& t = client.rampTest;
    if(t.phase == 0)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    const char* tag = "flatslide_ramp";
    const double now = qcvm->time;
    const Slide& s = server.slide[1];
    const float speed = groundSpeed(pl);
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frames = 0;
    };
    const RampRun& r = rampRuns[za::min(t.run, rampRunCount - 1)];
    const glm::vec2 dir{za::cos(glm::radians(r.yaw)), za::sin(glm::radians(r.yaw))};
    const float along = glm::dot(glm::vec2{pl->v.origin[0] - r.start.x, pl->v.origin[1] - r.start.y}, dir);
    // The next run (or the verdict after the last).
    const auto runDone = [&]() {
        Cbuf_InsertText("-forward\n-flatcrouch\n");
        flatplay::traceBody(false);
        t.run++;
        t.n = t.air = 0;
        next(t.run < rampRunCount ? 1 : 99);
    };
    char line[256];
    if(inPhase > 6.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "%s: timed out (step %d: %.0f along, speed %.0f, ground %d, sliding %d)", r.name, t.phase,
            along, speed, ground ? 1 : 0, s.on ? 1 : 0);
        testCheck(t.tally, tag, false, line);
        runDone();
        return;
    }
    switch(t.phase)
    {
        case 1: // place him before the ramp, the keys let go (not setpos: it turns noclip on)
            Cbuf_InsertText("-forward\n-moveleft\n-moveright\n-flatcrouch\n");
            pl->v.origin[0] = r.start.x;
            pl->v.origin[1] = r.start.y;
            pl->v.origin[2] = r.start.z;
            pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
            SV_LinkEdict(pl, false);
            flatplay::traceBody(true);
            next(2);
            break;
        case 2: // settled (and up out of the last run's crouch): run along the ramp
            cl.viewangles[YAW] = r.yaw;
            cl.viewangles[PITCH] = 0.f;
            if(++t.frames > 10 && ground && !s.on && now > s.cooldownUntil && flatplay::crouchRatio() < 0.01f)
            {
                Cbuf_InsertText("+forward\n");
                next(3);
            }
            break;
        case 3: // at speed: crouch
            cl.viewangles[YAW] = r.yaw;
            if(ground && speed > 300.f && along > r.crouchAt)
            {
                Cbuf_InsertText("+flatcrouch\n");
                next(4);
            }
            break;
        case 4: // sliding along the ramp: a sample a frame, from the slide's 4th frame (its pose eased in by then or not,
                // an ease is smooth) to `length` along
        {
            cl.viewangles[YAW] = r.yaw;
            if(!s.on && t.frames == 0)
            {
                break;
            }
            if(++t.frames >= 4 && along < r.length && t.n < RampTest::most && s.on)
            {
                const glm::vec3 eye = flatplay::lastViewOrigin();
                t.eye[t.n] = eye.z;
                t.feet[t.n] = pl->v.origin[2];
                t.slope[t.n] = ground && za::abs(pl->v.velocity[2]) > 10.f;
                t.gun[t.n] = flatplay::lastMainHand() - eye;
                t.shoulder[t.n] = flatplay::lastMainShoulder() - eye;
                t.pelvis[t.n] = flatplay::lastPelvis() - eye;
                t.air += ground ? 0 : 1;
                if(t.verbose)
                {
                    const glm::vec3& g = t.gun[t.n];
                    const glm::vec3& sh = t.shoulder[t.n];
                    const glm::vec3& pv = t.pelvis[t.n];
                    Con_Printf("flatslide_ramp: run %d frame %d along %.0f z %.2f vz %.1f ground %d speed %.0f eye %.2f gun %.2f %.2f %.2f "
                               "shoulder %.2f %.2f %.2f pelvis %.2f %.2f %.2f\n",
                        t.run, t.n, along, pl->v.origin[2], pl->v.velocity[2], ground ? 1 : 0, speed, eye.z, g.x, g.y, g.z, sh.x, sh.y, sh.z,
                        pv.x, pv.y, pv.z);
                }
                t.n++;
            }
            if(along >= r.length || !s.on || t.n >= RampTest::most)
            {
                const float eyeBend = largestSlopeBend(t.eye, t.slope, t.n);
                const float feetBend = largestSlopeBend(t.feet, t.slope, t.n);
                const float gunBend = largestBend3(t.gun, t.n);
                const float bodyBend = za::max(largestBend3(t.shoulder, t.n), largestBend3(t.pelvis, t.n));
                const float rise = t.n > 1 ? t.feet[t.n - 1] - t.feet[0] : 0.f;
                // (At the real clock's pace -- vr_fixed_frames 0, frames shorter than a tick -- Quake's walk down a slope leaves
                // the ground for a frame now and then and his origin bounces (measured 2026-11-09: 1.4 a frame, 4 frames off
                // it); the view hides it (0.13). Said, not failed: Quake's own move, not the slide's.)
                const bool fixedClock = vr_fixed_frames.value != 0.f;
                q_snprintf(line, sizeof(line), "%s: %d frames sliding, %.0f units %s, %d off the ground (%s)%s", r.name, t.n, za::abs(rise),
                    rise > 0.f ? "up" : "down", t.air, s.on ? "still sliding" : s.endWhy, fixedClock ? "" : " (real clock: not counted)");
                testCheck(t.tally, tag, t.n >= 8 && za::abs(rise) >= 8.f && (t.air == 0 || !r.grounded || !fixedClock), line);
                q_snprintf(line, sizeof(line), "%s: on the slope, the view's height bends at most %.3f a frame (at most %.2f), his origin's %.3f%s",
                    r.name, eyeBend, rampBendMost, feetBend, fixedClock ? "" : " (real clock: not counted)");
                testCheck(t.tally, tag, t.n >= 8 && eyeBend <= rampBendMost && (feetBend <= rampBendMost || !fixedClock), line);
                q_snprintf(line, sizeof(line), "%s: the gun from the view bends at most %.3f a frame, the body (shoulder, pelvis) %.3f (at most %.2f)",
                    r.name, gunBend, bodyBend, rampGunBendMost);
                testCheck(t.tally, tag, t.n >= 8 && gunBend <= rampGunBendMost && bodyBend <= rampGunBendMost, line);
                runDone();
            }
            break;
        }
        default:
            Cbuf_InsertText("-forward\n-moveleft\n-moveright\n-flatcrouch\n");
            flatplay::traceBody(false);
            t.tally.verdict(tag);
            t = RampTest{};
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
                next(8);
            }
            break;
        case 8: // straight again by the wall: lean away from it
        case 10:
        case 12:
            if(inPhase > 0.1 && client.lean.dist == 0.f)
            {
                if(t.phase == 10)
                {
                    // Face the wall he stands against (it was at his side, 17 units off): the line from the gun's place
                    // 20 ahead started inside it, and the lean got no room either way (his report, 2026-11-08).
                    cl.viewangles[YAW] = anglemod(cl.viewangles[YAW] - 90.f * t.wallSide);
                }
                const bool right = t.phase == 8 ? t.wallSide < 0.f : t.phase == 10;
                (right ? client.lean.cmdR : client.lean.cmdL) = true;
                next(t.phase + 1);
            }
            break;
        case 9:
        case 11:
        case 13:
            if(inPhase > time + 0.2)
            {
                const hands::State& s = hands::current();
                const entity_t& pl = cl_entities[cl.viewentity];
                const glm::vec3 straight{pl.origin[0], pl.origin[1], pl.origin[2] + cl.viewheight - flatplay::crouchDrop()};
                const float way = client.lean.cmdR ? 1.f : -1.f;
                const float out = way * glm::dot(s.head - straight, levelRight(cl.viewangles[YAW]));
                q_snprintf(line, sizeof(line), "%s, leant %s: the eye %.1f units out (of %.0f)",
                    t.phase == 9 ? "a wall at his side" : "facing a wall he stands against", way > 0.f ? "right" : "left", out, full);
                testCheck(t.tally, tag, za::abs(out - full) < 0.5f, line);
                client.lean.cmdL = client.lean.cmdR = false;
                next(t.phase == 13 ? 99 : t.phase + 1);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = LeanTest{};
            break;
    }
}

// vr_flatprone_test (a weapon in the main hand, room around): lying down, a full lean right moves the eye
// vr_flat_lean_prone_dist and rolls the view and the lying body vr_flat_lean_prone_angle; the crouch pressed lying down
// gets up into the crouch, the gun's pose without a jump; lying down and looking down, the gun lies flush on the floor:
// no shot, the ironsight waits; looking level again the ironsight comes.
void proneTestFrame()
{
    ProneTest& t = client.proneTest;
    if(t.phase == 0)
    {
        return;
    }
    const char* tag = "flatprone";
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    if(t.pitchHeld)
    {
        cl.viewangles[PITCH] = t.pitch;
    }
    const float time = za::clamp(vr_flat_lean_time.value, 0.05f, 1.f);
    char line[256];
    if(inPhase > 8.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d)", t.phase);
        testCheck(t.tally, tag, false, line);
        next(99);
    }
    const hands::State& s = hands::current();
    const auto handInView = [&]() {
        const glm::vec3 d = s.pos[HAND_MAIN] - s.head;
        const float yaw = cl.viewangles[YAW];
        return glm::vec3{glm::dot(d, levelFwd(yaw)), glm::dot(d, levelRight(yaw)), d.z};
    };
    switch(t.phase)
    {
        case 1:
            Cbuf_InsertText("vr_prone_toggle\n");
            next(2);
            break;
        case 2: // lying down: lean right
            if(inPhase > 0.3 && flatplay::proneRatio() >= 1.f)
            {
                client.lean.cmdR = true;
                next(3);
            }
            break;
        case 3:
            if(inPhase > time + 0.2)
            {
                const entity_t& pl = cl_entities[cl.viewentity];
                const glm::vec3 straight{pl.origin[0], pl.origin[1], pl.origin[2] + cl.viewheight - flatplay::crouchDrop()};
                const float side = glm::dot(s.head - straight, levelRight(cl.viewangles[YAW]));
                const float dist = za::clamp(vr_flat_lean_prone_dist.value, 0.f, leanDistMost);
                const float angle = za::clamp(vr_flat_lean_prone_angle.value, 0.f, leanAngleMost);
                q_snprintf(line, sizeof(line), "lying down, leant right: the eye %.1f units right (of %.0f), roll %.1f (of %.0f), the body's roll %.1f",
                    side, dist, client.lean.roll, angle, proneLeanRoll());
                testCheck(t.tally, tag,
                    za::abs(side - dist) < 0.3f && za::abs(client.lean.roll - angle) < 0.1f && za::abs(proneLeanRoll() - angle) < 0.1f, line);
                client.lean.cmdR = false;
                next(4);
            }
            break;
        case 4: // straight again: the crouch pressed
            if(inPhase > 0.1 && client.lean.dist == 0.f)
            {
                t.handWas = handInView();
                t.biggestStep = 0.f;
                Cbuf_InsertText("+flatcrouch\n");
                next(5);
            }
            break;
        case 5:
        {
            const glm::vec3 h = handInView();
            t.biggestStep = za::max(t.biggestStep, glm::distance(h, t.handWas));
            t.handWas = h;
            if(inPhase > 0.2 && flatplay::proneRatio() <= 0.f && flatplay::crouchRatio() >= 1.f)
            {
                q_snprintf(line, sizeof(line), "the crouch lying down: up into the crouch (prone %.2f, crouch %.2f) in %.2f s, the gun's largest step %.2f units a frame",
                    flatplay::proneRatio(), flatplay::crouchRatio(), inPhase, t.biggestStep);
                testCheck(t.tally, tag, t.biggestStep < 1.5f, line);
                Cbuf_InsertText("-flatcrouch\n");
                next(6);
            }
            break;
        }
        case 6: // standing again: lie down, look down
            if(inPhase > 0.2 && flatplay::crouchRatio() < 0.01f)
            {
                Cbuf_InsertText("vr_prone_toggle\n");
                next(7);
            }
            break;
        case 7:
            if(inPhase > 0.3 && flatplay::proneRatio() >= 1.f)
            {
                t.pitch = 80.f;
                t.pitchHeld = true;
                Cbuf_InsertText("vr_flat_aim 1\n");
                next(8);
            }
            break;
        case 8: // looking down lying down, the ironsight asked for
            if(inPhase > 0.8)
            {
                float left = 0.f;
                const bool flush = flatsight::floorFlush(HAND_MAIN, &left);
                q_snprintf(line, sizeof(line), "lying down, looking down: the gun flush on the floor (%d, tilted %.1f deg, left %.2f in it), no shot (%d), the ironsight waits (%.2f)",
                    flush ? 1 : 0, flatsight::wallTilt(HAND_MAIN), left, flatsight::wallBlocksFire(HAND_MAIN) ? 1 : 0, flatsight::adsShare());
                testCheck(t.tally, tag,
                    flush && flatsight::wallTilt(HAND_MAIN) > 5.f && left < 0.5f && flatsight::wallBlocksFire(HAND_MAIN) && flatsight::adsShare() < 0.01f, line);
                t.pitch = 0.f;
                next(9);
            }
            break;
        case 9: // looking level: off the floor, the ironsight comes
            if(inPhase > 1.0)
            {
                q_snprintf(line, sizeof(line), "lying down, looking level: off the floor (%d), shots (%d), the ironsight on (%.2f)",
                    flatsight::floorFlush(HAND_MAIN) ? 1 : 0, flatsight::wallBlocksFire(HAND_MAIN) ? 0 : 1, flatsight::adsShare());
                testCheck(t.tally, tag, !flatsight::floorFlush(HAND_MAIN) && !flatsight::wallBlocksFire(HAND_MAIN) && flatsight::adsShare() > 0.9f, line);
                Cbuf_InsertText("vr_flat_aim 0; vr_prone_toggle\n");
                t.pitchHeld = false;
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = ProneTest{};
            break;
    }
}

// vr_flatslide_selftest: the slide's step on made-up floors, 72 steps a second.
// vrmantle.map's blocks (Misc/quakevr/climb/make_vrmantle_map.py): along x, each 96 wide with a 64 gap, the lip along
// y 0 facing -y, the floor at z 0. A case: the block, the look up (degrees), whether it mantles.
struct MantleCase
{
    int block;
    float lip;
    float up;
    bool mantles;
};
constexpr MantleCase mantleCases[] = {
    {0, 48.f, 35.f, true},   // waist high
    {1, 64.f, 35.f, true},
    {2, 80.f, 35.f, true},   // over the eyes
    {3, 96.f, 35.f, true},   // reached at the top of the jump
    {4, 128.f, 35.f, false}, // too high
    {5, 160.f, 35.f, false},
    {1, 64.f, 20.f, false},  // looking up 20: under vr_flat_mantle_look (30), no mantle
};
constexpr float mantleBlockStep = 160.f; // the blocks' width and gap
constexpr float mantleStandY = -18.f;    // his place in front of a block: its lip 2 units ahead of his box

void mantleTestFrame()
{
    MantleTest& t = client.mantleTest;
    if(t.phase == 0)
    {
        return;
    }
    progs::ServerVm vm;
    edict_t* pl = progs::localPlayer();
    if(!pl)
    {
        return;
    }
    const char* tag = "flatmantle";
    const double now = qcvm->time;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frames = 0;
    };
    cl.viewangles[PITCH] = t.pitch;
    cl.viewangles[YAW] = 90.f;
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    bool mantling = false;
    const int mantles = climb::flatMantles(pl, mantling);
    constexpr int caseCount = static_cast<int>(sizeof(mantleCases) / sizeof(mantleCases[0]));
    if(t.phase != 1 && now - t.phaseAt > 6.0)
    {
        char line[160];
        q_snprintf(line, sizeof(line), "case %d timed out (step %d, ground %d, mantling %d)", t.index, t.phase, ground ? 1 : 0,
            mantling ? 1 : 0);
        testCheck(t.tally, tag, false, line);
        Cbuf_InsertText("-jump\n");
        t.index++;
        next(1);
        return;
    }
    switch(t.phase)
    {
    case 1: // placed in front of the next block
        if(t.index >= caseCount || (t.only >= 0 && t.index > t.only))
        {
            Cbuf_InsertText("-jump\n");
            t.tally.verdict(tag);
            t = MantleTest{};
            return;
        }
        {
            const MantleCase& k = mantleCases[t.index];
            t.pitch = -k.up;
            const float x = static_cast<float>(k.block) * mantleBlockStep + 48.f;
            pl->v.origin[0] = x;
            pl->v.origin[1] = mantleStandY;
            pl->v.origin[2] = 24.f;
            pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
            SV_LinkEdict(pl, false);
        }
        next(2);
        break;
    case 2: // settled on the floor, the look held: the jump
        // (40 frames: past the climb's 0.4 s without a new hold after the last mantle, as a player's next jump would be)
        t.frames = ground ? t.frames + 1 : 0;
        if(t.frames >= 40)
        {
            t.mantlesBefore = mantles;
            t.airborne = false;
            t.onLedge = 0;
            t.empty = -1;
            Cbuf_InsertText("+jump\n");
            next(3);
        }
        break;
    case 3: // in the air, mantling or not, until he stands again
        if(!ground)
        {
            t.airborne = true;
        }
        if(mantling)
        {
            // (Its hands on the ledge: the bits the drawn hands are put on holds by; vr_climb.cpp flatMantleHands.)
            if(t.empty < 0)
            {
                t.empty = (climb::handFree(pl, 0, false) ? 1 : 0) | (climb::handFree(pl, 1, false) ? 2 : 0);
            }
            t.onLedge |= cl.stats[protocol::STAT_QVR_CLIMB] & 3;
        }
        t.frames = t.airborne && ground && !mantling ? t.frames + 1 : 0;
        if(t.frames >= 5)
        {
            const MantleCase& k = mantleCases[t.index];
            const float feet = pl->v.origin[2] + pl->v.mins[2];
            const bool mantled = mantles > t.mantlesBefore;
            const bool ok = k.mantles ? mantled && za::abs(feet - k.lip) < 2.f : !mantled && za::abs(feet) < 2.f;
            char line[200];
            q_snprintf(line, sizeof(line), "lip %.0f, looking up %.0f: %s (feet at %.1f; wanted %s)", k.lip, k.up,
                mantled ? "mantled" : "no mantle", feet, k.mantles ? "a mantle" : "none");
            testCheck(t.tally, tag, ok, line);
            if(mantled)
            {
                // The empty hands on the ledge, the others not (vr_flat_mantle_hands; his notes 2026-11-08).
                const int want = vr_flat_mantle_hands.value != 0.f ? za::max(t.empty, 0) : 0;
                q_snprintf(line, sizeof(line), "lip %.0f: hands on the ledge off %d main %d (empty: off %d main %d)", k.lip,
                    t.onLedge & 1, (t.onLedge >> 1) & 1, za::max(t.empty, 0) & 1, (za::max(t.empty, 0) >> 1) & 1);
                testCheck(t.tally, tag, t.empty >= 0 && t.onLedge == want, line);
            }
            Cbuf_InsertText("-jump\n");
            t.index++;
            next(1);
        }
        break;
    default:
        break;
    }
}

void mantleTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatmantle_test: flat-screen play (-flatscreenmode) in a game you host, on map vrmantle\n");
        return;
    }
    client.mantleTest = MantleTest{};
    client.mantleTest.phase = 1;
    if(Cmd_Argc() > 1)
    {
        client.mantleTest.only = za::max(0, Q_atoi(Cmd_Argv(1)));
        client.mantleTest.index = client.mantleTest.only;
    }
}

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

void roughTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatslide_rough_test: flat-screen play (-flatscreenmode) in a game you host, in vrtesthall\n");
        return;
    }
    client.roughTest = RoughTest{};
    client.roughTest.phase = 1;
    progs::ServerVm vm;
    client.roughTest.phaseAt = qcvm->time;
}

void rampTest_f()
{
    if(!flatplay::active() || !sv.active || q_strcasecmp(sv.name, "e1m1"))
    {
        Con_Printf("vr_flatslide_ramp_test [1: every frame]: slides down and up e1m1's ramps; flat-screen play (-flatscreenmode) in e1m1, a game you host\n");
        return;
    }
    client.rampTest = RampTest{};
    client.rampTest.phase = 1;
    client.rampTest.verbose = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) != 0;
    progs::ServerVm vm;
    client.rampTest.phaseAt = qcvm->time;
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

void proneTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatprone_test: flat-screen play (-flatscreenmode) in a game you host, a weapon held\n");
        return;
    }
    client.proneTest = ProneTest{};
    client.proneTest.phase = 1;
    client.proneTest.phaseAt = vr_gametime;
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
        // Toward any way but straight back: A or D alone ask for 90 degrees off the view, and the slide's way is seldom
        // the view's (he turned the mouse, or it already curved), so a side key often asked a little behind it and
        // the old "only ahead of it" turned nothing. A key held straight back (within steerBack of it) is no brake
        // and no turn round.
        if(za::abs(want) < glm::pi<float>() - steerBack)
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
    Cmd_AddCommand("vr_flatslide_rough_test", roughTest_f);
    Cmd_AddCommand("vr_flatslide_ramp_test", rampTest_f);
    Cmd_AddCommand("vr_flatlean_test", leanTest_f);
    Cmd_AddCommand("vr_flatprone_test", proneTest_f);
    Cmd_AddCommand("vr_flatmantle_test", mantleTest_f);
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
        roughTestFrame();
        rampTestFrame();
        leanTestFrame();
        proneTestFrame();
        mantleTestFrame();
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
    const float full = client.lean.full;
    return full > 0.f ? client.lean.dist / full : 0.f;
}

float proneLeanRoll()
{
    return flatplay::active() ? client.lean.roll * flatplay::proneRatio() : 0.f;
}

bool slidePose()
{
    if(flatplay::active() && sv.active && svs.maxclients >= 1)
    {
        return server.slide[1].on;
    }
    return sliding();
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
    // A jump: going up with the jump key held since it (QC PlayerJump clears FL_JUMPRELEASED until the key is let go).
    // Going up alone is no jump: up a ramp or a step at a slide's speed the floor's clip gives him more than jumpedUp.
    const int flags = static_cast<int>(ent->v.flags);
    const bool jumped = velocity[2] > flatmove::jumpedUp && (ent->v.button2 != 0.f || !(flags & FL_JUMPRELEASED));
    bool floorFound = false;
    const glm::vec3 normal = flatmove::floorNormal(ent, floorFound);
    const bool ground = onground || (floorFound && !jumped);
    const bool landed = ground && !s.grounded;
    s.held = held;
    s.grounded = ground;
    const float zWas = s.lastZ;
    s.lastZ = ent->v.origin[2];
    const glm::vec2 vel{velocity[0], velocity[1]};
    const float speed = glm::length(vel);

    if(!flatmove::slideOn() || !held || !hull::isFlat(ent) || hull::isProne(ent) || ent->v.waterlevel >= 2 ||
       static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        s.endWhy = s.on ? (!held ? "the key let go" : hull::isProne(ent) ? "prone" : ent->v.waterlevel >= 2 ? "water" : "not a flat-screen walk") : s.endWhy;
        flatmove::endSlide(s, now);
        return 0;
    }
    if(s.on && !ground)
    {
        if(jumped && !s.airborne)
        {
            s.jumpEnds++;
            s.endWhy = "a jump";
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
    // Up a step (the last move rose on a level floor: a ramp's rise is its slope's): the speed a ramp as steep would
    // have taken over the step's length, vr_flat_slide_slope * rise / speed. Stairs slow it as a ramp does.
    glm::vec2 stepped = vel;
    const float rise = ent->v.origin[2] - zWas;
    if(rise > 1.f && rise < 24.f && glm::length(glm::vec2{normal.x, normal.y}) <= 0.02f && speed > 1.f)
    {
        const float loss = za::clamp(vr_flat_slide_slope.value, 0.f, 2000.f) * rise / za::max(speed, 100.f);
        stepped *= za::max(0.f, speed - loss) / speed;
    }
    const flatmove::SlideStep st =
        flatmove::slideStep(stepped, normal, wish, static_cast<float>(host_frametime));
    velocity[0] = st.vel.x;
    velocity[1] = st.vel.y;
    if(st.ended)
    {
        s.endWhy = "slowed";
        flatmove::endSlide(s, now); // into the crouch: the walk's friction from the next move
    }
    return 1;
}

extern "C" int VR_FlatSlideStepUp(edict_t* ent)
{
    const int num = flatmove::clientSlot(ent);
    if(num < 0)
    {
        return 0;
    }
    const flatmove::Slide& s = flatmove::server.slide[num];
    return s.on && !s.airborne && s.grounded ? 1 : 0;
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
    for(const cvar_t* c : {&vr_flat_key_lean_left, &vr_flat_key_lean_right, &vr_flat_key_swap, &vr_flat_key_grab, &vr_flat_key_grab_alt, &vr_flat_key_reload,
            &vr_flat_key_prone, &vr_flat_key_bullettime, &vr_flat_key_flycam, &vr_flat_key_toolgun, &vr_flat_key_toolgun_menu, &vr_flat_key_toolgun_adjust})
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
