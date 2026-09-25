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

// Once per frame, before the controller buttons become keys: where each hand points on the panel;
// the pointing hand's spot is the menu's mouse.
void update(const hands::State& s);

// A trigger press or release (`key` the key it would send): K_MOUSE1 instead while it points at the
// menu (and for the release of such a press). A press also makes that hand the pointing one.
[[nodiscard]] int triggerKey(int hand, bool down, int key);

// Draws the laser and its spot on the panel, in the eye being rendered (after the panel).
void drawInEye(const hands::State& s);

} // namespace qvr::menuui
