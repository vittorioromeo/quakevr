// vr_main.cpp -- Quake VR module lifetime, core cvars and per-frame update.

#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_decals.hpp"
#include "vr_lighting.hpp"
#include "vr_backend.hpp"
#include "vr_throw.hpp"
#include "vr_client.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_lines.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_profile.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"
#include "vr_voicenotes.hpp"
#include "vr_flashlight.hpp"
#include "vr_weapons.hpp"

#include <cmath>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

using namespace qvr;

extern "C" void TexMgr_MemStats(int* count, int* normalmaps, double* megabytes); // gl_texmgr.c
namespace qvr::gfx
{
extern int targetsMade; // vr_gfx_gl.cpp
}

namespace
{

struct State
{
    std::unique_ptr<qvr::Backend> backend;
    qvr::TrackingState tracking;
    qvr::FrameState frame;
    bool restartRequested{false};
};

State* state = nullptr;

[[nodiscard]] std::unique_ptr<qvr::Backend> createBackend(const char* name)
{
    if(!strcmp(name, "mock"))
    {
        return qvr::makeMockBackend();
    }

    if(!strcmp(name, "openxr"))
    {
        if(auto backend = qvr::makeOpenXrBackend())
        {
            return backend;
        }

        Con_Warning("VR: this build has no OpenXR support\n");
        return nullptr;
    }

    Con_Warning("VR: unknown backend \"%s\" (available: openxr, mock)\n", name);
    return nullptr;
}

void stopBackend()
{
    if(!state->backend)
    {
        return;
    }

    Con_Printf("VR: stopping %s backend\n", state->backend->name());
    state->backend->stop();
    state->backend.reset();
}

void startBackend()
{
    std::unique_ptr<qvr::Backend> backend = createBackend(vr_backend.string);
    if(!backend)
    {
        return;
    }

    if(!backend->start())
    {
        Con_Warning("VR: failed to start %s backend (vr_restart to retry)\n", backend->name());
        backend->stop();
        return;
    }

    Con_Printf("VR: started %s backend\n", backend->name());
    state->backend = std::move(backend);
}

void onBackendSettingChanged(cvar_t* /* var */)
{
    // Applied at the start of the next frame, not from inside the cvar callback, so that
    // settings loaded from config.cfg before video init are handled uniformly.
    if(state)
    {
        state->restartRequested = true;
    }
}

void VR_Restart_f()
{
    if(state)
    {
        state->restartRequested = true;
    }
}

// quake.rc's last command: with VR enabled, start in the vrstart hub (tutorial, settings and
// the mission packs' portals) as the old engine did; otherwise play the attract demos. A map
// or demo started from the command line runs instead of either.
void VR_StartGame_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }

    if(vr_enabled.value && !sv.active && !cls.demoplayback && cls.state != ca_connected)
    {
        Cbuf_InsertText("maxplayers 1; deathmatch 0; coop 0; map vrstart\n");
        return;
    }

    Cbuf_InsertText("startdemos demo1 demo2 demo3\n");
}

void printPose(const char* label, const qvr::Pose& pose)
{
    Con_Printf("  %-5s %s pos (%.2f %.2f %.2f) rot (%.2f %.2f %.2f %.2f)\n", label,
        pose.valid ? "valid  " : "invalid", pose.position.x, pose.position.y,
        pose.position.z, pose.orientation.w, pose.orientation.x,
        pose.orientation.y, pose.orientation.z);
}

void VR_Status_f()
{
    if(!state->backend)
    {
        Con_Printf("VR: inactive (vr_enabled %s, vr_backend \"%s\")\n",
            vr_enabled.string, vr_backend.string);
        return;
    }

    Con_Printf("VR: active, backend \"%s\" (%s), world scale %g\n",
        state->backend->name(), state->backend->runtimeName(), vr_world_scale.value);

    // The eyes: the size rendered at (vr_render_scale times the image's), and the images handed to
    // the runtime (its recommended size, SteamVR's resolution included; its largest).
    const qvr::EyeSizes sizes = state->backend->eyeSizes();
    const int renderWidth = qvr::scaledEyeSize(sizes.width, sizes.maxWidth);
    const int renderHeight = qvr::scaledEyeSize(sizes.height, sizes.maxHeight);
    const double pixels = static_cast<double>(renderWidth) * renderHeight;
    const double imagePixels = static_cast<double>(sizes.width) * sizes.height;
    Con_Printf("  eyes  rendered %dx%d (vr_render_scale %g: %.0f%% of the image's pixels), images %dx%d (recommended "
               "%dx%d, largest %dx%d)\n",
        renderWidth, renderHeight, vr_render_scale.value, imagePixels > 0.0 ? 100.0 * pixels / imagePixels : 0.0,
        sizes.width, sizes.height, sizes.recommendedWidth, sizes.recommendedHeight, sizes.maxWidth, sizes.maxHeight);

    // The lenses' hidden area (vr_visibility_mask): its share of the image, and its bounds in the
    // eye's tangent space against the eye's field of view (they should lie within it).
    for(int eye = 0; eye < 2; eye++)
    {
        const qvr::HiddenArea* h = state->backend->hiddenArea(eye);
        if(!h)
        {
            Con_Printf("  hidden area: not given by the runtime (no XR_KHR_visibility_mask)\n");
            break;
        }
        const qvr::Fov& fov = state->frame.eyes[eye].fov;
        const float l = std::tan(fov.left), r = std::tan(fov.right), u = std::tan(fov.up), d = std::tan(fov.down);
        double area = 0.0;
        glm::vec2 lo{1e9f}, hi{-1e9f};
        for(std::size_t i = 0; i + 2 < h->indices.size(); i += 3)
        {
            const glm::vec2 a = h->vertices[h->indices[i]];
            const glm::vec2 b = h->vertices[h->indices[i + 1]];
            const glm::vec2 c = h->vertices[h->indices[i + 2]];
            area += 0.5 * std::fabs(static_cast<double>((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)));
        }
        for(const glm::vec2& v : h->vertices)
        {
            lo = glm::min(lo, v);
            hi = glm::max(hi, v);
        }
        const double image = static_cast<double>(r - l) * static_cast<double>(u - d);
        Con_Printf("  hidden area %c: %zu triangles, %.1f%% of the image (%s); x %.2f..%.2f y %.2f..%.2f, view %.2f..%.2f "
                   "%.2f..%.2f\n",
            eye == 0 ? 'L' : 'R', h->indices.size() / 3, image > 0.0 ? 100.0 * area / image : 0.0,
            vr_visibility_mask.value != 0.f ? "masked" : "vr_visibility_mask 0", h->vertices.empty() ? 0.f : lo.x,
            h->vertices.empty() ? 0.f : hi.x, h->vertices.empty() ? 0.f : lo.y, h->vertices.empty() ? 0.f : hi.y, l, r, d, u);
    }
    printPose("head", state->tracking.head);
    printPose("off", state->tracking.hands[qvr::HAND_OFF]);
    printPose("main", state->tracking.hands[qvr::HAND_MAIN]);

    const hands::State& hs = hands::current();
    if(hs.valid)
    {
        for(int h : {qvr::HAND_OFF, qvr::HAND_MAIN})
        {
            Con_Printf("  %-5s angles (%.1f %.1f %.1f), hotspot %d%s%s\n", h == qvr::HAND_MAIN ? "main" : "off",
                hs.rot[h].x, hs.rot[h].y, hs.rot[h].z, hs.hotspot[h], client::grabbing(h) ? ", grabbing" : "",
                twohand::helping(h) ? ", helping two-handed" : "");
        }
        Con_Printf("  two-handed aiming: %s\n", twohand::aiming() ? "yes" : "no");
    }
}

// ---- vr_memstats: what the game holds, to tell a leak across map loads ---------------------------

#ifdef _WIN32
// GetProcessMemoryInfo (kernel32's K32 export), declared here rather than through <windows.h>.
struct ProcessMemoryCounters
{
    unsigned long cb;
    unsigned long pageFaultCount;
    std::size_t peakWorkingSetSize, workingSetSize, quotaPeakPagedPoolUsage, quotaPagedPoolUsage,
        quotaPeakNonPagedPoolUsage, quotaNonPagedPoolUsage, pagefileUsage, peakPagefileUsage, privateUsage;
};
extern "C" __declspec(dllimport) void* __stdcall GetCurrentProcess();
extern "C" __declspec(dllimport) int __stdcall K32GetProcessMemoryInfo(void* process, ProcessMemoryCounters* counters,
    unsigned long size);
#endif

// Live GL objects of a kind: the names that are objects, from 1 until 4096 in a row are not.
using GlIsFn = GLboolean(APIENTRY*)(GLuint);
int countGlObjects(GlIsFn isObject, GLuint& highest)
{
    int count = 0;
    highest = 0;
    for(GLuint name = 1, misses = 0; misses < 4096 && name < (1u << 22); name++)
    {
        if(isObject(name))
        {
            count++;
            highest = name;
            misses = 0;
        }
        else
        {
            misses++;
        }
    }
    return count;
}

struct MemSample
{
    int vramTotal{-1}, vramFree{-1}, evictions{-1}, evictedMb{-1}; // MB; -1: not reported
    double workingSet{0.0}, peakWorkingSet{0.0}, privateBytes{0.0}; // MB
    double hunk{0.0};                                               // MB
    int textures{0}, normalmaps{0};
    double textureMb{0.0};
    int glTextures{0}, buffers{-1}, framebuffers{-1}, queries{-1}, programs{-1};
};

MemSample sampleMemory()
{
    MemSample m;

    // The GPU's memory (NVIDIA: GL_NVX_gpu_memory_info, all processes'; AMD: GL_ATI_meminfo, free only).
    while(glGetError() != GL_NO_ERROR)
    {
    }
    GLint total = 0, available = 0, evictions = 0, evicted = 0;
    glGetIntegerv(0x9048, &total); // GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, KB
    if(glGetError() == GL_NO_ERROR && total > 0)
    {
        glGetIntegerv(0x9049, &available); // GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
        glGetIntegerv(0x904A, &evictions); // GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX
        glGetIntegerv(0x904B, &evicted);   // GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX
        m.vramTotal = total / 1024;
        m.vramFree = available / 1024;
        m.evictions = evictions;
        m.evictedMb = evicted / 1024;
    }
    else
    {
        GLint ati[4] = {};
        glGetIntegerv(0x87FC, ati); // GL_TEXTURE_FREE_MEMORY_ATI
        if(glGetError() == GL_NO_ERROR && ati[0] > 0)
        {
            m.vramFree = ati[0] / 1024;
        }
    }

#ifdef _WIN32
    ProcessMemoryCounters pmc{};
    pmc.cb = sizeof(pmc);
    if(K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        m.workingSet = pmc.workingSetSize / 1048576.0;
        m.peakWorkingSet = pmc.peakWorkingSetSize / 1048576.0;
        m.privateBytes = pmc.privateUsage / 1048576.0;
    }
#endif
    m.hunk = Hunk_LowMark() / 1048576.0;
    TexMgr_MemStats(&m.textures, &m.normalmaps, &m.textureMb);

    // Every live GL object of the kinds that hold memory, the engine's, the module's and the OpenXR
    // runtime's in this context (its swapchain images): a count that grows from one load of a map to
    // the next is a leak.
    static GlIsFn isBuffer = nullptr, isFramebuffer = nullptr, isQuery = nullptr, isProgram = nullptr;
    if(!isBuffer)
    {
        isBuffer = reinterpret_cast<GlIsFn>(SDL_GL_GetProcAddress("glIsBuffer"));
        isFramebuffer = reinterpret_cast<GlIsFn>(SDL_GL_GetProcAddress("glIsFramebuffer"));
        isQuery = reinterpret_cast<GlIsFn>(SDL_GL_GetProcAddress("glIsQuery"));
        isProgram = reinterpret_cast<GlIsFn>(SDL_GL_GetProcAddress("glIsProgram"));
    }
    GLuint highest = 0;
    m.glTextures = countGlObjects(glIsTexture, highest);
    const auto count = [&](GlIsFn fn) { return fn ? countGlObjects(fn, highest) : -1; };
    m.buffers = count(isBuffer);
    m.framebuffers = count(isFramebuffer);
    m.queries = count(isQuery);
    m.programs = count(isProgram);
    return m;
}

void VR_MemStats_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }
    static double lastTime = 0.0;
    static int lastFrames = 0;
    static int calls = 0;
    const double seconds = realtime - lastTime;
    const int frames = host_framecount - lastFrames;

    Con_Printf("vr_memstats #%d, map \"%s\", %.1f s since the last: %d frames, %.2f ms a frame\n", ++calls,
        cl.worldmodel ? cl.worldmodel->name : "", calls > 1 ? seconds : 0.0, calls > 1 ? frames : 0,
        calls > 1 && frames > 0 ? 1000.0 * seconds / frames : 0.0);
    lastTime = realtime;
    lastFrames = host_framecount;

    const MemSample m = sampleMemory();
    if(m.vramTotal > 0)
    {
        Con_Printf("  VRAM  %d MB used of %d (all processes), %d MB free; %d evictions (%d MB) so far\n",
            m.vramTotal - m.vramFree, m.vramTotal, m.vramFree, m.evictions, m.evictedMb);
    }
    else if(m.vramFree > 0)
    {
        Con_Printf("  VRAM  %d MB free for textures\n", m.vramFree);
    }
    else
    {
        Con_Printf("  VRAM  not reported by this driver\n");
    }
#ifdef _WIN32
    Con_Printf("  RAM   working set %.1f MB (peak %.1f), private %.1f MB\n", m.workingSet, m.peakWorkingSet, m.privateBytes);
#endif
    Con_Printf("  hunk  %.1f MB used\n", m.hunk);
    Con_Printf("  textures (managed) %d, %d of them normal maps, %.1f MB\n", m.textures, m.normalmaps, m.textureMb);
    Con_Printf("  GL    %d textures (%d not managed), %d buffers, %d framebuffers, %d queries, %d programs\n", m.glTextures,
        m.glTextures - m.textures, m.buffers, m.framebuffers, m.queries, m.programs);
    Con_Printf("  VR    render targets (re)made %d times so far; ", gfx::targetsMade);
    decals::count_f();
}

// vr_memstats_log: the same, as a row of quakevr/profile/memstats_<date>.csv every so many seconds
// and once after each map load, with the frame rate since the last row: a session's slowdown next to
// what the game (and, in the VRAM columns, every other program) holds.
struct MemLog
{
    std::string path;
    double lastTime{0.0};
    int lastFrames{0};
    const void* lastWorld{nullptr};
    double worldSince{0.0};
};

MemLog memLog;

void writeMemLogRow(const char* reason)
{
    if(memLog.path.empty())
    {
        const std::time_t now = std::time(nullptr);
        char stamp[64];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
        const std::string dir = std::string{com_gamedir} + "/profile";
        Sys_mkdir(dir.c_str());
        memLog.path = dir + "/memstats_" + stamp + ".csv";
        if(FILE* f = std::fopen(memLog.path.c_str(), "w"))
        {
            std::fprintf(f, "clock,seconds,reason,map,frames,ms_per_frame,vram_used_mb,vram_total_mb,vram_free_mb,"
                            "evictions,evicted_mb,working_set_mb,peak_working_set_mb,private_mb,hunk_mb,textures,"
                            "normal_maps,texture_mb,gl_textures,gl_buffers,gl_framebuffers,gl_queries,gl_programs,"
                            "targets_made\n");
            std::fclose(f);
        }
        Con_DPrintf("vr_memstats_log: %s\n", memLog.path.c_str());
    }
    FILE* f = std::fopen(memLog.path.c_str(), "a");
    if(!f)
    {
        return;
    }
    const double seconds = realtime - memLog.lastTime;
    const int frames = host_framecount - memLog.lastFrames;
    memLog.lastTime = realtime;
    memLog.lastFrames = host_framecount;

    const MemSample m = sampleMemory();
    const std::time_t now = std::time(nullptr);
    char clock[32];
    std::strftime(clock, sizeof(clock), "%H:%M:%S", std::localtime(&now));
    std::fprintf(f, "%s,%.1f,%s,%s,%d,%.3f,%d,%d,%d,%d,%d,%.1f,%.1f,%.1f,%.1f,%d,%d,%.1f,%d,%d,%d,%d,%d,%d\n", clock,
        realtime, reason, cl.worldmodel ? cl.worldmodel->name : "", frames, frames > 0 ? 1000.0 * seconds / frames : 0.0,
        m.vramTotal > 0 ? m.vramTotal - m.vramFree : -1, m.vramTotal, m.vramFree, m.evictions, m.evictedMb, m.workingSet,
        m.peakWorkingSet, m.privateBytes, m.hunk, m.textures, m.normalmaps, m.textureMb, m.glTextures, m.buffers,
        m.framebuffers, m.queries, m.programs, gfx::targetsMade);
    std::fclose(f);
}

void memLogFrame()
{
    if(vr_memstats_log.value <= 0.f || cls.state != ca_connected || cls.signon != SIGNONS || !cl.worldmodel)
    {
        return;
    }
    // A new map: a row 5 seconds in (its textures made, the first frames' hitches past).
    if(cl.worldmodel != memLog.lastWorld)
    {
        memLog.lastWorld = cl.worldmodel;
        memLog.worldSince = realtime;
        return;
    }
    if(memLog.worldSince > 0.0 && realtime - memLog.worldSince >= 5.0)
    {
        memLog.worldSince = 0.0;
        writeMemLogRow("map");
        return;
    }
    if(realtime - memLog.lastTime >= q_max(vr_memstats_log.value, 5.f))
    {
        writeMemLogRow("timer");
    }
}

} // namespace

namespace qvr
{

const TrackingState& tracking()
{
    static const TrackingState fallback = standingPose();
    return state && state->backend ? state->tracking : fallback;
}

bool vrActive()
{
    return state && state->backend;
}

Backend* backend()
{
    return state ? state->backend.get() : nullptr;
}

const FrameState& frameState()
{
    static const FrameState none;
    return state && state->backend ? state->frame : none;
}

int scaledEyeSize(int image, int max)
{
    const float scale = CLAMP(0.25f, vr_render_scale.value, 2.f);
    int size = static_cast<int>(std::lround(static_cast<double>(image) * scale));
    if(max > 0)
    {
        size = q_min(size, max);
    }
    return q_max(size, 16);
}

} // namespace qvr

extern "C" void VR_Init()
{
    state = new State{};

    registerCvars();
    weapons::registerCvars();
    Cvar_SetCallback(&vr_enabled, onBackendSettingChanged);
    Cvar_SetCallback(&vr_backend, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime_json, onBackendSettingChanged);

    Cmd_AddCommand("vr_status", VR_Status_f);
    Cmd_AddCommand("vr_restart", VR_Restart_f);
    Cmd_AddCommand("menu_vr", menu::command_f);
    Cmd_AddCommand("vr_startgame", VR_StartGame_f);
    registerMockCommands();
    input::init();
    voicenotes::init();
    flashlight::init();
    client::init();
    server::init();
    Cmd_AddCommand("vr_dumpview", view::dumpView_f);
    anchor::registerCommands();
    Cmd_AddCommand("vr_decal_count", decals::count_f);
    Cmd_AddCommand("vr_memstats", VR_MemStats_f);
    lighting::init();
    profile::init();

    state->restartRequested = true;
}

extern "C" void VR_Shutdown()
{
    if(!state)
    {
        return;
    }

    voicenotes::shutdown();
    stopBackend();
    delete state;
    state = nullptr;
}

extern "C" void VR_BeginFrame()
{
    if(!state || cls.state == ca_dedicated)
    {
        return;
    }

    if(state->restartRequested)
    {
        state->restartRequested = false;
        stopBackend();

        if(vr_enabled.value)
        {
            startBackend();
        }
    }

    profile::begin("xr wait", false); // the runtime's pacing (xrWaitFrame) and the tracking
    const bool began = !state->backend || state->backend->beginFrame(state->tracking, state->frame);
    profile::end();
    if(!began)
    {
        Con_Warning("VR: %s session lost\n", state->backend->name());
        stopBackend();
    }

    lines::clear(); // queued anew every frame (teleport aim, crosshairs)
    text3d::clear();
    voicenotes::frame(); // after the clear: its indicator is queued anew each frame
    memLogFrame();
    profile::overlay();  // vr_profile 2
    throwing::filterGrips(state->tracking); // the analog grip's release, before it becomes a key
    input::update(state->tracking.input); // releases held keys when VR is off

    // Update the hands now, before the move is built (it carries the aim in the view angles).
    input::roomscaleJump(hands::current());
}

extern "C" int VR_IsActive()
{
    return vrActive();
}

extern "C" int VR_ModalMessageFrame()
{
    if(!VR_IsActive())
    {
        return 0;
    }

    // Keep the headset's frames going, showing the dialog, and the controllers' keys coming. Each is
    // a frame for the profiler too: else its GPU timer queries piled up (64 more at a time) for as
    // long as the dialog was up.
    VR_ProfileFrame();
    VR_BeginFrame();
    scr_drawdialog = true;
    SCR_UpdateScreen();
    scr_drawdialog = false;
    return 1;
}
