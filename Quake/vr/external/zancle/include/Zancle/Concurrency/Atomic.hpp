#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

#include "Zancle/Config.hpp"

#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/PtrDiffT.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsEnum.hpp"
#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsIntegral.hpp"
#include "Zancle/Trait/IsPointer.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/UnderlyingType.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Memory ordering constraint for an atomic operation
///
/// The underlying values match the `__ATOMIC_*` constants the
/// GCC/Clang builtins expect, so the cast to `int` is a no-op.
///
/// `Consume` is intentionally omitted: every mainstream compiler
/// strengthens it to `Acquire`, and its formal semantics are widely
/// considered a footgun. Use `Acquire` instead.
///
////////////////////////////////////////////////////////////
enum class MemoryOrder : int
{
    Relaxed = __ATOMIC_RELAXED,
    Acquire = __ATOMIC_ACQUIRE,
    Release = __ATOMIC_RELEASE,
    AcqRel  = __ATOMIC_ACQ_REL,
    SeqCst  = __ATOMIC_SEQ_CST,
};


namespace priv
{
////////////////////////////////////////////////////////////
template <typename T>
struct AtomicPointee;

template <typename T>
struct AtomicPointee<T*>
{
    using type = T;
};


////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isAtomicSupported = (ZA_IS_INTEGRAL(T) || ZA_IS_FLOATING_POINT(T) || ZA_IS_POINTER(T) ||
                                           ZA_IS_ENUM(T));


////////////////////////////////////////////////////////////
/// \brief Storage representation used by the atomic builtins
///
/// The `__atomic_*_n` builtins on Clang accept only integer and
/// pointer types. For floating-point and `enum` we therefore store
/// the value as the matching integer and bit-cast at the API
/// boundary. The atomic semantics are unaffected because CAS already
/// compares bit-patterns (which is also what the user sees).
///
////////////////////////////////////////////////////////////
template <typename T, bool IsEnum = za::isEnum<T>>
struct AtomicStorage
{
    using type = T;
};

template <typename T>
struct AtomicStorage<T, /* IsEnum */ true>
{
    using type = ZA_UNDERLYING_TYPE(T);
};

template <>
struct AtomicStorage<float, /* IsEnum */ false>
{
    static_assert(sizeof(float) == sizeof(za::U32));
    using type = za::U32;
};

template <>
struct AtomicStorage<double, /* IsEnum */ false>
{
    static_assert(sizeof(double) == sizeof(za::U64));
    using type = za::U64;
};


////////////////////////////////////////////////////////////
template <typename T>
using AtomicStorageType = typename AtomicStorage<T>::type;


////////////////////////////////////////////////////////////
/// \brief Convert `T` to its atomic-storage representation (no-op for non-float, bit-cast for float/double)
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline]] inline constexpr AtomicStorageType<T> toAtomicStorage(const T value) noexcept
{
    if constexpr (ZA_IS_SAME(T, AtomicStorageType<T>))
        return value;
    else
        return ZA_BIT_CAST(AtomicStorageType<T>, value);
}


////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline]] inline constexpr T fromAtomicStorage(const AtomicStorageType<T> bits) noexcept
{
    if constexpr (ZA_IS_SAME(T, AtomicStorageType<T>))
        return bits;
    else
        return ZA_BIT_CAST(T, bits);
}


////////////////////////////////////////////////////////////
/// \brief Compile-time legality check for a CAS failure-order pair
///
/// Mirrors the C++ rule: the failure order must not be `Release` or
/// `AcqRel`, and must not be stronger than the success order.
///
////////////////////////////////////////////////////////////
[[nodiscard]] inline consteval bool isLegalCasFailureOrder(const MemoryOrder success, const MemoryOrder failure) noexcept
{
    // Per the C++ standard: failure must not be `Release` or `AcqRel`,
    // and must not be stronger than `success` in the partial order
    //   `Relaxed`  <=  `Acquire`  <=  `AcqRel`  <=  `SeqCst`
    //   `Relaxed`  <=  `Release`  <=  `AcqRel`  <=  `SeqCst`
    // Acquire and Release are *incomparable*, so e.g.
    // `<Success=Release, Failure=Acquire>` is illegal even though
    // a naive linear ranking would accept it.
    if (failure == MemoryOrder::Release || failure == MemoryOrder::AcqRel)
        return false;

    // After the above check, `failure` is one of {`Relaxed`, `Acquire`, `SeqCst`}.
    if (failure == MemoryOrder::Relaxed)
        return true; // weakest; never stronger than anything

    if (failure == MemoryOrder::Acquire)
        return success == MemoryOrder::Acquire || // exactly equal
               success == MemoryOrder::AcqRel ||  // includes `Acquire`
               success == MemoryOrder::SeqCst;    // strictest

    // `failure` == `MemoryOrder::SeqCst`: only matches `SeqCst` success.
    return success == MemoryOrder::SeqCst;
}


////////////////////////////////////////////////////////////
[[nodiscard]] inline consteval bool isLegalLoadOrder(const MemoryOrder mo) noexcept
{
    return mo == MemoryOrder::Relaxed || mo == MemoryOrder::Acquire || mo == MemoryOrder::SeqCst;
}


////////////////////////////////////////////////////////////
[[nodiscard]] inline consteval bool isLegalStoreOrder(const MemoryOrder mo) noexcept
{
    return mo == MemoryOrder::Relaxed || mo == MemoryOrder::Release || mo == MemoryOrder::SeqCst;
}


////////////////////////////////////////////////////////////
/// \brief Platform wait/notify primitives (defined in AtomicWait.cpp)
///
/// Block the calling thread on a 4- or 8-byte aligned address until
/// the value at that address differs bit-wise from `expected`, or a
/// wake call targets the same address. Spurious wakeups are allowed.
///
////////////////////////////////////////////////////////////
ZA_SYSTEM_API void atomicWait32(const za::U32* addr, za::U32 expected) noexcept;
ZA_SYSTEM_API void atomicWait64(const za::U64* addr, za::U64 expected) noexcept;
ZA_SYSTEM_API void atomicNotifyOne(const void* addr) noexcept;
ZA_SYSTEM_API void atomicNotifyAll(const void* addr) noexcept;

} // namespace priv


////////////////////////////////////////////////////////////
// Convenience-alias generators for `Atomic<T>` member functions.
//
// Spelling out `a.load<MemoryOrder::Relaxed>()` or
// `a.template fetchAdd<MemoryOrder::SeqCst>(1)` everywhere is noisy
// (and the `template` keyword is required at every dependent call
// site). The macros below stamp out a per-order named alias for
// each operation -- e.g. `loadRelaxed()`, `fetchAddSeqCst(v)` --
// that simply forwards to the underlying templated method.
//
////////////////////////////////////////////////////////////
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ZA_PRIV_ATOMIC_LOAD_ALIAS(Suffix)                                           \
    [[nodiscard, gnu::always_inline, gnu::flatten]] T load##Suffix() const noexcept \
    {                                                                               \
        return load<MemoryOrder::Suffix>();                                         \
    }

#define ZA_PRIV_ATOMIC_STORE_ALIAS(Suffix)                                          \
    [[gnu::always_inline, gnu::flatten]] void store##Suffix(const T value) noexcept \
    {                                                                               \
        store<MemoryOrder::Suffix>(value);                                          \
    }

#define ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(Suffix)                                       \
    [[gnu::always_inline, gnu::flatten]] T exchange##Suffix(const T value) noexcept \
    {                                                                               \
        return exchange<MemoryOrder::Suffix>(value);                                \
    }

#define ZA_PRIV_ATOMIC_WAIT_ALIAS(Suffix)                                                       \
    [[gnu::always_inline, gnu::flatten]] void waitOnce##Suffix(const T expected) const noexcept \
        requires(sizeof(T) == 4u || sizeof(T) == 8u)                                            \
    {                                                                                           \
        waitOnce<MemoryOrder::Suffix>(expected);                                                \
    }

#define ZA_PRIV_ATOMIC_WAITUNTIL_ALIAS(Suffix)                                                   \
    [[gnu::always_inline, gnu::flatten]] void waitUntil##Suffix(auto&& predicate) const noexcept \
        requires(sizeof(T) == 4u || sizeof(T) == 8u)                                             \
    {                                                                                            \
        waitUntil<MemoryOrder::Suffix>(static_cast<decltype(predicate)>(predicate));             \
    }

#define ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, Suffix)                          \
    [[gnu::always_inline, gnu::flatten]] T Op##Suffix(const T arg) noexcept \
        requires(za::isIntegral<T> && !za::isSame<T, bool>)                 \
    {                                                                       \
        return Op<MemoryOrder::Suffix>(arg);                                \
    }

#define ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, Suffix)                                     \
    [[gnu::always_inline, gnu::flatten]] T Op##Suffix(const za::PtrDiffT arg) noexcept \
        requires(za::isPointer<T>)                                                     \
    {                                                                                  \
        return Op<MemoryOrder::Suffix>(arg);                                           \
    }

#define ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(Op) \
    ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, Relaxed) \
    ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, Acquire) \
    ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, Release) \
    ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, AcqRel)  \
    ZA_PRIV_ATOMIC_FETCH_INT_ALIAS(Op, SeqCst)

#define ZA_PRIV_ATOMIC_FETCH_PTR_ALL_ORDERS(Op) \
    ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, Relaxed) \
    ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, Acquire) \
    ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, Release) \
    ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, AcqRel)  \
    ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS(Op, SeqCst)
// NOLINTEND(bugprone-macro-parentheses)


////////////////////////////////////////////////////////////
/// \brief Lock-free atomic wrapper around a numerical type
///
/// \tparam T `bool`, integer, pointer, or floating-point type with `sizeof(T) <= 8`
///
////////////////////////////////////////////////////////////
template <typename T>
class [[nodiscard]] Atomic
{
    static_assert(priv::isAtomicSupported<T>, "Atomic<T> requires T to be integral, pointer, or floating-point");

    static_assert(sizeof(T) <= 8u,
                  "Atomic<T> intentionally does not support 16-byte types to avoid pulling in libatomic");

    static_assert(__atomic_always_lock_free(sizeof(T), nullptr),
                  "Atomic<T> requires T to be natively lock-free on the target architecture");

private:
    ////////////////////////////////////////////////////////////
    using Storage = priv::AtomicStorageType<T>;


    ////////////////////////////////////////////////////////////
    // Over-aligning to `sizeof(T)` guarantees lock-freedom on every
    // target the builtins support. Power-of-two sizes only (1/2/4/8).
    alignas(sizeof(T)) Storage m_value;


public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION;


    ////////////////////////////////////////////////////////////
    using value_type = T;


    ////////////////////////////////////////////////////////////
    /// \brief Value-initialize to zero (or null)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr Atomic() noexcept : m_value{}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Initialize with `value` (non-atomic)
    ///
    /// Marked `explicit` to match the "no implicit conversions" rule
    /// of this API. Use brace-init or direct-init at the call site:
    /// `Atomic<int> a{42};` (not `Atomic<int> a = 42;`).
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit constexpr Atomic(const T value) noexcept : m_value{priv::toAtomicStorage<T>(value)}
    {
    }


    ////////////////////////////////////////////////////////////
    Atomic(const Atomic&)            = delete;
    Atomic& operator=(const Atomic&) = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Atomically load and return the current value
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[nodiscard, gnu::always_inline]] T load() const noexcept
    {
        static_assert(priv::isLegalLoadOrder(MO), "load() memory order must be Relaxed, Acquire, or SeqCst");
        return priv::fromAtomicStorage<T>(__atomic_load_n(&m_value, static_cast<int>(MO)));
    }

    ////////////////////////////////////////////////////////////
    ZA_PRIV_ATOMIC_LOAD_ALIAS(Relaxed);
    ZA_PRIV_ATOMIC_LOAD_ALIAS(Acquire);
    ZA_PRIV_ATOMIC_LOAD_ALIAS(SeqCst);


    ////////////////////////////////////////////////////////////
    /// \brief Atomically store `value`
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] void store(const T value) noexcept
    {
        static_assert(priv::isLegalStoreOrder(MO), "store() memory order must be Relaxed, Release, or SeqCst");
        __atomic_store_n(&m_value, priv::toAtomicStorage<T>(value), static_cast<int>(MO));
    }

    ////////////////////////////////////////////////////////////
    ZA_PRIV_ATOMIC_STORE_ALIAS(Relaxed)
    ZA_PRIV_ATOMIC_STORE_ALIAS(Release)
    ZA_PRIV_ATOMIC_STORE_ALIAS(SeqCst)


    ////////////////////////////////////////////////////////////
    /// \brief Atomically replace the stored value with `value` and return the previous value
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T exchange(const T value) noexcept
    {
        return priv::fromAtomicStorage<T>(
            __atomic_exchange_n(&m_value, priv::toAtomicStorage<T>(value), static_cast<int>(MO)));
    }

    ////////////////////////////////////////////////////////////
    ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(Relaxed)
    ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(Acquire)
    ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(Release)
    ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(AcqRel)
    ZA_PRIV_ATOMIC_EXCHANGE_ALIAS(SeqCst)


    ////////////////////////////////////////////////////////////
    /// \brief Strong CAS: replace with `desired` iff the current bit-pattern equals `expected`, updating `expected` on failure
    ///
    /// Equality is **bit-wise**, matching `std::atomic`. Floating-point
    /// callers should be aware: `+0.0f` vs `-0.0f` (different sign bit)
    /// compares unequal, and `NaN` compares equal to a `NaN` with the
    /// same payload. This is rarely what value-equality semantics would
    /// give and is an inherent property of any atomic-CAS over FP.
    ///
    /// \return `true` on success, `false` on failure
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder Success, MemoryOrder Failure>
    [[gnu::always_inline]] bool compareExchangeStrong(T& expected, const T desired) noexcept
    {
        static_assert(priv::isLegalCasFailureOrder(Success, Failure),
                      "compareExchangeStrong: failure order must not be Release/AcqRel "
                      "and must not be stronger than the success order");

        Storage expectedBits = priv::toAtomicStorage<T>(expected);

        const bool ok = __atomic_compare_exchange_n(&m_value,
                                                    &expectedBits,
                                                    priv::toAtomicStorage<T>(desired),
                                                    /* weak */ false,
                                                    static_cast<int>(Success),
                                                    static_cast<int>(Failure));

        if (!ok)
            expected = priv::fromAtomicStorage<T>(expectedBits);

        return ok;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Weak CAS: like `compareExchangeStrong` but may spuriously fail (useful inside a CAS retry loop)
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder Success, MemoryOrder Failure>
    [[gnu::always_inline]] bool compareExchangeWeak(T& expected, const T desired) noexcept
    {
        static_assert(priv::isLegalCasFailureOrder(Success, Failure),
                      "compareExchangeWeak: failure order must not be Release/AcqRel "
                      "and must not be stronger than the success order");

        Storage expectedBits = priv::toAtomicStorage<T>(expected);

        const bool ok = __atomic_compare_exchange_n(&m_value,
                                                    &expectedBits,
                                                    priv::toAtomicStorage<T>(desired),
                                                    /* weak */ true,
                                                    static_cast<int>(Success),
                                                    static_cast<int>(Failure));

        if (!ok)
            expected = priv::fromAtomicStorage<T>(expectedBits);

        return ok;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Atomically add `arg` and return the previous value
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchAdd(const T arg) noexcept
        requires(za::isIntegral<T> && !za::isSame<T, bool>)
    {
        return __atomic_fetch_add(&m_value, arg, static_cast<int>(MO));
    }


    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchSub(const T arg) noexcept
        requires(za::isIntegral<T> && !za::isSame<T, bool>)
    {
        return __atomic_fetch_sub(&m_value, arg, static_cast<int>(MO));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Atomically advance the pointer by `arg` elements and return the previous value
    ///
    /// `arg` is in element units, scaled by `sizeof(*T)` to match `std::atomic` pointer semantics.
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchAdd(const za::PtrDiffT arg) noexcept
        requires(za::isPointer<T>)
    {
        using Pointee = typename priv::AtomicPointee<T>::type;
        return __atomic_fetch_add(&m_value, arg * static_cast<za::PtrDiffT>(sizeof(Pointee)), static_cast<int>(MO));
    }


    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchSub(const za::PtrDiffT arg) noexcept
        requires(za::isPointer<T>)
    {
        using Pointee = typename priv::AtomicPointee<T>::type;
        return __atomic_fetch_sub(&m_value, arg * static_cast<za::PtrDiffT>(sizeof(Pointee)), static_cast<int>(MO));
    }

    ////////////////////////////////////////////////////////////
    // Aliases for fetchAdd / fetchSub: integer overloads (5 orders x 2 ops).
    ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(fetchAdd)
    ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(fetchSub)

    ////////////////////////////////////////////////////////////
    // Aliases for fetchAdd / fetchSub: pointer overloads (5 orders x 2 ops).
    ZA_PRIV_ATOMIC_FETCH_PTR_ALL_ORDERS(fetchAdd)
    ZA_PRIV_ATOMIC_FETCH_PTR_ALL_ORDERS(fetchSub)


    ////////////////////////////////////////////////////////////
    /// \brief Atomically bitwise-AND `arg` into the value and return the previous value (integral types only)
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchAnd(const T arg) noexcept
        requires(za::isIntegral<T> && !za::isSame<T, bool>)
    {
        return __atomic_fetch_and(&m_value, arg, static_cast<int>(MO));
    }


    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchOr(const T arg) noexcept
        requires(za::isIntegral<T> && !za::isSame<T, bool>)
    {
        return __atomic_fetch_or(&m_value, arg, static_cast<int>(MO));
    }


    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline]] T fetchXor(const T arg) noexcept
        requires(za::isIntegral<T> && !za::isSame<T, bool>)
    {
        return __atomic_fetch_xor(&m_value, arg, static_cast<int>(MO));
    }

    ////////////////////////////////////////////////////////////
    // Aliases for fetchAnd / fetchOr / fetchXor (integer-only, 5 orders x 3 ops).
    ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(fetchAnd)
    ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(fetchOr)
    ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS(fetchXor)


    ////////////////////////////////////////////////////////////
    /// \brief Park the calling thread at most once if the stored value still equals `expected`
    ///
    /// Single-shot primitive: parks if the value matches `expected`,
    /// then returns -- the return may be spurious (the value might not
    /// have changed), so callers must re-check in a loop. For the
    /// usual "wait until a condition holds" pattern, use `waitUntil`
    /// which loops on a predicate.
    ///
    /// Differs from `std::atomic::wait`, which loops internally until
    /// it observes a different value; this is the lower-level primitive
    /// `std::atomic::wait` is built on top of.
    ///
    /// Requires `sizeof(T) == 4` or `sizeof(T) == 8` -- these are the
    /// sizes the platform wait primitives (futex / WaitOnAddress /
    /// `wasm.memory.atomic.wait`) operate on.
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    void waitOnce(const T expected) const noexcept
        requires(sizeof(T) == 4u || sizeof(T) == 8u)
    {
        static_assert(priv::isLegalLoadOrder(MO), "waitOnce() memory order must be Relaxed, Acquire, or SeqCst");

        // Quick re-check before going to the kernel -- covers the common
        // "value already changed" case without any syscall.
        if (priv::toAtomicStorage<T>(load<MO>()) != priv::toAtomicStorage<T>(expected))
            return;

        // Two-step conversion of `expected` to the wait-primitive's word type:
        //
        //   1. `toAtomicStorage<T>` -> `Storage` (no-op for int/ptr,
        //      `bit_cast` to U32/U64 for float/double).
        //
        //   2. `BIT_CAST(U32/U64, Storage)` -> futex word type (no-op for
        //      integer `Storage`; pointer-to-integer reinterpret where
        //      `static_cast` would be ill-formed).
        //
        if constexpr (sizeof(T) == 4u)
            priv::atomicWait32(reinterpret_cast<const za::U32*>(&m_value),
                               ZA_BIT_CAST(za::U32, priv::toAtomicStorage<T>(expected)));
        else
            priv::atomicWait64(reinterpret_cast<const za::U64*>(&m_value),
                               ZA_BIT_CAST(za::U64, priv::toAtomicStorage<T>(expected)));
    }

    ////////////////////////////////////////////////////////////
    ZA_PRIV_ATOMIC_WAIT_ALIAS(Relaxed)
    ZA_PRIV_ATOMIC_WAIT_ALIAS(Acquire)
    ZA_PRIV_ATOMIC_WAIT_ALIAS(SeqCst)


    ////////////////////////////////////////////////////////////
    /// \brief Block the calling thread until `predicate(load<MO>())` returns `true`
    ///
    /// Convenience loop on top of `waitOnce`. `predicate` is invoked
    /// with the most recent observed value.
    ///
    ////////////////////////////////////////////////////////////
    template <MemoryOrder MO>
    [[gnu::always_inline, gnu::flatten]] void waitUntil(auto&& predicate) const noexcept
        requires(sizeof(T) == 4u || sizeof(T) == 8u)
    {
        while (true)
        {
            const T val = load<MO>();

            if (predicate(val))
                return;

            waitOnce<MO>(val);
        }
    }

    ////////////////////////////////////////////////////////////
    ZA_PRIV_ATOMIC_WAITUNTIL_ALIAS(Relaxed)
    ZA_PRIV_ATOMIC_WAITUNTIL_ALIAS(Acquire)
    ZA_PRIV_ATOMIC_WAITUNTIL_ALIAS(SeqCst)


    ////////////////////////////////////////////////////////////
    /// \brief Wake one waiter currently blocked in `waitOnce`/`waitUntil`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void notifyOne() noexcept
        requires(sizeof(T) == 4u || sizeof(T) == 8u)
    {
        priv::atomicNotifyOne(static_cast<const void*>(&m_value));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Wake all waiters currently blocked in `waitOnce`/`waitUntil`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void notifyAll() noexcept
        requires(sizeof(T) == 4u || sizeof(T) == 8u)
    {
        priv::atomicNotifyAll(static_cast<const void*>(&m_value));
    }
};


// Pull-down the alias-generator macros once the class body is closed.
#undef ZA_PRIV_ATOMIC_LOAD_ALIAS
#undef ZA_PRIV_ATOMIC_STORE_ALIAS
#undef ZA_PRIV_ATOMIC_EXCHANGE_ALIAS
#undef ZA_PRIV_ATOMIC_WAIT_ALIAS
#undef ZA_PRIV_ATOMIC_WAITUNTIL_ALIAS
#undef ZA_PRIV_ATOMIC_FETCH_INT_ALIAS
#undef ZA_PRIV_ATOMIC_FETCH_PTR_ALIAS
#undef ZA_PRIV_ATOMIC_FETCH_INT_ALL_ORDERS
#undef ZA_PRIV_ATOMIC_FETCH_PTR_ALL_ORDERS


////////////////////////////////////////////////////////////
/// \brief Cross-thread memory fence
///
/// Prevents the compiler and CPU from reordering memory operations
/// across this point, with the strength selected by `MO`.
///
////////////////////////////////////////////////////////////
template <MemoryOrder MO>
[[gnu::always_inline]] inline void atomicThreadFence() noexcept
{
    __atomic_thread_fence(static_cast<int>(MO));
}


////////////////////////////////////////////////////////////
/// \brief Compiler-only fence (does not emit any CPU barrier)
///
/// Useful for synchronizing with a signal handler running on the
/// same thread without paying for an interprocessor barrier.
///
////////////////////////////////////////////////////////////
template <MemoryOrder MO>
[[gnu::always_inline]] inline void atomicSignalFence() noexcept
{
    __atomic_signal_fence(static_cast<int>(MO));
}

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Lightweight `std::atomic` substitute built on `__atomic_*` builtins
///
/// `Atomic<T>` provides lock-free atomic operations for numerical
/// types (integers, `bool`, `char`, pointers, and floating-point) on
/// every target where the GCC/Clang `__atomic_*` builtins are
/// available (x86, x86-64, ARM, ARM64, Emscripten/wasm, ...) without
/// pulling in the `<atomic>` header or the libatomic runtime library.
///
/// Differences vs. `std::atomic`:
///
/// - `sizeof(T) <= 8`: 16-byte atomics would pull in libatomic.
///
/// - `T` must be reported lock-free by `__atomic_always_lock_free`.
///
/// - **Memory order is a template parameter, never defaulted.**
///   Callers always spell out `load<MemoryOrder::Relaxed>()`, etc.
///   This produces strictly better codegen and lets us `static_assert`
///   constraints (e.g. CAS failure order).
///
/// - No implicit conversions, no operator overloads -- call `load()`,
///   `store()`, `fetchAdd()`, etc. explicitly. The value constructor
///   is `explicit`.
///
/// - `Atomic<bool>` exposes load/store/exchange/CAS only (no
///   `fetchAdd` / `fetchAnd` / etc.), matching `std::atomic<bool>`.
///
/// - Floating-point atomics expose only `load`/`store`/`exchange`/CAS,
///   matching `std::atomic` prior to C++20.
///
/// Emscripten note: `wait`/`notifyOne`/`notifyAll` only work in
/// builds with shared memory and pthreads (`-pthread`,
/// `-sSHARED_MEMORY`), and rely on `Atomics.wait` which is unavailable
/// on the browser main thread -- so a blocking `wait` issued from the
/// main thread will busy-spin or deadlock. Use `-sPROXY_TO_PTHREAD`
/// (or otherwise ensure waits happen on a worker) if you need
/// blocking semantics on the web.
///
////////////////////////////////////////////////////////////
