// vr_panel.hpp -- the 2D layer as a panel in the headset.

#pragma once

#include "vr_hands.hpp"

namespace qvr::panel
{

// The eyes were rendered this frame: the 2D pass goes to the canvas.
void setStereoThisFrame(bool stereo);

// The headset's eye image being rendered: its pixels per unit of tangent up the view (its projection's [1][1] times half
// its height). The canvas is sized for it (vr_menu_resolution).
void noteEyeImage(float pixelsPerTan);

// Draws the (previous frame's) 2D canvas in the eye being rendered: the menu's panel while one is open; in game, the
// hand's status bar and, with `headText`, the head-locked text (centre prints, notify lines: off for the spectator
// camera's and the mirror's UI with vr_spectator_hide_hud_text, vr_mirror_hide_hud_text).
void drawInEye(const hands::State& s, bool headText = true);

// The menu panel's quad in the world while it is shown in the eyes: its corner (the canvas's
// bottom left) and the axes spanning the canvas's width and height (up). False when it is not.
[[nodiscard]] bool menuQuad(const hands::State& s, glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis);
// Panel `instance`'s (menu::instanceCount; MULTI_MENU.md): the others' only while open and shown in the eyes.
[[nodiscard]] bool menuQuad(const hands::State& s, int instance, glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis);

// The menus are drawn into the canvas going to the eyes this frame (the panels other than the first can be shown).
[[nodiscard]] bool menusInEyes();
// Panel `instance` (not the first) opened: beside the first (right, then left), or where it was last this session
// (vr_menu_panel_remember: its place from the first's); the first then stays where it is (placed) so that they keep
// side by side. Closed: its place from the first's noted, its canvas freed.
void openMenuPanel(int instance);
void closeMenuPanel(int instance);

// The current menu's panel (menu::currentInstance) moved by a hand (vr_menuui.cpp's grab): its middle and unit axes
// (right, up) in the world, kept in the play space (it turns and walks with the player) until the panel closes or
// unplaceMenu puts it back in front of the head. menuQuad is the current menu's too.
void placeMenu(const hands::State& s, const glm::vec3& centre, const glm::vec3& right, const glm::vec3& up);
void unplaceMenu();
[[nodiscard]] bool menuPlaced();

// The HUD is Quake's status bar on a hand (vr_hud_mode 0, or dead: body::gearHiddenForDeath), not a CSQC HUD.
[[nodiscard]] bool statusBarOnHand();

} // namespace qvr::panel
