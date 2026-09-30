#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/IntTypes.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Represents a time value
///
////////////////////////////////////////////////////////////
class Time
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Construct a zero-valued time
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr Time() = default;


    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of seconds
    ///
    /// \see `asMilliseconds`, `asMicroseconds`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr float asSeconds() const
    {
        return static_cast<float>(m_microseconds) / 1'000'000.f;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of milliseconds
    ///
    /// \see `asSeconds`, `asMicroseconds`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr za::I64 asMilliseconds() const
    {
        return m_microseconds / 1000;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of microseconds
    ///
    /// \see `asSeconds`, `asMilliseconds`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr za::I64 asMicroseconds() const
    {
        return m_microseconds;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Equality comparison of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool operator==(const Time& rhs) const = default;


    ////////////////////////////////////////////////////////////
    /// \brief Less-than comparison of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator<(const Time lhs, const Time rhs)
    {
        return lhs.m_microseconds < rhs.m_microseconds;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Greater-than comparison of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator>(const Time lhs, const Time rhs)
    {
        return lhs.m_microseconds > rhs.m_microseconds;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Less-than-or-equal comparison of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator<=(const Time lhs, const Time rhs)
    {
        return lhs.m_microseconds <= rhs.m_microseconds;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Greater-than-or-equal comparison of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator>=(const Time lhs, const Time rhs)
    {
        return lhs.m_microseconds >= rhs.m_microseconds;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Negation of a time
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator-(const Time rhs)
    {
        return Time(-rhs.m_microseconds);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Sum of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator+(const Time lhs, const Time rhs)
    {
        return Time(lhs.m_microseconds + rhs.m_microseconds);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound addition of two times
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator+=(Time& lhs, const Time rhs)
    {
        lhs.m_microseconds += rhs.m_microseconds;
        return lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Difference of two times
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator-(const Time lhs, const Time rhs)
    {
        return Time(lhs.m_microseconds - rhs.m_microseconds);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound subtraction of two times
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator-=(Time& lhs, const Time rhs)
    {
        lhs.m_microseconds -= rhs.m_microseconds;
        return lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Scale a time by a floating-point factor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator*(const Time lhs, const float rhs)
    {
        return Time(floatToMicroseconds(static_cast<float>(lhs.m_microseconds) * rhs));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Scale a time by an integer factor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator*(const Time lhs, const za::I64 rhs)
    {
        return Time(lhs.m_microseconds * rhs);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Scale a time by a floating-point factor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator*(const float lhs, const Time rhs)
    {
        return rhs * lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Scale a time by an integer factor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator*(const za::I64 lhs, const Time rhs)
    {
        return rhs * lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound scale of a time by a floating-point factor
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator*=(Time& lhs, const float rhs)
    {
        return lhs = lhs * rhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound scale of a time by an integer factor
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator*=(Time& lhs, const za::I64 rhs)
    {
        return lhs = lhs * rhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Divide a time by a floating-point divisor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator/(const Time lhs, const float rhs)
    {
        ZA_ASSERT(rhs != 0 && "Time::operator/ cannot divide by 0");
        return Time(floatToMicroseconds(static_cast<float>(lhs.m_microseconds) / rhs));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Divide a time by an integer divisor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator/(const Time lhs, const za::I64 rhs)
    {
        ZA_ASSERT(rhs != 0 && "Time::operator/ cannot divide by 0");
        return Time(lhs.m_microseconds / rhs);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound divide of a time by a floating-point divisor
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator/=(Time& lhs, const float rhs)
    {
        ZA_ASSERT(rhs != 0 && "Time::operator/= cannot divide by 0");
        return lhs = lhs / rhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound divide of a time by an integer divisor
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator/=(Time& lhs, const za::I64 rhs)
    {
        ZA_ASSERT(rhs != 0 && "Time::operator/= cannot divide by 0");
        return lhs = lhs / rhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Ratio of two times as a floating-point value
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr float operator/(const Time lhs, const Time rhs)
    {
        ZA_ASSERT(rhs.m_microseconds != 0 && "Time::operator/ cannot divide by 0");
        return static_cast<float>(lhs.m_microseconds) / static_cast<float>(rhs.m_microseconds);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Remainder of one time divided by another
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr Time operator%(const Time lhs, const Time rhs)
    {
        ZA_ASSERT(rhs.m_microseconds != 0 && "Time::operator% cannot modulus by 0");
        return Time(lhs.m_microseconds % rhs.m_microseconds);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound remainder of one time divided by another
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] friend constexpr Time& operator%=(Time& lhs, const Time rhs)
    {
        ZA_ASSERT(rhs.m_microseconds != 0 && "Time::operator%= cannot modulus by 0");
        return lhs = lhs % rhs;
    }


    ////////////////////////////////////////////////////////////
    friend constexpr Time seconds(float amount);
    friend constexpr Time milliseconds(za::I32 amount);
    friend constexpr Time microseconds(za::I64 amount);

private:
    ////////////////////////////////////////////////////////////
    /// \brief Convert a floating-point number of microseconds, which must be representable as `za::I64`
    ///
    /// Converting NaN, infinity, or an out-of-range value to an integer is undefined behavior.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] static constexpr za::I64 floatToMicroseconds(const float amount)
    {
        // `2^63` is exactly representable as `float`; NaN fails both comparisons
        ZA_ASSERT(amount >= -9'223'372'036'854'775'808.f && amount < 9'223'372'036'854'775'808.f &&
                  "za::Time value is NaN, infinite, or out of range");

        return static_cast<za::I64>(amount);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from microseconds
    ///
    /// Prefer the `za::microseconds`, `za::milliseconds`, and
    /// `za::seconds` factory functions for clarity at call sites.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit Time(const za::I64 microseconds) : m_microseconds(microseconds)
    {
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    za::I64 m_microseconds{}; //!< Time value stored as microseconds
};


////////////////////////////////////////////////////////////
/// \brief Construct a time value from a number of seconds
///
/// \see `milliseconds`, `microseconds`
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr Time seconds(const float amount)
{
    return Time(Time::floatToMicroseconds(amount * 1'000'000.f));
}

////////////////////////////////////////////////////////////
/// \brief Construct a time value from a number of milliseconds
///
/// \see `seconds`, `microseconds`
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr Time milliseconds(const za::I32 amount)
{
    return Time(static_cast<za::I64>(amount) * 1000); // `za::I32` arithmetic would overflow after ~35.8 minutes
}


////////////////////////////////////////////////////////////
/// \brief Construct a time value from a number of microseconds
///
/// \see `seconds`, `milliseconds`
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr Time microseconds(const za::I64 amount)
{
    return Time(amount);
}

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::Time
/// \ingroup system
///
/// `za::Time` encapsulates a time value in a flexible way:
/// it can be defined or read as a number of seconds,
/// milliseconds, or microseconds, leaving the choice of
/// representation to the user.
///
/// `za::Time` is intentionally decoupled from `<chrono>` to keep
/// this header lightweight; conversions between `za::Time` and
/// `std::chrono::duration` are provided as opt-in helpers in
/// `Zancle/Chrono/TimeChronoUtil.hpp` (see `za::TimeChronoUtil`).
///
/// Time values support the usual arithmetic and comparison
/// operations, and represent a span (so they may be negative).
///
/// Usage example:
/// \code
/// za::Time t1 = za::seconds(0.1f);
/// za::I64 milli = t1.asMilliseconds(); // 100
///
/// za::Time t2 = za::milliseconds(30);
/// za::I64 micro = t2.asMicroseconds(); // 30'000
///
/// za::Time t3 = za::microseconds(-800'000);
/// float sec = t3.asSeconds(); // -0.8
/// \endcode
///
/// \code
/// void update(za::Time elapsed)
/// {
///    position += speed * elapsed.asSeconds();
/// }
///
/// update(za::milliseconds(100));
/// \endcode
///
/// \see `za::Clock`
///
////////////////////////////////////////////////////////////
