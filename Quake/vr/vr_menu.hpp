// vr_menu.hpp -- the VR Settings pages (Options > VR Settings); the engine's menu calls
// VR_Menu_Open/Draw/Key (vr_api.h).

#pragma once

namespace qvr::menu
{

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
