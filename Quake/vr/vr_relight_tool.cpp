// vr_relight_tool.cpp -- see vr_relight_tool.hpp.

#include "vr_relight_tool.hpp"
#include "vr_relight.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_sha256.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"

#include "miniz.h" // (the engine's C header, which guards itself for C++)

#include <stdlib.h>
#include <string.h>

namespace qvr::relight::tool
{
namespace
{

// The one file: ericw-tools 2.0.0-alpha11's Windows release (27,503,991 bytes; the sha256 of that file as GitHub serves
// it). The URL can be pointed elsewhere for a test (vr_relight_tool_url): whatever it serves must still be this file.
constexpr const char* version = "2.0.0-alpha11";
constexpr const char* releaseUrl =
    "https://github.com/ericwa/ericw-tools/releases/download/2.0.0-alpha11/ericw-tools-2.0.0-alpha11-win64.zip";
constexpr const char* releaseSha = "4e5ea11be2194a1c4acac6d6da9d5b5b9f65324fda2d67efa0731d1fd8e0745f";
constexpr za::U64 releaseBytes = 27503991;
constexpr za::U64 maxBytes = 64ull * 1024 * 1024; // a server sending more is stopped
constexpr za::U64 maxFileBytes = 48ull * 1024 * 1024; // (embree4.dll, the largest, is 37 MB)

// What is unpacked, from the zip's root (the files Windows/package-quakevr.ps1 ships): light.exe last, so that a lookup
// never finds it without its libraries.
constexpr const char* wanted[] = {"embree4.dll", "tbb12.dll", "tbbmalloc.dll", "gpl_v3.txt", "LICENSE-embree.txt",
    "README.md", "light.exe"};
constexpr int wantedCount = static_cast<int>(sizeof(wanted) / sizeof(wanted[0]));
constexpr const char* noticeName = "NOTICE.txt";

// Misc/quakevr/ericw-tools-NOTICE.txt's, for a copy downloaded in the game.
constexpr const char* noticeText =
    "ericw-tools' light, downloaded by Quake VR for the in-game relighting\n"
    "======================================================================\n"
    "\n"
    "The files in this folder (light.exe, embree4.dll, tbb12.dll, tbbmalloc.dll) are ericw-tools 2.0.0-alpha11's\n"
    "light and the libraries it needs, unchanged, from its Windows release, downloaded by Quake VR (VR Settings >\n"
    "Graphics > Relighting > Download ericw-tools) and checked against the release's SHA-256:\n"
    "\n"
    "    https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11\n"
    "    (ericw-tools-2.0.0-alpha11-win64.zip,\n"
    "     sha256 4e5ea11be2194a1c4acac6d6da9d5b5b9f65324fda2d67efa0731d1fd8e0745f)\n"
    "\n"
    "Quake VR runs light.exe as a separate program; it is not linked into Quake VR. Delete this folder and the game\n"
    "works as before (only the in-game relighting needs light).\n"
    "\n"
    "ericw-tools: Copyright (C) Eric Wasylishen, Kevin Shanahan (Tyrann), David Walton (Spike) and the ericw-tools\n"
    "contributors. GNU General Public License, version 3 or later: gpl_v3.txt in this folder. Its source code is the\n"
    "2.0.0-alpha11 tag, https://github.com/ericwa/ericw-tools/tree/2.0.0-alpha11, also offered beside the Quake VR\n"
    "release (ericw-tools-2.0.0-alpha11-src.zip).\n"
    "\n"
    "Embree (embree4.dll): Copyright (C) Intel Corporation. Apache License, Version 2.0: LICENSE-embree.txt.\n"
    "oneTBB (tbb12.dll, tbbmalloc.dll): Copyright (C) Intel Corporation. Apache License, Version 2.0 (the same text).\n";

enum class Phase
{
    Idle,
    Download,
    Check,
    Unpack,
    Done,
    Failed,
    Cancelled,
};

// The worker's live state (atomics: read by the page each frame) and its abort flag (the Download API's).
SDL_atomic_t cancelFlag{};
SDL_atomic_t livePhase{};
SDL_atomic_t liveBytes{};
SDL_atomic_t quitting{};
za::Atomic<bool> jobRunning{false};
za::Thread worker;

// Set on the main thread before the worker starts, then only read by it.
struct Request
{
    za::String url;
    za::String dir; // the install folder
    za::String tmp; // <dir>.download
};
Request request;

// The worker's result: written by it before jobRunning goes false, read by poll() after.
struct Result
{
    Phase phase{Phase::Idle};
    za::String message; // the console's line
    za::String brief;   // the page's (the tool line above it gives the folder)
};
Result result;
bool tooBig = false; // (the worker's: writeChunk runs on it)
const char* briefWhy = nullptr; // (the worker's: a failure's short reason for the page; null: "see the console")

// The main thread's: what the page shows of the last job.
Phase shownPhase = Phase::Idle;
za::String shownMessage;
za::String shownBrief;
int gen = 0;

// The page's texts (file scope: valid until the function's next call).
char dirText[MAX_OSPATH];
char progressBuf[96];
char statusBuf[MAX_OSPATH + 160];

[[nodiscard]] bool cancelled()
{
    return SDL_AtomicGet(&cancelFlag) != 0;
}

[[nodiscard]] const char* cancelText()
{
    return SDL_AtomicGet(&quitting) ? "cancelled (the game quit)" : "cancelled";
}

size_t writeChunk(void* buffer, size_t size, size_t nmemb, void* stream)
{
    if(cancelled())
    {
        return 0; // (the transfer stops)
    }
    za::Vector<char>& body = *static_cast<za::Vector<char>*>(stream);
    const za::SizeT n = size * nmemb;
    if(static_cast<za::U64>(body.size()) + n > maxBytes)
    {
        tooBig = true;
        return 0;
    }
    body.reserveMore(n);
    body.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    SDL_AtomicSet(&liveBytes, static_cast<int>(body.size()));
    return nmemb;
}

size_t readFromMemory(void* opaque, mz_uint64 ofs, void* buf, size_t n)
{
    const za::Vector<char>& body = *static_cast<const za::Vector<char>*>(opaque);
    if(ofs + n > body.size())
    {
        return 0;
    }
    memcpy(buf, body.data() + ofs, n);
    return n;
}

[[nodiscard]] za::String mb(za::U64 bytes)
{
    char text[32];
    q_snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / 1e6);
    return za::String{text};
}

[[nodiscard]] za::String inDir(const za::String& dir, const char* name)
{
    return dir + "/" + name;
}

// The download, into memory, checked against the pinned size and sha256.
[[nodiscard]] bool download(za::Vector<char>& body, za::String& why)
{
    download_t dl{};
    dl.write_fn = writeChunk;
    dl.write_data = &body;
    dl.abort = &cancelFlag;
    tooBig = false;
    const bool ok = Download(request.url.cStr(), &dl);
    if(cancelled())
    {
        why = cancelText();
        return false;
    }
    if(tooBig)
    {
        why = za::String{"the server sent more than "} + mb(maxBytes) + ": stopped";
        return false;
    }
    if(!ok)
    {
        why = dl.error ? za::String{dl.error} : za::String{"HTTP "} + za::toString(dl.response ? dl.response : 0);
        briefWhy = "no connection, or the server refused";
        return false;
    }
    SDL_AtomicSet(&livePhase, static_cast<int>(Phase::Check));
    const sha256::Digest got = sha256::of(body.data(), body.size());
    if(body.size() != releaseBytes || !sha256::matches(got, releaseSha))
    {
        char hex[65];
        sha256::toHex(got, hex);
        why = za::String{"the file is not ericw-tools "} + version + "'s release (" + mb(body.size()) + ", sha256 " +
              za::String{hex, 16} + "..., not " + mb(releaseBytes) + ", " + za::String{releaseSha, 16} +
              "...): a corrupted download or a changed file, not unpacked";
        briefWhy = "the file is not ericw-tools' release (corrupted?)";
        return false;
    }
    return true;
}

// The wanted files unpacked into request.tmp (made afresh), and the notice written.
[[nodiscard]] bool unpack(const za::Vector<char>& body, za::String& why)
{
    SDL_AtomicSet(&livePhase, static_cast<int>(Phase::Unpack));
    if(files::exists(request.tmp.cStr()))
    {
        files::removeAll(request.tmp.cStr()); // (a download a crash or a kill left half-made)
    }
    if(!files::createDirectories(request.tmp.cStr()))
    {
        why = "could not make " + request.tmp;
        return false;
    }
    mz_zip_archive z{};
    z.m_pRead = readFromMemory;
    z.m_pIO_opaque = const_cast<za::Vector<char>*>(&body);
    if(!mz_zip_reader_init(&z, static_cast<mz_uint64>(body.size()), 0))
    {
        why = "not a zip this engine can read";
        return false;
    }
    // Each wanted file's entry: by its exact name at the zip's root (nothing else is written, so no entry's path can
    // leave the folder).
    int index[wantedCount];
    for(int& i : index)
    {
        i = -1;
    }
    const int count = static_cast<int>(z.m_total_files);
    for(int e = 0; e < count; e++)
    {
        mz_zip_archive_file_stat st{};
        if(!mz_zip_reader_file_stat(&z, static_cast<mz_uint>(e), &st) ||
            mz_zip_reader_is_file_a_directory(&z, static_cast<mz_uint>(e)))
        {
            continue;
        }
        for(int w = 0; w < wantedCount; w++)
        {
            if(!strcmp(st.m_filename, wanted[w]) && st.m_uncomp_size <= maxFileBytes)
            {
                index[w] = e;
            }
        }
    }
    bool ok = true;
    for(int w = 0; ok && w < wantedCount; w++)
    {
        if(cancelled())
        {
            why = cancelText();
            ok = false;
            break;
        }
        if(index[w] < 0)
        {
            why = za::String{"the zip has no "} + wanted[w];
            ok = false;
            break;
        }
        size_t got = 0;
        void* data = mz_zip_reader_extract_to_heap(&z, static_cast<mz_uint>(index[w]), &got, 0);
        if(!data)
        {
            why = za::String{"could not unpack "} + wanted[w];
            ok = false;
            break;
        }
        const za::String path = inDir(request.tmp, wanted[w]);
        if(!files::writeBytes(path.cStr(), data, static_cast<za::SizeT>(got)))
        {
            why = "could not write " + path;
            ok = false;
        }
        free(data);
    }
    mz_zip_reader_end(&z);
    if(ok && !files::writeText(inDir(request.tmp, noticeName).cStr(), za::StringView{noticeText}))
    {
        why = "could not write " + inDir(request.tmp, noticeName);
        ok = false;
    }
    return ok;
}

// request.tmp's files moved into request.dir (made if it is not there), file by file, light.exe last: a lookup finds it
// only once its libraries are in place. (Not the folder renamed: MoveFileEx refuses a folder with
// MOVEFILE_REPLACE_EXISTING, and a file still held by a virus scan would stop it.)
[[nodiscard]] bool place(za::String& why)
{
    if(cancelled())
    {
        why = cancelText();
        return false;
    }
    if(!files::createDirectories(request.dir.cStr()))
    {
        why = "could not make " + request.dir;
        return false;
    }
    const char* order[wantedCount + 1];
    order[0] = noticeName;
    for(int w = 0; w < wantedCount; w++)
    {
        order[w + 1] = wanted[w];
    }
    for(const char* name : order)
    {
        if(!files::rename(inDir(request.tmp, name).cStr(), inDir(request.dir, name).cStr()))
        {
            why = za::String{"could not move "} + name + " into " + request.dir + " (in use?)";
            return false;
        }
    }
    files::removeAll(request.tmp.cStr());
    return true;
}

int run()
{
    Result r;
    za::Vector<char> body;
    za::String why;
    briefWhy = nullptr;
    if(download(body, why) && unpack(body, why) && place(why))
    {
        r.phase = Phase::Done;
        r.message = za::String{"ericw-tools "} + version + " installed in " + request.dir;
        r.brief = za::String{"ericw-tools "} + version + " installed: Relight This Map is ready.";
    }
    else
    {
        if(files::exists(request.tmp.cStr()))
        {
            files::removeAll(request.tmp.cStr()); // (nothing half-made left)
        }
        r.phase = cancelled() ? Phase::Cancelled : Phase::Failed;
        r.message = r.phase == Phase::Cancelled ? za::String{"download "} + cancelText() + ": nothing kept"
                                                : "download failed: " + why;
        r.brief = r.phase == Phase::Cancelled ? za::String{"Download cancelled: nothing kept."}
                                              : za::String{"Download failed: "} + (briefWhy ? briefWhy : "see the console") + ".";
    }
    result = ZA_MOVE(r);
    SDL_AtomicSet(&livePhase, static_cast<int>(result.phase));
    jobRunning.storeSeqCst(false);
    return 0;
}

void take()
{
    worker.join();
    shownPhase = result.phase;
    shownMessage = result.message;
    shownBrief = result.brief;
    gen++;
    Con_Printf("relight: %s\n", shownMessage.cStr());
}

} // namespace

const char* installDir()
{
    if(testDir())
    {
        q_strlcpy(dirText, vr_relight_tool_dir.string, sizeof(dirText));
    }
    else
    {
        q_snprintf(dirText, sizeof(dirText), "%s/quakevr/tools/ericw-tools",
            com_numbasedirs > 0 ? com_basedirs[com_numbasedirs - 1] : ".");
    }
    return dirText;
}

bool testDir()
{
    return vr_relight_tool_dir.string[0] != 0;
}

bool start()
{
#ifndef _WIN32
    Con_Printf("relight: the ericw-tools download is Windows only (its release's light.exe): build ericw-tools and set "
               "vr_relight_tool (docs/RELIGHTING.md)\n");
    return false;
#else
    if(jobRunning.loadSeqCst())
    {
        Con_Printf("relight: ericw-tools is being downloaded already (vr_relight_get_tool cancel stops it)\n");
        return false;
    }
    if(relight::running())
    {
        Con_Printf("relight: maps are being relit: cancel that first\n");
        return false;
    }
    if(worker.joinable())
    {
        take(); // (one that finished between two polls)
    }
    request.url = vr_relight_tool_url.string[0] ? za::String{vr_relight_tool_url.string} : za::String{releaseUrl};
    request.dir = files::generic(za::StringView{installDir()});
    request.tmp = request.dir + ".download";
    SDL_AtomicSet(&cancelFlag, 0);
    SDL_AtomicSet(&quitting, 0);
    SDL_AtomicSet(&liveBytes, 0);
    SDL_AtomicSet(&livePhase, static_cast<int>(Phase::Download));
    shownPhase = Phase::Download;
    shownMessage.clear();
    shownBrief.clear();
    Con_Printf("relight: downloading ericw-tools %s (%s) from %s into %s\n", version, sizeText, request.url.cStr(),
        request.dir.cStr());
    jobRunning.storeSeqCst(true);
    worker = za::Thread(run);
    return true;
#endif
}

bool cancel()
{
    if(!jobRunning.loadSeqCst())
    {
        return false;
    }
    SDL_AtomicSet(&cancelFlag, 1);
    return true;
}

bool running()
{
    return jobRunning.loadSeqCst();
}

void poll()
{
    if(worker.joinable() && !jobRunning.loadSeqCst())
    {
        take();
    }
}

int generation()
{
    return gen;
}

float progress()
{
    if(!jobRunning.loadSeqCst())
    {
        return -1.f;
    }
    switch(static_cast<Phase>(SDL_AtomicGet(&livePhase)))
    {
        case Phase::Download:
            return 0.9f * static_cast<float>(SDL_AtomicGet(&liveBytes)) / static_cast<float>(releaseBytes);
        case Phase::Check: return 0.92f;
        case Phase::Unpack: return 0.95f;
        default: return 1.f;
    }
}

const char* progressText()
{
    progressBuf[0] = 0;
    if(!jobRunning.loadSeqCst())
    {
        return progressBuf;
    }
    switch(static_cast<Phase>(SDL_AtomicGet(&livePhase)))
    {
        case Phase::Download:
            q_snprintf(progressBuf, sizeof(progressBuf), "%s of %s", mb(static_cast<za::U64>(SDL_AtomicGet(&liveBytes))).cStr(),
                mb(releaseBytes).cStr());
            break;
        case Phase::Check: q_strlcpy(progressBuf, "Checking", sizeof(progressBuf)); break;
        case Phase::Unpack: q_strlcpy(progressBuf, "Unpacking", sizeof(progressBuf)); break;
        default: break;
    }
    if(cancelled())
    {
        q_strlcpy(progressBuf, "Stopping...", sizeof(progressBuf));
    }
    return progressBuf;
}

const char* statusLine()
{
    if(jobRunning.loadSeqCst())
    {
        q_snprintf(statusBuf, sizeof(statusBuf), "Downloading ericw-tools %s...", version);
    }
    else
    {
        q_strlcpy(statusBuf, shownBrief.cStr(), sizeof(statusBuf));
    }
    return statusBuf;
}

void finish()
{
    // As vr_mapinstall.cpp's: cancelled, given 3 s; one still inside curl then (a blocking lookup) is let go, not
    // joined, and libcurl's global state kept for it. Its thread removes its half-made folder when it returns.
    if(!worker.joinable())
    {
        return;
    }
    SDL_AtomicSet(&quitting, 1);
    SDL_AtomicSet(&cancelFlag, 1);
    const za::U32 t0 = SDL_GetTicks();
    while(jobRunning.loadSeqCst() && SDL_GetTicks() - t0 < 3000)
    {
        SDL_Delay(10);
    }
    if(jobRunning.loadSeqCst())
    {
        Sys_Printf("relight: the ericw-tools download did not stop in 3 s; quitting without it\n");
        Download_KeepGlobalState();
        worker.detach();
        return;
    }
    worker.join();
}

void command()
{
    const char* arg = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(arg, "cancel"))
    {
        Con_Printf(cancel() ? "relight: cancelling the ericw-tools download...\n" : "relight: no ericw-tools download is running\n");
        return;
    }
    if(!q_strcasecmp(arg, "status"))
    {
        Con_Printf("relight: %s\n", relight::toolLine());
        Con_Printf("  install folder: %s%s\n", installDir(), testDir() ? " (vr_relight_tool_dir: the only place looked)" : "");
        Con_Printf("  file: ericw-tools %s, %s bytes, sha256 %s\n", version, za::toString(releaseBytes).cStr(), releaseSha);
        Con_Printf("  from: %s\n", vr_relight_tool_url.string[0] ? vr_relight_tool_url.string : releaseUrl);
        Con_Printf("  %s%s%s\n", running() ? "downloading: " : "last: ", running() ? progressText() : "",
            running() ? "" : (shownMessage.empty() ? "none in this session" : shownMessage.cStr()));
        return;
    }
    if(arg[0] && q_strcasecmp(arg, "force"))
    {
        Con_Printf("vr_relight_get_tool [force | cancel | status]: downloads ericw-tools %s's light (%s) into %s\n",
            version, sizeText, installDir());
        return;
    }
    if(q_strcasecmp(arg, "force") && relight::toolFound())
    {
        Con_Printf("relight: %s - nothing to download (vr_relight_get_tool force: download it anyway)\n", relight::toolLine());
        return;
    }
    start();
}

} // namespace qvr::relight::tool
