// vr_xr_stall.hpp -- held frames: the runtime holding the game's frames (xrWaitFrame or xrEndFrame far over the frame
// budget, as Virtual Desktop's 100 ms frames at the 2026-10-17 VR drop: docs/vr-port/ROUND22.md, "VR drop
// diagnostics"), said once as it starts and once as it ends (rate-limited) in qvr_openxr.txt, as Tracy messages and with
// developer 1 in the console, with what else was going on: the foreground window's program and title, whether it is
// the game's, the session's state, the frames' timings and the CPU and GPU frame times. Also the session's state changes
// and xrWaitFrame's and xrEndFrame's failures, with the time. While frames are on time: a few compares a frame.
#pragma once

namespace qvr::xrstall
{

// After each frame's end call: its xrWaitFrame and xrEndFrame times (ms; the mock: 0 and its end's), the display period
// (ms; 0 not known: the mock) and the session state's name.
void frame(double waitMs, double endMs, double periodMs, const char* state);

// A line (ending in a newline) in qvr_openxr.txt and as a Tracy message.
void note(const char* line);

// A frame call's failure (xrWaitFrame, xrEndFrame): logged with the time; the same call failing with the same code again
// within a second only counted (said with the next line logged).
void callFailed(const char* call, int code, const char* codeName);

// vr_debug_frame_stall_ms: the test's hold, waited inside the timed end call (both backends).
void debugStall();

// The local time, hh:mm:ss.mmm (va's buffer).
[[nodiscard]] const char* wallClock();

} // namespace qvr::xrstall
