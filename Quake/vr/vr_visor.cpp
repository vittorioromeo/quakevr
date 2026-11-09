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

#include "vr_visor_mounts.inc" // the mounts on the edges and the lens

// The mounts and their contents (vr_flat_visor_panels: which), the sway moving them all, through the lens.
void drawPanels(int viewW, int viewH, float tx)
{
    Layer L;
    L.w = static_cast<float>(viewW);
    L.h = static_cast<float>(viewH);
    const float scale = za::clamp(vr_flat_visor_scale.value, 0.5f, 2.f);
    // A virtual pixel: the vitals' 62 rows 11.5% of the view's height at scale 1; smaller where the bottom's three
    // mounts (527 across with their gaps) or the top's two (318) would not fit across.
    L.unit = za::min(za::min(static_cast<float>(viewH) * 0.115f * scale / 62.f, L.w / 527.f), L.w / 318.f);
    L.k = za::clamp(vr_flat_visor_warp.value, 0.f, 0.3f);
    L.overhang = {0.3f * L.w / L.unit, 0.3f * L.h / L.unit}; // (past 1 + k and the sway at every aspect)
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
    const float width = za::max(1.5f, L.unit * 0.8f);

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
        // The vitals' mount pulses red at low health (its face and its line).
        const float alarm = p == gadget::VisorVitals ? state.alarm : 0.f;
        const glm::vec3 faceRgb = glm::mix(colors.face, glm::vec3{0.16f, 0.015f, 0.01f}, za::clamp(alarm, 0.f, 1.f) * 0.85f);
        const glm::vec3 lineRgb = glm::mix(colors.line, colors.warn, za::clamp(alarm * 1.5f, 0.f, 1.f));
        drawMount(L, p, m, {faceRgb, opacity * mountFade}, {lineRgb, 0.9f * mountFade}, width);
        drawContents(L, p, m, img,
            {.shade = gfx::Shade::Screen, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false,
                .params = {time, crt, g, gadget::textGlow()}, .screen = {img.size.x, img.size.y, 0.5f}, .trueColor = true,
                .inkAlpha = fade},
            phosphor, out.rect);
        out.contact = edgeContact(L, p, m);
        out.drawn = true;
    }
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
    drawPanels(w, h, ty * aspect);
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
