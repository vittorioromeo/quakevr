// vr_geom.hpp -- small shared geometry helpers (header only).

#pragma once

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"

#include <glm/glm.hpp>

namespace qvr::geom
{

// Float pi and degrees-to-radians factor (the 3.14159265f every file used to spell out).
inline constexpr float pi = 3.14159265f;
inline constexpr float deg = pi / 180.f;

// Quake's world up (+z).
inline constexpr glm::vec3 up{0.f, 0.f, 1.f};

// A Quake vec3_t / float[3] as a glm::vec3, and back.
[[nodiscard]] inline glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

inline void store(const glm::vec3& g, float* out)
{
    out[0] = g.x;
    out[1] = g.y;
    out[2] = g.z;
}

// `v` as a unit vector, or `fallback` when its length is not above `eps`.
[[nodiscard]] inline glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback = up, float eps = 1e-5f)
{
    const float len = glm::length(v);
    return len > eps ? v / len : fallback;
}

// An angle in degrees wrapped to [-180, 180] (rounds to the nearest turn; any finite input, no loop).
[[nodiscard]] inline float wrap180(float a)
{
    return a - za::round(a / 360.f) * 360.f;
}

// The shortest signed yaw turn from `from` to `to`, degrees.
[[nodiscard]] inline float yawDelta(float to, float from)
{
    return wrap180(to - from);
}

// The yaw (degrees) of a direction's horizontal part, 0 along +x.
[[nodiscard]] inline float yawOf(const glm::vec3& dir)
{
    return glm::degrees(za::atan2(dir.y, dir.x));
}

// Tracking space (x right, y up, -z forward) to Quake space (x forward, y left, z up).
[[nodiscard]] inline glm::vec3 quakeFromTracking(const glm::vec3& v)
{
    return {-v.z, -v.x, v.y};
}

// Smoothstep of t clamped to [0, 1]: 3t^2 - 2t^3.
[[nodiscard]] inline float smooth01(float t)
{
    t = za::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Smoothstep of x between edges e0 and e1 (clamped); the edge width is at least `minWidth` (default: as given).
[[nodiscard]] inline float smoothstep(float e0, float e1, float x, float minWidth = -3.4e38f)
{
    return smooth01((x - e0) / za::max(e1 - e0, minWidth));
}

// `now` moved towards `target` by the exponential approach 1 - exp(-dt / tau).
[[nodiscard]] inline float approachExp(float now, float target, float dt, float tau)
{
    return now + (target - now) * (1.f - za::exp(-dt / tau));
}

// The point of segment ab nearest p; `t` (when given) is its parameter along ab. A segment with squared length not
// above `eps` counts as a point at a.
[[nodiscard]] inline glm::vec3 closestOnSegment(
    const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, float* t = nullptr, float eps = 1e-6f)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float s = len2 > eps ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    if(t != nullptr)
    {
        *t = s;
    }
    return a + ab * s;
}

// The distance from p to segment ab (see closestOnSegment).
[[nodiscard]] inline float segmentDistance(
    const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, float* t = nullptr, float eps = 1e-6f)
{
    return glm::length(p - closestOnSegment(p, a, b, t, eps));
}

// The point of triangle abc nearest p (Ericson, Real-Time Collision Detection, 5.1.5), and, when `bary` is given, its
// barycentric weights for a, b and c. The point is computed per region exactly as the copies it replaced did (a corner
// is returned as that corner, not rebuilt from the weights), so their results are unchanged bit for bit.
[[nodiscard]] inline glm::vec3 closestOnTriangle(
    const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, glm::vec3* bary = nullptr)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        if(bary != nullptr)
        {
            *bary = {1.f, 0.f, 0.f};
        }
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        if(bary != nullptr)
        {
            *bary = {0.f, 1.f, 0.f};
        }
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        const float v = d1 / (d1 - d3);
        if(bary != nullptr)
        {
            *bary = {1.f - v, v, 0.f};
        }
        return a + ab * v;
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        if(bary != nullptr)
        {
            *bary = {0.f, 0.f, 1.f};
        }
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        const float w = d2 / (d2 - d6);
        if(bary != nullptr)
        {
            *bary = {1.f - w, 0.f, w};
        }
        return a + ac * w;
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        if(bary != nullptr)
        {
            *bary = {0.f, 1.f - w, w};
        }
        return b + (c - b) * w;
    }
    const float denom = 1.f / (va + vb + vc);
    const float v = vb * denom, w = vc * denom;
    if(bary != nullptr)
    {
        *bary = {1.f - v - w, v, w};
    }
    return a + ab * v + ac * w;
}

} // namespace qvr::geom
