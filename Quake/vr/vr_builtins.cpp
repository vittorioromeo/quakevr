// vr_builtins.cpp -- Quake VR QuakeC builtins.
//
// The QC declares these as `= #0`; after Ironwail's own by-name binding they are still
// unbound, so they are given numbers from a private range here and bound by name.

#include "vr_progs.hpp"
#include "vr_brushes.hpp"
#include "vr_goresound.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/Unique.hpp"
#include "vr_melee_shared.h"
#include "vr_agarmor.hpp"
#include "vr_box3d.hpp"
#include "vr_toolgun.hpp"
#include "vr_carry2h.hpp"
#include "vr_crates.hpp"
#include "vr_debris.hpp"
#include "vr_foegrab.hpp"
#include "vr_mgroups.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_highlights.hpp"
#include "vr_hitmodel.hpp"
#include "vr_hitreact.hpp"
#include "vr_motion.hpp"
#include "vr_mem.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_portals.hpp"
#include "vr_props.hpp"
#include "vr_propthink.hpp"
#include "vr_protocol.hpp"
#include "vr_ropesim.hpp"
#include "vr_body.hpp"
#include "vr_bodycontact.hpp"
#include "vr_magholster.hpp"
#include "vr_collectfx.hpp"
#include "vr_server.hpp"
#include "vr_shock.hpp"
#include "vr_stealth.hpp"
#include "vr_selfcollide.hpp"
#include "vr_twohand.hpp"
#include "vr_cvars.hpp"
#include "vr_tips.hpp"
#include "vr_worldtext.hpp"
#include "vr_view.hpp"
#include "vr_admelee.hpp"
#include "vr_aggrip.hpp"
#include "vr_agpose.hpp"
#include "vr_poselive.hpp"
#include "vr_posestudio.hpp"
#include "vr_rigstudio.hpp"
#include "vr_adtank.hpp" // (hitmodel_segment's class -1: a tank's hose)
#include "vr_blade.hpp"
#include "vr_meleeshow.hpp"
#include "vr_weapons.hpp"
#include "vr_weight.hpp"
#include "vr_walltorch.hpp"
#include "vr_geom.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::progs
{
namespace
{

// Ironwail numbers its unnumbered builtins downwards from MAX_BUILTINS - 2 (none today: its highest numbered one is
// #627). The VR ones from 800 (1000, then 900, until they filled the slots below the 200 kept for Ironwail's).
constexpr int firstVrBuiltin = 800;

// ----------------------------------------------------------------------------
// Math and vectors

void PF_makeforward()
{
    vec3_t right, up;
    AngleVectors(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, right, up);
}

// The bounds of an entity's model (its mins with `max` 0, maxs otherwise), in model space:
// vector(entity e, float max) modelbounds.
void PF_modelbounds()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const bool max = G_FLOAT(OFS_PARM1) != 0.f;
    float* out = G_VECTOR(OFS_RETURN);
    VectorCopy(vec3_origin, out);

    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(model)
    {
        VectorCopy(max ? model->maxs : model->mins, out);
    }
}

// An entity's drawn box (its model as drawn: the weapon models' own scale and offset, a prop's Size, its model_scale
// about model_scale_origin and model_offset), not turned, about its origin: its lo (`max` 0) or hi corner:
// vector(entity e, float max) drawnbounds. A weapon model's raw bounds (modelbounds) are not where it is drawn.
void PF_drawnbounds()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const bool max = G_FLOAT(OFS_PARM1) != 0.f;
    float* out = G_VECTOR(OFS_RETURN);
    VectorCopy(vec3_origin, out);

    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model)
    {
        return;
    }
    const FieldOffsets& f = fields();
    glm::vec3 lo, hi;
    held::modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);
    const glm::vec3& v = max ? hi : lo;
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// The model_offset that puts the middle of an entity's drawn box (drawnbounds, with its model_scale and
// model_scale_origin; its model_offset ignored) at `at` (about its origin, not turned): vector(entity e, vector at)
// modeloffsetto. An alias model's model_offset is in its stored vertices' units (before the weapon models' and its
// own scale), so the drawn shift it gives is measured (it is linear in it).
void PF_modeloffsetto()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const float* a = G_VECTOR(OFS_PARM1);
    float* out = G_VECTOR(OFS_RETURN);
    VectorCopy(vec3_origin, out);

    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model)
    {
        return;
    }
    const FieldOffsets& f = fields();
    const glm::vec3 scale = fieldVec(ent, f.model_scale), scaleOrigin = fieldVec(ent, f.model_scale_origin);
    glm::vec3 lo0, hi0, lo1, hi1;
    held::modelBox(model, scale, scaleOrigin, glm::vec3{0.f}, lo0, hi0);
    held::modelBox(model, scale, scaleOrigin, glm::vec3{1.f}, lo1, hi1);
    const glm::vec3 c0 = (lo0 + hi0) * 0.5f;
    const glm::vec3 perUnit = (lo1 + hi1) * 0.5f - c0; // the drawn shift of a unit of offset, an axis
    for(int i = 0; i < 3; i++)
    {
        out[i] = fabsf(perUnit[i]) > 1e-6f ? (a[i] - c0[i]) / perUnit[i] : 0.f;
    }
}

// The middle of an entity's drawn model, turned with it, in the world (vr_rigid.cpp): vector(entity
// e) modelcentre. Dropped weapons and backpacks are drawn well off their box's middle.
void PF_modelcentre()
{
    const glm::vec3 c = physics::modelCentre(G_EDICT(OFS_PARM0));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = c.x;
    out[1] = c.y;
    out[2] = c.z;
}

// float(entity e, float ssgOpen) loadportof: the loading port of the gun lying about as `e` (its model's:
// view::modelLoadPort, the super shotgun's barrels turned down `ssgOpen` degrees), as it is drawn where it lies (its
// drawn transform, angles and origin), into its .loadportpos, .loadportaxis, .loadportface; FALSE (and them zero) if its
// model has none (vr_reload.qc VR_Reload_PropsFrame: a gun lying about loads as a held one).
void PF_loadportof()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const float open = G_FLOAT(OFS_PARM1);
    const FieldOffsets& f = fields();
    G_FLOAT(OFS_RETURN) = 0.f;
    if(f.loadportpos < 0 || f.loadportaxis < 0 || f.loadportface < 0)
    {
        return;
    }
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    glm::vec3 at{0.f}, axis{0.f}, face{0.f};
    if(!view::modelLoadPort(model, open, at, axis, face))
    {
        setFieldVec(ent, f.loadportpos, glm::vec3{0.f});
        setFieldVec(ent, f.loadportaxis, glm::vec3{0.f});
        setFieldVec(ent, f.loadportface, glm::vec3{0.f});
        return;
    }
    const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 p = held::drawnModelPoint(ent, at);
    const auto way = [&](const glm::vec3& d) {
        const glm::vec3 w = axes * (held::drawnModelPoint(ent, at + d) - p);
        return glm::length(w) > 1e-4f ? glm::normalize(w) : glm::vec3{0.f};
    };
    setFieldVec(ent, f.loadportpos, origin + axes * p);
    setFieldVec(ent, f.loadportaxis, way(axis));
    setFieldVec(ent, f.loadportface, way(face));
    G_FLOAT(OFS_RETURN) = 1.f;
}

// vector(entity e, vector a, vector b) shapenearest: the point of `e`'s shape (its Box3D body's, as it lies or a hand
// holds it: its hull, a gun's convex pieces) nearest the segment `a`..`b`; without a body, its box's (origin + mins ..
// maxs) nearest, tried at nine points along the segment (vr_reload.qc: a held prop hits a magazine or the super
// shotgun's barrels with its surface, not its box).
void PF_shapenearest()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const glm::vec3 a = progs::vecParm(OFS_PARM1);
    const glm::vec3 b = progs::vecParm(OFS_PARM2);
    glm::vec3 best{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    if(!box3d::shapeNearest(NUM_FOR_EDICT(ent), a, b, best))
    {
        const glm::vec3 lo = best + geom::vec(ent->v.mins);
        const glm::vec3 hi = best + geom::vec(ent->v.maxs);
        float bestDist = 1e9f;
        for(int i = 0; i <= 8; i++)
        {
            const glm::vec3 p = a + (b - a) * (static_cast<float>(i) / 8.f);
            const glm::vec3 q = glm::clamp(p, lo, hi);
            if(glm::distance(p, q) < bestDist)
            {
                bestDist = glm::distance(p, q);
                best = q;
            }
        }
    }
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = best.x;
    out[1] = best.y;
    out[2] = best.z;
}

// entity(vector org, float rad, vector dir, float mincos) findcone: findradius's chain (the same entities, in the same
// order: the last first), less those that have no model or whose drawn middle (modelcentre) is out of the cone from
// `org` along `dir` (a unit vector) whose cosine is `mincos`, or within a unit of `org`. A little wider than that cone:
// the caller tests each again as before (QC VR_Forcegrab_FindTarget: everything within reach of a hand, every frame;
// a pile of props in reach was hundreds of them through QC).
void PF_findcone()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float rad = G_FLOAT(OFS_PARM1) * G_FLOAT(OFS_PARM1);
    const glm::vec3 from{org[0], org[1], org[2]};
    const float* d = G_VECTOR(OFS_PARM2);
    const glm::vec3 dir{d[0], d[1], d[2]};
    const float minCos = G_FLOAT(OFS_PARM3) - 1e-4f;
    edict_t* chain = qcvm->edicts;
    edict_t* ent = NEXT_EDICT(qcvm->edicts);
    for(int i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
    {
        if(ent->free || static_cast<int>(ent->v.solid) == SOLID_NOT)
        {
            continue;
        }
        // (findradius's test, in its order and precision)
        float x = org[0] - (ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5);
        float lensq = x * x;
        if(lensq > rad)
        {
            continue;
        }
        x = org[1] - (ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5);
        lensq += x * x;
        if(lensq > rad)
        {
            continue;
        }
        x = org[2] - (ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5);
        lensq += x * x;
        if(lensq > rad)
        {
            continue;
        }
        if(!ent->v.model || !*PR_GetString(ent->v.model))
        {
            continue;
        }
        const glm::vec3 to = physics::modelCentre(ent) - from;
        const float dist = glm::length(to);
        if(dist < 0.999f || glm::dot(to, dir) < minCos * dist)
        {
            continue;
        }
        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(chain);
}

struct PullSearchScratch
{
    za::Vector<glm::vec3> origins;
    za::Vector<edict_t*> found; // the area grid's edicts near the origins
    auto members() { return mem::list(origins, found); }
};
mem::Scratch<PullSearchScratch> pullSearch{"force-grab search"};

// The force grab search's counts (vr_forcegrab_grid_stats): searches and edicts tested (by the grid; by the walk for a
// range the grid can't take).
struct ForcegrabGridStats
{
    long long searches = 0, tested = 0, walked = 0;
};
ForcegrabGridStats forcegrabGridStats;

// How far an edict's drawn middle (physics::modelCentre) may lie outside its linked box for the area grid to find it:
// a knocked-down ogre's lies 47 units out (the most measured: secret2, map2's kill-all, combined, combat_48), a weapon
// or a backpack's 6-12. The edicts found are then tested exactly as the walk tests every edict.
constexpr float forcegrabGridSlack = 128.f;

// Exact live model centres, rather than Quake collision bounds (a dropped gun's
// centre can be well outside them). Destination-room searches are conservative:
// aperture, occlusion and closest-image selection remain pullImage's responsibility.
[[nodiscard]] bool nearPullSearch(const glm::vec3& point, float range)
{
    if(!(range >= 0.f)) { return true; } // preserve the original comparisons for unusual QC inputs
    if(!(point.x - point.x == 0.f && point.y - point.y == 0.f && point.z - point.z == 0.f)) { return true; } // (a NaN centre: the original comparisons decide, as before)
    for(const glm::vec3& origin : pullSearch.origins)
    {
        // Account for float cancellation and the portal rotation's rounding. The
        // final original distance/cone tests still decide every candidate.
        const float margin = 1e-4f * (1.f + glm::dot(glm::abs(origin) + glm::abs(point), glm::vec3{1.f}));
        if(glm::all(glm::lessThanEqual(glm::abs(point - origin), glm::vec3{range + margin}))) { return true; }
    }
    return false;
}

// findportalcone's test of one edict (the walk's and the area grid's alike): in the cone, within range, near one of
// the search's origins (the hand, its images through teleporters).
[[nodiscard]] bool inPortalCone(edict_t* ent, const glm::vec3& from, float range, const glm::vec3& aim, float minCos)
{
    if(ent->free || static_cast<int>(ent->v.solid) == SOLID_NOT || !ent->v.model) { return false; }
    const glm::vec3 centre = physics::modelCentre(ent);
    if(!nearPullSearch(centre, range)) { return false; }
    const glm::vec3 to = portals::pullImage(from, centre) - from;
    const float distance = glm::length(to);
    return !(distance < 0.999f || distance > range || glm::dot(to, aim) < minCos * distance);
}

// The edicts the area grid links near the search's origins (every edict that can pass is linked:
// only SOLID_NOT isn't, as VR_TouchLinks takes it), each once, in edict order (the walk's order, so the same chain).
// A box round each origin: the range, nearPullSearch's margin and forcegrabGridSlack.
void gatherPortalConeCandidates(float range)
{
    za::Vector<edict_t*>& found = pullSearch.found;
    found.clear();
    const int space = qcvm->num_edicts;
    for(const glm::vec3& origin : pullSearch.origins)
    {
        const float margin = 1e-4f * (1.f + 2.f * glm::dot(glm::abs(origin), glm::vec3{1.f})) + 1.f;
        const float reach = range + margin + forcegrabGridSlack;
        const float lo[3] = {origin.x - reach, origin.y - reach, origin.z - reach};
        const float hi[3] = {origin.x + reach, origin.y + reach, origin.z + reach};
        const za::SizeT before = found.size();
        found.resize(before + static_cast<za::SizeT>(space));
        int count = 0;
        SV_AreaEdictsUnordered(lo, hi, found.data() + before, &count, space);
        found.resize(before + static_cast<za::SizeT>(count));
    }
    za::quickSort(found.data(), found.data() + found.size()); // edict order (the edicts are one array), each once
    found.resize(static_cast<za::SizeT>(za::unique(found.data(), found.data() + found.size()) - found.data()));
}

void PF_findportalcone()
{
    const float* p = G_VECTOR(OFS_PARM0);
    const glm::vec3 from{p[0], p[1], p[2]};
    const float range = G_FLOAT(OFS_PARM1);
    const float* d = G_VECTOR(OFS_PARM2);
    const glm::vec3 aim{d[0], d[1], d[2]};
    const float minCos = G_FLOAT(OFS_PARM3) - 1e-4f;
    portals::pullSearchOrigins(from, pullSearch.origins, range);
    edict_t* chain = qcvm->edicts;
    // (A range that isn't a number or is below 0: nearPullSearch lets everything through; the walk, as before.)
    // The grid's chain was checked against the walk's (vr_forcegrab_grid_verify, removed 2026-10-09): 0 differences
    // in 14,186 searches (PERF_DECISIONS.md 11); vr_prop_query_verify still compares it with the full scan.
    const bool grid = range >= 0.f && range < 1e6f;
    forcegrabGridStats.searches++;
    if(grid)
    {
        gatherPortalConeCandidates(range);
        for(edict_t* ent : pullSearch.found)
        {
            forcegrabGridStats.tested++;
            if(ent == qcvm->edicts || !inPortalCone(ent, from, range, aim, minCos)) { continue; }
            ent->v.chain = EDICT_TO_PROG(chain);
            chain = ent;
        }
    }
    else
    {
        edict_t* ent = NEXT_EDICT(qcvm->edicts);
        for(int i = 1; i < qcvm->num_edicts; ++i, ent = NEXT_EDICT(ent))
        {
            forcegrabGridStats.walked++;
            if(!inPortalCone(ent, from, range, aim, minCos)) { continue; }
            ent->v.chain = EDICT_TO_PROG(chain);
            chain = ent;
        }
    }
    if(vr_prop_query_verify.value)
    {
        // Re-run the original full scan without the broad phase. Compare the exact
        // chain (including order), without writing chain fields or calling QC.
        edict_t* expected = qcvm->edicts;
        edict_t* ent = NEXT_EDICT(qcvm->edicts);
        for(int i = 1; i < qcvm->num_edicts; ++i, ent = NEXT_EDICT(ent))
        {
            if(ent->free || static_cast<int>(ent->v.solid) == SOLID_NOT || !ent->v.model) { continue; }
            const glm::vec3 to = portals::pullImage(from, physics::modelCentre(ent)) - from;
            const float distance = glm::length(to);
            if(distance < 0.999f || distance > range || glm::dot(to, aim) < minCos * distance) { continue; }
            if(ent->v.chain != EDICT_TO_PROG(expected)) { Sys_Error("force-grab broad phase changed chain at %d", i); }
            expected = ent;
        }
        if(chain != expected) { Sys_Error("force-grab broad phase changed chain head"); }
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(chain);
}

void PF_portal_pullimage()
{
    const float* a = G_VECTOR(OFS_PARM0), *b = G_VECTOR(OFS_PARM1);
    const glm::vec3 result = portals::pullImage({a[0], a[1], a[2]}, {b[0], b[1], b[2]});
    progs::returnVec(result);
}

extern "C" void VR_PortalPullTarget(edict_t* ent, const float hand[3], int begin, float out[3]);
void PF_portal_pulltarget()
{
    VR_PortalPullTarget(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), G_FLOAT(OFS_PARM2) != 0.f, G_VECTOR(OFS_RETURN));
}

// Where the force grab takes an entity, in the world (vector(entity e) forcegrabpoint): a weapon by its handle, its
// origin (weapons::heldAtOrigin: it arrives in the hand as it is held), anything else by its drawn middle.
void PF_forcegrabpoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const glm::vec3 c = weapons::heldAtOrigin(model) ? geom::vec(e->v.origin)
                                                    : physics::modelCentre(e);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = c.x;
    out[1] = c.y;
    out[2] = c.z;
}

// void(vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity forent) tracebox:
// traceline with a box (the engine's own collision, hulls and all); sets the trace_ globals.
void PF_tracebox()
{
    const trace_t trace = SV_Move(G_VECTOR(OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2),
        G_VECTOR(OFS_PARM3), static_cast<int>(G_FLOAT(OFS_PARM4)), G_EDICT(OFS_PARM5));

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    VectorCopy(trace.endpos, pr_global_struct->trace_endpos);
    VectorCopy(trace.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist = trace.plane.dist;
    edict_t* hit = trace.ent ? trace.ent : qcvm->edicts;
    pr_global_struct->trace_ent = EDICT_TO_PROG(hit);
}

// ----------------------------------------------------------------------------
// Cvar handles: QC resolves cvar names once per map, then reads them by index.

za::Vector<cvar_t*> cvarHandles;

[[nodiscard]] cvar_t* cvarFromHandle(float handle)
{
    const int i = static_cast<int>(handle);
    if(i < 0 || i >= static_cast<int>(cvarHandles.size()))
    {
        Con_DPrintf("VR: invalid cvar handle %d\n", i);
        return nullptr;
    }

    return cvarHandles[i];
}

void PF_cvar_hmake()
{
    const char* name = G_STRING(OFS_PARM0);
    cvar_t* var = Cvar_FindVar(name);
    if(!var)
    {
        Con_Printf("VR: cvar_hmake: unknown cvar \"%s\"\n", name);
        G_FLOAT(OFS_RETURN) = -1.f;
        return;
    }

    cvarHandles.pushBack(var);
    G_FLOAT(OFS_RETURN) = static_cast<float>(cvarHandles.size() - 1);
}

void PF_cvar_hget()
{
    cvar_t* var = cvarFromHandle(G_FLOAT(OFS_PARM0));
    G_FLOAT(OFS_RETURN) = var ? var->value : 0.f;
}

void PF_cvar_hclear()
{
    cvarHandles.clear();
}

// ----------------------------------------------------------------------------
// World text (see vr_worldtext.hpp)

[[nodiscard]] int worldTextHandle()
{
    return static_cast<int>(G_FLOAT(OFS_PARM0));
}

[[nodiscard]] glm::vec3 vecParm1()
{
    const float* v = G_VECTOR(OFS_PARM1);
    return {v[0], v[1], v[2]};
}

void PF_worldtext_hmake()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(worldtext::serverMake());
}

void PF_worldtext_hsettext()
{
    worldtext::serverSetText(worldTextHandle(), G_STRING(OFS_PARM1));
}

void PF_worldtext_hsetpos()
{
    worldtext::serverSetPos(worldTextHandle(), vecParm1());
}

extern "C" float VR_PortalHandles(edict_t* trig, edict_t* who); // vr_portals.cpp

// float(entity trig, entity who) portal_handles: 1 if the engine carries this player through the trigger's teleporter
// (vr_portals_walk; teleport_touch then leaves him be).
void PF_portal_handles()
{
    G_FLOAT(OFS_RETURN) = VR_PortalHandles(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1));
}

extern "C" float VR_PortalCrossings(void);                    // vr_portals.cpp
extern "C" void VR_PortalEntry(int i, float out[3]);
extern "C" void VR_PortalExit(int i, float out[3]);
extern "C" void VR_PortalTurn(const float v[3], float out[3]);
extern "C" float VR_PortalFromImage(void);

// float() portal_crossings: how many teleporters the last MOVE_PORTALS traceline went through (0 none).
void PF_portal_crossings()
{
    G_FLOAT(OFS_RETURN) = VR_PortalCrossings();
}

// float() portal_from_image: 1 if it began at the player's image, his muzzle held through a teleporter (it may hit him).
void PF_portal_from_image()
{
    G_FLOAT(OFS_RETURN) = VR_PortalFromImage();
}

// vector(float i) portal_entry, portal_exit: where it went into the i-th (from 0) and came out of it.
void PF_portal_entry()
{
    VR_PortalEntry(static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_RETURN));
}

void PF_portal_exit()
{
    VR_PortalExit(static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_RETURN));
}

// vector(vector v) portal_turn: v turned as the last MOVE_PORTALS traceline was by its gates.
void PF_portal_turn()
{
    const float v[3] = {G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]};
    VR_PortalTurn(v, G_VECTOR(OFS_RETURN));
}

void PF_worldtext_hsetangles()
{
    worldtext::serverSetAngles(worldTextHandle(), vecParm1());
}

void PF_worldtext_hsethalign()
{
    worldtext::serverSetHAlign(
        worldTextHandle(), static_cast<worldtext::HAlign>(static_cast<int>(G_FLOAT(OFS_PARM1))));
}

void PF_worldtext_hsetscale()
{
    worldtext::serverSetScale(worldTextHandle(), G_FLOAT(OFS_PARM1));
}

// floattext(origin, text, colour, scale): a text that rises from `origin` and fades, facing
// whoever looks (a damage number), shown to every client.
void PF_floattext()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float* color = G_VECTOR(OFS_PARM2);
    worldtext::serverFloatText({org[0], org[1], org[2]}, G_STRING(OFS_PARM1), {color[0], color[1], color[2]},
        G_FLOAT(OFS_PARM3));
}

// ----------------------------------------------------------------------------
// Map tips (func_vr_tip: QC/vr_tips.qc; see vr_tips.hpp)

[[nodiscard]] int tipHandle()
{
    return static_cast<int>(G_FLOAT(OFS_PARM0));
}

void PF_vr_tip_make()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(tips::serverMake());
}

void PF_vr_tip_setname()
{
    tips::serverSetName(tipHandle(), G_STRING(OFS_PARM1));
}

void PF_vr_tip_settext()
{
    tips::serverSetText(tipHandle(), G_STRING(OFS_PARM1));
}

void PF_vr_tip_setpos()
{
    tips::serverSetPos(tipHandle(), vecParm1());
}

// void(float h, entity e) vr_tip_setentity: the tip follows that entity (the client follows it live). The world: a
// fixed point in the map (the client is told -1).
void PF_vr_tip_setentity()
{
    edict_t* ed = G_EDICT(OFS_PARM1);
    const int num = NUM_FOR_EDICT(ed);
    tips::serverSetEntity(tipHandle(), num > 0 ? num : -1, PR_GetString(ed->v.classname));
}

void PF_vr_tip_setdistance()
{
    tips::serverSetDistance(tipHandle(), G_FLOAT(OFS_PARM1));
}

void PF_vr_tip_setsize()
{
    tips::serverSetSize(tipHandle(), G_FLOAT(OFS_PARM1));
}

void PF_vr_tip_setdelay()
{
    tips::serverSetDelay(tipHandle(), G_FLOAT(OFS_PARM1));
}

void PF_vr_tip_setflags()
{
    tips::serverSetFlags(tipHandle(), static_cast<int>(G_FLOAT(OFS_PARM1)));
}

// ----------------------------------------------------------------------------
// Files

// float(string path) fileexists: whether a file is in the game's search path. Precaching a missing
// model is an error, so the QC checks first for data that may not be installed (the mission packs'
// monsters: vrfiringrange's monster buttons).
void PF_fileexists()
{
    G_FLOAT(OFS_RETURN) = COM_FileExists(G_STRING(OFS_PARM0), nullptr) ? 1.f : 0.f;
}

// float(string folder, float track) campaignmusic: a campaign's own CD track's number for target_vr_music
// (vr_music.cpp: "hipnotic", 4 is Scourge of Armagon's "Ranger Infiltration"), 0 when the player's Quake doesn't have
// it (no such campaign or track, no music file in its folders or the owned installs, external music off).
void PF_campaignmusic()
{
    const int track = VR_MusicCampaignTrack(G_STRING(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)));
    G_FLOAT(OFS_RETURN) = track && BGM_TrackAvailable(track) ? static_cast<float>(track) : 0.f;
}

// ----------------------------------------------------------------------------
// Highlight markers (vr_highlights.cpp; QC vr_highlights.qc)

// float() highlighting: whether a highlight log is running (the QC skips its work when not).
void PF_highlighting()
{
    G_FLOAT(OFS_RETURN) = highlights::active() ? 1.f : 0.f;
}

// highlight(kind, score, subject, detail, since): a moment for the log; `subject`'s classname (world: none); `score` 0:
// the kind's own; `since` the game time it began (0: now).
void PF_highlight()
{
    if(!highlights::active())
    {
        return;
    }
    const edict_t* subject = G_EDICT(OFS_PARM2);
    const char* name = subject != qcvm->edicts && !subject->free ? PR_GetString(subject->v.classname) : "";
    highlights::event(G_STRING(OFS_PARM0), G_FLOAT(OFS_PARM1), name, G_STRING(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

// ----------------------------------------------------------------------------
// Messages

// Same destinations as Ironwail's (static) WriteDest in pr_cmds.c (`len`: the most bytes the write takes, for
// MSG_BROADCAST's room; VR_BroadcastWritten after it).
[[nodiscard]] sizebuf_t* writeDest(int len)
{
    enum
    {
        MSG_BROADCAST = 0,
        MSG_ONE = 1,
        MSG_ALL = 2,
        MSG_INIT = 3
    };

    switch(static_cast<int>(G_FLOAT(OFS_PARM0)))
    {
        case MSG_BROADCAST: return VR_BroadcastDest(len); // sv.datagram, or nowhere when full (vr_server.cpp)
        case MSG_ONE:
        {
            edict_t* ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
            const int entnum = NUM_FOR_EDICT(ent);
            if(entnum < 1 || entnum > svs.maxclients)
            {
                PR_RunError("WriteDest: not a client");
            }
            return &svs.clients[entnum - 1].message;
        }
        case MSG_ALL: return &sv.reliable_datagram;
        case MSG_INIT: return sv.signon;
        default: PR_RunError("WriteDest: bad destination"); return nullptr;
    }
}

void PF_WriteVec3()
{
    sizebuf_t* dest = writeDest(12);
    const float* v = G_VECTOR(OFS_PARM1);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(dest, v[i], sv.protocolflags);
    }
    VR_BroadcastWritten(dest);
}

// ----------------------------------------------------------------------------
// Effects and the player's hands

#include "vr_builtins_melee.inc"
#include "vr_builtins_ragdoll.inc"
#include "vr_builtins_nav.inc"
#include "vr_builtins_held.inc"
#include "vr_builtins_hitmodel.inc"

struct VrBuiltin
{
    const char* name;
    builtin_t func;
};

constexpr VrBuiltin vrBuiltins[] = {
    {"makeforward", PF_makeforward},
    {"weaponhotspot", PF_weaponhotspot},
    {"weaponhotspotinfo", PF_weaponhotspotinfo},
    {"weapongrabspot", PF_weapongrabspot},
    {"weaponanygrip", PF_weaponanygrip},
    {"weapondrawnpose", PF_weapondrawnpose},
    {"admeleebend", PF_admeleebend},
    {"admeleehead", PF_admeleehead},
    {"meleeshow", PF_meleeshow},
    {"magholsterpoll", PF_magholsterpoll},
    {"holsterpos", PF_holsterpos},
    {"modelbounds", PF_modelbounds},
    {"modelcentre", PF_modelcentre},
    {"shapenearest", PF_shapenearest},
    {"loadportof", PF_loadportof},
    {"drawnbounds", PF_drawnbounds},
    {"modeloffsetto", PF_modeloffsetto},
    {"findcone", PF_findcone},
    {"findportalcone", PF_findportalcone},
    {"portal_pullimage", PF_portal_pullimage},
    {"portal_pulltarget", PF_portal_pulltarget},
    {"forcegrabpoint", PF_forcegrabpoint},
    {"catchblend", PF_catchblend},
    {"physicsblast", PF_physicsblast},
    {"propvalue", PF_propvalue},
    {"propgrip", PF_propgrip},
    {"weaponvalue", PF_weaponvalue},
    {"weightdamage", PF_weightdamage},
    {"weightleniency", PF_weightleniency},
    {"throwvelocity", PF_throwvelocity},
    {"throwweight", PF_throwweight},
    {"debrisplan", PF_debrisplan},
    {"debrismodel", PF_debrismodel},
    {"debrisput", PF_debrisput},
    {"debrisplace", PF_debrisplace},
    {"crateplan", PF_crateplan},
    {"cratemodel", PF_cratemodel},
    {"crateput", PF_crateput},
    {"cratecrowbar", PF_cratecrowbar},
    {"cratecrowbarput", PF_cratecrowbarput},
    {"crateplace", PF_crateplace},
    {"sightblocked", PF_sightblocked},
    {"heldshape", PF_heldshape},
    {"heldbox", PF_heldbox},
    {"modelpoint", PF_modelpoint},
    {"physicspush", PF_physicspush},
    {"isragdoll", PF_isragdoll},
    {"ragdollgrab", PF_ragdollgrab},
    {"canragdoll", PF_canragdoll},
    {"ragdollknockdown", PF_ragdollknockdown},
    {"ragdollshovetopple", PF_ragdollshovetopple},
    {"ragdollgetup", PF_ragdollgetup},
    {"ragdollcanstagger", PF_ragdollcanstagger},
    {"monstergroup", mgroups::PF_monstergroup},
    {"monstergroupget", mgroups::PF_monstergroupget},
    {"ragdolldrown", PF_ragdolldrown},
    {"ragdollguard", PF_ragdollguard},
    {"ragdollstagger", PF_ragdollstagger},
    {"ragdollstaggered", PF_ragdollstaggered},
    {"ragdollrelease", PF_ragdollrelease},
    {"ragdollfling", PF_ragdollfling},
    {"ragdollheld", PF_ragdollheld},
    {"ragdollreach", PF_ragdollreach},
    {"ragdollbone", PF_ragdollbone},
    {"ragdollinbox", PF_ragdollinbox},
    {"ragdollboxpart", PF_ragdollboxpart},
    {"ragdollpoint", PF_ragdollpoint},
    {"ragdolldecap", PF_ragdolldecap},
    {"ragdollcut", PF_ragdollcut},
    {"ragdollhead", PF_ragdollhead},
    {"ragdollcutlimb", PF_ragdollcutlimb},
    {"ragdolllimb", PF_ragdolllimb},
    {"ragdolllimbs", PF_ragdolllimbs},
    {"limbmodel", PF_limbmodel},
    {"limbpiece", PF_limbpiece},
    {"limbplace", PF_limbplace},
    {"physicsshot", PF_physicsshot},
    {"physicsdamp", PF_physicsdamp},
    {"ropestep", PF_ropestep},
    {"ropepivot", PF_ropepivot},
    {"ropesend", PF_ropesend},
    {"roperetract", PF_roperetract},
    {"ropeend", PF_ropeend},
    {"tracebox", PF_tracebox},
    {"cvar_hmake", PF_cvar_hmake},
    {"cvar_hget", PF_cvar_hget},
    {"cvar_hclear", PF_cvar_hclear},
    {"worldtext_hmake", PF_worldtext_hmake},
    {"worldtext_hsettext", PF_worldtext_hsettext},
    {"worldtext_hsetpos", PF_worldtext_hsetpos},
    {"worldtext_hsetangles", PF_worldtext_hsetangles},
    {"portal_handles", PF_portal_handles},
    {"portal_crossings", PF_portal_crossings},
    {"portal_entry", PF_portal_entry},
    {"portal_exit", PF_portal_exit},
    {"portal_turn", PF_portal_turn},
    {"portal_from_image", PF_portal_from_image},
    {"worldtext_hsethalign", PF_worldtext_hsethalign},
    {"worldtext_hsetscale", PF_worldtext_hsetscale},
    {"WriteVec3", PF_WriteVec3},
    {"particle2", PF_particle2},
    {"woundevent", PF_woundevent},
    {"hitreact", PF_hitreact},
    {"goresound", PF_goresound},
    {"haptic", PF_haptic},
    {"handimpact", PF_handimpact},
    {"carryangles", PF_carryangles},
    {"carrygrip", PF_carrygrip},
    {"carryplace", PF_carryplace},
    {"carrypouch", PF_carrypouch},
    {"carryfrontpouch", PF_carryfrontpouch},
    {"carryfit", PF_carryfit},
    {"carry2h", PF_carry2h},
    {"carrybody", PF_carrybody},
    {"carryreach", PF_carryreach},
    {"carry2hoff", PF_carry2hoff},
    {"carry2hkeep", PF_carry2hkeep},
    {"floattext", PF_floattext},
    {"vr_tip_make", PF_vr_tip_make},
    {"vr_tip_setname", PF_vr_tip_setname},
    {"vr_tip_settext", PF_vr_tip_settext},
    {"vr_tip_setpos", PF_vr_tip_setpos},
    {"vr_tip_setentity", PF_vr_tip_setentity},
    {"vr_tip_setdistance", PF_vr_tip_setdistance},
    {"vr_tip_setsize", PF_vr_tip_setsize},
    {"vr_tip_setdelay", PF_vr_tip_setdelay},
    {"vr_tip_setflags", PF_vr_tip_setflags},
    {"ejectcasings", PF_ejectcasings},
    {"weaponfired", PF_weaponfired},
    {"tracer", PF_tracer},
    {"watershock", PF_watershock},
    {"playershock", PF_playershock},
    {"viewmessage", PF_viewmessage},
    {"navplace", PF_navplace},
    {"portal_ai_sight", PF_portal_ai_sight},
    {"portal_ai_map", PF_portal_ai_map},
    {"portal_ai_client", PF_portal_ai_client},
    {"portal_ai_gateinfo", PF_portal_ai_gateinfo},
    {"portal_ai_gate", PF_portal_ai_gate},
    {"bodyshock", PF_bodyshock},
    {"bodyshockdeath", PF_bodyshockdeath},
    {"collectfx", PF_collectfx},
    {"bodysmoulder", PF_bodysmoulder},
    {"portal_carry", PF_portal_carry},
    {"findflags", PF_findflags},
    {"findflagsdue", PF_findflagsdue},
    {"propthink", propthink::PF_propthink},
    {"findflagsinview", PF_findflagsinview},
    {"liquidentry", PF_liquidentry},
    {"watersplash", PF_watersplash},
    {"fileexists", PF_fileexists},
    {"campaignmusic", PF_campaignmusic},
    {"highlighting", PF_highlighting},
    {"highlight", PF_highlight},
    {"motionevent", PF_motionevent},
    {"motionpoint", PF_motionpoint},
    {"motionvalue", PF_motionvalue},
    {"hitmodel_target", PF_hitmodel_target},
    {"hitmodel_segment", PF_hitmodel_segment},
    {"hitmodel_any", PF_hitmodel_any},
    {"bodygap", PF_bodygap},
    {"modelanchor", PF_modelanchor},
    {"modelanchorpoint", PF_modelanchorpoint},
    {"hitmodel_rest", PF_hitmodel_rest},
    {"debughitzone", PF_debughitzone},
    {"torchflametouch", PF_torchflametouch},
    {"mapflametouch", PF_mapflametouch},
    {"anglemod", PF_anglemod},
    {"stealthlight", stealth::PF_stealthlight},
    {"flashlightbeam", stealth::PF_flashlightbeam},
    {"pvsvisible", stealth::PF_pvsvisible},
    {"traceseethrough", stealth::PF_traceseethrough},
    {"clientlight", stealth::PF_clientlight},
    {"stealthprofile", stealth::PF_stealthprofile},
    {"meleerun", PF_meleerun},
    {"meleewristspeed", PF_meleewristspeed},
    {"meleewiggled", PF_meleewiggled},
    {"meleegripago", PF_meleegripago},
    {"meleearc", PF_meleearc},
    {"weaponheft", PF_weaponheft},
};

static_assert(firstVrBuiltin + za::getArraySize(vrBuiltins) < MAX_BUILTINS - 200,
    "VR builtins must not overlap Ironwail's downward-allocated builtins");

} // namespace

void sendWound(int num, const float* org, const float* dir, int kind, int amount, int extra)
{
    sendWoundEvent(num, org, dir, kind, amount, extra);
}

void sendParticle2(const float* org, const float* dir, int preset, int count)
{
    particle2Message(org, dir, preset, count);
}

// vr_forcegrab_grid_stats: the force grab's searches since the last (and resets them).
void forcegrabGridStats_f()
{
    ForcegrabGridStats& s = forcegrabGridStats;
    Con_Printf("vr_forcegrab_grid: %lld searches, %.1f edicts tested a search by the grid, %.1f by the walk\n",
        s.searches, s.searches ? static_cast<double>(s.tested) / static_cast<double>(s.searches) : 0.0,
        s.searches ? static_cast<double>(s.walked) / static_cast<double>(s.searches) : 0.0);
    s = ForcegrabGridStats{};
}

void propQueriesTest_f()
{
    if(!sv.active) { Con_Printf("vr_prop_query_test: enter a map first\n"); return; }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    const float verify = vr_prop_query_verify.value;
    vr_prop_query_verify.value = 1.f;
    physics::testModelQueries();
    float saved[OFS_PARM3 + 3 - OFS_PARM0];
    memcpy(saved, qcvm->globals + OFS_PARM0, sizeof(saved));
    const glm::vec3 player = geom::vec(progs::playerEdict()->v.origin);
    for(int k = 0; k < 96; k++)
    {
        const glm::vec3 from = player + glm::vec3{float((k % 8 - 4) * 32), float((k / 8 - 6) * 32), float(k % 3 * 16)};
        const glm::vec3 aim = glm::normalize(glm::vec3{float(k % 3 - 1), float(k % 5 - 2), .5f});
        progs::setVecParm(OFS_PARM0, from);
        progs::setVecParm(OFS_PARM2, aim);
        G_FLOAT(OFS_PARM1) = k % 4 == 0 ? 0.f : k % 4 == 1 ? 1.f : k % 4 == 2 ? 200.f : 4096.f;
        G_FLOAT(OFS_PARM3) = k % 2 == 0 ? -1.f : .85f;
        PF_findportalcone();
    }
    memcpy(qcvm->globals + OFS_PARM0, saved, sizeof(saved));
    vr_prop_query_verify.value = verify;
    PR_PopQCVM(oldVm);
    Con_Printf("prop force-grab queries: PASS 96 exact ordered-chain comparisons\n");
}

void bindBuiltins()
{
    int number = firstVrBuiltin;
    for(const VrBuiltin& b : vrBuiltins)
    {
        if(qcvm->builtins[number] != qcvm->builtins[0])
        {
            Sys_Error("VR builtin #%d (%s) collides with an engine builtin", number, b.name);
        }

        qcvm->builtins[number] = b.func;
        qcvm->builtin_ext[number] = STD_QC;

        if(const func_t f = findFunction(b.name))
        {
            dfunction_t& fn = qcvm->functions[f];
            if(fn.first_statement == 0)
            {
                fn.first_statement = -number;
            }
        }

        number++;
    }
}

void onEdictFree(edict_t* ed)
{
    // An entity removed: what the server keeps by its number is forgotten (physics::forgetEntity), and for a wounded one
    // the clients free its mask, else the next entity put in its slot (edicts are reused), with the same model (a
    // monster from the same spawner, a gib), would show its wounds. Reliable: once, at its end.
    if(qcvm != &sv.qcvm || !sv.active)
    {
        return;
    }
    // Checked: ED_Free runs for any edict, one a saved game's parse frees among them.
    const int num = NUM_FOR_EDICT_CHECKED(ed);
    if(num > 0)
    {
        physics::forgetEntity(num);
        ropesim::forget(num);
        box3d::toolForget(num); // (the toolgun's pins and joints on it)
        toolgun::forget(num);   // (and what its tools hold)
        brushes::edictFreed(num); // (a box made in the game: its brush goes with it)
    }
    if(num <= 0 || num >= static_cast<int>(woundsSent.size()) || !woundsSent[static_cast<za::SizeT>(num)])
    {
        return;
    }
    woundsSent[static_cast<za::SizeT>(num)] = 0;
    if(sv.reliable_datagram.cursize + 4 > sv.reliable_datagram.maxsize)
    {
        return;
    }
    MSG_WriteByte(&sv.reliable_datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.reliable_datagram, protocol::QVR_SVC_WOUNDCLEAR);
    MSG_WriteShort(&sv.reliable_datagram, num);
}

void resetBuiltinState()
{
    cvarHandles.clear();
    woundsSent.clear();
    worldtext::serverReset();
    tips::serverReset();
}

} // namespace qvr::progs
