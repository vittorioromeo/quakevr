// vr_evict.hpp -- dropping the stale entries of a per-entity cache (vr_ambient.cpp, vr_modellight.cpp): each entry
// notes the host frame it was last used in (`frame`); past a size limit, the ones not used for a while go.

#pragma once

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr
{

// When a cache over its limit may next have something to drop. A scan that dropped nothing (every entry used lately)
// would otherwise run again at the next lookup, and the one after -- every entity drawn, every frame: O(n^2) a frame.
// So a scan notes the oldest entry it keeps, and the next one waits until that entry can go: at most one scan a frame,
// and none while nothing is old enough.
struct Eviction
{
    int next{0}; // the first host frame a scan may find something to drop
};

// Drops the entries of `cache` (a map of values with a `frame`) not used in the last `maxAge` frames, if it holds more
// than `limit` and something can be old enough (see Eviction). `now`: this host frame.
template <typename Map>
void evictStale(Map& cache, za::SizeT limit, int maxAge, int now, Eviction& eviction)
{
    if(cache.size() <= limit || now < eviction.next)
    {
        return;
    }
    int oldest = now;
    erase_if(cache, [&](const auto& entry) { // (found by ADL: std::erase_if of the map's own header)
        if(entry.second.frame < now - maxAge)
        {
            return true;
        }
        oldest = za::min(oldest, entry.second.frame);
        return false;
    });
    eviction.next = za::max(oldest + maxAge + 1, now + 1);
}

} // namespace qvr
