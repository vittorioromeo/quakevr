#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(log10)
    #define ZA_MATH_LOG10(...)  __builtin_log10(__VA_ARGS__)
    #define ZA_MATH_LOG10F(...) __builtin_log10f(__VA_ARGS__)
    #define ZA_MATH_LOG10L(...) __builtin_log10l(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::log10` has `float` overloads: convert like `__builtin_log10`, which only takes `double`
    #define ZA_MATH_LOG10(x)    ::std::log10(static_cast<double>(x))
    #define ZA_MATH_LOG10F(...) ::std::log10f(__VA_ARGS__)
    #define ZA_MATH_LOG10L(...) ::std::log10l(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(log10, LOG10)
