#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_qvec3.hpp"

#include <gtc/type_ptr.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

[[nodiscard]] inline auto* toGlVec(qvec3& v) noexcept
{
    return glm::value_ptr(v);
}

[[nodiscard]] inline const auto* toGlVec(const qvec3& v) noexcept
{
    return glm::value_ptr(v);
}
