// vr_teleport.hpp -- aiming and requesting a teleport with the off hand (+teleport).

#pragma once

#include "vr_hands.hpp"

namespace qvr::teleport
{

void init(); // registers +teleport/-teleport

// Once per move: aims while +teleport is held (vr_teleport_enabled). Returns true, with the
// destination, when a teleport is released onto a valid spot.
[[nodiscard]] bool update(const hands::State& s, glm::vec3& target);

} // namespace qvr::teleport
