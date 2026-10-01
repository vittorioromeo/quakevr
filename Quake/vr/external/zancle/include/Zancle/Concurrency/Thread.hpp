#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

#include "Zancle/Config.hpp"

#include "Zancle/Base/FwdStdAlignedNewDelete.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/Decay.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"


////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////
namespace za
{
class Time;
}


////////////////////////////////////////////////////////////
namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Single-allocation control block for a spawned thread
///
/// One heap block holds this header at offset 0 and the user's
/// callable `F` placed at the next properly-aligned offset.
/// `deinit` is a thunk instantiated per `F` that knows where the
/// payload sits and how to destroy + free the whole allocation.
///
/// `runFirst == true`: run the callable, destroy it, free the block
///                    (the success path: thread thunk on completion).
/// `runFirst == false`: destroy the callable without running it,
///                     free the block (the spawn-failure path).
///
////////////////////////////////////////////////////////////
struct ThreadEntry
{
    za::U64 id;
    void (*deinit)(ThreadEntry*, bool runFirst) noexcept;
};

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Opaque, comparable identifier for a thread
///
/// Wraps a per-thread monotonic counter assigned the first time
/// a thread observes its own id. Stable across the lifetime of
/// the thread; never re-used (the counter is 64-bit). A
/// default-constructed `ThreadId` represents "no thread".
///
////////////////////////////////////////////////////////////
class ThreadId
{
public:
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr ThreadId() noexcept = default;


    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr explicit ThreadId(za::U64 value) noexcept : m_value{value}
    {
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr za::U64 value() const noexcept
    {
        return m_value;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr bool operator==(const ThreadId&) const noexcept = default;

private:
    za::U64 m_value{0u};
};


////////////////////////////////////////////////////////////
/// \brief Thread of execution; lightweight `std::jthread` substitute
///
/// Move-only. Default-constructed instances are not joinable. A
/// joinable instance whose destructor (or move-assignment target)
/// runs without a prior `join()`/`detach()` is **implicitly
/// `join()`'d** -- matching `std::jthread`'s automatic-join
/// behavior, not `std::thread`'s abort-on-drop. This means the
/// callable is allowed to run to its natural completion when the
/// `Thread` instance dies.
///
/// Cooperative cancellation (`stop_token` / `request_stop`) is
/// not yet implemented; if you need to interrupt the worker, pass
/// your own `Atomic<bool>` flag through the captured closure.
///
////////////////////////////////////////////////////////////
class ZA_SYSTEM_API Thread
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor; produces a non-joinable thread
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Thread() noexcept = default;


    ////////////////////////////////////////////////////////////
    /// \brief Spawn a new thread that will invoke `callable()`
    ///
    /// Takes ownership of `callable` (heap-allocated for the duration
    /// of the worker thread). Throws no C++ exceptions -- uses
    /// `za::abort()` if the OS refuses to spawn.
    ///
    /// Implementation detail: each unique callable type instantiates
    /// its own thunk, but the `Thread` ABI itself is type-erased.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
        requires(!za::isSame<za::RemoveCVRef<F>, Thread>)
    [[nodiscard]] explicit Thread(F&& callable) :
        // Decayed, so that a plain function (deduced as a reference to a function type) is stored as a pointer
        Thread(/* opaque tag */ int{}, allocateEntry<za::Decay<F>>(static_cast<F&&>(callable)))
    {
    }


    ////////////////////////////////////////////////////////////
    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;


    ////////////////////////////////////////////////////////////
    Thread(Thread&& other) noexcept;
    Thread& operator=(Thread&& other) noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Destructor; implicitly `join()`s if still joinable
    ///
    /// Matches `std::jthread`. The blocking join happens at
    /// scope exit -- design for it (i.e. don't capture references
    /// to long-lived state in the worker without thinking).
    ///
    ////////////////////////////////////////////////////////////
    ~Thread();


    ////////////////////////////////////////////////////////////
    /// \brief Block until the thread finishes execution
    ///
    /// After this returns, `joinable()` is `false`.
    ///
    ////////////////////////////////////////////////////////////
    void join();


    ////////////////////////////////////////////////////////////
    /// \brief Release ownership; the thread runs until completion
    ///
    /// After this returns, `joinable()` is `false` and the kernel
    /// thread cleans itself up when its callable returns.
    ///
    ////////////////////////////////////////////////////////////
    void detach();


    ////////////////////////////////////////////////////////////
    /// \brief Whether the thread is currently joinable
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool joinable() const noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Identifier of the wrapped thread, or `{}` when not joinable
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ThreadId getId() const noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Number of concurrent threads supported by the
    ///        implementation, or 0 if the value is undetermined
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static unsigned int hardwareConcurrency() noexcept;


private:
    ////////////////////////////////////////////////////////////
    /// \brief Type-erased private constructor used by the templated public one
    ///
    /// The leading `int` tag distinguishes this overload from the
    /// public templated constructor (avoiding any chance of
    /// overload-resolution ambiguity). The .cpp records `entry`,
    /// stamps its id, and spawns the kernel thread; on spawn
    /// failure it calls `entry->deinit(entry, /*runFirst*/ false)`
    /// to clean up the allocated callable + entry block.
    ///
    ////////////////////////////////////////////////////////////
    Thread(int, priv::ThreadEntry* entry);


    ////////////////////////////////////////////////////////////
    /// \brief Single-allocation factory for a `ThreadEntry`
    ///
    /// Allocates one heap block big enough to hold both the
    /// `ThreadEntry` header and the user's callable `FT` payload,
    /// places both, and stores a per-`FT` thunk that knows the
    /// layout for later destruction + free.
    ///
    ////////////////////////////////////////////////////////////
    template <typename FT, typename FFwd>
    [[nodiscard]] static priv::ThreadEntry* allocateEntry(FFwd&& callable)
    {
        constexpr za::SizeT alignment = alignof(FT) > alignof(priv::ThreadEntry) ? alignof(FT) : alignof(priv::ThreadEntry);

        // Round `sizeof(ThreadEntry)` up to `alignof(FT)` so the payload that follows starts at a properly-aligned offset.
        constexpr za::SizeT fOffset = (sizeof(priv::ThreadEntry) + alignof(FT) - 1u) / alignof(FT) * alignof(FT);

        constexpr za::SizeT totalSize = fOffset + sizeof(FT);

        void* const mem = ::operator new(totalSize, std::align_val_t{alignment});

        auto* const entry = ZA_PLACEMENT_NEW(mem)
            priv::ThreadEntry{/* id (overwritten by Thread(int, ...) below) */ 0u,
                              /* deinit */
                              +[](priv::ThreadEntry* const e, const bool runFirst) noexcept
        {
            auto* const fn = reinterpret_cast<FT*>(reinterpret_cast<char*>(e) + fOffset);

            if (runFirst)
                (*fn)();

            fn->~FT();
            ::operator delete(static_cast<void*>(e), std::align_val_t{alignment});
        }};

        ZA_PLACEMENT_NEW(reinterpret_cast<char*>(mem) + fOffset) FT(static_cast<FFwd&&>(callable));
        return entry;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Steal `other`'s state into a non-joinable `dst` slot
    ///
    /// `dst` must already be non-joinable (default-constructed,
    /// post-`join()`/`detach()`, or just-cleared). Used by both
    /// move constructor and move-assignment to dedupe the
    /// memcpy + zero-source body.
    ///
    ////////////////////////////////////////////////////////////
    static void takeFrom(Thread& dst, Thread& other) noexcept;


    ////////////////////////////////////////////////////////////
    // Opaque storage for the platform native handle.
    //
    // - POSIX:    `pthread_t`     (8 bytes on every supported libc).
    // - Win32:    `HANDLE` (8 bytes).
    // - 16 bytes / 8-byte alignment covers both with room to spare.
    //
    // The .cpp `static_assert`s sizeof / alignof against the real
    // platform type, so an ABI mismatch is a compile-time error.
    //
    ////////////////////////////////////////////////////////////
    alignas(za::U64) unsigned char m_native[16]{};
    za::U64 m_id{0u};
    bool    m_joinable{false};
};


////////////////////////////////////////////////////////////
/// \brief Static functions operating on the calling thread
///
/// Mirrors `std::this_thread`, exposed as a struct of static
/// methods (rather than a namespace) for consistency with the
/// CamelCase style of the surrounding `za::` API.
///
////////////////////////////////////////////////////////////
struct ThisThread
{
    ////////////////////////////////////////////////////////////
    /// \brief Identifier of the calling thread
    ///
    /// First call from a given thread runs a thread-local
    /// initialization that fetches a globally-unique id from a
    /// monotonic counter. Subsequent calls are a single TLS load.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static ZA_SYSTEM_API ThreadId getId() noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Hint to the scheduler to switch to another runnable thread
    ///
    ////////////////////////////////////////////////////////////
    static ZA_SYSTEM_API void yield() noexcept;


    ////////////////////////////////////////////////////////////
    /// \brief Block the calling thread for at least `duration`
    ///
    /// Forwards to the platform sleep primitive (`nanosleep` on
    /// POSIX, `Sleep` plus `timeBeginPeriod`/`timeEndPeriod` on
    /// Windows for sub-millisecond resolution). A non-positive
    /// `duration` returns immediately.
    ///
    /// Note: `std::this_thread::sleep_for` is intentionally not
    /// used here; it produces inconsistent sleep durations under
    /// MinGW-w64.
    ///
    ////////////////////////////////////////////////////////////
    static ZA_SYSTEM_API void sleepFor(Time duration);
};

} // namespace za
