// vr_jobs.cpp -- see vr_jobs.hpp. No engine headers here.

#include "vr_jobs.hpp"

#include "Zancle/Base/CpuRelax.hpp"
#include "Zancle/Concurrency/ThreadPool.hpp"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace qvr::jobs
{
namespace
{

struct Counters
{
    std::atomic<std::size_t> tasks{0}, claimedByWaiter{0}, loops{0}, serialLoops{0}, chunksCaller{0}, chunksHelpers{0},
        helpersCalledOff{0};
};
Counters counters; // every pool's, since start-up (vr_jobs_info)

std::atomic<Pool*> game{nullptr};
std::atomic<bool> parallelOn{true};

constexpr std::uint32_t closed = 0x80000000u;

// A parallelFor's state, shared by the caller and its helpers; reused (a pool's own list). A helper that starts after
// the caller has finished (it was queued behind other work) finds the gate closed and only lets go of it: the caller's
// frame (the body) may be gone by then, this is not.
struct Loop
{
    std::atomic<std::uint32_t> refs{0}; // the caller and the helpers not yet run
    std::atomic<std::uint32_t> gate{0}; // helpers running chunks; `closed` once the caller has run out of chunks
    std::atomic<std::size_t> next{0};   // the next chunk to take
    std::size_t count{0}, chunk{0}, chunks{0};
    const FunctionRef<void(std::size_t, std::size_t)>* body{nullptr}; // the caller's (read only inside the gate)
    std::mutex errorMutex;
    std::size_t errorChunk{SIZE_MAX}; // the lowest chunk that threw, and what
    std::exception_ptr error;
    Pool::Impl* owner{nullptr};
};

// Runs chunks until none is left; returns how many.
std::size_t runChunks(Loop& l)
{
    std::size_t ran = 0;
    for(std::size_t c; (c = l.next.fetch_add(1, std::memory_order_relaxed)) < l.chunks;)
    {
        const std::size_t begin = c * l.chunk;
        try
        {
            (*l.body)(begin, std::min(begin + l.chunk, l.count));
        }
        catch(...)
        {
            const std::lock_guard<std::mutex> lock{l.errorMutex};
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
    std::mutex loopsMutex;
    std::vector<std::unique_ptr<Loop>> loops; // every one made (a few: one per parallelFor under way at once)
    std::vector<Loop*> spare;
    std::optional<za::ThreadPool> threads; // reset first (~Pool): every task run, the workers joined

    Loop* acquire()
    {
        const std::lock_guard<std::mutex> lock{loopsMutex};
        if(spare.empty())
        {
            loops.push_back(std::make_unique<Loop>());
            loops.back()->owner = this;
            return loops.back().get();
        }
        Loop* l = spare.back();
        spare.pop_back();
        return l;
    }

    void release(Loop* l)
    {
        if(l->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            const std::lock_guard<std::mutex> lock{loopsMutex};
            spare.push_back(l);
        }
    }
};

// ----------------------------------------------------------------------------

bool detail::Job::claimAndRun() noexcept
{
    int expected = 0;
    if(!phase.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed))
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
    counters.tasks.fetch_add(1, std::memory_order_relaxed);
    phase.store(2, std::memory_order_release);
    phase.notify_all();
    return true;
}

void detail::Job::wait() noexcept
{
    if(claimAndRun())
    {
        counters.claimedByWaiter.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    for(int p; (p = phase.load(std::memory_order_acquire)) != 2;)
    {
        phase.wait(p, std::memory_order_acquire);
    }
}

// ----------------------------------------------------------------------------

Pool::Pool(int workers) : impl{std::make_unique<Impl>()}
{
    impl->workers = std::max(workers, 1);
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

void Pool::post(std::shared_ptr<detail::Job> job)
{
    impl->threads->post([job = std::move(job)] { job->claimAndRun(); });
}

void Pool::parallelFor(std::size_t count, std::size_t chunk, FunctionRef<void(std::size_t, std::size_t)> body, bool parallel)
{
    if(count == 0)
    {
        return;
    }
    const std::size_t threads = static_cast<std::size_t>(impl->workers) + 1;
    if(chunk == 0)
    {
        chunk = std::max<std::size_t>(count / (threads * 4), 1);
    }
    const std::size_t chunks = (count - 1) / chunk + 1;
    const std::size_t helpers = parallel ? std::min(chunks - 1, threads - 1) : 0;

    if(helpers == 0)
    {
        // The same chunks, in order, on this thread (and the same exception: the lowest chunk's).
        counters.serialLoops.fetch_add(1, std::memory_order_relaxed);
        counters.chunksCaller.fetch_add(chunks, std::memory_order_relaxed);
        std::exception_ptr error;
        for(std::size_t begin = 0; begin < count; begin += chunk)
        {
            try
            {
                body(begin, std::min(begin + chunk, count));
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

    counters.loops.fetch_add(1, std::memory_order_relaxed);
    Loop* l = impl->acquire();
    l->refs.store(static_cast<std::uint32_t>(helpers + 1), std::memory_order_relaxed);
    l->gate.store(0, std::memory_order_relaxed);
    l->next.store(0, std::memory_order_relaxed);
    l->count = count;
    l->chunk = chunk;
    l->chunks = chunks;
    l->body = &body;
    l->errorChunk = SIZE_MAX;
    l->error = nullptr;

    impl->threads->postCopies(
        [l] {
            std::uint32_t g = l->gate.load(std::memory_order_relaxed);
            bool entered = false;
            while(!(g & closed))
            {
                if(l->gate.compare_exchange_weak(g, g + 1, std::memory_order_acquire, std::memory_order_relaxed))
                {
                    entered = true;
                    break;
                }
            }
            if(entered)
            {
                counters.chunksHelpers.fetch_add(runChunks(*l), std::memory_order_relaxed);
                if(l->gate.fetch_sub(1, std::memory_order_release) - 1 == closed)
                {
                    l->gate.notify_all(); // the caller waits for the last one inside
                }
            }
            else
            {
                counters.helpersCalledOff.fetch_add(1, std::memory_order_relaxed);
            }
            l->owner->release(l);
        },
        static_cast<za::SizeT>(helpers));

    counters.chunksCaller.fetch_add(runChunks(*l), std::memory_order_relaxed);

    // No chunk left: helpers not yet in are called off; those running chunks finish them.
    std::uint32_t g = l->gate.fetch_or(closed, std::memory_order_acq_rel) | closed;
    for(int spin = 0; g != closed; spin++)
    {
        if(spin < 2048)
        {
            ZA_CPU_RELAX();
            g = l->gate.load(std::memory_order_acquire);
        }
        else
        {
            l->gate.wait(g, std::memory_order_acquire);
            g = l->gate.load(std::memory_order_acquire);
        }
    }
    const std::exception_ptr error = std::move(l->error);
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
    if(game.load())
    {
        return;
    }
    const int n = workers > 0 ? workers : std::min(static_cast<int>(za::ThreadPool::getHardwareWorkerCountExcludingCallingThread()), 31);
    game.store(new Pool{n});
}

void shutdown()
{
    // Null first: a task still running during the join finds no pool (its parallelFor runs on its own thread).
    delete game.exchange(nullptr);
}

void restart(int workers)
{
    shutdown();
    init(workers);
}

Pool* pool() noexcept
{
    return game.load(std::memory_order_acquire);
}

int workers() noexcept
{
    const Pool* p = pool();
    return p ? p->workers() : 0;
}

void setParallel(bool on) noexcept
{
    parallelOn.store(on, std::memory_order_relaxed);
}

bool parallel() noexcept
{
    return parallelOn.load(std::memory_order_relaxed);
}

Stats stats() noexcept
{
    Stats s;
    s.tasks = counters.tasks.load(std::memory_order_relaxed);
    s.claimedByWaiter = counters.claimedByWaiter.load(std::memory_order_relaxed);
    s.loops = counters.loops.load(std::memory_order_relaxed);
    s.serialLoops = counters.serialLoops.load(std::memory_order_relaxed);
    s.chunksCaller = counters.chunksCaller.load(std::memory_order_relaxed);
    s.chunksHelpers = counters.chunksHelpers.load(std::memory_order_relaxed);
    s.helpersCalledOff = counters.helpersCalledOff.load(std::memory_order_relaxed);
    return s;
}

void parallelFor(std::size_t count, std::size_t chunk, FunctionRef<void(std::size_t, std::size_t)> body)
{
    if(Pool* p = pool())
    {
        p->parallelFor(count, chunk, body, parallel());
        return;
    }
    // No pool (before VR_Init, after VR_Shutdown): the caller alone, as Pool's serial path.
    std::exception_ptr error;
    if(chunk == 0)
    {
        chunk = std::max<std::size_t>(count, 1);
    }
    counters.serialLoops.fetch_add(1, std::memory_order_relaxed);
    for(std::size_t begin = 0; begin < count; begin += chunk)
    {
        try
        {
            body(begin, std::min(begin + chunk, count));
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
