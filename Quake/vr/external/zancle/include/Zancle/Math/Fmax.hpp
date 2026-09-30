#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(fmax)
    #define ZA_MATH_FMAX(...)  __builtin_fmax(__VA_ARGS__)
    #define ZA_MATH_FMAXF(...) __builtin_fmaxf(__VA_ARGS__)
    #define ZA_MATH_FMAXL(...) __builtin_fmaxl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::fmax` has `float` overloads: convert like `__builtin_fmax`, which only takes `double`
    #define ZA_MATH_FMAX(x, y) ::std::fmax(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_FMAXF(...) ::std::fmaxf(__VA_ARGS__)
    #define ZA_MATH_FMAXL(...) ::std::fmaxl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(fmax, FMAX)
