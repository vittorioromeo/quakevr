// vr_hull.cpp -- see vr_hull.hpp.

#include "vr_hull.hpp"
#include "vr_alloccount.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StablePartition.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

#include <atomic>
#include <string.h>
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
constexpr float headTop = 5.f; // units: the top of a player's head over the headset's middle (hitBox; QC VR_SHOT_OVER_HEAD)

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
    za::U32 first;
    za::U32 count;
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
    za::U32 base;           // its hull 0's leaves in leafBrush: [base + node * 2 + side]
    za::U32 firstBrush, numBrushes;
};

// The server map's solid space as brushes: pointers into the hunk (the world's hull 0), released at every map change.
struct Brushes
{
    const mclipnode_t* clipnodes = nullptr; // the hull 0 they were built from (the world's and its brush models')
    za::Vector<Plane> planes;
    za::Vector<Brush> brushes;
    za::Vector<int> leafBrush; // [base + node * 2 + side]: the brush of that child when it is a solid leaf, else -1
    za::Vector<SubModel> subs; // the world's submodels (0: the world), then external models as they are met
    za::Vector<int> modelSub;  // [modelindex]: its entry in subs; -1 none (not a brush model), -2 not looked up
    za::Vector<int> clips;     // the world's recovered clip brushes
    za::Vector<int> leafClipStart; // [node * 2 + side]: where the clip brushes whose box centres reach into that
    za::Vector<int> leafClipList;  // leaf of the world's hull 0 (empty or solid) start in leafClipList (and end: +1)
    mutable za::Vector<za::SizeT> clipStamp; // a clip brush's last test (one per sweep: it is in many leaves)
    mutable za::SizeT stamp = 0;
    za::Vector<int> hull1Clip; // [hull 1 node * 2 + side]: a solid leaf's clip brush, -1 not one, -2 not looked at
    double ms = 0.0;            // the build's time
    double clipMs = 0.0;        // of which the clip brushes'
    int bevels = 0;             // planes added as Quake 2's bevels (axial, edge)
    int dropped = 0;            // solid leaves with no volume left (slivers under the build's epsilon)
    int hull1Leaves = 0;        // the world's hull 1 solid leaves looked at for clip brushes
    int numnodes = 0;
    za::U64 key = 0;            // the world's content (keyOf): what they are kept under for the same map's next load
    // The world's own (build): external models' brushes, planes and leaves follow (subOf), let go when kept.
    za::SizeT worldSubs = 0, worldPlanes = 0, worldBrushes = 0, worldLeafBrush = 0;
    int worldBevels = 0, worldDropped = 0;
    auto members()
    {
        return qvr::mem::list(clipnodes, planes, brushes, leafBrush, subs, modelSub, clips, leafClipStart, leafClipList, clipStamp, stamp, hull1Clip, ms,
            clipMs, bevels, dropped, hull1Leaves, numnodes, key, worldSubs, worldPlanes, worldBrushes, worldLeafBrush, worldBevels,
            worldDropped);
    }
};

// Its heap bytes (a kept map's: KeptMap).
za::SizeT heldBytes(const Brushes& b)
{
    return mem::heldBytes(b.planes) + mem::heldBytes(b.brushes) + mem::heldBytes(b.leafBrush) + mem::heldBytes(b.subs) +
           mem::heldBytes(b.modelSub) + mem::heldBytes(b.clips) + mem::heldBytes(b.leafClipStart) +
           mem::heldBytes(b.leafClipList) + mem::heldBytes(b.clipStamp) + mem::heldBytes(b.hull1Clip);
}
mem::Cache<Brushes> built{"hull brushes", mem::MapChange};

// ---------------------------------------------------------------------------------------------------------------
// The build: hull 0's tree walked from each model's head node, a convex polytope (faces with their windings)
// split by each node's plane on the way down; at a solid (or sky: solid to Quake's clipping hulls) leaf what is
// left is that leaf's brush.

// A face's points, in place up to 8 (PROFILING_2026-10.md, "Hull build: the containers": a winding starts as 4 points
// and gains one at most a cut; measured on warden, ad_grendel, e4m7 and e1m1: 4.1-4.6 points on average, 9 at the 99th
// percentile, 24 at most; 98.5% of warden's and 99.97% of the others' fit). Over 8 they spill to the heap.
constexpr za::SizeT windingInline = 8;
using Winding = za::SmallVector<glm::dvec3, windingInline>;

struct Face
{
    glm::dvec3 normal; // outward
    double dist;
    Winding w; // empty: the plane bounds the piece but its face was lost to the epsilon (the plane is kept)
    int tag = -1; // method A's build (Tree): the face's plane in the tree's table while not yet split on, else -1
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(za::isTriviallyRelocatable<Winding>); // (Poly's growth: a copy of its bytes)
};
using Poly = za::Vector<Face>;

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
    const za::SizeT count = in.size();
    if(count < 3)
    {
        return;
    }
    const double sign = keepFront ? 1.0 : -1.0;
    // The points' distances and sides: in place for a winding of up to 64 points (all of them in practice: 24 at most
    // measured; the heap's two allocations a call were a third of a large map's hull build, its threads queueing on the
    // heap's lock), on the heap above.
    za::SmallVector<double, 64> dists;
    za::SmallVector<int, 64> sides;
    dists.resize(count);
    sides.resize(count);
    bool anyBack = false, anyFront = false;
    for(za::SizeT i = 0; i < count; ++i)
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
    // (A convex winding cut by a plane gains one point at most: up to windingInline - 1 points in, it stays in place.)
    for(za::SizeT i = 0; i < count; ++i)
    {
        const za::SizeT j = (i + 1) % count;
        if(sides[i] >= 0)
        {
            out.pushBack(in[i]);
        }
        if(sides[i] == 0 || sides[j] == 0 || sides[i] == sides[j])
        {
            continue;
        }
        const double t = dists[i] / (dists[i] - dists[j]);
        out.pushBack(in[i] + (in[j] - in[i]) * t);
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
            lo = za::min(lo, t);
            hi = za::max(hi, t);
        }
    }
    if(lo > hi) // no points at all: nothing left
    {
        return;
    }
    if(lo >= -onEpsilon)
    {
        front = ZA_MOVE(p);
        return;
    }
    if(hi <= onEpsilon)
    {
        back = ZA_MOVE(p);
        return;
    }
    // The cap: the plane's winding clipped by each face in turn (two windings, each clip into the other).
    Winding caps[2] = {baseWinding(n, d), {}};
    int cap = 0;
    front.reserve(p.size() + 1); // (each side: a face of each of p's at most, and the cap: references stay valid)
    back.reserve(p.size() + 1);
    for(Face& f : p)
    {
        clipWinding(caps[cap], f.normal, f.dist, false, caps[cap ^ 1]);
        cap ^= 1;
        if(f.w.empty())
        {
            front.pushBack(f);
            back.pushBack(f);
            continue;
        }
        // Each side's face clipped straight into its place, taken back if nothing is left of it, unless nothing is left
        // on both sides (a sliver of a face lost to the epsilon): its plane still bounds both pieces. Dropped, a piece
        // could lose its only bound that way (found with a monster's 24-wide hull on e1m4: a piece reaching to the bogus
        // winding's end, its leaf solid out in the open).
        front.pushBack(Face{f.normal, f.dist, {}, f.tag});
        back.pushBack(Face{f.normal, f.dist, {}, f.tag});
        clipWinding(f.w, n, d, true, front.back().w);
        clipWinding(f.w, n, d, false, back.back().w);
        const bool frontLeft = !front.back().w.empty(), backLeft = !back.back().w.empty();
        if(frontLeft != backLeft)
        {
            (frontLeft ? back : front).popBack();
        }
    }
    // The cap's plane bounds both pieces even when its face is lost to the epsilon.
    front.pushBack(Face{-n, -d, caps[cap]});
    back.pushBack(Face{n, d, ZA_MOVE(caps[cap])});
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
            p.pushBack(Face{n, s ? maxs[axis] : -mins[axis], {}});
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
                f.w = tmp;
            }
        }
    }
    return p;
}

bool hasPlane(const Brushes& b, za::SizeT first, const glm::dvec3& n, double d)
{
    for(za::SizeT i = first; i < b.planes.size(); ++i)
    {
        const Plane& q = b.planes[i];
        if(glm::dot(glm::dvec3{q.normal}, n) > 1.0 - 1e-6 && za::abs(q.dist - d) < 0.01)
        {
            return true;
        }
    }
    return false;
}

bool hasNormal(const Brushes& b, za::SizeT first, const glm::dvec3& n)
{
    for(za::SizeT i = first; i < b.planes.size(); ++i)
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
    const za::SizeT first = b.planes.size();
    for(const Face& f : p)
    {
        if(!hasPlane(b, first, f.normal, f.dist))
        {
            b.planes.pushBack(Plane{glm::vec3{f.normal}, static_cast<float>(f.dist), 1.f});
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
                b.planes.pushBack(Plane{glm::vec3{n}, static_cast<float>(s ? hi[axis] : -lo[axis]), 1.f});
                ++b.bevels;
            }
        }
    }
    // Edge bevels: along each slanted edge, the planes through it square to an axis that have the whole piece behind.
    for(const Face& f : p)
    {
        const za::SizeT count = f.w.size();
        for(za::SizeT i = 0; i < count; ++i)
        {
            const glm::dvec3& a = f.w[i];
            glm::dvec3 e = f.w[(i + 1) % count] - a;
            const double len = glm::length(e);
            if(len < 0.5)
            {
                continue;
            }
            e /= len;
            if(za::abs(e.x) > 0.9999 || za::abs(e.y) > 0.9999 || za::abs(e.z) > 0.9999)
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
                        b.planes.pushBack(Plane{glm::vec3{n}, static_cast<float>(d), 1.f});
                        ++b.bevels;
                    }
                }
            }
        }
    }
    b.brushes.pushBack(Brush{static_cast<za::U32>(first), static_cast<za::U32>(b.planes.size() - first),
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
    splitPoly(ZA_MOVE(poly), glm::dvec3{plane.normal[0], plane.normal[1], plane.normal[2]}, plane.dist, parts[0],
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
            walk(hull, child, ZA_MOVE(parts[side]), onSolid);
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

void splitTop(const hull_t& hull, int num, Poly&& poly, int depth, za::Vector<WalkItem>& items)
{
    if(num < hull.firstclipnode || num > hull.lastclipnode)
    {
        return;
    }
    if(depth == 0)
    {
        items.pushBack(WalkItem{num, -1, ZA_MOVE(poly)});
        return;
    }
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    Poly parts[2];
    splitPoly(ZA_MOVE(poly), glm::dvec3{plane.normal[0], plane.normal[1], plane.normal[2]}, plane.dist, parts[0],
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
            splitTop(hull, child, ZA_MOVE(parts[side]), depth - 1, items);
        }
        else if(solidContents(child))
        {
            items.pushBack(WalkItem{num, side, ZA_MOVE(parts[side])});
        }
    }
}

jobs::Site walkSite{"hull walk"}; // (its parallelFor: vr_jobs_sites)

// Each item walked (items[i]'s pieces to onSolid(num, side, piece, outs[i])), on the pool.
template <class Out, class OnSolid>
void walkItems(const hull_t& hull, za::Vector<WalkItem>& items, za::Vector<Out>& outs, const OnSolid& onSolid)
{
    outs.clear();
    outs.resize(items.size());
    jobs::parallelFor(walkSite, items.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                Out& out = outs[i];
                auto on = [&](int num, int side, const Poly& piece) { onSolid(num, side, piece, out); };
                if(items[i].side < 0)
                {
                    walk(hull, items[i].num, ZA_MOVE(items[i].poly), on);
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
void addSubModel(Brushes& b, const hull_t& hull0, int numnodes, za::U32 base, int head, const float* mins,
    const float* maxs)
{
    const auto first = static_cast<za::U32>(b.brushes.size());
    if(head >= 0 && head < numnodes)
    {
        hull_t h = hull0;
        h.firstclipnode = 0;
        h.lastclipnode = numnodes - 1;
        const glm::dvec3 margin{64.0};
        const glm::dvec3 lo = glm::dvec3{mins[0], mins[1], mins[2]} - margin;
        const glm::dvec3 hi = glm::dvec3{maxs[0], maxs[1], maxs[2]} + margin;
        auto onSolid = [&b, base](int num, int side, const Poly& piece)
        { b.leafBrush[base + static_cast<za::SizeT>(num) * 2 + side] = emitBrush(b, piece); };
        walk(h, head, boxPoly(lo, hi), onSolid);
    }
    b.subs.pushBack(SubModel{hull0.clipnodes, head, base, first, static_cast<za::U32>(b.brushes.size()) - first});
}

// The map as brushes into b (the world's content: key).
void build(Brushes& b, qmodel_t* world, za::U64 key)
{
    const auto t0 = za::Clock::nowNanoseconds();
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
    b.leafBrush.clear();
    b.leafBrush.resize(static_cast<za::SizeT>(b.numnodes) * 2, -1);
    b.subs.clear();
    b.modelSub.clear();
    b.modelSub.resize(MAX_MODELS, -2);
    // The world's models (addSubModel's walk for each), their walks shared out on the pool together.
    hull_t h = hull0;
    h.firstclipnode = 0;
    h.lastclipnode = b.numnodes - 1;
    za::Vector<WalkItem> items;
    za::Vector<za::SizeT> subEnd(static_cast<za::SizeT>(za::max(world->numsubmodels, 0)));
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
        subEnd[static_cast<za::SizeT>(i)] = items.size();
    }
    struct Out
    {
        Brushes b; // the item's brushes and their planes, numbered from 0
        za::Vector<za::Pair<za::SizeT, int>> leaves; // (leaf key, its brush in b or -1)
    };
    za::Vector<Out> outs;
    walkItems(h, items, outs,
        [](int num, int side, const Poly& piece, Out& out)
        { out.leaves.emplaceBack(static_cast<za::SizeT>(num) * 2 + side, emitBrush(out.b, piece)); });
    za::SizeT item = 0;
    for(int i = 0; i < world->numsubmodels; ++i)
    {
        const auto first = static_cast<za::U32>(b.brushes.size());
        for(; item < subEnd[static_cast<za::SizeT>(i)]; ++item)
        {
            Out& o = outs[item];
            const auto planeBase = static_cast<za::U32>(b.planes.size());
            const auto brushBase = static_cast<int>(b.brushes.size());
            b.planes.emplaceBackRange(o.b.planes.data(), o.b.planes.size());
            for(Brush br : o.b.brushes)
            {
                br.first += planeBase;
                b.brushes.pushBack(br);
            }
            for(const auto& [key, brush] : o.leaves)
            {
                b.leafBrush[key] = brush < 0 ? -1 : brush + brushBase;
            }
            b.bevels += o.b.bevels;
            b.dropped += o.b.dropped;
        }
        b.subs.pushBack(SubModel{hull0.clipnodes, world->submodels[i].headnode[0], 0, first,
            static_cast<za::U32>(b.brushes.size()) - first});
    }
    const auto t1 = za::Clock::nowNanoseconds();
    recoverClips(b, world);
    const auto t2 = za::Clock::nowNanoseconds();
    b.clipMs = (static_cast<double>(t2 - t1) / 1e6);
    b.ms = (static_cast<double>(t2 - t0) / 1e6);
    b.key = key;
    b.worldSubs = b.subs.size();
    b.worldPlanes = b.planes.size();
    b.worldBrushes = b.brushes.size();
    b.worldLeafBrush = b.leafBrush.size();
    b.worldBevels = b.bevels;
    b.worldDropped = b.dropped;
}

// What the map's brushes and compiled hulls are made from, hashed (they are kept for a load of a map with the same:
// keepForReload): hull 0's nodes (the brushes' walk), hull 1's (the clip brushes'), the planes, the models' heads and
// bounds, and the build's version (to be bumped when a change to the build changes its results).
constexpr za::U64 buildVersion = 1;
za::U64 keyOf(const qmodel_t* world)
{
    namespace wy = ankerl::unordered_dense::detail::wyhash;
    za::U64 h = wy::mix(buildVersion, 0x9e3779b97f4a7c15ull);
    const auto add = [&h](const void* p, za::SizeT bytes) { h = wy::mix(h ^ bytes, p && bytes ? wy::hash(p, bytes) : 0); };
    const int counts[] = {world->numnodes, world->numclipnodes, world->numplanes, world->numsubmodels};
    add(counts, sizeof(counts));
    const hull_t& h0 = world->hulls[0];
    const hull_t& h1 = world->hulls[1];
    add(h0.clipnodes, static_cast<za::SizeT>(za::max(world->numnodes, 0)) * sizeof(mclipnode_t));
    add(h1.clipnodes, static_cast<za::SizeT>(za::max(world->numclipnodes, 0)) * sizeof(mclipnode_t));
    const int range[] = {h0.firstclipnode, h0.lastclipnode, h1.firstclipnode, h1.lastclipnode};
    add(range, sizeof(range));
    add(h1.clip_mins, sizeof(vec3_t));
    add(h1.clip_maxs, sizeof(vec3_t));
    for(int i = 0; i < world->numplanes; ++i)
    {
        const mplane_t& p = world->planes[i];
        const float v[] = {p.normal[0], p.normal[1], p.normal[2], p.dist};
        add(v, sizeof(v));
    }
    for(int i = 0; i < world->numsubmodels; ++i)
    {
        const dmodel_t& m = world->submodels[i];
        add(&m.headnode[0], sizeof(m.headnode[0]));
        add(m.mins, sizeof(m.mins));
        add(m.maxs, sizeof(m.maxs));
    }
    return h ? h : 1; // (0: none)
}

float widthSetting()
{
    return za::clamp(vr_hull_width.value, minWidth, maxWidth);
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
        build(built, world, keyOf(world));
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
    za::SizeT base = 0; // the model's leaves in leafBrush (SubModel::base)
    const float* halfspace = nullptr; // portal body portion: dot(normal, point) >= distance, in model space
    const Plane* boxPlanes = nullptr; // a solid entity's six planes
};

// Minimum projection of the box portion on this side of a portal. Its vertices are retained box corners and
// intersections of the twelve edges with the gate plane. Within a corner-crossing interval these move linearly.
double support(const Plane& p, const glm::dvec3& ext);
double halfDistance(const Sweep& s, const Plane& p, const glm::dvec3& centre)
{
    if(!s.halfspace) { return glm::dot(centre, glm::dvec3{p.normal}) - p.dist - support(p, s.ext); }
    const glm::dvec3 gate{s.halfspace[0], s.halfspace[1], s.halfspace[2]};
    const glm::dvec3 n{p.normal};
    glm::dvec3 v[8];
    double d[8], result = 1e30;
    for(int c = 0; c < 8; c++)
    {
        v[c] = centre + s.ext * glm::dvec3{(c & 1) ? 1.0 : -1.0, (c & 2) ? 1.0 : -1.0, (c & 4) ? 1.0 : -1.0};
        d[c] = glm::dot(gate, v[c]) - s.halfspace[3];
        if(d[c] >= -1e-6) { result = za::min(result, glm::dot(n, v[c]) - p.dist); }
    }
    for(int c = 0; c < 8; c++)
    {
        for(int axis = 0; axis < 3; axis++)
        {
            const int other = c ^ (1 << axis);
            if(other > c && (d[c] < 0.0) != (d[other] < 0.0))
            {
                const glm::dvec3 at = v[c] + (v[other] - v[c]) * (d[c] / (d[c] - d[other]));
                result = za::min(result, glm::dot(n, at) - p.dist);
            }
        }
    }
    // Recovered clip planes already include hull growth; preserve their convention.
    return result < 1e29 && p.grows != 1.0 ? glm::dot(centre, n) - p.dist - support(p, s.ext) : result;
}

double support(const Plane& p, const glm::dvec3& ext)
{
    return p.grows * (za::abs(p.normal.x * ext.x) + za::abs(p.normal.y * ext.y) + za::abs(p.normal.z * ext.z));
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
    for(za::U32 i = 0; i < br.count; ++i)
    {
        const Plane& p = s.boxPlanes ? s.boxPlanes[br.first + i] : s.b->planes[br.first + i];
        const double d1 = halfDistance(s, p, s.start), d2 = halfDistance(s, p, s.end);
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
            const double f = za::max(0.0, (d1 - distEpsilon) / (d1 - d2));
            if(f > enter)
            {
                enter = f;
                clip = &p;
            }
        }
        else // leaving
        {
            leave = za::min(leave, (d1 + distEpsilon) / (d1 - d2));
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
        s.fraction = za::max(enter, 0.0);
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
        offset = za::abs(s.ext.x * n.x) + za::abs(s.ext.y * n.y) + za::abs(s.ext.z * n.z);
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
    frac = za::clamp(frac, 0.0, 1.0);
    frac2 = za::clamp(frac2, 0.0, 1.0);
    const double midf = p1f + (p2f - p1f) * frac;
    sweepChild(s, num, side, p1f, midf, p1, p1 + (p2 - p1) * frac);
    const double midf2 = p1f + (p2f - p1f) * frac2;
    sweepChild(s, num, side ^ 1, midf2, p2f, p1 + (p2 - p1) * frac2, p2);
}

// The clip brushes reaching into a leaf, each once a sweep. Kept out of sweepChild: inlined there, the loop made
// every sweep through the tree twice as slow (MSVC), clip brushes or not.
QVR_NOINLINE void clipLeafClips(Sweep& s, za::SizeT key)
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
    const za::SizeT key = static_cast<za::SizeT>(num) * 2 + side;
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
    for(za::U32 i = 0; i < br.count; ++i)
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
    za::SizeT base)
{
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    const double t = glm::dot(n, p) - plane.dist;
    const double offset = za::abs(ext.x * n.x) + za::abs(ext.y * n.y) + za::abs(ext.z * n.z);
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
            const za::SizeT key = static_cast<za::SizeT>(num) * 2 + side;
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
    bool clips = true, za::SizeT base = 0)
{
    ++b.stamp;
    return boxInTree(b, hull, head, p, ext, clips && head == 0 && base == 0 && !b.leafClipStart.empty(), base);
}

// The leaves of the world's hull 0 a box reaches into (touching included): a clip brush's box centres.
void leavesOfBox(const hull_t& hull, int num, const glm::dvec3& lo, const glm::dvec3& hi, za::Vector<int>& out)
{
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    const glm::dvec3 centre = (lo + hi) * 0.5, half = (hi - lo) * 0.5;
    const double t = glm::dot(n, centre) - plane.dist;
    const double reach = za::abs(n.x * half.x) + za::abs(n.y * half.y) + za::abs(n.z * half.z) + 1.0;
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
            out.pushBack(num * 2 + side);
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
    b.hull1Clip.clear();
    b.hull1Clip.resize(static_cast<za::SizeT>(world->numclipnodes) * 2, -2);
    // Each walk item's (walkItems): its clip brushes and their planes, numbered from 0.
    struct Out
    {
        za::Vector<bool> open; // the piece's faces' (a buffer)
        za::Vector<Plane> planes;
        za::Vector<Brush> brushes;
        za::Vector<za::Pair<za::SizeT, bool>> leaves; // (hull 1 leaf key, a clip brush: its next in brushes)
        int hull1Leaves = 0;
        za::SizeT tests = 0; // boxInSolid's (b.stamp's count)
    };
    // (b is only read here: its hull 0 brushes, complete)
    auto onSolid = [&b, &h1, &h0, &sub, head, lift, e32](int num1, int side1, const Poly& piece, Out& out)
    {
        ++out.hull1Leaves;
        za::Vector<bool>& open = out.open;
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
            open.pushBack(isOpen);
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
        for(za::SizeT fi = 0; fi < piece.size() && !clip; ++fi)
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
            for(za::SizeT i = 0; i < f.w.size() && !clip; ++i)
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
            for(za::SizeT i = 0; i < f.w.size() && !clip; ++i)
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
        out.leaves.emplaceBack(static_cast<za::SizeT>(num1) * 2 + side1, clip);
        if(!clip)
        {
            return;
        }
        // Kept in the box centre's space; the open faces shrink back with a narrower box.
        const za::SizeT first = out.planes.size();
        for(za::SizeT fi = 0; fi < piece.size(); ++fi)
        {
            const Face& f = piece[fi];
            const double dist = f.dist + glm::dot(f.normal, lift);
            const double reach =
                za::abs(f.normal.x) * e32.x + za::abs(f.normal.y) * e32.y + za::abs(f.normal.z) * e32.z;
            out.planes.pushBack(
                Plane{glm::vec3{f.normal}, static_cast<float>(open[fi] ? dist - reach : dist), open[fi] ? 1.f : 0.f});
        }
        out.brushes.pushBack(Brush{static_cast<za::U32>(first), static_cast<za::U32>(out.planes.size() - first),
            glm::vec3{lo + lift}, glm::vec3{hi + lift}, true});
    };
    const glm::dvec3 margin{96.0};
    za::Vector<WalkItem> items;
    splitTop(h1, head,
        boxPoly(glm::dvec3{sub.mins[0], sub.mins[1], sub.mins[2]} - margin,
            glm::dvec3{sub.maxs[0], sub.maxs[1], sub.maxs[2]} + margin),
        walkSplitDepth, items);
    za::Vector<Out> outs;
    walkItems(h1, items, outs, onSolid);
    for(const Out& o : outs)
    {
        b.hull1Leaves += o.hull1Leaves;
        b.stamp += o.tests;
        za::SizeT next = 0;
        for(const auto& [key, clip] : o.leaves)
        {
            b.hull1Clip[key] = clip ? static_cast<int>(b.brushes.size()) : -1;
            if(!clip)
            {
                continue;
            }
            Brush br = o.brushes[next++];
            const auto from = o.planes.begin() + br.first;
            br.first = static_cast<za::U32>(b.planes.size());
            b.planes.emplaceBackRange(from, br.count);
            b.brushes.pushBack(br);
            b.clips.pushBack(static_cast<int>(b.brushes.size()) - 1);
        }
    }

    // Each clip brush into the leaves its box centres reach (a sweep's centre passes through them).
    za::Vector<za::Pair<int, int>> pairs; // (leaf key, clip brush)
    za::Vector<int> keys;
    for(const int c : b.clips)
    {
        keys.clear();
        leavesOfBox(h0, 0, glm::dvec3{b.brushes[c].mins}, glm::dvec3{b.brushes[c].maxs}, keys);
        for(const int k : keys)
        {
            pairs.emplaceBack(k, c);
        }
    }
    za::quickSort(pairs.begin(), pairs.end());
    b.leafClipStart.clear();
    b.leafClipStart.resize(static_cast<za::SizeT>(b.numnodes) * 2 + 1, 0);
    b.leafClipList.resize(pairs.size());
    for(za::SizeT i = 0; i < pairs.size(); ++i)
    {
        ++b.leafClipStart[pairs[i].first + 1];
        b.leafClipList[i] = pairs[i].second;
    }
    for(za::SizeT k = 1; k < b.leafClipStart.size(); ++k)
    {
        b.leafClipStart[k] += b.leafClipStart[k - 1];
    }
    b.clipStamp.clear();
    b.clipStamp.resize(b.brushes.size(), 0);
}

struct Result
{
    trace_t trace;
    int brushTests;
};

// A box (mins..maxs about the point) from start to end through a model's brushes, in its own space; Quake's trace.
Result boxTrace(const Brushes& b, const hull_t& hull0, int head, const glm::vec3& start, const glm::vec3& mins,
    const glm::vec3& maxs, const glm::vec3& end, za::SizeT base = 0, const float* halfspace = nullptr)
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
    s.halfspace = halfspace;
    if(head >= 0)
    {
        if(start == end && !halfspace)
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
    const bool endsolid = s.startsolid && s.getout && (halfspace
        ? boxTrace(b, hull0, head, end, mins, maxs, end, base, halfspace).trace.startsolid
        : boxInSolid(b, hull0, head, s.end, s.ext, true, base));
    const bool allsolid = s.startsolid && (!s.getout || endsolid);
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

// A table's planes by planeKey(dist), each key's in the order they were added.
using PlaneIndex = ankerl::unordered_dense::map<long long, za::Vector<int>>;
long long planeKey(float d)
{
    return static_cast<long long>(za::floor(d * 4.f));
}

struct Tree
{
    za::Vector<mclipnode_t> nodes;
    za::Vector<mplane_t> planes;
    za::Vector<int> heads;                    // [sub]: its tree's root in nodes; -1 not built yet
    // Its planes' index (TreeBuilder), kept from one model's build to the next: the first `indexed` planes are in it. Made
    // again for each model's build it was most of a big map's allocations (ad_grendel: 5.2 million of 8.5, its 440 brush
    // models times two trees, each indexing all the tree's planes again).
    PlaneIndex index;
    za::SizeT indexed = 0;
    // What a load of the same map gets back (keepForReload): the tree as the load left it (its world's tree, and the
    // models prepared with the map: checkpoint), the models' builds after it let go.
    za::SizeT keptNodes = 0, keptPlanes = 0;
    int keptSolid = 0, keptEmpty = 0;
    za::Vector<int> keptHeads; // (empty: nothing to keep)
    const mclipnode_t* forClipnodes = nullptr; // the Brushes it was built from (their world's hull 0)
    glm::vec3 ext{0.f};                        // the half size of the box it was built for
    int redone = 0;                            // pieces of its builds on the pool done again on one thread (buildTree)
    double ms = 0.0;                           // the builds so far
    int solidLeaves = 0, emptyLeaves = 0;
    bool fromDisk = false;                     // the world's tree read from the disk cache (vr_hull_cache)
    auto members()
    {
        return qvr::mem::list(nodes, planes, heads, index, indexed, keptNodes, keptPlanes, keptSolid, keptEmpty, keptHeads,
            forClipnodes, ext, ms, solidLeaves, emptyLeaves, redone, fromDisk);
    }
};
mem::Cache<Tree> tree{"hull tree", mem::MapChange};

// Its bytes, for a set holding trees (found by the set's heldBytes of a vector of them).
za::SizeT heldBytes(const Tree& t)
{
    return mem::heldBytes(t.nodes) + mem::heldBytes(t.planes) + mem::heldBytes(t.heads) + mem::heldBytes(t.index) +
           mem::heldBytes(t.keptHeads);
}

// Monsters' trees (vr_mhull): one per box size their widths ask for (a few: the widths come from the classes'
// settings); the player's size shares the player's tree.
struct MonsterTrees
{
    za::Vector<Tree> trees;
    auto members() { return qvr::mem::list(trees); }
};
mem::Cache<MonsterTrees> monsterTrees{"hull monster trees", mem::MapChange};
constexpr za::SizeT maxMonsterTrees = 128; // reserved before asynchronous builds: their Tree pointers remain stable
cvar_t vr_hull_audit = {"vr_hull_audit", "0", CVAR_NONE};
struct HullAudit
{
    za::U64 slots = 0, clears = 0, runtimeBuilds = 0;
};
HullAudit hullAudit;

// ---------------------------------------------------------------------------------------------------------------
// Kept for a reload (vr_hull_keep; PROFILING_2026-10.md, decision 1): at a map change the map's brushes and trees are
// kept, under the world's content (keyOf), for the next load of a map with the same (a death's reload, restart, a
// save's load, a changelevel back). They are the load's own results: the brushes of the world's models (external
// models' let go), each tree as the load left it (checkpoint: its world's tree and the models prepared with the map;
// models compiled later let go), so a load that gets them back has what a load that builds them makes. Their pointers
// into the hunk (the world's hull 0) are cleared while kept and set to the new world's when taken back. The trees are
// kept by box size: a width changed meanwhile is compiled again (its slot's size does not match), the others reused.

// The tree as it is now: what a reload gets back (the load's builds done: its world's tree, then the prepared models).
void checkpoint(Tree& t)
{
    t.keptNodes = t.nodes.size();
    t.keptPlanes = t.planes.size();
    t.keptSolid = t.solidLeaves;
    t.keptEmpty = t.emptyLeaves;
    t.keptHeads = t.heads;
}

bool keepable(const Tree& t)
{
    return !t.keptHeads.empty() && t.keptHeads[0] >= 0 && t.keptNodes <= t.nodes.size() && t.keptPlanes <= t.planes.size();
}

// Back to its checkpoint, its pointer cleared (kept).
void cutBack(Tree& t)
{
    if(t.indexed == t.planes.size())
    {
        for(za::SizeT i = t.planes.size(); i-- > t.keptPlanes;)
        {
            const auto found = t.index.find(planeKey(t.planes[i].dist));
            if(found != t.index.end() && !found->second.empty())
            {
                found->second.popBack(); // (the plane added last is its key's last)
            }
        }
        t.indexed = t.keptPlanes;
    }
    else
    {
        t.index.clear();
        t.indexed = 0;
    }
    t.planes.resize(t.keptPlanes);
    t.nodes.resize(t.keptNodes);
    t.heads = t.keptHeads;
    t.solidLeaves = t.keptSolid;
    t.emptyLeaves = t.keptEmpty;
    t.forClipnodes = nullptr;
}

// The world's own brushes (external models' let go), its pointers cleared (kept).
void cutBack(Brushes& b)
{
    b.subs.resize(b.worldSubs);
    b.planes.resize(b.worldPlanes);
    b.brushes.resize(b.worldBrushes);
    b.leafBrush.resize(b.worldLeafBrush);
    b.bevels = b.worldBevels;
    b.dropped = b.worldDropped;
    b.clipnodes = nullptr;
    for(SubModel& sm : b.subs)
    {
        sm.clipnodes = nullptr;
    }
}

struct KeptMap
{
    za::U64 key = 0;
    Brushes brushes;
    Tree player;                // (its heads empty: none)
    za::Vector<Tree> monsters;
};

za::SizeT heldBytes(const KeptMap& k)
{
    return heldBytes(k.brushes) + heldBytes(k.player) + mem::heldBytes(k.monsters);
}

struct KeptMaps
{
    za::Vector<KeptMap> maps; // the most recent last
    za::U64 hits = 0, misses = 0; // loads that got theirs back, loads that built them
    auto members() { return qvr::mem::list(maps, hits, misses); }
};
// Kept across game folder changes too (a map package mounted and let go): the key is the map's content, and nothing
// in them points anywhere while kept. Only counted (vr_memstats); vr_hull_keep bounds it.
mem::Cache<KeptMaps> kept{"hull kept maps", mem::Never};

za::SizeT keepLimit()
{
    return static_cast<za::SizeT>(za::clamp(static_cast<int>(vr_hull_keep.value), 0, 8));
}

// The oldest let go, past the setting's count.
void trimKept()
{
    za::Vector<KeptMap>& maps = kept.maps;
    const za::SizeT limit = keepLimit();
    if(maps.size() > limit)
    {
        maps.erase(maps.begin(), maps.begin() + (maps.size() - limit));
    }
}

void onKeepChanged(cvar_t*)
{
    if(keepLimit() == 0)
    {
        mem::release(kept.maps);
    }
    else
    {
        trimKept();
    }
}

// The kept map with this content made the server's (its pointers the new world's); false: none.
bool takeKept(const qmodel_t* world, za::U64 key)
{
    za::Vector<KeptMap>& maps = kept.maps;
    KeptMap* found = nullptr;
    for(KeptMap& k : maps)
    {
        found = k.key == key ? &k : found;
    }
    if(!found || keepLimit() == 0)
    {
        ++kept.misses;
        return false;
    }
    KeptMap k = ZA_MOVE(*found);
    maps.erase(found);
    const mclipnode_t* clipnodes = world->hulls[0].clipnodes;
    Brushes& b = built;
    b = ZA_MOVE(k.brushes);
    b.clipnodes = clipnodes;
    for(SubModel& sm : b.subs)
    {
        sm.clipnodes = clipnodes;
    }
    b.modelSub.clear();
    b.modelSub.resize(MAX_MODELS, -2); // (looked up again: the models' slots are the new load's)
    if(!k.player.heads.empty())
    {
        Tree& p = tree;
        p = ZA_MOVE(k.player);
        p.forClipnodes = clipnodes;
    }
    za::Vector<Tree>& v = monsterTrees.trees;
    v.clear();
    v.reserve(maxMonsterTrees);
    for(Tree& t : k.monsters)
    {
        v.pushBack(ZA_MOVE(t));
        v.back().forClipnodes = clipnodes;
    }
    ++kept.hits;
    return true;
}

// A piece of a grown brush in a node of the tree being built.
struct Frag
{
    Poly poly;
    glm::dvec3 lo, hi;
    int live = 0; // its faces (a winding left) on planes not yet split on: none, and it fills its node's space
    const Brush* brush = nullptr; // the brush it is a piece of
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(za::isTriviallyRelocatable<Poly>);
};
// A node's pieces, in place up to 4 (PROFILING_2026-10.md, "Hull build: the containers": a split's sides hold 3 at the
// median, 4.4-6.1 on average; in place, a node's two sides cost no allocation two times in three).
using Frags = za::SmallVector<Frag, 4>;

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
    // The builder of a tree: its planes and nodes go straight into t, its planes' index kept in t (made up to date here:
    // the planes added since, or all of them again if the table is shorter than the index, e.g. emptied).
    explicit TreeBuilder(Tree& t)
        : planes_{&t.planes}, nodes_{&t.nodes}, solid_{&t.solidLeaves}, empty_{&t.emptyLeaves}, ext_{t.ext},
          index_{&t.index}, indexed_{&t.indexed}
    {
        if(t.indexed > planes_->size())
        {
            t.index.clear();
            t.indexed = 0;
        }
        for(za::SizeT i = t.indexed; i < planes_->size(); ++i)
        {
            (*index_)[key((*planes_)[i].dist)].pushBack(static_cast<int>(i));
        }
        t.indexed = planes_->size();
    }

    // A builder on the pool over `base` (its table as it is now, only read: base does not change while this one works):
    // the planes it adds numbered after base's, its nodes from 0, its leaves counted in solid and empty, and every plane
    // it asks for logged.
    TreeBuilder(const TreeBuilder& base, za::Vector<mplane_t>& planes, za::Vector<mclipnode_t>& nodes, int& solid,
        int& empty, za::Vector<PlaneAsk>& log)
        : base_{&base}, baseCount_{base.count()}, planes_{&planes}, nodes_{&nodes}, solid_{&solid}, empty_{&empty},
          ext_{base.ext_}, log_{&log}
    {
    }

    TreeBuilder(const TreeBuilder&) = delete;
    TreeBuilder& operator=(const TreeBuilder&) = delete;

    // The table's planes (base's, then this one's).
    [[nodiscard]] za::SizeT count() const { return baseCount_ + planes_->size(); }
    [[nodiscard]] const mplane_t& planeAt(int i) const
    {
        const auto u = static_cast<za::SizeT>(i);
        return u < baseCount_ ? base_->planeAt(i) : (*planes_)[u - baseCount_];
    }

    // The table's plane for n, d (either way round: the table's faces its larger axis positive, as qbsp's do).
    int plane(glm::dvec3 n, double d)
    {
        const glm::dvec3 askedN = n;
        const double askedD = d;
        for(int a = 0; a < 3; ++a)
        {
            if(za::abs(n[a]) > 1.0 - 1e-6)
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
            planes_->pushBack(p);
            id = static_cast<int>(count()) - 1;
            (*index_)[k].pushBack(id);
            if(indexed_)
            {
                *indexed_ = planes_->size();
            }
        }
        if(log_)
        {
            log_->pushBack(PlaneAsk{askedN, askedD, id});
        }
        return id;
    }

    // (A builder on the pool) the asked plane answered as `as` (the table's plane of those very values, added if none):
    // logged as asked.
    int planeAs(const glm::dvec3& n, double d, const mplane_t& as)
    {
        const long long k = key(as.dist);
        int id = -1;
        for(long long kk = k - 1; kk <= k + 1 && id < 0; ++kk)
        {
            id = findSame(kk, as);
        }
        if(id < 0)
        {
            planes_->pushBack(as);
            id = static_cast<int>(count()) - 1;
            (*index_)[k].pushBack(id);
        }
        log_->pushBack(PlaneAsk{n, d, id});
        return id;
    }

    // The planes added since the table had `to` let go (the last first; the tree's builder only).
    void rollback(za::SizeT to)
    {
        while(planes_->size() > to)
        {
            (*index_)[key(planes_->back().dist)].popBack(); // (the plane added last is its key's last)
            planes_->popBack();
        }
        if(indexed_)
        {
            *indexed_ = planes_->size();
        }
    }

    // A brush grown by the box as a piece (false: nothing left of it).
    // given (growAllOnPool): the planes the table will answer for its brush's planes, found beforehand (a builder on
    // the pool takes those for its cuts, so they are the build on one thread's).
    bool grow(const Brushes& b, const Brush& br, const glm::dvec3& ext, Frag& out, const mplane_t* given = nullptr)
    {
        const glm::dvec3 pad = br.clip ? glm::dvec3{2.0} : ext + 2.0;
        Poly p = boxPoly(glm::dvec3{br.mins} - pad, glm::dvec3{br.maxs} + pad), front, back;
        for(za::U32 i = 0; i < br.count; ++i)
        {
            const Plane& q = b.planes[br.first + i];
            const glm::dvec3 n0{q.normal};
            const int tag = given ? planeAs(n0, q.dist + support(q, ext), given[i]) : plane(n0, q.dist + support(q, ext));
            glm::dvec3 n;
            double d;
            oriented(tag, n0, n, d);
            splitPoly(ZA_MOVE(p), n, d, front, back);
            if(back.empty())
            {
                return false;
            }
            if(!front.empty())
            {
                back.back().tag = tag; // the cut's face
            }
            p = ZA_MOVE(back);
            back = Poly{};
        }
        for(Face& f : p)
        {
            if(f.tag < 0 && !f.w.empty()) // the starting box's (only if a brush had no axial bevel there)
            {
                f.tag = plane(f.normal, f.dist);
            }
        }
        out.poly = ZA_MOVE(p);
        out.brush = &br;
        return finish(out, -1) && bounded(out);
    }

    // Debugging (vr_hull_leafdebug): the leaf holding this point is described as it is made.
    const glm::dvec3* watch = nullptr;
    const Brushes* debugBrushes = nullptr;
    int rebounded = 0; // pieces cut back to their brush's bounds (bounded)

    // The pieces' leaf: its contents (counted), or 0 when they need a node.
    int leaf(const Frags& frags)
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
    int split(Frags& frags, Frags (&sides)[2], glm::dvec3& n, double& d)
    {
        const int split = choose(frags);
        const mplane_t& mp = planeAt(split);
        n = glm::dvec3{mp.normal[0], mp.normal[1], mp.normal[2]};
        d = mp.dist;
        Poly parts[2];
        for(Frag& f : frags)
        {
            splitPoly(ZA_MOVE(f.poly), n, d, parts[0], parts[1]);
            for(int side = 0; side < 2; ++side)
            {
                if(parts[side].empty())
                {
                    continue;
                }
                Frag g;
                g.brush = f.brush;
                g.poly = ZA_MOVE(parts[side]);
                parts[side] = Poly{};
                if(finish(g, split, n, d, side ? -1.0 : 1.0) && bounded(g))
                {
                    sides[side].pushBack(ZA_MOVE(g));
                }
            }
        }
        frags.clear();
        frags.shrinkToFit();
        return split;
    }

    // The tree of the pieces; its root (a node, or a leaf's contents). region: the node's space (only when watching).
    int build(Frags& frags, Poly* region = nullptr)
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
        Frags sides[2];
        glm::dvec3 n;
        double d;
        const int split = this->split(frags, sides, n, d);
        Poly regions[2];
        if(watched)
        {
            splitPoly(ZA_MOVE(*region), n, d, regions[0], regions[1]);
        }
        const int node = static_cast<int>(nodes_->size());
        nodes_->pushBack(mclipnode_t{split, {0, 0}});
        const int front = build(sides[0], watched ? &regions[0] : nullptr);
        const int back = build(sides[1], watched ? &regions[1] : nullptr);
        (*nodes_)[static_cast<za::SizeT>(node)].children[0] = front;
        (*nodes_)[static_cast<za::SizeT>(node)].children[1] = back;
        return node;
    }

    // Its leaf counts added (a merged piece of the build's).
    void addLeaves(int solid, int empty)
    {
        *solid_ += solid;
        *empty_ += empty;
    }

    [[nodiscard]] za::Vector<mclipnode_t>& nodes() { return *nodes_; }

private:
    static long long key(float d) { return planeKey(d); }

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
        const auto found = index_->find(kk);
        if(found == index_->end())
        {
            return -1;
        }
        for(const int id : found->second)
        {
            const mplane_t& p = planeAt(id);
            // qbsp's epsilons (a normal's components, not their dot: far from the origin a small turn is far off)
            if(za::abs(p.dist - d) < 0.01 && za::abs(p.normal[0] - n.x) < 1e-5 && za::abs(p.normal[1] - n.y) < 1e-5 &&
                za::abs(p.normal[2] - n.z) < 1e-5)
            {
                return id;
            }
        }
        return -1;
    }

    // The first plane of the key's with these values (base's first), else -1.
    int findSame(long long kk, const mplane_t& as) const
    {
        if(base_)
        {
            if(const int id = base_->findSame(kk, as); id >= 0)
            {
                return id;
            }
        }
        const auto found = index_->find(kk);
        if(found == index_->end())
        {
            return -1;
        }
        for(const int id : found->second)
        {
            const mplane_t& p = planeAt(id);
            if(p.normal[0] == as.normal[0] && p.normal[1] == as.normal[1] && p.normal[2] == as.normal[2] &&
                p.dist == as.dist && p.type == as.type && p.signbits == as.signbits)
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
            for(za::U32 i = 0; i < br.count; ++i)
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
                splitPoly(ZA_MOVE(f.poly), n, d, front, back);
                if(back.empty())
                {
                    return false;
                }
                if(!front.empty())
                {
                    back.back().tag = tag;
                }
                f.poly = ZA_MOVE(back);
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
                reach = za::max(reach, (glm::dot(n, v) - d) * sign);
            }
            f.live += face.tag >= 0; // (a face lost to the epsilon too: its plane still bounds the piece)
        }
        return f.lo.x <= f.hi.x && f.hi.x - f.lo.x >= 0.01 && f.hi.y - f.lo.y >= 0.01 && f.hi.z - f.lo.z >= 0.01 &&
               (sign == 0.0 || reach >= 0.01);
    }

    // qbsp's choice (qbsp3's SelectSplitSide, on the pieces' bounds): the plane that most pieces lie on and that splits
    // the fewest, balanced, axial first. Many pieces: a sample of the planes (the build's time).
    int choose(const Frags& frags)
    {
        // The pieces' live faces' planes (in the order first met) and how many faces lie on each: counted in a table
        // the size of the faces (open addressed), not of the tree's planes (an array of all the planes a builder was
        // 5 MB on vrstart, made and zeroed by each of the pool's 2000 builders a tree: a third of its build).
        za::SizeT faces = 0;
        for(const Frag& f : frags)
        {
            faces += f.poly.size();
        }
        za::SizeT size = 16;
        while(size < faces * 2)
        {
            size *= 2;
        }
        const za::SizeT mask = size - 1;
        slots_.clear();
        slots_.resize(size, -1);
        cands_.clear();
        for(const Frag& f : frags)
        {
            for(const Face& face : f.poly)
            {
                if(face.tag < 0)
                {
                    continue;
                }
                za::SizeT i = (static_cast<za::SizeT>(static_cast<za::U32>(face.tag)) * 0x9E3779B97F4A7C15ull >> 20) & mask;
                while(slots_[i] >= 0 && cands_[static_cast<za::SizeT>(slots_[i])].tag != face.tag)
                {
                    i = (i + 1) & mask;
                }
                if(slots_[i] < 0)
                {
                    slots_[i] = static_cast<int>(cands_.size());
                    cands_.pushBack(Cand{face.tag, 0});
                }
                ++cands_[static_cast<za::SizeT>(slots_[i])].facing;
            }
        }
        za::stablePartition(cands_.begin(), cands_.end(),
            [this](const Cand& c) { return planeAt(c.tag).type < 3; });
        const za::SizeT step = za::max<za::SizeT>(1, cands_.size() * frags.size() / chooseBudget);
        // The pieces' bounds, their centres and half sizes, an array per axis (the planes are weighed against them in
        // turn: read in a row, not out of the pieces themselves; the same numbers as from the pieces).
        const za::SizeT nf = frags.size();
        bounds_.resize(nf * 12);
        double* const box = bounds_.data();
        for(za::SizeT i = 0; i < nf; ++i)
        {
            const Frag& f = frags[i];
            for(int a = 0; a < 3; ++a)
            {
                box[(0 + a) * nf + i] = f.lo[a];
                box[(3 + a) * nf + i] = f.hi[a];
                box[(6 + a) * nf + i] = (f.lo[a] + f.hi[a]) * 0.5;
                box[(9 + a) * nf + i] = (f.hi[a] - f.lo[a]) * 0.5;
            }
        }
        int best = cands_.front().tag;
        long long bestValue = LLONG_MIN;
        for(za::SizeT ci = 0; ci < cands_.size(); ci += step)
        {
            const int c = cands_[ci].tag;
            const mplane_t& p = planeAt(c);
            const glm::dvec3 n{p.normal[0], p.normal[1], p.normal[2]};
            const double dist = p.dist;
            int front = 0, back = 0;
            if(p.type < 3)
            {
                const double* const lo = box + (0 + p.type) * nf;
                const double* const hi = box + (3 + p.type) * nf;
                for(za::SizeT i = 0; i < nf; ++i)
                {
                    const bool isBack = hi[i] - dist <= onEpsilon;
                    back += isBack;
                    front += !isBack && lo[i] - dist >= -onEpsilon;
                }
            }
            else
            {
                const double ax = za::abs(n.x), ay = za::abs(n.y), az = za::abs(n.z);
                const double *const cx = box + 6 * nf, *const cy = box + 7 * nf, *const cz = box + 8 * nf;
                const double *const hx = box + 9 * nf, *const hy = box + 10 * nf, *const hz = box + 11 * nf;
                for(za::SizeT i = 0; i < nf; ++i)
                {
                    const double s = n.x * cx[i] + n.y * cy[i] + n.z * cz[i] - dist; // (glm::dot's order)
                    const double r = ax * hx[i] + ay * hy[i] + az * hz[i];
                    const bool isBack = s + r <= onEpsilon;
                    back += isBack;
                    front += !isBack && s - r >= -onEpsilon;
                }
            }
            const int splits = static_cast<int>(nf) - front - back;
            const long long value = 5ll * cands_[ci].facing - 5ll * splits - za::abs(front - back) +
                                    (p.type < 3 ? 5 : 0);
            if(value > bestValue)
            {
                bestValue = value;
                best = c;
            }
        }
        return best;
    }

    static constexpr za::SizeT chooseBudget = 40000; // planes x pieces weighed at a node, at most

    const TreeBuilder* base_ = nullptr; // (a builder on the pool) the table it adds to
    za::SizeT baseCount_ = 0;         // base's planes
    za::Vector<mplane_t>* planes_;     // its planes (after base's)
    za::Vector<mclipnode_t>* nodes_;
    int* solid_;
    int* empty_;
    glm::vec3 ext_;                     // the box's half size
    za::Vector<PlaneAsk>* log_ = nullptr;
    PlaneIndex ownIndex_;                // (a builder on the pool) its own planes' index
    PlaneIndex* index_ = &ownIndex_;     // its planes by key(dist): the tree's, or its own
    za::SizeT* indexed_ = nullptr;       // (the tree's builder) how many of the tree's planes its index holds
    struct Cand
    {
        int tag;    // the plane
        int facing; // the faces on it
    };
    za::Vector<int> slots_; // (choose) its table: an index in cands_, or -1
    za::Vector<Cand> cands_;
    za::Vector<double> bounds_; // (choose) the pieces' bounds by axis: lo, hi, centre, half size
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
constexpr za::SizeT shareMin = 48;   // fewer pieces: a subtree of their own (not split further here)

struct Unit
{
    const Unit* parent = nullptr;    // the unit whose split made its pieces (none: the tree's own table is its base)
    za::SizeT baseCount = 0;       // the planes it saw (its base's)
    za::Vector<mplane_t> planes;    // those it added (numbered from baseCount)
    za::Vector<PlaneAsk> log;
    za::Vector<int> map;            // its planes' numbers in the tree's table (the merge)
    int solid = 0, empty = 0, rebounded = 0;
    // A node's: a leaf (kind 0: root its contents), a split (kind 1: split, kids), a subtree (kind 2: root and nodes,
    // numbered from 0); input: its pieces (done again from them if the merge's check fails).
    int kind = 0;
    int root = 0;
    int split = 0;
    za::UniquePtr<Unit> kids[2]{nullptr, nullptr};
    za::Vector<mclipnode_t> nodes;
    Frags input;
};

// The merge's state: which unit's plane each plane added to the tree's table since `start` is.
struct Merge
{
    za::SizeT start = 0;
    za::Vector<const Unit*> owner; // [plane - start]
    int redone = 0;                 // units done again on the merging thread

    // A plane number of u's (or of the units above it) in the tree's table.
    [[nodiscard]] int real(const Unit& u, int id) const
    {
        for(const Unit* x = &u; x; x = x->parent)
        {
            if(static_cast<za::SizeT>(id) >= x->baseCount)
            {
                return x->map[static_cast<za::SizeT>(id) - x->baseCount];
            }
        }
        return id;
    }

    [[nodiscard]] bool mine(int id, const Unit& u) const
    {
        const za::SizeT i = static_cast<za::SizeT>(id) - start;
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

    void own(za::SizeT from, za::SizeT to, const Unit& u)
    {
        owner.resize(za::max(owner.size(), to - start), nullptr);
        for(za::SizeT i = from; i < to; ++i)
        {
            owner[i - start] = &u;
        }
    }

    static bool same(const mplane_t& a, const mplane_t& b)
    {
        return ZA_MEMCMP(a.normal, b.normal, sizeof(a.normal)) == 0 && ZA_MEMCMP(&a.dist, &b.dist, sizeof(a.dist)) == 0 &&
               a.type == b.type && a.signbits == b.signbits;
    }

    // u's asks put to the tree's table (tb) in turn: whether every answer is the plane u had.
    bool adopt(TreeBuilder& tb, Unit& u)
    {
        u.map.clear();
        u.map.resize(u.planes.size(), -1);
        for(const PlaneAsk& a : u.log)
        {
            const int got = tb.plane(a.n, a.d);
            const auto id = static_cast<za::SizeT>(a.id);
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
            if(static_cast<za::SizeT>(got) < start || mine(got, u) || !same(tb.planeAt(got), u.planes[id - u.baseCount]))
            {
                return false;
            }
            to = got;
            own(static_cast<za::SizeT>(got), static_cast<za::SizeT>(got) + 1, u);
        }
        return true;
    }

    // What u's asks added taken out again, before it is done on the merging thread.
    void undo(TreeBuilder& tb, za::SizeT to)
    {
        tb.rollback(to);
        owner.resize(za::min(owner.size(), to - start));
    }
};

jobs::Site speculateSite{"hull speculate"}; // (its parallelFor: vr_jobs_sites)

// A unit of the tree's top (its pieces), over base; its sides' units at once below it.
void speculate(Unit& u, const TreeBuilder& base, Frags&& frags, int depth)
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
        Frags sides[2];
        glm::dvec3 n;
        double d;
        u.split = tb.split(frags, sides, n, d);
        for(int side = 0; side < 2; ++side)
        {
            u.kids[side] = za::makeUnique<Unit>();
            u.kids[side]->parent = &u;
        }
        jobs::parallelFor(speculateSite, 2, 1,
            [&](za::SizeT begin, za::SizeT end)
            {
                for(za::SizeT i = begin; i < end; ++i)
                {
                    speculate(*u.kids[i], tb, ZA_MOVE(sides[i]), depth - 1);
                }
            });
    }
    u.rebounded = tb.rebounded;
}

// u's nodes into the tree (in the order the build on one thread makes them); its root.
int emit(TreeBuilder& tb, Unit& u, Merge& m)
{
    const za::SizeT saved = tb.count();
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
    za::Vector<mclipnode_t>& nodes = tb.nodes();
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
            nodes.pushBack(n);
        }
        return u.root >= 0 ? u.root + offset : u.root;
    }
    const int node = static_cast<int>(nodes.size());
    nodes.pushBack(mclipnode_t{m.real(u, u.split), {0, 0}});
    const int front = emit(tb, *u.kids[0], m);
    const int back = emit(tb, *u.kids[1], m);
    nodes[static_cast<za::SizeT>(node)].children[0] = front;
    nodes[static_cast<za::SizeT>(node)].children[1] = back;
    return node;
}

jobs::Site growSite{"hull grow"}; // (its parallelFor: vr_jobs_sites)

// The brushes grown by the box into pieces (in list's order, into frags) as tb.grow on one thread grows them, but on the
// pool: runs of growRun brushes, each run grown by a builder over tb's table as it is (only read meanwhile), with planes
// of its own and every ask logged. Then, brush by brush in list's order, its asks are put to tb's table as on one thread:
// if every answer is a plane of the same values as the run's answer (so its cuts were the same), its piece is taken, its
// faces' planes renumbered to the table's; else (a nearly equal plane added before it by a brush the run did not see)
// what its asks added is taken back out and the brush grown again here. vrstart's 33k brushes a tree: 1.5 s on one
// thread.
constexpr za::SizeT growRun = 256;

// The same plane in value (a zero's sign aside: a plane first asked for facing the other way is stored negated, -0 in
// its zero components; every cut made with either is the same).
bool sameValues(const mplane_t& a, const mplane_t& b)
{
    return a.normal[0] == b.normal[0] && a.normal[1] == b.normal[1] && a.normal[2] == b.normal[2] && a.dist == b.dist &&
           a.type == b.type && a.signbits == b.signbits;
}

void growAllOnPool(TreeBuilder& tb, const Brushes& b, const za::Vector<const Brush*>& list, const glm::dvec3& ext,
    Frags& frags)
{
    struct Run
    {
        za::Vector<mplane_t> planes;
        za::Vector<mclipnode_t> nodes; // (unused: a builder's)
        int solid = 0, empty = 0;
        za::Vector<PlaneAsk> log;
        Frags frags;
        za::Vector<za::U32> logStart; // [brush of the run]: its first ask in log (and one past the last's)
        za::Vector<int> fragOf;       // [brush of the run]: its piece in frags, or -1 (nothing left of it)
        za::Vector<int> rebounded;    // [brush of the run]: its pieces cut back (bounded)
        za::Vector<int> map;          // its planes' numbers in tb's table (-1: not yet)
    };
    const za::SizeT baseCount = tb.count();
    // The planes the table gives the brushes' own planes (the most of its asks), asked in list's order of a builder over
    // tb, as on one thread: the runs cut with these, so a brush's cuts are mostly the build on one thread's even where a
    // brush before it, in another run, added a nearly equal plane first (most of vrstart's brushes: its terrain's
    // prisms share their sides' planes to within the table's epsilons).
    za::Vector<za::U32> givenStart;
    za::Vector<mplane_t> given;
    {
        za::Vector<mplane_t> planes;
        za::Vector<mclipnode_t> nodes;
        int solid = 0, empty = 0;
        za::Vector<PlaneAsk> log;
        TreeBuilder pb{tb, planes, nodes, solid, empty, log};
        givenStart.reserve(list.size() + 1);
        for(const Brush* br : list)
        {
            givenStart.pushBack(static_cast<za::U32>(given.size()));
            for(za::U32 i = 0; i < br->count; ++i)
            {
                const Plane& q = b.planes[br->first + i];
                given.pushBack(pb.planeAt(pb.plane(glm::dvec3{q.normal}, q.dist + support(q, ext))));
            }
        }
        givenStart.pushBack(static_cast<za::U32>(given.size()));
    }
    za::Vector<Run> runs((list.size() + growRun - 1) / growRun);
    jobs::parallelFor(growSite, runs.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT r = begin; r < end; ++r)
            {
                Run& run = runs[r];
                TreeBuilder lb{tb, run.planes, run.nodes, run.solid, run.empty, run.log};
                const za::SizeT last = za::min(list.size(), (r + 1) * growRun);
                for(za::SizeT i = r * growRun; i < last; ++i)
                {
                    run.logStart.pushBack(static_cast<za::U32>(run.log.size()));
                    const int before = lb.rebounded;
                    Frag f;
                    if(lb.grow(b, *list[i], ext, f, given.data() + givenStart[i]))
                    {
                        run.fragOf.pushBack(static_cast<int>(run.frags.size()));
                        run.frags.pushBack(ZA_MOVE(f));
                    }
                    else
                    {
                        run.fragOf.pushBack(-1);
                    }
                    run.rebounded.pushBack(lb.rebounded - before);
                }
                run.logStart.pushBack(static_cast<za::U32>(run.log.size()));
            }
        });
    za::Vector<za::SizeT> touched; // (a brush's) the run's planes it gave numbers to
    for(za::SizeT r = 0; r < runs.size(); ++r)
    {
        Run& run = runs[r];
        run.map.clear();
        run.map.resize(run.planes.size(), -1);
        for(za::SizeT k = 0; k + 1 < run.logStart.size(); ++k)
        {
            const za::SizeT saved = tb.count();
            touched.clear();
            bool same = true;
            for(za::U32 q = run.logStart[k]; q < run.logStart[k + 1] && same; ++q)
            {
                const PlaneAsk& a = run.log[q];
                const int got = tb.plane(a.n, a.d);
                const auto id = static_cast<za::SizeT>(a.id);
                const mplane_t& had = id < baseCount ? tb.planeAt(a.id) : run.planes[id - baseCount];
                same = sameValues(tb.planeAt(got), had);
                if(same && id >= baseCount)
                {
                    int& to = run.map[id - baseCount];
                    if(to < 0)
                    {
                        to = got;
                        touched.pushBack(id - baseCount);
                    }
                    same = to == got;
                }
            }
            const Brush& br = *list[r * growRun + k];
            if(!same)
            {
                tb.rollback(saved);
                for(const za::SizeT p : touched)
                {
                    run.map[p] = -1;
                }
                Frag f;
                if(tb.grow(b, br, ext, f))
                {
                    frags.pushBack(ZA_MOVE(f));
                }
                continue;
            }
            tb.rebounded += run.rebounded[k];
            if(run.fragOf[k] < 0)
            {
                continue;
            }
            Frag& f = run.frags[static_cast<za::SizeT>(run.fragOf[k])];
            for(Face& face : f.poly)
            {
                if(face.tag >= 0 && static_cast<za::SizeT>(face.tag) >= baseCount)
                {
                    face.tag = run.map[static_cast<za::SizeT>(face.tag) - baseCount];
                }
            }
            frags.pushBack(ZA_MOVE(f));
        }
        mem::release(run.frags);
    }
}

// The tree of one model (sub), built into t (its root: a node; a lone leaf gets a node of its own). Only t is written:
// trees for different boxes are built at once on the pool (report false there: the pieces cut back are returned, for
// the main thread to print). On the pool (vr_jobs_parallel) unless watching (vr_hull_leafdebug).
int buildTree(Tree& t, const Brushes& b, za::SizeT sub, const glm::dvec3* watch = nullptr, bool report = true)
{
    const auto t0 = za::Clock::nowNanoseconds();
    TreeBuilder tb{t};
    tb.watch = watch;
    tb.debugBrushes = &b;
    const glm::dvec3 ext{t.ext};
    za::Vector<const Brush*> list;
    const SubModel& sm = b.subs[sub];
    for(za::U32 i = 0; i < sm.numBrushes; ++i)
    {
        list.pushBack(&b.brushes[sm.firstBrush + i]);
    }
    if(sub == 0)
    {
        for(const int c : b.clips)
        {
            list.pushBack(&b.brushes[static_cast<za::SizeT>(c)]);
        }
    }
    Frags frags;
    auto growAll = [&]
    {
        for(const Brush* br : list)
        {
            Frag f;
            if(tb.grow(b, *br, ext, f))
            {
                frags.pushBack(ZA_MOVE(f));
            }
        }
    };
    int root = 0;
    if(!watch && jobs::parallel() && jobs::pool())
    {
        Merge m;
        growAllOnPool(tb, b, list, ext, frags);
        Unit top;
        speculate(top, tb, ZA_MOVE(frags), shareDepth);
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
        t.nodes.pushBack(mclipnode_t{tb.plane(glm::dvec3{0.0, 0.0, 1.0}, 0.0), {root, root}});
        root = node;
    }
    t.heads[sub] = root;
    if(sub == 0)
    {
        checkpoint(t); // (the prepared models' builds move it on: prepareBrushModels)
    }
    if(tb.rebounded && report)
    {
        Con_DPrintf("hull: %d pieces cut back to their brushes' bounds (%gx%g, model %d)\n", tb.rebounded, t.ext.x * 2.f,
            t.ext.z * 2.f, static_cast<int>(sub));
    }
    t.ms += za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0);
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
    const za::PtrDiffT bytes = reinterpret_cast<const byte*>(ent) - reinterpret_cast<const byte*>(qcvm->edicts);
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

// Crouching (vr_crouch_hull; HULLS.md "Crouching"): a client whose eyes are low has a box only as tall from his feet as
// the lowest of the crouched heights his eyes are under (vr_crouch_height, then every vr_crouch_step up to Quake's 56
// less 4), against the map, bodies and shots; he keeps it, standing up, until a taller box fits where he is. Indexed by
// client number (1..maxclients): the box's height, 0 standing; cleared at each map's load.
constexpr int crouchSlots = MAX_SCOREBOARD + 1;
constexpr int maxCrouchLevels = 8;
constexpr float crouchStandMargin = 2.f; // units his eyes rise past a box's top before a taller one is tried
float crouchBox[crouchSlots]{};

bool crouchOn()
{
    return vr_crouch_hull.value != 0.f;
}

// The crouched heights, lowest first: vr_crouch_height (24 to hull 1's height less 4), then every vr_crouch_step (0:
// that one alone). Returns how many (at least 1, at most maxCrouchLevels).
int crouchLevels(const qmodel_t* world, float* out)
{
    const float most = hull1Height(world) - 4.f;
    const float lowest = za::clamp(vr_crouch_height.value, 24.f, most);
    const float step = vr_crouch_step.value >= 4.f ? vr_crouch_step.value : 0.f;
    int n = 0;
    out[n++] = lowest;
    while(step > 0.f && n < maxCrouchLevels && out[n - 1] + step <= most)
    {
        out[n] = out[n - 1] + step;
        ++n;
    }
    return n;
}

// The box's height for eyes this high over the feet: the lowest crouched height over them; 0 (standing) over all.
float crouchLevelFor(const qmodel_t* world, float eye)
{
    float levels[maxCrouchLevels];
    const int n = crouchLevels(world, levels);
    for(int i = 0; i < n; ++i)
    {
        if(eye < levels[i])
        {
            return levels[i];
        }
    }
    return 0.f;
}

float crouchTopNum(int num)
{
    return crouchOn() && num >= 1 && num < crouchSlots ? crouchBox[num] : 0.f;
}

// The client's crouched box's height (0: standing).
float crouchTop(const edict_t* ent)
{
    return crouchTopNum(clientNum(ent));
}

// A crouched box's half extent against the map (the walls' width; Quake's 32 with vr_hull_width 0).
glm::vec3 crouchExt(float height)
{
    const float half = (vr_hull_width.value > 0.f ? widthSetting() : 32.f) * 0.5f;
    return glm::vec3{half, half, height * 0.5f};
}

// Whether a client's box of this height (0: standing) fits where he is (the map, bodies).
bool boxFitsAt(edict_t* ent, int num, float height)
{
    const float was = crouchBox[num];
    crouchBox[num] = height;
    vec3_t at{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const trace_t tr = SV_Move(at, ent->v.mins, ent->v.maxs, at, MOVE_NORMAL, ent);
    crouchBox[num] = was;
    return !tr.startsolid && !tr.allsolid;
}

// vr_crouch_status: the first player's eye height over his feet, his box's height (0: standing) and whether he could
// stand.
void crouchStatus_f()
{
    if(!sv.active || svs.maxclients < 1 || !sv.worldmodel)
    {
        Con_Printf("vr_crouch_status: no game\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    edict_t* ent = EDICT_NUM(1);
    const int headOfs = progs::fields().headpos;
    const glm::vec3 head = headOfs >= 0 ? progs::fieldVec(ent, headOfs) : glm::vec3{0.f};
    const float eye = head == glm::vec3{0.f} ? 0.f : head.z - (ent->v.origin[2] + ent->v.mins[2]);
    const float top = crouchTopNum(1);
    Con_Printf("crouch: on %d box %.0f eye %.1f lowest %.0f standfits %d origin %.1f %.1f %.1f\n", crouchOn() ? 1 : 0,
        static_cast<double>(top), static_cast<double>(eye), static_cast<double>(crouchLevelFor(sv.worldmodel, 0.f)),
        top > 0.f ? (boxFitsAt(ent, 1, 0.f) ? 1 : 0) : 1, static_cast<double>(ent->v.origin[0]),
        static_cast<double>(ent->v.origin[1]), static_cast<double>(ent->v.origin[2]));
    PR_PopQCVM(oldvm);
}

// The player's width against entities' boxes: 0 Quake's (off).
float entWidthSetting()
{
    const float v = vr_hull_ent_width.value;
    if(v < 0.f)
    {
        return vr_hull_width.value > 0.f ? widthSetting() : 0.f;
    }
    return v > 0.f ? za::clamp(v, minWidth, maxWidth) : 0.f;
}

// The player's width that shots, missiles and splash traces hit (vr_hull_hit_width): 0 Quake's (off).
float hitWidthSetting()
{
    const float v = vr_hull_hit_width.value;
    return v > 0.f && v < maxWidth ? za::clamp(v, minWidth, maxWidth) : 0.f;
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
    {"monster_ranged_knight", "Ranged Knight", &vr_mhull_hknight}, // (Dawn of the Machine's: the death knight's width)
    {"monster_zombie", "Zombie", &vr_mhull_zombie},
    {"monster_wizard", "Scrag", &vr_mhull_wizard},
    {"monster_demon1", "Fiend", &vr_mhull_demon},
    {"monster_shambler", "Shambler", &vr_mhull_shambler},
    {"monster_super_shambler", "Super Shambler", &vr_mhull_shambler}, // (Dawn of the Machine's: the shambler's width)
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
    za::Vector<int> names;
    za::Vector<signed char> classes;
    auto members() { return qvr::mem::list(names, classes); }
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
    const auto i = static_cast<za::SizeT>(num);
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
    const float c = za::max(w, minMonsterWidth);
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
    int& known = b.modelSub[static_cast<za::SizeT>(index)];
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
    for(za::SizeT i = 0; i < b.subs.size(); ++i)
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
    const bool audit = vr_hull_audit.value != 0.f;
    const auto before = audit ? alloccount::statsThisThread() : alloccount::Stats{};
    const auto base = static_cast<za::U32>(b.leafBrush.size());
    b.leafBrush.resize(b.leafBrush.size() + static_cast<za::SizeT>(m->numnodes) * 2, -1);
    addSubModel(b, h0, m->numnodes, base, h0.firstclipnode, m->mins, m->maxs);
    if(audit)
    {
        const auto after = alloccount::statsThisThread();
        Con_Printf("hull_audit: brushes frame=%d target=%s sub=%zu new=%llu bytes=%llu\n", host_framecount,
            m->name, b.subs.size() - 1, static_cast<unsigned long long>(after.calls[static_cast<int>(alloccount::Kind::New)] - before.calls[static_cast<int>(alloccount::Kind::New)]),
            static_cast<unsigned long long>(after.requestedBytes - before.requestedBytes));
    }
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
    za::Vector<Tree>& v = monsterTrees.trees;
    for(Tree& t : v)
    {
        if(t.forClipnodes == clipnodes && t.ext == ext)
        {
            return t;
        }
    }
    if(v.size() >= maxMonsterTrees)
    {
        ++hullAudit.clears;
        if(vr_hull_audit.value != 0.f)
        {
            Con_Printf("hull_audit: clear frame=%d slots=%zu bytes=%zu\n", host_framecount, v.size(), monsterTrees.bytes());
        }
        v.clear();
    }
    ++hullAudit.slots;
    v.emplaceBack();
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
    za::Vector<Tree*> trees;           // being compiled, each by its job below (after the brushes)
    za::Vector<jobs::Future<int>> run; // (their pieces cut back)
    glm::vec3 playerExt{0.f};           // the player's tree's box, if it is one of them
    double posted = 0.0;                // when the builds were handed out (the load's report)
    bool kept = false;                  // the brushes (and the trees found) the last load's of the same map
};
Pending pending;

// The trees for these boxes whose world model (sub 0) is not compiled yet: their slots taken (their order kept, each
// once; the player's size is the player's tree); none being compiled already (pending).
za::Vector<Tree*> claimTrees(const mclipnode_t* clipnodes, const za::Vector<glm::vec3>& exts)
{
    for(const glm::vec3& ext : exts)
    {
        if(!pending.trees.empty() && !findSlot(clipnodes, ext) && monsterTrees.trees.size() >= maxMonsterTrees)
        {
            continue; // (a new slot would let go of the trees being made: this one is made when first used)
        }
        (void)slotFor(clipnodes, ext);
    }
    za::Vector<Tree*> out; // (found after every slot is taken: a new one may move the others)
    for(const glm::vec3& ext : exts)
    {
        Tree* t = findSlot(clipnodes, ext);
        if(t && za::find(pending.trees.begin(), pending.trees.end(), t) == pending.trees.end() &&
            (t->heads.empty() || t->heads[0] < 0) && za::find(out.begin(), out.end(), t) == out.end())
        {
            out.pushBack(t);
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

jobs::Site treesSite{"hull trees"}; // (its parallelFor: vr_jobs_sites)

// These trees compiled at once on the pool (the main thread one of them), reported in their order.
void compileTrees(const za::Vector<Tree*>& todo, const Brushes& b)
{
    za::Vector<int> rebounded(todo.size(), 0);
    jobs::parallelFor(treesSite, todo.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                rebounded[i] = compileWorldTree(*todo[i], b);
            }
        });
    for(za::SizeT i = 0; i < todo.size(); ++i)
    {
        reportTree(*todo[i], rebounded[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The world's compiled trees kept on disk (vr_hull_cache): `<gamedir>/cache/hulls/<build>/<world>_<box>.hul`, <world>
// the world's content (keyOf: hull 0's and hull 1's nodes, the planes, the models' heads), <box> the tree's box, <build>
// this file's compile time (changed code never reads an old tree). A tree is the same bytes whenever the same brushes are
// compiled for the same box (the build on the pool is the build on one thread's, node for node), so a file is that
// build's result: a cold start of a big map reads it instead of compiling it (vrstart: four trees of 1.2-1.4 million
// nodes, 9 s of 32 threads). Only trees that took diskMinMs or more are written (a small map's build is about as quick
// as its file). A file is read and checked (magic, version, sizes, the world and the box, every node's numbers, a sum of
// its bytes) on the tree's own job; anything amiss is compiled again and written over. Written beside its place and
// renamed (another copy of the game never reads half a file). The first use in a session under a game directory
// removes the other builds' folders and the oldest files past diskBudget. vr_hull_cache 2: read, then compiled anyway
// and compared (vr_hull_stats counts them).

constexpr char diskMagic[4] = {'Q', 'V', 'R', 'H'};
constexpr za::U32 diskVersion = 1;
constexpr double diskMinMs = 250.0;
constexpr za::U64 diskBudget = 1024ull << 20; // bytes in the build's folder (vrstart's four trees: 105 MB)

za::String makeDiskBuild()
{
    const char* stamp = __DATE__ " " __TIME__;
    za::U64 h = 14695981039346656037ull;
    for(; *stamp; ++stamp)
    {
        h = (h ^ static_cast<unsigned char>(*stamp)) * 1099511628211ull;
    }
    char build[24];
    snprintf(build, sizeof(build), "%016llx", static_cast<unsigned long long>(h));
    return build;
}
const za::String diskBuild = makeDiskBuild(); // (made before main, only read)

struct DiskHeader
{
    char magic[4];
    za::U32 version;
    za::U64 world;
    float ext[3];
    za::U32 nodes, planes;
    za::I32 root, solid, empty, rebounded;
    za::U64 sum; // of the nodes' and planes' bytes (diskSum)
};

constexpr za::U64 diskSeed = 14695981039346656037ull;

// A sum of the bytes (eight at a time; the rest one by one).
za::U64 diskSum(const void* data, za::SizeT size, za::U64 h)
{
    const unsigned char* c = static_cast<const unsigned char*>(data);
    za::SizeT i = 0;
    for(; i + 8 <= size; i += 8)
    {
        za::U64 w;
        memcpy(&w, c + i, 8);
        h = (h ^ w) * 0x9E3779B97F4A7C15ull;
        h ^= h >> 29;
    }
    for(; i < size; ++i)
    {
        h = (h ^ c[i]) * 1099511628211ull;
    }
    return h;
}

za::U64 diskSumOf(const za::Vector<mclipnode_t>& nodes, const za::Vector<mplane_t>& planes)
{
    return diskSum(planes.data(), planes.size() * sizeof(mplane_t),
        diskSum(nodes.data(), nodes.size() * sizeof(mclipnode_t), diskSeed));
}

// What a tree's job does with the disk (made on the main thread when it is posted: the game directory, the setting).
struct DiskJob
{
    int mode = 0;    // vr_hull_cache: 0 off, 1 read and write, 2 read, then compiled anyway and compared
    za::String path; // its file
    za::U64 world = 0;
};

// The disk cache's counts this session (vr_hull_stats), added to by the trees' jobs.
struct DiskCounts
{
    std::atomic<int> read{0}, written{0}, missed{0}, rejected{0}, same{0}, differed{0};
    std::atomic<long long> readUs{0};
};
DiskCounts diskCounts;
za::Vector<za::String> diskPruned; // the cache roots pruned this session (the main thread)

za::String diskRoot()
{
    return za::String{com_gamedir} + "/cache/hulls";
}

// The first use in a session under a game directory (the main thread, before the trees' jobs are posted): the other
// builds' folders removed, then the oldest files (by their last write) until the build's folder is within diskBudget.
void diskPrune()
{
    const za::String root = diskRoot();
    if(za::find(diskPruned.begin(), diskPruned.end(), root) != diskPruned.end())
    {
        return;
    }
    diskPruned.pushBack(root);
    za::Vector<za::String> others;
    files::forEachEntry(root.cStr(), [&](const char* name, bool) {
        if(strcmp(name, diskBuild.cStr()) != 0)
        {
            others.pushBack(root + "/" + name);
        }
    });
    for(const za::String& other : others)
    {
        files::removeAll(other.cStr());
    }
    struct Entry
    {
        za::String path;
        za::I64 time;
        za::U64 bytes;
    };
    za::Vector<Entry> entries;
    za::U64 total = 0;
    const za::String dir = root + "/" + diskBuild;
    files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory)
        {
            Entry e{dir + "/" + name, 0, 0};
            e.time = files::lastWriteTime(e.path.cStr());
            e.bytes = files::fileSize(e.path.cStr());
            total += e.bytes;
            entries.pushBack(ZA_MOVE(e));
        }
    });
    if(total > diskBudget)
    {
        za::quickSort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.time < b.time; });
        for(const Entry& e : entries)
        {
            if(total <= diskBudget * 3 / 4)
            {
                break;
            }
            if(files::remove(e.path.cStr()))
            {
                total -= e.bytes;
            }
        }
    }
    files::createDirectories(dir.cStr());
}

// The tree's job's disk (the main thread): off with the setting.
DiskJob diskJob(const Tree& t, qmodel_t* world)
{
    DiskJob d;
    d.mode = static_cast<int>(za::clamp(vr_hull_cache.value, 0.f, 2.f));
    if(d.mode == 0 || !world || !com_gamedir[0])
    {
        d.mode = 0;
        return d;
    }
    diskPrune();
    d.world = keyOf(world);
    char name[96];
    snprintf(name, sizeof(name), "/%016llx_%g_%g_%g.hul", static_cast<unsigned long long>(d.world),
        static_cast<double>(t.ext.x), static_cast<double>(t.ext.y), static_cast<double>(t.ext.z));
    d.path = diskRoot() + "/" + diskBuild + name;
    return d;
}

// The world's tree read into t (a fresh tree, its heads sized) and checked; false: no file, or one amiss (t untouched).
bool diskRead(Tree& t, const DiskJob& d, int& rebounded)
{
    FILE* in = Sys_fopen(d.path.cStr(), "rb");
    if(!in)
    {
        return false;
    }
    Sys_fseek(in, 0, SEEK_END);
    const qfileofs_t size = Sys_ftell(in);
    Sys_fseek(in, 0, SEEK_SET);
    DiskHeader h{};
    bool ok = size >= static_cast<qfileofs_t>(sizeof(h)) && fread(&h, 1, sizeof(h), in) == sizeof(h) &&
              memcmp(h.magic, diskMagic, 4) == 0 && h.version == diskVersion && h.world == d.world &&
              h.ext[0] == t.ext.x && h.ext[1] == t.ext.y && h.ext[2] == t.ext.z && h.nodes > 0 && h.root >= 0 &&
              static_cast<za::U32>(h.root) < h.nodes &&
              size == static_cast<qfileofs_t>(sizeof(h) + za::SizeT{h.nodes} * sizeof(mclipnode_t) +
                                              za::SizeT{h.planes} * sizeof(mplane_t));
    za::Vector<mclipnode_t> nodes;
    za::Vector<mplane_t> planes;
    if(ok)
    {
        // (room for the brush models' trees that follow in the same arrays, prepareBrushModels: exact sizes would grow
        // by half at their first node, 10 MB more a tree on vrstart)
        nodes.reserve(za::SizeT{h.nodes} + h.nodes / 16 + 1024);
        planes.reserve(za::SizeT{h.planes} + h.planes / 16 + 1024);
        nodes.resize(h.nodes);
        planes.resize(h.planes);
        ok = fread(nodes.data(), sizeof(mclipnode_t), nodes.size(), in) == nodes.size() &&
             fread(planes.data(), sizeof(mplane_t), planes.size(), in) == planes.size();
    }
    fclose(in);
    ok = ok && diskSumOf(nodes, planes) == h.sum;
    for(za::SizeT i = 0; ok && i < nodes.size(); ++i)
    {
        const mclipnode_t& n = nodes[i];
        ok = n.planenum >= 0 && static_cast<za::U32>(n.planenum) < h.planes;
        for(const int c : n.children)
        {
            ok = ok && (c >= 0 ? static_cast<za::U32>(c) < h.nodes : c >= CONTENTS_SKY);
        }
    }
    if(!ok)
    {
        ++diskCounts.rejected;
        return false;
    }
    t.nodes = ZA_MOVE(nodes);
    t.planes = ZA_MOVE(planes);
    t.index.clear(); // (made again by the next build that needs it: TreeBuilder)
    t.indexed = 0;
    t.heads[0] = h.root;
    t.solidLeaves = h.solid;
    t.emptyLeaves = h.empty;
    rebounded = h.rebounded;
    checkpoint(t); // (as buildTree does for the world)
    return true;
}

// The world's tree just compiled into t written (all its nodes and planes: the world's, nothing else built yet).
void diskWrite(const Tree& t, const DiskJob& d, int rebounded)
{
    DiskHeader h{};
    memcpy(h.magic, diskMagic, 4);
    h.version = diskVersion;
    h.world = d.world;
    h.ext[0] = t.ext.x;
    h.ext[1] = t.ext.y;
    h.ext[2] = t.ext.z;
    h.nodes = static_cast<za::U32>(t.nodes.size());
    h.planes = static_cast<za::U32>(t.planes.size());
    h.root = t.heads[0];
    h.solid = t.solidLeaves;
    h.empty = t.emptyLeaves;
    h.rebounded = rebounded;
    h.sum = diskSumOf(t.nodes, t.planes);
    char suffix[32];
    snprintf(suffix, sizeof(suffix), ".%llx.tmp", static_cast<unsigned long long>(za::Clock::nowNanoseconds()));
    const za::String tmp = d.path + suffix;
    FILE* out = Sys_fopen(tmp.cStr(), "wb");
    if(!out)
    {
        return;
    }
    bool ok = fwrite(&h, 1, sizeof(h), out) == sizeof(h) &&
              fwrite(t.nodes.data(), sizeof(mclipnode_t), t.nodes.size(), out) == t.nodes.size() &&
              fwrite(t.planes.data(), sizeof(mplane_t), t.planes.size(), out) == t.planes.size();
    ok = fclose(out) == 0 && ok;
    if(ok && files::rename(tmp.cStr(), d.path.cStr()))
    {
        ++diskCounts.written;
    }
    else
    {
        files::remove(tmp.cStr());
    }
}

za::U32 hashOf(const Tree& t);

// The world's tree in t from the disk, else compiled (and written when it took long enough); the pieces cut back.
int diskOrCompile(Tree& t, const Brushes& b, const DiskJob& d)
{
    if(d.mode == 0 || (!t.heads.empty() && t.heads[0] >= 0) || !t.planes.empty() || !t.nodes.empty())
    {
        return compileWorldTree(t, b); // (not a fresh tree: a file holds a fresh tree's build)
    }
    t.heads.resize(b.subs.size(), -1);
    const auto t0 = za::Clock::nowNanoseconds();
    int rebounded = 0;
    if(diskRead(t, d, rebounded))
    {
        const auto ns = za::Clock::nowNanoseconds() - t0;
        ++diskCounts.read;
        diskCounts.readUs += static_cast<long long>(ns / 1000);
        t.ms += za::nanosecondsToMilliseconds(ns);
        t.fromDisk = true;
        if(d.mode == 2) // compiled anyway into a fresh tree, compared
        {
            Tree fresh;
            fresh.ext = t.ext;
            fresh.forClipnodes = t.forClipnodes;
            fresh.heads.resize(b.subs.size(), -1);
            (void)buildTree(fresh, b, 0, nullptr, false);
            const bool same = fresh.heads[0] == t.heads[0] && fresh.solidLeaves == t.solidLeaves &&
                              fresh.emptyLeaves == t.emptyLeaves && fresh.nodes.size() == t.nodes.size() &&
                              fresh.planes.size() == t.planes.size() &&
                              ZA_MEMCMP(fresh.nodes.data(), t.nodes.data(), t.nodes.size() * sizeof(mclipnode_t)) == 0 &&
                              ZA_MEMCMP(fresh.planes.data(), t.planes.data(), t.planes.size() * sizeof(mplane_t)) == 0;
            ++(same ? diskCounts.same : diskCounts.differed);
        }
        return rebounded;
    }
    ++diskCounts.missed;
    rebounded = compileWorldTree(t, b);
    if(t.ms >= diskMinMs && t.heads[0] >= 0)
    {
        diskWrite(t, d, rebounded);
    }
    return rebounded;
}

// A tree compiled on the pool once the brushes are built (or read from the disk: vr_hull_cache).
void postTree(Tree* t)
{
    pending.trees.pushBack(t);
    DiskJob d = diskJob(*t, sv.worldmodel);
    pending.run.pushBack(jobs::async(
        [t, d = ZA_MOVE(d)]
        {
            pending.brushes.wait(); // (only waited on until settle: nothing else touches it meanwhile)
            return diskOrCompile(*t, built, d);
        }));
}

void settle()
{
    if(!pending.brushes.valid())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    za::Vector<jobs::Future<int>> run = ZA_MOVE(pending.run);
    za::Vector<Tree*> trees = ZA_MOVE(pending.trees);
    pending.run.clear();
    pending.trees.clear();
    za::Vector<int> rebounded;
    for(jobs::Future<int>& f : run)
    {
        rebounded.pushBack(f.get());
    }
    pending.brushes.get();
    const double t1 = Sys_DoubleTime();
    VR_TimeAdd("hull: the map's brushes and compiled hulls, waited for (built on the pool)", t1 - t0);
    Con_DPrintf("hull: %s %s as %d brushes in %.1f ms (on the pool; waited %.1f ms, %.1f ms after they began)\n",
        sv.worldmodel ? sv.worldmodel->name : "?", pending.kept ? "kept from its last load" : "rebuilt",
        static_cast<int>(built.brushes.size()), built.ms, (t1 - t0) * 1000.0, (t1 - pending.posted) * 1000.0);
    for(za::SizeT i = 0; i < trees.size(); ++i)
    {
        reportTree(*trees[i], rebounded[i]);
    }
}

const Tree& treeFor(const Brushes& b, za::SizeT sub, const glm::vec3& ext, const char* target = nullptr)
{
    settle();
    const bool audit = vr_hull_audit.value != 0.f;
    const bool known = audit && findSlot(b.clipnodes, ext);
    const za::U64 clearsBefore = hullAudit.clears;
    const auto before = audit ? alloccount::statsThisThread() : alloccount::Stats{};
    Tree& t = slotFor(b.clipnodes, ext);
    if(t.heads.size() < b.subs.size())
    {
        t.heads.resize(b.subs.size(), -1);
    }
    if(t.heads[sub] < 0)
    {
        ++hullAudit.runtimeBuilds;
        const double t0 = audit ? Sys_DoubleTime() : 0.0;
        buildTree(t, b, sub);
        if(audit)
        {
            const auto after = alloccount::statsThisThread();
            const auto delta = [&](alloccount::Kind k) {
                const int i = static_cast<int>(k);
                return after.calls[i] - before.calls[i];
            };
            Con_Printf("hull_audit: build frame=%d sub=%zu target=%s size=%.3fx%.3fx%.3f reason=%s new=%llu c=%llu "
                       "bytes=%llu retained=%zu ms=%.3f\n", host_framecount, sub, target ? target : "(probe)",
                static_cast<double>(ext.x * 2.f), static_cast<double>(ext.y * 2.f), static_cast<double>(ext.z * 2.f),
                hullAudit.clears != clearsBefore ? "cache-clear" : (known ? "lazy-submodel" : "new-size"),
                static_cast<unsigned long long>(delta(alloccount::Kind::New)),
                static_cast<unsigned long long>(delta(alloccount::Kind::Malloc) + delta(alloccount::Kind::Calloc) + delta(alloccount::Kind::Realloc)),
                static_cast<unsigned long long>(after.requestedBytes - before.requestedBytes), heldBytes(t),
                (Sys_DoubleTime() - t0) * 1000.0);
        }
        if(sub == 0)
        {
            Con_DPrintf("hull: %s compiled for %gx%g: %d nodes, %d planes in %.1f ms (%d pieces done again)\n", sv.worldmodel->name, ext.x * 2.f,
                ext.z * 2.f, static_cast<int>(t.nodes.size()), static_cast<int>(t.planes.size()), t.ms, t.redone);
        }
    }
    return t;
}

// Exercise more than the old 12-size limit and revisit every compiled world tree.
za::U32 hashOf(const Tree& t);

void cacheTest_f()
{
    if(!sv.active || !sv.worldmodel || !worldBrushes(sv.worldmodel)) return;
    settle();
    const int room = static_cast<int>(maxMonsterTrees - monsterTrees.trees.size());
    const int count = za::min(room, za::clamp(Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 64, 1, 120));
    if(count < 1) { Con_Printf("vr_hull_cachetest: no free slots; run on a fresh map\n"); return; }
    za::Vector<za::U32> hashes;
    const auto extAt = [](int i) { const float half = 4.f + static_cast<float>(i) / 32.f; return glm::vec3{half, half, 28.f}; };
    const auto clears = hullAudit.clears;
    const Tree* first = nullptr;
    for(int i = 0; i < count; ++i)
    {
        const Tree& t = treeFor(built, 0, extAt(i), "(cachetest)");
        if(i == 0) first = &t;
        hashes.pushBack(hashOf(t));
    }
    const auto builds = hullAudit.runtimeBuilds;
    bool ok = hullAudit.clears == clears;
    for(int i = 0; i < count; ++i)
    {
        ok = ok && hashOf(treeFor(built, 0, extAt(i), "(cachetest)")) == hashes[static_cast<za::SizeT>(i)];
    }
    ok = ok && hullAudit.runtimeBuilds == builds && findSlot(built.clipnodes, extAt(0)) == first;
    Con_Printf("vr_hull_cachetest: %s sizes=%d slots=%zu limit=%zu clears=%llu retained=%zu\n", ok ? "PASS" : "FAIL",
        count, monsterTrees.trees.size(), maxMonsterTrees, static_cast<unsigned long long>(hullAudit.clears - clears),
        monsterTrees.bytes());
}

// Diagnostic control: prebuild loaded brush models for the box sizes already in the cache.
// Runs synchronously only when requested; shifts first-touch work out of a subsequent capture.
void warmCache_f()
{
    if(!sv.active || !sv.worldmodel || !worldBrushes(sv.worldmodel)) return;
    settle();
    za::Vector<glm::vec3> exts;
    if(tree.forClipnodes == built.clipnodes) exts.pushBack(tree.ext);
    for(const Tree& t : monsterTrees.trees)
    {
        if(t.forClipnodes == built.clipnodes) exts.pushBack(t.ext);
    }
    const auto before = hullAudit.runtimeBuilds;
    for(int i = 1; i < MAX_MODELS; ++i)
    {
        const qmodel_t* model = sv.models[i];
        if(!model || model->type != mod_brush) continue;
        const int sub = subOf(built, i);
        if(sub < 0) continue;
        for(const glm::vec3& ext : exts)
        {
            (void)treeFor(built, static_cast<za::SizeT>(sub), ext, model->name);
        }
    }
    Con_Printf("hull_audit: warmcache built=%llu sizes=%zu slots=%zu bytes=%zu\n",
        static_cast<unsigned long long>(hullAudit.runtimeBuilds - before), exts.size(), monsterTrees.trees.size(),
        tree.bytes() + monsterTrees.bytes());
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
    za::U32 h = 2166136261u;
    template <class T>
    void add(const T& v)
    {
        const auto* p = reinterpret_cast<const unsigned char*>(&v);
        for(za::SizeT i = 0; i < sizeof(T); ++i)
        {
            h = (h ^ p[i]) * 16777619u;
        }
    }
};

za::U32 hashOf(const Brushes& b)
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
za::U32 hashOf(const Tree& t)
{
    Hash h;
    const int root = t.heads.empty() ? -1 : t.heads[0];
    za::Vector<int> stack{root};
    while(!stack.empty())
    {
        const int num = stack.back();
        stack.popBack();
        h.add(num);
        if(num < 0)
        {
            continue;
        }
        const mclipnode_t& n = t.nodes[static_cast<za::SizeT>(num)];
        const mplane_t& p = t.planes[static_cast<za::SizeT>(n.planenum)];
        h.add(n.planenum);
        h.add(p.normal);
        h.add(p.dist);
        h.add(p.type);
        h.add(p.signbits);
        stack.pushBack(n.children[1]);
        stack.pushBack(n.children[0]);
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
    const za::SizeT bytes = built.bytes();
    Con_Printf("hull: %s: %d nodes (hulls 1-2: %d clipnodes), %d brushes (%d clip), %d planes (%d bevels), %d slivers "
               "dropped, %.0f KB, built in %.1f ms (clip brushes %.1f); width %g (%s)\n",
        sv.worldmodel->name, b->numnodes, sv.worldmodel->numclipnodes, static_cast<int>(b->brushes.size()),
        static_cast<int>(b->clips.size()), static_cast<int>(b->planes.size()), b->bevels, b->dropped, bytes / 1024.0,
        b->ms, b->clipMs, vr_hull_width.value > 0.f ? widthSetting() : 0.f,
        vr_hull_width.value > 0.f ? "on" : "off: Quake's hull 1");
    Con_Printf("hull: hash brushes %08x (stamp %u)\n", hashOf(*b), static_cast<unsigned>(b->stamp));
    int external = 0;
    for(za::SizeT i = 0; i < b->subs.size(); ++i)
    {
        external += b->subs[i].clipnodes != b->clipnodes;
    }
    const Tree& t = tree;
    Con_Printf("hull: %d brush models (%d external .bsp); method %s; compiled hull: %s%d nodes, %d planes, %d solid "
               "and %d empty leaves, %.0f KB, %s in %.1f ms\n",
        static_cast<int>(b->subs.size()), external, vr_hull_method.value != 0.f ? "compiled hull" : "brush sweep",
        t.forClipnodes == b->clipnodes ? "" : "(none yet) ", static_cast<int>(t.nodes.size()),
        static_cast<int>(t.planes.size()), t.solidLeaves, t.emptyLeaves, tree.bytes() / 1024.0,
        t.fromDisk ? "read from the disk cache" : "built", t.ms);
    if(t.forClipnodes == b->clipnodes)
    {
        Con_Printf("hull: hash tree %gx%g %08x\n", t.ext.x * 2.f, t.ext.z * 2.f, hashOf(t));
    }
    Con_Printf("hull_audit: cache slots=%zu limit=%zu new_slots=%llu clears=%llu runtime_builds=%llu (since map load)\n",
        monsterTrees.trees.size(), maxMonsterTrees, static_cast<unsigned long long>(hullAudit.slots),
        static_cast<unsigned long long>(hullAudit.clears), static_cast<unsigned long long>(hullAudit.runtimeBuilds));
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
        Con_Printf("hull: monsters' tree %gx%g: %d nodes, %d planes, %.0f KB, %s in %.1f ms\n", m.ext.x * 2.f,
            m.ext.z * 2.f, static_cast<int>(m.nodes.size()), static_cast<int>(m.planes.size()), heldBytes(m) / 1024.0,
            m.fromDisk ? "read from the disk cache" : "built", m.ms);
        Con_Printf("hull: hash tree %gx%g %08x\n", m.ext.x * 2.f, m.ext.z * 2.f, hashOf(m));
    }
    Con_Printf("hull: monsters (vr_mhull %s): %d trees, %.0f KB, built in %.1f ms\n", vr_mhull.value != 0.f ? "on" : "off",
        count, monsterTrees.bytes() / 1024.0, ms);
    Con_Printf("hull: kept for reloads (vr_hull_keep %d): %d other maps, %.0f KB; this load's %s; %llu loads got theirs "
               "back, %llu built them\n",
        static_cast<int>(keepLimit()), static_cast<int>(kept.maps.size()), kept.bytes() / 1024.0,
        pending.kept ? "kept from its last load" : "built", static_cast<unsigned long long>(kept.hits),
        static_cast<unsigned long long>(kept.misses));
    Con_Printf("hull: disk cache (vr_hull_cache %d, this session): %d trees read (%.1f ms), %d compiled (no file), %d "
               "written, %d files amiss; compared (2): %d the same, %d different\n",
        static_cast<int>(vr_hull_cache.value), diskCounts.read.load(), static_cast<double>(diskCounts.readUs.load()) / 1000.0,
        diskCounts.missed.load(), diskCounts.written.load(), diskCounts.rejected.load(), diskCounts.same.load(),
        diskCounts.differed.load());
}

// vr_hull_keeptest: the map's brushes and its trees' world models built again from scratch, their hashes against the
// server's (kept from the last load or built with this one: they must be the same).
void keepTest_f()
{
    if(!sv.active || !sv.worldmodel || !worldBrushes(sv.worldmodel))
    {
        Con_Printf("vr_hull_keeptest: no map\n");
        return;
    }
    qmodel_t* world = sv.worldmodel;
    Brushes fresh;
    build(fresh, world, keyOf(world));
    Brushes live = built; // (the world's own, as a reload keeps them: what a fresh build makes)
    cutBack(live);
    bool ok = hashOf(live) == hashOf(fresh) && built.key == fresh.key;
    Con_Printf("vr_hull_keeptest: brushes %08x, fresh %08x; key %016llx, fresh %016llx\n", hashOf(live), hashOf(fresh),
        static_cast<unsigned long long>(built.key), static_cast<unsigned long long>(fresh.key));
    int trees = 0;
    auto check = [&](const Tree& t)
    {
        if(t.forClipnodes != built.clipnodes || t.heads.empty() || t.heads[0] < 0)
        {
            return;
        }
        Tree f;
        f.ext = t.ext;
        f.heads.resize(fresh.subs.size(), -1);
        buildTree(f, fresh, 0, nullptr, false);
        const bool same = hashOf(f) == hashOf(t);
        ok = ok && same;
        ++trees;
        Con_Printf("vr_hull_keeptest: tree %gx%g %08x, fresh %08x%s\n", t.ext.x * 2.f, t.ext.z * 2.f, hashOf(t), hashOf(f),
            same ? "" : " (differs)");
    };
    check(tree);
    for(const Tree& t : monsterTrees.trees)
    {
        check(t);
    }
    Con_Printf("vr_hull_keeptest: %s (%d trees; this load's %s)\n", ok ? "PASS" : "FAIL", trees,
        pending.kept ? "kept from its last load" : "built");
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
    const int count = Cmd_Argc() > 1 ? za::max(100, Q_atoi(Cmd_Argv(1))) : 20000;
    const float width = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), minWidth, maxWidth)
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
    za::FastNonCryptoRng rng{1234u}; // the same points every run
    const auto u01 = [&rng] { return rng.getF(0.f, 1.f); };
    auto randomPoint = [&] { return wmins + (wmaxs - wmins) * glm::vec3{u01(), u01(), u01()}; };

    za::Vector<glm::vec3> starts, ends;
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
        glm::vec3 dir{u01() * 2.f - 1.f, u01() * 2.f - 1.f, u01() * 2.f - 1.f};
        if(glm::length(dir) < 0.1f)
        {
            continue;
        }
        starts.pushBack(p);
        ends.pushBack(p + glm::normalize(dir) * (u01() * 256.f));
    }
    const int n = static_cast<int>(starts.size());
    if(!n)
    {
        Con_Printf("vr_hull_bench: no room found\n");
        return;
    }
    const glm::vec3 m32{-16.f, -16.f, -24.f}, M32{16.f, 16.f, 32.f};
    const glm::vec3 mw{-width * 0.5f, -width * 0.5f, -24.f}, Mw{width * 0.5f, width * 0.5f, 32.f};
    za::Vector<trace_t> stock(n), box32(n), boxw(n);
    Bench bs, b32, bw;
    // Each set twice, the second timed (the first warms the caches).
    for(int pass = 0; pass < 2; ++pass)
    {
        auto t0 = za::Clock::nowNanoseconds();
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
        bs.ns = static_cast<double>(za::Clock::nowNanoseconds() - t0) / n;
        b32.brushTests = bw.brushTests = 0;
        t0 = za::Clock::nowNanoseconds();
        for(int i = 0; i < n; ++i)
        {
            const Result r = boxTrace(*b, hull0, 0, starts[i], m32, M32, ends[i]);
            box32[i] = r.trace;
            b32.brushTests += r.brushTests;
        }
        b32.ns = static_cast<double>(za::Clock::nowNanoseconds() - t0) / n;
        t0 = za::Clock::nowNanoseconds();
        for(int i = 0; i < n; ++i)
        {
            const Result r = boxTrace(*b, hull0, 0, starts[i], mw, Mw, ends[i]);
            boxw[i] = r.trace;
            bw.brushTests += r.brushTests;
        }
        bw.ns = static_cast<double>(za::Clock::nowNanoseconds() - t0) / n;
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
        if(za::abs(gap) <= 1.f)
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
            worstStock = za::max(worstStock, gap);
        }
        else
        {
            ++brushShorter;
            worstBrush = za::max(worstBrush, -gap);
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
        t.heads.clear();
        t.heads.resize(b->subs.size(), -1);
        buildTree(t, *b, 0);
    };
    auto treeBytes = [](const Tree& t)
    { return t.nodes.capacity() * sizeof(mclipnode_t) + t.planes.capacity() * sizeof(mplane_t); };
    Tree a32, aw;
    compile(a32, m32, M32);
    compile(aw, mw, Mw);
    za::Vector<trace_t> tree32(n), treew(n);
    Bench ba32, baw;
    for(int pass = 0; pass < 2; ++pass)
    {
        for(int k = 0; k < 2; ++k)
        {
            const Tree& t = k ? aw : a32;
            const glm::vec3 centre = k ? (mw + Mw) * 0.5f : (m32 + M32) * 0.5f;
            za::Vector<trace_t>& out = k ? treew : tree32;
            const auto t0 = za::Clock::nowNanoseconds();
            for(int i = 0; i < n; ++i)
            {
                out[i] = treeTrace(t, t.heads[0], starts[i] + centre, ends[i] + centre);
            }
            (k ? baw : ba32).ns = static_cast<double>(za::Clock::nowNanoseconds() - t0) / n;
        }
    }
    // Agreement within a unit along the move; startsolid differing.
    auto compare = [&](const za::Vector<trace_t>& x, const za::Vector<trace_t>& y, int& same, int& xSooner, int& ySooner,
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
            if(za::abs(gap) <= 1.f)
            {
                ++same;
                continue;
            }
            (gap > 0.f ? xSooner : ySooner)++;
            if(za::abs(gap) >= 8.f && shown++ < 2)
            {
                Con_Printf("  %s: %.0f units apart from %.0f %.0f %.0f (%s sooner)\n", what, za::abs(gap), starts[i].x,
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
    za::U32 seed = 1;
    int frames = 0, stuck = 0, embedded = 0, outside = 0, hops = 0, hopFails = 0, stuckAfterHop = 0, moverHops = 0, stuckOther = 0;
    double travelled = 0.0;
    glm::vec3 last{0.f};
    bool lastValid = false;
};
WalkTest walkTest;

za::U32 nextRandom(za::U32& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

float random01(za::U32& s)
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
    za::Vector<Mon> mons;
    Counts counts[monsterClassCount];
    double until = 0.0, nextStep = 0.0, started = 0.0;
    za::U32 seed = 1;
    int mode = 0;
    edict_t* goal = nullptr;
    za::String then; // a command run at the end (a test's next map)
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
        p = o + glm::vec3{za::cos(yaw) * d, za::sin(yaw) * d, flies ? (random01(w.seed) - 0.5f) * 96.f : 24.f};
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
        Cbuf_AddText(w.then.cStr());
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
        const bool there = glm::length(across) < 32.f && za::abs(o.z - m.goal.z) < 48.f;
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
    w.seed = Cmd_Argc() > 2 ? static_cast<za::U32>(za::max(1, Q_atoi(Cmd_Argv(2)))) : 1u;
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
        w.mons.pushBack(m);
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
    const double offset = za::abs(ext.x * n.x) + za::abs(ext.y * n.y) + za::abs(ext.z * n.z);
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
        const za::SizeT key = static_cast<za::SizeT>(num) * 2 + side;
        auto report = [&](int index)
        {
            const Brush& br = b.brushes[index];
            if(!boxInBrush(b, br, p, ext))
            {
                return;
            }
            double best = -1e300;
            int bestPlane = -1;
            for(za::U32 i = 0; i < br.count; ++i)
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
    t.heads.clear();
    t.heads.resize(b->subs.size(), -1);
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
    za::U32 seed = 12345u;
    int shots = 0, hits = 0, dirs = 0;
    for(int k = 0; k < 8; ++k)
    {
        const float a = static_cast<float>(k) * 0.785398163f;
        const glm::vec3 from = target + glm::vec3{za::cos(a), za::sin(a), 0.f} * dist;
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
            const glm::vec3 dir{za::cos(a), za::sin(a), 0.f};
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
    const float reach = za::max(hi.x - lo.x, hi.y - lo.y) * 0.5f + 40.f;
    int tried = 0, met = 0;
    float gapLo = 1e9f, gapHi = -1e9f;
    for(int k = 0; k < 8; ++k)
    {
        const float a = static_cast<float>(k) * 0.785398163f;
        const glm::vec3 dir{-za::cos(a), -za::sin(a), 0.f}; // towards the entity
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
            gapLo = za::min(gapLo, gap);
            gapHi = za::max(gapHi, gap);
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
    w.seed = Cmd_Argc() > 2 ? static_cast<za::U32>(za::max(1, Q_atoi(Cmd_Argv(2)))) : 1u;
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
    return sv.worldmodel && vr_hull_method.value != 0.f &&
           (vr_hull_width.value > 0.f || vr_mhull.value != 0.f || crouchOn());
}

glm::vec3 playerExt(const qmodel_t* world)
{
    const float half = widthSetting() * 0.5f;
    return glm::vec3{half, half, hull1Height(world) * 0.5f};
}

// The boxes the trees are wanted for: the player's (first, if on), then each size the level's monsters ask for
// (vr_mhull), in their order (loading: while the server spawns them).
za::Vector<glm::vec3> wantedExts(bool loading)
{
    za::Vector<glm::vec3> exts;
    if(vr_hull_width.value > 0.f)
    {
        exts.pushBack(playerExt(sv.worldmodel));
    }
    if(crouchOn())
    {
        float levels[maxCrouchLevels];
        const int n = crouchLevels(sv.worldmodel, levels);
        for(int i = 0; i < n; ++i)
        {
            exts.pushBack(crouchExt(levels[i])); // the crouched player's (vr_crouch_hull)
        }
    }
    if(vr_mhull.value != 0.f && (sv.active || loading))
    {
        for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; ++i)
        {
            const edict_t* e = reinterpret_cast<const edict_t*>(reinterpret_cast<const byte*>(sv.qcvm.edicts) +
                static_cast<za::PtrDiffT>(i) * sv.qcvm.edict_size);
            float w = 0.f, h = 0.f;
            if(!e->free && monsterWidth(e, w, h, loading))
            {
                exts.emplaceBack(w * 0.5f, w * 0.5f, h * 0.5f);
            }
        }
    }
    return exts;
}

// Recover every loaded brush model before workers read the shared brushes. Each worker owns one size's Tree:
// its submodels append to the same node/plane arrays, so they must not be built concurrently with each other.
// The brush models a box can collide with now: those of live edicts that are neither triggers nor SOLID_NOT (the
// clipping lists never hold those: world.c's SV_ClipToLinks; func_illusionary, every trigger_*). One made solid later,
// or a model no edict uses yet, is built on its first trace (traceSub's runtime build).
void collidingSubs(za::Vector<za::SizeT>& subs)
{
    subs.clear();
    for(int e = 1; e < qcvm->num_edicts; ++e)
    {
        const edict_t* ent = EDICT_NUM(e);
        const int index = static_cast<int>(ent->v.modelindex);
        if(ent->free || index <= 0 || index >= MAX_MODELS || ent->v.solid == static_cast<float>(SOLID_TRIGGER) ||
           ent->v.solid == static_cast<float>(SOLID_NOT)) continue;
        const qmodel_t* model = sv.models[index];
        if(!model || model->type != mod_brush) continue;
        const int sub = subOf(built, index);
        if(sub <= 0) continue;
        const za::SizeT s = static_cast<za::SizeT>(sub);
        if(za::find(subs.begin(), subs.end(), s) == subs.end()) subs.pushBack(s);
    }
}

void prepareBrushModels()
{
    if(vr_hull_brushmodels.value == 0.f) return;
    const double started = Sys_DoubleTime();
    const za::SizeT beforeBytes = built.bytes() + tree.bytes() + monsterTrees.bytes();
    za::Vector<za::SizeT> subs;
    {
        qcvm_t* oldVm = nullptr;
        PR_PushQCVM(&sv.qcvm, &oldVm);
        collidingSubs(subs);
        PR_PopQCVM(oldVm);
    }
    za::Vector<Tree*> sizes;
    if(tree.forClipnodes == built.clipnodes) sizes.pushBack(&tree);
    for(Tree& t : monsterTrees.trees)
    {
        if(t.forClipnodes == built.clipnodes) sizes.pushBack(&t);
    }
    za::Vector<int> counts(sizes.size(), 0);
    jobs::parallelFor(treesSite, sizes.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                Tree& t = *sizes[i];
                if(t.heads.size() < built.subs.size()) t.heads.resize(built.subs.size(), -1);
                for(const za::SizeT sub : subs)
                {
                    if(t.heads[sub] >= 0) continue;
                    buildTree(t, built, sub, nullptr, false);
                    ++counts[i];
                }
            }
        });
    int count = 0;
    for(const int n : counts) count += n;
    if(built.subs.size() == built.worldSubs) // (with an external model's tree in them, a reload gets the world's alone)
    {
        for(Tree* t : sizes)
        {
            checkpoint(*t);
        }
    }
    const double elapsed = Sys_DoubleTime() - started;
    VR_TimeAdd("hull: loaded brush models prepared", elapsed);
    if(vr_hull_audit.value != 0.f)
    {
        Con_Printf("hull_audit: prepared models=%zu sizes=%zu builds=%d bytes_before=%zu bytes_after=%zu ms=%.3f\n",
            subs.size(), sizes.size(), count, beforeBytes, built.bytes() + tree.bytes() + monsterTrees.bytes(), elapsed * 1000.0);
    }
}

// Rebuild nodes independently against the same shared plane table. An isolated model's fresh plane table can
// choose slightly different near-equal planes from the world's table (the builder's existing plane deduplication).
// Do not fill missing cached heads: the test must catch incomplete preparation.
void preloadTest_f()
{
    if(!sv.active || !wanted() || vr_hull_brushmodels.value == 0.f || !worldBrushes(sv.worldmodel)) return;
    settle();
    za::Vector<const Tree*> sizes;
    if(tree.forClipnodes == built.clipnodes) sizes.pushBack(&tree);
    for(const Tree& t : monsterTrees.trees)
    {
        if(t.forClipnodes == built.clipnodes) sizes.pushBack(&t);
    }
    int checked = 0, mismatches = 0, missing = 0;
    za::U32 seed = 17;
    const auto random = [&seed] { seed = seed * 1664525u + 1013904223u; return static_cast<float>(seed >> 8) / 8388608.f - 1.f; };
    za::Vector<za::SizeT> wantedSubs; // (the prepared ones: those that can collide)
    {
        qcvm_t* oldVm = nullptr;
        PR_PushQCVM(&sv.qcvm, &oldVm);
        collidingSubs(wantedSubs);
        PR_PopQCVM(oldVm);
    }
    for(const Tree* cached : sizes)
    {
        for(const za::SizeT sub : wantedSubs)
        {
            if(sub >= cached->heads.size() || cached->heads[sub] < 0) { ++missing; continue; }
            Tree reference;
            reference.ext = cached->ext;
            reference.planes = cached->planes;
            reference.heads.resize(built.subs.size(), -1);
            buildTree(reference, built, sub, nullptr, false);
            const SubModel& sm = built.subs[sub];
            glm::vec3 lo{0.f}, hi{0.f};
            for(za::U32 j = 0; j < sm.numBrushes; ++j)
            {
                const Brush& br = built.brushes[sm.firstBrush + j];
                lo = j ? glm::min(lo, br.mins) : br.mins;
                hi = j ? glm::max(hi, br.maxs) : br.maxs;
            }
            const glm::vec3 centre = (lo + hi) * 0.5f;
            const glm::vec3 reach = (hi - lo) * 0.5f + cached->ext + glm::vec3{16.f};
            for(int n = 0; n < 128; ++n)
            {
                const glm::vec3 start = centre + reach * glm::vec3{random(), random(), random()};
                const glm::vec3 end = centre + reach * glm::vec3{random(), random(), random()};
                const trace_t a = treeTrace(*cached, cached->heads[sub], start, end);
                const trace_t b = treeTrace(reference, reference.heads[sub], start, end);
                bool same = a.startsolid == b.startsolid && a.allsolid == b.allsolid && fabsf(a.fraction - b.fraction) <= 0.00001f;
                for(int axis = 0; axis < 3; ++axis) same = same && fabsf(a.endpos[axis] - b.endpos[axis]) <= 0.0001f;
                if(!same)
                {
                    if(mismatches < 5)
                    {
                        Con_Printf("preloadtest mismatch sub=%zu size=%gx%gx%g fractions=%.9g/%.9g solid=%d,%d/%d,%d "
                            "end_delta=%g,%g,%g\n", sub, cached->ext.x * 2.f, cached->ext.y * 2.f, cached->ext.z * 2.f,
                            a.fraction, b.fraction, a.startsolid, a.allsolid, b.startsolid, b.allsolid,
                            a.endpos[0] - b.endpos[0], a.endpos[1] - b.endpos[1], a.endpos[2] - b.endpos[2]);
                    }
                    ++mismatches;
                }
                ++checked;
            }
        }
    }
    Con_Printf("vr_hull_preloadtest: %s traces=%d missing=%d mismatches=%d\n",
        checked && !missing && !mismatches ? "PASS" : "FAIL", checked, missing, mismatches);
}

// World and loaded brush hulls for these boxes, prepared before play (or after a width-setting change).
void prepare()
{
    if(!wanted() || !worldBrushes(sv.worldmodel))
    {
        return;
    }
    compileTrees(claimTrees(built.clipnodes, wantedExts(false)), built);
    prepareBrushModels();
}

void onWidthChanged(cvar_t*)
{
    prepare();
}

} // namespace

void init()
{
    Cvar_RegisterVariable(&vr_hull_audit);
    Cmd_AddCommand("vr_hull_warmcache", warmCache_f);
    Cmd_AddCommand("vr_hull_cachetest", cacheTest_f);
    Cmd_AddCommand("vr_hull_preloadtest", preloadTest_f);
    Cmd_AddCommand("vr_hull_stats", stats_f);
    Cmd_AddCommand("vr_hull_keeptest", keepTest_f);
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
    Cvar_SetCallback(&vr_hull_brushmodels, onWidthChanged);
    Cvar_SetCallback(&vr_mhull, onWidthChanged);
    Cvar_SetCallback(&vr_crouch_hull, onWidthChanged);
    Cvar_SetCallback(&vr_crouch_height, onWidthChanged);
    Cvar_SetCallback(&vr_crouch_step, onWidthChanged);
    Cmd_AddCommand("vr_crouch_status", crouchStatus_f);
    Cvar_SetCallback(&vr_hull_keep, onKeepChanged);
    for(const MonsterClass& c : monsterClasses)
    {
        Cvar_SetCallback(c.width, onWidthChanged);
    }
}

void beforeLoad()
{
    settle();
    for(float& c : crouchBox)
    {
        c = 0.f; // (each map starts standing)
    }
    qmodel_t* world = sv.worldmodel;
    if(!wanted() || world->type != mod_brush || world->numnodes <= 0 || built.clipnodes == world->hulls[0].clipnodes)
    {
        return;
    }
    hullAudit = HullAudit{};
    pending.posted = Sys_DoubleTime();
    // The last load's of the same map (keepForReload), else built on the pool (the trees found are not built again:
    // claimTrees passes them over).
    const za::U64 key = keyOf(world);
    pending.kept = takeKept(world, key);
    trimKept();
    VR_TimeAdd("hull: kept maps looked up", Sys_DoubleTime() - pending.posted);
    monsterTrees.trees.reserve(maxMonsterTrees); // (a slot taken while others are made moves none of them)
    pending.brushes = pending.kept ? jobs::async([] {}) : jobs::async([world, key] { build(built, world, key); });
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
    const za::Vector<glm::vec3> exts = wantedExts(true);
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

void keepForReload()
{
    settle();
    Brushes& b = built;
    if(keepLimit() == 0 || !b.clipnodes || !b.key || b.worldSubs == 0)
    {
        return;
    }
    const mclipnode_t* clipnodes = b.clipnodes;
    za::Vector<KeptMap>& maps = kept.maps;
    for(za::SizeT i = maps.size(); i-- > 0;)
    {
        if(maps[i].key == b.key)
        {
            maps.erase(maps.begin() + i);
        }
    }
    KeptMap k;
    k.key = b.key;
    k.brushes = ZA_MOVE(b);
    cutBack(k.brushes);
    Tree& p = tree;
    if(p.forClipnodes == clipnodes && keepable(p))
    {
        k.player = ZA_MOVE(p);
        cutBack(k.player);
    }
    for(Tree& t : monsterTrees.trees)
    {
        if(t.forClipnodes == clipnodes && keepable(t))
        {
            k.monsters.pushBack(ZA_MOVE(t));
            cutBack(k.monsters.back());
        }
    }
    maps.pushBack(ZA_MOVE(k)); // (trimmed at the next load, once it has looked for its own: beforeLoad)
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
    const float crouch = crouchTop(passedict);
    if((vr_hull_width.value <= 0.f && crouch <= 0.f) || !sv.active || !sv.worldmodel || !isPlayerBox(passedict, mins, maxs))
    {
        return false;
    }
    const float half = (vr_hull_width.value > 0.f ? widthSetting() : 32.f) * 0.5f;
    const float cx = (mins[0] + maxs[0]) * 0.5f, cy = (mins[1] + maxs[1]) * 0.5f;
    boxMins[0] = cx - half;
    boxMins[1] = cy - half;
    boxMins[2] = mins[2];
    boxMaxs[0] = cx + half;
    boxMaxs[1] = cy + half;
    boxMaxs[2] = mins[2] + (crouch > 0.f ? crouch : hull1Height(sv.worldmodel)); // hull 1's height (or
    // the crouched box's), from the box's feet as Quake places it
    return true;
}

bool entBox(const edict_t* passedict, const float* mins, const float* maxs, float* boxMins, float* boxMaxs)
{
    float width = entWidthSetting();
    const float crouch = crouchTop(passedict);
    if(sv.active && isPlayerBox(passedict, mins, maxs) && (width > 0.f || crouch > 0.f))
    {
        narrowBox(mins, maxs, width > 0.f ? width : 32.f, boxMins, boxMaxs);
        if(crouch > 0.f)
        {
            boxMaxs[2] = za::min(boxMaxs[2], mins[2] + crouch); // crouched (vr_crouch_hull)
        }
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
    const float crouch = crouchTop(touch);
    if(sv.active && isPlayerBox(touch, touch->v.mins, touch->v.maxs) && crouch > 0.f)
    {
        // Crouched (vr_crouch_hull): a body meets his crouched box, narrowed as below if it would be.
        narrowBox(touch->v.mins, touch->v.maxs, width > 0.f && categoryOn(mover) ? width : 32.f, boxMins, boxMaxs);
        boxMaxs[2] = za::min(boxMaxs[2], touch->v.mins[2] + crouch);
        return true;
    }
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

bool hitBox(const edict_t* touch, float* boxMins, float* boxMaxs, bool projectile)
{
    const float width = hitWidthSetting();
    if(!sv.active || !isPlayerBox(touch, touch->v.mins, touch->v.maxs))
    {
        return false;
    }
    // A bullet or missile only up to the top of his head (vr_hull_hit_head; NOTES.md e1m1_2026-10-01_02-50-03): crouched
    // behind a crate, the box stood as tall as standing, and the shots over the crate and his head hit him. Not a
    // monster's look whether it can hit him (CheckAttack's trace to his box's eyes, which then never met him crouched:
    // it never fired). (Not lower than 24 units: a head on the floor is a mock's or a glitch's.)
    float top = touch->v.maxs[2];
    const int headOfs = progs::fields().headpos;
    if(projectile && vr_hull_hit_head.value != 0.f && headOfs >= 0)
    {
        const glm::vec3 head = progs::fieldVec(const_cast<edict_t*>(touch), headOfs);
        if(head != glm::vec3{0.f})
        {
            top = za::clamp(head.z + headTop - touch->v.origin[2], touch->v.mins[2] + 24.f, touch->v.maxs[2]);
        }
    }
    const float crouch = crouchTop(touch);
    if(projectile && crouch > 0.f)
    {
        top = za::min(top, touch->v.mins[2] + crouch); // no higher than his crouched box
    }
    if(width <= 0.f && top >= touch->v.maxs[2])
    {
        return false;
    }
    if(width > 0.f)
    {
        narrowBox(touch->v.mins, touch->v.maxs, width, boxMins, boxMaxs);
    }
    else
    {
        for(int i = 0; i < 3; ++i)
        {
            boxMins[i] = touch->v.mins[i];
            boxMaxs[i] = touch->v.maxs[i];
        }
    }
    boxMaxs[2] = top;
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
        const Tree& t = treeFor(built, static_cast<za::SizeT>(sub), (hi - lo) * 0.5f, model->name);
        trace = treeTrace(t, t.heads[static_cast<za::SizeT>(sub)], s + centre, e + centre);
        for(int i = 0; i < 3; ++i)
        {
            trace.endpos[i] += origin[i] - centre[i];
        }
        return true;
    }
    const SubModel& sm = built.subs[static_cast<za::SizeT>(sub)];
    trace = boxTrace(built, model->hulls[0], sm.head, s, lo, hi, e, sm.base).trace;
    for(int i = 0; i < 3; ++i)
    {
        trace.endpos[i] += origin[i];
    }
    return true;
}

// A clipped box changes topology only when a corner crosses the portal. Trace each linear interval.
bool clipPortal(const edict_t* ent, const float* start, const float* mins, const float* maxs, const float* end,
                const float* plane, trace_t& trace)
{
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 a{start[0], start[1], start[2]}, b{end[0], end[1], end[2]};
    const glm::vec3 lo{mins[0], mins[1], mins[2]}, hi{maxs[0], maxs[1], maxs[2]};
    const glm::vec3 n{plane[0], plane[1], plane[2]};
    float localPlane[4]{n.x, n.y, n.z, plane[3] - glm::dot(n, origin)};
    float cuts[10]{0.f, 1.f};
    int count = 2;
    const float travel = glm::dot(n, b - a);
    if(za::fabs(travel) > 1e-6f)
    {
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 corner{(c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z};
            const float t = (plane[3] - glm::dot(n, a + corner)) / travel;
            if(t > 0.f && t < 1.f) { cuts[count++] = t; }
        }
    }
    za::quickSort(cuts, cuts + count);
    const int index = static_cast<int>(ent->v.modelindex);
    qmodel_t* model =
        static_cast<int>(ent->v.solid) == SOLID_BSP && index >= 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int sub = model && worldBrushes(sv.worldmodel) ? subOf(built, index) : -1;
    if(model && sub < 0) { return false; }
    Plane box[6]{};
    Brush br{};
    br.count = 6;
    br.mins = origin + glm::vec3{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]};
    br.maxs = origin + glm::vec3{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
    for(int i = 0; i < 6; i++)
    {
        const int axis = i / 2;
        box[i].normal[axis] = (i & 1) ? -1.f : 1.f;
        box[i].dist = (i & 1) ? -br.mins[axis] : br.maxs[axis];
        box[i].grows = 1.f;
    }
    memset(&trace, 0, sizeof(trace));
    trace.fraction = 1.f;
    VectorCopy(end, trace.endpos);
    for(int k = 0; k < count - 1; k++)
    {
        if(cuts[k + 1] < cuts[k] + 1e-7f && a != b) { continue; }
        const glm::vec3 from = glm::mix(a, b, cuts[k]), to = glm::mix(a, b, cuts[k + 1]);
        trace_t tr{};
        tr.fraction = 1.f;
        if(model)
        {
            const SubModel& sm = built.subs[static_cast<za::SizeT>(sub)];
            tr = boxTrace(built, model->hulls[0], sm.head, from - origin, lo, hi, to - origin, sm.base, localPlane)
                     .trace;
        }
        else
        {
            const glm::dvec3 centre = (glm::dvec3{lo} + glm::dvec3{hi}) * 0.5;
            Sweep sweep{};
            sweep.start = glm::dvec3{from} + centre;
            sweep.end = glm::dvec3{to} + centre;
            sweep.ext = (glm::dvec3{hi} - glm::dvec3{lo}) * 0.5;
            sweep.sweepMins = glm::min(from, to) + lo - 1.f;
            sweep.sweepMaxs = glm::max(from, to) + hi + 1.f;
            sweep.halfspace = plane;
            sweep.boxPlanes = box;
            clipToBrush(sweep, br);
            tr.startsolid = sweep.startsolid;
            tr.allsolid = sweep.startsolid && !sweep.getout;
            tr.fraction = static_cast<float>(sweep.fraction);
            if(sweep.hitPlane)
            {
                VectorCopy(sweep.hitPlane->normal, tr.plane.normal);
                tr.plane.dist = static_cast<float>(sweep.hitPlane->dist);
            }
        }
        if(k == 0)
        {
            trace.startsolid = tr.startsolid;
            trace.allsolid = tr.allsolid;
        }
        if(tr.fraction < 1.f || (k > 0 && tr.startsolid))
        {
            const float f = tr.startsolid ? 0.f : tr.fraction;
            trace.fraction = cuts[k] + (cuts[k + 1] - cuts[k]) * f;
            trace.plane = tr.plane;
            trace.ent = const_cast<edict_t*>(ent);
            const glm::vec3 at = glm::mix(a, b, trace.fraction);
            VectorCopy(at, trace.endpos);
            return true;
        }
    }
    if(trace.startsolid) { trace.ent = const_cast<edict_t*>(ent); }
    return true;
}

int playerBoxFits(qmodel_t* world, const glm::vec3& start, const glm::vec3& end)
{
    const float crouch = crouchTopNum(1); // (the local player: the client's lean)
    if((vr_hull_width.value <= 0.f && crouch <= 0.f) || !world || world != sv.worldmodel || !sv.active)
    {
        return -1;
    }
    const Brushes* b = worldBrushes(world);
    if(!b)
    {
        return -1;
    }
    const float half = (vr_hull_width.value > 0.f ? widthSetting() : 32.f) * 0.5f;
    const glm::vec3 lo{-half, -half, -24.f}, hi{half, half, -24.f + (crouch > 0.f ? crouch : hull1Height(world))};
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

void updateCrouch(edict_t* ent, const glm::vec3& head)
{
    const int num = clientNum(ent);
    if(num < 1 || num >= crouchSlots)
    {
        return;
    }
    float& c = crouchBox[num];
    if(!crouchOn() || !sv.active || !sv.worldmodel || ent->free || head == glm::vec3{0.f})
    {
        c = 0.f;
        return;
    }
    const float eye = head.z - (ent->v.origin[2] + ent->v.mins[2]);
    const float want = crouchLevelFor(sv.worldmodel, eye);
    if(want > 0.f && (c <= 0.f || want <= c))
    {
        c = want; // lower: always (a smaller box fits wherever the taller one does)
        return;
    }
    if(c <= 0.f || eye < c + crouchStandMargin)
    {
        return; // standing, or not risen clear of his box's top yet
    }
    // Taller: standing if his eyes ask for it and it fits, else the tallest crouched box that fits up to the one they ask
    // for; else he keeps his box (under a low ceiling).
    if(want <= 0.f && boxFitsAt(ent, num, 0.f))
    {
        c = 0.f;
        return;
    }
    float levels[maxCrouchLevels];
    for(int i = crouchLevels(sv.worldmodel, levels); i-- > 0;)
    {
        if(levels[i] > c && (want <= 0.f || levels[i] <= want) && boxFitsAt(ent, num, levels[i]))
        {
            c = levels[i];
            return;
        }
    }
}

bool isCrouched(const edict_t* ent)
{
    return crouchTop(ent) > 0.f;
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
    ent->v.velocity[0] = za::cos(w.yaw) * speed;
    ent->v.velocity[1] = za::sin(w.yaw) * speed;
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

// SV_UserFriction's ledge test (double friction when the floor drops away ahead): Quake looks down 34 units from a point
// 16 ahead of the box's centre at its feet (the leading edge of its 32 box). With vr_hull_edge_probe (on), from the
// narrow box's own leading edge (half its width ahead), and a point that starts inside solid has floor under it (Quake
// called it a drop: a trace starting in solid goes nowhere). Stairs with a clip brush ramp laid over them (MG1's
// start): the feet ride the ramp at the steps' noses (Quake's box exactly on them, a narrower box, standing lower on a
// slope, under them), so that point was inside the next step: double friction every frame, walking up slowed to a crawl,
// and a jump against the steps went nowhere (ROUND21.md, "Stuck on stairs, a fiend stuck on a bridge").
extern "C" int VR_HullOverDropoff(edict_t* ent, const float* origin, const float* vel, float speed)
{
    const bool fix = qvr::vr_hull_edge_probe.value != 0.f;
    float edge = 16.f;
    float lo[3], hi[3];
    if(fix && qvr::hull::moveBox(ent, ent->v.mins, ent->v.maxs, lo, hi))
    {
        edge = za::min(16.f, (hi[0] - lo[0]) * 0.5f);
    }
    vec3_t start, stop;
    start[0] = stop[0] = origin[0] + vel[0] / speed * edge;
    start[1] = stop[1] = origin[1] + vel[1] / speed * edge;
    start[2] = origin[2] + ent->v.mins[2];
    stop[2] = start[2] - 34.f;
    const trace_t trace = SV_Move(start, vec3_origin, vec3_origin, stop, MOVE_NOMONSTERS, ent);
    const bool drop = trace.fraction == 1.f && !(fix && trace.startsolid);
    if(qvr::vr_debug_walkmove.value && NUM_FOR_EDICT(ent) == 1)
    {
        Con_Printf("walkmove ledge test %.1f ahead from %.2f %.2f %.2f: %.3f%s%s: %s\n", edge, start[0], start[1],
            start[2], trace.fraction, trace.startsolid ? ", start solid" : "", trace.allsolid ? ", all solid" : "",
            drop ? "a drop (double friction)" : "floor");
    }
    return drop;
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

extern "C" int VR_HullHitBox(edict_t* touch, float* boxmins, float* boxmaxs, int projectile)
{
    return qvr::hull::hitBox(touch, boxmins, boxmaxs, projectile != 0);
}

extern "C" int VR_HullClipPortal(edict_t* ent, const float* start, const float* mins, const float* maxs,
    const float* end, const float* plane, trace_t* trace)
{
    return qvr::hull::clipPortal(ent,start,mins,maxs,end,plane,*trace);
}
