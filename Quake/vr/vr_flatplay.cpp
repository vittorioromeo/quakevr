// vr_flatplay.cpp -- flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen, with the hands posed by
// the game instead of a headset (docs/vr-port/FLATPLAY.md).
//
// What the mode does, all of it behind the flag:
//   - the flat player's hands are marked as tracked (vr_client.cpp), which is what lets the game code run its VR
//     gameplay for him at all;
//   - the hands are posed from the view angles, the weapon in each hand and the player's own movement: they hold a gun
//     the way a first-person viewmodel does, aim at the crosshair, trail a turn, bob on a walk and breathe at rest
//     (poseHands, called from vr_hands.cpp in place of the straight-out-in-front pose);
//   - E force-grabs what the view is pointed at, V punches with the right hand: both go through the game's own force
//     grab and melee, a punch being a real motion of the hand, so the melee rules decide whether it lands;
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
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_ragtool.hpp"
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
    bool keyMelee{false};        // V, last frame
    double punchAt{-1e9};        // V: the time the punch started (vr_gametime)
    double grabAt{-1e9};         // E: the time the reach started
    int grabs{0};                // vr_flatplay_status's counts
    int punches{0};

    // The game's pose (poseHands): each hand's place off its shoulder in the view's own frame (forward, right, up), eased
    // from one pose to the next (a weapon taken, a punch). In the view's frame, so the hands stay locked to the view.
    glm::vec3 local[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool localSeen[2]{false, false};
    double lastTime{-1e9};
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

// V's punch, seconds from the press: wound up, thrown, drawn back.
constexpr float punchWind = 0.13f;
constexpr float punchStrike = 0.12f;
constexpr float punchBack = 0.22f;
constexpr float punchReach = 24.f;   // units the fist goes forward in the strike
constexpr float punchWindUp = 5.f;   // and how it rises on the way back
constexpr float punchWindBack = 7.f;

// E's reach: the collecting (left) hand goes out towards what the view is on.
constexpr float grabTime = 0.30f;
constexpr float grabReach = 12.f;

// Easing from one pose to the next (a weapon taken, a punch, a reach), in the view's frame.
constexpr float handEase = 15.f;

// V's punch and E's reach last this long after the press (vr_flatplay_status).
constexpr float punchTotal = punchWind + punchStrike + punchBack;

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

// V's punch, as an offset along the aim (x) and up (z).
[[nodiscard]] glm::vec3 punchOffset()
{
    const double t = vr_gametime - fs.punchAt;
    if(t < 0.0 || t > punchTotal)
    {
        return glm::vec3{0.f};
    }
    if(t < punchWind)
    {
        const float u = slide(t, fs.punchAt, punchWind);
        return {-punchWindBack * u, 0.f, punchWindUp * u};
    }
    if(t < punchWind + punchStrike)
    {
        const float u = slide(t, fs.punchAt + punchWind, punchStrike);
        return {punchReach * u - punchWindBack * (1.f - u), 0.f, punchWindUp * (1.f - u)};
    }
    const float u = slide(t, fs.punchAt + punchWind + punchStrike, punchBack);
    return {punchReach * (1.f - u), 0.f, 0.f};
}

[[nodiscard]] bool punching()
{
    const double t = vr_gametime - fs.punchAt;
    return t >= 0.0 && t <= punchTotal;
}

// E's reach with the collecting hand.
[[nodiscard]] glm::vec3 grabOffset()
{
    const double t = vr_gametime - fs.grabAt;
    if(t < 0.0 || t > grabTime)
    {
        return glm::vec3{0.f};
    }
    const float out = t < grabTime * 0.45f ? slide(t, fs.grabAt, grabTime * 0.45f)
                                           : 1.f - slide(t, fs.grabAt + grabTime * 0.45f, grabTime * 0.55f);
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

// The keys, read from the keyboard state: nothing is rebound, and only while the game has the keys.
void pollKeys()
{
    const bool inGame = key_dest == key_game;
    const bool grab = inGame && keydown['e'] != 0;
    const bool melee = inGame && keydown['v'] != 0;

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
    if(melee && !fs.keyMelee)
    {
        fs.punchAt = vr_gametime;
        fs.punches++;
        if(vr_flatplay_debug.value)
        {
            Con_Printf("flatplay: V: punch\n");
        }
    }
    fs.keyGrab = grab;
    fs.keyMelee = melee;
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
}

void frame()
{
    const bool on = active();
    Cvar_SetValue("vr_flatplay", on ? 1.f : 0.f);

    if(on && !fs.announced)
    {
        fs.announced = true;
        Con_Printf("Quake VR: flatscreen play (-flatscreenmode): the VR gameplay on the desktop screen. E grabs what you "
                   "look at, 1 and 2 cycle the weapons you collected (and the empty hand) into the left and right hand, V punches, the mouse "
                   "fires the hand it fires in VR. vr_flatplay_status says what the mode is doing.\n");
    }
    if(!on && fs.wasOn)
    {
        Con_Warning("Quake VR: a headset took the game back: flatscreen play is off.\n");
    }
    fs.wasOn = on;

    if(!on)
    {
        fs.localSeen[0] = fs.localSeen[1] = false;
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

    glm::vec3 fwd, right, up;
    hands::angleVectors(aim, fwd, right, up);

    const bool mainHolds = heldIn(HAND_MAIN) >= 0;
    const bool offHolds = heldIn(HAND_OFF) >= 0;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        // Where the hand sits off its shoulder, in the view's frame: locked to the view. The looks (the sway, the bob, a
        // turn's lag) are the drawn hands' only (drawnOffset): the game's hands, and the shots, stay on the crosshair.
        const glm::vec3 target = targetOffset(h, reach, h == HAND_OFF ? mainHolds : offHolds) +
                                 (h == HAND_MAIN ? punchOffset() : grabOffset());
        if(!fs.localSeen[h])
        {
            fs.local[h] = target;
            fs.localSeen[h] = true;
        }
        else if(dt > 0.0)
        {
            fs.local[h] += (target - fs.local[h]) * ease(handEase, static_cast<float>(dt));
        }
        const glm::vec3& off = fs.local[h];

        s.pos[h] = shoulder(s, h) + fwd * off.x + right * off.y + up * off.z;
        s.rot[h] = aimRotAtCrosshair(aim, h);
        s.rot[h].z += vr_fakevr_handroll.value;
    }

    if(vr_flatplay_debug.value && host_framecount % 30 == 0 && dt > 0.0)
    {
        const glm::vec3 a = s.pos[HAND_OFF] - s.head;
        const glm::vec3 b = s.pos[HAND_MAIN] - s.head;
        const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
        Con_Printf("flatplay: aim %.1f %.1f speed %.0f punch %s grab %d hands: off (%.1f, %.1f, %.1f) "
                   "main (%.1f, %.1f, %.1f) from the head\n",
            aim.x, aim.y, speed, punching() ? "running" : "no", fs.grabs, a.x, a.y, a.z, b.x, b.y, b.z);
    }
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

    const glm::vec3 drawn = to + turn * (s.pos[h] - from) + camAxes * local;
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
               "(recovery %g)\n",
        vr_flatplay.value, vr_flat_reach.value, vr_flat_sway.value, vr_flat_bob.value, vr_flat_turnlag.value,
        vr_flat_turnlag_speed.value);
    Con_Printf("  E pressed %d times (force grab), V %d (punch); E and V are read from the keyboard, not from bindings.\n",
        fs.grabs, fs.punches);
    Con_Printf("  the game code: reloading off, the flashlight worn and always lit, body interactions on, 1, 2 and the wheel cycle (empty hand included) "
               "the weapons collected into the left and right hand.\n");
}

} // namespace qvr::flatplay
