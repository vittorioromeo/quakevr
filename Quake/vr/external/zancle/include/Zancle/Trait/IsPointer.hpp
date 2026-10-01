#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_pointer)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_POINTER(...) __is_pointer(__VA_ARGS__)

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename>
inline constexpr bool isPointerImpl = false;

////////////////////////////////////////////////////////////
// clang-format off
template <typename T> inline constexpr bool isPointerImpl<T*>                = true;
template <typename T> inline constexpr bool isPointerImpl<T* const>          = true;
template <typename T> inline constexpr bool isPointerImpl<T* volatile>       = true;
template <typename T> inline constexpr bool isPointerImpl<T* const volatile> = true;
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_IS_POINTER(...) ::za::priv::isPointerImpl<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isPointer = ZA_IS_POINTER(T);

} // namespace za
