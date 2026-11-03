// vr_flatplay.cpp -- flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen, with the hands posed by
// the game instead of a headset (docs/vr-port/FLATPLAY.md).
//
// What the mode does, all of it behind the flag:
//   - the flat player's hands are marked as tracked (vr_client.cpp), which is what lets the game code run its VR
//     gameplay for him at all;
//   - the hands are posed from the view angles, the weapon in each hand and the player's own movement: they hold a gun
//     the way a first-person viewmodel does, aim at the crosshair, trail a turn, bob on a walk and breathe at rest
//     (poseHands, called from vr_hands.cpp in place of the straight-out-in-front pose);
//   - E force-grabs what the view is pointed at, V punches with the right hand, and either hand's fire button punches
//     with a weapon a swing strikes with: all of it goes through the game's own force grab and melee, a punch being a
//     real motion of the hand, so the melee rules decide whether it lands (vr_flat_punch drives either hand's punch from
//     the console);
//   - 1 and 2 cycle the weapons collected, into the left and the right hand (QC weapons.qc, on the flag);
//   - reloading is off, the flashlight is worn on the head and always lit, and the body presses the buttons it walks into.
//
// The keys are read from the keyboard state, not from bindings: nothing is rebound and no config changes. The keys Quake
// already uses (1 and 2, its own weapon select) are answered by the game code only while the flag is on.
//
// -flatscreenmode implies no VR backend (vr_main.cpp's startBackend): the mode runs only while no headset session is up,
// so the flag is what asks for no backend at all, and noteBackendImplied says so once.

#include "vr_flatplay.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_client.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_protocol.hpp"
#include "vr_ragtool.hpp"
#include "vr_units.hpp"
#include "vr_weapon_ids.h"
#include "vr_weapons.hpp"

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"

extern "C" qboolean keydown[MAX_KEYS]; // keys.c

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

    bool keyGrab{false};         // E, last frame (press edges)
    bool keyV{false};            // V, last frame
    bool keyFire[2]{false, false}; // each hand's fire button, last frame: LMB / +attack the main hand,
                                   // RMB / +offhandattack the off hand (vr_client.cpp's attacking)
    int punchFrame[2]{-1, -1};   // the frame each hand's punch started (two hands punching at once)
    double punchAt[2]{-1e9, -1e9}; // per hand: the time its punch started (vr_gametime)
    double grabAt{-1e9};         // E: the time the reach started
    int grabs{0};                // vr_flatplay_status's counts
    int punches[2]{0, 0};        // and the punch's, per hand

    // The game's pose (poseHands): each hand's place off its shoulder in the view's own frame (forward, right, up), eased
    // from one pose to the next (a weapon taken, a punch). In the view's frame, so the hands stay locked to the view.
    glm::vec3 local[2]{glm::vec3{0.f}, glm::vec3{0.f}};
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
    glm::vec2 lag{0.f};          // the turn's lag (degrees, before vr_flat_turnlag) and its rate: a critically damped
    glm::vec2 lagRate{0.f};      // spring pulling the hands back onto the view
    glm::vec3 vel{0.f};          // the player's velocity last frame (world)
    glm::vec3 sway{0.f};         // the step's sway (the view's frame: forward, right, up; units) and its rate: a spring
    glm::vec3 swayRate{0.f};     // kicked by each change of the player's velocity (starting, stopping, a jump, a landing)
    float phase{0.f};            // the walk's cycle
    float amp{0.f};              // how much of a walk's bob (eased with the speed, none in the air)
    float breath{0.f};
};

Feel feel;

// The pose, measured from the shoulders (hands::bodyAnchor with vr_shoulder_offset, the same anchor the body's own
// holsters are placed at): the arms stay on the body whatever the height calibration. Forward, side and down in the
// view's own frame, Quake units.
constexpr float emptyFwd = 5.f;    // an empty hand: a little in front of its shoulder...
constexpr float emptyDown = 13.f;  // ... and down at its side, where a relaxed hand hangs
constexpr float emptyOut = 7.f;
constexpr float gunFwd = 17.f;     // a held weapon: out in front of the chest, on the centre line
constexpr float gunDown = 9.f;
constexpr float gunOut = 2.5f;
constexpr float supportFwd = 12.f; // the free hand next to the other hand's gun: ready under it
constexpr float supportDown = 11.f;
constexpr float supportOut = 5.f;

// The turn's lag (vr_flat_turnlag, vr_flat_turnlag_speed): the drawn hands keep their direction as the view turns and a
// critically damped spring (this stiff, rad/s) brings them back; this much of what it leaves shown, never more than
// maxLagDeg (softly). A turn of 180 degrees a second leaves them ~2 degrees behind, back within a quarter second.
constexpr float lagStiffness = 18.f;
constexpr float lagShown = 0.12f;
constexpr float maxLagDeg = 4.f;
constexpr float lagSnapDeg = 45.f; // a view turned more than this in a frame was set (a teleporter, a respawn): no lag

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
constexpr float bobRoll = 0.8f;
constexpr float bobYaw = 0.3f;
constexpr float bobEase = 6.f;
constexpr float breathRate = 1.6f; // rad/s
constexpr float breathUp = 0.2f;
constexpr float breathSide = 0.1f;
constexpr float breathPitch = 0.15f;

// The camera the hands are kept on: the drawn view, unless it is somewhere else (the chase camera, the ragdoll tool's
// orbit): then the pose's own head.
constexpr float camNear = 24.f;

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

// E's reach: the collecting (left) hand goes out towards what the view is on.
constexpr float grabTime = 0.30f;
constexpr float grabReach = 12.f;

// Easing from one pose to the next (a weapon taken, a punch, a reach), in the view's frame.
constexpr float handEase = 15.f;

// V's punch and E's reach last this long after the press (vr_flatplay_status).
constexpr float punchTotal = punchWind + punchStrike + punchDwell + punchBack;

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

// Where a shoulder is (the drawn body's).
[[nodiscard]] glm::vec3 shoulder(const hands::State& s, int h)
{
    const glm::vec3 off{vr_shoulder_offset_x.value, vr_shoulder_offset_y.value, vr_shoulder_offset_z.value};
    return h == HAND_OFF ? hands::bodyAnchor(s, {-off.x, -off.y, off.z}) : hands::bodyAnchor(s, off);
}

// The weapon `h` holds, as its settings' slot; -1 for an empty hand.
[[nodiscard]] int heldIn(int h)
{
    const int slot = weapons::heldSlot(h);
    return slot >= 0 && slot != weapons::fistSlot() ? slot : -1;
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

// Where a hand sits relative to its shoulder, in the view's frame.
[[nodiscard]] glm::vec3 targetOffset(int h, float reach, bool otherHolds)
{
    const float side = h == HAND_OFF ? -1.f : 1.f;
    if(heldIn(h) >= 0)
    {
        return {gunFwd * reach, side * gunOut * reach, -gunDown * reach};
    }
    if(otherHolds)
    {
        return {supportFwd * reach, side * supportOut * reach, -supportDown * reach};
    }
    return {emptyFwd * reach, side * emptyOut * reach, -emptyDown * reach};
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
[[nodiscard]] glm::vec3 punchSwing(int h, const glm::vec3& rest)
{
    const double t = vr_gametime - fs.punchAt[h];   // the press's own clock: seconds since the punch was started. Each
                                                    // phase's start is on that same clock, so a phase eased from its
                                                    // start actually moves: given the press's absolute time instead, the
                                                    // easing never got past its first step and the whole punch arrived as
                                                    // three snaps between its poses (measured: 7, 16 and 22 units in a
                                                    // frame - longer than the 3 units a blow has to find in its sweep).
    if(t < 0.0 || t > punchTotal)
    {
        return rest;
    }
    const float span = za::max(4.f, armSpan());
    const glm::vec2 plane{rest.x, rest.z};                                     // the punch's plane: the view's forward, its up and down
    const float r0 = za::max(0.5f, glm::length(plane));
    const float wound_share = za::min(r0, punchFold) / r0;                     // folded along its own line, to punchFold out
    const glm::vec3 wound{plane.x * wound_share, rest.y, plane.y * wound_share - punchWindBack};
    const glm::vec2 throwDir{plane.x, plane.y * punchLevel};                   // thrown along the aim, kept near its own height
    const float hit_share = za::min(span, r0 + punchReach) / za::max(0.05f, glm::length(throwDir));
    const glm::vec3 thrown{plane.x * hit_share, rest.y, plane.y * punchLevel * hit_share};
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

[[nodiscard]] bool punching(int h)
{
    const double t = vr_gametime - fs.punchAt[h];
    return t >= 0.0 && t <= punchTotal;
}

// A hand's punch, from V, from its fire button, or from vr_flat_punch. Each hand has its own clock; a hand that started
// one this very frame keeps it, and the other hand joins in only when both hold a weapon a swing strikes with — so the
// main hand's punch is what it has always been.
void startPunch(int h, const char* from)
{
    if(fs.punchFrame[1 - h] == host_framecount && !(punchesWith(HAND_OFF) && punchesWith(HAND_MAIN)))
    {
        return;
    }
    fs.punchAt[h] = vr_gametime;
    fs.punchFrame[h] = host_framecount;
    fs.punches[h]++;
    if(vr_flatplay_debug.value)
    {
        Con_Printf("flatplay: %s punch (%s): %s\n", h == HAND_MAIN ? "main" : "off", from,
            punchesWith(h) ? "melee" : "not a swinging weapon");
    }
}

// E's reach with the collecting hand.
[[nodiscard]] glm::vec3 grabOffset()
{
    const double t = vr_gametime - fs.grabAt;
    if(t < 0.0 || t > grabTime)
    {
        return glm::vec3{0.f};
    }
    const float out = t < grabTime * 0.45f ? slide(t, 0.0, grabTime * 0.45f)
                                           : 1.f - slide(t, grabTime * 0.45f, grabTime * 0.55f);
    return {grabReach * out, 0.f, -2.f * out};
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
        const glm::quat turn = glm::angleAxis(ang, glm::normalize(axis));
        rot = hands::anglesFromVectors(glm::normalize(turn * f), glm::normalize(turn * u));
    }
    return rot;
}

// The keys, read from the keyboard state: nothing is rebound, and E and V only while the game has the keyboard (the
// console takes it). The hands' fire buttons come from the same source the game's fire uses (vr_client.cpp's attacking:
// its +attack / +offhandattack state, which is what the mouse buttons drive), so a bound key, the mouse and a `+attack`
// said on the console are the same press.
void pollKeys()
{
    const bool inGame = key_dest == key_game;
    const bool grab = inGame && keydown['e'] != 0;
    const bool v = inGame && keydown['v'] != 0;
    bool fire[2];
    fire[HAND_OFF] = client::attacking(HAND_OFF);
    fire[HAND_MAIN] = client::attacking(HAND_MAIN);

    if(grab && !fs.keyGrab)
    {
        fs.grabAt = vr_gametime;
        fs.grabs++;
        Cbuf_AddText("impulse 46\n"); // QC vr_wpnforcegrab.qc: the force grab on what the view is pointed at
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: E: force grab\n");
        }
    }
    if(v && !fs.keyV)
    {
        startPunch(HAND_MAIN, "V"); // as it has always been: V punches with the main hand
    }
    for(int h = 0; h < HAND_COUNT; h++)
    {
        // The fire button punches only with a weapon a swing strikes with; a gun's trigger fires the gun as it does now.
        if(fire[h] && !fs.keyFire[h] && punchesWith(h))
        {
            startPunch(h, h == HAND_MAIN ? "the main hand's fire button" : "the off hand's fire button");
        }
    }
    fs.keyGrab = grab;
    fs.keyV = v;
    fs.keyFire[HAND_OFF] = fire[HAND_OFF];
    fs.keyFire[HAND_MAIN] = fire[HAND_MAIN];
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

    // The turn's lag: the hands keep their direction as the view turns, the spring brings them back.
    glm::vec2 turned{za::remainder(view.x - feel.view.x, 360.f), za::remainder(view.y - feel.view.y, 360.f)};
    feel.view = view;
    if(za::fabs(turned.x) > lagSnapDeg || za::fabs(turned.y) > lagSnapDeg)
    {
        turned = glm::vec2{0.f}; // set, not turned
        feel.lag = feel.lagRate = glm::vec2{0.f};
    }
    if(vr_flat_turnlag.value > 0.f)
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
    const glm::vec3 dv = vel - feel.vel;
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
    const float ampTarget = cl.onground && !noclip_anglehack ? za::clamp(speed / runSpeed, 0.f, 1.3f) : 0.f;
    feel.amp += (ampTarget - feel.amp) * ease(bobEase, dt);
    if(feel.amp > 1e-3f)
    {
        feel.phase = za::remainder(feel.phase + dt * 2.f * glm::pi<float>() * bobRate * za::clamp(speed / runSpeed, 0.35f, 1.3f),
            4.f * glm::pi<float>());
    }
    feel.breath = za::remainder(feel.breath + dt * breathRate, 4.f * glm::pi<float>());
}

} // namespace

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

void init()
{
    fs.flag = COM_CheckParm("-flatscreenmode") != 0 || ragtool::flagGiven(); // (-ragtool: the ragdoll tool, flat)
    Cmd_AddCommand("vr_flatplay_status", status_f);
    Cmd_AddCommand("vr_flat_punch", punch_f);
}

void frame()
{
    const bool on = active();
    Cvar_SetValue("vr_flatplay", on ? 1.f : 0.f);

    if(on && !fs.announced)
    {
        fs.announced = true;
        Con_Printf("Quake VR: flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen. E grabs what you "
                   "look at, 1 and 2 cycle the weapons collected (and the empty hand) into the left and right hand, V punches, and either "
                   "hand's fire button punches with a weapon a swing strikes with (with a gun it fires the gun, as in VR). "
                   "vr_flatplay_status says what the mode is doing.\n");
    }
    if(!on && fs.wasOn)
    {
        Con_Warning("Quake VR: a headset took the game back: flatscreen play is off.\n");
    }
    fs.wasOn = on;

    if(!on)
    {
        fs.localSeen[0] = fs.localSeen[1] = false;
        fs.mountSeen[0] = fs.mountSeen[1] = false;
        fs.probeTime[0] = fs.probeTime[1] = -1e9;
        fs.lastTime = -1e9;
        feel = Feel{};
        return;
    }

    pollKeys();
}

void poseHands(hands::State& s, const glm::vec3& aim)
{
    // Made at the frame's start, again for the move and again for the view (hands::relatch): the same frame's poses
    // differ only by the view's turn and the player's move since. The easing steps once a frame.
    double dt = fs.lastTime > -1e8 ? vr_gametime - fs.lastTime : 0.0;
    fs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);

    s.bodyYaw = aim.y; // the flat branch turns the body to the view the same way

    const float reach = za::clamp(vr_flat_reach.value, 0.25f, 3.f);

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

    for(int h = 0; h < HAND_COUNT; h++)
    {
        // Where the hand sits off its shoulder, in the view's frame: locked to the view. The looks (the sway, the bob, a
        // turn's lag) are the drawn hands' only (drawnOffset): the game's hands, and the shots, stay on the crosshair.
        const glm::vec3 rest = targetOffset(h, reach, h == HAND_OFF ? mainHolds : offHolds) +
            (h == HAND_OFF ? grabOffset() : glm::vec3{0.f});
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
        const glm::vec3 off = fs.local[h] + (punchSwing(h, rest) - rest);
        s.pos[h] = shoulder(s, h) + fwd * off.x + right * off.y + up * off.z;
        s.rot[h] = aimRotAtCrosshair(aim, h);
        s.rot[h].z += vr_fakevr_handroll.value;
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
    if(!punching(h) && glm::distance(joint, drawn) < span)
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

void drawnOffset(const hands::State& s, int h, glm::vec3& pos, glm::vec3& angles)
{
    pos = angles = glm::vec3{0.f};
    if(!active() || !s.valid)
    {
        return;
    }
    stepFeel();

    // The pose's frame (its head, the view's angles it was made with) and the drawn view's (its bob, roll and kick, its
    // own easing up a step): the hands moved from one onto the other, so they are locked to what is drawn.
    const glm::vec3 aim{cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    const glm::vec3 cam{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const bool onCam = !chase_active.value && glm::length(cam - s.head) < camNear;
    const glm::vec3 from = s.head;
    const glm::vec3 to = onCam ? cam : s.head;
    const glm::vec3 camAngles = onCam ? geom::vec(r_refdef.viewangles)
                                      : aim;

    // The turn's lag, shown (softly capped).
    const float lagAmount = za::clamp(vr_flat_turnlag.value, 0.f, 3.f);
    const float lagCap = maxLagDeg * lagAmount;

    const glm::vec2 lag{softCap(feel.lag.x * lagShown * lagAmount, lagCap), softCap(feel.lag.y * lagShown * lagAmount, lagCap)};

    const glm::mat3 poseAxes = axes(aim);
    const glm::mat3 drawnAxes = axes(camAngles + glm::vec3{lag.x, lag.y, 0.f});
    const glm::mat3 camAxes = axes(camAngles);
    const glm::mat3 turn = drawnAxes * glm::transpose(poseAxes);

    // The step's sway and the walk's bob, in the view's frame.
    const float bob = za::clamp(vr_flat_bob.value, 0.f, 3.f);
    const float rest = 1.f - za::clamp(feel.amp, 0.f, 1.f);
    const glm::vec3 bobLocal{0.f, bob * (bobSide * feel.amp * za::sin(feel.phase) + breathSide * rest * za::sin(feel.breath * 0.5f)),
        bob * (bobUp * feel.amp * (za::cos(2.f * feel.phase) - 1.f) * 0.5f + breathUp * rest * za::sin(feel.breath))};
    const glm::vec3 local = feel.sway + bobLocal;

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
        avatar::Skeleton k;
        const glm::vec3 root = avatar::skeleton(k) ? k.shoulder[h] : shoulder(s, h);
        const float span = armSpan() * hold;
        const glm::vec3 reachOut = drawn - root;
        const float d = glm::length(reachOut);
        if(span > 0.f && d > span)
        {
            drawn = root + reachOut * (span / d);
        }
    }

    pos = drawn - s.pos[h];

    const glm::mat3 handAxes = turn * axes(s.rot[h]);
    const glm::vec3 drawnRot = hands::anglesFromVectors(handAxes[0], handAxes[2]);
    for(int i = 0; i < 3; i++)
    {
        angles[i] = za::remainder(drawnRot[i] - s.rot[h][i], 360.f);
    }
    angles.x += bob * breathPitch * rest * za::sin(feel.breath);
    angles.y += bob * bobYaw * feel.amp * za::sin(feel.phase);
    angles.z += bob * bobRoll * feel.amp * za::sin(feel.phase);

    probeArm(s, h, drawn, drawnRot);
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
}

void printDrawn(const float* origin, const float* angles)
{
    // The drawn weapon in the camera's frame (forward, right, up), its angles less the camera's, and what the view does.
    glm::vec3 f, r, u;
    const glm::vec3 cam{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const glm::vec3 camAngles{r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]};
    hands::angleVectors(camAngles, f, r, u);
    const glm::vec3 d = geom::vec(origin) - cam;
    const float dyaw = za::remainder(angles[1] - camAngles.y, 360.f);
    Con_Printf("flatview: t %.4f local %.3f %.3f %.3f yaw %.3f pitch %.3f view %.2f %.2f speed %.0f bob %.2f ground %d "
               "lag %.2f\n",
        vr_gametime,
        glm::dot(d, f), glm::dot(d, r), glm::dot(d, u), dyaw, angles[0] + camAngles.x, camAngles.x, camAngles.y,
        glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]}), feel.amp, cl.onground ? 1 : 0, feel.lag.y);
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
    Con_Printf("  E pressed %d times (force grab); punches: the main hand %d, the off hand %d. E and V are read from the "
               "keyboard, not from bindings; each hand's punch starts from V, from its fire button with a weapon a swing "
               "strikes with (LMB / +attack the main hand, RMB / +offhandattack the off hand), or from vr_flat_punch.\n",
        fs.grabs, fs.punches[HAND_MAIN], fs.punches[HAND_OFF]);
    Con_Printf("  the game code: reloading off, the flashlight worn and always lit, body interactions on, 1, 2 and the wheel cycle (empty hand included) "
               "the weapons collected into the left and right hand.\n");
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
    if(!punchesWith(h))
    {
        Con_Printf("vr_flat_punch: the %s hand holds weapon %d, which its trigger does not strike with: no punch.\n",
            h == HAND_MAIN ? "main" : "off", heldWid(h));
        return;
    }
    startPunch(h, "vr_flat_punch");
}

} // namespace qvr::flatplay
