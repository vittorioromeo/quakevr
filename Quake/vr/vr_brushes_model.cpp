// vr_brushes_model.cpp -- the brush model (vr_brushes.cpp): polygons from a brush's planes, and the Quake brush model
// made from them (planes, hulls 0/1/2 as chains, edges, textures, surfaces, nodes) and set on a qmodel_t.

#include "vr_brushes_internal.hpp"
#include "vr_geom.hpp"

namespace qvr::brushes
{

namespace detail
{

// Quake's BaseWindingForPlane: a huge square on the plane, clockwise seen from in front.
[[nodiscard]] Winding baseWinding(const glm::dvec3& n, double d)
{
    int axis = 0;
    for(int i = 1; i < 3; i++)
    {
        if(za::fabs(n[i]) > za::fabs(n[axis]))
        {
            axis = i;
        }
    }
    glm::dvec3 up{0.0};
    up[axis == 2 ? 0 : 2] = 1.0;
    up = glm::normalize(up - n * glm::dot(up, n));
    const glm::dvec3 right = glm::cross(up, n) * worldExtent;
    up *= worldExtent;
    const glm::dvec3 org = n * d;
    Winding w;
    w.pushBack(org - right + up);
    w.pushBack(org + right + up);
    w.pushBack(org + right - up);
    w.pushBack(org - right - up);
    return w;
}

// Keeps the part of `w` behind the plane (dot(p, n) <= d), its order kept.
void clipWinding(Winding& w, const glm::dvec3& n, double d)
{
    Winding out;
    const za::SizeT count = w.size();
    for(za::SizeT i = 0; i < count; i++)
    {
        const glm::dvec3& a = w[i];
        const glm::dvec3& b = w[(i + 1) % count];
        const double da = glm::dot(a, n) - d, db = glm::dot(b, n) - d;
        if(da <= onEpsilon)
        {
            out.pushBack(a);
        }
        if((da > onEpsilon && db < -onEpsilon) || (da < -onEpsilon && db > onEpsilon))
        {
            out.pushBack(a + (b - a) * (da / (da - db)));
        }
    }
    w = ZA_MOVE(out);
}

[[nodiscard]] double weld(double v)
{
    const double g = za::round(v * 256.0) / 256.0;
    return za::fabs(g - v) < weldEpsilon ? g : v;
}

// Each face's polygon (world space; empty: the face has no area). True if it is a solid that can be made.
bool polygons(const Brush& b, za::Vector<Winding>& out)
{
    out.clear();
    int withArea = 0;
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        const Face& f = b.faces[i];
        Winding w = baseWinding(f.normal, f.dist);
        for(za::SizeT j = 0; j < b.faces.size() && w.size() >= 3; j++)
        {
            if(j != i)
            {
                clipWinding(w, b.faces[j].normal, b.faces[j].dist);
            }
        }
        for(glm::dvec3& p : w)
        {
            p = glm::dvec3{weld(p.x), weld(p.y), weld(p.z)};
        }
        // (corners welded onto one another: dropped)
        Winding u;
        for(const glm::dvec3& p : w)
        {
            if(u.empty() || glm::length(p - u.back()) > weldEpsilon)
            {
                u.pushBack(p);
            }
        }
        while(u.size() > 1 && glm::length(u.front() - u.back()) <= weldEpsilon)
        {
            u.popBack();
        }
        if(u.size() < 3)
        {
            u.clear();
        }
        else
        {
            withArea++;
        }
        out.pushBack(ZA_MOVE(u));
    }
    return withArea >= 4;
}

// ---------------------------------------------------------------------------------------------------------------------
// The model made from a brush: Quake's in-memory brush model, its arrays here (the qmodel_t points into them).

// The planes as Quake stores them: an axial plane's normal positive, any other's largest component positive; `flip`
// when that turned the face's normal round.
[[nodiscard]] mplane_t canonical(const glm::dvec3& normal, double dist, bool& flip)
{
    glm::dvec3 n = normal;
    double d = dist;
    int axis = 0;
    for(int i = 1; i < 3; i++)
    {
        if(za::fabs(n[i]) > za::fabs(n[axis]))
        {
            axis = i;
        }
    }
    flip = n[axis] < 0.0;
    if(flip)
    {
        n = -n;
        d = -d;
    }
    mplane_t p{};
    for(int i = 0; i < 3; i++)
    {
        p.normal[i] = static_cast<float>(n[i]);
    }
    p.dist = static_cast<float>(d);
    const bool axial = za::fabs(n[axis] - 1.0) < 1e-9;
    p.type = static_cast<byte>(axial ? PLANE_X + axis : PLANE_ANYX + axis);
    p.signbits = 0;
    for(int i = 0; i < 3; i++)
    {
        if(p.normal[i] < 0.f)
        {
            p.signbits = static_cast<byte>(p.signbits | (1 << i));
        }
    }
    return p;
}

// A chain of clipnodes, one a plane (outward normal, distance, model space): in front of any, empty; behind them all,
// solid. Its planes appended to `planes`.
void chain(const za::Vector<glm::dvec4>& faces, za::Vector<mplane_t>& planes, za::Vector<mclipnode_t>& out)
{
    out.clear();
    const int count = static_cast<int>(faces.size());
    for(int k = 0; k < count; k++)
    {
        bool flip = false;
        planes.pushBack(canonical(glm::dvec3{faces[k]}, faces[k].w, flip));
        mclipnode_t node{};
        node.planenum = static_cast<int>(planes.size()) - 1;
        const int inside = k + 1 < count ? k + 1 : CONTENTS_SOLID;
        node.children[flip ? 1 : 0] = CONTENTS_EMPTY;
        node.children[flip ? 0 : 1] = inside;
        out.pushBack(node);
    }
}

// The usable texture named so in the world (null: none).
[[nodiscard]] texture_t* worldTexture(const char* name)
{
    const qmodel_t* w = sv.worldmodel ? sv.worldmodel : cl.worldmodel;
    if(!w || !w->textures)
    {
        return nullptr;
    }
    for(int i = 0; i < w->numtextures; i++)
    {
        texture_t* t = w->textures[i];
        if(t && !q_strcasecmp(t->name, name))
        {
            return t;
        }
    }
    return nullptr;
}

// The bevels a box swept against a convex brush needs (Quake 2's qbsp, AddBrushBevels), appended to `planes` (its faces,
// outward, model space: world minus `origin`): the axial planes of its bounds lo..hi (world) where no face is one, then at
// each edge that isn't along an axis the planes through it along each axis that have the whole brush behind them. Pushed
// out by a box's support (hulls 1 and 2), the set is the brush grown by the box: without them a sloped or turned face's
// pushed plane would reach past its corners (the player stopped in the air off a wedge's edge).
void bevels(const za::Vector<Winding>& polys, const glm::dvec3& origin, const glm::dvec3& lo, const glm::dvec3& hi,
    za::Vector<glm::dvec4>& planes)
{
    const auto have = [&](const glm::dvec3& n) {
        for(const glm::dvec4& p : planes)
        {
            if(glm::length(glm::dvec3{p} - n) < 1e-6)
            {
                return true;
            }
        }
        return false;
    };
    for(int axis = 0; axis < 3; axis++)
    {
        for(const double sign : {1.0, -1.0})
        {
            glm::dvec3 n{0.0};
            n[axis] = sign;
            if(!have(n))
            {
                planes.pushBack(glm::dvec4{n, sign > 0.0 ? hi[axis] - origin[axis] : -(lo[axis] - origin[axis])});
            }
        }
    }
    const auto snapped = [](glm::dvec3 v) {
        for(int k = 0; k < 3; k++)
        {
            if(za::fabs(v[k]) < 1e-9)
            {
                v[k] = 0.0;
            }
            else if(za::fabs(za::fabs(v[k]) - 1.0) < 1e-9)
            {
                v[k] = v[k] > 0.0 ? 1.0 : -1.0;
            }
        }
        return v;
    };
    for(const Winding& w : polys)
    {
        for(za::SizeT j = 0; j < w.size(); j++)
        {
            const glm::dvec3 a = w[j] - origin, b = w[(j + 1) % w.size()] - origin;
            if(glm::length(b - a) < 0.5)
            {
                continue;
            }
            const glm::dvec3 edge = snapped(glm::normalize(b - a));
            if(za::fabs(edge.x) == 1.0 || za::fabs(edge.y) == 1.0 || za::fabs(edge.z) == 1.0)
            {
                continue; // (along an axis: the axial bevels have it)
            }
            for(int axis = 0; axis < 3; axis++)
            {
                for(const double sign : {-1.0, 1.0})
                {
                    glm::dvec3 along{0.0};
                    along[axis] = sign;
                    glm::dvec3 n = glm::cross(edge, along);
                    if(glm::length(n) < 0.5)
                    {
                        continue;
                    }
                    n = snapped(glm::normalize(n));
                    if(have(n))
                    {
                        continue;
                    }
                    const double d = glm::dot(a, n);
                    bool outer = true;
                    for(const Winding& v : polys)
                    {
                        for(const glm::dvec3& p : v)
                        {
                            outer = outer && glm::dot(p - origin, n) - d <= 0.1;
                        }
                    }
                    if(outer)
                    {
                        planes.pushBack(glm::dvec4{n, d});
                    }
                }
            }
        }
    }
}

// A face's edges and box (build: the surfaces').
struct FaceDraw
{
    int firstedge, numedges;
    glm::vec3 mins, maxs;
};

// build's state from phase to phase (each phase a function below, in the order build calls them).
struct BrushBuild
{
    const Brush* b{nullptr};
    za::Vector<Winding> polys;
    glm::dvec3 lo{1e30}, hi{-1e30};
    glm::dvec3 origin{0.0};
    za::Vector<glm::dvec4> faces; // the faces with area, model space: hull 0's planes
    za::Vector<int> faceOf;       // and each one's face of the brush
    za::Vector<FaceDraw> draws;
};

// The faces with area, model space: hull 0's planes. Hulls 1 and 2: the planes pushed out by the box (its support
// along each normal), with the bevels a brush that is not a box needs (bevels()); the same set is the narrow box's
// (clipBox).
void buildHulls(BrushBuild& bb, Built& out)
{
    const Brush* b = bb.b;
    const za::Vector<Winding>& polys = bb.polys;
    const glm::dvec3 &lo = bb.lo, &hi = bb.hi, &origin = bb.origin;
    za::Vector<glm::dvec4>& faces = bb.faces;
    za::Vector<int>& faceOf = bb.faceOf;
    for(za::SizeT i = 0; i < b->faces.size(); i++)
    {
        if(polys[i].empty())
        {
            continue;
        }
        const Face& f = b->faces[i];
        faces.pushBack(glm::dvec4{f.normal, f.dist - glm::dot(f.normal, origin)});
        faceOf.pushBack(static_cast<int>(i));
    }
    za::Vector<glm::dvec4> grown = faces;
    bevels(polys, origin, lo, hi, grown);
    for(const glm::dvec4& p : grown)
    {
        out.clipPlanes.pushBack(glm::vec4{p});
    }
    za::Vector<glm::dvec4> pushed[3];
    pushed[0] = faces;
    const qmodel_t* world = sv.worldmodel ? sv.worldmodel : cl.worldmodel;
    for(int h = 1; h <= 2; h++)
    {
        glm::dvec3 cmins{h == 1 ? -16.0 : -32.0, h == 1 ? -16.0 : -32.0, -24.0};
        glm::dvec3 cmaxs{h == 1 ? 16.0 : 32.0, h == 1 ? 16.0 : 32.0, h == 1 ? 32.0 : 64.0};
        if(world)
        {
            cmins = glm::dvec3{world->hulls[h].clip_mins[0], world->hulls[h].clip_mins[1], world->hulls[h].clip_mins[2]};
            cmaxs = glm::dvec3{world->hulls[h].clip_maxs[0], world->hulls[h].clip_maxs[1], world->hulls[h].clip_maxs[2]};
        }
        for(const glm::dvec4& f : grown)
        {
            double reach = 0.0;
            for(int i = 0; i < 3; i++)
            {
                reach -= f[i] * (f[i] > 0.0 ? cmins[i] : cmaxs[i]);
            }
            pushed[h].pushBack(glm::dvec4{glm::dvec3{f}, f.w + reach});
        }
    }
    for(int h = 0; h < 3; h++)
    {
        chain(pushed[h], out.planes, out.clipnodes[h]);
        out.hullPlanes[h] = pushed[h];
    }
}

// Vertices (shared), edges (shared, Quake's signed surfedges), one texinfo and surface a face.
void buildEdges(BrushBuild& bb, Built& out)
{
    const za::Vector<Winding>& polys = bb.polys;
    const glm::dvec3& origin = bb.origin;
    const za::Vector<int>& faceOf = bb.faceOf;
    za::Vector<FaceDraw>& draws = bb.draws;
    const int numFaces = static_cast<int>(bb.faces.size());
    out.edges.pushBack(medge_t{{0u, 0u}}); // (edge 0 unused: a surfedge's sign is its way)
    const auto vertexOf = [&](const glm::dvec3& p) {
        const glm::vec3 q{p - origin};
        for(za::SizeT i = 0; i < out.vertexes.size(); i++)
        {
            const float* v = out.vertexes[i].position;
            if(za::fabs(v[0] - q.x) < 1e-3f && za::fabs(v[1] - q.y) < 1e-3f && za::fabs(v[2] - q.z) < 1e-3f)
            {
                return static_cast<unsigned>(i);
            }
        }
        mvertex_t v{};
        geom::store(q, v.position);
        out.vertexes.pushBack(v);
        return static_cast<unsigned>(out.vertexes.size() - 1);
    };
    for(int k = 0; k < numFaces; k++)
    {
        const Winding& w = polys[static_cast<za::SizeT>(faceOf[k])];
        FaceDraw fd{static_cast<int>(out.surfedges.size()), static_cast<int>(w.size()), glm::vec3{1e30f}, glm::vec3{-1e30f}};
        for(za::SizeT i = 0; i < w.size(); i++)
        {
            const unsigned a = vertexOf(w[i]), c = vertexOf(w[(i + 1) % w.size()]);
            int found = 0;
            for(za::SizeT e = 1; e < out.edges.size() && !found; e++)
            {
                if(out.edges[e].v[0] == c && out.edges[e].v[1] == a)
                {
                    found = -static_cast<int>(e);
                }
            }
            if(!found)
            {
                out.edges.pushBack(medge_t{{a, c}});
                found = static_cast<int>(out.edges.size()) - 1;
            }
            out.surfedges.pushBack(found);
            const glm::vec3 q{w[i] - origin};
            fd.mins = glm::min(fd.mins, q);
            fd.maxs = glm::max(fd.maxs, q);
        }
        draws.pushBack(fd);
    }
}

// The textures (the world's; a name it lacks: its first usable one), the used ones by their type.
void buildTextures(const BrushBuild& bb, Built& out)
{
    const Brush* b = bb.b;
    const glm::dvec3& origin = bb.origin;
    const za::Vector<int>& faceOf = bb.faceOf;
    const int numFaces = static_cast<int>(bb.faces.size());
    for(int k = 0; k < numFaces; k++)
    {
        const Face& f = b->faces[static_cast<za::SizeT>(faceOf[k])];
        texture_t* t = worldTexture(f.texture);
        if(!t || !usableTexture(t))
        {
            t = worldTexture(currentTexture());
        }
        if(!t)
        {
            t = textureCount() > 0 ? textureAt(0) : nullptr;
        }
        int index = -1;
        for(za::SizeT i = 0; i < out.textures.size(); i++)
        {
            if(out.textures[i] == t)
            {
                index = static_cast<int>(i);
            }
        }
        if(index < 0)
        {
            out.textures.pushBack(t);
            index = static_cast<int>(out.textures.size()) - 1;
        }
        // Model space: the offset carries the origin (the texture stays where the world's axes put it).
        mtexinfo_t ti{};
        const glm::dvec3 axes[2] = {f.uAxis / (f.scaleU != 0.0 ? f.scaleU : 1.0), f.vAxis / (f.scaleV != 0.0 ? f.scaleV : 1.0)};
        const double offsets[2] = {f.uOffset, f.vOffset};
        for(int a = 0; a < 2; a++)
        {
            for(int i = 0; i < 3; i++)
            {
                ti.vecs[a][i] = static_cast<float>(axes[a][i]);
            }
            ti.vecs[a][3] = static_cast<float>(offsets[a] + glm::dot(axes[a], origin));
        }
        ti.texnum = index;
        ti.flags = 0;
        out.texinfo.pushBack(ti);
    }
    int perType[TEXTYPE_COUNT]{};
    for(texture_t* t : out.textures)
    {
        perType[t ? t->type : TEXTYPE_DEFAULT]++;
    }
    int at = 0;
    for(int ty = 0; ty < TEXTYPE_COUNT; ty++)
    {
        out.texofs[ty] = at;
        at += perType[ty];
    }
    out.texofs[TEXTYPE_COUNT] = at;
    out.used.resize(out.textures.size(), 0);
    int fill[TEXTYPE_COUNT];
    for(int ty = 0; ty < TEXTYPE_COUNT; ty++)
    {
        fill[ty] = out.texofs[ty];
    }
    for(za::SizeT i = 0; i < out.textures.size(); i++)
    {
        const int ty = out.textures[i] ? out.textures[i]->type : TEXTYPE_DEFAULT;
        out.used[static_cast<za::SizeT>(fill[ty]++)] = static_cast<int>(i);
    }
}

// The surfaces (their plane hull 0's: the first numFaces planes) and hull 0's nodes over them.
void buildSurfaces(const BrushBuild& bb, Built& out)
{
    const za::Vector<int>& faceOf = bb.faceOf;
    const za::Vector<FaceDraw>& draws = bb.draws;
    const int numFaces = static_cast<int>(bb.faces.size());
    out.surfaces.resize(static_cast<za::SizeT>(numFaces));
    out.surfaceFace.resize(static_cast<za::SizeT>(numFaces));
    out.nodes.resize(static_cast<za::SizeT>(numFaces));
    for(int k = 0; k < numFaces; k++)
    {
        msurface_t& s = out.surfaces[static_cast<za::SizeT>(k)];
        s = msurface_t{};
        const mclipnode_t& cn = out.clipnodes[0][static_cast<za::SizeT>(k)];
        s.plane = &out.planes[static_cast<za::SizeT>(cn.planenum)];
        s.flags = cn.children[1] == CONTENTS_EMPTY ? SURF_PLANEBACK : 0; // (the outside behind the stored plane)
        const FaceDraw& fd = draws[static_cast<za::SizeT>(k)];
        for(int i = 0; i < 3; i++)
        {
            s.mins[i] = fd.mins[i];
            s.maxs[i] = fd.maxs[i];
        }
        s.vbo_firstvert = -1;
        s.firstedge = fd.firstedge;
        s.numedges = static_cast<short>(fd.numedges);
        s.lightmaptexturenum = -1;
        s.styles[0] = 0;
        for(int i = 1; i < MAXLIGHTMAPS; i++)
        {
            s.styles[i] = 255;
        }
        s.samples = nullptr;
        s.luxsamples = nullptr;
        s.texinfo = &out.texinfo[static_cast<za::SizeT>(k)];
        // Quake's CalcSurfaceExtents.
        float smin[2]{1e30f, 1e30f}, smax[2]{-1e30f, -1e30f};
        for(int e = 0; e < fd.numedges; e++)
        {
            const int se = out.surfedges[static_cast<za::SizeT>(fd.firstedge + e)];
            const float* v = out.vertexes[se >= 0 ? out.edges[static_cast<za::SizeT>(se)].v[0] : out.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            for(int a = 0; a < 2; a++)
            {
                const float st = v[0] * s.texinfo->vecs[a][0] + v[1] * s.texinfo->vecs[a][1] + v[2] * s.texinfo->vecs[a][2] + s.texinfo->vecs[a][3];
                smin[a] = za::min(smin[a], st);
                smax[a] = za::max(smax[a], st);
            }
        }
        for(int a = 0; a < 2; a++)
        {
            const int bmin = static_cast<int>(za::floor(smin[a] / 16.f)), bmax = static_cast<int>(za::ceil(smax[a] / 16.f));
            s.texturemins[a] = bmin * 16;
            s.extents[a] = static_cast<short>(za::clamp((bmax - bmin) * 16, 0, 32000));
        }
        out.surfaceFace[static_cast<za::SizeT>(k)] = faceOf[k];

        mnode_t& n = out.nodes[static_cast<za::SizeT>(k)];
        n = mnode_t{};
        n.contents = 0;
        n.plane = s.plane;
        for(int i = 0; i < 3; i++)
        {
            n.minmaxs[i] = out.mins[i];
            n.minmaxs[3 + i] = out.maxs[i];
        }
        n.firstsurface = static_cast<unsigned>(k);
        n.numsurfaces = 1;
    }
    for(int k = 0; k < numFaces; k++)
    {
        const mclipnode_t& cn = out.clipnodes[0][static_cast<za::SizeT>(k)];
        mnode_t& n = out.nodes[static_cast<za::SizeT>(k)];
        for(int side = 0; side < 2; side++)
        {
            const int c = cn.children[side];
            n.children[side] = c >= 0 ? &out.nodes[static_cast<za::SizeT>(c)]
                                      : reinterpret_cast<mnode_t*>(&out.leafs[c == CONTENTS_SOLID ? 0 : 1]);
            if(c >= 0)
            {
                out.nodes[static_cast<za::SizeT>(c)].parent = &n;
            }
        }
    }
    for(mleaf_t& l : out.leafs)
    {
        for(int i = 0; i < 3; i++)
        {
            l.minmaxs[i] = out.mins[i];
            l.minmaxs[3 + i] = out.maxs[i];
        }
    }
}

// Made from the brush (null: an empty model: nothing solid, nothing drawn). `quakeHulls`: the world's hull 1 and 2
// boxes.
void build(const Brush* b, Built& out)
{
    out = Built{};
    out.leafs[0].contents = CONTENTS_SOLID;
    out.leafs[1].contents = CONTENTS_EMPTY;
    for(mleaf_t& l : out.leafs)
    {
        l.visframe = 0;
        l.compressed_vis = nullptr;
        l.firstmarksurface = nullptr;
        l.nummarksurfaces = 0;
    }
    BrushBuild bb;
    bb.b = b;
    za::Vector<Winding>& polys = bb.polys;
    if(!b || !polygons(*b, polys))
    {
        return;
    }
    glm::dvec3 &lo = bb.lo, &hi = bb.hi;
    for(const Winding& w : polys)
    {
        for(const glm::dvec3& p : w)
        {
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    bb.origin = glm::floor((lo + hi) * 0.5 + 0.5);
    const glm::dvec3& origin = bb.origin;
    out.origin = glm::vec3{origin};
    out.mins = glm::vec3{lo - origin};
    out.maxs = glm::vec3{hi - origin};

    buildHulls(bb, out);
    buildEdges(bb, out);
    buildTextures(bb, out);
    buildSurfaces(bb, out);
}

// The empty model (a name whose brush is gone, or a save's: nothing solid, nothing drawn). Never freed.
Built emptyBuilt;
mclipnode_t emptyClipnode{0, {CONTENTS_EMPTY, CONTENTS_EMPTY}};
mplane_t emptyPlane{{0.f, 0.f, 1.f}, 0.f, PLANE_Z, 0, {0, 0}};

// The qmodel_t made to show `b` (null: empty). Its slot's fields (texofs, firstcmd, vbo_firstvert) are the upload's.
void apply(qmodel_t* mod, Built* b, int sortId)
{
    const qmodel_t* world = sv.worldmodel ? sv.worldmodel : cl.worldmodel;
    mod->needload = false;
    mod->type = mod_brush;
    mod->numframes = 2;
    mod->synctype = ST_SYNC;
    mod->flags = 0;
    mod->sortkey = static_cast<uint32_t>((0x8000 + sortId) & MODSORT_MODELMASK) << MODSORT_FRAMEBITS;
    const bool empty = !b || b->surfaces.empty();
    Built& d = empty ? emptyBuilt : *b;
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->clipmins[i] = d.mins[i];
        mod->maxs[i] = mod->clipmaxs[i] = d.maxs[i];
    }
    float radius = 0.f;
    for(int i = 0; i < 3; i++)
    {
        const float m = za::max(za::fabs(d.mins[i]), za::fabs(d.maxs[i]));
        radius += m * m;
    }
    radius = za::sqrt(radius);
    for(int i = 0; i < 3; i++)
    {
        mod->rmins[i] = mod->ymins[i] = -radius;
        mod->rmaxs[i] = mod->ymaxs[i] = radius;
    }
    mod->clipbox = false;
    mod->firstmodelsurface = 0;
    mod->nummodelsurfaces = static_cast<int>(d.surfaces.size());
    mod->numsubmodels = 0;
    mod->submodels = nullptr;
    mod->numplanes = static_cast<int>(d.planes.size());
    mod->planes = d.planes.empty() ? &emptyPlane : d.planes.data();
    mod->numleafs = 1;
    mod->leafs = d.leafs;
    mod->numvertexes = static_cast<int>(d.vertexes.size());
    mod->vertexes = d.vertexes.data();
    mod->numedges = static_cast<int>(d.edges.size());
    mod->edges = d.edges.data();
    mod->numnodes = static_cast<int>(d.nodes.size());
    mod->nodes = d.nodes.data();
    mod->numtexinfo = static_cast<int>(d.texinfo.size());
    mod->texinfo = d.texinfo.data();
    mod->numsurfaces = static_cast<int>(d.surfaces.size());
    mod->surfaces = d.surfaces.data();
    mod->numsurfedges = static_cast<int>(d.surfedges.size());
    mod->surfedges = d.surfedges.data();
    mod->numclipnodes = static_cast<int>(d.clipnodes[0].size());
    mod->clipnodes = d.clipnodes[0].empty() ? &emptyClipnode : d.clipnodes[0].data();
    mod->nummarksurfaces = 0;
    mod->marksurfaces = nullptr;
    for(int h = 0; h < MAX_MAP_HULLS; h++)
    {
        hull_t& hull = mod->hulls[h];
        const int src = h < 3 ? h : 0;
        const za::Vector<mclipnode_t>& nodes = d.clipnodes[src];
        hull.clipnodes = nodes.empty() ? &emptyClipnode : const_cast<mclipnode_t*>(nodes.data());
        hull.planes = mod->planes;
        hull.firstclipnode = nodes.empty() ? CONTENTS_EMPTY : 0;
        hull.lastclipnode = nodes.empty() ? 0 : static_cast<int>(nodes.size()) - 1;
        for(int i = 0; i < 3; i++)
        {
            hull.clip_mins[i] = world && h > 0 && h < 3 ? world->hulls[h].clip_mins[i] : 0.f;
            hull.clip_maxs[i] = world && h > 0 && h < 3 ? world->hulls[h].clip_maxs[i] : 0.f;
        }
    }
    mod->numtextures = static_cast<int>(d.textures.size());
    mod->textures = d.textures.data();
    for(int i = 0; i <= TEXTYPE_COUNT; i++)
    {
        mod->texofs[i] = 0; // (drawn once its slot is written: upload)
    }
    mod->usedtextures = d.used.data();
    mod->firstcmd = 0;
    mod->visdata = nullptr;
    mod->lightdata = nullptr;
    mod->luxdata = nullptr;
    mod->entities = nullptr;
    mod->litfile = false;
    mod->viswarn = false;
    mod->bspversion = world ? world->bspversion : 0;
    mod->contentstransparent = 0;
    mod->haslitwater = false;
}
} // namespace detail
} // namespace qvr::brushes
