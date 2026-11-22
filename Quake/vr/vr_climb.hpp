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
struct Body;
struct Input;
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

// Server: how many of `ent`'s hands hold a climbable rope (vr_climb.cpp, "Holds on ropes"), hanging.
[[nodiscard]] int ropeHands(edict_t* ent);

// Server: the flatscreen mantles `ent` started since the map loaded (vr_flat_mantle: after a jump, the jump key held,
// looking up at a ledge in reach; vr_climb.cpp flatMantle), and whether it mantles now (any mantle).
[[nodiscard]] int flatMantles(edict_t* ent, bool& mantling);

// Server: the flatscreen ladder climbs `ent` started since the map loaded (vr_flat_ladder: walking into a stack of
// rungs; vr_climb.cpp flatLadder), and whether it is on one now.
[[nodiscard]] int flatLadders(edict_t* ent, bool& climbing);
// Server: the flatscreen rope climbs `ent` started since the map loaded (vr_flat_rope: walking or jumping into a
// climbable rope; vr_climb.cpp flatRope), and whether it is on one now.
[[nodiscard]] int flatRopes(edict_t* ent, bool& climbing);

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

// The take of a hold (MP B8j part 2): a pressed grip looking for one, run by single player's frame and by a remote
// client's moves one by one (the server's, VR_PredictPhysics, and his replay's: one code path, the same numbers where
// the two see the same world).
// What the press remembers: pressed at the last move (a press's edge takes), and how long ago a press in the air found
// no hold (it keeps looking while it stays pressed, vr_climb_air_grab_time; -1: none).
struct TakeMemory
{
    bool pressedLast[2]{false, false};
    float missAge[2]{-1.f, -1.f};
};
// What else decides a take: the server's, once a frame (and with a remote client's state, for his replay).
struct TakeRules
{
    bool enabled{false};             // climbing on, a walking player alive (the hands tracked: each move's own)
    bool tired{false};               // too little stamina for a new hang (a hand joining the other is let)
    bool busy[2]{false, false};      // the hand holds something (a weapon, a carried object, a force grab)
    unsigned holster[2]{0u, 0u};     // the hotspots where a holster wins over a hold (a bit each): standing, hanging
    float noGrabLeft{0.f};           // seconds before a new hold may be taken (after letting go, or a mantle's end)
    float airGrabTime{0.f};          // vr_climb_air_grab_time
    float leniency{0.f}, leniencyAir{0.f}; // the search's radius round the hand (units): on the ground, in the air
    float minHeight{0.f}, propMinHeight{0.f}; // vr_climb_min_height, vr_climb_prop_min_height
    // The hang's box and the mantle's rules (pmove::Hang's: a hold taken in a move hangs the next moves).
    float minsZ{-24.f}, maxsX{16.f}, maxsY{16.f}, mantleTime{0.f}, mantleHeight{0.f}, mantleSlope{0.f};
};
// The regrab delay (seconds without new holds after letting go of everything or a mantle's end).
inline constexpr float regrabDelay = 0.4f;
// Server: whether `ent`'s holds are taken in his moves (a remote client's moves one by one: VR_PredictWalks; not while
// a hold that moves runs his frame's hang).
[[nodiscard]] bool takesInMoves(edict_t* ent);
// Server: `ent`'s take rules now and its press memory -> false: no climber.
bool takeState(edict_t* ent, TakeRules& r, TakeMemory& m);
// Server: one of `ent`'s moves' takes, after its step (`b` the body the move left, stored in the edict; `in` the move;
// `r` the frame's rules, its noGrabLeft counted down) -> 0 none; 1 a hold on the world taken (b.hang: the next moves
// hang); 2 a hold that moves taken (his frame's hang from the next frame: the frame's moves stop here).
int moveTake(edict_t* ent, pmove::Body& b, TakeRules& r, const pmove::Input& in);
// Client (a remote client's replay, MP B8j part 2): the same take on the world alone, his replayed body `b` (a hold
// taken: b.hang), `m` and `r` the state's, kept through the moves.
void replayTake(pmove::Body& b, TakeMemory& m, TakeRules& r, const pmove::Input& in);

// Server: the player's holds as stats (STAT_QVR_CLIMB*), for the drawn hands.
void calcStats(edict_t* ent, int* statsi);

// Client: the drawn `hand`'s place and turn (its controller's `pos` and angles `rot`, before the fist's offsets, whose
// turn `handTurn` is: the drawn hand's axes in the controller's) put on its hold while it holds, eased on and off (moved
// by vr_climb_hand_*, turned to face the hold by vr_climb_hand_turn_blend and vr_climb_hand_pitch/_yaw/_roll).
// `lightShift`: where its light is to be sampled from its place (the place without vr_climb_hand_*: the looks' offset
// doesn't light it differently). Returns how far it is on its hold, 0 (the controller's) .. 1.
float drawnHand(const hands::State& s, int hand, const glm::mat3& handTurn, glm::vec3& pos, glm::vec3& rot,
    glm::vec3& lightShift);

// Client: whether the hold at `hold` (world) is on a climbable rope as this client draws it (climbrope::clientRopeDistance).
[[nodiscard]] bool ropeHold(const glm::vec3& hold);

// Client: where the palm's middle of a hand on a rope's hold goes from it (ropes4): towards the climber (`out`) by the
// rope's near side, not vr_climb_hand_*'s ledge offsets (which put a hand on a rope off it). Yours and another player's
// (vr_mpavatar.cpp: his held hands pinned there).
[[nodiscard]] glm::vec3 ropeHandOffset(const glm::vec3& out);
// Client (ropes5): a hand on a rope's hold `hold` (a server's: a tick old, on its points or its rider's line) with its
// way out `out` (towards the climber) -> true: its palm's middle on the rope as drawn this frame (the drawn curve's
// point nearest the hold, out from it by ropeHandOffset's or, a thick chain, its surface a little in) and the rope's way
// up there (`up`, if given). False: no rope drawn near it. Yours (drawnHand) and another player's (vr_mpavatar.cpp).
[[nodiscard]] bool ropePalm(const glm::vec3& hold, const glm::vec3& out, glm::vec3& palm, glm::vec3* up);

// Client: how far the drawn `hand` is on its hold, as drawnHand last made it (0..1): a flat mantle's free hand leaves the
// other hand's gun for it (vr_view_hands.cpp).
[[nodiscard]] float drawnShare(int hand);
// Client: where drawnHand last put the drawn `hand`'s palm middle on its hold (world) -> true; false: not on one.
// (vr_rope_hands_test's samples.)
[[nodiscard]] bool drawnPalm(int hand, glm::vec3& palm);

} // namespace qvr::climb
