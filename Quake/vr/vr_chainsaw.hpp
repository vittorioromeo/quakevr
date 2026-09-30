// vr_chainsaw.hpp -- the ogres' chainsaw's starter cord, on the client (QC vr_chainsaw.qc runs the engine, its fuel
// and its cuts; docs/vr-port/ROUND21.md, "The ogres' chainsaw").
//
// A chainsaw (progs/v_chainsaw.mdl, Misc/quakevr/make_chainsaw.py) in one hand: the other, empty, hand's grip at the
// cord's T-handle on its top takes it (the game does not see that grip). Held, the handle is drawn in the fist and the
// cord from its hole to it (a plain cable: vr_coil.cpp); the model shows its frame without the handle (the server sets
// it from the move). Pulled out past vr_chainsaw_pull_distance (cm) at vr_chainsaw_pull_speed (m/s) or faster along
// the cord, that is a pull: the move says so for 0.15 s (QC .sawcord), and the server may start the engine
// (vr_chainsaw_start_chance). A slower pull past it is only a weak one. The hand must come back towards the hole
// before the next. Let go (or pulled out past the cord's length), the handle flies back into its seat.
//
// The handle is read from the model as it loads: the vertices that differ between its frames 0 and 9 are the handle
// (their middle in frame 0: where the hand takes it), and where they meet in frame 9 is the cord's hole.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

#include <cstdint>

namespace qvr::chainsaw
{

void init(); // commands (vr_chainsaw_fit)

// Once per frame, as the view is set up, after the weapons and the flashlight are placed: the cord, the pull.
void setupView(const hands::State& s);

// The cord and the handle in the hand, in each eye's opaque scene: lit, depth-tested.
void drawOpaque();

// The grip of `hand` pressed or released (vr_input.cpp): true when the cord takes it (the game does not see it). A press
// it takes, it also takes the release of.
[[nodiscard]] bool grip(int hand, bool pressed);

// Whether `hand` holds a chainsaw's cord (a busy hand: no force grab).
[[nodiscard]] bool holds(int hand);

// The move's cord bits (QC .sawcord, VR_SAWCORD_*): which hand holds the other's cord, and a pull.
[[nodiscard]] std::uint8_t moveBits();

// vr_modelcollide.cpp: how deep (units) the bar of the chainsaw `model` drawn in `hand` may sink into a monster's model
// now (its chain running: the server's QVR_WPNFLAG_SAW_CHAIN; vr_chainsaw_overlap), 0 none; and where the bar starts
// along the model's x (its vertices further out are the bar's).
[[nodiscard]] float sinkDepth(int hand, const qmodel_t* model);
[[nodiscard]] float barStart(const qmodel_t* model);

// A new map, a disconnect: the cord let go.
void reset();

} // namespace qvr::chainsaw
