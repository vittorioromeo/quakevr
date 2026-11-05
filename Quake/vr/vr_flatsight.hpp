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

// vr_view.cpp, the frame's weapons placed, the hands still as drawn: each weapon's sight line in its hand's frame, what the
// ironsight is solved from. Nothing outside the mode.
void noteDrawn(const hands::State& s);

// flatplay::drawnOffset: the sight's place for the drawn hand `h` in the frame of the camera it is drawn with: its place and
// its turn. The share of it this frame (0: not on a sight, and they are left alone; 1: the sights lie on the ray).
[[nodiscard]] float drawnSight(int h, glm::vec3& hand, glm::mat3& turn);

// The crosshair's ray for a view at `aim`: its forward, turned to where vr_flat_ads_screen_x / _y have the sight picture.
[[nodiscard]] glm::vec3 rayDir(const glm::vec3& aim);

// The sight picture's place on the screen, this frame: percent of the screen's height, x right, y up. 0 0 at its centre.
[[nodiscard]] glm::vec2 screenPlace();

// How far the ironsight's travel has gone this frame (st.adsT, 0..1): the fraction its shift onto the crosshair and the
// field of view's narrowing are spent over, and what flatplay eases vr_flat_gun_near to vr_flat_ads_near by when it
// decides whether a drawn view counts as the player's own eyes (flatplay::keepNear). 0 outside the mode.
[[nodiscard]] float adsShare();

// How far hand `h`'s weapon is on its sights this frame, 0 outside the mode: the view's own looks are spent out with it.
[[nodiscard]] float drawnShare(int h);

// Whether the empty hand `h` is going to, or is on, the held weapon's two-handed grip.
[[nodiscard]] bool adsHelper(int h);

// The stance (vr_flat_stance). How far the free hand `h` is drawn on the held long gun's two-handed hotspot, 0..1: the view
// draws it there with the pose a helping hand has there (no grip: twohand::drawnTransition). 0 in a headset.
[[nodiscard]] float lookShare(int h);

// How far the torso is turned for a long gun, degrees of yaw (the free hand's shoulder forward), and the share of that turn
// for hand `h`'s gun: 0 for anything else.
[[nodiscard]] float stanceYaw();
[[nodiscard]] float stanceShare(int h);

// The field of view to render with: `basefov`, narrowed by vr_flat_aim_fov while the ironsight is on. Outside the
// mode, or with it off, exactly `basefov` (gl_screen.c through VR_FlatAdsFov).
[[nodiscard]] float fov(float basefov);

// vr_flatplay_status's line: the ironsight, and where the free hand is.
void statusLine();

// vr_flat_sweep's row (vr_flatsweep.cpp): the free hand against the hotspot, the shot against the crosshair's point, the sights
// against the ray. -1: doesn't apply to what is held.
struct SweepNumbers
{
    int holding{-1};
    bool hasSpot{false};
    float lookShare{0.f};
    bool lookFar{false};
    float spotOff{-1.f};
    float armHip{0.f};
    float armSight{0.f};
    float shotDeg{-1.f};
    float shotApart{-1.f};
    float rearOff{-1.f};
    float frontOff{-1.f};
    float relief{0.f};
    float adsT{0.f};
};
[[nodiscard]] SweepNumbers sweepNumbers(const hands::State& s);

// Tests (vr_flatplay_debug 2, the move pass, next to flatplay::printMove): what the two parts did to this frame's shot.
void print(const hands::State& s);

} // namespace qvr::flatsight
