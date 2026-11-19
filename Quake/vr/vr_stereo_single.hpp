// vr_stereo_single.hpp -- single-pass stereo (vr_stereo_single; docs/vr-port/STEREO.md): the passes ported so far drawn
// once for both eyes into a 2-layer texture array (GL_OVR_multiview2), before each eye renders the rest of its scene
// as before into its layer (texture views of it as the eye's scene targets).

#pragma once

#include "vr_stereo.hpp"

namespace qvr::stereosingle
{

void init(); // its commands (vr_stereo_single_info)

// This frame's eyes (VR_RenderView, before the eyes' loop): whether single-pass stereo runs (vr_stereo_single on, the
// driver able, no MSAA). `samples`: the eyes' scene MSAA.
[[nodiscard]] bool wanted(int samples);
// After eye `eye`'s V_SetupView (in VR_RenderView's set-up of both eyes): its view kept (r_refdef, its matrices).
void captureEye(int eye);
// Both eyes captured: the multiview pass (R_RenderStereoSingle), eye 0's state current, the eyes' targets bound as
// the eyes' loop does. `size`: the eyes' (their targets' allocated size, the part drawn). False: it did not run (the
// eyes render as before).
bool render(const stereo::ViewSize& size);
// The eyes' loop: eye `eye`'s own view begins (its r_refdef back; V_RenderView skips V_SetupView) / ends.
void beginEye(int eye);
void endEye();
// The frame's eyes are done.
void endFrame();

// VR stopped (stereo::releaseTargets): the arrays and their views freed (made again at the next single-pass frame).
void releaseTargets();

} // namespace qvr::stereosingle
