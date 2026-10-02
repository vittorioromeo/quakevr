// vr_lines.cpp -- see vr_lines.hpp.

#include "vr_lines.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_profile.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"


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
    bool scene{false}; // depth-tested, in the scene's translucent pass (drawInScene)
};

za::Vector<Line> queue;

// Blended over the scene [0], added onto it [1]; uv -1..1 across the width (and along a point):
// the soft edge.
za::Vector<gfx::Vertex> vertices[2];

void quad(za::Vector<gfx::Vertex>& out, const glm::vec3 (&p)[4], const glm::vec4 (&c)[4], const glm::vec2 (&uv)[4])
{
    for(int i : {0, 1, 2, 0, 2, 3})
    {
        out.pushBack({p[i], uv[i], c[i]});
    }
}

} // namespace

void line(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.pushBack({a, b, width, colorA, colorB, false});
}

void point(const glm::vec3& p, float size, const glm::vec4& color)
{
    queue.pushBack({p, p, size, color, color, true});
}

// Premultiplied blending with no alpha adds the colour.
void glow(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.pushBack({a, b, width, {glm::vec3{colorA}, 0.f}, {glm::vec3{colorB}, 0.f}, false, true});
}

void glowPoint(const glm::vec3& p, float size, const glm::vec4& color)
{
    queue.pushBack({p, p, size, {glm::vec3{color}, 0.f}, {glm::vec3{color}, 0.f}, true, true});
}

void sceneLine(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.pushBack({a, b, width, colorA, colorB, false, false, true});
}

void sceneGlow(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& colorA, const glm::vec4& colorB)
{
    queue.pushBack({a, b, width, {glm::vec3{colorA}, 0.f}, {glm::vec3{colorB}, 0.f}, false, true, true});
}

namespace
{

// The queue's lines of the scene's (or not) into `vertices`, facing `eye`, and drawn: depth-tested in the scene.
void drawQueued(const glm::vec3& eye, bool scene)
{
    vertices[0].clear();
    vertices[1].clear();
    for(const Line& l : queue)
    {
        if(l.scene != scene)
        {
            continue;
        }
        za::Vector<gfx::Vertex>& out = vertices[l.additive];
        if(l.point)
        {
            // A disc facing the eye.
            const glm::vec3 toEye = glm::normalize(eye - l.a);
            const glm::vec3 side = glm::normalize(glm::cross(toEye, za::fabs(toEye.z) < 0.99f
                                                                        ? glm::vec3{0.f, 0.f, 1.f}
                                                                        : glm::vec3{1.f, 0.f, 0.f}));
            const glm::vec3 up = glm::cross(side, toEye);
            const float r = l.width * 0.5f;
            quad(out,
                {l.a - side * r - up * r, l.a + side * r - up * r, l.a + side * r + up * r, l.a - side * r + up * r},
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
        quad(out, {l.a - side, l.b - side, l.b + side, l.a + side}, {l.colorA, l.colorB, l.colorB, l.colorA},
            {{0.f, -1.f}, {0.f, -1.f}, {0.f, 1.f}, {0.f, 1.f}});
    }

    if(vertices[0].empty() && vertices[1].empty())
    {
        return;
    }
    const glm::mat4 viewProjection = gfx::sceneViewProjection();
    if(!vertices[0].empty())
    {
        gfx::draw(vertices[0], viewProjection,
            {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = scene, .depthWrite = false});
    }
    if(!vertices[1].empty())
    {
        gfx::draw(vertices[1], viewProjection,
            {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Premultiplied, .depthTest = scene, .depthWrite = false});
    }
}

} // namespace

void drawInEye(const glm::vec3& eye)
{
    QVR_GPU_PROFILE("lines");
    if(queue.empty())
    {
        return;
    }
    drawQueued(eye, false);
}

void drawInScene()
{
    if(queue.empty())
    {
        return;
    }
    QVR_GPU_PROFILE("scene lines");
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    drawQueued(eye, true);
}

void clear()
{
    queue.clear();
}

} // namespace qvr::lines
