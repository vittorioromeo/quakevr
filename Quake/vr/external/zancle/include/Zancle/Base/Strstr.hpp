#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_strstr)

    ////////////////////////////////////////////////////////////
    #define ZA_STRSTR __builtin_strstr

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_STRSTR ::std::strstr

#endif
