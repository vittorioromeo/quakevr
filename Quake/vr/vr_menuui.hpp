// vr_menuui.hpp -- the VR menu style: Quake's menus made for a headset by the primitives that draw
// them (a bigger canvas with spaced-out rows, modern sliders, switches and boxes, a highlighted
// row), and a laser pointer that drives Ironwail's menu mouse support.

#pragma once

#include "vr_hands.hpp"

namespace qvr::menuui
{

// Where a flat screen's banner (vr_menubrand.cpp, M_DrawPlaque) was last drawn, menu x and y (x1 < x0: not drawn).
void bannerRect(float& x0, float& x1, float& y0, float& y1);
// menu_vr pos: the version label (vr_menubrand.cpp, VR_MenuDrawVersion) as last drawn: its text, its box (menu x and y)
// and what the menu draws right to under it, or why it was left out.
void printVersionLabel();
// Where the version label is on (vr_menu_version): the menus keep left of x or above y (menu x and y) to stay clear of it
// (a VR page whose rows or help reach under it ends above it: vr_menu.cpp, layout).
[[nodiscard]] bool versionLabelClearance(float& x, float& y);
// Opens Ironwail's menu `state` (m_main, m_singleplayer, m_options...) as its own way in does; m_none closes the menu.
void openMenu(int state);
// A menu is open in the headset with vr_menu_vr_style on.
[[nodiscard]] bool active();

// The menu panel's height in world units while the style is active (0 otherwise): one menu pixel
// (a character is 8) is vr_menu_scale units, as in the old engine, whatever the canvas's size.
[[nodiscard]] float panelHeight();

// The menus' height in menu pixels: Quake's 200, or with the style active, 200 times
// vr_menu_height (whole rows), the 320 x 200 of Quake's layout in its middle. The VR pages lay
// out their list and help in it.
[[nodiscard]] int menuHeight();

// Once per frame, before the controller buttons become keys: where each hand points on the panel;
// the pointing hand's spot is the menu's mouse.
void update(const hands::State& s);

// A trigger press or release (`key` the key it would send): K_MOUSE1 instead while it points at the
// menu (and for the release of such a press). A press also makes that hand the pointing one.
[[nodiscard]] int triggerKey(int hand, bool down, int key);

// Draws the laser and its spot on the panel, in the eye being rendered (after the panel), and the spectator camera's
// preview (vr_spectator_preview) under it (not in the camera's own view).
void drawInEye(const hands::State& s);

// Whether a menu in the headset shows the spectator camera's preview (vr_spectator_preview, while the camera is on;
// placed by the last 2D pass): vr_stereo.cpp then makes it from the camera's image.
[[nodiscard]] bool spectatorPreviewWanted();

// "Back to game": closes the menu from whatever page it is on, remembering that page for the next
// time it opens (vr_menu_remember), with a pulse in `hand`. The panel's top-left button, or the
// menu button held.
void backToGame(int hand);

// Whether the corner's buttons are over the menu: with the VR menu style in the headset, and on a flat screen (VR off)
// with vr_menu_flat_shortcuts (the desktop mouse clicks them).
[[nodiscard]] bool toolbarShown();

// The corner's buttons under "Back to game" ("Advanced VR", "Levels", "Checklist"; vr_menuui.cpp): the column's
// bottom (menu y; far above the menu when they are not shown), where the menus' rows start at the
// latest; whether the sticks' selection is on them (the menu's own cursor then hidden); and the
// selection moved onto them from a VR page's end (dir 1: down, onto the top one; -1: up, onto the
// bottom one).
[[nodiscard]] float toolbarBottom();
[[nodiscard]] float toolbarRight(); // the buttons' right edge (menu x; far left when they are not shown)
[[nodiscard]] float toolbarLeft();  // and their left edge (menu x; far left when they are not shown)
[[nodiscard]] bool toolbarFocused();
[[nodiscard]] bool toolbarRow(); // on a flat screen: a row of icons along the canvas's top
// In the headset: whether the column (its buttons, the banner under them, the spectator switch) stands clear left of
// what the menu draws, so that the menu's rows start at its top rather than below the buttons; and the nearest the
// column comes to the menu (menu x: the menu's leftmost text, less a gap).
[[nodiscard]] bool toolbarBeside();
[[nodiscard]] float toolbarLimit();
// The status box's bottom (menu y) where it is over a menu reaching right to `contentRight` (menu x); far above the
// menu where it is not (or is off).
[[nodiscard]] float statusBottom(float contentRight);
void focusToolbar(int dir);

// vr_mock_laser <x> <y> | back | search | console | advanced | levels | maps | checklist | spectator | off (tests): the
// main hand's laser on a spot of the menu, on one of the corner's buttons or on the spectator camera's switch (bottom
// left), whatever the hand's pose.
void mockLaser_f();

// vr_mock_mouse <x> <y> | <button> [click] (tests, flat screen): the desktop mouse moved to a spot of the menu (menu
// coordinates) or onto one of the corner's buttons (as vr_mock_laser names them), as the window's mouse motion moves
// it (M_Mousemove), and with `click` a left click there (K_MOUSE1 pressed and released).
void mockMouse_f();

// vr_mock_key <key> (tests): that key pressed and released (as Key_Event; its name as bind takes it), then the menu and
// the corner button the keys selected (-1: none).
void mockKey_f();

// menu_vr pos: where the pointing hand's laser meets the menu (on which panel, its uv).
void printLaser();

// The main hand's stick (up and down, `y`) in a menu, once a frame: scrolls a page with a
// scrollbar, a row at a time at a rate growing with the push. False (nothing done) when the
// page does not scroll: the stick navigates there.
bool scrollStick(float y);

// A VR page's slider in the VR menu style (as VR_MenuDrawSlider): `range` its value's share of the
// bar, `past` -1 or 1 when the value lies beyond the bar's left or right end (the thumb stays at
// that end, a lighter colour, an arrow outside it pointing on). False (nothing drawn) without the
// style: Quake's slider is drawn instead.
bool drawSlider(int x, int y, float range, int past, const char* desc);

// A progress bar on the row at y, from x0 to x1, filled to `fraction` (0..1): a track and its fill, as the sliders'.
// False (nothing drawn) without the style: the menu draws Quake's (vr_menu.cpp, progressBar).
bool drawProgress(int x0, int x1, int y, float fraction);

// An open drop-down list's highlighted choice (vr_menu.cpp): a bar from x0 to x1 on the row at y, in either menu style.
void drawListHighlight(float x0, float x1, int y);

} // namespace qvr::menuui
