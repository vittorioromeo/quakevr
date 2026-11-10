// vr_visor.cpp -- see vr_visor.hpp.

#include "vr_visor.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_diskcache.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatholster.hpp"
#include "vr_gadget.hpp"
#include "vr_geom.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Container/Vector.hpp"
#include "vr_zancle.hpp"
#include "vr_api.h"
#include "vr_api_render.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>

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
    glm::vec4 rect{0.f}; // where its contents are seen at rest: pixels from the view's top left (x0, y0, x1, y1)
    bool drawn{false};   // composited this frame
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
    int lastHealth{0};
    const void* lastWorld{nullptr};
    float bootAt{-100.f}; // the clock at the last power-on
    float glitch{0.f};    // a hit's glitch now (0..1)
    float fog{0.f};       // the fog now (0..1)
    bool underwater{false};
    float fade{1.f};      // the panels' (0 while dead)
    float alarm{0.f};     // the health panel's pulse now
    int fxDamage{0};      // vr_flat_visor_fx's: a hit to take next frame
    int hitDamage{0};     // the damage messages' since the last frame (onDamage: armor and blood)
    int hits{0};          // how many hits shook it (vr_flat_visor_report, the test)
    float fxFogHold{0.f}; // seconds the fog is held up (vr_flat_visor_fx fog)
    // A hit's shake (vr_flat_visor_shake): its strength at the hit, when (the clock), its wobble's phases.
    float shakePeak{0.f};
    float shakeAt{-100.f};
    unsigned shakeSeed{0};
    glm::vec2 shake{0.f}; // the offset now (degrees, in the sway)
    float logFade{1.f};   // the low panel's (vr_flat_visor_autohide_log: 0 while it has no lines)
    // The centre print's popup (vr_flat_visor_centerprint): the frame the 2D pass last left the game's centre print to it
    // (VR_CenterPrintOnVisor: Quake's own timing, scr_centertime, decides which), Quake's fade then, the popup's fade now,
    // when it last popped in (the clock: a new text), how many times, its text.
    struct CenterPrint
    {
        int claimed{-100};
        float alpha{0.f};
        float fade{0.f};
        float popAt{-100.f};
        int pops{0};
        za::Array<char, 1024> text{};
    } center;
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

// A hit's shake (vr_flat_visor_shake): its strength now, easing out over vr_flat_visor_shake_time seconds.
[[nodiscard]] float shakeLeft(const State& s)
{
    const float t = (s.clock - s.shakeAt) / za::clamp(vr_flat_visor_shake_time.value, 0.1f, 2.f);
    return t >= 0.f && t < 1.f ? s.shakePeak * (1.f - t) * (1.f - t) : 0.f;
}

// Its offset now (degrees): two wobbles a direction, a dozen or two a second, their phases the hit's own; at most 0.45
// degrees at strength 1 (a big hit), never more than 1.2 (the panels' readings still read: the sway's own clamp after).
[[nodiscard]] glm::vec2 shakeOffset(const State& s)
{
    const float k = shakeLeft(s);
    if(k <= 0.f)
    {
        return glm::vec2{0.f};
    }
    const float t = s.clock - s.shakeAt, tau = 2.f * glm::pi<float>();
    const auto phase = [&](unsigned i) { return tau * hash(s.shakeSeed + i * 0x9e3779b9u); };
    const glm::vec2 wobble{0.75f * za::sin(tau * 12.5f * t + phase(1)) + 0.25f * za::sin(tau * 23.f * t + phase(2)),
        0.75f * za::sin(tau * 10.5f * t + phase(3)) + 0.25f * za::sin(tau * 19.f * t + phase(4))};
    const glm::vec2 out = wobble * (0.45f * k);
    const float len = glm::length(out);
    return len > 1.2f ? out * (1.2f / len) : out;
}

// The centre print's popup's pop in (0 .. 1: done; vr_flat_visor_center_pop seconds from a new text).
[[nodiscard]] float centerPop()
{
    const float popTime = za::clamp(vr_flat_visor_center_pop.value, 0.f, 1.f);
    return popTime > 0.f ? za::clamp((state.clock - state.center.popAt) / popTime, 0.f, 1.f) : 1.f;
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
    if(s.lastWorld != cl.worldmodel || (s.lastHealth <= 0 && health > 0))
    {
        s.bootAt = s.clock;
        s.haveAngles = false;
        s.glitch = 0.f;
    }
    // A hit: the server's damage messages' armour and blood (onDamage; not health going down by itself, as a megahealth's
    // overheal ticking away, nor a new map's), the glitch by how much.
    const int taken = za::exchange(s.hitDamage, 0);
    const int lost = (s.lastWorld == cl.worldmodel && health > 0 ? taken : 0) + za::exchange(s.fxDamage, 0);
    if(lost > 0)
    {
        s.hits++;
        const float peak = za::clamp(vr_flat_visor_damage.value, 0.f, 2.f) * za::clamp(0.3f + static_cast<float>(lost) / 40.f, 0.f, 1.f);
        s.glitch = za::max(s.glitch, za::min(peak, 0.85f));
        // The shake: the stronger of this hit's and what is left of the last.
        const float kick = za::clamp(vr_flat_visor_shake.value, 0.f, 6.f) * za::clamp(0.25f + static_cast<float>(lost) / 50.f, 0.f, 1.f);
        if(kick > 0.f && kick >= shakeLeft(s))
        {
            s.shakePeak = kick;
            s.shakeAt = s.clock;
            s.shakeSeed = static_cast<unsigned>(host_framecount) * 2246822519u + static_cast<unsigned>(lost);
        }
    }
    s.glitch = za::max(0.f, s.glitch - dt / 0.4f);
    s.lastWorld = cl.worldmodel;
    s.lastHealth = health;

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
    s.shake = shakeOffset(s);
    s.sway = s.lag + bob + s.shake;

    // The low panel fades away while it has no lines (the last frame's: the gadget draws them after the view), and back
    // at once when a message arrives.
    const bool logEmpty = gadget::visorLogLines() <= 0;
    s.logFade = vr_flat_visor_autohide_log.value != 0.f && logEmpty ? za::max(0.f, s.logFade - dt / 0.5f)
                                                                     : za::min(1.f, s.logFade + dt / 0.15f);

    // The power-on sweep: each panel in turn.
    const float boot = vr_flat_visor_boot.value;
    for(int p = 0; p < VisorPanelCount; p++)
    {
        s.input.boot[p] = boot > 0.f ? za::clamp((s.clock - s.bootAt - boot * 0.05f * static_cast<float>(p)) / (boot * 0.7f), 0.f, 1.f) : 1.f;
    }
    s.input.alarm = s.alarm;
    s.input.log = vr_flat_visor_log.value != 0.f;

    // The centre print's popup: up (vr_flat_visor_center_pop) while the 2D pass leaves the centre print to it (Quake's
    // own fade, con_notifyfade, followed), down (vr_flat_visor_center_fade) once it stops (its time up, the console or the
    // menu up, paused); a new text pops in again (the scale, centerPop, and the power-on scan).
    State::CenterPrint& c = s.center;
    const bool claimed = c.claimed >= host_framecount - 1;
    const float popTime = za::clamp(vr_flat_visor_center_pop.value, 0.f, 1.f);
    const float fadeTime = za::clamp(vr_flat_visor_center_fade.value, 0.f, 1.f);
    const float target = claimed ? za::clamp(c.alpha, 0.f, 1.f) : 0.f;
    if(target > c.fade)
    {
        c.fade = popTime > 0.f ? za::min(target, c.fade + dt / popTime) : target;
    }
    else
    {
        c.fade = claimed || fadeTime <= 0.f ? target : za::max(target, c.fade - dt / fadeTime);
    }
    s.input.boot[gadget::VisorCenter] = za::min(s.input.boot[gadget::VisorCenter], centerPop());
    s.input.center = c.fade > 0.f ? c.text.data() : "";
}

// The glass's rim and fog triangles as last built (drawGlass).
struct GlassGrid
{
    float rim{-1.f}, fog{-1.f};
    glm::vec3 tint{-1.f};
    za::Array<za::Array<gfx::Vertex, gridX * gridY * 6>, 2> tris{};
    za::Array<int, 2> count{};
};
GlassGrid glassGrid;

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
    // (its triangles kept while the rim, the fog and the tint stay: built again only while the fog comes and goes)
    GlassGrid& grid = glassGrid;
    const bool fresh = rim == grid.rim && fog == grid.fog && tint == grid.tint;
    grid.rim = rim, grid.fog = fog, grid.tint = tint;
    for(int pass = 0; pass < 2; pass++)
    {
        const float k = pass == 0 ? rim : fog;
        if(k <= 0.f)
        {
            continue;
        }
        za::Array<gfx::Vertex, gridX * gridY * 6>& tris = grid.tris[pass];
        if(fresh)
        {
            gfx::draw({tris.data(), static_cast<za::SizeT>(grid.count[pass])}, glm::mat4{1.f},
                {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
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
                if(a.color.a <= 0.f && b.color.a <= 0.f && c.color.a <= 0.f && d.color.a <= 0.f)
                {
                    continue; // (the glass's clear middle: nothing to blend)
                }
                tris[n++] = a, tris[n++] = b, tris[n++] = c, tris[n++] = a, tris[n++] = c, tris[n++] = d;
            }
        }
        grid.count[pass] = n;
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

// The panels' layer and the pass that brings it onto the view. Each panel's depths are drawn unwarped and at rest (no
// sway) into an atlas, vr_flat_visor_ss texels a layer pixel, only when what they show changes; per pixel, the
// composite applies the lens, the zoom and the sway (each depth its own share of it) and the fades, with the moving
// scanlines over the panels alone (vr_flat_visor_scanlines, _scanline_speed, _scanline_width) and the frosted glass behind
// the mounts (vr_flat_visor_glass_*).
constexpr const char* layerVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";
// One panel's pixels (scissored to where it can be). Each view pixel's layer point (layerOf: the lens and the zoom
// undone; ceil(ss / 2)^2 of them, each a bilinear tap: at ss 2 the mean of the 2 x 2 texels it covers), each depth's
// slot read there less its sway, composited back to front (premultiplied) and faded. Over what the panels cover, dark
// lines every Scan.y pixels drifting by Scan.z, and a faint bright band rolling down at Scan.w. Under it, where the mounts
// are (the shapes' slot), the view behind blurred (Glass), bent by a ripple through the glass and towards the mounts'
// edges like a lens rim, drained of colour and tinted. Where the shapes' b is 0 (nothing of the panel can be there:
// most of its box), nothing. SlotC.w 1: the panels' alpha alone (bullet time's mask: no glass).
constexpr const char* layerFs = R"(#version 430
layout(binding = 0) uniform sampler2D Atlas;
layout(binding = 1) uniform sampler2D Glass;
layout(location = 0) uniform vec4 Rect;    // the view's corner in the target, its width and height
layout(location = 1) uniform vec4 Scan;    // strength, period (pixels), drift (pixels), the band's height (pixels from the bottom)
layout(location = 2) uniform vec4 GlassP;  // on (0: no glass), its bend (pixels), its tint, its colour drained
layout(location = 3) uniform vec4 GlassC;  // the tint's colour, how far in from the mounts' edges they bend it (pixels)
layout(location = 4) uniform vec4 Lens;    // the layer's width and height (the view's), the lens (k), the zoom
layout(location = 5) uniform vec4 Box;     // the panel's box in the layer (pixels, y down: x, y, w, h)
layout(location = 6) uniform vec4 SlotA;   // its slots' corners in the atlas (texels: the box's bottom left): the back plate's, the face's
layout(location = 7) uniform vec4 SlotB;   // the contents', what lies over them (glint, glows, frame)
layout(location = 8) uniform vec4 SlotC;   // the glass's shapes' (r the back plate, g the face), the contents' and over's texels a layer pixel (the rest: 1), 1: alpha only
layout(location = 9) uniform vec4 ShiftA;  // the sway (layer pixels): the back plate's, the contents'
layout(location = 10) uniform vec4 ShiftB; // the face's (and what lies over it), the mount's fade, the contents' fade
layout(location = 11) uniform float InkScan; // the scanlines' strength over the contents' ink, times Scan.x (vr_flat_visor_ink_crt)
layout(location = 12) uniform vec4 Pop;      // the panel grown from its middle (layer pixels: x, y) by 1 / z (the centre print's pop in; 1 the rest)
layout(location = 0) out vec4 Out;
vec2 layerOf(vec2 v)
{
    vec2 n = vec2(v.x / Lens.x * 2.0 - 1.0, 1.0 - v.y / Lens.y * 2.0);
    n *= 1.0 + Lens.z * (dot(n, n) - 1.0);
    vec2 mid = 0.5 * Lens.xy;
    vec2 q = mid + (vec2((n.x + 1.0) * mid.x, (1.0 - n.y) * mid.y) - mid) / Lens.w;
    return Pop.xy + (q - Pop.xy) * Pop.z;
}
vec4 slot(vec2 corner, vec2 q, float scale)
{
    vec2 t = vec2(q.x - Box.x, Box.y + Box.w - q.y);
    if (any(lessThan(t, vec2(-1.0))) || any(greaterThan(t, Box.zw + 1.0)))
        return vec4(0.0);
    return textureLod(Atlas, (corner + t * scale) / vec2(textureSize(Atlas, 0)), 0.0);
}
float shapes(vec2 q)
{
    float plate = slot(SlotC.xy, q - ShiftA.xy, 1.0).r, face = slot(SlotC.xy, q - ShiftB.xy, 1.0).g;
    return ShiftB.z * (face + (1.0 - face) * plate);
}
// (its gradient's: both read with the face's sway, one tap -- exact at rest, a few pixels off for the back plate's
// edges while it lags)
float shapesNear(vec2 q)
{
    vec2 s = slot(SlotC.xy, q - ShiftB.xy, 1.0).rg;
    return s.g + (1.0 - s.g) * s.r;
}
void main()
{
    vec2 p = gl_FragCoord.xy - Rect.xy;
    vec2 v = vec2(p.x, Rect.w - p.y); // (view pixels, y down)
    if (slot(SlotC.xy, layerOf(v) - ShiftB.xy, 1.0).b <= 0.0)
        discard; // (nothing of the panel can be here)
    int n = (int(SlotC.z) + 1) / 2;
    vec4 c = vec4(0.0);
    float ink = 0.0; // (the contents' coverage: their glyphs keep most of their light under the scanlines)
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
        {
            vec2 q = layerOf(v + (vec2(i, j) + 0.5) / float(n) - 0.5);
            vec4 s = slot(SlotA.xy, q - ShiftA.xy, 1.0) * ShiftB.z;
            vec4 f = slot(SlotA.zw, q - ShiftB.xy, 1.0) * ShiftB.z;
            s = f + (1.0 - f.a) * s;
            f = slot(SlotB.xy, q - ShiftA.zw, SlotC.z) * ShiftB.w;
            ink += f.a;
            s = f + (1.0 - f.a) * s;
            f = slot(SlotB.zw, q - ShiftB.xy, SlotC.z) * ShiftB.z;
            c += f + (1.0 - f.a) * s;
        }
    c /= float(n * n);
    if (SlotC.w > 0.5)
    {
        Out = c;
        return;
    }
    if (Scan.x > 0.0 && c.a > 0.0)
    {
        float lines = 0.5 + 0.5 * cos((p.y + Scan.z) * 6.2831853 / Scan.y);
        float d = (p.y - Scan.w) / (0.05 * Rect.w);
        float band = exp(-d * d);
        float k = Scan.x * mix(1.0, InkScan, clamp(ink / float(n * n), 0.0, 1.0));
        c.rgb *= (1.0 - 0.6 * k * lines) * (1.0 + 1.2 * k * band);
    }
    if (GlassP.x > 0.0)
    {
        vec2 q = layerOf(v);
        float m = shapes(q);
        if (m > 0.0)
        {
            float e = GlassC.w / Lens.w;
            vec2 g = ShiftB.z * vec2(shapesNear(q + vec2(e, 0.0)) - shapesNear(q - vec2(e, 0.0)),
                shapesNear(q - vec2(0.0, e)) - shapesNear(q + vec2(0.0, e)));
            vec2 size = Rect.zw;
            vec2 uv = p / size;
            vec2 r = p * (1080.0 / Rect.w);
            vec2 ripple = vec2(sin(r.y * 0.071 + 1.3 * sin(r.x * 0.023)), sin(r.x * 0.064 + 1.7 * sin(r.y * 0.029)));
            vec3 b = texture(Glass, uv + GlassP.y * (2.0 * ripple - 8.0 * g) / size).rgb;
            float luma = dot(b, vec3(0.299, 0.587, 0.114));
            b = mix(b, vec3(luma), GlassP.w);
            b = mix(b, luma * GlassC.rgb, GlassP.z);
            c.rgb += (1.0 - c.a) * m * b;
            c.a += (1.0 - c.a) * m;
        }
    }
    Out = c;
}
)";
// The glass's blur: one Kawase pass, four bilinear taps P.z texels out diagonally (a texel: P.xy).
constexpr const char* blurFs = R"(#version 430
layout(binding = 0) uniform sampler2D Src;
layout(location = 0) uniform vec4 P;
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = gl_FragCoord.xy * P.xy;
    vec2 o = P.xy * P.z;
    Out = 0.25 * (texture(Src, uv + o) + texture(Src, uv - o) + texture(Src, uv + vec2(o.x, -o.y)) +
        texture(Src, uv + vec2(-o.x, o.y)));
}
)";

// Each panel's depths, composited back to front: the atlas's slots, each the panel's box. The back plate (its share of
// the sway: vr_flat_visor_depth_lag), the face, the contents (vr_flat_visor_depth_content_lag), what lies over them with
// the face's sway (the glint, the glows, the frame), and the glass's shapes (r the back plate's, g the face's).
enum Slot
{
    SlotPlate,
    SlotFace,
    SlotContents,
    SlotOver,
    SlotShapes,
};
constexpr int SlotCount = 5;
constexpr int atlasWidth = 8192;

// A slot's texels a layer pixel at vr_flat_visor_ss: the contents and what lies over them (text, the frame's lines) at it;
// the back plate, the face and the shapes (flat fills, their edges under the lines) at one (a quarter of the reads).
[[nodiscard]] int slotScale(int s, int ss)
{
    return s == SlotContents || s == SlotOver ? ss : 1;
}

struct PanelLayer
{
    bool built{false};
    glm::vec4 box{0.f};                      // in the layer at rest (whole pixels, y down: x, y, w, h)
    za::Array<glm::ivec2, SlotCount> slot{}; // each slot's corner in the atlas (texels: the box's bottom left)
    glm::vec4 screen{0.f};                   // the view's pixels it covers at rest, the lines' reach round it (x0, y0, x1, y1)
    Mount mount;
    bool hasPlate{false};
    Ring plate, face; // (its outlines: laid out, then drawn once the atlas is packed)
};

struct LayerPass
{
    GLuint program{0};
    bool failed{false};
    gfx::RenderTarget atlas;
    gfx::RenderTarget tint; // bullet time's mask (tintMask): the panels' alpha over the view, while it is wanted
    // Each panel's triangles while the atlas is drawn (kept: nothing allocated once grown).
    za::Vector<gfx::Vertex> tris;
    za::Vector<gfx::Vertex> glows;
    za::Array<PanelLayer, VisorPanelCount> panels{};
    // As last drawn (vr_flat_visor_cache): what from (layerSignature; 0 never), the layer at rest, the view's horizontal
    // tan half-angle, the atlas's texels a layer pixel, each depth's share of the sway.
    unsigned long long signature{0};
    Layer L;
    float tx{1.f};
    int ss{1};
    float plateParallax{1.f}, contentParallax{1.f};
    glm::mat4 mvp{1.f}; // the slot being drawn's (useSlot)
    za::Array<unsigned long long, VisorPanelCount> mountSigs{}, pictureSigs{}; // each panel's as drawn (panelSignature)
    float swayPad{0.f}; // this frame's: how far round where they lie at rest the panels can be (view pixels: boxOf)
    // Where the panels went last (the bullet-time look leaves them alone: tintMask): the frame, the view in the target,
    // the mask drawn.
    int drawnFrame{-1};
    glm::vec4 drawnRect{0.f};
    bool tintDrawn{false};
};
LayerPass layerPass;

// The frosted glass (vr_flat_visor_glass_*): the view at half its size, blurred back and forth between two targets.
struct GlassPass
{
    GLuint blur{0};
    bool failed{false};
    gfx::RenderTarget sceneA, sceneB;
    bool on{false}; // this frame
};
GlassPass glassPass;

[[nodiscard]] glm::vec4 premul(const glm::vec3& rgb, float a)
{
    return {rgb * a, a};
}

// The triangles into the slot being drawn (layerPass.mvp), then emptied.
void drawList(za::Vector<gfx::Vertex>& v, gfx::Blend blend, gfx::Shade shade = gfx::Shade::Color)
{
    if(!v.empty())
    {
        gfx::draw({v.data(), v.size()}, layerPass.mvp, {.shade = shade, .blend = blend, .depthTest = false, .depthWrite = false});
    }
    v.clear();
}

// Panel `pl`'s slot `s` drawn into next: the viewport its box in the atlas, the whole layer's clip space (the vertices')
// mapped onto it (the clip volume: nothing drawn past the box).
void useSlot(const PanelLayer& pl, int s)
{
    LayerPass& lp = layerPass;
    const int k = slotScale(s, lp.ss);
    glViewport(pl.slot[s].x, pl.slot[s].y, static_cast<int>(pl.box.z) * k, static_cast<int>(pl.box.w) * k);
    glm::mat4 m{1.f};
    m[0][0] = lp.L.w / pl.box.z;
    m[3][0] = (lp.L.w - 2.f * pl.box.x) / pl.box.z - 1.f;
    m[1][1] = lp.L.h / pl.box.w;
    m[3][1] = 1.f - (lp.L.h - 2.f * pl.box.y) / pl.box.w;
    lp.mvp = m;
}

// vr_flat_visor_depth_color: "r g b" (0..1), or the face's own darker (empty, or unreadable: false).
[[nodiscard]] bool plateColour(glm::vec3& out)
{
    float r = 0.f, g = 0.f, b = 0.f;
    if(std::sscanf(vr_flat_visor_depth_color.string, "%f %f %f", &r, &g, &b) != 3)
    {
        return false;
    }
    out = glm::clamp(glm::vec3{r, g, b}, glm::vec3{0.f}, glm::vec3{1.f});
    return true;
}

// The layer for a viewW x viewH view, at rest (no sway: the composite's).
[[nodiscard]] Layer layerFor(int viewW, int viewH)
{
    Layer L;
    L.w = static_cast<float>(viewW);
    L.h = static_cast<float>(viewH);
    L.px1080 = L.h / 1080.f;
    const float scale = za::clamp(vr_flat_visor_scale.value, 0.5f, 2.f);
    // A virtual pixel: the vitals' 62 rows 11.5% of the view's height at scale 1; smaller where a narrow screen would not
    // fit the top corners' two mounts (318 across), nor the log's mount between them at about its size (527: then the
    // log's own fit, logFit, shrinks it alone).
    L.unit = za::min(za::min(static_cast<float>(viewH) * 0.115f * scale / 62.f, L.w / 527.f), L.w / 318.f);
    L.k = za::clamp(vr_flat_visor_warp.value, -0.2f, 0.2f);
    L.zoom = za::clamp(vr_flat_visor_zoom.value, 0.85f, 1.2f);
    L.overhang = {0.3f * L.w / L.unit, 0.3f * L.h / L.unit}; // (past 1 + k and the sway at every aspect, zoomed to 0.85)
    L.sideOut = za::clamp(vr_flat_visor_side_out.value, -4.f, 8.f);
    L.logOut = za::clamp(vr_flat_visor_log_out.value, -4.f, 8.f);
    const glm::vec2 vitals = gadget::visorImage(gadget::VisorVitals).size, status = gadget::visorImage(gadget::VisorStatus).size;
    // The side mounts' size: vr_flat_visor_side_scale, no taller than leaves them clear of the corners' mounts (their
    // contents, padding and struts: about 85 virtual pixels down from the top, up from the bottom).
    const float sideH = za::max(1.f, gadget::visorImage(gadget::VisorSideLeft).size.y) + 2.f * mountPad;
    L.sideScale = za::max(0.5f, za::min(za::clamp(vr_flat_visor_side_scale.value, 0.5f, 2.f), (L.h / L.unit - 170.f) / sideH));
    const float cornerReach = mountInset + za::max(vitals.x, status.x) + mountPad;
    const glm::vec2 logSize = gadget::visorImage(gadget::VisorLog).size;
    L.logScale = logFit(L, logSize, cornerReach, za::clamp(vr_flat_visor_log_width.value, 0.3f, 2.f));
    // The centre print's popup: under where the log's mount ends (hidden or not), no wider than the view.
    L.centerTop = logBottom(L, logSize) + mountPad + za::clamp(vr_flat_visor_center_gap.value, -20.f, 80.f);
    const float centerW = gadget::visorImage(gadget::VisorCenter).size.x + 2.f * mountPad;
    L.centerScale = za::clamp((L.w / L.unit - 2.f * mountInset) / za::max(1.f, centerW), 0.3f, 1.f);
    return L;
}

// Panel p's fades this frame: the whole panel's (death, the glitch's flicker, the boot's stutter, the log's autohide:
// x) and its mount's (the boot's sweep too: y).
[[nodiscard]] glm::vec2 panelFade(int p)
{
    const float g = state.glitch;
    const float flicker = g > 0.f ? 1.f - 0.45f * g * hash(static_cast<unsigned>(host_framecount) * 2654435761u) : 1.f;
    const float b = state.input.boot[p];
    const float stutter = b < 0.15f && hash(static_cast<unsigned>(host_framecount) * 7u + static_cast<unsigned>(p)) < 0.4f ? 0.35f : 1.f;
    const float hide = p == gadget::VisorLog      ? smoothstep(0.f, 1.f, state.logFade) // (vr_flat_visor_autohide_log)
                       : p == gadget::VisorCenter ? smoothstep(0.f, 1.f, state.center.fade)
                                                  : 1.f;
    const float fade = state.fade * flicker * stutter * hide;
    return {fade, fade * smoothstep(0.f, 0.2f, b)};
}

// The atlas's slots for every built panel at `ss` texels a layer pixel (shelves, the tallest first; a gap round each
// that a tap past its box never crosses): the height wanted, 0 when it does not fit.
[[nodiscard]] int packAtlas(int ss)
{
    LayerPass& lp = layerPass;
    const int gap = 2 * ss + 4;
    za::Array<int, VisorPanelCount * SlotCount> order{};
    int n = 0;
    const auto height = [&](int i) { return static_cast<int>(lp.panels[i / SlotCount].box.w) * slotScale(i % SlotCount, ss) + gap; };
    for(int i = 0; i < VisorPanelCount * SlotCount; i++)
    {
        if(!lp.panels[i / SlotCount].built)
        {
            continue;
        }
        int k = n++;
        for(; k > 0 && height(order[k - 1]) < height(i); k--)
        {
            order[k] = order[k - 1];
        }
        order[k] = i;
    }
    int x = 0, y = 0, shelf = 0;
    for(int k = 0; k < n; k++)
    {
        PanelLayer& pl = lp.panels[order[k] / SlotCount];
        const int w = static_cast<int>(pl.box.z) * slotScale(order[k] % SlotCount, ss) + gap, h = height(order[k]);
        if(w > atlasWidth)
        {
            return 0;
        }
        if(x + w > atlasWidth)
        {
            y += shelf;
            x = shelf = 0;
        }
        pl.slot[order[k] % SlotCount] = {x + gap / 2, y + gap / 2};
        x += w;
        shelf = za::max(shelf, h);
    }
    const int total = y + shelf;
    return total <= 8192 ? za::max(total, 1) : 0;
}

// What panel p's slots are drawn from beyond the whole atlas's (layerSignature): its mount's colours (the vitals'
// low-health pulse: every slot) and its picture (its texture, how many times it was drawn, a hit's glitch: the contents'
// slot alone).
struct PanelSignature
{
    unsigned long long mount{0}, picture{0};
};
[[nodiscard]] PanelSignature panelSignature(int p)
{
    const gadget::VisorImage img = gadget::visorImage(p);
    const float alarm = p == gadget::VisorVitals ? state.alarm : 0.f;
    unsigned long long picture = diskcache::fnvStart;
    const auto add = [&picture](const auto& v) { picture = diskcache::fnv(picture, &v, sizeof(v)); };
    add(img.texture), add(img.version), add(state.glitch);
    if(state.glitch > 0.f)
    {
        add(host_framecount); // (Shade::Screen's glitch: torn anew every frame)
    }
    return {diskcache::fnv(diskcache::fnvStart, &alarm, sizeof(alarm)), picture};
}

// Panel p into its slots in the atlas (bound), each cleared first: all of them (`all`), or its contents alone (its
// image drawn again, nothing else changed).
void drawPanel(int p, bool all)
{
    LayerPass& lp = layerPass;
    PanelLayer& pl = lp.panels[p];
    Panel& out = state.panels[p];
    if(!pl.built)
    {
        return;
    }
    const PanelSignature drawnFrom = panelSignature(p);
    lp.pictureSigs[p] = drawnFrom.picture;
    lp.mountSigs[p] = all ? drawnFrom.mount : lp.mountSigs[p];
    const Layer& L = lp.L;
    const float crt = za::clamp(vr_flat_visor_crt.value, 0.f, 2.f);
    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    const glm::vec4 phosphor{glm::vec3{gadget::visorPhosphor()}, 0.f}; // (alpha 0: the image's face clear, the mount's)
    const gadget::VisorColors colors = gadget::visorColors();
    const float opacity = za::clamp(vr_flat_visor_opacity.value, 0.f, 1.f);
    const float edge = za::clamp(vr_flat_visor_edge.value, 0.f, 8.f) * L.px1080;
    const float bevel = za::clamp(vr_flat_visor_bevel.value, 0.f, 2.f);
    const float sheen = za::clamp(vr_flat_visor_sheen.value, 0.f, 3.f);
    const float glowOut = za::clamp(vr_flat_visor_glow_outer.value, 0.f, 2.f);
    const float glowIn = za::clamp(vr_flat_visor_glow_inner.value, 0.f, 2.f);
    const float glowWidth = za::clamp(vr_flat_visor_glow_width.value, 1.f, 60.f) * L.px1080;
    // The back plate (vr_flat_visor_depth_*).
    const float plateAlpha = za::min(1.f, opacity * za::clamp(vr_flat_visor_depth_opacity.value, 0.f, 2.f));
    const float plateBevel = za::clamp(vr_flat_visor_depth_bevel.value, 0.f, 2.f);
    const float plateBevelWidth = za::clamp(vr_flat_visor_depth_bevel_width.value, 0.5f, 12.f) * L.px1080;
    glm::vec3 plateOwn{0.f};
    const bool plateOwnColour = plateColour(plateOwn);
    glEnable(GL_SCISSOR_TEST);
    for(int s = all ? 0 : SlotContents; s < (all ? SlotCount : SlotContents + 1); s++)
    {
        const int k = slotScale(s, lp.ss);
        glScissor(pl.slot[s].x, pl.slot[s].y, static_cast<int>(pl.box.z) * k, static_cast<int>(pl.box.w) * k);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
    // The contents, a little behind the glass (their own share of the sway: the composite's). (Each slot its own: in any
    // order.)
    const gadget::VisorImage img = gadget::visorImage(p);
    useSlot(pl, SlotContents);
    drawContents(L, p, pl.mount, img,
        {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
            .params = {time, crt * za::clamp(vr_flat_visor_ink_crt.value, 0.f, 1.f), state.glitch, gadget::textGlow()},
            .screen = {img.size.x, img.size.y, 0.5f}, .trueColor = true,
            .inkAlpha = 1.f},
        phosphor, lp.mvp, out.rect);
    if(!all)
    {
        return;
    }

    // The vitals' mount pulses red at low health (its face, its line and its glows).
    const float alarm = p == gadget::VisorVitals ? state.alarm : 0.f;
    const glm::vec3 alarmFace{0.16f, 0.015f, 0.01f};
    const glm::vec3 faceRgb = glm::mix(colors.face, alarmFace, za::clamp(alarm, 0.f, 1.f) * 0.85f);
    const glm::vec3 lineRgb = glm::mix(colors.line, colors.warn, za::clamp(alarm * 1.5f, 0.f, 1.f));

    // The back plate: darker (or its own colour), a dim line round it and its bevel lit from above.
    if(pl.hasPlate)
    {
        const Ring& ring = pl.plate;
        const glm::vec3 plateRgb = plateOwnColour ? glm::mix(plateOwn, alarmFace * 0.55f, za::clamp(alarm, 0.f, 1.f) * 0.85f)
                                                  : faceRgb * 0.55f;
        useSlot(pl, SlotPlate);
        face(L, ring, premul(plateRgb, plateAlpha), glm::vec2{0.f, 1.f}, 0.f, lp.tris);
        line(L, ring, 0.f, za::max(1.f, edge * 0.75f), premul(lineRgb * 0.35f, 0.8f), 0.f, lp.tris);
        if(plateBevel > 0.f)
        {
            // Its rim inside the line: light on the faces turned up, shadow on those turned down.
            const float in0 = -za::max(0.5f, edge * 0.375f), in1 = in0 - plateBevelWidth;
            const glm::vec3 lit = glm::mix(plateRgb, glm::vec3{1.f}, 0.55f);
            band(L, ring, in0, in1, premul(lit, 0.3f * plateBevel), glm::vec4{0.f}, 1.f, lp.tris, true);
            band(L, ring, in0, in1, premul(glm::vec3{0.f}, 0.35f * plateBevel), glm::vec4{0.f}, -1.f, lp.tris, true);
        }
        drawList(lp.tris, gfx::Blend::Premultiplied);
        useSlot(pl, SlotShapes);
        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        face(L, ring, premul(glm::vec3{1.f}, 1.f), glm::vec2{0.f, 1.f}, 0.f, lp.tris);
        drawList(lp.tris, gfx::Blend::Premultiplied);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    // The face, its sheen down the whole of it as seen (its top to its bottom on the screen).
    const Ring& ring = pl.face;
    const glm::vec4 box = shownBox(L, ring);
    useSlot(pl, SlotFace);
    face(L, ring, premul(faceRgb, opacity), glm::vec2{box.y, box.w}, sheen, lp.tris);
    drawList(lp.tris, gfx::Blend::Premultiplied);
    useSlot(pl, SlotShapes);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
    face(L, ring, premul(glm::vec3{1.f}, 1.f), glm::vec2{0.f, 1.f}, 0.f, lp.tris);
    drawList(lp.tris, gfx::Blend::Premultiplied);
    // (b: where anything of it can be -- the face and the plate and a band round each as far as the lines and glows
    // reach and the plate and the contents move from the face with the sway: the composite skips the rest of its box)
    {
        const float lag = 0.8f * mountInset * L.unit * za::max(za::abs(1.f - lp.plateParallax), 1.f - lp.contentParallax);
        const float reach = 2.f * (edge + glowWidth) + 6.f + lag; // (generous: a tap past it reads nothing)
        glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
        for(int r = 0; r < (pl.hasPlate ? 2 : 1); r++)
        {
            const Ring& outline = r == 0 ? pl.face : pl.plate;
            face(L, outline, premul(glm::vec3{1.f}, 1.f), glm::vec2{0.f, 1.f}, 0.f, lp.tris);
            band(L, outline, -2.f, reach, premul(glm::vec3{1.f}, 1.f), premul(glm::vec3{1.f}, 1.f), 0.f, lp.tris);
        }
        drawList(lp.tris, gfx::Blend::Premultiplied);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Over them: the glass's glint over the whole face, the glows (added: premultiplied with no alpha), the frame lit
    // from above and its inner highlight.
    useSlot(pl, SlotOver);
    if(sheen > 0.f)
    {
        glint(L, ring, box, sheen, lp.glows);
        drawList(lp.glows, gfx::Blend::Additive, gfx::Shade::SoftEdge);
    }
    const float half = edge * 0.5f;
    if(glowIn > 0.f)
    {
        band(L, ring, -half, -half - glowWidth, premul(lineRgb, 0.5f * glowIn) * glm::vec4{1.f, 1.f, 1.f, 0.f},
            glm::vec4{0.f}, 0.f, lp.glows);
    }
    if(glowOut > 0.f)
    {
        band(L, ring, half, half + glowWidth, premul(lineRgb, 0.5f * glowOut) * glm::vec4{1.f, 1.f, 1.f, 0.f},
            glm::vec4{0.f}, 0.f, lp.glows);
    }
    drawList(lp.glows, gfx::Blend::Premultiplied);
    if(edge > 0.f)
    {
        line(L, ring, 0.f, edge, premul(lineRgb, 0.9f), 0.45f * bevel, lp.tris);
        if(bevel > 0.f)
        {
            const glm::vec3 lit = glm::mix(lineRgb, glm::vec3{1.f}, 0.6f);
            line(L, ring, -(half + 0.9f * L.px1080), za::max(0.8f, edge * 0.5f), premul(lit, 0.3f * bevel), 0.8f * bevel, lp.tris);
        }
        drawList(lp.tris, gfx::Blend::Premultiplied);
    }
}

// The mounts and their contents (vr_flat_visor_panels: which) into the atlas, at rest, unfaded: each panel's back plate
// and its bevel, face, contents, glint, glows, frame and its highlight, its shapes for the glass. False: no atlas.
[[nodiscard]] bool buildPanels(int viewW, int viewH, int wantSs)
{
    LayerPass& lp = layerPass;
    Layer& L = lp.L;
    L = layerFor(viewW, viewH);
    const int shownMask = static_cast<int>(vr_flat_visor_panels.value);
    const float edge = za::clamp(vr_flat_visor_edge.value, 0.f, 8.f) * L.px1080;
    const float depth = za::clamp(vr_flat_visor_depth.value, 0.f, 3.f);
    const float glowWidth = za::clamp(vr_flat_visor_glow_width.value, 1.f, 60.f) * L.px1080;
    const float plateSize = za::clamp(vr_flat_visor_depth_size.value, 0.f, 60.f) * L.px1080;
    const float plateOffset = za::clamp(vr_flat_visor_depth_offset.value, 0.f, 20.f) * L.px1080;
    lp.plateParallax = za::max(-1.f, 1.f - za::clamp(vr_flat_visor_depth_lag.value, 0.f, 1.f) * depth);
    lp.contentParallax = za::max(0.f, 1.f - za::clamp(vr_flat_visor_depth_content_lag.value, 0.f, 1.f) * depth);

    // What of the layer the view can show (its edges through the lens), the sway's reach round it; how far the lines and
    // glows reach past an outline (view pixels; in the layer up to twice that, where the lens shrinks).
    const float most = 0.8f * mountInset * L.unit * za::max(1.f, za::max(za::abs(lp.plateParallax), lp.contentParallax));
    const float reach = edge * 0.5f + glowWidth + 3.f;
    glm::vec2 seenLo{1e9f}, seenHi{-1e9f};
    for(int i = 0; i <= 16; i++)
    {
        const float t = static_cast<float>(i) / 16.f;
        const glm::vec2 border[4] = {{t * L.w, 0.f}, {t * L.w, L.h}, {0.f, t * L.h}, {L.w, t * L.h}};
        for(const glm::vec2& v : border)
        {
            const glm::vec2 q = unshownPx(L, v);
            seenLo = glm::min(seenLo, q);
            seenHi = glm::max(seenHi, q);
        }
    }
    seenLo -= glm::vec2{most + 2.f * reach + 2.f};
    seenHi += glm::vec2{most + 2.f * reach + 2.f};

    // The outlines laid out, each panel's box in the layer and on the view.
    for(int p = 0; p < VisorPanelCount; p++)
    {
        PanelLayer& pl = lp.panels[p];
        pl.built = false;
        const gadget::VisorImage img = gadget::visorImage(p);
        const bool masked = p != gadget::VisorCenter && !(shownMask & (1 << p)); // (the centre print's: vr_flat_visor_centerprint)
        if(masked || !img.shown || img.size.y <= 0.f)
        {
            continue;
        }
        const bool side = p == gadget::VisorSideLeft || p == gadget::VisorSideRight;
        const float scale = p == gadget::VisorLog ? L.logScale : side ? L.sideScale : p == gadget::VisorCenter ? L.centerScale : 1.f;
        pl.mount = mountOf(L, p, img.size * scale);
        pl.hasPlate = depth > 0.f;
        if(pl.hasPlate)
        {
            // A little behind (towards the view's middle, as a deeper thing seen from the eye), grown.
            const MountPlace& place = mountPlaces[p];
            const glm::vec2 toMiddle = glm::normalize(glm::vec2{0.5f - place.anchor.x, 0.5f - place.anchor.y} * glm::vec2{L.w, L.h});
            ringOf(L, p, pl.mount, 1.f, toMiddle * (plateOffset * depth), pl.plate);
            grow(pl.plate, plateSize);
        }
        ringOf(L, p, pl.mount, 1.f, glm::vec2{0.f}, pl.face);
        glm::vec2 lo{1e9f}, hi{-1e9f}, shownLo{1e9f}, shownHi{-1e9f};
        const auto add = [&](const Ring& r) {
            for(int i = 0; i < r.n; i++)
            {
                const glm::vec2 q = toPx(L, glm::vec2{layerAt(L, r, i, r.at[i])});
                lo = glm::min(lo, q);
                hi = glm::max(hi, q);
                shownLo = glm::min(shownLo, r.at[i]);
                shownHi = glm::max(shownHi, r.at[i]);
            }
        };
        add(pl.face);
        if(pl.hasPlate)
        {
            add(pl.plate);
        }
        lo = glm::floor(glm::max(lo - 2.f * reach, seenLo));
        hi = glm::ceil(glm::min(hi + 2.f * reach, seenHi));
        if(hi.x <= lo.x || hi.y <= lo.y)
        {
            continue;
        }
        pl.box = {lo, hi - lo};
        const glm::vec2 view{L.w, L.h};
        shownLo = glm::clamp(shownLo - reach - 1.f, glm::vec2{0.f}, view);
        shownHi = glm::clamp(shownHi + reach + 1.f, glm::vec2{0.f}, view);
        pl.screen = {shownLo, shownHi};
        pl.built = true;
    }

    // The atlas: at vr_flat_visor_ss, less if that does not fit (grown only: drawn again in the same one).
    int ss = wantSs, height = 0;
    for(; ss >= 1 && height <= 0; ss--)
    {
        height = packAtlas(ss);
    }
    lp.ss = ss + 1;
    height = (height + 255) / 256 * 256;
    if(height <= 0 ||
        !lp.atlas.ensure(atlasWidth, za::max(height, lp.atlas.height), gfx::RenderTargetFormat{.internalFormat = GL_RGBA8}, "vr visor atlas"))
    {
        return false;
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, lp.atlas.fbo);
    glViewport(0, 0, lp.atlas.width, lp.atlas.height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    for(int p = 0; p < VisorPanelCount; p++)
    {
        drawPanel(p, true);
    }
    return true;
}

// The cvars buildPanels reads (the atlas drawn again when one changes).
const cvar_t* const layerCvars[] = {&vr_flat_visor_scale, &vr_flat_visor_warp, &vr_flat_visor_zoom, &vr_flat_visor_side_out,
    &vr_flat_visor_log_out, &vr_flat_visor_log_width, &vr_flat_visor_crt, &vr_flat_visor_opacity, &vr_flat_visor_panels,
    &vr_flat_visor_edge, &vr_flat_visor_depth, &vr_flat_visor_bevel, &vr_flat_visor_sheen, &vr_flat_visor_glow_outer,
    &vr_flat_visor_glow_inner, &vr_flat_visor_glow_width, &vr_flat_visor_depth_opacity, &vr_flat_visor_depth_size,
    &vr_flat_visor_depth_offset, &vr_flat_visor_depth_lag, &vr_flat_visor_depth_content_lag, &vr_flat_visor_depth_bevel,
    &vr_flat_visor_depth_bevel_width, &vr_flat_visor_side_scale, &vr_flat_visor_ink_crt, &vr_flat_visor_center_gap};

// What the atlas is laid out and drawn from: the view, the cvars, the colours, which panels are shown and their sizes. Not
// the sway, the fades nor the glitch (the composite's), nor what each panel shows (panelSignature). Changed: all of it
// laid out and drawn again.

[[nodiscard]] unsigned long long layerSignature(int w, int h, int ss)
{
    unsigned long long sig = diskcache::fnvStart;
    const auto add = [&sig](const auto& v) { sig = diskcache::fnv(sig, &v, sizeof(v)); };
    add(w), add(h), add(ss), add(layerPass.atlas.tex);
    for(const cvar_t* c : layerCvars)
    {
        add(c->value);
    }
    sig = diskcache::fnv(sig, vr_flat_visor_depth_color.string, std::strlen(vr_flat_visor_depth_color.string));
    for(int p = 0; p < VisorPanelCount; p++)
    {
        const gadget::VisorImage img = gadget::visorImage(p);
        add(img.size), add(img.shown); // (their pictures: panelSignature)
    }
    add(gadget::visorPhosphor()), add(gadget::visorColors()), add(gadget::textGlow());
    return sig;
}

// Panel p's rectangle on a w x h view this frame: where it is at rest (PanelLayer::screen) and as far round it as the sway
// moves it now (layerPass.swayPad), at `scale` and `pad` pixels more (x0, y0, x1, y1; y down).
[[nodiscard]] glm::ivec4 boxOf(int p, int w, int h, float scale, float pad)
{
    const glm::vec4 b = layerPass.panels[p].screen;
    pad += layerPass.swayPad * scale;
    return {za::max(0, static_cast<int>(std::floor(b.x * scale - pad))), za::max(0, static_cast<int>(std::floor(b.y * scale - pad))),
        za::min(w, static_cast<int>(std::ceil(b.z * scale + pad))), za::min(h, static_cast<int>(std::ceil(b.w * scale + pad)))};
}

// Each panel shown this frame's rectangle (boxOf) as a scissor in the framebuffer (the view at x, y from its bottom
// left), then `draw` there.
template <typename F>
void eachBox(int x, int y, int w, int h, float scale, float pad, F&& draw)
{
    if(vr_flat_visor_cache.value == 0.f) // (debug: the whole view, as before the rectangles)
    {
        draw(x, y, w, h);
        return;
    }
    for(int p = 0; p < VisorPanelCount; p++)
    {
        const glm::ivec4 b = boxOf(p, w, h, scale, pad);
        if(state.panels[p].drawn && b.z > b.x && b.w > b.y)
        {
            draw(x + b.x, y + (h - b.w), b.z - b.x, b.w - b.y); // (y up)
        }
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

// The glass's settings now: on (some blur or bend), and its program and targets ready for a w x h view.
[[nodiscard]] bool glassTargets(int w, int h);

[[nodiscard]] bool glassReady(int w, int h)
{
    if(vr_flat_visor_glass_blur.value <= 0.f && vr_flat_visor_glass_distort.value <= 0.f)
    {
        return false;
    }
    return glassTargets(w, h);
}

// The glass's blur program and its two half-size targets for a w x h view (the glass's, and the holster menu's blur).
[[nodiscard]] bool glassTargets(int w, int h)
{
    GlassPass& gp = glassPass;
    if(!gp.blur && !gp.failed)
    {
        gp.blur = gfx::glProgram(layerVs, blurFs, "vr visor glass blur");
        gp.failed = !gp.blur;
    }
    const int hw = za::max(1, w / 2), hh = za::max(1, h / 2);
    const gfx::RenderTargetFormat color{.internalFormat = GL_RGB10_A2};
    return !gp.failed && gp.sceneA.ensure(hw, hh, color, "vr visor glass a") && gp.sceneB.ensure(hw, hh, color, "vr visor glass b");
}

// The view behind the visor (`framebuffer` at x, y, w x h) at half its size, blurred by vr_flat_visor_glass_blur (pixels
// at 1080p): Kawase passes at growing distances, as many as its square root wants (at most 6). The blurred copy's texture.
[[nodiscard]] GLuint glassBlur(unsigned framebuffer, int x, int y, int w, int h, float blurPx, bool whole)
{
    GlassPass& gp = glassPass;
    const int hw = gp.sceneA.width, hh = gp.sceneA.height;
    // Only round the mounts (the layer's rectangles: the glass is seen nowhere else), padded by as far as the passes reach
    // and the bend samples (a pass reads its neighbours: what is wrong at the pad's edge never reaches the mounts).
    const float radius = za::clamp(blurPx, 0.f, 40.f) * static_cast<float>(h) / 1080.f * 0.5f; // (half size)
    // (The whole view: the holster menu's blur, menuBlur.)
    const auto boxes = [&](int bx0, int by0, int bw0, int bh0, float scale, float pad, auto&& fn) {
        if(whole)
        {
            fn(bx0, by0, bw0, bh0);
        }
        else
        {
            eachBox(bx0, by0, bw0, bh0, scale, pad, fn);
        }
    };
    const int passes = radius < 0.5f ? 0 : za::clamp(static_cast<int>(std::ceil(std::sqrt(2.f * radius))), 1, 6);
    const float step = passes ? za::clamp(radius / (0.5f * static_cast<float>(passes * passes)), 0.5f, 3.f) : 0.f;
    float reach = 4.f + za::clamp(vr_flat_visor_glass_distort.value, 0.f, 3.f) * static_cast<float>(h) / 1080.f * 6.f; // (the bend)
    for(int i = 0; i < passes; i++)
    {
        reach += ((static_cast<float>(i) + 0.5f) * step + 1.f) * 1.5f;
    }
    const float half = static_cast<float>(hw) / static_cast<float>(w);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, framebuffer);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, gp.sceneA.fbo);
    boxes(0, 0, hw, hh, half, reach, [&](int bx, int by, int bw, int bh) {
        const float up = static_cast<float>(w) / static_cast<float>(hw);
        GL_BlitFramebufferFunc(x + static_cast<int>(static_cast<float>(bx) * up), y + static_cast<int>(static_cast<float>(by) * up),
            x + static_cast<int>(static_cast<float>(bx + bw) * up), y + static_cast<int>(static_cast<float>(by + bh) * up), bx, by,
            bx + bw, by + bh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    });
    if(passes == 0)
    {
        return gp.sceneA.tex;
    }
    GL_UseProgram(gp.blur);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glViewport(0, 0, hw, hh);
    glEnable(GL_SCISSOR_TEST);
    gfx::RenderTarget* from = &gp.sceneA;
    gfx::RenderTarget* to = &gp.sceneB;
    for(int i = 0; i < passes; i++)
    {
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, to->fbo);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, from->tex);
        GL_Uniform4fFunc(0, 1.f / static_cast<float>(hw), 1.f / static_cast<float>(hh), (static_cast<float>(i) + 0.5f) * step, 0.f);
        boxes(0, 0, hw, hh, half, reach, [](int bx, int by, int bw, int bh) {
            glScissor(bx, by, bw, bh);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        });
        gfx::RenderTarget* const was = from;
        from = to;
        to = was;
    }
    glDisable(GL_SCISSOR_TEST);
    return from->tex;
}

// Each panel shown (Panel::drawn) onto the target (the program and its shared uniforms set): its slots, its sway, its
// fades (`fades`: panelFade's), scissored to where it can be. `alphaOnly`: bullet time's mask.
void composite(int x, int y, int w, int h, const za::Array<glm::vec2, VisorPanelCount>& fades, bool alphaOnly)
{
    const LayerPass& lp = layerPass;
    const glm::vec2 shift = swayShift(lp.L, state.sway, lp.tx);
    const glm::vec2 plate = shift * lp.plateParallax, contents = shift * lp.contentParallax;
    glEnable(GL_SCISSOR_TEST);
    for(int p = 0; p < VisorPanelCount; p++)
    {
        if(!state.panels[p].drawn)
        {
            continue;
        }
        const PanelLayer& pl = lp.panels[p];
        const auto corner = [&](int s) { return glm::vec2{pl.slot[s]}; };
        GL_Uniform4fFunc(5, pl.box.x, pl.box.y, pl.box.z, pl.box.w);
        GL_Uniform4fFunc(6, corner(SlotPlate).x, corner(SlotPlate).y, corner(SlotFace).x, corner(SlotFace).y);
        GL_Uniform4fFunc(7, corner(SlotContents).x, corner(SlotContents).y, corner(SlotOver).x, corner(SlotOver).y);
        GL_Uniform4fFunc(8, corner(SlotShapes).x, corner(SlotShapes).y, static_cast<float>(lp.ss), alphaOnly ? 1.f : 0.f);
        GL_Uniform4fFunc(9, plate.x, plate.y, contents.x, contents.y);
        GL_Uniform4fFunc(10, shift.x, shift.y, fades[p].y, fades[p].x);
        // The centre print's pop in: from 85% of its size (eased out) about its middle, swayed with its face.
        const float grow = p == gadget::VisorCenter ? 1.f - 0.15f * (1.f - smoothstep(0.f, 1.f, centerPop())) : 1.f;
        const glm::vec2 middle = (localToPx(lp.L, p, pl.mount.lo) + localToPx(lp.L, p, pl.mount.hi)) * 0.5f + shift;
        GL_Uniform4fFunc(12, middle.x, middle.y, 1.f / grow, 0.f);
        if(vr_flat_visor_cache.value == 0.f) // (debug: the whole view)
        {
            glScissor(x, y, w, h);
        }
        else
        {
            const glm::ivec4 b = boxOf(p, w, h, 1.f, 0.f);
            glScissor(x + b.x, y + (h - b.w), za::max(0, b.z - b.x), za::max(0, b.w - b.y)); // (y up)
        }
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_SCISSOR_TEST);
}

// Flatscreen play's holster menu (vr_flatholster.cpp blurRadius): the whole view blurred as the glass blurs what is behind
// the mounts (glassBlur's passes, at the menu's radius), put back over it.
void menuBlur(unsigned framebuffer, int x, int y, int w, int h)
{
    const float radius = flatholster::blurRadius();
    if(radius <= 0.f || w < 16 || h < 16)
    {
        return;
    }
    if(!glassTargets(w, h))
    {
        return;
    }
    GlassPass& gp = glassPass;
    const int hw = gp.sceneA.width, hh = gp.sceneA.height;
    QVR_GPU_PROFILE("holster menu blur");
    const GLuint tex = glassBlur(framebuffer, x, y, w, h, radius, true);
    const gfx::RenderTarget& out = tex == gp.sceneA.tex ? gp.sceneA : gp.sceneB;
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, out.fbo);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, framebuffer);
    GL_BlitFramebufferFunc(0, 0, hw, hh, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
    glViewport(x, y, w, h);
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

    LayerPass& lp = layerPass;
    GlassPass& gp = glassPass;
    gp.on = glassReady(w, h);
    // The panels' atlas, drawn again only when what it shows changed (standing, walking, turning: never).
    const int ss = za::clamp(static_cast<int>(vr_flat_visor_ss.value), 1, 4);
    const unsigned long long signature = layerSignature(w, h, ss);
    lp.tx = ty * aspect; // (the sway's scale: not in the signature, the speed FOV changes it running)
    if(!ensureLayerProgram())
    {
        GL_EndGroup();
        return;
    }
    if(signature != lp.signature || vr_flat_visor_cache.value == 0.f)
    {
        QVR_PROFILE("visor layer");
        lp.signature = buildPanels(w, h, ss) ? layerSignature(w, h, ss) : 0u; // (the atlas's texture in it)
        if(!lp.signature)
        {
            GL_EndGroup();
            return;
        }
    }
    else
    {
        // Only what a panel shows changed (a reading, the low-health pulse): that panel's slots drawn again (a picture
        // alone: its contents' slot).
        bool bound = false;
        for(int p = 0; p < VisorPanelCount; p++)
        {
            const PanelSignature now = panelSignature(p);
            const bool mount = now.mount != lp.mountSigs[p];
            if(lp.panels[p].built && (mount || now.picture != lp.pictureSigs[p]))
            {
                QVR_PROFILE("visor panel");
                if(!za::exchange(bound, true))
                {
                    GL_BindFramebufferFunc(GL_FRAMEBUFFER, lp.atlas.fbo);
                }
                drawPanel(p, mount);
            }
        }
    }
    za::Array<glm::vec2, VisorPanelCount> fades{};
    for(int p = 0; p < VisorPanelCount; p++)
    {
        fades[p] = panelFade(p);
        state.panels[p].drawn = lp.panels[p].built && fades[p].x > 0.f;
    }
    // Bullet time's mask wanted (its look leaves the panels alone: tintMask), its target ready.
    lp.tintDrawn = vr_flat_visor_no_bt_tint.value != 0.f && bullettime::look().strength > 0.f &&
                   lp.tint.ensure(w, h, gfx::RenderTargetFormat{.internalFormat = GL_RGBA8}, "vr visor tint mask");
    {
        // How far the sway moves the panels from where they lie at rest now (view pixels: the lens magnifies it at most
        // about 1.6 times towards the corners).
        const glm::vec2 shift = swayShift(lp.L, state.sway, lp.tx);
        const float most = glm::length(shift) * za::max(1.f, za::max(za::abs(lp.plateParallax), lp.contentParallax));
        lp.swayPad = most > 0.f ? most * 1.6f + 1.f : 0.f;
    }
    const GLuint behind = gp.on ? glassBlur(framebuffer, x, y, w, h, vr_flat_visor_glass_blur.value, false) : 0u;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
    glViewport(x, y, w, h);
    GL_UseProgram(lp.program);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // (premultiplied)
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, lp.atlas.tex);
    const float px = static_cast<float>(h) / 1080.f;
    const float scan = za::clamp(vr_flat_visor_scanlines.value, 0.f, 1.f);
    const float drift = static_cast<float>(za::fmod(realtime * static_cast<double>(vr_flat_visor_scanline_speed.value) * px, 100000.0));
    const float bandY = static_cast<float>(h) * (1.f - static_cast<float>(za::fmod(realtime * 0.12, 1.0))); // (rolling down)
    GL_Uniform4fFunc(0, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h));
    GL_Uniform4fFunc(1, scan, za::max(2.f, za::clamp(vr_flat_visor_scanline_width.value, 1.5f, 16.f) * px), drift, bandY);
    GL_Uniform1fFunc(11, za::clamp(vr_flat_visor_ink_crt.value, 0.f, 1.f));
    if(gp.on)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, behind);
        // The tint: the screens' colour at its brightest component's 1 (the view's brightness kept).
        const glm::vec3 tint{gadget::visorPhosphor()};
        const glm::vec3 tintRgb = tint / za::max(1e-3f, za::max(tint.x, za::max(tint.y, tint.z)));
        GL_Uniform4fFunc(2, 1.f, za::clamp(vr_flat_visor_glass_distort.value, 0.f, 3.f) * px,
            za::clamp(vr_flat_visor_glass_tint.value, 0.f, 1.f), za::clamp(vr_flat_visor_glass_desat.value, 0.f, 1.f));
        GL_Uniform4fFunc(3, tintRgb.x, tintRgb.y, tintRgb.z, 8.f * px);
    }
    else
    {
        GL_Uniform4fFunc(2, 0.f, 0.f, 0.f, 0.f);
    }
    GL_Uniform4fFunc(4, lp.L.w, lp.L.h, lp.L.k, lp.L.zoom);
    composite(x, y, w, h, fades, false);
    if(lp.tintDrawn)
    {
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, lp.tint.fbo);
        glViewport(0, 0, w, h);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        GL_Uniform4fFunc(0, 0.f, 0.f, static_cast<float>(w), static_cast<float>(h));
        composite(0, 0, w, h, fades, true);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebuffer);
        glViewport(x, y, w, h);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // (what GLS_BLEND_ALPHA expects)
    lp.drawnFrame = host_framecount;
    lp.drawnRect = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    GL_EndGroup();
}

void report_f()
{
    const State& s = state;
    Con_Printf("visor: active %d frame %d; sway %.3f %.3f (lag %.3f %.3f, bob %.3f); glitch %.3f fog %.3f underwater %d "
               "fade %.3f alarm %.3f boot %.2f %.2f %.2f; log lines %d\n",
        active() ? 1 : 0, host_framecount, s.sway.x, s.sway.y, s.lag.x, s.lag.y, s.bobAmp, s.glitch, s.fog,
        s.underwater ? 1 : 0, s.fade, s.alarm, s.input.boot[0], s.input.boot[3], s.input.boot[6], gadget::visorLogLines());
    Con_Printf("visor: shake %.3f (%.3f %.3f) logfade %.3f glass %d hits %d health %d\n", shakeLeft(s), s.shake.x, s.shake.y,
        s.logFade, glassPass.on ? 1 : 0, s.hits, static_cast<int>(readHealth()));
    const gadget::VisorSides sides = gadget::visorSides();
    Con_Printf("visor: sides left %d right %d shown %d scale %.2f\n", sides.left, sides.right, sides.rightShown, sides.rightScale);
    const glm::vec2 centerSize = gadget::visorImage(gadget::VisorCenter).size;
    Con_Printf("visor: center fade %.3f pop %.2f pops %d size %.0f %.0f scale %.2f claimed %d\n", s.center.fade, centerPop(),
        s.center.pops, centerSize.x, centerSize.y, layerPass.L.centerScale, s.center.claimed >= host_framecount - 1 ? 1 : 0);
    constexpr const char* names[VisorPanelCount] = {"vitals", "status", "lefthand", "righthand", "log", "sideleft", "sideright", "center"};
    // The edge contact with the sway now (the layer as last drawn, swayed as the composite does).
    Layer L = layerPass.L;
    L.shift = swayShift(L, s.sway, layerPass.tx);
    for(int p = 0; p < VisorPanelCount; p++)
    {
        const Panel& q = s.panels[p];
        const bool contact = layerPass.panels[p].built && edgeContact(L, p, layerPass.panels[p].mount);
        Con_Printf("visor: panel %s drawn %d rect %.1f %.1f %.1f %.1f contact %d (view %d x %d)\n", names[p], q.drawn ? 1 : 0,
            q.rect.x, q.rect.y, q.rect.z, q.rect.w, contact ? 1 : 0, r_refdef.vrect.width, r_refdef.vrect.height);
    }
}

// vr_flat_visor_fx damage <n> | hit <n> | boot | fog [seconds]: the effects on demand (the Debug menu's, the tests').
void fx_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(what, "damage"))
    {
        state.fxDamage = Cmd_Argc() > 2 ? za::max(1, Q_atoi(Cmd_Argv(2))) : 20;
    }
    else if(!q_strcasecmp(what, "hit"))
    {
        // A real hit's damage message, no health taken: the server sends the player's dmg_take as T_Damage leaves it.
        if(edict_t* const pl = progs::localPlayer())
        {
            const progs::ServerVm vm;
            pl->v.dmg_take += static_cast<float>(Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 1, 255) : 20);
            pl->v.dmg_inflictor = EDICT_TO_PROG(pl);
        }
    }
    else if(!q_strcasecmp(what, "boot"))
    {
        state.bootAt = state.clock;
    }
    else if(!q_strcasecmp(what, "fog"))
    {
        state.fxFogHold = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), 0.f, 30.f) : 1.f;
    }
    else if(!q_strcasecmp(what, "center"))
    {
        // A centre print as the game's (SCR_CenterPrint: Quake's timing; the popup's or over the view), "\n" a new line.
        const char* from = Cmd_Argc() > 2 ? Cmd_Argv(2) : "";
        if(!q_strcasecmp(from, "long"))
        {
            from = "The Visor\\n\\nA centre message as long as a mod's tutorial text, its lines wrapped to the panel's "
                   "width (vr_flat_visor_center_width) so that it never runs off the screen.\\nA last short line.";
        }
        za::Array<char, 1024> text{};
        za::SizeT n = 0;
        for(const char* c = from; *c && n + 1 < text.size(); c++)
        {
            const bool newline = c[0] == '\\' && c[1] == 'n';
            text[n++] = newline ? '\n' : *c;
            c += newline ? 1 : 0;
        }
        SCR_CenterPrint(text.data());
    }
    else
    {
        Con_Printf("vr_flat_visor_fx damage [amount] | hit [amount] | boot | fog [seconds] | center <text>|long: the visor's hit "
                   "glitch (hit: a real damage message from the server, no health taken), its power-on sweep, its "
                   "fog held up that long (then clearing), a centre print as the game's (\\n a new line)\n");
    }
}

} // namespace

void onDamage(int armor, int blood)
{
    state.hitDamage += za::max(0, armor) + za::max(0, blood);
}

bool active()
{
    return flatplay::active() && vr_flat_visor.value != 0.f && cls.state == ca_connected && cls.signon == SIGNONS &&
           cl.worldmodel && !cl.intermission && scr_viewsize.value < 120.f && !cl.qcvm.extfuncs.CSQC_DrawHud;
}

bool logShown()
{
    return active() && vr_flat_visor_log.value != 0.f;
}

unsigned tintMask(glm::vec4& rect)
{
    const LayerPass& lp = layerPass;
    if(vr_flat_visor_no_bt_tint.value == 0.f || lp.drawnFrame != host_framecount || !lp.tintDrawn || !active())
    {
        return 0;
    }
    rect = lp.drawnRect;
    return lp.tint.tex;
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

// SCR_CheckDrawCenterString, the game's centre print (`text`, Quake's fade `alpha`) due to be drawn this frame: nonzero
// when the visor's popup shows it instead (vr_flat_visor_centerprint; never the intermission's nor the finale's text).
// A text other than the one shown (or with the popup gone) pops in anew.
extern "C" int VR_CenterPrintOnVisor(const char* text, float alpha)
{
    if(!text || vr_flat_visor_centerprint.value == 0.f || cl.intermission || !visor::active())
    {
        return 0;
    }
    auto& c = visor::state.center;
    if(c.fade <= 0.f || std::strncmp(c.text.data(), text, c.text.size() - 1) != 0)
    {
        q_strlcpy(c.text.data(), text, c.text.size());
        c.popAt = visor::state.clock;
        c.pops++;
    }
    c.claimed = host_framecount;
    c.alpha = alpha;
    return 1;
}

// R_RenderView, after the window's view and its look: the visor over it, into `framebuffer` at (x, y, w, h); the window's
// own view only (never an eye's, nor a teleporter's offscreen camera).
extern "C" void VR_DrawVisor(unsigned framebuffer, int x, int y, int w, int h)
{
    if(stereo::isRenderingEye() || VR_PortalDrawing())
    {
        return;
    }
    visor::menuBlur(framebuffer, x, y, w, h); // (the holster menu's: vr_flatholster.cpp)
    if(!visor::active())
    {
        return;
    }
    visor::draw(framebuffer, x, y, w, h);
}
