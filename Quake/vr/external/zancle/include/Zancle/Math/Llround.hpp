#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(llround)
    #define ZA_MATH_LLROUND(...)  __builtin_llround(__VA_ARGS__)
    #define ZA_MATH_LLROUNDF(...) __builtin_llroundf(__VA_ARGS__)
    #define ZA_MATH_LLROUNDL(...) __builtin_llroundl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::llround` has `float` overloads: convert like `__builtin_llround`, which only takes `double`
    #define ZA_MATH_LLROUND(x)    ::std::llround(static_cast<double>(x))
    #define ZA_MATH_LLROUNDF(...) ::std::llroundf(__VA_ARGS__)
    #define ZA_MATH_LLROUNDL(...) ::std::llroundl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(llround, LLROUND)
