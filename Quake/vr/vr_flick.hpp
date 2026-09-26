// vr_flick.hpp -- flick reload: spinning the super shotgun open with a flick of the wrist.

#pragma once

#include "vr_hands.hpp"

namespace qvr::flick
{

// Once per hands update: detects the gesture and sets s.visualRot (the weapon's drawn angles,
// spinning a full turn after a flick).
void update(hands::State& s);

// Whether `hand` is flicking this frame (VRBITS0_*_RELOADFLICKING).
[[nodiscard]] bool flicking(int hand);

// Starts the spin without the gesture (the +flickreload commands).
void spin(int hand);

// How far the weapon in `hand` has turned in its spin, in degrees (0 .. 360: 90 the barrel up, 180
// back, 270 down), or -1 when it is not spinning (vr_shells.cpp times a flick's casings by it).
[[nodiscard]] float spinAngle(int hand);

void reset();

} // namespace qvr::flick
