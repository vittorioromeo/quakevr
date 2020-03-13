#pragma once

#include "debugapi.h"
#include "q_stdinc.hpp"

#define GLM_FORCE_INLINE
#include <glm.hpp>

#include <array>
#include <string>
#include <cassert>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <sstream>

#if !defined(QUAKE_FORCEINLINE)
#if defined(_MSC_VER)
#define QUAKE_FORCEINLINE __forceinline
#elif defined(__GNUC__) && __GNUC__ > 3
// Clang also defines __GNUC__ (as 4)
#define QUAKE_FORCEINLINE inline __attribute__((__always_inline__))
#else
#define QUAKE_FORCEINLINE inline
#endif
#endif

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

    [[nodiscard]] QUAKE_FORCEINLINE constexpr glm::vec3 toVec3(
        const vec3_t& v) noexcept
    {
        return {v[0], v[1], v[2]};
    }

    QUAKE_FORCEINLINE constexpr void toQuakeVec3(
        vec3_t out, const glm::vec3& v) noexcept
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
        return std::array{
            (std::string(maxLen - strlen(labels), ' ') + labels)...};
    }

    template <typename TVec3AMin, typename TVec3AMax, typename TVec3BMin,
        typename TVec3BMax>
    [[nodiscard]] constexpr bool boxIntersection(const TVec3AMin& aMin,
        const TVec3AMax& aMax, const TVec3BMin& bMin, const TVec3BMax& bMax)
    {
        return aMin[0] <= bMax[0] && //
               aMin[1] <= bMax[1] && //
               aMin[2] <= bMax[2] && //
               aMax[0] >= bMin[0] && //
               aMax[1] >= bMin[1] && //
               aMax[2] >= bMin[2];
    }

    [[nodiscard]] inline double lerp(double a, double b, double f) noexcept
    {
        return (a * (1.0 - f)) + (b * f);
    }

    inline void vec3lerp(vec3_t out, vec3_t start, vec3_t end, double f)
    {
        out[0] = lerp(start[0], end[0], f);
        out[1] = lerp(start[1], end[1], f);
        out[2] = lerp(start[2], end[2], f);
    }

    [[nodiscard]] inline glm::vec3 pitchYawRollFromDirectionVector(
        const glm::vec3& dir)
    {
        // From: https://stackoverflow.com/a/21627251/598696

        const auto pitch = asin(dir[2]);
        const auto yaw = atan2(dir[1], dir[0]);

        const auto up = glm::vec3{0, 0, 1};
        const auto w0 = glm::vec3{-dir[1], dir[0], 0};
        const auto u0 = glm::cross(w0, dir);

        // TODO VR: verify correctness of roll
        const auto roll = atan2(glm::dot(w0, up) / glm::length(w0),
            glm::dot(u0, up) / glm::length(u0));

        return glm::degrees(glm::vec3{pitch, yaw, roll});
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
    [[nodiscard]] QUAKE_FORCEINLINE T get(const glm::vec<D, T, P>& v) noexcept
    {
        return v[I];
    }

    // TODO VR:
    [[nodiscard]] QUAKE_FORCEINLINE constexpr glm::vec3 operator+(
        const glm::vec3& lhs, const vec3_t& rhs) noexcept
    {
        return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
    }

    [[nodiscard]] QUAKE_FORCEINLINE constexpr glm::vec3 operator+(
        const vec3_t& lhs, const glm::vec3& rhs) noexcept
    {
        return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
    }

    [[nodiscard]] QUAKE_FORCEINLINE constexpr glm::vec3 operator-(
        const glm::vec3& lhs, const vec3_t& rhs) noexcept
    {
        return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
    }

    [[nodiscard]] QUAKE_FORCEINLINE constexpr glm::vec3 operator-(
        const vec3_t& lhs, const glm::vec3& rhs) noexcept
    {
        return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
    }
} // namespace glm
