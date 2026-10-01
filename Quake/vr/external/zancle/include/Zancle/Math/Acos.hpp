#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(acos)
    #define ZA_MATH_ACOS(...)  __builtin_acos(__VA_ARGS__)
    #define ZA_MATH_ACOSF(...) __builtin_acosf(__VA_ARGS__)
    #define ZA_MATH_ACOSL(...) __builtin_acosl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::acos` has `float` overloads: convert like `__builtin_acos`, which only takes `double`
    #define ZA_MATH_ACOS(x)    ::std::acos(static_cast<double>(x))
    #define ZA_MATH_ACOSF(...) ::std::acosf(__VA_ARGS__)
    #define ZA_MATH_ACOSL(...) ::std::acosl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(acos, ACOS)
