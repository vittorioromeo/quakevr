#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(fabs)
    #define ZA_MATH_FABS(...)  __builtin_fabs(__VA_ARGS__)
    #define ZA_MATH_FABSF(...) __builtin_fabsf(__VA_ARGS__)
    #define ZA_MATH_FABSL(...) __builtin_fabsl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::fabs` has `float` overloads: convert like `__builtin_fabs`, which only takes `double`
    #define ZA_MATH_FABS(x)    ::std::fabs(static_cast<double>(x))
    #define ZA_MATH_FABSF(...) ::std::fabsf(__VA_ARGS__)
    #define ZA_MATH_FABSL(...) ::std::fabsl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(fabs, FABS)
