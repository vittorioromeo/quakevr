// vr_input.hpp -- controller input.

#pragma once

#include "vr_backend.hpp"
#include "vr_hands.hpp"

namespace qvr::input
{

void init();

// Once per host frame with the backend's input.
void update(const InputState& in);

// Once per host frame after the hands: a jump in the room jumps in the game (vr_roomscale_jump).
void roomscaleJump(const hands::State& s);

// Whether the hand's upper face button (B on the main hand, Y on the off hand, by role) is held in the game: the
// grappling hook's reel (QVR_BUTTON_*HANDSECONDARY). Not while a menu or the console is up, nor when the posing mode,
// a voice note or the flashlight took the press.
[[nodiscard]] bool secondaryHeld(int hand);

// svc_quakevr QVR_SVC_HAPTIC (vr_client.cpp dispatches it).
void parseHaptic();

} // namespace qvr::input
