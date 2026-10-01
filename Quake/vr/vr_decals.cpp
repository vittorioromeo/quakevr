// vr_decals.cpp -- see vr_decals.hpp.

#include "vr_decals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_gore.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_modellight.hpp"
#include "vr_profile.hpp"
#include "vr_ring.hpp"
#include "vr_trace.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"


namespace qvr::decals
{
namespace
{

enum Kind : int
{
    Blood,
    BloodDrop,
    Scorch,
    Hole,
    // The gore's (vr_gore.cpp):
    Splatter, // a hit's spray: a blot and droplets flung along the cell's +x (the spray's way)
    Streak,   // runs of blood down a wall, from a blot at the cell's -x end to +x (down)
    Pool,     // a pool of blood, thick and dark
    Splotch,  // a gib's splat: a big blot with spikes and drops all round
    KindCount
};

// The atlas: 8 x 4 cells of 256 texels (2048 x 1024 RGBA, mipmapped: 11 MB). Blood splats 0-5,
// drops 6-7, scorches 8-10, chips 11-15, splatters 16-19, streaks 20-22, pools 23-25, splotches 26-28.
constexpr int cellSize = 256;
constexpr int cellsPerRow = 8;
constexpr int cellRows = 4;
constexpr int atlasWidth = cellSize * cellsPerRow;
constexpr int atlasHeight = cellSize * cellRows;
constexpr int firstCell[KindCount] = {0, 6, 8, 11, 16, 20, 23, 26};
constexpr int cellCount[KindCount] = {6, 2, 3, 5, 4, 3, 3, 3};
// Every kind has cells, contiguous, and they all fit in the atlas (buildAtlas writes each cell's texels; a cell past the
// last row would write past the image).
constexpr bool atlasCellsValid()
{
    for(int k = 0; k < KindCount; k++)
    {
        if(cellCount[k] < 1 || (k + 1 < KindCount && firstCell[k] + cellCount[k] != firstCell[k + 1]))
        {
            return false;
        }
    }
    return firstCell[KindCount - 1] + cellCount[KindCount - 1] <= cellsPerRow * cellRows;
}
static_assert(atlasCellsValid(), "decal atlas: a kind without cells, cells not contiguous, or no room: add a row");

// The chips are lit from the cell's +y (the decal's v, turned towards the light that reaches it),
// this high above the surface.
const glm::vec3 chipLight = glm::normalize(glm::vec3{0.f, 0.8f, 0.6f});

gfx::Texture atlas = 0;

// A corner of a decal's triangles: on a face of the world, where in the decal's cell (0..1).
struct Corner
{
    glm::vec3 pos;
    glm::vec2 uv;
};

// A mark's buffers, laid on the world (the main thread's; the atlas's worker has its own).
struct DecalScratch
{
    za::Vector<glm::vec3> poly, clipped; // a face's polygon cut to the mark (clipToWorld)
    za::Vector<Corner> tris;             // the mark's triangles (add)
    auto members() { return qvr::mem::list(poly, clipped, tris); }
};
mem::Scratch<DecalScratch> scratch{"decals"};

struct Decal
{
    glm::vec3 centre, u, v; // u, v: half extents on the surface (at its full size)
    int cell;
    double born;            // when it shows (a delayed one later than it was placed)
    float grow = 0.f;       // seconds it takes to spread to its full size (0: at once),
    float growFrom = 1.f;   // from this part of it
    bool fromStart = false; // spreading from its -u end (a run down a wall), not its middle
    float darken = 0.f;     // how much darker it gets as it spreads and dries
    za::Vector<Corner> tris; // its footprint clipped to the world's faces under it (clipToWorld)
};

// Oldest first (drawn in that order; the oldest go first): a ring of vr_decal_max slots, whose marks' triangles keep
// their buffers for the next mark laid there.
Ring<Decal> decals;
int dropCount = 0; // the blood drops among them

[[nodiscard]] bool isDrop(const Decal& d)
{
    return d.cell >= firstCell[BloodDrop] && d.cell < firstCell[BloodDrop] + cellCount[BloodDrop];
}

void popOldest()
{
    dropCount -= isDrop(decals.front());
    decals.popFront();
}
za::Vector<gfx::Vertex> vertices;       // the marks that change this frame
za::Vector<gfx::Vertex> staticVertices; // the others,
gfx::StaticTriangles staticTriangles;     // in their own buffer: uploaded when they change, not for each eye and frame
bool staticDirty = false;                 // staticVertices changed since they were uploaded
double staticUntil = 0.0;                // when one of those starts to change (fades)
double staticLife = 0.0;                 // vr_decal_life they were built for
int builtFrame = -1; // the host frame `vertices` were built in; -1 when decals came or went since
int addedThisFrame = 0;
int addedFrame = -1;
int goreFrame = -1; // the host frame gore::frame last ran in

// ---- The atlas ---------------------------------------------------------------------------------

thread_local unsigned rngState = 12345u; // (the atlas is made on a worker thread, and by vr_decal_atlas)
[[nodiscard]] float frand()
{
    rngState = rngState * 1664525u + 1013904223u;
    return static_cast<float>(rngState >> 8) / 16777216.f;
}

[[nodiscard]] float hash(int i, int seed)
{
    unsigned h = static_cast<unsigned>(i) * 374761393u + static_cast<unsigned>(seed) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xFFFF) / 65535.f;
}

// Smooth periodic noise round a circle (`a` in turns), 0..1.
[[nodiscard]] float ring(float a, int lobes, int seed)
{
    const float x = a * lobes;
    const int i = static_cast<int>(za::floor(x));
    const float f = x - i;
    const float s = f * f * (3.f - 2.f * f);
    return hash(((i % lobes) + lobes) % lobes, seed) * (1.f - s) + hash((((i + 1) % lobes) + lobes) % lobes, seed) * s;
}

[[nodiscard]] float smoothstep(float e0, float e1, float x)
{
    const float t = za::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Smooth value noise over the plane, 0..1.
[[nodiscard]] float noise(glm::vec2 p, int seed)
{
    const int x = static_cast<int>(za::floor(p.x)), y = static_cast<int>(za::floor(p.y));
    const float fx = p.x - x, fy = p.y - y;
    const float sx = fx * fx * (3.f - 2.f * fx), sy = fy * fy * (3.f - 2.f * fy);
    const auto h = [seed](int i, int j) { // (the lattice point mixed in unsigned arithmetic: it wraps)
        return hash(static_cast<int>(static_cast<unsigned>(i) * 7919u + static_cast<unsigned>(j) * 104729u), seed);
    };
    return (h(x, y) * (1.f - sx) + h(x + 1, y) * sx) * (1.f - sy) + (h(x, y + 1) * (1.f - sx) + h(x + 1, y + 1) * sx) * sy;
}

[[nodiscard]] float segmentDistance(glm::vec2 p, glm::vec2 a, glm::vec2 b, float& along)
{
    const glm::vec2 ab = b - a;
    along = za::clamp(glm::dot(p - a, ab) / za::max(glm::dot(ab, ab), 1e-8f), 0.f, 1.f);
    return glm::length(p - a - ab * along);
}

struct Blob
{
    glm::vec2 c;
    float r;
    glm::vec2 stretch; // elongation direction times its amount
};

// A chip's crack: a jagged line out from the crater, its width tapering from `width`.
struct Crack
{
    za::Vector<glm::vec2> points;
    float width;
};

// A chip's cracks and the flecks of debris round it (small blobs).
struct ChipShape
{
    za::Vector<Crack> cracks;
    za::Vector<Blob> flecks;
};

// A chip, variant `seed`: a crater knocked into the surface, its rim thrown up round it, cracks
// running out and a halo of dust and flecks. Its relief (the crater's depth, the rim's height, in
// cell units) is shaded by `chipLight`: the crater's far wall and the rim towards the light lit,
// the others in shadow. What the surface is multiplied by at `p` (-1..1), 0..2.
[[nodiscard]] glm::vec3 chipTexel(int seed, const ChipShape& shape, glm::vec2 p)
{
    const float radius = 0.2f + 0.05f * hash(1, seed); // the crater's

    // The crater's radius where `q` is, over it: a ragged hole.
    const auto relative = [&](glm::vec2 q) {
        const float a = za::atan2(q.y, q.x) / 6.2831853f + 0.5f;
        const float edge = radius * (1.f + 0.22f * (ring(a, 7, seed) - 0.5f) + 0.12f * (ring(a, 19, seed + 1) - 0.5f));
        return glm::length(q) / edge;
    };
    const auto height = [&](glm::vec2 q) {
        const float rr = relative(q);
        float h = rr < 1.f ? -0.1f * (1.f - rr * rr) : 0.f;                      // the bowl
        h += 0.022f * za::exp(-(rr - 1.05f) * (rr - 1.05f) / 0.09f);            // the rim
        h += 0.012f * (noise(q * 26.f, seed + 2) - 0.5f) * za::max(0.f, 1.3f - rr); // broken, rough
        return h;
    };

    const float e = 1.5f / cellSize;
    const float dx = (height(p + glm::vec2{e, 0.f}) - height(p - glm::vec2{e, 0.f})) / (2.f * e);
    const float dy = (height(p + glm::vec2{0.f, e}) - height(p - glm::vec2{0.f, e})) / (2.f * e);
    const glm::vec3 n = glm::normalize(glm::vec3{-dx, -dy, 1.f});
    const float rr = relative(p);

    // Lit as the flat surface round it is lit: 1 where flat.
    float shade = za::max(0.f, glm::dot(n, chipLight)) / chipLight.z;
    // The crater's own shadow: its wall on the light's side hides the floor next to it.
    if(rr < 1.f)
    {
        const glm::vec2 dir = p / za::max(glm::length(p), 1e-4f);
        shade *= 1.f - 0.55f * smoothstep(-0.2f, 0.9f, dir.y) * (1.f - rr * rr);
    }
    // Deeper is darker (less of the light gets in), and the broken material inside darker still;
    // at the very bottom, the hole.
    float albedo = rr < 1.f ? 0.45f + 0.4f * rr * rr : 1.f;
    albedo *= smoothstep(0.12f, 0.4f, rr) * 0.9f + 0.1f;
    float f = albedo * glm::mix(1.f, shade, 0.9f);

    // The cracks: dark grooves, fading out along them.
    float crack = 0.f;
    for(const Crack& c : shape.cracks)
    {
        for(za::SizeT i = 0; i + 1 < c.points.size(); i++)
        {
            float along = 0.f;
            const float d = segmentDistance(p, c.points[i], c.points[i + 1], along);
            const float t = (static_cast<float>(i) + along) / static_cast<float>(c.points.size() - 1);
            const float w = c.width * (1.f - 0.85f * t);
            crack = za::max(crack, smoothstep(w, w * 0.3f, d) * (1.f - 0.5f * t));
        }
    }
    f *= 1.f - 0.7f * crack * smoothstep(0.7f, 1.1f, rr);

    // Dust round it (lighter), flecks of debris (darker), fading out towards the cell's edge.
    const float a = za::atan2(p.y, p.x) / 6.2831853f + 0.5f;
    const float r = glm::length(p);
    const float halo = smoothstep(0.55f + 0.25f * ring(a, 11, seed + 3), 0.25f, r) * smoothstep(0.8f, 1.3f, rr);
    f *= 1.f + 0.16f * halo * noise(p * 9.f, seed + 4);
    float fleck = 0.f;
    for(const Blob& b : shape.flecks)
    {
        fleck = za::max(fleck, smoothstep(b.r, b.r * 0.4f, glm::length(p - b.c)));
    }
    f *= 1.f - 0.4f * fleck * smoothstep(1.1f, 1.5f, rr);

    // Slightly warm where it marks the surface: dust and the soot of the hit.
    const glm::vec3 tint = glm::mix(glm::vec3{1.f}, glm::vec3{1.03f, 1.f, 0.95f}, za::min(1.f, za::fabs(f - 1.f) * 3.f));
    return glm::clamp(f * tint, 0.f, 2.f);
}

// Coverage of a cell texel at `p` (-1..1): what `kind` variant `seed` covers, and its colour (what
// the surface is multiplied by).
void cellTexel(Kind kind, int seed, const za::Vector<Blob>& blobs, glm::vec2 p, float& alpha, glm::vec3& color)
{
    const float r = glm::length(p);
    const float a = za::atan2(p.y, p.x) / 6.2831853f + 0.5f;
    switch(kind)
    {
        case Blood:
        case BloodDrop:
        {
            float cover = 0.f;
            if(kind == Blood)
            {
                const float edge = 0.38f + 0.16f * ring(a, 7, seed) + 0.06f * ring(a, 19, seed + 5);
                cover = smoothstep(edge + 0.03f, edge - 0.03f, r);
            }
            for(const Blob& b : blobs)
            {
                glm::vec2 d = p - b.c;
                const glm::vec2 axis = glm::normalize(b.stretch + glm::vec2{1e-4f});
                const float stretch = glm::length(b.stretch);
                d -= axis * glm::dot(d, axis) * (stretch / (1.f + stretch));
                cover = za::max(cover, smoothstep(b.r + 0.02f, b.r - 0.02f, glm::length(d)));
            }
            alpha = cover * (0.8f + 0.12f * noise(p * 14.f, seed) + 0.08f * noise(p * 45.f, seed + 1));
            // Darker, thicker in the middle.
            color = glm::mix(glm::vec3{0.3f, 0.015f, 0.012f}, glm::vec3{0.6f, 0.05f, 0.04f}, smoothstep(0.f, 0.7f, r));
            break;
        }
        case Scorch:
        {
            const float edge = 0.72f + 0.2f * ring(a, 9, seed) + 0.08f * (noise(p * 5.f, seed + 1) - 0.5f);
            const float soot = 0.55f * noise(p * 4.f, seed + 3) + 0.3f * noise(p * 11.f, seed + 4) + 0.15f * noise(p * 29.f, seed + 5);
            alpha = smoothstep(edge, 0.1f, r) * za::clamp(0.35f + 0.85f * soot, 0.f, 1.f) * 0.95f;
            color = glm::vec3{0.1f, 0.085f, 0.07f};
            break;
        }
        default: break; // chipTexel, goreTexel
    }
}

// ---- The gore's marks: shapes by their signed distance (cell units, < 0 inside) ------------------

struct Ellipse
{
    glm::vec2 c, r;
};

// A capsule from `a` (radius `ra`) tapering to `b` (radius `rb`).
struct Capsule
{
    glm::vec2 a, b;
    float ra, rb;
};

// A run down a wall: along +x from `x0` to `x1` about `y`, wavering, `width` across (half),
// narrowing to its end, where a drop swells.
struct Run
{
    float x0, x1, y, width, phase;
};

struct GoreShape
{
    za::Vector<Ellipse> ellipses;
    za::Vector<Capsule> capsules;
    za::Vector<Run> runs;
    float edge = 0.f; // a ragged blot round the middle this big (0 none): splotches and pools
    int lobes = 9;
};

[[nodiscard]] float ellipseDistance(const Ellipse& e, glm::vec2 p)
{
    const glm::vec2 q = (p - e.c) / e.r;
    return (glm::length(q) - 1.f) * za::min(e.r.x, e.r.y);
}

[[nodiscard]] float goreDistance(const GoreShape& s, int seed, glm::vec2 p)
{
    float d = 10.f;
    if(s.edge > 0.f)
    {
        const float a = za::atan2(p.y, p.x) / 6.2831853f + 0.5f;
        const float edge = s.edge * (1.f + 0.3f * (ring(a, s.lobes, seed) - 0.5f) + 0.14f * (ring(a, 23, seed + 1) - 0.5f) +
                                        0.05f * (ring(a, 61, seed + 2) - 0.5f));
        d = glm::length(p) - edge;
    }
    for(const Ellipse& e : s.ellipses)
    {
        if(za::fabs(p.x - e.c.x) < e.r.x + 0.05f && za::fabs(p.y - e.c.y) < e.r.y + 0.05f)
        {
            d = za::min(d, ellipseDistance(e, p));
        }
    }
    for(const Capsule& c : s.capsules)
    {
        float t = 0.f;
        const float dist = segmentDistance(p, c.a, c.b, t);
        d = za::min(d, dist - glm::mix(c.ra, c.rb, t));
    }
    for(const Run& r : s.runs)
    {
        if(p.x < r.x0 - 0.1f || p.x > r.x1 + 0.1f)
        {
            continue;
        }
        const float along = za::clamp((p.x - r.x0) / za::max(r.x1 - r.x0, 1e-3f), 0.f, 1.f);
        const float y = r.y + 0.025f * za::sin(p.x * 7.f + r.phase) + 0.012f * za::sin(p.x * 19.f + r.phase * 2.f);
        const float w = r.width * (1.f - 0.4f * along);
        const float inside = za::max(r.x0 - p.x, p.x - r.x1); // < 0 between its ends
        d = za::min(d, za::max(za::fabs(p.y - y) - w, inside));
        // The drop at its end (the quad is stretched along x: short in x).
        d = za::min(d, ellipseDistance({{r.x1, y}, {w * 0.7f, w * 1.6f}}, p));
    }
    return d;
}

// A gore cell's shape, variant `seed` (frand seeded by it).
[[nodiscard]] GoreShape goreShape(Kind kind)
{
    GoreShape s;
    switch(kind)
    {
        case Splatter:
        {
            // A blot where it struck (towards -x), a few lumps round it, then droplets flung along
            // +x in a fan, smaller and more drawn out the further they flew, some with a tail.
            const glm::vec2 core{-0.45f + 0.06f * frand(), (frand() - 0.5f) * 0.08f};
            s.ellipses.pushBack({core, {0.3f + 0.06f * frand(), 0.24f + 0.06f * frand()}});
            for(int i = 0; i < 5; i++)
            {
                const float a = frand() * 6.2831853f;
                s.ellipses.pushBack({core + glm::vec2{za::cos(a) * 0.24f + 0.1f, za::sin(a) * 0.2f},
                    glm::vec2{0.09f + 0.07f * frand(), 0.08f + 0.06f * frand()}});
            }
            for(int i = 0; i < 46; i++)
            {
                const float t = za::pow(frand(), 0.8f);
                const float ang = (frand() + frand() + frand() - 1.5f) * 0.55f;
                const glm::vec2 dir{za::cos(ang), za::sin(ang)};
                const glm::vec2 at = core + dir * (0.2f + t * 1.1f);
                if(za::fabs(at.x) > 0.9f || za::fabs(at.y) > 0.9f)
                {
                    continue;
                }
                const float r = (0.07f * (1.f - t) + 0.014f) * (0.6f + 0.8f * frand());
                const float stretch = 1.f + 2.5f * t * frand();
                if(t > 0.2f && frand() < 0.55f)
                {
                    s.capsules.pushBack({at - dir * (0.05f + 0.22f * t), at, r * 0.3f, r});
                }
                // Drawn out along its flight (a rotated ellipse: a capsule of its length).
                s.capsules.pushBack({at - dir * r * (stretch - 1.f), at, r * 0.85f, r});
            }
            break;
        }
        case Streak:
        {
            // The quad is about three times as long (x) as it is wide (y): the blot at the top is
            // short in x, the runs' widths are across.
            s.ellipses.pushBack({{-0.8f, (frand() - 0.5f) * 0.1f}, {0.14f, 0.34f + 0.1f * frand()}});
            s.ellipses.pushBack({{-0.72f, (frand() - 0.5f) * 0.3f}, {0.08f, 0.16f}});
            const int runs = 2 + static_cast<int>(frand() * 3.f);
            for(int i = 0; i < runs; i++)
            {
                const float y = (static_cast<float>(i) + 0.5f) / runs * 0.56f - 0.28f + (frand() - 0.5f) * 0.08f;
                s.runs.pushBack({-0.78f, -0.35f + frand() * 1.15f, y, 0.025f + 0.045f * frand(), frand() * 6.28f});
            }
            break;
        }
        case Pool:
        {
            s.edge = 0.6f;
            s.lobes = 5 + static_cast<int>(frand() * 3.f);
            for(int i = 0; i < 4; i++)
            {
                const float a = frand() * 6.2831853f;
                const glm::vec2 dir{za::cos(a), za::sin(a)};
                s.ellipses.pushBack({dir * (0.5f + 0.15f * frand()), glm::vec2{0.14f + 0.1f * frand()}});
            }
            for(int i = 0; i < 10; i++)
            {
                const float a = frand() * 6.2831853f;
                s.ellipses.pushBack(
                    {glm::vec2{za::cos(a), za::sin(a)} * (0.78f + 0.12f * frand()), glm::vec2{0.015f + 0.025f * frand()}});
            }
            break;
        }
        case Splotch:
        {
            // A ragged blot, spikes thrown out from it ending in drops, drops round it.
            s.edge = 0.3f;
            s.lobes = 7 + static_cast<int>(frand() * 5.f);
            const int spikes = 8 + static_cast<int>(frand() * 6.f);
            for(int i = 0; i < spikes; i++)
            {
                const float a = (static_cast<float>(i) + frand() * 0.8f) / spikes * 6.2831853f;
                const glm::vec2 dir{za::cos(a), za::sin(a)};
                const float len = 0.5f + 0.4f * frand();
                const float w = 0.045f + 0.04f * frand();
                s.capsules.pushBack({dir * 0.2f, dir * len, w, w * 0.25f});
                s.ellipses.pushBack({dir * (len + 0.03f), glm::vec2{w * (0.5f + 0.4f * frand())}});
            }
            for(int i = 0; i < 26; i++)
            {
                const float a = frand() * 6.2831853f;
                s.ellipses.pushBack(
                    {glm::vec2{za::cos(a), za::sin(a)} * (0.4f + 0.5f * frand()), glm::vec2{0.01f + 0.03f * frand()}});
            }
            break;
        }
        default: break;
    }
    return s;
}

// What the surface is multiplied by at `p` (-1..1) for a gore cell: blood, darker where it is thick
// (deep inside the shape), a pool darker still.
[[nodiscard]] glm::vec3 goreTexel(Kind kind, int seed, const GoreShape& s, glm::vec2 p)
{
    const float d = goreDistance(s, seed, p);
    if(d > 0.02f)
    {
        return glm::vec3{1.f};
    }
    const float cover = smoothstep(0.012f, -0.012f, d);
    const float thick = smoothstep(0.f, kind == Pool ? -0.25f : -0.07f, d);
    const float alpha = cover * (0.84f + 0.1f * noise(p * 14.f, seed) + 0.06f * noise(p * 45.f, seed + 1));
    const glm::vec3 thin = kind == Pool ? glm::vec3{0.72f, 0.04f, 0.03f} : glm::vec3{0.8f, 0.05f, 0.04f};
    const glm::vec3 deep = kind == Pool ? glm::vec3{0.38f, 0.015f, 0.01f} : glm::vec3{0.42f, 0.02f, 0.015f};
    const glm::vec3 color = glm::mix(thin, deep, thick * (0.75f + 0.25f * noise(p * 6.f, seed + 2)));
    return glm::mix(glm::vec3{1.f}, color, za::clamp(alpha, 0.f, 1.f));
}

// A texel of what the surface is multiplied by, `f` (0..2 in each channel; 1 leaves it as it is),
// for the modulating blend (dst * (colour + 1 - alpha)), premultiplied: dimming by alpha, brightening
// by the colour. Linear in `f`, so that filtering and the fading vertex colour stay right.
void encode(glm::vec3 f, unsigned char* out)
{
    f = glm::clamp(f, 0.f, 2.f);
    const float dim = za::max(0.f, 1.f - qza::minOf(f.r, f.g, f.b));
    const glm::vec3 c = f - 1.f + dim;
    for(int i = 0; i < 3; i++)
    {
        out[i] = static_cast<unsigned char>(za::clamp(c[i], 0.f, 1.f) * 255.f + 0.5f);
    }
    out[3] = static_cast<unsigned char>(za::clamp(dim, 0.f, 1.f) * 255.f + 0.5f);
}

[[nodiscard]] za::Vector<unsigned char> buildAtlas()
{
    za::Vector<unsigned char> rgba(atlasWidth * atlasHeight * 4, 0);
    // The cells shared out among the game's threads (vr_jobs.hpp): each writes its own texels, its random numbers seeded
    // by its number (the thread's own generator): the same atlas whatever ran it.
    const int cells = firstCell[KindCount - 1] + cellCount[KindCount - 1];
    jobs::parallelFor(static_cast<za::SizeT>(cells), 1, [&](za::SizeT c0, za::SizeT c1) {
        for(int cell = static_cast<int>(c0); cell < static_cast<int>(c1); cell++)
        {
            int kind = KindCount - 1;
            while(cell < firstCell[kind])
            {
                kind--;
            }
            const int seed = cell * 131 + 7;
            rngState = static_cast<unsigned>(seed);

            // A chip's cracks (a few, jagged, out from the crater's edge) and flecks.
            ChipShape chip;
            za::Vector<Crack>& cracks = chip.cracks;
            if(kind == Hole)
            {
                for(int i = 0; i < 70; i++)
                {
                    const float ang = frand() * 6.2831853f;
                    const float dist = 0.28f + 0.5f * frand() * frand();
                    chip.flecks.pushBack({glm::vec2{za::cos(ang), za::sin(ang)} * dist, 0.006f + frand() * 0.012f, glm::vec2{0.f}});
                }
                const int count = 3 + static_cast<int>(frand() * 4.f);
                for(int i = 0; i < count; i++)
                {
                    float ang = (static_cast<float>(i) + 0.2f + frand() * 0.6f) / count * 6.2831853f;
                    glm::vec2 at = glm::vec2{za::cos(ang), za::sin(ang)} * 0.18f;
                    Crack c{{at}, 0.012f + frand() * 0.012f};
                    const float length = 0.25f + frand() * 0.45f;
                    const int steps = 4 + static_cast<int>(frand() * 3.f);
                    for(int s = 0; s < steps; s++)
                    {
                        ang += (frand() - 0.5f) * 0.9f;
                        at += glm::vec2{za::cos(ang), za::sin(ang)} * (length / steps);
                        c.points.pushBack(at);
                    }
                    cracks.pushBack(ZA_MOVE(c));
                    // Now and then a branch off its middle.
                    if(frand() < 0.4f)
                    {
                        const glm::vec2 from = cracks.back().points[cracks.back().points.size() / 2];
                        const float b = ang + (frand() < 0.5f ? -0.8f : 0.8f);
                        cracks.pushBack({{from, from + glm::vec2{za::cos(b), za::sin(b)} * (0.08f + frand() * 0.12f)},
                            cracks.back().width * 0.6f});
                    }
                }
            }

            // Blood's satellite droplets and streaks (a drop cell: a few drops only).
            za::Vector<Blob> blobs;
            const int count = kind == Blood ? 9 + static_cast<int>(frand() * 6) : kind == BloodDrop ? 4 : 0;
            for(int i = 0; i < count; i++)
            {
                const float ang = frand() * 6.2831853f;
                const float dist = kind == Blood ? 0.45f + frand() * 0.45f : frand() * 0.55f;
                const glm::vec2 dir{za::cos(ang), za::sin(ang)};
                const float streak = kind == Blood && frand() < 0.4f ? 1.f + frand() * 3.f : 0.f;
                blobs.pushBack({dir * dist, (kind == Blood ? 0.03f : 0.08f) + frand() * (kind == Blood ? 0.06f : 0.12f),
                    dir * streak});
            }

            // The gore's shapes.
            const GoreShape gore = kind >= Splatter ? goreShape(static_cast<Kind>(kind)) : GoreShape{};

            const int ox = (cell % cellsPerRow) * cellSize, oy = (cell / cellsPerRow) * cellSize;
            for(int y = 0; y < cellSize; y++)
            {
                for(int x = 0; x < cellSize; x++)
                {
                    const glm::vec2 p{(x + 0.5f) / cellSize * 2.f - 1.f, (y + 0.5f) / cellSize * 2.f - 1.f};
                    glm::vec3 f{1.f};
                    if(kind == Hole)
                    {
                        f = chipTexel(seed, chip, p);
                    }
                    else if(kind >= Splatter)
                    {
                        f = goreTexel(static_cast<Kind>(kind), seed, gore, p);
                    }
                    else
                    {
                        float alpha = 0.f;
                        glm::vec3 color{1.f};
                        cellTexel(static_cast<Kind>(kind), seed, blobs, p, alpha, color);
                        f = glm::mix(glm::vec3{1.f}, color, za::clamp(alpha, 0.f, 1.f));
                    }
                    // Faded out to nothing at the cell's border: filtering (and the smaller
                    // mipmaps) must not reach the next cell.
                    const int edge = qza::minOf(x, y, cellSize - 1 - x, cellSize - 1 - y);
                    f = glm::mix(glm::vec3{1.f}, f, smoothstep(3.f, 14.f, static_cast<float>(edge)));
                    encode(f, &rgba[((oy + y) * atlasWidth + ox + x) * 4]);
                }
            }
        }
    });
    return rgba;
}

// The atlas's texels, made from start-up on the game's threads (decals::init): buildAtlas reads nothing but its arguments
// and constants, and the random numbers are the thread's own.
jobs::Future<za::Vector<unsigned char>> atlasTexels;

void makeAtlas()
{
    const double t0 = Sys_DoubleTime();
    const bool ahead = atlasTexels.valid();
    const za::Vector<unsigned char> rgba = ahead ? atlasTexels.get() : buildAtlas();
    if(developer.value)
    {
        za::U32 hash = 2166136261u; // (FNV-1a: the same atlas whatever made it)
        for(const unsigned char t : rgba)
        {
            hash = (hash ^ t) * 16777619u;
        }
        Con_DPrintf("VR decals: the atlas %s (%.1f ms waited; texels %08x)\n", ahead ? "made ahead" : "made now",
            (Sys_DoubleTime() - t0) * 1e3, hash);
    }
    atlas = gfx::createTexture(atlasWidth, atlasHeight, rgba.data(), true);
}

// ---- Placing ----------------------------------------------------------------------------------

// The static world along `from` -> `to`: where, and its normal.
[[nodiscard]] bool hitWorld(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    const trace_t tr = worldtrace::world(from, to, false);
    if(tr.fraction >= 1.f || tr.startsolid || tr.allsolid)
    {
        return false;
    }
    where = worldtrace::endPos(tr);
    normal = worldtrace::normal(tr);
    fraction = tr.fraction;
    return true;
}

// The nearest surface within `reach` of `org` (along `prefer` first, then down, then round).
[[nodiscard]] bool nearest(const glm::vec3& org, float reach, const glm::vec3& prefer, glm::vec3& where, glm::vec3& normal)
{
    const glm::vec3 dirs[] = {prefer, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}};
    float best = 2.f;
    for(const glm::vec3& d : dirs)
    {
        if(glm::dot(d, d) < 0.5f)
        {
            continue;
        }
        glm::vec3 w, n;
        float f;
        if(hitWorld(org, org + glm::normalize(d) * reach, w, n, f) && f < best)
        {
            best = f;
            where = w;
            normal = n;
        }
    }
    return best <= 1.f;
}

// Keeps the part of `poly` on the inner side of the plane (dot(p, normal) <= dist) into `out`.
void clipPolygon(const za::Vector<glm::vec3>& poly, const glm::vec3& normal, float dist, za::Vector<glm::vec3>& out)
{
    out.clear();
    const za::SizeT n = poly.size();
    for(za::SizeT i = 0; i < n; i++)
    {
        const glm::vec3& a = poly[i];
        const glm::vec3& b = poly[(i + 1) % n];
        const float da = glm::dot(a, normal) - dist, db = glm::dot(b, normal) - dist;
        if(da <= 0.f)
        {
            out.pushBack(a);
        }
        if((da < 0.f && db > 0.f) || (da > 0.f && db < 0.f))
        {
            out.pushBack(a + (b - a) * (da / (da - db)));
        }
    }
}

// Lays decal `d` (on a surface facing `n`) on the world: its footprint (the box of its centre, u
// and v, `depth` deep either side of the surface) cut out of each face of the world in it that faces
// about its way, as triangles (into `tris`, emptied first), lifted off the face a little. Faces are found down the BSP
// tree (those on the nodes whose planes cross the box); sky, liquids and fences are left out.
void clipToWorld(const Decal& d, const glm::vec3& n, float depth, za::Vector<Corner>& tris)
{
    tris.clear();
    const qmodel_t* m = cl.worldmodel;
    const float hu = glm::length(d.u), hv = glm::length(d.v);
    if(!m || !m->nodes || hu <= 0.f || hv <= 0.f)
    {
        return;
    }
    const glm::vec3 U = d.u / hu, V = d.v / hv;
    const glm::vec3 extent = glm::abs(d.u) + glm::abs(d.v) + glm::abs(n) * depth;
    const glm::vec3 lo = d.centre - extent, hi = d.centre + extent;
    const float sideDist[4] = {glm::dot(d.centre, U) + hu, glm::dot(d.centre, -U) + hu, glm::dot(d.centre, V) + hv,
        glm::dot(d.centre, -V) + hv};
    const glm::vec3 sideNormal[4] = {U, -U, V, -V};

    za::Vector<glm::vec3>& poly = scratch.poly;
    za::Vector<glm::vec3>& clipped = scratch.clipped;
    constexpr za::SizeT maxCorners = 32 * 3; // over very broken ground, the rest is left out
    const mnode_t* stack[256];
    int top = 0;
    stack[top++] = m->nodes + m->hulls[0].firstclipnode;
    while(top > 0 && tris.size() < maxCorners)
    {
        const mnode_t* node = stack[--top];
        if(node->contents < 0)
        {
            continue; // a leaf
        }
        const mplane_t* plane = node->plane;
        const glm::vec3 pn{plane->normal[0], plane->normal[1], plane->normal[2]};
        const float side = glm::dot(d.centre, pn) - plane->dist;
        const float reach = glm::dot(extent, glm::abs(pn));
        if(side > reach)
        {
            stack[top++] = node->children[0];
            continue;
        }
        if(side < -reach)
        {
            stack[top++] = node->children[1];
            continue;
        }
        if(top + 2 <= 256)
        {
            stack[top++] = node->children[0];
            stack[top++] = node->children[1];
        }

        // The faces on this node's plane.
        for(unsigned int i = 0; i < node->numsurfaces && tris.size() < maxCorners; i++)
        {
            const msurface_t* s = m->surfaces + node->firstsurface + i;
            if(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_DRAWFENCE | SURF_NOTEXTURE))
            {
                continue;
            }
            if(s->maxs[0] < lo.x || s->mins[0] > hi.x || s->maxs[1] < lo.y || s->mins[1] > hi.y || s->maxs[2] < lo.z ||
                s->mins[2] > hi.z)
            {
                continue;
            }
            const float flip = (s->flags & SURF_PLANEBACK) ? -1.f : 1.f;
            const glm::vec3 sn = glm::vec3{s->plane->normal[0], s->plane->normal[1], s->plane->normal[2]} * flip;
            const float off = glm::dot(d.centre, sn) - s->plane->dist * flip; // the centre in front of it (> 0)
            if(glm::dot(sn, n) < 0.5f || za::fabs(off) > depth)
            {
                continue;
            }
            poly.clear();
            for(int e = 0; e < s->numedges; e++)
            {
                const int se = m->surfedges[s->firstedge + e];
                const unsigned int vi = se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1];
                const float* p = m->vertexes[vi].position;
                poly.pushBack({p[0], p[1], p[2]});
            }
            for(int k = 0; k < 4 && poly.size() >= 3; k++)
            {
                clipPolygon(poly, sideNormal[k], sideDist[k], clipped);
                poly.swap(clipped);
            }
            if(poly.size() < 3)
            {
                continue;
            }
            // A fan of triangles, lifted off the face (not fighting it for depth), its cell
            // coordinates from where each corner is in the footprint.
            const auto corner = [&](const glm::vec3& p) {
                const glm::vec3 r = p - d.centre;
                return Corner{p + sn * 0.2f, {glm::dot(r, U) / hu * 0.5f + 0.5f, glm::dot(r, V) / hv * 0.5f + 0.5f}};
            };
            for(za::SizeT k = 1; k + 1 < poly.size() && tris.size() < maxCorners; k++)
            {
                tris.pushBack(corner(poly[0]));
                tris.pushBack(corner(poly[k]));
                tris.pushBack(corner(poly[k + 1]));
            }
        }
    }
}

// The direction along a surface at `where` (normal `n`) that the light lighting it comes from: the
// strongest of the map's lights in front of it that sees it (Quake's linear falloff, weighted by
// how squarely it faces it), else above (a wall; zero on a floor or a ceiling).
[[nodiscard]] glm::vec3 lightAlong(const glm::vec3& where, const glm::vec3& n)
{
    struct Candidate
    {
        float weight;
        glm::vec3 pos;
    };
    Candidate best[4]{};
    for(const modellight::MapLight& l : modellight::mapLights())
    {
        const glm::vec3 d = l.pos - where;
        const float dist = glm::length(d);
        const float facing = dist > 1.f ? glm::dot(d, n) / dist : 0.f;
        const float weight = (l.value - dist * za::max(l.scale, 0.01f)) * facing;
        if(facing <= 0.05f || weight <= best[3].weight)
        {
            continue;
        }
        int i = 3;
        for(; i > 0 && best[i - 1].weight < weight; i--)
        {
            best[i] = best[i - 1];
        }
        best[i] = {weight, l.pos};
    }
    for(const Candidate& c : best)
    {
        if(c.weight <= 0.f)
        {
            break;
        }
        glm::vec3 w, hn;
        float f;
        if(hitWorld(where + n * 1.f, c.pos, w, hn, f))
        {
            continue; // it does not see it
        }
        const glm::vec3 d = c.pos - where;
        const glm::vec3 along = d - n * glm::dot(d, n);
        if(glm::length(along) > 0.05f * glm::length(d))
        {
            return glm::normalize(along);
        }
    }
    const glm::vec3 up = glm::vec3{0.f, 0.f, 1.f} - n * n.z;
    return glm::length(up) > 0.3f ? glm::normalize(up) : glm::vec3{0.f};
}

bool add(Kind kind, const glm::vec3& where, const glm::vec3& normal, float size, const MarkOptions& o = {})
{
    const int max = static_cast<int>(vr_decal_max.value);
    if(!vr_decals.value || max <= 0 || !cl.worldmodel)
    {
        return false;
    }
    if(addedFrame != host_framecount)
    {
        addedFrame = host_framecount;
        addedThisFrame = 0;
    }
    if(++addedThisFrame > 64)
    {
        return false; // a shotgun blast into a wall is enough at once (the gore spreads its own over frames)
    }

    // Not the same mark twice in one place (Quake's effect and Quake VR's for one hit).
    for(za::SizeT k = 0; k < decals.size() && k < 16; k++)
    {
        const Decal& e = decals[decals.size() - 1 - k];
        if(cl.time - e.born < 0.1 && glm::distance(e.centre, where) < 2.f && o.delay <= 0.f)
        {
            return false;
        }
    }

    const glm::vec3 n = glm::normalize(normal);
    const glm::vec3 t0 = glm::normalize(glm::cross(n, za::fabs(n.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0}));
    const glm::vec3 t1 = glm::cross(n, t0);
    const float ang = static_cast<float>(rand() % 6283) * 0.001f;
    glm::vec3 u = (t0 * za::cos(ang) + t1 * za::sin(ang));
    // Turned along `along` on the surface, if given (a spray's way, a run's down).
    if(const glm::vec3 a = o.along - n * glm::dot(o.along, n); glm::length(a) > 0.1f * glm::length(o.along) && glm::length(a) > 1e-4f)
    {
        u = glm::normalize(a);
    }
    glm::vec3 v = glm::cross(n, u);
    if(kind == Hole)
    {
        // Its relief lit from where the light comes from (v: the cell's lit side); mirrored at
        // random for variety (the light stays).
        if(const glm::vec3 lit = lightAlong(where, n); glm::dot(lit, lit) > 0.f)
        {
            v = lit;
            u = glm::cross(v, n) * (rand() & 1 ? 1.f : -1.f);
        }
    }
    // Laid on the world's faces under it, cut where they end (an edge, a corner): over a wall's
    // panels and recesses, the floor of an alcove, the steps of a stair. A run down a wall keeps
    // its top where it is.
    const bool big = kind >= Splatter;
    const float halfV = size * 0.5f, halfU = halfV * za::max(0.25f, o.aspect);
    Decal d{o.fromStart ? where + u * halfU : where, u * halfU, v * halfV, firstCell[kind] + rand() % cellCount[kind],
        cl.time + o.delay};
    d.grow = o.grow;
    d.growFrom = za::clamp(o.growFrom, 0.f, 1.f);
    d.fromStart = o.fromStart;
    d.darken = za::clamp(o.darken, 0.f, 0.9f);
    za::Vector<Corner>& tris = scratch.tris;
    clipToWorld(d, n, big ? za::clamp(halfV * 0.35f, 4.f, 12.f) : 3.f, tris);
    if(tris.empty())
    {
        return false;
    }
    // Drops (a steady drip from wounds, gibs and ceilings) take at most a quarter of the marks: the
    // oldest drop goes, not a splat or a pool (the others keep their order).
    if(kind == BloodDrop && dropCount >= za::max(16, max / 4))
    {
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            if(isDrop(decals[k]))
            {
                decals.eraseAt(k);
                dropCount--;
                break;
            }
        }
    }
    // Room for it: the oldest go (as many as vr_decal_max, lowered, leaves no room for); the ring made that size if
    // it changed.
    while(decals.size() >= static_cast<za::SizeT>(max))
    {
        popOldest();
    }
    if(decals.capacity() != static_cast<za::SizeT>(max))
    {
        decals.setCapacity(static_cast<za::SizeT>(max));
    }
    // Into the ring's next slot, its triangles into the buffer the slot's last mark had.
    Decal& slot = decals.pushBack();
    za::Vector<Corner> buffer = ZA_MOVE(slot.tris);
    slot = d;
    slot.tris = ZA_MOVE(buffer);
    slot.tris.assignRange(tris.begin(), tris.end());
    dropCount += isDrop(slot);
    builtFrame = -1;
    return true;
}

[[nodiscard]] float random(float lo, float hi)
{
    return lo + (hi - lo) * static_cast<float>(rand() % 1000) * 0.001f;
}

// ---- Gibs' blood (VR_GibTrail) ----------------------------------------------------------------

// What a gib (or head) has done so far, by entity number.
struct Gib
{
    const qmodel_t* model = nullptr;
    double seen = 0.0;       // cl.time it was last drawn,
    glm::vec3 origin{0.f};   // and where
    double msgTime = 0.0;    // the server message `velocity` is from
    glm::vec3 velocity{0.f}; // between its last two positions from the server
    bool moving = false;     // `velocity` is known
    float sinceTrail = 0.f;  // units travelled since the last trail particles,
    float sinceDrop = 0.f;   // and since the last drop on the floor
    int dropsLeft = 0;       // a gib bleeds dry
    double splatTime = 0.0;  // its last splat
    double stillSince = 0.0; // cl.time it stopped moving (0: moving)
    bool restChecked = false; // looked under it since it stopped,
    bool hanging = false;     // nothing under it (stuck to a ceiling or a wall, or held): it drips
    int pools = 0;            // pools left under it at rest (vr_gore)
};

ankerl::unordered_dense::map<int, Gib> gibs;
int gibsPrunedFrame = 0;

// Hipnotic's bullet holes turned into chips (VR_BulletHoleSprite), by entity number: where they were.
ankerl::unordered_dense::map<int, glm::vec3> holes;
int holesPrunedFrame = 0;

constexpr int gibDrops = 64;           // drops a gib leaves in all
constexpr float gibTrailSpacing = 5.f; // units between trail particles (vr_gib_blood_trail 1)
constexpr float gibDropSpacing = 20.f; // and between drops
constexpr float gibSplatSpeed = 150.f; // units/s it must go at, and its velocity turn, to splat

// Calls `f` every `spacing` units along `from` -> `to` (at most `max` times), `since` carrying the
// distance travelled since the last one over from call to call.
template <typename F>
void along(const glm::vec3& from, const glm::vec3& to, float spacing, float& since, int max, F&& f)
{
    const glm::vec3 d = to - from;
    const float len = glm::length(d);
    float at = za::max(0.f, spacing - since); // how far along the next one is
    int n = 0;
    while(at <= len && n < max)
    {
        f(from + d * (len > 0.f ? at / len : 0.f));
        at += spacing;
        n++;
    }
    since = n == max ? 0.f : len - (at - spacing);
}

// A drop on the floor under a gib at `org` (its blood falls straight down).
void drip(const glm::vec3& org)
{
    const glm::vec3 o = org + glm::vec3{random(-2.f, 2.f), random(-2.f, 2.f), 0.f};
    glm::vec3 where, normal;
    float f;
    if(hitWorld(o, o - glm::vec3{0, 0, 128}, where, normal, f) && normal.z > 0.6f)
    {
        // Low over the floor (sliding, rolling, carried low) a smear; from higher, smaller drops. As
        // big as Quake's gibs call for (a gib is 10-30 units across).
        add(BloodDrop, where, normal, f * 128.f < 16.f ? random(12.f, 20.f) : random(8.f, 15.f));
    }
}

// A gib struck the world between `from` and `to` (its last two positions from the server), going
// at `oldVelocity`, turned by `change` (beyond gravity's): a splat where it hit, a spurt of blood.
void splat(const glm::vec3& from, const glm::vec3& to, const glm::vec3& oldVelocity, const glm::vec3& change)
{
    const float strength = glm::length(change);
    const float reach = glm::distance(from, to) + 16.f; // a gib's box is about this big
    glm::vec3 where, normal;
    float f;
    // The surface pushed it back along its normal, so it lies the other way (else ahead of it).
    if(!hitWorld(from, from - change / strength * reach, where, normal, f) &&
       !hitWorld(from, from + glm::normalize(oldVelocity) * reach, where, normal, f))
    {
        return;
    }
    // With the gore (vr_gore), a big splat, runs down a wall, drips from a ceiling.
    if(!gore::gibImpact(where, normal, strength, oldVelocity))
    {
        add(Blood, where, normal, za::clamp(16.f + strength * 0.03f, 16.f, 40.f));
    }

    // Quake VR's blood (Quake's own with vr_particles 0).
    vec3_t org, dir;
    for(int i = 0; i < 3; i++)
    {
        org[i] = where[i] + normal[i] * 2.f;
        dir[i] = normal[i] * 2.f;
    }
    R_RunParticleEffect(org, dir, 73, strength > 500.f ? 24 : 12);
}

} // namespace

void drop(const glm::vec3& org, float size)
{
    glm::vec3 where, normal;
    float f;
    if(vr_decals.value && cl.worldmodel &&
        hitWorld(org + glm::vec3{0, 0, 2}, org - glm::vec3{0, 0, 16}, where, normal, f) && normal.z > 0.6f)
    {
        add(BloodDrop, where, normal, size);
    }
}

bool place(Mark mark, const glm::vec3& where, const glm::vec3& normal, float size, const MarkOptions& o)
{
    constexpr Kind kinds[] = {Blood, BloodDrop, Splatter, Streak, Pool, Splotch};
    return vr_decals.value && cl.worldmodel && add(kinds[static_cast<int>(mark)], where, normal, size, o);
}

bool trace(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    return hitWorld(from, to, where, normal, fraction);
}

void chip(const glm::vec3& org)
{
    glm::vec3 where, normal;
    if(vr_decals.value && cl.worldmodel && nearest(org, 8.f, glm::vec3{0.f}, where, normal))
    {
        // Onto the surface straight under it (`nearest` looks along the axes only).
        glm::vec3 w, n;
        float f;
        if(hitWorld(org + normal * 2.f, org - normal * 8.f, w, n, f))
        {
            where = w;
            normal = n;
        }
        add(Hole, where, normal, random(5.5f, 7.5f));
    }
}

void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count)
{
    using particles::Preset;
    if(!vr_decals.value || !cl.worldmodel)
    {
        return;
    }
    glm::vec3 where, normal;
    float f;
    const glm::vec3 back = glm::dot(dir, dir) > 1e-4f ? -glm::normalize(dir) : glm::vec3{0.f};
    switch(preset)
    {
        case Preset::Blood: // a pool on the floor below, spatter on a wall near by
        {
            const float size = za::clamp(8.f + count * 0.4f, 8.f, 28.f);
            if(hitWorld(org, org - glm::vec3{0, 0, 160}, where, normal, f) && normal.z > 0.6f)
            {
                add(Blood, where, normal, size * (0.8f + f * 0.6f)); // the farther it falls, the wider it spreads
            }
            // (With the gore, its sprays mark the walls: vr_gore.cpp.)
            const float a = random(0.f, 6.2831853f);
            const glm::vec3 side = glm::dot(dir, dir) > 1e-4f ? glm::normalize(dir) : glm::vec3{za::cos(a), za::sin(a), random(-0.3f, 0.2f)};
            if(gore::level() == 0 && hitWorld(org, org + glm::normalize(side) * 72.f, where, normal, f))
            {
                add(Blood, where, normal, size * 0.7f);
            }
            break;
        }
        case Preset::BulletPuff: // a chip where it hit
        case Preset::Sparks:     // a melee blow on a wall
            if(nearest(org, 6.f, back, where, normal))
            {
                add(Hole, where, normal, preset == Preset::BulletPuff ? random(5.5f, 7.5f) : random(6.f, 8.f));
            }
            break;
        case Preset::Explosion:
            if(nearest(org, 64.f, glm::vec3{0, 0, -1}, where, normal))
            {
                add(Scorch, where, normal, random(44.f, 60.f));
            }
            break;
        case Preset::Lightning:
        case Preset::LavaSpike:
            if(nearest(org, 10.f, back, where, normal))
            {
                add(Scorch, where, normal, random(8.f, 14.f));
            }
            break;
        default: break;
    }
}

// A decal's triangles as they are now (spreading, darkening, fading) into `out`.
void appendDecal(const Decal& d, double life, za::Vector<gfx::Vertex>& out)
{
    const float age = static_cast<float>(cl.time - d.born);
    if(age < 0.f)
    {
        return; // not there yet (blood flying to it)
    }
    constexpr float insetU = 0.5f / atlasWidth, insetV = 0.5f / atlasHeight;
    const float a = za::clamp(static_cast<float>((life - age) / 5.0), 0.f, 1.f);
    // Spreading (a pool, a run down a wall; eased out: fast at first), and darkening as it does
    // (and dries).
    float s = 1.f;
    if(d.grow > 0.f)
    {
        const float t = 1.f - za::min(1.f, age / d.grow);
        s = d.growFrom + (1.f - d.growFrom) * (1.f - t * t);
    }
    const float dark = 1.f - d.darken * za::min(1.f, age / za::max(d.grow, 8.f));
    const glm::vec4 c{a * dark, a * dark, a * dark, a};
    const float u0 = static_cast<float>(d.cell % cellsPerRow) / cellsPerRow + insetU;
    const float v0 = static_cast<float>(d.cell / cellsPerRow) / cellRows + insetV;
    const glm::vec2 cellSpan{1.f / cellsPerRow - 2.f * insetU, 1.f / cellRows - 2.f * insetV};
    // Spreading: its triangles drawn in towards where it spreads from, along the surface (its
    // middle; a run's top, along u only).
    const glm::vec3 U = glm::normalize(d.u), V = glm::normalize(d.v);
    const glm::vec3 anchor = d.fromStart ? d.centre - d.u : d.centre;
    const float su = s, sv = d.fromStart ? 1.f : s;
    for(const Corner& k : d.tris)
    {
        glm::vec3 p = k.pos;
        if(s < 1.f)
        {
            const glm::vec3 r = p - anchor;
            const float ru = glm::dot(r, U), rv = glm::dot(r, V);
            p = anchor + r - U * (ru * (1.f - su)) - V * (rv * (1.f - sv));
        }
        out.pushBack({p, {u0 + k.uv.x * cellSpan.x, v0 + k.uv.y * cellSpan.y}, c});
    }
}

void draw()
{
    QVR_GPU_PROFILE("decals");
    if(!vr_decals.value)
    {
        return;
    }
    // The gore's work for the frame first (its marks, drips and pools: vr_gore.cpp), once.
    if(goreFrame != host_framecount)
    {
        goreFrame = host_framecount;
        gore::frame();
    }
    if(decals.empty())
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }

    // Built once a frame for both eyes: the marks that change (spreading, darkening, showing,
    // fading) each frame, the others only when marks come or go or one of them starts or stops
    // changing.
    if(builtFrame != host_framecount)
    {
        QVR_PROFILE("decal verts");
        const bool setChanged = builtFrame == -1;
        builtFrame = host_framecount;

        // Faded out over their last five seconds.
        const double life = za::max(5.f, vr_decal_life.value);
        bool popped = false;
        while(!decals.empty() && cl.time - decals.front().born > life)
        {
            popOldest();
            popped = true;
        }

        const bool restatic = setChanged || popped || cl.time >= staticUntil || life != staticLife;
        if(restatic)
        {
            staticVertices.clear();
            staticUntil = 1e30;
            staticLife = life;
            staticDirty = true;
        }
        vertices.clear();
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            const Decal& d = decals[k];
            // When it last changes: shown, spread, darkened; then its fading.
            const double settled = d.born + qza::maxOf(0.f, d.grow, d.darken > 0.f ? za::max(d.grow, 8.f) : 0.f);
            const double fades = d.born + life - 5.0;
            if(cl.time >= settled && cl.time < fades)
            {
                if(restatic)
                {
                    appendDecal(d, life, staticVertices);
                    staticUntil = za::min(staticUntil, fades);
                }
            }
            else
            {
                appendDecal(d, life, vertices);
                if(cl.time < settled)
                {
                    staticUntil = za::min(staticUntil, settled);
                }
            }
        }
    }
    const gfx::State state{
        .shade = gfx::Shade::Texture, .blend = gfx::Blend::Modulate, .depthTest = true, .depthWrite = false};
    if(staticDirty)
    {
        staticDirty = false;
        gfx::upload(staticTriangles, staticVertices);
    }
    gfx::draw(staticTriangles, gfx::sceneViewProjection(), state, atlas);
    if(!vertices.empty())
    {
        gfx::draw(vertices, gfx::sceneViewProjection(), state, atlas);
    }
}

void count_f()
{
    int kinds[KindCount]{};
    for(za::SizeT k = 0; k < decals.size(); k++)
    {
        const Decal& d = decals[k];
        int kind = KindCount - 1;
        while(kind > 0 && d.cell < firstCell[kind])
        {
            kind--;
        }
        kinds[kind]++;
    }
    Con_Printf("%d decals: %d blood, %d drops, %d scorch, %d chips, %d splatters, %d streaks, %d pools, %d splotches\n",
        static_cast<int>(decals.size()), kinds[Blood], kinds[BloodDrop], kinds[Scorch], kinds[Hole], kinds[Splatter],
        kinds[Streak], kinds[Pool], kinds[Splotch]);
    Con_Printf("%d vertices settled, %d changing (made again each frame)\n", static_cast<int>(staticVertices.size()),
        static_cast<int>(vertices.size()));
    Con_Printf("settled vertices uploaded %lld times so far, %.1f KB in all (%.1f KB a frame for both eyes if copied "
               "at each draw)\n",
        staticTriangles.uploads, static_cast<double>(staticTriangles.uploadedBytes) / 1024.0,
        static_cast<double>(staticVertices.size() * sizeof(gfx::Vertex) * 2) / 1024.0);
    gore::count();
}

void atlas_f()
{
    // As the marks look on a mid grey wall: grey * (colour + 1 - alpha), the texels premultiplied.
    const za::Vector<unsigned char> rgba = buildAtlas();
    za::Vector<unsigned char> out(static_cast<za::SizeT>(atlasWidth) * atlasHeight * 3);
    for(za::SizeT i = 0; i < out.size() / 3; i++)
    {
        const float dim = rgba[i * 4 + 3] / 255.f;
        for(int c = 0; c < 3; c++)
        {
            const float f = rgba[i * 4 + c] / 255.f + 1.f - dim;
            out[i * 3 + c] = static_cast<unsigned char>(za::clamp(f * 0.6f, 0.f, 1.f) * 255.f + 0.5f);
        }
    }
    const char* name = "decal_atlas.png";
    if(Image_WritePNG(name, out.data(), atlasWidth, atlasHeight, 24, false))
    {
        Con_Printf("Wrote %s/%s (%d x %d cells: blood, drops, scorches, chips, splatters, streaks, pools, splotches)\n",
            com_gamedir, name, cellsPerRow, cellRows);
    }
}

void init()
{
    if(!atlasTexels.valid() && !atlas)
    {
        atlasTexels = jobs::async(buildAtlas);
    }
}

void prepare()
{
    if(!atlas && vr_decals.value)
    {
        makeAtlas();
    }
}

void clear()
{
    decals.clear();
    dropCount = 0;
    builtFrame = -1;
    gibs.clear();
    holes.clear();
    gore::clear();
}

// Decals on the walls (vr_memstats).
int liveCount()
{
    return static_cast<int>(decals.size());
}

} // namespace qvr::decals

// cl_tent.c: Quake's own wall hits and explosions.
extern "C" void VR_DecalTempEntity(int scorch, const float* pos)
{
    using qvr::particles::Preset;
    qvr::decals::fromEffect(
        {pos[0], pos[1], pos[2]}, glm::vec3{0.f}, scorch ? Preset::Explosion : Preset::BulletPuff, 1);
}

// CL_RelinkEntities, every frame for each entity: Hipnotic's bullet holes (progs/s_bullet.spr, the
// QC leaves one where each bullet and nail hits the world) are low-resolution sprites; with
// decals on, each becomes a chip instead (once, when it first shows up) and the sprite is not
// drawn. Nonzero if it is not to be drawn.
extern "C" int VR_BulletHoleSprite(int ent)
{
    using namespace qvr;
    using namespace qvr::decals;

    const entity_t& e = cl_entities[ent];
    if(!e.model || e.model->type != mod_sprite || strcmp(e.model->name, "progs/s_bullet.spr") ||
        !(cl.protocolflags & PRFL_QUAKEVR) || !vr_decals.value || !cl.worldmodel)
    {
        return 0;
    }

    // Forgotten when the slot holds something else.
    if(host_framecount - holesPrunedFrame > 100 || host_framecount < holesPrunedFrame)
    {
        holesPrunedFrame = host_framecount;
        erase_if(holes, [](const auto& kv) { // (ankerl's, by ADL: std::erase_if's for its maps)
            const qmodel_t* m = kv.first < cl.num_entities ? cl_entities[kv.first].model : nullptr;
            return !m || strcmp(m->name, "progs/s_bullet.spr");
        });
    }

    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};
    const auto it = holes.find(ent);
    if(it == holes.end() || glm::distance(it->second, o) > 0.5f)
    {
        holes[ent] = o;
        chip(o);
    }
    return 1;
}

// CL_RelinkEntities, every frame for each gib and head (flying, sliding, lying, carried): Quake VR's
// blood trail behind it while it moves, drops on the floor under it (a trail that stays), and a
// splat and a spurt of blood where it hits a wall or the floor. A hit is seen in the positions the
// server sends: the velocity between the last two turning more than gravity turns it. Nonzero if
// Quake VR's particles drew the trail (Quake's is not drawn then).
extern "C" int VR_GibTrail(int ent, int zombie)
{
    using namespace qvr;
    using namespace qvr::decals;
    QVR_PROFILE("gib trail");
    if(!vr_gib_blood.value || ent <= 0 || ent >= cl.num_entities || !cl.worldmodel)
    {
        return 0;
    }
    const entity_t& e = cl_entities[ent];
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};

    // Forgotten when not seen for a second.
    if(host_framecount - gibsPrunedFrame > 100 || host_framecount < gibsPrunedFrame)
    {
        gibsPrunedFrame = host_framecount;
        erase_if(gibs, [](const auto& kv) { return cl.time - kv.second.seen > 1.0 || cl.time < kv.second.seen; });
    }

    // A new one (or a new gib in the slot, or a teleport): from here.
    Gib& g = gibs[ent];
    if(g.model != e.model || cl.time - g.seen > 0.25 || cl.time < g.seen || glm::distance(g.origin, o) > 128.f)
    {
        g = Gib{};
        g.model = e.model;
        g.origin = o;
        g.msgTime = e.msgtime;
        g.dropsLeft = gibDrops;
    }

    // A new position from the server: its velocity since the one before, and whether the world
    // turned it (a bounce, a landing, a wall).
    const double dt = cl.mtime[0] - cl.mtime[1];
    if(e.msgtime != g.msgTime && dt > 0.001)
    {
        g.msgTime = e.msgtime;
        const glm::vec3 p0{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]};
        const glm::vec3 p1{e.msg_origins[1][0], e.msg_origins[1][1], e.msg_origins[1][2]};
        const glm::vec3 v = (p0 - p1) / static_cast<float>(dt);
        if(g.moving && glm::length(g.velocity) > gibSplatSpeed && cl.time - g.splatTime > 0.15)
        {
            const glm::vec3 change = v - (g.velocity - glm::vec3{0.f, 0.f, sv_gravity.value * static_cast<float>(dt)});
            if(glm::length(change) > gibSplatSpeed)
            {
                g.splatTime = cl.time;
                splat(p1, p0, g.velocity, change);
            }
        }
        g.velocity = v;
        g.moving = true;
    }

    // The trail, where it went since the last frame (a zombie's gibs bleed less).
    const float density = za::clamp(vr_gib_blood_trail.value, 0.f, 4.f) * (zombie ? 0.5f : 1.f);
    const bool ours = particles::enabled();
    const float travelled = glm::distance(g.origin, o);
    if(density > 0.f && travelled > 0.01f)
    {
        const glm::vec3 dir = (o - g.origin) / travelled;
        if(ours)
        {
            along(g.origin, o, gibTrailSpacing / density, g.sinceTrail, 24,
                [&](const glm::vec3& p) { particles::spawn(p, dir, particles::Preset::BloodTrail, 1); });
        }
        if(vr_decals.value && g.dropsLeft > 0)
        {
            along(g.origin, o, gibDropSpacing / density, g.sinceDrop, za::min(4, g.dropsLeft),
                [&](const glm::vec3& p) {
                    drip(p);
                    g.dropsLeft--;
                });
        }
    }

    // At rest (vr_gore): a small pool spreads under it; hanging (nothing under it: stuck to a
    // ceiling or a wall, or held still), it drips.
    if(travelled < 0.05f && vr_decals.value && gore::level() > 0)
    {
        if(g.stillSince <= 0.0)
        {
            g.stillSince = cl.time;
        }
        if(!g.restChecked && cl.time - g.stillSince > 0.3)
        {
            g.restChecked = true;
            glm::vec3 where, normal;
            float f;
            const float below = (e.model ? za::clamp(-e.model->mins[2], 4.f, 16.f) : 8.f) + 10.f;
            g.hanging = !hitWorld(o, o - glm::vec3{0.f, 0.f, below}, where, normal, f);
            if(!g.hanging && normal.z > 0.7f && g.pools < 2)
            {
                g.pools++;
                gore::gibRest(where, normal);
            }
        }
        if(g.hanging)
        {
            gore::gibHanging(ent, o);
        }
    }
    else if(travelled >= 0.05f)
    {
        g.stillSince = 0.0;
        g.restChecked = false;
        g.hanging = false;
    }
    g.origin = o;
    g.seen = cl.time;
    return ours;
}
