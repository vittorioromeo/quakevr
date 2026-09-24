// vr_body.hpp -- the player's body in the world: holster positions and hand hotspots.

#pragma once

#include "vr_hands.hpp"

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
};

[[nodiscard]] glm::vec3 holsterPosition(const hands::State& s, Holster holster);

// Where a hand is, for holstering, two-handed grabs and passing a weapon between hands.
[[nodiscard]] Hotspot hotspot(const hands::State& s, int hand);

// The hotspot of a holster, for highlighting it when a hand hovers it.
[[nodiscard]] Hotspot holsterHotspot(Holster holster);

// Tuning aids (old engine's vr_showfn): markers at the holsters (vr_show_hip_holsters,
// vr_show_shoulder_holsters, vr_show_upper_holsters) sized to their reach, green while a hand
// is there, and at the virtual stock's shoulders (vr_show_virtual_stock). Once per frame.
void queueDebug(const hands::State& s);

} // namespace qvr::body
