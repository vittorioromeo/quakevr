// vr_shadows.cpp -- soft blob shadows under the player and the hands (vr_player_shadows: 0 off,
// 1 hands, 2 body, 3 both). The old engine projected the models' shadows (r_shadows), which
// Ironwail does not have; a dark disc on the floor below serves the same purpose in VR:
// judging heights when jumping or reaching down. Drawn in the scene pass, depth-tested.
// Needs the local server's world for the traces (see vr_trace).

#include "vr_backend.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_shadows.hpp"
#include "vr_trace.hpp"

#include <vector>

using namespace qvr;

namespace
{

std::vector<gfx::Vertex> vertices; // uv -1..1 across the disc
// A disc on the floor below `from`, fading with height (up to `range` units).
void blob(const glm::vec3& from, float radius, float range, float strength)
{
    const auto tr = worldtrace::move(from, glm::vec3{0.f}, glm::vec3{0.f}, from - glm::vec3{0.f, 0.f, range}, MOVE_NOMONSTERS);
    if(!tr || tr->fraction >= 1.f || tr->startsolid)
    {
        return;
    }

    const glm::vec3 n = worldtrace::normal(*tr);
    if(n.z < 0.7f)
    {
        return; // not a floor
    }

    const float alpha = strength * (1.f - tr->fraction);
    const glm::vec3 centre = worldtrace::endPos(*tr) + n * 0.25f;
    const glm::vec3 u = glm::normalize(glm::cross(n, glm::vec3{0.f, 1.f, 0.f} + n * 0.001f)) * radius;
    const glm::vec3 v = glm::cross(n, u);

    const glm::vec4 color{0.f, 0.f, 0.f, alpha};
    const gfx::Vertex c[4] = {{centre - u - v, {-1.f, -1.f}, color}, {centre + u - v, {1.f, -1.f}, color},
        {centre + u + v, {1.f, 1.f}, color}, {centre - u + v, {-1.f, 1.f}, color}};
    for(int i : {0, 1, 2, 0, 2, 3})
    {
        vertices.push_back(c[i]);
    }
}

} // namespace

// From VR_DrawSceneOpaque (vr_text3d.cpp).
void shadows::draw()
{
    const int mode = static_cast<int>(vr_player_shadows.value);
    const hands::State& s = hands::current();
    if(mode <= 0 || !vrActive() || !s.valid)
    {
        return;
    }

    vertices.clear();
    if(mode == 2 || mode == 3)
    {
        blob(s.playerOrigin, 14.f, 256.f, 0.65f);
    }
    if(mode == 1 || mode == 3)
    {
        for(const glm::vec3& hand : s.pos)
        {
            blob(hand, 3.5f, 96.f, 0.7f);
        }
    }

    if(vertices.empty())
    {
        return;
    }

    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = true, .depthWrite = false});
}
