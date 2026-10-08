// vr_gearlights.cpp -- see vr_gearlights.hpp.

#include "vr_gearlights.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_stealth.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::gearlights
{
namespace
{

// The button: the inner of the two on the gadget's lower edge (Misc/quakevr/make_gadget.py: at x 0.25, y -1.33,
// z -0.05 of the model, 0.2 wide), its face down the screen's up.
constexpr glm::vec3 buttonLocal{0.25f, -1.38f, -0.05f};
// The hit volume's cut: a fingertip counts only from the button's side of a plane this share of its radius behind its
// middle (never from over the screen: a tap on the screen's lower edge isn't a press).
constexpr float cutBehind = 0.5f;
constexpr float fadeTime = 0.15f; // real seconds the lights take to dim or come back

struct State
{
    bool pressing = false;     // the fingertip in the hit volume (a press is its arrival; it re-arms well out of it)
    double pressedAt = -1.0;   // realtime of the last press that counted (vr_gadget_button_cooldown)
    float level = -1.f;        // the lights' eased level, 0 (dimmed) .. 1 (on); -1: not yet (starts where the cvar is)
    double easedAt = -1.0;     // realtime it was eased last
};
State state;

[[nodiscard]] float target()
{
    return vr_gear_lights.value != 0.f ? 1.f : 0.f;
}

[[nodiscard]] float eased()
{
    return state.level < 0.f ? target() : state.level;
}

[[nodiscard]] glm::vec3 fingertip(const hands::State& s, int hand)
{
    return s.pos[hand] + hands::forward(s.rot[hand]) * (vr_gadget_button_reach.value * 0.01f * units::metresToUnits());
}

void buzz(int hand, float amplitude)
{
    if(Backend* const be = backend(); be && (hand == HAND_OFF || hand == HAND_MAIN))
    {
        be->haptic(hand, 0.025f, 200.f, amplitude);
    }
}

// The fingertip's arrival in the hit volume presses the button, unless one counted less than vr_gadget_button_cooldown
// ago or a screen tap is under way; it re-arms 1.5 radii out (or back behind the cut).
void pressButton(const hands::State& s, int hand)
{
    glm::vec3 at, out;
    float radius = 0.f;
    if(!button(at, out, radius))
    {
        state.pressing = false;
        return;
    }
    const glm::vec3 tip = fingertip(s, hand);
    const float d = glm::length(tip - at);
    const float side = glm::dot(tip - at, out);
    const bool in = d <= radius && side >= -cutBehind * radius;
    if(state.pressing && (d > radius * 1.5f || side < -cutBehind * radius * 2.f))
    {
        state.pressing = false;
    }
    if(state.pressing || !in)
    {
        return;
    }
    state.pressing = true;
    const double since = realtime - state.pressedAt;
    const bool tap = bullettime::tapping();
    if((state.pressedAt >= 0.0 && since < za::max(0.f, vr_gadget_button_cooldown.value)) || tap)
    {
        if(vr_debug_gadget_button.value)
        {
            Con_Printf("gadget button: press ignored (%s)\n", tap ? "a screen tap" : "cooling down");
        }
        return;
    }
    state.pressedAt = realtime;
    if(vr_debug_gadget_button.value)
    {
        Con_Printf("gadget button: pressed (fingertip %.1f cm from its middle)\n", d / units::metresToUnits() * 100.f);
    }
    toggle(hand);
}

// A ring of the hit volume's sphere: round `c` in the plane of `a` and `b` (unit, square to each other).
void ring(const glm::vec3& c, const glm::vec3& a, const glm::vec3& b, float r, const glm::vec4& colour)
{
    constexpr int steps = 24;
    constexpr float pi = 3.14159265f;
    for(int i = 0; i < steps; i++)
    {
        const float t0 = 2.f * pi * static_cast<float>(i) / steps;
        const float t1 = 2.f * pi * static_cast<float>(i + 1) / steps;
        lines::line(c + (a * za::cos(t0) + b * za::sin(t0)) * r, c + (a * za::cos(t1) + b * za::sin(t1)) * r, 0.05f,
            colour, colour);
    }
}

void toggle_f()
{
    toggle(-1);
}

void info_f()
{
    glm::vec3 at, out;
    float radius = 0.f;
    const hands::State& s = hands::current();
    const float m2u = units::metresToUnits();
    Con_Printf("gear lights: %s (light %.3f, screens %.3f)\n", vr_gear_lights.value != 0.f ? "on" : "off", light(),
        screen());
    if(button(at, out, radius))
    {
        Con_Printf("gadget button: at %.2f %.2f %.2f, radius %.2f units (%.1f cm), %s\n", at.x, at.y, at.z, radius,
            radius / m2u * 100.f, state.pressing ? "pressed" : "up");
        if(s.valid)
        {
            Con_Printf("gadget button: the other hand's fingertip %.1f cm from it\n",
                glm::length(fingertip(s, 1 - hands::gadgetHand()) - at) / m2u * 100.f);
        }
    }
    if(s.valid)
    {
        Con_Printf("gear lights: the stealth light on you %.2f\n", stealth::lightFresh(s.playerOrigin));
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_gear_lights_toggle", toggle_f);
    Cmd_AddCommand("vr_gear_lights_info", info_f);
}

void frame()
{
    // The level eased towards the cvar's (in real time: menus, bullet time and the recording's slow motion alike).
    const float goal = target();
    if(state.level < 0.f || state.easedAt < 0.0 || realtime < state.easedAt)
    {
        state.level = goal;
    }
    else
    {
        const float most = static_cast<float>(realtime - state.easedAt) / fadeTime;
        state.level = state.level < goal ? za::min(goal, state.level + most) : za::max(goal, state.level - most);
    }
    state.easedAt = realtime;

    const hands::State& s = hands::current();
    if(vr_gadget_button.value == 0.f || key_dest != key_game || !s.valid || cls.state != ca_connected ||
        cls.signon != SIGNONS)
    {
        state.pressing = false;
        return;
    }
    pressButton(s, 1 - hands::gadgetHand());
}

float light()
{
    return glm::mix(za::clamp(vr_gear_lights_dim.value, 0.f, 1.f), 1.f, eased());
}

float screen()
{
    return glm::mix(za::clamp(vr_gear_lights_screen_dim.value, 0.f, 1.f), 1.f, eased());
}

void toggle(int hand)
{
    const bool on = vr_gear_lights.value == 0.f;
    Cvar_SetValueQuick(&vr_gear_lights, on ? 1.f : 0.f);
    const cvar_t& sound = on ? vr_gear_lights_sound_on : vr_gear_lights_sound_off;
    if(sound.string && sound.string[0] && cls.state == ca_connected)
    {
        S_LocalSound(sound.string);
    }
    buzz(hand, 0.35f);
    buzz(hands::gadgetHand(), 0.2f);
    if(vr_debug_gadget_button.value)
    {
        Con_Printf("gear lights: %s\n", on ? "on" : "off");
    }
}

bool button(glm::vec3& at, glm::vec3& out, float& radius)
{
    const gadget::Pose& gp = gadget::pose();
    if(!gadget::active() || !gp.valid)
    {
        return false;
    }
    const float cm = 0.01f * units::metresToUnits();
    at = gp.origin + gp.axes * (buttonLocal * gp.scale) +
         gp.axes * glm::vec3{vr_gadget_button_x.value, vr_gadget_button_y.value, vr_gadget_button_z.value} * cm;
    out = -gp.axes[1];
    radius = za::max(0.3f, vr_gadget_button_size.value) * cm;
    return true;
}

bool buttonHandTarget(int hand, float units, glm::vec3& out)
{
    glm::vec3 at, face;
    float radius = 0.f;
    const hands::State& s = hands::current();
    if(!s.valid || !button(at, face, radius))
    {
        return false;
    }
    out = at + face * units - (fingertip(s, hand) - s.pos[hand]);
    return true;
}

void debugDraw()
{
    glm::vec3 at, out;
    float radius = 0.f;
    if(!vr_debug_gadget_button.value || !button(at, out, radius))
    {
        return;
    }
    const gadget::Pose& gp = gadget::pose();
    const bool cooling = state.pressedAt >= 0.0 && realtime - state.pressedAt < za::max(0.f, vr_gadget_button_cooldown.value);
    const glm::vec4 colour = cooling ? glm::vec4{1.f, 0.2f, 0.15f, 0.9f}
                             : state.pressing ? glm::vec4{1.f, 0.9f, 0.1f, 0.9f}
                                              : glm::vec4{0.2f, 1.f, 0.3f, 0.9f};
    const glm::vec3 x = gp.axes[0], y = gp.axes[1], z = gp.axes[2];
    ring(at, x, y, radius, colour);
    ring(at, x, z, radius, colour);
    ring(at, y, z, radius, colour);
    // The cut: a fingertip behind this disc (towards the screen) doesn't press.
    const float back = cutBehind * radius;
    ring(at - out * back, x, z, za::sqrt(za::max(0.f, radius * radius - back * back)), colour * glm::vec4{1.f, 1.f, 1.f, 0.5f});
    if(const hands::State& s = hands::current(); s.valid)
    {
        lines::point(fingertip(s, 1 - hands::gadgetHand()), 0.3f, colour);
    }

    // With 2, the screen tap's zone: its rectangle (with vr_bullettime_tap_margin) on the face and
    // vr_bullettime_tap_depth over it.
    bullettime::Screen sc;
    if(vr_debug_gadget_button.value < 2.f || !bullettime::screen(sc))
    {
        return;
    }
    const float cm = 0.01f * units::metresToUnits();
    const glm::vec2 half = sc.halfSize + glm::vec2{za::max(0.f, vr_bullettime_tap_margin.value) * cm};
    const float depth = za::max(0.5f, vr_bullettime_tap_depth.value) * cm;
    const glm::vec4 zone{0.3f, 0.7f, 1.f, 0.8f};
    glm::vec3 c[8];
    for(int i = 0; i < 8; i++)
    {
        c[i] = sc.centre + sc.right * ((i & 1) ? half.x : -half.x) + sc.up * ((i & 2) ? half.y : -half.y) +
               sc.normal * ((i & 4) ? depth : 0.f);
    }
    constexpr int edges[24] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 2, 1, 3, 4, 6, 5, 7, 0, 4, 1, 5, 2, 6, 3, 7};
    for(int e = 0; e < 24; e += 2)
    {
        lines::line(c[edges[e]], c[edges[e + 1]], 0.05f, zone, zone);
    }
}

} // namespace qvr::gearlights
