// vr_main.hpp -- access to the VR module's per-frame state from other VR sources.

#pragma once

#include "vr_backend.hpp"

#include "Zancle/String/String.hpp"

namespace qvr
{

// Tracking from the running backend, or a fixed standing pose (the mock backend's) when VR is
// off: the Quake VR progs always need hands, even when played on a flat screen.
[[nodiscard]] const TrackingState& tracking();

// Bumped by every map load (R_NewMap): per-map data keyed on cl.worldmodel alone would outlive a map loaded again
// into the same model (the same name from another file: vr_relit_maps switched, or the original vs the re-release).
[[nodiscard]] int worldGeneration();

[[nodiscard]] bool vrActive();

// The running backend (null when VR is off) and its current frame.
[[nodiscard]] Backend* backend();
// A backend (re)start asked for (start-up, a vr_backend change) and not yet made: it happens at the next frame's start.
[[nodiscard]] bool backendRestartPending();
[[nodiscard]] const FrameState& frameState();

// QVR round 21: the frames' rate and cost, averaged over half a second, for the wrist gadget's FPS counter
// (vr_gadget_fps): frames a second (from the frames' periods), the CPU's work a frame (the host frame less the
// runtime's and the swap's waits: the memory log's busy_ms) and the eyes' GPU time a frame (its gpu_eyes_ms; -1 while
// none has been read back), from the phases timed every frame anyway (vr_profile.hpp). False until the first half
// second is in. Call it once a frame while it is shown.
struct FrameRate
{
    float fps{0.f};
    float cpuMs{0.f};
    float gpuMs{-1.f};
};
[[nodiscard]] bool frameRate(FrameRate& out);

// The menu's status box's lines (vr_menu_status): VR, mock VR or flat; the runtime; the resolution rendered; the target
// rate; the frames a second and the CPU's and GPU's work; the RAM and VRAM held. Once a frame while it is shown.
void statusLines(za::Vector<za::String>& out);
// Its eye lines (vr_status prints them too): the size rendered, as the runtime's times Eye Image Size times Render
// Scale; the panel (vr_xr_panel, or by the headset's name) and the runtime's and the eyes' shares of its pixels; a
// warning over vr_xr_res_warn (lines beginning with '!').
void eyeSizeLines(const Backend& b, za::Vector<za::String>& out);

// The runtime's own menu has the focus (or vr_debug_runtime_menu), and a single player game is paused for it
// (vr_xr_unfocused_pause): as of this frame's VR_BeginFrame.
[[nodiscard]] bool runtimeMenuOpen();
[[nodiscard]] bool runtimeMenuPaused();

} // namespace qvr
