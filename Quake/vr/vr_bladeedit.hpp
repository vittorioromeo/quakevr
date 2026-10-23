// vr_bladeedit.hpp -- the blade shape editor (Weapon Offsets > Posing Mode > Edit Blade Shape; Debug > Views; ROUND22.md,
// "Blade shape editor"): the weapon in the hand's melee blade (vr_blade.hpp: what its blows strike with) shaped by hand,
// live. Show Melee Blades is on while it runs (QC's own blow line and sweeps, as they strike); over it the blade as its
// settings lay it (its line from the near end through the hand along its curve to the tip, its width as a tube) and its
// handles, which the other hand's grip takes and drags: the tip (Muzzle X/Y/Z), the near end (Blade Near End), the width
// (Blade Width), five points of the curve (Blade Spine: a smooth spline through them; the first drag seeds them from the
// model's curve) and a head's two corners (Blade Head; when it has one). A panel ahead (touch a button, pull the trigger):
// Undo, Reset Blade, Straighten, Head Box On/Off, Cancel (everything back as it was) and Done (vr_blade_dump's lines
// printed). B/Y undoes, the menu button is Done. The game goes on (the hands strike as they would): the buttons are the
// editor's, the sticks don't move the player.

#pragma once

#include "vr_engine.hpp"
#include "vr_posing.hpp"

namespace qvr::bladeedit
{

void init(); // the commands

[[nodiscard]] bool active();

// Enters it on the weapon `hand` holds (HAND_MAIN 1, HAND_OFF 0); `returnPage`: the VR Settings page reopened on leaving
// (-1: none). False (said why) if it can't.
bool start(int hand, int returnPage);
// Leaves it: `keep` (Done) keeps every change, else (Cancel) puts back every setting as it was on entering.
void finish(bool keep);

// The controllers (vr_input.cpp): a press or release (true: taken, not a key), and the sticks each frame.
bool button(int hand, posing::Button b, bool down);
void sticks(const glm::vec2& off, const glm::vec2& main);

// Once a frame (VR_BeginFrame, after the texts are cleared): a held handle follows its hand; the blade, handles and panel
// queued; leaving when the game does.
void frame();

// Handle `index` (0 the tip, 1 the near end, 2 the width, 3..7 the curve's points, 8 and 9 the head's low and high
// corners), `offset` units off it along the line, its normal in the blade's plane and the model's +y; and the panel's
// button `index` (0 Undo, 1 Reset Blade, 2 Straighten, 3 Head Box, 4 Cancel, 5 Done): where they are, for the mock hands
// (vr_mock_hand_to <hand> bladehandle|bladebutton <index>). False: not active, not shown.
[[nodiscard]] bool handlePoint(int index, const glm::vec3& offset, glm::vec3& out);
[[nodiscard]] bool buttonPoint(int index, glm::vec3& out);

} // namespace qvr::bladeedit
