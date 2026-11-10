// vr_ledgeshow.cpp -- Debug > Show Ledge Shove Zones (vr_knockdown_ledge_show): see vr_ledgeshow.hpp.

#include "vr_ledgeshow.hpp"

#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_lines.hpp"
#include "vr_progs.hpp"
#include "vr_trace.hpp"

#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

namespace qvr::ledgeshow
{
namespace
{

constexpr float kStep = 4.f;      // units between the points the floor is looked for at
constexpr float kLength = 512.f;  // units: the line's longest
constexpr int kPoints = static_cast<int>(kLength / kStep) + 1;
constexpr float kRise = 12.f;     // units over the last floor the way is traced at (a step up higher: a wall)
constexpr float kLift = 1.5f;     // units the line is drawn over the floor

float lastEdge = -2.f; // the edge's distance last printed (-1 none): a line in the console when it changes

} // namespace

void draw(float yaw)
{
    edict_t* player = progs::playerEdict();
    if(vr_knockdown_ledge_show.value == 0.f || !player)
    {
        lastEdge = -2.f;
        return;
    }
    const progs::ServerVm vm; // (the traces need the server's VM)
    const float drop = vr_knockdown_ledge_drop.value;
    const float topple = za::max(vr_knockdown_ledge_topple.value, 0.f);
    const float reach = za::max(vr_knockdown_ledge_backpedal_reach.value, topple);
    const glm::vec3 dir{za::cos(glm::radians(yaw)), za::sin(glm::radians(yaw)), 0.f};
    glm::vec3 at = geom::vec(player->v.origin);
    at.z += player->v.mins[2]; // (the feet)
    glm::vec3 points[kPoints];
    int n = 0;
    points[n++] = at;
    float edge = -1.f;
    for(float d = kStep; d <= kLength && drop > 0.f; d += kStep)
    {
        const glm::vec3 from{at.x, at.y, at.z + kRise};
        const glm::vec3 to = glm::vec3{at.x, at.y, at.z} + dir * kStep + glm::vec3{0.f, 0.f, kRise};
        if(worldtrace::serverLine(from, to, player).fraction < 1.f)
        {
            break; // a wall, or a step up: no ledge this way
        }
        const trace_t down = worldtrace::serverLine(to, glm::vec3{to.x, to.y, at.z - drop}, player);
        if(down.fraction >= 1.f && !down.startsolid)
        {
            edge = d - kStep * 0.5f; // (between the last floor and the first none)
            points[n++] = glm::vec3{to.x, to.y, at.z} - dir * (kStep * 0.5f);
            break;
        }
        at = worldtrace::endPos(down);
        points[n++] = at;
    }
    if(edge != lastEdge)
    {
        lastEdge = edge;
        if(edge >= 0.f)
        {
            Con_Printf("ledgeshow: edge %.0f units ahead: topple from %.0f on (within %.0f of it), backpedal from %.0f on "
                       "(within %.0f)\n",
                edge, za::max(edge - topple, 0.f), topple, za::max(edge - reach, 0.f), reach);
        }
        else
        {
            Con_Printf("ledgeshow: no ledge ahead\n");
        }
    }
    const glm::vec4 grey{0.6f, 0.6f, 0.6f, 0.7f}, yellow{1.f, 0.85f, 0.1f, 0.95f}, red{1.f, 0.12f, 0.08f, 0.95f},
        white{1.f, 1.f, 1.f, 1.f};
    const glm::vec3 lift{0.f, 0.f, kLift};
    float along = 0.f;
    for(int i = 1; i < n; i++)
    {
        const float mid = along + kStep * 0.5f;
        along += kStep;
        const float left = edge >= 0.f ? edge - za::min(mid, edge) : -1.f; // (to the edge)
        const glm::vec4& c = left < 0.f ? grey : left <= topple ? red : left <= reach ? yellow : grey;
        lines::sceneLine(points[i - 1] + lift, points[i] + lift, 1.5f, c, c);
    }
    if(edge >= 0.f && n > 0)
    {
        const glm::vec3 across{-dir.y, dir.x, 0.f};
        lines::sceneLine(points[n - 1] + lift - across * 8.f, points[n - 1] + lift + across * 8.f, 1.5f, white, white);
    }
}

} // namespace qvr::ledgeshow
