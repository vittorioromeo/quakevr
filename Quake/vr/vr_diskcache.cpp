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
#include "vr_api.h"

#include "Zancle/Concurrency/Atomic.hpp"

namespace
{

using qvr::diskcache::Product;

constexpr unsigned version = 1; // the framed files' layout (read / write)

struct ProductInfo
{
    const char* kind;
    const char* what;
    unsigned long long budget; // bytes kept in its build's folder (0: no limit)
    cvar_t* mode; // (nullptr: always on, no setting)
};

const ProductInfo products[] = {
    {"normalmaps", "skins' normal maps", 0, &qvr::vr_normalmap_cache},
    {"gunpieces", "guns' convex pieces", 0, &qvr::vr_gun_pieces_cache},
    {"hulls", "compiled hull trees", 1024ull << 20, &qvr::vr_hull_cache},
    {"ao", "models' occlusion bakes", 64ull << 20, &qvr::vr_ao_cache},
    {"dequant", "dequantised poses", 256ull << 20, &qvr::vr_mdl_dequant_cache},
    {"rigs", "ragdoll rigs", 0, &qvr::vr_ragdoll_rig_cache},
    {"reliefmaps", "weapons' relief maps", 0, &qvr::vr_normalmap_cache},
    {"lightgrid", "big maps' model light grids", 256ull << 20, nullptr},
    {"navmesh", "monsters' navmeshes", 256ull << 20, &qvr::vr_nav_cache},
};
static_assert(sizeof(products) / sizeof(products[0]) == static_cast<size_t>(Product::Count));

const ProductInfo& infoOf(Product p)
{
    return products[static_cast<int>(p)];
}

struct Counts
{
    za::Atomic<long long> hits{0}, misses{0}, written{0}, rejected{0}, same{0}, differed{0}, missesInPlay{0};
    za::Atomic<long long> bytesRead{0}, bytesWritten{0}, readUs{0};
};
Counts counts[static_cast<int>(Product::Count)];

za::Atomic<bool> playing{false};

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
za::AtomicMutex prepareLocks[static_cast<int>(Product::Count)]; // held through a product's prune and folder making

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
// The program pack's write under way (progWrite): one at a time, each waiting for the last (two on the pool at once,
// the older renamed into place after the newer would win). Finished with the others (VR_TexCacheFinishWrites).
qvr::jobs::Future<void> progWriting;
za::Atomic<bool> progWriteFailed{false}; // (a worker's failure, said on the main thread: the console isn't a worker's)

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
    // One thread prunes and makes the product's folder; any other asking meanwhile (a write's job, the main thread)
    // waits for it: no file is written before its folder is made, none while the prune runs (it never removes one that
    // is being written).
    za::LockGuard folderGuard(prepareLocks[static_cast<int>(p)]);
    {
        za::LockGuard guard(preparedLock);
        if(za::find(prepared.begin(), prepared.end(), dir) != prepared.end())
        {
            return dir;
        }
    }
    prune(p, kindDir, build);
    {
        QVR_TRACY_SCOPE(tracyScope, "disk cache directories");
        files::createDirectories(dir.cStr());
    }
    za::LockGuard guard(preparedLock);
    prepared.pushBack(dir);
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
    const cvar_t* mode = infoOf(p).mode;
    if(!mode)
    {
        return 1;
    }
    const float v = mode->value;
    return v <= 0.f ? 0 : v >= 2.f ? 2 : 1;
}

void noteHit(Product p, size_t bytes, double seconds)
{
    Counts& c = counts[static_cast<int>(p)];
    c.hits.fetchAddSeqCst(1);
    c.bytesRead.fetchAddSeqCst(static_cast<long long>(bytes));
    c.readUs.fetchAddSeqCst(static_cast<long long>(seconds * 1e6));
}

void noteMiss(Product p, const char* what)
{
    Counts& c = counts[static_cast<int>(p)];
    c.misses.fetchAddSeqCst(1);
    if(playing.loadRelaxed())
    {
        c.missesInPlay.fetchAddSeqCst(1);
        if(developer.value)
        {
            Con_DPrintf("cache miss in play: %s %s (made now)\n", kindOf(p), what ? what : "");
        }
    }
}

void noteWrite(Product p, size_t bytes)
{
    Counts& c = counts[static_cast<int>(p)];
    c.written.fetchAddSeqCst(1);
    c.bytesWritten.fetchAddSeqCst(static_cast<long long>(bytes));
}

void noteRejected(Product p)
{
    counts[static_cast<int>(p)].rejected.fetchAddSeqCst(1);
}

void noteChecked(Product p, bool same, const char* what)
{
    Counts& c = counts[static_cast<int>(p)];
    (same ? c.same : c.differed).fetchAddSeqCst(1);
    if(!same)
    {
        Con_Warning("%s cache check: %s differs from its file (%lld of %lld so far)\n", kindOf(p), what ? what : "",
            c.differed.loadSeqCst(), c.same.loadSeqCst() + c.differed.loadSeqCst());
    }
}

void setInPlay(bool inPlay)
{
    playing.storeRelaxed(inPlay);
}

bool inPlay()
{
    return playing.loadRelaxed();
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

int readRgbaAt(const za::String& folder, unsigned long long key, unsigned char* rgba, int width, int height, double& seconds)
{
    const double t0 = Sys_DoubleTime();
    unsigned w = 0, h = 0;
    bool there = false;
    za::Vector<char> data;
    if(!readFile(folder + nameOf(key, "nrm"), normalMagic, w, h, data, there))
    {
        return there ? 3 : 2;
    }
    if(w != static_cast<unsigned>(width) || h != static_cast<unsigned>(height) ||
        data.size() != static_cast<size_t>(width) * height * 4)
    {
        return 3;
    }
    memcpy(rgba, data.data(), data.size());
    seconds = Sys_DoubleTime() - t0;
    return 1;
}

void noteRgbaRead(Product p, int status, size_t bytes, double seconds, const char* what)
{
    if(status == 1)
    {
        noteHit(p, bytes, seconds);
        return;
    }
    if(status == 3)
    {
        noteRejected(p);
    }
    noteMiss(p, what);
}

bool has(Product p, const char* build, unsigned long long key, const char* ext)
{
    return files::isFile(fileFor(p, build, key, ext).cStr());
}

void write(Product p, const char* build, unsigned long long key, const char* ext, const char (&magic)[5], unsigned a,
    unsigned b, const void* data, size_t length)
{
    QVR_TRACY_SCOPE(tracyScope, "disk cache write prepare");
    // (The folder made ready, its first time a session pruned too, in the job: off the main thread.)
    const za::String path = fileFor(p, build, key, ext);
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
        const double t0 = Sys_DoubleTime();
        writes.front().wait();
        VR_TimeAdd("disk cache: a write waited for (64 under way)", Sys_DoubleTime() - t0); // load timing (vr_startup_times)
        writes.erase(writes.begin());
    }
    QVR_TRACY_SCOPE(submitScope, "disk cache submit");
    writes.pushBack(jobs::async([p, path, buildName = za::String{build}, bytes = ZA_MOVE(bytes)] {
        QVR_TRACY_SCOPE(tracyScope, "disk cache write async");
        (void)prepare(p, buildName.cStr());
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
            info.kind, info.what, info.mode ? info.mode->name : "(always)", info.mode ? info.mode->value : 1.f, files, static_cast<double>(bytes) / 1048576.0,
            c.hits.loadSeqCst(), static_cast<double>(c.bytesRead.loadSeqCst()) / 1048576.0, static_cast<double>(c.readUs.loadSeqCst()) / 1000.0,
            c.misses.loadSeqCst(), c.missesInPlay.loadSeqCst(), c.written.loadSeqCst(), static_cast<double>(c.bytesWritten.loadSeqCst()) / 1048576.0,
            c.rejected.loadSeqCst(), c.same.loadSeqCst(), c.differed.loadSeqCst());
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

namespace
{

[[nodiscard]] int rgbaCacheLoad(Product p, const char* build, unsigned long long key, unsigned char* rgba, int width,
    int height, const char* what)
{
    unsigned w = 0, h = 0;
    za::Vector<char> data;
    if(!qvr::diskcache::read(p, build, key, "nrm", normalMagic, w, h, data, what))
    {
        return 0;
    }
    if(w != static_cast<unsigned>(width) || h != static_cast<unsigned>(height) ||
        data.size() != static_cast<size_t>(width) * height * 4)
    {
        qvr::diskcache::noteRejected(p);
        return 0;
    }
    memcpy(rgba, data.data(), data.size());
    return 1;
}

void rgbaCacheStore(Product p, const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    qvr::diskcache::write(p, build, key, "nrm", normalMagic, static_cast<unsigned>(width), static_cast<unsigned>(height),
        rgba, static_cast<size_t>(width) * height * 4);
}

} // namespace

extern "C" int VR_NormalCacheLoad(const char* build, unsigned long long key, unsigned char* rgba, int width, int height)
{
    return rgbaCacheLoad(Product::NormalMaps, build, key, rgba, width, height, "(a skin)");
}

extern "C" void VR_NormalCacheStore(const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    rgbaCacheStore(Product::NormalMaps, build, key, rgba, width, height);
}

extern "C" int VR_ReliefCacheLoad(const char* build, unsigned long long key, unsigned char* rgba, int width, int height)
{
    return rgbaCacheLoad(Product::ReliefMaps, build, key, rgba, width, height, "(a weapon's relief)");
}

extern "C" void VR_ReliefCacheStore(const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    rgbaCacheStore(Product::ReliefMaps, build, key, rgba, width, height);
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
    progWriting.wait(); // (the program pack's)
}

extern "C" void VR_NormalCacheChecked(int same, const char* name)
{
    qvr::diskcache::noteChecked(Product::NormalMaps, same != 0, name);
}

// The GL programs' binaries (vr_api.h VR_ProgCache*; gl_shaders.c; QUEST_PLAN.md, C4d): the Quest's Adreno compiles the
// start's ~120 programs in ~51 s at every start (its EGL blob cache keeps nothing: no shaders_cache file in the app's
// cache folder); the driver gives each one back (glProgramBinary) from one pack file per driver identity (vendor,
// renderer, version: another driver, or an update of it, never reads it), read once a session. A program's key hashes
// the very text the driver compiles (VR_GLShaderHash: the ES rewrite and its switches included). Pack: "QVPB", its
// layout's version, the count, the session's number (version 2), then each program's key (8 bytes), format, length,
// the last session that used it (version 2) and binary. Main thread only (GL).
// Several shader sets live side by side (2026-11-17, his adb session: safe mode on and off, vr_shadow_dlights, MSAA
// each a set of ~120 programs, 45-75 s to compile on the Quest 3): a program is dropped only when no session used it
// for progKeepSessions sessions, or the oldest first past progMaxBytes (a version 1 pack is read: its programs as
// last used by the session before).
// (From harness4's stash: on the PC it saved nothing, 273 vs 296 ms, so it is off there by default.)
namespace
{

constexpr char progMagic[4] = {'Q', 'V', 'P', 'B'};
constexpr unsigned progVersion = 2;
constexpr unsigned progKeepSessions = 24;                 // a set unused this many sessions goes
constexpr size_t progMaxBytes = size_t{128} * 1024 * 1024; // the pack's binaries at most (the oldest go first)

struct ProgEntry
{
    unsigned long long key;
    unsigned format;
    za::Vector<char> data;
    bool used;        // asked for, or made, this session
    unsigned session; // the last session that used it
};

struct ProgCache
{
    za::Vector<ProgEntry> entries;
    unsigned long long driver = 0; // the pack's driver identity (its file's name)
    unsigned session = 1;          // this session's number (the pack's, plus one)
    bool loaded = false;
    bool batch = false; // inside GL_CreateShaders: written at its end
    bool dirty = false;
    const char* loadNote = "";  // the pack's read, for the start-up trace (VR_StepNote)
    int hits = 0, misses = 0;   // this batch's programs given back / not in the pack (VR_ProgCacheEnd's note)
};
ProgCache progCache;

za::String progPath(unsigned long long driver)
{
    char name[48];
    snprintf(name, sizeof(name), "/programs/%016llx.bin", driver);
    return rootDir() + name;
}

void progLoad()
{
    progCache.entries.clear();
    progCache.session = 1;
    za::Vector<char> bytes;
    if(!qvr::files::readBytes(progPath(progCache.driver).cStr(), bytes))
    {
        progCache.loadNote = "no pack for this driver yet (the first start, a new driver or a cleared cache)";
        return;
    }
    if(bytes.size() < 12 || memcmp(bytes.data(), progMagic, 4) != 0)
    {
        progCache.loadNote = "the pack is not one (cut short or damaged): every program made again";
        return;
    }
    unsigned header[2];
    memcpy(header, bytes.data() + 4, sizeof(header));
    if(header[0] != progVersion && header[0] != 1)
    {
        progCache.loadNote = "the pack is an older layout: every program made again";
        return;
    }
    const bool stamped = header[0] >= 2; // version 2: the session's number, and each program's last session
    unsigned packSession = 0;
    size_t at = 12;
    if(stamped)
    {
        if(bytes.size() < 16)
        {
            progCache.loadNote = "the pack is not one (cut short or damaged): every program made again";
            return;
        }
        memcpy(&packSession, bytes.data() + 12, 4);
        at = 16;
    }
    progCache.session = packSession + 1;
    progCache.loadNote = stamped ? "read" : "read (version 1: its programs kept as the last session's)";
    const size_t entryHead = stamped ? 20 : 16;
    for(unsigned i = 0; i < header[1]; i++)
    {
        ProgEntry e{};
        unsigned length = 0;
        if(at + entryHead > bytes.size())
        {
            break;
        }
        memcpy(&e.key, bytes.data() + at, 8);
        memcpy(&e.format, bytes.data() + at + 8, 4);
        memcpy(&length, bytes.data() + at + 12, 4);
        e.session = packSession;
        if(stamped)
        {
            memcpy(&e.session, bytes.data() + at + 16, 4);
        }
        at += entryHead;
        if(length > bytes.size() - at)
        {
            break;
        }
        e.data.resize(length);
        memcpy(e.data.data(), bytes.data() + at, length);
        at += length;
        progCache.entries.pushBack(static_cast<ProgEntry&&>(e));
    }
}

// The programs kept: those used in the last progKeepSessions sessions (each shader set a session used: safe mode on
// and off, the lite forms, MSAA), then the most recently used first up to progMaxBytes. Returns how many went.
int progPrune()
{
    const unsigned now = progCache.session;
    za::Vector<ProgEntry> kept;
    int dropped = 0;
    for(ProgEntry& e : progCache.entries)
    {
        if(e.used || now - e.session < progKeepSessions)
        {
            kept.pushBack(static_cast<ProgEntry&&>(e));
        }
        else
        {
            dropped++;
        }
    }
    size_t total = 0;
    for(const ProgEntry& e : kept)
    {
        total += e.data.size();
    }
    while(total > progMaxBytes)
    {
        size_t oldest = kept.size();
        for(size_t i = 0; i < kept.size(); i++)
        {
            if(!kept[i].used && (oldest == kept.size() || kept[i].session < kept[oldest].session))
            {
                oldest = i;
            }
        }
        if(oldest == kept.size())
        {
            break; // (this session's own: all kept)
        }
        total -= kept[oldest].data.size();
        kept.erase(kept.begin() + oldest);
        dropped++;
    }
    progCache.entries = static_cast<za::Vector<ProgEntry>&&>(kept);
    return dropped;
}

void progWrite()
{
    progPrune();
    za::Vector<char> bytes;
    const unsigned header[3] = {progVersion, static_cast<unsigned>(progCache.entries.size()), progCache.session};
    bytes.resize(16);
    memcpy(bytes.data(), progMagic, 4);
    memcpy(bytes.data() + 4, header, sizeof(header));
    for(const ProgEntry& e : progCache.entries)
    {
        const size_t at = bytes.size();
        const unsigned length = static_cast<unsigned>(e.data.size());
        const unsigned session = e.used ? progCache.session : e.session;
        bytes.resize(at + 20 + length);
        memcpy(bytes.data() + at, &e.key, 8);
        memcpy(bytes.data() + at + 8, &e.format, 4);
        memcpy(bytes.data() + at + 12, &length, 4);
        memcpy(bytes.data() + at + 16, &session, 4);
        if(length)
        {
            memcpy(bytes.data() + at + 20, e.data.data(), length);
        }
    }
    // The file written on the game's pool (the pack is up to 128 MB: a program made in play, or a queued set's
    // every-10-s save, waited for it on the main thread); its bytes made here, from the entries the main thread changes.
    const za::String dir = rootDir() + "/programs";
    const za::String path = progPath(progCache.driver);
    if(progWriteFailed.exchangeSeqCst(false))
    {
        Con_DPrintf("vr_program_cache: %s not written (the last time)\n", path.cStr());
    }
    progWriting.wait(); // (the last pack's write done first)
    progWriting = qvr::jobs::async([dir, path, bytes = ZA_MOVE(bytes)] {
        QVR_TRACY_SCOPE(tracyScope, "program pack write async");
        qvr::files::createDirectories(dir.cStr()); // (thread-safe: the map installer's workers call it)
        const qvr::diskcache::Part part{bytes.data(), bytes.size()};
        if(!qvr::diskcache::writeAtomic(path, &part, 1))
        {
            progWriteFailed.storeSeqCst(true);
        }
    });
    progCache.dirty = false;
}

ProgEntry* progFind(unsigned long long key)
{
    for(ProgEntry& e : progCache.entries)
    {
        if(e.key == key)
        {
            return &e;
        }
    }
    return nullptr;
}

} // namespace

extern "C" int VR_ProgCacheOn(void)
{
#if defined(__ANDROID__)
    return qvr::vr_program_cache.value >= 1.f;
#else
    return qvr::vr_program_cache.value >= 2.f;
#endif
}

extern "C" unsigned long long VR_ProgCacheKey(unsigned long long h, const char* text)
{
    const size_t n = text ? strlen(text) : 0;
    h = qvr::diskcache::wideHash(h ? h : qvr::diskcache::fnvStart, &n, sizeof(n)); // (each part's length first: no run-ons)
    return qvr::diskcache::wideHash(h, text ? text : "", n);
}

extern "C" void VR_ProgCacheBegin(const char* vendor, const char* renderer, const char* version)
{
    unsigned long long driver = VR_ProgCacheKey(0, vendor);
    driver = VR_ProgCacheKey(driver, renderer);
    driver = VR_ProgCacheKey(driver, version);
    if(!progCache.loaded || driver != progCache.driver)
    {
        progCache.driver = driver;
        progCache.loaded = true;
        const double t0 = Sys_DoubleTime();
        progLoad();
        VR_TimeAdd(progCache.entries.empty() ? "  the program cache's pack: none" : "  the program cache's pack read",
            Sys_DoubleTime() - t0); // load timing (vr_startup_times)
        Con_DPrintf("vr_program_cache: %d programs in %s\n", static_cast<int>(progCache.entries.size()),
            progPath(driver).cStr());
        VR_StepNote("program cache: %s (%d programs, %s)", progCache.loadNote, static_cast<int>(progCache.entries.size()),
            progPath(driver).cStr());
    }
    progCache.batch = true;
    progCache.hits = progCache.misses = 0;
}

extern "C" const void* VR_ProgCacheFind(unsigned long long key, unsigned* format, int* length)
{
    ProgEntry* e = progCache.loaded ? progFind(key) : nullptr;
    if(!e || e->data.empty())
    {
        progCache.misses++;
        return nullptr;
    }
    progCache.hits++;
    e->used = true;
    *format = e->format;
    *length = static_cast<int>(e->data.size());
    return e->data.data();
}

extern "C" void VR_ProgCacheReject(unsigned long long key)
{
    for(size_t i = 0; i < progCache.entries.size(); i++)
    {
        if(progCache.entries[i].key == key)
        {
            progCache.entries.erase(progCache.entries.begin() + i);
            progCache.dirty = true;
            return;
        }
    }
}

extern "C" void VR_ProgCacheAdd(unsigned long long key, unsigned format, const void* data, int length)
{
    if(!progCache.loaded || length <= 0)
    {
        return;
    }
    ProgEntry* e = progFind(key);
    if(!e)
    {
        progCache.entries.pushBack(ProgEntry{key, 0, {}, true, progCache.session});
        e = &progCache.entries.back();
    }
    e->format = format;
    e->used = true;
    e->data.resize(static_cast<size_t>(length));
    memcpy(e->data.data(), data, static_cast<size_t>(length));
    progCache.dirty = true;
    if(!progCache.batch)
    {
        progWrite(); // (a program made in play: rare, the decals' retro ones)
    }
}

extern "C" void VR_ProgCacheEnd(void)
{
    progCache.batch = false;
    if(progCache.misses)
    {
        // (a pack read but programs missing from it: their text changed since it was written, a new build's shaders or
        // switches: those are compiled from their sources, the slow start, and added)
        VR_StepNote("program cache: %d from their binaries, %d compiled (%s)", progCache.hits, progCache.misses,
            progCache.hits ? "not in the pack: their shaders changed since (a new build) or are new"
                           : "none in the pack matched: a new build's shaders, or no pack");
    }
    else
    {
        VR_StepNote("program cache: all %d from their binaries", progCache.hits);
    }
    if(progCache.dirty)
    {
        progWrite();
    }
}

// A queued set's programs (gl_shaders.c GL_ShadersPump): those done so far written every few seconds while it is
// pending, so a start stopped mid-set keeps them.
extern "C" void VR_ProgCacheFlush(void)
{
    if(progCache.dirty)
    {
        progWrite();
    }
}

extern "C" void VR_ProgCacheCounts(int* hits, int* misses)
{
    *hits = progCache.hits;
    *misses = progCache.misses;
}
