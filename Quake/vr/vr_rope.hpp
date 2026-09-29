// vr_rope.hpp -- the grappling hook's rope drawn (docs/vr-port/ROUND21.md, "Grapple: unreel; rope drawn in one piece").
//
// The rope is Rogue's chain beam (progs/beam.mdl's links, a quarter size), from the gun to the hook. Taut, a straight
// line; slack (vr_grapple_sag), it hangs along the parabola of its length under the line between its ends, lying on
// the floor where it would go through it. The curve is sampled as finely as its sag and length need (a short, deep one
// finely: its bend at each sample a few degrees at most, the chord a twentieth of a unit from the curve at most), and
// the links are drawn end to end along it, each bent with it on the GPU (gfx::drawBent), so that the rope is one piece
// with no gaps and no straight pieces, however it curves. The links' look is the model's own (its skin, fullbright).

#pragma once

namespace qvr::rope
{

// VR_ParseBeamEntity: a grappling hook's rope beam (key: its entity and beam id) came with its slack (0 .. 1: the
// share of its length that hangs).
void setSlack(int key, float slack);

// QVR_SVC_ROPE: a rope's simulated points (the server's, vr_ropesim.cpp), for its beam: drawn through them.
void parsePoints();

// At sign-on: every rope forgotten.
void forget();

// VR_DrawSceneOpaque, in each view: this frame's ropes.
void drawOpaque();

} // namespace qvr::rope
