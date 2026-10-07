// vr_panel.hpp -- the 2D layer as a panel in the headset.

#pragma once

#include "vr_hands.hpp"

namespace qvr::panel
{

// The eyes were rendered this frame: the 2D pass goes to the canvas.
void setStereoThisFrame(bool stereo);

// Draws the (previous frame's) 2D canvas in the eye being rendered: the menu's panel while one is open; in game, the
// hand's status bar and, with `headText`, the head-locked text (centre prints, notify lines: off for the spectator
// camera's and the mirror's UI with vr_spectator_hide_hud_text, vr_mirror_hide_hud_text).
void drawInEye(const hands::State& s, bool headText = true);

// The menu panel's quad in the world while it is shown in the eyes: its corner (the canvas's
// bottom left) and the axes spanning the canvas's width and height (up). False when it is not.
[[nodiscard]] bool menuQuad(const hands::State& s, glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis);

// The HUD is Quake's status bar on a hand (vr_hud_mode 0, or dead: body::gearHiddenForDeath), not a CSQC HUD.
[[nodiscard]] bool statusBarOnHand();

} // namespace qvr::panel
