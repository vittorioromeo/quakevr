#pragma once

#ifdef WIN32
#include "debugapi.h"
#endif

#include "q_stdinc.hpp"
#include "cvar.hpp"
#include "mathlib.hpp"
#include "quakeglm.hpp"

#include <array>
#include <algorithm>
#include <string>
#include <cassert>
#include <cmath>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <sstream>
#include <variant>

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

// TODO VR: (P2) ugly declaration
float VR_GetMenuMult() noexcept;

namespace quake::util
{
    template <typename T>
    [[nodiscard]] constexpr T mapRange(const T input, const T inputMin,
        const T inputMax, const T outputMin, const T outputMax) noexcept
    {
        const T slope =
            T(1.0) * (outputMax - outputMin) / (inputMax - inputMin);

        return outputMin + slope * (input - inputMin);
    }

    template <typename T>
    [[nodiscard]] T cvarToEnum(const cvar_t& x) noexcept
    {
        return static_cast<T>(static_cast<int>(x.value));
    }

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
    void debugPrint([[maybe_unused]] const Ts&... xs)
    {
#ifdef WIN32
        OutputDebugStringA(stringCat(xs...).data());
#endif
    }

    template <typename... Ts>
    void debugPrintSeparated([[maybe_unused]] const std::string_view separator,
        [[maybe_unused]] const Ts&... xs)
    {
#ifdef WIN32
        OutputDebugStringA(stringCatSeparated(separator, xs...).data());
#endif
    }

    template <typename TVec3AMin, typename TVec3AMax, typename TVec3BMin,
        typename TVec3BMax>
    [[nodiscard]] constexpr QUAKE_FORCEINLINE bool boxIntersection(
        const TVec3AMin& aMin, const TVec3AMax& aMax, const TVec3BMin& bMin,
        const TVec3BMax& bMax) noexcept
    {
        return aMin[0] <= bMax[0] && //
               aMin[1] <= bMax[1] && //
               aMin[2] <= bMax[2] && //
               aMax[0] >= bMin[0] && //
               aMax[1] >= bMin[1] && //
               aMax[2] >= bMin[2];
    }

    template <typename TEntA, typename TEntB>
    [[nodiscard]] constexpr QUAKE_FORCEINLINE bool entBoxIntersection(
        const TEntA& entA, const TEntB& entB) noexcept
    {
        return boxIntersection(
            entA->v.absmin, entA->v.absmax, entB->v.absmin, entB->v.absmax);
    }

    [[nodiscard]] QUAKE_FORCEINLINE double lerp(
        double a, double b, double f) noexcept
    {
        return (a * (1.0 - f)) + (b * f);
    }

    template <typename T>
    [[nodiscard]] constexpr QUAKE_FORCEINLINE auto safeAsin(const T x) noexcept
    {
        assert(!std::isnan(x));
        assert(x >= T(-1));
        assert(x <= T(1));

        return std::asin(x);
    }

    template <typename T>
    [[nodiscard]] constexpr QUAKE_FORCEINLINE auto safeAtan2(
        const T y, const T x) noexcept
    {
        assert(!std::isnan(y));
        assert(!std::isnan(x));
        assert(y != T(0) || x != T(0));

        return std::atan2(y, x);
    }

    [[nodiscard]] inline glm::vec3 pitchYawRollFromDirectionVector(
        const glm::vec3& up, const glm::vec3& dir)
    {
        // From: https://stackoverflow.com/a/21627251/598696

        const auto pitch = safeAsin(dir[2]);
        const auto yaw = safeAtan2(dir[1], dir[0]);

        const auto w0 = glm::vec3{-dir[1], dir[0], 0};
        const auto u0 = glm::cross(w0, dir);

        const auto w0len = glm::length(w0);
        assert(w0len != 0);

        const auto u0len = glm::length(u0);
        assert(u0len != 0);

        const auto roll =
            safeAtan2(glm::dot(w0, up) / w0len, glm::dot(u0, up) / u0len);

        auto res = glm::degrees(glm::vec3{pitch, yaw, roll});
        res[0 /* PITCH */] *= -1.f;
        res[2 /* ROLL */] -= 180.f;
        return res;
    }

    [[nodiscard]] QUAKE_FORCEINLINE std::tuple<glm::vec3, glm::vec3, glm::vec3>
    getAngledVectors(const glm::vec3& v) noexcept
    {
        glm::vec3 forward, right, up;
        AngleVectors(v, forward, right, up);
        return std::tuple{forward, right, up};
    }

    [[nodiscard]] QUAKE_FORCEINLINE glm::vec3
    getDirectionVectorFromPitchYawRoll(const glm::vec3& v) noexcept
    {
        return AngleVectorsOnlyFwd(v);
    }

    // TODO VR: (P2) same as above, use one or the other
    [[nodiscard]] QUAKE_FORCEINLINE glm::vec3 getFwdVecFromPitchYawRoll(
        const glm::vec3& v) noexcept
    {
        return AngleVectorsOnlyFwd(v);
    }

    template <typename T>
    [[nodiscard]] auto makeMenuAdjuster(const bool isLeft)
    {
        return [isLeft](
                   const cvar_t& cvar, const T incr, const T min, const T max) {
            const float factor = VR_GetMenuMult() >= 3 ? 6.f : VR_GetMenuMult();

            const T adjIncr = incr * static_cast<T>(factor);

            const auto newVal = static_cast<T>(
                isLeft ? cvar.value - adjIncr : cvar.value + adjIncr);
            const auto res = static_cast<T>(std::clamp(newVal, min, max));

            Cvar_SetValue(cvar.name, res);
        };
    }

    template <typename T>
    [[nodiscard]] QUAKE_FORCEINLINE bool hitSomething(const T& trace) noexcept
    {
        return trace.fraction < 1.f;
    }

    template <typename... Fs>
    struct overload_set : Fs...
    {
        template <typename... FFwds>
        constexpr overload_set(FFwds&&... fFwds)
            : Fs{std::forward<FFwds>(fFwds)}...
        {
        }

        using Fs::operator()...;
    };

    template <typename... Fs>
    overload_set(Fs...) -> overload_set<Fs...>;

    template <typename Variant, typename... Fs>
    constexpr decltype(auto) match(Variant&& v, Fs&&... fs)
    {
        return std::visit(
            overload_set{std::forward<Fs>(fs)...}, std::forward<Variant>(v));
    }

    // TODO VR: (P1) reuse throughout project
    [[nodiscard]] QUAKE_FORCEINLINE glm::vec3 redirectVector(
        const glm::vec3& input, const glm::vec3& examplar) noexcept
    {
        const auto [fwd, right, up] = getAngledVectors(examplar);
        return fwd * input[0] + right * input[1] + up * input[2];
    }

    template <typename TTrace>
    [[nodiscard]] QUAKE_FORCEINLINE bool traceHitGround(const TTrace& trace)
    {
        return trace.plane.normal[2] > 0.7;
    }

    [[nodiscard]] inline int getMaxMSAALevel() noexcept
    {
        int res;
        glGetIntegerv(GL_MAX_SAMPLES, &res);
        return res;
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
} // namespace glm
