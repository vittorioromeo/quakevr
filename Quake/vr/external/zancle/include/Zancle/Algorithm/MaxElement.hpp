#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Iterator to the greatest element under `comp` (first such if ties), or `last` if empty
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename Comparer>
[[nodiscard, gnu::always_inline]] constexpr ForwardIt maxElement(ForwardIt first, const ForwardIt last, Comparer&& comp)
{
    if (first == last)
        return last;

    ForwardIt largest = first;

    while (++first != last)
        if (comp(*largest, *first))
            largest = first;

    return largest;
}


////////////////////////////////////////////////////////////
/// \brief `maxElement` overload using `operator<`
///
////////////////////////////////////////////////////////////
template <typename ForwardIt>
[[nodiscard, gnu::always_inline, gnu::flatten]] constexpr ForwardIt maxElement(const ForwardIt first, const ForwardIt last)
{
    return maxElement(first, last, [](const auto& a, const auto& b) { return a < b; });
}

} // namespace za
