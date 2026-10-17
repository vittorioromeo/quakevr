// Tracy v0.14.1 bridge. CPU nesting is per-thread; GL bookkeeping belongs to the main thread.
// Explicit ends also allow Host_Error/Host_EndGame to unwind before longjmp recovery.
#include "vr_tracy.h"
#ifdef QVR_TRACY
#include "vr_engine.hpp"
#include "external/tracy/tracy/TracyC.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#include "external/tracy/tracy/Tracy.hpp"
#pragma clang diagnostic pop
#include "SDL.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string_view>
#include "external/box3d/include/box3d/base.h"
#include "vr_mem.hpp"
#include "vr_jobs.hpp"
#include "vr_profile.hpp"
#include "vr_profile.h"
#include "vr_backend.hpp"
#include "vr_main.hpp"
#include "vr_ragdoll.hpp"
#include "vr_decals.hpp"
#include "vr_particles.hpp"
extern "C" int r_numactiveparticles;
extern "C" int vr_tracy_physics_zones = 0;
extern "C" int vr_tracy_render_zones = 0;
extern "C" int vr_tracy_gpu_zones = 0;
#ifdef _WIN32
// GetProcessMemoryInfo (kernel32's K32 export), declared here rather than through <windows.h> (as vr_main.cpp's).
namespace
{
struct ProcessMemoryCounters
{
    unsigned long cb;
    unsigned long pageFaultCount;
    size_t peakWorkingSetSize, workingSetSize, quotaPeakPagedPoolUsage, quotaPagedPoolUsage, quotaPeakNonPagedPoolUsage,
        quotaNonPagedPoolUsage, pagefileUsage, peakPagefileUsage, privateUsage;
};
}
extern "C" __declspec(dllimport) void* __stdcall GetCurrentProcess();
extern "C" __declspec(dllimport) int __stdcall K32GetProcessMemoryInfo(void* process, ProcessMemoryCounters* counters,
    unsigned long size);
#endif
extern "C" void Memory_TracyPlots(void);
extern "C" double TexMgr_TracyBytes(void);
extern "C" void Memory_TracySnapshot(void);
extern "C" void Memory_TracyValidate(void);
extern "C" int Memory_TracyCacheMove(cache_user_t* cache);
extern "C" const char* VR_ActiveCampaignFolder();
extern "C" int VR_IsNativeCampaignLaunch();

namespace
{
constexpr unsigned maxDepth = 256;
constexpr unsigned maxNames = 1024;
constexpr unsigned queryCount = 4096;
struct Open
{
    TracyCZoneCtx cpu{};
    uint64_t connection{};
    const char* name{}; // a persistent name (VR_TracyCurrentName), null for VR_TracyBeginAt's
    bool gpu{};
};
struct Thread
{
    Open stack[maxDepth]{};
    unsigned depth{};
    unsigned overflow{};
    const ___tracy_source_location_data* sources[maxNames]{};
    unsigned names{};
    bool named{};
};
thread_local Thread thread;
___tracy_source_location_data sources[maxNames]{};
unsigned names{};
std::atomic_flag sourceLock = ATOMIC_FLAG_INIT;
std::atomic<unsigned> dropped{0};
unsigned forceClosed{};
bool abortedByError{};
SDL_threadID mainThread{};
cvar_t gpuEnabled = {"vr_tracy_gpu", "1", CVAR_NONE};
cvar_t gpuDetail = {"vr_tracy_gpu_detail", "0", CVAR_NONE};
unsigned queries[queryCount]{};
unsigned head{}, tail{}, pending{}, openGpu{}, contextSerial{};
uint64_t connection{};
bool gpuReady{};
cvar_t qcDetail = {"vr_tracy_qc_detail", "0", CVAR_NONE};
cvar_t memoryDetail = {"vr_tracy_memory_detail", "0", CVAR_NONE};
// SV_Physics's zone per edict (sv_phys.c SV_PHYS_ZONE): two or three per edict a tick inflate "Server physics" by
// about 0.1 ms, so off by default (the player's own "SV edict client" and its hooks are always zoned).
cvar_t physicsDetail = {"vr_tracy_physics_detail", "0", CVAR_NONE};
// Per-entity render steps (r_alias.c: each model's frame, light, instance; a few zones a model an eye): off by default.
cvar_t renderDetail = {"vr_tracy_render_detail", "0", CVAR_NONE};
struct Allocation { unsigned long long bytes; int pool, end; };
std::unordered_map<const void*, Allocation> allocations;
constexpr const char* poolNames[] = {"Zone", "Hunk", "Cache"};
uint64_t memorySession{};
bool memoryActive{}, snapshotting{};
bool poolAnnounced[3]{};
unsigned memoryMismatches{};
unsigned memoryTransitions{};
// Native Box3D macros use Tracy's default pool. Keep survivors even while disconnected, so later
// native frees/reallocations remain meaningful in the next capture. Box3D can allocate on workers.
std::mutex boxLock;
std::unordered_map<const void*, unsigned long long> boxLive;
unsigned long long boxBytes{};
uint64_t boxSession{};
void boxSync()
{
    const uint64_t session = TracyCIsConnected ? tracy::GetProfiler().ConnectionId() : 0;
    if(session == boxSession) return;
    boxSession = session;
    if(session)
        for(const auto& [ptr, bytes] : boxLive) ___tracy_emit_memory_alloc(ptr, static_cast<size_t>(bytes));
}
void boxCheckpoint()
{
    bool pass;
    size_t live;
    unsigned long long captured, actual;
    {
        std::lock_guard lock{boxLock};
        boxSync();
        if(!boxSession) return;
        actual = static_cast<unsigned long long>(b3GetByteCount());
        captured = boxBytes;
        live = boxLive.size();
        pass = actual == captured;
        if(!pass) ++memoryMismatches;
        VR_TracyPlot("Box3D heap bytes", static_cast<double>(actual));
        VR_TracyPlot("Tracy default captured bytes", static_cast<double>(captured));
        VR_TracyPlot("Box3D live allocations", static_cast<double>(live));
        VR_TracyPlot("Tracy pool mismatches", memoryMismatches);
    }
    Con_Printf("Tracy Box3D default: %s live %zu bytes %llu/%llu\n", pass ? "PASS" : "FAIL", live, captured, actual);
}
unsigned resourceFrame{};
double displayPeriodMs{};
int missedDisplayPeriods{-1};
uint64_t metadataSession{};
char metadataMap[64]{};
thread_local bool qcOpen[1024]{};
thread_local unsigned qcDepth{}, qcOverflow{};
void memorySync()
{
    if(snapshotting || memoryTransitions) return;
    const uint64_t session = TracyCIsConnected ? tracy::GetProfiler().ConnectionId() : 0;
    const bool active = session && memoryDetail.value != 0;
    if(session == memorySession && active == memoryActive) return;
    // v0.14.1's server returns before advancing its serial clock for an unknown discard pool.
    // Only discard names already announced in THIS session; new sessions start with allocations.
    if(session == memorySession && session)
        for(unsigned pool = 0; pool < 3; ++pool)
            if(poolAnnounced[pool]) ___tracy_emit_memory_discard(poolNames[pool]);
    if(session != memorySession) std::memset(poolAnnounced, 0, sizeof(poolAnnounced));
    allocations.clear();
    memorySession = session;
    memoryActive = active;
    if(active)
    {
        snapshotting = true;
        Memory_TracySnapshot(); // baseline before any mutation, including late connect and mode changes
        Memory_TracyValidate();
        snapshotting = false;
    }
}

using QueryGen = void (APIENTRY *)(int, unsigned*);
using QueryDelete = void (APIENTRY *)(int, const unsigned*);
using QueryCounter = void (APIENTRY *)(unsigned, unsigned);
using QueryIV = void (APIENTRY *)(unsigned, unsigned, int*);
using QueryU64 = void (APIENTRY *)(unsigned, unsigned, uint64_t*);
using Integer64 = void (APIENTRY *)(unsigned, int64_t*);
QueryGen gen{};
QueryDelete del{};
QueryCounter counter{};
QueryIV available{};
QueryU64 result{};
Integer64 integer64{};
constexpr unsigned timestamp = 0x8E28;
constexpr unsigned queryAvailable = 0x8867;
constexpr unsigned queryResult = 0x8866;

const ___tracy_source_location_data* source(const char* name)
{
    for(unsigned i = 0; i < thread.names; ++i)
        if(thread.sources[i]->name == name)
            return thread.sources[i];
    while(sourceLock.test_and_set(std::memory_order_acquire)) {}
    for(unsigned i = 0; i < names; ++i)
        if(std::strcmp(sources[i].name, name) == 0)
        {
            const auto* s = &sources[i];
            sourceLock.clear(std::memory_order_release);
            if(thread.names < maxNames) thread.sources[thread.names++] = s;
            return s;
        }
    if(names == maxNames)
    {
        sourceLock.clear(std::memory_order_release);
        dropped.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    auto& s = sources[names++];
    s = {name, "QVR scope", "Quake/vr/vr_tracy.cpp", 0, 0};
    sourceLock.clear(std::memory_order_release);
    if(thread.names < maxNames) thread.sources[thread.names++] = &s;
    return &s;
}
uint16_t query()
{
    const unsigned id = head;
    counter(queries[id], timestamp);
    head = (head + 1) % queryCount;
    ++pending;
    return static_cast<uint16_t>(id);
}
void collect()
{
    while(pending)
    {
        int ready = 0;
        available(queries[tail], queryAvailable, &ready);
        if(!ready) break; // never wait for the GPU
        uint64_t time = 0;
        result(queries[tail], queryResult, &time);
        ___tracy_emit_gpu_time({static_cast<int64_t>(time), static_cast<uint16_t>(tail),
            static_cast<uint8_t>(contextSerial)});
        tail = (tail + 1) % queryCount;
        --pending;
    }
}
void gpuConnect(uint64_t session)
{
    if(!SDL_GL_GetCurrentContext()) return;
    connection = session;
    head = tail = pending = openGpu = 0;
    gpuReady = false;
    if(!queries[0])
    {
        gen = reinterpret_cast<QueryGen>(SDL_GL_GetProcAddress("glGenQueries"));
        del = reinterpret_cast<QueryDelete>(SDL_GL_GetProcAddress("glDeleteQueries"));
        counter = reinterpret_cast<QueryCounter>(SDL_GL_GetProcAddress("glQueryCounter"));
        available = reinterpret_cast<QueryIV>(SDL_GL_GetProcAddress("glGetQueryObjectiv"));
        result = reinterpret_cast<QueryU64>(SDL_GL_GetProcAddress("glGetQueryObjectui64v"));
        integer64 = reinterpret_cast<Integer64>(SDL_GL_GetProcAddress("glGetInteger64v"));
        if(!gen || !del || !counter || !available || !result || !integer64) return;
        gen(queryCount, queries);
    }
    // IDs in Tracy's wire format are 8-bit; do not wrap into an earlier context.
    if(contextSerial >= 254) return;
    ++contextSerial;
    int64_t time = 0;
    integer64(timestamp, &time); // one synchronization per connection/context, never per frame
    ___tracy_emit_gpu_new_context({time, 1.f, static_cast<uint8_t>(contextSerial), 0,
        static_cast<uint8_t>(tracy::GpuContextType::OpenGl)});
    constexpr char name[] = "Quake VR OpenGL";
    ___tracy_emit_gpu_context_name({static_cast<uint8_t>(contextSerial), name, sizeof(name) - 1});
    gpuReady = true;
}
bool coarse(const char* name)
{
    return std::strcmp(name, "eye L") == 0 || std::strcmp(name, "eye R") == 0 || std::strcmp(name, "eyes: single pass") == 0 ||
        std::strcmp(name, "window view") == 0 || std::strcmp(name, "shadow maps") == 0 || std::strcmp(name, "3D") == 0;
}
void status()
{
    Con_Printf("Tracy 0.14.1: %s, CPU depth %u, dropped %u, force-closed at frame start %u, GPU %s detail %.0f pending %u context %u\n",
        TracyCIsConnected ? "connected" : "waiting on localhost:8086", thread.depth,
        dropped.load(std::memory_order_relaxed), forceClosed, gpuReady ? "ready" : "idle", gpuDetail.value,
        pending, contextSerial);
    Con_Printf("  detail: gpu %s, gpu_detail %s, qc %s, physics %s, render %s, memory %s (vr_tracy_preset full|off)\n",
        gpuEnabled.string, gpuDetail.string, qcDetail.string, physicsDetail.string, renderDetail.string, memoryDetail.string);
}
// vr_tracy_preset full|off: every detail cvar for a full capture (TRACY.md, "Full capture"), or back to the defaults.
struct PresetValue
{
    cvar_t* var;
    float full;
};
// Memory detail stays off: its allocation events cost about 5 ms of CPU a frame in a fight (combat_48: busy p50 7.4 ms
// against 2.5; ROUND22.md, "Tracy coverage"). Turn it on by hand for an allocation capture, not a timing one.
const PresetValue presetValues[] = {{&gpuEnabled, 1.f}, {&gpuDetail, 1.f}, {&qcDetail, 1.f}, {&physicsDetail, 1.f},
    {&renderDetail, 1.f}, {&memoryDetail, 0.f}};
void preset()
{
    const char* which = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const bool full = q_strcasecmp(which, "full") == 0;
    if(!full && q_strcasecmp(which, "off") != 0)
    {
        Con_Printf("vr_tracy_preset full: every timing detail on (GPU passes, QC functions, physics per edict, render "
                   "steps; memory events off: they cost ~5 ms a frame); vr_tracy_preset off: the defaults\n");
        status();
        return;
    }
    for(const PresetValue& p : presetValues)
        if(full) Cvar_SetValueQuick(p.var, p.full);
        else Cvar_SetQuick(p.var, p.var->default_string);
    Con_Printf("Tracy detail preset: %s\n", full ? "full" : "off (defaults)");
    if(const auto* profile = Cvar_FindVar("vr_profile"); full && profile && profile->value != 0.f)
        Con_Printf("  vr_profile is %s: set it to 0 for a capture (it times the same scopes again)\n", profile->string);
    status();
}
void memoryTest()
{
    QVR_TRACY_SCOPE(scope, "Tracy allocation lifecycle test");
    Con_Printf("Tracy memory test: run from the console/menu, not mid-fight (may evict cache)\n");
    const float oldDetail = memoryDetail.value;
    Cvar_SetValueQuick(&memoryDetail, 1);
    void* ptr = Z_Malloc(37);
    Memory_TracyValidate();
    ptr = Z_Realloc(ptr, 131);
    Memory_TracyValidate();
    Z_Free(ptr);
    Memory_TracyValidate();
    boxCheckpoint();
    const int mark = Hunk_LowMark();
    Hunk_AllocName(91, "tracy-test");
    Memory_TracyValidate();
    Hunk_FreeToLowMark(mark);
    Memory_TracyValidate();
    cache_user_t cache{};
    Cache_Alloc(&cache, 513, "tracy-test");
    Memory_TracyValidate();
    Con_Printf("Tracy cache move test: %s\n", Memory_TracyCacheMove(&cache) ? "MOVED" : "evicted");
    Memory_TracyValidate();
    if(cache.data) Cache_Free(&cache, false);
    Memory_TracyValidate();
    ptr = Z_Malloc(71);
    Cvar_SetValueQuick(&memoryDetail, 0);
    memorySync();
    Cvar_SetValueQuick(&memoryDetail, 1);
    memorySync();
    Z_Free(ptr);
    Memory_TracyValidate();
    Cvar_SetValueQuick(&memoryDetail, oldDetail);
    unsigned long long bytes[3]{};
    for(const auto& [p, a] : allocations) bytes[a.pool] += a.bytes;
    Con_Printf("Tracy memory test: allocations %zu Zone %llu Hunk %llu Cache %llu bytes\n", allocations.size(), bytes[0], bytes[1], bytes[2]);
}
void abortTest()
{
    VR_TracyBegin("Tracy recovery test", 0);
    Host_Error("Tracy recovery test (intentional)");
}
void stackTest()
{
    const unsigned before = thread.depth;
    const unsigned oldDrops = dropped.load(std::memory_order_relaxed);
    for(unsigned i = 0; i < maxDepth + 64; ++i) VR_TracyBegin("Tracy bounded nesting test", 0);
    for(unsigned i = 0; i < maxDepth + 64; ++i) VR_TracyEnd();
    Con_Printf("Tracy nesting test: %s depth %u/%u overflow %u dropped %u\n",
        thread.depth == before && thread.overflow == 0 ? "PASS" : "FAIL", thread.depth, before,
        thread.overflow, dropped.load(std::memory_order_relaxed) - oldDrops);
}
}

extern "C" void VR_TracyInit()
{
    mainThread = SDL_ThreadID();
    TracyCSetThreadName("Quake VR main");
    thread.named = true;
    Cvar_RegisterVariable(&gpuEnabled);
    Cvar_RegisterVariable(&gpuDetail);
    Cvar_RegisterVariable(&qcDetail);
    Cvar_RegisterVariable(&memoryDetail);
    Cvar_RegisterVariable(&physicsDetail);
    Cvar_RegisterVariable(&renderDetail);
    Cmd_AddCommand("vr_tracy_status", status);
    Cmd_AddCommand("vr_tracy_preset", preset);
    Cmd_AddCommand("vr_tracy_test_memory", memoryTest);
    Cmd_AddCommand("vr_tracy_test_error", abortTest);
    Cmd_AddCommand("vr_tracy_test_stack", stackTest);
    Cmd_AddCommand("vr_tracy_test_gl_reset", VR_TracyGLShutdown);
}
extern "C" void VR_TracyThreadName(const char* name)
{
    if(!thread.named)
    {
        TracyCSetThreadName(name);
        thread.named = true;
    }
}
extern "C" void VR_TracyText(const char* text)
{
    if(thread.depth && !thread.overflow)
    {
        const auto ctx = thread.stack[thread.depth - 1].cpu;
        if(ctx.active)
        {
            const size_t length = std::strlen(text);
            ___tracy_emit_zone_text(ctx, text, length < 65535 ? length : 65534);
        }
    }
}
extern "C" const char* VR_TracyCurrentName()
{
    if(!TracyCIsConnected) return nullptr;
    for(unsigned d = thread.depth; d > 0; --d)
        if(thread.stack[d - 1].name) return thread.stack[d - 1].name;
    return nullptr;
}
extern "C" void VR_TracyJobText(const char* from, const char* file, unsigned line)
{
    if(!TracyCIsConnected || !thread.depth || !thread.stack[thread.depth - 1].cpu.active) return;
    const char* base = file;
    for(const char* c = file; *c; ++c)
        if(*c == '/' || *c == '\\') base = c + 1;
    char text[256];
    const int length = q_snprintf(text, sizeof(text), "%s @ %s:%u", from ? from : "(no zone)", base, line);
    ___tracy_emit_zone_text(thread.stack[thread.depth - 1].cpu, text, length < 0 ? 0 : std::min<size_t>(static_cast<size_t>(length), sizeof(text) - 1));
}
extern "C" void VR_TracyBegin(const char* name, int gpu)
{
    if(thread.depth == maxDepth)
    {
        ++thread.overflow;
        dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    Open& o = thread.stack[thread.depth++];
    o = {};
    o.name = name;
    if(!TracyCIsConnected) return;
    const auto* s = source(name);
    if(!s) return;
#ifdef TRACY_CALLSTACK
    o.cpu = ___tracy_emit_zone_begin_callstack(s, TRACY_CALLSTACK, 1);
#else
    o.cpu = ___tracy_emit_zone_begin(s, 1);
#endif
    // The upstream inactive context leaves id/connectionId indeterminate: a disconnect can
    // race the check above, so never read those fields unless the returned zone is active.
    if(!o.cpu.active)
    {
        o.cpu = {};
        return;
    }
    o.connection = o.cpu.connectionId;
    if(gpu && gpuReady && gpuEnabled.value && SDL_ThreadID() == mainThread &&
        (gpuDetail.value || coarse(name)) && connection == o.connection)
    {
        // Reserve one timestamp for each open zone's end; a full ring drops whole zones.
        if(pending + openGpu + 2 > queryCount)
            dropped.fetch_add(1, std::memory_order_relaxed);
        else
        {
            const uint16_t id = query();
            ___tracy_emit_gpu_zone_begin({reinterpret_cast<uint64_t>(s), id,
                static_cast<uint8_t>(contextSerial)});
            ++openGpu;
            o.gpu = true;
        }
    }
}
extern "C" void VR_TracyEnd()
{
    if(thread.overflow) { --thread.overflow; return; }
    if(!thread.depth) return;
    const Open o = thread.stack[--thread.depth];
    if(o.gpu && gpuReady && connection == o.connection &&
        tracy::GetProfiler().ConnectionId() == o.connection && TracyCIsConnected)
    {
        ___tracy_emit_gpu_zone_end({query(), static_cast<uint8_t>(contextSerial)});
        --openGpu;
    }
    if(o.cpu.active) ___tracy_emit_zone_end(o.cpu);
}
extern "C" void VR_TracyAbort()
{
    if(thread.depth || thread.overflow) abortedByError = true;
    while(thread.depth || thread.overflow) VR_TracyEnd();
    qcDepth = qcOverflow = 0;
    memoryTransitions = 0;
}
extern "C" void VR_TracyFrame()
{
    // Previous frame may have left explicit C scopes open after a non-local exit.
    // Count them: a leak outside Host_Error recovery shows up in vr_tracy_status.
    if(thread.depth || thread.overflow) { if(!abortedByError) ++forceClosed; }
    abortedByError = false;
    VR_TracyAbort();
    vr_tracy_physics_zones = physicsDetail.value != 0.f;
    vr_tracy_render_zones = renderDetail.value != 0.f;
    vr_tracy_gpu_zones = TracyCIsConnected && gpuReady && gpuEnabled.value != 0.f && gpuDetail.value != 0.f;
    TracyCFrameMark;
    memorySync();
    if(TracyCIsConnected)
    {
        std::lock_guard lock{boxLock};
        boxSync();
    }
    if(TracyCIsConnected)
    {
        VR_TracyPlot("VR display period ms", displayPeriodMs);
        VR_TracyPlot("VR missed display periods", missedDisplayPeriods);
        VR_TracyPlot("Draw calls", vr_profcounts.drawcalls);
        // The last frame's counts (vr_profcounts: cleared after this, at VR_ProfileFrame). Ironwail's rs_brushpolys and
        // rs_skypolys are never counted (the GPU culls the world), and rs_aliaspolys only under r_speeds.
        VR_TracyPlot("World PVS leaves", vr_profcounts.worldleaves);
        VR_TracyPlot("Alias triangles", vr_profcounts.aliastris);
        VR_TracyPlot("Particles", r_numactiveparticles + qvr::particles::liveCount());
        VR_TracyPlot("Decals", qvr::decals::liveCount());
        // The always-on phase timers' last frame (vr_profile.hpp FrameSample): the CPU work (the host frame less the
        // runtime's and the swap's waits) and the eyes' GPU time, which is read back a few frames late (the newest known).
        qvr::profile::FrameSample sample;
        if(qvr::profile::frameSample(0, sample))
        {
            VR_TracyPlot("Frame CPU busy ms", sample.cpuMs);
            VR_TracyPlot("Frame period ms", sample.periodMs);
        }
        for(int back = 0; back < 8 && qvr::profile::frameSample(back, sample); ++back)
            if(sample.gpuMs >= 0.f)
            {
                VR_TracyPlot("GPU eyes ms", sample.gpuMs);
                break;
            }
        const auto session = tracy::GetProfiler().ConnectionId();
        if(metadataSession != session || std::strcmp(metadataMap, cl.mapname) != 0)
        {
            metadataSession = session;
            q_strlcpy(metadataMap, cl.mapname, sizeof(metadataMap));
            VR_TracyBeginAt("Capture context", __FILE__, __LINE__);
            VR_TracyText(cl.mapname);
            VR_TracyText(com_gamedir);
            VR_TracyText(gl_renderer ? gl_renderer : "unknown GL renderer");
            char value[512];
            const char* campaign = VR_ActiveCampaignFolder();
            q_snprintf(value, sizeof(value), "campaign=%s native_launch=%d", campaign ? campaign : "external/default", VR_IsNativeCampaignLaunch());
            VR_TracyText(value);
            for(const char* name : qvr::profile::settingCvars)
                if(const auto* v = Cvar_FindVar(name))
                {
                    q_snprintf(value, sizeof(value), "%s=%s", name, v->string);
                    VR_TracyText(value);
                }
            for(const char* name : {"vr_ragdolls", "vr_jobs_threads", "vr_tracy_qc_detail", "vr_tracy_memory_detail", "vr_tracy_gpu", "vr_tracy_gpu_detail", "vr_tracy_physics_detail", "vr_tracy_render_detail", "vr_profile"})
                if(const auto* v = Cvar_FindVar(name))
                {
                    q_snprintf(value, sizeof(value), "%s=%s", name, v->string);
                    VR_TracyText(value);
                }
            for(const cvar_t* v = Cvar_FindVarAfter("", 0); v; v = Cvar_FindVarAfter(v->name, 0))
                if(std::strncmp(v->name, "vr_pbr", 6) == 0 || std::strncmp(v->name, "vr_parallax", 11) == 0)
                {
                    q_snprintf(value, sizeof(value), "%s=%s", v->name, v->string);
                    VR_TracyText(value);
                }
            if(auto* be = qvr::backend())
            {
                int w{}, h{}; be->eyeResolution(w, h);
                q_snprintf(value, sizeof(value), "backend=%s runtime=%s eyes=%dx%d window=%dx%d", be->name(), be->runtimeName(), w, h, vid.width, vid.height);
                VR_TracyText(value);
            }
            VR_TracyEnd();
        }
    }
    if(TracyCIsConnected && ++resourceFrame % 60 == 0)
    {
        Memory_TracyPlots();
        qvr::ragdoll::tracyPlots();
#ifdef _WIN32
        ProcessMemoryCounters pmc{};
        pmc.cb = sizeof(pmc);
        if(K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            VR_TracyPlot("Process working set bytes", static_cast<double>(pmc.workingSetSize));
            VR_TracyPlot("Process private bytes", static_cast<double>(pmc.privateUsage));
        }
#endif
        VR_TracyPlot("Texture RGBA estimate bytes", TexMgr_TracyBytes());
        const auto totals = qvr::mem::totals();
        VR_TracyPlot("VR scratch bytes", static_cast<double>(totals.scratchBytes));
        VR_TracyPlot("VR cache bytes", static_cast<double>(totals.cacheBytes));
        // Scratch/cache registrations can share a system label; emit its total, not alternating values.
        std::unordered_map<std::string_view, double> systemBytes;
        for(auto* r = qvr::mem::Registration::first(); r; r = r->next())
            systemBytes[r->system()] += static_cast<double>(r->bytes());
        for(const auto& [system, bytes] : systemBytes) VR_TracyPlot(system.data(), bytes);
        const auto jobs = qvr::jobs::stats();
        VR_TracyPlot("Jobs executed", static_cast<double>(jobs.tasks));
        VR_TracyPlot("Jobs claimed by waiter", static_cast<double>(jobs.claimedByWaiter));
        VR_TracyPlot("Jobs parallel loops", static_cast<double>(jobs.loops));
        VR_TracyPlot("Server edicts", sv.qcvm.num_edicts);
        VR_TracyPlot("Client visible entities", cl_numvisedicts);
    }
    if(!TracyCIsConnected)
    {
        gpuReady = false;
        connection = 0;
        head = tail = pending = openGpu = 0;
        return;
    }
    const uint64_t session = tracy::GetProfiler().ConnectionId();
    if(gpuEnabled.value && (!gpuReady || connection != session)) gpuConnect(session);
    if(gpuReady) collect();
}
extern "C" void VR_TracyDisplay(double ms, int missed) { displayPeriodMs = ms; missedDisplayPeriods = missed; }
extern "C" void VR_TracyBoxAlloc(const void* ptr, unsigned long long bytes)
{
    if(!ptr) return;
    std::lock_guard lock{boxLock};
    boxSync();
    if(boxLive.emplace(ptr, bytes).second)
    {
        boxBytes += bytes;
        if(boxSession && TracyCIsConnected && boxSession == tracy::GetProfiler().ConnectionId())
            ___tracy_emit_memory_alloc(ptr, static_cast<size_t>(bytes));
    }
}
extern "C" void VR_TracyBoxFree(const void* ptr)
{
    if(!ptr) return;
    std::lock_guard lock{boxLock};
    boxSync();
    const auto it = boxLive.find(ptr);
    if(it == boxLive.end()) return;
    if(boxSession && TracyCIsConnected && boxSession == tracy::GetProfiler().ConnectionId()) ___tracy_emit_memory_free(ptr);
    boxBytes -= it->second;
    boxLive.erase(it);
}
extern "C" int VR_TracyConnected() { return TracyCIsConnected; }
extern "C" void VR_TracyPlot(const char* name, double value)
{
    if(TracyCIsConnected) ___tracy_emit_plot(name, value);
}
extern "C" void VR_TracyBeginAt(const char* name, const char* file, unsigned line)
{
    if(thread.depth == maxDepth) { ++thread.overflow; dropped.fetch_add(1, std::memory_order_relaxed); return; }
    Open& o = thread.stack[thread.depth++];
    o = {};
    if(!TracyCIsConnected) return;
    const auto loc = ___tracy_alloc_srcloc_name(line, file, std::strlen(file), "QVR", 3, name, std::strlen(name), 0);
#ifdef TRACY_CALLSTACK
    o.cpu = ___tracy_emit_zone_begin_alloc_callstack(loc, TRACY_CALLSTACK, 1);
#else
    o.cpu = ___tracy_emit_zone_begin_alloc(loc, 1);
#endif
    if(o.cpu.active) o.connection = o.cpu.connectionId;
    else o.cpu = {};
}
namespace
{
// A zone at a persistent source location (no allocation); callstacks only where the build has them and stack is set.
void beginStatic(const ___tracy_source_location_data* data, [[maybe_unused]] bool stack)
{
    if(thread.depth == maxDepth) { ++thread.overflow; dropped.fetch_add(1, std::memory_order_relaxed); return; }
    Open& o = thread.stack[thread.depth++];
    o = {};
    o.name = data->name;
    if(!TracyCIsConnected) return;
#ifdef TRACY_CALLSTACK
    o.cpu = stack ? ___tracy_emit_zone_begin_callstack(data, TRACY_CALLSTACK, 1) : ___tracy_emit_zone_begin(data, 1);
#else
    o.cpu = ___tracy_emit_zone_begin(data, 1);
#endif
    if(o.cpu.active) o.connection = o.cpu.connectionId;
    else o.cpu = {};
}

// QC zones (vr_tracy_qc_detail). Each QC function and builtin of a VM gets one persistent source location when its progs
// load (VR_TracyQCProgs, server and client VMs, every load), interned by name, kind and file: Tracy reads a location
// lazily on its own thread, so none is ever freed, and a map change reuses the same ones (one row per function across
// loads). A call then costs a table lookup and Tracy's plain zone begin/end: no allocation, no string copy, no zone
// text, and never a callstack (it would be the interpreter's own loop, the same for every call). The kind is the
// location's function field ("QC function" / "QC builtin"); the progs' source file is its file.
// Levels: 1 QC functions; 2 + the builtins not listed below; 3 every builtin too (the old coverage).
// Trivial builtins, zoned only at level 3: called often and done in well under a microsecond, so their zones cost
// about what they measure and add nothing to a timing. Picked from level-3 captures of combat_48, play_e1m1_fight and
// play_ad_tfuma_fight by calls a frame x mean time (docs/vr-port/TRACY.md, "QC zones"). Matched by name: the progs'
// name, else the engine's builtin table's (fteqcc strips most builtin names), else "builtin #N".
// Measured (2026-10-16, level 3, mean ns including the zone): cvar_hget 16, max 20, vlen 20, min 24, isragdoll 23,
// makevectors 69, fabs 14, nextent 28, normalize 24, anglemod 29, random 28, rint 30, vectoyaw 55, cos/sin 45,
// makeforward 80, ceil 68, ChangeYaw 41; together most of the builtin calls. Their unmeasured siblings (same kind
// of work) are listed too. Not trivial (kept at 2): cvar (163 ns, a string lookup), traces, finds, sounds, movement.
constexpr std::string_view qcTrivialBuiltins[] = {
    // vector and scalar math
    "makevectors", "makeforward", "normalize", "vlen", "vectoyaw", "vectoangles", "anglemod", "ChangeYaw", "random",
    "fabs", "floor", "ceil", "rint", "bound", "min", "max", "pow", "sqrt", "sin", "cos",
    // conversions
    "ftos", "vtos", "etos", "stof",
    // cached lookups
    "cvar_hget", "nextent", "isragdoll",
};
constexpr const char* qcFunctionKind = "QC function";
constexpr const char* qcBuiltinKind = "QC builtin";
struct QCEntry
{
    const ___tracy_source_location_data* loc{};
    float level{}; // the lowest vr_tracy_qc_detail that zones it
};
struct QCTable
{
    const qcvm_t* vm{};
    const dfunction_t* functions{};
    int count{};
    std::vector<QCEntry> entries;
};
QCTable qcTables[3]; // one per VM (server, client); a spare slot
std::deque<std::string> qcStrings; // backing store of the locations' names and files (stable addresses)
std::deque<___tracy_source_location_data> qcLocations;
std::unordered_map<std::string, const ___tracy_source_location_data*> qcInterned;
std::string qcKey;

const char* qcString(const char* text)
{
    return qcStrings.emplace_back(text).c_str();
}
const ___tracy_source_location_data* qcLocation(const char* name, const char* kind, const char* file)
{
    qcKey.assign(name).append(1, '\x1f').append(kind).append(1, '\x1f').append(file);
    if(const auto it = qcInterned.find(qcKey); it != qcInterned.end()) return it->second;
    const auto* loc = &qcLocations.emplace_back(___tracy_source_location_data{qcString(name), kind, qcString(file), 0, 0});
    qcInterned.emplace(qcKey, loc);
    return loc;
}
const char* qcBuiltinName(const dfunction_t& f, char (&buffer)[32])
{
    const char* name = PR_GetString(f.s_name);
    if(name && *name) return name;
    for(int j = 0; j < pr_numbuiltindefs; j++)
        if(pr_builtindefs[j].number == -f.first_statement) return pr_builtindefs[j].name;
    q_snprintf(buffer, sizeof(buffer), "builtin #%d", -f.first_statement);
    return buffer;
}
} // namespace
extern "C" void VR_TracyBeginLoc(const struct vr_tracy_loc* loc)
{
    beginStatic(reinterpret_cast<const ___tracy_source_location_data*>(loc), true);
}
extern "C" void VR_TracyQCProgs()
{
    QCTable* table = nullptr;
    for(QCTable& t : qcTables)
        if(t.vm == qcvm) table = &t;
    for(QCTable& t : qcTables)
        if(!table && !t.vm) table = &t;
    if(!table) table = &qcTables[std::size(qcTables) - 1];
    table->vm = qcvm;
    table->functions = qcvm->functions;
    table->count = qcvm->progs ? qcvm->progs->numfunctions : 0;
    table->entries.assign(static_cast<size_t>(table->count), {});
    for(int i = 0; i < table->count; i++)
    {
        const dfunction_t& f = qcvm->functions[i];
        const char* file = PR_GetString(f.s_file);
        if(!file) file = "";
        if(f.first_statement >= 0)
        {
            const char* name = PR_GetString(f.s_name);
            table->entries[i] = {qcLocation(name && *name ? name : "?", qcFunctionKind, file), 1};
            continue;
        }
        char buffer[32];
        const char* name = qcBuiltinName(f, buffer);
        const bool trivial = std::find(std::begin(qcTrivialBuiltins), std::end(qcTrivialBuiltins), name) != std::end(qcTrivialBuiltins);
        table->entries[i] = {qcLocation(name, qcBuiltinKind, file), trivial ? 3.f : 2.f};
    }
}
extern "C" void VR_TracyQC(const void* function)
{
    if(qcDepth >= 1024) { ++qcOverflow; return; } // include nested VM / builtin reentry
    const ___tracy_source_location_data* loc = nullptr;
    if(qcDetail.value > 0 && TracyCIsConnected)
        for(const QCTable& t : qcTables)
            if(t.vm == qcvm)
            {
                const ptrdiff_t i = static_cast<const dfunction_t*>(function) - t.functions;
                if(t.functions == qcvm->functions && i >= 0 && i < t.count && t.entries[i].level <= qcDetail.value)
                    loc = t.entries[i].loc;
                break;
            }
    qcOpen[qcDepth++] = loc != nullptr;
    if(loc) beginStatic(loc, false);
}
extern "C" void VR_TracyQCEnd()
{
    if(qcOverflow) { --qcOverflow; return; }
    if(qcDepth && qcOpen[--qcDepth]) VR_TracyEnd();
}
extern "C" void VR_TracyAlloc(const void* ptr, unsigned long long bytes, int pool, int end)
{
    memorySync();
    if(!memoryActive || !ptr || !TracyCIsConnected || memorySession != tracy::GetProfiler().ConnectionId()) return;
    const auto [it, inserted] = allocations.emplace(ptr, Allocation{bytes, pool, end});
    if(inserted)
    {
        ___tracy_emit_memory_alloc_named(ptr, static_cast<size_t>(bytes), poolNames[pool]);
        poolAnnounced[pool] = true;
    }
}
extern "C" void VR_TracyFree(const void* ptr)
{
    memorySync();
    if(!memoryActive || !TracyCIsConnected || memorySession != tracy::GetProfiler().ConnectionId()) return;
    const auto it = allocations.find(ptr);
    if(it == allocations.end()) return;
    ___tracy_emit_memory_free_named(ptr, poolNames[it->second.pool]);
    allocations.erase(it);
}
extern "C" int VR_TracyCheckPool(int pool, unsigned long long count, unsigned long long bytes)
{
    if(!memoryActive) return 1;
    unsigned long long capturedCount{}, capturedBytes{};
    for(const auto& [ptr, a] : allocations)
        if(a.pool == pool) { ++capturedCount; capturedBytes += a.bytes; }
    const bool pass = count == capturedCount && bytes == capturedBytes;
    if(!pass) ++memoryMismatches;
    constexpr const char* countNames[] = {"Zone live blocks", "Hunk live blocks", "Cache live blocks"};
    constexpr const char* bytesNames[] = {"Zone physical bytes", "Hunk physical bytes", "Cache physical bytes"};
    constexpr const char* capturedNames[] = {"Tracy Zone captured bytes", "Tracy Hunk captured bytes", "Tracy Cache captured bytes"};
    VR_TracyPlot(countNames[pool], static_cast<double>(count));
    VR_TracyPlot(bytesNames[pool], static_cast<double>(bytes));
    VR_TracyPlot(capturedNames[pool], static_cast<double>(capturedBytes));
    VR_TracyPlot("Tracy pool mismatches", memoryMismatches);
    Con_Printf("Tracy pool %s: %s live %llu/%llu bytes %llu/%llu\n", poolNames[pool], pass ? "PASS" : "FAIL", capturedCount, count, capturedBytes, bytes);
    return pass;
}
extern "C" void VR_TracyHunkReset(int mark)
{
    memorySync();
    if(!memoryActive || !TracyCIsConnected || memorySession != tracy::GetProfiler().ConnectionId()) return;
    for(auto it = allocations.begin(); it != allocations.end();)
        if(it->second.pool == 1 && it->second.end > mark)
        {
            ___tracy_emit_memory_free_named(it->first, poolNames[1]);
            it = allocations.erase(it);
        }
        else ++it;
}
extern "C" void VR_TracyAllocationWindow(int enter)
{
    if(enter)
    {
        memorySync(); // old headers are still valid before the transition
        ++memoryTransitions;
    }
    else
    {
        if(memoryTransitions) --memoryTransitions;
        memorySync(); // new header/cache state is now complete, including a late connection
    }
}
extern "C" void VR_TracyGLShutdown()
{
    VR_TracyAbort();
    if(queries[0] && del) del(queryCount, queries);
    std::memset(queries, 0, sizeof(queries));
    gpuReady = false;
    connection = 0;
    head = tail = pending = openGpu = 0;
}
#endif
