// vr_flatplay.hpp -- flatscreen play (-flatscreenmode): the VR game played on the desktop screen with the mouse and
// the keyboard (docs/vr-port/FLATPLAY.md).
//
// The mode is the flag plus no headset session (VR_IsActive() == 0). Without the flag, or in a headset, nothing here
// runs: the game is exactly as it was. With it, the flat player's hands are marked as tracked, so the VR gameplay --
// held weapons, the force grab, melee, body interactions -- runs on poses the game makes up from the view angles, the
// weapon in each hand and the player's own movement, instead of a headset's tracking.
//
// -flatscreenmode implies no VR backend: the mode needs no headset session to begin with, so the flag stands in for
// `vr_backend none; vr_restart` (vr_main.cpp's startBackend asks flatplay::flagGiven, and flatplay::noteBackendImplied
// says once what the flag implied). Start the game without the flag to play in a headset.

#pragma once

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::flatplay
{

// The mode runs now: -flatscreenmode was given and no headset session is up.
[[nodiscard]] bool active();

// -flatscreenmode was given, whatever the headset: vr_main.cpp's backend selection, which the flag implies no backend
// for (noteBackendImplied).
[[nodiscard]] bool flagGiven();

// The weapon `h` holds, as its settings' slot; -1 for an empty hand.
[[nodiscard]] int heldIn(int h);

// A pose number (vr_flat_gun_*, vr_flat_ads_*, vr_flat_empty_*, vr_flat_support_*) clamped to the room it may move in, the
// view's frame either way: -30..30 units.
[[nodiscard]] float poseNum(const Tunable& c);

// Whether this hand's punch is running (its clock: flatplay's own, so never in a headset). The free hand on a grip lets go
// of it for as long (vr_flatsight.cpp, vr_twohand.cpp), and a press meanwhile is not a new punch.
[[nodiscard]] bool punching(int h);

// How hand `h` must be turned for the weapon it holds to shoot along the forward of `angles`, whatever its Shot Pitch and
// Yaw (the drawn gun and its muzzle follow the hand). A hand holding nothing: `angles`.
[[nodiscard]] glm::vec3 aimRotAt(const glm::vec3& angles, int h);

// The one console line for that implication (vr_main.cpp asks before it starts a backend).
void noteBackendImplied();

// VR_Init: reads the flag, registers vr_flatplay_status, vr_flat_punch, +flatcrouch / -flatcrouch.
void init();

// VR_BeginFrame, before the hands are posed: the keys (E, V, Left Ctrl, F, G) and each hand's fire button (the game's
// own fire state: with a weapon a swing strikes with it, that button starts that hand's punch), the cvars the mode
// stands in for (reloading off, the flashlight worn with F switching it, body interactions on), vr_flatplay itself.
void frame();

// The crouch Left Ctrl / +flatcrouch holds, eased 0 standing .. 1 crouched, and how far it puts his head down (units):
// vr_hands.cpp's flat branch lowers his head and his height with it, view.c's VR_FlatViewDrop the drawn view. 0 standing,
// and 0 outside the mode (a headset player crouches with his own tracked head).
[[nodiscard]] float crouchRatio();

// The model FOV (vr_flat_model_fov, docs/vr-port/FLATPLAY.md): the player's own models and what leaves them drawn as if at
// their own field of view, whatever the world's. View-space x and y are scaled round the view axis by
// tan(world / 2) / tan(model / 2), z and w left alone, so depth stays exact against the world and the centre of the screen
// does not move; the scale fades to 1 by distance from the eye, none past vr_flat_model_fov_far. Looks only: nothing
// measured, aimed or traced reads it. numbers: x the scale (0: off, and outside the mode), y near, z far.
struct ModelFov
{
    glm::vec3 eye{0.f};
    glm::vec3 axis{0.f};
    glm::vec3 numbers{0.f};
};
[[nodiscard]] ModelFov modelFov();
// r_alias.c's flushes in the view (not a shadow map's), counted for vr_flatplay_status with those inside a headset's eye:
// temporal AA collects motion only there (vr_temporal.cpp), and the model FOV does not scale what it collects.
void noteModelFovBatch();
[[nodiscard]] float crouchDrop();

// The flatscreen look's window (vr_flat_pitch_up, vr_flat_pitch_down), in the plain game's pitch (positive looking down):
// asked at Quake/cl_main.c's CL_AccumulateCmd, where the frame's looking has all landed in cl.viewangles (VR_FlatPitchClamp,
// vr_api.h). Outside the mode the angle comes back as it was, so plain play and a headset are bounded only by
// cl_maxpitch / cl_minpitch, as they were.
[[nodiscard]] float pitchClamp(float pitch);

// Quake/host.c's Host_WriteConfigurationToFile, just before it writes the key bindings (VR_FlatKeysForConfigWrite,
// vr_api.h): the config on disk is the plain game's, so while flatscreen play holds Ctrl (+flatcrouch, the crouch key
// that must not fire: vr_flat_crouch_fire) the bind is given back for the write and taken up again on the mode's next
// frame. Nothing to give back when the mode is not holding it.
void keysForConfigWrite();

// vr_hands.cpp's flat-screen branch: where each hand goes and how it is turned, in world space: the hands placed in the
// view's frame held to vr_flat_pose_pitch, each hand's punch a swing of that arm about its shoulder (punchSwing). The
// straight out-in-front pose stands when the mode is off.
void poseHands(hands::State& s, const glm::vec3& aim);

// The drawn hand's looks (vr_view.cpp VR_SetupViewEntities, as a shake: the game's hands and the shots don't move): kept
// on the drawn view (its bob, roll and kick), with the step's sway (vr_flat_sway), the walk's bob (vr_flat_bob) and the
// turn's lag (vr_flat_turnlag, vr_flat_turnlag_speed), and held within vr_flat_arm_hold of the shoulder the arm hangs
// from so the hand stays on its arm. Zero outside the mode.
void drawnOffset(const hands::State& s, int h, glm::vec3& pos, glm::vec3& angles);

// The crosshair's ray as this pose has it: the pose's head (its crouch's drop in it) with the eye's nudges vr_flat_cam_fwd
// / _up / _side, along the aim; the head alone when the drawn view is not the player's own (a chase view, the ragdoll
// tool's orbit, a view standing further from his head than vr_flat_gun_near, eased to vr_flat_ads_near while the ironsight
// is on). The one ray the ironsight centres the held weapon on and the guns aim along (vr_flatsight.cpp): the hands carry
// the same nudge, so a camera nudge moves all of it together.
void crosshairRay(const hands::State& s, const glm::vec3& aim, glm::vec3& org, glm::vec3& dir);

// Where `h`'s shoulder is as the pose measures from it, and what an arm can hold a hand at from it (world units).
[[nodiscard]] glm::vec3 shoulderOf(const hands::State& s, int h);
[[nodiscard]] float armReach();

// The eye's nudge (vr_flat_cam_*) in the world at the view's angles `aim`: 0 0 0 at the defaults, and outside the mode. The
// drawn body measures whether the view is inside its head from the eye it nudges (vr_avatar.cpp).
[[nodiscard]] glm::vec3 eyeNudge(const glm::vec3& aim);

// vr_flat_orbit: the drawn view off him, looking back at his own pose (a test aid: Debug > Reports).
struct Orbit
{
    float yaw{0.f};  // degrees round from the view's forward
    float dist{0.f}; // units off (0: in his eyes)
    float down{8.f}; // units below his eyes
};
extern Orbit orbit;
void orbit_f();

// Tests (vr_flatplay_debug 2): the drawn weapon in the camera's frame, a line a frame.
void printDrawn(const float* origin, const float* angles);

// Tests (vr_flatplay_debug 2): the main hand's shot as the move sends it, against the view (vr_client.cpp buildMove).
void printMove(const hands::State& s);

// vr_flatplay_status: what the mode is, and its pose.
void status_f();

// vr_flat_punch [main|off]: a hand's punch from the console, for the tests (V is a key and cannot be driven from a
// script; the fire buttons can, through +attack and +offhandattack). Only while the mode runs, and only with a weapon a
// swing strikes with.
void punch_f();

} // namespace qvr::flatplay
