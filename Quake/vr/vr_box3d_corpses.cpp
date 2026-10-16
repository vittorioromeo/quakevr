#include "vr_alloccount.h"
// vr_box3d_corpses.cpp -- corpses lying still: when one is, its box or hulls, fed, written back and settled.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// ----------------------------------------------------------------------------
// Corpses in the physics (vr_corpse_collide; ROUND21.md, "Corpse collision"). A dead monster lying still (the QC's
// corpse, vr_corpse 2: VR_Corpse_Arm; or any dead monster whose frame has stopped changing: the bosses, vr_corpse_gib 0)
// is a body that props, thrown things and the hands' bodies meet: Fixed, a kinematic body following its entity (nothing
// moves it); Pushable, a heavy dynamic body (vr_corpse_collide_mass, vr_corpse_collide_friction) that slides on the
// floor, upright (it only turns about its yaw), pushed by what meets it as by mass (limitPushes: the hands, held things;
// a player walking into it shoves it, shoveBumped), its entity following it. Its shape: its lying box (Quake's, no
// taller than corpseTop), or hulls fitted to its pose (corpseHulls: its drawn frame cut in pieces along its length).
// The entity stays touchable and not solid: shots, missiles, blows and flames meet it as before (MOVE_HITGIBS, precise
// hits). Players and monsters meet it in Quake's moves (VR_CorpseBox: vr_corpse_collide_player, _monsters).
// One body per corpse, as a ragdoll's would be a few joined ones: the kind, the categories and the passes (corpseMask,
// shouldCollide, noteCorpseInside) are the ragdoll's to keep.
constexpr float corpseSettle = 0.5f; // s: a dead monster's frame unchanged this long lies still (no vr_corpse 2)
constexpr float corpsePiece = 24.f;  // units: the pieces of a fitted shape about this long (corpseHulls)
} // namespace

[[nodiscard]] int corpseMode()
{
    return CLAMP(0, static_cast<int>(vr_corpse_collide.value), 4);
}

// A dead monster, not solid, not a gib or head (those are props), not thrown about (a bouncing body).
[[nodiscard]] bool deadMonster(edict_t* ent)
{
    if(!hasFlag(ent, FL_MONSTER) || ent->v.health > 0.f)
    {
        return false;
    }
    const int solid = static_cast<int>(ent->v.solid), movetype = static_cast<int>(ent->v.movetype);
    if((solid != SOLID_NOT && solid != SOLID_NOT_BUT_TOUCHABLE) || movetype == MOVETYPE_BOUNCE ||
        movetype == MOVETYPE_NOCLIP || movetype == MOVETYPE_FLYMISSILE || movetype == MOVETYPE_PUSH)
    {
        return false;
    }
    if(isRigid(ent) || fieldFloatOr(ent, fields().vr_gib, 0.f) != 0.f)
    {
        return false;
    }
    const qmodel_t* model = modelOf(ent);
    return model && model->type == mod_alias;
}

// A living monster a shove knocked down (QC vr_knockdown.qc: .vr_knockdown 1, touchable and not solid): a ragdoll as a
// dead one's ("Knockdowns").
[[nodiscard]] bool knockedDown(edict_t* ent)
{
    if(!hasFlag(ent, FL_MONSTER) || ent->v.health <= 0.f || fields().vr_knockdown < 0 ||
        fieldFloat(ent, fields().vr_knockdown) != 1.f)
    {
        return false;
    }
    const int solid = static_cast<int>(ent->v.solid);
    const qmodel_t* model = modelOf(ent);
    return (solid == SOLID_NOT || solid == SOLID_NOT_BUT_TOUCHABLE) && model && model->type == mod_alias;
}

// Once a frame, before the bodies: each dead monster's frame and model, and since when it has had them.
void watchCorpses()
{
    auto& watch = world->corpseWatch;
    watch.resize(static_cast<size_t>(qcvm->num_edicts));
    world->motion.resize(static_cast<size_t>(qcvm->num_edicts));
    const bool on = corpseMode() != 0 || vr_ragdoll.value >= 1.f; // (ragdolls: when a frame changed, createRagdoll)
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        // A monster's motion (a beheaded one's ragdoll goes on with it: ownMotion). A teleport (or a respawn) starts it
        // again.
        World::MonsterMotion& m = world->motion[static_cast<size_t>(num)];
        edict_t* const me = EDICT_NUM(num);
        if(vr_ragdoll.value < 1.f || me->free || !hasFlag(me, FL_MONSTER))
        {
            m = World::MonsterMotion{};
        }
        else
        {
            const glm::vec3 at = vec(me->v.origin);
            if(m.n == 0 || at != m.at[0])
            {
                if(m.n > 0 && glm::length(at - m.at[0]) > 64.f)
                {
                    m.n = 0;
                }
                m.at[2] = m.at[1];
                m.at[1] = m.at[0];
                m.at[0] = at;
                m.when[2] = m.when[1];
                m.when[1] = m.when[0];
                m.when[0] = qcvm->time;
                m.n = za::min(m.n + 1, 3);
            }
            m.vel = vec(me->v.velocity);
        }
    }
    if(vr_ragdoll_active.value != 0.f)
    {
        for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
        {
            edict_t* const me = EDICT_NUM(num);
            if(!me->free && hasFlag(me, FL_MONSTER) && me->v.health > 0.f)
            {
                World::MonsterMotion& m = world->motion[static_cast<size_t>(num)];
                m.alive = ownMotion(num);
                m.aliveAt = qcvm->time;
            }
        }
    }
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        World::CorpseWatch& w = watch[static_cast<size_t>(num)];
        edict_t* ent = EDICT_NUM(num);
        if(!on || ent->free || !deadMonster(ent))
        {
            w = World::CorpseWatch{};
            if(on && !ent->free && hasFlag(ent, FL_MONSTER) && ent->v.health <= 0.f)
            {
                w.dying = true; // (dying, still solid: its frame and where it is)
                w.frame = ent->v.frame;
                w.model = ent->v.modelindex;
                w.origin = vec(ent->v.origin);
            }
            continue;
        }
        w.last = w.since < 0.0 && !w.dying ? vec(ent->v.origin) : w.origin;
        w.origin = vec(ent->v.origin);
        if(w.since < 0.0 || w.frame != ent->v.frame || w.model != ent->v.modelindex)
        {
            const bool seen = w.since >= 0.0 || (w.dying && w.frame != ent->v.frame);
            w.changed = seen && w.model == ent->v.modelindex ? qcvm->time : -1.0;
            w.dying = false;
            w.frame = ent->v.frame;
            w.model = ent->v.modelindex;
            w.since = qcvm->time;
        }
    }
}

// Monster `num`'s own motion (units/s): its origin's over its last two changes (a walking monster steps every 0.1 s:
// two steps; one moved by physics, two frames; within 0.3 s: a first step after standing still, over a step's 0.1 s),
// none if it hasn't moved for 0.2 s (it stood still); at most 800.
[[nodiscard]] glm::vec3 ownMotion(int num)
{
    if(num < 0 || num >= static_cast<int>(world->motion.size()))
    {
        return glm::vec3{0.f};
    }
    const World::MonsterMotion& m = world->motion[static_cast<size_t>(num)];
    if(m.n < 2 || qcvm->time - m.when[0] > 0.2)
    {
        return glm::vec3{0.f};
    }
    int k = m.n - 1;
    while(k > 1 && m.when[0] - m.when[static_cast<za::SizeT>(k)] > 0.3)
    {
        k--;
    }
    const double span = za::min(m.when[0] - m.when[static_cast<za::SizeT>(k)], k == 1 ? 0.1 : 1.0);
    if(span < 1e-3)
    {
        return glm::vec3{0.f};
    }
    const glm::vec3 v = (m.at[0] - m.at[static_cast<za::SizeT>(k)]) / static_cast<float>(span);
    const float speed = glm::length(v);
    return speed > 800.f ? v * (800.f / speed) : v;
}

// Monster `num`'s own motion as it died (units/s): the last frame it was alive's, if that was within 0.6 s (its death
// frames until it stops being solid); none else.
[[nodiscard]] glm::vec3 deathMotion(int num)
{
    if(num < 0 || num >= static_cast<int>(world->motion.size()))
    {
        return glm::vec3{0.f};
    }
    if(const ragtest::Force* f = ragtest::forced(num); f && (f->walk != glm::vec3{0.f} || f->still))
    {
        return f->walk; // (a test's stumble, or a monster it moved: vr_ragdoll_act)
    }
    const World::MonsterMotion& m = world->motion[static_cast<size_t>(num)];
    return m.aliveAt >= 0.0 && qcvm->time - m.aliveAt < 0.6 ? m.alive : glm::vec3{0.f};
}

// Whether `ent` is a corpse lying still: the QC's (vr_corpse 2), or a dead monster (not one the QC still watches dying,
// vr_corpse 1) whose frame has stayed for corpseSettle, on the ground (or swimming, or stopped).
[[nodiscard]] bool isCorpse(edict_t* ent, int num)
{
    if(num <= svs.maxclients || num >= static_cast<int>(world->corpseWatch.size()))
    {
        return false;
    }
    const World::CorpseWatch& w = world->corpseWatch[static_cast<size_t>(num)];
    if(w.since < 0.0)
    {
        return false;
    }
    const float state = fieldFloatOr(ent, fields().vr_corpse, 0.f);
    if(state >= 2.f)
    {
        return true;
    }
    return state == 0.f && qcvm->time - w.since >= corpseSettle &&
           (hasFlag(ent, FL_ONGROUND) || hasFlag(ent, FL_SWIM) || glm::length(vec(ent->v.velocity)) < 1.f);
}

// Its lying box (units, from its origin): Quake's (VR_Corpse_Arm's, low and wide), no taller than corpseTop.
void corpseBox(edict_t* ent, glm::vec3& lo, glm::vec3& hi)
{
    lo = vec(ent->v.mins);
    hi = vec(ent->v.maxs);
    hi.z = za::min(hi.z, lo.z + corpseTop);
    hi = glm::max(hi, lo + glm::vec3{2.f});
}

// The categories a corpse's body meets: the level and the doors (a pushable one rests and slides on them), the loose
// props (and thrown ones: shouldCollide sorts them, vr_corpse_collide_props and _thrown), what the hands hold and the
// hands' bodies (vr_corpse_collide_held: they push a pushable one; both kinematic, a fixed one and they pass).
[[nodiscard]] uint64_t corpseMask()
{
    uint64_t mask = catWorld | catMover;
    if(vr_corpse_collide_props.value || vr_corpse_collide_thrown.value)
    {
        mask |= catProp;
    }
    if(vr_corpse_collide_held.value)
    {
        mask |= catHeld | catReach;
    }
    return mask;
}

// A shape fitted to a corpse's pose: its drawn frame (in its axes, from its origin; nothing below its box's floor) cut
// across its length (its vertices' main horizontal axis) into pieces about corpsePiece units long, each piece's convex
// hull (a unit over into the next: no gaps at the cuts). Made once per model, frame and scale. Empty: its box.
[[nodiscard]] const za::Vector<b3HullData*>& corpseHulls(edict_t* ent, qmodel_t* model)
{
    const float floor = ent->v.mins[2] + 0.25f;
    const za::Array<float, 9> scale = scaleFields(ent);
    const PropHullKey key{model, static_cast<int>(ent->v.frame), {scale[0], scale[1], scale[2], scale[3], scale[4], floor}};
    auto it = world->corpseHulls.find(key);
    if(it != world->corpseHulls.end())
    {
        return it->second;
    }
    za::Vector<b3HullData*> hulls;
    za::Vector<glm::vec3>& vertices = scratch.actorVerts;
    if(held::drawnVertices(ent, vertices) && vertices.size() >= 4)
    {
        // Its main horizontal axis: the larger eigenvector of the vertices' spread (a 2x2 covariance).
        glm::vec2 mean{0.f};
        for(glm::vec3& v : vertices)
        {
            v.z = za::max(v.z, floor);
            mean += glm::vec2{v.x, v.y};
        }
        mean /= static_cast<float>(vertices.size());
        float xx = 0.f, xy = 0.f, yy = 0.f;
        for(const glm::vec3& v : vertices)
        {
            const glm::vec2 d = glm::vec2{v.x, v.y} - mean;
            xx += d.x * d.x;
            xy += d.x * d.y;
            yy += d.y * d.y;
        }
        const float angle = 0.5f * za::atan2(2.f * xy, xx - yy);
        const glm::vec2 axis{za::cos(angle), za::sin(angle)};
        float from = 1e9f, to = -1e9f;
        for(const glm::vec3& v : vertices)
        {
            const float t = glm::dot(glm::vec2{v.x, v.y}, axis);
            from = za::min(from, t);
            to = za::max(to, t);
        }
        const int pieces = CLAMP(1, static_cast<int>((to - from) / corpsePiece + 0.5f), 4);
        const float length = (to - from) / static_cast<float>(pieces);
        za::Vector<b3Vec3> points;
        for(int p = 0; p < pieces; p++)
        {
            const float a = from + length * static_cast<float>(p) - 1.f, b = from + length * static_cast<float>(p + 1) + 1.f;
            points.clear();
            for(const glm::vec3& v : vertices)
            {
                const float t = glm::dot(glm::vec2{v.x, v.y}, axis);
                if(t >= a && t <= b)
                {
                    points.pushBack(world->toM(v));
                }
            }
            b3HullData* hull = points.size() >= 4 ? b3CreateHull(points.data(), static_cast<int>(points.size()), 16) : nullptr;
            if(hull && hull->innerRadius * world->m2u < 0.5f)
            {
                b3DestroyHull(hull); // (flat: a piece of a thin limb)
                hull = nullptr;
            }
            if(hull)
            {
                hulls.pushBack(hull);
            }
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: corpse %s frame %d: %d hulls fitted\n", model->name, key.frame, static_cast<int>(hulls.size()));
    }
    return world->corpseHulls.emplace(key, static_cast<za::Vector<b3HullData*>&&>(hulls)).first->second;
}

// Whether a corpse is a dynamic body (Pushable: vr_corpse_collide 2 and 4): not one floating in the water (a fish's, an
// eel's: nothing lifts it; it stays where it floats).
[[nodiscard]] bool corpsePushable(edict_t* ent)
{
    const int mode = corpseMode();
    return (mode == 2 || mode == 4) && !hasFlag(ent, FL_SWIM);
}

// The corners of hull shape `shape`, each moved in by `sink` (m) along each axis (not past its middle), turned by `xf`'s
// rotation: into `points`, their count.
int shrunkCorners(b3ShapeId shape, const b3WorldTransform& xf, float sink, za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS>& points)
{
    const b3HullData* hull = b3Shape_GetHull(shape);
    const b3Vec3* p = b3GetHullPoints(hull);
    const b3Vec3 c = hull->center;
    const int n = za::min(hull->vertexCount, static_cast<int>(points.size()));
    for(int v = 0; v < n; v++)
    {
        b3Vec3 q = p[v];
        q.x = q.x > c.x ? za::max(c.x, q.x - sink) : za::min(c.x, q.x + sink);
        q.y = q.y > c.y ? za::max(c.y, q.y - sink) : za::min(c.y, q.y + sink);
        q.z = q.z > c.z ? za::max(c.z, q.z - sink) : za::min(c.z, q.z + sink);
        points[static_cast<size_t>(v)] = b3RotateVector(xf.q, q);
    }
    return n;
}

// What `filter` passes overlapping each hull shape of `body` shrunk by `sink` (shrunkCorners): `fcn` called with
// `context` for each, as b3World_OverlapShape calls it.
void overlapShrunk(b3BodyId body, float sink, b3QueryFilter filter, b3OverlapResultFcn* fcn, void* context)
{
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(body);
    za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int k = 0; k < count; k++)
    {
        if(b3Shape_GetType(shapes[k]) != b3_hullShape)
        {
            continue;
        }
        const b3ShapeProxy proxy{points.data(), shrunkCorners(shapes[k], xf, sink, points), 0.f};
        b3World_OverlapShape(world->id, xf.p, &proxy, filter, fcn, context);
    }
}

// The props a corpse's body is made in (a monster's drop lying where it fell, gibs under it, a box it fell onto): they pass
// through it until clear of it (World::inside, shouldCollide), as a small gib made inside a monster does (noteBornInside),
// rather than Box3D pushing them out of it. Its shapes less a centimetre: what only touches it (a box resting on it, its
// body made again) still meets it.
void noteCorpseInside(int num, const Slot& s)
{
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catCorpse;
    filter.maskBits = catProp;
    constexpr float sink = 0.01f; // m
    overlapShrunk(s.body, sink, filter,
        [](b3ShapeId shape, void* raw) {
            const int corpse = *static_cast<const int*>(raw);
            const int prop = numOf(shape);
            auto& in = world->inside;
            if(prop > 0 && prop < static_cast<int>(world->slots.size()) && world->slots[prop].kind == Kind::Prop &&
                !za::anyOf(in.begin(), in.end(), [&](const World::Inside& i) { return i.prop == prop && i.other == corpse; }))
            {
                in.pushBack({prop, corpse, world->slots[prop].born});
                if(vr_debug_box3d.value)
                {
                    Con_Printf("box3d: %d %s inside corpse %d as it is made: passes through until clear\n", prop,
                        PR_GetString(EDICT_NUM(prop)->v.classname), corpse);
                }
            }
            return true;
        },
        const_cast<int*>(&num));
}

// A corpse's shapes on its body (createBody): its fitted hulls (Shape of Its Pose) or its lying box, of its mass
// (vr_corpse_collide_mass: its density from their volume; a fixed one's only matters to nothing) and friction.
void addCorpseShapes(edict_t* ent, int num, qmodel_t* model, Slot& s)
{
    glm::vec3 lo, hi;
    corpseBox(ent, lo, hi);
    s.mins = lo;
    s.maxs = hi;
    b3ShapeDef def = shapeDef(num, catCorpse, s.corpseMask);
    def.enableCustomFiltering = true; // (shouldCollide: thrown things, vr_corpse_collide_thrown; what it is made in)
    def.enableHitEvents = s.corpseDynamic; // (a pushable one's knocks: vr_physsound.cpp, soundHits)
    def.baseMaterial.friction = za::max(s.corpseFriction, 0.f);
    const za::Vector<b3HullData*>* hulls = s.corpseFitted ? &corpseHulls(ent, model) : nullptr;
    const glm::vec3 boxLo{lo.x, lo.y, lo.z + 0.25f}; // (a hair off the floor it lies on, as the fitted hulls)
    const glm::vec3 half = (hi - boxLo) * 0.5f / world->m2u;
    float volume = 8.f * half.x * half.y * half.z;
    if(hulls && !hulls->empty())
    {
        volume = 0.f;
        for(const b3HullData* hull : *hulls)
        {
            volume += hull->volume;
        }
    }
    def.density = za::max(s.massSetting, 1.f) / za::max(volume, 1e-6f);
    if(hulls && !hulls->empty())
    {
        for(const b3HullData* hull : *hulls)
        {
            b3CreateHullShape(s.body, &def, hull);
        }
    }
    else
    {
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((boxLo + hi) * 0.5f));
        b3CreateHullShape(s.body, &def, &box.base);
    }
    noteCorpseInside(num, s);
}

// A pushable corpse, before the step: moved by Quake or the QC (a lift's push, a teleport, setorigin) its body goes
// there; a velocity given it (a knock) is its body's, never Quake's own move (its entity stays on the ground).
void feedCorpse(edict_t* ent, Slot& s)
{
    const glm::vec3 origin = vec(ent->v.origin);
    if(origin != s.origin)
    {
        b3Body_SetTransform(s.body, world->toM(origin), rotationOf(ent, s));
        b3Body_SetAwake(s.body, true);
        s.origin = origin;
    }
    const glm::vec3 v = vec(ent->v.velocity);
    if(glm::length(v) > 1.f)
    {
        b3Body_SetAwake(s.body, true);
        b3Body_SetLinearVelocity(s.body, b3v(glmv(b3Body_GetLinearVelocity(s.body)) + glmv(world->toM(v))));
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: corpse %d knocked by QC: %.0f u/s\n", NUM_FOR_EDICT(ent), glm::length(v));
        }
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    setFlag(ent, FL_ONGROUND, true);
}

// A pushable corpse, after the step: its entity where its body is, turned as it (its yaw: a fitted one).
void writeCorpse(edict_t* ent, Slot& s)
{
    const b3WorldTransform xf = b3Body_GetTransform(s.body);
    glm::vec3 origin = world->toU(xf.p);
    if(origin.z < world->map->mins[2] - 1024.f)
    {
        // (Fallen out of the world: it stops there, as a prop.)
        b3Body_SetLinearVelocity(s.body, b3Vec3_zero);
        b3Body_SetAngularVelocity(s.body, b3Vec3_zero);
        b3Body_SetAwake(s.body, false);
    }
    store(origin, ent->v.origin);
    if(s.corpseFitted)
    {
        const glm::quat q = fromB3(xf.q);
        ent->v.angles[1] = glm::degrees(za::atan2(2.f * (q.w * q.z + q.x * q.y), 1.f - 2.f * (q.y * q.y + q.z * q.z)));
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    setFlag(ent, FL_ONGROUND, true);
    SV_LinkEdict(ent, false);
    s.origin = vec(ent->v.origin);
    s.angles = vec(ent->v.angles);
    s.asleep = !b3Body_IsAwake(s.body);
}

namespace
{
// Once a frame, after watchCorpses: the dead monsters' poses and the mapper's corpses.
// - A corpse left still mid-death (its frame unchanged for corpseStuck, no ragdoll: drawn upright or falling, "dead ogres
//   standing" of 1.0.1) is put at that death's last frame, once (said in the console the first time in a map).
// - A mapper's corpse (vr_corpse_placed: QC vr_mg_corpses.qc) wakes when a player comes within vr_corpse_placed_wake
//   (its ragdoll made as room allows); a woken one that got no ragdoll sleeps again once no player is within 1.5 times
//   that, to try again (the QC puts one no player is near back to sleep to make room: VR_Knockdown_MakeRoom).
constexpr double corpseStuck = 2.0; // s
} // namespace

void settleCorpses()
{
    const float wake = za::max(vr_corpse_placed_wake.value, 0.f);
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && num < static_cast<int>(world->corpseWatch.size()); num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(ent->free || !deadMonster(ent) || isRagdollSlot(num))
        {
            continue;
        }
        const float placed = fieldFloatOr(ent, fields().vr_corpse_placed, 0.f);
        if(placed != 0.f)
        {
            float nearest = 1e30f;
            for(int i = 1; i <= svs.maxclients; i++)
            {
                edict_t* player = EDICT_NUM(i);
                if(!player->free && svs.clients[i - 1].active && svs.clients[i - 1].spawned) // (not yet: at the origin)
                {
                    nearest = za::min(nearest, glm::distance(vec(player->v.origin), vec(ent->v.origin)));
                }
            }
            if(placed == 1.f && wake > 0.f && nearest < wake && vr_ragdoll.value >= 1.f)
            {
                setFieldFloat(ent, fields().vr_corpse_placed, 2.f);
            }
            else if(placed == 2.f && nearest >= wake * 1.5f)
            {
                setFieldFloat(ent, fields().vr_corpse_placed, 1.f);
                za::vectorEraseIf(world->roomRefused, [num](int n) { return n == num; });
            }
        }
        World::CorpseWatch& w = world->corpseWatch[static_cast<za::SizeT>(num)];
        qmodel_t* model = modelOf(ent);
        if(w.settled || w.since < 0.0 || qcvm->time - w.since < corpseStuck || !ragdoll::eligible(model))
        {
            continue;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(model);
        const int frame = static_cast<int>(ent->v.frame);
        const float progress = rig ? ragdoll::deathProgress(*rig, frame) : -1.f;
        if(progress < 0.f || progress >= 0.999f)
        {
            continue;
        }
        const int end = ragdoll::deathEnd(*rig, frame);
        const bool say = !world->corpseFrameSaid;
        world->corpseFrameSaid = true;
        (say ? Con_Printf : Con_DPrintf)("corpse: %d %s (%s) lay still %.1f s at frame %d, %.0f%% of its death: put at its "
                                         "death's last frame, %d\n",
            num, PR_GetString(ent->v.classname), model->name, qcvm->time - w.since, frame, progress * 100.f, end);
        ent->v.frame = static_cast<float>(end);
        w.settled = true;
        w.frame = ent->v.frame;
        w.since = qcvm->time;
        w.changed = -1.0;
    }
}
} // namespace box3d::detail
