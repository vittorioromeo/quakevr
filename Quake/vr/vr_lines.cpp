// vr_lines.cpp -- see vr_lines.hpp.

#include "vr_lines.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"

#include <vector>

namespace qvr::lines
{
namespace
{

struct Line
{
    glm::vec3 a, b;
    float width;
    glm::vec4 colorA, colorB;
    bool point;
    bool additive{false};
};

std::vector<Line> queue;
std::vector<gfx::Vertex> vertices; // uv -1..1 across the width (and along a point): the soft edge

void quad(const glm::vec3 (&p)[4], const glm::vec4 (&c)[4], const glm::vec2 (&uv)[4])
{
    for(int i : {0, 1, 2, 0, 2, 3})
    {
        vertices.push_back({p[i], uv[i], c[i]});
    }
}

} // namespace

void line(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.push_back({a, b, width, colorA, colorB, false});
}

void point(const glm::vec3& p, float size, const glm::vec4& color)
{
    queue.push_back({p, p, size, color, color, true});
}

// Premultiplied blending with no alpha adds the colour.
void glow(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.push_back({a, b, width, {glm::vec3{colorA}, 0.f}, {glm::vec3{colorB}, 0.f}, false, true});
}

void glowPoint(const glm::vec3& p, float size, const glm::vec4& color)
{
    queue.push_back({p, p, size, {glm::vec3{color}, 0.f}, {glm::vec3{color}, 0.f}, true, true});
}

void drawInEye(const glm::vec3& eye)
{
    if(queue.empty())
    {
        return;
    }

    for(const bool additive : {false, true})
    {
    vertices.clear();
    for(const Line& l : queue)
    {
        if(l.additive != additive)
        {
            continue;
        }
        if(l.point)
        {
            // A disc facing the eye.
            const glm::vec3 toEye = glm::normalize(eye - l.a);
            const glm::vec3 side = glm::normalize(glm::cross(toEye, std::fabs(toEye.z) < 0.99f
                                                                        ? glm::vec3{0.f, 0.f, 1.f}
                                                                        : glm::vec3{1.f, 0.f, 0.f}));
            const glm::vec3 up = glm::cross(side, toEye);
            const float r = l.width * 0.5f;
            quad({l.a - side * r - up * r, l.a + side * r - up * r, l.a + side * r + up * r, l.a - side * r + up * r},
                {l.colorA, l.colorA, l.colorA, l.colorA}, {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}});
            continue;
        }

        // A ribbon along the line, turned to face the eye.
        const glm::vec3 dir = l.b - l.a;
        glm::vec3 side = glm::cross(dir, eye - (l.a + l.b) * 0.5f);
        if(glm::length(side) < 1e-4f)
        {
            continue;
        }
        side = glm::normalize(side) * (l.width * 0.5f);
        quad({l.a - side, l.b - side, l.b + side, l.a + side}, {l.colorA, l.colorB, l.colorB, l.colorA},
            {{0.f, -1.f}, {0.f, -1.f}, {0.f, 1.f}, {0.f, 1.f}});
    }

    if(!vertices.empty())
    {
        gfx::draw(vertices, gfx::sceneViewProjection(),
            {.shade = gfx::Shade::SoftEdge, .blend = additive ? gfx::Blend::Premultiplied : gfx::Blend::Alpha,
                .depthTest = false, .depthWrite = false});
    }
    }
}

void clear()
{
    queue.clear();
}

} // namespace qvr::lines
