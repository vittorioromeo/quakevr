// vr_tips.hpp -- tips for new players (vr_tips): each shown once, the first time you come near what it is about and can
// see it (vr_tips_distance, vr_tips_view_angle, vr_tips_line_of_sight, for vr_tips_delay seconds). Shown as a panel
// floating by it with a line to it (vr_tips 1: over the scene, as the motion recorder's notice), or as one of the wrist
// gadget's hologram messages (vr_tips 2: it waits there until you look at the gadget, which chimes and buzzes your
// hand meanwhile; the panel when there is no hologram). The tips shown are kept in vr_tips_seen (the config), emptied by
// vr_tips_reset (VR Settings > Tips > Show Tips Again).
//
// The tips (vr_tips.cpp, `tips`): a wall torch on its wall (vr_walltorch.cpp: it can be taken and sets enemies on fire).
// A new one: its name, text and what it is about (a client entity's test); a tip for an action done the first time
// would call show() from where the action is noticed.

#pragma once

namespace qvr::tips
{

// Once a frame, after the frame's texts are cleared (VR_BeginFrame): a tip due shown, the one showing laid out.
void frame();

// vr_tips_reset: every tip as never shown (and the one showing gone).
void reset_f();

// vr_tips_test [name]: a tip shown now (the first one by default) on the nearest of what it is about in view, however
// far, as vr_tips shows it, without counting it as shown: to try the two ways and the settings.
void test_f();

} // namespace qvr::tips
