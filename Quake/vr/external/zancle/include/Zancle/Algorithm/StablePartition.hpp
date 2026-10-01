#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Algorithm/Rotate.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Stably partition `[first, first + count)`, with `count > 0`
///
/// Partitions both halves, then rotates the left half's `false` block
/// past the right half's `true` block. Each element is passed to
/// `predicate` exactly once, from left to right.
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename Diff, typename Predicate>
constexpr RandomIt stablePartitionImpl(const RandomIt first, const Diff count, Predicate& predicate)
{
    if (count == 1)
        return predicate(*first) ? first + 1 : first;

    const Diff     leftCount = count / 2;
    const RandomIt middle    = first + leftCount;

    const RandomIt leftEnd  = stablePartitionImpl(first, leftCount, predicate);
    const RandomIt rightEnd = stablePartitionImpl(middle, count - leftCount, predicate);

    // `[leftEnd, middle)` is false, `[middle, rightEnd)` is true: swap the two blocks
    return ::za::rotate(leftEnd, middle, rightEnd); // qualified: ADL would also find `std::rotate` for `std` iterators
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Reorder `[first, last)` so that the elements satisfying `predicate` precede the others,
///        preserving the relative order of the elements within each group
///
/// Same semantics as `std::stable_partition`: `predicate` is called
/// exactly once per element. Works in place, without allocating:
/// `O(N log N)` swaps and `O(log N)` stack depth.
///
/// \return Iterator to the first element of the second group (or `last` if there is none)
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename Predicate>
[[gnu::always_inline]] inline constexpr RandomIt stablePartition(const RandomIt first, const RandomIt last, Predicate&& predicate)
{
    const auto count = last - first;

    if (count <= 0)
        return first;

    return priv::stablePartitionImpl(first, count, predicate);
}

} // namespace za
