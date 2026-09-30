#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(sin)
    #define ZA_MATH_SIN(...)  __builtin_sin(__VA_ARGS__)
    #define ZA_MATH_SINF(...) __builtin_sinf(__VA_ARGS__)
    #define ZA_MATH_SINL(...) __builtin_sinl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::sin` has `float` overloads: convert like `__builtin_sin`, which only takes `double`
    #define ZA_MATH_SIN(x)    ::std::sin(static_cast<double>(x))
    #define ZA_MATH_SINF(...) ::std::sinf(__VA_ARGS__)
    #define ZA_MATH_SINL(...) ::std::sinl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(sin, SIN)
