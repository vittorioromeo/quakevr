// vr_nav.cpp -- navmesh pathfinding for walking monsters (vr_nav.hpp; docs/vr-port/NAVMESH.md).

#include "vr_nav.hpp"

#include "vr_box3d.hpp"
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
// 2: drop and lift links. 4: the narrow band (vr_nav_narrow; 3 its first try, drop links from the band).
constexpr unsigned navVersion = 4;
constexpr char navMagic[5] = "QVNM";

// Detour's areas and flags. areaNarrow: the narrow band's dry floor (closer to walls and ledges than the clearance).
constexpr unsigned char areaGround = 1, areaWater = 2, areaSlime = 3, areaLava = 4, areaDrop = 5, areaLift = 6, areaNarrow = 7;
constexpr unsigned char rcNarrowArea = 62; // (Recast's area of the band's spans, till the polygons get theirs)
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

// The build's settings (settingsNow).
struct Settings
{
    float cs, ch, climb, slope, clearance;
    int tile;
    float drops, dropMax, dropMaxBig, lifts; // (the links': vr_nav_drop, its heights, vr_nav_lifts)
    float narrow; // (vr_nav_narrow: the narrow band's clearance, times the half width; 0 none)
};

// A prop's footprint cut out of the navmesh (props as obstacles): a convex outline (Quake's x, y) and its height's span.
constexpr int maxObstacleVerts = 16;
struct Obstacle
{
    float xy[maxObstacleVerts * 2];
    int count = 0;
    float zlo = 0.f, zhi = 0.f;
    glm::vec2 lo{0.f}, hi{0.f}; // its outline's bounds
};

// One tile rebuilt with the props' obstacles on it (a batch's, on the pool): its tile, the obstacles reaching it and its
// links (as the mesh has them), and the job's data (null: nothing walkable left).
struct ObstacleTile
{
    int x = 0, y = 0;
    za::Vector<Obstacle> obstacles;
    za::Vector<Link> links;
    Tile out{0, 0, nullptr, 0};
    double ms = 0.0;
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
    // Props as obstacles (runtime only: the cached file is the mesh without them). The settings it was made with (its
    // tiles are rebuilt with the same), per tile: changed since its last batch, its data before any obstacle (empty:
    // unchanged), its triangles and brushes (the batches' bins, made the first time a tile is rebuilt). One batch at a
    // time on the pool (obsJob: obsTiles), no sooner than obsAt.
    Settings settings{};
    int tilesX = 0, tilesY = 0;
    bool obsInit = false; // the marks there were when its mesh came made its first batch
    za::Vector<unsigned char> dirty;
    za::Vector<za::Vector<unsigned char>> original;
    za::Vector<za::Vector<Link>> originalLinks;
    za::Vector<za::Vector<int>> obsBins, obsBrushBins;
    za::Vector<unsigned char> binned;
    jobs::Future<void> obsJob;
    bool obsPending = false;
    za::Vector<ObstacleTile> obsTiles;
    double obsAt = 0.0, obsJobMs = 0.0;
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
    // Its last steer (the crowd test's stall causes): why it returned (Why), where it stood, when, which way.
    int why = 0;
    bool partial = false;     // its path stops short of the goal (no way there on the mesh)
    glm::vec3 lastPos{0.f};
    double lastAt = -1.0;
    float lastYaw = 0.f;
};

// Why a steer returned what it did (the crowd test's causes).
enum Why : int
{
    whyNone,
    whyOffMesh,   // its feet on no polygon: Quake's chase
    whyNoPath,    // no polygon at the goal, or no path: Quake's chase
    whyEnd,       // no corner left, or the goal straight ahead on the mesh: Quake's chase
    whyLink,      // following a drop or a ride
    whySteered,   // a yaw toward the next corner
    whyCount
};

// A loose solid prop as the obstacles see it (by edict number): its pose at the last look, since when it rests, its mark
// (where it was marked: the mark goes when it moves on from there).
struct PropTrack
{
    bool live = false, marked = false, small = false; // (small: looked at at rest, too small or light to block)
    glm::vec3 origin{0.f};
    glm::mat3 turn{1.f};
    double stillSince = -1.0;
    glm::vec3 markOrigin{0.f};
    glm::mat3 markTurn{1.f};
    Obstacle mark;
};

// The obstacles' counts (vr_nav_info, builtin navobstacles).
struct ObstacleStats
{
    int batches = 0, tiles = 0, restored = 0, marks = 0, unmarks = 0;
    double jobMs = 0.0, worstJobMs = 0.0, applyMs = 0.0, worstApplyMs = 0.0;
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
    za::Vector<PropTrack> props; // by edict number (refreshProps)
    ObstacleStats obs;
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

Settings settingsNow()
{
    return Settings{za::max(vr_nav_cell.value, 0.5f), za::max(vr_nav_cell_height.value, 0.25f), za::max(vr_nav_climb.value, 0.f),
        vr_nav_slope.value, za::max(vr_nav_clearance.value, 0.f), za::max(static_cast<int>(vr_nav_tile.value), 8),
        vr_nav_drop.value != 0.f ? 1.f : 0.f, za::max(vr_nav_drop_max.value, 0.f), za::max(vr_nav_drop_max_big.value, 0.f),
        vr_nav_lifts.value != 0.f ? 1.f : 0.f, za::max(vr_nav_narrow.value, 0.f)};
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
// The narrow band (`narrowRadius` voxels, under the clearance's cfg.walkableRadius; 0 none): eroded by the clearance, a
// place that only a gap a little wider than the box leads to (a stair's top two treads beside a tower's wall and over
// a drop: the arena's towers) was cut off. The spans the smaller erosion keeps and the clearance's took become the band's
// own area (their own regions and polygons, at vr_nav_narrow_cost), so a path keeps to the clearance where there is
// room and goes through the band only where nothing else leads (the box still collides with the real walls).
bool erodeWithBand(rcContext& ctx, int radius, int narrowRadius, rcCompactHeightfield& chf, za::Vector<unsigned char>& keep)
{
    if(narrowRadius <= 0 || narrowRadius >= radius)
    {
        return rcErodeWalkableArea(&ctx, radius, chf);
    }
    const auto n = static_cast<za::SizeT>(chf.spanCount);
    keep.clear();
    keep.emplaceBackRange(chf.areas, n);
    if(!rcErodeWalkableArea(&ctx, radius, chf))
    {
        return false;
    }
    for(za::SizeT i = 0; i < n; ++i)
    {
        const unsigned char eroded = chf.areas[i]; // (keep: the clearance's; chf: the original again)
        chf.areas[i] = keep[i];
        keep[i] = eroded;
    }
    if(!rcErodeWalkableArea(&ctx, narrowRadius, chf))
    {
        return false;
    }
    for(za::SizeT i = 0; i < n; ++i)
    {
        if(chf.areas[i] != RC_NULL_AREA && keep[i] == RC_NULL_AREA)
        {
            chf.areas[i] = rcNarrowArea;
        }
    }
    return true;
}

// The props' obstacles cut out of the tile's floor before its erosion (so the clearance keeps the box off them as off a
// wall): the spans under each outline whose floor is under the prop's bottom by less than the class's height (its head
// would meet it) and under its top by more than a step (it can't step over it).
void markObstacles(rcContext& ctx, const za::Vector<Obstacle>& obstacles, const rcConfig& cfg, rcCompactHeightfield& chf)
{
    const float height = static_cast<float>(cfg.walkableHeight) * cfg.ch, climb = static_cast<float>(cfg.walkableClimb) * cfg.ch;
    for(const Obstacle& o : obstacles)
    {
        float verts[maxObstacleVerts * 3];
        for(int k = 0; k < o.count; ++k)
        {
            verts[k * 3] = o.xy[k * 2];
            verts[k * 3 + 1] = 0.f;
            verts[k * 3 + 2] = -o.xy[k * 2 + 1];
        }
        const float hmin = o.zlo - height + cfg.ch, hmax = o.zhi - climb;
        if(hmax > hmin && o.count >= 3)
        {
            rcMarkConvexPolyArea(&ctx, verts, o.count, hmin, hmax, RC_NULL_AREA, chf);
        }
    }
}

bool buildTile(const Geometry& g, const za::Vector<int>& bin, const za::Vector<int>& brushBin, const rcConfig& base, int narrowRadius,
    int tx, int ty, TileSrc& out, const za::Vector<Obstacle>* obstacles = nullptr)
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
    za::Vector<unsigned char> band;
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
        if(!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf, *chf))
        {
            break;
        }
        if(obstacles)
        {
            markObstacles(ctx, *obstacles, cfg, *chf);
        }
        if(!erodeWithBand(ctx, cfg.walkableRadius, narrowRadius, *chf, band) || !rcBuildDistanceField(&ctx, *chf) ||
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
            const unsigned char rcArea = pmesh->areas[i];
            const unsigned char under = rcArea == RC_WALKABLE_AREA || rcArea == rcNarrowArea ? areaAt(fromRc(m)) : 0;
            pmesh->areas[i] = rcArea == rcNarrowArea && under == areaGround ? areaNarrow : under;
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
            // (Not from the narrow band: a link starts at the clearance's edge, where the box stands clear of the ledge, as
            // without the band; that edge's neighbour is a band polygon now.)
            if(poly.getType() != DT_POLYTYPE_GROUND || poly.getArea() == areaLava || poly.getArea() == areaSlime ||
                poly.getArea() == areaNarrow)
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
                const unsigned short nei = poly.neis[k];
                const bool toBand = nei != 0 && !(nei & DT_EXT_LINK) && tile->polys[nei - 1].getArea() == areaNarrow;
                if(nei != 0 && !toBand)
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

// The class's Recast settings (each tile's bounds are set by buildTile) and its narrow band's radius (voxels).
rcConfig classConfig(const Settings& s, const NavClass& c, const Grid& grid, int& narrowRadius)
{
    rcConfig cfg{};
    cfg.cs = s.cs;
    cfg.ch = s.ch;
    cfg.walkableSlopeAngle = s.slope;
    cfg.walkableHeight = static_cast<int>(za::ceil(c.height / s.ch));
    cfg.walkableClimb = static_cast<int>(za::floor(s.climb / s.ch));
    // Kept off walls and ledges by the box's reach at any angle (a square's corner: radius times up to 1.41), so a
    // path's corner never puts a box corner over a ledge (SV_CheckBottom) or into a slanted wall.
    cfg.walkableRadius = static_cast<int>(za::ceil(c.radius * s.clearance / s.cs));
    // ... and the narrow band's, closer (erodeWithBand).
    narrowRadius = s.narrow > 0.f ? static_cast<int>(za::ceil(c.radius * s.narrow / s.cs)) : 0;
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
    return cfg;
}

// Each triangle and brush into the tiles its bounds (with the border) reach; `want` (per tile; null: all) the tiles
// binned (a props' batch: the few it rebuilds).
void binTiles(const Geometry& g, const Grid& grid, const rcConfig& cfg, const za::Vector<unsigned char>* want,
    za::Vector<za::Vector<int>>& bins, za::Vector<za::Vector<int>>& brushBins)
{
    const int tilesX = grid.tilesX, tilesY = grid.tilesY;
    const float tw = static_cast<float>(cfg.tileSize) * cfg.cs;
    const float border = static_cast<float>(cfg.borderSize) * cfg.cs;
    bins.resize(static_cast<za::SizeT>(tilesX * tilesY));
    brushBins.resize(bins.size());
    const auto wanted = [want](za::SizeT i) { return !want || (*want)[i] != 0; };
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
                const auto i = static_cast<za::SizeT>(y * tilesX + x);
                if(wanted(i))
                {
                    bins[i].pushBack(t);
                }
            }
        }
    }
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
                const auto i = static_cast<za::SizeT>(y * tilesX + x);
                if(wanted(i))
                {
                    brushBins[i].pushBack(static_cast<int>(b));
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
    int narrowRadius = 0;
    const rcConfig cfg = classConfig(s, c, grid, narrowRadius);
    const int tilesX = grid.tilesX, tilesY = grid.tilesY;
    const float tw = static_cast<float>(cfg.tileSize) * s.cs;
    za::Vector<za::Vector<int>> bins, brushBins;
    binTiles(g, grid, cfg, nullptr, bins, brushBins);
    za::Vector<TileSrc> srcs;
    za::Vector<unsigned char> has;
    srcs.resize(bins.size(), TileSrc{0, 0, nullptr, nullptr});
    has.resize(bins.size(), 0);
    jobs::parallelFor(tileSite, bins.size(), 1, [&](za::SizeT begin, za::SizeT end) {
        for(za::SizeT i = begin; i < end; ++i)
        {
            const int x = static_cast<int>(i) % tilesX, y = static_cast<int>(i) / tilesX;
            has[i] = !bins[i].empty() && buildTile(g, bins[i], brushBins[i], cfg, narrowRadius, x, y, srcs[i]) ? 1 : 0;
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
        s.dropMaxBig, s.lifts, s.narrow};
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
    if(c.obsPending)
    {
        c.obsJob.wait();
        c.obsPending = false;
    }
    for(ObstacleTile& t : c.obsTiles)
    {
        dtFree(t.out.data);
        t.out.data = nullptr;
    }
    c.obsTiles.clear();
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
    state.props.clear();
    state.obs = ObstacleStats{};
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
    c.settings = s;
    const Grid grid = gridFor(state.geo, s);
    c.tilesX = grid.tilesX;
    c.tilesY = grid.tilesY;
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

void refreshProps(double now); // (below: props as obstacles)

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
    refreshProps(now); // (and the props come to rest or moved on: props as obstacles)
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

// ---------------------------------------------------------------------------------------------------------------------
// Props as obstacles (vr_nav_props; NAVMESH.md 14): a loose solid prop (a crate, a barrel, an explosive box: Box3D's,
// not in a hand) that has rested vr_nav_prop_rest, wide and heavy enough, is cut out of every class's mesh. The tiles it
// reaches are built again from their own input with the obstacles on them, in batches on the pool (one per class at a
// time, no more often than vr_nav_prop_batch); its mark goes when it moves on from where it was marked, is taken up or
// goes, and a tile with none left gets its data back as it was. The cached file is the mesh without them.

// The convex outline of `pts` (Quake's x, y; sorted here) counter-clockwise (Andrew's monotone chain), at most
// maxObstacleVerts corners (a rounder one thinned: a little inside its outline).
int outlineOf(za::Vector<glm::vec2>& pts, float* out)
{
    const za::SizeT n = pts.size();
    for(za::SizeT i = 1; i < n; ++i) // (a prop's few points: insertion sort)
    {
        const glm::vec2 p = pts[i];
        za::SizeT j = i;
        for(; j > 0 && (pts[j - 1].x > p.x || (pts[j - 1].x == p.x && pts[j - 1].y > p.y)); --j)
        {
            pts[j] = pts[j - 1];
        }
        pts[j] = p;
    }
    za::Vector<glm::vec2> h;
    h.resize(n * 2 + 1);
    const auto cross = [](const glm::vec2& o, const glm::vec2& a, const glm::vec2& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    za::SizeT k = 0;
    for(za::SizeT i = 0; i < n; ++i)
    {
        for(; k >= 2 && cross(h[k - 2], h[k - 1], pts[i]) <= 0.f; --k) {}
        h[k++] = pts[i];
    }
    for(za::SizeT i = n - 1, lower = k + 1; i-- > 0;)
    {
        for(; k >= lower && cross(h[k - 2], h[k - 1], pts[i]) <= 0.f; --k) {}
        h[k++] = pts[i];
    }
    const int count = static_cast<int>(k > 0 ? k - 1 : 0);
    const int keep = za::min(count, maxObstacleVerts);
    for(int i = 0; i < keep; ++i)
    {
        const glm::vec2& p = h[static_cast<za::SizeT>(i * count / keep)];
        out[i * 2] = p.x;
        out[i * 2 + 1] = p.y;
    }
    return keep;
}

// A prop's obstacle from its shape where it lies (its pieces' points: box3d::ClimbShape); false if it should not block:
// no higher than a step (stepped over), narrower than vr_nav_prop_size across, lighter than vr_nav_prop_mass.
bool obstacleOf(const box3d::ClimbShape& shape, const box3d::ClimbBody& body, Obstacle& o)
{
    if(shape.points.size() < 3 || body.mass < vr_nav_prop_mass.value)
    {
        return false;
    }
    za::Vector<glm::vec2> pts;
    pts.reserve(shape.points.size());
    o.zlo = 1e30f;
    o.zhi = -1e30f;
    for(const glm::vec3& p : shape.points)
    {
        const glm::vec3 w = body.turn * p + body.origin;
        pts.pushBack({w.x, w.y});
        o.zlo = za::min(o.zlo, w.z);
        o.zhi = za::max(o.zhi, w.z);
    }
    o.count = outlineOf(pts, o.xy);
    if(o.count < 3 || o.zhi - o.zlo <= vr_nav_climb.value)
    {
        return false;
    }
    // Its width across its narrower side: the least, over its edges, of how far its outline reaches from the edge.
    float narrow = 1e30f;
    o.lo = o.hi = {o.xy[0], o.xy[1]};
    for(int i = 0; i < o.count; ++i)
    {
        const glm::vec2 a{o.xy[i * 2], o.xy[i * 2 + 1]};
        const int j = (i + 1) % o.count;
        const glm::vec2 e = glm::vec2{o.xy[j * 2], o.xy[j * 2 + 1]} - a;
        const float len = glm::length(e);
        o.lo = glm::min(o.lo, a);
        o.hi = glm::max(o.hi, a);
        if(len < 1e-3f)
        {
            continue;
        }
        const glm::vec2 inward{-e.y / len, e.x / len};
        float reach = 0.f;
        for(int k = 0; k < o.count; ++k)
        {
            reach = za::max(reach, glm::dot(glm::vec2{o.xy[k * 2], o.xy[k * 2 + 1]} - a, inward));
        }
        narrow = za::min(narrow, reach);
    }
    return narrow >= vr_nav_prop_size.value;
}

// The class's tiles an obstacle's outline reaches (with the tiles' border: the clearance round it reaches into them).
template <typename F>
void obstacleTiles(const NavClass& c, const Obstacle& o, F&& fn)
{
    const Settings& s = c.settings;
    const float tw = c.params.tileWidth;
    const float border = (za::ceil(c.radius * s.clearance / s.cs) + 3.f) * s.cs;
    // Recast's x is Quake's x, its z Quake's -y.
    const int x0 = za::max(0, static_cast<int>(za::floor((o.lo.x - border - c.params.orig[0]) / tw)));
    const int x1 = za::min(c.tilesX - 1, static_cast<int>(za::floor((o.hi.x + border - c.params.orig[0]) / tw)));
    const int y0 = za::max(0, static_cast<int>(za::floor((-o.hi.y - border - c.params.orig[2]) / tw)));
    const int y1 = za::min(c.tilesY - 1, static_cast<int>(za::floor((-o.lo.y + border - c.params.orig[2]) / tw)));
    for(int y = y0; y <= y1; ++y)
    {
        for(int x = x0; x <= x1; ++x)
        {
            fn(y * c.tilesX + x);
        }
    }
}

void dirtyFor(const Obstacle& o)
{
    for(NavClass& c : state.classes)
    {
        if(c.obsInit)
        {
            obstacleTiles(c, o, [&c](int ti) { c.dirty[static_cast<za::SizeT>(ti)] = 1; });
        }
    }
}

void unmark(PropTrack& t)
{
    if(t.marked)
    {
        t.marked = false;
        dirtyFor(t.mark);
        ++state.obs.unmarks;
    }
}

// The props looked at (the movers' tick): each loose solid prop's rest, its mark made or taken off.
void refreshProps(double now)
{
    const bool on = vr_nav_props.value != 0.f;
    const int count = sv.qcvm.num_edicts;
    if(state.props.size() < static_cast<za::SizeT>(count))
    {
        state.props.resize(static_cast<za::SizeT>(count));
    }
    const float moveOn = za::max(vr_nav_prop_move.value, 0.f);
    constexpr float cosTurned = 0.9848f; // (10 degrees)
    for(za::SizeT n = 1; n < state.props.size(); ++n)
    {
        PropTrack& t = state.props[n];
        const int num = static_cast<int>(n);
        box3d::ClimbBody body;
        const bool loose = on && progs::edictAt(num) && box3d::isBox3DProp(num) && box3d::climbBody(num, body);
        if(!loose)
        {
            unmark(t);
            t = PropTrack{};
            continue;
        }
        const bool moving = (!body.asleep && (glm::length(body.velocity) > 4.f || glm::length(body.spin) > 0.3f)) ||
                            (t.live && glm::length(body.origin - t.origin) > 1.f);
        t.live = true;
        t.origin = body.origin;
        t.turn = body.turn;
        if(moving)
        {
            t.stillSince = -1.0;
            t.small = false;
        }
        else if(t.stillSince < 0.0)
        {
            t.stillSince = now;
        }
        if(t.marked)
        {
            const bool turned = glm::dot(t.markTurn[0], body.turn[0]) < cosTurned || glm::dot(t.markTurn[2], body.turn[2]) < cosTurned;
            if(glm::length(body.origin - t.markOrigin) <= moveOn && !turned)
            {
                continue;
            }
            unmark(t);
        }
        if(t.small || t.stillSince < 0.0 || now - t.stillSince < static_cast<double>(vr_nav_prop_rest.value))
        {
            continue;
        }
        box3d::ClimbBody at;
        const box3d::ClimbShape* shape = box3d::climbShape(num, at);
        Obstacle o;
        if(!shape || !obstacleOf(*shape, at, o))
        {
            t.small = true;
            continue;
        }
        t.marked = true;
        t.mark = o;
        t.markOrigin = body.origin;
        t.markTurn = body.turn;
        dirtyFor(o);
        ++state.obs.marks;
    }
}

// After tiles of class `ci` changed: walkers whose path crossed one find theirs again, the movers' flags are set again.
void afterTilesChanged(const NavClass& c, int ci)
{
    for(Agent& a : state.agents)
    {
        if(a.cls != ci)
        {
            continue;
        }
        for(const dtPolyRef r : a.path)
        {
            if(!c.mesh->isValidPolyRef(r))
            {
                a.path.clear();
                a.repathAt = 0.0;
                break;
            }
        }
    }
    state.moversAt = 0.0;
}

// The class's tile `ti` back as the mesh had it before any obstacle.
void restoreTile(NavClass& c, za::SizeT ti)
{
    za::Vector<unsigned char>& orig = c.original[ti];
    const int x = static_cast<int>(ti) % c.tilesX, y = static_cast<int>(ti) / c.tilesX;
    if(const dtTileRef old = c.mesh->getTileRefAt(x, y, 0))
    {
        (void)c.mesh->removeTile(old, nullptr, nullptr);
    }
    auto* data = static_cast<unsigned char*>(dtAlloc(static_cast<za::SizeT>(orig.size()), DT_ALLOC_PERM));
    if(data)
    {
        memcpy(data, orig.data(), orig.size());
        if(dtStatusFailed(c.mesh->addTile(data, static_cast<int>(orig.size()), DT_TILE_FREE_DATA, 0, nullptr)))
        {
            dtFree(data);
        }
    }
    orig = za::Vector<unsigned char>{};
    ++state.obs.restored;
}

// A tile's links as the mesh has them (a tile rebuilt keeps its drops and rides).
void linksOf(const dtMeshTile& tile, za::Vector<Link>& out)
{
    for(int k = 0; k < tile.header->offMeshConCount; ++k)
    {
        const dtOffMeshConnection& con = tile.offMeshCons[k];
        const dtPoly& poly = tile.polys[con.poly];
        Link l{};
        memcpy(l.pos, con.pos, sizeof(l.pos));
        l.rad = con.rad;
        l.dir = (con.flags & DT_OFFMESH_CON_BIDIR) ? DT_OFFMESH_CON_BIDIR : 0;
        l.area = poly.getArea();
        l.flags = poly.flags;
        l.id = con.userId;
        out.pushBack(l);
    }
}

// A batch's tiles built (the pool's job): their bins made the first time, then each with its obstacles and links.
void runObstacleBatch(NavClass& c)
{
    const auto t0 = za::Clock::nowNanoseconds();
    const Geometry& g = state.geo;
    const Settings& s = c.settings;
    const Grid grid = gridFor(g, s);
    int narrowRadius = 0;
    const rcConfig cfg = classConfig(s, c, grid, narrowRadius);
    za::Vector<unsigned char> want;
    want.resize(c.binned.size(), 0);
    bool need = false;
    for(const ObstacleTile& t : c.obsTiles)
    {
        const auto ti = static_cast<za::SizeT>(t.y * c.tilesX + t.x);
        if(!c.binned[ti])
        {
            want[ti] = 1;
            need = true;
        }
    }
    if(need)
    {
        za::Vector<za::Vector<int>> bins, brushBins;
        binTiles(g, grid, cfg, &want, bins, brushBins);
        for(za::SizeT ti = 0; ti < want.size(); ++ti)
        {
            if(want[ti])
            {
                c.obsBins[ti] = ZA_MOVE(bins[ti]);
                c.obsBrushBins[ti] = ZA_MOVE(brushBins[ti]);
                c.binned[ti] = 1;
            }
        }
    }
    jobs::parallelFor(tileSite, c.obsTiles.size(), 1, [&](za::SizeT begin, za::SizeT end) {
        for(za::SizeT i = begin; i < end; ++i)
        {
            ObstacleTile& t = c.obsTiles[i];
            const auto t1 = za::Clock::nowNanoseconds();
            const auto ti = static_cast<za::SizeT>(t.y * c.tilesX + t.x);
            TileSrc src{t.x, t.y, nullptr, nullptr};
            if(!c.obsBins[ti].empty() && buildTile(g, c.obsBins[ti], c.obsBrushBins[ti], cfg, narrowRadius, t.x, t.y, src, &t.obstacles))
            {
                (void)tileData(src, c, s, t.links, t.out);
                rcFreePolyMesh(src.pmesh);
                rcFreePolyMeshDetail(src.dmesh);
            }
            t.ms = msSince(t1);
        }
    });
    c.obsJobMs = msSince(t0);
}

// The batch done: its tiles into the mesh (main thread).
void applyObstacleBatch(NavClass& c, int ci)
{
    const auto t0 = za::Clock::nowNanoseconds();
    for(ObstacleTile& t : c.obsTiles)
    {
        if(const dtTileRef old = c.mesh->getTileRefAt(t.x, t.y, 0))
        {
            (void)c.mesh->removeTile(old, nullptr, nullptr);
        }
        if(t.out.data && dtStatusFailed(c.mesh->addTile(t.out.data, t.out.size, DT_TILE_FREE_DATA, 0, nullptr)))
        {
            dtFree(t.out.data);
        }
        t.out.data = nullptr;
    }
    afterTilesChanged(c, ci);
    const double apply = msSince(t0);
    ObstacleStats& st = state.obs;
    ++st.batches;
    st.tiles += static_cast<int>(c.obsTiles.size());
    st.jobMs += c.obsJobMs;
    st.worstJobMs = za::max(st.worstJobMs, c.obsJobMs);
    st.applyMs += apply;
    st.worstApplyMs = za::max(st.worstApplyMs, apply);
    Con_DPrintf("nav: props: class r%.0f h%.0f: %d tiles rebuilt in %.1f ms on the pool, put in in %.2f ms\n", c.radius, c.height,
        static_cast<int>(c.obsTiles.size()), c.obsJobMs, apply);
    c.obsTiles.clear();
}

// The class's changed tiles: those with no obstacle left get their data back, the others go to the pool together.
void startObstacleBatch(NavClass& c, int ci, double now)
{
    c.obsTiles.clear();
    bool restored = false;
    for(za::SizeT ti = 0; ti < c.dirty.size(); ++ti)
    {
        if(!c.dirty[ti])
        {
            continue;
        }
        c.dirty[ti] = 0;
        const int x = static_cast<int>(ti) % c.tilesX, y = static_cast<int>(ti) / c.tilesX;
        ObstacleTile t;
        t.x = x;
        t.y = y;
        for(const PropTrack& p : state.props)
        {
            if(p.marked)
            {
                obstacleTiles(c, p.mark, [&](int k) {
                    if(static_cast<za::SizeT>(k) == ti)
                    {
                        t.obstacles.pushBack(p.mark);
                    }
                });
            }
        }
        if(t.obstacles.empty())
        {
            if(!c.original[ti].empty())
            {
                restoreTile(c, ti);
                restored = true;
            }
            continue;
        }
        // The tile as it was (and its links) kept the first time an obstacle reaches it.
        if(c.original[ti].empty())
        {
            const dtMeshTile* tile = static_cast<const dtNavMesh*>(c.mesh)->getTileAt(x, y, 0);
            if(!tile || !tile->header)
            {
                continue; // (nothing walkable there)
            }
            c.original[ti].emplaceBackRange(tile->data, static_cast<za::SizeT>(tile->dataSize));
            c.originalLinks[ti].clear();
            linksOf(*tile, c.originalLinks[ti]);
        }
        t.links.emplaceBackRange(c.originalLinks[ti].data(), c.originalLinks[ti].size());
        c.obsTiles.pushBack(ZA_MOVE(t));
    }
    if(restored)
    {
        afterTilesChanged(c, ci);
    }
    if(c.obsTiles.empty())
    {
        return;
    }
    c.obsAt = now + static_cast<double>(za::max(vr_nav_prop_batch.value, 0.f));
    c.obsPending = true;
    const auto i = static_cast<za::SizeT>(ci);
    c.obsJob = jobs::async([i]() { runObstacleBatch(state.classes[i]); });
}

// Each class's batch joined when done, the next started when due (main thread; each steer, the debug view, navobstacles).
void pollObstacles(double now)
{
    for(za::SizeT i = 0; i < state.classes.size(); ++i)
    {
        NavClass& c = state.classes[i];
        if(c.pending || !c.mesh || c.failed)
        {
            continue;
        }
        const auto ci = static_cast<int>(i);
        if(!c.obsInit)
        {
            const auto tiles = static_cast<za::SizeT>(c.tilesX * c.tilesY);
            c.dirty.resize(tiles, 0);
            c.original.resize(tiles);
            c.originalLinks.resize(tiles);
            c.obsBins.resize(tiles);
            c.obsBrushBins.resize(tiles);
            c.binned.resize(tiles, 0);
            c.obsInit = true;
            for(const PropTrack& p : state.props)
            {
                if(p.marked)
                {
                    obstacleTiles(c, p.mark, [&c](int ti) { c.dirty[static_cast<za::SizeT>(ti)] = 1; });
                }
            }
        }
        if(c.obsPending)
        {
            if(!c.obsJob.ready())
            {
                continue;
            }
            c.obsJob.get();
            c.obsPending = false;
            applyObstacleBatch(c, ci);
        }
        if(now >= c.obsAt)
        {
            startObstacleBatch(c, ci, now);
        }
    }
}

// What's left to do: props coming to rest that may block, tiles waiting for their batch, batches on the pool.
int obstaclesPending()
{
    int n = 0;
    for(const PropTrack& p : state.props)
    {
        n += p.live && !p.marked && !p.small ? 1 : 0;
    }
    for(const NavClass& c : state.classes)
    {
        n += c.obsPending ? 1 : 0;
        for(const unsigned char d : c.dirty)
        {
            n += d;
        }
    }
    return n;
}

// The movers' and the props' tick, and the props' batches (main thread).
void tickWorld()
{
    refreshMovers();
    pollObstacles(sv.qcvm.time);
}

void setCosts()
{
    filter.setIncludeFlags(flagWalk);
    filter.setExcludeFlags(flagBlocked);
    filter.setAreaCost(areaGround, 1.f);
    filter.setAreaCost(areaNarrow, za::max(vr_nav_narrow_cost.value, 1.f));
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
        int narrow = 0; // (the narrow band's polygons: vr_nav_narrow)
        if(c.mesh && !c.pending)
        {
            const dtNavMesh& m = *c.mesh;
            for(int t = 0; t < m.getMaxTiles(); ++t)
            {
                const dtMeshTile* tile = m.getTile(t);
                for(int p = 0; tile && tile->header && p < tile->header->polyCount; ++p)
                {
                    narrow += tile->polys[p].getArea() == areaNarrow ? 1 : 0;
                }
            }
        }
        Con_Printf("nav: class %d: radius %.0f height %.0f: %s, %d tiles, %d polygons (%d narrow), %d drop and %d lift links, %.1f KB, %s "
                   "in %.1f ms (joined in %.1f ms), %d blocked\n",
            static_cast<int>(i), c.radius, c.height, c.pending ? "building" : c.failed ? "FAILED" : "ready", c.tiles, c.polys, narrow,
            c.drops, c.lifts, static_cast<double>(c.bytes) / 1024.0, c.fromDisk ? "read" : "built", c.ms, c.joinMs,
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
    int marked = 0;
    for(const PropTrack& p : state.props)
    {
        marked += p.marked ? 1 : 0;
    }
    const ObstacleStats& st = state.obs;
    Con_Printf("nav: props: %d blocking now (%d marked, %d unmarked); %d batches, %d tiles rebuilt, %d restored; pool %.1f ms "
               "(worst batch %.1f), main thread %.2f ms (worst %.2f); %d pending\n",
        marked, st.marks, st.unmarks, st.batches, st.tiles, st.restored, st.jobMs, st.worstJobMs, st.applyMs, st.worstApplyMs,
        obstaclesPending());
}

// Builtin navobstacles (vr_nav.hpp).
float obstacleInfo(int what)
{
    if(!sv.active || !state.ready)
    {
        return 0.f;
    }
    tickWorld();
    if(what == 0)
    {
        return static_cast<float>(obstaclesPending());
    }
    if(what == 1)
    {
        int marked = 0;
        for(const PropTrack& p : state.props)
        {
            marked += p.marked ? 1 : 0;
        }
        return static_cast<float>(marked);
    }
    const ObstacleStats& st = state.obs;
    return what == 2   ? static_cast<float>(st.batches)
           : what == 3 ? static_cast<float>(st.tiles + st.restored)
           : what == 4 ? static_cast<float>(st.jobMs)
           : what == 5 ? static_cast<float>(st.worstJobMs)
           : what == 6 ? static_cast<float>(st.worstApplyMs)
                       : 0.f;
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

// vr_nav_crowdtest <seconds> [near] [dump]: every walker on the map woken onto the player; every half second each one's
// move sampled: a stall is a half second it moved under 8 units while farther than `near` (128) from him. A sample whose
// monster is busy (an attack, pain, a leap, knocked down, in the air) is left out. Each stall is given its cause (Cause,
// from the monster's state and its steers that half second) and logged (`navstall|<class>|<cause>|x|y|z`); the end
// prints a table of causes by monster type, the frames the "no chase" and "busy" ones were in, whether each walker had a
// whole path to him at the start, and the walkers, their stalls and how many came within `near` of him; it passes when
// every one did. `dump`: the walkers' classes' polygons logged too (`navpoly|...`, for a top-down heatmap).
enum Cause : int
{
    causeInSolid,    // its box in a wall (placed there): it cannot move at all
    causeIdle,       // waiting by design: standing, walking, the knight's pause, an armed one's taunt (vr_ag_hunt)
    causeInfight,    // after another monster (infighting: one struck another in the crowd)
    causeNoChase,    // after him, but no steer that half second (ai_run did something else: turned, gated, searched)
    causeOffMesh,    // its feet on no polygon: Quake's chase
    causeNoPath,     // no polygon at his feet, or no path: Quake's chase
    causeLink,       // on a drop or a ride
    causeEndBlocked, // the path handed to Quake's chase (him straight ahead, or none left), blocked
    causeUnreach,    // as above, its path stopping short of him (no way to him on the mesh)
    causeEndOther,   // as above, otherwise
    causeMonster,    // steered, its step blocked by another monster's box
    causeWall,       // steered, its step blocked by a wall or a brush entity
    causeLedge,      // steered, the way clear but walkmove refused (the ledge test)
    causeSlow,       // steered and moved, but under 8 units (back and forth)
    causeCount
};
constexpr const char* causeNames[causeCount] = {"insolid", "idle", "infight", "nochase", "offmesh", "nopath", "link", "end-blocked", "unreach",
    "end-other", "monster", "wall", "ledge", "slow"};

struct CrowdWin
{
    int steers = 0, steered = 0, blocked = 0;
    int whys[whyCount]{};
};

struct CrowdType
{
    const char* name;
    int samples = 0, busy = 0, stalls = 0;
    int busyAir = 0, busyKnock = 0; // (of busy: in the air, knocked down)
    int causes[causeCount]{};
};

struct CrowdFrame
{
    const char* name;
    int count;
};

struct CrowdTest
{
    bool armed = false, dump = false;
    double since = 0.0, until = 0.0, nextSample = 0.0;
    float near = 128.f;
    za::Vector<glm::vec4> last; // by edict number: the last sample's place (w: 1 sampled)
    za::Vector<unsigned char> came, reachDone;
    za::Vector<CrowdWin> win;   // by edict number: its steers since the last sample
    za::Vector<CrowdType> types;
    za::Vector<CrowdFrame> noChaseFrames, busyFrames, idleFrames;
    za::Vector<unsigned char> member; // by edict number: a walker alive at the start (those that came later left out)
    za::Vector<glm::vec4> stallAt;    // each stall's place (w: its Cause), drawn at vr_nav_debug 2
    int stalls = 0, samples = 0, busy = 0;
    int reachWhole = 0, reachShort = 0;
    float reachWorst = 0.f;
};
CrowdTest crowdTest;
bool crowdDump = false; // vr_nav_crowdtest dump: the next test's end logs the polygons
int crowdWaits = 0;      // frames waited for a scene's monsters to start

const char* functionName(func_t f)
{
    return f > 0 && f < qcvm->progs->numfunctions ? PR_GetString(qcvm->functions[f].s_name) : "(none)";
}

void countFrame(za::Vector<CrowdFrame>& frames, const char* name)
{
    for(CrowdFrame& f : frames)
    {
        if(!strcmp(f.name, name))
        {
            ++f.count;
            return;
        }
    }
    frames.pushBack(CrowdFrame{name, 1});
}

CrowdType& crowdType(const char* name)
{
    for(CrowdType& t : crowdTest.types)
    {
        if(!strcmp(t.name, name))
        {
            return t;
        }
    }
    crowdTest.types.pushBack(CrowdType{name});
    return crowdTest.types.back();
}

// (Each steer while the crowd test runs: the last steer's outcome counted, now it has moved or not.)
void crowdNote(const edict_t* ent, Agent& a, const glm::vec3& pos, double now)
{
    const auto n = static_cast<za::SizeT>(NUM_FOR_EDICT(const_cast<edict_t*>(ent)));
    if(crowdTest.armed && n < crowdTest.win.size())
    {
        CrowdWin& w = crowdTest.win[n];
        ++w.steers;
        if(a.lastAt >= 0.0 && now - a.lastAt < 0.3)
        {
            w.whys[a.why] += 1;
            const bool moved = flatDist(pos, a.lastPos) > 0.5f;
            w.steered += a.why == whySteered ? 1 : 0;
            w.blocked += a.why == whySteered && !moved ? 1 : 0;
        }
    }
    a.lastPos = pos;
    a.lastAt = now;
}

bool busyFrame(const char* f)
{
    constexpr const char* busy[] = {"atk", "att", "pain", "leap", "die", "death", "dead", "knock", "jump", "shove", "slam",
        "smash", "stun", "melee", "swing", "slice", "magic", "getup", "fall", "char", "take", "agp_act"};
    for(const char* b : busy)
    {
        if(strstr(f, b))
        {
            return true;
        }
    }
    return false;
}

// Its box in a wall or a brush entity (a spot in a wall: it never moves).
bool inSolid(edict_t* e)
{
    const trace_t tr = SV_Move(e->v.origin, e->v.mins, e->v.maxs, e->v.origin, MOVE_NOMONSTERS, e);
    return tr.startsolid || tr.allsolid;
}

// In the air: off the ground with no floor right under its box (the armed humanoids walk without FL_ONGROUND).
bool airborne(edict_t* e)
{
    if((static_cast<int>(e->v.flags) & FL_ONGROUND) || inSolid(e))
    {
        return false;
    }
    vec3_t down;
    VectorCopy(e->v.origin, down);
    down[2] -= 4.f;
    return SV_Move(e->v.origin, e->v.mins, e->v.maxs, down, MOVE_NOMONSTERS, e).fraction >= 1.f;
}

// What a stalled walker's step meets a little ahead along that yaw: 0 nothing, 1 a monster, 2 anything else.
int stepMeets(edict_t* e, float yaw)
{
    const float r = glm::radians(yaw);
    const glm::vec3 at = geom::vec(e->v.origin);
    const glm::vec3 to = at + glm::vec3{glm::cos(r), glm::sin(r), 0.f} * 16.f;
    vec3_t s, t;
    VectorCopy(&at.x, s);
    VectorCopy(&to.x, t);
    const trace_t tr = SV_Move(s, e->v.mins, e->v.maxs, t, MOVE_NORMAL, e);
    if(tr.fraction >= 1.f && !tr.allsolid && !tr.startsolid)
    {
        return 0;
    }
    return tr.ent && tr.ent != sv.qcvm.edicts && (static_cast<int>(tr.ent->v.flags) & FL_MONSTER) ? 1 : 2;
}

Cause stallCause(edict_t* e, const Agent* a, const CrowdWin& w, const edict_t* p)
{
    if(inSolid(e))
    {
        return causeInSolid;
    }
    const char* frame = functionName(e->v.think);
    if(p && e->v.enemy && e->v.enemy != EDICT_TO_PROG(p))
    {
        return causeInfight;
    }
    if(!p || !e->v.enemy || strstr(frame, "stand") || strstr(frame, "walk") || strstr(frame, "pause") || strstr(frame, "hunt"))
    {
        countFrame(crowdTest.idleFrames, frame);
        return causeIdle;
    }
    if(w.steers == 0 || !a)
    {
        countFrame(crowdTest.noChaseFrames, frame);
        return causeNoChase;
    }
    if(w.whys[whyLink] > 0)
    {
        return causeLink;
    }
    if(w.blocked > 0)
    {
        const int meets = stepMeets(e, a->lastYaw);
        return meets == 1 ? causeMonster : meets == 2 ? causeWall : causeLedge;
    }
    if(w.steered > 0 && w.steered * 2 >= w.steers)
    {
        return causeSlow;
    }
    if(w.whys[whyOffMesh] > 0 && a->why == whyOffMesh)
    {
        return causeOffMesh;
    }
    if(w.whys[whyNoPath] > 0 && a->why == whyNoPath)
    {
        return causeNoPath;
    }
    if(a->partial)
    {
        return causeUnreach;
    }
    return stepMeets(e, e->v.angles[1]) != 0 ? causeEndBlocked : causeEndOther;
}

// Whether a walker has a whole path to him on its class's mesh (else how far short its path stops).
bool wholePath(const edict_t* e, const glm::vec3& him, float& gap)
{
    gap = 0.f;
    NavClass* c = readyClass(classFor(e, false));
    if(!c)
    {
        return false;
    }
    const glm::vec3 a = toRc(feet(e)), b = toRc(him);
    const float ext[3] = {c->radius + 16.f, 32.f, c->radius + 16.f}, goalExt[3] = {64.f, 96.f, 64.f};
    dtPolyRef s = 0, g = 0;
    float sp[3], gp[3];
    if(dtStatusFailed(c->query->findNearestPoly(&a.x, ext, &filter, &s, sp)) || !s ||
       dtStatusFailed(c->query->findNearestPoly(&b.x, goalExt, &filter, &g, gp)) || !g)
    {
        gap = -1.f;
        return false;
    }
    dtPolyRef buf[maxPath];
    int n = 0;
    (void)c->query->findPath(s, g, sp, gp, &filter, buf, &n, maxPath);
    if(n > 0 && buf[n - 1] == g)
    {
        return true;
    }
    float end[3];
    (void)c->query->closestPointOnPoly(n > 0 ? buf[n - 1] : s, gp, end, nullptr);
    gap = glm::length(fromRc(end) - fromRc(gp));
    return false;
}

void dumpPolys()
{
    za::Vector<int> seen;
    progs::forEachEdict(1, [&](edict_t* e) {
        const int cls = walker(e) ? classFor(e, false) : -1;
        NavClass* c = readyClass(cls);
        for(const int k : seen)
        {
            c = k == cls ? nullptr : c;
        }
        if(!c)
        {
            return;
        }
        seen.pushBack(cls);
        for(int i = 0; i < c->mesh->getMaxTiles(); ++i)
        {
            const dtMeshTile* tile = static_cast<const dtNavMesh*>(c->mesh)->getTile(i);
            for(int k = 0; tile && tile->header && k < tile->header->polyCount; ++k)
            {
                const dtPoly& poly = tile->polys[k];
                if(poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                {
                    continue;
                }
                char line[512];
                int len = q_snprintf(line, sizeof(line), "navpoly|%d|%.0f", cls, static_cast<double>(c->radius));
                for(int v = 0; v < poly.vertCount && len < 440; ++v)
                {
                    const glm::vec3 q = fromRc(&tile->verts[poly.verts[v] * 3]);
                    len += q_snprintf(line + len, sizeof(line) - static_cast<size_t>(len), "|%.0f %.0f %.0f", static_cast<double>(q.x),
                        static_cast<double>(q.y), static_cast<double>(q.z));
                }
                Con_Printf("%s\n", line);
            }
        }
    });
}

void crowdtest_f()
{
    if(Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "dump"))
    {
        crowdDump = true; // (before a scene that runs the test)
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_nav_crowdtest <seconds> [near] [dump]; vr_nav_crowdtest dump: the next one dumps\n");
        return;
    }
    // A scene's map loads and its monsters start a few frames after they are placed (walkmonster_start_go): the test
    // waits a frame at a time till the server runs, none is still to start and there are walkers (at most 10 s of
    // frames: then it goes ahead with what there is).
    int walkers = 0, starting = 0;
    if(sv.active)
    {
        const progs::ServerVm vm;
        progs::forEachEdict(1, [&](edict_t* e) {
            walkers += walker(e) && e->v.health > 0.f ? 1 : 0;
            starting += strstr(functionName(e->v.think), "monster_start_go") ? 1 : 0;
        });
    }
    if((!sv.active || walkers == 0 || starting > 0) && crowdWaits < 720)
    {
        ++crowdWaits;
        Cbuf_InsertText(va("wait\nvr_nav_crowdtest %s %s %s\n", Cmd_Argv(1), Cmd_Argc() > 2 ? Cmd_Argv(2) : "128",
            Cmd_Argc() > 3 ? Cmd_Argv(3) : "0"));
        return;
    }
    crowdWaits = 0;
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    crowdTest = CrowdTest{};
    crowdTest.armed = true;
    crowdTest.since = sv.qcvm.time;
    crowdTest.until = sv.qcvm.time + atof(Cmd_Argv(1));
    crowdTest.nextSample = sv.qcvm.time + 0.5;
    crowdTest.near = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 128.f;
    crowdTest.dump = crowdDump || (Cmd_Argc() > 3 && atoi(Cmd_Argv(3)) != 0);
    const auto edicts = static_cast<za::SizeT>(sv.qcvm.max_edicts);
    crowdTest.last.resize(edicts, glm::vec4{0.f});
    crowdTest.came.resize(edicts, 0);
    crowdTest.reachDone.resize(edicts, 0);
    crowdTest.win.resize(edicts, CrowdWin{});
    crowdTest.member.resize(edicts, 0);
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
            crowdTest.member[static_cast<za::SizeT>(NUM_FOR_EDICT(e))] = 1;
            e->v.enemy = EDICT_TO_PROG(p);
            pr_global_struct->self = EDICT_TO_PROG(e);
            pr_global_struct->time = sv.qcvm.time;
            PR_ExecuteProgram(found);
        }
    });
    pr_global_struct->self = oldSelf;
}

void crowdReport()
{
    Con_Printf("nav crowd causes: type samples busy stalls |");
    for(const char* c : causeNames)
    {
        Con_Printf(" %s", c);
    }
    Con_Printf("\n");
    for(const CrowdType& t : crowdTest.types)
    {
        Con_Printf("nav crowd causes: %s %d %d(air %d, down %d) %d |", t.name, t.samples, t.busy, t.busyAir, t.busyKnock, t.stalls);
        for(const int c : t.causes)
        {
            Con_Printf(" %d", c);
        }
        Con_Printf("\n");
    }
    for(const CrowdFrame& f : crowdTest.noChaseFrames)
    {
        Con_Printf("nav crowd nochase frame: %s %d\n", f.name, f.count);
    }
    for(const CrowdFrame& f : crowdTest.busyFrames)
    {
        Con_Printf("nav crowd busy frame: %s %d\n", f.name, f.count);
    }
    for(const CrowdFrame& f : crowdTest.idleFrames)
    {
        Con_Printf("nav crowd idle frame: %s %d\n", f.name, f.count);
    }
    Con_Printf("nav crowd paths at the start: %d whole, %d short (worst %.0f units short)\n", crowdTest.reachWhole,
        crowdTest.reachShort, static_cast<double>(crowdTest.reachWorst));
    if(crowdTest.dump)
    {
        crowdDump = false;
        dumpPolys();
    }
}

// (Each steer: the armed crowd test's samples and end.)
void crowdCheck()
{
    if(!crowdTest.armed || sv.qcvm.time < crowdTest.nextSample)
    {
        return;
    }
    crowdTest.nextSample = sv.qcvm.time + 0.5;
    edict_t* p = progs::playerEdict();
    const glm::vec3 him = p ? geom::vec(p->v.origin) : glm::vec3{0.f};
    const int knock = progs::fields().vr_knockdown;
    int walkers = 0, came = 0;
    progs::forEachEdict(1, [&](edict_t* e) {
        const auto n = static_cast<za::SizeT>(NUM_FOR_EDICT(e));
        if(!walker(e) || e->v.health <= 0.f || n >= crowdTest.last.size() || !crowdTest.member[n])
        {
            return;
        }
        ++walkers;
        const glm::vec3 at = geom::vec(e->v.origin);
        if(!crowdTest.reachDone[n] && p)
        {
            float gap = 0.f;
            const bool whole = wholePath(e, feet(p), gap);
            crowdTest.reachDone[n] = gap >= 0.f ? 1 : 0;
            crowdTest.reachWhole += whole && gap >= 0.f ? 1 : 0;
            crowdTest.reachShort += !whole && gap >= 0.f ? 1 : 0;
            crowdTest.reachWorst = za::max(crowdTest.reachWorst, gap);
        }
        const bool near = glm::length(at - him) < crowdTest.near;
        crowdTest.came[n] = crowdTest.came[n] || near ? 1 : 0;
        came += crowdTest.came[n];
        glm::vec4& l = crowdTest.last[n];
        CrowdWin& w = crowdTest.win[n];
        if(l.w > 0.f && !near)
        {
            CrowdType& t = crowdType(PR_GetString(e->v.classname));
            const char* frame = functionName(e->v.think);
            const bool busy = (knock >= 0 && progs::fieldFloat(e, knock) != 0.f) || airborne(e) || busyFrame(frame);
            if(busy)
            {
                ++t.busy;
                t.busyAir += airborne(e) ? 1 : 0;
                t.busyKnock += knock >= 0 && progs::fieldFloat(e, knock) != 0.f ? 1 : 0;
                ++crowdTest.busy;
                countFrame(crowdTest.busyFrames, frame);
            }
            else
            {
                ++t.samples;
                ++crowdTest.samples;
                if(glm::length(glm::vec3{l} - at) < 8.f)
                {
                    const Agent* a = n < state.agents.size() ? &state.agents[n] : nullptr;
                    const Cause c = stallCause(e, a, w, p);
                    ++t.stalls;
                    ++t.causes[c];
                    ++crowdTest.stalls;
                    crowdTest.stallAt.pushBack(glm::vec4{at, static_cast<float>(c)});
                    Con_Printf("navstall|%s|%s|%.0f|%.0f|%.0f\n", t.name, causeNames[c], static_cast<double>(at.x),
                        static_cast<double>(at.y), static_cast<double>(at.z));
                }
            }
        }
        l = glm::vec4{at, 1.f};
        w = CrowdWin{};
    });
    if(sv.qcvm.time < crowdTest.until && came < walkers)
    {
        return;
    }
    crowdTest.armed = false;
    crowdReport();
    Con_Printf("nav crowd: %d walkers, %d of them came within %.0f of him in %.1f s; %d stalls in %d samples away from him "
               "(%d busy left out)\n",
        walkers, came, static_cast<double>(crowdTest.near), sv.qcvm.time - crowdTest.since, crowdTest.stalls, crowdTest.samples,
        crowdTest.busy);
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

// Where a walker heads for its foe: a melee one (the QC's vr_nav_melee: no missile attack, a dog's leap, an armed one
// with no ranged weapon; else no th_missile) farther than vr_nav_slot_engage takes a slot on a
// ring round him (vr_nav_slots, vr_nav_slot_radius): its own (none other chasing him holds it), the free one nearest
// its bearing from him, kept while it stays free; others his feet.
glm::vec3 approach(edict_t* ent, Agent& a, int num, int goalNum, const glm::vec3& pos, const glm::vec3& goalPos, double now)
{
    const int k = za::min(static_cast<int>(vr_nav_slots.value), 32);
    const int melee = progs::fields().vr_nav_melee, missile = progs::fields().th_missile;
    const bool isMelee = melee >= 0 ? progs::fieldFloat(ent, melee) != 0.f : missile >= 0 && progs::fieldInt(ent, missile) == 0;
    if(k <= 0 || !isMelee || flatDist(pos, goalPos) <= vr_nav_slot_engage.value)
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
    tickWorld();
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
    crowdNote(ent, a, pos, now);
    if(a.link)
    {
        if(a.cls == cls)
        {
            a.why = whyLink;
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
        a.why = whyOffMesh;
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
            a.why = whyNoPath;
            a.path.clear();
            ++state.fallbacks;
            return -1.f;
        }
        dtPolyRef buf[maxPath];
        int n = 0;
        (void)c->query->findPath(startRef, endRef, startPt, endPt, &filt, buf, &n, maxPath);
        a.path.clear();
        a.path.emplaceBackRange(buf, static_cast<za::SizeT>(n));
        a.partial = n > 0 && buf[n - 1] != endRef;
        if(a.partial)
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
        a.why = whyNoPath;
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
            a.why = whyLink;
            return beginLink(ent, a, *c, pos, now, refs[i], i);
        }
    }
    a.waitLink = 0;
    // None left, or the goal straight ahead on the mesh: Quake's own chase (its melee range, its dodges). A slot straight
    // ahead is still walked to.
    if(next < 0 || ((flags[next] & DT_STRAIGHTPATH_END) && a.slot < 0))
    {
        a.why = whyEnd;
        ++state.fallbacks;
        return -1.f;
    }
    a.why = whySteered;
    a.lastYaw = steerToward(*c, a, pos, next, nc, flags, num);
    return a.lastYaw;
}

int action(const edict_t* ent)
{
    const int num = ent ? NUM_FOR_EDICT(const_cast<edict_t*>(ent)) : 0;
    return num > 0 && static_cast<za::SizeT>(num) < state.agents.size() ? state.agents[static_cast<za::SizeT>(num)].action : 0;
}

float obstacles(int what)
{
    return obstacleInfo(what);
}

void serverFrame()
{
    if(vr_nav.value != 0.f && sv.active && state.ready)
    {
        tickWorld();
    }
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
    tickWorld(); // (the props' marks kept current while nothing chases)
    const float range = vr_nav_debug_range.value;
    const glm::vec3 lift{0.f, 0.f, 2.f};
    // The props cut out of the mesh: each one's outline at its foot and its top, joined at the corners (purple).
    const glm::vec4 purple{0.75f, 0.3f, 1.f, 1.f};
    for(const PropTrack& p : state.props)
    {
        if(!p.marked || glm::length(p.markOrigin - view) > range)
        {
            continue;
        }
        const Obstacle& o = p.mark;
        for(int k = 0; k < o.count; ++k)
        {
            const int j = (k + 1) % o.count;
            const glm::vec3 a{o.xy[k * 2], o.xy[k * 2 + 1], o.zlo + 2.f}, b{o.xy[j * 2], o.xy[j * 2 + 1], o.zlo + 2.f};
            const glm::vec3 up{0.f, 0.f, o.zhi - o.zlo};
            lines::sceneLine(a, b, 2.f, purple, purple);
            lines::sceneLine(a + up, b + up, 1.5f, purple, purple);
            lines::sceneLine(a, a + up, 1.f, purple, purple);
        }
    }
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
                            : area == areaNarrow ? glm::vec4{0.85f, 0.85f, 0.2f, 0.3f}
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
    // The crowd test's stalls (this one's or the last one's): a post by cause, red another monster in the way, blue a
    // wall, green a ledge, black in a wall, purple no chase, grey the rest.
    for(const glm::vec4& s : crowdTest.stallAt)
    {
        const int c = static_cast<int>(s.w);
        const glm::vec4 col = c == causeMonster ? glm::vec4{1.f, 0.1f, 0.1f, 1.f}
                              : c == causeWall  ? glm::vec4{0.2f, 0.4f, 1.f, 1.f}
                              : c == causeLedge ? glm::vec4{0.1f, 0.9f, 0.1f, 1.f}
                              : c == causeInSolid ? glm::vec4{0.f, 0.f, 0.f, 1.f}
                              : c == causeNoChase ? glm::vec4{0.6f, 0.2f, 0.9f, 1.f}
                                                  : glm::vec4{0.6f, 0.6f, 0.6f, 1.f};
        const glm::vec3 p{s};
        lines::sceneLine(p, p + glm::vec3{0.f, 0.f, 48.f}, 4.f, col, col);
    }
}

} // namespace qvr::nav
