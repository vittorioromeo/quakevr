#pragma once

#include "quakeglm_macros.hpp"

#include <glm/common.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <string_view>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#endif

#include "quakeglm_qvec3.hpp"

#include <GL/glew.h>
