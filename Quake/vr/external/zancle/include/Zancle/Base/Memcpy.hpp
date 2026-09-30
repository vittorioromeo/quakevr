#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_memcpy)

    ////////////////////////////////////////////////////////////
    #define ZA_MEMCPY __builtin_memcpy

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_MEMCPY ::std::memcpy

#endif
