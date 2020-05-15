#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_macros.hpp"

// TODO VR: (P1) fix glm include paths
#include <mat4x4.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

using qmat4 = glm::mat4;
