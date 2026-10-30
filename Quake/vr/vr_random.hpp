#pragma once

// vr_random.hpp -- the one spelling of "a float in [lo, hi]" and of the small seeded streams the effects files keep.
// Each file keeps its own stream and seed (tests replay them): these helpers take the stream explicitly, and a
// sequence is exactly what the file's own copy gave.

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::vrand
{

// lo + (hi - lo) * a uniform [0, 1] draw from a za::FastNonCryptoRng. Unlike rng.getF(lo, hi) it allows lo > hi.
template <typename Rng> [[nodiscard]] inline float range(Rng& rng, const float lo, const float hi)
{
    return lo + (hi - lo) * rng.getF(0.f, 1.f);
}

// xorshift32: the next state (the stream is the state itself).
[[nodiscard]] inline za::U32 xorshift32(za::U32& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// xorshift32 as a [0, 1) float from the low 24 bits (wounds, rig2 and the hull diagnostic's streams).
[[nodiscard]] inline float xorshift01(za::U32& s)
{
    return static_cast<float>(xorshift32(s) & 0xffffffu) / 16777216.f;
}

// The LCG (1664525, 1013904223) as a [0, 1) float from the top 24 bits (bttrails, weaponfx, dents).
[[nodiscard]] inline float lcg01(za::U32& s)
{
    s = s * 1664525u + 1013904223u;
    return static_cast<float>(s >> 8) * (1.f / 16777216.f);
}

// lo + (hi - lo) * a draw of the stream above (lo > hi allowed).
[[nodiscard]] inline float xorshiftRange(za::U32& s, const float lo, const float hi)
{
    return lo + (hi - lo) * xorshift01(s);
}
[[nodiscard]] inline float lcgRange(za::U32& s, const float lo, const float hi)
{
    return lo + (hi - lo) * lcg01(s);
}

} // namespace qvr::vrand
