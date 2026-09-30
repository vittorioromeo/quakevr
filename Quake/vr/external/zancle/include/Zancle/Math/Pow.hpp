#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(pow)
    #define ZA_MATH_POW(...)  __builtin_pow(__VA_ARGS__)
    #define ZA_MATH_POWF(...) __builtin_powf(__VA_ARGS__)
    #define ZA_MATH_POWL(...) __builtin_powl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::pow` has `float` overloads: convert like `__builtin_pow`, which only takes `double`
    #define ZA_MATH_POW(x, y) ::std::pow(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_POWF(...) ::std::powf(__VA_ARGS__)
    #define ZA_MATH_POWL(...) ::std::powl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(pow, POW)
