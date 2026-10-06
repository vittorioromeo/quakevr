// vr_bench.hpp -- the benchmark recorder (docs/vr-port/BENCHMARKS.md).
//
// vr_bench_begin <name> [frames | <seconds>s] records every frame from the next one on: its period (start to the next
// start), the host frame and the CPU's work in it (less the runtime's and the swap's waits), each always-on profiler
// phase's CPU time (vr_profile.hpp's Phase), each GPU phase's time as it is read back (the eyes, the shadow maps, the
// world, the models, the particles, the decals...), the frame's traces, draw calls and alias models, and the main
// thread's heap events; and every 16th frame what there is (edicts, monsters alive, Box3D bodies awake, particles,
// decals, lights). vr_bench_end (or the frame count, or the seconds, reached) writes it all to
// <gamedir>/profile/bench/<name>.json: means, medians, 95th and 99th percentiles, worst frames, hitch counts, the
// graphics settings and the counts at the start and end (scripts check the set-up was the same from run to run), and
// prints one "vr_bench: ..." line. vr_bench_seed <n> restarts the C library's random numbers (QuakeC's random())
// before a scenario's set-up, as motion playback does. Recording costs a few stores a frame; nothing else changes.

#pragma once

#include "vr_profile.hpp"

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::bench
{

// Recording (cheap: the profiler's hooks test it).
extern bool recording;

// vr_profile.cpp: a frame ended. Its period, its host frame and its work (ms), and each phase's CPU time (ns).
void frame(double periodMs, double hostMs, double busyMs, const za::I64 (&phaseNs)[profile::PhaseCount]);
// vr_profile.cpp: a frame's GPU phases read back (ms; 0 for a phase it did not time).
void gpuFrame(const double (&phaseMs)[profile::PhaseCount]);

void registerCommands();

} // namespace qvr::bench
