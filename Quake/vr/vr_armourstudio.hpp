#pragma once

// vr_armourstudio.hpp -- the Armour Studio (Debug > Tools > Armour Studio; docs/vr-port/ARMOUR_STUDIO.md): the armour's plates
// tweaked by hand per wearer kind and piece, live, in the headset or on a flat screen (the VR menu's own sliders). The kind
// stands ahead of you (vr_armourstudio: an armed humanoid with its fists, held still, unhurt, in a chosen armour tier, turned as
// you like, its body a ghost) with the piece picked outlined; its values are vr_armourstudio_* cvars the page's sliders move, each
// change laid into the tweaks' table (vr_armourdata.hpp) and the armour made again. Save writes quakevr/data/armour.txt; Undo,
// Reset Part, Reset Kind. The player's kind edits your own body's plates (seen on the menu's body clone or in a mirror).

namespace qvr::armourstudio
{

void init();  // the commands: vr_armourstudio, _close, _save, _undo, _reset_part, _reset_kind; vr_armour_test
void frame(); // the sliders against the table, the subject kept, the test's steps

// The page's status line: what is edited, how many parts are set, whether saved.
[[nodiscard]] const char* status();
// Whether the studio runs (Start given, not closed): its subject kept ahead of you.
[[nodiscard]] bool running();
// Its window closed by the player (its X, Escape, its close command): the studio closed too with vr_armourstudio_autoclose
// (its subject removed, the armed humanoids free again: vr_agrunt_freeze back).
void windowClosed();

// The subject's animations (its model's frames grouped by name: stand, run, pain...; none without a subject), played by
// vr_armourstudio_anim / _anim_group / _anim_play / _anim_speed / _anim_frame; the line saying which and where.
[[nodiscard]] int animCount();
[[nodiscard]] const char* animName(int i);
[[nodiscard]] int animFrames(int i);
[[nodiscard]] const char* animStatus();

// Whether the body's mirror image (vr_bodyclone.cpp) is wanted for the studio: the ImGui window open on the player's kind
// (its plates are your own body's), placed straight ahead of you as no menu panel is there to stand beside.
[[nodiscard]] bool cloneWanted();

// The armour your body (and so its mirror image) wears while the studio shows the player's kind (its ImGui window open, or
// the menu's Armour Studio page, on Kind = Player): the studio's tier (`type` 1 green .. 3 red) and every piece, or the part
// picked alone (`pieces`), whatever armour you have. False: not shown, your own armour as it is.
[[nodiscard]] bool playerArmour(int& type, int& pieces);

// The flat-screen ImGui window (vr_armourstudio_window.cpp; vr_armourstudio_window): the menu page's rows in a floating
// window over play, the same vr_armourstudio_* cvars and the same commands -- no state of its own beyond its place.
void windowInit();     // the commands: vr_armourstudio_window [open|close|toggle|test], vr_armourstudio_window_close
void windowFrame();    // vr_main's client frame, before the other ImGui tools (it closes when one of them opens)
void windowShutdown(); // closed (the input grab given back)
[[nodiscard]] bool windowOpen();

} // namespace qvr::armourstudio
