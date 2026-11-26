// vr_flatgrenade.cpp -- flatscreen play's quick grenade and throw key as drawn (his words 2026-11-10, QC vr_flatgrenade.qc's
// header; his ask 2026-11-23, the throw key; docs/vr-port/FLATPLAY.md, "The quick grenade", "The throw key").
//
// The game decides (QC vr_flatgrenade.qc): G (vr_flat_key_grenade, sent here as impulse 57) picks the hand and runs the
// phases, which come back in STAT_QVR_FLATGREN (the phase + 8 for the main hand); J (vr_flat_key_throw: 39 down, 40 up)
// winds up and throws what a hand holds, its phase in the same stat (* 16, + 64 the main hand, + 128 both hands). This
// file draws them, each a share of the hand's way to a place off its shoulder, eased (smooth01) and laid over the pose in
// this order:
//   - hold: the grenade held out in view (vr_flat_grenade_hold_*, along the look), from the trip to the pouch until the
//     throw's follow-through is over; it eases back over vr_flat_throw_back_time;
//   - wind: the throw's wind-up above and beside the shoulder (vr_flat_throw_wind_*, along the look: it stays at the
//     screen's upper corner, his words 2026-11-23), the grenade's (its fire button or J held) and the throw key's alike,
//     in vr_flat_throw_wind_time, drawn further back as it charges (vr_flat_throw_charge_back); gone as the throw's
//     follow-through comes; the thing leaves from the hand there (QC VR_FlatThrow_From);
//   - guide: the other hand, if free (flatsight::handEmpty), forward with its palm away and its fingers open
//     (vr_flat_throw_guide_*) while the throwing hand winds up; back over vr_flat_throw_back_time;
//   - follow: the throw's follow-through (vr_flat_throw_follow_*, along the look), in vr_flat_throw_follow_time, then
//     back with the hold; a thing held in both hands is thrown by the one hand, the other letting go (letsGo);
//   - away: the trip to the pouch (vr_flat_grenade_fetch_*, at his side and a little behind, level with the body, out of
//     view), over vr_flat_grenade_fetch_time; back in view with the grenade over vr_flat_grenade_up_time.
// The pose made is the game's too (the server sees the hand where it is drawn, and the grenade follows it). While a hand
// is busy with it, it doesn't punch, and the off hand lets go of a weapon's grip (vr_flatsight.cpp, vr_twohand.cpp),
// taking it again as it eases back. vr_flat_grenade_debug 1 prints the steps (flatgren: ...); vr_flatgrenade_report the state.

#include "vr_flatgrenade.hpp"

#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN
#include "vr_box3d.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_seat.hpp"
#include "vr_flashlight.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"
extern "C" int Key_StringToKeynum(const char* str); // keys.c

namespace qvr::flatgrenade
{

namespace
{

// The game's phases (QC VR_FLATGREN_*).
constexpr int phaseFetch = 1;
constexpr int phaseHeld = 2; // (3: armed, held the same way)
constexpr int phaseArmed = 3;
constexpr int phaseWind = 4;
constexpr int phaseThrown = 5;

// The throw key's (QC VR_FLATTHROW_*: STAT_QVR_FLATGREN's bits 16 and 32; 64 the main hand's, 128 held in both hands;
// 256 off hand H-grabbed, 512 main hand H-grabbed, 1024 H-grabbed 2H prop or ragdoll hold pose).
constexpr int throwWind = 1;
constexpr int throwThrown = 2;
constexpr int throwPending = 3; // tap-drop vs hold-throw window (< vr_flat_throw_tap_time): no wind-up shown yet

// The throw key's impulses (QC weapons.qc): 39 the key down, 40 up. Grab / pull (H): 96 toggle, 97 hold down, 98 hold up.
constexpr int throwResendFrames = 8; // the key up while the server still winds up: 40 again this many frames on
constexpr int pullRetryFrames = 6;   // hold mode: retry grab impulse while key held and nothing grabbed yet

// Each hand's shares (0..1, before their ease).
struct Shares
{
    float away{0.f};
    float hold{0.f};
    float wind{0.f};
    float follow{0.f};
    float charge{0.f};    // the wind-up's charge (its share of the full wind-up): drawn further back
    float guide{0.f};     // the other hand, free, guiding the throw (forward, its palm away, open: vr_flat_throw_guide_*)
    float grabReach{0.f}; // Grab / Pull (H): brief forward reach-out animation
    float grabHold{0.f};  // Grab / Pull (H): comfortable forward hold-out pose while carrying a 2H crate or ragdoll
    float grabPulse{0.f}; // remaining seconds of the brief reach-out pulse
};

// The grenade, throw/drop and grab/pull as drawn (no function-local statics: a named file-scope struct, CODE_STYLE.md).
struct GrenadeState
{
    Shares hand[2];
    int phase{0};        // the server's, this frame
    int hand0{-1};       // its hand (kept after the phase ends, while the hand eases back)
    bool keyDown{false}; // G, last frame
    int printed{0};      // (vr_flat_grenade_debug) the places whose arrival was printed this grenade: bit per place
    int presses{0};
    double lastTime{-1e9};
    // The throw / drop key (B).
    int throwPhase{0};         // the server's, this frame
    int throwHand{-1};         // its hand (kept while it eases back)
    bool throwBoth{false};     // held in both hands as it wound up: the other hand let go (letsGo)
    bool throwKeyDown{false};
    bool throwKeyWound{false}; // B went down on a quick grenade: its wind-up is B's (QC .vr_flatgren_bykey)
    int throwPresses{0};
    int throwResend{0};        // frames to the next 40 while the key is up and the server still winds up
    double windStart{-1.0};    // (vr_gametime) when the shown wind-up began
    // The cancel (flatthrow2): the throwing hand's attack button pressed while B is held.
    int buttonsWas{0};    // the mouse buttons held last frame (1 the left, 2 the right)
    int swallow{0};       // the buttons that cancelled a throw, held since: they do nothing till let go
    int cancels{0};       // throws cancelled (impulse 41 sent on a fresh press)
    int cancelResend{0};  // frames to the next 41 while the server still winds up after one
    bool cancelSent{false}; // 41 sent and the server still aims since (B held): resent every cancelResend frames
    // The Grab / Pull key (H).
    bool grabHeld[2]{false, false}; // server reports this hand holds an H-grabbed prop or ragdoll
    bool grabHoldPose{false};       // server reports 2H prop or ragdoll held (extended hold-out pose)
    bool pullKeyDown{false};
    int pullPresses{0};
    int pullResend{0};              // frames to resend impulse 38 in hold mode if server still holds
    int pullRetry{0};               // frames to retry impulse 37 in hold mode while searching
};

// splitscreen (SPLITSCREEN.md, phase 4a): the current seat's (seat::Part::FlatGrenade)
[[nodiscard]] GrenadeState& gs()
{
    return seat::part<GrenadeState>(seat::current(), seat::Part::FlatGrenade);
}

[[nodiscard]] float secs(const Tunable& t)
{
    return za::clamp(t.value, 0.02f, 2.f);
}

[[nodiscard]] const char* handName(int h)
{
    return h == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] bool anyShare(const Shares& a)
{
    return a.away > 0.f || a.hold > 0.f || a.wind > 0.f || a.follow > 0.f || a.guide > 0.f || a.grabReach > 0.f || a.grabHold > 0.f;
}

// `v` towards `want` (0 or 1) at 1 / `up` a second rising and 1 / `down` falling.
void step(float& v, bool want, float up, float down, float dt)
{
    v = want ? za::min(1.f, v + dt / up) : za::max(0.f, v - dt / down);
}

void report_f()
{
    Con_Printf("flatgren: phase %d, hand %s; off: away %.2f hold %.2f wind %.2f follow %.2f; main: away %.2f hold %.2f wind %.2f "
               "follow %.2f; G presses %d\n",
        gs().phase, gs().hand0 < 0 ? "none" : handName(gs().hand0), gs().hand[HAND_OFF].away, gs().hand[HAND_OFF].hold,
        gs().hand[HAND_OFF].wind, gs().hand[HAND_OFF].follow, gs().hand[HAND_MAIN].away, gs().hand[HAND_MAIN].hold,
        gs().hand[HAND_MAIN].wind, gs().hand[HAND_MAIN].follow, gs().presses);
    Con_Printf("flatthrow: phase %d, hand %s%s; charge off %.2f main %.2f; B presses %d; cancels %d, swallowed buttons %d\n", gs().throwPhase,
        gs().throwHand < 0 ? "none" : handName(gs().throwHand), gs().throwBoth ? " (was in both hands: the other let go)" : "", gs().hand[HAND_OFF].charge,
        gs().hand[HAND_MAIN].charge, gs().throwPresses, gs().cancels, gs().swallow);
    Con_Printf("flatgrab: mode %s, held off %d main %d (holdPose %d); reach off %.2f main %.2f; hold off %.2f main %.2f; H presses %d\n",
        vr_flat_grab_hold.value != 0.f ? "hold" : "toggle", gs().grabHeld[HAND_OFF] ? 1 : 0, gs().grabHeld[HAND_MAIN] ? 1 : 0,
        gs().grabHoldPose ? 1 : 0, gs().hand[HAND_OFF].grabReach, gs().hand[HAND_MAIN].grabReach, gs().hand[HAND_OFF].grabHold,
        gs().hand[HAND_MAIN].grabHold, gs().pullPresses);
    // (A big prop dragged by its surface, a listen server's: each hand's prop and how far its held point is from the hand.)
    box3d::PropDragHold d[2];
    const bool dragOff = sv.active && box3d::propDragHold(cl.viewentity, HAND_OFF, d[HAND_OFF]);
    const bool dragMain = sv.active && box3d::propDragHold(cl.viewentity, HAND_MAIN, d[HAND_MAIN]);
    if(dragOff || dragMain)
    {
        Con_Printf("flatgrab: drags off %d (%.1f u off the hand) main %d (%.1f u off the hand)\n", dragOff ? d[HAND_OFF].num : 0,
            d[HAND_OFF].off, dragMain ? d[HAND_MAIN].num : 0, d[HAND_MAIN].off);
    }
}

// The throw key's hand while the key is held, as the server's STAT_QVR_FLATGREN `stat` says: its tap window, its wind-up,
// a quick grenade wound up by the key; -1 none.
[[nodiscard]] int aimHand(int stat)
{
    if(key_dest != key_game || !flatplay::keyHeld(vr_flat_key_throw))
    {
        return -1;
    }
    const int tPhase = (stat >> 4) & 3;
    if(tPhase == throwWind || tPhase == throwPending)
    {
        return (stat & 64) ? HAND_MAIN : HAND_OFF;
    }
    if(gs().throwKeyWound && (stat & 7) == phaseWind)
    {
        return (stat & 8) ? HAND_MAIN : HAND_OFF;
    }
    return -1;
}

// The throw / drop key (vr_flat_key_throw, default B): 39 as it goes down, 40 as it comes up, and 40 again while the
// server still waits or winds up with it up (an impulse lost to another in the same move).
void throwKey(int stat)
{
    const bool down = key_dest == key_game && flatplay::keyHeld(vr_flat_key_throw);
    const int grenPhase = stat & 7;
    const int tPhase = (stat >> 4) & 3;
    const bool serverWinds = tPhase == throwWind || tPhase == throwPending || (gs().throwKeyWound && grenPhase == phaseWind);
    if(down && !gs().throwKeyDown)
    {
        gs().throwPresses++;
        gs().throwKeyWound = grenPhase == phaseHeld || grenPhase == phaseArmed;
        Cbuf_InsertText("impulse 39\n");
        if(vr_flat_grenade_debug.value)
        {
            Con_Printf("flatthrow: %s down: pending/wind up\n", vr_flat_key_throw.string);
        }
    }
    else if(!down && gs().throwKeyDown)
    {
        Cbuf_InsertText("impulse 40\n");
        gs().throwResend = throwResendFrames;
        if(vr_flat_grenade_debug.value)
        {
            Con_Printf("flatthrow: %s up: drop/throw\n", vr_flat_key_throw.string);
        }
    }
    else if(!down && serverWinds && --gs().throwResend <= 0)
    {
        Cbuf_InsertText("impulse 40\n");
        gs().throwResend = throwResendFrames;
    }
    if(!serverWinds && !down && grenPhase != phaseWind)
    {
        gs().throwKeyWound = false;
    }
    gs().throwKeyDown = down;

    // The cancel (flatthrow2, his words 2026-11-26: "cancel a throw by pressing the throwing hand's corresponding attack
    // button while the throw key is being pressed"): a fresh press of that hand's button (flatplay::attackButtons) while B
    // is held sends 41 (QC VR_FlatThrow_Cancel: back to the hold, nothing thrown); the button does nothing till it is let
    // go (swallowedButtons: flatplay::routeFire), and 41 again while the server still winds up (an impulse lost).
    const int buttons = (client::mainButton() ? 1 : 0) | (client::offhandButton() ? 2 : 0);
    const int fresh = buttons & ~gs().buttonsWas;
    gs().buttonsWas = buttons;
    gs().swallow &= buttons;
    const int aimer = aimHand(stat);
    const int cancelBy = aimer >= 0 ? fresh & flatplay::attackButtons(aimer) : 0;
    if(cancelBy != 0)
    {
        gs().swallow |= cancelBy;
        gs().cancels++;
        gs().cancelSent = true;
        gs().cancelResend = throwResendFrames;
        Cbuf_InsertText("impulse 41\n");
        if(vr_flat_grenade_debug.value)
        {
            Con_Printf("flatthrow: the %s hand's attack button (%s) with %s held: throw cancelled\n", handName(aimer),
                (cancelBy & 1) ? "left" : "right", vr_flat_key_throw.string);
        }
    }
    else if(aimer < 0)
    {
        gs().cancelSent = false;
    }
    else if(gs().cancelSent && --gs().cancelResend <= 0)
    {
        Cbuf_InsertText("impulse 41\n");
        gs().cancelResend = throwResendFrames;
    }
}

// The Grab / Pull key (vr_flat_key_pull, default H): uses every available empty hand to grab physics props, crates,
// ragdolls and gibs in front of the player. Toggle mode (default, vr_flat_grab_hold 0): impulse 96 on press;
// Hold mode (vr_flat_grab_hold 1): impulse 97 on press (and retry while searching), impulse 98 on release.
void pullKey(int stat)
{
    const bool down = key_dest == key_game && flatplay::keyHeld(vr_flat_key_pull);
    const bool holdMode = vr_flat_grab_hold.value != 0.f;
    const bool grabOff = (stat & 256) != 0;
    const bool grabMain = (stat & 512) != 0;
    const bool serverHeld = grabOff || grabMain;
    gs().grabHeld[HAND_OFF] = grabOff;
    gs().grabHeld[HAND_MAIN] = grabMain;
    gs().grabHoldPose = (stat & 1024) != 0;

    if(down && !gs().pullKeyDown)
    {
        gs().pullPresses++;
        gs().pullRetry = pullRetryFrames;
        if(holdMode || !serverHeld)
        {
            const float pulse = secs(vr_flat_grab_reach_time);
            for(int h : {HAND_OFF, HAND_MAIN})
            {
                if(held::handEmpty(h) && !flashlight::holds(h))
                {
                    gs().hand[h].grabPulse = pulse;
                }
            }
        }
        Cbuf_InsertText(holdMode ? "impulse 97\n" : "impulse 96\n");
        if(vr_flat_grenade_debug.value || vr_flatplay_debug.value)
        {
            Con_Printf("flatgrab: %s down (%s)\n", vr_flat_key_pull.string, holdMode ? "hold grab" : (serverHeld ? "toggle release" : "toggle grab"));
        }
    }
    else if(down && holdMode && !serverHeld && --gs().pullRetry <= 0)
    {
        Cbuf_InsertText("impulse 97\n");
        gs().pullRetry = pullRetryFrames;
    }
    else if(!down && gs().pullKeyDown)
    {
        if(holdMode)
        {
            Cbuf_InsertText("impulse 98\n");
            gs().pullResend = throwResendFrames;
            if(vr_flat_grenade_debug.value || vr_flatplay_debug.value)
            {
                Con_Printf("flatgrab: %s up (hold release)\n", vr_flat_key_pull.string);
            }
        }
    }
    else if(!down && holdMode && serverHeld && --gs().pullResend <= 0)
    {
        Cbuf_InsertText("impulse 98\n");
        gs().pullResend = throwResendFrames;
    }
    gs().pullKeyDown = down;
}

// A place off the shoulder `sh`: `fwd` along `f`, `out` along the hand's own side of `r`, `up` along `u`, in shares of the
// arm's reach `span`.
[[nodiscard]] glm::vec3 place(const glm::vec3& sh, const glm::vec3& f, const glm::vec3& r, const glm::vec3& u, float side,
    float span, float fwd, float out, float up)
{
    return sh + (f * fwd + r * (side * out) + u * up) * span;
}

void blend(hands::State& s, int h, const glm::vec3& pos, const glm::vec3& rot, float a)
{
    if(a <= 0.f)
    {
        return;
    }
    s.pos[h] += (pos - s.pos[h]) * a;
    s.rot[h].x += geom::yawDelta(rot.x, s.rot[h].x) * a;
    s.rot[h].y += geom::yawDelta(rot.y, s.rot[h].y) * a;
    s.rot[h].z += geom::yawDelta(rot.z, s.rot[h].z) * a;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flatgrenade_report", report_f);
}

void frame(bool on)
{
    double dt = gs().lastTime > -1e8 ? vr_gametime - gs().lastTime : 0.0;
    gs().lastTime = vr_gametime;
    dt = za::clamp(dt, 0.0, 0.1);
    if(!on)
    {
        const int presses = gs().presses;
        const int throwPresses = gs().throwPresses;
        const int pullPresses = gs().pullPresses;
        const int cancels = gs().cancels;
        gs() = GrenadeState{};
        gs().presses = presses;
        gs().throwPresses = throwPresses;
        gs().pullPresses = pullPresses;
        gs().cancels = cancels;
        return;
    }

    // G: the game picks the hand and runs it (QC VR_FlatGrenade_Key).
    const bool down = key_dest == key_game && flatplay::keyHeld(vr_flat_key_grenade);
    if(down && !gs().keyDown)
    {
        gs().presses++;
        Cbuf_InsertText("impulse 57\n"); // (InsertText: now, not after what is queued)
        if(vr_flat_grenade_debug.value)
        {
            Con_Printf("flatgren: %s: quick grenade\n", vr_flat_key_grenade.string);
        }
    }
    gs().keyDown = down;

    const int stat = cl.stats[protocol::STAT_QVR_FLATGREN];
    throwKey(stat); // B: unified throw / drop key
    pullKey(stat);  // H: Grab / Pull key
    const int phase = stat & 7;
    if(phase != gs().phase && vr_flat_grenade_debug.value)
    {
        Con_Printf("flatgren: phase %d -> %d (%s hand)\n", gs().phase, phase, (stat & 8) ? "main" : "off");
    }
    gs().phase = phase;
    if(phase != 0)
    {
        gs().hand0 = (stat & 8) ? HAND_MAIN : HAND_OFF;
    }
    if(phase == phaseFetch)
    {
        gs().printed = 0;
    }
    const int rawThrowPhase = (stat >> 4) & 3;
    const int throwPhase = rawThrowPhase == throwPending ? 0 : rawThrowPhase; // (pending: no wind-up shown on tap)
    if(throwPhase != gs().throwPhase && vr_flat_grenade_debug.value)
    {
        Con_Printf("flatthrow: phase %d -> %d (%s hand%s)\n", gs().throwPhase, throwPhase, (stat & 64) ? "main" : "off",
            (stat & 128) ? ", both hands" : "");
    }
    if(throwPhase == throwWind && gs().throwPhase != throwWind)
    {
        gs().printed = 0;
    }
    gs().throwPhase = throwPhase;
    if(throwPhase != 0)
    {
        gs().throwHand = (stat & 64) ? HAND_MAIN : HAND_OFF;
        gs().throwBoth = (stat & 128) != 0;
    }
    // The wind-up's charge (shown: the server's own is its time held, QC VR_FlatThrow_Throw, VR_FlatGrenade_Throw).
    const bool winding = throwPhase == throwWind || phase == phaseWind;
    if(winding && gs().windStart < 0.0)
    {
        gs().windStart = vr_gametime;
    }
    else if(!winding)
    {
        gs().windStart = -1.0;
    }
    const float t = static_cast<float>(dt);
    if(t <= 0.f)
    {
        return;
    }
    const float chargeFull =
        phase == phaseWind ? za::clamp(vr_flat_grenade_wind_full.value, 0.1f, 5.f) : za::clamp(vr_flat_throw_charge_time.value, 0.05f, 5.f);
    const float charge = winding ? za::clamp(static_cast<float>(vr_gametime - gs().windStart) / chargeFull, 0.f, 1.f) : 0.f;
    // The other hand guides the wind-up when it is free (nothing in it, not busy: his words 2026-11-23), props, guns and
    // grenades alike; it eases back as the throw follows through.
    const int thrower = throwPhase == throwWind ? gs().throwHand : phase == phaseWind ? gs().hand0 : -1;
    const int guider = thrower >= 0 ? 1 - thrower : -1;
    const bool guides = guider >= 0 && vr_flat_throw_guide.value != 0.f && flatsight::handEmpty(guider) && !busy(guider);
    const bool holdMode = vr_flat_grab_hold.value != 0.f;
    const bool anyGrabHeld = gs().grabHeld[HAND_OFF] || gs().grabHeld[HAND_MAIN];
    const float grabUp = secs(vr_flat_grab_reach_time);
    const float grabDown = secs(vr_flat_grab_back_time);
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        Shares& a = gs().hand[h];
        if(a.grabPulse > 0.f)
        {
            a.grabPulse = za::max(0.f, a.grabPulse - t);
        }
        const bool searching = holdMode && gs().pullKeyDown && !anyGrabHeld && held::handEmpty(h) && !flashlight::holds(h);
        const bool wantReach = (a.grabPulse > 0.f || searching) && !gs().grabHeld[h];
        const bool wantHold = gs().grabHeld[h] && gs().grabHoldPose;
        step(a.grabReach, wantReach, grabUp, grabDown, t);
        step(a.grabHold, wantHold, grabUp, grabDown, t);
        step(a.guide, guides && h == guider, secs(vr_flat_throw_wind_time), secs(vr_flat_throw_back_time), t);
        const bool mine = phase != 0 && h == gs().hand0;
        const bool throws = throwPhase != 0 && h == gs().throwHand; // (the other hand of a thing held in both goes with it: apply)
        const float back = secs(vr_flat_throw_back_time);
        const float follow = secs(vr_flat_throw_follow_time);
        step(a.away, mine && phase == phaseFetch, secs(vr_flat_grenade_fetch_time), secs(vr_flat_grenade_up_time), t);
        step(a.hold, mine, secs(vr_flat_grenade_fetch_time), back, t);
        // The throw's motion, the grenade's and the throw key's alike: the wind-up, the follow-through.
        step(a.wind, (mine && phase == phaseWind) || (throws && throwPhase == throwWind), secs(vr_flat_throw_wind_time), follow, t);
        step(a.follow, (mine && phase == phaseThrown) || (throws && throwPhase == throwThrown), follow, back, t);
        if((mine && phase == phaseWind) || (throws && throwPhase == throwWind))
        {
            a.charge = charge;
        }
        else if(a.wind <= 0.f)
        {
            a.charge = 0.f;
        }
    }
}

void apply(hands::State& s)
{
    for(int h : {HAND_OFF, HAND_MAIN})
    {
        const Shares& a = gs().hand[h];
        if(!anyShare(a))
        {
            continue;
        }
        const glm::vec3 sh = flatplay::shoulderOf(s, h);
        const float span = flatplay::armReach();
        // The look, pitched no further than the hands follow it (vr_flat_pose_pitch), and the body's level frame.
        const float most = za::clamp(vr_flat_pose_pitch.value, 0.f, 90.f);
        const glm::vec3 look{za::clamp(s.headAngles.x, -most, most), s.headAngles.y, 0.f};
        glm::vec3 vf, vr, vu, lf, lr, lu;
        hands::angleVectors(look, vf, vr, vu);
        hands::angleVectors({0.f, look.y, 0.f}, lf, lr, lu);
        const float side = glm::dot(sh - s.head, lr) >= 0.f ? 1.f : -1.f; // (the hand's own side, whichever is main)

        const glm::vec3 holdAt = place(sh, vf, vr, vu, side, span, vr_flat_grenade_hold_fwd.value, vr_flat_grenade_hold_out.value,
            -vr_flat_grenade_hold_down.value);
        // The throw's motion (the grenade's and the throw key's): wound up above and beside the shoulder along the look, so
        // that it stays at the screen's upper corner (his words 2026-11-23: it went too far back), further back as it
        // charges, then out along the look in the follow-through.
        const float back = za::clamp(vr_flat_throw_charge_back.value, -1.f, 1.f) * geom::smooth01(a.charge);
        const glm::vec3 windAt = place(sh, vf, vr, vu, side, span, vr_flat_throw_wind_fwd.value - back, vr_flat_throw_wind_out.value,
            vr_flat_throw_wind_up.value);
        const glm::vec3 followAt = place(sh, vf, vr, vu, side, span, vr_flat_throw_follow_fwd.value, vr_flat_throw_follow_out.value,
            -vr_flat_throw_follow_down.value);
        const glm::vec3 fetchAt = place(sh, lf, lr, lu, side, span, vr_flat_grenade_fetch_fwd.value, vr_flat_grenade_fetch_out.value,
            -vr_flat_grenade_fetch_down.value);
        const auto pitch = [](const Tunable& t) { return za::clamp(t.value, -90.f, 90.f); };

        // Grab / Pull (H): hold-out pose for 2H props/ragdolls, and subtle forward reach-out animation when grabbing.
        // Applied before throw wind/follow so winding up B smoothly overrides from a held grab pose.
        if(a.grabHold > 0.f)
        {
            const glm::vec3 grabHoldAt = place(sh, vf, vr, vu, side, span, vr_flat_grab_hold_fwd.value,
                vr_flat_grab_hold_out.value, -vr_flat_grab_hold_down.value);
            const glm::vec3 palmIn = glm::normalize(-vr * (side * 0.85f) - vu * 0.5f);
            blend(s, h, grabHoldAt, flatplay::handFacing(h, vf, palmIn), geom::smooth01(a.grabHold));
        }
        if(a.grabReach > 0.f)
        {
            const glm::vec3 grabReachAt = place(sh, vf, vr, vu, side, span, vr_flat_grab_reach_fwd.value,
                vr_flat_grab_reach_out.value, -vr_flat_grab_reach_down.value);
            const glm::vec3 palmReach = glm::normalize(-vr * (side * 0.6f) - vu * 0.8f);
            blend(s, h, grabReachAt, flatplay::handFacing(h, vf, palmReach), geom::smooth01(a.grabReach));
        }

        blend(s, h, holdAt, {look.x + pitch(vr_flat_grenade_hold_pitch), look.y, 0.f}, geom::smooth01(a.hold));
        // (The wind-up's yaw turns the fingers out to the hand's side, its roll mirrored by side.)
        blend(s, h, windAt,
            {look.x + pitch(vr_flat_throw_wind_pitch), look.y - side * za::clamp(vr_flat_throw_wind_yaw.value, -90.f, 90.f),
                side * za::clamp(vr_flat_throw_wind_roll.value, -180.f, 180.f)},
            geom::smooth01(a.wind));
        // A weapon held with its grip flipped, wound up (flatthrow2, his words 2026-11-26): moved and turned about the fist
        // over the wind-up by vr_flat_throw_rev_* (all 0: as any weapon's), by the flip's drawn share.
        if(const float rk = geom::smooth01(a.wind) * flatplay::flipShare(h); rk > 0.f)
        {
            const auto units = [](const Tunable& c) { return za::clamp(c.value, -30.f, 30.f); };
            const auto degrees = [](const Tunable& c) { return za::clamp(c.value, -180.f, 180.f); };
            s.pos[h] += (vf * units(vr_flat_throw_rev_fwd) + vr * (side * units(vr_flat_throw_rev_out)) + vu * units(vr_flat_throw_rev_up)) * rk;
            const glm::vec3 turn{degrees(vr_flat_throw_rev_pitch), degrees(vr_flat_throw_rev_yaw), degrees(vr_flat_throw_rev_roll)};
            if(turn != glm::vec3{0.f})
            {
                s.rot[h] = flatplay::blendHandAngles(s.rot[h], flatplay::turnedHandAngles(s.rot[h], turn, side), rk);
            }
        }
        blend(s, h, followAt, {look.x + pitch(vr_flat_throw_follow_pitch), look.y, 0.f}, geom::smooth01(a.follow));
        // The release's arc (flatthrow3): on the way from the wind-up into the follow-through (the thing still in the hand
        // for vr_flat_throw_release_time of it, QC VR_FlatThrow_Release), the hand raised vr_flat_throw_release_arc (a share
        // of the reach) at its middle, where the wind-up's share and the follow-through's cross.
        const bool following = (gs().phase == phaseThrown && gs().hand0 == h) || (gs().throwPhase == throwThrown && gs().throwHand == h);
        if(const float arc = za::clamp(vr_flat_throw_release_arc.value, -0.5f, 0.5f); following && arc != 0.f)
        {
            s.pos[h] += vu * (arc * span * 4.f * geom::smooth01(a.wind) * geom::smooth01(a.follow));
        }
        blend(s, h, fetchAt, {pitch(vr_flat_grenade_fetch_pitch), look.y, 0.f}, geom::smooth01(a.away));
        if(a.guide > 0.f)
        {
            // The guiding hand: forward off its shoulder along the look, the fingers raised vr_flat_throw_guide_pitch and
            // turned in vr_flat_throw_guide_yaw, the palm away from him (down at pitch 0, turned forward as they rise),
            // turned about the fingers vr_flat_throw_guide_roll (out positive).
            const glm::vec3 guideAt = place(sh, vf, vr, vu, side, span, vr_flat_throw_guide_fwd.value, vr_flat_throw_guide_out.value,
                vr_flat_throw_guide_up.value);
            const float gp = glm::radians(pitch(vr_flat_throw_guide_pitch));
            const float gy = glm::radians(za::clamp(vr_flat_throw_guide_yaw.value, -90.f, 90.f));
            const float gr = glm::radians(za::clamp(vr_flat_throw_guide_roll.value, -180.f, 180.f)) * side;
            const glm::vec3 ahead = vf * za::cos(gy) - vr * (side * za::sin(gy));
            const glm::vec3 fingers = ahead * za::cos(gp) + vu * za::sin(gp);
            glm::vec3 palm = -vu * za::cos(gp) + ahead * za::sin(gp);
            palm = palm * za::cos(gr) + glm::cross(fingers, palm) * za::sin(gr);
            blend(s, h, guideAt, flatplay::handFacing(h, fingers, palm), geom::smooth01(a.guide));
        }

        // (Held in both hands as the wind-up began: the other hand let go, letsGo; it doesn't go along: his call 2026-11-23.)

        // (vr_flat_grenade_debug: where the hand is as each place is reached, off the head along the look: the tests.)
        if(vr_flat_grenade_debug.value)
        {
            const float full[4] = {a.away, a.hold * (1.f - a.away) * (1.f - a.wind) * (1.f - a.follow), a.wind, a.follow};
            const char* names[4] = {"the pouch", "held", "overhead", "the follow-through"};
            for(int i = 0; i < 4; i++)
            {
                const int bit = 1 << i;
                if(full[i] >= 1.f && !(gs().printed & bit))
                {
                    gs().printed |= bit;
                    const glm::vec3 d = s.pos[h] - s.head;
                    Con_Printf("flatgren: %s hand at %s: %.1f forward, %.1f right, %.1f up of the eye, pitch %.0f\n", handName(h),
                        names[i], glm::dot(d, vf), glm::dot(d, vr), glm::dot(d, vu), s.rot[h].x);
                }
            }
        }
    }
}

int heldHand()
{
    return gs().phase >= phaseHeld && gs().phase <= phaseWind ? gs().hand0 : -1;
}

bool busy(int h)
{
    // (The phases only: on the way back after the throw the hand may take the grip again, so it eases from the
    // follow-through straight onto it.)
    // The throw key's hand too (the other hand of a thing held in both lets go of it: letsGo), and Grab / Pull (H)
    // reaching or holding.
    return h >= 0 && h < 2 &&
           ((gs().phase != 0 && gs().hand0 == h) || (gs().throwPhase != 0 && gs().throwHand == h) || gs().grabHeld[h] ||
               gs().hand[h].grabPulse > 0.f || gs().hand[h].grabReach > 0.1f);
}

bool letsGo(int holding, int helping)
{
    return busy(holding) || busy(helping);
}

bool throwsWith(int h)
{
    return h >= 0 && h < 2 && gs().throwPhase != 0 && gs().throwHand == h;
}

bool aimsWith(int h)
{
    return h >= 0 && h < 2 && aimHand(cl.stats[protocol::STAT_QVR_FLATGREN]) == h;
}

int swallowedButtons()
{
    return gs().swallow;
}

float guideShare(int h)
{
    return h >= 0 && h < 2 ? geom::smooth01(gs().hand[h].guide) : 0.f;
}

float grabOpenShare(int h)
{
    return h >= 0 && h < 2 && !gs().grabHeld[h] ? geom::smooth01(gs().hand[h].grabReach) : 0.f;
}

} // namespace qvr::flatgrenade
