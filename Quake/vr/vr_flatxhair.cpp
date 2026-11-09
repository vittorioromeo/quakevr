// vr_flatxhair.cpp -- see vr_flatxhair.hpp.

#include "vr_flatxhair.hpp"
#include "vr_api.h"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN / HAND_COUNT
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_flycam.hpp"
#include "vr_gadget.hpp"
#include "vr_geom.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_hands.hpp"
#include "vr_move.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_trace.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace qvr::flatxhair
{
namespace
{

enum Style : int
{
    Dot = 0,
    Cross = 1,
    CrossDot = 2,
    Circle = 3,
    CircleDot = 4,
    Tee = 5,
    Chevron = 6,
    Brackets = 7,
    StyleCount
};

constexpr float shotRange = 2048.f;   // QC FireBullets: 2048 past the muzzle
constexpr float backNear = 96.f;      // QC VR_ShotBackPoint: a hand further than this off its muzzle shoots from the muzzle
constexpr float unsnapShare = 1.5f;   // a snapped place lets go of the centre only this far past the snap (no flicker at its edge)
constexpr float glowWidth = 5.f;      // the glow's reach past the lines at vr_flat_xhair_glow 1 (pixels at 1080)
constexpr float markLength = 0.55f;   // two crosshairs: each one's mark on its hand's side, this share of its size
constexpr int discSegments = 40;      // a circle or a dot, drawn as this many pieces round it
constexpr float blockedDim = 0.35f;   // a gun the wall stops from firing (vr_flat_wall_block_fire): its crosshair this dim

// The last move's shot line for each hand (noteMove).
struct ShotLine
{
    bool valid{false};
    glm::vec3 muzzle{0.f};
    glm::vec3 hand{0.f};
    glm::vec3 dir{0.f};
};

// One hand's crosshair this frame, in the view's pixels from its top left.
struct Aim
{
    bool shown{false};    // a gun in the hand: its crosshair is drawn
    bool had{false};      // `drawn` holds last frame's place
    bool snapped{false};  // on the centre
    bool blocked{false};  // its trigger is stopped (the wall)
    glm::vec3 hit{0.f};   // where its shot stops
    glm::vec2 raw{0.f};   // that, projected
    glm::vec2 drawn{0.f}; // where it is drawn (smoothed)
};

// Pellet ends since the last probe (noteTracer).
struct Pellets
{
    glm::vec3 sum{0.f};
    int count{0};
};

struct State
{
    ShotLine shot[HAND_COUNT];
    Aim aim[HAND_COUNT];
    Pellets pellets[HAND_COUNT];
    int frame{-1};
    bool merged{true};          // two guns drawn as one
    bool drawnAny{false};       // last frame drew one
    glm::vec2 centre{0.f};      // the crosshair's centre (the ironsight's picture moves it)
    glm::vec2 view{1.f};        // the view's size in pixels
    glm::mat4 viewProj{1.f};    // the window view's camera, last drawn
    za::Vector<gfx::Vertex> tris;
    za::Vector<gfx::Vertex> glows;
};
State st;

[[nodiscard]] float tune(const cvar_t& v, float lo, float hi)
{
    return v.value != v.value ? lo : za::clamp(v.value, lo, hi);
}

[[nodiscard]] bool playing()
{
    return flatplay::active() && vr_flat_xhair.value != 0.f && cls.state == ca_connected &&
           cls.signon == SIGNONS && cl.worldmodel && !cl.intermission && !CL_InCutscene() && scr_viewsize.value < 130.f &&
           cl.stats[STAT_HEALTH] > 0 && key_dest != key_menu && !cls.demoplayback;
}

// QC VR_ShotBackPoint: behind the muzzle on the shot's line, level with the hand (a wall between: the hand).
[[nodiscard]] glm::vec3 backPoint(const ShotLine& s)
{
    const float back = glm::dot(s.muzzle - s.hand, s.dir);
    if(back <= 0.f || glm::distance(s.muzzle, s.hand) > backNear)
    {
        return s.muzzle;
    }
    const glm::vec3 at = s.muzzle - s.dir * back;
    const auto tr = worldtrace::move(s.hand, glm::vec3{0.f}, glm::vec3{0.f}, at, MOVE_NOMONSTERS);
    if(tr && (tr->fraction < 1.f || tr->startsolid))
    {
        return s.hand;
    }
    return at;
}

// Where the shot stops (QC FireBullets: from the back point, 2048 past the muzzle; monsters' models, gibs and heads).
[[nodiscard]] glm::vec3 shotHit(const ShotLine& s)
{
    const glm::vec3 src = backPoint(s);
    const glm::vec3 end = src + s.dir * (shotRange + glm::distance(s.muzzle, src));
    const auto tr = worldtrace::move(src, glm::vec3{0.f}, glm::vec3{0.f}, end, MOVE_NORMAL | MOVE_HITGIBS | MOVE_HITMODEL);
    return tr ? worldtrace::endPos(*tr) : end;
}

// A world point on the view (pixels from its top left); false behind the eye.
[[nodiscard]] bool project(const glm::vec3& p, glm::vec2& out)
{
    const glm::vec4 c = st.viewProj * glm::vec4{p, 1.f};
    if(c.w <= 1e-3f)
    {
        return false;
    }
    out = {(c.x / c.w * 0.5f + 0.5f) * st.view.x, (0.5f - c.y / c.w * 0.5f) * st.view.y};
    return true;
}

// The hand holds a gun its crosshair is for: a weapon, not the fist, not one whose crosshair is forbidden (melee).
[[nodiscard]] bool gunIn(int h)
{
    const int slot = weapons::heldSlot(h);
    return slot >= 0 && slot != weapons::fistSlot() && weapons::value(slot, weapons::Key::CrosshairMode) != 1.f;
}

// Once a frame: each hand's hit, its place on the view, snapped and smoothed.
void update(int w, int h)
{
    if(st.frame == host_framecount)
    {
        return;
    }
    st.frame = host_framecount;
    st.view = {static_cast<float>(w), static_cast<float>(h)};
    st.viewProj = glm::make_mat4(r_matviewproj);
    float sx = 0.f, sy = 0.f;
    VR_FlatCrosshairShift(&sx, &sy); // (fractions of the height, x right, y down: the ironsight's picture moved)
    st.centre = st.view * 0.5f + glm::vec2{sx, sy} * st.view.y;

    const float px = st.view.y / 1080.f;
    const float snap = tune(vr_flat_xhair_snap, 0.f, 200.f) * px;
    const float tau = tune(vr_flat_xhair_smooth, 0.f, 0.5f);
    const float dt = za::clamp(static_cast<float>(host_frametime), 0.f, 0.1f);
    const float follow = tau > 0.f ? 1.f - za::exp(-dt / tau) : 1.f;
    for(int k = 0; k < HAND_COUNT; k++)
    {
        Aim& a = st.aim[k];
        a.shown = st.shot[k].valid && gunIn(k);
        if(!a.shown)
        {
            a.had = false;
            continue;
        }
        a.blocked = flatsight::wallBlocksFire(k);
        a.hit = shotHit(st.shot[k]);
        if(!project(a.hit, a.raw))
        {
            a.raw = st.centre;
        }
        const float off = glm::distance(a.raw, st.centre);
        a.snapped = off <= (a.snapped && a.had ? snap * unsnapShare : snap);
        const glm::vec2 want = glm::clamp(a.snapped ? st.centre : a.raw, glm::vec2{0.f}, st.view);
        a.drawn = a.had ? a.drawn + (want - a.drawn) * follow : want;
        a.had = true;
    }
    const Aim& l = st.aim[HAND_OFF];
    const Aim& r = st.aim[HAND_MAIN];
    if(l.shown && r.shown)
    {
        const float apart = glm::distance(l.drawn, r.drawn);
        st.merged = (l.snapped && r.snapped) || apart <= (st.merged ? snap * unsnapShare : snap);
    }
    else
    {
        st.merged = true;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing: in the view's pixels, each edge antialiased by its coverage (a one-pixel ramp across the true edge).

struct Ink
{
    glm::vec3 rgb{1.f};
    float alpha{1.f};
    bool glow{false}; // added (premultiplied with no alpha)
};

[[nodiscard]] gfx::Vertex vert(const glm::vec2& p, const Ink& ink, float a)
{
    const float alpha = ink.alpha * a;
    gfx::Vertex v;
    v.pos = {p.x / st.view.x * 2.f - 1.f, 1.f - p.y / st.view.y * 2.f, 0.f};
    v.color = {ink.rgb * alpha, ink.glow ? 0.f : alpha};
    return v;
}

void quad(za::Vector<gfx::Vertex>& out, const gfx::Vertex& a, const gfx::Vertex& b, const gfx::Vertex& c, const gfx::Vertex& d)
{
    out.pushBack(a);
    out.pushBack(b);
    out.pushBack(c);
    out.pushBack(a);
    out.pushBack(c);
    out.pushBack(d);
}

// A box about `c` along `u` (unit) and its perpendicular: solid within half sizes `in`, fading to nothing at `outer`.
void box(const glm::vec2& c, const glm::vec2& u, const glm::vec2& in, const glm::vec2& outer, const Ink& ink)
{
    za::Vector<gfx::Vertex>& out = ink.glow ? st.glows : st.tris;
    const glm::vec2 v{-u.y, u.x};
    const float xs[4] = {-outer.x, -in.x, in.x, outer.x};
    const float ys[4] = {-outer.y, -in.y, in.y, outer.y};
    // (Thinner than the ramp: dimmer, as its coverage.)
    const float thin = za::min(1.f, za::min(outer.x + in.x, outer.y + in.y));
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            const auto at = [&](int x, int y) {
                const float a = (x == 0 || x == 3 || y == 0 || y == 3) ? 0.f : thin;
                return vert(c + u * xs[x] + v * ys[y], ink, a);
            };
            quad(out, at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));
        }
    }
}

// A line from `a` to `b`, `width` across, square ends: its own edges ramped (`ramp` 1 pixel), or a glow `ramp` wide.
void bar(const glm::vec2& a, const glm::vec2& b, float width, float grow, float ramp, const Ink& ink)
{
    const glm::vec2 d = b - a;
    const float len = glm::length(d);
    const glm::vec2 u = len > 1e-4f ? d / len : glm::vec2{1.f, 0.f};
    const glm::vec2 half{len * 0.5f + grow, width * 0.5f + grow};
    if(ink.glow)
    {
        box((a + b) * 0.5f, u, half, half + ramp, ink);
        return;
    }
    box((a + b) * 0.5f, u, glm::max(half - ramp * 0.5f, glm::vec2{0.f}), half + ramp * 0.5f, ink);
}

// A ring about `c` from radius `r0` to `r1` (0: a disc), with the same edges.
void ring(const glm::vec2& c, float r0, float r1, float grow, float ramp, const Ink& ink)
{
    za::Vector<gfx::Vertex>& out = ink.glow ? st.glows : st.tris;
    r0 = r0 > 0.f ? za::max(0.f, r0 - grow) : 0.f;
    r1 += grow;
    // The radii across it and the opacity at each.
    float rs[4];
    float as[4];
    int n = 0;
    const float thin = za::min(1.f, (r1 - r0) / za::max(ramp, 1e-3f) + (r0 <= 0.f ? 1.f : 0.f));
    if(ink.glow)
    {
        if(r0 > 0.f)
        {
            rs[n] = za::max(0.f, r0 - ramp), as[n++] = 0.f;
        }
        rs[n] = r0, as[n++] = 1.f;
        rs[n] = r1, as[n++] = 1.f;
        rs[n] = r1 + ramp, as[n++] = 0.f;
    }
    else
    {
        if(r0 > 0.f)
        {
            rs[n] = za::max(0.f, r0 - ramp * 0.5f), as[n++] = 0.f;
            rs[n] = za::min(r0 + ramp * 0.5f, (r0 + r1) * 0.5f), as[n++] = thin;
        }
        else
        {
            rs[n] = 0.f, as[n++] = thin;
        }
        rs[n] = za::max(r1 - ramp * 0.5f, r0 > 0.f ? (r0 + r1) * 0.5f : 0.f), as[n++] = thin;
        rs[n] = r1 + ramp * 0.5f, as[n++] = 0.f;
    }
    for(int s = 0; s < discSegments; s++)
    {
        const float t0 = 2.f * glm::pi<float>() * static_cast<float>(s) / discSegments;
        const float t1 = 2.f * glm::pi<float>() * static_cast<float>(s + 1) / discSegments;
        const glm::vec2 e0{za::cos(t0), za::sin(t0)}, e1{za::cos(t1), za::sin(t1)};
        for(int i = 0; i + 1 < n; i++)
        {
            quad(out, vert(c + e0 * rs[i], ink, as[i]), vert(c + e1 * rs[i], ink, as[i]), vert(c + e1 * rs[i + 1], ink, as[i + 1]),
                vert(c + e0 * rs[i + 1], ink, as[i + 1]));
        }
    }
}

// The shape about `c` in one ink: grown by `grow` (the outline's), ramped by `ramp` (1 pixel; the glow's reach). `side`: -1
// the left hand's mark, 1 the right's, 0 none.
void shape(const glm::vec2& c, int style, float grow, float ramp, int side, const Ink& ink)
{
    const float px = st.view.y / 1080.f;
    const float size = tune(vr_flat_xhair_size, 1.f, 64.f) * px;
    const float thick = tune(vr_flat_xhair_thickness, 0.5f, 12.f) * px;
    const float gap = za::min(tune(vr_flat_xhair_gap, 0.f, 48.f) * px, size * 0.9f);
    const float dot = za::max(thick * 0.75f, 1.f);
    const auto arm = [&](const glm::vec2& way) { bar(c + way * gap, c + way * size, thick, grow, ramp, ink); };
    switch(style)
    {
        case Dot: ring(c, 0.f, za::max(dot, size * 0.25f), grow, ramp, ink); break;
        case Cross:
        case CrossDot:
            arm({1.f, 0.f});
            arm({-1.f, 0.f});
            arm({0.f, 1.f});
            arm({0.f, -1.f});
            if(style == CrossDot)
            {
                ring(c, 0.f, dot, grow, ramp, ink);
            }
            break;
        case Circle:
        case CircleDot:
            ring(c, za::max(size - thick, 0.f), size, grow, ramp, ink);
            if(style == CircleDot)
            {
                ring(c, 0.f, dot, grow, ramp, ink);
            }
            break;
        case Tee:
            arm({1.f, 0.f});
            arm({-1.f, 0.f});
            arm({0.f, 1.f});
            ring(c, 0.f, dot, grow, ramp, ink);
            break;
        case Chevron:
        {
            // A "^" whose tip is on the point, its legs down and out.
            const glm::vec2 left = glm::normalize(glm::vec2{-1.f, 1.2f}), right = glm::normalize(glm::vec2{1.f, 1.2f});
            const glm::vec2 tip = c + glm::vec2{0.f, gap * 0.5f};
            bar(tip, tip + left * size, thick, grow, ramp, ink);
            bar(tip, tip + right * size, thick, grow, ramp, ink);
            ring(c, 0.f, dot, grow, ramp, ink);
            break;
        }
        default: // Brackets: the four corners of a box `size` from the point, and the dot on it.
        {
            const float l = size * 0.45f;
            for(int sx = -1; sx <= 1; sx += 2)
            {
                for(int sy = -1; sy <= 1; sy += 2)
                {
                    const glm::vec2 k = c + glm::vec2{sx * size, sy * size};
                    bar(k, k - glm::vec2{sx * l, 0.f}, thick, grow, ramp, ink);
                    bar(k - glm::vec2{0.f, sy * (thick * 0.5f)}, k - glm::vec2{0.f, sy * l}, thick, grow, ramp, ink);
                }
            }
            ring(c, 0.f, dot, grow, ramp, ink);
            break;
        }
    }
    if(side != 0)
    {
        // Its hand's mark: a short upright line outside it on that side.
        const float x = static_cast<float>(side) * (size + gap + thick * 2.f);
        const float half = size * markLength * 0.5f;
        bar(c + glm::vec2{x, -half}, c + glm::vec2{x, half}, thick, grow, ramp, ink);
    }
}

void drawLists()
{
    const auto flush = [](za::Vector<gfx::Vertex>& v) {
        if(!v.empty())
        {
            gfx::draw({v.data(), v.size()}, glm::mat4{1.f},
                {.shade = gfx::Shade::Color, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false});
        }
        v.clear();
    };
    flush(st.glows);
    flush(st.tris);
}

// One crosshair at `c`: glow, outline, lines.
void one(const glm::vec2& c, int side, float dim)
{
    const gadget::VisorColors colors = gadget::visorColors();
    // The frame's green at full brightness (the visor's own line is a dim one, on a dark face).
    const float peak = za::max(za::max(colors.line.r, colors.line.g), za::max(colors.line.b, 1e-3f));
    const glm::vec3 lineRgb = glm::min(colors.line / peak, glm::vec3{1.f});
    const float opacity = tune(vr_flat_xhair_opacity, 0.f, 1.f) * dim * flatsight::crosshairFade();
    if(opacity <= 0.f)
    {
        return;
    }
    const int style = static_cast<int>(tune(vr_flat_xhair_style, 0.f, static_cast<float>(StyleCount - 1)));
    const float px = st.view.y / 1080.f;
    const float glow = tune(vr_flat_xhair_glow, 0.f, 2.f);
    const float outline = tune(vr_flat_xhair_outline, 0.f, 6.f) * px;
    if(glow > 0.f)
    {
        shape(c, style, outline, glowWidth * px * za::max(glow, 0.5f), side, {lineRgb, 0.35f * glow * opacity, true});
    }
    if(outline > 0.f)
    {
        shape(c, style, outline, 1.f, side, {colors.face * 0.5f, 0.75f * opacity, false});
    }
    drawLists(); // (the outline under the lines, whatever their order)
    shape(c, style, 0.f, 1.f, side, {lineRgb, opacity, false});
    drawLists();
}

void draw(unsigned framebuffer, int x, int y, int w, int h)
{
    update(w, h);
    st.drawnAny = false;
    const Aim& l = st.aim[HAND_OFF];
    const Aim& r = st.aim[HAND_MAIN];
    GL_BeginGroup("VR flat crosshair");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
    glViewport(x, y, w, h);
    if(l.shown && r.shown && !st.merged)
    {
        one(l.drawn, -1, l.blocked ? blockedDim : 1.f);
        one(r.drawn, 1, r.blocked ? blockedDim : 1.f);
    }
    else if(l.shown && r.shown)
    {
        one(l.snapped && r.snapped ? st.centre : (l.drawn + r.drawn) * 0.5f, 0, l.blocked && r.blocked ? blockedDim : 1.f);
    }
    else if(l.shown || r.shown)
    {
        const Aim& a = l.shown ? l : r;
        one(a.drawn, 0, a.blocked ? blockedDim : 1.f);
    }
    else
    {
        one(st.centre, 0, 1.f); // (no gun: where he looks, for the hands, the grab and the punch)
    }
    st.drawnAny = true;
    GL_EndGroup();
}

void probe_f()
{
    if(!flatplay::active())
    {
        Con_Printf("flatxhair: flatscreen play only\n");
        return;
    }
    // (Short lines: the console wraps long ones.)
    const float px = st.view.y / 1080.f;
    Con_Printf("flatxhair: on %d drawn %d merged %d style %d\n", playing() ? 1 : 0, st.drawnAny ? 1 : 0, st.merged ? 1 : 0,
        static_cast<int>(vr_flat_xhair_style.value));
    Con_Printf("flatxhair: view %.0f %.0f centre %.1f %.1f snap %.1f px\n", st.view.x, st.view.y, st.centre.x, st.centre.y,
        tune(vr_flat_xhair_snap, 0.f, 200.f) * px);
    // (Why not, when not: the mode's cvar, a menu, dead, an intermission, viewsize 130.)
    Con_Printf("flatxhair: why xhair %g menu %d health %d intermission %d viewsize %g\n", vr_flat_xhair.value,
        key_dest == key_menu ? 1 : 0, cl.stats[STAT_HEALTH], cl.intermission, scr_viewsize.value);
    for(int k = 0; k < HAND_COUNT; k++)
    {
        const Aim& a = st.aim[k];
        const char* name = k == HAND_MAIN ? "main" : "off";
        Con_Printf("flatxhair: %s shown %d snapped %d blocked %d drawn %.1f %.1f off %.1f\n", name, a.shown ? 1 : 0,
            a.snapped ? 1 : 0, a.blocked ? 1 : 0, a.drawn.x, a.drawn.y, glm::distance(a.drawn, st.centre));
        Con_Printf("flatxhair: %s raw %.1f %.1f off %.1f hit %.1f %.1f %.1f\n", name, a.raw.x, a.raw.y,
            glm::distance(a.raw, st.centre), a.hit.x, a.hit.y, a.hit.z);
        Pellets& p = st.pellets[k];
        if(p.count > 0)
        {
            // Where his pellets landed (their mean), on the view now, against the drawn crosshair and the centre.
            const glm::vec3 mean = p.sum / static_cast<float>(p.count);
            glm::vec2 at{0.f};
            const bool seen = project(mean, at);
            Con_Printf("flatxhair: %s pellets %d from drawn %.1f px from centre %.1f px\n", name, p.count,
                glm::distance(at, a.drawn), glm::distance(at, st.centre));
            Con_Printf("flatxhair: %s landed %.1f %.1f %.1f seen %d at %.1f %.1f from hit %.1f\n", name, mean.x, mean.y, mean.z,
                seen ? 1 : 0, at.x, at.y, glm::distance(mean, a.hit));
        }
        p = {};
    }
}

} // namespace

bool replacesCrosshair()
{
    return flatplay::active() && vr_flat_xhair.value != 0.f;
}

void noteMove(const VrMove& move)
{
    for(int k = 0; k < HAND_COUNT; k++)
    {
        ShotLine& s = st.shot[k];
        s.valid = true;
        s.muzzle = move.muzzlePos[k];
        s.hand = move.hands[k].pos;
        s.dir = hands::forward(move.shotRot[k]);
    }
}

void noteTracer(int hand, const glm::vec3& to)
{
    if(hand >= 0 && hand < HAND_COUNT)
    {
        st.pellets[hand].sum += to;
        st.pellets[hand].count++;
    }
}

void init()
{
    Cmd_AddCommand("vr_flat_xhair_probe", probe_f);
}

} // namespace qvr::flatxhair

using namespace qvr;

extern "C" int VR_FlatXhairOn(void)
{
    return flatxhair::replacesCrosshair() ? 1 : 0;
}

// R_RenderView, after the visor: the crosshair over the window's view (never an eye's, nor a teleporter's camera).
extern "C" void VR_DrawFlatCrosshair(unsigned framebuffer, int x, int y, int w, int h)
{
    if(stereo::isRenderingEye() || VR_PortalDrawing() || w < 16 || h < 16)
    {
        return;
    }
    if(!flatxhair::playing() || flycam::on())
    {
        flatxhair::st.drawnAny = false;
        return;
    }
    QVR_GPU_PROFILE("flat crosshair");
    flatxhair::draw(framebuffer, x, y, w, h);
}
