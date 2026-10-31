// vr_dirparryviz.hpp -- Show Directional Parry (vr_dirparry_debug): the directional parry's readings drawn.
//
// - The QC (vr_dirparry.qc VR_DRP_Debug) sends each reading of an armed humanoid's swing as its own code computes it:
//   its blade's line then (and where the span it follows begins and ends), its way, your weapon's parry line, the
//   ideal (square to its blade and its way) through your line's nearest point, the gap between the two lines
//   (debugdirparry), then the reading's note: its verdict (a strike's: parried, partial, miss; or a live reading during
//   its strike, or its blow held back for the window), where they met and its numbers (debugdirparrynote).
// - draw (with the view's debug draws; flat: vr_ragtool.cpp's overlay) draws the live reading, and the last strike's
//   verdict held a few seconds with a readout (score against the threshold as a bar, the angle and the contact with
//   their limits and marks, the hand, when, the swing's speed) facing you. A strike's verdict is also printed.
// - Debug only: nothing here changes the game; nothing is kept or drawn while off.
#pragma once

#include <glm/vec3.hpp>

namespace qvr::dirparryviz
{

void PF_debugdirparry();     // debugdirparry(kind, a, b): a line of the reading being sent
void PF_debugdirparrynote(); // float debugdirparrynote(verdict, at, score, angle, contact, more): the reading ends
void draw(const glm::vec3& eye); // the view's debug draws
[[nodiscard]] bool on();         // vr_dirparry_debug (the flat view's overlay draws the lines then)

} // namespace qvr::dirparryviz
