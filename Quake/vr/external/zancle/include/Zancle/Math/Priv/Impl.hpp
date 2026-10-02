#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Internal helpers for the `za::<name>` math wrappers (e.g. `za::sqrt`)
///
/// Provides `ZA_PRIV_HAS_MATH_BUILTIN(name)`, which checks
/// whether all three of `__builtin_<name>`, `__builtin_<name>f`, and
/// `__builtin_<name>l` are available. Used by per-function math
/// headers to decide whether to define `ZA_MATH_<NAME>(F|L)`
/// macros that point at the compiler builtins or at `::std::<name>`
/// from `<cmath>`.
///
/// Also provides `ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(name, NAME)`
/// and its 2-arg counterpart, which generate the type-dispatching
/// `za::<name>` function template that calls the appropriate
/// `ZA_MATH_<NAME>(F|L)` macro for `float`, `double`, or
/// `long double`. Caller must define those macros before invoking.
///
/// The wrappers take exactly `float`, `double`, or `long double` (both
/// arguments of the same type, for the 2-arg ones), and return that
/// type. Unlike `<cmath>`, integers are not converted to `double`, nor
/// are mixed `float`/`double` arguments promoted: deliberately, so that
/// every conversion (and its precision or performance cost) is visible
/// at the call site, e.g. `za::sqrt(static_cast<float>(n))`.
///
/// \warning The generated wrappers are `constexpr`, but whether they can
///          actually be evaluated at compile time depends on the compiler's
///          constant folding of the underlying builtin: GCC can evaluate all
///          of them except `rint` (which depends on the run-time rounding
///          mode), whereas Clang can only evaluate `fabs`, `fmax`, and `fmin`.
///          Code relying on a compile-time evaluation of any other wrapper
///          compiles with GCC but not with Clang.
///
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"

#include "Zancle/Trait/IsSame.hpp"


////////////////////////////////////////////////////////////
// Checks the `__builtin_`-prefixed names (the ones actually used), not the plain library names:
// the latter are not builtins under `-fno-builtin`/`-ffreestanding`, while the former still are.
#define ZA_PRIV_HAS_MATH_BUILTIN(name) \
    (ZA_HAS_BUILTIN(__builtin_##name) && ZA_HAS_BUILTIN(__builtin_##name##f) && ZA_HAS_BUILTIN(__builtin_##name##l))


////////////////////////////////////////////////////////////
#define ZA_PRIV_DEFINE_MATH_WRAPPER_1ARG(name, NAME)                                                             \
    namespace za                                                                                                 \
    {                                                                                                            \
                                                                                                                 \
    template <typename T>                                                                                        \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] inline constexpr auto name(const T arg) noexcept \
    {                                                                                                            \
        if constexpr (ZA_IS_SAME(T, float))                                                                      \
            return ZA_MATH_##NAME##F(arg);                                                                       \
        else if constexpr (ZA_IS_SAME(T, double))                                                                \
            return ZA_MATH_##NAME(arg);                                                                          \
        else if constexpr (ZA_IS_SAME(T, long double))                                                           \
            return ZA_MATH_##NAME##L(arg);                                                                       \
        else                                                                                                     \
            static_assert(false, "`za::" #name "` takes exactly `float`, `double`, or `long double`");           \
    }                                                                                                            \
                                                                                                                 \
    } // namespace za

////////////////////////////////////////////////////////////
#define ZA_PRIV_DEFINE_MATH_WRAPPER_2ARG(name, NAME)                                                                            \
    namespace za                                                                                                                \
    {                                                                                                                           \
                                                                                                                                \
    template <typename T>                                                                                                       \
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] inline constexpr auto name(const T arg0, const T arg1) noexcept \
    {                                                                                                                           \
        if constexpr (ZA_IS_SAME(T, float))                                                                                     \
            return ZA_MATH_##NAME##F(arg0, arg1);                                                                               \
        else if constexpr (ZA_IS_SAME(T, double))                                                                               \
            return ZA_MATH_##NAME(arg0, arg1);                                                                                  \
        else if constexpr (ZA_IS_SAME(T, long double))                                                                          \
            return ZA_MATH_##NAME##L(arg0, arg1);                                                                               \
        else                                                                                                                    \
            static_assert(false, "`za::" #name "` takes two `float`, `double`, or `long double` of the same type");             \
    }                                                                                                                           \
                                                                                                                                \
    } // namespace za
