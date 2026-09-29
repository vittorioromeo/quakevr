// vr_startup.cpp -- where the start-up and each map's load spend their time: marks at the end of each stage
// (VR_TimeMark), printed by vr_startup_times; and vr_walltime, the wall clock for test runs (docs/vr-port/TESTING.md).
// Stages: the engine's Host_Init calls, the first frame (the configs, the autoexec), the VR backend's start, the
// first frame drawn; a map's load from SV_SpawnServer (or the client's server info) to its first frame drawn.

#include "vr_engine.hpp"

#include <cstring>
#include <string>
#include <vector>

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
    std::string name;
    double ms;
    double at; // seconds since the process started
};

struct Group
{
    std::string title;
    std::vector<Mark> marks;
    double start = 0.0; // Sys_DoubleTime
    double last = 0.0;
    double nestedMs = 0.0; // map loads inside it (a start-up whose autoexec loads a map)
    struct Sum
    {
        std::string what;
        double ms;
        int count;
    };
    std::vector<Sum> sums; // VR_TimeAdd: kinds of work, across the stages
    bool open = false;
};

double processStart = -1.0; // Sys_DoubleTime of the process's creation
double lastWall = -1.0;     // vr_walltime's previous call
double lastCpu = 0.0;
Group startup;
Group load;
std::vector<std::string> loads; // every load's one-line summary

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
    g.marks.push_back({stage, (now - g.last) * 1000.0, sinceProcess(now)});
    g.last = now;
}

void print(const Group& g)
{
    Con_Printf("%s\n", g.title.c_str());
    for(const Mark& m : g.marks)
    {
        Con_Printf("  %-40s %8.1f ms  (at %.3f s)\n", m.name.c_str(), m.ms, m.at);
    }
    if(!g.sums.empty())
    {
        Con_Printf("  work across the stages (the lines indented under a line are part of it):\n");
        for(const Group::Sum& w : g.sums)
        {
            Con_Printf("    %-50s %8.1f ms  (%d)\n", w.what.c_str(), w.ms, w.count);
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
    for(const std::string& s : loads)
    {
        Con_Printf("%s\n", s.c_str());
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
    g->sums.push_back({what, seconds * 1000.0, 1});
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
        mark(startup, "(until the map load)", now);
    }
    load = Group{};
    load.title = std::string("vr_startup_times: the last map load (") + what + "), to its first frame drawn";
    load.start = load.last = now;
    load.open = true;
    VR_FileCacheEnable(1);
}

extern "C" void VR_TimeFrameEnd(int signedOn, int idle)
{
    const double now = Sys_DoubleTime();
    if(load.open && idle)
    {
        load.open = false; // a load that failed (Host_Error): not timed; the file lookups ask the file system again
        load.marks.clear();
        load.sums.clear();
        VR_FileCacheEnable(startup.open ? 1 : 0);
    }
    if(load.open && signedOn)
    {
        mark(load, "first frame drawn", now);
        load.open = false;
        VR_ImagePrefetchEnd();
        VR_FileCacheEnable(0);
        const double ms = (load.last - load.start) * 1000.0;
        loads.push_back(va("vr_startup_times: load %d: %.1f ms (%s)", static_cast<int>(loads.size()) + 1, ms,
            load.title.c_str() + load.title.find('(')));
        if(startup.open)
        {
            startup.nestedMs += ms;
            startup.last = now;
            startup.marks.push_back({"map load (below)", ms, sinceProcess(now)});
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
