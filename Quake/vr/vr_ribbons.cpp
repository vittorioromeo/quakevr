// vr_ribbons.cpp -- see vr_ribbons.hpp.

#include "vr_ribbons.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_profile.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::ribbons
{
namespace
{

constexpr int kMaxSmoothing = 8;

// The Catmull-Rom curve through b and c (a before, d after) at t (0 at b, 1 at c).
[[nodiscard]] glm::vec3 catmullRom(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, float t)
{
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.f * b) + (c - a) * t + (2.f * a - 5.f * b + 4.f * c - d) * t2 + (3.f * b - a - 3.f * c + d) * t3);
}

// A place along the ribbon (a point, or one between two).
struct Sample
{
    glm::vec3 pos;
    float halfWidth;
    glm::vec4 color;
};

} // namespace

View sceneView(float minPixels)
{
    View v;
    glm::vec3 right, up;
    gfx::sceneCamera(v.eye, right, up);
    int viewport[4];
    R_SceneViewport(viewport);
    // (Quake's projection, its rows swapped about: 1 / tan(fov y / 2) at [2][1]; R_DrawParticles reads it so too.)
    v.pixelsPerUnit = za::abs(r_matproj[2 * 4 + 1]) * 0.5f * static_cast<float>(viewport[3]);
    v.minPixels = minPixels;
    return v;
}

int append(za::Vector<gfx::Vertex>& out, const View& view, za::Span<const Point> points, int smoothing)
{
    const int n = static_cast<int>(points.size());
    if(n < 2)
    {
        return 0;
    }
    const int k = za::clamp(smoothing, 1, kMaxSmoothing);
    const za::SizeT before = out.size();
    glm::vec3 lastSide{0.f, 0.f, 1.f};
    gfx::Vertex prevA{}, prevB{};
    bool havePrev = false;
    glm::vec3 prevPos{0.f};
    // Each sample in turn: its two corners across the ribbon, joined to the last sample's.
    const auto put = [&](const Sample& s, const glm::vec3& tangent) {
        const glm::vec3 toEye = view.eye - s.pos;
        const float distance = glm::length(toEye);
        glm::vec3 side = glm::cross(tangent, toEye);
        const float l = glm::length(side);
        side = l > 1e-6f ? side / l : lastSide;
        lastSide = side;
        // No thinner than minPixels on screen: wider, its alpha lowered as much (the same light over the pixels).
        float hw = s.halfWidth;
        float alpha = s.color.a * glm::smoothstep(view.nearFade * 0.33f, view.nearFade, distance);
        const float pixels = 2.f * hw * view.pixelsPerUnit / za::max(distance, 0.01f);
        if(pixels < view.minPixels && view.pixelsPerUnit > 0.f)
        {
            alpha *= za::max(pixels, 0.f) / view.minPixels;
            hw = 0.5f * view.minPixels * za::max(distance, 0.01f) / view.pixelsPerUnit;
        }
        const glm::vec4 c{s.color.r, s.color.g, s.color.b, alpha};
        const gfx::Vertex a{s.pos - side * hw, glm::vec2{-1.f, 0.f}, c};
        const gfx::Vertex b{s.pos + side * hw, glm::vec2{1.f, 0.f}, c};
        if(havePrev && (prevA.color.a > 0.f || alpha > 0.f))
        {
            out.pushBack(prevA);
            out.pushBack(prevB);
            out.pushBack(b);
            out.pushBack(prevA);
            out.pushBack(b);
            out.pushBack(a);
        }
        prevA = a;
        prevB = b;
        havePrev = true;
    };
    const auto pointAt = [&](int i) -> const glm::vec3& { return points[static_cast<za::SizeT>(za::clamp(i, 0, n - 1))].pos; };
    for(int i = 0; i < n - 1; i++)
    {
        const Point& p = points[static_cast<za::SizeT>(i)];
        const Point& q = points[static_cast<za::SizeT>(i + 1)];
        const glm::vec3& a = pointAt(i - 1);
        const glm::vec3& d = pointAt(i + 2);
        for(int j = (i == 0 ? 0 : 1); j <= k; j++)
        {
            const float t = static_cast<float>(j) / static_cast<float>(k);
            Sample s;
            s.pos = k > 1 ? catmullRom(a, p.pos, q.pos, d, t) : glm::mix(p.pos, q.pos, t);
            s.halfWidth = p.halfWidth + (q.halfWidth - p.halfWidth) * t;
            s.color = glm::mix(p.color, q.color, t);
            // Its way along: towards the next sample (the last: from the one before).
            const float tn = za::min(1.f, t + 1.f / static_cast<float>(k));
            glm::vec3 tangent = (k > 1 ? catmullRom(a, p.pos, q.pos, d, tn) : glm::mix(p.pos, q.pos, tn)) - s.pos;
            if(glm::dot(tangent, tangent) < 1e-10f)
            {
                tangent = havePrev ? s.pos - prevPos : q.pos - p.pos;
            }
            put(s, tangent);
            prevPos = s.pos;
        }
    }
    return static_cast<int>(out.size() - before);
}

void draw(za::Span<const gfx::Vertex> triangles, bool additive)
{
    if(triangles.empty())
    {
        return;
    }
    // Each pixel drawn once (State::once): where a ribbon folds over itself on a sharp turn, or two cross, the smoke is
    // no denser (it was: creases); vr_debug_ribbon_overlap 1 blends them again, to compare.
    gfx::draw(triangles, gfx::sceneViewProjection(),
        gfx::State{.shade = gfx::Shade::SoftEdge, .blend = additive ? gfx::Blend::Additive : gfx::Blend::Alpha, .depthTest = true,
            .depthWrite = false, .once = vr_debug_ribbon_overlap.value == 0.f});
}

} // namespace qvr::ribbons
