#pragma once

// vr_flataimsens.hpp -- the look's speed in the flatscreen ironsight (his words 2026-11-09: "a flat ADS multiplier plus
// ADS sensitivity setting with a match-distance option"). in_sdl.c IN_FovScale slows the mouse and the gyro by the
// zoom's tangent ratio, tan(sighted fov / 2) / tan(fov / 2): a mouse move puts the same point near the centre of the
// screen on the crosshair sighted as at hipfire. vr_flat_aim_sens_match moves that point out (a monitor distance match:
// so many half screen heights from the centre), vr_flat_aim_sens multiplies on top; both come in with the ironsight's
// travel, as its field of view does, and neither does anything outside flatscreen play or unsighted.

namespace qvr::flataimsens
{

void init(); // vr_flat_aim_sens_test, vr_flat_aim_sens_status

// Once a frame from flatplay::frame: vr_flat_aim_sens_test's next step, when one runs.
void frame();

// The look's scale sighted against hipfire for a view drawn at `lookFov` whose hipfire is `hipFov` (Quake's 4:3
// horizontal fovs, as scr_fov): the zoom's tangent ratio at vr_flat_aim_sens_match 0, the match's ratio of angles
// otherwise; times vr_flat_aim_sens over the travel `travel` (0..1).
[[nodiscard]] float scaleFor(float lookFov, float hipFov, float travel);

// What IN_FovScale multiplies its tangent ratio by (VR_FlatAimSens): scaleFor over that ratio. Exactly 1 outside
// flatscreen play, unsighted, and at vr_flat_aim_sens 1 with vr_flat_aim_sens_match 0.
[[nodiscard]] float lookFactor(float lookFov, float hipFov);

// The stick's look (in_sdl.c IN_JoyMove; VR_FlatAimStickScale): scaleFor against the ironsight's own hipfire fov, the
// mouse's slowing for a look that has none of its own. Exactly 1 outside flatscreen play and unsighted.
[[nodiscard]] float stickScale(float lookFov);

// The menu's readout: the scale with the travel all done at fov `hipFov` (vr_flat_aim_fov's sighted fov).
[[nodiscard]] float sightedScale(float hipFov);

} // namespace qvr::flataimsens
