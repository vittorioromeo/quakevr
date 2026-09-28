// vr_profile.hpp -- a small scope profiler (docs/vr-port/TESTING.md, "Profiling").
//
// vr_profile 1 times the named scopes of every frame, on the CPU (steady_clock) and, for GPU
// scopes, on the GPU (OpenGL timestamp queries, read back a few frames later so the CPU never
// waits for them). Scopes nest: the report is a call tree (the eyes' scopes apart), with each
// scope's average and worst per-frame time over the interval, written every vr_profile_interval
// seconds (or on vr_profile_dump) to <gamedir>/profile/profile_<map>_<date>_<time>.csv, with a
// summary in the console. The time is also sorted into the game's systems (vr_profile_systems.hpp: vr_profile_report,
// the panel vr_profile_overlay, a CSV row a second with vr_profile_csv, the hitch log); vr_profile 2 shows the panel
// over the wrist gadget.
// With vr_profile 0 a scope costs a lookup of its name: a few of them (the phases below) are always
// timed, cheaply, for vr_memstats_log.

#pragma once

#include "vr_profile.h"

namespace qvr::profile
{

// Collecting this frame (vr_profile, latched at the frame's start).
extern bool active;

void begin(const char* name, bool gpu);
void end();

void init();    // commands (VR_Init)
void overlay(); // the systems' panel (vr_profile_overlay, vr_profile 2), queued as text (after text3d::clear)

// ---- Always on, whatever vr_profile is (vr_memstats_log, vr_memstats) ----
// The scopes of these names add up their CPU time every frame (the same name summed, both eyes'
// scopes too), and those marked GPU in vr_profile.cpp their GPU time too, through one timestamp
// query at each end, read back a few frames later (never waited for: a frame not ready by then is
// dropped). About 30 queries and 150 clock reads a frame.
enum Phase
{
    XrWait,       // VR_BeginFrame's backend->beginFrame: xrWaitFrame, xrBeginFrame, the tracking
    XrWaitFrame,  // xrWaitFrame alone (OpenXR)
    Commands,
    Server,       // Host_ServerFrame
    Physics,      // SV_Physics
    ClientRead,   // CL_ReadFromServer
    ViewEntities, // our view entities (hands, weapons, body)
    Screen,       // SCR_UpdateScreen: both eyes and the runtime's calls in between
    EyeL,
    EyeR,
    XrAcquire,    // xrAcquire/WaitSwapchainImage
    XrRelease,
    XrSubmit,     // xrEndFrame
    Swap,         // the window's buffer swap
    RunParticles, // CL_RunParticles
    Sound,
    Rigid,        // rigid bodies (gibs, thrown things)
    ShadowMaps,
    WorldBrush,
    Alias,
    Particles,    // Ironwail's particle pass (Quake VR's particles are drawn in it)
    VrParticles,
    Decals,
    PhaseCount
};

[[nodiscard]] const char* phaseName(Phase phase);
[[nodiscard]] bool phaseGpu(Phase phase);

// Sums since the last take().
struct PhaseSums
{
    int frames{0};            // summed (hitches, frames over 250 ms, left out)
    int hitches{0};
    int slowFrames{0};        // longer than 1.25 of the runtime's display periods: a refresh missed
    double periodMs{0.0};     // a frame's start to the next's
    double periodMaxMs{0.0};
    double hostMs{0.0};       // _Host_Frame's CPU time (to VR_ProfileFrameEnd)
    double cpuMs[PhaseCount]{};
    int gpuFrames{0};         // frames whose GPU times were read back
    int gpuDropped{0};        // and those not ready in time
    double gpuMs[PhaseCount]{};
    double displayPeriodMs{0.0}; // the runtime's, last told (0: not known, the mock)
};

[[nodiscard]] PhaseSums takePhases();

// The runtime's display period (xrWaitFrame's predictedDisplayPeriod), in milliseconds.
void noteDisplayPeriod(double ms);
[[nodiscard]] double displayPeriodMs(); // (0: not known, the mock)

// ---- The last frames, one by one (the wrist gadget's detailed counter, vr_gadget_fps 2) ----
// A frame from the same phases (always timed): when it began (seconds, the steady clock of nowSeconds), its period (to
// the next frame's start), our CPU work (the host frame less the runtime's and the swap's waits: the memory log's
// busy_ms) and the eyes' GPU time (gpu_eyes_ms: read back a few frames later; -1 until then, or if it was dropped).
// Hitches (frames over 250 ms, left out of the sums) are kept here: they are the worst spikes.
struct FrameSample
{
    double time{0.0};
    float periodMs{0.f};
    float cpuMs{0.f};
    float gpuMs{-1.f};
};
constexpr int frameHistorySize = 1024; // 7 s at 144 Hz

// The `back`th newest frame kept (0 the newest); false past the oldest.
[[nodiscard]] bool frameSample(int back, FrameSample& out);
[[nodiscard]] double nowSeconds();

class Scope
{
public:
    Scope(const char* name, bool gpu)
    {
        begin(name, gpu);
    }

    ~Scope()
    {
        end();
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

} // namespace qvr::profile

#define QVR_PROFILE_JOIN2(a, b) a##b
#define QVR_PROFILE_JOIN(a, b) QVR_PROFILE_JOIN2(a, b)

// Times the rest of the enclosing block (name: a string literal): on the CPU, or on both.
#define QVR_PROFILE(name) const ::qvr::profile::Scope QVR_PROFILE_JOIN(qvrProfileScope, __LINE__){name, false}
#define QVR_GPU_PROFILE(name) const ::qvr::profile::Scope QVR_PROFILE_JOIN(qvrProfileScope, __LINE__){name, true}
