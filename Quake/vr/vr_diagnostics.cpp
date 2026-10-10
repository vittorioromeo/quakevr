// vr_diagnostics.cpp -- diagnostics mode (1.0.1): everything a bug report needs, in one folder.
//
// "-diagnostics" on the command line (from the first console line; the installer's "Quake VR Unleashed (Diagnostics)"
// shortcut), or "vr_diagnostics 1" (from the next start, once the config is read: VID_Init), turns on, for that run:
//   - <game folder>/diagnostics/<date>_<time>/, printed at the start and at the end (opened in Explorer at a quit);
//   - console.log: the console, from the first line (-diagnostics) or from the window's creation (the cvar);
//   - gl_debug.log: a debug GL context (SDL_GL_CONTEXT_DEBUG_FLAG) and synchronous GL debug output, every message with
//     its source, type, id and severity; each id's first 10, then every 100th, with counts (a summary at the end);
//   - openxr_debug.log: the XR_EXT_debug_utils messenger (all severities and types: the loader's and the runtime's
//     messages), Khronos' core validation layer when it is installed (never shipped: ROUND22.md), the context checks;
//   - debug_output.log: whatever the process says to a debugger (OutputDebugString): the OpenXR loader's log
//     (XR_LOADER_DEBUG=all), runtimes', drivers';
//   - crash/: the crash report and dump, if any;
//   - at a quit or a crash, copies of: gl_startup.log, qvr_openxr.txt, and the runtimes' logs written during the run
//     (SteamVR: <Steam>/logs/vrserver.txt, vrcompositor.txt, vrmonitor.txt, vrclient_ironwail.txt; Virtual Desktop:
//     %ProgramData%/Virtual Desktop/OpenXR.log (VDXR's) and StreamerLog.txt). Meta's are not looked for.
// Off (the default), none of it runs: one branch in the GL debug callback, which is only installed with -gldebug.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_xr_runtime.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

using namespace qvr;

extern "C" char com_gamedir[];
extern "C" void Sys_mkdir(const char* path);
extern "C" void LOG_Reopen(const char* path);

namespace
{

constexpr int idSlots = 512;
constexpr unsigned firstShown = 10; // each id's first messages logged, then every 100th

struct IdCount
{
    unsigned key = 0; // the GL id, or the XR message id's hash (0: free)
    unsigned count = 0;
    char what[96]{};  // for the summary
};

struct State
{
    bool on = false;
    bool ended = false;
    char why[64]{};
    char dir[1024]{};
    FILE* gl = nullptr;
    FILE* xr = nullptr;
    FILE* debugOut = nullptr; // debug_output.log
    unsigned glTotal = 0;
    unsigned xrTotal = 0;
    IdCount glIds[idSlots]{};
    IdCount xrIds[idSlots]{};
    time_t began = 0;
#ifdef _WIN32
    SRWLOCK lock = SRWLOCK_INIT; // the XR messenger can be called from a runtime's thread
#endif
};
State diag;
// The game's own OutputDebugString (Sys_Printf: the console, in console.log already) left out of debug_output.log; a
// wide message's narrow copy (Windows raises both) too.
thread_local bool ownOutput = false;
thread_local bool skipNarrow = false;

void lock()
{
#ifdef _WIN32
    AcquireSRWLockExclusive(&diag.lock);
#endif
}

void unlock()
{
#ifdef _WIN32
    ReleaseSRWLockExclusive(&diag.lock);
#endif
}

FILE* openLog(const char* name)
{
    char path[1200];
    q_snprintf(path, sizeof(path), "%s/%s", diag.dir, name);
    FILE* f = fopen(path, "wb");
    if(f)
    {
        setvbuf(f, nullptr, _IONBF, 0); // (in the file at once: a crash loses nothing)
    }
    return f;
}

void writeSummaryLine(const char* text)
{
    char path[1200];
    q_snprintf(path, sizeof(path), "%s/diagnostics.txt", diag.dir);
    if(FILE* f = fopen(path, "ab"))
    {
        fputs(text, f);
        fclose(f);
    }
}

// The slot of a key (GL id, XR id hash), and whether this occurrence is logged.
bool countId(IdCount* ids, unsigned key, const char* what, unsigned& count)
{
    key = key ? key : 1u;
    for(int i = 0; i < idSlots; i++)
    {
        IdCount& slot = ids[(key + static_cast<unsigned>(i)) % idSlots];
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
    return true; // (the table full: every one logged)
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

#ifdef _WIN32
// Everything said to a debugger (OutputDebugString: the OpenXR loader's own log, which it sends there and to its debug
// messengers, runtimes', drivers'), caught in the process as the exception OutputDebugString raises, into
// debug_output.log. (XR_LOADER_DEBUG=all is set too, but this loader, 1.1.63, reads it as it loads with the game,
// before: its messages come through the debug messenger, openxr_debug.log.)
LONG CALLBACK debugOutputHandler(EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if((code != DBG_PRINTEXCEPTION_C && code != 0x4001000Au /* DBG_PRINTEXCEPTION_WIDE_C */) || !diag.debugOut ||
        ep->ExceptionRecord->NumberParameters < 2)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if(ownOutput || (code == DBG_PRINTEXCEPTION_C && skipNarrow))
    {
        skipNarrow = false;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    skipNarrow = code != DBG_PRINTEXCEPTION_C;
    const ULONG_PTR length = ep->ExceptionRecord->ExceptionInformation[0];
    const ULONG_PTR text = ep->ExceptionRecord->ExceptionInformation[1];
    lock();
    if(code == DBG_PRINTEXCEPTION_C)
    {
        fprintf(diag.debugOut, "%9.3f %.*s", Sys_DoubleTime(), static_cast<int>(length > 4000 ? 4000 : length),
            reinterpret_cast<const char*>(text));
    }
    else
    {
        fprintf(diag.debugOut, "%9.3f %.*ls", Sys_DoubleTime(), static_cast<int>(length > 4000 ? 4000 : length),
            reinterpret_cast<const wchar_t*>(text));
    }
    unlock();
    return EXCEPTION_CONTINUE_SEARCH; // (OutputDebugString handles its own exception, and a debugger still sees it)
}

void catchDebugOutput()
{
    diag.debugOut = openLog("debug_output.log");
    const PVOID handler = diag.debugOut ? AddVectoredExceptionHandler(0, debugOutputHandler) : nullptr;
    writeSummaryLine(va("debug_output.log: OutputDebugString (the OpenXR loader with XR_LOADER_DEBUG=all, runtimes, drivers): "
                        "%s\n",
        handler ? "caught" : "NOT caught"));
}

// In the process's environment, the game's CRT's and the shared ucrtbase.dll's (the loader's getenv reads its own copy).
void setEnv(const char* var, const char* value)
{
    SetEnvironmentVariableA(var, value);
    _putenv_s(var, value);
    using PutenvFunc = int(__cdecl*)(const char*, const char*);
    HMODULE ucrt = GetModuleHandleA("ucrtbase.dll");
    if(const auto put = ucrt ? reinterpret_cast<PutenvFunc>(GetProcAddress(ucrt, "_putenv_s")) : nullptr)
    {
        put(var, value);
    }
}

// A copy of src (when it exists and was written since the run began, or always) into the folder as name.
void copyIn(const char* src, const char* name, bool always)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(!GetFileAttributesExA(src, GetFileExInfoStandard, &data))
    {
        return;
    }
    ULARGE_INTEGER t;
    t.LowPart = data.ftLastWriteTime.dwLowDateTime;
    t.HighPart = data.ftLastWriteTime.dwHighDateTime;
    const time_t written = static_cast<time_t>(t.QuadPart / 10000000ull - 11644473600ull);
    if(!always && written + 60 < diag.began)
    {
        writeSummaryLine(va("  %s: not written during this run, not copied\n", src));
        return;
    }
    char dst[1200];
    q_snprintf(dst, sizeof(dst), "%s/%s", diag.dir, name);
    const BOOL ok = CopyFileA(src, dst, FALSE);
    writeSummaryLine(va("  %s -> %s%s\n", src, name, ok ? "" : " (FAILED: in use?)"));
}

void copyLogs()
{
    writeSummaryLine("Logs copied:\n");
    char path[1200];
    q_snprintf(path, sizeof(path), "%s/crash/gl_startup.log", com_gamedir);
    copyIn(path, "gl_startup.log", true);
    copyIn(xrruntime::logPath(), "qvr_openxr.txt", false);
    // SteamVR: under Steam's folder (the registry's SteamPath).
    char steam[MAX_PATH] = "";
    DWORD size = sizeof(steam);
    if(RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath", RRF_RT_REG_SZ, nullptr, steam, &size) ==
        ERROR_SUCCESS)
    {
        for(const char* log : {"vrserver.txt", "vrcompositor.txt", "vrmonitor.txt", "vrclient_ironwail.txt", "vrstartup.txt"})
        {
            q_snprintf(path, sizeof(path), "%s/logs/%s", steam, log);
            copyIn(path, va("steamvr_%s", log), false);
        }
    }
    // Virtual Desktop: VDXR's log and the Streamer's, in %ProgramData%\Virtual Desktop (seen on the author's PC).
    char programData[MAX_PATH] = "";
    if(GetEnvironmentVariableA("ProgramData", programData, sizeof(programData)))
    {
        q_snprintf(path, sizeof(path), "%s\\Virtual Desktop\\OpenXR.log", programData);
        copyIn(path, "vdxr_OpenXR.log", false);
        q_snprintf(path, sizeof(path), "%s\\Virtual Desktop\\StreamerLog.txt", programData);
        copyIn(path, "vd_StreamerLog.txt", false);
    }
}
#endif

void writeIdSummary(FILE* f, const IdCount* ids, unsigned total, const char* what)
{
    if(!f)
    {
        return;
    }
    fprintf(f, "\n== %u %s messages in all; by id:\n", total, what);
    for(int i = 0; i < idSlots; i++)
    {
        if(ids[i].key)
        {
            fprintf(f, "  %6u x %s\n", ids[i].count, ids[i].what);
        }
    }
}

void status_f()
{
    if(!diag.on)
    {
        Con_Printf("Diagnostics mode is off: start with -diagnostics (or vr_diagnostics 1, from the next start)\n");
        return;
    }
    Con_Printf("Diagnostics mode on (%s): %s\n", diag.why, diag.dir);
    Con_Printf("  GL debug messages: %u; OpenXR debug messages: %u\n", diag.glTotal, diag.xrTotal);
}

} // namespace

// After the filesystem (Host_Init; -diagnostics) and again at VID_Init (vr_diagnostics, read from the config by then).
extern "C" void VR_DiagnosticsBegin(void)
{
    if(diag.on || !com_gamedir[0])
    {
        return;
    }
    const bool param = COM_CheckParm("-diagnostics") != 0;
    if(!param && vr_diagnostics.value == 0.f)
    {
        return;
    }
    diag.on = true;
    diag.began = time(nullptr);
    q_strlcpy(diag.why, param ? "-diagnostics" : "vr_diagnostics 1", sizeof(diag.why));
    char stampText[32];
    strftime(stampText, sizeof(stampText), "%Y-%m-%d_%H-%M-%S", localtime(&diag.began));
    char parent[1024];
    q_snprintf(parent, sizeof(parent), "%s/diagnostics", com_gamedir);
    Sys_mkdir(parent);
    q_snprintf(diag.dir, sizeof(diag.dir), "%s/%s", parent, stampText);
    Sys_mkdir(diag.dir);

    char path[1200];
    q_snprintf(path, sizeof(path), "%s/diagnostics.txt", diag.dir);
    if(FILE* f = fopen(path, "wb"))
    {
        fprintf(f, "Quake VR %s diagnostics (%s), started %s\n", VR_BuildVersion(), diag.why, stampText);
#ifdef _WIN32
        fprintf(f, "Command line: %s\n", GetCommandLineA());
#endif
        fprintf(f, "Files: console.log, gl_debug.log, openxr_debug.log, debug_output.log, crash/ (if it crashed), and the "
                   "logs copied at the end (listed below)\n");
        fclose(f);
    }
    q_snprintf(path, sizeof(path), "%s/console.log", diag.dir);
    LOG_Reopen(path);
    diag.gl = openLog("gl_debug.log");
    diag.xr = openLog("openxr_debug.log");
    if(diag.gl)
    {
        fprintf(diag.gl, "# GL debug output (a debug context, synchronous): time, source, type, id, severity, message; "
                         "each id's first %u, then every 100th\n",
            firstShown);
    }
    if(diag.xr)
    {
        fputs("# OpenXR: the debug utils messenger's messages (loader and runtime), the backend's checks\n", diag.xr);
    }
    VR_SetCrashDir(diag.dir); // (the reports in <folder>/crash)
#ifdef _WIN32
    setEnv("XR_LOADER_DEBUG", "all");
    // Khronos' core validation layer, when a copy sits next to the game (openxr_layers\; not shipped) or is installed.
    char exe[MAX_PATH];
    if(GetModuleFileNameA(nullptr, exe, sizeof(exe)))
    {
        if(char* slash = strrchr(exe, '\\'))
        {
            q_strlcpy(slash + 1, "openxr_layers", sizeof(exe) - static_cast<size_t>(slash + 1 - exe));
            if(GetFileAttributesA(exe) != INVALID_FILE_ATTRIBUTES)
            {
                setEnv("XR_API_LAYER_PATH", exe);
                writeSummaryLine(va("XR_API_LAYER_PATH: %s\n", exe));
            }
        }
    }
    catchDebugOutput();
#endif
    Con_Printf("\x02" "Diagnostics mode (%s): logs go to %s\n", diag.why, diag.dir);
}

// Sys_Printf, around its OutputDebugString: the game's own (not in debug_output.log).
extern "C" void VR_DiagnosticsOwnOutput(int on)
{
    ownOutput = on != 0;
}

extern "C" int VR_DiagnosticsOn(void)
{
    return diag.on ? 1 : 0;
}

extern "C" const char* VR_DiagnosticsDir(void)
{
    return diag.on ? diag.dir : "";
}

extern "C" void VR_DiagnosticsGL(unsigned source, unsigned type, unsigned id, unsigned severity, const char* message)
{
    if(!diag.gl)
    {
        return;
    }
    diag.glTotal++;
    char what[96];
    q_snprintf(what, sizeof(what), "GL id %u (source 0x%x, type 0x%x, severity 0x%x)", id, source, type, severity);
    unsigned count = 0;
    if(countId(diag.glIds, id ^ (type << 20), what, count))
    {
        fprintf(diag.gl, "%9.3f source 0x%x type 0x%x id %u severity 0x%x (%u so far): %s\n", Sys_DoubleTime(), source, type,
            id, severity, count, message ? message : "");
    }
}

// (From any thread: a runtime calls its messenger from its own; no va() here.)
extern "C" void VR_DiagnosticsXr(const char* severity, const char* messageId, const char* function, const char* message)
{
    if(!diag.xr)
    {
        return;
    }
    char what[96];
    q_snprintf(what, sizeof(what), "%s %s", messageId ? messageId : "-", function ? function : "-");
    lock();
    diag.xrTotal++;
    unsigned count = 0;
    if(countId(diag.xrIds, hashText(messageId) ^ hashText(function), what, count))
    {
        fprintf(diag.xr, "%9.3f [%s] %s (%u so far): %s\n", Sys_DoubleTime(), severity ? severity : "?", what, count,
            message ? message : "");
    }
    unlock();
}

extern "C" void VR_DiagnosticsNote(const char* line)
{
    if(diag.xr && line)
    {
        lock();
        fprintf(diag.xr, "%9.3f %s", Sys_DoubleTime(), line);
        unlock();
    }
}

// Host_Shutdown (crashed 0) and the crash handler (1): the summaries, the logs copied, the folder named.
extern "C" void VR_DiagnosticsEnd(int crashed)
{
    if(!diag.on || diag.ended)
    {
        return;
    }
    diag.ended = true;
    writeIdSummary(diag.gl, diag.glIds, diag.glTotal, "GL debug");
    writeIdSummary(diag.xr, diag.xrIds, diag.xrTotal, "OpenXR debug");
    writeSummaryLine(va("Ended: %s, %.0f s after the start\n", crashed ? "a crash (crash/ has the report)" : "a quit or an error",
        difftime(time(nullptr), diag.began)));
#ifdef _WIN32
    copyLogs();
#endif
    if(!crashed)
    {
        Con_Printf("Diagnostics: everything is in %s\n", diag.dir);
#ifdef _WIN32
        if(!getenv("QVR_NO_ERROR_DIALOG") && !getenv("QVR_TEST_HIDDEN"))
        {
            char full[MAX_PATH];
            if(GetFullPathNameA(diag.dir, sizeof(full), full, nullptr))
            {
                ShellExecuteA(nullptr, "open", full, nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
#endif
    }
}

extern "C" void VR_DiagnosticsInit(void)
{
    Cmd_AddCommand("vr_diagnostics_status", status_f);
}
