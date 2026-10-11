// vr_brushbake.cpp -- the runtime brushes' full bake (vr_brushbake.hpp): the brushes appended to a .bsp as submodels
// for ericw-tools' light, and their light read back from the relit world.

#include "vr_brushbake.hpp"

#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <stdio.h>
#include <string.h>

namespace qvr::brushbake
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The .bsp's lumps, read and written (little-endian, as Quake's).

constexpr int numLumps = 15;
enum Lump : int
{
    Entities,
    Planes,
    Textures,
    Vertexes,
    Visibility,
    Nodes,
    Texinfo,
    Faces,
    Lighting,
    Clipnodes,
    Leafs,
    Marksurfaces,
    Edges,
    Surfedges,
    Models
};

enum class Format
{
    Bsp29,
    Bsp2,    // "BSP2": everything wide, node and leaf bounds floats
    Bsp2rmq, // "2PSB": wide indices, node and leaf bounds shorts
};

struct Sizes
{
    int node, face, clipnode, leaf, edge;
};

[[nodiscard]] Sizes sizesOf(Format f)
{
    switch(f)
    {
    case Format::Bsp29: return Sizes{24, 20, 8, 28, 4};
    case Format::Bsp2: return Sizes{44, 28, 12, 44, 8};
    case Format::Bsp2rmq: return Sizes{32, 28, 12, 32, 8};
    }
    return Sizes{24, 20, 8, 28, 4};
}

struct Writer
{
    za::Vector<unsigned char>& v;
    void bytes(const void* p, za::SizeT n)
    {
        const za::SizeT at = v.size();
        v.resize(at + n);
        memcpy(v.data() + at, p, n);
    }
    void i32(int x)
    {
        x = LittleLong(x);
        bytes(&x, 4);
    }
    void u32(unsigned x) { i32(static_cast<int>(x)); }
    void i16(int x)
    {
        short s = LittleShort(static_cast<short>(x));
        bytes(&s, 2);
    }
    void f32(double x)
    {
        float f = LittleFloat(static_cast<float>(x));
        bytes(&f, 4);
    }
};

[[nodiscard]] int readI32(const unsigned char* p)
{
    int v;
    memcpy(&v, p, 4);
    return LittleLong(v);
}

// A plane as Quake stores it (its largest component positive; its type), and whether that turned the normal round.
struct StoredPlane
{
    glm::dvec3 n;
    double d;
    int type;
    bool flip;
};

[[nodiscard]] StoredPlane stored(const glm::dvec3& normal, double dist)
{
    int axis = 0;
    for(int i = 1; i < 3; i++)
    {
        if(za::fabs(normal[i]) > za::fabs(normal[axis]))
        {
            axis = i;
        }
    }
    StoredPlane p{normal, dist, 0, normal[axis] < 0.0};
    if(p.flip)
    {
        p.n = -p.n;
        p.d = -p.d;
    }
    p.type = za::fabs(p.n[axis] - 1.0) < 1e-9 ? axis : 3 + axis;
    return p;
}

// The world's light.exe light for its brushes: the "// qvr_brushbake" lines of its entity lump. The main thread's,
// read again for another world.
struct BakedEntry
{
    unsigned long long hash{0};
    int model{0};
    int faces{0};
};
struct Baked
{
    const qmodel_t* world{nullptr};
    const char* entities{nullptr};
    za::Vector<BakedEntry> list;
};
Baked baked;

const Baked& bakedOfWorld()
{
    Baked& b = baked;
    const qmodel_t* w = cl.worldmodel;
    if(b.world == w && b.entities == (w ? w->entities : nullptr))
    {
        return b;
    }
    b.world = w;
    b.entities = w ? w->entities : nullptr;
    b.list.clear();
    for(const char* p = b.entities ? strstr(b.entities, "// qvr_brushbake *") : nullptr; p; p = strstr(p + 1, "// qvr_brushbake *"))
    {
        BakedEntry e;
        if(sscanf(p, "// qvr_brushbake *%d %llx %d", &e.model, &e.hash, &e.faces) == 3)
        {
            b.list.pushBack(e);
        }
    }
    return b;
}

// A lux texel from ericw's .lux sample at a face (its frame s, -t, normal) in the world shader's (VR_FillSurfaceLux's).
[[nodiscard]] unsigned luxTexel(const msurface_t& s, const byte* lux)
{
    glm::dvec3 n{s.plane->normal[0], s.plane->normal[1], s.plane->normal[2]};
    if(s.flags & SURF_PLANEBACK)
    {
        n = -n;
    }
    glm::dvec3 r0{s.texinfo->vecs[0][0], s.texinfo->vecs[0][1], s.texinfo->vecs[0][2]};
    glm::dvec3 r1 = -glm::dvec3{s.texinfo->vecs[1][0], s.texinfo->vecs[1][1], s.texinfo->vecs[1][2]};
    if(glm::length(r0) < 1e-9 || glm::length(r1) < 1e-9)
    {
        return 0xffff8080u;
    }
    r0 = glm::normalize(r0);
    r1 = glm::normalize(r1);
    const glm::dmat3 frame = glm::transpose(glm::dmat3{r0, r1, n}); // (rows s, -t, n)
    if(za::fabs(glm::determinant(frame)) < 1e-4)
    {
        return 0xffff8080u;
    }
    const glm::dvec3 stored{lux[0] / 128.0 - 1.0, lux[1] / 128.0 - 1.0, lux[2] / 128.0 - 1.0};
    glm::dvec3 dir = glm::inverse(frame) * stored;
    if(glm::length(dir) < 1e-6)
    {
        return 0xffff8080u;
    }
    dir = glm::normalize(dir);
    glm::dvec3 t = r0 - n * glm::dot(r0, n);
    if(glm::length(t) < 1e-4)
    {
        return 0xffff8080u;
    }
    t = glm::normalize(t);
    const glm::dvec3 b = glm::cross(n, t);
    const auto enc = [](double v) { return static_cast<unsigned>(za::clamp(v * 127.5 + 127.5, 0.0, 255.0) + 0.5); };
    return enc(glm::dot(dir, t)) | (enc(glm::dot(dir, b)) << 8) | (enc(glm::dot(dir, n)) << 16) | 0xff000000u;
}

} // namespace

bool augment(const unsigned char* data, za::SizeT size, za::Vector<unsigned char>& out, za::String& entities, za::String& notes,
    za::String& error, const Options& options)
{
    out.clear();
    entities.clear();
    notes.clear();
    if(!data || size < 4 + numLumps * 8)
    {
        error = "not a .bsp";
        return false;
    }
    Format format;
    if(readI32(data) == 29)
    {
        format = Format::Bsp29;
    }
    else if(!memcmp(data, "BSP2", 4))
    {
        format = Format::Bsp2;
    }
    else if(!memcmp(data, "2PSB", 4))
    {
        format = Format::Bsp2rmq;
    }
    else
    {
        error = "not a BSP29, BSP2 or 2PSB map";
        return false;
    }
    const Sizes sz = sizesOf(format);
    const bool wide = format != Format::Bsp29;
    za::Vector<unsigned char> lumps[numLumps];
    for(int i = 0; i < numLumps; i++)
    {
        const int o = readI32(data + 4 + i * 8), l = readI32(data + 8 + i * 8);
        if(o < 0 || l < 0 || static_cast<za::SizeT>(o) + static_cast<za::SizeT>(l) > size)
        {
            error = "a lump outside the file";
            return false;
        }
        lumps[i].resize(static_cast<za::SizeT>(l));
        if(l)
        {
            memcpy(lumps[i].data(), data + o, static_cast<za::SizeT>(l));
        }
    }
    const auto count = [&](Lump l, int each) { return static_cast<int>(lumps[l].size()) / each; };
    // The textures' names (texinfo's miptex is their index).
    za::Vector<za::String> textureNames;
    if(lumps[Textures].size() >= 4)
    {
        const int n = readI32(lumps[Textures].data());
        for(int i = 0; i < n && 4 + static_cast<za::SizeT>(i + 1) * 4 <= lumps[Textures].size(); i++)
        {
            const int off = readI32(lumps[Textures].data() + 4 + i * 4);
            char name[17] = "";
            if(off >= 0 && static_cast<za::SizeT>(off) + 16 <= lumps[Textures].size())
            {
                memcpy(name, lumps[Textures].data() + off, 16);
            }
            textureNames.pushBack(za::String{name});
        }
    }
    const auto textureIndex = [&](const char* name) {
        for(za::SizeT i = 0; i < textureNames.size(); i++)
        {
            if(!q_strcasecmp(textureNames[i].cStr(), name))
            {
                return static_cast<int>(i);
            }
        }
        return 0;
    };

    Writer planes{lumps[Planes]}, vertexes{lumps[Vertexes]}, nodes{lumps[Nodes]}, texinfo{lumps[Texinfo]}, faces{lumps[Faces]},
        clipnodes{lumps[Clipnodes]}, leafs{lumps[Leafs]}, edges{lumps[Edges]}, surfedges{lumps[Surfedges]}, models{lumps[Models]};
    const auto addPlane = [&](const StoredPlane& p) {
        const int index = count(Planes, 20);
        planes.f32(p.n.x);
        planes.f32(p.n.y);
        planes.f32(p.n.z);
        planes.f32(p.d);
        planes.i32(p.type);
        return index;
    };
    const auto bound = [&](Writer& w, double v, bool up) {
        if(format == Format::Bsp2)
        {
            w.f32(v);
        }
        else
        {
            w.i16(static_cast<int>(za::clamp(up ? za::ceil(v) : za::floor(v), -32768.0, 32767.0)));
        }
    };
    // One empty leaf for every brush's hull 0 (the outside of its faces; leaf 0, the map's, is the solid inside).
    const int emptyLeaf = count(Leafs, sz.leaf);
    leafs.i32(CONTENTS_EMPTY);
    leafs.i32(-1);
    for(int i = 0; i < 6; i++)
    {
        bound(leafs, 0.0, false);
    }
    if(wide)
    {
        leafs.u32(0);
        leafs.u32(0);
    }
    else
    {
        leafs.i16(0);
        leafs.i16(0);
    }
    leafs.i32(0); // (ambient levels)
    const auto leafChild = [&](int leaf) { return -(leaf + 1); };

    int made = 0;
    za::String failed;
    brushes::forEach([&](const brushes::Brush& b) {
        if(!failed.empty())
        {
            return;
        }
        brushes::BakeShape shape;
        if(!brushes::bakeShape(b, shape))
        {
            return;
        }
        const int model = count(Models, 64);
        const int firstFace = count(Faces, sz.face);
        const int firstNode = count(Nodes, sz.node);
        const int numSurfaces = static_cast<int>(shape.surfaces.size());
        // Vertices (shared within the brush).
        za::Vector<glm::dvec3> verts;
        const int firstVertex = count(Vertexes, 12);
        const auto vertexOf = [&](const glm::dvec3& p) {
            for(za::SizeT i = 0; i < verts.size(); i++)
            {
                if(glm::length(verts[i] - p) < 1e-3)
                {
                    return firstVertex + static_cast<int>(i);
                }
            }
            verts.pushBack(p);
            vertexes.f32(p.x);
            vertexes.f32(p.y);
            vertexes.f32(p.z);
            return firstVertex + static_cast<int>(verts.size()) - 1;
        };
        for(int k = 0; k < numSurfaces; k++)
        {
            const brushes::BakeShape::Surface& s = shape.surfaces[static_cast<za::SizeT>(k)];
            const StoredPlane p = stored(s.normal, s.dist);
            const int plane = addPlane(p);
            const int ti = count(Texinfo, 40);
            for(const glm::dvec4& v : s.vecs)
            {
                texinfo.f32(v.x);
                texinfo.f32(v.y);
                texinfo.f32(v.z);
                texinfo.f32(v.w);
            }
            texinfo.i32(textureIndex(s.texture));
            texinfo.i32(0);
            const int firstEdge = count(Surfedges, 4);
            for(za::SizeT i = 0; i < s.poly.size(); i++)
            {
                const int a = vertexOf(s.poly[i]), c = vertexOf(s.poly[(i + 1) % s.poly.size()]);
                const int edge = count(Edges, sz.edge);
                if(wide)
                {
                    edges.u32(static_cast<unsigned>(a));
                    edges.u32(static_cast<unsigned>(c));
                }
                else
                {
                    edges.i16(a);
                    edges.i16(c);
                }
                surfedges.i32(edge);
            }
            if(wide)
            {
                faces.i32(plane);
                faces.i32(p.flip ? 1 : 0);
                faces.i32(firstEdge);
                faces.i32(static_cast<int>(s.poly.size()));
                faces.i32(ti);
            }
            else
            {
                faces.i16(plane);
                faces.i16(p.flip ? 1 : 0);
                faces.i32(firstEdge);
                faces.i16(static_cast<int>(s.poly.size()));
                faces.i16(ti);
            }
            const unsigned char styles[4] = {0, 255, 255, 255};
            faces.bytes(styles, 4);
            int lightofs = -1;
            if(options.fakeLight >= 0)
            {
                // (the engine's extents, CalcSurfaceExtents': its luxels' count; a row and a column to spare)
                double lo2[2]{1e30, 1e30}, hi2[2]{-1e30, -1e30};
                for(const glm::dvec3& q : s.poly)
                {
                    for(int a = 0; a < 2; a++)
                    {
                        const double v = static_cast<double>(static_cast<float>(q.x)) * static_cast<float>(s.vecs[a].x) +
                                         static_cast<double>(static_cast<float>(q.y)) * static_cast<float>(s.vecs[a].y) +
                                         static_cast<double>(static_cast<float>(q.z)) * static_cast<float>(s.vecs[a].z) +
                                         static_cast<float>(s.vecs[a].w);
                        lo2[a] = za::min(lo2[a], v);
                        hi2[a] = za::max(hi2[a], v);
                    }
                }
                const int sw = static_cast<int>(za::ceil(hi2[0] / 16.0) - za::floor(lo2[0] / 16.0)) + 2;
                const int th = static_cast<int>(za::ceil(hi2[1] / 16.0) - za::floor(lo2[1] / 16.0)) + 2;
                lightofs = static_cast<int>(lumps[Lighting].size());
                lumps[Lighting].resize(lumps[Lighting].size() + static_cast<za::SizeT>(sw * th), static_cast<unsigned char>(options.fakeLight));
            }
            faces.i32(lightofs);
            // Hull 0's node over this face's plane: outside, the empty leaf; inside, the next node (the last: solid).
            const int inside = k + 1 < numSurfaces ? firstNode + k + 1 : leafChild(0);
            const int front = p.flip ? inside : leafChild(emptyLeaf), back = p.flip ? leafChild(emptyLeaf) : inside;
            nodes.i32(plane);
            if(wide)
            {
                nodes.i32(front);
                nodes.i32(back);
            }
            else
            {
                nodes.i16(front);
                nodes.i16(back);
            }
            for(int a = 0; a < 3; a++)
            {
                bound(nodes, shape.lo[a] - 1.0, false);
            }
            for(int a = 0; a < 3; a++)
            {
                bound(nodes, shape.hi[a] + 1.0, true);
            }
            if(wide)
            {
                nodes.u32(static_cast<unsigned>(firstFace + k));
                nodes.u32(1u);
            }
            else
            {
                nodes.i16(firstFace + k);
                nodes.i16(1);
            }
        }
        // Hulls 1 and 2: chains of clipnodes (outside empty, inside the next; the last's inside solid).
        int heads[2]{0, 0};
        for(int h = 0; h < 2; h++)
        {
            const za::Vector<glm::dvec4>& set = shape.hulls[h];
            heads[h] = count(Clipnodes, sz.clipnode);
            for(za::SizeT k = 0; k < set.size(); k++)
            {
                const StoredPlane p = stored(glm::dvec3{set[k]}, set[k].w);
                const int plane = addPlane(p);
                const int inside = k + 1 < set.size() ? heads[h] + static_cast<int>(k) + 1 : CONTENTS_SOLID;
                const int front = p.flip ? inside : CONTENTS_EMPTY, back = p.flip ? CONTENTS_EMPTY : inside;
                clipnodes.i32(plane);
                if(wide)
                {
                    clipnodes.i32(front);
                    clipnodes.i32(back);
                }
                else
                {
                    clipnodes.i16(front);
                    clipnodes.i16(back);
                }
            }
        }
        for(int a = 0; a < 3; a++)
        {
            models.f32(shape.lo[a] - 1.0);
        }
        for(int a = 0; a < 3; a++)
        {
            models.f32(shape.hi[a] + 1.0);
        }
        for(int a = 0; a < 3; a++)
        {
            models.f32(0.0);
        }
        models.i32(firstNode);
        models.i32(heads[0]);
        models.i32(heads[1]);
        models.i32(0);
        models.i32(0); // (visleafs)
        models.i32(firstFace);
        models.i32(numSurfaces);
        entities += va("{\n\"classname\" \"%s\"\n\"model\" \"*%d\"\n\"_shadow\" \"1\"\n}\n", options.classname, model);
        notes += va("// qvr_brushbake *%d %016llx %d\n", model, brushes::bakeHash(b), numSurfaces);
        made++;
    });
    if(!made)
    {
        error = "no brushes";
        return false;
    }
    // BSP29's short indices: its vertices (edges), planes and texinfo (faces), nodes, clipnodes and leaves.
    if(!wide && (count(Vertexes, 12) > 65535 || count(Planes, 20) > 32767 || count(Texinfo, 40) > 32767 ||
                    count(Nodes, sz.node) + count(Leafs, sz.leaf) > 65535 || count(Clipnodes, sz.clipnode) > 32767 ||
                    count(Faces, sz.face) > 65535))
    {
        error = "the map (BSP29) has no room for them";
        return false;
    }
    if(options.entitiesIn)
    {
        // (the map's entities, theirs and the notes)
        za::SizeT n = 0;
        while(n < lumps[Entities].size() && lumps[Entities][n])
        {
            n++;
        }
        za::String text{reinterpret_cast<const char*>(lumps[Entities].data()), n};
        text += entities;
        text += notes;
        lumps[Entities].resize(text.size() + 1);
        memcpy(lumps[Entities].data(), text.cStr(), text.size() + 1);
    }
    // The file: its header, its 15 lumps one after another (no BSPX: light makes its own).
    Writer w{out};
    w.bytes(data, 4);
    out.resize(4 + numLumps * 8, 0);
    for(int i = 0; i < numLumps; i++)
    {
        while(out.size() % 4)
        {
            out.pushBack(0);
        }
        const int at = static_cast<int>(out.size());
        w.bytes(lumps[i].data(), lumps[i].size());
        const int o = LittleLong(at), l = LittleLong(static_cast<int>(lumps[i].size()));
        memcpy(out.data() + 4 + i * 8, &o, 4);
        memcpy(out.data() + 8 + i * 8, &l, 4);
    }
    while(out.size() % 4)
    {
        out.pushBack(0);
    }
    return true;
}

bool fromWorld(const brushes::Brush& b, int faces, za::Vector<brushlight::Map>& maps, brushlight::Texels& texels)
{
    const Baked& bk = bakedOfWorld();
    const qmodel_t* w = cl.worldmodel;
    if(bk.list.empty() || !w || !w->lightdata)
    {
        return false;
    }
    const unsigned long long hash = brushes::bakeHash(b);
    const BakedEntry* entry = nullptr;
    for(const BakedEntry& e : bk.list)
    {
        if(e.hash == hash && e.faces == faces)
        {
            entry = &e;
        }
    }
    if(!entry || entry->model <= 0 || entry->model >= w->numsubmodels)
    {
        return false;
    }
    const dmodel_t& sub = w->submodels[entry->model];
    if(sub.numfaces != faces || sub.firstface < 0 || sub.firstface + faces > w->numsurfaces)
    {
        return false;
    }
    const msurface_t* surfs = w->surfaces + sub.firstface;
    for(int k = 0; k < faces; k++)
    {
        if(!surfs[k].samples || surfs[k].styles[0] == 255)
        {
            return false;
        }
    }
    // Their maps as light.exe made them (16 texture units a luxel), every other luxel kept until they fit.
    for(int every = 1; every <= 16; every *= 2)
    {
        maps.clear();
        for(int k = 0; k < faces; k++)
        {
            const msurface_t& s = surfs[k];
            brushlight::Map m;
            const int smax = (s.extents[0] >> 4) + 1, tmax = (s.extents[1] >> 4) + 1;
            m.w = (smax - 1) / every + 1;
            m.h = (tmax - 1) / every + 1;
            m.step = 16.0 * every;
            m.s0 = s.texturemins[0];
            m.t0 = s.texturemins[1];
            m.numStyles = 0;
            for(int i = 0; i < 4; i++)
            {
                m.styles[i] = s.styles[i];
                m.numStyles += s.styles[i] != 255 ? 1 : 0;
            }
            maps.pushBack(m);
        }
        if(!brushlight::pack(maps))
        {
            continue;
        }
        texels.light.clear();
        texels.light.resize(static_cast<za::SizeT>(brushlight::rectWidth * brushlight::rectHeight), 0xff000000u);
        texels.lux.clear();
        texels.lux.resize(static_cast<za::SizeT>(brushlight::rectWidth * brushlight::rectHeight), 0xffff8080u);
        for(int k = 0; k < faces; k++)
        {
            const msurface_t& s = surfs[k];
            const brushlight::Map& m = maps[static_cast<za::SizeT>(k)];
            const int smax = (s.extents[0] >> 4) + 1, tmax = (s.extents[1] >> 4) + 1, size = smax * tmax;
            for(int st = 0; st < m.numStyles; st++)
            {
                for(int j = 0; j < m.h; j++)
                {
                    for(int i = 0; i < m.w; i++)
                    {
                        const int si = za::min(i * every, smax - 1), sj = za::min(j * every, tmax - 1);
                        const byte* rgb = s.samples + (st * size + sj * smax + si) * 3;
                        texels.light[static_cast<za::SizeT>((m.y + j) * brushlight::rectWidth + m.x + st * m.w + i)] =
                            rgb[0] | (rgb[1] << 8) | (rgb[2] << 16) | 0xff000000u;
                        if(st == 0 && s.luxsamples)
                        {
                            texels.lux[static_cast<za::SizeT>((m.y + j) * brushlight::rectWidth + m.x + i)] =
                                luxTexel(s, s.luxsamples + (sj * smax + si) * 3);
                        }
                    }
                }
            }
        }
        return true;
    }
    return false;
}

int bakedCount()
{
    return static_cast<int>(bakedOfWorld().list.size());
}

} // namespace qvr::brushbake
