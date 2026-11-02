// vr_closequarters.hpp -- close-quarters slowdown (vr_closequarters; Locomotion > Close Quarters): walking straight into
// a live enemy's face near it is gradually slowed, so a full-speed run into one doesn't end in a mess of bodies, while
// strafing past it keeps (nearly) full speed. SV_AirMove calls VR_CloseQuartersMove (vr_api.h) on the wish velocity.
// ROUND23.md, "Close-quarters slowdown".

#pragma once

#include "vr_engine.hpp"

namespace qvr::closequarters
{

void init(); // the test command (vr_closequarters_test)

// The factors on a horizontal wish velocity `wish` of a player at `at` near a live enemy whose box's nearest point is
// `gap` metres away horizontally, `toEnemy` the unit horizontal way to it: the part towards it times `toward`, the
// sideways part times `side` (both 1: unchanged).
struct Factors
{
    float toward{1.f};
    float side{1.f};
    float weight{0.f}; // how much of the slowdown towards it applies (0 .. 1: its ramp times the angle's weight)
};
[[nodiscard]] Factors factorsFor(const glm::vec3& wish, const glm::vec3& toEnemy, float gap);

} // namespace qvr::closequarters
