#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_macros.hpp"

// TODO VR: (P1) fix glm include paths
#include <gtc/quaternion.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

using qquat = glm::quat;

[[nodiscard]] inline auto toGlMat(const qquat& q) noexcept
{
    return glm::mat4_cast(q);
}
