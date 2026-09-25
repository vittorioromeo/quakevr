// vr_move.hpp -- the VR block appended to clc_move (see vr_protocol.hpp).

#pragma once

#include "vr_engine.hpp"

#include <cstdint>

namespace qvr
{

struct VrHandMove
{
    glm::vec3 pos{0.f};      // world position
    glm::vec3 rot{0.f};      // angles (pitch, yaw, roll)
    glm::vec3 vel{0.f};      // metres / second (play space, turned with it; not the player's own motion)
    glm::vec3 throwVel{0.f}; // smoothed velocity used for throwing
    float velMag{0.f};
    glm::vec3 angVel{0.f};   // radians / second, of the throw
    glm::vec3 throwPos{0.f}; // world position the thrown object left the hand at
    float throwAge{0.f};     // seconds since it left the hand
};

struct VrMove
{
    glm::vec3 headAngles{0.f}; // -> .v_viewangle (.v_angle carries the aim angles)
    float vrYaw{0.f};          // accumulated snap/smooth turn
    VrHandMove hands[2];       // [0] off hand, [1] main hand
    glm::vec3 headVel{0.f};
    glm::vec3 muzzlePos[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // [0] off hand, [1] main hand
    std::uint16_t vrBits0{0};  // QVR_VRBITS0_* (QC/vr_defs.qc)
    glm::vec3 teleportTarget{0.f};
    std::uint8_t hotspots[2]{0, 0}; // QVR_HS_* for [0] off hand, [1] main hand
    glm::vec3 roomscaleMove{0.f};   // world units / second
    std::uint8_t buttons{0};        // protocol::QVR_BUTTON_*
    glm::vec3 origin{0.f};          // the player origin the client placed the hands from
    glm::vec3 headPos{0.f};         // the head (between the eyes), world
};

void writeVrMove(sizebuf_t* buf, const VrMove& move);
[[nodiscard]] VrMove readVrMove();

} // namespace qvr
