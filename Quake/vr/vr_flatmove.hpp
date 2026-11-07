// vr_flatmove.hpp -- flatscreen play's slide and lean (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Sliding" and
// "Leaning"; ROUND24.md, "Flat-screen slide and lean").
//
// The slide (vr_flat_slide, vr_flat_slide_*): crouching while running faster than vr_flat_slide_min starts it, on the
// server (VR_FlatSlideMove, from SV_AirMove): his speed boosted, then no walking and no ground friction but its own,
// a slope pulling him down it, until he is slower than vr_flat_slide_end, lets go of the crouch or jumps (a jump keeps the
// speed: Quake's air move). The client reads the local server's word (sliding) for the view's extra drop and the body's
// pose (vr_avatar_pitch.cpp: the trunk leant back, the legs out in front).
//
// The lean (vr_flat_lean, vr_flat_lean_*): Q / E hold (or toggle) a lean: the eye moves sideways (no further than a
// trace at its side lets it, vr_flat_lean_margin off the wall), and down as it swings, the view and the held weapon roll,
// the body tilts from the hips to the eye (the pose's head and its lean: vr_hands.cpp placeFlat). Everything downstream --
// the hands, the crosshair's ray, the ironsight, the wall's tilt (vr_flatsight.cpp) -- reads the leant head, so it all
// moves as one. The server meets shots and missiles with the leant upper body (VR_LeanHitBoxes), never his box against
// the world.

#pragma once

#include "vr_engine.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace qvr::flatmove
{

// VR_Init: the cvars' commands (+flatleanleft, ..., vr_flatslide_selftest, vr_flatslide_test, vr_flatlean_test,
// vr_flatmove_status).
void init();

// flatplay::frame, every frame the mode runs (after its keys): the lean keys, the lean's ease and its wall clamp, the
// slide's eased share, the running tests. `on` false: the mode is not running (everything back to rest).
void frame(bool on);

// The lean as the pose takes it this frame, world units: the eye's move (sideways and down) and the view's roll
// (degrees, + right). Zero outside the mode and standing straight.
[[nodiscard]] glm::vec3 leanShift();
[[nodiscard]] float leanRoll();
// The lean's signed share now, -1 (full left) .. 1 (full right), the wall's clamp in it (the tests, the status).
[[nodiscard]] float leanShare();

// The local player sliding (the local server's word; a client of another server: his crouch held at a slide's speed
// on the ground), and the slide pose's eased share 0..1 (the view's extra drop: vr_flat_slide_drop).
[[nodiscard]] bool sliding();
[[nodiscard]] float slideShare();
// How much further down the slide puts his eyes now (units; flatplay::crouchDrop adds it).
[[nodiscard]] float slideDrop();
// How far behind the eye the slide's leant-back trunk hangs from (units: vr_flat_slide_neck_back; the avatar takes it by
// its slide share). 0 outside the mode.
[[nodiscard]] float slideNeckBack();

// One step of the slide on the ground (the self-test drives it directly): the speed along the floor `vel`, the floor's
// normal, the move keys' way (unit, or zero), seconds. Out: the new speed along the floor; `ended` when it is slower
// than vr_flat_slide_end.
struct SlideStep
{
    glm::vec2 vel{0.f};
    bool ended{false};
};
[[nodiscard]] SlideStep slideStep(const glm::vec2& vel, const glm::vec3& normal, const glm::vec2& wish, float dt);

} // namespace qvr::flatmove
