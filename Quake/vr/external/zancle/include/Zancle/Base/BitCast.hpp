#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_bit_cast)

    ////////////////////////////////////////////////////////////
    #define ZA_BIT_CAST(type, ...) __builtin_bit_cast(type, __VA_ARGS__)

#else

    #include <bit>

    ////////////////////////////////////////////////////////////
    #define ZA_BIT_CAST(type, ...) ::std::bit_cast<type>(__VA_ARGS__)

#endif
