// vr_flail.hpp -- AD support: the death sergeant's flail drawn in the hands (QC vr_ad_melee.qc VR_ADMELEE_FLAIL): its
// handle is the hand's weapon model (progs/w_blueflail.mdl#melee); its spiked head (progs/w_blueflail.mdl#flailhead,
// vr_admelee.cpp) where the physics has it (vr_box3d_flail.cpp, a listen server's), turned with its ring towards its
// chain; the chain from the handle's end as drawn (the hand's muzzle) to the ring, the flashlight's chain of links
// (vr_coil.cpp, rigid iron), lit, depth-tested in each eye's opaque scene.

#pragma once

#include "vr_coil.hpp"
#include "vr_hands.hpp"
#include "vr_view.hpp"

namespace qvr::flail
{

// Its chain's look (blue steel links; the rigid chain, the chain's length vr_flail_chain): the hands' and the flails
// lying about's (vr_adtank.cpp's world lines).
[[nodiscard]] coil::Style chainStyle();

// A flail lying about (QC vr_ad_melee.qc "The flail lying about"): its chain's ends as drawn, the handle's tip and the
// way the chain leaves it (along the handle), the head's ring and the way the chain leaves it (out of the ball), world
// units. False: no flail made (AD's data not there) or not drawn.
[[nodiscard]] bool lyingEnds(const view::ViewEntity& handle, const view::ViewEntity& head, glm::vec3& a, glm::vec3& aDir,
    glm::vec3& b, glm::vec3& bDir);

// Once a frame with the view's effects (after the weapons are placed): each hand's head placed and drawn, its chain
// stepped.
void frame(const hands::State& s);

// The chains, in each eye's opaque scene.
void drawOpaque();

} // namespace qvr::flail
