// vr_stereo.hpp -- per-eye rendering state.

#pragma once

namespace qvr::stereo
{

[[nodiscard]] bool isRenderingEye();
[[nodiscard]] int eye(); // 0 left, 1 right; valid while rendering an eye
[[nodiscard]] bool isFirstEye(); // the first eye rendered this frame; valid while rendering an eye

} // namespace qvr::stereo
