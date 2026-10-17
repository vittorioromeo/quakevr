// vr_xr_overlap.cpp -- see vr_xr_overlap.hpp.

#include "vr_xr_overlap.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"

#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Math/MinMax.hpp"

#include "SDL.h"

extern "C" void BGM_Update(void); // bgmusic.c
extern "C" cvar_t host_maxfps;    // host.c

namespace qvr::xroverlap
{

namespace
{

// The GPU's tail foretold, the lesser of two lower bounds:
// - the eyes' GPU time (the profiler's timestamp queries, dynres's input: the least of the last gpuKept read back) less
//   the CPU's time since the first eye began: the GPU cannot start a command before the CPU issues it;
// - the least of the last tailsKept tails measured.
// Of it, budgetShare is spent. No overlap until both are known (the first frames, after a level's load).
constexpr int gpuKept = 4;
constexpr int tailsKept = 4;
constexpr double budgetShare = 0.8;
// The tail measured: a timestamp query after the frame's last command, read back a few frames later (never waited
// for), against the flush's CPU time. The GL's clock is tied to the CPU's (glGetInteger64v GL_TIMESTAMP) once every
// calibrateEvery seconds as the screen update starts: a glGet waits for NVIDIA's threaded driver to run all queued,
// 0.5-1.4 ms after the eyes' draws (measured), next to nothing with the queue empty.
constexpr int queryCount = 8;
constexpr double calibrateEvery = 1.0;
// A task's cost foretold: the moving average of its runs (either place), unknownCostMs before its first.
constexpr double costRate = 0.2;
constexpr double unknownCostMs = 0.05;
constexpr float testWorkMaxMs = 50.f;
constexpr double lateShare = 1.25; // vr_xr_overlap_report's late frames: over this many frames (as vr_memstats's slow_frames)

using GetInteger64 = void(APIENTRY*)(GLenum, GLint64*);

struct Pending
{
    GLuint query{0};
    double flushMs{0.0}; // the CPU's time at the flush
    double estimateMs{0.0};
    double workMs{0.0}; // the work run before the end call
    double endMs{0.0};  // the end call's time
    bool waiting{false};
};

struct Task
{
    double costMs{-1.0};
    bool ran{false}; // ran before this frame's end call: its usual place skips it
};

// vr_xr_overlap_report: since the last.
struct Stats
{
    int frames{0};     // frames the overlap was weighed for
    int overlapped{0}; // and ran a task in
    double workMs{0.0};
    double estimateMs{0.0};
    int measured{0}; // tails read back
    double tailMs{0.0};
    double errorMs{0.0}; // the tail less its estimate
    double absErrorMs{0.0};
    int under{0};       // tails under their estimate (the bound's misses)
    int overruns{0};    // frames whose work outlasted the GPU's tail (their end call came that much late)
    double overrunMs{0.0};
    int waited{0};      // frames whose end call returned within waitSlackMs of the GPU's end (it waited for the GPU)
    double ownMs{0.0};  // the overlap's own: the query, the flush, the read-back
    int calibrations{0};
    double calibrateMs{0.0};
    int early[VR_XROVERLAP_COUNT]{};
    int late[VR_XROVERLAP_COUNT]{};
    int ends{0};
    double endMs{0.0};
    double endMaxMs{0.0};
    int intervals{0}; // between two rendered end calls
    double intervalMs{0.0};
    int lateFrames{0}; // over lateShare frames
};
constexpr double waitSlackMs = 0.5;

struct State
{
    bool armed{false};
    bool made{false};
    bool failed{false};
    GetInteger64 getInteger64{nullptr};
    // The GL's clock less the CPU's (ns), and when it was taken (CPU ms; 0: not yet).
    double gpuLessCpuNs{0.0};
    double calibrated{0.0};
    Pending pending[queryCount];
    int next{0}; // the slot written next: the oldest
    double tails[tailsKept]{};
    int tailCount{0};
    int tailNext{0};
    double eyesBegin{0.0}; // the CPU's time as the first eye began (ms)
    Task tasks[VR_XROVERLAP_COUNT];
    Pending* current{nullptr}; // this frame's, between beforeEnd and afterEnd
    double lastEnd{0.0};       // the last rendered end call's return (ms; 0 none)
    Stats stats;
};
State st;

const char* const taskNames[VR_XROVERLAP_COUNT] = {"particles", "music", "test"};

[[nodiscard]] double nowMs()
{
    return static_cast<double>(za::Clock::nowNanoseconds()) / 1e6;
}

[[nodiscard]] float testWorkMs()
{
    return za::min(vr_debug_xr_overlap_work_ms.value, testWorkMaxMs);
}

void runTask(int task)
{
    switch(task)
    {
        case VR_XROVERLAP_PARTICLES: CL_RunParticles(); break;
        case VR_XROVERLAP_MUSIC: BGM_Update(); break; // adds music raw samples (S_Update paints them, after)
        case VR_XROVERLAP_TEST:
        {
            const double until = nowMs() + static_cast<double>(testWorkMs());
            while(nowMs() < until)
            {
            }
            break;
        }
        default: break;
    }
}

double timedRun(int task)
{
    const double t0 = nowMs();
    runTask(task);
    const double ms = nowMs() - t0;
    Task& t = st.tasks[task];
    t.costMs = t.costMs < 0.0 ? ms : t.costMs + costRate * (ms - t.costMs);
    return ms;
}

[[nodiscard]] bool ensureMade()
{
    if(st.made || st.failed)
    {
        return st.made;
    }
    st.getInteger64 = reinterpret_cast<GetInteger64>(SDL_GL_GetProcAddress("glGetInteger64v"));
    if(!st.getInteger64 || !GL_QueryCounterFunc || !GL_GetQueryObjectivFunc || !GL_GetQueryObjectui64vFunc ||
        !GL_GenQueriesFunc)
    {
        st.failed = true;
        Con_DPrintf("vr_xr_overlap: no GL timestamp queries: off\n");
        return false;
    }
    for(Pending& p : st.pending)
    {
        GL_GenQueriesFunc(1, &p.query);
    }
    st.made = true;
    return true;
}

// The GL's clock tied to the CPU's (the screen update's start: the driver's queue about empty).
void calibrate()
{
    const double now = nowMs();
    if(st.calibrated > 0.0 && now - st.calibrated < calibrateEvery * 1000.0)
    {
        return;
    }
    const za::I64 before = za::Clock::nowNanoseconds();
    GLint64 gpu = 0;
    st.getInteger64(GL_TIMESTAMP, &gpu);
    const za::I64 after = za::Clock::nowNanoseconds();
    st.gpuLessCpuNs = static_cast<double>(gpu) - 0.5 * (static_cast<double>(before) + static_cast<double>(after));
    st.calibrated = now;
    ++st.stats.calibrations;
    st.stats.calibrateMs += static_cast<double>(after - before) / 1e6;
}

// The tails done since, oldest first (never waits).
void resolve()
{
    for(int k = 0; k < queryCount; k++)
    {
        Pending& p = st.pending[(st.next + k) % queryCount];
        if(!p.waiting)
        {
            continue;
        }
        GLint available = 0;
        GL_GetQueryObjectivFunc(p.query, GL_QUERY_RESULT_AVAILABLE, &available);
        if(!available)
        {
            break;
        }
        GLuint64 done = 0;
        GL_GetQueryObjectui64vFunc(p.query, GL_QUERY_RESULT, &done);
        p.waiting = false;
        const double doneMs = (static_cast<double>(done) - st.gpuLessCpuNs) / 1e6; // on the CPU's clock
        const double tail = za::max(0.0, doneMs - p.flushMs);
        st.tails[st.tailNext] = tail;
        st.tailNext = (st.tailNext + 1) % tailsKept;
        st.tailCount = za::min(st.tailCount + 1, tailsKept);
        const double error = tail - p.estimateMs;
        Stats& s = st.stats;
        ++s.measured;
        s.tailMs += tail;
        s.errorMs += error;
        s.absErrorMs += error < 0.0 ? -error : error;
        s.under += error < 0.0 ? 1 : 0;
        if(p.workMs > tail)
        {
            ++s.overruns;
            s.overrunMs += p.workMs - tail;
        }
        const double endReturn = p.flushMs + p.workMs + p.endMs;
        s.waited += endReturn >= doneMs - 0.05 && endReturn <= doneMs + waitSlackMs ? 1 : 0;
        VR_TracyPlot("XR overlap GPU tail ms", tail);
        VR_TracyPlot("XR overlap estimate error ms", error);
    }
}

// The tail foretold at the flush (ms), at `now`: < 0 not known.
[[nodiscard]] double estimateMs(double now)
{
    if(st.tailCount < tailsKept || st.eyesBegin <= 0.0)
    {
        return -1.0;
    }
    double least = st.tails[0];
    for(int i = 1; i < st.tailCount; i++)
    {
        least = za::min(least, st.tails[i]);
    }
    double gpu = -1.0;
    int found = 0;
    profile::FrameSample s;
    for(int back = 0; found < gpuKept && back < 16 && profile::frameSample(back, s); back++)
    {
        if(s.gpuMs >= 0.f)
        {
            gpu = found == 0 ? s.gpuMs : za::min(gpu, static_cast<double>(s.gpuMs));
            ++found;
        }
    }
    if(found == 0)
    {
        return -1.0;
    }
    return za::min(least, gpu - (now - st.eyesBegin));
}

void report_f()
{
    const Stats& s = st.stats;
    const auto per = [](double sum, int n) { return n > 0 ? sum / n : 0.0; };
    Con_Printf("vr_xr_overlap_report: vr_xr_overlap %g, %d frames weighed (%d overlapped), estimate %.3f ms, GPU tail "
               "%.3f ms (%d read back), error %+.3f ms (abs %.3f, %d under), work %.3f ms a frame before the end call, "
               "past the tail %d frames (%.3f ms each); end call %.3f ms (max %.2f, %d; %d waited for the GPU)\n",
        vr_xr_overlap.value, s.frames, s.overlapped, per(s.estimateMs, s.frames), per(s.tailMs, s.measured), s.measured,
        per(s.errorMs, s.measured), per(s.absErrorMs, s.measured), s.under, per(s.workMs, s.frames), s.overruns,
        per(s.overrunMs, s.overruns), per(s.endMs, s.ends), s.endMaxMs, s.ends, s.waited);
    Con_Printf("  its own %.3f ms a frame (query, flush, read-back), clock ties %d (%.3f ms each)\n", per(s.ownMs, s.frames),
        s.calibrations, per(s.calibrateMs, s.calibrations));
    Con_Printf("  frames: %.3f ms apart, %d late of %d (over %.2f frames)\n", per(s.intervalMs, s.intervals),
        s.lateFrames, s.intervals, lateShare);
    Con_Printf("  before/after the end call:");
    for(int i = 0; i < VR_XROVERLAP_COUNT; i++)
    {
        Con_Printf(" %s %d/%d (%.3f ms)", taskNames[i], s.early[i], s.late[i], za::max(0.0, st.tasks[i].costMs));
    }
    Con_Printf("\n");
    st.stats = Stats{};
}

} // namespace

void eyesBegin()
{
    st.eyesBegin = nowMs();
}

void beforeEnd(bool rendered, bool spectator)
{
    const bool armed = st.armed;
    st.armed = false;
    st.current = nullptr;
    for(Task& t : st.tasks)
    {
        t.ran = false;
    }
    if(!armed || !rendered || vr_xr_overlap.value <= 0.f || !st.made || st.calibrated <= 0.0)
    {
        st.eyesBegin = 0.0;
        return;
    }
    QVR_TRACY_SCOPE(zoneOverlap, "xr overlap");
    const double start = nowMs();
    Pending& p = st.pending[st.next];
    st.next = (st.next + 1) % queryCount;
    GL_QueryCounterFunc(p.query, GL_TIMESTAMP); // after the frame's last command
    glFlush();                                  // the GPU has it all: it runs while the work below does
    const double t0 = nowMs();                  // (the tail is from here: the work's budget too)
    const double estimate = estimateMs(t0);
    st.eyesBegin = 0.0;
    resolve();
    const double budget = estimate * budgetShare;
    bool any = false;
    double tasksMs = 0.0;
    for(int task = 0; task < VR_XROVERLAP_COUNT; task++)
    {
        if((task == VR_XROVERLAP_PARTICLES && spectator) || (task == VR_XROVERLAP_TEST && testWorkMs() <= 0.f))
        {
            continue;
        }
        const Task& t = st.tasks[task];
        const double cost = t.costMs < 0.0 ? unknownCostMs : t.costMs;
        if(nowMs() - t0 + cost > budget)
        {
            continue; // deferred: its usual place, after the end call
        }
        tasksMs += timedRun(task);
        st.tasks[task].ran = true;
        ++st.stats.early[task];
        any = true;
    }
    const double end = nowMs();
    p.flushMs = t0;
    p.estimateMs = za::max(0.0, estimate);
    p.workMs = end - t0;
    p.endMs = 0.0;
    p.waiting = true;
    st.current = &p;
    Stats& s = st.stats;
    ++s.frames;
    s.overlapped += any ? 1 : 0;
    s.workMs += p.workMs;
    s.estimateMs += p.estimateMs;
    s.ownMs += end - start - tasksMs;
    VR_TracyPlot("XR overlap ms", p.workMs);
    VR_TracyPlot("XR overlap estimate ms", p.estimateMs);
}

void afterEnd(bool rendered, double endMs)
{
    if(st.current)
    {
        st.current->endMs = endMs;
        st.current = nullptr;
    }
    const double now = nowMs();
    const double interval = st.lastEnd > 0.0 ? now - st.lastEnd : 0.0;
    st.lastEnd = rendered ? now : 0.0;
    if(!rendered)
    {
        return;
    }
    Stats& s = st.stats;
    ++s.ends;
    s.endMs += endMs;
    s.endMaxMs = za::max(s.endMaxMs, endMs);
    // Late: the interval from the last end call over 1.25 frames (the runtime's display period; the mock's: host_maxfps).
    const double display = profile::displayPeriodMs();
    const double period = display > 0.0 ? display : host_maxfps.value > 0.f ? 1000.0 / host_maxfps.value : 0.0;
    if(interval > 0.0 && period > 0.0)
    {
        ++s.intervals;
        s.intervalMs += interval;
        s.lateFrames += interval > lateShare * period ? 1 : 0;
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_xr_overlap_report", report_f);
}

} // namespace qvr::xroverlap

extern "C" void VR_XrOverlapArm(int on)
{
    using namespace qvr::xroverlap;
    st.armed = on != 0;
    if(!on)
    {
        return;
    }
    for(Task& t : st.tasks)
    {
        t.ran = false; // (a frame that jumped out before its usual places)
    }
    if(qvr::vr_xr_overlap.value > 0.f && qvr::backend() && cls.state != ca_dedicated && ensureMade())
    {
        calibrate();
    }
}

extern "C" void VR_XrOverlapLate(int task)
{
    using namespace qvr::xroverlap;
    if(task < 0 || task >= VR_XROVERLAP_COUNT)
    {
        return;
    }
    Task& t = st.tasks[task];
    if(t.ran)
    {
        t.ran = false;
        return;
    }
    if(task == VR_XROVERLAP_TEST && testWorkMs() <= 0.f)
    {
        return;
    }
    timedRun(task);
    ++st.stats.late[task];
}
