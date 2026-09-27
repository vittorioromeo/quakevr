// vr_hands.cpp -- see vr_hands.hpp.

#include "vr_hands.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_flick.hpp"
#include "vr_handpose.hpp"
#include "vr_main.hpp"
#include "vr_throw.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_weapons.hpp"

#include <cmath>

using namespace qvr;

namespace qvr::hands
{
namespace
{

State state;
int stateFrame = -1;

// OpenXR tracking space (+x right, +y up, -z forward) to Quake (+x forward, +y left, +z up).
[[nodiscard]] glm::vec3 quakeFromTracking(const glm::vec3& v)
{
    return {-v.z, -v.x, v.y};
}

// Quake angles (pitch down positive, yaw, roll) of a tracking-space orientation, turned by
// `yawOffset` degrees.
[[nodiscard]] glm::vec3 anglesFromTracking(const glm::quat& q, float yawOffset)
{
    const glm::vec3 f = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 0.f, -1.f}), yawOffset);
    const glm::vec3 u = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 1.f, 0.f}), yawOffset);
    return anglesFromVectors(f, u);
}

[[nodiscard]] glm::mat3 basisOf(const glm::vec3& angles); // below

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

// Each hand's calibrated point off its tracked one, and its grip's (tracking space, metres): their velocities are the
// tracked ones' plus the turn's (update, updateVelocities).
glm::vec3 calibratedOffset[2]{glm::vec3{0.f}, glm::vec3{0.f}};
glm::vec3 calibratedGripOffset[2]{glm::vec3{0.f}, glm::vec3{0.f}};

// Rotation of the play space around the vertical axis: accumulated snap/smooth turning, and
// re-based whenever the server sets the view angle (spawning, teleporters).
float turnYaw = 0.f;
bool pendingYawValid = false;
float pendingYaw = 0.f;

// Body-relative positions of the previous update, for velocities when the runtime reports none.
struct Previous
{
    bool valid{false};
    double time{0.0};
    glm::vec3 hands[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 head{0.f};
};

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
    float drop{0.f};            // how far below that it is, smoothed (a walk's bob evened out)
    glm::vec2 handsRef{0.f};    // the hands' middle off the box's middle (metres), learnt while the head is over it
    bool handsRefValid{false};
    float speed{0.f};           // the head's horizontal speed (metres a second), smoothed
    float moving{0.f};          // seconds it has been moving
    float hold{0.f};            // how sure it is a lean, smoothed: quick to rise, slower to let go
    glm::vec4 cues{0.f};        // drop, tilt, hands (0..1 each), standing height: for vr_debug_lean
};

LeanSense leanSense;

void resetLean()
{
    lean = glm::vec3{0.f};
    lastBodyValid = false;
    leanSense = LeanSense{};
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
    LeanSense& ls = leanSense;
    const float calibrated = units::eyeHeight();
    const float height = t.head.position.y;
    // In the room's metres (vr_roomscale_move_mult scales the head's motion in the game, not the body's shape).
    const float toMetres = 1.f / (m2u * std::max(0.1f, vr_roomscale_move_mult.value));
    const glm::vec2 off = glm::vec2{lean.x, lean.y} * toMetres;
    const float offLen = glm::length(off);
    const glm::vec2 dir = offLen > 1e-4f ? off / offLen : glm::vec2{0.f};
    constexpr float CENTRED = 0.05f; // metres: the head over the box's middle
    const auto ease = [&](float tau) { return 1.f - std::exp(-dt / tau); };

    if(!ls.valid)
    {
        ls = LeanSense{};
        ls.standing = height > 0.85f * calibrated ? height : calibrated;
        ls.valid = true;
    }

    // The standing height: learnt while the head is over the box and not crouching, rising quickly (standing up
    // straighter), sinking slowly (settling); kept while the head is off (a lean's drop is not forgotten).
    if(offLen < CENTRED && height > ls.standing - 0.12f)
    {
        ls.standing += (height - ls.standing) * ease(height > ls.standing ? 0.4f : 4.f);
    }
    ls.standing = CLAMP(0.8f * calibrated, ls.standing, 1.25f * calibrated);
    ls.drop += (std::max(0.f, ls.standing - height) - ls.drop) * ease(0.15f);

    // A lean swings the head on an arc about the hips (about 0.43 of the eyes' height below them): how much lower that
    // puts it this far out. A step keeps the height (a walk bobs about a centimetre).
    const float arm = 0.43f * ls.standing;
    const float reach = std::min(offLen, 0.9f * arm);
    const float arcDrop = arm - std::sqrt(arm * arm - reach * reach);
    const float dropCue = smoothstep01(std::max(0.015f, 0.35f * arcDrop), std::max(0.035f, 0.75f * arcDrop), ls.drop);

    // The head tilted the way it is off: rolled towards it (sideways), or pitched towards it (forward only half: the
    // eyes look down walking too).
    const auto horizontal = [&](const glm::vec3& v) {
        const glm::vec3 w = rotateYaw(quakeFromTracking(t.head.orientation * v), turnYaw);
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
        const glm::vec3 rel = rotateYaw(quakeFromTracking(mid - t.head.position), turnYaw);
        const glm::vec2 hands = off + glm::vec2{rel.x, rel.y}; // metres off the box's middle
        if(offLen < CENTRED)
        {
            ls.handsRef = ls.handsRefValid ? ls.handsRef + (hands - ls.handsRef) * ease(0.3f) : hands;
            ls.handsRefValid = true;
        }
        else if(ls.handsRefValid)
        {
            const float went = glm::dot(hands - ls.handsRef, dir) / offLen; // 1: as far as the head
            const float low = std::max(t.hands[0].position.y, t.hands[1].position.y) < 0.62f * ls.standing ? 1.f : 0.5f;
            handsCue = low * smoothstep01(0.75f, 0.35f, went);
        }
    }
    else
    {
        ls.handsRefValid = false;
    }

    // Weighed together (any one can tell), scaled by vr_lean_detect.
    const float sure = 1.f - (1.f - dropCue) * (1.f - 0.85f * tiltCue) * (1.f - 0.7f * handsCue);
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
    if(lastBodyValid && glm::length(glm::vec2{body.x - lastBody.x, body.y - lastBody.y}) > 64.f)
    {
        lean = glm::vec3{0.f};
        leanSense.handsRefValid = false;
    }
    lastBody = body;
    lastBodyValid = true;

    const glm::vec3 head{t.head.position.x, 0.f, t.head.position.z};
    if(!t.head.valid)
    {
        lastHeadValid = false;
        return;
    }

    glm::vec3 step{0.f};
    if(lastHeadValid)
    {
        const glm::vec3 delta =
            rotateYaw(quakeFromTracking(head - lastHead) * m2u, turnYaw) * vr_roomscale_move_mult.value;

        // A jump (recentred play space, tracking lost and found) is not a step.
        if(glm::length(delta) < 50.f)
        {
            step = glm::vec3{delta.x, delta.y, 0.f};
            lean += step;
        }
    }

    lastHead = head;
    lastHeadValid = true;

    const float dt = static_cast<float>(CLAMP(0.0, host_frametime, 0.1));
    const bool detect = vr_lean_detect.value > 0.f;
    senseLean(t, m2u, step, dt); // also the standing height, for the drawn body's lean

    const float radius = CLAMP(0.f, vr_lean_radius.value, 14.f);
    const float length = glm::length(lean);
    if(length > radius)
    {
        // Past the radius the body walks after the head (if its box can: else the head stops).
        const glm::vec3 excess = lean * ((length - radius) / length);
        roomscaleMove += excess;
        lean -= excess;
        return;
    }

    // Back under the head: at vr_lean_recenter (0: never); with vr_lean_detect, not while leaning, and otherwise also
    // as fast as it takes to close the gap in about a quarter of a second. While the head moves, only once it has kept
    // going for a moment at a walking pace (a lean's first instants show few cues, and a slow lean drifts); at rest,
    // as the cues say.
    float speed = vr_lean_recenter.value * m2u;
    if(detect && speed > 0.f)
    {
        const LeanSense& ls = leanSense;
        const float going =
            ls.moving > 0.f ? smoothstep01(0.12f, 0.35f, ls.moving) * smoothstep01(0.15f, 0.35f, ls.speed) : 1.f;
        speed = std::max(speed, length / 0.25f) * (1.f - ls.hold) * going;
    }
    if(length > 0.01f && speed > 0.f && !noclip_anglehack)
    {
        const float step = std::min(length, speed * dt);
        const glm::vec3 to = body + lean * (step / length);
        if(worldtrace::playerBoxFits(body, to) && worldtrace::line(to, to - glm::vec3{0.f, 0.f, 48.f}) < 1.f)
        {
            roomscaleMove += to - body;
            lean -= to - body;
        }
    }
}

// Fills the velocities: from the runtime when it has them, else by differencing body-relative
// positions (the flat-screen hands, or a runtime without velocities).
void updateVelocities(const TrackingState* t)
{
    const float u2m = 1.f / units::metresToUnits();
    const double dt = previous.valid ? realtime - previous.time : 0.0;
    // Recomputed within the same frame (a turn, a server yaw): keep the frame's velocities.
    const auto differenced = [&](const glm::vec3& now, const glm::vec3& before, const glm::vec3& same) {
        if(!previous.valid)
        {
            return glm::vec3{0.f};
        }
        return dt > 0.0 ? (now - before) * u2m / static_cast<float>(dt) : same;
    };
    const auto fromTracking = [&](const glm::vec3& v) { return rotateYaw(quakeFromTracking(v), turnYaw); };

    // Relative to the floor below the head (the box's middle and the lean): the body sliding back
    // under the head is not the hands' own motion.
    const glm::vec3 base = state.playerOrigin + state.lean;
    const glm::vec3 head = state.head - base;
    state.headVel = t && t->head.velocityValid ? fromTracking(t->head.linearVelocity)
                                               : differenced(head, previous.head, state.headVel);

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 local = state.pos[h] - base;
        if(t && t->hands[h].velocityValid)
        {
            // A calibrated hand's point off the tracked one goes round with the controller's turn.
            const glm::vec3& off = calibratedOffset[h];
            state.vel[h] = fromTracking(off == glm::vec3{0.f} ? t->hands[h].linearVelocity
                                                               : t->hands[h].linearVelocity + glm::cross(t->hands[h].angularVelocity, off));
            state.angVel[h] = fromTracking(t->hands[h].angularVelocity);
        }
        else
        {
            state.vel[h] = differenced(local, previous.hands[h], state.vel[h]);
            state.angVel[h] = glm::vec3{0.f};
        }

        previous.hands[h] = local;
        // On the runtime's clock when it has one: the release is timed on it too. Throws go with
        // the palm, where the object is held, not the controller point further out.
        const double time = t && t->time >= 0.0 ? t->time : realtime;
        const glm::vec3& gripOff = calibratedGripOffset[h];
        const glm::vec3 throwVel = t && t->hands[h].velocityValid && t->hands[h].gripVelocityValid
                                       ? fromTracking(gripOff == glm::vec3{0.f} ? t->hands[h].gripVelocity
                                                                               : t->hands[h].gripVelocity +
                                                                                     glm::cross(t->hands[h].angularVelocity, gripOff))
                                       : state.vel[h];
        throwing::sample(h, time, state.pos[h], throwVel, state.angVel[h], forward(state.rot[h]));
    }

    previous.head = head;
    previous.time = realtime;
    previous.valid = true;
}

// Yaw the head faces, also when looking steeply down or up (then the head's up vector tells
// which way the face points). From the old engine's VR_GetHeadFwdAngleBlended.
[[nodiscard]] float headYawBlended()
{
    const float pitch = state.headAngles.x;
    if(std::fabs(pitch) <= 50.f)
    {
        return state.headAngles.y;
    }

    glm::vec3 fwd, right, up;
    angleVectors(state.headAngles, fwd, right, up);
    const glm::vec3 dir = glm::mix(fwd, pitch > 0.f ? up : -up, std::fabs(pitch) / 90.f);
    return glm::degrees(std::atan2(dir.y, dir.x));
}

// The torso faces between the head and the hands (old engine's VR_GetBodyYawAngle): the head's
// yaw, pulled towards where the hands are relative to the shoulders.
[[nodiscard]] float bodyYaw(const TrackingState& t)
{
    const float headYaw = headYawBlended();
    if(!t.hands[HAND_OFF].valid || !t.hands[HAND_MAIN].valid)
    {
        return headYaw;
    }

    glm::vec3 headFwd, headRight, headUp;
    angleVectors({0.f, headYaw, 0.f}, headFwd, headRight, headUp);

    const glm::vec3 chest = state.playerOrigin + state.lean - headFwd * 10.f;
    const glm::vec3 shoulders[2]{chest - headRight * 6.5f, chest + headRight * 6.5f};

    glm::vec3 handDir{0.f};
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 hand = state.pos[h];
        hand.z = shoulders[HAND_OFF].z;
        handDir += (hand - shoulders[h]) * 0.5f;
    }
    handDir /= 10.f;

    // Hands behind the body pull only a little.
    if(glm::dot(handDir, headFwd) < 0.f && glm::length(handDir) > 0.1f)
    {
        handDir = glm::normalize(handDir) * 0.1f;
    }

    const glm::vec3 dir = glm::mix(headFwd, handDir, 0.8f);
    return glm::length(dir) > 0.f ? glm::degrees(std::atan2(dir.y, dir.x)) : headYaw;
}

// Round 21, third pass: the held weapon's Hand and Weapon Together offset (vr_wofs_whole_*: x forward, y left, z up;
// pitch up, yaw left, roll), applied to the hand as tracked and calibrated, in its aim frame, about its point: the
// weapon, the hand, the muzzle, the aim and the melee all follow. As for the main hand, mirrored for the off hand (as
// the weapon's own offsets). Not for the empty hand.
void applyWholeOffset(int h)
{
    glm::mat3 turn{1.f};
    if(wholeOffset(weapons::heldSlot(h), h, state.pos[h], state.rot[h], turn))
    {
        state.wholeTurn[h] = turn;
    }
}

void update()
{
    QVR_PROFILE("hands");
    state.valid = false;
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected || !cl.viewentity)
    {
        previous.valid = false;
        lastHeadValid = false;
        roomscaleMove = glm::vec3{0.f};
        resetLean();
        return;
    }

    const TrackingState& t = tracking();
    const entity_t& player = cl_entities[cl.viewentity];
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const float yaw = cl.viewangles[YAW];
    const float m2u = units::metresToUnits();

    state.playerOrigin = {player.origin[0], player.origin[1], player.origin[2]};

    // Stair steps: the origin rises at once, so ease the body (head, eyes, hands) up after it,
    // as V_CalcRefdef does for the flat view (80 units/s, at most 12 behind).
    {
        float& smoothZ = stairSmoothZ;
        double& lastTime = stairLastTime;
        const float z = state.playerOrigin.z;
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
        state.playerOrigin.z = smoothZ;
    }

    if(vrActive())
    {
        updateRoomscale(t, m2u, state.playerOrigin);
        state.lean = lean;
        state.leanHold = vr_lean_detect.value > 0.f ? leanSense.hold : 0.f;
        state.leanCues = leanSense.cues;
        state.standingHeight = leanSense.valid ? leanSense.standing : units::eyeHeight();

        // Positions are relative to the play-space floor below the head: the box's middle and the
        // lean.
        const glm::vec3 floorBelowHead{t.head.position.x, 0.f, t.head.position.z};
        const glm::vec3 base = state.playerOrigin + lean + glm::vec3{0.f, 0.f, vr_floor_offset.value};
        const auto toWorld = [&](const glm::vec3& trackingPos) {
            return base + rotateYaw(quakeFromTracking(trackingPos - floorBelowHead) * m2u, turnYaw);
        };

        if(pendingYawValid)
        {
            // Turn the play space so that the head faces the yaw the server asked for.
            pendingYawValid = false;
            turnYaw = pendingYaw - anglesFromTracking(t.head.orientation, 0.f).y;
        }

        state.head = toWorld(t.head.position);
        state.headAngles = anglesFromTracking(t.head.orientation, turnYaw);
        state.headHeight = t.head.position.y;

        const FrameState& frame = frameState();
        for(int eye = 0; eye < 2; eye++)
        {
            state.eyeOrigin[eye] = toWorld(frame.eyes[eye].pose.position);
            state.eyeAngles[eye] = anglesFromTracking(frame.eyes[eye].pose.orientation, turnYaw);
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
            calibratedOffset[h] = handPos - t.hands[h].position;
            calibratedGripOffset[h] = cal.move != glm::vec3{0.f} ? gripOri * trackingFromQuake(cal.move * 0.01f) : glm::vec3{0.f};
            state.pos[h] = toWorld(handPos);
            state.rot[h] = anglesFromTracking(handOri, turnYaw);
            state.calibratedPos[h] = state.pos[h];
            state.calibratedRot[h] = state.rot[h];
            state.calTurn[h] = cal.turn.z != 0.f ? basisOf(state.rot[h]) * glm::transpose(basisOf(anglesFromTracking(unrolled, turnYaw)))
                                                 : glm::mat3{1.f};
            state.controllerPos[h] = toWorld(t.hands[h].position);
            state.controllerRot[h] = anglesFromTracking(t.hands[h].orientation, turnYaw);
            state.gripPos[h] = toWorld(gripPos);
            state.gripRot[h] = anglesFromTracking(gripOri, turnYaw);
            state.aimRot[h] = state.rot[h];
            state.wholeTurn[h] = glm::mat3{1.f};
            applyWholeOffset(h);
        }

        handpose::resolvePositions(state, turnYaw);
        twohand::apply(state);
        handpose::weightDirections(state, turnYaw);

        // The server takes the aim from the move's view angles (.v_angle): the main hand.
        for(int i = 0; i < 3; i++)
        {
            cl.viewangles[i] = state.rot[HAND_MAIN][i];
        }
    }
    else
    {
        // Flat screen ("fake VR" in the old engine): hands held in front of the view, following
        // its pitch too, so that looking down reaches for the floor.
        glm::vec3 fwd, right, up;
        angleVectors(aim, fwd, right, up);

        state.lean = glm::vec3{0.f};
        state.leanHold = 0.f;
        state.standingHeight = units::eyeHeight();
        state.head = state.playerOrigin + glm::vec3{0.f, 0.f, cl.viewheight};
        state.headAngles = aim;
        state.headHeight = vr_height_calibration.value;

        const glm::vec3 centre = state.playerOrigin + fwd * 16.5f + up * 15.5f;
        state.pos[HAND_OFF] = centre - right * 5.5f;
        state.pos[HAND_MAIN] = centre + right * 5.5f;
        for(glm::vec3& rot : state.rot)
        {
            rot = aim + glm::vec3{0.f, 0.f, vr_fakevr_handroll.value};
        }
        for(int h = 0; h < HAND_COUNT; h++)
        {
            state.calTurn[h] = glm::mat3{1.f};
            state.controllerPos[h] = state.calibratedPos[h] = state.pos[h];
            state.controllerRot[h] = state.calibratedRot[h] = state.aimRot[h] = state.rot[h];
            state.gripPos[h] = state.pos[h];
            state.gripRot[h] = state.rot[h];
            state.wholeTurn[h] = glm::mat3{1.f};
            applyWholeOffset(h);
        }
    }

    updateVelocities(vrActive() ? &t : nullptr);
    flick::update(state);

    state.bodyYaw = vrActive() ? bodyYaw(t) : yaw;

    state.crouchRatio = state.headHeight > 0.f
                            ? CLAMP(0.f, vr_height_calibration.value / state.headHeight - 1.f, 1.f)
                            : 0.f;

    body::updateHotspots(state);
    twohand::updateHotspots(state); // a carried gun's handle

    state.valid = true;
}

} // namespace

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

void setServerYaw(float yaw)
{
    pendingYawValid = true;
    pendingYaw = yaw;
    stateFrame = -1; // recompute the hands with the new yaw
}

void addTurn(float degrees)
{
    turnYaw = std::remainder(turnYaw + degrees, 360.f);
    stateFrame = -1;
}

float playSpaceYaw()
{
    return turnYaw;
}

void resetClientState()
{
    stairSmoothZ = 0.f;
    stairLastTime = -1.0;
    previous.valid = false;
    stateFrame = -1;
}

void setPlaySpaceYaw(float yaw)
{
    turnYaw = std::remainder(yaw, 360.f);
    pendingYawValid = false;
    stateFrame = -1;
}

void setLean(const glm::vec3& worldLean)
{
    lean = glm::vec3{worldLean.x, worldLean.y, 0.f};
    stateFrame = -1;
}

glm::vec3 takeRoomscaleMove()
{
    const glm::vec3 move = roomscaleMove;
    roomscaleMove = glm::vec3{0.f};
    return move;
}

State& current()
{
    if(stateFrame != host_framecount)
    {
        stateFrame = host_framecount;
        update();
    }

    return state;
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
           up * vr_height_calibration.value * offsets.z;
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
    const float pitch = glm::degrees(std::asin(CLAMP(-1.f, -fwd.z, 1.f)));
    const float yaw = glm::degrees(std::atan2(fwd.y, fwd.x));

    glm::vec3 f0, r0, u0;
    angleVectors({pitch, yaw, 0.f}, f0, r0, u0);
    const float roll = glm::degrees(std::atan2(glm::dot(up, r0), glm::dot(up, u0)));

    return {pitch, yaw, roll};
}

glm::vec3 rotateYaw(const glm::vec3& v, float degrees)
{
    const float r = glm::radians(degrees);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
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

namespace
{

// rot's axes (forward, left, up).
[[nodiscard]] glm::mat3 basisOf(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    angleVectors(angles, f, r, u);
    return glm::mat3{f, -r, u};
}

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
