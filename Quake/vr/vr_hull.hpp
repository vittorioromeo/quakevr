// vr_hull.hpp -- a player narrower than Quake's hull 1 against the map, on unmodified maps (vr_hull_width;
// docs/vr-port/HULLS.md). Quake's maps carry their clipping hulls precompiled for two box sizes (hull 1: 32x32x56,
// hull 2: 64x64x88), so the player stands 16 units off every wall. At a map's load the world's and its brush models'
// solid space is rebuilt from hull 0 (the drawing BSP) as convex brushes, one per solid leaf, with Quake 2's bevels;
// a box of any size is then swept against them as Quake 2 does (the tree walked with the box's extent, each solid
// leaf's brush clipped), with Quake's trace results (startsolid, allsolid, the 1/32 unit back-off).
//
// Only the player's own box (a client's move with a 32-wide box, the height kept at hull 1's 56) and only against
// SOLID_BSP models (the world, doors, lifts, trains) is narrowed: its box against monsters, items, triggers and shots
// stays Quake's. Off (vr_hull_width 0) nothing is built and every trace is Quake's.

#pragma once

#include "vr_engine.hpp"

namespace qvr::hull
{

// VR_Init: the vr_hull_stats and vr_hull_bench commands, the setting's callback.
void init();

// VR_OnSpawnServerAfterLoad: the server's map rebuilt as brushes (with vr_hull_width on; else on first use).
void afterLoad();

// SV_Move: true if this move meets BSP models with the narrower box (a client's own 32-wide box), filled in
// (mins and maxs are the move's).
[[nodiscard]] bool moveBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs);

// SV_ClipMoveToEntity for a SOLID_BSP entity with moveBox's box: false if its model has no brushes (another BSP
// file's), and Quake's hull is used.
[[nodiscard]] bool clipBSP(const edict_t* ent, const float* start, const float* boxMins, const float* boxMaxs,
    const float* end, trace_t& trace);

// The client's lean recentring (worldtrace::playerBoxFits): the player's box, narrowed, from start to end in the
// world: 1 if it gets there, 0 if not, -1 when narrowing is off (Quake's hull 1 is used).
[[nodiscard]] int playerBoxFits(qmodel_t* world, const glm::vec3& start, const glm::vec3& end);

// VR_ClientPreMove: vr_hull_walktest's random walk drives the first player (a test aid; nothing when not running).
void walkTestFrame(edict_t* ent);

} // namespace qvr::hull
