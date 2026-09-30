#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(atan2)
    #define ZA_MATH_ATAN2(...)  __builtin_atan2(__VA_ARGS__)
    #define ZA_MATH_ATAN2F(...) __builtin_atan2f(__VA_ARGS__)
    #define ZA_MATH_ATAN2L(...) __builtin_atan2l(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::atan2` has `float` overloads: convert like `__builtin_atan2`, which only takes `double`
    #define ZA_MATH_ATAN2(x, y) ::std::atan2(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_ATAN2F(...) ::std::atan2f(__VA_ARGS__)
    #define ZA_MATH_ATAN2L(...) ::std::atan2l(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(atan2, ATAN2)
