// vr_nav.cpp -- navmesh pathfinding for walking monsters (vr_nav.hpp; docs/vr-port/NAVMESH.md).

#include "vr_nav.hpp"

#include "vr_brushes.hpp"
#include "vr_cvars.hpp"
#include "vr_diskcache.hpp"
#include "vr_geom.hpp"
#include "vr_hull.hpp"
#include "vr_jobs.hpp"
#include "vr_lines.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_tunables.hpp"

#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "Recast.h"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

namespace qvr::nav
{
namespace
{

// Bumped whenever the build could make another mesh from the same input (the steps, their constants, the areas).
// 2: drop and lift links.
constexpr unsigned navVersion = 2;
constexpr char navMagic[5] = "QVNM";

// Detour's areas and flags.
constexpr unsigned char areaGround = 1, areaWater = 2, areaSlime = 3, areaLava = 4, areaDrop = 5, areaLift = 6;
constexpr unsigned short flagWalk = 1, flagBlocked = 2, flagDrop = 4, flagLift = 8;

constexpr int maxPath = 256;    // polygons in a monster's path
constexpr int maxCorners = 6;   // a path's straight corners looked at a step
constexpr int queryNodes = 4096; // Detour's search nodes per query
constexpr za::SizeT maxClasses = 32; // size classes a map can have

// Recast is y-up: Quake's (x, y, z) is Recast's (x, z, -y) (a turn, so the windings keep their sense).
glm::vec3 toRc(const glm::vec3& q)
{
    return {q.x, q.z, -q.y};
}

glm::vec3 fromRc(const float* r)
{
    return {r[0], -r[2], r[1]};
}

double msSince(za::I64 t0)
{
    return static_cast<double>(za::Clock::nowNanoseconds() - t0) / 1e6;
}

// A lift's (func_plat's) or train's ride between two of its stops: the mover's origin at each, its bounds (its own
// space); `both`: either way (a lift), else a to b only (a train's leg).
struct Ride
{
    int mover;
    glm::vec3 a, b, mins, maxs;
    bool both;
};

// The map's walls and floors as triangles (Recast's space), the walkable ones (Quake's floor rule) marked.
struct Geometry
{
    za::Vector<float> verts;
    za::Vector<int> tris;
    za::Vector<unsigned char> walkable; // RC_WALKABLE_AREA or RC_NULL_AREA
    glm::vec3 bmin{0.f}, bmax{0.f};    // Recast's space
    // The brushes themselves (Quake's space), each filled as solid in the heightfield (a face's triangles alone leave
    // the inside open: a floor under a func_wall or inside a clip brush would stay walkable), with their flat bounds
    // (x, y lo; x, y hi).
    za::Vector<glm::vec4> planes;
    za::Vector<hull::NavBrush> solids;
    za::Vector<glm::vec4> boxes;
    za::Vector<Ride> rides; // the lifts' and trains' rides (gatherRides)
    za::U64 hash = 0;
    int brushes = 0, clipBrushes = 0, entityBrushes = 0;
    double ms = 0.0;
};

// A tile's navmesh data (dtAlloc'ed: the navmesh frees it).
struct Tile
{
    int x, y;
    unsigned char* data;
    int size;
};

// A tile's polygons as Recast left them (the build's, until its data is made with its links).
struct TileSrc
{
    int x, y;
    rcPolyMesh* pmesh;
    rcPolyMeshDetail* dmesh;
};

// An off-mesh link (Recast's space): a drop off a ledge (areaDrop) or a ride on a lift or train (areaLift, its ride's
// index plus one as the id).
struct Link
{
    float pos[6];
    float rad;
    unsigned char dir, area;
    unsigned short flags;
    unsigned id;
};

// One size class: its monsters' radius (half width) and height, its navmesh and query.
struct NavClass
{
    float radius = 0.f, height = 0.f;
    dtNavMesh* mesh = nullptr;
    dtNavMeshQuery* query = nullptr;
    jobs::Future<void> job;   // its build on the pool (pending)
    bool pending = false;
    za::Vector<Tile> built;   // the job's tiles, added on the main thread at its join
    dtNavMeshParams params{};
    za::U64 key = 0;
    double ms = 0.0;          // the build's (or the file's read)
    double joinMs = 0.0;      // its join on the main thread (the mesh made, the file written): a hitch if long
    bool fromDisk = false, failed = false;
    int polys = 0, tiles = 0, drops = 0, lifts = 0;
    za::SizeT bytes = 0;
    za::Vector<dtPolyRef> blocked; // polygons a door or lift stands on (flagBlocked)
    // The job's results, taken at its join: the mesh and its query, the cache file's data (its tiles' count, checksum).
    dtNavMesh* jobMesh = nullptr;
    dtNavMeshQuery* jobQuery = nullptr;
    za::Vector<char> blob;
    unsigned blobCount = 0, blobSum = 0;
};

// A monster's path (by edict number).
struct Agent
{
    int cls = -1;
    int goal = 0;
    double repathAt = 0.0;
    glm::vec3 goalAt{0.f};    // the goal's feet when the path was found
    float endRc[3]{};         // the path's end (Recast's space)
    za::Vector<dtPolyRef> path;
    float corners[maxCorners * 3]{};
    int cornerCount = 0, next = 0;
    double drawUntil = 0.0;   // vr_nav_debug 2
    // A link under way (followLink): its polygon, kind (areaDrop, areaLift), ends (Quake's space), when it began, the
    // ride's stops (board, leave) and phase (0 boarding, 1 riding, 2 stepping off), whether it left the ground.
    dtPolyRef link = 0;
    unsigned char linkArea = 0;
    int ride = -1, phase = 0;
    glm::vec3 linkFrom{0.f}, linkTo{0.f}, board{0.f}, leave{0.f};
    double linkSince = 0.0, phaseSince = 0.0;
    bool airborne = false;
    dtPolyRef waitLink = 0;   // the lift link it waits at, since when
    double waitSince = 0.0;
    double noLinksUntil = 0.0; // a link given up: plans without links till then
    int action = 0;           // navlink: 0 walk, 1 walk off the ledge (a drop), 2 stand (waiting, riding, falling)
    int slot = -1, slotGoal = 0; // its approach slot round that goal (approach), claimed when
    double slotAt = 0.0;
};

// A walker as the crowd's separation sees it (crowdNow: once a frame).
struct Crowd
{
    glm::vec3 pos;
    float radius;
    int num;
};

struct State
{
    Geometry geo;
    za::Vector<NavClass> classes;
    za::Vector<int> movers;   // brush entities left out of the mesh: doors, lifts, trains (edict numbers)
    za::Vector<Agent> agents;
    double moversAt = 0.0;
    bool ready = false;       // a map's geometry gathered
    int steers = 0, paths = 0, fallbacks = 0;
    int drops = 0, rides = 0, linkFails = 0; // links taken to their end, given up
    za::Vector<Crowd> crowd;  // the live walkers, as of crowdAt
    double crowdAt = -1.0;
};

State state;
dtQueryFilter filter;
dtQueryFilter filterNoLinks; // (a monster that gave a link up, for a while)
jobs::Site brushSite{"nav brushes"};
jobs::Site tileSite{"nav tiles"};

// ---------------------------------------------------------------------------------------------------------------------
// The geometry.

bool isMover(const char* cls)
{
    static constexpr const char* prefixes[] = {"func_door", "func_plat", "func_train", "func_button", "func_rotat",
        "func_bob", "func_elvtr", "func_movewall", "func_new_plat", "rotate_object", "door", "plat", "train"};
    for(const char* p : prefixes)
    {
        if(!strncmp(cls, p, strlen(p)))
        {
            return true;
        }
    }
    return false;
}

struct BrushSet
{
    za::Vector<glm::vec4> planes;
    za::Vector<hull::NavBrush> brushes;
};

// The planes of every solid the mesh is made from: the world's brushes and clip brushes, and the brush entities that
// stand still (func_wall, breakables...) where they stand. Movers are listed apart.
void gatherSolids(BrushSet& set, za::Vector<int>& movers, Geometry& g)
{
    (void)hull::navSolids(0, true, set.planes, set.brushes);
    for(const hull::NavBrush& b : set.brushes)
    {
        g.clipBrushes += b.clip ? 1 : 0;
    }
    g.brushes = static_cast<int>(set.brushes.size());
    progs::forEachEdict(1, [&](edict_t* e) {
        if(static_cast<int>(e->v.solid) != SOLID_BSP || !e->v.model)
        {
            return;
        }
        const char* model = PR_GetString(e->v.model);
        if(model[0] != '*')
        {
            return;
        }
        const char* cls = e->v.classname ? PR_GetString(e->v.classname) : "";
        if(isMover(cls))
        {
            movers.pushBack(NUM_FOR_EDICT(e));
            return;
        }
        if(e->v.angles[0] != 0.f || e->v.angles[1] != 0.f || e->v.angles[2] != 0.f)
        {
            return; // (a turned one: left out)
        }
        const za::SizeT first = set.brushes.size(), firstPlane = set.planes.size();
        if(!hull::navSolids(atoi(model + 1), false, set.planes, set.brushes))
        {
            return;
        }
        const glm::vec3 o = geom::vec(e->v.origin);
        for(za::SizeT i = firstPlane; i < set.planes.size(); ++i)
        {
            set.planes[i].w += glm::dot(glm::vec3{set.planes[i]}, o);
        }
        g.entityBrushes += static_cast<int>(set.brushes.size() - first);
    });
}

edict_t* findByTargetname(const char* name)
{
    edict_t* found = nullptr;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(!found && e->v.targetname && !strcmp(PR_GetString(e->v.targetname), name))
        {
            found = e;
        }
    });
    return found;
}

// The lifts' and trains' rides (main thread, at the load), from the QC's fields: a func_plat between its bottom (pos2)
// and its top (pos1); a func_train from each stop on its path (a corner it waits at) to the next.
void gatherRides(const za::Vector<int>& movers, Geometry& g)
{
    g.rides.clear();
    const int pos1 = progs::fields().pos1, pos2 = progs::fields().pos2, wait = progs::fields().wait;
    for(const int num : movers)
    {
        edict_t* e = progs::edictAt(num);
        if(!e || e->free)
        {
            continue;
        }
        const char* cls = e->v.classname ? PR_GetString(e->v.classname) : "";
        const glm::vec3 mins = geom::vec(e->v.mins), maxs = geom::vec(e->v.maxs);
        // (Quake's spawn functions rename them "plat" and "train".)
        if((!strcmp(cls, "plat") || !strcmp(cls, "func_plat")) && pos1 >= 0 && pos2 >= 0)
        {
            const glm::vec3 top = progs::fieldVec(e, pos1), bottom = progs::fieldVec(e, pos2);
            if(top.z - bottom.z > 1.f)
            {
                g.rides.pushBack(Ride{num, bottom, top, mins, maxs, true});
            }
        }
        else if((!strcmp(cls, "train") || !strcmp(cls, "func_train")) && e->v.target && wait >= 0)
        {
            za::Vector<glm::vec3> stops;
            const char* name = PR_GetString(e->v.target);
            const edict_t* first = nullptr;
            for(int k = 0; k < 64 && name && name[0]; ++k)
            {
                edict_t* corner = findByTargetname(name);
                if(!corner || corner == first)
                {
                    break;
                }
                first = first ? first : corner;
                if(progs::fieldFloat(corner, wait) > 0.f)
                {
                    stops.pushBack(geom::vec(corner->v.origin) - mins); // (train_next: the corner less its mins)
                }
                name = corner->v.target ? PR_GetString(corner->v.target) : nullptr;
            }
            for(za::SizeT k = 0; stops.size() >= 2 && k < stops.size(); ++k)
            {
                g.rides.pushBack(Ride{num, stops[k], stops[(k + 1) % stops.size()], mins, maxs, false});
            }
        }
    }
}

// Each brush's faces as triangles (on the pool: brushes::forEachPolygon per brush, chunks merged in order).
void triangulate(const BrushSet& set, Geometry& g)
{
    const float walkZ = za::cos(glm::radians(vr_nav_slope.value));
    struct Out
    {
        za::Vector<float> verts;
        za::Vector<int> tris;
        za::Vector<unsigned char> walkable;
        za::Vector<glm::vec4> boxes;
    };
    constexpr za::SizeT chunk = 256;
    const za::SizeT n = set.brushes.size();
    za::Vector<Out> outs;
    outs.resize((n + chunk - 1) / chunk);
    jobs::parallelFor(brushSite, outs.size(), 1, [&](za::SizeT begin, za::SizeT end) {
        brushes::Brush br;
        for(za::SizeT c = begin; c < end; ++c)
        {
            Out& o = outs[c];
            for(za::SizeT i = c * chunk; i < za::min(n, (c + 1) * chunk); ++i)
            {
                const hull::NavBrush& b = set.brushes[i];
                br.faces.clear();
                for(int k = 0; k < b.count; ++k)
                {
                    const glm::vec4& p = set.planes[static_cast<za::SizeT>(b.first + k)];
                    brushes::Face f;
                    f.normal = glm::dvec3{p.x, p.y, p.z};
                    f.dist = p.w;
                    br.faces.pushBack(f);
                }
                glm::vec4 box{1e30f, 1e30f, -1e30f, -1e30f};
                brushes::forEachPolygon(br, [&](int face, const glm::dvec3* pts, int count) {
                    const glm::vec4& p = set.planes[static_cast<za::SizeT>(b.first + face)];
                    const int base = static_cast<int>(o.verts.size() / 3);
                    for(int v = 0; v < count; ++v)
                    {
                        box = glm::vec4{za::min(box.x, static_cast<float>(pts[v].x)), za::min(box.y, static_cast<float>(pts[v].y)),
                            za::max(box.z, static_cast<float>(pts[v].x)), za::max(box.w, static_cast<float>(pts[v].y))};
                        const glm::vec3 r = toRc(glm::vec3{pts[v]});
                        o.verts.pushBack(r.x);
                        o.verts.pushBack(r.y);
                        o.verts.pushBack(r.z);
                    }
                    const unsigned char area = p.z >= walkZ ? RC_WALKABLE_AREA : RC_NULL_AREA;
                    for(int v = 1; v + 1 < count; ++v)
                    {
                        o.tris.pushBack(base); // (Recast rasterises either winding: the areas are set here)
                        o.tris.pushBack(base + v);
                        o.tris.pushBack(base + v + 1);
                        o.walkable.pushBack(area);
                    }
                });
                o.boxes.pushBack(box);
            }
        }
    });
    g.verts.clear();
    g.tris.clear();
    g.walkable.clear();
    for(const Out& o : outs)
    {
        const int base = static_cast<int>(g.verts.size() / 3);
        g.verts.emplaceBackRange(o.verts.data(), o.verts.size());
        for(const int t : o.tris)
        {
            g.tris.pushBack(base + t);
        }
        g.walkable.emplaceBackRange(o.walkable.data(), o.walkable.size());
        g.boxes.emplaceBackRange(o.boxes.data(), o.boxes.size());
    }
    g.bmin = glm::vec3{1e30f};
    g.bmax = glm::vec3{-1e30f};
    for(za::SizeT i = 0; i + 2 < g.verts.size(); i += 3)
    {
        const glm::vec3 v{g.verts[i], g.verts[i + 1], g.verts[i + 2]};
        g.bmin = glm::min(g.bmin, v);
        g.bmax = glm::max(g.bmax, v);
    }
    za::U64 h = diskcache::fnvStart;
    h = diskcache::wideHash(h, g.verts.data(), g.verts.size() * sizeof(float));
    h = diskcache::wideHash(h, g.tris.data(), g.tris.size() * sizeof(int));
    h = diskcache::wideHash(h, g.walkable.data(), g.walkable.size());
    g.hash = h;
}

// ---------------------------------------------------------------------------------------------------------------------
// The build.

struct Settings
{
    float cs, ch, climb, slope, clearance;
    int tile;
    float drops, dropMax, dropMaxBig, lifts; // (the links': vr_nav_drop, its heights, vr_nav_lifts)
};

Settings settingsNow()
{
    return Settings{za::max(vr_nav_cell.value, 0.5f), za::max(vr_nav_cell_height.value, 0.25f), za::max(vr_nav_climb.value, 0.f),
        vr_nav_slope.value, za::max(vr_nav_clearance.value, 0.f), za::max(static_cast<int>(vr_nav_tile.value), 8),
        vr_nav_drop.value != 0.f ? 1.f : 0.f, za::max(vr_nav_drop_max.value, 0.f), za::max(vr_nav_drop_max_big.value, 0.f),
        vr_nav_lifts.value != 0.f ? 1.f : 0.f};
}

unsigned char areaAt(const glm::vec3& q)
{
    const int c = worldtrace::contents(q);
    return c == CONTENTS_LAVA ? areaLava : c == CONTENTS_SLIME ? areaSlime : c == CONTENTS_WATER ? areaWater : areaGround;
}

// The tile's brushes filled: each cell's column through a brush (at the cell's middle) a solid span, merged with the
// spans of its faces (a walkable top keeps its area).
void fillSolids(rcContext& ctx, const Geometry& g, const za::Vector<int>& brushBin, const rcConfig& cfg, rcHeightfield& hf)
{
    for(const int bi : brushBin)
    {
        const hull::NavBrush& b = g.solids[static_cast<za::SizeT>(bi)];
        const glm::vec4& box = g.boxes[static_cast<za::SizeT>(bi)];
        // Recast's x is Quake's x, its z Quake's -y.
        const int x0 = za::max(0, static_cast<int>(za::floor((box.x - cfg.bmin[0]) / cfg.cs)));
        const int x1 = za::min(cfg.width - 1, static_cast<int>(za::floor((box.z - cfg.bmin[0]) / cfg.cs)));
        const int z0 = za::max(0, static_cast<int>(za::floor((-box.w - cfg.bmin[2]) / cfg.cs)));
        const int z1 = za::min(cfg.height - 1, static_cast<int>(za::floor((-box.y - cfg.bmin[2]) / cfg.cs)));
        for(int z = z0; z <= z1; ++z)
        {
            for(int x = x0; x <= x1; ++x)
            {
                const float qx = cfg.bmin[0] + (static_cast<float>(x) + 0.5f) * cfg.cs;
                const float qy = -(cfg.bmin[2] + (static_cast<float>(z) + 0.5f) * cfg.cs);
                float lo = -1e30f, hi = 1e30f;
                bool out = false;
                for(int k = 0; k < b.count && !out; ++k)
                {
                    const glm::vec4& p = g.planes[static_cast<za::SizeT>(b.first + k)];
                    const float side = p.x * qx + p.y * qy;
                    if(za::abs(p.z) < 1e-4f)
                    {
                        out = side >= p.w;
                    }
                    else if(p.z > 0.f)
                    {
                        hi = za::min(hi, (p.w - side) / p.z);
                    }
                    else
                    {
                        lo = za::max(lo, (p.w - side) / p.z);
                    }
                }
                if(out || lo >= hi)
                {
                    continue;
                }
                const float top = static_cast<float>(RC_SPAN_MAX_HEIGHT);
                const float smin = za::clamp(za::floor((lo - cfg.bmin[1]) / cfg.ch), 0.f, top);
                const float smax = za::clamp(za::ceil((hi - cfg.bmin[1]) / cfg.ch), 0.f, top);
                if(smax > smin)
                {
                    (void)rcAddSpan(&ctx, hf, x, z, static_cast<unsigned short>(smin), static_cast<unsigned short>(smax), RC_NULL_AREA,
                        cfg.walkableClimb);
                }
            }
        }
    }
}

// One tile of a class's mesh: its triangles (bins), Recast's steps, Detour's data. False: nothing walkable there.
bool buildTile(const Geometry& g, const za::Vector<int>& bin, const za::Vector<int>& brushBin, const rcConfig& base, int tx, int ty,
    TileSrc& out)
{
    rcConfig cfg = base;
    const float tw = static_cast<float>(cfg.tileSize) * cfg.cs;
    cfg.bmin[0] = g.bmin.x + static_cast<float>(tx) * tw - static_cast<float>(cfg.borderSize) * cfg.cs;
    cfg.bmin[1] = g.bmin.y;
    cfg.bmin[2] = g.bmin.z + static_cast<float>(ty) * tw - static_cast<float>(cfg.borderSize) * cfg.cs;
    cfg.bmax[0] = g.bmin.x + static_cast<float>(tx + 1) * tw + static_cast<float>(cfg.borderSize) * cfg.cs;
    cfg.bmax[1] = g.bmax.y;
    cfg.bmax[2] = g.bmin.z + static_cast<float>(ty + 1) * tw + static_cast<float>(cfg.borderSize) * cfg.cs;
    za::Vector<int> tris;
    za::Vector<unsigned char> areas;
    tris.reserve(bin.size() * 3);
    areas.reserve(bin.size());
    for(const int t : bin)
    {
        const auto i = static_cast<za::SizeT>(t);
        tris.pushBack(g.tris[i * 3]);
        tris.pushBack(g.tris[i * 3 + 1]);
        tris.pushBack(g.tris[i * 3 + 2]);
        areas.pushBack(g.walkable[i]);
    }
    rcContext ctx{false};
    bool ok = false;
    rcHeightfield* hf = rcAllocHeightfield();
    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    rcContourSet* cset = rcAllocContourSet();
    rcPolyMesh* pmesh = rcAllocPolyMesh();
    rcPolyMeshDetail* dmesh = rcAllocPolyMeshDetail();
    do
    {
        if(!hf || !chf || !cset || !pmesh || !dmesh || areas.empty())
        {
            break;
        }
        if(!rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch) ||
            !rcRasterizeTriangles(&ctx, g.verts.data(), static_cast<int>(g.verts.size() / 3), tris.data(), areas.data(),
                static_cast<int>(areas.size()), *hf, cfg.walkableClimb))
        {
            break;
        }
        fillSolids(ctx, g, brushBin, cfg, *hf);
        rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
        rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
        rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
        if(!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf) ||
            !rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf) || !rcBuildDistanceField(&ctx, *chf) ||
            !rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea) ||
            !rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset) || cset->nconts == 0 ||
            !rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh) || pmesh->npolys == 0 ||
            !rcBuildPolyMeshDetail(&ctx, *pmesh, *chf, cfg.detailSampleDist, cfg.detailSampleMaxError, *dmesh))
        {
            break;
        }
        // Each polygon's area by what fills the space over its middle (liquids cost more: vr_nav_*_cost).
        for(int i = 0; i < pmesh->npolys; ++i)
        {
            const unsigned short* p = &pmesh->polys[i * pmesh->nvp * 2];
            float mid[3]{0.f, 0.f, 0.f};
            int nv = 0;
            for(int k = 0; k < pmesh->nvp && p[k] != RC_MESH_NULL_IDX; ++k, ++nv)
            {
                const unsigned short* v = &pmesh->verts[p[k] * 3];
                mid[0] += pmesh->bmin[0] + static_cast<float>(v[0]) * pmesh->cs;
                mid[1] += pmesh->bmin[1] + static_cast<float>(v[1]) * pmesh->ch;
                mid[2] += pmesh->bmin[2] + static_cast<float>(v[2]) * pmesh->cs;
            }
            const float inv = 1.f / static_cast<float>(za::max(nv, 1));
            const float m[3]{mid[0] * inv, mid[1] * inv + 8.f, mid[2] * inv};
            pmesh->areas[i] = pmesh->areas[i] == RC_WALKABLE_AREA ? areaAt(fromRc(m)) : 0;
            pmesh->flags[i] = flagWalk;
        }
        ok = true;
    } while(false);
    rcFreeHeightField(hf);
    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);
    if(!ok)
    {
        rcFreePolyMesh(pmesh);
        rcFreePolyMeshDetail(dmesh);
        return false;
    }
    out = TileSrc{tx, ty, pmesh, dmesh};
    return true;
}

// A tile's Detour data from its polygons, with its links (those starting in it). False: none made.
bool tileData(const TileSrc& t, const NavClass& c, const Settings& s, const za::Vector<Link>& links, Tile& out)
{
    const rcPolyMesh& pm = *t.pmesh;
    const rcPolyMeshDetail& dm = *t.dmesh;
    dtNavMeshCreateParams p{};
    p.verts = pm.verts;
    p.vertCount = pm.nverts;
    p.polys = pm.polys;
    p.polyAreas = pm.areas;
    p.polyFlags = pm.flags;
    p.polyCount = pm.npolys;
    p.nvp = pm.nvp;
    p.detailMeshes = dm.meshes;
    p.detailVerts = dm.verts;
    p.detailVertsCount = dm.nverts;
    p.detailTris = dm.tris;
    p.detailTriCount = dm.ntris;
    za::Vector<float> verts, rads;
    za::Vector<unsigned char> dirs, areas;
    za::Vector<unsigned short> flags;
    za::Vector<unsigned> ids;
    for(const Link& l : links)
    {
        verts.emplaceBackRange(l.pos, 6);
        rads.pushBack(l.rad);
        dirs.pushBack(l.dir);
        areas.pushBack(l.area);
        flags.pushBack(l.flags);
        ids.pushBack(l.id);
    }
    p.offMeshConVerts = verts.data();
    p.offMeshConRad = rads.data();
    p.offMeshConDir = dirs.data();
    p.offMeshConAreas = areas.data();
    p.offMeshConFlags = flags.data();
    p.offMeshConUserID = ids.data();
    p.offMeshConCount = static_cast<int>(links.size());
    p.walkableHeight = c.height;
    p.walkableRadius = c.radius;
    p.walkableClimb = s.climb;
    p.tileX = t.x;
    p.tileY = t.y;
    p.tileLayer = 0;
    memcpy(p.bmin, pm.bmin, sizeof(p.bmin));
    memcpy(p.bmax, pm.bmax, sizeof(p.bmax));
    p.cs = pm.cs;
    p.ch = pm.ch;
    p.buildBvTree = true;
    unsigned char* data = nullptr;
    int size = 0;
    if(!dtCreateNavMeshData(&p, &data, &size))
    {
        dtFree(data);
        return false;
    }
    out = Tile{t.x, t.y, data, size};
    return true;
}

// The tiles' grid for a mesh: Detour's refs hold a tile and a polygon in 22 bits.
struct Grid
{
    int tilesX = 0, tilesY = 0, tile = 0;
    int polyBits = 0;
};

Grid gridFor(const Geometry& g, const Settings& s)
{
    Grid grid;
    grid.tile = s.tile;
    for(;;)
    {
        const float tw = static_cast<float>(grid.tile) * s.cs;
        grid.tilesX = za::max(1, static_cast<int>(za::ceil((g.bmax.x - g.bmin.x) / tw)));
        grid.tilesY = za::max(1, static_cast<int>(za::ceil((g.bmax.z - g.bmin.z) / tw)));
        const unsigned tiles = dtNextPow2(static_cast<unsigned>(grid.tilesX * grid.tilesY));
        const int tileBits = static_cast<int>(dtIlog2(tiles));
        if(tileBits <= 12 || grid.tile >= 1024)
        {
            grid.polyBits = za::min(22 - tileBits, 14);
            return grid;
        }
        grid.tile *= 2; // (a vast map: bigger tiles, so each keeps room for its polygons)
    }
}

dtNavMeshParams meshParams(const Geometry& g, const Settings& s, const Grid& grid)
{
    dtNavMeshParams p{};
    p.orig[0] = g.bmin.x;
    p.orig[1] = g.bmin.y;
    p.orig[2] = g.bmin.z;
    p.tileWidth = static_cast<float>(grid.tile) * s.cs;
    p.tileHeight = p.tileWidth;
    p.maxTiles = grid.tilesX * grid.tilesY;
    p.maxPolys = 1 << grid.polyBits;
    return p;
}

unsigned checksum(const char* p, za::SizeT n);

// A mesh's counts: its tiles, polygons, bytes and links (drops, lifts).
void countMesh(const dtNavMesh& mesh, NavClass& c)
{
    c.polys = c.tiles = c.drops = c.lifts = 0;
    c.bytes = 0;
    for(int i = 0; i < mesh.getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = mesh.getTile(i);
        if(!tile || !tile->header)
        {
            continue;
        }
        ++c.tiles;
        c.bytes += static_cast<za::SizeT>(tile->dataSize);
        for(int p = 0; p < tile->header->polyCount; ++p)
        {
            const dtPoly& poly = tile->polys[p];
            const bool con = poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION;
            c.polys += con ? 0 : 1;
            c.drops += con && poly.getArea() == areaDrop ? 1 : 0;
            c.lifts += con && poly.getArea() == areaLift ? 1 : 0;
        }
    }
}

// The solids by tile, for the links' room checks.
struct BrushGrid
{
    const Geometry& g;
    const za::Vector<za::Vector<int>>& bins;
    int tilesX, tilesY;
    float tw;
};

// Whether a box (Quake's space) meets one of the solids (more than half a unit into it, by its planes: a little
// cautious beside a brush's edges).
bool boxInSolid(const BrushGrid& bg, const glm::vec3& lo, const glm::vec3& hi)
{
    const Geometry& g = bg.g;
    // Recast's x is Quake's x, its z Quake's -y.
    const int x0 = za::max(0, static_cast<int>(za::floor((lo.x - g.bmin.x) / bg.tw)));
    const int x1 = za::min(bg.tilesX - 1, static_cast<int>(za::floor((hi.x - g.bmin.x) / bg.tw)));
    const int y0 = za::max(0, static_cast<int>(za::floor((-hi.y - g.bmin.z) / bg.tw)));
    const int y1 = za::min(bg.tilesY - 1, static_cast<int>(za::floor((-lo.y - g.bmin.z) / bg.tw)));
    const glm::vec3 mid = (lo + hi) * 0.5f, half = (hi - lo) * 0.5f;
    for(int y = y0; y <= y1; ++y)
    {
        for(int x = x0; x <= x1; ++x)
        {
            for(const int bi : bg.bins[static_cast<za::SizeT>(y * bg.tilesX + x)])
            {
                const glm::vec4& box = g.boxes[static_cast<za::SizeT>(bi)];
                if(box.x > hi.x || box.z < lo.x || box.y > hi.y || box.w < lo.y)
                {
                    continue;
                }
                const hull::NavBrush& b = g.solids[static_cast<za::SizeT>(bi)];
                bool in = b.count > 0;
                for(int k = 0; k < b.count && in; ++k)
                {
                    const glm::vec4& p = g.planes[static_cast<za::SizeT>(b.first + k)];
                    const float reach = za::abs(p.x) * half.x + za::abs(p.y) * half.y + za::abs(p.z) * half.z;
                    in = glm::dot(glm::vec3{p}, mid) - reach < p.w - 0.5f;
                }
                if(in)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// Whether a monster's box (half width r, height h, its feet `lift` over z) moves from a to b (flat) at height z
// without meeting a solid: boxes every half width along the way.
bool sweepClear(const BrushGrid& bg, const glm::vec2& a, const glm::vec2& b, float z, float lift, float r, float h)
{
    const float len = glm::length(b - a);
    const int steps = za::max(1, static_cast<int>(za::ceil(len / za::max(r * 0.5f, 2.f))));
    for(int i = 0; i <= steps; ++i)
    {
        const glm::vec2 p = a + (b - a) * (static_cast<float>(i) / static_cast<float>(steps));
        if(boxInSolid(bg, glm::vec3{p.x - r, p.y - r, z + lift}, glm::vec3{p.x + r, p.y + r, z + h}))
        {
            return false;
        }
    }
    return true;
}

// The tile index (in the build's grid) a point (Recast's space) is in; -1 outside.
int tileIndexOf(const dtNavMeshParams& p, int tilesX, int tilesY, const float* rc)
{
    const int x = static_cast<int>(za::floor((rc[0] - p.orig[0]) / p.tileWidth));
    const int y = static_cast<int>(za::floor((rc[2] - p.orig[2]) / p.tileHeight));
    return x < 0 || y < 0 || x >= tilesX || y >= tilesY ? -1 : y * tilesX + x;
}

// A link added to its start's tile, unless one already there starts and ends near it (a ledge's edges side by side).
void addLink(za::Vector<za::Vector<Link>>& links, int tile, const Link& l, float near)
{
    if(tile < 0)
    {
        return;
    }
    za::Vector<Link>& list = links[static_cast<za::SizeT>(tile)];
    for(const Link& o : list)
    {
        if(o.area == l.area && dtVdist(o.pos, l.pos) < near && dtVdist(o.pos + 3, l.pos + 3) < near * 1.5f)
        {
            return;
        }
    }
    list.pushBack(l);
}

// Drop links: from the middle of each mesh edge with nothing beyond it (a wall or a ledge), where the monster's box
// walks off unhindered and falls clear to a floor polygon below (more than a step down, at most the class's highest
// drop; not slime or lava).
void findDrops(const Geometry& g, const Settings& s, const NavClass& c, const BrushGrid& bg, const dtNavMesh& mesh,
    const dtNavMeshQuery& query, za::Vector<za::Vector<Link>>& links)
{
    const float maxDrop = c.height > 60.f ? s.dropMaxBig : s.dropMax;
    const float r = c.radius, h = c.height;
    const float out = r * s.clearance + r * 1.42f + 4.f; // past the mesh's edge till the box clears the ledge at any angle
    dtQueryFilter all;
    (void)g;
    for(int t = 0; t < mesh.getMaxTiles(); ++t)
    {
        const dtMeshTile* tile = mesh.getTile(t);
        if(!tile || !tile->header)
        {
            continue;
        }
        const int tileIndex = tile->header->y * bg.tilesX + tile->header->x;
        for(int p = 0; p < tile->header->polyCount; ++p)
        {
            const dtPoly& poly = tile->polys[p];
            if(poly.getType() != DT_POLYTYPE_GROUND || poly.getArea() == areaLava || poly.getArea() == areaSlime)
            {
                continue;
            }
            float cen[3]{0.f, 0.f, 0.f};
            for(int k = 0; k < poly.vertCount; ++k)
            {
                dtVadd(cen, cen, &tile->verts[poly.verts[k] * 3]);
            }
            dtVscale(cen, cen, 1.f / static_cast<float>(poly.vertCount));
            for(int k = 0; k < poly.vertCount; ++k)
            {
                if(poly.neis[k] != 0)
                {
                    continue;
                }
                const float* va = &tile->verts[poly.verts[k] * 3];
                const float* vb = &tile->verts[poly.verts[(k + 1) % poly.vertCount] * 3];
                const float ex = vb[0] - va[0], ez = vb[2] - va[2];
                const float len = za::sqrt(ex * ex + ez * ez);
                if(len < 8.f)
                {
                    continue;
                }
                float m[3];
                dtVlerp(m, va, vb, 0.5f);
                float nx = ez / len, nz = -ex / len;
                if(nx * (m[0] - cen[0]) + nz * (m[2] - cen[2]) < 0.f)
                {
                    nx = -nx;
                    nz = -nz;
                }
                const glm::vec3 mq = fromRc(m);
                const glm::vec2 dir{nx, -nz}; // (Quake's: Recast's z is Quake's -y)
                const glm::vec2 at{mq.x, mq.y}, beyond = at + dir * out;
                const float top = mq.z;
                if(!sweepClear(bg, at, beyond, top, s.climb * 0.5f, r, h))
                {
                    continue;
                }
                // The first floor under the place beyond: a polygon right under it.
                const float probe[3] = {beyond.x, top, -beyond.y};
                const float centre[3] = {beyond.x, top - (s.climb + maxDrop) * 0.5f, -beyond.y};
                const float ext[3] = {2.f, (maxDrop - s.climb) * 0.5f + 2.f, 2.f};
                dtPolyRef found[32];
                int n = 0;
                if(dtStatusFailed(query.queryPolygons(centre, ext, &all, found, &n, 32)))
                {
                    continue;
                }
                float best = -1e30f;
                for(int i = 0; i < n; ++i)
                {
                    const dtMeshTile* lt = nullptr;
                    const dtPoly* lp = nullptr;
                    float cl[3];
                    bool over = false;
                    if(dtStatusFailed(mesh.getTileAndPolyByRef(found[i], &lt, &lp)) || lp->getArea() == areaLava ||
                        lp->getArea() == areaSlime || dtStatusFailed(query.closestPointOnPoly(found[i], probe, cl, &over)) || !over)
                    {
                        continue;
                    }
                    const float drop = top - cl[1];
                    if(drop > s.climb + 2.f && drop <= maxDrop && cl[1] > best)
                    {
                        best = cl[1];
                    }
                }
                if(best < -1e29f || !sweepClear(bg, beyond, beyond, best, s.climb * 0.5f, r, top - best + h))
                {
                    continue;
                }
                Link l{};
                dtVcopy(l.pos, m);
                l.pos[3] = beyond.x;
                l.pos[4] = best;
                l.pos[5] = -beyond.y;
                l.rad = s.cs * 2.f;
                l.dir = 0;
                l.area = areaDrop;
                l.flags = flagWalk | flagDrop;
                l.id = 0;
                addLink(links, tileIndex, l, 32.f);
            }
        }
    }
}

// The places beside a mover's footprint at a stop (its origin there) where a monster stands to board it: one on each
// side where the mesh comes within reach at the mover's top's height and the way onto it is clear.
void rideEnds(const Settings& s, const NavClass& c, const BrushGrid& bg, const dtNavMeshQuery& query, const Ride& ride,
    const glm::vec3& org, za::Vector<glm::vec3>& ends)
{
    ends.clear();
    const float r = c.radius, top = org.z + ride.maxs.z;
    const glm::vec2 lo{org.x + ride.mins.x, org.y + ride.mins.y}, hi{org.x + ride.maxs.x, org.y + ride.maxs.y};
    const glm::vec2 mid = (lo + hi) * 0.5f;
    const glm::vec2 sides[4][2] = {{{hi.x, mid.y}, {1.f, 0.f}}, {{lo.x, mid.y}, {-1.f, 0.f}}, {{mid.x, hi.y}, {0.f, 1.f}},
        {{mid.x, lo.y}, {0.f, -1.f}}};
    dtQueryFilter all;
    const float reach = r * s.clearance + 16.f;
    for(const auto& side : sides)
    {
        const glm::vec2 q = side[0] + side[1] * (r * s.clearance + 8.f);
        const float at[3] = {q.x, top, -q.y};
        const float ext[3] = {reach, s.climb + 8.f, reach};
        dtPolyRef ref = 0;
        float near[3];
        if(dtStatusFailed(query.findNearestPoly(at, ext, &all, &ref, near)) || !ref)
        {
            continue;
        }
        const glm::vec3 nq = fromRc(near);
        const glm::vec2 onto = side[0] - side[1] * za::min(r, glm::length(hi - lo) * 0.25f);
        if(za::abs(nq.z - top) > s.climb || !sweepClear(bg, glm::vec2{nq}, onto, top, s.climb * 0.5f, r, c.height))
        {
            continue;
        }
        ends.pushBack(nq);
    }
}

// Ride links: each ride's boarding places at one stop to those at the other (both ways for a lift).
void findRides(const Geometry& g, const Settings& s, const NavClass& c, const BrushGrid& bg, const dtNavMesh& mesh,
    const dtNavMeshQuery& query, za::Vector<za::Vector<Link>>& links)
{
    const dtNavMeshParams* p = mesh.getParams();
    za::Vector<glm::vec3> ea, eb;
    for(za::SizeT ri = 0; ri < g.rides.size(); ++ri)
    {
        const Ride& ride = g.rides[ri];
        rideEnds(s, c, bg, query, ride, ride.a, ea);
        rideEnds(s, c, bg, query, ride, ride.b, eb);
        for(const glm::vec3& a : ea)
        {
            for(const glm::vec3& b : eb)
            {
                if(glm::length(a - b) < 32.f)
                {
                    continue;
                }
                Link l{};
                const glm::vec3 ra = toRc(a), rb = toRc(b);
                dtVcopy(l.pos, &ra.x);
                dtVcopy(l.pos + 3, &rb.x);
                l.rad = s.cs * 2.f;
                l.dir = ride.both ? DT_OFFMESH_CON_BIDIR : 0;
                l.area = areaLift;
                l.flags = flagWalk | flagLift;
                l.id = static_cast<unsigned>(ri) + 1;
                const int ta = tileIndexOf(*p, bg.tilesX, bg.tilesY, l.pos), tb = tileIndexOf(*p, bg.tilesX, bg.tilesY, l.pos + 3);
                // (Detour joins a link's far end only to its start tile's neighbours.)
                if(ta >= 0 && tb >= 0 && za::abs(ta % bg.tilesX - tb % bg.tilesX) <= 1 && za::abs(ta / bg.tilesX - tb / bg.tilesX) <= 1)
                {
                    addLink(links, ta, l, 1.f);
                }
            }
        }
    }
}

// The class's tiles built (any thread: the pool's job), into c.built.
void buildClass(const Geometry& g, const Settings& s, NavClass& c)
{
    const auto t0 = za::Clock::nowNanoseconds();
    const Grid grid = gridFor(g, s);
    c.params = meshParams(g, s, grid);
    rcConfig cfg{};
    cfg.cs = s.cs;
    cfg.ch = s.ch;
    cfg.walkableSlopeAngle = s.slope;
    cfg.walkableHeight = static_cast<int>(za::ceil(c.height / s.ch));
    cfg.walkableClimb = static_cast<int>(za::floor(s.climb / s.ch));
    // Kept off walls and ledges by the box's reach at any angle (a square's corner: radius times up to 1.41), so a
    // path's corner never puts a box corner over a ledge (SV_CheckBottom) or into a slanted wall.
    cfg.walkableRadius = static_cast<int>(za::ceil(c.radius * s.clearance / s.cs));
    cfg.maxEdgeLen = static_cast<int>(48.f / s.cs);
    cfg.maxSimplificationError = 1.3f;
    cfg.minRegionArea = static_cast<int>(rcSqr(24.f / s.cs));
    cfg.mergeRegionArea = static_cast<int>(rcSqr(80.f / s.cs));
    cfg.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
    cfg.tileSize = grid.tile;
    cfg.borderSize = cfg.walkableRadius + 3;
    cfg.width = cfg.tileSize + cfg.borderSize * 2;
    cfg.height = cfg.width;
    cfg.detailSampleDist = s.cs * 6.f;
    cfg.detailSampleMaxError = s.ch;
    // Each triangle into the tiles its bounds (with the border) reach.
    const int tilesX = grid.tilesX, tilesY = grid.tilesY;
    const float tw = static_cast<float>(cfg.tileSize) * s.cs;
    const float border = static_cast<float>(cfg.borderSize) * s.cs;
    za::Vector<za::Vector<int>> bins;
    bins.resize(static_cast<za::SizeT>(tilesX * tilesY));
    const int triCount = static_cast<int>(g.walkable.size());
    for(int t = 0; t < triCount; ++t)
    {
        float lo[2]{1e30f, 1e30f}, hi[2]{-1e30f, -1e30f};
        for(int k = 0; k < 3; ++k)
        {
            const float* v = &g.verts[static_cast<za::SizeT>(g.tris[static_cast<za::SizeT>(t * 3 + k)] * 3)];
            lo[0] = za::min(lo[0], v[0]);
            hi[0] = za::max(hi[0], v[0]);
            lo[1] = za::min(lo[1], v[2]);
            hi[1] = za::max(hi[1], v[2]);
        }
        const int x0 = za::max(0, static_cast<int>(za::floor((lo[0] - border - g.bmin.x) / tw)));
        const int x1 = za::min(tilesX - 1, static_cast<int>(za::floor((hi[0] + border - g.bmin.x) / tw)));
        const int y0 = za::max(0, static_cast<int>(za::floor((lo[1] - border - g.bmin.z) / tw)));
        const int y1 = za::min(tilesY - 1, static_cast<int>(za::floor((hi[1] + border - g.bmin.z) / tw)));
        for(int y = y0; y <= y1; ++y)
        {
            for(int x = x0; x <= x1; ++x)
            {
                bins[static_cast<za::SizeT>(y * tilesX + x)].pushBack(t);
            }
        }
    }
    za::Vector<za::Vector<int>> brushBins;
    brushBins.resize(bins.size());
    for(za::SizeT b = 0; b < g.boxes.size(); ++b)
    {
        const glm::vec4& box = g.boxes[b];
        if(box.x > box.z)
        {
            continue; // (no faces)
        }
        const int x0 = za::max(0, static_cast<int>(za::floor((box.x - border - g.bmin.x) / tw)));
        const int x1 = za::min(tilesX - 1, static_cast<int>(za::floor((box.z + border - g.bmin.x) / tw)));
        const int y0 = za::max(0, static_cast<int>(za::floor((-box.w - border - g.bmin.z) / tw)));
        const int y1 = za::min(tilesY - 1, static_cast<int>(za::floor((-box.y + border - g.bmin.z) / tw)));
        for(int y = y0; y <= y1; ++y)
        {
            for(int x = x0; x <= x1; ++x)
            {
                brushBins[static_cast<za::SizeT>(y * tilesX + x)].pushBack(static_cast<int>(b));
            }
        }
    }
    za::Vector<TileSrc> srcs;
    za::Vector<unsigned char> has;
    srcs.resize(bins.size(), TileSrc{0, 0, nullptr, nullptr});
    has.resize(bins.size(), 0);
    jobs::parallelFor(tileSite, bins.size(), 1, [&](za::SizeT begin, za::SizeT end) {
        for(za::SizeT i = begin; i < end; ++i)
        {
            const int x = static_cast<int>(i) % tilesX, y = static_cast<int>(i) / tilesX;
            has[i] = !bins[i].empty() && buildTile(g, bins[i], brushBins[i], cfg, x, y, srcs[i]) ? 1 : 0;
        }
    });
    // The mesh without links, then the links found on it, then each tile with links made again with them.
    dtNavMesh* mesh = dtAllocNavMesh();
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    za::Vector<dtTileRef> refs;
    refs.resize(bins.size(), 0);
    bool ok = mesh && query && dtStatusSucceed(mesh->init(&c.params));
    const za::Vector<Link> none;
    for(za::SizeT i = 0; i < srcs.size() && ok; ++i)
    {
        Tile t{};
        if(has[i] && tileData(srcs[i], c, s, none, t) && dtStatusFailed(mesh->addTile(t.data, t.size, DT_TILE_FREE_DATA, 0, &refs[i])))
        {
            dtFree(t.data);
        }
    }
    ok = ok && dtStatusSucceed(query->init(mesh, queryNodes));
    if(ok)
    {
        za::Vector<za::Vector<Link>> links;
        links.resize(bins.size());
        const BrushGrid bg{g, brushBins, tilesX, tilesY, tw};
        if(s.drops != 0.f)
        {
            findDrops(g, s, c, bg, *mesh, *query, links);
        }
        if(s.lifts != 0.f)
        {
            findRides(g, s, c, bg, *mesh, *query, links);
        }
        for(za::SizeT i = 0; i < srcs.size(); ++i)
        {
            Tile t{};
            if(links[i].empty() || !refs[i] || !tileData(srcs[i], c, s, links[i], t))
            {
                continue;
            }
            (void)mesh->removeTile(refs[i], nullptr, nullptr);
            if(dtStatusFailed(mesh->addTile(t.data, t.size, DT_TILE_FREE_DATA, 0, &refs[i])))
            {
                dtFree(t.data);
            }
        }
    }
    for(TileSrc& t : srcs)
    {
        rcFreePolyMesh(t.pmesh);
        rcFreePolyMeshDetail(t.dmesh);
    }
    if(!ok)
    {
        dtFreeNavMeshQuery(query);
        dtFreeNavMesh(mesh);
        mesh = nullptr;
        query = nullptr;
    }
    c.jobMesh = mesh;
    c.jobQuery = query;
    // The cache file's data: the params, then each tile (x, y, size, its data).
    c.blob.clear();
    c.blobCount = 0;
    if(mesh)
    {
        const auto put = [&c](const void* p, za::SizeT n) { c.blob.emplaceBackRange(static_cast<const char*>(p), n); };
        put(&c.params, sizeof(c.params));
        const dtNavMesh* cm = mesh;
        for(int i = 0; i < cm->getMaxTiles(); ++i)
        {
            const dtMeshTile* tile = cm->getTile(i);
            if(tile && tile->header && tile->data)
            {
                const int head[3] = {tile->header->x, tile->header->y, tile->dataSize};
                put(head, sizeof(head));
                put(tile->data, static_cast<za::SizeT>(tile->dataSize));
                ++c.blobCount;
            }
        }
        c.blobSum = checksum(c.blob.data(), c.blob.size());
        countMesh(*mesh, c);
    }
    c.ms = msSince(t0);
}

// ---------------------------------------------------------------------------------------------------------------------
// The disk cache: a class's mesh as its params and tiles; the file's second number a checksum of the rest.

za::String buildName()
{
    const unsigned parts[] = {navVersion, static_cast<unsigned>(DT_NAVMESH_VERSION)};
    return diskcache::buildOf(parts, 2);
}

za::U64 keyOf(const Geometry& g, const Settings& s, const NavClass& c)
{
    const float v[] = {s.cs, s.ch, s.climb, s.slope, static_cast<float>(s.tile), c.radius, c.height, s.clearance, s.drops, s.dropMax,
        s.dropMaxBig, s.lifts};
    za::U64 h = diskcache::fnv(g.hash, v, sizeof(v));
    for(const Ride& r : g.rides)
    {
        const float w[] = {r.a.x, r.a.y, r.a.z, r.b.x, r.b.y, r.b.z, r.mins.x, r.mins.y, r.mins.z, r.maxs.x, r.maxs.y, r.maxs.z,
            r.both ? 1.f : 0.f};
        h = diskcache::fnv(h, w, sizeof(w));
    }
    return h;
}

unsigned checksum(const char* p, za::SizeT n)
{
    const za::U64 h = diskcache::fnv(diskcache::fnvStart, p, n);
    return static_cast<unsigned>(h ^ (h >> 32));
}

void freeTiles(za::Vector<Tile>& tiles)
{
    for(Tile& t : tiles)
    {
        dtFree(t.data);
    }
    tiles.clear();
}

bool deserialise(const za::Vector<char>& in, unsigned count, NavClass& c)
{
    za::SizeT at = 0;
    const auto get = [&](void* p, za::SizeT n) {
        if(at + n > in.size())
        {
            return false;
        }
        memcpy(p, in.data() + at, n);
        at += n;
        return true;
    };
    if(!get(&c.params, sizeof(c.params)))
    {
        return false;
    }
    for(unsigned i = 0; i < count; ++i)
    {
        int head[3];
        if(!get(head, sizeof(head)) || head[2] <= 0 || at + static_cast<za::SizeT>(head[2]) > in.size())
        {
            freeTiles(c.built);
            return false;
        }
        auto* data = static_cast<unsigned char*>(dtAlloc(static_cast<size_t>(head[2]), DT_ALLOC_PERM));
        memcpy(data, in.data() + at, static_cast<za::SizeT>(head[2]));
        at += static_cast<za::SizeT>(head[2]);
        c.built.pushBack(Tile{head[0], head[1], data, head[2]});
    }
    return at == in.size();
}

// ---------------------------------------------------------------------------------------------------------------------
// Classes.

void releaseClass(NavClass& c)
{
    if(c.pending)
    {
        c.job.wait();
        c.pending = false;
    }
    freeTiles(c.built);
    dtFreeNavMeshQuery(c.query);
    dtFreeNavMesh(c.mesh);
    dtFreeNavMeshQuery(c.jobQuery);
    dtFreeNavMesh(c.jobMesh);
    c.query = nullptr;
    c.mesh = nullptr;
    c.jobQuery = nullptr;
    c.jobMesh = nullptr;
}

void resetAll()
{
    for(NavClass& c : state.classes)
    {
        releaseClass(c);
    }
    state.classes.clear();
    state.movers.clear();
    state.agents.clear();
    state.ready = false;
}

void printClass(const NavClass& c);

// The tiles read from the disk cache made the class's mesh (main thread).
void finishClass(NavClass& c)
{
    c.mesh = dtAllocNavMesh();
    c.failed = !c.mesh || dtStatusFailed(c.mesh->init(&c.params));
    for(Tile& t : c.built)
    {
        if(!c.failed && dtStatusSucceed(c.mesh->addTile(t.data, t.size, DT_TILE_FREE_DATA, 0, nullptr)))
        {
            t.data = nullptr; // (the mesh's now)
        }
    }
    freeTiles(c.built);
    c.query = c.failed ? nullptr : dtAllocNavMeshQuery();
    if(c.query && dtStatusFailed(c.query->init(c.mesh, queryNodes)))
    {
        dtFreeNavMeshQuery(c.query);
        c.query = nullptr;
    }
    c.failed = c.failed || !c.query;
    if(c.mesh)
    {
        countMesh(*c.mesh, c);
    }
    printClass(c);
}

void printClass(const NavClass& c)
{
    Con_DPrintf("nav: class r%.0f h%.0f: %d tiles, %d polygons, %d drop and %d lift links, %.1f KB, %s %.1f ms\n", c.radius,
        c.height, c.tiles, c.polys, c.drops, c.lifts, static_cast<double>(c.bytes) / 1024.0, c.fromDisk ? "read" : "built", c.ms);
}

// A class's mesh from the disk cache, or built: on the pool (async) or now.
void startClass(za::SizeT index, bool async)
{
    NavClass& c = state.classes[index];
    const Settings s = settingsNow();
    c.key = keyOf(state.geo, s, c);
    const int mode = diskcache::modeOf(diskcache::Product::NavMesh);
    const za::String build = buildName();
    if(mode == 1)
    {
        const auto t0 = za::Clock::nowNanoseconds();
        unsigned count = 0, sum = 0;
        za::Vector<char> data;
        if(diskcache::read(diskcache::Product::NavMesh, build.cStr(), c.key, "nav", navMagic, count, sum, data, "navmesh"))
        {
            if(checksum(data.data(), data.size()) == sum && deserialise(data, count, c))
            {
                c.fromDisk = true;
                c.ms = msSince(t0);
                diskcache::noteHit(diskcache::Product::NavMesh, data.size(), c.ms / 1000.0);
                finishClass(c);
                return;
            }
            freeTiles(c.built);
            diskcache::noteRejected(diskcache::Product::NavMesh);
        }
    }
    const za::SizeT i = index;
    const auto run = [i, s]() {
        NavClass& k = state.classes[i];
        buildClass(state.geo, s, k);
    };
    c.fromDisk = false;
    c.pending = true;
    if(async)
    {
        c.job = jobs::async(run);
        return;
    }
    run(); // (joined by its first use: readyClass)
}

// The class's build done: its mesh made, its file written (or compared). Main thread.
void joinClass(NavClass& c)
{
    if(!c.pending)
    {
        return;
    }
    if(c.job.valid())
    {
        c.job.get();
    }
    c.pending = false;
    const auto tj = za::Clock::nowNanoseconds();
    // (The job made the mesh, its query and the file's data: only the file's write is left here.)
    c.mesh = c.jobMesh;
    c.query = c.jobQuery;
    c.jobMesh = nullptr;
    c.jobQuery = nullptr;
    c.failed = !c.query;
    const int mode = diskcache::modeOf(diskcache::Product::NavMesh);
    if(mode != 0 && !c.failed)
    {
        const za::Vector<char>& data = c.blob;
        const za::String build = buildName();
        if(mode == 2)
        {
            unsigned count = 0, oldSum = 0;
            za::Vector<char> old;
            if(diskcache::read(diskcache::Product::NavMesh, build.cStr(), c.key, "nav", navMagic, count, oldSum, old, "navmesh"))
            {
                const bool same = old.size() == data.size() && oldSum == c.blobSum && !memcmp(old.data(), data.data(), data.size());
                diskcache::noteChecked(diskcache::Product::NavMesh, same, "navmesh");
            }
        }
        diskcache::write(diskcache::Product::NavMesh, build.cStr(), c.key, "nav", navMagic, c.blobCount, c.blobSum, data.data(),
            data.size());
    }
    c.blob = za::Vector<char>{};
    printClass(c);
    c.joinMs = msSince(tj);
}

// The class index for a monster's size (-1: none yet); `make`: added and its build started when missing.
int classFor(const edict_t* ent, bool make)
{
    float width = 0.f, height = 0.f;
    hull::moveSize(ent, width, height);
    const float step = za::max(vr_nav_radius_step.value, 1.f);
    const float radius = za::ceil(width * 0.5f / step) * step;
    for(za::SizeT i = 0; i < state.classes.size(); ++i)
    {
        if(state.classes[i].radius == radius && state.classes[i].height == height)
        {
            return static_cast<int>(i);
        }
    }
    // (Capacity reserved at the load: a job holds its class by index while others are added.)
    if(!make || !state.ready || state.classes.size() >= maxClasses)
    {
        return -1;
    }
    const za::SizeT index = state.classes.size();
    state.classes.emplaceBack();
    state.classes[index].radius = radius;
    state.classes[index].height = height;
    startClass(index, vr_nav_async.value != 0.f);
    return static_cast<int>(index);
}

bool walker(const edict_t* e)
{
    const int flags = static_cast<int>(e->v.flags);
    return (flags & FL_MONSTER) && !(flags & (FL_FLY | FL_SWIM)) && static_cast<int>(e->v.solid) == SOLID_SLIDEBOX;
}

// A class ready to answer (its build joined when done).
NavClass* readyClass(int index)
{
    if(index < 0 || static_cast<za::SizeT>(index) >= state.classes.size())
    {
        return nullptr;
    }
    NavClass& c = state.classes[static_cast<za::SizeT>(index)];
    if(c.pending && (c.job.ready() || !c.job.valid()))
    {
        joinClass(c);
    }
    return !c.pending && c.query ? &c : nullptr; // (while its job runs, the job's)
}

// ---------------------------------------------------------------------------------------------------------------------
// Movers: the floor a door, lift or train stands on is blocked while it does.

void refreshMovers()
{
    const double now = sv.qcvm.time;
    if(now < state.moversAt)
    {
        return;
    }
    state.moversAt = now + static_cast<double>(za::max(vr_nav_movers.value, 0.02f));
    // (Meanwhile the classes of walkers spawned in play, built on the pool before they first chase.)
    progs::forEachEdict(1, [](edict_t* e) {
        if(walker(e) && e->v.health > 0.f)
        {
            (void)classFor(e, true);
        }
    });
    for(NavClass& c : state.classes)
    {
        if(c.pending || !c.mesh)
        {
            continue;
        }
        for(const dtPolyRef r : c.blocked)
        {
            (void)c.mesh->setPolyFlags(r, flagWalk);
        }
        c.blocked.clear();
        dtQueryFilter all;
        for(const int num : state.movers)
        {
            const edict_t* e = progs::edictAt(num);
            if(!e || static_cast<int>(e->v.solid) != SOLID_BSP)
            {
                continue;
            }
            const glm::vec3 lo = geom::vec(e->v.absmin), hi = geom::vec(e->v.absmax);
            const glm::vec3 mid = toRc((lo + hi) * 0.5f);
            const float ext[3] = {(hi.x - lo.x) * 0.5f + c.radius, (hi.z - lo.z) * 0.5f + c.height, (hi.y - lo.y) * 0.5f + c.radius};
            dtPolyRef refs[128];
            int n = 0;
            if(dtStatusFailed(c.query->queryPolygons(&mid.x, ext, &all, refs, &n, 128)))
            {
                continue;
            }
            for(int i = 0; i < n; ++i)
            {
                const dtMeshTile* tile = nullptr;
                const dtPoly* poly = nullptr;
                if(dtStatusFailed(c.mesh->getTileAndPolyByRef(refs[i], &tile, &poly)))
                {
                    continue;
                }
                float zlo = 1e30f, zhi = -1e30f;
                for(int k = 0; k < poly->vertCount; ++k)
                {
                    const float y = tile->verts[poly->verts[k] * 3 + 1];
                    zlo = za::min(zlo, y);
                    zhi = za::max(zhi, y);
                }
                // In the way: over the floor's step and under a monster's head.
                if(lo.z < zhi + c.height - 1.f && hi.z > zlo + vr_nav_climb.value)
                {
                    (void)c.mesh->setPolyFlags(refs[i], flagWalk | flagBlocked);
                    c.blocked.pushBack(refs[i]);
                }
            }
        }
    }
}

void setCosts()
{
    filter.setIncludeFlags(flagWalk);
    filter.setExcludeFlags(flagBlocked);
    filter.setAreaCost(areaGround, 1.f);
    filter.setAreaCost(areaWater, za::max(vr_nav_water_cost.value, 1.f));
    filter.setAreaCost(areaSlime, za::max(vr_nav_slime_cost.value, 1.f));
    filter.setAreaCost(areaLava, za::max(vr_nav_lava_cost.value, 1.f));
    filter.setAreaCost(areaDrop, za::max(vr_nav_drop_cost.value, 0.1f));
    filter.setAreaCost(areaLift, za::max(vr_nav_lift_cost.value, 0.1f));
    filterNoLinks = filter;
    filterNoLinks.setExcludeFlags(flagBlocked | flagDrop | flagLift);
}

glm::vec3 feet(const edict_t* e)
{
    return {e->v.origin[0], e->v.origin[1], e->v.origin[2] + e->v.mins[2]};
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

void info_f()
{
    if(!state.ready)
    {
        Con_Printf("nav: no map\n");
        return;
    }
    for(NavClass& c : state.classes)
    {
        joinClass(c); // (a diagnostic: waits for the builds under way)
    }
    const Geometry& g = state.geo;
    Con_Printf("nav: %d brushes (%d clip, %d of still brush entities), %d triangles, gathered in %.1f ms; %d movers\n", g.brushes,
        g.clipBrushes, g.entityBrushes, static_cast<int>(g.walkable.size()), g.ms, static_cast<int>(state.movers.size()));
    for(za::SizeT i = 0; i < state.classes.size(); ++i)
    {
        const NavClass& c = state.classes[i];
        Con_Printf("nav: class %d: radius %.0f height %.0f: %s, %d tiles, %d polygons, %d drop and %d lift links, %.1f KB, %s in %.1f ms "
                   "(joined in %.1f ms), %d blocked\n",
            static_cast<int>(i), c.radius, c.height, c.pending ? "building" : c.failed ? "FAILED" : "ready", c.tiles, c.polys, c.drops,
            c.lifts, static_cast<double>(c.bytes) / 1024.0, c.fromDisk ? "read" : "built", c.ms, c.joinMs,
            static_cast<int>(c.blocked.size()));
    }
    // The walkers now on the map by class (a class's first one named), and any without a class (none expected).
    za::Vector<int> counts;
    za::Vector<const char*> names;
    counts.resize(state.classes.size() + 1, 0);
    names.resize(state.classes.size() + 1, "");
    {
        const progs::ServerVm vm;
        progs::forEachEdict(1, [&](edict_t* e) {
            if(walker(e) && e->v.health > 0.f)
            {
                const int k = classFor(e, false);
                const za::SizeT slot = k < 0 ? state.classes.size() : static_cast<za::SizeT>(k);
                names[slot] = counts[slot]++ == 0 && e->v.classname ? PR_GetString(e->v.classname) : names[slot];
            }
        });
    }
    for(za::SizeT i = 0; i < counts.size(); ++i)
    {
        if(counts[i] > 0)
        {
            Con_Printf("nav: %d walkers of class %s (%s...)\n", counts[i], i < state.classes.size() ? va("%d", static_cast<int>(i)) : "NONE",
                names[i]);
        }
    }
    Con_Printf("nav: %d steers, %d paths found, %d left to Quake's chase; %d drops and %d rides taken, %d links given up; %d rides "
               "on the map\n",
        state.steers, state.paths, state.fallbacks, state.drops, state.rides, state.linkFails, static_cast<int>(g.rides.size()));
}

// vr_nav_cliptest: no navmesh polygon stands inside a clip brush (it would be a path through it). Ends on its verdict.
void cliptest_f()
{
    BrushSet set;
    if(!state.ready || !hull::navSolids(0, true, set.planes, set.brushes))
    {
        Con_Printf("nav cliptest: no map\n");
        testkit::verdict("nav cliptest", 0, 1);
        return;
    }
    int clips = 0, inside = 0, polysChecked = 0;
    for(za::SizeT ci = 0; ci < state.classes.size(); ++ci)
    {
        NavClass* c = readyClass(static_cast<int>(ci));
        if(!c)
        {
            continue;
        }
        for(int t = 0; t < c->mesh->getMaxTiles(); ++t)
        {
            const dtMeshTile* tile = static_cast<const dtNavMesh*>(c->mesh)->getTile(t);
            if(!tile || !tile->header)
            {
                continue;
            }
            for(int p = 0; p < tile->header->polyCount; ++p)
            {
                const dtPoly& poly = tile->polys[p];
                glm::vec3 mid{0.f};
                for(int k = 0; k < poly.vertCount; ++k)
                {
                    mid += fromRc(&tile->verts[poly.verts[k] * 3]);
                }
                mid /= static_cast<float>(za::max<int>(poly.vertCount, 1));
                mid.z += 1.f;
                ++polysChecked;
                for(const hull::NavBrush& b : set.brushes)
                {
                    if(!b.clip)
                    {
                        continue;
                    }
                    bool in = true;
                    for(int k = 0; k < b.count && in; ++k)
                    {
                        const glm::vec4& pl = set.planes[static_cast<za::SizeT>(b.first + k)];
                        in = glm::dot(glm::vec3{pl}, mid) < pl.w - 0.5f;
                    }
                    if(in && inside < 8)
                    {
                        Con_Printf("nav cliptest: class %d polygon at %.0f %.0f %.0f inside clip brush %d\n",
                            static_cast<int>(ci), mid.x, mid.y, mid.z, static_cast<int>(&b - set.brushes.data()));
                    }
                    inside += in ? 1 : 0;
                }
            }
        }
    }
    for(const hull::NavBrush& b : set.brushes)
    {
        clips += b.clip ? 1 : 0;
    }
    Con_Printf("nav cliptest: %d clip brushes, %d polygons checked, %d inside one\n", clips, polysChecked, inside);
    const bool pass = inside == 0 && polysChecked > 0;
    testkit::verdict("nav cliptest", pass ? 1 : 0, pass ? 0 : 1);
}

// vr_nav_toptest: each polygon over a clip brush's or a brush entity's top (where Quake's box, traced down, stops
// higher than a point does, or on a brush entity) stands within the climb of where the box stands. Ends on its verdict.
void toptest_f()
{
    if(!state.ready)
    {
        Con_Printf("nav toptest: no map\n");
        testkit::verdict("nav toptest", 0, 1);
        return;
    }
    const progs::ServerVm vm;
    int tops = 0, off = 0, polysChecked = 0;
    float worst = 0.f;
    vec3_t boxMins{-16.f, -16.f, -24.f}, boxMaxs{16.f, 16.f, 32.f}, zero{0.f, 0.f, 0.f};
    for(za::SizeT ci = 0; ci < state.classes.size(); ++ci)
    {
        NavClass* c = readyClass(static_cast<int>(ci));
        if(!c)
        {
            continue;
        }
        for(int t = 0; t < c->mesh->getMaxTiles(); ++t)
        {
            const dtMeshTile* tile = static_cast<const dtNavMesh*>(c->mesh)->getTile(t);
            if(!tile || !tile->header)
            {
                continue;
            }
            const dtPolyRef base = c->mesh->getPolyRefBase(tile);
            for(int p = 0; p < tile->header->polyCount; ++p)
            {
                const dtPoly& poly = tile->polys[p];
                if(poly.getType() != DT_POLYTYPE_GROUND)
                {
                    continue;
                }
                float mid[3]{0.f, 0.f, 0.f};
                for(int k = 0; k < poly.vertCount; ++k)
                {
                    dtVadd(mid, mid, &tile->verts[poly.verts[k] * 3]);
                }
                dtVscale(mid, mid, 1.f / static_cast<float>(za::max<int>(poly.vertCount, 1)));
                float h = mid[1];
                (void)c->query->getPolyHeight(base | static_cast<dtPolyRef>(p), mid, &h);
                mid[1] = h;
                const glm::vec3 q = fromRc(mid);
                ++polysChecked;
                vec3_t from{q.x, q.y, q.z + 40.f}, to{q.x, q.y, q.z - 64.f}, pfrom{q.x, q.y, q.z + 16.f};
                const trace_t box = SV_Move(from, boxMins, boxMaxs, to, MOVE_NOMONSTERS, sv.qcvm.edicts);
                const trace_t pt = SV_Move(pfrom, zero, zero, to, MOVE_NOMONSTERS, sv.qcvm.edicts);
                if(box.startsolid || box.allsolid || box.fraction >= 1.f)
                {
                    continue;
                }
                const float boxFloor = box.endpos[2] + boxMins[2];
                const float pointFloor = pt.fraction < 1.f && !pt.startsolid ? pt.endpos[2] : -1e30f;
                const bool entityTop = box.ent && box.ent != sv.qcvm.edicts;
                if(!entityTop && pointFloor > boxFloor - 4.f)
                {
                    continue; // (the world's own floor: not a clip brush's top)
                }
                ++tops;
                const float d = za::abs(q.z - boxFloor);
                worst = za::max(worst, d);
                if(d > vr_nav_climb.value)
                {
                    if(off < 8)
                    {
                        Con_Printf("nav toptest: class %d polygon at %.0f %.0f %.0f, %s top at %.0f (%+.0f)\n", static_cast<int>(ci),
                            q.x, q.y, q.z, entityTop ? "an entity's" : "a clip brush's", boxFloor, q.z - boxFloor);
                    }
                    ++off;
                }
            }
        }
    }
    // The input itself: each clip brush's top (its most upward face at its middle) against where Quake's box stands there.
    int clipTops = 0, clipOff = 0;
    const Geometry& g = state.geo;
    for(za::SizeT bi = 0; bi < g.solids.size(); ++bi)
    {
        const hull::NavBrush& b = g.solids[bi];
        const glm::vec4& box = g.boxes[bi];
        if(!b.clip || box.x > box.z)
        {
            continue;
        }
        const float x = (box.x + box.z) * 0.5f, y = (box.y + box.w) * 0.5f;
        float top = -1e30f, bestZ = 0.f;
        for(int k = 0; k < b.count; ++k)
        {
            const glm::vec4& pl = g.planes[static_cast<za::SizeT>(b.first + k)];
            if(pl.z > bestZ)
            {
                bestZ = pl.z;
                top = (pl.w - pl.x * x - pl.y * y) / pl.z;
            }
        }
        if(bestZ < 0.7f)
        {
            continue;
        }
        vec3_t from{x, y, top + 40.f}, to{x, y, top - 80.f};
        const trace_t tr = SV_Move(from, boxMins, boxMaxs, to, MOVE_NOMONSTERS, sv.qcvm.edicts);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
            continue;
        }
        ++clipTops;
        const float d = top - (tr.endpos[2] + boxMins[2]);
        if(za::abs(d) > 20.f) // (the centre space's 28 or 56; a narrow brush's box can stand a little higher on what is beside it)
        {
            if(clipOff < 8)
            {
                Con_Printf("nav toptest: clip brush %d at %.0f %.0f: top %.0f in the input, Quake's box stands at %.0f (%+.0f)\n",
                    static_cast<int>(bi), x, y, top, tr.endpos[2] + boxMins[2], d);
            }
            ++clipOff;
        }
    }
    Con_Printf("nav toptest: %d clip brush tops in the open, %d off by more than 20 units\n", clipTops, clipOff);
    off += clipOff; // (none expected since the clip tops' fix)
    Con_Printf("nav toptest: %d polygons checked, %d on clip or entity tops, %d off by more than the climb (worst %.1f)\n",
        polysChecked, tops, off, static_cast<double>(worst));
    const bool pass = off == 0 && polysChecked > 0;
    testkit::verdict("nav toptest", pass ? 1 : 0, pass ? 0 : 1);
}

// vr_nav_links [class]: a class's links (its drops and rides), where they start and end.
void links_f()
{
    NavClass* c = state.ready ? readyClass(Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0) : nullptr;
    if(c && c->pending)
    {
        joinClass(*c);
    }
    if(!c || !c->mesh)
    {
        Con_Printf("nav links: no such class\n");
        return;
    }
    for(za::SizeT i = 0; i < state.geo.rides.size(); ++i)
    {
        const Ride& r = state.geo.rides[i];
        Con_Printf("nav ride %d: edict %d, %s, from %.0f %.0f %.0f to %.0f %.0f %.0f, bounds %.0f %.0f %.0f .. %.0f %.0f %.0f\n",
            static_cast<int>(i) + 1, r.mover, r.both ? "both ways" : "one way", r.a.x, r.a.y, r.a.z, r.b.x, r.b.y, r.b.z, r.mins.x,
            r.mins.y, r.mins.z, r.maxs.x, r.maxs.y, r.maxs.z);
    }
    const dtNavMesh* mesh = c->mesh;
    for(int t = 0; t < mesh->getMaxTiles(); ++t)
    {
        const dtMeshTile* tile = mesh->getTile(t);
        for(int k = 0; tile && tile->header && k < tile->header->offMeshConCount; ++k)
        {
            const dtOffMeshConnection& con = tile->offMeshCons[k];
            const glm::vec3 a = fromRc(&con.pos[0]), b = fromRc(&con.pos[3]);
            Con_Printf("nav link: %s %.0f %.0f %.0f -> %.0f %.0f %.0f\n",
                tile->polys[con.poly].getArea() == areaDrop ? "drop" : va("ride %u", con.userId), a.x, a.y, a.z, b.x, b.y, b.z);
        }
    }
}

// vr_nav_reachtest <name> <distance> <seconds> [drop|ride]: a test's end: passes when a walker comes within the distance
// of the player (and, with drop or ride, after a link of that kind was taken to its end), fails after the seconds (game
// time). Prints its verdict line.
struct ReachTest
{
    za::String name;
    float distance = 0.f;
    double since = 0.0, until = 0.0;
    int need = 0; // 1 a drop, 2 a ride
    za::String only; // a classname: only that monster counts (the map's own are about too)
    bool armed = false;
};
ReachTest reachTest;

void reachtest_f()
{
    if(Cmd_Argc() < 4 || !sv.active)
    {
        Con_Printf("vr_nav_reachtest <name> <distance> <seconds> [drop|ride|any] [classname]\n");
        return;
    }
    reachTest.name = Cmd_Argv(1);
    reachTest.distance = static_cast<float>(atof(Cmd_Argv(2)));
    reachTest.since = sv.qcvm.time;
    reachTest.until = sv.qcvm.time + atof(Cmd_Argv(3));
    reachTest.need = Cmd_Argc() > 4 ? (!strcmp(Cmd_Argv(4), "drop") ? 1 : !strcmp(Cmd_Argv(4), "ride") ? 2 : 0) : 0;
    reachTest.only = Cmd_Argc() > 5 ? Cmd_Argv(5) : "";
    reachTest.armed = true;
    state.drops = state.rides = state.linkFails = 0;
    // The named monsters woken onto the player (FoundTarget), seen or not: the scene's foe may start out of his sight.
    const progs::ServerVm vm;
    edict_t* p = progs::playerEdict();
    const func_t found = progs::findFunction("FoundTarget");
    if(!p || !found || !Cmd_Argv(5)[0])
    {
        return;
    }
    const int oldSelf = pr_global_struct->self;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(e->v.health > 0.f && e->v.classname && !strcmp(PR_GetString(e->v.classname), reachTest.only.cStr()))
        {
            e->v.enemy = EDICT_TO_PROG(p);
            pr_global_struct->self = EDICT_TO_PROG(e);
            pr_global_struct->time = sv.qcvm.time;
            PR_ExecuteProgram(found);
        }
    });
    pr_global_struct->self = oldSelf;
}

// (Each steer: the armed reach test's check.)
void reachCheck(const edict_t* ent)
{
    if(!reachTest.armed)
    {
        return;
    }
    const edict_t* p = progs::playerEdict();
    const bool timedOut = sv.qcvm.time > reachTest.until;
    const bool linkDone = reachTest.need == 0 || (reachTest.need == 1 ? state.drops > 0 : state.rides > 0);
    const char* only = reachTest.only.cStr();
    const bool counts = !only[0] || (ent->v.classname && !strcmp(only, PR_GetString(ent->v.classname)));
    const bool near = counts && p && glm::length(geom::vec(p->v.origin) - geom::vec(ent->v.origin)) < reachTest.distance;
    if(!(near && linkDone) && !timedOut)
    {
        return;
    }
    reachTest.armed = false;
    Con_Printf("%s: %s after %.1f s; %d drops and %d rides taken, %d links given up (it at %.0f %.0f %.0f, the player at %.0f %.0f "
               "%.0f)\n",
        reachTest.name.cStr(), near && linkDone ? "reached" : "timed out", sv.qcvm.time - reachTest.since, state.drops, state.rides,
        state.linkFails, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], p ? p->v.origin[0] : 0.f, p ? p->v.origin[1] : 0.f,
        p ? p->v.origin[2] : 0.f);
    testkit::verdict(reachTest.name.cStr(), near && linkDone ? 1 : 0, near && linkDone ? 0 : 1);
}

void rebuild_f()
{
    afterLoad();
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

void crowdtest_f(); // (below, by the steering)

void init()
{
    Cmd_AddCommand("vr_nav_info", info_f);
    Cmd_AddCommand("vr_nav_cliptest", cliptest_f);
    Cmd_AddCommand("vr_nav_toptest", toptest_f);
    Cmd_AddCommand("vr_nav_links", links_f);
    Cmd_AddCommand("vr_nav_reachtest", reachtest_f);
    Cmd_AddCommand("vr_nav_crowdtest", crowdtest_f);
    Cmd_AddCommand("vr_nav_rebuild", rebuild_f);
}

void afterLoad()
{
    resetAll();
    if(!sv.active || !sv.worldmodel)
    {
        return;
    }
    const progs::ServerVm vm; // (vr_nav_rebuild runs from the console)
    const auto t0 = za::Clock::nowNanoseconds();
    Geometry& g = state.geo;
    g = Geometry{};
    BrushSet set;
    gatherSolids(set, state.movers, g);
    gatherRides(state.movers, g);
    triangulate(set, g);
    g.planes = static_cast<za::Vector<glm::vec4>&&>(set.planes);
    g.solids = static_cast<za::Vector<hull::NavBrush>&&>(set.brushes);
    g.ms = msSince(t0);
    state.ready = !g.walkable.empty();
    state.moversAt = 0.0;
    state.classes.reserve(maxClasses);
    if(!state.ready)
    {
        return;
    }
    // The classes of the walkers the map spawned, their builds on the pool together.
    progs::forEachEdict(1, [](edict_t* e) {
        if(walker(e))
        {
            (void)classFor(e, true);
        }
    });
    Con_DPrintf("nav: %s: %d triangles from %d brushes in %.1f ms, %d size classes\n", sv.name,
        static_cast<int>(g.walkable.size()), g.brushes, g.ms, static_cast<int>(state.classes.size()));
}

float yawTo(const glm::vec3& from, const glm::vec3& to)
{
    const float yaw = geom::yawOf(to - from);
    return yaw < 0.f ? yaw + 360.f : yaw;
}

float flatDist(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(glm::vec2{a.x - b.x, a.y - b.y});
}

glm::vec3 moverMiddle(const edict_t* m)
{
    return (geom::vec(m->v.absmin) + geom::vec(m->v.absmax)) * 0.5f;
}

// The mover stopped at that origin (a lift or train at one of its stops).
bool moverAt(const edict_t* m, const glm::vec3& org)
{
    return glm::length(geom::vec(m->v.origin) - org) < 2.f && glm::length(geom::vec(m->v.velocity)) < 1.f;
}

const edict_t* rideMover(int ride)
{
    if(ride < 0 || static_cast<za::SizeT>(ride) >= state.geo.rides.size())
    {
        return nullptr;
    }
    const edict_t* m = progs::edictAt(state.geo.rides[static_cast<za::SizeT>(ride)].mover);
    return m && !m->free && static_cast<int>(m->v.solid) == SOLID_BSP ? m : nullptr;
}

// vr_nav_crowdtest <seconds> [near]: every walker on the map woken onto the player; every half second each one's move
// sampled: a stall is a half second it moved under 8 units while farther than `near` (128) from him. Its end prints the
// walkers, their stalls and how many came within `near` of him; it passes when every one did. (Stuck counts in a crowd:
// vr_nav_separation, vr_nav_slots.)
struct CrowdTest
{
    bool armed = false;
    double since = 0.0, until = 0.0, nextSample = 0.0;
    float near = 128.f;
    za::Vector<glm::vec4> last; // by edict number: the last sample's place (w: 1 sampled)
    za::Vector<unsigned char> came;
    int stalls = 0, samples = 0;
};
CrowdTest crowdTest;

void crowdtest_f()
{
    if(Cmd_Argc() < 2 || !sv.active)
    {
        Con_Printf("vr_nav_crowdtest <seconds> [near]\n");
        return;
    }
    const progs::ServerVm vm;
    crowdTest = CrowdTest{};
    crowdTest.armed = true;
    crowdTest.since = sv.qcvm.time;
    crowdTest.until = sv.qcvm.time + atof(Cmd_Argv(1));
    crowdTest.nextSample = sv.qcvm.time + 0.5;
    crowdTest.near = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 128.f;
    crowdTest.last.resize(static_cast<za::SizeT>(sv.qcvm.max_edicts), glm::vec4{0.f});
    crowdTest.came.resize(static_cast<za::SizeT>(sv.qcvm.max_edicts), 0);
    edict_t* p = progs::playerEdict();
    const func_t found = progs::findFunction("FoundTarget");
    if(!p || !found)
    {
        return;
    }
    const int oldSelf = pr_global_struct->self;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(walker(e) && e->v.health > 0.f)
        {
            e->v.enemy = EDICT_TO_PROG(p);
            pr_global_struct->self = EDICT_TO_PROG(e);
            pr_global_struct->time = sv.qcvm.time;
            PR_ExecuteProgram(found);
        }
    });
    pr_global_struct->self = oldSelf;
}

// (Each steer: the armed crowd test's samples and end.)
void crowdCheck()
{
    if(!crowdTest.armed || sv.qcvm.time < crowdTest.nextSample)
    {
        return;
    }
    crowdTest.nextSample = sv.qcvm.time + 0.5;
    const edict_t* p = progs::playerEdict();
    const glm::vec3 him = p ? geom::vec(p->v.origin) : glm::vec3{0.f};
    int walkers = 0, came = 0;
    progs::forEachEdict(1, [&](edict_t* e) {
        const auto n = static_cast<za::SizeT>(NUM_FOR_EDICT(e));
        if(!walker(e) || e->v.health <= 0.f || n >= crowdTest.last.size())
        {
            return;
        }
        ++walkers;
        const glm::vec3 at = geom::vec(e->v.origin);
        const bool near = glm::length(at - him) < crowdTest.near;
        crowdTest.came[n] = crowdTest.came[n] || near ? 1 : 0;
        came += crowdTest.came[n];
        glm::vec4& l = crowdTest.last[n];
        if(l.w > 0.f && !near)
        {
            ++crowdTest.samples;
            crowdTest.stalls += glm::length(glm::vec3{l} - at) < 8.f ? 1 : 0;
        }
        l = glm::vec4{at, 1.f};
    });
    if(sv.qcvm.time < crowdTest.until && came < walkers)
    {
        return;
    }
    crowdTest.armed = false;
    Con_Printf("nav crowd: %d walkers, %d of them came within %.0f of him in %.1f s; %d stalls in %d samples away from him\n",
        walkers, came, static_cast<double>(crowdTest.near), sv.qcvm.time - crowdTest.since, crowdTest.stalls, crowdTest.samples);
    testkit::verdict("nav crowd", came == walkers && walkers > 0 ? 1 : 0, came == walkers && walkers > 0 ? 0 : 1);
}

// (While a reach test runs: the links' events, logged.)
void note(const char* what, const glm::vec3& pos)
{
    if(reachTest.armed)
    {
        Con_Printf("nav: %.2f s: %s at %.0f %.0f %.0f\n", sv.qcvm.time - reachTest.since, what, pos.x, pos.y, pos.z);
    }
}

// A link done (the monster at its far end): its path found again from there.
float endLink(Agent& a)
{
    a.link = 0;
    a.waitLink = 0;
    a.airborne = false;
    a.path.clear();
    return -1.f;
}

// A link given up: the monster plans without links for vr_nav_link_retry.
float giveUp(Agent& a, double now)
{
    ++state.linkFails;
    a.noLinksUntil = now + static_cast<double>(za::max(vr_nav_link_retry.value, 0.f));
    return endLink(a);
}

// A link under way: a drop (walk off the ledge, fall, land) or a ride (onto the mover's middle while it is at this
// stop, stand till it stops at the other, step off toward the far end). The yaw; -1: done or given up (Quake's chase
// this frame, a new path the next).
float followLink(edict_t* ent, Agent& a, const NavClass& c, const glm::vec3& pos, double now)
{
    const bool onGround = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    if(a.linkArea == areaDrop)
    {
        if(!onGround)
        {
            if(!a.airborne)
            {
                note("over the ledge", pos);
            }
            a.airborne = true;
            a.action = 2;
            return yawTo(pos, a.linkTo);
        }
        if(a.airborne) // (landed)
        {
            note("landed", pos);
            ent->v.velocity[0] = ent->v.velocity[1] = 0.f;
            ++state.drops;
            return endLink(a);
        }
        if(now - a.linkSince > static_cast<double>(vr_nav_drop_time.value) || flatDist(pos, a.linkFrom) > c.radius * 4.f + 32.f)
        {
            note("drop given up", pos);
            return giveUp(a, now);
        }
        a.action = 1;
        return yawTo(pos, a.linkTo);
    }
    const edict_t* m = rideMover(a.ride);
    const double wait = static_cast<double>(za::max(vr_nav_lift_wait.value, 0.1f));
    if(!m || now - a.linkSince > wait * 3.0)
    {
        return giveUp(a, now);
    }
    const glm::vec3 mid = moverMiddle(m);
    const bool onMover = onGround && PROG_TO_EDICT(ent->v.groundentity) == m;
    if(a.phase == 0)
    {
        if(!onMover && !moverAt(m, a.board))
        {
            a.link = 0; // (it left before the monster was on: back to waiting for it)
            a.action = 2;
            return yawTo(pos, mid);
        }
        const float half = za::min(m->v.size[0], m->v.size[1]) * 0.5f;
        if(flatDist(pos, mid) > za::max(4.f, half - c.radius - 4.f))
        {
            return yawTo(pos, mid);
        }
        a.phase = 1;
        a.phaseSince = now;
        note("on the lift", pos);
    }
    if(a.phase == 1)
    {
        if(moverAt(m, a.leave))
        {
            a.phase = 2;
            a.phaseSince = now;
            note("stepping off", pos);
            return yawTo(pos, a.linkTo);
        }
        if((!onMover && onGround && !moverAt(m, a.board)) || (moverAt(m, a.board) && now - a.phaseSince > wait))
        {
            note("ride given up", pos);
            return giveUp(a, now); // (left behind, or the lift never set off)
        }
        a.action = 2;
        return yawTo(pos, a.linkTo);
    }
    if(flatDist(pos, a.linkTo) <= za::max(vr_nav_corner_reached.value, 8.f) * 2.f || now - a.phaseSince > 3.0)
    {
        note("ride done", pos);
        ++state.rides;
        return endLink(a);
    }
    return yawTo(pos, a.linkTo);
}

// The monster at a link's start (path corner i, the off-mesh link's; i + 1 its far end): a drop begins; a ride begins
// when its mover is at this stop, else the monster waits beside it (vr_nav_lift_wait at most).
float beginLink(edict_t* ent, Agent& a, const NavClass& c, const glm::vec3& pos, double now, dtPolyRef ref, int i)
{
    const dtMeshTile* tile = nullptr;
    const dtPoly* poly = nullptr;
    const dtOffMeshConnection* con = c.mesh->getOffMeshConnectionByRef(ref);
    if(!con || dtStatusFailed(c.mesh->getTileAndPolyByRef(ref, &tile, &poly)))
    {
        return giveUp(a, now);
    }
    a.linkArea = poly->getArea();
    a.linkFrom = fromRc(&a.corners[i * 3]);
    a.linkTo = fromRc(&a.corners[(i + 1) * 3]);
    a.airborne = false;
    a.phase = 0;
    if(a.linkArea == areaDrop)
    {
        a.link = ref;
        a.linkSince = now;
        note("drop begins", pos);
        return followLink(ent, a, c, pos, now);
    }
    a.ride = static_cast<int>(con->userId) - 1;
    const edict_t* m = rideMover(a.ride);
    if(!m)
    {
        return giveUp(a, now);
    }
    // Which stop is this one: the one whose standing place (its top's middle) is nearer.
    const Ride& ride = state.geo.rides[static_cast<za::SizeT>(a.ride)];
    const glm::vec3 lift{(ride.mins.x + ride.maxs.x) * 0.5f, (ride.mins.y + ride.maxs.y) * 0.5f, ride.maxs.z};
    const bool atA = glm::length(ride.a + lift - a.linkFrom) <= glm::length(ride.b + lift - a.linkFrom);
    a.board = atA ? ride.a : ride.b;
    a.leave = atA ? ride.b : ride.a;
    if(moverAt(m, a.board))
    {
        a.link = ref;
        a.linkSince = now;
        a.waitLink = 0;
        note("boarding", pos);
        return followLink(ent, a, c, pos, now);
    }
    if(a.waitLink != ref)
    {
        a.waitLink = ref;
        a.waitSince = now;
        note("waiting for the lift", pos);
    }
    if(now - a.waitSince > static_cast<double>(za::max(vr_nav_lift_wait.value, 0.1f)))
    {
        note("wait given up", pos);
        return giveUp(a, now);
    }
    a.action = 2;
    return yawTo(pos, moverMiddle(m));
}

// The live walkers (their feet, half widths, numbers), gathered once a frame.
const za::Vector<Crowd>& crowdNow()
{
    if(state.crowdAt != sv.qcvm.time)
    {
        state.crowdAt = sv.qcvm.time;
        state.crowd.clear();
        progs::forEachEdict(1, [](edict_t* e) {
            if(walker(e) && e->v.health > 0.f)
            {
                state.crowd.pushBack(Crowd{feet(e), e->v.size[0] * 0.5f, NUM_FOR_EDICT(e)});
            }
        });
    }
    return state.crowd;
}

// Where a walker heads for its foe: a melee one (no missile attack) farther than vr_nav_slot_engage takes a slot on a
// ring round him (vr_nav_slots, vr_nav_slot_radius): its own (none other chasing him holds it), the free one nearest
// its bearing from him, kept while it stays free; others his feet.
glm::vec3 approach(edict_t* ent, Agent& a, int num, int goalNum, const glm::vec3& pos, const glm::vec3& goalPos, double now)
{
    const int k = za::min(static_cast<int>(vr_nav_slots.value), 32);
    const int missile = progs::fields().th_missile;
    if(k <= 0 || missile < 0 || progs::fieldInt(ent, missile) != 0 || flatDist(pos, goalPos) <= vr_nav_slot_engage.value)
    {
        a.slot = -1;
        return goalPos;
    }
    unsigned taken = 0;
    for(za::SizeT i = 0; i < state.agents.size(); ++i)
    {
        const Agent& o = state.agents[i];
        if(static_cast<int>(i) != num && o.slot >= 0 && o.slot < k && o.slotGoal == goalNum && now - o.slotAt < 0.5)
        {
            taken |= 1u << static_cast<unsigned>(o.slot);
        }
    }
    const float step = glm::two_pi<float>() / static_cast<float>(k);
    int pick = a.slotGoal == goalNum && a.slot >= 0 && a.slot < k && !(taken & (1u << static_cast<unsigned>(a.slot))) ? a.slot : -1;
    if(pick < 0)
    {
        const float bearing = glm::atan(pos.y - goalPos.y, pos.x - goalPos.x);
        const int pref = ((static_cast<int>(za::floor(bearing / step + 0.5f)) % k) + k) % k;
        pick = pref;
        for(int d = 0; d <= k / 2; ++d)
        {
            const int up = (pref + d) % k, down = (pref - d + k) % k;
            if(!(taken & (1u << static_cast<unsigned>(up))))
            {
                pick = up;
                break;
            }
            if(!(taken & (1u << static_cast<unsigned>(down))))
            {
                pick = down;
                break;
            }
        }
    }
    a.slot = pick;
    a.slotGoal = goalNum;
    a.slotAt = now;
    const float angle = static_cast<float>(pick) * step;
    return goalPos + glm::vec3{glm::cos(angle), glm::sin(angle), 0.f} * vr_nav_slot_radius.value;
}

// The yaw toward the path's next corner, its turn rounded (vr_nav_smooth: near the corner, already toward the one
// after) and kept apart from the walkers close by (vr_nav_separation: never turned back on its path).
float steerToward(const NavClass& c, const Agent& a, const glm::vec3& pos, int next, int nc, const unsigned char* flags, int num)
{
    glm::vec3 target = fromRc(&a.corners[next * 3]);
    const bool link = (flags[next] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
    const float smooth = vr_nav_smooth.value;
    if(smooth > 0.f && next + 1 < nc && !link)
    {
        const float d = flatDist(pos, target);
        const glm::vec3 after = fromRc(&a.corners[(next + 1) * 3]);
        const glm::vec2 dir{after.x - target.x, after.y - target.y};
        const float len = glm::length(dir);
        if(d < smooth && len > 1.f)
        {
            const glm::vec2 on = dir / len * za::min(smooth - d, len * 0.5f);
            target += glm::vec3{on.x, on.y, 0.f};
        }
    }
    glm::vec2 want{target.x - pos.x, target.y - pos.y};
    const float wl = glm::length(want);
    if(wl < 0.01f)
    {
        return yawTo(pos, target);
    }
    want /= wl;
    const float sep = vr_nav_separation.value;
    if(sep > 0.f && !link)
    {
        glm::vec2 push{0.f};
        for(const Crowd& w : crowdNow())
        {
            const glm::vec2 off{pos.x - w.pos.x, pos.y - w.pos.y};
            const float dist = glm::length(off);
            const float range = c.radius + w.radius + vr_nav_separation_range.value;
            if(w.num != num && za::abs(pos.z - w.pos.z) < 48.f && dist > 0.01f && dist < range)
            {
                push += off / dist * (1.f - dist / range);
            }
        }
        const glm::vec2 sum = want + push * sep;
        const float sl = glm::length(sum);
        if(sl > 0.01f && glm::dot(sum / sl, want) > 0.f)
        {
            want = sum / sl;
        }
    }
    return yawTo(pos, pos + glm::vec3{want.x, want.y, 0.f});
}

float steer(edict_t* ent, edict_t* goal)
{
    if(vr_nav.value == 0.f || !sv.active || !state.ready || !ent || !goal || !walker(ent))
    {
        return -1.f;
    }
    ++state.steers;
    NavClass* c = readyClass(classFor(ent, true));
    if(!c)
    {
        ++state.fallbacks;
        return -1.f;
    }
    refreshMovers();
    setCosts();
    const int num = NUM_FOR_EDICT(ent);
    if(state.agents.size() <= static_cast<za::SizeT>(num))
    {
        state.agents.resize(static_cast<za::SizeT>(num) + 1);
    }
    Agent& a = state.agents[static_cast<za::SizeT>(num)];
    const int cls = static_cast<int>(c - state.classes.data());
    const glm::vec3 pos = feet(ent), goalPos = feet(goal);
    const double now = sv.qcvm.time;
    a.action = 0;
    reachCheck(ent);
    crowdCheck();
    if(a.link)
    {
        if(a.cls == cls)
        {
            return followLink(ent, a, *c, pos, now);
        }
        a.link = 0;
    }
    const dtQueryFilter& filt = now < a.noLinksUntil ? filterNoLinks : filter;
    const int goalNum = NUM_FOR_EDICT(goal);
    const int slotBefore = a.slot;
    const glm::vec3 aim = approach(ent, a, num, goalNum, pos, goalPos, now);
    const glm::vec3 startRc = toRc(pos), goalRc = toRc(aim);
    const float startExt[3] = {c->radius + 16.f, 32.f, c->radius + 16.f};
    dtPolyRef startRef = 0;
    float startPt[3];
    if(dtStatusFailed(c->query->findNearestPoly(&startRc.x, startExt, &filt, &startRef, startPt)) || !startRef)
    {
        a.path.clear();
        ++state.fallbacks;
        return -1.f;
    }
    bool repath = a.cls != cls || a.goal != goalNum || a.path.empty() || now >= a.repathAt || a.slot != slotBefore ||
                  glm::length(goalPos - a.goalAt) > vr_nav_repath_move.value;
    int from = 0;
    if(!repath)
    {
        from = -1;
        for(za::SizeT i = 0; i < a.path.size(); ++i)
        {
            if(a.path[i] == startRef)
            {
                from = static_cast<int>(i);
                break;
            }
        }
        repath = from < 0;
        from = za::max(from, 0);
    }
    if(repath)
    {
        const float goalExt[3] = {64.f, 96.f, 64.f};
        dtPolyRef endRef = 0;
        float endPt[3];
        if(dtStatusFailed(c->query->findNearestPoly(&goalRc.x, goalExt, &filt, &endRef, endPt)) || !endRef)
        {
            a.path.clear();
            ++state.fallbacks;
            return -1.f;
        }
        dtPolyRef buf[maxPath];
        int n = 0;
        (void)c->query->findPath(startRef, endRef, startPt, endPt, &filt, buf, &n, maxPath);
        a.path.clear();
        a.path.emplaceBackRange(buf, static_cast<za::SizeT>(n));
        if(n > 0 && buf[n - 1] != endRef)
        {
            (void)c->query->closestPointOnPoly(buf[n - 1], endPt, endPt, nullptr); // (partial: as near as it gets)
        }
        memcpy(a.endRc, endPt, sizeof(a.endRc));
        a.cls = cls;
        a.goal = goalNum;
        a.goalAt = goalPos;
        // Staggered: each monster's interval a little longer or shorter by its number.
        const float jitter = 0.75f + 0.5f * static_cast<float>((num * 37) % 64) / 64.f;
        a.repathAt = now + static_cast<double>(za::max(vr_nav_repath.value, 0.05f) * jitter);
        from = 0;
        ++state.paths;
    }
    if(a.path.empty())
    {
        ++state.fallbacks;
        return -1.f;
    }
    unsigned char flags[maxCorners];
    dtPolyRef refs[maxCorners];
    int nc = 0;
    (void)c->query->findStraightPath(startPt, a.endRc, a.path.data() + from, static_cast<int>(a.path.size()) - from,
        a.corners, flags, refs, &nc, maxCorners);
    a.cornerCount = nc;
    a.drawUntil = now + 1.0;
    // The first corner not yet reached (the first is where it stands).
    const float reached = za::max(vr_nav_corner_reached.value, 1.f);
    int next = -1;
    for(int i = 0; i < nc; ++i)
    {
        const glm::vec3 q = fromRc(&a.corners[i * 3]);
        if(glm::length(glm::vec2{q.x - pos.x, q.y - pos.y}) > reached)
        {
            next = i;
            break;
        }
    }
    a.next = next;
    // At a link's start (a corner reached that begins one): the drop or ride begins (or the wait for the lift).
    for(int i = 0; i < (next < 0 ? nc : next); ++i)
    {
        if((flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) && i + 1 < nc)
        {
            a.cls = cls;
            return beginLink(ent, a, *c, pos, now, refs[i], i);
        }
    }
    a.waitLink = 0;
    // None left, or the goal straight ahead on the mesh: Quake's own chase (its melee range, its dodges). A slot straight
    // ahead is still walked to.
    if(next < 0 || ((flags[next] & DT_STRAIGHTPATH_END) && a.slot < 0))
    {
        ++state.fallbacks;
        return -1.f;
    }
    return steerToward(*c, a, pos, next, nc, flags, num);
}

int action(const edict_t* ent)
{
    const int num = ent ? NUM_FOR_EDICT(const_cast<edict_t*>(ent)) : 0;
    return num > 0 && static_cast<za::SizeT>(num) < state.agents.size() ? state.agents[static_cast<za::SizeT>(num)].action : 0;
}

void debugDraw(const glm::vec3& view)
{
    if(vr_nav_debug.value == 0.f || !sv.active || !state.ready || state.classes.empty())
    {
        return;
    }
    const progs::ServerVm vm; // (the view's draws run with the client's VM current)
    // Which class: the setting's, else the nearest walker's.
    int cls = static_cast<int>(vr_nav_debug_class.value);
    if(cls < 0)
    {
        float best = 1e30f;
        cls = 0;
        progs::forEachEdict(1, [&](edict_t* e) {
            if(walker(e))
            {
                const float d = glm::length(geom::vec(e->v.origin) - view);
                const int k = classFor(e, false);
                if(k >= 0 && d < best)
                {
                    best = d;
                    cls = k;
                }
            }
        });
    }
    NavClass* c = readyClass(cls);
    if(!c)
    {
        return;
    }
    const float range = vr_nav_debug_range.value;
    const glm::vec3 lift{0.f, 0.f, 2.f};
    for(int t = 0; t < c->mesh->getMaxTiles(); ++t)
    {
        const dtMeshTile* tile = static_cast<const dtNavMesh*>(c->mesh)->getTile(t);
        if(!tile || !tile->header)
        {
            continue;
        }
        const glm::vec3 tlo = fromRc(tile->header->bmin), thi = fromRc(tile->header->bmax);
        const glm::vec3 near = glm::clamp(view, glm::min(tlo, thi), glm::max(tlo, thi));
        if(glm::length(near - view) > range)
        {
            continue;
        }
        const dtPolyRef base = c->mesh->getPolyRefBase(tile);
        for(int p = 0; p < tile->header->polyCount; ++p)
        {
            const dtPoly& poly = tile->polys[p];
            if(poly.getType() != DT_POLYTYPE_GROUND || poly.vertCount < 3)
            {
                continue;
            }
            const glm::vec3 v0 = fromRc(&tile->verts[poly.verts[0] * 3]) + lift;
            if(glm::length(v0 - view) > range)
            {
                continue;
            }
            unsigned short f = 0;
            (void)c->mesh->getPolyFlags(base | static_cast<dtPolyRef>(p), &f);
            const unsigned char area = poly.getArea();
            glm::vec4 col = (f & flagBlocked)        ? glm::vec4{0.9f, 0.1f, 0.1f, 0.45f}
                            : area == areaWater ? glm::vec4{0.1f, 0.3f, 0.9f, 0.4f}
                            : area == areaSlime ? glm::vec4{0.3f, 0.9f, 0.1f, 0.4f}
                            : area == areaLava  ? glm::vec4{1.f, 0.5f, 0.05f, 0.45f}
                                                : glm::vec4{0.1f, 0.75f, 0.7f, 0.3f};
            const glm::vec4 edge{col.r * 0.5f, col.g * 0.5f, col.b * 0.5f, 0.9f};
            glm::vec3 prev = fromRc(&tile->verts[poly.verts[poly.vertCount - 1] * 3]) + lift;
            for(int k = 0; k < poly.vertCount; ++k)
            {
                const glm::vec3 v = fromRc(&tile->verts[poly.verts[k] * 3]) + lift;
                if(k >= 2)
                {
                    lines::triangle(v0, fromRc(&tile->verts[poly.verts[k - 1] * 3]) + lift, v, col);
                }
                lines::sceneLine(prev, v, 0.8f, edge, edge);
                prev = v;
            }
        }
        // Its links: a drop orange (out over the ledge, then down), a lift's or train's ride magenta.
        for(int k = 0; k < tile->header->offMeshConCount; ++k)
        {
            const dtOffMeshConnection& con = tile->offMeshCons[k];
            const glm::vec3 a = fromRc(&con.pos[0]) + lift, b = fromRc(&con.pos[3]) + lift;
            if(glm::length(a - view) > range)
            {
                continue;
            }
            if(tile->polys[con.poly].getArea() == areaDrop)
            {
                const glm::vec4 orange{1.f, 0.55f, 0.1f, 1.f};
                const glm::vec3 over{b.x, b.y, a.z};
                lines::sceneLine(a, over, 2.f, orange, orange);
                lines::sceneLine(over, b, 2.f, orange, orange);
            }
            else
            {
                const glm::vec4 magenta{0.9f, 0.2f, 0.9f, 1.f};
                lines::sceneLine(a, b, 3.f, magenta, magenta);
            }
        }
    }
    if(vr_nav_debug.value < 2.f)
    {
        return;
    }
    const glm::vec4 yellow{1.f, 0.9f, 0.1f, 1.f}, white{1.f, 1.f, 1.f, 1.f};
    for(const Agent& a : state.agents)
    {
        if(a.drawUntil < sv.qcvm.time || a.cornerCount < 2)
        {
            continue;
        }
        const glm::vec3 up{0.f, 0.f, 6.f};
        for(int i = 0; i + 1 < a.cornerCount; ++i)
        {
            lines::sceneLine(fromRc(&a.corners[i * 3]) + up, fromRc(&a.corners[(i + 1) * 3]) + up, 2.f, yellow, yellow);
        }
        if(a.next >= 0)
        {
            const glm::vec3 n = fromRc(&a.corners[a.next * 3]) + up;
            lines::sceneLine(n, n + glm::vec3{0.f, 0.f, 24.f}, 3.f, white, white);
        }
    }
}

} // namespace qvr::nav
