// vr_shadows.cpp -- soft blob shadows under the player, the hands (vr_player_shadows: 0 off,
// 1 hands, 2 body, 3 both) and the monsters and items (vr_entity_shadows). The old engine projected
// the models' shadows (r_shadows), which Ironwail does not have; a dark disc on the floor below
// serves the same purpose in VR: judging heights when jumping or reaching down, and seeing that
// things stand on the ground. A model's disc is pushed away from the light it is shaded from
// (vr_modellight). Built once per frame, drawn in each eye's scene pass, depth-tested.

#include "vr_backend.hpp"
#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_modellight.hpp"
#include "vr_shadows.hpp"
#include "vr_trace.hpp"

#include <algorithm>
#include <vector>

using namespace qvr;

extern "C" int VR_IsViewEntity(const entity_t* e);

namespace
{

std::vector<gfx::Vertex> vertices; // uv -1..1 across the disc
int builtFrame = -1;

// A disc on the floor below `from`, fading with the height above it beyond `lift` (gone at
// `range` units), moved by `shift`.
void blob(const glm::vec3& from, float radius, float range, float strength, const glm::vec3& shift = glm::vec3{0.f},
    float lift = 0.f)
{
    const trace_t tr = worldtrace::world(from, from - glm::vec3{0.f, 0.f, range + lift});
    if(tr.fraction >= 1.f || tr.startsolid)
    {
        return;
    }

    const glm::vec3 n = worldtrace::normal(tr);
    if(n.z < 0.7f)
    {
        return; // not a floor
    }

    const float height = std::max(0.f, tr.fraction * (range + lift) - lift);
    const float alpha = strength * std::max(0.f, 1.f - height / range);
    const glm::vec3 centre = worldtrace::endPos(tr) + n * 0.25f + shift;
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

// Monsters and items: alias models in the scene, not the player's own, not the flames and beams
// Quake marks as casting no shadow, not see-through ones.
void entityBlobs()
{
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        // Alias models, and the brush models of ammo and health boxes (maps/b_*.bsp).
        const bool itemBox = e->model && e->model->type == mod_brush && e->model->name[0] != '*' && e->model != cl.worldmodel;
        if(!e->model || (e->model->type != mod_alias && !itemBox) || (e->model->flags & MOD_NOSHADOW) ||
            e == &cl_entities[cl.viewentity] || VR_IsViewEntity(e) || e->alpha != ENTALPHA_DEFAULT)
        {
            continue;
        }

        const float scale = ENTSCALE_DECODE(e->scale);
        const glm::vec3 mins{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
        const glm::vec3 maxs{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
        const float width = std::max(maxs.x - mins.x, maxs.y - mins.y) * scale;
        if(width < 4.f)
        {
            continue; // nails, gibs' bits
        }
        const float radius = std::clamp(width * 0.45f, 4.f, 40.f);

        // Away from the light, the more so the more directional it is.
        const glm::vec4 light = modellight::direction(e);
        const glm::vec3 shift = glm::vec3{-light.x, -light.y, 0.f} * (radius * 0.4f * light.w);

        // From the origin: a monster's is its middle, an item's its base (the model's own bounds
        // may reach below the floor).
        const glm::vec3 from{e->origin[0], e->origin[1], e->origin[2] + 8.f};
        blob(from, radius, 128.f, 0.6f, shift, 8.f + std::max(0.f, -mins.z * scale));
    }
}

void build()
{
    vertices.clear();
    const hands::State& s = hands::current();
    const int mode = static_cast<int>(vr_player_shadows.value);
    if(s.valid && (mode == 2 || mode == 3))
    {
        blob(s.playerOrigin, 14.f, 256.f, 0.65f);
    }
    if(s.valid && (mode == 1 || mode == 3))
    {
        for(const glm::vec3& hand : s.pos)
        {
            blob(hand, 3.5f, 96.f, 0.7f);
        }
    }
    if(vr_entity_shadows.value)
    {
        entityBlobs();
    }
}

} // namespace

// From VR_DrawSceneOpaque (vr_text3d.cpp).
void shadows::draw()
{
    if(!vrActive() || !cl.worldmodel)
    {
        return;
    }

    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;
        build();
    }
    if(vertices.empty())
    {
        return;
    }

    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = true, .depthWrite = false});
}
