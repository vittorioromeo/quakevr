// vr_flatsweep.hpp -- vr_flat_sweep (Debug > Reports): the flatscreen pose measured from a look straight up to one straight
// down, standing and crouching, per weapon stance, hipfire and ironsight. A test's tool: it drives the view's pitch by
// itself, waits for the pose to settle at each step (a condition, no count of frames) and writes one row a step to a CSV next
// to the screenshots. Misc/quakevr/flatsweep_test.sh runs it and flatsweep_check.py judges the rows (docs/vr-port/FLATPLAY.md,
// "The pose sweep").

#pragma once

namespace qvr::flatsweep
{

// flatplay::init: the command.
void init();

// flatplay::frame, before the hands are posed: the sweep's next step, nothing when none runs.
void frame();

} // namespace qvr::flatsweep
