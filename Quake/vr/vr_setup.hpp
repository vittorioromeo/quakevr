// vr_setup.hpp -- VR Calibration: the first-time setup (the main menu's first row, "VR CALIBRATION", confirmed). It takes
// the player to the calibration room (maps/vrcalibration.bsp, made by Misc/quakevr/make_vrcalibration_map.py) and, a few
// seconds after the player appears, calibrates step by step, the instructions floating in front of the eyes:
//
//  1. Height: standing tall (sitting up straight, with Position: Seated) and still: vr_height_calibration, as Body and
//     Display > Set Height Now.
//  2. Body: Body Calibration's poses (vr_bodycal.hpp; seated without its first), applied at once when the poses agree;
//     otherwise its page opens on the result (redo a pose, Apply or Cancel) and the setup goes on when the menu closes.
//  3. Main hand: the hand raised high is the main one (vr_lefthanded).
//
// Each result is printed (the wrist gadget's log) and the config saved; a summary shows at the end. The menu button stops
// it (as Body Calibration's); the room's START CALIBRATION button (`vr_setup here`) runs it again.
//
// The room's wall buttons (func_button with buttonEffect 3 and targetname "vr_setup_option <key>\n") each step one of
// the main settings through its presets (the table in vr_setup.cpp: turning, locomotion, main hand, world scale, the
// body, the HUD...), print the new value and save the config; each button's value is drawn on a small screen above it.
// Its boards say where the rest is in the menus: their texts name the pages ({menu:<page title>}, menu::expandPaths),
// so the paths follow the menus, and a page or row that no longer exists shows and warns (vr_menu_path_check).

#pragma once

namespace qvr::setup
{

void init(); // the commands

// Once a frame (vr_main.cpp, after the texts are cleared and Body Calibration's frame): the steps, their text, the
// buttons' value screens.
void frame();

// Whether the setup is running (a step or its summary shown).
[[nodiscard]] bool running();

} // namespace qvr::setup
