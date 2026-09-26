// vr_gpustats.hpp -- the GPU as the whole system uses it, for the memory log (vr_memstats_log):
// its clocks, temperature, power and why it slows (NVIDIA's NVML), and each program's share of its
// engines (Windows' "GPU Engine" performance counters): whose GPU work grew when a session slows.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace qvr::gpustats
{

// Starts the sampling thread (a sample every second) on first use; stops it.
void start();
void stop();

// The columns since the last call: averages of the samples (clocks, temperature, power, engine use by
// process), the slowdown reasons seen, and the programs using the GPU most.
void columns(std::vector<std::pair<std::string, std::string>>& c);

} // namespace qvr::gpustats
