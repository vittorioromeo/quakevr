#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/ThreadPool.hpp"

#include "Zancle/Base/Ctzll.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/InterferenceSize.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/RemoveReference.hpp"


////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////
namespace za
{
class ParallelForSlots;
} // namespace za


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief How `parallelFor` wakes its helpers, depending on the cost of waking sleeping workers
///
/// `ThreadPool` wakes workers through a semaphore: on Windows, waking any
/// number of them is a single call, so the caller posts every helper at
/// once (flat). Elsewhere, waking each worker is a syscall of its own
/// (e.g. ~8us on Linux, ~240us for 31 workers), so the caller only wakes
/// two helpers, and each helper that starts wakes two more (tree): the
/// caller starts working right away, and the wakeups proceed in parallel.
///
////////////////////////////////////////////////////////////
#ifdef ZA_SYSTEM_WINDOWS
inline constexpr bool parallelForTreeWake = false;
#else
inline constexpr bool parallelForTreeWake = true;
#endif


////////////////////////////////////////////////////////////
template <bool TreeWake, typename F>
void parallelForImpl(ThreadPool& pool, ParallelForSlots& slots, SizeT count, F&& f, SizeT chunkSize) noexcept;

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Bookkeeping storage for `za::parallelFor`, owned by the user
///
/// Holds up to `slotCount` "gates", one per `parallelFor` call in
/// progress (nested calls and calls from other threads included). A call
/// that finds no free gate still completes, entirely on the calling
/// thread.
///
/// Usually one per pool, living as long as it. Any number of pools may
/// share one. Destruction order does not matter: the destructor waits
/// for helper tasks that are still queued in a pool, which only look at
/// the gates (so the pool must still be able to run them, e.g. the
/// destructor must not run inside one of that pool's tasks while it is
/// the only worker). It must not be destroyed while a `parallelFor`
/// using it is in progress.
///
/// It also bounds the helper tasks posted and not finished yet: a helper
/// queued behind busy workers stays queued after its call returned, until
/// a worker gets to it (and finds nothing to do). Once a call's pool has
/// `outstandingHelpersPerWorker` such helpers per worker, calls post no
/// more helpers and run on their calling threads, so back-to-back calls
/// while every worker is busy cannot grow the queue without bound.
///
////////////////////////////////////////////////////////////
class ZA_SYSTEM_API ParallelForSlots
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Maximum number of `parallelFor` calls in progress at once that can use helpers
    ///
    ////////////////////////////////////////////////////////////
    static constexpr SizeT slotCount = 64u;


    ////////////////////////////////////////////////////////////
    /// \brief Helper tasks not yet finished, per worker of a call's pool, beyond which calls post no more helpers
    ///
    /// As many as `slotCount` calls in progress at once can use helpers,
    /// each posting up to one per worker.
    ///
    ////////////////////////////////////////////////////////////
    static constexpr SizeT outstandingHelpersPerWorker = slotCount;


    ////////////////////////////////////////////////////////////
    ParallelForSlots() noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Wait until every helper task posted through these slots has run (see the class documentation)
    ///
    ////////////////////////////////////////////////////////////
    ~ParallelForSlots();


    ////////////////////////////////////////////////////////////
    ParallelForSlots(const ParallelForSlots&)            = delete;
    ParallelForSlots& operator=(const ParallelForSlots&) = delete;

private:
    ////////////////////////////////////////////////////////////
    template <bool TreeWake, typename F>
    friend void priv::parallelForImpl(ThreadPool& pool, ParallelForSlots& slots, SizeT count, F&& f, SizeT chunkSize) noexcept;


    ////////////////////////////////////////////////////////////
    // Gate word layout: [ generation (32 bits) | closed | waiting | helpers inside (30 bits) ]
    static constexpr U64 insideMask   = (U64{1} << 30u) - 1u;
    static constexpr U64 waitingBit   = U64{1} << 30u;
    static constexpr U64 closedBit    = U64{1} << 31u;
    static constexpr U64 destroyBit   = U64{1} << 63u; // in `m_outstanding`: the destructor is waiting
    static constexpr U64 allSlotsBits = ~U64{0};


    ////////////////////////////////////////////////////////////
    static_assert(slotCount == 64u, "the free mask is one 64-bit word");


    ////////////////////////////////////////////////////////////
    /// \brief Claim a free gate; `slotCount` if none is free
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] SizeT acquireSlot() noexcept
    {
        U64 used = m_usedMask.loadRelaxed();

        while (used != allSlotsBits)
        {
            const auto slot = static_cast<SizeT>(ZA_CTZLL(~used));

            if (m_usedMask.compareExchangeWeak<MemoryOrder::Acquire, MemoryOrder::Relaxed>(used, used | (U64{1} << slot)))
                return slot;
        }

        return slotCount;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Account for up to `wanted` new helper tasks, keeping the helpers not yet finished within `budget`
    ///
    /// \return How many helpers to post (`0` once the budget is used up)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] SizeT reserveHelpers(const SizeT wanted, const SizeT budget) noexcept
    {
        U64 outstanding = m_outstanding.loadRelaxed();

        while (outstanding < budget)
        {
            const auto  available = static_cast<SizeT>(budget - outstanding);
            const SizeT reserved  = wanted < available ? wanted : available;

            if (m_outstanding.compareExchangeWeak<MemoryOrder::Relaxed, MemoryOrder::Relaxed>(outstanding, outstanding + reserved))
                return reserved;
        }

        return 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Generation of a gate just acquired (open, nobody inside)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] U32 getGeneration(const SizeT slot) const noexcept
    {
        return static_cast<U32>(m_gates[slot].word.loadRelaxed() >> 32u);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Helper side: enter the gate, unless it was closed or reused since the helper was posted
    ///
    /// A helper that fails to enter touches nothing else of its `parallelFor`
    /// call, which may have returned long ago.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] bool tryEnter(const SizeT slot, const U32 generation) noexcept
    {
        Atomic<U64>& word     = m_gates[slot].word;
        U64          expected = word.loadRelaxed();

        while (static_cast<U32>(expected >> 32u) == generation && (expected & closedBit) == 0u)
            if (word.compareExchangeWeak<MemoryOrder::Acquire, MemoryOrder::Relaxed>(expected, expected + 1u))
                return true;

        return false;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Helper side: leave the gate, waking the caller if it sleeps waiting for the last helper
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void leave(const SizeT slot) noexcept
    {
        Atomic<U64>& word = m_gates[slot].word;
        const U64    prev = word.fetchSubRelease(1u);

        if ((prev & insideMask) == 1u && (prev & waitingBit) != 0u)
            word.notifyOne();
    }


    ////////////////////////////////////////////////////////////
    /// \brief Helper side: account for a helper task that has run (the last thing a helper does)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void finishHelper() noexcept
    {
        // Only the address is used after the decrement: the destructor may already be done
        const void* const address = &m_outstanding;

        if (m_outstanding.fetchSubAcqRel(1u) == (destroyBit | 1u))
            priv::atomicNotify64(address, /* wakeAll */ false);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Caller side: close the gate (no helper can enter any more) and wait for the helpers inside
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void closeAndWait(const SizeT slot) noexcept
    {
        if ((m_gates[slot].word.fetchOrAcqRel(closedBit) & insideMask) != 0u) [[unlikely]]
            waitForHelpersInside(slot);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Caller side: reopen the gate under a new generation (stale helpers can no longer enter), and free it
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void releaseSlot(const SizeT slot) noexcept
    {
        const U64 nextGeneration = U64{static_cast<U32>(getGeneration(slot) + 1u)} << 32u;

        m_gates[slot].word.storeRelease(nextGeneration);
        m_usedMask.fetchAndRelease(~(U64{1} << slot));
    }


    ////////////////////////////////////////////////////////////
    void waitForHelpersInside(SizeT slot) noexcept;

    ////////////////////////////////////////////////////////////
    /// \brief Post helpers `[first, first + count)` of a call (`count` is 1 or 2), as part of a tree wake
    ///
    /// The caller posts helpers 0 and 1, and helper `i` posts helpers `2i + 2`
    /// and `2i + 3` (see `priv::parallelForTreeWake`). Posts fewer, or none,
    /// once the helper budget is used up (see `reserveHelpers`).
    ///
    ////////////////////////////////////////////////////////////
    template <typename Frame>
    static void postHelpers(ParallelForSlots& slots,
                            const SizeT       slot,
                            const U32         generation,
                            Frame* const      frame,
                            const SizeT       first,
                            const SizeT       count) noexcept
    {
        const SizeT reserved = slots.reserveHelpers(count, frame->helperBudget);

        ThreadPool::Task tasks[2];
        for (SizeT i = 0u; i < reserved; ++i)
            tasks[i] = ThreadPool::Task{[&slots, slot, generation, frame, index = first + i]
            { runHelper(slots, slot, generation, frame, index); }};

        frame->pool->postBulk(tasks, reserved);
    }

    ////////////////////////////////////////////////////////////
    /// \brief Body of helper `index`: enter the gate, wake its own children, process chunks
    ///
    /// The frame (on the caller's stack) is only touched after entering the gate.
    ///
    ////////////////////////////////////////////////////////////
    template <typename Frame>
    static void runHelper(ParallelForSlots& slots, const SizeT slot, const U32 generation, Frame* const frame, const SizeT index) noexcept
    {
        if (slots.tryEnter(slot, generation))
        {
            // Wake the children first, but only if there may be work left for them
            const SizeT firstChild = index * 2u + 2u;

            if (firstChild < frame->nHelpers && frame->hasChunksLeft())
                postHelpers(slots,
                            slot,
                            generation,
                            frame,
                            firstChild,
                            frame->nHelpers - firstChild < 2u ? frame->nHelpers - firstChild : 2u);

            frame->processChunks();
            slots.leave(slot);
        }

        slots.finishHelper();
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    struct alignas(hardwareDestructiveInterferenceSize) Gate
    {
        Atomic<U64> word{0u};
    };

    Gate m_gates[slotCount];

    alignas(hardwareDestructiveInterferenceSize) Atomic<U64> m_usedMask{0u};    //!< Bit `i` set: gate `i` in use
    alignas(hardwareDestructiveInterferenceSize) Atomic<U64> m_outstanding{0u}; //!< Helper tasks not yet finished
};


////////////////////////////////////////////////////////////
/// \brief Call `f(begin, end)` on chunks covering `[0, count)` in parallel on `pool`, and wait for all of them
///
/// Chunks are claimed dynamically by the calling thread (which always
/// takes part) and by up to `pool.getWorkerCount()` helper tasks, so
/// uneven work balances itself. `chunkSize` defaults to about a quarter
/// of an even split between all threads; pass `1` for few, very uneven
/// work items.
///
/// Returns as soon as every chunk is done. The calling thread never runs
/// unrelated tasks while waiting, and does not wait for helper tasks that
/// have not started yet (e.g. queued behind long tasks): those find the
/// work done and return at once whenever they run. So a call takes as
/// long as its own work, not as long as whatever else the pool is busy
/// with. When every worker is busy, the caller simply does all the work
/// (and, once too many helpers are queued, posts none: see
/// `ParallelForSlots`).
///
/// `slots` holds the call's bookkeeping (see `ParallelForSlots`). Calls
/// may be nested (e.g. `f` itself calling `parallelFor`) and may be made
/// concurrently from multiple threads.
///
/// `f` is invoked concurrently from multiple threads, and must not assume
/// anything about which thread runs which chunk. `f` must not throw: an
/// exception escaping it terminates the program.
///
////////////////////////////////////////////////////////////
template <typename F>
void parallelFor(ThreadPool& pool, ParallelForSlots& slots, const SizeT count, F&& f, const SizeT chunkSize = 0u) noexcept
{
    priv::parallelForImpl<priv::parallelForTreeWake>(pool, slots, count, static_cast<F&&>(f), chunkSize);
}

} // namespace za


namespace za::priv
{
////////////////////////////////////////////////////////////
template <bool TreeWake, typename F>
void parallelForImpl(ThreadPool& pool, ParallelForSlots& slots, const SizeT count, F&& f, SizeT chunkSize) noexcept
{
    if (count == 0u)
        return;

    const SizeT nThreads = pool.getWorkerCount() + 1u; // workers and the calling thread

    if (chunkSize == 0u)
        chunkSize = count / (nThreads * 4u) > 0u ? count / (nThreads * 4u) : 1u;

    const SizeT nChunks  = (count - 1u) / chunkSize + 1u;
    const SizeT nHelpers = nChunks - 1u < nThreads - 1u ? nChunks - 1u : nThreads - 1u;

    // The call's state, shared with the helpers (which only touch it after entering the gate)
    struct Frame
    {
        alignas(hardwareDestructiveInterferenceSize) Atomic<SizeT> nextChunk{0u};

        ThreadPool*         pool;
        RemoveReference<F>* f;
        SizeT               count;
        SizeT               chunkSize;
        SizeT               nChunks;
        SizeT               nHelpers;
        SizeT               helperBudget; // see `ParallelForSlots::reserveHelpers`

        [[nodiscard, gnu::always_inline]] bool hasChunksLeft() const noexcept
        {
            return nextChunk.loadRelaxed() < nChunks;
        }

        void processChunks() noexcept
        {
            for (SizeT chunk = nextChunk.fetchAddRelaxed(1u); chunk < nChunks; chunk = nextChunk.fetchAddRelaxed(1u))
            {
                const SizeT begin = chunk * chunkSize;
                (*f)(begin, count - begin > chunkSize ? begin + chunkSize : count);
            }
        }
    };

    Frame frame{.pool         = &pool,
                .f            = &f,
                .count        = count,
                .chunkSize    = chunkSize,
                .nChunks      = nChunks,
                .nHelpers     = nHelpers,
                .helperBudget = ParallelForSlots::outstandingHelpersPerWorker * (nThreads - 1u)};

    // No helpers needed, or every gate in use: do everything on the calling thread
    const SizeT slot = nHelpers > 0u ? slots.acquireSlot() : ParallelForSlots::slotCount;

    if (slot == ParallelForSlots::slotCount)
    {
        frame.processChunks();
        return;
    }

    // Helpers only touch the frame after entering the gate, which the caller closes and drains before returning
    const U32 generation = slots.getGeneration(slot);

    if constexpr (TreeWake)
    {
        ParallelForSlots::postHelpers(slots, slot, generation, &frame, 0u, nHelpers < 2u ? nHelpers : 2u);
    }
    else
    {
        // Every helper at once (within the budget), with an index that has no children
        const SizeT reserved = slots.reserveHelpers(nHelpers, frame.helperBudget);

        if (reserved > 0u)
            pool.postCopies(ThreadPool::Task{[&slots, slot, generation, framePtr = &frame, nHelpers]
            { ParallelForSlots::runHelper(slots, slot, generation, framePtr, nHelpers); }},
                            reserved);
    }

    frame.processChunks();

    slots.closeAndWait(slot); // every chunk is claimed: only wait for the helpers still finishing one
    slots.releaseSlot(slot);
}

} // namespace za::priv
