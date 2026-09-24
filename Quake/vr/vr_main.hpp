// vr_main.hpp -- access to the VR module's per-frame state from other VR sources.

#pragma once

#include "vr_backend.hpp"

namespace qvr
{

// Tracking from the running backend, or a fixed standing pose (the mock backend's) when VR is
// off: the Quake VR progs always need hands, even when played on a flat screen.
[[nodiscard]] const TrackingState& tracking();

[[nodiscard]] bool vrActive();

// The running backend (null when VR is off) and its current frame.
[[nodiscard]] Backend* backend();
[[nodiscard]] const FrameState& frameState();

} // namespace qvr
