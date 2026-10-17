// vr_xr_overlap.hpp -- the main thread's work under the GPU's tail (vr_xr_overlap). Virtual Desktop's OpenGL path makes
// xrEndFrame wait until the GPU has finished the frame, so the main thread idled there for the GPU's tail, and the frame
// took the CPU's time plus that tail. After both eyes' draws the frame is flushed, and work that reads no head or hand
// pose (the particles' step, the music's stream; vr_debug_xr_overlap_work_ms's test) runs before xrEndFrame, while its
// foretold cost fits in the tail the last frames' GPU timestamps foretell; what does not fit runs at its usual place
// after the end call (host.c, VR_XrOverlapLate). docs/vr-port/ROUND22.md, "Work under xrEndFrame's GPU wait".
#pragma once

namespace qvr::xroverlap
{

// vr_stereo.cpp: as the eyes' draws begin (the estimate's CPU side: the GPU cannot run a command before it is issued).
void eyesBegin();

// vr_stereo.cpp: after both eyes' draws (and the mirror), before the end call. rendered: both eyes submitted; spectator:
// the window's spectator camera is drawn after the end call (it draws the particles: their step then waits for it).
void beforeEnd(bool rendered, bool spectator);

// After the end call: its time (ms), for vr_xr_overlap_report.
void afterEnd(bool rendered, double endMs);

// vr_xr_overlap_report.
void registerCommands();

} // namespace qvr::xroverlap
