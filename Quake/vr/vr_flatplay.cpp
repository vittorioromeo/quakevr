// vr_flatplay.cpp -- flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen, with the hands posed by
// the game instead of a headset (docs/vr-port/FLATPLAY.md).
//
// What the mode does, all of it behind the flag:
//   - the flat player's hands are marked as tracked (vr_client.cpp), which is what lets the game code run its VR
//     gameplay for him at all;
//   - the hands are posed from the view angles, the weapon in each hand and the player's own movement: they hold a gun
//     the way a first-person viewmodel does, aim at the crosshair, trail a turn, bob on a walk and breathe at rest
//     (poseHands, called from vr_hands.cpp in place of the straight-out-in-front pose);
//   - mouse 4 (vr_flat_key_grab) force-grabs what the view is pointed at, V punches with the right hand, and either hand's fire button punches
//     with a weapon a swing strikes with: all of it goes through the game's own force grab and melee, a punch being a
//     real motion of the hand, so the melee rules decide whether it lands (vr_flat_punch drives either hand's punch from
//     the console);
//   - 1 and 2 cycle the weapons collected, into the left and the right hand (QC weapons.qc, on the flag);
//   - R reloads (vr_flat_key_reload), and an empty clip does by itself with vr_flat_autoreload on (off by default):
//     the gun's hand goes down to the nearest hip holster and back (vr_flatreload.cpp; QC vr_flatreload.qc), the flashlight is worn on the head (F
//     switches it off and on), and the body presses the buttons it walks into.
//
// The keys are read from the keyboard state, not from bindings, with one exception named below: Left Ctrl is the engine's
// fire key by default (Quake/default_cfg.h binds CTRL +attack) and the mode's crouch key, so while the mode runs CTRL is
// bound to +flatcrouch and nothing fires from it (vr_flat_crouch_fire 1 gives the plain game's bind back). The bind is
// given back when the mode ends and before the engine writes its config out (VR_FlatKeysForConfigWrite, Quake/host.c), so
// the config on disk stays the plain game's. The keys Quake already uses (1 and 2, its own weapon select) are answered by
// the game code only while the flag is on.
//
// -flatscreenmode implies no VR backend (vr_main.cpp's startBackend): the mode runs only while no headset session is up,
// so the flag is what asks for no backend at all, and noteBackendImplied says so once.

#include "vr_flatplay.hpp"
#include "vr_flatholster.hpp"
#include "vr_tracy.h"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_avatar.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_client.hpp"
#include "vr_deathview.hpp"
#include "vr_flathud.hpp"
#include "vr_flathit.hpp"
#include "vr_flatxhair.hpp"
#include "vr_flatleanpose.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatfgrab.hpp"
#include "vr_flatgrenade.hpp"
#include "vr_flatreload.hpp"
#include "vr_flatswap.hpp"
#include "vr_flycam.hpp"
#include "vr_flatsight.hpp"
#include "vr_flataimsens.hpp"
#include "vr_flatspeedfov.hpp"
#include "vr_flatsweep.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_handpose.hpp"
#include "vr_handsource.hpp"
#include "vr_hull.hpp"
#include "vr_main.hpp"
#include "vr_protocol.hpp"
#include "vr_stereo.hpp"
#include "vr_toolgun.hpp"
#include "vr_ragtool.hpp"
#include "vr_slidesound.hpp"
#include "vr_units.hpp"
#include "vr_weapon_ids.h"
#include "vr_weapons.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"

#include <math.h>

extern "C" qboolean keydown[MAX_KEYS]; // keys.c
extern "C" int Key_StringToKeynum(const char* str); // keys.c: a key's number from its bind name (vr_flat_key_swap, ...)

namespace qvr::flatplay
{

namespace
{

// The mode's state for the session (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct FlatState
{
    bool flag{false};            // -flatscreenmode on the command line
    bool announced{false};       // its console line was printed
    bool backendNoted{false};    // the "-flatscreenmode implies no VR backend" line was printed
    bool wasOn{false};           // last frame: to say so when a headset takes the game back
    float dualT{0.f};            // a gun in each hand, eased as the hands are (handEase): the drawn turn of dualProneTurn

    bool keyGrab{false};         // E, last frame (press edges)
    bool keyV{false};            // V, last frame
    bool keyFire[2]{false, false}; // each hand's fire button, last frame: LMB / +attack the main hand,
                                   // RMB / +offhandattack the off hand (vr_client.cpp's attacking)
    int punchFrame[2]{-1, -1};   // the frame each hand's punch started (two hands punching at once)
    double punchAt[2]{-1e9, -1e9}; // per hand: the time its punch started (vr_gametime)
    bool punchOverhead[2]{false, false}; // per hand: that punch is a weapon's overhead swing (vr_flat_swing), not the fist's throw
    int grabs{0};                // vr_flatplay_status's counts
    int punches[2]{0, 0};        // and the punch's, per hand
    // The shove (vr_flat_key_shove, vr_flat_shove_now): the hand pushing, when its push started, whether the game has been
    // told yet (QC VR_FlatShove, as the hand reaches out), and when the key shoves again (vr_flat_shove_cooldown).
    int shoveHand{HAND_OFF};
    bool shoveBoth{false};       // both hands push (QC's two-handed shove: both free, a weapon in each, or vr_flat_shove_both_always)
    double shoveAt{-1e9};
    bool shoveSent{true};
    double shoveReady{-1e9};
    int shoves{0};
    bool keyShove{false};        // the shove key, last frame

    // The game's pose (poseHands): each hand's place off its shoulder in the view's own frame (forward, right, up), eased
    // from one pose to the next (a weapon taken, a punch). In the view's frame, so the hands stay locked to the view.
    glm::vec3 local[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float hang[2]{0.f, 0.f}; // how far an empty hand has gone to hanging at rest (poseHands)
    float guard[2]{0.f, 0.f}; // that rest's share of the guard (vr_flat_empty_hand_pose: 1 the guard, 0 relaxed), eased
    float wallBack[2]{0.f, 0.f}; // how far an empty hand is back towards its shoulder off a wall (units; handOffWall)
    float wallDepth[2]{0.f, 0.f}; // and how deep in it the pose had it this frame (units, its margin in; the status prints it)
    glm::vec3 shoulders[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the arms' joints this pose stands on (avatar::restShoulders)
    bool shouldersKnown{false};
    bool localSeen[2]{false, false};
    double lastTime{-1e9};

    // The tests' probe (probeArm): where each drawn hand's wrist sits on its own arm, learned in the hand's own frame
    // while the arm is long enough to hold it there (so it is the same measure vr_body_wrist_info prints).
    glm::vec3 armMount[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool mountSeen[2]{false, false};
    glm::vec3 lastDrawn[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // (its place last probe: the hand's speed, a punch's strength)
    double probeTime[2]{-1e9, -1e9};
};

FlatState fs;

// The looks the drawn hands are given on top of the game's pose (drawnOffset; stepFeel once a frame). Not the game's:
// the shots go from the steady hands, along the crosshair.
struct Feel
{
    double lastTime{-1e9};
    bool seen{false};
    glm::vec2 view{0.f};         // the view's pitch and yaw, last frame
    double noKickUntil{-1.0};    // a gate's turn of his velocity is no change of it, until this time (cl.time)
    glm::vec2 lag{0.f};          // the turn's lag (degrees, before vr_flat_turnlag) and its rate: a critically damped
    glm::vec2 lagRate{0.f};      // spring pulling the hands back onto the view
    glm::vec3 vel{0.f};          // the player's velocity last frame (world)
    glm::vec3 sway{0.f};         // the step's sway (the view's frame: forward, right, up; units) and its rate: a spring
    glm::vec3 swayRate{0.f};     // kicked by each change of the player's velocity (starting, stopping, a jump, a landing)
    float phase{0.f};            // the walk's cycle
    float amp{0.f};              // how much of a walk's bob (eased with the speed, none in the air)
    float breath{0.f};
    float dt{0.f};               // the frame's step, for what steps per hand (below)
    glm::quat gunTurn[2]{glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat{1.f, 0.f, 0.f, 0.f}}; // each drawn hand's turn in the view's
    bool gunTurnSeen[2]{false, false};                                                   // frame, eased (gunTurnEase)
    double gunTurnAt[2]{-1e9, -1e9};
    glm::mat3 wallTurn[2]{glm::mat3{1.f}, glm::mat3{1.f}}; // each drawn hand's tilt at a wall, as last drawn (drawnWallTurn),
    glm::vec3 wallShift[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // and the hand's move by it (round a point behind the hand)
    glm::vec3 drawnAt[2]{glm::vec3{0.f}, glm::vec3{0.f}};   // each drawn hand, as last drawn (before a wall's tilt): drawnHand
};

Feel feel;

// The keys beyond E and V, and the crouch one of them holds. Its own struct, apart from FlatState's pose fields: the
// mode's keys and its pose are worked on apart (FLATPLAY.md).
struct FlatKeys
{
    bool keyCtrl{false};    // Left Ctrl, last frame (Right Ctrl is the same key code: in_sdl.c maps both to K_CTRL)
    bool keyFlash{false};   // F, last frame
    bool keyDrop{false};    // H (vr_flat_key_drop), last frame
    bool keySwap{false};    // Q (vr_flat_key_swap), last frame
    bool keyTime{false};    // Z (vr_flat_key_bullettime), last frame
    bool cmdHeld{false};    // +flatcrouch held: the console's way of the same hold
    bool keyProne{false};   // X (vr_flat_key_prone), last frame
    bool crouchWas{false};  // the crouch held (Left Ctrl or +flatcrouch), last frame: a press while lying down gets up to it
    bool prone{false};     // lying down asked for (vr_flat_key_prone, vr_prone_toggle): a toggle
    bool riseWanted{false}; // asked to get up (the prone key, the crouch) where no crouched box fits: up once one does
    float proneRatio{0.f};  // lying down's share now: 0 up .. 1 down, eased over vr_prone_body_time (crouchShape)
    float proneLin{0.f};    // its ease's even progress

    float ratio{0.f};       // the crouch's share now: 0 standing .. 1 crouched, eased at vr_flat_crouch_time each way
    float lin{0.f};         // the ease's even progress, which `ratio` is through crouchShape
    double lastTime{-1e9};  // the frame the ease was last stepped at (the ease steps once a frame)
    float lastDt{0.f};      // that step's seconds (vr_flat_crouch_trace's allowance)

    int crouches{0};        // vr_flatplay_status's counts: a crouch hold started
    int toggles{0};         // F: the flashlight switched
    int drops{0};           // H: impulse 47 sent (QC drops what the hand holds)
    int swaps{0};           // Q: impulse 48 sent (QC swaps what the hands hold)
    int times{0};           // Z: bullet time asked to start or stop
};

FlatKeys keyState;

// The crouch's ease (vr_flat_crouch_smooth): the share at even progress x, and back. 1: smoothstep, its steepest 1.5
// times the even speed, at the middle; 0: even.
[[nodiscard]] bool crouchSmooth()
{
    return vr_flat_crouch_smooth.value != 0.f;
}

[[nodiscard]] float crouchShape(float x)
{
    x = za::clamp(x, 0.f, 1.f);
    return crouchSmooth() ? x * x * (3.f - 2.f * x) : x;
}

[[nodiscard]] float crouchUnshape(float y)
{
    y = za::clamp(y, 0.f, 1.f);
    return crouchSmooth() ? 0.5f - std::sin(std::asin(1.f - 2.f * y) / 3.f) : y;
}

// vr_flat_crouch_trace's run: one line a frame of the drawn eye's and the main hand's height, then the biggest step either
// took against the most the ease lets the eye move in a frame (the tests' probe: FLATPLAY.md, "Crouching").
struct CrouchTrace
{
    int left{0};          // frames still to print (0: not running)
    int frames{0};
    int lastFrame{-1};    // the host frame sampled last (one sample a frame)
    bool have{false};     // a frame before this one to step from
    float eye{0.f};       // last frame's heights, world z
    float hand{0.f};
    float eyeStep{0.f};   // the biggest step each took
    float handStep{0.f};
    float allowed{0.f};   // the most the ease allows the eye a frame, at the frames' own times
};

CrouchTrace crouchTrace;
float traceHandZ{0.f}; // poseHands' main hand this frame, world z
glm::vec3 traceHand{0.f};    // poseHands' main hand this frame, world (vr_flatslide_ramp_test)
glm::vec3 traceHead{0.f};    // poseHands' head this frame, world (vr_flatslide_ramp_test)
glm::vec3 traceViewOrg{0.f}; // VR_FlatViewDrop's view origin this frame, world (vr_flatslide_ramp_test)
bool traceBodyOn{false};     // the drawn body's points below wanted (vr_flatslide_ramp_test: the Follower's torso solve)
glm::vec3 traceShoulder{0.f}; // poseHands' main shoulder this frame, world (the body's chest, solveTorso's)
glm::vec3 tracePelvis{0.f};   // the body's pelvis this frame, world (only while traceBodyOn)
int traceArmHolds{0};         // drawn hands the arm's hold pulled in so far (drawnOffset; the slide jitter measure)

// Left Ctrl, the bind the mode takes over (the crouch key must not fire the weapon). Ctrl is the engine's fire key in the
// plain game -- Quake/default_cfg.h binds CTRL +attack -- and flatscreen play reads Ctrl itself for the crouch, so a hold
// would crouch and fire at once. While the mode runs CTRL is bound to +flatcrouch instead, the console's way of the very
// same hold (pollKeysFlat reads the key and the command, and both feed one eased crouch: no second crouch). What CTRL was
// bound to is kept here and not in FlatKeys, because FlatKeys is reset the frame the mode ends and the bind outlives it:
// it is given back when the mode ends, when vr_flat_crouch_fire asks Ctrl to keep firing, and every time the engine is
// about to write its config out (VR_FlatKeysForConfigWrite below, called from Quake/host.c).
struct CtrlBind
{
    char was[1024]{};    // what CTRL held when the mode last took it (empty: it was unbound)
    bool taken{false};   // the mode has it (+flatcrouch)
};

CtrlBind ctrlBind;

// The pose, measured from the shoulders (hands::bodyAnchor with vr_shoulder_offset, the same anchor the body's own
// holsters are placed at): the arms stay on the body whatever the height calibration. Forward, side and down in the
// view's own frame, Quake units: vr_flat_empty_*, vr_flat_gun_*, vr_flat_support_*, each read through poseNum.

// The turn's lag (vr_flat_turnlag, vr_flat_turnlag_speed): the drawn hands keep their direction as the view turns and a
// critically damped spring (this stiff, rad/s) brings them back; this much of what it leaves shown, never more than
// vr_flat_turnlag_cap (10, softly). A turn of 180 degrees a second leaves them ~2 degrees behind, back within a quarter second.
constexpr float lagStiffness = 18.f;
constexpr float lagShown = 0.12f;
constexpr float lagAmountMax = 10.f; // vr_flat_turnlag's range, either way
constexpr float lagCapMax = 30.f;   // vr_flat_turnlag_cap's (degrees)
constexpr double portalNoKickTime = 0.05; // s: a tick and a little: the velocity the client lerps across a gate
constexpr float wallDropFull = 45.f; // drawnOffset: a sighted gun tilted this far at a wall is all of vr_flat_wall_tilt_drop down (degrees)
constexpr float bodyJumpedOver = 64.f; // the body as drawn this far from the pose's: a jump of the player, not a pose (units)
constexpr float lagSnapDeg = 45.f; // a view turned more than this in a frame was set (a teleporter, a respawn): no lag

// The drawn gun's turn onto what the crosshair is on (flatsight: a gun points from its muzzle at that point). The game's
// hand has it at once and the shot goes there, but the crosshair crossing a near edge moves that point by metres in a
// frame: drawn, the gun swings to it at this rate (1/s) instead of snapping. A look, like the sway.
constexpr float gunTurnEase = 18.f;

// The step's sway (vr_flat_sway): each change of the player's velocity kicks the hands the other way (units/s for each
// unit/s of change; more up and down: a jump's and a landing's dip), a spring this stiff and damped settles them.
constexpr float swayKick = 0.12f;
constexpr float swayKickUp = 0.17f;
constexpr float swayStiffness = 11.f;
constexpr float swayDamping = 0.55f; // (of critical: a little overshoot, the settle)
constexpr float maxSway = 5.f;       // units, softly

// The walk's bob (vr_flat_bob): a figure of eight at a run (320 units/s), its rate and size with the speed; breathing
// at rest. Units and degrees in the view's frame.
constexpr float runSpeed = 320.f;
constexpr float bobRate = 1.15f; // cycles a second at a run (two steps each)
constexpr float bobSide = 0.7f;
constexpr float bobUp = 0.6f;
constexpr float bobFwd = 0.6f;    // vr_flat_bob_fwd's at 1: units each way at a run (fwdBob)
constexpr float bobFwdMax = 3.f;  // and its range
constexpr float bobRoll = 0.8f;
constexpr float bobYaw = 0.3f;
constexpr float bobEase = 6.f;
constexpr float breathRate = 1.6f; // rad/s
constexpr float breathUp = 0.2f;
constexpr float breathSide = 0.1f;

// The slide's turn of the body (poseHands): its eased share (no stance turn, the hips and legs to the slide's way), the
// slide's way (degrees, kept from the last frame it was moving) and the chest's turn back to the view (trunkTwist).
struct SlideTurn
{
    float lin{0.f};
    float yaw{0.f};
    float twist{0.f};
    // The hips' yaw eased after where they are asked to face (vr_flat_slide_turn_smooth): its lag behind it (degrees) and
    // the lag's rate, a critically damped spring; `easing` while a slide's share holds it.
    float lag{0.f};
    float lagRate{0.f};
    float yaw0{0.f}; // the eased yaw last frame
    bool easing{false};
};
SlideTurn slideTurn;

// Sliding: the walk's bob (the drawn view's, the hands', the body's ride) times this, vr_flat_slide_bob of it at a full
// slide pose (no walk: the feet stayed on the floor while the body bobbed through it).
[[nodiscard]] float slideBobScale()
{
    return 1.f - (1.f - za::clamp(vr_flat_slide_bob.value, 0.f, 1.f)) * flatmove::slideShare();
}
constexpr float breathPitch = 0.15f;

// The camera the hands are kept on: the drawn view, unless it is somewhere else (the chase camera, the ragdoll tool's
// orbit, a view standing further from his own head than keepNear): then the pose's own head. vr_flat_gun_near, eased to
// vr_flat_ads_near over the ironsight's travel -- and the mode's own nudges of the eye (vr_flat_cam_*) are put back
// before that distance is measured, so moving the eye does not read as "the view is somewhere else" and leave the hands
// and the crosshair measuring from the head while the screen is drawn from the nudged eye.
constexpr float nearMin = 8.f;    // vr_flat_gun_near / vr_flat_ads_near's range
constexpr float nearMax = 60.f;
constexpr float camOffMax = 20.f; // vr_flat_cam_fwd / vr_flat_cam_up / vr_flat_cam_side: how far the eye may be nudged
constexpr float eyeLeanMax = 30.f; // vr_flat_eye_lean's most: the chest's share of a look at 0.5 and this much of a lever carries the eye 13 at 85 (his box's side is 16)
constexpr float eyeOffMax = 8.f; // vr_flat_eye_fwd / vr_flat_eye_up: the eye off the neck. At 8 and 8 a look up takes it 15.3 back; his box's side is 16

// The flatscreen eye's own nudge, in the view's frame: (forward, the view's right, up). 0 0 0 outside the mode, and 0 0 0
// at the defaults -- which is why the defaults leave every number the mode measures exactly as they were.
[[nodiscard]] glm::vec3 camShift()
{
    if(!active())
    {
        return glm::vec3{0.f, 0.f, 0.f};
    }
    return {za::clamp(vr_flat_cam_fwd.value, -camOffMax, camOffMax), za::clamp(vr_flat_cam_side.value, -camOffMax, camOffMax),
        za::clamp(vr_flat_cam_up.value, -camOffMax, camOffMax)};
}

// How far the chest's share of the look carries the neck, and the head on it, off where a level look has them: a turn about the
// spine (vr_flat_eye_lean: how far under the neck it stands; flatplay::chestPitch, the chest's own turn, in solveTorso).
[[nodiscard]] glm::vec3 chestCarry(const glm::vec3& viewAngles)
{
    const float lean = glm::radians(chestPitch(viewAngles.x));
    glm::vec3 levelFwd{}, right{}, levelUp{};
    hands::angleVectors({0.f, viewAngles.y, 0.f}, levelFwd, right, levelUp);
    const float lever = za::clamp(vr_flat_eye_lean.value, 0.f, eyeLeanMax);
    return (levelFwd * za::sin(lean) + levelUp * (za::cos(lean) - 1.f)) * lever;
}

// The neck pivot (vr_flat_eye_pivot): the eye stands off a point in his neck (vr_flat_eye_fwd forward, vr_flat_eye_up up) and the
// look's pitch turns it about that point, so a look down carries it forward and clear of his chest, and a look up back and
// down. Against the level look's eye, the place everything else is measured from: nothing at a level look, whatever the yaw
// (no roll in it: a strafe's tilt of the view doesn't move the eye).
[[nodiscard]] glm::vec3 neckPivotShift(const glm::vec3& viewAngles)
{
    if(!active() || vr_flat_eye_pivot.value == 0.f)
    {
        return glm::vec3{0.f, 0.f, 0.f};
    }
    const float forward = za::clamp(vr_flat_eye_fwd.value, 0.f, eyeOffMax);
    const float upward = za::clamp(vr_flat_eye_up.value, 0.f, eyeOffMax);
    glm::vec3 pitchedFwd{}, levelFwd{}, right{}, pitchedUp{}, levelUp{};
    hands::angleVectors({viewAngles.x, viewAngles.y, 0.f}, pitchedFwd, right, pitchedUp);
    hands::angleVectors({0.f, viewAngles.y, 0.f}, levelFwd, right, levelUp);
    return (pitchedFwd - levelFwd) * forward + (pitchedUp - levelUp) * upward + chestCarry(viewAngles);
}

// The eye's whole move in the world: the neck's turn, and the nudge along the axes of the view it is drawn with (the drawn view's
// angles: its own forward, right and up, punch and idle bob included -- "in the view's own frame of reference" is what the eye
// follows).
[[nodiscard]] glm::vec3 camShiftWorld(const glm::vec3& viewAngles)
{
    const glm::vec3 pivot = neckPivotShift(viewAngles);
    const glm::vec3 off = camShift();
    if(off == glm::vec3{0.f, 0.f, 0.f}) // nothing to nudge (the defaults, and outside the mode)
    {
        return pivot;
    }
    glm::vec3 fwd{}, right{}, up{};
    hands::angleVectors(viewAngles, fwd, right, up);
    return pivot + fwd * off.x + right * off.y + up * off.z;
}

// The nudge alone, in the world: what a pose standing on the body carries (the neck's turn moves the eye off the body, not the hands).
[[nodiscard]] glm::vec3 camNudgeWorld(const glm::vec3& viewAngles)
{
    const glm::vec3 off = camShift();
    glm::vec3 fwd{}, right{}, up{};
    hands::angleVectors(viewAngles, fwd, right, up);
    return fwd * off.x + right * off.y + up * off.z;
}

// The distance the drawn hands and the crosshair's ray stop counting the drawn view as his own eyes at: vr_flat_gun_near
// with the hipfire pose, eased to vr_flat_ads_near over the ironsight's travel.
[[nodiscard]] float keepNear()
{
    const float hip = za::clamp(vr_flat_gun_near.value, nearMin, nearMax);
    const float ads = za::clamp(vr_flat_ads_near.value, nearMin, nearMax);
    return hip + (ads - hip) * za::clamp(flatsight::adsShare(), 0.f, 1.f);
}

// The share of the drawn view's own move off the pose's eye (the walk's bob, a step's easing) the drawn body and hands ride together:
// vr_flat_body_bob, all of it in the ironsight (the sights stand on the view).
[[nodiscard]] float rideShare()
{
    const float share = za::clamp(vr_flat_body_bob.value, 0.f, 1.f);
    return share + (1.f - share) * za::clamp(flatsight::adsShare(), 0.f, 1.f);
}

// Whether the drawn view is his own eyes: not a chase view, and not standing further from his own head than keepNear --
// with the mode's own nudges put back first (see the block above). drawnOffset and crosshairRay both ask this, so the
// drawn weapon and the shot's crosshair can never be measuring from different cameras.
[[nodiscard]] bool viewIsHisOwn(const hands::State& s)
{
    if(chase_active.value)
    {
        return false;
    }
    // (His own eye: with the fly camera up, the view V_CalcRefdef made for him, not the camera's. vr_flycam.cpp)
    const glm::vec3 cam = flycam::eyeOrg();
    return glm::length(cam - camShiftWorld(flycam::eyeAngles()) - s.head) < keepNear();
}

// V's punch, seconds from the press: folded, thrown, held, drawn back.
constexpr float punchWind = 0.19f;   // the fold takes about as long as the throw, on purpose: the game reads the hand's
                                     // motion, not its plan. A pose whose grip moves backward along the punch's own line
                                     // at more than 0.6 of the speed the fist hits with reads as a blow that wiggled out
                                     // and back (VR_MELEE_WIGGLE_BACK, VR_MELEE_WIGGLE_TIME 0.25 s) and is refused, and a
                                     // far point moving backward along its arm reads as pulled back (VR_MELEE_PULLBACK).
                                     // 11 units of fold in 0.19 s peaks at some 2.7 m/s, under the 3.6 that is 0.6 of the
                                     // 6 m/s the fist arrives at; the same fold in the old 0.13 s peaked past 4.5 (2026-11-03).
constexpr float punchStrike = 0.12f;   // the throw is quick on purpose: what the game's melee rules read is the hand's
                                       // speed (vr_melee_speed 3.2 m/s, a weapon's swing 1.25 times that), not how far
                                       // the fist goes, and its reach is capped by the arm (armSpan, punchSwing). Its
                                       // length is also the window the wrist rule measures over (VR_MELEE_WRIST_TIME
                                       // 0.12 s), so a swing thrown this long is judged on this same distance.
constexpr float punchDwell = 0.07f;    // the extension is held a moment at its end before the arm draws back: what it
                                       // reached stays reached, and the draw-back is its own move rather than the tail of
                                       // the throw (measured: without the hold the fist was already on its way back
                                       // through what it punched while the game read the blow, so the only contacts the
                                       // monster's box gave were on the way out)
constexpr float punchBack = 0.22f;
constexpr float punchReach = 24.f;   // units the fist goes forward in the strike
constexpr float punchFold = 6.f;     // where the chambered hand sits: this far out from its shoulder along the arm's own
                                     // line, and punchWindBack below it — the arm folds and the hand drops, so the chamber
                                     // is a bend rather than a walk back through the world (it used to bring the hand to
                                     // 2 units out, 14 units back: the game read that as a punch that pulled back, and its
                                     // speed as a blow that wiggled out and back — both refusals, measured 2026-11-03).
                                     // The fold is as deep as it is because the strike can only throw as far as the arm
                                     // reaches (armSpan, about 22 units here) and the hand already rests 16 of them out: an
                                     // extension from where it rests moves the fist 6 units, which read as the hand's speed
                                     // is well under what a landed blow needs (QC VR_Melee_LandNeed: 4.32 m/s a fist, 4.00
                                     // an axe), so the fold gathers the rest of the arm's length. At punchFold 6 the
                                     // chambered hand sits 5.8 out and 8.5 below its shoulder; measured the same day, the
                                     // throw then travels 17.6 units (16.1 of them forward) in its 0.12 s — a fist about
                                     // 6.7 m/s against the 4.0 an axe's blow asks for, a grip net of 0.49 m against the
                                     // 0.33 a swing at that speed is charged (VR_MELEE_RUN 0.2 with VR_MELEE_SLOW_RUN 0.4),
                                     // and a wrist 0.35 m over the wrist rule's own 0.12 s window against the 2.53 m/s a
                                     // swing's wrist is charged (vr_melee_wrist_speed 1.1 times VR_MELEE_WRIST_SWING 2.3).
constexpr float punchWindBack = 7.f;   // and how much lower it sits there
constexpr float punchLevel = 0.25f;    // the thrown fist's height, times where its hand rests at (0: level with its shoulder, 1: as it rests)

// Easing from one pose to the next (a weapon taken, a punch, a reach), in the view's frame.
constexpr float handEase = 15.f;
constexpr float hangPitch = 60.f; // an empty hand at rest: its fingers this far below level (degrees)
constexpr float guardFwd = 12.f;  // the guard a free hand beside a gun punches from (punchSwing): forward of its shoulder,
constexpr float guardDown = 11.f; // and below it, before the reach

// An empty hand off walls and props (handOffWall): kept vr_flat_wall_margin and this much more off them at rest (the fist
// round its point, as flatsight's freeRoom), found in this many halvings of the way to its shoulder, and eased back out
// at this rate (1/s, as flatsight's backOut: no jump as the wall clears; it goes back at once).
constexpr float handRoom = 2.f;
constexpr int handOffHalvings = 8;
constexpr float handBackOut = 12.f;
constexpr float handMarginMax = 24.f;

// V's punch and E's reach last this long after the press (vr_flatplay_status).
constexpr float punchTotal = punchWind + punchStrike + punchDwell + punchBack;

// A weapon's overhead swing (vr_flat_swing; his notes 2026-11-10): seconds of its windup (the hand up over the head, the
// weapon turned up), its hold at the top, the strike down to the crosshair, the hold at its end, and the way back.
struct SwingTimes
{
    float wind, top, strike, dwell, back;
};

[[nodiscard]] SwingTimes swingTimes()
{
    return {za::clamp(vr_flat_swing_wind.value, 0.05f, 1.f), za::clamp(vr_flat_swing_top.value, 0.f, 0.5f),
        za::clamp(vr_flat_swing_strike.value, 0.05f, 0.5f), za::clamp(vr_flat_swing_dwell.value, 0.f, 0.5f),
        za::clamp(vr_flat_swing_back.value, 0.05f, 1.f)};
}

// How long `h`'s punch lasts: the fist's throw, or a weapon's overhead swing.
[[nodiscard]] float punchLen(int h)
{
    if(!fs.punchOverhead[h])
    {
        return punchTotal;
    }
    const SwingTimes w = swingTimes();
    return w.wind + w.top + w.strike + w.dwell + w.back;
}

// And how long its last part, the arm drawn back, takes.
[[nodiscard]] float punchBackLen(int h)
{
    return fs.punchOverhead[h] ? swingTimes().back : punchBack;
}

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
}

// `x`, never past `cap` either way: unchanged while small, bending smoothly towards it.
[[nodiscard]] float softCap(float x, float cap)
{
    return cap > 0.f ? x / za::sqrt(1.f + (x / cap) * (x / cap)) : 0.f;
}

// 0 before `start` + `len` seconds has begun, 1 at its end, eased at both ends (so its speed is continuous).
[[nodiscard]] float slide(double t, double start, float len)
{
    if(t <= start)
    {
        return 0.f;
    }
    const float u = za::clamp(static_cast<float>((t - start) / len), 0.f, 1.f);
    return u * u * (3.f - 2.f * u);
}

[[nodiscard]] bool bodyPose()
{
    return vr_flat_body_pose.value != 0.f;
}

// The old pose's anchor against the body's real shoulder at a level look, standing: vr_flat_gun_*, _empty_*, _support_* are measured
// from the real one, in the view's frame (forward, out at the hand's side, up), so with vr_flat_body_pose 0 they come back off the
// anchor by this. And a punch keeps the old anchor's numbers: where it winds up and how far it throws (punchSwing) were tuned on them.
constexpr glm::vec3 anchorFromShoulder{2.5f, -3.f, 12.f};

// Where a shoulder is: the joint the arm hangs from, before the hand lifts it (avatar::restShoulders: his crouch, the stance's
// turn and the chest's share in it), kept by poseHands; with vr_flat_body_pose 0 the old anchor (hands::bodyAnchor).
[[nodiscard]] glm::vec3 shoulder(const hands::State& s, int h)
{
    if(bodyPose() && fs.shouldersKnown)
    {
        return fs.shoulders[h];
    }
    const glm::vec3 off{vr_shoulder_offset_x.value, vr_shoulder_offset_y.value, vr_shoulder_offset_z.value};
    return h == HAND_OFF ? hands::bodyAnchor(s, {off.x, -off.y, off.z}) : hands::bodyAnchor(s, off); // mirrored across the body's centre line: only the side changes (as vr_body.cpp's and vr_twohand.cpp's shoulders)
}

// The weapon id `h` holds (STAT_QVR_WEAPON / STAT_QVR_WEAPON2: the ids the game code and the engine share,
// vr_weapon_ids.h); WID_FIST for an empty hand.
[[nodiscard]] int heldWid(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

// The hands a fire button punches with: the weapon's own trigger does the striking, so only a hand that strikes with a
// swing punches — an empty hand (the fist: it is in the flatscreen cycle, QC weapons.qc), the axe, HIPNOTIC's mjolnir, a
// knight's sword, the crowbar, the super axe. Not the chainsaw (its trigger runs the chain: vr_chainsaw.qc), not the
// toolgun or the grapple, not a gun (its trigger fires it).
[[nodiscard]] bool punchesWith(int h)
{
    switch(heldWid(h))
    {
    case WID_FIST:
    case WID_AXE:
    case WID_MJOLNIR:
    case WID_SWORD:
    case WID_CROWBAR:
    case WID_SUPERAXE: return true;
    default: return false;
    }
}

// What the drawn arm can hold a hand at, world units from its shoulder: the body's arm (vr_body_arm_length times the
// model's bones), stretched up to vr_body_arm_stretch, then the shoulder reaching out by vr_body_shoulder_reach — the
// same order and the same numbers vr_avatar.cpp solves the arm with ("only past all of it does the hand leave the arm").
// Body Calibration's own measured arms differ from the model's; the tests run without it.
[[nodiscard]] float armSpan()
{
    const float m2w = units::metresToUnits() * units::bodyScale();
    float upper, fore;
    avatar::armBones(upper, fore);
    const float bones = (upper + fore) * m2w * za::clamp(vr_body_arm_length.value, 0.5f, 2.f);
    return bones * za::max(1.f, vr_body_arm_stretch.value) + za::max(0.f, vr_body_shoulder_reach.value) * m2w;
}

// The body pose's limits, the part of them that is shape and not taste.
constexpr float poseTweakMax = 10.f;  // the body pose's tweaks in units (vr_flat_arm_out_down, vr_flat_crouch_shoulder_*)
constexpr float clearMost = 15.f;     // vr_flat_clear_chest, vr_flat_clear_eye
constexpr float clearEase = 3.f;      // the chest's limit eases in over this much above it, units
constexpr float clearFadeAbove = 6.f; // a hand this far over its shoulder has no chest to clear
constexpr float eyeEase = 1.5f;       // the eye's limit, the same
constexpr float neckLength = 0.09f;   // the neck under the top of it, metres of the body (vr_flatsweep's shape)
constexpr float screenEase = 2.f;     // the screen's limit, the same
constexpr float keepEase = 0.1f;      // the arm limits, the same, as a share of the arm
constexpr float acrossKeep = 0.8f;    // the stance's hotspot keeps across the view within this much of what the free arm keeps it at
constexpr float lookMost = 85.f;      // a look straight down, for the arm's move out

constexpr float stanceFadeFrom = 10.f; // the look's degrees off level the stance starts fading at, and has gone by
constexpr float stanceFadeTo = 55.f;

// Lying down's share of the held weapon's pose (vr_flat_gun_*_prone): how far down he is, with the body pose on.
[[nodiscard]] float proneShare()
{
    return bodyPose() ? keyState.proneRatio : 0.f;
}

// A lying-down gun pose number at the look `pitch` (Quake's: positive down): `level` at a level look, going to `down`
// looking straight down and `up` straight up along the same smooth curve as Gun Out's (gunOutAt).
[[nodiscard]] float proneAt(const Tunable& level, const Tunable& down, const Tunable& up, float pitch)
{
    const float l = poseNum(level);
    const float w = geom::smooth01(za::clamp(glm::abs(pitch) / lookMost, 0.f, 1.f));
    return l + (poseNum(pitch >= 0.f ? down : up) - l) * w;
}

// Dual wielding lying down, hand `h`'s gun's own nudge (vr_flat_dual_prone_main_* / _off_*): forward, out at its side, up.
[[nodiscard]] glm::vec3 dualProneMove(int h)
{
    return h == HAND_OFF ? glm::vec3{poseNum(vr_flat_dual_prone_off_fwd), poseNum(vr_flat_dual_prone_off_out), poseNum(vr_flat_dual_prone_off_up)}
                         : glm::vec3{poseNum(vr_flat_dual_prone_main_fwd), poseNum(vr_flat_dual_prone_main_out), poseNum(vr_flat_dual_prone_main_up)};
}

// And its drawn turn (degrees: the muzzle up, out at its side, the top canted out), as Quake's angles of the hand's own
// frame (pitch down, yaw left, roll the right side down).
constexpr float dualProneTurnMost = 45.f;
[[nodiscard]] glm::vec3 dualProneTurn(int h)
{
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const auto deg = [](const Tunable& c) { return za::clamp(c.value, -dualProneTurnMost, dualProneTurnMost); };
    const bool off = h == HAND_OFF;
    return {-deg(off ? vr_flat_dual_prone_off_pitch : vr_flat_dual_prone_main_pitch),
        -side * deg(off ? vr_flat_dual_prone_off_yaw : vr_flat_dual_prone_main_yaw),
        side * deg(off ? vr_flat_dual_prone_off_roll : vr_flat_dual_prone_main_roll)};
}

// The held weapon's Gun Out at the look `pitch` (Quake's: positive down): vr_flat_gun_out at a level look (crouched,
// vr_flat_gun_out_crouch, by how deep), going to vr_flat_gun_out_down looking straight down and vr_flat_gun_out_up straight up
// along a smooth curve (no corner at level or at the ends): the gun comes in towards the middle looking up or down, as his
// tuning has it (1 level, -4 at either end, -2 crouched).
[[nodiscard]] float gunOutAt(float pitch)
{
    const float crouched = bodyPose() ? keyState.ratio : 0.f;
    const float level = poseNum(vr_flat_gun_out) + (poseNum(vr_flat_gun_out_crouch) - poseNum(vr_flat_gun_out)) * crouched;
    const float end = poseNum(pitch >= 0.f ? vr_flat_gun_out_down : vr_flat_gun_out_up);
    const float w = geom::smooth01(za::clamp(glm::abs(pitch) / lookMost, 0.f, 1.f));
    const float upright = level + (end - level) * w;
    // (Lying down, its own: vr_flat_gun_out_prone, going to _prone_down / _prone_up the same way, by how far down he is.)
    return upright + (proneAt(vr_flat_gun_out_prone, vr_flat_gun_out_prone_down, vr_flat_gun_out_prone_up, pitch) - upright) *
                         proneShare();
}

// Where a hand sits relative to its shoulder, in the view's frame: the held weapon's pose (vr_flat_gun_ahead / _down /
// _out), the free hand's next to the other hand's gun (vr_flat_support_*), an empty hand's (vr_flat_empty_*). Positive
// is forward, out at that hand's own side, and down (the z term reads the negative of the cvar, which is why the cvars
// read "how far below": a bigger number is a lower hand, as it was when these were the constants). vr_flat_reach
// multiplies the whole thing, as it did before.
[[nodiscard]] glm::vec3 targetOffset(int h, float reach, bool otherHolds, float pitch, float stanceAsked = -1.f)
{
    const float side = h == HAND_OFF ? -1.f : 1.f;
    if(heldIn(h) >= 0)
    {
        // (The stance's own, on a long gun: flatsight::stanceAndLook. A level look's: a look up or down takes it off, and so does
        // some of it a crouch, whose trunk leans into the gun.)
        const float away = bodyPose() ? za::clamp((glm::abs(pitch) - stanceFadeFrom) / (stanceFadeTo - stanceFadeFrom), 0.f, 1.f) : 0.f;
        const float crouched = bodyPose() ? keyState.ratio * za::clamp(vr_flat_stance_fade_crouch.value, 0.f, 1.f) : 0.f;
        const float fade = (1.f - crouched) * (1.f - za::clamp(vr_flat_stance_fade.value, 0.f, 1.f) * away * away * (3.f - 2.f * away));
        const float stance = (stanceAsked >= 0.f ? stanceAsked : flatsight::stanceShare(h)) * fade;
        // (A gun whose grip the free hand takes for real gets the stance's height, not its way back or in: the same height on the
        // screen as the others. Pulled back, the free hand's punch missed: FLATPLAY.md, "The stance".)
        const float lift = (stanceAsked >= 0.f ? stanceAsked : flatsight::stanceLift(h)) * fade;
        // (Crouched, the gun goes out at its side, its stock off the chest the crouch leans forward.)
        const float crouchOut = bodyPose() ? keyState.ratio * za::clamp(vr_flat_crouch_gun_out.value, -poseTweakMax, poseTweakMax) : 0.f;
        // (A pistol's stance: both hands in front of him, its own numbers. A long gun's: towards the shoulder pocket.)
        const bool pistol = flatsight::stancePistol();
        const float back = poseNum(pistol ? vr_flat_pistol_back : vr_flat_stance_back);
        const float in = poseNum(pistol ? vr_flat_pistol_in : vr_flat_stance_in);
        const float upBy = poseNum(pistol ? vr_flat_pistol_up : vr_flat_stance_up);
        // (Gun Forward and Gun Down crouched take over from the standing ones by how deep the crouch is, as Gun Out
        // Crouched does in gunOutAt: vr_flat_gun_ahead_crouch, vr_flat_gun_down_crouch.)
        const float deep = bodyPose() ? keyState.ratio : 0.f;
        const float aheadUp = poseNum(vr_flat_gun_ahead) + (poseNum(vr_flat_gun_ahead_crouch) - poseNum(vr_flat_gun_ahead)) * deep;
        const float belowUp = poseNum(vr_flat_gun_down) + (poseNum(vr_flat_gun_down_crouch) - poseNum(vr_flat_gun_down)) * deep;
        // (Lying down, Gun Forward / Down Prone take over by how far down he is, each going to its own looking straight
        // down or up: vr_flat_gun_ahead_prone(_down, _up), vr_flat_gun_down_prone(_down, _up).)
        const float prone = proneShare();
        const float ahead = aheadUp + (proneAt(vr_flat_gun_ahead_prone, vr_flat_gun_ahead_prone_down, vr_flat_gun_ahead_prone_up, pitch) - aheadUp) * prone;
        const float below = belowUp + (proneAt(vr_flat_gun_down_prone, vr_flat_gun_down_prone_down, vr_flat_gun_down_prone_up, pitch) - belowUp) * prone;
        // (In a slide, its own nudges by the slide's share: vr_flat_gun_ahead_slide, _down_slide, _out_slide.)
        const float slid = bodyPose() ? flatmove::slideShare() : 0.f;
        const glm::vec3 slideBy{poseNum(vr_flat_gun_ahead_slide), poseNum(vr_flat_gun_out_slide), poseNum(vr_flat_gun_down_slide)};
        // (Dual wielding lying down, each hand's own nudge by how far down he is: no stance lifts two guns, and at Gun Down
        // Prone they hung a hand's width over the floor, their grips in it: vr_flat_dual_prone_main_* / _off_*.)
        const glm::vec3 dual = otherHolds ? dualProneMove(h) * prone : glm::vec3{0.f};
        return {(ahead - stance * back + slid * slideBy.x + dual.x) * reach,
            side * (gunOutAt(pitch) - stance * in + crouchOut + slid * slideBy.y + dual.y) * reach,
            -(below - lift * upBy + slid * slideBy.z - dual.z) * reach};
    }
    const glm::vec3 empty{
        poseNum(vr_flat_empty_fwd) * reach, side * poseNum(vr_flat_empty_out) * reach, -poseNum(vr_flat_empty_down) * reach};
    if(otherHolds)
    {
        // (At a wall the free hand lets go of the other's gun, to an empty hand's place: flatsight::wallLetGo.)
        const glm::vec3 support{
            poseNum(vr_flat_support_fwd) * reach, side * poseNum(vr_flat_support_out) * reach, -poseNum(vr_flat_support_down) * reach};
        return support + (empty - support) * flatsight::wallLetGo(1 - h);
    }
    return empty;
}

// A point on the punch's line: u of the way from `from` to `to` (both where a hand sits off its shoulder, in the view's
// frame). The line lies in the view's forward and up, the plane the old straight punch moved in: a punch stays a punch
// along the aim, and the hand's side off the centre line is held all the way along.
[[nodiscard]] glm::vec3 punchPoint(const glm::vec3& from, const glm::vec3& to, float u)
{
    return from + (to - from) * u;
}

// A hand's punch: the arm folded, the fist thrown along the arm's own line, the extension held, the arm drawn back, from
// where the hand rests. The fist is chambered to punchFold out from its shoulder along the arm's own line and
// punchWindBack lower — the arm folds and the hand drops. A punch thrown from where a hand hangs has to gather its speed
// somewhere, and gathering it by dragging the hand back through the world is what the game charges it for: it used to
// chamber 16 units back along the line, some 12 units of travel arriving as over 4 m/s of backward grip speed, and the
// game's own rules read that as a punch that pulled back and a blow that wiggled out and back, and refused it both ways
// (measured 2026-11-03). The throw is then an extension: from the fold out to what the arm can hold it at, along the same
// line the arm already lies on, so the arm straightens and the wrist travels with it — the arm solver puts the wrist on
// the shoulder-to-hand line, which is what the game's wrist rule measures, while a hand shoved along the aim over a bent
// arm moves the wrist nothing.
// (punchReach is the punch asking for its whole length; what the arm has is what it gets. The punch used to be a
// straight shove off the shoulder, past the arm's reach by as much as 14 units and its hand left hanging off the wrist:
// flatarm's `past its arm` said so, and drawnOffset holds a look, a knock or a shake to the same arm). The thrown fist
// keeps near the height its hand rests at (punchLevel): a hanging hand's fist goes forward rather than down, and a fist
// already out in front goes further forward rather than up into an upper-cut. Forward is what a blow needs - a part of
// what the hand holds must sweep through the thing in front of it, at vr_melee_speed (3.2 m/s, a weapon's swing 1.25
// times that, a stab 0.75: QC vr_melee.qc), and a fist thrown over what it was aimed at sweeps nothing: the punch used
// to rise into an upper-cut and dealt no damage at all to a knight standing in front of the player.
// `target`: what the crosshair is on (the point vr_flat_punch_aim_dist along its ray), off the same shoulder in the same
// frame as `rest`: the fist is thrown at it by vr_flat_punch_aim (his notes 2026-11-08: it hit low, beside the target).
[[nodiscard]] glm::vec3 punchSwing(int h, const glm::vec3& rest, const glm::vec3& target)
{
    const double t = vr_gametime - fs.punchAt[h];   // the press's own clock: seconds since the punch was started. Each
                                                    // phase's start is on that same clock, so a phase eased from its
                                                    // start actually moves: given the press's absolute time instead, the
                                                    // easing never got past its first step and the whole punch arrived as
                                                    // three snaps between its poses (measured: 7, 16 and 22 units in a
                                                    // frame - longer than the 3 units a blow has to find in its sweep).
    if(t < 0.0 || t > punchTotal || fs.punchOverhead[h])
    {
        return rest; // (a weapon's overhead swing is overheadSwing's)
    }
    const float span = za::max(4.f, armSpan());
    // A free hand beside a gun hangs low: its punch is measured from where a guard stands, not thrown at the floor.
    const float guardReach = za::clamp(vr_flat_reach.value, 0.25f, 3.f);
    const bool guards = heldIn(h) < 0 && heldIn(1 - h) >= 0;
    const glm::vec2 plane = guards ? glm::vec2{guardFwd * guardReach, -guardDown * guardReach} : glm::vec2{rest.x, rest.z}; // the punch's plane: the view's forward, its up and down
    const float r0 = za::max(0.5f, glm::length(plane));
    const float wound_share = za::min(r0, punchFold) / r0;                     // folded along its own line, to punchFold out
    const glm::vec3 wound{plane.x * wound_share, rest.y, plane.y * wound_share - punchWindBack};
    const glm::vec2 throwDir{plane.x, plane.y * punchLevel};                   // thrown along the aim, kept near its own height
    const float hit_share = za::min(span, r0 + punchReach) / za::max(0.05f, glm::length(throwDir));
    glm::vec3 thrown{plane.x * hit_share, rest.y, plane.y * punchLevel * hit_share};
    if(const float aimShare = za::clamp(vr_flat_punch_aim.value, 0.f, 1.f); aimShare > 0.f && glm::length(target) > 1.f)
    {
        // At the target, out to what the arm reaches (as far as the old throw went), never past the target itself.
        const float reachTo = za::min(glm::length(target), za::min(span, r0 + punchReach));
        thrown += (glm::normalize(target) * reachTo - thrown) * aimShare;
    }
    if(t < punchWind)
    {
        return punchPoint(rest, wound, slide(t, 0.0, punchWind));
    }
    if(t < punchWind + punchStrike)
    {
        return punchPoint(wound, thrown, slide(t, punchWind, punchStrike));
    }
    if(t < punchWind + punchStrike + punchDwell)
    {
        return thrown;   // the extension held where it reached: what the punch got to stays got to, and the draw-back is
                         // its own move after it rather than the tail of the throw. Measured without the hold, the fist
                         // was already on its way back through what it punched while the game read the blow, and the only
                         // contacts the monster's box gave came on the way out (2026-11-03).
    }
    return punchPoint(thrown, rest, slide(t, punchWind + punchStrike + punchDwell, punchBack));
}

// What the punch, the swing and the shove go for, from shoulder `root` (his notes 2026-11-10: the swing a bit off centre):
// what the crosshair's ray hits, as the guns aim (flatsight::crosshairHit; vr_flat_melee_aim_hit), no further along the
// ray than the arm reaches from `root` (`armReach`: a punch or a swing, so the blow ends on the ray rather than beside it)
// or than vr_flat_punch_aim_dist (the shove's push, whose hands travel vr_flat_shove_anim_reach whatever the arm). 0:
// vr_flat_punch_aim_dist along the ray, as before.
[[nodiscard]] glm::vec3 meleeAimPoint(const hands::State& s, const glm::vec3& aim, const glm::vec3& root, bool armReach)
{
    glm::vec3 org, dir;
    crosshairRay(s, aim, org, dir);
    const float fixed = za::clamp(vr_flat_punch_aim_dist.value, 8.f, 96.f);
    if(vr_flat_melee_aim_hit.value == 0.f)
    {
        return org + dir * fixed;
    }
    const float hit = glm::dot(flatsight::crosshairHit(s, aim) - org, dir);
    // (The furthest point along the ray the arm reaches: |org + dir d - root| = its span.)
    const glm::vec3 w = org - root;
    const float b = glm::dot(w, dir), span = za::max(4.f, armSpan());
    const float disc = b * b - (glm::dot(w, w) - span * span);
    const float reach = armReach && disc > 0.f ? -b + za::sqrt(disc) : fixed;
    return org + dir * za::clamp(za::min(hit, reach), 8.f, 96.f);
}

// A weapon's overhead swing (vr_flat_swing; his notes 2026-11-10, replacing the fist's straight throw for anything held):
// the hand goes up over the head (vr_flat_swing_raise_fwd / _up / _out: off the eye) with the weapon turned up
// (vr_flat_swing_raise_pitch, and _raise_yaw / _raise_roll), waits a moment there, and comes down round the shoulder to
// what the crosshair is on (`at`, under it by vr_flat_swing_end_down, ahead by _end_fwd, out by _end_out, at what the arm
// reaches), the wrist turning all the way through until the weapon points down (vr_flat_swing_end_pitch, and _end_yaw /
// _end_roll); held, then back to `rest`, where the hand is posed without it. World places; `root` the shoulder, `fwd`,
// `right`, `up` the frame the hands are placed in. `turn`: how far the weapon is turned off its pose (degrees: up, out
// to its side, rolled; turnedHand). The blow is the game's to find, as the fist's: the hand's motion is a real one (an
// action, vr_handsource.hpp), and what the weapon's far end sweeps through on the way down is what it hits; asked for by
// a press, the melee rules take it as a deliberate blow whatever the windup (QC VR_HandMotionAction). The windup eases
// into its top and waits there.
[[nodiscard]] glm::vec3 overheadSwing(int h, const glm::vec3& root, const glm::vec3& rest, const glm::vec3& eye, const glm::vec3& at,
    const glm::vec3& fwd, const glm::vec3& right, const glm::vec3& up, glm::vec3& turn)
{
    turn = glm::vec3{0.f};
    const double t = vr_gametime - fs.punchAt[h];
    if(!fs.punchOverhead[h] || t < 0.0 || t > punchLen(h))
    {
        return rest;
    }
    const SwingTimes w = swingTimes();
    const float span = za::max(4.f, armSpan());
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const auto inReach = [&root](const glm::vec3& p, float most) {
        const glm::vec3 d = p - root;
        const float len = glm::length(d);
        return len > most && len > 0.001f ? root + d * (most / len) : p;
    };
    const auto offset = [](const Tunable& t) { return za::clamp(t.value, -24.f, 24.f); };
    const auto angle = [](const Tunable& t, float most) { return za::clamp(t.value, -most, most); };
    const glm::vec3 top = inReach(eye + up * offset(vr_flat_swing_raise_up) + fwd * offset(vr_flat_swing_raise_fwd) +
                                      right * (side * offset(vr_flat_swing_raise_out)),
        span * 0.98f);
    const glm::vec3 toAim = at - up * offset(vr_flat_swing_end_down) + fwd * offset(vr_flat_swing_end_fwd) +
                            right * (side * offset(vr_flat_swing_end_out)) - root;
    const float aimLen = glm::length(toAim);
    const glm::vec3 end = root + (aimLen > 0.001f ? toAim / aimLen : fwd) * za::min(span, aimLen);
    const glm::vec3 upTurn{za::clamp(vr_flat_swing_raise_pitch.value, 0.f, 150.f), angle(vr_flat_swing_raise_yaw, 90.f),
        angle(vr_flat_swing_raise_roll, 90.f)};
    // The strike turns the wrist to vr_flat_swing_end_pitch, whatever the hand holds: that is the blow. A melee
    // weapon's haft stands up the hand, not along the aim as a gun's barrel does, so at that angle its head is still
    // over the hand (seen 2026-11-10); it follows through to vr_flat_swing_end_pitch_melee over the hold after the
    // strike, turned on by _end_yaw_melee and rolled by _end_roll_melee. (Turned that far within the strike itself, the
    // axe missed a knight 24 ahead at every angle past 50: the blow is the strike's, the follow-through only a look.)
    const float downPitch = za::clamp(vr_flat_swing_end_pitch.value, -30.f, 90.f);
    const glm::vec3 downTurn{-downPitch, angle(vr_flat_swing_end_yaw, 90.f), angle(vr_flat_swing_end_roll, 90.f)};
    const glm::vec3 followTurn = meleeIn(h) ? glm::vec3{-za::max(downPitch, za::clamp(vr_flat_swing_end_pitch_melee.value, -30.f, 150.f)),
                                                  downTurn.y + angle(vr_flat_swing_end_yaw_melee, 90.f),
                                                  downTurn.z + angle(vr_flat_swing_end_roll_melee, 90.f)}
                                            : downTurn;
    double k = t;
    if(k < w.wind)
    {
        const float u = slide(k, 0.0, w.wind);
        turn = upTurn * u;
        return rest + (top - rest) * u;
    }
    k -= w.wind;
    if(k < w.top)
    {
        turn = upTurn;
        return top;
    }
    k -= w.top;
    if(k < w.strike)
    {
        const float u = slide(k, 0.0, w.strike);
        turn = upTurn + (downTurn - upTurn) * u;
        // Round the shoulder: the way out of it turned from the top's to the end's, its length eased between.
        const glm::vec3 a = top - root, b = end - root;
        const float la = glm::length(a), lb = glm::length(b);
        const float ang = la > 0.001f && lb > 0.001f ? za::acos(za::clamp(glm::dot(a / la, b / lb), -1.f, 1.f)) : 0.f;
        if(ang < 1e-3f || ang > 3.f)
        {
            return top + (end - top) * u;
        }
        const glm::vec3 dir = (a / la * za::sin((1.f - u) * ang) + b / lb * za::sin(u * ang)) / za::sin(ang);
        return root + dir * (la + (lb - la) * u);
    }
    k -= w.strike;
    if(k < w.dwell)
    {
        turn = downTurn + (followTurn - downTurn) * slide(k, 0.0, w.dwell);
        return end;
    }
    k -= w.dwell;
    const float u = slide(k, 0.0, w.back);
    turn = followTurn * (1.f - u);
    return end + (rest - end) * u;
}

// Hand angles `rot` turned by `turn` (degrees): up by its x about the hand's own right (negative: down), then out to the
// hand's own side by its y about its up (`side`: 1 the main hand, -1 the off hand: mirrored), then rolled by its z about
// its forward, the top out to its own side.
[[nodiscard]] glm::vec3 turnedHand(const glm::vec3& rot, const glm::vec3& turn, float side)
{
    if(turn == glm::vec3{0.f})
    {
        return rot;
    }
    glm::vec3 f, r, u;
    hands::angleVectors(rot, f, r, u);
    const float p = glm::radians(turn.x), y = glm::radians(turn.y * side), z = glm::radians(turn.z * side);
    const glm::vec3 f1 = f * za::cos(p) + u * za::sin(p), u1 = u * za::cos(p) - f * za::sin(p);
    const glm::vec3 f2 = f1 * za::cos(y) + r * za::sin(y), r2 = r * za::cos(y) - f1 * za::sin(y);
    return hands::anglesFromVectors(f2, u1 * za::cos(z) + r2 * za::sin(z));
}

// A shoving hand's angles: the palm to `toTarget`, the fingers up (vr_flat_shove_palm_up above level; vr_flat_shove_palm_turn
// about them, mirrored for the off hand: the hand at roll 0 has its palm in towards the middle, as on a grip).
[[nodiscard]] glm::vec3 palmAt(int h, const glm::vec3& toTarget, const glm::vec3& viewUp)
{
    const float len = glm::length(toTarget);
    const glm::vec3 f0 = len > 0.001f ? toTarget / len : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 u0 = viewUp - f0 * glm::dot(viewUp, f0);
    u0 = glm::length(u0) > 0.001f ? glm::normalize(u0) : glm::vec3{0.f, 0.f, 1.f};
    const float a = glm::radians(za::clamp(vr_flat_shove_palm_up.value, 0.f, 90.f));
    const glm::vec3 fingers = f0 * za::cos(a) + u0 * za::sin(a);
    const glm::vec3 back = u0 * za::cos(a) - f0 * za::sin(a);
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const float turn = -side * glm::radians(za::clamp(vr_flat_shove_palm_turn.value, -180.f, 180.f));
    return hands::anglesFromVectors(fingers, back * za::cos(turn) + glm::cross(fingers, back) * za::sin(turn));
}

[[nodiscard]] glm::quat quatOfAngles(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::quat_cast(glm::mat3{f, -r, u});
}

// Hand angles `a` turned `k` (0 .. 1) of the way to `b`, the short way round.
[[nodiscard]] glm::vec3 blendAngles(const glm::vec3& a, const glm::vec3& b, float k)
{
    if(k <= 0.f)
    {
        return a;
    }
    if(k >= 1.f)
    {
        return b;
    }
    const glm::mat3 m = glm::mat3_cast(glm::slerp(quatOfAngles(a), quatOfAngles(b), k));
    return hands::anglesFromVectors(m[0], m[2]);
}

// Whether `h`'s punch has reached its draw-back (or is not running).
[[nodiscard]] bool drawingBack(int h)
{
    return !punching(h) || vr_gametime - fs.punchAt[h] >= punchLen(h) - punchBackLen(h);
}

// A hand's punch, from V, from its fire button, or from vr_flat_punch. Each hand has its own clock. One hand at a time
// (his notes 2026-11-10: no punching with both at once): the other punches only once this one draws back
// (vr_flat_punch_both 1: as before, a hand that started one this very frame keeps it, and the other joins in only when
// both hold a weapon a swing strikes with). With a weapon in the hand it is the overhead swing (vr_flat_swing).
void startPunch(int h, const char* from)
{
    if(punching(h))
    {
        return; // a press while it runs is not a new punch: restarted, the hand snaps back to the rest pose
    }
    if(vr_flat_punch_both.value != 0.f)
    {
        if(fs.punchFrame[1 - h] == host_framecount && !(punchesWith(HAND_OFF) && punchesWith(HAND_MAIN)))
        {
            return;
        }
    }
    else if(!drawingBack(1 - h))
    {
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s punch (%s): the other hand's punch is still out\n", h == HAND_MAIN ? "main" : "off", from);
        }
        return;
    }
    fs.punchAt[h] = vr_gametime;
    fs.punchFrame[h] = host_framecount;
    fs.punchOverhead[h] = vr_flat_swing.value != 0.f && heldIn(h) >= 0;
    fs.punches[h]++;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: %s punch (%s): %s%s\n", h == HAND_MAIN ? "main" : "off", from,
            punchesWith(h) ? "melee" : "not a swinging weapon", fs.punchOverhead[h] ? ", overhead swing" : "");
    }
}

// Whether `h` pushes in the shove (the one hand, or both).
[[nodiscard]] bool inShove(int h)
{
    return h == fs.shoveHand || fs.shoveBoth;
}

// The shove's parts (his notes 2026-11-10: a windup like the fist's, then a fast push; each part its own time): seconds of
// its windup (the hands drawn back, the palms turning to the target), its push out (it lands at its end), its hold out,
// and the hands' way back to where they were (vr_flat_shove_<wind|push|hold|recover>_time; until 2026-11-10 shares of
// vr_flat_shove_time 0.4, the same look), all played vr_flat_shove_anim_speed times faster.
struct ShoveTimes
{
    float wind, push, hold, back;
};

[[nodiscard]] ShoveTimes shoveTimes()
{
    const float k = 1.f / za::clamp(vr_flat_shove_anim_speed.value, 0.25f, 4.f);
    return {za::clamp(vr_flat_shove_wind_time.value, 0.f, 1.f) * k, za::clamp(vr_flat_shove_push_time.value, 0.02f, 0.5f) * k,
        za::clamp(vr_flat_shove_hold_time.value, 0.f, 0.5f) * k, za::clamp(vr_flat_shove_recover_time.value, 0.05f, 1.f) * k};
}

// The whole shove (seconds).
[[nodiscard]] float shoveTotal()
{
    const ShoveTimes w = shoveTimes();
    return w.wind + w.push + w.hold + w.back;
}

// Where `h` is in the shove: its windup (0 .. 1 .. 0), its push out (0 .. 1 .. 0), and its palm's turn (0 .. 1 .. 0).
struct ShovePhase
{
    float wind{0.f};
    float out{0.f};
    float palm{0.f};
};

[[nodiscard]] ShovePhase shovePhase(int h)
{
    const double t = vr_gametime - fs.shoveAt;
    const float total = shoveTotal();
    if(!inShove(h) || t < 0.0 || t > total)
    {
        return {};
    }
    const ShoveTimes w = shoveTimes();
    if(t < w.wind)
    {
        const float u = slide(t, 0.0, w.wind);
        return {u, 0.f, u};
    }
    if(t < w.wind + w.push)
    {
        const float u = slide(t, w.wind, w.push);
        return {1.f - u, u, 1.f};
    }
    if(t < w.wind + w.push + w.hold)
    {
        return {0.f, 1.f, 1.f};
    }
    const float u = 1.f - slide(t, w.wind + w.push + w.hold, w.back);
    return {0.f, u, u};
}

// Whether `h`'s physics body lets ragdolls and corpses through (vr_flat_shove_pass_bodies): while its hands go back
// after the shove, and shovePassGrace after (the body it pushed may still lie in the hand at rest). Coming back, the
// hands met the body they had just shoved and dragged it back with them (his notes 2026-11-10). Not before: out at full
// push they still push it on (measured: passing from the landing on, a grunt shoved from 16 units ended 5 units nearer).
constexpr float shovePassGrace = 0.15f;
[[nodiscard]] bool shovePassesBodies(int h)
{
    const ShoveTimes w = shoveTimes();
    const double t = vr_gametime - fs.shoveAt;
    return vr_flat_shove_pass_bodies.value != 0.f && inShove(h) && fs.shoveSent && t >= w.wind + w.push + w.hold &&
           t <= shoveTotal() + shovePassGrace;
}

[[nodiscard]] bool shovingNow(int h)
{
    const ShovePhase p = shovePhase(h);
    return p.wind > 0.f || p.out > 0.f || (inShove(h) && !fs.shoveSent);
}

// The shove's hand offset in the view's frame (forward, right, up): pushed out ahead and in towards the middle.
// `toTarget`: from where the hand rests to what the crosshair is on, in the same frame: the push goes there by
// vr_flat_shove_aim (his notes 2026-11-08). Both hands pushing: each at its own side of it, vr_flat_shove_both_apart
// apart. Then the hand's own nudge at full push (vr_flat_shove_<main|off>_fwd / _out / _up: forward, out at its own
// side, up; units), by how far out it is.
// The windup before it: drawn back and up (vr_flat_shove_wind_back, _up). The target is vr_flat_shove_aim_up over the
// crosshair's point (his notes 2026-11-10: a bit higher, like the fist's).
[[nodiscard]] glm::vec3 shoveOffset(int h, const glm::vec3& toTargetAsked)
{
    const ShovePhase phase = shovePhase(h);
    const glm::vec3 wound = phase.wind * glm::vec3{-za::clamp(vr_flat_shove_wind_back.value, 0.f, 24.f), 0.f,
                                             za::clamp(vr_flat_shove_wind_up.value, -12.f, 12.f)};
    const glm::vec3 toTarget = toTargetAsked + glm::vec3{0.f, 0.f, za::clamp(vr_flat_shove_aim_up.value, -24.f, 24.f)};
    const float share = phase.out;
    const float out = share * za::clamp(vr_flat_shove_anim_reach.value, 0.f, 96.f);
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const glm::vec3 plain{out, -side * 0.35f * out, 0.15f * out};
    const bool main = h == HAND_MAIN;
    const glm::vec3 nudge = share * glm::vec3{poseNum(main ? vr_flat_shove_main_fwd : vr_flat_shove_off_fwd),
                                        side * poseNum(main ? vr_flat_shove_main_out : vr_flat_shove_off_out),
                                        poseNum(main ? vr_flat_shove_main_up : vr_flat_shove_off_up)};
    const float aimShare = za::clamp(vr_flat_shove_aim.value, 0.f, 1.f);
    const glm::vec3 aimAt = toTarget + glm::vec3{0.f, fs.shoveBoth ? side * 0.5f * za::clamp(vr_flat_shove_both_apart.value, 0.f, 24.f) : 0.f, 0.f};
    if(out <= 0.f || aimShare <= 0.f || glm::length(aimAt) < 1.f)
    {
        return plain + nudge + wound;
    }
    const glm::vec3 aimed = glm::normalize(aimAt) * za::min(glm::length(plain), glm::length(aimAt));
    return plain + (aimed - plain) * aimShare + nudge + wound;
}

// A shove (the shove key, vr_flat_shove_now): an open hand pushed out in front of him. The game's one-hand shove lands
// as the hand reaches out (QC VR_FlatShove, impulse 53 the off hand, 54 the main); the push itself is only drawn, and is
// synthesized motion (vr_handsource.hpp): the melee rules do not read it as a slap or a shove of their own. With an
// empty hand, the off hand's first; with none, the off hand (what it holds pushes with it); with both empty, both (56).
// A weapon in each hand: both, the weapons kept in them (vr_flat_shove_both_armed; his notes 2026-11-10: as the armed
// humanoids shove), and every shove both with vr_flat_shove_both_always. A free hand on a grip lets go of it for the
// shove (flatplay::shoving, as for a punch: vr_flatsight.cpp, vr_twohand.cpp).
void startShove(const char* from)
{
    if(vr_flat_shove.value == 0.f)
    {
        return;
    }
    if(vr_gametime < fs.shoveReady)
    {
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: shove (%s): cooling down, %.2f s left\n", from, fs.shoveReady - vr_gametime);
        }
        return;
    }
    const int h = heldIn(HAND_OFF) < 0 || heldIn(HAND_MAIN) >= 0 ? HAND_OFF : HAND_MAIN;
    fs.shoveHand = h;
    // Both hands free: both push (his notes 2026-11-08), the headset's two-handed shove (vr_flat_shove_both 0: the one).
    const bool bothFree = heldIn(HAND_OFF) < 0 && heldIn(HAND_MAIN) < 0;
    const bool bothArmed = heldIn(HAND_OFF) >= 0 && heldIn(HAND_MAIN) >= 0;
    fs.shoveBoth = vr_flat_shove_both_always.value != 0.f ||
                   (vr_flat_shove_both.value != 0.f && (bothFree || (bothArmed && vr_flat_shove_both_armed.value != 0.f)));
    fs.shoveAt = vr_gametime;
    fs.shoveSent = false;
    fs.shoveReady = vr_gametime + za::fmax(vr_flat_shove_cooldown.value, shoveTotal());
    fs.shoves++;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: shove (%s): %s (holding: off %d, main %d)\n", from,
            fs.shoveBoth ? "both hands" : h == HAND_MAIN ? "main hand" : "off hand", heldWid(HAND_OFF), heldWid(HAND_MAIN));
    }
}

// Each frame: the shove told to the game as its hand reaches out, at the push's end (QC: impulse 53 the off hand, 54 the
// main, 56 both).
void shoveFrame()
{
    const ShoveTimes w = shoveTimes();
    if(fs.shoveSent || vr_gametime - fs.shoveAt < w.wind + w.push)
    {
        return;
    }
    fs.shoveSent = true;
    Cbuf_InsertText(fs.shoveBoth ? "impulse 56\n" : fs.shoveHand == HAND_MAIN ? "impulse 54\n" : "impulse 53\n");
}

// How a hand must be turned for the weapon it holds to shoot along `aim`, whatever its Shot Pitch and Yaw (the drawn
// gun and its muzzle follow the hand): the hand is aimed the other way by the same amount.
[[nodiscard]] glm::vec3 aimRotAtCrosshair(const glm::vec3& aim, int h)
{
    const int slot = heldIn(h);
    if(slot < 0)
    {
        return aim;
    }
    const glm::vec3 want = hands::forward(aim);
    glm::vec3 rot = aim;
    for(int i = 0; i < 3; i++)
    {
        glm::vec3 f, r, u;
        hands::angleVectors(weapons::shotAngles(rot, slot, h == HAND_OFF), f, r, u);
        const glm::vec3 axis = glm::cross(f, want);
        const float ang = glm::length(axis);
        if(ang < 1e-4f)
        {
            break;
        }
        // The hand turned by what its shot is off. Its own frame: the shot's is the hand's with Shot Pitch and Yaw on it,
        // and turning that one instead left every shot off by them.
        const glm::quat turn = glm::angleAxis(za::asin(za::min(ang, 1.f)), glm::normalize(axis));
        hands::angleVectors(rot, f, r, u);
        rot = hands::anglesFromVectors(glm::normalize(turn * f), glm::normalize(turn * u));
    }
    return rot;
}

// The keys, read from the keyboard state (Ctrl's bind is the one exception, named at the top of the file), and E and V
// only while the game has the keyboard (the console takes it). The hands' fire buttons come from the same source the
// game's fire uses (vr_client.cpp's attacking: its +attack / +offhandattack state, which is what the mouse buttons drive),
// so a bound key, the mouse and a `+attack` said on the console are the same press.
void pollKeys()
{
    const bool inGame = key_dest == key_game;
    // The force grab's keys (vr_flat_key_grab, mouse 4: the side button the shove leaves; vr_flat_key_grab_alt, none: R
    // reloads now, vr_flatreload.cpp).
    const auto down = [inGame](const cvar_t& c) {
        const int k = c.string[0] ? Key_StringToKeynum(c.string) : -1;
        return inGame && k > 0 && k < MAX_KEYS && keydown[k] != 0;
    };
    const bool grab = down(vr_flat_key_grab) || down(vr_flat_key_grab_alt);
    const bool v = inGame && keydown['v'] != 0;
    bool fire[2];
    fire[HAND_OFF] = client::attacking(HAND_OFF);
    fire[HAND_MAIN] = client::attacking(HAND_MAIN);

    if(grab && !fs.keyGrab)
    {
        fs.grabs++;
        // The hand turns its palm out first, then the grab is asked for with it (vr_flatfgrab.cpp: QC
        // vr_wpnforcegrab.qc, the force grab on what the view is pointed at).
        flatfgrab::press();
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: force grab\n", vr_flat_key_grab.string);
        }
    }
    if(v && !fs.keyV && !flatgrenade::busy(HAND_MAIN))
    {
        startPunch(HAND_MAIN, "V"); // as it has always been: V punches with the main hand
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        // The fire button punches only with a weapon a swing strikes with; a gun's trigger fires the gun as it does now.
        // (Not a hand busy with the quick grenade: its fire button arms and throws it, vr_flatgrenade.cpp.)
        if(fire[h] && !fs.keyFire[h] && punchesWith(h) && !flatgrenade::busy(h))
        {
            startPunch(h, h == HAND_MAIN ? "the main hand's fire button" : "the off hand's fire button");
        }
    }
    fs.keyGrab = grab;
    fs.keyV = v;
    fs.keyFire[HAND_OFF] = fire[HAND_OFF];
    fs.keyFire[HAND_MAIN] = fire[HAND_MAIN];
}

// Left Ctrl crouches while it is held and stands up when it is let go, F switches the flashlight, H drops what a hand
// holds. Read from the keyboard state like E and V, and only while the game has the keys. (+flatcrouch / -flatcrouch hold
// the crouch the same way, for the console and the mode's own Ctrl bind: CtrlBind above.)
// Lying down's drop of the eyes (vr_prone): from Quake's standing eyes (cl.viewheight over the origin, 24 over the feet)
// to vr_flat_prone_eye over the feet (at the defaults 46 to 9.5: 36.5), under vr_prone_eye, where the server takes him
// for lying down (vr_hull.cpp updateCrouch) as it does a headset player whose eyes are that low.
[[nodiscard]] float proneDrop()
{
    // (vr_flat_prone_eye, under the server's vr_prone_eye by 2 at least; until 2026-11-08 vr_prone_eye less 4, 14. And
    // Quake's near plane, 4, under the lying box's top (vr_prone_height, 12 at least: vr_hull.cpp proneHeight), so the
    // view never reaches into the ceiling of a vent the box just passes: 12 at the default 16.)
    const float boxTop = za::max(12.f, vr_prone_height.value);
    const float eye = za::clamp(vr_flat_prone_eye.value, 6.f, za::max(6.f, za::min(vr_prone_eye.value - 2.f, boxTop - 4.f)));
    return za::clamp(cl.viewheight + 24.f - eye, 0.f, 44.f);
}

// Getting up from lying down (the prone key, the crouch pressed): only where the lowest crouched box fits (the server's
// own test for the rise, hull::localRiseFits). Under a lower ceiling (a 20-high vent) he stays lying, view, body and
// box, and gets up by himself once he has crawled out to where it fits (pollKeysFlat). Until 2026-11-09 the client let
// go of lying down at once and the server kept the lying box: the crouch held lifted his eyes 28 over his feet into the
// vent's ceiling (no cap while the crouch key holds him down), where the teleporter over the vent is.
void getUp(const char* why)
{
    if(!qvr::hull::localRiseFits())
    {
        keyState.riseWanted = true;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: no room to get up: still lying, up once it fits\n", why);
        }
        return;
    }
    keyState.prone = false;
    keyState.riseWanted = false;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: %s: get up\n", why);
    }
}

// vr_prone_toggle and the prone key: lie down, or get up again (getUp: only where a crouched box fits). Pressed again
// while a get-up waits for room, it takes the wish back: he stays lying.
void toggleProne(const char* why)
{
    if(vr_prone.value == 0.f || vr_crouch_hull.value == 0.f)
    {
        keyState.prone = false;
        keyState.riseWanted = false;
        Con_Printf("flatplay: lying down is off (vr_prone, vr_crouch_hull)\n");
        return;
    }
    if(keyState.prone && keyState.riseWanted)
    {
        keyState.riseWanted = false;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: stay lying\n", why);
        }
        return;
    }
    if(keyState.prone)
    {
        getUp(why);
        return;
    }
    // Not mid-slide (his bug of 2026-11-10: "it is currently possible to go prone during a slide"): the key does
    // nothing until the slide is over (the server won't lay a sliding body down either: vr_hull.cpp).
    if(flatmove::slidePose())
    {
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: no lying down mid-slide\n", why);
        }
        return;
    }
    keyState.prone = true;
    keyState.riseWanted = false;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: %s: lie down\n", why);
    }
}

void proneToggle_f()
{
    if(!active())
    {
        Con_Printf("vr_prone_toggle: flat-screen play only; in a headset, lie down for real (your eyes under vr_prone_eye)\n");
        return;
    }
    toggleProne("vr_prone_toggle");
}

// The toolgun in flatscreen play (vr_toolgun.cpp has the headset's buttons): F7 (vr_flat_key_toolgun) puts it in the
// main hand and back (impulse 49, QC weapons.qc VR_FlatToolgun_Toggle); with it held, the main hand's fire button (the
// left mouse button) is its trigger, mouse 3 (vr_flat_key_toolgun_menu) its B/Y: the panel, a screen overlay with the
// mouse's cursor while open (vr_toolgun_panel.cpp drawFlat, flatMouse), the left button clicking on it; and B
// (vr_flat_key_toolgun_adjust) held its X/A: the mouse the tool's (toolgun::flatNudge) instead of the view's.
struct FlatToolgunKeys
{
    bool key{false};    // vr_flat_key_toolgun, last frame
    bool menu{false};   // vr_flat_key_toolgun_menu, last frame
    bool fire{false};   // the main hand's fire, last frame, while the toolgun was held
    bool click{false};  // that press went to the panel (its release goes there too)
    bool adjust{false}; // vr_flat_key_toolgun_adjust held, last frame, while the toolgun was held
};
FlatToolgunKeys flatTg;

void toolgunKeys()
{
    const bool inGame = key_dest == key_game;
    const auto down = [inGame](const cvar_t& c) {
        const int k = c.string[0] ? Key_StringToKeynum(c.string) : -1;
        return inGame && k > 0 && k < MAX_KEYS && keydown[k] != 0;
    };
    const bool key = down(vr_flat_key_toolgun);
    if(key && !flatTg.key)
    {
        Cbuf_InsertText("impulse 49\n");
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: the toolgun\n", vr_flat_key_toolgun.string);
        }
    }
    flatTg.key = key;
    const bool held = toolgun::heldHand() == HAND_MAIN;
    const bool menu = down(vr_flat_key_toolgun_menu);
    if(menu && !flatTg.menu && held)
    {
        (void)toolgun::button(HAND_MAIN, posing::Button::Secondary, true);
        (void)toolgun::button(HAND_MAIN, posing::Button::Secondary, false);
    }
    flatTg.menu = menu;
    const bool fire = held && client::attacking(HAND_MAIN);
    if(fire != flatTg.fire)
    {
        if(fire)
        {
            // With the panel open a press is a click on it (where the cursor is), else the toolgun's trigger.
            flatTg.click = toolgun::panel::flatShown() && toolgun::panel::flatClick(true);
            if(!flatTg.click)
            {
                (void)toolgun::button(HAND_MAIN, posing::Button::Trigger, true);
            }
        }
        else
        {
            (void)(za::exchange(flatTg.click, false) ? toolgun::panel::flatClick(false)
                                                     : toolgun::button(HAND_MAIN, posing::Button::Trigger, false));
        }
    }
    flatTg.fire = fire;
    const bool adjust = held && down(vr_flat_key_toolgun_adjust);
    if(adjust != flatTg.adjust)
    {
        (void)toolgun::button(HAND_MAIN, posing::Button::Primary, adjust);
    }
    flatTg.adjust = adjust;
}

} // namespace

void swapHands(const char* what)
{
    keyState.swaps++;
    // The same InsertText as F and H: the swap travels as the impulse the game code reads (QC weapons.qc, 48 ->
    // vr_wpnholster.qc VR_FlatSwapHands), so `impulse 48` on the console swaps the same.
    // With vr_flat_swap_anim the hands meet first and the swap is sent as they do (vr_flatswap.cpp).
    if(!flatswap::press())
    {
        Cbuf_InsertText("impulse 48\n");
    }
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: %s: swap hands\n", what);
    }
}

// IN_MouseMotion: the toolgun panel's cursor while it is open on the screen, or the tool's adjust while B is held
// (flatTg.adjust): the mouse is theirs, the view stays.
extern "C" int VR_FlatToolgunMouse(int dx, int dy)
{
    if(!flatplay::active())
    {
        return 0;
    }
    if(toolgun::panel::flatMouse(dx, dy) || flatholster::mouse(dx, dy)) // (the holster menu's cursor: vr_flatholster.cpp)
    {
        return 1;
    }
    if(flatTg.adjust)
    {
        return toolgun::flatNudge(-static_cast<float>(dy) * vr_flat_toolgun_push_scale.value,
                   -static_cast<float>(dx) * vr_flat_toolgun_turn_scale.value)
                   ? 1
                   : 0;
    }
    return 0;
}

// Key_Event's Esc in the game: closes the toolgun panel open on the screen instead of opening the menu.
extern "C" int VR_FlatToolgunEscape(void)
{
    if(flatholster::escape()) // (the holster menu: closed until its key is pressed again)
    {
        return 1;
    }
    if(!toolgun::panel::flatShown())
    {
        return 0;
    }
    toolgun::panel::close();
    return 1;
}

namespace
{

void pollKeysFlat()
{
    const bool inGame = key_dest == key_game;
    const bool ctrl = inGame && keydown[K_CTRL] != 0;
    const bool flash = inGame && keydown['f'] != 0;
    // H (vr_flat_key_drop; G until 2026-11-10, his words: "Rebind drop to 'H' and free the 'G' key up": G is the quick
    // grenade now, vr_flatgrenade.cpp).
    const int dropKey = vr_flat_key_drop.string[0] ? Key_StringToKeynum(vr_flat_key_drop.string) : -1;
    const bool drop = inGame && dropKey > 0 && dropKey < MAX_KEYS && keydown[dropKey] != 0;

    if(ctrl && !keyState.keyCtrl)
    {
        keyState.crouches++;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: Left Ctrl: crouch\n");
        }
    }
    if(flash && !keyState.keyFlash)
    {
        keyState.toggles++;
        // InsertText, not AddText: the switch runs next. AddText would put it after the commands already queued, which
        // in a test run means after the script's waits (vr_mapinstall.cpp says the same). The console's own switch is
        // vr_flashlight_toggle (vr_flashlight.cpp), so a bind or the console does the very same thing.
        Cbuf_InsertText("vr_flashlight_toggle\n");
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: F: the flashlight\n");
        }
    }
    if(drop && !keyState.keyDrop)
    {
        keyState.drops++;
        // The same InsertText as the F above: the drop is asked for now, not after what is already queued. It travels as
        // the impulse the game code reads (QC weapons.qc ImpulseCommands, vr_wpnforcegrab.qc VR_Forcegrab_FlatDrop), so
        // `impulse 47` on the console drops the same thing.
        Cbuf_InsertText("impulse 47\n");
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: drop\n", vr_flat_key_drop.string);
        }
    }
    // Q and Z, each the key its cvar names (a bind name: Key_StringToKeynum), read like F and H.
    const int swapKey = Key_StringToKeynum(vr_flat_key_swap.string);
    const int timeKey = Key_StringToKeynum(vr_flat_key_bullettime.string);
    const bool swap = inGame && swapKey > 0 && swapKey < MAX_KEYS && keydown[swapKey] != 0;
    const bool slow = inGame && timeKey > 0 && timeKey < MAX_KEYS && keydown[timeKey] != 0;
    if(swap && !keyState.keySwap)
    {
        swapHands(vr_flat_key_swap.string);
    }
    if(slow && !keyState.keyTime)
    {
        keyState.times++;
        // Bullet time's own press (vr_bullettime.cpp keyPress): the rules of the gadget's tap and of the stick press --
        // the meter, the cooldown, the bounce guard -- with nothing of the flatscreen's own.
        bullettime::keyPress("flatscreen key");
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: %s: bullet time\n", vr_flat_key_bullettime.string);
        }
    }
    keyState.keyCtrl = ctrl;
    keyState.keyFlash = flash;
    keyState.keyDrop = drop;
    keyState.keySwap = swap;
    keyState.keyTime = slow;
    // X (vr_flat_key_prone): lie down, get up (vr_prone_toggle's toggle).
    const int proneKey = Key_StringToKeynum(vr_flat_key_prone.string);
    const bool proneDown = inGame && proneKey > 0 && proneKey < MAX_KEYS && keydown[proneKey] != 0;
    if(proneDown && !keyState.keyProne)
    {
        toggleProne("flatscreen key");
    }
    keyState.keyProne = proneDown;
    // The crouch pressed while lying down (Left Ctrl, +flatcrouch): up out of it into the crouch, the one easing out as the
    // other eases in (the eyes take the deeper of the two: crouchDrop), the gun's pose from the prone one to the crouch
    // one (targetOffset). It used to keep him lying down with the crouch's gun pose on top.
    const bool crouchNow = ctrl || keyState.cmdHeld;
    if(crouchNow && !keyState.crouchWas && keyState.prone)
    {
        getUp("crouch while lying down"); // (up to a crouch, where it fits)
    }
    keyState.crouchWas = crouchNow;
    // A get-up that waited for room: up as soon as the lowest crouched box fits (crawled out of the vent).
    if(keyState.prone && keyState.riseWanted && qvr::hull::localRiseFits())
    {
        getUp("room to get up");
    }
    // Mouse 5 (vr_flat_key_shove): a shove.
    const int shoveKey = Key_StringToKeynum(vr_flat_key_shove.string);
    const bool shove = inGame && shoveKey > 0 && shoveKey < MAX_KEYS && keydown[shoveKey] != 0;
    if(shove && !fs.keyShove)
    {
        startShove(vr_flat_key_shove.string);
    }
    fs.keyShove = shove;
    shoveFrame();
    toolgunKeys();
    flatholster::frame(true); // the wheel's button: the holster menu (vr_flatholster.cpp)

    // Down while held, back up when let go, each way in vr_flat_crouch_time seconds. Stepped once a frame, like the pose.
    // `lin` goes at an even speed; the share everything reads (`ratio`: the view, his head and box, the body, the arms,
    // the weapon) is it through crouchShape, which starts and ends gently (vr_flat_crouch_smooth 1).
    double dt = keyState.lastTime > -1e8 ? vr_gametime - keyState.lastTime : 0.0;
    keyState.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    keyState.lastDt = static_cast<float>(dt);
    const float want = (ctrl || keyState.cmdHeld) ? 1.f : 0.f;
    if(dt > 0.0)
    {
        const float step = static_cast<float>(dt) / za::clamp(vr_flat_crouch_time.value, 0.05f, 2.f);
        keyState.lin += za::clamp(want - keyState.lin, -step, step);
    }
    keyState.ratio = crouchShape(keyState.lin);
    // Lying down (vr_flat_key_prone): down while asked, up when not, each way in vr_prone_body_time; off (vr_prone 0, or
    // the crouched box off) it never goes down.
    // (Dead, or between maps: up again, so a respawn or the next map starts standing, as the server starts him.)
    if(!(vr_prone.value != 0.f && vr_crouch_hull.value != 0.f) || cls.signon != SIGNONS || cl.stats[STAT_HEALTH] <= 0)
    {
        keyState.prone = false;
        keyState.riseWanted = false;
    }
    if(dt > 0.0)
    {
        const float step = static_cast<float>(dt) / za::clamp(vr_prone_body_time.value, 0.1f, 3.f);
        keyState.proneLin += za::clamp((keyState.prone ? 1.f : 0.f) - keyState.proneLin, -step, step);
    }
    keyState.proneRatio = crouchShape(keyState.proneLin);
    // Let go under a low ceiling, he rises only as far as his box lets him: the server keeps his box crouched where the
    // standing one does not fit (the headset's checks: vr_hull.cpp updateCrouch), and his eyes stop flatEyeHeadroom under
    // its top (hull::localEyeCap) -- in a 40-high tunnel he stays down, in a 48-high gap he comes up to 36 over his feet.
    // His eyes standing are Quake's: cl.viewheight over his origin, 24 over his feet (Quake's player box: mins z -24).
    // Only a box the world keeps him in caps him: while the key holds him down the box is the lowest one over his own
    // lowered eyes, its cap always under them, and capping by it dragged the view down in steps of a box (8 units a
    // frame: the crouch looked instant). localEyeCap says nothing then (-1).
    const float cap = qvr::hull::localEyeCap();
    const float down = za::clamp(vr_flat_crouch_drop.value, 0.f, 40.f);
    if(cap >= 0.f && down > 0.f)
    {
        const float standEye = cl.viewheight + 24.f;
        const float least = za::clamp((standEye - cap) / down, 0.f, 1.f);
        if(keyState.ratio < least)
        {
            keyState.ratio = least;
            keyState.lin = crouchUnshape(least); // (held there: it rises from here, not from where the ease had got to)
        }
        // Kept lying down (the server keeps his box low under a ceiling the crouched one does not fit): the lying drop
        // holds his eyes under that box's top the same way.
        const float proneDown = proneDrop();
        const float need = standEye - cap;
        if(need > down && proneDown > 0.f && keyState.proneRatio * proneDown < need)
        {
            keyState.proneRatio = za::clamp(need / proneDown, 0.f, 1.f);
            keyState.proneLin = crouchUnshape(keyState.proneRatio);
        }
    }
}

// +flatcrouch / -flatcrouch: the crouch held and let go from the console (the same hold Left Ctrl gives).
void crouchHold_f()
{
    keyState.cmdHeld = true;
}

void crouchRelease_f()
{
    keyState.cmdHeld = false;
}

// Ctrl's bind, taken over and given back (CtrlBind above). Key_SetBinding is what the console's `bind` uses, so the mode's
// bind is an ordinary one: bindlist shows it, Key_WriteBindings would write it out (VR_FlatKeysForConfigWrite below stops
// that), and a `bind CTRL ...` said on the console during the mode takes it away until the mode asks for it again.
[[nodiscard]] bool ctrlHolds()
{
    return ctrlBind.taken && keybindings[K_CTRL] && !q_strcasecmp(keybindings[K_CTRL], "+flatcrouch");
}

void takeCtrlBind()
{
    if(ctrlHolds())
    {
        return; // still the mode's bind
    }
    // Remember what Ctrl holds now, every time -- not only the first time. The mode's frames start inside VR_BeginFrame
    // (Quake/host.c), before the frame's Cbuf_Execute runs the start-up configs, so at the very first frame Ctrl has no
    // bind at all yet; its own line (bind "CTRL" "+attack", Quake/default_cfg.h, the saved config) lands a moment later,
    // over the mode's, and the mode asks for the key again on the next frame. Each later ask happens for the same reason:
    // something else has just changed Ctrl's bind -- a config, a `bind CTRL ...` said on the console, an unbindall -- and
    // that value is the plain game's, so it is what the mode gives back before the engine writes its config out. Keeping
    // only the first one remembered would keep the empty state of that first frame and write it out as the player's bind.
    q_strlcpy(ctrlBind.was, keybindings[K_CTRL] ? keybindings[K_CTRL] : "", sizeof(ctrlBind.was));
    Key_SetBinding(K_CTRL, "+flatcrouch");
    ctrlBind.taken = true;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: CTRL `%s` -> +flatcrouch (vr_flat_crouch_fire 0: the crouch key does not fire the "
                   "weapon)\n", ctrlBind.was[0] ? ctrlBind.was : "unbound");
    }
}

void giveCtrlBindBack()
{
    if(!ctrlBind.taken)
    {
        return;
    }
    ctrlBind.taken = false;
    Key_SetBinding(K_CTRL, ctrlBind.was[0] ? ctrlBind.was : nullptr);
}

// Quake angles' axes as columns: forward, right, up.
[[nodiscard]] glm::mat3 axes(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::mat3{f, r, u};
}

// The drawn hands' looks, stepped once a frame (the game's clock: the same with vr_fixed_frames).
void stepFeel()
{
    const double t = vr_gametime;
    if(feel.seen && t == feel.lastTime)
    {
        return;
    }
    const glm::vec2 view{cl.viewangles[0], cl.viewangles[1]};
    const glm::vec3 vel{cl.velocity[0], cl.velocity[1], cl.velocity[2]};
    if(!feel.seen || t < feel.lastTime)
    {
        feel = Feel{};
        feel.seen = true;
        feel.lastTime = t;
        feel.view = view;
        feel.vel = vel;
        return;
    }
    const float dt = static_cast<float>(za::min(t - feel.lastTime, 0.1));
    feel.lastTime = t;
    feel.dt = dt;

    // The turn's lag: the hands keep their direction as the view turns, the spring brings them back.
    glm::vec2 turned{za::remainder(view.x - feel.view.x, 360.f), za::remainder(view.y - feel.view.y, 360.f)};
    feel.view = view;
    if(za::fabs(turned.x) > lagSnapDeg || za::fabs(turned.y) > lagSnapDeg)
    {
        turned = glm::vec2{0.f}; // set, not turned
        feel.lag = feel.lagRate = glm::vec2{0.f};
    }
    if(vr_flat_turnlag.value != 0.f) // (negative: drawnOffset shows it the other way, the hands leading the turn)
    {
        feel.lag -= turned;
        // Critically damped, stepped exactly: x(t) = (x0 + (v0 + w x0) t) e^-wt.
        const float w = lagStiffness * za::clamp(vr_flat_turnlag_speed.value, 0.1f, 5.f);
        const float e = za::exp(-w * dt);
        const glm::vec2 x0 = feel.lag, v0 = feel.lagRate;
        const glm::vec2 c = v0 + x0 * w;
        feel.lag = (x0 + c * dt) * e;
        feel.lagRate = (v0 - c * (w * dt)) * e;
    }
    else
    {
        feel.lag = feel.lagRate = glm::vec2{0.f};
    }

    // The step's sway: the change of velocity, in the view's frame (level), kicks the hands the other way.
    const glm::vec3 dv = t < feel.noKickUntil ? glm::vec3{0.f} : vel - feel.vel; // (a gate turned it: VR_FlatPortalAngle)
    feel.vel = vel;
    const float sway = za::clamp(vr_flat_sway.value, 0.f, 3.f);
    if(sway > 0.f)
    {
        const glm::mat3 level = axes(glm::vec3{0.f, view.y, 0.f});
        const glm::vec3 dvLocal{glm::dot(dv, level[0]), glm::dot(dv, level[1]), dv.z};
        feel.swayRate -= glm::vec3{dvLocal.x * swayKick, dvLocal.y * swayKick, dvLocal.z * swayKickUp} * sway;
        // The spring, in small steps (semi-implicit: stable at any frame rate).
        const int steps = za::max(1, static_cast<int>(dt * 240.f + 0.5f));
        const float h = dt / static_cast<float>(steps);
        for(int i = 0; i < steps; i++)
        {
            const glm::vec3 acc = -feel.sway * (swayStiffness * swayStiffness) - feel.swayRate * (2.f * swayDamping * swayStiffness);
            feel.swayRate += acc * h;
            feel.sway += feel.swayRate * h;
        }
        const float cap = maxSway * sway;
        if(const float len = glm::length(feel.sway); len > 1e-4f)
        {
            feel.sway *= softCap(len, cap) / len;
        }
    }
    else
    {
        feel.sway = feel.swayRate = glm::vec3{0.f};
    }

    // The walk's bob: its size eased with the speed on the ground, its cycle with the speed.
    const float speed = glm::length(glm::vec2{vel.x, vel.y});
    // (Sliding: no walk, no bob, vr_flat_slide_bob of it: slideBobScale.)
    const float ampTarget =
        cl.onground && !noclip_anglehack ? za::clamp(speed / runSpeed, 0.f, 1.3f) * slideBobScale() : 0.f;
    feel.amp += (ampTarget - feel.amp) * ease(bobEase, dt);
    if(feel.amp > 1e-3f)
    {
        feel.phase = za::remainder(feel.phase + dt * 2.f * glm::pi<float>() * bobRate * za::clamp(speed / runSpeed, 0.35f, 1.3f),
            4.f * glm::pi<float>());
    }
    feel.breath = za::remainder(feel.breath + dt * breathRate, 4.f * glm::pi<float>());
}

// The walk's forward and back bob (vr_flat_bob_fwd, his words 2026-11-06): along the view's own forward (`camAngles`, its
// pitch included), in step with the walk, forward at each step's dip (the up and down bob's lowest); sighted,
// vr_flat_ads_bob_fwd of it. Separate from vr_flat_bob. The body, the arms and the gun ride it together (bodyRide,
// drawnOffset). World units.
[[nodiscard]] glm::vec3 fwdBob(const glm::vec3& camAngles)
{
    const float amount = za::clamp(vr_flat_bob_fwd.value, 0.f, bobFwdMax);
    if(amount <= 0.f)
    {
        return glm::vec3{0.f};
    }
    const float ads = za::clamp(flatsight::adsShare(), 0.f, 1.f);
    const float share = 1.f + (za::clamp(vr_flat_ads_bob_fwd.value, 0.f, 1.f) - 1.f) * ads;
    return axes(camAngles)[0] * (amount * bobFwd * share * feel.amp * -za::cos(2.f * feel.phase));
}

} // namespace

int heldIn(int h)
{
    const int slot = weapons::heldSlot(h);
    return slot >= 0 && slot != weapons::fistSlot() ? slot : -1;
}

int loneWeaponHand()
{
    const bool off = heldIn(HAND_OFF) >= 0;
    const bool main = heldIn(HAND_MAIN) >= 0;
    return off == main ? -1 : main ? HAND_MAIN : HAND_OFF;
}

namespace
{

// The mouse's buttons by what the hands hold (routeFire, before a wall stops a raised gun).
[[nodiscard]] FireRoute routeButtons(bool lmb, bool rmb)
{
    FireRoute r;
    // A hand holding the quick grenade (vr_flatgrenade.cpp): each button its own hand's, LMB the main and RMB the off hand,
    // so the grenade has its button whatever the other hand holds (and RMB no ironsight while it is held).
    if(flatgrenade::heldHand() >= 0)
    {
        r.fire[HAND_MAIN] = lmb;
        r.fire[HAND_OFF] = rmb;
        return r;
    }
    const int lone = loneWeaponHand();
    if(lone >= 0)
    {
        r.fire[lone] = lmb;
        r.rmbAims = true;
        return r;
    }
    r.fire[HAND_MAIN] = lmb;
    r.fire[HAND_OFF] = rmb;
    return r;
}

} // namespace

FireRoute routeFire(bool lmb, bool rmb)
{
    // A gun turned up out of a wall does not fire (vr_flat_wall_block_fire): the button does nothing till it comes down.
    // A melee weapon swings as ever (a blow is never stopped at a wall).
    // The holster menu open: the left button is its click (vr_flatholster.cpp), nothing fires and nothing aims.
    if(flatholster::open())
    {
        return FireRoute{};
    }
    FireRoute r = routeButtons(lmb, rmb);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        // (Nor a gun on its way to the hip and back to reload: vr_flatreload.cpp; the game stops it too.)
        if(r.fire[h] && !punchesWith(h) && (flatsight::wallBlocksFire(h) || flatreload::busy(h)))
        {
            r.fire[h] = false;
        }
        // (Nor while the hands swap what they hold: vr_flatswap.cpp.)
        if(flatswap::busy())
        {
            r.fire[h] = false;
        }
    }
    return r;
}

float poseNum(const Tunable& c)
{
    return za::clamp(c.value, -30.f, 30.f);
}

bool meleeIn(int h)
{
    switch(heldWid(h))
    {
    case WID_AXE:
    case WID_MJOLNIR:
    case WID_SWORD:
    case WID_CHAINSAW:
    case WID_CROWBAR:
    case WID_SUPERAXE: return true;
    default: return false;
    }
}

bool punching(int h)
{
    const double t = vr_gametime - fs.punchAt[h];
    return t >= 0.0 && t <= punchLen(h);
}

bool shoving(int h)
{
    return h >= 0 && h < HAND_COUNT && shovingNow(h);
}

float fistShare(int h)
{
    if(h < 0 || h >= HAND_COUNT || heldIn(h) >= 0)
    {
        return 0.f;
    }
    // (Opened by E's grab as its palm turns out, and by a prop held up in it: flatfgrab.)
    const float open = za::max(flatfgrab::openShare(h), flatfgrab::carryShare(h));
    return (punching(h) ? 1.f : za::clamp(fs.guard[h] * fs.hang[h], 0.f, 1.f)) * (1.f - open);
}

glm::vec3 aimRotAt(const glm::vec3& angles, int h)
{
    return aimRotAtCrosshair(angles, h);
}

bool active()
{
    // The flag, and no headset session: asked where it is needed rather than remembered, so that a system asked before
    // the first frame (a fresh start's flashlight) gives the same answer as one asked later.
    return fs.flag && !vrActive();
}

bool flagGiven()
{
    return fs.flag;
}

void noteBackendImplied()
{
    // One line, named by the flag that implied it (the same shape as the engine's COM_CheckParmImplied).
    if(fs.backendNoted)
    {
        return;
    }
    fs.backendNoted = true;
    Con_Printf("Command line: -flatscreenmode implies no VR backend: none is started (flatscreen play runs only while "
               "no headset session is up; start the game without the flag to play in a headset)\n");
}

// How far the crouch this player holds (Left Ctrl, +flatcrouch) puts his head down, in units: 0 standing, and 0 outside
// the mode (in a headset nothing here runs and the player crouches with his own tracked head). The flat branch of
// vr_hands.cpp lowers his head and the body's crouch with it; the crouched boxes (vr_hull.cpp) and the QC's .vr_stand
// (vr_server.cpp) follow that lowered head.
float crouchDrop()
{
    // Lying down (vr_flat_key_prone) goes lower than the crouch: the deeper of the two.
    // A slide (vr_flatmove.cpp) takes him vr_flat_slide_drop lower than the crouch.
    return active() ? za::max(keyState.ratio * za::clamp(vr_flat_crouch_drop.value, 0.f, 40.f), keyState.proneRatio * proneDrop()) +
                          flatmove::slideDrop()
                    : 0.f;
}

float proneRatio()
{
    return active() ? keyState.proneRatio : 0.f;
}

glm::vec3 lastViewOrigin()
{
    return traceViewOrg;
}

glm::vec3 lastPoseHead()
{
    return traceHead;
}

glm::vec3 lastMainHand()
{
    return traceHand;
}

void traceBody(bool on)
{
    traceBodyOn = on;
}

glm::vec3 lastMainShoulder()
{
    return traceShoulder;
}

glm::vec3 lastPelvis()
{
    return tracePelvis;
}

glm::vec3 lastDrawnMainHand()
{
    return feel.drawnAt[HAND_MAIN];
}

int armHolds()
{
    return traceArmHolds;
}

// Its eased share: 0 standing .. 1 fully crouched (vr_flatplay_status, the tests).
float crouchRatio()
{
    return active() ? keyState.ratio : 0.f;
}

bool crouchHeld()
{
    // (Lying down asked for holds it too: the server's box follows his eyes down, and he lies once they are low.)
    return active() && (keyState.keyCtrl || keyState.cmdHeld || keyState.prone);
}

// The flatscreen look's window, in the plain game's pitch -- Quake's pitch is positive looking down, so looking up is
// negative: the view sits between -vr_flat_pitch_up and +vr_flat_pitch_down. It is asked at the one place where the
// frame's looking has all landed in cl.viewangles (Quake/cl_main.c CL_AccumulateCmd: the look keys, the arrow keys and the
// mouse wheel in CL_AdjustAngles just above it, the mouse, the stick and the gyro in IN_Move just above it too), so one
// clamp covers every way of looking, and it lands before the frame's move is sent and before the view is built. Outside
// the mode the angle comes back as it was: plain play and a headset keep cl_maxpitch / cl_minpitch, which is their own
// setting. The window only narrows what those already let through this frame -- they each clamped first -- so a player
// who set cl_maxpitch to 40 still looks only 40 degrees down in flatscreen play.
[[nodiscard]] float pitchClamp(float pitch)
{
    if(!active())
    {
        return pitch;
    }
    const float up = za::clamp(vr_flat_pitch_up.value, 0.f, 90.f);
    const float down = za::clamp(vr_flat_pitch_down.value, 0.f, 90.f);
    return za::clamp(pitch, -up, down);
}

// Quake/host.c is about to write the key bindings out: Ctrl's bind given back, so the config on disk keeps the plain
// game's CTRL (+attack, unless the player had it bound to something else). The mode's next frame takes it up again; at a
// quit there is no next frame, and that is the point -- a flatscreen session leaves no +flatcrouch in the config.
void keysForConfigWrite()
{
    giveCtrlBindBack();
}

void init()
{
    fs.flag = COM_CheckParm("-flatscreenmode") != 0 || ragtool::flagGiven(); // (-ragtool: the ragdoll tool, flat)
    Cmd_AddCommand("vr_flatplay_status", status_f);
    flatmove::init(); // the slide and the lean (vr_flatmove.cpp)
    Cmd_AddCommand("+flatcrouch", crouchHold_f); // Left Ctrl's hold, the console's way (a bind or a script can hold it)
    Cmd_AddCommand("-flatcrouch", crouchRelease_f);
    Cmd_AddCommand("vr_prone_toggle", proneToggle_f); // the prone key's toggle (vr_flat_key_prone), the console's way
    Cmd_AddCommand("vr_flat_punch", punch_f);
    Cmd_AddCommand("vr_flat_shove_now", shoveNow_f); // the shove key's shove, the console's way (tests)
    Cmd_AddCommand("vr_flat_melee_probe", meleeProbe_f); // each punch's, swing's and shove's numbers (flatmelee_test.sh)
    Cmd_AddCommand("vr_flat_flick_test", flickTest_f); // the tests' mouse, flicked hard (no gesture may come of it)
    Cmd_AddCommand("vr_flat_orbit", orbit_f);
    flycam::init(); // vr_flat_flycam_toggle, vr_flat_flycam_test (vr_flycam.cpp)
    flatsweep::init(); // vr_flat_sweep (vr_flatsweep.cpp)
    flathud::init();   // vr_flathud_report (vr_flathud.cpp)
    flatreload::init(); // vr_flatreload_report (vr_flatreload.cpp)
    flatholster::init(); // +flatholsters, vr_flat_holster_menu_dump, vr_flatholster_test (vr_flatholster.cpp)
    flatfgrab::init();  // vr_flat_fg_report, vr_flat_fg_press (vr_flatfgrab.cpp)
    flatgrenade::init(); // vr_flatgrenade_report (vr_flatgrenade.cpp)
    flatswap::init(); // vr_flatswap_report (vr_flatswap.cpp)
    flathit::init();   // vr_flat_hit_probe, vr_flat_hit_test (vr_flathit.cpp)
    flatxhair::init(); // vr_flat_xhair_probe (vr_flatxhair.cpp)
    flatspeedfov::init(); // vr_flat_speed_fov_test, _status (vr_flatspeedfov.cpp)
    flataimsens::init(); // vr_flat_aim_sens_test, _status (vr_flataimsens.cpp)
    flatleanpose::init(); // vr_flatleanpose_test (vr_flatleanpose.cpp)
    flatsight::init(); // vr_flat_aim, the ironsight (vr_flatsight.cpp): registered either way, so a headset run can be
                       // told it is flatscreen play only
}

void frame()
{
    const bool on = active();
    Cvar_SetValue("vr_flatplay", on ? 1.f : 0.f);
    flycam::frame(on);  // the fly camera (vr_flat_flycam; vr_flycam.cpp): its key, the flight. While it flies nothing of
                        // his is read or stepped below -- no keys, no ironsight, no hit's punch into the aim, no lean:
                        // he stands exactly as he was
    const bool frozen = on && flycam::on();
    flatsweep::frame(); // vr_flat_sweep's next step, when one runs: before the pose reads the look
    flickTestFrame();   // vr_flat_flick_test's next flick, when one runs: likewise
    flataimsens::frame(); // vr_flat_aim_sens_test's next step, when one runs
    if(!frozen)
    {
        flatsight::frame(); // the ironsight's key edge, and the eases; it resets itself when the mode is off
        flatspeedfov::frame(on); // the speed FOV's ease (vr_flat_speed_fov); gone at once when the mode is off
        flathit::frame();   // a hit's punch of the view: this frame's part of it into the aim, before the pose reads the look
    }

    if(on && !fs.announced)
    {
        fs.announced = true;
        Con_Printf("Quake VR: flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen. E grabs what you "
                   "look at, 1 and 2 cycle the weapons collected (and the empty hand) into the left and right hand, V punches, either "
                   "hand's fire button punches with a weapon a swing strikes with (with a gun it fires the gun, as in VR), Left Ctrl "
                   "crouches while you hold it and no longer fires (vr_flat_crouch_fire 1 gives Ctrl its firing bind back), F "
                   "switches the flashlight, H drops what a hand holds, G takes a grenade, 3 swaps the hands, Z starts bullet time, R reloads. With one weapon "
                   "held the left mouse button fires it and the right one aims down its sights. vr_flatplay_status says what "
                   "the mode is doing.\n");
    }
    if(!on && fs.wasOn)
    {
        Con_Warning("Quake VR: a headset took the game back: flatscreen play is off.\n");
    }
    fs.wasOn = on;

    // Left Ctrl, the crouch key that is also the plain game's fire key: while the mode runs it is the mode's bind
    // (+flatcrouch) unless vr_flat_crouch_fire asks Ctrl to keep firing. Given back the frame the mode is not running.
    if(on && vr_flat_crouch_fire.value == 0.f)
    {
        takeCtrlBind();
    }
    else
    {
        giveCtrlBindBack();
    }

    if(!on)
    {
        fs.localSeen[0] = fs.localSeen[1] = false;
        fs.mountSeen[0] = fs.mountSeen[1] = false;
        fs.probeTime[0] = fs.probeTime[1] = -1e9;
        fs.lastTime = -1e9;
        feel = Feel{};
        keyState = FlatKeys{}; // no crouch held, no key edges: a headset player crouches with his own head
        flatmove::frame(false);
        slidesound::frame(false);
        flatreload::frame(false);
        flatholster::frame(false);
        flatfgrab::frame(false);
        flatgrenade::frame(false);
        flatswap::frame(false);
        return;
    }

    if(frozen)
    {
        keyState.lastTime = vr_gametime; // (the crouch's ease held where it was, not stepped a long frame on landing)
        return;
    }
    pollKeys();
    pollKeysFlat();
    flatreload::frame(true); // R, and each hand's trip to the hip as the server says (vr_flatreload.cpp)
    flatfgrab::frame(true);  // E's grab: each hand's palm turned out, the grab sent, held while it flies (vr_flatfgrab.cpp)
    flatgrenade::frame(true); // G, and the grenade's hand as the server says (vr_flatgrenade.cpp)
    flatswap::frame(true); // the hands' swap, sent as they meet (vr_flatswap.cpp)
    flatmove::frame(true); // the lean (Q / E) and the slide's pose (vr_flatmove.cpp)
    flatleanpose::frame(); // vr_flatleanpose_test's next step, when one runs
    slidesound::frame(true); // its scrape (vr_slidesound.cpp)
    flatxhair::frame(); // vr_flat_xhair_turntest's mouse (vr_flatxhair.cpp)
}

namespace
{

// The body's and the view's share in a blow (his notes 2026-11-10): a punch (an empty hand's, or with a prop) tips the
// view down into it and rolls it toward its side, turns the chest its shoulder forward and leans it in, all of it at the
// reach and gone as the arm draws back (vr_flat_punch_cam_*, _torso_*); a weapon's overhead swing tips the view and the
// chest back with the windup and forward with the strike (vr_flat_swing_cam_*, _torso_*). Degrees: `pitch` the view down,
// `roll` the view's roll, `twist` the chest turned left (the main hand's shoulder forward), `chest` the chest forward.
// The view's only: the aim, the shots and the hands' pose stay where they were (VR_FlatViewLean adds it to the drawn view).
struct MeleeLean
{
    float pitch{0.f};
    float roll{0.f};
    float twist{0.f};
    float chest{0.f};
};

[[nodiscard]] MeleeLean meleeLean()
{
    MeleeLean m;
    if(!active())
    {
        return m;
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(!punching(h))
        {
            continue;
        }
        const double t = vr_gametime - fs.punchAt[h];
        const float side = h == HAND_OFF ? -1.f : 1.f;
        if(fs.punchOverhead[h])
        {
            // -1 at the windup's top, held, through 0 to 1 at the strike's end, held, back to 0.
            const SwingTimes w = swingTimes();
            const double a = w.wind, b = a + w.top, c = b + w.strike, d = c + w.dwell;
            const float u = t < a   ? -slide(t, 0.0, w.wind)
                            : t < b ? -1.f
                            : t < c ? -1.f + 2.f * slide(t, b, w.strike)
                            : t < d ? 1.f
                                    : 1.f - slide(t, d, w.back);
            m.pitch += u * za::clamp(u < 0.f ? vr_flat_swing_cam_up.value : vr_flat_swing_cam_down.value, -6.f, 6.f);
            m.chest += u * za::clamp(u < 0.f ? vr_flat_swing_torso_up.value : vr_flat_swing_torso_down.value, -15.f, 15.f);
            continue;
        }
        const double reach = punchWind + punchStrike;
        const float e = t < reach                ? slide(t, 0.0, static_cast<float>(reach))
                        : t < reach + punchDwell ? 1.f
                                                 : 1.f - slide(t, reach + punchDwell, punchBack);
        m.pitch += e * za::clamp(vr_flat_punch_cam_pitch.value, -6.f, 6.f);
        m.roll += e * side * za::clamp(vr_flat_punch_cam_roll.value, -6.f, 6.f);
        m.twist += e * side * za::clamp(vr_flat_punch_torso_twist.value, -30.f, 30.f);
        m.chest += e * za::clamp(vr_flat_punch_torso_lean.value, -15.f, 15.f);
    }
    return m;
}

} // namespace

glm::vec3 meleeViewTilt()
{
    const MeleeLean m = meleeLean();
    return {m.pitch, 0.f, m.roll};
}

float trunkTwist()
{
    return active() && bodyPose() ? slideTurn.twist + meleeLean().twist : 0.f;
}

float proneNeckDrop()
{
    return active() && bodyPose() ? za::clamp(vr_flat_prone_neck_drop.value, 0.f, 10.f) : 0.f;
}

float chestPitch(float pitch)
{
    if(!bodyPose())
    {
        return 0.f;
    }
    return pitch * za::clamp(pitch > 0.f ? vr_flat_chest_down.value : vr_flat_chest_up.value, 0.f, 1.f) + meleeLean().chest;
}

glm::vec3 crouchNeck(const glm::vec3& levelFwd, float pitch)
{
    if(!active() || !bodyPose())
    {
        return glm::vec3{0.f};
    }
    // (Forward fades with a look down: under that look the neck would come into the view.)
    const float down = za::clamp(pitch / lookMost, 0.f, 1.f);
    return levelFwd * (za::clamp(vr_flat_crouch_shoulder_fwd.value, -poseTweakMax, poseTweakMax) * (1.f - down)) -
           glm::vec3{0.f, 0.f, za::clamp(vr_flat_crouch_shoulder_down.value, -poseTweakMax, poseTweakMax)};
}

bool onBody()
{
    return active() && bodyPose();
}

float gunOut(float pitch)
{
    return gunOutAt(pitch);
}

glm::vec3 bodyRide(const hands::State& s)
{
    if(!active() || !bodyPose() || !s.valid || !viewIsHisOwn(s))
    {
        return glm::vec3{0.f};
    }
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const glm::vec3 cam = flycam::eyeOrg(); // (his own eye, not the fly camera's)
    return (cam - (s.head + camShiftWorld(aim))) * rideShare() + fwdBob(flycam::eyeAngles());
}

bool drawnSkeleton(const hands::State& s, avatar::Skeleton& out)
{
    return avatar::skeletonUnder(s.head + bodyRide(s), out);
}

glm::vec3 levelHang(const hands::State& s, int h)
{
    if(!active() || !bodyPose() || fs.hang[h] <= 0.f || !fs.shouldersKnown)
    {
        return glm::vec3{0.f};
    }
    const int held = 1 - h;
    const glm::vec3 aim{0.f, s.headAngles.y, 0.f};
    const glm::mat3 level = axes(aim);
    const float reach = za::clamp(vr_flat_reach.value, 0.25f, 3.f);
    const glm::vec3 restFree = targetOffset(h, reach, true, 0.f);
    // (Without the stance: the stance follows from the choice, so it can't move what chooses. It did: a shotgun's cup chosen, the stance
    // brought the gun back, the grip ahead was nearer, a real grip took the stance off, the gun went out, the cup was nearer.)
    const glm::vec3 restHeld = targetOffset(held, reach, false, 0.f, 0.f);
    const glm::vec3 apart = fs.shoulders[h] - fs.shoulders[held] + level * (restFree - restHeld);
    const glm::mat3 toGun = axes(s.rot[held]) * glm::transpose(level);
    return s.pos[held] + toGun * apart - s.pos[h];
}

namespace
{

// x, kept above `low` without a corner: x itself from low + width up, nearing low below it.
[[nodiscard]] float softAbove(float x, float low, float width)
{
    return x >= low + width ? x : low + width * za::exp((x - low - width) / width);
}

[[nodiscard]] float softBelow(float x, float high, float width)
{
    return -softAbove(-x, -high, width);
}

constexpr float stockMost = 6.f;     // vr_flat_clear_stock's limit
constexpr float stockEase = 1.5f;    // ... eased in over this much above it
constexpr float stockPushMost = 12.f; // the farthest forward it takes the gun, units

struct TorsoCaps
{
    glm::vec3 a[4], b[4];
    float r[4];
};

// The torso's four capsules, as vr_selfcollide's sceneFor and vr_flatsweep's bodyShapes make them (the chest's two, the pelvis's two).
[[nodiscard]] TorsoCaps torsoCaps(const hands::State& s)
{
    const avatar::BuildShape& build = avatar::buildShape(avatar::bodyBuild());
    const avatar::Torso t = avatar::torso(s);
    const float k = units::metresToUnits() * units::bodyScale();
    TorsoCaps out;
    int n = 0;
    for(const float side : {-1.f, 1.f})
    {
        const glm::vec3 cs = t.chest.rot[1] * (side * 0.058f * build.torso * build.capSide * k) - t.chest.rot[2] * (0.01f * k);
        out.a[n] = t.chest.pos + t.chest.rot[0] * (-0.06f * k) + cs;
        out.b[n] = t.chest.pos + t.chest.rot[0] * (0.09f * k) + cs;
        out.r[n++] = 0.125f * build.torso * k;
        const glm::vec3 ps = t.pelvis.rot[1] * (side * 0.055f * k) - t.pelvis.rot[2] * (0.01f * k);
        out.a[n] = t.pelvis.pos + ps;
        out.b[n] = t.pelvis.pos + t.pelvis.rot[0] * (0.12f * k) + ps;
        out.r[n++] = 0.11f * k;
    }
    return out;
}

// How far the segment from `rear` to `grip` stays outside the torso's capsules (negative: inside), sampled along it.
[[nodiscard]] float torsoClearance(const TorsoCaps& t, const glm::vec3& rear, const glm::vec3& grip)
{
    float least = 1e9f;
    constexpr int samples = 9;
    for(int i = 0; i < samples; i++)
    {
        const glm::vec3 p = glm::mix(rear, grip, static_cast<float>(i) / (samples - 1));
        for(int c = 0; c < 4; c++)
        {
            least = za::min(least, geom::segmentDistance(p, t.a[c], t.b[c]) - t.r[c]);
        }
    }
    return least;
}

// The hand on the body (vr_flat_body_pose): where it stands for this look, from its shoulder. `local` is its pose at a level look,
// off its shoulder (forward, right, up: targetOffset). The arm turns about the shoulder by its share of the look, then the limits,
// each eased in so no look shows a corner: its hotspot within the free arm (the stance's), clear of the chest, clear of the eye, on
// the screen, within its own arm.
// The lean's turn of the upper body (vr_flat_lean_arms, vr_flat_lean_arms_pivot; his words 2026-11-09: "as we apply the torso
// rotation, we rotate and move the arms at the same time as the torso so that everything gets rotated/tilted with the torso
// as a pivot. Then we ensure that the weapons are still aiming at the right spot"). The arms and the guns are posed as he
// stands upright, then moved with the eye as one rigid body: the move that takes the upright eye to the leant one
// (vr_flatmove.cpp leanShift), turning about the level forward by the upper body's tilt -- at vr_flat_lean_arms_pivot 0 the
// view's own lean roll (the guns keep their place on the screen while the room tilts), else the eye's sideways move's angle
// that far under it (at vr_flat_lean_pivot's 30, the eye's own arc: 32 degrees at a full lean, the guns 17 degrees round on
// the screen) -- vr_flat_lean_arms of it. The guns then aim again from where their hands are (the crosshair's point:
// aimRotAtCrosshair, flatsight's aimAtPoint) and roll vr_flat_lean_gun_roll of the view's lean roll. Before it
// (agent/lean_pose), the shoulders came off the body leant from its hips, the hands off them unturned, and the drawn hands
// were turned round the eye by the view's roll on top: on the screen the lower gun went 2 down and 4 in, the upper 4 up.
constexpr float armsPivotMost = 120.f; // vr_flat_lean_arms_pivot's most

struct LeanRig
{
    bool on{false};
    glm::vec3 shift{0.f}; // the eye's move (flatmove::leanShift)
    glm::vec3 eye{0.f};   // the leant head (the pose's s.head)
    glm::mat3 turn{1.f};

    // Where a point of the upright pose goes leant.
    [[nodiscard]] glm::vec3 operator()(const glm::vec3& p) const { return on ? eye + turn * (p - (eye - shift)) : p; }
};

[[nodiscard]] LeanRig leanRig(const hands::State& s, float yaw)
{
    LeanRig r;
    r.shift = flatmove::leanShift();
    if(r.shift == glm::vec3{0.f} && flatmove::leanRoll() == 0.f)
    {
        return r;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);
    const float pivot = za::clamp(vr_flat_lean_arms_pivot.value, 0.f, armsPivotMost);
    const float tilt = pivot >= 1.f ? za::asin(za::clamp(glm::dot(r.shift, right) / pivot, -1.f, 1.f)) : glm::radians(flatmove::leanRoll());
    const float angle = tilt * za::clamp(vr_flat_lean_arms.value, 0.f, 1.f);
    // (Round the level forward, + to the right: the top goes the way he leans.)
    r.turn = glm::mat3_cast(glm::angleAxis(angle, fwd));
    r.eye = s.head;
    r.on = true;
    return r;
}

// `roots`: the shoulders, as `s` (the upright pose while leaning: leanRig turns the result) has them.
[[nodiscard]] glm::vec3 handOnBody(const hands::State& s, const glm::vec3 (&roots)[2], int h, const glm::vec3& aim, const glm::vec3& local,
    const glm::vec3& eye, const glm::vec3& carry)
{
    glm::vec3 levelFwd, levelRight, levelUp;
    hands::angleVectors({0.f, aim.y, 0.f}, levelFwd, levelRight, levelUp);
    const float side = h == HAND_OFF ? -1.f : 1.f;
    const glm::vec3 root = roots[h];
    const float span = armSpan();

    const float turn = glm::radians(-aim.x * za::clamp(aim.x > 0.f ? vr_flat_arm_down.value : vr_flat_arm_up.value, 0.f, 1.5f));
    const float c = za::cos(turn), sn = za::sin(turn);
    const float lookOut = za::clamp(glm::abs(aim.x) / lookMost, 0.f, 1.f);
    const float armOut = za::clamp(aim.x > 0.f ? vr_flat_arm_out_down.value : vr_flat_arm_out_up.value, -poseTweakMax, poseTweakMax);
    glm::vec3 off{local.x * c - local.z * sn, local.y + side * armOut * lookOut * lookOut * (3.f - 2.f * lookOut), local.x * sn + local.z * c};
    glm::vec3 p = root + levelFwd * off.x + levelRight * off.y + levelUp * off.z;

    // The stance: the hotspot the free hand is drawn on stays in that arm's reach, the gun drawn back along the view, so it keeps its
    // side of the screen and doesn't come across his face.
    glm::vec3 spot;
    const float stance = flatsight::stanceSpot(h, spot);
    const auto keepSpot = [&](bool backAlong) {
        if(stance <= 0.f)
        {
            return;
        }
        glm::vec3 vf, vr, vu;
        hands::angleVectors(aim, vf, vr, vu);
        // (From its rest shoulder, as the free hand's own need is measured: flatsight's armShare.)
        const glm::vec3 otherRoot = roots[1 - h];
        const float keep = span * za::clamp(vr_flat_keep_grip.value, 0.3f, 1.5f);
        // Across the view first, as far as keeps the most of the way back along it (the gun out at its side by the crouch, say)...
        glm::vec3 to = p + axes(aim) * spot - otherRoot;
        const glm::vec3 acrossWay = to - vf * glm::dot(to, vf);
        const float across = glm::length(acrossWay);
        if(across > 1e-3f)
        {
            p -= acrossWay / across * (across - softBelow(across, keep * acrossKeep, keepEase * span)) * stance;
        }
        // ... then back along it (the first time only: again, after the chest's and the eye's limits, it brought the stock into the
        // chest on a look up).
        if(!backAlong)
        {
            return;
        }
        to = p + axes(aim) * spot - otherRoot;
        const float along = glm::dot(to, vf);
        const float across2 = glm::dot(to, to) - along * along;
        const float most = keep * keep > across2 ? za::sqrt(keep * keep - across2) : 0.f;
        p -= vf * (along - softBelow(along, most, keepEase * span)) * stance;
    };
    keepSpot(true);

    // Clear of the chest: ahead of the shoulder, while the hand is below it (a hand over the shoulder has no chest before it).
    const auto clearChest = [&]() {
        const glm::vec3 rel = p - root;
        const float ahead = glm::dot(rel, levelFwd);
        const float below = za::clamp(1.f - glm::dot(rel, levelUp) / clearFadeAbove, 0.f, 1.f);
        const float keep = za::clamp(vr_flat_clear_chest.value, 0.f, clearMost);
        if(below > 0.f)
        {
            p += levelFwd * (softAbove(ahead, keep * below, clearEase) - ahead);
        }
    };
    clearChest();

    // Clear of the eye, and of the neck under his head (a look up takes the eye back over it).
    {
        const float keep = za::clamp(vr_flat_clear_eye.value, 0.f, clearMost);
        const glm::vec3 v = p - eye;
        const float d = glm::length(v);
        if(d > 1e-3f)
        {
            p = eye + v / d * softAbove(d, keep, eyeEase);
        }
        const float m2w = units::metresToUnits() * units::bodyScale();
        const glm::vec3 neckTop = s.head + headLean(aim) - levelUp * (vr_body_eye_up.value * m2w);
        const float z = za::clamp(glm::dot(neckTop - p, levelUp), 0.f, neckLength * m2w);
        const glm::vec3 fromNeck = p - (neckTop - levelUp * z);
        const float dn = glm::length(fromNeck);
        if(dn > 1e-3f)
        {
            p += fromNeck / dn * (softAbove(dn, keep, eyeEase) - dn);
        }
    }

    // On the screen: forward enough along the view for how far it stands off the view's middle.
    {
        glm::vec3 vf, vr, vu;
        hands::angleVectors(aim, vf, vr, vu);
        const glm::vec3 v = p - eye;
        const float f = glm::dot(v, vf);
        const float across = glm::length(v - vf * f);
        const float flat = za::clamp(vr_flat_screen_angle.value, 20.f, 85.f);
        const float cheek = za::clamp(vr_flat_screen_angle_stance.value, 20.f, 85.f);
        const float angle = glm::radians(flat + (cheek - flat) * (flatsight::stancePistol() ? 0.f : flatsight::stanceShare(h)));
        const float low = across / glm::tan(angle);
        p += vf * (softAbove(f, low, screenEase) - f);
    }

    // The hotspot again: the chest, the eye and the screen push the gun forward and out, and crouched or looking straight down that
    // took the hotspot out of the free arm's reach (the hand let go). And the chest again after it (it brings the gun back).
    if(stance > 0.f)
    {
        keepSpot(false);
        clearChest();
    }

    // Within its own arm.
    {
        const glm::vec3 v = p - root;
        const float d = glm::length(v);
        if(d > 1e-3f)
        {
            p = root + v / d * softBelow(d, span * za::clamp(vr_flat_keep_arm.value, 0.3f, 1.f), keepEase * span);
        }
    }

    // The stock out of the torso: Gun Out by look brings a long gun in across the chest (his -4 looking up and down, -2 crouched), and
    // its rear went into the torso. The gun goes forward along the view as far as keeps its rear vr_flat_clear_stock out, so it keeps
    // its place across the screen (his look) and only comes a little forward: the stock rests against the chest.
    glm::vec3 stock;
    const float stockKeep = za::clamp(vr_flat_clear_stock.value, 0.f, stockMost);
    if(stockKeep > 0.f && heldIn(h) >= 0 && flatsight::stockOff(h, stock))
    {
        const glm::mat3 view = axes(aim);
        const glm::vec3 back = view * stock;
        const TorsoCaps caps = torsoCaps(s);
        const float before = torsoClearance(caps, p + carry + back, p + carry);
        const float want = softAbove(before, stockKeep, stockEase);
        if(want > before + 1e-4f)
        {
            float lo = 0.f, hi = stockPushMost;
            for(int i = 0; i < 12; i++)
            {
                const float mid = 0.5f * (lo + hi);
                const glm::vec3 q = p + carry + view[0] * mid;
                (torsoClearance(caps, q + back, q) >= want ? hi : lo) = mid;
            }
            p += view[0] * hi;
        }
    }
    return p;
}

} // namespace

float torsoGap(const hands::State& s, const glm::vec3& rear, const glm::vec3& hand)
{
    return active() && bodyPose() ? torsoClearance(torsoCaps(s), rear, hand) : 1e9f;
}

// An empty hand off walls and props (his notes 2026-11-08: the guard's fists went into them; a punch went through): back
// along its way to its shoulder (`root`) until its point, seen from the eye, is vr_flat_wall_margin and handRoom off them,
// as a held weapon goes back (flatsight's push back: handpose's traces, monsters not counted). A punch or a shove stops at
// the surface: only vr_flat_punch_wall_margin off it, at once, so the blow still meets the wall. Not a free hand beside
// the other's gun at rest (flatsight's freeOffWall and its grip). vr_flat_hand_collide 0, or Gun Against Walls off: none.
namespace
{
void handOffWall(hands::State& s, int h, const glm::vec3& root, float dt)
{
    const bool acting = punching(h) || shoving(h);
    const glm::vec3 way = root - s.pos[h];
    const float len = glm::length(way);
    const bool mine = vr_flat_hand_collide.value != 0.f && vr_flat_gun_collide.value != 0.f && heldIn(h) < 0 &&
                      (acting || heldIn(1 - h) < 0) && len > 0.01f;
    fs.wallDepth[h] = 0.f;
    if(!mine)
    {
        fs.wallBack[h] = 0.f;
        return;
    }
    // (A punch's margin may be negative: that far into the surface, its bite: stopped on it, the fist's blow line never
    // reached the wall and QC's wall rule had no blow to land.)
    const float punchMargin = za::clamp(vr_flat_punch_wall_margin.value, -handMarginMax, handMarginMax);
    const float margin = acting ? za::max(0.f, punchMargin) : za::clamp(vr_flat_wall_margin.value, 0.f, handMarginMax) + handRoom;
    const float bite = acting ? za::max(0.f, -punchMargin) : 0.f;
    const auto depthAt = [&](float t) { return handpose::pointWallDepth(s.head, s.pos[h] + way * t, margin); };
    float need = 0.f;
    fs.wallDepth[h] = depthAt(0.f);
    if(fs.wallDepth[h] > 0.f)
    {
        float lo = 0.f, hi = 1.f;
        if(depthAt(1.f) <= 0.f)
        {
            for(int i = 0; i < handOffHalvings; i++)
            {
                const float mid = 0.5f * (lo + hi);
                (depthAt(mid) > 0.f ? lo : hi) = mid;
            }
        }
        need = za::max(0.f, hi * len - bite);
    }
    // (Back at once; out eased, from the pose's first making each frame: the move's and the view's remakes keep it.)
    if(need >= fs.wallBack[h] || acting)
    {
        fs.wallBack[h] = need;
    }
    else if(dt > 0.f)
    {
        fs.wallBack[h] += (need - fs.wallBack[h]) * ease(handBackOut, dt);
    }
    s.pos[h] += way * (za::min(fs.wallBack[h], len) / len);
    // The tests' probe (vr_flatplay_debug 2): how deep the pose had the hand (its margin in), how far back it went, and how
    // deep its point is now (0: clear of the surface; its margin not counted), each frame it met a wall.
    if(vr_flatplay_debug.value >= 2 && dt > 0.f && (fs.wallDepth[h] > 0.f || fs.wallBack[h] > 0.f))
    {
        Con_Printf("flatwall: %s hand t %.3f %s in %.2f back %.2f now %.2f\n", h == HAND_MAIN ? "main" : "off", vr_gametime,
            punching(h) ? "punch" : shoving(h) ? "shove" : "rest", fs.wallDepth[h], fs.wallBack[h],
            handpose::pointWallDepth(s.head, s.pos[h], 0.f));
    }
}
} // namespace

float stockBackMost(const hands::State& s, int h, const glm::vec3& way, float most, float keepAsked)
{
    glm::vec3 stock;
    const float clear = za::clamp(vr_flat_clear_stock.value, 0.f, stockMost);
    if(most <= 0.f || clear <= 0.f || !active() || !bodyPose() || heldIn(h) < 0 || !flatsight::stockOff(h, stock))
    {
        return most;
    }
    const float keep = keepAsked < 0.f ? clear : za::min(keepAsked, clear);
    const glm::vec3 back = axes(geom::vec(cl.viewangles)) * stock;
    const TorsoCaps caps = torsoCaps(s);
    const float want = za::min(keep, torsoClearance(caps, s.pos[h] + back, s.pos[h]));
    const glm::vec3 far = s.pos[h] + way * most;
    if(torsoClearance(caps, far + back, far) >= want)
    {
        return most;
    }
    float lo = 0.f, hi = most;
    for(int i = 0; i < 12; i++)
    {
        const float mid = 0.5f * (lo + hi);
        const glm::vec3 q = s.pos[h] + way * mid;
        (torsoClearance(caps, q + back, q) >= want ? lo : hi) = mid;
    }
    return lo;
}

namespace
{

// vr_flat_melee_probe (the tests, flatmelee_test.sh): each hand's punch, overhead swing and shove measured on the final
// pose (after the ironsight's grip and the reload: what the game gets), one "flatmelee:" line as it ends. The hand's most
// over the eye; the weapon's (the hand's forward) most up and down off the view's level (degrees); at full push, the palm
// facing the target (its normal: the hand's right turned in, as on a grip; cosine) and the fingers up (cosine); the view's
// tilt (meleeViewTilt) at its most each way; whether the other hand was in a punch at the same time.
struct MeleeProbe
{
    bool on{false};
    bool running[2]{false, false};
    bool shove[2]{false, false};
    int frames[2]{0, 0};
    float overEye[2]{-1e9f, -1e9f};
    float up[2]{-90.f, -90.f};
    float down[2]{90.f, 90.f};
    float palm[2]{-1.f, -1.f};
    float fingers[2]{-1.f, -1.f};
    float pitchLo[2]{0.f, 0.f};
    float pitchHi[2]{0.f, 0.f};
    float roll[2]{0.f, 0.f};
    float off[2]{-1.f, -1.f}; // a punch's or swing's hand at its end (its held reach): how far beside the crosshair's ray (units)
    bool overlap[2]{false, false};
    int lines{0};
};
MeleeProbe meleeProbe;

void probeMelee(const hands::State& s, const glm::vec3& aim, const glm::vec3& eye, const glm::vec3& viewUp)
{
    MeleeProbe& p = meleeProbe;
    if(!p.on)
    {
        return;
    }
    glm::vec3 org, dir;
    crosshairRay(s, aim, org, dir);
    const glm::vec3 tilt = meleeViewTilt();
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const glm::vec3 aimPoint = meleeAimPoint(s, aim, shoulder(s, h), !shoving(h)) + viewUp * za::clamp(vr_flat_shove_aim_up.value, -24.f, 24.f);
        const bool acting = punching(h) || shoving(h);
        if(acting && !p.running[h])
        {
            p.running[h] = true;
            p.shove[h] = shoving(h);
            p.frames[h] = 0;
            p.overEye[h] = -1e9f;
            p.up[h] = -90.f;
            p.down[h] = 90.f;
            p.palm[h] = p.fingers[h] = -1.f;
            p.pitchLo[h] = p.pitchHi[h] = p.roll[h] = 0.f;
            p.overlap[h] = false;
            p.off[h] = -1.f;
        }
        if(p.running[h] && acting)
        {
            glm::vec3 f, r, u;
            hands::angleVectors(s.rot[h], f, r, u);
            const float elev = glm::degrees(za::asin(za::clamp(glm::dot(f, viewUp), -1.f, 1.f)));
            p.frames[h]++;
            p.overEye[h] = za::max(p.overEye[h], glm::dot(s.pos[h] - eye, viewUp));
            p.up[h] = za::max(p.up[h], elev);
            p.down[h] = za::min(p.down[h], elev);
            if(p.shove[h] && shovePhase(h).out >= 0.999f)
            {
                const glm::vec3 palmNormal = h == HAND_MAIN ? -r : r;
                const glm::vec3 to = aimPoint - s.pos[h];
                p.palm[h] = za::max(p.palm[h], glm::length(to) > 0.001f ? glm::dot(palmNormal, glm::normalize(to)) : 0.f);
                p.fingers[h] = za::max(p.fingers[h], glm::dot(f, viewUp));
            }
            // (At the blow's end, held: beside the ray, its up and down left out. The swing's end is under the ray by
            // vr_flat_swing_end_down, and the fist's keeps near its own height.)
            if(!p.shove[h] && punching(h))
            {
                const double t = vr_gametime - fs.punchAt[h];
                const SwingTimes w = swingTimes();
                const double from = fs.punchOverhead[h] ? w.wind + w.top + w.strike : punchWind + punchStrike;
                const double to = from + (fs.punchOverhead[h] ? w.dwell : punchDwell);
                if(t >= from && t <= to)
                {
                    glm::vec3 d = s.pos[h] - org;
                    d -= dir * glm::dot(d, dir);
                    d -= viewUp * glm::dot(d, viewUp);
                    p.off[h] = za::max(p.off[h], glm::length(d));
                }
            }
            p.pitchLo[h] = za::min(p.pitchLo[h], tilt.x);
            p.pitchHi[h] = za::max(p.pitchHi[h], tilt.x);
            p.roll[h] = za::fabs(tilt.z) > za::fabs(p.roll[h]) ? tilt.z : p.roll[h];
            p.overlap[h] = p.overlap[h] || (!p.shove[h] && punching(1 - h) && !drawingBack(1 - h) && !drawingBack(h));
        }
        else if(p.running[h])
        {
            p.running[h] = false;
            p.lines++;
            // (Short: a console line wraps past its width, and the tests read it whole.)
            Con_Printf("flatmelee: %s %s: n %d eye %.1f up %.0f dn %.0f palm %.2f fing %.2f vp %.2f %.2f roll %.2f both %d off %.1f\n",
                h == HAND_MAIN ? "main" : "off", p.shove[h] ? (fs.shoveBoth ? "shove both" : "shove") : fs.punchOverhead[h] ? "swing" : "punch",
                p.frames[h], static_cast<double>(p.overEye[h]), static_cast<double>(p.up[h]), static_cast<double>(p.down[h]),
                static_cast<double>(p.palm[h]), static_cast<double>(p.fingers[h]), static_cast<double>(p.pitchLo[h]),
                static_cast<double>(p.pitchHi[h]), static_cast<double>(p.roll[h]), p.overlap[h] ? 1 : 0, static_cast<double>(p.off[h]));
        }
    }
}

} // namespace

// vr_flat_melee_probe [0|1|done]: the probe on (1, or no argument) or off; `done` prints how many lines it gave, the
// tests' end.
void meleeProbe_f()
{
    const char* a = Cmd_Argc() > 1 ? Cmd_Argv(1) : "1";
    if(!q_strcasecmp(a, "done"))
    {
        Con_Printf("flatmelee: done, %d lines\n", meleeProbe.lines);
        return;
    }
    meleeProbe = MeleeProbe{};
    meleeProbe.on = Q_atoi(a) != 0;
    Con_Printf("flatmelee: probe %s\n", meleeProbe.on ? "on" : "off");
}

void poseHands(hands::State& s, const glm::vec3& aim)
{
    // Made at the frame's start, again for the move and again for the view (hands::relatch): the same frame's poses
    // differ only by the view's turn and the player's move since. The easing steps once a frame.
    double dt = fs.lastTime > -1e8 ? vr_gametime - fs.lastTime : 0.0;
    fs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);

    // The flat branch turns the body to the view the same way; a long gun's stance turns it off the view (the view stays).
    // Sliding (vr_flatmove.cpp; his words 2026-11-08): no stance turn (the leant-back trunk turned off the view bent the
    // legs), eased out over vr_flat_slide_turn_time; and with vr_flat_slide_legs the hips and legs face the slide's way,
    // the chest turned back to the view (trunkTwist), at most vr_flat_slide_legs_twist off it.
    {
        SlideTurn& t = slideTurn;
        const bool slide = flatmove::slidePose();
        if(dt > 0.0)
        {
            const float step = static_cast<float>(dt) / za::clamp(vr_flat_slide_turn_time.value, 0.02f, 1.f);
            t.lin += za::clamp((slide ? 1.f : 0.f) - t.lin, -step, step);
        }
        const glm::vec2 vel{cl.velocity[0], cl.velocity[1]};
        if(slide && glm::length(vel) > 60.f)
        {
            t.yaw = geom::yawOf(glm::vec3{vel, 0.f});
        }
        const float share = geom::smooth01(t.lin);
        float off = 0.f;
        if(vr_flat_slide_legs.value != 0.f && share > 0.f)
        {
            off = geom::yawDelta(t.yaw, aim.y);
            const float most = za::clamp(vr_flat_slide_legs_twist.value, 0.f, 90.f);
            off = za::clamp(off, -most, most) * share;
        }
        // The hips eased after it (his note 2026-11-08: the body turned in abrupt steps sliding): the slide's way follows the
        // velocity, which steps with the server's frames and the steering, and a look past the twist's limit drags them
        // round at once. The lag fades with the slide's share; the chest stays on the view (the twist takes the lag too).
        const float want = aim.y + flatsight::stanceYaw() * (1.f - share) + off;
        const float smooth = za::clamp(vr_flat_slide_turn_smooth.value, 0.f, 1.f);
        if(share > 0.f && smooth > 0.f)
        {
            if(!t.easing)
            {
                t.easing = true;
                t.lag = t.lagRate = 0.f;
                t.yaw0 = want;
            }
            else if(dt > 0.0)
            {
                const float step = static_cast<float>(dt);
                const float omega = 4.74f / za::max(0.02f, smooth); // (95% of a step in `smooth`: vr_avatar_pitch.cpp's easePitch)
                const float x = za::clamp(geom::yawDelta(t.yaw0, want), -120.f, 120.f);
                const float temp = (t.lagRate + omega * x) * step;
                const float decay = za::exp(-omega * step);
                t.lagRate = (t.lagRate - omega * temp) * decay;
                t.lag = (x + temp) * decay;
                t.yaw0 = want + t.lag;
            }
        }
        else
        {
            t.easing = false;
            t.lag = t.lagRate = 0.f;
        }
        const float lag = t.lag * share;
        t.twist = -(off + lag);
        s.bodyYaw = want + lag;
    }

    const float reach = za::clamp(vr_flat_reach.value, 0.25f, 3.f);

    // Leaning: the hands are posed as he stands upright (`upright`: the head back where the lean took it from, the body
    // unleant) and turned with the upper body (leanRig).
    QVR_TRACY_SCOPE(zoneFlatPoseRig, "flat pose: rig");
    const LeanRig rig = leanRig(s, aim.y);
    hands::State unleant;
    if(rig.on)
    {
        unleant = s;
        unleant.head -= rig.shift;
        unleant.headHeight -= rig.shift.z / units::metresToUnits();
        unleant.lean = glm::vec3{0.f};
        unleant.leanHold = 0.f;
    }
    const hands::State& upright = rig.on ? unleant : s;

    // The arms' joints as the body stands this frame: the chest's share and the crouch in them, the stance's turn just made.
    const bool onBody = bodyPose();
    fs.shouldersKnown = onBody;
    glm::vec3 roots[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // upright
    if(onBody)
    {
        QVR_TRACY_CALL("flat pose: rest shoulders", avatar::restShoulders(upright, roots));
        // (Lying down, the body hangs vr_flat_prone_neck_drop lower than the view, its shoulders with it: the poses are
        // made off where they were, up by as much, so the gun stays where the eye's poses put it, clear of the floor; the
        // drawn arms reach up to it from the lower shoulders.)
        const float lift = proneNeckDrop() * proneShare();
        for(int h = 0; h < HAND_COUNT; h++)
        {
            roots[h].z += lift;
            fs.shoulders[h] = rig(roots[h]);
        }
    }

    // The frame the hands are placed in: the view's way and roll, its up and down held to vr_flat_pose_pitch degrees.
    // The anchor the flat hands are built on does not tip with the head (hands::bodyAnchor, vr_hands.cpp) while the body
    // the arms hang from does (avatar::solveTorso, vr_avatar.cpp): a look straight up or down carries the hands past
    // where the anchor is, so the arms are asked for what they do not have. Held to a pitch, they stay in the arc the
    // arms sweep; the hands still turn with the view (90 places them as before).
    const float posePitch = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
    const glm::vec3 placeAngles{za::clamp(aim.x, -posePitch, posePitch), aim.y, aim.z};

    glm::vec3 fwd, right, up;
    hands::angleVectors(placeAngles, fwd, right, up);

    const bool mainHolds = heldIn(HAND_MAIN) >= 0;
    const bool offHolds = heldIn(HAND_OFF) >= 0;
    fs.dualT += ((mainHolds && offHolds ? 1.f : 0.f) - fs.dualT) * (dt > 0.0 ? ease(handEase, static_cast<float>(dt)) : 0.f);

    // The eye's nudge (vr_flat_cam_*) carries the hands: the drawn hands stay where they are on the screen, and these are
    // the drawn hands, so the muzzle the shots go from is the one he sees, and it stands where the crosshair's ray is read.
    // (On the body: the nudge alone. The neck's turn moves the eye off a body that stays, and the hands are on the body.)
    const bool ownView = viewIsHisOwn(s);
    const glm::vec3 nudge = ownView ? (onBody ? camNudgeWorld(aim) : camShiftWorld(aim)) : glm::vec3{0.f};
    const glm::vec3 eye = ownView ? s.head + camShiftWorld(aim) : s.head;
    glm::vec3 viewFwd, viewRight, viewUp;
    hands::angleVectors(aim, viewFwd, viewRight, viewUp);

    QVR_TRACY_SCOPE(zoneFlatPoseHands, "flat pose: hands");
    for(int h = 0; h < HAND_COUNT; h++)
    {
        // Where the hand sits off its shoulder, in the view's frame: locked to the view. The looks (the sway, the bob, a
        // turn's lag) are the drawn hands' only (drawnOffset): the game's hands, and the shots, stay on the crosshair.
        // (E's grab poses its hand over all of this: flatfgrab::apply below.)
        const glm::vec3 rest = targetOffset(h, reach, h == HAND_OFF ? mainHolds : offHolds, aim.x);
        if(!fs.localSeen[h])
        {
            fs.local[h] = rest;
            fs.localSeen[h] = true;
        }
        else if(dt > 0.0)
        {
            fs.local[h] += (rest - fs.local[h]) * ease(handEase, static_cast<float>(dt));
        }
        // The punch is not eased. Its own phases (chambered, thrown, drawn back) are its motion, and the game's melee
        // rules ask the striking hand for vr_melee_speed (3.2 m/s, a weapon's swing 1.25 times that, twice that for a
        // full blow): eased toward the punch's own target at handEase the hand never gets there, and the punch lands as
        // a shove with no blow in it. Where a hand sits and what E reaches for are eased, as they were.
        const float sideOf = h == HAND_OFF ? -1.f : 1.f;
        const glm::vec3 fromAnchor{anchorFromShoulder.x, anchorFromShoulder.y * sideOf, anchorFromShoulder.z};
        const glm::vec3 restOld = rest - fromAnchor; // the numbers off the old anchor: the punch's, and vr_flat_body_pose 0's
        // A punch is the melee rules' to judge, a real blow (vr_handsource.hpp: an action); the rest of the pose is made
        // up, the shove's push too (its blow is the game's own: VR_FlatShove), and no gesture reads it.
        if(punching(h))
        {
            handsource::mark(h, handsource::Source::Action, "punch");
        }
        if(shovePassesBodies(h))
        {
            handsource::passBodies(h); // (the shove's hands on their way back: vr_flat_shove_pass_bodies)
        }
        // What the crosshair is on, off this shoulder in the frame the hands are placed in: the punch and the shove go there.
        glm::vec3 target{0.f};
        glm::vec3 aimPoint{0.f}; // (and that point itself)
        if(punching(h) || shoving(h))
        {
            aimPoint = meleeAimPoint(s, aim, shoulder(s, h) + nudge, punching(h));
            const glm::vec3 at = aimPoint - (shoulder(s, h) + nudge);
            target = {glm::dot(at, fwd), glm::dot(at, right), glm::dot(at, up)};
        }
        const glm::vec3 swing = punchSwing(h, restOld, target) - restOld + shoveOffset(h, target - restOld);
        const glm::vec3 off = onBody ? fs.local[h] + swing : fs.local[h] - fromAnchor + swing;
        if(onBody)
        {
            const glm::vec3 gunView = heldIn(h) < 0 ? glm::vec3{0.f}
                                                    : viewFwd * za::clamp(vr_flat_gun_view_fwd.value, -poseTweakMax, poseTweakMax) +
                                                          viewRight * za::clamp(vr_flat_gun_view_right.value, -poseTweakMax, poseTweakMax) +
                                                          viewUp * za::clamp(vr_flat_gun_view_up.value, -poseTweakMax, poseTweakMax);
            // (The nudge and the gun's own view offsets carry the hand after the body's limits: the stock's is measured with them.
            // Leaning, posed upright and turned with the body; a punch's or a shove's swing after it, on the crosshair's point.)
            s.pos[h] = rig(handOnBody(upright, roots, h, aim, fs.local[h], eye - rig.shift, nudge + gunView) + nudge + gunView) +
                       fwd * swing.x + right * swing.y + up * swing.z;
        }
        else
        {
            const glm::vec3 still = off - swing;
            s.pos[h] = rig(shoulder(upright, h) + nudge + fwd * still.x + right * still.y + up * still.z) + fwd * swing.x + right * swing.y +
                       up * swing.z;
        }
        // (Leaning, the hand's turn rolled vr_flat_lean_gun_roll of the view's lean roll: aimed again from where the turn took it.)
        s.rot[h] = aimRotAtCrosshair(aim + glm::vec3{0.f, 0.f, gunLeanRoll()}, h);
        s.rot[h].z += vr_fakevr_handroll.value;
        // A melee weapon is always held turned out (vr_flat_melee_hold_yaw) and rolled (_hold_roll; his notes 2026-11-10);
        // its swing turns from there.
        if(meleeIn(h))
        {
            s.rot[h] = turnedHand(s.rot[h], {0.f, za::clamp(vr_flat_melee_hold_yaw.value, -90.f, 90.f),
                                                za::clamp(vr_flat_melee_hold_roll.value, -90.f, 90.f)},
                sideOf);
        }

        // A weapon's overhead swing (vr_flat_swing): its own way round the shoulder from where the hand is posed, the
        // weapon turned up over the head and down through the strike (overheadSwing).
        if(fs.punchOverhead[h] && punching(h))
        {
            glm::vec3 turn{0.f};
            s.pos[h] = overheadSwing(h, shoulder(s, h) + nudge, s.pos[h], eye, aimPoint, fwd, right, up, turn);
            s.rot[h] = turnedHand(s.rot[h], turn, sideOf);
        }

        // An empty hand at rest hangs from its shoulder whatever the look: level with the body, its fingers down, not out
        // in front of him pointing where he looks. Not while it does anything: a punch, E's reach, a grip it is going to.
        const bool hangs = heldIn(h) < 0 && !punching(h) && !shoving(h) && !flatfgrab::busy(h) &&
                           !(flatsight::adsHelper(h));
        // The rest's pose (vr_flat_empty_hand_pose): relaxed, the arm down the body, or fists up in a guard (2: the guard
        // with both hands empty), eased from one to the other as the other hand takes or lets go of a weapon.
        const int restPose = static_cast<int>(za::round(poseNum(vr_flat_empty_hand_pose)));
        const bool guardWanted = restPose == 1 || (restPose == 2 && !mainHolds && !offHolds);
        const float takeTime = poseNum(vr_flat_empty_take_time);
        if(heldIn(h) >= 0 && (restPose < 0 || takeTime <= 0.f))
        {
            fs.hang[h] = 0.f; // (a weapon's shots go along the crosshair from the frame it is taken)
        }
        else if(dt > 0.0)
        {
            // (A weapon taken comes up out of the rest over vr_flat_empty_take_time: 95% of the way in that time.)
            const float k = heldIn(h) >= 0 ? 3.f / takeTime : handEase;
            fs.hang[h] += ((hangs ? 1.f : 0.f) - fs.hang[h]) * ease(k, static_cast<float>(dt));
            if(heldIn(h) < 0 || fs.hang[h] < 0.01f)
            {
                fs.guard[h] += ((guardWanted ? 1.f : 0.f) - fs.guard[h]) * ease(handEase, static_cast<float>(dt));
            }
            if(fs.hang[h] < 0.01f && heldIn(h) >= 0)
            {
                fs.hang[h] = 0.f;
            }
        }
        if(fs.hang[h] > 0.f)
        {
            glm::vec3 levelFwd, levelRight, levelUp;
            hands::angleVectors({0.f, aim.y, 0.f}, levelFwd, levelRight, levelUp);
            glm::vec3 at = off;
            float pitchTo = hangPitch, rollTo = 0.f;
            if(restPose >= 0)
            {
                // (Shares of the arm's reach off the shoulder, level with the body; the punch's or shove's swing on top.)
                const float span = armSpan(), g = fs.guard[h];
                const auto mixed = [g](const Tunable& relaxed, const Tunable& guard) {
                    return poseNum(relaxed) + (poseNum(guard) - poseNum(relaxed)) * g;
                };
                at = glm::vec3{mixed(vr_flat_relaxed_fwd, vr_flat_guard_fwd), sideOf * mixed(vr_flat_relaxed_out, vr_flat_guard_out),
                         -mixed(vr_flat_relaxed_down, vr_flat_guard_down)} *
                         span +
                     swing;
                pitchTo = mixed(vr_flat_relaxed_pitch, vr_flat_guard_pitch);
                rollTo = sideOf * mixed(vr_flat_relaxed_roll, vr_flat_guard_roll);
            }
            const glm::vec3 level = shoulder(s, h) + nudge + levelFwd * at.x + levelRight * at.y + levelUp * at.z;
            s.pos[h] += (level - s.pos[h]) * fs.hang[h];
            s.rot[h].x += (pitchTo - s.rot[h].x) * fs.hang[h];
            s.rot[h].z += (rollTo - s.rot[h].z) * fs.hang[h];
        }
        QVR_TRACY_CALL("flat pose: off wall", handOffWall(s, h, shoulder(s, h) + nudge, static_cast<float>(dt)));
    }

    // The ironsight and the free hand on a grip (vr_flatsight.cpp): over the pose just made, with E's reach for the
    // empty hand so a force grab still reaches out with it.
    // (E's reach is no longer an offset here: the grabbing hand's pose is flatfgrab's, below, and a hand on a grip lets go of it
    // for its grab: flatfgrab::busy in vr_flatsight.cpp and vr_twohand.cpp.)
    QVR_TRACY_CALL("flat pose: sight", flatsight::apply(s, aim, glm::vec3{0.f}));
    // A reload's trip (vr_flatreload.cpp): the gun's hand to the hip holster and back, over all of the above.
    QVR_TRACY_CALL("flat pose: reload", flatreload::apply(s));
    // E's grab (its hand's palm turned to what it pulls) and a prop held up in a hand (vr_flatfgrab.cpp), over all of it.
    QVR_TRACY_CALL("flat pose: force grab", flatfgrab::apply(s, aim));
    // The quick grenade's hand (vr_flatgrenade.cpp) and the hands' swap (vr_flatswap.cpp), over all of it.
    QVR_TRACY_CALL("flat pose: grenade", flatgrenade::apply(s));
    QVR_TRACY_CALL("flat pose: swap", flatswap::apply(s));
    // A shove turns the hand to its palm at the target, the fingers up (vr_flat_shove_palm; his notes 2026-11-10), a
    // weapon in it turned with it (the two-handed shove of a weapon in each hand keeps them held). Last, over the rest's
    // hang (it eases out as the shove starts) and the gun's own aim at the crosshair's point (flatsight's: a gun's turn
    // there undid the palms, measured 2026-11-10); the probe measures the pose the game gets.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(const float palm = shovePhase(h).palm * za::clamp(vr_flat_shove_palm.value, 0.f, 1.f); palm > 0.f)
        {
            const glm::vec3 at = meleeAimPoint(s, aim, shoulder(s, h) + nudge, false) + up * za::clamp(vr_flat_shove_aim_up.value, -24.f, 24.f);
            s.rot[h] = blendAngles(s.rot[h], palmAt(h, at - (shoulder(s, h) + nudge), viewUp), palm);
        }
    }
    probeMelee(s, aim, eye, viewUp);
    traceHandZ = s.pos[HAND_MAIN].z;
    traceHand = s.pos[HAND_MAIN];
    traceHead = s.head;
    traceShoulder = shoulder(s, HAND_MAIN);
    if(traceBodyOn)
    {
        // (A point the standing pelvis carries, under the standing head: where the pelvis has taken it.)
        QVR_TRACY_SCOPE(zoneTraceBody, "flat pose: trace body");
        const avatar::Follower body(s);
        const float below = 0.8f * units::metresToUnits() * units::bodyScale();
        tracePelvis = body(avatar::Part::Pelvis, avatar::standing(s).head - glm::vec3{0.f, 0.f, below});
    }

    if(vr_flatplay_debug.value && host_framecount % 30 == 0 && dt > 0.0)
    {
        const glm::vec3 a = s.pos[HAND_OFF] - s.head;
        const glm::vec3 b = s.pos[HAND_MAIN] - s.head;
        const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
        Con_Printf("flatplay: aim %.1f %.1f speed %.0f punch off %s main %s grab %d hands: off (%.1f, %.1f, %.1f) "
                   "main (%.1f, %.1f, %.1f) from the head\n",
            aim.x, aim.y, speed, punching(HAND_OFF) ? "running" : "no", punching(HAND_MAIN) ? "running" : "no", fs.grabs,
            a.x, a.y, a.z, b.x, b.y, b.z);
    }
}

// The tests' probe (vr_flatplay_debug 2, flatscreen only): each drawn hand against the arm the body poses for it.
// `off-wrist` is how far the drawn hand's wrist sits off the body's wrist, the same measure vr_body_wrist_info prints
// (world units, and the millimetres of the body: what the hand is off the arm by) — the arm's own wrist, mapped through
// the mount learned while the arm was long enough to reach the hand, so that it reads 0 where the arm holds the hand.
// `arm` is what the arm can hold a hand at from its shoulder (vr_body_arm_length, vr_body_arm_stretch,
// vr_body_shoulder_reach), `past its arm` how far the drawn hand is beyond that, `shoulder to hand` where the drawn hand
// is, `bend` the wrist's bend against the forearm the body poses. One line a hand a frame while that hand punches, else
// one every half second.
void probeArm(const hands::State& s, int h, const glm::vec3& drawn, const glm::vec3& drawnRot)
{
    if(vr_flatplay_debug.value < 2 || (!punching(h) && host_framecount % 45 != 0))
    {
        return;
    }
    const float m2w = units::metresToUnits() * units::bodyScale();
    const float span = armSpan();
    avatar::ForearmFrame f;
    avatar::Skeleton k;
    if(!avatar::forearmFrame(h, 1.f, f) || !avatar::skeleton(k))
    {
        Con_Printf("flatarm: hand %s t %.4f punch %d no body posed this frame\n", h == HAND_MAIN ? "main" : "off",
            vr_gametime, punching(h) ? 1 : 0);
        return;
    }
    const glm::vec3 joint = k.shoulder[h];
    const glm::mat3 handAxes = axes(drawnRot);
    // The mount is learned whenever the arm is really holding this hand — a unit or more inside its reach — punch running
    // or not. Learned only at rest, it went stale across a look: the wrist's bend changes, and an old mount maps the
    // arm's wrist through the wrong bend, which read as an 857 mm gap on one frame after a scripted straight-up look when
    // the arm itself was 0.00 units short of the hand (measured 2026-11-03). A fresh mount reads 0 where it should.
    if(glm::distance(joint, drawn) + 1.f < span)
    {
        fs.armMount[h] = glm::transpose(handAxes) * (f.point - drawn); // the arm's wrist, in the hand's own frame
        fs.mountSeen[h] = true;
    }
    const float d = glm::distance(joint, drawn);
    const float gap = fs.mountSeen[h] ? glm::distance(f.point, drawn + handAxes * fs.armMount[h]) : -1.f;
    const float bend = glm::degrees(za::acos(za::clamp(glm::dot(f.axes[0], hands::forward(drawnRot)), -1.f, 1.f)));
    const double dt = vr_gametime - fs.probeTime[h];
    fs.probeTime[h] = vr_gametime;
    const float speed = dt > 0.0 ? glm::distance(drawn, fs.lastDrawn[h]) / static_cast<float>(dt) : 0.f;
    fs.lastDrawn[h] = drawn;
    Con_Printf("flatarm: hand %s t %.4f punch %d off-wrist %.2f u (%.0f mm) past its arm %.2f u arm %.2f shoulder to "
               "hand %.2f ik %.2f speed %.0f bend %.0f pitch %.1f\n",
        h == HAND_MAIN ? "main" : "off", vr_gametime, punching(h) ? 1 : 0, gap, gap * 1000.f / m2w,
        za::max(0.f, d - span), span, d, d / span, speed, bend, drawnRot.x);
}

glm::mat3 drawnWallTurn(int h, glm::vec3& shift)
{
    const bool on = active() && h >= 0 && h < 2;
    shift = on ? feel.wallShift[h] : glm::vec3{0.f};
    return on ? feel.wallTurn[h] : glm::mat3{1.f};
}

void drawnOffset(const hands::State& s, int h, glm::vec3& pos, glm::vec3& angles)
{
    pos = angles = glm::vec3{0.f};
    feel.wallTurn[h] = glm::mat3{1.f};
    feel.wallShift[h] = glm::vec3{0.f};
    if(!active() || !s.valid)
    {
        return;
    }
    stepFeel();

    // The pose's frame (its head, the view's angles it was made with) and the drawn view's (its bob, roll and kick, its
    // own easing up a step, and where vr_flat_cam_fwd / _up / _side put the eye -- V_CalcRefdef has already moved this
    // origin by them, VR_FlatViewDrop below): the hands moved from one onto the other, so they are locked to what is
    // drawn, the nudged eye included. Whether the drawn view still counts as his own eyes is viewIsHisOwn's, the same
    // answer crosshairRay gives the shot's crosshair.
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const glm::vec3 cam = flycam::eyeOrg(); // (his own eye, not the fly camera's: vr_flycam.cpp)
    const bool onCam = viewIsHisOwn(s);
    const glm::vec3 from = onCam ? s.head + camShiftWorld(aim) : s.head; // (the pose's hands carry the nudge: poseHands)
    const glm::vec3 view = onCam ? cam : s.head;
    // (The view's own move off the pose's eye -- the walk's bob, a step's easing -- by vr_flat_body_bob's share, as the body rides it:
    // bodyRide. The sights below stand on the view itself.)
    const glm::vec3 camAngles = onCam ? flycam::eyeAngles()
                                      : aim;
    // (And the walk's forward and back bob, which the body rides too: bodyRide.)
    const glm::vec3 forwardBob = onCam ? fwdBob(camAngles) : glm::vec3{0.f};
    const glm::vec3 to = from + (view - from) * rideShare() + forwardBob;

    // The turn's lag, shown (softly capped at vr_flat_turnlag_cap times the amount). Negative, the same turned the other way:
    // the hands lead the turn, swinging into it ahead of the view (his words: "Goldeneye-like 'turn with the camera'").
    const float lagAmount = za::clamp(vr_flat_turnlag.value, -lagAmountMax, lagAmountMax);
    const float lagCap = za::clamp(vr_flat_turnlag_cap.value, 0.f, lagCapMax) * za::fabs(lagAmount);

    // On the ironsight the hand is the solve's, from this camera (the block below): the looks are spent out over its travel.
    glm::vec3 sightHand{0.f};
    glm::mat3 sightTurn{1.f};
    const float onSight = flatsight::drawnSight(h, sightHand, sightTurn);
    const float looks = 1.f - onSight;

    const glm::vec2 lag{softCap(feel.lag.x * lagShown * lagAmount, lagCap) * looks, softCap(feel.lag.y * lagShown * lagAmount, lagCap) * looks};

    // (Leaning, the pose is already turned with the body and its guns rolled (poseHands' leanRig): the view's lean roll is the pose's
    // own, not a turn of the drawn hands round the eye on top of it.)
    const glm::mat3 poseAxes = axes(aim + glm::vec3{0.f, 0.f, onCam ? flatmove::leanRoll() : 0.f});
    const glm::mat3 drawnAxes = axes(camAngles + glm::vec3{lag.x, lag.y, 0.f});
    const glm::mat3 camAxes = axes(camAngles);
    const glm::mat3 turn = drawnAxes * glm::transpose(poseAxes);

    // The step's sway and the walk's bob, in the view's frame.
    const float bob = za::clamp(vr_flat_bob.value, 0.f, 3.f);
    const float rest = 1.f - za::clamp(feel.amp, 0.f, 1.f);
    const glm::vec3 walkBob{0.f, bob * bobSide * feel.amp * za::sin(feel.phase), bob * bobUp * feel.amp * (za::cos(2.f * feel.phase) - 1.f) * 0.5f};
    const glm::vec3 breath{0.f, bob * breathSide * rest * za::sin(feel.breath * 0.5f), bob * breathUp * rest * za::sin(feel.breath)};
    const glm::vec3 local = (feel.sway + walkBob + breath) * looks;
    // In the ironsight the sway and the walk's bob are kept at vr_flat_ads_sway / vr_flat_ads_bob of themselves (no breathing: at rest
    // the sights stay exact).
    const glm::vec3 adsLocal =
        feel.sway * za::clamp(vr_flat_ads_sway.value, 0.f, 1.f) + walkBob * za::clamp(vr_flat_ads_bob.value, 0.f, 1.f);

    glm::vec3 drawn = to + turn * (s.pos[h] - from) + camAxes * local;

    // The arm holds the hand it can reach. A punch throws the fist, a look turns the whole frame, a knock or a shake
    // moves the hand: any of them can ask the arm for more than it has, and then the drawn hand hangs off the wrist the
    // arm ends at. The drawn hand is put back where the arm can hold it: a straight line out of the shoulder the body
    // poses that arm on, at most vr_flat_arm_hold times what the arm reaches (armSpan: the bones, their stretch and the
    // shoulder's own reach; 0: no limit, the hands as they were placed). The hand keeps its angles: the look, the shot
    // and the crosshair are the rotation's, and this is only ever the drawn hand (the mode's own, vr_flatplay.cpp).
    const float hold = za::clamp(vr_flat_arm_hold.value, 0.f, 2.f);
    if(hold > 0.f)
    {
        // (The body as last drawn, a frame old, carried with the head's move since (drawnSkeleton): taken where it was
        // drawn, the shoulder lagged by the frame's move, a slide's 400 u/s times a frame time that varies: the gun and the
        // arm jittered sliding, 3 cm rms at the real clock (his report 2026-11-10; ROUND24.md "Slide jitter"). Still, a pose
        // far off the drawn one -- a respawn -- takes the pose's own shoulder, that frame.)
        avatar::Skeleton k;
        const glm::vec3 poseRoot = shoulder(s, h);
        const glm::vec3 root = drawnSkeleton(s, k) && glm::distance(k.shoulder[h], poseRoot) < bodyJumpedOver ? k.shoulder[h] : poseRoot;
        const float span = armSpan() * hold;
        const glm::vec3 reachOut = drawn - root;
        const float d = glm::length(reachOut);
        if(span > 0.f && d > span)
        {
            drawn = root + reachOut * (span / d);
            traceArmHolds++;
        }
    }

    // The last word on the ironsight: the sights on the ray of the camera this frame is drawn with, the solve's hand in its
    // frame (it stays within the arm: vr_flatsight.cpp's reliefFor), whatever the frame's bob, kick and nudges did to it.
    drawn += (view + forwardBob + camAxes * (sightHand + adsLocal) - drawn) * onSight;

    pos = drawn - s.pos[h];
    feel.drawnAt[h] = drawn;

    // The hand's turn in the view's frame (the pose's: onto what the crosshair is on), eased as gunTurnEase says.
    const glm::quat gunTurn = glm::quat_cast(glm::transpose(poseAxes) * axes(s.rot[h]));
    if(!feel.gunTurnSeen[h])
    {
        feel.gunTurn[h] = gunTurn;
        feel.gunTurnSeen[h] = true;
    }
    else if(feel.gunTurnAt[h] != feel.lastTime)
    {
        feel.gunTurn[h] = glm::slerp(feel.gunTurn[h], gunTurn, ease(gunTurnEase, feel.dt));
    }
    feel.gunTurnAt[h] = feel.lastTime;
    const glm::quat drawnTurn = onSight > 0.f ? glm::slerp(feel.gunTurn[h], glm::quat_cast(sightTurn), onSight) : feel.gunTurn[h];
    // At a wall the way back couldn't clear, the muzzle tilted up round the hand (flatsight::wallTilt): drawn only.
    glm::vec3 tiltPivot{0.f};
    const float tilt = flatsight::wallTilt(h, &tiltPivot);
    const glm::mat3 wallTurn = flatsight::wallTurn(h, drawnAxes);
    feel.wallTurn[h] = wallTurn;
    if(tilt != 0.f)
    {
        const glm::vec3 pivot = drawnAxes * tiltPivot;
        // (Sighted, the tilted gun comes down off the eye too, by vr_flat_wall_tilt_drop, all of it by wallDropFull's tilt:
        // tilted on the sight line, its underside filled the screen.)
        const float drop = za::clamp(vr_flat_wall_tilt_drop.value, 0.f, 20.f) * onSight * geom::smooth01(za::clamp(tilt / wallDropFull, 0.f, 1.f));
        feel.wallShift[h] = pivot - wallTurn * pivot - drawnAxes[2] * drop;
        pos += feel.wallShift[h];
    }
    // Dual wielding lying down, the drawn gun's own turn in its hand (vr_flat_dual_prone_*_pitch / _yaw / _roll): drawn only.
    const float dualShare = heldIn(h) >= 0 ? proneShare() * fs.dualT : 0.f;
    const glm::mat3 dualTurn = dualShare > 0.f ? axes(dualProneTurn(h) * dualShare) : glm::mat3{1.f};
    const glm::mat3 handAxes = wallTurn * drawnAxes * glm::mat3_cast(drawnTurn) * dualTurn;
    const glm::vec3 drawnRot = hands::anglesFromVectors(handAxes[0], handAxes[2]);
    for(int i = 0; i < 3; i++)
    {
        angles[i] = za::remainder(drawnRot[i] - s.rot[h][i], 360.f);
    }
    angles.x += looks * bob * breathPitch * rest * za::sin(feel.breath);
    const float bobTurn = looks + onSight * za::clamp(vr_flat_ads_bob.value, 0.f, 1.f);
    angles.y += bobTurn * bob * bobYaw * feel.amp * za::sin(feel.phase);
    angles.z += bobTurn * bob * bobRoll * feel.amp * za::sin(feel.phase);

    probeArm(s, h, drawn, drawnRot);
}

// The crosshair's ray: where the middle of the screen points, as this pose has it: the pose's head (its crouch's drop in
// it) with the eye's nudges (vr_flat_cam_fwd / _up / _side; not in a chase view, the ragdoll tool's orbit, a view standing
// further off than keepNear), along the aim. Not r_refdef's: the move's pose is made before the frame's view, and that
// one is a frame behind the mouse and the player's move. This one ray is what the ironsight measures and centres the held
// weapon onto and what a gun aims at the point of (vr_flatsight.cpp), and the head and the nudge are what drawnOffset
// carries the drawn hands from: the gun and the crosshair cannot be measuring from different places.
void crosshairRay(const hands::State& s, const glm::vec3& aim, glm::vec3& org, glm::vec3& dir)
{
    org = viewIsHisOwn(s) ? s.head + camShiftWorld(aim) : s.head;
    dir = flatsight::rayDir(aim); // (forward, unless the ironsight's picture is moved on the screen, the crosshair with it)
}

glm::vec3 drawnHand(int h)
{
    return feel.drawnAt[h];
}

float gunLeanRoll()
{
    return active() ? flatmove::leanRoll() * za::clamp(vr_flat_lean_gun_roll.value, 0.f, 1.f) : 0.f;
}

glm::vec3 shoulderOf(const hands::State& s, int h)
{
    return shoulder(s, h);
}

float armReach()
{
    return armSpan();
}

glm::vec3 headLean(const glm::vec3& aim)
{
    return active() && vr_flat_eye_pivot.value != 0.f ? chestCarry(aim) : glm::vec3{0.f};
}

glm::vec3 eyeNudge(const glm::vec3& aim)
{
    return camShiftWorld(aim);
}

Orbit orbit;

// vr_flat_orbit [yaw] [distance] [down]: the drawn view off him, looking back at his pose. No arguments: back in his eyes.
void orbit_f()
{
    orbit.yaw = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 0.f;
    orbit.dist = Cmd_Argc() > 1 ? za::max(30.f, Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 44.f) : 0.f;
    orbit.down = Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 8.f;
}

void printMove(const hands::State& s)
{
    // The main hand's shot as the move sends it (its weapon's Shot Pitch and Yaw on the hand's angles) against the view
    // the move carries: the crosshair's.
    const int slot = heldIn(HAND_MAIN);
    const glm::vec3 shot = slot >= 0 ? weapons::shotAngles(s.rot[HAND_MAIN], slot, false) : s.rot[HAND_MAIN];
    const glm::vec3 f = hands::forward(shot);
    const glm::vec3 v = hands::forward(geom::vec(cl.viewangles));
    Con_Printf("flatmove: t %.4f weapon %d shot off the view %.4f deg (view %.2f %.2f)\n", vr_gametime, slot,
        glm::degrees(za::acos(za::clamp(glm::dot(f, v), -1.f, 1.f))), cl.viewangles[0], cl.viewangles[1]);
    flatsight::print(s); // and what the ironsight and the free hand did to this frame's pose (vr_flatsight.cpp)
}

void printDrawn(const float* origin, const float* angles)
{
    // The drawn weapon in the camera's frame (forward, right, up), its angles less the camera's, and what the view does.
    glm::vec3 f, r, u;
    const glm::vec3 cam{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const glm::vec3 camAngles{r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]};
    hands::angleVectors(camAngles, f, r, u);
    const glm::vec3 d = geom::vec(origin) - cam;
    // The weapon's turn in the camera's own frame (forward, left, up), not its angles less the camera's: those differ
    // whenever the camera rolls -- strafing does, cl_rollangle's 2 degrees -- and a gun pitched 7 degrees off the view
    // read a quarter of a degree of yaw off it while it stood rigidly in the rolled view (2026-11-09; the entity's pitch
    // is the model's, negated: held::axesFromAngles).
    glm::vec3 gf, gr, gu;
    hands::angleVectors(glm::vec3{-angles[0], angles[1], angles[2]}, gf, gr, gu);
    const auto inCam = [&](const glm::vec3& v) { return glm::vec3{glm::dot(v, f), -glm::dot(v, r), glm::dot(v, u)}; };
    const glm::vec3 rel = hands::anglesFromVectors(inCam(gf), inCam(gu));
    Con_Printf("flatview: t %.4f local %.3f %.3f %.3f yaw %.3f pitch %.3f roll %.3f view %.2f %.2f speed %.0f bob %.2f "
               "ground %d lag %.2f\n",
        vr_gametime, glm::dot(d, f), glm::dot(d, r), glm::dot(d, u), za::remainder(rel.y, 360.f), -rel.x,
        za::remainder(rel.z, 360.f), camAngles.x, camAngles.y, glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]}),
        feel.amp, cl.onground ? 1 : 0, feel.lag.y);
    // The camera this frame is drawn from, and the player it is drawn for: the eye's place and the eye's own nudges
    // (vr_flat_cam_fwd forward, vr_flat_cam_side the view's right, vr_flat_cam_up), against the origin he really stands
    // at -- which these do not touch (his box and his server-side position are measured from there: vr_hull.cpp,
    // vr_server.cpp). His eye's height above that origin is what the mode moves: the crouch's drop and these, nothing else.
    const glm::vec3 off = camShift();
    const glm::vec3 his = geom::vec(cl_entities[cl.viewentity].origin);
    const glm::vec3 pivot = neckPivotShift(camAngles);
    Con_Printf("flatcam: t %.4f origin %.1f %.1f %.1f eye %.1f above him %.1f nudge %.2f %.2f %.2f keep %.1f neck turn %.2f %.2f %.2f\n",
        vr_gametime, his.x, his.y, his.z, r_refdef.vieworg[2], r_refdef.vieworg[2] - his.z,
        off.x, off.y, off.z, keepNear(), pivot.x, pivot.y, pivot.z);
}

struct ModelFovBatches
{
    long long drawn{0};
    long long inEye{0};
} modelFovBatches;

// The model FOV's numbers, and each part's distance from the eye as posed: what vr_flat_model_fov_near and _far are set from.
void modelFovStatus()
{
    const ModelFov f = modelFov();
    Con_Printf("  model fov: %g, scale %.3f, in full to %.1f, none past %.1f; %lld batches drawn, %lld in a headset's eye\n",
        vr_flat_model_fov.value, f.numbers.x, f.numbers.y, f.numbers.z, modelFovBatches.drawn, modelFovBatches.inEye);
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const hands::State& s = hands::current();
    avatar::Skeleton b;
    if(!avatar::skeleton(b))
    {
        Con_Printf("  model fov: the body is not posed\n");
        return;
    }
    const auto far2 = [&eye](const glm::vec3 (&p)[2]) { return za::max(glm::distance(eye, p[0]), glm::distance(eye, p[1])); };
    const auto near2 = [&eye](const glm::vec3 (&p)[2]) { return za::min(glm::distance(eye, p[0]), glm::distance(eye, p[1])); };
    Con_Printf("  from the eye: muzzle %.1f %.1f; shoulder %.1f..%.1f; elbow %.1f..%.1f; wrist %.1f..%.1f\n",
        s.muzzleValid[0] ? glm::distance(eye, s.muzzle[0]) : 0.f, s.muzzleValid[1] ? glm::distance(eye, s.muzzle[1]) : 0.f,
        near2(b.shoulder), far2(b.shoulder), near2(b.elbow), far2(b.elbow), near2(b.wrist), far2(b.wrist));
    if(b.legs)
    {
        Con_Printf("  from the eye: hip %.1f..%.1f; knee %.1f..%.1f; ankle %.1f..%.1f; toe %.1f..%.1f\n", near2(b.hip), far2(b.hip),
            near2(b.knee), far2(b.knee), near2(b.ankle), far2(b.ankle), near2(b.toe), far2(b.toe));
    }
}

void status_f()
{
    Con_Printf("flatscreen play: -flatscreenmode %s, a headset session %s, the mode %s\n", fs.flag ? "given" : "not given",
        vrActive() ? "up" : "none", active() ? "on" : "off");
    if(!fs.flag)
    {
        Con_Printf("  start the game with -flatscreenmode to play it on the desktop screen.\n");
        return;
    }
    Con_Printf("  vr_flatplay %g (the game code's gate), vr_flat_reach %g, vr_flat_sway %g, vr_flat_bob %g, vr_flat_turnlag %g "
               "(recovery %g), vr_flat_arm_hold %g, vr_flat_pose_pitch %g\n",
        vr_flatplay.value, vr_flat_reach.value, vr_flat_sway.value, vr_flat_bob.value, vr_flat_turnlag.value,
        vr_flat_turnlag_speed.value, vr_flat_arm_hold.value, vr_flat_pose_pitch.value);
    const float lookUp = za::clamp(vr_flat_pitch_up.value, 0.f, 90.f);
    const float lookDown = za::clamp(vr_flat_pitch_down.value, 0.f, 90.f);
    Con_Printf("  the look: vr_flat_pitch_up %g, vr_flat_pitch_down %g: his view stays between %g and %g (Quake's pitch, "
               "positive looking down). Plain play and a headset are bounded by cl_maxpitch / cl_minpitch instead, which "
               "stay the player's own setting.\n",
        vr_flat_pitch_up.value, vr_flat_pitch_down.value, -lookUp, lookDown);
    // Short lines: the console wraps a long one, and these numbers are meant to be read off it and typed back in.
    Con_Printf("  where the weapon and the hands sit, in the view's frame (forward, out at the hand's own side, down):\n"
               "    held weapon %g %g %g (vr_flat_gun_*), crouched %g %g %g (vr_flat_gun_*_crouch)\n"
               "    an empty hand %g %g %g, a supporting hand %g %g %g (vr_flat_empty_*, vr_flat_support_*)\n"
               "    vr_flat_reach %g times their size; a bigger _down is the lower hand\n"
               "    the drawn view counts as his own eyes while it stands within %g of his head (vr_flat_gun_near),\n"
               "    eased to %g while the ironsight is on (vr_flat_ads_near).\n",
        poseNum(vr_flat_gun_ahead), poseNum(vr_flat_gun_down), poseNum(vr_flat_gun_out),
        poseNum(vr_flat_gun_ahead_crouch), poseNum(vr_flat_gun_down_crouch), poseNum(vr_flat_gun_out_crouch),
        poseNum(vr_flat_empty_fwd), poseNum(vr_flat_empty_down),
        poseNum(vr_flat_empty_out), poseNum(vr_flat_support_fwd), poseNum(vr_flat_support_down),
        poseNum(vr_flat_support_out), za::clamp(vr_flat_reach.value, 0.25f, 3.f), keepNear(),
        za::clamp(vr_flat_ads_near.value, nearMin, nearMax));
    const glm::vec3 eye = camShift();
    Con_Printf("  his eye on the screen: %g forward, %g up, %g to the view's right (vr_flat_cam_fwd / _up / _side: + is "
               "towards what he looks at, + is higher, + is the view's own right), in the view's own frame and on top of "
               "the crouch's own drop. Only the drawn view moves, and with it the crosshair's ray every aiming measurement "
               "uses (vr_flat_gun_near %g measures it with these put back): his origin, his collision box and his body are "
               "measured from where he stands.\n",
        eye.x, eye.y, eye.z, za::clamp(vr_flat_gun_near.value, nearMin, nearMax));
    Con_Printf("  the neck: %s, the eye %g forward and %g up of it (vr_flat_eye_pivot, vr_flat_eye_fwd, vr_flat_eye_up): the look's pitch "
               "turns the eye about it, at a level look it stands where it always did.\n",
        vr_flat_eye_pivot.value != 0.f ? "on" : "off", za::clamp(vr_flat_eye_fwd.value, 0.f, eyeOffMax), za::clamp(vr_flat_eye_up.value, 0.f, eyeOffMax));
    if(const float flatNear = VR_FlatNearPlane(); flatNear > 0.f)
    {
        Con_Printf("  near plane: %g units (vr_nearclip), float depth\n", flatNear);
    }
    else
    {
        Con_Printf("  near plane: the desktop's, 24-bit depth (held items drawn at it: vr_nearclip_held)\n");
    }
    Con_Printf("  E pressed %d times (force grab); punches: the main hand %d, the off hand %d. E and V are read from the "
               "keyboard, not from bindings; each hand's punch starts from V, from its fire button with a weapon a swing "
               "strikes with (LMB / +attack the main hand, RMB / +offhandattack the off hand), or from vr_flat_punch.\n",
        fs.grabs, fs.punches[HAND_MAIN], fs.punches[HAND_OFF]);
    const int lone = loneWeaponHand();
    Con_Printf("  The mouse: %s (one weapon held, either hand: LMB fires it, RMB the ironsight, vr_flat_aim_mode %g %s; "
               "else LMB the right hand, RMB the left). %s pressed %d times (vr_flat_key_swap: the hands swapped), %s %d "
               "(vr_flat_key_bullettime: bullet time).\n",
        lone < 0 ? "LMB the right hand, RMB the left" : lone == HAND_MAIN ? "LMB the right hand's weapon, RMB aims"
                                                                           : "LMB the left hand's weapon, RMB aims",
        vr_flat_aim_mode.value, vr_flat_aim_mode.value != 0.f ? "toggle" : "hold", vr_flat_key_swap.string, keyState.swaps,
        vr_flat_key_bullettime.string, keyState.times);
    Con_Printf("  Left Ctrl pressed %d times (crouch), F %d (the flashlight), G %d (what the hand holds dropped), read the "
               "same way. Crouched %g (held %d): his head %.1f units down of %g standing, eased in %g s each way. Ctrl is "
               "bound to `%s` now (vr_flat_crouch_fire %g: 0 the mode holds +flatcrouch, so the crouch key fires nothing, "
               "and the plain game's bind is what the config keeps; 1 Ctrl keeps its own bind and fires as it crouches).\n",
        keyState.crouches, keyState.toggles, keyState.drops, keyState.ratio,
        (keyState.cmdHeld || keyState.keyCtrl) ? 1 : 0, crouchDrop(),
        za::clamp(vr_flat_crouch_drop.value, 0.f, 40.f), za::clamp(vr_flat_crouch_time.value, 0.05f, 2.f),
        keybindings[K_CTRL] ? keybindings[K_CTRL] : "nothing", vr_flat_crouch_fire.value);
    Con_Printf("  the game code: reloading %s (R; auto %s), body interactions on, 1, 2 and the wheel cycle (empty hand "
               "included) the weapons collected into the left and right hand, an ammo or health box grabbed with E taken at once.\n",
        vr_flat_reload.value != 0.f ? "at the hip holsters" : "off", vr_flat_autoreload.value != 0.f ? "on" : "off");
    Con_Printf("  the flashlight: worn on the head, lit until F puts it out, and it stays out (a level change, a saved "
               "game).\n");
    flatsight::statusLine(); // the ironsight, and where the free hand is (vr_flatsight.cpp)
    modelFovStatus();
}

// vr_flat_flick_test [frames] [degrees]: the tests' mouse, flicked hard: for `frames` frames (144 if none) the view turns
// by `degrees` a frame (25 if none; 1800 degrees a second at 72 frames) in yaw and half that in pitch, the other way every
// three frames, as a mouse moved fast left and right does; then FlickTest::settle still frames for the game to read the last
// of it, and "flatflick: done". No gesture may come of it (vr_handsource.hpp): Misc/quakevr/flatinput_test.sh.
struct FlickTest
{
    int left{0};     // frames of flicking left
    int frame{0};    // frames flicked
    int settle{0};   // still frames left before the verdict line
    float deg{25.f}; // the turn a frame
};
FlickTest flickTest;
constexpr int flickSettle = 36; // half a second: the server's frames and the gestures' own windows (the revolver's 0.25 s)

void flickTestFrame()
{
    if(flickTest.left > 0)
    {
        const float way = (flickTest.frame / 3) % 2 == 0 ? 1.f : -1.f;
        cl.viewangles[YAW] += way * flickTest.deg;
        cl.viewangles[PITCH] = za::clamp(cl.viewangles[PITCH] + way * flickTest.deg * 0.5f, -60.f, 60.f);
        flickTest.frame++;
        if(--flickTest.left == 0)
        {
            flickTest.settle = flickSettle;
        }
        return;
    }
    if(flickTest.settle > 0 && --flickTest.settle == 0)
    {
        Con_Printf("flatflick: done, %d frames of %.0f-degree flicks; hands now %s, %s\n", flickTest.frame,
            static_cast<double>(flickTest.deg), handsource::name(handsource::source(HAND_OFF)),
            handsource::name(handsource::source(HAND_MAIN)));
    }
}

void flickTest_f()
{
    flickTest = FlickTest{};
    flickTest.left = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 144;
    flickTest.deg = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), 1.f, 44.f) : 25.f;
    Con_Printf("flatflick: %d frames of %.0f-degree flicks\n", flickTest.left, static_cast<double>(flickTest.deg));
}

// vr_flat_shove_now: the shove key's shove (its cooldown and all), from the console: the tests' button.
void shoveNow_f()
{
    startShove("vr_flat_shove_now");
}

// vr_flat_punch [main|off]: a hand's punch, driven from the console (the tests' button: V and the mouse buttons cannot
// be driven from a script). The same gate as the fire button: only a weapon a swing strikes with punches with.
void punch_f()
{
    if(!active())
    {
        Con_Printf("vr_flat_punch: flatscreen play is not running (no -flatscreenmode, or a headset has the game): "
                   "nothing to punch with.\n");
        return;
    }
    const char* arg = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "main";
    const bool mainHand = !q_strcasecmp(arg, "main") || !q_strcasecmp(arg, "right");
    const bool offHand = !q_strcasecmp(arg, "off") || !q_strcasecmp(arg, "offhand") || !q_strcasecmp(arg, "left");
    if(!mainHand && !offHand)
    {
        Con_Printf("vr_flat_punch: say `vr_flat_punch main` or `vr_flat_punch off` (not `%s`).\n", arg);
        return;
    }
    const int h = offHand ? HAND_OFF : HAND_MAIN;
    // (`vr_flat_punch main v`: V's punch, whatever the hand holds: a gun swung overhead, vr_flat_swing.)
    const bool asV = Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(2), "v");
    if(!asV && !punchesWith(h))
    {
        Con_Printf("vr_flat_punch: the %s hand holds weapon %d, which its trigger does not strike with: no punch.\n",
            h == HAND_MAIN ? "main" : "off", heldWid(h));
        return;
    }
    startPunch(h, "vr_flat_punch");
}

void noteModelFovBatch()
{
    modelFovBatches.drawn++;
    modelFovBatches.inEye += stereo::isRenderingEye() ? 1 : 0;
}

ModelFov modelFov()
{
    ModelFov f;
    if(vr_flat_model_fov.value <= 0.f || !active())
    {
        return f;
    }
    constexpr float halfDegree = static_cast<float>(M_PI) / 360.f;
    const float nearest = za::max(0.f, vr_flat_model_fov_near.value);
    f.numbers.x = tanf(r_refdef.basefov * halfDegree) / tanf(za::clamp(vr_flat_model_fov.value, 10.f, 170.f) * halfDegree);
    f.numbers.y = nearest;
    f.numbers.z = za::max(nearest + 1.f, vr_flat_model_fov_far.value);
    glm::vec3 right, up;
    gfx::sceneCamera(f.eye, right, up);
    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    f.axis = {viewProjection[0][3], viewProjection[1][3], viewProjection[2][3]}; // (the row that makes w: the view's depth)
    return f;
}

void traceCrouch(float eyeZ)
{
    CrouchTrace& t = crouchTrace;
    if(vr_flat_crouch_trace.value <= 0.f)
    {
        t = CrouchTrace{};
        return;
    }
    if(t.lastFrame == host_framecount)
    {
        return; // one sample a frame
    }
    if(t.left <= 0)
    {
        t = CrouchTrace{};
        t.left = static_cast<int>(vr_flat_crouch_trace.value);
    }
    t.lastFrame = host_framecount;
    const float down = za::clamp(vr_flat_crouch_drop.value, 0.f, 40.f);
    // The most the eye may move a frame: the drop over the crouch's time at the ease's steepest (1.5: smoothstep's).
    const float steepest = crouchSmooth() ? 1.5f : 1.f;
    const float allow = down * steepest * keyState.lastDt / za::clamp(vr_flat_crouch_time.value, 0.05f, 2.f);
    const float eyeStep = t.have ? eyeZ - t.eye : 0.f;
    const float handStep = t.have ? traceHandZ - t.hand : 0.f;
    if(t.have)
    {
        t.eyeStep = za::max(t.eyeStep, glm::abs(eyeStep));
        t.handStep = za::max(t.handStep, glm::abs(handStep));
        t.allowed = za::max(t.allowed, allow);
    }
    t.have = true;
    t.eye = eyeZ;
    t.hand = traceHandZ;
    ++t.frames;
    Con_Printf("flatcrouch: frame %d held %d ratio %.3f eye %.2f (%+.2f) hand %.2f (%+.2f) cap %.1f\n", t.frames,
        crouchHeld() ? 1 : 0, keyState.ratio, eyeZ, eyeStep, traceHandZ, handStep, qvr::hull::localEyeCap());
    if(--t.left > 0)
    {
        return;
    }
    // The verdict: the eye never stepped more than the ease allows (a hundredth over, for rounding), the hand never more
    // than twice it (the hand also takes the shoulders' and the weapon's crouch offsets, eased with the same share).
    const bool pass = t.eyeStep <= t.allowed + 0.01f && t.handStep <= 2.f * t.allowed + 0.01f;
    Con_Printf("flatcrouch: trace done: %d frames, eye step at most %.2f, hand step at most %.2f, the ease allows %.2f a "
               "frame: %s\n",
        t.frames, t.eyeStep, t.handStep, t.allowed, pass ? "PASS" : "FAIL");
    Cvar_SetValueQuick(&vr_flat_crouch_trace, 0.f);
    t = CrouchTrace{};
}

} // namespace qvr::flatplay

// view.c's V_CalcRefdef: a flatscreen player holding his crouch (Left Ctrl, +flatcrouch) sees from lower, the same
// vr_flat_crouch_drop the game poses his body with; and he sees from wherever vr_flat_cam_fwd / vr_flat_cam_up /
// vr_flat_cam_side put his eye -- forward, up and the view's own right, in the frame of the view this frame draws (its
// punch and idle bob included: the nudge is the view's, not the body's). This is the one place the mode moves the eye, so
// it is the one camera every aiming measurement reads: flatplay::crosshairRay takes the crosshair's ray from the origin
// set here and drawnOffset keeps the drawn hands on it, so the crosshair moves with the eye and the gun stays on the
// crosshair. His own origin, his collision box and the crouch's box are not touched (vr_hull.cpp, vr_server.cpp measure
// them from where he stands). Outside the mode, and in a chase view, the view is left exactly as it was.
// V_CalcRefdef: Quake's view bob (cl_bob) times this: sliding, vr_flat_slide_bob of it (slideBobScale); 1 outside the mode.
extern "C" float VR_FlatBobScale()
{
    return qvr::flatplay::active() ? qvr::flatplay::slideBobScale() : 1.f;
}

extern "C" void VR_FlatViewDrop(float* vieworg, const float* viewangles)
{
    if(!qvr::flatplay::active() || chase_active.value)
    {
        return;
    }
    const glm::vec3 nudge = qvr::flatplay::camShiftWorld(qvr::geom::vec(viewangles));
    vieworg[0] += nudge.x;
    vieworg[1] += nudge.y;
    vieworg[2] += nudge.z - qvr::flatplay::crouchDrop();
    qvr::flatplay::traceCrouch(vieworg[2]);
    qvr::flatplay::traceViewOrg = {vieworg[0], vieworg[1], vieworg[2]};
}

// V_CalcRefdef, after VR_FlatViewDrop: dead, the view in the ragdoll's head as the headset's is (vr_death_view 2 or Vomit
// Mode; vr_deathview.cpp). Not in a chase view.
extern "C" void VR_FlatDeathView(float* vieworg, float* viewangles)
{
    if(!qvr::flatplay::active() || chase_active.value)
    {
        return;
    }
    glm::vec3 origin = qvr::geom::vec(vieworg), angles = qvr::geom::vec(viewangles);
    qvr::deathview::flatView(origin, angles);
    for(int i = 0; i < 3; i++)
    {
        vieworg[i] = origin[i];
        viewangles[i] = angles[i];
    }
}

// svc_setangle (cl_parse.c, before VR_OnSetAngle takes the same message): the angle of a walk through a seamless teleporter
// (hands::portalCrossing). A headset's view is its own and only the play space turns; a flat-screen player's view IS this
// angle, and the server's has the pitch level and the yaw of a move a tick old: the look jumped at every gate. His view goes
// on from what it was, turned by the gate alone. The turn is not his, so the turn's lag does not take it either: the drawn
// gun stays where it was in the view. A plain teleporter, a level's start and a respawn have no crossing: set as sent.
extern "C" void VR_FlatPortalAngle(const float* before, float* angles)
{
    float turn = 0.f;
    if(!qvr::flatplay::active() || !qvr::hands::portalAnglePending(turn))
    {
        return;
    }
    angles[PITCH] = before[PITCH];
    angles[YAW] = za::remainder(before[YAW] + turn, 360.f);
    angles[ROLL] = before[ROLL];
    // (His velocity is turned with him, over a tick as the client lerps it: no kick of the sway from that either.)
    qvr::flatplay::feel.view.y = za::remainder(qvr::flatplay::feel.view.y + turn, 360.f);
    qvr::flatplay::feel.noKickUntil = vr_gametime + qvr::flatplay::portalNoKickTime;
}

// V_CalcRefdef, after it: vr_flat_orbit's camera, off him: looking at his own pose from `yaw` degrees round the view, `dist`
// units off (Debug > Reports). A drawn view that far from his head is not his own eyes (viewIsHisOwn): the hands, the gun and
// the crosshair's ray stay as his head has them, so what is drawn is the pose he plays with.
extern "C" void VR_FlatOrbitView(float* vieworg, float* viewangles)
{
    if(!qvr::flatplay::active() || chase_active.value || qvr::flatplay::orbit.dist <= 0.f)
    {
        return;
    }
    const float yaw = viewangles[1] + qvr::flatplay::orbit.yaw;
    const glm::vec3 fwd = qvr::hands::forward({0.f, yaw, 0.f});
    for(int i = 0; i < 3; i++)
    {
        vieworg[i] += fwd[i] * qvr::flatplay::orbit.dist;
    }
    vieworg[2] -= qvr::flatplay::orbit.down;
    viewangles[0] = 0.f;
    viewangles[1] = yaw + 180.f;
    viewangles[2] = 0.f;
}

// Quake/host.c's Host_WriteConfigurationToFile, before Key_WriteBindings: flatscreen play's Ctrl bind given back so the
// config on disk stays the plain game's (Ctrl +attack). Called whatever asked for the write -- quitting, the shutdown, the
// menu closing with a setting changed, writeconfig -- and while the mode is not holding Ctrl it does nothing.
extern "C" void VR_FlatKeysForConfigWrite(void)
{
    qvr::flatplay::keysForConfigWrite();
}

// Quake/cl_main.c's CL_AccumulateCmd, once the frame's looking has all landed in cl.viewangles: the flatscreen look's
// window (vr_flat_pitch_up above, vr_flat_pitch_down below; Quake's pitch is positive looking down). Outside the mode the
// angle comes back as it was -- plain play and a headset keep cl_maxpitch / cl_minpitch, their own setting.
extern "C" float VR_FlatPitchClamp(float pitch)
{
    return qvr::flatplay::pitchClamp(pitch);
}

// vr_units.hpp: the mode's fixed body height (vr_flat_body_height), whatever the headset's Height says; 0 outside it.
float qvr::units::flatEyeHeight()
{
    return qvr::flatplay::active() ? za::clamp(vr_flat_body_height.value, 1.f, 2.2f) : 0.f;
}
