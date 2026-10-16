// vr_warmup.cpp -- see vr_warmup.hpp.

#include "vr_warmup.hpp"
#include "vr_ao.hpp"
#include "vr_cvars.hpp"
#include "vr_diskcache.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"
#include "vr_prepare.hpp"
#include "vr_tracy.h"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#ifdef _WIN32
// kernel32 (declared here rather than through <windows.h>, as vr_ao.cpp does)
__declspec(dllimport) void* __stdcall GetCurrentThread(void);
__declspec(dllimport) int __stdcall SetThreadPriority(void* thread, int priority);
__declspec(dllimport) int __stdcall GetThreadPriority(void* thread);
#endif
}

namespace qvr::warmup
{

namespace
{

constexpr double startDelay = 5.0;   // seconds after the first frame
constexpr double lateFrame = 0.014;  // a frame interval over this pauses it (90 Hz: 11.1 ms)
constexpr double quietAfter = 2.0;   // seconds without a late frame before it goes on
constexpr double doneWriteEvery = 15.0;
constexpr unsigned warmupVersion = 1; // the done list's: BUMP when the warm-up makes another product

// A file holding models: a .pak, or a loose .mdl.
struct Container
{
    za::String path;
    za::U64 size{0};
    za::I64 time{0};
    int items{0}, finished{0};
};

struct Item
{
    za::SizeT container{0};
    za::U32 offset{0}, size{0};
    char name[64]{};
};

struct Listing
{
    za::Vector<Container> containers;
    za::Vector<Item> items;
};

struct Result
{
    int code{-1}; // 1 made, 0 already there, -1 not made (not a model it makes, unreadable), -2 stopped
    double seconds{0.0};
};

enum class Phase
{
    Waiting,
    Listing,
    Running,
    Done,
    Disabled // a test run, the installer's run, a dedicated server
};

struct State
{
    Phase phase{Phase::Waiting};
    double firstFrame{0.0}, lastFrame{0.0}, quietUntil{0.0}, lastDoneWrite{0.0};
    double startedAt{0.0}, doneAt{0.0}, workerSeconds{0.0};
    const char* pauseWhy{""};
    Listing list;
    za::SizeT next{0};
    bool listing{false}, running{false};
    jobs::Future<Listing> listed;
    jobs::Future<Result> current;
    za::SizeT currentIndex{0};
    int made{0}, there{0}, skipped{0};
    za::String doneFile;
    za::Vector<za::String> doneLines; // read at the start, added to as containers finish
    bool doneDirty{false};
};
State state;
std::atomic<bool> stopping{false};
char statusText[192];

[[nodiscard]] za::String doneLineOf(const za::String& path, za::U64 size, za::I64 time)
{
    char tail[64];
    snprintf(tail, sizeof(tail), "\t%llu\t%lld", static_cast<unsigned long long>(size), static_cast<long long>(time));
    return path + tail;
}

[[nodiscard]] bool endsWithMdl(const char* name)
{
    const size_t n = strlen(name);
    return n > 4 && !q_strcasecmp(name + n - 4, ".mdl");
}

[[nodiscard]] za::I32 rd32(const unsigned char* p)
{
    za::I32 v;
    memcpy(&v, p, 4);
    return v;
}

// The models of each game folder of `bases` (a pool thread): its .pak files' .mdl entries, its loose progs/**.mdl;
// those in `done` left out.
Listing list(const za::Vector<za::String>& bases, const za::Vector<za::String>& done)
{
    QVR_TRACY_SCOPE(tracyScope, "cache warm-up listing");
    Listing out;
    const auto isDone = [&](const za::String& line) { return za::find(done.begin(), done.end(), line) != done.end(); };
    const auto addContainer = [&](const za::String& path) -> za::SizeT {
        Container c;
        c.path = path;
        c.size = files::fileSize(path.cStr());
        c.time = files::lastWriteTime(path.cStr());
        if(isDone(doneLineOf(c.path, c.size, c.time)))
        {
            return static_cast<za::SizeT>(-1);
        }
        out.containers.pushBack(ZA_MOVE(c));
        return out.containers.size() - 1;
    };
    za::Vector<za::String> loose;
    // (a folder's tree, its .mdl files; its own function: the lambda can't call itself)
    struct Walk
    {
        static void run(const za::String& dir, za::Vector<za::String>& found, int depth)
        {
            files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
                if(isDirectory && depth < 4 && name[0] != '.')
                {
                    run(dir + "/" + name, found, depth + 1);
                }
                else if(!isDirectory && endsWithMdl(name))
                {
                    found.pushBack(dir + "/" + name);
                }
            });
        }
    };
    for(const za::String& base : bases)
    {
        za::Vector<za::String> games;
        files::forEachEntry(base.cStr(), [&](const char* name, bool isDirectory) {
            if(isDirectory && name[0] != '.' && q_strcasecmp(name, "cache") != 0)
            {
                games.pushBack(base + "/" + name);
            }
        });
        for(const za::String& game : games)
        {
            if(stopping.load())
            {
                return out;
            }
            za::Vector<za::String> paks;
            files::forEachEntry(game.cStr(), [&](const char* name, bool isDirectory) {
                const size_t n = strlen(name);
                if(!isDirectory && n > 4 && !q_strcasecmp(name + n - 4, ".pak"))
                {
                    paks.pushBack(game + "/" + name);
                }
            });
            for(const za::String& pak : paks)
            {
                FILE* f = Sys_fopen(pak.cStr(), "rb");
                if(!f)
                {
                    continue;
                }
                unsigned char header[12];
                za::Vector<unsigned char> dir;
                if(fread(header, 1, 12, f) == 12 && !memcmp(header, "PACK", 4) && rd32(header + 8) > 0 &&
                    rd32(header + 8) % 64 == 0 && rd32(header + 8) < (64 << 20))
                {
                    dir.resize(static_cast<za::SizeT>(rd32(header + 8)));
                    if(Sys_fseek(f, rd32(header + 4), SEEK_SET) != 0 || fread(dir.data(), 1, dir.size(), f) != dir.size())
                    {
                        dir.clear();
                    }
                }
                fclose(f);
                za::SizeT c = static_cast<za::SizeT>(-1);
                for(za::SizeT e = 0; e + 64 <= dir.size(); e += 64)
                {
                    char name[57];
                    memcpy(name, dir.data() + e, 56);
                    name[56] = 0;
                    if(!endsWithMdl(name) || strlen(name) >= sizeof(Item::name))
                    {
                        continue;
                    }
                    if(c == static_cast<za::SizeT>(-1))
                    {
                        c = addContainer(pak);
                        if(c == static_cast<za::SizeT>(-1))
                        {
                            break; // (done before)
                        }
                    }
                    Item it;
                    it.container = c;
                    it.offset = static_cast<za::U32>(rd32(dir.data() + e + 56));
                    it.size = static_cast<za::U32>(rd32(dir.data() + e + 60));
                    q_strlcpy(it.name, name, sizeof(it.name));
                    out.items.pushBack(it);
                    out.containers[c].items++;
                }
            }
            loose.clear();
            Walk::run(game + "/progs", loose, 0);
            for(const za::String& path : loose)
            {
                const za::SizeT c = addContainer(path);
                if(c == static_cast<za::SizeT>(-1))
                {
                    continue;
                }
                Item it;
                it.container = c;
                it.size = static_cast<za::U32>(out.containers[c].size);
                const char* progs = strstr(path.cStr(), "/progs/");
                q_strlcpy(it.name, progs ? progs + 1 : path.cStr(), sizeof(it.name));
                out.items.pushBack(it);
                out.containers[c].items++;
            }
        }
    }
    return out;
}

// A .mdl's poses, triangles, scale and origin (as Mod_LoadAliasModel reads them), then its products made.
Result makeItem(const za::String& container, Item item)
{
    QVR_TRACY_SCOPE(tracyScope, "cache warm-up model");
    Result r;
    const double t0 = Sys_DoubleTime();
#ifdef _WIN32
    const int was = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), -15); // THREAD_PRIORITY_IDLE
#endif
    za::Vector<unsigned char> bytes;
    FILE* f = Sys_fopen(container.cStr(), "rb");
    if(f)
    {
        bytes.resize(item.size);
        if(Sys_fseek(f, static_cast<long>(item.offset), SEEK_SET) != 0 || fread(bytes.data(), 1, bytes.size(), f) != bytes.size())
        {
            bytes.clear();
        }
        fclose(f);
    }
    const unsigned char* d = bytes.data();
    const size_t n = bytes.size();
    size_t p = 84;
    bool ok = n >= 84 && !memcmp(d, "IDPO", 4) && rd32(d + 4) == 6;
    const int numskins = ok ? rd32(d + 48) : 0, w = ok ? rd32(d + 52) : 0, h = ok ? rd32(d + 56) : 0;
    const int numverts = ok ? rd32(d + 60) : 0, numtris = ok ? rd32(d + 64) : 0, numframes = ok ? rd32(d + 68) : 0;
    ok = ok && numskins >= 0 && numskins < 256 && w > 0 && h > 0 && w <= 4096 && h <= 4096 && numverts > 0 &&
         numverts < 65536 && numtris > 0 && numtris < 1 << 20 && numframes > 0 && numframes < 1 << 16;
    const size_t skinBytes = ok ? static_cast<size_t>(w) * h : 0;
    for(int s = 0; ok && s < numskins; s++)
    {
        ok = p + 4 <= n;
        if(ok && rd32(d + p) == 0)
        {
            p += 4 + skinBytes;
        }
        else if(ok)
        {
            ok = p + 8 <= n;
            const int count = ok ? rd32(d + p + 4) : 0;
            ok = ok && count > 0 && count < 1024;
            p += 8 + static_cast<size_t>(count) * (4 + skinBytes);
        }
        ok = ok && p <= n;
    }
    p += static_cast<size_t>(numverts) * 12; // st verts
    const size_t trisAt = p;
    p += static_cast<size_t>(numtris) * 16;
    ok = ok && p <= n;
    za::Vector<int> tris;
    for(int t = 0; ok && t < numtris; t++)
    {
        for(int k = 0; k < 3; k++)
        {
            const int v = rd32(d + trisAt + static_cast<size_t>(t) * 16 + 4 + 4 * k);
            ok = ok && v >= 0 && v < numverts;
            tris.pushBack(v);
        }
    }
    za::Vector<unsigned char> verts; // the poses in order, numverts trivertx_t (4 bytes) each
    const size_t poseBytes = static_cast<size_t>(numverts) * 4;
    int numposes = 0;
    for(int fr = 0; ok && fr < numframes; fr++)
    {
        ok = p + 4 <= n;
        const int type = ok ? rd32(d + p) : 0;
        p += 4;
        int count = 1;
        if(ok && type != 0)
        {
            ok = p + 4 <= n;
            count = ok ? rd32(d + p) : 0;
            ok = ok && count > 0 && count < 1024;
            p += 4 + 8 + static_cast<size_t>(count) * 4; // the count, the group's bounds, its intervals
        }
        for(int k = 0; ok && k < count; k++)
        {
            p += 8 + 16; // its bounds, its name
            ok = p + poseBytes <= n;
            if(ok)
            {
                const size_t at = verts.size();
                verts.resize(at + poseBytes);
                memcpy(verts.data() + at, d + p, poseBytes);
                numposes++;
            }
            p += poseBytes;
        }
    }
    if(ok)
    {
        float scale[3], origin[3];
        memcpy(scale, d + 8, sizeof(scale));
        memcpy(origin, d + 20, sizeof(origin));
        r.code = ao::warmBake(item.name, numverts, numposes, verts.data(), tris.data(), numtris, scale, origin);
    }
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), was);
#endif
    if(stopping.load())
    {
        r.code = -2;
    }
    r.seconds = Sys_DoubleTime() - t0;
    return r;
}

[[nodiscard]] bool allowed()
{
    if(cls.state == ca_dedicated || prepare::active())
    {
        return false;
    }
    return !getenv("QVR_TEST_HIDDEN") || vr_cache_warmup_debug.value >= 2.f;
}

void writeDone()
{
    if(!state.doneDirty || state.doneFile.empty())
    {
        return;
    }
    state.doneDirty = false;
    za::String text;
    for(const za::String& line : state.doneLines)
    {
        text += line;
        text += "\n";
    }
    files::createDirectories((diskcache::root() + "/warmup").cStr());
    const diskcache::Part part{text.cStr(), text.size()};
    (void)diskcache::writeAtomic(state.doneFile, &part, 1);
}

void startListing()
{
    char name[48];
    snprintf(name, sizeof(name), "/warmup/done_w%u_ao%d.txt", warmupVersion, ao::bakeVersion());
    state.doneFile = diskcache::root() + name;
    za::String text;
    state.doneLines.clear();
    if(files::readText(state.doneFile.cStr(), text))
    {
        files::forLines(za::StringView{text.cStr(), text.size()}, [&](za::StringView line) {
            if(line.size())
            {
                state.doneLines.pushBack(za::String{line});
            }
        });
    }
    za::Vector<za::String> bases;
    for(int i = 0; i < com_numbasedirs; i++)
    {
        bases.pushBack(za::String{com_basedirs[i]});
    }
    state.phase = Phase::Listing;
    state.startedAt = Sys_DoubleTime();
    state.listed = jobs::async([bases = ZA_MOVE(bases), done = state.doneLines] { return list(bases, done); });
    state.listing = true;
}

// The model just made: counted, its container noted done when all of its are.
void harvest()
{
    const Result r = state.current.get();
    state.running = false;
    state.workerSeconds += r.seconds;
    const Item& it = state.list.items[state.currentIndex];
    if(r.code == -2)
    {
        state.next = state.currentIndex; // (made again next session)
        return;
    }
    (r.code == 1 ? state.made : r.code == 0 ? state.there : state.skipped)++;
    if(r.code == 1 && vr_cache_warmup_debug.value >= 1.f)
    {
        Con_DPrintf("cache warm-up: %s baked, %.0f ms\n", it.name, r.seconds * 1000.0);
    }
    Container& c = state.list.containers[it.container];
    if(++c.finished == c.items)
    {
        state.doneLines.pushBack(doneLineOf(c.path, c.size, c.time));
        state.doneDirty = true;
    }
}

} // namespace

void frame()
{
    const double now = Sys_DoubleTime();
    const double interval = state.lastFrame > 0.0 ? now - state.lastFrame : 0.0;
    state.lastFrame = now;
    if(state.firstFrame == 0.0)
    {
        state.firstFrame = now;
    }
    if(state.phase == Phase::Disabled || state.phase == Phase::Done)
    {
        return;
    }
    if(state.running && state.current.ready())
    {
        harvest();
    }
    if(state.listing && state.listed.ready())
    {
        state.list = state.listed.get();
        state.listing = false;
        state.phase = Phase::Running;
        Con_DPrintf("cache warm-up: %d models in %d files to look at (%.1f ms to list)\n",
            static_cast<int>(state.list.items.size()), static_cast<int>(state.list.containers.size()),
            (Sys_DoubleTime() - state.startedAt) * 1000.0);
    }
    const bool loading = scr_disabled_for_loading || (cls.state == ca_connected && cls.signon < SIGNONS);
    if(interval > lateFrame || loading)
    {
        state.quietUntil = now + quietAfter;
    }
    state.pauseWhy = !vr_cache_warmup.value ? "off"
                     : loading               ? "a map loading"
                     : now < state.quietUntil ? "frames running late"
                                              : "";
    if(state.phase == Phase::Waiting)
    {
        if(!allowed())
        {
            state.phase = Phase::Disabled;
            return;
        }
        if(vr_cache_warmup.value && !state.pauseWhy[0] && now - state.firstFrame >= startDelay)
        {
            startListing();
        }
        return;
    }
    if(state.phase != Phase::Running || state.running)
    {
        return;
    }
    if(state.doneDirty && now - state.lastDoneWrite > doneWriteEvery)
    {
        state.lastDoneWrite = now;
        writeDone();
    }
    if(state.pauseWhy[0] || stopping.load())
    {
        return;
    }
    if(state.next >= state.list.items.size())
    {
        state.phase = Phase::Done;
        state.doneAt = now;
        writeDone();
        Con_DPrintf("cache warm-up: done, %d made, %d already there, %d not for it, %.1f s of a worker's time\n", state.made,
            state.there, state.skipped, state.workerSeconds);
        return;
    }
    state.currentIndex = state.next++;
    const Item item = state.list.items[state.currentIndex];
    state.current = jobs::async([path = state.list.containers[item.container].path, item] { return makeItem(path, item); });
    state.running = true;
}

void stop()
{
    stopping.store(true);
    if(state.listing)
    {
        (void)state.listed.get();
        state.listing = false;
    }
    if(state.running)
    {
        harvest();
    }
    writeDone();
}

const char* statusLine(int)
{
    const int total = static_cast<int>(state.list.items.size());
    const int done = state.made + state.there + state.skipped;
    const char* what = state.phase == Phase::Disabled ? "not in this run"
                       : state.phase == Phase::Waiting ? (vr_cache_warmup.value ? "starting" : "off")
                       : state.phase == Phase::Listing ? "listing the games' models"
                       : state.phase == Phase::Done    ? "done"
                       : state.pauseWhy[0]             ? state.pauseWhy
                                                       : "running";
    snprintf(statusText, sizeof(statusText), "Cache warm-up: %d of %d models (%d made), %s%s", done, total, state.made,
        state.phase == Phase::Running && state.pauseWhy[0] ? "paused: " : "", what);
    return statusText;
}

void printProgress()
{
    Con_Printf("%s; %d already there, %d not for it; %.1f s of a worker's time%s\n", statusLine(0), state.there,
        state.skipped, state.workerSeconds,
        state.phase == Phase::Done ? va(", finished %.1f s after it started", state.doneAt - state.startedAt) : "");
}

void registerCommands()
{
    Cmd_AddCommand("vr_cache_warmup_info", printProgress);
}

} // namespace qvr::warmup
