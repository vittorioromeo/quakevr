// vr_nailslide.hpp -- the nail pistol's slide as drawn (QC vr_nailpistol.qc; ROUND23.md, "Nail pistol: moving slide"):
// the server says when it is back (the weapon frame 1: a shot's first third of vr_nailpistol_slide_cycle, or locked back
// while the gun is empty); the client draws it travelling there and home (vr_nailpistol_slide_travel of the gun's model
// units back, progs/v_nailpistol.mdl frame 1 its full 3 units: drawn as frames 0 and 1 blended). An edge to back: it goes
// all the way back (a third of the cycle) even when the server's moment is shorter, then home over the rest; a lock let
// go (a magazine with rounds in: the server's rack sound) runs it home the same. Every other nail pistol drawn (an armed
// grunt's, a holstered or lying one) shows its frame 1 at the same travel.

#pragma once

struct qmodel_s;

namespace qvr::nailslide
{

// Each frame, from setupWeapon: the gun drawn in hand `hand` (`gun` its model, `frame` the server's frame for it).
void step(int hand, const qmodel_s* gun, int frame);

// Its place in hand `hand` now: 0 home .. 1 back by the travel (tests, the debug print).
[[nodiscard]] float position(int hand);

} // namespace qvr::nailslide
