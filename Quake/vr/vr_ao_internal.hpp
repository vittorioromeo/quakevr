#pragma once

// vr_ao_internal.hpp -- what vr_ao.cpp (see vr_ao.hpp) shares with vr_ao_cache.cpp (the bakes kept on disk). Internal:
// included by those two alone. What was the file's anonymous namespace and both use is namespace qvr::ao::detail.

#include "vr_engine.hpp"
#include "vr_sha256.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <glm/vec3.hpp>

namespace qvr::ao::detail
{

constexpr int RAYS = 24;
// The bake's other numbers (all of them: the disk cache's key holds them and the rays, so a change here bakes again).
constexpr float REACH_SHARE = 0.125f; // reach: this share of the first pose's diagonal ...
constexpr float REACH_MIN = 1.f;      // ... within these units
constexpr float REACH_MAX = 16.f;
constexpr float NEAREST_SHARE = 0.02f; // hits nearer than this share of the reach are left out
constexpr float LIFT_SHARE = 0.03f;    // the rays start this share of the reach off the surface, along its normal

// The bake's rays (vr_ao.cpp).
extern const za::Array<glm::vec3, RAYS> rayDirs;

struct Tri
{
    za::U16 v[3];
};

struct BakeJob
{
    za::String name;
    qmodel_t* model{nullptr};
    za::U64 hash{0};
    int numverts{0};
    int numposes{0};
    za::Vector<trivertx_t> verts;
    za::Vector<Tri> tris;
    glm::vec3 scale{1.f}, origin{0.f};
    za::Vector<unsigned char> vis;
    double seconds{0.0};
    // The disk cache (vr_ao_cache, read when it was queued): its folder, and what the bake task did with it.
    int cacheMode{0};
    bool fromDisk{false}; // read, not baked
    bool rejected{false}; // a file was there, but not a whole one of this model's (baked again, and written over)
    bool written{false};
    int checked{0};       // vr_ao_cache 2: baked anyway and compared with the file: 1 the same, -1 different
    bool aborted{false};  // the game stopped part way
};

// The AO's FNV-1a start: FNV's offset basis a digit short (14695981039346656037 without its last 7). Kept: the files'
// payload hashes (FileHeader::payloadHash) are made with it.
constexpr za::U64 aoFnvStart = 1469598103934665603ull;

// Bump it when the bake's bytes or the file's layout change (not for a faster bake of the same bytes: the grid and the
// SIMD rays of 2026-10-06 kept version 1, checked against the reference and the files).
constexpr za::U32 BAKE_VERSION = 1;
constexpr za::U64 CACHE_BUDGET = 64ull << 20; // bytes in the version's folder (a game's full set is a few MB)

enum class FileRead
{
    Missing,
    Rejected,
    Read
};

// vr_ao_cache.cpp
[[nodiscard]] sha256::Digest bakeKey(const BakeJob& job);
[[nodiscard]] za::String versionDir();
[[nodiscard]] za::String fileFor(const sha256::Digest& key);
[[nodiscard]] FileRead readBake(const za::String& path, const BakeJob& job, const sha256::Digest& key, za::Vector<unsigned char>& vis);
[[nodiscard]] bool writeBake(const za::String& path, const BakeJob& job, const sha256::Digest& key);
void pruneOnce();

} // namespace qvr::ao::detail
