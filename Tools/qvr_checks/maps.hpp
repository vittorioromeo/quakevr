// qvr_checks: the dense maps and sets (Zancle's ankerl::unordered_dense) and their ordered walks.
#pragma once

#include "../../Quake/vr/vr_zancle.hpp" // qza::sortedByKey
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"

namespace qvrc
{

template <typename K, typename V>
using Map = ankerl::unordered_dense::map<K, V>;

template <typename K>
using Set = ankerl::unordered_dense::set<K>;

// A set's items in order (a std::set's walk; the items are unique, so the sort has no ties).
template <typename T>
[[nodiscard]] za::Vector<T> sortedItems(const Set<T>& s)
{
    za::Vector<T> v;
    v.reserve(s.size());
    for(const auto& x : s)
    {
        v.pushBack(x);
    }
    za::quickSort(v.begin(), v.end(), [](const T& a, const T& b) { return a < b; });
    return v;
}

} // namespace qvrc
