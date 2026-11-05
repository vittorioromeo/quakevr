// vr_flatfov_glsl.h -- flatscreen play's model FOV in the shaders (flatplay::modelFov, vr_flatplay.hpp): a world position to where it is
// drawn. Across the view axis scaled, along it left alone: the view's depth, so its depth against the world, stays exact.
#pragma once

#define QVR_FLATFOV_POINT_GLSL \
"vec3 FlatFovPoint(vec3 p, vec3 eye, vec3 axis, vec3 numbers) // numbers: scale, near, far\n" \
"{\n" \
"	vec3 d = p - eye;\n" \
"	vec3 along = axis * dot(d, axis);\n" \
"	return eye + along + (d - along) * mix(numbers.x, 1.0, smoothstep(numbers.y, numbers.z, length(d)));\n" \
"}\n"

// The Quake VR draws' (vr_gfx_gl.cpp flatFovUniforms): FlatFovNumbers.x 0, as they are linked: the position as it came.
#define QVR_FLATFOV_GFX_GLSL \
"layout(location = 94) uniform vec3 FlatFovEye;\n" \
"layout(location = 95) uniform vec3 FlatFovAxis;\n" \
"layout(location = 96) uniform vec3 FlatFovNumbers;\n" \
QVR_FLATFOV_POINT_GLSL \
"vec3 FlatFovDrawn(vec3 p)\n" \
"{\n" \
"	return FlatFovNumbers.x > 0.0 ? FlatFovPoint(p, FlatFovEye, FlatFovAxis, FlatFovNumbers) : p;\n" \
"}\n"
