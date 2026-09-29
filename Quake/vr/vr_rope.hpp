// vr_rope.hpp -- the grappling hook's rope drawn (docs/vr-port/ROUND21.md, "Grapple: unreel; rope drawn in one piece").
//
// The rope is Rogue's chain beam (progs/beam.mdl's links, a quarter size), from the gun to the hook. Taut, a straight
// line; slack (vr_grapple_sag), it hangs: with the physical rope (vr_grapple_rope_sim) as a chain of points pinned at
// the gun, the corners the server's rope wraps round and the hook, falling and lying on the world (its points and pieces
// traced: never through it); without, along the parabola of its length under the line between its ends, lying on the
// floor where it would go through it. The curve is sampled as finely as its sag and length need (a short, deep one
// finely: its bend at each sample a few degrees at most, the chord a twentieth of a unit from the curve at most), and
// the links are drawn end to end along it, each bent with it on the GPU (gfx::drawBent), so that the rope is one piece
// with no gaps and no straight pieces, however it curves. The links' look is the model's own (its skin, fullbright).

#pragma once

namespace qvr::rope
{

// VR_ParseBeamEntity: a grappling hook's rope beam (key: its entity and beam id) came with its slack (0 .. 1: the
// share of its length that hangs).
void setSlack(int key, float slack);

// QVR_SVC_ROPE: the corners a rope wraps round (the server's, vr_ropesim.cpp), for its beam: its slack drawn hanging
// between them (a chain of points, vr_grapple_rope_spacing apart, lying on the world, never through it).
void parseCorners();

// vr_debug_rope: the drawn ropes' chains (points, pieces), their corners and taut paths, in the world (VR view frame).
void debugDraw();

// vr_grapple_rope_draw_dump (tests): each drawn rope's chain, points inside the world, pieces through it.
void drawDump_f();

// At sign-on: every rope forgotten.
void forget();

// VR_DrawSceneOpaque, in each view: this frame's ropes.
void drawOpaque();

} // namespace qvr::rope
