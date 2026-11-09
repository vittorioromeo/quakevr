// vr_flatleanpose.hpp -- vr_flatleanpose_test (flatscreen play, -flatscreenmode; docs/vr-port/FLATPLAY.md, "Leaning";
// ROUND24.md, "Flat-screen lean: the arms turn with the torso"): each pair of weapons held upright, at a full lean left
// and right, and leant at a wall at his side; per gun its pose and drawn place off the eye, its height off the floor,
// whether it fires and how far its shot is off the crosshair's point. Checks: leant, each gun is where the upright pose
// turned rigidly with the upper body round the lean's pivot puts it (vr_flat_lean_arms), fires, and its shot goes through
// the crosshair; at the wall, no gun's trigger stopped that was free upright there.

#pragma once

namespace qvr::flatleanpose
{

// VR_Init (through flatplay::init): the command.
void init();

// Every frame flatscreen play runs (flatplay::frame): the test's next step, when one runs.
void frame();

} // namespace qvr::flatleanpose
