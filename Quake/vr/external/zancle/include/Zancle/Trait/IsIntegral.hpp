#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_integral)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_INTEGRAL(...) __is_integral(__VA_ARGS__)

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
// cv-qualifiers are handled via partial specializations rather than builtins,
// as GCC rejects builtin type traits in function signatures (e.g. `requires`)
// clang-format off
template <typename T> inline constexpr bool isIntegralImpl                   = false;
template <typename T> inline constexpr bool isIntegralImpl<const T>          = isIntegralImpl<T>;
template <typename T> inline constexpr bool isIntegralImpl<volatile T>       = isIntegralImpl<T>;
template <typename T> inline constexpr bool isIntegralImpl<const volatile T> = isIntegralImpl<T>;
// clang-format on


// clang-format off
////////////////////////////////////////////////////////////
template <> inline constexpr bool isIntegralImpl<bool>               = true;
template <> inline constexpr bool isIntegralImpl<char>               = true;
template <> inline constexpr bool isIntegralImpl<signed char>        = true;
template <> inline constexpr bool isIntegralImpl<unsigned char>      = true;
template <> inline constexpr bool isIntegralImpl<wchar_t>            = true;
template <> inline constexpr bool isIntegralImpl<char8_t>            = true;
template <> inline constexpr bool isIntegralImpl<char16_t>           = true;
template <> inline constexpr bool isIntegralImpl<char32_t>           = true;
template <> inline constexpr bool isIntegralImpl<short>              = true;
template <> inline constexpr bool isIntegralImpl<unsigned short>     = true;
template <> inline constexpr bool isIntegralImpl<int>                = true;
template <> inline constexpr bool isIntegralImpl<unsigned int>       = true;
template <> inline constexpr bool isIntegralImpl<long>               = true;
template <> inline constexpr bool isIntegralImpl<unsigned long>      = true;
template <> inline constexpr bool isIntegralImpl<long long>          = true;
template <> inline constexpr bool isIntegralImpl<unsigned long long> = true;

    #ifdef __SIZEOF_INT128__
template <> inline constexpr bool isIntegralImpl<__int128_t>         = true;
template <> inline constexpr bool isIntegralImpl<__uint128_t>        = true;
    #endif
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_IS_INTEGRAL(...) ::za::priv::isIntegralImpl<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isIntegral = ZA_IS_INTEGRAL(T);

} // namespace za
