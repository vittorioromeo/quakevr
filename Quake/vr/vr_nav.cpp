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
#include "Zancle/String/String.hpp"

#include <string.h>

namespace qvr::nav
{
namespace
{

// Bumped whenever the build could make another mesh from the same input (the steps, their constants, the areas).
constexpr unsigned navVersion = 1;
constexpr char navMagic[5] = "QVNM";

// Detour's areas and flags.
constexpr unsigned char areaGround = 1, areaWater = 2, areaSlime = 3, areaLava = 4;
constexpr unsigned short flagWalk = 1, flagBlocked = 2;

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
    bool fromDisk = false, failed = false;
    int polys = 0, tiles = 0;
    za::SizeT bytes = 0;
    za::Vector<dtPolyRef> blocked; // polygons a door or lift stands on (flagBlocked)
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
};

State state;
dtQueryFilter filter;
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
};

Settings settingsNow()
{
    return Settings{za::max(vr_nav_cell.value, 0.5f), za::max(vr_nav_cell_height.value, 0.25f), za::max(vr_nav_climb.value, 0.f),
        vr_nav_slope.value, za::max(vr_nav_clearance.value, 0.f), za::max(static_cast<int>(vr_nav_tile.value), 8)};
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
bool buildTile(const Geometry& g, const za::Vector<int>& bin, const za::Vector<int>& brushBin, const Settings& s, const rcConfig& base, int tx, int ty,
    const NavClass& c, Tile& out)
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
    unsigned char* data = nullptr;
    int size = 0;
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
        dtNavMeshCreateParams p{};
        p.verts = pmesh->verts;
        p.vertCount = pmesh->nverts;
        p.polys = pmesh->polys;
        p.polyAreas = pmesh->areas;
        p.polyFlags = pmesh->flags;
        p.polyCount = pmesh->npolys;
        p.nvp = pmesh->nvp;
        p.detailMeshes = dmesh->meshes;
        p.detailVerts = dmesh->verts;
        p.detailVertsCount = dmesh->nverts;
        p.detailTris = dmesh->tris;
        p.detailTriCount = dmesh->ntris;
        p.walkableHeight = c.height;
        p.walkableRadius = c.radius;
        p.walkableClimb = s.climb;
        p.tileX = tx;
        p.tileY = ty;
        p.tileLayer = 0;
        memcpy(p.bmin, pmesh->bmin, sizeof(p.bmin));
        memcpy(p.bmax, pmesh->bmax, sizeof(p.bmax));
        p.cs = cfg.cs;
        p.ch = cfg.ch;
        p.buildBvTree = true;
        ok = dtCreateNavMeshData(&p, &data, &size);
    } while(false);
    rcFreeHeightField(hf);
    rcFreeCompactHeightfield(chf);
    rcFreeContourSet(cset);
    rcFreePolyMesh(pmesh);
    rcFreePolyMeshDetail(dmesh);
    if(!ok)
    {
        dtFree(data);
        return false;
    }
    out = Tile{tx, ty, data, size};
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
    za::Vector<Tile> tiles;
    za::Vector<unsigned char> has;
    tiles.resize(bins.size(), Tile{0, 0, nullptr, 0});
    has.resize(bins.size(), 0);
    jobs::parallelFor(tileSite, bins.size(), 1, [&](za::SizeT begin, za::SizeT end) {
        for(za::SizeT i = begin; i < end; ++i)
        {
            const int x = static_cast<int>(i) % tilesX, y = static_cast<int>(i) / tilesX;
            has[i] = !bins[i].empty() && buildTile(g, bins[i], brushBins[i], s, cfg, x, y, c, tiles[i]) ? 1 : 0;
        }
    });
    c.built.clear();
    for(za::SizeT i = 0; i < tiles.size(); ++i)
    {
        if(has[i])
        {
            c.built.pushBack(tiles[i]);
        }
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
    const float v[] = {s.cs, s.ch, s.climb, s.slope, static_cast<float>(s.tile), c.radius, c.height, s.clearance};
    return diskcache::fnv(g.hash, v, sizeof(v));
}

unsigned checksum(const char* p, za::SizeT n)
{
    const za::U64 h = diskcache::fnv(diskcache::fnvStart, p, n);
    return static_cast<unsigned>(h ^ (h >> 32));
}

void serialise(const NavClass& c, za::Vector<char>& out)
{
    out.clear();
    const auto put = [&out](const void* p, za::SizeT n) { out.emplaceBackRange(static_cast<const char*>(p), n); };
    put(&c.params, sizeof(c.params));
    for(const Tile& t : c.built)
    {
        const int head[3] = {t.x, t.y, t.size};
        put(head, sizeof(head));
        put(t.data, static_cast<za::SizeT>(t.size));
    }
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
    c.query = nullptr;
    c.mesh = nullptr;
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

// The built tiles made the class's mesh (main thread).
void finishClass(NavClass& c)
{
    c.mesh = dtAllocNavMesh();
    c.failed = !c.mesh || dtStatusFailed(c.mesh->init(&c.params));
    c.polys = 0;
    c.bytes = 0;
    c.tiles = 0;
    for(Tile& t : c.built)
    {
        if(!c.failed && dtStatusSucceed(c.mesh->addTile(t.data, t.size, DT_TILE_FREE_DATA, 0, nullptr)))
        {
            c.polys += reinterpret_cast<const dtMeshHeader*>(t.data)->polyCount;
            c.bytes += static_cast<za::SizeT>(t.size);
            ++c.tiles;
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
    Con_DPrintf("nav: class r%.0f h%.0f: %d tiles, %d polygons, %.1f KB, %s %.1f ms\n", c.radius, c.height, c.tiles, c.polys,
        static_cast<double>(c.bytes) / 1024.0, c.fromDisk ? "read" : "built", c.ms);
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
    const int mode = diskcache::modeOf(diskcache::Product::NavMesh);
    if(mode != 0)
    {
        za::Vector<char> data;
        serialise(c, data);
        const unsigned sum = checksum(data.data(), data.size());
        const za::String build = buildName();
        if(mode == 2)
        {
            unsigned count = 0, oldSum = 0;
            za::Vector<char> old;
            if(diskcache::read(diskcache::Product::NavMesh, build.cStr(), c.key, "nav", navMagic, count, oldSum, old, "navmesh"))
            {
                const bool same = old.size() == data.size() && oldSum == sum && !memcmp(old.data(), data.data(), data.size());
                diskcache::noteChecked(diskcache::Product::NavMesh, same, "navmesh");
            }
        }
        diskcache::write(diskcache::Product::NavMesh, build.cStr(), c.key, "nav", navMagic,
            static_cast<unsigned>(c.built.size()), sum, data.data(), data.size());
    }
    finishClass(c);
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
    return c.query ? &c : nullptr;
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
    for(NavClass& c : state.classes)
    {
        if(!c.mesh)
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
        Con_Printf("nav: class %d: radius %.0f height %.0f: %s, %d tiles, %d polygons, %.1f KB, %s in %.1f ms, %d blocked\n",
            static_cast<int>(i), c.radius, c.height, c.pending ? "building" : c.failed ? "FAILED" : "ready", c.tiles, c.polys,
            static_cast<double>(c.bytes) / 1024.0, c.fromDisk ? "read" : "built", c.ms, static_cast<int>(c.blocked.size()));
    }
    Con_Printf("nav: %d steers, %d paths found, %d left to Quake's chase\n", state.steers, state.paths, state.fallbacks);
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

void rebuild_f()
{
    afterLoad();
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_nav_info", info_f);
    Cmd_AddCommand("vr_nav_cliptest", cliptest_f);
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
    const glm::vec3 startRc = toRc(pos), goalRc = toRc(goalPos);
    const float startExt[3] = {c->radius + 16.f, 32.f, c->radius + 16.f};
    dtPolyRef startRef = 0;
    float startPt[3];
    if(dtStatusFailed(c->query->findNearestPoly(&startRc.x, startExt, &filter, &startRef, startPt)) || !startRef)
    {
        a.path.clear();
        ++state.fallbacks;
        return -1.f;
    }
    const double now = sv.qcvm.time;
    const int goalNum = NUM_FOR_EDICT(goal);
    bool repath = a.cls != cls || a.goal != goalNum || a.path.empty() || now >= a.repathAt ||
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
        if(dtStatusFailed(c->query->findNearestPoly(&goalRc.x, goalExt, &filter, &endRef, endPt)) || !endRef)
        {
            a.path.clear();
            ++state.fallbacks;
            return -1.f;
        }
        dtPolyRef buf[maxPath];
        int n = 0;
        (void)c->query->findPath(startRef, endRef, startPt, endPt, &filter, buf, &n, maxPath);
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
    // None left, or the goal straight ahead on the mesh: Quake's own chase (its melee range, its dodges).
    if(next < 0 || (flags[next] & DT_STRAIGHTPATH_END))
    {
        ++state.fallbacks;
        return -1.f;
    }
    const glm::vec3 to = fromRc(&a.corners[next * 3]) - pos;
    float yaw = geom::yawOf(to);
    return yaw < 0.f ? yaw + 360.f : yaw;
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
