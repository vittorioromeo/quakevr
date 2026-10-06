// vr_bench.cpp -- see vr_bench.hpp.

#include "vr_bench.hpp"
#include "vr_alloccount.hpp"
#include "vr_box3d.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_lighting.hpp"
#include "vr_main.hpp"
#include "vr_particles.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern "C" int r_numactiveparticles; // r_part.c

namespace qvr::bench
{

bool recording = false;

namespace
{

using profile::PhaseCount;

// Each frame's numbers (ms, or a count).
enum Series
{
    Period,
    Host,
    Busy,
    Traces,
    DrawCalls,
    AliasDrawn,
    HeapEvents, // the main thread's new/malloc/calloc/realloc
    SeriesCount
};
constexpr const char* seriesNames[SeriesCount] = {
    "frame_ms", "host_ms", "cpu_busy_ms", "traces", "draw_calls", "alias_drawn", "heap_allocs"};

// What there is, every 16th frame and at the start and end.
enum Count
{
    Edicts,
    Monsters,
    Bodies,
    BodiesAwake,
    Contacts,
    Particles,   // Ironwail's (r_numactiveparticles)
    VrParticles, // Quake VR's
    Decals,
    Dlights,
    ShadowDlights,
    ShadowMapLights,
    CountCount
};
constexpr const char* countNames[CountCount] = {"edicts", "monsters_alive", "box3d_bodies", "box3d_awake",
    "box3d_contacts", "particles", "vr_particles", "decals", "dlights", "shadow_dlights", "shadow_maplights"};
constexpr int countEvery = 16;
// GPU read-backs dropped at the start: the slots in flight then are the frames before it (vr_profile.cpp's ring).
constexpr int gpuSkip = 6;

struct Capture
{
    za::String name;
    za::String map;
    int targetFrames{0};    // 0: until vr_bench_end (or the seconds)
    double targetSeconds{0.0};
    za::I64 startNs{0};
    int frames{0};
    int skip{0}; // the frame vr_bench_begin ran in (its rest only): not recorded
    int gpuSkipLeft{0};
    za::Vector<float> series[SeriesCount];
    za::Vector<float> gpuEyes;
    double cpuPhaseSum[PhaseCount]{};
    double gpuPhaseSum[PhaseCount]{};
    int gpuFrames{0};
    double countSum[CountCount]{};
    int countMax[CountCount]{};
    int countSamples{0};
    int startCounts[CountCount]{};
    za::U64 heapBefore{0};
    za::U64 heapRequestedStart{0};
    za::U64 heapRequestedBefore{0};
};
Capture capture;

[[nodiscard]] za::U64 heapEvents(const alloccount::Stats& s)
{
    using alloccount::Kind;
    return s.calls[static_cast<int>(Kind::New)] + s.calls[static_cast<int>(Kind::Malloc)] +
           s.calls[static_cast<int>(Kind::Calloc)] + s.calls[static_cast<int>(Kind::Realloc)];
}

void takeCounts(int (&n)[CountCount])
{
    for(int& c : n)
    {
        c = 0;
    }
    if(sv.active && sv.qcvm.progs)
    {
        qcvm_t* old = nullptr;
        PR_PushQCVM(&sv.qcvm, &old);
        for(int i = 1; i < qcvm->num_edicts; i++)
        {
            edict_t* ent = EDICT_NUM(i);
            if(ent->free)
            {
                continue;
            }
            n[Edicts]++;
            if(!ZA_STRNCMP(PR_GetString(ent->v.classname), "monster_", 8) && ent->v.deadflag == 0.f &&
                ent->v.health > 0.f)
            {
                n[Monsters]++;
            }
        }
        PR_PopQCVM(old);
    }
    box3d::profileCounts(n[Bodies], n[BodiesAwake], n[Contacts]);
    n[Particles] = r_numactiveparticles;
    n[VrParticles] = particles::liveCount();
    n[Decals] = decals::liveCount();
    for(const dlight_t& l : cl_dlights)
    {
        n[Dlights] += l.die >= cl.time && l.radius > 0.f ? 1 : 0;
    }
    lighting::shadowCounts(n[ShadowDlights], n[ShadowMapLights]);
}

void printCounts(const char* when, const int (&n)[CountCount])
{
    Con_Printf("vr_bench: %s counts", when);
    for(int c = 0; c < CountCount; c++)
    {
        Con_Printf(" %s=%d", countNames[c], n[c]);
    }
    Con_Printf("\n");
}

struct Stats
{
    int n{0};
    double avg{0.0}, p50{0.0}, p95{0.0}, p99{0.0}, max{0.0};
};

// Nearest-rank percentiles of a copy (the series stays in frame order).
[[nodiscard]] Stats stats(const za::Vector<float>& v)
{
    Stats s;
    s.n = static_cast<int>(v.size());
    if(s.n == 0)
    {
        return s;
    }
    za::Vector<float> sorted;
    sorted.reserve(v.size());
    double sum = 0.0;
    for(const float x : v)
    {
        sorted.pushBack(x);
        sum += x;
    }
    za::quickSort(sorted.begin(), sorted.end());
    const auto at = [&](double q) {
        const int k = za::min(s.n - 1, za::max(0, static_cast<int>(q * s.n + 0.999999) - 1));
        return static_cast<double>(sorted[static_cast<za::SizeT>(k)]);
    };
    s.avg = sum / s.n;
    s.p50 = at(0.50);
    s.p95 = at(0.95);
    s.p99 = at(0.99);
    s.max = sorted[sorted.size() - 1];
    return s;
}

void writeStats(FILE* f, const char* name, const Stats& s, bool last = false)
{
    fprintf(f, "    \"%s\": {\"n\": %d, \"avg\": %.4f, \"p50\": %.4f, \"p95\": %.4f, \"p99\": %.4f, \"max\": %.4f}%s\n",
        name, s.n, s.avg, s.p50, s.p95, s.p99, s.max, last ? "" : ",");
}

[[nodiscard]] za::String jsonEscaped(const char* text)
{
    za::String out;
    for(const char* c = text; *c; c++)
    {
        if(*c == '"' || *c == '\\')
        {
            out += '\\';
        }
        if(static_cast<unsigned char>(*c) >= 32)
        {
            out += *c;
        }
    }
    return out;
}

void finish()
{
    recording = false;
    Capture& c = capture;
    int endCounts[CountCount]{};
    takeCounts(endCounts);
    const double seconds = static_cast<double>(za::Clock::nowNanoseconds() - c.startNs) / 1e9;
    const za::String dir = za::String{com_gamedir} + "/profile";
    Sys_mkdir(dir.cStr());
    const za::String benchDir = dir + "/bench";
    Sys_mkdir(benchDir.cStr());
    const za::String path = benchDir + "/" + c.name + ".json";
    FILE* f = fopen(path.cStr(), "w");
    if(!f)
    {
        Con_Printf("vr_bench: can't write %s\n", path.cStr());
        return;
    }
    Stats s[SeriesCount];
    for(int i = 0; i < SeriesCount; i++)
    {
        s[i] = stats(c.series[i]);
    }
    const Stats eyes = stats(c.gpuEyes);
    const za::Vector<float>& period = c.series[Period];
    // Hitches: over fixed limits (a refresh at 90 and 72 Hz, two at 60, a tenth and a quarter of a second) and over
    // twice the median frame.
    constexpr double limits[] = {11.111, 13.889, 33.333, 100.0, 250.0};
    int over[5]{};
    int overMedian = 0;
    for(const float p : period)
    {
        for(int k = 0; k < 5; k++)
        {
            over[k] += p > limits[k] ? 1 : 0;
        }
        overMedian += p > 2.0 * s[Period].p50 ? 1 : 0;
    }
    const time_t now = time(nullptr);
    char when[32];
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", localtime(&now));
    int w = 0, h = 0;
    if(Backend* be = backend())
    {
        be->eyeResolution(w, h);
    }
    const double frames = za::max(c.frames, 1);

    fprintf(f, "{\n  \"name\": \"%s\",\n  \"map\": \"%s\",\n  \"date\": \"%s\",\n", c.name.cStr(),
        jsonEscaped(c.map.cStr()).cStr(), when);
    fprintf(f, "  \"frames\": %d,\n  \"seconds\": %.3f,\n  \"eye_resolution\": [%d, %d],\n", c.frames, seconds, w, h);
    fprintf(f, "  \"gl_renderer\": \"%s\",\n", jsonEscaped(gl_renderer ? gl_renderer : "?").cStr());
    fprintf(f, "  \"settings\": {");
    bool first = true;
    for(const char* name : profile::settingCvars)
    {
        if(const cvar_t* v = Cvar_FindVar(name))
        {
            fprintf(f, "%s\"%s\": \"%s\"", first ? "" : ", ", name, jsonEscaped(v->string).cStr());
            first = false;
        }
    }
    fprintf(f, "},\n  \"frame\": {\n");
    for(int i = 0; i < SeriesCount; i++)
    {
        writeStats(f, seriesNames[i], s[i]);
    }
    writeStats(f, "gpu_eyes_ms", eyes, true);
    fprintf(f, "  },\n  \"hitches\": {\"over_11ms\": %d, \"over_14ms\": %d, \"over_33ms\": %d, \"over_100ms\": %d, "
               "\"over_250ms\": %d, \"over_2x_median\": %d},\n",
        over[0], over[1], over[2], over[3], over[4], overMedian);
    fprintf(f, "  \"cpu_phase_ms\": {");
    for(int i = 0; i < PhaseCount; i++)
    {
        fprintf(f, "%s\"%s\": %.4f", i ? ", " : "", profile::phaseName(static_cast<profile::Phase>(i)),
            c.cpuPhaseSum[i] / frames);
    }
    fprintf(f, "},\n  \"gpu_frames\": %d,\n  \"gpu_phase_ms\": {", c.gpuFrames);
    first = true;
    for(int i = 0; i < PhaseCount; i++)
    {
        if(profile::phaseGpu(static_cast<profile::Phase>(i)))
        {
            fprintf(f, "%s\"%s\": %.4f", first ? "" : ", ", profile::phaseName(static_cast<profile::Phase>(i)),
                c.gpuPhaseSum[i] / za::max(c.gpuFrames, 1));
            first = false;
        }
    }
    fprintf(f, "},\n  \"heap_requested_kib\": %.1f,\n",
        static_cast<double>(c.heapRequestedBefore - c.heapRequestedStart) / 1024.0);
    fprintf(f, "  \"counts\": {");
    for(int k = 0; k < CountCount; k++)
    {
        fprintf(f, "%s\"%s\": {\"start\": %d, \"avg\": %.1f, \"max\": %d, \"end\": %d}", k ? ", " : "", countNames[k],
            c.startCounts[k], c.countSum[k] / za::max(c.countSamples, 1), c.countMax[k], endCounts[k]);
    }
    fprintf(f, "}\n}\n");
    fclose(f);

    printCounts("end", endCounts);
    Con_Printf("vr_bench: %s %d frames %.1f s: frame avg %.3f p50 %.3f p95 %.3f p99 %.3f max %.2f ms; cpu busy avg %.3f "
               "p99 %.3f; gpu eyes avg %.3f p99 %.3f (%d); hitches >33ms %d; heap %.1f/frame -> %s\n",
        c.name.cStr(), c.frames, seconds, s[Period].avg, s[Period].p50, s[Period].p95, s[Period].p99, s[Period].max,
        s[Busy].avg, s[Busy].p99, eyes.avg, eyes.p99, eyes.n, over[2], s[HeapEvents].avg, path.cStr());
}

void begin_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_bench_begin <name> [frames | <seconds>s]: records each frame's times until vr_bench_end (or the "
                   "count), into <gamedir>/profile/bench/<name>.json\n");
        return;
    }
    if(recording)
    {
        Con_Printf("vr_bench: %s ended by a new capture\n", capture.name.cStr());
        finish();
    }
    Capture& c = capture;
    c.name.clear();
    for(const char* p = Cmd_Argv(1); *p; p++) // (a file name)
    {
        const char ch = *p;
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                        ch == '_' || ch == '-' || ch == '.';
        c.name += ok ? ch : '_';
    }
    c.map = cl.mapname;
    c.targetFrames = 0;
    c.targetSeconds = 0.0;
    if(Cmd_Argc() > 2)
    {
        const char* arg = Cmd_Argv(2);
        const za::SizeT len = za::String{arg}.size();
        if(len > 0 && arg[len - 1] == 's')
        {
            c.targetSeconds = atof(arg);
        }
        else
        {
            c.targetFrames = atoi(arg);
        }
    }
    const za::SizeT reserve = static_cast<za::SizeT>(c.targetFrames > 0 ? c.targetFrames + 8 : 1 << 16);
    for(za::Vector<float>& v : c.series)
    {
        v.clear();
        v.reserve(reserve); // (no allocation while recording: the heap counts are the game's)
    }
    c.gpuEyes.clear();
    c.gpuEyes.reserve(reserve);
    for(int i = 0; i < PhaseCount; i++)
    {
        c.cpuPhaseSum[i] = 0.0;
        c.gpuPhaseSum[i] = 0.0;
    }
    for(int k = 0; k < CountCount; k++)
    {
        c.countSum[k] = 0.0;
        c.countMax[k] = 0;
    }
    c.countSamples = 0;
    c.gpuFrames = 0;
    c.frames = 0;
    c.skip = 1;
    c.gpuSkipLeft = gpuSkip;
    takeCounts(c.startCounts);
    printCounts("start", c.startCounts);
    const alloccount::Stats heap = alloccount::statsThisThread();
    c.heapBefore = heapEvents(heap);
    c.heapRequestedStart = c.heapRequestedBefore = heap.requestedBytes;
    c.startNs = za::Clock::nowNanoseconds();
    recording = true;
}

void end_f()
{
    if(!recording)
    {
        Con_Printf("vr_bench_end: no capture (vr_bench_begin)\n");
        return;
    }
    finish();
}

void seed_f()
{
    const unsigned seed = Cmd_Argc() > 1 ? static_cast<unsigned>(atoi(Cmd_Argv(1))) : 1u;
    srand(seed); // (QuakeC's random() and the engine's rand())
    Con_Printf("vr_bench_seed %u\n", seed);
}

} // namespace

void frame(double periodMs, double hostMs, double busyMs, const za::I64 (&phaseNs)[profile::PhaseCount])
{
    Capture& c = capture;
    const alloccount::Stats heap = alloccount::statsThisThread();
    const za::U64 events = heapEvents(heap);
    if(c.skip > 0)
    {
        c.skip--;
        c.heapBefore = events;
        c.heapRequestedStart = c.heapRequestedBefore = heap.requestedBytes;
        c.startNs = za::Clock::nowNanoseconds();
        return;
    }
    const float values[SeriesCount] = {static_cast<float>(periodMs), static_cast<float>(hostMs),
        static_cast<float>(busyMs), static_cast<float>(vr_profcounts.traces), static_cast<float>(vr_profcounts.drawcalls),
        static_cast<float>(vr_profcounts.aliasdrawn), static_cast<float>(events - c.heapBefore)};
    c.heapBefore = events;
    c.heapRequestedBefore = heap.requestedBytes;
    for(int i = 0; i < SeriesCount; i++)
    {
        c.series[i].pushBack(values[i]);
    }
    for(int i = 0; i < PhaseCount; i++)
    {
        c.cpuPhaseSum[i] += static_cast<double>(phaseNs[i]) / 1e6;
    }
    if(c.frames % countEvery == 0)
    {
        int n[CountCount];
        takeCounts(n);
        for(int k = 0; k < CountCount; k++)
        {
            c.countSum[k] += n[k];
            c.countMax[k] = za::max(c.countMax[k], n[k]);
        }
        c.countSamples++;
    }
    c.frames++;
    const bool framesDone = c.targetFrames > 0 && c.frames >= c.targetFrames;
    const bool secondsDone =
        c.targetSeconds > 0.0 && static_cast<double>(za::Clock::nowNanoseconds() - c.startNs) / 1e9 >= c.targetSeconds;
    if(framesDone || secondsDone)
    {
        finish();
    }
}

void gpuFrame(const double (&phaseMs)[profile::PhaseCount])
{
    Capture& c = capture;
    if(c.gpuSkipLeft > 0)
    {
        c.gpuSkipLeft--;
        return;
    }
    for(int i = 0; i < PhaseCount; i++)
    {
        c.gpuPhaseSum[i] += phaseMs[i];
    }
    c.gpuEyes.pushBack(static_cast<float>(phaseMs[profile::EyeL] + phaseMs[profile::EyeR]));
    c.gpuFrames++;
}

void registerCommands()
{
    Cmd_AddCommand("vr_bench_begin", begin_f);
    Cmd_AddCommand("vr_bench_end", end_f);
    Cmd_AddCommand("vr_bench_seed", seed_f);
}

} // namespace qvr::bench
