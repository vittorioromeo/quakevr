#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Find the first element equal to `target`, or `rangeEnd` if not found
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename T>
[[nodiscard, gnu::always_inline]] constexpr ForwardIt find(ForwardIt rangeBegin, const ForwardIt rangeEnd, const T& target)
{
    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (*rangeBegin == target)
            return rangeBegin;

    return rangeEnd;
}


////////////////////////////////////////////////////////////
/// \brief Find the first element satisfying `predicate`, or `rangeEnd` if not found
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename Predicate>
[[nodiscard, gnu::always_inline]] constexpr ForwardIt findIf(ForwardIt rangeBegin, const ForwardIt rangeEnd, Predicate&& predicate)
{
    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (predicate(*rangeBegin))
            return rangeBegin;

    return rangeEnd;
}

} // namespace za
