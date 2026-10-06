// vr_shock.cpp -- see vr_shock.hpp.
//
// Every arc is a jagged line (a few segments, each end but the last pushed off the straight line at
// random), drawn as a wide faint glow under a thin bright core, as Quad Damage's always were. They
// are reshaped every frame from the frame's number (the same in both eyes), and some flicker out.
// The effects are few at once (the surfaces: one a place, however many bolts go in there; kept a
// moment past the last).

#include "vr_modelmetadata.hpp"
#include "vr_shock.hpp"
#include "vr_modelcollide.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <string.h>


namespace qvr::shock
{
namespace
{

// A lasting shock's arc walking over the body (KindBodyDeath): from a point of its surface (a triangle and where on it,
// so that it follows the body as it moves) to another near it, then on from there.
struct Crawler
{
    int tri{-1}; // -1: not placed yet
    float u{0.f}, v{0.f};
    int next{-1}; // -1: not chosen yet
    float nu{0.f}, nv{0.f};
    double hop{0.0}; // when it steps there (cl.time)
};
constexpr int maxCrawlers = 16;

struct Effect
{
    int kind{KindSurface};
    glm::vec3 org{0.f};
    float radius{0.f};
    double start{0.0};
    double fresh{0.0}; // a lasting shock's last bolt (KindBodyDeath): it wears off from then
    double until{0.0}; // 0: none
    const qmodel_t* model{nullptr}; // a body's (KindBody, KindBodyDeath): its model as first drawn (another: it is gone)
    int arcs{0};                    // a body's: the arcs drawn last frame (vr_shock_info)
    Crawler crawlers[maxCrawlers];  // a lasting shock's walking arcs
};

// The bodies' buffers (the client's frame: the main thread).
struct ShockScratch
{
    za::Vector<glm::vec3> tris;   // a body's triangles as drawn (frame, info_f)
    za::Vector<glm::vec3> arcPts; // the points of a body's arcs drawn this frame (vr_shock_info's measure)
    auto members() { return mem::list(tris, arcPts); }
};
mem::Scratch<ShockScratch> scratch{"shock"};

constexpr int maxEffects = 32; // (lasting shocks on a room's monsters, each hit's, the water's)
Effect effects[maxEffects];

// The player's own shock (KindSelf): from when, until when (cl.time).
double selfStart = 0.0;
double selfUntil = 0.0;

int lastFrame = -1; // effects drawn once a frame, however often the view is set up

// vr_shock_info: the next frame measures where the bodies' arcs lie against their surfaces (measureArcs); meanwhile
// arc() keeps its points in arcRecord.
bool measureNext = false;
za::Vector<glm::vec3>* arcRecord = nullptr;

// The lightning's beams as CL_UpdateTEnts drew them (VR_BeamDrawn), by beam slot: Quad's arcs along them (vr_beam_arcs).
struct BeamSeen
{
    glm::vec3 a{0.f}, b{0.f};
    int frame{-10}; // host_framecount it was drawn in
};
BeamSeen beamsSeen[MAX_BEAMS];
constexpr float beamArcSpacing = 24.f; // units of beam an arc at vr_beam_arcs 1
constexpr float beamArcClear = 12.f;   // units from the beam's start without arcs (the gun's muzzle)
constexpr int maxBeamArcs = 96;        // a beam's at most

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
// arcs, seen from further); `scene`: hidden behind what is in front of it (lines::sceneLine; else drawn over all).
void arc(Random& rnd, glm::vec3 a, const glm::vec3& target, int segments, float jitter, float fade, bool flat = false,
    float width = 1.f, bool lit = false, bool scene = false)
{
    const auto line = scene ? lines::sceneLine : lines::line;
    const auto glowLine = scene ? lines::sceneGlow : lines::glow;
    const float bright = (0.6f + 0.4f * rnd()) * fade;
    const glm::vec4 core{0.75f, 0.85f, 1.f, 0.95f * bright};
    const glm::vec4 glow{0.3f, 0.45f, 1.f, 0.35f * bright};
    if(arcRecord)
    {
        arcRecord->pushBack(a);
    }
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
            glowLine(a, b, 1.2f * width, halo, halo);
        }
        line(a, b, 0.6f * width, glow, glow);
        line(a, b, 0.15f * width, core, core);
        if(arcRecord)
        {
            arcRecord->pushBack(b);
        }
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

// Quad Damage's arcs along a lightning beam from `a` to `b`, round Quake's bolt models: short crackles hugging it here and
// there (as Quad's on the forearms, bigger), some longer ones running along it, a few out of where it strikes. Reshaped
// every frame; some flicker out.
void drawBeamArcs(int index, const glm::vec3& a, const glm::vec3& b)
{
    const float len = glm::distance(a, b);
    const float amount = za::max(0.f, vr_beam_arcs.value);
    if(len < 1.f || amount <= 0.f)
    {
        return;
    }
    const glm::vec3 along = (b - a) / len;
    const float spread = za::clamp(vr_beam_arcs_spread.value, 0.5f, 128.f);
    const float width = za::clamp(vr_beam_arcs_width.value, 0.1f, 8.f);
    Random rnd{static_cast<unsigned>(host_framecount) * 2246822519u + static_cast<unsigned>(index) * 374761393u + 11u};
    const int count = za::clamp(static_cast<int>(len / beamArcSpacing * amount + 0.5f), 1, maxBeamArcs);
    const float from0 = za::min(beamArcClear / len, 0.5f); // (not on the gun the beam comes out of)
    for(int bolt = 0; bolt < count; bolt++)
    {
        if(rnd() < 0.3f)
        {
            continue; // flicker
        }
        const glm::vec3 from = glm::mix(a, b, from0 + (1.f - from0) * rnd()) + rnd.dir() * (spread * 0.3f * rnd());
        const float reach = spread * (0.8f + 1.6f * rnd());
        const glm::vec3 dir = glm::normalize(along * ((rnd() - 0.5f) * 1.6f) + rnd.dir());
        arc(rnd, from, from + dir * reach, 5, reach * 0.18f, 1.f, false, width, true, true);
    }
    // Longer ones along the beam, weaving round it.
    const int runs = za::clamp(static_cast<int>(len / (beamArcSpacing * 4.f) * amount + 0.5f), 0, maxBeamArcs / 4);
    for(int run = 0; run < runs; run++)
    {
        if(rnd() < 0.4f)
        {
            continue;
        }
        const float t0 = from0 + (1.f - from0) * rnd();
        const float t1 = za::min(1.f, t0 + (spread * (3.f + 5.f * rnd())) / len);
        const glm::vec3 p0 = glm::mix(a, b, t0) + rnd.dir() * (spread * 0.4f);
        const glm::vec3 p1 = glm::mix(a, b, t1) + rnd.dir() * (spread * 0.4f);
        arc(rnd, p0, p1, 8, spread * 0.5f, 0.9f, false, width, true, true);
    }
    // Out of where it strikes.
    for(int k = 0; k < 3; k++)
    {
        if(rnd() < 0.35f * amount)
        {
            const float reach = spread * (1.f + 1.5f * rnd());
            arc(rnd, b, b + glm::normalize(rnd.dir() - along * 0.5f) * reach, 5, reach * 0.2f, 1.f, false, width, true, true);
        }
    }
}

// Whether models `a` and `b` are one body's: the same .mdl, or it and its ragdoll's skinned copy ("<model>#rag": the
// client swaps them as the ragdoll is made and gone).
[[nodiscard]] bool sameBody(const qmodel_t* a, const qmodel_t* b)
{
    const char* ha = strchr(a->name, '#');
    const char* hb = strchr(b->name, '#');
    const size_t la = ha ? static_cast<size_t>(ha - a->name) : strlen(a->name);
    const size_t lb = hb ? static_cast<size_t>(hb - b->name) : strlen(b->name);
    return la == lb && !strncmp(a->name, b->name, la);
}

// A body's surface as drawn now (KindBody, KindBodyDeath): its triangles into `tris`; false if it is not there to draw
// on (gone, out of the last update; another model in its slot now: the effect ends).
[[nodiscard]] bool bodyTriangles(Effect& e, za::Vector<glm::vec3>& tris)
{
    const int num = static_cast<int>(e.radius);
    if(num <= 0 || num >= cl.num_entities)
    {
        return false;
    }
    const entity_t& ent = cl_entities[num];
    if(!ent.model || ent.msgtime < cl.mtime[0] - 0.001)
    {
        return false;
    }
    if(!e.model)
    {
        e.model = ent.model;
    }
    else if(e.model != ent.model && !sameBody(e.model, ent.model))
    {
        e.until = 0.0; // (gibbed, or removed and its slot taken: not this body any more)
        return false;
    }
    return modelcollide::drawnTriangles(ent, num, tris) && tris.size() >= 3;
}

// Which way a body's triangles face: +1 if their winding's normals point out of it, -1 if in (Quake's models wind
// them clockwise as seen from outside: in), from where they face against its middle.
[[nodiscard]] float outwardSign(const za::Vector<glm::vec3>& tris)
{
    glm::vec3 mid{0.f};
    for(const glm::vec3& p : tris)
    {
        mid += p;
    }
    mid /= static_cast<float>(za::max<size_t>(tris.size(), 1));
    float facing = 0.f;
    for(size_t i = 0; i + 2 < tris.size(); i += 3)
    {
        const glm::vec3 centre = (tris[i] + tris[i + 1] + tris[i + 2]) / 3.f;
        facing += glm::dot(glm::cross(tris[i + 1] - tris[i], tris[i + 2] - tris[i]), centre - mid);
    }
    return facing < 0.f ? -1.f : 1.f;
}

// The point at `u` `v` on triangle `tri` of `tris` and the way out of the body there (`outward`: outwardSign); false on
// a degenerate one.
[[nodiscard]] bool surfaceAt(const za::Vector<glm::vec3>& tris, size_t tri, float u, float v, float outward, glm::vec3& at,
    glm::vec3& out)
{
    const glm::vec3 a = tris[tri * 3], b = tris[tri * 3 + 1], c = tris[tri * 3 + 2];
    const glm::vec3 cross = glm::cross(b - a, c - a);
    const float area = glm::length(cross);
    if(area < 1e-5f)
    {
        return false;
    }
    at = a + (b - a) * u + (c - a) * v;
    out = cross * (outward / area);
    return true;
}

// A random place on a random triangle of `tris` (`count` of them): its triangle and where on it.
void randomPlace(Random& rnd, size_t count, int& tri, float& u, float& v)
{
    tri = static_cast<int>(za::min(static_cast<size_t>(rnd() * static_cast<float>(count)), count - 1));
    u = rnd();
    v = rnd();
    if(u + v > 1.f)
    {
        u = 1.f - u;
        v = 1.f - v;
    }
}

// A point on a random triangle of `tris` and the way out of the body there; false on a degenerate one.
[[nodiscard]] bool surfacePoint(Random& rnd, const za::Vector<glm::vec3>& tris, float outward, glm::vec3& at, glm::vec3& out)
{
    int tri;
    float u, v;
    randomPlace(rnd, tris.size() / 3, tri, u, v);
    return surfaceAt(tris, static_cast<size_t>(tri), u, v, outward, at, out);
}

// The lasting shock's arcs off the body's surface (units).
constexpr float bodyArcLift = 1.5f; // their feet off the skin (a crackle's: up to as much again)
constexpr float bodyArcBow = 0.2f;  // a long arc's middle out off the body, x its length
constexpr float crawlStep = 11.f;   // a walking arc's step over the body (crawlStepMin..crawlStepMax)
constexpr float crawlStepMin = 5.f, crawlStepMax = 20.f;

// An arc from `a` (out of the body along `na`) to `b` (along `nb`) bowing out between them by `bow` units: it stays
// off the body where the straight line would cut through it (an arm to the chest, round a shoulder).
void bodyArc(Random& rnd, const glm::vec3& a, const glm::vec3& na, const glm::vec3& b, const glm::vec3& nb, float bow,
    float fade, float width)
{
    const float len = glm::distance(a, b);
    glm::vec3 outMid = na + nb;
    const float m = glm::length(outMid);
    outMid = m > 1e-3f ? outMid / m : na;
    const glm::vec3 mid = (a + b) * 0.5f + outMid * bow;
    arc(rnd, a, mid, 3, len * 0.1f, fade, false, width, true, true);
    arc(rnd, mid, b, 3, len * 0.1f, fade, false, width, true, true);
}

// Lightning's lasting shock on a body it struck (KindBodyDeath; alive or dead, carried over its death): Quad Damage's
// arcs crawling over it as over the arms, lifted off the skin to be seen: arcs walking over it from point to point
// (crawlers), short crackles springing off it, now and then a longer one across it, limb to limb; fewer and fainter
// as the shock wears off, with a flickering blue light. Returns the arcs drawn.
int drawBodyDeath(int index, Effect& e, const za::Vector<glm::vec3>& tris, Random& rnd)
{
    const float amount = za::max(0.f, vr_shock_arcs.value);
    if(amount <= 0.f)
    {
        return 0;
    }
    const double now = cl.time;
    const size_t count = tris.size() / 3;
    const float outward = outwardSign(tris);
    const float k = static_cast<float>(za::clamp((e.until - now) / za::max(0.1, e.until - e.fresh), 0.0, 1.0));
    const float fade = effectFade(e.start, e.until) * (0.45f + 0.55f * k);
    const bool surge = rnd() < 0.15f + 0.25f * k; // a jolt: more of them, brighter
    int drawn = 0;
    glm::vec3 centre{0.f};
    int around = 0;

    // Walking arcs: each from where it stands to a point near it, moving on every few hundredths of a second.
    const int walkers = za::min(maxCrawlers, static_cast<int>((5.f + 4.f * k) * amount + 0.5f));
    for(int c = 0; c < walkers; c++)
    {
        Crawler& w = e.crawlers[c];
        if(w.tri < 0 || w.tri >= static_cast<int>(count) || w.next >= static_cast<int>(count))
        {
            randomPlace(rnd, count, w.tri, w.u, w.v);
            w.next = -1;
        }
        if(w.next >= 0 && now >= w.hop)
        {
            w.tri = w.next;
            w.u = w.nu;
            w.v = w.nv;
            w.next = -1;
        }
        glm::vec3 a, na;
        if(!surfaceAt(tris, static_cast<size_t>(w.tri), w.u, w.v, outward, a, na))
        {
            w.tri = -1;
            continue;
        }
        if(w.next < 0)
        {
            // The next step: of a few places tried, the one nearest a step away.
            float best = 1e30f;
            for(int tries = 0; tries < 8; tries++)
            {
                int tri;
                float u, v;
                randomPlace(rnd, count, tri, u, v);
                glm::vec3 b, nb;
                if(!surfaceAt(tris, static_cast<size_t>(tri), u, v, outward, b, nb))
                {
                    continue;
                }
                const float d = glm::distance(a, b);
                const float miss = (d < crawlStepMin || d > crawlStepMax ? 100.f : 0.f) + za::abs(d - crawlStep);
                if(miss < best)
                {
                    best = miss;
                    w.next = tri;
                    w.nu = u;
                    w.nv = v;
                }
            }
            w.hop = now + 0.04 + 0.08 * rnd();
            if(w.next < 0)
            {
                continue;
            }
        }
        glm::vec3 b, nb;
        if(!surfaceAt(tris, static_cast<size_t>(w.next), w.nu, w.nv, outward, b, nb))
        {
            w.next = -1;
            continue;
        }
        centre += a;
        around++;
        if(rnd() < 0.12f)
        {
            continue; // flicker
        }
        const float len = glm::distance(a, b);
        bodyArc(rnd, a + na * bodyArcLift, na, b + nb * bodyArcLift, nb, bodyArcLift + len * bodyArcBow,
            fade * (surge ? 1.f : 0.9f), 1.f);
        drawn++;
    }

    // Crackles springing off the skin, as Quad's over the forearms, out from it at a slant (5..12 units long).
    const int crawl = static_cast<int>((8.f + 14.f * k) * amount * (surge ? 1.5f : 1.f) + 0.5f);
    for(int bolt = 0; bolt < crawl; bolt++)
    {
        glm::vec3 at, out;
        if(!surfacePoint(rnd, tris, outward, at, out))
        {
            continue;
        }
        centre += at;
        around++;
        if(rnd() < 0.25f)
        {
            continue; // flicker
        }
        glm::vec3 along = rnd.dir();
        along -= out * glm::dot(along, out);
        const float len = 5.f + 7.f * rnd();
        const glm::vec3 from = at + out * (bodyArcLift * (1.f + rnd()));
        const glm::vec3 to = from + glm::normalize(along + out * (0.6f + 0.6f * rnd()) + 1e-3f) * len;
        arc(rnd, from, to, 5, len * 0.15f, fade * (surge ? 1.f : 0.85f), false, 0.9f, true, true);
        drawn++;
    }
    // Longer ones across the body, from one part to another not far off, bowing out over it.
    const int across = static_cast<int>((1.f + 3.f * k) * amount * (surge ? 2.f : 1.f) + 0.5f);
    for(int bolt = 0; bolt < across; bolt++)
    {
        glm::vec3 a, na, b, nb;
        if(!surfacePoint(rnd, tris, outward, a, na) || !surfacePoint(rnd, tris, outward, b, nb) || rnd() < 0.35f)
        {
            continue;
        }
        const float len = glm::distance(a, b);
        if(len < 6.f || len > 40.f)
        {
            continue;
        }
        bodyArc(rnd, a + na * bodyArcLift, na, b + nb * bodyArcLift, nb, bodyArcLift + len * bodyArcBow, fade, 1.f);
        drawn++;
    }
    if(around > 0)
    {
        light(index, centre / static_cast<float>(around), (60.f + 90.f * k) * (surge ? 1.3f : 1.f), fade, rnd);
    }
    return drawn;
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
        const bool body = kind == KindBody || kind == KindBodyDeath;
        if(e.until > now && e.kind == kind && (body ? e.radius == radius : glm::distance(e.org, org) < 24.f))
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
        effects[slot] = Effect{};
        effects[slot].start = now;
    }
    Effect& e = effects[slot];
    e.fresh = now; // (a lasting shock struck again: wearing off from now)
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
    const float duration = static_cast<float>(MSG_ReadByte()) / (kind == KindBodyDeath ? 4.f : 50.f);
    add(kind, {org[0], org[1], org[2]}, radius, duration);
}

// The point of triangle a b c nearest `p` (Ericson, Real-Time Collision Detection 5.1.5).
[[nodiscard]] glm::vec3 nearestOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f) { return a; }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3) { return b; }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f) { return a + ab * (d1 / (d1 - d3)); }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6) { return c; }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f) { return a + ac * (d2 / (d2 - d6)); }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) { return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))); }
    const float denom = 1.f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

// Whether `p` is inside the closed surface `tris`: the most of three rays out of it cross it an odd number of times.
[[nodiscard]] bool insideSurface(const glm::vec3& p, const za::Vector<glm::vec3>& tris)
{
    constexpr glm::vec3 rays[3] = {{0.123f, 0.271f, 0.954f}, {0.913f, -0.311f, 0.264f}, {-0.402f, 0.817f, -0.413f}};
    int odd = 0;
    for(const glm::vec3& d : rays)
    {
        int crossings = 0;
        for(size_t i = 0; i + 2 < tris.size(); i += 3)
        {
            const glm::vec3 e1 = tris[i + 1] - tris[i], e2 = tris[i + 2] - tris[i];
            const glm::vec3 h = glm::cross(d, e2);
            const float det = glm::dot(e1, h);
            if(za::abs(det) < 1e-8f) { continue; }
            const glm::vec3 s = p - tris[i];
            const float u = glm::dot(s, h) / det;
            if(u < 0.f || u > 1.f) { continue; }
            const glm::vec3 q = glm::cross(s, e1);
            const float v = glm::dot(d, q) / det;
            if(v < 0.f || u + v > 1.f) { continue; }
            crossings += glm::dot(e2, q) / det > 0.f ? 1 : 0;
        }
        odd += crossings & 1;
    }
    return odd >= 2;
}

// vr_shock_info's measure: where entity `num`'s arcs drawn this frame (`count` points from `first` of arcPts) lie
// against its surface `tris`: how many inside it, their mean distance off it (outside +, inside -), the share at least
// 1 unit out.
void measureArcs(int num, const za::Vector<glm::vec3>& tris, size_t first, size_t count)
{
    const za::Vector<glm::vec3>& pts = scratch.arcPts;
    int inside = 0, clear = 0;
    double sum = 0.0;
    for(size_t i = first; i < first + count && i < pts.size(); i++)
    {
        const glm::vec3& p = pts[i];
        float best = 1e30f;
        for(size_t t = 0; t + 2 < tris.size(); t += 3)
        {
            best = za::min(best, glm::distance(p, nearestOnTriangle(p, tris[t], tris[t + 1], tris[t + 2])));
        }
        const bool in = insideSurface(p, tris);
        inside += in ? 1 : 0;
        clear += !in && best >= 1.f ? 1 : 0;
        sum += in ? -best : best;
    }
    const float n = static_cast<float>(za::max<size_t>(count, 1));
    Con_Printf("bodyshock-arcs: entity=%d points=%d inside=%.2f mean_out=%.2f clear1=%.2f\n", num, static_cast<int>(count),
        static_cast<float>(inside) / n, static_cast<float>(sum) / n, static_cast<float>(clear) / n);
}

void frame(const hands::State& s)
{
    if(host_framecount == lastFrame)
    {
        return;
    }
    lastFrame = host_framecount;
    QVR_PROFILE("shock arcs");

    Random rnd{static_cast<unsigned>(host_framecount) * 2246822519u + 7u};
    drawSelf(s, rnd);
    for(int i = 0; i < MAX_BEAMS; i++)
    {
        const BeamSeen& b = beamsSeen[i];
        if(host_framecount - b.frame <= 1) // (drawn this frame, or the last if the view comes first)
        {
            drawBeamArcs(i, b.a, b.b);
        }
    }
    const double now = cl.time;
    const bool measure = measureNext;
    measureNext = false;
    for(int i = 0; i < maxEffects; i++)
    {
        Effect& e = effects[i];
        if(e.until <= now)
        {
            continue;
        }
        if(e.kind == KindBody || e.kind == KindBodyDeath)
        {
            za::Vector<glm::vec3>& tris = scratch.tris;
            e.arcs = 0;
            if((e.kind == KindBodyDeath && vr_shock_arcs.value <= 0.f) || !bodyTriangles(e, tris))
            {
                continue;
            }
            scratch.arcPts.clear();
            arcRecord = measure ? &scratch.arcPts : nullptr;
            if(e.kind == KindBodyDeath)
            {
                e.arcs = drawBodyDeath(i, e, tris, rnd);
                arcRecord = nullptr;
                if(measure)
                {
                    measureArcs(static_cast<int>(e.radius), tris, 0, scratch.arcPts.size());
                }
                continue;
            }
            const float fade = za::clamp(static_cast<float>((e.until - now) / 0.25), 0.f, 1.f);
            const float outward = outwardSign(tris);
            for(int bolt = 0; bolt < 12; bolt++)
            {
                const size_t index = static_cast<size_t>(rnd() * (tris.size() / 3)) * 3;
                const glm::vec3 a = tris[index], b = tris[index + 1], c = tris[index + 2];
                const glm::vec3 cross = glm::cross(b - a, c - a);
                if(glm::length(cross) < 1e-5f) { continue; }
                const glm::vec3 n = glm::normalize(cross) * outward; // (off the skin: 1 unit)
                const glm::vec3 from = glm::mix(a, b, rnd()) + n;
                const glm::vec3 to = glm::mix(a, c, rnd()) + n;
                arc(rnd, from, to, 4, 0.4f, fade, false, 0.45f, true, true);
                e.arcs++;
            }
            arcRecord = nullptr;
            if(measure)
            {
                measureArcs(static_cast<int>(e.radius), tris, 0, scratch.arcPts.size());
            }
        }
        else if(e.kind == KindBurst)
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
    for(BeamSeen& b : beamsSeen)
    {
        b = BeamSeen{};
    }
    selfStart = selfUntil = 0.0;
}

// vr_shock_info: the bodies with arcs on them now: entity, kind (3 a hit's, 4 lasting), triangles, arcs drawn last
// frame, seconds left.
void info_f()
{
    int active = 0;
    for(const auto& effect : effects)
    {
        if((effect.kind != KindBody && effect.kind != KindBodyDeath) || effect.until <= cl.time) { continue; }
        const int num = static_cast<int>(effect.radius);
        if(num <= 0 || num >= cl.num_entities) { continue; }
        za::Vector<glm::vec3>& triangles = scratch.tris;
        modelcollide::drawnTriangles(cl_entities[num], num, triangles);
        active++;
        Con_Printf("bodyshock: entity=%d kind=%d triangles=%d arcs=%d remaining=%.2f\n", num, effect.kind,
            static_cast<int>(triangles.size() / 3), effect.arcs, effect.until - cl.time);
    }
    Con_Printf("bodyshock: active=%d\n", active);
    measureNext = true; // (the next frame's arcs: bodyshock-arcs)
}

void registerCommands()
{
    Cmd_AddCommand("vr_shock_test", test_f);
    Cmd_AddCommand("vr_shock_info", info_f);
}

} // namespace qvr::shock

// A lightning beam (Quake's bolt models: the lightning gun's, a shambler's, Chthon's) drawn this frame between these ends:
// Quad Damage's arcs along it in the next view (vr_beam_arcs).
extern "C" void VR_BeamDrawn(int index, qmodel_t* model, const float* start, const float* end)
{
    if(index < 0 || index >= MAX_BEAMS || !model || !qvr::modelmeta::has(model, qvr::modelmeta::Trait::Bolt))
    {
        return;
    }
    qvr::shock::BeamSeen& b = qvr::shock::beamsSeen[index];
    b.a = glm::vec3{start[0], start[1], start[2]};
    b.b = glm::vec3{end[0], end[1], end[2]};
    b.frame = host_framecount;
}
