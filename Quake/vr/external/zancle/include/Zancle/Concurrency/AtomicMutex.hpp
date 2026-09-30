#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Concurrency/Atomic.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Non-recursive mutex built on `za::Atomic` wait/notify.
///
/// Drepper-style 3-state futex mutex (`Futexes Are Tricky`, glibc
/// `lowlevellock`). The state encoding is:
///
///   `0` -- unlocked
///   `1` -- locked, no waiters announced
///   `2` -- locked, at least one waiter is parked (or about to park)
///
/// The uncontended `0 -> 1 -> 0` cycle issues no wake syscall: `unlock`
/// only calls `notifyOne` when it observes the state was `2`, proving a
/// waiter announced itself. This matches what the standard library does
/// for `std::mutex` and avoids the per-unlock `futex(WAKE)` /
/// `WakeByAddressSingle` overhead of the naive 2-state encoding.
///
/// No recursion, no priority inheritance, no fairness guarantees --
/// LIFO-ish under contention.
///
/// Use `za::LockGuard` (in `Zancle/Concurrency/LockGuard.hpp`) for RAII locking.
///
////////////////////////////////////////////////////////////
class AtomicMutex
{
public:
    ////////////////////////////////////////////////////////////
    AtomicMutex() = default;

    AtomicMutex(const AtomicMutex&)            = delete;
    AtomicMutex& operator=(const AtomicMutex&) = delete;

    AtomicMutex(AtomicMutex&&)            = delete;
    AtomicMutex& operator=(AtomicMutex&&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Acquire the mutex, blocking if necessary
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void lock() noexcept
    {
        // Fast path: 0 -> 1 (locked, no waiters announced).
        unsigned int expected = 0u;
        if (m_state.compareExchangeStrong<MemoryOrder::Acquire, MemoryOrder::Relaxed>(expected, 1u)) [[likely]]
            return;

        lockSlow(expected);
    }

    ////////////////////////////////////////////////////////////
    /// \brief Try to acquire the mutex without blocking
    ///
    /// \return `true` if the mutex was acquired, `false` if it was held by another thread
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] bool tryLock() noexcept
    {
        // Only attempt the clean 0 -> 1 transition. We deliberately never
        // inflate to 2 here: a failed `tryLock` has not committed to waiting,
        // so it must not force the current owner to issue a wake on unlock.
        unsigned int expected = 0u;
        return m_state.compareExchangeStrong<MemoryOrder::Acquire, MemoryOrder::Relaxed>(expected, 1u);
    }

    ////////////////////////////////////////////////////////////
    /// \brief Release the mutex; wake one waiter only if waiters were announced
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void unlock() noexcept
    {
        // Atomically swap state to `0`:
        //   1 -> 0  uncontended: no waiter announced, skip the wake syscall.
        //   2 -> 0  contended: at least one waiter is parked, wake exactly one.
        //
        // Single Release RMW pairs with the `Acquire` on the next lock's CAS
        // or exchange and publishes the critical-section writes -- a woken
        // thread's `exchange<Acquire>(2)` reading from this Release store
        // gets the synchronizes-with edge directly, with no release-sequence
        // reasoning needed.
        //
        // Spurious over-wake (waking when no waiter is actually parked
        // anymore -- e.g. a third thread already acquired via the fast
        // path between this exchange and `notifyOne`) is harmless: the
        // woken thread's `exchange<Acquire>` sees a non-zero value and
        // parks again.
        if (m_state.exchange<MemoryOrder::Release>(0u) == 2u) [[unlikely]]
            m_state.notifyOne();
    }

private:
    ////////////////////////////////////////////////////////////
    /// \brief Slow path of `lock()`: parking loop on contention.
    ///
    /// Out-of-line so the inlined fast path stays tight at every call
    /// site. Not marked `gnu::cold` -- under heavy contention this *is*
    /// the hot path, and cold-bias would hurt code layout/scheduling.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::noinline]] void lockSlow(unsigned int expected) noexcept
    {
        // `expected` holds the observed value from the failed fast-path
        // CAS (1 or 2). Inflate the state to 2 ("locked, has waiters")
        // so the current owner's eventual unlock issues a wake.
        // `exchange` does this unconditionally and returns the *previous*
        // value: if it was 0 (the owner released between our fast-path
        // CAS and this exchange) we just acquired and we're done --
        // otherwise we park.
        if (expected != 2u)
            expected = m_state.exchange<MemoryOrder::Acquire>(2u);

        while (expected != 0u)
        {
            // Park while the word still says "locked with waiters". The
            // kernel-side compare avoids missed wakeups: if `unlock` raced
            // past us, `m_state` is already 0 and `wait` returns immediately.
            // Spurious wakeups are absorbed by the outer loop.
            //
            // `Relaxed` is sufficient here -- the subsequent
            // `exchange<Acquire>` is what synchronizes with the prior owner.
            m_state.waitOnceRelaxed(2u);
            expected = m_state.exchange<MemoryOrder::Acquire>(2u);
        }
    }

    ////////////////////////////////////////////////////////////
    // 4-byte atomic so it's compatible with `wait` (which requires 4 or 8 bytes
    // -- futex / WaitOnAddress operate on 32-bit words). 3-state encoding:
    // 0 = unlocked, 1 = locked-no-waiters, 2 = locked-with-waiters.
    Atomic<unsigned int> m_state{0u};
};

} // namespace za
