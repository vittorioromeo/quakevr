// vr_gore.hpp -- over-the-top gore (vr_gore; after Brutal Doom's): blood sprayed onto the walls,
// floor and ceiling behind what is hit, big splats and a burst of blood all round where a monster is
// gibbed or a gib bursts, pools of blood spreading under corpses, runs of blood down walls, and
// blood dripping from splats on the ceiling and from gibs stuck there (the QC sticks gibs flung
// into a ceiling or a wall for vr_gore_stick seconds). While wounded, blood also drips round the
// player's feet (vr_body_blood_floor). All client-side marks (vr_decals.cpp), from events the QC
// sends as particle2 presets (vr_carry.qc) and from what the client sees of gibs (VR_GibTrail).
//
// Cheap and capped: the lines of blood a hit or a burst throws are traced a few a frame (they show
// as they would arrive: a little later the further they flew); a drop is one trace when it starts
// to fall (its landing is known from it; the falling drop is a particle), its mark rate-limited;
// at most a few dozen drip sources and pending pools. The marks count against vr_decal_max and
// expire with vr_decal_life.

#pragma once

#include <glm/glm.hpp>

namespace qvr::gore
{

// The QC's gore events: particle2 presets far above Quake VR's particle presets.
//   EventHit:    `org` the hit, `dir` the blow's way (times 7), `count` the damage + 1000 x its kind
//                (0 a shot or a missile, 1 a shotgun blast, 2 an explosion's, 3 a hand's blow).
//   EventBurst:  a monster gibbed or a gib burst at `org`, `dir` the blow's way (times 7, or zero),
//                `count` how big (10: a gib's).
//   EventCorpse: a monster died whole at `org` (the corpse is the entity there), `dir.x * 8` its size.
constexpr int EventHit = 40;
constexpr int EventBurst = 41;
constexpr int EventCorpse = 42;

// A particle2 message (parseParticle2): true if it was one of the gore's events (drawn as marks
// only), false for the particles to draw it.
bool event(const glm::vec3& org, const glm::vec3& dir, int preset, int count);

// vr_gore, 0..2 (0: Quake VR's blood only).
[[nodiscard]] int level();

// VR_GibTrail: a gib hit the world at `where` going at `velocity`, turned by `strength` units/s: a
// big splat, a run down a wall, drips from a ceiling. False with vr_gore 0 (the old splat then).
bool gibImpact(const glm::vec3& where, const glm::vec3& normal, float strength, const glm::vec3& velocity);

// A gib came to rest on the floor at `where`: a small pool spreads under it.
void gibRest(const glm::vec3& where, const glm::vec3& normal);

// Every frame a gib hangs still with nothing under it (stuck to a ceiling or a wall, or held),
// entity `ent` at `org`: it drips (until it bleeds dry).
void gibHanging(int ent, const glm::vec3& org);

// Once a frame (decals::draw): the queued lines of blood, drips, landings, pools and the player's.
void frame();

// All gone (a new map).
void clear();

// vr_decal_count: what the gore has going.
void count();

// vr_gore_test [damage | burst | corpse]: as if a monster 64 units ahead were shot from here (40
// damage), gibbed, or died (a pool).
void test_f();

} // namespace qvr::gore
