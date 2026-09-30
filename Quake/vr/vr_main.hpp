// vr_main.hpp -- access to the VR module's per-frame state from other VR sources.

#pragma once

#include "vr_backend.hpp"

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

} // namespace qvr
