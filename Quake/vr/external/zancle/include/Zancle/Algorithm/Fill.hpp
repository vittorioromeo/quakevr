#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/SizeT.hpp"


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


////////////////////////////////////////////////////////////
/// \brief Assign `value` to every element of `range` (e.g. a `za::Array`, `za::Vector`, or `za::Span`)
///
////////////////////////////////////////////////////////////
template <typename Range, typename T>
    requires requires(Range&& range) {
        range.begin();
        range.end();
    }
[[gnu::always_inline]] constexpr void fill(Range&& range, const T& value)
{
    za::fill(range.begin(), range.end(), value);
}


////////////////////////////////////////////////////////////
/// \brief Assign `value` to every element of a C-style array
///
////////////////////////////////////////////////////////////
template <typename Element, SizeT N, typename T>
[[gnu::always_inline]] constexpr void fill(Element (&array)[N], const T& value)
{
    za::fill(array + 0, array + N, value);
}

} // namespace za
