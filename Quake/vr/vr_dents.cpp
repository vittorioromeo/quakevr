// vr_dents.cpp -- flesh dents (vr_dents.hpp; ROUND22.md, "Flesh dents").

#include "vr_dents.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>

namespace qvr::dents
{
namespace
{

constexpr int maxDents = 16;               // a body's at most (vr_gore_dents_max up to it)
constexpr int rowsPerDent = 3;             // the shader's (DentDepth): centre and radius, normal and depth, axis and stretch
constexpr int rowsPerBody = 1 + maxDents * rowsPerDent; // a header (its model's flesh table: QVR_ALIAS_VS_DENTS), the dents
constexpr int maxBodies = 128;             // bodies with dents at once: past it, the one hit longest ago gives them up
constexpr int fleshRows = 16384;           // the models' flesh tables after the bodies' rows (4 vertices a row: 65536)
constexpr float maxDepthShare = 0.6f;      // a dent's depth at most this share of its radius
constexpr float thicknessShare = 0.4f;     // ... and of the body's thickness under it (no see-through holes)
constexpr float maxRadius = 7.f;           // merged dents grow to this at most (units, before vr_gore_dents_size)
constexpr float fleshFade = 1.5f;          // flesh this near a non-flesh piece (rest pose, units) fades to none at it
constexpr float collapsedSize = 0.5f;      // a piece of the mesh this small in some pose is dropped or hidden there (a weapon)
constexpr float speckSize = 2.f;           // ... and this small at rest, tucked away: no flesh fades near it
constexpr float blastPartShare = 0.8f;     // a blast's crater's radius at most this share of its part's half width
constexpr float blastHeadShare = 0.4f;     // ... on the head (and its jaw): much smaller there
constexpr float blastThickShare = 0.6f;    // ... of the body's thickness under it, on a model with no rig
constexpr float blastHeadFurther = 10.f;   // a blast's nearest point on the head counted this much further (units)

struct Dent
{
    glm::vec3 c{0.f};            // its centre in the rest pose (model units)
    float r{0.f};                // its radius (units)
    glm::vec3 n{0.f, 0.f, 1.f};  // the struck surface's rest normal
    float depth{0.f};            // how far it pushes in at its centre (units)
    glm::vec3 axis{1.f, 0.f, 0.f}; // a blade's cut: its long axis (rest space)
    float stretch{1.f};          // how drawn out along it (1 round)
    float limit{1e30f};          // its depth at most, whatever the scales (the body's thickness under it)
    float rmax{1e30f};           // its radius at most, merged or not (a blast's crater: its part's size)
    double born{0.0};            // realtime it was made (the oldest merged away first)
};

// A model's flesh (made on its first dent): each vertex's share of flesh (1 flesh; 0 a weapon, a muzzle flash: never
// dented or painted; fading between near one), its rest triangles' parts, and its table's rows for the shader.
struct Flesh
{
    za::Vector<float> weight;       // each .mdl vertex's
    za::Vector<float> drawn;        // ... in the drawn vertices' order (the shader's gl_VertexID)
    za::Vector<za::U8> triPart;     // each rest triangle's part (its first corner's bone; 255 none)
    za::Vector<za::U8> triOff;      // 1: a triangle with a corner of no flesh (no dent placed on it)
    float partHalf[ragdoll::maxBones]{}; // each bone's half width (half its rest points' middle extent)
    bool partHead[ragdoll::maxBones]{};  // the head and what hangs on it (a jaw)
    int row{-1};                    // its table's first row after the bodies' (-1: none, all flesh)
    bool written{false};
    int notFlesh{0}, loose{0}, labelled{0}, collapsed{0}, faded{0}; // vertices (the info)
};

// Rig bones that are a weapon, not flesh, though they stay in the hand as the body dies (a loose piece and a dropped one
// are found by themselves): id's ogre's left hand is his grenade launcher; AD's weapon hands, each bone the weapon
// with the hand round its grip (vr_ad_rigs.inc), and the death sergeant's flail.
struct NotFleshBone
{
    modelmeta::Id model;
    const char* bone;
};
constexpr NotFleshBone notFleshBones[] = {
    {modelmeta::Id::Ogre, "hand_l"},
    {modelmeta::Id::AdDcrossbow, "hand_r"},
    {modelmeta::Id::AdHogre, "hand_r"},
    {modelmeta::Id::AdHogremac, "hand_r"},
    {modelmeta::Id::AdHogreham, "hand_r"},
    {modelmeta::Id::AdDguard, "hand_r"},
    {modelmeta::Id::AdJustice, "hand_r"},
    {modelmeta::Id::AdJudge, "hand_r"},
    {modelmeta::Id::AdDsergeant, "hand_r"},
    {modelmeta::Id::AdDsergeant, "flail"},
};

[[nodiscard]] za::SizeT heldBytes(const Flesh& f)
{
    return mem::heldBytes(f.weight) + mem::heldBytes(f.drawn) + mem::heldBytes(f.triPart) + mem::heldBytes(f.triOff);
}

struct Body
{
    const qmodel_t* model{nullptr}; // its .mdl (a ragdoll's source)
    int slot{-1};                   // its rows' block in the buffer
    int count{0};
    Dent dents[maxDents];
    double lastHit{0.0};
    bool dirty{true};               // its rows to write
};

struct State
{
    ankerl::unordered_dense::map<const entity_t*, Body> bodies; // (each its own block of rows)
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<glm::vec3>> rest; // a model's triangles in pose 0 (units)
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<za::U16>> restVerts; // ... their corners' .mdl vertices
    ankerl::unordered_dense::map<const qmodel_t*, Flesh> flesh;
    int fleshNext{0}; // the flesh tables' next free row
    auto members() { return mem::list(bodies, rest, restVerts, flesh, fleshNext); }
};
mem::Cache<State> state{"flesh dents", mem::MapChange | mem::GameDirChange | mem::ModelReload};

GLuint buffer = 0;       // the rows (SSBO 12), maxBodies blocks
float seenDepth = -1.f, seenSize = -1.f; // the scales the rows were written with
za::U32 rng = 0x2545f491u;
int writesTotal = 0;     // bodies' rows written (the info)
double strikeSeconds = 0.0; // time spent placing dents (the info)
int strikes = 0;

[[nodiscard]] float rnd(float lo, float hi)
{
    rng = rng * 1664525u + 1013904223u;
    return lo + (hi - lo) * static_cast<float>(rng >> 8) * (1.f / 16777216.f);
}

[[nodiscard]] const qmodel_t* sourceOf(const entity_t* e)
{
    const qmodel_t* src = ragdoll::sourceModel(e);
    return src ? src : e->model;
}

// A model's triangles in its rest pose (pose 0, model units), as vr_modelcollide.cpp's trianglesOf orders them.
const za::Vector<glm::vec3>* restOf(const qmodel_t* model)
{
    za::Vector<glm::vec3>& out = state.rest[model];
    if(!out.empty())
    {
        return &out;
    }
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
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes); // pose 0
    const int n = hdr->numindexes / 3 * 3;
    out.reserve(static_cast<za::SizeT>(n));
    za::Vector<za::U16>& corners = state.restVerts[model];
    corners.clear();
    for(int i = 0; i < n; i++)
    {
        corners.pushBack(mesh[indexes[i]].vertindex);
        const trivertx_t& t = verts[mesh[indexes[i]].vertindex];
        out.pushBack(glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]});
    }
    return out.empty() ? nullptr : &out;
}

[[nodiscard]] int pieceRoot(za::Vector<int>& up, int v)
{
    while(up[static_cast<za::SizeT>(v)] != v)
    {
        up[static_cast<za::SizeT>(v)] = up[static_cast<za::SizeT>(up[static_cast<za::SizeT>(v)])];
        v = up[static_cast<za::SizeT>(v)];
    }
    return v;
}

// The flesh of `model` (restOf first: its rest triangles' corners). Not flesh: a loose piece of its rig (vr_ragdoll.cpp:
// the grunt's shotgun, the enforcer's rifle, the ogre's chainsaw, the knights' swords, which leave the body as they die)
// and any piece of the mesh (its triangles joined by their corners) shrunk to a speck in some frame: a weapon dropped in
// a death, a muzzle flash, on a model with no rig too (AD's). Flesh within fleshFade of either (rest pose) fades out.
const Flesh& fleshOf(const qmodel_t* model, const aliashdr_t* hdr)
{
    const auto [it, made] = state.flesh.try_emplace(model);
    Flesh& f = it->second;
    if(!made)
    {
        return f;
    }
    const za::Vector<za::U16>& corners = state.restVerts[model];
    const int nv = hdr->numverts;
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes);
    const auto at = [&](int pose, int v) {
        const trivertx_t& t = verts[pose * nv + v];
        return glm::vec3{t.v[0] * hdr->scale[0], t.v[1] * hdr->scale[1], t.v[2] * hdr->scale[2]};
    };
    za::Vector<za::U8> off(static_cast<za::SizeT>(nv), 0);  // 1 not flesh, 2 ... and flesh fades near it
    // The rig's loose pieces, its parts.
    const ragdoll::Rig* rig = ragdoll::eligible(model) ? ragdoll::rigFor(const_cast<qmodel_t*>(model)) : nullptr;
    if(rig && static_cast<int>(rig->vertBone.size()) != nv)
    {
        rig = nullptr;
    }
    // The mesh's pieces; each one's extent at rest and its least in any pose.
    za::Vector<int> up(static_cast<za::SizeT>(nv));
    for(int v = 0; v < nv; v++)
    {
        up[static_cast<za::SizeT>(v)] = v;
    }
    for(za::SizeT i = 0; i + 2 < corners.size(); i += 3)
    {
        const int a = corners[i], b = corners[i + 1], c = corners[i + 2];
        if(a == b || b == c || a == c) // (a triangle with a repeated corner joins nothing: the ogre's saw to his hand)
        {
            continue;
        }
        up[static_cast<za::SizeT>(pieceRoot(up, b))] = pieceRoot(up, a);
        up[static_cast<za::SizeT>(pieceRoot(up, c))] = pieceRoot(up, a);
    }
    za::Vector<int> piece(static_cast<za::SizeT>(nv)), size;
    ankerl::unordered_dense::map<int, int> index;
    for(int v = 0; v < nv; v++)
    {
        const auto [p, added] = index.try_emplace(pieceRoot(up, v), static_cast<int>(size.size()));
        if(added)
        {
            size.pushBack(0);
        }
        piece[static_cast<za::SizeT>(v)] = p->second;
        size[static_cast<za::SizeT>(p->second)]++;
    }
    const za::SizeT np = size.size();
    za::Vector<float> restExtent(np, 0.f), leastExtent(np, 1e30f);
    za::Vector<glm::vec3> lo(np), hi(np);
    for(int pose = 0; pose < hdr->numposes && np > 1; pose++)
    {
        for(za::SizeT p = 0; p < np; p++)
        {
            lo[p] = glm::vec3{1e30f};
            hi[p] = glm::vec3{-1e30f};
        }
        for(int v = 0; v < nv; v++)
        {
            const za::SizeT p = static_cast<za::SizeT>(piece[static_cast<za::SizeT>(v)]);
            lo[p] = glm::min(lo[p], at(pose, v));
            hi[p] = glm::max(hi[p], at(pose, v));
        }
        for(za::SizeT p = 0; p < np; p++)
        {
            const glm::vec3 e = hi[p] - lo[p];
            const float extent = za::max(e.x, za::max(e.y, e.z));
            leastExtent[p] = za::min(leastExtent[p], extent);
            if(pose == 0)
            {
                restExtent[p] = extent;
            }
        }
    }
    za::U32 labelled = 0; // (bits: the rig's bones that are weapons)
    for(const NotFleshBone& nb : notFleshBones)
    {
        for(int b = 0; rig && b < rig->numBones; b++)
        {
            if(modelmeta::is(model, nb.model) && strcmp(rig->bones[b].name, nb.bone) == 0)
            {
                labelled |= 1u << b;
            }
        }
    }
    for(int v = 0; v < nv; v++)
    {
        const za::SizeT p = static_cast<za::SizeT>(piece[static_cast<za::SizeT>(v)]);
        const int bone = rig ? rig->vertBone[static_cast<za::SizeT>(v)] : -1;
        const bool loose = bone >= 0 && rig->bones[bone].joint == ragdoll::Joint::Loose;
        const bool weapon = bone >= 0 && (labelled & (1u << bone));
        const bool collapsed = np > 1 && 2 * size[p] < nv && leastExtent[p] < collapsedSize; // (never the body)
        if(loose || weapon || collapsed)
        {
            off[static_cast<za::SizeT>(v)] = restExtent[p] < speckSize && !weapon ? 1 : 2;
            f.notFlesh++;
            f.loose += loose ? 1 : 0;
            f.labelled += weapon ? 1 : 0;
            f.collapsed += collapsed ? 1 : 0;
        }
    }
    // The weights: none on a piece that isn't flesh, fading in near one (sharp at a speck tucked away inside).
    f.weight = za::Vector<float>(static_cast<za::SizeT>(nv), 1.f);
    for(int v = 0; v < nv; v++)
    {
        if(off[static_cast<za::SizeT>(v)])
        {
            f.weight[static_cast<za::SizeT>(v)] = 0.f;
            continue;
        }
        float nearest = 1e30f;
        const glm::vec3 p = at(0, v);
        for(int o = 0; o < nv && f.notFlesh; o++)
        {
            if(off[static_cast<za::SizeT>(o)] == 2)
            {
                const glm::vec3 d = at(0, o) - p;
                nearest = za::min(nearest, glm::dot(d, d));
            }
        }
        if(nearest < fleshFade * fleshFade)
        {
            const float x = za::clamp((za::sqrt(nearest) - 0.25f) / (fleshFade - 0.25f), 0.f, 1.f);
            f.weight[static_cast<za::SizeT>(v)] = x * x * (3.f - 2.f * x);
            f.faded++;
        }
    }
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    f.drawn = za::Vector<float>(static_cast<za::SizeT>(hdr->numverts_vbo), 1.f);
    for(int v = 0; v < hdr->numverts_vbo; v++)
    {
        f.drawn[static_cast<za::SizeT>(v)] = f.weight[mesh[v].vertindex];
    }
    // The rest triangles: their parts, and whether they have a corner of no flesh.
    f.triPart = za::Vector<za::U8>(corners.size() / 3, za::U8{255});
    f.triOff = za::Vector<za::U8>(corners.size() / 3, za::U8{0});
    for(za::SizeT i = 0; i + 2 < corners.size(); i += 3)
    {
        f.triOff[i / 3] = f.weight[corners[i]] <= 0.f || f.weight[corners[i + 1]] <= 0.f || f.weight[corners[i + 2]] <= 0.f;
        if(rig)
        {
            f.triPart[i / 3] = rig->vertBone[corners[i]];
        }
    }
    for(int b = 0; rig && b < rig->numBones; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        glm::vec3 l{1e30f}, h{-1e30f};
        for(const glm::vec3& p : bone.points)
        {
            l = glm::min(l, p);
            h = glm::max(h, p);
        }
        const glm::vec3 e = bone.points.empty() ? glm::vec3{0.f} : h - l;
        const float middle = za::max(za::min(e.x, e.y), za::min(za::max(e.x, e.y), e.z));
        f.partHalf[b] = 0.5f * middle;
        f.partHead[b] = rig->head >= 0 && (b == rig->head || bone.parent == rig->head) && bone.joint != ragdoll::Joint::Loose;
    }
    // Its table's rows: none when every vertex is flesh (the shader's header 0).
    const int rows = (hdr->numverts_vbo + 3) / 4;
    if(f.notFlesh > 0 && state.fleshNext + rows <= fleshRows)
    {
        f.row = state.fleshNext;
        state.fleshNext += rows;
    }
    return f;
}

[[nodiscard]] glm::vec3 outward(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float l = glm::length(n);
    return l > 1e-8f ? -n / l : glm::vec3{0.f, 0.f, 1.f};
}

// The point of triangle abc nearest p, as barycentrics (Ericson, "Real-Time Collision Detection" 5.1.5).
[[nodiscard]] glm::vec3 nearestOn(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return {1.f, 0.f, 0.f};
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return {0.f, 1.f, 0.f};
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        const float v = d1 / (d1 - d3);
        return {1.f - v, v, 0.f};
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return {0.f, 0.f, 1.f};
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        const float w = d2 / (d2 - d6);
        return {1.f - w, 0.f, w};
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return {0.f, 1.f - w, w};
    }
    const float denom = 1.f / (va + vb + vc);
    const float v = vb * denom, w = vc * denom;
    return {1.f - v - w, v, w};
}

// How far a line from `from` going `dir` runs inside the drawn body before it leaves it (the first triangle it meets).
[[nodiscard]] float thickness(za::Span<const glm::vec3> tris, const glm::vec3& from, const glm::vec3& dir)
{
    float best = 1e30f;
    for(za::SizeT i = 0; i + 2 < tris.size(); i += 3)
    {
        const glm::vec3& a = tris[i];
        const glm::vec3 e1 = tris[i + 1] - a, e2 = tris[i + 2] - a;
        const glm::vec3 pv = glm::cross(dir, e2);
        const float det = glm::dot(e1, pv);
        if(det > -1e-9f && det < 1e-9f)
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 tv = from - a;
        const float u = glm::dot(tv, pv) * inv;
        if(u < 0.f || u > 1.f)
        {
            continue;
        }
        const glm::vec3 qv = glm::cross(tv, e1);
        const float v = glm::dot(dir, qv) * inv;
        if(v < 0.f || u + v > 1.f)
        {
            continue;
        }
        const float t = glm::dot(e2, qv) * inv;
        if(t > 0.05f && t < best)
        {
            best = t;
        }
    }
    return best;
}

struct Size
{
    float r, depth, stretch;
};

[[nodiscard]] Size sizeOf(int kind, float amount)
{
    switch(kind)
    {
        case kindShot: return {rnd(2.f, 2.6f), rnd(0.9f, 1.2f), 1.f};
        case kindNail: return {za::min(2.4f + 0.05f * amount, 3.6f) * rnd(0.9f, 1.1f), rnd(1.2f, 1.6f), 1.f};
        case kindMelee: return {za::min(3.f + 0.03f * amount, 5.f) * rnd(0.9f, 1.1f), rnd(1.4f, 1.9f), rnd(2.2f, 3.f)};
        case kindBlast: return {za::min(3.8f + 0.015f * amount, 5.5f) * rnd(0.9f, 1.1f), za::min(2.f + 0.01f * amount, 3.f), 1.f};
        default: return {rnd(2.6f, 3.4f), rnd(1.1f, 1.5f), 1.f};
    }
}

// Merges `d` into `into`: the centre between them by their areas, wider and a little deeper.
void merge(Dent& into, const Dent& d)
{
    const float wa = into.r * into.r, wb = d.r * d.r;
    into.c = (into.c * wa + d.c * wb) / (wa + wb);
    const float big = za::max(into.r, d.r), small = za::min(into.r, d.r);
    const bool keepAxis = into.r >= d.r;
    into.r = za::min(za::min(big + 0.3f * small, big * 1.25f), za::max(big, maxRadius));
    into.rmax = za::min(into.rmax, d.rmax);
    into.r = za::min(into.r, into.rmax);
    into.limit = za::min(into.limit, d.limit);
    into.depth = za::min(za::max(into.depth, d.depth) + 0.25f * za::min(into.depth, d.depth), into.r * maxDepthShare);
    const glm::vec3 n = into.n * wa + d.n * wb;
    into.n = glm::length(n) > 1e-4f ? glm::normalize(n) : d.n;
    if(!keepAxis)
    {
        into.axis = d.axis;
        into.stretch = d.stretch;
    }
    into.born = za::max(into.born, d.born);
}

void add(Body& b, const Dent& d)
{
    const int cap = za::clamp(static_cast<int>(vr_gore_dents_max.value), 1, maxDents);
    // Onto one it overlaps much: the two one wider crater.
    for(int i = 0; i < b.count; i++)
    {
        Dent& o = b.dents[i];
        if(glm::distance(o.c, d.c) < 0.45f * (o.r + d.r) && glm::dot(o.n, d.n) > 0.3f)
        {
            merge(o, d);
            return;
        }
    }
    while(b.count >= cap)
    {
        // Full: the oldest merged into the one nearest it (near enough), else let go of (its painted wound stays).
        int oldest = 0;
        for(int i = 1; i < b.count; i++)
        {
            if(b.dents[i].born < b.dents[oldest].born)
            {
                oldest = i;
            }
        }
        const Dent old = b.dents[oldest];
        for(int i = oldest; i + 1 < b.count; i++)
        {
            b.dents[i] = b.dents[i + 1];
        }
        b.count--;
        int nearest = -1;
        float best = 1e30f;
        for(int i = 0; i < b.count; i++)
        {
            const float dist = glm::distance(b.dents[i].c, old.c);
            if(dist < best && dist < 2.5f * (b.dents[i].r + old.r) && glm::dot(b.dents[i].n, old.n) > 0.f)
            {
                best = dist;
                nearest = i;
            }
        }
        if(nearest >= 0)
        {
            merge(b.dents[nearest], old);
        }
    }
    b.dents[b.count++] = d;
}

// The body of `e` (made, with a block of rows, if it has none): null if past the pool and none can be freed.
Body* bodyOf(const entity_t* e, const qmodel_t* model)
{
    if(const auto it = state.bodies.find(e); it != state.bodies.end())
    {
        if(it->second.model != model) // another model now (a slot reused, a head): starts clean
        {
            it->second.model = model;
            it->second.count = 0;
            it->second.dirty = true;
        }
        return &it->second;
    }
    bool used[maxBodies]{};
    for(const auto& [ent, b] : state.bodies)
    {
        used[b.slot] = true;
    }
    int slot = -1;
    for(int i = 0; i < maxBodies && slot < 0; i++)
    {
        if(!used[i])
        {
            slot = i;
        }
    }
    if(slot < 0) // full: the body hit longest ago gives its dents up
    {
        const entity_t* oldest = nullptr;
        double when = 1e30;
        for(const auto& [ent, b] : state.bodies)
        {
            if(b.lastHit < when)
            {
                when = b.lastHit;
                oldest = ent;
            }
        }
        if(!oldest)
        {
            return nullptr;
        }
        slot = state.bodies[oldest].slot;
        state.bodies.erase(oldest);
    }
    Body& b = state.bodies[e];
    b = Body{};
    b.model = model;
    b.slot = slot;
    return &b;
}

void writeRows(const Body& b, float depthScale, float sizeScale)
{
    glm::vec4 rows[rowsPerBody];
    // The header: its model's flesh table, as its first vertex's index (4 a row) plus 1; 0 none (all flesh).
    const auto fl = state.flesh.find(b.model);
    const int fleshRow = fl != state.flesh.end() ? fl->second.row : -1;
    rows[0] = glm::vec4{fleshRow >= 0 ? static_cast<float>((maxBodies * rowsPerBody + fleshRow) * 4 + 1) : 0.f, 0.f, 0.f, 0.f};
    for(int i = 0; i < b.count; i++)
    {
        const Dent& d = b.dents[i];
        const float r = za::min(d.r * sizeScale, d.rmax);
        rows[1 + i * 3 + 0] = glm::vec4{d.c, r};
        rows[1 + i * 3 + 1] = glm::vec4{d.n, za::min(za::min(d.depth * depthScale * sizeScale, r * maxDepthShare * za::max(depthScale, 1.f)), d.limit)};
        rows[1 + i * 3 + 2] = glm::vec4{d.axis, za::max(d.stretch, 1.f)};
    }
    if(b.count <= 0)
    {
        return;
    }
    GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(b.slot) * rowsPerBody * static_cast<GLintptr>(sizeof(glm::vec4)),
        static_cast<GLsizeiptr>((1 + b.count * rowsPerDent) * static_cast<int>(sizeof(glm::vec4))), rows);
    writesTotal++;
}

} // namespace

void struck(const entity_t* e, za::Span<const glm::vec3> tris, const glm::vec3& at, const glm::vec3& n, const glm::vec3& way,
    int kind, float amount)
{
    if(!vr_gore_dents.value || !e || !e->model || tris.size() < 3)
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    const qmodel_t* model = sourceOf(e);
    const za::Vector<glm::vec3>* rest = restOf(model);
    if(!rest || rest->size() != tris.size())
    {
        return;
    }
    const Flesh& flesh = fleshOf(model, static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model))));
    // The drawn triangle facing the blow nearest the point struck, and where on it (a blast's: flesh only, the head's
    // counted further: its crater on the chest or a limb rather than over his face).
    za::SizeT best = tris.size();
    float bestDist = 1e30f, bestRank = 1e30f;
    glm::vec3 bary{0.f};
    const bool blast = kind == kindBlast; // `at` the blast's centre: the nearest point facing it
    for(za::SizeT i = 0; i + 2 < tris.size(); i += 3)
    {
        const glm::vec3 out = outward(tris[i], tris[i + 1], tris[i + 2]);
        if(!blast && glm::dot(out, n) < 0.3f)
        {
            continue;
        }
        const glm::vec3 w = nearestOn(at, tris[i], tris[i + 1], tris[i + 2]);
        const glm::vec3 p = tris[i] * w.x + tris[i + 1] * w.y + tris[i + 2] * w.z;
        const float dist = glm::distance(at, p);
        if(blast && glm::dot(out, at - p) < 0.2f * dist)
        {
            continue;
        }
        const za::U8 part = flesh.triPart[i / 3];
        const float rank = blast && part < ragdoll::maxBones && flesh.partHead[part] ? dist + blastHeadFurther : dist;
        if(rank < bestRank && !(blast && flesh.triOff[i / 3]))
        {
            bestRank = rank;
            bestDist = dist;
            best = i;
            bary = w;
        }
    }
    if(best >= tris.size() || bestDist > (blast ? 200.f : 6.f) || flesh.triOff[best / 3]) // (a gun: no flesh in it)
    {
        return;
    }
    const glm::vec3 &a = tris[best], &b = tris[best + 1], &c = tris[best + 2];
    const glm::vec3 &ra = (*rest)[best], &rb = (*rest)[best + 1], &rc = (*rest)[best + 2];
    const glm::vec3 drawnN = outward(a, b, c);
    const glm::vec3 restN = outward(ra, rb, rc);
    const glm::vec3 point = a * bary.x + b * bary.y + c * bary.z;

    const Size s = sizeOf(kind, amount);
    Dent d;
    d.c = ra * bary.x + rb * bary.y + rc * bary.z;
    d.r = s.r;
    d.n = restN;
    d.stretch = s.stretch;
    d.born = realtime;
    // Held to a share of the body's thickness under it (an arm, a tail: never through) and of its radius.
    const float thick = thickness(tris, point - drawnN * 0.02f, -drawnN);
    // A blast's crater (and its lesser wounds) held to the part it lands on (a head's much smaller: never his whole
    // face); with no rig, to the body's thickness under it.
    const bool blasted = blast || kind == kindBlastBleed;
    const za::U8 part = flesh.triPart[best / 3];
    if(blasted && part < ragdoll::maxBones && flesh.partHalf[part] > 0.f)
    {
        d.rmax = flesh.partHalf[part] * (flesh.partHead[part] ? blastHeadShare : blastPartShare);
    }
    else if(blasted && thick < 1e29f)
    {
        d.rmax = thick * blastThickShare;
    }
    d.rmax = za::max(d.rmax, 1.5f);
    d.r = za::min(d.r, d.rmax);
    d.depth = za::min(s.depth, d.r * maxDepthShare);
    if(thick < 1e29f)
    {
        d.limit = thick * thicknessShare;
    }
    // A blade's cut drawn out along its swing on the surface, carried from the drawn triangle's frame to the rest one's.
    if(s.stretch > 1.f)
    {
        const glm::vec3 along = way - drawnN * glm::dot(way, drawnN);
        const glm::mat3 drawnFrame{b - a, c - a, drawnN};
        const glm::mat3 restFrame{rb - ra, rc - ra, restN};
        if(glm::length(along) > 1e-3f && za::fabs(glm::determinant(drawnFrame)) > 1e-6f)
        {
            const glm::vec3 axis = restFrame * (glm::inverse(drawnFrame) * along);
            const glm::vec3 flat = axis - restN * glm::dot(axis, restN);
            d.axis = glm::length(flat) > 1e-4f ? glm::normalize(flat) : glm::vec3{1.f, 0.f, 0.f};
        }
        else
        {
            d.stretch = 1.f;
        }
    }
    Body* body = bodyOf(e, model);
    if(body)
    {
        add(*body, d);
        body->lastHit = realtime;
        body->dirty = true;
    }
    strikeSeconds += Sys_DoubleTime() - t0;
    strikes++;
    if(vr_wounds_debug.value)
    {
        Con_Printf("dents: kind %d on %s part %d%s: r %.1f (max %.1f) depth %.2f (thick %.1f) stretch %.1f, %d now\n", kind,
            model->name, part < ragdoll::maxBones ? part : -1, part < ragdoll::maxBones && flesh.partHead[part] ? " (head)" : "",
            d.r, d.rmax < 1e29f ? d.rmax : -1.f, d.depth, thick < 1e29f ? thick : -1.f, d.stretch, body ? body->count : -1);
    }
}

void forget(const entity_t* e)
{
    state.bodies.erase(e);
}

void clear()
{
    state.bodies.clear();
}

int bodyCount()
{
    return static_cast<int>(state.bodies.size());
}

void info_f()
{
    int dents = 0;
    for(const auto& [e, b] : state.bodies)
    {
        dents += b.count;
    }
    Con_Printf("flesh dents: %s, %d bodies (of %d), %d dents (max %d each), %d writes, %d placed in %.3f ms (%.1f us each)\n",
        vr_gore_dents.value ? "on" : "off", static_cast<int>(state.bodies.size()), maxBodies, dents,
        za::clamp(static_cast<int>(vr_gore_dents_max.value), 1, maxDents), writesTotal, strikes, strikeSeconds * 1000.0,
        strikes ? strikeSeconds * 1e6 / strikes : 0.0);
    for(const auto& [e, b] : state.bodies)
    {
        const int num = static_cast<int>(e - cl_entities);
        Con_Printf("  %d %s: %d dents, slot %d\n", num >= 0 && num < cl_max_edicts ? num : -1, b.model ? b.model->name : "?", b.count, b.slot);
    }
    for(const auto& [m, f] : state.flesh)
    {
        Con_Printf("  flesh %s: %d vertices, %d not flesh (%d loose, %d weapon bones, %d dropped or hidden), %d fading; table row %d\n",
            m->name, static_cast<int>(f.weight.size()), f.notFlesh, f.loose, f.labelled, f.collapsed, f.faded, f.row);
    }
}

} // namespace qvr::dents

using namespace qvr;

// R_DrawAliasModel_Real (VR_AliasInstance): the instance's dents, its rows' first and count (zero: none), its drawn
// model's pose to units (the scale and its origin: a .mdl's bytes; a ragdoll's skinned model's units as they are).
extern "C" void VR_AliasDent(const entity_t* e, const void* aliashdr, float dent[4], float scale[4])
{
    dent[0] = dent[1] = dent[2] = dent[3] = 0.f;
    scale[0] = scale[1] = scale[2] = 1.f;
    scale[3] = 0.f;
    if(!vr_gore_dents.value || dents::state.bodies.empty() || !e || !e->model)
    {
        return;
    }
    const auto it = dents::state.bodies.find(e);
    if(it == dents::state.bodies.end() || it->second.count <= 0 || it->second.model != dents::sourceOf(e))
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const int cap = za::clamp(static_cast<int>(vr_gore_dents_max.value), 1, dents::maxDents);
    dent[0] = static_cast<float>(it->second.slot * dents::rowsPerBody + 1); // (its header before it)
    dent[1] = static_cast<float>(za::min(it->second.count, cap));
    dent[2] = hdr->scale_origin[0];
    dent[3] = hdr->scale_origin[1];
    scale[0] = hdr->scale[0];
    scale[1] = hdr->scale[1];
    scale[2] = hdr->scale[2];
    scale[3] = hdr->scale_origin[2];
}

// R_FlushAliasInstances, R_PaintAliasWounds: the dents' rows bound for the alias shaders (SSBO 12), the changed ones
// written first.
extern "C" void VR_DentBind(void)
{
    if(!dents::buffer)
    {
        dents::buffer = GL_CreateBuffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, "vr flesh dents",
            sizeof(glm::vec4) * (dents::rowsPerBody * dents::maxBodies + dents::fleshRows), nullptr);
    }
    for(auto& [m, f] : dents::state.flesh) // a model's flesh table, before the first body's rows that read it
    {
        if(f.row >= 0 && !f.written)
        {
            GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, dents::buffer);
            GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER,
                static_cast<GLintptr>(dents::rowsPerBody * dents::maxBodies + f.row) * static_cast<GLintptr>(sizeof(glm::vec4)),
                static_cast<GLsizeiptr>(f.drawn.size() * sizeof(float)), f.drawn.data());
            f.written = true;
        }
    }
    const float depthK = za::max(vr_gore_dents_depth.value, 0.f), sizeK = za::max(vr_gore_dents_size.value, 0.05f);
    const bool rescaled = depthK != dents::seenDepth || sizeK != dents::seenSize;
    dents::seenDepth = depthK;
    dents::seenSize = sizeK;
    for(auto& [e, b] : dents::state.bodies)
    {
        if(b.dirty || rescaled)
        {
            dents::writeRows(b, depthK, sizeK);
            b.dirty = false;
        }
    }
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 12, dents::buffer, 0,
        static_cast<GLsizeiptr>(sizeof(glm::vec4) * (dents::rowsPerBody * dents::maxBodies + dents::fleshRows)));
}
