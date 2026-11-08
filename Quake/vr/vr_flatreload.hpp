// vr_flatreload.hpp -- flatscreen play's reload as drawn: the gun's hand down to the nearest hip holster and back
// (vr_flatreload.cpp; the game's side is QC vr_flatreload.qc; docs/vr-port/FLATPLAY.md, "Reloading").
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatreload
{

// vr_flatreload_report, vr_flat_reload_pose (Debug > Reports > Flatscreen Play; the tests).
void init();

// Once a frame, from flatplay::frame (`on`: the mode runs and the fly camera isn't flying): R's press (vr_flat_key_reload:
// impulse 55), and each hand's trip stepped from the server's STAT_QVR_FLATRELOAD.
void frame(bool on);

// After flatplay's pose and the ironsight's (flatplay::poseHands): each hand on its trip moved to its hip holster and
// turned muzzle-down by its share of the trip.
void apply(hands::State& s);

// Whether hand `h`'s gun is on its trip (the server's bit, or the drawn hand still coming back): it doesn't fire, the
// ironsight waits, and the free hand lets go of its grip and hangs relaxed (vr_flatsight.cpp, vr_twohand.cpp).
[[nodiscard]] bool busy(int h);

} // namespace qvr::flatreload
