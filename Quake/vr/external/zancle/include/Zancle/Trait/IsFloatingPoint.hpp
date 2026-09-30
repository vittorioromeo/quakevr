#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__is_floating_point)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_FLOATING_POINT(...) __is_floating_point(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/IsSame.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
// cv-qualifiers are handled via partial specializations rather than builtins,
// as GCC rejects builtin type traits in function signatures (e.g. `requires`).
//
// The vendor-extension `__float128` is treated like an extended floating-point
// type, as Clang's `__is_floating_point` does (the standard leaves this to the
// implementation). It is handled by the primary template rather than by an
// explicit specialization, as it may be the same type as `_Float128` or
// `long double` on some targets.
// clang-format off
    #ifdef __SIZEOF_FLOAT128__
template <typename T> inline constexpr bool isFloatingPointImpl                   = ZA_IS_SAME(T, __float128);
    #else
template <typename T> inline constexpr bool isFloatingPointImpl                   = false;
    #endif
template <typename T> inline constexpr bool isFloatingPointImpl<const T>          = isFloatingPointImpl<T>;
template <typename T> inline constexpr bool isFloatingPointImpl<volatile T>       = isFloatingPointImpl<T>;
template <typename T> inline constexpr bool isFloatingPointImpl<const volatile T> = isFloatingPointImpl<T>;
// clang-format on


// clang-format off
////////////////////////////////////////////////////////////
template <> inline constexpr bool isFloatingPointImpl<float>       = true;
template <> inline constexpr bool isFloatingPointImpl<double>      = true;
template <> inline constexpr bool isFloatingPointImpl<long double> = true;

    #ifdef __STDCPP_FLOAT16_T__
template <> inline constexpr bool isFloatingPointImpl<_Float16>    = true;
    #endif

    #ifdef __STDCPP_FLOAT32_T__
template <> inline constexpr bool isFloatingPointImpl<_Float32>    = true;
    #endif

    #ifdef __STDCPP_FLOAT64_T__
template <> inline constexpr bool isFloatingPointImpl<_Float64>    = true;
    #endif

    #ifdef __STDCPP_FLOAT128_T__
template <> inline constexpr bool isFloatingPointImpl<_Float128>   = true;
    #endif

    #ifdef __STDCPP_BFLOAT16_T__
template <> inline constexpr bool isFloatingPointImpl<__bf16>      = true;
    #endif
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_IS_FLOATING_POINT(...) ::za::priv::isFloatingPointImpl<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isFloatingPoint = ZA_IS_FLOATING_POINT(T);

} // namespace za
