// vr_brainjiggle.hpp -- brain chunks wobble when they hit something (Vittorio, 2026-10-19: the brain chunks "squishier":
// "a cheap squish/jiggle (vertex wobble on impact)").
//
// Drawn only: the physics' body (Box3D, its hull the model's) stays rigid. Each brain chunk drawn (progs/gib_brain*.mdl,
// make_brains.py) has its speed followed from where it is drawn frame to frame; a sudden change of it (a landing, a
// bounce, a hand's jerk: faster than vr_smallgibs_brains_jiggle_speed m/s) kicks a damped spring: the chunk squashed
// along the change (flattened on the floor it landed on) and bulged across it, its volume kept, overshooting into a
// stretch and settling in about half a second. The kick grows with the change, times vr_smallgibs_brains_jiggle (0 off).
// The springs are kept only while they wobble or their chunk is drawn (a map change clears them).

#pragma once

#include "vr_engine.hpp"

namespace qvr::brainjiggle
{

// VR_AliasPreTransform: `e`'s world matrix squashed and bulged about its origin by its spring, if a brain chunk (else
// untouched); its spring stepped to cl.time first (once a time, whichever pass draws it first).
void squashModel(const entity_t* e, float matrix[16]);

// vr_brain_jiggle_info: the chunks followed, those wobbling, the kicks and the most squash since the last.
void registerCommands();

} // namespace qvr::brainjiggle
