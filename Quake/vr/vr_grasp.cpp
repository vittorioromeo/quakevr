// vr_grasp.cpp -- see vr_grasp.hpp.

#include "vr_grasp.hpp"
#include "vr_jobs.hpp"
#include "vr_render.hpp"
#include "vr_api_render.h"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_mem.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/matrix_transform.hpp>


namespace qvr::grasp
{
namespace
{

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
// The hand as spheres (made with the mesh: make_hand_rig.py; following an edited mesh, handrig::rig()): each finger
// segment (bones 1..3) as a row of spheres along its side towards the palm, as wide as it is (rig space, at rest: each
// bone's own frame there); the palm's side as spheres just under its skin, the ball of the thumb likewise; and the
// palm's middle (fixed: cups are placed from it).

struct Sphere
{
    glm::vec3 c;
    float r;
};

struct Kinematics
{
    za::Vector<Sphere> bone[handrig::FingerCount][handrig::jointsPerFinger + 1];
    float rate[handrig::FingerCount][handrig::jointsPerFinger]{}; // radians a curl frame, at most
    za::Vector<Sphere> palm;
    za::Vector<Sphere> thenar; // the ball of the thumb
    glm::vec3 palmCentre{0.f};
};

Kinematics buildKinematics()
{
    const handrig::Rig& rig = handrig::rig();
    Kinematics k;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            k.rate[f][j] = handrig::jointRate(f, j);
        }
    }
    for(const handrig::SegmentSphere& s : rig.segmentSpheres)
    {
        k.bone[s.finger][s.bone].pushBack({s.c, s.r});
    }
    for(const handrig::Sphere& s : rig.palmSpheres)
    {
        k.palm.pushBack({s.c, s.r});
    }
    for(const handrig::Sphere& s : rig.thenarSpheres)
    {
        k.thenar.pushBack({s.c, s.r});
    }
    k.palmCentre = vec(handrig::data::palmCentre);
    return k;
}

// Built again when another rig is put in use (vr_hand_reload): keyed by the rig's generation.
struct KinematicsCache
{
    Kinematics k;
    unsigned built = 0; // the rig's generation k is of (0: none yet)
    auto members() { return mem::list(k, built); }
};
[[nodiscard]] za::SizeT heldBytes(const Kinematics& k) // (vr_mem.hpp)
{
    return mem::heldBytes(k.bone) + mem::heldBytes(k.palm) + mem::heldBytes(k.thenar);
}
mem::Cache<KinematicsCache> kinematicsCache{"grasp kinematics", mem::Never};

const Kinematics& kinematics()
{
    Kinematics& k = kinematicsCache.k;
    unsigned& built = kinematicsCache.built;
    if(built != handrig::generation())
    {
        k = buildKinematics();
        built = handrig::generation();
    }
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
    za::Vector<Tri> tris;
    glm::mat4 rawToReal{1.f};
    // +1 if the triangles are wound outwards (their normals, b - a by c - a, point out of the model), -1 inwards (Quake's
    // own winding, as its models are drawn): the sign of the volume they close (measureWinding). For signedDistance,
    // rayHit and surfaceDistance.
    float outward{1.f};

    float cell{0.f}; // the grid's (real units), for the hand's size it was made at
    glm::vec3 lo{0.f}, hi{0.f}; // the triangles' box
    int size[3]{1, 1, 1};
    za::Vector<za::U32> first; // per cell, its first item; one more at the end
    // Per cell, its triangles (in their order): the index (the low 16 bits), and whether this cell is the triangle's
    // first on x, y, z (bits 16, 17, 18). A query looks at a triangle only in the first of its cells it covers, told by
    // those bits and the query's own first cells: no state kept between queries (they run on several threads at once,
    // vr_jobs.hpp), and a triangle's other cells skipped without reading it.
    za::Vector<za::U32> items;

    [[nodiscard]] int cellOf(float v, int k) const
    {
        return za::clamp(static_cast<int>((v - lo[k]) / cell), 0, size[k] - 1);
    }

    void buildGrid(float cellSize)
    {
        cell = cellSize;
        hi = glm::vec3{-1e9f};
        lo = glm::vec3{1e9f};
        for(const Tri& t : tris)
        {
            lo = glm::min(lo, t.lo);
            hi = glm::max(hi, t.hi);
        }
        za::SizeT cells = 1;
        for(int k = 0; k < 3; k++)
        {
            size[k] = za::clamp(static_cast<int>((hi[k] - lo[k]) / cell) + 1, 1, 256);
            cells *= static_cast<za::SizeT>(size[k]);
        }
        const auto each = [&](const Tri& t, auto&& f) {
            int from[3], to[3];
            for(int k = 0; k < 3; k++)
            {
                from[k] = cellOf(t.lo[k], k);
                to[k] = cellOf(t.hi[k], k);
            }
            for(int z = from[2]; z <= to[2]; z++)
            {
                for(int y = from[1]; y <= to[1]; y++)
                {
                    for(int x = from[0]; x <= to[0]; x++)
                    {
                        const za::U32 firsts = (x == from[0] ? 1u << 16 : 0u) | (y == from[1] ? 1u << 17 : 0u) |
                                                     (z == from[2] ? 1u << 18 : 0u);
                        f((static_cast<za::SizeT>(z) * static_cast<za::SizeT>(size[1]) + static_cast<za::SizeT>(y)) *
                              static_cast<za::SizeT>(size[0]) +
                              static_cast<za::SizeT>(x),
                            firsts);
                    }
                }
            }
        };
        za::Vector<za::U32> counts(cells, 0u);
        for(const Tri& t : tris)
        {
            each(t, [&](za::SizeT c, za::U32) { counts[c]++; });
        }
        first.clear();
        first.resize(cells + 1, 0u);
        for(za::SizeT c = 0; c < cells; c++)
        {
            first[c + 1] = first[c] + counts[c];
        }
        items.resize(first[cells]);
        za::fill(counts.begin(), counts.end(), 0u);
        for(za::SizeT i = 0; i < tris.size(); i++)
        {
            each(tris[i], [&](za::SizeT c, za::U32 firsts) {
                items[first[c] + counts[c]++] = static_cast<za::U32>(i) | firsts;
            });
        }
    }

    // The distance from `p` to the nearest triangle, at most `limit` (beyond it, `limit`, or more: the distance to the
    // triangles' box when that is farther, a lower bound, so that a finger in open air steps as far as it may at once);
    // its point in `at`, its normal (as wound) in `normal`.
    [[nodiscard]] float nearest(const glm::vec3& p, float limit, glm::vec3* at = nullptr, glm::vec3* normal = nullptr) const
    {
        const glm::vec3 away = glm::max(glm::max(lo - p, p - hi), glm::vec3{0.f});
        const float boxDistance2 = glm::dot(away, away);
        if(boxDistance2 >= limit * limit)
        {
            return za::sqrt(boxDistance2);
        }
        float best = limit;
        int from[3], to[3];
        for(int k = 0; k < 3; k++)
        {
            from[k] = static_cast<int>(za::floor((p[k] - limit - lo[k]) / cell));
            to[k] = static_cast<int>(za::floor((p[k] + limit - lo[k]) / cell));
            if(to[k] < 0 || from[k] >= size[k])
            {
                return best;
            }
            from[k] = za::max(from[k], 0);
            to[k] = za::min(to[k], size[k] - 1);
        }
        for(int z = from[2]; z <= to[2]; z++)
        {
            for(int y = from[1]; y <= to[1]; y++)
            {
                const za::SizeT row =
                    (static_cast<za::SizeT>(z) * static_cast<za::SizeT>(size[1]) + static_cast<za::SizeT>(y)) *
                    static_cast<za::SizeT>(size[0]);
                for(int x = from[0]; x <= to[0]; x++)
                {
                    // A triangle is looked at in the first of its cells the query covers (the scan's order: z, y, x):
                    // on each axis, its own first cell or the query's.
                    const za::U32 queryFirsts = (x == from[0] ? 1u << 16 : 0u) | (y == from[1] ? 1u << 17 : 0u) |
                                                      (z == from[2] ? 1u << 18 : 0u);
                    const za::SizeT c = row + static_cast<za::SizeT>(x);
                    for(za::U32 i = first[c]; i < first[c + 1]; i++)
                    {
                        const za::U32 item = items[i];
                        if(((item | queryFirsts) & (7u << 16)) != (7u << 16))
                        {
                            continue;
                        }
                        const Tri& t = tris[item & 0xffffu];
                        // Lower bounds of its distance: to its box, to its plane.
                        const glm::vec3 outside = glm::max(glm::max(t.lo - p, p - t.hi), glm::vec3{0.f});
                        if(glm::dot(outside, outside) >= best * best || za::fabs(glm::dot(t.normal, p) - t.plane) >= best)
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
                            if(normal)
                            {
                                *normal = t.normal;
                            }
                        }
                    }
                }
            }
        }
        return best;
    }

    // Where a ray from `from` along the unit `dir` first meets a triangle (Moller-Trumbore), within `reach`.
    // `normal`: the triangle met's, facing `from`; `wound`: as wound.
    [[nodiscard]] bool ray(const glm::vec3& from, const glm::vec3& dir, float reach, float& hit, glm::vec3* normal = nullptr,
        glm::vec3* wound = nullptr) const
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
            if(za::fabs(det) < 1e-12f)
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
            if(wound)
            {
                *wound = t.normal;
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
// The triangles' winding (Space::outward): the sign of the volume they close (the divergence theorem: a sixth of each
// triangle's a . (b x c)), about their box's middle. A model is closed, or nearly: its sign is its winding's.
void measureWinding(Shape::Space& space)
{
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const Shape::Space::Tri& t : space.tris)
    {
        lo = glm::min(lo, t.lo);
        hi = glm::max(hi, t.hi);
    }
    const glm::vec3 mid = (lo + hi) * 0.5f;
    double volume = 0.0;
    for(const Shape::Space::Tri& t : space.tris)
    {
        volume += static_cast<double>(glm::dot(t.a - mid, glm::cross(t.b - mid, t.c - mid)));
    }
    space.outward = volume < 0.0 ? -1.f : 1.f;
}
} // namespace

za::SizeT heldBytes(const Shape& s)
{
    za::SizeT n = s.tris.capacity() * sizeof(Triangle);
    if(const Shape::Space* p = s.space.get())
    {
        n += sizeof(Shape::Space) + p->tris.capacity() * sizeof(Shape::Space::Tri) + p->first.capacity() * sizeof(za::U32) +
             p->items.capacity() * sizeof(za::U32);
    }
    return n;
}

namespace
{

// ----------------------------------------------------------------------------
// A solve's view of the held thing: its shape, and the hand's (rig) space taken into the shape's real units.

struct Target
{
    const Shape::Space* space;
    glm::mat4 base;      // the rig (as the controller has the hand) to the shape's real units
    float scale;         // hand units per real unit (the least of the axes', if not the same: distances never overstated)
    const Shape::Space* extra{nullptr}; // another thing in the way (the other hand, for a cup), in rig units
    glm::mat4 extraBase{1.f};
    float allowance{0.f}; // hand units it may be sunk into, more than the held thing
    glm::mat4 rigToReal{1.f}, extraToReal{1.f}; // with the hand turned and moved as the solve has it

    // The hand turned by `turn` about the palm's middle, then moved by `move`.
    void place(const glm::quat& turn, const glm::vec3& move);
};

// The distance (hand units) from the rig point `p` to the held thing (or the other thing in the way, less its
// allowance), at most `limit` hand units.
[[nodiscard]] float nearest(const Target& target, const glm::vec3& p, float limit)
{
    float d = target.scale * target.space->nearest(glm::vec3{target.rigToReal * glm::vec4{p, 1.f}}, limit / target.scale);
    if(target.extra)
    {
        d = za::fmin(d, target.extra->nearest(glm::vec3{target.extraToReal * glm::vec4{p, 1.f}}, limit) + target.allowance);
    }
    return d;
}

// ----------------------------------------------------------------------------
// Closing a finger.

// A finger's solve: what it closes on, the hand (its thumb turned as tried), and its probes' count (vr_grasp_bench).
// Solves run on several threads at once (solve): each has its own context, and shares only what it reads.
struct Context
{
    const Target* target;
    const handrig::Pose* pose;
    float overlap;
    int* probes;
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
    (*ctx.probes)++;
    const Kinematics& k = kinematics();
    handrig::Rigid seg[handrig::jointsPerFinger + 1];
    handrig::fingerSegments(*ctx.pose, finger, c, seg);
    glm::vec3 pivot[handrig::jointsPerFinger];
    for(int j = firstActive; j < handrig::jointsPerFinger; j++)
    {
        pivot[j] = seg[j](handrig::rig().pivot[finger][j]);
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
            out.clear[b] = za::fmin(out.clear[b], clear);
            if(lever > 1e-6f)
            {
                out.advance = za::fmin(out.advance, za::fmax(clear, 0.f) / lever);
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
        za::copy(warm->stop, warm->stop + 3, c);
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
            float lo = free, hi = za::min(free + step, maxCurl);
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
        const float step = za::fmax(p.advance, leastStep);
        for(int j = firstActive; j < handrig::jointsPerFinger; j++)
        {
            c[j] = za::fmin(c[j] + step, maxCurl);
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
                            za::copy(t, t + 3, s);
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
        za::copy(s, s + 3, out.stop);
    }
}

// The thumb Outside what the palm holds (ThumbStyle::Outside: In the Palm): one in it open (on the ball of the thumb,
// the thing sits on its base) closed from the fist, or closing past it into the gap under it, is tucked between the palm
// and it (NOTES.md vrfiringrange_2026-10-01_00-23-13 and 00-24-47). If no deeper than `sink` hand units in it open, it
// lies along it open instead (its base sunk that little), which is how a thumb holds a stone or a brick from the side.
void lieAlong(const Context& ctx, float sink, FingerStop& st)
{
    if(!(sink > 0.f) || (st.met && !st.fromClosed && !st.startsInside))
    {
        return; // closed onto it from open: it wraps it already
    }
    const float open[handrig::jointsPerFinger]{0.f, 0.f, 0.f};
    Probe p;
    probe(ctx, handrig::Thumb, open, 0, p);
    float least = 1e9f;
    for(int b = 1; b <= handrig::jointsPerFinger; b++)
    {
        least = za::fmin(least, p.clear[b]);
    }
    if(least < -tolerance - sink || least > tolerance)
    {
        return; // in it too deep (left as it was), or clear of it open (free: nothing to lie along)
    }
    st = FingerStop{};
    za::fill(st.stop, st.stop + handrig::jointsPerFinger, 0.f);
    st.met = true;
    st.lying = za::fmax(-least, 0.f);
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
    if(s.lying >= 0.f)
    {
        return 0.3f - 0.2f * s.lying; // lying along it (lieAlong): the less sunk in it, the better
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
// (Round 21, second pass: no longer 60 degrees across nor 30 up: the skin round the thumb's base stretched thin.)
// From wideTurns on, the wide ones (Settings::thumbWide): swung far up and away from the index, the thumb opened wide
// round something thick in the palm (a hand grenade, a brick) whose side its usual turns are all in (NOTES.md
// vrfiringrange_2026-10-01_12-08-49: "the thumb kind of clips a little bit into it"); tried for In the Palm only.
constexpr ThumbTurn thumbTurns[] = {{0.f, 0.f}, {15.f, 0.f}, {30.f, 0.f}, {45.f, 0.f}, {15.f, -15.f}, {30.f, -15.f},
    {45.f, -15.f}, {30.f, 15.f}, {45.f, 15.f}, {0.f, -15.f}, {0.f, -45.f}, {15.f, -45.f}, {0.f, -60.f}, {15.f, -60.f},
    {30.f, -60.f}};
constexpr int thumbTurnCount = static_cast<int>(sizeof(thumbTurns) / sizeof(thumbTurns[0]));
constexpr int wideTurns = 10;

[[nodiscard]] glm::quat thumbQuat(const ThumbTurn& t)
{
    return glm::angleAxis(glm::radians(t.swing), glm::vec3{0.f, 1.f, 0.f}) *
           glm::angleAxis(glm::radians(t.opposition), glm::vec3{-1.f, 0.f, 0.f});
}

// How the thumb holds: wrapped round (a grip, a handle), along the top (swung up and away: a hotspot's Thumb on Top), or
// outside what the palm holds (In the Palm: a rock, a brick; any turn, never tucked between the palm and it; Wide: the
// wide turns too).
enum class ThumbStyle
{
    Wrap,
    Top,
    Outside,
    OutsideWide,
};

[[nodiscard]] ThumbStyle thumbStyleOf(const Settings& s)
{
    return s.thumbTop       ? ThumbStyle::Top
           : s.thumbOutside ? (s.thumbWide ? ThumbStyle::OutsideWide : ThumbStyle::Outside)
                            : ThumbStyle::Wrap;
}

[[nodiscard]] bool outside(ThumbStyle style)
{
    return style == ThumbStyle::Outside || style == ThumbStyle::OutsideWide;
}

// Whether thumb turn `i` fits the style.
[[nodiscard]] bool thumbStyle(int i, ThumbStyle style)
{
    if(i >= wideTurns)
    {
        return style == ThumbStyle::OutsideWide;
    }
    return outside(style) || (style == ThumbStyle::Top ? thumbTurns[i].swing < 0.f : thumbTurns[i].swing >= 0.f);
}

// The thumb: closed at each turn of its metacarpal (of the style asked for), the one holding best (a turn costs a
// little: a thumb held naturally beats a contorted one holding a little better). Solved again (the solve before
// known): its turn only, closed from where it held. The turns tried (solve closes them on the game's threads, each with
// its own pose), then the choice among them (in their order: the first of equals).
[[nodiscard]] bool thumbTried(int i, const Solution* previous, ThumbStyle style)
{
    const bool again = previous && previous->thumbChoice >= 0 && thumbStyle(previous->thumbChoice, style);
    return !((again && i != previous->thumbChoice) || !thumbStyle(i, style));
}

// `tucked`: Outside (tuckedThumbs), the turns whose thumb ends tucked between the palm and what it holds: taken only if
// no turn's thumb holds it from outside. OutsideWide: the wide turns (wideTurns on) are taken only when no usual turn
// holds it cleanly from outside (met, not in it nor tucked, lying along it no deeper than cleanLie) and a wide one does,
// and then only a clean one: the thumb wraps round the outside, not into it (a grenade's side, a brick's).
constexpr float cleanLie = 0.1f; // hand units a thumb lying along it may be in it and still count as clean (~1 mm)
void chooseThumb(const FingerStop (&tried)[thumbTurnCount], const bool (&tucked)[thumbTurnCount], const Solution* previous,
    ThumbStyle style, FingerStop& out, glm::quat& turn, int& choice)
{
    const auto clean = [&](int i) {
        return tried[i].met && !tried[i].startsInside && !tucked[i] && tried[i].lying <= cleanLie;
    };
    bool anyUsual = false, usualClean = false, wideClean = false;
    for(int i = 0; i < thumbTurnCount; i++)
    {
        if(thumbTried(i, previous, style))
        {
            const bool usual = i < wideTurns;
            anyUsual = anyUsual || usual;
            usualClean = usualClean || (usual && clean(i));
            wideClean = wideClean || (!usual && clean(i));
        }
    }
    const bool wide = !anyUsual || (wideClean && !usualClean);
    const auto considered = [&](int i) {
        return thumbTried(i, previous, style) && (i < wideTurns ? !wide : wide && (!anyUsual || clean(i)));
    };
    bool outsideMet = false;
    for(int i = 0; i < thumbTurnCount && outside(style); i++)
    {
        outsideMet = outsideMet || (considered(i) && tried[i].met && !tried[i].startsInside && !tucked[i]);
    }
    float best = -1e9f;
    for(int i = 0; i < thumbTurnCount; i++)
    {
        if(!considered(i))
        {
            continue;
        }
        const ThumbTurn& t = thumbTurns[i];
        const FingerStop& st = tried[i];
        const float value = score(st) - 0.003f * (t.opposition + za::fabs(t.swing));
        if(!st.startsInside && !(outsideMet && (tucked[i] || !st.met)) && value > best)
        {
            best = value;
            out = st;
            turn = thumbQuat(t);
            choice = i;
        }
    }
    if(best > -1e9f)
    {
        return;
    }
    // In it at every turn and curl (its base in a grip): left as the controller has it, at a natural turn.
    const bool again = previous && previous->thumbChoice >= 0 && thumbStyle(previous->thumbChoice, style);
    out = FingerStop{};
    out.startsInside = true;
    out.leastInside = true;
    choice = again ? previous->thumbChoice : style == ThumbStyle::Top ? 4 : 1;
    turn = thumbQuat(thumbTurns[choice]);
}

// The palm's spheres' (and the thenar's) least clearance from the held thing (negative: in it).
[[nodiscard]] float palmClearance(const Target& target, float overlap, bool thenar)
{
    const Kinematics& k = kinematics();
    float least = 1e9f;
    const auto test = [&](const Sphere& sp) {
        const float r = sp.r - overlap;
        least = za::fmin(least, nearest(target, sp.c, r + searchReach) - r);
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

// Outside (ThumbStyle::Outside): the thumb turns whose closed thumb's tip is nearer the palm than half way to the middle
// of what the palm holds (measured towards it from the palm's middle): under it, between the palm and it, not round its
// side (NOTES.md vrfiringrange_2026-10-01_00-23-13: a rock's or a brick's thumb squashed against the palm). A thumb in
// it open, closed from the fist, is always under it.
void tuckedThumbs(const handrig::Pose& pose, const Target& target, const FingerStop (&thumbs)[thumbTurnCount],
    bool (&tucked)[thumbTurnCount])
{
    const Shape::Space& space = *target.space;
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const Shape::Space::Tri& t : space.tris)
    {
        lo = glm::min(lo, t.lo);
        hi = glm::max(hi, t.hi);
    }
    const glm::vec3 palm = kinematics().palmCentre;
    const glm::vec3 towards = glm::vec3{glm::inverse(target.rigToReal) * glm::vec4{(lo + hi) * 0.5f, 1.f}} - palm;
    const float away = glm::length(towards);
    for(int i = 0; i < thumbTurnCount; i++)
    {
        handrig::Pose turned = pose;
        turned.metacarpal = thumbQuat(thumbTurns[i]);
        glm::vec3 points[4];
        fingerPoints(turned, handrig::Thumb, thumbs[i].stop, points);
        tucked[i] = thumbs[i].fromClosed || (away > 1e-4f && glm::dot(points[3] - palm, towards / away) < 0.5f * away);
    }
}

// The palm's place for a grip through the hand (see solve): the places tried, and the best one's move. The places are
// independent: each tried on the game's threads (vr_jobs.hpp) with its own copy of the target, the best then chosen in
// their order (the first of equals), as when one thread tried them in turn.
struct Place
{
    bool found{false}; // clear somewhere along the palm's normal, and so tried
    glm::vec3 move{0.f};
    float value{0.f};
    int probes{0};
};

glm::vec3 placeInside(const handrig::Pose& pose, const Target& target, const Settings& settings, int& tried, int& probes)
{
    const auto value = [&](Target& t, const glm::vec3& move, int& n) {
        t.place(glm::quat{1.f, 0.f, 0.f, 0.f}, move);
        const Context ctx{&t, &pose, settings.overlap, &n};
        float total = -0.1f * glm::length(glm::vec3{3.f * move.x, move.y, move.z});
        for(int f = handrig::Index; f < handrig::FingerCount; f++)
        {
            FingerStop st;
            solveFinger(ctx, f, false, nullptr, st);
            total += score(st);
        }
        return total;
    };
    const auto consider = [&](float dx, float dz, Place& out) {
        // Flush along the palm's normal: coming from as far back as it may, the last place clear before it meets the
        // grip (in quarter steps), if any within reach.
        Target t = target;
        const float reach = za::sqrt(za::fmax(settings.palmLimit * settings.palmLimit - dx * dx - dz * dz, 0.f));
        bool wasClear = false;
        float clearY = 0.f;
        for(float dy = -reach; dy <= reach + 1e-4f; dy += 0.25f)
        {
            t.place(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3{dx, dy, dz});
            if(palmClearance(t, settings.overlap, false) >= -tolerance)
            {
                wasClear = true;
                clearY = dy;
                continue;
            }
            if(!wasClear)
            {
                continue;
            }
            out.found = true;
            out.move = glm::vec3{dx, clearY, dz};
            out.value = value(t, out.move, out.probes);
            return;
        }
    };
    const auto choose = [&](const Place* places, int count, glm::vec3& best, float& bestValue) {
        for(int i = 0; i < count; i++)
        {
            probes += places[i].probes;
            if(places[i].found)
            {
                tried++;
                if(places[i].value > bestValue)
                {
                    bestValue = places[i].value;
                    best = places[i].move;
                }
            }
        }
    };

    // The place given, and 4 x 5 around it.
    constexpr float dxs[4] = {-1.f, 0.f, 1.f, 2.f};
    constexpr float dzs[5] = {-3.f, -2.f, -1.f, 0.f, 1.f};
    Place first[21];
    jobs::parallelFor(21, 1, [&](za::SizeT b, za::SizeT e) {
        for(za::SizeT i = b; i < e; i++)
        {
            if(i == 0)
            {
                Target t = target;
                first[0].found = true;
                first[0].value = value(t, glm::vec3{0.f}, first[0].probes);
            }
            else
            {
                consider(dxs[(i - 1) / 5], dzs[(i - 1) % 5], first[i]);
            }
        }
    });
    glm::vec3 best{0.f};
    float bestValue = first[0].value;
    tried = 1;
    probes += first[0].probes;
    choose(first + 1, 20, best, bestValue);

    // Then half a step round the best.
    const glm::vec3 centre = best;
    constexpr float offsets[8][2] = {{-0.5f, -0.5f}, {-0.5f, 0.f}, {-0.5f, 0.5f}, {0.f, -0.5f}, {0.f, 0.5f}, {0.5f, -0.5f},
        {0.5f, 0.f}, {0.5f, 0.5f}};
    Place second[8];
    jobs::parallelFor(8, 1, [&](za::SizeT b, za::SizeT e) {
        for(za::SizeT i = b; i < e; i++)
        {
            consider(centre.x + offsets[i][0], centre.z + offsets[i][1], second[i]);
        }
    });
    choose(second, 8, best, bestValue);
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
    za::SizeT operator()(const ShapeKey& k) const
    {
        return ankerl::unordered_dense::hash<const void*>{}(k.model) ^ (static_cast<za::SizeT>(k.frame) * 2654435761u);
    }
};
struct CachedShape
{
    za::String name; // the model's (a slot reused by another model after a game change is built again)
    Shape shape;
    bool valid{false};
};
ankerl::unordered_dense::map<ShapeKey, za::UniquePtr<CachedShape>, ShapeKeyHash> shapes; // (pointers into it are kept)

// ----------------------------------------------------------------------------
// Afresh solves remembered (round 22, profiling). A solve with no solve before and nothing else in the way is a pure
// function of the shape, its place in the hand, the settings and the fingers' shifts (the solver reads nothing else),
// and afresh it costs 1.5-6.5 ms (the palm's place searched: up to 29 places, the four fingers closed at each). A weapon
// taken into the hand was solved afresh as it came and again, the same, once it rested (updateGrasp: the same canonical
// place), and each time it was taken again (weapon switches, a foregrip let go and taken back): the profile's worst
// CPU frames in play. The same inputs give the result they gave, bit for bit; anything else is solved as before.

struct Remembered
{
    const Shape* shape{nullptr};
    za::SizeT tris{0};
    unsigned rig{0}; // handrig::generation()
    glm::mat4 shapeToRig{1.f};
    Settings settings;
    glm::vec3 shift[handrig::FingerCount]{};
    Solution solution;
};
constexpr int rememberedCount = 32; // a hand's weapons, grips and fits, both hands (no heap: a fixed table)
za::Array<Remembered, rememberedCount> remembered{};
int rememberedNext = 0;
int rememberedUsed = 0;

[[nodiscard]] bool sameSettings(const Settings& a, const Settings& b)
{
    return a.palmLimit == b.palmLimit && a.palmTurnLimit == b.palmTurnLimit && a.overlap == b.overlap &&
           a.thenar == b.thenar && a.thumbTop == b.thumbTop && a.thumbOutside == b.thumbOutside && a.thumbSink == b.thumbSink && a.thumbWide == b.thumbWide && a.fixedPalm == b.fixedPalm &&
           a.palmMove == b.palmMove &&
           a.palmTurnMove == b.palmTurnMove && a.searchPlace == b.searchPlace;
}

[[nodiscard]] bool sameInputs(const Remembered& r, const handrig::Pose& pose, const Shape& shape,
    const glm::mat4& shapeToRig, const Settings& settings)
{
    if(r.shape != &shape || r.tris != shape.tris.size() || r.rig != handrig::generation() || r.shapeToRig != shapeToRig ||
        !sameSettings(r.settings, settings))
    {
        return false;
    }
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        if(r.shift[f] != pose.shift[f])
        {
            return false;
        }
    }
    return true;
}

bool buildShape(const entity_t& e, int frame, Shape& out)
{
    out.tris.clear();
    out.space = za::makeUnique<Shape::Space>();
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
                out.tris.pushBack({{vertex(0), vertex(k - 1), vertex(k)}});
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
            out.tris.pushBack(t);
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
        out.space->tris.pushBack(r);
    }
    measureWinding(*out.space);
    return !out.tris.empty();
}

} // namespace

void reset()
{
    shapes.clear();
    forgetSolves();
}

void forgetSolves()
{
    za::fill(remembered.begin(), remembered.end(), Remembered{});
    rememberedNext = 0;
    rememberedUsed = 0;
}

void makeShape(const za::Vector<Triangle>& tris, Shape& out)
{
    out.tris = tris;
    if(!out.space)
    {
        out.space = za::makeUnique<Shape::Space>();
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
        space.tris.pushBack(r);
    }
    measureWinding(space);
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
    CachedShape& cached = qza::stableAt<CachedShape>(shapes, ShapeKey{e.model, f});
    if(cached.name != e.model->name)
    {
        if(!cached.name.empty())
        {
            forgetSolves(); // a slot's shape made again in place: the solves remembered for it are another model's
        }
        cached.name = e.model->name;
        cached.valid = buildShape(e, f, cached.shape);
    }
    return cached.valid ? &cached.shape : nullptr;
}

bool worldTriangles(const entity_t& e, bool mirrored, int frame, za::Vector<Triangle>& out)
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
        out.pushBack({{glm::vec3{m * glm::vec4{t.p[0], 1.f}}, glm::vec3{m * glm::vec4{t.p[1], 1.f}},
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
    const auto t0 = qza::nowNs();
    // Afresh with nothing else in the way: the same inputs as a solve remembered give its result (Remembered).
    const bool memo = !previous && !extra;
    if(memo)
    {
        for(int i = 0; i < rememberedUsed; i++)
        {
            if(sameInputs(remembered[static_cast<za::SizeT>(i)], start, shape, shapeToRig, settings))
            {
                out = remembered[static_cast<za::SizeT>(i)].solution;
                out.seconds = qza::secondsSince(t0);
                return;
            }
        }
    }
    const Kinematics& k = kinematics();
    out = Solution{};
    int probes = 0; // (vr_grasp_bench)
    out.palmCentre = k.palmCentre;
    handrig::Pose pose = start;
    pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};
    Shape::Space& space = *shape.space;
    out.triangles = static_cast<int>(space.tris.size());

    // The rig in the shape's real units, and how many hand units one of those is (the axes' least, should the
    // shape be stretched unevenly: distances are then understated, never overstated).
    const glm::mat4 realToRig = shapeToRig * glm::inverse(space.rawToReal);
    const glm::mat4 rigToReal = glm::inverse(realToRig);
    const float scale = za::fmin(glm::length(glm::vec3{realToRig[0]}),
        za::fmin(glm::length(glm::vec3{realToRig[1]}), glm::length(glm::vec3{realToRig[2]})));
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
        target.allowance = za::fmax(extraOverlap - settings.overlap, 0.f);
    }
    target.place(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3{0.f});

    // Whether the palm is in it (a gun's grip through the hand).
    const bool inside = palmClearance(target, settings.overlap, false) < -tolerance;

    // The palm turned (at most palmTurnLimit degrees, about its middle) to face the surface in front of it: the held
    // thing turned the other way round it, for everything below. Not when the palm is in it: nothing to face. The
    // surface: the one a ray from the palm's middle along its normal meets, or else the nearest to its middle on its
    // side (beside the fist: a big thing held by its edge).
    const glm::vec3 up{0.f, 1.f, 0.f};
    if(settings.palmTurnLimit > 0.f && !inside && !settings.searchPlace && !settings.fixedPalm)
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
            const float angle = za::acos(za::clamp(glm::dot(up, want), -1.f, 1.f));
            if(angle > glm::radians(1.f))
            {
                const glm::vec3 axis = glm::normalize(glm::cross(up, want));
                out.palmTurn = glm::angleAxis(za::min(angle, glm::radians(settings.palmTurnLimit)), axis);
            }
        }
    }

    // The palm flush on it (step 4): moved along its normal from as far back as it may go (if clear there), towards it
    // until its spheres meet it (conservative advancement: each step as far as they are clear), no further in than it
    // may go. Not when in it.
    if(settings.palmLimit > 0.f && !inside && !settings.searchPlace && !settings.fixedPalm)
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
                y = za::fmin(y + clear, settings.palmLimit);
            }
            // Nothing met within reach: where it is (the palm doesn't go looking).
            out.palm = y < settings.palmLimit ? up * y : glm::vec3{0.f};
        }
    }

    // A grip through the hand (a weapon's): the palm moved a little along the fingers (x) and the grip (z), flush along
    // its normal there, to where the fingers hold best, closed coarsely at each (as round 21's first solver did); solved
    // again, the place before (the weapon doesn't move in the hand).
    if(settings.fixedPalm)
    {
        out.palm = settings.palmMove;
        out.palmTurn = settings.palmTurnMove;
    }
    else if((inside || settings.searchPlace) && settings.palmLimit > 0.f)
    {
        out.palm = previous ? previous->palm : placeInside(pose, target, settings, out.places, probes);
    }
    target.place(out.palmTurn, out.palm);

    // The fingers: the thumb at each turn tried, and the four fingers, each closed on the game's threads (each turn
    // with its own pose), then the thumb's turn chosen.
    const ThumbStyle style = thumbStyleOf(settings);
    const bool again = previous && previous->thumbChoice >= 0 && thumbStyle(previous->thumbChoice, style);
    int work[thumbTurnCount + handrig::FingerCount - 1];
    int count = 0;
    for(int i = 0; i < thumbTurnCount; i++)
    {
        if(thumbTried(i, previous, style))
        {
            work[count++] = i;
        }
    }
    for(int f = handrig::Index; f < handrig::FingerCount; f++)
    {
        work[count++] = thumbTurnCount + f - handrig::Index;
    }
    FingerStop thumbs[thumbTurnCount];
    int jobProbes[thumbTurnCount + handrig::FingerCount - 1]{};
    jobs::parallelFor(static_cast<za::SizeT>(count), 1, [&](za::SizeT b, za::SizeT e) {
        for(za::SizeT j = b; j < e; j++)
        {
            const int job = work[j];
            if(job < thumbTurnCount)
            {
                handrig::Pose turned = pose;
                turned.metacarpal = thumbQuat(thumbTurns[job]);
                const Context ctx{&target, &turned, settings.overlap, &jobProbes[j]};
                solveFinger(ctx, handrig::Thumb, true, again ? &previous->finger[handrig::Thumb] : nullptr, thumbs[job]);
                if(outside(style))
                {
                    lieAlong(ctx, settings.thumbSink, thumbs[job]);
                }
            }
            else
            {
                const int f = job - thumbTurnCount + handrig::Index;
                const Context ctx{&target, &pose, settings.overlap, &jobProbes[j]};
                solveFinger(ctx, f, true, previous ? &previous->finger[f] : nullptr, out.finger[f]);
            }
        }
    });
    bool tucked[thumbTurnCount]{};
    if(outside(style))
    {
        tuckedThumbs(pose, target, thumbs, tucked);
    }
    chooseThumb(thumbs, tucked, previous, style, out.finger[handrig::Thumb], out.thumbTurn, out.thumbChoice);
    for(int j = 0; j < count; j++)
    {
        probes += jobProbes[j];
        const int f = work[j] < thumbTurnCount ? handrig::Thumb : work[j] - thumbTurnCount + handrig::Index;
        out.fingerProbes[f] += jobProbes[j];
    }
    out.probes = probes;
    out.seconds = qza::secondsSince(t0);
    if(memo)
    {
        Remembered& r = remembered[static_cast<za::SizeT>(rememberedNext)];
        r.shape = &shape;
        r.tris = shape.tris.size();
        r.rig = handrig::generation();
        r.shapeToRig = shapeToRig;
        r.settings = settings;
        za::copy(start.shift, start.shift + handrig::FingerCount, r.shift);
        r.solution = out;
        rememberedNext = (rememberedNext + 1) % rememberedCount;
        rememberedUsed = za::min(rememberedUsed + 1, rememberedCount);
    }
}

float pathCurl(float curl)
{
    const float c = za::fmin(za::fmax(curl, 0.f), 5.f);
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
    const float reach = path + za::fmin(za::fmax(engage, 0.f), 1.f) * (maxCurl - path);
    for(int j = 0; j < handrig::jointsPerFinger; j++)
    {
        out[j] = za::fmin(stop.stop[j], reach);
    }
}

void legacyGripChannel(const handrig::Pose& pose, glm::vec3& point, glm::vec3& dir, float& radius)
{
    // The old hand's channel at rest (its gripChannel with every finger at its default place); a finger's offset
    // moved its circle's middle with it, and the channel's point is their mean.
    constexpr glm::vec3 restPoint{4.4881563f, 0.2731901f, -0.0914762f};
    glm::vec3 shift{0.f};
    for(int f = handrig::Index; f < handrig::FingerCount; f++)
    {
        shift += pose.shift[f];
    }
    point = restPoint + shift / static_cast<float>(handrig::FingerCount - handrig::Index);
    dir = glm::vec3{0.0950148f, 0.1614300f, 0.9822997f};
    radius = 0.7816211f;
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
            const za::Vector<Sphere>& row = k.bone[f][b];
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
    radius = za::fmax(radii / static_cast<float>(count), 0.f);
    return true;
}

bool signedDistance(const Shape& shape, const glm::mat4& shapeToWorld, const glm::vec3& p, float reach, float& distance,
    glm::vec3& at, glm::vec3& normal)
{
    Shape::Space& space = *shape.space;
    const glm::mat4 realToWorld = shapeToWorld * glm::inverse(space.rawToReal);
    const float scale = glm::length(glm::vec3{realToWorld[0]});
    if(!(scale > 1e-6f))
    {
        return false;
    }
    if(space.cell <= 0.f)
    {
        space.buildGrid(0.8f / scale); // about the hand's cells (a hand unit is about 0.4 world units)
    }
    const glm::mat4 worldToReal = glm::inverse(realToWorld);
    const glm::vec3 q{worldToReal * glm::vec4{p, 1.f}};
    glm::vec3 closest, n;
    const float d = space.nearest(q, reach / scale, &closest, &n);
    if(d >= reach / scale)
    {
        return false;
    }
    // Its outward normal by the model's winding (Space::outward), to the world by the inverse transpose, which keeps a
    // side outward under any placing, mirrored too.
    const glm::vec3 out = n * space.outward;
    normal = glm::normalize(glm::mat3{glm::transpose(worldToReal)} * out);
    at = glm::vec3{realToWorld * glm::vec4{closest, 1.f}};
    // Inside by the nearest triangle's side; never deeper than half the model's thinnest box side (beside an edge or a
    // corner, the triangle's side can be the wrong one).
    const bool in = glm::dot(q - closest, out) < 0.f && d <= 0.5f * qza::minOf(space.hi.x - space.lo.x, space.hi.y - space.lo.y, space.hi.z - space.lo.z);
    distance = in ? -d * scale : d * scale;
    return true;
}

bool rayHit(const Shape& shape, const glm::mat4& shapeToWorld, const glm::vec3& from, const glm::vec3& to, glm::vec3& at,
    glm::vec3& normal, bool& entering)
{
    const Shape::Space& space = *shape.space;
    const glm::mat4 realToWorld = shapeToWorld * glm::inverse(space.rawToReal);
    const glm::mat4 worldToReal = glm::inverse(realToWorld);
    const glm::vec3 a{worldToReal * glm::vec4{from, 1.f}}, b{worldToReal * glm::vec4{to, 1.f}};
    const float len = glm::length(b - a);
    float hit;
    glm::vec3 n, wound;
    const glm::vec3 dir = (b - a) / za::max(len, 1e-6f);
    if(!(len > 1e-6f) || !space.ray(a, dir, len, hit, &n, &wound))
    {
        return false;
    }
    entering = glm::dot(wound * space.outward, dir) < 0.f; // its outward side faces `from`
    at = glm::vec3{realToWorld * glm::vec4{a + dir * hit, 1.f}};
    normal = glm::normalize(glm::mat3{glm::transpose(worldToReal)} * n); // (facing `from`: kept so by the inverse transpose)
    return true;
}

float surfaceDistance(const Shape& shape, const glm::mat4& shapeToWorld, const glm::vec3& p, float reach, glm::vec3& at, bool& in)
{
    Shape::Space& space = *shape.space;
    const glm::mat4 realToWorld = shapeToWorld * glm::inverse(space.rawToReal);
    const float scale = glm::length(glm::vec3{realToWorld[0]});
    if(!(scale > 1e-6f))
    {
        return -1.f;
    }
    if(space.cell <= 0.f)
    {
        space.buildGrid(0.8f / scale); // (as signedDistance's)
    }
    const glm::vec3 q{glm::inverse(realToWorld) * glm::vec4{p, 1.f}};
    glm::vec3 closest, n;
    const float d = space.nearest(q, reach / scale, &closest, &n);
    if(d >= reach / scale)
    {
        return -1.f;
    }
    in = glm::dot(q - closest, n * space.outward) < 0.f; // (Quake's models are wound inwards: Space::outward)
    at = glm::vec3{realToWorld * glm::vec4{closest, 1.f}};
    return d * scale;
}

void fingerPoints(const handrig::Pose& pose, int finger, const float curls[handrig::jointsPerFinger], glm::vec3 out[4])
{
    const Kinematics& k = kinematics();
    handrig::Rigid seg[handrig::jointsPerFinger + 1];
    handrig::fingerSegments(pose, finger, curls, seg);
    for(int j = 0; j < handrig::jointsPerFinger; j++)
    {
        out[j] = seg[j](handrig::rig().pivot[finger][j]);
    }
    const za::Vector<Sphere>& row = k.bone[finger][handrig::jointsPerFinger];
    out[3] = row.empty() ? out[2] : seg[handrig::jointsPerFinger](row.back().c + glm::normalize(row.back().c - row.front().c) * row.back().r);
}

void fingertips(const handrig::Pose& pose, glm::vec3 out[handrig::FingerCount])
{
    const Kinematics& k = kinematics();
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        handrig::Rigid seg[handrig::jointsPerFinger + 1];
        handrig::fingerSegments(pose, f, pose.curl[f], seg);
        const za::Vector<Sphere>& row = k.bone[f][handrig::jointsPerFinger];
        out[f] = row.empty() ? seg[handrig::jointsPerFinger].t : seg[handrig::jointsPerFinger](row.back().c);
    }
}

void posedSpheres(const handrig::Pose& pose, za::Vector<glm::vec4>& out)
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
                out.pushBack(glm::vec4{seg[b](s.c), s.r});
            }
        }
    }
    for(const Sphere& s : k.palm)
    {
        out.pushBack(glm::vec4{s.c, s.r});
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
