// vr_hitmodel.cpp -- see vr_hitmodel.hpp.

#include "vr_hitmodel.hpp"
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_jobs.hpp"
#include "vr_lines.hpp"
#include "vr_modelcollide.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Iota.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

#include <algorithm>

namespace qvr::hitmodel
{
namespace
{


constexpr int leafSize = 4;

// ----------------------------------------------------------------------------
// A model's triangles in a hierarchy, and each pose's bounds of its nodes.

struct Node
{
    za::U32 first{0}; // a leaf's first triangle; an inner node's right child
    za::U32 count{0}; // a leaf's triangles; 0: an inner node (its left child is the next node)
    za::U32 skip{0};  // the node after its subtree
};

struct Bounds
{
    za::U8 lo[3], hi[3];
};

struct Mesh
{
    const void* hdr{nullptr}; // the model's data it was made from (a reloaded model is made again)
    bool valid{false};
    int numverts{0}, numposes{0};
    int restPose{0};     // the standing frame's first pose (a frame named stand*, else frame 0)
    float winding{1.f};  // -1: the triangles wind the other way round (their cross products point in)
    za::Vector<za::Array<za::U16, 3>> tris;
    za::Vector<Node> nodes;
    za::Vector<Bounds> bounds; // numposes * nodes.size()
    double buildMs{0.};
};

ankerl::unordered_dense::map<za::String, za::UniquePtr<Mesh>> meshes; // by model name, kept from map to map (pointers kept)
za::Array<Mesh*, MAX_MODELS> byIndex{};                  // this map's, by model index
za::Array<const qmodel_t*, MAX_MODELS> byIndexModel{};  // (what byIndex was found for)

[[nodiscard]] const aliashdr_t* quakeAlias(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc ||
        hdr->numframes <= 0 || hdr->numverts <= 0 || hdr->numposes <= 0 || hdr->numbones)
    {
        return nullptr;
    }
    return hdr;
}

[[nodiscard]] const trivertx_t* posesOf(const aliashdr_t* hdr)
{
    return reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
}

[[nodiscard]] glm::vec3 raw(const trivertx_t& v)
{
    return {v.v[0], v.v[1], v.v[2]};
}

[[nodiscard]] glm::vec3 anorm(const trivertx_t& v)
{
    const int i = v.lightnormalindex < NUMVERTEXNORMALS ? v.lightnormalindex : 0;
    return {r_avertexnormals[i][0], r_avertexnormals[i][1], r_avertexnormals[i][2]};
}

[[nodiscard]] int restPoseOf(const aliashdr_t* hdr)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(!q_strncasecmp(hdr->frames[f].name, "stand", 5))
        {
            return hdr->frames[f].firstpose;
        }
    }
    return hdr->frames[0].firstpose;
}

void build(Mesh& m, const aliashdr_t* hdr)
{
    const auto t0 = qza::nowNs();
    m = Mesh{};
    m.hdr = hdr;
    m.numverts = hdr->numverts;
    m.numposes = hdr->numposes;
    m.restPose = za::clamp(restPoseOf(hdr), 0, hdr->numposes - 1);

    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        const za::U16 a = mesh[indexes[i]].vertindex, b = mesh[indexes[i + 1]].vertindex, c = mesh[indexes[i + 2]].vertindex;
        if(a != b && b != c && a != c && a < hdr->numverts && b < hdr->numverts && c < hdr->numverts)
        {
            m.tris.pushBack({a, b, c});
        }
    }
    if(m.tris.empty() || m.tris.size() > 65535)
    {
        return;
    }
    const trivertx_t* poses = posesOf(hdr);
    const glm::vec3 hs{hdr->scale[0], hdr->scale[1], hdr->scale[2]};

    // Which way the triangles wind: the signed volume they enclose at rest (outward normals give a positive one).
    {
        const trivertx_t* v = poses + static_cast<za::SizeT>(m.restPose) * m.numverts;
        double vol = 0.;
        for(const auto& t : m.tris)
        {
            const glm::vec3 a = raw(v[t[0]]) * hs, b = raw(v[t[1]]) * hs, c = raw(v[t[2]]) * hs;
            vol += glm::dot(a, glm::cross(b, c));
        }
        m.winding = vol < 0. ? -1.f : 1.f;
    }

    // The hierarchy, over the triangles' centroids averaged over all the poses (one layout serves every pose).
    const za::SizeT n = m.tris.size();
    za::Vector<glm::vec3> centroid(n, glm::vec3{0.f});
    for(int p = 0; p < m.numposes; p++)
    {
        const trivertx_t* v = poses + static_cast<za::SizeT>(p) * m.numverts;
        for(za::SizeT i = 0; i < n; i++)
        {
            const auto& t = m.tris[i];
            centroid[i] += raw(v[t[0]]) + raw(v[t[1]]) + raw(v[t[2]]);
        }
    }
    za::Vector<za::U32> order(n);
    za::iota(order.begin(), order.end(), 0u);
    m.nodes.reserve(n / 2 + 1);
    const auto split = [&](auto&& self, za::U32 lo, za::U32 hi) -> void {
        const auto idx = static_cast<za::U32>(m.nodes.size());
        m.nodes.pushBack({});
        if(hi - lo <= static_cast<za::U32>(leafSize))
        {
            m.nodes[idx].first = lo;
            m.nodes[idx].count = hi - lo;
            return;
        }
        glm::vec3 cmin{1e30f}, cmax{-1e30f};
        for(za::U32 i = lo; i < hi; i++)
        {
            cmin = glm::min(cmin, centroid[order[i]]);
            cmax = glm::max(cmax, centroid[order[i]]);
        }
        const glm::vec3 ext = cmax - cmin;
        const int axis = ext.x >= ext.y && ext.x >= ext.z ? 0 : ext.y >= ext.z ? 1 : 2;
        const za::U32 mid = (lo + hi) / 2;
        // ZANCLE-TODO: no selection algorithm (std::nth_element: the tree as it was, so every hit is the same)
        std::nth_element(order.begin() + lo, order.begin() + mid, order.begin() + hi,
            [&](za::U32 a, za::U32 b) { return centroid[a][axis] < centroid[b][axis]; });
        self(self, lo, mid);
        m.nodes[idx].first = static_cast<za::U32>(m.nodes.size()); // the right child
        self(self, mid, hi);
    };
    split(split, 0u, static_cast<za::U32>(n));
    const auto skips = [&](auto&& self, za::U32 idx, za::U32 skip) -> void {
        m.nodes[idx].skip = skip;
        if(m.nodes[idx].count == 0)
        {
            const za::U32 right = m.nodes[idx].first;
            self(self, idx + 1, right);
            self(self, right, skip);
        }
    };
    skips(skips, 0u, static_cast<za::U32>(m.nodes.size()));
    za::Vector<za::Array<za::U16, 3>> sorted(n);
    for(za::SizeT i = 0; i < n; i++)
    {
        sorted[i] = m.tris[order[i]];
    }
    m.tris = ZA_MOVE(sorted);

    // Each pose's bounds of each node (children after their parent: from the last node back).
    const za::SizeT nn = m.nodes.size();
    m.bounds.resize(nn * static_cast<za::SizeT>(m.numposes));
    for(int p = 0; p < m.numposes; p++)
    {
        const trivertx_t* v = poses + static_cast<za::SizeT>(p) * m.numverts;
        Bounds* b = m.bounds.data() + nn * static_cast<za::SizeT>(p);
        for(za::SizeT i = nn; i-- > 0;)
        {
            const Node& nd = m.nodes[i];
            za::U8 lo[3]{255, 255, 255}, hi[3]{0, 0, 0};
            if(nd.count)
            {
                for(za::U32 k = nd.first; k < nd.first + nd.count; k++)
                {
                    for(const za::U16 vi : m.tris[k])
                    {
                        for(int a = 0; a < 3; a++)
                        {
                            lo[a] = za::min(lo[a], v[vi].v[a]);
                            hi[a] = za::max(hi[a], v[vi].v[a]);
                        }
                    }
                }
            }
            else
            {
                const Bounds& l = b[i + 1];
                const Bounds& r = b[nd.first];
                for(int a = 0; a < 3; a++)
                {
                    lo[a] = za::min(l.lo[a], r.lo[a]);
                    hi[a] = za::max(l.hi[a], r.hi[a]);
                }
            }
            za::copy(lo, lo + 3, b[i].lo);
            za::copy(hi, hi + 3, b[i].hi);
        }
    }
    m.valid = true;
    m.buildMs = qza::msSince(t0);
}

[[nodiscard]] const qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}

// The model's hierarchy (made now if it has none yet: a model precached after the map loaded).
const Mesh* meshOf(int index, const qmodel_t* model, const aliashdr_t*& hdr)
{
    hdr = nullptr;
    if(!model || index <= 0 || index >= MAX_MODELS)
    {
        return nullptr;
    }
    hdr = quakeAlias(model);
    if(!hdr)
    {
        return nullptr;
    }
    Mesh* m = byIndexModel[index] == model ? byIndex[index] : nullptr;
    if(!m || m->hdr != hdr)
    {
        m = &qza::stableAt<Mesh>(meshes, model->name);
        if(m->hdr != hdr || m->numverts != hdr->numverts || m->numposes != hdr->numposes)
        {
            build(*m, hdr);
        }
        byIndex[index] = m;
        byIndexModel[index] = model;
    }
    return m->valid ? m : nullptr;
}

// ----------------------------------------------------------------------------
// The client's lerp, kept on the server (r_alias.c R_SetupAliasFrame, R_SetupEntityTransform; cl_parse.c).

struct Track
{
    const qmodel_t* model{nullptr};
    double seen{-1.};
    int pose{0}, prevPose{0};
    double poseStart{0.};
    float poseDur{0.1f};
    bool step{false};
    glm::vec3 origin{0.f}, prevOrigin{0.f}, angles{0.f}, prevAngles{0.f};
    double moveStart{0.};
    float moveDur{0.1f};
};
za::Vector<Track> tracks;

[[nodiscard]] int poseAt(const aliashdr_t* hdr, float frameValue, double time, float* interval)
{
    int frame = static_cast<int>(frameValue);
    if(frame < 0 || frame >= hdr->numframes)
    {
        frame = 0;
    }
    int pose = hdr->frames[frame].firstpose;
    const int numposes = hdr->frames[frame].numposes;
    if(interval)
    {
        *interval = 0.f;
    }
    if(numposes > 1 && hdr->frames[frame].interval > 0.f)
    {
        pose += static_cast<int>(time / hdr->frames[frame].interval) % numposes;
        if(interval)
        {
            *interval = hdr->frames[frame].interval;
        }
    }
    return za::clamp(pose, 0, hdr->numposes - 1);
}

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] bool isTarget(edict_t* ent)
{
    if(!ent || ent->free || !((int)ent->v.flags & FL_MONSTER))
    {
        return false;
    }
    const int solid = static_cast<int>(ent->v.solid);
    return solid == SOLID_SLIDEBOX || solid == SOLID_BBOX || solid == SOLID_NOT_BUT_TOUCHABLE;
}

// Where and how `ent` is drawn now.
struct Drawn
{
    const Mesh* mesh{nullptr};
    const aliashdr_t* hdr{nullptr};
    int pose1{0}, pose2{0};
    float blend{0.f};
    glm::vec3 origin{0.f}, angles{0.f};
    // Its vertices as stored (bytes) to the world: A * v + b; normals: R * (n / netScale).
    glm::mat3 A{1.f}, Ainv{1.f}, R{1.f};
    glm::vec3 b{0.f}, invNetScale{1.f};
    // Its model's own space (units, about its origin, unturned): local = L * v + l.
    glm::vec3 L{1.f}, l{0.f};
};

bool checkNoLerp = false; // (vr_hitmodel_check: the model at its frame and origin, for comparison)

bool drawnOf(edict_t* ent, Drawn& d)
{
    const int index = static_cast<int>(ent->v.modelindex);
    d.mesh = meshOf(index, modelOf(ent), d.hdr);
    if(!d.mesh)
    {
        return false;
    }
    const double now = qcvm->time;
    d.origin = vec(ent->v.origin);
    d.angles = vec(ent->v.angles);
    d.pose1 = d.pose2 = poseAt(d.hdr, ent->v.frame, now, nullptr);
    d.blend = 0.f;
    const int num = NUM_FOR_EDICT(ent);
    if(num >= 0 && num < static_cast<int>(tracks.size()) && !checkNoLerp)
    {
        const Track& t = tracks[static_cast<za::SizeT>(num)];
        if(t.model == modelOf(ent) && t.seen >= now - 0.25 && t.seen >= ent->freetime)
        {
            const qmodel_t* model = modelOf(ent);
            const bool lerpModels = r_lerpmodels.value && !((model->flags & MOD_NOLERP) && r_lerpmodels.value != 2);
            d.pose1 = t.prevPose;
            d.pose2 = t.pose;
            d.blend = lerpModels ? za::clamp(static_cast<float>(now - t.poseStart) / za::max(t.poseDur, 1e-3f), 0.f, 1.f) : 1.f;
            if(t.step && r_lerpmove.value)
            {
                const float mb = za::clamp(static_cast<float>(now - t.moveStart) / za::max(t.moveDur, 1e-3f), 0.f, 1.f);
                d.origin = t.prevOrigin + (t.origin - t.prevOrigin) * mb;
                glm::vec3 da = t.angles - t.prevAngles;
                for(int i = 0; i < 3; i++)
                {
                    da[i] = da[i] > 180.f ? da[i] - 360.f : da[i] < -180.f ? da[i] + 360.f : da[i];
                }
                d.angles = t.prevAngles + da * mb;
            }
        }
    }
    if(d.blend <= 0.f)
    {
        d.pose2 = d.pose1;
    }
    else if(d.blend >= 1.f)
    {
        d.pose1 = d.pose2;
        d.blend = 0.f;
    }

    // vr_render.cpp's transform: R * S(scale) * [network scale about model_scale_origin] * T(scale_origin) *
    // S(hdr scale) * T(model_offset).
    using namespace progs;
    const FieldOffsets& f = fields();
    const glm::vec3 ns = glm::vec3{1.f} + fieldVec(ent, f.model_scale);
    const glm::vec3 so = fieldVec(ent, f.model_scale_origin);
    const glm::vec3 off = fieldVec(ent, f.model_offset);
    float es = 1.f;
    if(const eval_t* val = GetEdictFieldValue(ent, qcvm->extfields.scale); val && val->_float)
    {
        es = ENTSCALE_DECODE(ENTSCALE_ENCODE(val->_float));
    }
    es *= props::drawnSize(modelOf(ent)); // a prop's Size (Held Object Offsets)
    const glm::vec3 hs{d.hdr->scale[0], d.hdr->scale[1], d.hdr->scale[2]};
    const glm::vec3 ho{d.hdr->scale_origin[0], d.hdr->scale_origin[1], d.hdr->scale_origin[2]};
    d.L = es * ns * hs;
    d.l = es * (so * (glm::vec3{1.f} - ns) + ns * (ho + hs * off));
    d.R = held::axesFromAngles(&d.angles[0], false);
    d.A = d.R * glm::mat3{glm::vec3{d.L.x, 0.f, 0.f}, glm::vec3{0.f, d.L.y, 0.f}, glm::vec3{0.f, 0.f, d.L.z}};
    if(za::fabs(glm::determinant(d.A)) < 1e-9f)
    {
        return false;
    }
    d.Ainv = glm::inverse(d.A);
    d.b = d.origin + d.R * d.l;
    d.invNetScale = glm::vec3{1.f} / glm::max(glm::abs(ns), glm::vec3{1e-3f});
    return true;
}

// A vertex as drawn (the blend of its two poses), in the world, grown by `grow` along its normal.
struct Verts
{
    const trivertx_t* v1;
    const trivertx_t* v2;
    float blend;

    [[nodiscard]] glm::vec3 at(const Drawn& d, int vi, float grow) const
    {
        const trivertx_t& x = v1[vi];
        const trivertx_t& y = v2[vi];
        const glm::vec3 r = raw(x) + (raw(y) - raw(x)) * blend;
        glm::vec3 p = d.A * r + d.b;
        if(grow > 0.f)
        {
            const glm::vec3 n = anorm(x) + (anorm(y) - anorm(x)) * blend;
            const glm::vec3 w = d.R * (n * d.invNetScale);
            const float len = glm::length(w);
            if(len > 1e-6f)
            {
                p += w * (grow / len);
            }
        }
        return p;
    }
};

[[nodiscard]] Verts vertsOf(const Drawn& d)
{
    const trivertx_t* poses = posesOf(d.hdr);
    return {poses + static_cast<za::SizeT>(d.pose1) * d.mesh->numverts, poses + static_cast<za::SizeT>(d.pose2) * d.mesh->numverts,
        d.blend};
}

// ----------------------------------------------------------------------------
// The segment against the grown triangles.

struct Stats
{
    long long clips{0};    // broad phase tests (the engine's)
    long long narrow{0};   // model tests
    long long hits{0};
    long long throughs{0}; // met the box, missed the model
    long long insides{0};  // started inside
    long long nodes{0};
    long long tris{0};
    double ns{0.};         // the model tests' time
    long long builds{0};
    double buildMs{0.};
};
Stats stats;

struct Found
{
    float t{2.f};
    za::U32 tri{0};
    float u{0.f}, v{0.f};
    bool front{true};
    glm::vec3 faceN{0.f, 0.f, 1.f};
};

// The drawn vertices a test uses, each made once (a vertex is shared by about six triangles).
struct VertCache
{
    za::Vector<za::U32> stamp;
    za::Vector<glm::vec3> pos;
    za::U32 now{0};

    void begin(int numverts)
    {
        if(stamp.size() < static_cast<za::SizeT>(numverts))
        {
            stamp.clear();
            stamp.resize(static_cast<za::SizeT>(numverts), 0u);
            pos.resize(static_cast<za::SizeT>(numverts));
        }
        if(++now == 0u)
        {
            za::fill(stamp.begin(), stamp.end(), 0u);
            now = 1u;
        }
    }
};
VertCache vcache;

// The first crossing of a..a+dir (t 0..maxT) with the triangles grown by `grow`: the hierarchy nearest node first.
bool firstCrossing(const Drawn& d, const glm::vec3& a, const glm::vec3& dir, float grow, float maxT, Found& f)
{
    const Mesh& m = *d.mesh;
    const Verts vs = vertsOf(d);
    vcache.begin(m.numverts);
    const auto vert = [&](za::U16 vi) -> const glm::vec3& {
        if(vcache.stamp[vi] != vcache.now)
        {
            vcache.stamp[vi] = vcache.now;
            vcache.pos[vi] = vs.at(d, vi, grow);
        }
        return vcache.pos[vi];
    };
    const glm::vec3 sb = d.Ainv * (a - d.b);
    const glm::vec3 db = d.Ainv * dir;
    glm::vec3 e, inv;
    for(int i = 0; i < 3; i++)
    {
        e[i] = grow * glm::length(glm::vec3{d.Ainv[0][i], d.Ainv[1][i], d.Ainv[2][i]}) + 0.01f;
        inv[i] = za::fabs(db[i]) < 1e-9f ? 0.f : 1.f / db[i];
    }
    const za::SizeT nn = m.nodes.size();
    const Bounds* b1 = m.bounds.data() + nn * static_cast<za::SizeT>(d.pose1);
    const Bounds* b2 = m.bounds.data() + nn * static_cast<za::SizeT>(d.pose2);
    const bool two = d.pose1 != d.pose2;
    float best = maxT;
    bool found = false;
    // Where the segment enters node i's bounds (grown), or a value past `best`.
    const auto enter = [&](za::U32 i) {
        stats.nodes++;
        float t0 = 0.f, t1 = best;
        for(int k = 0; k < 3; k++)
        {
            float lo = b1[i].lo[k], hi = b1[i].hi[k];
            if(two)
            {
                lo = za::min(lo, static_cast<float>(b2[i].lo[k]));
                hi = za::max(hi, static_cast<float>(b2[i].hi[k]));
            }
            lo -= e[k];
            hi += e[k];
            if(inv[k] == 0.f)
            {
                if(sb[k] < lo || sb[k] > hi)
                {
                    return 1e30f;
                }
                continue;
            }
            float ta = (lo - sb[k]) * inv[k], tb = (hi - sb[k]) * inv[k];
            if(ta > tb)
            {
                za::genericSwap(ta, tb);
            }
            t0 = za::max(t0, ta);
            t1 = za::min(t1, tb);
            if(t0 > t1)
            {
                return 1e30f;
            }
        }
        return t0;
    };
    struct Entry
    {
        za::U32 node;
        float t;
    };
    za::Array<Entry, 64> stack;
    int sp = 0;
    if(const float t = enter(0); t <= best)
    {
        stack[sp++] = {0u, t};
    }
    while(sp > 0)
    {
        const Entry top = stack[--sp];
        if(top.t > best)
        {
            continue;
        }
        const Node& nd = m.nodes[top.node];
        if(nd.count == 0)
        {
            const za::U32 l = top.node + 1, r = nd.first;
            const float tl = enter(l), tr = enter(r);
            // The nearer one popped first.
            if(tl <= tr)
            {
                if(tr <= best && sp < 64)
                {
                    stack[sp++] = {r, tr};
                }
                if(tl <= best && sp < 64)
                {
                    stack[sp++] = {l, tl};
                }
            }
            else
            {
                if(tl <= best && sp < 64)
                {
                    stack[sp++] = {l, tl};
                }
                if(tr <= best && sp < 64)
                {
                    stack[sp++] = {r, tr};
                }
            }
            continue;
        }
        for(za::U32 k = nd.first; k < nd.first + nd.count; k++)
        {
            stats.tris++;
            const auto& t = m.tris[k];
            const glm::vec3& p0 = vert(t[0]);
            const glm::vec3 e1 = vert(t[1]) - p0, e2 = vert(t[2]) - p0;
            const glm::vec3 pv = glm::cross(dir, e2);
            const float det = glm::dot(e1, pv);
            if(za::fabs(det) < 1e-12f)
            {
                continue;
            }
            const float id = 1.f / det;
            const glm::vec3 tv = a - p0;
            const float u = glm::dot(tv, pv) * id;
            if(u < 0.f || u > 1.f)
            {
                continue;
            }
            const glm::vec3 qv = glm::cross(tv, e1);
            const float v = glm::dot(dir, qv) * id;
            if(v < 0.f || u + v > 1.f)
            {
                continue;
            }
            const float tt = glm::dot(e2, qv) * id;
            if(tt < 0.f || tt >= best)
            {
                continue;
            }
            best = tt;
            found = true;
            f.t = tt;
            f.tri = k;
            f.u = u;
            f.v = v;
            f.faceN = glm::cross(e1, e2) * m.winding;
            f.front = glm::dot(dir, f.faceN) < 0.f;
        }
    }
    return found;
}

// Whether `p` is inside the model grown by `grow`: a ray onwards along `dir` first crosses its surface on the way out.
bool inside(const Drawn& d, const glm::vec3& p, glm::vec3 dir, float grow)
{
    const Mesh& m = *d.mesh;
    const za::SizeT nn = m.nodes.size();
    const Bounds& r1 = m.bounds[nn * static_cast<za::SizeT>(d.pose1)];
    const Bounds& r2 = m.bounds[nn * static_cast<za::SizeT>(d.pose2)];
    const glm::vec3 lo = glm::min(glm::vec3{r1.lo[0], r1.lo[1], r1.lo[2]}, glm::vec3{r2.lo[0], r2.lo[1], r2.lo[2]});
    const glm::vec3 hi = glm::max(glm::vec3{r1.hi[0], r1.hi[1], r1.hi[2]}, glm::vec3{r2.hi[0], r2.hi[1], r2.hi[2]});
    // In its bounds at all (grown)?
    const glm::vec3 pb = d.Ainv * (p - d.b);
    for(int i = 0; i < 3; i++)
    {
        const float e = grow * glm::length(glm::vec3{d.Ainv[0][i], d.Ainv[1][i], d.Ainv[2][i]}) + 0.01f;
        if(pb[i] < lo[i] - e || pb[i] > hi[i] + e)
        {
            return false;
        }
    }
    const float len = glm::length(dir);
    dir = len > 1e-6f ? dir / len : glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 wlo = d.A * lo, whi = d.A * hi;
    const float reach = glm::length(whi - wlo) + 2.f * grow + 2.f;
    Found f;
    return firstCrossing(d, p, dir * reach, grow, 1.f, f) && !f.front;
}

// ----------------------------------------------------------------------------
// The last hit (restPoint), and what the debug view shows (vr_debug_hits).

struct Last
{
    int num{-1};
    double time{-1.};
    za::U32 tri{0};
    float u{0.f}, v{0.f};
    glm::vec3 point{0.f}, surface{0.f};
};
Last last;

struct Event
{
    double when{0.};
    bool hit{false};
    Class cls{Class::Guns};
    int num{0};
    glm::vec3 a{0.f}, b{0.f}; // the segment (to its hit)
    glm::vec3 point{0.f}, surface{0.f}, normal{0.f};
    za::Array<glm::vec3, 3> tri{};
    float grow{0.f};
    za::Vector<glm::vec3> wire; // the model as drawn then (3 a triangle)
};
za::Vector<Event> events; // the latest last
constexpr za::SizeT maxEvents = 12;
constexpr double eventLife = 4.;

const char* className(Class c)
{
    switch(c)
    {
    case Class::Guns: return "guns";
    case Class::Grapple: return "grapple";
    case Class::Melee: return "melee";
    case Class::Thrown: return "thrown";
    }
    return "?";
}

void record(edict_t* ent, const Drawn& d, const glm::vec3& a, const glm::vec3& b, float grow, Class c, const Hit* hit, const Found* f)
{
    if(!vr_debug_hits.value || cls.state == ca_dedicated)
    {
        return;
    }
    if(!hit && (vr_debug_hits.value < 2.f || c == Class::Melee))
    {
        return;
    }
    if(!hit) // a miss shown only if it went through its box (grown by the radius): the shots the model stopped counting
    {
        const glm::vec3 lo = vec(ent->v.absmin) - glm::vec3{grow}, hi = vec(ent->v.absmax) + glm::vec3{grow};
        const glm::vec3 dir = b - a;
        float t0 = 0.f, t1 = 1.f;
        for(int k = 0; k < 3; k++)
        {
            if(za::fabs(dir[k]) < 1e-9f)
            {
                if(a[k] < lo[k] || a[k] > hi[k])
                {
                    return;
                }
                continue;
            }
            float ta = (lo[k] - a[k]) / dir[k], tb = (hi[k] - a[k]) / dir[k];
            if(ta > tb)
            {
                za::genericSwap(ta, tb);
            }
            t0 = za::max(t0, ta);
            t1 = za::min(t1, tb);
            if(t0 > t1)
            {
                return;
            }
        }
    }
    Event e;
    e.when = realtime;
    e.hit = hit != nullptr;
    e.cls = c;
    e.num = NUM_FOR_EDICT(ent);
    e.a = a;
    e.b = hit ? hit->point : b;
    e.grow = grow;
    const Verts vs = vertsOf(d);
    e.wire.reserve(d.mesh->tris.size() * 3);
    for(const auto& t : d.mesh->tris)
    {
        for(const za::U16 vi : t)
        {
            e.wire.pushBack(vs.at(d, vi, 0.f));
        }
    }
    if(hit)
    {
        e.point = hit->point;
        e.surface = hit->surface;
        e.normal = hit->normal;
        if(f)
        {
            const auto& t = d.mesh->tris[f->tri];
            e.tri = {vs.at(d, t[0], 0.f), vs.at(d, t[1], 0.f), vs.at(d, t[2], 0.f)};
        }
    }
    if(events.size() >= maxEvents)
    {
        events.erase(events.begin());
    }
    const char* name = PR_GetString(ent->v.classname);
    if(hit)
    {
        Con_Printf("hit model: %s %d (%s, grown %.1f): at %.1f %.1f %.1f, on the model %.1f %.1f %.1f%s\n", name, e.num, className(c),
            grow, hit->point.x, hit->point.y, hit->point.z, hit->surface.x, hit->surface.y, hit->surface.z,
            hit->startInside ? " (started inside)" : "");
    }
    else
    {
        Con_Printf("hit model: %s %d (%s, grown %.1f): through its box, the model missed\n", name, e.num, className(c), grow);
    }
    events.pushBack(ZA_MOVE(e));
}

} // namespace

bool enabled()
{
    return vr_hit_precise.value != 0.f;
}

bool target(edict_t* ent)
{
    if(!enabled() || !isTarget(ent))
    {
        return false;
    }
    const aliashdr_t* hdr = nullptr;
    return meshOf(static_cast<int>(ent->v.modelindex), modelOf(ent), hdr) != nullptr;
}

float tolerance(Class c)
{
    switch(c)
    {
    case Class::Guns: return za::max(0.f, vr_hit_tolerance_guns.value);
    case Class::Grapple: return za::max(0.f, vr_hit_tolerance_grapple.value);
    case Class::Melee: return za::max(0.f, vr_hit_tolerance_melee.value);
    case Class::Thrown: return za::max(0.f, vr_hit_tolerance_thrown.value);
    }
    return 0.f;
}

bool segment(edict_t* ent, const glm::vec3& a, const glm::vec3& b, float radius, float maxT, Class c, Hit& out)
{
    Drawn d;
    if(!drawnOf(ent, d))
    {
        return false;
    }
    const auto t0 = qza::nowNs();
    stats.narrow++;
    const glm::vec3 dir = b - a;
    Found f;
    const bool crossed = firstCrossing(d, a, dir, radius, za::min(maxT, 1.f), f);
    // Started inside: its first crossing is on the way out, or it has none and a ray onwards leaves the model.
    const bool in = crossed ? !f.front : inside(d, a, dir, radius);
    const bool hit = crossed || in;
    out = Hit{};
    if(in)
    {
        stats.insides++;
        out.t = 0.f;
        out.startInside = true;
        out.point = out.surface = a; // (the model's place: the nearest triangle, restPoint)
        const float len = glm::length(dir);
        out.normal = len > 1e-6f ? -dir / len : glm::vec3{0.f, 0.f, 1.f};
        last = Last{};
    }
    else if(hit)
    {
        out.t = f.t;
        out.point = a + dir * f.t;
        const float len = glm::length(f.faceN);
        out.normal = len > 1e-9f ? f.faceN / len : glm::vec3{0.f, 0.f, 1.f};
        const Verts vs = vertsOf(d);
        const auto& t = d.mesh->tris[f.tri];
        const glm::vec3 p0 = vs.at(d, t[0], 0.f), p1 = vs.at(d, t[1], 0.f), p2 = vs.at(d, t[2], 0.f);
        out.surface = p0 + (p1 - p0) * f.u + (p2 - p0) * f.v;
        out.tri = static_cast<int>(f.tri);
        out.u = f.u;
        out.v = f.v;
        last = Last{NUM_FOR_EDICT(ent), qcvm->time, f.tri, f.u, f.v, out.point, out.surface};
    }
    if(hit)
    {
        stats.hits++;
    }
    else
    {
        stats.throughs++;
    }
    stats.ns += qza::nsSince(t0);
    record(ent, d, a, b, radius, c, hit ? &out : nullptr, hit && !in ? &f : nullptr);
    return hit;
}

bool clip(edict_t* ent, const glm::vec3& a, const glm::vec3& b, const glm::vec3& mins, const glm::vec3& maxs, float tol, Class c,
    float maxT, Hit& out)
{
    stats.clips++;
    // The broad phase is Quake's: the boxes the engine's clip met (grown by the tolerance and the reach, world.c), and
    // here the model's own bounds as drawn (the hierarchy's root), not its box: an arm reaching out of it is hit too.
    // The mover: the largest sphere in its box.
    float moverRadius = 1e30f;
    for(int k = 0; k < 3; k++)
    {
        moverRadius = qza::minOf(moverRadius, -mins[k], maxs[k]);
    }
    moverRadius = za::max(0.f, moverRadius);
    return segment(ent, a, b, tol + moverRadius, maxT, c, out);
}

bool restPoint(edict_t* ent, const glm::vec3& p, glm::vec3& out)
{
    if(!target(ent))
    {
        return false;
    }
    Drawn d;
    if(!drawnOf(ent, d))
    {
        return false;
    }
    const Mesh& m = *d.mesh;
    za::U32 tri = 0;
    float u = 0.f, v = 0.f;
    const int num = NUM_FOR_EDICT(ent);
    if(last.num == num && last.time == qcvm->time &&
        (glm::length(last.point - p) < 1.f || glm::length(last.surface - p) < 1.f) && last.tri < m.tris.size())
    {
        tri = last.tri;
        u = last.u;
        v = last.v;
    }
    else
    {
        // The drawn triangle nearest `p` (Ericson, Real-Time Collision Detection, 5.1.5), and where on it.
        const Verts vs = vertsOf(d);
        float best = 1e30f;
        for(za::U32 k = 0; k < m.tris.size(); k++)
        {
            const auto& t = m.tris[k];
            const glm::vec3 a = vs.at(d, t[0], 0.f), b = vs.at(d, t[1], 0.f), c = vs.at(d, t[2], 0.f);
            const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
            float s = 0.f, w = 0.f; // p's nearest point: a + ab * s + ac * w
            const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
            const glm::vec3 bp = p - b;
            const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
            const glm::vec3 cp = p - c;
            const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
            const float vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
            if(d1 <= 0.f && d2 <= 0.f)
            {
            }
            else if(d3 >= 0.f && d4 <= d3)
            {
                s = 1.f;
            }
            else if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
            {
                s = d1 / (d1 - d3);
            }
            else if(d6 >= 0.f && d5 <= d6)
            {
                w = 1.f;
            }
            else if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
            {
                w = d2 / (d2 - d6);
            }
            else if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
            {
                w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                s = 1.f - w;
            }
            else
            {
                const float denom = 1.f / (va + vb + vc);
                s = vb * denom;
                w = vc * denom;
            }
            const glm::vec3 q = a + ab * s + ac * w;
            const float dist = glm::dot(q - p, q - p);
            if(dist < best)
            {
                best = dist;
                tri = k;
                u = s;
                v = w;
            }
        }
    }
    // The same place on the standing pose, in its own space, placed at its origin and turned with its yaw.
    const trivertx_t* rest = posesOf(d.hdr) + static_cast<za::SizeT>(m.restPose) * m.numverts;
    const auto& t = m.tris[tri];
    const glm::vec3 r0 = raw(rest[t[0]]), r1 = raw(rest[t[1]]), r2 = raw(rest[t[2]]);
    const glm::vec3 local = d.L * (r0 + (r1 - r0) * u + (r2 - r0) * v) + d.l;
    const float yaw[3]{0.f, ent->v.angles[1], 0.f};
    out = vec(ent->v.origin) + held::axesFromAngles(yaw, false) * local;
    return true;
}

bool anchorFrame(edict_t* ent, int tri, float u, float v, glm::vec3& point, glm::mat3& axes)
{
    Drawn d;
    if(tri < 0 || !target(ent) || !drawnOf(ent, d) || static_cast<za::SizeT>(tri) >= d.mesh->tris.size())
    {
        return false;
    }
    const Verts vs = vertsOf(d);
    const auto& t = d.mesh->tris[static_cast<za::SizeT>(tri)];
    const glm::vec3 p0 = vs.at(d, t[0], 0.f), p1 = vs.at(d, t[1], 0.f), p2 = vs.at(d, t[2], 0.f);
    point = p0 + (p1 - p0) * u + (p2 - p0) * v;
    const glm::vec3 e1 = p1 - p0, n = glm::cross(e1, p2 - p0) * d.mesh->winding;
    if(glm::length(e1) < 1e-4f || glm::length(n) < 1e-6f)
    {
        return false; // (a triangle squashed flat in this pose)
    }
    const glm::vec3 x = glm::normalize(e1), z = glm::normalize(n);
    axes = glm::mat3{x, glm::cross(z, x), z};
    return true;
}

void serverFrame()
{
    if(!enabled() || !sv.active)
    {
        return;
    }
    const double now = qcvm->time;
    if(tracks.size() < static_cast<za::SizeT>(qcvm->max_edicts))
    {
        tracks.resize(static_cast<za::SizeT>(qcvm->max_edicts));
    }
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        Track& t = tracks[static_cast<za::SizeT>(num)];
        const qmodel_t* model = isTarget(ent) ? modelOf(ent) : nullptr;
        const aliashdr_t* hdr = nullptr;
        if(!model || !meshOf(static_cast<int>(ent->v.modelindex), model, hdr))
        {
            t.model = nullptr;
            continue;
        }
        float interval = 0.f;
        const int pose = poseAt(hdr, ent->v.frame, now, &interval);
        const glm::vec3 origin = vec(ent->v.origin), angles = vec(ent->v.angles);
        // The client's lerp time: a framegroup's interval; else the think's (U_LERPFINISH), or 0.1 s.
        const float lerpTime = ent->sendinterval ? za::max(0.01f, static_cast<float>(ent->v.nextthink - ent->oldthinktime)) : 0.1f;
        if(t.model != model || t.seen < now - 0.25 || t.seen < ent->freetime)
        {
            t = Track{};
            t.model = model;
            t.pose = t.prevPose = pose;
            t.origin = t.prevOrigin = origin;
            t.angles = t.prevAngles = angles;
        }
        else
        {
            if(pose != t.pose)
            {
                t.prevPose = t.pose;
                t.pose = pose;
                t.poseStart = now;
                t.poseDur = interval > 0.f ? interval : lerpTime;
            }
            if(origin != t.origin || angles != t.angles)
            {
                t.prevOrigin = t.origin; // (the client lerps a step of any length: a dog's 64-unit bound, a 70-degree turn)
                t.prevAngles = t.angles;
                t.origin = origin;
                t.angles = angles;
                t.moveStart = now;
                t.moveDur = lerpTime;
            }
        }
        t.step = static_cast<int>(ent->v.movetype) == MOVETYPE_STEP;
        t.seen = now;
    }
}

void afterLoad()
{
    const auto t0 = qza::nowNs();
    int built = 0, tris = 0;
    // The meshes to make, made at once on the game's thread pool (each build writes only its own mesh), before the
    // walk below finds them made.
    za::Array<bool, MAX_MODELS> had{};
    za::Vector<qza::Pair<Mesh*, const aliashdr_t*>> todo;
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        const qmodel_t* model = sv.models[i];
        const aliashdr_t* hdr = model ? quakeAlias(model) : nullptr;
        had[i] = hdr && meshes.count(model->name) && meshes[model->name]->hdr == hdr;
        if(!hdr || (byIndexModel[i] == model && byIndex[i] && byIndex[i]->hdr == hdr))
        {
            continue; // (none, or the one meshOf keeps)
        }
        Mesh& m = qza::stableAt<Mesh>(meshes, model->name);
        if((m.hdr != hdr || m.numverts != hdr->numverts || m.numposes != hdr->numposes) &&
            !za::anyOf(todo.begin(), todo.end(), [&m](const auto& t) { return t.first == &m; }))
        {
            todo.emplaceBack(&m, hdr);
        }
    }
    qvr::jobs::parallelFor(todo.size(), 1,
        [&todo](za::SizeT begin, za::SizeT end)
        {
            for(za::SizeT k = begin; k < end; k++)
            {
                build(*todo[k].first, todo[k].second);
            }
        });
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        const aliashdr_t* hdr = nullptr;
        const bool had_ = had[i];
        if(const Mesh* m = meshOf(i, sv.models[i], hdr); m && !had_)
        {
            built++;
            tris += static_cast<int>(m->tris.size());
            stats.builds++;
            stats.buildMs += m->buildMs;
        }
    }
    // The models this map doesn't use are forgotten (the memory is this map's models').
    for(auto it = meshes.begin(); it != meshes.end();)
    {
        const bool used = za::anyOf(byIndex.begin(), byIndex.end(), [&](const Mesh* m) { return m == it->second.get(); });
        it = used ? (it + 1) : meshes.erase(it);
    }
    const double ms = qza::msSince(t0);
    // (the precached models' meshes' hash, FNV-1a: the same made on the pool or not)
    za::U32 hash = 2166136261u;
    const auto add = [&hash](const void* data, za::SizeT size)
    {
        for(za::SizeT k = 0; k < size; k++)
        {
            hash = (hash ^ static_cast<const unsigned char*>(data)[k]) * 16777619u;
        }
    };
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i] && developer.value; i++)
    {
        if(const Mesh* m = byIndexModel[i] == sv.models[i] ? byIndex[i] : nullptr)
        {
            add(&m->restPose, sizeof(m->restPose));
            add(&m->winding, sizeof(m->winding));
            add(m->tris.data(), m->tris.size() * sizeof(m->tris[0]));
            add(m->nodes.data(), m->nodes.size() * sizeof(Node));
            add(m->bounds.data(), m->bounds.size() * sizeof(Bounds));
        }
    }
    Con_DPrintf("hit models: %d made (%d triangles) in %.1f ms, hash %08x\n", built, tris, ms, static_cast<unsigned>(hash));
}

void reset()
{
    za::fill(byIndex.begin(), byIndex.end(), nullptr);
    za::fill(byIndexModel.begin(), byIndexModel.end(), nullptr);
    tracks.clear();
    last = Last{};
    events.clear();
}

void debugDraw()
{
    if(!vr_debug_hits.value || events.empty())
    {
        return;
    }
    const double now = realtime;
    while(!events.empty() && (now - events.front().when > eventLife || now < events.front().when))
    {
        events.erase(events.begin());
    }
    // The latest event's model in full (its wireframe), the others' hits only.
    for(za::SizeT i = 0; i < events.size(); i++)
    {
        const Event& e = events[i];
        const float fade = static_cast<float>(1. - (now - e.when) / eventLife);
        const bool latest = i + 1 == events.size();
        if(latest)
        {
            const glm::vec4 wire{0.2f, 0.8f, 1.f, 0.35f * fade};
            for(za::SizeT k = 0; k + 2 < e.wire.size(); k += 3)
            {
                lines::line(e.wire[k], e.wire[k + 1], 0.12f, wire, wire);
                lines::line(e.wire[k + 1], e.wire[k + 2], 0.12f, wire, wire);
                lines::line(e.wire[k + 2], e.wire[k], 0.12f, wire, wire);
            }
        }
        if(!e.hit)
        {
            const glm::vec4 miss{1.f, 0.5f, 0.1f, 0.9f * fade};
            lines::line(e.a, e.b, 0.25f, miss, miss);
            continue;
        }
        const glm::vec4 ray{1.f, 1.f, 1.f, 0.6f * fade}, tri{0.2f, 1.f, 0.2f, fade}, onModel{1.f, 0.1f, 0.1f, fade},
            grown{1.f, 0.9f, 0.1f, fade};
        lines::line(e.a, e.point, 0.15f, ray, ray);
        lines::line(e.tri[0], e.tri[1], 0.3f, tri, tri);
        lines::line(e.tri[1], e.tri[2], 0.3f, tri, tri);
        lines::line(e.tri[2], e.tri[0], 0.3f, tri, tri);
        lines::point(e.surface, 1.6f, onModel);
        lines::point(e.point, 0.9f, grown);
        lines::line(e.surface, e.surface + e.normal * 6.f, 0.2f, onModel, onModel);
    }
}

void stats_f()
{
    Con_Printf("precise hit detection: %s; tolerances: guns %.1f, grapple %.1f, melee %.1f, thrown %.1f\n",
        enabled() ? "on" : "off", tolerance(Class::Guns), tolerance(Class::Grapple), tolerance(Class::Melee), tolerance(Class::Thrown));
    Con_Printf("  %lld broad tests, %lld model tests: %lld hits (%lld started inside), %lld through the box\n", stats.clips,
        stats.narrow, stats.hits, stats.insides, stats.throughs);
    if(stats.narrow)
    {
        Con_Printf("  a model test: %.2f us, %.1f nodes, %.1f triangles\n", stats.ns / 1000. / static_cast<double>(stats.narrow),
            static_cast<double>(stats.nodes) / static_cast<double>(stats.narrow), static_cast<double>(stats.tris) / static_cast<double>(stats.narrow));
    }
    Con_Printf("  %lld hierarchies made at map loads, %.1f ms in all\n", stats.builds, stats.buildMs);
    za::SizeT bytes = 0, count = 0;
    for(const auto& [name, mp] : meshes)
    {
        const Mesh& m = *mp;
        if(m.valid)
        {
            count++;
            bytes += m.bounds.size() * sizeof(Bounds) + m.tris.size() * sizeof(m.tris[0]) + m.nodes.size() * sizeof(Node);
        }
    }
    Con_Printf("  %zu models' hierarchies kept: %.2f MB\n", count, static_cast<double>(bytes) / (1024. * 1024.));
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        stats = Stats{};
    }
}

// vr_hitmodel_check: each monster's model as the hit test poses it against the model as the client draws it this frame
// (vr_modelcollide.cpp's drawnTriangles: the renderer's lerp): the mean and the largest distance between the same
// vertices. Accumulated over the calls since `vr_hitmodel_check reset`; printed each call with `vr_hitmodel_check print`.
namespace
{

// vr_hitmodel_check's totals since its last `reset` (a debug command's own).
struct CheckTotals
{
    double sum{0.}, worst{0.}, sumRaw{0.}, worstRaw{0.};
    long long count{0}, calls{0};
    za::String worstName;
};
CheckTotals checkTotals;

} // namespace

void check_f()
{
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        checkTotals = CheckTotals{};
        return;
    }
    auto& [sum, worst, sumRaw, worstRaw, count, calls, worstName] = checkTotals;
    if(!sv.active || cls.state != ca_connected)
    {
        Con_Printf("vr_hitmodel_check: no local game\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    calls++;
    za::Vector<glm::vec3> drawn; // (a debug command's: made each call)
    for(int num = 1; num < qcvm->num_edicts && num < cl_max_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const entity_t& e = cl_entities[num];
        Drawn d;
        if(!target(ent) || e.model != modelOf(ent) || !drawnOf(ent, d) || !modelcollide::drawnTriangles(e, num, drawn))
        {
            continue;
        }
        const auto* base = reinterpret_cast<const byte*>(d.hdr);
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + d.hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + d.hdr->indexes);
        const Verts vs = vertsOf(d);
        Drawn raw; // without the lerp: its frame, at its origin
        checkNoLerp = true;
        drawnOf(ent, raw);
        checkNoLerp = false;
        const Verts rs = vertsOf(raw);
        for(za::SizeT i = 0; i < drawn.size() && static_cast<int>(i) < d.hdr->numindexes; i++)
        {
            const float distRaw = glm::length(rs.at(raw, mesh[indexes[i]].vertindex, 0.f) - drawn[i]);
            sumRaw += distRaw;
            worstRaw = za::max(worstRaw, static_cast<double>(distRaw));
            const float dist = glm::length(vs.at(d, mesh[indexes[i]].vertindex, 0.f) - drawn[i]);
            sum += dist;
            count++;
            if(dist > worst)
            {
                worst = dist;
                const glm::vec3 cl3{e.origin[0], e.origin[1], e.origin[2]};
                worstName = va("%s, frame %d, poses %d-%d at %.2f, origin %.1f %.1f %.1f, the hit test's %.1f %.1f %.1f, the client entity's %.1f %.1f %.1f, movetype %d, flags %d",
                    PR_GetString(ent->v.classname), static_cast<int>(ent->v.frame), d.pose1, d.pose2, d.blend, ent->v.origin[0],
                    ent->v.origin[1], ent->v.origin[2], d.origin.x, d.origin.y, d.origin.z, cl3.x, cl3.y, cl3.z,
                    static_cast<int>(ent->v.movetype), static_cast<int>(ent->v.flags));
                const Track& tr = tracks[static_cast<za::SizeT>(num)];
                worstName += va("; client poses %d-%d, lerp from %.3f (flags %d), cl.time %.3f; server time %.3f, the track's poses %d-%d from %.3f for %.3f, seen %.3f",
                    e.previouspose, e.currentpose, e.lerpstart, e.lerpflags, cl.time, qcvm->time, tr.prevPose, tr.pose, tr.poseStart,
                    tr.poseDur, tr.seen);
                worstName += va("; angles %.1f %.1f, the hit test's %.1f %.1f, the client's %.1f %.1f (current %.1f, previous %.1f, move lerp from %.3f); the client's origins %.1f %.1f -> %.1f %.1f; the track's %.1f %.1f -> %.1f %.1f from %.3f",
                    ent->v.angles[0], ent->v.angles[1], d.angles.x, d.angles.y, e.angles[0], e.angles[1], e.currentangles[1], e.previousangles[1],
                    e.movelerpstart, e.previousorigin[0], e.previousorigin[1], e.currentorigin[0], e.currentorigin[1], tr.prevOrigin.x,
                    tr.prevOrigin.y, tr.origin.x, tr.origin.y, tr.moveStart);
            }
        }
    }
    PR_PopQCVM(oldvm);
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "print"))
    {
        Con_Printf("vr_hitmodel_check: %lld calls, %lld vertices: the hit model %.2f units from the drawn one on average, "
                   "%.2f at worst (%s); without the lerp %.2f, %.2f at worst\n", calls, count,
            count ? sum / static_cast<double>(count) : 0., worst, worstName.cStr(), count ? sumRaw / static_cast<double>(count) : 0.,
            worstRaw);
    }
}

// vr_hitmodel_bench [rays]: for each monster in the map, rays from all round aimed at random points in its box: how
// many meet the box, how many of those the model (the empty part of the box), and each test's cost.
void bench_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_hitmodel_bench: no server\n");
        return;
    }
    const int rays = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 2000;
    const float tol = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : tolerance(Class::Guns);
    const bool newest = Cmd_Argc() > 3 && !strcmp(Cmd_Argv(3), "newest"); // only the monster spawned last
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const bool wasOn = enabled();
    if(!wasOn)
    {
        Cvar_SetQuick(&vr_hit_precise, "1");
    }
    za::FastNonCryptoRng rng{1234u}; // the same rays every run
    const auto uni = [&rng] { return rng.getF(0.f, 1.f); };
    const float saveDebug = vr_debug_hits.value;
    vr_debug_hits.value = 0.f;
    Con_Printf("vr_hitmodel_bench: %d rays a monster, tolerance %.1f\n", rays, tol);
    ankerl::unordered_dense::map<za::String, bool> seen;
    int only = -1;
    for(int num = 1; newest && num < qcvm->num_edicts; num++)
    {
        only = target(EDICT_NUM(num)) ? num : only;
    }
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(!target(ent) || (newest && num != only))
        {
            continue;
        }
        const za::String name = PR_GetString(ent->v.classname);
        if(seen[name + (static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? " (corpse)" : "")])
        {
            continue;
        }
        seen[name + (static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? " (corpse)" : "")] = true;
        const glm::vec3 lo = vec(ent->v.absmin) + glm::vec3{1.f}, hi = vec(ent->v.absmax) - glm::vec3{1.f};
        const glm::vec3 c = (lo + hi) * 0.5f;
        const float reach = glm::length(hi - lo) * 2.f + 32.f;
        int boxHits = 0, modelHits = 0;
        double boxNs = 0., modelNs = 0.;
        const Stats before = stats;
        vec3_t zero{0.f, 0.f, 0.f};
        for(int r = 0; r < rays; r++)
        {
            const float z = uni() * 2.f - 1.f, phi = uni() * 6.2831853f, s = za::sqrt(za::max(0.f, 1.f - z * z));
            const glm::vec3 from = c + glm::vec3{s * za::cos(phi), s * za::sin(phi), z} * reach;
            const glm::vec3 aim = lo + (hi - lo) * glm::vec3{uni(), uni(), uni()};
            const glm::vec3 to = from + (aim - from) * 2.f;
            vec3_t a{from.x, from.y, from.z}, b{to.x, to.y, to.z};
            auto t0 = qza::nowNs();
            const trace_t tr = SV_ClipMoveToEntity(ent, a, zero, zero, b);
            boxNs += qza::nsSince(t0);
            if(tr.fraction < 1.f || tr.startsolid)
            {
                boxHits++;
            }
            t0 = qza::nowNs();
            Hit h;
            const bool hit = clip(ent, from, to, glm::vec3{0.f}, glm::vec3{0.f}, tol, Class::Guns, 1.f, h);
            modelNs += qza::nsSince(t0);
            modelHits += hit;
        }
        const long long narrow = stats.narrow - before.narrow;
        const aliashdr_t* hdr = nullptr;
        const Mesh* m = meshOf(static_cast<int>(ent->v.modelindex), modelOf(ent), hdr);
        // How far its model as drawn now reaches out of its box.
        float overhang = 0.f;
        Drawn d;
        if(drawnOf(ent, d))
        {
            const glm::vec3 blo = vec(ent->v.origin) + vec(ent->v.mins), bhi = vec(ent->v.origin) + vec(ent->v.maxs);
            for(const int p : {d.pose1, d.pose2})
            {
                const trivertx_t* v = posesOf(hdr) + static_cast<za::SizeT>(p) * m->numverts;
                for(int k = 0; k < m->numverts; k++)
                {
                    const glm::vec3 w = d.A * raw(v[k]) + d.b;
                    overhang = za::max(overhang, glm::length(w - glm::clamp(w, blo, bhi)));
                }
            }
        }
        Con_Printf("  %-22s %s: %4zu triangles, %3d poses (built in %.2f ms); box %d%%, model %d%% of the rays (%d%% of the box's); "
                   "box %.2f us, model %.2f us (%.1f nodes, %.1f triangles a test); the model reaches %.1f out of its box\n",
            name.cStr(), static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? "corpse" : "alive", m->tris.size(), m->numposes, m->buildMs,
            boxHits * 100 / rays, modelHits * 100 / rays, boxHits ? modelHits * 100 / boxHits : 0, boxNs / 1000. / rays,
            modelNs / 1000. / rays, narrow ? static_cast<double>(stats.nodes - before.nodes) / narrow : 0.,
            narrow ? static_cast<double>(stats.tris - before.tris) / narrow : 0., overhang);
    }
    vr_debug_hits.value = saveDebug;
    if(!wasOn)
    {
        Cvar_SetQuick(&vr_hit_precise, "0");
    }
    PR_PopQCVM(oldvm);
}

} // namespace qvr::hitmodel

// ----------------------------------------------------------------------------
// The engine's side (world.c, sv_phys.c).

extern "C" float VR_HitModelTolerance(int type)
{
    if(!(type & MOVE_HITMODEL) || !qvr::hitmodel::enabled())
    {
        return -1.f;
    }
    return qvr::hitmodel::tolerance(static_cast<qvr::hitmodel::Class>((type >> MOVE_HITMODEL_CLASS_SHIFT) & 3));
}

extern "C" int VR_HitModelTarget(edict_t* ent)
{
    return qvr::hitmodel::target(ent);
}

extern "C" int VR_HitModelClip(edict_t* ent, const float* start, const float* mins, const float* maxs, const float* end, int type,
    float tolerance, float maxfraction, trace_t* trace)
{
    using namespace qvr::hitmodel;
    const glm::vec3 a{start[0], start[1], start[2]}, b{end[0], end[1], end[2]};
    Hit h;
    const Class c = static_cast<Class>((type >> MOVE_HITMODEL_CLASS_SHIFT) & 3);
    const bool fine = vr_profile_fine != 0;
    if(fine)
    {
        VR_ProfileBegin("hit model");
    }
    const bool hit = clip(ent, a, b, glm::vec3{mins[0], mins[1], mins[2]}, glm::vec3{maxs[0], maxs[1], maxs[2]}, tolerance, c,
        maxfraction, h);
    if(fine)
    {
        VR_ProfileEnd();
    }
    if(!hit)
    {
        return 0;
    }
    memset(trace, 0, sizeof(*trace));
    trace->fraction = h.t;
    trace->startsolid = h.startInside;
    trace->allsolid = false;
    trace->inopen = true;
    trace->endpos[0] = h.point.x;
    trace->endpos[1] = h.point.y;
    trace->endpos[2] = h.point.z;
    trace->plane.normal[0] = h.normal.x;
    trace->plane.normal[1] = h.normal.y;
    trace->plane.normal[2] = h.normal.z;
    trace->plane.dist = glm::dot(h.normal, h.point);
    trace->ent = ent;
    return 1;
}

// A projectile's move (SV_PushEntity): a solid missile (MOVETYPE_FLYMISSILE, a bouncing grenade) meets monsters'
// models; its class from .vr_hitclass (the grappling hook's), else a gun's.
extern "C" int VR_HitModelMoveFlags(edict_t* ent)
{
    if(!qvr::hitmodel::enabled() || (static_cast<int>(ent->v.solid) != SOLID_BBOX && static_cast<int>(ent->v.solid) != SOLID_SLIDEBOX))
    {
        return 0;
    }
    if(static_cast<int>(ent->v.movetype) != MOVETYPE_FLYMISSILE && static_cast<int>(ent->v.movetype) != MOVETYPE_BOUNCE)
    {
        return 0;
    }
    const int cls = static_cast<int>(qvr::progs::fieldFloatOr(ent, qvr::progs::fields().vr_hitclass, 0.f));
    if(cls < 0)
    {
        return 0; // its box, as before
    }
    return MOVE_HITMODEL | ((cls & 3) << MOVE_HITMODEL_CLASS_SHIFT);
}
