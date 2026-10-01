#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief First element of the sorted range `[first, last)` that is greater than `value`, or `last`
///
/// Same semantics as `std::upper_bound`: `[first, last)` must be
/// partitioned with respect to `!comp(value, element)`, and `comp` is
/// called with the value first, at most `log2(last - first) + 1` times.
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename T, typename Comparer>
[[nodiscard, gnu::always_inline]] constexpr RandomIt upperBound(RandomIt first, const RandomIt last, const T& value, Comparer&& comp)
{
    auto count = last - first;

    while (count > 0)
    {
        const auto     step   = count / 2;
        const RandomIt middle = first + step;

        if (!comp(value, *middle))
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
/// \brief `upperBound` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename RandomIt, typename T>
[[nodiscard, gnu::always_inline, gnu::flatten]] constexpr RandomIt upperBound(const RandomIt first,
                                                                              const RandomIt last,
                                                                              const T&       value)
{
    return upperBound(first, last, value, [](const auto& v, const auto& element) { return v < element; });
}

} // namespace za
