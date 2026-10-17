// vr_decals.cpp -- see vr_decals.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_decals.hpp"
#include "vr_brushes.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_gore.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_modellight.hpp"
#include "vr_permdecals.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_retro.hpp"
#include "vr_stereo.hpp"
#include "vr_ring.hpp"
#include "vr_trace.hpp"

#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strncmp.hpp"
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
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/type_ptr.hpp>


namespace qvr::decals
{
namespace
{

// The vr_decal_stress command's lattice permutation counter (stress_f): file scope, not a function-local static.
unsigned stressSerial = 0;
// The vr_decal_carve_test command's lattice place counter (carveTest_f), and its last marks' middle, surface normal and
// way across (for its look).
unsigned carveTestSerial = 0;
glm::vec3 carveTestCentre{0.f}, carveTestNormal{0.f}, carveTestAcross{0.f};

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
    // The melee marks' (vr_decal_melee): a blunt blow's dent, a blade's groove (stretched along it), carved
    Scuff,    // the surface crushed a little darker in the middle, its dust lighter round it
    KindCount
};

// The gore's own kinds (Splatter to Splotch): big, drawn by goreTexel.
[[nodiscard]] constexpr bool goreKind(int kind)
{
    return kind >= Splatter && kind <= Splotch;
}

// The atlas: 8 x 4 cells of 256 texels (2048 x 1024 RGBA, mipmapped: 11 MB). Blood splats 0-5,
// drops 6-7, scorches 8-10, chips 11-15, splatters 16-19, streaks 20-22, pools 23-25, splotches 26-28, scuffs 29-30.
constexpr int cellSize = 256;
constexpr int cellsPerRow = 8;
constexpr int cellRows = 4;
constexpr int atlasWidth = cellSize * cellsPerRow;
constexpr int atlasHeight = cellSize * cellRows;
constexpr int firstCell[KindCount] = {0, 6, 8, 11, 16, 20, 23, 26, 29};
constexpr int cellCount[KindCount] = {6, 2, 3, 5, 4, 3, 3, 3, 2};
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
    glm::vec3 normal{0.f};   // the surface's it was laid on,
    float depth = 0.f;       // and how far from that plane a face takes it (clipToWorld)
    int staticAt = -1;       // its first vertex in staticVertices while it is settled there (-1: it is not)
    // The world decal grid's buckets it is listed in (worldBuckets), for the grid of `bucketsMask` (0: not made yet):
    // a mark keeps its place, so only the new ones are hashed when the grid is made again.
    za::Vector<za::U32> buckets;
    za::U32 bucketsMask = 0;
    int brush = 0; // the runtime brush (vr_brushes.cpp) it lies on, by id (0: none; -1: more than one)
    bool permTried = false; // baked into the permanent layer and found no face there (vr_permdecals: it stays live)
    bool fromLayer = false; // one of the permanent layer's, shown live while it is off (showRecord: dropped when on)
    // Carved (vr_decal_carve; the shader's Decal.carve): its crater's depth in units (0: flat), radius in units, rim's
    // height over its depth, dark core's part of the radius + 2 x its shape (0 a pit, 1 a nail's hole, 2 a blast's).
    glm::vec4 carve{0.f};
};

// Oldest first (drawn in that order; the oldest go first): a ring of vr_decal_max slots, whose marks' triangles keep
// their buffers for the next mark laid there.
Ring<Decal> decals;
int dropCount = 0; // the blood drops among them

[[nodiscard]] bool isDrop(const Decal& d)
{
    return d.cell >= firstCell[BloodDrop] && d.cell < firstCell[BloodDrop] + cellCount[BloodDrop];
}

za::Vector<gfx::Vertex> vertices; // the marks that change this frame
// The others (settled: shown, spread and darkened, not yet fading), in the order they settled, in their own buffer:
// a mark's triangles are added when it settles and uploaded then, not for each eye and frame, and not again when other
// marks come or go. A mark that starts to fade or goes leaves its triangles there with no area (drawing nothing) until
// enough have gone to pack the rest.
za::Vector<gfx::Vertex> staticVertices;
gfx::StaticTriangles staticTriangles;
int staticGone = 0;        // vertices in staticVertices left by marks that went
bool staticResend = false; // all of staticVertices to upload (packed, or cleared)
struct VertexRange
{
    int first, end;
};
za::Vector<VertexRange> staticChanged; // what else changed in it since it was uploaded

void staticTouched(int first, int end)
{
    if(!staticChanged.empty() && staticChanged.back().end == first)
    {
        staticChanged.back().end = end; // (the marks settling in a frame: one range)
        return;
    }
    staticChanged.pushBack({first, end});
}

// Its triangles out of the settled marks' buffer (left there with no area: drawn as nothing).
void unsettle(Decal& d)
{
    if(d.staticAt < 0)
    {
        return;
    }
    const int first = d.staticAt, end = first + static_cast<int>(d.tris.size());
    for(int i = first; i < end; i++)
    {
        staticVertices[static_cast<za::SizeT>(i)].pos = staticVertices[static_cast<za::SizeT>(first)].pos;
    }
    staticTouched(first, end);
    staticGone += end - first;
    d.staticAt = -1;
}

bool worldDirty = true; // decals came or went since the world's shader had them (VR_DecalsFrame)

void popOldest()
{
    worldDirty = true;
    unsettle(decals.front());
    dropCount -= isDrop(decals.front());
    decals.popFront();
}
int builtFrame = -1; // the host frame `vertices` were built in (-1 after a clear)
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

// A melee mark's scuff (Scuff; its carving does the rest): the surface crushed a little darker in the middle, dust a
// little lighter round it, mottled; nothing past its edge (what the surface is multiplied by).
[[nodiscard]] glm::vec3 scuffTexel(int seed, glm::vec2 p)
{
    const float r = glm::length(p);
    const float grain = 0.6f * noise(p * 9.f, seed) + 0.4f * noise(p * 23.f, seed + 1);
    const float crush = smoothstep(0.55f, 0.f, r) * (0.08f + 0.1f * grain);
    const float dust = smoothstep(0.85f, 0.5f, r) * smoothstep(0.15f, 0.5f, r) * 0.08f * grain;
    return glm::vec3{1.f - crush + dust};
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
    const float dim = za::max(0.f, 1.f - za::min(f.r, f.g, f.b));
    const glm::vec3 c = f - 1.f + dim;
    for(int i = 0; i < 3; i++)
    {
        out[i] = static_cast<unsigned char>(za::clamp(c[i], 0.f, 1.f) * 255.f + 0.5f);
    }
    out[3] = static_cast<unsigned char>(za::clamp(dim, 0.f, 1.f) * 255.f + 0.5f);
}

jobs::Site atlasSite{"decal atlas"}; // (its parallelFor: vr_jobs_sites)

[[nodiscard]] za::Vector<unsigned char> buildAtlas()
{
    za::Vector<unsigned char> rgba(atlasWidth * atlasHeight * 4, 0);
    // The cells shared out among the game's threads (vr_jobs.hpp): each writes its own texels, its random numbers seeded
    // by its number (the thread's own generator): the same atlas whatever ran it.
    const int cells = firstCell[KindCount - 1] + cellCount[KindCount - 1];
    jobs::parallelFor(atlasSite, static_cast<za::SizeT>(cells), 1, [&](za::SizeT c0, za::SizeT c1) {
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
            const GoreShape gore = goreKind(kind) ? goreShape(static_cast<Kind>(kind)) : GoreShape{};

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
                    else if(goreKind(kind))
                    {
                        f = goreTexel(static_cast<Kind>(kind), seed, gore, p);
                    }
                    else if(kind == Scuff)
                    {
                        f = scuffTexel(seed, p);
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
                    const int edge = za::min(x, y, cellSize - 1 - x, cellSize - 1 - y);
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
    atlas = gfx::createTexture(atlasWidth, atlasHeight, rgba.data(), true, "decal atlas");
}

// ---- Placing ----------------------------------------------------------------------------------

// The static world along `from` -> `to`: where, and its normal.
[[nodiscard]] int entityAlong(const glm::vec3& from, const glm::vec3& to, float fraction, trace_t& tr);

[[nodiscard]] bool hitWorld(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    // With the permanent layer (vr_permdecals), the brush models too: a mark on a door is baked into its faces.
    trace_t tr = worldtrace::world(from, to, permdecals::on());
    bool world = tr.fraction < 1.f && !tr.startsolid && !tr.allsolid;
    // A brush entity of its own file (or the map's, the permanent layer off) nearer: marked in its space (add).
    if(trace_t te; entityAlong(from, to, world ? tr.fraction : 1.f, te) > 0)
    {
        tr = te;
        world = true;
    }
    // The boxes made in the game are the world's kind too (vr_brushes.cpp): the nearer of the two.
    glm::vec3 bw, bn;
    float bf = 1.f;
    if(brushes::traceLine(from, to, bw, bn, bf) && (!world || bf < tr.fraction))
    {
        where = bw;
        normal = bn;
        fraction = bf;
        return true;
    }
    if(!world)
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

constexpr za::SizeT maxCorners = 32 * 3; // a mark's triangles' corners at most: over very broken ground, the rest left out

// Face `s` of model `m` as a polygon, into `poly` (emptied first).
void facePolygon(const qmodel_t* m, const msurface_t* s, za::Vector<glm::vec3>& poly)
{
    poly.clear();
    for(int e = 0; e < s->numedges; e++)
    {
        const int se = m->surfedges[s->firstedge + e];
        const unsigned int vi = se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1];
        const float* p = m->vertexes[vi].position;
        poly.pushBack({p[0], p[1], p[2]});
    }
}

// scratch.poly (a face facing `sn`) cut to decal `d`'s footprint, as triangles added to `tris`.
void layOnFace(const Decal& d, const glm::vec3& sn, za::Vector<Corner>& tris)
{
    const float hu = glm::length(d.u), hv = glm::length(d.v);
    const glm::vec3 U = d.u / hu, V = d.v / hv;
    const float sideDist[4] = {glm::dot(d.centre, U) + hu, glm::dot(d.centre, -U) + hu, glm::dot(d.centre, V) + hv,
        glm::dot(d.centre, -V) + hv};
    const glm::vec3 sideNormal[4] = {U, -U, V, -V};
    za::Vector<glm::vec3>& poly = scratch.poly;
    za::Vector<glm::vec3>& clipped = scratch.clipped;
    for(int k = 0; k < 4 && poly.size() >= 3; k++)
    {
        clipPolygon(poly, sideNormal[k], sideDist[k], clipped);
        poly.swap(clipped);
    }
    if(poly.size() < 3)
    {
        return;
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

// Lays decal `d` (on a surface facing `n`) on the world: its footprint (the box of its centre, u
// and v, `depth` deep either side of the surface) cut out of each face of the world in it that faces
// about its way, as triangles (into `tris`, emptied first), lifted off the face a little. Faces are found down the BSP
// tree (those on the nodes whose planes cross the box); sky, liquids and fences are left out. `brush`: the runtime brush
// it was laid on (0: none, -1: more than one).
void clipToWorld(const Decal& d, const glm::vec3& n, float depth, za::Vector<Corner>& tris, int& brush)
{
    tris.clear();
    brush = 0;
    const qmodel_t* m = cl.worldmodel;
    const float hu = glm::length(d.u), hv = glm::length(d.v);
    if(!m || !m->nodes || hu <= 0.f || hv <= 0.f)
    {
        return;
    }
    const glm::vec3 extent = glm::abs(d.u) + glm::abs(d.v) + glm::abs(n) * depth;
    const glm::vec3 lo = d.centre - extent, hi = d.centre + extent;
    za::Vector<glm::vec3>& poly = scratch.poly;
    const auto layOn = [&](const glm::vec3& sn) { layOnFace(d, sn, tris); };
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
            facePolygon(m, s, poly);
            layOn(sn);
        }
    }
    // The boxes made in the game (vr_brushes.cpp): their faces in the footprint's box, as the world's.
    brushes::facesIn(lo, hi, [&](const za::Vector<glm::vec3>& face, const glm::vec3& sn, int id) {
        if(tris.size() >= maxCorners || face.size() < 3 || glm::dot(sn, n) < 0.5f ||
            za::fabs(glm::dot(d.centre - face[0], sn)) > depth)
        {
            return;
        }
        poly.clear();
        for(const glm::vec3& p : face)
        {
            poly.pushBack(p);
        }
        const za::SizeT before = tris.size();
        layOn(sn);
        if(tris.size() > before)
        {
            brush = brush == 0 || brush == id ? id : -1;
        }
    });
}

// ---- On brush entities (vr_decal_entities) ------------------------------------------------------

// The marks on a brush entity whose model is a file of its own (the firing range's spawn buttons and panels; one model
// for many entities, so the permanent layer, laid out on a model's faces, can't take them), or on one of the map's
// own (*n: doors, lifts) with that layer off: each cut out of the entity's faces in its own space, kept live and drawn
// where the entity is drawn, each frame (moving, turning with it). Flat: the carved relief is the world's shader's.
struct EntityDecal
{
    int ent = 0;
    const qmodel_t* model = nullptr;
    Decal d; // in the model's space
};
constexpr za::SizeT maxEntityDecals = 128;
Ring<EntityDecal> entityDecals{maxEntityDecals};
za::Vector<gfx::Vertex> entityVertices, entityLocal; // (drawEntityDecals')

// Whether a mark on entity `e` is laid on it here (not the items' boxes, b_*.bsp; not the boxes made in the game,
// vr_brushes.cpp; not the map's own models while the permanent layer bakes them).
[[nodiscard]] bool markable(const entity_t& e)
{
    const qmodel_t* m = e.model;
    if(!vr_decal_entities.value || !m || m->type != mod_brush || m == cl.worldmodel || VR_IsRuntimeBrushModel(m))
    {
        return false;
    }
    if(m->name[0] == '*')
    {
        return !permdecals::on();
    }
    return ZA_STRNCMP(m->name, "maps/b_", 7) != 0;
}

// The markable brush entity along `from` -> `to` nearer than the world's `fraction` (1: none), or 0.
[[nodiscard]] int entityAlong(const glm::vec3& from, const glm::vec3& to, float fraction, trace_t& tr)
{
    if(!vr_decal_entities.value)
    {
        return 0;
    }
    int ent = 0;
    tr = worldtrace::world(from, to, true, true, 0, 0, &ent);
    return ent > 0 && tr.fraction < fraction && !tr.startsolid && markable(cl_entities[ent]) ? ent : 0;
}

// Lays `d` (in model `m`'s space) on that model's faces, as clipToWorld on the world's.
void clipToModel(const Decal& d, const qmodel_t* m, za::Vector<Corner>& tris)
{
    tris.clear();
    if(glm::length(d.u) <= 0.f || glm::length(d.v) <= 0.f)
    {
        return;
    }
    const glm::vec3 extent = glm::abs(d.u) + glm::abs(d.v) + glm::abs(d.normal) * d.depth;
    const glm::vec3 lo = d.centre - extent, hi = d.centre + extent;
    for(int i = 0; i < m->nummodelsurfaces && tris.size() < maxCorners; i++)
    {
        const msurface_t* s = m->surfaces + m->firstmodelsurface + i;
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
        if(glm::dot(sn, d.normal) < 0.5f || za::fabs(glm::dot(d.centre, sn) - s->plane->dist * flip) > d.depth)
        {
            continue;
        }
        facePolygon(m, s, scratch.poly);
        layOnFace(d, sn, tris);
    }
}

// Mark `d` (in the world's space) laid on brush entity `ent`, in its space: false if it found no face of it.
bool addOnEntity(int ent, const Decal& d)
{
    entity_t& e = cl_entities[ent];
    float mat[16];
    VR_BModelMatrix(&e, mat);
    const glm::mat4 toModel = glm::inverse(glm::make_mat4(mat));
    const glm::mat3 turn{toModel};
    Decal local = d;
    local.centre = glm::vec3{toModel * glm::vec4{d.centre, 1.f}};
    local.u = turn * d.u;
    local.v = turn * d.v;
    local.normal = glm::normalize(turn * d.normal);
    clipToModel(local, e.model, scratch.tris);
    if(scratch.tris.empty())
    {
        return false;
    }
    if(entityDecals.full())
    {
        entityDecals.popFront();
    }
    EntityDecal& slot = entityDecals.pushBack();
    slot.ent = ent;
    slot.model = e.model;
    slot.d = local;
    slot.d.tris.clear();
    for(const Corner& c : scratch.tris)
    {
        slot.d.tris.pushBack(c);
    }
    return true;
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

// Mark `d` as the permanent layer bakes it (vr_permdecals.cpp): settled (spread to its full size, darkened).
[[nodiscard]] permdecals::Stamp stampOf(const Decal& d)
{
    return {d.centre, d.u, d.v, d.normal, d.depth, 1.f - d.darken, d.cell, 0};
}

// A live mark the ring lets go of (to the cap, a drop's limit, its life) before it was baked: baked as it would have
// settled with the permanent layer on (vr_permdecals), not lost; with the layer off, kept for when it is on again
// (permdecals::record; the rule: vr_permdecals.hpp's kept).
void bakeEarly(const Decal& d)
{
    if(d.brush != 0 || d.permTried || d.fromLayer)
    {
        return;
    }
    if(permdecals::on())
    {
        (void)permdecals::bakeWorld(stampOf(d));
    }
    else
    {
        permdecals::record(stampOf(d));
    }
}

// Mark `d` (its triangles `tris`) into the ring: the oldest go first if it is full (as many as vr_decal_max, lowered,
// leaves no room for), the ring made `max` long if that changed; into the ring's next slot, its triangles into the
// buffer the slot's last mark had.
void pushDecal(const Decal& d, const za::Vector<Corner>& tris, int max)
{
    while(decals.size() >= static_cast<za::SizeT>(max))
    {
        bakeEarly(decals.front());
        popOldest();
    }
    if(decals.capacity() != static_cast<za::SizeT>(max))
    {
        decals.setCapacity(static_cast<za::SizeT>(max));
    }
    Decal& slot = decals.pushBack();
    za::Vector<Corner> buffer = ZA_MOVE(slot.tris);
    za::Vector<za::U32> bucketBuffer = ZA_MOVE(slot.buckets);
    slot = d;
    slot.staticAt = -1;
    slot.tris = ZA_MOVE(buffer);
    slot.buckets = ZA_MOVE(bucketBuffer);
    slot.buckets.clear();
    slot.bucketsMask = 0;
    slot.tris.assignRange(tris.begin(), tris.end());
    dropCount += isDrop(slot);
    worldDirty = true; // (`vertices`, made each frame, take it in the next one: in both eyes together)
}

// The permanent layer switched off (vr_permdecals 0), or a save loaded with it off: the newest of its marks on the
// static world shown live, as many as the ring has room for (vr_decal_max with the live ones), older than the live ones
// (the ring's order), settled (spread, darkened) and with a whole life ahead (vr_decal_life). They stay in the layer's
// stamps (fromLayer: dropped from the ring, not baked again, when it is on again). The rule: vr_permdecals.hpp's kept.
za::Vector<permdecals::Stamp> recordShown; // (showRecord's)
za::Vector<Decal> liveKept;               // (showRecord's: the live marks moved behind those shown)

void showRecord()
{
    const int max = static_cast<int>(vr_decal_max.value);
    if(max <= 0 || !cl.worldmodel)
    {
        return;
    }
    QVR_PROFILE("decals from the permanent layer");
    permdecals::newestWorld(max - static_cast<int>(za::min(decals.size(), static_cast<za::SizeT>(max))), recordShown);
    if(recordShown.empty())
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }
    liveKept.clear();
    while(!decals.empty())
    {
        unsettle(decals.front());
        dropCount -= isDrop(decals.front());
        liveKept.pushBack(ZA_MOVE(decals.front()));
        decals.popFront();
    }
    worldDirty = true;
    za::Vector<Corner>& tris = scratch.tris;
    int shown = 0;
    for(const permdecals::Stamp& s : recordShown)
    {
        Decal d{s.centre, s.u, s.v, s.cell, cl.time - 9.0};
        d.darken = za::clamp(1.f - s.dark, 0.f, 0.9f);
        d.normal = s.normal;
        d.depth = s.depth;
        d.fromLayer = true;
        clipToWorld(d, d.normal, d.depth, tris, d.brush);
        if(!tris.empty())
        {
            pushDecal(d, tris, max);
            shown++;
        }
    }
    for(Decal& d : liveKept)
    {
        pushDecal(d, d.tris, max);
    }
    liveKept.clear();
    Con_DPrintf("decals: %d of the permanent layer's %d newest marks shown live (vr_permdecals 0)\n", shown,
        static_cast<int>(recordShown.size()));
}

// `carve`: carved (vr_decal_carve) as {its crater's depth in units (0 flat), radius over the mark's half size, rim's
// height over its depth, core + 2 x shape} (Decal::carve).
bool add(Kind kind, const glm::vec3& where, const glm::vec3& normal, float size, const MarkOptions& o = {},
    const glm::vec4& carve = glm::vec4{0.f})
{
    QVR_PROFILE("decal add");
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
    // Turned along `along` on the surface, if given (a spray's way, a run's down, a blade's groove).
    bool along = false;
    if(const glm::vec3 a = o.along - n * glm::dot(o.along, n); glm::length(a) > 0.1f * glm::length(o.along) && glm::length(a) > 1e-4f)
    {
        u = glm::normalize(a);
        along = true;
    }
    glm::vec3 v = glm::cross(n, u);
    if((kind == Hole || carve.x > 0.f) && !along)
    {
        // Its relief lit from where the light comes from (v: the cell's lit side; a carved crater's guess at where the
        // baked light comes from, CarveAt); mirrored at random for variety (the light stays).
        if(const glm::vec3 lit = lightAlong(where, n); glm::dot(lit, lit) > 0.f)
        {
            v = lit;
            u = glm::cross(v, n) * (rand() & 1 ? 1.f : -1.f);
        }
    }
    // Laid on the world's faces under it, cut where they end (an edge, a corner): over a wall's
    // panels and recesses, the floor of an alcove, the steps of a stair. A run down a wall keeps
    // its top where it is.
    const bool big = goreKind(kind);
    const float halfV = size * 0.5f, halfU = halfV * za::max(0.25f, o.aspect);
    Decal d{o.fromStart ? where + u * halfU : where, u * halfU, v * halfV, firstCell[kind] + rand() % cellCount[kind],
        cl.time + o.delay};
    d.grow = o.grow;
    d.growFrom = za::clamp(o.growFrom, 0.f, 1.f);
    d.fromStart = o.fromStart;
    d.darken = za::clamp(o.darken, 0.f, 0.9f);
    d.normal = n;
    d.depth = big ? za::clamp(halfV * 0.35f, 4.f, 12.f) : 3.f;
    d.carve = carve.x > 0.f ? glm::vec4{carve.x, carve.y * halfV, carve.z, carve.w} : glm::vec4{0.f};
    // On a brush entity of its own file (or the map's, the permanent layer off): laid on it, in its space.
    trace_t onEnt;
    if(const int ent = entityAlong(where + n * 2.f, where - n * 3.f, 1.f, onEnt); ent > 0)
    {
        return addOnEntity(ent, d);
    }
    za::Vector<Corner>& tris = scratch.tris;
    clipToWorld(d, n, d.depth, tris, d.brush);
    // With the permanent layer (vr_permdecals), its part on the brush models (doors, lifts) baked when it shows (at once,
    // or after its delay), in their own space: the live marks are the static world's.
    int onModels = 0;
    if(permdecals::on())
    {
        if(!atlas)
        {
            makeAtlas();
        }
        onModels = permdecals::bakeEntities(stampOf(d), d.born); // (shown later: baked then)
    }
    if(tris.empty())
    {
        return onModels > 0;
    }
    // Drops (a steady drip from wounds, gibs and ceilings) take at most a quarter of the marks: the
    // oldest drop goes, not a splat or a pool (the others keep their order).
    if(kind == BloodDrop && dropCount >= za::max(16, max / 4))
    {
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            if(isDrop(decals[k]))
            {
                bakeEarly(decals[k]);
                unsettle(decals[k]);
                worldDirty = true;
                decals.eraseAt(k);
                dropCount--;
                break;
            }
        }
    }
    pushDecal(d, tris, max);
    return true;
}

[[nodiscard]] float random(float lo, float hi)
{
    return lo + (hi - lo) * static_cast<float>(rand() % 1000) * 0.001f;
}

// ---- Carved holes and craters (vr_decal_carve) -------------------------------------------------

// What hit the wall, as the carved marks tell them apart (Quake's temp entities, VR_DecalTempEntity; the QC's effects,
// fromEffect). The expansions' and Arcane Dimensions' weapons send the same ones.
enum class Impact
{
    Pellet,        // a shotgun's pellet, an enforcer's or a gunner's bullet (TE_GUNSHOT)
    Nail,          // a nail (TE_SPIKE)
    SuperNail,     // a super nail (TE_SUPERSPIKE)
    LavaNail,      // Dissolution of Eternity's lava nails (TE_SPIKE, TE_SUPERSPIKE from a progs/lspike.mdl)
    LavaSuperNail,
    MonsterSpike,  // a scrag's or a death knight's (TE_WIZSPIKE, TE_KNIGHTSPIKE)
    Blow,          // a melee blow, a stuck axe, the grapple (the QC's bullet puffs and sparks)
    Blast          // a rocket's, a grenade's (TE_EXPLOSION, TE_EXPLOSION2, the QC's explosions)
};

// Each's mark: its kind, size (and +- at random), its crater (depth in units, radius over the mark's half size, rim's
// height over its depth, dark core's part of the radius, shape: 0 a pit, 1 a nail's hole, 2 a blast's), and the damage
// cvar it grows with (as its square root, from that cvar's shipped value; none: nullptr).
struct ImpactShape
{
    Kind kind;
    float size, jitter;
    float depth, radius, rim, core, shape;
    const cvar_t* damage;
    float shipped;
};

[[nodiscard]] ImpactShape impactShape(Impact what)
{
    switch(what)
    {
        case Impact::Pellet: return {Hole, 6.5f, 1.f, 0.9f, 0.46f, 0.32f, 0.3f, 0.f, &vr_dmg_shotgun, 4.f};
        case Impact::Nail: return {Hole, 7.f, 0.8f, 1.7f, 0.3f, 0.25f, 0.5f, 1.f, &vr_dmg_nail, 9.f};
        case Impact::SuperNail: return {Hole, 9.f, 1.f, 2.2f, 0.34f, 0.25f, 0.5f, 1.f, &vr_dmg_super_nail, 18.f};
        case Impact::LavaNail: return {Hole, 9.5f, 1.f, 2.f, 0.36f, 0.3f, 0.45f, 1.f, &vr_dmg_lava_nail, 15.f};
        case Impact::LavaSuperNail: return {Hole, 11.f, 1.f, 2.4f, 0.37f, 0.3f, 0.45f, 1.f, &vr_dmg_lava_nail, 15.f};
        case Impact::MonsterSpike: return {Hole, 8.f, 1.f, 1.6f, 0.34f, 0.25f, 0.45f, 1.f, nullptr, 1.f};
        case Impact::Blow: return {Hole, 7.f, 1.f, 0.6f, 0.42f, 0.35f, 0.2f, 0.f, nullptr, 1.f};
        case Impact::Blast: return {Scorch, 52.f, 8.f, 4.f, 0.46f, 0.4f, 0.f, 2.f, &vr_dmg_rocket, 100.f};
    }
    return {Hole, 6.5f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, nullptr, 1.f};
}

// The melee marks laid lately (meleeEvent): the same blow's Quake effect (a gunshot's puff, sparks) leaves no pit of its
// own next to one, nor does the same blow (a held prop's knock: its physics hit too) a second mark.
struct RecentMelee
{
    glm::vec3 at{0.f};
    double time = -1.0;
};
constexpr int recentMeleeCount = 8;
RecentMelee recentMelee[recentMeleeCount];
int recentMeleeNext = 0;

[[nodiscard]] bool meleeMarkNear(const glm::vec3& at, float within, double since)
{
    for(const RecentMelee& r : recentMelee)
    {
        if(r.time >= 0.0 && cl.time >= r.time && cl.time - r.time <= since && glm::distance(r.at, at) < within)
        {
            return true;
        }
    }
    return false;
}

// A mark of `what` laid at `where` (normal `normal`): carved, unless it is off (vr_decal_carve 0: Quake VR's flat marks,
// their sizes as before carving).
bool impactMark(Impact what, const glm::vec3& where, const glm::vec3& normal)
{
    const ImpactShape s = impactShape(what);
    const bool carving = vr_decal_carve.value != 0.f;
    if((what == Impact::Pellet || what == Impact::Blow) && meleeMarkNear(where, 10.f, 0.25))
    {
        return false; // a melee blow's own mark is there (meleeEvent)
    }
    if(!carving)
    {
        const float size = s.kind == Scorch ? random(44.f, 60.f) : what == Impact::Blow ? random(6.f, 8.f) : random(5.5f, 7.5f);
        return add(s.kind, where, normal, size);
    }
    const float grows = s.damage ? za::clamp(za::sqrt(za::max(s.damage->value, 0.f) / s.shipped), 0.6f, 1.8f) : 1.f;
    const float wide = za::clamp(vr_decal_carve_size.value, 0.5f, 2.f);
    const float size = (s.size + random(-s.jitter, s.jitter)) * grows * wide;
    return add(s.kind, where, normal, size, {}, glm::vec4{s.depth * grows, s.radius, s.rim, s.core + 2.f * s.shape});
}

// Where `what` at `org` (going along `dir`, may be zero) marks the world: a hit's on the surface nearest it, a blast's
// below it (else nearest).
void impactAt(Impact what, const glm::vec3& org, const glm::vec3& dir)
{
    glm::vec3 where, normal;
    const glm::vec3 back = glm::dot(dir, dir) > 1e-4f ? -glm::normalize(dir) : glm::vec3{0.f};
    if(what == Impact::Blast ? nearest(org, 64.f, glm::vec3{0, 0, -1}, where, normal) : nearest(org, 6.f, back, where, normal))
    {
        (void)impactMark(what, where, normal);
    }
}

// ---- Melee marks (vr_decal_melee) ---------------------------------------------------------------

// What a melee blow on the world leaves (the QC's VR_Melee_WallMark; a prop's hard hit, sendMeleeMark).
enum class MeleeMark
{
    Blunt, // a round dent: the crowbar, a gun, a pommel, Mjolnir, a club, a fist holding a prop, a thrown prop
    Slash, // a long groove along the swing: a sword's blade, the chainsaw's bar
    Stab,  // a short one: a blade's point driven in
    Chop   // a shorter, wider and deeper one: an axe's head
};

// A melee blow's mark at `where` on the surface of normal `normal`: a dent, or a blade's groove along `line` (on the
// surface; none or straight in: any way), both bigger and deeper with the blow's `damage` (as its square root, 1 at 20).
bool meleeMark(MeleeMark kind, const glm::vec3& where, const glm::vec3& normal, const glm::vec3& line, float damage)
{
    const float grows = za::clamp(za::sqrt(za::max(damage, 1.f) / 20.f), 0.6f, 1.8f);
    MarkOptions o;
    if(kind == MeleeMark::Blunt)
    {
        // Shallow and round (a pellet's pit is 0.9 deep, 6.5 wide): its radius 0.48 of the mark's half size.
        const float size = (12.f + random(-1.f, 1.f)) * grows * za::clamp(vr_decal_melee_blunt_size.value, 0.5f, 2.f);
        const float depth = 0.6f * grows * za::clamp(vr_decal_melee_blunt_depth.value, 0.f, 4.f);
        return add(Scuff, where, normal, size, o, glm::vec4{depth, 0.48f, 0.25f, 2.f * 3.f});
    }
    // The groove 0.6 of the mark's width across and of its length along, its length over its width the mark's aspect.
    const float width = (kind == MeleeMark::Chop ? 5.f : 3.6f) * za::sqrt(grows) * za::clamp(vr_decal_melee_slash_width.value, 0.5f, 2.f);
    const float length = (kind == MeleeMark::Stab ? 7.f : kind == MeleeMark::Chop ? 15.f : 26.f) * grows *
                         za::clamp(vr_decal_melee_slash_length.value, 0.5f, 2.f) * random(0.85f, 1.15f);
    const float depth = (kind == MeleeMark::Chop ? 1.1f : kind == MeleeMark::Stab ? 1.f : 0.8f) * grows *
                        za::clamp(vr_decal_melee_slash_depth.value, 0.f, 4.f);
    o.along = line;
    o.aspect = za::max(1.f, length / width);
    return add(Scuff, where, normal, width, o, glm::vec4{depth, 0.6f, 0.2f, 2.f * 4.f});
}

// A nail's hit at `pos` from Dissolution of Eternity's lava nailguns: a lava nail (progs/lspike.mdl) seen near it in
// the last updates (the nail goes in the message that brings its hit).
[[nodiscard]] bool lavaNailNear(const glm::vec3& pos)
{
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(e.model && e.msgtime >= cl.mtime[1] - 0.001 && !q_strcasecmp(e.model->name, "progs/lspike.mdl") &&
            glm::distance(glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, pos) < 160.f)
        {
            return true;
        }
    }
    return false;
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

// A ragdoll's limbs flung (limbTrail), by entity and limb: where each end was last drawn.
struct Limb
{
    glm::vec3 at{0.f};
    double seen = 0.0;
    float sinceTrail = 0.f, sinceDrop = 0.f;
    int dropsLeft = 0;
};
ankerl::unordered_dense::map<int, Limb> limbs;
int limbsPrunedFrame = 0;
constexpr int limbDrops = 6;           // drops a limb leaves in all
constexpr float limbBleedSpeed = 150.f; // units/s: a limb flung faster than this bleeds a trail

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
        (void)impactMark(Impact::Pellet, where, normal);
    }
}

bool meleeEvent(const glm::vec3& org, const glm::vec3& dir, int preset, int count)
{
    if(preset != EventMeleeMark)
    {
        return false;
    }
    if(!vr_decals.value || !vr_decal_melee.value || !vr_decal_carve.value || !cl.worldmodel)
    {
        return true; // (the blow's Quake effect leaves its pit, as before)
    }
    const int kind = za::clamp(count / 1000, 0, 3);
    const float damage = static_cast<float>(count % 1000) * 0.25f;
    const glm::vec3 d = glm::dot(dir, dir) > 1e-4f ? glm::normalize(dir) : glm::vec3{0.f};
    glm::vec3 where, normal;
    if(!nearest(org, 8.f, kind == 0 ? d : glm::vec3{0.f}, where, normal) || meleeMarkNear(where, 6.f, 0.15))
    {
        return true;
    }
    recentMelee[recentMeleeNext] = {where, cl.time};
    recentMeleeNext = (recentMeleeNext + 1) % recentMeleeCount;
    (void)meleeMark(static_cast<MeleeMark>(kind), where, normal, kind == 0 ? glm::vec3{0.f} : d, damage);
    return true;
}

void propHit(const glm::vec3& at, const glm::vec3& normal, float speed, float mass)
{
    const float least = vr_decal_melee_prop_speed.value;
    if(least <= 0.f || speed < least || !sv.active || sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return;
    }
    // As a blow's damage: 20 for a 10 kg crate at 10 m/s, with the speed and the weight's cube root.
    const float damage = 20.f * (speed / 10.f) * za::clamp(za::pow(za::max(mass, 0.1f) / 10.f, 1.f / 3.f), 0.5f, 1.6f);
    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_PARTICLE2);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i] + normal[i] * 2.f, sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-127, static_cast<int>(-normal[i] * 7.f * 16.f), 127));
    }
    MSG_WriteByte(&sv.datagram, EventMeleeMark);
    MSG_WriteShort(&sv.datagram, CLAMP(1, static_cast<int>(damage * 4.f + 0.5f), 999)); // (kind 0: a dent)
    VR_BroadcastMessageEnd();
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
        case Preset::BulletPuff: // a chip where it hit (the QC's: a melee blow, a stuck axe, the grapple)
        case Preset::Sparks:     // a melee blow on a wall
            impactAt(Impact::Blow, org, dir);
            break;
        case Preset::Explosion: impactAt(Impact::Blast, org, glm::vec3{0.f}); break;
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

// The marks faded out (over their last five seconds) gone; vr_decal_life (5 at least).
double expire()
{
    const double life = za::max(5.f, vr_decal_life.value);
    while(!decals.empty() && cl.time - decals.front().born > life)
    {
        bakeEarly(decals.front()); // (the permanent layer off: kept for when it is on)
        popOldest();
    }
    return life;
}

// ---- On the world's own surfaces (vr_decals_world) ---------------------------------------------

// Drawn in the world's shader (QVR_DECAL_FUNCTIONS, vr_glsl.h), with or without retro textures, not as meshes over it.
[[nodiscard]] bool onWorld()
{
    return vr_decals_world.value != 0.f;
}

// What the shader reads (vr_glsl.h's Decal): one a mark, in the ring's order.
struct WorldDecal
{
    glm::vec4 centre; // xyz, w its cell
    glm::vec4 u;      // a unit, w half its length
    glm::vec4 v;      // a unit, w half its width
    glm::vec4 n;      // its surface's normal, w its depth
    glm::vec4 time;   // when it shows (on worldClock's), grow, growFrom, darken (+2: from its start)
    glm::vec4 carve;  // Decal::carve
};
static_assert(sizeof(WorldDecal) == 96);
static_assert(cellsPerRow == 8 && cellRows == 4 && atlasWidth == 2048 && atlasHeight == 1024,
    "QVR_DECAL_FUNCTIONS (vr_glsl.h) has the atlas's layout");

// The grid they are found in (DecalGrid): cells of 32 units, hashed into buckets as the shader hashes them. A mark is
// listed in the buckets of the cells that reach its rectangle (at its full size, through the surface along its normal:
// where the shader can find a pixel in it), each once (its memberships: Decal::buckets).
constexpr float worldCell = 32.f;
// A bucket lists at most vr_decal_bucket_max marks (32 shipped; 255 at most: its header's length, 8 bits). Past that a
// mark is left out of every bucket it is in, not only the full one: newest first, a mark is listed only if each of its
// buckets has room. A pile of more marks than that on one cell loses its oldest whole, as the ring loses its oldest;
// never a cell-shaped hole in an older mark under newer ones (2026-10-11: the newest 64 of each bucket left square
// holes in the pools under a pile of gibs'). Each pixel of the world walks its bucket's whole list: a pile of 255 cost
// the frame rate where it was looked at (2026-10-12: vr_decal_bucket_max).
constexpr za::U32 worldBucketMarksMax = 255;
za::U32 worldBucketMarks = worldBucketMarksMax; // the limit the last grid made was listed under

[[nodiscard]] za::U32 worldBucketLimit()
{
    return static_cast<za::U32>(za::clamp(static_cast<int>(vr_decal_bucket_max.value), 1, static_cast<int>(worldBucketMarksMax)));
}
// The cells looked at: those of a mark's box, and as far again as the parallax mapping can move a pixel along a
// surface (3 times its depth, vr_parallax_depth up to 4); listed, those that reach its rectangle.
constexpr float worldReach = 12.f;

[[nodiscard]] za::U32 worldCellHash(int x, int y, int z)
{
    return (static_cast<za::U32>(x) * 73856093u) ^ (static_cast<za::U32>(y) * 19349663u) ^
           (static_cast<za::U32>(z) * 83492791u);
}

za::Vector<WorldDecal> worldDecals;
za::Vector<za::U32> worldGrid;
// Reused scratch: stamp each bucket once per mark (its memberships kept in the mark: Decal::buckets).
za::Vector<za::U32> worldBucketStamp;
za::U32 worldStamp = 0;
// Reused scratch: whether each mark was listed in the last grid made (each of its buckets had room), by ring order.
za::Vector<za::U8> worldMarkListed;
za::U32 worldLeftOut = 0; // marks left out of the last grid made (a bucket of theirs full)
gfx::StorageBuffer worldDecalBuffer, worldGridBuffer;
double worldClock = 0.0; // cl.time the marks' times count from (the floats near 0)
long long worldBuilds = 0;
int worldFrame = -1; // the host frame of the last view that drew the marks on the world (VR_DecalsFrame: its first view makes the grid)

// Mark `d`'s buckets in first-cell order, each once, into `out`: those of the cells reaching its rectangle. The stamp
// table matches mask + 1.
void worldBuckets(const WorldDecal& d, za::U32 mask, za::Vector<za::U32>& out)
{
    if(++worldStamp == 0)
    {
        // Reusing generation 1 must not mistake an old stamp for this mark's membership.
        worldBucketStamp.assign(worldBucketStamp.size(), 0u);
        worldStamp = 1;
    }
    const glm::vec3 c{d.centre};
    const glm::vec3 extent = glm::abs(glm::vec3{d.u}) * d.u.w + glm::abs(glm::vec3{d.v}) * d.v.w +
                             glm::abs(glm::vec3{d.n}) * d.n.w + glm::vec3{worldReach};
    const glm::ivec3 lo{glm::floor((c - extent) * (1.f / worldCell))};
    const glm::ivec3 hi{glm::floor((c + extent) * (1.f / worldCell))};
    // A cell reaches the rectangle unless its box (a little larger: the shader's rounding) projected on the
    // rectangle's length or width way misses it (the shader's own early-out, with room to spare; the projection
    // along the other ways only lets more cells in).
    const glm::vec3 u{d.u}, v{d.v};
    const float half = worldCell * 0.5f + 0.25f;
    const float reachU = d.u.w * 1.002f + 0.05f + half * (za::fabs(u.x) + za::fabs(u.y) + za::fabs(u.z));
    const float reachV = d.v.w * 1.002f + 0.05f + half * (za::fabs(v.x) + za::fabs(v.y) + za::fabs(v.z));
    for(int z = lo.z; z <= hi.z; z++)
    {
        for(int y = lo.y; y <= hi.y; y++)
        {
            for(int x = lo.x; x <= hi.x; x++)
            {
                const glm::vec3 r =
                    glm::vec3{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f} *
                        worldCell -
                    c;
                if(za::fabs(glm::dot(r, u)) > reachU || za::fabs(glm::dot(r, v)) > reachV)
                {
                    continue;
                }
                const za::U32 b = worldCellHash(x, y, z) & mask;
                if(worldBucketStamp[b] != worldStamp)
                {
                    worldBucketStamp[b] = worldStamp;
                    out.pushBack(b);
                }
            }
        }
    }
}

// The marks and their grid made again and uploaded (when marks came or went).
void buildWorld()
{
    QVR_PROFILE("decals on the world");
    worldBuilds++;
    worldClock = cl.time;
    {
        QVR_PROFILE("decal records");
        worldDecals.clear();
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            const Decal& d = decals[k];
            const float hu = glm::length(d.u), hv = glm::length(d.v);
            WorldDecal w;
            w.centre = {d.centre, static_cast<float>(d.cell)};
            w.u = {hu > 0.f ? d.u / hu : glm::vec3{0.f}, za::max(hu, 1e-3f)};
            w.v = {hv > 0.f ? d.v / hv : glm::vec3{0.f}, za::max(hv, 1e-3f)};
            w.n = {d.normal, d.depth};
            w.time = {static_cast<float>(d.born - worldClock), d.grow, d.growFrom, d.darken + (d.fromStart ? 2.f : 0.f)};
            w.carve = d.carve;
            worldDecals.pushBack(w);
        }
    }

    // Buckets: twice the cells listed (at least 256), a power of two.
    za::SizeT cells = 0;
    {
        QVR_PROFILE("decal grid sizing");
        for(const WorldDecal& w : worldDecals)
        {
            const glm::vec3 extent = glm::abs(glm::vec3{w.u}) * w.u.w + glm::abs(glm::vec3{w.v}) * w.v.w +
                                     glm::abs(glm::vec3{w.n}) * w.n.w + glm::vec3{worldReach};
            const glm::vec3 span = glm::floor(extent * (2.f / worldCell)) + 2.f;
            cells += static_cast<za::SizeT>(span.x * span.y * span.z);
        }
    }
    za::U32 buckets = 256;
    while(buckets < 2 * cells && buckets < (1u << 20))
    {
        buckets <<= 1;
    }
    const za::U32 mask = buckets - 1;
    {
        QVR_PROFILE("decal grid count");
        // Newest first, a mark is listed if each of its buckets has room (its header counts the marks listed in it,
        // then holds its list's start and length: the prefix pass): in all of them, or none.
        worldGrid.assign(1 + buckets, 0u);
        if(worldBucketStamp.size() != buckets)
        {
            worldBucketStamp.assign(buckets, 0u);
            worldStamp = 0;
        }
        worldMarkListed.assign(decals.size(), za::U8{0});
        worldLeftOut = 0;
        worldBucketMarks = worldBucketLimit();
        for(za::SizeT k = decals.size(); k-- > 0;)
        {
            Decal& d = decals[k];
            if(d.bucketsMask != mask) // (new, or the grid's size changed)
            {
                d.buckets.clear();
                worldBuckets(worldDecals[k], mask, d.buckets);
                d.bucketsMask = mask;
            }
            bool room = true;
            for(const za::U32 b : d.buckets)
            {
                room = room && worldGrid[1 + b] < worldBucketMarks;
            }
            if(!room)
            {
                worldLeftOut++;
                continue;
            }
            worldMarkListed[k] = 1;
            for(const za::U32 b : d.buckets)
            {
                worldGrid[1 + b]++;
            }
        }
    }
    // Each bucket's list: the marks listed in it, newest first.
    za::U32 total = 0;
    {
        QVR_PROFILE("decal grid prefix");
        worldGrid[0] = mask;
        for(za::U32 b = 0; b < buckets; b++)
        {
            const za::U32 n = worldGrid[1 + b];
            worldGrid[1 + b] = total << 8; // (its length counted up as the fill lists its marks)
            total += n;
        }
        worldGrid.resize(1 + buckets + total, 0u);
    }
    {
        QVR_PROFILE("decal grid fill");
        // Newest first again: a bucket's header's length grows with each mark listed, to the length the prefix pass
        // gave its list (vr_decal_bucket_max at most, 255: its 8 bits).
        for(za::SizeT k = worldDecals.size(); k-- > 0;)
        {
            if(!worldMarkListed[k])
            {
                continue;
            }
            for(const za::U32 b : decals[k].buckets)
            {
                za::U32& header = worldGrid[1 + b];
                worldGrid[1 + buckets + (header >> 8) + (header & 255u)] = static_cast<za::U32>(k);
                header++;
            }
        }
    }
    QVR_PROFILE("decal buffers upload");
    gfx::upload(worldDecalBuffer, worldDecals.data(), worldDecals.size() * sizeof(WorldDecal));
    gfx::upload(worldGridBuffer, worldGrid.data(), worldGrid.size() * sizeof(za::U32));
}

int permFrame = -1;  // the host frame bakeSettled last ran in (VR_DecalsFrame's first view: the main thread)
int layerShown = -1; // the permanent layer shown (1) or kept off (0) at the last frame with vr_decals on (-1: not yet)
za::Vector<za::U8> bakedNow; // (bakeSettled's: the marks gone from the ring this frame, by ring order)

// With the permanent layer (vr_permdecals), the marks that have settled (shown, spread, darkened; a tenth of a second
// at least, for a hit's second effect to find its first: add; 8 s for blood that darkens as it dries) baked into it and
// gone from the ring, so that the live cap no longer limits what stays: those on the static world (those on the boxes
// made in the game stay live: theirs go with them, removeOn). The oldest first, vr_permdecal_bake_max pages a frame at
// most (the others stay live a frame or more). Those shown from it while it was off (fromLayer) leave the ring. Then
// what was baked drawn into the layer's atlas. Once a frame, before the world draws. The layer switched off: its newest
// marks shown live (showRecord); on again: what was kept while off baked (rebake).
void bakeSettled()
{
    if(permFrame == host_framecount)
    {
        return;
    }
    permFrame = host_framecount;
    if(!cl.worldmodel)
    {
        return;
    }
    const int restored = permdecals::restore(); // (a saved game's marks, once its map has loaded)
    if(restored != 0 && !atlas)
    {
        makeAtlas();
    }
    if(vr_decals.value)
    {
        const bool shown = permdecals::on(), kept = permdecals::kept();
        if(kept && (layerShown == 1 || restored == 2))
        {
            showRecord();
        }
        if(shown)
        {
            (void)permdecals::rebake();
            (void)permdecals::bakeDue(); // (the brush models' marks shown by now)
        }
        if(shown || kept)
        {
            layerShown = shown ? 1 : 0;
        }
    }
    if(vr_decals.value && permdecals::on() && !decals.empty())
    {
        QVR_PROFILE("decals baked");
        if(!atlas)
        {
            makeAtlas();
        }
        const int budget = za::max(1, static_cast<int>(vr_permdecal_bake_max.value));
        const za::SizeT n = decals.size();
        bakedNow.assign(n, 0);
        za::SizeT gone = 0;
        // Carved marks (vr_decal_carve) stay live, the newest vr_decal_carve_max of them (the layer keeps no depth), until
        // they are older or would start to fade out (vr_decal_life): then baked flat as the others.
        const bool carving = vr_decal_carve.value != 0.f && onWorld();
        const int carveMax = za::max(0, static_cast<int>(vr_decal_carve_max.value));
        int carvedLeft = 0; // the carved marks from here on, this one too
        if(carving)
        {
            for(za::SizeT i = 0; i < n; i++)
            {
                carvedLeft += decals[i].carve.x > 0.f && !decals[i].fromLayer ? 1 : 0;
            }
        }
        const double carveUntil = static_cast<double>(za::max(10.f, vr_decal_life.value)) - 6.0;
        for(za::SizeT i = 0; i < n; i++)
        {
            Decal& d = decals[i];
            if(d.fromLayer)
            {
                bakedNow[i] = 1; // (in the layer already)
                gone++;
                continue;
            }
            const bool carved = carving && d.carve.x > 0.f;
            if(carved && carvedLeft-- <= carveMax && cl.time - d.born < carveUntil)
            {
                continue;
            }
            const double settled =
                d.born + za::max(0.1f, d.grow, d.darken > 0.f ? za::max(d.grow, 8.f) : 0.f);
            if(d.brush == 0 && !d.permTried && cl.time >= settled && permdecals::pendingPages() < budget)
            {
                if(permdecals::bakeWorld(stampOf(d)) > 0)
                {
                    bakedNow[i] = 1;
                    gone++;
                    continue;
                }
                d.permTried = true; // (no face of the layer under it: it stays live)
            }
        }
        // Kept marks moved back over the gone ones (in order), the gone then off the front.
        if(gone > 0)
        {
            za::SizeT firstKept = n;
            for(za::SizeT i = n; i-- > 0;)
            {
                if(bakedNow[i])
                {
                    unsettle(decals[i]);
                    dropCount -= isDrop(decals[i]);
                    continue;
                }
                firstKept--;
                if(firstKept != i)
                {
                    za::genericSwap(decals[firstKept], decals[i]);
                }
            }
            for(za::SizeT i = 0; i < firstKept; i++)
            {
                decals.popFront();
            }
            worldDirty = true;
        }
    }
    if(atlas)
    {
        permdecals::flush(atlas);
    }
}

// The marks on brush entities, where each is drawn now (vr_decal_entities): the ones whose entity is gone or has
// another model dropped.
void drawEntityDecals()
{
    for(za::SizeT k = 0; k < entityDecals.size();)
    {
        const EntityDecal& m = entityDecals[k];
        const bool gone = m.ent <= 0 || m.ent >= cl.num_entities || cl_entities[m.ent].model != m.model;
        if(gone)
        {
            entityDecals.eraseAt(k);
            continue;
        }
        k++;
    }
    if(entityDecals.empty())
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }
    // The layer on: they stay (as the baked marks), to the cap; off, they fade as the live marks.
    const double life = permdecals::on() ? 1e9 : za::max(5.f, vr_decal_life.value);
    entityVertices.clear();
    for(za::SizeT k = 0; k < entityDecals.size(); k++)
    {
        const EntityDecal& m = entityDecals[k];
        entity_t& e = cl_entities[m.ent];
        float mat[16];
        VR_BModelMatrix(&e, mat);
        const glm::mat4 toWorld = glm::make_mat4(mat);
        entityLocal.clear();
        appendDecal(m.d, life, entityLocal);
        for(gfx::Vertex& v : entityLocal)
        {
            v.pos = glm::vec3{toWorld * glm::vec4{v.pos, 1.f}};
            entityVertices.pushBack(v);
        }
    }
    if(!entityVertices.empty())
    {
        const gfx::State state{.shade = gfx::Shade::Texture, .blend = gfx::Blend::Modulate, .depthTest = true,
            .depthWrite = false, .retro = retro::categorySet(retro::Category::Decals)};
        gfx::draw(entityVertices, gfx::sceneViewProjection(), state, atlas);
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
    drawEntityDecals(); // (not in the world's shader: theirs move)
    if(decals.empty() || onWorld())
    {
        return; // (on the world: in its shader, QVR_WORLD_FS_DECALS)
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
        builtFrame = host_framecount;

        const double life = expire();

        vertices.clear();
        int settledVertices = 0;
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            Decal& d = decals[k];
            // When it last changes: shown, spread, darkened; then its fading. (Settled, it is drawn the same each
            // frame: appendDecal's alpha is 1 and its spreading and darkening done.)
            const double settled = d.born + za::max(0.f, d.grow, d.darken > 0.f ? za::max(d.grow, 8.f) : 0.f);
            const double fades = d.born + life - 5.0;
            if(cl.time >= settled && cl.time < fades)
            {
                if(d.staticAt < 0)
                {
                    d.staticAt = static_cast<int>(staticVertices.size());
                    appendDecal(d, life, staticVertices);
                    staticTouched(d.staticAt, static_cast<int>(staticVertices.size()));
                }
                settledVertices += static_cast<int>(d.tris.size());
            }
            else
            {
                unsettle(d);
                appendDecal(d, life, vertices);
            }
        }

        // Packed when more of it is gone than is left: the settled marks' triangles again, in their order.
        if(staticGone > za::max(4096, settledVertices))
        {
            za::Vector<gfx::Vertex> packed;
            packed.reserve(static_cast<za::SizeT>(settledVertices));
            for(za::SizeT k = 0; k < decals.size(); k++)
            {
                Decal& d = decals[k];
                if(d.staticAt >= 0)
                {
                    const int at = static_cast<int>(packed.size());
                    for(za::SizeT i = 0; i < d.tris.size(); i++)
                    {
                        packed.pushBack(staticVertices[static_cast<za::SizeT>(d.staticAt) + i]);
                    }
                    d.staticAt = at;
                }
            }
            staticVertices.swap(packed);
            staticGone = 0;
            staticResend = true;
        }
    }
    const gfx::State state{.shade = gfx::Shade::Texture, .blend = gfx::Blend::Modulate, .depthTest = true,
        .depthWrite = false, .retro = retro::categorySet(retro::Category::Decals)}; // retro textures (vr_retro.hpp)
    if(staticResend || staticVertices.size() * sizeof(gfx::Vertex) > staticTriangles.capacity)
    {
        gfx::upload(staticTriangles, staticVertices);
    }
    else
    {
        for(const VertexRange& r : staticChanged)
        {
            gfx::update(staticTriangles, staticVertices, static_cast<za::SizeT>(r.first),
                static_cast<za::SizeT>(r.end - r.first));
        }
    }
    staticResend = false;
    staticChanged.clear();
    gfx::draw(staticTriangles, gfx::sceneViewProjection(), state, atlas);
    if(!vertices.empty())
    {
        gfx::draw(vertices, gfx::sceneViewProjection(), state, atlas);
    }
}

// What vr_decal_stress lays: splatters on the floor (its default), spreading pools on the floor, splatters on the wall
// ahead, runs spreading down it (every kind of the world's shader's paths, laid the same way each time: tests compare
// the images of two builds); pools stacked on a spot 96 units ahead (each 0.5 to 1 times the size, as gibs' pools
// piled up: decal_stack_test.sh), or pools and splatters in turn clustered round it (0.4 to 1 times the size, up to
// the size away: a gib's burst).
enum class Stress
{
    Splatter,
    Pool,
    Wall,
    Run,
    Stack,
    Cluster
};

// `count` marks of `kind` on the floor ahead of the player (or the wall ahead), `size` units across
// (vr_decal_stress): how many were made.
int stressMarks(int count, float size, Stress kind)
{
    vec3_t forward, right, up;
    AngleVectors(cl.viewangles, forward, right, up);
    const glm::vec3 f{forward[0], forward[1], 0.f}, r{right[0], right[1], 0.f};
    const auto& e = cl_entities[cl.viewentity];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    const bool wall = kind == Stress::Wall || kind == Stress::Run;
    const bool pile = kind == Stress::Stack || kind == Stress::Cluster;
    // The marks' turns and atlas cells (rand) the same each time: what else draws from it (as many times as frames
    // came, or a chip somewhere) leaves them as they were
    srand(stressSerial * 2654435761u + 1u);
    int made = 0;
    for(int i = 0; i < count; i++)
    {
        // Permute the 64 x 64 lattice so that the newest marks cover the whole area (on a wall: 6 units apart across
        // it, 2 up it from 32 below the eyes).
        const unsigned k = (stressSerial++ * 109u) & 4095u;
        const glm::vec3 across = r * (6.f * (static_cast<float>(k >> 6u) - 31.5f));
        glm::vec3 from = wall ? origin + across + glm::vec3{0.f, 0.f, 2.f * static_cast<float>(k & 63u) - 32.f}
                              : origin + f * (64.f + 6.f * static_cast<float>(k & 63u)) + across +
                                    glm::vec3{0.f, 0.f, 24.f};
        float markSize = size;
        if(pile)
        {
            // (stacked: a few units apart, round the spot, as add takes one mark a place in a tenth of a second)
            const bool stack = kind == Stress::Stack;
            const float turn = static_cast<float>(i) * 2.39996f, out = 2.5f + 0.25f * static_cast<float>(i);
            const float a = stack ? out * za::cos(turn) : random(-1.f, 1.f) * size;
            const float b = stack ? out * za::sin(turn) : random(-1.f, 1.f) * size;
            from = origin + glm::normalize(f) * (96.f + a) + glm::normalize(r) * b + glm::vec3{0.f, 0.f, 24.f};
            markSize = size * (stack ? random(0.5f, 1.f) : random(0.4f, 1.f));
        }
        const glm::vec3 to = wall ? from + glm::normalize(f) * 2048.f : from - glm::vec3{0.f, 0.f, 2048.f};
        glm::vec3 where, normal;
        float fraction;
        if(!hitWorld(from, to, where, normal, fraction))
        {
            continue;
        }
        MarkOptions o;
        Mark mark = Mark::Splatter;
        if(kind == Stress::Pool || kind == Stress::Stack || (kind == Stress::Cluster && (i & 1)))
        {
            mark = Mark::Pool;
            o.grow = 6.f;
            o.growFrom = 0.12f;
            o.darken = 0.5f;
        }
        else if(kind == Stress::Run)
        {
            mark = Mark::Streak;
            o.along = glm::vec3{0.f, 0.f, -1.f};
            o.aspect = 4.f;
            o.grow = 3.f;
            o.growFrom = 0.15f;
            o.fromStart = true;
            o.darken = 0.3f;
        }
        made += place(mark, where, normal, markSize, o);
    }
    return made;
}

void carveTest_f()
{
    if(!cl.worldmodel)
    {
        Con_Printf("vr_decal_carve_test: enter a map first\n");
        return;
    }
    // The head (in VR; else the view as last drawn).
    const hands::State& hs = hands::current();
    glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]}, f, r, u;
    if(hs.valid)
    {
        eye = hs.head;
        hands::angleVectors(hs.headAngles, f, r, u);
    }
    else
    {
        vec3_t forward, right, up;
        AngleVectors(r_refdef.viewangles, forward, right, up);
        f = {forward[0], forward[1], forward[2]};
    }
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";

    // The view moved to look at the last marks laid: head-on from `dist` along their surface's normal, or grazing (15
    // degrees over it, from their left), through setpos (and the mock headset's look, vr_mock_look).
    if(!q_strcasecmp(what, "look"))
    {
        if(glm::dot(carveTestNormal, carveTestNormal) == 0.f || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
        {
            Con_Printf("vr_decal_carve_test look head|graze [dist]: lay some first\n");
            return;
        }
        const bool graze = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "graze");
        const float dist = Cmd_Argc() > 3 ? za::clamp(Q_atof(Cmd_Argv(3)), 8.f, 1024.f) : 40.f;
        const glm::vec3 n = carveTestNormal, side = carveTestAcross;
        constexpr float s15 = 0.258819f, c15 = 0.965926f;
        // (head-on: 10 degrees from the normal, so that a floor's view keeps its yaw)
        const glm::vec3 to = carveTestCentre + (graze ? n * s15 - side * c15 : glm::normalize(n - side * 0.176f)) * dist;
        const glm::vec3 d = glm::normalize(carveTestCentre - to);
        const float yaw = za::atan2(d.y, d.x) * (180.f / 3.14159265f);
        const float pitch = -za::atan2(d.z, glm::length(glm::vec2{d.x, d.y})) * (180.f / 3.14159265f);
        const auto& e = cl_entities[cl.viewentity];
        const glm::vec3 o = to - (eye - glm::vec3{e.origin[0], e.origin[1], e.origin[2]}); // (the eye's height over it)
        Cbuf_InsertText(va("vr_mock_look %.2f 0; setpos %.2f %.2f %.2f 0 %.2f 0\n", pitch, o.x, o.y, o.z, yaw));
        return;
    }

    const bool melee = !q_strcasecmp(what, "melee"); // (the melee marks: vr_decal_melee)
    const int lattice = melee ? 0 : za::clamp(atoi(what), 0, 64);
    glm::vec3 centre, n;
    float fraction;
    if(!hitWorld(eye, eye + f * 1024.f, centre, n, fraction))
    {
        Con_Printf("vr_decal_carve_test: no surface ahead\n");
        return;
    }
    // On the surface: `up` its upward way (a floor's: away from the eye), `across` to the right.
    glm::vec3 upward = za::fabs(n.z) < 0.7f ? glm::vec3{0.f, 0.f, 1.f} : f;
    upward = glm::normalize(upward - n * glm::dot(upward, n));
    const glm::vec3 across = glm::cross(upward, n);
    carveTestCentre = centre;
    carveTestNormal = n;
    carveTestAcross = across;
    srand(carveTestSerial * 2654435761u + 7u);
    const auto at = [&](Impact kind, float a, float b) {
        const glm::vec3 p = centre + across * a + upward * b;
        glm::vec3 where, normal;
        float fr;
        return hitWorld(p + n * 8.f, p - n * 16.f, where, normal, fr) && impactMark(kind, where, normal);
    };
    int made = 0;
    if(melee)
    {
        // (below)
    }
    else if(!q_strcasecmp(what, "blast"))
    {
        made += at(Impact::Blast, 0.f, 0.f); // (one blast's crater in the middle)
    }
    else if(lattice > 0)
    {
        // A lattice 12 units apart, 16 across, pellets, nails and super nails in turn, continued by the next call (each
        // call 64 at most: add's limit a frame).
        constexpr Impact kinds[] = {Impact::Pellet, Impact::Nail, Impact::SuperNail};
        for(int i = 0; i < lattice; i++, carveTestSerial++)
        {
            const float a = 12.f * (static_cast<float>(carveTestSerial % 16u) - 7.5f);
            const float b = 12.f * (static_cast<float>(carveTestSerial / 16u) - 6.f);
            made += at(kinds[carveTestSerial % 3u], a, b);
        }
    }
    else
    {
        // Pellets in a row, nails and super nails under them, lava nails under those, a blast's crater to the right.
        for(int i = 0; i < 5; i++)
        {
            made += at(Impact::Pellet, -36.f + 9.f * static_cast<float>(i), 14.f);
        }
        for(int i = 0; i < 3; i++)
        {
            made += at(Impact::Nail, -36.f + 10.f * static_cast<float>(i), 0.f);
        }
        for(int i = 0; i < 2; i++)
        {
            made += at(Impact::SuperNail, -4.f + 12.f * static_cast<float>(i), 0.f);
        }
        made += at(Impact::LavaNail, -32.f, -14.f);
        made += at(Impact::LavaSuperNail, -16.f, -14.f);
        made += at(Impact::Blast, 40.f, 0.f);
    }
    if(melee)
    {
        const auto blow = [&](MeleeMark kind, float a, float b, const glm::vec3& line, float damage) {
            const glm::vec3 p = centre + across * a + upward * b;
            glm::vec3 where, normal;
            float fr;
            return hitWorld(p + n * 8.f, p - n * 16.f, where, normal, fr) && meleeMark(kind, where, normal, line, damage);
        };
        const char* only = Cmd_Argc() > 2 ? Cmd_Argv(2) : "";
        const int count = za::clamp(atoi(only), 0, 64);
        if(!q_strcasecmp(only, "blunt"))
        {
            // Dents of 10, 20 and 40 damage in a row (close-ups).
            for(int i = 0; i < 3; i++)
            {
                made += blow(MeleeMark::Blunt, -14.f + 13.f * static_cast<float>(i) + 2.f * static_cast<float>(i * i), 0.f,
                    glm::vec3{0.f}, 10.f * static_cast<float>(1 << i));
            }
        }
        else if(!q_strcasecmp(only, "slash"))
        {
            made += blow(MeleeMark::Slash, 0.f, 0.f, across + upward * 0.3f, 20.f); // one slash (close-ups)
        }
        else if(count > 0)
        {
            // A lattice of dents and slashes 20 units apart, 8 across, continued by the next call.
            for(int i = 0; i < count; i++, carveTestSerial++)
            {
                const float a = 20.f * (static_cast<float>(carveTestSerial % 8u) - 3.5f);
                const float b = 20.f * (static_cast<float>(carveTestSerial / 8u % 8u) - 3.5f);
                made += carveTestSerial % 2u ? blow(MeleeMark::Slash, a, b, across + upward * 0.4f, 20.f) :
                                               blow(MeleeMark::Blunt, a, b, glm::vec3{0.f}, 20.f);
            }
        }
        else
        {
            // Dents of 10, 20 and 40 damage over a slash across (20), a slanting one and an axe's chop (30) under it, a
            // stab and a slash up at the sides.
            for(int i = 0; i < 3; i++)
            {
                made += blow(MeleeMark::Blunt, -16.f + 15.f * static_cast<float>(i) + 2.f * static_cast<float>(i * i), 14.f,
                    glm::vec3{0.f}, 10.f * static_cast<float>(1 << i));
            }
            made += blow(MeleeMark::Slash, 0.f, 0.f, across, 20.f);
            made += blow(MeleeMark::Slash, -10.f, -14.f, across + upward, 20.f);
            made += blow(MeleeMark::Chop, 12.f, -14.f, across, 30.f);
            made += blow(MeleeMark::Stab, -26.f, 2.f, upward, 20.f);
            made += blow(MeleeMark::Slash, 28.f, -4.f, upward, 20.f);
        }
    }
    Con_Printf("vr_decal_carve_test: %d marks laid round %.1f %.1f %.1f (normal %.2f %.2f %.2f), %.0f units ahead of %.1f %.1f "
               "%.1f along %.2f %.2f %.2f\n",
        made, centre.x, centre.y, centre.z, n.x, n.y, n.z, fraction * 1024.f, eye.x, eye.y, eye.z, f.x, f.y, f.z);
}

void stress_f()
{
    if(!cl.worldmodel || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        Con_Printf("vr_decal_stress: enter a map first\n");
        return;
    }
    const int count = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 1, 64) : 64;
    const float size = Cmd_Argc() > 2 ? za::clamp(Q_atof(Cmd_Argv(2)), 1.f, 256.f) : 64.f;
    const char* name = Cmd_Argc() > 3 ? Cmd_Argv(3) : "splatter";
    Stress kind = Stress::Splatter;
    if(!q_strcasecmp(name, "pool"))
    {
        kind = Stress::Pool;
    }
    else if(!q_strcasecmp(name, "wall"))
    {
        kind = Stress::Wall;
    }
    else if(!q_strcasecmp(name, "run"))
    {
        kind = Stress::Run;
    }
    else if(!q_strcasecmp(name, "stack"))
    {
        kind = Stress::Stack;
    }
    else if(!q_strcasecmp(name, "cluster"))
    {
        kind = Stress::Cluster;
    }
    else if(q_strcasecmp(name, "splatter"))
    {
        Con_Printf("vr_decal_stress [count] [size] [splatter|pool|wall|run|stack|cluster]: unknown kind %s\n", name);
        return;
    }
    const int made = stressMarks(count, size, kind);
    Con_DPrintf("vr_decal_stress: %d of %d marks, size %.0f, %s\n", made, count, size, name);
}

void at_f()
{
    if(Cmd_Argc() < 7 || !cl.worldmodel)
    {
        Con_Printf("vr_decal_at x y z dx dy dz [blood|hole|scorch|pool] [size] [delay]: a mark where the line meets a surface "
                   "(shown `delay` seconds later, as the gore's flying blood)\n");
        return;
    }
    const glm::vec3 from{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const glm::vec3 dir{Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6))};
    const char* name = Cmd_Argc() > 7 ? Cmd_Argv(7) : "blood";
    const Kind kind = !q_strcasecmp(name, "hole") ? Hole : !q_strcasecmp(name, "scorch") ? Scorch : !q_strcasecmp(name, "pool") ? Pool : Blood;
    const float size = Cmd_Argc() > 8 ? Q_atof(Cmd_Argv(8)) : 24.f;
    glm::vec3 where, normal;
    float f;
    if(glm::dot(dir, dir) < 1e-6f || !hitWorld(from, from + glm::normalize(dir) * 512.f, where, normal, f))
    {
        Con_Printf("vr_decal_at: nothing there\n");
        return;
    }
    // Its turn and cell from its arguments (rand: the same mark each run, whatever ran before it).
    unsigned seed = 2166136261u;
    for(const char* c = Cmd_Args(); c && *c; c++)
    {
        seed = (seed ^ static_cast<unsigned char>(*c)) * 16777619u;
    }
    srand(seed);
    MarkOptions o;
    o.delay = Cmd_Argc() > 9 ? za::max(0.f, static_cast<float>(Q_atof(Cmd_Argv(9)))) : 0.f;
    const bool made = add(kind, where, normal, size, o);
    int entity = 0;
    (void)worldtrace::world(from, from + glm::normalize(dir) * 512.f, true, true, 0, 0, &entity);
    Con_Printf("vr_decal_at: %s at %.1f %.1f %.1f (normal %.2f %.2f %.2f), entity %d (%s); %d marks on brush entities\n",
        made ? "laid" : "none", where.x, where.y, where.z, normal.x, normal.y, normal.z, entity,
        entity > 0 && cl_entities[entity].model ? cl_entities[entity].model->name : "-", static_cast<int>(entityDecals.size()));
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
    int showing = 0, onBrushes = 0, carved = 0, nailShaped = 0, craters = 0, dents = 0, grooves = 0;
    for(za::SizeT k = 0; k < decals.size(); k++)
    {
        const Decal& d = decals[k];
        showing += d.tris.empty() ? 0 : 1;
        onBrushes += !d.tris.empty() && d.brush != 0 ? 1 : 0;
        carved += d.carve.x > 0.f ? 1 : 0;
        nailShaped += d.carve.x > 0.f && d.carve.w >= 2.f && d.carve.w < 4.f ? 1 : 0;
        craters += d.carve.x > 0.f && d.carve.w >= 4.f && d.carve.w < 6.f ? 1 : 0;
        dents += d.carve.x > 0.f && d.carve.w >= 6.f && d.carve.w < 8.f ? 1 : 0;
        grooves += d.carve.x > 0.f && d.carve.w >= 8.f ? 1 : 0;
    }
    Con_Printf("%d decals: %d blood, %d drops, %d scorch, %d chips, %d splatters, %d streaks, %d pools, %d splotches, %d "
               "scuffs\n",
        static_cast<int>(decals.size()), kinds[Blood], kinds[BloodDrop], kinds[Scorch], kinds[Hole], kinds[Splatter],
        kinds[Streak], kinds[Pool], kinds[Splotch], kinds[Scuff]);
    Con_Printf("%d carved (vr_decal_carve %g, style %g): %d pits, %d nail holes, %d blast craters, %d melee dents, %d "
               "blade grooves\n",
        carved, vr_decal_carve.value, vr_decal_carve_style.value, carved - nailShaped - craters - dents - grooves,
        nailShaped, craters, dents, grooves);
    Con_Printf("decals laid: %d (%d on runtime brushes; the rest removed in place)\n", showing, onBrushes);
    Con_Printf("%d marks on brush entities (vr_decal_entities %g), %d vertices drawn for them last\n",
        static_cast<int>(entityDecals.size()), vr_decal_entities.value, static_cast<int>(entityVertices.size()));
    Con_Printf("%d vertices settled (%d of them left by marks gone), %d changing (made again each frame)\n",
        static_cast<int>(staticVertices.size()), staticGone, static_cast<int>(vertices.size()));
    Con_Printf("settled vertices uploaded %lld times so far, %.1f KB in all (%.1f KB a frame for both eyes if copied "
               "at each draw)\n",
        staticTriangles.uploads, static_cast<double>(staticTriangles.uploadedBytes) / 1024.0,
        static_cast<double>(staticVertices.size() * sizeof(gfx::Vertex) * 2) / 1024.0);
    if(onWorld())
    {
        Con_Printf("drawn in the world's shader, following its parallax (vr_decals_world 1; vr_parallax %g, depth %g, "
                   "texture heights %s): %d marks, %d grid entries, made %lld times so far\n",
            vr_parallax.value, vr_parallax_depth.value, TexMgr_IndexedSmooth() ? "on" : "only replacement textures' (vr_texture_smooth 2 for Quake's)",
            static_cast<int>(worldDecals.size()), static_cast<int>(worldGrid.size()), worldBuilds);
        za::U32 occupied = 0, full = 0, largest = 0, longest = 0;
        za::Vector<za::U32> counts; // each bucket's marks (all of them: the grid lists them while each of a mark's has room)
        counts.assign(worldGrid.empty() ? 0u : worldGrid[0] + 1u, 0u);
        for(za::SizeT k = 0; k < decals.size(); k++)
        {
            if(!worldGrid.empty() && decals[k].bucketsMask == worldGrid[0]) // (hashed for the last grid made)
            {
                for(const za::U32 b : decals[k].buckets)
                {
                    counts[b]++;
                }
            }
        }
        for(za::SizeT b = 0; b < counts.size(); b++)
        {
            occupied += counts[b] > 0;
            full += counts[b] > worldBucketMarks;
            largest = za::max(largest, counts[b]);
            longest = za::max(longest, worldGrid[1 + b] & 255u);
        }
        const za::U32 listed = worldGrid.empty() ? 0u : static_cast<za::U32>(worldGrid.size()) - worldGrid[0] - 2u;
        Con_Printf("world decal grid: %d buckets, %u occupied, %u over %u marks (%u marks left out whole), %u the most "
                   "in one, %u the longest list; %u listed (the cells reaching each mark); %.1f KB marks, %.1f KB grid\n",
            static_cast<int>(counts.size()), occupied, full, worldBucketMarks, worldLeftOut, largest, longest, listed,
            static_cast<double>(worldDecals.size() * sizeof(WorldDecal)) / 1024.0,
            static_cast<double>(worldGrid.size() * sizeof(za::U32)) / 1024.0);
    }
    else
    {
        Con_Printf("drawn as meshes over the world, not following its parallax (%s)\n",
            "vr_decals_world 0");
    }
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
    entityDecals.clear();
    dropCount = 0;
    builtFrame = -1;
    worldDirty = true;
    staticVertices.clear();
    staticGone = 0;
    staticChanged.clear();
    staticResend = true;
    gibs.clear();
    holes.clear();
    gore::clear();
    permdecals::clear(); // (and those baked for good)
}

// A mark laid on brush `id` (one on several brushes: if its middle is in lo..hi).
[[nodiscard]] bool onBrush(const Decal& d, int id, const glm::vec3& lo, const glm::vec3& hi)
{
    return !d.tris.empty() && (d.brush == id || (d.brush == -1 && !glm::any(glm::lessThan(d.centre, lo)) &&
                                                     !glm::any(glm::greaterThan(d.centre, hi))));
}

int countOn(int id, const glm::vec3& lo, const glm::vec3& hi)
{
    int n = 0;
    for(za::SizeT k = 0; k < decals.size(); k++)
    {
        n += onBrush(decals[k], id, lo, hi) ? 1 : 0;
    }
    return n;
}

void removeOn(int id, const glm::vec3& lo, const glm::vec3& hi)
{
    for(za::SizeT k = 0; k < decals.size(); k++)
    {
        Decal& d = decals[k];
        if(!onBrush(d, id, lo, hi))
        {
            continue;
        }
        // Gone in place (its ring slot kept: the ring only loses its oldest): no area, no triangles.
        unsettle(d);
        d.tris.clear();
        d.u = d.v = glm::vec3{0.f};
        d.depth = 0.f;
        d.buckets.clear();
        d.bucketsMask = 0;
        worldDirty = true;
        builtFrame = -1;
    }
}

// Decals on the walls (vr_memstats).
int liveCount()
{
    return static_cast<int>(decals.size());
}

void liveStamps(za::Vector<permdecals::Stamp>& out)
{
    out.clear();
    for(za::SizeT k = 0; k < decals.size(); k++)
    {
        const Decal& d = decals[k];
        if(d.brush == 0 && !d.permTried && !d.fromLayer && !d.tris.empty())
        {
            out.pushBack(stampOf(d));
        }
    }
}

// A ragdoll's limb (vr_ragdoll.cpp, every frame it is drawn: `key` its entity's and the limb's), its end at `at`: flung
// faster than limbBleedSpeed, it leaves a blood trail and drops on the floor as a gib does (vr_ragdoll_blood; the gibs'
// density, vr_gib_blood_trail; fewer drops).
void limbTrail(int key, const glm::vec3& at)
{
    if(!vr_ragdoll_blood.value || !vr_gib_blood.value || !cl.worldmodel)
    {
        return;
    }
    if(host_framecount - limbsPrunedFrame > 100 || host_framecount < limbsPrunedFrame)
    {
        limbsPrunedFrame = host_framecount;
        erase_if(limbs, [](const auto& kv) { return cl.time - kv.second.seen > 1.0 || cl.time < kv.second.seen; });
    }
    Limb& l = limbs[key];
    const float dt = static_cast<float>(cl.time - l.seen);
    if(dt > 0.25f || dt < 0.f || glm::distance(l.at, at) > 128.f)
    {
        l = Limb{};
        l.at = at;
        l.seen = cl.time;
        l.dropsLeft = limbDrops;
        return;
    }
    const float travelled = glm::distance(l.at, at);
    const float density = za::clamp(vr_gib_blood_trail.value, 0.f, 4.f);
    if(dt > 0.f && travelled > 0.01f && travelled / dt > limbBleedSpeed && density > 0.f)
    {
        const glm::vec3 dir = (at - l.at) / travelled;
        if(particles::enabled())
        {
            along(l.at, at, 2.f * gibTrailSpacing / density, l.sinceTrail, 12,
                [&](const glm::vec3& p) { particles::spawn(p, dir, particles::Preset::BloodTrail, 1); });
        }
        if(vr_decals.value && l.dropsLeft > 0)
        {
            along(l.at, at, 3.f * gibDropSpacing / density, l.sinceDrop, za::min(1, l.dropsLeft),
                [&](const glm::vec3& p) {
                    drip(p);
                    l.dropsLeft--;
                });
        }
    }
    l.at = at;
    l.seen = cl.time;
}

} // namespace qvr::decals

// cl_tent.c: Quake's own wall hits and explosions (vr_api.h's QVR_DECAL_TE_*), each its own carved mark.
extern "C" void VR_DecalTempEntity(int what, const float* pos)
{
    using namespace qvr::decals;
    if(!qvr::vr_decals.value || !cl.worldmodel)
    {
        return;
    }
    const glm::vec3 p{pos[0], pos[1], pos[2]};
    Impact impact = Impact::Pellet;
    switch(what)
    {
        case QVR_DECAL_TE_EXPLOSION: impact = Impact::Blast; break;
        case QVR_DECAL_TE_SPIKE: impact = lavaNailNear(p) ? Impact::LavaNail : Impact::Nail; break;
        case QVR_DECAL_TE_SUPERSPIKE: impact = lavaNailNear(p) ? Impact::LavaSuperNail : Impact::SuperNail; break;
        case QVR_DECAL_TE_MONSTERSPIKE: impact = Impact::MonsterSpike; break;
        default: break;
    }
    impactAt(impact, p, glm::vec3{0.f});
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
    if(!e.model || e.model->type != mod_sprite || !modelmeta::is(e.model, modelmeta::Id::BulletDecal) ||
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
            return !m || !modelmeta::is(m, modelmeta::Id::BulletDecal);
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

using namespace qvr;

// R_SetupView (each view): the marks on the world made again for its shader if they came or went (once), and its clock
// (DecalClock: x now on the marks' clock, y vr_decal_life, z 1: on).
extern "C" void VR_DecalsFrame(float clock[4])
{
    using namespace qvr::decals;
    bakeSettled(); // (the permanent layer's: once a frame)
    clock[0] = clock[1] = clock[2] = clock[3] = 0.f;
    if(!vr_decals.value || !onWorld() || !cl.worldmodel)
    {
        return;
    }
    // Made once a frame, in its first view, for every view of it: marks that come or go during the frame (blood
    // landing in the left eye's particles, the gore's frame in its decals::draw) show in both eyes together, from the
    // next frame (made again in the right eye, they showed there a frame before the left).
    const bool firstView = worldFrame != host_framecount;
    worldFrame = host_framecount;
    const double life = expire();
    if(qvr::decals::decals.empty())
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }
    if(worldBucketLimit() != worldBucketMarks)
    {
        worldDirty = true; // (vr_decal_bucket_max changed: the grid listed again under it)
    }
    if(worldDirty && firstView)
    {
        worldDirty = false;
        buildWorld();
    }
    if(!atlas || worldDecals.empty())
    {
        return;
    }
    clock[0] = static_cast<float>(cl.time - worldClock);
    clock[1] = static_cast<float>(life);
    clock[2] = 1.f;
    clock[3] = static_cast<float>(retro::categorySet(retro::Category::Decals)); // their retro set (0 none)
}

// R_SetupView: the carved marks' settings (DecalCarve, vr_glsl.h's CarveAt): the depth's multiplier (0: off, flat), the
// rim's, the most steps a ray takes + 128 x the style (vr_decal_carve_style: 0 smooth, 1 retro, 2 faceted, 3 retro on
// retro textures' blocks, else smooth), the distance past which they are flat.
extern "C" void VR_DecalCarveFrame(float out[4])
{
    using namespace qvr;
    const bool on = vr_decal_carve.value != 0.f && vr_decals.value != 0.f && vr_decals_world.value != 0.f;
    out[0] = on ? za::clamp(vr_decal_carve_depth.value, 0.f, 4.f) : 0.f;
    out[1] = za::clamp(vr_decal_carve_rim.value, 0.f, 4.f);
    const float style = vr_decal_carve_style.value < 0.f ? 3.f : za::clamp(za::floor(vr_decal_carve_style.value), 0.f, 2.f);
    out[2] = za::clamp(za::floor(vr_decal_carve_steps.value), 4.f, 64.f) + 128.f * style;
    out[3] = za::max(vr_decal_carve_fade.value, 16.f);
}

// R_DrawBrushModels_Real: the marks (3), their grid (4) and the atlas (unit 14) for the world's shader.
extern "C" void VR_BindDecals(void)
{
    using namespace qvr::decals;
    permdecals::bind(); // (the permanent layer: its table on 10, its atlas on unit 18)
    if(worldDecals.empty() || !atlas)
    {
        return;
    }
    gfx::bindStorage(3, worldDecalBuffer);
    gfx::bindStorage(4, worldGridBuffer);
    gfx::bindTexture(14, atlas);
}
