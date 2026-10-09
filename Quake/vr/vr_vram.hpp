#pragma once

// vr_vram.hpp -- vr_vram_report: what the game holds in the GPU's memory, every GL texture, renderbuffer and buffer of
// its context sized from GL's own description of it (levels, layers, samples, format), grouped by what it is (from
// its GL label: the engine's textures carry their names, the VR module's targets theirs), next to what Windows counts
// for the process and for every other program on the GPU (vrcompositor, Virtual Desktop, browsers...).
//
//   vr_vram_report          the categories, the largest objects, the unlabelled ones, the other programs
//   vr_vram_report diff     also what was made and freed since the last report, by group (a leak: a group that grows)
//   vr_vram_report csv      also every object to quakevr/profile/vram_<date>.csv
//   vr_vram_report all      every group, not just the largest
//
// A one-off: the scan asks GL a few dozen things per object (tens of ms), never per frame.

namespace qvr::vram
{

void report_f();

// The last report's totals in MB (-1 before the first): vr_status's line, the tests'.
struct Totals
{
    double glMb{-1.0};       // every object GL describes
    double texturesMb{-1.0}; // of which textures
    double targetsMb{-1.0};  // render targets (the eyes', the effects', the runtime's swapchain images)
    double processMb{-1.0};  // Windows' count of the process's dedicated GPU memory (-1: not known)
    int objects{0};
};
[[nodiscard]] Totals lastTotals();

} // namespace qvr::vram
