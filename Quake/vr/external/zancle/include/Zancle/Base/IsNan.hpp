#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__builtin_isnan)

    ////////////////////////////////////////////////////////////
    #define ZA_ISNAN __builtin_isnan

#else

    #include <cmath>

    ////////////////////////////////////////////////////////////
    #define ZA_ISNAN ::std::isnan

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable wrapper for `__builtin_isnan` / `std::isnan`
///
/// \warning Under `-ffinite-math-only` (implied by `-ffast-math`, which
///          the release presets use), the compiler assumes that no
///          NaN values exist and folds `ZA_ISNAN(x)` to `false`.
///          Guard such checks with `#if !__FINITE_MATH_ONLY__` (as
///          `ToChars.hpp` does), or inspect the bit pattern via
///          `ZA_BIT_CAST` when the check must work in every build.
///
////////////////////////////////////////////////////////////
