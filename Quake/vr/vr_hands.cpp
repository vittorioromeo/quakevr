// vr_hands.cpp -- see vr_hands.hpp.

#include "vr_hands.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

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

    const float pitch = glm::degrees(std::asin(CLAMP(-1.f, -f.z, 1.f)));
    const float yaw = glm::degrees(std::atan2(f.y, f.x));

    glm::vec3 f0, r0, u0;
    angleVectors({pitch, yaw, 0.f}, f0, r0, u0);
    const float roll = glm::degrees(std::atan2(glm::dot(u, r0), glm::dot(u, u0)));

    return {pitch, yaw, roll};
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
// TODO VR: (P5) snap/smooth turning from the thumbstick.
float turnYaw = 0.f;
bool pendingYawValid = false;
float pendingYaw = 0.f;

void update()
{
    state.valid = false;
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected || !cl.viewentity)
    {
        return;
    }

    const TrackingState& t = tracking();
    const entity_t& player = cl_entities[cl.viewentity];
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const float yaw = cl.viewangles[YAW];
    const float m2u = metersToUnits();

    state.playerOrigin = {player.origin[0], player.origin[1], player.origin[2]};

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

    // TODO VR: (P5) blend the head direction with the hands, as the old engine did.
    state.bodyYaw = vrActive() ? state.headAngles.y : yaw;

    state.crouchRatio = state.headHeight > 0.f
                            ? CLAMP(0.f, vr_height_calibration.value / state.headHeight - 1.f, 1.f)
                            : 0.f;

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

glm::vec3 redirect(const glm::vec3& v, const glm::vec3& angles)
{
    glm::vec3 fwd, right, up;
    angleVectors(angles, fwd, right, up);
    return fwd * v.x + right * v.y + up * v.z;
}

} // namespace qvr::hands
