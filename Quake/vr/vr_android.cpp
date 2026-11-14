// vr_android.cpp -- the Android platform layer's skeleton (QUEST_PLAN.md, C2c): where the game's files are, the crash
// handler (signals, the stack by unwinding, a report file and the log), and no child processes. The layer's logic is
// shared; the calls into Android go through `os` below, whose Android forms compile only in an Android build (C3)
// and whose PC forms stand in for them, so that the layer compiles and runs on the PC: in the fake-Android mode
// (-fakeandroid, QVR_FAKE_ANDROID=1 in the environment, or a build with QVR_FAKE_ANDROID defined). Off on the PC
// by default: nothing here changes a PC run without the switch. vr_android_test checks the layer (Debug > Startup).

#include "vr_api.h"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __ANDROID__
#include <SDL_system.h>
#include <android/log.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unwind.h>
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

// A line in the system log (logcat), signal-safe.
void log(const char* text)
{
    __android_log_write(ANDROID_LOG_ERROR, "QuakeVR", text);
}

struct UnwindState
{
    void** frames;
    int count;
    int max;
};

_Unwind_Reason_Code unwindFrame(_Unwind_Context* context, void* arg)
{
    UnwindState* s = static_cast<UnwindState*>(arg);
    const uintptr_t pc = _Unwind_GetIP(context);
    if(pc && s->count < s->max)
    {
        s->frames[s->count++] = reinterpret_cast<void*>(pc);
    }
    return s->count < s->max ? _URC_NO_REASON : _URC_END_OF_STACK;
}

// The return addresses on this thread's stack (no glibc backtrace on Android: the unwinder's).
int backtrace(void** frames, int max)
{
    UnwindState s{frames, 0, max};
    _Unwind_Backtrace(unwindFrame, &s);
    return s.count;
}

void installSignal(int sig, void (*handler)(int))
{
    struct sigaction sa{};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; // (a second fault in the handler ends the process)
    sigaction(sig, &sa, nullptr);
}

// The whole of `data` into `path` (signal-safe: open, write, close).
bool writeFile(const char* path, const char* data, size_t len)
{
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0)
    {
        return false;
    }
    const bool ok = write(fd, data, len) == static_cast<ssize_t>(len);
    close(fd);
    return ok;
}
#else
// The fake-Android mode: QVR_FAKE_ANDROID_DIR as the external files folder, else none (the PC's base folder kept).
const char* externalFilesDir()
{
    return getenv("QVR_FAKE_ANDROID_DIR");
}

void log(const char* text)
{
    fputs(text, stderr);
}

int backtrace(void**, int)
{
    return 0; // (the PC's own crash handler has the stacks)
}

// Recorded, not installed: the PC's crash handler (vr_crash.cpp) stays the one that runs.
void (*installed[NSIG])(int){};
void installSignal(int sig, void (*handler)(int))
{
    if(sig >= 0 && sig < NSIG)
    {
        installed[sig] = handler;
    }
}

bool writeFile(const char* path, const char* data, size_t len)
{
    FILE* f = fopen(path, "wb");
    if(!f)
    {
        return false;
    }
    const bool ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}
#endif
} // namespace os

// The fatal signals the handler reports (SIGBUS: POSIX only).
constexpr int crashSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL,
#ifdef SIGBUS
    SIGBUS
#endif
};

constexpr int maxPath = 1024;
constexpr int maxFrames = 48;
constexpr int reportSize = 4096;

// The layer's state (set once at start-up, main thread; the crash handler reads it).
struct Android
{
    bool on = false;            // an Android build, or the fake-Android mode
    char base[maxPath]{};       // the base folder (id1 and quakevr in it)
    char crashDir[maxPath]{};   // <base>/quakevr/crash
    char crashPath[maxPath]{};  // <crashDir>/crash.txt: the last crash's report
    char report[reportSize]{};  // the report, built in the handler without allocating
    void* frames[maxFrames]{};
} android;

// Signal-safe text building into a fixed buffer.
struct Writer
{
    char* out;
    size_t cap;
    size_t len = 0;

    void put(const char* s)
    {
        for(; s && *s && len + 1 < cap; s++)
        {
            out[len++] = *s;
        }
        out[len] = '\0';
    }
    void hex(uintptr_t v)
    {
        char digits[2 + sizeof(uintptr_t) * 2 + 1];
        int n = 0;
        digits[n++] = '0';
        digits[n++] = 'x';
        for(int shift = static_cast<int>(sizeof(uintptr_t)) * 8 - 4; shift >= 0; shift -= 4)
        {
            digits[n++] = "0123456789abcdef"[(v >> shift) & 15];
        }
        digits[n] = '\0';
        put(digits);
    }
    void dec(int v)
    {
        char digits[12];
        int n = sizeof digits - 1;
        digits[n] = '\0';
        const bool negative = v < 0;
        unsigned u = negative ? 0u - static_cast<unsigned>(v) : static_cast<unsigned>(v);
        do
        {
            digits[--n] = static_cast<char>('0' + u % 10);
            u /= 10;
        } while(u && n > 1);
        if(negative && n > 0)
        {
            digits[--n] = '-';
        }
        put(digits + n);
    }
};

const char* signalName(int sig)
{
    switch(sig)
    {
        case SIGSEGV: return "SIGSEGV (bad memory access)";
        case SIGABRT: return "SIGABRT (abort)";
        case SIGFPE: return "SIGFPE (arithmetic)";
        case SIGILL: return "SIGILL (illegal instruction)";
#ifdef SIGBUS
        case SIGBUS: return "SIGBUS (bus error)";
#endif
        default: return "signal";
    }
}

// The report of a crash by `sig` with `count` frames, into android.report: its length.
size_t formatReport(int sig, int count)
{
    Writer w{android.report, sizeof android.report};
    w.put("Quake VR crashed: signal ");
    w.dec(sig);
    w.put(" ");
    w.put(signalName(sig));
    w.put("\nBuild: ");
    w.put(VR_BuildInfo());
    w.put("\nStack (return addresses; symbolize with the build's unstripped .so: ndk-stack or addr2line):\n");
    for(int i = 0; i < count; i++)
    {
        w.put("  ");
        w.hex(reinterpret_cast<uintptr_t>(android.frames[i]));
        w.put("\n");
    }
    return w.len;
}

// A fatal signal: the report into the log and <crashDir>/crash.txt, then the signal again, by default (the system's
// own tombstone follows).
void crashSignal(int sig)
{
    const int count = os::backtrace(android.frames, maxFrames);
    const size_t len = formatReport(sig, count);
    os::log(android.report);
    os::writeFile(android.crashPath, android.report, len);
    signal(sig, SIG_DFL);
    raise(sig);
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
#ifndef __ANDROID__
    for(const int sig : crashSignals)
    {
        t.check(os::installed[sig] == crashSignal, name, va("no handler for signal %d", sig));
    }
#endif
    // A report as the handler writes it, to a test file beside crash.txt (not a real crash's).
    android.frames[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234abcd));
    const size_t len = formatReport(SIGSEGV, 1);
    char path[maxPath];
    q_snprintf(path, sizeof path, "%s/crash_test.txt", android.crashDir);
    t.check(os::writeFile(path, android.report, len), name, "report not written");
    char back[reportSize]{};
    if(FILE* f = fopen(path, "rb"))
    {
        const size_t n = fread(back, 1, sizeof back - 1, f);
        back[n] = '\0';
        fclose(f);
    }
    t.check(strlen(back) == len, name, "report read back at another length");
    t.check(strstr(back, va("signal %d SIGSEGV", SIGSEGV)) != nullptr, name, "no signal line");
    t.check(strstr(back, "0x") && strstr(back, "1234abcd"), name, "no frame address");
    t.check(strstr(back, VR_BuildInfo()) != nullptr, name, "no build line");
    Con_Printf("vr_android_test: base %s, crash reports %s\n", android.base, android.crashPath);
    t.verdict(name);
}

} // namespace

// main, after COM_InitArgv, before the crash handler and SDL: on Android (or -fakeandroid on the PC), the base
// folder (the external files folder unless -basedir) and the crash handler. Returns the base folder to use, or
// null (the PC's own).
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
    copyPath(android.crashPath, android.crashDir, "/crash.txt");
    for(const int sig : crashSignals)
    {
        os::installSignal(sig, crashSignal);
    }
    char pak[maxPath];
    copyPath(pak, android.base, "/id1/pak0.pak");
    if(!(Sys_FileType(pak) & FS_ENT_FILE))
    {
        char line[maxPath + 128];
        q_snprintf(line, sizeof line, "Quake VR: no %s: copy Quake's id1 folder into %s\n", pak, android.base);
        os::log(line);
    }
    return given ? nullptr : android.base;
}

extern "C" int VR_Android(void)
{
    return android.on;
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
