// vr_flatfgrab.hpp -- flatscreen play's force grab as drawn, and a held prop's pose (vr_flatfgrab.cpp; the grab itself is
// the game's: QC vr_wpnforcegrab.qc VR_Forcegrab_FlatGrab; docs/vr-port/FLATPLAY.md, "Force grab").
//
// E (vr_flat_key_grab) no longer pulls at once. The hand that will take it (the main one if it is free, else the off one,
// as the game chooses) first turns its palm to what it will pull and opens its fingers (vr_flat_fg_turn_time); a hand on a
// held gun's two-handed grip lets go of it first, smoothly, and comes away from the gun (vr_flat_fg_detach_time more).
// Then the grab is asked for, with that hand (impulse 57 the off hand, 58 the main), and the effect starts from its palm
// (vr_fgfx.cpp). The hand holds the pose, its palm on the object, while the object flies to it; caught or missed, it eases
// back to its own pose (vr_flat_fg_release_time). A hand pulling something takes no second grab until the first is over
// (his note 2026-11-10: two quick presses sent two objects at one hand); the next press goes to the other hand if it is free.
//
// A prop held in one hand (a box, a brick, a gib; not a weapon) is held up in front of him (vr_flat_carry_*), not hung at
// his side or closed into the guard's fist.
#pragma once

#include "vr_hands.hpp"

namespace qvr::flatfgrab
{

// vr_flat_fg_report (Debug > Reports > Flatscreen Play; the tests).
void init();

// E's press (flatplay::pollKeys): the hand is chosen and starts turning its palm out; nothing when neither hand is free.
void press();

// Once a frame, from flatplay::frame (`on`: the mode runs and the fly camera isn't flying): each hand's grab stepped (the
// turn's end sends the grab; the pull's end, from the server's STAT_QVR_FGMAIN / _FGOFF, starts the way back).
void frame(bool on);

// After flatplay's pose, the ironsight's and the reload's (flatplay::poseHands): a hand on its grab moved and turned to its
// palm pose by its share, and a hand holding a prop to the carry pose by its own. `aim`: the view's angles.
void apply(hands::State& s, const glm::vec3& aim);

// Whether hand `h` is on a grab (turning out, or holding the pose while its object flies): it lets go of a gun's grip
// (vr_flatsight.cpp, vr_twohand.cpp) and takes no second grab.
[[nodiscard]] bool busy(int h);

// How open hand `h` is held by its grab, 0..1 (the guard's fist and the punch close it: flatplay::fistShare).
[[nodiscard]] float openShare(int h);

// How far hand `h` is in the carry pose, 0..1 (a prop in it: not the guard's fist either).
[[nodiscard]] float carryShare(int h);

} // namespace qvr::flatfgrab
