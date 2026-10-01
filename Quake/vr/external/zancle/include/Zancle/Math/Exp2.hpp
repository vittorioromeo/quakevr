#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(exp2)
    #define ZA_MATH_EXP2(...)  __builtin_exp2(__VA_ARGS__)
    #define ZA_MATH_EXP2F(...) __builtin_exp2f(__VA_ARGS__)
    #define ZA_MATH_EXP2L(...) __builtin_exp2l(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::exp2` has `float` overloads: convert like `__builtin_exp2`, which only takes `double`
    #define ZA_MATH_EXP2(x)    ::std::exp2(static_cast<double>(x))
    #define ZA_MATH_EXP2F(...) ::std::exp2f(__VA_ARGS__)
    #define ZA_MATH_EXP2L(...) ::std::exp2l(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(exp2, EXP2)
