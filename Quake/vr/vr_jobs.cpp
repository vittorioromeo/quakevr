// vr_jobs.cpp -- see vr_jobs.hpp. No engine headers here.

#include "vr_jobs.hpp"
#include "vr_crtheap.h"
#include "vr_tracy.h"
#ifdef QVR_TRACY
#include "Zancle/Concurrency/Atomic.hpp"
namespace { za::Atomic<unsigned> tracyQueued{0}; }
#endif

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/ParallelFor.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Concurrency/ThreadPool.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h> // (snprintf: describeNow, the stall line)

namespace qvr::jobs
{
namespace
{

struct Counters
{
    za::Atomic<za::SizeT> tasks, claimedByWaiter, loops, serialLoops, chunksCaller, chunksHelpers;
};
Counters counters; // every pool's, since start-up (vr_jobs_info)

za::Atomic<Pool*> game{nullptr};
za::Atomic<bool> parallelOn{true};

using MO = za::MemoryOrder;

// What each worker runs now (describeNow): where its job was posted (line > 0) or its parallelFor's site (line 0),
// and since when. A worker takes its slot with its first job (the pool's workers, and a thread that ran queued jobs
// while the pool was destroyed); past maxBusy, none.
constexpr int maxBusy = 64;
struct Busy
{
    za::Atomic<const char*> what{nullptr};
    za::Atomic<unsigned> line{0};
    za::Atomic<za::I64> since{0};
};
Busy busy[maxBusy];
za::Atomic<int> busySlots{0};
thread_local int busySlot = -1;
za::Atomic<za::SizeT> queuedTasks{0}; // posted with Pool::post and not started
za::Atomic<void (*)(const char*)> stallReport{nullptr};

// Marks this worker busy with `what` while it lives (then puts back what it was busy with: a nested call's chunk).
class BusyScope
{
public:
    BusyScope(const char* what, unsigned line) noexcept
    {
        if(busySlot < 0)
        {
            busySlot = busySlots.fetchAddRelaxed(1);
        }
        if(busySlot >= maxBusy)
        {
            return;
        }
        b = &busy[busySlot];
        prevWhat = b->what.loadRelaxed();
        prevLine = b->line.loadRelaxed();
        prevSince = b->since.loadRelaxed();
        b->since.storeRelaxed(za::Clock::nowNanoseconds());
        b->line.storeRelaxed(line);
        b->what.storeRelease(what ? what : "?");
    }
    ~BusyScope()
    {
        if(b)
        {
            b->since.storeRelaxed(prevSince);
            b->line.storeRelaxed(prevLine);
            b->what.storeRelease(prevWhat);
        }
    }
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;

private:
    Busy* b{nullptr};
    const char* prevWhat{nullptr};
    unsigned prevLine{0};
    za::I64 prevSince{0};
};

} // namespace

struct Pool::Impl
{
    int workers{1};
    za::ParallelForSlots loops;           // parallelFor's gates (Zancle's: a helper that starts late finds its gate shut)
    za::Optional<za::ThreadPool> threads; // reset first (~Pool): every task run, the workers joined
};

// ----------------------------------------------------------------------------

bool detail::Job::claimAndRun() noexcept
{
    int expected = 0;
    if(!phase.compareExchangeStrong<MO::Acquire, MO::Relaxed>(expected, 1))
    {
        return false;
    }
    QVR_TRACY_SCOPE(scope, "Job execute");
#ifdef QVR_TRACY
    if(file)
    {
        VR_TracyJobText(tracyFrom, file, line); // where it was posted (the viewer groups by it: Find Zone)
    }
#endif
    execute();
    counters.tasks.fetchAddRelaxed(1u);
    phase.storeRelease(2);
    phase.notifyAll();
    return true;
}

void detail::Job::wait() noexcept
{
    QVR_TRACY_SCOPE(scope, "Job wait or claim");
    if(claimAndRun())
    {
        counters.claimedByWaiter.fetchAddRelaxed(1u);
        return;
    }
    for(int p; (p = phase.loadAcquire()) != 2;)
    {
        QVR_TRACY_SCOPE(scope, "Job blocking wait");
        phase.waitOnceAcquire(p);
    }
}

// ----------------------------------------------------------------------------

Pool::Pool(int workers) : impl{za::makeUnique<Impl>()}
{
    impl->workers = za::max(workers, 1);
    impl->threads.emplace(static_cast<za::SizeT>(impl->workers));
}

Pool::~Pool()
{
    impl->threads.reset(); // (the helpers still queued run here: they find their gates shut; then the gates go)
}

int Pool::workers() const noexcept
{
    return impl->workers;
}

za::SizeT Pool::helpersQueued() const noexcept
{
    return static_cast<za::SizeT>(impl->loops.getOutstandingHelpers());
}

za::SizeT Pool::gatesInUse() const noexcept
{
    return impl->loops.getSlotsInUse();
}

void Pool::post(detail::JobPtr<detail::Job> job)
{
    QVR_TRACY_SCOPE(scope, "Job enqueue");
#ifdef QVR_TRACY
    VR_TracyPlot("Job queue entries", tracyQueued.fetchAddRelaxed(1) + 1);
#endif
    queuedTasks.fetchAddRelaxed(1u);
    impl->threads->post([job = ZA_MOVE(job)] {
        VR_TracyThreadName("Quake VR worker");
#ifdef QVR_TRACY
        VR_TracyPlot("Job queue entries", tracyQueued.fetchSubRelaxed(1) - 1);
#endif
        queuedTasks.fetchSubRelaxed(1u);
        const BusyScope busyNow{job->file, job->line};
        job->claimAndRun();
        VR_CrtHeapThreadTidy(); // (the worker's own heap, at most once a vr_heap_thread_tidy: vr_crtheap.h)
    });
}

// ----------------------------------------------------------------------------

// The Task's job, kept by the Task's own reference (never deleted by the pool). A post whose job the waiter ran leaves its
// pool task queued: it finds the job claimed and does nothing, or, if the Task was posted again meanwhile, runs that
// post (the later pool task then finds it claimed): one run a post either way.
struct Task::Job final : detail::Job
{
    void (*fn)(void*){nullptr};
    void* context{nullptr};

    void execute() noexcept override
    {
        fn(context);
    }
};

Task::Task() : job{new Job}
{
    job->phase.storeRelaxed(2); // (idle: nothing posted)
}

Task::~Task()
{
    wait();
}

void detail::Job::note(const char* fileName, unsigned lineNumber) noexcept
{
#ifdef QVR_TRACY
    tracyFrom = VR_TracyCurrentName();
#endif
    file = fileName;
    line = lineNumber;
}

void Task::post(Pool& pool, void (*fn)(void*), void* context, const za::SourceLocation where)
{
    ZA_ASSERT(job->phase.loadAcquire() == 2);
    QVR_JOBS_NOTE(job);
    job->fn = fn;
    job->context = context;
    job->phase.storeRelease(0);
    pool.post(detail::JobPtr<detail::Job>{job});
}

void Task::wait() noexcept
{
    job->wait();
}

bool Task::tryRun() noexcept
{
    if(!job->claimAndRun())
    {
        return false;
    }
    counters.claimedByWaiter.fetchAddRelaxed(1u);
    return true;
}

// ----------------------------------------------------------------------------

namespace
{

// What a call adds to its site's counters (vr_jobs_sites).
void record(Site* site, za::SizeT items, bool split, za::SizeT caller, za::SizeT helpers, za::I64 since)
{
    if(!site)
    {
        return;
    }
    const za::U64 ns = static_cast<za::U64>(za::max<za::I64>(za::Clock::nowNanoseconds() - since, 0));
    site->calls.fetchAddRelaxed(1u);
    site->split.fetchAddRelaxed(split ? 1u : 0u);
    site->items.fetchAddRelaxed(items);
    site->chunksCaller.fetchAddRelaxed(caller);
    site->chunksHelpers.fetchAddRelaxed(helpers);
    site->nanoseconds.fetchAddRelaxed(ns);
    for(za::U64 worst = site->worstNanoseconds.loadRelaxed();
        ns > worst && !site->worstNanoseconds.compareExchangeWeak<MO::Relaxed, MO::Relaxed>(worst, ns);)
    {
    }
}

Site* siteList = nullptr; // (constant-initialised: every Site's constructor, at static initialisation, finds it)

constexpr za::I64 stallNanoseconds = 1'000'000'000; // a split call's caller alone this long: stallReport

// The stall line (stallReport): the call's caller has run alone over a second (`during`), or the call has ended.
void reportStall(const char* name, za::SizeT mine, za::SizeT chunks, za::I64 start, za::SizeT gatesAtStart, bool during) noexcept
{
    void (*const report)(const char*) = stallReport.loadAcquire();
    if(!report)
    {
        return;
    }
    char now[1024];
    char line[1280];
    const double s = static_cast<double>(za::Clock::nowNanoseconds() - start) * 1e-9;
    if(during)
    {
        describeNow(now, sizeof(now));
        snprintf(line, sizeof(line),
            "parallelFor \"%s\": %.1f s with no helper (%zu of %zu chunks run by its caller; %zu of 64 gates in use at its "
            "start); %s",
            name, s, mine, chunks, gatesAtStart, now);
    }
    else
    {
        snprintf(line, sizeof(line), "parallelFor \"%s\": ended after %.1f s, %zu of %zu chunks by helpers", name, s,
            chunks - mine, chunks);
    }
    report(line);
}

} // namespace

Site::Site(const char* name) noexcept : name{name}, next{siteList}
{
    siteList = this;
}

Site* sites() noexcept
{
    return siteList;
}

void Pool::parallelFor(za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body, bool parallel,
    Site* site) noexcept
{
    QVR_TRACY_SCOPE(scope, "Job parallel dispatch and join");
    if(site) VR_TracyText(site->name);
    if(count == 0)
    {
        return;
    }
    const za::I64 since = site ? za::Clock::nowNanoseconds() : 0;
    const za::SizeT threads = static_cast<za::SizeT>(impl->workers) + 1;
    if(chunk == 0)
    {
        chunk = za::max<za::SizeT>(count / (threads * 4), 1); // (as za::parallelFor's own default)
    }
    const za::SizeT chunks = (count - 1) / chunk + 1;

    if(!parallel || chunks == 1)
    {
        // The same chunks, in order, on this thread.
        counters.serialLoops.fetchAddRelaxed(1u);
        counters.chunksCaller.fetchAddRelaxed(chunks);
        QVR_TRACY_SCOPE(serialScope, "Job serial chunks"); // (the work itself, apart from the dispatch's self time)
        for(za::SizeT begin = 0; begin < count; begin += chunk)
        {
            body(begin, za::min(begin + chunk, count));
        }
        record(site, count, false, chunks, 0, since);
        return;
    }

    // Zancle's (B4, upstream bef08e826): the caller takes chunks too and, once none is left, waits only for the helpers
    // running one; a helper that starts later (queued behind other work) finds its gate shut and returns. The caller
    // never runs anyone else's task. With every gate taken (64 calls under way at once), the caller runs it alone.
    // The helpers' chunks are the rest: every chunk runs exactly once, and za::parallelFor makes `chunks` of them (the
    // same (count - 1) / chunk + 1 for the explicit chunk passed; with every gate taken the caller runs all of them).
    // Only the caller writes `mine` (no atomic per chunk: most calls have chunk 1); a helper only reads the id.
    counters.loops.fetchAddRelaxed(1u);
    const za::ThreadId caller = za::ThisThread::getId();
    const char* const name = site ? site->name : "parallelFor";
    const za::I64 start = site ? since : za::Clock::nowNanoseconds();
    za::SizeT mine = 0;
    za::Atomic<bool> helped{false}; // (a helper ran a chunk)
    bool stalled = false;           // (the caller said it ran alone over a second: stallReport)
    const za::SizeT gatesAtStart = impl->loops.getSlotsInUse();
    za::parallelFor(
        *impl->threads, impl->loops, count,
        [&](za::SizeT begin, za::SizeT end) {
            VR_TracyThreadName("Quake VR worker");
            QVR_TRACY_SCOPE(scope, "Job parallel chunk");
            VR_TracyText(name);
            if(za::ThisThread::getId() == caller)
            {
                body(begin, end);
                mine++;
                if(!stalled && !helped.loadRelaxed() && za::Clock::nowNanoseconds() - start > stallNanoseconds)
                {
                    stalled = true;
                    reportStall(name, mine, chunks, start, gatesAtStart, true);
                }
            }
            else
            {
                {
                    const BusyScope busyNow{name, 0};
                    helped.storeRelaxed(true);
                    body(begin, end);
                }
                VR_CrtHeapThreadTidy(); // (a helper's own heap, at most once a vr_heap_thread_tidy)
            }
        },
        chunk);
    ZA_ASSERT(mine <= chunks);
    if(stalled)
    {
        reportStall(name, mine, chunks, start, gatesAtStart, false);
    }
    counters.chunksCaller.fetchAddRelaxed(mine);
    counters.chunksHelpers.fetchAddRelaxed(chunks - mine);
    record(site, count, true, mine, chunks - mine, since);
}

// ----------------------------------------------------------------------------

int hardwareThreads() noexcept
{
    return static_cast<int>(za::ThreadPool::getOptimalThreadCount());
}

void init(int workers)
{
    if(game.loadSeqCst())
    {
        return;
    }
    const int n = workers > 0 ? workers : za::min(static_cast<int>(za::ThreadPool::getOptimalWorkerCount()), 31);
    setHeapTidy(VR_CrtHeapThreadTidyPeriod()); // (before the workers start: they take the idle hook from the first wait)
    game.storeSeqCst(new Pool{n});
}

void shutdown()
{
    // Null first: a task still running during the join finds no pool (its parallelFor runs on its own thread).
    delete game.exchangeSeqCst(nullptr);
}

void restart(int workers)
{
    shutdown();
    init(workers);
}

void setHeapTidy(int milliseconds)
{
    VR_CrtHeapSetThreadTidy(milliseconds);
    za::ThreadPool::setIdleHook(milliseconds > 0 ? &VR_CrtHeapIdleTidy : nullptr, milliseconds);
}

Pool* pool() noexcept
{
    return game.loadAcquire();
}

int workers() noexcept
{
    const Pool* p = pool();
    return p ? p->workers() : 0;
}

void setParallel(bool on) noexcept
{
    parallelOn.storeRelaxed(on);
}

const char* describeNow(char* out, za::SizeT size) noexcept
{
    const Pool* p = pool();
    const za::I64 now = za::Clock::nowNanoseconds();
    // The busy workers grouped by what they run (where a job was posted is its file's name and line).
    struct Group
    {
        const char* what;
        unsigned line;
        int count;
        za::I64 longest;
    };
    Group groups[maxBusy];
    int nGroups = 0, nBusy = 0;
    const int slots = za::min(busySlots.loadRelaxed(), maxBusy);
    for(int i = 0; i < slots; i++)
    {
        const char* what = busy[i].what.loadAcquire();
        if(!what)
        {
            continue;
        }
        const unsigned line = busy[i].line.loadRelaxed();
        const za::I64 took = now - busy[i].since.loadRelaxed();
        nBusy++;
        int g = 0;
        while(g < nGroups && !(groups[g].what == what && groups[g].line == line))
        {
            g++;
        }
        if(g == nGroups)
        {
            groups[nGroups++] = Group{what, line, 0, 0};
        }
        groups[g].count++;
        groups[g].longest = za::max(groups[g].longest, took);
    }
    za::SizeT used = static_cast<za::SizeT>(snprintf(out, size, "pool: %zu tasks and %zu helpers queued, %zu of 64 gates, %d of %d workers busy",
        queuedTasks.loadRelaxed(), p ? p->helpersQueued() : 0, p ? p->gatesInUse() : 0, nBusy, p ? p->workers() : 0));
    for(int g = 0; g < nGroups && used < size; g++)
    {
        const char* what = groups[g].what;
        for(const char* c = what; *c; c++)
        {
            if(*c == '/' || *c == '\\')
            {
                what = c + 1; // (a file's name without its folders)
            }
        }
        char at[16] = "";
        if(groups[g].line)
        {
            snprintf(at, sizeof(at), ":%u", groups[g].line);
        }
        used += static_cast<za::SizeT>(snprintf(out + used, size - used, "%s %s%s x%d (%.1f s)", g ? "," : ":", what, at,
            groups[g].count, static_cast<double>(groups[g].longest) * 1e-9));
    }
    return out;
}

void setStallReport(void (*report)(const char* line)) noexcept
{
    stallReport.storeRelease(report);
}

bool parallel() noexcept
{
    return parallelOn.loadRelaxed();
}

Stats stats() noexcept
{
    Stats s;
    s.tasks = counters.tasks.loadRelaxed();
    s.claimedByWaiter = counters.claimedByWaiter.loadRelaxed();
    s.loops = counters.loops.loadRelaxed();
    s.serialLoops = counters.serialLoops.loadRelaxed();
    s.chunksCaller = counters.chunksCaller.loadRelaxed();
    s.chunksHelpers = counters.chunksHelpers.loadRelaxed();
    return s;
}

void parallelFor(Site& site, za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body, bool split) noexcept
{
    if(Pool* p = pool())
    {
        p->parallelFor(count, chunk, body, split && parallel(), &site);
        return;
    }
    // No pool (before VR_Init, after VR_Shutdown): the caller alone, as Pool's serial path.
    if(count == 0)
    {
        return;
    }
    const za::I64 since = za::Clock::nowNanoseconds();
    if(chunk == 0)
    {
        chunk = count;
    }
    counters.serialLoops.fetchAddRelaxed(1u);
    for(za::SizeT begin = 0; begin < count; begin += chunk)
    {
        body(begin, za::min(begin + chunk, count));
    }
    record(&site, count, false, (count - 1) / chunk + 1, 0, since);
}

} // namespace qvr::jobs
