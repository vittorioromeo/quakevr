#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_macros.hpp"

#include <vec3.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

using qfloat = float;
using qvec3 = glm::vec3;

inline constexpr qvec3 vec3_zero{0.f, 0.f, 0.f};
