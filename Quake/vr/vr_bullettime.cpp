// vr_bullettime.cpp -- see vr_bullettime.hpp.

#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_highlights.hpp"
#include "vr_main.hpp"

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
    double pressedAt = -1.0; // realtime of the last press
};
State state;

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
        playSound(vr_bullettime_sound_denied);
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: not ready (meter %.2f, cooldown %.1f s)\n", state.level, state.cooldown);
        }
        return;
    }
    state.active = true;
    highlights::bulletTime(true, scale());
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
    if(!inGame() || key_dest != key_game || !s.valid || !button(at, out))
    {
        state.pressing = false;
        return;
    }
    const float radius = za::max(0.1f, vr_bullettime_button_radius.value);
    const float d = glm::length(fingertip(s, hand) - at);
    if(vr_debug_bullettime.value >= 2.f)
    {
        Con_Printf("bullet time: fingertip %.1f units from the button (%s)\n", d, state.pressing ? "on" : "off");
    }
    if(!state.pressing && d <= radius)
    {
        state.pressing = true;
        if(realtime - state.pressedAt > 0.3)
        {
            state.pressedAt = realtime;
            buzz(hand, 0.04f, 0.5f);
            buzz(hands::gadgetHand(), 0.04f, 0.3f);
            toggle();
        }
    }
    else if(state.pressing && d > radius * 1.5f)
    {
        state.pressing = false;
    }
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
