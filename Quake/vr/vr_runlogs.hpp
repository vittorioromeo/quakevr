// vr_runlogs.hpp -- every run's logs, always on (<game folder>/logs/<date>_<time>/, the newest 5 runs kept), and the
// bug report made from them (vr_runlogs.cpp; the C entry points are in vr_api.h, VR_Logs*).

#pragma once

#include <ctime>

namespace qvr::runlogs
{

using LineFn = void (*)(const char* line);

// The run's folder ("" before VR_LogsBegin, and in -bugreport's run).
[[nodiscard]] const char* dir();

// A line added to a file of the run's folder (opened and closed for it: a crash loses nothing).
void append(const char* name, const char* text);

// gl_startup.log (always), qvr_openxr.txt and the runtimes' logs (SteamVR's, Virtual Desktop's: written since `since`)
// copied into `dir`; each copy said to `line` (may be null).
void copyRuntimeLogs(const char* dir, time_t since, LineFn line);

// A file of the run's folder (console.log, openxr_debug.log: flushed first) copied into `dir`.
void copyRunFile(const char* name, const char* dir);

// The OpenXR debug messages so far (vr_diagnostics_status).
[[nodiscard]] unsigned xrMessages();

} // namespace qvr::runlogs
