// vr_flatsight.hpp -- the flatscreen player's ironsight, and his free hand on a grip (Vittorio's flatscreen list,
// items 3 and 4; docs/vr-port/FLATPLAY.md). All of it qvr::flatplay only: in a headset these do nothing.
//
// Item 3, the two-handed grab: one hand holds a weapon and the other is empty, so the empty hand is brought onto that
// weapon's own two-handed grip hotspot -- the point the view already offers in VR (hands::State::grip2H, carried to
// this frame's pose the way a muzzle is) -- and vr_twohand.cpp takes hold of it there: helpKind 1 (a hotspot), the
// game code gets its two-handed aiming bits, and the weapon's weight, recoil and spread become the two-handed ones.
// Dual wielding is not this: both hands are full, so the case never arises and their pose is untouched.
//
// Item 4, the ironsight: the mouse wheel's button (K_MOUSE3) toggles it, vr_flat_aim from the console the same way.
// A gun with a sight line (sightalign::sightLine: a ranged weapon), held in one hand alone. The field of view narrows
// (vr_flat_aim_fov degrees off the plain flatscreen FOV, through VR_FlatAdsFov in gl_screen.c), and the holding hand is
// moved sideways so that the muzzle its weapon shoots from stands on the crosshair's ray: the sights come to the
// middle of the screen and the shots go through it. The hand's turn stays the crosshair's aim, so what moves is where
// the shot comes from, not where it is going.
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

// flatplay::poseHands: item 3's grip (the empty hand onto the held weapon's hotspot), the two-handed machinery over
// it, and item 4's centring of the held weapon on the crosshair. `reach` is flatplay's E reach for the empty hand
// (its own view-frame offset), so a force grab still reaches out. Safe to call with nothing held: it says so and
// leaves the pose alone.
void apply(hands::State& s, const glm::vec3& aim, const glm::vec3& reach);

// The ironsight is on for a weapon it applies to (vr_flatplay_status, gl_screen.c).
[[nodiscard]] bool aiming();

// The field of view to render with: `basefov`, narrowed by vr_flat_aim_fov while the ironsight is on. Outside the
// mode, or with it off, exactly `basefov` (gl_screen.c through VR_FlatAdsFov).
[[nodiscard]] float fov(float basefov);

// vr_flatplay_status's line: the ironsight, and where the free hand is.
void statusLine();

// Tests (vr_flatplay_debug 2, the move pass, next to flatplay::printMove): what the two parts did to this frame's shot.
void print(const hands::State& s);

} // namespace qvr::flatsight
