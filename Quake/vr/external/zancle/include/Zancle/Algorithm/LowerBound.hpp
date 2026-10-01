#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief First element of the sorted range `[first, last)` that is not less than `value`, or `last`
///
/// Same semantics as `std::lower_bound`: `[first, last)` must be
/// partitioned with respect to `comp(element, value)`, which is called
/// with the element first, at most `log2(last - first) + 1` times.
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename T, typename Comparer>
[[nodiscard, gnu::always_inline]] constexpr RandomIt lowerBound(RandomIt first, const RandomIt last, const T& value, Comparer&& comp)
{
    auto count = last - first;

    while (count > 0)
    {
        const auto     step   = count / 2;
        const RandomIt middle = first + step;

        if (comp(*middle, value))
        {
            first = middle + 1;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }

    return first;
}


////////////////////////////////////////////////////////////
/// \brief `lowerBound` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename T>
[[nodiscard, gnu::always_inline, gnu::flatten]] constexpr RandomIt lowerBound(const RandomIt first,
                                                                              const RandomIt last,
                                                                              const T&       value)
{
    return lowerBound(first, last, value, [](const auto& element, const auto& v) { return element < v; });
}

} // namespace za
