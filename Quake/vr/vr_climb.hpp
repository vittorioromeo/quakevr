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

namespace qvr::pmove
{
struct Mantle;
struct Hang;
struct HangSums;
}

namespace qvr::climb
{

void init(); // registers vr_climb_probe, vr_climb_mantle_grunt_test

// Server, a new map loading (VR_OnSpawnServerBeforeLoad): the player's grunts precached (vr_climb_mantle_grunt_sound,
// vr_foegrab_throw_grunt_sound).
void precache();

// The player's grunts (the mantle's, the judo throw's: one list, chosen by a setting's number; the menus' Grunt Sound
// choices): `choice`'s sample (clamped into the list).
[[nodiscard]] const char* gruntSample(float choice);

// Server: `ent` grunts `choice` (gruntSample) at `volume` (0..1; 0 none), on its voice channel, if precached. True: heard.
bool grunt(edict_t* ent, float choice, float volume);

// Client: whether `hand` holds a ledge now (the server's holds, STAT_QVR_CLIMB): it is not free to take anything else
// (a weapon's second grip, a hotspot).
[[nodiscard]] bool holding(int hand);

// Client: whether the player mantles now (the server's mantle phase, STAT_QVR_CLIMB's bit 30): flat play's mantle FOV.
[[nodiscard]] bool mantling();

// Client: where `hand` holds now (the server's hold on the lip, STAT_QVR_CLIMB*, without the drawn hand's offsets) and
// the way out from its face (horizontal). False: it holds nothing. (vr_avatar.cpp: the feet's holds while climbing.)
bool heldAt(int hand, glm::vec3& at, glm::vec3& out);

// Server: the same for player `ent`'s hand `hand` (0 off, 1 main), as calcStats sends his own client: the others'
// copies of his body climb from his holds (vr_mpavatar.cpp, MP_PLAN.md B5c).
bool serverHold(edict_t* ent, int hand, glm::vec3& at, glm::vec3& out);

// Server: whether `ent`'s hand `hand` (0 off, 1 main) is empty (no weapon, carried thing, force grab, flashlight) and its
// grip there is not for a holster or a pouch (one that would draw or take something): free to hold on to something else
// (an enemy: vr_foegrab.cpp).
[[nodiscard]] bool handFree(edict_t* ent, int hand, bool holsters = true); // (holsters false: the grip at one counts not)

// Server: the prop (its edict number) `ent`'s hand `hand` (0 off, 1 main) holds a ledge of (vr_climb.cpp, "Holds on
// props"), 0 none: its hand's bodies don't push it (vr_box3d.cpp).
[[nodiscard]] int heldProp(edict_t* ent, int hand);

// Server: the flatscreen mantles `ent` started since the map loaded (vr_flat_mantle: after a jump, the jump key held,
// looking up at a ledge in reach; vr_climb.cpp flatMantle), and whether it mantles now (any mantle).
[[nodiscard]] int flatMantles(edict_t* ent, bool& mantling);

// Server: every player's holds and mantle forgotten (a map loaded, a saved game loaded: their entity numbers and ledge
// maps are another world's).
void reset();

// Server: `ent` mantling: its way now (the brush model's or prop's motion since it began added) and the time it has
// run, for pmove::mantleStep -> true (false: not mantling). Single player's frame and a remote client's moves (MP B8h).
[[nodiscard]] bool mantlePath(edict_t* ent, pmove::Mantle& m);
// Server: `ent`'s mantle after a frame or his moves ran it (pmove::mantleStep, his place set): its time kept; `done`,
// at its end: he stands (on a brush model or prop: on it), the climb lets go.
void mantleMoved(edict_t* ent, const pmove::Mantle& m, bool done);
// Server: `ent` hanging with every hold on the world (still: a hold on a brush model or prop moves with it, the frame's
// hang its own): its state for pmove::hangStep (the holds, each hand's last place relative to the body, the owed,
// slipping) -> true; false: not hanging so. A remote client's moves run its steps (MP B8i).
[[nodiscard]] bool hangPath(edict_t* ent, pmove::Hang& h);
// Server: after his moves ran the hang's steps (`h`, their sums `s`; his place set): the owed, the hands' last places and
// the mantle's search kept; a move's mantle test began the mantle (h.mantleHand; `m` its way and time run since): the
// climb mantles (or stands, at its end); else the frame's part, single player's too: kept within reach, the owed
// settled, the reach checked (out of reach: he falls), the no-room buzz, the stamina.
void hangMoved(edict_t* ent, const pmove::Hang& h, const pmove::HangSums& s, const pmove::Mantle& m);

// Server: the player's holds as stats (STAT_QVR_CLIMB*), for the drawn hands.
void calcStats(edict_t* ent, int* statsi);

// Client: the drawn `hand`'s place and turn (its controller's `pos` and angles `rot`, before the fist's offsets, whose
// turn `handTurn` is: the drawn hand's axes in the controller's) put on its hold while it holds, eased on and off (moved
// by vr_climb_hand_*, turned to face the hold by vr_climb_hand_turn_blend and vr_climb_hand_pitch/_yaw/_roll).
// `lightShift`: where its light is to be sampled from its place (the place without vr_climb_hand_*: the looks' offset
// doesn't light it differently). Returns how far it is on its hold, 0 (the controller's) .. 1.
float drawnHand(const hands::State& s, int hand, const glm::mat3& handTurn, glm::vec3& pos, glm::vec3& rot,
    glm::vec3& lightShift);

// Client: how far the drawn `hand` is on its hold, as drawnHand last made it (0..1): a flat mantle's free hand leaves the
// other hand's gun for it (vr_view_hands.cpp).
[[nodiscard]] float drawnShare(int hand);

} // namespace qvr::climb
