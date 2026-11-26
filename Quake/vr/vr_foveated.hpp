// vr_foveated.hpp -- fixed foveated rendering (vr_foveated): the eye's scene shaded at full rate near the lens centre,
// once per 2x2 pixels further out and once per 4x4 at the edge, by NVIDIA's variable-rate shading
// (GL_NV_shading_rate_image; off where the driver lacks it). Each eye has its own shading-rate image (the lenses'
// centres are mirrored), one texel per tile of the size the driver gives, its rate from the angle between the view
// axis and the tile's nearest point.
//
// It covers the scene: the world, models, liquids, particles, the hands and weapons (from right after the scene's
// clear to the end of the eye's V_RenderView), only while one of the eye's own scene framebuffers is bound (a pass
// into a texture of another size, such as the water's scene distances or a screen's canvas, shades every pixel). Not
// the post-processing, bloom, the upscale or the UI. The depth pre-pass has no fragment shader: nothing changes there.

#pragma once

#include "vr_engine.hpp"

namespace qvr::foveated
{

// Whether the driver has GL_NV_shading_rate_image (checked once).
[[nodiscard]] bool supported();

// The eye's scene begins (its framebuffer bound, after the clear and the lenses' hidden area): variable-rate shading
// on for it, if vr_foveated asks. `width` x `height`: the scene's size.
void beginScene(int eye, int width, int height);

// Single-pass stereo's multiview pass (vr_stereo_single.cpp): both eyes' scenes at once into `fbo` (bound), each view
// shaded by its eye's rates (a 2-layer rate image); ended by endScene.
void beginSceneLayered(unsigned fbo, int width, int height);
// The eye's scene is over: variable-rate shading off.
void endScene();

// What is bound now (until the next framebuffer bind) shaded as the eye's scene is (`on`: variable-rate while the
// scene's is; a target of the scene's size whose pixels are the scene's, as vr_gfx_gl.cpp's particles composited
// apart) or at full rate (`on` false: a pass copying pixels into the scene's own framebuffer).
void shadeBoundAsScene(bool on);

// The Quest's foveation (questrender), for vr_foveated_debug: the Adreno driver's pixel density 1 / (gain^2 d^2 - area)
// (QCOM_texture_foveated's, full where that is over 1) at d from the focal point, in the eye's drawn part's -1..1.
// The GL one on our scene (vr_gl_foveation) is exactly this; the runtime's (vr_xr_foveation) is Meta's own, modelled.
struct DensityZones
{
    float focalX = 0.f, focalY = 0.f;
    float gain = 0.f; // 0: no zones
    float area = 0.f;
};

// The runtime's fixed foveation at `level` (vr_xr_foveation: 1 low, 2 medium, 3 high) as modelled: centred on the part
// drawn (the runtime centres it on the rect submitted: ROUND25 "qfov4"), the density falling off as each level's
// pattern, in whole 2x1 / 2x2 steps; the driver reduces whole bins, so the true edges fall on bin edges.
[[nodiscard]] DensityZones runtimeZones(int level);

// vr_foveated_debug: the rates (yellow 2x2, red 4x4) and vr_upscale's circle (cyan) over the eye's image `fbo`
// (width x height), the scene having been sceneWidth x sceneHeight; on the Quest `quest`'s zones (yellow: half the
// pixels, 2x1; red: a quarter, 2x2).
void drawDebug(int eye, GLuint fbo, int width, int height, int sceneWidth, int sceneHeight, const DensityZones& quest = {});

// vr_foveated_gaze_test: the rates around a gaze off the lens centre, and the mock's gaze (vr_mock_gaze) followed
// (vr_foveated_gaze); "vr_foveated_gaze_test: N passed, M failed".
void gazeTest_f();

// What the foveation's full-rate centre follows now (VR Settings > Graphics' line under Eye-Tracked Foveation): the
// eyes while the eye tracker tracks them, else the lens centre and why (no tracker, off, from the next VR start...).
[[nodiscard]] const char* gazeStatus();

} // namespace qvr::foveated
