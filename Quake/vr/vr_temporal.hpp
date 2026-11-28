#pragma once

// Temporal anti-aliasing in the eyes (docs/vr-port/TEMPORAL.md, stages 2a and 2b): per-eye motion vectors (the camera's
// motion from the depth and the previous frame's view-projection; the models' and the moving brush models' own from a
// motion output of their shading), a reactive mask (translucency, particles, liquids), sub-pixel jitter and a
// per-eye history resolved before the post-processing (vr_taa), with debug views (vr_temporal_debug).

#include "vr_stereo.hpp"

namespace qvr::temporal
{

// Whether the eyes collect motion vectors this frame (vr_taa or vr_temporal_debug, two-pass stereo, no MSAA).
[[nodiscard]] bool wanted();

// The eyes' MSAA while temporal AA runs: none (the temporal filter replaces it).
[[nodiscard]] float eyeFsaa(float fsaa);

// Flat-screen play's window view takes the same TAA (vr_flat_taa; VR_FlatRenderView): wanted this frame, and the
// window's scene samples (GL_CreateFrameBuffers, VR_SceneSamples, as the window's targets are made): 1 under it (the
// temporal filter replaces MSAA).
[[nodiscard]] bool flatWanted();
[[nodiscard]] int windowSamples(int samples);

// VR_RenderView: once a frame before anything is drawn (the frame's slots for the models' positions), with the eye
// targets' framebuffers (the motion target is attached to them while the eyes collect) and `size` (their allocated size,
// at which the textures are made, and the part drawn this frame: another drawn size starts the history anew).
void beginFrame(unsigned sceneFbo, unsigned compositeFbo, const stereo::ViewSize& size, int samples);

// Around each eye's V_RenderView.
void beginEye(int eye);
void endEye();

void registerCommands();

// A gfx draw's own motion (gfx::State::prevFromNow: the wrist gadget's screen): when the bound framebuffer is the eye's
// scene collecting motion vectors, the draws that follow write the motion target alone (attachment 1; true) until
// endOwnMotion. Its texels: QVR_TEMPORAL_WRITE's (vr_glsl.h).
[[nodiscard]] bool beginOwnMotion();
void endOwnMotion();

// VR stopped (stereo::releaseTargets): the eye-sized textures freed (made again, the history anew, at their next use) and
// the motion target detached from the eyes' framebuffers (made again: attached again next frame).
void releaseTargets();

} // namespace qvr::temporal
