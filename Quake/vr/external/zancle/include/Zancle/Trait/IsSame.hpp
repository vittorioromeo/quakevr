#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__is_same)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_SAME(a, b) __is_same(a, b)

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename, typename>
inline constexpr bool isSameImpl = false;


////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isSameImpl<T, T> = true;

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_IS_SAME(a, b) ::za::priv::isSameImpl<a, b>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename A, typename B>
inline constexpr bool isSame = ZA_IS_SAME(A, B);

} // namespace za
