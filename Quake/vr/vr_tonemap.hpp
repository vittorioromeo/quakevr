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
// vr_gl_probe: queries around an eye's scene (begin, then end: prints its primitives and whether samples passed).
void glProbeScene(bool begin);
// vr_gl_probe 2 and 3, the bisection's draws (a full-screen triangle of one colour): 2 blue under the scene (`after`
// false: after the clear, no depth test), so a scene whose fragments are rejected leaves it blue and one that writes 0
// paints it black; 3 red over it (`after` true) where the depth still holds the clear value (nothing drawn there).
void glProbeDraw(bool after);
// vr_gl_probe 4 and 5: the world's first `indices` indices (`ibo`, `vbo` of `stride`) in green, no depth test: 4
// through the frame's ViewProj alone, 5 through the first world instance's matrix too.
void glProbeWorld(unsigned vbo, unsigned ibo, int stride, int indices);
// vr_gl_probe: an upload (`size` bytes of `data` at `offset` in `buffer`) read back and compared.
void probeUpload(const char* what, unsigned buffer, size_t offset, const void* data, size_t size);
// vr_gl_probe: a GPU-written buffer's words read back (how many are not zero, the largest, the first eight).
void probeBuffer(const char* what, unsigned buffer, size_t offset, size_t size);
// vr_gl_probe: the GL state at a draw (`where`; the first three of the probe's eye scene) and its program's validation.
void probeState(const char* where);
// vr_gl_probe 1 (Debug > Startup; once, then 0): at an eye's `stage` (0 scene drawn, 1 post-processed, 2 with the UI),
// each framebuffer's completeness, colour format and the share of its pixels that are not black (read back), and a
// clear-and-read self-test: where an eye's image goes black, by numbers (the Quest: C4c). Clears it after the right
// eye's last stage.
void glProbe(int eye, int stage, unsigned sceneFbo, unsigned compositeFbo, unsigned imageFbo, int width, int height,
    int imageWidth, int imageHeight);
// The number the next eyeshot's files take (eyeshots/<map>_<number>_L.png).
[[nodiscard]] int nextEyeshot();

} // namespace qvr::tonemap
