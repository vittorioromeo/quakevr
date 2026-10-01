#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__builtin_bit_cast)

    ////////////////////////////////////////////////////////////
    #define ZA_BIT_CAST(type, ...) __builtin_bit_cast(type, __VA_ARGS__)

#else

    #include <bit>

    ////////////////////////////////////////////////////////////
    #define ZA_BIT_CAST(type, ...) ::std::bit_cast<type>(__VA_ARGS__)

#endif
