#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"

#include "Zancle/Base/AssertAndAssume.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(remainder)
    #define ZA_MATH_REMAINDER(...)  __builtin_remainder(__VA_ARGS__)
    #define ZA_MATH_REMAINDERF(...) __builtin_remainderf(__VA_ARGS__)
    #define ZA_MATH_REMAINDERL(...) __builtin_remainderl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::remainder` has `float` overloads: convert like `__builtin_remainder`, which only takes `double`
    #define ZA_MATH_REMAINDER(x, y) ::std::remainder(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_REMAINDERF(...) ::std::remainderf(__VA_ARGS__)
    #define ZA_MATH_REMAINDERL(...) ::std::remainderl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
// `za::remainder(a, b)`: the IEEE remainder `a - n * b`, where `n` is `a / b` rounded to the nearest
// integer (ties to even), exactly like `std::remainder`: the result is in `[-b / 2, b / 2]` (e.g.
// `remainder(190.f, 360.f) == -170.f`). For the truncated remainder (like `std::fmod`), see
// `za::truncatedRemainder` (fast) and `za::fmod` (exact).
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(remainder, REMAINDER)


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Truncated remainder `a - trunc(a / b) * b` for floats (sign follows `a`)
///
/// Same semantics as `std::fmod` (e.g. `truncatedRemainder(-7.f, 3.f) == -1.f`),
/// not `std::remainder` (which rounds the quotient to the nearest integer
/// instead of truncating it: see `za::remainder`). Lighter and faster than
/// `std::fmod`, but not exact: the quotient is truncated via an `int`
/// conversion and the result is computed in `float` arithmetic, so its
/// absolute error grows with `|a|` (up to about `ulp(a)`), which is
/// sufficient for the gameplay use cases that need it.
///
/// `b` must be strictly positive, and `|a / b|` must be less than `2^31`
/// (as the quotient is truncated via `int`).
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] constexpr float truncatedRemainder(const float a, const float b) noexcept
{
    ZA_ASSERT_AND_ASSUME(b > 0.f);

    return a - static_cast<float>(static_cast<int>(a / b)) * b;
}


////////////////////////////////////////////////////////////
/// \brief Like `truncatedRemainder` but always returns a value in `[0, b)`
///
/// Equivalent to `((a % b) + b) % b` for floats (e.g.
/// `positiveRemainder(-7.f, 3.f) == 2.f`), with the same precision as
/// `truncatedRemainder`. `b` must be strictly positive, and `|a / b|` must
/// be less than `2^31` (as the quotient is truncated via `int`). Useful for
/// wrapping angles or texture coordinates.
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] constexpr float positiveRemainder(const float a, const float b) noexcept
{
    ZA_ASSERT_AND_ASSUME(b > 0.f);

    const auto val = a - static_cast<float>(static_cast<int>(a / b)) * b;

    // Arithmetic rather than a ternary, so that it compiles to branchless code (the sign of `val`
    // is unpredictable, and a mispredicted branch costs far more than the multiplication)
    const auto wrapped = val + b * static_cast<float>(val < 0.f);

    // Rounding can leave `wrapped` just outside `[0, b)`: `val + b` rounds to exactly `b` when `val`
    // is a tiny negative number (e.g. `-1e-8f`), and when `a / b` rounds towards zero across an
    // integer, `val` is slightly below `-b` (e.g. `a = -2224.24756f` and `b = tau` give a negative
    // `wrapped` of `-1.76e-5f`). The correct result is within rounding error of `0` (modulo `b`) in both cases.
    return (wrapped >= 0.f && wrapped < b) ? wrapped : 0.f;
}

} // namespace za
