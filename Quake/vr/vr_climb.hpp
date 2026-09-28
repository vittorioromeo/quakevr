// vr_climb.hpp -- climbing: holds taken with either hand or both (vr_climb, experimental; see vr_climb.cpp).
//
// The physics hooks are VR_ClimbPreThink and VR_ClientClimb (vr_api.h).

#pragma once

#include "vr_engine.hpp"

namespace qvr::hands
{
struct State;
}

namespace qvr::climb
{

void init(); // registers vr_climb_probe

// Server: the player's holds as stats (STAT_QVR_CLIMB*), for the drawn hands.
void calcStats(edict_t* ent, int* statsi);

// Client: the drawn `hand`'s place (its controller's, before the fist's offsets) put on its hold while it holds,
// eased on and off (moved by vr_climb_hand_*). `lightShift`: where its light is to be sampled from its place (the
// place without vr_climb_hand_*: the looks' offset doesn't light it differently).
void drawnHand(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& lightShift);

} // namespace qvr::climb
