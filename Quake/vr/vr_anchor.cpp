#include "vr_alloccount.h"
// vr_anchor.cpp -- see vr_anchor.hpp.

#include "vr_anchor.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::anchor
{
namespace
{

struct Triangle
{
    int facesfront;
    int vertindex[3];
};

// ----------------------------------------------------------------------------
// QuakeSpasm's BuildTris strip/fan search (gl_mesh.c before QuakeSpasm 0.93), reduced to
// computing the vertex order. Kept structurally identical so the order matches exactly.

class StripBuilder
{
public:
    explicit StripBuilder(const za::Vector<Triangle>& tris) : triangles(tris), used(tris.size(), 0)
    {
    }

    [[nodiscard]] za::Vector<int> buildVertexOrder()
    {
        za::Vector<int> order;
        const int numtris = static_cast<int>(triangles.size());

        for(int i = 0; i < numtris; i++)
        {
            if(used[i])
            {
                continue;
            }

            int bestlen = 0;
            int bestverts[1024];
            int besttris[1024];

            for(int type = 0; type < 2; type++)
            {
                for(int startv = 0; startv < 3; startv++)
                {
                    const int len = type == 1 ? stripLength(i, startv) : fanLength(i, startv);
                    if(len > bestlen)
                    {
                        bestlen = len;
                        for(int j = 0; j < bestlen + 2; j++)
                        {
                            bestverts[j] = stripverts[j];
                        }
                        for(int j = 0; j < bestlen; j++)
                        {
                            besttris[j] = striptris[j];
                        }
                    }
                }
            }

            for(int j = 0; j < bestlen; j++)
            {
                used[besttris[j]] = 1;
            }

            for(int j = 0; j < bestlen + 2; j++)
            {
                order.pushBack(bestverts[j]);
            }
        }

        return order;
    }

private:
    const za::Vector<Triangle>& triangles;
    za::Vector<int> used;
    int stripverts[128];
    int striptris[128];
    int stripcount{0};

    void clearTempUsed(int starttri)
    {
        for(int j = starttri + 1; j < static_cast<int>(triangles.size()); j++)
        {
            if(used[j] == 2)
            {
                used[j] = 0;
            }
        }
    }

    [[nodiscard]] int stripLength(int starttri, int startv)
    {
        used[starttri] = 2;
        const Triangle& last = triangles[starttri];

        stripverts[0] = last.vertindex[startv % 3];
        stripverts[1] = last.vertindex[(startv + 1) % 3];
        stripverts[2] = last.vertindex[(startv + 2) % 3];
        striptris[0] = starttri;
        stripcount = 1;

        int m1 = last.vertindex[(startv + 2) % 3];
        int m2 = last.vertindex[(startv + 1) % 3];

        bool extended = true;
        while(extended && stripcount < 126)
        {
            extended = false;
            for(int j = starttri + 1; j < static_cast<int>(triangles.size()) && !extended; j++)
            {
                const Triangle& check = triangles[j];
                if(check.facesfront != last.facesfront)
                {
                    continue;
                }

                for(int k = 0; k < 3; k++)
                {
                    if(check.vertindex[k] != m1 || check.vertindex[(k + 1) % 3] != m2)
                    {
                        continue;
                    }

                    if(used[j])
                    {
                        clearTempUsed(starttri);
                        return stripcount;
                    }

                    if(stripcount & 1)
                    {
                        m2 = check.vertindex[(k + 2) % 3];
                    }
                    else
                    {
                        m1 = check.vertindex[(k + 2) % 3];
                    }

                    stripverts[stripcount + 2] = check.vertindex[(k + 2) % 3];
                    striptris[stripcount] = j;
                    stripcount++;
                    used[j] = 2;
                    extended = true;
                    break;
                }
            }
        }

        clearTempUsed(starttri);
        return stripcount;
    }

    [[nodiscard]] int fanLength(int starttri, int startv)
    {
        used[starttri] = 2;
        const Triangle& last = triangles[starttri];

        stripverts[0] = last.vertindex[startv % 3];
        stripverts[1] = last.vertindex[(startv + 1) % 3];
        stripverts[2] = last.vertindex[(startv + 2) % 3];
        striptris[0] = starttri;
        stripcount = 1;

        const int m1 = last.vertindex[startv % 3];
        int m2 = last.vertindex[(startv + 2) % 3];

        bool extended = true;
        while(extended && stripcount < 126)
        {
            extended = false;
            for(int j = starttri + 1; j < static_cast<int>(triangles.size()) && !extended; j++)
            {
                const Triangle& check = triangles[j];
                if(check.facesfront != last.facesfront)
                {
                    continue;
                }

                for(int k = 0; k < 3; k++)
                {
                    if(check.vertindex[k] != m1 || check.vertindex[(k + 1) % 3] != m2)
                    {
                        continue;
                    }

                    if(used[j])
                    {
                        clearTempUsed(starttri);
                        return stripcount;
                    }

                    m2 = check.vertindex[(k + 2) % 3];
                    stripverts[stripcount + 2] = m2;
                    striptris[stripcount] = j;
                    stripcount++;
                    used[j] = 2;
                    extended = true;
                    break;
                }
            }
        }

        clearTempUsed(starttri);
        return stripcount;
    }
};

// ----------------------------------------------------------------------------
// The model file: what the in-memory model no longer has (its triangles), and its rest pose.

// A model file's triangles and its rest pose (frame 0, a group's first pose) in its own coordinates (scale and origin
// applied), with the anchors bound on it so far (by their text).
struct Bound
{
    int vert[3]{0, 0, 0};
    float weight[3]{1.f, 0.f, 0.f};
    int tri{-1};           // the triangle bound to (-1: a legacy anchor's vertex)
    float distance{0.f};   // from the anchor's point to the surface (model units)
    int ties{0};           // other triangles as near that bind elsewhere (a hint picks among them)
    bool corner{false};    // snapped to a corner of the triangle
    bool legacy{false};
};

struct Binding
{
    za::String text;
    Bound bound;
};
[[nodiscard]] za::SizeT heldBytes(const Binding& b) // (vr_mem.hpp)
{
    return mem::heldBytes(b.text);
}

struct Mesh
{
    za::String name;
    bool ok{false};
    int numverts{0};
    za::Vector<Triangle> tris;
    za::Vector<glm::vec3> rest;
    za::Vector<glm::vec3> poses; // every pose (legacyToText's check only)
    za::Vector<int> order; // the strip order (legacy anchors), built when one asks
    bool orderBuilt{false};
    za::Vector<Binding> bindings;
};
[[nodiscard]] za::SizeT heldBytes(const Mesh& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.name) + mem::heldBytes(m.tris) + mem::heldBytes(m.rest) + mem::heldBytes(m.poses) + mem::heldBytes(m.order) +
           mem::heldBytes(m.bindings);
}

// Reads `modelName`'s triangles and rest pose (and with `allPoses` every pose, one after another). False: no such Quake model file (or a broken one).
[[nodiscard]] bool loadMesh(const char* modelName, Mesh& out, bool allPoses = false)
{
    out.ok = false;
    out.numverts = 0;
    out.tris.clear();
    out.rest.clear();
    out.poses.clear();
    out.order.clear();
    out.orderBuilt = false;
    out.bindings.clear();

    byte* data = COM_LoadMallocFile(modelName, nullptr);
    if(!data)
    {
        return false;
    }
    const int size = com_filesize;

    const auto readInt = [&](int offset) {
        int v;
        memcpy(&v, data + offset, sizeof(v));
        return LittleLong(v);
    };
    const auto readFloat = [&](int offset) {
        float v;
        memcpy(&v, data + offset, sizeof(v));
        return LittleFloat(v);
    };

    // mdl_t: ident, version, scale[3], scale_origin[3], boundingradius, eyeposition[3],
    // numskins, skinwidth, skinheight, numverts, numtris, numframes, synctype, flags, size.
    constexpr int headerSize = 84;
    if(size < headerSize || readInt(0) != IDPOLYHEADER)
    {
        VR_HeapFree(data);
        return false;
    }

    const glm::vec3 scale{readFloat(8), readFloat(12), readFloat(16)};
    const glm::vec3 origin{readFloat(20), readFloat(24), readFloat(28)};
    const int numskins = readInt(48);
    const int skinwidth = readInt(52);
    const int skinheight = readInt(56);
    const int numverts = readInt(60);
    const int numtris = readInt(64);
    const int numframes = readInt(68);
    const int skinsize = skinwidth * skinheight;
    if(numverts <= 0 || numtris <= 0 || numframes <= 0 || skinsize < 0)
    {
        VR_HeapFree(data);
        return false;
    }

    int ofs = headerSize;
    for(int i = 0; i < numskins && ofs + 4 <= size; i++)
    {
        const int type = readInt(ofs);
        ofs += 4;
        if(type == 0)
        {
            ofs += skinsize;
        }
        else
        {
            const int groupSkins = readInt(ofs);
            ofs += 4 + groupSkins * 4 + groupSkins * skinsize;
        }
    }

    ofs += numverts * 12; // stverts: onseam, s, t

    if(ofs < 0 || ofs + numtris * 16 > size)
    {
        VR_HeapFree(data);
        return false;
    }

    out.tris.resize(numtris);
    for(int i = 0; i < numtris; i++, ofs += 16)
    {
        out.tris[i].facesfront = readInt(ofs);
        for(int k = 0; k < 3; k++)
        {
            out.tris[i].vertindex[k] = CLAMP(0, readInt(ofs + 4 + k * 4), numverts - 1);
        }
    }

    // The frames: a simple frame (type 0, then bboxmin, bboxmax, name[16], the vertices), or a group (its count, bboxes,
    // intervals, then its frames). The rest pose is the first (frame 0, a group's first); every pose only if asked.
    constexpr int frameHeader = 4 + 4 + 16;
    const auto readPose = [&](int at, glm::vec3* to) {
        for(int v = 0; v < numverts; v++)
        {
            const byte* t = data + at + v * 4;
            to[v] = glm::vec3{t[0], t[1], t[2]} * scale + origin;
        }
    };
    const int poseBytes = frameHeader + numverts * 4;
    for(int frame = 0; frame < numframes; frame++)
    {
        if(ofs + 4 > size)
        {
            break;
        }
        const bool group = readInt(ofs) != 0;
        const int count = group ? (ofs + 8 <= size ? readInt(ofs + 4) : 0) : 1;
        ofs += group ? 4 + 4 + 8 + count * 4 : 4;
        if(count <= 0 || ofs + count * poseBytes > size)
        {
            break;
        }
        for(int i = 0; i < count; i++, ofs += poseBytes)
        {
            if(out.rest.empty())
            {
                out.rest.resize(numverts);
                readPose(ofs + frameHeader, out.rest.data());
            }
            if(!allPoses)
            {
                break;
            }
            const za::SizeT first = out.poses.size();
            out.poses.resize(first + static_cast<za::SizeT>(numverts));
            readPose(ofs + frameHeader, out.poses.data() + first);
        }
        if(!allPoses)
        {
            break;
        }
    }
    if(out.rest.empty())
    {
        VR_HeapFree(data);
        return false;
    }

    VR_HeapFree(data);
    out.numverts = numverts;
    out.ok = true;
    return true;
}

[[nodiscard]] const za::Vector<int>& stripOrder(Mesh& m)
{
    if(!m.orderBuilt)
    {
        m.order = StripBuilder{m.tris}.buildVertexOrder();
        m.orderBuilt = true;
    }
    return m.order;
}

// ----------------------------------------------------------------------------
// Anchor text.

struct Spec
{
    bool legacy{true};
    int index{0};          // a legacy anchor's
    glm::vec3 point{0.f};  // a point's
    int hint{-1};
};

[[nodiscard]] Spec parse(const char* text)
{
    Spec s;
    if(!text)
    {
        return s;
    }
    float n[4]{};
    int count = 0;
    const char* p = text;
    while(count < 4)
    {
        while(*p == ' ' || *p == '\t' || *p == ',' || *p == '_') // ('_': a motion take header's, vr_motion.cpp)
        {
            p++;
        }
        if(!*p)
        {
            break;
        }
        char* end = nullptr;
        const float f = strtof(p, &end);
        if(end == p)
        {
            break;
        }
        n[count++] = f;
        p = end;
    }
    if(count >= 3)
    {
        s.legacy = false;
        s.point = {n[0], n[1], n[2]};
        s.hint = count >= 4 ? static_cast<int>(n[3]) : -1;
        return s;
    }
    s.index = count >= 1 ? static_cast<int>(n[0]) : 0;
    return s;
}

// A coordinate as anchors write it: 4 decimals (well under a byte step of any model), no trailing zeros.
void appendNumber(za::String& out, float f)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(f));
    int len = static_cast<int>(strlen(buf));
    while(len > 0 && buf[len - 1] == '0')
    {
        buf[--len] = 0;
    }
    if(len > 0 && buf[len - 1] == '.')
    {
        buf[--len] = 0;
    }
    if(!strcmp(buf, "-0"))
    {
        strcpy(buf, "0");
    }
    if(!out.empty())
    {
        out += " ";
    }
    out += buf;
}

[[nodiscard]] za::String pointText(const glm::vec3& p, int hint)
{
    za::String out;
    appendNumber(out, p.x);
    appendNumber(out, p.y);
    appendNumber(out, p.z);
    if(hint >= 0)
    {
        out += " ";
        out += va("%d", hint);
    }
    return out;
}

// ----------------------------------------------------------------------------
// Binding a point to the surface.

// Points this near a triangle's corner are on it (the text's rounding is 0.00005 a coordinate).
constexpr float snapDistance = 0.001f;
// Triangles this much nearer than the nearest are as near (the first, or the hint, is taken).
constexpr float tieDistance = 0.001f;

struct Closest
{
    glm::dvec3 point{0.0};
    double w[3]{1.0, 0.0, 0.0};
};

// The point of triangle abc nearest p, and its barycentric weights (Ericson, Real-Time Collision Detection 5.1.5).
[[nodiscard]] Closest closestOnTriangle(const glm::dvec3& p, const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& c)
{
    const glm::dvec3 ab = b - a;
    const glm::dvec3 ac = c - a;
    const glm::dvec3 ap = p - a;
    const double d1 = glm::dot(ab, ap);
    const double d2 = glm::dot(ac, ap);
    if(d1 <= 0.0 && d2 <= 0.0)
    {
        return {a, {1.0, 0.0, 0.0}};
    }
    const glm::dvec3 bp = p - b;
    const double d3 = glm::dot(ab, bp);
    const double d4 = glm::dot(ac, bp);
    if(d3 >= 0.0 && d4 <= d3)
    {
        return {b, {0.0, 1.0, 0.0}};
    }
    const double vc = d1 * d4 - d3 * d2;
    if(vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0 && d1 - d3 > 0.0)
    {
        const double v = d1 / (d1 - d3);
        return {a + v * ab, {1.0 - v, v, 0.0}};
    }
    const glm::dvec3 cp = p - c;
    const double d5 = glm::dot(ab, cp);
    const double d6 = glm::dot(ac, cp);
    if(d6 >= 0.0 && d5 <= d6)
    {
        return {c, {0.0, 0.0, 1.0}};
    }
    const double vb = d5 * d2 - d1 * d6;
    if(vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0 && d2 - d6 > 0.0)
    {
        const double w = d2 / (d2 - d6);
        return {a + w * ac, {1.0 - w, 0.0, w}};
    }
    const double va = d3 * d6 - d5 * d4;
    if(va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0 && (d4 - d3) + (d5 - d6) > 0.0)
    {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return {b + w * (c - b), {0.0, 1.0 - w, w}};
    }
    const double sum = va + vb + vc;
    if(sum <= 1e-18)
    {
        // A degenerate triangle (no area): its nearest corner.
        const double da = glm::distance(p, a);
        const double db = glm::distance(p, b);
        const double dc = glm::distance(p, c);
        if(da <= db && da <= dc)
        {
            return {a, {1.0, 0.0, 0.0}};
        }
        return db <= dc ? Closest{b, {0.0, 1.0, 0.0}} : Closest{c, {0.0, 0.0, 1.0}};
    }
    const double v = vb / sum;
    const double w = vc / sum;
    return {a + ab * v + ac * w, {1.0 - v - w, v, w}};
}

// Triangle `t`'s binding of `p`: its nearest point's weights, snapped to a corner `p` is on.
[[nodiscard]] Bound bindTriangle(const Mesh& m, int t, const glm::vec3& p)
{
    const Triangle& tri = m.tris[t];
    Bound b;
    b.tri = t;
    const glm::dvec3 dp{p};
    const Closest c = closestOnTriangle(dp, glm::dvec3{m.rest[tri.vertindex[0]]}, glm::dvec3{m.rest[tri.vertindex[1]]},
        glm::dvec3{m.rest[tri.vertindex[2]]});
    b.distance = static_cast<float>(glm::distance(dp, c.point));
    int corner = -1; // the nearest corner it is on (of corners in one place, the first: a hint picks the triangle)
    float cornerDistance = snapDistance;
    for(int k = 0; k < 3; k++)
    {
        b.vert[k] = tri.vertindex[k];
        b.weight[k] = static_cast<float>(c.w[k]);
        const float d = glm::distance(p, m.rest[tri.vertindex[k]]);
        if(d < cornerDistance)
        {
            corner = k;
            cornerDistance = d;
        }
    }
    if(corner >= 0)
    {
        for(int k = 0; k < 3; k++)
        {
            b.weight[k] = k == corner ? 1.f : 0.f;
        }
        b.distance = cornerDistance;
        b.corner = true;
    }
    return b;
}

// Whether two bindings move the same: the same vertices with the same weights (in any order).
[[nodiscard]] bool sameBinding(const Bound& a, const Bound& b)
{
    for(int i = 0; i < 3; i++)
    {
        if(a.weight[i] < 1e-6f)
        {
            continue;
        }
        float w = 0.f;
        for(int j = 0; j < 3; j++)
        {
            if(b.vert[j] == a.vert[i])
            {
                w += b.weight[j];
            }
        }
        if(glm::abs(w - a.weight[i]) > 1e-4f)
        {
            return false;
        }
    }
    return true;
}

// The bindings `s` could take: the triangles as near as the nearest (within the text's rounding) and, if any of them puts
// the point on a corner (a vertex anchor), only those (not a triangle of another part passing that close), in the file's
// order; the hint among them even if it doesn't.
void candidates(const Mesh& m, const Spec& s, za::Vector<Bound>& out)
{
    out.clear();
    float nearest = 1e30f;
    bool anyCorner = false;
    for(int t = 0; t < static_cast<int>(m.tris.size()); t++)
    {
        nearest = glm::min(nearest, bindTriangle(m, t, s.point).distance);
    }
    for(int t = 0; t < static_cast<int>(m.tris.size()); t++)
    {
        const Bound b = bindTriangle(m, t, s.point);
        anyCorner = anyCorner || (b.distance <= nearest + tieDistance && b.corner);
    }
    for(int t = 0; t < static_cast<int>(m.tris.size()); t++)
    {
        const Bound b = bindTriangle(m, t, s.point);
        if(b.distance <= nearest + tieDistance && (!anyCorner || b.corner || t == s.hint))
        {
            out.pushBack(b);
        }
    }
}

[[nodiscard]] Bound bind(Mesh& m, const Spec& s)
{
    Bound out;
    if(s.legacy)
    {
        const za::Vector<int>& order = stripOrder(m);
        out.legacy = true;
        out.vert[0] = out.vert[1] = out.vert[2] = order.empty() ? 0 : order[CLAMP(0, s.index, static_cast<int>(order.size()) - 1)];
        return out;
    }
    if(m.tris.empty())
    {
        return out;
    }

    za::Vector<Bound> tied;
    candidates(m, s, tied);
    // The hint if it is one of them, else the first. `ties`: the others that bind elsewhere (another vertex in the same
    // place: two parts touching there, a collapsed muzzle flash): the triangle order picks, which an edit may change.
    out = tied[0];
    for(const Bound& b : tied)
    {
        if(b.tri == s.hint)
        {
            out = b;
        }
    }
    out.ties = 0;
    for(const Bound& b : tied)
    {
        out.ties += !sameBinding(out, b);
    }
    return out;
}

// Model -> its mesh (by the name it was read for: a model's slot is reused for another after a game change).
struct AnchorCache
{
    ankerl::unordered_dense::map<const qmodel_t*, Mesh> meshes;
    auto members() { return mem::list(meshes); }
};
mem::Cache<AnchorCache> anchorCache{"anchors", mem::GameDirChange | mem::ModelReload};

// The model's mesh (read the first time; nullptr: no Quake model file, or not this model's vertices).
[[nodiscard]] Mesh* meshFor(const qmodel_t* model, const aliashdr_t* hdr)
{
    Mesh& m = anchorCache.meshes[model];
    if(m.name != model->name)
    {
        m.name = model->name;
        (void)loadMesh(model->name, m);
    }
    return m.ok && m.numverts == hdr->numverts ? &m : nullptr;
}

// The binding of `text` on `m` (bound the first time).
[[nodiscard]] const Bound& boundFor(Mesh& m, const char* text)
{
    const char* t = text ? text : "";
    for(const Binding& b : m.bindings)
    {
        if(!strcmp(b.text.cStr(), t))
        {
            return b.bound;
        }
    }
    if(m.bindings.size() >= 64)
    {
        m.bindings.clear(); // (settings being tried: a few anchors are in use at once)
    }
    m.bindings.pushBack(Binding{za::String{t}, bind(m, parse(t))});
    return m.bindings.back().bound;
}

[[nodiscard]] glm::vec3 poseVertex(const aliashdr_t* hdr, int pose, int vertex)
{
    const trivertx_t* verts = reinterpret_cast<const trivertx_t*>(
        reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& v = verts[pose * hdr->numverts + vertex];
    return {v.v[0], v.v[1], v.v[2]};
}

// Pose index for the entity's current frame (animated for frame groups).
[[nodiscard]] int currentPose(const entity_t& ent, const aliashdr_t* hdr)
{
    const maliasframedesc_t& frame = hdr->frames[ent.frame >= 0 && ent.frame < hdr->numframes ? ent.frame : 0];
    if(frame.numposes <= 1)
    {
        return frame.firstpose;
    }

    const float interval = frame.interval > 0.f ? frame.interval : 0.1f;
    return frame.firstpose + static_cast<int>(cl.time / interval) % frame.numposes;
}

// The vertex as the renderer will draw it this frame: R_SetupAliasFrame blends from
// .previouspose to .currentpose over .lerptime, and a frame change only starts a new blend
// (from the pose shown so far) when the entity is drawn. Reads the entity without changing it.
[[nodiscard]] glm::vec3 drawnVertex(const entity_t& e, const aliashdr_t* hdr, int vertex)
{
    const int pose = currentPose(e, hdr);
    const bool lerping = r_lerpmodels.value && !(e.model->flags & MOD_NOLERP && r_lerpmodels.value != 2);
    if(!lerping || (e.lerpflags & (LERP_RESETANIM | LERP_RESETANIM2)))
    {
        return poseVertex(hdr, pose, vertex);
    }

    if(pose != e.currentpose)
    {
        return poseVertex(hdr, e.currentpose, vertex); // a new blend starts from here
    }

    float blend = 1.f;
    if(e.lerpflags & LERP_FINISH && R_FrameLerpFinish(&e) > e.lerpstart)
    {
        blend = static_cast<float>((cl.time - e.lerpstart) / (R_FrameLerpFinish(&e) - e.lerpstart));
    }
    else if(e.lerptime > 0.f)
    {
        blend = static_cast<float>((cl.time - e.lerpstart) / e.lerptime);
    }
    blend = CLAMP(0.f, blend, 1.f);

    return glm::mix(poseVertex(hdr, e.previouspose, vertex), poseVertex(hdr, e.currentpose, vertex), blend);
}

// A vertex as drawn, blended towards the zero pose by `zeroBlend`.
[[nodiscard]] glm::vec3 blendedVertex(const entity_t& e, const aliashdr_t* hdr, int vertex, float zeroBlend)
{
    const glm::vec3 posed = drawnVertex(e, hdr, vertex);
    if(zeroBlend <= 0.f)
    {
        return posed;
    }
    return glm::mix(posed, poseVertex(hdr, zeroPose(hdr), vertex), zeroBlend);
}

[[nodiscard]] const aliashdr_t* quakeHeader(const entity_t& ent)
{
    if(!ent.model || ent.model->type != mod_alias)
    {
        return nullptr;
    }
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ent.model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || hdr->numverts <= 0)
    {
        return nullptr;
    }
    return hdr;
}

// A binding's point in pose `pose` (the byte space), as the old vertex anchors read their vertex.
[[nodiscard]] glm::vec3 boundInPose(const aliashdr_t* hdr, const Bound& b, int pose)
{
    if(b.legacy)
    {
        return poseVertex(hdr, pose, b.vert[0]);
    }
    glm::vec3 p{0.f};
    for(int k = 0; k < 3; k++)
    {
        if(b.weight[k] != 0.f)
        {
            p += poseVertex(hdr, pose, b.vert[k]) * b.weight[k];
        }
    }
    return p;
}

} // namespace

int zeroPose(const aliashdr_t* hdr)
{
    const maliasframedesc_t& frame = hdr->frames[0];
    if(frame.numposes <= 1)
    {
        return frame.firstpose;
    }

    // The old engine animated zero-pose groups at a fixed 10 fps.
    return frame.firstpose + static_cast<int>(cl.time / 0.1) % frame.numposes;
}

glm::vec3 posedAnchor(const entity_t& ent, const char* anchor, float zeroBlend)
{
    const aliashdr_t* hdr = quakeHeader(ent);
    if(!hdr)
    {
        return glm::vec3{0.f};
    }
    Mesh* m = meshFor(ent.model, hdr);
    if(!m)
    {
        return blendedVertex(ent, hdr, 0, zeroBlend);
    }
    const Bound& b = boundFor(*m, anchor);
    if(b.legacy)
    {
        return blendedVertex(ent, hdr, CLAMP(0, b.vert[0], hdr->numverts - 1), zeroBlend);
    }
    glm::vec3 p{0.f};
    for(int k = 0; k < 3; k++)
    {
        if(b.weight[k] != 0.f)
        {
            p += blendedVertex(ent, hdr, b.vert[k], zeroBlend) * b.weight[k];
        }
    }
    return p;
}

glm::vec3 posedVertex(const entity_t& ent, int vertex, float zeroBlend)
{
    const aliashdr_t* hdr = quakeHeader(ent);
    if(!hdr)
    {
        return glm::vec3{0.f};
    }
    return blendedVertex(ent, hdr, CLAMP(0, vertex, hdr->numverts - 1), zeroBlend);
}

bool posedVertices(const entity_t& ent, float zeroBlend, za::Vector<glm::vec3>& rest, za::Vector<glm::vec3>& now)
{
    rest.clear();
    now.clear();
    const aliashdr_t* hdr = quakeHeader(ent);
    if(!hdr)
    {
        return false;
    }
    const int zero = zeroPose(hdr);
    rest.resize(hdr->numverts);
    now.resize(hdr->numverts);
    for(int v = 0; v < hdr->numverts; v++)
    {
        rest[v] = poseVertex(hdr, zero, v);
        now[v] = glm::mix(drawnVertex(ent, hdr, v), rest[v], CLAMP(0.f, zeroBlend, 1.f));
    }
    return true;
}

bool isLegacy(const char* anchor)
{
    return parse(anchor).legacy;
}

bool legacyToText(const char* modelName, int index, za::String& out, bool* exact)
{
    Mesh m;
    if(!loadMesh(modelName, m, true))
    {
        return false;
    }
    const za::Vector<int>& order = stripOrder(m);
    if(order.empty())
    {
        return false;
    }
    const int vertex = order[CLAMP(0, index, static_cast<int>(order.size()) - 1)];
    const za::String plain = pointText(m.rest[vertex], -1);
    const Spec s = parse(plain.cStr());
    // A binding that is where the vertex is in every pose (another vertex in the same place at rest, two parts touching
    // there, may move elsewhere).
    const int numposes = static_cast<int>(m.poses.size()) / m.numverts;
    const auto followsVertex = [&](const Bound& b) {
        for(int pose = 0; pose < numposes; pose++)
        {
            const glm::vec3* P = m.poses.data() + static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(m.numverts);
            const glm::vec3 at = P[b.vert[0]] * b.weight[0] + P[b.vert[1]] * b.weight[1] + P[b.vert[2]] * b.weight[2];
            if(glm::distance(at, P[vertex]) > 1e-4f)
            {
                return false;
            }
        }
        return true;
    };
    if(exact)
    {
        *exact = true;
    }
    // Without a hint when every binding the point could take goes where the vertex goes (the triangle order then doesn't
    // matter: an edit may renumber them).
    za::Vector<Bound> tied;
    candidates(m, s, tied);
    bool allFollow = !tied.empty();
    for(const Bound& b : tied)
    {
        allFollow = allFollow && followsVertex(b);
    }
    if(allFollow)
    {
        out = plain;
        return true;
    }
    // Another vertex in the same place (two parts touching there, a collapsed muzzle flash) that moves elsewhere: the first
    // of the vertex's triangles that binds the point to where it goes, as the hint (so the triangle order doesn't pick).
    for(int t = 0; t < static_cast<int>(m.tris.size()); t++)
    {
        const Triangle& tri = m.tris[t];
        if(tri.vertindex[0] != vertex && tri.vertindex[1] != vertex && tri.vertindex[2] != vertex)
        {
            continue;
        }
        Spec hinted = s;
        hinted.hint = t;
        if(followsVertex(bind(m, hinted)))
        {
            out = pointText(m.rest[vertex], t);
            return true;
        }
    }
    if(exact)
    {
        *exact = followsVertex(bind(m, s)); // none does: the point as it is (vr_anchor_compare says how far off)
    }
    out = plain;
    return true;
}

namespace
{

[[nodiscard]] const aliashdr_t* commandModel(qmodel_t*& model)
{
    model = Mod_ForName(Cmd_Argv(1), false);
    if(!model || model->type != mod_alias)
    {
        Con_Printf("%s: not an alias model\n", Cmd_Argv(1));
        return nullptr;
    }
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes)
    {
        Con_Printf("%s: no Quake vertices\n", Cmd_Argv(1));
        return nullptr;
    }
    return hdr;
}

// The arguments from `first` on, as one text (an anchor's numbers).
[[nodiscard]] za::String argsFrom(int first)
{
    za::String text;
    for(int i = first; i < Cmd_Argc(); i++)
    {
        if(i > first)
        {
            text += " ";
        }
        text += Cmd_Argv(i);
    }
    return text;
}

[[nodiscard]] glm::vec3 modelSpace(const aliashdr_t* hdr, const glm::vec3& v)
{
    return v * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} +
           glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
}

void printBound(const qmodel_t* model, const char* text, const Bound& b, const Mesh& m)
{
    if(b.legacy)
    {
        const glm::vec3& p = m.rest[b.vert[0]];
        Con_Printf("%s anchor \"%s\": legacy index, vertex %d at %.4f %.4f %.4f (as a point: \"%s\")\n", model->name, text,
            b.vert[0], p.x, p.y, p.z, pointText(p, -1).cStr());
        return;
    }
    glm::vec3 p{0.f};
    for(int k = 0; k < 3; k++)
    {
        p += m.rest[b.vert[k]] * b.weight[k];
    }
    Con_Printf("%s anchor \"%s\": triangle %d, vertices %d %d %d weights %.4f %.4f %.4f, %.4f from the surface at %.4f %.4f "
               "%.4f%s\n",
        model->name, text, b.tri, b.vert[0], b.vert[1], b.vert[2], b.weight[0], b.weight[1], b.weight[2], b.distance, p.x,
        p.y, p.z, b.ties ? va(" (%d other triangles as near: a hint picks)", b.ties) : "");
}

void anchorInfo_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_anchor_info <model> <anchor: x y z [triangle], or a legacy index>\n");
        return;
    }
    qmodel_t* model = nullptr;
    const aliashdr_t* hdr = commandModel(model);
    Mesh* m = hdr ? meshFor(model, hdr) : nullptr;
    if(!m)
    {
        Con_Printf("%s: no model file matching it\n", Cmd_Argv(1));
        return;
    }
    const za::String text = argsFrom(2);
    printBound(model, text.cStr(), boundFor(*m, text.cStr()), *m);
}

void anchorNearest_f()
{
    if(Cmd_Argc() != 5)
    {
        Con_Printf("usage: vr_anchor_nearest <model> <x> <y> <z>\n");
        return;
    }
    qmodel_t* model = nullptr;
    const aliashdr_t* hdr = commandModel(model);
    Mesh* m = hdr ? meshFor(model, hdr) : nullptr;
    if(!m)
    {
        Con_Printf("%s: no model file matching it\n", Cmd_Argv(1));
        return;
    }
    const glm::vec3 target{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    Spec s;
    s.legacy = false;
    s.point = target;
    const Bound b = bind(*m, s);
    glm::vec3 p{0.f};
    for(int k = 0; k < 3; k++)
    {
        p += m->rest[b.vert[k]] * b.weight[k];
    }
    const za::String text = pointText(p, b.ties ? b.tri : -1);
    Con_Printf("%s nearest surface point: \"%s\" (%.4f away; triangle %d)\n", model->name, text.cStr(),
        glm::distance(p, target), b.tri);
}

void anchorFromIndex_f()
{
    if(Cmd_Argc() != 3)
    {
        Con_Printf("usage: vr_anchor_from_index <model> <legacy anchor index>\n");
        return;
    }
    za::String text;
    bool exact = true;
    if(!legacyToText(Cmd_Argv(1), Q_atoi(Cmd_Argv(2)), text, &exact))
    {
        Con_Printf("anchor_from_index %s %s: no model file\n", Cmd_Argv(1), Cmd_Argv(2));
        return;
    }
    Con_Printf("anchor_from_index %s %s \"%s\"%s\n", Cmd_Argv(1), Cmd_Argv(2), text.cStr(),
        exact ? "" : " (inexact: no triangle follows the vertex in every pose)");
}

// Every pose: the legacy index's vertex against the anchor's point (model units). The check of a conversion.
void anchorCompare_f()
{
    if(Cmd_Argc() < 4)
    {
        Con_Printf("usage: vr_anchor_compare <model> <legacy anchor index> <anchor: x y z [triangle]>\n");
        return;
    }
    qmodel_t* model = nullptr;
    const aliashdr_t* hdr = commandModel(model);
    Mesh* m = hdr ? meshFor(model, hdr) : nullptr;
    if(!m)
    {
        Con_Printf("anchor_compare %s: no model file matching it\n", Cmd_Argv(1));
        return;
    }
    Spec legacySpec;
    legacySpec.index = Q_atoi(Cmd_Argv(2));
    const Bound old = bind(*m, legacySpec);
    const za::String text = argsFrom(3);
    const Bound now = boundFor(*m, text.cStr());
    float worst = 0.f;
    int worstPose = 0;
    for(int pose = 0; pose < hdr->numposes; pose++)
    {
        const float d = glm::distance(modelSpace(hdr, boundInPose(hdr, old, pose)), modelSpace(hdr, boundInPose(hdr, now, pose)));
        if(d > worst)
        {
            worst = d;
            worstPose = pose;
        }
    }
    Con_Printf("anchor_compare %s %s \"%s\": %d poses, max %.5f (pose %d); vertex %d -> triangle %d vertices %d %d %d "
               "weights %.3f %.3f %.3f, %.5f off the surface, %d ties\n",
        model->name, Cmd_Argv(2), text.cStr(), hdr->numposes, worst, worstPose, old.vert[0], now.tri, now.vert[0],
        now.vert[1], now.vert[2], now.weight[0], now.weight[1], now.weight[2], now.distance, now.ties);
}

} // namespace

void onGameDirChanged()
{
    anchorCache.meshes.clear();
}

void registerCommands()
{
    Cmd_AddCommand("vr_anchor_info", anchorInfo_f);
    Cmd_AddCommand("vr_anchor_nearest", anchorNearest_f);
    Cmd_AddCommand("vr_anchor_from_index", anchorFromIndex_f);
    Cmd_AddCommand("vr_anchor_compare", anchorCompare_f);
}

} // namespace qvr::anchor
