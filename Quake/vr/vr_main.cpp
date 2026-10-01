// vr_main.cpp -- Quake VR module lifetime, core cvars and per-frame update.

#include "vr_audio.hpp"
#include "vr_hitmodel.hpp"
#include "vr_box3d.hpp"
#include "vr_hull.hpp"
#include "vr_unstick.hpp"
#include "vr_engine.hpp"
#include "vr_imgprefetch.hpp"
#include "vr_anchor.hpp"
#include "vr_chainsaw.hpp"
#include "vr_decals.hpp"
#include "vr_gore.hpp"
#include "vr_envmap.hpp"
#include "vr_lighting.hpp"
#include "vr_backend.hpp"
#include "vr_throw.hpp"
#include "vr_client.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_input.hpp"
#include "vr_lines.hpp"
#include "vr_limits.hpp"
#include "vr_text3d.hpp"
#include "vr_torso.hpp"
#include "vr_twohand.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_checklist.hpp"
#include "vr_menuui.hpp"
#include "vr_motion.hpp"
#include "vr_zancle.hpp"
#include "vr_posing.hpp"
#include "vr_sightalign.hpp"
#include "vr_bigfont.hpp"
#include "vr_bodycal.hpp"
#include "vr_setup.hpp"
#include "vr_ao.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"
#include "vr_voicenotes.hpp"
#include "vr_detail.hpp"
#include "vr_flashlight.hpp"
#include "vr_grasp.hpp"
#include "vr_modelcollide.hpp"
#include "vr_selfcollide.hpp"
#include "vr_gpustats.hpp"
#include "vr_jobs.hpp"
#include "vr_gfx.hpp"
#include "vr_props.hpp"
#include "vr_fatigue.hpp"
#include "vr_weight.hpp"
#include "vr_weapons.hpp"
#include "vr_painknock.hpp"
#include "vr_particles.hpp"
#include "vr_shells.hpp"
#include "vr_weaponfx.hpp"
#include "vr_worldtext.hpp"
#include "vr_water.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Base/Abort.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/Tan.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    za::UniquePtr<qvr::Backend> backend{nullptr};
    qvr::TrackingState tracking;
    qvr::FrameState frame;
    bool restartRequested{false};
};

State* state = nullptr;

[[nodiscard]] za::UniquePtr<qvr::Backend> createBackend(const char* name)
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
    // -vrmock: the mock headset whatever vr_backend says (the review's re-evaluation runs a second copy of the game
    // beside the one in the headset: it must never open the runtime's session).
    za::UniquePtr<qvr::Backend> backend = createBackend(COM_CheckParm("-vrmock") ? "mock" : vr_backend.string);
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
    state->backend = ZA_MOVE(backend);
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

    // (A vr_motion_eval from the start-up script loads its own map: neither the hub nor the demos meanwhile.)
    if(qvr::motion::evaluating())
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
        const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
        double area = 0.0;
        glm::vec2 lo{1e9f}, hi{-1e9f};
        for(za::SizeT i = 0; i + 2 < h->indices.size(); i += 3)
        {
            const glm::vec2 a = h->vertices[h->indices[i]];
            const glm::vec2 b = h->vertices[h->indices[i + 1]];
            const glm::vec2 c = h->vertices[h->indices[i + 2]];
            area += 0.5 * za::fabs(static_cast<double>((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)));
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
    za::SizeT peakWorkingSetSize, workingSetSize, quotaPeakPagedPoolUsage, quotaPagedPoolUsage,
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

// The GL functions sampleMemory counts objects with (looked up on its first scan).
struct GlIsFns
{
    GlIsFn isBuffer = nullptr, isFramebuffer = nullptr, isQuery = nullptr, isProgram = nullptr;
};
GlIsFns glIsFns;

// vr_memstats's last call (the time and frames since it are printed).
struct MemStatsCalls
{
    double lastTime = 0.0;
    int lastFrames = 0;
    int calls = 0;
};
MemStatsCalls memStatsCalls;

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
    const za::Clock scanClock;
    GlIsFn& isBuffer = glIsFns.isBuffer;
    GlIsFn& isFramebuffer = glIsFns.isFramebuffer;
    GlIsFn& isQuery = glIsFns.isQuery;
    GlIsFn& isProgram = glIsFns.isProgram;
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
    m.scanMs = static_cast<double>(scanClock.getElapsedTime().asMicroseconds()) / 1e3;
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
Readers logReader, commandReader, rateReader; // rateReader: the wrist gadget's FPS counter (frameRate)

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
    addSums(rateReader.sums, p);
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
        if(!ZA_STRNCMP(classname, "monster_", 8))
        {
            (ent->v.deadflag != 0.f || ent->v.health <= 0.f ? e.corpses : e.monsters)++;
        }
        else if(!ZA_STRNCMP(model, "progs/h_", 8))
        {
            e.heads++;
        }
        else if(!ZA_STRNCMP(model, "progs/gib", 9))
        {
            e.gibs++;
        }
        else if(!ZA_STRCMP(classname, "thrown_weapon"))
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
using Columns = za::Vector<gpustats::Column>;

void column(Columns& c, const char* name, const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    c.pushBack(gpustats::Column{name, buf});
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
    column(c, "targets_by_name", "%s", gfx::targetsMadeByName().cStr());
}

void VR_MemStats_f()
{
    if(cls.state == ca_dedicated)
    {
        return;
    }
    double& lastTime = memStatsCalls.lastTime;
    int& lastFrames = memStatsCalls.lastFrames;
    int& calls = memStatsCalls.calls;
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
    Con_Printf("  VR    render targets (re)made %d times so far (%s); ", gfx::targetsMade, gfx::targetsMadeByName().cStr());
    decals::count_f();
    const mem::Totals held = mem::totals();
    Con_Printf("  VR    scratch buffers %.1f KiB (%d sets), caches %.1f KiB (%d sets); the largest:\n",
        static_cast<double>(held.scratchBytes) / 1024.0, held.scratchSets, static_cast<double>(held.cacheBytes) / 1024.0,
        held.cacheSets);
    mem::printLargest(6);

    // Per frame since the last vr_memstats: the phases' times and the counts, as the log's columns.
    drainPhases();
    Columns c;
    timingColumns(c, commandReader);
    commandReader = Readers{};
    za::String line;
    for(const auto& [name, value] : c)
    {
        if(name == "targets_by_name")
        {
            continue;
        }
        const za::String word = name + " " + value;
        if(line.size() + word.size() + 2 > 100)
        {
            Con_Printf("  %s\n", line.cStr());
            line.clear();
        }
        line += (line.empty() ? "" : ", ") + word;
    }
    if(!line.empty())
    {
        Con_Printf("  %s\n", line.cStr());
    }
}

// vr_debug_crash [access|abort]: crashes the game on purpose, to test the crash report (vr_crash.cpp, VR_InstallCrashHandler:
// in a test run, qvr_crash.txt with the stack and qvr_crash.dmp): an access violation (the default) or abort().
void VR_DebugCrash_f()
{
    if(Cmd_Argc() > 1 && q_strcasecmp(Cmd_Argv(1), "abort") == 0)
    {
        za::abort();
    }
    int* volatile nowhere = nullptr; // (volatile: the compiler can't see it is null)
    *nowhere = 1;
}

// vr_memstats_log: the same, as a row of quakevr/profile/memstats_<date>.csv every so many seconds
// and once after each map load, with the frame rate since the last row: a session's slowdown next to
// what the game (and, in the VRAM columns, every other program) holds, whose time grew (ours, on the
// CPU or the GPU, or the runtime's waits), and what there was to draw.
struct MemLog
{
    za::String path;
    double lastTime{0.0};
    int lastFrames{0};
    const void* lastWorld{nullptr};
    double worldSince{0.0};
};

MemLog memLog;

// The GL objects counted as each map loads (VR_NewMap): every name tested with glIs* up to 4096 past the last one found,
// some 25,000 calls that wait for the driver's thread, 12-13 ms: a dropped frame when a row did it in play. A count
// that grows from one load to the next is a leak; the rows repeat the last load's.
MemSample glCounted;

void countGlForLog()
{
    if(vr_memstats_log.value <= 0.f)
    {
        return;
    }
    QVR_PROFILE("memory log");
    glCounted = sampleMemory(true);
}

void writeMemLogRow(const char* reason)
{
    const double seconds = realtime - memLog.lastTime;
    const int frames = host_framecount - memLog.lastFrames;
    memLog.lastTime = realtime;
    memLog.lastFrames = host_framecount;

    QVR_PROFILE("memory log");
    MemSample m = sampleMemory(false);
    m.glTextures = glCounted.glTextures;
    m.buffers = glCounted.buffers;
    m.framebuffers = glCounted.framebuffers;
    m.queries = glCounted.queries;
    m.programs = glCounted.programs;
    m.scanMs = glCounted.scanMs;

    const time_t now = time(nullptr);
    char clock[32];
    strftime(clock, sizeof(clock), "%H:%M:%S", localtime(&now));
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
    column(c, "gl_scan_ms", "%.2f", m.scanMs); // the count's time, at the map's load
    drainPhases();
    timingColumns(c, logReader);
    logReader = Readers{};
    gpustats::columns(c); // the GPU as the whole system uses it: clocks, slowdowns, programs

    if(memLog.path.empty())
    {
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        const za::String dir = za::String{com_gamedir} + "/profile";
        Sys_mkdir(dir.cStr());
        memLog.path = dir + "/memstats_" + stamp + ".csv";
        if(FILE* f = fopen(memLog.path.cStr(), "w"))
        {
            for(za::SizeT i = 0; i < c.size(); i++)
            {
                fprintf(f, "%s%s", i ? "," : "", c[i].name.cStr());
            }
            fprintf(f, "\n");
            fclose(f);
        }
        Con_DPrintf("vr_memstats_log: %s\n", memLog.path.cStr());
    }
    FILE* f = fopen(memLog.path.cStr(), "a");
    if(!f)
    {
        return;
    }
    for(za::SizeT i = 0; i < c.size(); i++)
    {
        fprintf(f, "%s%s", i ? "," : "", c[i].value.cStr());
    }
    fprintf(f, "\n");
    fclose(f);
}

void memLogFrame()
{
    // The GPU's figures (clocks, slowdowns, each program's use of its engines: gpustats' sampling thread) only while
    // profiling (vr_profile, the profiler's panel or its CSV capture) or asked for (vr_memstats_log_gpu): no thread
    // otherwise.
    const bool profiling = vr_profile.value != 0.f || vr_profile_overlay.value != 0.f || vr_profile_csv.value != 0.f;
    if(vr_memstats_log.value > 0.f && (profiling || vr_memstats_log_gpu.value > 0.f))
    {
        gpustats::start();
    }
    else
    {
        gpustats::stop();
    }
    if(vr_memstats_log.value <= 0.f)
    {
        return;
    }
    if(cls.state != ca_connected || cls.signon != SIGNONS || !cl.worldmodel)
    {
        // Loading: the map that follows is a new one, even the same map again (a save loaded, a
        // restart: the same model, which Ironwail keeps).
        memLog.lastWorld = nullptr;
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

namespace
{
int worldGen = 0;
}

int worldGeneration()
{
    return worldGen;
}

const TrackingState fallbackTracking = standingPose(); // (tracking: without a headset; built before main)
const FrameState noFrame;                               // (frameState: likewise)

// The wrist gadget's FPS counter's window (frameRate).
struct RateWindow
{
    double start = -1.0;
    FrameRate last;
    bool valid = false;
};
RateWindow rateWindow;

const TrackingState& tracking()
{
    const TrackingState& fallback = fallbackTracking;
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

bool backendRestartPending()
{
    return state && state->restartRequested;
}

const FrameState& frameState()
{
    return state && state->backend ? state->frame : noFrame;
}

bool frameRate(FrameRate& out)
{
    double& windowStart = rateWindow.start;
    FrameRate& last = rateWindow.last;
    bool& valid = rateWindow.valid;
    if(windowStart < 0.0 || realtime < windowStart)
    {
        drainPhases(); // (what came before is not this window's)
        rateReader = Readers{};
        windowStart = realtime;
    }
    else if(realtime - windowStart >= 0.5)
    {
        drainPhases();
        const profile::PhaseSums& p = rateReader.sums;
        if(p.frames > 0)
        {
            using namespace profile;
            const double frames = p.frames;
            const double waits = p.cpuMs[XrWait] + p.cpuMs[XrAcquire] + p.cpuMs[XrRelease] + p.cpuMs[XrSubmit] + p.cpuMs[Swap];
            last.fps = p.periodMs > 0.0 ? static_cast<float>(1000.0 * frames / p.periodMs) : 0.f;
            last.cpuMs = static_cast<float>(q_max(0.0, (p.hostMs - waits) / frames));
            last.gpuMs = p.gpuFrames > 0 ? static_cast<float>((p.gpuMs[EyeL] + p.gpuMs[EyeR]) / p.gpuFrames) : -1.f;
            valid = true;
        }
        rateReader = Readers{};
        windowStart = realtime;
    }
    out = last;
    return valid;
}

int scaledEyeSize(int image, int max)
{
    const float scale = CLAMP(0.25f, vr_render_scale.value, 2.f);
    int size = static_cast<int>(za::lround(static_cast<double>(image) * scale));
    if(max > 0)
    {
        size = q_min(size, max);
    }
    return q_max(size, 16);
}

} // namespace qvr

static void applyUnpacedSwap(); // vr_mock_fast (below)

// vr_test_dialog [seconds] [turn] [shot]: the New Game confirmation (SCR_ModalMessage) for this long, closing by itself, the
// mock head turned `turn` degrees a second meanwhile (vr_mock_look); the eyes' images (vr_eyeshot `shot`, 1; 3 with the
// UI) at its first and last frames, and the eyes' yaw printed then: they must follow the head (NOTES.md start_2026-09-30_11-26-59; the
// view stuck to the face while the dialog was up).
static struct
{
    bool on = false;
    double start = 0.0;
    float seconds = 2.f;
    float turn = 0.f;
    int shot = 1;
    int frames = 0;
    bool lastShot = false;
    float headYaw[2]{};
    float eyeYaw[2]{};
} dialogTest;

static double modalAnswerSince = 0.0; // vr_test_modal_answer: when the dialog it answers showed

static void testDialog_f()
{
    dialogTest = {};
    dialogTest.seconds = Cmd_Argc() > 1 ? CLAMP(0.2f, static_cast<float>(Q_atof(Cmd_Argv(1))), 30.f) : 2.f;
    dialogTest.turn = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 45.f;
    dialogTest.shot = Cmd_Argc() > 3 ? CLAMP(1, Q_atoi(Cmd_Argv(3)), 3) : 1;
    dialogTest.on = true;
    dialogTest.start = Sys_DoubleTime();
    const int answer = SCR_ModalMessage("Are you sure you want to\nstart a new game? (y/n)\n", dialogTest.seconds);
    dialogTest.on = false;
    Con_Printf("test dialog: %d frames (answer %d); head yaw %.1f -> %.1f, eye yaw %.1f -> %.1f\n", dialogTest.frames,
        answer, dialogTest.headYaw[0], dialogTest.headYaw[1], dialogTest.eyeYaw[0], dialogTest.eyeYaw[1]);
    Cmd_ExecuteString("vr_mock_look 0 0", src_command);
}

extern "C" void VR_NewMap()
{
    ++qvr::worldGen;

    // What the map's first frames, the first shot or the first hit made on first use, made now, in the load: the decals'
    // atlas (0.4 s: the first mark's frame), the view's own models (0.3 s the first time), the liquids' volume and mesh
    // (20-100 ms each map), the particles' atlas, the detail textures, the torch's shape, the casings' model and sounds.
    QVR_PROFILE("vr prewarm");
    za::String times; // developer 1: what each took
    double total = 0.0;
    const auto step = [&](const char* name, void (*prepare)()) {
        QVR_PROFILE(name);
        const double start = Sys_DoubleTime();
        prepare();
        const double ms = (Sys_DoubleTime() - start) * 1000.0;
        total += ms;
        times += va("%s%s %.1f", times.empty() ? "" : ", ", name, ms);
        VR_TimeMark(va("VR_NewMap: %s", name));
    };
    step("decal atlas", decals::prepare);
    step("particle atlas", particles::prepare);
    step("detail textures", detail::prepare);
    step("liquids", water::prepare);
    step("view models", view::prepareModels);
    step("torch", flashlight::prepare);
    step("casings", shells::prepare);
    step("muzzle flash", weaponfx::prepare);
    Con_DPrintf("vr prewarm: %.1f ms (%s)\n", total, times.cStr());

    countGlForLog(); // the memory log's GL objects, in the load (12-13 ms)
    VR_TimeMark("VR_NewMap: GL object count");
}

extern "C" void VR_Init()
{
    state = new State{};

    VR_TimeInit(); // vr_startup_times, vr_walltime
    jobs::start(); // the game's thread pool (vr_jobs.hpp), first: the systems below post to it
    imgprefetch::start(); // the images the start-up and the first map load decode, decoded ahead (the file system is up)
    registerCvars();
    jobs::registerCommands();
    weapons::registerCvars();
    props::registerCvars();
    weight::registerCommands();
    fatigue::registerCommands();
    painknock::registerCommands();
    Cvar_SetCallback(&vr_enabled, onBackendSettingChanged);
    Cvar_SetCallback(&vr_backend, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime, onBackendSettingChanged);
    Cvar_SetCallback(&vr_xr_runtime_json, onBackendSettingChanged);

    Cmd_AddCommand("vr_status", VR_Status_f);
    Cmd_AddCommand("vr_restart", VR_Restart_f);
    Cmd_AddCommand("menu_vr", menu::command_f);
    Cmd_AddCommand("vr_mock_laser", menuui::mockLaser_f);
    Cmd_AddCommand("vr_bigfont", bigfont::report_f);
    Cmd_AddCommand("vr_checklist", checklist::command_f);
    Cmd_AddCommand("vr_handcal_match", menu::handCalMatch_f);
    Cmd_AddCommand("vr_startgame", VR_StartGame_f);
    registerMockCommands();
    input::init();
    voicenotes::init();
    posing::init();
    sightalign::init();
    bodycal::init();
    setup::init();
    motion::init();
    flashlight::init();
    chainsaw::init();
    detail::init();
    hull::init();
    unstick::init();
    particles::init();
    decals::init();
    client::init();
    server::init();
    Cmd_AddCommand("vr_dumpview", view::dumpView_f);
    Cmd_AddCommand("vr_carry_check", held::carryCheck_f);
    Cmd_AddCommand("vr_grasp_dump", view::graspDump_f);
    Cmd_AddCommand("vr_grasp_bench", view::graspBench_f);
    Cmd_AddCommand("vr_grasp_sweep", view::graspSweep_f);
    Cmd_AddCommand("vr_grasp_spheres", grasp::spheres_f);
    Cmd_AddCommand("vr_hand_reload", handrig::reload_f);
    Cmd_AddCommand("vr_hand_rig_info", handrig::info_f);
    Cmd_AddCommand("vr_model_reload", view::modelReload_f);
    Cmd_AddCommand("vr_model_collide_bench", modelcollide::bench_f);
    Cmd_AddCommand("vr_hitmodel_bench", hitmodel::bench_f);
    Cmd_AddCommand("vr_zancle_math_test", qza::mathTest_f);
    Cmd_AddCommand("vr_hitmodel_stats", hitmodel::stats_f);
    Cmd_AddCommand("vr_hitmodel_check", hitmodel::check_f);
    Cmd_AddCommand("vr_body_collide_bench", selfcollide::bench_f);
    Cmd_AddCommand("vr_wounds_test", wounds::test_f);
    Cmd_AddCommand("vr_wounds_info", wounds::info_f);
    Cmd_AddCommand("vr_wounds_dump", wounds::dump_f);
    Cmd_AddCommand("vr_test_remove", progs::testRemove_f);
    Cmd_AddCommand("vr_test_dialog", testDialog_f);
    Cmd_AddCommand("vr_hotspots_legacy", view::hotspotsLegacy_f);
    Cmd_AddCommand("vr_hotspots_check", view::hotspotsCheck_f);
    Cmd_AddCommand("vr_weapon_hotspot_here", view::hotspotHere_f);
    Cmd_AddCommand("vr_hotspot_fit", view::hotspotFit_f);
    anchor::registerCommands();
    Cmd_AddCommand("vr_torso_report", torso::report_f);
    Cmd_AddCommand("vr_decal_count", decals::count_f);
    Cmd_AddCommand("vr_limits", limits::command_f);
    Cmd_AddCommand("vr_decal_atlas", decals::atlas_f);
    Cmd_AddCommand("vr_gore_test", gore::test_f);
    Cmd_AddCommand("vr_memstats", VR_MemStats_f);
    Cmd_AddCommand("vr_debug_crash", VR_DebugCrash_f);
    lighting::init();
    Cvar_SetCallback(&vr_map_liquid_alpha, [](cvar_t*) { R_UpdateLiquidAlpha(); }); // gl_rmisc.c: the liquids' alphas again
    envmap::init(); // vr_envmap_dump
    profile::init();
    audio::init(); // spatial audio's commands (vr_snd_info, vr_snd_test...); Steam Audio is loaded on first use

    state->restartRequested = true;
}

extern "C" void VR_Shutdown()
{
    hull::finishLoads(); // (a map load's builds, if a quit came in the middle of one)
    box3d::finishLoads();
    imgprefetch::shutdown(); // (the decoding tasks finished)
    ao::shutdown(); // (the models' occlusion bakes, VR or not)
    gpustats::stop();
    if(state)
    {
        voicenotes::shutdown();
        motion::shutdown();
        gpustats::stop();
        stopBackend();
        delete state;
        state = nullptr;
    }
    voicenotes::finishWrites(); // the screenshots and notes still being saved (VR or not)
    audio::shutdown(); // (its simulations finished, Steam Audio's objects released, phonon.dll unloaded)
    jobs::shutdown(); // last: whatever the systems above left queued run, the workers joined
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

    applyUnpacedSwap();

    profile::begin("xr wait", false); // the runtime's pacing (xrWaitFrame) and the tracking
    const bool began = !state->backend || state->backend->beginFrame(state->tracking, state->frame);
    profile::end();
    QVR_PROFILE("vr frame setup"); // the rest: the recorder, the texts queued anew, the input
    if(!began)
    {
        Con_Warning("VR: %s session lost\n", state->backend->name());
        stopBackend();
    }
    if(state->backend)
    {
        motion::afterTracking(state->tracking, state->frame); // the recorder's copy; a playback's poses
    }

    sampleCounts(); // vr_memstats: the last frame's, before its texts are cleared
    lines::clear(); // queued anew every frame (teleport aim, crosshairs)
    text3d::clear();
    voicenotes::frame(); // after the clear: its indicator is queued anew each frame
    motion::frame();     // the motion recorder's indicator, likewise
    posing::frame();     // the weapon posing mode's text, likewise
    sightalign::frame(); // Align Sights to My Aim: its countdown, text and state
    bodycal::frame();    // Body Calibration: its steps, text, ghost and preview
    setup::frame();      // VR Calibration: its steps and text, the calibration room's value screens
    configFrame();       // the config saved as the menu closes, if a setting changed (the preview taken off above)
    memLogFrame();
    profile::overlay();  // the profiler's panel (vr_profile_overlay)
    throwing::filterGrips(state->tracking); // the analog grip's release, before it becomes a key
    input::update(state->tracking.input); // releases held keys when VR is off

    // Update the hands now, before the move is built (it carries the aim in the view angles).
    input::roomscaleJump(hands::current());
}

extern "C" int VR_IsActive()
{
    return vrActive();
}

extern "C" int VR_Unpaced()
{
    const Backend* const be = backend();
    return be && vr_mock_fast.value != 0.f && !strcmp(be->name(), "mock") && motion::gameClockFixed();
}

// vr_mock_fast 2: unpaced frames aren't drawn at all (VR_HeadlessView instead): no GL, no wait for the GPU, which
// parallel test runs share (vr_motion_eval's shards). Every such frame, not some: the same frames whatever the machine.
extern "C" int VR_SkipScreen()
{
    return VR_Unpaced() && vr_mock_fast.value >= 2.f;
}

namespace
{
// vr_mock_fast's presents and vsync (VR_SkipSwap, applyUnpacedSwap: the main thread).
struct UnpacedSwap
{
    double lastPresent = 0.0; // the last frame presented (Sys_DoubleTime)
    bool vsyncOff = false;    // vsync turned off for unpaced frames
};
UnpacedSwap unpacedSwap;
} // namespace

extern "C" int VR_SkipSwap()
{
    // vr_mock_fast: a present waits for the display's refresh (vsync off or not: the compositor's pace in a window),
    // most of an unpaced frame. Present ten times a second: the window still shows the run.
    double& lastPresent = unpacedSwap.lastPresent;
    if(!VR_Unpaced())
    {
        return 0;
    }
    const double now = Sys_DoubleTime();
    if(now - lastPresent < 0.1)
    {
        return 1;
    }
    lastPresent = now;
    return 0;
}

// vr_mock_fast: no vsync either while the frames run unpaced (the window's vid_vsync again after).
static void applyUnpacedSwap()
{
    bool& off = unpacedSwap.vsyncOff;
    const bool unpaced = VR_Unpaced();
    if(unpaced != off)
    {
        off = unpaced;
        SDL_GL_SetSwapInterval(unpaced ? 0 : CLAMP(-4, static_cast<int>(Cvar_VariableValue("vid_vsync")), 4));
    }
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
    // vr_test_modal_answer: the dialog answered by itself once it has shown for half a second (tests).
    if(vr_test_modal_answer.value >= 0.f)
    {
        if(modalAnswerSince <= 0.0)
        {
            modalAnswerSince = Sys_DoubleTime();
        }
        else if(Sys_DoubleTime() - modalAnswerSince >= 0.5)
        {
            const int key = vr_test_modal_answer.value != 0.f ? K_ABUTTON : K_BBUTTON;
            Cvar_SetValueQuick(&vr_test_modal_answer, -1.f);
            modalAnswerSince = 0.0;
            Key_Event(key, true);
            Key_Event(key, false);
        }
    }
    int shot = -1; // vr_test_dialog: this frame's eye images (0 the first, 1 the last)
    if(dialogTest.on)
    {
        const float t = static_cast<float>(Sys_DoubleTime() - dialogTest.start);
        const float yaw = dialogTest.turn * t;
        Cmd_ExecuteString(va("vr_mock_look 0 %g", yaw), src_command); // read by VR_BeginFrame's tracking
        if(dialogTest.frames == 0)
        {
            shot = 0;
        }
        else if(!dialogTest.lastShot && t >= dialogTest.seconds - 0.3f)
        {
            shot = 1;
            dialogTest.lastShot = true;
        }
        if(shot >= 0)
        {
            dialogTest.headYaw[shot] = yaw;
            Cvar_SetValueQuick(&vr_eyeshot, static_cast<float>(dialogTest.shot));
        }
        dialogTest.frames++;
    }
    hands::refresh(); // the head and hands from this frame's tracking (no host frame counted it): not stuck to the face
    VR_BeginFrame();
    scr_drawdialog = true;
    SCR_UpdateScreen();
    scr_drawdialog = false;
    if(shot >= 0)
    {
        dialogTest.eyeYaw[shot] = hands::current().eyeAngles[0][YAW];
    }
    return 1;
}

extern "C" void VR_HostFrameEnd()
{
    qvr::motion::hostFrameEnd();
}

extern "C" double VR_HostFrameTime(double time)
{
    return qvr::motion::hostFrameTime(time);
}

extern "C" int VR_ServerFrameOverride(double* frametime)
{
    return qvr::motion::serverFrameOverride(*frametime);
}
