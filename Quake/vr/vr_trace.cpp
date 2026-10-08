// vr_trace.cpp -- see vr_trace.hpp.

#include "vr_client.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_engine.hpp"
#include "vr_hull.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Vocabulary/Optional.hpp"

namespace qvr::worldtrace
{

za::Optional<trace_t> move(
    const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& end, int type)
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        return za::nullOpt;
    }

    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);

    vec3_t a{start.x, start.y, start.z};
    vec3_t b{end.x, end.y, end.z};
    vec3_t lo{mins.x, mins.y, mins.z};
    vec3_t hi{maxs.x, maxs.y, maxs.z};
    trace_t tr;
    if(!(type & MOVE_PORTALS) || !VR_PortalReachMove(svs.clients[0].edict, a, lo, hi, b, type & ~MOVE_PORTALS, &tr))
    {
        tr = SV_Move(a, lo, hi, b, type & ~MOVE_PORTALS, svs.clients[0].edict);
    }

    PR_PopQCVM(oldvm);
    return za::makeOptional(tr);
}

namespace
{

// A line through one of a BSP model's hulls (0: a point, 1: the player's box), placed at `origin`
// (not rotated).
trace_t hullTrace(qmodel_t* model, const glm::vec3& origin, const glm::vec3& start, const glm::vec3& end, int hullIndex = 0)
{
    trace_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.f;
    tr.allsolid = true;
    vec3_t a{start.x - origin.x, start.y - origin.y, start.z - origin.z};
    vec3_t b{end.x - origin.x, end.y - origin.y, end.z - origin.z};
    VectorCopy(b, tr.endpos);
    hull_t* hull = &model->hulls[hullIndex];
    ++vr_profcounts.hullchecks;
    SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, a, b, &tr);
    for(int i = 0; i < 3; i++)
    {
        tr.endpos[i] += origin[i];
    }
    return tr;
}

// The client's entities that may be lifts, doors and platforms (a brush model not the world's, in the last message):
// world() looped over every entity for each line (explosion debris: seven a chunk a step). Only a message's entity
// update makes one of these (CL_ParseUpdate: cl_entityupdates), so the list is made again after one (or a new message,
// a map or more entities); each line still tests every condition on those listed, so the same entities pass as in a
// loop over all of them, in the same order. Main thread (as cl_entities).
struct BrushEntities
{
    za::Vector<int> list;
    unsigned int updates = 0;
    double mtime = -1.0;
    int count = -1;
    const qmodel_t* worldModel = nullptr;
};
BrushEntities brushList;

const za::Vector<int>& brushEntityList()
{
    BrushEntities& b = brushList;
    if(b.updates != cl_entityupdates || b.mtime != cl.mtime[0] || b.count != cl.num_entities || b.worldModel != cl.worldmodel)
    {
        b.updates = cl_entityupdates;
        b.mtime = cl.mtime[0];
        b.count = cl.num_entities;
        b.worldModel = cl.worldmodel;
        b.list.clear();
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& e = cl_entities[i];
            if(e.model && e.model->type == mod_brush && e.model != cl.worldmodel && e.msgtime == cl.mtime[0])
            {
                b.list.pushBack(i);
            }
        }
    }
    return b.list;
}

} // namespace

trace_t world(const glm::vec3& start, const glm::vec3& end, bool brushEntities, bool ownFiles, int skipA, int skipB, int* hitEntity)
{
    if(hitEntity)
    {
        *hitEntity = 0;
    }
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
    for(const int i : brushEntityList())
    {
        entity_t& e = cl_entities[i];
        if(i == skipA || i == skipB || !e.model || e.model->type != mod_brush || e.model == cl.worldmodel || e.msgtime != cl.mtime[0])
        {
            continue;
        }
        // (The line's box first: the cheap test that rejects nearly all of them, before the model's metadata lookup.)
        const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
        const glm::vec3 mins = origin + glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]};
        const glm::vec3 maxs = origin + glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]};
        if(glm::any(glm::lessThan(maxs, lo)) || glm::any(glm::greaterThan(mins, hi)) ||
            (!ownFiles && !qvr::modelmeta::isSubmodel(e.model)))
        {
            continue;
        }
        // Not the things lying round (a rigid body: an ammo or health box, an explosive box), nor a brush model drawn
        // scaled or moved off its origin (the props' Size): its hull is the model's own, unscaled and unturned, so it
        // stood where nothing is drawn (a box of shells, drawn 4.8 units across: a 24-unit cube from its middle up and aside), and a
        // prop held over boxes on the floor was pushed up and about by them (wallFrame). Those are pushed in Box3D.
        if(const client::EntityVr* net = client::entityVr(i);
            (e.scale != 0 && e.scale != ENTSCALE_DEFAULT) ||
            (net && (net->noRotate || net->scale != glm::vec3{0.f} || net->offset != glm::vec3{0.f})))
        {
            continue;
        }
        const trace_t t = hullTrace(e.model, origin, start, end);
        if(t.fraction < tr.fraction && !t.startsolid)
        {
            tr = t;
            if(hitEntity)
            {
                *hitEntity = i;
            }
        }
    }
    return tr;
}

bool playerBoxFits(const glm::vec3& start, const glm::vec3& end)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    if(const int narrow = hull::playerBoxFits(cl.worldmodel, start, end); narrow >= 0)
    {
        return narrow != 0; // the player's narrower box (vr_hull_width)
    }
    const trace_t tr = hullTrace(cl.worldmodel, glm::vec3{0.f}, start, end, 1);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1.f;
}

float line(const glm::vec3& start, const glm::vec3& end)
{
    const trace_t tr = world(start, end, false);
    return tr.startsolid ? 0.f : tr.fraction;
}

} // namespace qvr::worldtrace
