// vr_setup.hpp -- VR Calibration: the first-time setup (the main menu's first row, "VR CALIBRATION", confirmed). It takes
// the player to the calibration room (maps/vrcalibration.bsp, made by Misc/quakevr/make_vrcalibration_map.py) and, a few
// seconds after the player appears, calibrates step by step, the instructions floating in front of the eyes:
//
//  0. A welcome in the middle of the view for a few seconds (a start; not the restart after the menu).
//  1. Height: standing tall (sitting up straight, with Position: Seated) and still: vr_height_calibration, as VR Settings >
//     Set Height Now.
//  2. Body: Body Calibration's poses (vr_bodycal.hpp; seated without its first), applied at once when the poses agree;
//     otherwise its page opens on the result (redo a pose, Apply or Cancel) and the setup goes on when the menu closes.
// (No main hand step: there is no main hand setting.)
//
// Each result is printed (the wrist gadget's log) and the config saved; a summary shows at the end. The room is
// calibration only (no buttons): the menu button pauses the setup on Body Calibration's page (its Position: standing or
// seated) and it starts over when the menu closes; the main menu's VR Calibration row runs it again later. A glowing
// doorway behind the player leads to the vrstart hub, or the tutorial until it has been started once (a first start:
// QC changelevel_touch); the board over it and the summary say which. Elsewhere (`vr_setup here`) the menu button stops
// it. From the first frame its stickman (Body Calibration's ghost) shows, standing, it stays to the summary.

//
// A map's func_button with buttonEffect 3 and targetname "vr_setup_option <key>\n" (the test hall's wall buttons:
// maps/vrtesthall.bsp) steps one of the main settings through its presets (the table in vr_setup.cpp: turning,
// locomotion, the sticks, the gadget's arm, the torch's hip, world scale, the body, the HUD...), prints the new value and
// saves the config; each such button's value is drawn on a small screen above it.
// The room's board says where each calibration is in the menus: its text names the pages ({menu:<page title>},
// menu::expandPaths), so the paths follow the menus, and a page or row that no longer exists shows and warns
// (vr_menu_path_check).

#pragma once

namespace qvr::setup
{

void init(); // the commands

// Once a frame (vr_main.cpp, after the texts are cleared and Body Calibration's frame): the steps, their text, the
// buttons' value screens.
void frame();

// Whether the setup is running (a step or its summary shown).
[[nodiscard]] bool running();

// The calibration room's doorway, on the board over it ("{calibration exit}" in a board's text, vr_worldtext.cpp): "VR
// TUTORIAL" at a first start (vr_tutorial_started 0: QC changelevel_touch sends it there), else "VR HUB".
[[nodiscard]] const char* exitBoardName();

} // namespace qvr::setup
