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

// Builtin navsteer(monster, goal): the yaw (degrees, 0..360) toward the next corner of the monster's path to the goal's
// feet, or -1 when the chase should stay Quake's (vr_nav 0, no navmesh for its size yet, off the mesh, no path, or the
// goal in a straight line on the mesh).
[[nodiscard]] float steer(edict_t* ent, edict_t* goal);

// The view's debug draws (vr_nav_debug): the navmesh round `view`, and with 2 the monsters' paths.
void debugDraw(const glm::vec3& view);

} // namespace qvr::nav
