#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Algorithm/Remove.hpp"

#include "Zancle/Base/SizeT.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Stable in-place equivalent of `std::erase_if` for vectors; returns the number of elements removed
///
////////////////////////////////////////////////////////////
template <typename Vector, typename Predicate>
[[gnu::always_inline]] inline constexpr SizeT vectorEraseIf(Vector& vector, Predicate&& predicate)
{
    const auto it       = removeIf(vector.begin(), vector.end(), predicate);
    const auto nRemoved = static_cast<SizeT>(vector.end() - it);

    vector.erase(it, vector.end());
    return nRemoved;
}

} // namespace za
