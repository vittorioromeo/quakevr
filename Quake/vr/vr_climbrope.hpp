// vr_climbrope.hpp -- climbable ropes and chains (docs/vr-port/ROPES.md): map entities (QC vr_rope, vr_chain) and the
// toolgun's (Spawn > Props > Ropes), each a chain of points hanging from its top, stepped by the server with the
// grappling hook rope's and the magnetic holsters' Verlet steps (vr_verlet.hpp: fall, held to its pieces' lengths, kept
// out of the world), pushed by what meets it (the players' hands and bodies, monsters, props, explosions, shots), sent to
// every client (QVR_SVC_CLIMBROPE) and drawn there as a rope (a plain cable) or a chain of links (vr_coil.hpp, the
// magnetic holsters' chain): the two styles look different and behave the same.
//
// Phase 1 of the ropes (ROUND25.md, "ropes"): the rope as a physical thing. Climbing it (VR hand over hand, flat-screen
// auto-climb) and pumping its swing come in the next phases.

#pragma once

#include "vr_engine.hpp"


namespace qvr::climbrope
{

void init(); // vr_rope_info, vr_rope_test

// Server, once a frame after the physics (vr_server.cpp's frame end): new rope entities found and made, every rope
// stepped (pushed by what meets it), sent to the clients where it moved.
void serverFrame();

// Server: a new client's spawn state (VR_WriteClientSpawnState): every rope as it is now.
void serverWriteAll(sizebuf_t* msg);

// Server: the entity `num` freed (its rope, if it was one, gone: the clients told).
void forget(int num);

// Server: an explosion of `damage` at `at` (T_RadiusDamage's physicsblast): the ropes within its reach thrown.
void blast(const glm::vec3& at, float damage);

// Server: a shot from `start` to `end` with `velocity` (units/s) and `mass` (kg) (QC's physicsshot): the ropes it passes
// through pushed where it crosses them.
void shot(const glm::vec3& start, const glm::vec3& end, const glm::vec3& velocity, float mass);

// Whether `classname` is a rope's (vr_rope, vr_chain).
[[nodiscard]] bool isRopeClass(const char* classname);

// Server: the toolgun just made rope entity `e` where its ghost stood (`at`, on the floor or a wall): its settings from
// the spawn panel's (vr_rope_spawn_*), its top found above `at` (the ceiling, or its length up).
void toolgunPlaced(edict_t* e, const glm::vec3& at);

// Client: QVR_SVC_CLIMBROPE.
void parse();

// Client: each eye's opaque scene: the ropes.
void drawOpaque();

} // namespace qvr::climbrope
