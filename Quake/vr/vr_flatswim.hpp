// vr_flatswim.hpp -- flatscreen play's swimming as drawn: the hands' strokes under water (vr_flatswim.cpp;
// docs/vr-port/FLATPLAY.md, "Swimming").
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatswim
{

// vr_flatswim_report, vr_flatswim_probe (Debug > Reports > Flatscreen Play; the tests).
void init();

// Whether the local flat-screen player swims now (Quake's own swimming: the water at his waist off the bottom, or over
// his head) and the strokes are on (vr_flat_swim). The hands' two-handed grips and stances let go while it is true
// (vr_flat_swim_one_hand: vr_flatsight.cpp, vr_twohand.cpp, flatplay's melee stance), so the free hand can swim; they
// come back on leaving the water.
[[nodiscard]] bool swimming();
[[nodiscard]] bool freesHand();

// Once a frame, from flatplay::poseHands before its hands' loop (`dt` > 0: the frame's first pose): the stroke's cycle,
// its kind from the way he moves (forward, back, sideways, up, down; treading still), each hand's share.
void step(float dt, const glm::vec3& aim);

// From flatplay::poseHands' loop, over hand `h`'s pose (after the hang and the melee poses, before the wall, the
// ironsight, the reload, the grabs and the swap, which all take over from it): a free hand strokes, a hand holding a
// weapon only follows the stroke's motion a little (vr_flat_swim_gun_share), its aim kept. `shoulder` is the hand's
// shoulder (world), `span` the arm's reach (units). `measure`: the frame's first pose (the probe reads it).
void poseHand(hands::State& s, int h, const glm::vec3& aim, const glm::vec3& shoulder, float span, bool measure);

// Hand `h`'s share of the stroke now (0..1, eased: the water's and the take-over's).
[[nodiscard]] float share(int h);

} // namespace qvr::flatswim
