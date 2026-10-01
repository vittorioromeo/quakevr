#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(ceil)
    #define ZA_MATH_CEIL(...)  __builtin_ceil(__VA_ARGS__)
    #define ZA_MATH_CEILF(...) __builtin_ceilf(__VA_ARGS__)
    #define ZA_MATH_CEILL(...) __builtin_ceill(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::ceil` has `float` overloads: convert like `__builtin_ceil`, which only takes `double`
    #define ZA_MATH_CEIL(x)    ::std::ceil(static_cast<double>(x))
    #define ZA_MATH_CEILF(...) ::std::ceilf(__VA_ARGS__)
    #define ZA_MATH_CEILL(...) ::std::ceill(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(ceil, CEIL)
