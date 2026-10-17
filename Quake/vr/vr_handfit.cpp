// vr_handfit.cpp -- the hand fits' shared core: the shape query and the solver (vr_handfit.hpp).

#include "vr_handfit.hpp"

#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::handfit
{

namespace
{

// Hulls: how far `p` is in past `allowed`: in a hull, the least of its face distances inward (left at the first face it
// is within `allowed` of: not in that hull past it); the most of the hulls'.
[[nodiscard]] float hullsSunk(const Shape& s, const glm::vec3& p, float allowed, int* face)
{
    float most = 0.f;
    int begin = 0;
    for(int k = 0; k < s.hulls; k++)
    {
        const int end = s.hullEnds[k];
        float inner = none;
        int nearest = begin;
        int j = begin;
        for(; j < end; j++)
        {
            const glm::vec4& pl = s.planes[j];
            const float d = pl.w - (pl.x * p.x + pl.y * p.y + pl.z * p.z);
            if(d <= allowed)
            {
                break;
            }
            if(d < inner)
            {
                inner = d;
                nearest = j;
            }
        }
        if(j == end && end > begin && inner - allowed > most)
        {
            most = inner - allowed;
            if(face)
            {
                *face = nearest;
            }
        }
        begin = end;
    }
    return most;
}

// Hulls: in each hull `p` is in past `allowed`, from where the first of its faces (each one's distance falling by its
// normal's share of the move along `e`) comes within it; the most of the hulls'.
[[nodiscard]] float hullsOut(const Shape& s, const glm::vec3& p, const glm::vec3& e, float allowed)
{
    float most = 0.f;
    int begin = 0;
    for(int k = 0; k < s.hulls; k++)
    {
        const int end = s.hullEnds[k];
        float least = none;
        for(int j = begin; j < end; j++)
        {
            const glm::vec4& pl = s.planes[j];
            const float d = pl.w - (pl.x * p.x + pl.y * p.y + pl.z * p.z);
            if(d <= allowed)
            {
                least = 0.f;
                break;
            }
            const float along = pl.x * e.x + pl.y * e.y + pl.z * e.z;
            if(along > 1e-6f)
            {
                least = za::fmin(least, (d - allowed) / along);
            }
        }
        most = za::fmax(most, least);
        begin = end;
    }
    return most;
}

// Surface: the triangles' nearest y over (p.x, p.z) (where the line through it along y crosses them: barycentric in the
// x-z plane); none if nothing is over it.
[[nodiscard]] float surfaceNearest(const Shape& s, const glm::vec3& p)
{
    const float x = p.x, z = p.z;
    float nearest = none;
    for(int i = 0; i < s.triangleCount; i++)
    {
        const Triangle& t = s.triangles[i];
        const glm::vec2 a{t.p[0].x, t.p[0].z}, b{t.p[1].x, t.p[1].z}, c{t.p[2].x, t.p[2].z};
        const float det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if(za::fabs(det) < 1e-6f)
        {
            continue;
        }
        const float u = ((x - a.x) * (c.y - a.y) - (c.x - a.x) * (z - a.y)) / det;
        const float v = ((b.x - a.x) * (z - a.y) - (x - a.x) * (b.y - a.y)) / det;
        if(u < 0.f || v < 0.f || u + v > 1.f)
        {
            continue;
        }
        nearest = za::min(nearest, t.p[0].y + u * (t.p[1].y - t.p[0].y) + v * (t.p[2].y - t.p[0].y));
    }
    return nearest;
}

// The probes moved `s` along `e`: the deepest group's sunk() (each group at its least sunk set).
[[nodiscard]] float deepestAt(const Shape& shape, const Probes& probes, const glm::vec3& e, float s)
{
    float deepest = -none;
    for(int g = 0; g < probes.groupCount; g++)
    {
        const Group& group = probes.groups[g];
        float best = none;
        for(int a = 0; a < group.alternatives && group.count > 0; a++)
        {
            float most = -none;
            for(int k = 0; k < group.count; k++)
            {
                const glm::vec4& p = probes.points[group.first + a * group.count + k];
                most = za::max(most, sunk(shape, glm::vec3{p} + e * s, p.w));
            }
            best = za::min(best, most);
        }
        deepest = group.count > 0 ? za::max(deepest, best) : deepest;
    }
    return deepest;
}

// Hulls, Surface: the least move along `e` that leaves no group in past its allowance (each group's soonest set; a set out
// when its last point is). A Hulls point never needs less than 0: a set is skipped once one is out where it is.
[[nodiscard]] Slide exact(const Shape& shape, const Probes& probes, const glm::vec3& e)
{
    const float least = shape.kind == Shape::Kind::Hulls ? 0.f : -none;
    float t = -none;
    for(int g = 0; g < probes.groupCount && t < none; g++)
    {
        const Group& group = probes.groups[g];
        if(group.count <= 0)
        {
            continue;
        }
        float best = none;
        for(int a = 0; a < group.alternatives && best > least; a++)
        {
            float most = -none;
            for(int k = 0; k < group.count && most < best; k++)
            {
                const glm::vec4& p = probes.points[group.first + a * group.count + k];
                most = za::fmax(most, outAlong(shape, glm::vec3{p}, e, p.w));
            }
            best = za::fmin(best, most);
        }
        t = za::fmax(t, best);
    }
    Slide out;
    out.t = t;
    out.found = t > -none && t < none;
    return out;
}

// Distance: marched along `e` in steps, then halved down to where the probes cross the surface (to a few thousandths of a
// unit): from clear of it towards it, from in it back out.
[[nodiscard]] Slide marched(const Shape& shape, const Probes& probes, const glm::vec3& e, float reach)
{
    constexpr float step = 0.25f; // units
    Slide out;
    out.before = deepestAt(shape, probes, e, 0.f);
    const bool clear = out.before < 0.f;
    const float dir = clear ? 1.f : -1.f;
    float lo = 0.f, hi = 0.f;
    bool found = false;
    for(float s = step; s <= reach + step * 0.5f && !found; s += step)
    {
        if((deepestAt(shape, probes, e, dir * s) < 0.f) != clear)
        {
            lo = s - step;
            hi = s;
            found = true;
        }
    }
    if(!found)
    {
        return out;
    }
    for(int i = 0; i < 12; i++)
    {
        const float mid = (lo + hi) * 0.5f;
        ((deepestAt(shape, probes, e, dir * mid) < 0.f) == clear ? lo : hi) = mid;
    }
    out.found = true;
    out.t = dir * (clear ? lo : hi); // (touching: the side of the crossing clear of it)
    return out;
}

} // namespace

Shape Shape::ofHulls(const glm::vec4* planes, const int* hullEnds, int hulls)
{
    Shape s;
    s.kind = Kind::Hulls;
    s.planes = planes;
    s.hullEnds = hullEnds;
    s.hulls = hulls;
    return s;
}

Shape Shape::ofSurface(const Triangle* triangles, int count)
{
    Shape s;
    s.kind = Kind::Surface;
    s.triangles = triangles;
    s.triangleCount = count;
    return s;
}

Shape Shape::ofDistance(float (*distance)(const void*, const glm::vec3&), const void* context)
{
    Shape s;
    s.kind = Kind::Distance;
    s.distance = distance;
    s.context = context;
    return s;
}

float sunk(const Shape& shape, const glm::vec3& p, float allowed, int* face)
{
    switch(shape.kind)
    {
        case Shape::Kind::Hulls: return hullsSunk(shape, p, allowed, face);
        case Shape::Kind::Surface:
        {
            const float nearest = surfaceNearest(shape, p);
            return nearest == none ? -none : p.y - allowed - nearest;
        }
        case Shape::Kind::Distance: return -shape.distance(shape.context, p) - allowed;
    }
    return 0.f;
}

float outAlong(const Shape& shape, const glm::vec3& p, const glm::vec3& e, float allowed)
{
    switch(shape.kind)
    {
        case Shape::Kind::Hulls: return hullsOut(shape, p, e, allowed);
        case Shape::Kind::Surface: return sunk(shape, p, allowed); // (along -y: as far as it is in)
        case Shape::Kind::Distance: return none;
    }
    return none;
}

Slide slide(const Shape& shape, const Probes& probes, const glm::vec3& e, float reach)
{
    return shape.kind == Shape::Kind::Distance ? marched(shape, probes, e, reach) : exact(shape, probes, e);
}

} // namespace qvr::handfit
