#pragma once

// vr_zancle.hpp -- what the Quake VR code needs that Zancle (external/zancle, vendored) does not have yet: small
// stand-ins in namespace qza, written the way Zancle writes its own (compiler builtins behind macros, always-inline
// templates that take exactly float, double or long double). Each is a proposal for Zancle (ZANCLE-TODO; the list in
// docs/vr-port/ROUND21.md, "Zancle migration"): when Zancle has it, the call sites move to it and this goes.

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

namespace qza
{

// ZANCLE-TODO: no quiet NaN constant (std::nanf(""), std::numeric_limits<float>::quiet_NaN()).
inline constexpr float nanF = __builtin_nanf("");

// ZANCLE-TODO: za::remainder is not std::remainder: it truncates the quotient (fmod's result, through an int: -360..360
// for a wrap by 360, and wrong past INT_MAX quotients), where std::remainder rounds it to the nearest (IEEE remainder:
// -180..180, the angle wraps here). The IEEE one, on the builtin (vr_zancle.cpp's vr_zancle_math_test checks it).
template <typename T>
[[nodiscard, gnu::always_inline, gnu::const]] inline auto remainder(const T a, const T b) noexcept
{
    if constexpr (ZA_IS_SAME(T, float))
        return __builtin_remainderf(a, b);
    else if constexpr (ZA_IS_SAME(T, double))
        return __builtin_remainder(a, b);
    else if constexpr (ZA_IS_SAME(T, long double))
        return __builtin_remainderl(a, b);
    else
        static_assert(false);
}

// ZANCLE-TODO: no `abs` (std::abs's overloads: the integers' as well as fabs): the same result types as std::abs (a
// small integer promoted to int first).
template <typename T>
[[nodiscard, gnu::always_inline, gnu::const]] inline constexpr auto abs(const T x) noexcept
{
    if constexpr (ZA_IS_FLOATING_POINT(T))
        return za::fabs(x);
    else
    {
        const auto v = +x;
        return v < 0 ? -v : v;
    }
}

// ZANCLE-TODO: za::Clock and za::Time count microseconds: the profiler and the benchmarks time things of well under
// one. A steady clock in nanoseconds (vr_zancle.cpp: std::chrono::steady_clock, kept out of the headers), and the time
// since a reading in seconds, milliseconds, microseconds or nanoseconds (as std::chrono::duration<double, ...> gives it).
[[nodiscard]] za::I64 nowNs() noexcept;

// vr_zancle_math_test (vr_zancle.cpp): Zancle's math against the standard library's, on edge values.
void mathTest_f();

[[nodiscard]] inline double nsSince(const za::I64 startNs) noexcept
{
    return static_cast<double>(nowNs() - startNs);
}

[[nodiscard]] inline double usSince(const za::I64 startNs) noexcept
{
    return static_cast<double>(nowNs() - startNs) / 1e3;
}

[[nodiscard]] inline double msSince(const za::I64 startNs) noexcept
{
    return static_cast<double>(nowNs() - startNs) / 1e6;
}

[[nodiscard]] inline double secondsSince(const za::I64 startNs) noexcept
{
    return static_cast<double>(nowNs() - startNs) / 1e9;
}

// ZANCLE-TODO: MinMax takes two values: std::min and std::max over an initializer list (the first smallest, the first
// largest, as those return: a fold of za::min / za::max from the left).
template <typename T>
[[nodiscard, gnu::always_inline]] constexpr T minOf(const T a) noexcept
{
    return a;
}

template <typename T, typename... Ts>
[[nodiscard, gnu::always_inline]] constexpr T minOf(const T a, const T b, const Ts... rest) noexcept
{
    return minOf(za::min(a, b), rest...);
}

template <typename T>
[[nodiscard, gnu::always_inline]] constexpr T maxOf(const T a) noexcept
{
    return a;
}

template <typename T, typename... Ts>
[[nodiscard, gnu::always_inline]] constexpr T maxOf(const T a, const T b, const Ts... rest) noexcept
{
    return maxOf(za::max(a, b), rest...);
}

// ZANCLE-TODO: String has no (count, char) constructor (std::string's): `count` copies of `c`.
[[nodiscard]] inline za::String repeated(const za::SizeT count, const char c)
{
    za::String s;
    s.resize(count, c);
    return s;
}

// ZANCLE-TODO: Array (and Vector) have no ordering operators (std::array's <): a before b, element by element.
template <typename A, typename B>
[[nodiscard]] constexpr bool lexicographicLess(const A& a, const B& b)
{
    auto ia = a.begin();
    auto ib = b.begin();
    for(; ia != a.end() && ib != b.end(); ++ia, ++ib)
    {
        if(*ia < *ib)
        {
            return true;
        }
        if(*ib < *ia)
        {
            return false;
        }
    }
    return ia == a.end() && ib != b.end();
}

// ZANCLE-TODO: Span has no size_bytes() (std::span's): its elements' bytes.
template <typename Span>
[[nodiscard, gnu::always_inline]] constexpr auto sizeBytes(const Span& s) noexcept
{
    return s.size() * sizeof(*s.data());
}

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

// ZANCLE-TODO: no pair (std::pair): an aggregate of two, compared member by member (==, <, as std::pair's). Where the
// two have names, a struct of its own is clearer: this is for (key, index) lists sorted and searched as pairs.
template <typename A, typename B>
struct Pair
{
    A first;
    B second;

    [[nodiscard]] constexpr bool operator==(const Pair&) const = default;
    [[nodiscard]] constexpr bool operator<(const Pair& o) const { return first < o.first || (!(o.first < first) && second < o.second); }
    [[nodiscard]] constexpr bool operator>(const Pair& o) const { return o < *this; }
    [[nodiscard]] constexpr bool operator<=(const Pair& o) const { return !(o < *this); }
    [[nodiscard]] constexpr bool operator>=(const Pair& o) const { return !(*this < o); }
};

template <typename A, typename B>
Pair(A, B) -> Pair<A, B>;

template <typename A, typename B>
[[nodiscard]] constexpr Pair<A, B> makePair(A a, B b)
{
    return Pair<A, B>{static_cast<A&&>(a), static_cast<B&&>(b)};
}

// ZANCLE-TODO: Zancle's containers have no reverse iterators (rbegin, rend): over a contiguous container from its
// last element to its first, `it` then `++it` as std::reverse_iterator's.
template <typename P>
struct ReverseIterator
{
    P p; // one past the element

    [[nodiscard]] constexpr decltype(auto) operator*() const noexcept { return *(p - 1); }
    [[nodiscard]] constexpr P operator->() const noexcept { return p - 1; }
    constexpr ReverseIterator& operator++() noexcept
    {
        --p;
        return *this;
    }
    [[nodiscard]] constexpr bool operator==(const ReverseIterator&) const = default;
};

template <typename Container>
[[nodiscard]] constexpr auto rbegin(Container& c) noexcept
{
    return ReverseIterator<decltype(c.data())>{c.data() + c.size()};
}

template <typename Container>
[[nodiscard]] constexpr auto rend(Container& c) noexcept
{
    return ReverseIterator<decltype(c.data())>{c.data()};
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
