// vr_climb.hpp -- ledge grabbing and mantling (vr_climb, experimental; see vr_climb.cpp).
//
// The physics hooks are VR_ClimbPreThink and VR_ClientClimb (vr_api.h).

#pragma once

namespace qvr::climb
{

void init(); // registers vr_climb_probe

} // namespace qvr::climb
