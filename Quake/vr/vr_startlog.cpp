// vr_startlog.cpp -- the start-up trace (vr_startup_log; docs/vr-port/ROUND25.md, "startuplog"): each step of the
// start-up and of the first map's load logged as it begins and ends, with the wall clock since the process started:
//   [startup +1.234s] video (window, GL context, the engine's shaders) begin
//   [startup +1.811s] video (window, GL context, the engine's shaders) end (took 577.0 ms)
// (nested steps indented), the stages that only end (VR_TimeMark with no step of their name open) as "done", notes (a
// pak's files, progs.dat's size and CRC), "first frame shown" and a summary with the slowest steps. The same code on
// the PC and the Quest: the lines go to the console (Sys_Printf: the terminal, the run's log, logcat on Android, a line
// at a time) and to <game folder>/startup.log (quakevr/startup.log, made anew at each start, each line flushed; on the
// Quest in the app's files folder: vr_android_export startup.log).
// The heartbeat: a watchdog thread reads what the main thread is in (a seqlock snapshot: no lock the main thread waits
// on) and, while one step has run longer than 2 s, says every 2 s "still in <step> (<its progress>)": the step's own
// progress (models 12/40, the signon's frames) and the last GL step (each shader compile and link, the OpenXR calls).
// A hang shows where it is. The trace runs from the process's start to the first frame drawn and, when no map loaded
// in the start-up, on to the first map's first frame. vr_startup_log 0: no console lines (the file still written).

#include "vr_startlog.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include <cstdarg>
#include <cstdio>
#ifndef _WIN32
#include <unistd.h> // (write: the watchdog's line to the terminal while the loader's output is caught)
#endif

namespace qvr::startlog
{

namespace
{

constexpr int maxDepth = 12;
constexpr int nameLength = 160;
constexpr double beatEvery = 2.0; // seconds: the heartbeat's period, and how long a step runs before its first beat

enum class Phase
{
    Off,
    Startup,  // the process's start to the first frame drawn
    FirstMap, // then, when no map loaded in the start-up, to the first map's first frame
};

struct OpenStep
{
    char name[nameLength];
    double begin;
};

struct DoneStep
{
    za::String name;
    double ms;
};

// The main thread's side.
struct Trace
{
    Phase phase = Phase::Off;
    double processStart = 0.0;
    za::ThreadId mainThread;
    OpenStep stack[maxDepth];
    int depth = 0;
    int lost = 0; // steps begun past maxDepth (not traced)
    bool loadOpen = false;
    bool mapShown = false; // a map's first frame drawn in the trace
    OpenStep load{};
    double lastMark = 0.0;
    char lastMarkName[nameLength] = "process start";
    char progress[nameLength] = ""; // the innermost step's (VR_StepProgress, a load's frames)
    char glStep[nameLength] = "";   // the last GL step since the innermost step began
    long glSteps = 0;
    za::Vector<DoneStep> done;
    int phaseFirstDone = 0; // done's first step of the phase being summed up
    za::String pending;     // the lines before the game folder is known (the file opens then)
    FILE* file = nullptr;
    bool fileTried = false;
    bool fileMade = false;
    za::String summary; // the last summary line (vr_startup_trace)
    za::String path;
    double stalled = 0.0; // vr_startup_trace stall's seconds (its check)
};
Trace trace;

// What the watchdog reads: a seqlock (even: stable) over the label and its start.
struct Shared
{
    za::Atomic<unsigned> seq{0};
    char label[2 * nameLength + 64] = "";
    double since = 0.0;
    bool active = false;
    double processStart = 0.0;
    za::Atomic<bool> quiet{false};
    za::Atomic<bool> stop{false};
    za::Atomic<bool> running{false};
    za::Atomic<FILE*> file{nullptr};
    za::Atomic<int> beats{0}; // the "still in" lines said (vr_startup_trace check)
};
Shared shared;

bool onMain()
{
    return za::ThisThread::getId() == trace.mainThread;
}

bool loud()
{
    return !(qvr::vr_startup_log.flags & CVAR_REGISTERED) || qvr::vr_startup_log.value != 0.f;
}

// After the file system's start (VR_StartupLogOpen): the game folder known (quakevr), the lines so far written.
void openFile()
{
    if(trace.fileTried || !com_gamedir[0])
    {
        return;
    }
    trace.fileTried = true;
    trace.path = za::String{com_gamedir} + "/startup.log";
    trace.file = fopen(trace.path.cStr(), "wb");
    if(trace.file)
    {
        trace.fileMade = true;
        fputs(trace.pending.cStr(), trace.file);
        fflush(trace.file);
        shared.file.storeRelease(trace.file);
    }
    trace.pending.clear();
}

// A line (main thread): the console, the file.
void emit(int indent, const char* fmt, ...) FUNC_PRINTF(2, 3);
void emit(int indent, const char* fmt, ...)
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    char line[1200];
    q_snprintf(line, sizeof(line), "[startup +%.3fs] %*s%s\n", Sys_DoubleTime() - trace.processStart, indent * 2, "", text);
    if(trace.file)
    {
        fputs(line, trace.file);
        fflush(trace.file);
    }
    else if(!trace.fileTried)
    {
        trace.pending += line;
    }
    const bool l = loud();
    shared.quiet.storeRelaxed(!l);
    if(l && con_initialized)
    {
        Con_SafePrintf("%s", line); // (no screen update for it while loading)
    }
    else if(l)
    {
        Sys_Printf("%s", line); // (before the console: what Con_Printf would do then)
    }
}

// The snapshot the watchdog reads, after any change (main thread).
void publish()
{
    char label[sizeof(shared.label)];
    double since = trace.lastMark;
    if(trace.depth > 0)
    {
        q_strlcpy(label, trace.stack[trace.depth - 1].name, sizeof(label));
        since = trace.stack[trace.depth - 1].begin;
    }
    else if(trace.loadOpen)
    {
        q_strlcpy(label, trace.load.name, sizeof(label));
        since = trace.load.begin;
    }
    else
    {
        q_snprintf(label, sizeof(label), "the stage after \"%s\"", trace.lastMarkName);
    }
    const za::SizeT n = ZA_STRLEN(label);
    if(trace.progress[0] || trace.glStep[0])
    {
        q_snprintf(label + n, sizeof(label) - n, " (%s%s%s)", trace.progress, trace.progress[0] && trace.glStep[0] ? "; " : "",
            trace.glStep[0] ? va("GL step %ld: %s", trace.glSteps, trace.glStep) : "");
    }
    const bool active = trace.phase == Phase::Startup || trace.depth > 0 || trace.loadOpen;

    shared.seq.fetchAddRelaxed(1); // odd: being written
    za::atomicThreadFence<za::MemoryOrder::Release>();
    ZA_MEMCPY(shared.label, label, sizeof(label));
    shared.since = since;
    shared.active = active;
    za::atomicThreadFence<za::MemoryOrder::Release>();
    shared.seq.fetchAddRelaxed(1);
}

void clearProgress()
{
    trace.progress[0] = 0;
    trace.glStep[0] = 0;
}

void beginStep(const char* name)
{
    if(trace.depth >= maxDepth)
    {
        trace.lost++;
        return;
    }
    OpenStep& s = trace.stack[trace.depth];
    q_strlcpy(s.name, name, sizeof(s.name));
    s.begin = Sys_DoubleTime();
    emit(trace.depth, "%s begin", s.name);
    trace.depth++;
    clearProgress();
    publish();
}

void endStep(const char* why)
{
    if(trace.lost > 0)
    {
        trace.lost--;
        return;
    }
    if(trace.depth <= 0)
    {
        return;
    }
    trace.depth--;
    const OpenStep& s = trace.stack[trace.depth];
    const double now = Sys_DoubleTime();
    const double ms = (now - s.begin) * 1000.0;
    emit(trace.depth, "%s end (took %.1f ms)%s", s.name, ms, why);
    trace.done.pushBack({s.name, ms});
    trace.lastMark = now;
    q_strlcpy(trace.lastMarkName, s.name, sizeof(trace.lastMarkName));
    clearProgress();
    publish();
}

// The summary of the phase: its time, its slowest steps, and the work across them (vr_startup.cpp's sums).
void summarize(const char* what, double ms, const char* work)
{
    za::Vector<const DoneStep*> slow;
    for(za::SizeT i = static_cast<za::SizeT>(trace.phaseFirstDone); i < trace.done.size(); i++)
    {
        slow.pushBack(&trace.done[i]);
    }
    za::insertionSort(slow.begin(), slow.end(), [](const DoneStep* a, const DoneStep* b) { return a->ms > b->ms; }); // (a few hundred at most)
    za::String line = va("summary: %s %.0f ms; slowest:", what, ms);
    for(za::SizeT i = 0; i < slow.size() && i < 6; i++)
    {
        line += va("%s %s %.0f ms", i ? "," : "", slow[i]->name.cStr(), slow[i]->ms);
    }
    if(work && work[0])
    {
        line += va("; work: %s", work);
    }
    emit(0, "%s", line.cStr());
    trace.summary = line;
    trace.phaseFirstDone = static_cast<int>(trace.done.size());
}

void stopTrace()
{
    trace.phase = Phase::Off;
    shared.stop.storeRelaxed(true);
    publish();
}

// The watchdog (its own thread): reads the snapshot every 100 ms; a step past 2 s gets a line every 2 s.
void watchdog()
{
    double beatSince = -1.0;
    int beats = 0;
    while(!shared.stop.loadRelaxed())
    {
        za::ThisThread::sleepFor(za::milliseconds(100));
        char label[sizeof(shared.label)];
        double since = 0.0;
        bool active = false;
        for(;;)
        {
            const unsigned s1 = shared.seq.loadAcquire();
            if(s1 & 1u)
            {
                continue;
            }
            ZA_MEMCPY(label, shared.label, sizeof(label));
            since = shared.since;
            active = shared.active;
            za::atomicThreadFence<za::MemoryOrder::Acquire>();
            if(shared.seq.loadRelaxed() == s1)
            {
                break;
            }
        }
        label[sizeof(label) - 1] = 0;
        if(!active)
        {
            continue;
        }
        if(since != beatSince)
        {
            beatSince = since;
            beats = 0;
        }
        const double now = Sys_DoubleTime();
        if(now - since < beatEvery * (beats + 1))
        {
            continue;
        }
        beats++;
        shared.beats.fetchAddRelaxed(1);
        char line[sizeof(label) + 96];
        q_snprintf(line, sizeof(line), "[startup +%.3fs] still in %s (%.1f s so far)\n", now - shared.processStart, label,
            now - since);
        if(!shared.quiet.loadRelaxed())
        {
#ifndef _WIN32
            // (the OpenXR loader's output caught: the terminal's own stdout, or a hang there says nothing; VR_TerminalFd)
            if(const int fd = VR_TerminalFd(1); fd >= 0)
            {
                (void)!write(fd, line, ZA_STRLEN(line));
            }
            else
#endif
            {
                Sys_Printf("%s", line); // (logcat on Android: VR_AndroidLog is thread-safe; the run's log is locked)
            }
        }
        if(FILE* f = shared.file.loadAcquire())
        {
            fputs(line, f); // (stdio locks the stream: the main thread's lines whole)
            fflush(f);
        }
    }
    shared.running.storeRelease(false);
}

// vr_startup_trace: the trace's file and its last summary. Tests (a start's script, while the trace runs):
//   vr_startup_trace stall <s>  the main thread sleeps s seconds inside a step (the heartbeat's lines; real time)
//   vr_startup_trace check      "startuplog: N passed, M failed": the file made, the start-up summed up, no step left
//                               open, and after a stall of 5 s or more at least two "still in" lines
void trace_f()
{
    if(Cmd_Argc() >= 3 && !ZA_STRCMP(Cmd_Argv(1), "stall"))
    {
        const double seconds = Q_atof(Cmd_Argv(2));
        VR_StepBegin("vr_startup_trace stall (a test)");
        VR_StepProgress("sleeping %.1f s", seconds);
        za::ThisThread::sleepFor(za::milliseconds(static_cast<za::I64>(seconds * 1000.0)));
        VR_StepEnd();
        trace.stalled = seconds;
        return;
    }
    if(Cmd_Argc() >= 2 && !ZA_STRCMP(Cmd_Argv(1), "check"))
    {
        qvr::testkit::Tally t;
        t.check(trace.fileMade, "startuplog", va("startup.log not made (%s)", trace.path.cStr()));
        t.check(!trace.summary.empty(), "startuplog", "the start-up not summed up (no first frame shown)");
        t.check(trace.depth == 0 && trace.lost == 0, "startuplog", "a step left open");
        const int beats = shared.beats.loadRelaxed();
        if(trace.stalled >= 5.0)
        {
            t.check(beats >= 2, "startuplog", va("the heartbeat's \"still in\" lines: %d (2 or more wanted)", beats));
        }
        Con_Printf("startuplog: %d \"still in\" lines, the file %s\n", beats, trace.path.cStr());
        t.verdict("startuplog");
        return;
    }
    Con_Printf("vr_startup_trace: %s (vr_startup_log %s; %s)\n", trace.path.empty() ? "no file" : trace.path.cStr(),
        qvr::vr_startup_log.string, trace.phase == Phase::Off ? "over" : "still tracing");
    if(!trace.summary.empty())
    {
        Con_Printf("  %s\n", trace.summary.cStr());
    }
}

} // namespace

void start(double processStart)
{
    trace.processStart = processStart;
    trace.mainThread = za::ThisThread::getId();
    trace.lastMark = processStart;
    trace.phase = Phase::Startup;
    shared.processStart = processStart;
    emit(0, "process start (Quake VR %s; the times are since the process started)", VR_BuildInfo());
    publish();
    shared.running.storeRelaxed(true);
    za::Thread thread{watchdog};
    thread.detach(); // (stops by itself at the trace's end; never joined: a quit mid-start-up needn't wait for it)
}

void init()
{
    Cmd_AddCommand("vr_startup_trace", trace_f);
}

void mark(const char* stage, double now)
{
    if(trace.phase == Phase::Off || !onMain())
    {
        return;
    }
    if(trace.depth > 0 && trace.lost == 0 && !ZA_STRCMP(trace.stack[trace.depth - 1].name, stage))
    {
        endStep("");
        return;
    }
    emit(trace.depth, "%s done (%.1f ms since \"%s\")", stage, (now - trace.lastMark) * 1000.0, trace.lastMarkName);
    trace.lastMark = now;
    q_strlcpy(trace.lastMarkName, stage, sizeof(trace.lastMarkName));
    if(trace.depth == 0)
    {
        clearProgress();
    }
    publish();
}

void loadBegin(const char* what, double now)
{
    if(trace.phase == Phase::Off || !onMain())
    {
        return;
    }
    if(trace.loadOpen)
    {
        emit(trace.depth, "%s: another load begins", trace.load.name);
    }
    q_snprintf(trace.load.name, sizeof(trace.load.name), "map load (%s)", what);
    trace.load.begin = now;
    trace.loadOpen = true;
    emit(trace.depth, "%s begin", trace.load.name);
    clearProgress();
    publish();
}

void loadFrame(int frames, int signon)
{
    if(!trace.loadOpen || trace.depth > 0)
    {
        return;
    }
    q_snprintf(trace.progress, sizeof(trace.progress), "%d frames so far, waiting for the signon: %d of 4", frames, signon);
    publish();
}

void loadEnd(double now, bool failed, const char* work)
{
    if(!trace.loadOpen)
    {
        return;
    }
    trace.loadOpen = false;
    const double ms = (now - trace.load.begin) * 1000.0;
    if(failed)
    {
        emit(0, "%s failed (after %.1f ms)", trace.load.name, ms);
    }
    else
    {
        emit(0, "%s end (took %.1f ms): its first frame shown", trace.load.name, ms);
        trace.done.pushBack({trace.load.name, ms});
        trace.mapShown = true;
    }
    trace.lastMark = now;
    q_strlcpy(trace.lastMarkName, trace.load.name, sizeof(trace.lastMarkName));
    clearProgress();
    if(!failed && trace.phase == Phase::FirstMap)
    {
        summarize(trace.load.name, ms, work);
        stopTrace();
        return;
    }
    publish();
}

void startupEnd(double now, const char* work)
{
    if(trace.phase != Phase::Startup)
    {
        return;
    }
    const double ms = (now - trace.processStart) * 1000.0;
    emit(0, "first frame shown (the start-up took %.1f ms)", ms);
    summarize("start-up", ms, work);
    if(trace.mapShown)
    {
        stopTrace();
        return;
    }
    trace.phase = Phase::FirstMap; // (traced on to the first map's first frame)
    emit(0, "the trace goes on to the first map's load");
    trace.lastMark = now;
    q_strlcpy(trace.lastMarkName, "first frame shown", sizeof(trace.lastMarkName));
    publish();
}

void frameEnd()
{
    if(trace.phase == Phase::Off)
    {
        if(trace.file && !shared.running.loadAcquire())
        {
            shared.file.storeRelaxed(nullptr);
            fclose(trace.file); // (the watchdog gone: nothing else writes it)
            trace.file = nullptr;
        }
        return;
    }
    while(trace.depth > 0 || trace.lost > 0)
    {
        endStep(" (left open at the frame's end: an error?)");
    }
}

void glStep(const char* text)
{
    if(trace.phase == Phase::Off || !onMain())
    {
        return;
    }
    trace.glSteps++;
    q_strlcpy(trace.glStep, text, sizeof(trace.glStep));
    publish();
}

void quit()
{
    if(trace.phase != Phase::Off)
    {
        emit(0, "quit before the trace's end");
        stopTrace();
    }
}

} // namespace qvr::startlog

extern "C" void VR_StepBegin(const char* step)
{
    qvr::loadbar::stepBegin(step);
    if(qvr::startlog::trace.phase != qvr::startlog::Phase::Off && qvr::startlog::onMain())
    {
        qvr::startlog::beginStep(step);
    }
}

extern "C" void VR_StartupLogOpen(void)
{
    if(qvr::startlog::trace.phase != qvr::startlog::Phase::Off && qvr::startlog::onMain())
    {
        qvr::startlog::openFile();
    }
}

extern "C" void VR_StepEnd(void)
{
    qvr::loadbar::stepEnd();
    if(qvr::startlog::trace.phase != qvr::startlog::Phase::Off && qvr::startlog::onMain())
    {
        qvr::startlog::endStep("");
    }
}

extern "C" void VR_StepProgress(const char* fmt, ...)
{
    using namespace qvr::startlog;
    if(trace.phase == Phase::Off || !onMain())
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(trace.progress, sizeof(trace.progress), fmt, args);
    va_end(args);
    publish();
}

extern "C" void VR_StepNote(const char* fmt, ...)
{
    using namespace qvr::startlog;
    if(trace.phase == Phase::Off || !onMain())
    {
        return;
    }
    char text[1024];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    emit(trace.depth, "%s", text);
}
