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

namespace qvr::avatar
{
struct Skeleton;
}

namespace qvr::flatplay
{

// The mode runs now: -flatscreenmode was given and no headset session is up.
[[nodiscard]] bool active();

// What the swap key (vr_flat_key_swap, 3) does: the hands swap what they hold, met first with vr_flat_swap_anim
// (vr_flatswap.cpp), else at once (impulse 48). The holster menu's SWAP too (vr_flatholster.cpp); `what` for the debug line.
void swapHands(const char* what);

// -flatscreenmode was given, whatever the headset: vr_main.cpp's backend selection, which the flag implies no backend
// for (noteBackendImplied).
[[nodiscard]] bool flagGiven();

// The weapon `h` holds, as its settings' slot; -1 for an empty hand.
[[nodiscard]] int heldIn(int h);

// Whether `h` holds anything that is not a weapon: a prop, an item, a torch (STAT_QVR_CARRYMAIN / _CARRYOFF, or drawn in
// it: held::heldEntity). Such a hand is not free to steady the other hand's gun: that gun is held one-handed (his note
// 2026-11-10; vr_flatsight.cpp, vr_twohand.cpp).
[[nodiscard]] bool holdsThing(int h);

// Whether `h` holds a prop in its fist (vr_flat_carry_pose 2): a prop in this hand alone, no weapon. The hand is posed as
// the empty hand's guard, the prop in the fist, and V or its fire button strikes with it (vr_flat_prop_melee: the
// overhead swing, or the fist's punch); the game's melee takes the prop as what strikes (QC vr_carry.qc).
[[nodiscard]] bool propInFist(int h);

// The hand holding the only weapon held (heldIn), whichever hand that is; -1 with a weapon in each hand or in neither.
[[nodiscard]] int loneWeaponHand();

// The mouse's two buttons as a flatscreen player's hands take them (his words 2026-11-05; FLATPLAY.md, "The keys"): `lmb`
// is +attack's state (the left mouse button), `rmb` +offhandattack's (the right). With one weapon held, whichever hand
// holds it, the left button fires it and the right is the ironsight (vr_flatsight.cpp, vr_flat_aim_mode), so it fires
// nothing; with a weapon in each hand, or none, the left button is the right hand's and the right the left's, as in VR.
// fire[HAND_MAIN], fire[HAND_OFF]: what each hand's trigger reads.
struct FireRoute
{
    bool fire[2]{false, false};
    bool rmbAims{false}; // the right button is the ironsight's (one weapon held)
};
[[nodiscard]] FireRoute routeFire(bool lmb, bool rmb);

// A pose number (vr_flat_gun_*, vr_flat_ads_*, vr_flat_empty_*, vr_flat_support_*) clamped to the room it may move in, the
// view's frame either way: -30..30 units.
[[nodiscard]] float poseNum(const Tunable& c);

// Whether this hand's punch is running (its clock: flatplay's own, so never in a headset). The free hand on a grip lets go
// of it for as long (vr_flatsight.cpp, vr_twohand.cpp), and a press meanwhile is not a new punch.
[[nodiscard]] bool punching(int h);
// Whether this hand pushes in a shove running now (its windup, push and way back; vr_flat_key_shove). The free hand on a
// grip lets go of it for as long, as for a punch.
[[nodiscard]] bool shoving(int h);
// The drawn view's tilt from a blow (degrees: pitch down, 0, roll): a punch tips it into the punch, a weapon's overhead
// swing back with the windup and forward with the strike (vr_flat_punch_cam_*, vr_flat_swing_cam_*). The view only: the
// aim and the shots stay (VR_FlatViewLean adds it). 0 outside the mode.
[[nodiscard]] glm::vec3 meleeViewTilt();
// How far empty hand `h`'s fingers close into a fist (0 open .. 1): its rest's share of the guard
// (vr_flat_empty_hand_pose), and all the way while it punches.
[[nodiscard]] float fistShare(int h);

// Whether this hand holds a melee weapon (the axe, mjolnir, a sword, the chainsaw, the crowbar, the super axe; AD's melee
// weapons are these ids too): at a wall it only goes back, never turns muzzle up (vr_flatsight.cpp).
[[nodiscard]] bool meleeIn(int h);

// meleeIn less the chainsaw: a melee weapon swung by a handle (the axe, mjolnir, a sword, the crowbar, the super axe, AD's
// blades): its two-handed grip is the lowest on that handle (vr_view_weapon.cpp's chooseGrip).
[[nodiscard]] bool swungMelee(int h);

// The parry's guard (vr_flat_parry_*: the right mouse button held with one melee weapon, meleeIn): its share for hand
// `h` (0 at rest .. 1 in the guard), eased in and out over vr_flat_parry_in_time / _out_time.
[[nodiscard]] float parryShare(int h);

// How hand `h` must be turned for the weapon it holds to shoot along the forward of `angles`, whatever its Shot Pitch and
// Yaw (the drawn gun and its muzzle follow the hand). A hand holding nothing: `angles`.
[[nodiscard]] glm::vec3 aimRotAt(const glm::vec3& angles, int h);

// The one console line for that implication (vr_main.cpp asks before it starts a backend).
void noteBackendImplied();

// VR_Init: reads the flag, registers vr_flatplay_status, vr_flat_punch, +flatcrouch / -flatcrouch.
void init();

// VR_BeginFrame, before the hands are posed: the keys (R, V, Left Ctrl, F, G, 3, Z, X; the leans Q and E: vr_flatmove.cpp) and each hand's fire button (the game's
// own fire state: with a weapon a swing strikes with it, that button starts that hand's punch), the cvars the mode
// stands in for (reloading off, the flashlight worn with F switching it, body interactions on), vr_flatplay itself.
void frame();

// The crouch Left Ctrl / +flatcrouch holds, eased 0 standing .. 1 crouched, and how far it puts his head down (units):
// vr_hands.cpp's flat branch lowers his head and his height with it, view.c's VR_FlatViewDrop the drawn view. 0 standing,
// and 0 outside the mode (a headset player crouches with his own tracked head).
[[nodiscard]] float crouchRatio();
// Whether the crouch key (Left Ctrl, +flatcrouch) is held now: the move's word for the server (protocol::moveFlatCrouchBit),
// which keeps his box crouched under a low ceiling after it is let go (vr_hull.cpp updateCrouch). False outside the mode.
[[nodiscard]] bool crouchHeld();
// vr_flat_crouch_trace's sample, once a frame from VR_FlatViewDrop with the drawn eye's height (world z).
void traceCrouch(float eyeZ);

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
// Lying down's eased share (vr_flat_key_prone, vr_prone_toggle): 0 up .. 1 down; 0 outside the mode.
[[nodiscard]] float proneRatio();
// The last frame's view origin (V_CalcRefdef's, after the mode's drop and lean) and drawn main hand, world: the ramp
// slide test's jitter measure (vr_flatmove.cpp vr_flatslide_ramp_test).
[[nodiscard]] glm::vec3 lastViewOrigin();
[[nodiscard]] glm::vec3 lastMainHand();
[[nodiscard]] glm::vec3 lastPoseHead(); // (the head the hands were posed from)
// The drawn body the same way: the main hand's shoulder (each frame) and the pelvis (while traceBody(true): it solves the
// torso once more a frame).
void traceBody(bool on);
[[nodiscard]] glm::vec3 lastMainShoulder();
[[nodiscard]] glm::vec3 lastPelvis();
// The main hand as last drawn (drawnOffset: the pose's hand with the looks and the arm's hold), world, and how many drawn
// hands the arm's hold has pulled in so far (the slide jitter measure).
[[nodiscard]] glm::vec3 lastDrawnMainHand();
[[nodiscard]] int armHolds();

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
// body's frame off the shoulders the arms hang from (handOnBody, vr_flat_body_pose; 0: the view's frame held to vr_flat_pose_pitch), each hand's punch a swing of that arm about its shoulder (punchSwing). The
// straight out-in-front pose stands when the mode is off.
void poseHands(hands::State& s, const glm::vec3& aim);

// The body pose's two asks of the avatar (vr_avatar.cpp solveTorso), both nothing with vr_flat_body_pose 0: how far the chest
// bends for this look's pitch (degrees, + forward: vr_flat_chest_down, _up), and the neck's move off the eye a full crouch
// makes, `levelFwd` being the level look's forward (world units: vr_flat_crouch_shoulder_down, _fwd).
[[nodiscard]] float chestPitch(float pitch);
// The slide's turn of the chest back to the view (degrees about the trunk, + left): with vr_flat_slide_legs the hips and legs
// face the slide's way (the body's yaw, poseHands) and the chest turns back towards the view, at most
// vr_flat_slide_legs_twist. 0 outside a slide and the mode.
[[nodiscard]] float trunkTwist();
// Lying down, how far further under the eye the drawn trunk hangs from (units, vr_flat_prone_neck_drop): the body lies
// lower than the view, the chest almost on the floor, the gun's poses and the view as they were. 0 outside the mode.
[[nodiscard]] float proneNeckDrop();
[[nodiscard]] glm::vec3 crouchNeck(const glm::vec3& levelFwd, float pitch);
// The free hand `h` hanging: the move from where it stands to where it would stand off the gun in its other hand had the look been
// level (what chooses the grip: vr_view_weapon.cpp). 0 outside the mode, with vr_flat_body_pose 0, and for a hand doing anything else.
[[nodiscard]] glm::vec3 levelHang(const hands::State& s, int h);
// The held weapon's Gun Out (units before vr_flat_reach) at the look `pitch` (Quake's, positive down), the crouch as it is now:
// vr_flat_gun_out level, vr_flat_gun_out_crouch crouched, going to vr_flat_gun_out_down / _up along a smooth curve. The sweep's.
[[nodiscard]] float gunOut(float pitch);
// How far the line from `rear` to `hand` stays out of the torso (units; negative: in it; huge with no body pose).
// flatsight's wall poses score the turned weapon's points with it.
[[nodiscard]] float torsoGap(const hands::State& s, const glm::vec3& rear, const glm::vec3& hand);
// How far back along `way` (world, unit length) the weapon hand `h` holds can go from where it is now, at most `most`, keeping the
// weapon's rear vr_flat_clear_stock out of the torso (or no deeper than it is now): a wall's push back (flatsight's pushBack) stops
// at the chest. `keep` (units, 0..vr_flat_clear_stock; negative: all of vr_flat_clear_stock) asks for less of that clearance.
// vr_flat_clear_stock 0 keeps none, as ever.
[[nodiscard]] float stockBackMost(const hands::State& s, int h, const glm::vec3& way, float most, float keep = -1.f);
// The drawn body's move with the drawn view (world units): the view's own move off the pose's eye -- the walk's bob, a step's easing --
// by vr_flat_body_bob (all of it sighted), which the drawn hands ride too (drawnOffset), so body, arms and hands move as one. 0 outside
// the mode, off the body pose, and when the drawn view isn't his own eyes.
[[nodiscard]] glm::vec3 bodyRide(const hands::State& s);
// The drawn body as last posed, carried to where this frame's will stand (the head it is posed under: s.head with bodyRide;
// avatar::skeletonUnder): for what is placed before this frame's body is posed. False when the body is not posed.
[[nodiscard]] bool drawnSkeleton(const hands::State& s, avatar::Skeleton& out);
// The hands stand on the body (vr_flat_body_pose), in the mode.
[[nodiscard]] bool onBody();
// How far the chest's share of this look carries his head and neck (vr_flat_eye_lean). In the eye's place already: solveTorso's head.
[[nodiscard]] glm::vec3 headLean(const glm::vec3& aim);

// The drawn hand's looks (vr_view.cpp VR_SetupViewEntities, as a shake: the game's hands and the shots don't move): kept
// on the drawn view (its bob, roll and kick), with the step's sway (vr_flat_sway), the walk's bob (vr_flat_bob) and the
// turn's lag (vr_flat_turnlag, vr_flat_turnlag_speed), and held within vr_flat_arm_hold of the shoulder the arm hangs
// from so the hand stays on its arm. Zero outside the mode.
void drawnOffset(const hands::State& s, int h, glm::vec3& pos, glm::vec3& angles);
// Where drawnOffset drew hand `h` last (world; before a wall's tilt): vr_flatleanpose_test's drawn guns.
[[nodiscard]] glm::vec3 drawnHand(int h);
// Hand `h`'s gun's roll about its bore, degrees (as the view's roll): leaning (Q / E), vr_flat_lean_gun_roll of the view's
// lean roll (flatmove::leanRoll); sliding, vr_flat_slide_gun_roll by the slide's share, its top out to its hand's side.
// poseHands turns the hands with it and flatsight's aim at the crosshair's point keeps it. 0 otherwise.
[[nodiscard]] float gunRoll(int h);

// The tilt at a wall drawnOffset gave hand `h` last (flatsight::wallTilt; identity outside the mode), and the drawn hand's
// move by it (`shift`): the view puts the hotspot it placed on that drawn weapon back off it, so the game's two-handed grip
// keeps the untilted gun's.
[[nodiscard]] glm::mat3 drawnWallTurn(int h, glm::vec3& shift);

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

// vr_flat_punch [main|off] [v]: a hand's punch from the console, for the tests (V is a key and cannot be driven from a
// script; the fire buttons can, through +attack and +offhandattack). Only while the mode runs, and only with a weapon a
// swing strikes with; `v` as V's punch, whatever the hand holds (a gun's overhead swing).
void punch_f();

// vr_flat_shove_now: the shove key's shove from the console (vr_flat_key_shove's; the tests' button).
void shoveNow_f();

// vr_flat_melee_probe [1|0|done]: each punch's, overhead swing's and shove's numbers, a "flatmelee:" line as each ends
// (the hand over the eye, the weapon up and down, the shove's palm and fingers, the view's tilt); `done` the tests' end.
void meleeProbe_f();

// vr_flat_flick_test [frames] [degrees]: the view flicked hard, as the tests' mouse ("flatflick: done" at its end).
void flickTest_f();
void flickTestFrame();

} // namespace qvr::flatplay
