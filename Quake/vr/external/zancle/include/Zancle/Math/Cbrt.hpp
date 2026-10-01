#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(cbrt)
    #define ZA_MATH_CBRT(...)  __builtin_cbrt(__VA_ARGS__)
    #define ZA_MATH_CBRTF(...) __builtin_cbrtf(__VA_ARGS__)
    #define ZA_MATH_CBRTL(...) __builtin_cbrtl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::cbrt` has `float` overloads: convert like `__builtin_cbrt`, which only takes `double`
    #define ZA_MATH_CBRT(x)    ::std::cbrt(static_cast<double>(x))
    #define ZA_MATH_CBRTF(...) ::std::cbrtf(__VA_ARGS__)
    #define ZA_MATH_CBRTL(...) ::std::cbrtl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(cbrt, CBRT)
