#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Assign `value` to every element of `[first, last)`
///
/// Same semantics as `std::fill`.
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename T>
[[gnu::always_inline]] constexpr void fill(ForwardIt first, const ForwardIt last, const T& value)
{
    for (; first != last; ++first)
        *first = value;
}

} // namespace za
