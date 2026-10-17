// vr_diskcache.cpp -- the derived-data cache (vr_diskcache.hpp, docs/vr-port/CACHES.md). Until 2026-10-16 each
// product kept its own folders under each game folder (`<gamedir>/cache/<kind>`) with its own pruning and writes; one
// root now, one way to make a folder, prune it, write a file and count.

#include "vr_diskcache.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"
#include "vr_tracy.h"
#include "vr_warmup.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <atomic>

namespace
{

using qvr::diskcache::Product;

constexpr unsigned version = 1; // the framed files' layout (read / write)

struct ProductInfo
{
    const char* kind;
    const char* what;
    unsigned long long budget; // bytes kept in its build's folder (0: no limit)
    cvar_t* mode;
};

const ProductInfo products[] = {
    {"normalmaps", "skins' normal maps", 0, &qvr::vr_normalmap_cache},
    {"gunpieces", "guns' convex pieces", 0, &qvr::vr_gun_pieces_cache},
    {"hulls", "compiled hull trees", 1024ull << 20, &qvr::vr_hull_cache},
    {"ao", "models' occlusion bakes", 64ull << 20, &qvr::vr_ao_cache},
    {"dequant", "dequantised poses", 256ull << 20, &qvr::vr_mdl_dequant_cache},
    {"rigs", "ragdoll rigs", 0, &qvr::vr_ragdoll_rig_cache},
};
static_assert(sizeof(products) / sizeof(products[0]) == static_cast<size_t>(Product::Count));

const ProductInfo& infoOf(Product p)
{
    return products[static_cast<int>(p)];
}

struct Counts
{
    std::atomic<long long> hits{0}, misses{0}, written{0}, rejected{0}, same{0}, differed{0}, missesInPlay{0};
    std::atomic<long long> bytesRead{0}, bytesWritten{0}, readUs{0};
};
Counts counts[static_cast<int>(Product::Count)];

std::atomic<bool> playing{false};

// Models loaded from their files while a map is in play (VR_NoteModelLoad: the main thread's): a first use the map's
// load didn't precache, its load the frame's hitch. vr_cache_info counts them; developer 1 prints each.
struct ModelLoadsInPlay
{
    long long count{0};
    double ms{0.0};
    double worstMs{0.0};
};
ModelLoadsInPlay modelLoadsInPlay;

// The folders made ready this session (prepare), by their whole path: any thread.
za::AtomicMutex preparedLock;
za::Vector<za::String> prepared;

za::String rootDir()
{
    if(qvr::vr_cache_dir.string && qvr::vr_cache_dir.string[0])
    {
        return za::String{qvr::vr_cache_dir.string};
    }
    return qvr::files::baseCacheDir();
}

// The product's other builds' folders (and stray files) removed, then its oldest files (by their last write) until its
// build's folder is within its budget.
void prune(Product p, const za::String& kindDir, const char* build)
{
    QVR_TRACY_SCOPE(tracyScope, "disk cache prune");
    za::Vector<za::String> others;
    qvr::files::forEachEntry(kindDir.cStr(), [&](const char* name, bool) {
        if(strcmp(name, build) != 0)
        {
            others.pushBack(kindDir + "/" + name);
        }
    });
    for(const za::String& other : others)
    {
        qvr::files::removeAll(other.cStr());
    }
    const unsigned long long budget = infoOf(p).budget;
    if(!budget)
    {
        return;
    }
    struct Entry
    {
        za::String path;
        za::I64 time;
        za::U64 bytes;
    };
    za::Vector<Entry> entries;
    za::U64 total = 0;
    const za::String dir = kindDir + "/" + build;
    qvr::files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory)
        {
            Entry e{dir + "/" + name, 0, 0};
            e.time = qvr::files::lastWriteTime(e.path.cStr());
            e.bytes = qvr::files::fileSize(e.path.cStr());
            total += e.bytes;
            entries.pushBack(ZA_MOVE(e));
        }
    });
    if(total <= budget)
    {
        return;
    }
    za::quickSort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.time < b.time; });
    for(const Entry& e : entries)
    {
        if(total <= budget * 3 / 4)
        {
            break;
        }
        if(qvr::files::remove(e.path.cStr()))
        {
            total -= e.bytes;
        }
    }
}

// A key's file in its build's folder: `/<key 16 hex>.<ext>`.
za::String nameOf(unsigned long long key, const char* ext)
{
    char name[40];
    snprintf(name, sizeof(name), "/%016llx.%s", key, ext);
    return name;
}

za::String fileFor(Product p, const char* build, unsigned long long key, const char* ext)
{
    return qvr::diskcache::folderOf(p, build) + nameOf(key, ext);
}

// A framed file: 4 bytes of magic, the version, two numbers of the caller's, then the data.
bool readFile(const za::String& path, const char (&magic)[5], unsigned& a, unsigned& b, za::Vector<char>& data, bool& there)
{
    QVR_TRACY_SCOPE(tracyScope, "disk cache read");
    FILE* in = Sys_fopen(path.cStr(), "rb");
    there = in != nullptr;
    if(!in)
    {
        return false;
    }
    Sys_fseek(in, 0, SEEK_END);
    const qfileofs_t size = Sys_ftell(in);
    Sys_fseek(in, 0, SEEK_SET);
    constexpr qfileofs_t headerSize = 4 + 3 * sizeof(unsigned);
    char m[4];
    unsigned header[3];
    bool ok = size >= headerSize && fread(m, 1, sizeof(m), in) == sizeof(m) &&
              fread(header, 1, sizeof(header), in) == sizeof(header) && memcmp(m, magic, 4) == 0 && header[0] == version;
    if(ok)
    {
        a = header[1];
        b = header[2];
        data.resize(static_cast<size_t>(size - headerSize));
        ok = fread(data.data(), 1, data.size(), in) == data.size();
    }
    fclose(in);
    return ok;
}

// The files being written on the game's pool (write; the main thread's list): finished at the latest by
// VR_TexCacheFinishWrites (the game's shutdown), the oldest waited for when too many are under way (each holds its
// bytes). Until a file is renamed into place it is not there: the same key asked for meanwhile is made again (the same
// bytes), as before its first write.
za::Vector<qvr::jobs::Future<void>> writes;
constexpr za::SizeT writesMost = 64;

constexpr char normalMagic[5] = "QVRN";

} // namespace

namespace qvr::diskcache
{

za::String root()
{
    return rootDir();
}

const char* kindOf(Product p)
{
    return infoOf(p).kind;
}

za::String folderOf(Product p, const char* build)
{
    return rootDir() + "/" + kindOf(p) + "/" + build;
}

za::String prepare(Product p, const char* build)
{
    const za::String kindDir = rootDir() + "/" + kindOf(p);
    za::String dir = kindDir + "/" + build;
    {
        za::LockGuard guard(preparedLock);
        if(za::find(prepared.begin(), prepared.end(), dir) != prepared.end())
        {
            return dir;
        }
        prepared.pushBack(dir);
    }
    // (Two threads preparing the same folder at once: both prune and make it, the same result.)
    prune(p, kindDir, build);
    QVR_TRACY_SCOPE(tracyScope, "disk cache directories");
    files::createDirectories(dir.cStr());
    return dir;
}

bool writeAtomic(const za::String& path, const Part* parts, int count)
{
    char suffix[48];
    snprintf(suffix, sizeof(suffix), ".%llx.%x.tmp", static_cast<unsigned long long>(Sys_DoubleTime() * 1e6),
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(&suffix) >> 4));
    const za::String tmp = path + suffix;
    FILE* out = Sys_fopen(tmp.cStr(), "wb");
    if(!out)
    {
        return false;
    }
    bool ok = true;
    for(int i = 0; i < count && ok; i++)
    {
        ok = !parts[i].size || fwrite(parts[i].data, 1, parts[i].size, out) == parts[i].size;
    }
    ok = fclose(out) == 0 && ok;
    if(!ok || !files::rename(tmp.cStr(), path.cStr()))
    {
        files::remove(tmp.cStr());
        return false;
    }
    return true;
}

int modeOf(Product p)
{
    const float v = infoOf(p).mode->value;
    return v <= 0.f ? 0 : v >= 2.f ? 2 : 1;
}

void noteHit(Product p, size_t bytes, double seconds)
{
    Counts& c = counts[static_cast<int>(p)];
    ++c.hits;
    c.bytesRead += static_cast<long long>(bytes);
    c.readUs += static_cast<long long>(seconds * 1e6);
}

void noteMiss(Product p, const char* what)
{
    Counts& c = counts[static_cast<int>(p)];
    ++c.misses;
    if(playing.load(std::memory_order_relaxed))
    {
        ++c.missesInPlay;
        if(developer.value)
        {
            Con_DPrintf("cache miss in play: %s %s (made now)\n", kindOf(p), what ? what : "");
        }
    }
}

void noteWrite(Product p, size_t bytes)
{
    Counts& c = counts[static_cast<int>(p)];
    ++c.written;
    c.bytesWritten += static_cast<long long>(bytes);
}

void noteRejected(Product p)
{
    ++counts[static_cast<int>(p)].rejected;
}

void noteChecked(Product p, bool same, const char* what)
{
    Counts& c = counts[static_cast<int>(p)];
    ++(same ? c.same : c.differed);
    if(!same)
    {
        Con_Warning("%s cache check: %s differs from its file (%lld of %lld so far)\n", kindOf(p), what ? what : "",
            c.differed.load(), c.same.load() + c.differed.load());
    }
}

void setInPlay(bool inPlay)
{
    playing.store(inPlay, std::memory_order_relaxed);
}

bool inPlay()
{
    return playing.load(std::memory_order_relaxed);
}

bool read(Product p, const char* build, unsigned long long key, const char* ext, const char (&magic)[5], unsigned& a,
    unsigned& b, za::Vector<char>& data, const char* what)
{
    const double t0 = Sys_DoubleTime();
    bool there = false;
    if(!readFile(fileFor(p, build, key, ext), magic, a, b, data, there))
    {
        if(there)
        {
            noteRejected(p);
        }
        noteMiss(p, what);
        return false;
    }
    noteHit(p, data.size(), Sys_DoubleTime() - t0);
    return true;
}

bool has(Product p, const char* build, unsigned long long key, const char* ext)
{
    return files::isFile(fileFor(p, build, key, ext).cStr());
}

void write(Product p, const char* build, unsigned long long key, const char* ext, const char (&magic)[5], unsigned a,
    unsigned b, const void* data, size_t length)
{
    QVR_TRACY_SCOPE(tracyScope, "disk cache write prepare");
    const za::String path = prepare(p, build) + nameOf(key, ext); // (fileFor's, the folder made ready)
    za::Vector<unsigned char> bytes;
    const unsigned header[3] = {version, a, b};
    bytes.resize(4 + sizeof(header) + length);
    memcpy(bytes.data(), magic, 4);
    memcpy(bytes.data() + 4, header, sizeof(header));
    if(length)
    {
        memcpy(bytes.data() + 4 + sizeof(header), data, length);
    }
    while(!writes.empty() && writes.front().ready())
    {
        writes.erase(writes.begin());
    }
    if(writes.size() >= writesMost)
    {
        QVR_TRACY_SCOPE(tracyScope, "disk cache oldest wait");
        writes.front().wait();
        writes.erase(writes.begin());
    }
    QVR_TRACY_SCOPE(submitScope, "disk cache submit");
    writes.pushBack(jobs::async([p, path, bytes = ZA_MOVE(bytes)] {
        QVR_TRACY_SCOPE(tracyScope, "disk cache write async");
        const Part part{bytes.data(), bytes.size()};
        if(writeAtomic(path, &part, 1))
        {
            noteWrite(p, bytes.size());
        }
    }));
}

za::String buildOf(const unsigned* parts, size_t count)
{
    char text[24];
    snprintf(text, sizeof(text), "%016llx", fnv(fnvStart, parts, count * sizeof(unsigned)));
    return text;
}

void info_f()
{
    Con_Printf("vr_cache_info: the derived-data cache, %s (vr_cache_dir)\n", rootDir().cStr());
    long long allFiles = 0, allBytes = 0;
    for(int i = 0; i < static_cast<int>(Product::Count); i++)
    {
        const Product p = static_cast<Product>(i);
        const ProductInfo& info = infoOf(p);
        const Counts& c = counts[i];
        long long files = 0, bytes = 0;
        const za::String kindDir = rootDir() + "/" + info.kind;
        files::forEachEntry(kindDir.cStr(), [&](const char* name, bool isDirectory) {
            if(!isDirectory)
            {
                return;
            }
            const za::String dir = kindDir + "/" + name;
            files::forEachEntry(dir.cStr(), [&](const char* file, bool sub) {
                if(!sub)
                {
                    files++;
                    bytes += static_cast<long long>(files::fileSize((dir + "/" + file).cStr()));
                }
            });
        });
        allFiles += files;
        allBytes += bytes;
        Con_Printf("  %-10s %-24s %s %g: %lld files, %.1f MB on disk; this session %lld read (%.1f MB, %.1f ms), %lld "
                   "missed (%lld in play), %lld written (%.1f MB), %lld rejected, checked %lld same %lld differed\n",
            info.kind, info.what, info.mode->name, info.mode->value, files, static_cast<double>(bytes) / 1048576.0,
            c.hits.load(), static_cast<double>(c.bytesRead.load()) / 1048576.0, static_cast<double>(c.readUs.load()) / 1000.0,
            c.misses.load(), c.missesInPlay.load(), c.written.load(), static_cast<double>(c.bytesWritten.load()) / 1048576.0,
            c.rejected.load(), c.same.load(), c.differed.load());
    }
    Con_Printf("  all: %lld files, %.1f MB\n", allFiles, static_cast<double>(allBytes) / 1048576.0);
    Con_Printf("  models loaded in play: %lld (%.1f ms, the worst %.1f ms)\n", modelLoadsInPlay.count,
        modelLoadsInPlay.ms, modelLoadsInPlay.worstMs);
    warmup::printProgress();
}

} // namespace qvr::diskcache

using qvr::diskcache::Product;

extern "C" int VR_NormalCacheMode(void)
{
    return qvr::diskcache::modeOf(Product::NormalMaps);
}

extern "C" int VR_NormalCacheLoad(const char* build, unsigned long long key, unsigned char* rgba, int width, int height)
{
    unsigned w = 0, h = 0;
    za::Vector<char> data;
    if(!qvr::diskcache::read(Product::NormalMaps, build, key, "nrm", normalMagic, w, h, data, "(a skin)"))
    {
        return 0;
    }
    if(w != static_cast<unsigned>(width) || h != static_cast<unsigned>(height) ||
        data.size() != static_cast<size_t>(width) * height * 4)
    {
        qvr::diskcache::noteRejected(Product::NormalMaps);
        return 0;
    }
    memcpy(rgba, data.data(), data.size());
    return 1;
}

extern "C" void VR_NormalCacheStore(const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    qvr::diskcache::write(Product::NormalMaps, build, key, "nrm", normalMagic, static_cast<unsigned>(width),
        static_cast<unsigned>(height), rgba, static_cast<size_t>(width) * height * 4);
}

extern "C" void VR_NoteModelLoad(const char* name, double seconds)
{
    if(!qvr::diskcache::inPlay())
    {
        return;
    }
    const double ms = seconds * 1000.0;
    ++modelLoadsInPlay.count;
    modelLoadsInPlay.ms += ms;
    modelLoadsInPlay.worstMs = ms > modelLoadsInPlay.worstMs ? ms : modelLoadsInPlay.worstMs;
    Con_DPrintf("model loaded in play: %s, %.1f ms\n", name, ms);
}

// The files still being written finished (VR_Shutdown, before the pool goes).
extern "C" void VR_TexCacheFinishWrites(void)
{
    writes.clear(); // (each Future waits for its write)
}

extern "C" void VR_NormalCacheChecked(int same, const char* name)
{
    qvr::diskcache::noteChecked(Product::NormalMaps, same != 0, name);
}
