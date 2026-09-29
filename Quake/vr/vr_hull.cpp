// vr_hull.cpp -- see vr_hull.hpp.

#include "vr_hull.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

namespace qvr::hull
{
namespace
{

#ifdef _MSC_VER
#define QVR_NOINLINE __declspec(noinline)
#else
#define QVR_NOINLINE __attribute__((noinline))
#endif

constexpr double distEpsilon = 0.03125; // Quake's DIST_EPSILON: how far a move stops short of a surface
constexpr double onEpsilon = 0.001;     // a winding's point this close to a plane is on it (the build)
constexpr double bogus = 262144.0;      // a plane's first winding, far past any map (BSP2's included)
constexpr float minWidth = 8.f, maxWidth = 32.f;

// A brush's face, outward: inside is dot(normal, p) < dist.
// A brush's face, outward: inside is dot(normal, p) < dist + grows * the box's reach along normal. A face of hull 0's
// brushes grows with the box (grows 1); a recovered clip brush's face (see recoverClips) is already grown by Quake's
// 32 box and shrinks back with a narrower one if it faces the open (grows 1, dist less Quake's box), else stays.
struct Plane
{
    glm::vec3 normal;
    float dist;
    float grows;
};

struct Brush
{
    std::uint32_t first;
    std::uint32_t count;
    glm::vec3 mins, maxs; // hull 0's: its bounds; a clip brush's: the bounds of the box centres it holds (32 box)
    bool clip;
};

// The server map's solid space as brushes: pointers into the hunk (the world's hull 0), released at every map change.
struct Brushes
{
    const mclipnode_t* clipnodes = nullptr; // the hull 0 they were built from (the world's and its brush models')
    std::vector<Plane> planes;
    std::vector<Brush> brushes;
    std::vector<int> leafBrush; // [node * 2 + side]: the brush of that child when it is a solid leaf, else -1
    std::vector<int> clips;     // the world's recovered clip brushes
    std::vector<int> leafClipStart; // [node * 2 + side]: where the clip brushes whose box centres reach into that
    std::vector<int> leafClipList;  // leaf of the world's hull 0 (empty or solid) start in leafClipList (and end: +1)
    mutable std::vector<std::size_t> clipStamp; // a clip brush's last test (one per sweep: it is in many leaves)
    mutable std::size_t stamp = 0;
    std::vector<int> hull1Clip; // [hull 1 node * 2 + side]: a solid leaf's clip brush, -1 not one, -2 not looked at
    double ms = 0.0;            // the build's time
    double clipMs = 0.0;        // of which the clip brushes'
    int bevels = 0;             // planes added as Quake 2's bevels (axial, edge)
    int dropped = 0;            // solid leaves with no volume left (slivers under the build's epsilon)
    int hull1Leaves = 0;        // the world's hull 1 solid leaves looked at for clip brushes
    int numnodes = 0;
    auto members()
    {
        return std::tie(clipnodes, planes, brushes, leafBrush, clips, leafClipStart, leafClipList, clipStamp, stamp, hull1Clip, ms,
            clipMs, bevels, dropped, hull1Leaves, numnodes);
    }
};
mem::Cache<Brushes> built{"hull brushes", mem::MapChange};

// ---------------------------------------------------------------------------------------------------------------
// The build: hull 0's tree walked from each model's head node, a convex polytope (faces with their windings)
// split by each node's plane on the way down; at a solid (or sky: solid to Quake's clipping hulls) leaf what is
// left is that leaf's brush.

using Winding = std::vector<glm::dvec3>;

struct Face
{
    glm::dvec3 normal; // outward
    double dist;
    Winding w; // empty: the plane bounds the piece but its face was lost to the epsilon (the plane is kept)
};
using Poly = std::vector<Face>;

Winding baseWinding(const glm::dvec3& n, double d)
{
    const glm::dvec3 a = glm::abs(n);
    glm::dvec3 up = (a.z >= a.x && a.z >= a.y) ? glm::dvec3{1.0, 0.0, 0.0} : glm::dvec3{0.0, 0.0, 1.0};
    up = glm::normalize(up - n * glm::dot(up, n));
    const glm::dvec3 right = glm::cross(up, n) * bogus;
    up *= bogus;
    const glm::dvec3 org = n * d;
    return {org - right + up, org + right + up, org + right - up, org - right - up};
}

// The part of `in` on the kept side of the plane (front: dot(n, p) >= d).
void clipWinding(const Winding& in, const glm::dvec3& n, double d, bool keepFront, Winding& out)
{
    out.clear();
    const std::size_t count = in.size();
    if(count < 3)
    {
        return;
    }
    const double sign = keepFront ? 1.0 : -1.0;
    std::vector<double> dists(count);
    std::vector<int> sides(count);
    bool anyBack = false, anyFront = false;
    for(std::size_t i = 0; i < count; ++i)
    {
        dists[i] = (glm::dot(n, in[i]) - d) * sign;
        sides[i] = dists[i] > onEpsilon ? 1 : (dists[i] < -onEpsilon ? -1 : 0);
        anyFront = anyFront || sides[i] > 0;
        anyBack = anyBack || sides[i] < 0;
    }
    if(!anyBack)
    {
        out = in;
        return;
    }
    if(!anyFront)
    {
        return;
    }
    for(std::size_t i = 0; i < count; ++i)
    {
        const std::size_t j = (i + 1) % count;
        if(sides[i] >= 0)
        {
            out.push_back(in[i]);
        }
        if(sides[i] == 0 || sides[j] == 0 || sides[i] == sides[j])
        {
            continue;
        }
        const double t = dists[i] / (dists[i] - dists[j]);
        out.push_back(in[i] + (in[j] - in[i]) * t);
    }
    if(out.size() < 3)
    {
        out.clear();
    }
}

// `p` split by the plane: front (dot(n, x) >= d) and back. A side the plane leaves nothing of stays empty.
void splitPoly(Poly&& p, const glm::dvec3& n, double d, Poly& front, Poly& back)
{
    front.clear();
    back.clear();
    double lo = 1e300, hi = -1e300;
    for(const Face& f : p)
    {
        for(const glm::dvec3& v : f.w)
        {
            const double t = glm::dot(n, v) - d;
            lo = std::min(lo, t);
            hi = std::max(hi, t);
        }
    }
    if(lo > hi) // no points at all: nothing left
    {
        return;
    }
    if(lo >= -onEpsilon)
    {
        front = std::move(p);
        return;
    }
    if(hi <= onEpsilon)
    {
        back = std::move(p);
        return;
    }
    Winding cap = baseWinding(n, d), tmp;
    for(Face& f : p)
    {
        clipWinding(cap, f.normal, f.dist, false, tmp);
        std::swap(cap, tmp);
        if(f.w.empty())
        {
            front.push_back(f);
            back.push_back(f);
            continue;
        }
        Face fb{f.normal, f.dist, {}};
        clipWinding(f.w, n, d, false, fb.w);
        clipWinding(f.w, n, d, true, tmp);
        if(!tmp.empty())
        {
            front.push_back(Face{f.normal, f.dist, tmp});
        }
        if(!fb.w.empty())
        {
            back.push_back(std::move(fb));
        }
    }
    // The cap's plane bounds both pieces even when its face is lost to the epsilon.
    front.push_back(Face{-n, -d, cap});
    back.push_back(Face{n, d, std::move(cap)});
}

Poly boxPoly(const glm::dvec3& mins, const glm::dvec3& maxs)
{
    Poly p;
    for(int axis = 0; axis < 3; ++axis)
    {
        for(int s = 0; s < 2; ++s)
        {
            glm::dvec3 n{0.0};
            n[axis] = s ? 1.0 : -1.0;
            p.push_back(Face{n, s ? maxs[axis] : -mins[axis], {}});
        }
    }
    Winding tmp;
    for(Face& f : p)
    {
        f.w = baseWinding(f.normal, f.dist);
        for(const Face& g : p)
        {
            if(&g != &f)
            {
                clipWinding(f.w, g.normal, g.dist, false, tmp);
                std::swap(f.w, tmp);
            }
        }
    }
    return p;
}

bool hasPlane(const Brushes& b, std::size_t first, const glm::dvec3& n, double d)
{
    for(std::size_t i = first; i < b.planes.size(); ++i)
    {
        const Plane& q = b.planes[i];
        if(glm::dot(glm::dvec3{q.normal}, n) > 1.0 - 1e-6 && std::abs(q.dist - d) < 0.01)
        {
            return true;
        }
    }
    return false;
}

bool hasNormal(const Brushes& b, std::size_t first, const glm::dvec3& n)
{
    for(std::size_t i = first; i < b.planes.size(); ++i)
    {
        if(glm::dot(glm::dvec3{b.planes[i].normal}, n) > 1.0 - 1e-6)
        {
            return true;
        }
    }
    return false;
}

// A solid leaf's piece as a brush: its faces' planes, then Quake 2's bevels (qbsp's AddBrushBevels) so that a box's
// sweep meets it where the box meets it, not where the planes' extensions do past its edges and corners.
int emitBrush(Brushes& b, const Poly& p)
{
    glm::dvec3 lo{1e300}, hi{-1e300};
    for(const Face& f : p)
    {
        for(const glm::dvec3& v : f.w)
        {
            lo = glm::min(lo, v);
            hi = glm::max(hi, v);
        }
    }
    if(lo.x > hi.x || hi.x - lo.x < onEpsilon || hi.y - lo.y < onEpsilon || hi.z - lo.z < onEpsilon)
    {
        ++b.dropped;
        return -1;
    }
    const std::size_t first = b.planes.size();
    for(const Face& f : p)
    {
        if(!hasPlane(b, first, f.normal, f.dist))
        {
            b.planes.push_back(Plane{glm::vec3{f.normal}, static_cast<float>(f.dist), 1.f});
        }
    }
    // Axial bevels: the piece's bounds, where no face already faces that way.
    for(int axis = 0; axis < 3; ++axis)
    {
        for(int s = 0; s < 2; ++s)
        {
            glm::dvec3 n{0.0};
            n[axis] = s ? 1.0 : -1.0;
            if(!hasNormal(b, first, n))
            {
                b.planes.push_back(Plane{glm::vec3{n}, static_cast<float>(s ? hi[axis] : -lo[axis]), 1.f});
                ++b.bevels;
            }
        }
    }
    // Edge bevels: along each slanted edge, the planes through it square to an axis that have the whole piece behind.
    for(const Face& f : p)
    {
        const std::size_t count = f.w.size();
        for(std::size_t i = 0; i < count; ++i)
        {
            const glm::dvec3& a = f.w[i];
            glm::dvec3 e = f.w[(i + 1) % count] - a;
            const double len = glm::length(e);
            if(len < 0.5)
            {
                continue;
            }
            e /= len;
            if(std::abs(e.x) > 0.9999 || std::abs(e.y) > 0.9999 || std::abs(e.z) > 0.9999)
            {
                continue; // an axial edge: the axial bevels have it
            }
            for(int axis = 0; axis < 3; ++axis)
            {
                for(int s = 0; s < 2; ++s)
                {
                    glm::dvec3 ax{0.0};
                    ax[axis] = s ? 1.0 : -1.0;
                    glm::dvec3 n = glm::cross(e, ax);
                    const double nl = glm::length(n);
                    if(nl < 0.5)
                    {
                        continue;
                    }
                    n /= nl;
                    const double d = glm::dot(a, n);
                    if(hasPlane(b, first, n, d))
                    {
                        continue;
                    }
                    bool behind = true;
                    for(const Face& g : p)
                    {
                        for(const glm::dvec3& v : g.w)
                        {
                            if(glm::dot(v, n) - d > 0.1)
                            {
                                behind = false;
                                break;
                            }
                        }
                        if(!behind)
                        {
                            break;
                        }
                    }
                    if(behind)
                    {
                        b.planes.push_back(Plane{glm::vec3{n}, static_cast<float>(d), 1.f});
                        ++b.bevels;
                    }
                }
            }
        }
    }
    b.brushes.push_back(Brush{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(b.planes.size() - first),
        glm::vec3{lo}, glm::vec3{hi}, false});
    return static_cast<int>(b.brushes.size()) - 1;
}

bool solidContents(int contents)
{
    return contents == CONTENTS_SOLID || contents == CONTENTS_SKY;
}

// Each solid leaf's piece to onSolid(node, side, piece).
template <class OnSolid>
void walk(const hull_t& hull, int num, Poly&& poly, OnSolid& onSolid)
{
    if(num < hull.firstclipnode || num > hull.lastclipnode)
    {
        return;
    }
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    Poly parts[2];
    splitPoly(std::move(poly), glm::dvec3{plane.normal[0], plane.normal[1], plane.normal[2]}, plane.dist, parts[0],
        parts[1]);
    for(int side = 0; side < 2; ++side)
    {
        const int child = node.children[side];
        if(parts[side].empty())
        {
            continue;
        }
        if(child >= 0)
        {
            walk(hull, child, std::move(parts[side]), onSolid);
        }
        else if(solidContents(child))
        {
            onSolid(num, side, parts[side]);
        }
    }
}

void recoverClips(Brushes& b, qmodel_t* world);

void build(qmodel_t* world)
{
    const auto t0 = std::chrono::steady_clock::now();
    Brushes& b = built;
    const hull_t& hull0 = world->hulls[0];
    b.clipnodes = hull0.clipnodes;
    b.numnodes = world->numnodes;
    b.planes.clear();
    b.brushes.clear();
    b.clips.clear();
    b.leafClipStart.clear();
    b.leafClipList.clear();
    b.clipStamp.clear();
    b.bevels = 0;
    b.dropped = 0;
    b.hull1Leaves = 0;
    b.leafBrush.assign(static_cast<std::size_t>(b.numnodes) * 2, -1);
    hull_t h = hull0;
    h.firstclipnode = 0;
    h.lastclipnode = b.numnodes - 1;
    for(int i = 0; i < world->numsubmodels; ++i)
    {
        const dmodel_t& sub = world->submodels[i];
        const int head = sub.headnode[0];
        if(head < 0 || head >= b.numnodes)
        {
            continue;
        }
        // The model's bounds and a margin: the world's outside is solid in hull 0, cut off here (no one gets there).
        const glm::dvec3 margin{64.0};
        const glm::dvec3 mins = glm::dvec3{sub.mins[0], sub.mins[1], sub.mins[2]} - margin;
        const glm::dvec3 maxs = glm::dvec3{sub.maxs[0], sub.maxs[1], sub.maxs[2]} + margin;
        auto onSolid = [&b](int num, int side, const Poly& piece)
        { b.leafBrush[static_cast<std::size_t>(num) * 2 + side] = emitBrush(b, piece); };
        walk(h, head, boxPoly(mins, maxs), onSolid);
    }
    const auto t1 = std::chrono::steady_clock::now();
    recoverClips(b, world);
    const auto t2 = std::chrono::steady_clock::now();
    b.clipMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    b.ms = std::chrono::duration<double, std::milli>(t2 - t0).count();
}

float widthSetting()
{
    return std::clamp(vr_hull_width.value, minWidth, maxWidth);
}

// The server's map as brushes, built now if not yet (the setting turned on during a map: a hitch, once).
const Brushes* worldBrushes(qmodel_t* world)
{
    if(!world || world->type != mod_brush || world->numnodes <= 0)
    {
        return nullptr;
    }
    if(built.clipnodes != world->hulls[0].clipnodes)
    {
        build(world);
        Con_DPrintf("hull: %s rebuilt as %d brushes in %.1f ms\n", world->name, static_cast<int>(built.brushes.size()),
            built.ms);
    }
    return &built;
}

// ---------------------------------------------------------------------------------------------------------------
// The sweep: Quake 2's CM_RecursiveHullCheck and CM_ClipBoxToBrush, with Quake's rules for a box that starts
// exactly on a surface (outside: the drawing hull's "d >= 0 is in front") and for allsolid.

struct Sweep
{
    const Brushes* b;
    const hull_t* hull;
    glm::dvec3 start, end; // the box's centre
    glm::dvec3 ext;        // its half size
    glm::vec3 sweepMins, sweepMaxs;   // the swept box's bounds
    glm::vec3 centreMins, centreMaxs; // its centre's (clip brushes)
    bool clips = false;               // the world's clip brushes too
    double fraction = 1.0;
    bool startsolid = false;
    bool getout = true; // false: the start brush holds the end too
    const Plane* hitPlane = nullptr;
    int brushTests = 0;
};

double support(const Plane& p, const glm::dvec3& ext)
{
    return p.grows * (std::abs(p.normal.x * ext.x) + std::abs(p.normal.y * ext.y) + std::abs(p.normal.z * ext.z));
}

void clipToBrush(Sweep& s, const Brush& br)
{
    const glm::vec3& lo = br.clip ? s.centreMins : s.sweepMins;
    const glm::vec3& hi = br.clip ? s.centreMaxs : s.sweepMaxs;
    if(br.mins.x > hi.x || br.mins.y > hi.y || br.mins.z > hi.z || br.maxs.x < lo.x || br.maxs.y < lo.y ||
        br.maxs.z < lo.z)
    {
        return;
    }
    ++s.brushTests;
    double enter = -1.0, leave = 1.0;
    const Plane* clip = nullptr;
    bool startout = false, getout = false;
    for(std::uint32_t i = 0; i < br.count; ++i)
    {
        const Plane& p = s.b->planes[br.first + i];
        const glm::dvec3 n{p.normal};
        const double dist = p.dist + support(p, s.ext);
        const double d1 = glm::dot(s.start, n) - dist, d2 = glm::dot(s.end, n) - dist;
        getout = getout || d2 >= 0.0;
        startout = startout || d1 >= 0.0;
        if(d1 >= 0.0 && d2 >= 0.0)
        {
            return; // wholly in front of one face: the sweep misses it
        }
        if(d1 < 0.0 && d2 < 0.0)
        {
            continue;
        }
        if(d1 >= 0.0) // entering
        {
            // Clamped at 0 (as Quake 3 does, not Quake 2): a box starting less than the epsilon off the face and
            // moving into it would otherwise get a large negative fraction, miss the brush, and end up inside it.
            const double f = std::max(0.0, (d1 - distEpsilon) / (d1 - d2));
            if(f > enter)
            {
                enter = f;
                clip = &p;
            }
        }
        else // leaving
        {
            leave = std::min(leave, (d1 + distEpsilon) / (d1 - d2));
        }
    }
    if(!startout)
    {
        s.startsolid = true;
        s.getout = s.getout && getout;
        return;
    }
    if(enter < leave && enter > -1.0 && enter < s.fraction)
    {
        s.fraction = std::max(enter, 0.0);
        s.hitPlane = clip;
    }
}

void sweepChild(Sweep& s, int num, int side, double p1f, double p2f, const glm::dvec3& p1, const glm::dvec3& p2);

void sweepNode(Sweep& s, int num, double p1f, double p2f, const glm::dvec3& p1, const glm::dvec3& p2)
{
    if(s.fraction <= p1f)
    {
        return; // already hit something nearer
    }
    const mclipnode_t& node = s.hull->clipnodes[num];
    const mplane_t& plane = s.hull->planes[node.planenum];
    double t1, t2, offset;
    if(plane.type < 3)
    {
        t1 = p1[plane.type] - plane.dist;
        t2 = p2[plane.type] - plane.dist;
        offset = s.ext[plane.type];
    }
    else
    {
        const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
        t1 = glm::dot(n, p1) - plane.dist;
        t2 = glm::dot(n, p2) - plane.dist;
        offset = std::abs(s.ext.x * n.x) + std::abs(s.ext.y * n.y) + std::abs(s.ext.z * n.z);
    }
    // Quake 3's unit of slop past the box's extent.
    if(t1 >= offset + 1.0 && t2 >= offset + 1.0)
    {
        sweepChild(s, num, 0, p1f, p2f, p1, p2);
        return;
    }
    if(t1 < -offset - 1.0 && t2 < -offset - 1.0)
    {
        sweepChild(s, num, 1, p1f, p2f, p1, p2);
        return;
    }
    int side;
    double frac, frac2;
    if(t1 < t2)
    {
        const double idist = 1.0 / (t1 - t2);
        side = 1;
        frac2 = (t1 + offset + distEpsilon) * idist;
        frac = (t1 - offset + distEpsilon) * idist;
    }
    else if(t1 > t2)
    {
        const double idist = 1.0 / (t1 - t2);
        side = 0;
        frac2 = (t1 - offset - distEpsilon) * idist;
        frac = (t1 + offset + distEpsilon) * idist;
    }
    else
    {
        side = 0;
        frac = 1.0;
        frac2 = 0.0;
    }
    frac = std::clamp(frac, 0.0, 1.0);
    frac2 = std::clamp(frac2, 0.0, 1.0);
    const double midf = p1f + (p2f - p1f) * frac;
    sweepChild(s, num, side, p1f, midf, p1, p1 + (p2 - p1) * frac);
    const double midf2 = p1f + (p2f - p1f) * frac2;
    sweepChild(s, num, side ^ 1, midf2, p2f, p1 + (p2 - p1) * frac2, p2);
}

// The clip brushes reaching into a leaf, each once a sweep. Kept out of sweepChild: inlined there, the loop made
// every sweep through the tree twice as slow (MSVC), clip brushes or not.
QVR_NOINLINE void clipLeafClips(Sweep& s, std::size_t key)
{
    for(int i = s.b->leafClipStart[key]; i < s.b->leafClipStart[key + 1]; ++i)
    {
        const int c = s.b->leafClipList[i];
        if(s.b->clipStamp[c] != s.b->stamp)
        {
            s.b->clipStamp[c] = s.b->stamp;
            clipToBrush(s, s.b->brushes[c]);
        }
    }
}

void sweepChild(Sweep& s, int num, int side, double p1f, double p2f, const glm::dvec3& p1, const glm::dvec3& p2)
{
    const int child = s.hull->clipnodes[num].children[side];
    if(child >= 0)
    {
        sweepNode(s, child, p1f, p2f, p1, p2);
        return;
    }
    const std::size_t key = static_cast<std::size_t>(num) * 2 + side;
    const int brush = s.b->leafBrush[key];
    if(brush >= 0)
    {
        clipToBrush(s, s.b->brushes[brush]);
    }
    if(s.clips && s.b->leafClipStart[key] != s.b->leafClipStart[key + 1])
    {
        clipLeafClips(s, key);
    }
}

bool boxInBrush(const Brushes& b, const Brush& br, const glm::dvec3& p, const glm::dvec3& ext)
{
    for(std::uint32_t i = 0; i < br.count; ++i)
    {
        const Plane& q = b.planes[br.first + i];
        if(glm::dot(p, glm::dvec3{q.normal}) - (q.dist + support(q, ext)) >= 0.0)
        {
            return false;
        }
    }
    return true;
}

// Whether the box at p overlaps a brush of the tree (touching is not: Quake's "d >= 0 is in front").
bool boxInTree(const Brushes& b, const hull_t& hull, int num, const glm::dvec3& p, const glm::dvec3& ext, bool clips)
{
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    const double t = glm::dot(n, p) - plane.dist;
    const double offset = std::abs(ext.x * n.x) + std::abs(ext.y * n.y) + std::abs(ext.z * n.z);
    for(int side = 0; side < 2; ++side)
    {
        if(side == 0 ? t <= -offset - 1.0 : t >= offset + 1.0)
        {
            continue;
        }
        const int child = node.children[side];
        if(child >= 0)
        {
            if(boxInTree(b, hull, child, p, ext, clips))
            {
                return true;
            }
        }
        else
        {
            const std::size_t key = static_cast<std::size_t>(num) * 2 + side;
            if(const int brush = b.leafBrush[key]; brush >= 0 && boxInBrush(b, b.brushes[brush], p, ext))
            {
                return true;
            }
            for(int i = clips ? b.leafClipStart[key] : 0, e = clips ? b.leafClipStart[key + 1] : 0; i < e; ++i)
            {
                const int c = b.leafClipList[i];
                if(b.clipStamp[c] != b.stamp)
                {
                    b.clipStamp[c] = b.stamp;
                    if(boxInBrush(b, b.brushes[c], p, ext))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// ... or a clip brush (the world's: head 0).
bool boxInSolid(const Brushes& b, const hull_t& hull, int head, const glm::dvec3& p, const glm::dvec3& ext,
    bool clips = true)
{
    ++b.stamp;
    return boxInTree(b, hull, head, p, ext, clips && head == 0 && !b.leafClipStart.empty());
}

// The leaves of the world's hull 0 a box reaches into (touching included): a clip brush's box centres.
void leavesOfBox(const hull_t& hull, int num, const glm::dvec3& lo, const glm::dvec3& hi, std::vector<int>& out)
{
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    const glm::dvec3 centre = (lo + hi) * 0.5, half = (hi - lo) * 0.5;
    const double t = glm::dot(n, centre) - plane.dist;
    const double reach = std::abs(n.x * half.x) + std::abs(n.y * half.y) + std::abs(n.z * half.z) + 1.0;
    for(int side = 0; side < 2; ++side)
    {
        if(side == 0 ? t < -reach : t >= reach)
        {
            continue;
        }
        const int child = node.children[side];
        if(child >= 0)
        {
            leavesOfBox(hull, child, lo, hi, out);
        }
        else
        {
            out.push_back(num * 2 + side);
        }
    }
}

// Clip brushes: solid only to the boxes, so hull 0 has nothing of them, but hull 1 has them grown by Quake's box.
// The world's hull 1 is cut into its solid leaves as hull 0 is; a leaf where Quake's box somewhere meets none of hull
// 0's brushes holds a clip brush (or qbsp's rounding of a corner). It is kept in the box's centre space as it is
// (Quake's 32 box meets it exactly as hull 1 does), and a narrower box meets it shrunk on the faces that face the open
// (the others lie against more of hull 1's solid). Solid to monsters too in Quake; here only to the narrower player.
void recoverClips(Brushes& b, qmodel_t* world)
{
    const hull_t& hull1 = world->hulls[1];
    const dmodel_t& sub = world->submodels[0];
    const int head = sub.headnode[1];
    if(head < 0 || world->numclipnodes <= 0 || !hull1.clipnodes)
    {
        return;
    }
    hull_t h1 = hull1;
    h1.firstclipnode = 0;
    h1.lastclipnode = world->numclipnodes - 1;
    hull_t h0 = world->hulls[0];
    const glm::dvec3 e32{16.0, 16.0, 28.0};
    const glm::dvec3 lift{0.0, 0.0, 4.0}; // hull 1's point is 4 below the centre of Quake's box (-24..32)
    b.hull1Clip.assign(static_cast<std::size_t>(world->numclipnodes) * 2, -2);
    std::vector<bool> open; // the piece's faces' (a build's buffer)
    auto onSolid = [&](int num1, int side1, const Poly& piece)
    {
        ++b.hull1Leaves;
        // Its faces that face the open (hull 1 empty just past their middle), its centre and bounds.
        open.clear();
        glm::dvec3 c{0.0}, lo{1e300}, hi{-1e300};
        int count = 0;
        for(const Face& f : piece)
        {
            glm::dvec3 fc{0.0};
            for(const glm::dvec3& v : f.w)
            {
                fc += v;
                lo = glm::min(lo, v);
                hi = glm::max(hi, v);
            }
            c += fc;
            count += static_cast<int>(f.w.size());
            bool isOpen = false;
            if(!f.w.empty())
            {
                fc = fc / static_cast<double>(f.w.size()) + f.normal * 0.5;
                vec3_t q{static_cast<float>(fc.x), static_cast<float>(fc.y), static_cast<float>(fc.z)};
                isOpen = SV_HullPointContents(&h1, head, q) != CONTENTS_SOLID;
            }
            open.push_back(isOpen);
        }
        if(!count)
        {
            return;
        }
        c /= count;
        // Is there room for Quake's box anywhere in it (inside the world: past it, hull 0's brushes stop)? Where hull
        // 1 is rounder or has a clip brush, just inside its open faces: on each (its middle, toward its corners and edge
        // middles) a unit in. Then at its centre, near its corners and on a 3x3x3 lattice across it.
        auto roomAt = [&](const glm::dvec3& p)
        {
            for(int k = 0; k < 3; ++k)
            {
                if(p[k] < sub.mins[k] || p[k] > sub.maxs[k])
                {
                    return false;
                }
            }
            return !boxInSolid(b, h0, 0, p + lift, e32, false);
        };
        bool clip = false;
        for(std::size_t fi = 0; fi < piece.size() && !clip; ++fi)
        {
            const Face& f = piece[fi];
            if(!open[fi])
            {
                continue;
            }
            glm::dvec3 fc{0.0};
            for(const glm::dvec3& v : f.w)
            {
                fc += v;
            }
            fc /= static_cast<double>(f.w.size());
            const glm::dvec3 in = -f.normal;
            clip = roomAt(fc + in);
            for(std::size_t i = 0; i < f.w.size() && !clip; ++i)
            {
                // Toward each corner and edge middle: at 45% and 85% of the way from the face's middle.
                const glm::dvec3 mid = (f.w[i] + f.w[(i + 1) % f.w.size()]) * 0.5;
                clip = roomAt(fc + (f.w[i] - fc) * 0.85 + in) || roomAt(fc + (mid - fc) * 0.85 + in) ||
                       roomAt(fc + (f.w[i] - fc) * 0.45 + in) || roomAt(fc + (mid - fc) * 0.45 + in);
            }
        }
        clip = clip || roomAt(c);
        for(const Face& f : piece)
        {
            for(std::size_t i = 0; i < f.w.size() && !clip; ++i)
            {
                clip = roomAt(c + (f.w[i] - c) * 0.9);
            }
        }
        for(int k = 0; k < 27 && !clip; ++k)
        {
            const glm::dvec3 t{0.2 + 0.3 * (k % 3), 0.2 + 0.3 * (k / 3 % 3), 0.2 + 0.3 * (k / 9)};
            const glm::dvec3 p = lo + (hi - lo) * t;
            bool inside = true;
            for(const Face& f : piece)
            {
                inside = inside && glm::dot(f.normal, p) - f.dist < -0.5;
            }
            clip = inside && roomAt(p);
        }
        b.hull1Clip[static_cast<std::size_t>(num1) * 2 + side1] = clip ? static_cast<int>(b.brushes.size()) : -1;
        if(!clip)
        {
            return;
        }
        // Kept in the box centre's space; the open faces shrink back with a narrower box.
        const std::size_t first = b.planes.size();
        for(std::size_t fi = 0; fi < piece.size(); ++fi)
        {
            const Face& f = piece[fi];
            const double dist = f.dist + glm::dot(f.normal, lift);
            const double reach =
                std::abs(f.normal.x) * e32.x + std::abs(f.normal.y) * e32.y + std::abs(f.normal.z) * e32.z;
            b.planes.push_back(
                Plane{glm::vec3{f.normal}, static_cast<float>(open[fi] ? dist - reach : dist), open[fi] ? 1.f : 0.f});
        }
        b.brushes.push_back(Brush{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(b.planes.size() - first),
            glm::vec3{lo + lift}, glm::vec3{hi + lift}, true});
        b.clips.push_back(static_cast<int>(b.brushes.size()) - 1);
    };
    const glm::dvec3 margin{96.0};
    walk(h1, head,
        boxPoly(glm::dvec3{sub.mins[0], sub.mins[1], sub.mins[2]} - margin,
            glm::dvec3{sub.maxs[0], sub.maxs[1], sub.maxs[2]} + margin),
        onSolid);

    // Each clip brush into the leaves its box centres reach (a sweep's centre passes through them).
    std::vector<std::pair<int, int>> pairs; // (leaf key, clip brush)
    std::vector<int> keys;
    for(const int c : b.clips)
    {
        keys.clear();
        leavesOfBox(h0, 0, glm::dvec3{b.brushes[c].mins}, glm::dvec3{b.brushes[c].maxs}, keys);
        for(const int k : keys)
        {
            pairs.emplace_back(k, c);
        }
    }
    std::sort(pairs.begin(), pairs.end());
    b.leafClipStart.assign(static_cast<std::size_t>(b.numnodes) * 2 + 1, 0);
    b.leafClipList.resize(pairs.size());
    for(std::size_t i = 0; i < pairs.size(); ++i)
    {
        ++b.leafClipStart[pairs[i].first + 1];
        b.leafClipList[i] = pairs[i].second;
    }
    for(std::size_t k = 1; k < b.leafClipStart.size(); ++k)
    {
        b.leafClipStart[k] += b.leafClipStart[k - 1];
    }
    b.clipStamp.assign(b.brushes.size(), 0);
}

struct Result
{
    trace_t trace;
    int brushTests;
};

// A box (mins..maxs about the point) from start to end through a model's brushes, in its own space; Quake's trace.
Result boxTrace(const Brushes& b, const hull_t& hull0, int head, const glm::vec3& start, const glm::vec3& mins,
    const glm::vec3& maxs, const glm::vec3& end)
{
    Result r{};
    trace_t& tr = r.trace;
    tr.fraction = 1.f;
    VectorCopy(end, tr.endpos);
    const glm::dvec3 centre = (glm::dvec3{mins} + glm::dvec3{maxs}) * 0.5;
    const glm::vec3 c{centre};
    Sweep s{&b, &hull0, glm::dvec3{start} + centre, glm::dvec3{end} + centre, (glm::dvec3{maxs} - glm::dvec3{mins}) * 0.5,
        glm::min(start, end) + mins - 1.f, glm::max(start, end) + maxs + 1.f, glm::min(start, end) + c - 1.f,
        glm::max(start, end) + c + 1.f};
    if(head >= 0)
    {
        if(start == end)
        {
            s.startsolid = boxInSolid(b, hull0, head, s.start, s.ext);
            s.getout = !s.startsolid;
        }
        else
        {
            s.clips = head == 0 && !b.leafClipStart.empty();
            ++b.stamp;
            sweepNode(s, head, 0.0, 1.0, s.start, s.end);
        }
    }
    r.brushTests = s.brushTests;
    // Quake's allsolid: the whole move in solid; its trace then keeps fraction 1 and the end (SV_RecursiveHullCheck).
    const bool allsolid = s.startsolid && (!s.getout || boxInSolid(b, hull0, head, s.end, s.ext));
    tr.startsolid = s.startsolid;
    tr.allsolid = allsolid;
    tr.inopen = !allsolid;
    if(allsolid || s.fraction >= 1.0)
    {
        return r;
    }
    tr.fraction = static_cast<float>(s.fraction);
    for(int i = 0; i < 3; ++i)
    {
        tr.endpos[i] = static_cast<float>(start[i] + s.fraction * (end[i] - start[i]));
    }
    if(s.hitPlane)
    {
        VectorCopy(s.hitPlane->normal, tr.plane.normal);
        tr.plane.dist = static_cast<float>(s.hitPlane->dist + support(*s.hitPlane, s.ext));
    }
    return r;
}

int clientNum(const edict_t* ent)
{
    if(!ent || qcvm != &sv.qcvm || !qcvm->edicts || qcvm->edict_size <= 0)
    {
        return -1;
    }
    const std::ptrdiff_t bytes = reinterpret_cast<const byte*>(ent) - reinterpret_cast<const byte*>(qcvm->edicts);
    return static_cast<int>(bytes / qcvm->edict_size);
}

float hull1Height(const qmodel_t* world)
{
    const float h = world ? world->hulls[1].clip_maxs[2] - world->hulls[1].clip_mins[2] : 0.f;
    return h > 0.f ? h : 56.f;
}

// ---------------------------------------------------------------------------------------------------------------
// Test aids: vr_hull_bench (numbers) and vr_hull_walktest (a random walk looking for stuck and fall-through).

struct Bench
{
    double ns = 0.0;
    int brushTests = 0;
};

// Hull 0's solid points in Quake's player box at p (a 2-unit grid): the bench's check of what it reports.
constexpr int boxGridPoints = 17 * 17 * 29;
int hull0PointsInBox(const hull_t& hull0, const glm::vec3& p)
{
    int solid = 0;
    for(float x = -16.f; x <= 16.f; x += 2.f)
    {
        for(float y = -16.f; y <= 16.f; y += 2.f)
        {
            for(float z = -24.f; z <= 32.f; z += 2.f)
            {
                vec3_t q{p.x + x, p.y + y, p.z + z};
                const int contents = SV_HullPointContents(const_cast<hull_t*>(&hull0), 0, q);
                solid += contents == CONTENTS_SOLID ? 1 : (contents == CONTENTS_SKY ? 100000 : 0);
            }
        }
    }
    return solid;
}

void stats_f()
{
    if(!sv.active || !sv.worldmodel)
    {
        Con_Printf("vr_hull_stats: no map\n");
        return;
    }
    const Brushes* b = worldBrushes(sv.worldmodel);
    if(!b)
    {
        return;
    }
    const std::size_t bytes = built.bytes();
    Con_Printf("hull: %s: %d nodes (hulls 1-2: %d clipnodes), %d brushes (%d clip), %d planes (%d bevels), %d slivers "
               "dropped, %.0f KB, built in %.1f ms (clip brushes %.1f); width %g (%s)\n",
        sv.worldmodel->name, b->numnodes, sv.worldmodel->numclipnodes, static_cast<int>(b->brushes.size()),
        static_cast<int>(b->clips.size()), static_cast<int>(b->planes.size()), b->bevels, b->dropped, bytes / 1024.0,
        b->ms, b->clipMs, vr_hull_width.value > 0.f ? widthSetting() : 0.f,
        vr_hull_width.value > 0.f ? "on" : "off: Quake's hull 1");
}

// vr_hull_bench [traces] [width]: random moves from where Quake's player box fits (hull 1), each through hull 1,
// through the brushes with the same 32x32x56 box (they should agree, but for clip brushes: hull 1 only), and with
// the narrow box; times and disagreements (the first few printed, with what hull 0 has there).
void bench_f()
{
    if(!sv.active || !sv.worldmodel)
    {
        Con_Printf("vr_hull_bench [traces] [width]: needs a map\n");
        return;
    }
    const int count = Cmd_Argc() > 1 ? std::max(100, Q_atoi(Cmd_Argv(1))) : 20000;
    const float width = Cmd_Argc() > 2 ? std::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), minWidth, maxWidth)
                                       : (vr_hull_width.value > 0.f ? widthSetting() : 20.f);
    qmodel_t* world = sv.worldmodel;
    built.clipnodes = nullptr; // time a fresh build
    const Brushes* b = worldBrushes(world);
    if(!b)
    {
        return;
    }
    hull_t* hull1 = &world->hulls[1];
    const hull_t& hull0 = world->hulls[0];
    const glm::vec3 wmins{world->mins[0], world->mins[1], world->mins[2]};
    const glm::vec3 wmaxs{world->maxs[0], world->maxs[1], world->maxs[2]};
    std::mt19937 rng{1234u};
    std::uniform_real_distribution<float> u01{0.f, 1.f};
    auto randomPoint = [&] { return wmins + (wmaxs - wmins) * glm::vec3{u01(rng), u01(rng), u01(rng)}; };

    std::vector<glm::vec3> starts, ends;
    int tries = 0;
    while(static_cast<int>(starts.size()) < count && tries < count * 200)
    {
        ++tries;
        glm::vec3 p = randomPoint();
        vec3_t v{p.x, p.y, p.z};
        if(SV_HullPointContents(hull1, hull1->firstclipnode, v) == CONTENTS_SOLID)
        {
            continue;
        }
        glm::vec3 dir{u01(rng) * 2.f - 1.f, u01(rng) * 2.f - 1.f, u01(rng) * 2.f - 1.f};
        if(glm::length(dir) < 0.1f)
        {
            continue;
        }
        starts.push_back(p);
        ends.push_back(p + glm::normalize(dir) * (u01(rng) * 256.f));
    }
    const int n = static_cast<int>(starts.size());
    if(!n)
    {
        Con_Printf("vr_hull_bench: no room found\n");
        return;
    }
    const glm::vec3 m32{-16.f, -16.f, -24.f}, M32{16.f, 16.f, 32.f};
    const glm::vec3 mw{-width * 0.5f, -width * 0.5f, -24.f}, Mw{width * 0.5f, width * 0.5f, 32.f};
    std::vector<trace_t> stock(n), box32(n), boxw(n);
    using Clock = std::chrono::steady_clock;
    Bench bs, b32, bw;
    // Each set twice, the second timed (the first warms the caches).
    for(int pass = 0; pass < 2; ++pass)
    {
        auto t0 = Clock::now();
        for(int i = 0; i < n; ++i)
        {
            trace_t& tr = stock[i];
            memset(&tr, 0, sizeof(tr));
            tr.fraction = 1.f;
            tr.allsolid = true;
            vec3_t a{starts[i].x, starts[i].y, starts[i].z}, e{ends[i].x, ends[i].y, ends[i].z};
            VectorCopy(e, tr.endpos);
            SV_RecursiveHullCheck(hull1, hull1->firstclipnode, 0.f, 1.f, a, e, &tr);
        }
        bs.ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / n;
        b32.brushTests = bw.brushTests = 0;
        t0 = Clock::now();
        for(int i = 0; i < n; ++i)
        {
            const Result r = boxTrace(*b, hull0, 0, starts[i], m32, M32, ends[i]);
            box32[i] = r.trace;
            b32.brushTests += r.brushTests;
        }
        b32.ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / n;
        t0 = Clock::now();
        for(int i = 0; i < n; ++i)
        {
            const Result r = boxTrace(*b, hull0, 0, starts[i], mw, Mw, ends[i]);
            boxw[i] = r.trace;
            bw.brushTests += r.brushTests;
        }
        bw.ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / n;
    }

    // Agreement of the two 32 boxes: more than a unit apart along the move. Hull 1 stopping much sooner (8 units or
    // more) is a clip brush or qbsp's own expansion; a little sooner, qbsp's rounder corners.
    int agree = 0, stockShorter = 0, stockMuch = 0, brushShorter = 0, brushStartSolid = 0, wider = 0;
    float worstStock = 0.f, worstBrush = 0.f;
    for(int i = 0; i < n; ++i)
    {
        const float len = glm::distance(starts[i], ends[i]);
        const float fs = stock[i].allsolid ? 0.f : stock[i].fraction, fb = box32[i].allsolid ? 0.f : box32[i].fraction;
        if(box32[i].startsolid && !stock[i].startsolid)
        {
            ++brushStartSolid;
            if(brushStartSolid <= 3)
            {
                Con_Printf("  brush startsolid at %.0f %.0f %.0f: hull 0 solid points in the box %d of %d\n",
                    starts[i].x, starts[i].y, starts[i].z, hull0PointsInBox(hull0, starts[i]), boxGridPoints);
            }
        }
        const float gap = (fb - fs) * len;
        if(std::abs(gap) <= 1.f)
        {
            ++agree;
        }
        else if(gap > 0.f)
        {
            ++stockShorter;
            stockMuch += gap >= 8.f;
            if(gap >= 8.f && stockMuch <= 3)
            {
                // Hull 0's points in the box half a unit into the face hull 1 stopped at: none, and hull 1 has solid that
                // hull 0 has not (a clip brush).
                const glm::vec3 past = glm::vec3{stock[i].endpos[0], stock[i].endpos[1], stock[i].endpos[2]} +
                                       glm::vec3{stock[i].plane.normal[0], stock[i].plane.normal[1], stock[i].plane.normal[2]} * -0.5f;
                int key = -1;
                for(int num = hull1->firstclipnode; num >= 0;)
                {
                    const mclipnode_t& nd = hull1->clipnodes[num];
                    const mplane_t& pl = hull1->planes[nd.planenum];
                    const int side = pl.normal[0] * past.x + pl.normal[1] * past.y + pl.normal[2] * past.z - pl.dist < 0.f;
                    key = num * 2 + side;
                    num = nd.children[side];
                }
                const int cls = key >= 0 && key < static_cast<int>(b->hull1Clip.size()) ? b->hull1Clip[key] : -3;
                Con_Printf("  hull1 stops %.0f units sooner at %.0f %.0f %.0f: hull 0 solid points in the box %d, hull 1 "
                           "leaf %d: %d; brushes say solid %d\n",
                    gap, stock[i].endpos[0], stock[i].endpos[1], stock[i].endpos[2], hull0PointsInBox(hull0, past), key,
                    cls, boxInSolid(*b, hull0, 0, glm::dvec3{past} + glm::dvec3{0.0, 0.0, 4.0}, glm::dvec3{16.0, 16.0, 28.0}, false) ? 1 : 0);
            }
            worstStock = std::max(worstStock, gap);
        }
        else
        {
            ++brushShorter;
            worstBrush = std::max(worstBrush, -gap);
        }
        if(!boxw[i].startsolid && boxw[i].fraction + 1e-4f < fb)
        {
            ++wider; // the narrow box stopped sooner than the wide one: never expected
        }
    }
    Con_Printf("hullbench %s: build %.1f ms (clips %.1f: %d of %d hull 1 leaves), %d brushes, %d planes; %d moves: hull1 %.0f ns, brush32 %.0f ns (%.1f "
               "brushes), brush%g %.0f ns (%.1f brushes)\n",
        world->name, b->ms, b->clipMs, static_cast<int>(b->clips.size()), b->hull1Leaves,
        static_cast<int>(b->brushes.size()), static_cast<int>(b->planes.size()), n, bs.ns, b32.ns,
        static_cast<double>(b32.brushTests) / n, width, bw.ns, static_cast<double>(bw.brushTests) / n);
    Con_Printf("hullbench %s: 32 vs hull1: %.2f%% agree (1 unit); hull1 shorter %d (%d by 8+, worst %.1f); brush "
               "shorter %d (worst %.1f); brush startsolid %d; narrow stopped sooner %d\n",
        world->name, 100.0 * agree / n, stockShorter, stockMuch, worstStock, brushShorter, worstBrush, brushStartSolid,
        wider);
}

// The random walk: each server frame (VR_ClientPreMove) the first player is driven in a random direction at run
// speed (a new one every so often, now and then a jump), and put down somewhere random (on the floor, where the
// box fits) every few seconds. Counted: frames the box starts in solid (stuck), frames its centre is inside hull 0
// solid (in a wall or through the floor), and frames outside the world.
struct WalkTest
{
    double until = 0.0;
    double nextTurn = 0.0;
    double nextJump = 0.0;
    double nextHop = 0.0;
    float yaw = 0.f;
    std::uint32_t seed = 1;
    int frames = 0, stuck = 0, embedded = 0, outside = 0, hops = 0, hopFails = 0, stuckAfterHop = 0, moverHops = 0, stuckOther = 0;
    double travelled = 0.0;
    glm::vec3 last{0.f};
    bool lastValid = false;
};
WalkTest walkTest;

std::uint32_t nextRandom(std::uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

float random01(std::uint32_t& s)
{
    return static_cast<float>(nextRandom(s) & 0xffffff) / 16777216.f;
}

// Somewhere random the player's box stands (on a floor, in the open, not in liquid).
bool hop(edict_t* ent)
{
    qmodel_t* world = sv.worldmodel;
    glm::vec3 wmins{world->mins[0], world->mins[1], world->mins[2]};
    glm::vec3 wmaxs{world->maxs[0], world->maxs[1], world->maxs[2]};
    WalkTest& w = walkTest;
    // Every other hop near a door, lift, train or button (a pusher), if the map has any.
    if(random01(w.seed) < 0.5f)
    {
        int count = 0;
        edict_t* chosen = nullptr;
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
        {
            edict_t* e = EDICT_NUM(i);
            if(!e->free && static_cast<int>(e->v.solid) == SOLID_BSP && static_cast<int>(e->v.movetype) == MOVETYPE_PUSH &&
                random01(w.seed) * static_cast<float>(++count) < 1.f)
            {
                chosen = e; // reservoir sampling: each pusher alike
            }
        }
        if(chosen)
        {
            wmins = glm::vec3{chosen->v.absmin[0] - 96.f, chosen->v.absmin[1] - 96.f, chosen->v.absmin[2]};
            wmaxs = glm::vec3{chosen->v.absmax[0] + 96.f, chosen->v.absmax[1] + 96.f, chosen->v.absmax[2] + 128.f};
            ++w.moverHops;
        }
    }
    for(int attempt = 0; attempt < 400; ++attempt)
    {
        glm::vec3 p = wmins + (wmaxs - wmins) * glm::vec3{random01(w.seed), random01(w.seed), random01(w.seed)};
        vec3_t v{p.x, p.y, p.z};
        if(SV_PointContents(v) != CONTENTS_EMPTY)
        {
            continue;
        }
        vec3_t down{p.x, p.y, p.z - 4096.f};
        const trace_t tr = SV_Move(v, ent->v.mins, ent->v.maxs, down, MOVE_NOMONSTERS, ent);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f || tr.plane.normal[2] < 0.7f)
        {
            continue;
        }
        vec3_t at;
        VectorCopy(tr.endpos, at);
        if(SV_PointContents(at) != CONTENTS_EMPTY)
        {
            continue;
        }
        vec3_t was;
        VectorCopy(ent->v.origin, was);
        VectorCopy(at, ent->v.origin);
        if(SV_TestEntityPosition(ent)) // on a monster: somewhere else
        {
            VectorCopy(was, ent->v.origin);
            continue;
        }
        ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
        SV_LinkEdict(ent, false);
        w.lastValid = false;
        return true;
    }
    return false;
}

// vr_hull_probe: which of the map's brushes the first player's narrow box is in, and by how much (the plane it is
// least inside of): for a "stuck" report.
void probeTree(const Brushes& b, const hull_t& hull, int num, const glm::dvec3& p, const glm::dvec3& ext)
{
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    const double t = glm::dot(n, p) - plane.dist;
    const double offset = std::abs(ext.x * n.x) + std::abs(ext.y * n.y) + std::abs(ext.z * n.z);
    for(int side = 0; side < 2; ++side)
    {
        if(side == 0 ? t <= -offset - 1.0 : t >= offset + 1.0)
        {
            continue;
        }
        const int child = node.children[side];
        if(child >= 0)
        {
            probeTree(b, hull, child, p, ext);
            continue;
        }
        const std::size_t key = static_cast<std::size_t>(num) * 2 + side;
        auto report = [&](int index)
        {
            const Brush& br = b.brushes[index];
            if(!boxInBrush(b, br, p, ext))
            {
                return;
            }
            double best = -1e300;
            int bestPlane = -1;
            for(std::uint32_t i = 0; i < br.count; ++i)
            {
                const Plane& q = b.planes[br.first + i];
                const double d = glm::dot(p, glm::dvec3{q.normal}) - (q.dist + support(q, ext));
                if(d > best)
                {
                    best = d;
                    bestPlane = static_cast<int>(i);
                }
            }
            const Plane& q = b.planes[br.first + bestPlane];
            Con_Printf("  in %s brush %d (%u planes, %.1f %.1f %.1f .. %.1f %.1f %.1f): least inside plane %d "
                       "(%.6f %.6f %.6f, %.3f) by %.6f\n",
                br.clip ? "clip" : "hull 0", index, br.count, br.mins.x, br.mins.y, br.mins.z, br.maxs.x, br.maxs.y,
                br.maxs.z, bestPlane, q.normal.x, q.normal.y, q.normal.z, q.dist, -best);
        };
        if(b.leafBrush[key] >= 0)
        {
            report(b.leafBrush[key]);
        }
        for(int i = b.leafClipStart.empty() ? 0 : b.leafClipStart[key],
                e = b.leafClipStart.empty() ? 0 : b.leafClipStart[key + 1];
            i < e; ++i)
        {
            report(b.leafClipList[i]);
        }
    }
}

void probe_f()
{
    if(!sv.active || !sv.worldmodel || svs.maxclients < 1)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    edict_t* ent = EDICT_NUM(1);
    float lo[3], hi[3];
    const Brushes* b = worldBrushes(sv.worldmodel);
    const bool narrow = b && moveBox(ent, ent->v.mins, ent->v.maxs, lo, hi);
    PR_PopQCVM(oldvm);
    if(!narrow)
    {
        Con_Printf("vr_hull_probe: the narrow box is off\n");
        return;
    }
    const glm::dvec3 c = glm::dvec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} +
                         (glm::dvec3{lo[0], lo[1], lo[2]} + glm::dvec3{hi[0], hi[1], hi[2]}) * 0.5;
    const glm::dvec3 e = (glm::dvec3{hi[0], hi[1], hi[2]} - glm::dvec3{lo[0], lo[1], lo[2]}) * 0.5;
    Con_Printf("vr_hull_probe at %.3f %.3f %.3f: %s\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2],
        boxInSolid(*b, sv.worldmodel->hulls[0], 0, c, e) ? "in solid" : "free");
    probeTree(*b, sv.worldmodel->hulls[0], 0, c, e);
}

void walkTest_f()
{
    WalkTest& w = walkTest;
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_hull_walktest <seconds> [seed]: a random walk of the player (god and notarget it first); "
                   "the counts at the end\n");
        return;
    }
    w = WalkTest{};
    w.until = sv.qcvm.time + Q_atof(Cmd_Argv(1));
    w.seed = Cmd_Argc() > 2 ? static_cast<std::uint32_t>(std::max(1, Q_atoi(Cmd_Argv(2)))) : 1u;
    w.nextHop = sv.qcvm.time + 4.0;
}

void onWidthChanged(cvar_t*)
{
    if(vr_hull_width.value > 0.f && sv.active && sv.worldmodel)
    {
        (void)worldBrushes(sv.worldmodel);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_hull_stats", stats_f);
    Cmd_AddCommand("vr_hull_bench", bench_f);
    Cmd_AddCommand("vr_hull_walktest", walkTest_f);
    Cmd_AddCommand("vr_hull_probe", probe_f);
    Cvar_SetCallback(&vr_hull_width, onWidthChanged);
}

void afterLoad()
{
    if(vr_hull_width.value > 0.f && sv.worldmodel)
    {
        (void)worldBrushes(sv.worldmodel);
    }
}

bool moveBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs)
{
    if(vr_hull_width.value <= 0.f || !sv.active || !sv.worldmodel)
    {
        return false;
    }
    const int num = clientNum(passedict);
    if(num < 1 || num > svs.maxclients)
    {
        return false;
    }
    if(maxs[0] - mins[0] != 32.f || maxs[1] - mins[1] != 32.f)
    {
        return false; // not the player's own box (a point, or a box QC traces with)
    }
    const float half = widthSetting() * 0.5f;
    const float cx = (mins[0] + maxs[0]) * 0.5f, cy = (mins[1] + maxs[1]) * 0.5f;
    boxMins[0] = cx - half;
    boxMins[1] = cy - half;
    boxMins[2] = mins[2];
    boxMaxs[0] = cx + half;
    boxMaxs[1] = cy + half;
    boxMaxs[2] = mins[2] + hull1Height(sv.worldmodel); // hull 1's height, from the box's feet as Quake places it
    return true;
}

bool clipBSP(const edict_t* ent, const float* start, const float* boxMins, const float* boxMaxs, const float* end,
    trace_t& trace)
{
    const int index = static_cast<int>(ent->v.modelindex);
    qmodel_t* model = index >= 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model || model->type != mod_brush)
    {
        return false;
    }
    const Brushes* b = worldBrushes(sv.worldmodel);
    if(!b || model->hulls[0].clipnodes != b->clipnodes)
    {
        return false;
    }
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 s = glm::vec3{start[0], start[1], start[2]} - origin;
    const glm::vec3 e = glm::vec3{end[0], end[1], end[2]} - origin;
    ++vr_profcounts.hullchecks;
    trace = boxTrace(*b, model->hulls[0], model->hulls[0].firstclipnode, s,
        glm::vec3{boxMins[0], boxMins[1], boxMins[2]}, glm::vec3{boxMaxs[0], boxMaxs[1], boxMaxs[2]}, e)
                .trace;
    for(int i = 0; i < 3; ++i)
    {
        trace.endpos[i] += origin[i];
    }
    return true;
}

int playerBoxFits(qmodel_t* world, const glm::vec3& start, const glm::vec3& end)
{
    if(vr_hull_width.value <= 0.f || !world || world != sv.worldmodel || !sv.active)
    {
        return -1;
    }
    const Brushes* b = worldBrushes(world);
    if(!b)
    {
        return -1;
    }
    const float half = widthSetting() * 0.5f;
    const trace_t tr = boxTrace(*b, world->hulls[0], 0, start, glm::vec3{-half, -half, -24.f},
        glm::vec3{half, half, -24.f + hull1Height(world)}, end)
                           .trace;
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.f;
}

void walkTestFrame(edict_t* ent)
{
    WalkTest& w = walkTest;
    if(w.until <= 0.0 || clientNum(ent) != 1)
    {
        return;
    }
    if(sv.qcvm.time >= w.until)
    {
        Con_Printf("hullwalk %s: width %g, %d frames, %.0f units walked, %d hops (%d near pushers, %d failed), stuck %d "
                   "(%d right after a hop), in monsters or items %d, embedded %d, outside %d\n",
            sv.worldmodel->name, vr_hull_width.value > 0.f ? widthSetting() : 32.f, w.frames, w.travelled, w.hops,
            w.moverHops, w.hopFails, w.stuck, w.stuckAfterHop, w.stuckOther, w.embedded, w.outside);
        w.until = 0.0;
        return;
    }
    ++w.frames;
    const glm::vec3 o{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    if(w.lastValid)
    {
        w.travelled += glm::distance(o, w.last);
    }
    const bool afterHop = !w.lastValid;
    w.last = o;
    w.lastValid = true;
    edict_t* in = SV_TestEntityPosition(ent);
    if(in && static_cast<int>(in->v.solid) != SOLID_BSP)
    {
        if(++w.stuckOther <= 3) // in a monster or an item: not the map's collision
        {
            Con_Printf("hullwalk: in %s at %.1f %.1f %.1f\n", PR_GetString(in->v.classname), o.x, o.y, o.z);
        }
    }
    else if(in)
    {
        ++w.stuck;
        w.stuckAfterHop += afterHop;
        if(w.stuck <= 5)
        {
            // In what: an entity, or the world's hull 0 brushes, or (neither) a recovered clip brush.
            const char* what = in == qcvm->edicts ? "the world's clip brushes" : PR_GetString(in->v.classname);
            if(in == qcvm->edicts && built.clipnodes)
            {
                float lo[3], hi[3];
                if(moveBox(ent, ent->v.mins, ent->v.maxs, lo, hi))
                {
                    const glm::dvec3 c = glm::dvec3{o} + (glm::dvec3{lo[0], lo[1], lo[2]} + glm::dvec3{hi[0], hi[1], hi[2]}) * 0.5;
                    const glm::dvec3 e = (glm::dvec3{hi[0], hi[1], hi[2]} - glm::dvec3{lo[0], lo[1], lo[2]}) * 0.5;
                    if(boxInSolid(built, sv.worldmodel->hulls[0], 0, c, e, false))
                    {
                        what = "the world's hull 0 brushes";
                    }
                }
            }
            Con_Printf("hullwalk: stuck at %.1f %.1f %.1f in %s (velocity %.0f %.0f %.0f)\n", o.x, o.y, o.z, what,
                ent->v.velocity[0], ent->v.velocity[1], ent->v.velocity[2]);
        }
    }
    vec3_t v{o.x, o.y, o.z};
    if(SV_HullPointContents(&sv.worldmodel->hulls[0], 0, v) == CONTENTS_SOLID)
    {
        ++w.embedded;
        if(w.embedded <= 5)
        {
            Con_Printf("hullwalk: embedded at %.1f %.1f %.1f\n", o.x, o.y, o.z);
        }
    }
    for(int i = 0; i < 3; ++i)
    {
        if(o[i] < sv.worldmodel->mins[i] - 64.f || o[i] > sv.worldmodel->maxs[i] + 64.f)
        {
            ++w.outside;
            break;
        }
    }
    if(sv.qcvm.time >= w.nextHop)
    {
        if(hop(ent))
        {
            ++w.hops;
        }
        else
        {
            ++w.hopFails;
        }
        w.nextHop = sv.qcvm.time + 3.0 + 4.0 * random01(w.seed);
    }
    if(sv.qcvm.time >= w.nextTurn)
    {
        w.yaw = random01(w.seed) * 6.2831853f;
        w.nextTurn = sv.qcvm.time + 0.3 + 1.2 * random01(w.seed);
    }
    const float speed = 320.f;
    ent->v.velocity[0] = std::cos(w.yaw) * speed;
    ent->v.velocity[1] = std::sin(w.yaw) * speed;
    if(sv.qcvm.time >= w.nextJump && ((int)ent->v.flags & FL_ONGROUND))
    {
        ent->v.velocity[2] = 270.f;
        ent->v.flags = static_cast<float>((int)ent->v.flags & ~FL_ONGROUND);
        w.nextJump = sv.qcvm.time + 0.5 + 3.0 * random01(w.seed);
    }
}

} // namespace qvr::hull

extern "C" int VR_HullMoveBox(edict_t* passedict, const float* mins, const float* maxs, float* boxmins, float* boxmaxs)
{
    return qvr::hull::moveBox(passedict, mins, maxs, boxmins, boxmaxs);
}

extern "C" int VR_HullClipBSP(edict_t* ent, const float* start, const float* boxmins, const float* boxmaxs,
    const float* end, trace_t* trace)
{
    return qvr::hull::clipBSP(ent, start, boxmins, boxmaxs, end, *trace);
}
