// vr_seated.hpp -- seated play (docs/vr-port/SEATED.md). With Position: Seated (vr_bodycal_seated, VR Calibration's
// first row) the game stands you up: the head, the hands and the tracked body are raised by your standing eye height
// (vr_height_calibration) less your seated one (vr_seated_height), so your eyes are a standing player's, the body stands
// on the floor with its legs planted and walking (body tracking's legs left out: bodytrack.cpp), a jump in the room
// doesn't jump (it can't be made sitting; the head rising as you sit up would), the holsters and pouches take their
// seated layout (vr_*_seated_*: body.cpp, view.cpp), and a hand reaching low goes further down so that the floor is in
// reach from the chair (vr_seated_reach). vr_crouch_toggle crouches (vr_seated_crouch_depth); ducking in the chair still
// crouches as far as the head goes down.

#pragma once

namespace qvr
{
struct TrackingState;
}

namespace qvr::seated
{

// Seated play is on (vr_bodycal_seated).
[[nodiscard]] bool active();

// Your eyes' height sitting, metres: vr_seated_height, or 0.74 of the standing height while it isn't measured.
[[nodiscard]] float eyeHeight();

// Metres the tracking is raised in the game now: the standing eye height less the seated one (vr_seated_stand), less
// vr_seated_crouch_depth as far as the crouch toggle has eased in; 0 standing.
[[nodiscard]] float lift();

// Once a frame, before the tracking is placed (hands.cpp): the crouch toggle eased; `in` as the game places it into
// `out`: the head and the hands raised by lift(), a hand low lowered again towards the floor (vr_seated_reach; not
// while a calibration measures you). False (`out` untouched) standing: the tracking as it is.
bool raise(const TrackingState& in, TrackingState& out);

// The floor reach: metres a hand `realY` metres over the room's floor is lowered from the lift (0 over
// vr_seated_reach_start, all of the lift at vr_seated_reach_floor and under).
[[nodiscard]] float handDrop(float realY);

void registerCommands(); // vr_seated_status, vr_crouch_toggle, vr_seated_height_now

} // namespace qvr::seated
