// vr_body.hpp -- the player's body in the world: holster positions and hand hotspots.

#pragma once

#include "vr_hands.hpp"

#include <array>

namespace qvr::body
{

enum Holster : int
{
    LeftShoulder,
    RightShoulder,
    LeftHip,
    RightHip,
    LeftUpper,
    RightUpper,
    HolsterCount
};

// QC/vr_defs.qc QVR_HS_*.
enum Hotspot : int
{
    HS_NONE = 0,
    HS_OFFHAND_2H_GRAB = 1,
    HS_MAINHAND_2H_GRAB = 2,
    HS_LEFT_SHOULDER_HOLSTER = 3,
    HS_RIGHT_SHOULDER_HOLSTER = 4,
    HS_LEFT_HIP_HOLSTER = 5,
    HS_RIGHT_HIP_HOLSTER = 6,
    HS_HAND_SWITCH = 7,
    HS_LEFT_UPPER_HOLSTER = 8,
    HS_RIGHT_UPPER_HOLSTER = 9,
    HS_CARRIED_GRIP = 10, // the handle of the gun the other hand carries by its foregrip (vr_twohand.cpp)
    HS_GRENADE_POUCH = 11, // the grenade pouch at the small of the back (vr_handgrenade; either hand)
};

// Holster positions follow the body's lean and crouch (vr_avatar) with vr_body_anchors, else the
// old engine's placement. For several holsters, holsterPositions solves the body once.
using HolsterPositions = std::array<glm::vec3, HolsterCount>;
[[nodiscard]] glm::vec3 holsterPosition(const hands::State& s, Holster holster);

// Where a holster rests on the drawn body (vr_body_mode, vr_body_anchors: the hips and upper
// holsters on its front): the way the body's surface faces there (`out`, away from the body), its
// up along the surface, and how far the holster's position stands out of it (world units). `out`
// is zero where the holster is not on the body (no body, the shoulders, the old placement).
struct HolsterPlate
{
    glm::vec3 out{0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    float clearance{0.f};
};
using HolsterPlates = std::array<HolsterPlate, HolsterCount>;
[[nodiscard]] HolsterPositions holsterPositions(const hands::State& s, HolsterPlates* plates = nullptr);

// The grenade pouch (vr_handgrenade, vr_grenade_pouch_*; ROUND21.md, "Hand grenades from the back pouch"): on the belt
// at the small of the back, where a hand reaches for it, and (`plate`) the body's surface there (its `out` behind the
// back; zero `out` without the body: the pouch then faces straight back). Placed as the hip holsters are: carried by
// the pelvis with vr_body_anchors, round the hips' ring (never in the body), else the old placement's.
[[nodiscard]] bool pouchEnabled();
[[nodiscard]] glm::vec3 pouchPosition(const hands::State& s, HolsterPlate* plate = nullptr);
[[nodiscard]] float pouchReach(); // world units (vr_grenade_pouch_thresh)

// hands::bodyAnchor, carried by the chest with vr_body_anchors (the virtual stock's shoulders).
[[nodiscard]] glm::vec3 chestAnchor(const hands::State& s, const glm::vec3& offsets);

// Where each hand is (s.hotspot), for holstering, two-handed grabs and passing a weapon between
// hands.
void updateHotspots(hands::State& s);

// Round 21's hip and upper holster X (vr_hip_offset_x, vr_upper_holster_offset_x) on the body: stopped at the body's
// front then, it goes round the body now; each becomes the X that keeps the holster where it was (vr_migrate_config).
void migrateHolsters();

// How near a hand must be to a holster to be at it (world units: vr_*_holster_thresh).
[[nodiscard]] float holsterReach(Holster holster);

// The hotspot of a holster, for highlighting it when a hand hovers it.
[[nodiscard]] Hotspot holsterHotspot(Holster holster);

// Tuning aids (old engine's vr_showfn): markers at the holsters (vr_show_hip_holsters,
// vr_show_shoulder_holsters, vr_show_upper_holsters) sized to their reach, green while a hand
// is there, and at the virtual stock's shoulders (vr_show_virtual_stock). Once per frame.
void queueDebug(const hands::State& s);

} // namespace qvr::body
