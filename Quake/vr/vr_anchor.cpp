#include "vr_alloccount.h"
// vr_anchor.cpp -- see vr_anchor.hpp.

#include "vr_anchor.hpp"
#include "vr_engine.hpp"
#include "vr_mdldequant.hpp"
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

// ----------------------------------------------------------------------------
// The model file: what the in-memory model no longer has (its triangles), and its rest pose.

struct Triangle
{
    int vert[3];
};

// An anchor bound to a triangle of the rest pose: barycentric weights over its three vertices.
struct Bound
{
    int vert[3]{0, 0, 0};
    float weight[3]{1.f, 0.f, 0.f};
    int tri{-1};         // the triangle bound to (-1: not an anchor's text: the model's first vertex)
    float distance{0.f}; // from the anchor's point to the surface (model units)
    bool corner{false};  // snapped to a corner of the triangle
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

// A model file's triangles and its rest pose (frame 0, a group's first pose) in its own coordinates (scale and origin
// applied), with the anchors bound on it so far (by their text).
struct Mesh
{
    za::String name;
    bool ok{false};
    int numverts{0};
    za::Vector<Triangle> tris;
    za::Vector<glm::vec3> rest;
    za::Vector<Binding> bindings;
};
[[nodiscard]] za::SizeT heldBytes(const Mesh& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.name) + mem::heldBytes(m.tris) + mem::heldBytes(m.rest) + mem::heldBytes(m.bindings);
}

// Reads `modelName`'s triangles and rest pose. False: no such Quake model file (or a broken one).
[[nodiscard]] bool loadMesh(const char* modelName, Mesh& out)
{
    out.ok = false;
    out.numverts = 0;
    out.tris.clear();
    out.rest.clear();
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
        for(int k = 0; k < 3; k++) // (after facesfront)
        {
            out.tris[i].vert[k] = CLAMP(0, readInt(ofs + 4 + k * 4), numverts - 1);
        }
    }

    // The first frame: a simple frame (type 0, then bboxmin, bboxmax, name[16], the vertices), or a group (its count,
    // bboxes, intervals, then its frames: the first is the rest pose).
    constexpr int frameHeader = 4 + 4 + 16;
    if(ofs + 4 <= size)
    {
        const bool group = readInt(ofs) != 0;
        const int count = group ? (ofs + 8 <= size ? readInt(ofs + 4) : 0) : 1;
        ofs += group ? 4 + 4 + 8 + count * 4 : 4;
        if(count > 0 && ofs + frameHeader + numverts * 4 <= size)
        {
            out.rest.resize(numverts);
            for(int v = 0; v < numverts; v++)
            {
                const byte* t = data + ofs + frameHeader + v * 4;
                out.rest[v] = glm::vec3{t[0], t[1], t[2]} * scale + origin;
            }
        }
    }
    VR_HeapFree(data);
    if(out.rest.empty())
    {
        return false;
    }
    out.numverts = numverts;
    out.ok = true;
    return true;
}

// ----------------------------------------------------------------------------
// Anchor text: "x y z" (separated by spaces, tabs, commas or '_', a motion take header's: vr_motion.cpp).

[[nodiscard]] bool parse(const char* text, glm::vec3& point)
{
    float n[3]{};
    int count = 0;
    const char* p = text ? text : "";
    while(count < 3)
    {
        while(*p == ' ' || *p == '\t' || *p == ',' || *p == '_')
        {
            p++;
        }
        char* end = nullptr;
        const float f = strtof(p, &end);
        if(!*p || end == p)
        {
            break;
        }
        n[count++] = f;
        p = end;
    }
    point = {n[0], n[1], n[2]};
    return count == 3;
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

[[nodiscard]] za::String pointText(const glm::vec3& p)
{
    za::String out;
    appendNumber(out, p.x);
    appendNumber(out, p.y);
    appendNumber(out, p.z);
    return out;
}

// ----------------------------------------------------------------------------
// Binding a point to the surface.

// Points this near a triangle's corner are on it (the text's rounding is 0.00005 a coordinate).
constexpr float snapDistance = 0.001f;
// Triangles this much nearer than the nearest are as near (the first is taken).
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
    const Closest c = closestOnTriangle(dp, glm::dvec3{m.rest[tri.vert[0]]}, glm::dvec3{m.rest[tri.vert[1]]},
        glm::dvec3{m.rest[tri.vert[2]]});
    b.distance = static_cast<float>(glm::distance(dp, c.point));
    int corner = -1; // the nearest corner it is on (of corners in one place, the first)
    float cornerDistance = snapDistance;
    for(int k = 0; k < 3; k++)
    {
        b.vert[k] = tri.vert[k];
        b.weight[k] = static_cast<float>(c.w[k]);
        const float d = glm::distance(p, m.rest[tri.vert[k]]);
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

// The bindings the point could take: the triangles as near as the nearest (within the text's rounding), and of them
// those with the point on a corner if any (a vertex anchor: not a triangle of another part passing that close), in the
// file's order.
void candidates(const Mesh& m, const glm::vec3& p, za::Vector<Bound>& out)
{
    za::Vector<Bound> all;
    all.reserve(m.tris.size());
    float nearest = 1e30f;
    bool anyCorner = false;
    for(int t = 0; t < static_cast<int>(m.tris.size()); t++)
    {
        all.pushBack(bindTriangle(m, t, p));
        nearest = glm::min(nearest, all.back().distance);
    }
    for(const Bound& b : all)
    {
        anyCorner = anyCorner || (b.distance <= nearest + tieDistance && b.corner);
    }
    out.clear();
    for(const Bound& b : all)
    {
        if(b.distance <= nearest + tieDistance && (!anyCorner || b.corner))
        {
            out.pushBack(b);
        }
    }
}

// The point's binding: the first of its candidates.
[[nodiscard]] Bound bind(const Mesh& m, const glm::vec3& p)
{
    za::Vector<Bound> tied;
    candidates(m, p, tied);
    return tied.empty() ? Bound{} : tied[0];
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

// The binding of `text` on `m` (bound the first time; not an anchor's text: the first vertex).
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
    glm::vec3 p;
    const bool ok = parse(t, p);
    if(!ok && *t)
    {
        // (once per model and text: the binding is kept; an old config's vertex index, a hand-typed value)
        Con_Printf("Warning: %s: anchor \"%s\" is not three numbers (x y z): it sits on the model's first vertex\n",
            m.name.cStr(), t);
    }
    m.bindings.pushBack(Binding{za::String{t}, ok ? bind(m, p) : Bound{}});
    return m.bindings.back().bound;
}

[[nodiscard]] glm::vec3 poseVertex(const aliashdr_t* hdr, int pose, int vertex)
{
    return mdldequant::poseVertex(hdr, pose, vertex); // as drawn: dequantised (vr_mdl_dequant)
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

bool isAnchor(const char* text)
{
    glm::vec3 p;
    return parse(text, p);
}

namespace
{

// The command's model (its first argument), its header and its mesh; nullptr (said why) without one.
[[nodiscard]] Mesh* commandMesh(qmodel_t*& model, const aliashdr_t*& hdr)
{
    model = Mod_ForName(Cmd_Argv(1), false);
    hdr = nullptr;
    Mesh* m = nullptr;
    if(model && model->type == mod_alias)
    {
        hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
        m = hdr->poseverttype == aliashdr_t::PV_QUAKE1 && hdr->vertexes ? meshFor(model, hdr) : nullptr;
    }
    if(!m)
    {
        Con_Printf("%s: not a Quake alias model with a file matching it\n", Cmd_Argv(1));
    }
    return m;
}

[[nodiscard]] glm::vec3 inPose(const aliashdr_t* hdr, const Bound& b, int pose)
{
    glm::vec3 p{0.f};
    for(int k = 0; k < 3; k++)
    {
        p += poseVertex(hdr, pose, b.vert[k]) * b.weight[k];
    }
    return p;
}

// Where `b` (the binding of `p`) is at rest, and a note naming the other triangles as near that would put the point
// elsewhere in some pose (two parts touching there: the file's triangle order picks; a UV seam's twins move together).
void describe(const aliashdr_t* hdr, const Mesh& m, const glm::vec3& p, const Bound& b, glm::vec3& at, za::String& note)
{
    at = m.rest[b.vert[0]] * b.weight[0] + m.rest[b.vert[1]] * b.weight[1] + m.rest[b.vert[2]] * b.weight[2];
    za::Vector<Bound> tied;
    candidates(m, p, tied);
    int ties = 0;
    for(const Bound& t : tied)
    {
        for(int pose = 0; pose < hdr->numposes; pose++)
        {
            if(glm::distance(inPose(hdr, t, pose), inPose(hdr, b, pose)) > 1e-3f)
            {
                ties++;
                break;
            }
        }
    }
    note = ties ? va(" (%d other triangles as near move elsewhere: the file's order picks; a point off the seam is steadier)",
                      ties)
                : "";
}

void anchorInfo_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_anchor_info <model> <x> <y> <z>\n");
        return;
    }
    qmodel_t* model = nullptr;
    const aliashdr_t* hdr = nullptr;
    Mesh* m = commandMesh(model, hdr);
    if(!m)
    {
        return;
    }
    za::String text;
    for(int i = 2; i < Cmd_Argc(); i++)
    {
        text += (i > 2 ? " " : "") + za::String{Cmd_Argv(i)};
    }
    glm::vec3 p;
    if(!parse(text.cStr(), p))
    {
        Con_Printf("%s: \"%s\" is not an anchor (x y z)\n", model->name, text.cStr());
        return;
    }
    const Bound& b = boundFor(*m, text.cStr());
    glm::vec3 at;
    za::String note;
    describe(hdr, *m, p, b, at, note);
    Con_Printf("%s anchor \"%s\": triangle %d, vertices %d %d %d weights %.4f %.4f %.4f, %.4f from the surface at %.4f %.4f "
               "%.4f%s\n",
        model->name, text.cStr(), b.tri, b.vert[0], b.vert[1], b.vert[2], b.weight[0], b.weight[1], b.weight[2], b.distance,
        at.x, at.y, at.z, note.cStr());
}

void anchorNearest_f()
{
    if(Cmd_Argc() != 5)
    {
        Con_Printf("usage: vr_anchor_nearest <model> <x> <y> <z>\n");
        return;
    }
    qmodel_t* model = nullptr;
    const aliashdr_t* hdr = nullptr;
    Mesh* m = commandMesh(model, hdr);
    if(!m)
    {
        return;
    }
    const glm::vec3 target{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    glm::vec3 at;
    za::String note;
    describe(hdr, *m, target, bind(*m, target), at, note);
    Con_Printf("%s nearest surface point: \"%s\" (%.4f away)%s\n", model->name, pointText(at).cStr(),
        glm::distance(at, target), note.cStr());
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
}

} // namespace qvr::anchor
