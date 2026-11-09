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

// How far hand `h`'s drawn weapon is turned out of a wall the way back couldn't clear (degrees, the whole turn: the muzzle up,
// vr_flat_wall_tilt, and its yaw and roll, vr_flat_wall_tilt_yaw / _roll, eased in and out over vr_flat_wall_tilt_time), and
// the point it turns round, off the hand in the view's frame (`pivot`; vr_flat_wall_tilt_pivot). 0 outside the mode.
[[nodiscard]] float wallTilt(int h, glm::vec3* pivot = nullptr);

// That turn in the world, for a view whose axes are `axes` (columns forward, right, up): flatplay::drawnOffset turns the
// drawn hand by it round the pivot. The game's hand keeps the crosshair's turn. Identity outside the mode.
[[nodiscard]] glm::mat3 wallTurn(int h, const glm::mat3& axes);

// vr_flat_wall_shot (his words 2026-11-09: "I would like the shots to actually go in the direction of the muzzle"): how much
// of hand `h`'s shot leaves from the muzzle of its gun as the wall turned it (by that turn's angle: none at 0, all of it from
// a few degrees), not from its start eased onto the crosshair's ray. 0 with the cvar off or outside the mode.
[[nodiscard]] float wallShot(int h);

// The shot's angles `angles` (the crosshair's, turned by the weapon's Shot Pitch and Yaw) turned as the wall turned the gun
// (vr_client.cpp handMove: what the move sends). Unchanged with vr_flat_wall_shot 0, off a wall, outside the mode.
[[nodiscard]] glm::vec3 wallShotAngles(int h, const glm::vec3& angles);

// Hand `h`'s gun as the view last drew it: its muzzle and its angles (vr_flat_xhair_probe: where the drawn gun points against
// where its shots go). False with no muzzle placed, outside the mode.
[[nodiscard]] bool drawnGun(int h, glm::vec3& muzzle, glm::vec3& rot);

// Whether hand `h`'s trigger is stopped: its gun is turned out of a wall past a few degrees and vr_flat_wall_block_fire is on
// (flatplay::routeFire). A melee weapon is never stopped (the caller's: it knows what punches).
[[nodiscard]] bool wallBlocksFire(int h);

// Lying down (vr_flat_prone_floor): hand `h`'s gun met the floor this frame and its tilt is the floor's (laid flush on
// it); `left` (if given): how deep in it the tilt left it (units, the tilt's own measure; the room past it does the rest).
[[nodiscard]] bool floorFlush(int h, float* left = nullptr);

// How deep hand `h`'s gun stood in the walls (the floor too, lying down) before its tilt this frame (units, the tilt's
// measure; 0 clear); `floorDepth`: the floor's part alone, `floorAt`: its deepest point off the hand in the gun's frame
// (forward, right, up: the stock is behind, the magazine under). The prone tests' and the status's.
[[nodiscard]] float wallDepth(int h, float* floorDepth = nullptr, glm::vec3* floorAt = nullptr);

// A shot's drawn recoil of a hand (weaponfx::recoilOffset), kept at vr_flat_ads_recoil of itself as far as the ironsight
// has come (adsShare, smoothed); untouched outside the mode (a headset's recoil is its own). vr_flat_recoil_probe reads it.
void keepRecoil(glm::vec3& pos, glm::vec3& angles);

// The crosshairs' opacity (the 2D one, gl_screen.c through VR_FlatCrosshairAlpha, and vr_crosshair's dot and lasers): 1, faded
// to 0 as the ironsight comes up (adsShare, smoothed: its own travel, vr_flat_aim_time) and back in on release. 1 outside the mode.
[[nodiscard]] float crosshairFade();

// Whether the empty hand `h` is going to, or is on, the held weapon's two-handed grip.
[[nodiscard]] bool adsHelper(int h);

// The stance (vr_flat_stance). How far the free hand `h` is drawn on the held long gun's two-handed hotspot, 0..1: the view
// draws it there with the pose a helping hand has there (no grip: twohand::drawnTransition). 0 in a headset.
[[nodiscard]] float lookShare(int h);

// How far down the fore-end, towards the holding hand, the free hand `h` holds instead of on the hotspot itself (units along the
// held hand's forward): where its arm can't take the hotspot (sighted with a long gun, looking up), it slides along the gun and
// stays on it rather than letting go. 0 elsewhere.
[[nodiscard]] float lookSlide(int h);

// The elbow's pole for the free arm `h` on the hotspot, looking up: `way` (world, unit) down under the gun and out at its own side,
// and the share of the arm's own pole it takes (vr_flat_up_elbow, by how far up he looks; 0: none). avatar::solveArm reads it, so
// that arm clears the eye instead of bending its elbow up in front of it. 0 for any other arm, and in a headset.
[[nodiscard]] float elbowPole(int h, glm::vec3& way);

// How far the torso is turned for a long gun, degrees of yaw (the free hand's shoulder forward), and the share of that turn
// for hand `h`'s gun: 0 for anything else.
[[nodiscard]] float stanceYaw();
[[nodiscard]] float stanceShare(int h);
// The same share for any gun in the stance, one the free hand really grips included (the grenade and rocket launchers, the
// proximity gun, the laser cannon, the Enforcer's rifle: their grips are taken for real, `carry`): the stance's height
// (vr_flat_stance_up), which stanceShare would otherwise leave those guns 4 units under the others.
[[nodiscard]] float stanceLift(int h);
// How far the free hand has let go of the gun in hand `h` (0..1): a hipfire gun pushed back or tilted at a wall (the wall
// state) is held one-handed, the free hand easing off to its own pose over vr_flat_wall_tilt_time, and back onto the gun
// as it comes off the wall. 0 outside the mode.
[[nodiscard]] float wallLetGo(int h);

// The stance's kind: a pistol's (vr_flat_stance_pistol: both hands in front of him, vr_flat_pistol_*) rather than a long gun's
// (vr_flat_stance_*: the gun's hand towards the shoulder pocket, by his cheek).
[[nodiscard]] bool stancePistol();

// The chest's lean forward into the gun, degrees: the ironsight's travel in a two-handed stance (vr_flat_ads_lean). The head and
// the eye stay; avatar::solveTorso adds it to the chest's share of the look.
[[nodiscard]] float stanceLean();

// The field of view to render with: `basefov`, narrowed by vr_flat_aim_fov while the ironsight is on. Outside the
// mode, or with it off, exactly `basefov` (gl_screen.c through VR_FlatAdsFov).
[[nodiscard]] float fov(float basefov);

// vr_flatplay_status's line: the ironsight, and where the free hand is.
void statusLine();

// vr_flat_sweep's row (vr_flatsweep.cpp): the free hand against the hotspot, the shot against the crosshair's point, the sights
// against the ray. -1: doesn't apply to what is held.
// The stance's gun hand `h`: the hotspot the free hand is drawn on, in that hand's frame, and how far into the stance it is (0: no
// stance on this hand, `local` not set).
[[nodiscard]] float stanceSpot(int h, glm::vec3& local);
// The rear of the weapon hand `h` holds (its model's back end, on its middle line) off that hand, in the view's frame (forward, right,
// up), as it was last drawn: the stock the flat pose keeps out of the chest. False: not known for what it holds now.
[[nodiscard]] bool stockOff(int h, glm::vec3& local);

struct SweepNumbers
{
    int holding{-1};
    bool hasSpot{false};
    float lookShare{0.f};
    bool lookFar{false};
    float spotOff{-1.f};
    float spotSlide{0.f}; // how far down the fore-end the free hand holds (lookSlide)
    float armHip{0.f};
    float armSight{0.f};
    float shotDeg{-1.f};
    float shotApart{-1.f};
    glm::vec3 shotPoint{0.f}; // what the crosshair is on, and where the shot's line stops
    glm::vec3 shotStop{0.f};
    float rearOff{-1.f};
    float frontOff{-1.f};
    float relief{0.f};
    float adsT{0.f};
    float wallOn{0.f}; // the held hand's wall state (0..1): there its arm and the stock go into the torso on purpose
};
[[nodiscard]] SweepNumbers sweepNumbers(const hands::State& s);

// Tests (vr_flatplay_debug 2, the move pass, next to flatplay::printMove): what the two parts did to this frame's shot.
void print(const hands::State& s);

} // namespace qvr::flatsight
