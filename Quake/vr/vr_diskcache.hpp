// vr_diskcache.hpp -- the derived-data cache (docs/vr-port/CACHES.md): work the loads would redo every session, kept on
// disk under one root shared by every game folder, `<cache root>/<product>/<build>/<file>`:
// - <cache root>: vr_cache_dir when set (tests: an empty folder is a first-ever session), else `<base>/cache` (the last
//   base directory's, beside the map index's files). The files' names are hashes of everything their maker reads, so
//   one root serves every game (id1's grunt read through hipnotic is the same file). Nothing in it is ever committed.
// - <product>: one folder per kind of work (Product below), each with its own verify setting (0 off, 1 read and write,
//   2 made anyway and compared with the file).
// - <build>: the maker's own version (a constant its owner bumps when the maker changes: a changed maker never reads an
//   old file; a new build of the same maker keeps them). Made once a session at its first use; the product's other
//   builds' folders are removed then, and its oldest files past its budget.
// Files are written beside their place and renamed (writeAtomic: another copy of the game never reads half a file).
// The counts (hits, misses, files written, checks) per product: vr_cache_info. A miss while playing (not in a load) is
// printed with developer 1 ("cache miss in play"), so first uses the loads didn't prepare show.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <stddef.h>

namespace qvr::diskcache
{

enum class Product : int
{
    NormalMaps, // skins' normal maps (vr_normalmaps.cpp; vr_normalmap_cache)
    GunPieces,  // guns' convex pieces (vr_box3d.cpp gunPieces; vr_gun_pieces_cache)
    Hulls,      // the world's compiled trees (vr_hull.cpp; vr_hull_cache)
    AO,         // the models' own occlusion bakes (vr_ao.cpp; vr_ao_cache)
    Dequant,    // .mdl poses dequantised (vr_mdldequant.cpp; vr_mdl_dequant_cache)
    Rigs,       // the monsters' ragdoll rigs (vr_ragdoll.cpp; vr_ragdoll_rig_cache)
    Count
};

// The cache's root (vr_cache_dir, else `<base>/cache`).
[[nodiscard]] za::String root();

// Its folder's name under the root ("normalmaps").
[[nodiscard]] const char* kindOf(Product p);

// `<cache root>/<kind>/<build>`, nothing made (a read's).
[[nodiscard]] za::String folderOf(Product p, const char* build);

// The same folder made ready for writing (once a session per root and build, any thread: the other builds' folders
// removed, the oldest files past the product's budget, the folder made).
[[nodiscard]] za::String prepare(Product p, const char* build);

// A piece of a file's bytes (writeAtomic).
struct Part
{
    const void* data;
    size_t size;
};

// `parts` (in order) written beside `path` and renamed into place; false: not written (nothing left behind).
bool writeAtomic(const za::String& path, const Part* parts, int count);

// The product's setting: 0 off, 1 read and write, 2 made anyway and compared.
[[nodiscard]] int modeOf(Product p);

// Counts (any thread). `what`: a name for the miss's line (a model's), or null.
void noteHit(Product p, size_t bytes, double seconds);
void noteMiss(Product p, const char* what);
void noteWrite(Product p, size_t bytes);
void noteRejected(Product p); // a file there but amiss (made again and written over)
void noteChecked(Product p, bool same, const char* what);

// The main thread, each frame (VR_HostFrameEnd): whether a map is in play (signed on, not loading): misses from now on
// are "in play".
void setInPlay(bool inPlay);
[[nodiscard]] bool inPlay();

// The file of `key` (`<folder>/<key 16 hex>.<ext>`): 4 bytes of magic, the layout's version, two numbers of the
// caller's, then the data. read: its two numbers and its data, or false (none, another version, short); a hit or a
// miss is counted. write: on the pool (its folder made here), counted when written.
[[nodiscard]] bool read(Product p, const char* build, unsigned long long key, const char* ext, const char (&magic)[5],
    unsigned& a, unsigned& b, za::Vector<char>& data, const char* what = nullptr);
void write(Product p, const char* build, unsigned long long key, const char* ext, const char (&magic)[5], unsigned a,
    unsigned b, const void* data, size_t length);
// Whether that file is there (no read; the warm-up's and the loads' look-ahead).
[[nodiscard]] bool has(Product p, const char* build, unsigned long long key, const char* ext);

// A build folder's name from a product's version numbers (FNV-1a of them, 16 hex digits).
[[nodiscard]] za::String buildOf(const unsigned* parts, size_t count);

// vr_cache_info: each product's build, setting, this session's counts and its files on disk.
void info_f();

// FNV-1a, 64 bits (the keys' and the builds' hash): `h` 14695981039346656037 to start.
[[nodiscard]] inline unsigned long long fnv(unsigned long long h, const void* p, size_t n)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(p);
    for(size_t i = 0; i < n; i++)
    {
        h = (h ^ bytes[i]) * 1099511628211ull;
    }
    return h;
}

constexpr unsigned long long fnvStart = 14695981039346656037ull;

} // namespace qvr::diskcache
