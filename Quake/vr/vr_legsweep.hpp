// vr_legsweep.hpp -- the leg sweep (Combat > Knockdowns > Leg Sweep; docs/vr-port/ROUND22.md, "Leg sweep"): a
// staff-like thing in a hand (a long gun, a crowbar, a sword's hilt, a plank) swept level and fast through a humanoid's
// lower legs knocks it down as a judo foot sweep does: its feet slide along the sweep, the rest of it topples over the
// other way (forward or backward of its own facing, whichever way the sweep goes past it). What counts as staff-like
// comes from the thing's own drawn shape (its length along its long axis and its slenderness) and the part that meets
// the legs (a blade's edge never sweeps).

#pragma once

#include "vr_engine.hpp"

namespace qvr::legsweep
{

void init();        // vr_legsweep_table
void serverFrame(); // VR_ServerFrameEnd, before foegrab::serverFrame: the hands' things against the humanoids' legs

} // namespace qvr::legsweep
