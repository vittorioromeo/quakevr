// vr_stereo.hpp -- per-eye rendering state.

#pragma once

#include <glm/glm.hpp>

struct glframebufs_s; // (the engine's: glquake.h; at global scope, also where this header is included before it)

namespace qvr::stereo
{

void init(); // its commands (vr_stereo_targets_test)

// VR stopped (vr_main.cpp stopBackend: vr_enabled 0, the session lost, a restart): every eye-sized GL object freed (the
// eyes' and the spectator camera's scene targets, the views through teleporters', the resample target, single-pass
// stereo's arrays, the particles', TAA's, the glow's, the haze's, the scene distances', the upscaler's), made again
// when the eyes are next drawn.
void releaseTargets();

// The view being drawn (an eye, the spectator camera, a view through a gate; outside them, the window's): the size its
// targets are allocated at, and the bottom left part of them this frame draws. They differ under dynamic resolution
// (vr_dynres.hpp: the eyes' targets allocated once at its ceiling, a smaller part drawn below it). While the view is
// drawn the engine holds them in vid.width/vid.height (allocated: the size its framebuffers are made and read at) and
// glwidth/glheight, r_refdef.vrect (drawn: the viewports); a pass reads the one it means here.
struct ViewSize
{
    int allocatedWidth = 0, allocatedHeight = 0;
    int drawnWidth = 0, drawnHeight = 0;
};
[[nodiscard]] ViewSize viewSize();

// QvrViewUV's map (vr_viewuv_glsl.h) of a texture allocatedWidth x allocatedHeight of which the bottom left drawnWidth x
// drawnHeight is read: drawn / allocated (exactly 1 when whole), half a texel, in its uv.
[[nodiscard]] glm::vec4 viewUvMap(int drawnWidth, int drawnHeight, int allocatedWidth, int allocatedHeight);

[[nodiscard]] bool isRenderingEye();
// The eye being drawn straight into its swapchain image (vr_xr_direct, or the Quest's fixed foveation: no post-process,
// the tone curve in the scene's shaders): 0 no, 1 with the composite's depth texture, 2 with a depth renderbuffer of its
// own (no depth texture to read: the soft and half-size particles and the liquids' distances go without).
[[nodiscard]] int directEye();
[[nodiscard]] int eye(); // 0 left, 1 right; valid while rendering an eye
[[nodiscard]] bool isFirstEye(); // the first eye rendered this frame; valid while rendering an eye
// Rendering the desktop window's spectator camera (vr_window.cpp): a third view after the eyes, as an eye (0: its
// per-eye tables are the left eye's) with the camera's own place, angles and projection, the eyes' entities, no hidden
// area, foveation or upscale; isFirstEye is false. Caches sized to the view keep one for it (vr_bloom, vr_haze,
// vr_water).
[[nodiscard]] bool isSpectator();
// The menus' small copy of the spectator camera's image as the window shows it (vr_spectator_preview: made while
// menuui::spectatorPreviewWanted), 0 when there is none current; `aspect` its height over its width.
[[nodiscard]] unsigned spectatorPreview(float& aspect);

// Single-pass stereo (vr_stereo_single.cpp): the eye whose projection and hidden area apply now; the lenses' hidden area
// of eye `eye` drawn into the bound framebuffer; the eyes' scene colour and depth/stencil formats; the eyes' targets
// (their OIT textures, attached with each eye's layer) and a count bumped each time they are made again.
void setCurrentEye(int eye);
void drawHiddenAreaOf(int eye);
void eyeSceneFormats(unsigned& color, unsigned& depth);
[[nodiscard]] const ::glframebufs_s& eyeFramebufs();
[[nodiscard]] unsigned eyeTargetsGeneration();

} // namespace qvr::stereo
