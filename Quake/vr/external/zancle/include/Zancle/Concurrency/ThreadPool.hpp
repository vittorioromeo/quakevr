#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep

#include "Zancle/Concurrency/Atomic.hpp"

#include "Zancle/Vocabulary/FixedFunction.hpp"
#include "Zancle/Vocabulary/InPlacePImpl.hpp"

#include "Zancle/Base/CpuRelax.hpp"
#include "Zancle/Base/InterferenceSize.hpp"
#include "Zancle/Base/SizeT.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Manages a pool of worker threads to execute tasks concurrently.
///
/// This class provides a simple way to offload work to a fixed number
/// of background threads. Tasks are submitted using the `post` method
/// (or `postBulk`/`postCopies` for batches) and are executed by the
/// next available worker thread. `parallelFor` splits a range of work
/// between the workers and the calling thread, and waits for it.
///
/// Tasks and `parallelFor` bodies must not throw: an exception escaping
/// one terminates the program (`std::terminate`), as the pool cannot
/// report it to whoever posted the work, and other threads may still
/// depend on the frame it would unwind.
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
    /// Tasks still running may keep posting tasks and calling
    /// `parallelFor` until they return: everything they post also runs
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
    /// \brief Call `f(begin, end)` on chunks covering `[0, count)` in parallel, and wait for all of them
    ///
    /// Chunks are claimed dynamically by the calling thread (which always
    /// takes part) and by up to `getWorkerCount()` workers, so uneven work
    /// balances itself. `chunkSize` defaults to about a quarter of an even
    /// split between all threads; pass `1` for few, very uneven work items.
    ///
    /// `f` is invoked concurrently from multiple threads. It must not
    /// assume anything about which thread runs which chunk.
    ///
    /// While waiting, the calling thread runs pending tasks (possibly
    /// unrelated ones) instead of idling, which also makes nested calls
    /// from within a task safe.
    ///
    /// `f` must not throw: an exception escaping it terminates the program.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    void parallelFor(SizeT count, F&& f, SizeT chunkSize = 0u) noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Number of workers in the pool
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] SizeT getWorkerCount() const noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Hint at the optimal number of threads for CPU-bound tasks (equivalent to `za::Thread::hardwareConcurrency`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static SizeT getHardwareWorkerCount() noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Hardware thread count minus one (at least one): the worker count to use when the calling thread also works
    ///
    /// Leaves a hardware thread for the thread that owns the pool (e.g.
    /// the main thread, which also takes part in `parallelFor`), so that
    /// the workers and that thread together do not oversubscribe the CPU.
    /// Returns `1` on single-threaded systems, as a pool needs a worker.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static SizeT getHardwareWorkerCountExcludingCallingThread() noexcept;

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    struct Impl;
    InPlacePImpl<Impl, 896> m_impl; //!< Implementation details
};


////////////////////////////////////////////////////////////
template <typename F>
void ThreadPool::parallelFor(const SizeT count, F&& f, SizeT chunkSize) noexcept
{
    if (count == 0u)
        return;

    const SizeT nThreads = getWorkerCount() + 1u; // workers and the calling thread

    if (chunkSize == 0u)
        chunkSize = count / (nThreads * 4u) > 0u ? count / (nThreads * 4u) : 1u;

    const SizeT nChunks  = (count - 1u) / chunkSize + 1u;
    const SizeT nHelpers = nChunks - 1u < nThreads - 1u ? nChunks - 1u : nThreads - 1u;

    alignas(hardwareDestructiveInterferenceSize) Atomic<SizeT> nextChunk{0u};
    alignas(hardwareDestructiveInterferenceSize) Atomic<SizeT> helpersNotStarted{nHelpers};
    alignas(hardwareDestructiveInterferenceSize) Atomic<SizeT> helpersRemaining{nHelpers};

    const auto processChunks = [&]
    {
        for (SizeT chunk; (chunk = nextChunk.fetchAddRelaxed(1u)) < nChunks;)
        {
            const SizeT begin = chunk * chunkSize;
            f(begin, count - begin > chunkSize ? begin + chunkSize : count);
        }
    };

    if (nHelpers > 0u)
        postCopies(
            [&]
        {
            helpersNotStarted.fetchSubRelaxed(1u);
            processChunks();

            // `helpersRemaining` may be destroyed as soon as it reaches zero: only its address is used afterwards
            if (helpersRemaining.fetchSubAcqRel(1u) == 1u)
                priv::atomicNotifyOne(&helpersRemaining);
        },
            nHelpers);

    processChunks();

    // Helpers reference this stack frame: wait until all of them are done
    while (helpersRemaining.loadAcquire() != 0u)
    {
        // Run pending tasks (such as our own helpers, if all workers are busy) instead of idling
        if (tryRunPendingTask())
            continue;

        // A helper that has not started yet might still be queued (e.g. behind a stop task while the pool is being
        // destroyed, which `tryRunPendingTask` skips): keep trying to run it, as no worker may be left to do so
        if (helpersNotStarted.loadRelaxed() != 0u)
        {
            ZA_CPU_RELAX();
            continue;
        }

        // Every helper is running on some thread and will finish without our help, so blocking cannot deadlock
        for (int i = 0; i < 256 && helpersRemaining.loadAcquire() != 0u; ++i)
            ZA_CPU_RELAX();

        helpersRemaining.waitUntilAcquire([](const SizeT n) { return n == 0u; });
    }
}

} // namespace za
