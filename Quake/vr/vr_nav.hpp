// vr_nav.hpp -- navmesh pathfinding for walking monsters (docs/vr-port/NAVMESH.md): Recast builds a navmesh of the server's
// map at its load for each monster size class (the widths the monsters move with, vr_hull's moveSize, at Quake's heights)
// from vr_hull's convex brushes (clip brushes included), tiled on the game's pool and kept on disk (cache/navmesh); Detour
// finds a chasing monster's path to its foe, and the QC's chase (VR_AI_MoveToGoal) steps toward the path's next corner
// with Quake's own walkmove (navsteer). Doors, lifts and trains are left out of the mesh and block the floor they stand
// on while they do. vr_nav 0: Quake's chase as before.

#pragma once

#include "vr_engine.hpp"

namespace qvr::nav
{

// VR_Init: the vr_nav_* commands.
void init();

// VR_OnSpawnServerAfterLoad: the map's geometry gathered, the size classes of the monsters it spawned built on the pool.
void afterLoad();

// VR_OnClearMemory, before the hunk goes (every map load, a failed one too): the builds and props' batches still on the
// pool joined (they read the old world's model) and the navmeshes forgotten.
void finishLoads();

// Builtin navsteer(monster, goal): the yaw (degrees, 0..360) toward the next corner of the monster's path to the goal's
// feet, or -1 when the chase should stay Quake's (vr_nav 0, no navmesh for its size yet, off the mesh, no path, or the
// goal in a straight line on the mesh).
[[nodiscard]] float steer(edict_t* ent, edict_t* goal);
// Builtin navlink(monster): what its last steer asks besides the yaw: 0 walk (walkmove), 1 walk off the ledge (a drop
// link: where walkmove stops at the edge, the QC lets it step off and fall), 2 stand (waiting for a lift, riding one, or
// in the air off a ledge).
[[nodiscard]] int action(const edict_t* ent);

// Builtin navreach(monster, goal): 1 if its class's navmesh has a whole path from its feet to the goal's (vr_nav on), 0
// if not, -1 if there is no navmesh for it (yet), vr_nav is off or it isn't a walker. (QC VR_NAV_MeleeOff.)
[[nodiscard]] float reach(edict_t* ent, edict_t* goal);

// Each server frame (VR_PhysicsFrameEnd, the server's VM current): the movers' and the props' tick (props as obstacles:
// their rests looked at, their tiles' rebuilds started and joined), whether or not anything chases.
void serverFrame();
// Each server frame (VR_PhysicsFrameEnd, first, whether or not rigid bodies run): counted, the clock the pool jobs'
// results land by (vr_nav_job_frames, vr_nav_build_frames: on a fixed frame after their start, whatever the CPU's load).
void countFrame();

// Builtin navobstacles(what): props as obstacles (vr_nav_props), looked at now: 0 what is left to do (props coming to
// rest that may block, tiles waiting for their rebuild, rebuilds on the pool: 0 when the mesh is current), 1 props
// blocking, 2 rebuild batches done, 3 tiles rebuilt or given back since the map's load; 4 the batches' pool time in
// all (ms), 5 the worst batch's, 6 the worst batch's put-in on the main thread.
[[nodiscard]] float obstacles(int what);

// The view's debug draws (vr_nav_debug): the navmesh round `view`, and with 2 the monsters' paths.
void debugDraw(const glm::vec3& view);

} // namespace qvr::nav
