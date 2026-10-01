// vr_allocsites.cpp -- vr_alloc_sites [frames] [lines]: the main thread's C++ allocations over the next frames (300) by
// where they were asked for (vr_alloccount.cpp's trace: the first frame of each call stack outside Zancle's containers,
// the standard library and the scratch sets), the commonest first: how many a frame, the place, and its caller. To find
// the buffers a frame makes and frees (CODE_STYLE.md, "Scratch buffers and caches"); the profiler's "allocations" says
// how many. Debug > Profiling and Memory > Allocation Sites. Windows (dbghelp, with the build's .pdb).

#include "vr_engine.hpp"
#include "vr_alloccount.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::allocsites
{
namespace
{

int framesLeft = 0;  // of the trace under way (0: none)
int framesTraced = 0;
int linesShown = 25;

void report()
{
    za::U64 total = 0, dropped = 0;
    const za::Vector<alloccount::Site> sites = alloccount::traceSites(total, dropped);
    const double frames = static_cast<double>(za::max(framesTraced, 1));
    Con_Printf("vr_alloc_sites: %llu allocations in %d frames (%.2f a frame), %d sites%s\n",
        static_cast<unsigned long long>(total), framesTraced, static_cast<double>(total) / frames,
        static_cast<int>(sites.size()), dropped ? va(" (%llu not placed: the table was full)", static_cast<unsigned long long>(dropped)) : "");
    int shown = 0;
    for(const alloccount::Site& s : sites)
    {
        if(shown++ >= linesShown)
        {
            break;
        }
        Con_Printf("%8.2f/frame %6llu  %s\n                  <- %s\n", static_cast<double>(s.count) / frames,
            static_cast<unsigned long long>(s.count), s.where.cStr(), s.via.empty() ? "?" : s.via.cStr());
    }
}

void command_f()
{
    if(framesLeft > 0)
    {
        Con_Printf("vr_alloc_sites: already tracing (%d frames left)\n", framesLeft);
        return;
    }
    framesTraced = Cmd_Argc() > 1 ? za::max(Q_atoi(Cmd_Argv(1)), 1) : 300;
    linesShown = Cmd_Argc() > 2 ? za::max(Q_atoi(Cmd_Argv(2)), 1) : 25;
    framesLeft = framesTraced;
    Con_Printf("vr_alloc_sites: tracing the main thread's allocations for %d frames\n", framesTraced);
    alloccount::traceBegin();
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_alloc_sites", command_f);
}

void frameEnd()
{
    if(framesLeft <= 0 || --framesLeft > 0)
    {
        return;
    }
    alloccount::traceEnd();
    report();
}

} // namespace qvr::allocsites
