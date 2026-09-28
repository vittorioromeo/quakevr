// vr_window.hpp -- what the desktop window shows while in VR, for recording (vr_window_view): the left eye as it is,
// the smoothed mirror (the eye's image turned to a steadied head and cropped to the window) or the spectator camera
// (the scene rendered a third time for the window from the head, steadied). ROUND21.md, "Recording: smoothed mirror
// and spectator camera".

#pragma once

#include "vr_backend.hpp"
#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::window
{

enum class View
{
    Off,       // vr_mirror 0
    Raw,       // the left eye (or both, vr_mirror 2) as they are
    Smoothed,  // the smoothed mirror
    Spectator, // the spectator camera
};

// This frame's (set by update).
[[nodiscard]] View view();

// Once a frame, before the eyes: this frame's view and the steadied head (its filters run in tracking space, so
// the player's turns and moves carry it exactly as they carry the eyes; only the head's own motion is smoothed).
void update(const FrameState& frame, const hands::State& s);

// The smoothed mirror: the map from the window's normalized coordinates (-1..1, x right, y up) to the left eye's image
// (uv, homogeneous: divide by z) for a window of aspect ratio `aspect` (width / height), of the eye whose image is
// `fov` wide and hides `hidden` (null: nothing). The steadied view keeps within the image (the crop's margin): a
// steadying that would show past its edges is pulled back towards the eye (and the filter with it).
[[nodiscard]] glm::mat3 mirrorMap(const Fov& fov, float aspect, const HiddenArea* hidden);

// The spectator camera for a window of aspect ratio `aspect`: where it is, its Quake angles, the tangents of its
// half fields of view.
struct Camera
{
    glm::vec3 origin{0.f};
    glm::vec3 angles{0.f};
    float tanX = 1.f;
    float tanY = 1.f;
};
[[nodiscard]] const Camera& spectator(float aspect);
[[nodiscard]] const Camera& spectatorCamera(); // as spectator() last made it

// The spectator camera's resolution scale (vr_spectator_scale, clamped).
[[nodiscard]] float spectatorScale();

} // namespace qvr::window
