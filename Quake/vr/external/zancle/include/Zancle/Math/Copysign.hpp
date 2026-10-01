#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(copysign)
    #define ZA_MATH_COPYSIGN(...)  __builtin_copysign(__VA_ARGS__)
    #define ZA_MATH_COPYSIGNF(...) __builtin_copysignf(__VA_ARGS__)
    #define ZA_MATH_COPYSIGNL(...) __builtin_copysignl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::copysign` has `float` overloads: convert like `__builtin_copysign`, which only takes `double`
    #define ZA_MATH_COPYSIGN(x, y) ::std::copysign(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_COPYSIGNF(...) ::std::copysignf(__VA_ARGS__)
    #define ZA_MATH_COPYSIGNL(...) ::std::copysignl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(copysign, COPYSIGN)
