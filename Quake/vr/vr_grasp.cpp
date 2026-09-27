// vr_grasp.cpp -- see vr_grasp.hpp.

#include "vr_grasp.hpp"
#include "vr_render.hpp"
#include "vr_api_render.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace qvr::grasp
{
namespace
{

using handrig::data::firstVertex;
using handrig::data::vertices;

constexpr float maxCurl = 4.f;       // the tightest fist
constexpr float tolerance = 0.01f;   // hand units: a sphere this close touches
constexpr float leastStep = 0.002f;  // curl frames: the smallest step closing makes (conservative advancement)
constexpr int maxSteps = 64;         // steps closing a finger (a few near a contact, one or two across open air)
constexpr int halvings = 7;          // opening from the fist to the first clear curl: to 1/512 of a step
constexpr float searchReach = 0.6f;  // hand units beyond a sphere's radius a distance query looks (farther: "clear by this")
constexpr float cellHandUnits = 2.f; // the shapes' grid cells, in hand units (as big as they are in the hand)

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

// ----------------------------------------------------------------------------
// The hand as spheres: each finger segment (bones 1..3) as a row of spheres along its side towards the palm, fitted to
// its vertices at the bind pose (rig space: each bone's own frame there); the palm's side as spheres just under its
// skin.

struct Sphere
{
    glm::vec3 c;
    float r;
};

struct Kinematics
{
    std::vector<Sphere> bone[handrig::FingerCount][handrig::jointsPerFinger + 1];
    float rate[handrig::FingerCount][handrig::jointsPerFinger]{}; // radians a curl frame, at most
    std::vector<Sphere> palm;
    std::vector<Sphere> thenar; // the ball of the thumb
    glm::vec3 palmCentre{0.f};
};

Kinematics buildKinematics()
{
    Kinematics k;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            k.rate[f][j] = handrig::jointRate(f, j);
        }
        for(int b = 1; b <= handrig::jointsPerFinger; b++)
        {
            // The segment's vertices: the ring at its far end (its bone's) and the one at its joint (the bone
            // before's): its triangles join them. Its axis: from the near ring's middle to the far one's.
            std::vector<glm::vec3> pts;
            glm::vec3 ring[2]{glm::vec3{0.f}, glm::vec3{0.f}};
            int counts[2]{};
            for(int v = firstVertex[f]; v < firstVertex[f + 1]; v++)
            {
                const int end = vertices[v].bone == b ? 1 : vertices[v].bone == b - 1 ? 0 : -1;
                if(end >= 0)
                {
                    pts.push_back(vec(vertices[v].local[0]));
                    ring[end] += pts.back();
                    counts[end]++;
                }
            }
            if(!counts[0] || !counts[1])
            {
                continue;
            }
            ring[0] /= static_cast<float>(counts[0]);
            ring[1] /= static_cast<float>(counts[1]);
            const glm::vec3 mid = 0.5f * (ring[0] + ring[1]);
            const glm::vec3 dir = glm::normalize(ring[1] - ring[0]);

            // Its cross-section: the direction its vertices spread most across the axis (`deep`), and across that.
            float lo = 1e9f, hi = -1e9f;
            glm::mat3 across{0.f};
            for(const glm::vec3& p : pts)
            {
                const float t = glm::dot(p - mid, dir);
                lo = std::fmin(lo, t);
                hi = std::fmax(hi, t);
                const glm::vec3 q = p - mid - dir * t;
                across += glm::outerProduct(q, q);
            }
            glm::vec3 deep = glm::normalize(glm::cross(dir, std::fabs(dir.x) < 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 1.f, 0.f}));
            for(int i = 0; i < 16; i++)
            {
                glm::vec3 next = across * deep;
                next -= dir * glm::dot(next, dir);
                if(glm::length(next) < 1e-9f)
                {
                    break;
                }
                deep = glm::normalize(next);
            }
            const glm::vec3 wide = glm::cross(dir, deep);
            glm::vec2 centre{0.f};
            for(const glm::vec3& p : pts)
            {
                centre += glm::vec2{glm::dot(p - mid, deep), glm::dot(p - mid, wide)};
            }
            centre /= static_cast<float>(pts.size());
            float deepHalf = 0.f, wideHalf = 0.f;
            for(const glm::vec3& p : pts)
            {
                deepHalf = std::fmax(deepHalf, std::fabs(glm::dot(p - mid, deep) - centre.x));
                wideHalf = std::fmax(wideHalf, std::fabs(glm::dot(p - mid, wide) - centre.y));
            }

            // One row of spheres as wide as the segment, along its side towards the palm (+y, the side that closes
            // onto what the hand holds): a deep, blocky segment's back is left out. A radius apart, from end to end
            // (the ends half of one in).
            const float radius = 0.95f * wideHalf;
            const float towardsPalm = glm::dot(deep, glm::vec3{0.f, 1.f, 0.f}) >= 0.f ? 1.f : -1.f;
            const float offset = std::fmax(0.95f * deepHalf - radius, 0.f) * towardsPalm;
            const glm::vec3 axis = mid + deep * (centre.x + offset) + wide * centre.y;
            const float a = lo + radius * 0.5f, z = hi - radius * 0.5f;
            const int n = z > a ? std::max(2, static_cast<int>(std::ceil((z - a) / radius)) + 1) : 1;
            for(int i = 0; i < n; i++)
            {
                const float t = n > 1 ? a + (z - a) * static_cast<float>(i) / static_cast<float>(n - 1) : 0.5f * (lo + hi);
                k.bone[f][b].push_back({axis + dir * t, radius});
            }
        }
    }

    // The palm's side (+y): a sphere under the middle of each of its triangles facing that way, its top on the skin.
    constexpr float palmRadius = 0.45f;
    glm::vec3 sum{0.f};
    int count = 0;
    for(int t = 0; t < handrig::data::numPalmTriangles; t++)
    {
        const auto* tri = handrig::data::palmTriangles[t];
        const auto& a = handrig::data::palmVertices[tri[0]];
        const auto& b = handrig::data::palmVertices[tri[1]];
        const auto& c = handrig::data::palmVertices[tri[2]];
        const glm::vec3 pa = vec(a.pos), pb = vec(b.pos), pc = vec(c.pos);
        glm::vec3 n = glm::cross(pb - pa, pc - pa);
        if(glm::length(n) < 1e-6f)
        {
            continue;
        }
        n = glm::normalize(n);
        if(glm::dot(n, vec(a.normal) + vec(b.normal) + vec(c.normal)) < 0.f)
        {
            n = -n; // outwards, as the vertices' normals
        }
        if(n.y < 0.35f)
        {
            continue;
        }
        const glm::vec3 p = (pa + pb + pc) / 3.f;
        const Sphere s{p - n * palmRadius, palmRadius};
        if(a.thumb + b.thumb + c.thumb > 0.f)
        {
            k.thenar.push_back(s);
        }
        else
        {
            k.palm.push_back(s);
            sum += p;
            count++;
        }
    }
    k.palmCentre = count ? sum / static_cast<float>(count) : glm::vec3{0.f};
    return k;
}

const Kinematics& kinematics()
{
    static const Kinematics k = buildKinematics();
    return k;
}

// The point of triangle abc nearest p (Ericson, Real-Time Collision Detection, 5.1.5).
[[nodiscard]] glm::vec3 closestOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return a + ab * (d1 / (d1 - d3));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denom = 1.f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

} // namespace

// ----------------------------------------------------------------------------
// The shapes: a model's triangles, kept in its own coordinates as they are measured (an alias model's raw vertices
// scaled by its header: "real" model units, the same on every axis), with a grid of cells for the distance queries.
// Made once per model and pose (the grid once per size in the hand), for every solve after.

struct Shape::Space
{
    struct Tri
    {
        glm::vec3 a, b, c;
        glm::vec3 normal; // unit (0 for a degenerate one)
        float plane;      // dot(normal, a)
        glm::vec3 lo, hi; // its box
    };
    std::vector<Tri> tris;
    glm::mat4 rawToReal{1.f};

    float cell{0.f}; // the grid's (real units), for the hand's size it was made at
    glm::vec3 lo{0.f};
    int size[3]{1, 1, 1};
    std::vector<std::uint32_t> first; // per cell, its first item; one more at the end
    std::vector<std::uint16_t> items;
    std::vector<std::uint32_t> stamps; // per triangle: the query that last looked at it
    std::uint32_t stamp{0};

    void buildGrid(float cellSize)
    {
        cell = cellSize;
        glm::vec3 hi{-1e9f};
        lo = glm::vec3{1e9f};
        for(const Tri& t : tris)
        {
            lo = glm::min(lo, t.lo);
            hi = glm::max(hi, t.hi);
        }
        std::size_t cells = 1;
        for(int k = 0; k < 3; k++)
        {
            size[k] = std::clamp(static_cast<int>((hi[k] - lo[k]) / cell) + 1, 1, 256);
            cells *= static_cast<std::size_t>(size[k]);
        }
        const auto each = [&](const Tri& t, auto&& f) {
            int from[3], to[3];
            for(int k = 0; k < 3; k++)
            {
                from[k] = std::clamp(static_cast<int>((t.lo[k] - lo[k]) / cell), 0, size[k] - 1);
                to[k] = std::clamp(static_cast<int>((t.hi[k] - lo[k]) / cell), 0, size[k] - 1);
            }
            for(int z = from[2]; z <= to[2]; z++)
            {
                for(int y = from[1]; y <= to[1]; y++)
                {
                    for(int x = from[0]; x <= to[0]; x++)
                    {
                        f((static_cast<std::size_t>(z) * static_cast<std::size_t>(size[1]) + static_cast<std::size_t>(y)) *
                              static_cast<std::size_t>(size[0]) +
                          static_cast<std::size_t>(x));
                    }
                }
            }
        };
        std::vector<std::uint32_t> counts(cells, 0u);
        for(const Tri& t : tris)
        {
            each(t, [&](std::size_t c) { counts[c]++; });
        }
        first.assign(cells + 1, 0u);
        for(std::size_t c = 0; c < cells; c++)
        {
            first[c + 1] = first[c] + counts[c];
        }
        items.resize(first[cells]);
        std::fill(counts.begin(), counts.end(), 0u);
        for(std::size_t i = 0; i < tris.size(); i++)
        {
            each(tris[i], [&](std::size_t c) { items[first[c] + counts[c]++] = static_cast<std::uint16_t>(i); });
        }
        stamps.assign(tris.size(), 0u);
        stamp = 0;
    }

    // The distance from `p` to the nearest triangle, at most `limit` (beyond it, `limit`); its point in `at`.
    [[nodiscard]] float nearest(const glm::vec3& p, float limit, glm::vec3* at = nullptr)
    {
        float best = limit;
        int from[3], to[3];
        for(int k = 0; k < 3; k++)
        {
            from[k] = static_cast<int>(std::floor((p[k] - limit - lo[k]) / cell));
            to[k] = static_cast<int>(std::floor((p[k] + limit - lo[k]) / cell));
            if(to[k] < 0 || from[k] >= size[k])
            {
                return best;
            }
            from[k] = std::max(from[k], 0);
            to[k] = std::min(to[k], size[k] - 1);
        }
        if(++stamp == 0)
        {
            std::fill(stamps.begin(), stamps.end(), 0u);
            stamp = 1;
        }
        for(int z = from[2]; z <= to[2]; z++)
        {
            for(int y = from[1]; y <= to[1]; y++)
            {
                const std::size_t row =
                    (static_cast<std::size_t>(z) * static_cast<std::size_t>(size[1]) + static_cast<std::size_t>(y)) *
                    static_cast<std::size_t>(size[0]);
                for(std::size_t c = row + static_cast<std::size_t>(from[0]); c <= row + static_cast<std::size_t>(to[0]); c++)
                {
                    for(std::uint32_t i = first[c]; i < first[c + 1]; i++)
                    {
                        const std::uint16_t index = items[i];
                        if(stamps[index] == stamp)
                        {
                            continue; // in another cell already looked at
                        }
                        stamps[index] = stamp;
                        const Tri& t = tris[index];
                        // Lower bounds of its distance: to its box, to its plane.
                        const glm::vec3 outside = glm::max(glm::max(t.lo - p, p - t.hi), glm::vec3{0.f});
                        if(glm::dot(outside, outside) >= best * best || std::fabs(glm::dot(t.normal, p) - t.plane) >= best)
                        {
                            continue;
                        }
                        const glm::vec3 q = closestOnTriangle(p, t.a, t.b, t.c);
                        const float d = glm::distance(p, q);
                        if(d < best)
                        {
                            best = d;
                            if(at)
                            {
                                *at = q;
                            }
                        }
                    }
                }
            }
        }
        return best;
    }

    // Where a ray from `from` along the unit `dir` first meets a triangle (Moller-Trumbore), within `reach`.
    [[nodiscard]] bool ray(const glm::vec3& from, const glm::vec3& dir, float reach, float& hit, glm::vec3* normal = nullptr) const
    {
        hit = reach;
        bool found = false;
        const glm::vec3 end = from + dir * reach;
        const glm::vec3 rlo = glm::min(from, end), rhi = glm::max(from, end);
        for(const Tri& t : tris)
        {
            if(glm::any(glm::lessThan(t.hi, rlo)) || glm::any(glm::greaterThan(t.lo, rhi)))
            {
                continue;
            }
            const glm::vec3 e1 = t.b - t.a, e2 = t.c - t.a;
            const glm::vec3 pv = glm::cross(dir, e2);
            const float det = glm::dot(e1, pv);
            if(std::fabs(det) < 1e-12f)
            {
                continue;
            }
            const float inv = 1.f / det;
            const glm::vec3 tv = from - t.a;
            const float u = glm::dot(tv, pv) * inv;
            if(u < 0.f || u > 1.f)
            {
                continue;
            }
            const glm::vec3 qv = glm::cross(tv, e1);
            const float v = glm::dot(dir, qv) * inv;
            const float s = glm::dot(e2, qv) * inv;
            if(v < 0.f || u + v > 1.f || s < 0.f || s >= hit)
            {
                continue;
            }
            hit = s;
            if(normal)
            {
                *normal = glm::dot(t.normal, dir) > 0.f ? -t.normal : t.normal;
            }
            found = true;
        }
        return found;
    }
};

Shape::Shape() = default;
Shape::~Shape() = default;
Shape::Shape(Shape&&) noexcept = default;
Shape& Shape::operator=(Shape&&) noexcept = default;

namespace
{

// ----------------------------------------------------------------------------
// A solve's view of the held thing: its shape, and the hand's (rig) space taken into the shape's real units.

struct Target
{
    Shape::Space* space;
    glm::mat4 base;      // the rig (as the controller has the hand) to the shape's real units
    float scale;         // hand units per real unit (the least of the axes', if not the same: distances never overstated)
    Shape::Space* extra{nullptr}; // another thing in the way (the other hand, for a cup), in rig units
    glm::mat4 extraBase{1.f};
    float allowance{0.f}; // hand units it may be sunk into, more than the held thing
    glm::mat4 rigToReal{1.f}, extraToReal{1.f}; // with the hand turned and moved as the solve has it

    // The hand turned by `turn` about the palm's middle, then moved by `move`.
    void place(const glm::quat& turn, const glm::vec3& move);
};

int probes = 0; // this solve's (vr_grasp_bench)

// The distance (hand units) from the rig point `p` to the held thing (or the other thing in the way, less its
// allowance), at most `limit` hand units.
[[nodiscard]] float nearest(Target& target, const glm::vec3& p, float limit)
{
    float d = target.scale * target.space->nearest(glm::vec3{target.rigToReal * glm::vec4{p, 1.f}}, limit / target.scale);
    if(target.extra)
    {
        d = std::fmin(d, target.extra->nearest(glm::vec3{target.extraToReal * glm::vec4{p, 1.f}}, limit) + target.allowance);
    }
    return d;
}

// ----------------------------------------------------------------------------
// Closing a finger.

struct Context
{
    Target* target;
    const handrig::Pose* pose;
    float overlap;
};

// A finger at curls `c`, the joints from `firstActive` on closing: each bone's (from firstActive + 1 on) clearance
// (its spheres' least distance to the held thing, less their radius; negative: in it), and the curl its joints may
// close by before any sphere could reach it (conservative advancement: each sphere's clearance over how far a curl
// frame of those joints can move it, their turn rates times its distance from their pivots).
struct Probe
{
    float clear[handrig::jointsPerFinger + 1];
    float advance;
};

void probe(const Context& ctx, int finger, const float c[handrig::jointsPerFinger], int firstActive, Probe& out)
{
    probes++;
    const Kinematics& k = kinematics();
    handrig::Rigid seg[handrig::jointsPerFinger + 1];
    handrig::fingerSegments(*ctx.pose, finger, c, seg);
    glm::vec3 pivot[handrig::jointsPerFinger];
    for(int j = firstActive; j < handrig::jointsPerFinger; j++)
    {
        pivot[j] = seg[j](vec(handrig::data::pivots[finger][j]));
    }
    out.advance = 1e9f;
    for(float& clear : out.clear)
    {
        clear = 1e9f;
    }
    for(int b = firstActive + 1; b <= handrig::jointsPerFinger; b++)
    {
        for(const Sphere& s : k.bone[finger][b])
        {
            const glm::vec3 p = seg[b](s.c);
            const float r = s.r - ctx.overlap;
            float lever = 0.f;
            for(int j = firstActive; j < b; j++)
            {
                lever += k.rate[finger][j] * glm::distance(p, pivot[j]);
            }
            const float clear = nearest(*ctx.target, p, r + searchReach) - r;
            out.clear[b] = std::fmin(out.clear[b], clear);
            if(lever > 1e-6f)
            {
                out.advance = std::fmin(out.advance, std::fmax(clear, 0.f) / lever);
            }
        }
    }
}

[[nodiscard]] bool clearFrom(const Probe& p, int firstActive)
{
    for(int b = firstActive + 1; b <= handrig::jointsPerFinger; b++)
    {
        if(p.clear[b] < -tolerance)
        {
            return false;
        }
    }
    return true;
}

// Closes one finger (see vr_grasp.hpp). `warm`: where it held before (the thing moved a little in the hand): opened from
// there (all its joints by the same share) until clear, then closed onto it again: a few steps, and no jump to another
// hold that is as good.
void solveFinger(const Context& ctx, int finger, bool settle, const FingerStop* warm, FingerStop& out)
{
    out = FingerStop{};
    float c[3]{0.f, 0.f, 0.f};
    int firstActive = 0; // joints from this one on still close
    Probe p;
    bool started = false;
    if(warm && warm->met && !warm->startsInside)
    {
        std::copy(warm->stop, warm->stop + 3, c);
        probe(ctx, finger, c, 0, p);
        if(clearFrom(p, 0))
        {
            started = true;
        }
        else
        {
            const float open[3]{0.f, 0.f, 0.f};
            probe(ctx, finger, open, 0, p);
            if(clearFrom(p, 0))
            {
                float lo = 0.f, hi = 1.f;
                for(int h = 0; h < halvings; h++)
                {
                    const float mid = 0.5f * (lo + hi);
                    const float cs[3]{warm->stop[0] * mid, warm->stop[1] * mid, warm->stop[2] * mid};
                    probe(ctx, finger, cs, 0, p);
                    (clearFrom(p, 0) ? lo : hi) = mid;
                }
                for(int j = 0; j < 3; j++)
                {
                    c[j] = warm->stop[j] * lo;
                }
                started = true;
            }
        }
        out.fromClosed = started && warm->fromClosed;
        settle = false; // settled before: kept as it was (stable), only closed onto it again
    }
    if(!started)
    {
        c[0] = c[1] = c[2] = 0.f;
        probe(ctx, finger, c, 0, p);
    }
    if(!started && !clearFrom(p, 0))
    {
        // Open, the finger is already in it (a gun's frame, where the straight fingers point). Then it closes from
        // the other end: the tightest curl it is clear at, opening from the full fist (a thick grip through the
        // fist: the fingers open round it), and from there joint by joint as below.
        constexpr float step = 0.5f;
        float free = -1.f;
        for(float t = maxCurl; t > 0.f; t -= step)
        {
            const float cs[3]{t, t, t};
            probe(ctx, finger, cs, 0, p);
            if(clearFrom(p, 0))
            {
                free = t;
                break;
            }
        }
        if(free < 0.f)
        {
            out.startsInside = true;
            return;
        }
        if(free < maxCurl)
        {
            float lo = free, hi = std::min(free + step, maxCurl);
            for(int h = 0; h < halvings; h++)
            {
                const float mid = 0.5f * (lo + hi);
                const float cs[3]{mid, mid, mid};
                probe(ctx, finger, cs, 0, p);
                (clearFrom(p, 0) ? lo : hi) = mid;
            }
            free = lo;
        }
        out.fromClosed = true;
        c[0] = c[1] = c[2] = free;
    }

    // Closing: the joints from firstActive on together (each by the same step), each step as far as no sphere can
    // reach anything (conservative advancement).
    for(int steps = 0; firstActive < handrig::jointsPerFinger && steps < maxSteps; steps++)
    {
        bool closing = false;
        for(int j = firstActive; j < handrig::jointsPerFinger; j++)
        {
            closing = closing || c[j] < maxCurl;
        }
        if(!closing)
        {
            break;
        }
        probe(ctx, finger, c, firstActive, p);
        // The most distal bone touching stops the joints that move it; those past it go on.
        int touching = 0;
        for(int b = handrig::jointsPerFinger; b > firstActive; b--)
        {
            if(p.clear[b] <= tolerance)
            {
                touching = b;
                break;
            }
        }
        if(touching)
        {
            for(int j = firstActive; j < touching; j++)
            {
                out.stop[j] = c[j];
            }
            out.met = true;
            firstActive = touching;
            continue;
        }
        const float step = std::fmax(p.advance, leastStep);
        for(int j = firstActive; j < handrig::jointsPerFinger; j++)
        {
            c[j] = std::fmin(c[j] + step, maxCurl);
        }
    }
    for(int j = firstActive; j < handrig::jointsPerFinger; j++)
    {
        out.stop[j] = c[j]; // free to the fist (or where it was after the most steps)
    }

    // Then it settles round what it met: a joint closes further while a joint past it opens as much as that needs
    // (a fingertip slides along the surface as the knuckles close), as long as that closes the finger more, knuckles
    // first; the joints are never past the tightest fist.
    if(out.met && settle)
    {
        float s[3]{out.stop[0], out.stop[1], out.stop[2]};
        constexpr float weight[3] = {1.2f, 1.f, 0.8f};
        const auto value = [&](const float* v) { return weight[0] * v[0] + weight[1] * v[1] + weight[2] * v[2]; };
        for(const float delta : {0.3f, 0.15f})
        {
            for(int pass = 0; pass < 2; pass++)
            {
                bool better = false;
                for(int j = 0; j < 3 && !better; j++)
                {
                    for(int kk = j; kk < 3 && !better; kk++)
                    {
                        float t[3]{s[0], s[1], s[2]};
                        t[j] += delta;
                        if(kk > j)
                        {
                            t[kk] -= delta;
                        }
                        if(t[j] > maxCurl || t[kk] < 0.f || value(t) <= value(s) + 1e-4f)
                        {
                            continue;
                        }
                        probe(ctx, finger, t, j, p);
                        if(clearFrom(p, j))
                        {
                            std::copy(t, t + 3, s);
                            better = true;
                        }
                    }
                }
                if(!better)
                {
                    break;
                }
            }
        }
        std::copy(s, s + 3, out.stop);
    }
}

// How well a finger holds: -1 in it at every curl, 0 touching nothing, else its closure where it stopped (0..1).
// A wrap (its joints stopped one after another: the segments round what it holds) beats a finger stopped by its tip.
[[nodiscard]] float score(const FingerStop& s)
{
    if(s.startsInside)
    {
        return -1.f;
    }
    if(!s.met)
    {
        return 0.f;
    }
    const float closure = (s.stop[0] + s.stop[1] + s.stop[2]) / (3.f * maxCurl);
    const float wrap = (s.stop[1] - s.stop[0] > 0.1f ? 0.5f : 0.f) + (s.stop[2] - s.stop[1] > 0.1f ? 0.5f : 0.f);
    return 0.6f * closure + 0.4f * wrap;
}

// The thumb's turns at its base: across the palm (opposition: about the hand's long axis, from beside the index
// finger towards the palm) and swung in the palm's plane (about its normal: positive down towards the little finger's
// side, negative up and away, over the top of what the hand holds).
struct ThumbTurn
{
    float opposition, swing;
};
constexpr ThumbTurn thumbTurns[] = {{0.f, 0.f}, {15.f, 0.f}, {30.f, 0.f}, {45.f, 0.f}, {60.f, 0.f}, {15.f, -15.f},
    {30.f, -15.f}, {45.f, -15.f}, {30.f, 15.f}, {45.f, 15.f}, {0.f, -15.f}, {15.f, -30.f}};
constexpr int thumbTurnCount = static_cast<int>(sizeof(thumbTurns) / sizeof(thumbTurns[0]));

[[nodiscard]] glm::quat thumbQuat(const ThumbTurn& t)
{
    return glm::angleAxis(glm::radians(t.swing), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(t.opposition), glm::vec3{-1.f, 0.f, 0.f});
}

// Whether a thumb turn fits the style: along the top (swung up and away), or wrapping round (not).
[[nodiscard]] bool thumbStyle(const ThumbTurn& t, bool top)
{
    return top ? t.swing < 0.f : t.swing >= 0.f;
}

// The thumb: closed at each turn of its metacarpal (of the style asked for), the one holding best (a turn costs a
// little: a thumb held naturally beats a contorted one holding a little better). Solved again (the solve before
// known): its turn only, closed from where it held.
void solveThumb(handrig::Pose& pose, const Context& ctx, const Solution* previous, bool top, FingerStop& out, glm::quat& turn,
    int& choice)
{
    float best = -1e9f;
    const glm::quat keep = pose.metacarpal;
    const bool again = previous && previous->thumbChoice >= 0 && thumbStyle(thumbTurns[previous->thumbChoice], top);
    for(int i = 0; i < thumbTurnCount; i++)
    {
        if((again && i != previous->thumbChoice) || !thumbStyle(thumbTurns[i], top))
        {
            continue;
        }
        const ThumbTurn& t = thumbTurns[i];
        pose.metacarpal = thumbQuat(t);
        FingerStop st;
        solveFinger(ctx, handrig::Thumb, true, again ? &previous->finger[handrig::Thumb] : nullptr, st);
        const float value = score(st) - 0.003f * (t.opposition + std::fabs(t.swing));
        if(!st.startsInside && value > best)
        {
            best = value;
            out = st;
            turn = pose.metacarpal;
            choice = i;
        }
    }
    pose.metacarpal = keep;
    if(best > -1e9f)
    {
        return;
    }
    // In it at every turn and curl (its base in a grip): left as the controller has it, at a natural turn.
    out = FingerStop{};
    out.startsInside = true;
    out.leastInside = true;
    choice = again ? previous->thumbChoice : top ? 5 : 1;
    turn = thumbQuat(thumbTurns[choice]);
}

// The palm's spheres' (and the thenar's) least clearance from the held thing (negative: in it).
[[nodiscard]] float palmClearance(Target& target, float overlap, bool thenar)
{
    const Kinematics& k = kinematics();
    float least = 1e9f;
    const auto test = [&](const Sphere& sp) {
        const float r = sp.r - overlap;
        least = std::fmin(least, nearest(target, sp.c, r + searchReach) - r);
    };
    for(const Sphere& sp : k.palm)
    {
        test(sp);
    }
    if(thenar)
    {
        for(const Sphere& sp : k.thenar)
        {
            test(sp);
        }
    }
    return least;
}

// The rig to the shape's real units, the hand turned by `turn` about the palm's middle, then moved by `move`.
[[nodiscard]] glm::mat4 handTo(const glm::mat4& rigToReal, const glm::quat& turn, const glm::vec3& move)
{
    const glm::vec3 c = kinematics().palmCentre;
    const glm::mat4 hand = glm::translate(glm::mat4{1.f}, c) * glm::mat4_cast(turn) * glm::translate(glm::mat4{1.f}, move - c);
    return rigToReal * hand;
}

void Target::place(const glm::quat& turn, const glm::vec3& move)
{
    rigToReal = handTo(base, turn, move);
    if(extra)
    {
        extraToReal = handTo(extraBase, turn, move);
    }
}

// The palm's place for a grip through the hand (see solve): the places tried, and the best one's move.
glm::vec3 placeInside(handrig::Pose& pose, Target& target, const Settings& settings, int& tried)
{
    const auto at = [&](const glm::vec3& move) { target.place(glm::quat{1.f, 0.f, 0.f, 0.f}, move); };
    const auto value = [&](const glm::vec3& move) {
        at(move);
        const Context ctx{&target, &pose, settings.overlap};
        float total = -0.1f * glm::length(glm::vec3{3.f * move.x, move.y, move.z});
        for(int f = handrig::Index; f < handrig::FingerCount; f++)
        {
            FingerStop st;
            solveFinger(ctx, f, false, nullptr, st);
            total += score(st);
        }
        return total;
    };
    glm::vec3 best{0.f};
    float bestValue = value(best);
    tried = 1;
    const auto consider = [&](float dx, float dz) {
        // Flush along the palm's normal: coming from as far back as it may, the last place clear before it meets the
        // grip (in quarter steps), if any within reach.
        const float reach = std::sqrt(std::fmax(settings.palmLimit * settings.palmLimit - dx * dx - dz * dz, 0.f));
        bool wasClear = false;
        float clearY = 0.f;
        for(float dy = -reach; dy <= reach + 1e-4f; dy += 0.25f)
        {
            at(glm::vec3{dx, dy, dz});
            if(palmClearance(target, settings.overlap, false) >= -tolerance)
            {
                wasClear = true;
                clearY = dy;
                continue;
            }
            if(!wasClear)
            {
                continue;
            }
            const glm::vec3 move{dx, clearY, dz};
            const float v = value(move);
            tried++;
            if(v > bestValue)
            {
                bestValue = v;
                best = move;
            }
            return;
        }
    };
    for(const float dx : {-1.f, 0.f, 1.f, 2.f})
    {
        for(const float dz : {-3.f, -2.f, -1.f, 0.f, 1.f})
        {
            consider(dx, dz);
        }
    }
    const glm::vec3 centre = best;
    for(const float ox : {-0.5f, 0.f, 0.5f})
    {
        for(const float oz : {-0.5f, 0.f, 0.5f})
        {
            if(ox != 0.f || oz != 0.f)
            {
                consider(centre.x + ox, centre.z + oz);
            }
        }
    }
    return best;
}

// ----------------------------------------------------------------------------
// The shapes' cache.

struct ShapeKey
{
    const qmodel_t* model;
    int frame;
    bool operator==(const ShapeKey& o) const { return model == o.model && frame == o.frame; }
};
struct ShapeKeyHash
{
    std::size_t operator()(const ShapeKey& k) const
    {
        return std::hash<const void*>{}(k.model) ^ (static_cast<std::size_t>(k.frame) * 2654435761u);
    }
};
struct CachedShape
{
    std::string name; // the model's (a slot reused by another model after a game change is built again)
    Shape shape;
    bool valid{false};
};
std::unordered_map<ShapeKey, CachedShape, ShapeKeyHash> shapes;

bool buildShape(const entity_t& e, int frame, Shape& out)
{
    out.tris.clear();
    out.space = std::make_unique<Shape::Space>();
    const qmodel_t* model = e.model;
    if(model->type == mod_brush)
    {
        for(int i = 0; i < model->nummodelsurfaces; i++)
        {
            const msurface_t& surf = model->surfaces[model->firstmodelsurface + i];
            const auto vertex = [&](int k) {
                const int ed = model->surfedges[surf.firstedge + k];
                const mvertex_t& v = model->vertexes[ed >= 0 ? model->edges[ed].v[0] : model->edges[-ed].v[1]];
                return glm::vec3{v.position[0], v.position[1], v.position[2]};
            };
            for(int k = 2; k < surf.numedges; k++)
            {
                out.tris.push_back({{vertex(0), vertex(k - 1), vertex(k)}});
            }
        }
    }
    else if(model->type == mod_alias)
    {
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
        if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc || hdr->numframes <= 0)
        {
            return false;
        }
        const int pose = hdr->frames[frame >= 0 && frame < hdr->numframes ? frame : 0].firstpose;
        const auto* base = reinterpret_cast<const byte*>(hdr);
        const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + pose * hdr->numverts;
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
        for(int i = 0; i + 2 < hdr->numindexes; i += 3)
        {
            Triangle t;
            for(int k = 0; k < 3; k++)
            {
                const trivertx_t& v = verts[mesh[indexes[i + k]].vertindex];
                t.p[k] = glm::vec3{v.v[0], v.v[1], v.v[2]};
            }
            out.tris.push_back(t);
        }
        // Raw vertices to the model's own units: its header's scale (per axis) and origin.
        glm::mat4& m = out.space->rawToReal;
        for(int k = 0; k < 3; k++)
        {
            m[k][k] = hdr->scale[k];
            m[3][k] = hdr->scale_origin[k];
        }
    }
    if(out.tris.size() >= 65535)
    {
        out.tris.resize(65535);
    }
    for(const Triangle& t : out.tris)
    {
        Shape::Space::Tri r;
        r.a = glm::vec3{out.space->rawToReal * glm::vec4{t.p[0], 1.f}};
        r.b = glm::vec3{out.space->rawToReal * glm::vec4{t.p[1], 1.f}};
        r.c = glm::vec3{out.space->rawToReal * glm::vec4{t.p[2], 1.f}};
        const glm::vec3 n = glm::cross(r.b - r.a, r.c - r.a);
        const float len = glm::length(n);
        r.normal = len > 1e-12f ? n / len : glm::vec3{0.f};
        r.plane = glm::dot(r.normal, r.a);
        r.lo = glm::min(r.a, glm::min(r.b, r.c));
        r.hi = glm::max(r.a, glm::max(r.b, r.c));
        out.space->tris.push_back(r);
    }
    return !out.tris.empty();
}

} // namespace

void reset()
{
    shapes.clear();
}

void makeShape(const std::vector<Triangle>& tris, Shape& out)
{
    out.tris = tris;
    if(!out.space)
    {
        out.space = std::make_unique<Shape::Space>();
    }
    Shape::Space& space = *out.space;
    space.tris.clear();
    space.cell = 0.f; // its grid made for the next solve
    for(const Triangle& t : out.tris)
    {
        Shape::Space::Tri r;
        r.a = t.p[0];
        r.b = t.p[1];
        r.c = t.p[2];
        const glm::vec3 n = glm::cross(r.b - r.a, r.c - r.a);
        const float len = glm::length(n);
        r.normal = len > 1e-12f ? n / len : glm::vec3{0.f};
        r.plane = glm::dot(r.normal, r.a);
        r.lo = glm::min(r.a, glm::min(r.b, r.c));
        r.hi = glm::max(r.a, glm::max(r.b, r.c));
        space.tris.push_back(r);
    }
}

glm::mat4 shapeToWorld(const entity_t& e, bool mirrored)
{
    float m[16];
    if(e.model && e.model->type == mod_brush)
    {
        vec3_t origin, angles{-e.angles[0], e.angles[1], e.angles[2]};
        VectorCopy(e.origin, origin);
        R_EntityMatrix(m, origin, angles, e.scale);
        VR_BrushTransform(&e, m);
    }
    else
    {
        render::entityMatrix(e, mirrored, e.scale, glm::vec3{0.f}, m);
    }
    glm::mat4 r;
    for(int c = 0; c < 16; c++)
    {
        r[c / 4][c % 4] = m[c];
    }
    return r;
}

const Shape* shapeOf(const entity_t& e, int frame)
{
    if(!e.model || (e.model->type != mod_alias && e.model->type != mod_brush))
    {
        return nullptr;
    }
    const int f = e.model->type == mod_alias ? (frame >= 0 ? frame : e.frame) : 0;
    CachedShape& cached = shapes[ShapeKey{e.model, f}];
    if(cached.name != e.model->name)
    {
        cached.name = e.model->name;
        cached.valid = buildShape(e, f, cached.shape);
    }
    return cached.valid ? &cached.shape : nullptr;
}

bool worldTriangles(const entity_t& e, bool mirrored, int frame, std::vector<Triangle>& out)
{
    out.clear();
    const Shape* shape = shapeOf(e, frame);
    if(!shape)
    {
        return false;
    }
    const glm::mat4 m = shapeToWorld(e, mirrored);
    for(const Triangle& t : shape->tris)
    {
        out.push_back({{glm::vec3{m * glm::vec4{t.p[0], 1.f}}, glm::vec3{m * glm::vec4{t.p[1], 1.f}},
            glm::vec3{m * glm::vec4{t.p[2], 1.f}}}});
    }
    return !out.empty();
}

glm::vec3 palmCentre()
{
    return kinematics().palmCentre;
}

void solve(const handrig::Pose& start, const Shape& shape, const glm::mat4& shapeToRig, const Settings& settings,
    const Solution* previous, Solution& out, Shape* extra, const glm::mat4& extraToRig, float extraOverlap)
{
    const auto t0 = std::chrono::steady_clock::now();
    const Kinematics& k = kinematics();
    out = Solution{};
    probes = 0;
    out.palmCentre = k.palmCentre;
    handrig::Pose pose = start;
    pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};
    Shape::Space& space = *shape.space;
    out.triangles = static_cast<int>(space.tris.size());

    // The rig in the shape's real units, and how many hand units one of those is (the axes' least, should the
    // shape be stretched unevenly: distances are then understated, never overstated).
    const glm::mat4 realToRig = shapeToRig * glm::inverse(space.rawToReal);
    const glm::mat4 rigToReal = glm::inverse(realToRig);
    const float scale = std::fmin(glm::length(glm::vec3{realToRig[0]}),
        std::fmin(glm::length(glm::vec3{realToRig[1]}), glm::length(glm::vec3{realToRig[2]})));
    if(!(scale > 1e-6f))
    {
        return;
    }
    // Its grid, for cells of cellHandUnits in the hand (made again if held at a quite different size).
    const float cell = cellHandUnits / scale;
    if(space.cell <= 0.f || cell < space.cell * 0.7f || cell > space.cell * 1.4f)
    {
        space.buildGrid(cell);
    }
    Target target{&space, rigToReal, scale};
    if(extra && extra->space && !extra->space->tris.empty())
    {
        if(extra->space->cell <= 0.f)
        {
            extra->space->buildGrid(cellHandUnits);
        }
        target.extra = extra->space.get();
        target.extraBase = glm::inverse(extraToRig);
        target.allowance = std::fmax(extraOverlap - settings.overlap, 0.f);
    }
    target.place(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3{0.f});

    // Whether the palm is in it (a gun's grip through the hand).
    const bool inside = palmClearance(target, settings.overlap, false) < -tolerance;

    // The palm turned (at most palmTurnLimit degrees, about its middle) to face the surface in front of it: the held
    // thing turned the other way round it, for everything below. Not when the palm is in it: nothing to face. The
    // surface: the one a ray from the palm's middle along its normal meets, or else the nearest to its middle on its
    // side (beside the fist: a big thing held by its edge).
    const glm::vec3 up{0.f, 1.f, 0.f};
    if(settings.palmTurnLimit > 0.f && !inside)
    {
        const glm::vec3 from{rigToReal * glm::vec4{k.palmCentre, 1.f}};
        const glm::vec3 dir = glm::normalize(glm::vec3{rigToReal * glm::vec4{up, 0.f}});
        float hit;
        glm::vec3 n{0.f};
        bool facing = space.ray(from, dir, (settings.palmLimit + 8.f) / scale, hit, &n);
        if(facing)
        {
            n = glm::normalize(glm::vec3{glm::transpose(rigToReal) * glm::vec4{n, 0.f}}); // a normal into the rig
        }
        else
        {
            glm::vec3 q;
            const float reach = (settings.palmLimit + 4.f) / scale;
            const float d = space.nearest(from, reach, &q);
            if(d < reach && d > 1e-4f)
            {
                const glm::vec3 qr{realToRig * glm::vec4{q, 1.f}};
                if(glm::dot(qr - k.palmCentre, up) > 0.f)
                {
                    n = glm::normalize(k.palmCentre - qr);
                    facing = true;
                }
            }
        }
        if(facing)
        {
            const glm::vec3 want = -n;
            const float angle = std::acos(std::clamp(glm::dot(up, want), -1.f, 1.f));
            if(angle > glm::radians(1.f))
            {
                const glm::vec3 axis = glm::normalize(glm::cross(up, want));
                out.palmTurn = glm::angleAxis(std::min(angle, glm::radians(settings.palmTurnLimit)), axis);
            }
        }
    }

    // The palm flush on it (step 4): moved along its normal from as far back as it may go (if clear there), towards it
    // until its spheres meet it (conservative advancement: each step as far as they are clear), no further in than it
    // may go. Not when in it.
    if(settings.palmLimit > 0.f && !inside)
    {
        float y = -settings.palmLimit;
        target.place(out.palmTurn, up * y);
        if(palmClearance(target, settings.overlap, settings.thenar) >= -tolerance)
        {
            for(int step = 0; step < 32 && y < settings.palmLimit; step++)
            {
                target.place(out.palmTurn, up * y);
                const float clear = palmClearance(target, settings.overlap, settings.thenar);
                if(clear <= 0.05f)
                {
                    break;
                }
                y = std::fmin(y + clear, settings.palmLimit);
            }
            // Nothing met within reach: where it is (the palm doesn't go looking).
            out.palm = y < settings.palmLimit ? up * y : glm::vec3{0.f};
        }
    }

    // A grip through the hand (a weapon's): the palm moved a little along the fingers (x) and the grip (z), flush along
    // its normal there, to where the fingers hold best, closed coarsely at each (as round 21's first solver did); solved
    // again, the place before (the weapon doesn't move in the hand).
    if(inside && settings.palmLimit > 0.f)
    {
        out.palm = previous ? previous->palm : placeInside(pose, target, settings, out.places);
    }
    target.place(out.palmTurn, out.palm);

    // The fingers.
    const Context ctx{&target, &pose, settings.overlap};
    solveThumb(pose, ctx, previous, settings.thumbTop, out.finger[handrig::Thumb], out.thumbTurn, out.thumbChoice);
    for(int f = handrig::Index; f < handrig::FingerCount; f++)
    {
        solveFinger(ctx, f, true, previous ? &previous->finger[f] : nullptr, out.finger[f]);
    }
    out.probes = probes;
    out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

float pathCurl(float curl)
{
    const float c = std::fmin(std::fmax(curl, 0.f), 5.f);
    return c <= maxCurl ? c : 2.f * maxCurl - c;
}

void curls(const FingerStop& stop, float curl, float engage, float out[handrig::jointsPerFinger])
{
    const float path = pathCurl(curl);
    if(!stop.met)
    {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            out[j] = path;
        }
        return;
    }
    const float reach = path + std::fmin(std::fmax(engage, 0.f), 1.f) * (maxCurl - path);
    for(int j = 0; j < handrig::jointsPerFinger; j++)
    {
        out[j] = std::fmin(stop.stop[j], reach);
    }
}

bool gripChannel(const handrig::Pose& pose, glm::vec3& point, glm::vec3& dir, float& radius)
{
    // Each finger half closed: the circle through its three segments' middles (their middle spheres), its centre
    // and radius; the channel the line through the centres (least squares), less the spheres' radius.
    const Kinematics& k = kinematics();
    constexpr float curl = 2.5f;
    glm::vec3 centres[handrig::FingerCount];
    int count = 0;
    float radii = 0.f;
    for(int f = handrig::Index; f < handrig::FingerCount; f++)
    {
        const float c[3]{curl, curl, curl};
        handrig::Rigid seg[handrig::jointsPerFinger + 1];
        handrig::fingerSegments(pose, f, c, seg);
        glm::vec3 p[3];
        float r = 0.f;
        bool ok = true;
        for(int b = 1; b <= 3; b++)
        {
            const std::vector<Sphere>& row = k.bone[f][b];
            if(row.empty())
            {
                ok = false;
                break;
            }
            const Sphere& s = row[row.size() / 2];
            p[b - 1] = seg[b](s.c);
            r += s.r / 3.f;
        }
        if(!ok)
        {
            continue;
        }
        // The circle through three points: its centre (in their plane).
        const glm::vec3 a = p[0] - p[2], b = p[1] - p[2];
        const glm::vec3 axb = glm::cross(a, b);
        const float d = 2.f * glm::dot(axb, axb);
        if(d < 1e-9f)
        {
            continue;
        }
        const glm::vec3 centre = p[2] + glm::cross(glm::dot(a, a) * b - glm::dot(b, b) * a, axb) / d;
        centres[count++] = centre;
        radii += (glm::distance(centre, p[0]) + glm::distance(centre, p[1]) + glm::distance(centre, p[2])) / 3.f - r;
    }
    if(count < 2)
    {
        return false;
    }
    point = glm::vec3{0.f};
    for(int i = 0; i < count; i++)
    {
        point += centres[i];
    }
    point /= static_cast<float>(count);
    glm::mat3 cov{0.f};
    for(int i = 0; i < count; i++)
    {
        cov += glm::outerProduct(centres[i] - point, centres[i] - point);
    }
    dir = glm::normalize(centres[0] - centres[count - 1]); // from the little finger's side to the index's
    for(int i = 0; i < 16; i++)
    {
        const glm::vec3 next = cov * dir;
        if(glm::length(next) < 1e-9f)
        {
            break;
        }
        dir = glm::normalize(next);
    }
    if(glm::dot(dir, centres[0] - centres[count - 1]) < 0.f)
    {
        dir = -dir;
    }
    radius = std::fmax(radii / static_cast<float>(count), 0.f);
    return true;
}

void posedSpheres(const handrig::Pose& pose, std::vector<glm::vec4>& out)
{
    const Kinematics& k = kinematics();
    out.clear();
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        handrig::Rigid seg[handrig::jointsPerFinger + 1];
        handrig::fingerSegments(pose, f, pose.curl[f], seg);
        for(int b = 1; b <= handrig::jointsPerFinger; b++)
        {
            for(const Sphere& s : k.bone[f][b])
            {
                out.push_back(glm::vec4{seg[b](s.c), s.r});
            }
        }
    }
    for(const Sphere& s : k.palm)
    {
        out.push_back(glm::vec4{s.c, s.r});
    }
}

void spheres_f()
{
    const Kinematics& k = kinematics();
    constexpr const char* names[handrig::FingerCount] = {"thumb", "index", "middle", "ring", "pinky"};
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        Con_Printf("%s: joint rates %.1f %.1f %.1f deg/frame\n", names[f], glm::degrees(k.rate[f][0]), glm::degrees(k.rate[f][1]),
            glm::degrees(k.rate[f][2]));
        for(int b = 1; b <= handrig::jointsPerFinger; b++)
        {
            for(const Sphere& s : k.bone[f][b])
            {
                Con_Printf("  bone %d: (%.2f %.2f %.2f) r %.2f\n", b, s.c.x, s.c.y, s.c.z, s.r);
            }
        }
    }
    Con_Printf("palm: %d spheres, thenar %d; its middle (%.2f %.2f %.2f)\n", static_cast<int>(k.palm.size()),
        static_cast<int>(k.thenar.size()), k.palmCentre.x, k.palmCentre.y, k.palmCentre.z);
}

} // namespace qvr::grasp
