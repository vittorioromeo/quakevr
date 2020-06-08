#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wmacro-redefined"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "quakeglm_macros.hpp"

#include <glm/vec4.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

using qvec4 = glm::vec4;
