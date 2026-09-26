// vr_decals.cpp -- see vr_decals.hpp.

#include "vr_decals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_modellight.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>

namespace qvr::decals
{
namespace
{

enum Kind : int
{
    Blood,
    BloodDrop,
    Scorch,
    Hole
};

// The atlas: 4 x 4 cells of 256 texels (1024 x 1024 RGBA, mipmapped: 5.3 MB). Blood splats 0-5,
// drops 6-7, scorches 8-10, chips 11-15.
constexpr int cellSize = 256;
constexpr int cellsPerRow = 4;
constexpr int atlasSize = cellSize * cellsPerRow;
constexpr int firstCell[] = {0, 6, 8, 11};
constexpr int cellCount[] = {6, 2, 3, 5};

// The chips are lit from the cell's +y (the decal's v, turned towards the light that reaches it),
// this high above the surface.
const glm::vec3 chipLight = glm::normalize(glm::vec3{0.f, 0.8f, 0.6f});

gfx::Texture atlas = 0;

struct Decal
{
    glm::vec3 centre, u, v; // u, v: half extents on the surface
    int cell;
    double born;
};

std::deque<Decal> decals;
std::vector<gfx::Vertex> vertices;
int builtFrame = -1; // the host frame `vertices` were built in; -1 when decals came or went since
int addedThisFrame = 0;
int addedFrame = -1;

// ---- The atlas ---------------------------------------------------------------------------------

unsigned rngState = 12345u;
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
    const int i = static_cast<int>(std::floor(x));
    const float f = x - i;
    const float s = f * f * (3.f - 2.f * f);
    return hash(((i % lobes) + lobes) % lobes, seed) * (1.f - s) + hash((((i + 1) % lobes) + lobes) % lobes, seed) * s;
}

[[nodiscard]] float smoothstep(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Smooth value noise over the plane, 0..1.
[[nodiscard]] float noise(glm::vec2 p, int seed)
{
    const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
    const float fx = p.x - x, fy = p.y - y;
    const float sx = fx * fx * (3.f - 2.f * fx), sy = fy * fy * (3.f - 2.f * fy);
    const auto h = [seed](int i, int j) { return hash(i * 7919 + j * 104729, seed); };
    return (h(x, y) * (1.f - sx) + h(x + 1, y) * sx) * (1.f - sy) + (h(x, y + 1) * (1.f - sx) + h(x + 1, y + 1) * sx) * sy;
}

[[nodiscard]] float segmentDistance(glm::vec2 p, glm::vec2 a, glm::vec2 b, float& along)
{
    const glm::vec2 ab = b - a;
    along = std::clamp(glm::dot(p - a, ab) / std::max(glm::dot(ab, ab), 1e-8f), 0.f, 1.f);
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
    std::vector<glm::vec2> points;
    float width;
};

// A chip's cracks and the flecks of debris round it (small blobs).
struct ChipShape
{
    std::vector<Crack> cracks;
    std::vector<Blob> flecks;
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
        const float a = std::atan2(q.y, q.x) / 6.2831853f + 0.5f;
        const float edge = radius * (1.f + 0.22f * (ring(a, 7, seed) - 0.5f) + 0.12f * (ring(a, 19, seed + 1) - 0.5f));
        return glm::length(q) / edge;
    };
    const auto height = [&](glm::vec2 q) {
        const float rr = relative(q);
        float h = rr < 1.f ? -0.1f * (1.f - rr * rr) : 0.f;                      // the bowl
        h += 0.022f * std::exp(-(rr - 1.05f) * (rr - 1.05f) / 0.09f);            // the rim
        h += 0.012f * (noise(q * 26.f, seed + 2) - 0.5f) * std::max(0.f, 1.3f - rr); // broken, rough
        return h;
    };

    const float e = 1.5f / cellSize;
    const float dx = (height(p + glm::vec2{e, 0.f}) - height(p - glm::vec2{e, 0.f})) / (2.f * e);
    const float dy = (height(p + glm::vec2{0.f, e}) - height(p - glm::vec2{0.f, e})) / (2.f * e);
    const glm::vec3 n = glm::normalize(glm::vec3{-dx, -dy, 1.f});
    const float rr = relative(p);

    // Lit as the flat surface round it is lit: 1 where flat.
    float shade = std::max(0.f, glm::dot(n, chipLight)) / chipLight.z;
    // The crater's own shadow: its wall on the light's side hides the floor next to it.
    if(rr < 1.f)
    {
        const glm::vec2 dir = p / std::max(glm::length(p), 1e-4f);
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
        for(std::size_t i = 0; i + 1 < c.points.size(); i++)
        {
            float along = 0.f;
            const float d = segmentDistance(p, c.points[i], c.points[i + 1], along);
            const float t = (static_cast<float>(i) + along) / static_cast<float>(c.points.size() - 1);
            const float w = c.width * (1.f - 0.85f * t);
            crack = std::max(crack, smoothstep(w, w * 0.3f, d) * (1.f - 0.5f * t));
        }
    }
    f *= 1.f - 0.7f * crack * smoothstep(0.7f, 1.1f, rr);

    // Dust round it (lighter), flecks of debris (darker), fading out towards the cell's edge.
    const float a = std::atan2(p.y, p.x) / 6.2831853f + 0.5f;
    const float r = glm::length(p);
    const float halo = smoothstep(0.55f + 0.25f * ring(a, 11, seed + 3), 0.25f, r) * smoothstep(0.8f, 1.3f, rr);
    f *= 1.f + 0.16f * halo * noise(p * 9.f, seed + 4);
    float fleck = 0.f;
    for(const Blob& b : shape.flecks)
    {
        fleck = std::max(fleck, smoothstep(b.r, b.r * 0.4f, glm::length(p - b.c)));
    }
    f *= 1.f - 0.4f * fleck * smoothstep(1.1f, 1.5f, rr);

    // Slightly warm where it marks the surface: dust and the soot of the hit.
    const glm::vec3 tint = glm::mix(glm::vec3{1.f}, glm::vec3{1.03f, 1.f, 0.95f}, std::min(1.f, std::fabs(f - 1.f) * 3.f));
    return glm::clamp(f * tint, 0.f, 2.f);
}

// Coverage of a cell texel at `p` (-1..1): what `kind` variant `seed` covers, and its colour (what
// the surface is multiplied by).
void cellTexel(Kind kind, int seed, const std::vector<Blob>& blobs, glm::vec2 p, float& alpha, glm::vec3& color)
{
    const float r = glm::length(p);
    const float a = std::atan2(p.y, p.x) / 6.2831853f + 0.5f;
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
                cover = std::max(cover, smoothstep(b.r + 0.02f, b.r - 0.02f, glm::length(d)));
            }
            alpha = cover * (0.8f + 0.12f * noise(p * 14.f, seed) + 0.08f * noise(p * 45.f, seed + 1));
            // Darker, thicker in the middle.
            color = glm::mix(glm::vec3{0.18f, 0.01f, 0.01f}, glm::vec3{0.42f, 0.04f, 0.03f}, smoothstep(0.f, 0.7f, r));
            break;
        }
        case Scorch:
        {
            const float edge = 0.72f + 0.2f * ring(a, 9, seed) + 0.08f * (noise(p * 5.f, seed + 1) - 0.5f);
            const float soot = 0.55f * noise(p * 4.f, seed + 3) + 0.3f * noise(p * 11.f, seed + 4) + 0.15f * noise(p * 29.f, seed + 5);
            alpha = smoothstep(edge, 0.1f, r) * std::clamp(0.35f + 0.85f * soot, 0.f, 1.f) * 0.95f;
            color = glm::vec3{0.1f, 0.085f, 0.07f};
            break;
        }
        case Hole: break; // chipTexel
    }
}

// A texel of what the surface is multiplied by, `f` (0..2 in each channel; 1 leaves it as it is),
// for the modulating blend (dst * (colour + 1 - alpha)), premultiplied: dimming by alpha, brightening
// by the colour. Linear in `f`, so that filtering and the fading vertex colour stay right.
void encode(glm::vec3 f, unsigned char* out)
{
    f = glm::clamp(f, 0.f, 2.f);
    const float dim = std::max(0.f, 1.f - std::min({f.r, f.g, f.b}));
    const glm::vec3 c = f - 1.f + dim;
    for(int i = 0; i < 3; i++)
    {
        out[i] = static_cast<unsigned char>(std::clamp(c[i], 0.f, 1.f) * 255.f + 0.5f);
    }
    out[3] = static_cast<unsigned char>(std::clamp(dim, 0.f, 1.f) * 255.f + 0.5f);
}

void makeAtlas()
{
    std::vector<unsigned char> rgba(atlasSize * atlasSize * 4, 0);
    for(int kind = 0; kind < 4; kind++)
    {
        for(int n = 0; n < cellCount[kind]; n++)
        {
            const int cell = firstCell[kind] + n;
            const int seed = cell * 131 + 7;
            rngState = static_cast<unsigned>(seed);

            // A chip's cracks (a few, jagged, out from the crater's edge) and flecks.
            ChipShape chip;
            std::vector<Crack>& cracks = chip.cracks;
            if(kind == Hole)
            {
                for(int i = 0; i < 70; i++)
                {
                    const float ang = frand() * 6.2831853f;
                    const float dist = 0.28f + 0.5f * frand() * frand();
                    chip.flecks.push_back({glm::vec2{std::cos(ang), std::sin(ang)} * dist, 0.006f + frand() * 0.012f, glm::vec2{0.f}});
                }
                const int count = 3 + static_cast<int>(frand() * 4.f);
                for(int i = 0; i < count; i++)
                {
                    float ang = (static_cast<float>(i) + 0.2f + frand() * 0.6f) / count * 6.2831853f;
                    glm::vec2 at = glm::vec2{std::cos(ang), std::sin(ang)} * 0.18f;
                    Crack c{{at}, 0.012f + frand() * 0.012f};
                    const float length = 0.25f + frand() * 0.45f;
                    const int steps = 4 + static_cast<int>(frand() * 3.f);
                    for(int s = 0; s < steps; s++)
                    {
                        ang += (frand() - 0.5f) * 0.9f;
                        at += glm::vec2{std::cos(ang), std::sin(ang)} * (length / steps);
                        c.points.push_back(at);
                    }
                    cracks.push_back(std::move(c));
                    // Now and then a branch off its middle.
                    if(frand() < 0.4f)
                    {
                        const glm::vec2 from = cracks.back().points[cracks.back().points.size() / 2];
                        const float b = ang + (frand() < 0.5f ? -0.8f : 0.8f);
                        cracks.push_back({{from, from + glm::vec2{std::cos(b), std::sin(b)} * (0.08f + frand() * 0.12f)},
                            cracks.back().width * 0.6f});
                    }
                }
            }

            // Blood's satellite droplets and streaks (a drop cell: a few drops only).
            std::vector<Blob> blobs;
            const int count = kind == Blood ? 9 + static_cast<int>(frand() * 6) : kind == BloodDrop ? 4 : 0;
            for(int i = 0; i < count; i++)
            {
                const float ang = frand() * 6.2831853f;
                const float dist = kind == Blood ? 0.45f + frand() * 0.45f : frand() * 0.55f;
                const glm::vec2 dir{std::cos(ang), std::sin(ang)};
                const float streak = kind == Blood && frand() < 0.4f ? 1.f + frand() * 3.f : 0.f;
                blobs.push_back({dir * dist, (kind == Blood ? 0.03f : 0.08f) + frand() * (kind == Blood ? 0.06f : 0.12f),
                    dir * streak});
            }

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
                    else
                    {
                        float alpha = 0.f;
                        glm::vec3 color{1.f};
                        cellTexel(static_cast<Kind>(kind), seed, blobs, p, alpha, color);
                        f = glm::mix(glm::vec3{1.f}, color, std::clamp(alpha, 0.f, 1.f));
                    }
                    // Faded out to nothing at the cell's border: filtering (and the smaller
                    // mipmaps) must not reach the next cell.
                    const int edge = std::min({x, y, cellSize - 1 - x, cellSize - 1 - y});
                    f = glm::mix(glm::vec3{1.f}, f, smoothstep(3.f, 14.f, static_cast<float>(edge)));
                    encode(f, &rgba[((oy + y) * atlasSize + ox + x) * 4]);
                }
            }
        }
    }
    atlas = gfx::createTexture(atlasSize, atlasSize, rgba.data(), true);
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

// Whether a surface with normal `n` lies under `p` (the decal's corner does not hang in the air).
[[nodiscard]] bool supported(const glm::vec3& p, const glm::vec3& n)
{
    glm::vec3 w, hn;
    float f;
    return hitWorld(p + n * 2.f, p - n * 2.f, w, hn, f) && glm::dot(hn, n) > 0.9f;
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
        const float weight = (l.value - dist * std::max(l.scale, 0.01f)) * facing;
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

void add(Kind kind, const glm::vec3& where, const glm::vec3& normal, float size)
{
    const int max = static_cast<int>(vr_decal_max.value);
    if(!vr_decals.value || max <= 0 || !cl.worldmodel)
    {
        return;
    }
    if(addedFrame != host_framecount)
    {
        addedFrame = host_framecount;
        addedThisFrame = 0;
    }
    if(++addedThisFrame > 24)
    {
        return; // a shotgun blast into a wall is enough at once
    }

    // Not the same mark twice in one place (Quake's effect and Quake VR's for one hit).
    for(auto it = decals.rbegin(); it != decals.rend() && it - decals.rbegin() < 16; ++it)
    {
        if(cl.time - it->born < 0.1 && glm::distance(it->centre, where) < 2.f)
        {
            return;
        }
    }

    const glm::vec3 n = glm::normalize(normal);
    const glm::vec3 t0 = glm::normalize(glm::cross(n, std::fabs(n.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0}));
    const glm::vec3 t1 = glm::cross(n, t0);
    const float ang = static_cast<float>(rand() % 6283) * 0.001f;
    glm::vec3 u = (t0 * std::cos(ang) + t1 * std::sin(ang));
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
    const glm::vec3 centre = where + n * 0.2f;

    // Shrunk (once) or dropped where it would hang over an edge.
    for(int attempt = 0; attempt < 2; attempt++)
    {
        const float half = size * 0.5f;
        bool ok = true;
        for(const glm::vec2 c : {glm::vec2{-1, -1}, glm::vec2{1, -1}, glm::vec2{1, 1}, glm::vec2{-1, 1}})
        {
            if(!supported(where + (u * c.x + v * c.y) * (half * 0.8f), n))
            {
                ok = false;
                break;
            }
        }
        if(ok)
        {
            const int cell = firstCell[kind] + rand() % cellCount[kind];
            decals.push_back({centre, u * half, v * half, cell, cl.time});
            while(static_cast<int>(decals.size()) > max)
            {
                decals.pop_front();
            }
            builtFrame = -1;
            return;
        }
        size *= 0.55f;
        if(size < 2.f)
        {
            return;
        }
    }
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
};

std::unordered_map<int, Gib> gibs;
int gibsPrunedFrame = 0;

// Hipnotic's bullet holes turned into chips (VR_BulletHoleSprite), by entity number: where they were.
std::unordered_map<int, glm::vec3> holes;
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
    float at = std::max(0.f, spacing - since); // how far along the next one is
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
    add(Blood, where, normal, std::clamp(16.f + strength * 0.03f, 16.f, 40.f));

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
            const float size = std::clamp(8.f + count * 0.4f, 8.f, 28.f);
            if(hitWorld(org, org - glm::vec3{0, 0, 160}, where, normal, f) && normal.z > 0.6f)
            {
                add(Blood, where, normal, size * (0.8f + f * 0.6f)); // the farther it falls, the wider it spreads
            }
            const float a = random(0.f, 6.2831853f);
            const glm::vec3 side = glm::dot(dir, dir) > 1e-4f ? glm::normalize(dir) : glm::vec3{std::cos(a), std::sin(a), random(-0.3f, 0.2f)};
            if(hitWorld(org, org + glm::normalize(side) * 72.f, where, normal, f))
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

void draw()
{
    QVR_GPU_PROFILE("decals");
    if(decals.empty() || !vr_decals.value)
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }

    // Built once a frame (and again when marks come or go), for both eyes.
    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;

        // Faded out over their last five seconds.
        const double life = std::max(5.f, vr_decal_life.value);
        while(!decals.empty() && cl.time - decals.front().born > life)
        {
            decals.pop_front();
        }

        vertices.clear();
        constexpr float inset = 0.5f / atlasSize;
        for(const Decal& d : decals)
        {
            const float age = static_cast<float>(cl.time - d.born);
            const float a = std::clamp(static_cast<float>((life - age) / 5.0), 0.f, 1.f);
            const glm::vec4 c{a, a, a, a};
            const float u0 = static_cast<float>(d.cell % cellsPerRow) / cellsPerRow + inset;
            const float v0 = static_cast<float>(d.cell / cellsPerRow) / cellsPerRow + inset;
            const float u1 = u0 + 1.f / cellsPerRow - 2.f * inset, v1 = v0 + 1.f / cellsPerRow - 2.f * inset;
            const gfx::Vertex q[4] = {{d.centre - d.u - d.v, {u0, v0}, c}, {d.centre + d.u - d.v, {u1, v0}, c},
                {d.centre + d.u + d.v, {u1, v1}, c}, {d.centre - d.u + d.v, {u0, v1}, c}};
            for(int i : {0, 1, 2, 0, 2, 3})
            {
                vertices.push_back(q[i]);
            }
        }
    }
    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Modulate, .depthTest = true, .depthWrite = false}, atlas);
}

void count_f()
{
    int kinds[4]{};
    for(const Decal& d : decals)
    {
        const Kind kind = d.cell >= firstCell[Hole]        ? Hole
                          : d.cell >= firstCell[Scorch]    ? Scorch
                          : d.cell >= firstCell[BloodDrop] ? BloodDrop
                                                           : Blood;
        kinds[kind]++;
    }
    Con_Printf("%d decals: %d blood, %d drops, %d scorch, %d chips\n", static_cast<int>(decals.size()), kinds[0], kinds[1],
        kinds[2], kinds[3]);
}

void clear()
{
    decals.clear();
    builtFrame = -1;
    gibs.clear();
    holes.clear();
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
        std::erase_if(holes, [](const auto& kv) {
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
        std::erase_if(gibs, [](const auto& kv) { return cl.time - kv.second.seen > 1.0 || cl.time < kv.second.seen; });
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
    const float density = std::clamp(vr_gib_blood_trail.value, 0.f, 4.f) * (zombie ? 0.5f : 1.f);
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
            along(g.origin, o, gibDropSpacing / density, g.sinceDrop, std::min(4, g.dropsLeft),
                [&](const glm::vec3& p) {
                    drip(p);
                    g.dropsLeft--;
                });
        }
    }
    g.origin = o;
    g.seen = cl.time;
    return ours;
}
