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

void reset();

} // namespace qvr::flick
