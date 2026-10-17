// vr_runlogs.cpp -- every run's logs (always on, cheap: ROUND22.md, "Always-on logs and the bug report") and the bug
// report made from them.
//
// <game folder>/logs/<date>_<time>/, a folder a run (the newest 5 kept: older ones removed as a run starts):
//   - console.log: the console from its first line (buffered, a write a second: console.c, LOG_OpenRun);
//   - system.txt: the build, the command line, Windows' version, the GPU and its driver, each VR start's runtime with
//     the API layers and overlays around it, the incidents (below);
//   - qvr_openxr.txt: the VR starts' log (vr_xr_runtime.hpp: what <game folder>/qvr_openxr.txt gets, this run's);
//   - openxr_debug.log: the XR_EXT_debug_utils messenger (the loader's and the runtime's messages; info, warnings and
//     errors, diagnostics mode's verbose ones too): each message id's first 10, then every 100th, 20000 lines at most;
//   - runtime/: at an incident (a VR start failed, a crash inside the runtime caught, the game crashed), the runtimes'
//     logs written during the run (Virtual Desktop's OpenXR.log, VDXR's, and StreamerLog.txt; SteamVR's), gl_startup.log
//     and qvr_openxr.txt, copied.
// Diagnostics mode (vr_diagnostics.cpp) keeps the rest: the debug GL context, the validation layer, XR_LOADER_DEBUG.
//
// The bug report: the console's bugreport, Options > Create Bug Report, VR Settings > Headset > Create Bug Report; or
// -bugreport (the installer's "Quake VR Unleashed (Create Bug Report)"): the last run's, made without starting the
// game. <game folder>/bugreports/<date>_<time>.zip (stored, not compressed: the tree's miniz only reads) with the newest
// two runs' folders, the crash reports since, gl_startup.log, the config, the runtimes' logs written since, the newest
// diagnostics folder since, and summary.txt (the build, Windows, the GPU, the runtime, the API layers and overlays, the
// files); the folder opened in Explorer.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_runlogs.hpp"
#include "vr_xr_runtime.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

using namespace qvr;

extern "C" char com_gamedir[];
extern "C" void LOG_OpenRun(const char* path);
extern "C" void LOG_Flush(void);

namespace
{

constexpr int keptRuns = 5;
constexpr int reportRuns = 2;         // the bug report's runs: the newest (this one, or the last) and the one before
constexpr int idSlots = 512;
constexpr unsigned firstShown = 10;   // each message id's first ones logged, then every 100th
constexpr unsigned xrLineCap = 20000; // openxr_debug.log's lines at most (then counted only)
constexpr uint64_t fileCap = 64ull << 20; // a file copied or zipped: its last 64 MB at most
constexpr int maxNames = 128;
constexpr int nameSize = 64;

struct IdCount
{
    unsigned key = 0; // the message id's hash (0: free)
    unsigned count = 0;
    char what[96]{};
};

struct State
{
    bool begun = false;
    char dir[1024]{}; // <game folder>/logs/<stamp> ("" without one)
    char stamp[nameSize]{};
    time_t began = 0;
    FILE* xr = nullptr; // openxr_debug.log, opened at the first message
    bool xrOpened = false;
    unsigned xrTotal = 0;
    unsigned xrWritten = 0;
    IdCount xrIds[idSlots]{};
#ifdef _WIN32
    SRWLOCK lock = SRWLOCK_INIT; // the messenger is called from the runtime's threads too
#endif
};
State logs;

void lock()
{
#ifdef _WIN32
    AcquireSRWLockExclusive(&logs.lock);
#endif
}

void unlock()
{
#ifdef _WIN32
    ReleaseSRWLockExclusive(&logs.lock);
#endif
}

void systemLine(const char* line)
{
    runlogs::append("system.txt", line);
}

void stampOf(time_t t, char* out, size_t size)
{
    const tm* local = localtime(&t);
    if(!local || !strftime(out, size, "%Y-%m-%d_%H-%M-%S", local))
    {
        q_strlcpy(out, "unknown", size);
    }
}

// A run's folder name, "2026-10-17_14-05-09" (with "_2".. for a second run in the same second), and its time.
bool runTime(const char* name, time_t* t)
{
    constexpr const char* shape = "dddd-dd-dd_dd-dd-dd";
    for(int i = 0; shape[i]; i++)
    {
        const char c = name[i];
        if(shape[i] == 'd' ? (c < '0' || c > '9') : c != shape[i])
        {
            return false;
        }
    }
    if(name[19] && name[19] != '_')
    {
        return false;
    }
    if(t)
    {
        tm parts{};
        parts.tm_year = atoi(name) - 1900;
        parts.tm_mon = atoi(name + 5) - 1;
        parts.tm_mday = atoi(name + 8);
        parts.tm_hour = atoi(name + 11);
        parts.tm_min = atoi(name + 14);
        parts.tm_sec = atoi(name + 17);
        parts.tm_isdst = -1;
        *t = mktime(&parts);
    }
    return true;
}

// The CRC-32 of the zip files (miniz's is static to miniz.c).
struct CrcTable
{
    uint32_t v[256];
};

constexpr CrcTable makeCrcTable()
{
    CrcTable t{};
    for(uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for(int k = 0; k < 8; k++)
        {
            c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        t.v[i] = c;
    }
    return t;
}

constexpr CrcTable crcTable = makeCrcTable();

uint32_t crc32Of(const char* data, size_t size)
{
    uint32_t c = 0xFFFFFFFFu;
    for(size_t i = 0; i < size; i++)
    {
        c = crcTable.v[(c ^ static_cast<unsigned char>(data[i])) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

#ifdef _WIN32
time_t timeOf(const FILETIME& ft)
{
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    return static_cast<time_t>(t.QuadPart / 10000000ull - 11644473600ull);
}

void makeDir(const char* path)
{
    CreateDirectoryA(path, nullptr); // (no Sys_mkdir: its failure is a Sys_Error, and the crash reporter calls here)
}

// A file read whole while others may write it (a runtime's open log): its last fileCap bytes; malloc'd (null: none).
char* readShared(const char* path, size_t* size, time_t* written)
{
    *size = 0;
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if(f == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }
    LARGE_INTEGER length{};
    FILETIME ft{};
    GetFileSizeEx(f, &length);
    GetFileTime(f, nullptr, nullptr, &ft);
    if(written)
    {
        *written = timeOf(ft);
    }
    const uint64_t total = static_cast<uint64_t>(length.QuadPart);
    const uint64_t take = total > fileCap ? fileCap : total;
    if(total > take)
    {
        LARGE_INTEGER at;
        at.QuadPart = static_cast<LONGLONG>(total - take);
        SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
    }
    char* data = static_cast<char*>(malloc(static_cast<size_t>(take) + 1));
    size_t done = 0;
    DWORD got = 0;
    while(data && done < take &&
          ReadFile(f, data + done, static_cast<DWORD>(take - done > (1u << 24) ? (1u << 24) : take - done), &got, nullptr) && got)
    {
        done += got;
    }
    CloseHandle(f);
    *size = done;
    return data;
}

bool writeWhole(const char* path, const char* data, size_t size)
{
    FILE* f = fopen(path, "wb");
    if(!f)
    {
        return false;
    }
    const bool ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

time_t writtenAt(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    return GetFileAttributesExA(path, GetFileExInfoStandard, &data) ? timeOf(data.ftLastWriteTime) : 0;
}

bool exists(const char* path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// A folder's entries' names (files or folders), sorted.
struct Names
{
    char name[maxNames][nameSize];
    int count = 0;
};

int compareNames(const void* a, const void* b)
{
    return strcmp(static_cast<const char*>(a), static_cast<const char*>(b));
}

void listNames(const char* dir, bool folders, Names& out)
{
    out.count = 0;
    char pattern[1200];
    q_snprintf(pattern, sizeof(pattern), "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if(h == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        const bool folder = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if(folder == folders && strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, "..") && out.count < maxNames &&
            strlen(fd.cFileName) < nameSize)
        {
            q_strlcpy(out.name[out.count++], fd.cFileName, nameSize);
        }
    } while(FindNextFileA(h, &fd));
    FindClose(h);
    qsort(out.name, static_cast<size_t>(out.count), nameSize, compareNames);
}

// The run folders in logs/, oldest first.
void listRuns(const char* parent, Names& out)
{
    listNames(parent, true, out);
    int kept = 0;
    for(int i = 0; i < out.count; i++)
    {
        if(runTime(out.name[i], nullptr))
        {
            memmove(out.name[kept++], out.name[i], nameSize);
        }
    }
    out.count = kept;
}

// A run's folder removed (its files, its runtime/ folder's).
void removeRun(const char* dir)
{
    Names files;
    listNames(dir, false, files);
    char path[1200];
    for(int i = 0; i < files.count; i++)
    {
        q_snprintf(path, sizeof(path), "%s/%s", dir, files.name[i]);
        DeleteFileA(path);
    }
    q_snprintf(path, sizeof(path), "%s/runtime", dir);
    listNames(path, false, files);
    for(int i = 0; i < files.count; i++)
    {
        DeleteFileA(va("%s/%s", path, files.name[i]));
    }
    RemoveDirectoryA(path);
    RemoveDirectoryA(dir);
}

// The logs copied at an incident and put in the bug report, besides the run's own.
struct Source
{
    char path[1200];
    const char* name;
    bool always;  // copied whenever it exists (else only when written during the run)
    bool runtime; // a runtime's (not the game's)
};

int sources(Source* out, int max)
{
    int n = 0;
    auto add = [&](const char* path, const char* name, bool always, bool runtime) {
        if(n < max)
        {
            q_strlcpy(out[n].path, path, sizeof(out[n].path));
            out[n].name = name;
            out[n].always = always;
            out[n].runtime = runtime;
            n++;
        }
    };
    add(va("%s/crash/gl_startup.log", com_gamedir), "gl_startup.log", true, false);
    add(xrruntime::logPath(), "qvr_openxr.txt", false, false);
    // SteamVR: under Steam's folder (the registry's SteamPath).
    char steam[MAX_PATH] = "";
    DWORD size = sizeof(steam);
    if(RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steam, &size) == ERROR_SUCCESS)
    {
        add(va("%s/logs/vrserver.txt", steam), "steamvr_vrserver.txt", false, true);
        add(va("%s/logs/vrcompositor.txt", steam), "steamvr_vrcompositor.txt", false, true);
        add(va("%s/logs/vrmonitor.txt", steam), "steamvr_vrmonitor.txt", false, true);
        add(va("%s/logs/vrclient_ironwail.txt", steam), "steamvr_vrclient_ironwail.txt", false, true);
        add(va("%s/logs/vrstartup.txt", steam), "steamvr_vrstartup.txt", false, true);
    }
    // Virtual Desktop: VDXR's log and the Streamer's, in %ProgramData%\Virtual Desktop. (Meta's are not looked for.)
    char programData[MAX_PATH] = "";
    if(GetEnvironmentVariableA("ProgramData", programData, sizeof(programData)))
    {
        add(va("%s\\Virtual Desktop\\OpenXR.log", programData), "vdxr_OpenXR.log", false, true);
        add(va("%s\\Virtual Desktop\\StreamerLog.txt", programData), "vd_StreamerLog.txt", false, true);
    }
    return n;
}

// One file copied (shared: a runtime may have it open), when it exists and was written since `since` (or always).
void copyIn(const Source& s, const char* dir, time_t since, runlogs::LineFn line)
{
    if(!exists(s.path))
    {
        return;
    }
    if(!s.always && writtenAt(s.path) + 60 < since)
    {
        if(line)
        {
            line(va("  %s: not written during this run, not copied\n", s.path));
        }
        return;
    }
    size_t size = 0;
    char* data = readShared(s.path, &size, nullptr);
    char dst[1200];
    q_snprintf(dst, sizeof(dst), "%s/%s", dir, s.name);
    const bool ok = data && writeWhole(dst, data, size);
    free(data);
    if(line)
    {
        line(va("  %s -> %s%s\n", s.path, s.name, ok ? "" : " (FAILED)"));
    }
}

void openFolder(const char* path, bool select)
{
    if(getenv("QVR_NO_ERROR_DIALOG") || getenv("QVR_TEST_HIDDEN"))
    {
        return; // (an automated test run: no Explorer)
    }
    char full[MAX_PATH];
    if(GetFullPathNameA(path, sizeof(full), full, nullptr))
    {
        for(char* c = full; *c; c++)
        {
            *c = *c == '/' ? '\\' : *c;
        }
        if(select)
        {
            ShellExecuteA(nullptr, "open", "explorer.exe", va("/select,\"%s\"", full), nullptr, SW_SHOWNORMAL);
        }
        else
        {
            ShellExecuteA(nullptr, "open", full, nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
}

const char* windowsVersion()
{
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    const auto get = ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    if(!get || get(&v) != 0)
    {
        return "unknown";
    }
    return va("%lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
}

// A zip of stored files (no compression; no zip64: each file within fileCap, the whole within 4 GB).
struct ZipEntry
{
    char name[256];
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
    uint16_t time;
    uint16_t date;
};

struct Zip
{
    FILE* f = nullptr;
    za::Vector<ZipEntry> entries;
    uint64_t offset = 0;
    bool failed = false;
    za::String listing; // summary.txt's file list
};

void put16(unsigned char*& p, unsigned v)
{
    *p++ = static_cast<unsigned char>(v & 0xFFu);
    *p++ = static_cast<unsigned char>((v >> 8) & 0xFFu);
}

void put32(unsigned char*& p, uint32_t v)
{
    put16(p, v & 0xFFFFu);
    put16(p, v >> 16);
}

void dosTime(time_t t, uint16_t& time, uint16_t& date)
{
    const tm* l = localtime(&t);
    if(!l || l->tm_year < 80)
    {
        time = 0;
        date = (1u << 5) | 1u;
        return;
    }
    time = static_cast<uint16_t>((l->tm_hour << 11) | (l->tm_min << 5) | (l->tm_sec / 2));
    date = static_cast<uint16_t>(((l->tm_year - 80) << 9) | ((l->tm_mon + 1) << 5) | l->tm_mday);
}

void zipAdd(Zip& z, const char* name, const char* data, size_t size, time_t when)
{
    const size_t nameLength = strlen(name);
    if(!z.f || z.failed || nameLength >= sizeof(ZipEntry::name) || z.offset + size + 512 > 0xF0000000ull)
    {
        return;
    }
    ZipEntry e{};
    q_strlcpy(e.name, name, sizeof(e.name));
    e.crc = crc32Of(data, size);
    e.size = static_cast<uint32_t>(size);
    e.offset = static_cast<uint32_t>(z.offset);
    dosTime(when, e.time, e.date);
    unsigned char header[30];
    unsigned char* p = header;
    put32(p, 0x04034b50u);
    put16(p, 10); // (version 1.0: stored)
    put16(p, 0);
    put16(p, 0); // stored
    put16(p, e.time);
    put16(p, e.date);
    put32(p, e.crc);
    put32(p, e.size);
    put32(p, e.size);
    put16(p, static_cast<unsigned>(nameLength));
    put16(p, 0);
    z.failed = fwrite(header, 1, sizeof(header), z.f) != sizeof(header) || fwrite(name, 1, nameLength, z.f) != nameLength ||
               (size && fwrite(data, 1, size, z.f) != size);
    z.offset += sizeof(header) + nameLength + size;
    z.entries.pushBack(e);
    z.listing += va("  %s (%u bytes)\n", e.name, static_cast<unsigned>(size));
}

bool zipFinish(Zip& z)
{
    if(!z.f)
    {
        return false;
    }
    const uint64_t start = z.offset;
    for(const ZipEntry& e : z.entries)
    {
        const size_t nameLength = strlen(e.name);
        unsigned char header[46];
        unsigned char* p = header;
        put32(p, 0x02014b50u);
        put16(p, 20);
        put16(p, 10);
        put16(p, 0);
        put16(p, 0);
        put16(p, e.time);
        put16(p, e.date);
        put32(p, e.crc);
        put32(p, e.size);
        put32(p, e.size);
        put16(p, static_cast<unsigned>(nameLength));
        put16(p, 0);
        put16(p, 0);
        put16(p, 0);
        put16(p, 0);
        put32(p, 0);
        put32(p, e.offset);
        z.failed = z.failed || fwrite(header, 1, sizeof(header), z.f) != sizeof(header) ||
                   fwrite(e.name, 1, nameLength, z.f) != nameLength;
        z.offset += sizeof(header) + nameLength;
    }
    unsigned char end[22];
    unsigned char* p = end;
    put32(p, 0x06054b50u);
    put16(p, 0);
    put16(p, 0);
    put16(p, static_cast<unsigned>(z.entries.size()));
    put16(p, static_cast<unsigned>(z.entries.size()));
    put32(p, static_cast<uint32_t>(z.offset - start));
    put32(p, static_cast<uint32_t>(start));
    put16(p, 0);
    z.failed = z.failed || fwrite(end, 1, sizeof(end), z.f) != sizeof(end);
    const bool closed = fclose(z.f) == 0;
    z.f = nullptr;
    return closed && !z.failed;
}

void zipAddFile(Zip& z, const char* name, const char* path)
{
    size_t size = 0;
    time_t written = 0;
    char* data = readShared(path, &size, &written);
    if(data)
    {
        zipAdd(z, name, data, size, written);
        free(data);
    }
}

// A folder's files (and its subfolders' files, `depth` deep) under prefix/.
void zipAddDir(Zip& z, const char* dir, const char* prefixText, int depth)
{
    char prefix[400];
    q_strlcpy(prefix, prefixText, sizeof(prefix)); // (often a va(): the calls below reuse its buffers)
    Names names;
    listNames(dir, false, names);
    char path[1200];
    char name[400];
    for(int i = 0; i < names.count; i++)
    {
        q_snprintf(path, sizeof(path), "%s/%s", dir, names.name[i]);
        q_snprintf(name, sizeof(name), "%s/%s", prefix, names.name[i]);
        zipAddFile(z, name, path);
    }
    if(depth > 0)
    {
        listNames(dir, true, names);
        for(int i = 0; i < names.count; i++)
        {
            q_snprintf(path, sizeof(path), "%s/%s", dir, names.name[i]);
            q_snprintf(name, sizeof(name), "%s/%s", prefix, names.name[i]);
            zipAddDir(z, path, name, depth - 1);
        }
    }
}

// The bug report: out gets the zip's path, or why there is none.
bool makeReport(bool lastRun, char* out, size_t outSize)
{
    LOG_Flush();
    lock();
    if(logs.xr)
    {
        fflush(logs.xr);
    }
    unlock();

    const time_t now = time(nullptr);
    char stamp[nameSize];
    stampOf(now, stamp, sizeof(stamp));
    char reports[1100];
    q_snprintf(reports, sizeof(reports), "%s/bugreports", com_gamedir);
    makeDir(reports);
    q_snprintf(out, outSize, "%s/%s.zip", reports, stamp);
    for(int n = 2; exists(out) && n < 10; n++)
    {
        q_snprintf(out, outSize, "%s/%s_%d.zip", reports, stamp, n);
    }
    Zip z;
    z.f = fopen(out, "wb");
    if(!z.f)
    {
        q_snprintf(out, outSize, "%s/%s.zip can't be written", reports, stamp);
        return false;
    }

    za::String summary;
    summary += va("Quake VR: Unleashed bug report, made %s %s\n", stamp,
        lastRun ? "with -bugreport (the last run's logs; the game not started)" : "in the game (this run's logs)");
    summary += va("Build: %s\n", VR_BuildInfo());
    summary += va("Windows: %s\n", windowsVersion());
    summary += va("Game folder: %s\n", com_gamedir);
    if(!lastRun)
    {
        const char* status = xrruntime::statusLine();
        summary += va("OpenXR now: %s\n", status[0] ? status : "the OpenXR backend hasn't started");
    }
    summary += va("OpenXR %s\n", xrruntime::layersAndOverlays().cStr());

    // The newest runs (this one, or the last), oldest of them first.
    char parent[1100];
    q_snprintf(parent, sizeof(parent), "%s/logs", com_gamedir);
    Names runs;
    listRuns(parent, runs);
    const int first = runs.count > reportRuns ? runs.count - reportRuns : 0;
    time_t since = now - 24 * 60 * 60;
    if(runs.count)
    {
        runTime(runs.name[first], &since);
        since -= 60;
    }
    bool runReport = false; // a run's own qvr_openxr.txt included
    char path[1200];
    for(int i = runs.count - 1; i >= first; i--)
    {
        q_snprintf(path, sizeof(path), "%s/%s", parent, runs.name[i]);
        if(i == runs.count - 1)
        {
            // The newest run's system lines (the GPU, the runtime) in the summary too.
            size_t size = 0;
            char* text = readShared(va("%s/system.txt", path), &size, nullptr);
            if(text)
            {
                text[size] = 0;
                summary += va("\n== logs/%s/system.txt (%s):\n", runs.name[i], lastRun ? "the last run" : "this run");
                summary += text;
                free(text);
            }
        }
        runReport = runReport || exists(va("%s/qvr_openxr.txt", path));
        zipAddDir(z, path, va("logs/%s", runs.name[i]), 1);
    }
    if(!runs.count)
    {
        summary += "\nNo run's logs (logs/): the game hasn't run since they began (1.0.2)\n";
    }

    // Crash reports and dumps since, gl_startup.log; the starts' log if no run had its own.
    char crash[1100];
    q_snprintf(crash, sizeof(crash), "%s/crash", com_gamedir);
    Names files;
    listNames(crash, false, files);
    for(int i = 0; i < files.count; i++)
    {
        q_snprintf(path, sizeof(path), "%s/%s", crash, files.name[i]);
        if(!strcmp(files.name[i], "gl_startup.log") || writtenAt(path) >= since)
        {
            zipAddFile(z, va("crash/%s", files.name[i]), path);
        }
    }
    if(!runReport)
    {
        zipAddFile(z, "qvr_openxr.txt", xrruntime::logPath());
    }
    zipAddFile(z, "config/" CONFIG_NAME, va("%s/" CONFIG_NAME, com_gamedir));
    zipAddFile(z, "config/autoexec.cfg", va("%s/autoexec.cfg", com_gamedir));

    // The runtimes' logs as they are now, when written since.
    Source list[16];
    const int n = sources(list, 16);
    for(int i = 0; i < n; i++)
    {
        if(list[i].runtime && exists(list[i].path) && writtenAt(list[i].path) >= since)
        {
            zipAddFile(z, va("runtime_now/%s", list[i].name), list[i].path);
        }
    }

    // The newest diagnostics mode folder, if made since.
    char diagnostics[1100];
    q_snprintf(diagnostics, sizeof(diagnostics), "%s/diagnostics", com_gamedir);
    listRuns(diagnostics, files);
    time_t diagTime = 0;
    if(files.count && runTime(files.name[files.count - 1], &diagTime) && diagTime >= since)
    {
        q_snprintf(path, sizeof(path), "%s/%s", diagnostics, files.name[files.count - 1]);
        zipAddDir(z, path, va("diagnostics/%s", files.name[files.count - 1]), 1);
    }

    summary += "\n== Files:\n";
    summary += z.listing.cStr();
    zipAdd(z, "summary.txt", summary.cStr(), summary.size(), now);
    if(!zipFinish(z))
    {
        q_strlcat(out, " (FAILED to write: disk full?)", outSize);
        return false;
    }
    return true;
}

void bugreport_f()
{
    char path[1200];
    if(!makeReport(false, path, sizeof(path)))
    {
        Con_Warning("Bug report: not made: %s\n", path);
        return;
    }
    char full[MAX_PATH];
    const char* shown = GetFullPathNameA(path, sizeof(full), full, nullptr) ? full : path;
    Con_Printf("\x02" "Bug report saved: %s\n", shown);
    Con_Printf("Attach it to your bug report, with what you were doing (it has this run's and the last run's logs).\n");
    SCR_CenterPrintFor(va("Bug report saved:\n%s", shown), 6.f);
    openFolder(path, true);
}

[[noreturn]] void reportAndExit()
{
    char path[1200];
    const bool ok = makeReport(true, path, sizeof(path));
    Sys_Printf(ok ? "Bug report saved: %s\n" : "Bug report not made: %s\n", path);
    if(ok)
    {
        openFolder(path, true);
    }
    else if(!getenv("QVR_NO_ERROR_DIALOG") && !getenv("QVR_TEST_HIDDEN"))
    {
        MessageBoxA(nullptr, va("No bug report could be made:\n%s", path), "Quake VR: Unleashed - Bug Report", MB_OK | MB_ICONWARNING);
    }
    exit(ok ? 0 : 1);
}
#endif

void openXrLog()
{
    if(logs.xrOpened || !logs.dir[0])
    {
        return;
    }
    logs.xrOpened = true;
    char path[1200];
    q_snprintf(path, sizeof(path), "%s/openxr_debug.log", logs.dir);
    logs.xr = fopen(path, "wb");
    if(logs.xr)
    {
        setvbuf(logs.xr, nullptr, _IONBF, 0); // (in the file at once: a crash loses nothing; few lines: rate-limited)
        fprintf(logs.xr, "# OpenXR: the debug utils messenger's messages (the loader's and the runtime's; vr_diagnostics_xr), the "
                         "backend's checks; each id's first %u, then every 100th; %u lines at most\n",
            firstShown, xrLineCap);
    }
}

// The slot of a message id, and whether this occurrence is logged.
bool countId(unsigned key, const char* what, unsigned& count)
{
    key = key ? key : 1u;
    for(int i = 0; i < idSlots; i++)
    {
        IdCount& slot = logs.xrIds[(key + static_cast<unsigned>(i)) % idSlots];
        if(slot.key == 0)
        {
            slot.key = key;
            q_strlcpy(slot.what, what, sizeof(slot.what));
        }
        if(slot.key == key)
        {
            count = ++slot.count;
            return count <= firstShown || count % 100 == 0;
        }
    }
    count = 0;
    return true; // (the table full: each one logged, within xrLineCap)
}

unsigned hashText(const char* text)
{
    unsigned h = 2166136261u;
    for(const char* c = text ? text : ""; *c; c++)
    {
        h = (h ^ static_cast<unsigned char>(*c)) * 16777619u;
    }
    return h;
}

// A line of openxr_debug.log, within its cap (locked).
bool xrLineAllowed()
{
    if(!logs.xr || logs.xrWritten > xrLineCap)
    {
        return false;
    }
    if(logs.xrWritten++ == xrLineCap)
    {
        fprintf(logs.xr, "%9.3f (%u lines: the rest counted only, in the summary at the end)\n", Sys_DoubleTime(), xrLineCap);
        return false;
    }
    return true;
}

void runlogsStatus_f()
{
    Con_Printf("This run's logs: %s\n", logs.dir[0] ? logs.dir : "none");
    Con_Printf("  OpenXR debug messages: %u (%u lines written)\n", logs.xrTotal, logs.xrWritten);
}

} // namespace

namespace qvr::runlogs
{

const char* dir()
{
    return logs.dir;
}

void append(const char* name, const char* text)
{
    if(!logs.dir[0] || !text)
    {
        return;
    }
    char path[1200];
    q_snprintf(path, sizeof(path), "%s/%s", logs.dir, name);
    if(FILE* f = fopen(path, "ab"))
    {
        fputs(text, f);
        fclose(f);
    }
}

void copyRuntimeLogs(const char* dir, time_t since, LineFn line)
{
#ifdef _WIN32
    Source list[16];
    const int n = sources(list, 16);
    for(int i = 0; i < n; i++)
    {
        copyIn(list[i], dir, since, line);
    }
#else
    (void)dir;
    (void)since;
    (void)line;
#endif
}

void copyRunFile(const char* name, const char* dir)
{
#ifdef _WIN32
    if(!logs.dir[0])
    {
        return;
    }
    LOG_Flush();
    Source s{};
    q_snprintf(s.path, sizeof(s.path), "%s/%s", logs.dir, name);
    s.name = name;
    s.always = true;
    copyIn(s, dir, 0, nullptr);
#else
    (void)name;
    (void)dir;
#endif
}

unsigned xrMessages()
{
    return logs.xrTotal;
}

} // namespace qvr::runlogs

// After the filesystem (Host_Init): the run's folder, its console.log from the first line; -bugreport: the report.
extern "C" void VR_LogsBegin(void)
{
    if(logs.begun || !com_gamedir[0])
    {
        return;
    }
    logs.begun = true;
#ifdef _WIN32
    if(COM_CheckParm("-bugreport"))
    {
        reportAndExit();
    }
    logs.began = time(nullptr);
    char parent[1100];
    q_snprintf(parent, sizeof(parent), "%s/logs", com_gamedir);
    makeDir(parent);
    Names runs;
    listRuns(parent, runs);
    char path[1200];
    for(int i = 0; i + keptRuns - 1 < runs.count; i++) // (this run is the 5th)
    {
        q_snprintf(path, sizeof(path), "%s/%s", parent, runs.name[i]);
        removeRun(path);
    }
    stampOf(logs.began, logs.stamp, sizeof(logs.stamp));
    q_snprintf(logs.dir, sizeof(logs.dir), "%s/%s", parent, logs.stamp);
    for(int n = 2; exists(logs.dir) && n < 100; n++)
    {
        q_snprintf(logs.dir, sizeof(logs.dir), "%s/%s_%d", parent, logs.stamp, n);
    }
    makeDir(logs.dir);
    if(!exists(logs.dir))
    {
        logs.dir[0] = 0;
        LOG_OpenRun(nullptr);
        return;
    }
    q_snprintf(path, sizeof(path), "%s/console.log", logs.dir);
    LOG_OpenRun(path);
    systemLine(va("Quake VR: Unleashed %s, run of %s\n", VR_BuildInfo(), logs.stamp));
    systemLine(va("Command line: %s\n", GetCommandLineA()));
    systemLine(va("Windows: %s\n", windowsVersion()));
    systemLine(va("Game folder: %s\n", com_gamedir));
#else
    LOG_OpenRun(nullptr);
#endif
}

extern "C" void VR_LogsEnd(void)
{
    lock();
    if(logs.xr)
    {
        fprintf(logs.xr, "\n== %u OpenXR debug messages in all; by id:\n", logs.xrTotal);
        for(const IdCount& slot : logs.xrIds)
        {
            if(slot.key)
            {
                fprintf(logs.xr, "  %6u x %s\n", slot.count, slot.what);
            }
        }
    }
    unlock();
}

extern "C" void VR_LogsSystemLine(const char* line)
{
    systemLine(line);
}

// A VR start failed, a crash inside the runtime was caught, the game crashed (the crash reporter's thread).
extern "C" void VR_LogsIncident(const char* what)
{
    if(!logs.dir[0])
    {
        return;
    }
    LOG_Flush();
    char line[600];
    const time_t now = time(nullptr);
    char when[nameSize];
    stampOf(now, when, sizeof(when));
    q_snprintf(line, sizeof(line), "%s incident: %s; the runtimes' logs copied into runtime/:\n", when, what ? what : "?");
    systemLine(line);
#ifdef _WIN32
    char dir[1200];
    q_snprintf(dir, sizeof(dir), "%s/runtime", logs.dir);
    makeDir(dir);
    runlogs::copyRuntimeLogs(dir, logs.began, systemLine);
#endif
}

// (From any thread: a runtime calls its messenger from its own.)
extern "C" void VR_DiagnosticsXr(const char* severity, const char* messageId, const char* function, const char* message)
{
    char what[96];
    q_snprintf(what, sizeof(what), "%s %s", messageId ? messageId : "-", function ? function : "-");
    lock();
    openXrLog();
    logs.xrTotal++;
    unsigned count = 0;
    if(logs.xr && countId(hashText(messageId) ^ hashText(function), what, count) && xrLineAllowed())
    {
        fprintf(logs.xr, "%9.3f [%s] %s (%u so far): %s\n", Sys_DoubleTime(), severity ? severity : "?", what, count,
            message ? message : "");
    }
    unlock();
}

extern "C" void VR_DiagnosticsNote(const char* line)
{
    if(!line)
    {
        return;
    }
    lock();
    openXrLog();
    if(xrLineAllowed())
    {
        fprintf(logs.xr, "%9.3f %s", Sys_DoubleTime(), line);
    }
    unlock();
}

extern "C" void VR_LogsInit(void)
{
#ifdef _WIN32
    Cmd_AddCommand("bugreport", bugreport_f);
#endif
    Cmd_AddCommand("vr_logs_status", runlogsStatus_f);
}
