// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#include "Zancle/Concurrency/ThreadPool.hpp"

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"

#include "Zancle/Container/Vector.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"

#include <blockingconcurrentqueue.h>
#include <concurrentqueue.h>

#pragma GCC diagnostic pop


namespace za
{
namespace
{
////////////////////////////////////////////////////////////
using TaskQueue = moodycamel::BlockingConcurrentQueue<ThreadPool::Task>;


////////////////////////////////////////////////////////////
/// \brief Enqueue `count` copies of `task`, waking consumers with a single signal
///
////////////////////////////////////////////////////////////
void enqueueCopies(TaskQueue& queue, const ThreadPool::Task& task, const SizeT count)
{
    // Minimal iterator for `enqueue_bulk` that yields `task` over and over
    struct RepeatIterator
    {
        const ThreadPool::Task* ptr;

        [[nodiscard]] const ThreadPool::Task& operator*() const noexcept
        {
            return *ptr;
        }

        RepeatIterator& operator++() noexcept
        {
            return *this;
        }

        RepeatIterator operator++(int) noexcept
        {
            return *this;
        }
    };

    if (count == 0u)
        return;

    [[maybe_unused]] const bool enqueued = queue.enqueue_bulk(RepeatIterator{&task}, count);
    ZA_ASSERT(enqueued);
}

} // namespace


////////////////////////////////////////////////////////////
/// Shutdown protocol:
///
/// - An empty task is a "stop task". Users cannot post one (asserted).
/// - The destructor posts one stop task per worker. A worker exits as soon
///   as its main loop dequeues one, so each worker consumes exactly one,
///   no matter how the tasks are distributed.
/// - Stop tasks dequeued elsewhere (by `tryRunPendingTask`, e.g. from a
///   `parallelFor` inside a task that runs during destruction) are put back.
/// - After joining the workers, the destructor runs any remaining task
///   (queued by other threads behind the stop tasks, or posted by tasks
///   during shutdown) on the destroying thread.
///
////////////////////////////////////////////////////////////
struct ThreadPool::Impl
{
    TaskQueue              queue;
    za::Vector<za::Thread> workers;
};


////////////////////////////////////////////////////////////
ThreadPool::ThreadPool(const SizeT workerCount)
{
    ZA_ASSERT(workerCount > 0u);

    m_impl->workers.reserve(workerCount);

    for (SizeT i = 0u; i < workerCount; ++i)
        // `noexcept`: a task that throws terminates the program (see the class documentation)
        m_impl->workers.emplaceBack([&queue = m_impl->queue]() noexcept
        {
            moodycamel::ConsumerToken token{queue};

            while (true)
            {
                Task task; // destroyed right after running, releasing whatever it captured
                queue.wait_dequeue(token, task);

                if (!task) [[unlikely]] // stop task
                    return;

                task();
            }
        });
}


////////////////////////////////////////////////////////////
ThreadPool::~ThreadPool()
{
    enqueueCopies(m_impl->queue, Task{}, m_impl->workers.size());

    for (za::Thread& worker : m_impl->workers)
        worker.join();

    while (tryRunPendingTask())
        ;
}


////////////////////////////////////////////////////////////
void ThreadPool::post(Task&& f)
{
    ZA_ASSERT(static_cast<bool>(f) && "cannot post an empty task");

    [[maybe_unused]] const bool enqueued = m_impl->queue.enqueue(ZA_MOVE(f));
    ZA_ASSERT(enqueued);
}


////////////////////////////////////////////////////////////
void ThreadPool::postBulk(Task* const tasks, const SizeT count)
{
    ZA_ASSERT(count == 0u || tasks != nullptr);

    // Minimal iterator for `enqueue_bulk` that moves from `tasks`
    struct MoveIterator
    {
        Task* ptr;

        [[nodiscard]] Task&& operator*() const noexcept
        {
            return static_cast<Task&&>(*ptr);
        }

        MoveIterator& operator++() noexcept
        {
            ++ptr;
            return *this;
        }

        MoveIterator operator++(int) noexcept
        {
            return MoveIterator{ptr++};
        }
    };

    if (count == 0u)
        return;

#ifdef ZA_DEBUG
    for (SizeT i = 0u; i < count; ++i)
        ZA_ASSERT(static_cast<bool>(tasks[i]) && "cannot post an empty task");
#endif

    [[maybe_unused]] const bool enqueued = m_impl->queue.enqueue_bulk(MoveIterator{tasks}, count);
    ZA_ASSERT(enqueued);
}


////////////////////////////////////////////////////////////
void ThreadPool::postCopies(const Task& task, const SizeT count)
{
    ZA_ASSERT((count == 0u || static_cast<bool>(task)) && "cannot post an empty task");
    enqueueCopies(m_impl->queue, task, count);
}


////////////////////////////////////////////////////////////
bool ThreadPool::tryRunPendingTask() noexcept
{
    Task task;

    if (!m_impl->queue.try_dequeue(task))
        return false;

    if (!task) [[unlikely]]
    {
        // Stop task: it belongs to a worker's main loop (the pool is being destroyed)
        m_impl->queue.enqueue(ZA_MOVE(task));
        return false;
    }

    task();
    return true;
}


////////////////////////////////////////////////////////////
SizeT ThreadPool::getWorkerCount() const noexcept
{
    return m_impl->workers.size();
}


////////////////////////////////////////////////////////////
SizeT ThreadPool::getHardwareWorkerCount() noexcept
{
    return static_cast<SizeT>(za::Thread::hardwareConcurrency());
}


////////////////////////////////////////////////////////////
SizeT ThreadPool::getHardwareWorkerCountExcludingCallingThread() noexcept
{
    const SizeT hardwareThreads = getHardwareWorkerCount(); // `0` if unknown
    return hardwareThreads > 1u ? hardwareThreads - 1u : 1u;
}

} // namespace za
