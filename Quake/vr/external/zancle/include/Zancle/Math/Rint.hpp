#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(rint)
    #define ZA_MATH_RINT(...)  __builtin_rint(__VA_ARGS__)
    #define ZA_MATH_RINTF(...) __builtin_rintf(__VA_ARGS__)
    #define ZA_MATH_RINTL(...) __builtin_rintl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::rint` has `float` overloads: convert like `__builtin_rint`, which only takes `double`
    #define ZA_MATH_RINT(x)    ::std::rint(static_cast<double>(x))
    #define ZA_MATH_RINTF(...) ::std::rintf(__VA_ARGS__)
    #define ZA_MATH_RINTL(...) ::std::rintl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(rint, RINT)
