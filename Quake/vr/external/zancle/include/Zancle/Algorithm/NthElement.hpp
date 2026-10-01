#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Algorithm/Sort.hpp"

#include "Zancle/Base/Assert.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Partially sort `[first, last)` so that `*nth` is the element a full sort would put there
///
/// Same semantics as `std::nth_element`: afterwards, no element before
/// `nth` is greater than `*nth`, and no element after it is less. The
/// order within the two sides is otherwise unspecified (and in general
/// differs from `std::nth_element`'s). Average `O(n)`: introselect, with
/// the same partitioning as `quickSort`, and a heapsort fallback for
/// pathological inputs (`O(n log n)` worst case).
///
/// `comp` must be a strict weak ordering (e.g. `<`, never `<=`). Does
/// nothing if `nth == last`.
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void nthElement(RandomIt first, const RandomIt nth, RandomIt last, auto&& comp)
{
    ZA_ASSERT(first <= nth && nth <= last);

    if (nth == last || last - first < 2)
        return;

    ZA_ASSERT(!comp(*first, *first) && "`comp` must be a strict weak ordering (e.g. `<`, not `<=`)");

    int depthLimit = priv::introDepthLimit(last - first);

    while (last - first > priv::insertionSortCutoffThreshold)
    {
        if (depthLimit-- == 0)
        {
            heapSort(first, last, comp);
            return;
        }

        const RandomIt pivot = priv::partitionAroundMedianOfThree(first, last, comp);

        if (pivot == nth)
            return;

        // Only the side containing `nth` needs further work
        if (nth < pivot)
            last = pivot;
        else
            first = pivot + 1;
    }

    insertionSort(first, last, comp);
}


////////////////////////////////////////////////////////////
/// \brief `nthElement` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void nthElement(const RandomIt first, const RandomIt nth, const RandomIt last)
{
    nthElement(first, nth, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za
