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

// The eye's scene is over: variable-rate shading off.
void endScene();

// vr_foveated_debug: the rates (yellow 2x2, red 4x4) and vr_upscale's circle (cyan) over the eye's image `fbo`
// (width x height), the scene having been sceneWidth x sceneHeight.
void drawDebug(int eye, GLuint fbo, int width, int height, int sceneWidth, int sceneHeight);

} // namespace qvr::foveated
