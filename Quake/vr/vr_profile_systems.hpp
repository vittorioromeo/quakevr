// vr_profile_systems.hpp -- the profiler's per-system breakdown (docs/vr-port/TESTING.md, "Profiling"; ROUND21.md,
// "Profiling: where the time goes").
//
// vr_profile.cpp times scopes as a call tree; this sorts that time into the game's systems (Box3D, QuakeC, the world's
// drawing, the shadow maps, waiting for the headset...), each scope's own time (less its child scopes') going to the
// system its name names, or its parent's. The systems thus split the frame, from one frame's start to the next's, with
// nothing counted twice: what no scope covers is "other", and the parts add up to the frame time. The same for the GPU's
// time (the GPU scopes), and apart for the views (each eye, the window's mirror or spectator camera, and what is shared).
// Shown by vr_profile_report (a table in the console), the panel (vr_profile_overlay), a row a second in a CSV file
// (vr_profile_csv), and the hitch log (vr_profile_hitch: frames over budget, with their breakdown).

#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/String/String.hpp"


namespace qvr::profile::systems
{

// A new scope's system and view: from its name, else its parent's.
void classify(const char* name, int parentSystem, int parentView, int& system, int& view);
// The tree's root (the frame's own time): "other", shared.
[[nodiscard]] int rootSystem();
[[nodiscard]] int rootView();

// This frame's CPU time in a system and view (nanoseconds; negative to take a child scope's time off its parent).
void cpu(int system, int view, za::I64 ns);
// The GPU's time, likewise (milliseconds), as a frame's timer queries are read back (a few frames later).
void gpu(int system, int view, double ms);
void gpuFrameDone(int queries); // one frame's `queries` timer queries all read: into the sums

// The profiler's own work this frame (vr_profile.cpp's frame processing), off the frame's "other".
void profilerTime(za::I64 ns);

// Counts, per frame.
struct Counts
{
    int traces{0};
    int hullChecks{0};
    int drawCalls{0};
    int aliasDrawn{0};
};

// A collected frame ended: `periodNs` from its start to the next's, `hostNs` of it in _Host_Frame (the rest is the frame
// rate cap's idle time). Frames over `longMs` are logged (the hitch log) but left out of the averages. `scopes`: for a
// hitch, its costliest scopes (vr_profile.cpp's call tree, with their paths).
void frameEnd(za::I64 now, za::I64 periodNs, za::I64 hostNs, const Counts& counts, double longMs,
    const char* map, const za::String& scopes);

// The hitch log's threshold: a frame whose _Host_Frame takes longer is a hitch (ms; 0: vr_profile_hitch off).
[[nodiscard]] double hitchMs();

void start(za::I64 now); // collecting from now (the sums restart)
void stop();                  // not collecting any more (a CSV capture is closed)

void init();    // commands
void overlay(); // the panel, queued as text (after text3d::clear)

} // namespace qvr::profile::systems
