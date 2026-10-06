// vr_mapinstall.hpp -- getting a package from the map index (vr_mapindex.hpp) into the game: its zip downloaded to
// <base>/cache/maps/<sha256>.zip, its files extracted into the package's own folder, <base>/qvr_addons/<id>/, laid
// out as a game dir (BSPs to maps/, the rest where the index's install.extract says the zip's root goes; its game
// folder, id1 or a mod's, dropped). That folder is on the search path only while the package is played, under quakevr
// and over the stock game (active(), below): no package's file meets another's, the stock game's maps never see one,
// and none can take the place of a Quake VR file (its progs, its VR models, sounds and ammo boxes).
//
// Installing and playing are separate actions: installing writes files and reports what it wrote, and never starts a
// map; play() (maps_play, the page's Play button) is what starts a package's startmap, and is what a package whose
// files are there already is offered.
//
// One job at a time, on a thread of its own (the download and the extraction never run on the main thread). poll()
// takes what it finished, on the main thread: the console line and the status the page shows.
// What was written is recorded in quakevr/cache/maps_installed.txt (one line per file), which is what "Installed"
// lists and what uninstall removes.
//
// The guards (the same for the page and the console): a package over maxZipBytes is refused; a zip whose files would
// unpack to more than maxUnpackedBytes, or hold more than maxFiles, is refused; a file path that would leave the game
// dir (`..`, an absolute path, a drive letter) is left out; `__MACOSX` and `.DS_Store` are; a single top-level folder
// around everything is stripped (the index's zipbasedir tag); a BSP whose version is not 29, 2PSB, BSP2 or Q64 is left
// out and said; a package carrying game code (progs.dat, qwprogs.dat, csprogs.dat: a mod, in the zip or in one of its
// paks) is refused; quake.rc and the *.cfg a mod folder carries are left out; the paks at its root are unpacked into
// loose files (in their order, over the loose ones, as the engine would read them).

#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include "vr_mapindex.hpp"
#include "vr_mem.hpp"

namespace qvr::mapinstall
{

constexpr za::U64 maxZipBytes = 200ull * 1024 * 1024; // a package's zip, refused above this
constexpr za::U64 maxUnpackedBytes = 400ull * 1024 * 1024; // and its files, unpacked
constexpr int maxFiles = 4000;

// Where a job is. Downloaded means the zip is in the cache; Installed means its files are in the game dir.
enum class Phase
{
    Idle,       // no job
    Download,   // the zip is coming
    Extract,    // the zip is unpacking
    Done,       // the job finished
    Failed,     // it stopped (its `message` says why)
};

// One file an installed package wrote (maps_installed.txt's line).
struct InstalledFile
{
    za::String sha;
    za::String path; // in the game dir, e.g. "maps/colony5.bsp"
    za::U64 bytes{0};
};

// A package that is installed, as the Installed list shows it.
struct Installed
{
    za::String sha;
    za::String title;
    za::U64 bytes{0};
    int files{0};
};

// The one live job, and what it reported. Filled by the worker thread behind `handoff`, read by poll() and the page.
struct Job
{
    za::String sha;     // the package it is for
    za::String title;   // its title, for the console line and the page
    Phase phase{Phase::Idle};
    bool install{true}; // false: only download the zip
    za::String message; // what to show ("4.1 MB of 5.1 MB", "12 files written, 2 already there")
    za::U64 gotBytes{0}; // zip bytes received so far
    za::U64 zipBytes{0}; // the package's size (the index's), for the percent
    int extractMs{0};  // what the unpacking took (maps_stats and the scratch file)
    int downloadMs{0};
    int written{0}; // files written
    int skipped{0}; // files left as they were (already on disk)
    za::Vector<InstalledFile> wrote; // what it put in the game dir (poll() records these in maps_installed.txt)

    auto members()
    {
        return mem::list(sha, title, phase, install, message, gotBytes, zipBytes, extractMs, downloadMs, written,
            skipped, wrote);
    }
};

// maps_get, maps_install, maps_installed, maps_uninstall.
void registerCommands();
// Start-up: the installed list read from maps_installed.txt (nothing is fetched or downloaded here).
void start();
// Quit: the job's thread is cancelled and joined.
void finish();
// The main thread, every frame: takes what the job's thread finished, prints its line, and records what it wrote.
void poll();

// The live job (never null; Phase::Idle when there is none). Read from the main thread only.
[[nodiscard]] const Job& job();
// Its progress, 0..100 (the download's bytes, then the extraction's files).
[[nodiscard]] int percent();
// Whether a job for this package is running now.
[[nodiscard]] bool busy(const za::String& sha);

// Begin the job (false: another one is running, or no package; `why` says which). `install` false: only the zip.
bool begin(const mapindex::Entry* entry, bool install, za::String* why = nullptr);
// The running job stopped (maps_cancel, the page's Cancel): the download within a second, the unpacking before its next
// file; its zip stays out of the cache, the files already unpacked stay recorded. False: no job is running.
bool cancel();
// A job is running (its thread has not handed it off yet), and whether it was asked to stop.
[[nodiscard]] bool running();
[[nodiscard]] bool cancelling();
// What the module is doing, in one line, always (maps_status, the page's status line): downloading what (bytes, the
// mirror, the seconds, how long nothing arrived), unpacking it, stopping it, or idle with how the last job ended.
[[nodiscard]] za::String statusLine();

// The package is installed (its files are in the game dir).
[[nodiscard]] bool installed(const za::String& sha);
// What is installed, one entry per package.
[[nodiscard]] const za::Vector<Installed>& installedList();
// Its files' bytes on disk (0: not installed).
[[nodiscard]] za::U64 installedBytes(const za::String& sha);
// Remove an installed package's files (the ones no other package wrote too), and the record.
bool uninstall(const za::String& sha);
// The package's startmap, started now: the page's Play action, and maps_play. Never part of installing. Cbuf_InsertText,
// so that it runs next rather than after the commands already queued (a script's `screenshot;quit` would come first).
bool play(const za::String& sha);

// The map packages' folders (each package in its own: <base>/qvr_addons/<id>/). One at a time is on the search path,
// under quakevr and over the stock game: the active one, mounted by Play, by a `map` whose BSP only a package has, or
// by loading a save made in one (its <save>.addon); a `map` the stock game has unmounts it. Every change rebuilds
// the game folders (vr_gamedir.cpp) and empties the caches, so no file of one package outlives it.
[[nodiscard]] const za::String& active();
// `sha` mounted ("" : none). False: it cannot be (not Quake VR's own game, or its folder is missing).
bool activate(const za::String& sha);
// vr_gamedir.cpp, while it adds quakevr: the active package's folder added to the search path (under quakevr).
void mountActive();
// VR_SkipSearchPath: a package's folder skipped while the stock game alone is asked (packageFor).
[[nodiscard]] bool skipSearchPath(const char* path);
// The package `map` is played from ("" : the stock game's, or none has it).
[[nodiscard]] za::String packageFor(const char* map);
// The `map` Play queued: true (and forgotten) when it is the active package's own.
[[nodiscard]] bool playPendingFor(const char* map);

// A byte count as the console and the page show it ("5.1 MB").
[[nodiscard]] za::String formatBytes(za::U64 bytes);

} // namespace qvr::mapinstall
