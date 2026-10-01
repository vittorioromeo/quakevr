#pragma once

// vr_zancle.hpp -- what the Quake VR code needs that Zancle (external/zancle, vendored) does not have yet: small
// stand-ins in namespace qza, written the way Zancle writes its own (compiler builtins behind macros, always-inline
// templates that take exactly float, double or long double). Each is a proposal for Zancle (ZANCLE-TODO; the list in
// docs/vr-port/ROUND21.md, "Zancle migration"): when Zancle has it, the call sites move to it and this goes.

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

namespace qza
{

// vr_zancle_math_test (vr_zancle.cpp): Zancle's math against the standard library's, on edge values.
void mathTest_f();

// ZANCLE-TODO: no map whose values stay where they are (std::unordered_map's nodes): a dense map's values move when it
// grows. Where a value's address is kept past the next insertion (a cache returning pointers into itself), the map
// holds za::UniquePtr<T>: the value at `key`, made (T{}) on first use.
template <typename T, typename Map, typename Key>
[[nodiscard]] T& stableAt(Map& map, const Key& key)
{
    auto& slot = map[key];
    if(!slot)
    {
        slot = za::makeUnique<T>();
    }
    return *slot;
}

// ZANCLE-TODO: no ordered map (a sorted flat map): where a std::map's order was used (its loops), the unordered map's
// entries sorted by key (the same order: keys are unique).
template <typename Map>
[[nodiscard]] za::Vector<const typename Map::value_type*> sortedByKey(const Map& map)
{
    za::Vector<const typename Map::value_type*> entries;
    entries.reserve(map.size());
    for(const auto& e : map)
    {
        entries.pushBack(&e);
    }
    za::quickSort(entries.begin(), entries.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
    return entries;
}

} // namespace qza
