// vr_agarmor.cpp -- armour for the armed humanoids, P0: shells made from the monster's own mesh, drawn on its authored
// rig (vr_agarmor.hpp has the method; ROUND22.md, "Armour for armed humanoids, P0").
//
// vr_agarmor_check [model] [random poses]: the shells' fit measured on the authored rig's poses (its rest, every joint
// near the pieces swept through its limits, ragdoll-like random poses within them): body vertices inside a plate (the
// body poking into or through it: inside the closed shell, by its winding number) and the plates' outer vertices
// inside the body (the plate sunk under the skin: the body showing over it), each by the region (neck and head,
// shoulders and arms, waist and legs, the torso itself) and its deepest. Ends on "agarmor check: N passed, M failed":
// the shells made, made alike twice (the same hash), closed, and clean in the rest pose.

#include "vr_testkit.hpp"
#include "vr_agarmor.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_mdldequant.hpp"
#include "vr_rig2.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

namespace qvr::agarmor
{

namespace
{

constexpr float pi = 3.14159265358979f;
constexpr int maxWorn = 128;  // armour entities a frame (up to three a body: P2's wear)
constexpr int maxSlots = 128; // the armour model's bones (each piece's bones its own)
constexpr int maxNamed = 256; // vr_agarmor_set's entities
constexpr const char* armorSuffix = "#armor";
constexpr const char* rigSuffix = "#rig2";
constexpr const char* skinModelName = "progs/vragarmor.mdl";
// Who wears it, and where the cuirass stops behind (model x; -1e9 nowhere): the enforcer's power pack is its own, so its
// cuirass is a breastplate to its back's surface (x -10: the pack from -10.5 to -23, up past its head; P2).
struct Wearer
{
    const char* model;
    float backCut;
};
constexpr Wearer wearers[] = {{"progs/vrgrunt.mdl", -1e9f}, {"progs/soldier.mdl", -1e9f}, {"progs/vrenforcer.mdl", -10.5f},
    {"progs/enforcer.mdl", -10.5f}};
// The ones with authored rigs the check runs on (no argument).
constexpr const char* checked[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl"};

// The atlas (make_agarmor.py's REGIONS): texels of its 256 x 128 skins.
struct Block
{
    float x0, y0, x1, y1;
};
constexpr Block cuirassBlock{0.f, 0.f, 160.f, 128.f};
constexpr Block helmetBlock{160.f, 0.f, 256.f, 64.f}; // the helmet's walls (mirrored left and right)
constexpr Block helmCapBlock{160.f, 64.f, 192.f, 96.f};
constexpr Block liningBlock{192.f, 64.f, 224.f, 96.f};
constexpr Block innerBlock{160.f, 96.f, 208.f, 128.f};
constexpr Block rimBlock{208.f, 96.f, 256.f, 128.f};

constexpr const char* pieceNames[NumPieces] = {"cuirass", "helmet", "pauldron_l", "pauldron_r", "bracer_l", "bracer_r", "thigh_l",
    "thigh_r", "shin_l", "shin_r"};
// A limb piece's bone (its region: the triangles weighing mostly on it), by piece.
constexpr const char* limbBone[NumPieces] = {nullptr, nullptr, "upperarm_l", "upperarm_r", "forearm_l", "forearm_r", "thigh_l", "thigh_r",
    "shin_l", "shin_r"};

template <typename T>
[[nodiscard]] T& at(za::Vector<T>& v, int i)
{
    return v[static_cast<za::SizeT>(i)];
}
template <typename T>
[[nodiscard]] const T& at(const za::Vector<T>& v, int i)
{
    return v[static_cast<za::SizeT>(i)];
}
template <typename T>
[[nodiscard]] int count(const za::Vector<T>& v)
{
    return static_cast<int>(v.size());
}

// A vertex's weights, every bone of the rig.
struct W
{
    float w[rig2::maxBones]{};
};
[[nodiscard]] W mixW(const W& a, const W& b, float t)
{
    W o;
    for(int i = 0; i < rig2::maxBones; i++)
    {
        o.w[i] = a.w[i] + (b.w[i] - a.w[i]) * t;
    }
    return o;
}
[[nodiscard]] int heaviest(const W& w)
{
    int best = 0;
    for(int i = 1; i < rig2::maxBones; i++)
    {
        best = w.w[i] > w.w[best] ? i : best;
    }
    return best;
}

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

// (a, b) -> slot: an open-addressed map of an undirected edge to a vertex or a count.
struct EdgeMap
{
    za::Vector<uint64_t> keys;
    za::Vector<int> vals;
    uint64_t mask{0};
    void init(int expected)
    {
        uint64_t size = 64;
        while(size < static_cast<uint64_t>(expected) * 2u + 16u)
        {
            size <<= 1u;
        }
        keys.clear();
        vals.clear();
        keys.resize(static_cast<za::SizeT>(size), ~0ull);
        vals.resize(static_cast<za::SizeT>(size), -1);
        mask = size - 1u;
    }
    // Its value's place (fresh: added, -1).
    int& get(int a, int b, bool& fresh)
    {
        const uint64_t lo = static_cast<uint64_t>(za::min(a, b)), hi = static_cast<uint64_t>(za::max(a, b));
        const uint64_t key = (hi << 32u) | lo;
        uint64_t h = (key * 0x9E3779B97F4A7C15ull) >> 20u;
        for(;;)
        {
            h &= mask;
            if(keys[static_cast<za::SizeT>(h)] == key)
            {
                fresh = false;
                return vals[static_cast<za::SizeT>(h)];
            }
            if(keys[static_cast<za::SizeT>(h)] == ~0ull)
            {
                keys[static_cast<za::SizeT>(h)] = key;
                fresh = true;
                return vals[static_cast<za::SizeT>(h)];
            }
            h++;
        }
    }
};

// ---- The body: the .mdl's rest pose as drawn, welded ----
struct Body
{
    za::Vector<glm::vec3> mdlRest; // per .mdl vertex
    za::Vector<int> weld;          // .mdl vertex -> welded vertex
    za::Vector<int> rep;           // welded vertex -> a .mdl vertex
    za::Vector<glm::vec3> p, n;    // per welded vertex
    za::Vector<W> w;
    za::Vector<int> tris;          // welded, wound outward (cross(b - a, c - a) out)
    bool nativeFlip{false};        // the .mdl's own winding the other way
};

void normalsOf(const za::Vector<glm::vec3>& p, const za::Vector<int>& tris, za::Vector<glm::vec3>& n)
{
    n.clear();
    n.resize(p.size(), glm::vec3{0.f});
    for(int t = 0; t + 2 < count(tris); t += 3)
    {
        const int a = at(tris, t), b = at(tris, t + 1), c = at(tris, t + 2);
        const glm::vec3 f = glm::cross(at(p, b) - at(p, a), at(p, c) - at(p, a)); // (area-weighted)
        at(n, a) += f;
        at(n, b) += f;
        at(n, c) += f;
    }
    for(glm::vec3& x : n)
    {
        x = safeNormalize(x, glm::vec3{0.f, 0.f, 1.f});
    }
}

bool buildBody(qmodel_t* src, const aliashdr_t* sh, const rig2::Rig& rig, Body& b)
{
    b = Body{};
    const int nm = sh->numverts;
    if(nm <= 0 || sh->numverts_vbo <= 0)
    {
        return false;
    }
    mdldequant::prepare(src);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(sh) + sh->indexes);
    b.mdlRest.resize(static_cast<za::SizeT>(nm), glm::vec3{0.f});
    za::Vector<uint8_t> filled(static_cast<za::SizeT>(nm), uint8_t{0});
    for(int v = 0; v < sh->numverts_vbo; v++)
    {
        const int mv = za::clamp(static_cast<int>(desc[v].vertindex), 0, nm - 1);
        if(!at(filled, mv))
        {
            float xyz[3];
            signed char nrm[3];
            mdldequant::restVertex(sh, v, xyz, nrm);
            at(b.mdlRest, mv) = glm::vec3{xyz[0], xyz[1], xyz[2]};
            at(filled, mv) = 1;
        }
    }
    b.weld.resize(static_cast<za::SizeT>(nm), 0);
    for(int mv = 0; mv < nm; mv++)
    {
        int found = -1;
        for(int k = 0; k < count(b.rep) && found < 0; k++)
        {
            found = glm::length(at(b.p, k) - at(b.mdlRest, mv)) < 1e-3f ? k : -1;
        }
        if(found < 0)
        {
            found = count(b.rep);
            b.rep.pushBack(mv);
            b.p.pushBack(at(b.mdlRest, mv));
            W w;
            const rig2::Weights* rw = mv < count(rig.weights) ? &at(rig.weights, mv) : nullptr;
            for(int i = 0; rw && i < rw->n; i++)
            {
                w.w[za::clamp(rw->bone[i], 0, rig2::maxBones - 1)] += rw->w[i];
            }
            if(!rw || rw->n == 0)
            {
                w.w[0] = 1.f;
            }
            b.w.pushBack(w);
        }
        at(b.weld, mv) = found;
    }
    double vol = 0.0;
    for(int i = 0; i + 2 < sh->numindexes; i += 3)
    {
        int t[3];
        for(int k = 0; k < 3; k++)
        {
            t[k] = at(b.weld, za::clamp(static_cast<int>(desc[idx[i + k]].vertindex), 0, nm - 1));
        }
        if(t[0] == t[1] || t[1] == t[2] || t[0] == t[2])
        {
            continue;
        }
        const glm::vec3 &pa = at(b.p, t[0]), &pb = at(b.p, t[1]), &pc = at(b.p, t[2]);
        if(glm::length(glm::cross(pb - pa, pc - pa)) < 1e-6f)
        {
            continue;
        }
        vol += static_cast<double>(glm::dot(pa, glm::cross(pb, pc)));
        for(int k = 0; k < 3; k++)
        {
            b.tris.pushBack(t[k]);
        }
    }
    b.nativeFlip = vol < 0.0;
    for(int i = 0; b.nativeFlip && i + 2 < count(b.tris); i += 3)
    {
        const int x = at(b.tris, i + 1);
        at(b.tris, i + 1) = at(b.tris, i + 2);
        at(b.tris, i + 2) = x;
    }
    normalsOf(b.p, b.tris, b.n);
    return !b.tris.empty();
}

// ---- A piece: where it is on the body ----
struct PieceFrame
{
    uint32_t bones{0};            // the region's bones
    glm::vec3 origin{0.f}, up{0.f, 0.f, 1.f}, fwd{1.f, 0.f, 0.f}, left{0.f, 1.f, 0.f};
    float length{1.f};
    float backCut{-1e9f}; // (the cuirass: nothing behind this model x; Wearer::backCut)
    bool ok{false};
};

[[nodiscard]] float backCutOf(const char* model, int piece)
{
    for(const Wearer& w : wearers)
    {
        if(piece == Cuirass && model && !strcmp(model, w.model))
        {
            return w.backCut;
        }
    }
    return -1e9f;
}

[[nodiscard]] PieceFrame frameOf(const rig2::Rig& rig, int piece)
{
    PieceFrame f;
    const auto bit = [&](const char* name) {
        const int b = rig2::boneByName(rig, name);
        f.ok = f.ok || b >= 0;
        return b >= 0 ? 1u << static_cast<unsigned>(b) : 0u;
    };
    if(piece == Cuirass)
    {
        f.bones = bit("spine") | bit("chest");
        const int s = rig2::boneByName(rig, "spine"), c = rig2::boneByName(rig, "chest");
        f.ok = s >= 0 && c >= 0;
        if(f.ok)
        {
            f.origin = rig.bones[s].pivot;
            f.up = safeNormalize(rig.bones[c].end - rig.bones[s].pivot, glm::vec3{0.f, 0.f, 1.f});
            f.length = glm::length(rig.bones[c].end - rig.bones[s].pivot);
        }
    }
    else if(limbBone[piece])
    {
        // A limb: round its bone, up it from its pivot (the texture's rows run down it).
        f.bones = bit(limbBone[piece]);
        const int b = rig2::boneByName(rig, limbBone[piece]);
        f.ok = b >= 0;
        if(f.ok)
        {
            const glm::vec3 d = rig.bones[b].end - rig.bones[b].pivot;
            f.up = safeNormalize(d, glm::vec3{0.f, 0.f, 1.f});
            f.length = za::max(glm::length(d), 1.f);
            f.origin = rig.bones[b].pivot;
        }
    }
    else
    {
        f.bones = bit("head");
        const int h = rig2::boneByName(rig, "head");
        f.ok = h >= 0;
        if(f.ok)
        {
            const glm::vec3 d = rig.bones[h].end - rig.bones[h].pivot;
            f.up = safeNormalize(d, glm::vec3{0.f, 0.f, 1.f});
            f.length = za::max(glm::length(d), 1.f);
            f.origin = rig.bones[h].pivot;
        }
    }
    f.fwd = safeNormalize(glm::vec3{1.f, 0.f, 0.f} - f.up * f.up.x, glm::vec3{1.f, 0.f, 0.f});
    f.left = glm::cross(f.up, f.fwd);
    return f;
}

// The piece's field at a body vertex: >= 0 covered (its weight on the region's bones past 0.5).
[[nodiscard]] float fieldOf(const Body& b, int v, const PieceFrame& f)
{
    const W& w = at(b.w, v);
    float region = 0.f;
    for(int i = 0; i < rig2::maxBones; i++)
    {
        region += ((f.bones >> static_cast<unsigned>(i)) & 1u) ? w.w[i] : 0.f;
    }
    // (behind the back's cut: out, linear in x so the crossing lands on the cut)
    const float field = za::min(region - 0.5f, (at(b.p, v).x - f.backCut) * 0.25f);
    return za::abs(field) < 1e-4f ? 1e-4f : field;
}

// ---- The shell ----
struct Vert
{
    glm::vec3 p{0.f}, n{0.f};
    glm::vec2 st{0.f}; // atlas texels
    W w;
    int piece{0}, layer{0}; // layer: 0 outer, 1 inner, 2 rim
};

struct Shell
{
    za::String model;
    int generation{-1};
    bool ok{false};
    Body body;
    za::Vector<Vert> verts;
    za::Vector<int> tris; // wound outward
    int pieceTris[NumPieces]{}, pieceVerts[NumPieces]{}, pieceOpen[NumPieces]{};
    int numSlots{0};
    int slotBone[maxSlots]{}, slotPiece[maxSlots]{};
    uint32_t hash{0};
    float made{0.f};    // ms
    char helmFit[320]{}; // the helmet's rings against the head (vr_agarmor_info, the check)
};

// A piece's surface as made (before the shell): its vertices on the body (orig), smoothed (p), their weights.
struct Local
{
    za::Vector<glm::vec3> p, orig;
    za::Vector<W> w;
    za::Vector<int> tris;
};

void clipPiece(const Body& b, const PieceFrame& f, Local& L)
{
    L = Local{};
    const int nv = count(b.p);
    za::Vector<float> field(static_cast<za::SizeT>(nv), 0.f);
    for(int v = 0; v < nv; v++)
    {
        at(field, v) = fieldOf(b, v, f);
    }
    za::Vector<int> local(static_cast<za::SizeT>(nv), -1);
    EdgeMap crossings;
    crossings.init(count(b.tris));
    const auto vertOf = [&](int v) {
        if(at(local, v) < 0)
        {
            at(local, v) = count(L.p);
            L.p.pushBack(at(b.p, v));
            L.orig.pushBack(at(b.p, v));
            L.w.pushBack(at(b.w, v));
        }
        return at(local, v);
    };
    const auto crossingOf = [&](int i, int j) {
        bool fresh = false;
        int& slot = crossings.get(i, j, fresh);
        if(fresh)
        {
            const int a = za::min(i, j), c = za::max(i, j);
            const float t = at(field, a) / (at(field, a) - at(field, c));
            slot = count(L.p);
            const glm::vec3 p = at(b.p, a) + (at(b.p, c) - at(b.p, a)) * t;
            L.p.pushBack(p);
            L.orig.pushBack(p);
            L.w.pushBack(mixW(at(b.w, a), at(b.w, c), t));
        }
        return slot;
    };
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        int poly[4];
        int np = 0;
        for(int k = 0; k < 3; k++)
        {
            const int i = at(b.tris, t + k), j = at(b.tris, t + (k + 1) % 3);
            const bool in = at(field, i) >= 0.f, jin = at(field, j) >= 0.f;
            if(in)
            {
                poly[np++] = vertOf(i);
            }
            if(in != jin)
            {
                poly[np++] = crossingOf(i, j);
            }
        }
        for(int k = 1; k + 1 < np; k++)
        {
            L.tris.pushBack(poly[0]);
            L.tris.pushBack(poly[k]);
            L.tris.pushBack(poly[k + 1]);
        }
    }
    // Only its big parts (a stray patch of weight elsewhere dropped): components by shared vertices.
    const int nt = count(L.tris) / 3, lv = count(L.p);
    za::Vector<int> parent(static_cast<za::SizeT>(lv), 0);
    for(int v = 0; v < lv; v++)
    {
        at(parent, v) = v;
    }
    const auto find = [&](int v) {
        while(at(parent, v) != v)
        {
            at(parent, v) = at(parent, at(parent, v));
            v = at(parent, v);
        }
        return v;
    };
    for(int t = 0; t < nt; t++)
    {
        for(int k = 1; k < 3; k++)
        {
            const int a = find(at(L.tris, 3 * t)), c = find(at(L.tris, 3 * t + k));
            at(parent, za::max(a, c)) = za::min(a, c);
        }
    }
    za::Vector<int> size(static_cast<za::SizeT>(lv), 0);
    for(int t = 0; t < nt; t++)
    {
        at(size, find(at(L.tris, 3 * t)))++;
    }
    int biggest = 0;
    for(int v = 0; v < lv; v++)
    {
        biggest = za::max(biggest, at(size, v));
    }
    Local kept;
    za::Vector<int> remap(static_cast<za::SizeT>(lv), -1);
    for(int t = 0; t < nt; t++)
    {
        if(at(size, find(at(L.tris, 3 * t))) * 5 < biggest)
        {
            continue;
        }
        for(int k = 0; k < 3; k++)
        {
            const int v = at(L.tris, 3 * t + k);
            if(at(remap, v) < 0)
            {
                at(remap, v) = count(kept.p);
                kept.p.pushBack(at(L.p, v));
                kept.orig.pushBack(at(L.orig, v));
                kept.w.pushBack(at(L.w, v));
            }
            kept.tris.pushBack(at(remap, v));
        }
    }
    L = ZA_MOVE(kept);
}

void subdivide(Local& L)
{
    EdgeMap mids;
    mids.init(count(L.tris));
    const auto midOf = [&](int a, int b) {
        bool fresh = false;
        int& slot = mids.get(a, b, fresh);
        if(fresh)
        {
            slot = count(L.p);
            const glm::vec3 p = (at(L.p, a) + at(L.p, b)) * 0.5f, o = (at(L.orig, a) + at(L.orig, b)) * 0.5f;
            const W w = mixW(at(L.w, a), at(L.w, b), 0.5f);
            L.p.pushBack(p);
            L.orig.pushBack(o);
            L.w.pushBack(w);
        }
        return slot;
    };
    za::Vector<int> out;
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        const int a = at(L.tris, t), b = at(L.tris, t + 1), c = at(L.tris, t + 2);
        const int ab = midOf(a, b), bc = midOf(b, c), ca = midOf(c, a);
        const int quad[12] = {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca};
        for(int k : quad)
        {
            out.pushBack(k);
        }
    }
    L.tris = ZA_MOVE(out);
}

// Its edges: each vertex's neighbours, and along its open edge only for the open edge's vertices.
void neighbours(const Local& L, za::Vector<za::Vector<int>>& nb, za::Vector<uint8_t>& open, za::Vector<int>& openEdges)
{
    const int nv = count(L.p);
    nb.clear();
    nb.resize(static_cast<za::SizeT>(nv));
    open.clear();
    open.resize(static_cast<za::SizeT>(nv), uint8_t{0});
    openEdges.clear();
    EdgeMap uses;
    uses.init(count(L.tris));
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            bool fresh = false;
            int& n = uses.get(at(L.tris, t + k), at(L.tris, t + (k + 1) % 3), fresh);
            n = fresh ? 1 : n + 1;
        }
    }
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = at(L.tris, t + k), b = at(L.tris, t + (k + 1) % 3);
            bool fresh = false;
            const int n = uses.get(a, b, fresh);
            if(n == 1)
            {
                at(open, a) = at(open, b) = 1;
                openEdges.pushBack(a); // (in its triangle's winding: the rim faces out of it)
                openEdges.pushBack(b);
            }
        }
    }
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = at(L.tris, t + k), b = at(L.tris, t + (k + 1) % 3);
            bool fresh = false;
            const int n = uses.get(a, b, fresh);
            const bool edgeOpen = n == 1;
            // An open vertex is smoothed along the open edge only; an inner one by all.
            for(int s = 0; s < 2; s++)
            {
                const int x = s ? b : a, y = s ? a : b;
                if(at(open, x) && !edgeOpen)
                {
                    continue;
                }
                bool have = false;
                for(int z : at(nb, x))
                {
                    have = have || z == y;
                }
                if(!have)
                {
                    at(nb, x).pushBack(y);
                }
            }
        }
    }
}

void smooth(Local& L, int iterations)
{
    za::Vector<za::Vector<int>> nb;
    za::Vector<uint8_t> open;
    za::Vector<int> openEdges;
    neighbours(L, nb, open, openEdges);
    const int nv = count(L.p);
    za::Vector<glm::vec3> d(static_cast<za::SizeT>(nv), glm::vec3{0.f});
    for(int it = 0; it < iterations * 2; it++)
    {
        const float k = (it & 1) ? -0.53f : 0.5f; // Taubin: shrink, then grow back
        for(int v = 0; v < nv; v++)
        {
            glm::vec3 sum{0.f};
            for(int u : at(nb, v))
            {
                sum += at(L.p, u);
            }
            at(d, v) = at(nb, v).empty() ? glm::vec3{0.f} : sum / static_cast<float>(at(nb, v).size()) - at(L.p, v);
        }
        for(int v = 0; v < nv; v++)
        {
            at(L.p, v) += at(d, v) * k;
        }
    }
}

[[nodiscard]] glm::vec2 toBlock(const Block& b, float a, float c)
{
    return glm::vec2{b.x0 + 0.5f + za::clamp(a, 0.f, 1.f) * (b.x1 - b.x0 - 1.f), b.y0 + 0.5f + za::clamp(c, 0.f, 1.f) * (b.y1 - b.y0 - 1.f)};
}

void addTri(Shell& s, int a, int b, int c)
{
    s.tris.pushBack(a);
    s.tris.pushBack(b);
    s.tris.pushBack(c);
}

// A piece's shell from its surface (the cuirass, a limb's plate): outer and inner layers, a rim along its open edge,
// texture coordinates round its frame's axis and up it (a limb's plate wears the cuirass's paint, its lames round it).
void shellOf(Shell& s, Local& L, const PieceFrame& f, int piece)
{
    const float gap = za::max(vr_agarmor_gap.value, 0.f), thick = za::max(vr_agarmor_thick.value, 0.05f);
    const int nv = count(L.p);
    za::Vector<glm::vec3> n;
    normalsOf(L.p, L.tris, n);
    // Never under the body where it was (smoothing shrinks the convex parts): pushed out along its normal.
    for(int v = 0; v < nv; v++)
    {
        const float d = glm::dot(at(L.orig, v) - at(L.p, v), at(n, v));
        at(L.p, v) += d > 0.f ? at(n, v) * d : glm::vec3{0.f};
    }
    normalsOf(L.p, L.tris, n);

    za::Vector<za::Vector<int>> nb;
    za::Vector<uint8_t> open;
    za::Vector<int> openEdges;
    neighbours(L, nb, open, openEdges);

    // Texture coordinates (a, b) of the outer place: round the torso (its front in the middle) and up it.
    za::Vector<glm::vec2> ab(static_cast<za::SizeT>(nv), glm::vec2{0.f});
    float hLo = 1e9f, hHi = -1e9f;
    for(int v = 0; v < nv; v++)
    {
        const float h = glm::dot(at(L.p, v) - f.origin, f.up);
        hLo = za::min(hLo, h);
        hHi = za::max(hHi, h);
    }
    for(int v = 0; v < nv; v++)
    {
        const glm::vec3 q = at(L.p, v) - f.origin;
        const float az = za::atan2(glm::dot(q, f.left), glm::dot(q, f.fwd)) / (2.f * pi); // -0.5..0.5, 0 the front
        const float h = glm::dot(q, f.up);
        at(ab, v) = glm::vec2{az + 0.5f, 1.f - (h - hLo) / za::max(hHi - hLo, 1e-3f)};
    }
    const Block& outerBlock = cuirassBlock;

    // Outer and inner layers: each vertex twice (a cuirass vertex again past the seam at its back for the triangles
    // across it).
    const int base = count(s.verts);
    for(int layer = 0; layer < 2; layer++)
    {
        for(int v = 0; v < nv; v++)
        {
            Vert o;
            o.p = at(L.p, v) + at(n, v) * (layer == 0 ? gap + thick : gap);
            o.n = layer == 0 ? at(n, v) : -at(n, v);
            const glm::vec2 a = at(ab, v);
            o.st = layer == 0 ? toBlock(outerBlock, a.x, a.y) : toBlock(innerBlock, a.x, a.y);
            o.w = at(L.w, v);
            o.piece = piece;
            o.layer = layer;
            s.verts.pushBack(o);
        }
    }
    za::Vector<int> seam(static_cast<za::SizeT>(nv * 2), -1);
    for(int t = 0; t + 2 < count(L.tris); t += 3)
    {
        for(int layer = 0; layer < 2; layer++)
        {
            int c[3];
            float lo = 2.f, hi = -1.f;
            for(int k = 0; k < 3; k++)
            {
                c[k] = at(L.tris, t + k);
                lo = za::min(lo, at(ab, c[k]).x);
                hi = za::max(hi, at(ab, c[k]).x);
            }
            int out[3];
            for(int k = 0; k < 3; k++)
            {
                out[k] = base + layer * nv + c[k];
                if(hi - lo > 0.5f && at(ab, c[k]).x < 0.5f)
                {
                    // across the seam: this corner at the seam's far side (a = 1)
                    int& dup = at(seam, layer * nv + c[k]);
                    if(dup < 0)
                    {
                        Vert o = at(s.verts, out[k]);
                        o.st = toBlock(layer == 0 ? outerBlock : innerBlock, 1.f, at(ab, c[k]).y);
                        dup = count(s.verts);
                        s.verts.pushBack(o);
                    }
                    out[k] = dup;
                }
            }
            if(layer == 0)
            {
                addTri(s, out[0], out[1], out[2]);
            }
            else
            {
                addTri(s, out[0], out[2], out[1]);
            }
        }
    }
    // The rim: a quad along each open edge, from the outer layer to the inner, facing out of the plate.
    for(int e = 0; e + 1 < count(openEdges); e += 2)
    {
        const int a = at(openEdges, e), b = at(openEdges, e + 1);
        const glm::vec3 oa = at(L.p, a) + at(n, a) * (gap + thick), ob = at(L.p, b) + at(n, b) * (gap + thick);
        const glm::vec3 ia = at(L.p, a) + at(n, a) * gap, ib = at(L.p, b) + at(n, b) * gap;
        const glm::vec3 face = safeNormalize(glm::cross(ia - oa, ib - oa), at(n, a));
        const glm::vec3 places[4] = {oa, ia, ib, ob};
        const int from[4] = {a, a, b, b};
        const float vb[4] = {0.1f, 0.9f, 0.9f, 0.1f};
        const int first = count(s.verts);
        for(int k = 0; k < 4; k++)
        {
            Vert o;
            o.p = places[k];
            o.n = face;
            o.st = toBlock(rimBlock, 0.25f + 0.5f * static_cast<float>(k == 2 || k == 3), vb[k]);
            o.w = at(L.w, from[k]);
            o.piece = piece;
            o.layer = 2;
            s.verts.pushBack(o);
        }
        addTri(s, first, first + 1, first + 2);
        addTri(s, first, first + 2, first + 3);
    }
    s.pieceOpen[piece] = count(openEdges) / 2;
}

// ---- The helmet: the Ranger's (the player's face in the status bar), built round the head ----
// A flat-topped bucket over the crown (its top edge chamfered, its lower edge a ledge over the brow), cheek guards from
// under it down the sides of the face to the jaw line, the back down to the nape, the face open (the eyes under the
// brow; no side brackets: they stood off as flanges, 2026-10-29). Fitted close: a small gap and taper, round chamfers,
// the guards round the head proper (not the collar blended onto it). In the head bone's frame (x forward, y left, z up its length from its pivot) each ring of it is an
// octagon (a front, a back, two sides, four chamfers) fitted round the head's vertices a gap off them: the ring takes
// every vertex of the bands above and below it (a face vertex only above the guards), so any place between two rings is
// clear too. Rigid on the head bone. Texture: the walls mirrored left and right (u from the front's middle round the
// side to the back's, v down the rows: make_agarmor.py's HELM_U and HELM_V), the cap a disc, the open edges the lining.
struct Ring
{
    float F{0.f}, B{0.f}, W{0.f}; // front, back, half width
    float Df{0.f}, Db{0.f};       // the chamfers: x + |y| <= Df, -x + |y| <= Db
};
enum HelmRow : int
{
    HCap,    // the top's edge
    HCrown,  // below its chamfer
    HBrow,   // the bucket's lower edge
    HLedge,  // under it, stepped in: the guards' top (the face's top in front)
    HCheek,  // the cheekbones
    HJaw,    // the jaw line (lower at the back: the nape)
    NumHelmRows
};
constexpr float helmV[NumHelmRows] = {0.f, 0.08f, 0.38f, 0.46f, 0.7f, 1.f};
// The octagon's points on its left half, front to back: the front's middle, the front chamfer's ends, the back chamfer's
// ends, the back's middle; round the helmet: 0..5 down its left, 6..9 back up its right.
constexpr int numHelmPoints = 6, numHelmCols = 10;
constexpr float helmU[numHelmPoints] = {0.f, 0.16f, 0.3f, 0.7f, 0.84f, 1.f};
[[nodiscard]] int pointOf(int col)
{
    return col <= 5 ? col : 10 - col;
}
[[nodiscard]] glm::vec2 ringPoint(const Ring& r, int point, int col)
{
    const float side = col <= 5 ? 1.f : -1.f;
    glm::vec2 p{0.f};
    switch(point)
    {
    case 0: p = {r.F, 0.f}; break;
    case 1: p = {r.F, r.Df - r.F}; break;
    case 2: p = {r.Df - r.W, r.W}; break;
    case 3: p = {r.W - r.Db, r.W}; break;
    case 4: p = {-r.B, r.Db - r.B}; break;
    default: p = {-r.B, 0.f}; break;
    }
    return glm::vec2{p.x, p.y * side};
}
[[nodiscard]] Ring grown(const Ring& r, float d)
{
    constexpr float root2 = 1.41421356f;
    return Ring{r.F + d, r.B + d, r.W + d, r.Df + d * root2, r.Db + d * root2};
}
// The lowest row of the strip from column c to c + 1: the face's two strips end under the bucket.
[[nodiscard]] int stripBottom(int c)
{
    return c == 0 || c == numHelmCols - 1 ? HLedge : HJaw;
}

struct HelmBuild
{
    Shell& s;
    glm::vec3 origin, fwd, left, up;
    W w;
    glm::vec3 toModel(const glm::vec3& h) const { return origin + fwd * h.x + left * h.y + up * h.z; }
    // A flat face (3 or 4 corners, wound outward), its own vertices.
    void face(const glm::vec3* p, const glm::vec2* st, int n, int layer)
    {
        const glm::vec3 a = toModel(p[0]), b = toModel(p[1]), c = toModel(p[2]);
        const glm::vec3 nrm = safeNormalize(glm::cross(b - a, c - a), up);
        const int first = count(s.verts);
        for(int k = 0; k < n; k++)
        {
            Vert o;
            o.p = toModel(p[k]);
            o.n = nrm;
            o.st = st[k];
            o.w = w;
            o.piece = Helmet;
            o.layer = layer;
            s.verts.pushBack(o);
        }
        for(int k = 1; k + 1 < n; k++)
        {
            addTri(s, first, first + k, first + k + 1);
        }
    }
};

void helmetOf(Shell& s, const Body& b, const PieceFrame& f, int headBone)
{
    // (rigid on the head: closer than the body's plates, and thinner; vr_agarmor_helmet_gap, _thick)
    const float gap = za::max(vr_agarmor_helmet_gap.value, 0.f), thick = za::max(vr_agarmor_helmet_thick.value, 0.05f);
    const float L = f.length;
    constexpr float root2 = 1.41421356f;
    // The head: its surface where it is weighted to it (its neck's top too; points across each triangle, the low-poly
    // head's big triangles having no vertices between its crown and its jaw), in its frame, |y| (the helmet is symmetric).
    za::Vector<glm::vec3> pts;
    za::Vector<float> ptW; // (its weight on the head)
    float top = 0.f;
    constexpr int steps = 6;
    for(int t = 0; t + 2 < count(b.tris); t += 3)
    {
        const int i0 = at(b.tris, t), i1 = at(b.tris, t + 1), i2 = at(b.tris, t + 2);
        const float w0 = at(b.w, i0).w[headBone], w1 = at(b.w, i1).w[headBone], w2 = at(b.w, i2).w[headBone];
        if(za::max(w0, w1, w2) < 0.2f)
        {
            continue;
        }
        for(int u = 0; u <= steps; u++)
        {
            for(int v = 0; u + v <= steps; v++)
            {
                const float bu = static_cast<float>(u) / steps, bv = static_cast<float>(v) / steps, bw = 1.f - bu - bv;
                const float wp = w0 * bw + w1 * bu + w2 * bv;
                if(wp < 0.2f)
                {
                    continue;
                }
                ptW.pushBack(wp);
                const glm::vec3 q = at(b.p, i0) * bw + at(b.p, i1) * bu + at(b.p, i2) * bv - f.origin;
                const glm::vec3 h{glm::dot(q, f.fwd), za::abs(glm::dot(q, f.left)), glm::dot(q, f.up)};
                pts.pushBack(h);
                top = za::max(top, h.z);
            }
        }
    }
    if(pts.empty())
    {
        return;
    }
    // The rows' heights (inner surface) per point: the bucket's lower edge and the guards' rows lower toward the back,
    // the jaw's down to the nape there.
    const float chamfer = 0.18f * L, faceHalf = 0.4f * L, taper = 0.03f * L;
    const float zTop = top + gap;
    float rowZ[NumHelmRows][numHelmPoints];
    const float zRow[NumHelmRows] = {zTop, zTop - chamfer, vr_agarmor_brow_front.value * L, (vr_agarmor_brow_front.value - 0.06f) * L, 0.3f * L, 0.04f * L};
    constexpr float browDrop[numHelmPoints] = {0.f, 0.f, 0.04f, 0.13f, 0.2f, 0.2f}, cheekDrop[numHelmPoints] = {0.f, 0.f, 0.f, 0.03f, 0.07f, 0.07f};
    for(int r = 0; r < NumHelmRows; r++)
    {
        for(int k = 0; k < numHelmPoints; k++)
        {
            const float drop = r == HBrow || r == HLedge ? browDrop[k] : r == HCheek ? cheekDrop[k] : 0.f;
            rowZ[r][k] = zRow[r] - drop * L;
        }
    }
    const float nape = za::min(vr_agarmor_brow_back.value * L, rowZ[HCheek][5] - 0.05f * L);
    rowZ[HJaw][3] = nape; // (the side's back corner clear of the neck as the head turns)
    rowZ[HJaw][4] = rowZ[HJaw][5] = nape;
    // The fit: each ring round every vertex of its bands (the cap's ring the crown's, stepped in).
    Ring ring[NumHelmRows];
    for(Ring& r : ring)
    {
        r = Ring{0.1f * L, 0.1f * L, 0.1f * L, 0.2f * L, 0.2f * L};
    }
    const auto need = [&](Ring& r, const glm::vec3& p, float in) {
        r.F = za::max(r.F, p.x + gap + in);
        r.B = za::max(r.B, -p.x + gap + in);
        r.W = za::max(r.W, p.y + gap + in);
        r.Df = za::max(r.Df, p.x + p.y + (gap + in) * root2);
        r.Db = za::max(r.Db, -p.x + p.y + (gap + in) * root2);
    };
    // A row's height at a place front to back (the points' places about the head's front and back: its chamfers' ends
    // near half way to its middle).
    float xFront = 0.f, xBack = 0.f;
    for(const glm::vec3& p : pts)
    {
        xFront = za::max(xFront, p.x);
        xBack = za::max(xBack, -p.x);
    }
    const float knotX[4] = {xFront, 0.5f * xFront, -0.5f * xBack, -xBack};
    const auto zAt = [&](int r, float x) {
        if(x >= knotX[0])
        {
            return rowZ[r][1];
        }
        for(int i = 0; i < 3; i++)
        {
            if(x >= knotX[i + 1])
            {
                const float t = (knotX[i] - x) / za::max(knotX[i] - knotX[i + 1], 1e-3f);
                return rowZ[r][i + 1] + (rowZ[r][i + 2] - rowZ[r][i + 1]) * t;
            }
        }
        return rowZ[r][4];
    };
    constexpr float margin = 0.25f;
    for(int r = 0; r + 1 < NumHelmRows; r++)
    {
        for(int i = 0; i < count(pts); i++)
        {
            const glm::vec3& p = at(pts, i);
            const float hi = zAt(r, p.x), lo = zAt(r + 1, p.x);
            const bool face = r + 1 > HLedge && p.x > 0.f && p.y < faceHalf;
            // (the guards round the head proper: not the collar's vertices blended onto it, the enforcer's out to its
            // shoulders, that made its guards flanges)
            const bool collar = r >= HLedge && at(ptW, i) < 0.5f;
            if(p.z > hi + margin || p.z < lo - margin || face || collar)
            {
                continue;
            }
            if(r == HCrown)
            {
                // the bucket's wall, its brow ring the crown's grown by the taper: the crown's ring as far in as that
                // lets it (a point a share s down the wall needs crown + taper s)
                const float s = za::clamp((hi - p.z) / za::max(hi - lo, 1e-3f), 0.f, 1.f);
                need(ring[HCrown], p, -taper * s);
                continue;
            }
            // (the cap's chamfer: a point a share s up it needs the crown's ring chamfer s further out)
            const float up = r == HCap ? za::clamp((p.z - lo) / za::max(hi - lo, 1e-3f), 0.f, 1.f) : 0.f;
            need(ring[r == HCap ? HCrown : r], p, chamfer * up);
            need(ring[r + 1], p, 0.f);
        }
    }
    // The guards no wider than the ledge over them and a little (what sticks out further below is the neck's and the
    // collar's, not the head's: the enforcer's shoulders made its guards side flanges).
    for(int r = HCheek; r < NumHelmRows; r++)
    {
        ring[r].W = za::min(ring[r].W, ring[HLedge].W + 0.1f * L);
    }
    // The style: the bucket a touch wider at its brow than its crown and a small ledge over the guards; its chamfers at
    // most 0.7 of its side (the guards' half: round, close to the head); the face's opening at least faceHalf wide.
    Ring& crown = ring[HCrown];
    Ring& brow = ring[HBrow];
    const Ring wider = grown(crown, taper), ledge = grown(ring[HLedge], 0.015f * L);
    brow = Ring{za::max(brow.F, wider.F, ledge.F), za::max(brow.B, wider.B, ledge.B), za::max(brow.W, wider.W, ledge.W),
        za::max(brow.Df, wider.Df, ledge.Df), za::max(brow.Db, wider.Db, ledge.Db)};
    for(int r = HCrown; r < NumHelmRows; r++)
    {
        Ring& x = ring[r];
        const float cut = (r <= HBrow ? 0.7f : 0.5f) * x.W; // the bucket rounder than the guards
        x.Df = za::clamp(za::max(x.Df, x.F + x.W - cut), 0.f, x.F + x.W);
        x.Db = za::clamp(za::max(x.Db, x.B + x.W - 0.7f * x.W), 0.f, x.B + x.W); // (the back round: clear of the upper back)
        x.Df = r >= HLedge ? za::clamp(x.F + faceHalf, x.Df, x.F + x.W) : x.Df;
    }
    ring[HCap] = grown(crown, -chamfer);

    HelmBuild hb{s, f.origin, f.fwd, f.left, f.up, W{}};
    hb.w.w[headBone] = 1.f;
    // The grid: (column, row) -> its place, inner (layer 1) and outer (layer 0: grown a thickness, its top raised).
    const auto place = [&](int col, int row, int layer) {
        const Ring r = layer == 0 ? grown(ring[row], thick) : ring[row];
        const glm::vec2 xy = ringPoint(r, pointOf(col), col);
        const float z = rowZ[row][pointOf(col)] + (layer == 0 && row <= HCrown ? thick : 0.f);
        return glm::vec3{xy.x, xy.y, z};
    };
    const auto wallST = [&](int col, int row, int layer) {
        return toBlock(layer == 0 ? helmetBlock : innerBlock, helmU[pointOf(col)], helmV[row]);
    };
    for(int layer = 0; layer < 2; layer++)
    {
        for(int c = 0; c < numHelmCols; c++)
        {
            const int d = (c + 1) % numHelmCols;
            for(int r = 0; r < stripBottom(c); r++)
            {
                glm::vec3 p[4] = {place(c, r, layer), place(c, r + 1, layer), place(d, r + 1, layer), place(d, r, layer)};
                glm::vec2 st[4] = {wallST(c, r, layer), wallST(c, r + 1, layer), wallST(d, r + 1, layer), wallST(d, r, layer)};
                // (round from the front to the left: c, its row below, then d's, is wound outward)
                if(layer == 1)
                {
                    za::genericSwap(p[1], p[3]);
                    za::genericSwap(st[1], st[3]);
                }
                hb.face(p, st, 4, layer);
            }
        }
        // The cap: a fan from its middle.
        const Ring cr = layer == 0 ? grown(ring[HCap], thick) : ring[HCap];
        const float z = zTop + (layer == 0 ? thick : 0.f), rad = za::max(cr.F, cr.B, cr.W);
        const glm::vec3 mid{(cr.F - cr.B) * 0.5f, 0.f, z};
        const auto capST = [&](const glm::vec3& p) {
            return toBlock(layer == 0 ? helmCapBlock : innerBlock, 0.5f + 0.5f * p.x / rad, 0.5f - 0.5f * p.y / rad);
        };
        for(int c = 0; c < numHelmCols; c++)
        {
            const int d = (c + 1) % numHelmCols;
            glm::vec3 p[3] = {mid, place(c, HCap, layer), place(d, HCap, layer)};
            if(layer == 1)
            {
                za::genericSwap(p[1], p[2]);
            }
            const glm::vec2 st[3] = {capST(p[0]), capST(p[1]), capST(p[2])};
            hb.face(p, st, 3, layer);
        }
    }
    // The lining: a face along each open edge from the outer layer to the inner (the outer wall's edge a -> b, wound
    // outward, its rim b, a, then the inner's a, b).
    const auto rim = [&](int ca, int ra, int cb, int rb) {
        const glm::vec3 p[4] = {place(cb, rb, 0), place(ca, ra, 0), place(ca, ra, 1), place(cb, rb, 1)};
        const glm::vec2 st[4] = {toBlock(liningBlock, 0.75f, 0.1f), toBlock(liningBlock, 0.25f, 0.1f), toBlock(liningBlock, 0.25f, 0.9f),
            toBlock(liningBlock, 0.75f, 0.9f)};
        hb.face(p, st, 4, 2);
        s.pieceOpen[Helmet]++;
    };
    for(int c = 0; c < numHelmCols; c++)
    {
        const int d = (c + 1) % numHelmCols, bot = stripBottom(c);
        rim(c, bot, d, bot); // its lower edge (the outer quad's c, bot -> d, bot)
    }
    // The guards' front edges: down column 1 (the left; the face strip ends above it) and column 9 (the right).
    for(int r = HLedge; r < HJaw; r++)
    {
        rim(1, r, 1, r + 1);
        rim(numHelmCols - 1, r + 1, numHelmCols - 1, r);
    }
    const Ring& gk = ring[HCheek];
    // (its outside's size in the head's frame: front to back, side to side, top to bottom)
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const Vert& v : s.verts)
    {
        const glm::vec3 q = v.p - f.origin;
        const glm::vec3 h{glm::dot(q, f.fwd), glm::dot(q, f.left), glm::dot(q, f.up)};
        lo = v.piece == Helmet ? glm::min(lo, h) : lo;
        hi = v.piece == Helmet ? glm::max(hi, h) : hi;
    }
    const glm::vec3 size = hi - lo;
    q_snprintf(s.helmFit, sizeof(s.helmFit),
        "head top %.2f, helmet top %.2f (inner); brow ring front %.2f back %.2f half width %.2f; cheek ring %.2f %.2f %.2f; "
        "face opening +-%.2f; brow %.2f, jaw %.2f, nape %.2f; outside %.2f deep, %.2f wide, %.2f high",
        static_cast<double>(top), static_cast<double>(zTop), static_cast<double>(brow.F), static_cast<double>(brow.B),
        static_cast<double>(brow.W), static_cast<double>(gk.F), static_cast<double>(gk.B), static_cast<double>(gk.W),
        static_cast<double>(gk.Df - gk.F), static_cast<double>(zRow[HBrow]), static_cast<double>(zRow[HJaw]),
        static_cast<double>(nape), static_cast<double>(size.x), static_cast<double>(size.y), static_cast<double>(size.z));
}

[[nodiscard]] uint32_t fnv(uint32_t h, const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    for(size_t i = 0; i < size; i++)
    {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Each vertex's four heaviest bones (renormalised), the rest dropped: as drawn.
void topFour(W& w)
{
    int keep[4] = {-1, -1, -1, -1};
    for(int k = 0; k < 4; k++)
    {
        int best = -1;
        for(int i = 0; i < rig2::maxBones; i++)
        {
            const bool taken = i == keep[0] || i == keep[1] || i == keep[2];
            best = !taken && w.w[i] > 1e-3f && (best < 0 || w.w[i] > w.w[best]) ? i : best;
        }
        keep[k] = best;
    }
    W o;
    float sum = 0.f;
    for(int k : keep)
    {
        sum += k >= 0 ? w.w[k] : 0.f;
    }
    for(int k : keep)
    {
        if(k >= 0)
        {
            o.w[k] = w.w[k] / za::max(sum, 1e-6f);
        }
    }
    w = o;
}

bool buildShell(qmodel_t* src, Shell& s)
{
    const double t0 = Sys_DoubleTime();
    const rig2::Rig* rig = rig2::rigFor(src);
    const auto* sh = src && src->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : nullptr;
    s.verts.clear();
    s.tris.clear();
    s.ok = false;
    s.numSlots = 0;
    if(!rig || !sh || sh->poseverttype != aliashdr_t::PV_QUAKE1 || !buildBody(src, sh, *rig, s.body))
    {
        return false;
    }
    s.generation = rig->generation;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        PieceFrame f = frameOf(*rig, piece);
        f.backCut = backCutOf(src->name, piece);
        const int firstVert = count(s.verts), firstTri = count(s.tris);
        if(f.ok && piece == Helmet)
        {
            helmetOf(s, s.body, f, rig2::boneByName(*rig, "head"));
        }
        else if(f.ok)
        {
            Local L;
            clipPiece(s.body, f, L);
            // (a limb's plate is small: not subdivided, a quarter of the triangles)
            for(int i = 0; i < (limbBone[piece] ? 0 : za::clamp(static_cast<int>(vr_agarmor_subdiv.value), 0, 2)); i++)
            {
                subdivide(L);
            }
            smooth(L, za::clamp(static_cast<int>(vr_agarmor_smooth.value), 0, 50));
            if(!L.tris.empty())
            {
                shellOf(s, L, f, piece);
            }
        }
        s.pieceVerts[piece] = count(s.verts) - firstVert;
        s.pieceTris[piece] = (count(s.tris) - firstTri) / 3;
    }
    // Slots: each piece's bones its own.
    int slotOf[NumPieces][rig2::maxBones];
    for(auto& row : slotOf)
    {
        for(int& x : row)
        {
            x = -1;
        }
    }
    for(Vert& v : s.verts)
    {
        topFour(v.w);
        for(int b = 0; b < rig2::maxBones; b++)
        {
            if(v.w.w[b] > 0.f && slotOf[v.piece][b] < 0 && s.numSlots < maxSlots)
            {
                slotOf[v.piece][b] = s.numSlots;
                s.slotBone[s.numSlots] = b;
                s.slotPiece[s.numSlots] = v.piece;
                s.numSlots++;
            }
        }
    }
    uint32_t h = 2166136261u;
    for(const Vert& v : s.verts)
    {
        h = fnv(h, &v.p, sizeof(v.p));
        h = fnv(h, &v.n, sizeof(v.n));
        h = fnv(h, &v.st, sizeof(v.st));
        h = fnv(h, v.w.w, sizeof(v.w.w));
    }
    h = fnv(h, s.tris.data(), sizeof(int) * s.tris.size());
    s.hash = h;
    s.ok = !s.tris.empty();
    s.made = static_cast<float>((Sys_DoubleTime() - t0) * 1000.0);
    return s.ok;
}

// ---- Who wears what ----
struct Named
{
    int num{0}, type{0}, pieces{0};
};
za::Vector<Named> named; // vr_agarmor_set

za::Vector<Shell> shells; // made this session (per model)

entity_t worn[maxWorn];
int wornSource[maxWorn]{}, wornPieces[maxWorn]{}, wornSlots[maxWorn]{};
int wornShell[maxWorn]{}; // (shells' index: a shell made meanwhile moves them)
int numWorn = 0;
float wornBones[maxWorn][maxSlots * 12]{};

struct Kind
{
    const qmodel_t* skinned{nullptr};
    char name[MAX_QPATH]{};
    qmodel_t* armor{nullptr};
};
za::Vector<Kind> kinds;

[[nodiscard]] bool isWearer(const char* base)
{
    for(const Wearer& w : wearers)
    {
        if(!strcmp(base, w.model))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] Shell* shellFor(qmodel_t* src)
{
    const rig2::Rig* rig = src ? rig2::rigFor(src) : nullptr;
    if(!rig)
    {
        return nullptr;
    }
    for(Shell& s : shells)
    {
        if(!strcmp(s.model.cStr(), src->name))
        {
            if(s.generation != rig->generation)
            {
                (void)buildShell(src, s);
            }
            return s.ok ? &s : nullptr;
        }
    }
    shells.pushBack(Shell{});
    Shell& s = shells.back();
    s.model = za::String{src->name};
    (void)buildShell(src, s);
    return s.ok ? &s : nullptr;
}

[[nodiscard]] int shellNamed(const char* model)
{
    for(int i = 0; i < count(shells); i++)
    {
        if(at(shells, i).ok && !strcmp(at(shells, i).model.cStr(), model))
        {
            return i;
        }
    }
    return -1;
}

// The armour model drawn over a body drawn as `skinned` ("<model>#rig2" of a wearer), else nullptr.
[[nodiscard]] qmodel_t* armorFor(const qmodel_t* skinned)
{
    for(const Kind& k : kinds)
    {
        if(k.skinned == skinned && !strcmp(k.name, skinned->name))
        {
            return k.armor;
        }
    }
    Kind k;
    k.skinned = skinned;
    q_strlcpy(k.name, skinned->name, sizeof(k.name));
    const size_t len = strlen(skinned->name), suffix = strlen(rigSuffix);
    if(len > suffix && !strcmp(skinned->name + len - suffix, rigSuffix))
    {
        char base[MAX_QPATH];
        q_strlcpy(base, skinned->name, za::min(sizeof(base), len - suffix + 1));
        if(isWearer(base))
        {
            char name[MAX_QPATH];
            q_snprintf(name, sizeof(name), "%s%s", base, armorSuffix);
            k.armor = Mod_ForName(name, false);
            k.armor = k.armor && k.armor->type == mod_alias ? k.armor : nullptr;
        }
    }
    for(Kind& old : kinds)
    {
        if(old.skinned == skinned)
        {
            old = k;
            return k.armor;
        }
    }
    kinds.pushBack(k);
    return k.armor;
}

// An entity's armour in a word (the server's, the client's): its type (bits 30-31), its worn pieces' wear (bits 20-29
// at a third of their pool or under, 10-19 at two thirds or under; P2's damage marks), its pieces (0-9).
[[nodiscard]] uint32_t packState(int type, int pieces, int wear)
{
    pieces &= allPieces;
    wear &= pieces | pieces << NumPieces;
    return type > 0 && pieces ? static_cast<uint32_t>(type) << 30u | static_cast<uint32_t>(wear) << 10u | static_cast<uint32_t>(pieces) : 0u;
}
[[nodiscard]] int stateType(uint32_t s)
{
    return static_cast<int>(s >> 30u);
}
[[nodiscard]] int statePieces(uint32_t s)
{
    return static_cast<int>(s & static_cast<uint32_t>(allPieces));
}
[[nodiscard]] int stateWear(uint32_t s)
{
    return static_cast<int>((s >> 10u) & 0xfffffu);
}

// The server's word on each entity (QVR_SVC_ARMOR: packState); and how many are armoured.
za::Vector<uint32_t> served;
int numServed = 0;

// Who wears what: vr_agarmor_set's entities, else vr_agarmor (debug: everyone, forced), else the server's word; `wear`
// its pieces' wear (packState's bits 10-29, shifted down: the server's word only, or vr_agarmor_wear's debug look).
void wornBy(int num, int& type, int& pieces, int& wear)
{
    type = za::clamp(static_cast<int>(vr_agarmor.value), 0, 4);
    type = type == 4 ? 1 + num % 3 : type;
    pieces = static_cast<int>(vr_agarmor_pieces.value) & allPieces;
    const int forced = za::clamp(static_cast<int>(vr_agarmor_wear.value), 0, 2);
    wear = forced == 0 ? 0 : forced == 1 ? allPieces : allPieces << NumPieces | allPieces;
    if(type == 0 && num >= 0 && num < count(served))
    {
        type = stateType(at(served, num));
        pieces = statePieces(at(served, num));
        wear = forced ? wear : stateWear(at(served, num));
    }
    for(const Named& n : named)
    {
        if(n.num == num)
        {
            type = n.type;
            pieces = n.pieces;
        }
    }
}

void forgetModels()
{
    // (the models made from the shells are made again when next drawn)
    for(Kind& k : kinds)
    {
        if(k.armor && k.armor->type == mod_alias && Cache_Check(&k.armor->cache))
        {
            Cache_Free(&k.armor->cache, false);
        }
    }
    kinds.clear();
}

// ---- Commands ----
void info_f()
{
    for(const Wearer& wearer : wearers)
    {
        const char* w = wearer.model;
        qmodel_t* src = Mod_ForName(w, false);
        Shell* s = src ? shellFor(src) : nullptr;
        if(!s)
        {
            Con_Printf("agarmor: %s: no shell (no authored rig?)\n", w);
            continue;
        }
        Con_Printf("agarmor: %s: %d vertices, %d triangles, %d bone slots, hash %08x, made in %.1f ms (body %d vertices, %d triangles)\n",
            w, count(s->verts), count(s->tris) / 3, s->numSlots, s->hash, static_cast<double>(s->made), count(s->body.p),
            count(s->body.tris) / 3);
        for(int p = 0; p < NumPieces; p++)
        {
            Con_Printf("agarmor:   %s: %d vertices, %d triangles, %d rim edges\n", pieceNames[p], s->pieceVerts[p], s->pieceTris[p],
                s->pieceOpen[p]);
        }
        Con_Printf("agarmor:   helmet fit: %s\n", s->helmFit);
    }
    Con_Printf("agarmor: %d drawn last frame\n", numWorn);
}

// vr_agarmor_headcam [view] [distance]: (the mock headset; the helmet's eyeshots) the eyes on two heads: the left on the
// nearest grunt's, the right on the nearest enforcer's (vrgrunt / vrenforcer, armoured or not), `distance` units off
// (default 22) at the helmet's middle (its rest pose where the body stands): view 1 front, 2 side (its right), 3
// three-quarter (front right), 4 behind; 0 (or none) the eyes from the head again.
void headcam_f()
{
    const int view = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0;
    const float dist = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 22.f;
    if(view <= 0 || view > 4)
    {
        setMockEyeCameras(false);
        Con_Printf("agarmor headcam: off\n");
        return;
    }
    const char* wanted[2] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl"};
    const glm::vec3 me = cl.viewentity > 0 && cl.viewentity < cl.num_entities ?
        glm::vec3{cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1], cl_entities[cl.viewentity].origin[2]} :
        glm::vec3{0.f};
    glm::vec3 camAt[2]{};
    float pitch[2] = {0.f, 0.f}, yaw[2] = {0.f, 0.f};
    for(int k = 0; k < 2; k++)
    {
        int found = -1;
        float best = 1e30f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& e = cl_entities[i];
            const float d = glm::distance(glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, me);
            found = e.model && !strcmp(e.model->name, wanted[k]) && d < best ? i : found;
            best = e.model && !strcmp(e.model->name, wanted[k]) && d < best ? d : best;
        }
        const Shell* s = found > 0 ? shellFor(cl_entities[found].model) : nullptr;
        glm::vec3 mid{0.f};
        int n = 0;
        for(int v = 0; s && v < count(s->verts); v++)
        {
            mid += at(s->verts, v).piece == Helmet ? at(s->verts, v).p : glm::vec3{0.f};
            n += at(s->verts, v).piece == Helmet ? 1 : 0;
        }
        if(n == 0)
        {
            Con_Printf("agarmor headcam: no %s with a helmet in view\n", wanted[k]);
            continue;
        }
        const entity_t& e = cl_entities[found];
        mid /= static_cast<float>(n);
        const float y = e.angles[1] * (pi / 180.f);
        const glm::vec3 fw{za::cos(y), za::sin(y), 0.f}, left{-za::sin(y), za::cos(y), 0.f};
        const glm::vec3 head = glm::vec3{e.origin[0], e.origin[1], e.origin[2]} + fw * mid.x + left * mid.y + glm::vec3{0.f, 0.f, mid.z};
        const glm::vec3 dirs[4] = {fw, -left, glm::normalize(fw - left), -fw};
        const float yaws[4] = {180.f, 90.f, 135.f, 0.f};
        camAt[k] = head + dirs[view - 1] * dist;
        yaw[k] = e.angles[1] + yaws[view - 1];
        Con_Printf("agarmor headcam: %s (entity %d) head at %.1f %.1f %.1f, view %d\n", wanted[k], found, static_cast<double>(head.x),
            static_cast<double>(head.y), static_cast<double>(head.z), view);
    }
    setMockEyeCameras(true, camAt, pitch, yaw);
}

void rebuild_f()
{
    shells.clear();
    forgetModels();
    Con_Printf("agarmor: shells made again when next drawn\n");
}

void set_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_agarmor_set <entity> <type 0 none, 1 green, 2 yellow, 3 red | -1 vr_agarmor's> [pieces: 1 cuirass, 2 helmet, 4/8 "
                   "pauldrons, 16/32 bracers, 64/128 thighs, 256/512 shins; 1023 all]\n");
        return;
    }
    const int num = Q_atoi(Cmd_Argv(1)), type = Q_atoi(Cmd_Argv(2));
    const int pieces = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : allPieces;
    // A live game's entity: its armour in QC (pools and all: vr_agarmor.qc VR_AgArmor_SetCmd), sent back as the server's.
    if(sv.active && num > svs.maxclients && num < sv.qcvm.num_edicts)
    {
        qcvm_t* old = nullptr;
        PR_PushQCVM(&sv.qcvm, &old);
        const func_t fn = progs::findFunction("VR_AgArmor_SetCmd");
        bool done = false;
        if(fn && !EDICT_NUM(num)->free)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
            G_INT(OFS_PARM0) = EDICT_TO_PROG(EDICT_NUM(num));
            G_FLOAT(OFS_PARM1) = static_cast<float>(type);
            G_FLOAT(OFS_PARM2) = static_cast<float>(pieces);
            PR_ExecuteProgram(fn);
            done = G_FLOAT(OFS_RETURN) != 0.f;
        }
        PR_PopQCVM(old);
        if(done)
        {
            setWorn(num, -1, 0); // (the server's word, not a drawn-only override)
            return;
        }
    }
    setWorn(num, type, pieces);
}

// ---- The check ----
struct Hit
{
    int count{0};
    float deepest{0.f};
};
enum Region : int
{
    RNeck,
    RShoulders,
    RWaist,
    RTorso,
    NumRegions
};
constexpr const char* regionNames[NumRegions] = {"neck", "shoulders", "waist", "torso"};

[[nodiscard]] int regionOf(const rig2::Rig& rig, int bone)
{
    const char* n = bone >= 0 && bone < rig.numBones ? rig.bones[bone].name : "";
    if(!strcmp(n, "neck") || !strcmp(n, "head"))
    {
        return RNeck;
    }
    if(!strncmp(n, "clavicle", 8) || !strncmp(n, "upperarm", 8) || !strncmp(n, "forearm", 7) || !strncmp(n, "hand", 4) ||
       !strcmp(n, "weapon"))
    {
        return RShoulders;
    }
    if(!strcmp(n, "pelvis") || !strncmp(n, "thigh", 5) || !strncmp(n, "shin", 4) || !strncmp(n, "foot", 4))
    {
        return RWaist;
    }
    return RTorso;
}

struct Tally
{
    Hit bodyIn[NumPieces][NumRegions];  // body vertices inside a plate
    Hit plateIn[NumPieces][NumRegions]; // plate (outer) vertices inside the body
    int poses{0};
    int posesBad{0};
    char worst[64]{};
    int worstCount{0};
    void add(const Tally& o)
    {
        for(int p = 0; p < NumPieces; p++)
        {
            for(int r = 0; r < NumRegions; r++)
            {
                bodyIn[p][r].count += o.bodyIn[p][r].count;
                bodyIn[p][r].deepest = za::max(bodyIn[p][r].deepest, o.bodyIn[p][r].deepest);
                plateIn[p][r].count += o.plateIn[p][r].count;
                plateIn[p][r].deepest = za::max(plateIn[p][r].deepest, o.plateIn[p][r].deepest);
            }
        }
        poses += o.poses;
        posesBad += o.posesBad;
    }
    [[nodiscard]] int total() const
    {
        int n = 0;
        for(int p = 0; p < NumPieces; p++)
        {
            for(int r = 0; r < NumRegions; r++)
            {
                n += bodyIn[p][r].count + plateIn[p][r].count;
            }
        }
        return n;
    }
};

// The solid angle of triangle abc seen from the origin (Van Oosterom and Strackee), over 4 pi: a closed surface's
// sum is its winding number round the point.
[[nodiscard]] float solidAngle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const float la = glm::length(a), lb = glm::length(b), lc = glm::length(c);
    const float num = glm::dot(a, glm::cross(b, c));
    const float den = la * lb * lc + glm::dot(a, b) * lc + glm::dot(b, c) * la + glm::dot(c, a) * lb;
    return 2.f * za::atan2(num, den) / (4.f * pi);
}

[[nodiscard]] float closestDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    // (Ericson's closest point on a triangle)
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return glm::length(p - a);
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return glm::length(p - b);
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return glm::length(p - (a + ab * (d1 / (d1 - d3))));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return glm::length(p - c);
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return glm::length(p - (a + ac * (d2 / (d2 - d6))));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return glm::length(p - (b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)))));
    }
    const float den = 1.f / (va + vb + vc);
    return glm::length(p - (a + ab * (vb * den) + ac * (vc * den)));
}

struct CheckData
{
    za::Vector<glm::vec3> bodyAll, body, shell;
    za::Vector<int> bodyRegion, shellRegion;
    za::Vector<uint8_t> nearPiece[NumPieces]; // body vertices near a piece at rest (the ones measured against it)
    // At rest already (the body's own overlaps: a plate under a shoulder pad or an arm hanging against the torso; a
    // body vertex bulging past a plate's edge): the poses count what their motion adds.
    za::Vector<uint8_t> restBodyIn[NumPieces], restPlateIn;
    bool baseline{false};
};
CheckData cd;

void skinShell(const Shell& s, const rigpose::Rigid* F, za::Vector<glm::vec3>& out)
{
    out.resize(s.verts.size(), glm::vec3{0.f});
    for(int v = 0; v < count(s.verts); v++)
    {
        const Vert& x = at(s.verts, v);
        glm::vec3 p{0.f};
        for(int b = 0; b < rig2::maxBones; b++)
        {
            p += x.w.w[b] > 0.f ? rigpose::apply(F[b], x.p) * x.w.w[b] : glm::vec3{0.f};
        }
        at(out, v) = p;
    }
}

void measure(const Shell& s, const rig2::Rig& rig, const rig2::Pose& pose, Tally& t)
{
    rigpose::Rigid F[rig2::maxBones];
    rig2::forward(rig, pose, F);
    const Body& b = s.body;
    cd.bodyAll.resize(b.mdlRest.size(), glm::vec3{0.f});
    rig2::skin(rig, F, b.mdlRest.data(), count(b.mdlRest), cd.bodyAll.data(), static_cast<bool>(vr_rig2_dq.value));
    cd.body.resize(b.p.size(), glm::vec3{0.f});
    for(int v = 0; v < count(b.p); v++)
    {
        at(cd.body, v) = at(cd.bodyAll, at(b.rep, v));
    }
    skinShell(s, F, cd.shell);
    Tally one;
    one.poses = 1;
    // Body vertices inside a plate: the winding number of the piece's closed shell round them.
    int firstTri = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const int nt = s.pieceTris[piece];
        for(int v = 0; v < count(cd.body); v++)
        {
            if(!at(cd.nearPiece[piece], v))
            {
                continue;
            }
            const glm::vec3 p = at(cd.body, v);
            float wn = 0.f;
            for(int k = firstTri; k < firstTri + nt; k++)
            {
                wn += solidAngle(at(cd.shell, at(s.tris, 3 * k)) - p, at(cd.shell, at(s.tris, 3 * k + 1)) - p,
                    at(cd.shell, at(s.tris, 3 * k + 2)) - p);
            }
            if(cd.baseline)
            {
                at(cd.restBodyIn[piece], v) = za::abs(wn) > 0.5f ? 1 : 0;
            }
            else if(at(cd.restBodyIn[piece], v))
            {
                continue;
            }
            if(za::abs(wn) > 0.5f)
            {
                float d = 1e9f;
                for(int k = firstTri; k < firstTri + nt; k++)
                {
                    d = za::min(d, closestDistance(p, at(cd.shell, at(s.tris, 3 * k)), at(cd.shell, at(s.tris, 3 * k + 1)),
                                       at(cd.shell, at(s.tris, 3 * k + 2))));
                }
                Hit& h = one.bodyIn[piece][at(cd.bodyRegion, v)];
                h.count++;
                h.deepest = za::max(h.deepest, d);
            }
        }
        firstTri += nt;
    }
    // The plates' outer vertices inside the body (its winding number: the .mdl is closed enough).
    for(int v = 0; v < count(cd.shell); v++)
    {
        const Vert& x = at(s.verts, v);
        if(x.layer != 0)
        {
            continue;
        }
        const glm::vec3 p = at(cd.shell, v);
        float wn = 0.f;
        for(int k = 0; k + 2 < count(b.tris); k += 3)
        {
            wn += solidAngle(at(cd.body, at(b.tris, k)) - p, at(cd.body, at(b.tris, k + 1)) - p, at(cd.body, at(b.tris, k + 2)) - p);
        }
        if(cd.baseline)
        {
            at(cd.restPlateIn, v) = wn > 0.5f ? 1 : 0;
        }
        else if(at(cd.restPlateIn, v))
        {
            continue;
        }
        if(wn > 0.5f)
        {
            float d = 1e9f;
            for(int k = 0; k + 2 < count(b.tris); k += 3)
            {
                d = za::min(d, closestDistance(p, at(cd.body, at(b.tris, k)), at(cd.body, at(b.tris, k + 1)), at(cd.body, at(b.tris, k + 2))));
            }
            Hit& h = one.plateIn[x.piece][at(cd.shellRegion, v)];
            h.count++;
            h.deepest = za::max(h.deepest, d);
        }
    }
    one.posesBad = one.total() > 0 ? 1 : 0;
    t.add(one);
    if(one.total() > t.worstCount)
    {
        t.worstCount = one.total();
    }
}

void printTally(const char* what, const Tally& t)
{
    Con_Printf("agarmor check: %s: %d poses, %d with faults\n", what, t.poses, t.posesBad);
    for(int p = 0; p < NumPieces; p++)
    {
        char line[512];
        int at0 = q_snprintf(line, sizeof(line), "agarmor check:   %s: body in plate", pieceNames[p]);
        for(int r = 0; r < NumRegions; r++)
        {
            at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), " %s %d (%.2f)", regionNames[r], t.bodyIn[p][r].count,
                static_cast<double>(t.bodyIn[p][r].deepest));
        }
        at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), "; plate in body");
        for(int r = 0; r < NumRegions; r++)
        {
            at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), " %s %d (%.2f)", regionNames[r], t.plateIn[p][r].count,
                static_cast<double>(t.plateIn[p][r].deepest));
        }
        Con_Printf("%s\n", line);
    }
    if(t.worst[0])
    {
        Con_Printf("agarmor check:   worst: %s (%d)\n", t.worst, t.worstCount);
    }
}

void checkModel(const char* modelName, int randoms, int& passed, int& failed)
{
    Con_Printf("agarmor check: %s\n", modelName);
    const auto verdict = [&](bool ok, const char* what) {
        Con_Printf("agarmor check: %s: %s (%s)\n", ok ? "PASS" : "FAIL", what, modelName);
        (ok ? passed : failed)++;
    };
    qmodel_t* src = Mod_ForName(modelName, false);
    Shell* sp = src ? shellFor(src) : nullptr;
    const rig2::Rig* rig = src ? rig2::rigFor(src) : nullptr;
    verdict(sp && rig, "the shells made");
    if(!sp || !rig)
    {
        return;
    }
    // Made again: alike.
    Shell again;
    again.model = sp->model;
    (void)buildShell(src, again);
    Con_Printf("agarmor check: hash %08x, again %08x; %d vertices, %d triangles (cuirass %d, helmet %d), %d slots\n", sp->hash, again.hash,
        count(sp->verts), count(sp->tris) / 3, sp->pieceTris[Cuirass], sp->pieceTris[Helmet], sp->numSlots);
    Con_Printf("agarmor check: helmet fit: %s\n", sp->helmFit);
    verdict(again.hash == sp->hash && again.hash != 0u, "made alike twice");
    const Shell& s = *sp;
    // Closed: every edge of each piece's shell used twice.
    {
        EdgeMap uses;
        uses.init(count(s.tris));
        // (by place: the seam's and the rim's copies welded)
        za::Vector<int> weld(s.verts.size(), 0);
        for(int v = 0; v < count(s.verts); v++)
        {
            at(weld, v) = v;
            for(int u = 0; u < v; u++)
            {
                if(at(weld, u) == u && at(s.verts, u).piece == at(s.verts, v).piece && glm::length(at(s.verts, u).p - at(s.verts, v).p) < 1e-4f)
                {
                    at(weld, v) = u;
                    break;
                }
            }
        }
        for(int k = 0; k + 2 < count(s.tris); k += 3)
        {
            for(int e = 0; e < 3; e++)
            {
                bool fresh = false;
                int& n = uses.get(at(weld, at(s.tris, k + e)), at(weld, at(s.tris, k + (e + 1) % 3)), fresh);
                n = fresh ? 1 : n + 1;
            }
        }
        int open = 0;
        for(za::SizeT i = 0; i < uses.keys.size(); i++)
        {
            open += uses.keys[i] != ~0ull && uses.vals[i] != 2 ? 1 : 0;
        }
        Con_Printf("agarmor check: edges not shared by exactly two triangles: %d\n", open);
        verdict(open == 0, "each shell closed");
        verdict(count(s.tris) / 3 <= 4000, "at most 4000 triangles a body (the full set)");
    }
    // What is measured against what: body vertices near each piece at rest; regions by heaviest bone.
    const Body& b = s.body;
    cd.bodyRegion.resize(b.p.size(), 0);
    for(int v = 0; v < count(b.p); v++)
    {
        at(cd.bodyRegion, v) = regionOf(*rig, heaviest(at(b.w, v)));
    }
    cd.shellRegion.resize(s.verts.size(), 0);
    for(int v = 0; v < count(s.verts); v++)
    {
        at(cd.shellRegion, v) = regionOf(*rig, heaviest(at(s.verts, v).w));
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for(const Vert& x : s.verts)
        {
            lo = x.piece == piece ? glm::min(lo, x.p) : lo;
            hi = x.piece == piece ? glm::max(hi, x.p) : hi;
        }
        cd.nearPiece[piece].clear();
        cd.nearPiece[piece].resize(b.p.size(), uint8_t{0});
        const glm::vec3 c = (lo + hi) * 0.5f;
        const float r = glm::length(hi - lo) * 0.5f + 6.f;
        for(int v = 0; v < count(b.p); v++)
        {
            at(cd.nearPiece[piece], v) = glm::length(at(b.p, v) - c) < r ? 1 : 0;
        }
    }
    const double t0 = Sys_DoubleTime();
    Tally rest;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        cd.restBodyIn[piece].clear();
        cd.restBodyIn[piece].resize(b.p.size(), uint8_t{0});
    }
    cd.restPlateIn.clear();
    cd.restPlateIn.resize(s.verts.size(), uint8_t{0});
    cd.baseline = true;
    measure(s, *rig, rig2::restPose(*rig), rest);
    cd.baseline = false;
    printTally("rest pose (the body's own overlaps; the poses below count only what their motion adds)", rest);
    int outer = 0;
    for(const Vert& x : s.verts)
    {
        outer += x.layer == 0 ? 1 : 0;
    }
    float restDeepest = 0.f;
    int restPlate = 0;
    for(int p = 0; p < NumPieces; p++)
    {
        for(int r = 0; r < NumRegions; r++)
        {
            restDeepest = za::max(restDeepest, rest.bodyIn[p][r].deepest);
            restPlate += rest.plateIn[p][r].count;
        }
    }
    Con_Printf("agarmor check: at rest %d of %d plate vertices inside the body (hidden by it), body in plate at most %.2f deep\n", restPlate,
        outer, static_cast<double>(restDeepest));
    verdict(restDeepest < za::max(vr_agarmor_thick.value, 0.05f) && restPlate * 5 < outer,
        "the rest pose: no body vertex through a plate's thickness, under a fifth of the plates hidden in the body (under the pads and arms)");
    // Every joint near the pieces through its limits: a ball's swing ends and diagonals, its twist's ends; a hinge's ends.
    const char* swept[] = {"spine", "chest", "neck", "head", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r"};
    Tally sweeps;
    for(const char* j : swept)
    {
        const int bone = rig2::boneByName(*rig, j);
        if(bone < 0)
        {
            continue;
        }
        const rig2::Bone& bb = rig->bones[bone];
        Tally one;
        const rig2::Pose restPose = rig2::restPose(*rig);
        const glm::vec3 r0 = restPose.a[bone];
        glm::vec3 tries[12];
        int n = 0;
        if(bb.joint == rig2::Joint::Ball)
        {
            tries[n++] = glm::vec3{bb.swingX.x, r0.y, r0.z};
            tries[n++] = glm::vec3{bb.swingX.y, r0.y, r0.z};
            tries[n++] = glm::vec3{r0.x, bb.swingY.x, r0.z};
            tries[n++] = glm::vec3{r0.x, bb.swingY.y, r0.z};
            tries[n++] = glm::vec3{r0.x, r0.y, bb.twist.x};
            tries[n++] = glm::vec3{r0.x, r0.y, bb.twist.y};
            for(int d = 0; d < 4; d++)
            {
                tries[n++] = glm::vec3{(d & 1) ? bb.swingX.y : bb.swingX.x, (d & 2) ? bb.swingY.y : bb.swingY.x, r0.z};
            }
        }
        else if(bb.joint == rig2::Joint::Hinge)
        {
            tries[n++] = glm::vec3{bb.range.x, 0.f, 0.f};
            tries[n++] = glm::vec3{bb.range.y, 0.f, 0.f};
        }
        for(int k = 0; k < n; k++)
        {
            rig2::Pose p = restPose;
            p.a[bone] = rig2::clampAngles(*rig, bone, tries[k]);
            Tally pt;
            measure(s, *rig, p, pt);
            if(pt.total() > one.worstCount)
            {
                one.worstCount = pt.total();
                q_snprintf(one.worst, sizeof(one.worst), "%s at (%.0f %.0f %.0f)", j, static_cast<double>(p.a[bone].x),
                    static_cast<double>(p.a[bone].y), static_cast<double>(p.a[bone].z));
            }
            one.add(pt);
        }
        char what[64];
        q_snprintf(what, sizeof(what), "sweep %s", j);
        printTally(what, one);
        const int worstCount = za::max(sweeps.worstCount, one.worstCount);
        if(one.worstCount > sweeps.worstCount)
        {
            q_strlcpy(sweeps.worst, one.worst, sizeof(sweeps.worst));
        }
        sweeps.add(one);
        sweeps.worstCount = worstCount;
    }
    printTally("all sweeps", sweeps);
    Tally random;
    uint32_t seed = 12345u;
    for(int i = 0; i < randoms; i++)
    {
        const rig2::Pose p = rig2::randomPose(*rig, seed);
        Tally pt;
        measure(s, *rig, p, pt);
        if(pt.total() > random.worstCount)
        {
            random.worstCount = pt.total();
            q_snprintf(random.worst, sizeof(random.worst), "random pose %d", i);
        }
        const int worstCount = random.worstCount;
        random.add(pt);
        random.worstCount = worstCount;
    }
    printTally("random poses", random);
    Con_Printf("agarmor check: measured in %.0f ms\n", (Sys_DoubleTime() - t0) * 1000.0);
}

// vr_agarmor_check [model | all] [random poses]: one model's shells, or (no model, "all") every authored wearer's.
void check_f()
{
    const bool all = Cmd_Argc() < 2 || !q_strcasecmp(Cmd_Argv(1), "all");
    const int randoms = Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 0, 5000) : 200;
    int passed = 0, failed = 0;
    if(all)
    {
        for(const char* m : checked)
        {
            checkModel(m, randoms, passed, failed);
        }
    }
    else
    {
        checkModel(Cmd_Argv(1), randoms, passed, failed);
    }
    qvr::testkit::verdict("agarmor check", passed, failed);
}

} // namespace

void setWorn(int num, int type, int pieces)
{
    for(za::SizeT i = 0; i < named.size(); i++)
    {
        if(named[i].num == num)
        {
            named.erase(&named[i]);
            break;
        }
    }
    if(type >= 0 && named.size() < static_cast<za::SizeT>(maxNamed))
    {
        named.pushBack(Named{num, za::clamp(type, 0, 3), pieces & allPieces});
    }
}

bool makeModel(qmodel_t* mod)
{
    const size_t len = strlen(mod->name), suffix = strlen(armorSuffix);
    if(len <= suffix || strcmp(mod->name + len - suffix, armorSuffix))
    {
        return false;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
    qmodel_t* src = isWearer(base) ? Mod_ForName(base, false) : nullptr;
    qmodel_t* skins = src ? Mod_ForName(skinModelName, false) : nullptr;
    const auto* kh = skins && skins->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(skins)) : nullptr;
    const Shell* s = kh ? shellFor(src) : nullptr;
    if(!s || s->numSlots <= 0)
    {
        Con_DPrintf("agarmor: %s not made (%s)\n", mod->name, !kh ? "no vragarmor.mdl" : "no shell");
        return false;
    }
    const int numVerts = count(s->verts), numIndexes = count(s->tris), numBones = s->numSlots;

    const int start = Hunk_LowMark();
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(sizeof(aliashdr_t))));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));

    hdr->ident = kh->ident;
    hdr->version = kh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = 64.f;
    hdr->numskins = kh->numskins;
    hdr->skinwidth = kh->skinwidth;
    hdr->skinheight = kh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = numIndexes / 3;
    hdr->numframes = 1;
    hdr->synctype = ST_SYNC;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    hdr->frames[0].firstpose = 0;
    hdr->frames[0].numposes = 1;
    hdr->frames[0].interval = 0.1f;
    q_strlcpy(hdr->frames[0].name, "armor", sizeof(hdr->frames[0].name));
    memcpy(hdr->gltextures, kh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, kh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, kh->texels, sizeof(hdr->texels));
    const rig2::Rig* rig = rig2::rigFor(src);
    for(int b = 0; b < numBones; b++)
    {
        q_snprintf(bones[b].name, sizeof(bones[b].name), "%s:%s", pieceNames[s->slotPiece[b]],
            rig && s->slotBone[b] < rig->numBones ? rig->bones[s->slotBone[b]].name : "?");
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    const float padW = static_cast<float>(TexMgr_PadConditional(kh->skinwidth)), padH = static_cast<float>(TexMgr_PadConditional(kh->skinheight));
    for(int v = 0; v < numVerts; v++)
    {
        const Vert& x = at(s->verts, v);
        iqmvert_t& o = verts[v];
        for(int i = 0; i < 3; i++)
        {
            o.xyz[i] = x.p[i];
            o.norm[i] = static_cast<int8_t>(za::clamp(static_cast<int>(x.n[i] * 127.f + (x.n[i] < 0.f ? -0.5f : 0.5f)), -127, 127));
        }
        o.norm[3] = 0;
        o.st[0] = x.st.x / padW;
        o.st[1] = x.st.y / padH;
        int left = 255, k = 0;
        int order[4] = {-1, -1, -1, -1};
        for(int b = 0; b < rig2::maxBones && k < 4; b++)
        {
            order[k] = x.w.w[b] > 0.f ? b : -1;
            k += x.w.w[b] > 0.f ? 1 : 0;
        }
        for(int i = 0; i < 4; i++)
        {
            o.weight[i] = 0;
            o.idx[i] = 0;
        }
        for(int i = 0; i < k; i++)
        {
            const int bone = order[i];
            int slot = 0;
            for(int q = 0; q < s->numSlots; q++)
            {
                slot = s->slotPiece[q] == x.piece && s->slotBone[q] == bone ? q : slot;
            }
            const int wb = i + 1 < k ? static_cast<int>(x.w.w[bone] * 255.f + 0.5f) : left;
            o.weight[i] = static_cast<uint8_t>(za::clamp(za::min(wb, left), 0, 255));
            o.idx[i] = static_cast<uint8_t>(slot);
            left -= o.weight[i];
        }
    }
    for(int i = 0; i + 2 < numIndexes; i += 3)
    {
        indexes[i] = static_cast<unsigned short>(at(s->tris, i));
        indexes[i + 1] = static_cast<unsigned short>(at(s->tris, s->body.nativeFlip ? i + 2 : i + 1));
        indexes[i + 2] = static_cast<unsigned short>(at(s->tris, s->body.nativeFlip ? i + 1 : i + 2));
    }
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);

    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod);
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = 1;
    mod->synctype = ST_SYNC;
    mod->flags = 0;
    mod->path_id = src->path_id;
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -64.f;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = 64.f;
    }
    const int total = Hunk_LowMark() - start;
    Cache_Alloc(&mod->cache, total, mod->name);
    if(!mod->cache.data)
    {
        Hunk_FreeToLowMark(start);
        return false;
    }
    memcpy(mod->cache.data, hdr, static_cast<size_t>(total));
    Hunk_FreeToLowMark(start);
    Con_DPrintf("agarmor: %s made (%d vertices, %d triangles, %d bones)\n", mod->name, numVerts, numIndexes / 3, numBones);
    return true;
}

void relink()
{
    numWorn = 0;
    if(cls.state != ca_connected || !cl_entities || (vr_agarmor.value == 0.f && named.empty() && numServed == 0))
    {
        return;
    }
    const int visible = cl_numvisedicts;
    for(int i = 0; i < visible && numWorn < maxWorn && cl_numvisedicts < MAX_VISEDICTS; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e < cl_entities || e >= cl_entities + cl_max_edicts || !strchr(e->model->name, '#'))
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        int type = 0, pieces = 0, wear = 0;
        wornBy(num, type, pieces, wear);
        qmodel_t* armor = type > 0 && pieces ? armorFor(e->model) : nullptr;
        if(!armor)
        {
            continue;
        }
        char base[MAX_QPATH];
        const size_t len = strlen(e->model->name), suffix = strlen(rigSuffix);
        q_strlcpy(base, e->model->name, za::min(sizeof(base), len - suffix + 1));
        const int shell = shellNamed(base);
        if(shell < 0)
        {
            continue;
        }
        // P2's damage marks: the pieces at each wear (whole, two thirds, a third) drawn with that wear's skin (type - 1 +
        // 3 x wear: make_agarmor.py's nine), an entity each (the pieces of the others collapsed). An old vragarmor.mdl
        // of three skins: all whole.
        const int numSkins = armor->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(armor))->numskins : 0;
        const int w66 = wear & allPieces, w33 = (wear >> NumPieces) & allPieces;
        const int byWear[3] = {numSkins >= 9 ? pieces & ~w66 & ~w33 : pieces, numSkins >= 9 ? pieces & w66 & ~w33 : 0,
            numSkins >= 9 ? pieces & w33 : 0};
        for(int level = 0; level < 3 && numWorn < maxWorn && cl_numvisedicts < MAX_VISEDICTS; level++)
        {
            if(!byWear[level])
            {
                continue;
            }
            const int k = numWorn++;
            entity_t& a = worn[k];
            a = *e;
            a.model = armor;
            a.skinnum = type - 1 + 3 * level;
            a.frame = 0;
            a.previouspose = a.currentpose = 0;
            a.lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
            wornSource[k] = num;
            wornPieces[k] = byWear[level];
            wornShell[k] = shell;
            wornSlots[k] = at(shells, shell).numSlots;
            cl_visedicts[cl_numvisedicts++] = &a;
        }
    }
}

int bonePoses(const entity_t* e, const float** matrices)
{
    if(numWorn <= 0 || e < &worn[0] || e >= &worn[numWorn])
    {
        return 0;
    }
    const int k = static_cast<int>(e - &worn[0]);
    const int n = wornSlots[k];
    if(!matrices)
    {
        return n;
    }
    const float* src = nullptr;
    const int have = VR_AliasBonePoses(&cl_entities[wornSource[k]], &src);
    // (the shells made again since: collapsed, drawn from the next frame)
    const Shell* s = wornShell[k] < count(shells) && at(shells, wornShell[k]).numSlots == n ? &at(shells, wornShell[k]) : nullptr;
    float* out = wornBones[k];
    for(int slot = 0; slot < n; slot++)
    {
        const int b = s ? s->slotBone[slot] : 0;
        const bool shown = s && src && b < have && ((wornPieces[k] >> s->slotPiece[slot]) & 1);
        for(int i = 0; i < 12; i++)
        {
            out[slot * 12 + i] = shown ? src[b * 12 + i] : 0.f; // (a hidden piece collapsed: its triangles gone)
        }
    }
    *matrices = out;
    return n;
}

const entity_t* sourceOf(const entity_t* e)
{
    if(numWorn <= 0 || e < &worn[0] || e >= &worn[numWorn])
    {
        return nullptr;
    }
    return &cl_entities[wornSource[e - &worn[0]]];
}

// ---- P1: the server's armour (QC's .vr_ar_type, .vr_ar_mask) sent to the clients ----
namespace
{
za::Vector<uint32_t> sent; // per entity: what the clients were last told (packState)

[[nodiscard]] uint32_t stateOf(edict_t* e)
{
    const progs::FieldOffsets& f = progs::fields();
    if(e->free || f.vr_ar_type < 0 || f.vr_ar_mask < 0)
    {
        return 0u;
    }
    const int type = za::clamp(static_cast<int>(progs::fieldFloat(e, f.vr_ar_type)), 0, 3);
    const int mask = static_cast<int>(progs::fieldFloat(e, f.vr_ar_mask)) & allPieces;
    const int wear = f.vr_ar_wear >= 0 ? static_cast<int>(progs::fieldFloat(e, f.vr_ar_wear)) : 0;
    return packState(type, mask, wear);
}

void writeState(sizebuf_t* msg, int num, uint32_t state)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_ARMOR);
    MSG_WriteShort(msg, num);
    MSG_WriteByte(msg, stateType(state));
    MSG_WriteShort(msg, statePieces(state));
    MSG_WriteShort(msg, stateWear(state) & allPieces);
    MSG_WriteShort(msg, stateWear(state) >> NumPieces);
}

// ---- A piece as a model of its own (the broken plate's debris) ----
// The piece's rigid frame: three body vertices under it (the nearest its middle, the farthest from that, the farthest
// from their line), its middle; rest -> as drawn.
struct PlateFrame
{
    int anchor[3]{-1, -1, -1}; // .mdl vertices
    glm::vec3 mid{0.f};
    glm::mat3 rest{1.f};
    bool ok{false};
};

[[nodiscard]] glm::mat3 frameFrom(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 x = safeNormalize(b - a, glm::vec3{1.f, 0.f, 0.f});
    const glm::vec3 z = safeNormalize(glm::cross(x, c - a), glm::vec3{0.f, 0.f, 1.f});
    return glm::mat3{x, glm::cross(z, x), z};
}

[[nodiscard]] PlateFrame plateFrameOf(const Shell& s, int piece)
{
    PlateFrame pf;
    int n = 0;
    for(const Vert& v : s.verts)
    {
        pf.mid += v.piece == piece ? v.p : glm::vec3{0.f};
        n += v.piece == piece ? 1 : 0;
    }
    if(n == 0)
    {
        return pf;
    }
    pf.mid /= static_cast<float>(n);
    // The body's vertices under the piece: the bone(s) most of its vertices weigh on.
    W sum;
    for(const Vert& v : s.verts)
    {
        for(int i = 0; i < rig2::maxBones && v.piece == piece; i++)
        {
            sum.w[i] += v.w.w[i];
        }
    }
    const int bone = heaviest(sum);
    const Body& b = s.body;
    int a = -1, c = -1, d = -1;
    float best = 1e9f;
    for(int v = 0; v < count(b.p); v++)
    {
        const float dist = glm::length(at(b.p, v) - pf.mid);
        a = at(b.w, v).w[bone] > 0.5f && dist < best ? v : a;
        best = at(b.w, v).w[bone] > 0.5f && dist < best ? dist : best;
    }
    if(a < 0)
    {
        return pf;
    }
    best = -1.f;
    for(int v = 0; v < count(b.p); v++)
    {
        const float dist = glm::length(at(b.p, v) - at(b.p, a));
        c = at(b.w, v).w[bone] > 0.5f && dist > best ? v : c;
        best = at(b.w, v).w[bone] > 0.5f && dist > best ? dist : best;
    }
    best = -1.f;
    const glm::vec3 axis = safeNormalize(at(b.p, c) - at(b.p, a), glm::vec3{1.f, 0.f, 0.f});
    for(int v = 0; v < count(b.p); v++)
    {
        const glm::vec3 q = at(b.p, v) - at(b.p, a);
        const float dist = glm::length(q - axis * glm::dot(q, axis));
        d = at(b.w, v).w[bone] > 0.5f && dist > best ? v : d;
        best = at(b.w, v).w[bone] > 0.5f && dist > best ? dist : best;
    }
    if(c < 0 || d < 0 || c == a || d == a || best < 0.5f)
    {
        return pf;
    }
    pf.anchor[0] = at(b.rep, a);
    pf.anchor[1] = at(b.rep, c);
    pf.anchor[2] = at(b.rep, d);
    pf.rest = frameFrom(at(b.p, a), at(b.p, c), at(b.p, d));
    pf.ok = true;
    return pf;
}

[[nodiscard]] const char* baseOf(edict_t* ent)
{
    const char* m = ent && !ent->free && ent->v.model ? PR_GetString(ent->v.model) : "";
    return isWearer(m) ? m : nullptr;
}

[[nodiscard]] const Shell* serverShell(const char* base)
{
    qmodel_t* src = base ? Mod_ForName(base, false) : nullptr;
    return src ? shellFor(src) : nullptr;
}

void putWord(za::Vector<byte>& f, int v)
{
    for(int i = 0; i < 4; i++)
    {
        f.pushBack(static_cast<byte>((static_cast<unsigned>(v) >> (8u * static_cast<unsigned>(i))) & 0xffu));
    }
}
void putFloat(za::Vector<byte>& f, float v)
{
    int i = 0;
    memcpy(&i, &v, 4);
    putWord(f, i);
}
} // namespace

void serverFrame()
{
    if(!sv.active || progs::fields().vr_ar_type < 0)
    {
        return;
    }
    const int n = sv.qcvm.num_edicts;
    if(count(sent) < n)
    {
        sent.resize(static_cast<za::SizeT>(n), 0u);
    }
    for(int i = svs.maxclients + 1; i < count(sent); i++)
    {
        const uint32_t now = i < n ? stateOf(EDICT_NUM(i)) : 0u;
        if(now != at(sent, i) && sv.reliable_datagram.cursize + 16 < sv.reliable_datagram.maxsize)
        {
            writeState(&sv.reliable_datagram, i, now);
            at(sent, i) = now;
        }
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    sent.clear();
    if(!sv.active || progs::fields().vr_ar_type < 0)
    {
        return;
    }
    writeState(msg, -1, 0u);
    sent.resize(static_cast<za::SizeT>(sv.qcvm.num_edicts), 0u);
    for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        at(sent, i) = stateOf(EDICT_NUM(i));
        if(at(sent, i) && msg->cursize + 16 < msg->maxsize)
        {
            writeState(msg, i, at(sent, i));
        }
    }
}

void parseEvent()
{
    const int num = MSG_ReadShort();
    const int type = MSG_ReadByte();
    const int pieces = MSG_ReadShort() & allPieces;
    const int w66 = MSG_ReadShort() & allPieces;
    const int w33 = MSG_ReadShort() & allPieces;
    if(num < 0)
    {
        served.clear();
        numServed = 0;
        return;
    }
    if(num >= count(served))
    {
        served.resize(static_cast<za::SizeT>(num + 1), 0u);
    }
    const uint32_t now = packState(za::clamp(type, 0, 3), pieces, w66 | w33 << NumPieces);
    numServed += (now != 0u ? 1 : 0) - (at(served, num) != 0u ? 1 : 0);
    at(served, num) = now;
}

namespace
{
int pieceOfBone(edict_t* ent, const float* p)
{
    const char* name = nullptr;
    if(box3d::boneAt(ent, glm::vec3{p[0], p[1], p[2]}, &name) < 0 || !name)
    {
        return -2;
    }
    if(!q_strcasecmp(name, "head"))
    {
        return Helmet;
    }
    if(!q_strcasecmp(name, "spine") || !q_strcasecmp(name, "chest"))
    {
        return Cuirass;
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        if(limbBone[piece] && !q_strcasecmp(name, limbBone[piece]))
        {
            return piece;
        }
    }
    return -1;
}

// A plate's three anchors as `ent` is drawn now (hitmodel's poses; else its rest pose where it stands).
void posedAnchors(edict_t* ent, const Shell& s, const PlateFrame& pf, glm::vec3* posed)
{
    bool drawn = true;
    for(int i = 0; i < 3; i++)
    {
        drawn = drawn && hitmodel::vertexPoint(ent, pf.anchor[i], posed[i]);
    }
    if(!drawn)
    {
        const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
        for(int i = 0; i < 3; i++)
        {
            posed[i] = glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} + axes * at(s.body.mdlRest, pf.anchor[i]);
        }
    }
}
} // namespace

int clientState(int num, int what)
{
    if(num < 0 || num >= count(served) || at(served, num) == 0u)
    {
        return -1;
    }
    const uint32_t s = at(served, num);
    return what == 0 ? stateType(s) : what == 1 ? statePieces(s) : stateWear(s);
}

// The point of triangle a, b, c nearest `p`.
[[nodiscard]] glm::vec3 nearestOnTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return a + ab * (d1 / (d1 - d3));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denom = 1.f / za::max(va + vb + vc, 1e-12f);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

// The plate nearest world point `p` on `ent` (each plate's own frame as drawn: the point taken back to its rest pose),
// within `reach` units; -1 none. What a hit there strikes: the ragdoll rig's nearest vertex (pieceOfBone) read a thigh
// plate's lower half as the shin's (the shin's vertices lie nearer the knee's skin than the thigh's do).
int plateNear(edict_t* ent, const Shell& s, const float* p, float reach, float& dist)
{
    bool ok[NumPieces]{};
    glm::vec3 rest[NumPieces]{};
    const glm::vec3 at3{p[0], p[1], p[2]};
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const PlateFrame pf = plateFrameOf(s, piece);
        if(!pf.ok)
        {
            continue;
        }
        glm::vec3 posed[3];
        posedAnchors(ent, s, pf, posed);
        const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
        rest[piece] = at(s.body.mdlRest, pf.anchor[0]) + glm::transpose(turn) * (at3 - posed[0]);
        ok[piece] = true;
    }
    int best = -1;
    float bestD = reach;
    for(int t = 0; t + 2 < count(s.tris); t += 3)
    {
        const Vert& v0 = at(s.verts, at(s.tris, t));
        if(!ok[v0.piece])
        {
            continue;
        }
        const glm::vec3& q = rest[v0.piece];
        const glm::vec3 n = nearestOnTri(q, v0.p, at(s.verts, at(s.tris, t + 1)).p, at(s.verts, at(s.tris, t + 2)).p);
        const float d = glm::distance(q, n);
        best = d < bestD ? v0.piece : best;
        bestD = za::min(d, bestD);
    }
    dist = bestD;
    return best;
}

int pieceAt(edict_t* ent, const float* p)
{
    int piece = pieceOfBone(ent, p);
    // (on a limb, or off every piece's bone: the limb plate struck, the nearest within a plate's reach of the skin
    // under it; the cuirass's and the helmet's hits stay their bones')
    const char* plateBase = baseOf(ent);
    const Shell* ps = plateBase ? serverShell(plateBase) : nullptr;
    const float reach = za::max(vr_agarmor_gap.value, 0.f) + za::max(vr_agarmor_thick.value, 0.05f) + 1.5f;
    float d = reach;
    const int nearPlate = ps ? plateNear(ent, *ps, p, reach, d) : -1;
    // (and on a limb plate itself, or just under it: that plate, whatever bone is nearest: a ragdoll's arm on its chest)
    const bool onPlate = d <= za::max(vr_agarmor_gap.value, 0.f) + za::max(vr_agarmor_thick.value, 0.05f) + 0.25f;
    piece = nearPlate >= 0 && limbBone[nearPlate] && (piece < 0 || limbBone[piece] || onPlate) ? nearPlate : piece;
    // (behind the cuirass's back cut, its own: the enforcer's power pack; the point taken back to the rest pose by the
    // cuirass's frame as drawn)
    const char* base = piece == Cuirass ? baseOf(ent) : nullptr;
    const float cut = backCutOf(base, Cuirass);
    const Shell* s = cut > -1e8f ? serverShell(base) : nullptr;
    const PlateFrame pf = s ? plateFrameOf(*s, Cuirass) : PlateFrame{};
    if(!pf.ok)
    {
        return piece;
    }
    glm::vec3 posed[3];
    posedAnchors(ent, *s, pf, posed);
    const glm::mat3 turn = frameFrom(posed[0], posed[1], posed[2]) * glm::transpose(pf.rest);
    const glm::vec3 rest = at(s->body.mdlRest, pf.anchor[0]) + glm::transpose(turn) * (glm::vec3{p[0], p[1], p[2]} - posed[0]);
    return rest.x < cut ? -1 : piece;
}

uint32_t pieceBones(edict_t* ent, int piece)
{
    if(piece < 0 || piece >= NumPieces)
    {
        return 0u;
    }
    const char* names[2] = {limbBone[piece], nullptr};
    if(piece == Helmet)
    {
        names[0] = "head";
    }
    else if(piece == Cuirass)
    {
        names[0] = "spine";
        names[1] = "chest";
    }
    uint32_t m = 0u;
    for(const char* n : names)
    {
        const int b = n ? box3d::boneNamed(ent, n) : -1;
        m |= b >= 0 && b < 32 ? 1u << b : 0u;
    }
    return m;
}

void plateWhere(edict_t* ent, int piece, int what, float* out)
{
    out[0] = out[1] = out[2] = 0.f;
    const Shell* s = serverShell(baseOf(ent));
    if(!s || piece < 0 || piece >= NumPieces)
    {
        return;
    }
    const PlateFrame pf = plateFrameOf(*s, piece);
    if(!pf.ok)
    {
        return;
    }
    glm::vec3 posed[3];
    posedAnchors(ent, *s, pf, posed);
    const glm::mat3 now = frameFrom(posed[0], posed[1], posed[2]);
    const glm::mat3 turn = now * glm::transpose(pf.rest);
    if(what == 0 || (what >= 2 && what <= 4))
    {
        const glm::vec3 restA = at(s->body.mdlRest, pf.anchor[0]);
        // (2, 3, 4: a point on its outer face 15% / 85% / 50% of its length along its frame's first axis: its ends, its middle)
        glm::vec3 restP = pf.mid;
        if(what != 0)
        {
            const glm::vec3 axis = pf.rest[0];
            float lo = 1e9f, hi = -1e9f;
            for(const Vert& v : s->verts)
            {
                const float x = glm::dot(v.p, axis);
                lo = v.piece == piece && v.layer == 0 ? za::min(lo, x) : lo;
                hi = v.piece == piece && v.layer == 0 ? za::max(hi, x) : hi;
            }
            const float want = lo + (hi - lo) * (what == 2 ? 0.15f : what == 3 ? 0.85f : 0.5f);
            float best = 1e9f;
            for(const Vert& v : s->verts)
            {
                const float d = za::abs(glm::dot(v.p, axis) - want);
                restP = v.piece == piece && v.layer == 0 && d < best ? v.p : restP;
                best = v.piece == piece && v.layer == 0 && d < best ? d : best;
            }
        }
        const glm::vec3 o = posed[0] + turn * (restP - restA);
        out[0] = o.x;
        out[1] = o.y;
        out[2] = o.z;
        return;
    }
    held::anglesFromAxes(now, out, false);
}

unsigned char* derivedFile(const char* name, unsigned int* path_id)
{
    const char* hash = strstr(name, "#plate");
    if(!hash)
    {
        return nullptr;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, name, za::min(sizeof(base), static_cast<size_t>(hash - name) + 1));
    const int piece = Q_atoi(hash + 6);
    const Shell* s = isWearer(base) ? serverShell(base) : nullptr;
    if(!s || piece < 0 || piece >= NumPieces)
    {
        return nullptr;
    }
    const PlateFrame pf = plateFrameOf(*s, piece);
    unsigned int skinPath = 0;
    byte* skin = pf.ok ? COM_LoadMallocFile(skinModelName, &skinPath) : nullptr;
    if(!skin)
    {
        return nullptr;
    }
    const size_t skinSize = static_cast<size_t>(com_filesize);
    int hdr[3] = {0, 0, 0}; // numskins, width, height
    memcpy(hdr, skin + 48, sizeof(hdr));
    const size_t skinBytes = static_cast<size_t>(hdr[0]) * (4u + static_cast<size_t>(hdr[1]) * static_cast<size_t>(hdr[2]));
    if(skinSize < 84u + skinBytes || hdr[0] <= 0)
    {
        VR_HeapFree(skin);
        return nullptr;
    }
    // Its vertices in its own frame (rest -> the frame's axes, round its middle).
    za::Vector<int> map(s->verts.size(), -1);
    za::Vector<glm::vec3> local;
    za::Vector<glm::vec2> st;
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int v = 0; v < count(s->verts); v++)
    {
        if(at(s->verts, v).piece == piece)
        {
            at(map, v) = count(local);
            const glm::vec3 q = glm::transpose(pf.rest) * (at(s->verts, v).p - pf.mid);
            local.pushBack(q);
            st.pushBack(at(s->verts, v).st);
            lo = glm::min(lo, q);
            hi = glm::max(hi, q);
        }
    }
    za::Vector<int> tris;
    for(int i = 0; i + 2 < count(s->tris); i += 3)
    {
        if(at(map, at(s->tris, i)) >= 0)
        {
            tris.pushBack(at(map, at(s->tris, i)));
            tris.pushBack(at(map, at(s->tris, s->body.nativeFlip ? i + 2 : i + 1)));
            tris.pushBack(at(map, at(s->tris, s->body.nativeFlip ? i + 1 : i + 2)));
        }
    }
    const glm::vec3 scale = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});
    za::Vector<byte> f;
    putWord(f, IDPOLYHEADER);
    putWord(f, ALIAS_VERSION);
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, scale[i]);
    }
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, lo[i]);
    }
    putFloat(f, glm::length(glm::max(glm::abs(lo), glm::abs(hi))));
    for(int i = 0; i < 3; i++)
    {
        putFloat(f, 0.f);
    }
    putWord(f, hdr[0]);
    putWord(f, hdr[1]);
    putWord(f, hdr[2]);
    putWord(f, count(local));
    putWord(f, count(tris) / 3);
    putWord(f, 1);
    putWord(f, 0); // synctype
    putWord(f, 0); // flags
    putFloat(f, 8.f);
    for(size_t i = 0; i < skinBytes; i++)
    {
        f.pushBack(skin[84 + i]);
    }
    VR_HeapFree(skin);
    for(const glm::vec2& t : st)
    {
        putWord(f, 0);
        putWord(f, za::clamp(static_cast<int>(t.x), 0, hdr[1] - 1));
        putWord(f, za::clamp(static_cast<int>(t.y), 0, hdr[2] - 1));
    }
    for(int i = 0; i + 2 < count(tris); i += 3)
    {
        putWord(f, 1);
        putWord(f, at(tris, i));
        putWord(f, at(tris, i + 1));
        putWord(f, at(tris, i + 2));
    }
    putWord(f, 0); // a single frame
    const byte box[8] = {0, 0, 0, 0, 255, 255, 255, 0};
    for(byte b : box)
    {
        f.pushBack(b);
    }
    for(int i = 0; i < 16; i++)
    {
        f.pushBack(static_cast<byte>(i < 5 ? "plate"[i] : 0));
    }
    for(const glm::vec3& q : local)
    {
        for(int i = 0; i < 3; i++)
        {
            f.pushBack(static_cast<byte>(za::clamp(static_cast<int>((q[i] - lo[i]) / scale[i] + 0.5f), 0, 255)));
        }
        f.pushBack(0);
    }
    byte* out = static_cast<byte*>(VR_HeapMalloc(f.size()));
    memcpy(out, f.data(), f.size());
    com_filesize = static_cast<int>(f.size());
    if(path_id)
    {
        *path_id = skinPath;
    }
    return out;
}

void init()
{
    Cmd_AddCommand("vr_agarmor_info", info_f);
    Cmd_AddCommand("vr_agarmor_rebuild", rebuild_f);
    Cmd_AddCommand("vr_agarmor_set", set_f);
    Cmd_AddCommand("vr_agarmor_check", check_f);
    Cmd_AddCommand("vr_agarmor_headcam", headcam_f);
}

} // namespace qvr::agarmor
