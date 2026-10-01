#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Algorithm/Find.hpp"

#include "Zancle/Base/Macros.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Move elements not satisfying `predicate` to the front of `[first, last)`
///
/// Elements past the returned iterator are in a valid but unspecified state.
///
/// \return Iterator to the new logical end of the range
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename Predicate>
[[nodiscard, gnu::always_inline]] inline constexpr ForwardIt removeIf(ForwardIt first, const ForwardIt last, Predicate&& predicate)
{
    first = findIf(first, last, predicate);

    if (first != last)
        for (ForwardIt i = first; ++i != last;)
            if (!predicate(*i))
                *first++ = ZA_MOVE(*i);

    return first;
}

} // namespace za
