#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(floor)
    #define ZA_MATH_FLOOR(...)  __builtin_floor(__VA_ARGS__)
    #define ZA_MATH_FLOORF(...) __builtin_floorf(__VA_ARGS__)
    #define ZA_MATH_FLOORL(...) __builtin_floorl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::floor` has `float` overloads: convert like `__builtin_floor`, which only takes `double`
    #define ZA_MATH_FLOOR(x)    ::std::floor(static_cast<double>(x))
    #define ZA_MATH_FLOORF(...) ::std::floorf(__VA_ARGS__)
    #define ZA_MATH_FLOORL(...) ::std::floorl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(floor, FLOOR)
