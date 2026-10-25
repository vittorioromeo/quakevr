#include "vr_alloccount.h"
// vr_box3d_props.cpp -- props: their shapes, kinds, bodies made and followed, fed and written back, the entities' sync.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// The prop's shapes on `body`: its hull or its box.
void addPropShapes(edict_t* ent, int num, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi, b3BodyId body, bool held)
{
    if(!held && isChunk(model))
    {
        addChunkShape(ent, num, body);
        return;
    }
    b3ShapeDef def = shapeDef(num, held ? catHeld : catProp, held ? catProp | catCorpse : propMask);
    if(!held && isSolidProp(ent))
    {
        def.filter.categoryBits |= catSolid; // pushed and tipped by the hands' bodies
        def.filter.maskBits |= catHand | catReachWeapon;
    }
    else if(!held)
    {
        def.filter.maskBits |= catReach; // pushed and held up by the empty hands and the held weapons (syncReach)
    }
    def.density = densityOf(ent, model);
    def.baseMaterial.restitution = isSoft(ent, model) ? 0.f : CLAMP(0.f, vr_throw_restitution.value, 1.f);
    def.enableContactEvents = !held;
    def.enableHitEvents = !held;
    def.enablePreSolveEvents = true;
    if(isGrenade(model))
    {
        def.baseMaterial.restitution = grenadeRestitution;
        def.enableCustomFiltering = !held; // (shouldCollide: not with its thrower)
    }
    else if(!held && (isSmallGib(ent) || monsterDropAge(ent) >= 0.f))
    {
        def.enableCustomFiltering = true; // (shouldCollide: not the body it was made inside, until clear: noteBornInside)
    }
    b3HullData* hull = propHull(ent, model, lo, hi);
    const glm::vec3 half = (hi - lo) * 0.5f / world->m2u;
    // A Mass set for its model: the density that gives it (Held Object Offsets).
    if(const float mass = massSetting(ent, model); mass > 0.f)
    {
        const float volume = hull ? hull->volume : 8.f * half.x * half.y * half.z;
        def.density = mass / za::max(volume, 1e-6f);
    }
    else
    {
        def.density *= props::massScale(model); // its estimate's Mass x (Held Object Weights)
    }
    // A gun: its convex pieces, as dense as its one hull would weigh (its mass as before, its shape as drawn).
    if(hull && isGun(ent) && model->type == mod_alias)
    {
        const PropHullKey key{model, static_cast<int>(ent->v.frame), {lo.x, lo.y, lo.z, hi.x, hi.y, hi.z}};
        const za::Vector<b3HullData*>* pieces =
            &gunPieces(key, [&](za::Vector<glm::vec3>& out) { return held::drawnVertices(ent, out); });
        if(!pieces->empty())
        {
            float volume = 0.f;
            for(const b3HullData* piece : *pieces)
            {
                volume += piece->volume;
            }
            // Its seated magazine, solid with it (one more piece: the gun weighs as before).
            const b3HullData* mag = nullptr;
            if(const qmodel_t* magModel = held ? nullptr : seatedMagazine(ent, model))
            {
                za::Vector<glm::vec3>& magVertices = scratch.magVerts;
                if(held::drawnMagazineVertices(ent, magModel, magVertices))
                {
                    mag = magazineHull(magModel, -6, magVertices);
                }
            }
            if(mag)
            {
                volume += mag->volume;
            }
            b3ShapeDef pieceDef = def;
            pieceDef.density = def.density * hull->volume / za::max(volume, 1e-9f);
            for(const b3HullData* piece : *pieces)
            {
                b3CreateHullShape(body, &pieceDef, piece);
            }
            if(mag)
            {
                b3CreateHullShape(body, &pieceDef, mag);
            }
            return;
        }
    }
    if(hull)
    {
        // (One hull: its seated magazine one more, as the pieces'.)
        const b3HullData* mag = nullptr;
        if(const qmodel_t* magModel = held ? nullptr : seatedMagazine(ent, model))
        {
            za::Vector<glm::vec3>& magVertices = scratch.magVerts;
            if(held::drawnMagazineVertices(ent, magModel, magVertices))
            {
                mag = magazineHull(magModel, -6, magVertices);
            }
        }
        b3ShapeDef hullDef = def;
        if(mag)
        {
            hullDef.density = def.density * hull->volume / za::max(hull->volume + mag->volume, 1e-9f);
        }
        b3CreateHullShape(body, &hullDef, hull);
        if(mag)
        {
            b3CreateHullShape(body, &hullDef, mag);
        }
        return;
    }
    const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((lo + hi) * 0.5f));
    b3CreateHullShape(body, &def, &box.base);
}
} // namespace

[[nodiscard]] bool isRigid(edict_t* ent)
{
    const int ofs = fields().vr_rigid;
    return ofs >= 0 && fieldFloat(ent, ofs) != 0.f;
}
[[nodiscard]] Kind kindOf(edict_t* ent, int num, const za::Vector<uint8_t>& carried)
{
    const int movetype = static_cast<int>(ent->v.movetype);
    qmodel_t* model = modelOf(ent);
    if(!model)
    {
        return Kind::None;
    }
    if(num < static_cast<int>(carried.size()) && carried[num])
    {
        return Kind::Held;
    }
    const bool rigid = isRigid(ent);
    // Pinned by the toolgun (frozen, or in the physgun's beam): a kinematic body where its entity is, as a pickup hanging.
    if(rigid && !toolRecords.pinned.empty() && toolPinned(num))
    {
        return model->type == mod_alias || model->type == mod_brush ? Kind::Fixture : Kind::None;
    }
    if(rigid && (movetype == MOVETYPE_TOSS || movetype == MOVETYPE_BOUNCE))
    {
        return model->type == mod_alias || model->type == mod_brush ? Kind::Prop : Kind::None;
    }
    const int solid = static_cast<int>(ent->v.solid);
    // A pickup hanging in the air or on a rack, not a rigid body until a hand knocks it loose: the map's pickups
    // (triggers), and those that become objects once taken (armour, weapons, keys: touchable, still).
    if(!rigid && hasFlag(ent, FL_ITEM) && movetype != MOVETYPE_NOCLIP &&
        (solid == SOLID_TRIGGER || (solid == SOLID_NOT_BUT_TOUCHABLE && movetype == MOVETYPE_NONE)) &&
        (model->type == mod_alias || model->type == mod_brush))
    {
        return Kind::Fixture;
    }
    if(wantsRagdoll(ent, num) || (corpseMode() != 0 && isCorpse(ent, num)))
    {
        return Kind::Corpse;
    }
    if(num <= svs.maxclients)
    {
        return vr_box3d_player_push.value && solid != SOLID_NOT && ent->v.health > 0.f ? Kind::Player : Kind::None;
    }
    if(solid == SOLID_BSP && model->type == mod_brush)
    {
        return Kind::Mover;
    }
    if(solid == SOLID_BBOX || solid == SOLID_SLIDEBOX)
    {
        const glm::vec3 size = vec(ent->v.maxs) - vec(ent->v.mins);
        return size.x >= 2.f && size.y >= 2.f && size.z >= 2.f ? Kind::Actor : Kind::None; // (missiles have none)
    }
    return Kind::None;
}
[[nodiscard]] za::Array<float, 9> scaleFields(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const glm::vec3 a = fieldVec(ent, f.model_scale), b = fieldVec(ent, f.model_scale_origin), c = fieldVec(ent, f.model_offset);
    return {a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z};
}

namespace
{
[[nodiscard]] float playerRadius()
{
    return za::max(vr_box3d_player_radius.value, 1.f) * 0.01f; // metres
}

// What a prop's drawn box and Mass are made from besides its entity (localBox, massSetting): the weapon and prop
// settings (their changes counted), the scales and offsets every weapon's drawing follows, Quake VR's protocol, and the
// ragdolls' masses a monster's head weighs a share of (Mass -1: headPropMass; their sum). Its count goes up whenever any
// of them changes (updateShapeGeneration, once a frame).
struct ShapeInputs
{
    unsigned weapons{0}, props{0};
    bool quakevr{false};
    float scales[8]{};
    bool operator==(const ShapeInputs& o) const
    {
        return weapons == o.weapons && props == o.props && quakevr == o.quakevr && !memcmp(scales, o.scales, sizeof(scales));
    }
};
ShapeInputs shapeInputs;

[[nodiscard]] float headMassSum()
{
    float sum = 0.f;
    for(const RagdollClass& c : ragdollClasses)
    {
        sum += c.head ? qvr::box3d::headPropMass(c.head) : 0.f;
    }
    return sum;
}

void updateShapeGeneration()
{
    const ShapeInputs now{weapons::settingsGeneration(), props::settingsGeneration(),
        (cl.protocolflags & PRFL_QUAKEVR) || (sv.active && (sv.protocolflags & PRFL_QUAKEVR)),
        {vr_world_scale.value, vr_gunmodelscale.value, vr_gunmodely.value, vr_leg_holster_model_scale.value,
            vr_leg_holster_model_x_offset.value, vr_leg_holster_model_y_offset.value, vr_leg_holster_model_z_offset.value,
            headMassSum()}};
    if(!(now == shapeInputs))
    {
        shapeInputs = now;
        shapeGeneration++;
    }
}

// The hulls of a runtime brush's older revisions (moverHulls) freed once no mover body made with them is left (its
// slot made again with the new ones, or gone): one per piece per Move, Resize or Paint, kept otherwise to the map's end.
void freeRetiredHulls()
{
    za::Vector<World::RetiredHull>& retired = world->retiredHulls;
    const za::SizeT before = retired.size();
    for(za::SizeT i = 0; i < retired.size();)
    {
        const World::RetiredHull& r = retired[i];
        bool used = false;
        for(const Slot& s : world->slots)
        {
            if(s.kind == Kind::Mover && s.model == r.model && s.modelRev == r.rev && B3_IS_NON_NULL(s.body))
            {
                used = true;
                break;
            }
        }
        if(used)
        {
            i++;
            continue;
        }
        b3DestroyHull(r.hull);
        retired[i] = retired.back();
        retired.popBack();
    }
    if(vr_debug_box3d.value && retired.size() != before)
    {
        Con_Printf("box3d: %d retired hulls freed, %d kept\n", static_cast<int>(before - retired.size()),
            static_cast<int>(retired.size()));
    }
}

// Whether the body made for `s` no longer fits the entity (its model, frame, scale or box changed).
[[nodiscard]] bool stale(edict_t* ent, Slot& s)
{
    switch(s.kind)
    {
    case Kind::Prop:
    case Kind::Held:
    case Kind::Fixture:
    {
        qmodel_t* model = modelOf(ent);
        if(s.model != model || s.frame != static_cast<int>(ent->v.frame) || s.scale != scaleFields(ent))
        {
            return true;
        }
        if(s.kind == Kind::Prop && s.mag != seatedMagazine(ent, model))
        {
            return true; // (its magazine taken out or put in: vr_reload_mag_collide)
        }
        // The drawn box and the Mass: made from the settings (a weapon's box: weapons::modelTransform, the world
        // scale, the gun model scale, each weapon's own), which can change while it lies there, and the entity's box
        // and solidity (a model drawn as its box).
        const glm::vec3 mins = vec(ent->v.mins), maxs = vec(ent->v.maxs);
        const bool solid = isSolidProp(ent);
        if(s.checkedGeneration == shapeGeneration && s.checkedMins == mins && s.checkedMaxs == maxs && s.checkedSolid == solid)
        {
            return false;
        }
        if(s.massSetting != massSetting(ent, model) || s.massScale != props::massScale(model))
        {
            return true;
        }
        glm::vec3 lo, hi;
        localBox(ent, model, lo, hi);
        if(glm::any(glm::greaterThan(glm::abs(lo - s.mins), glm::vec3{0.01f})) ||
            glm::any(glm::greaterThan(glm::abs(hi - s.maxs), glm::vec3{0.01f})))
        {
            return true;
        }
        s.checkedGeneration = shapeGeneration;
        s.checkedMins = mins;
        s.checkedMaxs = maxs;
        s.checkedSolid = solid;
        return false;
    }
    case Kind::Mover: return s.model != modelOf(ent) || s.modelRev != brushes::revision(s.model);
    case Kind::Actor: return s.model != modelOf(ent) || s.mins != vec(ent->v.mins) || s.maxs != vec(ent->v.maxs);
    case Kind::Player: return s.radius != playerRadius() || s.mins != vec(ent->v.mins) || s.maxs != vec(ent->v.maxs);
    case Kind::Corpse:
    {
        const int num = NUM_FOR_EDICT(ent);
        if(s.ragdoll >= 0 || (wantsRagdoll(ent, num) && !roomRefused(num)))
        {
            // A ragdoll stays one while it is wanted (its frames, its box are its own); a corpse becomes one.
            return s.ragdoll < 0 || s.model != modelOf(ent) || !wantsRagdoll(ent, num);
        }
        glm::vec3 lo, hi;
        corpseBox(ent, lo, hi);
        return s.model != modelOf(ent) || s.corpseMode != corpseMode() || s.corpseMask != corpseMask() || s.mins != lo ||
               s.maxs != hi || (s.corpseFitted && s.frame != static_cast<int>(ent->v.frame)) ||
               s.massSetting != vr_corpse_collide_mass.value || s.corpseFriction != vr_corpse_collide_friction.value ||
               s.corpseDynamic != corpsePushable(ent);
    }
    default: return false;
    }
}

// The yaw a spinning pickup is drawn at (cl_main.c's bobjrotate for EF_ROTATE models: 100 degrees a second), by the
// server's clock (the client's trails it by at most a frame).
[[nodiscard]] float spinYaw()
{
    return anglemod(static_cast<float>(100.0 * sv.qcvm.time));
}
} // namespace

// A body's turn: an actor's hull turns with its yaw (Quake's box and the players' capsules don't turn), the rest
// with their angles.
[[nodiscard]] b3Quat rotationOf(edict_t* ent, const Slot& s)
{
    if(s.kind == Kind::Player || (s.kind == Kind::Actor && !s.hull) || (s.kind == Kind::Corpse && !s.corpseFitted))
    {
        return b3Quat_identity;
    }
    if(s.kind == Kind::Actor || s.kind == Kind::Corpse)
    {
        const float yaw[3] = {0.f, ent->v.angles[1], 0.f};
        return toB3(turnOf(yaw, s.brush));
    }
    if(s.kind == Kind::Fixture && s.spins)
    {
        const float angles[3] = {ent->v.angles[0], spinYaw(), ent->v.angles[2]};
        return toB3(turnOf(angles, s.brush));
    }
    return toB3(turnOf(ent->v.angles, s.brush));
}

namespace
{
// How far the prop's shapes (their corners, where its body is) are sunk into a floor: the deepest of its lower half's
// corners inside a solid, measured to the upward-facing surface above it, if that surface is below the prop's middle
// (not a table top it lies under). 0 if none is.
[[nodiscard]] float floorDepth(edict_t* ent, b3BodyId body)
{
    const b3WorldTransform xf = b3Body_GetTransform(body);
    b3ShapeId shapes[maxBodyShapes];
    const int n = b3Body_GetShapes(body, shapes, maxBodyShapes);
    za::Vector<glm::vec3>& corners = scratch.corners;
    corners.clear();
    float lo = 1e9f, hi = -1e9f;
    for(int i = 0; i < n; i++)
    {
        const b3HullData* hull = b3Shape_GetType(shapes[i]) == b3_hullShape ? b3Shape_GetHull(shapes[i]) : nullptr;
        const b3Vec3* points = hull ? b3GetHullPoints(hull) : nullptr;
        for(int k = 0; points && k < hull->vertexCount; k++)
        {
            const glm::vec3 p = world->toU(b3Add(b3RotateVector(xf.q, points[k]), xf.p));
            corners.pushBack(p);
            lo = za::min(lo, p.z);
            hi = za::max(hi, p.z);
        }
    }
    const float middle = (lo + hi) * 0.5f;
    float depth = 0.f;
    for(const glm::vec3& c : corners)
    {
        if(c.z > middle)
        {
            continue;
        }
        vec3_t at{c.x, c.y, c.z};
        if(!SV_Move(at, vec3_origin, vec3_origin, at, MOVE_NOMONSTERS, ent).startsolid)
        {
            continue;
        }
        vec3_t from{c.x, c.y, middle};
        const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, at, MOVE_NOMONSTERS, ent);
        if(!tr.startsolid && tr.fraction < 1.f && tr.plane.normal[2] > 0.7f)
        {
            depth = za::max(depth, tr.endpos[2] - c.z);
        }
    }
    return depth;
}

// A prop found sunk into a floor (made there: a weapon dropped where an ammo box stands on the floor, a map's item
// placed low; or come to rest in it) is lifted out, straight up, by how deep it is and a little more: Box3D's contacts
// against the world's one-sided triangles push a body out only while its corners are shallow, and a body made deep
// in a floor stays there. Returns the lift.
float liftOutOfFloor(edict_t* ent, Slot& s, const char* when)
{
    const float depth = floorDepth(ent, s.body);
    if(depth < 0.05f)
    {
        return 0.f;
    }
    const float lift = za::min(depth + 0.05f, 64.f);
    b3WorldTransform xf = b3Body_GetTransform(s.body);
    xf.p.z += lift / world->m2u;
    b3Body_SetTransform(s.body, xf.p, xf.q);
    ent->v.origin[2] += lift;
    s.origin.z += lift;
    SV_LinkEdict(ent, false);
    Con_DPrintf("box3d: %d %s %s %.2f units into the floor: lifted\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), when,
        depth);
    return lift;
}

void solidBox(edict_t* ent, const Slot& s);

// resized: a prop made again for its new Size (Held Object Offsets), woken to settle at it (a smaller one resting
// would hang in the air).
void createBody(edict_t* ent, int num, Slot& s, Kind kind, bool resized = false)
{
    qmodel_t* model = modelOf(ent);
    s.size = props::drawnSize(model);
    s.kind = kind;
    s.flight = false;
    s.model = model;
    s.modelRev = brushes::revision(model); // (a runtime brush's model is made again in place: vr_brushes.cpp)
    s.hull = kind == Kind::Actor && model && model->type == mod_alias ? actorHull(ent, model) : nullptr;
    s.frame = static_cast<int>(ent->v.frame);
    s.scale = scaleFields(ent);
    s.brush = model && model->type == mod_brush;
    s.mag = kind == Kind::Prop ? seatedMagazine(ent, model) : nullptr;
    s.spins = kind == Kind::Fixture && model &&
              ((model->flags & EF_ROTATE) || fieldFloatOr(ent, fields().vr_pickup_spin, 0.f) != 0.f); // (a weapon pickup drawn as its prop)
    s.massSetting = massSetting(ent, model);
    s.massScale = props::massScale(model);
    s.soft = model && isSoft(ent, model);
    s.chunk = isChunk(model);
    s.sound = physsound::materialOf(ent, model);
    s.origin = vec(ent->v.origin);
    s.angles = vec(ent->v.angles);
    s.born = qcvm->time;
    if(kind == Kind::Prop)
    {
        world->made.pushBack(num); // (noteThrows)
    }
    if(kind == Kind::Corpse)
    {
        s.corpseMode = corpseMode();
        s.corpseMask = corpseMask();
        s.corpseFitted = s.corpseMode >= 3;
        s.corpseDynamic = corpsePushable(ent);
        s.massSetting = vr_corpse_collide_mass.value;
        s.corpseFriction = vr_corpse_collide_friction.value;
    }

    b3BodyDef def = b3DefaultBodyDef();
    def.userData = userOf(num);
    def.position = world->toM(s.origin);
    def.rotation = rotationOf(ent, s);
    def.type = kind == Kind::Prop || (kind == Kind::Corpse && s.corpseDynamic) ? b3_dynamicBody : b3_kinematicBody;
    if(kind == Kind::Corpse && s.corpseDynamic)
    {
        // Lying as it lies (asleep until something meets it), upright: it slides and turns about its yaw (a box: not
        // even that, square to Quake's box), slowed as a heavy thing dragged.
        def.isAwake = false;
        def.motionLocks.angularX = true;
        def.motionLocks.angularY = true;
        def.motionLocks.angularZ = !s.corpseFitted;
        def.linearDamping = 0.5f;
        def.angularDamping = 2.f;
    }

    if(kind == Kind::Prop)
    {
        const bool resting = hasFlag(ent, FL_ONGROUND) && glm::length(vec(ent->v.velocity)) <= 1.f;
        def.isAwake = !resting || resized;
        s.gravityScale = gravityScaleOf(ent);
        def.gravityScale = s.gravityScale;
        def.angularDamping = za::max(vr_throw_spin_drag.value, 0.f);
        if(fields().vr_prop_mass >= 0 && fieldFloat(ent, fields().vr_prop_mass) > 0.f)
        {
            def.sleepThreshold = smallPropSleepThreshold; // a small gib: asleep sooner (they come by the dozen)
        }
    }
    s.body = b3CreateBody(world->id, &def);

    switch(kind)
    {
    case Kind::Prop:
    case Kind::Held:
    {
        glm::vec3 lo, hi;
        localBox(ent, model, lo, hi);
        s.mins = lo;
        s.maxs = hi;
        addPropShapes(ent, num, model, lo, hi, s.body, kind == Kind::Held);
        break;
    }
    case Kind::Mover:
    {
        const b3ShapeDef def2 = shapeDef(num, catMover, catProp | catCorpse);
        for(b3HullData* hull : moverHulls(model))
        {
            b3CreateHullShape(s.body, &def2, hull);
        }
        break;
    }
    case Kind::Actor:
    {
        s.mins = vec(ent->v.mins);
        s.maxs = vec(ent->v.maxs);
        const b3ShapeDef def2 = shapeDef(num, catActor, catProp);
        if(s.hull)
        {
            b3CreateHullShape(s.body, &def2, s.hull);
            break;
        }
        const glm::vec3 half = (s.maxs - s.mins) * 0.5f / world->m2u;
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((s.mins + s.maxs) * 0.5f));
        b3CreateHullShape(s.body, &def2, &box.base);
        break;
    }
    case Kind::Corpse: addCorpseShapes(ent, num, model, s); break;
    case Kind::Fixture:
    {
        glm::vec3 lo, hi;
        localBox(ent, model, lo, hi);
        s.mins = lo;
        s.maxs = hi;
        const b3ShapeDef def2 = shapeDef(num, catFixture, catProp);
        if(b3HullData* hull = propHull(ent, model, lo, hi))
        {
            b3CreateHullShape(s.body, &def2, hull);
            break;
        }
        const glm::vec3 half = (hi - lo) * 0.5f / world->m2u;
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((lo + hi) * 0.5f));
        b3CreateHullShape(s.body, &def2, &box.base);
        break;
    }
    case Kind::Player:
    {
        s.mins = vec(ent->v.mins);
        s.maxs = vec(ent->v.maxs);
        s.radius = playerRadius();
        const float r = s.radius;
        const float bottom = s.mins.z / world->m2u + r, top = za::max(s.maxs.z / world->m2u - r, bottom + 0.01f);
        const b3Capsule capsule{b3Vec3{0.f, 0.f, bottom}, b3Vec3{0.f, 0.f, top}, r};
        b3ShapeDef def2 = shapeDef(num, catPlayer, catProp);
        def2.enablePreSolveEvents = true; // (preSolve: not what it stands on, standOn)
        b3CreateCapsuleShape(s.body, &def2, &capsule);
        break;
    }
    default: break;
    }

    if(kind == Kind::Prop)
    {
        if(resized && static_cast<int>(ent->v.solid) == SOLID_BBOX)
        {
            solidBox(ent, s); // (its Quake box at its new size now, not when it next moves)
            SV_LinkEdict(ent, false);
        }
        (void)liftOutOfFloor(ent, s, "made");

        // Asleep in water deeper than it floats (a map's item under water, a saved game's): it rises (as the old
        // solver's asleep bodies are "lifted").
        if(def.isAwake == false && sv_gravity.value > 0.f)
        {
            const b3AABB box = b3Body_ComputeAABB(s.body);
            const float part = submerged(world->toU(b3Body_GetWorldCenter(s.body)), box.lowerBound.z * world->m2u, box.upperBound.z * world->m2u);
            if(waterDensity(ent) * part > 1.02f)
            {
                b3Body_SetAwake(s.body, true);
            }
        }
        s.asleep = !b3Body_IsAwake(s.body);
        s.velocity = vec(ent->v.velocity);
        s.spin = fieldVec(ent, fields().vr_spin);
        if(!s.asleep)
        {
            b3Body_SetLinearVelocity(s.body, world->toM(s.velocity));
            b3Body_SetAngularVelocity(s.body, b3v(s.spin));
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %d %s: a %s body%s\n", num, PR_GetString(ent->v.classname), kindName(kind),
            kind == Kind::Prop ? (s.asleep ? ", asleep" : ", awake") : "");
    }
}

// A kinematic body follows its entity: the velocity that gets it where the entity is by the end of the step (what
// rests on it rides it), or there at once for a jump (a teleport, a respawn).
void follow(edict_t* ent, Slot& s, float dt)
{
    // (A carried prop: held back with its hand, pressed into a prop with nowhere to go: notePress.)
    const glm::vec3 origin = vec(ent->v.origin) + (s.kind == Kind::Held ? heldPress(NUM_FOR_EDICT(ent)) : glm::vec3{0.f});
    const glm::vec3 angles = vec(ent->v.angles);
    const bool moved = origin != s.origin || angles != s.angles || s.spins;
    if(!moved && !b3Body_IsAwake(s.body))
    {
        return;
    }
    const b3Quat rot = rotationOf(ent, s);
    const float distance = glm::distance(origin, s.origin);
    if(distance > 64.f)
    {
        b3Body_SetTransform(s.body, world->toM(origin), rot);
        b3Body_SetLinearVelocity(s.body, b3Vec3_zero);
        b3Body_SetAngularVelocity(s.body, b3Vec3_zero);
    }
    else
    {
        // A player's body shoves props at most at vr_box3d_player_push_speed: it jumps the rest of its move, and
        // the props it then overlaps are eased out of it (Box3D's contact softness, a few metres a second), rather
        // than kicked ahead at a run's speed (Quake's 320 units a second is 8 m/s). Props don't stop players (they
        // are not solid in Quake's movement): walking into a stack pushes through it, not into a wall of boxes.
        // A monster's likewise (vr_box3d_monster_push_speed): its steps are a frame's jumps of 8-30 units (a dog's run,
        // a grunt's walk), a body tens of metres a second fast for that step: walking into a crate it kicked it at that,
        // and the crate broke and flung into it (NOTES.md e1m1_2026-10-01_02-45-57, 02-50-44).
        const float pushSpeed = s.kind == Kind::Player ? za::max(vr_box3d_player_push_speed.value, 0.1f)
                              : s.kind == Kind::Actor && vr_box3d_monster_push_speed.value > 0.f ? vr_box3d_monster_push_speed.value
                                                                                                  : 0.f;
        const float most = pushSpeed * world->m2u * dt;
        if(pushSpeed > 0.f && distance > most)
        {
            const glm::vec3 from = origin - (origin - s.origin) * (most / distance);
            b3Body_SetTransform(s.body, world->toM(from), rot);
        }
        b3Body_SetTargetTransform(s.body, b3WorldTransform{world->toM(origin), rot}, dt, true);
    }
    s.origin = origin;
    s.angles = angles;
}
} // namespace

// A prop: what QC did to it since the last frame goes in (a place, a velocity, a spin, a wake).
void feedProp(edict_t* ent, Slot& s)
{
    const glm::vec3 origin = vec(ent->v.origin), angles = vec(ent->v.angles);
    const glm::vec3 velocity = vec(ent->v.velocity), spin = fieldVec(ent, fields().vr_spin);
    bool wake = false;
    if(origin != s.origin || angles != s.angles)
    {
        b3Body_SetTransform(s.body, world->toM(origin), toB3(turnOf(ent->v.angles, s.brush)));
        wake = true;
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d moved by QC to %.1f %.1f %.1f\n", NUM_FOR_EDICT(ent), origin.x, origin.y, origin.z);
        }
    }
    if(velocity != s.velocity || spin != s.spin)
    {
        glm::vec3 v = velocity;
        const float speed = glm::length(v);
        if(speed > sv_maxvelocity.value && speed > 0.f)
        {
            v *= sv_maxvelocity.value / speed;
        }
        b3Body_SetAwake(s.body, true);
        b3Body_SetLinearVelocity(s.body, world->toM(v));
        b3Body_SetAngularVelocity(s.body, b3v(spin));
        wake = true;
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d pushed by QC: %.0f u/s, spin %.1f\n", NUM_FOR_EDICT(ent), glm::length(v), glm::length(spin));
        }
    }
    if(s.asleep && !hasFlag(ent, FL_ONGROUND))
    {
        wake = true; // QC took it off the ground
    }
    if(wake)
    {
        b3Body_SetAwake(s.body, true);
        s.asleep = false;
    }
    const float g = gravityScaleOf(ent);
    if(g != s.gravityScale)
    {
        b3Body_SetGravityScale(s.body, g);
        s.gravityScale = g;
    }
    s.origin = origin;
    s.angles = angles;
    s.velocity = velocity;
    s.spin = spin;
}

namespace
{
// The props about to get their first body this frame (kindOf's Prop: a rigid entity tossed or bouncing, not held, not
// pinned; not a chunk), whose hull (propHull's key) isn't made yet: each one's built on the pool now, before the loop
// below makes the bodies, so a gib burst's hulls (a monster's head, gibs, brains: 4-8 at once) are built side by side
// instead of one after another in the sync (VRTRACE_20261016: 13 frames of 6-7.5 ms). The same points and the same
// propHullOf: the same hulls. vr_box3d_hulls_ahead 0: each built as its body is made, as before.
void prebuildPropHulls(const za::Vector<uint8_t>& carried)
{
    if(vr_box3d_hulls_ahead.value == 0.f || !jobs::pool() || !jobs::parallel())
    {
        return;
    }
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(ent->free || (num < static_cast<int>(world->slots.size()) && world->slots[num].kind != Kind::None) ||
           (num < static_cast<int>(carried.size()) && carried[num]))
        {
            continue;
        }
        qmodel_t* model = modelOf(ent);
        const int movetype = static_cast<int>(ent->v.movetype);
        if(!model || (model->type != mod_alias && model->type != mod_brush) || isChunk(model) || !isRigid(ent) ||
           (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE) || (!toolRecords.pinned.empty() && toolPinned(num)))
        {
            continue;
        }
        glm::vec3 lo, hi;
        localBox(ent, model, lo, hi);
        const PropHullKey key{model, static_cast<int>(ent->v.frame), {lo.x, lo.y, lo.z, hi.x, hi.y, hi.z}};
        if(world->propHulls.contains(key) || world->pendingHulls.contains(key))
        {
            continue;
        }
        za::Vector<b3Vec3> points;
        if(!propHullPoints(ent, points))
        {
            continue;
        }
        const int budget = isWeaponLike(ent) ? 32 : 16;
        const float m2u = world->m2u;
        world->pendingHulls.emplace(key, jobs::async([points = ZA_MOVE(points), budget, m2u]() mutable {
            QVR_TRACY_SCOPE(tracyScope, "prop hull ahead");
            return propHullOf(points, budget, m2u);
        }));
        hullBuilds.posted++;
    }
}
} // namespace

// The hulls built ahead whose bodies weren't made after all (another kind this frame): kept for their key.
void keepPendingHulls()
{
    for(auto& [key, job] : world->pendingHulls)
    {
        b3HullData* hull = job.get();
        if(!world->propHulls.emplace(key, hull).second && hull)
        {
            b3DestroyHull(hull);
        }
    }
    world->pendingHulls.clear();
}

// The entities, every server frame in edict order: each one's body made, moved, fed or destroyed.
void syncEntities(float dt)
{
    const FieldOffsets& f = fields();
    za::Vector<uint8_t>& carried = scratch.carried;
    carried.clear();
    carried.resize(static_cast<size_t>(qcvm->num_edicts), 0);
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        if(player->free)
        {
            continue;
        }
        for(const int ofs : {f.mainhand_held, f.offhand_held})
        {
            const int held = ofs >= 0 ? fieldInt(player, ofs) : 0;
            if(held > 0)
            {
                const int n = NUM_FOR_EDICT(PROG_TO_EDICT(held));
                if(n > svs.maxclients && n < qcvm->num_edicts &&
                   !(n < static_cast<int>(world->slots.size()) && world->slots[n].ragdoll >= 0)) // (a limb held: grabs)
                {
                    carried[n] = 1;
                }
            }
        }
    }

    updateShapeGeneration();
    if(!world->retiredHulls.empty())
    {
        freeRetiredHulls();
    }
    watchCorpses();
    settleCorpses();
    if(const int loadout = heldLoadout(); !world->gunsPrepared || loadout != world->gunsLoadout)
    {
        // (The player's hands and holsters are filled as it enters the map, after this first frame: made again then.)
        world->gunsPrepared = true;
        world->gunsLoadout = loadout;
        prepareHeldGuns();
    }
    if(vr_ragdoll.value >= 1.f && !world->ragdollsWarmed)
    {
        // The rigs made now (tens of milliseconds each, as the map starts), not as the first grunt dies.
        world->ragdollsWarmed = true;
        za::Vector<qmodel_t*> rigged;
        for(int i = 1; i < MAX_MODELS && sv.models[i]; i++)
        {
            if(ragdoll::eligible(sv.models[i]))
            {
                rigged.pushBack(sv.models[i]);
            }
        }
        const double w0 = Sys_DoubleTime();
        ragdoll::warmRigs(rigged.data(), static_cast<int>(rigged.size()));
        VR_TimeAdd("ragdoll: the map's rigs (warmRigs)", Sys_DoubleTime() - w0); // load timing (vr_startup_times)
        Con_DPrintf("ragdoll: the map's rigs (%d models) ready in %.1f ms\n", static_cast<int>(rigged.size()),
            (Sys_DoubleTime() - w0) * 1000.0);
    }
    // The corpses refused a ragdoll once (a full pool) are forgotten once they are something else.
    za::vectorEraseIf(world->roomRefused, [](int n) {
        return n >= qcvm->num_edicts || EDICT_NUM(n)->free || slotOf(n).kind != Kind::Corpse;
    });
    prebuildPropHulls(carried);
    world->syncingEntities = true;
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        Slot& s = slotOf(num);
        const Kind want = ent->free ? Kind::None : kindOf(ent, num, carried);
        if(want != s.kind || (want != Kind::None && stale(ent, s)))
        {
            const bool resized = want == Kind::Prop && s.kind == Kind::Prop && s.size != props::drawnSize(modelOf(ent));
            destroyBody(s);
            if(want == Kind::Corpse && wantsRagdoll(ent, num) && createRagdoll(ent, num, s))
            {
                writeRagdoll(ent, s); // (drawn as one at once)
                continue;
            }
            if(want != Kind::None)
            {
                createBody(ent, num, s, want, resized);
            }
            continue;
        }
        switch(s.kind)
        {
        case Kind::Prop: feedProp(ent, s); break;
        case Kind::Held:
        case Kind::Mover:
        case Kind::Actor:
        case Kind::Player:
        case Kind::Fixture: follow(ent, s, dt); break;
        case Kind::Corpse:
            if(s.ragdoll >= 0)
            {
                feedRagdoll(ent, s);
                driveShove(world->ragdolls[static_cast<za::SizeT>(s.ragdoll)], dt);
                shockRagdoll(ent, s, dt);
                driveActive(s, dt);
            }
            else if(s.corpseDynamic)
            {
                feedCorpse(ent, s);
            }
            else
            {
                follow(ent, s, dt);
            }
            break;
        default: break;
        }
    }
    world->syncingEntities = false;
    keepPendingHulls();
    // The corpses that found the ragdoll pool full: QC makes room for each, now that the loop is done (a refusal is not
    // asked again); their ragdolls are made next frame (stale: a corpse that wants one).
    for(za::SizeT i = 0; i < world->roomWanted.size(); i++)
    {
        const int num = world->roomWanted[i];
        if(num >= qcvm->num_edicts || EDICT_NUM(num)->free || roomRefused(num))
        {
            continue;
        }
        if(ragdollCount() < static_cast<int>(za::max(1.f, vr_ragdoll_max.value)))
        {
            continue; // (room made for an earlier one, or a ragdoll gone meanwhile)
        }
        if(!makeRagdollRoom(EDICT_NUM(num)))
        {
            world->roomRefused.pushBack(num);
        }
    }
    world->roomWanted.clear();
    for(size_t num = static_cast<size_t>(qcvm->num_edicts); num < world->slots.size(); num++)
    {
        if(world->slots[num].kind != Kind::None)
        {
            destroyBody(world->slots[num]);
        }
    }
}

namespace
{
// A prop's state from its body into its entity (awake, or just fallen asleep).
// A solid prop's Quake box (SOLID_BBOX: what shots, players and monsters meet) round its drawn box as it is turned now,
// so a tipped explosive box is hit and walked into where it lies.
void solidBox(edict_t* ent, const Slot& s)
{
    const glm::mat3 axes = held::axesFromAngles(ent->v.angles, s.brush);
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 corner{(i & 1) ? s.maxs.x : s.mins.x, (i & 2) ? s.maxs.y : s.mins.y, (i & 4) ? s.maxs.z : s.mins.z};
        const glm::vec3 p = axes * corner;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    store(lo, ent->v.mins);
    store(hi, ent->v.maxs);
    store(hi - lo, ent->v.size);
}
} // namespace

void writeProp(edict_t* ent, Slot& s)
{
    const FieldOffsets& f = fields();
    // Fallen out of the world (made inside a wall, where the mesh has no inside to push it out of, and keepInWorld
    // could not put it back): it stops there, asleep, instead of falling for ever.
    if(b3Body_IsAwake(s.body) && b3Body_GetPosition(s.body).z * world->m2u < world->map->mins[2] - 1024.f)
    {
        b3Body_SetLinearVelocity(s.body, b3Vec3_zero);
        b3Body_SetAngularVelocity(s.body, b3Vec3_zero);
        b3Body_SetAwake(s.body, false);
        Con_DPrintf("box3d: %d %s fell out of the world, stopped\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname));
    }
    bool asleep = !b3Body_IsAwake(s.body);
    // Just come to rest: not in a floor (a safety net; Box3D's contacts keep a landing body out of it).
    if(asleep && !s.asleep && liftOutOfFloor(ent, s, "come to rest") > 0.f)
    {
        asleep = !b3Body_IsAwake(s.body);
    }
    const b3WorldTransform xf = b3Body_GetTransform(s.body);
    const glm::vec3 origin = world->toU(xf.p);
    glm::vec3 velocity{0.f}, spin{0.f};
    if(!asleep)
    {
        velocity = world->toU(b3Body_GetLinearVelocity(s.body));
        spin = glmv(b3Body_GetAngularVelocity(s.body));
    }
    const glm::vec3 was = vec(ent->v.origin);
    store(origin, ent->v.origin);
    held::anglesFromAxes(glm::mat3_cast(fromB3(xf.q)), ent->v.angles, s.brush);
    if(static_cast<int>(ent->v.solid) == SOLID_BBOX)
    {
        solidBox(ent, s); // (a solid prop, an explosive box: Quake's box round it as it turned)
    }
    store(velocity, ent->v.velocity);
    VectorCopy(vec3_origin, ent->v.avelocity);
    setFieldVec(ent, f.vr_spin, spin);
    setFlag(ent, FL_ONGROUND, asleep);
    if(asleep)
    {
        ent->v.groundentity = EDICT_TO_PROG(supportOf(s));
        if(vr_debug_box3d.value)
        {
            edict_t* ground = PROG_TO_EDICT(ent->v.groundentity);
            Con_Printf("box3d: %d %s asleep at %.1f %.1f %.1f on %s\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), origin.x,
                origin.y, origin.z, ground == qcvm->edicts ? "the world" : PR_GetString(ground->v.classname));
        }
    }
    s.origin = origin;
    s.angles = vec(ent->v.angles);
    s.velocity = velocity;
    s.spin = spin;
    s.asleep = asleep;
    SV_LinkEdict(ent, !s.chunk); // (its triggers; an explosion's chunk has none to touch, nor splashes: silent, as before)
    if(!ent->free && origin != was && !s.chunk)
    {
        SV_CheckWaterTransition(ent);
    }
    if(vr_debug_box3d.value >= 2 && !asleep)
    {
        Con_Printf("box3d: %d at %.1f %.1f %.1f, %.0f u/s (%.1f %.1f %.1f), spin %.1f rad/s\n", NUM_FOR_EDICT(ent), origin.x, origin.y,
            origin.z, glm::length(velocity), velocity.x, velocity.y, velocity.z, glm::length(spin));
    }
}
} // namespace box3d::detail
