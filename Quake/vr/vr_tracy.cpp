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
#include <atomic>
#include <cstring>

namespace
{
constexpr unsigned maxDepth = 256;
constexpr unsigned maxNames = 1024;
constexpr unsigned queryCount = 4096;
struct Open
{
    TracyCZoneCtx cpu{};
    uint64_t connection{};
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
SDL_threadID mainThread{};
cvar_t gpuEnabled = {"vr_tracy_gpu", "1", CVAR_NONE};
cvar_t gpuDetail = {"vr_tracy_gpu_detail", "0", CVAR_NONE};
unsigned queries[queryCount]{};
unsigned head{}, tail{}, pending{}, openGpu{}, contextSerial{};
uint64_t connection{};
bool gpuReady{};
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
    return std::strcmp(name, "eye L") == 0 || std::strcmp(name, "eye R") == 0 ||
        std::strcmp(name, "window view") == 0 || std::strcmp(name, "shadow maps") == 0;
}
void status()
{
    Con_Printf("Tracy 0.14.1: %s, CPU depth %u, dropped %u, GPU %s detail %.0f pending %u context %u\n",
        TracyCIsConnected ? "connected" : "waiting on localhost:8086", thread.depth,
        dropped.load(std::memory_order_relaxed), gpuReady ? "ready" : "idle", gpuDetail.value,
        pending, contextSerial);
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
    Cmd_AddCommand("vr_tracy_status", status);
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
    if(!TracyCIsConnected) return;
    const auto* s = source(name);
    if(!s) return;
    o.cpu = ___tracy_emit_zone_begin(s, 1);
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
    while(thread.depth || thread.overflow) VR_TracyEnd();
}
extern "C" void VR_TracyFrame()
{
    // Previous frame may have left explicit C scopes open after a non-local exit.
    VR_TracyAbort();
    TracyCFrameMark;
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
