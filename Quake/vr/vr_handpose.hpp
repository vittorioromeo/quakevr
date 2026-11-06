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

// Flatscreen play's stop (vr_flatsight.cpp): how far the weapon in `hand`, at `pos` turned to `rot` with its muzzle `muzzle`
// from the hand, must go along `way` (a unit vector, into the body) for the lines from `from` (his head: clear) to its
// points to meet no surface, at most `most`. The same points, traces and margin as the headset's stop, along a set way
// rather than the least way out: the hand's place in the view is the pose's, and a slide along a wall takes it off it.
// `margin` (units; flatscreen play's vr_flat_wall_margin): a wall (not a floor or a ceiling) must also be this far off
// each point, the hand's own included, so the weapon and the hand on it stop short of the wall, not on it (0: as above).
[[nodiscard]] float gunBack(int hand, const glm::vec3& from, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    const glm::vec3& way, float most, float margin);

// The same weapon's points, as they stand: how deep the deepest is past the first wall on the line from `from` to it (0:
// all clear; floors and ceilings not counted). Flatscreen play's muzzle tilt at a wall (vr_flatsight.cpp, pushBack) tries
// its angles with it. A point counts `margin` deeper (`handMargin` for the hand's own point), so one that short of a wall
// counts as in it.
[[nodiscard]] float gunDepth(int hand, const glm::vec3& from, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    float margin, float handMargin);

// One point (an empty hand), the same way: how deep it is past the first wall on the line from `from` to it, counted
// `margin` deeper (0: clear; floors and ceilings not counted). Flatscreen play's free hand at a wall (vr_flatsight.cpp).
// The same weapon's points: the nearest's distance from `eye` (units). No traces.
[[nodiscard]] float gunNearest(int hand, const glm::vec3& eye, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle);

[[nodiscard]] float pointWallDepth(const glm::vec3& from, const glm::vec3& point, float margin);

// vr_gun_wall_depth: each hand's weapon as drawn last frame (held or carried), how far into the level it goes (its model's
// points past the surfaces the lines from the hand to them meet), and how far the walls held the hand out.
void gunWallDepth_f();

void reset();

} // namespace qvr::handpose
