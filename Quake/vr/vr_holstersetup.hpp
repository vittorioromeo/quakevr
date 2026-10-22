// vr_holstersetup.hpp -- Holster Setup (VR Settings > Holsters > Holster Setup; ROUND22.md, "Holster Setup: drag and
// drop"): the player-facing way to place the holsters and pouches. The game pauses (single player only), every holster's
// and pouch's zone is drawn as a labelled see-through box over it, and a grip inside one takes hold of it: the hand's moves
// move it (its zone and its model together, being where its offsets put it: the pair's offsets, vr_hip_offset_* and so
// on, or the seated ones while seated, with Symmetry on; its own trim, vr_holster_trim_<holster>_*, with Symmetry off; a
// pouch's own offsets), both hands on it resize it (vr_holster_size_<zone>), as does the holding hand's stick. A panel
// ahead (touch a button, pull the trigger): Symmetry, Undo, Reset Holster, Reset All, Cancel (everything back as it
// was on entering) and Done. B/Y undoes, the menu button is Done. Everything is the cvars themselves, live: the sliders
// show it.

#pragma once

#include "vr_engine.hpp"
#include "vr_posing.hpp"

namespace qvr::holstersetup
{

void init(); // the commands

[[nodiscard]] bool active();
// The game paused for it (host.c and sv_user.c through VR_RuntimeMenuPause): while active (single player only).
[[nodiscard]] bool pausesGame();

// Enters it; `returnPage`: the VR Settings page reopened on leaving (-1: none). False (said why) if it can't.
bool start(int returnPage);
// Leaves it: `keep` (Done) keeps every change, else (Cancel) puts back every setting as it was on entering.
void finish(bool keep);

// The controllers (vr_input.cpp): a press or release (true: taken, not a key), and the sticks each frame.
bool button(int hand, posing::Button b, bool down);
void sticks(const glm::vec2& off, const glm::vec2& main);

// Once a frame (VR_BeginFrame, after the texts are cleared): a held holster follows its hands; the boxes, labels and
// panel queued; leaving when the game does.
void frame();

// Zone `zone` (zones::Zone) is hovered or held: its models lit up (vr_view.cpp).
[[nodiscard]] bool highlighted(int zone);

// The panel's button `index` (0 Symmetry, 1 Undo, 2 Reset Holster, 3 Reset All, 4 Cancel, 5 Done): its middle, for the
// mock hands (vr_mock_hand_to <hand> setupbutton <index>). False: not active, no such button.
[[nodiscard]] bool buttonPoint(int index, glm::vec3& out);

} // namespace qvr::holstersetup
