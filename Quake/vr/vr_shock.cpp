// vr_shock.cpp -- see vr_shock.hpp.
//
// Every arc is a jagged line (a few segments, each end but the last pushed off the straight line at
// random), drawn as a wide faint glow under a thin bright core, as Quad Damage's always were. They
// are reshaped every frame from the frame's number (the same in both eyes), and some flicker out.
// The effects are few at once (the surfaces: one a place, however many bolts go in there; kept a
// moment past the last).

#include "vr_shock.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"


namespace qvr::shock
{
namespace
{

struct Effect
{
    int kind{KindSurface};
    glm::vec3 org{0.f};
    float radius{0.f};
    double start{0.0};
    double until{0.0}; // 0: none
};

constexpr int maxEffects = 16;
Effect effects[maxEffects];

// The player's own shock (KindSelf): from when, until when (cl.time).
double selfStart = 0.0;
double selfUntil = 0.0;

int lastFrame = -1; // effects drawn once a frame, however often the view is set up

// A frame's random numbers: 0..1, the same sequence for the same seed.
struct Random
{
    unsigned seed;

    float operator()()
    {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
    }

    glm::vec3 dir()
    {
        const float x = (*this)() - 0.5f, y = (*this)() - 0.5f, z = (*this)() - 0.5f;
        return glm::normalize(glm::vec3{x, y, z} + 1e-3f);
    }

    glm::vec3 flatDir()
    {
        const float a = (*this)() * 6.2831853f;
        return {za::cos(a), za::sin(a), 0.f};
    }
};

// A jagged arc from `a` to `target`: `segments`, each end but the last pushed off by up to `jitter` units (`flat`: only
// sideways and up, over a surface). `fade` dims it.
// `width` scales the lines (1: Quad's, on the arms); `lit`: a bright halo added onto the scene round it too (the world's
// arcs, seen from further).
void arc(Random& rnd, glm::vec3 a, const glm::vec3& target, int segments, float jitter, float fade, bool flat = false,
    float width = 1.f, bool lit = false)
{
    const float bright = (0.6f + 0.4f * rnd()) * fade;
    const glm::vec4 core{0.75f, 0.85f, 1.f, 0.95f * bright};
    const glm::vec4 glow{0.3f, 0.45f, 1.f, 0.35f * bright};
    for(int seg = 1; seg <= segments; seg++)
    {
        glm::vec3 b = glm::mix(a, target, static_cast<float>(seg) / static_cast<float>(segments));
        if(seg < segments)
        {
            if(flat)
            {
                b += rnd.flatDir() * (jitter * rnd());
                b.z += jitter * 0.15f * rnd();
            }
            else
            {
                b += rnd.dir() * jitter;
            }
        }
        if(lit)
        {
            const glm::vec4 halo{0.25f * bright, 0.4f * bright, 0.9f * bright, 1.f};
            lines::glow(a, b, 1.2f * width, halo, halo);
        }
        lines::line(a, b, 0.6f * width, glow, glow);
        lines::line(a, b, 0.15f * width, core, core);
        a = b;
    }
}

[[nodiscard]] float effectFade(double start, double until)
{
    const double now = cl.time;
    const float in = static_cast<float>(za::clamp((now - start) / 0.04, 0.0, 1.0));
    const float out = static_cast<float>(za::clamp((until - now) / 0.12, 0.0, 1.0));
    return za::min(in, out);
}

// A flickering blue light over the effect.
void light(int index, const glm::vec3& at, float radius, float fade, Random& rnd)
{
    dlight_t* dl = CL_AllocDlight(-(4200 + index));
    dl->origin[0] = at.x;
    dl->origin[1] = at.y;
    dl->origin[2] = at.z + 8.f;
    dl->radius = radius * fade * (0.7f + 0.3f * rnd());
    dl->die = static_cast<float>(cl.time + 0.05);
    dl->decay = 0.f;
    dl->minlight = 0.f;
    dl->color[0] = 0.55f;
    dl->color[1] = 0.7f;
    dl->color[2] = 1.f;
}

// Arcs over a liquid's surface: out from the point the beam goes in, branching, and a few crawling further out.
void drawSurface(int index, const Effect& e, Random& rnd)
{
    const float fade = effectFade(e.start, e.until);
    if(fade <= 0.f)
    {
        return;
    }
    const float reach = za::clamp(e.radius * 0.45f, 24.f, 160.f);
    const glm::vec3 o = e.org + glm::vec3{0.f, 0.f, 0.75f};
    for(int bolt = 0; bolt < 9; bolt++)
    {
        if(rnd() < 0.25f)
        {
            continue; // flicker
        }
        const glm::vec3 dir = rnd.flatDir();
        const float len = reach * (0.3f + 0.7f * rnd());
        const glm::vec3 end = o + dir * len;
        const int segments = za::clamp(static_cast<int>(len / 10.f), 4, 14);
        arc(rnd, o, end, segments, za::max(3.f, len * 0.12f), fade, true, 2.5f, true);
        if(rnd() < 0.6f)
        {
            const glm::vec3 from = glm::mix(o, end, 0.3f + 0.5f * rnd());
            const glm::vec3 branch = glm::normalize(dir + rnd.flatDir() * 0.9f);
            arc(rnd, from, from + branch * (len * (0.2f + 0.3f * rnd())), 4, len * 0.08f, fade * 0.8f, true, 2.f, true);
        }
    }
    for(int spark = 0; spark < 6; spark++) // loose sparks crawling over the water further out
    {
        if(rnd() < 0.4f)
        {
            continue;
        }
        const glm::vec3 at = o + rnd.flatDir() * (reach * (0.5f + 0.7f * rnd()));
        arc(rnd, at, at + rnd.flatDir() * (6.f + 10.f * rnd()), 3, 3.f, fade * 0.7f, true, 1.6f, true);
    }
    light(index, e.org, za::clamp(e.radius, 100.f, 300.f), fade, rnd);
}

// Arcs out from a point in the liquid, every way (the shock's source).
void drawBurst(int index, const Effect& e, Random& rnd)
{
    const float fade = effectFade(e.start, e.until);
    if(fade <= 0.f)
    {
        return;
    }
    const float reach = za::clamp(e.radius * 0.3f, 16.f, 110.f);
    for(int bolt = 0; bolt < 10; bolt++)
    {
        if(rnd() < 0.3f)
        {
            continue;
        }
        const float len = reach * (0.3f + 0.7f * rnd());
        arc(rnd, e.org, e.org + rnd.dir() * len, za::clamp(static_cast<int>(len / 10.f), 4, 10), len * 0.1f, fade, false,
            2.5f, true);
    }
    light(index, e.org, za::clamp(e.radius, 120.f, 320.f), fade, rnd);
}

// The player shocked: a flickering blue flash over the view (Quake's colour shift, as the pickup flash), a few arcs in
// front of the eyes, arcs over the hands and forearms (thicker than Quad's) and round the body.
void drawSelf(const hands::State& s, Random& rnd)
{
    const double now = cl.time;
    if(now >= selfUntil)
    {
        return;
    }
    const float k = static_cast<float>(za::clamp((selfUntil - now) / za::max(0.05, selfUntil - selfStart), 0.0, 1.0));
    const float flash = za::clamp(vr_lg_water_flash.value, 0.f, 1.f);

    if(flash > 0.f)
    {
        cshift_t& shift = cl.cshifts[CSHIFT_BONUS];
        shift.destcolor[0] = 150;
        shift.destcolor[1] = 185;
        shift.destcolor[2] = 255;
        const float percent = flash * (0.35f + 0.65f * k) * (rnd() < 0.25f ? 8.f : 25.f + 35.f * rnd());
        shift.percent = za::max(shift.percent, percent);
    }

    if(!s.valid)
    {
        return;
    }
    armArcs(s, rnd.seed ^ 0x5bd1e995u, 5 + static_cast<int>(7.f * k), 0.2f + 0.5f * k);
    const float m2w = units::metresToUnits() * units::bodyScale();

    // Round the body: the chest down to the hips, under the head.
    const glm::vec3 up{0.f, 0.f, 1.f};
    for(int bolt = 0; bolt < 6; bolt++)
    {
        if(rnd() < 0.35f + 0.4f * (1.f - k))
        {
            continue;
        }
        const glm::vec3 around = rnd.flatDir();
        const glm::vec3 a = s.head - up * ((0.3f + 0.45f * rnd()) * m2w) + around * (0.16f * m2w);
        arc(rnd, a, a + rnd.dir() * ((0.06f + 0.1f * rnd()) * m2w), 5, 0.015f * m2w, k);
    }

    // In front of the eyes: across the view, near (a third of a metre).
    if(flash > 0.f)
    {
        glm::vec3 fwd, right, vup;
        hands::angleVectors(s.headAngles, fwd, right, vup);
        const glm::vec3 centre = s.head + fwd * (0.35f * m2w);
        const int bolts = static_cast<int>(3.f * flash * k + 0.5f);
        for(int bolt = 0; bolt < bolts; bolt++)
        {
            if(rnd() < 0.4f)
            {
                continue;
            }
            const glm::vec3 a =
                centre + right * ((rnd() - 0.5f) * 0.5f * m2w) + vup * ((rnd() - 0.5f) * 0.4f * m2w);
            const glm::vec3 b = a + (right * (rnd() - 0.5f) + vup * (rnd() - 0.5f)) * (0.25f * m2w);
            arc(rnd, a, b, 6, 0.02f * m2w, k * flash, false, 0.5f);
        }
    }
}

void add(int kind, const glm::vec3& org, float radius, float duration)
{
    const double now = cl.time;
    if(kind == KindSelf)
    {
        if(now >= selfUntil)
        {
            selfStart = now;
        }
        selfUntil = za::max(selfUntil, now + duration);
        return;
    }

    // One a place: the next bolt going in there keeps it going.
    int slot = -1;
    for(int i = 0; i < maxEffects; i++)
    {
        const Effect& e = effects[i];
        if(e.until > now && e.kind == kind && glm::distance(e.org, org) < 24.f)
        {
            slot = i;
            break;
        }
    }
    if(slot < 0)
    {
        double oldest = 1e30;
        for(int i = 0; i < maxEffects; i++)
        {
            if(effects[i].until <= now)
            {
                slot = i;
                break;
            }
            if(effects[i].until < oldest)
            {
                oldest = effects[i].until;
                slot = i;
            }
        }
        effects[slot].start = now;
    }
    Effect& e = effects[slot];
    e.kind = kind;
    e.org = org;
    e.radius = radius;
    e.until = za::max(e.until > now ? e.until : 0.0, now + duration);
}

// vr_shock_test <kind> [radius] [duration]
void test_f()
{
    if(cls.state != ca_connected || !cl.worldmodel)
    {
        return;
    }
    const int kind = Cmd_Argc() > 1 ? CLAMP(0, Q_atoi(Cmd_Argv(1)), 2) : KindSelf;
    const float radius = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : vr_lg_water_radius.value;
    const float duration = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : (kind == KindSurface ? 3.f : 1.f);

    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up); // the view's (the head's)
    glm::vec3 at{r_refdef.vieworg[0] + fwd[0] * 128.f, r_refdef.vieworg[1] + fwd[1] * 128.f, r_refdef.vieworg[2]};
    if(kind == KindSurface)
    {
        // Down to the liquid below the point ahead: its surface (to a unit).
        bool found = false;
        for(float down = 0.f; down < 1024.f; down += 4.f)
        {
            vec3_t p{at.x, at.y, at.z - down};
            const int c = Mod_PointInLeaf(p, cl.worldmodel)->contents;
            if(c == CONTENTS_SOLID)
            {
                break;
            }
            if(c == CONTENTS_WATER || c == CONTENTS_SLIME || c == CONTENTS_LAVA)
            {
                for(float fine = 0.f; fine < 4.f; fine += 1.f)
                {
                    vec3_t q{at.x, at.y, at.z - down + 4.f - fine};
                    const int cq = Mod_PointInLeaf(q, cl.worldmodel)->contents;
                    if(cq == CONTENTS_WATER || cq == CONTENTS_SLIME || cq == CONTENTS_LAVA)
                    {
                        at.z = q[2];
                        break;
                    }
                }
                found = true;
                break;
            }
        }
        if(!found)
        {
            Con_Printf("vr_shock_test: no liquid below the point ahead\n");
            return;
        }
    }
    Con_Printf("vr_shock_test: kind %d at %.0f %.0f %.0f, radius %.0f, %.1f s\n", kind, at.x, at.y, at.z, radius, duration);
    add(kind, at, radius, duration);
}

} // namespace

void armArcs(const hands::State& s, unsigned seed, int bolts, float longChance)
{
    Random rnd{seed};
    const float m2w = units::metresToUnits() * units::bodyScale();
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 wrist = s.pos[hand];
        glm::vec3 dir = hands::forward(s.rot[hand]);
        if(glm::vec3 w, d; avatar::forearm(hand, w, d))
        {
            wrist = w;
            dir = glm::normalize(d);
        }
        const glm::vec3 elbow = wrist - dir * (0.26f * m2w);
        const glm::vec3 fingers = s.pos[hand] + hands::forward(s.rot[hand]) * (0.05f * m2w);

        for(int bolt = 0; bolt < bolts; bolt++)
        {
            if(rnd() < 0.3f)
            {
                continue; // flicker
            }
            const float along = rnd();
            glm::vec3 a = along < 0.25f ? fingers : glm::mix(wrist, elbow, (along - 0.25f) / 0.75f);
            a += rnd.dir() * (0.03f * m2w);
            arc(rnd, a, a + rnd.dir() * ((0.04f + 0.06f * rnd()) * m2w), 5, 0.012f * m2w, 1.f);
        }
        if(rnd() < longChance)
        {
            arc(rnd, fingers + rnd.dir() * (0.02f * m2w), glm::mix(wrist, elbow, 0.5f + 0.5f * rnd()) + rnd.dir() * (0.03f * m2w),
                8, 0.02f * m2w, 1.f);
        }
    }
}

void parse()
{
    const int kind = MSG_ReadByte();
    vec3_t org;
    for(int i = 0; i < 3; i++)
    {
        org[i] = MSG_ReadCoord(cl.protocolflags);
    }
    const float radius = static_cast<float>(MSG_ReadShort());
    const float duration = static_cast<float>(MSG_ReadByte()) / 50.f;
    add(kind, {org[0], org[1], org[2]}, radius, duration);
}

void frame(const hands::State& s)
{
    if(host_framecount == lastFrame)
    {
        return;
    }
    lastFrame = host_framecount;

    Random rnd{static_cast<unsigned>(host_framecount) * 2246822519u + 7u};
    drawSelf(s, rnd);
    const double now = cl.time;
    for(int i = 0; i < maxEffects; i++)
    {
        const Effect& e = effects[i];
        if(e.until <= now)
        {
            continue;
        }
        if(e.kind == KindBurst)
        {
            drawBurst(i, e, rnd);
        }
        else
        {
            drawSurface(i, e, rnd);
        }
    }
}

void clear()
{
    for(Effect& e : effects)
    {
        e = Effect{};
    }
    selfStart = selfUntil = 0.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_shock_test", test_f);
}

} // namespace qvr::shock
