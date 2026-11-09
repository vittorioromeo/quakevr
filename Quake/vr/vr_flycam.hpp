// vr_flycam.hpp -- flatscreen play's fly camera (vr_flat_flycam; ROUND24.md, "Flat-screen fly camera").
//
// A toggle (vr_flat_flycam_toggle, the key vr_flat_key_flycam: F8) takes the drawn view out of his eyes: the mouse turns
// it, W A S D fly it, Space (+jump, +moveup) up, Ctrl (+movedown) down, Shift faster (vr_flat_flycam_speed, _accel, _fast),
// through walls (no collision). He stays exactly where and as he was: his angles never change (CL_AccumulateCmd hands
// the look back, VR_FlatFlycamLook), his move is zero (VR_AdjustMove), no button or impulse is sent (CL_SendMove), the
// mode's own keys are not read (flatplay::frame), the hands the server is sent are the ones of the move before
// (vr_client.cpp poseMove), and the drawn hands are posed from his own eye, not the camera's (flatplay eyeOrg/eyeAngles).
// The body is seen from outside as any view off his eyes sees it (the head drawn: vr_avatar.cpp's `away`).
// vr_flat_flycam_pause 1 (the default) holds the game while it flies -- only a game this client's own server runs for
// one player (VR_RuntimeMenuPause: no physics, no player think); multiplayer and demos run on. Leaving puts the view back
// in his eyes; nothing of his was touched. A headset ignores it.

#pragma once

#include <glm/vec3.hpp>

namespace qvr::flycam
{

// flatplay::init: vr_flat_flycam_toggle, vr_flat_flycam_test.
void init();

// flatplay::frame, every frame (the mode on or not): the key, the cvar, leaving when the mode or the level goes, the
// flight, the running test.
void frame(bool flatOn);

// The camera flies (and so has his keys, his mouse and his buttons).
[[nodiscard]] bool on();
// Tests: the camera on (flatscreen play, in a game), at `pos` looking at `target` (no roll). False: not now.
bool lookAt(const glm::vec3& pos, const glm::vec3& target);

// The game held for it (vr_flat_flycam_pause, and a single-player game this client's server runs).
[[nodiscard]] bool holds();

// His own eye as V_CalcRefdef made it this frame, before the camera took the view: r_refdef's while the camera is off.
// What the drawn hands are posed from (flatplay viewIsHisOwn, bodyRide, drawnOffset).
[[nodiscard]] glm::vec3 eyeOrg();
[[nodiscard]] glm::vec3 eyeAngles();

} // namespace qvr::flycam
