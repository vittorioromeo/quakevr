// vr_tonemap.hpp -- the eyes' tone curve, colour grade and last dither (vr_tonemap, vr_exposure, vr_grade,
// vr_grade_strength, vr_dither; the GLSL and the renderer's hooks are in vr_tonemap.h), and vr_eyeshot.

#pragma once

#include "vr_engine.hpp"
#include "vr_tonemap.h"

namespace qvr::tonemap
{

// Whether the eyes' scene is a float target whose brightest the post-process rolls off (vr_tonemap).
[[nodiscard]] bool active();

// The eyes' scene colour format: RGBA16F with vr_tonemap (2: R11F_G11F_B10F), else Ironwail's RGB10_A2.
[[nodiscard]] unsigned sceneFormat();

// The post-process's (and the mirror's) Tone: exposure (0: no curve), knee, white point, grade strength (0: none);
// binds the grade chosen to texture unit `unit`.
[[nodiscard]] glm::vec4 bind(unsigned unit);

// vr_eyeshot: after an eye's post-process, saves its image (`imageFbo`, width x height) and with vr_eyeshot 2 its
// float scene (`sceneFbo`) to <gamedir>/eyeshots; clears vr_eyeshot after the right eye.
void eyeshot(int eye, unsigned imageFbo, unsigned sceneFbo, int width, int height);

} // namespace qvr::tonemap
