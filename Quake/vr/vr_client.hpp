// vr_client.hpp -- client-side Quake VR state shared with other VR sources.

#pragma once

#include "vr_backend.hpp"

#include <glm/glm.hpp>

struct qmodel_s;

namespace qvr::client
{

// Networked model transform of a client entity (see U_QVR_* in vr_protocol.hpp).
struct EntityVr
{
    glm::vec3 scale{0.f}; // offset from 1: 0 means unscaled
    glm::vec3 scaleOrigin{0.f};
    glm::vec3 offset{0.f};
    bool noRotate{false}; // a rigid body: an EF_ROTATE model (the backpack) must not spin
    bool spin{false};     // spins as an EF_ROTATE pickup though its model is not one (a weapon pickup drawn as its prop)
    int weaponUid{0};     // a weapon prop's weapon id (QC vr_weaponinst.qc; 0: not a weapon): its blood (vr_wounds.cpp)
    int clip{-1};         // and the rounds in it (-1: not a weapon prop): its ammo screen (vr_view.cpp setupWorldWeapons)
    float hotGlow{0.f};   // and its incandescent blade's glow, 0..1 (U_QVR_WEAPONUID's bits 24-30: vr_hotblade.cpp)
    bool noMag{false};    // a weapon prop with no magazine in (U_QVR_NOMAG: vr_view.cpp setupMagazines draws none)
    bool ssgOpen{false};  // a super shotgun prop broken open (U_QVR_SSGOPEN: vr_view.cpp setupSsgParts draws it open)
    int ssgLoaded{0};     // and its chambers loaded (0-2: the barrels' skin)
};

void init(); // registers input commands

// Whether the +grab button of `hand` (HAND_OFF, HAND_MAIN) is held.
[[nodiscard]] bool grabbing(int hand);

// Null when the server doesn't speak the VR protocol or the entity has no VR data.
[[nodiscard]] const EntityVr* entityVr(int num);

// A brush model precached after the map loaded: not drawn (its lightmaps and vertices are built with the map).
[[nodiscard]] bool lateBrushModel(const qmodel_s* model);

} // namespace qvr::client
