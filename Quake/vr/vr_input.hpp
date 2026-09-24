// vr_input.hpp -- controller input.

#pragma once

#include "vr_backend.hpp"
#include "vr_engine.hpp"

namespace qvr::input
{

void init();

// Once per host frame with the backend's input.
void update(const InputState& in);

// svc_quakevr QVR_SVC_HAPTIC (vr_client.cpp dispatches it).
void parseHaptic();

} // namespace qvr::input
