#include "vr_alloccount.h"
// vr_box3d_mesh.cpp -- the world mesh (its build and cache), the convex hulls of brush models, guns' pieces (and their disk cache), magazines and props, the shape definitions.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
jobs::Site meshSite{"box3d world mesh"}; // (its parallelFor: vr_jobs_sites)
} // namespace

// (Only reads the map: made on the game's thread pool while the map spawns, see beforeLoad.)
[[nodiscard]] b3MeshData* worldMesh(const qmodel_t* map, float m2u, bool junctions, MeshStats& stats)
{
    const double t0 = Sys_DoubleTime();
    struct Face
    {
        int first, count; // into corners
        glm::vec3 normal;
    };
    za::Vector<Face> faces;
    za::Vector<int> corners; // BSP vertex numbers, each face's outline in order
    za::Vector<uint8_t> used(static_cast<size_t>(map->numvertexes), 0);
    const auto position = [&](int v) { return vec(map->vertexes[v].position); };
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB) || surf.numedges < 3)
        {
            continue;
        }
        glm::vec3 normal = vec(surf.plane->normal);
        if(surf.flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        faces.pushBack({static_cast<int>(corners.size()), surf.numedges, normal});
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
            corners.pushBack(v);
            used[static_cast<size_t>(v)] = 1;
        }
    }

    // The corners in a grid (cells of `cell` units, sorted by cell), to find those near an edge.
    constexpr float cell = 64.f;
    constexpr float onEdge = 0.1f; // units from the edge's line: in it
    const auto cellOf = [](const glm::vec3& p) { return glm::ivec3{glm::floor(p / cell)}; };
    const auto keyOf = [](const glm::ivec3& c) {
        const auto part = [](int v) { return static_cast<uint64_t>(static_cast<uint32_t>(v) & 0x1fffffu); };
        return (part(c.x) << 42) | (part(c.y) << 21) | part(c.z);
    };
    za::Vector<za::Pair<uint64_t, int>> grid;
    for(int v = 0; v < map->numvertexes; v++)
    {
        if(used[static_cast<size_t>(v)])
        {
            grid.emplaceBack(keyOf(cellOf(position(v))), v);
        }
    }
    za::quickSort(grid.begin(), grid.end());
    // Each cell's run in the grid (looked up by key: a binary search of the grid for each of 27 cells round each step
    // along each edge was most of vrstart's 570 ms mesh, 90k faces).
    ankerl::unordered_dense::map<uint64_t, za::Pair<int, int>> cellRuns;
    cellRuns.reserve(grid.size());
    for(za::SizeT i = 0; i < grid.size();)
    {
        za::SizeT j = i + 1;
        while(j < grid.size() && grid[j].first == grid[i].first)
        {
            ++j;
        }
        cellRuns.emplace(grid[i].first, za::makePair(static_cast<int>(i), static_cast<int>(j)));
        i = j;
    }

    za::Vector<int32_t> remap(static_cast<size_t>(map->numvertexes), -1);
    za::Vector<b3Vec3> vertices;
    za::Vector<int32_t> indices;
    const auto index = [&](int v) {
        int32_t& r = remap[static_cast<size_t>(v)];
        if(r < 0)
        {
            r = static_cast<int32_t>(vertices.size());
            const glm::vec3 p = position(v) / m2u;
            vertices.pushBack(b3Vec3{p.x, p.y, p.z});
        }
        return r;
    };
    const auto triangle = [&](int32_t a, int32_t b, int32_t c, const glm::vec3& pa, const glm::vec3& pb, const glm::vec3& pc,
                              const glm::vec3& normal) {
        const glm::vec3 n = glm::cross(pb - pa, pc - pa);
        if(glm::length(n) < 1e-3f)
        {
            return; // degenerate
        }
        if(glm::dot(n, normal) < 0.f)
        {
            za::genericSwap(b, c);
        }
        indices.pushBack(a);
        indices.pushBack(b);
        indices.pushBack(c);
    };

    // A face's outline: its corners, and the T-junctions put into its edges. Found for runs of faces at once on the pool
    // (each run's outlines one after the other; read only: the map, the grid), then made into triangles in the faces'
    // order, as on one thread (the same mesh; vrstart's 90k faces: 430 ms of searching on one thread).
    struct Outlines
    {
        za::Vector<int> outline;
        za::Vector<uint8_t> corner; // per outline entry: one of the face's own corners (else a T-junction put in)
        za::Vector<int> start;      // [face of the run]: its outline's first entry (and one past the last's end)
        za::Vector<uint8_t> split;  // [face of the run]: it got a T-junction
        int junctions{0};
    };
    const auto findOutline = [&](const Face& f, Outlines& o, za::Vector<za::Pair<float, int>>& between,
                                 za::Vector<uint64_t>& nearCells) {
        za::Vector<int>& outline = o.outline;
        za::Vector<uint8_t>& corner = o.corner;
        bool split = false;
        for(int k = 0; k < f.count; k++)
        {
            const int a = corners[static_cast<size_t>(f.first + k)], b = corners[static_cast<size_t>(f.first + (k + 1) % f.count)];
            outline.pushBack(a);
            corner.pushBack(1);
            if(!junctions)
            {
                continue;
            }
            const glm::vec3 pa = position(a), pb = position(b);
            const glm::vec3 d = pb - pa;
            const float length = glm::length(d);
            if(length < 4.f * onEdge)
            {
                continue;
            }
            const glm::vec3 dir = d / length;
            // The cells along the edge (and those round them), in steps of half a cell; each looked in once (a corner
            // found twice was found at the same t, and dropped as one below either way).
            between.clear();
            nearCells.clear();
            const int steps = za::max(1, static_cast<int>(za::ceil(length / (cell * 0.5f))));
            glm::ivec3 last{INT32_MIN};
            for(int step = 0; step <= steps; step++)
            {
                const glm::ivec3 c = cellOf(pa + d * (static_cast<float>(step) / static_cast<float>(steps)));
                if(c == last)
                {
                    continue;
                }
                last = c;
                for(int dz = -1; dz <= 1; dz++)
                {
                    for(int dy = -1; dy <= 1; dy++)
                    {
                        for(int dx = -1; dx <= 1; dx++)
                        {
                            nearCells.pushBack(keyOf(c + glm::ivec3{dx, dy, dz}));
                        }
                    }
                }
            }
            za::quickSort(nearCells.begin(), nearCells.end());
            for(za::SizeT i = 0; i < nearCells.size(); ++i)
            {
                if(i > 0 && nearCells[i] == nearCells[i - 1])
                {
                    continue;
                }
                const auto run = cellRuns.find(nearCells[i]);
                if(run == cellRuns.end())
                {
                    continue;
                }
                for(int g = run->second.first; g < run->second.second; ++g)
                {
                    const int v = grid[static_cast<za::SizeT>(g)].second;
                    if(v == a || v == b)
                    {
                        continue;
                    }
                    const glm::vec3 p = position(v);
                    const float t = glm::dot(p - pa, dir);
                    if(t > 2.f * onEdge && t < length - 2.f * onEdge && glm::length(p - (pa + dir * t)) <= onEdge)
                    {
                        between.emplaceBack(t, v);
                    }
                }
            }
            if(between.empty())
            {
                continue;
            }
            za::quickSort(between.begin(), between.end());
            float lastT = 0.f;
            for(const auto& [t, v] : between)
            {
                if(t - lastT < 2.f * onEdge)
                {
                    continue; // (found from two cells, or two BSP vertices at one place)
                }
                lastT = t;
                outline.pushBack(v);
                corner.pushBack(0);
                o.junctions++;
                split = true;
            }
        }
        return split;
    };
    constexpr za::SizeT runFaces = 1024;
    za::Vector<Outlines> runs((faces.size() + runFaces - 1) / runFaces);
    jobs::parallelFor(meshSite, runs.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            za::Vector<za::Pair<float, int>> between;
            za::Vector<uint64_t> nearCells;
            for(za::SizeT r = begin; r < end; ++r)
            {
                Outlines& o = runs[r];
                const za::SizeT last = za::min(faces.size(), (r + 1) * runFaces);
                for(za::SizeT i = r * runFaces; i < last; ++i)
                {
                    o.start.pushBack(static_cast<int>(o.outline.size()));
                    o.split.pushBack(findOutline(faces[i], o, between, nearCells) ? 1 : 0);
                }
                o.start.pushBack(static_cast<int>(o.outline.size()));
            }
        });
    za::Vector<int> outline;
    za::Vector<uint8_t> corner;
    for(za::SizeT i = 0; i < faces.size(); ++i)
    {
        const Face& f = faces[i];
        const Outlines& o = runs[i / runFaces];
        const za::SizeT j = i % runFaces;
        const auto first = static_cast<za::SizeT>(o.start[j]), past = static_cast<za::SizeT>(o.start[j + 1]);
        outline.clear();
        corner.clear();
        outline.emplaceBackRange(o.outline.data() + first, past - first);
        corner.emplaceBackRange(o.corner.data() + first, past - first);
        const bool split = o.split[j] != 0;
        stats.faces++;
        if(!split)
        {
            const int a = outline[0];
            for(size_t k = 2; k < outline.size(); k++)
            {
                triangle(index(a), index(outline[k - 1]), index(outline[k]), position(a), position(outline[k - 1]), position(outline[k]),
                    f.normal);
            }
            continue;
        }
        // Fanned from one of its corners whose two edges have no junction in them (the fan's triangles then have every
        // piece of the outline as an edge, none of them flat), else from a new vertex in its middle.
        stats.junctionFaces++;
        const size_t n = outline.size();
        size_t from = n;
        for(size_t k = 0; k < n && from == n; k++)
        {
            if(corner[k] && corner[(k + 1) % n] && corner[(k + n - 1) % n])
            {
                from = k;
            }
        }
        if(from < n)
        {
            const int a = outline[from];
            for(size_t k = 2; k < n; k++)
            {
                const int b = outline[(from + k - 1) % n], c = outline[(from + k) % n];
                triangle(index(a), index(b), index(c), position(a), position(b), position(c), f.normal);
            }
            continue;
        }
        stats.middleFans++;
        glm::vec3 middle{0.f};
        for(const int v : outline)
        {
            middle += position(v);
        }
        middle /= static_cast<float>(outline.size());
        const int32_t m = static_cast<int32_t>(vertices.size());
        vertices.pushBack(b3Vec3{middle.x / m2u, middle.y / m2u, middle.z / m2u});
        for(size_t k = 0; k < outline.size(); k++)
        {
            const int a = outline[k], b = outline[(k + 1) % outline.size()];
            triangle(m, index(a), index(b), middle, position(a), position(b), f.normal);
        }
    }
    for(const Outlines& o : runs)
    {
        stats.junctions += o.junctions;
    }
    stats.triangles = static_cast<int>(indices.size() / 3);
    b3MeshData* mesh = nullptr;
    if(!indices.empty())
    {
        b3MeshDef def{};
        def.vertices = vertices.data();
        def.stride = 0;
        def.indices = indices.data();
        def.materialIndices = nullptr;
        def.vertexCount = static_cast<int>(vertices.size());
        def.triangleCount = static_cast<int>(indices.size() / 3);
        def.weldVertices = false;
        def.weldTolerance = 0.f;
        def.useMedianSplit = false;
        def.identifyEdges = true;
        def.clockWiseWinding = false;
        mesh = b3CreateMesh(&def, nullptr, 0);
    }
    stats.ms = (Sys_DoubleTime() - t0) * 1000.0;
    return mesh;
}

namespace
{
constexpr size_t meshCacheScales = 4;
za::U32 meshCacheSerial = 0;

[[nodiscard]] za::U32 floatWord(float f)
{
    return ZA_BIT_CAST(za::U32, f);
}
} // namespace

const MeshInputs& inputsOf(const qmodel_t* map)
{
    MeshInputs& in = meshInputs;
    if(in.map == map)
    {
        return in;
    }
    in.map = map;
    in.same.clear();
    in.words.clear();
    in.words.pushBack(static_cast<za::U32>(map->numvertexes));
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB) || surf.numedges < 3) // (worldMesh's)
        {
            continue;
        }
        const float sign = (surf.flags & SURF_PLANEBACK) ? -1.f : 1.f;
        in.words.pushBack(static_cast<za::U32>(surf.numedges));
        for(int k = 0; k < 3; k++)
        {
            in.words.pushBack(floatWord(sign * surf.plane->normal[k]));
        }
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const unsigned v = e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1];
            in.words.pushBack(v);
            for(int c = 0; c < 3; c++)
            {
                in.words.pushBack(floatWord(map->vertexes[v].position[c]));
            }
        }
    }
    za::U64 h = 14695981039346656037ull; // (FNV-1a over the words)
    for(const za::U32 w : in.words)
    {
        h = (h ^ w) * 1099511628211ull;
    }
    in.key = h;
    return in;
}

namespace
{
[[nodiscard]] bool sameMap(const MeshCache& c, const qmodel_t* map)
{
    if(strcmp(c.name, map->name) || c.vertexes != map->numvertexes || c.surfaces != map->numsurfaces ||
       c.junctions != (vr_box3d_mesh_junctions.value != 0.f))
    {
        return false;
    }
    MeshInputs& in = meshInputs;
    (void)inputsOf(map);
    if(c.inputsKey != in.key)
    {
        return false;
    }
    if(za::find(in.same.begin(), in.same.end(), c.serial) != in.same.end())
    {
        return true;
    }
    if(c.inputs.size() != in.words.size() ||
       memcmp(c.inputs.data(), in.words.data(), in.words.size() * sizeof(za::U32)) != 0)
    {
        return false;
    }
    in.same.pushBack(c.serial);
    return true;
}
} // namespace

[[nodiscard]] MeshCache* findMesh(const qmodel_t* map, float m2u)
{
    settleMesh(); // (the one beforeLoad made, if any)
    for(MeshCache& c : meshCache)
    {
        if(c.m2u == m2u && sameMap(c, map))
        {
            return &c;
        }
    }
    return nullptr;
}

// Whether `mesh` is this map's, as the settings want it (else the world is made again: destroyed first, as its shape
// uses the mesh).
[[nodiscard]] bool meshCurrent(const qmodel_t* map, float m2u, const b3MeshData* mesh)
{
    const MeshCache* c = findMesh(map, m2u);
    return c && c->mesh == mesh;
}

namespace
{
// (Only with the world destroyed: a mesh let go may be the one its shape used.)
// A mesh made, into the cache (the latest used last): the least recently used let go first, beyond the few scales of
// this map and the few other maps kept (vr_box3d_mesh_keep_maps; another setting's, at once).
MeshCache& addMesh(MeshCache&& made, const qmodel_t* map)
{
    const int keepMaps = za::clamp(static_cast<int>(vr_box3d_mesh_keep_maps.value), 0, 8);
    const bool junctions = vr_box3d_mesh_junctions.value != 0.f;
    // Kept: this map's newest scales, then the other maps' newest (one scale each), newest first.
    za::Vector<za::U8> keep(meshCache.size(), 0);
    {
        size_t scales = 1; // (the one being added)
        za::Vector<const MeshCache*> others;
        for(size_t i = meshCache.size(); i-- > 0;)
        {
            const MeshCache& c = meshCache[i];
            if(c.junctions != junctions)
            {
                continue;
            }
            if(sameMap(c, map))
            {
                if(scales < meshCacheScales)
                {
                    keep[i] = 1;
                    scales++;
                }
                continue;
            }
            bool counted = false;
            for(const MeshCache* o : others)
            {
                counted = counted || (!strcmp(o->name, c.name) && o->inputsKey == c.inputsKey);
            }
            if(!counted && others.size() < static_cast<size_t>(keepMaps))
            {
                others.pushBack(&c);
                keep[i] = 1;
            }
        }
    }
    size_t index = 0;
    for(auto it = meshCache.begin(); it != meshCache.end(); index++)
    {
        if(!keep[index] && !(world && world->mesh == it->mesh)) // (never the one the world's shape uses)
        {
            if(it->mesh)
            {
                b3DestroyMesh(it->mesh);
            }
            it = meshCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
    MeshCache& c = meshCache.emplaceBack(ZA_MOVE(made));
    const MeshInputs& in = inputsOf(map);
    c.inputsKey = in.key;
    c.inputs = in.words;
    c.serial = ++meshCacheSerial;
    meshInputs.same.pushBack(c.serial);
    if(vr_debug_box3d.value)
    {
        // (its bytes' hash, FNV-1a: the same made on the pool or not)
        uint32_t hash = 2166136261u;
        const auto* bytes = reinterpret_cast<const unsigned char*>(c.mesh);
        for(int32_t i = 0; c.mesh && i < c.mesh->byteCount; i++)
        {
            hash = (hash ^ bytes[i]) * 16777619u;
        }
        Con_Printf("box3d: the mesh of %s: %d faces, %d triangles, %d T-junctions joined in %d faces (%d fanned from the middle), %.1f ms, hash %08x\n", c.name,
            c.stats.faces, c.stats.triangles, c.stats.junctions, c.stats.junctionFaces, c.stats.middleFans, c.stats.ms,
            static_cast<unsigned>(hash));
    }
    return c;
}
} // namespace

void settleMesh()
{
    if(!pendingMesh.job.valid())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    MeshCache made = pendingMesh.key;
    made.mesh = pendingMesh.job.get();
    made.stats = pendingMesh.key.stats;
    VR_TimeAdd("box3d: the world's mesh, waited for (made on the pool)", Sys_DoubleTime() - t0);
    (void)addMesh(ZA_MOVE(made), pendingMesh.map);
}
[[nodiscard]] b3MeshData* cachedWorldMesh(const qmodel_t* map, float m2u)
{
    if(MeshCache* found = findMesh(map, m2u))
    {
        za::rotate(found, found + 1, meshCache.data() + meshCache.size()); // the latest used last
        return meshCache.back().mesh;
    }
    MeshCache made;
    q_strlcpy(made.name, map->name, sizeof(made.name));
    made.vertexes = map->numvertexes;
    made.surfaces = map->numsurfaces;
    made.m2u = m2u;
    made.junctions = vr_box3d_mesh_junctions.value != 0.f;
    made.mesh = worldMesh(map, m2u, made.junctions, made.stats);
    return addMesh(ZA_MOVE(made), map).mesh;
}
[[nodiscard]] za::Vector<b3Vec3> corners(const za::Vector<HalfSpace>& planes, float m2u)
{
    za::Vector<glm::dvec3> points;
    const size_t count = planes.size();
    for(size_t i = 0; i < count; i++)
    {
        for(size_t j = i + 1; j < count; j++)
        {
            const glm::dvec3 ij = glm::cross(planes[i].n, planes[j].n);
            if(glm::dot(ij, ij) < 1e-12)
            {
                continue;
            }
            for(size_t k = j + 1; k < count; k++)
            {
                const double det = glm::dot(ij, planes[k].n);
                if(za::abs(det) < 1e-9)
                {
                    continue;
                }
                const glm::dvec3 p = (glm::cross(planes[j].n, planes[k].n) * planes[i].d +
                                         glm::cross(planes[k].n, planes[i].n) * planes[j].d + ij * planes[k].d) /
                                     det;
                bool inside = true;
                for(size_t m = 0; m < count && inside; m++)
                {
                    inside = glm::dot(planes[m].n, p) <= planes[m].d + 0.01;
                }
                if(inside && !za::anyOf(points.begin(), points.end(), [&](const glm::dvec3& q) { return glm::distance(p, q) < 0.05; }))
                {
                    points.pushBack(p);
                }
            }
        }
    }
    za::Vector<b3Vec3> out;
    out.reserve(points.size());
    for(const glm::dvec3& p : points)
    {
        out.pushBack(b3Vec3{static_cast<float>(p.x / m2u), static_cast<float>(p.y / m2u), static_cast<float>(p.z / m2u)});
    }
    return out;
}

namespace
{
// The solid leaves of a brush model's hull 0 (the drawn brushes' space, split by the BSP), each a convex region.
void solidLeaves(const hull_t& hull, int num, za::Vector<HalfSpace>& path, float m2u, za::Vector<b3HullData*>& out, int depth)
{
    if(num < 0)
    {
        if(num == CONTENTS_SOLID)
        {
            const za::Vector<b3Vec3> points = corners(path, m2u);
            if(points.size() >= 4)
            {
                if(b3HullData* h = b3CreateHull(points.data(), static_cast<int>(points.size()), 64))
                {
                    out.pushBack(h);
                }
            }
        }
        return;
    }
    if(depth > 100 || num > hull.lastclipnode)
    {
        return;
    }
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    path.pushBack({-n, -static_cast<double>(plane.dist)}); // in front: dot(n, p) >= dist
    solidLeaves(hull, node.children[0], path, m2u, out, depth + 1);
    path.back() = {n, static_cast<double>(plane.dist)}; // behind
    solidLeaves(hull, node.children[1], path, m2u, out, depth + 1);
    path.popBack();
}
} // namespace

[[nodiscard]] const za::Vector<b3HullData*>& moverHulls(qmodel_t* model)
{
    auto it = world->moverHulls.find(model);
    const za::U32 rev = brushes::revision(model);
    if(it != world->moverHulls.end())
    {
        const auto r = world->moverHullRevs.find(model);
        const za::U32 oldRev = r == world->moverHullRevs.end() ? 0u : r->second;
        if(oldRev == rev)
        {
            return it->second;
        }
        for(b3HullData* hull : it->second)
        {
            world->retiredHulls.pushBack({model, oldRev, hull});
        }
        world->moverHulls.erase(it);
    }
    if(rev)
    {
        world->moverHullRevs[model] = rev;
    }
    za::Vector<b3HullData*> hulls;
    const hull_t& hull = model->hulls[0];
    if(hull.clipnodes && hull.planes)
    {
        za::Vector<HalfSpace> path;
        for(int i = 0; i < 3; i++) // the model's bounds close the leaves open to the outside
        {
            glm::dvec3 n{0.0};
            n[i] = 1.0;
            path.pushBack({n, static_cast<double>(model->maxs[i]) + 1.0});
            path.pushBack({-n, -static_cast<double>(model->mins[i]) + 1.0});
        }
        solidLeaves(hull, hull.firstclipnode, path, world->m2u, hulls, 0);
    }
    if(hulls.empty()) // nothing solid found: its bounds
    {
        const glm::vec3 lo = vec(model->mins), hi = vec(model->maxs);
        za::Array<b3Vec3, 8> points;
        for(int i = 0; i < 8; i++)
        {
            points[i] = world->toM(glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z});
        }
        if(b3HullData* h = b3CreateHull(points.data(), 8, 8))
        {
            hulls.pushBack(h);
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s: %d convex pieces\n", model->name, static_cast<int>(hulls.size()));
    }
    return world->moverHulls.emplace(model, ZA_MOVE(hulls)).first->second;
}

namespace
{
// How far the drawn corners reach out of a hull made of some of them (the most any is beyond one of its faces).
[[nodiscard]] float hullShortfall(const b3HullData* hull, const za::Vector<b3Vec3>& points)
{
    const b3Plane* planes = b3GetHullPlanes(hull);
    float most = 0.f;
    for(const b3Vec3& p : points)
    {
        float out = -1e9f;
        for(int i = 0; i < hull->faceCount; i++)
        {
            out = za::max(out, b3Dot(planes[i].normal, p) - planes[i].offset);
        }
        most = za::max(most, out);
    }
    return most;
}

// A hull of the drawn corners (metres) with at least `budget` vertices, more as needed to leave none of them further
// out than hullTolerance units (Box3D's limit is 128).
constexpr float hullTolerance = 0.2f;
} // namespace

[[nodiscard]] b3HullData* fittedHull(const za::Vector<b3Vec3>& points, int budget, float m2u)
{
    b3HullData* hull = b3CreateHull(points.data(), static_cast<int>(points.size()), budget);
    while(hull && budget < B3_MAX_HULL_VERTICES && hull->vertexCount >= budget &&
          hullShortfall(hull, points) * m2u > hullTolerance)
    {
        budget = za::min(budget * 3 / 2, B3_MAX_HULL_VERTICES);
        b3HullData* more = b3CreateHull(points.data(), static_cast<int>(points.size()), budget);
        if(!more)
        {
            break;
        }
        b3DestroyHull(hull);
        hull = more;
    }
    return hull;
}

namespace
{
// The guns' pieces kept on disk (vr_gun_pieces_cache; vr_diskcache.hpp, cache/gunpieces): a gun's first cut in a
// session (10-85 ms on the main thread, as the first grunt's gun drops) read back instead from a session before. The
// key: a hash of all that decompose reads (the triangles, the settings); the folder: decompose's build (convex::build).
// The file: the report, each piece's end in points, the points (model units), exactly as cut.
constexpr char gunPiecesMagic[5] = "QVRG";
constexpr unsigned gunPiecesMostFile = 64; // (a file claiming more pieces is not ours)

[[nodiscard]] unsigned long long gunPiecesKey(const za::Vector<glm::vec3>& triangles, const convex::Settings& settings)
{
    QVR_TRACY_SCOPE(tracyScope, "gun pieces key");
    unsigned long long h = diskcache::fnvStart;
    h = diskcache::fnv(h, &settings.cell, sizeof(settings.cell));
    h = diskcache::fnv(h, &settings.tolerance, sizeof(settings.tolerance));
    h = diskcache::fnv(h, &settings.maxPieces, sizeof(settings.maxPieces));
    const za::U64 count = triangles.size();
    h = diskcache::fnv(h, &count, sizeof(count));
    return diskcache::fnv(h, triangles.data(), triangles.size() * sizeof(glm::vec3));
}

[[nodiscard]] bool readGunPieces(
    unsigned long long key, za::Vector<convex::Piece>& pieces, convex::Report& report, const char* name)
{
    QVR_TRACY_SCOPE(tracyScope, "gun pieces read");
    unsigned count = 0, points = 0;
    za::Vector<char> data;
    char what[96];
    snprintf(what, sizeof(what), "(%s, %016llx)", name ? name : "a gun", key);
    if(!diskcache::read(diskcache::Product::GunPieces, convex::build(), key, "gpc", gunPiecesMagic, count, points, data, what) ||
       count > gunPiecesMostFile ||
       data.size() != sizeof(convex::Report) + count * sizeof(za::I32) + static_cast<size_t>(points) * sizeof(glm::vec3))
    {
        return false;
    }
    memcpy(&report, data.data(), sizeof(report));
    const char* ends = data.data() + sizeof(report);
    const char* at = ends + count * sizeof(za::I32);
    pieces.clear();
    pieces.resize(count);
    za::I32 begin = 0;
    for(unsigned i = 0; i < count; i++)
    {
        za::I32 end = 0;
        memcpy(&end, ends + i * sizeof(za::I32), sizeof(end));
        if(end < begin || end > static_cast<za::I32>(points))
        {
            pieces.clear();
            return false;
        }
        pieces[i].points.resize(static_cast<za::SizeT>(end - begin));
        memcpy(pieces[i].points.data(), at + static_cast<size_t>(begin) * sizeof(glm::vec3),
            static_cast<size_t>(end - begin) * sizeof(glm::vec3));
        begin = end;
    }
    if(begin != static_cast<za::I32>(points))
    {
        pieces.clear();
        return false;
    }
    return true;
}

void writeGunPieces(unsigned long long key, const za::Vector<convex::Piece>& pieces, const convex::Report& report)
{
    QVR_TRACY_SCOPE(tracyScope, "gun pieces write");
    za::I32 points = 0;
    for(const convex::Piece& piece : pieces)
    {
        points += static_cast<za::I32>(piece.points.size());
    }
    za::Vector<char> data;
    data.resize(sizeof(report) + pieces.size() * sizeof(za::I32) + static_cast<size_t>(points) * sizeof(glm::vec3));
    memcpy(data.data(), &report, sizeof(report));
    char* ends = data.data() + sizeof(report);
    char* at = ends + pieces.size() * sizeof(za::I32);
    za::I32 end = 0;
    for(za::SizeT i = 0; i < pieces.size(); i++)
    {
        memcpy(at + static_cast<size_t>(end) * sizeof(glm::vec3), pieces[i].points.data(),
            pieces[i].points.size() * sizeof(glm::vec3));
        end += static_cast<za::I32>(pieces[i].points.size());
        memcpy(ends + i * sizeof(za::I32), &end, sizeof(end));
    }
    diskcache::write(diskcache::Product::GunPieces, convex::build(), key, "gpc", gunPiecesMagic, static_cast<unsigned>(pieces.size()),
        static_cast<unsigned>(points), data.data(), data.size());
}

[[nodiscard]] bool samePieces(const za::Vector<convex::Piece>& a, const za::Vector<convex::Piece>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.size(); i++)
    {
        if(a[i].points.size() != b[i].points.size() ||
           memcmp(a[i].points.data(), b[i].points.data(), a[i].points.size() * sizeof(glm::vec3)) != 0)
        {
            return false;
        }
    }
    return true;
}
} // namespace

// A gun's convex pieces (decompose), read from the disk when a session before cut them (vr_gun_pieces_cache 1: the
// same pieces; 2: cut as well and compared, the cut ones used); true: read.
[[nodiscard]] bool cutGun(const za::Vector<glm::vec3>& triangles, const convex::Settings& settings,
    za::Vector<convex::Piece>& pieces, convex::Report& report, const char* name)
{
    QVR_TRACY_SCOPE(tracyScope, "gun pieces cut");
    const int mode = static_cast<int>(za::clamp(vr_gun_pieces_cache.value, 0.f, 2.f));
    const unsigned long long key = mode ? gunPiecesKey(triangles, settings) : 0;
    const double t0 = Sys_DoubleTime();
    const bool fromDisk = mode && readGunPieces(key, pieces, report, name);
#ifdef QVR_TRACY
    char detail[192];
    snprintf(detail, sizeof(detail), "%s mode=%d hit=%d key=%016llx corners=%zu most=%d", name, mode, fromDisk, key,
        static_cast<size_t>(triangles.size()), settings.maxPieces);
    VR_TracyText(detail);
#endif
    if(fromDisk)
    {
        gunPiecesDisk.read++;
        report.ms = (Sys_DoubleTime() - t0) * 1000.0; // (the read's time)
        if(mode < 2)
        {
            return true;
        }
    }
    za::Vector<convex::Piece> cut;
    convex::Report cutReport;
    if(!convex::decompose(triangles, settings, cut, &cutReport))
    {
        cut.clear();
    }
    gunPiecesDisk.cut++;
    if(fromDisk)
    {
        gunPiecesDisk.checked++;
        diskcache::noteChecked(diskcache::Product::GunPieces, samePieces(pieces, cut), name);
        if(!samePieces(pieces, cut))
        {
            gunPiecesDisk.differed++;
            Con_Warning("vr_gun_pieces_cache 2: %s's convex pieces differ from their file (%d of %d so far)\n", name,
                gunPiecesDisk.differed.load(), gunPiecesDisk.checked.load());
        }
    }
    else if(mode)
    {
        writeGunPieces(key, cut, cutReport);
        gunPiecesDisk.written++;
    }
    pieces = ZA_MOVE(cut);
    report = cutReport;
    return false;
}

// A held gun's pieces mirrored (the off hand's: y negated), from the main hand's (`base`, `baseKey`).
[[nodiscard]] const za::Vector<b3HullData*>& mirroredPieces(const PropHullKey& baseKey, const za::Vector<b3HullData*>& base)
{
    const PropHullKey key{baseKey.model, -3, {}};
    auto it = world->gunPieces.find(key);
    if(it != world->gunPieces.end())
    {
        return it->second;
    }
    za::Vector<b3HullData*> hulls;
    za::Vector<b3Vec3> points;
    for(const b3HullData* piece : base)
    {
        points.clear();
        for(int i = 0; i < piece->vertexCount; i++)
        {
            const b3Vec3 p = b3GetHullPoints(piece)[i];
            points.pushBack(b3Vec3{p.x, -p.y, p.z});
        }
        if(b3HullData* hull = convex::hull(points.data(), static_cast<int>(points.size()), static_cast<int>(points.size())))
        {
            hulls.pushBack(hull);
        }
    }
    if(const auto report = world->gunReports.find(baseKey); report != world->gunReports.end())
    {
        world->gunReports[key] = report->second;
    }
    return world->gunPieces.emplace(key, ZA_MOVE(hulls)).first->second;
}

// A gun's seated magazine (vr_reload_mag_collide: solid with the gun) as one hull (metres) of its drawn `vertices`
// (units, in the gun's entity's axes: held::magazineVertices), kept a world by its model, `code` (a held gun's -4, -5
// mirrored; a lying one's -6) and its vertices' box (the gun's drawn transform). nullptr: none.
[[nodiscard]] const b3HullData* magazineHull(const qmodel_t* mag, int code, const za::Vector<glm::vec3>& vertices)
{
    if(vertices.size() < 4)
    {
        return nullptr;
    }
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const glm::vec3& v : vertices)
    {
        lo = glm::min(lo, v);
        hi = glm::max(hi, v);
    }
    const PropHullKey key{mag, code, {lo.x, lo.y, lo.z, hi.x, hi.y, hi.z}};
    auto it = world->propHulls.find(key);
    if(it != world->propHulls.end())
    {
        return it->second;
    }
    za::Vector<b3Vec3> points;
    points.reserve(vertices.size());
    for(const glm::vec3& v : vertices)
    {
        points.pushBack(world->toM(v));
    }
    b3HullData* hull = convex::hull(points.data(), static_cast<int>(points.size()), 32);
    if(vr_debug_box3d.value && hull)
    {
        Con_Printf("box3d: %s seated: a hull of %d vertices, %.2f x %.2f x %.2f units\n", mag->name, hull->vertexCount,
            hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
    }
    world->propHulls.emplace(key, hull);
    return hull;
}
[[nodiscard]] bool isGun(edict_t* ent)
{
    const char* name = PR_GetString(ent->v.classname);
    return !strcmp(name, "thrown_weapon") || !strncmp(name, "weapon_", 7);
}

namespace
{
void noteHullBuild(const qmodel_t* model, int frame, double ms)
{
    HullBuildStats& s = hullBuilds;
    s.built++;
    s.ms += ms;
    s.mostMs = za::max(s.mostMs, ms);
    za::SizeT at = s.slowest.size();
    while(at > 0 && s.slowest[at - 1].ms < ms)
    {
        at--;
    }
    if(at >= s.slowest.size())
    {
        return;
    }
    for(za::SizeT i = s.slowest.size() - 1; i > at; i--)
    {
        s.slowest[i] = s.slowest[i - 1];
    }
    HullBuildStats::Slow& slow = s.slowest[at];
    q_strlcpy(slow.model, model ? model->name : "?", sizeof(slow.model));
    slow.frame = frame;
    slow.ms = ms;
    slow.time = qcvm ? qcvm->time : 0.0;
}
} // namespace

// The prop's drawn corners (metres), false if it has too few for a hull.
[[nodiscard]] bool propHullPoints(edict_t* ent, za::Vector<b3Vec3>& points)
{
    za::Vector<glm::vec3>& vertices = scratch.propVerts;
    if(!held::drawnVertices(ent, vertices) || vertices.size() < 4)
    {
        return false;
    }
    points.clear();
    points.reserve(vertices.size());
    for(const glm::vec3& v : vertices)
    {
        points.pushBack(world->toM(v));
    }
    return true;
}

// The prop's hull of its drawn corners (any thread: prebuildPropHulls). A weapon's shape in detail (budget 32: it rests
// on its side, its grip, its magazine); the rest a little blockier (16: a backpack, a gib: a rounded hull rolls down a
// slope, a real one's give stops it). But never short of the drawn surface by more than hullTolerance: the hull is a
// subset of the drawn corners (Box3D's quickhull stops at the budget), and a corner left out is drawn inside what the
// body rests on (a corpse's 16-corner hull left its drawn back 1.3 units in the floor). The budget grows until every
// drawn corner is within it. A flat model's hull is thinner than Box3D can collide well: none (its box instead).
[[nodiscard]] b3HullData* propHullOf(const za::Vector<b3Vec3>& points, int budget, float m2u)
{
    b3HullData* hull = fittedHull(points, budget, m2u);
    if(hull && hull->innerRadius * m2u < 0.25f)
    {
        b3DestroyHull(hull);
        hull = nullptr;
    }
    return hull;
}

// The prop's hull: the convex hull of its drawn surface (an alias model's frame, a brush model's faces: the ammo and
// health boxes as drawn, not Quake's padded box), or none (nullptr: its box).
[[nodiscard]] b3HullData* propHull(edict_t* ent, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi)
{
    const PropHullKey key{model, static_cast<int>(ent->v.frame), {lo.x, lo.y, lo.z, hi.x, hi.y, hi.z}};
    auto it = world->propHulls.find(key);
    if(it != world->propHulls.end())
    {
        return it->second;
    }
    const double t0 = Sys_DoubleTime();
    b3HullData* hull = nullptr;
    za::Vector<b3Vec3> points;
    if(const auto pending = world->pendingHulls.find(key); pending != world->pendingHulls.end())
    {
        hull = pending->second.get(); // (built ahead: done, or waited for, or built here if no worker came to it)
        world->pendingHulls.erase(pending);
        hullBuilds.ahead++;
        if(vr_box3d_hulls_ahead.value >= 2.f) // (built here too and compared: Box3D's hash of the hull's contents)
        {
            b3HullData* here = propHullPoints(ent, points) ? propHullOf(points, isWeaponLike(ent) ? 32 : 16, world->m2u) : nullptr;
            const bool same = (!here && !hull) || (here && hull && here->hash == hull->hash &&
                                                    here->vertexCount == hull->vertexCount && here->faceCount == hull->faceCount);
            (same ? hullBuilds.same : hullBuilds.differed)++;
            if(here)
            {
                b3DestroyHull(here);
            }
        }
    }
    else if(propHullPoints(ent, points))
    {
        hull = propHullOf(points, isWeaponLike(ent) ? 32 : 16, world->m2u);
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s frame %d: %s\n", model->name, key.frame,
            hull ? va("a hull of %d vertices, %.0f cm^3", hull->vertexCount, hull->volume * 1e6f) : "a box");
    }
    noteHullBuild(model, key.frame, (Sys_DoubleTime() - t0) * 1000.0);
    world->propHulls.emplace(key, hull);
    return hull;
}

// A monster's (or another solid alias model's) hull: its drawn model at rest (frame 0, its first stand frame), as
// props' are (propHull's cache, frame -1), turned with its yaw: props rest against the monster, not against Quake's
// box round it (a grunt's is 32 units wide, its drawn body half that). The one shape for all its frames: a body's
// shape made again as it animates would lose its contacts (their warm start, and a new touch each time). nullptr: its
// box.
[[nodiscard]] b3HullData* actorHull(edict_t* ent, qmodel_t* model)
{
    const PropHullKey key{model, -1, {}};
    auto it = world->propHulls.find(key);
    if(it != world->propHulls.end())
    {
        return it->second;
    }
    const double t0 = Sys_DoubleTime();
    b3HullData* hull = nullptr;
    za::Vector<glm::vec3>& vertices = scratch.actorVerts;
    const float frame = ent->v.frame;
    ent->v.frame = 0.f;
    const bool drawn = held::drawnVertices(ent, vertices);
    ent->v.frame = frame;
    if(drawn && vertices.size() >= 4)
    {
        za::Vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.pushBack(world->toM(v));
        }
        hull = b3CreateHull(points.data(), static_cast<int>(points.size()), 24);
        if(hull && hull->innerRadius * world->m2u < 2.f)
        {
            b3DestroyHull(hull); // a thin thing (a sprite-like model): Quake's box
            hull = nullptr;
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s at rest: %s\n", model->name,
            hull ? va("a hull of %d vertices, %.0f litres", hull->vertexCount, hull->volume * 1e3f) : "its box");
    }
    noteHullBuild(model, -1, (Sys_DoubleTime() - t0) * 1000.0);
    world->propHulls.emplace(key, hull);
    return hull;
}
[[nodiscard]] b3ShapeDef shapeDef(int num, uint64_t category, uint64_t mask)
{
    b3ShapeDef def = b3DefaultShapeDef();
    def.userData = userOf(num);
    def.filter.categoryBits = category;
    def.filter.maskBits = mask;
    def.baseMaterial.friction = za::max(vr_throw_friction.value, 0.f);
    def.baseMaterial.restitution = 0.f;
    def.density = 1.f;
    return def;
}

// An explosion's chunk (vr_explosiondebris.cpp: progs/vr_explosion_debris.mdl, its size its box).
[[nodiscard]] bool isChunk(const qmodel_t* model)
{
    return model && modelmeta::is(model, modelmeta::Id::VrExplosionDebris);
}

// Whether a contact's shapes are a chunk's: its sounds, touches and impacts are left out (a chunk at 9 m/s would
// knock a crate's sound, touch a door, set off an explosive box's hard hit).
[[nodiscard]] bool chunkPair(b3ShapeId a, b3ShapeId b)
{
    return ((b3Shape_GetFilter(a).categoryBits | b3Shape_GetFilter(b).categoryBits) & catChunk) != 0;
}

// A chunk's shape: a sphere as wide as its box (its entity's size), its own mass (.vr_prop_mass), bouncing as
// vr_explosion_debris_bounce, no events or filtering of its own (made inside a monster, Box3D pushes it out).
void addChunkShape(edict_t* ent, int num, b3BodyId body)
{
    b3ShapeDef def = shapeDef(num, catProp | catChunk, chunkMask);
    const float radius = za::max(ent->v.maxs[0], 0.25f) / world->m2u;
    const float volume = 4.18879f * radius * radius * radius;
    def.density = za::max(massSetting(ent, nullptr), 1e-4f) / za::max(volume, 1e-9f);
    def.baseMaterial.restitution = CLAMP(0.f, vr_explosion_debris_bounce.value, 1.f);
    def.baseMaterial.rollingResistance = 0.2f; // (a sphere: it settles instead of rolling across the floor)
    def.enableContactEvents = false;
    def.enableHitEvents = false;
    def.enablePreSolveEvents = false;
    def.enableCustomFiltering = false;
    const b3Sphere sphere{b3Vec3{0.f, 0.f, 0.f}, radius};
    b3CreateSphereShape(body, &def, &sphere);
}

// The magazine seated in the gun `ent` lying about (drawn by the client: immersive reloading, its magazine in, not
// QVR_WPNFLAG_NOMAG), solid with it (vr_reload_mag_collide): its model (view::magazineModelOf), nullptr none.
[[nodiscard]] const qmodel_t* seatedMagazine(edict_t* ent, const qmodel_t* model)
{
    if(!vr_reload_mag_collide.value || vr_holster_mode.value != 0.f || static_cast<int>(vr_reload_mode.value) != 3 ||
        !model || model->type != mod_alias || !isGun(ent))
    {
        return nullptr;
    }
    constexpr int weaponFlagNoMag = 16; // QC's QVR_WPNFLAG_NOMAG (vr_defs.qc)
    if(static_cast<int>(fieldFloatOr(ent, fields().weaponflags, 0.f)) & weaponFlagNoMag)
    {
        return nullptr;
    }
    return view::magazineModelOf(model);
}
} // namespace box3d::detail
