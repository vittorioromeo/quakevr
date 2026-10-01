#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__builtin_isfinite)

    ////////////////////////////////////////////////////////////
    #define ZA_ISFINITE __builtin_isfinite

#else

    #include <cmath>

    ////////////////////////////////////////////////////////////
    #define ZA_ISFINITE ::std::isfinite

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable wrapper for `__builtin_isfinite` / `std::isfinite`
///
/// `ZA_ISFINITE(x)` is `true` unless `x` is an infinity or a NaN.
///
/// \warning Under `-ffinite-math-only` (implied by `-ffast-math`, which
///          the release presets use), the compiler assumes that no
///          infinities or NaN values exist and folds `ZA_ISFINITE(x)`
///          to `true`. Guard such checks with `#if !__FINITE_MATH_ONLY__`
///          (as `ToChars.hpp` does), or inspect the bit pattern via
///          `ZA_BIT_CAST` when the check must work in every build.
///
////////////////////////////////////////////////////////////
