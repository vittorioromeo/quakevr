// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp"

#include "Zancle/Concurrency/Atomic.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/InterferenceSize.hpp"
#include "Zancle/Base/Limits.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/UIntPtrT.hpp"


////////////////////////////////////////////////////////////
// Platform backend selection
//
// Every backend except Win32, Emscripten, and the spin fallback waits
// on 32-bit words only: 64-bit waits then sleep on a per-slot version
// counter instead (see `atomicWait64`).
////////////////////////////////////////////////////////////
#if defined(ZA_SYSTEM_LINUX) || defined(ZA_SYSTEM_ANDROID)
    #define ZA_PRIV_WAIT_LINUX_FUTEX 1 // `futex(2)`
#elif defined(ZA_SYSTEM_WINDOWS)
    #define ZA_PRIV_WAIT_WIN32 1 // `WaitOnAddress`
#elif defined(ZA_SYSTEM_EMSCRIPTEN)
    #define ZA_PRIV_WAIT_EMSCRIPTEN 1 // `Atomics.wait`
#elif defined(ZA_SYSTEM_MACOS) || defined(ZA_SYSTEM_IOS)
    #define ZA_PRIV_WAIT_APPLE_ULOCK 1 // `__ulock_wait` (as used by libc++)
#elif defined(ZA_SYSTEM_FREEBSD)
    #define ZA_PRIV_WAIT_FREEBSD_UMTX 1 // `_umtx_op(2)` (as used by libc++)
#elif defined(ZA_SYSTEM_OPENBSD)
    #define ZA_PRIV_WAIT_OPENBSD_FUTEX 1 // `futex(2)`
#else
    #define ZA_PRIV_WAIT_SPIN 1 // yield-based busy wait (e.g. NetBSD)
#endif

#if ZA_PRIV_WAIT_WIN32 || ZA_PRIV_WAIT_EMSCRIPTEN || ZA_PRIV_WAIT_SPIN
    #define ZA_PRIV_WAIT_NATIVE_64 1 // waits on 64-bit words directly
#else
    #define ZA_PRIV_WAIT_NATIVE_64 0
#endif


////////////////////////////////////////////////////////////
// Headers (platform)
////////////////////////////////////////////////////////////
#if ZA_PRIV_WAIT_LINUX_FUTEX

    #include <linux/futex.h>
    #include <sys/syscall.h>
    #include <unistd.h>

    #include <ctime>

#elif ZA_PRIV_WAIT_WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include "Zancle/Base/WindowsHeader.hpp"

    #include <synchapi.h>

#elif ZA_PRIV_WAIT_EMSCRIPTEN

    #include <emscripten/threading.h>

#elif ZA_PRIV_WAIT_APPLE_ULOCK

// Private but stable Darwin API (macOS 10.12+, iOS 10+), also used by libc++ to implement `std::atomic::wait`.
// See https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/ulock.h
extern "C" int __ulock_wait(za::U32 operation, void* addr, za::U64 value, za::U32 timeoutMicroseconds);
extern "C" int __ulock_wake(za::U32 operation, void* addr, za::U64 wakeValue);

#elif ZA_PRIV_WAIT_FREEBSD_UMTX

    #include <sys/types.h>
    #include <sys/umtx.h>

#elif ZA_PRIV_WAIT_OPENBSD_FUTEX

    #include <sys/futex.h>
    #include <sys/time.h>

#elif ZA_PRIV_WAIT_SPIN

    #include "Zancle/Base/CpuRelax.hpp"

    #include <sched.h>

#endif


namespace za::priv
{
namespace
{
////////////////////////////////////////////////////////////
/// \brief Per-address contention slot
///
/// Tracks the number of threads currently parked on (or about to park
/// on) a given address. Every slot lives on its own cache line to
/// avoid false sharing between unrelated atomics whose addresses
/// happen to hash to neighbouring entries.
///
/// `version` is what 64-bit waiters sleep on where the platform only
/// waits on 32-bit words: every 64-bit notification targeting the slot
/// increments it (see `atomicWait64`).
///
////////////////////////////////////////////////////////////
struct alignas(za::hardwareDestructiveInterferenceSize) ContentionSlot
{
    Atomic<za::U32> waiters{0u};
    alignas(4) za::U32 version{0u}; // accessed with the `__atomic` builtins, as its address is waited on
};


////////////////////////////////////////////////////////////
constexpr za::SizeT slotCount = 256u;


////////////////////////////////////////////////////////////
/// \brief Global parking-lot table
///
/// Indexed by a hash of the user's atomic address. Two atomics may
/// hash to the same slot, which causes a benign over-count of waiters
/// (a notify may issue a syscall that finds nobody to wake, but
/// correctness is unaffected -- `notifyOne`/`notifyAll` never miss a
/// real waiter).
///
////////////////////////////////////////////////////////////
constinit ContentionSlot slots[slotCount];


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::pure]] inline ContentionSlot& slotFor(const void* const addr) noexcept
{
    // Murmur3 64-bit finalizer -- gives good entropy even when most of
    // the input bits are predictable (heap addresses share a prefix).
    auto h = static_cast<za::U64>(reinterpret_cast<za::UIntPtrT>(addr));
    h ^= h >> 33;
    h *= 0xff'51'af'd7'ed'55'8c'cdULL;
    h ^= h >> 33;
    h *= 0xc4'ce'b9'fe'1a'85'ec'53ULL;
    h ^= h >> 33;

    return slots[h & (slotCount - 1u)];
}


#if ZA_PRIV_WAIT_SPIN

////////////////////////////////////////////////////////////
template <typename T>
inline void spinWait(const T* const addr, const T expected) noexcept
{
    // Yield-based fallback used on platforms where we don't have a
    // native wait primitive. Burns CPU proportionally to the number of
    // waiters; paired with a matching `notify` it just collapses into
    // a busy-poll until the change is observed.
    constexpr int spinIterations = 64;

    for (int i = 0; i < spinIterations; ++i)
    {
        if (__atomic_load_n(addr, __ATOMIC_ACQUIRE) != expected)
            return;

        ZA_CPU_RELAX();
    }

    while (__atomic_load_n(addr, __ATOMIC_ACQUIRE) == expected)
        sched_yield();
}

#endif


////////////////////////////////////////////////////////////
/// \brief Native blocking call on a 32-bit address
///
/// Returns when (a) the value at `addr` differs from `expected`,
/// (b) someone calls `platformWake` on `addr`, (c) spuriously, or
/// (d) on Linux, every 2 seconds (defensive timeout -- see below).
///
////////////////////////////////////////////////////////////
void platformWait32(const za::U32* const addr, const za::U32 expected) noexcept
{
#if ZA_PRIV_WAIT_LINUX_FUTEX
    // 2-second timeout copied from libc++. The kernel cancels the
    // timer on a normal wake, so the happy path pays nothing extra.
    // The defensive value here is to recover from theoretically
    // possible missed-wake hazards (sandboxed runtimes, mismatched
    // FUTEX_PRIVATE flags, kernel ABI bugs, etc.) -- if such a thing
    // ever fires, the waiter loops back through the predicate check
    // and either makes forward progress or re-enters wait.
    const struct timespec timeout{2, 0};

    syscall(SYS_futex,
            const_cast<za::U32*>(addr),
            FUTEX_WAIT_PRIVATE,
            static_cast<int>(expected),
            &timeout,
            /* uaddr2 */ nullptr,
            /* val3   */ 0);

#elif ZA_PRIV_WAIT_WIN32
    za::U32 compare = expected;
    WaitOnAddress(const_cast<za::U32*>(addr), &compare, sizeof(compare), INFINITE);

#elif ZA_PRIV_WAIT_EMSCRIPTEN
    emscripten_atomic_wait_u32(const_cast<za::U32*>(addr), expected, /* maxWaitNanoseconds */ -1);

#elif ZA_PRIV_WAIT_APPLE_ULOCK
    constexpr za::U32 ulCompareAndWait = 1u; // `UL_COMPARE_AND_WAIT`
    __ulock_wait(ulCompareAndWait, const_cast<za::U32*>(addr), expected, /* timeout: none */ 0u);

#elif ZA_PRIV_WAIT_FREEBSD_UMTX
    _umtx_op(const_cast<za::U32*>(addr), UMTX_OP_WAIT_UINT_PRIVATE, expected, /* uaddr */ nullptr, /* timeout */ nullptr);

#elif ZA_PRIV_WAIT_OPENBSD_FUTEX
    futex(const_cast<za::U32*>(addr),
          FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
          static_cast<int>(expected),
          /* timeout */ nullptr,
          /* uaddr2  */ nullptr);

#elif ZA_PRIV_WAIT_SPIN
    spinWait(addr, expected);
#endif
}


#if ZA_PRIV_WAIT_NATIVE_64

////////////////////////////////////////////////////////////
void platformWait64(const za::U64* const addr, const za::U64 expected) noexcept
{
    #if ZA_PRIV_WAIT_WIN32
    za::U64 compare = expected;
    WaitOnAddress(const_cast<za::U64*>(addr), &compare, sizeof(compare), INFINITE);

    #elif ZA_PRIV_WAIT_EMSCRIPTEN
    emscripten_atomic_wait_u64(const_cast<za::U64*>(addr), expected, /* maxWaitNanoseconds */ -1);

    #elif ZA_PRIV_WAIT_SPIN
    spinWait(addr, expected);
    #endif
}

#endif


////////////////////////////////////////////////////////////
void platformWake(const void* const addr, const bool wakeOne) noexcept
{
#if ZA_PRIV_WAIT_LINUX_FUTEX
    syscall(SYS_futex,
            const_cast<void*>(addr),
            FUTEX_WAKE_PRIVATE,
            /* val: number of waiters to wake */ wakeOne ? 1 : ZA_INT_MAX,
            /* timeout */ nullptr,
            /* uaddr2  */ nullptr,
            /* val3    */ 0);

#elif ZA_PRIV_WAIT_WIN32
    if (wakeOne)
        WakeByAddressSingle(const_cast<void*>(addr));
    else
        WakeByAddressAll(const_cast<void*>(addr));

#elif ZA_PRIV_WAIT_EMSCRIPTEN
    emscripten_atomic_notify(const_cast<void*>(addr), wakeOne ? 1 : ZA_INT_MAX);

#elif ZA_PRIV_WAIT_APPLE_ULOCK
    constexpr za::U32 ulCompareAndWait = 1u;            // `UL_COMPARE_AND_WAIT` (must match the wait operation)
    constexpr za::U32 ulfWakeAll       = 0x1'00u;       // `ULF_WAKE_ALL`
    constexpr za::U32 ulfNoErrno       = 0x1'00'00'00u; // `ULF_NO_ERRNO`
    __ulock_wake(ulCompareAndWait | ulfNoErrno | (wakeOne ? 0u : ulfWakeAll), const_cast<void*>(addr), /* wakeValue */ 0u);

#elif ZA_PRIV_WAIT_FREEBSD_UMTX
    _umtx_op(const_cast<void*>(addr), UMTX_OP_WAKE_PRIVATE, wakeOne ? 1 : ZA_INT_MAX, /* uaddr */ nullptr, /* uaddr2 */ nullptr);

#elif ZA_PRIV_WAIT_OPENBSD_FUTEX
    futex(static_cast<volatile za::U32*>(const_cast<void*>(addr)),
          FUTEX_WAKE | FUTEX_PRIVATE_FLAG,
          wakeOne ? 1 : ZA_INT_MAX,
          /* timeout */ nullptr,
          /* uaddr2  */ nullptr);

#elif ZA_PRIV_WAIT_SPIN
    // Spin-wait fallback wakes itself by re-loading; nothing to do.
    (void)addr;
    (void)wakeOne;
#endif
}


////////////////////////////////////////////////////////////
/// \brief Whether a thread may be parked on `addr` (if `false`, notifying can be skipped)
///
/// The SeqCst fence orders the caller's preceding store to the watched
/// value before the load of the waiter count, whatever the store's
/// memory order. Without it, a plain `Release` store could become
/// visible after this load (e.g. on x86 the store may still sit in the
/// store buffer), so a waiter could re-check the old value and park
/// while we read a zero count and skip the wake: a missed wakeup.
///
/// It pairs with the SeqCst RMW in `atomicWait32`/`atomicWait64`: if
/// we read a zero count, the waiter's increment is ordered after our
/// fence, so its re-check of the value observes the new value and it
/// does not park.
///
////////////////////////////////////////////////////////////
[[nodiscard]] bool mayHaveWaiters(const void* const addr) noexcept
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return slotFor(addr).waiters.loadRelaxed() != 0u;
}

} // namespace


////////////////////////////////////////////////////////////
void atomicWait32(const za::U32* const addr, const za::U32 expected) noexcept
{
    auto& slot = slotFor(addr);

    // Register as a waiter BEFORE re-checking the value. The SeqCst
    // RMW pairs with the SeqCst fence in `mayHaveWaiters`: at least one
    // of the two threads sees the other's effect, so we can never
    // simultaneously (a) miss a wake and (b) park.
    slot.waiters.fetchAddSeqCst(1u);

    // Re-check the value under the new ordering. If it has already
    // changed (e.g. the writer wrote between our caller's pre-check
    // and our increment), skip the syscall entirely.
    if (__atomic_load_n(addr, __ATOMIC_SEQ_CST) == expected)
        platformWait32(addr, expected);

    slot.waiters.fetchSubRelease(1u);
}


////////////////////////////////////////////////////////////
void atomicWait64(const za::U64* const addr, const za::U64 expected) noexcept
{
    auto& slot = slotFor(addr);

    slot.waiters.fetchAddSeqCst(1u);

#if ZA_PRIV_WAIT_NATIVE_64
    if (__atomic_load_n(addr, __ATOMIC_SEQ_CST) == expected)
        platformWait64(addr, expected);
#else
    // The platform only waits on 32-bit words. Waiting on half of the value
    // would lose wakeups: if only the other half changed (e.g. a `double`
    // going from `1.0` to `2.0`) between the check below and the kernel's
    // compare, the notification would find nobody parked, and the waiter
    // would then sleep on an unchanged half. Instead, sleep on the slot's
    // version, which every 64-bit notification targeting the slot changes:
    // reading it before checking the value means that a notification after
    // the check makes the kernel's compare fail (or wakes the sleeper).
    const za::U32 version = __atomic_load_n(&slot.version, __ATOMIC_SEQ_CST);

    if (__atomic_load_n(addr, __ATOMIC_SEQ_CST) == expected)
        platformWait32(&slot.version, version);
#endif

    slot.waiters.fetchSubRelease(1u);
}


////////////////////////////////////////////////////////////
void atomicNotify32(const void* const addr, const bool wakeAll) noexcept
{
    // Skip the syscall when no thread is parked on this address
    if (mayHaveWaiters(addr))
        platformWake(addr, /* wakeOne */ !wakeAll);
}


////////////////////////////////////////////////////////////
void atomicNotify64(const void* const addr, const bool wakeAll) noexcept
{
    if (!mayHaveWaiters(addr))
        return;

#if ZA_PRIV_WAIT_NATIVE_64
    platformWake(addr, /* wakeOne */ !wakeAll);
#else
    // 64-bit waiters sleep on the slot's version (see `atomicWait64`), shared by every address
    // hashing to the slot: wake them all, as waking one could pick a waiter for another address
    (void)wakeAll;

    auto& slot = slotFor(addr);
    __atomic_fetch_add(&slot.version, 1u, __ATOMIC_SEQ_CST);
    platformWake(&slot.version, /* wakeOne */ false);
#endif
}

} // namespace za::priv
