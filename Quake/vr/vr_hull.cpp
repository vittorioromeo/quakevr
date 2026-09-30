// vr_hull.cpp -- see vr_hull.hpp.

#include "vr_hull.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <tuple>
#include <unordered_map>
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

// A brush model's brushes: one of the world's submodels (its doors, lifts, func_walls), or an external .bsp's model
// (maps/b_*.bsp, vrfiringrange's panels and tables), whose hull 0 has its own nodes: its leaves' entries in leafBrush
// start at `base`.
struct SubModel
{
    const mclipnode_t* clipnodes; // its hull 0's
    int head;                     // its head node in them
    std::uint32_t base;           // its hull 0's leaves in leafBrush: [base + node * 2 + side]
    std::uint32_t firstBrush, numBrushes;
};

// The server map's solid space as brushes: pointers into the hunk (the world's hull 0), released at every map change.
struct Brushes
{
    const mclipnode_t* clipnodes = nullptr; // the hull 0 they were built from (the world's and its brush models')
    std::vector<Plane> planes;
    std::vector<Brush> brushes;
    std::vector<int> leafBrush; // [base + node * 2 + side]: the brush of that child when it is a solid leaf, else -1
    std::vector<SubModel> subs; // the world's submodels (0: the world), then external models as they are met
    std::vector<int> modelSub;  // [modelindex]: its entry in subs; -1 none (not a brush model), -2 not looked up
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
        return std::tie(clipnodes, planes, brushes, leafBrush, subs, modelSub, clips, leafClipStart, leafClipList, clipStamp, stamp, hull1Clip, ms,
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
    int tag = -1; // method A's build (Tree): the face's plane in the tree's table while not yet split on, else -1
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
        Face fb{f.normal, f.dist, {}, f.tag};
        clipWinding(f.w, n, d, false, fb.w);
        clipWinding(f.w, n, d, true, tmp);
        if(!tmp.empty())
        {
            front.push_back(Face{f.normal, f.dist, tmp, f.tag});
        }
        if(!fb.w.empty())
        {
            back.push_back(std::move(fb));
        }
        else if(tmp.empty())
        {
            // Lost to the epsilon on both sides (a sliver of a face): its plane still bounds both pieces. Dropped, a
            // piece could lose its only bound that way (found with a monster's 24-wide hull on e1m4: a piece reaching
            // to the bogus winding's end, its leaf solid out in the open).
            front.push_back(Face{f.normal, f.dist, {}, f.tag});
            back.push_back(Face{f.normal, f.dist, {}, f.tag});
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

// The walk shared out on the game's thread pool: the tree's top `walkSplitDepth` levels split here, in the walk's order,
// into items: the subtrees below them, and the solid leaves' pieces met on the way down; each item then walked on its
// own (a worker's or this thread) with its own output, and the outputs merged in the items' order: the same pieces, in
// the same order, as the walk on one thread.
constexpr int walkSplitDepth = 9; // up to 512 subtrees

struct WalkItem
{
    int num;  // the subtree's node; a piece's: its leaf's parent node
    int side; // -1: the subtree from num; 0, 1: that child of num, a solid leaf, and its piece
    Poly poly;
};

void splitTop(const hull_t& hull, int num, Poly&& poly, int depth, std::vector<WalkItem>& items)
{
    if(num < hull.firstclipnode || num > hull.lastclipnode)
    {
        return;
    }
    if(depth == 0)
    {
        items.push_back(WalkItem{num, -1, std::move(poly)});
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
            splitTop(hull, child, std::move(parts[side]), depth - 1, items);
        }
        else if(solidContents(child))
        {
            items.push_back(WalkItem{num, side, std::move(parts[side])});
        }
    }
}

// Each item walked (items[i]'s pieces to onSolid(num, side, piece, outs[i])), on the pool.
template <class Out, class OnSolid>
void walkItems(const hull_t& hull, std::vector<WalkItem>& items, std::vector<Out>& outs, const OnSolid& onSolid)
{
    outs.clear();
    outs.resize(items.size());
    jobs::parallelFor(items.size(), 1,
        [&](std::size_t begin, std::size_t end)
        {
            for(std::size_t i = begin; i < end; ++i)
            {
                Out& out = outs[i];
                auto on = [&](int num, int side, const Poly& piece) { onSolid(num, side, piece, out); };
                if(items[i].side < 0)
                {
                    walk(hull, items[i].num, std::move(items[i].poly), on);
                }
                else
                {
                    on(items[i].num, items[i].side, items[i].poly);
                }
                items[i].poly = Poly{};
            }
        });
}

void recoverClips(Brushes& b, qmodel_t* world);

// A model's brushes: hull 0's tree walked from its head node (numnodes: its hull 0's nodes; base: their leaves' place in
// leafBrush). Its bounds and a margin: the world's outside is solid in hull 0, cut off here (no one gets there).
void addSubModel(Brushes& b, const hull_t& hull0, int numnodes, std::uint32_t base, int head, const float* mins,
    const float* maxs)
{
    const auto first = static_cast<std::uint32_t>(b.brushes.size());
    if(head >= 0 && head < numnodes)
    {
        hull_t h = hull0;
        h.firstclipnode = 0;
        h.lastclipnode = numnodes - 1;
        const glm::dvec3 margin{64.0};
        const glm::dvec3 lo = glm::dvec3{mins[0], mins[1], mins[2]} - margin;
        const glm::dvec3 hi = glm::dvec3{maxs[0], maxs[1], maxs[2]} + margin;
        auto onSolid = [&b, base](int num, int side, const Poly& piece)
        { b.leafBrush[base + static_cast<std::size_t>(num) * 2 + side] = emitBrush(b, piece); };
        walk(h, head, boxPoly(lo, hi), onSolid);
    }
    b.subs.push_back(SubModel{hull0.clipnodes, head, base, first, static_cast<std::uint32_t>(b.brushes.size()) - first});
}

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
    b.subs.clear();
    b.modelSub.assign(MAX_MODELS, -2);
    // The world's models (addSubModel's walk for each), their walks shared out on the pool together.
    hull_t h = hull0;
    h.firstclipnode = 0;
    h.lastclipnode = b.numnodes - 1;
    std::vector<WalkItem> items;
    std::vector<std::size_t> subEnd(static_cast<std::size_t>(std::max(world->numsubmodels, 0)));
    for(int i = 0; i < world->numsubmodels; ++i)
    {
        const dmodel_t& sub = world->submodels[i];
        const int head = sub.headnode[0];
        if(head >= 0 && head < b.numnodes)
        {
            const glm::dvec3 margin{64.0};
            splitTop(h, head,
                boxPoly(glm::dvec3{sub.mins[0], sub.mins[1], sub.mins[2]} - margin,
                    glm::dvec3{sub.maxs[0], sub.maxs[1], sub.maxs[2]} + margin),
                walkSplitDepth, items);
        }
        subEnd[static_cast<std::size_t>(i)] = items.size();
    }
    struct Out
    {
        Brushes b; // the item's brushes and their planes, numbered from 0
        std::vector<std::pair<std::size_t, int>> leaves; // (leaf key, its brush in b or -1)
    };
    std::vector<Out> outs;
    walkItems(h, items, outs,
        [](int num, int side, const Poly& piece, Out& out)
        { out.leaves.emplace_back(static_cast<std::size_t>(num) * 2 + side, emitBrush(out.b, piece)); });
    std::size_t item = 0;
    for(int i = 0; i < world->numsubmodels; ++i)
    {
        const auto first = static_cast<std::uint32_t>(b.brushes.size());
        for(; item < subEnd[static_cast<std::size_t>(i)]; ++item)
        {
            Out& o = outs[item];
            const auto planeBase = static_cast<std::uint32_t>(b.planes.size());
            const auto brushBase = static_cast<int>(b.brushes.size());
            b.planes.insert(b.planes.end(), o.b.planes.begin(), o.b.planes.end());
            for(Brush br : o.b.brushes)
            {
                br.first += planeBase;
                b.brushes.push_back(br);
            }
            for(const auto& [key, brush] : o.leaves)
            {
                b.leafBrush[key] = brush < 0 ? -1 : brush + brushBase;
            }
            b.bevels += o.b.bevels;
            b.dropped += o.b.dropped;
        }
        b.subs.push_back(SubModel{hull0.clipnodes, world->submodels[i].headnode[0], 0, first,
            static_cast<std::uint32_t>(b.brushes.size()) - first});
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

void settle();

// The server's map as brushes, built now if not yet (the setting turned on during a map: a hitch, once).
const Brushes* worldBrushes(qmodel_t* world)
{
    settle(); // (the map load's builds on the pool)
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
    std::size_t base = 0; // the model's leaves in leafBrush (SubModel::base)
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
    const int brush = s.b->leafBrush[s.base + key];
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
bool boxInTree(const Brushes& b, const hull_t& hull, int num, const glm::dvec3& p, const glm::dvec3& ext, bool clips,
    std::size_t base)
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
            if(boxInTree(b, hull, child, p, ext, clips, base))
            {
                return true;
            }
        }
        else
        {
            const std::size_t key = static_cast<std::size_t>(num) * 2 + side;
            if(const int brush = b.leafBrush[base + key]; brush >= 0 && boxInBrush(b, b.brushes[brush], p, ext))
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
    bool clips = true, std::size_t base = 0)
{
    ++b.stamp;
    return boxInTree(b, hull, head, p, ext, clips && head == 0 && base == 0 && !b.leafClipStart.empty(), base);
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
    // Each walk item's (walkItems): its clip brushes and their planes, numbered from 0.
    struct Out
    {
        std::vector<bool> open; // the piece's faces' (a buffer)
        std::vector<Plane> planes;
        std::vector<Brush> brushes;
        std::vector<std::pair<std::size_t, bool>> leaves; // (hull 1 leaf key, a clip brush: its next in brushes)
        int hull1Leaves = 0;
        std::size_t tests = 0; // boxInSolid's (b.stamp's count)
    };
    // (b is only read here: its hull 0 brushes, complete)
    auto onSolid = [&b, &h1, &h0, &sub, head, lift, e32](int num1, int side1, const Poly& piece, Out& out)
    {
        ++out.hull1Leaves;
        std::vector<bool>& open = out.open;
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
            ++out.tests; // (boxInSolid without its clip brushes, b.stamp counted in the merge)
            return !boxInTree(b, h0, 0, p + lift, e32, false, 0);
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
        out.leaves.emplace_back(static_cast<std::size_t>(num1) * 2 + side1, clip);
        if(!clip)
        {
            return;
        }
        // Kept in the box centre's space; the open faces shrink back with a narrower box.
        const std::size_t first = out.planes.size();
        for(std::size_t fi = 0; fi < piece.size(); ++fi)
        {
            const Face& f = piece[fi];
            const double dist = f.dist + glm::dot(f.normal, lift);
            const double reach =
                std::abs(f.normal.x) * e32.x + std::abs(f.normal.y) * e32.y + std::abs(f.normal.z) * e32.z;
            out.planes.push_back(
                Plane{glm::vec3{f.normal}, static_cast<float>(open[fi] ? dist - reach : dist), open[fi] ? 1.f : 0.f});
        }
        out.brushes.push_back(Brush{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(out.planes.size() - first),
            glm::vec3{lo + lift}, glm::vec3{hi + lift}, true});
    };
    const glm::dvec3 margin{96.0};
    std::vector<WalkItem> items;
    splitTop(h1, head,
        boxPoly(glm::dvec3{sub.mins[0], sub.mins[1], sub.mins[2]} - margin,
            glm::dvec3{sub.maxs[0], sub.maxs[1], sub.maxs[2]} + margin),
        walkSplitDepth, items);
    std::vector<Out> outs;
    walkItems(h1, items, outs, onSolid);
    for(const Out& o : outs)
    {
        b.hull1Leaves += o.hull1Leaves;
        b.stamp += o.tests;
        std::size_t next = 0;
        for(const auto& [key, clip] : o.leaves)
        {
            b.hull1Clip[key] = clip ? static_cast<int>(b.brushes.size()) : -1;
            if(!clip)
            {
                continue;
            }
            Brush br = o.brushes[next++];
            const auto from = o.planes.begin() + br.first;
            br.first = static_cast<std::uint32_t>(b.planes.size());
            b.planes.insert(b.planes.end(), from, from + br.count);
            b.brushes.push_back(br);
            b.clips.push_back(static_cast<int>(b.brushes.size()) - 1);
        }
    }

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
    const glm::vec3& maxs, const glm::vec3& end, std::size_t base = 0)
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
    s.base = base;
    if(head >= 0)
    {
        if(start == end)
        {
            s.startsolid = boxInSolid(b, hull0, head, s.start, s.ext, true, base);
            s.getout = !s.startsolid;
        }
        else
        {
            s.clips = head == 0 && base == 0 && !b.leafClipStart.empty();
            ++b.stamp;
            sweepNode(s, head, 0.0, 1.0, s.start, s.end);
        }
    }
    r.brushTests = s.brushTests;
    // Quake's allsolid: the whole move in solid; its trace then keeps fraction 1 and the end (SV_RecursiveHullCheck).
    const bool allsolid = s.startsolid && (!s.getout || boxInSolid(b, hull0, head, s.end, s.ext, true, base));
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

// ---------------------------------------------------------------------------------------------------------------
// Method A (vr_hull_method 1): a clipping hull compiled for the box at load, as qbsp compiles hull 1. Each of a model's
// brushes (hull 0's, with their bevels, and the world's recovered clip brushes) is grown by the box: its planes moved
// out by the box's reach, which with Quake 2's bevels is exactly the brush's Minkowski sum with the box. A BSP tree is
// compiled from the grown brushes, their faces the splitting planes: a leaf is solid where one grown brush holds all of
// it (every face of that brush already split on, the leaf on its inside), empty where none reaches. Being Quake's own
// clipnodes and planes, it is traced by SV_RecursiveHullCheck exactly as hull 1 is (in the box centre's space).

struct Tree
{
    std::vector<mclipnode_t> nodes;
    std::vector<mplane_t> planes;
    std::vector<int> heads;                    // [sub]: its tree's root in nodes; -1 not built yet
    const mclipnode_t* forClipnodes = nullptr; // the Brushes it was built from (their world's hull 0)
    glm::vec3 ext{0.f};                        // the half size of the box it was built for
    int redone = 0;                            // pieces of its builds on the pool done again on one thread (buildTree)
    double ms = 0.0;                           // the builds so far
    int solidLeaves = 0, emptyLeaves = 0;
    auto members() { return std::tie(nodes, planes, heads, forClipnodes, ext, ms, solidLeaves, emptyLeaves, redone); }
};
mem::Cache<Tree> tree{"hull tree", mem::MapChange};

// Its bytes, for a set holding trees (found by the set's heldBytes of a vector of them).
std::size_t heldBytes(const Tree& t)
{
    return mem::heldBytes(t.nodes) + mem::heldBytes(t.planes) + mem::heldBytes(t.heads);
}

// Monsters' trees (vr_mhull): one per box size their widths ask for (a few: the widths come from the classes'
// settings); the player's size shares the player's tree.
struct MonsterTrees
{
    std::vector<Tree> trees;
    auto members() { return std::tie(trees); }
};
mem::Cache<MonsterTrees> monsterTrees{"hull monster trees", mem::MapChange};
constexpr std::size_t maxMonsterTrees = 12; // more box sizes than this at once: all compiled again (not expected)

// A piece of a grown brush in a node of the tree being built.
struct Frag
{
    Poly poly;
    glm::dvec3 lo, hi;
    int live = 0; // its faces (a winding left) on planes not yet split on: none, and it fills its node's space
    const Brush* brush = nullptr; // the brush it is a piece of
};

// A plane asked of the table (TreeBuilder::plane) by a builder on the pool: what it asked and what it got (the merge
// asks the tree's table the same, in the order the build on one thread would have: see Merge).
struct PlaneAsk
{
    glm::dvec3 n;
    double d;
    int id;
};

class TreeBuilder
{
public:
    // The builder of a tree: its planes and nodes go straight into t.
    explicit TreeBuilder(Tree& t)
        : planes_{&t.planes}, nodes_{&t.nodes}, solid_{&t.solidLeaves}, empty_{&t.emptyLeaves}, ext_{t.ext}
    {
        for(std::size_t i = 0; i < planes_->size(); ++i)
        {
            index_[key((*planes_)[i].dist)].push_back(static_cast<int>(i));
        }
    }

    // A builder on the pool over `base` (its table as it is now, only read: base does not change while this one works):
    // the planes it adds numbered after base's, its nodes from 0, its leaves counted in solid and empty, and every plane
    // it asks for logged.
    TreeBuilder(const TreeBuilder& base, std::vector<mplane_t>& planes, std::vector<mclipnode_t>& nodes, int& solid,
        int& empty, std::vector<PlaneAsk>& log)
        : base_{&base}, baseCount_{base.count()}, planes_{&planes}, nodes_{&nodes}, solid_{&solid}, empty_{&empty},
          ext_{base.ext_}, log_{&log}
    {
    }

    TreeBuilder(const TreeBuilder&) = delete;
    TreeBuilder& operator=(const TreeBuilder&) = delete;

    // The table's planes (base's, then this one's).
    [[nodiscard]] std::size_t count() const { return baseCount_ + planes_->size(); }
    [[nodiscard]] const mplane_t& planeAt(int i) const
    {
        const auto u = static_cast<std::size_t>(i);
        return u < baseCount_ ? base_->planeAt(i) : (*planes_)[u - baseCount_];
    }

    // The table's plane for n, d (either way round: the table's faces its larger axis positive, as qbsp's do).
    int plane(glm::dvec3 n, double d)
    {
        const glm::dvec3 askedN = n;
        const double askedD = d;
        for(int a = 0; a < 3; ++a)
        {
            if(std::abs(n[a]) > 1.0 - 1e-6)
            {
                const double s = n[a] > 0.0 ? 1.0 : -1.0;
                n = glm::dvec3{0.0};
                n[a] = s;
            }
        }
        const glm::dvec3 a = glm::abs(n);
        const int major = a.x >= a.y && a.x >= a.z ? 0 : (a.y >= a.z ? 1 : 2);
        if(n[major] < 0.0)
        {
            n = -n;
            d = -d;
        }
        const long long k = key(static_cast<float>(d));
        int id = -1;
        for(long long kk = k - 1; kk <= k + 1 && id < 0; ++kk)
        {
            id = find(kk, n, d);
        }
        if(id < 0)
        {
            mplane_t p{};
            for(int i = 0; i < 3; ++i)
            {
                p.normal[i] = static_cast<float>(n[i]);
                p.signbits = static_cast<byte>(p.signbits | (p.normal[i] < 0.f ? (1 << i) : 0));
            }
            p.dist = static_cast<float>(d);
            p.type = static_cast<byte>(a.x > 1.0 - 1e-6 ? 0 : (a.y > 1.0 - 1e-6 ? 1 : (a.z > 1.0 - 1e-6 ? 2 : 3 + major)));
            planes_->push_back(p);
            id = static_cast<int>(count()) - 1;
            index_[k].push_back(id);
        }
        if(log_)
        {
            log_->push_back(PlaneAsk{askedN, askedD, id});
        }
        return id;
    }

    // The planes added since the table had `to` let go (the last first; the tree's builder only).
    void rollback(std::size_t to)
    {
        while(planes_->size() > to)
        {
            index_[key(planes_->back().dist)].pop_back(); // (the plane added last is its key's last)
            planes_->pop_back();
        }
    }

    // A brush grown by the box as a piece (false: nothing left of it).
    bool grow(const Brushes& b, const Brush& br, const glm::dvec3& ext, Frag& out)
    {
        const glm::dvec3 pad = br.clip ? glm::dvec3{2.0} : ext + 2.0;
        Poly p = boxPoly(glm::dvec3{br.mins} - pad, glm::dvec3{br.maxs} + pad), front, back;
        for(std::uint32_t i = 0; i < br.count; ++i)
        {
            const Plane& q = b.planes[br.first + i];
            const glm::dvec3 n0{q.normal};
            const int tag = plane(n0, q.dist + support(q, ext));
            glm::dvec3 n;
            double d;
            oriented(tag, n0, n, d);
            splitPoly(std::move(p), n, d, front, back);
            if(back.empty())
            {
                return false;
            }
            if(!front.empty())
            {
                back.back().tag = tag; // the cut's face
            }
            p = std::move(back);
            back = Poly{};
        }
        for(Face& f : p)
        {
            if(f.tag < 0 && !f.w.empty()) // the starting box's (only if a brush had no axial bevel there)
            {
                f.tag = plane(f.normal, f.dist);
            }
        }
        out.poly = std::move(p);
        out.brush = &br;
        return finish(out, -1) && bounded(out);
    }

    // Debugging (vr_hull_leafdebug): the leaf holding this point is described as it is made.
    const glm::dvec3* watch = nullptr;
    const Brushes* debugBrushes = nullptr;
    int rebounded = 0; // pieces cut back to their brush's bounds (bounded)

    // The pieces' leaf: its contents (counted), or 0 when they need a node.
    int leaf(const std::vector<Frag>& frags)
    {
        if(frags.empty())
        {
            ++*empty_;
            return CONTENTS_EMPTY;
        }
        for(const Frag& f : frags)
        {
            if(f.live == 0)
            {
                ++*solid_;
                return CONTENTS_SOLID;
            }
        }
        return 0;
    }

    // The pieces split by the plane chosen for their node (returned; n, d: it facing its way) into the front's and
    // the back's.
    int split(std::vector<Frag>& frags, std::vector<Frag> (&sides)[2], glm::dvec3& n, double& d)
    {
        const int split = choose(frags);
        const mplane_t& mp = planeAt(split);
        n = glm::dvec3{mp.normal[0], mp.normal[1], mp.normal[2]};
        d = mp.dist;
        Poly parts[2];
        for(Frag& f : frags)
        {
            splitPoly(std::move(f.poly), n, d, parts[0], parts[1]);
            for(int side = 0; side < 2; ++side)
            {
                if(parts[side].empty())
                {
                    continue;
                }
                Frag g;
                g.brush = f.brush;
                g.poly = std::move(parts[side]);
                parts[side] = Poly{};
                if(finish(g, split, n, d, side ? -1.0 : 1.0) && bounded(g))
                {
                    sides[side].push_back(std::move(g));
                }
            }
        }
        frags.clear();
        frags.shrink_to_fit();
        return split;
    }

    // The tree of the pieces; its root (a node, or a leaf's contents). region: the node's space (only when watching).
    int build(std::vector<Frag>& frags, Poly* region = nullptr)
    {
        bool watched = false;
        if(watch && region)
        {
            watched = true;
            for(const Face& face : *region)
            {
                watched = watched && glm::dot(face.normal, *watch) - face.dist <= 0.0;
            }
        }
        if(watched && !frags.empty())
        {
            for(const Frag& f : frags)
            {
                if(f.live == 0)
                {
                    describe(f, *region);
                    break;
                }
            }
        }
        if(const int contents = leaf(frags))
        {
            return contents;
        }
        std::vector<Frag> sides[2];
        glm::dvec3 n;
        double d;
        const int split = this->split(frags, sides, n, d);
        Poly regions[2];
        if(watched)
        {
            splitPoly(std::move(*region), n, d, regions[0], regions[1]);
        }
        const int node = static_cast<int>(nodes_->size());
        nodes_->push_back(mclipnode_t{split, {0, 0}});
        const int front = build(sides[0], watched ? &regions[0] : nullptr);
        const int back = build(sides[1], watched ? &regions[1] : nullptr);
        (*nodes_)[static_cast<std::size_t>(node)].children[0] = front;
        (*nodes_)[static_cast<std::size_t>(node)].children[1] = back;
        return node;
    }

    // Its leaf counts added (a merged piece of the build's).
    void addLeaves(int solid, int empty)
    {
        *solid_ += solid;
        *empty_ += empty;
    }

    [[nodiscard]] std::vector<mclipnode_t>& nodes() { return *nodes_; }

private:
    static long long key(float d) { return static_cast<long long>(std::floor(d * 4.f)); }

    // The first plane of the key's within qbsp's epsilons of n, d (in the order they were added: base's first), else -1.
    int find(long long kk, const glm::dvec3& n, double d) const
    {
        if(base_)
        {
            if(const int id = base_->find(kk, n, d); id >= 0)
            {
                return id;
            }
        }
        const auto found = index_.find(kk);
        if(found == index_.end())
        {
            return -1;
        }
        for(const int id : found->second)
        {
            const mplane_t& p = planeAt(id);
            // qbsp's epsilons (a normal's components, not their dot: far from the origin a small turn is far off)
            if(std::abs(p.dist - d) < 0.01 && std::abs(p.normal[0] - n.x) < 1e-5 && std::abs(p.normal[1] - n.y) < 1e-5 &&
                std::abs(p.normal[2] - n.z) < 1e-5)
            {
                return id;
            }
        }
        return -1;
    }

    // vr_hull_leafdebug: the solid piece making the watched point's leaf.
    void describe(const Frag& f, const Poly& region) const
    {
        glm::dvec3 lo{1e300}, hi{-1e300};
        for(const Face& face : region)
        {
            for(const glm::dvec3& v : face.w)
            {
                lo = glm::min(lo, v);
                hi = glm::max(hi, v);
            }
        }
        Con_Printf("leaf: solid by a piece %.2f %.2f %.2f .. %.2f %.2f %.2f; the leaf %.2f %.2f %.2f .. %.2f %.2f %.2f\n",
            f.lo.x, f.lo.y, f.lo.z, f.hi.x, f.hi.y, f.hi.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
        for(const Face& face : f.poly)
        {
            Con_Printf("  face %.3f %.3f %.3f d %.3f, %d points, tag %d\n", face.normal.x, face.normal.y, face.normal.z,
                face.dist, static_cast<int>(face.w.size()), face.tag);
        }
        if(f.brush && debugBrushes)
        {
            const Brush& br = *f.brush;
            Con_Printf("  brush %d %s %.2f %.2f %.2f .. %.2f %.2f %.2f, %u planes:\n",
                static_cast<int>(&br - debugBrushes->brushes.data()), br.clip ? "(clip)" : "", br.mins.x, br.mins.y,
                br.mins.z, br.maxs.x, br.maxs.y, br.maxs.z, br.count);
            for(std::uint32_t i = 0; i < br.count; ++i)
            {
                const Plane& q = debugBrushes->planes[br.first + i];
                Con_Printf("    plane %.4f %.4f %.4f d %.3f\n", q.normal.x, q.normal.y, q.normal.z, q.dist);
            }
        }
    }

    // A piece reaching out of its grown brush's bounds (the padded box it was cut from) lost a bounding face to the
    // epsilon (a sliver of one: the build can't tell) and runs on to the bogus winding's end: its leaf would be solid
    // out in the open (found with a monster's 24-wide hull on e1m4, a recovered clip brush's piece). It is cut by that
    // box's faces again, as live faces. False: nothing left of it.
    bool bounded(Frag& f)
    {
        if(!f.brush)
        {
            return true;
        }
        const glm::dvec3 pad = f.brush->clip ? glm::dvec3{2.0} : glm::dvec3{ext_} + 2.0;
        const glm::dvec3 lo = glm::dvec3{f.brush->mins} - pad, hi = glm::dvec3{f.brush->maxs} + pad;
        constexpr double slack = 0.5;
        bool cut = false;
        for(int axis = 0; axis < 3; ++axis)
        {
            for(int s = 0; s < 2; ++s)
            {
                if(s ? f.hi[axis] <= hi[axis] + slack : f.lo[axis] >= lo[axis] - slack)
                {
                    continue;
                }
                glm::dvec3 n0{0.0};
                n0[axis] = s ? 1.0 : -1.0;
                const int tag = plane(n0, s ? hi[axis] : -lo[axis]);
                glm::dvec3 n;
                double d;
                oriented(tag, n0, n, d);
                Poly front, back;
                splitPoly(std::move(f.poly), n, d, front, back);
                if(back.empty())
                {
                    return false;
                }
                if(!front.empty())
                {
                    back.back().tag = tag;
                }
                f.poly = std::move(back);
                cut = true;
            }
        }
        rebounded += cut;
        return !cut || finish(f, -1);
    }

    // The table's plane i, facing the way n does.
    void oriented(int i, const glm::dvec3& n, glm::dvec3& on, double& od) const
    {
        const mplane_t& p = planeAt(i);
        on = glm::dvec3{p.normal[0], p.normal[1], p.normal[2]};
        od = p.dist;
        if(glm::dot(on, n) < 0.0)
        {
            on = -on;
            od = -od;
        }
    }

    // Its faces on the plane just split on (n, d; it is on the side `sign` points to) are done with; its bounds and
    // live faces. False: a sliver, nothing of it further than the epsilon from that plane (a face lying on the plane
    // goes to both sides of the split: the side the piece is not on gets the face alone, flat).
    static bool finish(Frag& f, int split, const glm::dvec3& n = glm::dvec3{0.0}, double d = 0.0, double sign = 0.0)
    {
        f.lo = glm::dvec3{1e300};
        f.hi = glm::dvec3{-1e300};
        f.live = 0;
        double reach = 0.0;
        for(Face& face : f.poly)
        {
            if(face.tag == split)
            {
                face.tag = -1;
            }
            for(const glm::dvec3& v : face.w)
            {
                f.lo = glm::min(f.lo, v);
                f.hi = glm::max(f.hi, v);
                reach = std::max(reach, (glm::dot(n, v) - d) * sign);
            }
            f.live += face.tag >= 0; // (a face lost to the epsilon too: its plane still bounds the piece)
        }
        return f.lo.x <= f.hi.x && f.hi.x - f.lo.x >= 0.01 && f.hi.y - f.lo.y >= 0.01 && f.hi.z - f.lo.z >= 0.01 &&
               (sign == 0.0 || reach >= 0.01);
    }

    // qbsp's choice (qbsp3's SelectSplitSide, on the pieces' bounds): the plane that most pieces lie on and that splits
    // the fewest, balanced, axial first. Many pieces: a sample of the planes (the build's time).
    int choose(const std::vector<Frag>& frags)
    {
        seen_.resize(count(), 0);
        facing_.resize(count(), 0);
        ++stamp_;
        cands_.clear();
        for(const Frag& f : frags)
        {
            for(const Face& face : f.poly)
            {
                if(face.tag < 0)
                {
                    continue;
                }
                const auto t = static_cast<std::size_t>(face.tag);
                if(seen_[t] != stamp_)
                {
                    seen_[t] = stamp_;
                    facing_[t] = 0;
                    cands_.push_back(face.tag);
                }
                ++facing_[t];
            }
        }
        std::stable_partition(cands_.begin(), cands_.end(),
            [this](int c) { return planeAt(c).type < 3; });
        const std::size_t step = std::max<std::size_t>(1, cands_.size() * frags.size() / chooseBudget);
        int best = cands_.front();
        long long bestValue = std::numeric_limits<long long>::min();
        for(std::size_t ci = 0; ci < cands_.size(); ci += step)
        {
            const int c = cands_[ci];
            const mplane_t& p = planeAt(c);
            const glm::dvec3 n{p.normal[0], p.normal[1], p.normal[2]};
            int front = 0, back = 0, splits = 0;
            for(const Frag& f : frags)
            {
                double lo, hi;
                if(p.type < 3)
                {
                    lo = f.lo[p.type] - p.dist;
                    hi = f.hi[p.type] - p.dist;
                }
                else
                {
                    const glm::dvec3 centre = (f.lo + f.hi) * 0.5, half = (f.hi - f.lo) * 0.5;
                    const double s = glm::dot(n, centre) - p.dist;
                    const double r = std::abs(n.x) * half.x + std::abs(n.y) * half.y + std::abs(n.z) * half.z;
                    lo = s - r;
                    hi = s + r;
                }
                if(hi <= onEpsilon)
                {
                    ++back;
                }
                else if(lo >= -onEpsilon)
                {
                    ++front;
                }
                else
                {
                    ++splits;
                }
            }
            const long long value = 5ll * facing_[static_cast<std::size_t>(c)] - 5ll * splits - std::abs(front - back) +
                                    (p.type < 3 ? 5 : 0);
            if(value > bestValue)
            {
                bestValue = value;
                best = c;
            }
        }
        return best;
    }

    static constexpr std::size_t chooseBudget = 40000; // planes x pieces weighed at a node, at most

    const TreeBuilder* base_ = nullptr; // (a builder on the pool) the table it adds to
    std::size_t baseCount_ = 0;         // base's planes
    std::vector<mplane_t>* planes_;     // its planes (after base's)
    std::vector<mclipnode_t>* nodes_;
    int* solid_;
    int* empty_;
    glm::vec3 ext_;                     // the box's half size
    std::vector<PlaneAsk>* log_ = nullptr;
    std::unordered_map<long long, std::vector<int>> index_; // its planes by key(dist), each key's in the order added
    std::vector<std::size_t> seen_;
    std::vector<int> facing_;
    std::size_t stamp_ = 0;
    std::vector<int> cands_;
};

// A tree's build shared out on the game's thread pool, the same tree as the build on one thread (buildTree's reference,
// vr_jobs_parallel 0), node for node and plane for plane:
// - The brushes are grown on one thread (they add most of the table's planes, and nearly the same planes as each
//   other: grown apart, nearly every brush would be done again). The tree's top is split `shareDepth` levels down,
//   each node's split and, below the top, each subtree a unit, the two sides of a node at once.
// - A unit asks the table for its planes as it would on one thread, but it only sees the table as it was when it began
//   (its base's), not the planes the units before it (in the order the build on one thread goes) add meanwhile. So it
//   adds its planes to a table of its own, and logs what it asked and got.
// - The merge then goes through the units in that order, asking the tree's table the same: if every answer is the
//   plane the unit had (the same values; a plane it added: a plane new to it, one to one), it computed what the build
//   on one thread computes, and its pieces or nodes are taken, its planes renumbered. Else (rarely: two units adding
//   nearly the same plane) its planes are taken back out of the table and it is done again there, on the merging
//   thread, as the build on one thread does it.
constexpr int shareDepth = 10;         // the top's levels: up to 1024 subtrees
constexpr std::size_t shareMin = 48;   // fewer pieces: a subtree of their own (not split further here)

struct Unit
{
    const Unit* parent = nullptr;    // the unit whose split made its pieces (none: the tree's own table is its base)
    std::size_t baseCount = 0;       // the planes it saw (its base's)
    std::vector<mplane_t> planes;    // those it added (numbered from baseCount)
    std::vector<PlaneAsk> log;
    std::vector<int> map;            // its planes' numbers in the tree's table (the merge)
    int solid = 0, empty = 0, rebounded = 0;
    // A node's: a leaf (kind 0: root its contents), a split (kind 1: split, kids), a subtree (kind 2: root and nodes,
    // numbered from 0); input: its pieces (done again from them if the merge's check fails).
    int kind = 0;
    int root = 0;
    int split = 0;
    std::unique_ptr<Unit> kids[2];
    std::vector<mclipnode_t> nodes;
    std::vector<Frag> input;
};

// The merge's state: which unit's plane each plane added to the tree's table since `start` is.
struct Merge
{
    std::size_t start = 0;
    std::vector<const Unit*> owner; // [plane - start]
    int redone = 0;                 // units done again on the merging thread

    // A plane number of u's (or of the units above it) in the tree's table.
    [[nodiscard]] int real(const Unit& u, int id) const
    {
        for(const Unit* x = &u; x; x = x->parent)
        {
            if(static_cast<std::size_t>(id) >= x->baseCount)
            {
                return x->map[static_cast<std::size_t>(id) - x->baseCount];
            }
        }
        return id;
    }

    [[nodiscard]] bool mine(int id, const Unit& u) const
    {
        const std::size_t i = static_cast<std::size_t>(id) - start;
        const Unit* o = i < owner.size() ? owner[i] : nullptr;
        for(const Unit* x = &u; x && o; x = x->parent)
        {
            if(o == x)
            {
                return true;
            }
        }
        return false;
    }

    void own(std::size_t from, std::size_t to, const Unit& u)
    {
        owner.resize(std::max(owner.size(), to - start), nullptr);
        for(std::size_t i = from; i < to; ++i)
        {
            owner[i - start] = &u;
        }
    }

    static bool same(const mplane_t& a, const mplane_t& b)
    {
        return std::memcmp(a.normal, b.normal, sizeof(a.normal)) == 0 && std::memcmp(&a.dist, &b.dist, sizeof(a.dist)) == 0 &&
               a.type == b.type && a.signbits == b.signbits;
    }

    // u's asks put to the tree's table (tb) in turn: whether every answer is the plane u had.
    bool adopt(TreeBuilder& tb, Unit& u)
    {
        u.map.assign(u.planes.size(), -1);
        for(const PlaneAsk& a : u.log)
        {
            const int got = tb.plane(a.n, a.d);
            const auto id = static_cast<std::size_t>(a.id);
            if(id < u.baseCount)
            {
                if(got != (u.parent ? real(*u.parent, a.id) : a.id))
                {
                    return false;
                }
                continue;
            }
            int& to = u.map[id - u.baseCount];
            if(to >= 0)
            {
                if(to != got)
                {
                    return false;
                }
                continue;
            }
            // One it added: the table's must be new to it (not one it or the units above it could see or had) and the same.
            if(static_cast<std::size_t>(got) < start || mine(got, u) || !same(tb.planeAt(got), u.planes[id - u.baseCount]))
            {
                return false;
            }
            to = got;
            own(static_cast<std::size_t>(got), static_cast<std::size_t>(got) + 1, u);
        }
        return true;
    }

    // What u's asks added taken out again, before it is done on the merging thread.
    void undo(TreeBuilder& tb, std::size_t to)
    {
        tb.rollback(to);
        owner.resize(std::min(owner.size(), to - start));
    }
};

// A unit of the tree's top (its pieces), over base; its sides' units at once below it.
void speculate(Unit& u, const TreeBuilder& base, std::vector<Frag>&& frags, int depth)
{
    u.baseCount = base.count();
    TreeBuilder tb{base, u.planes, u.nodes, u.solid, u.empty, u.log};
    if(depth == 0 || frags.size() < shareMin)
    {
        u.kind = 2;
        u.input = frags;
        u.root = tb.build(frags);
    }
    else if(const int contents = tb.leaf(frags))
    {
        u.kind = 0; // (asks for no plane: never done again)
        u.root = contents;
    }
    else
    {
        u.kind = 1;
        u.input = frags;
        std::vector<Frag> sides[2];
        glm::dvec3 n;
        double d;
        u.split = tb.split(frags, sides, n, d);
        for(int side = 0; side < 2; ++side)
        {
            u.kids[side] = std::make_unique<Unit>();
            u.kids[side]->parent = &u;
        }
        jobs::parallelFor(2, 1,
            [&](std::size_t begin, std::size_t end)
            {
                for(std::size_t i = begin; i < end; ++i)
                {
                    speculate(*u.kids[i], tb, std::move(sides[i]), depth - 1);
                }
            });
    }
    u.rebounded = tb.rebounded;
}

// u's nodes into the tree (in the order the build on one thread makes them); its root.
int emit(TreeBuilder& tb, Unit& u, Merge& m)
{
    const std::size_t saved = tb.count();
    if(!m.adopt(tb, u))
    {
        m.undo(tb, saved);
        ++m.redone;
        const int root = tb.build(u.input);
        m.own(saved, tb.count(), u);
        return root;
    }
    tb.addLeaves(u.solid, u.empty);
    tb.rebounded += u.rebounded;
    std::vector<mclipnode_t>& nodes = tb.nodes();
    if(u.kind == 0)
    {
        return u.root;
    }
    if(u.kind == 2)
    {
        const int offset = static_cast<int>(nodes.size());
        for(mclipnode_t n : u.nodes)
        {
            n.planenum = m.real(u, n.planenum);
            for(int& c : n.children)
            {
                c = c >= 0 ? c + offset : c;
            }
            nodes.push_back(n);
        }
        return u.root >= 0 ? u.root + offset : u.root;
    }
    const int node = static_cast<int>(nodes.size());
    nodes.push_back(mclipnode_t{m.real(u, u.split), {0, 0}});
    const int front = emit(tb, *u.kids[0], m);
    const int back = emit(tb, *u.kids[1], m);
    nodes[static_cast<std::size_t>(node)].children[0] = front;
    nodes[static_cast<std::size_t>(node)].children[1] = back;
    return node;
}

// The tree of one model (sub), built into t (its root: a node; a lone leaf gets a node of its own). Only t is written:
// trees for different boxes are built at once on the pool (report false there: the pieces cut back are returned, for
// the main thread to print). On the pool (vr_jobs_parallel) unless watching (vr_hull_leafdebug).
int buildTree(Tree& t, const Brushes& b, std::size_t sub, const glm::dvec3* watch = nullptr, bool report = true)
{
    const auto t0 = std::chrono::steady_clock::now();
    TreeBuilder tb{t};
    tb.watch = watch;
    tb.debugBrushes = &b;
    const glm::dvec3 ext{t.ext};
    std::vector<const Brush*> list;
    const SubModel& sm = b.subs[sub];
    for(std::uint32_t i = 0; i < sm.numBrushes; ++i)
    {
        list.push_back(&b.brushes[sm.firstBrush + i]);
    }
    if(sub == 0)
    {
        for(const int c : b.clips)
        {
            list.push_back(&b.brushes[static_cast<std::size_t>(c)]);
        }
    }
    std::vector<Frag> frags;
    auto growAll = [&]
    {
        for(const Brush* br : list)
        {
            Frag f;
            if(tb.grow(b, *br, ext, f))
            {
                frags.push_back(std::move(f));
            }
        }
    };
    int root = 0;
    if(!watch && jobs::parallel() && jobs::pool())
    {
        Merge m;
        growAll();
        Unit top;
        speculate(top, tb, std::move(frags), shareDepth);
        m.start = tb.count();
        m.owner.clear();
        root = emit(tb, top, m);
        t.redone += m.redone;
    }
    else
    {
        growAll();
        Poly region = watch ? boxPoly(glm::dvec3{-bogus * 0.5}, glm::dvec3{bogus * 0.5}) : Poly{};
        root = tb.build(frags, watch ? &region : nullptr);
    }
    if(root < 0)
    {
        const int node = static_cast<int>(t.nodes.size());
        t.nodes.push_back(mclipnode_t{tb.plane(glm::dvec3{0.0, 0.0, 1.0}, 0.0), {root, root}});
        root = node;
    }
    t.heads[sub] = root;
    if(tb.rebounded && report)
    {
        Con_DPrintf("hull: %d pieces cut back to their brushes' bounds (%gx%g, model %d)\n", tb.rebounded, t.ext.x * 2.f,
            t.ext.z * 2.f, static_cast<int>(sub));
    }
    t.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return tb.rebounded;
}

// Quake's trace of a point (the box's centre) through a model's tree.
trace_t treeTrace(const Tree& t, int head, const glm::vec3& start, const glm::vec3& end)
{
    trace_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.f;
    tr.allsolid = true;
    VectorCopy(end, tr.endpos);
    hull_t h{};
    h.clipnodes = const_cast<mclipnode_t*>(t.nodes.data());
    h.planes = const_cast<mplane_t*>(t.planes.data());
    h.firstclipnode = head;
    h.lastclipnode = static_cast<int>(t.nodes.size()) - 1;
    vec3_t a{start.x, start.y, start.z}, e{end.x, end.y, end.z};
    SV_RecursiveHullCheck(&h, head, 0.f, 1.f, a, e, &tr);
    return tr;
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

// A player's own box: a client's, 32 wide (not a point, nor a box QC traces with).
bool isPlayerBox(const edict_t* ent, const float* mins, const float* maxs)
{
    const int num = clientNum(ent);
    return num >= 1 && num <= svs.maxclients && maxs[0] - mins[0] == 32.f && maxs[1] - mins[1] == 32.f;
}

// The player's width against entities' boxes: 0 Quake's (off).
float entWidthSetting()
{
    const float v = vr_hull_ent_width.value;
    if(v < 0.f)
    {
        return vr_hull_width.value > 0.f ? widthSetting() : 0.f;
    }
    return v > 0.f ? std::clamp(v, minWidth, maxWidth) : 0.f;
}

// The player's width that shots, missiles and splash traces hit (vr_hull_hit_width): 0 Quake's (off).
float hitWidthSetting()
{
    const float v = vr_hull_hit_width.value;
    return v > 0.f && v < maxWidth ? std::clamp(v, minWidth, maxWidth) : 0.f;
}

// A box narrowed to the width about its centre, its height kept.
void narrowBox(const float* mins, const float* maxs, float width, float* boxMins, float* boxMaxs)
{
    for(int i = 0; i < 2; ++i)
    {
        const float c = (mins[i] + maxs[i]) * 0.5f;
        boxMins[i] = c - width * 0.5f;
        boxMaxs[i] = c + width * 0.5f;
    }
    boxMins[2] = mins[2];
    boxMaxs[2] = maxs[2];
}

// Whether the narrow entity width holds between a player and this entity (its category's setting): players, monsters,
// other boxes (explosive boxes, solid items; anything moving without an entity).
bool categoryOn(const edict_t* other)
{
    const int num = clientNum(other);
    if(num >= 1 && num <= svs.maxclients)
    {
        return vr_hull_players.value != 0.f;
    }
    if(other && (static_cast<int>(other->v.flags) & FL_MONSTER))
    {
        return vr_hull_monsters.value != 0.f;
    }
    return vr_hull_boxes.value != 0.f;
}

// ---------------------------------------------------------------------------------------------------------------
// Monsters' widths (vr_mhull; HULLS.md, "Monsters"): Quake moves a monster against the map with hull 1 (32 wide) or
// hull 2 (64), whatever its own box (Quake VR's QC gives most of them narrower boxes: a grunt's is 24, an ogre's 40).
// With vr_mhull a class's monsters move against the map with a compiled hull of its width instead (by default their
// own box's; Quake's hull height kept: 56 for hull 1's, 88 for hull 2's). A width narrower than their box also meets
// other bodies (vr_mhull_ents) and tests ledges with its corners (vr_mhull_ledges).

struct MonsterClass
{
    const char* classname;
    const char* name;
    cvar_t* width;
};
const MonsterClass monsterClasses[] = {
    {"monster_army", "Grunt", &vr_mhull_army},
    {"monster_dog", "Rottweiler", &vr_mhull_dog},
    {"monster_ogre", "Ogre", &vr_mhull_ogre},
    {"monster_knight", "Knight", &vr_mhull_knight},
    {"monster_hell_knight", "Death Knight", &vr_mhull_hknight},
    {"monster_zombie", "Zombie", &vr_mhull_zombie},
    {"monster_wizard", "Scrag", &vr_mhull_wizard},
    {"monster_demon1", "Fiend", &vr_mhull_demon},
    {"monster_shambler", "Shambler", &vr_mhull_shambler},
    {"monster_shalrath", "Vore", &vr_mhull_shalrath},
    {"monster_enforcer", "Enforcer", &vr_mhull_enforcer},
    {"monster_fish", "Rotfish", &vr_mhull_fish},
    {"monster_tarbaby", "Spawn", &vr_mhull_tarbaby},
};
constexpr int monsterClassCount = static_cast<int>(sizeof(monsterClasses) / sizeof(monsterClasses[0]));
constexpr float minMonsterWidth = 16.f;

// Each edict's class (an index in monsterClasses, -1 none), for the classname it had when looked up.
struct ClassCache
{
    std::vector<int> names;
    std::vector<signed char> classes;
    auto members() { return std::tie(names, classes); }
};
mem::Cache<ClassCache> classCache{"hull monster classes", mem::MapChange};

int monsterClassOf(const edict_t* ent)
{
    const int num = clientNum(ent);
    if(num <= 0)
    {
        return -1;
    }
    ClassCache& c = classCache;
    const auto i = static_cast<std::size_t>(num);
    if(c.names.size() <= i)
    {
        c.names.resize(i + 1, -1);
        c.classes.resize(i + 1, -1);
    }
    const int name = static_cast<int>(ent->v.classname);
    if(c.names[i] != name)
    {
        c.names[i] = name;
        c.classes[i] = -1;
        const char* s = name ? PR_GetString(name) : "";
        for(int k = 0; k < monsterClassCount; ++k)
        {
            if(!strcmp(s, monsterClasses[k].classname))
            {
                c.classes[i] = static_cast<signed char>(k);
                break;
            }
        }
    }
    return c.classes[i];
}

float hull2Height(const qmodel_t* world)
{
    const float h = world ? world->hulls[2].clip_maxs[2] - world->hulls[2].clip_mins[2] : 0.f;
    return h > 0.f ? h : 88.f;
}

// Quake's hull for a box of this width (SV_HullForEntity: hull 1 up to 32, else hull 2): its width and height.
void quakeHull(float size, float& width, float& height)
{
    const bool one = size <= 32.f;
    const hull_t& h = sv.worldmodel->hulls[one ? 1 : 2];
    width = h.clip_maxs[0] - h.clip_mins[0];
    width = width > 0.f ? width : (one ? 32.f : 64.f);
    height = one ? hull1Height(sv.worldmodel) : hull2Height(sv.worldmodel);
}

// A class's width for its monsters' box (size) and Quake's hull for it: 0 when they keep Quake's hull.
float classWidth(int cls, float size, float quake)
{
    const float v = monsterClasses[cls].width->value;
    const float w = v < 0.f ? size : v; // -1: its own box's
    if(w <= 0.f)
    {
        return 0.f;
    }
    const float c = std::max(w, minMonsterWidth);
    return c < quake ? c : 0.f;
}

// A live monster of a known class moving with its narrower width (vr_mhull): its width, and the height its world
// clipping keeps (Quake's hull's).
// (loading: also while the server spawns the map, for the trees compiled ahead: see spawned)
bool monsterWidth(const edict_t* ent, float& width, float& height, bool loading = false)
{
    if(vr_mhull.value == 0.f || !ent || !(sv.active || loading) || !sv.worldmodel ||
        static_cast<int>(ent->v.solid) != SOLID_SLIDEBOX)
    {
        return false;
    }
    const float size = ent->v.size[0];
    if(size <= 3.f || ent->v.size[1] != size)
    {
        return false;
    }
    const int cls = monsterClassOf(ent);
    if(cls < 0)
    {
        return false;
    }
    float quake = 0.f;
    quakeHull(size, quake, height);
    width = classWidth(cls, size, quake);
    return width > 0.f;
}

// ... and narrower than its own box (a width set below it): what bodies and the ledge test meet.
bool monsterNarrower(const edict_t* ent, float& width)
{
    float height = 0.f;
    return monsterWidth(ent, width, height) && width < ent->v.size[0];
}

// The box is the entity's own (not a box QC traces with).
bool ownBox(const edict_t* ent, const float* mins, const float* maxs)
{
    for(int i = 0; i < 3; ++i)
    {
        if(mins[i] != ent->v.mins[i] || maxs[i] != ent->v.maxs[i])
        {
            return false;
        }
    }
    return true;
}

// The model at a model index as one of b's: the world's submodels, or an external .bsp model's brushes built now (the
// first time it is met). -1: not a brush model.
int subOf(Brushes& b, int index)
{
    if(index < 0 || index >= static_cast<int>(b.modelSub.size()))
    {
        return -1;
    }
    int& known = b.modelSub[static_cast<std::size_t>(index)];
    if(known != -2)
    {
        return known;
    }
    known = -1;
    qmodel_t* m = sv.models[index];
    if(!m || m->type != mod_brush || !m->hulls[0].clipnodes || m->numnodes <= 0)
    {
        return known;
    }
    const hull_t& h0 = m->hulls[0];
    for(std::size_t i = 0; i < b.subs.size(); ++i)
    {
        if(b.subs[i].clipnodes == h0.clipnodes && b.subs[i].head == h0.firstclipnode)
        {
            return known = static_cast<int>(i);
        }
    }
    if(h0.clipnodes == b.clipnodes)
    {
        return known; // the world's, but none of its submodels (not expected)
    }
    const auto base = static_cast<std::uint32_t>(b.leafBrush.size());
    b.leafBrush.resize(b.leafBrush.size() + static_cast<std::size_t>(m->numnodes) * 2, -1);
    addSubModel(b, h0, m->numnodes, base, h0.firstclipnode, m->mins, m->maxs);
    Con_DPrintf("hull: %s: %u brushes\n", m->name, b.subs.back().numBrushes);
    return known = static_cast<int>(b.subs.size()) - 1;
}

// Method A's hull of a model (sub) for the box of half size ext: compiled now if not yet (the world's with the map or
// the setting; a new width compiles all again).
// The tree for a box size: the player's (the player's size, or what it holds already), else a monster's (vr_mhull).
Tree& slotFor(const mclipnode_t* clipnodes, const glm::vec3& ext)
{
    Tree& p = tree;
    if(p.forClipnodes == clipnodes && p.ext == ext)
    {
        return p;
    }
    const float half = widthSetting() * 0.5f;
    if(vr_hull_width.value > 0.f && sv.worldmodel && ext == glm::vec3{half, half, hull1Height(sv.worldmodel) * 0.5f})
    {
        tree.release();
        p.forClipnodes = clipnodes;
        p.ext = ext;
        return p;
    }
    std::vector<Tree>& v = monsterTrees.trees;
    for(Tree& t : v)
    {
        if(t.forClipnodes == clipnodes && t.ext == ext)
        {
            return t;
        }
    }
    if(v.size() >= maxMonsterTrees)
    {
        v.clear();
    }
    v.emplace_back();
    v.back().forClipnodes = clipnodes;
    v.back().ext = ext;
    return v.back();
}

// The tree already made (or being made) for this box, if any (no slot taken).
Tree* findSlot(const mclipnode_t* clipnodes, const glm::vec3& ext)
{
    if(tree.forClipnodes == clipnodes && tree.ext == ext)
    {
        return &static_cast<Tree&>(tree);
    }
    for(Tree& t : monsterTrees.trees)
    {
        if(t.forClipnodes == clipnodes && t.ext == ext)
        {
            return &t;
        }
    }
    return nullptr;
}

// The map load's builds on the pool (beforeLoad, spawned): the map as brushes, then the player's and the monsters'
// trees, made while the server spawns the map; settle() waits for them (every use of the brushes or the trees first).
// While they run, the main thread touches neither `built` nor the trees being made.
struct Pending
{
    jobs::Future<void> brushes;         // build(): the map as brushes
    std::vector<Tree*> trees;           // being compiled, each by its job below (after the brushes)
    std::vector<jobs::Future<int>> run; // (their pieces cut back)
    glm::vec3 playerExt{0.f};           // the player's tree's box, if it is one of them
    double posted = 0.0;                // when the builds were handed out (the load's report)
};
Pending pending;

// The trees for these boxes whose world model (sub 0) is not compiled yet: their slots taken (their order kept, each
// once; the player's size is the player's tree); none being compiled already (pending).
std::vector<Tree*> claimTrees(const mclipnode_t* clipnodes, const std::vector<glm::vec3>& exts)
{
    for(const glm::vec3& ext : exts)
    {
        if(!pending.trees.empty() && !findSlot(clipnodes, ext) && monsterTrees.trees.size() >= maxMonsterTrees)
        {
            continue; // (a new slot would let go of the trees being made: this one is made when first used)
        }
        (void)slotFor(clipnodes, ext);
    }
    std::vector<Tree*> out; // (found after every slot is taken: a new one may move the others)
    for(const glm::vec3& ext : exts)
    {
        Tree* t = findSlot(clipnodes, ext);
        if(t && std::find(pending.trees.begin(), pending.trees.end(), t) == pending.trees.end() &&
            (t->heads.empty() || t->heads[0] < 0) && std::find(out.begin(), out.end(), t) == out.end())
        {
            out.push_back(t);
        }
    }
    return out;
}

// The world model's tree in t (after the map's brushes are built); the pieces cut back.
int compileWorldTree(Tree& t, const Brushes& b)
{
    if(t.heads.size() < b.subs.size())
    {
        t.heads.resize(b.subs.size(), -1);
    }
    return t.heads[0] < 0 ? buildTree(t, b, 0, nullptr, false) : 0;
}

void reportTree(const Tree& t, int rebounded)
{
    if(rebounded)
    {
        Con_DPrintf("hull: %d pieces cut back to their brushes' bounds (%gx%g, model 0)\n", rebounded, t.ext.x * 2.f,
            t.ext.z * 2.f);
    }
    Con_DPrintf("hull: %s compiled for %gx%g: %d nodes, %d planes in %.1f ms (%d pieces done again)\n", sv.worldmodel ? sv.worldmodel->name : "?",
        t.ext.x * 2.f, t.ext.z * 2.f, static_cast<int>(t.nodes.size()), static_cast<int>(t.planes.size()), t.ms, t.redone);
}

// These trees compiled at once on the pool (the main thread one of them), reported in their order.
void compileTrees(const std::vector<Tree*>& todo, const Brushes& b)
{
    std::vector<int> rebounded(todo.size(), 0);
    jobs::parallelFor(todo.size(), 1,
        [&](std::size_t begin, std::size_t end)
        {
            for(std::size_t i = begin; i < end; ++i)
            {
                rebounded[i] = compileWorldTree(*todo[i], b);
            }
        });
    for(std::size_t i = 0; i < todo.size(); ++i)
    {
        reportTree(*todo[i], rebounded[i]);
    }
}

// A tree compiled on the pool once the brushes are built.
void postTree(Tree* t)
{
    pending.trees.push_back(t);
    pending.run.push_back(jobs::async(
        [t]
        {
            pending.brushes.wait(); // (only waited on until settle: nothing else touches it meanwhile)
            return compileWorldTree(*t, built);
        }));
}

void settle()
{
    if(!pending.brushes.valid())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    std::vector<jobs::Future<int>> run = std::move(pending.run);
    std::vector<Tree*> trees = std::move(pending.trees);
    pending.run.clear();
    pending.trees.clear();
    std::vector<int> rebounded;
    for(jobs::Future<int>& f : run)
    {
        rebounded.push_back(f.get());
    }
    pending.brushes.get();
    const double t1 = Sys_DoubleTime();
    VR_TimeAdd("hull: the map's brushes and compiled hulls, waited for (built on the pool)", t1 - t0);
    Con_DPrintf("hull: %s rebuilt as %d brushes in %.1f ms (on the pool; waited %.1f ms, %.1f ms after they began)\n",
        sv.worldmodel ? sv.worldmodel->name : "?", static_cast<int>(built.brushes.size()), built.ms, (t1 - t0) * 1000.0,
        (t1 - pending.posted) * 1000.0);
    for(std::size_t i = 0; i < trees.size(); ++i)
    {
        reportTree(*trees[i], rebounded[i]);
    }
}

const Tree& treeFor(const Brushes& b, std::size_t sub, const glm::vec3& ext)
{
    settle();
    Tree& t = slotFor(b.clipnodes, ext);
    if(t.heads.size() < b.subs.size())
    {
        t.heads.resize(b.subs.size(), -1);
    }
    if(t.heads[sub] < 0)
    {
        buildTree(t, b, sub);
        if(sub == 0)
        {
            Con_DPrintf("hull: %s compiled for %gx%g: %d nodes, %d planes in %.1f ms (%d pieces done again)\n", sv.worldmodel->name, ext.x * 2.f,
                ext.z * 2.f, static_cast<int>(t.nodes.size()), static_cast<int>(t.planes.size()), t.ms, t.redone);
        }
    }
    return t;
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

// FNV-1a over the build's results (vr_hull_stats: the same on the pool and on one thread).
struct Hash
{
    std::uint32_t h = 2166136261u;
    template <class T>
    void add(const T& v)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&v);
        for(std::size_t i = 0; i < sizeof(T); ++i)
        {
            h = (h ^ p[i]) * 16777619u;
        }
    }
};

std::uint32_t hashOf(const Brushes& b)
{
    Hash h;
    for(const Plane& q : b.planes)
    {
        h.add(q.normal);
        h.add(q.dist);
        h.add(q.grows);
    }
    for(const Brush& br : b.brushes)
    {
        h.add(br.first);
        h.add(br.count);
        h.add(br.mins);
        h.add(br.maxs);
        h.add(br.clip);
    }
    for(const int v : b.leafBrush)
    {
        h.add(v);
    }
    for(const int v : b.clips)
    {
        h.add(v);
    }
    for(const int v : b.leafClipStart)
    {
        h.add(v);
    }
    for(const int v : b.leafClipList)
    {
        h.add(v);
    }
    for(const int v : b.hull1Clip)
    {
        h.add(v);
    }
    for(const SubModel& sm : b.subs)
    {
        h.add(sm.head);
        h.add(sm.firstBrush);
        h.add(sm.numBrushes);
    }
    h.add(b.bevels);
    h.add(b.dropped);
    h.add(b.hull1Leaves);
    return h.h;
}

// The world's tree (its models' trees, compiled when first met, follow in the same arrays): its nodes from the root,
// with their numbers and planes.
std::uint32_t hashOf(const Tree& t)
{
    Hash h;
    const int root = t.heads.empty() ? -1 : t.heads[0];
    std::vector<int> stack{root};
    while(!stack.empty())
    {
        const int num = stack.back();
        stack.pop_back();
        h.add(num);
        if(num < 0)
        {
            continue;
        }
        const mclipnode_t& n = t.nodes[static_cast<std::size_t>(num)];
        const mplane_t& p = t.planes[static_cast<std::size_t>(n.planenum)];
        h.add(n.planenum);
        h.add(p.normal);
        h.add(p.dist);
        h.add(p.type);
        h.add(p.signbits);
        stack.push_back(n.children[1]);
        stack.push_back(n.children[0]);
    }
    return h.h;
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
    Con_Printf("hull: hash brushes %08x (stamp %u)\n", hashOf(*b), static_cast<unsigned>(b->stamp));
    int external = 0;
    for(std::size_t i = 0; i < b->subs.size(); ++i)
    {
        external += b->subs[i].clipnodes != b->clipnodes;
    }
    const Tree& t = tree;
    Con_Printf("hull: %d brush models (%d external .bsp); method %s; compiled hull: %s%d nodes, %d planes, %d solid "
               "and %d empty leaves, %.0f KB, built in %.1f ms\n",
        static_cast<int>(b->subs.size()), external, vr_hull_method.value != 0.f ? "compiled hull" : "brush sweep",
        t.forClipnodes == b->clipnodes ? "" : "(none yet) ", static_cast<int>(t.nodes.size()),
        static_cast<int>(t.planes.size()), t.solidLeaves, t.emptyLeaves, tree.bytes() / 1024.0, t.ms);
    if(t.forClipnodes == b->clipnodes)
    {
        Con_Printf("hull: hash tree %gx%g %08x\n", t.ext.x * 2.f, t.ext.z * 2.f, hashOf(t));
    }
    // The monsters' (vr_mhull).
    double ms = 0.0;
    int count = 0;
    for(const Tree& m : static_cast<MonsterTrees&>(monsterTrees).trees)
    {
        if(m.forClipnodes != b->clipnodes)
        {
            continue;
        }
        ++count;
        ms += m.ms;
        Con_Printf("hull: monsters' tree %gx%g: %d nodes, %d planes, %.0f KB, built in %.1f ms\n", m.ext.x * 2.f,
            m.ext.z * 2.f, static_cast<int>(m.nodes.size()), static_cast<int>(m.planes.size()), heldBytes(m) / 1024.0, m.ms);
        Con_Printf("hull: hash tree %gx%g %08x\n", m.ext.x * 2.f, m.ext.z * 2.f, hashOf(m));
    }
    Con_Printf("hull: monsters (vr_mhull %s): %d trees, %.0f KB, built in %.1f ms\n", vr_mhull.value != 0.f ? "on" : "off",
        count, monsterTrees.bytes() / 1024.0, ms);
}

// vr_mhull_reset: every class's width back to its own box's (the settings' defaults).
void reset_f()
{
    for(const MonsterClass& c : monsterClasses)
    {
        Cvar_SetQuick(c.width, c.width->default_string);
    }
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

    // Method A: the hulls compiled for the 32 box (against hull 1: how close qbsp's own is) and for the width (against
    // the sweep: both methods should agree), each built fresh and timed.
    auto compile = [&](Tree& t, const glm::vec3& mins, const glm::vec3& maxs)
    {
        t.ext = (maxs - mins) * 0.5f;
        t.forClipnodes = b->clipnodes;
        t.heads.assign(b->subs.size(), -1);
        buildTree(t, *b, 0);
    };
    auto treeBytes = [](const Tree& t)
    { return t.nodes.capacity() * sizeof(mclipnode_t) + t.planes.capacity() * sizeof(mplane_t); };
    Tree a32, aw;
    compile(a32, m32, M32);
    compile(aw, mw, Mw);
    std::vector<trace_t> tree32(n), treew(n);
    Bench ba32, baw;
    for(int pass = 0; pass < 2; ++pass)
    {
        for(int k = 0; k < 2; ++k)
        {
            const Tree& t = k ? aw : a32;
            const glm::vec3 centre = k ? (mw + Mw) * 0.5f : (m32 + M32) * 0.5f;
            std::vector<trace_t>& out = k ? treew : tree32;
            const auto t0 = Clock::now();
            for(int i = 0; i < n; ++i)
            {
                out[i] = treeTrace(t, t.heads[0], starts[i] + centre, ends[i] + centre);
            }
            (k ? baw : ba32).ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / n;
        }
    }
    // Agreement within a unit along the move; startsolid differing.
    auto compare = [&](const std::vector<trace_t>& x, const std::vector<trace_t>& y, int& same, int& xSooner, int& ySooner,
                       int& solidDiff, const char* what)
    {
        same = xSooner = ySooner = solidDiff = 0;
        int shown = 0;
        for(int i = 0; i < n; ++i)
        {
            const float len = glm::distance(starts[i], ends[i]);
            const float fx = x[i].allsolid ? 0.f : x[i].fraction, fy = y[i].allsolid ? 0.f : y[i].fraction;
            solidDiff += (x[i].startsolid != 0) != (y[i].startsolid != 0);
            const float gap = (fy - fx) * len;
            if(std::abs(gap) <= 1.f)
            {
                ++same;
                continue;
            }
            (gap > 0.f ? xSooner : ySooner)++;
            if(std::abs(gap) >= 8.f && shown++ < 2)
            {
                Con_Printf("  %s: %.0f units apart from %.0f %.0f %.0f (%s sooner)\n", what, std::abs(gap), starts[i].x,
                    starts[i].y, starts[i].z, gap > 0.f ? "first" : "second");
            }
        }
    };
    int s1, x1, y1, d1, s2, x2, y2, d2;
    compare(tree32, stock, s1, x1, y1, d1, "hull32 vs hull1");
    compare(treew, boxw, s2, x2, y2, d2, "hullw vs brushw");
    Con_Printf("hullbench %s: method A: compiled 32 in %.1f ms (%d nodes, %d planes, %.0f KB; hull 1 has %d nodes), %g in "
               "%.1f ms (%d nodes, %.0f KB); hull32 %.0f ns, hull%g %.0f ns\n",
        world->name, a32.ms, static_cast<int>(a32.nodes.size()), static_cast<int>(a32.planes.size()),
        treeBytes(a32) / 1024.0, world->hulls[1].lastclipnode - world->hulls[1].firstclipnode + 1, width, aw.ms,
        static_cast<int>(aw.nodes.size()), treeBytes(aw) / 1024.0, ba32.ns, width, baw.ns);
    Con_Printf("hullbench %s: method A: hull32 vs hull1 %.2f%% agree (hull32 sooner %d, hull1 sooner %d, startsolid "
               "differs %d); hull%g vs brush%g %.2f%% agree (hull sooner %d, brush sooner %d, startsolid differs %d)\n",
        world->name, 100.0 * s1 / n, x1, y1, d1, width, width, 100.0 * s2 / n, x2, y2, d2);
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

// vr_mhull_walktest: the level's monsters walked (pathing with vr_mhull on or off). Mode 0 (random walk): their AI
// stopped, each is sent by movetogoal (16 units a tenth of a second, the id AI's step) to a floor spot 96-384 units away
// in a random direction, a new one when it gets there (within 32 units across, 48 up or down) or after 8 seconds.
// Mode 1 (patrol): their own AI left running (notarget the player first), the path_corners they reach counted. For
// each class: the units walked, stuck (2-second spells moving less than 16 units), drops (a walker 20 units lower than
// a tenth of a second before: off a ledge, or a lift), in solid (tenths of a second SV_TestEntityPosition finds its box
// in something), embedded (its origin in the world's solid) and the goals reached.

struct MonsterWalk
{
    struct Mon
    {
        int num = 0, cls = 0;
        glm::vec3 last{0.f}, windowStart{0.f}, goal{0.f};
        double windowUntil = 0.0, goalUntil = 0.0;
        int oldThink = 0, lastGoal = 0;
        bool patrols = false;
    };
    struct Counts
    {
        int monsters = 0, patrollers = 0, steps = 0, stuck = 0, drops = 0, inSolid = 0, embedded = 0, reached = 0,
            given = 0, corners = 0;
        double moved = 0.0;
    };
    std::vector<Mon> mons;
    Counts counts[monsterClassCount];
    double until = 0.0, nextStep = 0.0, started = 0.0;
    std::uint32_t seed = 1;
    int mode = 0;
    edict_t* goal = nullptr;
    std::string then; // a command run at the end (a test's next map)
};
MonsterWalk monsterWalk;

// A spot 96-384 units away in a random direction the monster can stand at (a walker: on a floor, not in liquid; a
// flyer or swimmer: in what it is in now); somewhere that way if none is found.
glm::vec3 monsterGoal(MonsterWalk& w, const edict_t* e)
{
    const glm::vec3 o{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    const bool flies = (static_cast<int>(e->v.flags) & (FL_FLY | FL_SWIM)) != 0;
    vec3_t ov{o.x, o.y, o.z};
    const int here = SV_PointContents(ov);
    glm::vec3 p = o;
    for(int attempt = 0; attempt < 24; ++attempt)
    {
        const float yaw = random01(w.seed) * 6.2831853f, d = 96.f + 288.f * random01(w.seed);
        p = o + glm::vec3{std::cos(yaw) * d, std::sin(yaw) * d, flies ? (random01(w.seed) - 0.5f) * 96.f : 24.f};
        vec3_t v{p.x, p.y, p.z};
        if(flies)
        {
            if(SV_PointContents(v) == here)
            {
                return p;
            }
            continue;
        }
        if(SV_PointContents(v) != CONTENTS_EMPTY)
        {
            continue;
        }
        vec3_t down{p.x, p.y, p.z - 160.f};
        const trace_t tr = SV_Move(v, vec3_origin, vec3_origin, down, MOVE_NOMONSTERS, nullptr);
        if(tr.startsolid || tr.fraction >= 1.f || tr.plane.normal[2] < 0.7f)
        {
            continue;
        }
        vec3_t above{tr.endpos[0], tr.endpos[1], tr.endpos[2] + 8.f};
        if(SV_PointContents(above) != CONTENTS_EMPTY)
        {
            continue;
        }
        return glm::vec3{tr.endpos[0], tr.endpos[1], tr.endpos[2] - e->v.mins[2]};
    }
    return p;
}

void monsterWalkEnd(MonsterWalk& w)
{
    const bool on = vr_mhull.value != 0.f;
    Con_Printf("mhullwalk %s: mode %s, vr_mhull %d (ents %d, ledges %d), %.0f s\n", sv.worldmodel->name,
        w.mode ? "patrol" : "random", on, vr_mhull_ents.value != 0.f, vr_mhull_ledges.value != 0.f, sv.qcvm.time - w.started);
    MonsterWalk::Counts total;
    for(int k = 0; k < monsterClassCount; ++k)
    {
        const MonsterWalk::Counts& c = w.counts[k];
        if(!c.monsters)
        {
            continue;
        }
        float width = 0.f, box = 0.f;
        for(const MonsterWalk::Mon& m : w.mons)
        {
            if(m.cls == k)
            {
                const edict_t* e = EDICT_NUM(m.num);
                float quake = 0.f, height = 0.f;
                box = e->v.size[0];
                quakeHull(box, quake, height);
                width = on ? classWidth(k, box, quake) : 0.f;
                width = width > 0.f ? width : quake;
                break;
            }
        }
        Con_Printf("mhullwalk %-12s n %2d box %2.0f walls %2.0f: walked %6.0f, stuck %3d, drops %3d, in solid %3d, embedded %2d, "
                   "goals %3d/%3d, corners %3d (%d patrol)\n",
            monsterClasses[k].name, c.monsters, box, width, c.moved, c.stuck, c.drops, c.inSolid, c.embedded, c.reached, c.given,
            c.corners, c.patrollers);
        total.monsters += c.monsters;
        total.moved += c.moved;
        total.stuck += c.stuck;
        total.drops += c.drops;
        total.inSolid += c.inSolid;
        total.embedded += c.embedded;
        total.reached += c.reached;
        total.given += c.given;
        total.corners += c.corners;
        total.patrollers += c.patrollers;
    }
    Con_Printf("mhullwalk total        n %2d: walked %6.0f, stuck %3d, drops %3d, in solid %3d, embedded %2d, goals %3d/%3d, "
               "corners %3d (%d patrol)\n",
        total.monsters, total.moved, total.stuck, total.drops, total.inSolid, total.embedded, total.reached, total.given,
        total.corners, total.patrollers);
    // Their AI given back (random walk).
    for(const MonsterWalk::Mon& m : w.mons)
    {
        edict_t* e = EDICT_NUM(m.num);
        if(!e->free && w.mode == 0 && m.oldThink)
        {
            e->v.think = m.oldThink;
            e->v.nextthink = static_cast<float>(sv.qcvm.time + 0.1);
        }
    }
    if(w.goal && !w.goal->free)
    {
        ED_Free(w.goal);
    }
    w.goal = nullptr;
    w.until = 0.0;
    mem::release(w.mons);
    if(!w.then.empty())
    {
        Cbuf_AddText(w.then.c_str());
        Cbuf_AddText("\n");
    }
}

void monsterWalkStep(MonsterWalk& w)
{
    const double now = sv.qcvm.time;
    for(MonsterWalk::Mon& m : w.mons)
    {
        edict_t* e = EDICT_NUM(m.num);
        if(e->free || e->v.health <= 0.f || static_cast<int>(e->v.solid) != SOLID_SLIDEBOX)
        {
            continue;
        }
        MonsterWalk::Counts& c = w.counts[m.cls];
        const glm::vec3 o{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
        ++c.steps;
        c.moved += glm::distance(o, m.last);
        const bool flies = (static_cast<int>(e->v.flags) & (FL_FLY | FL_SWIM)) != 0;
        if(!flies && o.z < m.last.z - 20.f)
        {
            ++c.drops;
            if(c.drops <= 2)
            {
                Con_DPrintf("mhullwalk: %s %d dropped %.0f at %.0f %.0f %.0f\n", monsterClasses[m.cls].name, m.num,
                    m.last.z - o.z, o.x, o.y, o.z);
            }
        }
        m.last = o;
        if(const edict_t* in = SV_TestEntityPosition(e))
        {
            if(++c.inSolid <= 2)
            {
                Con_Printf("mhullwalk: %s %d in %s at %.0f %.0f %.0f\n", monsterClasses[m.cls].name, m.num,
                    PR_GetString(in->v.classname), o.x, o.y, o.z);
            }
        }
        vec3_t ov{o.x, o.y, o.z};
        if(SV_HullPointContents(&sv.worldmodel->hulls[0], 0, ov) == CONTENTS_SOLID)
        {
            ++c.embedded;
        }
        if(now >= m.windowUntil)
        {
            c.stuck += glm::distance(o, m.windowStart) < 16.f && (w.mode == 0 || m.patrols);
            m.windowStart = o;
            m.windowUntil = now + 2.0;
        }
        if(w.mode == 1)
        {
            const int g = e->v.goalentity;
            if(m.patrols && g != m.lastGoal && g)
            {
                c.corners += !strcmp(PR_GetString(PROG_TO_EDICT(g)->v.classname), "path_corner");
            }
            m.lastGoal = g;
            continue;
        }
        const glm::vec2 across{o.x - m.goal.x, o.y - m.goal.y};
        const bool there = glm::length(across) < 32.f && std::abs(o.z - m.goal.z) < 48.f;
        if(there || now >= m.goalUntil)
        {
            c.reached += there;
            m.goal = monsterGoal(w, e);
            m.goalUntil = now + 8.0;
            ++c.given;
        }
        VectorCopy(m.goal, w.goal->v.origin);
        e->v.goalentity = EDICT_TO_PROG(w.goal);
        e->v.enemy = 0;
        pr_global_struct->self = EDICT_TO_PROG(e);
        G_FLOAT(OFS_PARM0) = 16.f;
        SV_MoveToGoal();
    }
}

void monsterWalkFrame()
{
    MonsterWalk& w = monsterWalk;
    if(w.until <= 0.0)
    {
        return;
    }
    if(sv.qcvm.time < w.started) // a new map
    {
        w.until = 0.0;
        w.goal = nullptr;
        mem::release(w.mons);
        return;
    }
    if(sv.qcvm.time >= w.until)
    {
        monsterWalkEnd(w);
        return;
    }
    if(sv.qcvm.time < w.nextStep)
    {
        return;
    }
    w.nextStep += 0.1;
    const int oldSelf = pr_global_struct->self;
    monsterWalkStep(w);
    pr_global_struct->self = oldSelf;
}

void monsterWalk_f()
{
    if(Cmd_Argc() < 2 || !sv.active)
    {
        Con_Printf("vr_mhull_walktest <seconds> [seed] [mode: 0 random walk, 1 patrol] [command run at the end]: the "
                   "level's monsters walked; the counts by class at the end\n");
        return;
    }
    MonsterWalk& w = monsterWalk;
    w.mons.clear();
    for(MonsterWalk::Counts& c : w.counts)
    {
        c = MonsterWalk::Counts{};
    }
    w.started = sv.qcvm.time;
    w.until = sv.qcvm.time + Q_atof(Cmd_Argv(1));
    w.nextStep = sv.qcvm.time;
    w.seed = Cmd_Argc() > 2 ? static_cast<std::uint32_t>(std::max(1, Q_atoi(Cmd_Argv(2)))) : 1u;
    w.mode = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 0;
    w.then.clear();
    for(int i = 4; i < Cmd_Argc(); ++i)
    {
        w.then += i > 4 ? " " : "";
        w.then += Cmd_Argv(i);
    }
    srand(w.seed); // movetogoal's turns
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    w.goal = ED_Alloc();
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        const int cls = e->free ? -1 : monsterClassOf(e);
        if(cls < 0 || e->v.health <= 0.f || static_cast<int>(e->v.solid) != SOLID_SLIDEBOX)
        {
            continue;
        }
        MonsterWalk::Mon m;
        m.num = i;
        m.cls = cls;
        m.last = m.windowStart = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
        m.windowUntil = sv.qcvm.time + 2.0;
        m.lastGoal = e->v.goalentity;
        m.patrols = e->v.goalentity && !strcmp(PR_GetString(PROG_TO_EDICT(e->v.goalentity)->v.classname), "path_corner");
        ++w.counts[cls].monsters;
        w.counts[cls].patrollers += m.patrols;
        if(w.mode == 0)
        {
            m.oldThink = e->v.think;
            e->v.nextthink = 0.f; // its AI stopped: the walk moves it
        }
        w.mons.push_back(m);
    }
    PR_PopQCVM(oldvm);
    Con_Printf("vr_mhull_walktest: %d monsters, %s\n", static_cast<int>(w.mons.size()), w.mode ? "patrol" : "random walk");
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

// vr_hull_leafdebug <edict>: a fresh compiled hull for that entity's narrow box, the leaf at its centre described.
void leafDebug_f()
{
    const int num = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1;
    if(!sv.active || !sv.worldmodel || num < 1 || num >= sv.qcvm.num_edicts)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    edict_t* ent = EDICT_NUM(num);
    float lo[3], hi[3];
    const Brushes* b = worldBrushes(sv.worldmodel);
    const bool narrow = b && moveBox(ent, ent->v.mins, ent->v.maxs, lo, hi);
    PR_PopQCVM(oldvm);
    if(!narrow)
    {
        return;
    }
    const glm::dvec3 c = glm::dvec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} +
                         (glm::dvec3{lo[0], lo[1], lo[2]} + glm::dvec3{hi[0], hi[1], hi[2]}) * 0.5;
    Tree t;
    t.ext = (glm::vec3{hi[0], hi[1], hi[2]} - glm::vec3{lo[0], lo[1], lo[2]}) * 0.5f;
    t.heads.assign(b->subs.size(), -1);
    buildTree(t, *b, 0, &c);
    Con_Printf("leaf debug at %.2f %.2f %.2f: %d nodes\n", c.x, c.y, c.z, static_cast<int>(t.nodes.size()));
}

void probe_f()
{
    if(!sv.active || !sv.worldmodel || svs.maxclients < 1)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    const int num = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1; // vr_hull_probe <edict>: a monster's (vr_mhull)
    if(num < 1 || num >= qcvm->num_edicts)
    {
        PR_PopQCVM(oldvm);
        return;
    }
    edict_t* ent = EDICT_NUM(num);
    float lo[3], hi[3];
    const Brushes* b = worldBrushes(sv.worldmodel);
    {
        // Quake's hull there (hull 1 or 2, as SV_HullForEntity picks it).
        const hull_t& qh = sv.worldmodel->hulls[ent->v.size[0] <= 32.f ? 1 : 2];
        vec3_t q;
        for(int i = 0; i < 3; ++i)
        {
            q[i] = ent->v.origin[i] - (qh.clip_mins[i] - ent->v.mins[i]);
        }
        Con_Printf("vr_hull_probe %d (%s): Quake's hull: %s\n", num, PR_GetString(ent->v.classname),
            SV_HullPointContents(const_cast<hull_t*>(&qh), qh.firstclipnode, q) == CONTENTS_SOLID ? "solid" : "empty");
    }
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
    // Method A's compiled hull at the box's centre.
    const Tree& t = treeFor(built, 0, glm::vec3{e});
    hull_t h{};
    h.clipnodes = const_cast<mclipnode_t*>(t.nodes.data());
    h.planes = const_cast<mplane_t*>(t.planes.data());
    h.firstclipnode = t.heads[0];
    h.lastclipnode = static_cast<int>(t.nodes.size()) - 1;
    vec3_t p{static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z)};
    Con_Printf("  compiled hull: %s\n", SV_HullPointContents(&h, h.firstclipnode, p) == CONTENTS_SOLID ? "solid" : "empty");
}

// vr_hull_approach [classname [n]]: how close the first player's box gets to things, through SV_Move as play moves it.
// No argument: from where the player stands, in 8 directions, to whatever the box meets first. With a classname: the
// n-th such entity (0 first), approached from 8 directions round it on its floor; a monster's box also moved into the
// player (the other way). Each: the gap from the player's centre to the surface it stopped at (Quake's 32 box: 16).
float centreGap(edict_t* player, const glm::vec3& from, const glm::vec3& dir)
{
    vec3_t a{from.x, from.y, from.z}, b{from.x + dir.x * 64.f, from.y + dir.y * 64.f, from.z + dir.z * 64.f};
    const trace_t tr = SV_Move(a, vec3_origin, vec3_origin, b, MOVE_NORMAL, player);
    return tr.fraction < 1.f ? tr.fraction * 64.f : -1.f;
}

// vr_hull_hittest [distance] [spread]: shots at the first player (hitscan traces, as a grunt's: aimed at its origin, spread
// per axis like FireBullets') from 8 directions round it at that distance (300; 0.1), 200 each with a fixed seed: how many
// hit it with the current Width Shots Hit (vr_hull_hit_width). Directions a wall blocks are left out.
void hitTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_hull_hittest [distance] [spread]: needs a map\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const float dist = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 300.f;
    const float spread = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 0.1f;
    edict_t* player = EDICT_NUM(1);
    const glm::vec3 target{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    std::uint32_t seed = 12345u;
    int shots = 0, hits = 0, dirs = 0;
    for(int k = 0; k < 8; ++k)
    {
        const float a = static_cast<float>(k) * 0.785398163f;
        const glm::vec3 from = target + glm::vec3{std::cos(a), std::sin(a), 0.f} * dist;
        vec3_t f{from.x, from.y, from.z}, t{target.x, target.y, target.z};
        const trace_t clear = SV_Move(f, vec3_origin, vec3_origin, t, MOVE_NOMONSTERS, nullptr);
        if(clear.fraction < 1.f || clear.startsolid || clear.allsolid)
        {
            continue; // a wall between, or outside the map
        }
        ++dirs;
        const glm::vec3 aim = glm::normalize(target - from);
        const glm::vec3 right = glm::normalize(glm::vec3{-aim.y, aim.x, 0.f}), up{0.f, 0.f, 1.f};
        for(int i = 0; i < 200; ++i)
        {
            const float cr = (random01(seed) + random01(seed)) - 1.f, cu = (random01(seed) + random01(seed)) - 1.f;
            const glm::vec3 end = from + (aim + right * (cr * spread) + up * (cu * spread)) * (dist * 2.f);
            vec3_t e{end.x, end.y, end.z};
            const trace_t tr = SV_Move(f, vec3_origin, vec3_origin, e, MOVE_NORMAL, nullptr);
            ++shots;
            hits += tr.ent == player ? 1 : 0;
        }
    }
    const float w = hitWidthSetting();
    Con_Printf("hullhit: width %g, distance %g, spread %g: %d of %d shots hit (%d directions)\n",
        w > 0.f ? w : 32.f, dist, spread, hits, shots, dirs);
    PR_PopQCVM(oldvm);
}

void approachRun();

void approach_f()
{
    if(!sv.active)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    approachRun();
    PR_PopQCVM(oldvm);
}

void approachRun()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_hull_approach [classname [n]]: needs a map\n");
        return;
    }
    edict_t* player = EDICT_NUM(1);
    const glm::vec3 origin{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    if(Cmd_Argc() < 2)
    {
        for(int k = 0; k < 8; ++k)
        {
            const float a = static_cast<float>(k) * 0.785398163f;
            const glm::vec3 dir{std::cos(a), std::sin(a), 0.f};
            const glm::vec3 end = origin + dir * 256.f;
            vec3_t s{origin.x, origin.y, origin.z}, e{end.x, end.y, end.z};
            const trace_t tr = SV_Move(s, player->v.mins, player->v.maxs, e, MOVE_NORMAL, player);
            if(tr.fraction >= 1.f)
            {
                continue;
            }
            const glm::vec3 stop{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
            Con_Printf("approach %d deg: %s, centre %.2f units from its surface\n", k * 45,
                tr.ent ? PR_GetString(tr.ent->v.classname) : "?", centreGap(player, stop, dir));
        }
        return;
    }
    const char* name = Cmd_Argv(1);
    int nth = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 0;
    edict_t* target = nullptr;
    for(int i = 1; i < qcvm->num_edicts && !target; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && !strcmp(PR_GetString(e->v.classname), name) && nth-- == 0)
        {
            target = e;
        }
    }
    if(!target)
    {
        Con_Printf("approach: no %s\n", name);
        return;
    }
    const int solid = static_cast<int>(target->v.solid);
    const glm::vec3 lo{target->v.absmin[0], target->v.absmin[1], target->v.absmin[2]};
    const glm::vec3 hi{target->v.absmax[0], target->v.absmax[1], target->v.absmax[2]};
    const glm::vec3 c = (lo + hi) * 0.5f;
    if(solid == SOLID_TRIGGER || solid == SOLID_NOT || solid == SOLID_NOT_BUT_TOUCHABLE)
    {
        Con_Printf("approach %s at %.0f %.0f %.0f: not solid (solid %d): touched, not met; the player's touch box is Quake's (%.0f wide), "
                   "so touched with its centre %.1f units from the %s's box\n",
            name, c.x, c.y, c.z, solid, player->v.maxs[0] - player->v.mins[0], (player->v.maxs[0] - player->v.mins[0]) * 0.5f, name);
        return;
    }
    const float reach = std::max(hi.x - lo.x, hi.y - lo.y) * 0.5f + 40.f;
    int tried = 0, met = 0;
    float gapLo = 1e9f, gapHi = -1e9f;
    for(int k = 0; k < 8; ++k)
    {
        const float a = static_cast<float>(k) * 0.785398163f;
        const glm::vec3 dir{-std::cos(a), -std::sin(a), 0.f}; // towards the entity
        bool done = false;
        for(float up = 1.f; up <= 25.f && !done; up += 8.f)
        {
            const glm::vec3 start{c.x - dir.x * reach, c.y - dir.y * reach, lo.z + 24.f + up};
            vec3_t s{start.x, start.y, start.z}, e{c.x, c.y, start.z};
            const trace_t fit = SV_Move(s, player->v.mins, player->v.maxs, s, MOVE_NORMAL, player);
            if(fit.startsolid)
            {
                continue;
            }
            const trace_t tr = SV_Move(s, player->v.mins, player->v.maxs, e, MOVE_NORMAL, player);
            done = true;
            ++tried;
            if(tr.ent != target)
            {
                Con_Printf("approach %s %d deg: stopped by %s first\n", name, k * 45,
                    tr.ent ? PR_GetString(tr.ent->v.classname) : "nothing");
                continue;
            }
            ++met;
            const glm::vec3 stop{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
            const float gap = centreGap(player, stop, dir);
            gapLo = std::min(gapLo, gap);
            gapHi = std::max(gapHi, gap);
            // A monster moved into the player standing there (a body's move: it meets the player's box as shown to it).
            if((static_cast<int>(target->v.flags) & FL_MONSTER) && k == 0)
            {
                vec3_t was, back;
                VectorCopy(player->v.origin, was);
                VectorCopy(tr.endpos, player->v.origin);
                player->v.origin[0] -= dir.x * 24.f;
                player->v.origin[1] -= dir.y * 24.f;
                SV_LinkEdict(player, false);
                VectorCopy(target->v.origin, back);
                vec3_t mend{player->v.origin[0], player->v.origin[1], target->v.origin[2]};
                const trace_t mt = SV_Move(target->v.origin, target->v.mins, target->v.maxs, mend, MOVE_NORMAL, target);
                const glm::vec3 pc{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
                // The monster's box face from the player's centre, once moved.
                VectorCopy(mt.endpos, target->v.origin);
                SV_LinkEdict(target, false);
                const float gap = centreGap(player, pc, dir);
                Con_Printf("approach %s moved into the player: hit %s; player's centre %.2f units from its box\n", name,
                    mt.ent ? PR_GetString(mt.ent->v.classname) : "nothing", gap);
                VectorCopy(back, target->v.origin);
                SV_LinkEdict(target, false);
                VectorCopy(was, player->v.origin);
                SV_LinkEdict(player, false);
            }
        }
    }
    Con_Printf("approach %s (%s at %.0f %.0f %.0f): %d directions tried, %d met it: centre %.2f to %.2f units from its surface\n", name,
        PR_GetString(target->v.model), c.x, c.y, c.z, tried, met, met ? gapLo : 0.f, met ? gapHi : 0.f);
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
    // The level's exits closed (not solid): walking into one would end the walk at the intermission, the player put
    // at its camera spot (in a ceiling, often) and frozen there.
    if(sv.active)
    {
        qcvm_t* oldvm = nullptr;
        PR_PushQCVM(&sv.qcvm, &oldvm);
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
        {
            edict_t* e = EDICT_NUM(i);
            if(!e->free && !strcmp(PR_GetString(e->v.classname), "trigger_changelevel"))
            {
                e->v.solid = SOLID_NOT;
                SV_LinkEdict(e, false);
            }
        }
        PR_PopQCVM(oldvm);
    }
}

// The world's brushes, and with method A its hull for the player's box: built with the map or the setting (not at the
// first move: a hitch in play).
bool wanted()
{
    return sv.worldmodel && vr_hull_method.value != 0.f && (vr_hull_width.value > 0.f || vr_mhull.value != 0.f);
}

glm::vec3 playerExt(const qmodel_t* world)
{
    const float half = widthSetting() * 0.5f;
    return glm::vec3{half, half, hull1Height(world) * 0.5f};
}

// The boxes the trees are wanted for: the player's (first, if on), then each size the level's monsters ask for
// (vr_mhull), in their order (loading: while the server spawns them).
std::vector<glm::vec3> wantedExts(bool loading)
{
    std::vector<glm::vec3> exts;
    if(vr_hull_width.value > 0.f)
    {
        exts.push_back(playerExt(sv.worldmodel));
    }
    if(vr_mhull.value != 0.f && (sv.active || loading))
    {
        for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; ++i)
        {
            const edict_t* e = reinterpret_cast<const edict_t*>(reinterpret_cast<const byte*>(sv.qcvm.edicts) +
                static_cast<std::ptrdiff_t>(i) * sv.qcvm.edict_size);
            float w = 0.f, h = 0.f;
            if(!e->free && monsterWidth(e, w, h, loading))
            {
                exts.emplace_back(w * 0.5f, w * 0.5f, h * 0.5f);
            }
        }
    }
    return exts;
}

// The trees for those boxes, compiled at once on the pool (not at the first move: a hitch in play).
void prepare()
{
    if(!wanted() || !worldBrushes(sv.worldmodel))
    {
        return;
    }
    compileTrees(claimTrees(built.clipnodes, wantedExts(false)), built);
}

void onWidthChanged(cvar_t*)
{
    prepare();
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_hull_stats", stats_f);
    Cmd_AddCommand("vr_hull_bench", bench_f);
    Cmd_AddCommand("vr_hull_walktest", walkTest_f);
    Cmd_AddCommand("vr_hull_probe", probe_f);
    Cmd_AddCommand("vr_hull_leafdebug", leafDebug_f);
    Cmd_AddCommand("vr_hull_approach", approach_f);
    Cmd_AddCommand("vr_hull_hittest", hitTest_f);
    Cmd_AddCommand("vr_mhull_walktest", monsterWalk_f);
    Cmd_AddCommand("vr_mhull_reset", reset_f);
    Cvar_SetCallback(&vr_hull_width, onWidthChanged);
    Cvar_SetCallback(&vr_hull_method, onWidthChanged);
    Cvar_SetCallback(&vr_mhull, onWidthChanged);
    for(const MonsterClass& c : monsterClasses)
    {
        Cvar_SetCallback(c.width, onWidthChanged);
    }
}

void beforeLoad()
{
    settle();
    qmodel_t* world = sv.worldmodel;
    if(!wanted() || world->type != mod_brush || world->numnodes <= 0 || built.clipnodes == world->hulls[0].clipnodes)
    {
        return;
    }
    pending.posted = Sys_DoubleTime();
    monsterTrees.trees.reserve(maxMonsterTrees); // (a slot taken while others are made moves none of them)
    pending.brushes = jobs::async([world] { build(world); });
    pending.playerExt = glm::vec3{0.f};
    if(vr_hull_width.value > 0.f)
    {
        pending.playerExt = playerExt(world);
        for(Tree* t : claimTrees(world->hulls[0].clipnodes, {pending.playerExt}))
        {
            postTree(t);
        }
    }
}

void entitySpawned(const edict_t* ent)
{
    float w = 0.f, h = 0.f;
    if(!pending.brushes.valid() || !wanted() || sv.state != ss_loading || ent->free || !monsterWidth(ent, w, h, true))
    {
        return;
    }
    const glm::vec3 ext{w * 0.5f, w * 0.5f, h * 0.5f};
    if(vr_hull_width.value > 0.f && playerExt(sv.worldmodel) != pending.playerExt)
    {
        return; // (spawned: see there)
    }
    if(findSlot(sv.worldmodel->hulls[0].clipnodes, ext))
    {
        return; // (made or being made)
    }
    for(Tree* t : claimTrees(sv.worldmodel->hulls[0].clipnodes, {ext}))
    {
        postTree(t);
    }
}

void spawned()
{
    if(!pending.brushes.valid() || !wanted())
    {
        return;
    }
    const std::vector<glm::vec3> exts = wantedExts(true);
    if(vr_hull_width.value > 0.f && exts.front() != pending.playerExt)
    {
        settle(); // (the player's width changed while the map spawned: its tree is made again, not while it is made)
        return;
    }
    for(Tree* t : claimTrees(sv.worldmodel->hulls[0].clipnodes, exts))
    {
        postTree(t);
    }
}

void finishLoads()
{
    settle();
}

void afterLoad()
{
    prepare();
}

bool moveBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs)
{
    float width = 0.f, height = 0.f;
    if(monsterWidth(passedict, width, height) && ownBox(passedict, mins, maxs))
    {
        narrowBox(mins, maxs, width, boxMins, boxMaxs);
        boxMaxs[2] = mins[2] + height; // Quake's hull's height, from the box's feet as Quake places it
        return true;
    }
    if(vr_hull_width.value <= 0.f || !sv.active || !sv.worldmodel || !isPlayerBox(passedict, mins, maxs))
    {
        return false;
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

bool entBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs)
{
    float width = entWidthSetting();
    if(width > 0.f && sv.active && isPlayerBox(passedict, mins, maxs))
    {
        narrowBox(mins, maxs, width, boxMins, boxMaxs);
        return true;
    }
    if(vr_mhull_ents.value != 0.f && monsterNarrower(passedict, width) && ownBox(passedict, mins, maxs))
    {
        narrowBox(mins, maxs, width, boxMins, boxMaxs);
        return true;
    }
    return false;
}

bool narrowsAgainst(const edict_t* mover, const edict_t* other)
{
    const int num = clientNum(mover);
    return num >= 1 && num <= svs.maxclients ? categoryOn(other) : true; // a monster's width meets every body
}

bool touchBox(const edict_t* touch, const edict_t* mover, float* boxMins, float* boxMaxs)
{
    float width = entWidthSetting();
    if(width > 0.f && sv.active && isPlayerBox(touch, touch->v.mins, touch->v.maxs) && categoryOn(mover))
    {
        narrowBox(touch->v.mins, touch->v.maxs, width, boxMins, boxMaxs);
        return true;
    }
    if(vr_mhull_ents.value != 0.f && monsterNarrower(touch, width))
    {
        narrowBox(touch->v.mins, touch->v.maxs, width, boxMins, boxMaxs); // a body moving into a monster (vr_mhull)
        return true;
    }
    return false;
}

bool footprint(const edict_t* ent, float* absMins, float* absMaxs)
{
    float width = 0.f;
    if(vr_mhull_ledges.value == 0.f || !monsterNarrower(ent, width))
    {
        return false;
    }
    narrowBox(absMins, absMaxs, width, absMins, absMaxs);
    return true;
}

bool hitBox(const edict_t* touch, float* boxMins, float* boxMaxs)
{
    const float width = hitWidthSetting();
    if(width <= 0.f || !sv.active || !isPlayerBox(touch, touch->v.mins, touch->v.maxs))
    {
        return false;
    }
    narrowBox(touch->v.mins, touch->v.maxs, width, boxMins, boxMaxs);
    return true;
}

bool clipBSP(const edict_t* ent, const float* start, const float* boxMins, const float* boxMaxs, const float* end,
    trace_t& trace)
{
    if(ent != sv.qcvm.edicts && vr_hull_brushmodels.value == 0.f)
    {
        return false; // brush models meet Quake's hull 1 (the setting off)
    }
    const int index = static_cast<int>(ent->v.modelindex);
    qmodel_t* model = index >= 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model || model->type != mod_brush || !worldBrushes(sv.worldmodel))
    {
        return false;
    }
    const int sub = subOf(built, index);
    if(sub < 0)
    {
        return false;
    }
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 s = glm::vec3{start[0], start[1], start[2]} - origin;
    const glm::vec3 e = glm::vec3{end[0], end[1], end[2]} - origin;
    const glm::vec3 lo{boxMins[0], boxMins[1], boxMins[2]}, hi{boxMaxs[0], boxMaxs[1], boxMaxs[2]};
    ++vr_profcounts.hullchecks;
    if(vr_hull_method.value != 0.f)
    {
        const glm::vec3 centre = (lo + hi) * 0.5f;
        const Tree& t = treeFor(built, static_cast<std::size_t>(sub), (hi - lo) * 0.5f);
        trace = treeTrace(t, t.heads[static_cast<std::size_t>(sub)], s + centre, e + centre);
        for(int i = 0; i < 3; ++i)
        {
            trace.endpos[i] += origin[i] - centre[i];
        }
        return true;
    }
    const SubModel& sm = built.subs[static_cast<std::size_t>(sub)];
    trace = boxTrace(built, model->hulls[0], sm.head, s, lo, hi, e, sm.base).trace;
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
    const glm::vec3 lo{-half, -half, -24.f}, hi{half, half, -24.f + hull1Height(world)};
    trace_t tr;
    if(vr_hull_method.value != 0.f)
    {
        const glm::vec3 centre = (lo + hi) * 0.5f;
        const Tree& t = treeFor(built, 0, (hi - lo) * 0.5f);
        tr = treeTrace(t, t.heads[0], start + centre, end + centre);
    }
    else
    {
        tr = boxTrace(*b, world->hulls[0], 0, start, lo, hi, end).trace;
    }
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.f;
}

void walkTestFrame(edict_t* ent)
{
    if(clientNum(ent) == 1)
    {
        monsterWalkFrame(); // vr_mhull_walktest
    }
    WalkTest& w = walkTest;
    if(w.until <= 0.0 || clientNum(ent) != 1)
    {
        return;
    }
    if(sv.qcvm.time >= w.until)
    {
        Con_Printf("hullwalk %s: width %g (%s), entities %g, %d frames, %.0f units walked, %d hops (%d near pushers, %d failed), stuck %d "
                   "(%d right after a hop), in monsters or items %d, embedded %d, outside %d\n",
            sv.worldmodel->name, vr_hull_width.value > 0.f ? widthSetting() : 32.f,
            vr_hull_width.value <= 0.f ? "Quake's hull 1" : (vr_hull_method.value != 0.f ? "compiled hull" : "brush sweep"), entWidthSetting() > 0.f ? entWidthSetting() : 32.f,
            w.frames, w.travelled, w.hops,
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
    const glm::vec3 prev = w.last;
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
            Con_Printf("hullwalk: stuck at %.1f %.1f %.1f in %s (velocity %.0f %.0f %.0f; the frame before at %.1f %.1f %.1f, on %s)\n", o.x, o.y, o.z, what,
                ent->v.velocity[0], ent->v.velocity[1], ent->v.velocity[2], prev.x, prev.y, prev.z, ent->v.groundentity ? PR_GetString(PROG_TO_EDICT(ent->v.groundentity)->v.classname) : "nothing");
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

extern "C" int VR_HullEntBox(edict_t* passedict, const float* mins, const float* maxs, float* boxmins, float* boxmaxs)
{
    return qvr::hull::entBox(passedict, mins, maxs, boxmins, boxmaxs);
}

extern "C" int VR_HullNarrowsAgainst(edict_t* mover, edict_t* other)
{
    return qvr::hull::narrowsAgainst(mover, other);
}

extern "C" int VR_HullFootprint(edict_t* ent, float* absmins, float* absmaxs)
{
    return qvr::hull::footprint(ent, absmins, absmaxs);
}

extern "C" int VR_HullTouchBox(edict_t* touch, edict_t* mover, float* boxmins, float* boxmaxs)
{
    return qvr::hull::touchBox(touch, mover, boxmins, boxmaxs);
}

extern "C" int VR_HullHitBox(edict_t* touch, float* boxmins, float* boxmaxs)
{
    return qvr::hull::hitBox(touch, boxmins, boxmaxs);
}
