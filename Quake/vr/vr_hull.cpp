// vr_hull.cpp -- see vr_hull.hpp.

#include "vr_hull_internal.hpp"
#include "vr_alloccount.hpp"
#include "vr_brushes.hpp"
#include "vr_diskcache.hpp"
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
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
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
#include "vr_srvrandom.hpp"

#include <atomic>
#include <string.h>
namespace qvr::hull
{
using namespace detail;

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
constexpr float headTop = 5.f; // units: the top of a player's head over the headset's middle (hitBox; QC VR_SHOT_OVER_HEAD)

// Its heap bytes (a kept map's: KeptMap).

// ---------------------------------------------------------------------------------------------------------------
// The build: hull 0's tree walked from each model's head node, a convex polytope (faces with their windings)
// split by each node's plane on the way down; at a solid (or sky: solid to Quake's clipping hulls) leaf what is
// left is that leaf's brush.

// A face's points, in place up to 5 (PROFILING_2026-10.md, "Hull build: the containers": a winding starts as 4 points
// and gains one at most a cut; measured on warden, ad_grendel, e4m7 and e1m1: 4.1-4.6 points on average, 9 at the 99th
// percentile, 24 at most). Over 5 they spill to the heap. It was 8 (98.5% of warden's and 99.97% of the others' fit), but
// the pieces held at once are most of a big map's build memory, a face's in place points most of a piece's bytes:
// vrstart's 7 trees' pieces at once 1.06 to 0.86 GB, no slower (ROUND21.md, "The hull build's memory").
constexpr za::SizeT windingInline = 5;
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

// `in` split by a plane into its front's part and its back's (as clipWinding keeping each side: the same points), from
// its points' distances to the plane (dists: dot(n, p) - d, as clipWinding computes them), worked out once for both.
void splitWinding(const Winding& in, const double* dists, Winding& front, Winding& back)
{
    front.clear();
    back.clear();
    const za::SizeT count = in.size();
    if(count < 3)
    {
        return;
    }
    za::SmallVector<int, 64> sides;
    sides.resize(count);
    bool anyBack = false, anyFront = false;
    for(za::SizeT i = 0; i < count; ++i)
    {
        sides[i] = dists[i] > onEpsilon ? 1 : (dists[i] < -onEpsilon ? -1 : 0);
        anyFront = anyFront || sides[i] > 0;
        anyBack = anyBack || sides[i] < 0;
    }
    if(!anyBack || !anyFront)
    {
        // all on one side (or on the plane): that side keeps it whole, the other gets nothing (on the plane: both whole)
        if(!anyBack)
        {
            front = in;
        }
        if(!anyFront)
        {
            back = in;
        }
        return;
    }
    // (the back's distances are the front's negated: the same sides turned, the same crossing points, as
    // (-a) / ((-a) - (-b)) is a / (a - b) exactly)
    for(za::SizeT i = 0; i < count; ++i)
    {
        const za::SizeT j = (i + 1) % count;
        if(sides[i] >= 0)
        {
            front.pushBack(in[i]);
        }
        if(sides[i] <= 0)
        {
            back.pushBack(in[i]);
        }
        if(sides[i] == 0 || sides[j] == 0 || sides[i] == sides[j])
        {
            continue;
        }
        const double t = dists[i] / (dists[i] - dists[j]);
        const glm::dvec3 p = in[i] + (in[j] - in[i]) * t;
        front.pushBack(p);
        back.pushBack(p);
    }
    if(front.size() < 3)
    {
        front.clear();
    }
    if(back.size() < 3)
    {
        back.clear();
    }
}

// `p` split by the plane: front (dot(n, x) >= d) and back. A side the plane leaves nothing of stays empty. Returns the
// side p went to whole (0 front, 1 back; -1: cut, or nothing of it), and its points' distances' range (tlo, thi).
int splitPoly(Poly&& p, const glm::dvec3& n, double d, Poly& front, Poly& back, double& tlo, double& thi)
{
    front.clear();
    back.clear();
    // The points' distances, face after face (in place for the usual pieces: 6-8 faces of 4-5 points), kept for the
    // faces' splits.
    za::SmallVector<double, 96> dists;
    double lo = 1e300, hi = -1e300;
    // The faces each side can get (all but those splitWinding leaves wholly to the other side): what each side's Poly
    // is given room for (not all of p's: the pieces held at once are most of the build's memory).
    za::SizeT frontFaces = 0, backFaces = 0;
    for(const Face& f : p)
    {
        double flo = 1e300, fhi = -1e300;
        for(const glm::dvec3& v : f.w)
        {
            const double t = glm::dot(n, v) - d;
            dists.pushBack(t);
            flo = za::min(flo, t);
            fhi = za::max(fhi, t);
        }
        lo = za::min(lo, flo);
        hi = za::max(hi, fhi);
        const bool whole = f.w.size() >= 3;
        frontFaces += !(whole && flo < -onEpsilon && fhi <= onEpsilon);
        backFaces += !(whole && fhi > onEpsilon && flo >= -onEpsilon);
    }
    tlo = lo;
    thi = hi;
    if(lo > hi) // no points at all: nothing left
    {
        return -1;
    }
    if(lo >= -onEpsilon)
    {
        front = ZA_MOVE(p);
        return 0;
    }
    if(hi <= onEpsilon)
    {
        back = ZA_MOVE(p);
        return 1;
    }
    // The cap: the plane's winding clipped by each face in turn (two windings, each clip into the other).
    Winding caps[2] = {baseWinding(n, d), {}};
    int cap = 0;
    front.reserve(frontFaces + 1); // (and the cap: references stay valid)
    back.reserve(backFaces + 1);
    const double* at = dists.data();
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
        // Each side's face split straight into its place, taken back if nothing is left of it, unless nothing is left
        // on both sides (a sliver of a face lost to the epsilon): its plane still bounds both pieces. Dropped, a piece
        // could lose its only bound that way (found with a monster's 24-wide hull on e1m4: a piece reaching to the bogus
        // winding's end, its leaf solid out in the open).
        Face& ff = front.emplaceBack();
        ff.normal = f.normal;
        ff.dist = f.dist;
        ff.tag = f.tag;
        Face& bf = back.emplaceBack();
        bf.normal = f.normal;
        bf.dist = f.dist;
        bf.tag = f.tag;
        splitWinding(f.w, at, ff.w, bf.w);
        at += f.w.size();
        const bool frontLeft = !ff.w.empty(), backLeft = !bf.w.empty();
        if(frontLeft != backLeft)
        {
            (frontLeft ? back : front).popBack();
        }
    }
    // The cap's plane bounds both pieces even when its face is lost to the epsilon.
    front.pushBack(Face{-n, -d, caps[cap]});
    back.pushBack(Face{n, d, ZA_MOVE(caps[cap])});
    return -1;
}

void splitPoly(Poly&& p, const glm::dvec3& n, double d, Poly& front, Poly& back)
{
    double tlo, thi;
    (void)splitPoly(ZA_MOVE(p), n, d, front, back, tlo, thi);
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
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

// What the map's brushes and compiled hulls are made from, hashed (they are kept for a load of a map with the same:
// keepForReload): hull 0's nodes (the brushes' walk), hull 1's (the clip brushes'), the planes, the models' heads and
// bounds, and the build's version (to be bumped when a change to the build changes its results).
constexpr za::U64 buildVersion = 1;
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

} // namespace
namespace detail
{
float widthSetting()
{
    return za::clamp(vr_hull_width.value, minWidth, maxWidth);
}
} // namespace detail
namespace
{

// The server's map as brushes, built now if not yet (the setting turned on during a map: a hitch, once).
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

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

} // namespace
namespace detail
{
double support(const Plane& p, const glm::dvec3& ext)
{
    return p.grows * (za::abs(p.normal.x * ext.x) + za::abs(p.normal.y * ext.y) + za::abs(p.normal.z * ext.z)) +
           p.growsZ * za::abs(p.normal.z * ext.z);
}
} // namespace detail
namespace
{

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

} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

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
} // namespace
namespace detail
{
bool boxInSolid(const Brushes& b, const hull_t& hull, int head, const glm::dvec3& p, const glm::dvec3& ext,
    bool clips, za::SizeT base)
{
    ++b.stamp;
    return boxInTree(b, hull, head, p, ext, clips && head == 0 && base == 0 && !b.leafClipStart.empty(), base);
}
} // namespace detail
namespace
{

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
        // Kept in the box centre's space; the open faces shrink back with a narrower box. The others lie against more
        // of hull 1's solid (a floor's, a ceiling's, a wall's, grown by Quake's box): a shorter box (crouched) meets that
        // floor's or ceiling's face nearer its own, so those faces move with its height as the floor's does (their
        // height's part of Quake's box out, the box's in). Kept where they were, a crouched box's centre went under a
        // clip brush lying on a floor (MG1's start: the clip ramp over the west stairs, walked under to the first step's
        // riser; a jump there hit the ramp's underside: NOTES.md start_2026-10-09_17-58-56). Width stays as it was.
        // Only where hull 1 stays solid twice the most it moves (16) past the face: a floor's or ceiling's grown slab.
        // A cut inside a slab near its other side (the stairs' clip brush under their landing) stays: moved, it rose out
        // of the landing's floor, and a crouched box stood 4 units over it.
        const za::SizeT first = out.planes.size();
        bool heightMoves = false;
        auto solidPast = [&h1, head](const Face& f)
        {
            glm::dvec3 fc{0.0};
            for(const glm::dvec3& v : f.w)
            {
                fc += v;
            }
            fc /= static_cast<double>(f.w.size());
            auto solidAt = [&](const glm::dvec3& p)
            {
                const glm::dvec3 q = p + f.normal * 32.0;
                vec3_t qf{static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z)};
                return SV_HullPointContents(&h1, head, qf) == CONTENTS_SOLID;
            };
            bool solid = solidAt(fc);
            for(za::SizeT i = 0; i < f.w.size() && solid; ++i)
            {
                solid = solidAt(fc + (f.w[i] - fc) * 0.85);
            }
            return solid;
        };
        for(za::SizeT fi = 0; fi < piece.size(); ++fi)
        {
            const Face& f = piece[fi];
            const double dist = f.dist + glm::dot(f.normal, lift);
            const double reach =
                za::abs(f.normal.x) * e32.x + za::abs(f.normal.y) * e32.y + za::abs(f.normal.z) * e32.z;
            if(open[fi])
            {
                out.planes.pushBack(Plane{glm::vec3{f.normal}, static_cast<float>(dist - reach), 1.f});
                continue;
            }
            const double reachZ = za::abs(f.normal.z) * e32.z;
            if(reachZ <= 0.01 || f.w.empty() || !solidPast(f))
            {
                out.planes.pushBack(Plane{glm::vec3{f.normal}, static_cast<float>(dist), 0.f});
                continue;
            }
            heightMoves = true;
            out.planes.pushBack(Plane{glm::vec3{f.normal}, static_cast<float>(dist + reachZ), 0.f, -1.f});
        }
        // (Its bounds then hold a shorter box's centres too: the lowest crouched box is 24 tall, 16 under Quake's half
        // height; a face moved that much down or up takes its corners along the faces beside it, twice as far for a
        // 26-degree ramp.)
        const glm::dvec3 grow = heightMoves ? glm::dvec3{32.0, 32.0, 16.0} : glm::dvec3{0.0};
        out.brushes.pushBack(Brush{static_cast<za::U32>(first), static_cast<za::U32>(out.planes.size() - first),
            glm::vec3{lo + lift - grow}, glm::vec3{hi + lift + grow}, true});
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

// A box (mins..maxs about the point) from start to end through a model's brushes, in its own space; Quake's trace.
} // namespace
namespace detail
{
Result boxTrace(const Brushes& b, const hull_t& hull0, int head, const glm::vec3& start, const glm::vec3& mins,
    const glm::vec3& maxs, const glm::vec3& end, za::SizeT base, const float* halfspace)
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
} // namespace detail
namespace
{

// ---------------------------------------------------------------------------------------------------------------
// Method A (the moves' only since 2026-10-16, the brush sweep's switch vr_hull_method retired; boxTrace, the sweep,
// stays for the portals' clipping and as vr_hull_bench's reference): a clipping hull compiled for the box at load, as
// qbsp compiles hull 1. Each of a model's
// brushes (hull 0's, with their bevels, and the world's recovered clip brushes) is grown by the box: its planes moved
// out by the box's reach, which with Quake 2's bevels is exactly the brush's Minkowski sum with the box. A BSP tree is
// compiled from the grown brushes, their faces the splitting planes: a leaf is solid where one grown brush holds all of
// it (every face of that brush already split on, the leaf on its inside), empty where none reaches. Being Quake's own
// clipnodes and planes, it is traced by SV_RecursiveHullCheck exactly as hull 1 is (in the box centre's space).

long long planeKey(float d)
{
    return static_cast<long long>(za::floor(d * 4.f));
}

// Its bytes, for a set holding trees (found by the set's heldBytes of a vector of them).

cvar_t vr_hull_audit = {"vr_hull_audit", "0", CVAR_NONE};

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
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

// The world's own brushes (external models' let go), its pointers cleared (kept).
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

} // namespace
namespace detail
{
za::SizeT keepLimit()
{
    return static_cast<za::SizeT>(za::clamp(static_cast<int>(vr_hull_keep.value), 0, 8));
}
} // namespace detail
namespace
{

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
        : base_{&base}, baseCount_{base.count()}, root_{base.root_ ? base.root_ : &base},
          rootCount_{base.root_ ? base.rootCount_ : base.count()}, planes_{&planes}, nodes_{&nodes}, solid_{&solid}, empty_{&empty},
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
        if(u < rootCount_)
        {
            return (*root_->planes_)[u]; // (the tree's own: straight there, not down the chain of bases)
        }
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
        // An axial plane (its normal exactly an axis: the table's are snapped) gives each point's distance as its
        // coordinate less d, so a piece's range of them is its bounds' (finish's, of the same points) less d, the same
        // numbers splitPoly works out point by point: a piece all on one side goes there whole without that pass.
        const int axis = mp.type < 3 ? mp.type : -1;
        for(Frag& f : frags)
        {
            double tlo, thi;
            int whole;
            if(axis >= 0 && ((tlo = f.lo[axis] - d) >= -onEpsilon || (thi = f.hi[axis] - d) <= onEpsilon))
            {
                thi = f.hi[axis] - d;
                whole = tlo >= -onEpsilon ? 0 : 1;
                parts[whole] = ZA_MOVE(f.poly);
                parts[whole ^ 1].clear();
            }
            else
            {
                whole = splitPoly(ZA_MOVE(f.poly), n, d, parts[0], parts[1], tlo, thi);
            }
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
                const bool kept = whole == side ? finishWhole(g, f, split, side ? -tlo : thi) :
                                                  finish(g, split, n, d, side ? -1.0 : 1.0);
                if(kept && bounded(g))
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

    // Its leaf counts, read and put back (the merge's redo of a unit above a failed one: emit).
    void leafCounts(int& solid, int& empty) const
    {
        solid = *solid_;
        empty = *empty_;
    }
    void setLeafCounts(int solid, int empty)
    {
        *solid_ = solid;
        *empty_ = empty;
    }

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

    // finish for a piece the split left whole (g: f's faces, on the side `reach` is the furthest of their points from
    // the plane, its sign turned to that side): f's bounds (its points' own: the same numbers finish gets), the rest as
    // finish does it.
    static bool finishWhole(Frag& g, const Frag& f, int split, double reach)
    {
        g.lo = f.lo;
        g.hi = f.hi;
        g.live = 0;
        for(Face& face : g.poly)
        {
            if(face.tag == split)
            {
                face.tag = -1;
            }
            g.live += face.tag >= 0;
        }
        reach = za::max(0.0, reach);
        return g.lo.x <= g.hi.x && g.hi.x - g.lo.x >= 0.01 && g.hi.y - g.lo.y >= 0.01 && g.hi.z - g.lo.z >= 0.01 &&
               reach >= 0.01;
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
        // The axial planes first, each group in the order met (a stable partition, in two passes over a copy: in place
        // it rotated blocks, N log N moves for every node of the tree)
        sorted_.clear();
        for(int axial = 1; axial >= 0; --axial)
        {
            for(const Cand& c : cands_)
            {
                if((planeAt(c.tag).type < 3) == (axial != 0))
                {
                    sorted_.pushBack(c);
                }
            }
        }
        cands_.swap(sorted_);
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
    const TreeBuilder* root_ = nullptr; // (a builder on the pool) the tree's own builder, at the end of the bases
    za::SizeT rootCount_ = 0;         // ... its planes (unchanged while this one works)
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
    za::Vector<Cand> sorted_; // (choose) cands_ with the axial planes first
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
// - Below the top's levels a unit of many pieces (shareBig or more) is split further, down to shareDeepest: a tree's
//   splits are often far from balanced (vrstart's 28x56 tree: a unit of 20,423 pieces at the top's last level, 9.5 s of
//   its 10.9 alone). Those deeper units keep no input (a copy of their pieces at every level would cost gigabytes): one
//   whose check fails fails the unit above it, up to the nearest that kept its input (a unit of the top's last level,
//   or a subtree of the top's levels), which is then done again on the merging thread as before (the same work as
//   without the deeper split, at worst). The splits of the top's levels above the last keep none either, but the top
//   (speculate).
// - What is kept is packed (PackedFrags: about half a Face's bytes), and the pieces of all the trees compiled at once are
//   held within a memory budget (gatedCompile): vrstart's first build peaked at 10.9 GB of working set, now about 3
//   (ROUND21.md, "The hull build's memory").
constexpr int shareDepth = 10;         // the top's levels: up to 1024 subtrees
constexpr za::SizeT shareMin = 48;   // fewer pieces: a subtree of their own (not split further here)
constexpr za::SizeT shareBig = 512;  // below the top's levels: this many pieces or more split further
constexpr int shareDeepest = -24;    // ... down to this depth (the top's levels count down to 0)

// A unit's pieces kept for a redo (emit), packed: each face's points as many as it has (a Face holds windingInline in
// place, 4.5 used on average), the same numbers, unpacked into the same pieces.
struct PackedFrags
{
    struct Piece
    {
        glm::dvec3 lo, hi;
        const Brush* brush;
        int live;
        za::U32 faces;
    };
    struct Side
    {
        glm::dvec3 normal;
        double dist;
        int tag;
        za::U32 points;
    };
    za::Vector<Piece> pieces;
    za::Vector<Side> faces;
    za::Vector<glm::dvec3> points;
};

PackedFrags pack(const Frags& frags)
{
    PackedFrags p;
    za::SizeT faces = 0, points = 0;
    for(const Frag& f : frags)
    {
        faces += f.poly.size();
        for(const Face& face : f.poly)
        {
            points += face.w.size();
        }
    }
    p.pieces.reserve(frags.size());
    p.faces.reserve(faces);
    p.points.reserve(points);
    for(const Frag& f : frags)
    {
        p.pieces.pushBack(PackedFrags::Piece{f.lo, f.hi, f.brush, f.live, static_cast<za::U32>(f.poly.size())});
        for(const Face& face : f.poly)
        {
            p.faces.pushBack(PackedFrags::Side{face.normal, face.dist, face.tag, static_cast<za::U32>(face.w.size())});
            for(const glm::dvec3& v : face.w)
            {
                p.points.pushBack(v);
            }
        }
    }
    return p;
}

Frags unpack(const PackedFrags& p)
{
    Frags frags;
    frags.reserve(p.pieces.size());
    za::SizeT face = 0, point = 0;
    for(const PackedFrags::Piece& piece : p.pieces)
    {
        Frag f;
        f.lo = piece.lo;
        f.hi = piece.hi;
        f.brush = piece.brush;
        f.live = piece.live;
        f.poly.resize(piece.faces);
        for(Face& out : f.poly)
        {
            const PackedFrags::Side& side = p.faces[face++];
            out.normal = side.normal;
            out.dist = side.dist;
            out.tag = side.tag;
            out.w = Winding(p.points.data() + point, p.points.data() + point + side.points);
            point += side.points;
        }
        frags.pushBack(ZA_MOVE(f));
    }
    return frags;
}

struct Unit
{
    const Unit* parent = nullptr;    // the unit whose split made its pieces (none: the tree's own table is its base)
    za::SizeT baseCount = 0;       // the planes it saw (its base's)
    za::Vector<mplane_t> planes;    // those it added (numbered from baseCount)
    za::Vector<PlaneAsk> log;
    za::Vector<int> map;            // its planes' numbers in the tree's table (the merge)
    int solid = 0, empty = 0, rebounded = 0;
    // A node's: a leaf (kind 0: root its contents), a split (kind 1: split, kids), a subtree (kind 2: root and nodes,
    // numbered from 0); input: its pieces (done again from them if the merge's check fails), packed, kept only by the
    // top, the top's last level and the subtrees of the top's levels (kept).
    int kind = 0;
    bool kept = false;
    int root = 0;
    int split = 0;
    za::UniquePtr<Unit> kids[2]{nullptr, nullptr};
    za::Vector<mclipnode_t> nodes;
    PackedFrags input;
    double ms = 0.0;      // (kind 2) its build's time, and its pieces (the tree's report)
    za::SizeT pieces = 0;
};

// The merge's state: which unit's plane each plane added to the tree's table since `start` is.
struct Merge
{
    za::SizeT start = 0;
    za::Vector<const Unit*> owner; // [plane - start]
    int redone = 0;                 // units done again on the merging thread
    // (buildModelsAtOnce: a model's unit, its base the tree's table before any model) a plane it added may be one an
    // earlier model added since, of exactly the same values: the one-thread build finds that one, and each later ask
    // is checked against it all the same.
    bool reuse = false;

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
            // (reuse: or one an earlier model added, not given to another of its planes)
            const bool earlier = reuse && static_cast<za::SizeT>(got) < start && static_cast<za::SizeT>(got) >= u.baseCount;
            if((static_cast<za::SizeT>(got) < start && !earlier) || (!earlier && mine(got, u)) ||
                !same(tb.planeAt(got), u.planes[id - u.baseCount]))
            {
                return false;
            }
            if(earlier)
            {
                for(const int other : u.map)
                {
                    if(other == got)
                    {
                        return false;
                    }
                }
            }
            to = got;
            if(!earlier)
            {
                own(static_cast<za::SizeT>(got), static_cast<za::SizeT>(got) + 1, u);
            }
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
    u.kept = depth >= 0;
    if(frags.size() < shareMin || (depth <= 0 && (frags.size() < shareBig || depth <= shareDeepest)))
    {
        u.kind = 2;
        if(u.kept)
        {
            u.input = pack(frags);
        }
        u.pieces = frags.size();
        const auto t0 = za::Clock::nowNanoseconds();
        u.root = tb.build(frags);
        u.ms = za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0);
    }
    else if(const int contents = tb.leaf(frags))
    {
        u.kind = 0; // (asks for no plane: never done again)
        u.root = contents;
    }
    else
    {
        u.kind = 1;
        // A split's input kept only by the top and the top's last level (the units below fail up to those). The levels
        // between keep none: their own checks fail only if their split asked for a plane (a piece cut back to its
        // brush's bounds: none of vrstart's 2765 splits there did), and such a failure is redone from the top (the whole
        // tree on the merging thread, as the build on one thread). A copy each, freed after the split, was 5 GB of
        // transient copies for vrstart (and the heap, holding what a load frees, did not always use it again: 11 GB at
        // times).
        u.kept = u.kept && (!u.parent || depth == 0);
        if(u.kept)
        {
            u.input = pack(frags);
        }
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

// u's nodes into the tree (in the order the build on one thread makes them); its root. False: u's check (or one below
// it) failed and u kept no input to do it again from: everything it added is taken back out, for the unit above to redo.
bool emit(TreeBuilder& tb, Unit& u, Merge& m, int& root)
{
    const za::SizeT saved = tb.count();
    za::Vector<mclipnode_t>& nodes = tb.nodes();
    const za::SizeT savedNodes = nodes.size();
    int savedSolid = 0, savedEmpty = 0;
    tb.leafCounts(savedSolid, savedEmpty);
    const int savedRebounded = tb.rebounded;
    // u done again from its input (the build on one thread), or false if it has none
    const auto redo = [&]
    {
        m.undo(tb, saved);
        nodes.resize(savedNodes);
        tb.setLeafCounts(savedSolid, savedEmpty);
        tb.rebounded = savedRebounded;
        if(!u.kept)
        {
            return false;
        }
        ++m.redone;
        Frags input = unpack(u.input);
        root = tb.build(input);
        m.own(saved, tb.count(), u);
        return true;
    };
    if(!m.adopt(tb, u))
    {
        return redo();
    }
    tb.addLeaves(u.solid, u.empty);
    tb.rebounded += u.rebounded;
    if(u.kind == 0)
    {
        root = u.root;
        return true;
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
        root = u.root >= 0 ? u.root + offset : u.root;
        return true;
    }
    const int node = static_cast<int>(nodes.size());
    nodes.pushBack(mclipnode_t{m.real(u, u.split), {0, 0}});
    int front = 0, back = 0;
    if(!emit(tb, *u.kids[0], m, front) || !emit(tb, *u.kids[1], m, back))
    {
        return redo();
    }
    nodes[static_cast<za::SizeT>(node)].children[0] = front;
    nodes[static_cast<za::SizeT>(node)].children[1] = back;
    root = node;
    return true;
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
} // namespace
namespace detail
{
int buildTree(Tree& t, const Brushes& b, za::SizeT sub, const glm::dvec3* watch, bool report)
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
        const auto g0 = za::Clock::nowNanoseconds();
        growAllOnPool(tb, b, list, ext, frags);
        const auto g1 = za::Clock::nowNanoseconds();
        Unit top;
        speculate(top, tb, ZA_MOVE(frags), shareDepth);
        const auto g2 = za::Clock::nowNanoseconds();
        m.start = tb.count();
        m.owner.clear();
        (void)emit(tb, top, m, root); // (top keeps its input, or can't fail: never false)
        const auto g3 = za::Clock::nowNanoseconds();
        t.growMs += za::nanosecondsToMilliseconds(g1 - g0);
        t.unitsMs += za::nanosecondsToMilliseconds(g2 - g1);
        t.mergeMs += za::nanosecondsToMilliseconds(g3 - g2);
        za::Vector<const Unit*> stack{&top};
        while(!stack.empty())
        {
            const Unit* x = stack.back();
            stack.popBack();
            if(x->kind == 2)
            {
                ++t.units;
                if(x->ms > t.slowestMs)
                {
                    t.slowestMs = x->ms;
                    t.slowestPieces = x->pieces;
                }
            }
            for(const za::UniquePtr<Unit>& k : x->kids)
            {
                if(k)
                {
                    stack.pushBack(k.get());
                }
            }
        }
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
} // namespace detail
namespace
{

jobs::Site modelsSite{"hull models"}; // (buildModelsAtOnce's units and merges: vr_jobs_sites)

// The trees of these models (subs, in order) for each of these trees, as buildTree makes them one model after another,
// but all at once: each model of each tree built on the pool as a unit over its tree's table as it is now (only read
// meanwhile: a map's doors, lifts and walls are small, 940 builds of 1-2 ms on mge5m2, a tree's in a row 190 ms), then
// each tree's units merged in the models' order as the units of a tree's top are (Merge, emit: every plane asked of the
// tree's table again, the unit's taken only if each answer is the one it had). A model whose check fails (a plane
// nearly the same as one an earlier model added) is built again by buildTree there, as before. counts: the models built.
void buildModelsAtOnce(const za::Vector<Tree*>& sizes, const Brushes& b, const za::Vector<za::SizeT>& subs,
    za::Vector<int>& counts)
{
    struct Job
    {
        za::SizeT size = 0, sub = 0;
    };
    za::Vector<Job> work;
    // (each made by its tree's task below, which brings the tree's plane index up to date: a tree read from the disk
    // cache indexes all its planes, 300,000 on vrstart; done before the units read it)
    za::Vector<za::UniquePtr<TreeBuilder>> bases(sizes.size());
    for(za::SizeT i = 0; i < sizes.size(); ++i)
    {
        Tree& t = *sizes[i];
        if(t.heads.size() < b.subs.size())
        {
            t.heads.resize(b.subs.size(), -1);
        }
        for(const za::SizeT sub : subs)
        {
            if(t.heads[sub] < 0)
            {
                work.pushBack(Job{i, sub});
            }
        }
    }
    // The planes the table will give the models' brushes' own planes (most of their asks), asked of a builder over each
    // tree's table in the models' order, as one thread would (growAllOnPool's `given`): a unit cuts with those, so an
    // earlier model's nearly equal plane is the one it uses too.
    za::Vector<za::Vector<mplane_t>> given(sizes.size());
    za::Vector<za::SizeT> givenAt(work.size()); // [unit]: its first brush's planes in given[its tree]
    jobs::parallelFor(modelsSite, sizes.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                bases[i] = za::makeUnique<TreeBuilder>(*sizes[i]);
                za::Vector<mplane_t> planes;
                za::Vector<mclipnode_t> nodes;
                int solid = 0, empty = 0;
                za::Vector<PlaneAsk> log;
                TreeBuilder pb{*bases[i], planes, nodes, solid, empty, log};
                const glm::dvec3 ext{sizes[i]->ext};
                for(za::SizeT k = 0; k < work.size(); ++k)
                {
                    if(work[k].size != i)
                    {
                        continue;
                    }
                    givenAt[k] = given[i].size();
                    const SubModel& sm = b.subs[work[k].sub];
                    for(za::U32 n = 0; n < sm.numBrushes; ++n)
                    {
                        const Brush& br = b.brushes[sm.firstBrush + n];
                        for(za::U32 q = 0; q < br.count; ++q)
                        {
                            const Plane& pl = b.planes[br.first + q];
                            given[i].pushBack(pb.planeAt(pb.plane(glm::dvec3{pl.normal}, pl.dist + support(pl, ext))));
                        }
                    }
                }
            }
        });
    za::Vector<Unit> units(work.size());
    jobs::parallelFor(modelsSite, work.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT k = begin; k < end; ++k)
            {
                const Job& j = work[k];
                const Tree& t = *sizes[j.size];
                Unit& u = units[k];
                const auto t0 = za::Clock::nowNanoseconds();
                u.baseCount = bases[j.size]->count();
                TreeBuilder ub{*bases[j.size], u.planes, u.nodes, u.solid, u.empty, u.log};
                const glm::dvec3 ext{t.ext};
                const SubModel& sm = b.subs[j.sub];
                Frags frags;
                const mplane_t* g = given[j.size].data() + givenAt[k];
                for(za::U32 i = 0; i < sm.numBrushes; ++i)
                {
                    const Brush& br = b.brushes[sm.firstBrush + i];
                    Frag f;
                    if(ub.grow(b, br, ext, f, g))
                    {
                        frags.pushBack(ZA_MOVE(f));
                    }
                    g += br.count;
                }
                u.kind = 2;
                u.root = ub.build(frags);
                u.rebounded = ub.rebounded;
                u.ms = za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0);
            }
        });
    bases.clear();
    jobs::parallelFor(modelsSite, sizes.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                Tree& t = *sizes[i];
                TreeBuilder tb{t};
                for(za::SizeT k = 0; k < work.size(); ++k)
                {
                    if(work[k].size != i)
                    {
                        continue;
                    }
                    const za::SizeT sub = work[k].sub;
                    Merge m;
                    m.start = tb.count();
                    m.reuse = true;
                    int root = 0;
                    if(emit(tb, units[k], m, root))
                    {
                        if(root < 0) // (a lone leaf: a node of its own, as buildTree gives it)
                        {
                            const int node = static_cast<int>(t.nodes.size());
                            t.nodes.pushBack(mclipnode_t{tb.plane(glm::dvec3{0.0, 0.0, 1.0}, 0.0), {root, root}});
                            root = node;
                        }
                        t.heads[sub] = root;
                        t.ms += units[k].ms;
                    }
                    else
                    {
                        ++t.redone;
                        (void)buildTree(t, b, sub, nullptr, false);
                    }
                    ++counts[i];
                }
            }
        });
}

// Quake's trace of a point (the box's centre) through a model's tree.
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

} // namespace
namespace detail
{
int clientNum(const edict_t* ent)
{
    if(!ent || qcvm != &sv.qcvm || !qcvm->edicts || qcvm->edict_size <= 0)
    {
        return -1;
    }
    const za::PtrDiffT bytes = reinterpret_cast<const byte*>(ent) - reinterpret_cast<const byte*>(qcvm->edicts);
    return static_cast<int>(bytes / qcvm->edict_size);
}
} // namespace detail
namespace
{

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

// Crouched, slower (vr_crouch_speed; VR_CrouchSpeedMax): per client, his eyes' height over his feet at his last move
// (0: unknown) and the slowing's share now (0 none .. 1 fully crouched), eased towards his eyes' over
// crouchSpeedEaseTime; cleared at each map's load. Fully slowed crouchSpeedBand units under the tallest crouched box's
// top; the crouched most speed never over crouchSpeedQuiet of the stealth AI's footsteps' speed (vr_stealth_run_speed).
constexpr float crouchSpeedBand = 8.f;
constexpr float crouchSpeedEaseTime = 0.25f;
constexpr float crouchSpeedQuiet = 0.9f;
float crouchEye[crouchSlots]{};
float crouchSlowShare[crouchSlots]{};

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
} // namespace
namespace detail
{
float entWidthSetting()
{
    const float v = vr_hull_ent_width.value;
    if(v < 0.f)
    {
        return vr_hull_width.value > 0.f ? widthSetting() : 0.f;
    }
    return v > 0.f ? za::clamp(v, minWidth, maxWidth) : 0.f;
}
} // namespace detail
namespace
{

// The player's width that shots, missiles and splash traces hit (vr_hull_hit_width): 0 Quake's (off).
} // namespace
namespace detail
{
float hitWidthSetting()
{
    const float v = vr_hull_hit_width.value;
    return v > 0.f && v < maxWidth ? za::clamp(v, minWidth, maxWidth) : 0.f;
}
} // namespace detail
namespace
{

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

constexpr float minMonsterWidth = 16.f;

// Each edict's class (an index in monsterClasses, -1 none), for the classname it had when looked up.
struct ClassCache
{
    za::Vector<int> names;
    za::Vector<signed char> classes;
    auto members() { return qvr::mem::list(names, classes); }
};
mem::Cache<ClassCache> classCache{"hull monster classes", mem::MapChange};

} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

float hull2Height(const qmodel_t* world)
{
    const float h = world ? world->hulls[2].clip_maxs[2] - world->hulls[2].clip_mins[2] : 0.f;
    return h > 0.f ? h : 88.f;
}

// Quake's hull for a box of this width (SV_HullForEntity: hull 1 up to 32, else hull 2): its width and height.
} // namespace
namespace detail
{
void quakeHull(float size, float& width, float& height)
{
    const bool one = size <= 32.f;
    const hull_t& h = sv.worldmodel->hulls[one ? 1 : 2];
    width = h.clip_maxs[0] - h.clip_mins[0];
    width = width > 0.f ? width : (one ? 32.f : 64.f);
    height = one ? hull1Height(sv.worldmodel) : hull2Height(sv.worldmodel);
}
} // namespace detail
namespace
{

// A class's width for its monsters' box (size) and Quake's hull for it: 0 when they keep Quake's hull.
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

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
    if(!m || m->type != mod_brush || !m->hulls[0].clipnodes || m->numnodes <= 0 || brushes::isRuntime(m))
    {
        return known; // (a runtime brush's: clipBSP meets its planes directly)
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
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

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
    Con_DPrintf("hull:   brushes grown %.0f ms, units built %.0f ms, merged %.0f ms; %d units, the slowest %.0f ms (%d pieces)\n",
        t.growMs, t.unitsMs, t.mergeMs, static_cast<int>(t.units), t.slowestMs, static_cast<int>(t.slowestPieces));
}

// ---------------------------------------------------------------------------------------------------------------
// The world's compiled trees kept on disk (vr_hull_cache): `<gamedir>/cache/hulls/<build>/<world>_<box>.hul`, <world>
// the world's content (keyOf: hull 0's and hull 1's nodes, the planes, the models' heads), <box> the tree's box, <build>
// the compile's version (treeVersion: bumped when the compile changes). A tree is the same bytes whenever the same brushes are
// compiled for the same box (the build on the pool is the build on one thread's, node for node), so a file is that
// build's result: a cold start of a big map reads it instead of compiling it (vrstart: four trees of 1.2-1.4 million
// nodes, 9 s of 32 threads). Only trees that took diskMinMs or more are written (a small map's build is about as quick
// as its file). A file is read and checked (magic, version, sizes, the world and the box, every node's numbers, a sum of
// its bytes) on the tree's own job; anything amiss is compiled again and written over. Written beside its place and
// renamed (another copy of the game never reads half a file). The first use in a session under a game directory
// removes the other builds' folders and the oldest files past its budget (vr_diskcache.cpp: 1 GB). vr_hull_cache 2: read, then compiled anyway
// and compared (vr_hull_stats counts them).

constexpr char diskMagic[4] = {'Q', 'V', 'R', 'H'};
constexpr za::U32 diskVersion = 1;
constexpr double diskMinMs = 250.0;

// The trees' compile version: BUMP IT whenever a change to the compile (growing, the units, speculate, merging, the
// rebounding, their constants) could change the tree compiled for the same brushes and box. The disk folder is this,
// the file layout's version and the brushes' buildVersion (also in keyOf), no longer the build's compile time
// (2026-10-16, VRTRACE_20261016 D): a new build reads the trees the last one wrote unless the compile changed.
// vr_hull_cache 2 compiles anyway and compares (a forgotten bump shows as "differed" in vr_hull_stats).
constexpr za::U32 treeVersion = 1;

za::String makeDiskBuild()
{
    const za::U32 parts[] = {treeVersion, diskVersion, static_cast<za::U32>(buildVersion),
        static_cast<za::U32>(sizeof(mclipnode_t)), static_cast<za::U32>(sizeof(mplane_t))};
    static_assert(sizeof(za::U32) == sizeof(unsigned));
    return diskcache::buildOf(parts, sizeof(parts) / sizeof(parts[0]));
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
    d.world = keyOf(world);
    char name[96];
    snprintf(name, sizeof(name), "/%016llx_%g_%g_%g.hul", static_cast<unsigned long long>(d.world),
        static_cast<double>(t.ext.x), static_cast<double>(t.ext.y), static_cast<double>(t.ext.z));
    // (the folder made, the other builds' and the oldest files past the budget removed: once a session, vr_diskcache.cpp)
    d.path = diskcache::prepare(diskcache::Product::Hulls, diskBuild.cStr()) + name;
    return d;
}

// The world's tree read into t (a fresh tree, its heads sized) and checked; false: no file, or one amiss (t untouched).
bool diskRead(Tree& t, const DiskJob& d, int& rebounded)
{
    const double t0 = Sys_DoubleTime();
    FILE* in = Sys_fopen(d.path.cStr(), "rb");
    if(!in)
    {
        diskcache::noteMiss(diskcache::Product::Hulls, "(the world's tree)");
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
        diskcache::noteRejected(diskcache::Product::Hulls);
        diskcache::noteMiss(diskcache::Product::Hulls, "(the world's tree, rejected)");
        return false;
    }
    diskcache::noteHit(diskcache::Product::Hulls, static_cast<size_t>(size), Sys_DoubleTime() - t0);
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
    const diskcache::Part parts[] = {{&h, sizeof(h)}, {t.nodes.data(), t.nodes.size() * sizeof(mclipnode_t)},
        {t.planes.data(), t.planes.size() * sizeof(mplane_t)}};
    if(diskcache::writeAtomic(d.path, parts, 3))
    {
        ++diskCounts.written;
        diskcache::noteWrite(diskcache::Product::Hulls, parts[0].size + parts[1].size + parts[2].size);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The world's trees compiled at once within a memory budget (vr_hull_build_mb). A compile holds its pieces while it
// runs, about buildBytesPerBrush a brush of the world's at its peak (vrstart: 33k a tree, 300-400 MB; its seven trees
// at once took 3.7 GB of working set, three at a time 2.9: ROUND21.md, "The hull build's memory"). A compile that would
// pass the budget while others run is queued, not waited for on a thread: the compile that ends next runs it on its own
// thread (its job, or its parallelFor chunk, ends after it), so the pool's workers keep working meanwhile. One always
// runs, whatever its size. A tree is the same bytes whichever thread compiles it and when.

constexpr za::U64 buildBytesPerBrush = 16u << 10;

// A tree to compile (and write to the disk, when it took long enough: disk.mode not 0).
struct GateWork
{
    Tree* tree = nullptr;
    const Brushes* brushes = nullptr;
    DiskJob disk;
    za::U64 cost = 0;   // its estimate (bytes)
    za::U64 budget = 0; // vr_hull_build_mb when it was handed out (bytes; the main thread's read)
};

struct BuildGate
{
    za::AtomicMutex lock;
    za::U64 inUse = 0;          // the compiles under way's estimates
    za::Vector<GateWork> queue; // waiting, in the order they came
};
BuildGate buildGate;

// vr_hull_build_mb in bytes (the main thread).
za::U64 buildBudget()
{
    return vr_hull_build_mb.value > 0.f ? static_cast<za::U64>(vr_hull_build_mb.value) << 20 : ~za::U64{0};
}

// t's compile from b (built: its world's brushes counted).
GateWork gateWork(Tree& t, const Brushes& b, DiskJob d, za::U64 budget)
{
    GateWork w;
    w.tree = &t;
    w.brushes = &b;
    w.disk = ZA_MOVE(d);
    const za::U64 brushes = b.subs.empty() ? 0 : b.subs[0].numBrushes + b.clips.size();
    w.cost = brushes * buildBytesPerBrush;
    w.budget = budget;
    return w;
}

// w's tree compiled (its pieces cut back in cutBack), then written to the disk if it took long enough.
void compileWork(const GateWork& w)
{
    Tree& t = *w.tree;
    t.cutBack = compileWorldTree(t, *w.brushes);
    if(w.disk.mode != 0 && t.ms >= diskMinMs && t.heads[0] >= 0)
    {
        diskWrite(t, w.disk, t.cutBack);
    }
}

// w compiled on this thread now if the budget allows, else queued; then, as each compile here ends, the first queued
// one that fits (any, once none is under way).
void gatedCompile(GateWork w)
{
    {
        za::LockGuard guard{buildGate.lock};
        if(buildGate.inUse != 0 && buildGate.inUse + w.cost > w.budget)
        {
            buildGate.queue.pushBack(ZA_MOVE(w));
            return;
        }
        buildGate.inUse += w.cost;
    }
    while(true)
    {
        compileWork(w);
        za::LockGuard guard{buildGate.lock};
        buildGate.inUse -= w.cost;
        za::Vector<GateWork>& queue = buildGate.queue;
        za::SizeT next = 0;
        while(next < queue.size() && buildGate.inUse != 0 && buildGate.inUse + queue[next].cost > queue[next].budget)
        {
            ++next;
        }
        if(next == queue.size())
        {
            return;
        }
        w = ZA_MOVE(queue[next]);
        queue.erase(queue.begin() + static_cast<za::PtrDiffT>(next));
        buildGate.inUse += w.cost;
    }
}

jobs::Site treesSite{"hull trees"}; // (its parallelFor: vr_jobs_sites)

// These trees compiled at once on the pool within the budget (the main thread one of them), reported in their order.
void compileTrees(const za::Vector<Tree*>& todo, const Brushes& b)
{
    const za::U64 budget = buildBudget();
    jobs::parallelFor(treesSite, todo.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT i = begin; i < end; ++i)
            {
                gatedCompile(gateWork(*todo[i], b, DiskJob{}, budget));
            }
        });
    for(Tree* t : todo)
    {
        reportTree(*t, t->cutBack);
    }
}

// The world's tree in t from the disk (its pieces cut back in cutBack), else compiled within the budget (and written
// when it took long enough: gatedCompile).
void diskOrCompile(Tree& t, const Brushes& b, GateWork w)
{
    const DiskJob& d = w.disk;
    if(d.mode == 0 || (!t.heads.empty() && t.heads[0] >= 0) || !t.planes.empty() || !t.nodes.empty())
    {
        w.disk.mode = 0; // (not a fresh tree: a file holds a fresh tree's build)
        gatedCompile(ZA_MOVE(w));
        return;
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
            diskcache::noteChecked(diskcache::Product::Hulls, same, "(the world's tree)");
        }
        t.cutBack = rebounded;
        return;
    }
    ++diskCounts.missed;
    gatedCompile(ZA_MOVE(w));
}

// A tree compiled on the pool once the brushes are built (or read from the disk: vr_hull_cache).
void postTree(Tree* t)
{
    pending.trees.pushBack(t);
    t->cutBack = 0;
    pending.run.pushBack(jobs::async(
        [t, d = diskJob(*t, sv.worldmodel), budget = buildBudget()]() mutable
        {
            pending.brushes.wait(); // (only waited on until settle: nothing else touches it meanwhile)
            diskOrCompile(*t, built, gateWork(*t, built, ZA_MOVE(d), budget));
        }));
}

} // namespace
namespace detail
{
void settle()
{
    if(!pending.brushes.valid())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    za::Vector<jobs::Future<void>> run = ZA_MOVE(pending.run);
    za::Vector<Tree*> trees = ZA_MOVE(pending.trees);
    pending.run.clear();
    pending.trees.clear();
    for(jobs::Future<void>& f : run)
    {
        f.get(); // (a compile queued by one ends with the job of the compile that ran it: all are done after these)
    }
    pending.brushes.get();
    const double t1 = Sys_DoubleTime();
    VR_TimeAdd("hull: the map's brushes and compiled hulls, waited for (built on the pool)", t1 - t0);
    Con_DPrintf("hull: %s %s as %d brushes in %.1f ms (on the pool; waited %.1f ms, %.1f ms after they began)\n",
        sv.worldmodel ? sv.worldmodel->name : "?", pending.kept ? "kept from its last load" : "rebuilt",
        static_cast<int>(built.brushes.size()), built.ms, (t1 - t0) * 1000.0, (t1 - pending.posted) * 1000.0);
    if(!trees.empty() && !built.subs.empty())
    {
        const za::U64 cost = (built.subs[0].numBrushes + built.clips.size()) * buildBytesPerBrush;
        Con_DPrintf("hull: %d trees, about %.0f MB each while compiled (vr_hull_build_mb %g)\n", static_cast<int>(trees.size()),
            static_cast<double>(cost) / 1048576.0, static_cast<double>(vr_hull_build_mb.value));
    }
    for(const Tree* t : trees)
    {
        reportTree(*t, t->cutBack);
    }
}
} // namespace detail
namespace
{

} // namespace
namespace detail
{
const Tree& treeFor(const Brushes& b, za::SizeT sub, const glm::vec3& ext, const char* target)
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
} // namespace detail
namespace
{

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

// vr_mhull_reset: every class's width back to its own box's (the settings' defaults).
void reset_f()
{
    for(const MonsterClass& c : monsterClasses)
    {
        Cvar_SetQuick(c.width, c.width->default_string);
    }
}

// The world's brushes and its hull for the player's box: built with the map or the setting (not at the first move: a
// hitch in play).
} // namespace
namespace detail
{
bool wanted()
{
    return sv.worldmodel && (vr_hull_width.value > 0.f || vr_mhull.value != 0.f || crouchOn());
}
} // namespace detail
namespace
{

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
} // namespace
namespace detail
{
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
} // namespace detail
namespace
{

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
    buildModelsAtOnce(sizes, built, subs, counts);
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
    registerDiagCommands(); // vr_hull_cachetest, _preloadtest, _stats, _keeptest, _bench, _walktest, _probe, _leafdebug, _approach, _hittest, vr_mhull_walktest (vr_hull_diag.cpp)
    Cmd_AddCommand("vr_mhull_reset", reset_f);
    Cvar_SetCallback(&vr_hull_width, onWidthChanged);
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
    for(int i = 0; i < crouchSlots; ++i)
    {
        crouchBox[i] = 0.f; // (each map starts standing)
        crouchEye[i] = 0.f;
        crouchSlowShare[i] = 0.f;
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
    if(model && brushes::isRuntime(model))
    {
        // A box made in the game (vr_brushes.cpp): its convex planes met directly (its model is made again in place
        // when it changes: never in the brushes kept here).
        const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
        if(!brushes::clipBox(model, glm::vec3{start[0], start[1], start[2]} - origin, glm::vec3{boxMins[0], boxMins[1], boxMins[2]},
               glm::vec3{boxMaxs[0], boxMaxs[1], boxMaxs[2]}, glm::vec3{end[0], end[1], end[2]} - origin, trace))
        {
            return false;
        }
        ++vr_profcounts.hullchecks;
        for(int i = 0; i < 3; ++i)
        {
            trace.endpos[i] += origin[i];
        }
        return true;
    }
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
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const Tree& t = treeFor(built, static_cast<za::SizeT>(sub), (hi - lo) * 0.5f, model->name);
    trace = treeTrace(t, t.heads[static_cast<za::SizeT>(sub)], s + centre, e + centre);
    for(int i = 0; i < 3; ++i)
    {
        trace.endpos[i] += origin[i] - centre[i];
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
    if(!worldBrushes(world))
    {
        return -1;
    }
    const float half = (vr_hull_width.value > 0.f ? widthSetting() : 32.f) * 0.5f;
    const glm::vec3 lo{-half, -half, -24.f}, hi{half, half, -24.f + (crouch > 0.f ? crouch : hull1Height(world))};
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const Tree& t = treeFor(built, 0, (hi - lo) * 0.5f);
    const trace_t tr = treeTrace(t, t.heads[0], start + centre, end + centre);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.f;
}

void updateCrouch(edict_t* ent, const glm::vec3& head)
{
    const int num = clientNum(ent);
    if(num < 1 || num >= crouchSlots)
    {
        return;
    }
    crouchEye[num] = (ent->free || head == glm::vec3{0.f}) ? 0.f : head.z - (ent->v.origin[2] + ent->v.mins[2]);
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

float crouchSpeedMax(edict_t* ent, float maxspeed)
{
    const int num = clientNum(ent);
    if(num < 1 || num >= crouchSlots || !sv.worldmodel)
    {
        return maxspeed;
    }
    // The share his eyes ask for: none over the tallest crouched box's top, all crouchSpeedBand units under it.
    float want = 0.f;
    if(vr_crouch_speed.value != 0.f && crouchEye[num] > 0.f)
    {
        float levels[maxCrouchLevels];
        const float top = levels[crouchLevels(sv.worldmodel, levels) - 1];
        want = za::clamp((top - crouchEye[num]) / crouchSpeedBand, 0.f, 1.f);
    }
    float& share = crouchSlowShare[num];
    const float step = static_cast<float>(host_frametime) / crouchSpeedEaseTime;
    share = want > share ? za::min(want, share + step) : za::max(want, share - step);
    if(share <= 0.f)
    {
        return maxspeed;
    }
    const float scaled = maxspeed * za::clamp(vr_crouch_speed_scale.value, 0.2f, 1.f);
    const float quiet = za::max(0.f, vr_stealth_run_speed.value * crouchSpeedQuiet);
    const float crouched = za::min(maxspeed, za::min(scaled, quiet));
    return maxspeed + (crouched - maxspeed) * share;
}

} // namespace qvr::hull

// SV_AirMove (walking, in the air): crouched, the stick's most speed (vr_crouch_speed; qvr::hull::crouchSpeedMax).
extern "C" float VR_CrouchSpeedMax(edict_t* ent, float maxspeed)
{
    return qvr::hull::crouchSpeedMax(ent, maxspeed);
}

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
