// vr_torso.hpp -- which way the torso faces (hands::State::bodyYaw), estimated from the head and the hands.

#pragma once

#include "vr_hands.hpp"

namespace qvr::torso
{

// Once per hands update in VR, after the head and hands are placed: the torso's yaw (world degrees). `headYaw` is the
// head's (hands.cpp's headYawBlended), `turnYaw` the play space's turn (the estimate is kept in the play space, so that
// snap and smooth turns and the server's yaw turn it at once), `handsValid` whether both controllers are tracked.
// vr_torso_mode 0: the old engine's estimate (VR_GetBodyYawAngle), stateless.
[[nodiscard]] float estimate(const hands::State& s, float headYaw, float turnYaw, bool handsValid);

// Starts the estimate over from the head (a new map, tracking lost).
void reset();

// vr_torso_report [label]: the last estimate's parts, in the play space's degrees (left positive).
void report_f();

} // namespace qvr::torso
