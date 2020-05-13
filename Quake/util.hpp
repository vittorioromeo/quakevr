#pragma once

#ifdef WIN32
#include <windows.h>
#include "debugapi.h"

#undef min
#undef max
#endif

#include "q_stdinc.hpp"
#include "cvar.hpp"
#include "mathlib.hpp"
#include "quakeglm.hpp"

#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <sstream>
#include <variant>

// TODO VR: (P2) ugly declaration
float VR_GetMenuMult() noexcept;

namespace quake::util
{

template <typename T>
[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr T mapRange(const T input,
    const T inputMin, const T inputMax, const T outputMin,
    const T outputMax) noexcept
{
    const T slope = T(1.0) * (outputMax - outputMin) / (inputMax - inputMin);

    return outputMin + slope * (input - inputMin);
}

template <typename T>
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN T cvarToEnum(const cvar_t& x) noexcept
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
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN constexpr bool boxIntersection(
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
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN constexpr bool entBoxIntersection(
    const TEntA& entA, const TEntB& entB) noexcept
{
    return boxIntersection(
        entA->v.absmin, entA->v.absmax, entB->v.absmin, entB->v.absmax);
}

[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN double lerp(
    double a, double b, double f) noexcept
{
    return (a * (1.0 - f)) + (b * f);
}

template <typename T>
[[nodiscard]] QUAKE_FORCEINLINE constexpr auto safeAsin(const T x) noexcept
{
    assert(!std::isnan(x));
    assert(x >= T(-1));
    assert(x <= T(1));

    return std::asin(x);
}

template <typename T>
[[nodiscard]] QUAKE_FORCEINLINE constexpr auto safeAtan2(
    const T y, const T x) noexcept
{
    assert(!std::isnan(y));
    assert(!std::isnan(x));
    assert(y != T(0) || x != T(0));

    return std::atan2(y, x);
}

[[nodiscard]] inline qvec3 pitchYawRollFromDirectionVector(
    const qvec3& up, const qvec3& dir)
{
    // From: https://stackoverflow.com/a/21627251/598696

    const auto pitch = safeAsin(dir[2]);
    const auto yaw = safeAtan2(dir[1], dir[0]);

    const auto w0 = qvec3{-dir[1], dir[0], 0};
    const auto u0 = glm::cross(w0, dir);

    const auto w0len = glm::length(w0);
    assert(w0len != 0);

    const auto u0len = glm::length(u0);
    assert(u0len != 0);

    const auto roll =
        safeAtan2(glm::dot(w0, up) / w0len, glm::dot(u0, up) / u0len);

    auto res = glm::degrees(qvec3{pitch, yaw, roll});
    res[0 /* PITCH */] *= -1.f;
    res[2 /* ROLL */] -= 180.f;
    return res;
}

[[nodiscard]] QUAKE_FORCEINLINE std::tuple<qvec3, qvec3, qvec3>
getAngledVectors(const qvec3& v) noexcept
{
    const auto yawRadians = glm::radians(v[YAW]);
    assert(!std::isnan(yawRadians));
    assert(!std::isinf(yawRadians));

    const auto sy = std::sin(yawRadians);
    const auto cy = std::cos(yawRadians);

    const auto pitchRadians = glm::radians(v[PITCH]);
    assert(!std::isnan(pitchRadians));
    assert(!std::isinf(pitchRadians));

    const auto sp = std::sin(pitchRadians);
    const auto cp = std::cos(pitchRadians);

    const auto rollRadians = glm::radians(v[ROLL]);
    assert(!std::isnan(rollRadians));
    assert(!std::isinf(rollRadians));

    const auto sr = std::sin(rollRadians);
    const auto cr = std::cos(rollRadians);

    const qvec3 forward{//
        cp * cy,        //
        cp * sy,        //
        -sp};

    const qvec3 right{                           //
        (-1.f * sr * sp * cy + -1.f * cr * -sy), //
        (-1.f * sr * sp * sy + -1.f * cr * cy),  //
        -1.f * sr * cp};

    const qvec3 up{                 //
        (cr * sp * cy + -sr * -sy), //
        (cr * sp * sy + -sr * cy),  //
        cr * cp};

    return std::tuple{forward, right, up};
}

[[nodiscard]] QUAKE_FORCEINLINE qvec3 getDirectionVectorFromPitchYawRoll(
    const qvec3& v) noexcept
{
    return AngleVectorsOnlyFwd(v);
}

// TODO VR: (P2) same as above, use one or the other
[[nodiscard]] QUAKE_FORCEINLINE qvec3 getFwdVecFromPitchYawRoll(
    const qvec3& v) noexcept
{
    return AngleVectorsOnlyFwd(v);
}

template <typename T>
[[nodiscard]] auto makeMenuAdjuster(const bool isLeft)
{
    return
        [isLeft](const cvar_t& cvar, const T incr, const T min, const T max) {
            const float factor = VR_GetMenuMult() >= 3 ? 6.f : VR_GetMenuMult();

            const T adjIncr = incr * static_cast<T>(factor);

            const auto newVal = static_cast<T>(
                isLeft ? cvar.value - adjIncr : cvar.value + adjIncr);
            const auto res = static_cast<T>(std::clamp(newVal, min, max));

            Cvar_SetValue(cvar.name, res);
        };
}

template <typename T>
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN bool hitSomething(
    const T& trace) noexcept
{
    return trace.fraction < 1.f;
}

template <typename... Fs>
struct overload_set : Fs...
{
    template <typename... FFwds>
    constexpr overload_set(FFwds&&... fFwds) : Fs{std::forward<FFwds>(fFwds)}...
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
        overload_set<std::decay_t<Fs>...>{std::forward<Fs>(fs)...},
        std::forward<Variant>(v));
}

// TODO VR: (P2) reuse throughout project
[[nodiscard]] QUAKE_FORCEINLINE qvec3 redirectVector(
    const qvec3& input, const qvec3& examplar) noexcept
{
    const auto [fwd, right, up] = getAngledVectors(examplar);
    return fwd * input[0] + right * input[1] + up * input[2];
}

template <typename TTrace>
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN bool traceHitGround(const TTrace& trace)
{
    return trace.plane.normal[2] > 0.7;
}

[[nodiscard]] inline int getMaxMSAALevel() noexcept
{
    int res;
    glGetIntegerv(GL_MAX_SAMPLES, &res);
    return res;
}

[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr bool hasFlag(
    const float flags, const int x) noexcept
{
    return static_cast<int>(flags) & x;
}

[[nodiscard]] QUAKE_FORCEINLINE_PUREFN constexpr bool hasFlag(
    const edict_t* edict, const int x) noexcept
{
    return hasFlag(edict->v.flags, x);
}

[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr int removeFlag(
    const float flags, const int x) noexcept
{
    return static_cast<int>(flags) & ~x;
}

QUAKE_FORCEINLINE constexpr void removeFlag(
    edict_t* edict, const int x) noexcept
{
    edict->v.flags = removeFlag(edict->v.flags, x);
}

[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr int addFlag(
    const float flags, const int x) noexcept
{
    return static_cast<int>(flags) | x;
}

QUAKE_FORCEINLINE constexpr void addFlag(edict_t* edict, const int x) noexcept
{
    edict->v.flags = addFlag(edict->v.flags, x);
}

[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr int toggleFlag(
    const float flags, const int x) noexcept
{
    return static_cast<int>(flags) ^ x;
}

QUAKE_FORCEINLINE constexpr void toggleFlag(
    edict_t* edict, const int x) noexcept
{
    edict->v.flags = toggleFlag(edict->v.flags, x);
}

template <typename... TFlags>
[[nodiscard]] QUAKE_FORCEINLINE_CONSTFN constexpr bool hasAnyFlag(
    const float flags, const TFlags... xs) noexcept
{
    return static_cast<int>(flags) & (xs | ...);
}

template <typename... TFlags>
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN constexpr bool hasAnyFlag(
    edict_t* edict, const TFlags... xs) noexcept
{
    return hasAnyFlag(edict->v.flags, xs...);
}

[[nodiscard]] QUAKE_FORCEINLINE_PUREFN bool canBeTouched(const edict_t* edict)
{
    return (edict->v.touch || edict->v.handtouch) &&
           edict->v.solid != SOLID_NOT;
}

[[nodiscard]] QUAKE_FORCEINLINE_PUREFN bool canBeHandTouched(
    const edict_t* edict)
{
    return edict->v.handtouch && edict->v.solid != SOLID_NOT;
}

template <typename F>
bool anyXYCorner(const edict_t& ent, F&& f)
{
    const qfloat left = ent.v.mins[0];
    const qfloat right = ent.v.maxs[0];
    const qfloat fwd = ent.v.mins[1];
    const qfloat back = ent.v.maxs[1];

    const auto doCorner = [&](const qfloat xOffset, const qfloat yOffset) {
        return f(qvec3{xOffset, yOffset, 0.f});
    };

    return doCorner(left, fwd) || doCorner(left, back) ||
           doCorner(right, fwd) || doCorner(right, back);
}

[[nodiscard]] static bool checkGroundCollision(const int moveType, edict_t* ent,
    trace_t& traceBuffer, qvec3& offsetBuffer, const qvec3& move,
    const float xBias, const float yBias)
{
    (void)xBias; // TODO VR: (P2) unused
    (void)yBias; // TODO VR: (P2) unused

    const auto checkCorner = [&](const qvec3& pos) {
        const qvec3 end = pos + move;

        traceBuffer = SV_MoveTrace(pos, end, moveType, ent);
        return hitSomething(traceBuffer) && traceHitGround(traceBuffer);
    };

    const qvec3 bottomOrigin = ent->v.origin + ent->v.mins[2];

    if(ent->v.mins == vec3_zero && ent->v.maxs == vec3_zero)
    {
        // Optimized case for zero-sized objects.
        offsetBuffer = vec3_zero;
        return checkCorner(bottomOrigin);
    }

    return anyXYCorner(*ent, [&](const qvec3& offset) {
        offsetBuffer = offset;
        return checkCorner(bottomOrigin + offsetBuffer);
    });
}

} // namespace quake::util

namespace std
{
template <int D, typename T, glm::qualifier P>
struct tuple_size<glm::vec<D, T, P>> : std::integral_constant<std::size_t, D>
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
[[nodiscard]] QUAKE_FORCEINLINE_PUREFN T get(
    const glm::vec<D, T, P>& v) noexcept
{
    return v[I];
}
} // namespace glm
