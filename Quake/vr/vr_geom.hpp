// vr_geom.hpp -- small shared geometry helpers (header only).

#pragma once

#include <glm/glm.hpp>

namespace qvr::geom
{

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
