#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/IntTypes.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Full 128-bit product of `a` and `b` using 32-bit halves (schoolbook multiplication)
///
/// Portable fallback of `mulWide`, for targets without `__int128`
/// (e.g. 32-bit ARM). Always defined, so that it can be tested
/// everywhere.
///
/// \return The low 64 bits of the product; the high 64 bits are written into `high`
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr U64 mulWidePortable(const U64 a, const U64 b, U64& high) noexcept
{
    const U64 aLo = a & 0xFF'FF'FF'FFu;
    const U64 aHi = a >> 32;
    const U64 bLo = b & 0xFF'FF'FF'FFu;
    const U64 bHi = b >> 32;

    const U64 loLo = aLo * bLo;
    const U64 hiLo = aHi * bLo;
    const U64 loHi = aLo * bHi;
    const U64 hiHi = aHi * bHi;

    // Middle column, including the carry out of the low half. Cannot overflow:
    // at most `(2^32 - 1)^2 + 2 * (2^32 - 1) == 2^64 - 1`.
    const U64 middle = (loLo >> 32) + (hiLo & 0xFF'FF'FF'FFu) + loHi;

    high = hiHi + (hiLo >> 32) + (middle >> 32);
    return (middle << 32) | (loLo & 0xFF'FF'FF'FFu);
}


////////////////////////////////////////////////////////////
/// \brief Full 128-bit product of `a` and `b`
///
/// \return The low 64 bits of the product; the high 64 bits are written into `high`
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr U64 mulWide(const U64 a, const U64 b, U64& high) noexcept
{
#ifdef __SIZEOF_INT128__
    __extension__ using U128 = unsigned __int128; // `__extension__`: avoids `-Wpedantic`

    const U128 product = static_cast<U128>(a) * b;

    high = static_cast<U64>(product >> 64);
    return static_cast<U64>(product);
#else
    return mulWidePortable(a, b, high);
#endif
}

} // namespace za::priv
