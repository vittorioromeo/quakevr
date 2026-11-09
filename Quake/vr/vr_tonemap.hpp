// vr_tonemap.hpp -- the eyes' tone curve, colour grade and last dither (vr_tonemap, vr_exposure, vr_grade,
// vr_grade_strength, vr_dither; the GLSL and the renderer's hooks are in vr_tonemap.h), and vr_eyeshot.

#pragma once

#include "vr_engine.hpp"
#include "vr_tonemap.h"

namespace qvr::tonemap
{

// The curve (QvrTonemap): unchanged up to the knee, 1 at the white point. Quake's brightest colours (fullbright
// lamps, lit sky) are about 1 and most of a frame is under 0.6, so a knee of 0.8 leaves nearly all of it as tuned;
// a white point of 4 (the brightest lightmaps doubled, coloured dynamic lights up to 4) keeps what they add apart.
// The eyes' and flat-screen tone mapping's (vr_flatlook.cpp).
inline constexpr float knee = 0.8f;
inline constexpr float white = 4.f;
// What the world and models may write into the float scene (SceneTone.x): past the white point is white anyway.
inline constexpr float sceneMax = 8.f;

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
// The number the next eyeshot's files take (eyeshots/<map>_<number>_L.png).
[[nodiscard]] int nextEyeshot();

} // namespace qvr::tonemap
