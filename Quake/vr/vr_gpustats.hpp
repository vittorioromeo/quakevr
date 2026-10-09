// vr_gpustats.hpp -- the GPU as the whole system uses it, for the memory log (vr_memstats_log):
// its clocks, temperature, power and why it slows (NVIDIA's NVML), and each program's share of its
// engines (Windows' "GPU Engine" performance counters): whose GPU work grew when a session slows.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::gpustats
{

// A column of the log: its header and this row's value.
struct Column
{
    za::String name;
    za::String value;
};

// Starts the sampling thread (a sample every second) on first use; stops it.
void start();
void stop();

// The columns since the last call: averages of the samples (clocks, temperature, power, engine use by
// process), the slowdown reasons seen, and the programs using the GPU most.
void columns(za::Vector<Column>& c);

// The GPU's memory, all processes' (NVML's: what GL_NVX_gpu_memory_info reports, read without the GL context, so no
// wait for the driver's thread: the memory log's 2-4 ms), read on a worker of the game's pool. requestVram() starts a
// read unless one is under way; latestVram() is the last one finished. `readable` false: no NVML (an AMD or Intel GPU,
// not Windows; known once the first read has finished): the caller asks GL as before.
struct Vram
{
    int totalMb{-1};
    int freeMb{-1};
    bool readable{true};
    int reads{0}; // reads finished
    int selfMb{-1};      // this process's dedicated GPU memory (Windows' "GPU Process Memory" counters; -1: not known)
    za::String programs; // the other programs holding the most, "vrcompositor.exe:2100 chrome.exe:900" (MB)
};
void requestVram();
[[nodiscard]] Vram latestVram();
void finishVram(); // VR_Shutdown, before the pool's: a read under way finished, NVML closed

// Each program's GPU memory as Windows counts it (the "GPU Process Memory" performance counters, its dedicated and
// shared usage, every adapter summed), the most first; `self` this process. Empty where there are none (not Windows,
// the counters off). Synchronous: a PDH query (some 10-50 ms); vr_vram_report's and the VRAM read's.
struct ProgramVram
{
    za::String name;
    unsigned pid{0};
    double dedicatedMb{0.0};
    double sharedMb{0.0};
    bool self{false};
};
[[nodiscard]] za::Vector<ProgramVram> programVram();

} // namespace qvr::gpustats
