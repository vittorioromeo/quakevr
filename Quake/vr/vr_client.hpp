// vr_client.hpp -- client-side Quake VR state shared with other VR sources.

#pragma once

#include "vr_backend.hpp"

#include <glm/glm.hpp>

struct qmodel_s;
struct entity_s;

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
    int clipSize{0};      // its magazine's size (0: unknown)
    int clip{-1};         // and the rounds in it (-1: not a weapon prop): its ammo screen (vr_view.cpp setupWorldWeapons)
    float hotGlow{0.f};   // and its incandescent blade's glow, 0..1 (U_QVR_WEAPONUID's bits 24-30: vr_hotblade.cpp)
    bool depleted{false}; // and a spent single-use enemy gun, never fired again (bit 31; QC .vr_depleted): no loot beam
                          // unless vr_enemy_weapon_depleted_loot (vr_lootbeams.cpp)
    bool noMag{false};    // a weapon prop with no magazine in (U_QVR_NOMAG: vr_view.cpp setupMagazines draws none)
    bool ssgOpen{false};  // a super shotgun prop broken open (U_QVR_SSGOPEN: vr_view.cpp setupSsgParts draws it open)
    int ssgLoaded{0};     // and its chambers loaded (0-2: the barrels' skin)
    // Not a weapon prop, its loot beam's state (vr_lootbeams.cpp; U_QVR_WEAPONUID with weapon id 0): -1 nothing sent,
    // else bit 7 carried in a hand, bits 0-6 a magazine's fullness in percent (127: not a magazine).
    int loot{-1};
};

void init(); // registers input commands

// Whether the +grab button of `hand` (HAND_OFF, HAND_MAIN) is held.
[[nodiscard]] bool grabbing(int hand);

// Whether the trigger of `hand` is held (+attack, +offhandattack).
[[nodiscard]] bool attacking(int hand);

// Null when the server doesn't speak the VR protocol or the entity has no VR data.
[[nodiscard]] const EntityVr* entityVr(int num);

// A brush model precached after the map loaded: not drawn (its lightmaps and vertices are built with the map).
[[nodiscard]] bool lateBrushModel(const qmodel_s* model);

// The number of a client entity: 1 up to below `limit` (-1: cl.num_entities), else -1 (null, the world, not one of
// cl_entities: your gun, a temporary or static entity).
[[nodiscard]] int entityNumber(const entity_s* e, int limit = -1);

} // namespace qvr::client
