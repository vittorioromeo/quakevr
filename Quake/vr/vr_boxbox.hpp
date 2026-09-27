// vr_boxbox.hpp -- oriented box against oriented box: the contact manifold (vr_rigid.cpp's stacking).
//
// The separating-axis test over the 15 axes (3 faces of each box, 9 edge pairs) finds the axis of least
// penetration (or of the largest gap, within a margin: speculative contacts). On a face axis, the other
// box's face most turned against it (the incident face) is clipped by the side planes of the reference
// face (Sutherland-Hodgman), and every clipped corner within the margin of the reference face is a
// contact: up to 8, reduced to the 4 that span the largest area (the deepest first). On an edge axis the
// contact is the closest points of the two edges. So a box resting flat on another has four contacts, at
// the corners of their overlap, which is what lets a stack stand still.
//
// Face axes are preferred to edge axes, and box A's faces to box B's, unless the other is clearly better
// (relative and absolute tolerances, as Box2D's): nearly equal choices otherwise flip from frame to frame,
// and the contacts (and their warm-started impulses) with them.

#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace qvr::boxbox
{

struct Box
{
    glm::vec3 c;  // centre, world
    glm::mat3 r;  // axes (columns), world
    glm::vec3 h;  // half-extents along them
};

struct Point
{
    glm::vec3 p;  // world: midway between the two surfaces
    float depth;  // penetration (> 0 inside, < 0 a gap)
};

struct Manifold
{
    glm::vec3 n{0.f}; // unit, from A towards B (B is pushed along it, A against it)
    int count{0};
    std::array<Point, 4> pts{};
    bool edge{false};
};

namespace detail
{

[[nodiscard]] inline float radius(const Box& b, const glm::vec3& axis)
{
    return b.h.x * std::abs(glm::dot(b.r[0], axis)) + b.h.y * std::abs(glm::dot(b.r[1], axis)) +
           b.h.z * std::abs(glm::dot(b.r[2], axis));
}

// Keeps the part of `in` (count n) with dot(p, axis) <= limit.
inline int clip(const glm::vec3* in, int n, const glm::vec3& axis, float limit, glm::vec3* out)
{
    int m = 0;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3& p = in[i];
        const glm::vec3& q = in[(i + 1) % n];
        const float dp = glm::dot(p, axis) - limit;
        const float dq = glm::dot(q, axis) - limit;
        if(dp <= 0.f)
        {
            out[m++] = p;
        }
        if((dp < 0.f && dq > 0.f) || (dp > 0.f && dq < 0.f))
        {
            out[m++] = p + (q - p) * (dp / (dp - dq));
        }
    }
    return m;
}

} // namespace detail

// Contacts between A and B, or false when they are further apart than `margin` along some axis.
[[nodiscard]] inline bool collide(const Box& a, const Box& b, float margin, Manifold& out)
{
    using detail::radius;
    const glm::vec3 t = b.c - a.c;

    // Face axes: A's, then B's.
    float faceSep[2] = {-1e30f, -1e30f};
    int faceAxis[2] = {0, 0};
    for(int which = 0; which < 2; which++)
    {
        const Box& box = which == 0 ? a : b;
        for(int i = 0; i < 3; i++)
        {
            const glm::vec3& axis = box.r[i];
            const float sep = std::abs(glm::dot(t, axis)) - box.h[i] - radius(which == 0 ? b : a, axis);
            if(sep > margin)
            {
                return false;
            }
            if(sep > faceSep[which])
            {
                faceSep[which] = sep;
                faceAxis[which] = i;
            }
        }
    }

    // Edge pairs (parallel edges give no axis of their own: a face axis covers them).
    float edgeSep = -1e30f;
    int edgeI = -1, edgeJ = -1;
    glm::vec3 edgeAxis{0.f};
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            glm::vec3 axis = glm::cross(a.r[i], b.r[j]);
            const float length = glm::length(axis);
            if(length < 1e-3f)
            {
                continue;
            }
            axis /= length;
            const float sep = std::abs(glm::dot(t, axis)) - radius(a, axis) - radius(b, axis);
            if(sep > margin)
            {
                return false;
            }
            if(sep > edgeSep)
            {
                edgeSep = sep;
                edgeI = i;
                edgeJ = j;
                edgeAxis = axis;
            }
        }
    }

    constexpr float relTol = 0.95f;
    const float absTol = 0.01f * std::min({a.h.x, a.h.y, a.h.z, b.h.x, b.h.y, b.h.z});
    int ref = 0; // 0: A's face, 1: B's face, 2: an edge pair
    float best = faceSep[0];
    if(faceSep[1] > relTol * best + absTol)
    {
        ref = 1;
        best = faceSep[1];
    }
    if(edgeI >= 0 && edgeSep > relTol * best + 5.f * absTol)
    {
        ref = 2;
        best = edgeSep;
    }

    out = Manifold{};
    if(ref == 2)
    {
        // The two edges nearest each other along the axis; the contact is between their closest points.
        const glm::vec3 n = glm::dot(t, edgeAxis) < 0.f ? -edgeAxis : edgeAxis;
        glm::vec3 pa = a.c, pb = b.c;
        for(int k = 0; k < 3; k++)
        {
            if(k != edgeI)
            {
                pa += a.r[k] * (glm::dot(a.r[k], n) >= 0.f ? a.h[k] : -a.h[k]);
            }
            if(k != edgeJ)
            {
                pb += b.r[k] * (glm::dot(b.r[k], n) >= 0.f ? -b.h[k] : b.h[k]);
            }
        }
        const glm::vec3& da = a.r[edgeI];
        const glm::vec3& db = b.r[edgeJ];
        const glm::vec3 r = pb - pa;
        const float k = glm::dot(da, db);
        const float denom = 1.f - k * k;
        float s = 0.f, u = 0.f;
        if(denom > 1e-6f)
        {
            s = (glm::dot(da, r) - k * glm::dot(db, r)) / denom;
            s = glm::clamp(s, -a.h[edgeI], a.h[edgeI]);
        }
        u = glm::clamp(glm::dot(db, pa + da * s - pb), -b.h[edgeJ], b.h[edgeJ]);
        s = glm::clamp(glm::dot(da, pb + db * u - pa), -a.h[edgeI], a.h[edgeI]);
        const glm::vec3 ca = pa + da * s;
        const glm::vec3 cb = pb + db * u;
        out.n = n;
        out.edge = true;
        out.count = 1;
        out.pts[0] = Point{(ca + cb) * 0.5f, -best};
        return true;
    }

    // Reference face (on `rb`, facing `nr` towards the incident box `ib`).
    const Box& rb = ref == 0 ? a : b;
    const Box& ib = ref == 0 ? b : a;
    const int k = faceAxis[ref];
    const glm::vec3 towards = ref == 0 ? t : -t;
    const glm::vec3 nr = glm::dot(towards, rb.r[k]) < 0.f ? -rb.r[k] : rb.r[k];
    const glm::vec3 faceCentre = rb.c + nr * rb.h[k];
    const int k1 = (k + 1) % 3, k2 = (k + 2) % 3;

    // Incident face: the one of the other box most turned against the reference normal.
    int j = 0;
    float most = -1.f;
    for(int i = 0; i < 3; i++)
    {
        const float d = std::abs(glm::dot(ib.r[i], nr));
        if(d > most)
        {
            most = d;
            j = i;
        }
    }
    const glm::vec3 ni = glm::dot(ib.r[j], nr) > 0.f ? -ib.r[j] : ib.r[j];
    const glm::vec3 ic = ib.c + ni * ib.h[j];
    const int j1 = (j + 1) % 3, j2 = (j + 2) % 3;
    const glm::vec3 e1 = ib.r[j1] * ib.h[j1], e2 = ib.r[j2] * ib.h[j2];

    // Relative to the reference box's centre, clipped by its four side planes.
    glm::vec3 bufA[8] = {ic + e1 + e2 - rb.c, ic - e1 + e2 - rb.c, ic - e1 - e2 - rb.c, ic + e1 - e2 - rb.c};
    glm::vec3 bufB[8];
    int n = 4;
    n = detail::clip(bufA, n, rb.r[k1], rb.h[k1], bufB);
    n = detail::clip(bufB, n, -rb.r[k1], rb.h[k1], bufA);
    n = detail::clip(bufA, n, rb.r[k2], rb.h[k2], bufB);
    n = detail::clip(bufB, n, -rb.r[k2], rb.h[k2], bufA);

    Point pts[8];
    int count = 0;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 p = bufA[i] + rb.c;
        const float depth = glm::dot(faceCentre - p, nr);
        if(depth >= -margin)
        {
            pts[count++] = Point{p + nr * (depth * 0.5f), depth};
        }
    }
    if(count == 0)
    {
        return false;
    }

    out.n = ref == 0 ? nr : -nr;
    if(count <= 4)
    {
        out.count = count;
        for(int i = 0; i < count; i++)
        {
            out.pts[i] = pts[i];
        }
        return true;
    }

    // More than 4: the deepest, the one furthest from it, the one furthest from the line of the two, and
    // the one outside that triangle that adds the most area.
    int i0 = 0;
    for(int i = 1; i < count; i++)
    {
        if(pts[i].depth > pts[i0].depth)
        {
            i0 = i;
        }
    }
    int i1 = i0 == 0 ? 1 : 0;
    float far = -1.f;
    for(int i = 0; i < count; i++)
    {
        const glm::vec3 d = pts[i].p - pts[i0].p;
        if(i != i0 && glm::dot(d, d) > far)
        {
            far = glm::dot(d, d);
            i1 = i;
        }
    }
    int i2 = -1;
    float area = -1.f;
    for(int i = 0; i < count; i++)
    {
        const float a2 = std::abs(glm::dot(glm::cross(pts[i1].p - pts[i0].p, pts[i].p - pts[i0].p), nr));
        if(i != i0 && i != i1 && a2 > area)
        {
            area = a2;
            i2 = i;
        }
    }
    out.pts[0] = pts[i0];
    out.pts[1] = pts[i1];
    out.count = 2;
    if(i2 < 0)
    {
        return true;
    }
    out.pts[2] = pts[i2];
    out.count = 3;

    // Signed areas against the triangle's own winding: a point outside has a negative one.
    const glm::vec3 tri[3] = {pts[i0].p, pts[i1].p, pts[i2].p};
    const float wind = glm::dot(glm::cross(tri[1] - tri[0], tri[2] - tri[0]), nr) >= 0.f ? 1.f : -1.f;
    int i3 = -1;
    float added = 0.f;
    for(int i = 0; i < count; i++)
    {
        if(i == i0 || i == i1 || i == i2)
        {
            continue;
        }
        float least = 0.f;
        for(int e = 0; e < 3; e++)
        {
            const float s = wind * glm::dot(glm::cross(tri[(e + 1) % 3] - tri[e], pts[i].p - tri[e]), nr);
            least = std::min(least, s);
        }
        if(-least > added)
        {
            added = -least;
            i3 = i;
        }
    }
    if(i3 >= 0)
    {
        out.pts[3] = pts[i3];
        out.count = 4;
    }
    return true;
}

} // namespace qvr::boxbox
