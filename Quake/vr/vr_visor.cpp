// vr_visor.cpp -- see vr_visor.hpp.

#include "vr_visor.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_gadget.hpp"
#include "vr_geom.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace qvr::visor
{
namespace
{

using gadget::VisorPanelCount;


// The sway (at vr_flat_visor_sway 1): the panels lag a turn by this share of it, at most `lagMost` degrees, a spring
// settling in about 0.2 s; they bob with the walk by at most `bobSide` and `bobUp` degrees at a run (320 units/s).
constexpr float lagShare = 0.25f;
constexpr float lagMost = 0.6f;
constexpr float springRate = 2.f * glm::pi<float>() / 0.3f;
constexpr float springDamping = 0.85f;
constexpr float bobRate = 1.15f; // cycles a second at a run (two steps each: the flat view's own bob, vr_flatplay.cpp)
constexpr float bobSide = 0.25f;
constexpr float bobUp = 0.2f;

// The glass's grid (the rim and the fog: smooth over the screen by its vertex colours).
constexpr int gridX = 32;
constexpr int gridY = 18;

struct Panel
{
    glm::vec4 rect{0.f}; // where its contents were drawn last: pixels from the view's top left (x0, y0, x1, y1)
    bool drawn{false};
    bool contact{false}; // its mount met the screen's edges with no gap (edgeContact)
};

struct State
{
    bool wasActive{false};
    int lastFrame{-1};
    float clock{0.f}; // seconds, the visor's own (host_frametime each frame)
    // The sway.
    bool haveAngles{false};
    float lastYaw{0.f}, lastPitch{0.f};
    glm::vec2 lag{0.f}, lagSpeed{0.f}; // degrees right and up, and their rates
    float bobPhase{0.f}, bobAmp{0.f};
    glm::vec2 sway{0.f}; // the offset drawn: lag and bob (degrees)
    // The effects.
    int lastHealth{0}, lastArmor{0};
    const void* lastWorld{nullptr};
    float bootAt{-100.f}; // the clock at the last power-on
    float glitch{0.f};    // a hit's glitch now (0..1)
    float fog{0.f};       // the fog now (0..1)
    bool underwater{false};
    float fade{1.f};      // the panels' (0 while dead)
    float alarm{0.f};     // the health panel's pulse now
    int fxDamage{0};      // vr_flat_visor_fx's: a hit to take next frame
    float fxFogHold{0.f}; // seconds the fog is held up (vr_flat_visor_fx fog)
    za::Array<Panel, VisorPanelCount> panels{};
    gadget::VisorInput input;
};
State state;

[[nodiscard]] float hash(unsigned n)
{
    n ^= n >> 16;
    n *= 0x7feb352dU;
    n ^= n >> 15;
    n *= 0x846ca68bU;
    n ^= n >> 16;
    return static_cast<float>(n & 0xffffff) / static_cast<float>(0x1000000);
}

using geom::smoothstep;

[[nodiscard]] float readHealth()
{
    return vr_gadget_test_state.value == 1.f ? 18.f : static_cast<float>(cl.stats[STAT_HEALTH]); // (the gadget's readings)
}

[[nodiscard]] bool eyeUnderwater()
{
    if(!r_viewleaf)
    {
        return false;
    }
    const int c = r_viewleaf->contents;
    return c == CONTENTS_WATER || c == CONTENTS_SLIME || c == CONTENTS_LAVA;
}

// Once a frame, with the view set up (R_RenderView): the clock, the sway and the effects.
void update()
{
    if(state.lastFrame == host_framecount)
    {
        return;
    }
    state.lastFrame = host_framecount;
    const float dt = za::clamp(static_cast<float>(host_frametime), 0.f, 0.1f);
    state.clock += dt;
    State& s = state;

    // A new map (or a save loaded), a spawn: the power-on sweep.
    const int health = static_cast<int>(readHealth());
    const int armor = cl.stats[STAT_ARMOR];
    if(s.lastWorld != cl.worldmodel || (s.lastHealth <= 0 && health > 0))
    {
        s.bootAt = s.clock;
        s.haveAngles = false;
        s.glitch = 0.f;
    }
    // A hit: health and armour lost (not a new map's), the glitch by how much.
    const int lost = s.lastWorld == cl.worldmodel && health > 0
                         ? za::max(0, s.lastHealth - health) + za::max(0, s.lastArmor - armor) + za::exchange(s.fxDamage, 0)
                         : za::exchange(s.fxDamage, 0);
    if(lost > 0)
    {
        const float peak = za::clamp(vr_flat_visor_damage.value, 0.f, 2.f) * za::clamp(0.3f + static_cast<float>(lost) / 40.f, 0.f, 1.f);
        s.glitch = za::max(s.glitch, za::min(peak, 0.85f));
    }
    s.glitch = za::max(0.f, s.glitch - dt / 0.4f);
    s.lastWorld = cl.worldmodel;
    s.lastHealth = health;
    s.lastArmor = armor;

    // Dead: the panels fade out (and power on again at the respawn).
    s.fade = health > 0 ? za::min(1.f, s.fade + dt / 0.3f) : za::max(0.f, s.fade - dt / 0.6f);

    // Low health: a pulse, faster and stronger at half the threshold.
    const float low = vr_flat_visor_low_health.value;
    if(low > 0.f && health > 0 && static_cast<float>(health) <= low)
    {
        const bool critical = static_cast<float>(health) <= low * 0.5f;
        const float rate = critical ? 2.2f : 1.2f;
        const float pulse = 0.5f + 0.5f * za::sin(s.clock * rate * 2.f * glm::pi<float>());
        s.alarm = (critical ? 0.55f + 0.45f * pulse : 0.2f + 0.4f * pulse);
    }
    else
    {
        s.alarm = 0.f;
    }

    // Under water the glass fogs up; it clears over vr_flat_visor_fog_clear seconds after surfacing.
    s.underwater = eyeUnderwater();
    s.fxFogHold = za::max(0.f, s.fxFogHold - dt);
    if(s.underwater || s.fxFogHold > 0.f)
    {
        s.fog = za::min(1.f, s.fog + dt / 0.5f);
    }
    else
    {
        s.fog = za::max(0.f, s.fog - dt / za::clamp(vr_flat_visor_fog_clear.value, 0.2f, 10.f));
    }

    // The sway: the turn's lag (a spring back to rest) and the walk's bob, both nothing standing still.
    const float sway = za::clamp(vr_flat_visor_sway.value, 0.f, 2.f);
    const float yaw = r_refdef.viewangles[YAW], pitch = r_refdef.viewangles[PITCH];
    if(s.haveAngles && sway > 0.f)
    {
        const float right = -geom::yawDelta(yaw, s.lastYaw); // (Quake's yaw grows turning left)
        const float up = -(pitch - s.lastPitch);           // (and its pitch looking down)
        const float most = lagMost * sway;
        s.lag.x = za::clamp(s.lag.x - right * lagShare * sway, -most, most);
        s.lag.y = za::clamp(s.lag.y - up * lagShare * sway, -most, most);
        for(int step = 0; step < 4; step++) // (a stiff spring: in steps)
        {
            const float h = dt / 4.f;
            const glm::vec2 accel = -springRate * springRate * s.lag - 2.f * springDamping * springRate * s.lagSpeed;
            s.lagSpeed += accel * h;
            s.lag += s.lagSpeed * h;
        }
        if(glm::dot(s.lag, s.lag) < 1e-8f && glm::dot(s.lagSpeed, s.lagSpeed) < 1e-6f)
        {
            s.lag = s.lagSpeed = glm::vec2{0.f};
        }
    }
    else
    {
        s.lag = s.lagSpeed = glm::vec2{0.f};
    }
    s.haveAngles = true;
    s.lastYaw = yaw;
    s.lastPitch = pitch;

    const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
    const float run = cl.onground ? za::clamp(speed / 320.f, 0.f, 1.f) : 0.f;
    s.bobAmp += (run - s.bobAmp) * za::min(1.f, dt * 6.f);
    if(s.bobAmp < 1e-3f && run <= 0.f)
    {
        s.bobAmp = 0.f;
    }
    s.bobPhase = za::fmod(s.bobPhase + dt * bobRate * za::max(speed / 320.f, 0.3f) * 2.f * glm::pi<float>(), 2.f * glm::pi<float>());
    const glm::vec2 bob = s.bobAmp * sway * glm::vec2{bobSide * za::sin(s.bobPhase), bobUp * za::sin(2.f * s.bobPhase)};
    s.sway = s.lag + bob;

    // The power-on sweep: each panel in turn.
    const float boot = vr_flat_visor_boot.value;
    for(int p = 0; p < VisorPanelCount; p++)
    {
        s.input.boot[p] = boot > 0.f ? za::clamp((s.clock - s.bootAt - boot * 0.05f * static_cast<float>(p)) / (boot * 0.7f), 0.f, 1.f) : 1.f;
    }
    s.input.alarm = s.alarm;
    s.input.log = vr_flat_visor_log.value != 0.f;
}

// The glass: a faint dark rim at the edges, the fog towards them, a reflection streak. In clip space (x, y -1..1).
void drawGlass(float aspect)
{
    const float rim = za::clamp(vr_flat_visor_frame.value, 0.f, 3.f);
    const float fog = state.fog * za::clamp(vr_flat_visor_fog.value, 0.f, 2.f);
    if(rim <= 0.f && fog <= 0.f)
    {
        return;
    }
    const glm::vec3 tint = glm::vec3{gadget::visorPhosphor()} * 0.04f;
    za::Array<gfx::Vertex, gridX * gridY * 6> tris{};
    for(int pass = 0; pass < 2; pass++)
    {
        const float k = pass == 0 ? rim : fog;
        if(k <= 0.f)
        {
            continue;
        }
        const auto at = [&](int i, int j) {
            const float u = -1.f + 2.f * static_cast<float>(i) / static_cast<float>(gridX - 1);
            const float v = -1.f + 2.f * static_cast<float>(j) / static_cast<float>(gridY - 1);
            // How far out (a squircle: 1 at the edges' middles, 1.19 in the corners).
            const float r = std::pow(std::pow(za::abs(u), 4.f) + std::pow(za::abs(v), 4.f), 0.25f);
            gfx::Vertex out{{u, v, 0.5f}};
            if(pass == 0)
            {
                out.color = {tint, k * 0.22f * smoothstep(0.8f, 1.18f, r)};
            }
            else
            {
                // Condensation: thicker in soft blotches (smooth waves across the glass), only towards the edges.
                const float n = 0.5f + 0.25f * za::sin(3.7f * u + 1.1f + 2.3f * za::sin(2.1f * v)) +
                                0.25f * za::sin(5.3f * v - 0.7f + 1.7f * za::sin(2.9f * u));
                out.color = {0.74f, 0.79f, 0.84f, za::min(0.6f, k * 0.34f * smoothstep(0.5f, 1.15f, r) * (0.55f + 0.45f * n))};
            }
            return out;
        };
        int n = 0;
        for(int j = 0; j + 1 < gridY; j++)
        {
            for(int i = 0; i + 1 < gridX; i++)
            {
                const gfx::Vertex a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
                tris[n++] = a, tris[n++] = b, tris[n++] = c, tris[n++] = a, tris[n++] = c, tris[n++] = d;
            }
        }
        gfx::draw({tris.data(), static_cast<za::SizeT>(n)}, glm::mat4{1.f},
            {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
    }
    if(rim > 0.f)
    {
        // The streak: a long soft glint across the glass's top left, and a thinner one beside it.
        for(int k = 0; k < 2; k++)
        {
            const glm::vec2 p0 = glm::vec2{-0.62f, 0.98f} + glm::vec2{0.07f, 0.f} * static_cast<float>(k);
            const glm::vec2 p1 = glm::vec2{-0.12f, 0.46f} + glm::vec2{0.07f, 0.f} * static_cast<float>(k);
            glm::vec2 along = (p1 - p0) * glm::vec2{aspect, 1.f}; // (in square units, so the width is even)
            const glm::vec2 side = glm::normalize(glm::vec2{-along.y, along.x}) * (k == 0 ? 0.025f : 0.01f) / glm::vec2{aspect, 1.f};
            const glm::vec4 c{1.f, 1.f, 1.f, rim * (k == 0 ? 0.035f : 0.025f)};
            const gfx::Vertex q[4] = {{{p0 - side, 0.5f}, {-1.f, -1.f}, c}, {{p1 - side, 0.5f}, {1.f, -1.f}, c},
                {{p1 + side, 0.5f}, {1.f, 1.f}, c}, {{p0 + side, 0.5f}, {-1.f, 1.f}, c}};
            const gfx::Vertex quad[6] = {q[0], q[1], q[2], q[0], q[2], q[3]};
            gfx::draw(quad, glm::mat4{1.f},
                {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Additive, .depthTest = false, .depthWrite = false});
        }
    }
}

#include "vr_visor_mounts.inc" // the mounts on the edges, the lens, the glass and metal look

// The panels' layer (supersampled: vr_flat_visor_ss) and the pass that brings it down onto the view, with the moving
// scanlines over the panels alone (vr_flat_visor_scanlines, _scanline_speed).
constexpr const char* layerVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";
// The view's pixel: the mean of its ss x ss layer texels (premultiplied); over what the panels cover, dark lines every
// Scan.y pixels drifting by Scan.z, and a faint bright band rolling down at Scan.w.
constexpr const char* layerFs = R"(#version 430
layout(binding = 0) uniform sampler2D Layer;
layout(location = 0) uniform vec4 Rect; // the view's corner in the target, the supersampling, the view's height
layout(location = 1) uniform vec4 Scan; // strength, period (pixels), drift (pixels), the band's height (pixels from the bottom)
layout(location = 0) out vec4 Out;
void main()
{
    vec2 p = gl_FragCoord.xy - Rect.xy;
    int ss = int(Rect.z);
    ivec2 base = ivec2(p) * ss;
    vec4 c = vec4(0.0);
    for (int j = 0; j < ss; j++)
        for (int i = 0; i < ss; i++)
            c += texelFetch(Layer, base + ivec2(i, j), 0);
    c /= float(ss * ss);
    if (Scan.x > 0.0 && c.a > 0.0)
    {
        float lines = 0.5 + 0.5 * cos((p.y + Scan.z) * 6.2831853 / Scan.y);
        float d = (p.y - Scan.w) / (0.05 * Rect.w);
        float band = exp(-d * d);
        c.rgb *= (1.0 - 0.6 * Scan.x * lines) * (1.0 + 1.2 * Scan.x * band);
    }
    Out = c;
}
)";

struct LayerPass
{
    GLuint program{0};
    bool failed{false};
    gfx::RenderTarget target;
    // Each panel's triangles, built again every frame (kept: nothing allocated once grown).
    za::Vector<gfx::Vertex> tris;
    za::Vector<gfx::Vertex> glows;
    Ring ring;
};
LayerPass layerPass;

[[nodiscard]] glm::vec4 premul(const glm::vec3& rgb, float a)
{
    return {rgb * a, a};
}

void drawList(za::Vector<gfx::Vertex>& v, gfx::Blend blend)
{
    if(!v.empty())
    {
        gfx::draw({v.data(), v.size()}, glm::mat4{1.f},
            {.shade = gfx::Shade::Color, .blend = blend, .depthTest = false, .depthWrite = false});
    }
    v.clear();
}

// The mounts and their contents (vr_flat_visor_panels: which), the sway moving them all, zoomed and through the lens.
// Into the layer (premultiplied): each panel's back plate, face, contents and glint, glows, frame and its highlight.
void drawPanels(int viewW, int viewH, float tx)
{
    Layer L;
    L.w = static_cast<float>(viewW);
    L.h = static_cast<float>(viewH);
    L.px1080 = L.h / 1080.f;
    const float scale = za::clamp(vr_flat_visor_scale.value, 0.5f, 2.f);
    // A virtual pixel: the vitals' 62 rows 11.5% of the view's height at scale 1; smaller where the bottom's three
    // mounts (527 across with their gaps) or the top's two (318) would not fit across.
    L.unit = za::min(za::min(static_cast<float>(viewH) * 0.115f * scale / 62.f, L.w / 527.f), L.w / 318.f);
    L.k = za::clamp(vr_flat_visor_warp.value, -0.2f, 0.2f);
    L.zoom = za::clamp(vr_flat_visor_zoom.value, 0.85f, 1.2f);
    L.overhang = {0.3f * L.w / L.unit, 0.3f * L.h / L.unit}; // (past 1 + k and the sway at every aspect, zoomed to 0.85)
    // The sway: degrees to pixels at this field of view, never more than most of the contents' inset.
    const glm::vec2 sway{glm::tan(glm::radians(state.sway.x)), glm::tan(glm::radians(state.sway.y))};
    const float most = 0.8f * mountInset * L.unit;
    L.shift = glm::clamp(glm::vec2{sway.x, -sway.y} / tx * (L.w * 0.5f), glm::vec2{-most}, glm::vec2{most});

    const float crt = za::clamp(vr_flat_visor_crt.value, 0.f, 2.f);
    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    const float g = state.glitch;
    const float flicker = g > 0.f ? 1.f - 0.45f * g * hash(static_cast<unsigned>(host_framecount) * 2654435761u) : 1.f;
    const glm::vec4 phosphor{glm::vec3{gadget::visorPhosphor()}, 0.f}; // (alpha 0: the image's face clear, the mount's)
    const gadget::VisorColors colors = gadget::visorColors();
    const float opacity = za::clamp(vr_flat_visor_opacity.value, 0.f, 1.f);
    const int shownMask = static_cast<int>(vr_flat_visor_panels.value);
    const float edge = za::clamp(vr_flat_visor_edge.value, 0.f, 8.f) * L.px1080;
    const float depth = za::clamp(vr_flat_visor_depth.value, 0.f, 3.f);
    const float bevel = za::clamp(vr_flat_visor_bevel.value, 0.f, 2.f);
    const float sheen = za::clamp(vr_flat_visor_sheen.value, 0.f, 3.f);
    const float glowOut = za::clamp(vr_flat_visor_glow_outer.value, 0.f, 2.f);
    const float glowIn = za::clamp(vr_flat_visor_glow_inner.value, 0.f, 2.f);
    const float glowWidth = za::clamp(vr_flat_visor_glow_width.value, 1.f, 60.f) * L.px1080;
    LayerPass& lp = layerPass;
    Ring& ring = lp.ring;

    for(int p = 0; p < VisorPanelCount; p++)
    {
        Panel& out = state.panels[p];
        out.drawn = false;
        const gadget::VisorImage img = gadget::visorImage(p);
        if(!(shownMask & (1 << p)) || !img.shown || img.size.y <= 0.f)
        {
            continue;
        }
        const float b = state.input.boot[p];
        const float stutter = b < 0.15f && hash(static_cast<unsigned>(host_framecount) * 7u + static_cast<unsigned>(p)) < 0.4f ? 0.35f : 1.f;
        const float fade = state.fade * flicker * stutter;
        const float mountFade = fade * smoothstep(0.f, 0.2f, b);
        if(fade <= 0.f)
        {
            continue;
        }
        const Mount m = mountOf(L, p, img.size);
        // The vitals' mount pulses red at low health (its face, its line and its glows).
        const float alarm = p == gadget::VisorVitals ? state.alarm : 0.f;
        const glm::vec3 faceRgb = glm::mix(colors.face, glm::vec3{0.16f, 0.015f, 0.01f}, za::clamp(alarm, 0.f, 1.f) * 0.85f);
        const glm::vec3 lineRgb = glm::mix(colors.line, colors.warn, za::clamp(alarm * 1.5f, 0.f, 1.f));
        // The contents' top and bottom as seen (the face's sheen runs down them).
        const glm::vec2 c0 = shownPx(L, localToPx(L, p, m.lo)), c1 = shownPx(L, localToPx(L, p, m.hi));
        const glm::vec2 topBottom{za::min(c0.y, c1.y), za::max(c0.y, c1.y)};

        // The back plate: a little behind (towards the view's middle, as a deeper thing seen from the eye) and lagging the
        // sway, darker, a dim line round it.
        if(depth > 0.f)
        {
            const MountPlace& place = mountPlaces[p];
            const glm::vec2 toMiddle = glm::normalize(glm::vec2{0.5f - place.anchor.x, 0.5f - place.anchor.y} * glm::vec2{L.w, L.h});
            ringOf(L, p, m, 1.f - 0.35f * depth, toMiddle * (3.f * depth * L.px1080), ring);
            face(L, ring, premul(faceRgb * 0.55f, opacity * 0.9f * mountFade), topBottom, 0.f, lp.tris);
            line(L, ring, 0.f, za::max(1.f, edge * 0.75f), premul(lineRgb * 0.35f, 0.8f * mountFade), 0.f, lp.tris);
            drawList(lp.tris, gfx::Blend::Premultiplied);
        }

        // The face, its sheen.
        ringOf(L, p, m, 1.f, glm::vec2{0.f}, ring);
        face(L, ring, premul(faceRgb, opacity * mountFade), topBottom, sheen, lp.tris);
        drawList(lp.tris, gfx::Blend::Premultiplied);

        // The contents, a little behind the glass (lagging the sway a touch), the glass's glint over them.
        drawContents(L, p, m, img,
            {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
                .params = {time, crt, g, gadget::textGlow()}, .screen = {img.size.x, img.size.y, 0.5f}, .trueColor = true,
                .inkAlpha = fade},
            phosphor, 1.f - 0.12f * depth, sheen * mountFade, out.rect);

        // The glows (added: premultiplied with no alpha), the frame lit from above and its inner highlight.
        const float half = edge * 0.5f;
        if(glowIn > 0.f)
        {
            band(L, ring, -half, -half - glowWidth, premul(lineRgb, 0.5f * glowIn * mountFade) * glm::vec4{1.f, 1.f, 1.f, 0.f},
                glm::vec4{0.f}, 0.f, lp.glows);
        }
        if(glowOut > 0.f)
        {
            band(L, ring, half, half + glowWidth, premul(lineRgb, 0.5f * glowOut * mountFade) * glm::vec4{1.f, 1.f, 1.f, 0.f},
                glm::vec4{0.f}, 0.f, lp.glows);
        }
        drawList(lp.glows, gfx::Blend::Premultiplied);
        if(edge > 0.f)
        {
            line(L, ring, 0.f, edge, premul(lineRgb, 0.9f * mountFade), 0.45f * bevel, lp.tris);
            if(bevel > 0.f)
            {
                const glm::vec3 lit = glm::mix(lineRgb, glm::vec3{1.f}, 0.6f);
                line(L, ring, -(half + 0.9f * L.px1080), za::max(0.8f, edge * 0.5f), premul(lit, 0.3f * bevel * mountFade),
                    0.8f * bevel, lp.tris);
            }
            drawList(lp.tris, gfx::Blend::Premultiplied);
        }
        out.contact = edgeContact(L, p, m);
        out.drawn = true;
    }
}

[[nodiscard]] bool ensureLayerProgram()
{
    if(layerPass.program || layerPass.failed)
    {
        return !layerPass.failed;
    }
    layerPass.program = gfx::glProgram(layerVs, layerFs, "vr visor layer");
    layerPass.failed = !layerPass.program;
    return !layerPass.failed;
}

void draw(unsigned framebuffer, int x, int y, int w, int h)
{
    if(w < 16 || h < 16)
    {
        return;
    }
    update();
    QVR_GPU_PROFILE("visor");
    GL_BeginGroup("VR visor");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
    glViewport(x, y, w, h);
    const float ty = glm::tan(glm::radians(za::clamp(r_refdef.fov_y, 10.f, 170.f)) * 0.5f);
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    drawGlass(aspect);

    // The panels into their layer at ss times the view's pixels (at most 8192 across), then down onto the view.
    int ss = za::clamp(static_cast<int>(vr_flat_visor_ss.value), 1, 4);
    while(ss > 1 && (w * ss > 8192 || h * ss > 8192))
    {
        ss--;
    }
    LayerPass& lp = layerPass;
    if(!ensureLayerProgram() || !lp.target.ensure(w * ss, h * ss, gfx::RenderTargetFormat{.internalFormat = GL_RGBA8}, "vr visor layer"))
    {
        GL_EndGroup();
        return;
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, lp.target.fbo);
    glViewport(0, 0, w * ss, h * ss);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawPanels(w, h, ty * aspect);

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
    glViewport(x, y, w, h);
    GL_UseProgram(lp.program);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // (premultiplied)
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, lp.target.tex);
    const float px = static_cast<float>(h) / 1080.f;
    const float scan = za::clamp(vr_flat_visor_scanlines.value, 0.f, 1.f);
    const float drift = static_cast<float>(za::fmod(realtime * static_cast<double>(vr_flat_visor_scanline_speed.value) * px, 100000.0));
    const float bandY = static_cast<float>(h) * (1.f - static_cast<float>(za::fmod(realtime * 0.12, 1.0))); // (rolling down)
    GL_Uniform4fFunc(0, static_cast<float>(x), static_cast<float>(y), static_cast<float>(ss), static_cast<float>(h));
    GL_Uniform4fFunc(1, scan, za::max(2.f, 3.f * px), drift, bandY);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // (what GLS_BLEND_ALPHA expects)
    GL_EndGroup();
}

void report_f()
{
    const State& s = state;
    Con_Printf("visor: active %d frame %d; sway %.3f %.3f (lag %.3f %.3f, bob %.3f); glitch %.3f fog %.3f underwater %d "
               "fade %.3f alarm %.3f boot %.2f %.2f %.2f; log lines %d\n",
        active() ? 1 : 0, host_framecount, s.sway.x, s.sway.y, s.lag.x, s.lag.y, s.bobAmp, s.glitch, s.fog,
        s.underwater ? 1 : 0, s.fade, s.alarm, s.input.boot[0], s.input.boot[3], s.input.boot[6], gadget::visorLogLines());
    constexpr const char* names[VisorPanelCount] = {"vitals", "status", "lefthand", "righthand", "log", "sideleft", "sideright"};
    for(int p = 0; p < VisorPanelCount; p++)
    {
        const Panel& q = s.panels[p];
        Con_Printf("visor: panel %s drawn %d rect %.1f %.1f %.1f %.1f contact %d (view %d x %d)\n", names[p], q.drawn ? 1 : 0,
            q.rect.x, q.rect.y, q.rect.z, q.rect.w, q.contact ? 1 : 0, r_refdef.vrect.width, r_refdef.vrect.height);
    }
}

// vr_flat_visor_fx damage <n> | boot | fog [seconds]: the effects on demand (the Debug menu's, the tests').
void fx_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(what, "damage"))
    {
        state.fxDamage = Cmd_Argc() > 2 ? za::max(1, Q_atoi(Cmd_Argv(2))) : 20;
    }
    else if(!q_strcasecmp(what, "boot"))
    {
        state.bootAt = state.clock;
    }
    else if(!q_strcasecmp(what, "fog"))
    {
        state.fxFogHold = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), 0.f, 30.f) : 1.f;
    }
    else
    {
        Con_Printf("vr_flat_visor_fx damage [amount] | boot | fog [seconds]: the visor's hit glitch, its power-on sweep, its "
                   "fog held up that long (then clearing)\n");
    }
}

} // namespace

bool active()
{
    return flatplay::active() && vr_flat_visor.value != 0.f && cls.state == ca_connected && cls.signon == SIGNONS &&
           cl.worldmodel && !cl.intermission && scr_viewsize.value < 120.f && !cl.qcvm.extfuncs.CSQC_DrawHud;
}

bool logShown()
{
    return active() && vr_flat_visor_log.value != 0.f;
}

void renderPanels()
{
    const bool on = active();
    if(on != za::exchange(state.wasActive, on))
    {
        vid.recalc_refdef = 1; // (the classic status bar's lines: SCR_CalcRefdef)
    }
    if(on)
    {
        gadget::renderVisor(state.input);
    }
}

void init()
{
    Cmd_AddCommand("vr_flat_visor_report", report_f);
    Cmd_AddCommand("vr_flat_visor_fx", fx_f);
}

} // namespace qvr::visor

using namespace qvr;

extern "C" int VR_VisorOn(void)
{
    return visor::active() ? 1 : 0;
}

// R_RenderView, after the window's view and its look: the visor over it, into `framebuffer` at (x, y, w, h); the window's
// own view only (never an eye's, nor a teleporter's offscreen camera).
extern "C" void VR_DrawVisor(unsigned framebuffer, int x, int y, int w, int h)
{
    if(!visor::active() || stereo::isRenderingEye() || VR_PortalDrawing())
    {
        return;
    }
    visor::draw(framebuffer, x, y, w, h);
}
