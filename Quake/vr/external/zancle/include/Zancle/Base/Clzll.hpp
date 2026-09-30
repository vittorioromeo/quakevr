#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__builtin_clzll)

    ////////////////////////////////////////////////////////////
    #define ZA_CLZLL __builtin_clzll

#else

    #include <bit>

    ////////////////////////////////////////////////////////////
    // Converts to `unsigned long long` first, as the `std` function is
    // type-generic while the builtin always operates on 64 bits
    #define ZA_CLZLL(...) ::std::countl_zero(static_cast<unsigned long long>(__VA_ARGS__))

#endif
