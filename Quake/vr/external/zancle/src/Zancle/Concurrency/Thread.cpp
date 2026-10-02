// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers (project)
////////////////////////////////////////////////////////////
#include "Zancle/Concurrency/Thread.hpp"

#include "Zancle/Config.hpp"

#include "Zancle/Concurrency/Atomic.hpp"

#include "Zancle/Chrono/Time.hpp"

#include "Zancle/Base/Abort.hpp"
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Launder.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"


////////////////////////////////////////////////////////////
// Platform backend selection
////////////////////////////////////////////////////////////
#if defined(ZA_SYSTEM_LINUX) || defined(ZA_SYSTEM_ANDROID) || defined(ZA_SYSTEM_MACOS) || defined(ZA_SYSTEM_IOS) || \
    defined(ZA_SYSTEM_FREEBSD) || defined(ZA_SYSTEM_OPENBSD) || defined(ZA_SYSTEM_NETBSD) ||                        \
    defined(ZA_SYSTEM_EMSCRIPTEN)
    #define ZA_THREAD_POSIX 1
#elif defined(ZA_SYSTEM_WINDOWS)
    #define ZA_THREAD_WIN32 1
#else
    #error "za::Thread: no thread backend implemented for this platform"
#endif


////////////////////////////////////////////////////////////
// Headers (platform)
////////////////////////////////////////////////////////////
#if ZA_THREAD_POSIX

    #include <pthread.h>
    #include <sched.h>
    #include <unistd.h>

    #include <cerrno>
    #include <ctime>

    #if defined(ZA_SYSTEM_MACOS) || defined(ZA_SYSTEM_IOS)
        #include <sys/sysctl.h>
    #endif

#elif ZA_THREAD_WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #include "Zancle/Base/WindowsHeader.hpp"

    #include <mmsystem.h>
    #include <process.h>
    #include <synchapi.h>

#endif


namespace za
{
namespace
{
////////////////////////////////////////////////////////////
// Id of the calling thread (`0` until first assigned)
////////////////////////////////////////////////////////////
thread_local za::U64 tlCurrentThreadId{0u};


////////////////////////////////////////////////////////////
// Reinterpret the opaque `m_native[16]` byte buffer as the
// platform handle. The `static_assert`s below catch any ABI
// surprise (pthread_t larger than expected, etc.) at compile time.
////////////////////////////////////////////////////////////
#if ZA_THREAD_POSIX

using NativeHandle = pthread_t;

static_assert(sizeof(NativeHandle) <= 16u);
static_assert(alignof(NativeHandle) <= alignof(za::U64));

#elif ZA_THREAD_WIN32

    #ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00'00'00'02 // missing from older SDKs/MinGW headers
    #endif

////////////////////////////////////////////////////////////
// Per-thread high-resolution waitable timer used by `sleepFor`
// (`handle` is null on Windows versions older than 10 1803)
////////////////////////////////////////////////////////////
struct HighResolutionTimer
{
    HANDLE handle = CreateWaitableTimerExW(/* attributes */ nullptr,
                                           /* name       */ nullptr,
                                           CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                           TIMER_ALL_ACCESS);

    HighResolutionTimer() = default;

    HighResolutionTimer(const HighResolutionTimer&)            = delete;
    HighResolutionTimer& operator=(const HighResolutionTimer&) = delete;

    ~HighResolutionTimer()
    {
        if (handle != nullptr)
            CloseHandle(handle);
    }
};


////////////////////////////////////////////////////////////
struct NativeHandle
{
    HANDLE handle;
};

static_assert(sizeof(NativeHandle) <= 16u);
static_assert(alignof(NativeHandle) <= alignof(za::U64));

#endif


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline NativeHandle& asNative(void* const p) noexcept
{
    // `m_native` is a raw byte buffer; `pthread_create` / `_beginthreadex`
    // construct the `NativeHandle` in place. `ZA_LAUNDER_CAST` tells
    // the compiler to treat the bytes as a fresh `NativeHandle*` (i.e. drop
    // any value-tracking that assumed it was still `unsigned char[16]`).
    return *ZA_LAUNDER_CAST(NativeHandle*, p);
}


////////////////////////////////////////////////////////////
// Monotonic counter for `ThreadId`. The first time a given
// thread asks for its id, it grabs the next value here.
////////////////////////////////////////////////////////////
constinit Atomic<za::U64> gNextThreadId{1u};


////////////////////////////////////////////////////////////
// Run the user callable + free the single-block entry. The
// header instantiates `entry->deinit` per `F` and embeds the
// payload's offset+alignment into the thunk, so this function
// stays type-erased.
//
// `noexcept` so the underlying OS doesn't see a stack unwind
// cross the trampoline frame; the user routine is itself
// `noexcept`, so an escaping exception in user code triggers
// `std::terminate` before reaching the platform thunk.
////////////////////////////////////////////////////////////
inline void runEntry(priv::ThreadEntry* const entry) noexcept
{
    // Establish this thread's id BEFORE running user code, so
    // the user's first `ThisThread::getId()` returns the same
    // value as `Thread::getId()` on the spawner side.
    tlCurrentThreadId = entry->id;

    // Runs the callable, destroys it, and frees the entry block.
    entry->deinit(entry, /* runFirst */ true);
}


////////////////////////////////////////////////////////////
// Per-platform start-routine signature wraps the noexcept body.
////////////////////////////////////////////////////////////
#if ZA_THREAD_POSIX

extern "C" void* threadStartRoutine(void* arg)
{
    runEntry(static_cast<priv::ThreadEntry*>(arg));
    return nullptr;
}

#elif ZA_THREAD_WIN32

extern "C" unsigned __stdcall threadStartRoutine(void* arg)
{
    runEntry(static_cast<priv::ThreadEntry*>(arg));
    return 0u;
}

#endif

} // namespace


////////////////////////////////////////////////////////////
Thread::Thread(int, priv::ThreadEntry* entry)
{
    // `entry` already owns the user callable (built in the header
    // factory). We just stamp its id and hand it to the kernel.
    entry->id = gNextThreadId.fetchAddRelaxed(1u);
    m_id      = entry->id;

#if ZA_THREAD_POSIX

    if (const int rc = pthread_create(&asNative(m_native), /* attr */ nullptr, &threadStartRoutine, entry); rc != 0)
    {
        // Destroys the user callable and frees the single-block entry
        // (without running the callable, since spawn never happened).
        entry->deinit(entry, /* runFirst */ false);
        za::abort();
    }

#elif ZA_THREAD_WIN32

    auto& native  = asNative(m_native);
    native.handle = reinterpret_cast<HANDLE>(
        _beginthreadex(/* security    */ nullptr,
                       /* stack_size  */ 0u,
                       &threadStartRoutine,
                       entry,
                       /* initflag    */ 0u,
                       /* thrdaddr    */ nullptr));

    if (native.handle == nullptr)
    {
        // Same single-block cleanup as the POSIX failure path above.
        entry->deinit(entry, /* runFirst */ false);
        za::abort();
    }

#endif

    m_joinable = true;
}


////////////////////////////////////////////////////////////
void Thread::takeFrom(Thread& dst, Thread& other) noexcept
{
    ZA_MEMCPY(dst.m_native, other.m_native, sizeof(dst.m_native));
    dst.m_id       = other.m_id;
    dst.m_joinable = other.m_joinable;

    ZA_MEMSET(other.m_native, 0, sizeof(other.m_native));
    other.m_id       = 0u;
    other.m_joinable = false;
}


////////////////////////////////////////////////////////////
Thread::Thread(Thread&& other) noexcept
{
    takeFrom(*this, other);
}


////////////////////////////////////////////////////////////
Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this == &other)
        return *this;

    // `std::jthread`-style semantics: an existing joinable thread is
    // implicitly joined before the slot is overwritten.
    if (m_joinable)
        join();

    takeFrom(*this, other);
    return *this;
}


////////////////////////////////////////////////////////////
Thread::~Thread()
{
    if (m_joinable)
        join();
}


////////////////////////////////////////////////////////////
void Thread::join()
{
    ZA_ASSERT(m_joinable);
    ZA_ASSERT(ThisThread::getId() != getId() && "a thread cannot join itself (deadlock)");

#if ZA_THREAD_POSIX

    pthread_join(asNative(m_native), /* retval */ nullptr);

#elif ZA_THREAD_WIN32

    auto& native = asNative(m_native);
    WaitForSingleObject(native.handle, INFINITE);
    CloseHandle(native.handle);
    native.handle = nullptr;

#endif

    m_joinable = false;
}


////////////////////////////////////////////////////////////
void Thread::detach()
{
    ZA_ASSERT(m_joinable);

#if ZA_THREAD_POSIX

    pthread_detach(asNative(m_native));

#elif ZA_THREAD_WIN32

    auto& native = asNative(m_native);
    CloseHandle(native.handle);
    native.handle = nullptr;

#endif

    m_joinable = false;
}


////////////////////////////////////////////////////////////
bool Thread::joinable() const noexcept
{
    return m_joinable;
}


////////////////////////////////////////////////////////////
ThreadId Thread::getId() const noexcept
{
    return ThreadId{m_id};
}


////////////////////////////////////////////////////////////
unsigned int Thread::hardwareConcurrency() noexcept
{
#if ZA_THREAD_POSIX

    const long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<unsigned int>(n) : 0u;

#elif ZA_THREAD_WIN32

    // Unlike `GetSystemInfo`, counts the processors of every group (not just the first 64)
    return static_cast<unsigned int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));

#endif
}


////////////////////////////////////////////////////////////
unsigned int Thread::usableHardwareConcurrency() noexcept
{
#if defined(__linux__) && defined(CPU_COUNT) // GNU extensions (glibc, musl, bionic)
    // The processors this process may run on (e.g. limited by `taskset`, cpusets, or containers)
    cpu_set_t set;
    if (sched_getaffinity(/* calling process */ 0, sizeof(set), &set) == 0)
        if (const int n = CPU_COUNT(&set); n > 0)
            return static_cast<unsigned int>(n);
#endif

    return hardwareConcurrency();
}


////////////////////////////////////////////////////////////
ThreadId ThisThread::getId() noexcept
{
    if (tlCurrentThreadId == 0u) [[unlikely]]
        tlCurrentThreadId = gNextThreadId.fetchAddRelaxed(1u);

    return ThreadId{tlCurrentThreadId};
}


////////////////////////////////////////////////////////////
void ThisThread::yield() noexcept
{
#if ZA_THREAD_POSIX
    sched_yield();
#elif ZA_THREAD_WIN32
    SwitchToThread();
#endif
}


////////////////////////////////////////////////////////////
void ThisThread::sleepFor(Time duration)
{
    if (duration <= Time{})
        return;

#if ZA_THREAD_POSIX

    const za::I64 usecs = duration.asMicroseconds();

    timespec ts{};
    ts.tv_sec  = static_cast<time_t>(usecs / 1'000'000);
    ts.tv_nsec = static_cast<long>((usecs % 1'000'000) * 1000);

    // Restart on EINTR using the remaining time the kernel writes back.
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
    }

#elif ZA_THREAD_WIN32

    // `std::this_thread::sleep_for` is intentionally not used: it
    // produces inconsistent results under MinGW-w64.
    const za::I64 usecs = duration.asMicroseconds();

    // Preferred: a high-resolution waitable timer (Windows 10 1803+),
    // which never wakes up early and needs no global timer resolution change
    thread_local const HighResolutionTimer timer;

    if (timer.handle != nullptr)
    {
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -usecs * 10; // negative: relative, in 100ns units

        if (SetWaitableTimer(timer.handle, &dueTime, /* period */ 0, nullptr, nullptr, /* resume */ FALSE) &&
            WaitForSingleObject(timer.handle, INFINITE) == WAIT_OBJECT_0)
            return;
    }

    // Fallback: `Sleep` with the system timer resolution bumped so that
    // it honors short durations more accurately. The duration is rounded
    // up to whole milliseconds, so that the sleep is never shorter.
    static const UINT periodMin = []
    {
        TIMECAPS tc;
        timeGetDevCaps(&tc, sizeof(TIMECAPS));
        return tc.wPeriodMin;
    }();

    constexpr za::I64 maxSleepMs = 0xFF'FF'FF'FE; // `INFINITE` (0xFFFFFFFF) must be avoided
    const za::I64     ms         = (usecs + 999) / 1000;

    timeBeginPeriod(periodMin);
    ::Sleep(static_cast<DWORD>(ms < maxSleepMs ? ms : maxSleepMs));
    timeEndPeriod(periodMin);

#endif
}

} // namespace za
