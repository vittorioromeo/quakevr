#pragma once

#include "debugapi.h"
#include "q_stdinc.hpp"

#include <glm.hpp>

#include <array>
#include <string>
#include <cassert>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <sstream>

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

    template <typename... Ts>
    void debugPrint(const Ts&... xs)
    {
        OutputDebugStringA(stringCat(xs...).data());
    }

    template <typename... Ts>
    void debugPrintSeparated(const std::string_view separator, const Ts&... xs)
    {
        OutputDebugStringA(stringCatSeparated(separator, xs...).data());
    }

    [[nodiscard]] constexpr glm::vec3 toVec3(const vec3_t& v) noexcept
    {
        return {v[0], v[1], v[2]};
    }

    constexpr void toQuakeVec3(vec3_t out, const glm::vec3& v) noexcept
    {
        out[0] = v[0];
        out[1] = v[1];
        out[2] = v[2];
    }

    template <typename... Ts>
    [[nodiscard]] constexpr auto makeAdjustedMenuLabels(const Ts&... labels)
    {
        constexpr auto maxLen = 25;

        assert(((strlen(labels) <= maxLen) && ...));
        return std::array{(std::string(maxLen - strlen(labels), ' ') + labels)...};
    }

    [[nodiscard]] constexpr bool boxIntersection(const vec3_t& aMin,
        const vec3_t& aMax, const vec3_t& bMin, const vec3_t& bMax)
    {
        return aMin[0] <= bMax[0] && //
               aMin[1] <= bMax[1] && //
               aMin[2] <= bMax[2] && //
               aMax[0] >= bMin[0] && //
               aMax[1] >= bMin[1] && //
               aMax[2] >= bMin[2];
    }
} // namespace quake::util

namespace std
{
    template <int D, typename T, glm::qualifier P>
    struct tuple_size<glm::vec<D, T, P>>
        : std::integral_constant<std::size_t, D>
    {
    };

    template <std::size_t I, int D, typename T, glm::qualifier P>
    struct tuple_element<I, glm::vec<D, T, P>>
    {
        using type = T;
    };
} // namespace std

namespace glm
{
    template <std::size_t I, int D, typename T, glm::qualifier P>
    [[nodiscard]] T get(const glm::vec<D, T, P>& v) noexcept
    {
        return v[I];
    }
} // namespace glm
