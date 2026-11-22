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

// Server: the rope the ray `from` + t `dir` (unit) meets first within `range` (its pieces, grown a little): its entity
// and `distance` along the ray; 0: none (the toolgun's aim: Remove, the physgun).
[[nodiscard]] int pick(const glm::vec3& from, const glm::vec3& dir, float range, float& distance);

// Climbing (P2, ROPES.md "Climbing"; vr_climb.cpp's rope holds). A hold on a rope: its entity, where along it (`s`
// units from the top, along the pieces) and how far straight from the top (`d`).
struct Grab
{
    int ent{0};
    float s{0.f}, d{0.f};
    glm::vec3 at{0.f};
    bool chain{false};
};
// Server: the rope piece nearest `hand` within `reach` units of its surface (not at its top) -> true and the hold there.
[[nodiscard]] bool findGrab(const glm::vec3& hand, float reach, Grab& out);
// Server: the hold `s` units along rope `ent` from its top (kept off its top and its free end) as client `client` would
// have it now: on his rider's line (his own), or on its points. False: the rope is gone. (Flat play's hand over hand,
// vr_climb.cpp flatRopeFrame.)
[[nodiscard]] bool grabAlong(int ent, int client, float s, Grab& out);
// Server: where a hold of client `client` (0-based) on rope `ent` is now: on its rider's line (his own), or on its points.
// False: the rope is gone.
[[nodiscard]] bool holdAt(int ent, int client, float s, float d, glm::vec3& at);
// Server: that hold's velocity (units/s): letting go and jumping off keep it.
[[nodiscard]] glm::vec3 holdVelocity(int ent, int client, float s, float d);
// Server, each frame client `client` holds rope `ent` (climbPreThink): his highest and lowest holding hands' places on
// it; `grabVel` his velocity as he took hold (a new rider's first swing). A frame he doesn't, he has let go. False:
// another rides it (this one's holds follow its points).
bool ride(int client, int ent, float sHigh, float dHigh, float sLow, float dLow, const glm::vec3& grabVel);
// Server: the rider's body couldn't make `missed` (units) of this frame's carry: his swing stopped that way.
void riderBlocked(int client, const glm::vec3& missed);
// Server: the rider's pump (units/s/s, level; P3), until set again.
void setPump(int client, const glm::vec3& accel);
// Server: the rope client `client` rides (true), its velocity at his lowest hand (units/s).
[[nodiscard]] bool riding(int client, int& ent, glm::vec3& handVel);
// Client: the rope passing within `within` units of `near` -> true, and its point at height `z` (or its bottom end).
[[nodiscard]] bool clientRopeAt(const glm::vec3& near, float within, float z, glm::vec3& at);
// Client: how far `p` is from the nearest rope's pieces (units; 1e9: none). (vr_mp_avatars' mp_ropepose.)
[[nodiscard]] float clientRopeDistance(const glm::vec3& p);

// Client: QVR_SVC_CLIMBROPE.
void parse();

// Client: each eye's opaque scene: the ropes.
void drawOpaque();

} // namespace qvr::climbrope
