// vr_modelcollide.cpp -- see vr_modelcollide.hpp.

#include "vr_modelcollide.hpp"
#include "vr_engine.hpp"
#include "vr_api_render.h"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_flashlight.hpp"
#include "vr_grasp.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::modelcollide
{
namespace
{

constexpr int sampleCount = 24;      // the weapon's vertices the rays go to
constexpr float handRayLength = 16.f; // units behind the hand (towards the chest) the hand's own ray starts
constexpr float margin = 0.3f;        // units the weapon is held off the surface (about a centimetre)
constexpr int rounds = 3;             // tests: the first, and again from the push (a curved surface)
constexpr float easeIn = 0.012f;      // seconds (time constants): pushed out at once, nearly
constexpr float easeOut = 0.06f;      // and let back in more slowly (no snapping when the contact ends)
constexpr float fastObject = 150.f;   // units a second: a thing moving faster (thrown, flying) is not in the way

// ----------------------------------------------------------------------------
// The models' triangles: an alias model's as indices into a pose's vertices (the renderer's order: the same for every
// pose), a brush model's as its faces fanned out, in its own coordinates. Made once per model.

struct ModelTris
{
    std::string name; // a slot reused by another model after a game change is made again
    bool valid{false};
    bool alias{false};
    std::vector<std::array<std::uint16_t, 3>> tris;
    std::vector<glm::vec3> brushVerts;
};
std::unordered_map<const qmodel_t*, ModelTris> modelTris;

[[nodiscard]] const aliashdr_t* quakeAlias(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc ||
        hdr->numframes <= 0 || hdr->numverts <= 0 || hdr->numbones)
    {
        return nullptr;
    }
    return hdr;
}

const ModelTris* trianglesOf(const qmodel_t* model)
{
    ModelTris& m = modelTris[model];
    if(m.name == model->name)
    {
        return m.valid ? &m : nullptr;
    }
    m = ModelTris{};
    m.name = model->name;
    if(const aliashdr_t* hdr = quakeAlias(model))
    {
        m.alias = true;
        const auto* base = reinterpret_cast<const byte*>(hdr);
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
        for(int i = 0; i + 2 < hdr->numindexes; i += 3)
        {
            m.tris.push_back({mesh[indexes[i]].vertindex, mesh[indexes[i + 1]].vertindex, mesh[indexes[i + 2]].vertindex});
        }
    }
    else if(model->type == mod_brush)
    {
        for(int i = 0; i < model->nummodelsurfaces; i++)
        {
            const msurface_t& surf = model->surfaces[model->firstmodelsurface + i];
            const auto vertex = [&](int k) {
                const int ed = model->surfedges[surf.firstedge + k];
                const mvertex_t& v = model->vertexes[ed >= 0 ? model->edges[ed].v[0] : model->edges[-ed].v[1]];
                return glm::vec3{v.position[0], v.position[1], v.position[2]};
            };
            const auto first = static_cast<std::uint16_t>(m.brushVerts.size());
            for(int k = 0; k < surf.numedges && m.brushVerts.size() < 65535; k++)
            {
                m.brushVerts.push_back(vertex(k));
            }
            for(int k = 2; k < surf.numedges && first + k < 65535; k++)
            {
                m.tris.push_back({first, static_cast<std::uint16_t>(first + k - 1), static_cast<std::uint16_t>(first + k)});
            }
        }
    }
    m.valid = !m.tris.empty();
    return m.valid ? &m : nullptr;
}

// ----------------------------------------------------------------------------
// The things in the way, posed as the renderer draws them this frame (r_alias.c's R_SetupAliasFrame and
// R_SetupEntityTransform, read only: they run when the entity is drawn, after the view is set up, and a pose or place
// that changed since starts a lerp there from the old one: drawn at the old one this frame).

struct Posed
{
    int frame{-1}; // host_framecount it was placed for
    bool valid{false};
    const ModelTris* model{nullptr};
    const entity_t* ent{nullptr};
    glm::mat4 m{1.f};           // its model's coordinates to the world, as drawn
    int pose1{0}, pose2{0};     // an alias model's poses and the blend between them
    float blend{0.f};
    glm::vec3 lo{0.f}, hi{0.f}; // its box as drawn (its frames' bounds, or a brush model's, placed)
    int vertsFrame{-1};         // host_framecount its vertices were posed for (only when something comes near)
    std::vector<glm::vec3> verts; // world
};
std::unordered_map<int, Posed> posedCache; // by entity number

struct Lerped
{
    int pose1{0}, pose2{0};
    float blend{0.f};
    glm::vec3 origin{0.f}, angles{0.f};
};

[[nodiscard]] Lerped lerpOf(const entity_t& e, const aliashdr_t* hdr)
{
    Lerped l;
    int frame = e.frame;
    if(frame < 0 || frame >= hdr->numframes)
    {
        frame = 0;
    }
    int pose = hdr->frames[frame].firstpose;
    const int numposes = hdr->frames[frame].numposes;
    float lerptime = 0.1f;
    if(numposes > 1)
    {
        lerptime = hdr->frames[frame].interval;
        pose += static_cast<int>(cl.time / lerptime) % numposes;
    }
    const bool lerpModels = r_lerpmodels.value && !(e.model->flags & MOD_NOLERP && r_lerpmodels.value != 2);
    if(!lerpModels || (e.lerpflags & LERP_RESETANIM))
    {
        l.pose1 = l.pose2 = pose;
    }
    else if(e.currentpose != pose)
    {
        l.pose1 = l.pose2 = (e.lerpflags & LERP_RESETANIM2) ? pose : e.currentpose; // a lerp starting: at its start
    }
    else
    {
        if(e.lerpflags & LERP_FINISH && numposes == 1)
        {
            l.blend = CLAMP(0.f, static_cast<float>(cl.time - e.lerpstart) / std::fmax(e.lerpfinish - e.lerpstart, 1e-4f), 1.f);
        }
        else
        {
            l.blend = CLAMP(0.f, static_cast<float>(cl.time - e.lerpstart) / lerptime, 1.f);
        }
        l.pose1 = l.blend >= 1.f ? e.currentpose : e.previouspose;
        l.pose2 = e.currentpose;
    }
    l.pose1 = CLAMP(0, l.pose1, hdr->numposes - 1);
    l.pose2 = CLAMP(0, l.pose2, hdr->numposes - 1);

    l.origin = glm::vec3{e.origin[0], e.origin[1], e.origin[2]};
    l.angles = glm::vec3{e.angles[0], e.angles[1], e.angles[2]};
    if(r_lerpmove.value && (e.lerpflags & LERP_MOVESTEP) && !(e.lerpflags & LERP_RESETMOVE))
    {
        const glm::vec3 current{e.currentorigin[0], e.currentorigin[1], e.currentorigin[2]};
        const glm::vec3 currentAngles{e.currentangles[0], e.currentangles[1], e.currentangles[2]};
        if(current != l.origin || currentAngles != l.angles)
        {
            l.origin = current; // a move starting: at its start
            l.angles = currentAngles;
        }
        else
        {
            const float blend = (e.lerpflags & LERP_FINISH)
                                    ? CLAMP(0.f, static_cast<float>(cl.time - e.movelerpstart) / std::fmax(e.lerpfinish - e.movelerpstart, 1e-4f), 1.f)
                                    : CLAMP(0.f, static_cast<float>(cl.time - e.movelerpstart) / 0.1f, 1.f);
            const glm::vec3 previous{e.previousorigin[0], e.previousorigin[1], e.previousorigin[2]};
            const glm::vec3 previousAngles{e.previousangles[0], e.previousangles[1], e.previousangles[2]};
            l.origin = previous + (current - previous) * blend;
            glm::vec3 d = currentAngles - previousAngles;
            for(int i = 0; i < 3; i++)
            {
                d[i] = d[i] > 180.f ? d[i] - 360.f : d[i] < -180.f ? d[i] + 360.f : d[i];
            }
            l.angles = previousAngles + d * blend;
        }
    }
    return l;
}

[[nodiscard]] glm::mat4 toMat4(const float m[16])
{
    glm::mat4 r;
    for(int c = 0; c < 16; c++)
    {
        r[c / 4][c % 4] = m[c];
    }
    return r;
}

// The pose of an alias model's frame that `pose` is in (its bounds), or -1.
[[nodiscard]] int frameOfPose(const aliashdr_t* hdr, int pose)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(pose >= hdr->frames[f].firstpose && pose < hdr->frames[f].firstpose + hdr->frames[f].numposes)
        {
            return f;
        }
    }
    return -1;
}

// Where `e` is drawn this frame and its box (cheap: no vertices yet).
const Posed* posed(int num, const entity_t& e)
{
    Posed& p = posedCache[num];
    if(p.frame == host_framecount)
    {
        return p.valid ? &p : nullptr;
    }
    p.frame = host_framecount;
    p.vertsFrame = -1;
    p.valid = false;
    p.ent = &e;
    p.model = trianglesOf(e.model);
    if(!p.model)
    {
        return nullptr;
    }
    float m16[16];
    glm::vec3 blo, bhi; // bounds in the model's coordinates
    if(p.model->alias)
    {
        const aliashdr_t* hdr = quakeAlias(e.model);
        if(!hdr || VR_AliasBonePoses(&e, nullptr))
        {
            return nullptr;
        }
        const Lerped l = lerpOf(e, hdr);
        p.pose1 = l.pose1;
        p.pose2 = l.pose2;
        p.blend = l.blend;
        vec3_t origin{l.origin.x, l.origin.y, l.origin.z}, angles{l.angles.x, l.angles.y, l.angles.z};
        R_EntityMatrix(m16, origin, angles, e.scale);
        VR_AliasPreTransform(&e, m16);
        ApplyTranslation(m16, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
        ApplyScale(m16, hdr->scale[0], hdr->scale[1], hdr->scale[2]);
        VR_AliasPostTransform(&e, m16);
        blo = glm::vec3{1e30f};
        bhi = glm::vec3{-1e30f};
        for(const int pose : {l.pose1, l.pose2})
        {
            const int f = frameOfPose(hdr, pose);
            const maliasframedesc_t& fd = hdr->frames[f >= 0 ? f : 0];
            blo = glm::min(blo, glm::vec3{fd.bboxmin.v[0], fd.bboxmin.v[1], fd.bboxmin.v[2]});
            bhi = glm::max(bhi, glm::vec3{fd.bboxmax.v[0], fd.bboxmax.v[1], fd.bboxmax.v[2]});
        }
    }
    else
    {
        vec3_t origin, angles{-e.angles[0], e.angles[1], e.angles[2]};
        VectorCopy(e.origin, origin);
        R_EntityMatrix(m16, origin, angles, e.scale);
        VR_BrushTransform(&e, m16);
        blo = glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]};
        bhi = glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]};
    }
    p.m = toMat4(m16);
    p.lo = glm::vec3{1e30f};
    p.hi = glm::vec3{-1e30f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 corner{c & 1 ? bhi.x : blo.x, c & 2 ? bhi.y : blo.y, c & 4 ? bhi.z : blo.z};
        const glm::vec3 w{p.m * glm::vec4{corner, 1.f}};
        p.lo = glm::min(p.lo, w);
        p.hi = glm::max(p.hi, w);
    }
    p.lo -= glm::vec3{0.5f}; // a trivertx is rounded
    p.hi += glm::vec3{0.5f};
    p.valid = true;
    return &p;
}

// Its vertices as drawn this frame (the lerp between its two poses), in the world: once a frame, when something is near.
const std::vector<glm::vec3>& vertsOf(Posed& p)
{
    if(p.vertsFrame == host_framecount)
    {
        return p.verts;
    }
    p.vertsFrame = host_framecount;
    if(p.model->alias)
    {
        const aliashdr_t* hdr = quakeAlias(p.ent->model);
        const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
        const trivertx_t* v1 = base + static_cast<std::size_t>(p.pose1) * static_cast<std::size_t>(hdr->numverts);
        const trivertx_t* v2 = base + static_cast<std::size_t>(p.pose2) * static_cast<std::size_t>(hdr->numverts);
        p.verts.resize(static_cast<std::size_t>(hdr->numverts));
        const float b = p.blend;
        for(int i = 0; i < hdr->numverts; i++)
        {
            const glm::vec3 raw{v1[i].v[0] + (v2[i].v[0] - v1[i].v[0]) * b, v1[i].v[1] + (v2[i].v[1] - v1[i].v[1]) * b,
                v1[i].v[2] + (v2[i].v[2] - v1[i].v[2]) * b};
            p.verts[static_cast<std::size_t>(i)] = glm::vec3{p.m * glm::vec4{raw, 1.f}};
        }
    }
    else
    {
        p.verts.resize(p.model->brushVerts.size());
        for(std::size_t i = 0; i < p.verts.size(); i++)
        {
            p.verts[i] = glm::vec3{p.m * glm::vec4{p.model->brushVerts[i], 1.f}};
        }
    }
    return p.verts;
}

// ----------------------------------------------------------------------------
// What is in the way: monsters (alive or dead: corpses; the training dummy) and other players; with objects, the rest
// that is drawn with a model and lies still (items and their boxes, gibs, heads, backpacks, weapons lying round), not
// what flies (missiles, thrown things) nor the level's own brush entities (doors, lifts: the walls' collision).

enum class Kind
{
    None,
    Monster,
    Object
};

[[nodiscard]] bool smallBrush(const qmodel_t* m)
{
    for(int i = 0; i < 3; i++)
    {
        if(m->maxs[i] - m->mins[i] > 72.f)
        {
            return false;
        }
    }
    return true;
}

// While hosting (the server's entities: its flags say what a thing is); else by the model.
[[nodiscard]] Kind kindOf(int num, const entity_t& e, bool hosting)
{
    if(hosting && num < qcvm->num_edicts)
    {
        edict_t* ed = EDICT_NUM(num);
        if(ed->free)
        {
            return Kind::None;
        }
        const int flags = static_cast<int>(ed->v.flags);
        if(flags & (FL_MONSTER | FL_CLIENT))
        {
            return Kind::Monster;
        }
        const int movetype = static_cast<int>(ed->v.movetype);
        if(movetype == MOVETYPE_FLYMISSILE || movetype == MOVETYPE_NOCLIP || VectorLength(ed->v.velocity) > fastObject)
        {
            return Kind::None;
        }
        if(e.model->type == mod_brush && (static_cast<int>(ed->v.solid) == SOLID_BSP || !smallBrush(e.model)))
        {
            return Kind::None;
        }
        return Kind::Object;
    }
    if(e.model->type == mod_brush)
    {
        return smallBrush(e.model) ? Kind::Object : Kind::None;
    }
    const int f = e.model->flags;
    if(f & (EF_ROCKET | EF_GRENADE | EF_TRACER | EF_TRACER2 | EF_TRACER3 | EF_ZOMGIB))
    {
        return Kind::None;
    }
    return (f & (EF_GIB | EF_ROTATE)) ? Kind::Object : Kind::Monster;
}

// ----------------------------------------------------------------------------
// The weapons: each hand's as it was drawn last frame (its model's pose, where it is in the hand), and the vertices its
// rays go to (per model and pose: farthest-point samples, the model's raw coordinates).

struct Recorded
{
    bool valid{false};
    const qmodel_t* model{nullptr};
    int frame{0};
    glm::mat4 toHand{1.f}; // the model's raw coordinates to the hand's frame
};
Recorded recorded[2];

struct SampleKey
{
    const qmodel_t* model;
    int pose;
    bool operator==(const SampleKey& o) const { return model == o.model && pose == o.pose; }
};
struct SampleKeyHash
{
    std::size_t operator()(const SampleKey& k) const
    {
        return std::hash<const void*>{}(k.model) ^ (static_cast<std::size_t>(k.pose) * 2654435761u);
    }
};
struct Samples
{
    std::string name;
    std::vector<glm::vec3> raw;
};
std::unordered_map<SampleKey, Samples, SampleKeyHash> samplesCache;

const std::vector<glm::vec3>* samplesOf(const qmodel_t* model, int frame)
{
    const aliashdr_t* hdr = quakeAlias(model);
    if(!hdr)
    {
        return nullptr;
    }
    const int pose = hdr->frames[frame >= 0 && frame < hdr->numframes ? frame : 0].firstpose;
    Samples& s = samplesCache[SampleKey{model, pose}];
    if(s.name == model->name)
    {
        return s.raw.empty() ? nullptr : &s.raw;
    }
    s.name = model->name;
    s.raw.clear();
    const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                        static_cast<std::size_t>(pose) * static_cast<std::size_t>(hdr->numverts);
    // Farthest-point sampling, in the model's own units (its raw vertices scaled by its header): from the vertex
    // farthest from their middle, each next the one farthest from those taken.
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    std::vector<glm::vec3> real(static_cast<std::size_t>(hdr->numverts));
    glm::vec3 middle{0.f};
    for(int i = 0; i < hdr->numverts; i++)
    {
        real[static_cast<std::size_t>(i)] = glm::vec3{verts[i].v[0], verts[i].v[1], verts[i].v[2]} * scale;
        middle += real[static_cast<std::size_t>(i)];
    }
    middle /= static_cast<float>(hdr->numverts);
    std::vector<float> nearest(real.size(), 1e30f);
    std::size_t next = 0;
    float farthest = -1.f;
    for(std::size_t i = 0; i < real.size(); i++)
    {
        if(const float d = glm::distance(real[i], middle); d > farthest)
        {
            farthest = d;
            next = i;
        }
    }
    for(int k = 0; k < sampleCount && k < hdr->numverts; k++)
    {
        s.raw.push_back(glm::vec3{verts[next].v[0], verts[next].v[1], verts[next].v[2]});
        const glm::vec3 taken = real[next];
        float best = -1.f;
        for(std::size_t i = 0; i < real.size(); i++)
        {
            nearest[i] = std::fmin(nearest[i], glm::distance(real[i], taken));
            if(nearest[i] > best)
            {
                best = nearest[i];
                next = i;
            }
        }
        if(best <= 0.f)
        {
            break;
        }
    }
    return s.raw.empty() ? nullptr : &s.raw;
}

// The hand's frame (its forward, right and up at `pos`).
[[nodiscard]] glm::mat4 handFrame(const glm::vec3& pos, const glm::vec3& rot)
{
    glm::vec3 f, r, u;
    hands::angleVectors(rot, f, r, u);
    glm::mat4 m{1.f};
    m[0] = glm::vec4{f, 0.f};
    m[1] = glm::vec4{r, 0.f};
    m[2] = glm::vec4{u, 0.f};
    m[3] = glm::vec4{pos, 1.f};
    return m;
}

// ----------------------------------------------------------------------------
// The test.

struct Tri
{
    glm::vec3 a, e1, e2;
    glm::vec3 lo, hi;
    int owner;
};

// Runs of a model's triangles in its own order (neighbours, mostly: a limb's), under one box: a ray skips a run at once.
constexpr std::uint32_t chunkSize = 8;
struct Chunk
{
    glm::vec3 lo, hi;
    std::uint32_t first, count;
};

struct Plane
{
    glm::vec3 n;
    float c; // the push p must have dot(p, n) >= c
};

struct Stats
{
    int models{0};    // near the weapon (their boxes)
    int triangles{0}; // theirs near it (summed over the rounds)
    int rays{0};
    int rounds{0};
    int planes{0};
    int entity{0};    // the last one that stopped it
};

struct Result
{
    glm::vec3 push{0.f};  // the least move out
    glm::vec3 given{0.f}; // as much of it as is given (vr_model_collide_max)
    bool blocked{false};
    Stats stats;
};

struct Ray
{
    glm::vec3 a, b;
    bool hand; // the hand's own: counts only if it ends inside
};

std::vector<Tri> tris;     // the triangles near the rays
std::vector<Chunk> chunks; // and their runs
std::vector<Ray> rays;
std::vector<Plane> planes;
std::vector<int> nearby;     // the entities near the weapon

// The first place `ray` goes into a triangle's front (`s` along it, 0..1), and after it the first place it comes out
// of the same model (1: it doesn't before its end).
struct Crossing
{
    bool in{false};
    float sIn{2.f}, sOut{1.f};
    glm::vec3 n{0.f}; // outward, unit
    int owner{0};     // the entity
};

[[nodiscard]] Crossing cross(const Ray& ray)
{
    Crossing c;
    const glm::vec3 d = ray.b - ray.a;
    const glm::vec3 rlo = glm::min(ray.a, ray.b), rhi = glm::max(ray.a, ray.b);
    int inTri = -1;
    constexpr int maxOuts = 32;
    float outS[maxOuts];
    int outOwner[maxOuts];
    int outs = 0;
    for(const Chunk& k : chunks)
    {
        if(k.hi.x < rlo.x || k.hi.y < rlo.y || k.hi.z < rlo.z || k.lo.x > rhi.x || k.lo.y > rhi.y || k.lo.z > rhi.z)
        {
            continue;
        }
        for(std::size_t i = k.first; i < k.first + k.count; i++)
        {
            const Tri& t = tris[i];
            if(t.hi.x < rlo.x || t.hi.y < rlo.y || t.hi.z < rlo.z || t.lo.x > rhi.x || t.lo.y > rhi.y || t.lo.z > rhi.z)
            {
                continue;
            }
            const glm::vec3 pv = glm::cross(d, t.e2);
            const float det = glm::dot(t.e1, pv);
            if(std::fabs(det) < 1e-9f)
            {
                continue;
            }
            const float inv = 1.f / det;
            const glm::vec3 tv = ray.a - t.a;
            const float u = glm::dot(tv, pv) * inv;
            if(u < 0.f || u > 1.f)
            {
                continue;
            }
            const glm::vec3 qv = glm::cross(tv, t.e1);
            const float v = glm::dot(d, qv) * inv;
            if(v < 0.f || u + v > 1.f)
            {
                continue;
            }
            const float s = glm::dot(t.e2, qv) * inv;
            if(s < 0.f || s > 1.f)
            {
                continue;
            }
            // Quake's models are wound clockwise seen from outside (both kinds: glquake culled GL_FRONT): cross(e1, e2)
            // points in, and det = -dot(d, cross(e1, e2)) is negative going into the model through its front.
            if(det < 0.f)
            {
                if(s < c.sIn)
                {
                    c.sIn = s;
                    inTri = static_cast<int>(i);
                }
            }
            else if(outs < maxOuts)
            {
                outS[outs] = s;
                outOwner[outs++] = t.owner;
            }
        }
    }
    if(inTri < 0)
    {
        return c;
    }
    c.in = true;
    const Tri& t = tris[static_cast<std::size_t>(inTri)];
    for(int i = 0; i < outs; i++)
    {
        if(outOwner[i] == t.owner && outS[i] > c.sIn && outS[i] < c.sOut)
        {
            c.sOut = outS[i];
        }
    }
    c.n = -glm::normalize(glm::cross(t.e1, t.e2)); // out
    c.owner = t.owner;
    return c;
}

// The triangles of the entities near the weapon that are within `lo`..`hi`.
void gather(const glm::vec3& lo, const glm::vec3& hi, Stats& stats)
{
    tris.clear();
    chunks.clear();
    for(const int num : nearby)
    {
        Posed& p = posedCache[num];
        if(p.hi.x < lo.x || p.hi.y < lo.y || p.hi.z < lo.z || p.lo.x > hi.x || p.lo.y > hi.y || p.lo.z > hi.z)
        {
            continue;
        }
        const std::vector<glm::vec3>& verts = vertsOf(p);
        for(const auto& tri : p.model->tris)
        {
            if(tri[0] >= verts.size() || tri[1] >= verts.size() || tri[2] >= verts.size())
            {
                continue;
            }
            const glm::vec3& a = verts[tri[0]];
            const glm::vec3& b = verts[tri[1]];
            const glm::vec3& c = verts[tri[2]];
            const glm::vec3 tlo = glm::min(a, glm::min(b, c)), thi = glm::max(a, glm::max(b, c));
            if(thi.x < lo.x || thi.y < lo.y || thi.z < lo.z || tlo.x > hi.x || tlo.y > hi.y || tlo.z > hi.z)
            {
                continue;
            }
            if(chunks.empty() || chunks.back().count == chunkSize || tris.back().owner != num)
            {
                chunks.push_back(Chunk{tlo, thi, static_cast<std::uint32_t>(tris.size()), 0});
            }
            Chunk& k = chunks.back();
            k.lo = glm::min(k.lo, tlo);
            k.hi = glm::max(k.hi, thi);
            k.count++;
            tris.push_back(Tri{a, b - a, c - a, tlo, thi, num});
        }
    }
    stats.triangles += static_cast<int>(tris.size());
}

// Why `hand` is not tested this frame (nullptr: it is), against what vr_model_collide says (1 monsters, 2 objects too).
[[nodiscard]] const char* notTested(const hands::State& s, int hand)
{
    const Recorded& r = recorded[hand];
    if(!vr_model_collide.value)
    {
        return "vr_model_collide 0";
    }
    if(!s.valid || !r.valid)
    {
        return "no weapon drawn";
    }
    if(r.model != weapons::heldModel(hand))
    {
        return "the weapon changed";
    }
    const int slot = weapons::heldSlot(hand);
    if(slot < 0)
    {
        return "not a weapon";
    }
    if(slot == weapons::fistSlot() && !vr_model_collide_hands.value)
    {
        return "an empty hand (vr_model_collide_hands 0)";
    }
    // Not the helping hand of a two-handed grip (drawn on the weapon it helps hold), not a gun carried by its foregrip,
    // not at a holster or another hotspot, not holding a thing or the flashlight.
    if(twohand::helping(hand) || twohand::carrying(hand))
    {
        return "two-handed: the helping hand, or a carried gun";
    }
    // (HS_*_2H_GRAB only says the hands are 5..25 units apart: helping() is the grip.)
    const int hs = s.hotspot[hand];
    if(hs != body::HS_NONE && hs != body::HS_OFFHAND_2H_GRAB && hs != body::HS_MAINHAND_2H_GRAB)
    {
        return "at a holster, or passing the weapon to the other hand";
    }
    if(held::heldEntity(hand) || flashlight::holds(hand))
    {
        return "holding a thing or the flashlight";
    }
    return nullptr;
}

[[nodiscard]] bool tested(const hands::State& s, int hand)
{
    return !notTested(s, hand);
}

// The push of `hand` out of the models near it, from its tracked pose.
Result test(const hands::State& s, int hand)
{
    Result result;
    const Recorded& r = recorded[hand];
    const std::vector<glm::vec3>* samples = samplesOf(r.model, r.frame);
    if(!samples)
    {
        return result;
    }
    const float most = std::fmax(vr_model_collide_max.value, 0.f) * 0.01f * units::metresToUnits();
    if(most <= 0.f)
    {
        return result;
    }

    // The rays: from the hand to the weapon's samples, and to the hand from behind it (towards the chest).
    const glm::vec3 grip = s.pos[hand];
    const glm::mat4 toWorld = handFrame(grip, s.visualRot[hand]) * r.toHand;
    glm::vec3 torso = s.playerOrigin;
    torso.z += vr_floor_offset.value + vr_gun_z_offset.value + 40.f;
    const float toTorso = glm::distance(torso, grip);
    rays.clear();
    if(toTorso > 1.f)
    {
        rays.push_back(Ray{grip + (torso - grip) * (std::fmin(handRayLength, toTorso) / toTorso), grip, true});
    }
    for(const glm::vec3& v : *samples)
    {
        rays.push_back(Ray{grip, glm::vec3{toWorld * glm::vec4{v, 1.f}}, false});
    }
    glm::vec3 lo = grip, hi = grip;
    for(const Ray& ray : rays)
    {
        lo = glm::min(lo, glm::min(ray.a, ray.b));
        hi = glm::max(hi, glm::max(ray.a, ray.b));
    }

    // The entities whose boxes (all their frames', turned) are near the weapon's, pushed as far as it may be.
    const bool hosting = sv.active && svs.maxclients >= 1;
    qcvm_t* oldvm = nullptr;
    if(hosting)
    {
        PR_PushQCVM(&sv.qcvm, &oldvm);
    }
    const int mode = static_cast<int>(vr_model_collide.value);
    const int heldA = held::heldEntity(0), heldB = held::heldEntity(1);
    const glm::vec3 reachLo = lo - glm::vec3{2.f * most + 1.f}, reachHi = hi + glm::vec3{2.f * most + 1.f};
    nearby.clear();
    for(int num = 1; num < cl.num_entities; num++)
    {
        const entity_t& e = cl_entities[num];
        if(!e.model || e.msgtime != cl.mtime[0] || num == cl.viewentity || num == heldA || num == heldB ||
            e.alpha == ENTALPHA_ZERO || (e.model->type != mod_alias && e.model->type != mod_brush))
        {
            continue;
        }
        // Its model's bounds (all its frames, turned any way), scaled as it is drawn (its scale, the networked one),
        // and moved by the networked offset.
        float grow = e.scale ? ENTSCALE_DECODE(e.scale) : 1.f;
        float pad = 0.f;
        if(const client::EntityVr* net = client::entityVr(num))
        {
            grow *= 1.f + std::fmax(0.f, std::fmax(net->scale.x, std::fmax(net->scale.y, net->scale.z)));
            pad = glm::length(net->offset) * grow + glm::length(net->scaleOrigin) * glm::length(net->scale);
        }
        glm::vec3 elo, ehi;
        for(int i = 0; i < 3; i++)
        {
            elo[i] = e.origin[i] + e.model->rmins[i] * grow - pad;
            ehi[i] = e.origin[i] + e.model->rmaxs[i] * grow + pad;
        }
        if(ehi.x < reachLo.x || ehi.y < reachLo.y || ehi.z < reachLo.z || elo.x > reachHi.x || elo.y > reachHi.y || elo.z > reachHi.z)
        {
            continue;
        }
        const Kind kind = kindOf(num, e, hosting);
        if(kind == Kind::None || (kind == Kind::Object && mode < 2))
        {
            continue;
        }
        // Its box as drawn this frame (its frames' bounds, placed as it is drawn).
        const Posed* p = posed(num, e);
        if(p && !(p->hi.x < reachLo.x || p->hi.y < reachLo.y || p->hi.z < reachLo.z || p->lo.x > reachHi.x || p->lo.y > reachHi.y ||
                    p->lo.z > reachHi.z))
        {
            nearby.push_back(num);
        }
    }
    if(hosting)
    {
        PR_PopQCVM(oldvm);
    }
    result.stats.models = static_cast<int>(nearby.size());
    if(nearby.empty())
    {
        return result;
    }

    // Rounds: the rays (moved by the push so far) against the triangles near them; each ray that goes into a model's
    // front, a plane its part inside must leave; the push, the least move out of every plane found (Gauss-Seidel).
    planes.clear();
    glm::vec3 p{0.f};
    for(int round = 0; round < rounds; round++)
    {
        // The triangles near the rays as the push so far moves them (each ray then skips the runs away from it).
        result.stats.rounds++;
        gather(lo + p - glm::vec3{margin}, hi + p + glm::vec3{margin}, result.stats);
        if(tris.empty())
        {
            break;
        }
        bool found = false;
        for(const Ray& ray : rays)
        {
            const Ray moved{ray.a + p, ray.b + p, ray.hand};
            result.stats.rays++;
            const Crossing c = cross(moved);
            if(!c.in || (ray.hand && c.sOut < 1.f))
            {
                continue; // clear (the hand's ray: the hand is out, past what it crosses)
            }
            const glm::vec3 d = moved.b - moved.a;
            const glm::vec3 entry = moved.a + d * c.sIn;
            const glm::vec3 deepest = moved.a + d * c.sOut;
            const float depth = glm::dot(entry - deepest, c.n); // of the part inside, below the surface it went in by
            if(depth + margin <= 0.f)
            {
                continue;
            }
            planes.push_back(Plane{c.n, depth + margin + glm::dot(p, c.n)});
            result.stats.entity = c.owner;
            found = true;
        }
        if(!found)
        {
            break;
        }
        for(int sweep = 0; sweep < 16; sweep++)
        {
            float worst = 0.f;
            for(const Plane& plane : planes)
            {
                const float short_ = plane.c - glm::dot(p, plane.n);
                if(short_ > 0.f)
                {
                    p += plane.n * short_;
                    worst = std::fmax(worst, short_);
                }
            }
            if(worst < 0.01f)
            {
                break;
            }
        }
        if(glm::length(p) >= 2.f * most)
        {
            break; // it lets go anyway
        }
    }
    result.stats.planes = static_cast<int>(planes.size());
    result.push = p;
    result.blocked = glm::length(p) > 0.f;
    const float len = glm::length(p);
    const float give = len <= most ? 1.f : std::fmax(0.f, 2.f - len / most); // past it: less, then none
    result.given = p * give;
    return result;
}

// ----------------------------------------------------------------------------
// The pushes, drawn.

glm::vec3 drawn[2]{glm::vec3{0.f}, glm::vec3{0.f}};
Result last[2];
int appliedFrame = -1;
double lastTime = -1.0;

void drawDebug(const hands::State& s, int hand, const Result& res)
{
    const Recorded& r = recorded[hand];
    const std::vector<glm::vec3>* samples = samplesOf(r.model, r.frame);
    if(!samples)
    {
        return;
    }
    // The weapon's rays where it is tracked (grey) and drawn (green; red while pushed), and the push.
    const glm::vec3 grip = s.pos[hand];
    const glm::mat4 toWorld = handFrame(grip, s.visualRot[hand]) * r.toHand;
    const glm::vec4 tracked{0.6f, 0.6f, 0.6f, 0.5f};
    const glm::vec4 shown = res.blocked ? glm::vec4{1.f, 0.2f, 0.1f, 0.9f} : glm::vec4{0.2f, 1.f, 0.3f, 0.9f};
    for(const glm::vec3& v : *samples)
    {
        const glm::vec3 w{toWorld * glm::vec4{v, 1.f}};
        lines::line(grip, w, 0.08f, tracked, tracked);
        lines::line(grip + drawn[hand], w + drawn[hand], 0.12f, shown, shown);
    }
    lines::line(grip, grip + drawn[hand], 0.2f, glm::vec4{1.f, 1.f, 0.f, 1.f}, glm::vec4{1.f, 1.f, 0.f, 1.f});
}

} // namespace

void beginView(hands::State& s)
{
    const bool again = appliedFrame == host_framecount;
    if(!again)
    {
        appliedFrame = host_framecount;
        const float dt = lastTime >= 0.0 ? static_cast<float>(std::clamp(realtime - lastTime, 0.0, 0.1)) : 0.f;
        lastTime = realtime;
        for(int hand = 0; hand < 2; hand++)
        {
            Result res;
            if(tested(s, hand))
            {
                QVR_PROFILE("model collide");
                res = test(s, hand);
            }
            const glm::vec3 target = res.given;
            // Out at once (nearly), back in more slowly.
            const bool deeper = glm::dot(target - drawn[hand], target) > 0.f;
            const float tau = deeper ? easeIn : easeOut;
            drawn[hand] += (target - drawn[hand]) * (dt > 0.f ? 1.f - std::exp(-dt / tau) : 1.f);
            if(glm::length(drawn[hand]) < 1e-3f && glm::length(target) == 0.f)
            {
                drawn[hand] = glm::vec3{0.f};
            }
            last[hand] = res;
            if(vr_debug_model_collide.value >= 1.f && (res.blocked || glm::length(drawn[hand]) > 0.f))
            {
                const Stats& st = res.stats;
                Con_Printf("model collide %s: t %.3f push %.2f (%.2f %.2f %.2f) given %.2f drawn %.2f (%.2f %.2f %.2f); ent %d, "
                           "%d models, %d tris, %d rays, %d rounds, %d planes\n",
                    hand == HAND_MAIN ? "main" : "off", cl.time, glm::length(res.push), res.push.x, res.push.y, res.push.z,
                    glm::length(res.given), glm::length(drawn[hand]), drawn[hand].x, drawn[hand].y, drawn[hand].z, st.entity,
                    st.models, st.triangles, st.rays, st.rounds, st.planes);
            }
            if(vr_debug_model_collide.value >= 2.f && tested(s, hand))
            {
                drawDebug(s, hand, res);
            }
        }
    }
    for(int hand = 0; hand < 2; hand++)
    {
        s.pos[hand] += drawn[hand];
    }
}

void endView(hands::State& s, const entity_s* const weapon[2], const bool mirrored[2])
{
    for(int hand = 0; hand < 2; hand++)
    {
        // The weapon as drawn, in the frame of the hand as drawn.
        Recorded& r = recorded[hand];
        const entity_t* e = weapon[hand];
        r.valid = e && e->model && quakeAlias(e->model) && !twohand::carrying(hand);
        if(r.valid)
        {
            r.model = e->model;
            r.frame = e->frame;
            r.toHand = glm::inverse(handFrame(s.pos[hand], s.visualRot[hand])) * grasp::shapeToWorld(*e, mirrored[hand]);
        }

        // What the game reads: as tracked.
        s.pos[hand] -= drawn[hand];
        if(s.muzzleValid[hand])
        {
            s.muzzle[hand] -= drawn[hand];
        }
        if(s.grip2HValid[hand])
        {
            s.grip2H[hand] -= drawn[hand];
        }
    }
}

glm::vec3 drawnOffset(int hand)
{
    return hand == 0 || hand == 1 ? drawn[hand] : glm::vec3{0.f};
}

void reset()
{
    for(int hand = 0; hand < 2; hand++)
    {
        drawn[hand] = glm::vec3{0.f};
        recorded[hand] = Recorded{};
        last[hand] = Result{};
    }
    posedCache.clear();
    appliedFrame = -1;
    lastTime = -1.0;
}

void bench_f()
{
    const hands::State& s = hands::current();
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "probe"))
    {
        // vr_model_collide_bench probe: a ray from the head along the view (200 units) against every model near it: each
        // triangle it crosses, going in (its front) or out.
        glm::vec3 f, r, u;
        hands::angleVectors(s.headAngles, f, r, u);
        const Ray ray{s.head, s.head + f * 200.f, false};
        nearby.clear();
        tris.clear();
        for(int num = 1; num < cl.num_entities; num++)
        {
            const entity_t& e = cl_entities[num];
            if(!e.model || e.msgtime != cl.mtime[0] || num == cl.viewentity || (e.model->type != mod_alias && e.model->type != mod_brush))
            {
                continue;
            }
            if(posed(num, e))
            {
                nearby.push_back(num);
            }
        }
        Stats st;
        gather(glm::min(ray.a, ray.b), glm::max(ray.a, ray.b), st);
        const std::vector<Tri> all = tris;
        for(std::size_t i = 0; i < all.size(); i++)
        {
            tris.assign(1, all[i]);
            chunks.assign(1, Chunk{all[i].lo, all[i].hi, 0, 1});
            const Crossing c = cross(ray);
            // One triangle: an entry is a front; else test it reversed for a back.
            if(c.in)
            {
                Con_Printf("probe: in  at %.1f units, entity %d (%s)\n", c.sIn * 200.f, all[i].owner, cl_entities[all[i].owner].model->name);
                continue;
            }
            Tri back = all[i];
            std::swap(back.e1, back.e2);
            tris.assign(1, back);
            if(const Crossing b = cross(ray); b.in)
            {
                Con_Printf("probe: out at %.1f units, entity %d (%s)\n", b.sIn * 200.f, all[i].owner, cl_entities[all[i].owner].model->name);
            }
        }
        return;
    }
    const int n = Cmd_Argc() > 1 ? std::max(1, Q_atoi(Cmd_Argv(1))) : 1000;
    for(int hand = 1; hand >= 0; hand--)
    {
        const char* name = hand == HAND_MAIN ? "main" : "off";
        if(const char* why = notTested(s, hand))
        {
            Con_Printf("vr_model_collide_bench: %s hand: not tested (%s)\n", name, why);
            continue;
        }
        std::vector<double> us;
        us.reserve(static_cast<std::size_t>(n));
        Result res;
        for(int i = 0; i < n; i++)
        {
            for(auto& entry : posedCache)
            {
                entry.second.frame = -1; // posed afresh each time, as each frame
            }
            const auto t0 = std::chrono::steady_clock::now();
            res = test(s, hand);
            us.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
        }
        std::sort(us.begin(), us.end());
        const Stats& st = res.stats;
        if(Cmd_Argc() > 2)
        {
            for(const int num : nearby)
            {
                const Posed& p = posedCache[num];
                Con_Printf("  near: %d %s, drawn (%.0f %.0f %.0f)..(%.0f %.0f %.0f)\n", num, cl_entities[num].model->name, p.lo.x,
                    p.lo.y, p.lo.z, p.hi.x, p.hi.y, p.hi.z);
            }
        }
        if(const std::vector<glm::vec3>* samples = samplesOf(recorded[hand].model, recorded[hand].frame))
        {
            // The weapon's reach: its farthest sample from the hand, and the muzzle.
            const glm::mat4 toWorld = handFrame(s.pos[hand], s.visualRot[hand]) * recorded[hand].toHand;
            float reach = 0.f;
            glm::vec3 tip{0.f};
            for(const glm::vec3& v : *samples)
            {
                const glm::vec3 w{toWorld * glm::vec4{v, 1.f}};
                if(glm::distance(w, s.pos[hand]) > reach)
                {
                    reach = glm::distance(w, s.pos[hand]);
                    tip = w;
                }
            }
            Con_Printf("  hand (%.1f %.1f %.1f), farthest sample (%.1f %.1f %.1f) %.1f units away, muzzle %.1f units away\n",
                s.pos[hand].x, s.pos[hand].y, s.pos[hand].z, tip.x, tip.y, tip.z, reach,
                s.muzzleValid[hand] ? glm::distance(s.muzzle[hand], s.pos[hand]) : -1.f);
        }
        Con_Printf("vr_model_collide_bench: %s hand, %s, %d times: min %.1f us, median %.1f, 99%% %.1f, max %.1f; %d models "
                   "near, %d triangles, %d rays, %d rounds, %d planes; push %.2f units\n",
            name, recorded[hand].model ? recorded[hand].model->name : "?", n, us.front(), us[us.size() / 2],
            us[std::min(us.size() - 1, us.size() * 99 / 100)], us.back(),
            st.models, st.triangles, st.rays, st.rounds, st.planes, glm::length(res.push));
    }
}

} // namespace qvr::modelcollide
