#include "vr_stats.hpp"
#include "vr_alloccount.h"
#include "vr_geom.hpp"
#include "vr_progs.hpp"
// vr_box3d_diag.cpp -- the physics commands: spawning, lists, hashes, shapes and the rest, and their registration;
// the piles, the timing benches, the ragdolls' and the player's commands are vr_box3d_diag_piles/perf/ragdoll/player.cpp
// (shared: vr_box3d_diag_internal.hpp).
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_box3d_diag_internal.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"

namespace box3d::detail
{
// ---------------------------------------------------------------------------------------------------------------------
// Test commands (either engine: they only move entities)

// The entities `which` names: a number, a classname (every one), or "props" (every rigid body).
[[nodiscard]] za::Vector<edict_t*> entitiesNamed(const char* which)
{
    za::Vector<edict_t*> out;
    if(which[0] >= '0' && which[0] <= '9')
    {
        const int num = Q_atoi(which);
        if(num > 0 && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
        {
            out.pushBack(EDICT_NUM(num));
        }
        return out;
    }
    progs::forEachEdict(1, [&](edict_t* e) {
        if((!strcmp(which, "props") ? isRigid(e) && modelOf(e) : !strcmp(PR_GetString(e->v.classname), which)))
        {
            out.pushBack(e);
        }
    });
    return out;
}

namespace
{

// vr_physics_hull_stats: the props' and actors' hulls made this session on the main thread, and the slowest.
void hullStats_f()
{
    const HullBuildStats& s = hullBuilds;
    Con_Printf("vr_physics_hull_stats: %d props' and actors' hulls made, %.1f ms of the sync (the slowest %.2f ms); %d "
               "built ahead on the pool, %d of them taken\n",
        s.built, s.ms, s.mostMs, s.posted, s.ahead);
    for(const HullBuildStats::Slow& slow : s.slowest)
    {
        if(slow.ms > 0.0)
        {
            Con_Printf("  %6.2f ms  %s frame %d (at %.1f s)\n", slow.ms, slow.model, slow.frame, slow.time);
        }
    }
}

// vr_physics_hash [piles]: a hash of every rigid body's origin, angles, velocity and spin, bit for bit (determinism tests:
// two runs of the same script print the same). `piles`: only what the stress tests spawned (vr_physics_bigpile), not the
// map's own props.
void hash_f()
{
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    const bool piles = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "piles");
    uint64_t h = 1469598103934665603ull;
    int count = 0;
    for(edict_t* e : entitiesNamed("props"))
    {
        if(piles && !(static_cast<int>(e->v.spawnflags) & stressTagBit))
        {
            continue;
        }
        const glm::vec3 v[4] = {vec(e->v.origin), vec(e->v.angles), vec(e->v.velocity), fieldVec(e, fields().vr_spin)};
        const auto* bytes = reinterpret_cast<const unsigned char*>(v);
        for(size_t i = 0; i < sizeof(v); i++)
        {
            h = (h ^ bytes[i]) * 1099511628211ull;
        }
        count++;
    }
    Con_Printf("vr_physics_hash: %d bodies, %016llx\n", count, static_cast<unsigned long long>(h));
}
} // namespace

// Box3D's profile b added to a (fields: floats only).
void addProfile(b3Profile& a, const b3Profile& b)
{
    static_assert(sizeof(b3Profile) % sizeof(float) == 0);
    float* const x = reinterpret_cast<float*>(&a);
    const float* const y = reinterpret_cast<const float*>(&b);
    for(za::SizeT i = 0; i < sizeof(b3Profile) / sizeof(float); i++)
    {
        x[i] += y[i];
    }
}

namespace
{

// vr_physics_list [<classname | props>]: the rigid bodies (or those), where they are and how they move.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
    Con_Printf("%d, %s:\n", static_cast<int>(list.size()), world ? "Box3D" : "no Box3D world");
    for(edict_t* e : list)
    {
        const int num = NUM_FOR_EDICT(e);
        const char* body = world && num < static_cast<int>(world->slots.size()) ? kindName(world->slots[num].kind) : "-";
        Con_Printf("  %d %s %.1f %.1f %.1f angles %.0f %.0f %.0f vel %.0f %s (%s)%s\n", num, PR_GetString(e->v.classname), e->v.origin[0],
            e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2], VectorLength(e->v.velocity),
            hasFlag(e, FL_ONGROUND) ? "asleep" : "awake", body, e->v.takedamage ? va(" health %.0f", e->v.health) : "");
        if(vr_debug_box3d.value)
        {
            const qmodel_t* model = modelOf(e);
            const float mass = qvr::box3d::propMass(e);
            Con_Printf("    %s: %.1f kg%s, thrown x%.2f\n", model ? model->name : "no model", mass,
                model && propMassSetting(model) > 0.f ? " (Held Object Offsets)" : model && weaponMassSetting(e, model) > 0.f ? " (Weapon Weights)" : "",
                qvr::props::throwScale(model ? qvr::props::slotForModel(model) : -1, mass));
            Con_Printf("    movetype %d, solid %d, rigid %d, flags %d\n", static_cast<int>(e->v.movetype), static_cast<int>(e->v.solid),
                isRigid(e) ? 1 : 0, static_cast<int>(e->v.flags));
            // Its Size (Held Object Offsets), for tests: the body's box (the world's axes), its Quake box, and the scale
            // it is drawn at (the client's entity matrix: its first axis' length).
            glm::vec3 body{0.f};
            if(world && num < static_cast<int>(world->slots.size()) && B3_IS_NON_NULL(world->slots[num].body) &&
                b3Body_IsValid(world->slots[num].body))
            {
                const b3AABB box = b3Body_ComputeAABB(world->slots[num].body);
                body = world->toU(box.upperBound) - world->toU(box.lowerBound);
            }
            float drawn = 0.f;
            if(num < cl.num_entities && cl_entities[num].model)
            {
                const entity_t& ce = cl_entities[num];
                vec3_t o, a{-ce.angles[0], ce.angles[1], ce.angles[2]};
                VectorCopy(ce.origin, o);
                float m[16];
                R_EntityMatrix(m, o, a, ce.scale);
                VR_BrushTransform(&ce, m);
                drawn = za::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            }
            Con_Printf("    size x%.2f: body %.1f %.1f %.1f, box %.1f %.1f %.1f, drawn x%.3f\n",
                model ? qvr::props::drawnSize(model) : 1.f, body.x, body.y, body.z, e->v.size[0], e->v.size[1], e->v.size[2],
                drawn);
        }
    }
}

// The extents of `body`'s hull shapes in its own frame (units; false: none), and the depth of `point` (world units)
// inside them (the most negative plane distance: > 0 inside; < 0 outside, roughly how far).
bool hullExtents(b3BodyId body, glm::vec3& lo, glm::vec3& hi, const glm::vec3* point, float* depth)
{
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(body);
    const glm::vec3 at = world->toU(xf.p);
    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    bool any = false;
    float inside = -1e9f;
    for(int i = 0; i < count; i++)
    {
        if(b3Shape_GetType(shapes[i]) != b3_hullShape)
        {
            continue;
        }
        const b3HullData* hull = b3Shape_GetHull(shapes[i]);
        const b3Vec3* p = b3GetHullPoints(hull);
        for(int k = 0; k < hull->vertexCount; k++)
        {
            lo = glm::min(lo, world->toU(p[k]));
            hi = glm::max(hi, world->toU(p[k]));
        }
        any = true;
        if(point && depth)
        {
            const glm::vec3 local = world->toU(b3InvRotateVector(xf.q, world->toM(*point - at)));
            const b3Plane* planes = b3GetHullPlanes(hull);
            float worst = -1e9f;
            for(int k = 0; k < hull->faceCount; k++)
            {
                worst = za::max(worst, glm::dot(glmv(planes[k].normal), local) - planes[k].offset * world->m2u);
            }
            inside = za::max(inside, -worst);
        }
    }
    if(depth)
    {
        *depth = inside;
    }
    return any;
}

// vr_physics_shapes [classname...]: for tests (the reload by contact's collision, ROUND21.md): each such prop's body (its
// hull's extents in its own frame) against its drawn model's (the server's drawn vertices, as the client draws it), and
// the first player's held weapons' reach bodies (their hulls) against the weapon as drawn (the client's view entity) and
// how deep its loading port lies inside the hull. Units and cm.
void shapes_f()
{
    if(!sv.active || !world)
    {
        return;
    }
    const progs::ServerVm vm;
    const float cm = 100.f / world->m2u;
    const auto box = [&](const char* what, const glm::vec3& lo, const glm::vec3& hi) {
        const glm::vec3 s = hi - lo;
        Con_Printf("    %s %.2f x %.2f x %.2f units (%.1f x %.1f x %.1f cm)\n", what, s.x, s.y, s.z, s.x * cm, s.y * cm,
            s.z * cm);
    };
    // A gun's convex pieces (gunPieces): how many, and the most their hulls (and its one hull) lie off the drawn gun.
    const auto pieces = [&](const PropHullKey& key, b3BodyId body) {
        const auto it = world->gunReports.find(key);
        if(it == world->gunReports.end())
        {
            Con_Printf("    convex pieces: none (one hull: vr_box3d_gun_pieces %g)\n", vr_box3d_gun_pieces.value);
            return;
        }
        const convex::Report& r = it->second;
        Con_Printf("    convex pieces: %d shapes, made in %.1f ms (cell %.2f units, %d cells); the most a hull lies off the "
                   "gun %.2f units (%.1f cm), one hull %.2f (%.1f cm); volume %.1f, one hull %.1f, the gun %.1f\n",
            b3Body_GetShapeCount(body), r.ms, r.cell, r.cells, r.gap, r.gap * cm, r.wholeGap, r.wholeGap * cm, r.volume,
            r.wholeVolume, r.solid);
    };
    for(int arg = 1; arg < za::max(Cmd_Argc(), 2); arg++)
    {
        for(edict_t* e : entitiesNamed(Cmd_Argc() > arg ? Cmd_Argv(arg) : "props"))
        {
            const int num = NUM_FOR_EDICT(e);
            if(num >= static_cast<int>(world->slots.size()) || B3_IS_NULL(world->slots[num].body) ||
                !b3Body_IsValid(world->slots[num].body))
            {
                continue;
            }
            const qmodel_t* model = modelOf(e);
            Con_Printf("  %d %s (%s, drawn x%.2f):\n", num, PR_GetString(e->v.classname), model ? model->name : "no model",
                model ? qvr::props::drawnSize(model) : 1.f);
            glm::vec3 lo, hi;
            if(hullExtents(world->slots[num].body, lo, hi, nullptr, nullptr))
            {
                box("body (its hull)", lo, hi);
            }
            else
            {
                Con_Printf("    body: a box (not a hull)\n");
            }
            za::Vector<glm::vec3>& vertices = scratch.propVerts;
            if(held::drawnVertices(e, vertices) && !vertices.empty())
            {
                glm::vec3 dlo{1e9f}, dhi{-1e9f};
                for(const glm::vec3& v : vertices)
                {
                    dlo = glm::min(dlo, v);
                    dhi = glm::max(dhi, v);
                }
                box("drawn          ", dlo, dhi);
            }
            box("Quake box      ", geom::vec(e->v.mins),
                geom::vec(e->v.maxs));
            if(model && isGun(e))
            {
                const Slot& slot = world->slots[num];
                pieces(PropHullKey{model, static_cast<int>(e->v.frame),
                           {slot.mins.x, slot.mins.y, slot.mins.z, slot.maxs.x, slot.maxs.y, slot.maxs.z}},
                    slot.body);
            }
        }
    }
    if(world->hands.size() < 2)
    {
        return;
    }
    edict_t* player = EDICT_NUM(1);
    for(int h = 0; h < 2; h++)
    {
        const World::HandBody& hb = world->hands[1][static_cast<size_t>(h)];
        if(B3_IS_NULL(hb.reach) || hb.key.what != World::ReachKey::Weapon)
        {
            continue;
        }
        const glm::vec3 port = fieldVec(player, progs::handFields(h).loadPortPos);
        glm::vec3 lo, hi;
        float depth = 0.f;
        if(!hullExtents(hb.reach, lo, hi, &port, &depth))
        {
            continue;
        }
        Con_Printf("  %s hand's weapon %s:\n", h ? "main" : "off", hb.key.model ? hb.key.model->name : "?");
        box("reach body (its hull)", lo, hi);
        // As drawn: the frame's vertices through the view entity (view::modelPoint), into the reach body's frame.
        if(const view::ViewEntity* ve = view::heldWeapon(h); ve && ve->ent.model == hb.key.model)
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ve->ent.model));
            const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                                hdr->frames[0].firstpose * hdr->numverts;
            const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
            const glm::vec3 at = world->toU(xf.p);
            glm::vec3 dlo{1e9f}, dhi{-1e9f};
            for(int k = 0; k < hdr->numverts; k++)
            {
                const glm::vec3 m{hdr->scale_origin[0] + hdr->scale[0] * verts[k].v[0],
                    hdr->scale_origin[1] + hdr->scale[1] * verts[k].v[1], hdr->scale_origin[2] + hdr->scale[2] * verts[k].v[2]};
                const glm::vec3 local = world->toU(b3InvRotateVector(xf.q, world->toM(view::modelPoint(*ve, m) - at)));
                dlo = glm::min(dlo, local);
                dhi = glm::max(dhi, local);
            }
            box("drawn                ", dlo, dhi);
            Con_Printf("    the hull's offset from the drawn: lo %.2f %.2f %.2f, hi %.2f %.2f %.2f units\n", lo.x - dlo.x,
                lo.y - dlo.y, lo.z - dlo.z, hi.x - dhi.x, hi.y - dhi.y, hi.z - dhi.z);
        }
        Con_Printf("    its loading port %s the hull: %.2f units (%.1f cm)\n", depth > 0.f ? "inside" : "outside",
            za::fabs(depth), za::fabs(depth) * cm);
        pieces(PropHullKey{hb.key.model, hb.key.mirrored ? -3 : -2, {}}, hb.reach);
    }
}

// vr_physics_forcegrab <number | classname | props>: whether the force grab may take each (QC's VR_Forcegrab_IsEligible,
// as the first player's hands search), for tests: an explosive box never.
void forcegrabCheck_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_physics_forcegrab <number | classname | props>\n");
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = qvr::progs::findFunction("VR_Forcegrab_IsEligible");
    if(!fn)
    {
        Con_Printf("vr_physics_forcegrab: no VR_Forcegrab_IsEligible in the progs\n");
        return;
    }
    for(edict_t* e : entitiesNamed(Cmd_Argv(1)))
    {
        const int oldSelf = pr_global_struct->self;
        pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
        pr_global_struct->time = qcvm->time;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
        PR_ExecuteProgram(fn);
        const bool eligible = G_FLOAT(OFS_RETURN) != 0.f;
        pr_global_struct->self = oldSelf;
        Con_Printf("vr_physics_forcegrab: %d %s: %s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname),
            eligible ? "may be force grabbed" : "never force grabbed");
    }
}

// vr_physics_spawn <classname> [<distance> [<left>]]: a map entity made by its spawn function (a key, a weapon, the
// biosuit, a powerup...) on the floor `distance` units (48) ahead of the first player and `left` units to the left, as
// the map would place it there, turned as vr_test_spawn_facing says. For tests.
void spawn_f()
{
    if(!sv.active || Cmd_Argc() < 2 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_spawn <classname> [<distance> [<left>]]\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = EDICT_NUM(1);
    vec3_t yaw{0.f, player->v.angles[1], 0.f};
    vec3_t forward, right, up;
    AngleVectors(yaw, forward, right, up);
    const bool reach = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "reach");
    const float distance = Cmd_Argc() > 2 && !reach ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 48.f;
    const float left = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : 0.f;
    glm::vec3 origin{0.f}, angles{0.f};
    for(int i = 0; i < 3; i++)
    {
        origin[i] = player->v.origin[i] + forward[i] * distance - right[i] * left;
    }
    // "reach" (the Debug menu's pickups): in front of you at arm's reach, a little below the eyes (where the head is and
    // faces, not the player's origin and angles: those stray in room scale), lying level, its muzzle away; never through
    // a wall (pulled back short of it), nor in your own box (a prop there was pushed off it, or hit you). It falls from
    // there to the floor before you.
    const hands::State& hs = hands::current();
    if(reach && hs.valid)
    {
        const float m2u = units::metresToUnits();
        glm::vec3 f, r, u;
        hands::angleVectors({0.f, hs.headAngles.y, 0.f}, f, r, u);
        const glm::vec3 from = hs.head;
        const glm::vec3 lo = vec(player->v.absmin) - glm::vec3{8.f}, hi = vec(player->v.absmax) + glm::vec3{8.f};
        float ahead = 0.45f * m2u;
        for(; ahead < 1.f * m2u; ahead += 1.f)
        {
            const glm::vec3 p = from + f * ahead - r * left;
            if(p.x < lo.x || p.x > hi.x || p.y < lo.y || p.y > hi.y)
            {
                break;
            }
        }
        const glm::vec3 to = from + f * ahead - glm::vec3{0.f, 0.f, 0.35f * m2u} - r * left;
        vec3_t a, b;
        store(from, a);
        store(to, b);
        const trace_t tr = SV_Move(a, vec3_origin, vec3_origin, b, MOVE_NOMONSTERS, player);
        const glm::vec3 end{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
        origin = tr.fraction < 1.f ? end - glm::normalize(to - from) * (0.1f * m2u) : to;
        angles = {0.f, hs.headAngles.y, 0.f};
        Con_Printf("vr_physics_spawn: %s at reach: %.0f %.0f %.0f, %.2f m from your eyes, %.2f m ahead\n", Cmd_Argv(1),
            origin.x, origin.y, origin.z, glm::distance(from, origin) / m2u, glm::dot(origin - from, f) / m2u);
    }
    // vr_test_spawn_facing 1: facing the player, 2: facing away (where the player faces); else the spawn's own (0),
    // as before. Set before its spawn function (a monster's ideal_yaw is taken from it).
    // (4: its left side to the player, 5: its right side.)
    const float facing = vr_test_spawn_facing.value;
    if(facing == 1.f || facing == 2.f || facing == 4.f || facing == 5.f)
    {
        angles.y = anglemod(player->v.angles[1] + (facing == 1.f ? 180.f : facing == 2.f ? 0.f : facing == 4.f ? 90.f : -90.f));
    }
    edict_t* e = qvr::box3d::spawnClass(Cmd_Argv(1), origin, angles);
    if(e)
    {
        Con_Printf("vr_physics_spawn: %d %s at %.0f %.0f %.0f\n", NUM_FOR_EDICT(e), Cmd_Argv(1), e->v.origin[0], e->v.origin[1], e->v.origin[2]);
    }
}

// vr_physics_fling <number | classname | props | nearest> <speed> [<yaw> [<up>]]: sets their velocity, `speed` units/s
// along `yaw` (degrees; the player's view by default) and `up` units/s up (0): a prop sent skidding along the floor (its
// scrape: the physics sounds), or tossed. `nearest`: the loose prop nearest the player. For tests (Debug menu: Slide the
// Nearest Prop).
void fling_f()
{
    if(!sv.active || Cmd_Argc() < 3 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_fling <number | classname | props | nearest> <speed> [<yaw> [<up>]]\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = EDICT_NUM(1);
    za::Vector<edict_t*> list;
    if(!strcmp(Cmd_Argv(1), "nearest"))
    {
        if(const int found = qvr::box3d::nearestProp())
        {
            list.pushBack(EDICT_NUM(found));
        }
    }
    else
    {
        list = entitiesNamed(Cmd_Argv(1));
    }
    const float speed = static_cast<float>(Q_atof(Cmd_Argv(2)));
    const float yaw = glm::radians(Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : player->v.v_angle[1]);
    const float up = Cmd_Argc() > 4 ? static_cast<float>(Q_atof(Cmd_Argv(4))) : 0.f;
    for(edict_t* e : list)
    {
        store(glm::vec3{za::cos(yaw) * speed, za::sin(yaw) * speed, up}, e->v.velocity);
        setFlag(e, FL_ONGROUND, false);
        // A pushable corpse (not a ragdoll): its body moves it, not its entity's velocity.
        const int n = NUM_FOR_EDICT(e);
        if(world && n < static_cast<int>(world->slots.size()) && world->slots[n].kind == Kind::Corpse &&
            world->slots[n].ragdoll < 0 && world->slots[n].corpseDynamic && B3_IS_NON_NULL(world->slots[n].body) &&
            b3Body_IsValid(world->slots[n].body))
        {
            b3Body_SetLinearVelocity(world->slots[n].body, world->toM(vec(e->v.velocity)));
            b3Body_SetAwake(world->slots[n].body, true);
            world->slots[n].asleep = false;
        }
        Con_Printf("vr_physics_fling: %d %s at %.0f %.0f %.0f u/s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname),
            e->v.velocity[0], e->v.velocity[1], e->v.velocity[2]);
    }
    if(list.empty())
    {
        Con_Printf("vr_physics_fling: none\n");
    }
}

// vr_physics_sink [<number | classname | props>]: how far each one's drawn model is inside the floor under it: its
// drawn surface's lowest corners (held::drawnVertices, where the entity is) against the floor straight below them (a
// point trace down from 24 units above), and the Box3D shape's lowest point (its hull's corners where the body is). A
// positive "sunk" is into the floor, negative above it; "hull" is how far the collision shape's bottom is below (-) or
// above (+) the drawn model's. The summary's average and most are of those within 4 units of the floor (lying on it,
// not on a box).
void sink_f()
{
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
    za::Vector<glm::vec3> vertices; // (a debug command's: made each call)
    float worst = 0.f, total = 0.f;
    int counted = 0;
    for(edict_t* e : list)
    {
        qmodel_t* model = modelOf(e);
        if(!model || !held::drawnVertices(e, vertices) || vertices.empty())
        {
            continue;
        }
        const int num = NUM_FOR_EDICT(e);
        const glm::mat3 axes = held::axesFromAngles(e->v.angles, model->type == mod_brush);
        const glm::vec3 origin = vec(e->v.origin);
        float low = 1e9f;
        for(glm::vec3& v : vertices)
        {
            v = origin + axes * v;
            low = za::min(low, v.z);
        }
        // The floor under the lowest corners (those within 2 units of the lowest: a gun on its side rests on several).
        float sunk = -1e9f;
        int traced = 0;
        for(const glm::vec3& v : vertices)
        {
            if(v.z > low + 2.f || traced >= 64)
            {
                continue;
            }
            traced++;
            // In a solid: how far under the surface above it (the floor it is sunk into); else how far above the
            // floor under it (negative).
            vec3_t at{v.x, v.y, v.z};
            const bool inside = SV_Move(at, vec3_origin, vec3_origin, at, MOVE_NOMONSTERS, e).startsolid;
            vec3_t start{v.x, v.y, v.z + (inside ? 24.f : 0.01f)}, end{v.x, v.y, inside ? v.z : v.z - 24.f};
            const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, e);
            if(tr.fraction < 1.f && !tr.startsolid && tr.plane.normal[2] > 0.7f)
            {
                sunk = za::max(sunk, tr.endpos[2] - v.z);
            }
        }
        float hullLow = 1e9f;
        if(world && num < static_cast<int>(world->slots.size()) &&
            B3_IS_NON_NULL(world->slots[num].body) && b3Body_IsValid(world->slots[num].body))
        {
            const b3BodyId body = world->slots[num].body;
            const b3WorldTransform xf = b3Body_GetTransform(body);
            b3ShapeId shapes[maxBodyShapes];
            const int n = b3Body_GetShapes(body, shapes, maxBodyShapes);
            for(int i = 0; i < n; i++)
            {
                const b3HullData* hull = b3Shape_GetType(shapes[i]) == b3_hullShape ? b3Shape_GetHull(shapes[i]) : nullptr;
                const b3Vec3* points = hull ? b3GetHullPoints(hull) : nullptr;
                for(int k = 0; points && k < hull->vertexCount; k++)
                {
                    const b3Vec3 p = b3RotateVector(xf.q, points[k]);
                    hullLow = za::min(hullLow, (p.z + xf.p.z) * world->m2u);
                }
            }
        }
        const bool onFloor = sunk > -1e8f;
        const char* kind = world && num < static_cast<int>(world->slots.size()) ? kindName(world->slots[num].kind) : "-";
        Con_Printf("  %d %s z %.2f drawn low %.2f", num, PR_GetString(e->v.classname), e->v.origin[2], low);
        Con_Printf(onFloor ? " sunk %.2f" : " (no floor)", sunk);
        if(hullLow < 1e8f)
        {
            Con_Printf(" hull %+.2f", hullLow - low);
        }
        Con_Printf(" %s (%s)\n", hasFlag(e, FL_ONGROUND) ? "asleep" : "awake", kind);
        if(onFloor && sunk > -4.f)
        {
            worst = za::max(worst, sunk);
            total += sunk;
            counted++;
        }
    }
    Con_Printf("vr_physics_sink: %d on a floor, sunk %.2f on average, %.2f at most\n", counted,
        counted ? total / static_cast<float>(counted) : 0.f, worst);
}

bool commandsRegistered = false; // (registerCommands: once)

void portalInfo_f()
{
    const progs::ServerVm vm;
    Con_Printf("physicsportals: generation=%d current=%d copies=%d\n", world ? world->generation : -1,
        worldGeneration(), world ? static_cast<int>(world->portalCopies.size()) : 0);
    if(world)
    {
        for(const World::PortalCopy& c : world->portalCopies)
        {
            const glm::vec3 here = world->toU(b3Body_GetPosition(c.original));
            const glm::vec3 there = world->toU(b3Body_GetPosition(c.copy));
            Con_Printf("physicsportals: %d %s at %.1f %.1f %.1f -> %.1f %.1f %.1f\n",
                static_cast<int>(reinterpret_cast<uintptr_t>(b3Body_GetUserData(c.original))), c.dynamic ? "dynamic" : "held",
                here.x, here.y, here.z, there.x, there.y, there.z);
        }
    }
}

void* heapAllocate(size_t size, int32_t alignment) { return VR_HeapAlignedAlloc(size, static_cast<size_t>(alignment)); }
void heapFree(void* p, size_t) { VR_HeapAlignedFree(p); }

// vr_gun_pieces_cache_stats: the guns' pieces this session, read from the disk or cut (Debug > Profiling and Memory).
void gunPiecesCacheStats_f()
{
    const GunPiecesDisk& d = gunPiecesDisk;
    Con_Printf("vr_gun_pieces_cache %g: %d guns' pieces read from the disk, %d cut, %d files written; checked %d, %d "
               "differed\n",
        vr_gun_pieces_cache.value, d.read.load(), d.cut.load(), d.written.load(), d.checked.load(), d.differed.load());
}
} // namespace

void registerCommands()
{
    bool& registered = commandsRegistered;
    if(!registered)
    {
        registered = true;
        b3SetAllocator(heapAllocate, heapFree);
        const b3Version v = b3GetVersion();
        Con_Printf("box3d: Box3D %d.%d.%d, %s\n", v.major, v.minor, v.revision,
            !cpuHasAvx2              ? "SSE2 kernels (4 lanes; no AVX2 on this CPU)"
            : vr_box3d_avx2.value    ? "AVX2 kernels (8 lanes)"
                                     : "SSE2 kernels (4 lanes; vr_box3d_avx2 0)");
        Cmd_AddCommand("vr_physics_player", player_f);
        Cmd_AddCommand("vr_gun_pieces_cache_stats", gunPiecesCacheStats_f);
        Cmd_AddCommand("vr_physics_stack", stack_f);
        Cmd_AddCommand("vr_physics_pyramid", pyramid_f);
        Cmd_AddCommand("vr_physics_list", list_f);
        Cmd_AddCommand("vr_physics_shapes", shapes_f);
        Cmd_AddCommand("vr_physics_loose", loose_f);
        Cmd_AddCommand("vr_physics_pile", pile_f);
        Cmd_AddCommand("vr_physics_bigpile", bigPile_f);
        Cmd_AddCommand("vr_physics_clearpiles", clearPiles_f);
        Cmd_AddCommand("vr_physics_hash", hash_f);
        Cmd_AddCommand("vr_struggle_status", struggleStatus_f);
        Cmd_AddCommand("vr_physics_hull_stats", hullStats_f);
        Cmd_AddCommand("vr_physics_steptime", steptime_f);
        Cmd_AddCommand("vr_physics_frametime", frametime_f);
        Cmd_AddCommand("vr_physics_mtbench", mtbench_f);
        Cmd_AddCommand("vr_physics_blast", blast_f);
        Cmd_AddCommand("vr_physics_sink", sink_f);
        Cmd_AddCommand("vr_prop_press_test", pressTest_f);
        Cmd_AddCommand("vr_prop_strike_test", strikeTest_f);
        Cmd_AddCommand("vr_physics_inlevel", inLevel_f);
        Cmd_AddCommand("vr_physics_approach", approach_f);
        Cmd_AddCommand("vr_physics_shotbench", shotBench_f);
        Cmd_AddCommand("vr_physics_fire", fire_f);
        Cmd_AddCommand("vr_knockdown_test", knockdownTest_f);
        Cmd_AddCommand("vr_shock_ragdoll_check", shockCheck_f);
        Cmd_AddCommand("vr_physics_inside", inside_f);
        Cmd_AddCommand("vr_physics_spawn", spawn_f);
        Cmd_AddCommand("vr_physics_fling", fling_f);
        Cmd_AddCommand("vr_physics_forcegrab", forcegrabCheck_f);
        Cmd_AddCommand("vr_physics_portals", portalInfo_f);
        Cmd_AddCommand("vr_corpse_list", corpseList_f);
        Cmd_AddCommand("vr_corpse_drop", corpseDrop_f);
        Cmd_AddCommand("vr_corpse_poses", corpsePoses_f);
        Cmd_AddCommand("vr_ragdoll_list", ragdollList_f);
        Cmd_AddCommand("vr_ragdoll_speeds", ragdollSpeeds_f);
        Cmd_AddCommand("vr_ragdoll_arm_test", ragdollArmTest_f);
        Cmd_AddCommand("vr_ragdoll_info", ragdoll::info_f);
        Cmd_AddCommand("vr_ragdoll_draw_info", ragdoll::drawInfo_f);
        Cmd_AddCommand("vr_limb_models", limbmodel::info_f);
        Cmd_AddCommand("vr_drawn_motion_test", ragdoll::motionTest_f);
        Cmd_AddCommand("vr_ragdoll_blast_test", ragdollBlastTest_f);
        Cmd_AddCommand("vr_ragdoll_active_list", ragdollActiveList_f);

        Cmd_AddCommand("vr_ragdoll_env_hit", ragdollEnvHit_f);
    }
}

} // namespace box3d::detail
