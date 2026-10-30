#pragma once

// vr_stats.hpp -- quantiles of a series. The diagnostics each rounded the rank their own way and their printed tables
// are compared across runs, so the rank rule is a parameter: a site names the rule it always had.

#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Base/SizeT.hpp"

namespace qvr::vrstats
{

enum class Rank
{
    Floor,   // floor(q * (n - 1))        (agglitch's tables)
    Nearest, // floor(q * (n - 1) + 0.5)  (the nearest rank: audio bench, self-collision fit)
    Count,   // floor(q * n), at most n-1 (Box3D step times)
};

// The index of the q quantile (0..1) among n > 0 sorted values. T is the arithmetic type the site rounded in
// (float or double: the same rule gives different borderline picks in the other).
template <typename T> [[nodiscard]] inline za::SizeT rankIndex(const za::SizeT n, const T q, const Rank rule)
{
    switch(rule)
    {
        case Rank::Floor: return static_cast<za::SizeT>(q * static_cast<T>(n - 1));
        case Rank::Nearest: return za::min(n - 1, static_cast<za::SizeT>(q * static_cast<T>(n - 1) + static_cast<T>(0.5)));
        case Rank::Count: return za::min(static_cast<za::SizeT>(q * static_cast<T>(n)), n - 1);
    }
    return 0;
}

// The q quantile of n values sorted ascending (0 for none).
template <typename T> [[nodiscard]] inline float quantileSorted(const float* sorted, const za::SizeT n, const T q, const Rank rule)
{
    return n == 0 ? 0.f : sorted[rankIndex<T>(n, q, rule)];
}

} // namespace qvr::vrstats
