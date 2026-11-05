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
constexpr const char* checked[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl", "progs/vrknight.mdl", "progs/vrhknight.mdl"};

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

inline constexpr int plateVerts = 28; // a plate's vertices (vr_agarmor.cpp addPlate): the face's four first, in corner order

// A piece's grid: rings up its axis (hAt), columns round it (azOf: mirrored about its front), a radius at every corner.
struct PlateGrid
{
    int rows{0}, cols{0};
    bool centred{false};  // a seam at the front (the cuirass); else a plate facing front
    float hTop{0.f}, hBottom{0.f};
    float spike{1.4f};    // a corner's radius at most, of the piece's middle one
    float yoke{0.f};      // the cuirass's first row: this much above the rest, closing in on the neck
    za::Vector<glm::vec2> axis; // per ring: the middle of the piece there, in (fwd, left)
    za::Vector<float> radius;   // per ring x column
    [[nodiscard]] float hAt(int ring) const
    {
        if(yoke > 0.f)
        {
            return ring == 0 ? hTop + yoke : hTop + (hBottom - hTop) * static_cast<float>(ring - 1) / static_cast<float>(rows - 1);
        }
        return hTop + (hBottom - hTop) * static_cast<float>(ring) / static_cast<float>(rows);
    }
    [[nodiscard]] float azOf(int col) const
    {
        return (static_cast<float>(col) + (centred ? 0.f : 0.5f)) * 6.2831853f / static_cast<float>(cols);
    }
    [[nodiscard]] int mirrorOf(int col) const
    {
        return centred ? (cols - col) % cols : cols - 1 - col;
    }
    [[nodiscard]] float& at2(int ring, int col)
    {
        return radius[static_cast<za::SizeT>(ring * cols + ((col % cols) + cols) % cols)];
    }
    [[nodiscard]] float at2(int ring, int col) const
    {
        return radius[static_cast<za::SizeT>(ring * cols + ((col % cols) + cols) % cols)];
    }
};

// A piece's grid where it stands: the plates' places, for what's inside them (vr_agarmor.cpp tubeAround).
struct PlateTube
{
    PlateGrid grid;
    glm::vec3 origin{0.f}, up{0.f, 0.f, 1.f}, fwd{1.f, 0.f, 0.f}, left{0.f, 1.f, 0.f};
    float length{0.f};
    int bone{-1}; // a limb's own bone
    // (its frame as a model of its own, made when first asked: vr_agarmor.cpp plateFrameOf)
    mutable bool frameKnown{false}, frameOk{false};
    mutable int frameAnchor[3]{-1, -1, -1};
    mutable int frameBone{-1};
    mutable glm::vec3 frameMid{0.f};
    mutable glm::mat3 frameRest{1.f};
};

struct Shell
{
    PlateTube tubes[NumPieces];
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
// The rig a shell is made on (the player's bones or the authored rig), a piece's region's bones and its frame (origin, up, fwd, left).
[[nodiscard]] const rig2::Rig* rigOfShell(qmodel_t* src);
[[nodiscard]] uint32_t pieceRegionBones(const rig2::Rig& rig, int piece);
void pieceAxes(const rig2::Rig& rig, int piece, glm::vec3& origin, glm::vec3& up, glm::vec3& fwd, glm::vec3& left);

// vr_agarmor_check.cpp: the vr_agarmor_check command.
void check_f();
void lookCheck_f(); // vr_agarmor_look_check: the plates' generator and paint

} // namespace qvr::agarmor::detail
