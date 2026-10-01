#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Algorithm/LowerBound.hpp"
#include "Zancle/Algorithm/Rotate.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/UpperBound.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Swap.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Stably merge the sorted `[first, middle)` and `[middle, last)` in place, without a buffer
///
/// Divide and conquer: split the longer run in half, find where its
/// middle element belongs in the other run (binary search), rotate the
/// two inner parts into place, and recurse on both halves.
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void mergeWithoutBuffer(const RandomIt first, const RandomIt middle, const RandomIt last, auto&& comp)
{
    const auto length1 = middle - first;
    const auto length2 = last - middle;

    if (length1 == 0 || length2 == 0)
        return;

    if (length1 + length2 == 2)
    {
        if (comp(*middle, *first))
            iterSwap(first, middle);

        return;
    }

    RandomIt firstCut;
    RandomIt secondCut;

    if (length1 > length2)
    {
        firstCut  = first + length1 / 2;
        secondCut = lowerBound(middle, last, *firstCut, comp); // equivalents of `*firstCut` stay after it
    }
    else
    {
        secondCut = middle + length2 / 2;
        firstCut  = upperBound(first, middle, *secondCut, comp); // equivalents of `*secondCut` stay before it
    }

    const RandomIt newMiddle = rotate(firstCut, middle, secondCut);

    mergeWithoutBuffer(first, firstCut, newMiddle, comp);
    mergeWithoutBuffer(newMiddle, secondCut, last, comp);
}


////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void stableSortImpl(const RandomIt first, const RandomIt last, auto&& comp)
{
    if (last - first <= insertionSortCutoffThreshold)
    {
        insertionSort(first, last, comp);
        return;
    }

    const RandomIt middle = first + (last - first) / 2;

    stableSortImpl(first, middle, comp);
    stableSortImpl(middle, last, comp);

    // Already in order (e.g. presorted input): nothing to merge
    if (!comp(*middle, *(middle - 1)))
        return;

    mergeWithoutBuffer(first, middle, last, comp);
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Sort `[first, last)` in non-descending order under `comp`, keeping the order of equivalent elements
///
/// Same semantics as `std::stable_sort`, but never allocates: a merge
/// sort that merges in place, by rotations. `O(n log n)` comparisons and
/// `O(n log^2 n)` element swaps (`std::stable_sort` uses the same
/// algorithm when it cannot allocate a buffer). Presorted runs are
/// detected with a single comparison per merge.
///
/// `comp` must be a strict weak ordering (e.g. `<`, never `<=`).
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void stableSort(const RandomIt first, const RandomIt last, auto&& comp)
{
    if (last - first < 2)
        return;

    ZA_ASSERT(!comp(*first, *first) && "`comp` must be a strict weak ordering (e.g. `<`, not `<=`)");

    priv::stableSortImpl(first, last, comp);
}


////////////////////////////////////////////////////////////
/// \brief `stableSort` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void stableSort(const RandomIt first, const RandomIt last)
{
    stableSort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za
