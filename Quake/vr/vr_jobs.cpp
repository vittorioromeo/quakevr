// vr_jobs.cpp -- see vr_jobs.hpp. No engine headers here.

#include "vr_jobs.hpp"

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
    execute();
    counters.tasks.fetchAddRelaxed(1u);
    phase.storeRelease(2);
    phase.notifyAll();
    return true;
}

void detail::Job::wait() noexcept
{
    if(claimAndRun())
    {
        counters.claimedByWaiter.fetchAddRelaxed(1u);
        return;
    }
    for(int p; (p = phase.loadAcquire()) != 2;)
    {
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

void Pool::post(detail::JobPtr<detail::Job> job)
{
    impl->threads->post([job = ZA_MOVE(job)] { job->claimAndRun(); });
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

void Task::post(Pool& pool, void (*fn)(void*), void* context)
{
    ZA_ASSERT(job->phase.loadAcquire() == 2);
    job->fn = fn;
    job->context = context;
    job->phase.storeRelease(0);
    pool.post(detail::JobPtr<detail::Job>{job});
}

void Task::wait() noexcept
{
    job->wait();
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
    za::SizeT mine = 0;
    za::parallelFor(
        *impl->threads, impl->loops, count,
        [&](za::SizeT begin, za::SizeT end) {
            body(begin, end);
            if(za::ThisThread::getId() == caller)
            {
                mine++;
            }
        },
        chunk);
    ZA_ASSERT(mine <= chunks);
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
