#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `true` if at least one element in the range satisfies `predicate`
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename Predicate>
[[nodiscard, gnu::always_inline]] constexpr bool anyOf(ForwardIt rangeBegin, const ForwardIt rangeEnd, Predicate&& predicate)
{
    for (; rangeBegin != rangeEnd; ++rangeBegin)
        if (predicate(*rangeBegin))
            return true;

    return false;
}

} // namespace za
