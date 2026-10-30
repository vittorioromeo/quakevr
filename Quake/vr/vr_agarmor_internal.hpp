#pragma once
// vr_agarmor_internal.hpp -- what vr_agarmor.cpp (see vr_agarmor.hpp) shares with its self-check, vr_agarmor_check.cpp.
// Internal: included by those two alone. The shell's types and the small helpers (once vr_agarmor.cpp's anonymous
// namespace) are namespace agarmor::detail; buildShell and shellFor are defined in vr_agarmor.cpp, check_f (the
// vr_agarmor_check command) in vr_agarmor_check.cpp.
#include "vr_agarmor.hpp"
#include "vr_engine.hpp"
#include "vr_rig2.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>

#include <stdint.h>

namespace qvr::agarmor::detail
{

constexpr int maxSlots = 128; // the armour model's bones (each piece's bones its own)

// The ones with authored rigs the check runs on (no argument).
constexpr const char* checked[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl"};

constexpr const char* pieceNames[NumPieces] = {"cuirass", "helmet", "pauldron_l", "pauldron_r", "bracer_l", "bracer_r", "thigh_l",
    "thigh_r", "shin_l", "shin_r"};

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
[[nodiscard]] inline W mixW(const W& a, const W& b, float t)
{
    W o;
    for(int i = 0; i < rig2::maxBones; i++)
    {
        o.w[i] = a.w[i] + (b.w[i] - a.w[i]) * t;
    }
    return o;
}
[[nodiscard]] inline int heaviest(const W& w)
{
    int best = 0;
    for(int i = 1; i < rig2::maxBones; i++)
    {
        best = w.w[i] > w.w[best] ? i : best;
    }
    return best;
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

// vr_agarmor.cpp: a model's shell made (again), and the one for a model (made when first asked; null: none).
bool buildShell(qmodel_t* src, Shell& s);
[[nodiscard]] Shell* shellFor(qmodel_t* src);

// vr_agarmor_check.cpp: the vr_agarmor_check command.
void check_f();

} // namespace qvr::agarmor::detail
