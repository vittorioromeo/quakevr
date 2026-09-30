#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#include "Zancle/Math/Priv/Impl.hpp"


////////////////////////////////////////////////////////////
#if ZA_PRIV_HAS_MATH_BUILTIN(fmod)
    #define ZA_MATH_FMOD(...)  __builtin_fmod(__VA_ARGS__)
    #define ZA_MATH_FMODF(...) __builtin_fmodf(__VA_ARGS__)
    #define ZA_MATH_FMODL(...) __builtin_fmodl(__VA_ARGS__)
#else
    #include <cmath> // IWYU pragma: keep

    // `::std::fmod` has `float` overloads: convert like `__builtin_fmod`, which only takes `double`
    #define ZA_MATH_FMOD(x, y) ::std::fmod(static_cast<double>(x), static_cast<double>(y))
    #define ZA_MATH_FMODF(...) ::std::fmodf(__VA_ARGS__)
    #define ZA_MATH_FMODL(...) ::std::fmodl(__VA_ARGS__)
#endif


////////////////////////////////////////////////////////////
ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(fmod, FMOD)
