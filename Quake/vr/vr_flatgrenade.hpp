// vr_flatgrenade.hpp -- flatscreen play's quick grenade as drawn: G's key, and the hand to the back pouch, back into view
// with the grenade, up into the overhead wind-up and out along the throw (vr_flatgrenade.cpp; the game's side is QC
// vr_flatgrenade.qc; docs/vr-port/FLATPLAY.md, "The quick grenade").
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatgrenade
{

// vr_flatgrenade_report (Debug > Reports > Flatscreen Play).
void init();

// Once a frame, from flatplay::frame (`on`: the mode runs and the fly camera isn't flying): G's press
// (vr_flat_key_grenade: impulse 57), and the hand's shares stepped from the server's STAT_QVR_FLATGREN.
void frame(bool on);

// After flatplay's pose, the ironsight's and the reload's (flatplay::poseHands): the grenade's hand moved to the pouch, the
// held place, the overhead wind-up or the follow-through by their shares.
void apply(hands::State& s);

// The hand holding a grenade now (the server's phase 2..4), -1 none: its fire button is the grenade's
// (flatplay::routeFire: LMB the main hand, RMB the off hand).
[[nodiscard]] int heldHand();

// Whether hand `h` is busy with a grenade (on its way to the pouch, holding one, its throw's follow-through): it neither
// punches nor takes a weapon's grip (vr_flatsight.cpp, vr_twohand.cpp).
[[nodiscard]] bool busy(int h);

} // namespace qvr::flatgrenade
