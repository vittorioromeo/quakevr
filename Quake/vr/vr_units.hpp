// vr_units.hpp -- real-world scale: metres to Quake units, and the player's body.
//
// At vr_world_scale 1 a Quake unit is 1.5 inches (0.0381 m), the classic estimate from the
// player's 56-unit height; vr_world_scale makes the world bigger (above 1) around the player.

#pragma once

#include "vr_cvars.hpp"

namespace qvr::units
{

// Quake units per metre at vr_world_scale 1.
inline constexpr float perMetre = 1.f / 0.0381f;

// Eye height the body and gadget models are made for (make_vrbody.py, make_gadget.py).
inline constexpr float modelEyeHeight = 1.646f;

[[nodiscard]] inline float worldScale()
{
    return vr_world_scale.value > 0.f ? vr_world_scale.value : 1.f;
}

// Quake units per real metre.
[[nodiscard]] inline float metresToUnits()
{
    return perMetre * worldScale();
}

// Flat-screen play (-flatscreenmode, no headset): the body's fixed height, vr_flat_body_height (the author's decision
// 2026-11-08: the body, its poses and the held weapons were tuned at 1.55 m and a headset's Height changed them); 0
// outside the mode (vr_flatplay.cpp).
[[nodiscard]] float flatEyeHeight();

// The player's eye height in metres: calibrated (vr_height_calibration) in the headset, vr_flat_body_height playing on
// the desktop screen.
[[nodiscard]] inline float eyeHeight()
{
    const float flat = flatEyeHeight();
    if(flat > 0.f)
    {
        return flat;
    }
    return vr_height_calibration.value > 0.5f ? vr_height_calibration.value : modelEyeHeight;
}

// How much bigger the player is than the models are made for.
[[nodiscard]] inline float bodyScale()
{
    return eyeHeight() / modelEyeHeight;
}

} // namespace qvr::units
