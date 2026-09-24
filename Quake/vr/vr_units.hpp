// vr_units.hpp -- real-world scale: metres to Quake units, and the player's body.
//
// At vr_world_scale 1 a Quake unit is 1.5 inches (0.0381 m), the classic estimate from the
// player's 56-unit height; vr_world_scale makes the world bigger (above 1) around the player.

#pragma once

#include "vr_cvars.hpp"

namespace qvr::units
{

// Quake units per metre at vr_world_scale 1.
constexpr float perMetre = 1.f / 0.0381f;

// Eye height the body and gadget models are made for (make_vrbody.py, make_gadget.py).
constexpr float modelEyeHeight = 1.646f;

[[nodiscard]] inline float worldScale()
{
    return vr_world_scale.value > 0.f ? vr_world_scale.value : 1.f;
}

// Quake units per real metre.
[[nodiscard]] inline float metresToUnits()
{
    return perMetre * worldScale();
}

// The player's calibrated eye height (vr_height_calibration), in metres.
[[nodiscard]] inline float eyeHeight()
{
    return vr_height_calibration.value > 0.5f ? vr_height_calibration.value : modelEyeHeight;
}

// How much bigger the player is than the models are made for.
[[nodiscard]] inline float bodyScale()
{
    return eyeHeight() / modelEyeHeight;
}

} // namespace qvr::units
