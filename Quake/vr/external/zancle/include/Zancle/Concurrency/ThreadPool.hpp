#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep

#include "Zancle/Vocabulary/FixedFunction.hpp"
#include "Zancle/Vocabulary/InPlacePImpl.hpp"

#include "Zancle/Base/SizeT.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Manages a pool of worker threads to execute tasks concurrently.
///
/// This class provides a simple way to offload work to a fixed number
/// of background threads. Tasks are submitted using the `post` method
/// (or `postBulk`/`postCopies` for batches) and are executed by the
/// next available worker thread. To split a range of work between the
/// workers and the calling thread, and wait for it, see `za::parallelFor`
/// (`Zancle/Concurrency/ParallelFor.hpp`).
///
/// Tasks must not throw: an exception escaping one terminates the program
/// (`std::terminate`), as the pool cannot report it to whoever posted the
/// work, and other threads may still depend on the frame it would unwind.
///
/// Queuing a task can allocate memory (the queue grows in blocks). If that
/// allocation fails, the program aborts (`za::abort`), in every build: a
/// task the pool failed to queue would otherwise be lost silently, and
/// whoever waits for it would wait forever.
///
////////////////////////////////////////////////////////////
class ZA_SYSTEM_API ThreadPool
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Task type, stored inline via `FixedFunction` (no allocation)
    ///
    ////////////////////////////////////////////////////////////
    using Task = FixedFunction<void(), 128>;

    ////////////////////////////////////////////////////////////
    /// \brief Start `workerCount` worker threads (must be > 0)
    ///
    ////////////////////////////////////////////////////////////
    explicit ThreadPool(SizeT workerCount);

    ////////////////////////////////////////////////////////////
    /// \brief Run all pending tasks, then stop and join all workers
    ///
    /// Tasks still running may keep posting tasks (e.g. through
    /// `za::parallelFor`) until they return: everything they post also runs
    /// before the destructor returns (possibly on the destroying thread).
    /// Other threads must not use the pool while it is being destroyed.
    ///
    ////////////////////////////////////////////////////////////
    ~ThreadPool();

    ////////////////////////////////////////////////////////////
    /// \brief Enqueue a task for the next available worker
    ///
    /// `f` must not be empty.
    ///
    ////////////////////////////////////////////////////////////
    void post(Task&& f);

    ////////////////////////////////////////////////////////////
    /// \brief Enqueue `count` tasks, moved from `tasks`, waking workers with a single signal
    ///
    /// Much cheaper than `count` calls to `post`, especially when many workers are idle.
    ///
    ////////////////////////////////////////////////////////////
    void postBulk(Task* tasks, SizeT count);

    ////////////////////////////////////////////////////////////
    /// \brief Enqueue `count` copies of `task`, waking workers with a single signal
    ///
    ////////////////////////////////////////////////////////////
    void postCopies(const Task& task, SizeT count);

    ////////////////////////////////////////////////////////////
    /// \brief Run one pending task on the calling thread, if any
    ///
    /// Useful to help instead of idling while waiting for tasks to complete.
    ///
    /// \return `true` if a task was run, `false` if no task was pending
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool tryRunPendingTask() noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Number of workers in the pool
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] SizeT getWorkerCount() const noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Number of threads that can run CPU-bound work at once, the calling thread included (at least one)
    ///
    /// `za::Thread::usableHardwareConcurrency`, or `1` if undetermined.
    /// E.g. the number of per-thread buffers to allocate for `za::parallelFor`,
    /// which runs on the workers and on the calling thread.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static SizeT getOptimalThreadCount() noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Number of workers to create for CPU-bound work (at least one)
    ///
    /// `getOptimalThreadCount() - 1`: leaves a hardware thread for the
    /// thread that owns the pool (e.g. the main thread, which renders or
    /// takes part in `za::parallelFor`), so that the workers and that
    /// thread together do not oversubscribe the CPU. Returns `1` on
    /// single-threaded systems, as a pool needs a worker.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static SizeT getOptimalWorkerCount() noexcept;

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    struct Impl;
    InPlacePImpl<Impl, 896> m_impl; //!< Implementation details
};

} // namespace za
