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
/// \brief Count elements in the range that convert to `true`
///
////////////////////////////////////////////////////////////
template <typename ForwardIt>
[[nodiscard, gnu::always_inline]] constexpr SizeT countTruthy(ForwardIt rangeBegin, const ForwardIt rangeEnd)
{
    SizeT result = 0u;

    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (static_cast<bool>(*rangeBegin))
            ++result;

    return result;
}


////////////////////////////////////////////////////////////
/// \brief Count elements in the range equal to `value`
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename T>
[[nodiscard, gnu::always_inline]] constexpr SizeT count(ForwardIt rangeBegin, const ForwardIt rangeEnd, const T& value)
{
    SizeT result = 0u;

    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (*rangeBegin == value)
            ++result;

    return result;
}


////////////////////////////////////////////////////////////
/// \brief Count elements in the range satisfying `predicate`
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename Predicate>
[[nodiscard, gnu::always_inline]] constexpr SizeT countIf(ForwardIt rangeBegin, const ForwardIt rangeEnd, Predicate&& predicate)
{
    SizeT result = 0u;

    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (predicate(*rangeBegin))
            ++result;

    return result;
}

} // namespace za
