// vr_touch.cpp -- see vr_touch.hpp.

#include "vr_touch.hpp"

#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_gearlights.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::touch
{
namespace
{

struct State
{
    Request req;
    int requestedAt{-1};   // host_framecount of the last request
    bool tested{false};    // the last test had a screen
    Surface surface;       // the screen as last tested
    Contact contact;
    bool armed{true};      // the fingertip has been in front of the face since the last touch (a touch may begin)
    double upAt{-1.0};     // realtime the last touch ended
};
State state;

[[nodiscard]] float cm()
{
    return 0.01f * units::metresToUnits();
}

// The screen asked for, now: the gadget's (as placed this frame) or the app's panel.
[[nodiscard]] bool surfaceNow(Surface& out)
{
    if(state.requestedAt < host_framecount - 1)
    {
        return false;
    }
    if(!state.req.gadget)
    {
        out = state.req.panel;
        return out.halfSize.x > 0.f && out.halfSize.y > 0.f;
    }
    bullettime::Screen z;
    if(!bullettime::screen(z))
    {
        return false;
    }
    out.centre = z.centre;
    out.right = z.right;
    out.up = z.up;
    out.normal = z.normal;
    out.halfSize = z.halfSize;
    return true;
}

void buzz(int hand, float amplitude)
{
    if(Backend* const be = backend(); be && !vr_disablehaptics.value && amplitude > 0.f)
    {
        be->haptic(hand, 0.02f, 220.f, za::min(1.f, amplitude));
    }
}

} // namespace

void request(const Request& r)
{
    state.req = r;
    state.requestedAt = host_framecount;
}

bool takesGadgetScreen()
{
    return state.requestedAt >= host_framecount - 1 && state.req.gadget;
}

const Contact& contact()
{
    return state.contact;
}

bool lastSurface(Surface& out)
{
    out = state.surface;
    return state.tested;
}

void viewFrame(const hands::State& s)
{
    Contact& c = state.contact;
    c.pressed = c.released = false;
    Surface z;
    const int hand = 1 - hands::gadgetHand();
    state.tested = s.valid && surfaceNow(z);
    if(!state.tested)
    {
        if(c.down)
        {
            c.released = true; // (the screen went: the touch ends)
            c.down = false;
            state.upAt = realtime;
        }
        c.valid = c.hover = c.hard = false;
        state.armed = true;
        c.serial++;
        return;
    }
    state.surface = z;

    const float unit = cm();
    const glm::vec3 tip = gearlights::pressingFingertip(s, hand);
    const glm::vec3 rel = tip - z.centre;
    const float u = glm::dot(rel, z.right) / z.halfSize.x;  // -1 .. 1 across
    const float w = glm::dot(rel, z.up) / z.halfSize.y;     // -1 .. 1 up
    const float h = glm::dot(rel, z.normal) / unit;         // cm in front
    const float marginU = za::max(0.f, vr_touch_margin.value) * unit / z.halfSize.x;
    const float marginW = za::max(0.f, vr_touch_margin.value) * unit / z.halfSize.y;
    const bool over = za::fabs(u) <= 1.f + marginU && za::fabs(w) <= 1.f + marginW;
    const bool overWide = za::fabs(u) <= 1.f + 2.f * marginU && za::fabs(w) <= 1.f + 2.f * marginW;
    const float press = za::max(0.f, vr_touch_press.value);
    const float release = za::max(press + 0.1f, vr_touch_release.value);
    const float depth = za::max(0.5f, vr_touch_depth.value);

    c.valid = true;
    c.height = h;
    c.pos = {za::clamp((u * 0.5f + 0.5f) * state.req.pixels.x, 0.f, state.req.pixels.x - 0.01f),
        za::clamp((0.5f - w * 0.5f) * state.req.pixels.y, 0.f, state.req.pixels.y - 0.01f)};

    if(c.down)
    {
        if(h > release || h < -depth - 1.f || !overWide)
        {
            c.down = false;
            c.released = true;
            c.hard = false;
            state.upAt = realtime;
            state.armed = h > press; // (pulled back out: in front again; through or off the side: wait till it is)
        }
        else
        {
            c.hard = h < -za::max(0.1f, vr_touch_hard.value);
        }
    }
    else
    {
        if(h > press)
        {
            state.armed = true; // in front of the face: the next touch may come (not from behind, nor from the side)
        }
        const bool soon = state.upAt >= 0.0 && realtime - state.upAt < za::max(0.f, vr_touch_debounce.value);
        if(state.armed && over && h <= press && h >= -depth && !soon)
        {
            c.down = true;
            c.pressed = true;
            c.hard = false;
            c.downPos = c.pos;
            c.downAt = realtime;
            state.armed = false;
            buzz(hand, za::clamp(vr_touch_haptic.value, 0.f, 1.f));
        }
    }
    c.hover = !c.down && over && h > press && h <= za::max(press, vr_touch_hover.value);
    c.serial++;

    if(vr_debug_touch.value >= 2.f || (vr_debug_touch.value && (c.pressed || c.released)))
    {
        Con_Printf("touch: %s at %.0f %.0f, %.2f cm in front%s\n",
            c.pressed ? "down" : c.released ? "up" : c.down ? "held" : c.hover ? "hover" : "away", c.pos.x, c.pos.y, h,
            c.hard ? " (hard)" : "");
    }
}

bool handTarget(int hand, float x, float y, float cmOver, glm::vec3& out)
{
    Surface z;
    const hands::State& s = hands::current();
    if(!s.valid || !surfaceNow(z) || (hand != 0 && hand != 1))
    {
        return false;
    }
    const float u = (x / state.req.pixels.x - 0.5f) * 2.f;
    const float w = (0.5f - y / state.req.pixels.y) * 2.f;
    const glm::vec3 at = z.centre + z.right * (u * z.halfSize.x) + z.up * (w * z.halfSize.y) + z.normal * (cmOver * cm());
    // (The fingertip from where the hand was as last drawn: the hand moved on since with the player.)
    out = at - (gearlights::pressingFingertip(s, hand) - bullettime::movedSinceView(s, hand) - s.pos[hand]);
    return true;
}

// ---- Widgets ---------------------------------------------------------------------------------------------------------

void Ui::begin(const Contact& contactNow, glm::vec2 pixels, float drawAlpha)
{
    c = contactNow;
    size = pixels;
    alpha = drawAlpha;
    count = 0;
    fresh = c.serial != seen;
    seen = c.serial;
    if(!fresh)
    {
        c.pressed = c.released = false; // (each edge once)
    }
    if(c.pressed)
    {
        activeId = -2; // the first widget under the touch's start takes it (else it is loose)
    }
    else if(!c.down && !c.released)
    {
        activeId = -1;
    }
}

void Ui::add(const Drawn& d)
{
    if(count < maxDrawn)
    {
        drawn[count++] = d;
    }
}

Button Ui::button(int id, const Rect& r, const char* label, Look look)
{
    Button b;
    if(activeId == -2 && r.contains(c.downPos))
    {
        activeId = id;
        b.pressed = true;
    }
    const bool mine = activeId == id;
    b.held = mine && c.down;
    b.released = mine && c.released;
    b.clicked = b.released && r.contains(c.pos);
    b.hover = (c.hover || (c.down && mine)) && r.contains(c.pos);
    Drawn d;
    d.r = r;
    d.label = label;
    d.look = look;
    d.held = b.held;
    d.hover = b.hover;
    add(d);
    return b;
}

Stick Ui::stick(int id, const Rect& zone, float radius, const char* label)
{
    Stick k;
    if(activeId == -2 && zone.contains(c.downPos))
    {
        activeId = id;
    }
    const bool mine = activeId == id;
    k.held = mine && c.down;
    if(k.held)
    {
        glm::vec2 v = (c.pos - c.downPos) / za::max(1.f, radius);
        const float l = glm::length(v);
        k.value = l > 1.f ? v / l : v;
        k.hard = c.hard;
    }
    Drawn d;
    d.r = zone;
    d.label = label;
    d.look = Look::Quiet;
    d.held = k.held;
    d.hover = c.hover && zone.contains(c.pos);
    d.stick = true;
    d.from = c.downPos;
    d.to = c.downPos + k.value * radius;
    add(d);
    return k;
}

bool Ui::loose(Button& out) const
{
    out = Button{};
    if(activeId != -2 && activeId != 0)
    {
        return false;
    }
    // (A touch that began on no widget: the app's own.)
    const_cast<Ui*>(this)->activeId = 0;
    out.pressed = c.pressed;
    out.held = c.down;
    out.released = c.released;
    out.clicked = c.released;
    out.hover = c.hover;
    return c.down || c.released;
}

void Ui::draw() const
{
    if(alpha <= 0.f)
    {
        return;
    }
    // A widget: a dark plate (a light one held), a frame, its label in the middle; a held stick its line and knob.
    for(int i = 0; i < count; i++)
    {
        const Drawn& d = drawn[i];
        const bool lit = d.held || d.hover;
        const float a = alpha * (d.held ? 1.f : d.hover ? 0.85f : d.look == Look::Quiet ? 0.35f : 0.6f);
        const float plate[3] = {d.held ? 0.85f : d.look == Look::Accent ? 0.45f : 0.08f,
            d.held ? 0.75f : d.look == Look::On ? 0.4f : 0.08f, d.held ? 0.3f : 0.08f};
        const float edge[3] = {lit ? 1.f : 0.85f, lit ? 0.95f : 0.8f, lit ? 0.6f : 0.65f};
        if(d.look != Look::Quiet || lit)
        {
            Draw_FillEx(d.r.x, d.r.y, d.r.w, d.r.h, plate, a * 0.55f);
        }
        Draw_FillEx(d.r.x, d.r.y, d.r.w, 1.f, edge, a);
        Draw_FillEx(d.r.x, d.r.y + d.r.h - 1.f, d.r.w, 1.f, edge, a);
        Draw_FillEx(d.r.x, d.r.y, 1.f, d.r.h, edge, a);
        Draw_FillEx(d.r.x + d.r.w - 1.f, d.r.y, 1.f, d.r.h, edge, a);
        if(d.stick && d.held)
        {
            const float knob[3] = {1.f, 0.9f, 0.4f};
            Draw_FillEx(d.from.x - 3.f, d.from.y - 3.f, 6.f, 6.f, edge, alpha * 0.8f);
            const int steps = 8;
            for(int k = 1; k <= steps; k++)
            {
                const glm::vec2 p = d.from + (d.to - d.from) * (static_cast<float>(k) / steps);
                Draw_FillEx(p.x - 1.f, p.y - 1.f, 2.f, 2.f, knob, alpha * 0.8f);
            }
            Draw_FillEx(d.to.x - 5.f, d.to.y - 5.f, 10.f, 10.f, knob, alpha);
        }
        if(d.label && d.label[0])
        {
            const float size = 8.f;
            const float tw = size * static_cast<float>(strlen(d.label));
            gfx::draw2D::color({1.f, 1.f, d.held ? 0.6f : 1.f, za::min(1.f, a * 1.4f)});
            gfx::draw2D::text(d.r.x + (d.r.w - tw) * 0.5f, d.r.y + (d.r.h - size) * 0.5f, size, d.label);
            gfx::draw2D::color({1.f, 1.f, 1.f, 1.f});
        }
    }
    // The cursor: where the fingertip is over the screen (a ring of four ticks; filled while touching).
    if(c.valid && (c.hover || c.down))
    {
        const float col[3] = {1.f, 1.f, 1.f};
        const float r = c.down ? 3.f : 5.f;
        const float a = za::min(1.f, alpha * 1.5f) * (c.down ? 1.f : 0.8f);
        Draw_FillEx(c.pos.x - r - 2.f, c.pos.y - 0.5f, 3.f, 1.f, col, a);
        Draw_FillEx(c.pos.x + r - 1.f, c.pos.y - 0.5f, 3.f, 1.f, col, a);
        Draw_FillEx(c.pos.x - 0.5f, c.pos.y - r - 2.f, 1.f, 3.f, col, a);
        Draw_FillEx(c.pos.x - 0.5f, c.pos.y + r - 1.f, 1.f, 3.f, col, a);
        if(c.down)
        {
            Draw_FillEx(c.pos.x - 1.5f, c.pos.y - 1.5f, 3.f, 3.f, col, a);
        }
    }
}

} // namespace qvr::touch
