// vr_menuui.hpp -- the VR menu style: Quake's menus made for a headset by the primitives that draw
// them (a bigger canvas with spaced-out rows, modern sliders, switches and boxes, a highlighted
// row), and a laser pointer that drives Ironwail's menu mouse support.

#pragma once

#include "vr_hands.hpp"

namespace qvr::menuui
{

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

// Draws the laser and its spot on the panel, in the eye being rendered (after the panel).
void drawInEye(const hands::State& s);

// "Back to game": closes the menu from whatever page it is on, remembering that page for the next
// time it opens (vr_menu_remember), with a pulse in `hand`. The panel's top-left button, or the
// menu button held.
void backToGame(int hand);

// The corner's buttons under "Back to game" ("Advanced VR", "Levels", "Checklist"; vr_menuui.cpp): the column's
// bottom (menu y; far above the menu when the style is off), where the menus' rows start at the
// latest; whether the sticks' selection is on them (the menu's own cursor then hidden); and the
// selection moved onto them from a VR page's end (dir 1: down, onto the top one; -1: up, onto the
// bottom one).
[[nodiscard]] float toolbarBottom();
[[nodiscard]] bool toolbarFocused();
void focusToolbar(int dir);

// vr_mock_laser <x> <y> | back | advanced | levels | checklist | off (tests): the main hand's laser on a spot of
// the menu, or on one of the corner's buttons, whatever the hand's pose.
void mockLaser_f();

// The main hand's stick (up and down, `y`) in a menu, once a frame: scrolls a page with a
// scrollbar, a row at a time at a rate growing with the push. False (nothing done) when the
// page does not scroll: the stick navigates there.
bool scrollStick(float y);

// A VR page's slider in the VR menu style (as VR_MenuDrawSlider): `range` its value's share of the
// bar, `past` -1 or 1 when the value lies beyond the bar's left or right end (the thumb stays at
// that end, a lighter colour, an arrow outside it pointing on). False (nothing drawn) without the
// style: Quake's slider is drawn instead.
bool drawSlider(int x, int y, float range, int past, const char* desc);

} // namespace qvr::menuui
