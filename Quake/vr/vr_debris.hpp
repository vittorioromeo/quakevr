// vr_debris.hpp -- rocks and bricks lying about the maps (docs/vr-port/ROUND21.md, "Rocks and bricks").
//
// At map load, after the map's own entities (QC vr_debris.qc, the first server frame, before the baselines), pieces are
// placed at the foot of walls, where a floor meets them: along each wall face's bottom edge, more in corners. What the
// textures there are made of decides what lies there (materialOf: a name-pattern table that holds for id's textures and
// for QRP's, which keep id's names): rocks on natural floors (grass, dirt, ground, rock) and at the foot of rock and
// stone walls; bricks at the foot of brick walls. Each spot is checked (a flat floor of the world's own, open air above,
// no liquid, room round the piece, clear of doors, lifts, teleporters, triggers, items, monsters and starts) and the
// pieces vary in model, size, turn and skin (the skin nearest the colour of the texture it lies by). Deterministic: a
// seed from the map's name (and vr_debris_seed), the same layout at every load.
//
// Limits: vr_debris_max a map, vr_debris_area_max an area, vr_debris_spacing between spots, and never more than leaves
// vr_debris_edicts_left entities free. None in multiplayer (a remote client's packets are 1400 bytes), in the maps of
// vr_debris_exclude, or in a map whose worldspawn has "_vr_debris" "0" (another number scales the chance).

#pragma once

#include "vr_engine.hpp"

namespace qvr::debris
{

// What a texture is made of, by its name.
enum class Material : int
{
    None,       // not a surface (sky, liquids, trigger, clip)
    Natural,    // ground, grass, dirt, mud, sand, gravel, moss, rock (rock1_2, rock4_1): rocks lie on it and by it
    Fieldstone, // walls of rough stones, cobbles (wall14_*, church1_2, city6_7): rocks at their foot
    Masonry,    // dressed stone blocks (wiz1_*, stone*, wswamp2_*, city4_*): a few rocks at their foot
    Brick,      // brick walls (*brick*, city2_*, city1_4): bricks at their foot
    Metal,      // metal, tech bases, computers, doors, lights: nothing
    Wood,       // wood: nothing
    Other,      // the rest: nothing
};

[[nodiscard]] Material materialOf(const char* texture);
[[nodiscard]] const char* materialName(Material m);

// Whether this map gets pieces (vr_debris, single player, not excluded, its worldspawn's "_vr_debris").
[[nodiscard]] bool enabledHere();

// A new map is loading: what was gathered for the last one goes (VR_OnSpawnServerBeforeLoad).
void reset();

// The new map's server is up (VR_OnSpawnServerAfterLoad): vr_debug_debris prints how long it took.
void afterLoad();

// Places the pieces for the map now loaded (after its entities spawned); how many.
int plan();

// The model of piece `i` of the last plan ("" if none).
[[nodiscard]] const char* modelOf(int i);

// Puts piece `i` on `e` (its model set, precached): its skin, size, turn, and its place resting on the floor, its box,
// on the ground. Its kind: 1 a rock, 2 a brick; 0 if there is no such piece.
int put(edict_t* e, int i);

// A piece placed by the map (QC vr_debris_piece; its model set, precached, one of the rocks' or bricks'; its skin and
// "angle" set): sized as the pieces lying about, level, turned by its yaw, resting on the floor at its origin (its lowest
// corner there), its box, on the ground. Its kind: 1 a rock, 2 a brick; 0 if its model is neither.
int putPlaced(edict_t* e);

} // namespace qvr::debris
