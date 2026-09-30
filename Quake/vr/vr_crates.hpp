// vr_crates.hpp -- wooden crates lying about the maps, and what monsters can't see through (docs/vr-port/ROUND21.md,
// "Wooden crates").
//
// At map load (QC vr_crates.qc, the first server frame, before the rocks and bricks and the baselines), crates are
// placed by walls, more in corners: along each wall face's bottom edge where a floor of the world meets it, a crate (a
// small one, 32 units, or a large one, 40 x 40 x 48) set against the wall, turned any way about the vertical and lying on
// any of its faces, sometimes another crate on it (turned and off its middle a little). Each place is checked: a flat
// floor of the world's own under all of it, nothing in its volume, open above, backed by the wall (not overhanging an
// opening), open floor in front of it (vr_crates_clearance: a passage it stands in stays passable), clear of every
// entity by a margin (vr_crates_margin; doors, lifts, teleporters and triggers more, the player's start most), and far
// from the other crates (vr_crates_spacing). Deterministic: a seed from the map's name (and vr_crates_seed).
//
// None in multiplayer, in the maps of vr_crates_exclude, or in a map whose worldspawn has "_vr_crates" "0" (another
// number scales the chance).
//
// Sight: QC's visible() asks sightblocked (vr_builtins.cpp) whether a monster's line of sight meets a solid prop that
// blocks it (.vr_blocksight: the crates, the explosive boxes), by its shape as it lies (Box3D's), or by its box for one
// Box3D doesn't move (an explosive box fixed in place: vr_explobox_physics 0).

#pragma once

#include "vr_engine.hpp"

namespace qvr::crates
{

// A new map is loading: the last one's plan goes (VR_OnSpawnServerBeforeLoad).
void reset();

// Plans the crates for the map now loaded (after its entities spawned, the crates' models precached); how many.
int plan();

// The model of crate `i` of the last plan ("" if none).
[[nodiscard]] const char* modelOf(int i);

// Puts crate `i` on `e` (its model set): its skin, turn, place resting on the floor or on the crate under it, its box, on
// the ground and still. Its kind: 1 small, 2 large; 0 if there is no such crate.
int put(edict_t* e, int i);

// A crate placed by the map or a test (QC vr_crate, its model set; its "angle" its yaw): level, resting on whatever is
// under its origin (within 128 units), its box, on the ground. Its kind: 1 small, 2 large; 0 if its model isn't a crate's.
int putPlaced(edict_t* e);

// Whether a monster's sight from `start` to `end` is blocked by a solid prop that blocks sight (.vr_blocksight > 0, not in
// a hand), not `ignoreA` nor `ignoreB` (edict numbers). The blocker's edict number, 0 if none.
[[nodiscard]] int sightBlocked(const glm::vec3& start, const glm::vec3& end, int ignoreA, int ignoreB);

} // namespace qvr::crates
