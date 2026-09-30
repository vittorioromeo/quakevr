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
// An exception thrown by a chunk or by f comes back to the caller: parallelFor rethrows the lowest chunk's once every
// chunk ran, get() rethrows f's.
//
// Results never depend on the number of workers or on which thread ran what, as long as each chunk writes only its own
// items: reduce them in a fixed order after the call.
//
// Zancle's own sources are built optimised, without its asserts, in every configuration (its README); this header keeps
// it out of its users' includes (vr_jobs.cpp has the pool).

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace qvr::jobs
{

// A reference to a callable (not owned: valid while the call that takes it lasts).
template <typename Signature>
class FunctionRef;

template <typename R, typename... Args>
class FunctionRef<R(Args...)>
{
public:
    template <typename F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, FunctionRef> && std::is_invocable_r_v<R, F&, Args...>)
    FunctionRef(F&& f) noexcept // NOLINT(google-explicit-constructor)
        : object{const_cast<void*>(static_cast<const void*>(std::addressof(f)))},
          call{[](void* o, Args... args) -> R { return (*static_cast<std::remove_reference_t<F>*>(o))(std::forward<Args>(args)...); }}
    {
    }

    R operator()(Args... args) const
    {
        return call(object, std::forward<Args>(args)...);
    }

private:
    void* object;
    R (*call)(void*, Args...);
};

namespace detail
{

// What a Future and its queued task share: whoever claims it first (a worker, or get()) runs it.
struct Job
{
    std::atomic<int> phase{0}; // 0 queued, 1 running, 2 done
    std::exception_ptr error;

    virtual ~Job() = default;
    virtual void execute() = 0;

    // Runs it here if nobody has claimed it yet; returns whether it did.
    bool claimAndRun() noexcept;
    // Returns once it is done (run here if nobody has claimed it yet).
    void wait() noexcept;
};

template <typename T>
struct Result : Job
{
    std::optional<T> value;
};

template <>
struct Result<void> : Job
{
};

template <typename T, typename F>
struct JobOf final : Result<T>
{
    F f;
    explicit JobOf(F&& fn) : f{std::move(fn)} {}
    explicit JobOf(const F& fn) : f{fn} {}
    void execute() override
    {
        if constexpr(std::is_void_v<T>)
        {
            f();
        }
        else
        {
            this->value.emplace(f());
        }
    }
};

} // namespace detail

// A result on its way (async).
template <typename T>
class Future
{
public:
    Future() = default;
    explicit Future(std::shared_ptr<detail::Result<T>> j) : job{std::move(j)} {}
    Future(Future&&) noexcept = default;
    Future& operator=(Future&& o) noexcept
    {
        if(this != &o)
        {
            finish();
            job = std::move(o.job);
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
        return job != nullptr;
    }
    [[nodiscard]] bool ready() const noexcept
    {
        return job && job->phase.load(std::memory_order_acquire) == 2;
    }
    void wait() const noexcept
    {
        if(job)
        {
            job->wait();
        }
    }
    // Waits, then returns the result (or rethrows f's exception). The Future is empty after.
    T get()
    {
        std::shared_ptr<detail::Result<T>> j = std::move(job);
        j->wait();
        if(j->error)
        {
            std::rethrow_exception(j->error);
        }
        if constexpr(!std::is_void_v<T>)
        {
            return std::move(*j->value);
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
    std::shared_ptr<detail::Result<T>> job;
};

// Counters since start-up, every pool's (vr_jobs_info).
struct Stats
{
    std::size_t tasks{0};         // async tasks run (by workers or claimed by their waiter)
    std::size_t claimedByWaiter{0}; // ... of them, run by the thread that waited
    std::size_t loops{0};         // parallelFor calls split between threads
    std::size_t serialLoops{0};   // parallelFor calls run on the caller alone (one chunk, no pool, or vr_jobs_parallel 0)
    std::size_t chunksCaller{0};  // chunks run by the calling thread
    std::size_t chunksHelpers{0}; // chunks run by helpers
    std::size_t helpersCalledOff{0}; // helpers that started after the caller had finished (nothing left to do)
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
    void parallelFor(std::size_t count, std::size_t chunk, FunctionRef<void(std::size_t, std::size_t)> body, bool parallel = true);

    template <typename F>
    [[nodiscard]] auto async(F&& f) -> Future<std::invoke_result_t<std::decay_t<F>&>>
    {
        using T = std::invoke_result_t<std::decay_t<F>&>;
        auto job = std::make_shared<detail::JobOf<T, std::decay_t<F>>>(std::forward<F>(f));
        post(job);
        return Future<T>{std::move(job)};
    }

    void post(std::shared_ptr<detail::Job> job);

    struct Impl;

private:
    std::unique_ptr<Impl> impl;
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
void parallelFor(std::size_t count, std::size_t chunk, FunctionRef<void(std::size_t, std::size_t)> body);

template <typename F>
[[nodiscard]] auto async(F&& f) -> Future<std::invoke_result_t<std::decay_t<F>&>>
{
    using T = std::invoke_result_t<std::decay_t<F>&>;
    auto job = std::make_shared<detail::JobOf<T, std::decay_t<F>>>(std::forward<F>(f));
    if(Pool* p = pool())
    {
        p->post(job);
    }
    else
    {
        job->claimAndRun();
    }
    return Future<T>{std::move(job)};
}

// The engine's side (vr_jobs_engine.cpp).
void start();            // VR_Init: the game's pool made
void registerCommands(); // vr_jobs_threads, vr_jobs_parallel; vr_jobs_info, vr_jobs_test
void info_f();
void test_f();

} // namespace qvr::jobs
