// vr_weaponstudio.hpp -- the Weapon Studio: one flat-screen window for every per-weapon setting (what the Weapon
// Offsets and Weapon Weights menus reach), the muzzle-and-sights alignment readout, and the held weapon seen as the
// game itself draws it: the player's own (its view model and IK) and an armed humanoid spawned frozen beside the
// player. It is a new reader and writer of the same store the menus use (the vr_wofs_<key>_NN stand-in cvars): the
// menus, the config, the console and the game see every change at once. Flat screen only (start with -flatscreenmode);
// the game's physics is held while it is open. See vr_weaponstudio.cpp and vr_weaponstudio_ui.cpp.

#pragma once

#include "vr_hands.hpp"
#include "vr_posestudio.hpp"

namespace qvr::weaponstudio
{

// The commands: vr_weaponstudio (open, or a subcommand: close, open, slot, preview, align, reset, print, copy, test),
// vr_weaponstudio_close, vr_weaponstudio_slot, vr_weaponstudio_preview, vr_weaponstudio_align, vr_weaponstudio_test;
// and Debug > Tools' row.
void init();
void shutdown(); // (VR_Shutdown: the window closed, its preview subject let go)

// Open, and whether the game's physics is held while it is (vr_main.cpp's VR_RuntimeMenuPause).
[[nodiscard]] bool active();
[[nodiscard]] bool holds();

// The window (vr_main.cpp's client frame, beside the Rig Studio's and the Arena Wave Editor's), and its view-side half:
// the muzzle, sight, shot and grip markers and their readouts, measured from the weapon as the view draws it
// (vr_view.cpp, beside Align Sights to My Aim).
void clientFrame();
void viewFrame(const hands::State& s);

// The shipped armed-humanoid arms solve asked to publish its pose notes to us too (vr_hitreact_skin.cpp): the preview
// humanoid says what it is doing and which weapons' slots its guns draw with.
[[nodiscard]] bool wantsNotes();
void note(const posestudio::Note& n);
} // namespace qvr::weaponstudio
