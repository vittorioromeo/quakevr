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
/// Only for contiguous sequences (e.g. `za::Vector`, `za::InPlaceVector`,
/// `std::vector`), checked via `data()`. Other containers with `begin`,
/// `end`, and `erase` would compile but be corrupted: e.g. a dense hash
/// map's values would move without their buckets. For those, use their own
/// `erase_if` (e.g. `erase_if(map, predicate)` for `ankerl::unordered_dense`
/// maps, found by argument-dependent lookup).
///
////////////////////////////////////////////////////////////
template <typename Vector, typename Predicate>
    requires requires(Vector& vector) { vector.data(); }
[[gnu::always_inline]] inline constexpr SizeT vectorEraseIf(Vector& vector, Predicate&& predicate)
{
    const auto it       = removeIf(vector.begin(), vector.end(), predicate);
    const auto nRemoved = static_cast<SizeT>(vector.end() - it);

    vector.erase(it, vector.end());
    return nRemoved;
}

} // namespace za
