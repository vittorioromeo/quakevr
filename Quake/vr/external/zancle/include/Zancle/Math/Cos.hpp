#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(cos)
    #define ZA_MATH_COS(...)  __builtin_cos(__VA_ARGS__)
    #define ZA_MATH_COSF(...) __builtin_cosf(__VA_ARGS__)
    #define ZA_MATH_COSL(...) __builtin_cosl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::cos` has `float` overloads: convert like `__builtin_cos`, which only takes `double`
    #define ZA_MATH_COS(x)    ::std::cos(static_cast<double>(x))
    #define ZA_MATH_COSF(...) ::std::cosf(__VA_ARGS__)
    #define ZA_MATH_COSL(...) ::std::cosl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(cos, COS)
