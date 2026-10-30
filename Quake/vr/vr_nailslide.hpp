// vr_nailslide.hpp -- the nail pistol's slide as drawn (QC vr_nailpistol.qc; ROUND23.md, "Nail pistol: moving slide"):
// the server says when it is back (the weapon frame 1: a shot's first third of vr_nailpistol_slide_cycle, or locked back
// while the gun is empty); the client draws it travelling there and home (vr_nailpistol_slide_travel of the gun's model
// units back, progs/v_nailpistol.mdl frame 1 its full 3 units: drawn as frames 0 and 1 blended). An edge to back: it goes
// all the way back (a third of the cycle) even when the server's moment is shorter, then home over the rest; a lock let
// go (a magazine with rounds in: the server's rack sound) runs it home the same. Every other nail pistol drawn (an armed
// grunt's, a holstered or lying one) shows its frame 1 at the same travel.

#pragma once

#include "vr_engine.hpp"

struct qmodel_s;

namespace qvr::nailslide
{

// Each frame, from setupWeapon: the gun drawn in hand `hand` (`gun` its model, `frame` the server's frame for it).
void step(int hand, const qmodel_s* gun, int frame);

// Its place in hand `hand` now: 0 home .. 1 back by the travel (tests, the debug print).
[[nodiscard]] float position(int hand);

// A holstered gun (`model`, its holster's clip and weapon flags stats; clip -1: a preview, a pose): a nail pistol with
// no round to fire, its slide locked back as it was in the hand (QC VR_NailPistol_Empty: reloading on, its clip empty or
// its magazine out; off, your nails spent). Drawn as frame 1 (vr_view.cpp setupHolsters).
[[nodiscard]] bool holsterLocked(const qmodel_s* model, int clip, int flags);

// A nail pistol lying about (a weapon prop, `e` a numbered client entity: U_QVR_WEAPONUID's clip, U_QVR_NOMAG) with no
// round to fire: drawn locked back whatever its frame.
[[nodiscard]] bool propLocked(const entity_t& e);

// Where its ammo screen is moved by the slide now (vr_nailpistol_screen_ride, the full-slide look: the screen is on the
// slide's left wall), in the model's byte space (anchor::posedAnchor's): the slide's drawn travel; 0 for anything else.
[[nodiscard]] glm::vec3 screenShift(const entity_t& e);

// The nail pistol's test (vr_np_test, QC vr_nailpistol_test.qc), each frame: what is drawn, into vr_np_seen_screen (the
// main hand's ammo screen moved by its slide, the gun's model units), vr_np_seen_holster and vr_np_seen_prop (the nail
// pistols drawn in the holsters and lying about: locked back + 100 x closed).
void probe();

} // namespace qvr::nailslide
