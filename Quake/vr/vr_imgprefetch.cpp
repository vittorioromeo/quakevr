#include "vr_alloccount.h"
// vr_imgprefetch.cpp -- the images the start-up and the first map load decode (the models' authored normal maps, the
// body's and the hands' skins: 40 PNGs of up to 1024 x 1024, 0.14 s on one thread), decoded ahead on worker threads
// while the window and the GL context are made (0.3 s the main thread spends waiting on the driver).
//
// Which ones: the list the last session decoded (`<gamedir>/cache/prefetch.txt`, written at the end of the first map
// load when it changed). At VR_Init (the file system is up) the main thread reads each file as the game's lookup finds
// it (COM_LoadMallocFile) and the workers decode the bytes. Image_LoadImage (image.c) then takes an image decoded
// ahead only if the file it opens holds exactly the same bytes (compared); else it decodes as before. Images not taken
// by the end of the first load are freed there; the decoding tasks (the game's thread pool, vr_jobs.hpp) are finished
// then, or at shutdown. An image asked for before a task has started on it is decoded by the thread that asks.
//
// A map load's textures (vr_texture_prefetch; VRTRACE_20261016, ROUND22.md): Mod_LoadTextures first finds each
// texture's replacement image, its glow and the external pack's maps as its loop will (VR_TexturePrefetchBegin), reads
// the png, tga and jpg files not decoded before (vr_imgcache) and has up to 8 tasks decode them, at most aheadBudget of
// pixels waiting; the loop's Image_LoadImage takes each as it asks for it (the file it opens the same one: identity,
// offset, length), so the uploads run in their order on the main thread while the workers decode.

#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_imgprefetch.hpp"
#include "vr_jobs.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "vr_cvars.hpp"
#include "vr_extmaps.hpp"

#include <atomic>
#include <initializer_list>

extern "C" unsigned char* Image_DecodeMemory(const unsigned char* bytes, int length, int* width, int* height); // image.c

namespace
{

struct Item
{
    za::String name; // as looked up: progs/vrbody_00_00_norm.png
    za::Vector<unsigned char> bytes;
    unsigned char* pixels = nullptr; // malloc'd by stb_image (the caller frees it, as stbi's own)
    int width = 0, height = 0;
    bool claimed = false;     // a task (or the asking thread) is decoding it
    za::Atomic<za::U32> done; // 1: decoded (set after the rest, under the lock; waited on by the thread that asks)
    bool taken = false;
    // A map load's (VR_TexturePrefetchBegin): taken if the file Image_LoadImage opens is this one (its identity, where
    // the image starts in it, its length: vr_imgcache's key), its bytes dropped once decoded.
    bool batch = false;
    qvr::files::Identity file;
    long offset = 0;
    int length = 0;
};

za::AtomicMutex lock;
za::Vector<za::UniquePtr<Item>> items;
za::Vector<qvr::jobs::Future<void>> tasks; // each decodes items until none is left
size_t next = 0; // the items before it are all claimed
bool active = false;

// The map load's textures (VR_TexturePrefetchBegin to VR_TexturePrefetchEnd, main thread but the counts).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::U64 operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};
constexpr long long aheadBudget = 256ll << 20; // decoded pixels not yet taken, at most (the tasks stop past it)
ankerl::unordered_dense::map<za::String, Item*, NameHash, NameEqual> batchByName;
// Images read ahead and not asked for (the pack's maps of a texture its picture doesn't match): not read again this
// session (a load that asks decodes it itself).
ankerl::unordered_dense::set<za::String, NameHash, NameEqual> unasked;
bool batchOpen = false;
int batchTasks = 0;                    // the tasks wanted (the pool's workers, up to 8)
std::atomic<long long> aheadBytes{0};  // the batch's pixels decoded and not taken
std::atomic<int> liveTasks{0};
struct BatchCounts
{
    int loads = 0, read = 0, taken = 0, asked = 0, unused = 0, cached = 0;
    double mainMs = 0.0; // finding and reading the files (Begin)
};
BatchCounts batchSession, batchLast, batchBiggest; // (vr_image_prefetch_info; the biggest set since it last printed)

za::Vector<za::String> manifest; // the list read at the start
za::Vector<za::String> decoded;  // this window's decodes (found png/tga/jpg files, over a millisecond)
bool windowOpen = false;
int takenCount = 0;  // decoded ahead and taken (developer 1: printed at the end)
int askedCount = 0;  // ... of them decoded by the thread that asked (no task had come to them yet)

za::String manifestPath()
{
    return za::String{com_gamedir} + "/cache/prefetch.txt";
}

// Decodes a claimed item (outside the lock).
void decode(Item* it)
{
    int w = 0, h = 0;
    unsigned char* px = Image_DecodeMemory(it->bytes.data(), static_cast<int>(it->bytes.size()), &w, &h);
    if(it->batch)
    {
        it->bytes = {}; // (matched by its file)
        if(px)
        {
            aheadBytes += static_cast<long long>(w) * h * 4;
        }
    }
    {
        za::LockGuard g(lock);
        it->pixels = px;
        it->width = w;
        it->height = h;
    }
    it->done.storeRelease(1u);
    it->done.notifyAll();
}

void work()
{
    for(;;)
    {
        Item* it = nullptr;
        {
            za::LockGuard g(lock);
            while(next < items.size() && items[next]->claimed)
            {
                next++;
            }
            if(next >= items.size() || (items[next]->batch && aheadBytes.load() >= aheadBudget))
            {
                return; // (past the budget: the asking thread starts tasks again as it takes them)
            }
            it = items[next++].get();
            it->claimed = true;
        }
        decode(it);
    }
}

// Every task finished (one not started yet runs here).
void finishTasks()
{
    for(qvr::jobs::Future<void>& t : tasks)
    {
        t.wait();
    }
    tasks.clear();
}

void freeItems()
{
    za::LockGuard g(lock);
    for(auto& it : items)
    {
        if(!it->taken && it->pixels)
        {
            VR_HeapFree(it->pixels);
        }
    }
    items.clear();
    next = 0;
}

// The map load's tasks started, up to batchTasks running, while items wait and the budget allows (main thread).
void spawnTasks()
{
    while(batchOpen && liveTasks.load() < batchTasks && aheadBytes.load() < aheadBudget)
    {
        {
            za::LockGuard g(lock);
            size_t n = next;
            while(n < items.size() && items[n]->claimed)
            {
                n++;
            }
            if(n >= items.size())
            {
                return;
            }
        }
        ++liveTasks;
        tasks.pushBack(qvr::jobs::async([] {
            work();
            --liveTasks;
        }));
    }
}

// Whether the start-up's list has `name` decoded ahead and not taken yet.
bool startupWaiting(const char* name)
{
    za::LockGuard g(lock);
    for(const auto& it : items)
    {
        if(!it->batch && !it->taken && it->name == name)
        {
            return true;
        }
    }
    return false;
}

// The image `base` (no extension) as Image_LoadImage would find it: true if it finds a file (png, tga, jpg, then pcx,
// lmp). A png, tga or jpg not decoded before (vr_imgcache), nor already waiting, is read for the workers.
bool batchFind(const char* base)
{
    if(!strncmp(base, "pbrmix/", 7))
    {
        return true; // (made, not read: Image_LoadImage stops there)
    }
    const bool extmap = VR_ExtMapsIsPath(base) != 0;
    static constexpr const char* formats[] = {"png", "tga", "jpg"};
    for(const char* ext : formats)
    {
        char path[MAX_OSPATH];
        q_snprintf(path, sizeof(path), "%s.%s", base, ext);
        if(VR_ImageCacheHasName(path) || unasked.contains(za::StringView{path}))
        {
            batchLast.cached++; // (decoded before, or read for nothing before: not opened here; the loop decodes a changed one)
            return true;
        }
        FILE* f = nullptr;
        if(extmap)
        {
            f = VR_ExtMapsOpen(path);
        }
        else
        {
            COM_FOpenFile(path, &f, nullptr);
        }
        if(!f)
        {
            continue;
        }
        const int length = com_filesize;
        auto it = za::makeUnique<Item>();
        if(batchByName.contains(za::StringView{path}) || startupWaiting(path))
        {
        }
        else if(VR_ImageCacheHas(path, f, length))
        {
            batchLast.cached++;
        }
        else if(length > 0 && qvr::files::identity(f, it->file) && (it->offset = ftell(f)) >= 0)
        {
            it->bytes.resize(static_cast<size_t>(length));
            if(fread(it->bytes.data(), 1, it->bytes.size(), f) == it->bytes.size())
            {
                it->name = path;
                it->batch = true;
                it->length = length;
                batchByName[it->name] = it.get();
                batchLast.read++;
                {
                    za::LockGuard g(lock);
                    items.pushBack(ZA_MOVE(it));
                }
                spawnTasks();
            }
        }
        fclose(f);
        return true;
    }
    if(!extmap)
    {
        static constexpr const char* others[] = {"pcx", "lmp"};
        for(const char* ext : others)
        {
            char path[MAX_OSPATH];
            q_snprintf(path, sizeof(path), "%s.%s", base, ext);
            FILE* f = nullptr;
            COM_FOpenFile(path, &f, nullptr);
            if(f)
            {
                fclose(f);
                return true;
            }
        }
    }
    return false;
}

// The first of `bases` found (its index), or -1.
int batchFindFirst(std::initializer_list<const char*> bases)
{
    int i = 0;
    for(const char* b : bases)
    {
        if(batchFind(b))
        {
            return i;
        }
        i++;
    }
    return -1;
}

// The map load's tasks finished (the items no task had come to dropped), its images not taken freed.
void batchEnd()
{
    if(!batchOpen)
    {
        return;
    }
    {
        za::LockGuard g(lock);
        for(auto& it : items)
        {
            if(it->batch && !it->claimed)
            {
                it->claimed = true;
                it->done.storeRelease(1u);
            }
        }
    }
    finishTasks();
    {
        za::LockGuard g(lock);
        za::Vector<za::UniquePtr<Item>> kept;
        for(auto& it : items)
        {
            if(!it->batch)
            {
                kept.pushBack(ZA_MOVE(it));
                continue;
            }
            if(!it->taken)
            {
                batchLast.unused++;
                unasked.insert(it->name);
                if(it->pixels)
                {
                    VR_HeapFree(it->pixels);
                }
            }
        }
        items = ZA_MOVE(kept);
        next = 0;
    }
    batchByName.clear();
    aheadBytes = 0;
    batchOpen = false;
    active = !items.empty() || windowOpen;
    if(batchLast.read + batchLast.cached >= batchBiggest.read + batchBiggest.cached)
    {
        batchBiggest = batchLast;
    }
    batchSession.loads++;
    batchSession.read += batchLast.read;
    batchSession.taken += batchLast.taken;
    batchSession.asked += batchLast.asked;
    batchSession.unused += batchLast.unused;
    batchSession.cached += batchLast.cached;
    batchSession.mainMs += batchLast.mainMs;
}

void printCounts(const char* what, const BatchCounts& c)
{
    Con_Printf("  %s: %d images read ahead, %d taken (%d decoded by the main thread: no task had come to them), %d not "
               "asked for; %d decoded before (vr_image_cache); %.1f ms finding and reading them\n",
        what, c.read, c.taken, c.asked, c.unused, c.cached, c.mainMs);
}

void info_f()
{
    Con_Printf("vr_image_prefetch_info (vr_texture_prefetch %d): the maps' textures decoded ahead\n",
        static_cast<int>(qvr::vr_texture_prefetch.value));
    printCounts("the biggest texture set since the last vr_image_prefetch_info (a map's world)", batchBiggest);
    batchBiggest = BatchCounts{};
    printCounts(va("this session (%d texture sets)", batchSession.loads), batchSession);
}

} // namespace

namespace qvr::imgprefetch
{

// VR_Init: the last session's list read, its files read, their decoding started.
void start()
{
    Cmd_AddCommand("vr_image_prefetch_info", info_f);
    windowOpen = true;
    za::String text;
    (void)qvr::files::readText(manifestPath().cStr(), text); // (none: an empty list)
    qvr::files::forLines(text, [](za::StringView line) {
        if(line.endsWith('\r'))
        {
            line.removeSuffix(1);
        }
        if(!line.empty())
        {
            manifest.emplaceBack(line);
        }
    });
    for(const za::String& name : manifest)
    {
        byte* data = COM_LoadMallocFile(name.cStr(), nullptr);
        if(!data)
        {
            continue;
        }
        auto it = za::makeUnique<Item>();
        it->name = name;
        it->bytes.assignRange(data, data + com_filesize);
        VR_HeapFree(data);
        items.pushBack(ZA_MOVE(it));
    }
    if(items.empty())
    {
        return;
    }
    active = true;
    const int threads = za::clamp(qvr::jobs::workers(), 1, 6);
    for(int i = 0; i < za::min<int>(threads, static_cast<int>(items.size())); i++)
    {
        tasks.pushBack(qvr::jobs::async(work));
    }
}

// The end of the first map load (or the shutdown): the workers joined, what wasn't taken freed, the list written if
// this session's differs.
void end()
{
    if(!windowOpen)
    {
        return;
    }
    windowOpen = false;
    batchEnd(); // (closed by now: Mod_LoadTextures)
    finishTasks();
    Con_DPrintf("imgprefetch: %d images read ahead, %d taken (%d of them decoded when asked for)\n", static_cast<int>(items.size()),
        takenCount, askedCount);
    freeItems();
    active = false;
    if(decoded != manifest && !decoded.empty())
    {
        qvr::files::createDirectories((za::String{com_gamedir} + "/cache").cStr());
        // (A temporary file of this copy's own: parallel test runs share the folder.)
        const za::String path = manifestPath(),
                          tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
        za::String text;
        for(const za::String& name : decoded)
        {
            text += name;
            text += '\n';
        }
        (void)qvr::files::writeText(tmp.cStr(), text);
        qvr::files::rename(tmp.cStr(), path.cStr());
    }
    manifest.clear();
    decoded.clear();
}

void shutdown()
{
    batchEnd();
    finishTasks();
    freeItems();
    active = false;
    windowOpen = false;
}

} // namespace qvr::imgprefetch

// Image_LoadImage: `name` (with its extension) opened as `f`, `length` bytes: the image decoded ahead if its bytes are
// the file's (malloc'd; *width, *height set), else NULL with `f` where it was.
extern "C" unsigned char* VR_ImagePrefetchTake(const char* name, FILE* f, int length, int* width, int* height)
{
    if(!active)
    {
        return nullptr;
    }
    Item* it = nullptr;
    bool decodeHere = false;
    {
        za::LockGuard g(lock);
        for(auto& i : items)
        {
            if(!i->batch && !i->taken && i->name == name)
            {
                it = i.get();
                break;
            }
        }
        if(!it && batchOpen) // (the map load's: Mod_LoadTextures, the main thread)
        {
            const auto b = batchByName.find(za::StringView{name});
            if(b != batchByName.end() && !b->second->taken)
            {
                it = b->second;
            }
        }
        if(!it)
        {
            return nullptr;
        }
        if(!it->claimed)
        {
            it->claimed = true; // no task has come to it yet: decoded here
            (it->batch ? batchLast.asked : askedCount)++;
            decodeHere = true;
        }
    }
    if(decodeHere)
    {
        decode(it);
    }
    it->done.waitUntilAcquire([](za::U32 done) { return done != 0u; });
    {
        za::LockGuard g(lock);
        it->taken = true; // (freed by the caller if taken, below if not)
    }
    bool same = false;
    if(it->batch)
    {
        if(it->pixels)
        {
            aheadBytes -= static_cast<long long>(it->width) * it->height * 4;
        }
        qvr::files::Identity file;
        same = it->pixels && length == it->length && qvr::files::identity(f, file) && file == it->file &&
               ftell(f) == it->offset;
        spawnTasks(); // (stopped at the budget: on again)
    }
    else
    {
        const long pos = ftell(f);
        za::Vector<unsigned char> bytes(static_cast<size_t>(za::max(length, 0)));
        same = length == static_cast<int>(it->bytes.size()) && it->pixels &&
               fread(bytes.data(), 1, bytes.size(), f) == bytes.size() && bytes == it->bytes;
        fseek(f, pos, SEEK_SET);
    }
    if(!same)
    {
        if(it->pixels)
        {
            VR_HeapFree(it->pixels);
            it->pixels = nullptr;
        }
        return nullptr;
    }
    unsigned char* px = it->pixels;
    it->pixels = nullptr;
    if(windowOpen && za::find(decoded.begin(), decoded.end(), it->name) == decoded.end())
    {
        decoded.pushBack(it->name); // (kept on the list: taken ahead, it took no time here)
    }
    *width = it->width;
    *height = it->height;
    (it->batch ? batchLast.taken : takenCount)++;
    return px;
}

// Image_LoadImage: a png, tga or jpg found and decoded (ahead or not) in this window, for the next session's list: one
// that took over 0.2 ms (over 1 ms before: the calibration room's 112 smaller ones, 50 ms decoded on the main thread at
// every start, read ahead now for 10 ms of the start-up; its images 120 to 80 ms).
extern "C" void VR_ImagePrefetchNote(const char* name, double seconds)
{
    if(windowOpen && seconds > 0.0002 && za::find(decoded.begin(), decoded.end(), name) == decoded.end())
    {
        decoded.pushBack(name);
    }
}

extern "C" void VR_ImagePrefetchEnd()
{
    qvr::imgprefetch::end();
}

// Mod_LoadTextures, before its loop: each texture's images found as its upload will look for them (a liquid's
// textures/<map>/#name then textures/#name; a regular one's the same without '#', then that file's _glow or _luma, then
// the external pack's: VR_ExtMapsPrepare's and VR_ExtMapsAttach's), the png, tga and jpg files not decoded before read
// here, in that order, and decoded on up to 8 tasks while the loop runs; Image_LoadImage takes each as it asks for it
// (the same file: the same pixels), or decodes it itself if no task has come to it yet. A file the loop doesn't ask for
// (the pack's maps of a texture its picture doesn't match) is decoded for nothing, on a worker.
extern "C" void VR_TexturePrefetchBegin(const char* mapname, const char* const* names, const int* kinds, int count)
{
    batchEnd(); // (one left open)
    batchLast = BatchCounts{};
    if(qvr::vr_texture_prefetch.value == 0.f || count <= 0)
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    batchOpen = true;
    active = true;
    batchTasks = za::clamp(qvr::jobs::workers(), 1, 8);
    za::Vector<za::Vector<za::String>> groups;
    for(int i = 0; i < count; i++)
    {
        const char* name = names[i];
        char a[MAX_OSPATH], b[MAX_OSPATH];
        if(kinds[i] == 1)
        {
            q_snprintf(a, sizeof(a), "textures/%s/#%s", mapname, name + 1);
            q_snprintf(b, sizeof(b), "textures/#%s", name + 1);
            (void)batchFindFirst({a, b});
        }
        else if(kinds[i] == 2)
        {
            q_snprintf(a, sizeof(a), "textures/%s/%s", mapname, name);
            q_snprintf(b, sizeof(b), "textures/%s", name);
            const int found = batchFindFirst({a, b});
            if(found >= 0)
            {
                char glow[MAX_OSPATH], luma[MAX_OSPATH];
                q_snprintf(glow, sizeof(glow), "%s_glow", found == 0 ? a : b);
                q_snprintf(luma, sizeof(luma), "%s_luma", found == 0 ? a : b);
                (void)batchFindFirst({glow, luma});
            }
            groups.clear();
            qvr::extmaps::prefetchNames(name, groups);
            for(const za::Vector<za::String>& g : groups)
            {
                for(const za::String& alt : g)
                {
                    if(batchFind(alt.cStr()))
                    {
                        break;
                    }
                }
            }
        }
    }
    batchLast.mainMs = (Sys_DoubleTime() - t0) * 1000.0;
    VR_TimeAdd("textures' images found and read for the workers (vr_texture_prefetch)", Sys_DoubleTime() - t0);
}

extern "C" void VR_TexturePrefetchEnd()
{
    batchEnd();
}
