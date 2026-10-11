// vr_brushes.cpp -- runtime brushes (vr_brushes.hpp; docs/vr-port/BRUSH_TOOL.md): the brush (planes, textures,
// alignment) and its .map text, the brushes of the map (each an entity, func_qvr_brush), the brush model made from a
// brush (planes, hulls 0/1/2 as chains, faces), its slot in the brush buffers (vertices, indices, draw commands, its
// flat light) and the vr_brush_* commands.

#include "vr_brushes.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_files.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_modelcollide.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::brushes
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// Polygons from planes.

using Winding = za::SmallVector<glm::dvec3, 16>;

constexpr double onEpsilon = 1e-6;   // a point this near a plane is on it
constexpr double weldEpsilon = 1e-4; // corners this near a 1/256 grid point are put on it
constexpr double worldExtent = 32768.0;
constexpr float distEpsilon = 0.03125f; // Quake's back-off from a plane hit (DIST_EPSILON)

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
// The default alignment (Quake's TextureAxisFromPlane): the world's axis nearest the normal picks the texture's axes.

constexpr double baseAxes[18][3] = {
    {0, 0, 1}, {1, 0, 0}, {0, -1, 0},  // floor
    {0, 0, -1}, {1, 0, 0}, {0, -1, 0}, // ceiling
    {1, 0, 0}, {0, 1, 0}, {0, 0, -1},  // west wall
    {-1, 0, 0}, {0, 1, 0}, {0, 0, -1}, // east wall
    {0, 1, 0}, {1, 0, 0}, {0, 0, -1},  // south wall
    {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, // north wall
};

[[nodiscard]] int baseAxisOf(const glm::dvec3& n)
{
    int best = 0;
    double bestDot = 0.0;
    for(int i = 0; i < 6; i++)
    {
        const double d = n.x * baseAxes[i * 3][0] + n.y * baseAxes[i * 3][1] + n.z * baseAxes[i * 3][2];
        if(d > bestDot)
        {
            bestDot = d;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The model made from a brush: Quake's in-memory brush model, its arrays here (the qmodel_t points into them).

struct Built
{
    za::Vector<mplane_t> planes;
    za::Vector<mclipnode_t> clipnodes[3];
    za::Vector<mnode_t> nodes;
    mleaf_t leafs[2]{}; // solid, empty
    za::Vector<mvertex_t> vertexes;
    za::Vector<medge_t> edges;
    za::Vector<int> surfedges;
    za::Vector<mtexinfo_t> texinfo;
    za::Vector<msurface_t> surfaces;
    za::Vector<int> surfaceFace; // each surface's face in the brush
    za::Vector<texture_t*> textures;
    za::Vector<int> used;
    za::Vector<glm::vec4> clipPlanes; // the faces (hull 0's planes, outward) and the bevels a box swept at it needs
    glm::vec3 origin{0.f};      // the entity's (model space = world - origin)
    glm::vec3 mins{0.f}, maxs{0.f};
    int texofs[TEXTYPE_COUNT + 1]{};
};

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
    za::Vector<Winding> polys;
    if(!b || !polygons(*b, polys))
    {
        return;
    }
    glm::dvec3 lo{1e30}, hi{-1e30};
    for(const Winding& w : polys)
    {
        for(const glm::dvec3& p : w)
        {
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    const glm::dvec3 origin = glm::floor((lo + hi) * 0.5 + 0.5);
    out.origin = glm::vec3{origin};
    out.mins = glm::vec3{lo - origin};
    out.maxs = glm::vec3{hi - origin};

    // The faces with area, model space: hull 0's planes.
    za::Vector<glm::dvec4> faces;
    za::Vector<int> faceOf;
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
    // Hulls 1 and 2: the planes pushed out by the box (its support along each normal), with the bevels a brush that is
    // not a box needs (bevels()); the same set is the narrow box's (clipBox).
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
    }

    // Vertices (shared), edges (shared, Quake's signed surfedges), one texinfo and surface a face.
    const int numFaces = static_cast<int>(faces.size());
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
        v.position[0] = q.x;
        v.position[1] = q.y;
        v.position[2] = q.z;
        out.vertexes.pushBack(v);
        return static_cast<unsigned>(out.vertexes.size() - 1);
    };
    struct FaceDraw
    {
        int firstedge, numedges;
        glm::vec3 mins, maxs;
    };
    za::Vector<FaceDraw> draws;
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

    // The textures (the world's; a name it lacks: its first usable one), the used ones by their type.
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

    // The surfaces (their plane hull 0's: the first numFaces planes) and hull 0's nodes over them.
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

// ---------------------------------------------------------------------------------------------------------------------
// The brushes of the map.

struct Entry
{
    Brush brush;
    int edict{0};
    qmodel_t* model{nullptr};
    za::UniquePtr<Built> built;
    za::UniquePtr<Built> retired; // the one before (a pointer into it kept a frame somewhere is still good)
    za::U32 revision{0};
    bool dirty{false}; // its slot to write (upload)
};

// An edit undone by vr_brush_undo: a brush made (undone: removed), changed or removed (undone: back as it was before).
struct UndoStep
{
    enum class Kind : int
    {
        Made,
        Changed,
        Removed
    } kind{Kind::Made};
    int id{0};
    Brush before;
};
constexpr za::SizeT undoDepth = 64;

// The listen server's brushes by id - 1 (an id: a slot in the brush buffers), the main thread's. Emptied with the world.
struct Registry
{
    za::Vector<za::UniquePtr<Entry>> entries;
    za::Vector<UndoStep> undo; // oldest first, up to undoDepth
    bool undoing{false};       // (an undo's own edits not recorded)
    za::U32 revisions{0};   // the models made (each its own revision)
    bool sweep{false};      // a world began: its brush entities without a brush removed (a saved game's), once
    bool uploadAll{false};  // the map's buffers made again (R_NewMap): every slot written again
    int lastSpawned{0};     // vr_brush_spawn's last thing (vr_brush_ent last)
    char texture[16]{};     // the texture new brushes get
    bool loadedSave{false}; // this world is a saved game's (its brushes from it: no brush file loaded for it)
    const qmodel_t* reserveWorld{nullptr}; // the world whose load made the brush buffers' reserve (R_NewMap)
};
Registry registry;

// A saved game's brush lines, read before its load (VR_ReadSaveInfo) for after it (VR_OnLoadGame); the text a save
// writes (saveLines). The main thread's.
struct SaveState
{
    za::String pending; // the loading save's brushes' text (allToText's)
    bool have{false};
    za::String lines;   // saveLines' last
};
SaveState saveState;

// The world's textures a brush may have (usableTexture), in its order, and its default (the most used plain one), found
// once a world (the panel asks each frame): the main thread's. Emptied at a map change (the world's textures go with
// the hunk; a later world may be in the same slot) and a game directory change.
struct TextureCache
{
    const qmodel_t* world{nullptr};
    za::Vector<texture_t*> usable;
    char defaultName[16]{};
    auto members() { return mem::list(world, usable, defaultName); }
};
mem::Cache<TextureCache> textureCache{"brush textures", mem::MapChange | mem::GameDirChange};

void recordUndo(UndoStep::Kind kind, int id, const Brush* before)
{
    if(registry.undoing)
    {
        return;
    }
    if(registry.undo.size() >= undoDepth)
    {
        registry.undo.erase(registry.undo.begin());
    }
    UndoStep s;
    s.kind = kind;
    s.id = id;
    if(before)
    {
        s.before = *before;
    }
    registry.undo.pushBack(ZA_MOVE(s));
}

[[nodiscard]] Entry* entryOf(int id)
{
    if(id <= 0 || id > static_cast<int>(registry.entries.size()))
    {
        return nullptr;
    }
    return registry.entries[static_cast<za::SizeT>(id - 1)].get();
}

[[nodiscard]] int idFromName(const char* name)
{
    const size_t n = strlen(modelPrefix);
    if(strncmp(name, modelPrefix, n) != 0)
    {
        return 0;
    }
    return atoi(name + n);
}

// How many brushes the map may have at once: its slots (made at its load: vr_brush_max then; before the client has made
// them for this map, as a saved game's brushes are made again, what it will make).
[[nodiscard]] int capacity()
{
    if(cls.state == ca_dedicated || !sv.worldmodel || registry.reserveWorld != sv.worldmodel)
    {
        return R_RuntimeBrushSlotsFor(za::clamp(static_cast<int>(vr_brush_max.value), 0, 4096));
    }
    return R_RuntimeBrushReserve()->slots;
}

// The server's VM while brushes change its edicts (as a command does).
struct ServerVm
{
    qcvm_t* old{nullptr};
    ServerVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~ServerVm() { PR_PopQCVM(old); }
};

// What else keeps things of a model learns that it changed: the ledges (climbing), the held weapons' triangles, the
// decals on its old faces (only those: the world's beside it stay).
void forgetModel(qmodel_t* mod, int id, const glm::vec3& oldLo, const glm::vec3& oldHi)
{
    ledges::forget(mod);
    modelcollide::forget(mod);
    if(oldLo.x <= oldHi.x)
    {
        decals::removeOn(id, oldLo - glm::vec3{2.f}, oldHi + glm::vec3{2.f});
    }
}

// The model of `e` made again from its brush (or empty), in place. `texturesOnly`: only its faces' textures changed (Paint,
// vr_brush_texture): its revision kept and nothing else told (Box3D's hulls, the ledges, the held weapons' triangles,
// the marks on it all stay as they are).
void rebuild(Entry& e, bool empty, bool texturesOnly = false)
{
    glm::vec3 oldLo{1.f}, oldHi{0.f};
    if(e.built && !e.built->surfaces.empty())
    {
        oldLo = e.built->origin + e.built->mins;
        oldHi = e.built->origin + e.built->maxs;
    }
    e.retired = ZA_MOVE(e.built);
    e.built = za::makeUnique<Built>();
    build(empty ? nullptr : &e.brush, *e.built);
    if(!texturesOnly)
    {
        e.revision = ++registry.revisions;
    }
    if(e.model)
    {
        apply(e.model, e.built.get(), e.brush.id);
        if(!texturesOnly)
        {
            forgetModel(e.model, e.brush.id, oldLo, oldHi);
        }
    }
    e.dirty = true;
}

// Whether two brushes have the same planes, in the same order (only their textures may differ).
[[nodiscard]] bool sameShape(const Brush& a, const Brush& b)
{
    if(a.faces.size() != b.faces.size())
    {
        return false;
    }
    for(za::SizeT i = 0; i < a.faces.size(); i++)
    {
        if(a.faces[i].normal != b.faces[i].normal || a.faces[i].dist != b.faces[i].dist)
        {
            return false;
        }
    }
    return true;
}

// The entity of brush `e` placed at its model (made if it has none).
bool placeEntity(Entry& e)
{
    const ServerVm vm;
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "%s%d", modelPrefix, e.brush.id);
    int index = -1;
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            index = i;
            break;
        }
    }
    if(index < 0)
    {
        index = VR_LatePrecacheModel(Hunk_Strdup(name, "qvrbrush")); // (the list keeps the pointer; made by Mod_LoadModel)
    }
    if(index <= 0 || !sv.models[index])
    {
        Con_Printf("brush: no model slot for %s\n", name);
        return false;
    }
    qmodel_t* mod = sv.models[index];
    if(e.model != mod)
    {
        e.model = mod;
        apply(mod, e.built.get(), e.brush.id);
    }
    edict_t* ed = e.edict > 0 ? EDICT_NUM(e.edict) : nullptr;
    if(!ed || ed->free)
    {
        ed = ED_Alloc();
        e.edict = NUM_FOR_EDICT(ed);
        ed->v.classname = PR_SetEngineString(className);
        ed->v.solid = SOLID_BSP;
        ed->v.movetype = MOVETYPE_PUSH;
        ed->v.takedamage = 0.f; // (DAMAGE_NO)
        VR_EdictIndex_Touch(ed); // (the edict index: a classname set in C)
    }
    ed->v.model = PR_SetEngineString(sv.model_precache[index]);
    ed->v.modelindex = static_cast<float>(index);
    for(int i = 0; i < 3; i++)
    {
        ed->v.origin[i] = e.built->origin[i];
        ed->v.mins[i] = mod->clipmins[i];
        ed->v.maxs[i] = mod->clipmaxs[i];
        ed->v.size[i] = mod->clipmaxs[i] - mod->clipmins[i];
        ed->v.angles[i] = 0.f;
        ed->v.velocity[i] = 0.f;
    }
    SV_LinkEdict(ed, false);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The slots: each brush's vertices, indices, draw commands and lightmap texels in the brush buffers' reserve.

// A face's light: the light the map has where it is (the lightmap under the point a little out from its middle, as a
// model there is lit), shaded by its way (vr_brush_shade: the top full, the sides 0.8, the bottom 0.55).
[[nodiscard]] unsigned faceLight(const glm::vec3& centre, const glm::vec3& normal)
{
    vec3_t p{centre.x + normal.x * 4.f, centre.y + normal.y * 4.f, centre.z + normal.z * 4.f};
    lightcache_t cache{};
    int level = R_LightPoint(p, 0.f, &cache);
    glm::vec3 c{lightcolor[0], lightcolor[1], lightcolor[2]};
    if(level <= 0)
    {
        c = glm::vec3{24.f}; // (over nothing lit: the void, the sky)
    }
    const float n = normal.z;
    const float shade = n >= 0.f ? 0.8f + 0.2f * n : 0.8f + 0.25f * n;
    c *= 1.f + (shade - 1.f) * za::clamp(vr_brush_shade.value, 0.f, 1.f);
    const auto channel = [](float v) { return static_cast<unsigned>(za::clamp(v, 0.f, 255.f) + 0.5f); };
    return channel(c.r) | (channel(c.g) << 8) | (channel(c.b) << 16) | 0xff000000u;
}

// Slot `slot` written for entry `e` (null: emptied).
void upload(int slot, Entry* e)
{
    const qvr_brush_reserve_t* r = R_RuntimeBrushReserve();
    if(slot < 0 || slot >= r->slots)
    {
        return;
    }
    if(!e || !e->model || !e->built || e->built->surfaces.empty())
    {
        R_RuntimeBrushUpload(slot, nullptr, 0, nullptr, 0, nullptr, 0);
        if(e && e->model)
        {
            for(int i = 0; i <= TEXTYPE_COUNT; i++)
            {
                e->model->texofs[i] = 0;
            }
        }
        return;
    }
    Built& b = *e->built;
    const int firstVert = r->firstvert + slot * QVR_BRUSH_SLOT_VERTS;
    const int firstIndex = r->firstindex + slot * QVR_BRUSH_SLOT_INDEXES;
    za::InPlaceVector<glvert_t, QVR_BRUSH_SLOT_VERTS> verts;
    za::InPlaceVector<unsigned, QVR_BRUSH_SLOT_INDEXES> indexes;
    za::InPlaceVector<bmodel_draw_indirect_t, QVR_BRUSH_SLOT_CMDS> cmds;
    za::InPlaceVector<unsigned, QVR_BRUSH_SLOT_LUXELS> texels, lux;
    const float lmS = (static_cast<float>(r->lmx + (slot % QVR_BRUSH_LUXEL_ROW) * QVR_BRUSH_SLOT_LUXELS) + 0.5f) / static_cast<float>(r->lmwidth);
    const float lmT = (static_cast<float>(r->lmy + slot / QVR_BRUSH_LUXEL_ROW) + 0.5f) / static_cast<float>(r->lmheight);
    const int numSurfaces = za::min(static_cast<int>(b.surfaces.size()), QVR_BRUSH_SLOT_LUXELS);
    // The vertices a face at a time; the indices a texture at a time (its command's range).
    for(int k = 0; k < numSurfaces; k++)
    {
        msurface_t& s = b.surfaces[static_cast<za::SizeT>(k)];
        if(static_cast<int>(verts.size()) + s.numedges > QVR_BRUSH_SLOT_VERTS)
        {
            s.vbo_firstvert = -1;
            continue;
        }
        s.vbo_firstvert = firstVert + static_cast<int>(verts.size());
        const texture_t* t = b.textures[static_cast<za::SizeT>(s.texinfo->texnum)];
        const float tw = t && t->width ? static_cast<float>(t->width) : 64.f, th = t && t->height ? static_cast<float>(t->height) : 64.f;
        glm::vec3 centre{0.f};
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            centre += glm::vec3{v[0], v[1], v[2]} / static_cast<float>(s.numedges);
        }
        for(int i = 0; i < s.numedges; i++)
        {
            const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
            const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
            glvert_t gv{};
            VectorCopy(v, gv.pos);
            float sc = (DotProduct(v, s.texinfo->vecs[0]) + s.texinfo->vecs[0][3]) / tw;
            float tc = (DotProduct(v, s.texinfo->vecs[1]) + s.texinfo->vecs[1][3]) / th;
            if(t && t->shift > 0) // (Q64's texture shift, as GL_BuildBModelVertexBuffer)
            {
                sc /= static_cast<float>(2 * t->shift);
                tc /= static_cast<float>(2 * t->shift);
            }
            gv.st[0] = sc;
            gv.st[1] = tc;
            gv.st[2] = lmS + static_cast<float>(k) / static_cast<float>(r->lmwidth);
            gv.st[3] = lmT;
            gv.lmofs = 0.f;
            gv.styles = 0u | (255u << 8) | (255u << 16) | (255u << 24);
            gv.swellpin = 0.f;
            gv.centre[0] = centre.x;
            gv.centre[1] = centre.y;
            gv.centre[2] = centre.z;
            verts.pushBack(gv);
        }
        glm::vec3 normal{s.plane->normal[0], s.plane->normal[1], s.plane->normal[2]};
        if(s.flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        texels.pushBack(faceLight(centre + b.origin, normal));
        lux.pushBack(128u | (128u << 8) | (255u << 16) | 0xff000000u); // (straight on)
    }
    for(int j = 0; j < static_cast<int>(b.used.size()) && j < QVR_BRUSH_SLOT_CMDS; j++)
    {
        bmodel_draw_indirect_t cmd{};
        cmd.firstIndex = static_cast<GLuint>(firstIndex + static_cast<int>(indexes.size()));
        cmd.instanceCount = 1;
        for(int k = 0; k < numSurfaces; k++)
        {
            const msurface_t& s = b.surfaces[static_cast<za::SizeT>(k)];
            if(s.texinfo->texnum != b.used[static_cast<za::SizeT>(j)] || s.vbo_firstvert < 0)
            {
                continue;
            }
            for(int i = 2; i < s.numedges && indexes.size() + 3 <= indexes.capacity(); i++)
            {
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert));
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert + i - 1));
                indexes.pushBack(static_cast<unsigned>(s.vbo_firstvert + i));
            }
        }
        cmd.count = static_cast<GLuint>(firstIndex + static_cast<int>(indexes.size())) - cmd.firstIndex;
        cmds.pushBack(cmd);
    }
    R_RuntimeBrushUpload(slot, verts.data(), static_cast<int>(verts.size()), indexes.data(), static_cast<int>(indexes.size()),
        cmds.data(), static_cast<int>(cmds.size()));
    R_RuntimeBrushLightmap(slot, texels.data(), lux.data(), static_cast<int>(texels.size()));
    if(developer.value)
    {
        Con_Printf("brush %d: slot %d, %d corners, %d indices, %d textures, light %06x %06x %06x %06x %06x %06x\n", e->brush.id,
            slot, static_cast<int>(verts.size()), static_cast<int>(indexes.size()), static_cast<int>(cmds.size()),
            texels.size() > 0 ? texels[0] & 0xffffff : 0, texels.size() > 1 ? texels[1] & 0xffffff : 0,
            texels.size() > 2 ? texels[2] & 0xffffff : 0, texels.size() > 3 ? texels[3] & 0xffffff : 0,
            texels.size() > 4 ? texels[4] & 0xffffff : 0, texels.size() > 5 ? texels[5] & 0xffffff : 0);
    }
    qmodel_t* mod = e->model;
    mod->firstcmd = r->firstcmd + slot * QVR_BRUSH_SLOT_CMDS;
    const int drawn = za::min(static_cast<int>(b.used.size()), QVR_BRUSH_SLOT_CMDS);
    for(int i = 0; i <= TEXTYPE_COUNT; i++)
    {
        mod->texofs[i] = za::min(b.texofs[i], drawn);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The text.

void appendNumber(za::String& out, double v)
{
    char buf[48];
    if(za::fabs(v - za::round(v)) < 1e-9 && za::fabs(v) < 1e15)
    {
        q_snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(za::round(v)));
    }
    else
    {
        q_snprintf(buf, sizeof(buf), "%.12g", v);
    }
    out += buf;
}

// The three points written for a face: on its polygon (clockwise seen from outside: qbsp's plane from points gives
// the outward normal), the corners spanning the largest triangle; a face with no polygon: three on its plane.
void facePoints(const Face& f, const Winding& w, glm::dvec3 (&p)[3])
{
    if(w.size() >= 3)
    {
        double best = -1.0;
        for(za::SizeT b = 1; b + 1 < w.size(); b++)
        {
            for(za::SizeT c = b + 1; c < w.size(); c++)
            {
                const double area = glm::length(glm::cross(w[b] - w[0], w[c] - w[0]));
                if(area > best)
                {
                    best = area;
                    p[0] = w[0];
                    p[1] = w[b];
                    p[2] = w[c];
                }
            }
        }
        return;
    }
    const Winding base = baseWinding(f.normal, f.dist);
    p[0] = base[0] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
    p[1] = base[1] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
    p[2] = base[2] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
}

// The text's tokens: words, numbers, ( ) [ ] { }; // comments skipped (the header read before).
struct Tokens
{
    za::StringView text;
    za::SizeT at{0};
    char token[64]{};

    bool next()
    {
        while(at < text.size())
        {
            const char c = text[at];
            if(c == '/' && at + 1 < text.size() && text[at + 1] == '/')
            {
                while(at < text.size() && text[at] != '\n')
                {
                    at++;
                }
                continue;
            }
            if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                at++;
                continue;
            }
            break;
        }
        if(at >= text.size())
        {
            token[0] = '\0';
            return false;
        }
        const char c = text[at];
        if(c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}')
        {
            token[0] = c;
            token[1] = '\0';
            at++;
            return true;
        }
        za::SizeT n = 0;
        while(at < text.size() && n + 1 < sizeof(token))
        {
            const char d = text[at];
            if(d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == '(' || d == ')' || d == '[' || d == ']' ||
                d == '{' || d == '}')
            {
                break;
            }
            token[n++] = d;
            at++;
        }
        token[n] = '\0';
        return true;
    }
    bool expect(const char* s) { return next() && !strcmp(token, s); }
    bool number(double& v)
    {
        if(!next())
        {
            return false;
        }
        char* end = nullptr;
        v = strtod(token, &end);
        return end && *end == '\0' && end != token;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

bool parseVec(int first, glm::dvec3& v)
{
    if(Cmd_Argc() < first + 3)
    {
        return false;
    }
    for(int i = 0; i < 3; i++)
    {
        v[i] = Q_atof(Cmd_Argv(first + i));
    }
    return true;
}

[[nodiscard]] bool serverReady()
{
    if(!sv.active || !sv.worldmodel)
    {
        Con_Printf("brush: no local game\n");
        return false;
    }
    return true;
}

// vr_brush_box <x y z> <X Y Z> [texture]: a box from corner to corner.
void box_f()
{
    glm::dvec3 a, c;
    if(!parseVec(1, a) || !parseVec(4, c))
    {
        Con_Printf("usage: vr_brush_box <x y z> <X Y Z> [texture]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    const int id = create(box(glm::min(a, c), glm::max(a, c), Cmd_Argc() > 7 ? Cmd_Argv(7) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d (entity %d)\n", id, entityOf(id));
    }
}

// vr_brush_here [size] [height] [texture] [distance]: a box `size` wide (64) and `height` tall (size) on the floor
// ahead of the player, its middle `distance` away (half its size and 48 more).
void here_f()
{
    if(!serverReady())
    {
        return;
    }
    const double size = Cmd_Argc() > 1 ? za::max(Q_atof(Cmd_Argv(1)), 1.0f) : 64.0;
    const double height = Cmd_Argc() > 2 ? za::max(Q_atof(Cmd_Argv(2)), 1.0f) : size;
    const double distance = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : size * 0.5 + 48.0;
    const ServerVm vm;
    edict_t* player = EDICT_NUM(1);
    const double yaw = glm::radians(static_cast<double>(player->v.angles[1]));
    const glm::dvec3 fwd{za::cos(yaw), za::sin(yaw), 0.0};
    glm::dvec3 centre = glm::dvec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]} + fwd * distance;
    vec3_t start{static_cast<float>(centre.x), static_cast<float>(centre.y), static_cast<float>(centre.z)};
    vec3_t end{start[0], start[1], start[2] - 512.f};
    const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, player);
    const double floorZ = tr.fraction < 1.f && tr.plane.normal[2] > 0.999f ? tr.plane.dist : tr.endpos[2]; // (the floor's plane: on it)
    centre.x = za::round(centre.x / 8.0) * 8.0;
    centre.y = za::round(centre.y / 8.0) * 8.0;
    const glm::dvec3 lo{centre.x - size * 0.5, centre.y - size * 0.5, za::round(floorZ)};
    const int id = create(box(lo, lo + glm::dvec3{size, size, height}, Cmd_Argc() > 3 ? Cmd_Argv(3) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d (entity %d) at %.0f %.0f %.0f\n", id, entityOf(id), lo.x, lo.y, lo.z);
    }
}

[[nodiscard]] int idArg(int i)
{
    const int id = Cmd_Argc() > i ? Q_atoi(Cmd_Argv(i)) : 0;
    if(!find(id))
    {
        Con_Printf("brush: no brush %s\n", Cmd_Argc() > i ? Cmd_Argv(i) : "(none given)");
        return 0;
    }
    return id;
}

// vr_brush_set <id> <x y z> <X Y Z>: its box (a box brush) changed.
void set_f()
{
    glm::dvec3 a, c;
    const int id = idArg(1);
    if(!id || !parseVec(2, a) || !parseVec(5, c))
    {
        if(id)
        {
            Con_Printf("usage: vr_brush_set <id> <x y z> <X Y Z>\n");
        }
        return;
    }
    // (any shape: stretched from its bounds to these)
    Brush b = *find(id);
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return;
    }
    scaleTo(b, lo, hi, glm::min(a, c), glm::max(a, c));
    if(change(id, b))
    {
        Con_Printf("brush: %d is %.0f %.0f %.0f .. %.0f %.0f %.0f\n", id, a.x, a.y, a.z, c.x, c.y, c.z);
    }
}

// vr_brush_move <id> <dx dy dz>
void move_f()
{
    glm::dvec3 d;
    const int id = idArg(1);
    if(!id || !parseVec(2, d))
    {
        return;
    }
    Brush b = *find(id);
    translate(b, d);
    if(change(id, b))
    {
        Con_Printf("brush: %d moved %.0f %.0f %.0f\n", id, d.x, d.y, d.z);
    }
}

// vr_brush_texture <id> <texture> [face]: every face's (or one's) texture.
void texture_f()
{
    const int id = idArg(1);
    if(!id || Cmd_Argc() < 3)
    {
        return;
    }
    if(!worldTexture(Cmd_Argv(2)) || !usableTexture(worldTexture(Cmd_Argv(2))))
    {
        Con_Printf("brush: the map has no texture %s a brush can have\n", Cmd_Argv(2));
        return;
    }
    Brush b = *find(id);
    const int face = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : -1;
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        if(face < 0 || face == static_cast<int>(i))
        {
            q_strlcpy(b.faces[i].texture, Cmd_Argv(2), sizeof(b.faces[i].texture));
        }
    }
    if(change(id, b))
    {
        Con_Printf("brush: %d textured %s\n", id, Cmd_Argv(2));
    }
}

// vr_brush_delete <id | all>
void delete_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "all"))
    {
        int n = 0;
        for(za::SizeT i = 0; i < registry.entries.size(); i++)
        {
            if(registry.entries[i] && remove(static_cast<int>(i) + 1))
            {
                n++;
            }
        }
        Con_Printf("brush: removed %d\n", n);
        return;
    }
    const int id = idArg(1);
    if(id && remove(id))
    {
        Con_Printf("brush: removed %d\n", id);
    }
}

// vr_brush_duplicate <id> <dx dy dz>
void duplicate_f()
{
    glm::dvec3 d;
    const int id = idArg(1);
    if(!id || !parseVec(2, d))
    {
        return;
    }
    Brush b = *find(id);
    translate(b, d);
    const int made = create(b);
    if(made)
    {
        Con_Printf("brush: %d copied as %d (entity %d)\n", id, made, entityOf(made));
    }
}

void list_f()
{
    Con_Printf("brushes: %d of %d (vr_brush_max %d at the map's load)\n", count(), capacity(), static_cast<int>(vr_brush_max.value));
    forEach([](const Brush& b) {
        glm::dvec3 lo, hi;
        const bool ok = bounds(b, lo, hi);
        const Entry* e = entryOf(b.id);
        const int marks = decals::countOn(b.id, glm::vec3{lo} - glm::vec3{2.f}, glm::vec3{hi} + glm::vec3{2.f});
        Con_Printf("  %d: entity %d, rev %u, %d faces, %s, %.0f %.0f %.0f .. %.0f %.0f %.0f, %d decals%s\n", b.id, e ? e->edict : 0,
            b.rev, static_cast<int>(b.faces.size()), b.faces.empty() ? "-" : b.faces[0].texture, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z,
            marks, ok ? "" : " (no volume)");
    });
}

// vr_brush_text <id>: its .map text.
void text_f()
{
    const int id = idArg(1);
    if(!id)
    {
        return;
    }
    za::String text;
    toText(*find(id), text);
    Con_Printf("%s", text.cStr());
}

// vr_brush_spawn <classname> <x y z> [yaw] [drop] (tests): a thing made there by its spawn function (as the toolgun's
// Spawn tool: a monster on a box); "drop": a prop let fall from there (its bottom at z: a crate dropped onto a box).
void spawn_f()
{
    glm::dvec3 at;
    if(Cmd_Argc() < 5 || !parseVec(2, at))
    {
        Con_Printf("usage: vr_brush_spawn <classname> <x y z> [yaw]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    const float yaw = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 0.f;
    edict_t* e = box3d::spawnClass(Cmd_Argv(1), glm::vec3{at}, glm::vec3{0.f, yaw, 0.f}, nullptr, nullptr, 0.f);
    if(e)
    {
        const ServerVm vm;
        registry.lastSpawned = NUM_FOR_EDICT(e);
        Con_Printf("brush: spawned %d %s\n", registry.lastSpawned, Cmd_Argv(1));
        if(Cmd_Argc() > 6 && !q_strcasecmp(Cmd_Argv(6), "drop"))
        {
            // (a prop made there lies still, asleep, where its placing put it: held there awake, its bottom at z, it falls)
            Cbuf_InsertText(va("vr_physics_stack %d 1 %g %g %g\n", registry.lastSpawned, at.x, at.y, at.z));
        }
    }
}

// vr_brush_ent <entity> (tests): where it is, what it stands on (a brush's: its id) and whether it lives.
void ent_f()
{
    if(!serverReady() || Cmd_Argc() < 2)
    {
        return;
    }
    const ServerVm vm;
    const int num = !q_strcasecmp(Cmd_Argv(1), "last") ? registry.lastSpawned : Q_atoi(Cmd_Argv(1));
    if(num <= 0 || num >= qcvm->num_edicts)
    {
        Con_Printf("brush ent: no entity %d\n", num);
        return;
    }
    edict_t* e = EDICT_NUM(num);
    const int ground = (static_cast<int>(e->v.flags) & FL_ONGROUND) ? NUM_FOR_EDICT(PROG_TO_EDICT(e->v.groundentity)) : -1;
    Con_Printf("brush ent: %d %s at %.1f %.1f %.1f, %s, ground %d%s, health %.0f, enemy %d\n", num,
        e->free ? "(free)" : PR_GetString(e->v.classname), e->v.origin[0], e->v.origin[1], e->v.origin[2],
        (static_cast<int>(e->v.flags) & FL_ONGROUND) ? "on ground" : "in the air", ground,
        ground > 0 && ofEntity(ground) ? va(" (brush %d)", ofEntity(ground)) : "", e->v.health,
        e->v.enemy ? NUM_FOR_EDICT(PROG_TO_EDICT(e->v.enemy)) : 0);
}

// vr_brush_trace <x y z> <X Y Z> [size] (tests): Quake's trace from one point to the other (a point, or a box `size`
// across: 32 the player's hull 1), as QC's traceline with nomonsters: where it stops and on what.
void trace_f()
{
    glm::dvec3 a, c;
    if(!parseVec(1, a) || !parseVec(4, c) || !serverReady())
    {
        Con_Printf("usage: vr_brush_trace <x y z> <X Y Z> [size]\n");
        return;
    }
    const ServerVm vm;
    const float half = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) * 0.5f : 0.f;
    vec3_t start{static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)};
    vec3_t end{static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z)};
    vec3_t mins{-half, -half, half > 0.f ? -24.f : 0.f}, maxs{half, half, half > 0.f ? 32.f : 0.f};
    const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NOMONSTERS, EDICT_NUM(1)); // (not the world: what it owns, all, would be passed by)
    const int num = tr.ent ? NUM_FOR_EDICT(tr.ent) : -1;
    Con_Printf("brush trace: fraction %.3f, end %.1f %.1f %.1f, normal %.2f %.2f %.2f, entity %d%s%s%s\n", tr.fraction,
        tr.endpos[0], tr.endpos[1], tr.endpos[2], tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2], num,
        num > 0 && ofEntity(num) ? va(" (brush %d)", ofEntity(num)) : "", tr.startsolid ? ", start solid" : "",
        tr.allsolid ? ", all solid" : "");
}

void textures_f()
{
    Con_Printf("brush textures: %d (current %s)\n", textureCount(), currentTexture());
    for(int i = 0; i < textureCount(); i++)
    {
        const texture_t* t = textureAt(i);
        Con_Printf("  %d: %s %ux%u%s\n", i, t->name, t->width, t->height, t->type == TEXTYPE_CUTOUT ? " (fence)" : "");
    }
}

// vr_brush_shape <box | wedge | cylinder | pyramid> <x y z> <X Y Z> [sides] [up] [rise] [texture]: a shape filling the box
// from corner to corner; `up` and `rise` an axis with its way (z, -z, x, ...: the default z, the wedge rising along x).
[[nodiscard]] bool parseAxis(const char* s, int& axis, double& sign)
{
    sign = s[0] == '-' ? -1.0 : 1.0;
    const char c = s[s[0] == '-' || s[0] == '+' ? 1 : 0];
    axis = c == 'x' || c == 'X' ? 0 : c == 'y' || c == 'Y' ? 1 : c == 'z' || c == 'Z' ? 2 : -1;
    return axis >= 0;
}

void shape_f()
{
    glm::dvec3 a, c;
    if(Cmd_Argc() < 8 || !parseVec(2, a) || !parseVec(5, c))
    {
        Con_Printf("usage: vr_brush_shape <box | wedge | cylinder | pyramid> <x y z> <X Y Z> [sides] [up] [rise] [texture]\n");
        return;
    }
    if(!serverReady())
    {
        return;
    }
    int up = 2, rise = 0;
    double upSign = 1.0, riseSign = 1.0;
    if(Cmd_Argc() > 9 && !parseAxis(Cmd_Argv(9), up, upSign))
    {
        up = 2;
    }
    if(Cmd_Argc() > 10 && !parseAxis(Cmd_Argv(10), rise, riseSign))
    {
        rise = 0;
    }
    const Shape s = shapeFromName(Cmd_Argv(1));
    const int sides = Cmd_Argc() > 8 ? Q_atoi(Cmd_Argv(8)) : 8;
    const int id = create(shape(s, glm::min(a, c), glm::max(a, c), up, upSign, rise, riseSign, sides,
        Cmd_Argc() > 11 ? Cmd_Argv(11) : currentTexture()));
    if(id)
    {
        Con_Printf("brush: made %d, a %s of %d faces (entity %d)\n", id, shapeName(s), static_cast<int>(find(id)->faces.size()),
            entityOf(id));
    }
}

// vr_brush_rotate <id> <pitch yaw roll> [lock]: turned about its bounds' middle (lock 0: the textures stay the world's).
void rotate_f()
{
    glm::dvec3 ang;
    const int id = idArg(1);
    if(!id || !parseVec(2, ang))
    {
        return;
    }
    Brush b = *find(id);
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return;
    }
    const glm::dmat3 r = turnFromAngles(ang);
    rotate(b, r, glm::floor((lo + hi) * 0.5 + 0.5), Cmd_Argc() > 5 ? Q_atoi(Cmd_Argv(5)) != 0 : true);
    if(change(id, b))
    {
        glm::dvec3 l2, h2;
        (void)bounds(b, l2, h2);
        Con_Printf("brush: %d turned %.0f %.0f %.0f, %.1f %.1f %.1f .. %.1f %.1f %.1f\n", id, ang.x, ang.y, ang.z, l2.x, l2.y, l2.z,
            h2.x, h2.y, h2.z);
    }
}

void undo_f()
{
    if(serverReady())
    {
        (void)undo();
    }
}

// vr_brush_align <id> <face | all> <reset | shift du dv | scale su sv | turn degrees>: a face's texture alignment.
void align_f()
{
    const int id = idArg(1);
    if(!id || Cmd_Argc() < 4)
    {
        if(id)
        {
            Con_Printf("usage: vr_brush_align <id> <face | all> <reset | shift du dv | scale su sv | turn degrees>\n");
        }
        return;
    }
    Brush b = *find(id);
    const bool all = !q_strcasecmp(Cmd_Argv(2), "all");
    const int face = all ? -1 : Q_atoi(Cmd_Argv(2));
    const char* op = Cmd_Argv(3);
    const double x = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 0.0, y = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : x;
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        if(!all && face != static_cast<int>(i))
        {
            continue;
        }
        Face& f = b.faces[i];
        if(!q_strcasecmp(op, "reset"))
        {
            defaultAlignment(f);
        }
        else if(!q_strcasecmp(op, "shift"))
        {
            f.uOffset += x;
            f.vOffset += y;
        }
        else if(!q_strcasecmp(op, "scale") && x != 0.0 && y != 0.0)
        {
            f.scaleU = x;
            f.scaleV = y;
        }
        else if(!q_strcasecmp(op, "turn"))
        {
            // The axes turned about the face's normal (Valve 220's rotation: the number kept with them).
            const glm::dmat3 r = glm::mat3_cast(glm::angleAxis(glm::radians(x), f.normal));
            f.uAxis = r * f.uAxis;
            f.vAxis = r * f.vAxis;
            f.rotation = za::remainder(f.rotation + x, 360.0);
        }
    }
    if(change(id, b))
    {
        Con_Printf("brush: %d aligned (%s %s)\n", id, Cmd_Argv(2), op);
    }
}

[[nodiscard]] za::String brushFile(const char* name); // (below)

// vr_brush_save [name]: this map's brushes to <game folder>/brushes/<map>[-name].brushes.
void save_f()
{
    if(!serverReady())
    {
        return;
    }
    const za::String path = brushFile(Cmd_Argc() > 1 ? Cmd_Argv(1) : "");
    za::String text;
    allToText(text);
    (void)files::createDirectories(za::String{files::parentPath(path)}.cStr());
    if(!files::writeText(path.cStr(), text))
    {
        Con_Printf("brush: couldn't write %s\n", path.cStr());
        return;
    }
    Con_Printf("brush: %d brushes saved to %s\n", count(), path.cStr());
}

// vr_brush_load [name] [add]: the brushes of <game folder>/brushes/<map>[-name].brushes made (the map's removed first,
// unless "add").
void load_f()
{
    if(!serverReady())
    {
        return;
    }
    const char* name = Cmd_Argc() > 1 && q_strcasecmp(Cmd_Argv(1), "add") ? Cmd_Argv(1) : "";
    const bool add = !q_strcasecmp(Cmd_Argv(Cmd_Argc() - 1), "add");
    const za::String path = brushFile(name);
    za::String text, warning;
    if(!files::readText(path.cStr(), text))
    {
        Con_Printf("brush: no brush file %s\n", path.cStr());
        return;
    }
    const int made = fromAllText(text, !add, warning);
    Con_Printf("brush: %d brushes made from %s%s%s\n", made, path.cStr(), warning.empty() ? "" : ": ", warning.cStr());
}

// vr_brush_texture_set <name>: the texture new brushes get.
void textureSet_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("brush: new brushes get %s\n", currentTexture());
        return;
    }
    setCurrentTexture(Cmd_Argv(1));
    Con_Printf("brush: new brushes get %s\n", currentTexture());
}

// The brushes' own check (vr_brush_selftest): the text of a box, a wedge (a sloped face) and a six-sided prism made and
// read back, their planes, textures and alignment the same; their models' hulls tested at points in and out (hull 0
// at the faces, hull 1 a box's reach further out); the made brushes' own texts too.
[[nodiscard]] bool samePlanes(const Brush& a, const Brush& b, za::String& why)
{
    if(a.faces.size() != b.faces.size())
    {
        why = va("%d faces read back as %d", static_cast<int>(a.faces.size()), static_cast<int>(b.faces.size()));
        return false;
    }
    for(za::SizeT i = 0; i < a.faces.size(); i++)
    {
        const Face& f = a.faces[i];
        const Face& g = b.faces[i];
        if(glm::length(f.normal - g.normal) > 1e-6 || za::fabs(f.dist - g.dist) > 1e-4 || strcmp(f.texture, g.texture) != 0 ||
            glm::length(f.uAxis - g.uAxis) > 1e-6 || glm::length(f.vAxis - g.vAxis) > 1e-6 || za::fabs(f.uOffset - g.uOffset) > 1e-6 ||
            za::fabs(f.vOffset - g.vOffset) > 1e-6 || za::fabs(f.scaleU - g.scaleU) > 1e-6 || za::fabs(f.scaleV - g.scaleV) > 1e-6)
        {
            why = va("face %d: %.3f %.3f %.3f %.3f %s read back as %.3f %.3f %.3f %.3f %s", static_cast<int>(i), f.normal.x, f.normal.y,
                f.normal.z, f.dist, f.texture, g.normal.x, g.normal.y, g.normal.z, g.dist, g.texture);
            return false;
        }
    }
    return true;
}

[[nodiscard]] int hullContents(const qmodel_t* m, int h, const glm::vec3& p)
{
    vec3_t q{p.x, p.y, p.z};
    return SV_HullPointContents(const_cast<hull_t*>(&m->hulls[h]), m->hulls[h].firstclipnode, q);
}

void selftest_f()
{
    int failed = 0, checked = 0;
    const auto check = [&](bool ok, const char* what) {
        checked++;
        if(!ok)
        {
            failed++;
            Con_Printf("brush selftest: FAIL %s\n", what);
        }
    };
    const char* tex = textureCount() > 0 ? textureAt(0)->name : "base";
    za::Vector<Brush> samples;
    samples.pushBack(box(glm::dvec3{-32.0, 16.0, 0.0}, glm::dvec3{48.0, 80.0, 72.0}, tex));
    {
        // A wedge: a box whose top slopes (its normal 45 degrees up), offsets and a scale on its faces.
        Brush w = box(glm::dvec3{0.0}, glm::dvec3{64.0, 64.0, 64.0}, tex);
        for(Face& f : w.faces)
        {
            if(f.normal.z > 0.5)
            {
                f.normal = glm::normalize(glm::dvec3{1.0, 0.0, 1.0});
                f.dist = glm::dot(f.normal, glm::dvec3{0.0, 0.0, 64.0});
                defaultAlignment(f);
                f.uOffset = 8.0;
                f.vOffset = -16.0;
                f.scaleU = 0.5;
            }
        }
        samples.pushBack(ZA_MOVE(w));
    }
    {
        // A six-sided prism (hexagonal) 48 tall.
        Brush p;
        for(int k = 0; k < 6; k++)
        {
            Face f;
            const double a = glm::radians(60.0 * k);
            f.normal = glm::dvec3{za::cos(a), za::sin(a), 0.0};
            f.dist = 40.0;
            q_strlcpy(f.texture, tex, sizeof(f.texture));
            defaultAlignment(f);
            p.faces.pushBack(f);
        }
        for(const double z : {1.0, -1.0})
        {
            Face f;
            f.normal = glm::dvec3{0.0, 0.0, z};
            f.dist = z > 0.0 ? 48.0 : 0.0;
            q_strlcpy(f.texture, tex, sizeof(f.texture));
            defaultAlignment(f);
            p.faces.pushBack(f);
        }
        samples.pushBack(ZA_MOVE(p));
    }
    // The tool's shapes, and a box turned 45 degrees about z (its hull 1 checked at its corner below: the bevels).
    const glm::dvec3 slo{0.0, 0.0, 0.0}, shi{96.0, 64.0, 48.0};
    for(const Shape s : {Shape::Wedge, Shape::Cylinder, Shape::Pyramid})
    {
        samples.pushBack(shape(s, slo, shi, 2, 1.0, 0, 1.0, s == Shape::Pyramid ? 4 : 8, tex));
    }
    const za::SizeT turnedIndex = samples.size();
    {
        Brush t = box(glm::dvec3{-32.0, -32.0, 0.0}, glm::dvec3{32.0, 32.0, 64.0}, tex);
        rotate(t, turnFromAngles(glm::dvec3{0.0, 45.0, 0.0}), glm::dvec3{0.0, 0.0, 32.0}, true);
        samples.pushBack(ZA_MOVE(t));
        // A box turned 90 degrees is a box again (its planes axial, whole).
        Brush q = box(glm::dvec3{0.0}, glm::dvec3{64.0, 32.0, 16.0}, tex);
        rotate(q, turnFromAngles(glm::dvec3{0.0, 90.0, 0.0}), glm::dvec3{32.0, 16.0, 8.0}, true);
        glm::dvec3 qlo, qhi;
        check(axial(q) && bounds(q, qlo, qhi) && qlo == glm::dvec3{16.0, -16.0, 0.0} && qhi == glm::dvec3{48.0, 48.0, 16.0},
            "a box turned 90 degrees: axial, 16 -16 0 .. 48 48 16");
        // Stretched to other bounds and back: the same planes.
        Brush w = shape(Shape::Wedge, slo, shi, 2, 1.0, 0, 1.0, 8, tex);
        Brush w2 = w;
        scaleTo(w2, slo, shi, glm::dvec3{8.0, 8.0, 8.0}, glm::dvec3{200.0, 40.0, 120.0});
        scaleTo(w2, glm::dvec3{8.0, 8.0, 8.0}, glm::dvec3{200.0, 40.0, 120.0}, slo, shi);
        za::String why;
        check(samePlanes(w, w2, why), va("a wedge stretched and back: %s", why.cStr()));
    }
    forEach([&](const Brush& b) { samples.pushBack(b); });
    for(za::SizeT i = 0; i < samples.size(); i++)
    {
        const Brush& b = samples[i];
        za::String text, error, why;
        toText(b, text);
        Brush back;
        const bool parsed = fromText(text, back, error);
        check(parsed, va("sample %d: not read back (%s)", static_cast<int>(i), error.cStr()));
        if(parsed)
        {
            check(samePlanes(b, back, why), va("sample %d: %s", static_cast<int>(i), why.cStr()));
            // Written again: the same text (a brush of whole planes), or the same planes read back once more (a sloped
            // face's points have digits a reading moves in their last place).
            za::String again, why2;
            toText(back, again);
            Brush back2;
            za::String error2;
            check(axial(b) ? again == text : fromText(again, back2, error2) && samePlanes(back, back2, why2),
                va("sample %d: its text written again differs %s", static_cast<int>(i), why2.cStr()));
        }
        check(invalid(b) == nullptr, va("sample %d: %s", static_cast<int>(i), invalid(b) ? invalid(b) : ""));
        // Its model: inside solid, outside empty, hull 1 grown by the player's box.
        Built built;
        build(&b, built);
        qmodel_t m{};
        apply(&m, &built, 0);
        // (its corners' average: inside a convex brush; the box tests on the flat-topped ones, where a level top is)
        glm::vec3 mid{0.f};
        for(const mvertex_t& v : built.vertexes)
        {
            mid += glm::vec3{v.position[0], v.position[1], v.position[2]} / static_cast<float>(built.vertexes.size());
        }
        check(hullContents(&m, 0, mid) == CONTENTS_SOLID, va("sample %d: hull 0 empty at its middle", static_cast<int>(i)));
        bool flatTop = false;
        for(const Face& f : b.faces)
        {
            flatTop = flatTop || f.normal.z > 1.0 - 1e-9;
        }
        if(flatTop)
        {
            const float top = built.maxs.z, bottom = built.mins.z;
            const struct
            {
                int hull;
                glm::vec3 at;
                int contents;
            } probes[] = {
                {0, {mid.x, mid.y, top + 1.f}, CONTENTS_EMPTY},
                {1, {mid.x, mid.y, top + 23.f}, CONTENTS_SOLID},  // (hull 1's box: 24 under its origin)
                {1, {mid.x, mid.y, top + 25.f}, CONTENTS_EMPTY},
                {1, {mid.x, mid.y, bottom - 31.f}, CONTENTS_SOLID}, // (32 over it)
                {1, {mid.x, mid.y, bottom - 33.f}, CONTENTS_EMPTY},
                {1, {built.maxs.x + 15.f, mid.y, mid.z}, CONTENTS_SOLID}, // (16 aside)
                {1, {built.maxs.x + 17.f, mid.y, mid.z}, CONTENTS_EMPTY},
                {2, {built.maxs.x + 31.f, mid.y, mid.z}, CONTENTS_SOLID}, // (hull 2: 32 aside)
                {2, {built.maxs.x + 33.f, mid.y, mid.z}, CONTENTS_EMPTY},
                {2, {mid.x, mid.y, bottom - 63.f}, CONTENTS_SOLID}, // (64 over it)
            };
            for(const auto& p : probes)
            {
                check(hullContents(&m, p.hull, p.at) == p.contents,
                    va("sample %d: hull %d %s at %.0f %.0f %.0f", static_cast<int>(i), p.hull,
                        p.contents == CONTENTS_SOLID ? "empty" : "solid", p.at.x, p.at.y, p.at.z));
            }
        }
        if(i == turnedIndex)
        {
            // The turned box's corner on +x is at x 45.25 (32 * sqrt 2): the player's box (16 aside) meets it 16 further,
            // not 32 (as its two faces' planes pushed out would meet without the bevel).
            const float cx = 32.f * 1.41421356f - built.origin.x;
            const float cz = 32.f - built.origin.z;
            check(hullContents(&m, 1, glm::vec3{cx + 15.f, -built.origin.y, cz}) == CONTENTS_SOLID, "turned box: hull 1 solid 15 off its corner");
            check(hullContents(&m, 1, glm::vec3{cx + 17.f, -built.origin.y, cz}) == CONTENTS_EMPTY, "turned box: hull 1 empty 17 off its corner (the bevel)");
            check(built.clipPlanes.size() >= 8, "turned box: its bevels");
        }
        if(i == 0)
        {
            Con_Printf("%s", text.cStr());
        }
    }
    Con_Printf("brush selftest: %s (%d checks, %d failed, %d brushes)\n", failed ? "FAIL" : "PASS", checked, failed,
        static_cast<int>(samples.size()));
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// The brush.

void defaultAlignment(Face& f)
{
    const int i = baseAxisOf(f.normal);
    f.uAxis = glm::dvec3{baseAxes[i * 3 + 1][0], baseAxes[i * 3 + 1][1], baseAxes[i * 3 + 1][2]};
    f.vAxis = glm::dvec3{baseAxes[i * 3 + 2][0], baseAxes[i * 3 + 2][1], baseAxes[i * 3 + 2][2]};
    f.uOffset = f.vOffset = 0.0;
    f.rotation = 0.0;
    f.scaleU = f.scaleV = 1.0;
}

Brush box(const glm::dvec3& lo, const glm::dvec3& hi, const char* texture)
{
    Brush b;
    for(int axis = 0; axis < 3; axis++)
    {
        for(const double sign : {1.0, -1.0})
        {
            Face f;
            f.normal = glm::dvec3{0.0};
            f.normal[axis] = sign;
            f.dist = sign > 0.0 ? hi[axis] : -lo[axis];
            q_strlcpy(f.texture, texture && texture[0] ? texture : "base", sizeof(f.texture));
            defaultAlignment(f);
            b.faces.pushBack(f);
        }
    }
    return b;
}

bool bounds(const Brush& b, glm::dvec3& lo, glm::dvec3& hi)
{
    za::Vector<Winding> polys;
    lo = glm::dvec3{0.0};
    hi = glm::dvec3{0.0};
    const bool solid = polygons(b, polys);
    glm::dvec3 a{1e30}, c{-1e30};
    for(const Winding& w : polys)
    {
        for(const glm::dvec3& p : w)
        {
            a = glm::min(a, p);
            c = glm::max(c, p);
        }
    }
    if(a.x > c.x)
    {
        return false;
    }
    lo = a;
    hi = c;
    return solid && c.x - a.x >= 1.0 && c.y - a.y >= 1.0 && c.z - a.z >= 1.0;
}

const char* invalid(const Brush& b)
{
    if(b.faces.size() < 4)
    {
        return "fewer than four faces";
    }
    if(static_cast<int>(b.faces.size()) > maxFaces)
    {
        return "more faces than a brush may have";
    }
    for(const Face& f : b.faces)
    {
        if(za::fabs(glm::length(f.normal) - 1.0) > 1e-6)
        {
            return "a face's normal is not a unit";
        }
    }
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return "no volume (a side under a unit, or not closed)";
    }
    for(int i = 0; i < 3; i++)
    {
        if(lo[i] < -16384.0 || hi[i] > 16384.0)
        {
            return "outside the world's bounds";
        }
    }
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    int verts = 0, tris = 0;
    for(const Winding& w : polys)
    {
        verts += static_cast<int>(w.size());
        tris += w.size() >= 3 ? static_cast<int>(w.size()) - 2 : 0;
    }
    if(verts > QVR_BRUSH_SLOT_VERTS || tris * 3 > QVR_BRUSH_SLOT_INDEXES)
    {
        return "too many corners for its slot";
    }
    return nullptr;
}

void translate(Brush& b, const glm::dvec3& delta)
{
    for(Face& f : b.faces)
    {
        f.dist += glm::dot(f.normal, delta);
        // The texture moves with the face: its coordinate at a moved point is what it was at the point before.
        f.uOffset -= glm::dot(f.uAxis, delta) / (f.scaleU != 0.0 ? f.scaleU : 1.0);
        f.vOffset -= glm::dot(f.vAxis, delta) / (f.scaleV != 0.0 ? f.scaleV : 1.0);
    }
}

namespace
{

constexpr const char* shapeNames[] = {"box", "wedge", "cylinder", "pyramid"};

[[nodiscard]] Face planeFace(const glm::dvec3& normal, double dist, const char* texture)
{
    Face f;
    f.normal = normal;
    f.dist = dist;
    q_strlcpy(f.texture, texture && texture[0] ? texture : "base", sizeof(f.texture));
    defaultAlignment(f);
    return f;
}

// A plane's normal and distance with the dust of turning and stretching swept off: components a hair from 0 or 1 put
// there (a box turned 90 degrees is a box again), a distance a hair from a whole number on it.
void tidy(glm::dvec3& n, double& d)
{
    for(int k = 0; k < 3; k++)
    {
        if(za::fabs(n[k]) < 1e-12)
        {
            n[k] = 0.0;
        }
        else if(za::fabs(za::fabs(n[k]) - 1.0) < 1e-12)
        {
            n[k] = n[k] > 0.0 ? 1.0 : -1.0;
        }
    }
    n = glm::normalize(n);
    if(za::fabs(d - za::round(d)) < 1e-7)
    {
        d = za::round(d);
    }
}

} // namespace

const char* shapeName(Shape s)
{
    const int i = static_cast<int>(s);
    return i >= 0 && i < static_cast<int>(Shape::Count) ? shapeNames[i] : "box";
}

Shape shapeFromName(const char* name)
{
    for(int i = 0; i < static_cast<int>(Shape::Count); i++)
    {
        if(!q_strcasecmp(name, shapeNames[i]))
        {
            return static_cast<Shape>(i);
        }
    }
    const int n = Q_atoi(name);
    return n >= 0 && n < static_cast<int>(Shape::Count) ? static_cast<Shape>(n) : Shape::Box;
}

Brush shape(Shape s, const glm::dvec3& lo, const glm::dvec3& hi, int upAxis, double upSign, int riseAxis, double riseSign,
    int sides, const char* texture)
{
    upAxis = za::clamp(upAxis, 0, 2);
    if(riseAxis == upAxis || riseAxis < 0 || riseAxis > 2)
    {
        riseAxis = (upAxis + 1) % 3;
    }
    if(s == Shape::Box)
    {
        return box(lo, hi, texture);
    }
    Brush b;
    glm::dvec3 up{0.0};
    up[upAxis] = upSign >= 0.0 ? 1.0 : -1.0;
    const double base = upSign >= 0.0 ? lo[upAxis] : hi[upAxis], top = upSign >= 0.0 ? hi[upAxis] : lo[upAxis];
    const double height = hi[upAxis] - lo[upAxis];
    if(s == Shape::Wedge)
    {
        const int side = 3 - upAxis - riseAxis;
        glm::dvec3 fwd{0.0};
        fwd[riseAxis] = riseSign >= 0.0 ? 1.0 : -1.0;
        const double length = hi[riseAxis] - lo[riseAxis];
        const double low = riseSign >= 0.0 ? lo[riseAxis] : hi[riseAxis], high = riseSign >= 0.0 ? hi[riseAxis] : lo[riseAxis];
        b.faces.pushBack(planeFace(-up, -base * up[upAxis], texture));     // the bottom
        b.faces.pushBack(planeFace(fwd, high * fwd[riseAxis], texture));    // the high end
        for(const double sg : {1.0, -1.0})                                  // the sides
        {
            glm::dvec3 n{0.0};
            n[side] = sg;
            b.faces.pushBack(planeFace(n, sg > 0.0 ? hi[side] : -lo[side], texture));
        }
        // The slope: through the low end's bottom edge and the high end's top edge.
        const glm::dvec3 n = glm::normalize(up * length - fwd * height);
        glm::dvec3 p{0.0};
        p[upAxis] = base;
        p[riseAxis] = low;
        double d = glm::dot(n, p);
        glm::dvec3 nn = n;
        tidy(nn, d);
        b.faces.pushBack(planeFace(nn, d, texture));
        (void)top;
        return b;
    }
    // A cylinder's or pyramid's polygon: `sides` corners round a circle (a face flat to the first axis), stretched to fill
    // the box across the up axis.
    sides = za::clamp(sides, 3, 12);
    const int a1 = (upAxis + 1) % 3, a2 = (upAxis + 2) % 3;
    za::InPlaceVector<glm::dvec2, 12> poly;
    glm::dvec2 pmin{1e30}, pmax{-1e30};
    for(int k = 0; k < sides; k++)
    {
        const double ang = 6.283185307179586 * (static_cast<double>(k) + 0.5) / static_cast<double>(sides);
        const glm::dvec2 p{za::cos(ang), za::sin(ang)};
        poly.pushBack(p);
        pmin = glm::min(pmin, p);
        pmax = glm::max(pmax, p);
    }
    for(glm::dvec2& p : poly)
    {
        p = glm::dvec2{lo[a1], lo[a2]} + (p - pmin) / (pmax - pmin) * glm::dvec2{hi[a1] - lo[a1], hi[a2] - lo[a2]};
    }
    const auto at = [&](const glm::dvec2& p, double h) {
        glm::dvec3 q{0.0};
        q[a1] = p.x;
        q[a2] = p.y;
        q[upAxis] = h;
        return q;
    };
    glm::dvec2 mid{(lo[a1] + hi[a1]) * 0.5, (lo[a2] + hi[a2]) * 0.5};
    const glm::dvec3 inside = at(mid, (base + top) * 0.5);
    b.faces.pushBack(planeFace(-up, -base * up[upAxis], texture)); // the base
    if(s == Shape::Cylinder)
    {
        b.faces.pushBack(planeFace(up, top * up[upAxis], texture));
    }
    const glm::dvec3 apex = at(mid, top);
    for(int k = 0; k < sides; k++)
    {
        const glm::dvec3 p = at(poly[static_cast<za::SizeT>(k)], base), q = at(poly[static_cast<za::SizeT>((k + 1) % sides)], base);
        const glm::dvec3 r = s == Shape::Cylinder ? p + up : apex;
        glm::dvec3 n = glm::normalize(glm::cross(q - p, r - p));
        if(glm::dot(n, inside - p) > 0.0)
        {
            n = -n;
        }
        double d = glm::dot(n, p);
        tidy(n, d);
        b.faces.pushBack(planeFace(n, d, texture));
    }
    return b;
}

glm::dmat3 turnFromAngles(const glm::dvec3& angles)
{
    // Quake's AngleVectors in doubles (sines of whole quarter turns exact).
    const auto sc = [](double deg, double& s, double& c) {
        const double q = deg / 90.0;
        if(za::fabs(q - za::round(q)) < 1e-12)
        {
            const int k = ((static_cast<int>(za::round(q)) % 4) + 4) % 4;
            s = k == 1 ? 1.0 : k == 3 ? -1.0 : 0.0;
            c = k == 0 ? 1.0 : k == 2 ? -1.0 : 0.0;
            return;
        }
        s = za::sin(glm::radians(deg));
        c = za::cos(glm::radians(deg));
    };
    double sp, cp, sy, cy, sr, cr;
    sc(angles.x, sp, cp);
    sc(angles.y, sy, cy);
    sc(angles.z, sr, cr);
    const glm::dvec3 forward{cp * cy, cp * sy, -sp};
    const glm::dvec3 right{-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp};
    const glm::dvec3 up{cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp};
    return glm::dmat3{forward, -right, up};
}

void rotate(Brush& b, const glm::dmat3& r, const glm::dvec3& pivot, bool lockTexture)
{
    for(Face& f : b.faces)
    {
        glm::dvec3 n = r * f.normal;
        double d = glm::dot(n, pivot) + (f.dist - glm::dot(f.normal, pivot));
        tidy(n, d);
        if(lockTexture)
        {
            glm::dvec3 u = r * f.uAxis, v = r * f.vAxis;
            for(int k = 0; k < 3; k++)
            {
                u[k] = za::fabs(u[k]) < 1e-12 ? 0.0 : u[k];
                v[k] = za::fabs(v[k]) < 1e-12 ? 0.0 : v[k];
            }
            f.uOffset += (glm::dot(pivot, f.uAxis) - glm::dot(pivot, u)) / (f.scaleU != 0.0 ? f.scaleU : 1.0);
            f.vOffset += (glm::dot(pivot, f.vAxis) - glm::dot(pivot, v)) / (f.scaleV != 0.0 ? f.scaleV : 1.0);
            f.uAxis = u;
            f.vAxis = v;
            f.normal = n;
            f.dist = d;
        }
        else
        {
            f.normal = n;
            f.dist = d;
            const double su = f.scaleU, sv = f.scaleV;
            defaultAlignment(f);
            f.scaleU = su;
            f.scaleV = sv;
        }
    }
}

void scaleTo(Brush& b, const glm::dvec3& lo, const glm::dvec3& hi, const glm::dvec3& lo2, const glm::dvec3& hi2)
{
    // p' = lo2 + (p - lo) * k: a plane n.p <= d becomes (n / k).p' <= d - n.lo + (n / k).lo2.
    const glm::dvec3 k = (hi2 - lo2) / glm::max(hi - lo, glm::dvec3{1e-9});
    for(Face& f : b.faces)
    {
        const glm::dvec3 m = f.normal / glm::max(k, glm::dvec3{1e-9});
        const double len = glm::length(m);
        glm::dvec3 n = m / len;
        double d = (f.dist - glm::dot(f.normal, lo) + glm::dot(m, lo2)) / len;
        tidy(n, d);
        f.normal = n;
        f.dist = d;
    }
}

void forEachPolygon(const Brush& b, za::FunctionRef<void(int face, const glm::dvec3* points, int count)> f)
{
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    for(za::SizeT i = 0; i < polys.size(); i++)
    {
        if(polys[i].size() >= 3)
        {
            f(static_cast<int>(i), polys[i].data(), static_cast<int>(polys[i].size()));
        }
    }
}

bool axial(const Brush& b)
{
    for(const Face& f : b.faces)
    {
        if(za::fabs(f.normal.x) != 1.0 && za::fabs(f.normal.y) != 1.0 && za::fabs(f.normal.z) != 1.0)
        {
            return false;
        }
    }
    return true;
}

void toText(const Brush& b, za::String& out)
{
    out.clear();
    out += va("// qvrbrush id %d rev %u group %d kind solid\n{\n", b.id, b.rev, b.group);
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        const Face& f = b.faces[i];
        glm::dvec3 p[3];
        facePoints(f, polys[i], p);
        for(const glm::dvec3& q : p)
        {
            out += "( ";
            for(int k = 0; k < 3; k++)
            {
                appendNumber(out, q[k]);
                out += ' ';
            }
            out += ") ";
        }
        out += f.texture[0] ? f.texture : "base";
        const glm::dvec4 axes[2] = {glm::dvec4{f.uAxis, f.uOffset}, glm::dvec4{f.vAxis, f.vOffset}};
        for(const glm::dvec4& a : axes)
        {
            out += " [ ";
            for(int k = 0; k < 4; k++)
            {
                appendNumber(out, a[k]);
                out += ' ';
            }
            out += ']';
        }
        out += ' ';
        appendNumber(out, f.rotation);
        out += ' ';
        appendNumber(out, f.scaleU);
        out += ' ';
        appendNumber(out, f.scaleV);
        out += '\n';
    }
    out += "}\n";
}

bool fromText(za::StringView text, Brush& out, za::String& error)
{
    out = Brush{};
    // The header comment, if any: its id, revision and group.
    const char* header = "// qvrbrush "; // (not a brush file's "// qvrbrushes")
    for(za::SizeT i = 0; i + strlen(header) <= text.size(); i++)
    {
        if(!strncmp(text.data() + i, header, strlen(header)))
        {
            char line[128];
            za::SizeT n = 0;
            for(za::SizeT k = i; k < text.size() && text[k] != '\n' && n + 1 < sizeof(line); k++)
            {
                line[n++] = text[k];
            }
            line[n] = '\0';
            unsigned rev = 0;
            int id = 0, group = 0;
            if(sscanf(line, "// qvrbrush id %d rev %u group %d", &id, &rev, &group) >= 1)
            {
                out.id = id;
                out.rev = rev;
                out.group = group;
            }
            break;
        }
    }
    Tokens t{text};
    if(!t.expect("{"))
    {
        error = "no {";
        return false;
    }
    for(;;)
    {
        if(!t.next())
        {
            error = "no }";
            return false;
        }
        if(!strcmp(t.token, "}"))
        {
            break;
        }
        if(strcmp(t.token, "("))
        {
            error = va("a face starts with %s, not (", t.token);
            return false;
        }
        glm::dvec3 p[3];
        for(int k = 0; k < 3; k++)
        {
            if((k > 0 && !t.expect("(")) || !t.number(p[k].x) || !t.number(p[k].y) || !t.number(p[k].z) || !t.expect(")"))
            {
                error = va("face %d: a point is not ( x y z )", static_cast<int>(out.faces.size()));
                return false;
            }
        }
        Face f;
        const glm::dvec3 n = glm::cross(p[0] - p[1], p[2] - p[1]);
        if(glm::length(n) < 1e-9)
        {
            error = va("face %d: its points are on one line", static_cast<int>(out.faces.size()));
            return false;
        }
        f.normal = glm::normalize(n);
        f.dist = glm::dot(p[0], f.normal);
        // (snapped: a plane of whole numbers read back exactly)
        for(int k = 0; k < 3; k++)
        {
            if(za::fabs(f.normal[k] - za::round(f.normal[k])) < 1e-12)
            {
                f.normal[k] = za::round(f.normal[k]);
            }
        }
        if(za::fabs(f.dist - za::round(f.dist)) < 1e-9)
        {
            f.dist = za::round(f.dist);
        }
        if(!t.next())
        {
            error = "a face without a texture";
            return false;
        }
        q_strlcpy(f.texture, t.token, sizeof(f.texture));
        defaultAlignment(f);
        // Valve 220 ([ u ] [ v ] rotation scales) or Quake's (offsets, rotation, scales).
        const za::SizeT mark = t.at;
        if(t.next() && !strcmp(t.token, "["))
        {
            glm::dvec4 axes[2];
            for(int a = 0; a < 2; a++)
            {
                if((a > 0 && !t.expect("[")) || !t.number(axes[a].x) || !t.number(axes[a].y) || !t.number(axes[a].z) ||
                    !t.number(axes[a].w) || !t.expect("]"))
                {
                    error = va("face %d: its axes are not [ x y z offset ]", static_cast<int>(out.faces.size()));
                    return false;
                }
            }
            f.uAxis = glm::dvec3{axes[0]};
            f.uOffset = axes[0].w;
            f.vAxis = glm::dvec3{axes[1]};
            f.vOffset = axes[1].w;
            if(!t.number(f.rotation) || !t.number(f.scaleU) || !t.number(f.scaleV))
            {
                error = va("face %d: no rotation and scales", static_cast<int>(out.faces.size()));
                return false;
            }
        }
        else
        {
            t.at = mark;
            double shift[2], rot, scale[2];
            if(!t.number(shift[0]) || !t.number(shift[1]) || !t.number(rot) || !t.number(scale[0]) || !t.number(scale[1]))
            {
                error = va("face %d: no offsets, rotation and scales", static_cast<int>(out.faces.size()));
                return false;
            }
            // qbsp's: the default axes turned by the rotation about the plane's axis, scaled.
            glm::dvec3 vecs[2] = {f.uAxis, f.vAxis};
            const double ang = glm::radians(rot);
            double sinv = za::sin(ang), cosv = za::cos(ang);
            if(rot == 0.0) { sinv = 0.0; cosv = 1.0; }
            else if(rot == 90.0) { sinv = 1.0; cosv = 0.0; }
            else if(rot == 180.0) { sinv = 0.0; cosv = -1.0; }
            else if(rot == 270.0) { sinv = -1.0; cosv = 0.0; }
            const int sv = vecs[0].x != 0.0 ? 0 : vecs[0].y != 0.0 ? 1 : 2;
            const int tv = vecs[1].x != 0.0 ? 0 : vecs[1].y != 0.0 ? 1 : 2;
            for(glm::dvec3& v : vecs)
            {
                const double ns = cosv * v[sv] - sinv * v[tv];
                const double nt = sinv * v[sv] + cosv * v[tv];
                v[sv] = ns;
                v[tv] = nt;
            }
            f.uAxis = vecs[0];
            f.vAxis = vecs[1];
            f.uOffset = shift[0];
            f.vOffset = shift[1];
            f.rotation = rot;
            f.scaleU = scale[0] != 0.0 ? scale[0] : 1.0;
            f.scaleV = scale[1] != 0.0 ? scale[1] : 1.0;
        }
        if(static_cast<int>(out.faces.size()) >= maxFaces)
        {
            error = "more faces than a brush may have";
            return false;
        }
        out.faces.pushBack(f);
    }
    if(out.faces.size() < 4)
    {
        error = "fewer than four faces";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The registry.

namespace
{
// The entity create gives the next brush instead of a new one (restoreAfterLoad: the saved game's), 0: a new one.
int adoptEdict = 0;
} // namespace

int create(const Brush& b, int wantId)
{
    const int adopt = adoptEdict;
    adoptEdict = 0;
    if(!sv.active || !sv.worldmodel)
    {
        return 0;
    }
    if(const char* why = invalid(b))
    {
        Con_Printf("brush: not made: %s\n", why);
        return 0;
    }
    const int cap = capacity();
    int id = 0;
    if(wantId > 0 && wantId <= cap && !entryOf(wantId))
    {
        id = wantId;
    }
    for(int i = 0; i < cap && !id; i++)
    {
        if(i >= static_cast<int>(registry.entries.size()) || !registry.entries[static_cast<za::SizeT>(i)])
        {
            id = i + 1;
            break;
        }
    }
    if(!id)
    {
        Con_Printf("brush: not made: the map has room for %d brushes (vr_brush_max at its load)\n", cap);
        return 0;
    }
    if(id > static_cast<int>(registry.entries.size()))
    {
        registry.entries.resize(static_cast<za::SizeT>(id));
    }
    registry.entries[static_cast<za::SizeT>(id - 1)] = za::makeUnique<Entry>();
    Entry& e = *registry.entries[static_cast<za::SizeT>(id - 1)];
    e.brush = b;
    e.brush.id = id;
    e.brush.rev = b.rev > 0 ? b.rev : 1;
    e.edict = adopt;
    rebuild(e, false);
    if(!placeEntity(e))
    {
        registry.entries[static_cast<za::SizeT>(id - 1)].reset();
        return 0;
    }
    recordUndo(UndoStep::Kind::Made, id, nullptr);
    return id;
}

bool undo()
{
    if(registry.undo.empty())
    {
        Con_Printf("brush: nothing to undo\n");
        return false;
    }
    const UndoStep s = registry.undo.back();
    registry.undo.popBack();
    registry.undoing = true;
    bool ok = false;
    switch(s.kind)
    {
    case UndoStep::Kind::Made:
        ok = remove(s.id);
        Con_Printf("brush: undone: %d made%s\n", s.id, ok ? " (removed)" : " (already gone)");
        break;
    case UndoStep::Kind::Changed:
        ok = change(s.id, s.before);
        Con_Printf("brush: undone: %d changed%s\n", s.id, ok ? " (as it was)" : " (gone)");
        break;
    case UndoStep::Kind::Removed:
    {
        const int id = create(s.before, s.id);
        ok = id != 0;
        Con_Printf("brush: undone: %d removed (back as %d)\n", s.id, id);
        break;
    }
    }
    registry.undoing = false;
    return ok;
}

bool change(int id, const Brush& b)
{
    Entry* e = entryOf(id);
    if(!e)
    {
        return false;
    }
    if(const char* why = invalid(b))
    {
        Con_Printf("brush: %d not changed: %s\n", id, why);
        return false;
    }
    const za::U32 rev = e->brush.rev;
    const bool painted = e->built && !e->built->surfaces.empty() && sameShape(e->brush, b);
    recordUndo(UndoStep::Kind::Changed, id, &e->brush);
    e->brush = b;
    e->brush.id = id;
    e->brush.rev = rev + 1;
    rebuild(*e, false, painted);
    return placeEntity(*e);
}

bool remove(int id)
{
    Entry* e = entryOf(id);
    if(!e)
    {
        return false;
    }
    const int num = e->edict;
    recordUndo(UndoStep::Kind::Removed, id, &e->brush);
    e->edict = 0; // (its entity's free below finds no brush: edictFreed)
    rebuild(*e, true);
    upload(id - 1, e);
    registry.entries[static_cast<za::SizeT>(id - 1)].reset();
    if(num > 0 && sv.active)
    {
        const ServerVm vm;
        edict_t* ed = EDICT_NUM(num);
        if(!ed->free)
        {
            ED_Free(ed);
        }
    }
    return true;
}

const Brush* find(int id)
{
    const Entry* e = entryOf(id);
    return e ? &e->brush : nullptr;
}

int ofEntity(int num)
{
    if(num <= 0)
    {
        return 0;
    }
    for(za::SizeT i = 0; i < registry.entries.size(); i++)
    {
        if(registry.entries[i] && registry.entries[i]->edict == num)
        {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

int entityOf(int id)
{
    const Entry* e = entryOf(id);
    return e ? e->edict : 0;
}

int count()
{
    int n = 0;
    for(const auto& e : registry.entries)
    {
        n += e ? 1 : 0;
    }
    return n;
}

void forEach(za::FunctionRef<void(const Brush&)> f)
{
    for(const auto& e : registry.entries)
    {
        if(e)
        {
            f(e->brush);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Keeping them.

namespace
{

// The map's shape (its planes: a relit copy has the same), as a brush file names it.
[[nodiscard]] unsigned mapHash()
{
    unsigned h = 2166136261u;
    const qmodel_t* w = sv.worldmodel;
    if(!w || !w->planes)
    {
        return 0u;
    }
    const unsigned char* bytes = nullptr;
    for(za::SizeT i = 0; i < static_cast<za::SizeT>(w->numplanes); i++)
    {
        const mplane_t& p = w->planes[i];
        const float v[4] = {p.normal[0], p.normal[1], p.normal[2], p.dist};
        bytes = reinterpret_cast<const unsigned char*>(v);
        for(za::SizeT k = 0; k < sizeof(v); k++)
        {
            h = (h ^ bytes[k]) * 16777619u;
        }
    }
    return h;
}

// The brushes in a text: each "{ ... }" with the lines before it (its header) since the last.
bool parseAll(za::StringView text, za::Vector<Brush>& out, za::String& error)
{
    out.clear();
    za::SizeT start = 0, at = 0;
    while(at < text.size())
    {
        za::SizeT end = at;
        while(end < text.size() && text[end] != '\n')
        {
            end++;
        }
        // (a line that is "}" alone, spaces aside, ends a brush)
        za::SizeT a = at, z = end;
        while(a < z && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r'))
        {
            a++;
        }
        while(z > a && (text[z - 1] == ' ' || text[z - 1] == '\t' || text[z - 1] == '\r'))
        {
            z--;
        }
        if(z - a == 1 && text[a] == '}')
        {
            Brush b;
            za::String why;
            if(!fromText(za::StringView{text.data() + start, end - start}, b, why))
            {
                error = va("brush %d: %s", static_cast<int>(out.size()) + 1, why.cStr());
                return false;
            }
            out.pushBack(ZA_MOVE(b));
            start = end + 1;
        }
        at = end + 1;
    }
    return true;
}

// The brush file's path for `name` (empty: the map's own): <game folder>/brushes/<map>[-name].brushes.
[[nodiscard]] za::String brushFile(const char* name)
{
    return za::String{va("%s/brushes/%s%s%s.brushes", com_gamedir, sv.name, name && name[0] ? "-" : "", name ? name : "")};
}

} // namespace

void allToText(za::String& out)
{
    out.clear();
    out += va("// qvrbrushes 1 map %s bsp %08x\n", sv.name, mapHash());
    za::String one;
    forEach([&](const Brush& b) {
        toText(b, one);
        out += one;
    });
}

int fromAllText(za::StringView text, bool replace, za::String& warning)
{
    warning.clear();
    za::Vector<Brush> list;
    za::String error;
    if(!parseAll(text, list, error))
    {
        warning = error;
        return 0;
    }
    char map[64] = "";
    unsigned hash = 0;
    const za::String copy{text.data(), text.size()};
    const char* header = strstr(copy.cStr(), "// qvrbrushes ");
    if(header && sscanf(header, "// qvrbrushes %*d map %63s bsp %x", map, &hash) == 2 && (q_strcasecmp(map, sv.name) || hash != mapHash()))
    {
        warning = va("made for %s (bsp %08x), this is %s (bsp %08x)", map, hash, sv.name, mapHash());
    }
    if(replace)
    {
        for(za::SizeT i = 0; i < registry.entries.size(); i++)
        {
            if(registry.entries[i])
            {
                remove(static_cast<int>(i) + 1);
            }
        }
    }
    int made = 0;
    for(const Brush& b : list)
    {
        made += create(b, b.id) ? 1 : 0;
    }
    return made;
}

const char* saveLines()
{
    SaveState& s = saveState;
    s.lines.clear();
    if(!count())
    {
        return "";
    }
    za::String all;
    allToText(all);
    za::SizeT at = 0;
    while(at < all.size())
    {
        za::SizeT end = at;
        while(end < all.size() && all[end] != '\n')
        {
            end++;
        }
        s.lines += "// qvrb ";
        s.lines += za::StringView{all.data() + at, end - at};
        s.lines += '\n';
        at = end + 1;
    }
    return s.lines.cStr();
}

void readSaveLines(const char* saveText)
{
    SaveState& s = saveState;
    s.pending.clear();
    s.have = false;
    for(const char* p = saveText ? strstr(saveText, "\n// qvrb ") : nullptr; p; p = strstr(p + 1, "\n// qvrb "))
    {
        const char* line = p + 9;
        const za::SizeT n = strcspn(line, "\r\n");
        s.pending += za::StringView{line, n};
        s.pending += '\n';
        s.have = true;
    }
}

void restoreAfterLoad()
{
    SaveState& s = saveState;
    registry.loadedSave = true;
    if(!s.have)
    {
        return;
    }
    s.have = false;
    za::Vector<Brush> list;
    za::String error;
    if(!parseAll(s.pending, list, error))
    {
        Con_Printf("brush: the saved game's brushes can't be read (%s)\n", error.cStr());
        return;
    }
    const ServerVm vm;
    registry.undoing = true;
    int made = 0;
    for(const Brush& b : list)
    {
        // Its entity: the one whose model is its own.
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%d", modelPrefix, b.id);
        int num = 0;
        for(int i = 1; i < qcvm->num_edicts && !num; i++)
        {
            edict_t* ed = EDICT_NUM(i);
            if(!ed->free && !strcmp(PR_GetString(ed->v.classname), className) && !strcmp(PR_GetString(ed->v.model), name) && !ofEntity(i))
            {
                num = i;
            }
        }
        adoptEdict = num;
        made += create(b, b.id) ? 1 : 0;
    }
    adoptEdict = 0;
    registry.undoing = false;
    s.pending.clear();
    Con_Printf("brush: %d brushes of the saved game made again\n", made);
}

// ---------------------------------------------------------------------------------------------------------------------
// Textures.

bool usableTexture(const texture_t* t)
{
    if(!t || !t->name[0] || !t->gltexture)
    {
        return false;
    }
    if(t->type != TEXTYPE_DEFAULT && t->type != TEXTYPE_CUTOUT)
    {
        return false; // (sky, liquids)
    }
    if(t->name[0] == '+' && t->name[1] != '0' && t->name[1] != 'a' && t->name[1] != 'A')
    {
        return false; // (an animation's later frames: its first is the one)
    }
    return q_strncasecmp(t->name, "trigger", 7) != 0 && q_strncasecmp(t->name, "clip", 4) != 0 &&
           q_strncasecmp(t->name, "skip", 4) != 0 && q_strncasecmp(t->name, "hint", 4) != 0 &&
           q_strncasecmp(t->name, "notexture", 9) != 0;
}

namespace
{

// The world's usable textures and its default (textureCache), found for the world now (again after a map change).
[[nodiscard]] const TextureCache& textures()
{
    TextureCache& c = textureCache;
    const qmodel_t* w = sv.worldmodel ? sv.worldmodel : cl.worldmodel;
    if(c.world == w && !c.usable.empty())
    {
        return c;
    }
    c.world = w;
    c.usable.clear();
    c.defaultName[0] = 0;
    if(!w || !w->textures)
    {
        return c;
    }
    for(int i = 0; i < w->numtextures; i++)
    {
        if(usableTexture(w->textures[i]))
        {
            c.usable.pushBack(w->textures[i]);
        }
    }
    // The map's most used plain texture (its walls').
    const texture_t* best = c.usable.empty() ? nullptr : c.usable[0];
    if(w->surfaces)
    {
        za::Vector<int> uses(static_cast<za::SizeT>(w->numtextures), 0);
        for(int i = 0; i < w->numsurfaces; i++)
        {
            const mtexinfo_t* ti = w->surfaces[i].texinfo;
            if(ti && ti->texnum >= 0 && ti->texnum < w->numtextures)
            {
                uses[static_cast<za::SizeT>(ti->texnum)]++;
            }
        }
        int most = 0;
        for(int i = 0; i < w->numtextures; i++)
        {
            const texture_t* t = w->textures[i];
            // (a wall's texture: not a trim strip, a light's or a button's)
            if(usableTexture(t) && t->type == TEXTYPE_DEFAULT && t->name[0] != '+' && t->width >= 32 && t->height >= 32 &&
                uses[static_cast<za::SizeT>(i)] > most)
            {
                most = uses[static_cast<za::SizeT>(i)];
                best = t;
            }
        }
    }
    q_strlcpy(c.defaultName, best ? best->name : "base", sizeof(c.defaultName));
    return c;
}

} // namespace

int textureCount()
{
    return static_cast<int>(textures().usable.size());
}

texture_t* textureAt(int index)
{
    const TextureCache& c = textures();
    return index >= 0 && index < static_cast<int>(c.usable.size()) ? c.usable[static_cast<za::SizeT>(index)] : nullptr;
}

const char* currentTexture()
{
    if(registry.texture[0] && usableTexture(worldTexture(registry.texture)))
    {
        return registry.texture;
    }
    const TextureCache& c = textures();
    return c.defaultName[0] ? c.defaultName : "base";
}

void setCurrentTexture(const char* name)
{
    texture_t* t = worldTexture(name);
    if(usableTexture(t))
    {
        q_strlcpy(registry.texture, t->name, sizeof(registry.texture));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The models.

bool isRuntime(const qmodel_t* model)
{
    return model && model->name[0] == '*' && idFromName(model->name) > 0;
}

za::U32 revision(const qmodel_t* model)
{
    if(!isRuntime(model))
    {
        return 0;
    }
    const Entry* e = entryOf(idFromName(model->name));
    return e && e->model == model ? e->revision : 0u;
}

bool clipBox(const qmodel_t* model, const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, trace_t& trace)
{
    if(!isRuntime(model))
    {
        return false;
    }
    memset(&trace, 0, sizeof(trace));
    trace.fraction = 1.f;
    trace.inopen = true;
    VectorCopy(end, trace.endpos);
    const Entry* e = entryOf(idFromName(model->name));
    if(!e || e->model != model || !e->built || e->built->clipnodes[1].empty())
    {
        return true; // (nothing solid)
    }
    // The brush's planes (model space) and its bevels: hulls 1 and 2's before they were pushed out.
    const za::Vector<glm::vec4>& planes = e->built->clipPlanes;
    float enter = -1.f, leave = 1.f;
    int lead = -1;
    bool startOut = false, getOut = false;
    for(za::SizeT i = 0; i < planes.size(); i++)
    {
        const glm::vec3 n{planes[i]};
        glm::vec3 ofs;
        for(int k = 0; k < 3; k++)
        {
            ofs[k] = n[k] < 0.f ? maxs[k] : mins[k];
        }
        const float dist = planes[i].w - glm::dot(ofs, n);
        const float d1 = glm::dot(start, n) - dist, d2 = glm::dot(end, n) - dist;
        if(d2 > 0.f)
        {
            getOut = true;
        }
        if(d1 > 0.f)
        {
            startOut = true;
        }
        if(d1 > 0.f && d2 >= d1)
        {
            return true; // (wholly in front of a plane: never in it)
        }
        if(d1 <= 0.f && d2 <= 0.f)
        {
            continue;
        }
        if(d1 > d2)
        {
            const float f = (d1 - distEpsilon) / (d1 - d2);
            if(f > enter)
            {
                enter = f;
                lead = static_cast<int>(i);
            }
        }
        else
        {
            const float f = (d1 + distEpsilon) / (d1 - d2);
            leave = za::min(leave, f);
        }
    }
    if(!startOut)
    {
        trace.startsolid = true;
        trace.allsolid = !getOut;
        trace.inopen = !trace.allsolid;
        if(trace.allsolid)
        {
            return true;
        }
        // (starting in it, getting out: Quake's trace from within a solid goes on to the end)
        return true;
    }
    if(enter < leave && enter > -1.f && lead >= 0)
    {
        const float f = za::max(enter, 0.f);
        trace.fraction = f;
        for(int k = 0; k < 3; k++)
        {
            trace.endpos[k] = start[k] + f * (end[k] - start[k]);
        }
        const glm::vec3 n{planes[static_cast<za::SizeT>(lead)]};
        glm::vec3 ofs;
        for(int k = 0; k < 3; k++)
        {
            ofs[k] = n[k] < 0.f ? maxs[k] : mins[k];
        }
        VectorCopy(n, trace.plane.normal);
        trace.plane.dist = planes[static_cast<za::SizeT>(lead)].w - glm::dot(ofs, n);
    }
    return true;
}

void facesIn(const glm::vec3& lo, const glm::vec3& hi,
    za::FunctionRef<void(const za::Vector<glm::vec3>& poly, const glm::vec3& normal, int id)> f)
{
    za::Vector<glm::vec3> poly;
    for(const auto& ep : registry.entries)
    {
        if(!ep || !ep->built || ep->built->surfaces.empty() || ep->edict <= 0 || ep->edict >= cl.num_entities)
        {
            continue;
        }
        const Built& b = *ep->built;
        const entity_t& ent = cl_entities[ep->edict];
        if(ent.model != ep->model)
        {
            continue;
        }
        const glm::vec3 origin{ent.origin[0], ent.origin[1], ent.origin[2]};
        if(glm::any(glm::lessThan(origin + b.maxs, lo)) || glm::any(glm::greaterThan(origin + b.mins, hi)))
        {
            continue;
        }
        for(const msurface_t& s : b.surfaces)
        {
            const glm::vec3 smin = origin + glm::vec3{s.mins[0], s.mins[1], s.mins[2]};
            const glm::vec3 smax = origin + glm::vec3{s.maxs[0], s.maxs[1], s.maxs[2]};
            if(glm::any(glm::lessThan(smax, lo)) || glm::any(glm::greaterThan(smin, hi)))
            {
                continue;
            }
            poly.clear();
            for(int i = 0; i < s.numedges; i++)
            {
                const int se = b.surfedges[static_cast<za::SizeT>(s.firstedge + i)];
                const float* v = b.vertexes[se >= 0 ? b.edges[static_cast<za::SizeT>(se)].v[0] : b.edges[static_cast<za::SizeT>(-se)].v[1]].position;
                poly.pushBack(origin + glm::vec3{v[0], v[1], v[2]});
            }
            glm::vec3 n{s.plane->normal[0], s.plane->normal[1], s.plane->normal[2]};
            if(s.flags & SURF_PLANEBACK)
            {
                n = -n;
            }
            f(poly, n, ep->brush.id);
        }
    }
}

bool traceLine(const glm::vec3& start, const glm::vec3& end, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    bool hit = false;
    fraction = 1.f;
    for(const auto& ep : registry.entries)
    {
        if(!ep || !ep->built || ep->built->surfaces.empty() || ep->edict <= 0 || ep->edict >= cl.num_entities)
        {
            continue;
        }
        const entity_t& ent = cl_entities[ep->edict];
        if(ent.model != ep->model)
        {
            continue;
        }
        const glm::vec3 origin{ent.origin[0], ent.origin[1], ent.origin[2]};
        trace_t tr;
        if(clipBox(ep->model, start - origin, glm::vec3{0.f}, glm::vec3{0.f}, end - origin, tr) && !tr.startsolid &&
            tr.fraction < fraction)
        {
            fraction = tr.fraction;
            where = glm::vec3{tr.endpos[0], tr.endpos[1], tr.endpos[2]} + origin;
            normal = glm::vec3{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
            hit = true;
        }
    }
    return hit;
}

// ---------------------------------------------------------------------------------------------------------------------
// Hooks.

void reset()
{
    // The models keep their names (the next map's load may find them in mod_known): empty until made again.
    for(auto& e : registry.entries)
    {
        if(e && e->model)
        {
            apply(e->model, nullptr, e->brush.id);
        }
    }
    registry.entries.clear();
    registry.undo.clear();
    registry.sweep = true;
    registry.loadedSave = false;
}

void edictFreed(int num)
{
    const int id = ofEntity(num);
    if(!id)
    {
        return;
    }
    Entry* e = entryOf(id);
    e->edict = 0; // (being freed: not again)
    remove(id);
}

void frame()
{
    if(registry.sweep && sv.active && sv.state == ss_active)
    {
        registry.sweep = false;
        const ServerVm vm;
        int strays = 0;
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
        {
            edict_t* ed = EDICT_NUM(i);
            if(!ed->free && !strcmp(PR_GetString(ed->v.classname), className) && !ofEntity(i))
            {
                ED_Free(ed);
                strays++;
            }
        }
        if(strays)
        {
            Con_Printf("brush: %d brush entities without their brush left out (a save of a build that didn't keep them)\n", strays);
        }
        // A new map (not a saved game's): its brush file's brushes, if it has one and vr_brush_autoload is on.
        if(!registry.loadedSave && vr_brush_autoload.value != 0.f && files::exists(brushFile("").cStr()))
        {
            za::String text, warning;
            if(files::readText(brushFile("").cStr(), text))
            {
                registry.undoing = true;
                const int made = fromAllText(text, false, warning);
                registry.undoing = false;
                Con_Printf("brush: %d brushes made from %s%s%s\n", made, brushFile("").cStr(), warning.empty() ? "" : ": ",
                    warning.cStr());
            }
        }
    }
    if(cls.state == ca_dedicated || !cl.worldmodel)
    {
        return;
    }
    const bool all = registry.uploadAll;
    registry.uploadAll = false;
    for(za::SizeT i = 0; i < registry.entries.size(); i++)
    {
        Entry* e = registry.entries[i].get();
        if(e && (e->dirty || all))
        {
            e->dirty = false;
            upload(static_cast<int>(i), e);
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_brush_box", box_f);
    Cmd_AddCommand("vr_brush_here", here_f);
    Cmd_AddCommand("vr_brush_set", set_f);
    Cmd_AddCommand("vr_brush_move", move_f);
    Cmd_AddCommand("vr_brush_texture", texture_f);
    Cmd_AddCommand("vr_brush_texture_set", textureSet_f);
    Cmd_AddCommand("vr_brush_delete", delete_f);
    Cmd_AddCommand("vr_brush_duplicate", duplicate_f);
    Cmd_AddCommand("vr_brush_list", list_f);
    Cmd_AddCommand("vr_brush_text", text_f);
    Cmd_AddCommand("vr_brush_textures", textures_f);
    Cmd_AddCommand("vr_brush_selftest", selftest_f);
    Cmd_AddCommand("vr_brush_spawn", spawn_f);
    Cmd_AddCommand("vr_brush_ent", ent_f);
    Cmd_AddCommand("vr_brush_trace", trace_f);
    Cmd_AddCommand("vr_brush_shape", shape_f);
    Cmd_AddCommand("vr_brush_rotate", rotate_f);
    Cmd_AddCommand("vr_brush_undo", undo_f);
    Cmd_AddCommand("vr_brush_align", align_f);
    Cmd_AddCommand("vr_brush_save", save_f);
    Cmd_AddCommand("vr_brush_load", load_f);
}

} // namespace qvr::brushes

// ---------------------------------------------------------------------------------------------------------------------
// The engine's hooks.

extern "C" int VR_RuntimeBrushModel(qmodel_t* mod)
{
    using namespace qvr::brushes;
    const int id = idFromName(mod->name);
    if(mod->name[0] != '*' || id <= 0)
    {
        return 0;
    }
    Entry* e = entryOf(id);
    if(e && e->built)
    {
        e->model = mod;
        apply(mod, e->built.get(), id);
        e->dirty = true;
    }
    else
    {
        apply(mod, nullptr, id); // (a save's brush, or one gone: empty)
    }
    return 1;
}

extern "C" int VR_IsRuntimeBrushModel(const qmodel_t* mod)
{
    return qvr::brushes::isRuntime(mod);
}

extern "C" int VR_RuntimeBrushSlots(void)
{
    return za::clamp(static_cast<int>(qvr::vr_brush_max.value), 0, 4096);
}

extern "C" void VR_RuntimeBrushesUpload(void)
{
    // The map's buffers made again: every brush's slot written again (its model not drawn until then).
    for(auto& e : qvr::brushes::registry.entries)
    {
        if(e && e->model)
        {
            for(int i = 0; i <= TEXTYPE_COUNT; i++)
            {
                e->model->texofs[i] = 0;
            }
        }
    }
    qvr::brushes::registry.uploadAll = true;
    qvr::brushes::registry.reserveWorld = cl.worldmodel;
}

extern "C" const char* VR_SaveBrushLines(void)
{
    return qvr::brushes::saveLines();
}
