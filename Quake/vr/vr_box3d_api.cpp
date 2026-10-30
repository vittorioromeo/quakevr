#include "vr_alloccount.h"
#include "vr_progs.hpp"
#include "vr_geom.hpp"
// vr_box3d_api.cpp -- the API: loads, pins, tool joints, blasts, pushes, shots and rays, held boxes, ropes and level
// checks, counts and queries.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace qvr::box3d
{
GunPiecesCount gunPiecesCount()
{
    return {gunPiecesDisk.read.load(), gunPiecesDisk.cut.load(), gunPiecesDisk.written.load()};
}

void beforeLoad()
{
    finishLoads();
    const qmodel_t* map = sv.worldmodel;
    if(!wanted() || !map || map->type != mod_brush)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const double t0 = Sys_DoubleTime();
    (void)inputsOf(map); // (what the mesh is made of, read once for this load)
    VR_TimeAdd("box3d: the world mesh's key (its faces read)", Sys_DoubleTime() - t0);
    if(MeshCache* found = findMesh(map, m2u))
    {
        za::rotate(found, found + 1, meshCache.data() + meshCache.size()); // the latest used last
        if(vr_debug_box3d.value)
        {
            const MeshCache& c = meshCache.back();
            Con_Printf("box3d: the mesh of %s kept (%d triangles; %.1f MB, its key %.1f MB)\n", map->name,
                c.stats.triangles, c.mesh ? c.mesh->byteCount / 1048576.0 : 0.0,
                static_cast<double>(c.inputs.size() * sizeof(za::U32)) / 1048576.0);
        }
        return; // (kept: the same map again, or one played before)
    }
    PendingMesh& p = pendingMesh;
    p.key = MeshCache{};
    q_strlcpy(p.key.name, map->name, sizeof(p.key.name));
    p.key.vertexes = map->numvertexes;
    p.key.surfaces = map->numsurfaces;
    p.key.m2u = m2u;
    p.key.junctions = vr_box3d_mesh_junctions.value != 0.f;
    p.map = map;
    MeshStats* stats = &p.key.stats; // (only the job writes it until settleMesh)
    const bool junctions = p.key.junctions;
    p.job = jobs::async([map, m2u, junctions, stats] { return worldMesh(map, m2u, junctions, *stats); });
}

void finishLoads()
{
    settleMesh();
    meshInputs.map = nullptr; // (the map's model goes with the hunk: read again at the next load's)
    meshInputs.same.clear();
}

bool toss(edict_t* ent)
{
    (void)ent;
    registerCommands();
    return wanted();
}

void noteServerPhysicsStart()
{
    serverPhysicsStart = Sys_DoubleTime();
}

void reset()
{
    registerCommands();
    destroyWorld();
    toolRecords.pinned.clear();
    toolRecords.joints.clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// The toolgun's pins and joints (vr_toolgun.cpp).

edict_t* spawnClass(const char* classname, const glm::vec3& origin, const glm::vec3& angles, const char* model,
    const char* key, float value, bool probe, float weaponFlags, const char* key2, float value2)
{
    if(!sv.active)
    {
        return nullptr;
    }
    const progs::ServerVm vm;
    func_t fn = qvr::progs::findFunction(classname);
    // AD support: AD's version where it owns the classname in an AD map, AD's artifacts, pickups and ladders in any
    // map (vr_ad.cpp), as ED_LoadFromFile
    if(!strncmp(classname, "ad_", 3)) // AD support: explicit AD variants in every spawn menu
    {
        if(!VR_AD_RulesOn() && !VR_AD_ToolgunSpawnsOutside(classname + 3)) { return nullptr; } // AD support
        classname += 3; // AD support: retain the canonical entity classname for VR and AD tests
    }
    else if(VR_AD_RulesOn())
    {
        const func_t ad = qvr::progs::findFunction(va("ad_%s", classname));
        fn = ad && VR_AD_OwnsClassname(classname, fn != 0) ? ad : fn;
    }
    else if(!fn && VR_AD_SpawnsOutside(classname))
    {
        fn = qvr::progs::findFunction(va("ad_%s", classname));
    }
    if(!fn)
    {
        Con_Printf("no spawn function %s\n", classname);
        return nullptr;
    }
    const auto engineString = [](const char* text) {
        char* copy = nullptr;
        const int str = PR_AllocString(static_cast<int>(strlen(text)) + 1, &copy);
        strcpy(copy, text);
        return str;
    };
    edict_t* e = ED_Alloc();
    store(origin, e->v.origin);
    store(angles, e->v.angles);
    e->v.classname = engineString(classname);
    if(model && model[0])
    {
        e->v.model = engineString(model); // (a rock's or a brick's: its spawn function sets the model it is given)
    }
    const char* const keys[2] = {key, key2};
    const float values[2] = {value, value2};
    for(int i = 0; i < 2; i++)
    {
        const int ofs = keys[i] && keys[i][0] ? ED_FindFieldOffset(keys[i]) : -1;
        if(ofs >= 0)
        {
            fieldFloat(e, ofs) = values[i]; // (func_weapon_grabbable's weapon, spawnflags; an armed monster's weapon2)
        }
    }
    if(weaponFlags != 0.f) // (func_weapon_grabbable's weapon flags: AD support, an AD melee weapon's kind)
    {
        const int ofs = progs::fields().weaponflags;
        if(ofs >= 0)
        {
            fieldFloat(e, ofs) = weaponFlags;
        }
    }
    VR_EdictIndex_Touch(e); // (the edict index: a classname set in C)
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    // A live spawn: what the map would make static (a torch, a flame) stays an entity (QC vr_spawn_live).
    // A probe (the toolgun's ghost: probe 1): made only to be looked at and removed (QC vr_spawn_live 2: no side things).
    float* const live = bindings().spawnLive;
    if(live)
    {
        *live = probe ? 2.f : 1.f;
    }
    PR_ExecuteProgram(fn);
    // What a map's item does over its first second (placed, made an object, shrunk), done now: drawn at its final size
    // and place from its first frame (QC VR_SpawnLive_Finish).
    if(!e->free)
    {
        if(const func_t finish = qvr::progs::findFunction("VR_SpawnLive_Finish"))
        {
            pr_global_struct->self = EDICT_TO_PROG(e);
            PR_ExecuteProgram(finish);
        }
    }
    if(live)
    {
        *live = 0.f;
    }
    if(e->free)
    {
        return nullptr;
    }
    SV_LinkEdict(e, false);
    return e;
}

void setPinned(int num, bool pinned)
{
    za::Vector<int>& list = toolRecords.pinned;
    const bool was = toolPinned(num);
    if(pinned && !was)
    {
        list.pushBack(num);
    }
    else if(!pinned && was)
    {
        za::vectorEraseIf(list, [num](int n) { return n == num; });
    }
}

bool isPinned(int num)
{
    return toolPinned(num);
}

int unpinAll()
{
    const int n = static_cast<int>(toolRecords.pinned.size());
    toolRecords.pinned.clear();
    return n;
}

bool addToolJoint(int a, int b, ToolJoint kind, const glm::vec3& atA, const glm::vec3& atB, const glm::vec3& axis)
{
    if(!world || a == b)
    {
        return false;
    }
    const b3BodyId ba = toolJointBody(a), bb = toolJointBody(b);
    if(B3_IS_NULL(ba) || B3_IS_NULL(bb))
    {
        return false;
    }
    const b3WorldTransform xa = b3Body_GetTransform(ba), xb = b3Body_GetTransform(bb);
    const glm::quat qa = fromB3(xa.q), qb = fromB3(xb.q);
    const glm::vec3 pa = glmv(xa.p), pb = glmv(xb.p);
    const glm::vec3 ma = glmv(world->toM(atA));
    const glm::vec3 mb = glmv(world->toM(atB));
    // The joint's frame in the world: a weld's and a ball's anything (the world's axes), a hinge's z along `axis` (it
    // turns about it), a slider's x along it (it slides along it).
    const glm::vec3 dir = glm::length(axis) > 1e-4f ? glm::normalize(axis) : glm::vec3{0.f, 0.f, 1.f};
    glm::quat frame{1.f, 0.f, 0.f, 0.f};
    if(kind == ToolJoint::Hinge)
    {
        frame = zTo(dir);
    }
    else if(kind == ToolJoint::Slider)
    {
        frame = zTo(dir) * glm::angleAxis(glm::radians(-90.f), glm::vec3{0.f, 1.f, 0.f}); // (x onto z, then z onto dir)
    }
    // Where the joint holds each body: a weld, a ball, a hinge and a slider at the second point (both frames there); a
    // rope and a spring from the first point on the first to the second on the second.
    const bool span = kind == ToolJoint::Rope || kind == ToolJoint::Spring;
    const glm::vec3 onA = span ? ma : mb;
    ToolJointRecord j;
    j.a = a;
    j.b = b;
    j.kind = kind;
    j.frameA = b3Transform{b3v(glm::inverse(qa) * (onA - pa)), toB3(glm::normalize(glm::inverse(qa) * frame))};
    j.frameB = b3Transform{b3v(glm::inverse(qb) * (mb - pb)), toB3(glm::normalize(glm::inverse(qb) * frame))};
    j.length = za::max(glm::distance(ma, mb), 0.05f);
    toolRecords.joints.pushBack(j);
    syncToolJoints();
    return true;
}

int removeToolJoints(int num)
{
    int removed = 0;
    za::vectorEraseIf(toolRecords.joints, [num, &removed](const ToolJointRecord& j) {
        if(num != 0 && j.a != num && j.b != num)
        {
            return false;
        }
        if(world && B3_IS_NON_NULL(j.id) && b3Joint_IsValid(j.id))
        {
            b3DestroyJoint(j.id, true);
        }
        removed++;
        return true;
    });
    return removed;
}

int toolJointCount(int num)
{
    int n = 0;
    for(const ToolJointRecord& j : toolRecords.joints)
    {
        if(num == 0 || j.a == num || j.b == num)
        {
            n += world && B3_IS_NON_NULL(j.id) && b3Joint_IsValid(j.id) ? 1 : 0;
        }
    }
    return n;
}

void forEachToolJoint(za::FunctionRef<void(const glm::vec3& atA, const glm::vec3& atB, ToolJoint kind)> fn)
{
    if(!world)
    {
        return;
    }
    for(const ToolJointRecord& j : toolRecords.joints)
    {
        if(B3_IS_NULL(j.id) || !b3Joint_IsValid(j.id))
        {
            continue;
        }
        const b3BodyId a = toolJointBody(j.a), b = toolJointBody(j.b);
        if(B3_IS_NULL(a) || B3_IS_NULL(b))
        {
            continue;
        }
        const b3WorldTransform xa = b3Body_GetTransform(a), xb = b3Body_GetTransform(b);
        const glm::vec3 pa = glmv(xa.p) + fromB3(xa.q) * glmv(j.frameA.p), pb = glmv(xb.p) + fromB3(xb.q) * glmv(j.frameB.p);
        fn(pa * world->m2u, pb * world->m2u, j.kind);
    }
}

void forEachFuelRope(za::FunctionRef<void(int gun, int tank, float length, bool joined)> fn)
{
    if(!world)
    {
        return;
    }
    for(const World::FuelRope& r : world->fuelRopes)
    {
        if(r.kind != World::RopeKind::Tank)
        {
            continue;
        }
        fn(r.gun, r.tank, (r.length > 0.f ? r.length : za::max(vr_adgun_tank_rope.value, 0.1f)) * world->m2u,
            B3_IS_NON_NULL(r.id) && b3Joint_IsValid(r.id));
    }
}

void forEachFlailRope(za::FunctionRef<void(int handle, int head, float length, bool joined, float apart)> fn)
{
    if(!world)
    {
        return;
    }
    for(const World::FuelRope& r : world->fuelRopes)
    {
        if(r.kind != World::RopeKind::Flail)
        {
            continue;
        }
        const bool joined = B3_IS_NON_NULL(r.id) && b3Joint_IsValid(r.id);
        float apart = -1.f;
        if(joined && b3Body_IsValid(r.gunBody) && b3Body_IsValid(r.tankBody))
        {
            const b3Transform a = b3Body_GetTransform(r.gunBody), b = b3Body_GetTransform(r.tankBody);
            const glm::vec3 pa = glmv(a.p) + fromB3(a.q) * glmv(world->toM(r.gunEnd));
            const glm::vec3 pb = glmv(b.p) + fromB3(b.q) * glmv(world->toM(r.tankEnd));
            apart = glm::distance(pa, pb) * world->m2u;
        }
        fn(r.gun, r.tank, (r.length > 0.f ? r.length : za::clamp(vr_flail_chain.value, 0.1f, 2.f)) * world->m2u, joined, apart);
    }
}

void toolForget(int num)
{
    if(toolPinned(num))
    {
        setPinned(num, false);
    }
    if(!toolRecords.joints.empty())
    {
        (void)removeToolJoints(num);
    }
}

void blast(const glm::vec3& at, float damage)
{
    if(!world || !wanted() || damage <= 0.f)
    {
        return;
    }
    // As T_RadiusDamage's: within damage + 40 units, `damage` less half the distance to the middle, seen from the
    // blast (its CanDamage: the middle, or the top). Thrown at 4 units a second per point (half of what Quake gives a
    // player) for a health box's mass, lighter things faster, heavier slower (by the square root of their mass, within
    // half to twice), at most 600 (15 m/s): a rocket beside a pile scatters it a few metres, not across the map. A
    // little upwards (a blast on the floor lifts what is round it).
    const float reach = damage + 40.f;
    constexpr float referenceMass = 6.f; // kg: a health box (its hull at 400 kg/m^3)
    // The ragdolls' parts (blastRagdoll), and the blast kept a moment for those its damage is about to make (a grunt it
    // killed goes limp a few frames later).
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0)
        {
            blastRagdoll(r, at, damage);
        }
    }
    world->recentBlasts[static_cast<za::SizeT>(world->nextBlast)] = World::RecentBlast{at, damage, qcvm->time};
    world->nextBlast = (world->nextBlast + 1) % static_cast<int>(world->recentBlasts.size());
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
        if(s.kind != Kind::Prop)
        {
            continue;
        }
        const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(s.body));
        const float distance = glm::distance(centre, at);
        const float points = damage - 0.5f * distance;
        if(distance > reach || points <= 0.f)
        {
            continue;
        }
        const b3AABB box = b3Body_ComputeAABB(s.body);
        bool seen = false;
        for(const float z : {centre.z, box.upperBound.z * world->m2u - 1.f})
        {
            vec3_t from, to;
            store(at, from);
            store(glm::vec3{centre.x, centre.y, z}, to);
            const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, nullptr);
            if(tr.fraction >= 1.f && !tr.startsolid)
            {
                seen = true;
                break;
            }
        }
        if(!seen)
        {
            continue;
        }
        glm::vec3 dir = distance > 0.01f ? (centre - at) / distance : glm::vec3{0.f, 0.f, 1.f};
        dir = glm::normalize(dir + glm::vec3{0.f, 0.f, 0.35f});
        const float mass = za::max(b3Body_GetMass(s.body), 1e-3f);
        const float speed = za::min(4.f * points * CLAMP(0.5f, za::sqrt(referenceMass / mass), 2.f), 600.f); // u/s
        // Through a point under the middle: the push along the floor turns it over (spin), a lift from under does not.
        const glm::vec3 low = centre - glm::vec3{0.f, 0.f, 0.3f * b3Body_GetMinExtent(s.body) * world->m2u};
        b3Body_ApplyLinearImpulse(s.body, world->toM(dir * (speed * mass)), world->toM(low), true);
        s.asleep = false;
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d blasted: %.0f points, %.1f kg, %.0f u/s\n", num, points, mass, speed);
        }
    }
}

bool push(edict_t* ent, const glm::vec3& at, const glm::vec3& velocity, float pusherMass, float restitution, float most)
{
    const int num = NUM_FOR_EDICT(ent);
    if(!world || num >= static_cast<int>(world->slots.size()) ||
        (world->slots[num].kind != Kind::Prop && world->slots[num].ragdoll < 0))
    {
        return false;
    }
    // (A ragdoll: the part nearest where it is pushed.)
    const b3BodyId body = world->slots[num].ragdoll >= 0 ? ragdollPartNear(num, at) : world->slots[num].body;
    if(B3_IS_NULL(body))
    {
        return false;
    }
    // Never into what holds it (the floor under a crate pressed from above, a wall behind it: vr_box3d_press_back).
    const glm::vec3 wanted = world->slots[num].ragdoll >= 0 ? velocity : unbraced(body, velocity);
    const float speed = glm::length(wanted) / world->m2u; // m/s
    if(speed < 1e-3f)
    {
        return true;
    }
    const glm::vec3 d = wanted / glm::length(wanted);
    const b3Vec3 point = world->toM(at);
    const b3Vec3 pv = b3Body_GetWorldPointVelocity(body, point);
    const float along = pv.x * d.x + pv.y * d.y + pv.z * d.z;
    if(along >= speed)
    {
        return true;
    }
    // The impulse along d at the point that gives it that speed there: over its mass and inertia as seen from the point.
    const b3Pos c = b3Body_GetWorldCenter(body);
    const glm::vec3 r{point.x - static_cast<float>(c.x), point.y - static_cast<float>(c.y), point.z - static_cast<float>(c.z)};
    const glm::vec3 rd = glm::cross(r, d);
    const b3Matrix3 inv = b3Body_GetWorldInverseRotationalInertia(body);
    const b3Vec3 irdB = b3MulMV(inv, b3Vec3{rd.x, rd.y, rd.z});
    const float k = b3Body_GetInverseMass(body) + glm::dot(rd, glm::vec3{irdB.x, irdB.y, irdB.z});
    if(k <= 0.f)
    {
        return false;
    }
    // (A pusher of a mass: the impulse of the two meeting, the pusher's inverse mass added; more by its restitution, the
    // point's gain (j k) at most `most`.)
    float j = (1.f + za::clamp(restitution, 0.f, 1.f)) * (speed - along) / (k + (pusherMass > 0.f ? 1.f / pusherMass : 0.f));
    if(most > 0.f)
    {
        j = za::min(j, most / k);
    }
    b3Body_ApplyLinearImpulse(body, b3Vec3{d.x * j, d.y * j, d.z * j}, point, true);
    world->slots[num].asleep = false;
    if(world->slots[num].ragdoll >= 0) // (a dying hand holding on, struck there, lets go: vr_ragdoll_active_env)
    {
        RagdollBodies& rag = world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
        ragactive::struck(rag.active, activeParts(rag), body);
    }
    if(vr_debug_box3d.value || (vr_debug_ragdoll.value && world->slots[num].ragdoll >= 0))
    {
        Con_Printf("box3d: %d pushed at %.1f %.1f %.1f to %.2f m/s along the push (%.2f before), %.1f N s\n", num, at.x, at.y, at.z,
            speed, along, j);
        if(pusherMass > 0.f)
        {
            Con_Printf("box3d: (that push by %.1f kg against its %.1f kg)\n", pusherMass, b3Body_GetMass(body));
        }
    }
    return true;
}

int shot(const glm::vec3& start, const glm::vec3& end, const glm::vec3& velocity, float pusherMass)
{
    const glm::vec3 delta = end - start;
    if(!world || glm::dot(delta, delta) < 1e-6f)
    {
        return 0;
    }
    // The nearest loose prop's shape along it (props only: not the world, movers, monsters, players, hands or what a
    // hand holds).
    struct Hit
    {
        int num{0};
        b3Vec3 point{};
    } hit;
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catProp;
    b3World_CastRay(world->id, world->toM(start), world->toM(delta), filter,
        [](b3ShapeId shape, b3Pos point, b3Vec3, float fraction, uint64_t, int, int, void* context) -> float {
            const int num = numOf(shape);
            if(num <= 0 || num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
            {
                return -1.f;
            }
            auto& out = *static_cast<Hit*>(context);
            out.num = num;
            out.point = point;
            return fraction;
        },
        &hit);
    if(hit.num <= 0 || !push(EDICT_NUM(hit.num), world->toU(hit.point), velocity, pusherMass))
    {
        return 0;
    }
    return hit.num;
}

int sightRay(const glm::vec3& start, const glm::vec3& end, int ignoreA, int ignoreB)
{
    const glm::vec3 delta = end - start;
    const int blockField = fields().vr_blocksight;
    if(!world || blockField < 0 || glm::dot(delta, delta) < 1e-6f)
    {
        return 0;
    }
    // Solid props' shapes only (catSolid): one in a hand is a held body (catHeld) and never met.
    struct Context
    {
        int num{0};
        int blockField, ignoreA, ignoreB;
    } ctx{0, blockField, ignoreA, ignoreB};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catSolid;
    b3World_CastRay(world->id, world->toM(start), world->toM(delta), filter,
        [](b3ShapeId shape, b3Pos, b3Vec3, float fraction, uint64_t, int, int, void* context) -> float {
            auto& c = *static_cast<Context*>(context);
            const int num = numOf(shape);
            if(num <= 0 || num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop ||
                num == c.ignoreA || num == c.ignoreB)
            {
                return -1.f;
            }
            edict_t* ent = EDICT_NUM(num);
            if(ent->free || fieldFloat(ent, c.blockField) <= 0.f || static_cast<int>(ent->v.solid) != SOLID_BBOX)
            {
                return -1.f;
            }
            c.num = num;
            return 0.f; // one is enough
        },
        &ctx);
    return ctx.num;
}

float heldRay(int num, const glm::vec3& start, const glm::vec3& end)
{
    const glm::vec3 delta = end - start;
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()) || glm::dot(delta, delta) < 1e-6f)
    {
        return 1.f;
    }
    const Slot& s = world->slots[num];
    if(s.kind != Kind::Held || !b3Body_IsValid(s.body))
    {
        return 1.f;
    }
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int n = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    float nearest = 1.f;
    for(int i = 0; i < n; i++)
    {
        const b3WorldCastOutput out = b3Shape_RayCast(shapes[i], world->toM(start), world->toM(delta));
        if(out.hit && out.fraction < nearest)
        {
            nearest = za::max(out.fraction, 0.f);
        }
    }
    return nearest;
}

int heldBox(int num, const glm::vec3& lo, const glm::vec3& hi, float reach)
{
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()))
    {
        return -1;
    }
    const Slot& s = world->slots[num];
    if(s.kind != Kind::Held || !b3Body_IsValid(s.body))
    {
        return -1;
    }
    // The box's corners in the held body's frame (its shapes' own), grown round by `reach` (the proxy's radius).
    const b3WorldTransform xf = b3Body_GetTransform(s.body);
    b3Vec3 corners[8];
    for(int k = 0; k < 8; k++)
    {
        const glm::vec3 c{(k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z};
        corners[k] = b3InvRotateVector(xf.q, b3Sub(world->toM(c), xf.p));
    }
    const b3ShapeProxy proxy{corners, 8, za::max(reach, 0.f) / world->m2u};
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int n = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    for(int i = 0; i < n; i++)
    {
        bool meets = false;
        switch(b3Shape_GetType(shapes[i]))
        {
            case b3_hullShape: meets = b3OverlapHull(b3Shape_GetHull(shapes[i]), b3Transform_identity, &proxy); break;
            case b3_capsuleShape:
            {
                const b3Capsule capsule = b3Shape_GetCapsule(shapes[i]);
                meets = b3OverlapCapsule(&capsule, b3Transform_identity, &proxy);
                break;
            }
            case b3_sphereShape:
            {
                const b3Sphere sphere = b3Shape_GetSphere(shapes[i]);
                meets = b3OverlapSphere(&sphere, b3Transform_identity, &proxy);
                break;
            }
            default: break;
        }
        if(meets)
        {
            return 1;
        }
    }
    return 0;
}

bool shapeNearest(int num, const glm::vec3& a, const glm::vec3& b, glm::vec3& out)
{
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[num];
    if((s.kind != Kind::Held && s.kind != Kind::Prop) || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
    {
        return false;
    }
    // The segment in the body's frame (its shapes' own); the nearest of its shapes by GJK (b3ShapeDistance). A held one
    // where the hand holds it now (its body follows it at the next step: a fast blow's frame would ask where it was).
    b3WorldTransform xf = b3Body_GetTransform(s.body);
    if(s.kind == Kind::Held)
    {
        edict_t* ent = EDICT_NUM(num);
        xf.p = world->toM(vec(ent->v.origin));
        xf.q = rotationOf(ent, s);
    }
    const glm::vec3 at = world->toU(xf.p);
    const b3Vec3 segment[2]{b3InvRotateVector(xf.q, world->toM(a - at)), b3InvRotateVector(xf.q, world->toM(b - at))};
    const b3ShapeProxy proxyB{segment, glm::distance(a, b) > 1e-4f ? 2 : 1, 0.f};
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int n = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    float best = 1e9f;
    b3Vec3 nearest{0.f, 0.f, 0.f};
    for(int i = 0; i < n; i++)
    {
        b3ShapeProxy proxyA{nullptr, 0, 0.f};
        b3Capsule capsule;
        b3Sphere sphere;
        switch(b3Shape_GetType(shapes[i]))
        {
            case b3_hullShape:
            {
                const b3HullData* hull = b3Shape_GetHull(shapes[i]);
                proxyA = b3ShapeProxy{b3GetHullPoints(hull), za::min(hull->vertexCount, B3_MAX_SHAPE_CAST_POINTS), 0.f};
                break;
            }
            case b3_capsuleShape:
                capsule = b3Shape_GetCapsule(shapes[i]);
                proxyA = b3ShapeProxy{&capsule.center1, 2, capsule.radius};
                break;
            case b3_sphereShape:
                sphere = b3Shape_GetSphere(shapes[i]);
                proxyA = b3ShapeProxy{&sphere.center, 1, sphere.radius};
                break;
            default: break;
        }
        if(proxyA.count == 0)
        {
            continue;
        }
        const b3DistanceInput input{proxyA, proxyB, b3Transform_identity, true};
        b3SimplexCache cache{};
        const b3DistanceOutput result = b3ShapeDistance(&input, &cache, nullptr, 0);
        if(result.distance < best)
        {
            best = result.distance;
            nearest = result.pointA;
        }
    }
    if(best >= 1e9f)
    {
        return false;
    }
    out = at + world->toU(b3RotateVector(xf.q, nearest));
    return true;
}

int hullPlanes(int num, glm::vec4* planes, int maxPlanes, int* hullEnds, int maxHulls, int& hulls)
{
    hulls = 0;
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()))
    {
        return 0;
    }
    const Slot& s = world->slots[num];
    if((s.kind != Kind::Held && s.kind != Kind::Prop) || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
    {
        return 0;
    }
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int n = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    int count = 0;
    for(int i = 0; i < n && hulls < maxHulls; i++)
    {
        if(b3Shape_GetType(shapes[i]) != b3_hullShape)
        {
            continue;
        }
        const b3HullData* hull = b3Shape_GetHull(shapes[i]);
        const b3Plane* p = hull ? b3GetHullPlanes(hull) : nullptr;
        if(!p || hull->faceCount <= 0 || count + hull->faceCount > maxPlanes)
        {
            continue;
        }
        for(int k = 0; k < hull->faceCount; k++)
        {
            planes[count++] = glm::vec4{p[k].normal.x, p[k].normal.y, p[k].normal.z, p[k].offset * world->m2u};
        }
        hullEnds[hulls++] = count;
    }
    return count;
}

bool isBox3DProp(int num)
{
    return world && num > 0 && num < static_cast<int>(world->slots.size()) && world->slots[num].kind == Kind::Prop;
}

bool contactPoint(int num, glm::vec3& point)
{
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[num];
    if(s.kind != Kind::Prop || b3Body_GetContactCapacity(s.body) <= 0)
    {
        return false;
    }
    za::Array<b3ContactData, 8> contacts;
    const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
    const b3Vec3 centre = b3Body_GetWorldCenter(s.body);
    float best = -1.f;
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& c = contacts[i];
        const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), s.body);
        for(int m = 0; m < c.manifoldCount; m++)
        {
            for(int k = 0; k < c.manifolds[m].pointCount; k++)
            {
                const b3ManifoldPoint& mp = c.manifolds[m].points[k];
                if(mp.totalNormalImpulse > best)
                {
                    best = mp.totalNormalImpulse;
                    point = world->toU(b3Add(centre, isA ? mp.anchorA : mp.anchorB));
                }
            }
        }
    }
    return best > 0.f; // (a speculative contact that didn't push is no touch)
}

bool castProps(const glm::vec3& from, const glm::vec3& to, int skip, PropHit& hit)
{
    hit = PropHit{};
    const glm::vec3 delta = to - from;
    if(!world || glm::dot(delta, delta) < 1e-6f)
    {
        return false;
    }
    struct Query
    {
        int skip;
        PropHit* hit;
    } q{skip, &hit};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catProp;
    b3World_CastRay(world->id, world->toM(from), world->toM(delta), filter,
        [](b3ShapeId shape, b3Pos point, b3Vec3 normal, float fraction, uint64_t, int, int, void* context) -> float {
            auto& cq = *static_cast<Query*>(context);
            const int num = numOf(shape);
            if(num <= 0 || num == cq.skip || num >= static_cast<int>(world->slots.size()) ||
                world->slots[num].kind != Kind::Prop)
            {
                return -1.f; // (not this one)
            }
            if(fraction < cq.hit->fraction)
            {
                cq.hit->num = num;
                cq.hit->fraction = fraction;
                cq.hit->point = world->toU(point);
                cq.hit->normal = glm::vec3{normal.x, normal.y, normal.z};
            }
            return fraction; // (the closest)
        },
        &q);
    return hit.num > 0;
}

bool damp(edict_t* ent, const glm::vec3& relativeTo, float keep, float keepSpin, float maxSpeed, const glm::vec3& add)
{
    const int num = NUM_FOR_EDICT(ent);
    if(!world || num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
    {
        return false;
    }
    const b3BodyId body = world->slots[num].body;
    const b3Vec3 lv = b3Body_GetLinearVelocity(body);
    const glm::vec3 v = glm::vec3{lv.x, lv.y, lv.z} * world->m2u; // units/s
    glm::vec3 rel = (v - relativeTo) * za::clamp(keep, 0.f, 1.f);
    const float speed = glm::length(rel);
    if(maxSpeed > 0.f && speed > maxSpeed)
    {
        rel *= maxSpeed / speed;
    }
    const glm::vec3 out = relativeTo + rel + add;
    if(glm::distance(out, v) > 1e-3f)
    {
        b3Body_SetLinearVelocity(body, world->toM(out));
        if(glm::length(add) > 0.f)
        {
            b3Body_SetAwake(body, true);
            world->slots[num].asleep = false;
        }
    }
    const b3Vec3 w = b3Body_GetAngularVelocity(body);
    const float ks = za::clamp(keepSpin, 0.f, 1.f);
    if(ks < 1.f)
    {
        b3Body_SetAngularVelocity(body, b3Vec3{w.x * ks, w.y * ks, w.z * ks});
    }
    return true;
}

float propMass(edict_t* ent)
{
    qmodel_t* model = ent && !ent->free ? modelOf(ent) : nullptr;
    if(!model || (model->type != mod_alias && model->type != mod_brush))
    {
        return 0.f;
    }
    if(const float mass = massSetting(ent, model); mass > 0.f)
    {
        return mass;
    }
    if(!world)
    {
        return 0.f;
    }
    glm::vec3 lo, hi;
    localBox(ent, model, lo, hi);
    const b3HullData* hull = propHull(ent, model, lo, hi);
    const glm::vec3 size = (hi - lo) / world->m2u;
    return (hull ? hull->volume : size.x * size.y * size.z) * densityOf(ent, model) * props::massScale(model);
}

namespace
{
// The rope's queries: what it can't pass through, less its ends.
struct RopeQuery
{
    int skipA{0};
    int skipB{0};
    box3d::RopeHit* hit{nullptr};
    bool any{false};
};

[[nodiscard]] b3QueryFilter ropeFilter()
{
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catProp;
    filter.maskBits = catWorld | catMover | catProp | catFixture;
    return filter;
}

} // namespace

bool ropeCast(const glm::vec3& from, const glm::vec3& to, float radius, int skipA, int skipB, RopeHit& hit)
{
    hit = RopeHit{};
    if(!world || glm::distance(from, to) < 1e-3f)
    {
        return false;
    }
    const b3Vec3 zero = b3Vec3_zero;
    const b3ShapeProxy proxy{&zero, 1, radius / world->m2u};
    RopeQuery q{skipA, skipB, &hit, false};
    b3World_CastShape(world->id, world->toM(from), &proxy, world->toM(to - from), ropeFilter(),
        [](b3ShapeId shape, b3Pos, b3Vec3 normal, float fraction, uint64_t, int, int, void* context) -> float {
            RopeQuery& rq = *static_cast<RopeQuery*>(context);
            const int num = numOf(shape);
            if(num != 0 && (num == rq.skipA || num == rq.skipB))
            {
                return -1.f; // (not this one)
            }
            if(fraction < rq.hit->fraction)
            {
                rq.hit->fraction = fraction;
                rq.hit->normal = glm::vec3{normal.x, normal.y, normal.z};
                rq.any = true;
            }
            return fraction; // (the closest)
        },
        &q);
    if(q.any)
    {
        hit.centre = from + (to - from) * hit.fraction;
    }
    return q.any;
}

bool castAt(int num, const glm::vec3& from, const glm::vec3& to, float radius, float& fraction, bool& hasBody)
{
    fraction = 1.f;
    // (A prop's or a fixture's body: the cast's filter meets only theirs, not an actor's box nor a held one.)
    hasBody = world && num > 0 && num < static_cast<int>(world->slots.size()) &&
              B3_IS_NON_NULL(world->slots[static_cast<za::SizeT>(num)].body) &&
              (world->slots[static_cast<za::SizeT>(num)].kind == Kind::Prop ||
                  world->slots[static_cast<za::SizeT>(num)].kind == Kind::Fixture);
    if(!hasBody || glm::distance(from, to) < 1e-3f)
    {
        return false;
    }
    struct Query
    {
        int num;
        float fraction;
        bool any;
    } q{num, 1.f, false};
    const b3Vec3 zero = b3Vec3_zero;
    const b3ShapeProxy proxy{&zero, 1, za::max(radius, 0.01f) / world->m2u};
    b3World_CastShape(world->id, world->toM(from), &proxy, world->toM(to - from), ropeFilter(),
        [](b3ShapeId shape, b3Pos, b3Vec3, float f, uint64_t, int, int, void* context) -> float {
            Query& cq = *static_cast<Query*>(context);
            if(numOf(shape) != cq.num)
            {
                return -1.f; // (not this one)
            }
            if(f < cq.fraction)
            {
                cq.fraction = f;
                cq.any = true;
            }
            return f;
        },
        &q);
    fraction = q.fraction;
    return q.any;
}

glm::vec3 pressOffset(int player, int hand)
{
    if(!world || player < 1 || player >= static_cast<int>(world->hands.size()) || hand < 0 || hand > 1)
    {
        return glm::vec3{0.f};
    }
    return world->hands[static_cast<za::SizeT>(player)][static_cast<za::SizeT>(hand)].press;
}

bool restsOnHand(int num, int player, int hand)
{
    if(!world || num <= 0 || num >= static_cast<int>(world->slots.size()) || player < 1 ||
        player >= static_cast<int>(world->hands.size()) || hand < 0 || hand > 1)
    {
        return false;
    }
    const Slot& s = world->slots[static_cast<za::SizeT>(num)];
    const World::HandBody& hb = world->hands[static_cast<za::SizeT>(player)][static_cast<za::SizeT>(hand)];
    if(s.kind != Kind::Prop || !b3Body_IsValid(s.body))
    {
        return false;
    }
    za::Array<b3ContactData, 16> contacts;
    const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& c = contacts[static_cast<za::SizeT>(i)];
        const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), s.body);
        const b3BodyId other = b3Shape_GetBody(isA ? c.shapeIdB : c.shapeIdA);
        if(!(B3_IS_NON_NULL(hb.reach) && B3_ID_EQUALS(other, hb.reach)) && !(B3_IS_NON_NULL(hb.body) && B3_ID_EQUALS(other, hb.body)))
        {
            continue;
        }
        for(int m = 0; m < c.manifoldCount; m++)
        {
            // (The manifold's normal points from A to B: here from the hand up into the prop.)
            const float up = isA ? -c.manifolds[m].normal.z : c.manifolds[m].normal.z;
            if(c.manifolds[m].pointCount > 0 && up > restUp)
            {
                return true;
            }
        }
    }
    return false;
}

namespace
{
// The level's solids for a held thing (the world and its brush entities), and a box's corners turned (metres, from
// its origin), shrunk by `in` units (the proxy's radius makes them up again).
[[nodiscard]] b3QueryFilter levelFilter()
{
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catProp | catHeld;
    filter.maskBits = catWorld | catMover | (vr_corpse_collide_held.value ? catCorpse : 0); // (a corpse: its mask says)
    return filter;
}

[[nodiscard]] za::Array<b3Vec3, 8> boxCorners(const glm::vec3& lo, const glm::vec3& hi, const glm::quat& rot, float in)
{
    za::Array<b3Vec3, 8> out;
    const glm::vec3 a = glm::min(lo + glm::vec3{in}, 0.5f * (lo + hi)), b = glm::max(hi - glm::vec3{in}, 0.5f * (lo + hi));
    for(int k = 0; k < 8; k++)
    {
        out[static_cast<size_t>(k)] = world->toM(rot * glm::vec3{k & 1 ? b.x : a.x, k & 2 ? b.y : a.y, k & 4 ? b.z : a.z});
    }
    return out;
}
} // namespace

namespace detail
{
// A prop's own box in its axes (units, from its origin): a brush model's own (held, its entity's box is the one round
// all its turns), else the drawn one its body was made with.
void ownBox(const Slot& s, glm::vec3& lo, glm::vec3& hi)
{
    if(s.model && s.model->type == mod_brush)
    {
        lo = geom::vec(s.model->mins);
        hi = geom::vec(s.model->maxs);
        return;
    }
    lo = s.mins;
    hi = s.maxs;
}
[[nodiscard]] bool boxInLevel(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& at, const glm::quat& rot, float in)
{
    // Clip the convex box at the aperture; each half queries the level in its own room.
    const auto corners = boxCorners(lo, hi, rot, in);
    glm::vec3 points[8], boundsLo{1e9f}, boundsHi{-1e9f};
    for(int i = 0; i < 8; i++)
    {
        points[i] = at + world->toU(corners[i]);
        boundsLo = glm::min(boundsLo, points[i]); boundsHi = glm::max(boundsHi, points[i]);
    }
    portals::LightGate gate;
    const bool split = portals::splitBounds(boundsLo, boundsHi, gate);
    bool any = false;
    const auto query = [&](const b3Vec3* vertices, int count, const glm::vec3& centre) {
        if(count < 4) { return; }
        const b3ShapeProxy proxy{vertices, count, 0.f};
        b3World_OverlapShape(world->id, world->toM(centre), &proxy, levelFilter(),
            [](b3ShapeId, void* context) { *static_cast<bool*>(context) = true; return false; }, &any);
    };
    if(!split) { query(corners.data(), 8, at); return any; }
    for(int half = 0; half < 2 && !any; half++)
    {
        b3Vec3 clipped[20]; int count = 0;
        const glm::vec3 centre = half ? gate.turn * (at - gate.from) + gate.to : at;
        const auto append = [&](glm::vec3 p) {
            if(half) { p = gate.turn * (p - gate.from) + gate.to; }
            clipped[count++] = world->toM(p - centre);
        };
        float distances[8];
        for(int i = 0; i < 8; i++)
        {
            distances[i] = glm::dot(gate.normal, points[i]) - gate.dist;
            if(half ? distances[i] <= 0.f : distances[i] >= 0.f) { append(points[i]); }
        }
        for(int i = 0; i < 8; i++) for(int axis = 0; axis < 3; axis++)
        {
            const int j = i ^ (1 << axis);
            if(j > i && distances[i] * distances[j] < 0.f)
            {
                append(glm::mix(points[i], points[j], distances[i] / (distances[i] - distances[j])));
            }
        }
        query(clipped, count, centre);
    }
    return any;
}

} // namespace detail

bool holdClear(int num, const glm::vec3& fromPos, const glm::quat& fromRot, glm::vec3& toPos, glm::quat& toRot)
{
    if(!world || num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[static_cast<size_t>(num)];
    glm::vec3 lo, hi;
    ownBox(s, lo, hi);
    if(B3_IS_NULL(s.body) || (s.kind != Kind::Held && s.kind != Kind::Prop) || hi.x <= lo.x)
    {
        return false;
    }
    // Where its box may be: clear of the level (a quarter of a unit inside its surface: touching is clear), or, already
    // in it where it starts (taken from the floor it lies a little in), no deeper than there (coarsely, by half units).
    // Overlap tests rather than a cast along the move: a box cast starting within its rounding of a pillar's edge missed
    // it, and the box went in.
    const auto depth = [&](const glm::vec3& at, const glm::quat& rot) {
        if(!boxInLevel(lo, hi, at, rot, 0.25f))
        {
            return 0.f;
        }
        float d = 0.5f;
        while(d < 4.f && boxInLevel(lo, hi, at, rot, d + 0.25f))
        {
            d += 0.5f;
        }
        return d;
    };
    // Its new turn where it is: deeper into the level, it keeps the old one.
    const float start = depth(fromPos, fromRot);
    bool blocked = false;
    if(toRot != fromRot && depth(fromPos, toRot) > start)
    {
        toRot = fromRot;
        blocked = true;
    }
    const auto fits = [&](const glm::vec3& at) {
        return start == 0.f ? !boxInLevel(lo, hi, at, toRot, 0.25f) : depth(at, toRot) <= start;
    };
    // As far along the move as it fits (halved eight times: a hundredth of a 32 unit step); stopped, the rest of the
    // move along each axis in turn: it slides along what it met.
    glm::vec3 at = fromPos;
    const auto advance = [&](const glm::vec3& to) {
        if(fits(to))
        {
            at = to;
            return true;
        }
        float good = 0.f, bad = 1.f;
        for(int k = 0; k < 8; k++)
        {
            const float mid = 0.5f * (good + bad);
            (fits(at + (to - at) * mid) ? good : bad) = mid;
        }
        at += (to - at) * good;
        return false;
    };
    if(!advance(toPos))
    {
        blocked = true;
        for(int axis = 0; axis < 3; axis++)
        {
            glm::vec3 to = at;
            to[axis] = toPos[axis];
            advance(to);
        }
        if(vr_debug_carry.value >= 2.f)
        {
            Con_Printf("carry2h: %d meets the level, %.1f units short\n", num, glm::distance(toPos, at));
        }
    }
    toPos = at;
    return blocked;
}

bool ropeOverlaps(const glm::vec3& at, float radius, int skipA, int skipB)
{
    if(!world)
    {
        return false;
    }
    const b3Vec3 zero = b3Vec3_zero;
    const b3ShapeProxy proxy{&zero, 1, radius / world->m2u};
    RopeQuery q{skipA, skipB, nullptr, false};
    b3World_OverlapShape(world->id, world->toM(at), &proxy, ropeFilter(),
        [](b3ShapeId shape, void* context) {
            RopeQuery& rq = *static_cast<RopeQuery*>(context);
            const int num = numOf(shape);
            if(num != 0 && (num == rq.skipA || num == rq.skipB))
            {
                return true; // (go on)
            }
            rq.any = true;
            return false;
        },
        &q);
    return q.any;
}

// The profiler's counts (vr_profile_report), read at a frame's end while it collects.
void profileCounts(int& bodies, int& awake, int& contacts)
{
    bodies = awake = contacts = 0;
    if(!world)
    {
        return;
    }
    const b3Counters c = b3World_GetCounters(world->id);
    bodies = c.bodyCount;
    awake = b3World_GetAwakeBodyCount(world->id);
    for(const int n : c.colorCounts) // the solver's constraints: the touching contacts of awake bodies (and joints)
    {
        contacts += n;
    }
}

void profileAwakeKinds(AwakeKinds& out)
{
    out = {};
    if(!world || !sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    const int n = glm::min(qcvm->num_edicts, static_cast<int>(world->slots.size()));
    for(int i = 1; i < n; i++)
    {
        const Slot& s = world->slots[i];
        const edict_t* e = EDICT_NUM(i);
        if(e->free || B3_IS_NULL(s.body))
        {
            continue;
        }
        if(s.ragdoll >= 0 && s.ragdoll < static_cast<int>(world->ragdolls.size()))
        {
            const RagdollBodies& r = world->ragdolls[s.ragdoll];
            const bool living = e->v.deadflag == 0.f && e->v.health > 0.f;
            int awake = 0;
            for(int b = 0; b < r.count; b++)
            {
                awake += !partCut(r, b) && b3Body_IsValid(r.body[b]) && b3Body_IsAwake(r.body[b]) ? 1 : 0;
            }
            (living ? out.ragdollsLiving : out.ragdollsDead)++;
            (living ? out.ragLivingAwake : out.ragDeadAwake) += awake;
            continue;
        }
        if(!b3Body_IsAwake(s.body))
        {
            continue;
        }
        const char* model = s.model ? s.model->name : "";
        if(s.kind == Kind::Prop && (strstr(model, "gib") || strstr(model, "/h_")))
        {
            out.gibsAwake++;
        }
        else if(s.kind == Kind::Prop || s.kind == Kind::Corpse)
        {
            out.propsAwake++;
        }
        else
        {
            out.otherAwake++;
        }
    }
}

int nearestProp()
{
    if(!world || !sv.active || svs.maxclients < 1)
    {
        return 0;
    }
    const progs::ServerVm vm;
    const edict_t* player = progs::playerEdict();
    float best = 1e9f;
    int found = 0;
    for(int i = 1; i < qcvm->num_edicts && i < static_cast<int>(world->slots.size()); i++)
    {
        const edict_t* e = EDICT_NUM(i);
        if(!e->free && world->slots[i].kind == Kind::Prop)
        {
            const float d = glm::distance(vec(e->v.origin), vec(player->v.origin));
            if(d < best)
            {
                best = d;
                found = i;
            }
        }
    }
    return found;
}

bool isRagdoll(int num)
{
    return ragdollOf(num) != nullptr;
}

bool activeKind(const edict_t* ent)
{
    return ent && !ent->free && activeClassOf(ent) != nullptr;
}

const ragactive::State* activeState(int num)
{
    const RagdollBodies* r = ragdollOf(num);
    return r ? &r->active : nullptr;
}

bool canRagdoll(edict_t* ent)
{
    if(!world || vr_ragdoll.value < 1.f || !ent || ent->free || !hasFlag(ent, FL_MONSTER) || hasFlag(ent, FL_SWIM) ||
        NUM_FOR_EDICT(ent) <= svs.maxclients)
    {
        return false;
    }
    qmodel_t* model = modelOf(ent);
    return model && model->type == mod_alias && ragdoll::eligible(model) && ragdoll::rigFor(model) != nullptr;
}
} // namespace qvr::box3d
