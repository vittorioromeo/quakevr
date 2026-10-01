#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(trunc)
    #define ZA_MATH_TRUNC(...)  __builtin_trunc(__VA_ARGS__)
    #define ZA_MATH_TRUNCF(...) __builtin_truncf(__VA_ARGS__)
    #define ZA_MATH_TRUNCL(...) __builtin_truncl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::trunc` has `float` overloads: convert like `__builtin_trunc`, which only takes `double`
    #define ZA_MATH_TRUNC(x)    ::std::trunc(static_cast<double>(x))
    #define ZA_MATH_TRUNCF(...) ::std::truncf(__VA_ARGS__)
    #define ZA_MATH_TRUNCL(...) ::std::truncl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(trunc, TRUNC)
