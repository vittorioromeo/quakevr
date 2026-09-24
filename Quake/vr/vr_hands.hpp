// vr_hands.hpp -- per-frame client-side VR body state: head, hands, body yaw and crouch, in
// world space. Computed once per host frame from the backend's tracking (or the standing pose
// when VR is off) and shared by the VR move, the view entities and later the menus.

#pragma once

#include "vr_engine.hpp"

namespace qvr::hands
{

struct State
{
    bool valid{false};

    glm::vec3 playerOrigin{0.f};
    glm::vec3 head{0.f};        // world eye position
    glm::vec3 headAngles{0.f};  // pitch, yaw, roll (view convention: pitch down is positive)
    float headHeight{0.f};      // metres above the play-space floor
    float bodyYaw{0.f};
    float crouchRatio{0.f};     // 0 standing .. 1 crouched (relative to vr_height_calibration)

    glm::vec3 eyeOrigin[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] left, [1] right (headset only)
    glm::vec3 eyeAngles[2]{glm::vec3{0.f}, glm::vec3{0.f}};

    glm::vec3 pos[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    glm::vec3 rot[2]{glm::vec3{0.f}, glm::vec3{0.f}};

    // Weapon muzzles, from the weapon models' anchor vertices: set by the view when it renders,
    // and kept until the next render (moves are sent before rendering).
    bool muzzleValid[2]{false, false};
    glm::vec3 muzzle[2]{glm::vec3{0.f}, glm::vec3{0.f}};
};

// The server set the view yaw: turn the play space to match (headset only).
void setServerYaw(float yaw);
[[nodiscard]] float playSpaceYaw();

// Updated at most once per host frame; valid only while connected to a VR-protocol server.
[[nodiscard]] State& current();

// Body anchor used for holsters, the torso and shoulder stocks: `offsets` are forward, right
// and up (scaled by vr_height_calibration), from the player origin.
[[nodiscard]] glm::vec3 bodyAnchor(const State& s, const glm::vec3& offsets);

// Helpers.
[[nodiscard]] glm::vec3 forward(const glm::vec3& angles);
void angleVectors(
    const glm::vec3& angles, glm::vec3& fwd, glm::vec3& right, glm::vec3& up);
[[nodiscard]] glm::vec3 redirect(const glm::vec3& v, const glm::vec3& angles); // f*x + r*y + u*z

} // namespace qvr::hands
