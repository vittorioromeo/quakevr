// vr_hands.cpp -- see vr_hands.hpp.

#include "vr_hands.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_flick.hpp"
#include "vr_handpose.hpp"
#include "vr_main.hpp"
#include "vr_throw.hpp"
#include "vr_twohand.hpp"

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

[[nodiscard]] glm::vec3 rotateYaw(const glm::vec3& v, float yawDegrees)
{
    const float r = glm::radians(yawDegrees);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

[[nodiscard]] float metersToUnits()
{
    return vr_world_scale.value / (1.5f * 0.0254f);
}

// Quake angles (pitch down positive, yaw, roll) of a tracking-space orientation, turned by
// `yawOffset` degrees.
[[nodiscard]] glm::vec3 anglesFromTracking(const glm::quat& q, float yawOffset)
{
    const glm::vec3 f = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 0.f, -1.f}), yawOffset);
    const glm::vec3 u = rotateYaw(quakeFromTracking(q * glm::vec3{0.f, 1.f, 0.f}), yawOffset);
    return anglesFromVectors(f, u);
}

// Controller rotation offsets (vr_gunangle/vr_gunyaw, vr_offhandpitch/vr_offhandyaw), in the
// controller's own frame.
[[nodiscard]] glm::quat withHandOffsets(const glm::quat& q, int hand)
{
    const float pitch = hand == HAND_MAIN ? vr_gunangle.value : vr_offhandpitch.value;
    const float yaw = hand == HAND_MAIN ? vr_gunyaw.value : vr_offhandyaw.value;

    return q * glm::angleAxis(glm::radians(yaw), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(-pitch), glm::vec3{1.f, 0.f, 0.f});
}

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

// Room-scale movement: the head's horizontal tracking position last frame, and the world-space
// walk accumulated since the last move was sent.
bool lastHeadValid = false;
glm::vec3 lastHead{0.f};
glm::vec3 roomscaleMove{0.f};

void updateRoomscale(const TrackingState& t, float m2u)
{
    const glm::vec3 head{t.head.position.x, 0.f, t.head.position.z};
    if(!t.head.valid)
    {
        lastHeadValid = false;
        return;
    }

    if(lastHeadValid)
    {
        const glm::vec3 delta =
            rotateYaw(quakeFromTracking(head - lastHead) * m2u, turnYaw) * vr_roomscale_move_mult.value;

        // A jump (recentred play space, tracking lost and found) is not a step.
        if(glm::length(delta) < 50.f)
        {
            roomscaleMove += delta;
        }
    }

    lastHead = head;
    lastHeadValid = true;
}

// Fills the velocities: from the runtime when it has them, else by differencing body-relative
// positions (the flat-screen hands, or a runtime without velocities).
void updateVelocities(const TrackingState* t)
{
    const float u2m = 1.f / metersToUnits();
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

    const glm::vec3 head = state.head - state.playerOrigin;
    state.headVel = t && t->head.velocityValid ? fromTracking(t->head.linearVelocity)
                                               : differenced(head, previous.head, state.headVel);

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 local = state.pos[h] - state.playerOrigin;
        if(t && t->hands[h].velocityValid)
        {
            state.vel[h] = fromTracking(t->hands[h].linearVelocity);
            state.angVel[h] = fromTracking(t->hands[h].angularVelocity);
        }
        else
        {
            state.vel[h] = differenced(local, previous.hands[h], state.vel[h]);
            state.angVel[h] = glm::vec3{0.f};
        }

        previous.hands[h] = local;
        // On the runtime's clock when it has one: the release is timed on it too.
        const double time = t && t->time >= 0.0 ? t->time : realtime;
        throwing::sample(h, time, state.pos[h], state.vel[h], state.angVel[h], forward(state.rot[h]));
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

    const glm::vec3 chest = state.playerOrigin - headFwd * 10.f;
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

void update()
{
    state.valid = false;
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected || !cl.viewentity)
    {
        previous.valid = false;
        lastHeadValid = false;
        roomscaleMove = glm::vec3{0.f};
        return;
    }

    const TrackingState& t = tracking();
    const entity_t& player = cl_entities[cl.viewentity];
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const float yaw = cl.viewangles[YAW];
    const float m2u = metersToUnits();

    state.playerOrigin = {player.origin[0], player.origin[1], player.origin[2]};

    // Stair steps: the origin rises at once, so ease the body (head, eyes, hands) up after it,
    // as V_CalcRefdef does for the flat view (80 units/s, at most 12 behind).
    {
        static float smoothZ = 0.f;
        static double lastTime = -1.0;
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
        lastTime = cl.time;
        state.playerOrigin.z = smoothZ;
    }

    if(vrActive())
    {
        // Positions are relative to the play-space floor below the head.
        const glm::vec3 floorBelowHead{t.head.position.x, 0.f, t.head.position.z};
        const glm::vec3 base = state.playerOrigin + glm::vec3{0.f, 0.f, vr_floor_offset.value};
        const auto toWorld = [&](const glm::vec3& trackingPos) {
            return base + rotateYaw(quakeFromTracking(trackingPos - floorBelowHead) * m2u, turnYaw);
        };

        if(pendingYawValid)
        {
            // Turn the play space so that the head faces the yaw the server asked for.
            pendingYawValid = false;
            turnYaw = pendingYaw - anglesFromTracking(t.head.orientation, 0.f).y;
        }

        updateRoomscale(t, m2u);

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
            state.pos[h] = toWorld(t.hands[h].position);
            state.rot[h] = anglesFromTracking(withHandOffsets(t.hands[h].orientation, h), turnYaw);
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
    }

    updateVelocities(vrActive() ? &t : nullptr);
    flick::update(state);

    state.bodyYaw = vrActive() ? bodyYaw(t) : yaw;

    state.crouchRatio = state.headHeight > 0.f
                            ? CLAMP(0.f, vr_height_calibration.value / state.headHeight - 1.f, 1.f)
                            : 0.f;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        state.hotspot[h] = body::hotspot(state, h);
    }

    state.valid = true;
}

} // namespace

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

    glm::vec3 origin = s.playerOrigin;
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

glm::vec3 redirect(const glm::vec3& v, const glm::vec3& angles)
{
    glm::vec3 fwd, right, up;
    angleVectors(angles, fwd, right, up);
    return fwd * v.x + right * v.y + up * v.z;
}

} // namespace qvr::hands
