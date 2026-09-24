// vr_physics.hpp -- server-side Quake VR physics state shared with other VR sources.

#pragma once

namespace qvr::physics
{

// From each client's VR move: are its hands really tracked, or placed in front of the body?
void setClientHandsTracked(int client, bool tracked);

} // namespace qvr::physics
