// vr_agarmor.cpp -- armour for the armed humanoids, P0: shells made from the monster's own mesh, drawn on its authored
// rig (vr_agarmor.hpp has the method; ROUND22.md, "Armour for armed humanoids, P0").
//
// vr_agarmor_check [model] [random poses]: the shells' fit measured on the authored rig's poses (its rest, every joint
// near the pieces swept through its limits, ragdoll-like random poses within them): body vertices inside a plate (the
// body poking into or through it: inside the closed shell, by its winding number) and the plates' outer vertices
// inside the body (the plate sunk under the skin: the body showing over it), each by the region (neck and head,
// shoulders and arms, waist and legs, the torso itself) and its deepest. Ends on "agarmor check: N passed, M failed":
// the shells made, made alike twice (the same hash), closed, and clean in the rest pose.

#include "vr_agarmor.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
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
constexpr int maxWorn = 64;   // armour entities a frame
constexpr int maxSlots = 64;  // the armour model's bones (each piece's bones its own)
constexpr int maxNamed = 64;  // vr_agarmor_set's entities
constexpr const char* armorSuffix = "#armor";
constexpr const char* rigSuffix = "#rig2";
constexpr const char* skinModelName = "progs/vragarmor.mdl";
constexpr const char* wearers[] = {"progs/vrgrunt.mdl", "progs/soldier.mdl"}; // P0: the grunt

// The atlas (make_agarmor.py's REGIONS): texels of its 256 x 128 skins.
struct Block
{
    float x0, y0, x1, y1;
};
constexpr Block cuirassBlock{0.f, 0.f, 160.f, 128.f};
constexpr Block helmetBlock{160.f, 0.f, 256.f, 96.f};
constexpr Block innerBlock{160.f, 96.f, 208.f, 128.f};
constexpr Block rimBlock{208.f, 96.f, 256.f, 128.f};

constexpr const char* pieceNames[NumPieces] = {"cuirass", "helmet"};

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
    bool ok{false};
};

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

// The piece's field at a body vertex: >= 0 covered (its weight on the region's bones past 0.5; the helmet above its
// brow line too: low at the back, above the eyes in front).
[[nodiscard]] float fieldOf(const Body& b, int v, int piece, const PieceFrame& f)
{
    const W& w = at(b.w, v);
    float region = 0.f;
    for(int i = 0; i < rig2::maxBones; i++)
    {
        region += ((f.bones >> static_cast<unsigned>(i)) & 1u) ? w.w[i] : 0.f;
    }
    float field = region - 0.5f;
    if(piece == Helmet)
    {
        const glm::vec3 q = at(b.p, v) - f.origin;
        const float h = glm::dot(q, f.up) / f.length, fw = glm::dot(q, f.fwd) / f.length;
        const float brow = vr_agarmor_brow_back.value + (vr_agarmor_brow_front.value - vr_agarmor_brow_back.value) * za::clamp(fw / 0.4f, 0.f, 1.f);
        field = za::min(field, (h - brow) * 2.f);
    }
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
    float made{0.f}; // ms
};

// A piece's surface as made (before the shell): its vertices on the body (orig), smoothed (p), their weights.
struct Local
{
    za::Vector<glm::vec3> p, orig;
    za::Vector<W> w;
    za::Vector<int> tris;
};

void clipPiece(const Body& b, int piece, const PieceFrame& f, Local& L)
{
    L = Local{};
    const int nv = count(b.p);
    za::Vector<float> field(static_cast<za::SizeT>(nv), 0.f);
    for(int v = 0; v < nv; v++)
    {
        at(field, v) = fieldOf(b, v, piece, f);
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

// One piece's shell from its surface: outer and inner layers, a rim along its open edge, texture coordinates.
void shellOf(Shell& s, Local& L, int piece, const PieceFrame& f)
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

    // Texture coordinates (a, b) of the outer place: the cuirass round the torso and up it; the helmet a disc from
    // above, its open edge at the rim's radius whatever its height there.
    za::Vector<glm::vec2> ab(static_cast<za::SizeT>(nv), glm::vec2{0.f});
    if(piece == Cuirass)
    {
        float lo = 1e9f, hi = -1e9f;
        for(int v = 0; v < nv; v++)
        {
            const float h = glm::dot(at(L.p, v) - f.origin, f.up);
            lo = za::min(lo, h);
            hi = za::max(hi, h);
        }
        for(int v = 0; v < nv; v++)
        {
            const glm::vec3 q = at(L.p, v) - f.origin;
            const float az = za::atan2(glm::dot(q, f.left), glm::dot(q, f.fwd)) / (2.f * pi); // -0.5..0.5, 0 the front
            const float h = glm::dot(q, f.up);
            at(ab, v) = glm::vec2{az + 0.5f, 1.f - (h - lo) / za::max(hi - lo, 1e-3f)};
        }
    }
    else
    {
        const glm::vec3 centre = f.origin + f.up * (0.3f * f.length);
        constexpr int bins = 24;
        float edge[bins];
        for(float& e : edge)
        {
            e = 0.f;
        }
        za::Vector<glm::vec2> polar(static_cast<za::SizeT>(nv), glm::vec2{0.f});
        for(int v = 0; v < nv; v++)
        {
            const glm::vec3 d = safeNormalize(at(L.p, v) - centre, f.up);
            const float pol = za::acos(za::clamp(glm::dot(d, f.up), -1.f, 1.f));
            const float az = za::atan2(glm::dot(d, f.left), glm::dot(d, f.fwd));
            at(polar, v) = glm::vec2{pol, az};
            const int bin = za::clamp(static_cast<int>((az + pi) / (2.f * pi) * bins), 0, bins - 1);
            edge[bin] = at(open, v) ? za::max(edge[bin], pol) : edge[bin];
        }
        float fallback = 0.f;
        for(float e : edge)
        {
            fallback = za::max(fallback, e);
        }
        for(int v = 0; v < nv; v++)
        {
            const float pol = at(polar, v).x, az = at(polar, v).y;
            // The rim's radius at its angle (bins blended, an empty bin its neighbours' or the widest)
            const float x = (az + pi) / (2.f * pi) * bins - 0.5f;
            const int b0 = (static_cast<int>(x + bins) % bins), b1 = (b0 + 1) % bins;
            const float t = x + bins - static_cast<float>(static_cast<int>(x + bins));
            const float e0 = edge[b0] > 0.f ? edge[b0] : fallback, e1 = edge[b1] > 0.f ? edge[b1] : fallback;
            const float rim = za::max(e0 + (e1 - e0) * t, 1e-3f);
            const float r = za::min(0.9f * pol / rim, 0.97f);
            at(ab, v) = glm::vec2{0.5f + 0.5f * r * za::cos(az), 0.5f - 0.5f * r * za::sin(az)};
        }
    }
    const Block& outerBlock = piece == Cuirass ? cuirassBlock : helmetBlock;

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
                if(piece == Cuirass && hi - lo > 0.5f && at(ab, c[k]).x < 0.5f)
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
        const PieceFrame f = frameOf(*rig, piece);
        const int firstVert = count(s.verts), firstTri = count(s.tris);
        if(f.ok)
        {
            Local L;
            clipPiece(s.body, piece, f, L);
            for(int i = 0; i < za::clamp(static_cast<int>(vr_agarmor_subdiv.value), 0, 2); i++)
            {
                subdivide(L);
            }
            smooth(L, za::clamp(static_cast<int>(vr_agarmor_smooth.value), 0, 50));
            if(!L.tris.empty())
            {
                shellOf(s, L, piece, f);
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
    for(const char* w : wearers)
    {
        if(!strcmp(base, w))
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

void wornBy(int num, int& type, int& pieces)
{
    type = za::clamp(static_cast<int>(vr_agarmor.value), 0, 4);
    type = type == 4 ? 1 + num % 3 : type;
    pieces = static_cast<int>(vr_agarmor_pieces.value) & ((1 << NumPieces) - 1);
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
    for(const char* w : wearers)
    {
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
    }
    Con_Printf("agarmor: %d drawn last frame\n", numWorn);
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
        Con_Printf("vr_agarmor_set <entity> <type 0 none, 1 green, 2 yellow, 3 red | -1 vr_agarmor's> [pieces: 1 cuirass, 2 helmet]\n");
        return;
    }
    setWorn(Q_atoi(Cmd_Argv(1)), Q_atoi(Cmd_Argv(2)), Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : (1 << NumPieces) - 1);
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

void check_f()
{
    const char* modelName = Cmd_Argc() > 1 ? Cmd_Argv(1) : wearers[0];
    const int randoms = Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 0, 5000) : 200;
    int passed = 0, failed = 0;
    const auto verdict = [&](bool ok, const char* what) {
        Con_Printf("agarmor check: %s: %s\n", ok ? "PASS" : "FAIL", what);
        (ok ? passed : failed)++;
    };
    qmodel_t* src = Mod_ForName(modelName, false);
    Shell* sp = src ? shellFor(src) : nullptr;
    const rig2::Rig* rig = src ? rig2::rigFor(src) : nullptr;
    verdict(sp && rig, "the shells made");
    if(!sp || !rig)
    {
        Con_Printf("agarmor check: %d passed, %d failed\n", passed, failed);
        return;
    }
    // Made again: alike.
    Shell again;
    again.model = sp->model;
    (void)buildShell(src, again);
    Con_Printf("agarmor check: hash %08x, again %08x; %d vertices, %d triangles (cuirass %d, helmet %d), %d slots\n", sp->hash, again.hash,
        count(sp->verts), count(sp->tris) / 3, sp->pieceTris[Cuirass], sp->pieceTris[Helmet], sp->numSlots);
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
        verdict(count(s.tris) / 3 <= 2500, "at most 2500 triangles a body");
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
    Con_Printf("agarmor check: %d passed, %d failed\n", passed, failed);
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
        named.pushBack(Named{num, za::clamp(type, 0, 3), pieces & ((1 << NumPieces) - 1)});
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
    if(cls.state != ca_connected || !cl_entities || (vr_agarmor.value == 0.f && named.empty()))
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
        int type = 0, pieces = 0;
        wornBy(num, type, pieces);
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
        const int k = numWorn++;
        entity_t& a = worn[k];
        a = *e;
        a.model = armor;
        a.skinnum = type - 1;
        a.frame = 0;
        a.previouspose = a.currentpose = 0;
        a.lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
        wornSource[k] = num;
        wornPieces[k] = pieces;
        wornShell[k] = shell;
        wornSlots[k] = at(shells, shell).numSlots;
        cl_visedicts[cl_numvisedicts++] = &a;
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

void init()
{
    Cmd_AddCommand("vr_agarmor_info", info_f);
    Cmd_AddCommand("vr_agarmor_rebuild", rebuild_f);
    Cmd_AddCommand("vr_agarmor_set", set_f);
    Cmd_AddCommand("vr_agarmor_check", check_f);
}

} // namespace qvr::agarmor
