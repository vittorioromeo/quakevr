// vr_dynres.hpp -- dynamic resolution (vr_dynres): the eyes' render scale driven by their measured GPU time, so a GPU
// short of the headset's refresh rate holds it.
//
// The eyes' GPU time (the always-on phases' timestamps, `eye L` + `eye R`: the scene, shadow maps, post-processing, the
// upscale and the UI; read back 2-5 frames late) is compared with a budget, vr_dynres_target of the headset's display
// period. The scale moves between vr_dynres_min and its ceiling (vr_dynres_max; below 0, the default: Render Scale, so a
// GPU with headroom renders exactly as without it) in buckets of 0.05 down from the ceiling: the eyes' targets are
// remade only when the bucket changes (as moving the Render Scale slider remakes them). Each frame's time is read
// against the scale that frame was drawn at (pixels go with its square), so the latency does not make it overshoot.
// Down is fast: two frames in a row over the budget drop it at once to the bucket that fits, as far as needed. Up is
// slow: one bucket at a time, only once the slower average says the next one fits within 85 % of the budget, a second
// after a drop and half a second after a rise; the gap between the two is the hysteresis that keeps it from pumping.
// The resample to the headset's size is the existing upscaler's (vr_upscale), TAA's when it is on.

#pragma once

namespace qvr::dynres
{

// The scale the eyes are drawn at this frame (vr_render_scale's when off or not in VR), within 0.25..2.
[[nodiscard]] float scale();

// At a frame's eyes (VR_RenderView): notes the scale this frame is drawn at (its frame serial, profile.cpp's).
void beginFrame();

// A frame's eyes' GPU time, read back (vr_profile.cpp): `serial` the frame's, `eyesMs` its eyes' (0: none drawn).
void gpuFrame(unsigned long long serial, double eyesMs);

// The status box's line ("Dynamic res x0.85 of 1.00, GPU 6.1 of 7.9 ms"); false while off.
[[nodiscard]] bool statusLine(char* out, int size);

// vr_dynres_test_load: an artificial GPU cost in the eye being drawn (Debug > Profiling), a full-view pass that costs
// this many units of work a pixel (it changes no pixel), to test the controller without a heavy scene. Into `fbo`
// (width x height); the framebuffer bound before is bound again. Negative: that load for one frame (its two eyes), a
// spike, then 0.
void testLoad(int eye, unsigned fbo, int width, int height);

} // namespace qvr::dynres
