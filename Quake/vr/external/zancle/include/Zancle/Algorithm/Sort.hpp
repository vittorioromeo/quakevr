#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Swap.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Insertion sort using `comp` as a strict-weak less-than
///
/// Elements already in place are only compared, never moved, so
/// sorted and nearly sorted ranges are cheap.
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void insertionSort(const RandomIt first, const RandomIt last, auto&& comp)
{
    if (last - first < 2)
        return;

    for (auto i = first + 1; i < last; ++i)
    {
        if (!comp(*i, *(i - 1)))
            continue;

        auto key = ZA_MOVE(*i);
        auto j   = i;

        do
        {
            *j = ZA_MOVE(*(j - 1));
            --j;
        } while (j > first && comp(key, *(j - 1)));

        *j = ZA_MOVE(key);
    }
}


////////////////////////////////////////////////////////////
/// \brief `insertionSort` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void insertionSort(const RandomIt first, const RandomIt last)
{
    insertionSort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Move `value` into the hole at `hole` of the max-heap `[first, first + count)`,
///        moving larger children up until the heap property holds
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename Diff, typename T>
inline constexpr void heapSiftDown(const RandomIt first, Diff hole, const Diff count, T& value, auto&& comp)
{
    while (true)
    {
        Diff child = 2 * hole + 1;

        if (child >= count)
            break;

        if (child + 1 < count && comp(*(first + child), *(first + (child + 1))))
            ++child;

        if (!comp(value, *(first + child)))
            break;

        *(first + hole) = ZA_MOVE(*(first + child));
        hole            = child;
    }

    *(first + hole) = ZA_MOVE(value);
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Sort `[first, last)` in non-descending order under `comp` (unstable, heapsort)
///
/// Guaranteed `O(n log n)` comparisons and `O(1)` extra space, but
/// usually slower than `quickSort` due to poor memory locality.
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void heapSort(const RandomIt first, const RandomIt last, auto&& comp)
{
    const auto count = last - first;

    if (count < 2)
        return;

    // Build a max-heap
    for (auto start = count / 2; start-- > 0;)
    {
        auto value = ZA_MOVE(*(first + start));
        priv::heapSiftDown(first, start, count, value, comp);
    }

    // Repeatedly move the maximum to the end of the shrinking heap
    for (auto end = count - 1; end > 0; --end)
    {
        auto value     = ZA_MOVE(*(first + end));
        *(first + end) = ZA_MOVE(*first);
        priv::heapSiftDown(first, decltype(end){0}, end, value, comp);
    }
}


////////////////////////////////////////////////////////////
/// \brief `heapSort` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void heapSort(const RandomIt first, const RandomIt last)
{
    heapSort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Introsort: median-of-three quicksort with Hoare partitioning, insertion sort for small
///        partitions, recursion on the smaller side (`O(log n)` stack depth), and a heapsort
///        fallback once `depthLimit` partitioning rounds are exhausted (`O(n log n)` worst case)
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
inline constexpr void quickSortImpl(RandomIt first, RandomIt last, int depthLimit, auto&& comp)
{
    enum : int
    {
        insertionSortCutoffThreshold = 16
    };

    while (last - first > insertionSortCutoffThreshold)
    {
        // Pathological input (e.g. median-of-three killer sequences): bail out to heapsort
        if (depthLimit-- == 0)
        {
            heapSort(first, last, comp);
            return;
        }

        // --- Median-of-three pivot selection ---
        // 1. Choose three elements: first, middle, and last.
        const RandomIt mid = first + (last - first) / 2;

        // 2. Sort these three elements to find the median.
        if (comp(*mid, *first))
            iterSwap(first, mid);

        if (comp(*(last - 1), *first))
            iterSwap(first, last - 1);

        if (comp(*(last - 1), *mid))
            iterSwap(mid, last - 1);

        // 3. The median is now at `mid`. Place it just before the end
        //    to act as the pivot. The element at `last-1` is now a sentinel,
        //    guaranteed to be >= the pivot.
        iterSwap(mid, last - 2);
        RandomIt pivot = last - 2;

        // --- Hoare-like Partitioning ---
        RandomIt i = first;
        RandomIt j = last - 2;

        while (true)
        {
            // The elements at `first` and `last-1` act as sentinels,
            // so we don't need boundary checks inside the loops.
            // (This relies on `comp` being a strict weak ordering.)
            while (comp(*++i, *pivot))
                ;

            while (comp(*pivot, *(--j)))
                ;

            if (i >= j)
                break;

            iterSwap(i, j);
        }

        // Restore the pivot to its final sorted position.
        iterSwap(i, pivot);

        // --- Tail-call optimization ---
        // Recurse on the smaller partition and loop on the larger one
        // to limit recursion depth to O(log n).
        if ((i - first) < (last - (i + 1)))
        {
            quickSortImpl(first, i, depthLimit, comp);
            first = i + 1; // loop on the larger right part
        }
        else
        {
            quickSortImpl(i + 1, last, depthLimit, comp);
            last = i; // loop on the larger left part
        }
    }

    // For the final small partitions, run insertion sort.
    insertionSort(first, last, comp);
}

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Sort `[first, last)` in non-descending order under `comp` (unstable, introsort)
///
/// `comp` must be a strict weak ordering (e.g. `<`, never `<=`):
/// otherwise the behavior is undefined, as partitioning relies on it
/// to stay within the range.
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void quickSort(const RandomIt first, const RandomIt last, auto&& comp)
{
    if (last - first < 2)
        return;

    ZA_ASSERT(!comp(*first, *first) && "`comp` must be a strict weak ordering (e.g. `<`, not `<=`)");

    // `2 * floor(log2(n))` partitioning rounds before falling back to heapsort
    int depthLimit = 0;
    for (auto n = last - first; n > 1; n /= 2)
        depthLimit += 2;

    priv::quickSortImpl(first, last, depthLimit, comp);
}


////////////////////////////////////////////////////////////
/// \brief `quickSort` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt>
[[gnu::always_inline]] inline constexpr void quickSort(const RandomIt first, const RandomIt last)
{
    quickSort(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za
