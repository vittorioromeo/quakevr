// vr_ao_cache.cpp -- the AO bakes kept on disk (vr_ao_cache): see vr_ao.cpp.

#include "vr_ao_internal.hpp"
#include "vr_diskcache.hpp"
#include "vr_sha256.hpp"

#include <stdio.h>
#include <string.h>

using namespace qvr;
using namespace qvr::ao::detail;

// ----------------------------------------------------------------------------
// The bakes kept on disk (vr_ao_cache): `<cache root>/ao/v<BAKE_VERSION>/<key>.ao` (vr_diskcache.hpp), beside the normal
// maps' and the hulls' caches. <key> is the SHA-256 of everything a bake reads: its numbers (RAYS, the shares,
// the rays' directions), Quake's vertex normals, and the model's poses (with their normal indices), triangles, scale
// and origin; so a changed model, or changed numbers, never reads an old file. A file is read and checked (magic,
// version, sizes, the whole key, its bytes' FNV-1a) on the bake task's thread, never the main thread; anything amiss
// is baked again and written over. Written beside its place and renamed (another copy of the game never reads half a
// file). The bake task's first write in each game directory removes the other versions' folders, and the oldest files
// past CACHE_BUDGET.

namespace
{

constexpr char FILE_MAGIC[4] = {'Q', 'V', 'R', 'A'};

struct FileHeader
{
    char magic[4];
    za::U32 version;
    za::U32 numverts;
    za::U32 numposes;
    za::U8 key[32];
    za::U64 payloadBytes;
    za::U64 payloadHash; // FNV-1a (64-bit) of the payload: numposes x numverts bytes, the poses in turn
};
static_assert(sizeof(FileHeader) == 64);

[[nodiscard]] za::U64 fnv64(const unsigned char* b, size_t n)
{
    return diskcache::fnv(aoFnvStart, b, n);
}

// The version's folder (vr_diskcache.cpp's root, shared by every game folder).
[[nodiscard]] za::String buildName()
{
    char keep[16];
    snprintf(keep, sizeof(keep), "v%u", BAKE_VERSION);
    return keep;
}

} // namespace

namespace qvr::ao::detail
{

[[nodiscard]] sha256::Digest bakeKey(const BakeJob& job)
{
    za::Vector<unsigned char> bytes;
    const auto put = [&bytes](const void* data, size_t n) {
        const size_t at = bytes.size();
        bytes.resize(at + n);
        memcpy(bytes.data() + at, data, n);
    };
    const char tag[8] = {'q', 'v', 'r', 'a', 'o', 'k', 'e', 'y'};
    put(tag, sizeof(tag));
    const za::U32 counts[5] = {BAKE_VERSION, static_cast<za::U32>(RAYS), static_cast<za::U32>(job.numverts),
        static_cast<za::U32>(job.numposes), static_cast<za::U32>(job.tris.size())};
    put(counts, sizeof(counts));
    const float numbers[5] = {REACH_SHARE, REACH_MIN, REACH_MAX, NEAREST_SHARE, LIFT_SHARE};
    put(numbers, sizeof(numbers));
    put(rayDirs.data(), sizeof(glm::vec3) * RAYS);
    put(r_avertexnormals, sizeof(r_avertexnormals));
    put(&job.scale, sizeof(job.scale));
    put(&job.origin, sizeof(job.origin));
    put(job.verts.data(), job.verts.size() * sizeof(trivertx_t));
    put(job.tris.data(), job.tris.size() * sizeof(Tri));
    return sha256::of(bytes.data(), bytes.size());
}

[[nodiscard]] za::String versionDir()
{
    return diskcache::folderOf(diskcache::Product::AO, buildName().cStr());
}

[[nodiscard]] za::String fileFor(const sha256::Digest& key)
{
    char hex[65];
    sha256::toHex(key, hex);
    hex[32] = '\0'; // (128 bits name it; the whole key is checked in the file)
    return versionDir() + "/" + hex + ".ao";
}

// The model's file, checked whole: its bytes into `vis` (numposes x numverts).
[[nodiscard]] FileRead readBake(const za::String& path, const BakeJob& job, const sha256::Digest& key, za::Vector<unsigned char>& vis)
{
    FILE* in = Sys_fopen(path.cStr(), "rb");
    if(!in)
    {
        return FileRead::Missing;
    }
    const size_t count = static_cast<size_t>(job.numposes) * job.numverts;
    FileHeader h;
    bool ok = fread(&h, 1, sizeof(h), in) == sizeof(h) && memcmp(h.magic, FILE_MAGIC, 4) == 0 && h.version == BAKE_VERSION &&
              h.numverts == static_cast<za::U32>(job.numverts) && h.numposes == static_cast<za::U32>(job.numposes) &&
              memcmp(h.key, key.bytes, sizeof(h.key)) == 0 && h.payloadBytes == count;
    if(ok)
    {
        vis.resize(count);
        char extra;
        ok = fread(vis.data(), 1, count, in) == count && fread(&extra, 1, 1, in) == 0 && fnv64(vis.data(), count) == h.payloadHash;
    }
    fclose(in);
    return ok ? FileRead::Read : FileRead::Rejected;
}

[[nodiscard]] bool writeBake(const za::String& path, const BakeJob& job, const sha256::Digest& key)
{
    FileHeader h{};
    memcpy(h.magic, FILE_MAGIC, 4);
    h.version = BAKE_VERSION;
    h.numverts = static_cast<za::U32>(job.numverts);
    h.numposes = static_cast<za::U32>(job.numposes);
    memcpy(h.key, key.bytes, sizeof(h.key));
    h.payloadBytes = job.vis.size();
    h.payloadHash = fnv64(job.vis.data(), job.vis.size());
    const diskcache::Part parts[] = {{&h, sizeof(h)}, {job.vis.data(), job.vis.size()}};
    if(!diskcache::writeAtomic(path, parts, 2))
    {
        return false;
    }
    diskcache::noteWrite(diskcache::Product::AO, sizeof(h) + job.vis.size());
    return true;
}

// The version's folder made ready (once a session: the other versions' folders removed, then the oldest files past
// its budget, CACHE_BUDGET; vr_diskcache.cpp).
void pruneOnce()
{
    static_assert(CACHE_BUDGET == 64ull << 20, "vr_diskcache.cpp's products table keeps the AO's budget");
    (void)diskcache::prepare(diskcache::Product::AO, buildName().cStr());
}

} // namespace qvr::ao::detail
