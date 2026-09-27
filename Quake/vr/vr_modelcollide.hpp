// vr_modelcollide.hpp -- held weapons and hands against models (round 21): a weapon held into a monster, a corpse,
// another player or a thing on the ground (a box, a gib, a weapon lying there) is drawn stopped at the model's surface
// -- its triangles as they are drawn this frame, not its box -- as walls already stop it (vr_handpose.cpp).
//
// The test (vr_modelcollide.cpp; on the main thread, no precomputation beyond a model's triangle list): the weapon as
// drawn is rays from the hand to a couple of dozen of its own vertices (spread over it: farthest-point samples of its
// model's pose), and one from 16 units behind the hand (towards the chest) to it: the hand itself in the model. Each
// ray is tested against the triangles of the models near it, posed as the renderer draws them this frame (the lerp
// between their two poses, and their movement's), Moller-Trumbore. A ray that goes into a model's surface (its front)
// makes a plane there: the part of the ray inside, up to where it comes out again (or its end), must go back out of
// that plane. The hand's push is the least move out of all the planes (Gauss-Seidel), tested again from there (at
// most three times: a curved surface). Up to vr_model_collide_max it is all of it; deeper, less and less; none at
// twice as deep (it lets go: a monster walking into you pushes the gun through it rather than away without limit).
// Eased in quickly and out slower.
//
// Drawn only: the hand, the weapon, the fingers and the arm are drawn moved; the game reads the tracked hands and
// muzzles as before (the melee's contacts and kinds, the shots, the aim, the two-handed grips): a blade stopped at a
// monster's surface still hits it as a blade through it did.

#pragma once

#include "vr_hands.hpp"

struct entity_s;

namespace qvr::modelcollide
{

// The view (VR_SetupViewEntities), before the weapons and hands are placed: each hand moved by its push (`s.pos`), so
// that its weapon, its fingers and its arm are drawn out of the models. Once per frame; again in the same frame (a
// redraw), the same push.
void beginView(hands::State& s);

// After they are placed: the push taken back out of what the game reads (the hands, the muzzles and the two-handed
// grips as tracked), and each hand's weapon as it was drawn (`weapon[hand]`, mirrored or not) kept for the next
// frame's test (its model's pose, and where it is in the hand).
void endView(hands::State& s, const entity_s* const weapon[2], const bool mirrored[2]);

// How far `hand`'s weapon and hand are drawn from where they are tracked (world units; zero when nothing stops them):
// what is drawn at the muzzle (a flash, a beam's start) goes there too.
[[nodiscard]] glm::vec3 drawnOffset(int hand);

// A new map: nothing pushed, nothing recorded.
void reset();

// vr_model_collide_bench [n]: the test of both hands as they are now, n times (1000): min, median and max
// microseconds, and what it tested (models near, their triangles near the weapon, rays, rounds).
void bench_f();

} // namespace qvr::modelcollide
