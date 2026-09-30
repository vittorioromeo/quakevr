// vr_ropesim.hpp -- the grappling hook's rope as the game has it (docs/vr-port/ROUND21.md, "Grapple: the rope wraps"):
// straight pieces between the gun and the hook, bent at the corners it wraps round (a pillar's edge, a doorway's jamb, a
// box's top), never through the world, doors and lifts or the props (Box3D's shapes, vr_box3d.cpp); the taut rope's
// path along them, which the QC's rope pulls along. The server's; the corners are sent to the clients, which draw the
// slack rope hanging between them (vr_rope.cpp).

#pragma once

#include "vr_engine.hpp"

#include <vector>

namespace qvr::ropesim
{

// The rope's state after a step, as the QC reads it.
struct Shape
{
    float path{0.f};           // the taut path's length from the game's end to the hook (around what is in the way)
    glm::vec3 pivotA{0.f};     // its first corner from the game's end (the hook itself when nothing is in the way)
    float beyondA{0.f};        // the path from there on to the hook
    glm::vec3 pivotB{0.f};     // its first corner from the hook (the game's end when nothing is in the way)
    float beyondB{0.f};        // the path from there on to the game's end
};

// Steps the rope of the hook `hook` (once a server frame: later calls give the same) with its ends: the gun (`gun`: where
// the rope leaves it: the muzzle, the holster, the gun lying about), the game's end (`game`: where the rope holds the
// player from: the body, or the gun) and the hook (`end`), `length` units long as the game has it (from `game`; unused:
// the slack is the client's), its ends' entities (`skipA`, `skipB`: not in its way; 0 none). The taut path's shape.
Shape step(edict_t* hook, const glm::vec3& gun, const glm::vec3& game, const glm::vec3& end, float length, int skipA, int skipB);

// The shape of the last step (a hook never stepped: straight from the ends given last... none: all 0).
[[nodiscard]] const Shape& shape(edict_t* hook);

// Writes the rope's corners to the clients (QVR_SVC_ROPE) when they changed (or now and then), for the beam of `owner`
// with beam id `beamId` (the rope drawn through them: vr_rope.cpp).
void send(edict_t* hook, edict_t* owner, int beamId);

// Tells the clients that the rope of `owner`'s beam `beamId` has ended (QVR_SVC_ROPE with ropeEnded corners): its hook's
// rope is another beam's now (its gun taken, dropped, passed to the other hand). The client takes the old beam away at
// once rather than drawing it for the 0.2 s a beam lasts (it was drawn from wherever its start went: the phantom rope
// between two guns lying about, NOTES.md vrfiringrange_2026-09-30_02-56-42).
void sendEnded(edict_t* owner, int beamId);

// The entity `num` removed: its rope forgotten.
void forget(int num);

// A new server: every rope forgotten.
void reset();

// The profiler's count: the ropes stepped this frame and their points (the ends and the corners).
void profileCounts(int& ropes, int& points);

} // namespace qvr::ropesim
