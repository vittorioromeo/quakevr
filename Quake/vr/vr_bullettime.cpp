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
#include "vr_view.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstdio>

namespace qvr::bullettime
{
namespace
{

struct State
{
    bool active = false;
    float level = 1.f;    // the meter, 0 .. 1
    float cooldown = 0.f; // real seconds left before the meter fills and it can start again
    float look = 0.f;     // the look's ease, 0 .. 1
    double triggeredAt = -1.0; // realtime of the last tap or stick press that counted (vr_bullettime_trigger_cooldown)
    float tapPeak = 0.f;       // the screen tap: the peak speed (m/s) into the screen, over it; 0: none
    double tapPeakAt = -1.0;   // ... and when
    int tapStriker = 0;        // ... and which striking point (0 the hand's middle, 1 its gun's butt)
    double tappedAt = -1.0;    // realtime of the last tap that counted (tapping())
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

// A tap or stick press that counts: the tick in both hands and the toggle, unless one counted a moment ago
// (vr_bullettime_trigger_cooldown: a bounce is one).
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

// The tapping hand's striking points: its middle (its palm: never what it holds) and, holding a gun
// (vr_bullettime_tap_butt), that gun's butt; with their velocities (m/s, the hand's turn included).
struct Striker
{
    glm::vec3 at{0.f};
    glm::vec3 vel{0.f};
};
constexpr const char* strikerNames[2] = {"hand", "gun's butt"};

[[nodiscard]] int strikers(const hands::State& s, int hand, Striker (&out)[2])
{
    const float m2u = units::metresToUnits();
    const auto at = [&](const glm::vec3& p) {
        return Striker{p, s.vel[hand] + glm::cross(s.angVel[hand], (p - s.pos[hand]) / m2u)};
    };
    out[0] = at(hands::palmPoint(s, hand));
    glm::vec3 butt;
    if(vr_bullettime_tap_butt.value != 0.f && view::heldWeaponButt(hand, butt))
    {
        out[1] = at(butt);
        return 2;
    }
    return 1;
}

// Where a striking point is against the screen: across it (u: its right, w: its up), over it (h: out of it; units), and
// its velocity against the screen's point under it (the gadget's arm may move and turn too).
struct Against
{
    float u{0.f}, w{0.f}, h{0.f};
    glm::vec3 v{0.f}; // m/s
    float into{0.f};  // its speed into the screen (m/s)
};

[[nodiscard]] Against against(const hands::State& s, const Screen& z, const Striker& k)
{
    const int g = hands::gadgetHand();
    const glm::vec3 rel = k.at - z.centre;
    Against a;
    a.u = glm::dot(rel, z.right);
    a.w = glm::dot(rel, z.up);
    a.h = glm::dot(rel, z.normal);
    const glm::vec3 under = k.at - z.normal * a.h;
    a.v = k.vel - (s.vel[g] + glm::cross(s.angVel[g], (under - s.pos[g]) / units::metresToUnits()));
    a.into = -glm::dot(a.v, z.normal);
    return a;
}

// The screen tap: a striking point of the other hand (strikers) over the gadget's screen (within vr_bullettime_tap_margin
// of its edges), having come straight into it (within vr_bullettime_tap_angle of its normal) at vr_bullettime_tap_speed
// or more (against the screen's own motion: both may move), then stopped on it (within vr_bullettime_tap_depth of its
// face, its speed into it down to vr_bullettime_tap_stop of the peak within vr_bullettime_tap_window seconds: an impact;
// a hand passing by keeps its speed). A swing across the screen (the melee) comes from the side; resting, brushing or
// soft touches are too slow; hands moving together (a two-handed hold) have no speed towards each other, and are ignored
// anyway unless vr_bullettime_tap_twohanded.
void tapScreen(const hands::State& s, int hand, const Screen& z)
{
    const bool allowed = (vr_bullettime_tap_holding.value != 0.f || held::handEmpty(hand)) &&
                         (vr_bullettime_tap_twohanded.value != 0.f || !twoHanded());
    if(!allowed)
    {
        if(vr_debug_bullettime.value >= 2.f)
        {
            Con_Printf("bullet time: tap off (%s)\n", twoHanded() ? "two-handed" : "holding");
        }
        state.tapPeak = 0.f;
        return;
    }
    const float cm = 0.01f * units::metresToUnits();
    const float margin = za::max(0.f, vr_bullettime_tap_margin.value) * cm;
    const float depth = za::max(0.5f, vr_bullettime_tap_depth.value) * cm;
    const float least = za::max(0.05f, vr_bullettime_tap_speed.value);
    const float cosMost = za::cos(glm::radians(za::clamp(vr_bullettime_tap_angle.value, 1.f, 89.f)));
    const auto over = [&](const Against& a) {
        return za::fabs(a.u) <= z.halfSize.x + margin && za::fabs(a.w) <= z.halfSize.y + margin;
    };
    Striker strike[2];
    const int count = strikers(s, hand, strike);
    if(state.tapPeak > 0.f && state.tapStriker >= count)
    {
        state.tapPeak = 0.f; // (the gun let go of)
    }
    for(int i = 0; i < count; ++i)
    {
        const Against a = against(s, z, strike[i]);
        const float speed = glm::length(a.v);
        if(vr_debug_bullettime.value >= 2.f && over(a) && a.h < depth + 20.f * cm && a.h > -2.f * depth)
        {
            Con_Printf("bullet time: tap %s %.1f cm over the screen (%.1f, %.1f), into it %.2f m/s of %.2f, peak %.2f\n",
                strikerNames[i], a.h / cm, a.u / cm, a.w / cm, a.into, speed, state.tapPeak);
        }
        // Coming at it: over the screen, from a little above it to just through it, straight and fast enough.
        const bool coming = over(a) && a.h <= depth + 12.f * cm && a.h >= -2.f * depth;
        if(coming && a.into >= least && a.into >= cosMost * speed && a.into >= state.tapPeak)
        {
            state.tapPeak = a.into;
            state.tapPeakAt = realtime;
            state.tapStriker = i;
        }
    }
    if(state.tapPeak <= 0.f)
    {
        return;
    }
    if(realtime - state.tapPeakAt > za::max(0.f, vr_bullettime_tap_window.value))
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: a tap at %.2f m/s never stopped on the screen\n", state.tapPeak);
        }
        state.tapPeak = 0.f; // came at it, but never stopped there
        return;
    }
    const Against a = against(s, z, strike[state.tapStriker]);
    if(over(a) && a.h <= depth && a.h >= -2.f * depth &&
        a.into <= state.tapPeak * za::clamp(vr_bullettime_tap_stop.value, 0.f, 1.f))
    {
        char what[80];
        q_snprintf(what, sizeof(what), "screen tapped by the %s at %.2f m/s", strikerNames[state.tapStriker], state.tapPeak);
        state.tapPeak = 0.f;
        state.tappedAt = realtime;
        trigger(hand, what);
    }
}

// vr_bullettime_trigger's stick: HAND_OFF (the left controller) for 1, HAND_MAIN (the right) for 2; -1: the gadget.
[[nodiscard]] int stickHand()
{
    const int t = static_cast<int>(vr_bullettime_trigger.value);
    return t == 1 ? HAND_OFF : t == 2 ? HAND_MAIN : -1;
}

// vr_bullettime: as the screen tap.
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
    const hands::State& s = hands::current();
    Screen z;
    // A stick press starts it instead (vr_bullettime_trigger): the screen tap does nothing.
    if(!inGame() || key_dest != key_game || !s.valid || stickHand() >= 0 || vr_bullettime_tap.value == 0.f || !screen(z))
    {
        state.tapPeak = 0.f;
        return;
    }
    tapScreen(s, 1 - hands::gadgetHand(), z);
}

bool screen(Screen& out)
{
    const gadget::Pose& gp = gadget::pose();
    if(!gadget::active() || !gp.valid)
    {
        return false;
    }
    glm::vec3 corner;
    glm::vec2 size;
    gadget::screenRect(corner, size);
    const glm::vec3 mid{corner.x + size.x * 0.5f, corner.y + size.y * 0.5f, corner.z};
    out.centre = gp.origin + gp.axes * (mid * gp.scale);
    out.right = gp.axes[0];
    out.up = gp.axes[1];
    out.normal = gp.axes[2];
    out.halfSize = size * (0.5f * gp.scale);
    return true;
}

bool tapping()
{
    return state.tapPeak > 0.f || (state.tappedAt >= 0.0 && realtime - state.tappedAt < 0.5);
}

bool tapHandTarget(int hand, float cm, float sideCm, bool butt, glm::vec3& out)
{
    Screen z;
    const hands::State& s = hands::current();
    glm::vec3 from{0.f};
    if(!s.valid || !screen(z) || (butt ? !view::heldWeaponButt(hand, from) : false))
    {
        return false;
    }
    if(!butt)
    {
        from = hands::palmPoint(s, hand);
    }
    const float u = 0.01f * units::metresToUnits();
    out = z.centre + z.normal * (cm * u) + z.right * (sideCm * u) - (from - s.pos[hand]);
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

} // namespace qvr::bullettime
