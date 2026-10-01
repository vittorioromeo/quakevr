#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_void)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_VOID(...) __is_void(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/IsSame.hpp"


    ////////////////////////////////////////////////////////////
    #define ZA_IS_VOID(...)                                                      \
        (ZA_IS_SAME(__VA_ARGS__, void) || ZA_IS_SAME(__VA_ARGS__, const void) || \
         ZA_IS_SAME(__VA_ARGS__, volatile void) || ZA_IS_SAME(__VA_ARGS__, const volatile void))

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isVoid = ZA_IS_VOID(T);

} // namespace za
