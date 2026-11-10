// vr_flatfgrab.cpp -- see vr_flatfgrab.hpp.
//
// Each hand's grab goes Idle -> Turn -> Pull -> Back -> Idle on the game clock (vr_gametime: the same steps whatever the
// frame rate). Turn: the share of the palm pose rises over vr_flat_fg_turn_time (and vr_flat_fg_detach_time more for a
// hand on a gun's grip), the fingers open with it; at its end the grab is asked for (impulse 50 / 51). Pull: the pose
// held, the palm turned to the object as the server reports it (STAT_QVR_FGMAIN / _FGOFF: flying, state 3) until it stops
// flying (caught or missed), the hand holds something, or the server never sent it flying (nothing to take: pullGrace).
// Back: the share falls over vr_flat_fg_release_time.
//
// The pose: the hand out from its shoulder (vr_flat_fg_fwd / _out / _down, shares of the arm's reach, in the frame the
// flat hands are placed in), its palm (grip::handFrame's measured normal: the empty jointed hand as drawn) turned to the
// object (vr_flat_fg_palm_aim of the way from the view's forward), the fingers up, turned vr_flat_fg_finger_tilt degrees
// out about the palm's normal.
//
// vr_flat_fg_report prints each hand's step; vr_flat_fg_press presses E (the tests); vr_flatplay_debug 1 prints each step.

#include "vr_flatfgrab.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_fgfx.hpp"
#include "vr_flashlight.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_geom.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_twohand.hpp"
#include "vr_walltorch.hpp"

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

namespace qvr::flatfgrab
{

namespace
{

enum class Phase : int
{
    Idle,
    Turn,
    Pull,
    Back,
};

constexpr double pullGrace = 0.3; // s: sent, and the server never had it fly (nothing where he looked): back
constexpr double pullMost = 5.0;  // s: the longest a pose is held for one pull (a flight is well under it)
constexpr float farAim = 256.f;   // units: the crosshair's point the palm turns to while no object is known

// Each hand's grab (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct Grab
{
    Phase phase{Phase::Idle};
    double at{-1e9};       // when the step began (vr_gametime)
    float turnTime{0.f};   // this grab's turn, the detach in it
    float share{0.f};      // the pose's share this frame, 0..1 (eased)
    float backFrom{0.f};   // the share the way back started from
    bool sawFlight{false}; // the server had its object flying
    bool detached{false};  // it began on a gun's grip
    float palmOff{0.f};    // the last pose's palm off the way to its object, degrees (vr_flat_fg_report)
};

struct GrabState
{
    Grab grab[2];
    float carry[2]{0.f, 0.f}; // the carry pose's share, before its ease
    float upright[2]{0.f, 0.f}; // a held torch's turn flame up, its share before its ease (vr_flat_torch_upright)
    float torchOff[2]{-1.f, -1.f}; // that torch's stick off upright as drawn, degrees, after the turn (-1: no torch; vr_flat_fg_report)
    double lastTime{-1e9};
    int presses{0}, sent{0}, refused{0}, doubled{0};
};

GrabState gs;

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] const char* phaseName(Phase p)
{
    switch(p)
    {
    case Phase::Turn: return "turning";
    case Phase::Pull: return "pulling";
    case Phase::Back: return "back";
    default: return "idle";
    }
}

[[nodiscard]] bool flying(int h)
{
    int ent = 0, state = 0;
    glm::vec3 at{0.f};
    return fgfx::handTarget(h, ent, state, at) && state == 3;
}

[[nodiscard]] bool torchIn(int h); // (below)

// Whether hand `h` can take E's grab now: empty (as the game's VRIsHandEmpty: no weapon, nothing carried), not holding the
// torch, not on a grab of its own, and nothing still flying to it.
[[nodiscard]] bool canTake(int h)
{
    const Phase p = gs.grab[h].phase;
    return held::handEmpty(h) && !flashlight::holds(h) && (p == Phase::Idle || p == Phase::Back) && !flying(h);
}

// Whether `h` is on, or going to, a held gun's two-handed grip (the grip itself, the ironsight's or the stance's).
[[nodiscard]] bool onGrip(int h)
{
    return twohand::drawnHelping(h) || flatsight::adsHelper(h) || flatsight::lookShare(h) > 0.01f;
}

void send(int h)
{
    Grab& g = gs.grab[h];
    g.phase = Phase::Pull;
    g.at = vr_gametime;
    g.sawFlight = false;
    gs.sent++;
    Cbuf_InsertText(h == HAND_OFF ? "impulse 50\n" : "impulse 51\n"); // QC weapons.qc: VR_Forcegrab_FlatGrab with that hand
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatfgrab: %s hand: palm out, grab sent\n", handName(h));
    }
}

void startBack(int h, const char* why)
{
    Grab& g = gs.grab[h];
    g.phase = Phase::Back;
    g.at = vr_gametime;
    g.backFrom = g.share;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatfgrab: %s hand: back to its pose (%s)\n", handName(h), why);
    }
}

void report_f()
{
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const Grab& g = gs.grab[h];
        int ent = 0, state = 0;
        glm::vec3 at{0.f};
        const bool known = fgfx::handTarget(h, ent, state, at);
        Con_Printf("flatfgrab: %s hand: %s, share %.2f, open %.2f, carry %.2f, palm off %.1f deg, %s, server %d state %d, "
                   "empty %d\n",
            handName(h), phaseName(g.phase), g.share, openShare(h), carryShare(h), g.palmOff, g.detached ? "from the grip" : "free",
            known ? ent : 0, known ? state : 0, held::handEmpty(h) ? 1 : 0);
        // (Its own short line, the console's width: the tests read it whole.)
        Con_Printf("flatfgrab: %s prop: in fist %d, fist %.2f, torch %d, upright %.2f, off %.1f deg\n", handName(h),
            flatplay::propInFist(h) ? 1 : 0, flatplay::fistShare(h), torchIn(h) ? 1 : 0, geom::smooth01(gs.upright[h]),
            gs.torchOff[h]);
    }
    Con_Printf("flatfgrab: presses %d, sent %d, refused %d (no free hand), second presses on a busy hand %d\n", gs.presses,
        gs.sent, gs.refused, gs.doubled);
    // Where the effect starts on each hand (fgfx::palmOrigin), in the hand's frame (forward, left, up), against the palm's
    // middle and the old start (2 units ahead of the hand's point): headset or flat.
    const hands::State& s = hands::current();
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        glm::vec3 f, r, u;
        hands::angleVectors(s.rot[h], f, r, u);
        const auto local = [&](const glm::vec3& p) {
            const glm::vec3 d = p - s.pos[h];
            return glm::vec3{glm::dot(d, f), -glm::dot(d, r), glm::dot(d, u)};
        };
        const glm::vec3 palm = local(hands::palmPoint(s, h));
        const glm::vec3 fx = local(fgfx::palmOrigin(s, h));
        const grip::HandFrame hf = grip::handFrame(h, h == HAND_OFF, true);
        Con_Printf("flatfgrab: %s hand fx start (fwd left up) %.2f %.2f %.2f; palm middle %.2f %.2f %.2f (known %d, measured %d, "
                   "normal %.2f %.2f %.2f); old start 2 0 0\n",
            handName(h), fx.x, fx.y, fx.z, palm.x, palm.y, palm.z, s.palmValid[h] ? 1 : 0, hf.measured ? 1 : 0, hf.palmNormal.x,
            hf.palmNormal.y, hf.palmNormal.z);
    }
}

void press_f()
{
    press();
}

// The pose's turn: hand axes (world: forward, left, up as columns) with its palm's normal along `palm` and its fingers along
// `fingers` (both unit, at right angles), for hand `h` as its jointed model is drawn (grip::handFrame: its palm's way in
// the hand's frame).
[[nodiscard]] glm::mat3 palmAxes(int h, const glm::vec3& palm, const glm::vec3& fingers)
{
    const grip::HandFrame f = grip::handFrame(h, h == HAND_OFF, true);
    glm::vec3 p = glm::length(f.palmNormal) > 1e-3f ? glm::normalize(f.palmNormal) : glm::vec3{0.f, h == HAND_OFF ? -1.f : 1.f, 0.f};
    glm::vec3 x = glm::vec3{1.f, 0.f, 0.f} - p * p.x; // the fingers' way in the hand: its forward, off the palm's normal
    if(glm::length(x) < 1e-3f)
    {
        x = glm::vec3{0.f, 0.f, 1.f} - p * p.z;
    }
    x = glm::normalize(x);
    const glm::mat3 local{p, x, glm::cross(p, x)};
    const glm::mat3 world{palm, fingers, glm::cross(palm, fingers)};
    return world * glm::transpose(local);
}

// `s.pos[h]` / `s.rot[h]` moved `share` of the way to `pos` and the axes `axes`.
void blendTo(hands::State& s, int h, const glm::vec3& pos, const glm::mat3& axes, float share)
{
    glm::vec3 f, r, u;
    hands::angleVectors(s.rot[h], f, r, u);
    const glm::quat from = glm::quat_cast(glm::mat3{f, -r, u});
    const glm::quat to = glm::quat_cast(axes);
    const glm::mat3 m = glm::mat3_cast(glm::slerp(from, to, share));
    s.pos[h] += (pos - s.pos[h]) * share;
    s.rot[h] = hands::anglesFromVectors(m[0], m[2]);
}

// The frame the flat hands are placed in (the view's yaw and roll, its pitch held to vr_flat_pose_pitch: poseHands').
void placeFrame(const glm::vec3& aim, glm::vec3& fwd, glm::vec3& right, glm::vec3& up)
{
    const float posePitch = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
    hands::angleVectors(glm::vec3{za::clamp(aim.x, -posePitch, posePitch), aim.y, aim.z}, fwd, right, up);
}

void grabPose(hands::State& s, int h, const glm::vec3& aim)
{
    Grab& g = gs.grab[h];
    const float k = geom::smooth01(g.share);
    if(k <= 0.f)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    placeFrame(aim, fwd, right, up);
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const float span = flatplay::armReach();
    const glm::vec3 shoulder = flatplay::shoulderOf(s, h) + flatplay::eyeNudge(aim);
    const glm::vec3 pos = shoulder + (fwd * flatplay::poseNum(vr_flat_fg_fwd) + right * (side * flatplay::poseNum(vr_flat_fg_out)) -
                                         up * flatplay::poseNum(vr_flat_fg_down)) *
                                         span;

    // What it turns to: its object as the server has it (aimed at, or flying to it), else the crosshair's point far out.
    glm::vec3 org, dir;
    flatplay::crosshairRay(s, aim, org, dir);
    glm::vec3 at = org + dir * farAim;
    int ent = 0, state = 0;
    glm::vec3 target{0.f};
    if(fgfx::handTarget(h, ent, state, target))
    {
        at = target;
    }
    glm::vec3 viewFwd, viewRight, viewUp;
    hands::angleVectors(aim, viewFwd, viewRight, viewUp);
    glm::vec3 toward = at - pos;
    toward = glm::length(toward) > 1e-3f ? glm::normalize(toward) : viewFwd;
    const float aimShare = za::clamp(vr_flat_fg_palm_aim.value, 0.f, 1.f);
    glm::vec3 palm = viewFwd + (toward - viewFwd) * aimShare;
    palm = glm::length(palm) > 1e-3f ? glm::normalize(palm) : viewFwd;

    // The fingers up (the world's up off the palm's normal; looking straight up or down, the view's up), turned out about it.
    glm::vec3 fingers = glm::vec3{0.f, 0.f, 1.f} - palm * palm.z;
    if(glm::length(fingers) < 0.2f)
    {
        fingers = viewUp - palm * glm::dot(viewUp, palm);
    }
    fingers = glm::normalize(fingers);
    const float tilt = glm::radians(za::clamp(vr_flat_fg_finger_tilt.value, -90.f, 90.f)) * side;
    fingers = glm::normalize(glm::angleAxis(-tilt, palm) * fingers);

    const glm::mat3 axes = palmAxes(h, palm, fingers);
    blendTo(s, h, pos, axes, k);
    g.palmOff = glm::degrees(za::acos(za::clamp(glm::dot(palm, toward), -1.f, 1.f)));
}

void carryPose(hands::State& s, int h, const glm::vec3& aim)
{
    const float k = carryShare(h);
    if(k <= 0.f)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    placeFrame(aim, fwd, right, up);
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const float span = flatplay::armReach();
    const glm::vec3 shoulder = flatplay::shoulderOf(s, h) + flatplay::eyeNudge(aim);
    const glm::vec3 pos = shoulder + (fwd * flatplay::poseNum(vr_flat_carry_fwd) + right * (side * flatplay::poseNum(vr_flat_carry_out)) -
                                         up * flatplay::poseNum(vr_flat_carry_down)) *
                                         span;
    const float posePitch = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
    const glm::vec3 rot{za::clamp(aim.x, -posePitch, posePitch) + za::clamp(vr_flat_carry_pitch.value, -90.f, 90.f),
        aim.y + side * za::clamp(vr_flat_carry_yaw.value, -90.f, 90.f), aim.z + side * za::clamp(vr_flat_carry_roll.value, -180.f, 180.f)};
    glm::vec3 f, r, u;
    hands::angleVectors(rot, f, r, u);
    blendTo(s, h, pos, glm::mat3{f, -r, u}, k);
}

// The wall torch (our stick, vr_walltorch.cpp) drawn in `h` alone.
[[nodiscard]] bool torchIn(int h)
{
    const int ent = held::heldEntity(h);
    return ent > 0 && ent < cl.num_entities && held::heldEntity(1 - h) != ent && walltorch::isStick(cl_entities[ent]);
}

// A torch held in one hand turned flame up (vr_flat_torch_upright; his note 2026-11-10: flatscreen only): the hand turned
// the short way round until the stick it holds (its place in the hand: held::placeInHand; the stick's +x is its head, QC
// VR_WallTorch_Take) stands upright, by the eased share. The hand's own turn is what the game places the torch from, so the torch turns with it.
// gs.torchOff: how far off upright the stick is after it (vr_flat_fg_report, the tests).
void uprightPose(hands::State& s, int h)
{
    gs.torchOff[h] = -1.f;
    glm::vec3 origin{0.f}, other{0.f};
    glm::mat3 axes{1.f};
    bool both = false;
    if(!torchIn(h) || held::placeInHand(h, origin, axes, both, other) == 0 || both)
    {
        return;
    }
    const glm::mat3 hand = held::axesFromAngles(&s.rot[h].x, true);
    const glm::vec3 stick = glm::normalize(hand * axes[0]);
    const glm::vec3 up{0.f, 0.f, 1.f};
    const float k = geom::smooth01(gs.upright[h]);
    glm::vec3 now = stick;
    if(k > 0.f)
    {
        glm::vec3 axis = glm::cross(stick, up);
        const float angle = za::acos(za::clamp(glm::dot(stick, up), -1.f, 1.f));
        axis = glm::length(axis) > 1e-4f ? glm::normalize(axis) : hand[0]; // (head straight down: over the hand's forward)
        const glm::quat turn = glm::angleAxis(angle * k, axis);
        const glm::mat3 m = glm::mat3_cast(turn) * hand;
        s.rot[h] = hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
        now = glm::normalize(glm::mat3_cast(turn) * stick);
    }
    gs.torchOff[h] = glm::degrees(za::acos(za::clamp(glm::dot(now, up), -1.f, 1.f)));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_fg_report", report_f);
    Cmd_AddCommand("vr_flat_fg_press", press_f);
}

void press()
{
    if(!flatplay::active())
    {
        return;
    }
    gs.presses++;
    if(vr_flat_fg_pose.value == 0.f)
    {
        // The pose off: the grab at once, the game choosing the hand (as before 2026-11-10; it still takes no second
        // object to a hand that is pulling one).
        gs.sent++;
        Cbuf_InsertText("impulse 46\n");
        return;
    }
    // The main hand when it is free (his note 2026-11-08), else the off hand: the game's own choice (QC
    // VR_Forcegrab_FlatGrab), told it so it is the hand turned out.
    int h = -1;
    for(int c : {HAND_MAIN, HAND_OFF})
    {
        if(canTake(c))
        {
            h = c;
            break;
        }
    }
    if(gs.grab[HAND_MAIN].phase == Phase::Turn || gs.grab[HAND_MAIN].phase == Phase::Pull || flying(HAND_MAIN))
    {
        gs.doubled++; // (a press while the main hand is still on its grab: it goes to the off hand, or nowhere)
    }
    if(h < 0)
    {
        gs.refused++;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatfgrab: no free hand for the grab\n");
        }
        return;
    }
    Grab& g = gs.grab[h];
    g.detached = onGrip(h);
    g.turnTime = za::max(0.f, vr_flat_fg_turn_time.value) + (g.detached ? za::max(0.f, vr_flat_fg_detach_time.value) : 0.f);
    g.phase = Phase::Turn;
    g.at = vr_gametime;
    g.sawFlight = false;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatfgrab: %s hand: turning its palm out over %.2f s%s\n", handName(h), g.turnTime,
            g.detached ? " (off the gun's grip first)" : "");
    }
    if(g.turnTime <= 0.f)
    {
        g.share = 1.f;
        send(h);
    }
}

void frame(bool on)
{
    double dt = gs.lastTime > -1e8 ? vr_gametime - gs.lastTime : 0.0;
    gs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    if(!on)
    {
        const GrabState kept = gs;
        gs = GrabState{};
        gs.presses = kept.presses;
        gs.sent = kept.sent;
        gs.refused = kept.refused;
        gs.doubled = kept.doubled;
        return;
    }
    const double now = vr_gametime;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        Grab& g = gs.grab[h];
        const double t = now - g.at;
        switch(g.phase)
        {
        case Phase::Turn:
        {
            g.share = g.turnTime > 0.f ? za::clamp(static_cast<float>(t / g.turnTime), 0.f, 1.f) : 1.f;
            if(t >= g.turnTime)
            {
                g.share = 1.f;
                send(h);
            }
            break;
        }
        case Phase::Pull:
        {
            g.share = 1.f;
            const bool fly = flying(h);
            g.sawFlight = g.sawFlight || fly;
            if(g.sawFlight && !fly)
            {
                startBack(h, held::handEmpty(h) ? "missed" : "caught");
            }
            else if(!g.sawFlight && t > pullGrace)
            {
                startBack(h, "nothing flew");
            }
            else if(t > pullMost)
            {
                startBack(h, "held too long");
            }
            break;
        }
        case Phase::Back:
        {
            const float back = za::max(0.01f, vr_flat_fg_release_time.value);
            const float k = za::clamp(static_cast<float>(t / back), 0.f, 1.f);
            g.share = g.backFrom * (1.f - k);
            if(k >= 1.f)
            {
                g.phase = Phase::Idle;
                g.share = 0.f;
            }
            break;
        }
        default: g.share = 0.f; break;
        }

        // The carry pose held up in front (vr_flat_carry_pose 1): a prop in this hand alone (not a weapon, not one held in
        // both), eased in and out. (2, the default: the prop in the fist, in the guard: flatplay's empty hand's pose.)
        const int ent = held::heldEntity(h);
        const bool carrying = za::round(flatplay::poseNum(vr_flat_carry_pose)) == 1.f && ent != 0 && flatplay::heldIn(h) < 0 &&
                              held::heldEntity(h == HAND_MAIN ? HAND_OFF : HAND_MAIN) != ent;
        const float ease = static_cast<float>(dt) / za::max(0.02f, vr_flat_carry_time.value);
        gs.carry[h] = za::clamp(gs.carry[h] + (carrying ? ease : -ease), 0.f, 1.f);
        // A torch held in one hand turned flame up (vr_flat_torch_upright), not while it strikes or reaches.
        const bool upright = vr_flat_torch_upright.value != 0.f && torchIn(h) && !flatplay::punching(h) &&
                             !flatplay::shoving(h) && g.phase != Phase::Turn && g.phase != Phase::Pull;
        gs.upright[h] = za::clamp(gs.upright[h] + (upright ? ease : -ease), 0.f, 1.f);
    }
}

void apply(hands::State& s, const glm::vec3& aim)
{
    if(!flatplay::active())
    {
        return;
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        carryPose(s, h, aim);
        grabPose(s, h, aim);
        uprightPose(s, h);
    }
}

bool busy(int h)
{
    if(h < 0 || h >= HAND_COUNT || !flatplay::active())
    {
        return false;
    }
    const Phase p = gs.grab[h].phase;
    return p == Phase::Turn || p == Phase::Pull;
}

float openShare(int h)
{
    return h >= 0 && h < HAND_COUNT && flatplay::active() ? geom::smooth01(gs.grab[h].share) : 0.f;
}

float carryShare(int h)
{
    return h >= 0 && h < HAND_COUNT && flatplay::active() ? geom::smooth01(gs.carry[h]) : 0.f;
}

} // namespace qvr::flatfgrab
