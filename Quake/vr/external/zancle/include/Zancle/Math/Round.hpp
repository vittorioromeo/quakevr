#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(round)
    #define ZA_MATH_ROUND(...)  __builtin_round(__VA_ARGS__)
    #define ZA_MATH_ROUNDF(...) __builtin_roundf(__VA_ARGS__)
    #define ZA_MATH_ROUNDL(...) __builtin_roundl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::round` has `float` overloads: convert like `__builtin_round`, which only takes `double`
    #define ZA_MATH_ROUND(x)    ::std::round(static_cast<double>(x))
    #define ZA_MATH_ROUNDF(...) ::std::roundf(__VA_ARGS__)
    #define ZA_MATH_ROUNDL(...) ::std::roundl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(round, ROUND)
