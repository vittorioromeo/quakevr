// vr_comfortfade.hpp -- the comfort fade: when the game moves the player somewhere else at once (a scripted teleport, not
// one he walks into), the view goes black and fades back in over vr_comfort_teleport_fade seconds, so the jump is not seen
// as a cut. The server's QC stuffs `vr_comfort_fade` to his client (QC VR_ComfortFade: Dawn of the Machine's boss
// teleports, MG3_PLAN.md M3-25). Drawn as the bonus colour shift (the eyes' blend, as vr_shock.cpp's flash), black.
#pragma once

namespace qvr::comfortfade
{
// The view black now, back in over `seconds` (0 or less: nothing).
void start(float seconds);
// Once a frame (the view setup): the black left of a fade.
void frame();
// No fade (a new map, a disconnect).
void clear();
// vr_comfort_fade [seconds]: a fade (vr_comfort_teleport_fade's seconds by default; 0 there: none).
void registerCommands();
} // namespace qvr::comfortfade
