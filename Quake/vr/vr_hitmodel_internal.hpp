#pragma once

// vr_hitmodel.hpp -- what vr_hitmodel.cpp (see vr_hitmodel.hpp) shares with its diagnostics, vr_hitmodel_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// hitmodel::detail: its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_geom.hpp"
#include "vr_hitmodel.hpp"
#include "vr_ad.hpp" // AD support: small pellets retain precise model/corpse impacts.
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
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Iota.hpp"
#include "Zancle/Algorithm/NthElement.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Base/BitCast.hpp"
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
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

namespace qvr::hitmodel::detail
{

using geom::vec;

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
inline Stats stats;

struct Zone
{
    int num{0};
    int zone{0};
    glm::vec3 lo{0.f}, hi{0.f};
    float radius{0.f};
};
inline za::Vector<Zone> zones;   // this server frame's (QC's VR_Decap_DebugFrame)
inline double zonesTime{-1.0};   // its sv.qcvm.time

struct ZoneVertex { glm::vec3 rest, drawn; };
using ZonePolygon = za::Vector<ZoneVertex>;
struct ZoneSample { int num, region; glm::vec3 rest; };
inline za::Vector<ZoneSample> zoneSamples;
inline bool zoneSampling = false; // painting positional damage's regions (animatedZones): their pieces are vr_hitzones_check's samples
inline int zoneModels = 0, zoneFallbacks = 0;
inline za::Array<int, 5> zonePieces{}; // (body, head, extremities, legs; melee beheading's)
inline int zoneDecapModels = 0;        // models whose melee beheading zone is painted on as drawn
inline za::U32 zonePoseHash = 0;

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

inline ankerl::unordered_dense::map<za::String, za::UniquePtr<Mesh>> meshes; // by model name, kept from map to map (pointers kept)
inline za::Array<Mesh*, MAX_MODELS> byIndex{};                  // this map's, by model index
inline za::Array<const qmodel_t*, MAX_MODELS> byIndexModel{};  // (what byIndex was found for)

[[nodiscard]] inline const trivertx_t* posesOf(const aliashdr_t* hdr)
{
    return reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
}

[[nodiscard]] inline glm::vec3 raw(const trivertx_t& v)
{
    return {v.v[0], v.v[1], v.v[2]};
}

[[nodiscard]] inline glm::vec3 anorm(const trivertx_t& v)
{
    const int i = v.lightnormalindex < NUMVERTEXNORMALS ? v.lightnormalindex : 0;
    return {r_avertexnormals[i][0], r_avertexnormals[i][1], r_avertexnormals[i][2]};
}

[[nodiscard]] inline const qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}

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
    // A ragdoll (vr_ragdoll.cpp): its vertices and normals as drawn, in the world (ragdollScratch); null otherwise.
    const glm::vec3* skinned{nullptr};
    const glm::vec3* skinnedN{nullptr};
};

inline bool checkNoLerp = false; // (vr_hitmodel_check: the model at its frame and origin, for comparison)

struct Verts
{
    const trivertx_t* v1;
    const trivertx_t* v2;
    float blend;

    [[nodiscard]] glm::vec3 at(const Drawn& d, int vi, float grow) const
    {
        if(d.skinned)
        {
            return d.skinned[vi] + d.skinnedN[vi] * grow;
        }
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

[[nodiscard]] inline Verts vertsOf(const Drawn& d)
{
    const trivertx_t* poses = posesOf(d.hdr);
    return {poses + static_cast<za::SizeT>(d.pose1) * d.mesh->numverts, poses + static_cast<za::SizeT>(d.pose2) * d.mesh->numverts,
        d.blend};
}

const Mesh* meshOf(int index, const qmodel_t* model, const aliashdr_t*& hdr);
bool drawnOf(edict_t* ent, Drawn& d);

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
inline za::Vector<Track> tracks;

} // namespace qvr::hitmodel::detail
