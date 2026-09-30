#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(lround)
    #define ZA_MATH_LROUND(...)  __builtin_lround(__VA_ARGS__)
    #define ZA_MATH_LROUNDF(...) __builtin_lroundf(__VA_ARGS__)
    #define ZA_MATH_LROUNDL(...) __builtin_lroundl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::lround` has `float` overloads: convert like `__builtin_lround`, which only takes `double`
    #define ZA_MATH_LROUND(x)    ::std::lround(static_cast<double>(x))
    #define ZA_MATH_LROUNDF(...) ::std::lroundf(__VA_ARGS__)
    #define ZA_MATH_LROUNDL(...) ::std::lroundl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(lround, LROUND)
