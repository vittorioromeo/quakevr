// vr_bullettime.cpp -- see vr_bullettime.hpp.

#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_highlights.hpp"
#include "vr_main.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstdio>

namespace qvr::bullettime
{
namespace
{

// The button: the inner of the two on the gadget's lower edge (Misc/quakevr/make_gadget.py: at x 0.25, y -1.33,
// z -0.05 of the model, 0.4 wide), its face down the screen's up.
constexpr glm::vec3 buttonLocal{0.25f, -1.38f, -0.05f};

struct State
{
    bool active = false;
    float level = 1.f;    // the meter, 0 .. 1
    float cooldown = 0.f; // real seconds left before the meter fills and it can start again
    float look = 0.f;     // the look's ease, 0 .. 1
    bool pressing = false; // the fingertip on the button (a press is its arrival)
    double triggeredAt = -1.0; // realtime of the last press or tap that counted (vr_bullettime_trigger_cooldown)
    float tapPeak = 0.f;       // the wrist tap: the hands' peak speed (m/s) coming together, near the zone; 0: none
    double tapPeakAt = -1.0;   // ... and when
    bool stickTaken[HAND_COUNT] = {}; // the stick press taken at its press (vr_bullettime_trigger): its release is too
};
State state;

// The "off" sound (vr_bullettime_sound_off: a placeholder 3 s long, longer than a short burst): played for at most
// vr_bullettime_sound_off_max real seconds, then faded out over offFade; cut (faded over offCut) as bullet time starts
// again, or a denied press sounds. Its channel found again each frame (by its sound, the player's entity and the local
// sounds' channel -1, as S_LocalSound starts it): a channel taken by another sound since is never touched.
struct OffSound
{
    sfx_t* sfx = nullptr;   // playing (nullptr: none, or done with)
    double fadeFrom = 0.0;  // realtime its fade starts
    double fadeLen = 0.0;   // ... and lasts
};
OffSound offSound;
constexpr double offFade = 0.25;
constexpr double offCut = 0.06;

[[nodiscard]] channel_t* offChannel()
{
    if(!offSound.sfx || cl.viewentity <= 0)
    {
        return nullptr;
    }
    for(int i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
    {
        channel_t* ch = &snd_channels[i];
        if(ch->sfx == offSound.sfx && ch->entnum == cl.viewentity && ch->entchannel == -1)
        {
            return ch;
        }
    }
    return nullptr;
}

// The "off" sound's fade, each frame (advance).
void fadeOffSound()
{
    channel_t* ch = offChannel();
    if(!ch)
    {
        offSound.sfx = nullptr;
        return;
    }
    const double t = realtime - offSound.fadeFrom;
    if(t <= 0.0)
    {
        return;
    }
    const double gain = offSound.fadeLen > 0.0 ? 1.0 - t / offSound.fadeLen : 0.0;
    if(gain <= 0.0)
    {
        ch->sfx = nullptr; // (as S_StopSound: this channel only)
        ch->end = 0;
        offSound.sfx = nullptr;
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: the off sound cut %.2f s after its fade began\n", t);
        }
        return;
    }
    ch->master_vol = za::min(ch->master_vol, static_cast<int>(255.0 * gain));
}

// Its fade from now, over `seconds` (not later than one under way).
void cutOffSound(double seconds)
{
    if(offSound.sfx && realtime + seconds < offSound.fadeFrom + offSound.fadeLen)
    {
        offSound.fadeFrom = realtime;
        offSound.fadeLen = seconds;
    }
}

[[nodiscard]] bool inGame()
{
    return vr_bullettime_enabled.value != 0.f && sv.active && svs.maxclients == 1 && cls.state == ca_connected &&
           cls.signon == SIGNONS && !cl.intermission && cl.stats[STAT_HEALTH] > 0;
}

void playSound(const cvar_t& name)
{
    if(name.string && name.string[0])
    {
        S_LocalSound(name.string);
    }
}

void buzz(int hand, float seconds, float amplitude)
{
    if(Backend* const be = backend(); be && (hand == HAND_OFF || hand == HAND_MAIN))
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void stop(bool quiet)
{
    if(!state.active)
    {
        return;
    }
    state.active = false;
    highlights::bulletTime(false, 1.f);
    state.cooldown = za::max(0.f, vr_bullettime_cooldown.value);
    if(!quiet)
    {
        playSound(vr_bullettime_sound_off);
        offSound = OffSound{};
        const float most = vr_bullettime_sound_off_max.value;
        if(most > 0.f && vr_bullettime_sound_off.string && vr_bullettime_sound_off.string[0])
        {
            offSound.sfx = S_PrecacheSound(vr_bullettime_sound_off.string);
            offSound.fadeFrom = realtime + static_cast<double>(most);
            offSound.fadeLen = offFade;
        }
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: off, meter %.2f, cooldown %.1f s\n", state.level, state.cooldown);
    }
}

// The press point: the other hand's fingertip, vr_bullettime_button_reach units ahead of its place.
[[nodiscard]] glm::vec3 fingertip(const hands::State& s, int hand)
{
    return s.pos[hand] + hands::forward(s.rot[hand]) * vr_bullettime_button_reach.value;
}

// A press or tap that counts: the tick in both hands and the toggle, unless one counted a moment ago
// (vr_bullettime_trigger_cooldown: a press and a tap together, or a bounce, are one).
void trigger(int hand, const char* what)
{
    if(state.triggeredAt >= 0.0 && realtime - state.triggeredAt < za::max(0.f, vr_bullettime_trigger_cooldown.value))
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: %s ignored (%.2f s after the last)\n", what, realtime - state.triggeredAt);
        }
        return;
    }
    state.triggeredAt = realtime;
    const float haptic = za::clamp(vr_bullettime_haptic.value, 0.f, 2.f);
    if(haptic > 0.f)
    {
        buzz(hand, 0.04f, za::min(1.f, 0.5f * haptic));
        buzz(hands::gadgetHand(), 0.04f, za::min(1.f, 0.3f * haptic));
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: %s\n", what);
    }
    toggle();
}

// The button: the fingertip's arrival within vr_bullettime_button_radius of its middle presses it; it re-arms past 1.5x.
void pressButton(const hands::State& s, int hand, const glm::vec3& at)
{
    const float radius = za::max(0.1f, vr_bullettime_button_radius.value);
    const float d = glm::length(fingertip(s, hand) - at);
    if(vr_debug_bullettime.value == 2.f)
    {
        Con_Printf("bullet time: fingertip %.1f units from the button (%s)\n", d, state.pressing ? "on" : "off");
    }
    if(!state.pressing && d <= radius)
    {
        state.pressing = true;
        trigger(hand, "button pressed");
    }
    else if(state.pressing && d > radius * 1.5f)
    {
        state.pressing = false;
    }
}

// Whether both hands hold one weapon (the off hand on a foregrip, a free grip, a blade grip, a cup).
[[nodiscard]] bool twoHanded()
{
    for(int h = 0; h < HAND_COUNT; ++h)
    {
        if(twohand::helping(h) || twohand::helpKind(h) > 0 || twohand::support(h) > 0.f)
        {
            return true;
        }
    }
    return false;
}

// The wrist tap: the other hand's middle (its palm: never what it holds) near the gadget's middle
// (vr_bullettime_tap_radius), having come at it at vr_bullettime_tap_speed or more (the hands' speed towards each other:
// both may move), and then stopped there (its speed down to vr_bullettime_tap_stop of the peak within
// vr_bullettime_tap_window seconds: an impact; a hand passing by keeps its speed). Resting or brushing hands are too
// slow; hands moving together (a two-handed hold) have no speed towards each other, and are ignored anyway unless
// vr_bullettime_tap_twohanded.
void tapWrist(const hands::State& s, int hand)
{
    glm::vec3 centre;
    const bool allowed = tapZone(centre) && (vr_bullettime_tap_holding.value != 0.f || held::handEmpty(hand)) &&
                         (vr_bullettime_tap_twohanded.value != 0.f || !twoHanded());
    if(!allowed)
    {
        if(vr_debug_bullettime.value >= 3.f)
        {
            Con_Printf("bullet time: tap off (%s)\n", twoHanded() ? "two-handed" : "holding, or no gadget");
        }
        state.tapPeak = 0.f;
        return;
    }
    const float m2u = units::metresToUnits();
    const float radius = za::max(1.f, vr_bullettime_tap_radius.value) * 0.01f * m2u;
    const glm::vec3 p = hands::palmPoint(s, hand);
    const glm::vec3 toward = centre - p;
    const float d = glm::length(toward);
    const glm::vec3 rel = s.vel[hand] - s.vel[hands::gadgetHand()]; // m/s
    const float speed = glm::length(rel);
    const float closing = d > 1e-3f ? glm::dot(rel, toward / d) : speed;
    const float least = za::max(0.05f, vr_bullettime_tap_speed.value);
    if(vr_debug_bullettime.value >= 3.f)
    {
        Con_Printf("bullet time: tap %.1f cm from the gadget, closing %.2f m/s, speed %.2f, peak %.2f\n",
                   d / m2u * 100.f, closing, speed, state.tapPeak);
    }
    if(d <= radius * 1.5f && closing >= least && speed >= state.tapPeak)
    {
        state.tapPeak = speed;
        state.tapPeakAt = realtime;
    }
    if(state.tapPeak > 0.f && realtime - state.tapPeakAt > za::max(0.f, vr_bullettime_tap_window.value))
    {
        state.tapPeak = 0.f; // came at it, but never stopped there
    }
    if(state.tapPeak > 0.f && d <= radius && speed <= state.tapPeak * za::clamp(vr_bullettime_tap_stop.value, 0.f, 1.f))
    {
        char what[64];
        q_snprintf(what, sizeof(what), "wrist tapped at %.2f m/s", state.tapPeak);
        state.tapPeak = 0.f;
        trigger(hand, what);
    }
}

// vr_bullettime_trigger's stick: HAND_OFF (the left controller) for 1, HAND_MAIN (the right) for 2; -1: the gadget.
[[nodiscard]] int stickHand()
{
    const int t = static_cast<int>(vr_bullettime_trigger.value);
    return t == 1 ? HAND_OFF : t == 2 ? HAND_MAIN : -1;
}

// vr_bullettime: as the gadget's button.
void bullettime_f()
{
    if(!inGame())
    {
        Con_Printf("vr_bullettime: needs vr_bullettime_enabled 1 and a single-player game\n");
        return;
    }
    toggle();
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_bullettime", bullettime_f);
}

bool stickPress(int hand, bool now)
{
    if(!now)
    {
        const bool taken = state.stickTaken[hand];
        state.stickTaken[hand] = false;
        return taken;
    }
    if(hand != stickHand() || key_dest != key_game || vr_bullettime_enabled.value == 0.f)
    {
        return false;
    }
    state.stickTaken[hand] = true;
    if(inGame())
    {
        trigger(hand, hand == HAND_OFF ? "left stick press" : "right stick press");
    }
    return true;
}

void toggle()
{
    if(!inGame())
    {
        return;
    }
    if(state.active)
    {
        stop(false);
        return;
    }
    if(state.cooldown > 0.f || state.level < za::clamp(vr_bullettime_min.value, 0.01f, 1.f))
    {
        cutOffSound(offCut);
        playSound(vr_bullettime_sound_denied);
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: not ready (meter %.2f, cooldown %.1f s)\n", state.level, state.cooldown);
        }
        return;
    }
    state.active = true;
    highlights::bulletTime(true, scale());
    cutOffSound(offCut);
    playSound(vr_bullettime_sound_on);
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: on at %.2fx, meter %.2f (%.1f s)\n", scale(), state.level,
                   state.level * za::max(0.1f, vr_bullettime_duration.value));
    }
}

void advance(double dt)
{
    const float fadeTime = 0.25f;
    fadeOffSound();
    if(!sv.active)
    {
        state = State{};
        return;
    }
    if(!inGame())
    {
        stop(true);
    }
    const float step = static_cast<float>(dt);
    const bool running = key_dest == key_game; // a menu open: the meter waits
    if(running && state.active)
    {
        state.level -= step / za::max(0.1f, vr_bullettime_duration.value);
        if(state.level <= 0.f)
        {
            state.level = 0.f;
            stop(false);
        }
    }
    else if(running && state.cooldown > 0.f)
    {
        state.cooldown = za::max(0.f, state.cooldown - step);
    }
    else if(running)
    {
        state.level = za::min(1.f, state.level + step / za::max(0.1f, vr_bullettime_recharge.value));
    }
    const float target = state.active ? 1.f : 0.f;
    const float most = step / fadeTime;
    state.look = state.look < target ? za::min(target, state.look + most) : za::max(target, state.look - most);
}

void frame()
{
    glm::vec3 at, out;
    const hands::State& s = hands::current();
    const int hand = 1 - hands::gadgetHand();
    // A stick press starts it instead (vr_bullettime_trigger): the gadget's button and the wrist tap do nothing.
    if(!inGame() || key_dest != key_game || !s.valid || stickHand() >= 0 || !button(at, out))
    {
        state.pressing = false;
        state.tapPeak = 0.f;
        return;
    }
    if(vr_bullettime_button.value != 0.f)
    {
        pressButton(s, hand, at);
    }
    else
    {
        state.pressing = false;
    }
    if(vr_bullettime_tap.value != 0.f)
    {
        tapWrist(s, hand);
    }
    else
    {
        state.tapPeak = 0.f;
    }
}

bool tapZone(glm::vec3& centre)
{
    const gadget::Pose& gp = gadget::pose();
    if(!gadget::active() || !gp.valid)
    {
        return false;
    }
    centre = gp.origin;
    return true;
}

bool tapHandTarget(int hand, float cm, glm::vec3& out)
{
    glm::vec3 centre;
    const hands::State& s = hands::current();
    if(!s.valid || !tapZone(centre))
    {
        return false;
    }
    const glm::vec3 palm = hands::palmPoint(s, hand);
    glm::vec3 dir = palm - centre;
    const float len = glm::length(dir);
    dir = len > 1e-3f ? dir / len : gadget::pose().axes[2];
    out = centre + dir * (cm * 0.01f * units::metresToUnits()) - (palm - s.pos[hand]);
    return true;
}

float scale()
{
    return state.active ? za::clamp(vr_bullettime_scale.value, 0.05f, 1.f) : 1.f;
}

bool sandevistan()
{
    return state.active && vr_bullettime_sandevistan.value != 0.f;
}

Meter meter()
{
    Meter m;
    m.enabled = vr_bullettime_enabled.value != 0.f && sv.active && svs.maxclients == 1;
    m.active = state.active;
    m.cooling = !state.active && state.cooldown > 0.f;
    m.ready = !state.active && !m.cooling && state.level >= za::clamp(vr_bullettime_min.value, 0.01f, 1.f);
    m.level = state.level;
    return m;
}

Look look()
{
    Look l;
    l.strength = state.look * za::clamp(vr_bullettime_fx.value, 0.f, 1.f);
    l.desaturate = za::clamp(vr_bullettime_fx_desat.value, 0.f, 1.f);
    l.vignette = za::clamp(vr_bullettime_fx_vignette.value, 0.f, 1.f);
    float r = 1.f, g = 1.f, b = 1.f;
    if(std::sscanf(vr_bullettime_fx_tint.string, "%f %f %f", &r, &g, &b) == 3)
    {
        l.tint = glm::clamp(glm::vec3{r, g, b}, glm::vec3{0.f}, glm::vec3{2.f});
    }
    return l;
}

bool button(glm::vec3& at, glm::vec3& out)
{
    const gadget::Pose& gp = gadget::pose();
    if(!gadget::active() || !gp.valid)
    {
        return false;
    }
    at = gp.origin + gp.axes * (buttonLocal * gp.scale);
    out = -gp.axes[1];
    return true;
}

bool buttonHandTarget(int hand, glm::vec3& out)
{
    glm::vec3 at, face;
    const hands::State& s = hands::current();
    if(!s.valid || !button(at, face))
    {
        return false;
    }
    out = at - (fingertip(s, hand) - s.pos[hand]);
    return true;
}

} // namespace qvr::bullettime
