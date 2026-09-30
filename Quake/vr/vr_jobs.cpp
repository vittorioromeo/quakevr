// vr_jobs.cpp -- see vr_jobs.hpp. No engine headers here.

#include "vr_jobs.hpp"

#include "Zancle/Base/CpuRelax.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/ThreadPool.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::jobs
{
namespace
{

struct Counters
{
    za::Atomic<za::SizeT> tasks, claimedByWaiter, loops, serialLoops, chunksCaller, chunksHelpers, helpersCalledOff;
};
Counters counters; // every pool's, since start-up (vr_jobs_info)

za::Atomic<Pool*> game{nullptr};
za::Atomic<bool> parallelOn{true};

using MO = za::MemoryOrder;
constexpr za::U32 closed = 0x80000000u;
constexpr za::SizeT noChunk = static_cast<za::SizeT>(-1);

// A parallelFor's state, shared by the caller and its helpers; reused (a pool's own list). A helper that starts after
// the caller has finished (it was queued behind other work) finds the gate closed and only lets go of it: the caller's
// frame (the body) may be gone by then, this is not.
struct Loop
{
    za::Atomic<za::U32> refs;  // the caller and the helpers not yet run
    za::Atomic<za::U32> gate;  // helpers running chunks; `closed` once the caller has run out of chunks
    za::Atomic<za::SizeT> next; // the next chunk to take
    za::SizeT count{0}, chunk{0}, chunks{0};
    const FunctionRef<void(za::SizeT, za::SizeT)>* body{nullptr}; // the caller's (read only inside the gate)
    za::AtomicMutex errorMutex;
    za::SizeT errorChunk{noChunk}; // the lowest chunk that threw, and what
    std::exception_ptr error;      // ZANCLE-TODO: no exception transport
    Pool::Impl* owner{nullptr};
};

// Runs chunks until none is left; returns how many.
za::SizeT runChunks(Loop& l)
{
    za::SizeT ran = 0;
    for(za::SizeT c; (c = l.next.fetchAddRelaxed(1u)) < l.chunks;)
    {
        const za::SizeT begin = c * l.chunk;
        try
        {
            (*l.body)(begin, za::min(begin + l.chunk, l.count));
        }
        catch(...)
        {
            const za::LockGuard lock{l.errorMutex};
            if(c < l.errorChunk)
            {
                l.errorChunk = c;
                l.error = std::current_exception();
            }
        }
        ran++;
    }
    return ran;
}

} // namespace

struct Pool::Impl
{
    int workers{1};
    za::AtomicMutex loopsMutex;
    za::Vector<za::UniquePtr<Loop>> loops; // every one made (a few: one per parallelFor under way at once)
    za::Vector<Loop*> spare;
    za::Optional<za::ThreadPool> threads; // reset first (~Pool): every task run, the workers joined

    Loop* acquire()
    {
        const za::LockGuard lock{loopsMutex};
        if(spare.empty())
        {
            loops.pushBack(za::makeUnique<Loop>());
            loops.back()->owner = this;
            return loops.back().get();
        }
        Loop* l = spare.back();
        spare.popBack();
        return l;
    }

    void release(Loop* l)
    {
        if(l->refs.fetchSubAcqRel(1u) == 1u)
        {
            const za::LockGuard lock{loopsMutex};
            spare.pushBack(l);
        }
    }
};

// ----------------------------------------------------------------------------

bool detail::Job::claimAndRun() noexcept
{
    int expected = 0;
    if(!phase.compareExchangeStrong<MO::Acquire, MO::Relaxed>(expected, 1))
    {
        return false;
    }
    try
    {
        execute();
    }
    catch(...)
    {
        error = std::current_exception();
    }
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
    impl->threads.reset(); // (the helpers still queued let go of their loops here)
}

int Pool::workers() const noexcept
{
    return impl->workers;
}

void Pool::post(detail::JobPtr<detail::Job> job)
{
    impl->threads->post([job = ZA_MOVE(job)] { job->claimAndRun(); });
}

void Pool::parallelFor(za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body, bool parallel)
{
    if(count == 0)
    {
        return;
    }
    const za::SizeT threads = static_cast<za::SizeT>(impl->workers) + 1;
    if(chunk == 0)
    {
        chunk = za::max<za::SizeT>(count / (threads * 4), 1);
    }
    const za::SizeT chunks = (count - 1) / chunk + 1;
    const za::SizeT helpers = parallel ? za::min(chunks - 1, threads - 1) : 0;

    if(helpers == 0)
    {
        // The same chunks, in order, on this thread (and the same exception: the lowest chunk's).
        counters.serialLoops.fetchAddRelaxed(1u);
        counters.chunksCaller.fetchAddRelaxed(chunks);
        std::exception_ptr error; // ZANCLE-TODO: no exception transport
        for(za::SizeT begin = 0; begin < count; begin += chunk)
        {
            try
            {
                body(begin, za::min(begin + chunk, count));
            }
            catch(...)
            {
                if(!error)
                {
                    error = std::current_exception();
                }
            }
        }
        if(error)
        {
            std::rethrow_exception(error);
        }
        return;
    }

    counters.loops.fetchAddRelaxed(1u);
    Loop* l = impl->acquire();
    l->refs.storeRelaxed(static_cast<za::U32>(helpers + 1));
    l->gate.storeRelaxed(0u);
    l->next.storeRelaxed(0u);
    l->count = count;
    l->chunk = chunk;
    l->chunks = chunks;
    l->body = &body;
    l->errorChunk = noChunk;
    l->error = nullptr;

    impl->threads->postCopies(
        [l] {
            za::U32 g = l->gate.loadRelaxed();
            bool entered = false;
            while(!(g & closed))
            {
                if(l->gate.compareExchangeWeak<MO::Acquire, MO::Relaxed>(g, g + 1))
                {
                    entered = true;
                    break;
                }
            }
            if(entered)
            {
                counters.chunksHelpers.fetchAddRelaxed(runChunks(*l));
                if(l->gate.fetchSubRelease(1u) - 1 == closed)
                {
                    l->gate.notifyAll(); // the caller waits for the last one inside
                }
            }
            else
            {
                counters.helpersCalledOff.fetchAddRelaxed(1u);
            }
            l->owner->release(l);
        },
        static_cast<za::SizeT>(helpers));

    counters.chunksCaller.fetchAddRelaxed(runChunks(*l));

    // No chunk left: helpers not yet in are called off; those running chunks finish them.
    za::U32 g = l->gate.fetchOrAcqRel(closed) | closed;
    for(int spin = 0; g != closed; spin++)
    {
        if(spin < 2048)
        {
            ZA_CPU_RELAX();
            g = l->gate.loadAcquire();
        }
        else
        {
            l->gate.waitOnceAcquire(g);
            g = l->gate.loadAcquire();
        }
    }
    const std::exception_ptr error = ZA_MOVE(l->error); // ZANCLE-TODO: no exception transport
    l->error = nullptr;
    impl->release(l);
    if(error)
    {
        std::rethrow_exception(error);
    }
}

// ----------------------------------------------------------------------------

int hardwareThreads() noexcept
{
    return static_cast<int>(za::ThreadPool::getHardwareWorkerCount());
}

void init(int workers)
{
    if(game.loadSeqCst())
    {
        return;
    }
    const int n = workers > 0 ? workers : za::min(static_cast<int>(za::ThreadPool::getHardwareWorkerCountExcludingCallingThread()), 31);
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
    s.helpersCalledOff = counters.helpersCalledOff.loadRelaxed();
    return s;
}

void parallelFor(za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body)
{
    if(Pool* p = pool())
    {
        p->parallelFor(count, chunk, body, parallel());
        return;
    }
    // No pool (before VR_Init, after VR_Shutdown): the caller alone, as Pool's serial path.
    std::exception_ptr error; // ZANCLE-TODO: no exception transport
    if(chunk == 0)
    {
        chunk = za::max<za::SizeT>(count, 1);
    }
    counters.serialLoops.fetchAddRelaxed(1u);
    for(za::SizeT begin = 0; begin < count; begin += chunk)
    {
        try
        {
            body(begin, za::min(begin + chunk, count));
        }
        catch(...)
        {
            if(!error)
            {
                error = std::current_exception();
            }
        }
    }
    if(error)
    {
        std::rethrow_exception(error);
    }
}

} // namespace qvr::jobs
