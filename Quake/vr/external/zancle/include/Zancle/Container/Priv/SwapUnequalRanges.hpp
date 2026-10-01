#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"


namespace za::priv::VectorUtils
{
////////////////////////////////////////////////////////////
/// \brief Swap the contents of two inline buffers holding `lhsSize` and `rhsSize` elements
///
/// Swaps the common prefix element-wise, relocates the longer buffer's
/// tail into the shorter one, and swaps the sizes. Used by the vectors
/// with inline storage (`SmallVector`, `InPlaceVector`).
///
////////////////////////////////////////////////////////////
template <typename T>
[[gnu::always_inline]] inline constexpr void swapUnequalRanges(T* lhsData, SizeT& lhsSize, T* rhsData, SizeT& rhsSize)
{
    const SizeT s1 = lhsSize;
    const SizeT s2 = rhsSize;

    const SizeT commonSize = s2 < s1 ? s2 : s1;

    for (SizeT i = 0u; i < commonSize; ++i)
        za::genericSwap(lhsData[i], rhsData[i]); // Swap elements in the common part

    if (s1 > s2) // `lhs` is larger; its tail elements move to `rhs`
        relocateRange(rhsData + commonSize, lhsData + commonSize, lhsData + s1);
    else if (s2 > s1) // `rhs` is larger; its tail elements move to `lhs`
        relocateRange(lhsData + commonSize, rhsData + commonSize, rhsData + s2);

    lhsSize = s2;
    rhsSize = s1;
}

} // namespace za::priv::VectorUtils
