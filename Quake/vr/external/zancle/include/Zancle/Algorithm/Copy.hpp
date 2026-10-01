#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/IsConstantEvaluated.hpp"
#include "Zancle/Base/Memmove.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyAssignable.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename SrcIt, typename DstIt>
inline constexpr bool isMemmoveCopyable = false;


////////////////////////////////////////////////////////////
template <typename T, typename U>
inline constexpr bool isMemmoveCopyable<T*, U*> = (ZA_IS_SAME(T, U) || ZA_IS_SAME(T, const U)) &&
                                                  ZA_IS_TRIVIALLY_ASSIGNABLE(U&, T&);

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Copy `[rangeBegin, rangeEnd)` to `targetIter`, returning the destination end iterator
///
/// Copies between pointers to the same trivially assignable type use `memmove`.
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename TargetForwardIt>
[[gnu::always_inline]] constexpr TargetForwardIt copy(ForwardIt rangeBegin, const ForwardIt rangeEnd, TargetForwardIt targetIter)
{
    if constexpr (priv::isMemmoveCopyable<ForwardIt, TargetForwardIt>)
    {
        if (!ZA_IS_CONSTANT_EVALUATED())
        {
            const auto count = static_cast<SizeT>(rangeEnd - rangeBegin);

            if (count != 0u) // avoid `memmove(null, null, 0)` (UB) for an empty range
                ZA_MEMMOVE(targetIter, rangeBegin, count * sizeof(*targetIter));

            return targetIter + count;
        }
    }

    while (rangeBegin != rangeEnd)
        *targetIter++ = *rangeBegin++;

    return targetIter;
}

} // namespace za
