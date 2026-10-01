#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_strlen)

    ////////////////////////////////////////////////////////////
    #define ZA_STRLEN __builtin_strlen

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_STRLEN ::std::strlen

#endif
