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
#include "vr_weapons.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

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

    glm::vec3 aim{0.f};          // the aim the hands are built from
    bool aimSeen{false};
    float yawRate{0.f};          // degrees a second the view turns: the arms trail it
    glm::vec3 vel{0.f};          // the player's own velocity (world units/s)
    glm::vec3 origin{0.f};
    bool originSeen{false};
    float walk{0.f};             // the walk cycle's phase
    float breath{0.f};
    glm::vec3 hand[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the eased hand positions
    bool handSeen[2]{false, false};
    double lastTime{-1e9};
};

FlatState fs;

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

// The arms trail the view: this much of a turn, this much of a step, never more than this.
constexpr float turnLag = 0.075f;
constexpr float moveLag = 0.035f;
constexpr float maxTrail = 14.f; // units a hand is ever left behind

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

// Easing. A punch and a reach are not eased: their own curves are smooth, and easing them would take away the speed the
// game's melee rules need.
constexpr float handEase = 15.f;

// V's punch and E's reach last this long after the press (vr_flatplay_status).
constexpr float punchTotal = punchWind + punchStrike + punchBack;

[[nodiscard]] float ease(float k, float dt)
{
    return dt > 0.f ? 1.f - za::exp(-k * dt) : 1.f;
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
    fs.flag = COM_CheckParm("-flatscreenmode") != 0;
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
        fs.aimSeen = fs.originSeen = false;
        fs.handSeen[0] = fs.handSeen[1] = false;
        fs.lastTime = -1e9;
        return;
    }

    pollKeys();
}

void poseHands(hands::State& s, const glm::vec3& aim)
{
    double dt = fs.lastTime > -1e8 ? vr_gametime - fs.lastTime : 0.0;
    fs.lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);

    s.bodyYaw = aim.y; // the flat branch turns the body to the view the same way

    const float reach = za::clamp(vr_flat_reach.value, 0.25f, 3.f);
    const float sway = za::clamp(vr_flat_sway.value, 0.f, 2.f);
    const float bob = za::clamp(vr_flat_bob.value, 0.f, 2.f);

    // The aim the hands are built from, and how fast it turns.
    if(!fs.aimSeen)
    {
        fs.aim = aim;
        fs.aimSeen = true;
    }
    float dyaw = aim.y - fs.aim.y;
    while(dyaw > 180.f)
    {
        dyaw -= 360.f;
    }
    while(dyaw < -180.f)
    {
        dyaw += 360.f;
    }
    fs.yawRate = dt > 0.0 ? fs.yawRate * 0.7f + (dyaw / static_cast<float>(dt)) * 0.3f : 0.f;
    fs.aim = aim;

    // The player's own motion.
    if(!fs.originSeen)
    {
        fs.origin = s.playerOrigin;
        fs.originSeen = true;
    }
    const glm::vec3 vel = dt > 0.0 ? (s.playerOrigin - fs.origin) / static_cast<float>(dt) : glm::vec3{0.f};
    fs.vel = fs.vel * 0.8f + vel * 0.2f;
    fs.origin = s.playerOrigin;

    // Walking and breathing.
    const float speed = glm::length(glm::vec2{fs.vel.x, fs.vel.y});
    fs.walk = cl.onground && speed > 30.f ? fs.walk + static_cast<float>(dt) * za::clamp(speed / 120.f, 0.2f, 2.f) * 6.f
                                          : 0.f;
    fs.breath += static_cast<float>(dt) * 1.6f;

    glm::vec3 fwd, right, up;
    hands::angleVectors(fs.aim, fwd, right, up);

    // How far the arms are left behind by a turn and by a step.
    // (A step's: back along his own horizontal motion, whatever way he faces: walking forward leaves the guns behind,
    // under the crosshair, strafing beside it. The whole never more than maxTrail.)
    const float trail = za::clamp(fs.yawRate * turnLag, -maxTrail, maxTrail) * sway;
    glm::vec3 lag = right * -trail - glm::vec3{fs.vel.x, fs.vel.y, 0.f} * (moveLag * sway);
    if(const float lagLen = glm::length(lag); lagLen > maxTrail)
    {
        lag *= maxTrail / lagLen;
    }

    const bool mainHolds = heldIn(HAND_MAIN) >= 0;
    const bool offHolds = heldIn(HAND_OFF) >= 0;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 off = targetOffset(h, reach, h == HAND_OFF ? mainHolds : offHolds);

        // The hands' own motion: a walk's bob, and breathing. Small, and never in the way.
        const float side = h == HAND_OFF ? -1.f : 1.f;
        off.z -= 1.4f * bob * za::sin(fs.walk);
        off.y += side * 1.1f * bob * za::sin(fs.walk * 0.5f);
        off.z += 0.35f * bob * za::sin(fs.breath);
        off.x += 0.25f * bob * za::sin(fs.breath * 0.5f);

        off += h == HAND_MAIN ? punchOffset() : grabOffset();

        const glm::vec3 target = shoulder(s, h) + fwd * off.x + right * off.y + up * off.z + lag;

        if(!fs.handSeen[h])
        {
            fs.hand[h] = target;
            fs.handSeen[h] = true;
        }
        else
        {
            fs.hand[h] += (target - fs.hand[h]) * ease(handEase, static_cast<float>(dt));
        }

        s.pos[h] = fs.hand[h];
        s.rot[h] = aimRotAtCrosshair(fs.aim, h);
        s.rot[h].z += vr_fakevr_handroll.value;
    }

    if(vr_flatplay_debug.value && host_framecount % 30 == 0)
    {
        const glm::vec3 a = s.pos[HAND_OFF] - s.head;
        const glm::vec3 b = s.pos[HAND_MAIN] - s.head;
        Con_Printf("flatplay: aim %.1f %.1f turn %.0f deg/s speed %.0f punch %s grab %d hands: off (%.1f, %.1f, %.1f) "
                   "main (%.1f, %.1f, %.1f) from the head\n",
            fs.aim.x, fs.aim.y, fs.yawRate, speed, punching() ? "running" : "no", fs.grabs, a.x, a.y, a.z, b.x, b.y,
            b.z);
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
    Con_Printf("  vr_flatplay %g (the game code's gate), vr_flat_reach %g, vr_flat_sway %g, vr_flat_bob %g\n",
        vr_flatplay.value, vr_flat_reach.value, vr_flat_sway.value, vr_flat_bob.value);
    Con_Printf("  E pressed %d times (force grab), V %d (punch); E and V are read from the keyboard, not from bindings.\n",
        fs.grabs, fs.punches);
    Con_Printf("  the game code: reloading off, the flashlight worn and always lit, body interactions on, 1, 2 and the wheel cycle (empty hand included) "
               "the weapons collected into the left and right hand.\n");
}

} // namespace qvr::flatplay
