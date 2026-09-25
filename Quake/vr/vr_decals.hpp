// vr_decals.hpp -- blood and scorch marks on the world (vr_decals): blood splats on the floor under
// and the walls beside what bleeds, drops along the floor under flying gibs, scorch marks where
// things explode, chips where bullets and nails hit.
//
// Each is a quad lying on the surface found by a trace (the static world only: none on doors or
// lifts), turned at random, shrunk or dropped where it would hang over an edge. They darken what is
// under them (a modulating blend: the surface times the mark), so they take the light of where they
// are with no lighting of their own. The marks are drawn once at start-up into an atlas (splats,
// drops, scorches, chips; several of each). At most vr_decal_max are kept (the oldest go first),
// each fading out at the end of vr_decal_life seconds. One draw a frame in each eye, in the opaque
// pass (VR_DrawSceneOpaque), depth-tested.

#pragma once

#include "vr_particles.hpp"

#include <glm/glm.hpp>

namespace qvr::decals
{

// A Quake VR particle effect at `org` along `dir` (may be zero).
void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count);

// Drawn in each eye (VR_DrawSceneOpaque).
void draw();

// All gone (a new map).
void clear();

// vr_decal_count: how many there are, of each kind.
void count_f();

} // namespace qvr::decals
