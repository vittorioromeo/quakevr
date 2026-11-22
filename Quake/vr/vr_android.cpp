// vr_android.cpp -- the Android platform layer (QUEST_PLAN.md, C2c and C3): where the game's files are, the crash
// reports' folder (the handler is vr_crash.cpp's POSIX one, shared with Linux: signals, the stack by unwinding on
// Android, the report into the folder and logcat), the console's text in logcat, and no child processes. The layer's
// logic is shared; the calls into Android go through `os` below, whose Android forms compile only in an Android build
// and whose PC forms stand in for them, so that the layer compiles and runs on the PC: in the fake-Android mode
// (-fakeandroid, QVR_FAKE_ANDROID=1 in the environment, or a build with QVR_FAKE_ANDROID defined). Off on the PC by
// default: nothing here changes a PC run without the switch. vr_android_test checks the layer (Debug > Startup).

#include "vr_api.h"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_testkit.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Base/Strstr.hpp"

#include <cstdio>
#include <cstdlib>

#ifdef __ANDROID__
#include <SDL_system.h>
#include <android/log.h>
#endif

namespace
{

// The Android calls the layer makes, and their PC stand-ins.
namespace os
{
#ifdef __ANDROID__
// The app's external files folder (/storage/emulated/0/Android/data/<package>/files): where the player copies id1.
const char* externalFilesDir()
{
    return SDL_AndroidGetExternalStoragePath();
}

// A message in the system log (logcat): `error` for the layer's own warnings, else the console's text.
void log(const char* text, bool error)
{
    __android_log_write(error ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "QuakeVR", text);
}
#else
// The fake-Android mode: QVR_FAKE_ANDROID_DIR as the external files folder, else none (the PC's base folder kept).
const char* externalFilesDir()
{
    return getenv("QVR_FAKE_ANDROID_DIR");
}

void log(const char* text, bool)
{
    fputs(text, stderr);
    fputs("\n", stderr);
}
#endif
} // namespace os

constexpr int maxPath = 1024;
constexpr size_t logLineSize = 1024; // logcat cuts a message at about 4 KB; a console line is far shorter

// The layer's state (set once at start-up, main thread).
struct Android
{
    bool on = false;            // an Android build, or the fake-Android mode
    char base[maxPath]{};       // the base folder (id1 and quakevr in it)
    char crashDir[maxPath]{};   // <base>/quakevr/crash: vr_crash.cpp's reports
    bool firstRun = false;      // no config at start: the Quest's graphics settings applied (VR_AndroidFirstRun)
} android;

// No config (ironwail.cfg) in `gamedir` yet: the game's first run there.
bool noConfigIn(const char* gamedir)
{
    return !(Sys_FileType(va("%s/%s", gamedir, CONFIG_NAME)) & FS_ENT_FILE);
}

// The console's text on its way to logcat: logcat makes each write a line, Sys_Printf's text comes in pieces, so the
// pieces are joined into lines here (any thread: the spin lock).
struct LogLines
{
    SDL_SpinLock lock = 0;
    char line[logLineSize]{};
    size_t len = 0;
    void (*sink)(const char*) = nullptr; // (vr_android_test's capture; null: os::log)
} logLines;

void emitLine()
{
    logLines.line[logLines.len] = '\0';
    if(logLines.sink)
    {
        logLines.sink(logLines.line);
    }
    else
    {
        os::log(logLines.line, false);
    }
    logLines.len = 0;
}

void logText(const char* text)
{
    SDL_AtomicLock(&logLines.lock);
    for(const char* c = text; c && *c; c++)
    {
        if(*c == '\n')
        {
            emitLine();
        }
        else
        {
            logLines.line[logLines.len++] = *c;
            if(logLines.len + 1 >= sizeof logLines.line)
            {
                emitLine(); // (a line longer than the buffer: in pieces)
            }
        }
    }
    SDL_AtomicUnlock(&logLines.lock);
}

void copyPath(char (&out)[maxPath], const char* a, const char* b = "")
{
    q_snprintf(out, sizeof out, "%s%s", a, b);
}

// vr_android_export's source: an absolute path as given; else the first that exists of <game folder>/<path> and
// <internal files>/<path>, and for "profile/<rest>" (the PC's folder of the profiler's files) <internal files>/<rest>:
// what the game writes outside the game folder on Android (files/perf/: the perf recorder's CSVs; his adb session
// of 2026-11-17 asked for "profile/perf" and got "failed, 0 bytes"). False: none exists (`out` the first tried).
bool resolveExport(char (&out)[maxPath], const char* path, const char* gameDir, const char* internal)
{
    if(path[0] == '/')
    {
        copyPath(out, path);
        return Sys_FileType(out) != FS_ENT_NONE;
    }
    const char* rest = !ZA_STRNCMP(path, "profile/", 8) ? path + 8 : nullptr;
    const char* const tries[][2] = {{gameDir, path}, {internal, path}, {internal, rest}};
    bool first = true;
    for(const auto& tr : tries)
    {
        if(!tr[0] || !tr[1] || !tr[1][0])
        {
            continue;
        }
        char at[maxPath];
        q_snprintf(at, sizeof at, "%s/%s", tr[0], tr[1]);
        if(first)
        {
            copyPath(out, at);
            first = false;
        }
        if(Sys_FileType(at) != FS_ENT_NONE)
        {
            copyPath(out, at);
            return true;
        }
    }
    return false;
}

bool fakeRequested(int argc, char** argv)
{
#ifdef QVR_FAKE_ANDROID
    (void)argc;
    (void)argv;
    return true;
#else
    for(int i = 1; i < argc; i++)
    {
        if(!q_strcasecmp(argv[i], "-fakeandroid"))
        {
            return true;
        }
    }
    const char* env = getenv("QVR_FAKE_ANDROID");
    return env && *env && ZA_STRCMP(env, "0") != 0;
#endif
}

// vr_android_test's logcat capture.
struct LogCapture
{
    int lines = 0;
    size_t chars = 0;
    size_t longest = 0;
    char first[64]{};
    char second[64]{};
} logCapture;

void captureLine(const char* line)
{
    const size_t n = ZA_STRLEN(line);
    if(logCapture.lines == 0)
    {
        q_strlcpy(logCapture.first, line, sizeof logCapture.first);
    }
    else if(logCapture.lines == 1)
    {
        q_strlcpy(logCapture.second, line, sizeof logCapture.second);
    }
    logCapture.lines++;
    logCapture.chars += n;
    logCapture.longest = n > logCapture.longest ? n : logCapture.longest;
}

// vr_android_test: the layer's checks (an Android build, or the PC's fake-Android mode).
void test_f()
{
    qvr::testkit::Tally t;
    constexpr const char* name = "vr_android_test";
    if(!t.check(android.on, name, "not in the Android layer (start with -fakeandroid)"))
    {
        t.verdict(name);
        return;
    }
    t.check(!VR_ChildProcesses(), name, "child processes allowed");
    t.check(android.base[0] != '\0', name, "no base folder");
    char expect[maxPath];
    q_snprintf(expect, sizeof expect, "%s/quakevr/crash", android.base);
    t.check(!ZA_STRCMP(android.crashDir, expect), name, "crash folder not <base>/quakevr/crash");
    t.check(Sys_FileType(android.crashDir) & FS_ENT_DIRECTORY, name, "crash folder not made");

    // The crash handler (vr_crash.cpp) reports into the same folder: a file put there is seen through its path.
    char probe[maxPath];
    q_snprintf(probe, sizeof probe, "%s/crash_probe.txt", android.crashDir);
    if(FILE* f = fopen(probe, "wb"))
    {
        fputs("vr_android_test\n", f);
        fclose(f);
    }
    t.check(Sys_FileType(va("%s/crash_probe.txt", VR_CrashReportDir())) & FS_ENT_FILE, name,
        va("the crash reports go elsewhere: %s", VR_CrashReportDir()));

    // The console's text into logcat: pieces joined into lines, a newline a message, an overlong line in pieces.
    SDL_AtomicLock(&logLines.lock);
    const size_t pending = logLines.len; // (text of the game's own, waiting for its newline: kept aside)
    char kept[logLineSize];
    ZA_MEMCPY(kept, logLines.line, pending);
    logLines.len = 0;
    logLines.sink = captureLine;
    SDL_AtomicUnlock(&logLines.lock);
    logCapture = {};
    logText("abc");
    logText("def\nghi\n");
    t.check(logCapture.lines == 2, name, va("%d log lines from \"abc\" \"def\\nghi\\n\", not 2", logCapture.lines));
    t.check(!ZA_STRCMP(logCapture.first, "abcdef") && !ZA_STRCMP(logCapture.second, "ghi"), name, "log lines not joined");
    logText("jkl");
    t.check(logCapture.lines == 2, name, "a line without its newline sent");
    logText("\n");
    char longLine[2600];
    ZA_MEMSET(longLine, 'x', sizeof longLine - 2);
    longLine[sizeof longLine - 2] = '\n';
    longLine[sizeof longLine - 1] = '\0';
    logCapture = {};
    logText(longLine);
    t.check(logCapture.lines == 3 && logCapture.chars == sizeof longLine - 2 && logCapture.longest < logLineSize, name,
        va("a %d-character line: %d messages, %d characters", static_cast<int>(sizeof longLine - 2), logCapture.lines,
            static_cast<int>(logCapture.chars)));
    SDL_AtomicLock(&logLines.lock);
    logLines.sink = nullptr;
    ZA_MEMCPY(logLines.line, kept, pending);
    logLines.len = pending;
    SDL_AtomicUnlock(&logLines.lock);

    // The first run (the Quest's graphics settings then): a folder without a config is one, a folder with one is not.
    t.check(noConfigIn(android.crashDir), name, "a config seen where there is none");
    char cfgDir[maxPath];
    copyPath(cfgDir, android.crashDir, "/config_test");
    (void)qvr::files::createDirectories(cfgDir);
    if(FILE* f = fopen(va("%s/%s", cfgDir, CONFIG_NAME), "wb"))
    {
        fputs("// vr_android_test\n", f);
        fclose(f);
    }
    t.check(!noConfigIn(cfgDir), name, "a config not seen");

    // vr_android_export's sources: the game folder's first, then the app's files (cfgDir standing in for them), and
    // "profile/<rest>" there as <rest>; a path in neither not found.
    char found[maxPath];
    t.check(resolveExport(found, CONFIG_NAME, android.crashDir, cfgDir) && ZA_STRSTR(found, "config_test"), name,
        va("export: %s not found in the app's files (%s)", CONFIG_NAME, found));
    t.check(resolveExport(found, "profile/" CONFIG_NAME, android.crashDir, cfgDir) && ZA_STRSTR(found, "config_test"), name,
        va("export: profile/%s not taken as the app's %s (%s)", CONFIG_NAME, CONFIG_NAME, found));
    t.check(resolveExport(found, "config_test", android.crashDir, nullptr), name, "export: a game-folder path not found");
    t.check(!resolveExport(found, "no_such_file.txt", android.crashDir, cfgDir), name, "export: a missing file found");

    // (the test's files out of the crash folder again: a bug report's zip takes every file there)
    (void)qvr::files::remove(probe);
    (void)qvr::files::remove(va("%s/%s", cfgDir, CONFIG_NAME));
    (void)qvr::files::remove(cfgDir);

    Con_Printf("vr_android_test: base %s, crash reports %s, first run %d\n", android.base, VR_CrashReportDir(),
        android.firstRun ? 1 : 0);
    t.verdict(name);
}

// vr_android_export <path> (Debug > Startup): a file or a folder's files under the game folder (eyeshots, a save,
// a log) copied into the app's internal files folder's export/, which a debuggable APK's `adb shell run-as` reads
// (what the game writes in its external folder is 0660 to adb's shell: section 6i of QUEST_PLAN.md). Also what the
// game writes in the app's own files (resolveExport: "perf", the perf recorder's CSVs).
void export_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_android_export <file or folder under the game folder, or the app's files (perf)>: copied to the "
                   "app's internal export/\n");
        return;
    }
#ifdef __ANDROID__
    const char* internal = SDL_AndroidGetInternalStoragePath();
#else
    const char* internal = nullptr;
#endif
    if(!internal)
    {
        Con_Printf("vr_android_export: Android only\n");
        return;
    }
    char to[maxPath];
    copyPath(to, internal, "/export");
    if(!qvr::files::createDirectories(to)) // (not Sys_mkdir: a failure ends the game)
    {
        Con_Printf("vr_android_export: the export folder %s can't be made\n", to);
        return;
    }
    char from[maxPath];
    if(!resolveExport(from, Cmd_Argv(1), com_gamedir, internal))
    {
        Con_Printf("vr_android_export: %s not found (under the game folder %s, nor the app's files %s)\n", Cmd_Argv(1),
            com_gamedir, internal);
        return;
    }
    const auto copyOne = [&](const char* src, const char* name) {
        char dst[maxPath];
        q_snprintf(dst, sizeof dst, "%s/%s", to, name);
        FILE* in = fopen(src, "rb");
        FILE* out = in ? fopen(dst, "wb") : nullptr;
        size_t bytes = 0;
        bool written = out != nullptr;
        char buf[16384];
        for(size_t n = 0; out && (n = fread(buf, 1, sizeof buf, in)) > 0; bytes += n)
        {
            written = written && fwrite(buf, 1, n, out) == n;
        }
        if(out)
        {
            written = fclose(out) == 0 && written;
        }
        if(in)
        {
            fclose(in);
        }
        Con_Printf("vr_android_export: %s -> %s (%s, %d bytes)\n", src, dst,
            written ? "copied" : out ? "failed: not all written (the storage full?)"
            : in ? "failed: the export folder cannot be written" : "failed: cannot be read", static_cast<int>(bytes));
    };
    if(Sys_FileType(from) & FS_ENT_DIRECTORY)
    {
        findfile_t* f = Sys_FindFirst(from, nullptr);
        for(; f; f = Sys_FindNext(f))
        {
            if(!(f->attribs & FA_DIRECTORY))
            {
                char src[maxPath];
                q_snprintf(src, sizeof src, "%s/%s", from, f->name);
                copyOne(src, f->name);
            }
        }
        return;
    }
    const char* slash = strrchr(from, '/');
    copyOne(from, slash ? slash + 1 : from);
}

} // namespace

// main, after COM_InitArgv, before the crash handler and SDL: on Android (or -fakeandroid on the PC), the base
// folder (the external files folder unless -basedir) and the crash reports' folder. Returns the base folder to use,
// or null (the PC's own).
extern "C" const char* VR_AndroidStart(int argc, char** argv)
{
#ifdef __ANDROID__
    android.on = true;
#else
    android.on = fakeRequested(argc, argv);
#endif
    if(!android.on)
    {
        return nullptr;
    }
    const char* given = nullptr;
    for(int i = 1; i + 1 < argc; i++)
    {
        if(!q_strcasecmp(argv[i], "-basedir"))
        {
            given = argv[i + 1];
        }
    }
    const char* external = os::externalFilesDir();
    copyPath(android.base, given ? given : external ? external : ".");
    char quakevr[maxPath];
    copyPath(quakevr, android.base, "/quakevr");
    copyPath(android.crashDir, quakevr, "/crash");
    // (not Sys_mkdir: its failure is a Sys_Error, before the crash handler and SDL are up: an adb-pushed quakevr folder
    // the app can't write ended the start there, QUEST_PLAN.md 6i. Said in logcat; the missing pak's line or the
    // engine's own error follows with what to do.)
    if(!qvr::files::createDirectories(android.crashDir))
    {
        char line[maxPath + 96];
        q_snprintf(line, sizeof line, "Quake VR: %s can't be made (the folder not writable by the app?)", android.crashDir);
        os::log(line, true);
    }
    VR_SetCrashDir(quakevr); // (vr_crash.cpp's reports in <base>/quakevr/crash, before Host_Init sets the game's)
    char pak[maxPath];
    copyPath(pak, android.base, "/id1/pak0.pak");
    if(!(Sys_FileType(pak) & FS_ENT_FILE))
    {
        char line[maxPath + 128];
        q_snprintf(line, sizeof line, "Quake VR: no %s: copy Quake's id1 folder into %s", pak, android.base);
        os::log(line, true);
    }
    return given ? nullptr : android.base;
}

extern "C" int VR_Android(void)
{
    return android.on;
}

extern "C" void VR_AndroidLog(const char* text)
{
    logText(text);
}

extern "C" int VR_AndroidFirstRun(void)
{
    android.firstRun = android.on && noConfigIn(com_gamedir);
    return android.firstRun;
}

// Child processes of our own (the relighting's light runs, the motion review's evaluation copy): none on Android
// (the app may not start its own executable) nor with -nochildprocesses.
extern "C" int VR_ChildProcesses(void)
{
    return !android.on && !COM_CheckParm("-nochildprocesses");
}

extern "C" void VR_AndroidInit(void)
{
    Cmd_AddCommand("vr_android_test", test_f);
    Cmd_AddCommand("vr_android_export", export_f);
}
