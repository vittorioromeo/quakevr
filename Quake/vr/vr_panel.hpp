// vr_panel.hpp -- the 2D layer as a panel in the headset.

#pragma once

#include "vr_hands.hpp"

namespace qvr::panel
{

// The eyes were rendered this frame: the 2D pass goes to the canvas.
void setStereoThisFrame(bool stereo);

// Draws the (previous frame's) 2D canvas in the eye being rendered, if a menu is open.
void drawInEye(const hands::State& s);

// The menu panel's quad in the world while it is shown in the eyes: its corner (the canvas's
// bottom left) and the axes spanning the canvas's width and height (up). False when it is not.
[[nodiscard]] bool menuQuad(const hands::State& s, glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis);

} // namespace qvr::panel
