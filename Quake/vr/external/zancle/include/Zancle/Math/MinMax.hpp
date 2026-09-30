#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/LifetimeAttributes.hpp"

#include "Zancle/Trait/IsClass.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Return the smaller of two values
///
/// Equivalent to `std::min` but does not include `<algorithm>`.
/// Non-class types (arithmetic, enums, pointers) are taken and returned
/// by value, which avoids stack spills in unoptimized builds. Class types
/// are taken and returned by `const` reference, like `std::min`.
///
/// If the values are equivalent, returns `a`.
///
////////////////////////////////////////////////////////////
template <typename T>
    requires(!za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::const]] constexpr T min(const T a, const T b) noexcept
{
    return b < a ? b : a;
}


////////////////////////////////////////////////////////////
template <typename T>
    requires(za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& min(const T& a ZA_LIFETIMEBOUND, const T& b ZA_LIFETIMEBOUND) noexcept
{
    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return b < a ? b : a;
}


////////////////////////////////////////////////////////////
/// \brief Return the larger of two values
///
/// Equivalent to `std::max` but does not include `<algorithm>`.
/// Non-class types (arithmetic, enums, pointers) are taken and returned
/// by value, which avoids stack spills in unoptimized builds. Class types
/// are taken and returned by `const` reference, like `std::max`.
///
/// If the values are equivalent, returns `a`.
///
////////////////////////////////////////////////////////////
template <typename T>
    requires(!za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::const]] constexpr T max(const T a, const T b) noexcept
{
    return a < b ? b : a;
}


////////////////////////////////////////////////////////////
template <typename T>
    requires(za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& max(const T& a ZA_LIFETIMEBOUND, const T& b ZA_LIFETIMEBOUND) noexcept
{
    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return a < b ? b : a;
}

} // namespace za
