// vr_hands.cpp -- see vr_hands.hpp.

#include "vr_animrec.hpp"
#include "vr_hands.hpp"
#include "vr_mem.hpp"
#include "vr_tracy.h"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_body.hpp"
#include "vr_bodytrack.hpp"
#include "vr_cvars.hpp"
#include "vr_flick.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatmove.hpp"
#include "vr_handpose.hpp"
#include "vr_handsource.hpp"
#include "vr_main.hpp"
#include "vr_motion.hpp"
#include "vr_throw.hpp"
#include "vr_timescale.hpp"
#include "vr_torso.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_seated.hpp"
#include "vr_seat.hpp"
#include "vr_slide.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"
#include "vr_api.h"


using namespace qvr;

namespace qvr::hands
{
namespace
{

// OpenXR tracking space (+x right, +y up, -z forward) to Quake (+x forward, +y left, +z up).
using geom::quakeFromTracking;

// Quake angles (pitch down positive, yaw, roll) of a tracking-space orientation, turned by
// `yawOffset` degrees.
[[nodiscard]] glm::vec3 anglesFromTracking(const glm::quat& q, float yawOffset)
{
    const glm::vec3 f = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 0.f, -1.f}), yawOffset);
    const glm::vec3 u = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 1.f, 0.f}), yawOffset);
    return anglesFromVectors(f, u);
}


// Quake axes (x forward, y left, z up) to OpenXR tracking space.
[[nodiscard]] glm::vec3 trackingFromQuake(const glm::vec3& v)
{
    return {-v.y, v.z, -v.x};
}

// Controller rotation offsets (vr_gunangle/vr_gunyaw, vr_offhandpitch/vr_offhandyaw: the calibration's pitch and yaw), in
// the controller's own frame.
[[nodiscard]] glm::quat withHandOffsets(const glm::quat& q, int hand)
{
    const Calibration c = calibration(hand);
    const float pitch = c.turn.x;
    const float yaw = c.turn.y;

    return q * glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(-pitch), glm::vec3{1.f, 0.f, 0.f});
}

// The throw's frame on the controller (vr_throw.cpp: the wrist flick's lever to the held object's centre): the
// controller turned by the pitch and yaw the hands had before the calibration (Gun Angle 39.5 and Gun Yaw 4, the off
// hand's 40.25 and -4: the defaults the throws were tuned with, until 2026-09-28), whatever the calibration now. The
// calibration makes the drawn hand match the real one; the throws, the author's feel of them, go with the controller's
// own motion, which the calibration doesn't change (ROUND21.md, "Climbing: sliding along the wall; throw angle after
// calibration"). vr_throw_pitch tilts the result.
[[nodiscard]] glm::quat throwFrame(const glm::quat& q, int hand)
{
    const float pitch = hand == HAND_MAIN ? 39.5f : 40.25f;
    const float yaw = hand == HAND_MAIN ? 4.f : -4.f;
    return q * glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(-pitch), glm::vec3{1.f, 0.f, 0.f});
}

// The rest of the hand calibration (hands::calibration), on a hand turned by withHandOffsets (`pos`, `ori`: tracking
// space) whose controller's grip pose is (`gripPos`, `gripOri`): the roll about where the hand points, through the grip,
// then the move along the grip's axes. Nothing is touched at 0 (the same numbers, bit for bit, as before it).
void calibrate(const Calibration& c, const glm::vec3& gripPos, const glm::quat& gripOri, glm::vec3& pos, glm::quat& ori)
{
    if(c.turn.z != 0.f)
    {
        const glm::quat rolled = ori * glm::angleAxis(glm::radians(c.turn.z), glm::vec3{0.f, 0.f, -1.f});
        pos = gripPos + (rolled * glm::inverse(ori)) * (pos - gripPos);
        ori = glm::normalize(rolled);
    }
    if(c.move != glm::vec3{0.f})
    {
        pos += gripOri * trackingFromQuake(c.move * 0.01f);
    }
}

// Body-relative positions of the previous update, for velocities when the runtime reports none.
struct Previous
{
    bool valid{false};
    double time{0.0};
    glm::vec3 hands[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 head{0.f};
    // poseSpin: each controller's orientation (tracking space) at its last sample's time (< 0: none), and the spin
    // since the sample before (world, rad/s).
    glm::quat handRot[2]{glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat{1.f, 0.f, 0.f, 0.f}};
    double handRotTime[2]{-1.0, -1.0};
    glm::vec3 handSpin[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float handSpinLag[2]{0.f, 0.f}; // seconds the spin is older than its sample: half the turn's interval (its middle)
    // The same of the controller's own orientation (timescale::controllerPose: the slowed hand's lags it), for the
    // throw's direction in slow motion (vr_throw.cpp strokeEstimate).
    glm::quat ownRot[2]{glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat{1.f, 0.f, 0.f, 0.f}};
    double ownRotTime[2]{-1.0, -1.0};
    glm::vec3 ownSpin[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float ownSpinLag[2]{0.f, 0.f};
};

// Leaning or walking (vr_lean_detect). The head moving off the box's middle is either a lean -- the feet stay, the
// back tilts at the hips or bends sideways, so the head goes down a little as it goes out (more the further out: an
// arc about the hips) and usually tilts the way it goes, while hands hanging at the sides stay by the hips -- or the
// player walking in the room: the whole body moves, the head keeping its standing height and staying level, the
// hands going along. The cues are weighed into how sure it is a lean (`hold`, 0..1). Within vr_lean_radius the box
// (and the drawn body's hips and feet, vr_avatar.cpp) stays while it is a lean, and otherwise follows the head, the
// faster the further it is off (walking keeps it close under the head), once the head has moved on for a moment or
// come to rest (the first instants of a lean show few cues). Past the radius the body follows the head, as ever.
struct LeanSense
{
    bool valid{false};
    float standing{0.f};        // the head's standing height (metres), learnt while over the box and upright
    bool standingSeen{false};   // ... taken from the head (not the Height setting it starts from)
    float calibratedAt{0.f};    // the Height setting it was learnt with (a new one, measured standing, starts it over)
    float steadyAt{0.f};        // the head's height it has held (within 3 cm) ...
    float steadyFor{0.f};       // ... this long (seconds): the first standing height is taken from a steady head
    float drop{0.f};            // how far below that it is, smoothed (a walk's bob evened out)
    glm::vec2 handsRef{0.f};    // the hands' middle off the box's middle (metres), learnt while the head is over it
    bool handsRefValid{false};
    float speed{0.f};           // the head's horizontal speed (metres a second), smoothed
    float moving{0.f};          // seconds it has been moving
    float hold{0.f};            // how sure it is a lean, smoothed: quick to rise, slower to let go
    glm::vec4 cues{0.f};        // drop, tilt, hands (0..1 each), standing height: for vr_debug_lean
};

// A seat's hands rig: its tracking this frame, the play space's turn, the body's walk and lean (phase 4b)
struct Rig
{
    State state;
    TrackingState seatedTracking; // seated (vr_seated.hpp): this frame's tracking raised
    int stateFrame = -1;
    // Each hand's calibrated point off its tracked one (tracking space, metres): its velocity is the tracked one's plus the
    // turn's (update, updateVelocities). Not the throws': they go with the controller's grip (updateVelocities).
    glm::vec3 calibratedOffset[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    // Rotation of the play space around the vertical axis: accumulated snap/smooth turning, and
    // re-based whenever the server sets the view angle (spawning, teleporters).
    float turnYaw = 0.f;
    bool pendingYawValid = false;
    float pendingYaw = 0.f;
    Previous previous;
    // The stair smoothing (update): the eased height, and the client time it was eased at (cl.time: it starts over
    // with each map, so this is reset with it).
    float stairSmoothZ = 0.f;
    double stairLastTime = -1.0;
    // Room-scale movement: the head's horizontal tracking position last frame, and the world-space
    // walk accumulated since the last move was sent.
    //
    // Leaning (vr_lean_radius): the head moves off the middle of the player's box by up to the radius
    // before the body walks after it, so that a face gets near a wall and over a railing where the box
    // (32 units wide, 16 from its middle to a side) stops. The head never leaves the box's footprint,
    // which is always in open space. Only the head's motion beyond the radius is walked; at rest the
    // body slides back under the head (vr_lean_recenter) where its box can go and there is floor under
    // it (not over a ledge) -- unless the head is leaning (vr_lean_detect, below).
    bool lastHeadValid = false;
    glm::vec3 lastHead{0.f};
    glm::vec3 roomscaleMove{0.f};
    glm::vec3 lean{0.f};
    glm::vec3 lastBody{0.f};
    bool lastBodyValid = false;
    // Reset Position (vr_recenter): the body put under the head at the next frame, where its box fits.
    bool recenterPending = false;

    // A walk through a teleporter (portalCrossing, vr_portals.cpp): the body's coming jump is not a teleport's: the play space
    // turns by the gate's yaw (not to the server's view angle), the lean turns with it (not dropped) and the stairs' easing
    // keeps its height under the head.
    bool portalJumpPending = false;
    float portalJumpYaw = 0.f;
    bool portalAngleTaken = false;
    double portalJumpTime = -1.0;
    glm::vec3 lastRawOrigin{0.f};
    bool lastRawOriginValid = false;
    LeanSense leanSense;
};

// splitscreen (SPLITSCREEN.md, phase 4b): the current seat's (seat::Part::Hands)
[[nodiscard]] Rig& rig()
{
    return seat::part<Rig>(seat::current(), seat::Part::Hands);
}

void resetLean()
{
    rig().lean = glm::vec3{0.f};
    rig().lastBodyValid = false;
    rig().leanSense = LeanSense{};
}

[[nodiscard]] float smoothstep01(float e0, float e1, float x)
{
    const float u = CLAMP(0.f, (x - e0) / (e1 - e0), 1.f);
    return u * u * (3.f - 2.f * u);
}

// How sure the head's offset from the box (`lean`, world units) is a lean, from the tracking this frame (see
// LeanSense); `step` is the head's horizontal motion this frame (world units, turned and scaled as the lean).
void senseLean(const TrackingState& t, float m2u, const glm::vec3& step, float dt)
{
    LeanSense& ls = rig().leanSense;
    const float calibrated = units::eyeHeight();
    const float height = t.head.position.y;
    // In the room's metres (vr_roomscale_move_mult scales the head's motion in the game, not the body's shape).
    const float toMetres = 1.f / (m2u * za::max(0.1f, vr_roomscale_move_mult.value));
    const glm::vec2 off = glm::vec2{rig().lean.x, rig().lean.y} * toMetres;
    const float offLen = glm::length(off);
    const glm::vec2 dir = offLen > 1e-4f ? off / offLen : glm::vec2{0.f};
    constexpr float CENTRED = 0.05f; // metres: the head over the box's middle
    const auto ease = [&](float tau) { return 1.f - za::exp(-dt / tau); };

    if(!ls.valid)
    {
        ls = LeanSense{};
        ls.valid = true;
    }
    // From the Height setting until the head is first high enough to be standing (not sitting down, the headset not on
    // the desk), from the head then; a new Height (measured standing) starts it over. (It only sinks while the head is
    // within 12 cm under it, so a start sitting down or a Height applied later left it at the old Height for good: the
    // body, its legs fitted to it, crouched under the head; ROUND22.md, "Calibration audit".)
    if(za::fabs(calibrated - ls.calibratedAt) > 0.005f)
    {
        ls.calibratedAt = calibrated;
        ls.standing = calibrated;
        ls.standingSeen = false;
    }
    // The first one taken from the head held steady a moment (0.5 s within 3 cm) at a height a standing player's head
    // has (85% to 110% of the Height setting, which is measured standing): not the first frame over 85%, which was often
    // the headset lifted over the head as it is put on at the first start, learnt for good (it sinks only from 12 cm
    // under it): the legs fitted to a head 30 cm higher, long and bent at the knees, until the map was reloaded (his
    // note vrstart_2026-10-27_01-24-52; ROUND22.md, "Legs too long at the first start").
    const bool plausible = height > 0.85f * calibrated && height < 1.1f * calibrated;
    if(plausible && za::fabs(height - ls.steadyAt) < 0.03f)
    {
        ls.steadyFor += dt;
    }
    else
    {
        ls.steadyAt = height;
        ls.steadyFor = 0.f;
    }
    // A take played back (vr_motion_play, vr_motion_eval: its map loaded afresh, the lean sense with it) starts in the
    // middle of a session whose standing height was learnt long before: its first frame high enough is taken, as it was
    // before the steady rule. Waiting for a steady head there (a take often starts already moving) left the Height
    // setting's for the whole take: the body placed otherwise than live, the replayed hands up to 15 units off theirs
    // (slash_backswing_up_right_2026-10-12_12-35-43: 3.4 -> 14.8), its blow's damage 37.0 -> 16.9 (ROUND23.md, "Eval
    // drift: the lean sense's steady rule in replays").
    const bool replay = motion::playing() || motion::evaluating();
    if(!ls.standingSeen && (replay ? height > 0.85f * calibrated : plausible && ls.steadyFor >= 0.5f))
    {
        ls.standing = height;
        ls.standingSeen = true;
    }

    // The standing height: learnt while the head is over the box and not crouching, rising quickly (standing up
    // straighter) up to 8 cm over it, slowly further (a jump, tiptoes, the headset lifted off: a second of it took it
    // over 12 cm above the head for good), sinking slowly (settling); kept while the head is off (a lean's drop is not
    // forgotten).
    if(offLen < CENTRED && height > ls.standing - 0.12f)
    {
        const bool quick = height > ls.standing && height < ls.standing + 0.08f;
        ls.standing += (height - ls.standing) * ease(quick ? 0.4f : 4.f);
    }
    ls.standing = CLAMP(0.8f * calibrated, ls.standing, 1.25f * calibrated);
    ls.drop += (za::max(0.f, ls.standing - height) - ls.drop) * ease(0.15f);

    // A lean swings the head on an arc about the hips (about 0.43 of the eyes' height below them): how much lower that
    // puts it this far out. A step keeps the height (a walk bobs about a centimetre).
    const float arm = 0.43f * ls.standing;
    const float reach = za::min(offLen, 0.9f * arm);
    const float arcDrop = arm - za::sqrt(arm * arm - reach * reach);
    const float dropCue = smoothstep01(za::max(0.015f, 0.35f * arcDrop), za::max(0.035f, 0.75f * arcDrop), ls.drop);

    // The head tilted the way it is off: rolled towards it (sideways), or pitched towards it (forward only half: the
    // eyes look down walking too).
    const auto horizontal = [&](const glm::vec3& v) {
        const glm::vec3 w = rotateYaw(quakeFromTracking(t.head.orientation * v), rig().turnYaw);
        return glm::vec2{w.x, w.y};
    };
    const glm::vec2 up = horizontal({0.f, 1.f, 0.f});
    glm::vec2 fwd = horizontal({0.f, 0.f, -1.f});
    fwd = glm::length(fwd) > 1e-3f ? glm::normalize(fwd) : glm::vec2{1.f, 0.f};
    const glm::vec2 side{-fwd.y, fwd.x};
    const float along = glm::dot(dir, fwd);
    const float towards = glm::dot(dir, side) * glm::dot(up, side) + along * glm::dot(up, fwd) * (along > 0.f ? 0.5f : 1.f);
    const float tiltCue = smoothstep01(0.08f, 0.26f, towards); // about 5 to 15 degrees

    // The hands: their middle off the box, learnt while the head is over it; with the head off, how far they went
    // along with it. Hanging at the sides they stay by the hips in a lean, and walk along in a step; held up they
    // tell less (aiming round a corner, they lean with the head).
    float handsCue = 0.f;
    if(t.hands[0].valid && t.hands[1].valid)
    {
        const glm::vec3 mid = (t.hands[0].position + t.hands[1].position) * 0.5f;
        const glm::vec3 rel = rotateYaw(quakeFromTracking(mid - t.head.position), rig().turnYaw);
        const glm::vec2 hands = off + glm::vec2{rel.x, rel.y}; // metres off the box's middle
        if(offLen < CENTRED)
        {
            ls.handsRef = ls.handsRefValid ? ls.handsRef + (hands - ls.handsRef) * ease(0.3f) : hands;
            ls.handsRefValid = true;
        }
        else if(ls.handsRefValid)
        {
            const float went = glm::dot(hands - ls.handsRef, dir) / offLen; // 1: as far as the head
            const float low = za::max(t.hands[0].position.y, t.hands[1].position.y) < 0.62f * ls.standing ? 1.f : 0.5f;
            handsCue = low * smoothstep01(0.75f, 0.35f, went);
        }
    }
    else
    {
        ls.handsRefValid = false;
    }

    // Weighed together (any one can tell), scaled by vr_lean_detect; with body tracking, the tracked spine's tilt blended
    // in (vr_bodytrack.cpp: a lean tilts it, a step or a neck bend does not).
    const float sure =
        bodytrack::leanSure(1.f - (1.f - dropCue) * (1.f - 0.85f * tiltCue) * (1.f - 0.7f * handsCue), off);
    const float target = CLAMP(0.f, sure * vr_lean_detect.value, 1.f);
    ls.hold += (target - ls.hold) * ease(target > ls.hold ? 0.08f : 0.35f);

    const float speed = dt > 0.f ? glm::length(glm::vec2{step.x, step.y}) * toMetres / dt : 0.f;
    ls.speed += (speed - ls.speed) * ease(0.1f);
    ls.moving = ls.speed > 0.12f ? ls.moving + dt : 0.f;
    ls.cues = {dropCue, tiltCue, handsCue, ls.standing};
}

void updateRoomscale(const TrackingState& t, float m2u, const glm::vec3& body)
{
    // A teleport, a respawn, a new map: the body is put under the head.
    if(rig().lastBodyValid && glm::length(glm::vec2{body.x - rig().lastBody.x, body.y - rig().lastBody.y}) > 64.f)
    {
        if(rig().portalJumpPending) // through a teleporter: the head where it was over the body, turned with the gate
        {
            rig().lean = rotateYaw(rig().lean, rig().portalJumpYaw);
            rig().portalJumpPending = false;
        }
        else
        {
            rig().lean = glm::vec3{0.f};
        }
        rig().leanSense.handsRefValid = false;
    }
    rig().lastBody = body;
    rig().lastBodyValid = true;

    const glm::vec3 head{t.head.position.x, 0.f, t.head.position.z};
    if(!t.head.valid)
    {
        rig().lastHeadValid = false;
        return;
    }

    glm::vec3 step{0.f};
    if(rig().lastHeadValid)
    {
        const glm::vec3 delta =
            rotateYaw(quakeFromTracking(head - rig().lastHead) * m2u, rig().turnYaw) * vr_roomscale_move_mult.value;

        // A jump (recentred play space, tracking lost and found) is not a step.
        if(glm::length(delta) < 50.f)
        {
            step = glm::vec3{delta.x, delta.y, 0.f};
            rig().lean += step;
        }
    }

    rig().lastHead = head;
    rig().lastHeadValid = true;

    if(rig().recenterPending)
    {
        rig().recenterPending = false;
        const glm::vec3 to = body + glm::vec3{rig().lean.x, rig().lean.y, 0.f};
        if(glm::length(rig().lean) > 0.01f && worldtrace::playerBoxFits(body, to))
        {
            rig().roomscaleMove += to - body;
            rig().lean = glm::vec3{0.f};
        }
        torso::reset(); // and the torso faces where the head does
    }

    const float dt = static_cast<float>(CLAMP(0.0, host_frametime, 0.1));
    const bool detect = vr_lean_detect.value > 0.f;
    senseLean(t, m2u, step, dt); // also the standing height, for the drawn body's lean

    const float radius = CLAMP(0.f, vr_lean_radius.value, 14.f);
    const float length = glm::length(rig().lean);
    if(length > radius)
    {
        // Past the radius the body walks after the head (if its box can: else the head stops).
        const glm::vec3 excess = rig().lean * ((length - radius) / length);
        rig().roomscaleMove += excess;
        rig().lean -= excess;
        return;
    }

    // Back under the head: at vr_lean_recenter (0: never); with vr_lean_detect, not while leaning, and otherwise also
    // as fast as it takes to close the gap in about a quarter of a second. While the head moves, only once it has kept
    // going for a moment at a walking pace (a lean's first instants show few cues, and a slow lean drifts); at rest,
    // as the cues say.
    float speed = vr_lean_recenter.value * m2u;
    if(detect && speed > 0.f)
    {
        const LeanSense& ls = rig().leanSense;
        const float going =
            ls.moving > 0.f ? smoothstep01(0.12f, 0.35f, ls.moving) * smoothstep01(0.15f, 0.35f, ls.speed) : 1.f;
        speed = za::max(speed, length / 0.25f) * (1.f - ls.hold) * going;
    }
    if(length > 0.01f && speed > 0.f && !noclip_anglehack)
    {
        const float step = za::min(length, speed * dt);
        const glm::vec3 to = body + rig().lean * (step / length);
        if(worldtrace::playerBoxFits(body, to) && worldtrace::line(to, to - glm::vec3{0.f, 0.f, 48.f}) < 1.f)
        {
            rig().roomscaleMove += to - body;
            rig().lean -= to - body;
        }
    }
}

// Body drift (vr_debug_body_error, vr_body_error): where the game has the body's box (the feet) in the room, in metres
// on Quake's axes (x forward, y left) of the play space unturned, and which way the torso faces there (degrees): the
// head as tracked less the lean, turned back by the play space's turn. A player standing still in the room keeps both
// whatever the stick does; the mark is where they were.
struct BodyDrift
{
    bool marked{false};
    glm::vec2 pos{0.f};
    float yaw{0.f};
    bool logging{false};
    double nextPrint{0.0};
};

BodyDrift bodyDrift;

[[nodiscard]] glm::vec2 bodyInRoom(const TrackingState& t, float m2u)
{
    const glm::vec3 head = quakeFromTracking(glm::vec3{t.head.position.x, 0.f, t.head.position.z});
    const glm::vec3 leanRoom = rotateYaw(rig().lean, -rig().turnYaw) / (m2u * za::max(0.1f, vr_roomscale_move_mult.value));
    return glm::vec2{head.x - leanRoom.x, head.y - leanRoom.y};
}

[[nodiscard]] float bodyYawInRoom()
{
    return za::remainder(rig().state.bodyYaw - rig().turnYaw, 360.f);
}

void markBodyDrift()
{
    bodyDrift.marked = true;
    bodyDrift.pos = bodyInRoom(tracking(), units::metresToUnits());
    bodyDrift.yaw = bodyYawInRoom();
}

void printBodyDrift(const char* label)
{
    if(!vrActive() || !rig().state.valid)
    {
        Con_Printf("body error: no headset\n");
        return;
    }
    if(!bodyDrift.marked)
    {
        markBodyDrift();
    }
    const float m2u = units::metresToUnits();
    const glm::vec2 off = (bodyInRoom(tracking(), m2u) - bodyDrift.pos) * 100.f;
    const float yawOff = za::remainder(bodyYawInRoom() - bodyDrift.yaw, 360.f);
    // The lean in the body's frame (cm forward, left of the torso).
    const glm::vec3 leanBody = rotateYaw(rig().lean, -rig().state.bodyYaw) * (100.f / m2u);
    Con_Printf("body error%s%s (t %.2f): pos %.1f cm (fwd %.1f left %.1f) yaw %.1f; lean fwd %.1f left %.1f cm, hold %.2f\n",
        *label ? " " : "", label, vr_gametime, glm::length(off), off.x, off.y, yawOff, leanBody.x, leanBody.y, rig().state.leanHold);
}

// A controller's spin (tracking space, rad/s) from its turn since its previous sample (vr_throw_spin_from_pose): what
// its orientation did, whatever frame the runtime gives its angular velocity in (VirtualDesktopXR: the controller's
// own, not the tracking space's). Kept within a frame resampled at the same time; none after a gap of over 0.1 s.
// `own`: of the controller's own orientation (timescale::controllerPose), not the hand's. `lag`: seconds the spin is
// older than `time` (the turn's average over the interval is its middle's: half a frame, 7 ms at 72 fps, 2 at 240; the
// throw's estimate takes it there, vr_throw.cpp).
[[nodiscard]] glm::vec3 poseSpin(int h, const glm::quat& rot, double time, float& lag, bool own = false)
{
    glm::quat& lastRot = own ? rig().previous.ownRot[h] : rig().previous.handRot[h];
    double& lastTime = own ? rig().previous.ownRotTime[h] : rig().previous.handRotTime[h];
    glm::vec3& lastSpin = own ? rig().previous.ownSpin[h] : rig().previous.handSpin[h];
    float& lastLag = own ? rig().previous.ownSpinLag[h] : rig().previous.handSpinLag[h];
    const double dt = time - lastTime;
    if(rig().previous.valid && lastTime >= 0.0 && dt == 0.0)
    {
        lag = lastLag;
        return lastSpin;
    }
    glm::vec3 spin{0.f};
    lag = 0.f;
    if(rig().previous.valid && lastTime >= 0.0 && dt > 0.0 && dt < 0.1)
    {
        lag = static_cast<float>(dt * 0.5);
        glm::quat d = rot * glm::inverse(lastRot);
        if(d.w < 0.f)
        {
            d = -d;
        }
        const glm::vec3 v{d.x, d.y, d.z};
        if(const float s = glm::length(v); s > 1e-7f)
        {
            spin = v * (2.f * za::atan2(s, d.w) / (s * static_cast<float>(dt)));
        }
    }
    lastRot = rot;
    lastTime = time;
    lastSpin = spin;
    lastLag = lag;
    return spin;
}

// Fills the velocities: from the runtime when it has them, else by differencing body-relative
// positions (the flat-screen hands, or a runtime without velocities).
void updateVelocities(const TrackingState* t)
{
    const float u2m = 1.f / units::metresToUnits();
    const double dt = rig().previous.valid ? vr_gametime - rig().previous.time : 0.0;
    // Recomputed within the same frame (a turn, a server yaw): keep the frame's velocities.
    const auto differenced = [&](const glm::vec3& now, const glm::vec3& before, const glm::vec3& same) {
        if(!rig().previous.valid)
        {
            return glm::vec3{0.f};
        }
        return dt > 0.0 ? (now - before) * u2m / static_cast<float>(dt) : same;
    };
    const auto fromTracking = [&](const glm::vec3& v) { return rotateYaw(quakeFromTracking(v), rig().turnYaw); };

    // Relative to the floor below the head (the box's middle and the lean): the body sliding back
    // under the head is not the hands' own motion.
    const glm::vec3 base = rig().state.playerOrigin + rig().state.lean;
    const glm::vec3 head = rig().state.head - base;
    rig().state.headVel = t && t->head.velocityValid ? fromTracking(t->head.linearVelocity)
                                               : differenced(head, rig().previous.head, rig().state.headVel);

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 local = rig().state.pos[h] - base;
        if(t && t->hands[h].velocityValid)
        {
            // A calibrated hand's point off the tracked one goes round with the controller's turn.
            const glm::vec3& off = rig().calibratedOffset[h];
            rig().state.vel[h] = fromTracking(off == glm::vec3{0.f} ? t->hands[h].linearVelocity
                                                               : t->hands[h].linearVelocity + glm::cross(t->hands[h].angularVelocity, off));
            rig().state.angVel[h] = fromTracking(t->hands[h].angularVelocity);
        }
        else
        {
            rig().state.vel[h] = differenced(local, rig().previous.hands[h], rig().state.vel[h]);
            rig().state.angVel[h] = glm::vec3{0.f};
        }

        rig().previous.hands[h] = local;
        // On the runtime's clock when it has one: the release is timed on it too. Throws go with
        // the palm, where the object is held, not the controller point further out; and with the
        // controller alone, whatever the hand calibration (see throwFrame).
        const double time = t && t->time >= 0.0 ? t->time : vr_gametime;
        throwing::Motion drawn{rig().state.pos[h], rig().state.vel[h], rig().state.angVel[h], forward(rig().state.rot[h])};
        // The controller's own motion where the slowed hand lags it (slow motion: timescale::controllerPose), the same
        // otherwise: the way a throw goes (vr_throw.cpp strokeEstimate).
        throwing::Motion own = drawn;
        const Pose* ctrl = t ? timescale::controllerPose(h) : nullptr;
        rig().state.ownVel[h] = rig().state.vel[h];
        if(ctrl && ctrl->velocityValid)
        {
            const glm::vec3& off = rig().calibratedOffset[h];
            rig().state.ownVel[h] = fromTracking(off == glm::vec3{0.f} ? ctrl->linearVelocity
                                                                 : ctrl->linearVelocity + glm::cross(ctrl->angularVelocity, off));
        }
        if(t && t->hands[h].velocityValid)
        {
            const Pose& p = t->hands[h];
            drawn.vel = fromTracking(p.gripVelocityValid ? p.gripVelocity : p.linearVelocity);
            drawn.forward = forward(anglesFromTracking(throwFrame(p.orientation, h), rig().turnYaw));
            float spinLag = 0.f, ownSpinLag = 0.f;
            const glm::vec3 spin = poseSpin(h, p.orientation, time, spinLag);
            const glm::vec3 ownSpin = poseSpin(h, ctrl ? ctrl->orientation : p.orientation, time, ownSpinLag, true);
            if(vr_throw_spin_from_pose.value)
            {
                drawn.angVel = fromTracking(spin);
            }
            else
            {
                spinLag = ownSpinLag = 0.f; // the runtime's: of the sample's own time
            }
            drawn.spinLag = spinLag;
            own = drawn;
            if(ctrl && ctrl->velocityValid)
            {
                own.pos = rig().state.pos[h] + fromTracking(ctrl->position - p.position) * units::metresToUnits();
                own.vel = fromTracking(ctrl->gripVelocityValid ? ctrl->gripVelocity : ctrl->linearVelocity);
                own.forward = forward(anglesFromTracking(throwFrame(ctrl->orientation, h), rig().turnYaw));
                own.angVel = vr_throw_spin_from_pose.value ? fromTracking(ownSpin) : fromTracking(ctrl->angularVelocity);
                own.spinLag = ownSpinLag;
            }
        }
        throwing::sample(h, time, drawn, own);
    }

    rig().previous.head = head;
    rig().previous.time = vr_gametime;
    rig().previous.valid = true;
}

// Yaw the head faces, also when looking steeply down or up (then the head's up vector tells
// which way the face points). From the old engine's VR_GetHeadFwdAngleBlended.
[[nodiscard]] float headYawBlended()
{
    const float pitch = rig().state.headAngles.x;
    if(za::fabs(pitch) <= 50.f)
    {
        return rig().state.headAngles.y;
    }

    glm::vec3 fwd, right, up;
    angleVectors(rig().state.headAngles, fwd, right, up);
    const glm::vec3 dir = glm::mix(fwd, pitch > 0.f ? up : -up, za::fabs(pitch) / 90.f);
    return glm::degrees(za::atan2(dir.y, dir.x));
}

// Round 21, third pass: the held weapon's Hand and Weapon Together offset (vr_wofs_whole_*: x forward, y left, z up;
// pitch up, yaw left, roll), applied to the hand as tracked and calibrated, in its aim frame, about its point: the
// weapon, the hand, the muzzle, the aim and the melee all follow. As for the main hand, mirrored for the off hand (as
// the weapon's own offsets). Not for the empty hand.
void applyWholeOffset(int h)
{
    glm::mat3 turn{1.f};
    if(wholeOffset(weapons::heldSlot(h), h, rig().state.pos[h], rig().state.rot[h], turn))
    {
        rig().state.wholeTurn[h] = turn;
    }
}

// update's phases: the stair steps' easing of the body.
void easeStairs(const entity_t& player)
{
    float& smoothZ = rig().stairSmoothZ;
    double& lastTime = rig().stairLastTime;
    const float z = rig().state.playerOrigin.z;
    if(rig().portalJumpPending && cl.time - rig().portalJumpTime > 0.5)
    {
        rig().portalJumpPending = false; // (no jump came: a gate leading close by)
    }
    if(rig().portalJumpPending && rig().lastRawOriginValid &&
        glm::length(glm::vec2{rig().state.playerOrigin.x - rig().lastRawOrigin.x, rig().state.playerOrigin.y - rig().lastRawOrigin.y}) > 64.f)
    {
        smoothZ += z - rig().lastRawOrigin.z; // through a teleporter: still that far under the origin
    }
    else if(rig().lastRawOriginValid && cl.onground && z > rig().lastRawOrigin.z && lastTime >= 0.0)
    {
        // carried up by a lift the body rides with it: only a step's jump is eased (V_CalcRefdef's rule: the eye
        // 12 units behind a rising plat, "compressed by the lift", his note of 2026-11-21; vr_view_lift_follow)
        smoothZ += VR_ViewLiftRise(z - rig().lastRawOrigin.z, static_cast<float>(cl.time - lastTime));
    }
    rig().lastRawOrigin = rig().state.playerOrigin;
    rig().lastRawOriginValid = true;
    if(!noclip_anglehack && cl.onground && z - smoothZ > 0.f && lastTime >= 0.0)
    {
        if(cl.time != lastTime)
        {
            smoothZ += static_cast<float>(CLAMP(0.0, cl.time - lastTime, 0.1)) * 80.f;
        }
        smoothZ = CLAMP(z - 12.f, smoothZ, z);
    }
    else
    {
        smoothZ = z;
    }
    // Not eased up from where the player was before the server first placed it (the world's origin, after a
    // loaded game or a new map): the body rose 12 units over its first frames, and a two-handed hold taken
    // then kept the gap (vr_held.cpp).
    lastTime = player.msgtime > 0.0 ? cl.time : -1.0;
    rig().state.playerOrigin.z = smoothZ;
}

// The tracked head, eyes and hands placed in the world (vrActive).
void placeTracked(const TrackingState& t, float m2u, float seatedLift)
{
    updateRoomscale(t, m2u, rig().state.playerOrigin);
    rig().state.lean = rig().lean;
    rig().state.leanHold = vr_lean_detect.value > 0.f ? rig().leanSense.hold : 0.f;
    rig().state.leanCues = rig().leanSense.cues;
    rig().state.standingHeight = rig().leanSense.valid ? rig().leanSense.standing : units::eyeHeight();

    // Positions are relative to the play-space floor below the head: the box's middle and the
    // lean.
    const glm::vec3 floorBelowHead{t.head.position.x, 0.f, t.head.position.z};
    glm::vec3 base = rig().state.playerOrigin + rig().lean + glm::vec3{0.f, 0.f, vr_floor_offset.value};
    // Sliding (vr_slide, vr_slide_camera; vr_slide.cpp viewDrop): the whole tracked frame let down, eased, until the eyes
    // are the flat slide's -- the view, the hands and the body hung from the head all with it, as the flat slide's crouch
    // lowers its head (placeFlat); the head's own moves stay one to one on top. Its height (headHeight) with it, so the
    // body's floor stays the box's.
    const FrameState& frame = frameState();
    const glm::vec3 eyesTracked =
        0.5f * (frame.eyes[0].pose.position + frame.eyes[1].pose.position) + glm::vec3{0.f, seatedLift, 0.f};
    const float slideDrop =
        slide::viewDrop(base.z + (quakeFromTracking(eyesTracked - floorBelowHead) * m2u).z - rig().state.playerOrigin.z);
    base.z -= slideDrop;
    const auto toWorld = [&](const glm::vec3& trackingPos) {
        return base + rotateYaw(quakeFromTracking(trackingPos - floorBelowHead) * m2u, rig().turnYaw);
    };

    if(rig().pendingYawValid)
    {
        // Turn the play space so that the head faces the yaw the server asked for.
        rig().pendingYawValid = false;
        rig().turnYaw = rig().pendingYaw - anglesFromTracking(t.head.orientation, 0.f).y;
    }
    rig().state.turnYaw = rig().turnYaw;
    rig().state.spaceOrigin = toWorld(glm::vec3{0.f});
    // The tracked body, as the head and hands (seated: raised as they are, the floor under it lowered).
    bodytrack::place(base, floorBelowHead - glm::vec3{0.f, seatedLift, 0.f}, t.head.position, rig().turnYaw, m2u);

    rig().state.head = toWorld(t.head.position);
    rig().state.headAngles = anglesFromTracking(t.head.orientation, rig().turnYaw);
    rig().state.headHeight = t.head.position.y - slideDrop / m2u;
    // Sliding: the body's floor (the head less headHeight: vr_avatar.cpp solveTorso) where the flat slide's is, off the
    // box's floor the slide pose's hips are measured from (vr_avatar_pitch.cpp): Quake's standing eyes (46 over it) less
    // the flat body's eye height (vr_flat_body_height; placeFlat's head less its headHeight). Not vr_floor_offset's
    // play-space floor, 2 units over the box's at the defaults (the crouched box another unit up): the legs bent up to it
    // from the hips, the feet 4 units over the flat slide's.
    if(const float share = slide::viewDropShare(); share > 0.f)
    {
        const edict_t* pl = progs::localPlayer();
        const float flatFloor =
            rig().state.playerOrigin.z + (pl != nullptr ? pl->v.mins[2] : -24.f) + 46.f - units::flatBodyEyeHeight() * m2u;
        const float bodyFloor = rig().state.head.z - rig().state.headHeight * m2u;
        rig().state.headHeight += share * (bodyFloor - flatFloor) / m2u;
    }

    for(int eye = 0; eye < 2; eye++)
    {
        rig().state.eyeOrigin[eye] = toWorld(frame.eyes[eye].pose.position + glm::vec3{0.f, seatedLift, 0.f});
        rig().state.eyeAngles[eye] = anglesFromTracking(frame.eyes[eye].pose.orientation, rig().turnYaw);
    }

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const GripInRaw& grip = t.gripInHand[h];
        const glm::vec3 gripPos = t.hands[h].position + t.hands[h].orientation * grip.offset;
        const glm::quat gripOri = t.hands[h].orientation * grip.turn;
        // The hand on its controller: the calibration (its pitch and yaw, then its roll and move).
        const Calibration cal = calibration(h);
        glm::vec3 handPos = t.hands[h].position;
        glm::quat handOri = withHandOffsets(t.hands[h].orientation, h);
        const glm::quat unrolled = handOri;
        calibrate(cal, gripPos, gripOri, handPos, handOri);
        rig().calibratedOffset[h] = handPos - t.hands[h].position;
        rig().state.pos[h] = toWorld(handPos);
        rig().state.rot[h] = anglesFromTracking(handOri, rig().turnYaw);
        rig().state.calibratedPos[h] = rig().state.pos[h];
        rig().state.calibratedRot[h] = rig().state.rot[h];
        rig().state.calTurn[h] = cal.turn.z != 0.f ? basisOf(rig().state.rot[h]) * glm::transpose(basisOf(anglesFromTracking(unrolled, rig().turnYaw)))
                                             : glm::mat3{1.f};
        rig().state.controllerPos[h] = toWorld(t.hands[h].position);
        rig().state.controllerRot[h] = anglesFromTracking(t.hands[h].orientation, rig().turnYaw);
        rig().state.gripPos[h] = toWorld(gripPos);
        rig().state.gripRot[h] = anglesFromTracking(gripOri, rig().turnYaw);
        rig().state.aimRot[h] = rig().state.rot[h];
        rig().state.wholeTurn[h] = glm::mat3{1.f};
        applyWholeOffset(h);
        rig().state.unresolvedPos[h] = rig().state.pos[h];
        rig().state.unresolvedRot[h] = rig().state.rot[h];
    }

    handpose::resolvePositions(rig().state, rig().turnYaw);
    twohand::apply(rig().state);
    handpose::weightDirections(rig().state, rig().turnYaw);

    // The server takes the aim from the move's view angles (.v_angle): the main hand.
    for(int i = 0; i < 3; i++)
    {
        cl.viewangles[i] = rig().state.rot[HAND_MAIN][i];
    }
}

// The hands placed without a headset.
void placeFlat(const glm::vec3& aim, float yaw)
{
    // Flat screen ("fake VR" in the old engine): hands held in front of the view, following
    // its pitch too, so that looking down reaches for the floor.
    glm::vec3 fwd, right, up;
    angleVectors(aim, fwd, right, up);

    rig().state.lean = glm::vec3{0.f};
    rig().state.turnYaw = yaw;
    rig().state.spaceOrigin = rig().state.playerOrigin;
    rig().state.leanHold = 0.f;
    rig().state.standingHeight = units::eyeHeight();
    rig().state.head = rig().state.playerOrigin + glm::vec3{0.f, 0.f, cl.viewheight};
    rig().state.headAngles = aim;
    rig().state.headHeight = units::eyeHeight(); // (vr_flat_body_height: the mode's fixed height)

    // -flatscreenmode holding his crouch (Left Ctrl, +flatcrouch: vr_flatplay.cpp) puts his head down by the eased
    // vr_flat_crouch_drop, and his height with it: crouchRatio (below, the same way a tracked crouch reads), the body's
    // crouch and the legs' bend (vr_avatar.cpp), the shoulder anchor the hands are posed from (bodyAnchor), the drawn
    // view (view.c's VR_FlatViewDrop) and the crouched box his move's head gives (vr_hull.cpp, vr_server.cpp) all follow.
    // 0 standing, and 0 outside the mode: a headset player's head comes from the tracking, not from here.
    const float crouchDown = flatplay::crouchDrop();
    if(crouchDown > 0.f)
    {
        rig().state.head[2] -= crouchDown;
        rig().state.headHeight = units::eyeHeight() - crouchDown / units::metresToUnits();
    }
    // Leaning (Q / E, vr_flatmove.cpp): his head moved sideways and a little down as it swings, and the body's lean
    // toward it (the trunk tilts from the hips: vr_avatar.cpp solveTorso, as a headset player's lean). The hands, the
    // crosshair's ray, the ironsight and the wall's tilt all stand on this head, so they lean with it.
    const glm::vec3 leanShift = flatmove::leanShift();
    if(leanShift != glm::vec3{0.f})
    {
        rig().state.head += leanShift;
        rig().state.headHeight += leanShift.z / units::metresToUnits();
        rig().state.lean = glm::vec3{leanShift.x, leanShift.y, 0.f};
        rig().state.leanHold = 1.f;
    }


    const glm::vec3 centre = rig().state.playerOrigin + fwd * 16.5f + up * 15.5f;
    if(flatplay::active())
    {
        // -flatscreenmode: the hands posed from the view, the weapon in each hand and the player's own motion
        // (vr_flatplay.cpp); the pose below is the old one, for a flat game started without the flag.
        QVR_TRACY_CALL("hands: flat pose", flatplay::poseHands(rig().state, aim));
        QVR_TRACY_CALL("hands: anim preview", animrec::applyPreview(rig().state)); // the animation recorder's preview (vr_ar_play), blended over the pose
    }
    else
    {
        rig().state.pos[HAND_OFF] = centre - right * 5.5f;
        rig().state.pos[HAND_MAIN] = centre + right * 5.5f;
        for(glm::vec3& rot : rig().state.rot)
        {
            rot = aim + glm::vec3{0.f, 0.f, vr_fakevr_handroll.value};
        }
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        rig().state.calTurn[h] = glm::mat3{1.f};
        rig().state.controllerPos[h] = rig().state.calibratedPos[h] = rig().state.pos[h];
        rig().state.controllerRot[h] = rig().state.calibratedRot[h] = rig().state.aimRot[h] = rig().state.rot[h];
        rig().state.gripPos[h] = rig().state.pos[h];
        rig().state.gripRot[h] = rig().state.rot[h];
        rig().state.wholeTurn[h] = glm::mat3{1.f};
        // (Not -flatscreenmode's: Hand and Weapon Together corrects a controller's hold. His hands are posed in the view's
        // frame, the shots solved from the crosshair (vr_flatsight.cpp): moved and turned after that, a gun's shot was off.)
        if(!flatplay::active())
        {
            applyWholeOffset(h);
        }
    }
}

// Body drift's log (vr_debug_body_error).
void logBodyDrift()
{
    if(vr_debug_body_error.value && vrActive())
    {
        if(!bodyDrift.logging || !bodyDrift.marked)
        {
            markBodyDrift();
            bodyDrift.nextPrint = vr_gametime;
        }
        bodyDrift.logging = true;
        if(vr_gametime >= bodyDrift.nextPrint)
        {
            bodyDrift.nextPrint = vr_gametime + 0.5;
            printBodyDrift("");
        }
    }
    else
    {
        bodyDrift.logging = false;
    }
}

void update()
{
    QVR_PROFILE("hands");
    rig().state.valid = false;
    // A demo of another protocol (id's demo1-3) on the Quest has its view placed too: the eyes are drawn from it
    // (VR_RenderView needs the head), or the headset showed nothing while the window played it (QUEST_PLAN.md 6z). The
    // PC keeps the window's flat demo (its HUD and the demo's own gun; the headset blank), as before.
    if(!((cl.protocolflags & PRFL_QUAKEVR) || demoViewOnly()) || cls.state != ca_connected || !cl.viewentity)
    {
        rig().previous.valid = false;
        rig().lastHeadValid = false;
        torso::reset();
        rig().roomscaleMove = glm::vec3{0.f};
        resetLean();
        return;
    }

    // Seated (vr_seated.hpp): the head and the hands raised, a standing player's; the eyes and the tracked body below too.
    const bool raised = vrActive() && seated::raise(tracking(), rig().seatedTracking);
    const TrackingState& t = raised ? rig().seatedTracking : tracking();
    const float seatedLift = raised ? seated::lift() : 0.f;
    const entity_t& player = cl_entities[cl.viewentity];
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const float yaw = cl.viewangles[YAW];
    const float m2u = units::metresToUnits();

    rig().state.playerOrigin = {player.origin[0], player.origin[1], player.origin[2]};

    // Stair steps: the origin rises at once, so ease the body (head, eyes, hands) up after it,
    // as V_CalcRefdef does for the flat view (80 units/s, at most 12 behind).
    easeStairs(player);

    // Each hand's motion: a controller's in the headset, made up on the flat screen; whatever moves a hand on top marks
    // it while it is placed (vr_handsource.hpp).
    handsource::beginFrame(vrActive());
    if(vrActive())
    {
        placeTracked(t, m2u, seatedLift);
    }
    else
    {
        QVR_TRACY_CALL("hands: place flat", placeFlat(aim, yaw));
    }

    QVR_TRACY_CALL("hands: velocities", updateVelocities(vrActive() ? &t : nullptr));
    QVR_TRACY_CALL("hands: flick", flick::update(rig().state));

    // The torso's direction (vr_torso.cpp): the head's, pulled a little by the hands in front of it; with body tracking,
    // the tracked hips' (vr_bodytrack.cpp). Flatscreen play: the view's, turned by a long gun's stance (flatplay::poseHands set it,
    // and the hands stand on the shoulders of that turned body: reset to the view's here, the body was drawn unturned under them).
    rig().state.bodyYaw = vrActive() ? bodytrack::bodyYaw(torso::estimate(rig().state, headYawBlended(), rig().turnYaw,
                                                     t.hands[HAND_OFF].valid && t.hands[HAND_MAIN].valid))
                    : flatplay::active() ? rig().state.bodyYaw
                                         : yaw;
    // A headset's slide: the drawn hips and legs to the slide's way (vr_slide_legs; the avatar reads it).
    if(vrActive())
    {
        slide::headsetLegs(rig().state.bodyYaw);
    }

    rig().state.crouchRatio = rig().state.headHeight > 0.f
                            ? CLAMP(0.f, units::eyeHeight() / rig().state.headHeight - 1.f, 1.f)
                            : 0.f;

    QVR_TRACY_CALL("hands: body hotspots", body::updateHotspots(rig().state));
    QVR_TRACY_CALL("hands: twohand hotspots", twohand::updateHotspots(rig().state)); // a carried gun's handle

    // The poses' own clock (vr_move.hpp: the melee and the legs' sweep measure a blow over these times when the client
    // sends them). A headset's poses carry the tracking's sample time; the flat game's are made from the view at the
    // client's clock, so they carry that instead of nothing - and a pose that carries nothing reads as no news, which is
    // what a flatscreen fist's blows were up against (QC vr_melee.qc VR_Melee_Track, vr_legsweep.cpp).
    rig().state.poseTime = vrActive() ? t.time : vr_gametime;
    rig().state.valid = true;

    // Body drift's log (vr_debug_body_error): marked where the body is when turned on.
    logBodyDrift();
}

} // namespace

int moveHand()
{
    return vr_stick_swap.value != 0.f ? HAND_MAIN : HAND_OFF;
}

int gadgetHand()
{
    return vr_gadget_arm.value != 0.f ? HAND_MAIN : HAND_OFF;
}

Calibration calibration(int hand)
{
    Calibration c;
    if(hand == HAND_MAIN || vr_handcal_off_mirror.value != 0.f)
    {
        c.move = {vr_handcal_x.value, vr_handcal_y.value, vr_handcal_z.value};
        c.turn = {vr_gunangle.value, vr_gunyaw.value, vr_handcal_roll.value};
        if(hand != HAND_MAIN)
        {
            c.move.y = -c.move.y;
            c.turn.y = -c.turn.y;
            c.turn.z = -c.turn.z;
        }
    }
    else
    {
        c.move = {vr_handcal_off_x.value, vr_handcal_off_y.value, vr_handcal_off_z.value};
        c.turn = {vr_offhandpitch.value, vr_offhandyaw.value, vr_handcal_off_roll.value};
    }
    return c;
}

glm::quat aimedController(const glm::quat& controller, int hand)
{
    return withHandOffsets(controller, hand);
}

void portalCrossing(float yawDegrees)
{
    rig().portalJumpPending = true;
    rig().portalJumpYaw = yawDegrees;
    rig().portalAngleTaken = false;
    rig().portalJumpTime = cl.time;
}

bool portalAnglePending(float& yawDegrees)
{
    yawDegrees = rig().portalJumpYaw;
    return rig().portalJumpPending && !rig().portalAngleTaken;
}

void setServerYaw(float yaw)
{
    if(rig().portalJumpPending && !rig().portalAngleTaken)
    {
        // The server's angle for a walk through a teleporter: the play space turned by the gate's yaw itself (the
        // server's head angle is a frame old: its turn would be off by the head's own turn since).
        // Not recomputed now (addTurn would): this frame's hands were made at its start, from the body where the last
        // frame left it (the client draws a tick behind the server). Made again here, after the message that carries
        // the body, the crossing's frame drew it a frame early and the next frame the same again: a hitch at every
        // teleporter (ROUND21.md, "A frame seen through after a teleporter"). The next frame's hands take the turn, the
        // carried body and the lean together.
        rig().portalAngleTaken = true;
        rig().turnYaw = za::remainder(rig().turnYaw + rig().portalJumpYaw, 360.f);
        return;
    }
    rig().pendingYawValid = true;
    rig().pendingYaw = yaw;
    rig().stateFrame = -1; // recompute the hands with the new yaw
}

void addTurn(float degrees)
{
    rig().turnYaw = za::remainder(rig().turnYaw + degrees, 360.f);
    rig().stateFrame = -1;
}

// A stick turn (vr_lean_turn). The play space turns about the head (the view stays where it is), so the real body, a
// lean behind the head, swings round it: the lean (the head off the box's middle, kept in the world's axes) turns with
// the play space, and the box goes where the body now stands -- for a smooth turn (vr_lean_turn 2), where the box fits
// and has floor under it (sent as room-scale walking, as the body following a step is); else, and for a snap (the view
// jumps anyway), the head swings round the body instead, which stays where it stands. Not turned, the lean would point
// the way it did in the world before the turn (the drawn body leant off its own forward and stood off where the real
// one is). The lean's learnt hands (LeanSense) are off the box's middle in the world's axes too, and turn the same.
void stickTurn(float degrees, bool snap)
{
    const int mode = static_cast<int>(vr_lean_turn.value);
    if(vrActive())
    {
        const glm::vec3 turned = rotateYaw(rig().lean, degrees);
        const glm::vec3 swing = rig().lean - turned; // the box's move to keep the head where it is
        if(mode >= 2 && !snap && rig().lastBodyValid && glm::length(swing) > 1e-3f && !noclip_anglehack)
        {
            // As the body sliding back under the head: not into a wall, nor off a ledge (leant out over a drop and
            // turning, the body would swing out over it).
            const glm::vec3 from = rig().lastBody + rig().roomscaleMove;
            const glm::vec3 to = from + swing;
            if(worldtrace::playerBoxFits(from, to) && worldtrace::line(to, to - glm::vec3{0.f, 0.f, 48.f}) < 1.f)
            {
                rig().roomscaleMove += swing;
            }
        }
        rig().lean = turned;
        if(rig().leanSense.handsRefValid)
        {
            const glm::vec3 ref = rotateYaw(glm::vec3{rig().leanSense.handsRef, 0.f}, degrees);
            rig().leanSense.handsRef = glm::vec2{ref.x, ref.y};
        }
    }
    addTurn(degrees);
}

float playSpaceYaw()
{
    return rig().turnYaw;
}

void bodyError_f()
{
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "mark"))
    {
        if(vrActive() && rig().state.valid)
        {
            markBodyDrift();
        }
        printBodyDrift("mark");
        return;
    }
    printBodyDrift(Cmd_Argc() >= 2 ? Cmd_Argv(1) : "");
}

void recenter_f()
{
    rig().recenterPending = true;
    Con_Printf("vr_recenter: lean %.1f units\n", glm::length(rig().lean));
}

void resetClientState()
{
    rig().stairSmoothZ = 0.f;
    rig().stairLastTime = -1.0;
    rig().previous.valid = false;
    rig().stateFrame = -1;
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): what follows the client's time.
// seat-shared: the reset hook (it resets the current seat's)
mem::Reset clientStateReset{"hands", mem::ResetEvent::ClientState, mem::ResetPhase::State, &resetClientState};
} // namespace

void resetForPlayback()
{
    resetClientState();
    rig().lastHeadValid = false; // (the last take's head: not a step into this one)
    torso::reset();
    rig().roomscaleMove = glm::vec3{0.f};
    resetLean();
}

void setPlaySpaceYaw(float yaw)
{
    rig().turnYaw = za::remainder(yaw, 360.f);
    rig().pendingYawValid = false;
    rig().stateFrame = -1;
}

void relatch()
{
    const Previous keep = rig().previous;
    rig().stateFrame = -1;
    (void)current();
    rig().previous = keep;
}

void refresh()
{
    rig().stateFrame = -1; // the velocities are kept (the same vr_gametime: updateVelocities)
}

void setLean(const glm::vec3& worldLean)
{
    rig().lean = glm::vec3{worldLean.x, worldLean.y, 0.f};
    rig().stateFrame = -1;
}

glm::vec3 takeRoomscaleMove()
{
    const glm::vec3 move = rig().roomscaleMove;
    rig().roomscaleMove = glm::vec3{0.f};
    return move;
}

bool demoViewOnly()
{
    return cls.demoplayback && !(cl.protocolflags & PRFL_QUAKEVR) && (VR_Android() || VR_GLESCompat());
}

State& current()
{
    if(rig().stateFrame != host_framecount)
    {
        rig().stateFrame = host_framecount;
        update();
    }

    return rig().state;
}

glm::vec3 bodyAnchor(const State& s, const glm::vec3& offsets)
{
    const float heightRatio = CLAMP(0.f, s.crouchRatio, 0.8f);

    glm::vec3 fwd, right, up;
    angleVectors({heightRatio * -35.f, s.bodyYaw, 0.f}, fwd, right, up);

    // Under the head, as the drawn body is: the box's middle and the lean (vr_lean_radius).
    glm::vec3 origin = s.playerOrigin + s.lean;
    origin.z += 2.f - s.crouchRatio * 18.f;

    return origin + right * offsets.y + fwd * offsets.x +
           up * units::eyeHeight() * offsets.z;
}

glm::vec3 forward(const glm::vec3& angles)
{
    glm::vec3 fwd, right, up;
    angleVectors(angles, fwd, right, up);
    return fwd;
}

void angleVectors(const glm::vec3& angles, glm::vec3& fwd, glm::vec3& right, glm::vec3& up)
{
    vec3_t in{angles.x, angles.y, angles.z}, f, r, u;
    AngleVectors(in, f, r, u);
    fwd = {f[0], f[1], f[2]};
    right = {r[0], r[1], r[2]};
    up = {u[0], u[1], u[2]};
}

glm::vec3 anglesFromVectors(const glm::vec3& fwd, const glm::vec3& up)
{
    const float pitch = glm::degrees(za::asin(CLAMP(-1.f, -fwd.z, 1.f)));
    const float yaw = glm::degrees(za::atan2(fwd.y, fwd.x));

    glm::vec3 f0, r0, u0;
    angleVectors({pitch, yaw, 0.f}, f0, r0, u0);
    const float roll = glm::degrees(za::atan2(glm::dot(up, r0), glm::dot(up, u0)));

    return {pitch, yaw, roll};
}

glm::vec3 rotateYaw(const glm::vec3& v, float degrees)
{
    const float r = glm::radians(degrees);
    const float c = za::cos(r);
    const float s = za::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

glm::vec3 calibrationOffset(int hand)
{
    return hand >= 0 && hand < 2 ? rig().calibratedOffset[hand] : glm::vec3{0.f};
}

glm::vec3 carriedSincePlaced(const State& s, int h, const glm::vec3& placed)
{
    return s.pos[h] + rotateYaw(placed - s.placedFrom[h], s.turnYaw - s.placedTurnYaw);
}

glm::vec3 palmPoint(const State& s, int hand)
{
    return s.palmValid[hand] ? s.pos[hand] + redirect(s.palmLocal[hand], s.rot[hand]) : s.pos[hand];
}

glm::vec3 redirect(const glm::vec3& v, const glm::vec3& angles)
{
    glm::vec3 fwd, right, up;
    angleVectors(angles, fwd, right, up);
    return fwd * v.x + right * v.y + up * v.z;
}

// Quake angles' axes (forward, left, up).
glm::mat3 basisOf(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    angleVectors(angles, f, r, u);
    return glm::mat3{f, -r, u};
}

namespace
{

// A slot's Hand and Weapon Together offset, as for hand `h` (mirrored for the off hand); false if it has none.
bool wholeValues(int slot, int h, glm::vec3& p, glm::vec3& a)
{
    using weapons::Key;
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    p = weapons::vec(slot, Key::WholeX, Key::WholeY, Key::WholeZ);
    a = weapons::vec(slot, Key::WholePitch, Key::WholeYaw, Key::WholeRoll);
    if(p == glm::vec3{0.f} && a == glm::vec3{0.f})
    {
        return false;
    }
    if(h == HAND_OFF)
    {
        p.y = -p.y;
        a.y = -a.y;
        a.z = -a.z;
    }
    return true;
}

} // namespace

bool wholeOffset(int slot, int h, glm::vec3& pos, glm::vec3& rot, glm::mat3& turn)
{
    glm::vec3 p, a;
    if(!wholeValues(slot, h, p, a))
    {
        return false;
    }
    pos += redirect({p.x, -p.y, p.z}, rot);
    if(a == glm::vec3{0.f})
    {
        return false;
    }
    // rot's axes (forward, left, up) times the offset's turn in them (pitch up: the view's pitch is down).
    const glm::mat3 before = basisOf(rot);
    const glm::mat3 b = before * basisOf({-a.x, a.y, a.z});
    rot = anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
    turn = basisOf(rot) * glm::transpose(before);
    return true;
}

void undoWholeOffset(const State& s, int h, glm::vec3& pos, glm::vec3& rot)
{
    glm::vec3 p, a;
    if(!wholeValues(weapons::heldSlot(h), h, p, a))
    {
        return;
    }
    if(a != glm::vec3{0.f})
    {
        const glm::mat3 b = glm::transpose(s.wholeTurn[h]) * basisOf(rot);
        rot = anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
    }
    pos -= redirect({p.x, -p.y, p.z}, rot);
}

} // namespace qvr::hands
