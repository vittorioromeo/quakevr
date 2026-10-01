// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Chrono/Clock.hpp"

#include "Zancle/Chrono/Time.hpp"

#include "Zancle/Base/IntTypes.hpp"

#if defined(ZA_SYSTEM_WINDOWS)
    #include "Zancle/Base/WindowsHeader.hpp"
#else
    #include <time.h>
#endif


namespace za::priv
{
namespace
{
////////////////////////////////////////////////////////////
/// \brief Read the OS monotonic clock, in nanoseconds since an unspecified epoch
///
/// The clocks are the ones `std::chrono::steady_clock` uses on each
/// platform, so behavior is unchanged from the previous `<chrono>`-based
/// implementation:
///
/// - Windows: `QueryPerformanceCounter` (as in MSVC's STL, and in MinGW's
///   `clock_gettime(CLOCK_MONOTONIC)`).
/// - macOS/iOS: `CLOCK_UPTIME_RAW` (as in libc++); does not advance while
///   the system is asleep.
/// - Linux, BSDs, Android, Emscripten: `clock_gettime(CLOCK_MONOTONIC)` (as
///   in libstdc++ and libc++); on Emscripten this is `performance.now()`,
///   whose resolution is coarsened by browsers (typically 5-100us).
///
/// Android builds can opt into `CLOCK_BOOTTIME` with
/// `ZA_ANDROID_USE_SUSPEND_AWARE_CLOCK`, which keeps advancing while the
/// device is suspended.
///
/// For more information on Linux clocks visit:
/// https://man7.org/linux/man-pages/man2/clock_gettime.2.html
///
////////////////////////////////////////////////////////////
// TODO P0: test on Emscripten in browsers, which coarsen `performance.now()` (only verified under node,
//          with Emscripten 6.0.10: 1us resolution, monotonic, agrees with `std::chrono::steady_clock`)
[[nodiscard]] I64 monotonicNanoseconds() noexcept
{
#if defined(ZA_SYSTEM_WINDOWS)

    // Fixed at boot, and cannot fail on Windows XP or later
    static const I64 frequency = []
    {
        LARGE_INTEGER result;
        QueryPerformanceFrequency(&result);
        return static_cast<I64>(result.QuadPart);
    }();

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    const auto ticks = static_cast<I64>(counter.QuadPart);

    // Windows 10+ always reports 10 MHz: avoid the divisions
    if (frequency == 10'000'000) [[likely]]
        return ticks * 100;

    // Split to avoid overflowing `ticks * 1e9` (the remainder is below `frequency`)
    return (ticks / frequency) * 1'000'000'000 + (ticks % frequency) * 1'000'000'000 / frequency;

#elif defined(ZA_SYSTEM_MACOS) || defined(ZA_SYSTEM_IOS)

    return static_cast<I64>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW));

#else

    #if defined(ZA_SYSTEM_ANDROID) && defined(ZA_ANDROID_USE_SUSPEND_AWARE_CLOCK)
    constexpr clockid_t clockId = CLOCK_BOOTTIME;
    #else
    constexpr clockid_t clockId = CLOCK_MONOTONIC;
    #endif

    timespec ts{};
    clock_gettime(clockId, &ts);

    return static_cast<I64>(ts.tv_sec) * 1'000'000'000 + static_cast<I64>(ts.tv_nsec);

#endif
}


////////////////////////////////////////////////////////////
/// \brief `stopPoint` value of a running clock (never returned by `monotonicNanoseconds`)
///
////////////////////////////////////////////////////////////
constexpr I64 clockRunning = -9'223'372'036'854'775'807 - 1;


////////////////////////////////////////////////////////////
[[nodiscard]] constexpr Time nanosecondsToTime(const I64 nanoseconds)
{
    return microseconds(nanoseconds / 1000); // truncates toward zero, like `std::chrono::duration_cast`
}


////////////////////////////////////////////////////////////
/// \brief Report the time elapsed from `refPoint` to `endPoint`, and move `refPoint` so that
///        a new measurement starts at `now`
///
/// `za::Time` only holds whole microseconds: the unreported sub-microsecond
/// remainder is carried over into the new measurement instead of being
/// dropped, so that the times reported by consecutive restarts add up to
/// the real elapsed time (instead of losing ~0.5us per restart on average).
///
////////////////////////////////////////////////////////////
[[nodiscard]] Time restartMeasurement(I64& refPoint, const I64 endPoint, const I64 now)
{
    const I64 elapsed  = endPoint - refPoint;
    const I64 reported = elapsed / 1000 * 1000;

    refPoint = now - (elapsed - reported);
    return microseconds(reported / 1000);
}

} // namespace
} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
Clock::Clock() : m_refPoint{priv::monotonicNanoseconds()}, m_stopPoint{priv::clockRunning}
{
}


////////////////////////////////////////////////////////////
Time Clock::getElapsedTime() const
{
    return priv::nanosecondsToTime(getElapsedNanoseconds());
}


////////////////////////////////////////////////////////////
I64 Clock::getElapsedNanoseconds() const noexcept
{
    const I64 endPoint = isRunning() ? priv::monotonicNanoseconds() : m_stopPoint;
    return endPoint - m_refPoint;
}


////////////////////////////////////////////////////////////
bool Clock::isRunning() const
{
    return m_stopPoint == priv::clockRunning;
}


////////////////////////////////////////////////////////////
void Clock::start()
{
    if (isRunning())
        return;

    m_refPoint += priv::monotonicNanoseconds() - m_stopPoint;
    m_stopPoint = priv::clockRunning;
}


////////////////////////////////////////////////////////////
void Clock::stop()
{
    if (!isRunning())
        return;

    m_stopPoint = priv::monotonicNanoseconds();
}


////////////////////////////////////////////////////////////
Time Clock::restart()
{
    // Single clock reading: a second one would lose the time between the two
    const I64  now     = priv::monotonicNanoseconds();
    const Time elapsed = priv::restartMeasurement(m_refPoint, isRunning() ? now : m_stopPoint, now);

    m_stopPoint = priv::clockRunning;
    return elapsed;
}


////////////////////////////////////////////////////////////
Time Clock::reset()
{
    const I64  now     = priv::monotonicNanoseconds();
    const Time elapsed = priv::restartMeasurement(m_refPoint, isRunning() ? now : m_stopPoint, now);

    m_stopPoint = now;
    return elapsed;
}


////////////////////////////////////////////////////////////
Time Clock::now()
{
    return priv::nanosecondsToTime(priv::monotonicNanoseconds());
}


////////////////////////////////////////////////////////////
I64 Clock::nowNanoseconds() noexcept
{
    return priv::monotonicNanoseconds();
}

} // namespace za
