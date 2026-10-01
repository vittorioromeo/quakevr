#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/SizeT.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Whether `[lhs, lhs + lhsSize)` orders before `[rhs, rhs + rhsSize)`, element by element
///
/// Same semantics as `std::lexicographical_compare`: the first pair of
/// elements that differ decides, otherwise the shorter range is less.
/// Uses only the elements' `operator<`.
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] constexpr bool lexicographicLess(const T* lhs, const SizeT lhsSize, const T* rhs, const SizeT rhsSize)
{
    for (const T* const lhsEnd = lhs + (lhsSize < rhsSize ? lhsSize : rhsSize); lhs != lhsEnd; ++lhs, ++rhs)
    {
        if (*lhs < *rhs)
            return true;

        if (*rhs < *lhs)
            return false;
    }

    return lhsSize < rhsSize;
}

} // namespace za::priv
