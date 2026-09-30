// vr_handpose.hpp -- where the tracked hands end up in the world: collision with the level
// (hands and weapon muzzles), and the weight of what they hold (its spring: vr_weight.cpp).

#pragma once

#include "vr_hands.hpp"

namespace qvr::handpose
{

// Before two-handed aiming: collisions. `turnYaw` is the play space's.
void resolvePositions(hands::State& s, float turnYaw);

// After two-handed aiming: the weight (what each hand holds, through its spring; vr_weight.cpp).
void weightDirections(hands::State& s, float turnYaw);

// Whether the weapon in `hand` is pushed back by a wall (no two-handed grip then).
[[nodiscard]] bool gunColliding(int hand);

// How far this frame's resolvePositions moved `hand` (out of the walls, with what it holds; vr_debug_hand_offset).
[[nodiscard]] glm::vec3 wallPush(int hand);

void reset();

} // namespace qvr::handpose
