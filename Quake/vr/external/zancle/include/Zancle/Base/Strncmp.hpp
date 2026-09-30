#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_strncmp)

    ////////////////////////////////////////////////////////////
    #define ZA_STRNCMP __builtin_strncmp

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_STRNCMP ::std::strncmp

#endif
