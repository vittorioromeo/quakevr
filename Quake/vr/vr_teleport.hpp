// vr_teleport.hpp -- aiming and requesting a teleport: with the off hand (+teleport), or with the turning stick's hand
// (vr_teleport_enabled 2, Smooth + Teleport: vr_input.cpp's turning stick pushed forward).

#pragma once

#include "vr_hands.hpp"

namespace qvr::teleport
{

void init(); // registers +teleport/-teleport, vr_teleport_both_test

// Once per move: aims while +teleport is held (vr_teleport_enabled 1 or 2) or the turning stick aims (2). Returns true,
// with the destination, when a teleport is released onto a valid spot.
[[nodiscard]] bool update(const hands::State& s, glm::vec3& target);

// vr_teleport_enabled 2 (Smooth + Teleport): the turning stick's push forward aims (vr_input.cpp routeSticks).
[[nodiscard]] bool stickMode();

// The input's (vr_input.cpp, Smooth + Teleport): the turning stick of `hand` held forward (aiming with that hand), or let
// go (back to the middle: the teleport, onto a valid spot); `cancel`: pulled back, no teleport.
void stickAim(int hand, bool held, bool cancel);

} // namespace qvr::teleport
