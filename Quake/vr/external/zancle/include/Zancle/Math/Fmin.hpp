#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(fmin)
    #define ZA_MATH_FMIN(...)  __builtin_fmin(__VA_ARGS__)
    #define ZA_MATH_FMINF(...) __builtin_fminf(__VA_ARGS__)
    #define ZA_MATH_FMINL(...) __builtin_fminl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::fmin` has `float` overloads: convert like `__builtin_fmin`, which only takes `double`
    #define ZA_MATH_FMIN(x, y) ::std::fmin(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_FMINF(...) ::std::fminf(__VA_ARGS__)
    #define ZA_MATH_FMINL(...) ::std::fminl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(fmin, FMIN)
