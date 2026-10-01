#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/Rint.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Clzll.hpp"
#include "Zancle/Base/IsInf.hpp"
#include "Zancle/Base/IsNan.hpp"
#include "Zancle/Base/Signbit.hpp"

#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsIntegral.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsUnsigned.hpp"
#include "Zancle/Trait/MakeUnsigned.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Precomputed powers of 10 (supports up to 10 decimal places)
///
////////////////////////////////////////////////////////////
inline constexpr const long long powersOf10[] = {
    1ll,
    10ll,
    100ll,
    1000ll,
    10'000ll,
    100'000ll,
    1'000'000ll,
    10'000'000ll,
    100'000'000ll,
    1'000'000'000ll,
    10'000'000'000ll,
};


////////////////////////////////////////////////////////////
/// \brief 200-byte ASCII table: bytes `[2*N .. 2*N+1]` hold the two-digit
/// representation of `N` for `0 <= N < 100` (e.g. index 7 -> `"07"`)
///
////////////////////////////////////////////////////////////
inline constexpr char digitPairs[201] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";


////////////////////////////////////////////////////////////
/// \brief Maps a value's top set-bit index (0..63) to its decimal digit count
///
/// `decimalDigitsFromTopBit[b]` is the digit count of the largest 64-bit value
/// whose highest set bit is `b`. Because all values sharing a top bit span at
/// most one decimal-length boundary, this is the exact digit count or an
/// over-estimate by exactly one, corrected by a single comparison below.
///
////////////////////////////////////////////////////////////
inline constexpr unsigned char decimalDigitsFromTopBit[64] =
    {1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7,
     7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14,
     14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};


////////////////////////////////////////////////////////////
/// \brief Smallest values requiring a given decimal digit count
///
/// `decimalThreshold[t] == 10^(t-1)` is the smallest `t`-digit number, used to
/// correct the over-estimate from `decimalDigitsFromTopBit`. Indices `0` and `1`
/// are `0` so the correction is a no-op for the single-digit / zero case.
///
////////////////////////////////////////////////////////////
inline constexpr unsigned long long decimalThreshold[21] =
    {0ull,
     0ull,
     10ull,
     100ull,
     1000ull,
     10'000ull,
     100'000ull,
     1'000'000ull,
     10'000'000ull,
     100'000'000ull,
     1'000'000'000ull,
     10'000'000'000ull,
     100'000'000'000ull,
     1'000'000'000'000ull,
     10'000'000'000'000ull,
     100'000'000'000'000ull,
     1'000'000'000'000'000ull,
     10'000'000'000'000'000ull,
     100'000'000'000'000'000ull,
     1'000'000'000'000'000'000ull,
     10'000'000'000'000'000'000ull};


////////////////////////////////////////////////////////////
/// \brief Number of decimal digits needed to represent `value`
///
/// Branchless: one count-leading-zeros to find the top set bit, a table lookup
/// for a digit-count estimate (exact or one too high), and a single comparison
/// to correct the off-by-one. Latency is independent of the input's magnitude.
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline, gnu::pure]] inline constexpr int decimalDigitCount(const T x) noexcept
{
    static_assert(ZA_IS_UNSIGNED(T));

    // Widen to 64-bit so a single `clzll`-based path covers every unsigned width.
    const unsigned long long n = x;

    // `n | 1ull` keeps the operand nonzero: `__builtin_clzll(0)` is UB and the
    // `0` and `1` cases share digit count `1`, so the `| 1` is load-bearing.
    const int t = decimalDigitsFromTopBit[63 - ZA_CLZLL(n | 1ull)];

    // Subtract one when the estimate over-counts (i.e. `n` is below the smallest
    // `t`-digit value); `decimalThreshold[1] == 0` makes this a no-op for `t == 1`.
    return t - (n < decimalThreshold[t]);
}


////////////////////////////////////////////////////////////
/// \brief Jeaiii-style: write the decimal representation of an unsigned
/// `value` into `[first, last)` using a 200-byte digit-pair lookup table.
///
/// Halves the number of `divide-by-10` operations vs. the naïve loop and
/// avoids the in-place reverse pass: computes digit count up front, then
/// writes pairs right-to-left straight into the destination.
///
/// \return Pointer one past the last written character, or `nullptr` if
/// the buffer is too small.
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline, gnu::flatten]] constexpr char* unsignedToChars(char* const first, const char* const last, T value)
{
    static_assert(ZA_IS_UNSIGNED(T));

    const int digits = decimalDigitCount(value);

    if (last - first < digits)
        return nullptr; // Buffer too small

    char* const end = first + digits;
    char*       p   = end;

    // Two-at-a-time loop: divide by 100, look up the pair. Use literal `100u`
    // for the comparison (rather than `T{100}`) so a narrow type like
    // `unsigned char` doesn't trip the brace-init narrowing check.
    while (value >= 100u)
    {
        const auto pairIdx = static_cast<unsigned>(value % 100u) * 2u;
        value              = static_cast<T>(value / 100u);
        p -= 2;

        p[0] = digitPairs[pairIdx];
        p[1] = digitPairs[pairIdx + 1u];
    }

    // Tail: one or two digits left.
    if (value >= 10u)
    {
        const auto pairIdx = static_cast<unsigned>(value) * 2u;
        p -= 2;

        p[0] = digitPairs[pairIdx];
        p[1] = digitPairs[pairIdx + 1u];
    }
    else
    {
        *--p = static_cast<char>('0' + static_cast<unsigned>(value));
    }

    return end;
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Write the decimal representation of an integral `value` into `[first, last)` (mimics `std::to_chars`)
///
/// \return Pointer one past the last written character, or `nullptr` if the buffer is too small
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] constexpr char* toChars(char* first, const char* const last, const T value)
    requires isIntegral<T>
{
    if constexpr (ZA_IS_SAME(T, bool))
    {
        if (first >= last)
            return nullptr;

        *first++ = value ? '1' : '0';
        return first;
    }
    else if constexpr (isUnsigned<T>)
    {
        return priv::unsignedToChars(first, last, value);
    }
    else
    {
        // Use the unsigned counterpart for calculations to correctly handle T_MIN
        using UT          = MakeUnsigned<T>;
        const auto uValue = static_cast<UT>(value);

        if (value < 0)
        {
            if (first >= last)
                return nullptr; // Buffer too small

            *first++ = '-';

            // The cast back to `UT` after `-uValue` is required for types narrower
            // than `int`: integer promotion turns `-uValue` into a (signed) `int`,
            // defeating the wrap-around trick and producing a negative value that
            // `unsignedToChars` would silently print as nothing.
            return priv::unsignedToChars(first, last, static_cast<UT>(-uValue));
        }

        return priv::unsignedToChars(first, last, uValue);
    }
}


////////////////////////////////////////////////////////////
/// \brief Write a floating-point `value` into `[first, last)` with fixed `precision`.
///
/// Matches `std::to_chars(..., chars_format::fixed, precision)` for finite values
/// in the supported precision range. Special-value handling: `NaN` is written as
/// `"nan"` (no sign), infinities as `"inf"` / `"-inf"`, negative zero preserves
/// its sign (`"-0.00"`). Rounding uses the active FPU rounding mode (default:
/// round-half-to-even, matching IEEE-754 and `std::to_chars`).
///
/// \pre `precision >= 0 && precision <= 10`.
/// \pre `value` is finite when compiled with `-ffinite-math-only` (or
///      `-ffast-math`, which implies it). Under that flag the compiler may
///      legally fold `__builtin_isnan` / `__builtin_isinf` to `false`, so
///      NaN/inf inputs would silently produce garbage output.
///
/// \return Pointer one past the last written character, or `nullptr` on:
///         - buffer too small,
///         - `value` outside the `long long`-representable range
///           (≈ `±9.2e18`, scaled by `10^precision`), in which case the caller
///           should pre-scale or use a different formatter.
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] constexpr char* toChars(char* first, const char* const last, T value, const int precision = 6)
    requires isFloatingPoint<T>
{
    ZA_ASSERT(precision >= 0 && precision <= 10);

    // Promote narrow floats to `double` so the internal `value * 10^precision`
    // multiplication preserves all the input's bits. `float`'s 24-bit mantissa
    // fits losslessly into `double`'s 53-bit mantissa, so the cast is exact.
    // Without this, e.g. `0.1f` at precision 10 would print "0.1000000000"
    // instead of the true "0.1000000015".
    if constexpr (sizeof(T) < sizeof(double))
        return toChars(first, last, static_cast<double>(value), precision);

    char* p = first;

#if !__FINITE_MATH_ONLY__
    // NaN: emit "nan" with no sign (matches `std::to_chars` and IEEE-754).
    // Under `-ffinite-math-only`, this branch folds to dead code: that's
    // intentional -- passing NaN under that flag is out of contract (see
    // precondition in the doc).
    if (ZA_ISNAN(value)) [[unlikely]]
    {
        if (last - p < 3)
            return nullptr;

        *p++ = 'n';
        *p++ = 'a';
        *p++ = 'n';
        return p;
    }
#endif

    // Sign via signbit so `-0.0` keeps its sign (matches `std::to_chars`).
    if (ZA_SIGNBIT(value))
    {
        if (p >= last)
            return nullptr;

        *p++  = '-';
        value = -value;
    }

#if !__FINITE_MATH_ONLY__
    // Infinity: emit "inf" after any sign already written. Same out-of-contract
    // status as NaN under `-ffinite-math-only`.
    if (ZA_ISINF(value)) [[unlikely]]
    {
        if (last - p < 3)
            return nullptr;

        *p++ = 'i';
        *p++ = 'n';
        *p++ = 'f';
        return p;
    }
#endif

    // Out-of-range guard: the integer-part conversion casts to `long long`,
    // which is UB for values outside its range. Pick a threshold safely
    // below `LLONG_MAX` (= 9.223e18) so that rounding doesn't push us over.
    constexpr T safeLLongUpper = static_cast<T>(9'000'000'000'000'000'000LL);

    if (precision == 0)
    {
        if (value > safeLLongUpper) [[unlikely]]
            return nullptr;

        // `value` is non-negative here (signbit branch already negated it),
        // so the unsigned cast is well-defined.
        const auto roundedAsInt = static_cast<unsigned long long>(za::rint(value));
        return priv::unsignedToChars(p, last, roundedAsInt);
    }

    const long long multiplier = priv::powersOf10[precision];
    const T         scaled     = value * static_cast<T>(multiplier);

    if (scaled > safeLLongUpper) [[unlikely]]
        return nullptr;

    // `value` is non-negative at this point (signbit branch already negated it)
    // and `scaled <= 9e18 < LLONG_MAX`, so the unsigned cast is well-defined.
    const auto roundedScaledValue = static_cast<unsigned long long>(za::rint(scaled));
    const auto finalIntPart       = roundedScaledValue / static_cast<unsigned long long>(multiplier);
    auto       finalFracPart      = roundedScaledValue % static_cast<unsigned long long>(multiplier);

    p = priv::unsignedToChars(p, last, finalIntPart);

    if (p == nullptr)
        return nullptr;

    if (last - p < precision + 1) // '.' + `precision` digits
        return nullptr;

    *p++ = '.';

    // Write the fractional digits directly into [p, p + precision), backward.
    // The output is always exactly `precision` chars wide; any shortfall vs. the
    // natural digit count is filled with leading zeros on the left.
    char* const fracStart  = p;
    char* const fracOutEnd = p + precision;
    char*       c          = fracOutEnd - 1;

    while (finalFracPart > 0)
    {
        *c-- = '0' + static_cast<char>(finalFracPart % 10);
        finalFracPart /= 10;
    }

    while (c >= fracStart)
        *c-- = '0';

    return fracOutEnd;
}

} // namespace za
