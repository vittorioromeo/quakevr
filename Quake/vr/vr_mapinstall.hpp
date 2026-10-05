// vr_mapinstall.hpp -- getting a package from the map index (vr_mapindex.hpp) into the game: its zip downloaded to
// quakevr/cache/maps/<sha256>.zip, its files extracted into the user's game dir (BSPs to <game dir>/maps/, where the
// engine's `map` command looks, everything else at the game dir's root), and the package's startmap started.
//
// One job at a time, on a thread of its own (the download and the extraction never run on the main thread). poll()
// takes what it finished, on the main thread: the console line, the status the page shows, and `map <startmap>`.
// What was written is recorded in quakevr/cache/maps_installed.txt (one line per file), which is what "Installed"
// lists and what uninstall removes.
//
// The guards (the same for the page and the console): a package over maxZipBytes is refused; a zip whose files would
// unpack to more than maxUnpackedBytes, or hold more than maxFiles, is refused; a file path that would leave the game
// dir (`..`, an absolute path, a drive letter) is left out; `__MACOSX` and `.DS_Store` are; a single top-level folder
// around everything is stripped (the index's zipbasedir tag); a BSP whose version is not 29, 2PSB, BSP2 or Q64 is left
// out and said; a file already on disk is never overwritten (it is skipped and counted in the status).

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
// The main thread, every frame: takes what the job's thread finished, prints its line, and starts the map it installed.
void poll();

// The live job (never null; Phase::Idle when there is none). Read from the main thread only.
[[nodiscard]] const Job& job();
// Its progress, 0..100 (the download's bytes, then the extraction's files).
[[nodiscard]] int percent();
// Whether a job for this package is running now.
[[nodiscard]] bool busy(const za::String& sha);

// Begin the job (false: another one is running). `install` false: only the zip.
bool begin(const mapindex::Entry* entry, bool install);
// The current job's download stopped (its zip stays out of the cache).
void cancel();

// The zip is in the cache (its path in `out`).
[[nodiscard]] bool cached(const za::String& sha, za::String& out);
// The package is installed (its files are in the game dir).
[[nodiscard]] bool installed(const za::String& sha);
// What is installed, one entry per package.
[[nodiscard]] const za::Vector<Installed>& installedList();
// Its files' bytes on disk (0: not installed).
[[nodiscard]] za::U64 installedBytes(const za::String& sha);
// Remove an installed package's files (the ones no other package wrote too), and the record.
bool uninstall(const za::String& sha);
// The package's startmap, started now: the page's Play action, and what a finished install does. Cbuf_InsertText, so
// that it runs next rather than after the commands already queued (a script's `screenshot;quit` would come first).
bool play(const za::String& sha);

// The game dir the files go to (com_basedirs' last: the user's quakevr), and its maps dir.
[[nodiscard]] const za::String& gameDir();
[[nodiscard]] za::String mapsDir();

} // namespace qvr::mapinstall
