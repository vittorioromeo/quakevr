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
#include "Zancle/Vocabulary/UniquePtr.hpp"

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
};

za::AtomicMutex lock;
za::Vector<za::UniquePtr<Item>> items;
za::Vector<qvr::jobs::Future<void>> tasks; // each decodes items until none is left
size_t next = 0; // the items before it are all claimed
bool active = false;

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
            if(next >= items.size())
            {
                return;
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
            free(it->pixels);
        }
    }
    items.clear();
    next = 0;
}

} // namespace

namespace qvr::imgprefetch
{

// VR_Init: the last session's list read, its files read, their decoding started.
void start()
{
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
        free(data);
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
            if(!i->taken && i->name == name)
            {
                it = i.get();
                break;
            }
        }
        if(!it)
        {
            return nullptr;
        }
        if(!it->claimed)
        {
            it->claimed = true; // no task has come to it yet: decoded here
            askedCount++;
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
    const long pos = ftell(f);
    za::Vector<unsigned char> bytes(static_cast<size_t>(za::max(length, 0)));
    const bool same = length == static_cast<int>(it->bytes.size()) && it->pixels &&
                      fread(bytes.data(), 1, bytes.size(), f) == bytes.size() && bytes == it->bytes;
    fseek(f, pos, SEEK_SET);
    if(!same)
    {
        if(it->pixels)
        {
            free(it->pixels);
            it->pixels = nullptr;
        }
        return nullptr;
    }
    unsigned char* px = it->pixels;
    it->pixels = nullptr;
    if(za::find(decoded.begin(), decoded.end(), it->name) == decoded.end())
    {
        decoded.pushBack(it->name); // (kept on the list: taken ahead, it took no time here)
    }
    *width = it->width;
    *height = it->height;
    takenCount++;
    return px;
}

// Image_LoadImage: a png, tga or jpg found and decoded (ahead or not) in this window, for the next session's list.
extern "C" void VR_ImagePrefetchNote(const char* name, double seconds)
{
    if(windowOpen && seconds > 0.001 && za::find(decoded.begin(), decoded.end(), name) == decoded.end())
    {
        decoded.pushBack(name);
    }
}

extern "C" void VR_ImagePrefetchEnd()
{
    qvr::imgprefetch::end();
}
