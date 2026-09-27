// vr_carry2h.hpp -- a prop held in both hands (vr_carry.qc: a box, backpack, gib, head or armour gripped by the
// second hand while the first carries it; vr_carry_two_hands).
//
// When the second hand takes hold, each hand's grip is kept in the object's frame, with the object's turn in each
// hand. From then on the object follows both hands rigidly, as if held at both grips:
//
// - Its turn: each hand alone would carry the object turned so (its turn in that hand); the two are averaged (a
//   quaternion mean, never degenerate: the halves are on the same side), and that is then swung the least way that
//   lines up its grip-to-grip axis with the line between the hands. So rotating both hands together rotates it
//   exactly; moving one hand swings it about the other; twisting both hands about the line between them rolls it
//   (one hand alone: half as much).
// - Its place: the middle of its grips on the middle of the hands. Pulling the hands apart (or together) never
//   stretches it: each hand is then off its grip along the line by half the change.
// - Hands (almost) on the same spot, or gripped there: no line to follow, the average turn alone (as one hand).
//
// When every hand moves as one rigid body, the solve gives back that motion exactly. The server places the object so
// (QC's carry2h); the local client draws it so from the hands of this frame (vr_held.cpp), and draws each hand on its
// grip (a few centimetres off the controller at most, vr_carry_two_hands_drift).

#pragma once

#include "vr_engine.hpp"

namespace qvr::carry2h
{

// A pose: a place in the world and a turn (the axes forward, left, up as columns: held::axesFromAngles's).
struct Frame
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
};

// What was kept when the second hand took hold. Hands: [0] off, [1] main (as hands::State).
struct Hold
{
    glm::quat inHand[2]{glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat{1.f, 0.f, 0.f, 0.f}}; // the object's turn in each hand
    glm::vec3 grip[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand's grip, in the object's frame (from its origin)
    glm::vec3 axis{1.f, 0.f, 0.f}; // from the off hand's grip to the main hand's, in the object's frame (unit)
    float span{0.f};               // the grips' distance (world units)
};

[[nodiscard]] Hold record(const Frame& object, const Frame hands[2]);

// The object's pose for the hands at `hands`.
[[nodiscard]] Frame solve(const Hold& hold, const Frame hands[2]);

// Where hand `hand` would be, held on its grip on the object at `object` (its controller's pose: the pose `hands`
// has in record and solve).
[[nodiscard]] Frame onGrip(const Hold& hold, const Frame& object, int hand);

// A turn from angles (held::axesFromAngles's conventions) and back.
[[nodiscard]] glm::quat fromAngles(const float* angles, bool brush);
void toAngles(const glm::quat& q, float* out, bool brush);

// Server side (QC's carry2h): the object `ent` the player `player` holds in both hands. `grab`: the second hand has
// just taken hold (the grips are kept; the result is where it is). Else the place it should go to (its angles are
// set now); kept first if nothing was (a saved game, a new server). Its hands are the player's fields.
[[nodiscard]] glm::vec3 serverPlace(edict_t* ent, edict_t* player, bool grab);

// Server side (QC's carryreach): whether a hand at `point` can take hold of `ent` (in its turned box, within
// vr_carry_reach of its drawn surface), as a hand touching it can (vr_physics.cpp); vr_debug_carry draws the test.
[[nodiscard]] bool reaches(edict_t* ent, const glm::vec3& point, int hand);

// Forgets the kept holds (a new server).
void resetServer();

} // namespace qvr::carry2h
