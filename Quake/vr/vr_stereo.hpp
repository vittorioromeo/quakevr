// vr_stereo.hpp -- per-eye rendering state.

#pragma once

#include <glm/glm.hpp>

namespace qvr::stereo
{

[[nodiscard]] bool isRenderingEye();
[[nodiscard]] int eye(); // 0 left, 1 right; valid while rendering an eye

} // namespace qvr::stereo
