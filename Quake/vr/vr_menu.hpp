// vr_menu.hpp -- the VR Settings pages (Options > VR Settings); the engine's menu calls
// VR_Menu_Open/Draw/Key (vr_api.h).

#pragma once

#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"

namespace qvr::menu
{

// The VR menus that can be open at once, each with its own page, Back stack, selection and what is under way
// (vr_menu.cpp MenuInstance; MULTI_MENU.md). The first is the menu as ever (Quake's menus and the VR pages); the others
// (New Panel, the corner's button) show VR pages only, each on a panel of its own in the headset. The menu code works on
// the current one: the menu's entry points (draw, keys, mouse) act on it, and selectInstance switches (before drawing or
// feeding another), swapping Quake's menu state (m_state) and the menus' mouse with it: the current one is the focused
// panel (the last pointed at), which the keys and the sticks drive.
inline constexpr int instanceCount = 3;
[[nodiscard]] int currentInstance();
void selectInstance(int instance);
[[nodiscard]] bool instanceOpen(int instance); // (the first: while the menu is open)
[[nodiscard]] int openInstances();             // the panels open (0: the menu closed)
// Quake's menu shown on the first panel (m_state's value there; m_none: the menu closed).
[[nodiscard]] int mainState();
// Another panel opened (New Panel), on the page the current one shows (the VR Settings when that is not a VR page or a
// page for one panel at a time); false with a console line when it cannot (the most open, not in the headset).
bool openInstance();
// A panel other than the first closed (its Close Panel button, Back from its first page); the first focused if it was.
void closeInstance(int instance);
// The panels other than the first closed, the first focused (Back to game, the menu closing any way).
void closeExtraInstances();
// Once a frame and before drawing: the panels put right after the menu closed, or a panel other than the first left the
// VR pages for one of Quake's menus (that opens on the first panel).
void tidyInstances();
// Draws panel `instance`'s menu into the canvas being drawn (vr_panel.cpp, its own canvas), the current one kept.
void drawInstance(int instance);
// vr_menu_panels [open | close <n> | focus <n>]: the panels open (MENUPANELS lines), or one opened, closed or focused.
void panels_f();

// The path to a VR Settings page from Quake's main menu, the fewest links from the VR Settings or the Advanced VR Options
// by the labels the player reads on the way ("Options > VR Settings > ...", "Advanced VR > Movement > Locomotion"; the
// main menu's Advanced VR row and the corner's button open those); `spec` is the page's title,
// or "<title> > <row label>" for a row on it. False when no page has that title, no link reaches it, or it has no row of
// that label (the menus changed: the calibration room's boards, vr_setup.hpp).
bool pathTo(za::StringView spec, za::String& out);
// `text` with each {menu:<spec>} replaced by pathTo's path, broken into lines of about `width` characters at its " > "s
// (the maps' text boards: vr_worldtext.cpp). A spec not found is shown as "[menu? <spec>]", warned about in the console
// (MENU PATH MISSING) and counted in `missing`.
[[nodiscard]] za::String expandPaths(za::StringView text, int width, int* missing);
// vr_menu_path_check [file or text]: every {menu:...} in the loaded map's entities, a file or the text given; "menu paths: N
// found, M missing".
void pathCheck_f();
// Every setting's row on the pages under the VR Settings and the Advanced VR Options, whatever Menu Detail shows (Search's
// index, built afresh): its cvar, the pages above it ("Advanced VR Options > Combat") and its label, nearest the roots
// first (a setting on two pages: twice). vr_propose_defaults (vr_propose_defaults.cpp).
void forEachSettingPath(za::FunctionRef<void(const struct cvar_s& var, const char* page, const char* label)> f);
// vr_menu_dump [level]: the pages' texts as MD| console lines (the copy review).
void dump_f();
// vr_menu_press <page title> > <row label>: that button pressed (its command run, its function called), whatever Menu
// Detail shows; the tests' (Misc/quakevr/ragdoll/ragtest_test.sh: each Ragdoll Behaviours button).
void press_f();
// The Body Calibration page's number (menu::reopen).
[[nodiscard]] int bodyCalibrationPage();
[[nodiscard]] int retroOverridePage(); // Graphics > Retro Textures > Override (vr_retro.cpp's picks)

// menu_vr [page [row]]: the VR Settings, or one of its pages (1: Advanced VR Options); menu_vr list: the
// pages; menu_vr pos: the menu shown, and on a VR page its selected row and scroll (tests).
void command_f();

// vr_handcal_match: Hand/Gun Calibration > Match Controller Preview.
void handCalMatch_f();

// The page shown (while m_state is m_vr), and the VR Settings reopened at one (its selection,
// scroll and way back as they were left).
[[nodiscard]] int currentPage();
// The toolgun panel's cheats (vr_menu_toolgun.inc; vr_toolgun_panel.cpp's More): Debug > Cheats and Recording's rows
// worth having at hand, by their labels: how many, each one's short name, whether it is on (a switch's, god mode's...),
// and one used (its command, its action, or its switch flipped), as the row's click would.
[[nodiscard]] int toolgunCheatCount();
[[nodiscard]] const char* toolgunCheatName(int i);
[[nodiscard]] bool toolgunCheatOn(int i);
void toolgunCheatUse(int i);
void reopen(int page);

// "Advanced VR" (the corner's button, the main menu's row): the Advanced VR Options from any menu, their selection and
// scroll as they were left; Back from them goes to the VR Settings (to the main menu when opened from its row).
void jumpToAdvanced();

// "Checklist" (the corner's button): the Checklist page (vr_checklist.hpp) from any menu, the list read again; Back
// from it goes to the VR Settings.
void jumpToChecklist();

// "VR Settings" (the corner's button): the VR Settings page from any menu (a tick on it already).
void jumpToSettings();

// "Main Menu" (the corner's button, under Back to game): Quake's main menu from any menu.
void jumpToMainMenu();

// "Relighting" (the corner's button): Graphics > Relighting from any menu (Menu Detail raised to Advanced if lower);
// Back from it goes up the tree: Graphics, Advanced VR Options, VR Settings.
void jumpToRelighting();

// The ragdoll tool's page (Debug > Ragdoll Tool; Tab in the tool: vr_ragtool.cpp), the menu opened on it if closed.
void jumpToRagtool();

// The presets' callbacks (Locomotion's Comfort, Body and Display's Handedness) and VR Settings' own rows (Turning Mode, Move Towards, the hands), once the cvars are registered.
void init();

// The leftmost x (menu pixels) the menu shown draws text at: a VR page's longest label (right-aligned to the values'
// column, reaching left of Quake's 320 columns) or its help; Ironwail's lists laid out from the canvas's width; else 0.
// The corner's buttons stand left of it (vr_menuui.cpp).
[[nodiscard]] float contentLeft();

// The rightmost x (menu pixels) the menu shown draws at from menu y `y` down (-1e9: nothing there): a VR page's rows and
// their scrollbar, its help box at its widest, Search's, the Map Library's and the console's keyboards; Quake's and
// Ironwail's menus as menu.c's M_ContentRightBelow says (the main menu's rows as drawn, the others M_ContentExtent's).
// The corner's version label and update notice keep right of it (vr_menubrand.cpp).
[[nodiscard]] float contentRightBelow(float y);
// And the leftmost (1e9: nothing there): a flat screen's Recording button keeps left of it (vr_menuui.cpp).
[[nodiscard]] float contentLeftBelow(float y);

// "Recording" (the corner's button, bottom left): Graphics > Recording (the desktop window's view, the spectator camera,
// OBS) from any menu, at any Menu Detail (VR Settings links to it at Standard too); Back returns where it was pressed.
void jumpToRecording();

// Menu Detail at Developer (vr_menu_level 2): the tuning and testing pages, the corner's Checklist button.
[[nodiscard]] bool developerLevel();

// Search (the corner's button): the Search page, from any menu (vr_menu_search.inc).
void openSearch();
void search_f(); // vr_menu_search <text>
void sliderStep_f(); // vr_menu_slider_step <cvar> <steps> [fine]: steps a slider as the menu's keys do (tests)

// Console (the corner's button): Quake's console with a keyboard, from any menu (vr_menu_console.inc).
void openConsole();

// Map Library (the corner's Maps button, and Single Player > Map Library): the map browser page (vr_menu_maps.inc).
void openMaps();
void mapsPage_f(); // maps_page [text]: the Map Library page, with the text typed in
void mapsPageStats_f(); // maps_page_stats: what the page's list costs, and its layout (Debug > Tools)

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
// Weapon Offsets > Flashlight: while it is shown (Show the Torch's Place on), the hand whose weapon the page edits (still
// holding it): the flashlight draws the torch's place on it (vr_flashlight.cpp drawMountPreview); false otherwise.
[[nodiscard]] bool flashlightMountPreview(int& hand);

// The body's mirror image (vr_bodyclone.cpp): a VR page that tweaks something worn on the body is shown (Flashlight - On a
// Gun or Head, Weapon Offsets - Holstered), or a row of such a setting has been chosen on the page shown (the holsters',
// the pouches', the flashlight's place on the body and its head zones, the wrist gadget's mount, the virtual stock's, the
// pauldrons'): kept while that page stays. vr_menu_body_clone 2: any VR page.
[[nodiscard]] bool bodyClonePage();

} // namespace qvr::menu
