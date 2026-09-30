#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_memcmp)

    ////////////////////////////////////////////////////////////
    #define ZA_MEMCMP __builtin_memcmp

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_MEMCMP ::std::memcmp

#endif
