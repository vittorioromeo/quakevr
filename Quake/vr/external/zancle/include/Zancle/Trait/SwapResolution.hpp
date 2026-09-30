#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Trait/DeclVal.hpp"


namespace za::priv::swap_adl
{
////////////////////////////////////////////////////////////
// A deleted generic `swap` template. A *specialized* ADL `swap` wins
// over it, so an unqualified `swap(a, b)` picks a real overload when one
// exists and is ill-formed otherwise (rather than silently binding a
// generic fallback, or colliding ambiguously with a generic `std::swap`).
//
// This is the single source of truth for how a swap is resolved: it is
// shared by the `genericSwap` CPO and the `isNoThrowSwappable` trait.
//
////////////////////////////////////////////////////////////
template <typename T>
void swap(T&, T&) = delete;


////////////////////////////////////////////////////////////
template <typename T>
concept MemberSwappable = requires(T& a, T& b) { a.swap(b); };


////////////////////////////////////////////////////////////
template <typename T>
concept AdlSwappable = requires(T& a, T& b) { swap(a, b); };


////////////////////////////////////////////////////////////
template <typename T>
concept MoveSwappable = requires(T& a) {
    T(static_cast<T&&>(a));
    a = static_cast<T&&>(a);
};


////////////////////////////////////////////////////////////
/// \brief `true` iff the swap `SwapFn` performs for `T` is `noexcept`
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] consteval bool computeNoThrowSwap() noexcept
{
    if constexpr (MemberSwappable<T>)
        return noexcept(za::declVal<T&>().swap(za::declVal<T&>()));
    else if constexpr (AdlSwappable<T>)
        return noexcept(swap(za::declVal<T&>(), za::declVal<T&>()));
    else if constexpr (MoveSwappable<T>)
        return noexcept(T(za::declVal<T&&>())) && noexcept(za::declVal<T&>() = za::declVal<T&&>());
    else
        return false;
}


////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isNoThrowSwappableV = computeNoThrowSwap<T>();


////////////////////////////////////////////////////////////
// `SwapFn` swaps arrays element-wise, so an array is nothrow-swappable
// exactly when its element type is (recursing through nested arrays).
template <typename T, decltype(sizeof(0)) N>
inline constexpr bool isNoThrowSwappableV<T[N]> = isNoThrowSwappableV<T>;

} // namespace za::priv::swap_adl
