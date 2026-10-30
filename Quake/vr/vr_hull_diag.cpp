// vr_hull_diag.cpp -- the hull tests, benches and probes: vr_hull_cachetest, _preloadtest, _stats, _keeptest, _bench, _walktest,
// _probe, _leafdebug, _approach, _hittest and vr_mhull_walktest.
// Split from vr_hull.cpp (a pure move); what they share is in vr_hull_internal.hpp.

#include "vr_random.hpp"
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

// Exercise more than the old 12-size limit and revisit every compiled world tree.
za::U32 hashOf(const Tree& t);

void cacheTest_f()
{
    if(!sv.active || !sv.worldmodel || !worldBrushes(sv.worldmodel)) return;
    settle();
    joinBackground();
    const int room =static_cast<int>(maxMonsterTrees - monsterTrees.trees.size());
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
        h.add(q.growsZ);
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
    Con_Printf("hull: compiled in play on the pool (vr_hull_async %d, this session): %d started, %d running (waited for "
               "now)\n", static_cast<int>(vr_hull_async.value), backgroundStarted(), backgroundRunning());
    joinBackground();
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
    Con_Printf("hull: %d brush models (%d external .bsp); compiled hull: %s%d nodes, %d planes, %d solid "
               "and %d empty leaves, %.0f KB, %s in %.1f ms\n",
        static_cast<int>(b->subs.size()), external,
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
    joinBackground();
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
    joinBackground(); // (the trees compiled in play read the brushes built again here)
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

// Somewhere random the player's box stands (on a floor, in the open, not in liquid).
bool hop(edict_t* ent)
{
    qmodel_t* world = sv.worldmodel;
    glm::vec3 wmins{world->mins[0], world->mins[1], world->mins[2]};
    glm::vec3 wmaxs{world->maxs[0], world->maxs[1], world->maxs[2]};
    WalkTest& w = walkTest;
    // Every other hop near a door, lift, train or button (a pusher), if the map has any.
    if(vrand::xorshift01(w.seed) < 0.5f)
    {
        int count = 0;
        edict_t* chosen = nullptr;
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
        {
            edict_t* e = EDICT_NUM(i);
            if(!e->free && static_cast<int>(e->v.solid) == SOLID_BSP && static_cast<int>(e->v.movetype) == MOVETYPE_PUSH &&
                vrand::xorshift01(w.seed) * static_cast<float>(++count) < 1.f)
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
        glm::vec3 p = wmins + (wmaxs - wmins) * glm::vec3{vrand::xorshift01(w.seed), vrand::xorshift01(w.seed), vrand::xorshift01(w.seed)};
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
        const float yaw = vrand::xorshift01(w.seed) * 6.2831853f, d = 96.f + 288.f * vrand::xorshift01(w.seed);
        p = o + glm::vec3{za::cos(yaw) * d, za::sin(yaw) * d, flies ? (vrand::xorshift01(w.seed) - 0.5f) * 96.f : 24.f};
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
    qvr::srvrandom::seedNow(w.seed); // movetogoal's turns (the server's stream)
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
            const float cr = (vrand::xorshift01(seed) + vrand::xorshift01(seed)) - 1.f, cu = (vrand::xorshift01(seed) + vrand::xorshift01(seed)) - 1.f;
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

// Rebuild nodes independently against the same shared plane table. An isolated model's fresh plane table can
// choose slightly different near-equal planes from the world's table (the builder's existing plane deduplication).
// Do not fill missing cached heads: the test must catch incomplete preparation.
void preloadTest_f()
{
    if(!sv.active || !wanted() || vr_hull_brushmodels.value == 0.f || !worldBrushes(sv.worldmodel)) return;
    settle();
    joinBackground();
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

} // namespace

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
            vr_hull_width.value <= 0.f ? "Quake's hull 1" : "compiled hull", entWidthSetting() > 0.f ? entWidthSetting() : 32.f,
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
        w.nextHop = sv.qcvm.time + 3.0 + 4.0 * vrand::xorshift01(w.seed);
    }
    if(sv.qcvm.time >= w.nextTurn)
    {
        w.yaw = vrand::xorshift01(w.seed) * 6.2831853f;
        w.nextTurn = sv.qcvm.time + 0.3 + 1.2 * vrand::xorshift01(w.seed);
    }
    const float speed = 320.f;
    ent->v.velocity[0] = za::cos(w.yaw) * speed;
    ent->v.velocity[1] = za::sin(w.yaw) * speed;
    if(sv.qcvm.time >= w.nextJump && ((int)ent->v.flags & FL_ONGROUND))
    {
        ent->v.velocity[2] = 270.f;
        ent->v.flags = static_cast<float>((int)ent->v.flags & ~FL_ONGROUND);
        w.nextJump = sv.qcvm.time + 0.5 + 3.0 * vrand::xorshift01(w.seed);
    }
}

void registerDiagCommands()
{
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
}

} // namespace qvr::hull
