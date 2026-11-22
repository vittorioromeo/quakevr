// vr_units.hpp -- real-world scale: metres to Quake units, and the player's body.
//
// At vr_world_scale 1 a Quake unit is 1.5 inches (0.0381 m), the classic estimate from the
// player's 56-unit height; vr_world_scale makes the world bigger (above 1) around the player.

#pragma once

#include "vr_cvars.hpp"

#include <cstdlib>

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
// vr_flat_body_height as the mode takes it, in or out of the mode (a headset's slide sets its body's floor by it:
// vr_hands.cpp placeTracked).
[[nodiscard]] float flatBodyEyeHeight();

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

// vr_world_scale's default (a player who never changed it).
[[nodiscard]] inline float defaultWorldScale()
{
    const float s = vr_world_scale.default_string ? static_cast<float>(atof(vr_world_scale.default_string)) : 0.f;
    return s > 0.f ? s : 1.f;
}

// A bot's (FrikBot's) standing eye height in world units, its feet to its eyes: the default player's -- the eye height
// the body models are made for (vr_height_calibration's default) at vr_world_scale's default. His voice notes
// 2026-11-21: bots looked "very small compared to the player"; their box's view height (46 units) at world scale 1 drew
// them some three quarters of a player's height. Its body (vr_server.cpp writeAvatars, vr_mpavatar.cpp setupBody), its
// drawn head and its hands' pose (vr_builtins_melee.inc botflatpose) all take it.
[[nodiscard]] inline float botEyeUnits()
{
    return modelEyeHeight * perMetre * defaultWorldScale();
}

// ... how far above its box's eyes (`boxEye`: its view height over its feet, DEFAULT_VIEWHEIGHT - mins_z) its drawn head is.
[[nodiscard]] inline float botEyeRaise(float boxEye)
{
    return botEyeUnits() - boxEye;
}

} // namespace qvr::units
