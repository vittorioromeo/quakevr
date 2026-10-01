#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/Fabs.hpp"

#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsIntegral.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsUnsigned.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Absolute value of a signed integer or a floating-point number
///
/// Equivalent to `std::abs`, with the same result types: floating-point
/// numbers keep their type (via `za::fabs`), and signed integers smaller
/// than `int` are promoted to `int` first. Unsigned integers (whose
/// absolute value is themselves) and `bool` are rejected, as by `std::abs`.
///
/// As for `std::abs`, the absolute value of the most negative integer
/// (e.g. `INT_MIN`) is not representable: undefined behavior.
///
////////////////////////////////////////////////////////////
template <typename T>
    requires(ZA_IS_FLOATING_POINT(T) || (ZA_IS_INTEGRAL(T) && !ZA_IS_UNSIGNED(T) && !ZA_IS_SAME(T, bool)))
[[nodiscard, gnu::always_inline, gnu::const]] constexpr auto abs(const T x) noexcept
{
    if constexpr (ZA_IS_FLOATING_POINT(T))
    {
        return za::fabs(x);
    }
    else
    {
        const auto promoted = +x; // like `std::abs`: `int` for smaller integers
        return promoted < 0 ? -promoted : promoted;
    }
}

} // namespace za
