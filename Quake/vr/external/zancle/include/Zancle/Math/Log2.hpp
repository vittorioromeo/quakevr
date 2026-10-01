#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(log2)
    #define ZA_MATH_LOG2(...)  __builtin_log2(__VA_ARGS__)
    #define ZA_MATH_LOG2F(...) __builtin_log2f(__VA_ARGS__)
    #define ZA_MATH_LOG2L(...) __builtin_log2l(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::log2` has `float` overloads: convert like `__builtin_log2`, which only takes `double`
    #define ZA_MATH_LOG2(x)    ::std::log2(static_cast<double>(x))
    #define ZA_MATH_LOG2F(...) ::std::log2f(__VA_ARGS__)
    #define ZA_MATH_LOG2L(...) ::std::log2l(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(log2, LOG2)
