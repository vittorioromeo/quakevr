#pragma once

// vr_relight_tool.hpp -- ericw-tools' light got from its GitHub release when the game finds none (Graphics > Relighting's
// "Download ericw-tools"; vr_relight_get_tool). Windows only (the release's light.exe).
//
// One file, pinned: ericw-tools 2.0.0-alpha11's Windows zip (version, URL, size and sha256 below; the release package
// ships the same files, Windows/package-quakevr.ps1). Downloaded into memory with the engine's Download (libcurl), on a
// thread of its own; checked against the pinned sha256 (a corrupted or changed file is never unpacked); then the files
// light needs and their licence texts (only those, by name, from the zip's root) unpacked with miniz into
// <folder>.download/ beside the install folder and renamed into place, light.exe last: a lookup never finds a light.exe
// without its libraries. Cancel (and a quit) stops it and removes the half-made folder; nothing is written until the
// download is complete and checked.
//
// The install folder: <user base dir>/quakevr/tools/ericw-tools/, where the release package puts its copy and where
// vr_relight.cpp's findTool looks first. vr_relight_tool_dir (testing) moves it, and makes it the only place looked in.

namespace qvr::relight::tool
{

// Where it goes (vr_relight_tool_dir, else <user base dir>/quakevr/tools/ericw-tools): a path, valid until the next call.
[[nodiscard]] const char* installDir();

// Whether vr_relight_tool_dir is set: findTool then looks in installDir() alone (after vr_relight_tool).
[[nodiscard]] bool testDir();

// Starts the download (false: one is running, or this platform has none; the console says why).
bool start();
// Cancels the running one (false: none).
bool cancel();
[[nodiscard]] bool running();

// The main thread's part (relight::poll, each frame): a finished job's console line, and the lookup told.
void poll();

// Changes when a download finishes (vr_relight.cpp's tool line looks again).
[[nodiscard]] int generation();

// The page's lines: its bar (below 0: no bar), the bar's text and a line of status ("" : nothing to say).
[[nodiscard]] float progress();
[[nodiscard]] const char* progressText();
[[nodiscard]] const char* statusLine();

// Host_Shutdown (VR_StopDownloads) and VR_Shutdown: a download cancelled, waited for 3 s, its half-made folder removed.
void finish();

// vr_relight_get_tool [cancel | status].
void command();

// The Windows release's size, for the page's button.
constexpr const char* sizeText = "27.5 MB";

} // namespace qvr::relight::tool
