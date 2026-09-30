// vr_climb.hpp -- climbing: holds taken with either hand or both (vr_climb, experimental; see vr_climb.cpp).
//
// The physics hooks are VR_ClimbPreThink and VR_ClientClimb (vr_api.h).

#pragma once

#include "vr_engine.hpp"

extern "C" int VR_ClientClimb(edict_t* ent); // VR_ClientSpecialMove: 1 hung or mantled instead of the move, -1 freed

namespace qvr::hands
{
struct State;
}

namespace qvr::climb
{

void init(); // registers vr_climb_probe

// Server: every player's holds and mantle forgotten (a map loaded, a saved game loaded: their entity numbers and ledge
// maps are another world's).
void reset();

// Server: the player's holds as stats (STAT_QVR_CLIMB*), for the drawn hands.
void calcStats(edict_t* ent, int* statsi);

// Client: the drawn `hand`'s place and turn (its controller's `pos` and angles `rot`, before the fist's offsets, whose
// turn `handTurn` is: the drawn hand's axes in the controller's) put on its hold while it holds, eased on and off (moved
// by vr_climb_hand_*, turned to face the hold by vr_climb_hand_turn_blend and vr_climb_hand_pitch/_yaw/_roll).
// `lightShift`: where its light is to be sampled from its place (the place without vr_climb_hand_*: the looks' offset
// doesn't light it differently). Returns how far it is on its hold, 0 (the controller's) .. 1.
float drawnHand(const hands::State& s, int hand, const glm::mat3& handTurn, glm::vec3& pos, glm::vec3& rot,
    glm::vec3& lightShift);

} // namespace qvr::climb
