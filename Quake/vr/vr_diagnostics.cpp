// vr_diagnostics.cpp -- diagnostics mode (1.0.1): everything a bug report needs, in one folder.
//
// "-diagnostics" on the command line (from the first console line; the installer's "Quake VR Unleashed (diagnostics)"
// shortcut), or "vr_diagnostics 1" (from the next start, once the config is read: VID_Init), turns on, for that run:
//   - <game folder>/diagnostics/<date>_<time>/, printed at the start and at the end (opened in Explorer at a quit);
//   - console.log, openxr_debug.log, system.txt: copies of the run's own (every run keeps them: vr_runlogs.cpp, in
//     <game folder>/logs/<date>_<time>; in diagnostics mode the messenger's verbose messages too);
//   - gl_debug.log: a debug GL context (SDL_GL_CONTEXT_DEBUG_FLAG) and synchronous GL debug output, every message with
//     its source, type, id and severity; each id's first 10, then every 100th, with counts (a summary at the end);
//   - openxr_validation.log: Khronos' core validation layer (shipped in quakevr/xr_layers/, enabled only here: every
//     OpenXR call the game makes checked against the specification; useValidationLayer, vr_backend_openxr.cpp);
//   - debug_output.log: whatever the process says to a debugger (OutputDebugString): the OpenXR loader's log
//     (XR_LOADER_DEBUG=all), runtimes', drivers';
//   - crash/: the crash report and dump, if any;
//   - at a quit or a crash, copies of: gl_startup.log, qvr_openxr.txt, and the runtimes' logs written during the run
//     (SteamVR: <Steam>/logs/vrserver.txt, vrcompositor.txt, vrmonitor.txt, vrclient_ironwail.txt; Virtual Desktop:
//     %ProgramData%/Virtual Desktop/OpenXR.log (VDXR's) and StreamerLog.txt). Meta's are not looked for.
// Off (the default), none of it runs: one branch in the GL debug callback, which is only installed with -gldebug.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_runlogs.hpp"
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

namespace
{

constexpr int idSlots = 512;
constexpr unsigned firstShown = 10; // each id's first messages logged, then every 100th

struct IdCount
{
    unsigned key = 0; // the GL id (0: free)
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
    FILE* debugOut = nullptr; // debug_output.log
    unsigned glTotal = 0;
    IdCount glIds[idSlots]{};
    time_t began = 0;
#ifdef _WIN32
    SRWLOCK lock = SRWLOCK_INIT; // debug_output.log is written from any thread
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

// The slot of a key (GL id), and whether this occurrence is logged.
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

// Khronos' core validation layer (XR_APILAYER_LUNARG_core_validation, shipped in quakevr/xr_layers/: ROUND22.md,
// "OpenXR core validation layer"): its folder put first in XR_API_LAYER_PATH (the loader reads it at each layer
// enumeration and xrCreateInstance, with GetEnvironmentVariable), and its output to openxr_validation.log. The backend
// enables it by name (vr_backend_openxr.cpp); outside diagnostics mode none of this is set, so the loader never sees it.
void useValidationLayer()
{
    char dir[1024]{};
    for(const searchpath_t* s = com_searchpaths; s && !dir[0]; s = s->next)
    {
        if(!s->pack && (Sys_FileType(va("%s/xr_layers/XrApiLayer_core_validation.json", s->filename)) & FS_ENT_FILE))
        {
            q_snprintf(dir, sizeof(dir), "%s/xr_layers", s->filename);
        }
    }
    if(!dir[0])
    {
        writeSummaryLine("OpenXR core validation layer: not found (xr_layers/ in a game folder)\n");
        return;
    }
    for(char* c = dir; *c; ++c)
    {
        *c = *c == '/' ? '\\' : *c;
    }
    // Ours first; a path the player set already (their own layers) kept after it.
    char path[4096]{};
    const DWORD had = GetEnvironmentVariableA("XR_API_LAYER_PATH", path, sizeof(path));
    char joined[4096 + 1024 + 2];
    q_snprintf(joined, sizeof(joined), had && had < sizeof(path) ? "%s;%s" : "%s", dir, path);
    setEnv("XR_API_LAYER_PATH", joined);
    char log[1200];
    q_snprintf(log, sizeof(log), "%s/openxr_validation.log", diag.dir);
    setEnv("XR_CORE_VALIDATION_EXPORT_TYPE", "text");
    setEnv("XR_CORE_VALIDATION_FILE_NAME", log);
    if(FILE* f = fopen(log, "wb")) // (the layer appends: this stays first)
    {
        fputs("# Khronos' OpenXR core validation layer 1.1.63 (quakevr/xr_layers): its findings from each VR start "
              "(vr_diagnostics_xr_validation 0: not loaded).\n"
              "# Known false positive, the layer's own bug: VALID_ERROR VUID-xrDestroyDebugUtilsMessengerEXT-messenger-"
              "parameter right before \"Core Validation Layer will be destroyed\" (the layer destroys again, at "
              "xrDestroyInstance, the debug messenger the game already destroyed).\n",
            f);
        fclose(f);
    }
    char line[sizeof(joined) + 96];
    q_snprintf(line, sizeof(line), "XR_API_LAYER_PATH: %s (the OpenXR core validation layer: openxr_validation.log)\n", joined);
    writeSummaryLine(line);
}

// The run's own logs (vr_runlogs.cpp: console.log, openxr_debug.log, system.txt) and the runtimes' (written during the
// run), gl_startup.log and qvr_openxr.txt, copied into the folder.
void copyLogs()
{
    writeSummaryLine("Logs copied:\n");
    for(const char* name : {"console.log", "openxr_debug.log", "system.txt"})
    {
        runlogs::copyRunFile(name, diag.dir);
        writeSummaryLine(va("  logs/.../%s -> %s\n", name, name));
    }
    runlogs::copyRuntimeLogs(diag.dir, diag.began, writeSummaryLine);
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
    Con_Printf("  GL debug messages: %u; OpenXR debug messages: %u\n", diag.glTotal, runlogs::xrMessages());
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
        fprintf(f, "Files: gl_debug.log, openxr_validation.log (once VR starts), debug_output.log, crash/ (if it "
                   "crashed), and the logs copied at the end (listed below: the run's console.log, openxr_debug.log and "
                   "system.txt, from %s)\n",
            runlogs::dir());
        fclose(f);
    }
    diag.gl = openLog("gl_debug.log");
    if(diag.gl)
    {
        fprintf(diag.gl, "# GL debug output (a debug context, synchronous): time, source, type, id, severity, message; "
                         "each id's first %u, then every 100th\n",
            firstShown);
    }
    VR_SetCrashDir(diag.dir); // (the reports in <folder>/crash)
#ifdef _WIN32
    setEnv("XR_LOADER_DEBUG", "all");
    useValidationLayer();
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

// (VR_DiagnosticsXr and VR_DiagnosticsNote: vr_runlogs.cpp, every run's openxr_debug.log.)

// Host_Shutdown (crashed 0) and the crash handler (1): the summaries, the logs copied, the folder named.
extern "C" void VR_DiagnosticsEnd(int crashed)
{
    if(!diag.on || diag.ended)
    {
        return;
    }
    diag.ended = true;
    writeIdSummary(diag.gl, diag.glIds, diag.glTotal, "GL debug");
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
