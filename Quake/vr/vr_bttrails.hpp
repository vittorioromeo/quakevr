// vr_bttrails.hpp -- bullet time's distortion trails (F.E.A.R.'s): in bullet time a wake follows each bullet, nail,
// rocket, grenade and monster's shot, the view behind it bent as through a glass rod that ripples, fading along its
// length and with its age (vr_bullettime_trails*; ROUND21.md, "Bullet time's distortion trails").
//
// A trail is the places its projectile passed and when (real time): the entities' (VR_DistortionTrail, each frame they
// are relinked) and the hitscan shots' (hitscan: a head flying from the muzzle at the tracers' speed, tracer drawn or
// not). It is drawn after the heat haze (VR_DrawHeatHaze): a ribbon in the world, its triangles made once a frame
// (turned to face the first eye: the same ribbon in both eyes), over a copy of the scene (vr_haze.cpp copyScene) read a
// little off to the side: a shift in the world across the ribbon, projected in each eye (the same bend in both), not
// from what is in front of it (the scene's distances), blended in at its edges. None in views through a teleporter.
// Only in bullet time (and fading out after it ends), unless vr_bullettime_trails 2.

#pragma once

#include <glm/glm.hpp>

namespace qvr::bttrails
{

// VR_AdvanceTime (real seconds): the trails' clock, their strength eased in and out with bullet time, the old ones
// forgotten.
void advance(double dt);

// Whether a hitscan shot (`enemy`: a monster's) would start a trail now (vr_weaponfx.cpp: its muzzle found for it).
[[nodiscard]] bool wanted(bool enemy);

// A hitscan shot (vr_weaponfx.cpp parseTracer): from the muzzle to what it hit, its head flying at `speed` units a second
// of the game's time; `enemy`: a monster's.
void hitscan(const glm::vec3& from, const glm::vec3& to, float speed, bool enemy);

// VR_DrawHeatHaze, after the haze: this view's trails.
void draw();

// Forgets them all (a new map, a disconnect).
void clear();

// vr_bullettime_trails_test, vr_bullettime_trails_list.
void registerCommands();

} // namespace qvr::bttrails
