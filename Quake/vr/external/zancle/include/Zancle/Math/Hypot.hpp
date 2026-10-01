#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(hypot)
    #define ZA_MATH_HYPOT(...)  __builtin_hypot(__VA_ARGS__)
    #define ZA_MATH_HYPOTF(...) __builtin_hypotf(__VA_ARGS__)
    #define ZA_MATH_HYPOTL(...) __builtin_hypotl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::hypot` has `float` overloads: convert like `__builtin_hypot`, which only takes `double`
    #define ZA_MATH_HYPOT(x, y) ::std::hypot(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_HYPOTF(...) ::std::hypotf(__VA_ARGS__)
    #define ZA_MATH_HYPOTL(...) ::std::hypotl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(hypot, HYPOT)
