// vr_startup.cpp -- where the start-up and each map's load spend their time: marks at the end of each stage
// (VR_TimeMark), printed by vr_startup_times; and vr_walltime, the wall clock for test runs (docs/vr-port/TESTING.md).
// Stages: the engine's Host_Init calls, the first frame (the configs, the autoexec), the VR backend's start, the
// first frame drawn; a map's load from its command (map, changelevel, restart, load: VR_TimeLoadCommand), else from
// SV_SpawnServer (or the client's server info), to its first frame drawn. A load that ends while a benchmark records
// (vr_bench_begin) goes into its JSON too (bench::loadDone: the stages, the work, the frames to the signon).

#include "vr_bench.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

struct Mark
{
    za::String name;
    double ms;
    double at; // seconds since the process started
};

struct Group
{
    za::String title;
    za::Vector<Mark> marks;
    double start = 0.0; // Sys_DoubleTime
    double last = 0.0;
    double nestedMs = 0.0; // map loads inside it (a start-up whose autoexec loads a map)
    struct Sum
    {
        za::String what;
        double ms;
        int count;
    };
    za::Vector<Sum> sums; // VR_TimeAdd: kinds of work, across the stages
    bool open = false;
    za::String what;           // a load's: its command and spawn ("map e1m2: e1m2", "client")
    int frames = 0;            // a load's frames ended before it signed on
    double lastFrameEnd = 0.0; // the last of them's end
};

double processStart = -1.0; // Sys_DoubleTime of the process's creation
double lastWall = -1.0;     // vr_walltime's previous call
double lastCpu = 0.0;
Group startup;
Group load;
za::Vector<za::String> loads; // every load's one-line summary
// A load's command (VR_TimeLoadCommand) until its spawn opens the load: its start, its stages so far (the map package,
// the campaign, the old server shut down), its name. Dropped at the frame's end when no load opened (a refused map).
struct Command
{
    double start = -1.0;
    double last = 0.0;
    za::String what;
    za::Vector<Mark> marks;
};
Command command;

double sinceProcess(double t)
{
    return t - processStart;
}

Group* current()
{
    return load.open ? &load : startup.open ? &startup : nullptr;
}

void mark(Group& g, const char* stage, double now)
{
    g.marks.pushBack({stage, (now - g.last) * 1000.0, sinceProcess(now)});
    g.last = now;
}

void print(const Group& g)
{
    Con_Printf("%s\n", g.title.cStr());
    for(const Mark& m : g.marks)
    {
        Con_Printf("  %-40s %8.1f ms  (at %.3f s)\n", m.name.cStr(), m.ms, m.at);
    }
    if(!g.sums.empty())
    {
        Con_Printf("  work across the stages (the lines indented under a line are part of it):\n");
        for(const Group::Sum& w : g.sums)
        {
            Con_Printf("    %-50s %8.1f ms  (%d)\n", w.what.cStr(), w.ms, w.count);
        }
    }
    if(!g.marks.empty())
    {
        const double total = (g.last - g.start) * 1000.0;
        if(g.nestedMs > 0.0)
        {
            Con_Printf("  %-40s %8.1f ms  (of which the map load %.1f)\n", "total", total, g.nestedMs);
        }
        else
        {
            Con_Printf("  %-40s %8.1f ms\n", "total", total);
        }
    }
}

void times_f()
{
    print(startup);
    if(!load.marks.empty())
    {
        print(load);
    }
    for(const za::String& s : loads)
    {
        Con_Printf("%s\n", s.cStr());
    }
}

// vr_walltime [label]: seconds since the process started and since the last vr_walltime (the wall clock, whatever the
// game's clock does: vr_fixed_frames, vr_mock_fast).
void walltime_f()
{
    const double now = Sys_DoubleTime();
    double cpu = 0.0; // the process's CPU time (all its threads): what a frame cap's waiting costs
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    if(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    {
        const auto u = [](const FILETIME& f) { return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
        cpu = static_cast<double>(u(kernel) + u(user)) * 1e-7;
    }
#endif
    Con_Printf("vr_walltime %s: %.3f s since start, %.3f s since the last (cpu %.3f s)\n", Cmd_Argc() > 1 ? Cmd_Args() : "",
        sinceProcess(now), lastWall < 0.0 ? 0.0 : now - lastWall, lastWall < 0.0 ? 0.0 : cpu - lastCpu);
    lastWall = now;
    lastCpu = cpu;
}

} // namespace

extern "C" void VR_TimeStart()
{
    const double now = Sys_DoubleTime();
    processStart = now;
#ifdef _WIN32
    // The time before main (the loader, the DLLs, the C runtime): the process's creation time on the same clock.
    FILETIME created, exited, kernel, user, nowFt;
    if(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    {
        GetSystemTimePreciseAsFileTime(&nowFt);
        const auto u = [](const FILETIME& f) { return (static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
        const double before = static_cast<double>(u(nowFt) - u(created)) * 1e-7;
        if(before > 0.0 && before < 60.0)
        {
            processStart = now - before;
        }
    }
#endif
    startup = Group{};
    startup.title = "vr_startup_times: start-up, process start to the first frame drawn";
    startup.start = processStart;
    startup.last = processStart;
    startup.open = true;
    mark(startup, "before main (loader, DLLs, C runtime)", now);
    VR_FileCacheEnable(1);
}

extern "C" void VR_TimeInit()
{
    Cmd_AddCommand("vr_startup_times", times_f);
    Cmd_AddCommand("vr_walltime", walltime_f);
}

extern "C" void VR_TimeMark(const char* stage)
{
    if(command.start >= 0.0)
    {
        const double now = Sys_DoubleTime();
        command.marks.pushBack({stage, (now - command.last) * 1000.0, sinceProcess(now)});
        command.last = now;
        return;
    }
    if(Group* g = current())
    {
        mark(*g, stage, Sys_DoubleTime());
    }
}

extern "C" void VR_TimeAdd(const char* what, double seconds)
{
    Group* const g = current();
    if(!g)
    {
        return;
    }
    for(Group::Sum& w : g->sums)
    {
        if(w.what == what)
        {
            w.ms += seconds * 1000.0;
            w.count++;
            return;
        }
    }
    g->sums.pushBack({what, seconds * 1000.0, 1});
}

extern "C" void VR_TimeLoadBegin(const char* what)
{
    if(load.open && !strcmp(what, "client"))
    {
        return; // (a server's load, then its own client's: one load; a server's load always starts anew)
    }
    const double now = Sys_DoubleTime();
    if(startup.open)
    {
        mark(startup, "(until the map load)", command.start >= 0.0 ? command.start : now); // (its command: the load's)
    }
    load = Group{};
    load.what = what;
    load.title = za::String("vr_startup_times: the last map load (") + what + "), to its first frame drawn";
    load.start = load.last = now;
    if(command.start >= 0.0)
    {
        // From the command: its stages before the spawn, then the rest of it to here.
        load.what = command.what + ": " + what;
        load.title = za::String("vr_startup_times: the last map load (") + load.what + "), from the command to its first "
                                                                                          "frame drawn";
        load.start = command.start;
        load.marks = static_cast<za::Vector<Mark>&&>(command.marks);
        load.last = command.last;
        mark(load, "command: the rest, to the spawn", now);
        command = Command{};
    }
    load.open = true;
    VR_FileCacheEnable(1);
}

extern "C" void VR_TimeLoadCommand(const char* what)
{
    command = Command{};
    command.start = command.last = Sys_DoubleTime();
    command.what = what;
}

extern "C" void VR_TimeFrameEnd(int signedOn, int idle)
{
    const double now = Sys_DoubleTime();
    if(command.start >= 0.0)
    {
        command = Command{}; // (a command that loaded nothing: refused, or a map not found)
    }
    if(load.open && idle)
    {
        load.open = false; // a load that failed (Host_Error): not timed; the file lookups ask the file system again
        load.marks.clear();
        load.sums.clear();
        VR_FileCacheEnable(startup.open ? 1 : 0);
    }
    if(load.open && !signedOn && !idle)
    {
        load.frames++;
        load.lastFrameEnd = now;
    }
    else if(load.open && signedOn)
    {
        // The frames before the signon (the load's own and the handshake's) apart from the first one drawn (its
        // shaders' and textures' first use; and a frame cap's wait before it).
        if(load.frames > 0 && load.lastFrameEnd > load.last)
        {
            const double end = load.lastFrameEnd;
            load.marks.pushBack({va("frames to the signon (%d), the rest of them", load.frames), (end - load.last) * 1000.0,
                sinceProcess(end)});
            load.last = end;
        }
        mark(load, "first frame drawn", now);
        load.open = false;
        VR_ImagePrefetchEnd();
        VR_FileCacheEnable(0);
        const double ms = (load.last - load.start) * 1000.0;
        loads.pushBack(va("vr_startup_times: load %d: %.1f ms (%s)", static_cast<int>(loads.size()) + 1, ms,
            load.title.cStr() + load.title.find('(')));
        if(qvr::bench::recording)
        {
            za::Vector<qvr::bench::LoadStage> stages;
            za::Vector<qvr::bench::LoadStage> work;
            stages.reserve(load.marks.size());
            work.reserve(load.sums.size());
            for(const Mark& m : load.marks)
            {
                stages.pushBack({m.name.cStr(), m.ms, 1});
            }
            for(const Group::Sum& w : load.sums)
            {
                work.pushBack({w.what.cStr(), w.ms, w.count});
            }
            qvr::bench::loadDone(load.what.cStr(), cl.mapname, ms, load.frames, stages.data(),
                static_cast<int>(stages.size()), work.data(), static_cast<int>(work.size()));
        }
        if(startup.open)
        {
            startup.nestedMs += ms;
            startup.last = now;
            startup.marks.pushBack({"map load (below)", ms, sinceProcess(now)});
        }
        if(developer.value)
        {
            print(load);
        }
    }
    if(startup.open && !load.open)
    {
        mark(startup, "first frame drawn", now);
        startup.open = false;
        VR_FileCacheEnable(0);
        if(developer.value)
        {
            print(startup);
        }
    }
}
