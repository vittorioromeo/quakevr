// vr_input.hpp -- controller input.

#pragma once

#include "vr_backend.hpp"
#include "vr_hands.hpp"
#include "vr_engine.hpp"

namespace qvr::input
{

void init();

// Once per host frame with the backend's input.
void update(const InputState& in);

// Once per host frame after the hands: a jump in the room jumps in the game (vr_roomscale_jump).
void roomscaleJump(const hands::State& s);

// svc_quakevr QVR_SVC_HAPTIC (vr_client.cpp dispatches it).
void parseHaptic();

} // namespace qvr::input
