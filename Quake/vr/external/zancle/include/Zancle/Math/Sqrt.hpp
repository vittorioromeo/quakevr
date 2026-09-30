#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(sqrt)
    #define ZA_MATH_SQRT(...)  __builtin_sqrt(__VA_ARGS__)
    #define ZA_MATH_SQRTF(...) __builtin_sqrtf(__VA_ARGS__)
    #define ZA_MATH_SQRTL(...) __builtin_sqrtl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::sqrt` has `float` overloads: convert like `__builtin_sqrt`, which only takes `double`
    #define ZA_MATH_SQRT(x)    ::std::sqrt(static_cast<double>(x))
    #define ZA_MATH_SQRTF(...) ::std::sqrtf(__VA_ARGS__)
    #define ZA_MATH_SQRTL(...) ::std::sqrtl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(sqrt, SQRT)
