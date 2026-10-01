#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_signbit)

    ////////////////////////////////////////////////////////////
    #define ZA_SIGNBIT __builtin_signbit

#else

    #include <cmath>

    ////////////////////////////////////////////////////////////
    #define ZA_SIGNBIT ::std::signbit

#endif
