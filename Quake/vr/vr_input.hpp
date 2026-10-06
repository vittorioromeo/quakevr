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
// The same for the lower face button (A on the main hand, X on the off hand): the grappling hook's unreel
// (QVR_BUTTON_*HANDPRIMARY). Its key (jump, reload) is pressed as ever.
[[nodiscard]] bool primaryHeld(int hand);

// svc_quakevr QVR_SVC_HAPTIC (vr_client.cpp dispatches it).
void parseHaptic();

} // namespace qvr::input

namespace qvr::inputlag
{

// vr_inputlag_test (vr_inputlag.cpp): a desktop key or mouse motion's frames to the game.
void registerCommands();
// Once per host frame, at its end (VR_HostFrameEnd).
void frameEnd();

} // namespace qvr::inputlag
