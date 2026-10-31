// vr_dirparryviz.hpp -- Show Directional Parry (vr_dirparry_debug): the directional parry's readings drawn.
//
// - The QC (vr_dirparry.qc VR_DRP_Debug) sends each reading of an armed humanoid's swing as its own code computes it:
//   its blade's line then (and where the span it follows begins and ends), its way, your weapon's parry line, the
//   ideal (square to its blade and its way) through your line's nearest point, the gap between the two lines
//   (debugdirparry), then the reading's note: its verdict (a strike's: parried, partial, miss; or a live reading during
//   its strike, or its blow held back for the window), where they met and its numbers (debugdirparrynote).
// - Every server frame, each held melee weapon's line as the parry reads it (QC VR_AG_WeaponLine, both ends) and last
//   frame's, the sweep between them (debugdirparryline), with the drawn weapon's own butt and tip (its model's, by the
//   renderer's matrix) as dots: on the line's ends when the line is right (vr_dirparry_linecheck: the worst gap).
// - draw (with the view's debug draws; flat: vr_ragtool.cpp's overlay) draws the live reading, and the last strike's
//   verdict held a few seconds with a readout (score against the threshold as a bar, the angle and the contact with
//   their limits and marks, the hand, when, the swing's speed) facing you. A strike's verdict is also printed.
// - Debug only: nothing here changes the game; nothing is kept or drawn while off.
#pragma once

#include "vr_engine.hpp"

#include <glm/vec3.hpp>

namespace qvr::dirparryviz
{

void PF_debugdirparry();     // debugdirparry(kind, a, b): a line of the reading being sent
// float debugdirparryline(w, a, b, a0, b0): held melee weapon w's line as the parry reads it this server frame (QC
// VR_AG_WeaponLine: back a, front b) and last frame's; drawn while fresh. Returns how far (units) the line's ends are
// from the drawn weapon's butt and tip (its model's own, view::weaponLocalGeometry, by the renderer's last matrix for
// it: drawnMatrix), the worse of the two; -1 not measured (off, not drawn lately).
void PF_debugdirparryline();
void drawnMatrix(const entity_t* e, const float* matrix); // VR_AliasPostTransform: a tracked weapon's matrix kept
void registerCommands(); // vr_dirparry_linecheck [reset]: the worst line-end distances measured (and reset)
void PF_debugdirparrynote(); // float debugdirparrynote(verdict, at, score, angle, contact, more): the reading ends
void draw(const glm::vec3& eye); // the view's debug draws
[[nodiscard]] bool on();         // vr_dirparry_debug (the flat view's overlay draws the lines then)

} // namespace qvr::dirparryviz
