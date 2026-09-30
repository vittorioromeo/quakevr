#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_memset)

    ////////////////////////////////////////////////////////////
    #define ZA_MEMSET __builtin_memset

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_MEMSET ::std::memset

#endif
