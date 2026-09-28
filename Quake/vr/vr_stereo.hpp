// vr_stereo.hpp -- per-eye rendering state.

#pragma once

namespace qvr::stereo
{

[[nodiscard]] bool isRenderingEye();
[[nodiscard]] int eye(); // 0 left, 1 right; valid while rendering an eye
[[nodiscard]] bool isFirstEye(); // the first eye rendered this frame; valid while rendering an eye
// Rendering the desktop window's spectator camera (vr_window.cpp): a third view after the eyes, as an eye (0: its
// per-eye tables are the left eye's) with the camera's own place, angles and projection, the eyes' entities, no hidden
// area, foveation or upscale; isFirstEye is false. Caches sized to the view keep one for it (vr_bloom, vr_haze,
// vr_water).
[[nodiscard]] bool isSpectator();

} // namespace qvr::stereo
