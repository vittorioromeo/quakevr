// vr_hull.hpp -- a player narrower than Quake's hull 1 against the map, on unmodified maps (vr_hull_width;
// docs/vr-port/HULLS.md). Quake's maps carry their clipping hulls precompiled for two box sizes (hull 1: 32x32x56,
// hull 2: 64x64x88), so the player stands 16 units off every wall. At a map's load the world's and its brush models'
// solid space is rebuilt from hull 0 (the drawing BSP) as convex brushes, one per solid leaf, with Quake 2's bevels;
// a box of any size is then swept against them as Quake 2 does (the tree walked with the box's extent, each solid
// leaf's brush clipped), with Quake's trace results (startsolid, allsolid, the 1/32 unit back-off).
//
// The moves meet the map with a clipping hull compiled at load for the width from the same brushes grown by the box
// (qbsp's way), traced by Quake's own SV_RecursiveHullCheck (the sweep, as vr_hull_method 0, retired 2026-10-16: it
// stays for the portals' clipping and vr_hull_bench).
//
// Only the player's own box (a client's move with a 32-wide box, the height kept at hull 1's 56) is narrowed: against
// the world and every brush model (its submodels, external .bsp models) with vr_hull_width, and against other entities'
// boxes (monsters, players, solid boxes; both ways: a body moving into a player meets its narrower box too) with
// vr_hull_ent_width. Shots, missiles and precise hits meet Quake's 32 box; items and triggers are touched with it.
// Off (vr_hull_width 0, vr_hull_ent_width 0 or -1) nothing is built and every trace is Quake's.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::hull
{

// VR_Init: the vr_hull_stats and vr_hull_bench commands, the setting's callback.
void init();
void registerDiagCommands(); // the hull tests and benches (vr_hull_diag.cpp)

// The map load's builds on the game's thread pool, made while the server spawns the map: VR_OnSpawnServerBeforeLoad
// starts the map as brushes and the player's compiled hull, VR_OnEntitySpawned each monster width's hull as the first
// monster of that width spawns (VR_OnSpawnServerSpawned any left).
// Their first use waits for them; finishLoads() too (the map's memory about to go, shutdown). The results are those
// of the builds on one thread.
void beforeLoad();
void entitySpawned(const edict_t* ent);
void spawned();
void finishLoads();
// VR_OnClearMemory, before the map's memory goes: its brushes and compiled hulls kept for a load of the same map (its
// BSP's content: a death's reload, restart, a save of it, a changelevel back), vr_hull_keep maps of them.
void keepForReload();

// VR_OnSpawnServerAfterLoad: the server's map rebuilt as brushes (with vr_hull_width on; else on first use).
void afterLoad();

// SV_Move: true if this move meets BSP models with the narrower box (a client's own 32-wide box), filled in
// (mins and maxs are the move's).
[[nodiscard]] bool moveBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs);

// SV_Move: true if this move meets other entities' boxes with the narrower box (vr_hull_ent_width), filled in.
[[nodiscard]] bool entBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs);

// A player's box as a body moving into it (mover; null: none) meets it: true if narrowed, filled in.
[[nodiscard]] bool touchBox(const edict_t* touch, const edict_t* mover, float* boxMins, float* boxMaxs);

// The navmesh (vr_nav.cpp): the server map's solid space as the convex brushes this file rebuilt (hull 0's solid leaves,
// and with `clips` the world's recovered clip brushes), for the world's model `sub` (0 the world, i its "*i"), in the
// model's own space. Each brush is `count` planes from `first` in `planes` (normal xyz, distance w: inside is
// dot(normal, p) < w; a clip brush's floor and top faces stand at its box centres' height for Quake's 32 box). Main
// thread; false without a server map.
struct NavBrush
{
    int first, count;
    bool clip;
};
[[nodiscard]] bool navSolids(int sub, bool clips, za::Vector<glm::vec4>& planes, za::Vector<NavBrush>& brushes);

// A walking monster's box against the map as it moves (vr_mhull: its class's width and Quake's height; else Quake's hull
// for its size): its width and height.
void moveSize(const edict_t* ent, float& width, float& height);

// A monster's corners for SV_CheckBottom (vr_mhull_ledges): absMins..absMaxs narrowed to its width; true if narrowed.
[[nodiscard]] bool footprint(const edict_t* ent, float* absMins, float* absMaxs);

// A player's box as a shot or missile meets it: narrowed (vr_hull_hit_width) and, a bullet or missile (`projectile`), only
// up to the top of his head (vr_hull_hit_head). True if changed.
[[nodiscard]] bool hitBox(const edict_t* touch, float* boxMins, float* boxMaxs, bool projectile);

// SV_ClipMoveToEntity for a SOLID_BSP entity with moveBox's box (the compiled hull): false if not a brush model, or
// a brush model other than the world with vr_hull_brushmodels off: Quake's hull is used.
[[nodiscard]] bool clipBSP(const edict_t* ent, const float* start, const float* boxMins, const float* boxMaxs,
    const float* end, trace_t& trace);

// The client's lean recentring (worldtrace::playerBoxFits): the player's box, narrowed, from start to end in the
// world: 1 if it gets there, 0 if not, -1 when narrowing is off (Quake's hull 1 is used).
[[nodiscard]] int playerBoxFits(qmodel_t* world, const glm::vec3& start, const glm::vec3& end);

// A client's move received (vr_server.cpp): his crouch from his head's height over his feet: crouched
// when his eyes are under the crouched box's top (vr_crouch_height); standing again once they are back over it and the
// standing box fits where he is. flatscreenCrouch (vr_flatplay.cpp, vr_server.cpp): whether the flatscreen player's
// crouch key is held now -- his eyes stand under the tallest crouched box's top whatever he does, so his key, not their
// height, says whether he is crouched. True for a headset player, who crouches with his own head: nothing about him
// changes.
void updateCrouch(edict_t* ent, const glm::vec3& head, bool flatscreenCrouch);
// Whether a client has his crouched box (the box moveBox, entBox, touchBox and hitBox give him).
[[nodiscard]] bool isCrouched(const edict_t* ent);
// Whether a client lies down (vr_prone: updateCrouch; his box then vr_prone_height tall, a crouched box to the rest).
[[nodiscard]] bool isProne(const edict_t* ent);
// Whether a client has the lying box (its own slot), vr_prone on or not (switched off while lying under a low ceiling
// he keeps it until he can get up): the triggers' and the water's checks follow it (VR_HullTouchTop, VR_HullWaistZ).
[[nodiscard]] bool hasLyingBox(const edict_t* ent);
// The local player (client 1 of this server) lying down: 1, 0; -1 with no server here (a client of another one).
[[nodiscard]] int localProne();
// Whether the local player, lying down, could get up: the lowest crouched box fits where he is (updateCrouch's own test
// for the rise). True when he is not lying or there is no server here (nothing to ask: the server decides).
// Flat-screen play's ways out of lying down (vr_flatplay.cpp: the prone key, the crouch) ask it first.
[[nodiscard]] bool localRiseFits();
// A client playing flat-screen (-flatscreenmode: .vr_flatscreen, as his last move said), and whether that move held his
// crouch key (protocol::moveFlatCrouchBit): the slide (vr_flatmove.cpp) starts from it, the lean's hit boxes need the first.
[[nodiscard]] bool isFlat(const edict_t* ent);
[[nodiscard]] bool flatCrouchHeld(const edict_t* ent);
// Flat-screen play's rise from a crouch (vr_flatplay.cpp pollKeysFlat): the highest the local player's eyes may stand
// over his feet in the box this server gives him -- his crouched box's top less flatEyeHeadroom (8), so the view never
// rises into a ceiling his box is kept under (updateCrouch, the key let go: standing if it fits, else the tallest crouched
// box that does, else the one he has: the headset's checks); -1 standing, while his crouch key holds him down (that box
// follows his own lowered eyes and is no ceiling to them), or no server here (no limit).
[[nodiscard]] float localEyeCap();
// Standing up under a low ceiling (vr_lowceil; updateCrouch): a headset player lying or crouched whose real head rises
// into the ceiling over his box: 0 not, 1 lying (lie back down), 2 crouched (crouch back down). Held, he keeps his box
// and stays where he is (crouchSpeedMax 0, the arms' crawl, VR_ClientRoomscaleMove, VR_ClientTeleport).
[[nodiscard]] int heightHold(const edict_t* ent);
// Held, the height in the world over which his hands touch nothing (his box's top: a teleporter in the wall over a vent);
// 1e30 when not held.
[[nodiscard]] float heldReachTop(const edict_t* ent);
// The local player's (client 1 of this server; all 0 with no server here): the hold's kind, whether his eyes are past the
// ceiling or a wall seen from his box (the view darkened: vr_lowceil.cpp), his eyes' height over his feet, the ceiling's
// found (0: none) and his box's top.
struct LocalHold
{
    int kind{0};
    bool hidden{false};
    float eye{0.f};
    float ceiling{0.f};
    float top{0.f};
};
[[nodiscard]] LocalHold localHold();
// SV_AirMove (vr_crouch_speed): the stick's most speed `maxspeed` for a client crouched (his eyes from updateCrouch under
// the tallest crouched box's top: down to vr_crouch_speed_scale of it 8 units under, never over 90% of the stealth AI's
// footsteps' speed, vr_stealth_run_speed), eased in and out over 0.25 s (one call a server frame); as it is standing.
[[nodiscard]] float crouchSpeedMax(edict_t* ent, float maxspeed);

// VR_ClientPreMove: vr_hull_walktest's random walk drives the first player (a test aid; nothing when not running).
void walkTestFrame(edict_t* ent);

} // namespace qvr::hull
