#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"

#include "Zancle/Trait/IsClass.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Clamp `value` into the closed range `[minValue, maxValue]`
///
/// Like `std::clamp`, only requires `T` to provide `operator<`.
///
/// Non-class types (arithmetic, enums, pointers) are taken and returned
/// by value, which avoids stack spills in unoptimized builds. Class types
/// are taken and returned by `const` reference, like `std::clamp`.
///
/// \return `value` if in range, otherwise the closer bound
///
////////////////////////////////////////////////////////////
template <typename T>
    requires(!za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::const]] constexpr T clamp(const T value, const T minValue, const T maxValue) noexcept
{
    ZA_ASSERT(!(maxValue < minValue));

    return (value < minValue) ? minValue : ((maxValue < value) ? maxValue : value);
}


////////////////////////////////////////////////////////////
template <typename T>
    requires(za::isClass<T>)
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& clamp(const T& value    ZA_LIFETIMEBOUND,
                                                                      const T& minValue ZA_LIFETIMEBOUND,
                                                                      const T& maxValue ZA_LIFETIMEBOUND) noexcept
{
    ZA_ASSERT(!(maxValue < minValue));

    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return (value < minValue) ? minValue : ((maxValue < value) ? maxValue : value);
}

} // namespace za
