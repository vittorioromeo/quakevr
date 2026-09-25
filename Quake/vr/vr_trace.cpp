// vr_trace.cpp -- see vr_trace.hpp.

#include "vr_trace.hpp"
#include "vr_engine.hpp"

namespace qvr::worldtrace
{

std::optional<trace_t> move(
    const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& end, int type)
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        return std::nullopt;
    }

    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);

    vec3_t a{start.x, start.y, start.z};
    vec3_t b{end.x, end.y, end.z};
    vec3_t lo{mins.x, mins.y, mins.z};
    vec3_t hi{maxs.x, maxs.y, maxs.z};
    const trace_t tr = SV_Move(a, lo, hi, b, type, svs.clients[0].edict);

    PR_PopQCVM(oldvm);
    return tr;
}

namespace
{

// A line through one BSP model's hull 0, placed at `origin` (not rotated).
trace_t hullTrace(qmodel_t* model, const glm::vec3& origin, const glm::vec3& start, const glm::vec3& end)
{
    trace_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.f;
    tr.allsolid = true;
    vec3_t a{start.x - origin.x, start.y - origin.y, start.z - origin.z};
    vec3_t b{end.x - origin.x, end.y - origin.y, end.z - origin.z};
    VectorCopy(b, tr.endpos);
    hull_t* hull = &model->hulls[0];
    SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, a, b, &tr);
    for(int i = 0; i < 3; i++)
    {
        tr.endpos[i] += origin[i];
    }
    return tr;
}

} // namespace

trace_t world(const glm::vec3& start, const glm::vec3& end, bool brushEntities)
{
    trace_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.f;
    tr.endpos[0] = end.x;
    tr.endpos[1] = end.y;
    tr.endpos[2] = end.z;
    if(!cl.worldmodel)
    {
        return tr;
    }
    tr = hullTrace(cl.worldmodel, glm::vec3{0.f}, start, end);
    if(!brushEntities || tr.startsolid)
    {
        return tr;
    }

    // Lifts, doors and platforms the client has this frame, where the line's box meets theirs.
    const glm::vec3 lo = glm::min(start, end);
    const glm::vec3 hi = glm::max(start, end);
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t& e = cl_entities[i];
        if(!e.model || e.model->type != mod_brush || e.model->name[0] != '*' || e.msgtime != cl.mtime[0])
        {
            continue;
        }
        const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
        const glm::vec3 mins = origin + glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]};
        const glm::vec3 maxs = origin + glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]};
        if(glm::any(glm::lessThan(maxs, lo)) || glm::any(glm::greaterThan(mins, hi)))
        {
            continue;
        }
        const trace_t t = hullTrace(e.model, origin, start, end);
        if(t.fraction < tr.fraction && !t.startsolid)
        {
            tr = t;
        }
    }
    return tr;
}

float line(const glm::vec3& start, const glm::vec3& end)
{
    const trace_t tr = world(start, end, false);
    return tr.startsolid ? 0.f : tr.fraction;
}

} // namespace qvr::worldtrace
