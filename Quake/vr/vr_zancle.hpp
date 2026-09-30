#pragma once

// vr_zancle.hpp -- what the Quake VR code needs that Zancle (external/zancle, vendored) does not have yet: small
// stand-ins in namespace qza, written the way Zancle writes its own (compiler builtins behind macros, always-inline
// templates that take exactly float, double or long double). Each is a proposal for Zancle (ZANCLE-TODO; the list in
// docs/vr-port/ROUND21.md, "Zancle migration"): when Zancle has it, the call sites move to it and this goes.

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Priv/Impl.hpp"
#include "Zancle/Trait/IsFloatingPoint.hpp"

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
