// vr_client.hpp -- client-side Quake VR state shared with other VR sources.

#pragma once

#include "vr_backend.hpp"

#include <glm/glm.hpp>

namespace qvr::client
{

// Networked model transform of a client entity (see U_QVR_* in vr_protocol.hpp).
struct EntityVr
{
    glm::vec3 scale{0.f}; // offset from 1: 0 means unscaled
    glm::vec3 scaleOrigin{0.f};
    glm::vec3 offset{0.f};
    bool noRotate{false}; // a rigid body: an EF_ROTATE model (the backpack) must not spin
};

void init(); // registers input commands

// Whether the +grab button of `hand` (HAND_OFF, HAND_MAIN) is held.
[[nodiscard]] bool grabbing(int hand);

// Null when the server doesn't speak the VR protocol or the entity has no VR data.
[[nodiscard]] const EntityVr* entityVr(int num);

} // namespace qvr::client
