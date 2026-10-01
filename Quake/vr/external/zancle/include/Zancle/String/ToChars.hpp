#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/Clzll.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/IsInf.hpp"
#include "Zancle/Base/IsNan.hpp"
#include "Zancle/Base/MulWide.hpp"
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
    static_assert(sizeof(T) <= sizeof(unsigned long long), "128-bit integers are not supported");

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


////////////////////////////////////////////////////////////
/// \brief Write the decimal representation of a finite `double` that is at least `2^53` into `[first, last)`
///
/// Such values are integers (`mantissa * 2^exponent`), but may exceed the
/// 64-bit range (up to `DBL_MAX`, 309 digits): they are printed exactly
/// through a small base-`10^9` big integer.
///
/// \return Pointer one past the last written character, or `nullptr` if the buffer is too small.
///
////////////////////////////////////////////////////////////
[[nodiscard]] constexpr char* largeDoubleToChars(char* const first, const char* const last, const double value)
{
    const auto bits     = ZA_BIT_CAST(za::U64, value);
    const auto mantissa = (bits & ((za::U64{1} << 52) - 1u)) | (za::U64{1} << 52);
    int        exponent = static_cast<int>((bits >> 52) & 0x7'FFu) - 1075; // `value == mantissa * 2^exponent`

    ZA_ASSERT(exponent > 0 && exponent <= 971);

    // Little-endian base-10^9 limbs: `DBL_MAX` has 309 digits, i.e. 35 limbs
    constexpr za::U32 limbBase = 1'000'000'000u;

    za::U32 limbs[36]{};
    int     limbCount = 0;

    for (za::U64 rest = mantissa; rest != 0u; rest /= limbBase)
        limbs[limbCount++] = static_cast<za::U32>(rest % limbBase);

    // Multiply by `2^exponent`, at most 2^29 at a time so that `limb * 2^29 + carry` fits in 64 bits
    while (exponent > 0)
    {
        const int shift = exponent < 29 ? exponent : 29;
        exponent -= shift;

        za::U64 carry = 0u;
        for (int i = 0; i < limbCount; ++i)
        {
            const za::U64 product = (za::U64{limbs[i]} << shift) + carry;
            limbs[i]              = static_cast<za::U32>(product % limbBase);
            carry                 = product / limbBase;
        }

        for (; carry != 0u; carry /= limbBase)
            limbs[limbCount++] = static_cast<za::U32>(carry % limbBase);
    }

    // Most significant limb without leading zeros, then zero-padded 9-digit groups
    char* p = unsignedToChars(first, last, limbs[limbCount - 1]);
    if (p == nullptr || last - p < 9 * (limbCount - 1))
        return nullptr;

    for (int i = limbCount - 2; i >= 0; --i)
    {
        za::U32 group = limbs[i];

        for (char* c = p + 9; c != p; group /= 10u)
            *--c = static_cast<char>('0' + group % 10u);

        p += 9;
    }

    return p;
}


////////////////////////////////////////////////////////////
/// \brief `fraction * multiplier` rounded to the nearest integer, exactly, with ties to even
///
/// `fraction` is in `[0, 1)` and `multiplier` is at most `10^10`. The result
/// may equal `multiplier` (rounding up to the next integer). For ties, the
/// parity is that of the result, or of the integer part (`integerPartOdd`)
/// when `multiplier == 1` (i.e. when no fractional digit is printed).
///
/// Exact: `fraction` is exactly `mantissa / 2^shift`, and `mantissa * multiplier`
/// (below `2^53 * 10^10 < 2^87`) is computed exactly in 128 bits. This does not
/// depend on the FPU rounding mode, and avoids the double rounding of
/// `rint(fraction * multiplier)`.
///
////////////////////////////////////////////////////////////
[[nodiscard]] constexpr U64 roundScaledFraction(const double fraction, const U64 multiplier, const bool integerPartOdd) noexcept
{
    ZA_ASSERT(fraction >= 0.0 && fraction < 1.0);

    // `fraction == mantissa * 2^-shift` (subnormals: no implicit bit, and the exponent of the smallest normal)
    const auto bits          = ZA_BIT_CAST(U64, fraction);
    const auto exponentField = static_cast<int>(bits >> 52);
    const U64  mantissa      = (bits & ((U64{1} << 52) - 1u)) | (exponentField != 0 ? U64{1} << 52 : U64{0});
    const int  shift         = 1075 - (exponentField != 0 ? exponentField : 1); // at least 53, as `fraction < 1`

    // `product == mantissa * multiplier == high * 2^64 + low`
    U64       high = 0u;
    const U64 low  = mulWide(mantissa, multiplier, high);

    // `result = product >> shift`, rounded according to the discarded bits (`remainder`) vs half (`2^(shift - 1)`)
    U64  result      = 0u;
    bool aboveHalf   = false;
    bool exactlyHalf = false;

    if (shift < 64)
    {
        result              = (high << (64 - shift)) | (low >> shift);
        const U64 remainder = low & ((U64{1} << shift) - 1u);
        const U64 half      = U64{1} << (shift - 1);

        aboveHalf   = remainder > half;
        exactlyHalf = remainder == half;
    }
    else if (shift == 64)
    {
        result      = high;
        aboveHalf   = low > (U64{1} << 63);
        exactlyHalf = low == (U64{1} << 63);
    }
    else if (shift < 128)
    {
        const int s             = shift - 64; // the remainder is `(high & mask) * 2^64 + low`
        result                  = high >> s;
        const U64 remainderHigh = high & ((U64{1} << s) - 1u);
        const U64 halfHigh      = U64{1} << (s - 1);

        aboveHalf   = remainderHigh > halfHigh || (remainderHigh == halfHigh && low != 0u);
        exactlyHalf = remainderHigh == halfHigh && low == 0u;
    }
    // else: `product < 2^87 <= half`, so the result is zero

    const bool odd = multiplier == 1u ? integerPartOdd : (result & 1u) != 0u;
    return result + ((aboveHalf || (exactlyHalf && odd)) ? 1u : 0u);
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
    requires(isIntegral<T> && sizeof(T) <= sizeof(unsigned long long)) // no 128-bit integers
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
/// Mirrors `std::to_chars(..., chars_format::fixed, precision)` for finite
/// values, whatever their magnitude (up to `DBL_MAX`). Special-value handling:
/// `NaN` is written as `"nan"` (no sign, unlike some standard libraries'
/// `"-nan"`), infinities as `"inf"` / `"-inf"`, negative zero preserves its
/// sign (`"-0.00"`). Rounding is exact, to nearest with ties to even,
/// independently of the FPU rounding mode.
///
/// `long double` values are formatted through `double`.
///
/// \pre `precision >= 0 && precision <= 10` (otherwise `nullptr` is returned).
/// \pre `value` is finite when compiled with `-ffinite-math-only` (or
///      `-ffast-math`, which implies it). Under that flag the compiler may
///      legally fold `__builtin_isnan` / `__builtin_isinf` to `false`, so
///      NaN/inf inputs would silently produce garbage output.
///
/// \return Pointer one past the last written character, or `nullptr` if the
///         buffer is too small or `precision` is out of range.
///
template <typename T>
[[nodiscard]] constexpr char* toChars(char* first, const char* const last, T value, const int precision = 6)
    requires isFloatingPoint<T>
{
    ZA_ASSERT(precision >= 0 && precision <= 10);

    if (precision < 0 || precision > 10) [[unlikely]] // `powersOf10` bounds, also in release builds
        return nullptr;

    // Format narrow floats as `double`: `float`'s 24-bit mantissa fits losslessly into `double`'s
    // 53-bit mantissa, so e.g. `0.1f` at precision 10 prints the true "0.1000000015".
    if constexpr (!ZA_IS_SAME(T, double))
    {
        return toChars(first, last, static_cast<double>(value), precision);
    }
    else
    {
        char* p = first;

#if !__FINITE_MATH_ONLY__
        // NaN: emit "nan" with no sign. Under `-ffinite-math-only`, this branch folds
        // to dead code: passing NaN under that flag is out of contract (see above).
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
        // Infinity: emit "inf" after any sign already written.
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

        // From `2^53` onwards, doubles are integers (possibly beyond 64 bits): no fractional digits
        constexpr double twoTo53 = 9'007'199'254'740'992.0;

        unsigned long long fracDigits = 0u; // the first `precision` fractional digits, as an integer

        if (value >= twoTo53)
        {
            p = priv::largeDoubleToChars(p, last, value);
        }
        else
        {
            // Split into the (exact) integer and fractional parts, so that only the fraction
            // is scaled by `10^precision`: no overflow, whatever the magnitude
            auto         intPart  = static_cast<unsigned long long>(value); // truncates: `value < 2^53`
            const double fraction = value - static_cast<double>(intPart);   // exact

            const auto multiplier = static_cast<unsigned long long>(priv::powersOf10[precision]);
            fracDigits            = priv::roundScaledFraction(fraction, multiplier, (intPart & 1u) != 0u);

            // Rounding up to the next integer (e.g. 0.9999999 at precision 6, or 2.5 -> 3 at precision 0)
            if (fracDigits == multiplier)
            {
                ++intPart;
                fracDigits = 0u;
            }

            p = priv::unsignedToChars(p, last, intPart);
        }

        if (p == nullptr)
            return nullptr;

        if (precision == 0)
            return p;

        if (last - p < precision + 1) // '.' + `precision` digits
            return nullptr;

        *p++ = '.';

        // Write the fractional digits into [p, p + precision), backward, zero-padded on the left
        char* const fracEnd = p + precision;

        for (char* c = fracEnd; c != p; fracDigits /= 10u)
            *--c = static_cast<char>('0' + fracDigits % 10u);

        return fracEnd;
    }
}

} // namespace za
