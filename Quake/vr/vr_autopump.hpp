// vr_autopump.hpp -- the shotgun's auto pump (ROUND21.md, "Shotgun auto pump"): after each shot the gun cycles itself,
// its fore-end driven back along its guide rods and sprung forward again (polish_weapons.py auto_pump: the rods, the
// housings, the yoke; the moving fore-end progs/vr_pump_on_v_shot.mdl, drawn with the gun without it,
// progs/vr_pumpbody_on_v_shot.mdl, while it moves: vr_view.cpp setupPumps). The spent shell leaves the ejection port as
// the fore-end reaches the back (vr_shells.cpp waits for it), with a light tick in the hand and a mechanical clack at
// each end of the stroke.
//
// Game time (cl.time): slowed with the world in bullet time, the same at any frame rate. vr_autopump (on), _time (the
// whole stroke, 0.3 s: back in its first 35%, a moment at the back, forward), _travel (2.5 model units), _sound,
// _haptics; Weapons > Weapon Effects. vr_autopump_hold 0..1 holds every shotgun's fore-end at that point of the stroke
// (Debug > Tests: pictures); vr_debug_weaponfx 1 prints each stroke's start, back and end.

#pragma once

namespace qvr::autopump
{

// A shot of the shotgun in `hand` (QVR_SVC_FIRED, vr_weaponfx_test: weaponfx::playerFired): a stroke starts; one still
// running starts over.
void fired(int hand);

// How far back the fore-end of the shotgun in `hand` is now, in its model's units (0 at rest, or off).
[[nodiscard]] float travel(int hand);

// The game time the stroke of the shot fired at about `shot` (within a tenth of a second) reaches the back, in
// `rear`, and how fast it went back on average (model units per second of game time) in `backSpeed`: false without
// such a stroke (auto pump off, no shot then).
[[nodiscard]] bool rearTime(int hand, double shot, double& rear, float& backSpeed);

// Once a frame, from the view's setup: the strokes' ends (their clacks and ticks at the shotgun in that hand,
// `isShotgun`, at `where`), the stroke's log.
void frame(const bool (&isShotgun)[2], const float (&where)[2][3]);

// Precaches the clacks (map load).
void prepare();

// A new map, a reset: no stroke running.
void clear();

} // namespace qvr::autopump
