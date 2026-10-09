// vr_xr_runtime.hpp -- which OpenXR runtime the OpenXR backend loads (vr_xr_runtime), and in what order it tries the
// others when that one fails (Auto).
//
// The loader loads the runtime named by XR_RUNTIME_JSON, or the system's active one
// (HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime); the installed ones are listed under ...\AvailableRuntimes. Auto
// (vr_xr_runtime 4, the default) picks the one whose app is running: Virtual Desktop's Streamer -> VDXR, SteamVR's
// vrserver/vrmonitor -> SteamVR, Meta's OVRServer_x64 -> Meta's runtime; then the system's active runtime, then the
// other installed ones. While the Streamer runs, Virtual Desktop's own OpenXR runtime setting (its StreamerSettings.json)
// set to SteamVR puts SteamVR first, VDXR next. The loader unloads a runtime when its last instance is destroyed and
// reads XR_RUNTIME_JSON again at the next xrCreateInstance, so the backend tries them in turn in one process (verified
// with the fake runtime, Misc/quakevr/fakexr; docs/vr-port/TESTING.md, "OpenXR runtime choice").

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::xrruntime
{

// One runtime to try: its manifest (empty: the loader's own choice, the system's active runtime), what it is, and why
// it is tried.
struct Attempt
{
    za::String manifest;
    za::String label;  // "Virtual Desktop (VDXR)", "SteamVR", "Meta (Oculus)", or the manifest's file name
    za::String reason; // "Streamer running", "the system's active runtime"...
};

struct Plan
{
    za::Vector<Attempt> attempts; // in order (at least one)
    bool keepEnvironment = false; // XR_RUNTIME_JSON was set outside the game: left as it is
    bool fallback = false;        // on to the next when one fails (Auto)
    za::String summary;           // "Auto: Virtual Desktop (VDXR) - Streamer running"
    za::String mode;              // "Auto", "System default"... (or "XR_RUNTIME_JSON")
};

// vr_xr_runtime_explain; and XR_RUNTIME_JSON as the game was started with it (before the backend first sets it).
void registerCommands();

// What vr_xr_runtime and the system say now (reads the registry and the running processes, or vr_xr_test_*'s).
[[nodiscard]] Plan plan();

// XR_RUNTIME_JSON for the attempt (before xrCreateInstance).
void use(const Plan& plan, const Attempt& attempt);

// The attempt fails without loading anything (vr_xr_test_fail; in the test environment, no manifest).
[[nodiscard]] bool simulatedFailure(const Attempt& attempt);

// The outcome of the backend's start, shown in VR Settings > Headset under OpenXR Runtime: the attempt that started
// (its index in the plan), or none (-1).
void setOutcome(const Plan& plan, int started);

// That line ("" before the backend first started).
[[nodiscard]] const char* statusLine();

// The log of the backend's starts, <game folder>/qvr_openxr.txt (rewritten at the game's first start, added to at each
// restart; each line written at once, so a crash loses none): the command line, XR_RUNTIME_JSON as the game started,
// vr_xr_runtime_explain's report, each attempt (XR_RUNTIME_JSON set, the runtime the loader loaded, the call that
// failed), the outcome, the stops.
[[nodiscard]] const char* logPath();

// A start begins: the log's header and vr_xr_runtime_explain's report.
void beginLog();

// To the console and the log (once a start began).
void note(const char* format, ...);
void warn(const char* format, ...);

// To the log only.
void logLine(const char* text);

// After xrCreateInstance: the runtime the loader loaded (xrGetInstanceProperties' name), and whether it is the
// attempt's (its manifest's library loaded), else a warning.
void loaded(const Attempt& attempt, const char* runtimeName);

// Before a runtime is unloaded (the backend's stop): the graphics DLLs it may have loaded (d3d11.dll, dxgi.dll,
// d3d12.dll, vulkan-1.dll) kept loaded for good, as the GPU driver may still use them (VDXR's d3d11.dll: NVIDIA's
// OpenGL driver crashed the game reading it once unloaded).
void keepGraphicsModules();

// The attempt is SteamVR's (its manifest steamxr_win64.json).
[[nodiscard]] bool isSteamVR(const Attempt& attempt);

} // namespace qvr::xrruntime
