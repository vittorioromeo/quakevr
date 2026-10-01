#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_trivially_destructible)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_DESTRUCTIBLE(...) __is_trivially_destructible(__VA_ARGS__)

#elif ZA_HAS_BUILTIN(__has_trivial_destructor) // e.g. GCC 15, which lacks `__is_trivially_destructible`

namespace za::priv
{
////////////////////////////////////////////////////////////
// `__has_trivial_destructor` alone neither checks that the destructor is accessible and
// not deleted, nor handles references and arrays like `std::is_trivially_destructible`.
// It is only evaluated for callable destructors, as GCC warns otherwise.
template <typename T>
[[nodiscard]] consteval bool isTriviallyDestructibleHelper() noexcept
{
    if constexpr (requires(T& t) { t.~T(); })
        return __has_trivial_destructor(T);
    else
        return false;
}


////////////////////////////////////////////////////////////
// clang-format off
template <typename T>            inline constexpr bool isTriviallyDestructibleImpl          = isTriviallyDestructibleHelper<T>();
template <typename T>            inline constexpr bool isTriviallyDestructibleImpl<T&>      = true;
template <typename T>            inline constexpr bool isTriviallyDestructibleImpl<T&&>     = true;
template <typename T, auto Size> inline constexpr bool isTriviallyDestructibleImpl<T[Size]> = isTriviallyDestructibleImpl<T>;
template <typename T>            inline constexpr bool isTriviallyDestructibleImpl<T[]>     = false;
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_DESTRUCTIBLE(...) ::za::priv::isTriviallyDestructibleImpl<__VA_ARGS__>

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_DESTRUCTIBLE(...) ::std::is_trivially_destructible_v<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isTriviallyDestructible = ZA_IS_TRIVIALLY_DESTRUCTIBLE(T);

} // namespace za
