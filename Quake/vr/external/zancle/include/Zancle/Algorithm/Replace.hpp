#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Replace every element of `[first, last)` equal to `oldValue` with `newValue`
///
/// Same semantics as `std::replace`, including taking both values by
/// reference: if `oldValue` refers to an element of the range, later
/// elements are compared against its replaced value.
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename T>
[[gnu::always_inline]] constexpr void replace(ForwardIt first, const ForwardIt last, const T& oldValue, const T& newValue)
{
    for (; first != last; ++first)
        if (*first == oldValue)
            *first = newValue;
}

} // namespace za
