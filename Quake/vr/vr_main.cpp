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
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"
#include "vr_voicenotes.hpp"
#include "vr_flashlight.hpp"
#include "vr_gfx.hpp"
#include "vr_weapons.hpp"
#include "vr_particles.hpp"
#include "vr_shells.hpp"
#include "vr_worldtext.hpp"

#include <chrono>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace qvr;

extern "C" void TexMgr_MemStats(int* count, int* normalmaps, double* megabytes); // gl_texmgr.c
namespace qvr::gfx
{
extern int targetsMade; // vr_gfx_gl.cpp
}
extern "C" int r_numactiveparticles; // r_part.c

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
            Con_Printf("  %-5s angles (%.1f %.1f %.1f), weapon %d, hotspot %d%s%s%s\n", h == qvr::HAND_MAIN ? "main" : "off",
                hs.rot[h].x, hs.rot[h].y, hs.rot[h].z,
                cl.stats[h == qvr::HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2], hs.hotspot[h],
                client::grabbing(h) ? ", grabbing" : "",
                twohand::helping(h) ? ", helping two-handed" : "",
                twohand::carrying(h) ? ", carrying by the foregrip" : "");
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
    double scanMs{0.0}; // what counting the GL objects took
};

MemSample sampleMemory(bool scanGl = true)
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
    if(!scanGl)
    {
        return m;
    }
    const auto scanStart = std::chrono::steady_clock::now();
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
    m.scanMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - scanStart).count();
    return m;
}

// ---- What the frames cost, and what there is to draw (always on: vr_profile.hpp's phases) ------

// The phases' sums (profile::takePhases), for two readers: the log's rows and the console command.
void addSums(profile::PhaseSums& to, const profile::PhaseSums& from)
{
    to.frames += from.frames;
    to.hitches += from.hitches;
    to.slowFrames += from.slowFrames;
    to.periodMs += from.periodMs;
    to.periodMaxMs = q_max(to.periodMaxMs, from.periodMaxMs);
    to.hostMs += from.hostMs;
    to.gpuFrames += from.gpuFrames;
    to.gpuDropped += from.gpuDropped;
    for(int i = 0; i < profile::PhaseCount; i++)
    {
        to.cpuMs[i] += from.cpuMs[i];
        to.gpuMs[i] += from.gpuMs[i];
    }
    to.displayPeriodMs = from.displayPeriodMs;
}

// Things drawn and alive, sampled every few frames and averaged over a row.
struct FrameCounts
{
    int samples{0};
    double visedicts{0.0}, dlights{0.0}, shadowDlights{0.0}, shadowMapLights{0.0}, particles{0.0}, vrParticles{0.0},
        beams{0.0}, channels{0.0}, texts{0.0};
};

void addCounts(FrameCounts& to, const FrameCounts& from)
{
    to.samples += from.samples;
    to.visedicts += from.visedicts;
    to.dlights += from.dlights;
    to.shadowDlights += from.shadowDlights;
    to.shadowMapLights += from.shadowMapLights;
    to.particles += from.particles;
    to.vrParticles += from.vrParticles;
    to.beams += from.beams;
    to.channels += from.channels;
    to.texts += from.texts;
}

struct Readers
{
    profile::PhaseSums sums;
    FrameCounts counts;
};
Readers logReader, commandReader;

// The last frame's counts (VR_BeginFrame, before the texts are cleared), every 8th frame.
void sampleCounts()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS || (host_framecount & 7) != 0)
    {
        return;
    }
    FrameCounts c;
    c.samples = 1;
    c.visedicts = cl_numvisedicts;
    for(const dlight_t& l : cl_dlights)
    {
        c.dlights += l.die >= cl.time && l.radius > 0.f ? 1.0 : 0.0;
    }
    int dl = 0, ml = 0;
    lighting::shadowCounts(dl, ml);
    c.shadowDlights = dl;
    c.shadowMapLights = ml;
    c.particles = r_numactiveparticles;
    c.vrParticles = particles::liveCount();
    for(const beam_t& b : cl_beams)
    {
        c.beams += b.model && b.endtime >= cl.time ? 1.0 : 0.0;
    }
    for(int i = 0; i < MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS && i < total_channels; i++)
    {
        c.channels += snd_channels[i].sfx ? 1.0 : 0.0;
    }
    int texts = 0, boards = 0;
    text3d::counts(texts, boards);
    c.texts = texts;
    addCounts(logReader.counts, c);
    addCounts(commandReader.counts, c);
}

void drainPhases()
{
    const profile::PhaseSums p = profile::takePhases();
    addSums(logReader.sums, p);
    addSums(commandReader.sums, p);
}

// The server's entities: in use, and of kinds that pile up in play.
struct EdictCounts
{
    int inUse{-1}, highest{-1}, monsters{0}, corpses{0}, heads{0}, gibs{0}, missiles{0}, thrown{0};
};

EdictCounts countEdicts()
{
    EdictCounts e;
    if(!sv.active || !sv.qcvm.progs)
    {
        return e;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    e.inUse = 0;
    e.highest = qcvm->num_edicts;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* ent = EDICT_NUM(i);
        if(ent->free)
        {
            continue;
        }
        e.inUse++;
        const char* classname = PR_GetString(ent->v.classname);
        const int modelindex = static_cast<int>(ent->v.modelindex);
        const char* model = modelindex > 0 && modelindex < MAX_MODELS && sv.model_precache[modelindex]
                                ? sv.model_precache[modelindex]
                                : "";
        if(!std::strncmp(classname, "monster_", 8))
        {
            (ent->v.deadflag != 0.f || ent->v.health <= 0.f ? e.corpses : e.monsters)++;
        }
        else if(!std::strncmp(model, "progs/h_", 8))
        {
            e.heads++;
        }
        else if(!std::strncmp(model, "progs/gib", 9))
        {
            e.gibs++;
        }
        else if(!std::strcmp(classname, "thrown_weapon"))
        {
            e.thrown++;
        }
        else if(static_cast<int>(ent->v.movetype) == MOVETYPE_FLYMISSILE)
        {
            e.missiles++;
        }
    }
    PR_PopQCVM(old);
    return e;
}

// A row's (or the command's) numbers: named columns, the old ones first.
using Columns = std::vector<std::pair<std::string, std::string>>;

void column(Columns& c, const char* name, const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    c.emplace_back(name, buf);
}

// The phases' and counts' columns, per frame over what `r` gathered since it was last reset.
void timingColumns(Columns& c, const Readers& r)
{
    const profile::PhaseSums& p = r.sums;
    const double frames = q_max(p.frames, 1);
    const double gpuFrames = q_max(p.gpuFrames, 1);
    const auto cpu = [&](profile::Phase ph) { return p.cpuMs[ph] / frames; };
    const auto gpu = [&](profile::Phase ph) { return p.gpuMs[ph] / gpuFrames; };
    using namespace profile;
    const double waits = cpu(XrWait) + cpu(XrAcquire) + cpu(XrRelease) + cpu(XrSubmit) + cpu(Swap);
    column(c, "display_ms", "%.3f", p.displayPeriodMs);
    column(c, "slow_frames", "%d", p.slowFrames);
    column(c, "hitches", "%d", p.hitches);
    column(c, "period_ms", "%.3f", p.periodMs / frames);
    column(c, "period_max_ms", "%.2f", p.periodMaxMs);
    column(c, "host_ms", "%.3f", p.hostMs / frames);
    column(c, "busy_ms", "%.3f", q_max(0.0, p.hostMs / frames - waits));
    column(c, "xr_wait_ms", "%.3f", cpu(XrWait));
    column(c, "xr_waitframe_ms", "%.3f", cpu(XrWaitFrame));
    column(c, "xr_acquire_ms", "%.3f", cpu(XrAcquire));
    column(c, "xr_release_ms", "%.3f", cpu(XrRelease));
    column(c, "xr_submit_ms", "%.3f", cpu(XrSubmit));
    column(c, "swap_ms", "%.3f", cpu(Swap));
    column(c, "commands_ms", "%.3f", cpu(Commands));
    column(c, "server_ms", "%.3f", cpu(Server));
    column(c, "physics_ms", "%.3f", cpu(Physics));
    column(c, "rigid_ms", "%.3f", cpu(Rigid));
    column(c, "client_ms", "%.3f", cpu(ClientRead));
    column(c, "view_ents_ms", "%.3f", cpu(ViewEntities));
    column(c, "screen_ms", "%.3f", cpu(Screen));
    column(c, "eyes_cpu_ms", "%.3f", cpu(EyeL) + cpu(EyeR));
    column(c, "run_particles_ms", "%.3f", cpu(RunParticles));
    column(c, "sound_ms", "%.3f", cpu(Sound));
    column(c, "gpu_frames", "%d", p.gpuFrames);
    column(c, "gpu_dropped", "%d", p.gpuDropped);
    column(c, "gpu_eyes_ms", "%.3f", gpu(EyeL) + gpu(EyeR));
    column(c, "gpu_eye_l_ms", "%.3f", gpu(EyeL));
    column(c, "gpu_eye_r_ms", "%.3f", gpu(EyeR));
    column(c, "gpu_shadows_ms", "%.3f", gpu(ShadowMaps));
    column(c, "gpu_world_ms", "%.3f", gpu(WorldBrush));
    column(c, "gpu_alias_ms", "%.3f", gpu(Alias));
    column(c, "gpu_particles_ms", "%.3f", gpu(Particles));
    column(c, "gpu_vr_particles_ms", "%.3f", gpu(VrParticles));
    column(c, "gpu_decals_ms", "%.3f", gpu(Decals));
    column(c, "gpu_acquire_ms", "%.3f", gpu(XrAcquire));
    column(c, "gpu_release_ms", "%.3f", gpu(XrRelease));
    column(c, "gpu_submit_ms", "%.3f", gpu(XrSubmit));

    const FrameCounts& f = r.counts;
    const double n = q_max(f.samples, 1);
    column(c, "visedicts", "%.1f", f.visedicts / n);
    column(c, "dlights", "%.1f", f.dlights / n);
    column(c, "shadow_dlights", "%.1f", f.shadowDlights / n);
    column(c, "shadow_maplights", "%.1f", f.shadowMapLights / n);
    column(c, "particles", "%.0f", f.particles / n);
    column(c, "vr_particles", "%.0f", f.vrParticles / n);
    column(c, "beams", "%.1f", f.beams / n);
    column(c, "sound_channels", "%.1f", f.channels / n);
    column(c, "texts", "%.1f", f.texts / n);

    const EdictCounts e = countEdicts();
    column(c, "edicts", "%d", e.inUse);
    column(c, "edicts_high", "%d", e.highest);
    column(c, "monsters", "%d", e.monsters);
    column(c, "corpses", "%d", e.corpses);
    column(c, "heads", "%d", e.heads);
    column(c, "gibs", "%d", e.gibs);
    column(c, "missiles", "%d", e.missiles);
    column(c, "thrown_weapons", "%d", e.thrown);
    column(c, "cl_entities", "%d", cl.num_entities);
    column(c, "decals", "%d", decals::liveCount());
    column(c, "shells", "%d", shells::liveCount());
    column(c, "world_texts", "%d", static_cast<int>(worldtext::clientTexts().size()));
    column(c, "float_texts", "%d", static_cast<int>(worldtext::clientFloatTexts(cl.time).size()));
    int texts = 0, boards = 0;
    text3d::counts(texts, boards);
    column(c, "boards", "%d", boards);
    column(c, "static_sounds", "%d", q_max(0, total_channels - MAX_DYNAMIC_CHANNELS - NUM_AMBIENTS));
    column(c, "targets_by_name", "%s", gfx::targetsMadeByName().c_str());
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

    ++calls; // not in the call: its arguments are evaluated in no set order (MSVC: right to left)
    Con_Printf("vr_memstats #%d, map \"%s\", %.1f s since the last: %d frames, %.2f ms a frame\n", calls,
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
    Con_Printf("  GL    %d textures (%d not managed), %d buffers, %d framebuffers, %d queries, %d programs (%.1f ms to count)\n",
        m.glTextures, m.glTextures - m.textures, m.buffers, m.framebuffers, m.queries, m.programs, m.scanMs);
    Con_Printf("  VR    render targets (re)made %d times so far (%s); ", gfx::targetsMade, gfx::targetsMadeByName().c_str());
    decals::count_f();

    // Per frame since the last vr_memstats: the phases' times and the counts, as the log's columns.
    drainPhases();
    Columns c;
    timingColumns(c, commandReader);
    commandReader = Readers{};
    std::string line;
    for(const auto& [name, value] : c)
    {
        if(name == "targets_by_name")
        {
            continue;
        }
        const std::string word = name + " " + value;
        if(line.size() + word.size() + 2 > 100)
        {
            Con_Printf("  %s\n", line.c_str());
            line.clear();
        }
        line += (line.empty() ? "" : ", ") + word;
    }
    if(!line.empty())
    {
        Con_Printf("  %s\n", line.c_str());
    }
}

// vr_memstats_log: the same, as a row of quakevr/profile/memstats_<date>.csv every so many seconds
// and once after each map load, with the frame rate since the last row: a session's slowdown next to
// what the game (and, in the VRAM columns, every other program) holds, whose time grew (ours, on the
// CPU or the GPU, or the runtime's waits), and what there was to draw.
struct MemLog
{
    std::string path;
    double lastTime{0.0};
    int lastFrames{0};
    const void* lastWorld{nullptr};
    double worldSince{0.0};
    double scanMs{0.0};  // the last GL object count's cost
    int timerRows{0};
    MemSample last;      // the last GL object counts (not scanned every row when that is slow)
};

MemLog memLog;

void writeMemLogRow(const char* reason)
{
    const double seconds = realtime - memLog.lastTime;
    const int frames = host_framecount - memLog.lastFrames;
    memLog.lastTime = realtime;
    memLog.lastFrames = host_framecount;

    // Counting GL objects takes a few milliseconds (a hitch in the headset): on map rows, and on every
    // 5th timer row once it has taken more than 2 ms.
    const bool timer = !std::strcmp(reason, "timer");
    const bool scan = !timer || memLog.scanMs < 2.0 || ++memLog.timerRows % 5 == 0;
    MemSample m = sampleMemory(scan);
    if(scan)
    {
        memLog.scanMs = m.scanMs;
        memLog.last = m;
    }
    else
    {
        m.glTextures = memLog.last.glTextures;
        m.buffers = memLog.last.buffers;
        m.framebuffers = memLog.last.framebuffers;
        m.queries = memLog.last.queries;
        m.programs = memLog.last.programs;
    }

    const std::time_t now = std::time(nullptr);
    char clock[32];
    std::strftime(clock, sizeof(clock), "%H:%M:%S", std::localtime(&now));
    Columns c;
    column(c, "clock", "%s", clock);
    column(c, "seconds", "%.1f", realtime);
    column(c, "reason", "%s", reason);
    column(c, "map", "%s", cl.worldmodel ? cl.worldmodel->name : "");
    column(c, "frames", "%d", frames);
    column(c, "ms_per_frame", "%.3f", frames > 0 ? 1000.0 * seconds / frames : 0.0);
    column(c, "vram_used_mb", "%d", m.vramTotal > 0 ? m.vramTotal - m.vramFree : -1);
    column(c, "vram_total_mb", "%d", m.vramTotal);
    column(c, "vram_free_mb", "%d", m.vramFree);
    column(c, "evictions", "%d", m.evictions);
    column(c, "evicted_mb", "%d", m.evictedMb);
    column(c, "working_set_mb", "%.1f", m.workingSet);
    column(c, "peak_working_set_mb", "%.1f", m.peakWorkingSet);
    column(c, "private_mb", "%.1f", m.privateBytes);
    column(c, "hunk_mb", "%.1f", m.hunk);
    column(c, "textures", "%d", m.textures);
    column(c, "normal_maps", "%d", m.normalmaps);
    column(c, "texture_mb", "%.1f", m.textureMb);
    column(c, "gl_textures", "%d", m.glTextures);
    column(c, "gl_buffers", "%d", m.buffers);
    column(c, "gl_framebuffers", "%d", m.framebuffers);
    column(c, "gl_queries", "%d", m.queries);
    column(c, "gl_programs", "%d", m.programs);
    column(c, "targets_made", "%d", gfx::targetsMade);
    if(scan)
    {
        column(c, "gl_scan_ms", "%.2f", memLog.scanMs);
    }
    else
    {
        c.emplace_back("gl_scan_ms", "");
    }
    drainPhases();
    timingColumns(c, logReader);
    logReader = Readers{};

    if(memLog.path.empty())
    {
        char stamp[64];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
        const std::string dir = std::string{com_gamedir} + "/profile";
        Sys_mkdir(dir.c_str());
        memLog.path = dir + "/memstats_" + stamp + ".csv";
        if(FILE* f = std::fopen(memLog.path.c_str(), "w"))
        {
            for(std::size_t i = 0; i < c.size(); i++)
            {
                std::fprintf(f, "%s%s", i ? "," : "", c[i].first.c_str());
            }
            std::fprintf(f, "\n");
            std::fclose(f);
        }
        Con_DPrintf("vr_memstats_log: %s\n", memLog.path.c_str());
    }
    FILE* f = std::fopen(memLog.path.c_str(), "a");
    if(!f)
    {
        return;
    }
    for(std::size_t i = 0; i < c.size(); i++)
    {
        std::fprintf(f, "%s%s", i ? "," : "", c[i].second.c_str());
    }
    std::fprintf(f, "\n");
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

    sampleCounts(); // vr_memstats: the last frame's, before its texts are cleared
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
