#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/SizeT.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Compile-time element count of a C-style array
///
////////////////////////////////////////////////////////////
template <typename T, auto N>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr SizeT getArraySize(const T (&)[N]) noexcept
{
    return N;
}


////////////////////////////////////////////////////////////
/// \brief Compile-time element count of a C-style array member
///
////////////////////////////////////////////////////////////
template <typename S, typename T, auto N>
[[nodiscard, gnu::always_inline, gnu::const]] constexpr SizeT getArraySize(const T (S::*)[N]) noexcept
{
    return N;
}

} // namespace za
