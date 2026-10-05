// vr_alloc_sites [frames] [lines] [peak]: main-thread heap events and optional busiest-frame stacks.
// Stack capture distorts timing; use ordinary profiling for performance comparisons.

#include "vr_engine.hpp"
#include "vr_alloccount.hpp"
#include "vr_alloccount.h"
#include <new>
#include <stdint.h>

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
alloccount::Stats started{}, last{}, finished{};
za::U64 peakRequests = 0, peakBytes = 0;
int peakFrame = 0;
int peakHostFrame = 0;
bool showPeak = false;

void report()
{
    za::U64 total = 0, dropped = 0;
    const za::Vector<alloccount::Site> sites = alloccount::traceSites(total, dropped);
    const double frames = static_cast<double>(za::max(framesTraced, 1));
    Con_Printf("vr_alloc_sites: %llu heap events in %d frames (%.2f a frame), %d sites%s\n",
        static_cast<unsigned long long>(total), framesTraced, static_cast<double>(total) / frames,
        static_cast<int>(sites.size()), dropped ? va(" (%llu not placed: the table was full)", static_cast<unsigned long long>(dropped)) : "");
    for(int i = 0; i < 6; ++i)
    {
        const auto kind = static_cast<alloccount::Kind>(i);
        const za::U64 calls = finished.calls[i] - started.calls[i];
        Con_Printf("vr_alloc_sites totals: %-7s %llu (%.3f/frame)\n", alloccount::kindName(kind),
            static_cast<unsigned long long>(calls), static_cast<double>(calls) / frames);
    }
    Con_Printf("vr_alloc_sites requests: %llu bytes; peak %llu requests, %llu bytes at trace frame %d\n",
        static_cast<unsigned long long>(finished.requestedBytes - started.requestedBytes),
        static_cast<unsigned long long>(peakRequests), static_cast<unsigned long long>(peakBytes), peakFrame);
    Con_Printf("vr_alloc_sites peak_host_frame: %d\n", peakHostFrame);
    if(showPeak)
    {
        const auto peakSites = alloccount::traceSites(total, dropped, true);
        for(const auto& s : peakSites)
        {
            Con_Printf("peak_site %-7s %llu bytes=%llu %s\n          <- %s\n", alloccount::kindName(s.kind),
                static_cast<unsigned long long>(s.count), static_cast<unsigned long long>(s.bytes),
                s.where.cStr(), s.via.empty() ? "?" : s.via.cStr());
        }
    }
    int shown = 0;
    for(const alloccount::Site& s : sites)
    {
        if(shown++ >= linesShown)
        {
            break;
        }
        Con_Printf("%8.2f/frame %6llu  %-7s %llu bytes  %s\n                  <- %s\n", static_cast<double>(s.count) / frames,
            static_cast<unsigned long long>(s.count), alloccount::kindName(s.kind),
            static_cast<unsigned long long>(s.bytes), s.where.cStr(), s.via.empty() ? "?" : s.via.cStr());
    }
}

void test_f()
{
    const auto before = alloccount::statsThisThread();
    auto* p = static_cast<unsigned char*>(VR_HeapMalloc(19));
    auto* z = static_cast<unsigned char*>(VR_HeapCalloc(4, 5));
    bool ok = p && z;
    if(p) p[0] = 73;
    if(z) for(int i = 0; i < 20; ++i) ok = ok && z[i] == 0;
    auto* grown = static_cast<unsigned char*>(VR_HeapRealloc(p, 41));
    ok = ok && grown && grown[0] == 73;
    VR_HeapFree(grown ? grown : p);
    VR_HeapFree(z);
    VR_HeapFree(nullptr);
    void* a = VR_HeapAlignedAlloc(32, 64);
    ok = ok && a && reinterpret_cast<uintptr_t>(a) % 64 == 0;
    VR_HeapAlignedFree(a);
    void* n = ::operator new(12);
    ::operator delete(n);
    void* an = ::operator new(16, std::align_val_t{64});
    ok = ok && reinterpret_cast<uintptr_t>(an) % 64 == 0;
    ::operator delete(an, std::align_val_t{64});
    const auto after = alloccount::statsThisThread();
    constexpr za::U64 expected[] = {2, 2, 2, 1, 1, 3};
    for(int i = 0; i < 6; ++i) ok = ok && after.calls[i] - before.calls[i] == expected[i];
    ok = ok && after.requestedBytes - before.requestedBytes == 140;
    Con_Printf("vr_alloc_test: %s (CRT semantics, zeroing, realloc contents, alignment, counters, no double count)\n", ok ? "PASS" : "FAIL");
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
    showPeak = Cmd_Argc() > 3 && Q_atoi(Cmd_Argv(3)) != 0;
    Con_Printf("vr_alloc_sites: tracing the main thread's heap events for %d frames\n", framesTraced);
    started = last = finished = alloccount::statsThisThread();
    peakRequests = peakBytes = 0;
    peakFrame = peakHostFrame = 0;
    alloccount::traceBegin();
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_alloc_sites", command_f);
    Cmd_AddCommand("vr_alloc_test", test_f);
}

void frameEnd()
{
    if(framesLeft <= 0) return;
    const auto now = alloccount::statsThisThread();
    za::U64 requests = 0;
    for(const auto k : {alloccount::Kind::New, alloccount::Kind::Malloc, alloccount::Kind::Calloc, alloccount::Kind::Realloc})
    {
        const int i = static_cast<int>(k);
        requests += now.calls[i] - last.calls[i];
    }
    const bool retainPeak = requests > peakRequests;
    if(retainPeak)
    {
        peakRequests = requests;
        peakBytes = now.requestedBytes - last.requestedBytes;
        peakFrame = framesTraced - framesLeft + 1;
        peakHostFrame = host_framecount;
    }
    alloccount::traceFrameEnd(retainPeak);
    last = finished = now;
    if(--framesLeft > 0)
    {
        return;
    }
    alloccount::traceEnd();
    report();
}

} // namespace qvr::allocsites
