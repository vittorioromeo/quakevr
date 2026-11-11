// vr_flatswap.hpp -- flatscreen play's hand swap as drawn (vr_flatswap.cpp; his words 2026-11-10: "Give the hand swap
// button ('3') a short animation"; docs/vr-port/FLATPLAY.md, "The hand swap").
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatswap
{

// vr_flatswap_report (Debug > Reports > Flatscreen Play).
void init();

// The swap key's press (vr_flat_key_swap, from flatplay's keys): starts the hands' trip to each other, the swap itself
// (impulse 48, QC VR_FlatSwapHands) sent as they meet. False when there is nothing to draw (vr_flat_swap_anim 0, both
// hands empty, a hand carrying a thing, a reload under way): the caller sends the impulse at once, as before.
[[nodiscard]] bool press();

// Once a frame, from flatplay::frame (`on`: the mode runs and the fly camera isn't flying): the trip stepped, the swap sent
// when the hands meet, and seen arriving (the hands' weapons changed).
void frame(bool on);

// After flatplay's pose, the ironsight's and the reload's (flatplay::poseHands): the hands moved to their meeting point by
// the trip's share, and back.
void apply(hands::State& s);

// Whether a swap is drawn now: the hands' fire buttons do nothing until it is over (flatplay::routeFire).
[[nodiscard]] bool busy();

// Whether the hands are on their way to the swap, before it arrives: the free hand lets go of a two-handed grip
// (vr_flatsight.cpp, vr_twohand.cpp) and takes the other's after it.
[[nodiscard]] bool gripBlocked();

// Whether a swap of the hands' meeting kind is drawn (not a weapon in both hands): flatplay puts a hand that took a prop or
// a weapon straight into its new pose, and the swap eases it there.
[[nodiscard]] bool meeting();

} // namespace qvr::flatswap
