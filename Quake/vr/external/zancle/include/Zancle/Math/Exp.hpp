#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(exp)
    #define ZA_MATH_EXP(...)  __builtin_exp(__VA_ARGS__)
    #define ZA_MATH_EXPF(...) __builtin_expf(__VA_ARGS__)
    #define ZA_MATH_EXPL(...) __builtin_expl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::exp` has `float` overloads: convert like `__builtin_exp`, which only takes `double`
    #define ZA_MATH_EXP(x)    ::std::exp(static_cast<double>(x))
    #define ZA_MATH_EXPF(...) ::std::expf(__VA_ARGS__)
    #define ZA_MATH_EXPL(...) ::std::expl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(exp, EXP)
