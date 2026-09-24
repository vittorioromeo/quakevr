// vr_twohand.hpp -- two-handed aiming: steadying a weapon with the other (empty) hand.

#pragma once

#include "vr_hands.hpp"

namespace qvr::twohand
{

// Turns the holding hands' rotations towards the helping hands, before they become the aim.
void apply(hands::State& s);

// Whether either hand is (mostly) aiming two-handed: VRBITS0_2H_AIMING.
[[nodiscard]] bool aiming();

// How far into a two-handed grip `hand` (holding the weapon) is, 0..1.
[[nodiscard]] float transition(int hand);

// Whether `hand` is the helping hand of a two-handed grip (it follows the weapon).
[[nodiscard]] bool helping(int hand);

void reset();

} // namespace qvr::twohand
