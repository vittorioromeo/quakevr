#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_strcmp)

    ////////////////////////////////////////////////////////////
    #define ZA_STRCMP __builtin_strcmp

#else

    #include <cstring>

    ////////////////////////////////////////////////////////////
    #define ZA_STRCMP ::std::strcmp

#endif
