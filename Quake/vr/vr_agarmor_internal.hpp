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
    float hTopRaw{0.f};   // (hTop before armour.txt's `start`: the upper arms' common top)
    float spike{1.4f};    // a corner's radius at most, of the piece's middle one
    float yoke{0.f};      // the cuirass's first row: this much above the rest, closing in on the neck
    float sleeveEnd{0.f}; // a limb's part: its joint's sleeve's far end (the joint, or past it as far as its region goes)
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

// ---- The elbows' and the knees' leather (vr_agarmor_bridge.cpp; ROUND24.md, "Leather bridge at elbows and knees") ----
// One tube per joint made every frame the body is drawn: from the posed end row of the upper part's plates
// (pauldron, thigh) to the posed first row of the lower part's (bracer, shin), curved along each row's direction, its bellows
// bunched on the inside of the bend and stretched flat on the outside. A part not worn: that end on the skin past the joint.
// Drawn as an entity of its own whose every vertex is a bone of its own (bridgeBones: each frame's places, through the
// skinned path's per-entity bones: lighting, shadows, culling and the rest as the plates').
enum Joint : int
{
    ElbowL,
    ElbowR,
    KneeL,
    KneeR,
    NumJoints
};
inline constexpr int bridgeCols = 6;      // a ring's corners: the limbs' grids' columns
inline constexpr int bridgeMostFolds = 4; // (the rings within one entity's 256 bones: 4 joints x 9 rings x 7)
inline constexpr int bridgeMostRings = 9;
inline constexpr int bridgeMostVerts = NumJoints * bridgeMostRings * (bridgeCols + 1);
inline constexpr int bridgeMostUnder = 96; // the skin's vertices between the rows a joint keeps under its leather

// A point carried by the bones as an armour vertex is: its rest place, its weights quantised as the armour model's are
// (makeModel), so its posed place is the drawn plate's to the float.
struct Skinned
{
    glm::vec3 p{0.f};
    int bone[4]{};
    float w[4]{};
    int n{0};
};

struct BridgeEnd
{
    bool ok{false};
    Skinned ring[bridgeCols];
    glm::vec3 x{1.f, 0.f, 0.f}, y{0.f, 1.f, 0.f}, z{0.f, 0.f, 1.f}; // its rest frame: z down the limb (upper part to lower)
    int bone{-1};                                                  // the bone turning that frame (the part's own limb bone)
};

struct Bridge
{
    bool ok{false};
    int upper{-1}, lower{-1};   // pieces
    BridgeEnd plates[2];        // 0 the upper part's last row, 1 the lower part's first (their feet)
    BridgeEnd skin[2];          // the same places on the skin (a part not worn)
    int folds{0}, rings{0}, first{0}; // first: its vertices' first in the bridge model
    float depth{0.f}, standoff{0.f}, scale{1.f};
    float taper[4]{1.f, 1.f, 1.f, 1.f}; // armour.txt's: the upper sleeve's taper_start, _end, the lower's
    Skinned under[bridgeMostUnder];      // the skin between the rows (kept under the leather at any bend)
    float underAlong[bridgeMostUnder]{}; // ... each one's place along the leather at rest (0 the upper row, 1 the lower)
    int numUnder{0};
    glm::vec3 pivot{0.f};                // the joint (rest; the lower part's bone's pivot): the curve's turn is round it
};

// What a bridge made: its end rings against the rows they meet, and the skin left outside it (vr_agarmor_bridge_check).
struct BridgeDiag
{
    float ringGap{0.f};          // the end rings' furthest corner from the rows' (units)
    int under{0}, outside{0};    // the skin's vertices tested, those outside the leather (after the push)
    int outsideInner{0};         // ... of which on the inside of the bend (the crease: the limbs themselves overlap there)
    float worst{0.f};            // the furthest outside (units)
    float worstAt{0.f};          // ... where along the leather (its rings: 0 the upper row's .. rings - 1)
    int atRows{0};               // outside within half a ring of either row (the plates' own fit there: not counted above)
    // (the curve: the rows' turn between them, the joint's reach along each row's line, their middles apart, the most pushed)
    float theta{0.f}, a0{0.f}, a1{0.f}, chord{0.f}, pushMost{0.f};
    float fit{0.f}, keep{0.f}, back{0.f}; // (the tangents' share kept for a forward middle, kept, the curve against its rings' frames)
    float m0{0.f}, m1{0.f};                   // (the tangents' lengths drawn)
    float creaseCut{0.f}, needMost{0.f};      // (the push the crease gave up, the most any skin sample asked: after the pushes)
    float firstNeed{0.f}, firstCrease{0.f};   // (the most a sample asked before any push, and that sample's crease share)
    int rings{0};
    glm::vec3 ringX[bridgeMostRings]{}, ringZ[bridgeMostRings]{}; // (each ring's frame: its x, its normal)
};

struct Shell
{
    PlateTube tubes[NumPieces];
    Bridge bridges[NumJoints];
    int bridgeVerts{0};          // the bridge model's vertices (0: none)
    za::Vector<glm::vec3> bridgeRestP, bridgeRestN; // its rest places and normals (the model's bind pose)
    za::Vector<int> bridgeTris;
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
    uint32_t tweaks{0};  // (armourdata::signature of its model when made: armour.txt's tweaks it has)
    int tweaksGen{0};    // (armourdata::generation then)
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

// vr_agarmor_bridge.cpp: a point's weights as the armour model quantises them; its place posed by `bones` (rig bones, 12
// floats each, row-major 3x4; false: a bone past `have`).
[[nodiscard]] Skinned skinnedOf(const glm::vec3& p, const W& w);
[[nodiscard]] bool posedOf(const Skinned& s, const float* bones, int have, glm::vec3& out);
// The shell's bridges' rest pose (their model's bind pose) and triangles, once its bridges are set (buildShell).
void bridgeRest(Shell& s);
// Joint j's vertices posed by `bones` (its rings in order, bridgeCols + 1 a ring), the ends on the plates worn (else the
// skin). False: not drawn (a bone missing). `diag` optional.
bool evalBridge(const Shell& s, int j, const float* bones, int have, bool upperWorn, bool lowerWorn, glm::vec3* outP,
    glm::vec3* outN, BridgeDiag* diag);
// Every bridge vertex's bone (a matrix each, 12 floats) for the pieces worn: a joint with neither part worn, or leather off,
// collapsed. Returns the count (s.bridgeVerts).
int bridgeBones(const Shell& s, int pieces, const float* bones, int have, float* out);
// "<model>#bridge": the bridge model of a shell (its skins vragarmor.mdl's `skins`).
bool makeBridgeModel(qmodel_t* mod, const Shell& s, const aliashdr_t* skins);
void bridgeCheck_f(); // vr_agarmor_bridge_check [model]: the end rings on the rows and the skin under it over each joint's range
void bridgeStats_f(); // vr_agarmor_bridge_stats: the bridges made lately and their cost
void bridgeStatsAdd(double seconds, int joints);
// vr_agarmor_bridge_trace (debug, a mask of joints): each drawn armoured body's leather against its last frame's, a console
// line a joint a frame (its furthest vertex move and its rows', its rings' and its rows' turn, the curve's numbers).
void bridgeTrace(const Shell& s, int ent, const char* model, int pieces, const float* bones, int have);

// vr_agarmor_check.cpp: the vr_agarmor_check command.
void check_f();
void lookCheck_f(); // vr_agarmor_look_check: the plates' generator and paint
void popCheck_f();  // vr_agarmor_pop_check [model]: the body against the plates, by region, pose and pieces worn
void popSample_f(); // vr_agarmor_pop_sample: the armoured bodies as drawn now, added to the sums
void popReport_f(); // vr_agarmor_pop_report: the sums, and started again

// vr_agarmor.cpp: the armoured bodies drawn this frame, for the sampler.
[[nodiscard]] int wornCount();
[[nodiscard]] const Shell* wornAt(int k, int& entnum);

} // namespace qvr::agarmor::detail
