#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToChars.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsIntegral.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Buffer size for `toChars` output of any `T` value (at the default precision)
///
/// 64-bit integers take at most 20 characters (19 digits and a sign).
/// Floating-point values take a sign, up to 309 integer digits (`DBL_MAX`),
/// the decimal point, and the fractional digits.
///
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr SizeT toStringBufferSize = isFloatingPoint<T> ? 1u + 309u + 1u + 10u : 32u;

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Convert an integral or floating-point value to a string
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] String toString(const T value)
    requires(isIntegral<T> || isFloatingPoint<T>)
{
    char buffer[priv::toStringBufferSize<T>];

    const char* const end = toChars(buffer, buffer + sizeof(buffer), value);
    ZA_ASSERT(end != nullptr);

    return String{buffer, static_cast<SizeT>(end - buffer)};
}


////////////////////////////////////////////////////////////
/// \brief Convert a `bool` to a string ("true" / "false").
///
////////////////////////////////////////////////////////////
[[nodiscard]] inline String toString(const bool value)
{
    return value ? String{"true"} : String{"false"};
}


////////////////////////////////////////////////////////////
/// \brief Append a numeric value's string representation to `str` without intermediate allocations
///
////////////////////////////////////////////////////////////
template <typename T>
void appendToString(String& str, const T value)
    requires(isIntegral<T> || isFloatingPoint<T>)
{
    char buffer[priv::toStringBufferSize<T>];

    const char* const end = toChars(buffer, buffer + sizeof(buffer), value);
    ZA_ASSERT(end != nullptr);

    str.append(buffer, static_cast<SizeT>(end - buffer));
}


////////////////////////////////////////////////////////////
/// \brief Append a `bool` to `str` as "true" / "false".
///
////////////////////////////////////////////////////////////
inline void appendToString(String& str, const bool value)
{
    if (value)
        str.append("true", 4u);
    else
        str.append("false", 5u);
}

} // namespace za
