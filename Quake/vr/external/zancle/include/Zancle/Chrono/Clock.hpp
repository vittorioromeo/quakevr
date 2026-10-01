#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

#include "Zancle/Config.hpp"

#include "Zancle/Base/IntTypes.hpp"


////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////
namespace za
{
class Time;
} // namespace za


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Utility class that measures the elapsed time
///
/// The clock starts automatically after being constructed.
///
////////////////////////////////////////////////////////////
class [[nodiscard]] ZA_SYSTEM_API Clock
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct and immediately start the clock
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit Clock();

    ////////////////////////////////////////////////////////////
    /// \brief Time elapsed since construction or the last `restart()`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Time getElapsedTime() const;

    ////////////////////////////////////////////////////////////
    /// \brief `true` if the clock is currently running
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isRunning() const;

    ////////////////////////////////////////////////////////////
    /// \brief Start the clock
    ///
    /// \see `stop`
    ///
    ////////////////////////////////////////////////////////////
    void start();

    ////////////////////////////////////////////////////////////
    /// \brief Stop the clock
    ///
    /// \see `start`
    ///
    ////////////////////////////////////////////////////////////
    void stop();

    ////////////////////////////////////////////////////////////
    /// \brief Reset the time counter to zero, leaving the clock running
    ///
    /// \return Elapsed time before the reset
    ///
    /// \see `reset`
    ///
    ////////////////////////////////////////////////////////////
    Time restart();

    ////////////////////////////////////////////////////////////
    /// \brief Reset the time counter to zero, leaving the clock paused
    ///
    /// \return Elapsed time before the reset
    ///
    /// \see `restart`
    ///
    ////////////////////////////////////////////////////////////
    Time reset();

    ////////////////////////////////////////////////////////////
    /// \brief Absolute time stamp from the OS's monotonic clock
    ///
    /// Not relative to any `za::Clock` instance; mainly useful for
    /// measuring intervals between two `now()` calls.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static Time now();

private:
    ////////////////////////////////////////////////////////////
    // Member data
    //
    // Readings of the OS monotonic clock, in nanoseconds since an
    // unspecified epoch, on every platform: the platform-specific
    // code stays in `Clock.cpp`.
    ////////////////////////////////////////////////////////////
    za::I64 m_refPoint;  //!< Time of last reset
    za::I64 m_stopPoint; //!< Time of last stop, or a sentinel value while running
};

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::Clock
/// \ingroup system
///
/// `za::Clock` is a lightweight class for measuring time.
///
/// It provides the most precise time that the underlying
/// OS can achieve (generally microseconds or nanoseconds).
/// It also ensures monotonicity, which means that the returned
/// time can never go backward, even if the system time is
/// changed.
///
/// Usage example:
/// \code
/// za::Clock clock;
/// ...
/// Time time1 = clock.getElapsedTime();
/// ...
/// Time time2 = clock.restart();
/// ...
/// Time time3 = clock.reset();
/// \endcode
///
/// The `za::Time` value returned by the clock can then be
/// converted to a number of seconds, milliseconds or even
/// microseconds.
///
/// \see `za::Time`
///
////////////////////////////////////////////////////////////
