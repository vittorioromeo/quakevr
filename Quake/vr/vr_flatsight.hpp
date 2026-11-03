// vr_flatsight.hpp -- the flatscreen player's ironsight, and his free hand on a grip (docs/vr-port/FLATPLAY.md). All of
// it qvr::flatplay only: in a headset these do nothing.
//
// The two-handed grab: one hand holds a weapon and the other is empty, so the empty hand is brought onto that
// weapon's own two-handed grip hotspot -- the point the view already offers in VR (hands::State::grip2H, carried to
// this frame's pose the way a muzzle is) -- and vr_twohand.cpp takes hold of it there: helpKind 1 (a hotspot), the
// game code gets its two-handed aiming bits, and the weapon's weight, recoil and spread become the two-handed ones.
// Dual wielding is not this: both hands are full, so the case never arises and their pose is untouched.
//
// The ironsight: the mouse wheel's button (K_MOUSE3) toggles it, vr_flat_aim from the console the same way.
// A gun with a sight line (sightalign::alignable: a ranged weapon), held in one hand alone. The field of view narrows
// (vr_flat_aim_fov degrees off the plain flatscreen FOV, through VR_FlatAdsFov in gl_screen.c), and the holding hand is
// moved so that the muzzle its weapon shoots from stands where vr_flat_ads_fwd / _up / _out and the weapon's own
// flat_ads_fwd / _up / _out (weapons.txt) put it, off the crosshair's ray: its final place on the screen, reached over
// the same seconds.
//
// The aim, ironsight or not: every gun points from its muzzle at what the ray through the middle of the screen hits (far
// along the ray if nothing), recomputed every frame from wherever the pose put the muzzle, so the shots go through the
// crosshair at any range and whatever the offsets or the eye's nudges (vr_flat_aim_point 0: along the view instead).
//
// The crosshair's ray itself is flatplay's (flatplay::crosshairRay): the camera the screen is drawn from, the same one
// its drawnOffset locks the hands onto, so the gun and the crosshair are measured from one place whatever vr_flat_cam_*
// says about that camera.
//
// A client not hosting has no world to trace (vr_trace.hpp): its guns point far along the ray.
//
// See vr_flatsight.cpp for how each part is measured.

#pragma once

#include "vr_hands.hpp"

namespace qvr::flatsight
{

// VR_Init (through flatplay::init): the vr_flat_aim command.
void init();

// flatplay::frame: the mouse wheel's button edge (K_MOUSE3), once a frame.
void frame();

// flatplay::poseHands: the grip (the empty hand onto the held weapon's hotspot), the two-handed machinery over it, and
// the centring of the held weapon on the crosshair. `reach` is flatplay's E reach for the empty hand
// (its own view-frame offset), so a force grab still reaches out. Safe to call with nothing held: it says so and
// leaves the pose alone.
void apply(hands::State& s, const glm::vec3& aim, const glm::vec3& reach);

// vr_view.cpp, the frame's muzzles just placed on the steady hands: each one's place in its hand's frame, what a turn of
// the hand before the next placing carries it by. Nothing outside the mode.
void notePlaced(const hands::State& s);

// How far the ironsight's travel has gone this frame (st.adsT, 0..1): the fraction its shift onto the crosshair and the
// field of view's narrowing are spent over, and what flatplay eases vr_flat_gun_near to vr_flat_ads_near by when it
// decides whether a drawn view counts as the player's own eyes (flatplay::keepNear). 0 outside the mode.
[[nodiscard]] float adsShare();

// The field of view to render with: `basefov`, narrowed by vr_flat_aim_fov while the ironsight is on. Outside the
// mode, or with it off, exactly `basefov` (gl_screen.c through VR_FlatAdsFov).
[[nodiscard]] float fov(float basefov);

// vr_flatplay_status's line: the ironsight, and where the free hand is.
void statusLine();

// Tests (vr_flatplay_debug 2, the move pass, next to flatplay::printMove): what the two parts did to this frame's shot.
void print(const hands::State& s);

} // namespace qvr::flatsight
