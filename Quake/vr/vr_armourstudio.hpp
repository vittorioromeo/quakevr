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

} // namespace qvr::armourstudio
