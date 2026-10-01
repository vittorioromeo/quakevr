#pragma once

// vr_jobs.hpp -- the game's thread pool: Zancle's (Quake/vr/external/zancle, its README), made at start-up (VR_Init)
// and joined at shutdown (VR_Shutdown, in a dedicated server too).
//
// Two ways in, both from any thread:
// - parallelFor(count, chunk, body): body(begin, end) over [0, count) in chunks; the calling thread takes chunks too and
//   returns once every chunk ran. Only workers that are free help: a helper still queued when the caller has run out of
//   chunks is called off, so the caller never waits for a task that has not started, and never runs anyone else's
//   (a long bake queued ahead of it can't take a frame's time). Nested calls are safe.
// - async(f): f on a worker; Future::get() waits for it (running f itself if no worker has taken it yet), and a Future
//   destroyed unfinished waits too (as std::async's).
// Nothing here throws, and neither may a chunk or f: the engine is built without exceptions, and everything the pool
// runs is called from noexcept functions (an exception escaping one, were it ever built with them, ends the game with
// the crash report: std::terminate's abort, vr_crash).
//
// Results never depend on the number of workers or on which thread ran what, as long as each chunk writes only its own
// items: reduce them in a fixed order after the call.
//
// Zancle's own sources are built optimised, without its asserts, in every configuration (its README); this header keeps
// it out of its users' includes (vr_jobs.cpp has the pool).

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Trait/Decay.hpp"
#include "Zancle/Trait/DeclVal.hpp"
#include "Zancle/Trait/IsVoid.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

namespace qvr::jobs
{

// A reference to a callable (not owned: valid while the call that takes it lasts).
template <typename Signature>
using FunctionRef = za::FunctionRef<Signature>;

namespace detail
{

// What a Future and its queued task share: whoever claims it first (a worker, or get()) runs it.
struct Job
{
    za::Atomic<int> phase{0}; // 0 queued, 1 running, 2 done
    za::Atomic<za::U32> refs; // the JobPtrs to it (the Future's, the queued task's)

    virtual ~Job() = default;
    virtual void execute() noexcept = 0;

    // Runs it here if nobody has claimed it yet; returns whether it did.
    bool claimAndRun() noexcept;
    // Returns once it is done (run here if nobody has claimed it yet).
    void wait() noexcept;
};

// A counted reference to a job (the queued task's and the Future's: std::shared_ptr's role, the count in the job): the
// job is deleted with the last one.
template <typename J>
class JobPtr
{
public:
    JobPtr() = default;
    explicit JobPtr(J* j) noexcept : p{j}
    {
        if(p)
        {
            p->refs.fetchAddRelaxed(1u);
        }
    }
    JobPtr(const JobPtr& o) noexcept : JobPtr{o.p} {}
    template <typename U>
    JobPtr(const JobPtr<U>& o) noexcept : JobPtr{o.get()} // (a Result<T>'s as a Job's)
    {
    }
    JobPtr(JobPtr&& o) noexcept : p{za::exchange(o.p, nullptr)} {}
    JobPtr& operator=(JobPtr o) noexcept
    {
        za::genericSwap(p, o.p);
        return *this;
    }
    ~JobPtr()
    {
        reset();
    }

    void reset() noexcept
    {
        if(p && p->refs.fetchSubAcqRel(1u) == 1u)
        {
            delete p;
        }
        p = nullptr;
    }
    [[nodiscard]] J* get() const noexcept
    {
        return p;
    }
    [[nodiscard]] J* operator->() const noexcept
    {
        return p;
    }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return p != nullptr;
    }

private:
    J* p{nullptr};
};

template <typename T>
struct Result : Job
{
    za::Optional<T> value;
};

template <>
struct Result<void> : Job
{
};

template <typename T, typename F>
struct JobOf final : Result<T>
{
    F f;
    explicit JobOf(F&& fn) : f{ZA_MOVE(fn)} {}
    explicit JobOf(const F& fn) : f{fn} {}
    void execute() noexcept override
    {
        if constexpr(ZA_IS_VOID(T))
        {
            f();
        }
        else
        {
            this->value.emplace(f());
        }
    }
};

// f's result type (std::invoke_result_t<std::decay_t<F>&>).
template <typename F>
using ResultOf = decltype(za::declVal<ZA_DECAY(F)&>()());

} // namespace detail

// A result on its way (async).
template <typename T>
class Future
{
public:
    Future() = default;
    explicit Future(detail::JobPtr<detail::Result<T>> j) : job{ZA_MOVE(j)} {}
    Future(Future&&) noexcept = default;
    Future& operator=(Future&& o) noexcept
    {
        if(this != &o)
        {
            finish();
            job = ZA_MOVE(o.job);
        }
        return *this;
    }
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    ~Future()
    {
        finish();
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return static_cast<bool>(job);
    }
    [[nodiscard]] bool ready() const noexcept
    {
        return job && job->phase.loadAcquire() == 2;
    }
    void wait() const noexcept
    {
        if(job)
        {
            job->wait();
        }
    }
    // Waits, then returns the result. The Future is empty after.
    T get() noexcept
    {
        detail::JobPtr<detail::Result<T>> j = ZA_MOVE(job);
        j->wait();
        if constexpr(!ZA_IS_VOID(T))
        {
            return ZA_MOVE(*j->value);
        }
    }

private:
    void finish() noexcept
    {
        if(job)
        {
            job->wait();
            job.reset();
        }
    }
    detail::JobPtr<detail::Result<T>> job;
};

// Counters since start-up, every pool's (vr_jobs_info).
struct Stats
{
    za::SizeT tasks{0};         // async tasks run (by workers or claimed by their waiter)
    za::SizeT claimedByWaiter{0}; // ... of them, run by the thread that waited
    za::SizeT loops{0};         // parallelFor calls split between threads
    za::SizeT serialLoops{0};   // parallelFor calls run on the caller alone (one chunk, no pool, or vr_jobs_parallel 0)
    za::SizeT chunksCaller{0};  // chunks run by the calling thread
    za::SizeT chunksHelpers{0}; // chunks run by helpers
    za::SizeT helpersCalledOff{0}; // helpers that started after the caller had finished (nothing left to do)
};

class Pool
{
public:
    // Starts `workers` threads (at least one).
    explicit Pool(int workers);
    // Runs every task still queued, then joins the workers.
    ~Pool();
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    [[nodiscard]] int workers() const noexcept;

    // body(begin, end) over [0, count) in chunks of `chunk` (0: about a quarter of an even share per thread), the
    // calling thread taking part; `parallel` false: every chunk on the calling thread (the same results).
    void parallelFor(za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body, bool parallel = true) noexcept;

    template <typename F>
    [[nodiscard]] auto async(F&& f) -> Future<detail::ResultOf<F>>
    {
        using T = detail::ResultOf<F>;
        detail::JobPtr<detail::Result<T>> job{new detail::JobOf<T, ZA_DECAY(F)>(ZA_FORWARD(f))};
        post(job);
        return Future<T>{ZA_MOVE(job)};
    }

    void post(detail::JobPtr<detail::Job> job);

    struct Impl;

private:
    za::UniquePtr<Impl> impl;
};

// A task made once and posted again and again without an allocation (Box3D's step tasks: vr_box3d.cpp, "Box3D on the
// pool"): post(pool, fn, context) queues fn(context); wait() returns once it ran, running it on the waiting thread if no
// worker has started it yet (as Future::get). Posted again only after wait(); destroyed, it waits.
class Task
{
public:
    Task();
    ~Task();
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void post(Pool& pool, void (*fn)(void*), void* context);
    void wait() noexcept;

    struct Job;

private:
    detail::JobPtr<Job> job;
};

// The game's pool (VR_Init .. VR_Shutdown): `workers` 0 is the hardware's threads less one (at most 31).
void init(int workers = 0);
void shutdown();
// Made again with that many workers (vr_jobs_threads): whatever is queued runs first.
void restart(int workers);
[[nodiscard]] Pool* pool() noexcept; // null outside init .. shutdown
[[nodiscard]] int workers() noexcept;  // the game pool's (0 without one)
[[nodiscard]] int hardwareThreads() noexcept;
// vr_jobs_parallel: parallelFor splits between threads (else the caller runs every chunk: the reference).
void setParallel(bool on) noexcept;
[[nodiscard]] bool parallel() noexcept;
[[nodiscard]] Stats stats() noexcept;

// On the game's pool; without one, on the calling thread (async: at once).
void parallelFor(za::SizeT count, za::SizeT chunk, FunctionRef<void(za::SizeT, za::SizeT)> body) noexcept;

template <typename F>
[[nodiscard]] auto async(F&& f) -> Future<detail::ResultOf<F>>
{
    using T = detail::ResultOf<F>;
    detail::JobPtr<detail::Result<T>> job{new detail::JobOf<T, ZA_DECAY(F)>(ZA_FORWARD(f))};
    if(Pool* p = pool())
    {
        p->post(job);
    }
    else
    {
        job->claimAndRun();
    }
    return Future<T>{ZA_MOVE(job)};
}

// The engine's side (vr_jobs_engine.cpp).
void start();            // VR_Init: the game's pool made
void registerCommands(); // vr_jobs_threads, vr_jobs_parallel; vr_jobs_info, vr_jobs_test
void info_f();
void test_f();

} // namespace qvr::jobs
