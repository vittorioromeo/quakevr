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
#include "vr_physsound.hpp"
#include "Zancle/Container/Vector.hpp"


namespace qvr::climbrope
{

void init(); // vr_rope_info, vr_rope_test

// Client, once a host frame after it is drawn (VR_HostFrameEnd): the client-side tests' samples and steps
// (vr_rope_hands_test, vr_rope_pause_test).
void clientFrame();

// Server, once a frame after the physics (vr_server.cpp's frame end): new rope entities found and made, every rope
// stepped (pushed by what meets it), sent to the clients where it moved. `worldRan`: SV_Physics ran this frame; false
// (paused: `pause`, single player's menu or console): the ropes stand as they are (ropes5).
void serverFrame(bool worldRan);

// Server: a new client's spawn state (VR_WriteClientSpawnState): every rope as it is now.
void serverWriteAll(sizebuf_t* msg);

// Server: the entity `num` freed (its rope, if it was one, gone: the clients told).
void forget(int num);

// Server: an explosion of `damage` at `at` (T_RadiusDamage's physicsblast): the ropes within its reach thrown.
void blast(const glm::vec3& at, float damage);

// Server: a shot from `start` to `end` with `velocity` (units/s) and `mass` (kg) (QC's physicsshot): the ropes it passes
// through pushed where it crosses them.
void shot(const glm::vec3& start, const glm::vec3& end, const glm::vec3& velocity, float mass);

// Server (ropes8c, ROPES.md "Chains"): a shot's, a nail's or a blow's line from `start` to `end` (a missile's box as a
// sphere of `radius`) against the chains' links (ropes are passed through): the nearest chain it meets before `fraction`
// (the move's hit so far) -> its entity, `fraction` and `normal` there; 0 none. `shooter` (or null): one climbing a rope
// passes the chains round him (his own in his hands).
[[nodiscard]] int chainClip(const glm::vec3& start, const glm::vec3& end, float radius, const edict_t* shooter,
    float& fraction, glm::vec3& normal);

// Server (ropes8a): whether edict `num` is a climbing pole's collision box (QC vr_pole_solid) -> true and its bounds
// (the navmesh's obstacles, vr_nav.cpp).
[[nodiscard]] bool solidBox(int num, glm::vec3& lo, glm::vec3& hi);

// Whether `classname` is a rope's (vr_rope, vr_chain) or a climbing pole's (vr_pole, ropes7: a static rope).
[[nodiscard]] bool isRopeClass(const char* classname);

// Server (ropes7): whether rope entity `ent` is traversed hanging under it, hand over hand along it (a monkey traverse:
// pinned at both ends, slanted past vr_rope_monkey_angle from upright), not climbed.
[[nodiscard]] bool monkey(int ent);

// Server (ropes9): whether rope entity `ent` is a climbing pole (rigid: climbed, never swung).
[[nodiscard]] bool pole(int ent);

// Server (ropes9): a climbing pole as the physics props meet it (vr_box3d_props.cpp syncPoles): its entity, its two
// ends and its radius (half its thickness), units.
struct PoleLine
{
    int ent{0};
    glm::vec3 a{0.f}, b{0.f};
    float radius{0.f};
};
// Every pole now -> `out` (cleared first).
void poleLines(za::Vector<PoleLine>& out);

// Server: what rope entity `ent` is made of (its grip's sound and haptics): a rope, a chain, a metal or wooden pole.
[[nodiscard]] physsound::RopeMaterial material(int ent);

// Server: a climbing hand of `player` took rope entity `ent` at `at`: its sound, a chain's or a rope's (physsound::ropeGrab),
// a headset's grip or flat-screen play's alike (ropes6: a rope's was silent to a headset's).
void grabbed(edict_t* player, int ent, const glm::vec3& at);

// Server: the toolgun just made rope entity `e` where its ghost stood (`at`, on the floor or a wall): its settings from
// the spawn panel's (vr_rope_spawn_*), its top found above `at` (the ceiling, or its length up). (ropes7) A pole stands
// from `at` up to the ceiling or its length; with vr_rope_spawn_span a rope is strung from 80 units over `at` to that far
// the `yaw` way (degrees), vr_rope_spawn_rise up, vr_rope_spawn_slack longer than the line.
// (`yaw`: the ghost's, degrees; it faces the player, so the rope is strung away from him.)
void toolgunPlaced(edict_t* e, const glm::vec3& at, float yaw);

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
// Client: the rope passing within `within` units of `near` -> true, and its point at height `z` (or its bottom end;
// `bottom`: its lowest point's height, ropes6).
[[nodiscard]] bool clientRopeAt(const glm::vec3& near, float within, float z, glm::vec3& at, float* bottom = nullptr);
// Client: the rope passing within `within` units of `near` -> true: its point as drawn this frame (on its curve through
// the points as shown) as far straight from its top as `near` is, its way up there (unit, towards its top) and its thickness (ropes5: a hand drawn
// on it, climb::ropePalm).
[[nodiscard]] bool clientRopeNearest(const glm::vec3& near, float within, glm::vec3& at, glm::vec3& up, float& thickness);
// Client (ropes7): whether the rope passing nearest `near` (within `within` units) is traversed hanging under it (a
// monkey traverse: the legs hang, the trunk isn't turned to it).
[[nodiscard]] bool clientRopeMonkey(const glm::vec3& near, float within);
// Client (ropes8d): his predicted move (vr_predict.cpp), his box (`mins`, `maxs`) from `start` to `end`, against the
// poles' boxes as the server places them (placeSolids, from each pole as sent) -> true if one was nearer than `best` (or
// he starts in one), `best` then that trace (ent null). Passed as passSolids does: a pole within reach while one of his
// drawn hands holds a rope, or one he is already in. A remote client only (a server here: false).
[[nodiscard]] bool clientSolidTrace(const float* start, const float* mins, const float* maxs, const float* end, trace_t& best);
// Client: how far `p` is from the nearest rope's pieces (units; 1e9: none). (vr_mp_avatars' mp_ropepose.)
[[nodiscard]] float clientRopeDistance(const glm::vec3& p);

// Client: QVR_SVC_CLIMBROPE.
void parse();

// Client: each eye's opaque scene: the ropes.
void drawOpaque();

} // namespace qvr::climbrope
