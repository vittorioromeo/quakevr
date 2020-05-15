#pragma once

#include "quakeglm_qvec3.hpp"

#include <gtc/type_ptr.hpp>

[[nodiscard]] inline auto* toGlVec(qvec3& v) noexcept
{
    return glm::value_ptr(v);
}

[[nodiscard]] inline const auto* toGlVec(const qvec3& v) noexcept
{
    return glm::value_ptr(v);
}
