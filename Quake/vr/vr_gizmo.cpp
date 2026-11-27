// vr_gizmo.cpp -- see vr_gizmo.hpp (the studios' transform handle). Lifted from the Rig Studio's viewport
// (vr_rigstudio_view.cpp) as it was, so its handles look and pick as before.

#include "vr_gizmo.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Tan.hpp"

namespace qvr::gizmo
{

namespace
{

using geom::pi;
using geom::deg;

[[nodiscard]] ImU32 col(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.x, c.y, c.z, c.w});
}

[[nodiscard]] ImVec2 iv(const glm::vec2& v)
{
    return ImVec2{v.x, v.y};
}

[[nodiscard]] float ringRadius(const Handle& h)
{
    return h.radius * (h.move && h.rot ? ringOut : 1.f);
}

// The ring (0..2) nearest `m` within 7 pixels, else -1.
[[nodiscard]] int hitRing(const Handle& h, const Projection& v, const glm::vec2& m)
{
    int best = -1;
    float bestD = 7.f;
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 pts[ringN];
        bool front[ringN];
        ringPoints(h, v, h.axis[i], pts, front, ringN);
        for(int k = 0; k < ringN; k++)
        {
            if(!front[k] && !front[(k + 1) % ringN])
            {
                continue;
            }
            const float d = segDist(m, pts[k], pts[(k + 1) % ringN]);
            if(d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
    }
    return best;
}

// A plane's square (parts 4..6) on the screen: between 0.25 and 0.45 of the radius along its two axes.
void planeQuad(const Handle& h, const Projection& v, int i, glm::vec2* q)
{
    const glm::vec3 a = h.axis[(i + 1) % 3], b = h.axis[(i + 2) % 3];
    v.project(h.centre + (a * 0.25f + b * 0.25f) * h.radius, q[0]);
    v.project(h.centre + (a * 0.45f + b * 0.25f) * h.radius, q[1]);
    v.project(h.centre + (a * 0.45f + b * 0.45f) * h.radius, q[2]);
    v.project(h.centre + (a * 0.25f + b * 0.45f) * h.radius, q[3]);
}

} // namespace

bool Projection::project(const glm::vec3& world, glm::vec2& out, float* depth) const
{
    const glm::vec3 d = world - org;
    const float z = glm::dot(d, fwd);
    if(depth)
    {
        *depth = z;
    }
    if(z < 0.5f)
    {
        out = glm::vec2{-10000.f};
        return false;
    }
    const float sx = glm::dot(d, right) / z / tanX;
    const float sy = glm::dot(d, up) / z / tanY;
    out = glm::vec2{x + (sx * 0.5f + 0.5f) * w, y + (0.5f - sy * 0.5f) * h};
    return true;
}

glm::vec3 Projection::rayDir(const glm::vec2& m) const
{
    const float sx = ((m.x - x) / w) * 2.f - 1.f;
    const float sy = 1.f - ((m.y - y) / h) * 2.f;
    return glm::normalize(fwd + right * (sx * tanX) + up * (sy * tanY));
}

bool Projection::rayPlane(const glm::vec2& m, const glm::vec3& p0, const glm::vec3& n, glm::vec3& hit) const
{
    const glm::vec3 d = rayDir(m);
    const float den = glm::dot(d, n);
    if(za::abs(den) < 1e-4f)
    {
        return false;
    }
    const float t = glm::dot(p0 - org, n) / den;
    if(t <= 0.f)
    {
        return false;
    }
    hit = org + d * t;
    return true;
}

Projection refdefProjection(float displayW, float displayH)
{
    Projection v;
    v.org = geom::vec(r_refdef.vieworg);
    vec3_t f, r, u;
    AngleVectors(r_refdef.viewangles, f, r, u);
    v.fwd = geom::vec(f);
    v.right = geom::vec(r);
    v.up = geom::vec(u);
    const float fx = r_refdef.fov_x > 1.f ? r_refdef.fov_x : 90.f;
    const float fy = r_refdef.fov_y > 1.f ? r_refdef.fov_y : 73.7f;
    v.tanX = za::tan(fx * 0.5f * deg);
    v.tanY = za::tan(fy * 0.5f * deg);
    v.w = za::max(displayW, 1.f);
    v.h = za::max(displayH, 1.f);
    return v;
}

float segDist(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
{
    const glm::vec2 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float t = l2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return glm::length(p - (a + ab * t));
}

void place(Handle& h, const Projection& v, float sizePixels)
{
    h.shown = false;
    glm::vec2 s;
    float depth = 1.f;
    if(!v.project(h.centre, s, &depth))
    {
        return;
    }
    h.radius = sizePixels * depth * 2.f * v.tanY / v.h;
    h.shown = true;
}

void ringPoints(const Handle& h, const Projection& v, const glm::vec3& a, glm::vec2* out, bool* front, int n)
{
    glm::vec3 u = glm::abs(a.z) < 0.9f ? glm::cross(a, glm::vec3{0.f, 0.f, 1.f}) : glm::cross(a, glm::vec3{1.f, 0.f, 0.f});
    u = glm::normalize(u);
    const glm::vec3 w = glm::cross(a, u);
    const float r = ringRadius(h);
    for(int i = 0; i < n; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n) * 2.f * pi;
        const glm::vec3 p = h.centre + (u * za::cos(t) + w * za::sin(t)) * r;
        v.project(p, out[i]);
        front[i] = glm::dot(p - h.centre, v.org - h.centre) >= -0.05f * r;
    }
}

int hit(const Handle& h, const Projection& v, const glm::vec2& m, const Style& style)
{
    if(!h.shown)
    {
        return -1;
    }
    glm::vec2 c;
    v.project(h.centre, c);
    if(h.rot && !h.move)
    {
        int best = hitRing(h, v, m);
        if(best < 0 && style.viewRing > 0.f && za::abs(glm::length(m - c) - style.viewRing) < 7.f)
        {
            return 3; // (the view ring)
        }
        return best >= 0 ? style.ringBase + best : best;
    }
    // Move and scale: the arms, the planes (move), the centre
    if(glm::length(m - c) < 8.f)
    {
        return 3;
    }
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 e;
        v.project(h.centre + h.axis[i] * h.radius, e);
        if(segDist(m, c, e) < (h.scale ? 7.f : 6.f))
        {
            return i;
        }
    }
    for(int i = 0; i < 3 && !h.scale; i++)
    {
        glm::vec2 q[4];
        planeQuad(h, v, i, q);
        const glm::vec2 mid = (q[0] + q[2]) * 0.5f;
        if(glm::length(m - mid) < za::max(6.f, glm::length(q[2] - q[0]) * 0.5f))
        {
            return 4 + i;
        }
    }
    if(h.rot)
    {
        const int r = hitRing(h, v, m); // (the rings outside the arrows)
        return r >= 0 ? 20 + r : -1;
    }
    return -1;
}

void draw(const Handle& h, const Projection& v, ImDrawList* dl, int hot, const Style& style)
{
    if(!h.shown || !dl)
    {
        return;
    }
    const glm::vec4 axisCol[3] = {{0.95f, 0.25f, 0.3f, 1.f}, {0.4f, 0.85f, 0.2f, 1.f}, {0.25f, 0.5f, 1.f, 1.f}};
    const glm::vec4 hotCol{1.f, 0.9f, 0.3f, 1.f};
    glm::vec2 c;
    v.project(h.centre, c);
    if(h.rot)
    {
        const int ringBase = h.move ? 20 : style.ringBase;
        for(int i = 0; i < 3; i++)
        {
            glm::vec2 pts[ringN];
            bool front[ringN];
            ringPoints(h, v, h.axis[i], pts, front, ringN);
            for(int k = 0; k < ringN; k++)
            {
                const int k2 = (k + 1) % ringN;
                const bool f = front[k] && front[k2];
                glm::vec4 cc = hot == ringBase + i ? hotCol : axisCol[i];
                cc.w = f ? 1.f : 0.25f;
                dl->AddLine(iv(pts[k]), iv(pts[k2]), col(cc), f ? 2.5f : 1.2f);
            }
        }
        if(!h.move && style.viewRing > 0.f)
        {
            dl->AddCircle(iv(c), style.viewRing, col(hot == 3 ? hotCol : glm::vec4{0.9f, 0.9f, 0.9f, 0.8f}), 48, 1.6f);
        }
        if(!h.move)
        {
            return;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 e;
        v.project(h.centre + h.axis[i] * h.radius, e);
        const ImU32 cc = col(hot == i ? hotCol : axisCol[i]);
        dl->AddLine(iv(c), iv(e), cc, 2.5f);
        const glm::vec2 d = glm::length(e - c) > 1e-3f ? glm::normalize(e - c) : glm::vec2{1.f, 0.f};
        const glm::vec2 n{-d.y, d.x};
        if(h.scale)
        {
            // (a box end: the scale arm's)
            dl->AddQuadFilled(iv(e + (n - d) * 5.f), iv(e + (n + d) * 5.f), iv(e + (d - n) * 5.f), iv(e - (n + d) * 5.f), cc);
        }
        else
        {
            dl->AddTriangleFilled(iv(e + d * 10.f), iv(e + n * 5.f), iv(e - n * 5.f), cc);
        }
    }
    for(int i = 0; i < 3 && !h.scale; i++)
    {
        glm::vec2 q[4];
        planeQuad(h, v, i, q);
        glm::vec4 cc = hot == 4 + i ? hotCol : axisCol[i];
        cc.w = 0.45f;
        dl->AddQuadFilled(iv(q[0]), iv(q[1]), iv(q[2]), iv(q[3]), col(cc));
    }
    if(h.scale)
    {
        dl->AddCircle(iv(c), 8.f, col(hot == 3 ? hotCol : glm::vec4{1.f, 1.f, 1.f, 0.85f}), 24, 2.f);
    }
    dl->AddCircleFilled(iv(c), 5.f, col(hot == 3 ? hotCol : glm::vec4{1.f, 1.f, 1.f, 0.85f}));
}

glm::vec3 axisMove(const Handle& h, const Projection& v, int axis, const glm::vec2& start, const glm::vec2& m)
{
    glm::vec2 c, e;
    v.project(h.centre, c);
    v.project(h.centre + h.axis[axis] * h.radius, e);
    const glm::vec2 A = e - c;
    const float l2 = za::max(glm::dot(A, A), 1e-3f);
    return h.axis[axis] * (h.radius * glm::dot(m - start, A) / l2);
}

glm::vec3 axisMoveExact(const Handle& h, const Projection& v, int axis, const glm::vec2& start, const glm::vec2& m)
{
    // (the axis line's nearest points to the rays through `start` and `m`)
    const glm::vec3 a = h.axis[axis];
    const auto along = [&](const glm::vec2& p, float& t) {
        const glm::vec3 d = v.rayDir(p), w = h.centre - v.org;
        const float b = glm::dot(a, d), den = 1.f - b * b;
        if(den < 1e-3f)
        {
            return false; // (the axis along the ray)
        }
        t = (b * glm::dot(d, w) - glm::dot(a, w)) / den;
        return true;
    };
    float t0 = 0.f, t1 = 0.f;
    if(!along(start, t0) || !along(m, t1))
    {
        return axisMove(h, v, axis, start, m);
    }
    return a * (t1 - t0);
}

float ringSweep(const Handle& h, const Projection& v, const glm::vec3& axisW, const glm::vec2& last, const glm::vec2& m)
{
    glm::vec2 c;
    v.project(h.centre, c);
    const glm::vec2 a0 = last - c, a1 = m - c;
    if(glm::length(a0) < 2.f || glm::length(a1) < 2.f)
    {
        return 0.f;
    }
    float d = za::atan2(-a1.y, a1.x) - za::atan2(-a0.y, a0.x);
    d = d > pi ? d - 2.f * pi : d < -pi ? d + 2.f * pi : d;
    return d * (glm::dot(axisW, v.fwd) < 0.f ? 1.f : -1.f);
}

float ringSweepExact(const Handle& h, const Projection& v, const glm::vec3& axisW, const glm::vec2& last, const glm::vec2& m)
{
    // (the mouse's rays on the ring's plane, the angle between them round its centre; the plane seen edge on: the screen's)
    glm::vec3 a, b;
    if(za::abs(glm::dot(v.rayDir(m), axisW)) < 0.15f || !v.rayPlane(last, h.centre, axisW, a) || !v.rayPlane(m, h.centre, axisW, b))
    {
        return ringSweep(h, v, axisW, last, m);
    }
    const glm::vec3 u = a - h.centre, w = b - h.centre;
    if(glm::length(u) < 1e-4f || glm::length(w) < 1e-4f)
    {
        return 0.f;
    }
    return za::atan2(glm::dot(axisW, glm::cross(u, w)), glm::dot(u, w));
}

bool ringGrip(const Handle& h, const Projection& v, int axis, glm::vec2& out)
{
    glm::vec2 c;
    v.project(h.centre, c);
    glm::vec2 pts[ringN];
    bool front[ringN];
    ringPoints(h, v, h.axis[axis], pts, front, ringN);
    int best = -1;
    for(int k = 0; k < ringN; k++)
    {
        if(front[k] && front[(k + 1) % ringN] && (best < 0 || glm::length(pts[k] - c) > glm::length(pts[best] - c)))
        {
            best = k;
        }
    }
    if(best < 0)
    {
        return false;
    }
    out = pts[best];
    return true;
}

} // namespace qvr::gizmo
