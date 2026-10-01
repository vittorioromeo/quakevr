#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"

#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IntTypes.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Portable `constexpr` `fabs` for compilers without `__builtin_fabs` (e.g. MSVC)
///
/// Clears the sign bit, which is exact for every value, including `-0`,
/// infinities, and NaNs (unlike `x < 0 ? -x : x`).
///
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::const]] inline constexpr float fabsViaSignBit(const float x) noexcept
{
    return ZA_BIT_CAST(float, ZA_BIT_CAST(U32, x) & 0x7F'FF'FF'FFu);
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline, gnu::const]] inline constexpr double fabsViaSignBit(const double x) noexcept
{
    return ZA_BIT_CAST(double, ZA_BIT_CAST(U64, x) & 0x7F'FF'FF'FF'FF'FF'FF'FFull);
}

} // namespace za::priv


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(fabs)
    #define ZA_MATH_FABS(...)  __builtin_fabs(__VA_ARGS__)
    #define ZA_MATH_FABSF(...) __builtin_fabsf(__VA_ARGS__)
    #define ZA_MATH_FABSL(...) __builtin_fabsl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `std::fabs` is not `constexpr` before C++23 on every standard library (e.g. MSVC's)
    #define ZA_MATH_FABS(x)    ::za::priv::fabsViaSignBit(static_cast<double>(x))
    #define ZA_MATH_FABSF(x)   ::za::priv::fabsViaSignBit(static_cast<float>(x))
    #define ZA_MATH_FABSL(...) ::std::fabsl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(fabs, FABS)
