// vr_xr_stall.cpp -- see vr_xr_stall.hpp.

#include "vr_xr_stall.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"
#include "vr_xr_runtime.hpp"

#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace qvr::xrstall
{

namespace
{

// A frame is held when its wait or end call takes over this many display periods, and at least heldFloorMs (the
// headset's frames are 7-14 ms; the drop's were 100 ms, xrEndFrame 79-87 ms of it); held from heldFrames in a row
// (a hitch is one frame), over after onTimeFrames in a row on time. An episode's start is logged at most once every
// logEvery seconds (its end only when its start was).
constexpr double heldPeriods = 3.0;
constexpr double heldFloorMs = 25.0;
constexpr int heldFrames = 3;
constexpr int onTimeFrames = 30;
constexpr double logEvery = 10.0;
constexpr int recentCount = 3;

struct Recent
{
    float waitMs{0.f};
    float endMs{0.f};
    float intervalMs{0.f}; // since the frame before
};

struct Watch
{
    Recent recent[recentCount];
    int recentNext{0};
    double lastFrame{-1.0};      // the last frame() call (Sys_DoubleTime)
    int over{0};                 // frames held in a row
    int onTime{0};               // frames on time in a row, while held
    bool held{false};
    bool logged{false};          // this episode's start was logged
    double since{0.0};           // the episode's start (its heldFrames-th frame)
    double lastHeld{0.0};        // its last frame held
    double lastLogged{-1e9};     // the last start logged
    int unlogged{0};             // episodes not logged since (logEvery)
    int frames{0};               // the episode's frames
    double maxWaitMs{0.0};
    double maxEndMs{0.0};
};
Watch watch;

// A frame call's last failure (callFailed), for its repeats.
struct Failure
{
    const char* call{nullptr};
    int code{0};
    double logged{-1e9};
    int repeats{0};
};
Failure failures[2]; // xrWaitFrame's, xrEndFrame's (by name)

// The text in a log line's quotes: its quotes and control characters made apostrophes.
void clean(char* text)
{
    for(char* c = text; *c; c++)
    {
        if(*c == '"' || static_cast<unsigned char>(*c) < 32)
        {
            *c = '\'';
        }
    }
}

// The foreground window (Win32, not SDL's focus): its program, its title and whether it is the game's.
za::String foreground()
{
#ifdef _WIN32
    HWND window = GetForegroundWindow();
    if(!window)
    {
        return "no foreground window";
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    char title[160] = "";
    GetWindowTextA(window, title, sizeof(title));
    clean(title);
    char image[MAX_PATH] = "?";
    if(HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
    {
        DWORD size = sizeof(image);
        if(!QueryFullProcessImageNameA(process, 0, image, &size))
        {
            strcpy(image, "?");
        }
        CloseHandle(process);
    }
    const char* slash = strrchr(image, '\\');
    const char* name = slash ? slash + 1 : image;
    const bool ours = pid == GetCurrentProcessId();
    return va("foreground %s \"%s\" (pid %lu; %s)", name, title, static_cast<unsigned long>(pid),
        ours ? "the game's window" : "not the game's window");
#else
    return "foreground window not known";
#endif
}

// What else was going on: the session, the windows, the frames, the CPU and the GPU.
za::String context(const char* state, double periodMs)
{
    za::String text = va("session %s; %s, SDL window %s; period %.2f ms; frames (wait/end/interval ms):", state,
        foreground().cStr(), windowUnfocused() ? "unfocused" : "focused", periodMs);
    for(int i = 0; i < recentCount; i++)
    {
        const Recent& r = watch.recent[(watch.recentNext + i) % recentCount];
        text += va(" %.1f/%.1f/%.1f", r.waitMs, r.endMs, r.intervalMs);
    }
    profile::FrameSample sample;
    if(profile::frameSample(0, sample))
    {
        text += va("; CPU busy %.2f ms", sample.cpuMs);
    }
    for(int back = 0; back < 8 && profile::frameSample(back, sample); ++back)
    {
        if(sample.gpuMs >= 0.f)
        {
            text += va(", GPU eyes %.2f ms", sample.gpuMs);
            break;
        }
    }
    return text;
}

void say(const char* line)
{
    note(line);
    Con_DPrintf("VR: %s", line);
}

} // namespace

const char* wallClock()
{
#ifdef _WIN32
    SYSTEMTIME t;
    GetLocalTime(&t);
    return va("%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
#else
    return va("%.3f", Sys_DoubleTime());
#endif
}

void note(const char* line)
{
    xrruntime::logLine(line);
#ifdef QVR_TRACY
    size_t length = strlen(line);
    while(length && line[length - 1] == '\n')
    {
        length--;
    }
    VR_TracyMessage(line, static_cast<unsigned>(length));
#endif
}

void frame(double waitMs, double endMs, double periodMs, const char* state)
{
    Watch& w = watch;
    const double now = Sys_DoubleTime();
    Recent& r = w.recent[w.recentNext];
    w.recentNext = (w.recentNext + 1) % recentCount;
    r.waitMs = static_cast<float>(waitMs);
    r.endMs = static_cast<float>(endMs);
    r.intervalMs = w.lastFrame >= 0.0 ? static_cast<float>((now - w.lastFrame) * 1000.0) : 0.f;
    w.lastFrame = now;

    const double limit = za::max(heldFloorMs, heldPeriods * periodMs);
    const bool over = waitMs > limit || endMs > limit;
    if(!w.held)
    {
        w.over = over ? w.over + 1 : 0;
        if(w.over < heldFrames)
        {
            return;
        }
        w.held = true;
        w.onTime = 0;
        w.since = w.lastHeld = now;
        w.frames = w.over;
        w.maxWaitMs = w.maxEndMs = 0.0;
        for(const Recent& f : w.recent)
        {
            w.maxWaitMs = za::max(w.maxWaitMs, static_cast<double>(f.waitMs));
            w.maxEndMs = za::max(w.maxEndMs, static_cast<double>(f.endMs));
        }
        w.logged = now - w.lastLogged >= logEvery;
        if(!w.logged)
        {
            w.unlogged++;
            return;
        }
        w.lastLogged = now;
        // (Each part copied before the next: va's buffers are few.)
        za::String line = wallClock();
        line += va(" held frames: the runtime holds the frames (%d in a row with xrWaitFrame or xrEndFrame over %.1f ms)",
            heldFrames, limit);
        if(w.unlogged)
        {
            line += va(", %d more episodes since the last logged", w.unlogged);
        }
        line += "; " + context(state, periodMs) + "\n";
        say(line.cStr());
        w.unlogged = 0;
        return;
    }

    w.frames++;
    w.maxWaitMs = za::max(w.maxWaitMs, waitMs);
    w.maxEndMs = za::max(w.maxEndMs, endMs);
    w.onTime = over ? 0 : w.onTime + 1;
    w.lastHeld = over ? now : w.lastHeld;
    if(w.onTime < onTimeFrames)
    {
        return;
    }
    w.held = false;
    w.over = 0;
    if(w.logged)
    {
        za::String line = wallClock();
        line += va(" held frames over: %.1f s, %d frames held (xrWaitFrame up to %.1f ms, xrEndFrame up to %.1f ms)",
            w.lastHeld - w.since, w.frames - onTimeFrames, w.maxWaitMs, w.maxEndMs);
        line += "; " + context(state, periodMs) + "\n";
        say(line.cStr());
    }
}

void callFailed(const char* call, int code, const char* codeName)
{
    Failure& f = failures[call && call[2] == 'E' ? 1 : 0]; // ("xrEndFrame", else "xrWaitFrame")
    const double now = Sys_DoubleTime();
    if(f.call == call && f.code == code && now - f.logged < 1.0)
    {
        f.repeats++;
        return;
    }
    za::String line = wallClock();
    line += va(" %s failed: %s (%d)", call, codeName, code);
    if(f.repeats)
    {
        line += va(" (%s failed %d more times since its last line)", f.call, f.repeats);
    }
    line += "\n";
    note(line.cStr());
    Con_Warning("OpenXR: %s", line.cStr());
    f = Failure{call, code, now, 0};
}

void debugStall()
{
    if(vr_debug_frame_stall_ms.value > 0.f)
    {
        Sys_Sleep(static_cast<unsigned long>(za::min(vr_debug_frame_stall_ms.value, 1000.f)));
    }
}

} // namespace qvr::xrstall
