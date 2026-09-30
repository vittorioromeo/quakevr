// vr_menu.hpp -- the VR Settings pages (Options > VR Settings); the engine's menu calls
// VR_Menu_Open/Draw/Key (vr_api.h).

#pragma once

#include <string>
#include <string_view>

namespace qvr::menu
{

// The path to a VR Settings page from Quake's main menu, the fewest links from the VR Settings by the labels the player
// reads on the way ("Options > VR Settings > Advanced VR Options > Movement > Locomotion"); `spec` is the page's title,
// or "<title> > <row label>" for a row on it. False when no page has that title, no link reaches it, or it has no row of
// that label (the menus changed: the calibration room's boards, vr_setup.hpp).
bool pathTo(std::string_view spec, std::string& out);
// `text` with each {menu:<spec>} replaced by pathTo's path, broken into lines of about `width` characters at its " > "s
// (the maps' text boards: vr_worldtext.cpp). A spec not found is shown as "[menu? <spec>]", warned about in the console
// (MENU PATH MISSING) and counted in `missing`.
[[nodiscard]] std::string expandPaths(std::string_view text, int width, int* missing);
// vr_menu_path_check [file or text]: every {menu:...} in the loaded map's entities, a file or the text given; "menu paths: N
// found, M missing".
void pathCheck_f();
// The Body Calibration page's number (menu::reopen).
[[nodiscard]] int bodyCalibrationPage();

// menu_vr [page [row]]: the VR Settings, or one of its pages (1: Advanced VR Options); menu_vr list: the
// pages; menu_vr pos: the menu shown, and on a VR page its selected row and scroll (tests).
void command_f();

// vr_handcal_match: Hand/Gun Calibration > Match Controller Preview.
void handCalMatch_f();

// The page shown (while m_state is m_vr), and the VR Settings reopened at one (its selection,
// scroll and way back as they were left).
[[nodiscard]] int currentPage();
void reopen(int page);

// "Advanced VR" (the corner's button): the Advanced VR Options from any menu, their selection and
// scroll as they were left; Back from them goes to the VR Settings, as always.
void jumpToAdvanced();

// "Checklist" (the corner's button): the Checklist page (vr_checklist.hpp) from any menu, the list read again; Back
// from it goes to the VR Settings.
void jumpToChecklist();

// The sticks' selection back on the page shown from the corner's buttons: onto its first setting
// (dir 1, going down) or its last (dir -1, going up).
void selectEnd(int dir);

// Scrolls the page shown by `rows` (the selection kept in view, moved only if it would leave it);
// false, doing nothing, when the page fits (or a slider or the scrollbar is held). 0: only asks.
bool scroll(int rows);

// The setting selected on the page shown (null when the VR Settings are not shown, or the selected
// row is not a setting): effects preview themselves while their settings are chosen.
[[nodiscard]] const struct cvar_s* selectedSetting();
// Weapon Offsets > Holstered: while one of its settings is chosen (Preview in Holster on), the hand whose weapon the page
// edits and the kind of holster (weapons::HolsterKind: 0 hips, 1 chest, 2 back) it is to be drawn in; false otherwise.
[[nodiscard]] bool holsterPreview(int& hand, int& kind);

} // namespace qvr::menu
