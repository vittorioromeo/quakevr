// vr_flattp.hpp -- flatscreen play's over-the-shoulder third-person camera (vr_flat_tp; docs/vr-port/FLATPLAY.md, "The
// third-person camera"; his request 2026-11-15: "an optional over-the-shoulder third-person camera for the flat-screen
// version, OFF by default").
//
// The camera stands off the eye (the pivot) behind, to one shoulder and up, in the frame of the look (yaw and pitch: it
// orbits the head as he looks up and down), looking the same way the eye looks. A fat ray (five lines, the probe radius
// apart) is swept from the eye to that spot and the camera pulled in to the first hit less the probe: at once (it is
// never further out than the sweep allows, so it never shows the inside of a wall) and back out eased (vr_flat_tp_ease_out).
// The near shoulder blocked, it swaps to the other one (vr_flat_tp_auto_swap), eased (vr_flat_tp_ease_side). Close to the
// head the body, hands and weapons fade (vr_flat_tp_fade_*). The aim: the crosshair's point is what the camera's ray hits
// (from the pivot's depth on), and the guns point at it from their muzzles as they always do (flatsight's aim at the
// point), so shots go where the crosshair is; a muzzle that can't reach that point shows it on the visor crosshair, which
// traces the shot itself. Aiming down the sights: back to his own eyes over the ironsight's travel (vr_flat_tp_aim 0), or
// the camera kept, drawn in over the shoulder (vr_flat_tp_aim 1).
//
// Flatscreen play only, and only with vr_flat_tp on, alive, out of the fly camera and the orbit aid: a headset and the
// option off draw exactly what they drew before (every entry point returns at once).

#pragma once

#include <glm/vec3.hpp>

namespace qvr::flattp
{

// The camera can be up this frame: flatscreen play, vr_flat_tp on, alive, no intermission, fly camera or orbit aid.
[[nodiscard]] bool on();

// The drawn view stands off his eyes (the camera's offset this frame is more than a hair): the hands and the body stay on
// the body (flatplay::viewIsHisOwn says no), the body is drawn with its head. False with the camera off, and once the
// ironsight has brought the camera back into his eyes (vr_flat_tp_aim 0).
[[nodiscard]] bool away();

// The camera for an eye at `eye` looking at `aim`, as this frame's state has it (side, how far out): not stepped. `eye`
// itself while the camera is not up.
[[nodiscard]] glm::vec3 camera(const glm::vec3& eye, const glm::vec3& aim);

// Where the crosshair's ray starts for that camera: its point level with the eye along the way it looks (`dir`), so nothing
// between the camera and him is ever aimed at. `eye` while the camera is not up.
[[nodiscard]] glm::vec3 rayStart(const glm::vec3& eye, const glm::vec3& aim, const glm::vec3& dir);

// The field of view's multiplier (vr_flat_tp_fov), 1 when not up and gone over the ironsight's travel back into his eyes.
[[nodiscard]] float fovScale();

// The opacity the body, the hands and the weapons are drawn with: 1 unless the camera is near his head (vr_flat_tp_fade_*).
[[nodiscard]] float bodyAlpha();

// V_CalcRefdef (VR_FlatThirdPersonView): the drawn view moved out to the camera, the state stepped. `bob`: the walk's bob
// V_CalcRefdef put into the eye, which the camera leaves out.
void view(float* vieworg, const float* viewangles, float bob);

// Once a frame (flatplay::frame): the field of view's recalculation asked for when its scale moves, the test's step.
void frame(bool on);

// vr_flat_tp_toggle, vr_flat_tp_status, vr_flat_tp_test.
void init();

} // namespace qvr::flattp
