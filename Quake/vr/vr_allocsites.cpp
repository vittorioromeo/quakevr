// vr_alloc_sites [frames] [lines] [peak]: main-thread heap events and optional busiest-frame stacks.
// Stack capture distorts timing; use ordinary profiling for performance comparisons.

// vr_heap [stats | collect | test | stress [threads] [ms]]: the process's C heap (mimalloc, vr_crtheap.c).

#include "vr_engine.hpp"
#include "vr_alloccount.hpp"
#include "vr_alloccount.h"
#include "vr_crtheap.h"
#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#endif

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
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

// ---- vr_heap ------------------------------------------------------------------------------------------------------

void heapSummary()
{
    const VR_CrtHeapStats_t s = VR_CrtHeapStats();
    Con_Printf("vr_heap: %s; %llu calls on blocks the C runtime's own functions allocated\n", VR_CrtHeapName(),
        VR_CrtHeapForeignCalls());
    if(VR_CrtHeapIsMimalloc())
    {
        Con_Printf("vr_heap: process working set %.1f MB (peak %.1f), commit %.1f MB (peak %.1f); mimalloc reserved %.1f MB, "
                   "committed %.1f MB (peak %.1f), %lld threads, %lld arenas\n",
            s.rssMb, s.peakRssMb, s.commitMb, s.peakCommitMb, s.reservedMb, s.committedMb, s.peakCommittedMb, s.threads,
            s.arenas);
    }
}

// Every way the code allocates gives mimalloc's blocks (or, with it off, none does), and a block the C runtime allocated
// itself goes back to it.
void heapTest()
{
    const bool mi = VR_CrtHeapIsMimalloc();
    bool ok = true;
    int checks = 0;
    const auto expect = [&](const void* p, const char* what) {
        checks++;
        if(!p || (VR_CrtHeapOwns(p) != 0) != mi)
        {
            ok = false;
            Con_Printf("vr_heap test: %s: %p %s\n", what, p, p ? (mi ? "not mimalloc's" : "mimalloc's") : "null");
        }
    };
    void* m = malloc(24);
    expect(m, "malloc");
    void* c = calloc(3, 40);
    expect(c, "calloc");
    for(int i = 0; c && i < 120; i++) ok = ok && static_cast<unsigned char*>(c)[i] == 0;
    if(m) memcpy(m, "mimalloc", 9);
    m = realloc(m, 5000);
    expect(m, "realloc");
    ok = ok && m && memcmp(m, "mimalloc", 9) == 0;
    ok = ok && realloc(c, 0) == nullptr; // frees it (the C runtime's semantics)
    char* d = strdup("quake");
    expect(d, "strdup");
    ok = ok && d && strcmp(d, "quake") == 0;
    void* hm = VR_HeapMalloc(100);
    expect(hm, "VR_HeapMalloc");
    int* n = new int(7);
    expect(n, "new");
    double* na = new double[33];
    expect(na, "new[]");
    void* an = ::operator new(48, std::align_val_t{256});
    expect(an, "aligned new");
    ok = ok && reinterpret_cast<uintptr_t>(an) % 256 == 0;
#ifdef _WIN32
    void* am = _aligned_malloc(100, 128);
    expect(am, "_aligned_malloc");
    ok = ok && reinterpret_cast<uintptr_t>(am) % 128 == 0;
    am = _aligned_realloc(am, 3000, 128);
    expect(am, "_aligned_realloc");
    ok = ok && reinterpret_cast<uintptr_t>(am) % 128 == 0;
    ok = ok && m && _msize(m) >= 5000;
    _aligned_free(am);
    // The C runtime's own allocation (_fullpath(NULL)): never mimalloc's; freed by it through the foreign path.
    const unsigned long long foreignBefore = VR_CrtHeapForeignCalls();
    char* full = _fullpath(nullptr, ".", 0);
    checks++;
    ok = ok && full && !VR_CrtHeapOwns(full);
    char* fullGrown = static_cast<char*>(realloc(full, 4096)); // stays in the C runtime's heap
    ok = ok && fullGrown && !VR_CrtHeapOwns(fullGrown);
    free(fullGrown ? fullGrown : full);
    const unsigned long long foreign = VR_CrtHeapForeignCalls() - foreignBefore;
    ok = ok && (!mi || foreign >= 2); // (other threads may add theirs)
#endif
    free(m);
    free(d);
    VR_HeapFree(hm);
    delete n;
    delete[] na;
    ::operator delete(an, std::align_val_t{256});
    free(nullptr);
    Con_Printf("vr_heap test: %s (%d kinds of block %s mimalloc's; contents, zeroing, alignment, realloc(p, 0), the C "
               "runtime's own blocks freed by it)\n", ok ? "PASS" : "FAIL", checks, mi ? "all" : "none");
}

// Threads allocating and freeing at once (malloc, calloc, realloc, new, aligned), a quarter of the blocks freed by
// another thread; each block's contents checked before it goes. Million operations a second: the heap's contention.
struct HeapStressShared
{
    za::AtomicMutex lock;
    za::Vector<void*> handoff; // blocks one thread made and another frees (malloc'd)
    za::Atomic<bool> stop{false};
    za::Atomic<za::U64> ops{0}, errors{0};
};

void heapStressThread(HeapStressShared& shared, za::U32 seed)
{
    constexpr int slots = 256;
    struct Slot
    {
        unsigned char* p;
        za::U32 size;
        za::U8 tag, kind;
    };
    Slot held[slots] = {};
    za::U32 x = seed * 2654435761u + 1;
    const auto next = [&] {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return x;
    };
    za::U64 ops = 0, errors = 0;
    const auto release = [&](Slot& s) {
        if(s.p[0] != s.tag || s.p[s.size - 1] != s.tag) errors++;
        if(s.kind == 2) delete[] s.p;
        else if(s.kind == 3) ::operator delete(s.p, std::align_val_t{64});
        else free(s.p);
        s.p = nullptr;
    };
    while(!shared.stop.loadRelaxed())
    {
        for(int burst = 0; burst < 256; burst++, ops++)
        {
            Slot& s = held[next() % slots];
            if(s.p)
            {
                const za::U32 r = next();
                if(s.kind == 0 && r % 8 == 0) // grown or shrunk in place of a free
                {
                    const za::U32 size = 1 + next() % 2048;
                    auto* p = static_cast<unsigned char*>(realloc(s.p, size));
                    if(!p || p[0] != s.tag) errors++;
                    s.p = p;
                    s.size = size;
                    memset(p, s.tag, size);
                }
                else if(s.kind <= 1 && r % 4 == 1) // to another thread
                {
                    if(s.p[0] != s.tag || s.p[s.size - 1] != s.tag) errors++;
                    za::LockGuard guard{shared.lock};
                    shared.handoff.emplaceBack(s.p);
                    s.p = nullptr;
                }
                else
                {
                    release(s);
                }
                continue;
            }
            const za::U32 r = next();
            const za::U32 size = r % 64 == 0 ? 4096 + next() % 200000 : 1 + next() % 512;
            s.kind = static_cast<za::U8>(r % 4);
            s.tag = static_cast<za::U8>(next() | 1);
            s.size = size;
            s.p = s.kind == 0   ? static_cast<unsigned char*>(malloc(size))
                  : s.kind == 1 ? static_cast<unsigned char*>(calloc(1, size))
                  : s.kind == 2 ? new unsigned char[size]
                                : static_cast<unsigned char*>(::operator new(size, std::align_val_t{64}));
            if(s.kind == 1 && (s.p[0] != 0 || s.p[size - 1] != 0)) errors++;
            memset(s.p, s.tag, size);
        }
        // Another thread's blocks freed here.
        void* theirs[64];
        int taken = 0;
        {
            za::LockGuard guard{shared.lock};
            while(taken < 64 && !shared.handoff.empty())
            {
                theirs[taken++] = shared.handoff.back();
                shared.handoff.popBack();
            }
        }
        for(int i = 0; i < taken; i++) free(theirs[i]);
    }
    for(Slot& s : held)
    {
        if(s.p) release(s);
    }
    shared.ops.fetchAddRelaxed(ops);
    shared.errors.fetchAddRelaxed(errors);
}

void heapStress(int threads, int ms)
{
    HeapStressShared shared;
    za::Vector<za::Thread> workers;
    const za::Clock clock;
    for(int t = 0; t < threads; t++)
    {
        workers.emplaceBack([&shared, t] { heapStressThread(shared, static_cast<za::U32>(t + 1)); });
    }
    while(clock.getElapsedTime().asMilliseconds() < ms)
    {
        Sys_Sleep(5);
    }
    shared.stop.storeRelease(true);
    for(za::Thread& w : workers) w.join();
    const double seconds = static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 1e6;
    for(void* p : shared.handoff) free(p);
    const za::U64 ops = shared.ops.loadRelaxed(), errors = shared.errors.loadRelaxed();
    Con_Printf("vr_heap stress: %s, %d threads, %.2f s: %.2f million operations a second (%llu), %llu bad blocks: %s\n",
        VR_CrtHeapName(), threads, seconds, static_cast<double>(ops) / seconds / 1e6, static_cast<unsigned long long>(ops),
        static_cast<unsigned long long>(errors), errors ? "FAIL" : "PASS");
}

void heapPrint(const char* text, void*)
{
    Con_Printf("%s", text);
}

void heap_f()
{
    const char* const what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(what, "stats"))
    {
        VR_CrtHeapPrintStats(heapPrint, nullptr);
    }
    else if(!q_strcasecmp(what, "collect"))
    {
        VR_CrtHeapCollect();
    }
    else if(!q_strcasecmp(what, "test"))
    {
        heapTest();
        return;
    }
    else if(!q_strcasecmp(what, "stress"))
    {
        heapStress(za::clamp(Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 8, 1, 64),
            za::clamp(Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 1000, 10, 60000));
        return;
    }
    else if(what[0])
    {
        Con_Printf("vr_heap [stats | collect | test | stress [threads] [ms]]\n");
        return;
    }
    heapSummary();
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
    Cmd_AddCommand("vr_heap", heap_f);
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
