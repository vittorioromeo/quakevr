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

} // namespace qvr::gpustats
