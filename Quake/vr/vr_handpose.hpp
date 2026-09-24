// vr_handpose.hpp -- where the tracked hands end up in the world: collision with the level
// (hands and weapon muzzles), and weapon weight (heavier weapons trail the hand).

#pragma once

#include "vr_hands.hpp"

namespace qvr::handpose
{

// Before two-handed aiming: collisions and position weight. `turnYaw` is the play space's.
void resolvePositions(hands::State& s, float turnYaw);

// After two-handed aiming: direction weight.
void weightDirections(hands::State& s, float turnYaw);

// Whether the weapon in `hand` is pushed back by a wall (no two-handed grip then).
[[nodiscard]] bool gunColliding(int hand);

void reset();

} // namespace qvr::handpose
