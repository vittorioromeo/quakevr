#pragma once

// Temporal anti-aliasing in the eyes (docs/vr-port/TEMPORAL.md, stages 2a and 2b): per-eye motion vectors (the camera's
// motion from the depth and the previous frame's view-projection; the models' and the moving brush models' own from a
// motion output of their shading), a reactive mask (translucency, particles, liquids), sub-pixel jitter and a
// per-eye history resolved before the post-processing (vr_taa), with debug views (vr_temporal_debug).

namespace qvr::temporal
{

// Whether the eyes collect motion vectors this frame (vr_taa or vr_temporal_debug, two-pass stereo, no MSAA).
[[nodiscard]] bool wanted();

// The eyes' MSAA while temporal AA runs: none (the temporal filter replaces it).
[[nodiscard]] float eyeFsaa(float fsaa);

// VR_RenderView: once a frame before anything is drawn (the frame's slots for the models' positions), with the eye
// targets' framebuffers (the motion target is attached to them while the eyes collect).
void beginFrame(unsigned sceneFbo, unsigned compositeFbo, int width, int height, int samples);

// Around each eye's V_RenderView.
void beginEye(int eye);
void endEye();

void registerCommands();

} // namespace qvr::temporal
