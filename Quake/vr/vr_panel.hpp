// vr_panel.hpp -- the 2D layer as a panel in the headset.

#pragma once

#include "vr_hands.hpp"

namespace qvr::panel
{

// The eyes were rendered this frame: the 2D pass goes to the canvas.
void setStereoThisFrame(bool stereo);

// Draws the (previous frame's) 2D canvas in the eye being rendered, if a menu is open.
void drawInEye(const hands::State& s);

} // namespace qvr::panel
