// vr_perfrec.hpp -- the perf recorder (vr_perf_record; QUEST_PLAN.md, C4n): every frame of a played session, one CSV row
// each, for finding the hitches and the sustained costs on the headset afterwards (Misc/quakevr/perf_report.py).
//
// A row: the frame's serial and time, the map, its period (start to the next start), the host frame and our CPU work in
// it (less the runtime's and the swap's waits), the runtime's display period and the display periods it missed
// (xrWaitFrame's predicted display times), the GPU's span over the frame (its first timestamp to its last) and each
// always-on profiler phase's CPU and GPU time (vr_profile.hpp's Phase; the GPU read back a few frames later, never waited
// for: empty when dropped), the draw calls, the dynamic lights alive, the main thread's heap events, and the frame's
// events: each kind's count and milliseconds (shader programs compiled or read from the binary cache, textures and
// models loaded, runtime lightmaps uploaded, sounds loaded, a map's load, a save, decal bakes, the navmesh's jobs landing,
// the QuakeC and Box3D steps, other timed work: VR_TimeAdd's) with the first ones' names.
//
// The file: <the app's internal files>/perf/perf_<date>_<time>.csv on Android (`adb exec-out run-as
// com.vittorioromeo.quakevr cat files/perf/<file>`), <game folder>/profile/perf/ elsewhere; written a second at a time.
// vr_perf_record 1 starts a file (the Debug menu's Profiling page too), 0 ends it. Off, it costs a flag's test a frame.

#pragma once

#include "vr_profile.hpp"

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::perfrec
{

// Recording (latched at each frame's end).
extern bool on;

// vr_perf_mark <label>: a mark in the frame's events (a script's A/B parts, a tester's moment).
void init();

// vr_profile.cpp: a named scope's event kind (its time summed into the frame's: "quakec", "box3d step", decal bakes...),
// or -1; a tagged scope begun (true: the outermost of its kind open, to be timed) and ended (its time, ns).
[[nodiscard]] int tagOf(const char* name);
[[nodiscard]] bool scopeBegin(int kind);
void scopeEnd(int kind, za::I64 ns);

// vr_profile.cpp: a frame ended (its serial, its start in seconds, its times in ms, each phase's CPU time in ns, its draw
// calls); its GPU times follow with gpu() if `gpuPending`.
void frame(za::U64 serial, double startS, double periodMs, double hostMs, double busyMs,
    const za::I64 (&phaseNs)[profile::PhaseCount], int drawcalls, bool gpuPending);
// vr_profile.cpp: a frame's GPU phases read back (ms; nullptr: dropped, not ready in time) and its span (first to last
// timestamp, ms).
void gpu(za::U64 serial, const double* phaseMs, double spanMs);
// vr_backend_openxr_frame.cpp: xrWaitFrame's display periods missed since the last frame, and the period (ms).
void xrFrame(int missed, double periodMs);

} // namespace qvr::perfrec
