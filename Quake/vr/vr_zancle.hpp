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
#include "Zancle/Math/Priv/Impl.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

// ZANCLE-TODO: Math lacks hypot, cbrt, log2, exp2, llround, copysign and trunc (the same ZA_MATH_* macro and wrapper
// pattern as its sin or fmax; the builtins below exist on GCC and Clang).
#define QZA_PRIV_MATH_1ARG(name)                                                                                    \
    template <typename T>                                                                                           \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] inline constexpr auto name(const T arg) noexcept    \
    {                                                                                                               \
        if constexpr (ZA_IS_SAME(T, float))                                                                         \
            return __builtin_##name##f(arg);                                                                        \
        else if constexpr (ZA_IS_SAME(T, double))                                                                   \
            return __builtin_##name(arg);                                                                           \
        else if constexpr (ZA_IS_SAME(T, long double))                                                              \
            return __builtin_##name##l(arg);                                                                        \
        else                                                                                                        \
            static_assert(false);                                                                                   \
    }

#define QZA_PRIV_MATH_2ARG(name)                                                                                    \
    template <typename T>                                                                                           \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] inline constexpr auto name(const T a, const T b) noexcept \
    {                                                                                                               \
        if constexpr (ZA_IS_SAME(T, float))                                                                         \
            return __builtin_##name##f(a, b);                                                                       \
        else if constexpr (ZA_IS_SAME(T, double))                                                                   \
            return __builtin_##name(a, b);                                                                          \
        else if constexpr (ZA_IS_SAME(T, long double))                                                              \
            return __builtin_##name##l(a, b);                                                                       \
        else                                                                                                        \
            static_assert(false);                                                                                   \
    }

namespace qza
{

QZA_PRIV_MATH_1ARG(cbrt)
QZA_PRIV_MATH_1ARG(log2)
QZA_PRIV_MATH_1ARG(exp2)
QZA_PRIV_MATH_1ARG(llround)
QZA_PRIV_MATH_1ARG(trunc)
QZA_PRIV_MATH_2ARG(hypot)
QZA_PRIV_MATH_2ARG(copysign)

// ZANCLE-TODO: no quiet NaN constant (std::nanf(""), std::numeric_limits<float>::quiet_NaN()).
inline constexpr float nanF = __builtin_nanf("");

// ZANCLE-TODO: IsNan.hpp / IsInf.hpp have no IsFinite (`__builtin_isfinite`).
template <typename T>
[[nodiscard, gnu::always_inline, gnu::const]] inline constexpr bool isfinite(const T x) noexcept
{
    static_assert(ZA_IS_FLOATING_POINT(T));
    return __builtin_isfinite(x);
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

// ZANCLE-TODO: Algorithm has no stable partition (std::stable_partition): the elements for which `pred` holds first,
// each group in its order (one result: the same as std's); the rest moved through a buffer. Returns the first of the
// rest.
template <typename T, typename Pred>
T* stablePartition(T* const first, T* const last, Pred&& pred)
{
    za::Vector<T> rest;
    T* out = first;
    for(T* it = first; it != last; ++it)
    {
        if(pred(*it))
        {
            if(out != it)
            {
                *out = static_cast<T&&>(*it);
            }
            ++out;
        }
        else
        {
            rest.pushBack(static_cast<T&&>(*it));
        }
    }
    T* const split = out;
    for(T& e : rest)
    {
        *out++ = static_cast<T&&>(e);
    }
    return split;
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

// ZANCLE-TODO: no binary search (std::lower_bound): the first element of the sorted [first, last) that is not less
// than `value` (less(element, value) false).
template <typename It, typename T, typename Less>
[[nodiscard]] constexpr It lowerBound(It first, const It last, const T& value, Less&& less)
{
    auto count = last - first;
    while(count > 0)
    {
        const auto half = count / 2;
        const It mid = first + half;
        if(less(*mid, value))
        {
            first = mid + 1;
            count -= half + 1;
        }
        else
        {
            count = half;
        }
    }
    return first;
}

template <typename It, typename T>
[[nodiscard]] constexpr It lowerBound(const It first, const It last, const T& value)
{
    return lowerBound(first, last, value, [](const auto& a, const auto& b) { return a < b; });
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

// ZANCLE-TODO: Algorithm has no fill (std::fill): every element of [first, last), or of a range (an array, a
// container), set to `value`.
template <typename It, typename T>
constexpr void fill(It first, const It last, const T& value)
{
    for(; first != last; ++first)
    {
        *first = value;
    }
}

template <typename Range, typename T>
constexpr void fill(Range& range, const T& value)
{
    for(auto& e : range)
    {
        e = value;
    }
}

// ZANCLE-TODO: Algorithm has no iota (std::iota): value, value + 1, ... into [first, last).
template <typename It, typename T>
constexpr void iota(It first, const It last, T value)
{
    for(; first != last; ++first, ++value)
    {
        *first = value;
    }
}

// ZANCLE-TODO: Algorithm has no replace (std::replace): every element equal to `from` set to `to`.
template <typename It, typename T>
constexpr void replace(It first, const It last, const T& from, const T& to)
{
    for(; first != last; ++first)
    {
        if(*first == from)
        {
            *first = to;
        }
    }
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

#undef QZA_PRIV_MATH_1ARG
#undef QZA_PRIV_MATH_2ARG
