// vr_decals.hpp -- blood and scorch marks on the world (vr_decals): blood splats on the floor under
// and the walls beside what bleeds, scorch marks where things explode, chips where bullets and
// nails hit. Gibs and heads bleed too (vr_gib_blood, VR_GibTrail): a trail of drops on the floor
// under them, a splat where they hit a wall or the floor, and Quake VR's blood trail behind them.
//
// Each is a quad lying on the surface found by a trace (the static world only: none on doors or
// lifts), turned at random, shrunk or dropped where it would hang over an edge. They multiply what
// is under them by 0..2 (a modulating blend that can darken and brighten), so they take the light
// of where they are with no lighting of their own. The marks are drawn once at start-up into a
// mipmapped atlas (splats, drops, scorches, chips; several of each). A chip is a dent: a crater, a
// rim and cracks, their relief baked in as lit from one side, the decal turned so that side faces
// the map's strongest light that reaches it (else up). Hipnotic's low-resolution bullet hole
// sprites become chips too (VR_BulletHoleSprite). At most vr_decal_max are kept (the oldest go first),
// each fading out at the end of vr_decal_life seconds. One draw a frame in each eye, in the opaque
// pass (VR_DrawSceneOpaque), depth-tested.

#pragma once

#include "vr_particles.hpp"

#include <glm/glm.hpp>

namespace qvr::decals
{

// A Quake VR particle effect at `org` along `dir` (may be zero).
void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count);

// A small drop of blood on the floor (or a gentle slope) just below `org`, `size` units across.
void drop(const glm::vec3& org, float size);

// A bullet's chip on the surface nearest `org` (Hipnotic's bullet hole sprites, VR_BulletHoleSprite).
void chip(const glm::vec3& org);

// Drawn in each eye (VR_DrawSceneOpaque).
void draw();

// All gone (a new map).
void clear();

// vr_decal_count: how many there are, of each kind.
void count_f();

} // namespace qvr::decals
