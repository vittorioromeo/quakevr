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
    // A buffer large enough for 64-bit integers and floating point numbers.
    // long long: -9,223,372,036,854,775,808 (19 digits + sign)
    // double: sign + integer part + '.' + fractional part (e.g., 9 digits) + null

    char buffer[64];

    const char* const end = toChars(buffer, buffer + 64, value);
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
    char buffer[64];

    const char* const end = toChars(buffer, buffer + 64, value);
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
