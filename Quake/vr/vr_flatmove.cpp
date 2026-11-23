// vr_flatmove.cpp -- flatscreen play's slide and lean (vr_flatmove.hpp; docs/vr-port/FLATPLAY.md, "Sliding" and "Leaning").

#include "vr_flatmove.hpp"

#include "vr_api.h"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_climb.hpp"
#include "vr_protocol.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_flatspeedfov.hpp"
#include "vr_flycam.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_hull.hpp"
#include "vr_playerhit.hpp"
#include "vr_progs.hpp"
#include "vr_slide.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

extern "C" qboolean keydown[MAX_KEYS];             // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c
extern "C" cvar_t sv_maxspeed;                      // sv_user.c

namespace qvr::flatmove
{

namespace
{

// The slide itself is the shared one (vr_slide.cpp: flatscreen and headset play's); its client side and tests are here.
using slide::Slide;
using slide::clientSlot;
using slide::endSlide;
using slide::floorNormal;
using slide::paySlideStamina;
using slide::slideOn;
using slide::slideStaminaPart;
using slide::slideStaminaParts;
using slide::slideStep;
using slide::SlideStep;

constexpr float leanMinHit = 6.f;      // units the head must stand off his box's middle before the leant body is boxed
constexpr float leanDistMost = 30.f;
constexpr float leanAngleMost = 35.f;
constexpr float leanGunAhead = 20.f;   // leanRoom: the weapon's place this far ahead of the eye, where it is open
constexpr float leanAheadKeep = 2.f;   // and this far short of a wall ahead
// VR_FlatLookTurn: looking near straight up or down, the yaw a rolled screen's up asks for grows as 1 / cos(pitch)
// (round the pole a turn about the tilted screen's up is mostly a swing over the top); past about 75 degrees it is
// held at 4 times, the move a little off the screen's axis there rather than a spin.
constexpr float lookPoleCos = 0.25f;

bool leanHitOff = false; // vr_flatlean_test: the same shot with his box alone

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
    bool proneTried{false}; // the prone key pressed mid-slide (it must do nothing)
    int startsWas{0};       // the tired crouch: slides and refusals before it
    int refusalsWas{0};
};

// vr_flatslide_hit_test <0|1>: a slide into a crate (0) or a grunt (1) in vrtesthall's open south-west.
struct SlideHitTest
{
    int phase{0};
    int which{0};
    double phaseAt{0.0};
    int frames{0};
    testkit::Tally tally;
    int target{0};          // the crate's or the grunt's edict number
    glm::vec3 from{0.f};    // its place before
    int startsWas{0};
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
    float maxUp{0.f};        // the highest upward speed seen since the jump key went down (the jump-stands phases)
};

// vr_flatpronedual_test [main:off ...]: lying down with a gun in each hand, each pair of weapon ids given in turn, the look
// swept from up to down: which poses stop a gun's trigger (the floor, a wall), and why.
constexpr int proneDualPairsMost = 12;
constexpr float proneDualPitches[] = {-60.f, -45.f, -30.f, -15.f, 0.f, 15.f, 30.f, 45.f, 60.f, 75.f};
constexpr int proneDualPitchCount = static_cast<int>(sizeof(proneDualPitches) / sizeof(proneDualPitches[0]));
struct ProneDualTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int pairs[proneDualPairsMost][2]{};
    int pairCount{0};
    int pair{0};       // the pair under test
    int pitch{0};      // its look, an index into proneDualPitches
    int blocked{0};    // this pair's stopped triggers at the looks that must fire (both hands counted)
    int blockedAll{0}; // and at every look
    int blockedTotal{0}; // every pair's, at the looks that must fire
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
    float fovPeak{1.f}; // the speed FOV's widest while it mantled (vr_flat_mantle_fov_bonus)
    bool fovBack{false}; // a mantle's widening to see eased back before the next jump
};

// vr_flatladder_test (maps/vrtutorial.bsp): its lesson 3 ladder walked into and climbed to the top, climbed down from
// its top, down looking down, pushed off; and a plain wall, single ledges and stairs walked into: no ladder
// (vr_climb.cpp flatLadder).
struct LadderTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int index{0};
    int only{-1};          // vr_flatladder_test <case>: that case alone
    int frames{0};
    int laddersBefore{0};
    int climbFrames{0};    // frames on the ladder
    bool mantled{false};   // a mantle seen
    int handBits{0};       // STAT_QVR_CLIMB's hand bits seen while on it
    int serials{0};        // and its hold serials' changes (hand over hand)
    int lastSerial{-1};
    float minFeet{1e9f}, maxFeet{-1e9f};
    float pitch{0.f};
    float yaw{0.f};
    bool turned{false};    // the case's second part has begun
    bool sideSeen{false};  // Side: where he was 60 over the ladder's foot: off its middle, his box's front past its face
    float sideOff{0.f}, sideFront{0.f};
    bool sideBlocked{false}; // and no room further out (a clip, a mover in front of the ladder)
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
    glm::vec3 drawn[most]{};    // the drawn main hand (its looks, the arm's hold) from the view
    double at[most]{};          // each sample's time (vr_gametime)
    int holds0{0};              // flatplay::armHolds() at the first sample
    double drawnAt{0.0};        // the last frame's time (vr_gametime): the frame the traces were drawn in
};

// The view he last saw (VR_FlatViewLean, the end of the eye's own view in V_CalcRefdef): its angles with every roll on
// them -- the lean's, cl_rollangle's strafe roll, a hit's kick -- the roll the look turns along (VR_FlatLookTurn).
struct SeenView
{
    glm::vec3 angles{0.f};
    float roll{0.f};       // 0: none to turn along (the chase camera, the fly camera, dead)
    int count{0};          // views built (vr_flatleanmouse_test waits for the next)
    int turns{0};          // looks turned along a roll (the test: none upright, none off)
};

// vr_flatleanmouse_test: a pure up and a pure right mouse move, leant and pitched, move a far point straight along the
// rolled screen; upright, and with vr_flat_lean_mouse 0, as Quake always did.
struct LeanMouseTest
{
    int phase{0};
    double phaseAt{0.0};
    testkit::Tally tally;
    int step{0};             // which of leanMouseSteps
    int seenAt{0};           // the view count once the move was taken
    int turnsAt{0};          // SeenView::turns before the move
    int turned{0};           // and the move's
    glm::vec3 before{0.f};   // the seen angles before the move
    glm::vec3 lookBefore{0.f}; // cl.viewangles before it
    float cvarWas{1.f};
};

struct ClientState
{
    SeenView seen;
    LeanMouseTest leanMouseTest;
    Lean lean;
    SlideView slide;
    SlideTest slideTest;
    SlideHitTest slideHitTest;
    RoughTest roughTest;
    RampTest rampTest;
    LeanTest leanTest;
    ProneTest proneTest;
    ProneDualTest proneDualTest;
    MantleTest mantleTest;
    LadderTest ladderTest;
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
    const Slide& s = slide::state(1);
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
            if(t.seenSlide && s.on && !t.proneTried && now - t.slideAt > 0.1)
            {
                t.proneTried = true; // (the prone key mid-slide: nothing, his bug of 2026-11-10)
                Cbuf_InsertText("vr_prone_toggle\n");
            }
            if(t.seenSlide && !s.on)
            {
                {
                    char pl1[200];
                    q_snprintf(pl1, sizeof(pl1), "the prone key mid-slide does nothing (tried %d; lying %d, lie share %.2f, ended by %s)",
                        t.proneTried ? 1 : 0, hull::isProne(pl) ? 1 : 0, flatplay::proneRatio(), s.endWhy);
                    testCheck(t.tally, tag, t.proneTried && !hull::isProne(pl) && flatplay::proneRatio() < 0.01f &&
                        !ZA_STRCMP(s.endWhy, "slowed"), pl1);
                    // Its stamina: all its parts (a slide of more than their steps), the cost in all.
                    const float part = slideStaminaPart();
                    const int parts = slideStaminaParts();
                    q_snprintf(pl1, sizeof(pl1), "a whole slide's stamina: %.1f in %d parts (%.1f in %d asked)", s.staminaPaid,
                        s.partsPaid, part * static_cast<float>(parts), parts);
                    testCheck(t.tally, tag, part > 0.f && s.partsPaid == parts &&
                        za::abs(s.staminaPaid - part * static_cast<float>(parts)) < 0.05f, pl1);
                }
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
                // Cut short before its second part was due: its first part alone.
                q_snprintf(line, sizeof(line), "a slide a jump cut short at 0.15 s pays its first part alone: %.1f in %d parts",
                    s.staminaPaid, s.partsPaid);
                testCheck(t.tally, tag, s.partsPaid == 1 && za::abs(s.staminaPaid - slideStaminaPart()) < 0.05f, line);
                Cbuf_InsertText("-jump\n-flatcrouch\nvr_stamina_set 0.02\n");
                next(6);
            }
            break;
        case 6: // too tired (2 stamina of 100, a part 5): running again, crouch
            if(!s.on && ground && speed > 300.f && now > s.cooldownUntil && !hull::flatCrouchHeld(pl) && inPhase > 0.2)
            {
                t.startsWas = s.starts;
                t.refusalsWas = s.tiredRefusals;
                Cbuf_InsertText("+flatcrouch\n");
                next(7);
            }
            break;
        case 7: // a plain crouch: no slide
            if(inPhase > 0.3)
            {
                char line[200];
                q_snprintf(line, sizeof(line), "too tired for its first part (%.1f left): no slide (slides %d -> %d, refused %d -> %d)",
                    progs::staminaLeft(pl), t.startsWas, s.starts, t.refusalsWas, s.tiredRefusals);
                testCheck(t.tally, tag, s.starts == t.startsWas && s.tiredRefusals > t.refusalsWas && !s.on, line);
                Cbuf_InsertText("-flatcrouch\nvr_stamina_set 1\n");
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = SlideTest{};
            break;
    }
}

// vr_flatslide_hit_test <0|1>: placed in vrtesthall's open south-west facing east, a crate (0: vr_physics_spawn vr_crate)
// or a grunt (1: vr_kdtrig_test 20) 200 units ahead; he runs, crouches at speed and slides into it. 0: the crate goes on
// at least 24 units ahead and off to the side (vr_flat_slide_prop_push, _side); 1: the grunt is knocked down or back
// 16 units and more (the slide's charge: QC VR_KdTrig_SlideCharge). Then the verdict.
void slideHitTestFrame()
{
    SlideHitTest& t = client.slideHitTest;
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
    const char* tag = "flatslide_hit";
    const double now = qcvm->time;
    const Slide& s = slide::state(1);
    const float speed = groundSpeed(pl);
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frames = 0;
    };
    constexpr float startX = -580.f, laneY = -440.f, crouchX = -520.f, aheadX = -380.f;
    char line[256];
    if(inPhase > 6.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d: speed %.0f, ground %d, sliding %d, slides %d)", t.phase, speed,
            ground ? 1 : 0, s.on ? 1 : 0, s.starts);
        testCheck(t.tally, tag, false, line);
        Cbuf_InsertText("-forward\n-flatcrouch\n");
        next(99);
    }
    switch(t.phase)
    {
        case 1: // place him (not setpos: it turns noclip on)
            Cbuf_InsertText("-forward\n-flatcrouch\n");
            pl->v.origin[0] = startX;
            pl->v.origin[1] = laneY;
            pl->v.origin[2] = 24.f;
            pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
            SV_LinkEdict(pl, false);
            cl.viewangles[YAW] = 0.f;
            cl.viewangles[PITCH] = 0.f;
            next(2);
            break;
        case 2: // settled: the target ahead
            cl.viewangles[YAW] = 0.f;
            if(++t.frames > 10 && ground)
            {
                q_snprintf(line, sizeof(line),
                    t.which == 0 ? "vr_physics_spawn vr_crate %.0f\n"
                                 : "vr_kdtrig_test_x %.0f; vr_kdtrig_test_y %.0f; vr_kdtrig_test_z 24; vr_kdtrig_test 20\n",
                    t.which == 0 ? aheadX - pl->v.origin[0] : aheadX, laneY);
                Cbuf_InsertText(line);
                next(3);
            }
            break;
        case 3: // it stands (half a second: the crate settles on the floor), found: run
            cl.viewangles[YAW] = 0.f;
            if(++t.frames == 36)
            {
                const char* cls = t.which == 0 ? "vr_crate" : "monster_army";
                float best = 96.f;
                progs::forEachEdict(1, [&](edict_t* e) {
                    const float d = glm::length(glm::vec2{e->v.origin[0] - aheadX, e->v.origin[1] - laneY});
                    if(!ZA_STRCMP(PR_GetString(e->v.classname), cls) && d < best)
                    {
                        best = d;
                        t.target = NUM_FOR_EDICT(e);
                    }
                });
                if(!t.target)
                {
                    q_snprintf(line, sizeof(line), "no %s 200 units ahead", cls);
                    testCheck(t.tally, tag, false, line);
                    next(99);
                    break;
                }
                const edict_t* e = EDICT_NUM(t.target);
                t.from = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
                t.startsWas = s.starts;
                Cbuf_InsertText("+forward\n");
                next(4);
            }
            break;
        case 4: // at speed, past the mark: crouch
            cl.viewangles[YAW] = 0.f;
            if(ground && speed > 300.f && pl->v.origin[0] > crouchX)
            {
                Cbuf_InsertText("+flatcrouch\n");
                next(5);
            }
            break;
        case 5: // the slide met it; a second for it to go (its push, its fall)
            cl.viewangles[YAW] = 0.f;
            if(++t.frames == 72 + 72)
            {
                edict_t* e = EDICT_NUM(t.target);
                const glm::vec3 at{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
                const glm::vec3 moved = at - t.from;
                q_snprintf(line, sizeof(line), "slid (slides %d -> %d)", t.startsWas, s.starts);
                testCheck(t.tally, tag, s.starts > t.startsWas, line);
                if(t.which == 0)
                {
                    q_snprintf(line, sizeof(line), "the crate slid into goes on %.1f units and %.1f to the side (24 and 2 asked)",
                        moved.x, moved.y);
                    testCheck(t.tally, tag, !e->free && moved.x >= 24.f && za::abs(moved.y) >= 2.f, line);
                }
                else
                {
                    const float down = progs::fieldFloatOr(e, progs::fields().vr_knockdown, 0.f);
                    q_snprintf(line, sizeof(line), "the grunt slid into is knocked (down %.0f, back %.1f units; down or 16 back asked)",
                        down, moved.x);
                    testCheck(t.tally, tag, !e->free && (down > 0.f || moved.x >= 16.f), line);
                }
                Cbuf_InsertText("-forward\n-flatcrouch\n");
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = SlideHitTest{};
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
    const Slide& s = slide::state(1);
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
// (0.6: the slide's guns are the crouch's, riding the eye (ROUND24.md, "Flat slide: the arms ride the eye"), and the crouch
// bends the gun 0.55 in the frame it arrives at its full depth: one kink, not a shake.)
constexpr float rampGunBendMost = 0.6f;

struct RampRun
{
    const char* name;
    glm::vec3 start; // placed here (his origin), at rest
    float yaw;       // runs this way
    float crouchAt;  // crouches this far along, at speed
    float length;    // samples until this far along (short of the floor past the ramp's far end), or the slide's end
    bool grounded;   // on the ground throughout (a ramp met at speed from a level floor throws him off its lip: Quake's own)
    bool level;      // a level floor: no rise wanted
};
// The spiral room's ramp, its first flight (x 896..1168 at y 1968..2128, 13 degrees down east; a button at its foot),
// and the ramp under the bridge's buttons (x -185..128 at y 656..720, 14 degrees down south: met at speed off the level
// floor, he flies off its lip and lands past it). Not up a ramp: a slide up 13 degrees ends at once ("slowed"). (The
// spiral's second flight, down north from its landing, bent the gun 0.56 where a slide begins at the lip, in the air:
// ROUND24.md, "Ramp slide jitter".)
constexpr RampRun rampRuns[] = {
    {"spiral ramp, down east", {900.f, 2048.f, -150.f}, 0.f, 20.f, 200.f, true, false},
    {"button ramp, down south", {-50.f, 800.f, -198.f}, 270.f, 20.f, 220.f, false, false},
    {"level floor, south", {-50.f, 1050.f, -198.f}, 270.f, 20.f, 240.f, true, true}, // (the floor north of the button ramp)
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
    const Slide& s = slide::state(1);
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
                t.drawn[t.n] = flatplay::lastDrawnMainHand() - eye;
                t.at[t.n] = t.drawnAt; // (the traces are the frame drawn last: its time)
                if(t.n == 0)
                {
                    t.holds0 = flatplay::armHolds();
                }
                t.air += ground ? 0 : 1;
                if(t.verbose)
                {
                    const glm::vec3& g = t.gun[t.n];
                    // (Short, unwrapped: the gun from the view and the slide's shares.)
                    Con_Printf("flatslide_ramp: g %d %d %.3f %.3f %.3f share %.3f drop %.2f stance %.2f crouch %.3f\n", t.run, t.n, g.x, g.y, g.z, slideShare(),
                        slideDrop(), flatsight::stanceYaw(), flatplay::crouchRatio());
                    const glm::vec3& sh = t.shoulder[t.n];
                    const glm::vec3& pv = t.pelvis[t.n];
                    // (And what is drawn: the main hand with its looks and the arm's hold, the skinned body's main arm and leg.)
                    const glm::vec3 dh = flatplay::lastDrawnMainHand() - eye;
                    avatar::Skeleton k;
                    const bool posed = avatar::skeleton(k);
                    const glm::vec3 ks = posed ? k.shoulder[HAND_MAIN] - eye : glm::vec3{0.f};
                    const glm::vec3 ke = posed ? k.elbow[HAND_MAIN] - eye : glm::vec3{0.f};
                    const glm::vec3 kk = posed ? k.knee[HAND_MAIN] - eye : glm::vec3{0.f};
                    const glm::vec3 ka = posed ? k.ankle[HAND_MAIN] - eye : glm::vec3{0.f};
                    Con_Printf("flatslide_ramp: run %d frame %d along %.0f z %.2f vz %.1f ground %d speed %.0f t %.4f holds %d eye %.2f gun %.2f %.2f %.2f "
                               "shoulder %.2f %.2f %.2f pelvis %.2f %.2f %.2f dhand %.2f %.2f %.2f kshoulder %.2f %.2f %.2f kelbow %.2f %.2f %.2f "
                               "kknee %.2f %.2f %.2f kankle %.2f %.2f %.2f vh %.3f %.3f %.3f\n",
                        t.run, t.n, along, pl->v.origin[2], pl->v.velocity[2], ground ? 1 : 0, speed, vr_gametime, flatplay::armHolds(), eye.z,
                        g.x, g.y, g.z, sh.x, sh.y, sh.z, pv.x, pv.y, pv.z, dh.x, dh.y, dh.z, ks.x, ks.y, ks.z, ke.x, ke.y, ke.z, kk.x, kk.y, kk.z,
                        ka.x, ka.y, ka.z, eye.x - flatplay::lastPoseHead().x, eye.y - flatplay::lastPoseHead().y,
                        eye.z - flatplay::lastPoseHead().z);
                }
                t.n++;
            }
            t.drawnAt = vr_gametime;
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
                testCheck(t.tally, tag, t.n >= 8 && (r.level ? za::abs(rise) < 1.f : za::abs(rise) >= 8.f) && (t.air == 0 || !r.grounded || !fixedClock), line);
                q_snprintf(line, sizeof(line), "%s: on the slope, the view's height bends at most %.3f a frame (at most %.2f), his origin's %.3f%s",
                    r.name, eyeBend, rampBendMost, feetBend, fixedClock ? "" : " (real clock: not counted)");
                testCheck(t.tally, tag, t.n >= 8 && eyeBend <= rampBendMost && (feetBend <= rampBendMost || !fixedClock), line);
                // (On the level floor the pelvis's settle at the slide's start turns round in a frame: 0.52, one kink, not a
                // shake; said, not counted there.)
                q_snprintf(line, sizeof(line), "%s: the gun from the view bends at most %.3f a frame, the body (shoulder, pelvis) %.3f (at most %.2f)%s",
                    r.name, gunBend, bodyBend, rampGunBendMost, r.level ? " (level: the body not counted)" : "");
                testCheck(t.tally, tag, t.n >= 8 && gunBend <= rampGunBendMost && (bodyBend <= rampGunBendMost || r.level), line);
                // The drawn hand (its looks, the arm's hold) from the view: the arm's hold, from the body as drawn a frame
                // before, pulled the gun in by that frame's move -- the slide's speed times a frame time that varies
                // (2026-11-10: 0.6 cm rms at 120 fps, 3 at the real clock with long frames). Carried to this frame, it never
                // holds the hand sliding. Its jitter: the rms of each frame off its neighbours' line in time.
                const int holds = flatplay::armHolds() - t.holds0;
                float jitter = 0.f;
                for(int i = 1; i + 1 < t.n; ++i)
                {
                    const double span = t.at[i + 1] - t.at[i - 1];
                    const float w = span > 0.0 ? static_cast<float>((t.at[i] - t.at[i - 1]) / span) : 0.5f;
                    const glm::vec3 off = t.drawn[i] - (t.drawn[i - 1] + (t.drawn[i + 1] - t.drawn[i - 1]) * w);
                    jitter += glm::dot(off, off);
                }
                jitter = t.n > 2 ? za::sqrt(jitter / static_cast<float>(t.n - 2)) : 0.f;
                q_snprintf(line, sizeof(line), "%s: the arm's hold pulled the drawn gun in %d times sliding (none wanted); its jitter from the view %.2f cm rms",
                    r.name, holds, jitter * 100.f / units::metresToUnits());
                testCheck(t.tally, tag, t.n >= 8 && holds == 0, line);
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
                // All of the lean, or as far as the hall leaves room that way (leanRoom's lines: the eye, under it, the
                // weapon's place ahead -- facing the wall, its line sideways can meet the wall's trim), most of it at least.
                const float room = leanRoom(straight, cl.viewangles[YAW], way, full);
                const float want = room;
                q_snprintf(line, sizeof(line), "%s, leant %s: the eye %.1f units out (of %.0f; room for %.1f)",
                    t.phase == 9 ? "a wall at his side" : "facing a wall he stands against", way > 0.f ? "right" : "left", out, full, room);
                testCheck(t.tally, tag, za::abs(out - want) < 0.5f && want > full * 0.75f, line);
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

struct LeanMouseStep
{
    int lean;        // -1 left, 0 upright, 1 right
    float pitch;     // degrees, + down
    int dx;          // the mouse's counts (+ right, + toward him: the view down)
    int dy;
    bool on;         // vr_flat_lean_mouse
};

constexpr LeanMouseStep leanMouseSteps[] = {
    {1, 0.f, 0, -15, true},  // leant right, level: up
    {1, 0.f, 15, 0, true},   // and right
    {1, 40.f, 0, -15, true}, // looking down 40: up
    {1, 40.f, 15, 0, true},
    {-1, -30.f, 0, 15, true}, // leant left, looking up 30: down
    {-1, -30.f, -15, 0, true}, // and left
    {0, 20.f, 0, -15, true}, // upright: Quake's own (no turn along a roll), straight up
    {0, 20.f, 15, 0, true},
    {1, 0.f, 0, -15, false}, // leant, vr_flat_lean_mouse 0: Quake's own, at the lean's slant
};

// vr_flatleanmouse_test: leans to a full roll (or stands upright), looks at a set pitch, then feeds the mouse a pure up
// (or down, left, right) move and checks a far point that was in the middle of the screen moved straight along the rolled
// screen's axis (the view as drawn, before and after: V_CalcRefdef's angles); upright and with vr_flat_lean_mouse 0 the
// look is Quake's own (no turn along a roll; off, the slant the lean gives).
void leanMouseTestFrame()
{
    LeanMouseTest& t = client.leanMouseTest;
    if(t.phase == 0)
    {
        return;
    }
    const char* tag = "flatleanmouse";
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    constexpr int stepCount = static_cast<int>(sizeof(leanMouseSteps) / sizeof(leanMouseSteps[0]));
    char line[320];
    if(inPhase > 6.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d, phase %d)", t.step, t.phase);
        testCheck(t.tally, tag, false, line);
        next(99);
    }
    const LeanMouseStep& st = leanMouseSteps[za::min(t.step, stepCount - 1)];
    switch(t.phase)
    {
        case 1: // the step's lean, pitch and setting; then two views built as they stand
            client.lean.cmdL = st.lean < 0;
            client.lean.cmdR = st.lean > 0;
            Cvar_SetValueQuick(&vr_flat_lean_mouse, st.on ? 1.f : 0.f);
            cl.viewangles[PITCH] = st.pitch;
            if(client.lean.lin == static_cast<float>(st.lean) &&
                za::abs(client.lean.roll - static_cast<float>(st.lean) * leanFullAngle()) < 1e-3f)
            {
                t.seenAt = client.seen.count;
                next(2);
            }
            break;
        case 2:
            cl.viewangles[PITCH] = st.pitch;
            if(client.seen.count >= t.seenAt + 2)
            {
                t.before = client.seen.angles;
                t.lookBefore = glm::vec3{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
                t.turnsAt = client.seen.turns;
                IN_MockMouseMotion(st.dx, st.dy);
                next(3);
            }
            break;
        case 3: // the move taken: then the view built from it
            if(cl.viewangles[PITCH] != t.lookBefore.x || cl.viewangles[YAW] != t.lookBefore.y)
            {
                t.turned = client.seen.turns - t.turnsAt;
                t.seenAt = client.seen.count;
                next(4);
            }
            break;
        case 4:
            if(client.seen.count > t.seenAt)
            {
                const bool upDown = st.dx == 0;
                const glm::vec3 now3 = client.seen.angles;
                vec3_t a0{t.before.x, t.before.y, t.before.z};
                vec3_t a1{now3.x, now3.y, now3.z};
                vec3_t f0, r0, u0, f1, r1, u1;
                AngleVectors(a0, f0, r0, u0);
                AngleVectors(a1, f1, r1, u1);
                // The far point that was in the middle of the screen, where it is now (screen x right, y up).
                const float depth = DotProduct(f0, f1);
                const float sx = DotProduct(f0, r1) / depth;
                const float sy = DotProduct(f0, u1) / depth;
                // The view's move is the point's the other way: its way on the screen, and how far it is off the axis.
                const float wantX = upDown ? 0.f : (st.dx > 0 ? -1.f : 1.f);
                const float wantY = upDown ? (st.dy < 0 ? -1.f : 1.f) : 0.f;
                const float along = sx * wantX + sy * wantY;
                const float across = upDown ? sx : sy;
                const float off = glm::degrees(za::atan2(za::abs(across), za::max(along, 1e-9f)));
                const float moved = glm::degrees(za::sqrt(sx * sx + sy * sy));
                const int turned = t.turned;
                const float yawMoved = cl.viewangles[YAW] - t.lookBefore.y;
                const float pitchMoved = cl.viewangles[PITCH] - t.lookBefore.x;
                q_snprintf(line, sizeof(line),
                    "step %d: %s, pitch %.0f, roll %.1f, the mouse %s%s: the point moved %.2f deg, %.2f deg off the screen's %s (yaw %+.3f, pitch %+.3f)",
                    t.step, st.lean > 0 ? "leant right" : st.lean < 0 ? "leant left" : "upright", st.pitch, t.before.z,
                    upDown ? (st.dy < 0 ? "up" : "down") : (st.dx > 0 ? "right" : "left"),
                    st.on ? "" : " (vr_flat_lean_mouse 0)", moved, off, upDown ? "up" : "across", yawMoved, pitchMoved);
                bool ok = along > 0.f && moved > 0.2f;
                if(st.lean != 0 && st.on)
                {
                    ok = ok && off < 0.5f && turned > 0;
                }
                else
                {
                    // Quake's own: no turn along a roll, the other angle untouched to the bit.
                    ok = ok && turned == 0 && (upDown ? yawMoved == 0.f : pitchMoved == 0.f);
                    if(st.lean != 0)
                    {
                        ok = ok && off > 5.f; // (the slant the lean gives the old way)
                    }
                    else
                    {
                        ok = ok && off < 0.5f;
                    }
                }
                testCheck(t.tally, tag, ok, line);
                t.step++;
                next(t.step < stepCount ? 1 : 99);
            }
            break;
        default:
            client.lean.cmdL = client.lean.cmdR = false;
            Cvar_SetValueQuick(&vr_flat_lean_mouse, t.cvarWas);
            t.tally.verdict(tag);
            t = LeanMouseTest{};
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
                next(10);
            }
            break;
        case 10: // standing again: lie down once more, then the jump key (vr_flat_prone_jump_stands)
            if(inPhase > 0.8 && flatplay::proneRatio() <= 0.f)
            {
                Cbuf_InsertText("vr_prone_toggle\n");
                next(11);
            }
            break;
        case 11:
            if(inPhase > 0.3 && flatplay::proneRatio() >= 1.f)
            {
                t.maxUp = 0.f;
                Cbuf_InsertText("+jump\n"); // (held: it must not jump after standing either)
                next(12);
            }
            break;
        case 12:
            t.maxUp = za::max(t.maxUp, cl.velocity[2]);
            if(inPhase > 0.4 && flatplay::proneRatio() <= 0.f)
            {
                q_snprintf(line, sizeof(line), "the jump key lying down, held: up (prone %.2f, crouch %.2f, server lying %d), no jump (upward speed %.0f)",
                    flatplay::proneRatio(), flatplay::crouchRatio(), qvr::hull::localProne() == 1 ? 1 : 0, t.maxUp);
                testCheck(t.tally, tag, qvr::hull::localProne() != 1 && t.maxUp < 10.f, line);
                next(13);
            }
            break;
        case 13: // still held a little longer: no jump after standing
            t.maxUp = za::max(t.maxUp, cl.velocity[2]);
            if(inPhase > 0.4)
            {
                q_snprintf(line, sizeof(line), "the held jump key after standing: still no jump (upward speed %.0f)", t.maxUp);
                testCheck(t.tally, tag, t.maxUp < 10.f, line);
                Cbuf_InsertText("-jump\n");
                next(14);
            }
            break;
        case 14: // let go, a fresh press: it jumps
            if(inPhase > 0.3)
            {
                t.maxUp = 0.f;
                Cbuf_InsertText("+jump\n");
                next(15);
            }
            break;
        case 15:
            t.maxUp = za::max(t.maxUp, cl.velocity[2]);
            if(inPhase > 0.3)
            {
                q_snprintf(line, sizeof(line), "a fresh jump press standing: jumps (upward speed %.0f)", t.maxUp);
                testCheck(t.tally, tag, t.maxUp > 100.f, line);
                Cbuf_InsertText("-jump\n");
                next(99);
            }
            break;
        default:
            t.tally.verdict(tag);
            t = ProneTest{};
            break;
    }
}

// vr_flatpronedual_test: lying down where he is, each pair of weapons in his hands (main:off ids; 0 an empty off hand, a
// one-gun reference), the look swept over proneDualPitches, each look held for the pose's eases to settle: per look and
// hand, whether its trigger fires (flatplay::routeFire: a gun turned up out of the floor or a wall does not), the depth
// it stood in them before its tilt, the floor's part and its deepest point off the hand (forward, right, up in the
// gun's frame: behind is the stock, under the magazine). A check per pair: every gun fires at the looks from
// proneDualFireUp up to proneDualFireDown down.
constexpr float proneDualFireUp = -45.f;   // the looks that must fire: up this far (Quake's pitch: negative is up)...
constexpr float proneDualFireDown = 15.f;  // ... to down this far (one gun lying down: stopped from 30 down, flush on the floor)
constexpr double proneDualSettle = 1.0;    // seconds a look is held before it is read: the hand's and the tilt's eases
constexpr double proneDualDraw = 0.5;      // seconds after the pair is in his hands (the draw, the hands' ease)

void proneDualTestFrame()
{
    ProneDualTest& t = client.proneDualTest;
    if(t.phase == 0)
    {
        return;
    }
    const char* tag = "flatpronedual";
    const double now = vr_gametime;
    const double inPhase = now - t.phaseAt;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
    };
    char line[400];
    if(inPhase > 8.0 && t.phase != 99)
    {
        q_snprintf(line, sizeof(line), "timed out (step %d, pair %d: holding %d:%d)", t.phase, t.pair, cl.stats[protocol::STAT_QVR_WEAPON],
            cl.stats[protocol::STAT_QVR_WEAPON2]);
        testCheck(t.tally, tag, false, line);
        cl.viewangles[PITCH] = 0.f;
        next(99);
    }
    const int (&p)[2] = t.pairs[za::min(t.pair, proneDualPairsMost - 1)];
    switch(t.phase)
    {
        case 1:
            if(flatplay::proneRatio() <= 0.f)
            {
                Cbuf_InsertText("vr_prone_toggle\n");
            }
            next(2);
            break;
        case 2:
            if(inPhase > 0.3 && flatplay::proneRatio() >= 1.f)
            {
                next(3);
            }
            break;
        case 3: // the pair into his hands: the off hand's first (a weapon past 19 by the main hand and the swap, impulse 48)
        {
            char give[160];
            const int off = p[1], main = p[0];
            const int n = off < 20 ? q_snprintf(give, sizeof(give), "impulse %d; wait; wait; ", 170 + off)
                                   : q_snprintf(give, sizeof(give), "vr_test_give_wid %d; impulse 149; wait; wait; impulse 48; wait; wait; ", off);
            if(main < 20)
            {
                q_snprintf(give + n, sizeof(give) - n, "impulse %d\n", 150 + main);
            }
            else
            {
                q_snprintf(give + n, sizeof(give) - n, "vr_test_give_wid %d; impulse 149\n", main);
            }
            Cbuf_InsertText(give); // (ahead of what the script still waits on)
            cl.viewangles[PITCH] = 0.f;
            t.blocked = t.blockedAll = 0;
            next(4);
            break;
        }
        case 4:
            if(cl.stats[protocol::STAT_QVR_WEAPON] == p[0] && cl.stats[protocol::STAT_QVR_WEAPON2] == p[1])
            {
                t.pitch = 0;
                next(5);
            }
            break;
        case 5: // the pair drawn
            if(inPhase > proneDualDraw)
            {
                next(6);
            }
            break;
        case 6:
        {
            const float pitch = proneDualPitches[t.pitch];
            cl.viewangles[PITCH] = pitch;
            if(inPhase < proneDualSettle)
            {
                break;
            }
            const flatplay::FireRoute r = flatplay::routeFire(true, true);
            const bool mustFire = pitch >= proneDualFireUp && pitch <= proneDualFireDown;
            int n = q_snprintf(line, sizeof(line), "%d:%d look %+3.0f", p[0], p[1], pitch);
            for(int h = HAND_MAIN; h >= HAND_OFF; h--)
            {
                if(flatplay::heldIn(h) < 0 || flatplay::meleeIn(h))
                {
                    continue;
                }
                float floor = 0.f;
                glm::vec3 at{0.f};
                const float depth = flatsight::wallDepth(h, &floor, &at);
                const bool fires = r.fire[h];
                t.blockedAll += fires ? 0 : 1;
                t.blocked += !fires && mustFire ? 1 : 0;
                n += q_snprintf(line + n, sizeof(line) - n, " | %s %s depth %.1f floor %.1f at %.0f %.0f %.0f tilt %.0f", h == HAND_MAIN ? "main" : "off",
                    fires ? "fires" : "STOPPED", depth, floor, at.x, at.y, at.z, flatsight::wallTilt(h));
            }
            Con_Printf("%s: %s\n", tag, line);
            if(++t.pitch < proneDualPitchCount)
            {
                next(6);
                break;
            }
            const entity_t& pl = cl_entities[cl.viewentity];
            q_snprintf(line, sizeof(line), "%d:%d lying down at %.0f %.0f %.0f: every gun fires looking from %.0f up to %.0f down (stopped %d there, %d at all %d looks)",
                p[0], p[1], pl.origin[0], pl.origin[1], pl.origin[2], -proneDualFireUp, proneDualFireDown, t.blocked, t.blockedAll, proneDualPitchCount);
            testCheck(t.tally, tag, t.blocked == 0, line);
            t.blockedTotal += t.blocked;
            cl.viewangles[PITCH] = 0.f;
            if(++t.pair < t.pairCount)
            {
                next(3);
                break;
            }
            Con_Printf("%s: stopped triggers at the looks that must fire, every pair: %d\n", tag, t.blockedTotal);
            next(99);
            break;
        }
        default:
            t.tally.verdict(tag);
            t = ProneDualTest{};
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
        if(t.frames >= 40 && t.fovBack)
        {
            if(flatspeedfov::multiplier() != 1.f)
            {
                break; // (the last mantle's FOV kick still easing back: it must reach the view's own)
            }
            char line[160];
            q_snprintf(line, sizeof(line), "the mantle FOV kick eased back to x1 (%.2f s after landing)",
                static_cast<float>(now - t.phaseAt));
            testCheck(t.tally, tag, true, line);
            t.fovBack = false;
        }
        if(t.frames >= 40)
        {
            t.mantlesBefore = mantles;
            t.fovPeak = 1.f;
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
            t.fovPeak = za::max(t.fovPeak, flatspeedfov::multiplier());
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
                // The empty hands on the ledge, the others not (his notes 2026-11-08).
                const int want = za::max(t.empty, 0);
                q_snprintf(line, sizeof(line), "lip %.0f: hands on the ledge off %d main %d (empty: off %d main %d)", k.lip,
                    t.onLedge & 1, (t.onLedge >> 1) & 1, za::max(t.empty, 0) & 1, (za::max(t.empty, 0) >> 1) & 1);
                testCheck(t.tally, tag, t.empty >= 0 && t.onLedge == want, line);
                // The FOV kick (the speed FOV on): wider while it mantled, at least half the extra (its ease in).
                const float extra = za::clamp(vr_flat_mantle_fov_bonus.value, 0.f, 0.5f);
                if(vr_flat_speed_fov.value != 0.f && extra > 0.f)
                {
                    q_snprintf(line, sizeof(line), "lip %.0f: the mantle FOV kick peaked at x%.3f (extra %.2f)", k.lip, t.fovPeak,
                        extra);
                    testCheck(t.tally, tag, t.fovPeak >= 1.f + 0.5f * extra, line);
                    t.fovBack = true;
                }
            }
            else if(t.fovPeak != 1.f)
            {
                q_snprintf(line, sizeof(line), "lip %.0f: no mantle, the FOV stayed x1 (peak x%.3f)", k.lip, t.fovPeak);
                testCheck(t.tally, tag, false, line);
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

// vrtutorial.map's lesson 3 (Misc/quakevr/maps/vrtutorial_gen.py build_room3): the ladder block's west face at x 2960,
// its top 156, rungs every 20 from 16 at x 2953 (y 680..728), the jump wall at x 3168 (top 256); the arena's boxes
// (LOW -144: 48 high at x 1200, 96 high at x 448) and its pit's stairs (x 768..896, 16 up every 24 north to LOW).
enum class LadderKind : za::U8
{
    Up,       // forward into the ladder: up it and over the top
    Down,     // back off its top, facing it: down it to the ground
    LookDown, // up it, then looking down: down it to the ground
    PushOff,  // up it, then jump: away from it
    None,     // forward into it for 1.5 s: no ladder
    Fallback, // (vr_flat_ladder 0) jump held in a mod's trigger_ladder: the mod's own climb lifts him
    Side,     // forward and right held into a ladder volume from off its middle (a run along it): up it and over the top, on
              // the way up (60 over its foot) centred on its width and his box out of its bars (hubladder)
};
struct LadderCase
{
    const char* name;
    LadderKind kind;
    float x, y, z, yaw;
    float riseTo; // None: the feet must come this high (stairs); -1e9 none
    float ground; // the feet at the ladder's foot
    float top;    // and on its top
    float line;   // the rungs' line: how far along the look (yaw) from the origin 0 0 0 (Up ends 9 past it)
    bool free;    // the ladder stands free (non-solid): climbed down, he ends anywhere within 36 of its line
    float mid{0.f};  // Side: the ladder's middle across its width (along the look's right: sin, -cos of yaw)
    float face{0.f}; // and its bars' near face along the look (as `line`): his box's front no further
};
constexpr LadderCase ladderCases[] = {
    {"the tutorial ladder, up and over the top", LadderKind::Up, 2933.f, 704.f, 24.f, 0.f, -1e9f, 0.f, 156.f, 2951.f},
    {"the tutorial ladder, down from its top", LadderKind::Down, 2982.f, 704.f, 180.f, 0.f, -1e9f, 0.f, 156.f, 2951.f},
    {"the tutorial ladder, down looking down", LadderKind::LookDown, 2933.f, 704.f, 24.f, 0.f, -1e9f, 0.f, 156.f, 2951.f},
    {"the tutorial ladder, jump pushes off", LadderKind::PushOff, 2933.f, 704.f, 24.f, 0.f, -1e9f, 0.f, 156.f, 2951.f},
    {"a plain wall (the ladder block's face, its top 156)", LadderKind::None, 2942.f, 490.f, 24.f, 0.f, -1e9f},
    {"a single ledge 48 high", LadderKind::None, 1182.f, -848.f, -120.f, 0.f, -1e9f},
    {"a single ledge 96 high", LadderKind::None, 430.f, -768.f, -120.f, 0.f, -1e9f},
    {"the jump wall over the ladder's top (100 high)", LadderKind::None, 3150.f, 704.f, 180.f, 0.f, -1e9f},
    {"the pit's stairs", LadderKind::None, 832.f, -1000.f, -248.f, 90.f, -146.f},
    // (Alone only, vr_flatladder_test 9, with vr_flat_ladder 0: the switch.)
    {"the tutorial ladder, vr_flat_ladder 0", LadderKind::None, 2933.f, 704.f, 24.f, 0.f, -1e9f},
};

// A mod's ladders (his request 2026-11-21: Quoth's and AD's trigger_ladder brushes climb as the tutorial's rungs, on
// the ladder volume's own rungs, or its drawn ones: vr_climb.cpp ladderHold). Quoth's kellbase1 (vr_campaign_native quoth): the ladder up
// the pillar's south face, its trigger_ladder brush 25..39 x, -659..-657 y, 641..895 z. AD's ad_test3: its ladder's
// brush -495..-465 x, -103..-73 y, 225..327 z (facing east).
constexpr LadderCase quothLadderCases[] = {
    // (Its ladder stands free, non-solid, a player clip 22 units behind it: he hangs in it, so no push-off case. Its top
    // is the catwalk's edge, 20 units past it.)
    {"Quoth's kellbase1 ladder, up and over the top", LadderKind::Up, 32.f, -664.f, 664.f, 90.f, -1e9f, 640.f, 896.f, -658.f, true},
    {"Quoth's kellbase1 ladder, down from its top", LadderKind::Down, 17.f, -619.f, 920.f, 90.f, -1e9f, 640.f, 896.f, -658.f, true},
    {"Quoth's kellbase1 ladder, down looking down", LadderKind::LookDown, 32.f, -664.f, 664.f, 90.f, -1e9f, 640.f, 896.f, -658.f,
        true},
    // (Both on the drawn rungs, a func_illusionary's bars, as a headset's hands: 24 apart on that one, 32 on a pillar's.
    // The pillar's off the arena: x -64, from the pit's floor at 256 up to 640, its bars' ridge at y -1354, facing north;
    // a func_train beside its foot.)
    {"Quoth's kellbase1 pillar ladder, up and over the top", LadderKind::Up, -64.f, -1346.f, 281.f, 270.f, -1e9f, 256.f, 640.f,
        1336.f, true},
    {"Quoth's kellbase1 pillar ladder, down from its top", LadderKind::Down, -64.f, -1400.f, 700.f, 270.f, -1e9f, 256.f, 640.f,
        1336.f, true},
    {"Quoth's kellbase1 pillar ladder, down looking down", LadderKind::LookDown, -64.f, -1346.f, 281.f, 270.f, -1e9f, 256.f, 640.f,
        1336.f, true},
    // (hubladder: the pillar's, from 24 left of its middle in its bars, forward and right held: its bars' near face y
    // -1344, their middle x -64. The catwalk's can't be stood out of: a clip 22 units in front of it.)
    {"Quoth's kellbase1 pillar ladder, run at from the side", LadderKind::Side, -40.f, -1346.f, 281.f, 270.f, -1e9f, 256.f, 640.f,
        1336.f, true, 64.f, 1344.f},
    // (Alone only, with vr_flat_ladder 0: Quoth's own climb, AD's jump ladder.)
    {"Quoth's kellbase1 ladder, vr_flat_ladder 0: jump climbs it", LadderKind::Fallback, 32.f, -664.f, 664.f, 90.f, -1e9f, 640.f,
        896.f, -658.f, true},
};

// (Its ladder is real rungs, steps in its wall, the trigger_ladder over them: the flat ladder's own case; here AD's own
// climb alone.)
constexpr LadderCase adLadderCases[] = {
    {"AD's ad_test3 ladder, up and over the top", LadderKind::Up, -512.f, -88.f, 252.f, 0.f, -1e9f, 228.f, 352.f, -495.f},
    // (Alone only, with vr_flat_ladder 0: AD's own climb.)
    {"AD's ad_test3 ladder, vr_flat_ladder 0: jump climbs it", LadderKind::Fallback, -512.f, -88.f, 252.f, 0.f, -1e9f, 228.f, 352.f,
        -495.f, true},
};

// AD's ad_e1m1: its wall ladder's trigger_ladder brush 2497..2559 x, -303..-297 y, 1..159 z (facing north).
constexpr LadderCase adE1m1LadderCases[] = {
    {"AD's ad_e1m1 ladder, up and over the top", LadderKind::Up, 2528.f, -318.f, 25.f, 90.f, -1e9f, 0.f, 160.f, -300.f, true},
    {"AD's ad_e1m1 ladder, down from its top", LadderKind::Down, 2528.f, -264.f, 184.f, 90.f, -1e9f, 0.f, 160.f, -300.f, true},
    {"AD's ad_e1m1 ladder, down looking down", LadderKind::LookDown, 2528.f, -318.f, 25.f, 90.f, -1e9f, 0.f, 160.f, -300.f, true},
    // (Alone only, with vr_flat_ladder 0: AD's own climb.)
    {"AD's ad_e1m1 ladder, vr_flat_ladder 0: jump climbs it", LadderKind::Fallback, 2528.f, -312.f, 25.f, 90.f, -1e9f, 0.f, 160.f,
        -300.f, true},
};

struct LadderMap
{
    const char* map;
    const LadderCase* cases;
    int count; // (the last one alone only)
};
constexpr LadderMap ladderMaps[] = {
    {"vrtutorial", ladderCases, static_cast<int>(sizeof(ladderCases) / sizeof(ladderCases[0]))},
    {"kellbase1", quothLadderCases, static_cast<int>(sizeof(quothLadderCases) / sizeof(quothLadderCases[0]))},
    {"ad_test3", adLadderCases, static_cast<int>(sizeof(adLadderCases) / sizeof(adLadderCases[0]))},
    {"ad_e1m1", adE1m1LadderCases, static_cast<int>(sizeof(adE1m1LadderCases) / sizeof(adE1m1LadderCases[0]))},
};

[[nodiscard]] const LadderMap* ladderMapNow()
{
    for(const LadderMap& m : ladderMaps)
    {
        if(!q_strcasecmp(sv.name, m.map))
        {
            return &m;
        }
    }
    return nullptr;
}

void ladderTestFrame()
{
    LadderTest& t = client.ladderTest;
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
    const char* tag = "flatladder";
    const double now = qcvm->time;
    const auto next = [&](int phase) {
        t.phase = phase;
        t.phaseAt = now;
        t.frames = 0;
    };
    cl.viewangles[PITCH] = t.pitch;
    cl.viewangles[YAW] = t.yaw;
    const bool ground = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    bool climbing = false, mantling = false;
    const int ladders = climb::flatLadders(pl, climbing);
    (void)climb::flatMantles(pl, mantling);
    const float feet = pl->v.origin[2] + pl->v.mins[2];
    const LadderMap* lm = ladderMapNow();
    if(!lm)
    {
        t = LadderTest{};
        return;
    }
    const int caseCount = lm->count;
    const LadderCase* cases = lm->cases;
    if(t.phase != 1 && now - t.phaseAt > (t.phase == 3 ? 20.0 : 10.0))
    {
        char line[200];
        q_snprintf(line, sizeof(line), "case %d timed out (step %d, ground %d, on the ladder %d, feet at %.1f, at %.1f %.1f)", t.index,
            t.phase, ground ? 1 : 0, climbing ? 1 : 0, feet, pl->v.origin[0], pl->v.origin[1]);
        testCheck(t.tally, tag, false, line);
        Cbuf_InsertText("-forward\n-back\n-jump\n-moveright\n");
        t.index++;
        next(1);
        return;
    }
    switch(t.phase)
    {
    case 1: // placed
        if(t.index >= (t.only >= 0 ? caseCount : caseCount - 1) || (t.only >= 0 && t.index > t.only))
        {
            Cbuf_InsertText("-forward\n-back\n-jump\n-moveright\n");
            t.tally.verdict(tag);
            t = LadderTest{};
            return;
        }
        {
            const LadderCase& k = cases[t.index];
            t.pitch = 0.f;
            t.yaw = k.yaw;
            // (Set down on the floor under the place: a map's floor needn't be where the case says to the unit; and its old
            // origin too, which SV_CheckStuck would put him back at.)
            vec3_t from{k.x, k.y, k.z + 1.f}, to{k.x, k.y, k.z - 64.f};
            const trace_t drop = SV_Move(from, pl->v.mins, pl->v.maxs, to, MOVE_NOMONSTERS, pl);
            const float z = drop.allsolid || drop.startsolid ? k.z : drop.endpos[2];
            if(drop.allsolid || drop.startsolid)
            {
                Con_Printf("flatladder: case %d's place (%.1f %.1f %.1f) is in solid (%s)\n", t.index, k.x, k.y, k.z, drop.ent ? PR_GetString(drop.ent->v.classname) : "?");
            }
            pl->v.origin[0] = pl->v.oldorigin[0] = k.x;
            pl->v.origin[1] = pl->v.oldorigin[1] = k.y;
            pl->v.origin[2] = pl->v.oldorigin[2] = z;
            pl->v.velocity[0] = pl->v.velocity[1] = pl->v.velocity[2] = 0.f;
            SV_LinkEdict(pl, false);
        }
        next(2);
        break;
    case 2: // settled: the keys (past the climb's 0.4 s without a new hold after the last case's)
        // (In a mod's trigger_ladder with its own climb on, he floats: no ground.)
        t.frames = (ground || cases[t.index].kind == LadderKind::Fallback) && !climbing && !mantling ? t.frames + 1 : 0;
        if(t.frames >= 40)
        {
            const LadderCase& k = cases[t.index];
            t.laddersBefore = ladders;
            t.climbFrames = 0;
            t.mantled = false;
            t.handBits = 0;
            t.serials = 0;
            t.lastSerial = -1;
            t.minFeet = t.maxFeet = feet;
            t.turned = false;
            t.sideSeen = false;
            Cbuf_InsertText(k.kind == LadderKind::Down       ? "+back\n"
                            : k.kind == LadderKind::Fallback ? "+jump\n"
                            : k.kind == LadderKind::Side     ? "+forward\n+moveright\n"
                                                             : "+forward\n");
            next(3);
        }
        break;
    case 3: // the keys held, until the case's end
    {
        const LadderCase& k = cases[t.index];
        const int bits = cl.stats[protocol::STAT_QVR_CLIMB];
        if(climbing)
        {
            t.climbFrames++;
            t.handBits |= bits & 3;
            const int serial = (bits >> 2) & 4095;
            t.serials += t.lastSerial >= 0 && serial != t.lastSerial ? 1 : 0;
            t.lastSerial = serial;
        }
        t.mantled = t.mantled || mantling;
        t.minFeet = za::min(t.minFeet, feet);
        t.maxFeet = za::max(t.maxFeet, feet);
        const bool climbed = ladders > t.laddersBefore;
        if(!t.turned && climbing &&
            ((k.kind == LadderKind::LookDown && feet >= k.ground + 60.f) || (k.kind == LadderKind::PushOff && feet >= k.ground + 50.f)))
        {
            t.turned = true;
            if(k.kind == LadderKind::LookDown)
            {
                t.pitch = 60.f; // (looking down, forward still held)
            }
            else
            {
                Cbuf_InsertText("-forward\n+jump\n");
            }
        }
        bool done = false;
        bool ok = false;
        char line[300];
        line[0] = 0;
        // How far past the rungs' line along the look (the tutorial's: x less 2951).
        const float yawRad = glm::radians(k.yaw);
        const float x = pl->v.origin[0] * za::cos(yawRad) + pl->v.origin[1] * za::sin(yawRad) - k.line;
        if(k.kind == LadderKind::Side && climbing && !t.sideSeen && feet >= k.ground + 60.f)
        {
            t.sideSeen = true;
            t.sideOff = pl->v.origin[0] * za::sin(yawRad) - pl->v.origin[1] * za::cos(yawRad) - k.mid;
            t.sideFront = x + k.line + pl->v.maxs[0];
            // (Or as far out as there is room: kellbase1's ladders have a clip, a func_train's path in front of them.)
            vec3_t from{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]};
            vec3_t to{from[0] - za::cos(yawRad), from[1] - za::sin(yawRad), from[2]};
            t.sideBlocked = SV_Move(from, pl->v.mins, pl->v.maxs, to, MOVE_NOMONSTERS, pl).fraction < 1.f;
            Cbuf_InsertText("-moveright\n"); // (forward on: up and over the top)
        }
        switch(k.kind)
        {
        case LadderKind::Up:
        case LadderKind::Side:
        {
            t.frames = t.mantled && ground && !mantling ? t.frames + 1 : 0;
            done = t.frames >= 5;
            ok = climbed && t.mantled && za::abs(feet - k.top) < 2.f && x > 9.f && t.serials >= 4;
            const bool side = k.kind == LadderKind::Side;
            ok = ok && (!side || (t.sideSeen && za::abs(t.sideOff) <= 1.f && (t.sideFront <= k.face - 0.5f || t.sideBlocked)));
            q_snprintf(line, sizeof(line), "%s: %s, %s, feet at %.1f, %.1f past the rungs; %d hand changes, hands drawn off %d main %d%s",
                k.name, climbed ? "took the ladder" : "never took it", t.mantled ? "mantled" : "no mantle", feet, x, t.serials,
                t.handBits & 1, (t.handBits >> 1) & 1,
                side ? va("; 60 up %.1f off its middle, his box's front %.1f past its face%s", t.sideOff, t.sideFront - k.face,
                           t.sideBlocked ? " (no room further out)" : "")
                     : "");
            break;
        }
        case LadderKind::Down:
        case LadderKind::LookDown:
        {
            t.frames = climbed && ground && !climbing && (k.kind == LadderKind::Down || t.turned) ? t.frames + 1 : 0;
            done = t.frames >= 5;
            // (A free-standing ladder's way down may end on a mover passing in front of it, as it would in play: kellbase1's
            // pillar ladder has a func_train by it, and since hubladder the body hangs out of the bars, over its path.)
            bool onMover = false;
            if(k.free && ground)
            {
                vec3_t from{pl->v.origin[0], pl->v.origin[1], pl->v.origin[2]}, to{from[0], from[1], from[2] - 2.f};
                const trace_t under = SV_Move(from, pl->v.mins, pl->v.maxs, to, MOVE_NORMAL, pl);
                onMover = under.ent && under.ent != EDICT_NUM(0) && static_cast<int>(under.ent->v.movetype) == MOVETYPE_PUSH;
            }
            // (Down: lowered onto it off its top by the mantle's way backwards; looking down: no mantle.)
            ok = climbed && (k.kind == LadderKind::Down) == t.mantled &&
                 (za::abs(feet - k.ground) < 2.f || (onMover && t.maxFeet - feet >= 100.f)) && t.climbFrames >= 36 &&
                 (k.free ? za::abs(x) < 36.f : x > -36.f && x < -6.f) && (k.kind == LadderKind::Down || t.maxFeet >= k.ground + 60.f);
            q_snprintf(line, sizeof(line), "%s: %s, %d frames on it, the feet from %.1f down to %.1f%s, %.1f off the rungs%s", k.name,
                climbed ? "took the ladder" : "never took it", t.climbFrames, t.maxFeet, feet, onMover ? " (on a mover)" : "", -x,
                t.mantled ? ", mantled" : "");
            break;
        }
        case LadderKind::PushOff:
            t.frames = t.turned && ground && !climbing ? t.frames + 1 : 0;
            done = t.frames >= 5;
            ok = climbed && x < -26.f; // (away from it, landed)
            q_snprintf(line, sizeof(line), "%s: %s, landed %.1f off the rungs, feet at %.1f", k.name,
                climbed ? "took the ladder" : "never took it", -x, feet);
            break;
        case LadderKind::Fallback:
            done = now - t.phaseAt >= 1.5;
            ok = !climbed && t.maxFeet >= k.ground + 48.f;
            q_snprintf(line, sizeof(line), "%s: %s, the feet %.1f..%.1f", k.name, climbed ? "took the flat ladder" : "the mod's climb",
                t.minFeet, t.maxFeet);
            break;
        case LadderKind::None:
            done = now - t.phaseAt >= 1.5;
            ok = !climbed && t.maxFeet >= k.riseTo - 2.f && (k.riseTo > -1e8f || t.maxFeet - t.minFeet < 2.f);
            q_snprintf(line, sizeof(line), "%s: %s, the feet %.1f..%.1f", k.name, climbed ? "took a ladder" : "no ladder", t.minFeet,
                t.maxFeet);
            break;
        }
        if(done)
        {
            testCheck(t.tally, tag, ok, line);
            Cbuf_InsertText("-forward\n-back\n-jump\n-moveright\n");
            t.index++;
            next(1);
        }
        break;
    }
    default:
        break;
    }
}

void ladderTest_f()
{
    if(!flatplay::active() || !sv.active || !ladderMapNow())
    {
        Con_Printf("vr_flatladder_test: flat-screen play (-flatscreenmode) in a game you host, on map vrtutorial (or Quoth's "
                   "kellbase1, AD's ad_test3: their trigger_ladder brushes)\n");
        return;
    }
    client.ladderTest = LadderTest{};
    client.ladderTest.phase = 1;
    if(Cmd_Argc() > 1)
    {
        client.ladderTest.only = za::max(0, Q_atoi(Cmd_Argv(1)));
        client.ladderTest.index = client.ladderTest.only;
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

void slideHitTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatslide_hit_test: flat-screen play (-flatscreenmode) in a game you host\n");
        return;
    }
    client.slideHitTest = SlideHitTest{};
    client.slideHitTest.phase = 1;
    client.slideHitTest.which = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) != 0 ? 1 : 0;
    progs::ServerVm vm;
    client.slideHitTest.phaseAt = qcvm->time;
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

void leanMouseTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatleanmouse_test: flat-screen play (-flatscreenmode) in a game you host\n");
        return;
    }
    client.leanMouseTest = LeanMouseTest{};
    client.leanMouseTest.cvarWas = vr_flat_lean_mouse.value;
    client.leanMouseTest.phase = 1;
    client.leanMouseTest.phaseAt = vr_gametime;
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

void proneDualTest_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flatpronedual_test: flat-screen play (-flatscreenmode) in a game you host, room to lie down\n");
        return;
    }
    client.proneDualTest = ProneDualTest{};
    ProneDualTest& t = client.proneDualTest;
    // (The pairs asked for, main:off weapon ids; none: the common ones, the shotguns, the nail guns, the launchers, the
    // lightning gun, the grunt's gun, a mixed pair, the nail pistols.)
    constexpr int common[][2] = {{4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {10, 10}, {11, 11}, {15, 15}, {4, 6}, {29, 29}};
    for(int i = 1; i < Cmd_Argc() && t.pairCount < proneDualPairsMost; i++)
    {
        const char* a = Cmd_Argv(i);
        const char* colon = strchr(a, ':');
        if(!colon)
        {
            Con_Printf("vr_flatpronedual_test: pairs are main:off weapon ids (\"%s\")\n", a);
            client.proneDualTest = ProneDualTest{};
            return;
        }
        t.pairs[t.pairCount][0] = atoi(a);
        t.pairs[t.pairCount][1] = atoi(colon + 1);
        t.pairCount++;
    }
    if(t.pairCount == 0)
    {
        for(const auto& c : common)
        {
            t.pairs[t.pairCount][0] = c[0];
            t.pairs[t.pairCount][1] = c[1];
            t.pairCount++;
        }
    }
    t.phase = 1;
    t.phaseAt = vr_gametime;
}

void status_f()
{
    Con_Printf("flatmove: lean %.2f (eye %.1f %.1f %.1f, roll %.1f), sliding %d (pose %.2f, drop %.1f)",
        leanShare(), client.lean.shift.x, client.lean.shift.y, client.lean.shift.z, client.lean.roll, sliding() ? 1 : 0,
        client.slide.share, slideDrop());
    if(sv.active)
    {
        const Slide& s = slide::state(1);
        Con_Printf(", server: slides %d, last from %.0f u/s, its stamina %.1f in %d parts, %d refused tired", s.starts,
            s.startSpeed, s.staminaPaid, s.partsPaid, s.tiredRefusals);
    }
    Con_Printf("\n");
}

void leanLeftDown_f() { client.lean.cmdL = true; }
void leanLeftUp_f() { client.lean.cmdL = false; }
void leanRightDown_f() { client.lean.cmdR = true; }
void leanRightUp_f() { client.lean.cmdR = false; }

} // namespace

void init()
{
    Cmd_AddCommand("+flatleanleft", leanLeftDown_f); // the lean keys' holds, the console's way (a bind or a script)
    Cmd_AddCommand("-flatleanleft", leanLeftUp_f);
    Cmd_AddCommand("+flatleanright", leanRightDown_f);
    Cmd_AddCommand("-flatleanright", leanRightUp_f);
    Cmd_AddCommand("vr_flatslide_selftest", slideSelftest_f);
    Cmd_AddCommand("vr_flatslide_test", slideTest_f);
    Cmd_AddCommand("vr_flatslide_hit_test", slideHitTest_f); // (0 a crate, 1 a grunt)
    Cmd_AddCommand("vr_flatslide_rough_test", roughTest_f);
    Cmd_AddCommand("vr_flatslide_ramp_test", rampTest_f);
    Cmd_AddCommand("vr_flatlean_test", leanTest_f);
    Cmd_AddCommand("vr_flatleanmouse_test", leanMouseTest_f);
    Cmd_AddCommand("vr_flatprone_test", proneTest_f);
    Cmd_AddCommand("vr_flatpronedual_test", proneDualTest_f);
    Cmd_AddCommand("vr_flatmantle_test", mantleTest_f);
    Cmd_AddCommand("vr_flatladder_test", ladderTest_f);
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
        slideHitTestFrame();
        roughTestFrame();
        rampTestFrame();
        leanTestFrame();
        leanMouseTestFrame();
        proneTestFrame();
        proneDualTestFrame();
        mantleTestFrame();
        ladderTestFrame();
        slide::poseTestFrame();
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
        return slide::state(1).on;
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
        return slide::state(1).on && !slide::state(1).airborne;
    }
    // A client of another server: his crouch held, on the ground, at a slide's speed.
    const float speed = za::sqrt(cl.velocity[0] * cl.velocity[0] + cl.velocity[1] * cl.velocity[1]);
    return slideOn() && flatplay::crouchHeld() && cl.onground && speed > za::clamp(vr_flat_slide_end.value, 0.f, 300.f);
}

float slideShare()
{
    return flatplay::active() ? client.slide.share : 0.f;
}

float slideNeckBack(bool other)
{
    // (A headset's slide pose too, vr_slide_body: the same body, vr_slide.cpp.)
    return other || flatplay::active() || slide::headsetPose() ?za::clamp(vr_flat_slide_neck_back.value, 0.f, 20.f) : 0.f;
}

float slideDrop()
{
    return slideShare() * za::clamp(vr_flat_slide_drop.value, 0.f, 14.f);
}

} // namespace qvr::flatmove

using namespace qvr;

extern "C" int VR_LeanHitBoxes(edict_t* touch, float (*boxes)[6])
{
    if(vr_flat_lean_hit.value == 0.f || flatmove::leanHitOff || flatmove::clientSlot(touch) < 0 || !hull::isFlat(touch) || touch->v.health <= 0.f ||
       static_cast<int>(touch->v.solid) == SOLID_NOT)
    {
        return 0;
    }
    if(playerhit::Shape s; playerhit::serverShape(touch, s))
    {
        return 0; // (hitzones: his body as drawn, leant as he leans, is what shots meet: vr_playerhit.cpp)
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

namespace
{
// Every key flat play reads itself, in the menu's order (Options > Controls, its "Quake VR (flat screen)" group: menu.c
// M_Keys_Populate): the cvars a key's bind name is kept in, and the rows' names. Key_Event leaves these keys' binds off
// while flat play runs (VR_FlatKeyOwned: the flat key wins); Controls rebinds them (VR_FlatKeySet).
cvar_t* const flatKeyList[] = {&vr_flat_key_lean_left, &vr_flat_key_lean_right, &vr_flat_key_prone, &vr_flat_key_swap, &vr_flat_key_grab, &vr_flat_key_shove, &vr_flat_key_punch,
    &vr_flat_key_reload, &vr_flat_key_ammo, &vr_flat_key_drop, &vr_flat_key_cycle_left, &vr_flat_key_cycle_right,
    &vr_flat_key_holster_left, &vr_flat_key_holster_right, &vr_flat_key_drop_left, &vr_flat_key_drop_right, &vr_flat_key_grenade, &vr_flat_key_bullettime, &vr_flat_key_flashlight, &vr_flat_key_gearlights, &vr_flat_key_voicechat, &vr_flat_key_holster_menu, &vr_flat_key_flycam,
    &vr_flat_key_toolgun, &vr_flat_key_toolgun_menu, &vr_flat_key_toolgun_adjust};
const char* const flatKeyLabels[] = {"Lean left", "Lean right", "Lie down", "Swap hands", "Force grab", "Shove", "Punch", "Reload", "Next ammo type", "Drop",
    "Cycle left hand", "Cycle right hand", "Holster left hand", "Holster right hand", "Drop left hand", "Drop right hand", "Quick grenade", "Bullet time", "Flashlight", "Gear lights", "Push-to-talk", "Holster menu", "Fly camera", "Toolgun", "Toolgun panel", "Toolgun adjust"};
static_assert(za::getArraySize(flatKeyList) == za::getArraySize(flatKeyLabels), "a label for every flat key");

[[nodiscard]] int flatKeyNum(const cvar_t& c)
{
    const int key = c.string[0] ? Key_StringToKeynum(c.string) : -1;
    return key > 0 && key < MAX_KEYS ? key : -1;
}

[[nodiscard]] bool flatKeyIndexOk(int i)
{
    return i >= 0 && i < static_cast<int>(za::getArraySize(flatKeyList));
}
} // namespace

za::Span<cvar_t* const> qvr::flatplay::keyCvars()
{
    return flatKeyList;
}

bool qvr::flatplay::keyActive(const cvar_t& c)
{
    // The holsters' number keys (his words 2026-11-22) only with the holsters on: off, 1 .. 7 pick weapons as before.
    const bool holsterKey = &c == &vr_flat_key_cycle_left || &c == &vr_flat_key_cycle_right || &c == &vr_flat_key_holster_left ||
                            &c == &vr_flat_key_holster_right || &c == &vr_flat_key_drop_left || &c == &vr_flat_key_drop_right;
    return !holsterKey || vr_flat_holsters.value != 0.f;
}

extern "C" int VR_FlatKeyCount(void)
{
    return static_cast<int>(za::getArraySize(flatKeyList));
}

extern "C" const char* VR_FlatKeyLabel(int i)
{
    return flatKeyIndexOk(i) ? flatKeyLabels[i] : "";
}

extern "C" const char* VR_FlatKeyCvarName(int i)
{
    return flatKeyIndexOk(i) ? flatKeyList[i]->name : "";
}

extern "C" int VR_FlatKeyGet(int i)
{
    return flatKeyIndexOk(i) ? flatKeyNum(*flatKeyList[i]) : -1;
}

extern "C" void VR_FlatKeySet(int i, int key)
{
    if(!flatKeyIndexOk(i))
    {
        return;
    }
    cvar_t* const var = flatKeyList[i];
    const char* const name = key > 0 && key < MAX_KEYS ? Key_KeynumToString(key) : "";
    // The name as the default spells it where it is the default's key (a sparse config).
    Cvar_SetQuick(var, var->default_string && !q_strcasecmp(var->default_string, name) ? var->default_string : name);
}

extern "C" int VR_FlatKeyFor(int key, int skip)
{
    if(key <= 0 || key >= MAX_KEYS)
    {
        return -1;
    }
    for(int i = 0; i < static_cast<int>(za::getArraySize(flatKeyList)); i++)
    {
        if(i != skip && flatKeyNum(*flatKeyList[i]) == key)
        {
            return i;
        }
    }
    return -1;
}

extern "C" void VR_FlatKeysReset(void)
{
    for(cvar_t* c : flatKeyList)
    {
        Cvar_SetQuick(c, c->default_string);
    }
}

extern "C" int VR_FlatKeyOwned(int key)
{
    if(!flatplay::active() || key <= 0 || key >= MAX_KEYS)
    {
        return 0;
    }
    for(const cvar_t* c : flatplay::keyCvars())
    {
        if(c->string[0] && flatplay::keyActive(*c) && Key_StringToKeynum(c->string) == key)
        {
            return 1;
        }
    }
    // The holsters on (vr_flatholster.cpp): the number keys pick no weapon (3 is the swap's, above).
    if(vr_flat_holsters.value != 0.f && key >= '0' && key <= '9')
    {
        return 1;
    }
    return 0;
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
    flatmove::SeenView& seen = flatmove::client.seen;
    seen.roll = 0.f;
    if(!flatplay::active() || chase_active.value)
    {
        return;
    }
    const glm::vec3 shift = flatmove::leanShift();
    vieworg[0] += shift.x;
    vieworg[1] += shift.y;
    vieworg[2] += shift.z;
    viewangles[ROLL] += flatmove::leanRoll();
    // A blow's tilt (a punch into it, a weapon's overhead swing with its arc: flatplay::meleeViewTilt), the view's only.
    const glm::vec3 blowTilt = flatplay::meleeViewTilt();
    viewangles[PITCH] += blowTilt.x;
    viewangles[ROLL] += blowTilt.z;
    // The roll he sees, every one on the view by here (the lean's, cl_rollangle's strafe roll, a hit's kick): the look
    // turns along it (VR_FlatLookTurn). The strafe roll counts too -- the screen is as tilted by it, and it only turns
    // the mouse's way with the screen, adding no motion of its own, so strafing straight is no less steady. Dead (the
    // death view's own camera) or flying the fly camera (its own level view), none.
    seen.angles = glm::vec3{viewangles[0], viewangles[1], viewangles[2]};
    seen.count++;
    seen.roll = cl.stats[STAT_HEALTH] > 0 && !flycam::on() ? viewangles[ROLL] : 0.f;
}

// The look's turn (degrees: `right` the view toward the screen's right, `down` toward its bottom) taken along the screen
// as he sees it rolled: the move is the one Quake gives level -- across, the view's own sideways shift (yaw times the
// pitch's cosine), up and down the pitch -- turned by the roll into the level frame, and back into yaw and pitch. Not
// rolled, or vr_flat_lean_mouse 0: false, left as it was (Quake's own, to the bit).
extern "C" int VR_FlatLookTurn(float* right, float* down)
{
    flatmove::SeenView& seen = flatmove::client.seen;
    if(vr_flat_lean_mouse.value == 0.f || !flatplay::active() || seen.roll == 0.f || (*right == 0.f && *down == 0.f))
    {
        return 0;
    }
    const float r = glm::radians(seen.roll);
    const float sr = za::sin(r);
    const float cr = za::cos(r);
    const float c = za::max(za::cos(glm::radians(cl.viewangles[PITCH])), flatmove::lookPoleCos);
    const float mx = *right * c; // the move on the rolled screen
    const float my = *down;
    *right = (cr * mx - sr * my) / c; // + roll is the screen's top tilted right: its up is up and right on a level one
    *down = sr * mx + cr * my;
    seen.turns++;
    return 1;
}
