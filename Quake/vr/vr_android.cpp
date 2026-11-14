// vr_android.cpp -- the Android platform layer (QUEST_PLAN.md, C2c and C3): where the game's files are, the crash
// reports' folder (the handler is vr_crash.cpp's POSIX one, shared with Linux: signals, the stack by unwinding on
// Android, the report into the folder and logcat), the console's text in logcat, and no child processes. The layer's
// logic is shared; the calls into Android go through `os` below, whose Android forms compile only in an Android build
// and whose PC forms stand in for them, so that the layer compiles and runs on the PC: in the fake-Android mode
// (-fakeandroid, QVR_FAKE_ANDROID=1 in the environment, or a build with QVR_FAKE_ANDROID defined). Off on the PC by
// default: nothing here changes a PC run without the switch. vr_android_test checks the layer (Debug > Startup).

#include "vr_api.h"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
} android;

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
    return env && *env && strcmp(env, "0") != 0;
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
    const size_t n = strlen(line);
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
    t.check(!strcmp(android.crashDir, expect), name, "crash folder not <base>/quakevr/crash");
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
    memcpy(kept, logLines.line, pending);
    logLines.len = 0;
    logLines.sink = captureLine;
    SDL_AtomicUnlock(&logLines.lock);
    logCapture = {};
    logText("abc");
    logText("def\nghi\n");
    t.check(logCapture.lines == 2, name, va("%d log lines from \"abc\" \"def\\nghi\\n\", not 2", logCapture.lines));
    t.check(!strcmp(logCapture.first, "abcdef") && !strcmp(logCapture.second, "ghi"), name, "log lines not joined");
    logText("jkl");
    t.check(logCapture.lines == 2, name, "a line without its newline sent");
    logText("\n");
    char longLine[2600];
    memset(longLine, 'x', sizeof longLine - 2);
    longLine[sizeof longLine - 2] = '\n';
    longLine[sizeof longLine - 1] = '\0';
    logCapture = {};
    logText(longLine);
    t.check(logCapture.lines == 3 && logCapture.chars == sizeof longLine - 2 && logCapture.longest < logLineSize, name,
        va("a %d-character line: %d messages, %d characters", static_cast<int>(sizeof longLine - 2), logCapture.lines,
            static_cast<int>(logCapture.chars)));
    SDL_AtomicLock(&logLines.lock);
    logLines.sink = nullptr;
    memcpy(logLines.line, kept, pending);
    logLines.len = pending;
    SDL_AtomicUnlock(&logLines.lock);

    Con_Printf("vr_android_test: base %s, crash reports %s\n", android.base, VR_CrashReportDir());
    t.verdict(name);
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
    Sys_mkdir(quakevr);
    copyPath(android.crashDir, quakevr, "/crash");
    Sys_mkdir(android.crashDir);
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

// Child processes of our own (the relighting's light runs, the motion review's evaluation copy): none on Android
// (the app may not start its own executable) nor with -nochildprocesses.
extern "C" int VR_ChildProcesses(void)
{
    return !android.on && !COM_CheckParm("-nochildprocesses");
}

extern "C" void VR_AndroidInit(void)
{
    Cmd_AddCommand("vr_android_test", test_f);
}
