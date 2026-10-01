#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__builtin_is_constant_evaluated)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()

#else

    #include <type_traits>

    ////////////////////////////////////////////////////////////
    #define ZA_IS_CONSTANT_EVALUATED() ::std::is_constant_evaluated()

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable wrapper for `__builtin_is_constant_evaluated` / `std::is_constant_evaluated`
///
/// Unlike `if consteval`, `ZA_IS_CONSTANT_EVALUATED()` is an expression,
/// so it can be used in a conditional operator or a macro expansion.
///
////////////////////////////////////////////////////////////
