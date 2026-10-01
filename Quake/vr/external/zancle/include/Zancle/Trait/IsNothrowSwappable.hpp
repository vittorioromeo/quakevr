#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Trait/SwapResolution.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Whether `genericSwap` on two `T` lvalues cannot throw
///
/// Follows the same dispatch as `genericSwap` (member `swap`, then
/// unambiguous ADL `swap`, then move-construct + move-assign; arrays
/// element-wise) and reports the `noexcept`-ness of the selected
/// operation. Equivalent to `noexcept(genericSwap(a, b))`, as both
/// share the same implementation (see `Zancle/Trait/SwapResolution.hpp`).
///
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isNoThrowSwappable = priv::swap_adl::isNoThrowSwappableV<T>;

} // namespace za


////////////////////////////////////////////////////////////
#define ZA_IS_NOTHROW_SWAPPABLE(...) ::za::isNoThrowSwappable<__VA_ARGS__>
