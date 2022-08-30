#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace quake::util
{

template <typename... Ts>
[[nodiscard]] std::string stringCat(const Ts&... xs)
{
    std::ostringstream oss;
    (oss << ... << xs);
    return oss.str();
}

template <typename T, typename... Ts>
[[nodiscard]] std::string stringCatSeparated(
    const std::string_view separator, const T& x, const Ts&... xs)
{
    std::ostringstream oss;
    oss << x;
    ((oss << separator << xs), ...);
    return oss.str();
}


} // namespace quake::util
