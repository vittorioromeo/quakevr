// vr_flail.hpp -- AD support: the death sergeant's flail drawn in the hands (QC vr_ad_melee.qc VR_ADMELEE_FLAIL): its
// handle is the hand's weapon model (progs/w_blueflail.mdl#melee); its spiked head (progs/w_blueflail.mdl#flailhead,
// vr_admelee.cpp) where the physics has it (vr_box3d_flail.cpp, a listen server's), turned with its ring towards its
// chain; the chain from the handle's end as drawn (the hand's muzzle) to the ring, the flashlight's chain of links
// (vr_coil.cpp, rigid iron), lit, depth-tested in each eye's opaque scene.

#pragma once

#include "vr_hands.hpp"

namespace qvr::flail
{

// Once a frame with the view's effects (after the weapons are placed): each hand's head placed and drawn, its chain
// stepped.
void frame(const hands::State& s);

// The chains, in each eye's opaque scene.
void drawOpaque();

} // namespace qvr::flail
