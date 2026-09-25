// vr_fgfx.hpp -- force grab's look (vr_wpnforcegrab.qc does the grabbing): the object a hand
// points at glows softly round its edges (vr_forcegrab_outline), fading in and out; a faint beam
// runs from the hand to it; locked on, a crackling tendril of energy joins them; pulled, the tendril
// follows the object and it leaves a sparkling trail. The server sends each hand's target and state
// (STAT_QVR_FGMAIN, STAT_QVR_FGOFF: entity * 4 + state).

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::fgfx
{

// Once a frame, before the eyes: fades the glows, queues the beams and the trail.
void queue(const hands::State& s);

// How much `e` glows (0..1), for the renderers.
[[nodiscard]] float entityGlow(const entity_t* e);

} // namespace qvr::fgfx
