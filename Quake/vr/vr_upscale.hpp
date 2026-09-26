// vr_upscale.hpp -- the resample from the size the eyes are rendered at (vr_render_scale) to the headset's image size:
// bilinear, AMD FidelityFX Super Resolution 1 (EASU, then RCAS sharpening) or NVIDIA Image Scaling (vr_upscale).
//
// It runs after the eye's post-processing (tone curve, grade, gamma, dither: FSR and NIS want display-referred,
// perceptual colours) and before the eye's UI (menus, HUD, lasers, wrist log), which is drawn over the result at the
// image's full size. Like OpenXR Toolkit's foveated FSR, the upscaler only runs within vr_upscale_radius degrees of the
// lens centre (the eye's projection centre, from its asymmetric field of view), fading to bilinear outside it. At
// vr_render_scale 1 nothing runs unless vr_upscale_sharpen_native asks for RCAS alone; above 1 it's the bilinear
// downsample it always was. The shaders are the vendors' own headers (external/fsr1, external/nis; MIT).

#pragma once

#include "vr_engine.hpp"

#include <glm/glm.hpp>

namespace qvr::upscale
{

// An eye's projection centre in a width x height image of its view, and the pixels per unit of tangent (view
// direction x/z, y/z) about it.
struct Lens
{
    glm::vec2 centre{0.f};
    glm::vec2 pixelsPerTangent{1.f};
};
[[nodiscard]] Lens lens(int eye, int width, int height);

// Whether the eyes must be post-processed into a texture even at render scale 1 (vr_upscale_sharpen_native).
[[nodiscard]] bool sharpenAtNative();

// The tangent of vr_upscale_radius (0: everywhere), for the debug overlay.
[[nodiscard]] float radiusTangent();

// The eye's post-processed colours, `source` (width x height, its framebuffer `sourceFbo`), into the framebuffer
// `target` (imageWidth x imageHeight). Leaves `target` bound.
void resample(int eye, GLuint source, GLuint sourceFbo, int width, int height, GLuint target, int imageWidth,
    int imageHeight);

} // namespace qvr::upscale
