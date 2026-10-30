#include "vr_alloccount.h"
// vr_box3d.cpp -- the rigid bodies in Box3D, the only rigid-body physics; see vr_box3d.hpp and
// docs/vr-port/ROUND21.md, "Box3D physics" and "Simplification: Box3D only".
//
// Every body lives in one Box3D world (Erin Catto's engine, Quake/vr/external/box3d), stepped once a server frame
// at the end of SV_Physics, after every entity has thought and moved (VR_PhysicsFrameEnd), so bodies also collide
// with each other (stacks, piles). (Quake VR's own solver, before, moved each body on its own and nothing stacked;
// it is kept on the branch archive/old-solver-stacking.)
//
// - The world: a static triangle mesh of the map's faces (the world model's, as drawn: sky and liquid faces left
//   out; clip brushes have no faces and point traces ignore them too, as the old solver's corners did).
// - Brush entities (doors, plats, trains, buttons, walls, rotating brushes: SOLID_BSP) are kinematic bodies made of
//   their model's solid leaves (hull 0: each a convex hull), driven each frame from their origin and angles with
//   the velocity that gets them there, so what rests on them rides them.
// - Monsters and other solid boxes (SOLID_BBOX, SOLID_SLIDEBOX with a size) are kinematic boxes, Quake's; players
//   (vr_box3d_player_push) kinematic capsules of their body's width. Kinematic bodies push props one way.
// - Props (.vr_rigid toss and bounce entities: thrown weapons, ammo and health boxes, backpacks, armour, gibs,
//   heads) are dynamic bodies: the convex hull of the drawn model (an alias model's frame: weapons, backpacks,
//   armour, gibs rest on their sides as drawn; a brush model's faces: the boxes as drawn, not Quake's padded box). Mass from the volume and a density per
//   kind (or the Mass set for its model: Held Object Offsets, vr_props.inc). Carried ones (in a hand, or both) are
//   kinematic, following the hand, so they push other props. Solid props (.vr_rigid 2: the explosive boxes) stay
//   SOLID_BBOX, their Quake box kept round them as they turn (solidBox), and report their hard hits (.vr_impact).
// - Corpses lying still (vr_corpse_collide; "Corpses in the physics" below, ROUND21.md, "Corpse collision") are bodies
//   props rest on and held things meet: fixed (kinematic) or heavy dynamic bodies, their lying box or hulls fitted to
//   their pose. Players and monsters meet them in Quake's moves as set (VR_CorpseBox).
// - The players' hands are kinematic spheres at their fists that push solid props (syncHands, vr_box3d_hand_push).
// - Players stand on solid props (vr_box3d_player_stand, "Standing on props": capsuleStandsOn, beforeStanding,
//   pressStanding, rideStanding): ground to Quake's movement, their weight pressing, carried as the prop moves. Their
//   boxes meet a solid prop's drawn box as turned, as a round column (vr_box3d_player_shape: VR_PropClip, propShape).
// - Shots and missiles (rockets, nails, grenades, lasers, the grappling hook; the guns' traces) meet a solid prop's drawn
//   box as turned, not its Quake box (vr_box3d_shot_shape: VR_PropShotClip, shotShape).
// - And kinematic bodies at full speed that push the other props and hold them up (syncReach, ROUND21.md, "Hands and
//   weapons as bodies"): an empty hand's open hand or fist (vr_box3d_hand_props; not grenades, which the palm catches),
//   a held weapon's drawn hull (vr_box3d_weapon_push: a grenade is batted). What was inside one as it was made (let go
//   of, thrown, a weapon taken) passes through it until clear; what is heavier than vr_box3d_hand_hold_mass slips off.
//   They push by mass (pushShare, limitPushes: vr_box3d_hand_mass, vr_box3d_weapon_arm_mass and each weapon's Mass, vr_box3d_push_force), as do
//   the fists' spheres and the carried props: a flick barely moves a heavy prop.
// - Box3D is authoritative for props: their origin, angles, velocity (.velocity, the centre of mass's), spin
//   (.vr_spin, rad/s) and sleep (FL_ONGROUND and its groundentity) are written back every frame. What QC changes
//   (a throw, a nudge, a force grab's drop, a knock, a teleport, keepInWorld's put-back) is seen against what was
//   written last and fed in, waking the body.
// - The old solver's behaviours were kept: the monsters' hit box along a thrown thing's flight (vr_throw_hitbox),
//   touches of what props hit (QC's damage), water (lift by depth, drag, floating flat, the bob), the splashes (the
//   water transition), vr_throw_restitution and vr_throw_friction as the materials, vr_throw_spin_drag.
// - A throw's spin settles in the air (spinAlign, vr_throw_spin_align): Box3D integrates the gyroscopic torque
//   (solver.c's b3IntegrateVelocitiesTask: implicit, one Newton step, always on), so a free body precesses and flips
//   about its middle axis as a real one does, but nothing takes energy from a tumble, so it never settles on its
//   steadiest axis as a thrown axe does. spinAlign does that, until the throw first touches anything.
//
// Single-threaded: the world has one worker and no task callbacks, which Box3D runs serially (each task inline:
// b3DefaultAddTaskFcn in physics_world.c); no scheduler, no threads. Deterministic: the same calls in the same
// order (entities in edict order) give the same result.
//
// The code is in parts (vr_box3d_*.cpp, one area each; this file: the world, its helpers, settings,
// building and destroying it, tool joints); vr_box3d_internal.hpp has what they share and the list.

#include "vr_box3d_internal.hpp"

#include "vr_admelee.hpp"
#include "vr_geom.hpp"
#include "vr_trace.hpp"

namespace box3d::detail
{
[[nodiscard]] const char* kindName(Kind k)
{
    switch(k)
    {
    case Kind::Prop: return "prop";
    case Kind::Held: return "held";
    case Kind::Mover: return "mover";
    case Kind::Actor: return "actor";
    case Kind::Player: return "player";
    case Kind::Fixture: return "fixture";
    case Kind::Corpse: return "corpse";
    default: return "none";
    }
}
using geom::vec;
using geom::store;
[[nodiscard]] qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}
[[nodiscard]] bool hasFlag(edict_t* ent, int flag)
{
    return (static_cast<int>(ent->v.flags) & flag) != 0;
}
void setFlag(edict_t* ent, int flag, bool on)
{
    const int flags = static_cast<int>(ent->v.flags);
    ent->v.flags = static_cast<float>(on ? flags | flag : flags & ~flag);
}
[[nodiscard]] float gravityScaleOf(edict_t* ent)
{
    const eval_t* val = GetEdictFieldValue(ent, qcvm->extfields.gravity);
    return val && val->_float ? val->_float : 1.f;
}

// A solid prop (.vr_rigid 2: an explosive box): SOLID_BBOX, so shots, players and monsters meet it, its Quake box kept
// round its drawn shape as it turns (solidBox).
[[nodiscard]] bool isSolidProp(edict_t* ent)
{
    const int ofs = fields().vr_rigid;
    return ofs >= 0 && fieldFloat(ent, ofs) >= 2.f;
}

// The prop's box as drawn, in its axes from its origin (vr_rigid.cpp's localBox: the networked scale and offset,
// the weapon scaling; never thinner than a unit).
void localBox(edict_t* ent, qmodel_t* model, glm::vec3& lo, glm::vec3& hi)
{
    if(model && model->type == mod_brush && isSolidProp(ent))
    {
        lo = geom::vec(model->mins); // (its Quake box follows its turn: solidBox)
        hi = geom::vec(model->maxs);
        const float size = props::drawnSize(model); // its Size (Held Object Offsets; the alias models': modelBox)
        lo *= size;
        hi *= size;
    }
    else if(!model || model->type != mod_alias)
    {
        lo = vec(ent->v.mins);
        hi = vec(ent->v.maxs);
        // A brush item's (an ammo box's) Size (Held Object Offsets). Not for a SOLID_BBOX one: solidBox writes its
        // Quake box from this, which would grow again each time.
        if(static_cast<int>(ent->v.solid) != SOLID_BBOX)
        {
            const float size = props::drawnSize(model);
            lo *= size;
            hi *= size;
        }
    }
    else
    {
        const FieldOffsets& f = fields();
        held::modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);
    }
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{0.5f});
    lo = centre - half;
    hi = centre + half;
}

namespace
{
// Water, as the old solver's: things float (items, backpacks, thrown weapons: 60% under at rest); gibs sink, slowly.
constexpr float floatDensity = 1.f / 0.6f; // relative to water's (the lift's scale)
constexpr float sinkDensity = 0.5f;
} // namespace

[[nodiscard]] float waterDensity(edict_t* ent)
{
    const bool gib = hasFlag(ent, physics::FL_FORCEGRABBABLE) && !hasFlag(ent, FL_ITEM);
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const bool wood = modelmeta::is(model, modelmeta::Id::Vrtorch) || modelmeta::has(model, modelmeta::Trait::Plank); // (a crate's pieces float)
    if(model)
    {
        if(const float stone = props::stoneDensity(model); stone > 0.f)
        {
            return 1000.f / stone; // a rock or a brick (vr_debris.cpp) sinks as stone does, faster than a gib
        }
    }
    return gib && !wood ? sinkDensity : floatDensity; // (a taken wall torch, a crate's piece are wood: they float)
}

namespace
{
// The deepest node of the world's hull 0 above every point (x, y, z), z from `zlo` to `zhi`: SV_HullPointContents
// from it gives what it gives from the root, for any point of that column. Each plane's distance is the expression
// SV_HullPointContents works out, a monotone function of z (x and y fixed), so the ends on one side put the whole
// column there. (A leaf's contents, below 0, when the column is in one leaf.)
[[nodiscard]] int columnNode(float x, float y, float zlo, float zhi)
{
    hull_t* hull = &sv.worldmodel->hulls[0];
    vec3_t a{x, y, zlo}, b{x, y, zhi};
    int num = 0;
    while(num >= 0)
    {
        const mclipnode_t* node = hull->clipnodes + num;
        const mplane_t* plane = hull->planes + node->planenum;
        float da, db;
        if(plane->type < 3)
        {
            da = a[plane->type] - plane->dist;
            db = b[plane->type] - plane->dist;
        }
        else
        {
            da = DoublePrecisionDotProduct(plane->normal, a) - plane->dist;
            db = DoublePrecisionDotProduct(plane->normal, b) - plane->dist;
        }
        if((da < 0) != (db < 0))
        {
            break;
        }
        num = node->children[da < 0 ? 1 : 0];
    }
    return num;
}

// Whether (x, y, z) is in water, slime or lava (SV_PointContents' answer), the hull walked from `node`
// (columnNode's, for a point of its column).
[[nodiscard]] bool wetFrom(int node, float x, float y, float z)
{
    vec3_t p{x, y, z};
    int contents = SV_HullPointContents(&sv.worldmodel->hulls[0], node, p); // (SV_PointContents' steps)
    if(worldtrace::isLiquid(contents))
    {
        contents = worldtrace::liquidKind(contents); // (a current: water)
    }
    return worldtrace::isLiquid(VR_LiquidContents(sv.worldmodel, p, contents));
}
} // namespace

// The part of the column from `lo` to `hi` (z) at `c` under water (0 to 1), by the BSP's liquid leaves. Every point
// it tries is on one column: the hull's walk down to that column's node is done once (MG3.md M3-28: a body's
// 2 to 15 point tests each step, 0.26 ms a frame of secret2's fight).
[[nodiscard]] float submerged(const glm::vec3& c, float lo, float hi)
{
    const int node = columnNode(c.x, c.y, za::min(lo, c.z), za::max(hi, c.z));
    float under = lo;
    if(!wetFrom(node, c.x, c.y, lo))
    {
        if(!wetFrom(node, c.x, c.y, c.z))
        {
            return 0.f;
        }
        under = c.z; // the bottom in a floor
    }
    if(wetFrom(node, c.x, c.y, hi))
    {
        return 1.f;
    }
    float above = hi;
    for(int i = 0; i < 12; i++)
    {
        const float mid = (under + above) * 0.5f;
        (wetFrom(node, c.x, c.y, mid) ? under : above) = mid;
    }
    return CLAMP(0.f, ((under + above) * 0.5f - lo) / za::max(hi - lo, 0.01f), 1.f);
}

// Weapons (thrown, dropped, or a map's weapon pickup) and keys: hard, detailed shapes.
[[nodiscard]] bool isWeaponLike(edict_t* ent)
{
    const char* name = PR_GetString(ent->v.classname);
    return !strcmp(name, "thrown_weapon") || !strncmp(name, "weapon_", 7) || !strncmp(name, "item_key", 8);
}

// A live grenade that can be caught (QC vr_grenade.qc: an ogre's, a multi-grenade ogre's, or the player's with
// vr_grenade_catch 2) as a rigid body, or a pouch grenade (the proximity grenade too): Quake's grenade models. Hard and
// heavy, it bounces as Quake's grenades did, and never meets its thrower's own body (Quake's rule for a missile and its
// owner: it leaves the ogre it is thrown from).
[[nodiscard]] bool isGrenade(const qmodel_t* model)
{
    return model->type == mod_alias && modelmeta::isQuakeGrenade(model);
}

// Densities (kg/m^3) of the props' hulls: only their ratios matter (what knocks what how far).
[[nodiscard]] float densityOf(edict_t* ent, const qmodel_t* model)
{
    if(model->type == mod_brush)
    {
        return 400.f; // ammo and health boxes: full of shells, nails, cells, medkits
    }
    if(isGrenade(model))
    {
        return 2000.f; // an iron shell full of explosive
    }
    if(modelmeta::has(model, modelmeta::Trait::LiveShell))
    {
        return 1300.f; // a shotgun shell: plastic, lead shot, a brass head (its slot's Mass sets it: 40 g)
    }
    if(modelmeta::has(model, modelmeta::Trait::Magazine) || modelmeta::has(model, modelmeta::Trait::LiveRound))
    {
        return 1500.f; // a magazine: a steel box of nails, a cell (its slot's Mass sets it)
    }
    if(const float stone = props::stoneDensity(model); stone > 0.f)
    {
        return stone; // the rocks and bricks lying about (vr_debris.cpp)
    }
    if(isWeaponLike(ent))
    {
        return 700.f; // guns and blades (their hulls are partly air), keys
    }
    if(hasFlag(ent, FL_ITEM))
    {
        return modelmeta::has(model, modelmeta::Trait::ContainsArmor) ? 600.f : 250.f; // armour; backpacks
    }
    return 1000.f; // gibs and heads: flesh
}

// A Mass set for the prop's model (Held Object Offsets, vr_props.inc), kg; else a weapon's own (a weapon lying about or
// thrown weighs what it weighs in the hand: Weapon Weights' Mass, vr_weapons.inc); 0: none (its volume times its
// density: a crowbar's hull made it 0.6 kg of its 2.2, and it landed with the light metal sounds).
[[nodiscard]] float propMassSetting(const qmodel_t* model)
{
    return model ? za::max(props::valueFor(model, props::Key::Mass), 0.f) : 0.f;
}
[[nodiscard]] float weaponMassSetting(edict_t* ent, const qmodel_t* model)
{
    const int slot = model && isWeaponLike(ent) ? weapons::slotForModel(model) : -1;
    return slot >= 0 && slot != weapons::fistSlot() ? za::max(weapons::value(slot, weapons::Key::Mass), 0.f) : 0.f;
}
[[nodiscard]] float massSetting(edict_t* ent, const qmodel_t* model)
{
    // Its own (QC .vr_prop_mass: a small gib, the gibs' models scaled down) over its model's.
    if(const float own = ent && fields().vr_prop_mass >= 0 ? fieldFloat(ent, fields().vr_prop_mass) : 0.f; own > 0.f)
    {
        return own;
    }
    const float prop = propMassSetting(model);
    return prop > 0.f ? prop : weaponMassSetting(ent, model);
}

// Soft things (backpacks, gibs, heads) land with a thud: no bounce, and their tumble dies away fast on the ground (a
// rigid hull of a backpack lands on an edge and tumbles down a gentle slope like a crate).
[[nodiscard]] bool isSoft(edict_t* ent, const qmodel_t* model)
{
    return model->type == mod_alias && !isWeaponLike(ent) && !modelmeta::has(model, modelmeta::Trait::ContainsArmor) && !isGrenade(model) &&
        !modelmeta::has(model, modelmeta::Trait::LiveShell) && !modelmeta::has(model, modelmeta::Trait::Magazine) &&
        !modelmeta::has(model, modelmeta::Trait::LiveRound) && props::stoneDensity(model) <= 0.f; // (rocks and bricks are hard; a shell bounces and rolls)
}

// Whether part `b` of `r` was cut off (its body is the part it was cut from's).
[[nodiscard]] bool partCut(const RagdollBodies& r, int b)
{
    return (r.cut & (1u << b)) != 0;
}

// How many parts `r` has (not cut off).
[[nodiscard]] int partsLeft(const RagdollBodies& r)
{
    int n = 0;
    for(int b = 0; b < r.count; b++)
    {
        n += (r.cut & (1u << b)) ? 0 : 1;
    }
    return n;
}
[[nodiscard]] bool toolPinned(int num)
{
    return za::find(toolRecords.pinned.begin(), toolRecords.pinned.end(), num) != toolRecords.pinned.end();
}

namespace
{
void runStepTask(void* p)
{
    StepTask& t = *static_cast<StepTask*>(p);
    qcvm_t* const vm = qcvm;
    globalvars_t* const globals = pr_global_struct;
    qcvm = t.vm;
    pr_global_struct = t.globals;
    t.fn(t.context);
    qcvm = vm;
    pr_global_struct = globals;
}
} // namespace

// Box3D's b3EnqueueTaskCallback (`user`: the world's StepTasks): on the pool; with one worker, no pool or past the bound, here and now
// (nullptr: nothing to finish).
void* enqueueStepTask(b3TaskCallback* fn, void* context, void* user, const char* /*name*/)
{
    StepTasks& w = *static_cast<StepTasks*>(user);
    jobs::Pool* const pool = jobs::pool();
    const int n = static_cast<int>(w.tasks.size());
    const int i = w.workers > 1 && pool ? w.next.fetchAddRelaxed(1) : n;
    if(i >= n)
    {
        fn(context);
        return nullptr;
    }
    StepTask& t = w.tasks[static_cast<za::SizeT>(i)];
    t.fn = fn;
    t.context = context;
    t.vm = qcvm;
    t.globals = pr_global_struct;
    t.task.post(*pool, runStepTask, &t);
    return &t;
}

// Box3D's b3FinishTaskCallback: returns once the task ran (on this thread if no worker has started it).
void finishStepTask(void* task, void* /*user*/)
{
    static_cast<StepTask*>(task)->task.wait();
}

namespace
{
[[nodiscard]] bool useAvx2()
{
    return vr_box3d_avx2.value != 0.f && cpuHasAvx2;
}
} // namespace

void applySimdWidth(b3WorldId id)
{
    b3World_EnableSSE2Fallback(id, !useAvx2());
}

namespace
{
// Box3D's workers: the game's pool's and this thread, at most vr_box3d_workers (0: no cap but Box3D's 32); 1 (the step on
// this thread alone) with vr_box3d_threads 0, without a pool, with vr_jobs_parallel 0 (the single-threaded reference),
// while vr_debug_box3d 2 prints from the callbacks (only the main thread prints), or while fewer than
// vr_box3d_threads_bodies bodies are awake (`awake`; with `current` workers above one, back to one under three quarters of
// it, so it doesn't flip each frame): a smaller step costs more handed out than on one thread (ROUND21.md, "Box3D on the
// pool").
[[nodiscard]] int stepWorkers(int awake, int current)
{
    const float least = za::max(vr_box3d_threads_bodies.value, 0.f) * (current > 1 ? 0.75f : 1.f);
    if(!vr_box3d_threads.value || !jobs::pool() || !jobs::parallel() || vr_debug_box3d.value >= 2.f ||
       static_cast<float>(awake) < least)
    {
        return 1;
    }
    const int cap = vr_box3d_workers.value >= 1.f ? static_cast<int>(vr_box3d_workers.value) : B3_MAX_WORKERS;
    return za::clamp(za::min(cap, jobs::workers() + 1), 1, B3_MAX_WORKERS);
}
} // namespace

[[nodiscard]] glm::quat turnOf(const float* angles, bool brush)
{
    return glm::normalize(glm::quat_cast(held::axesFromAngles(angles, brush)));
}
[[nodiscard]] void* userOf(int num)
{
    return reinterpret_cast<void*>(static_cast<intptr_t>(num));
}
[[nodiscard]] int numOf(b3ShapeId shape)
{
    return static_cast<int>(reinterpret_cast<intptr_t>(b3Shape_GetUserData(shape)));
}
[[nodiscard]] Slot& slotOf(int num)
{
    if(num >= static_cast<int>(world->slots.size()))
    {
        world->slots.resize(static_cast<size_t>(num) + 64);
    }
    return world->slots[num];
}
void destroyBody(Slot& s)
{
    if(s.ragdoll >= 0 && s.ragdoll < static_cast<int>(world->ragdolls.size()))
    {
        // A ragdoll's parts (their joints go with them), the hands holding it, and the client's drawing of it.
        RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
        dropGrabsOf(r.num);
        ragactive::end(r.active); // (its pin's anchor)
        for(int b = 0; b < r.count; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(B3_IS_NON_NULL(body) && b3Body_IsValid(body))
            {
                b3DestroyBody(body);
            }
        }
        ragdoll::unpublish(r.num);
        r = RagdollBodies{};
        s = Slot{};
        return;
    }
    if(B3_IS_NON_NULL(s.body) && b3Body_IsValid(s.body))
    {
        b3DestroyBody(s.body);
    }
    s = Slot{};
}
void updateSettings()
{
    if(const int workers = stepWorkers(b3World_GetAwakeBodyCount(world->id), world->tasks.workers);
       workers != world->tasks.workers)
    {
        b3World_SetWorkerCount(world->id, workers);
        world->tasks.workers = workers;
    }
    const float g = sv_gravity.value;
    if(g != world->gravity)
    {
        b3World_SetGravity(world->id, b3Vec3{0.f, 0.f, -g / world->m2u});
        world->gravity = g;
    }
    const float friction = za::max(vr_throw_friction.value, 0.f), restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);
    if(friction == world->friction && restitution == world->restitution)
    {
        return;
    }
    const bool first = world->friction < 0.f;
    world->friction = friction;
    world->restitution = restitution;
    if(first)
    {
        return; // (made with them)
    }
    // The materials: vr_throw_friction everywhere (Box3D mixes sqrt(a * b)), the props' vr_throw_restitution (it
    // takes the larger).
    if(B3_IS_NON_NULL(world->worldShape))
    {
        b3Shape_SetFriction(world->worldShape, friction);
    }
    za::Array<b3ShapeId, 64> shapes;
    for(Slot& s : world->slots)
    {
        if(s.kind == Kind::None)
        {
            continue;
        }
        const int count = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            b3Shape_SetFriction(shapes[i], friction);
            if((s.kind == Kind::Prop || s.kind == Kind::Held) && !s.soft)
            {
                b3Shape_SetRestitution(shapes[i], s.model && isGrenade(s.model) ? grenadeRestitution : restitution);
            }
        }
        if(s.kind == Kind::Prop)
        {
            b3Body_SetAngularDamping(s.body, za::max(vr_throw_spin_drag.value, 0.f));
        }
    }
}
void destroyWorld()
{
    if(!world)
    {
        return;
    }
    for(ToolJointRecord& j : toolRecords.joints)
    {
        j.id = b3_nullJointId; // (gone with the world: made again in the next one, syncToolJoints)
    }
    ragdoll::unpublishAll();
    if(b3World_IsValid(world->id))
    {
        b3DestroyWorld(world->id);
    }
    keepPendingHulls(); // (built ahead, not taken: freed below with the rest)
    for(auto& [key, hull] : world->propHulls)
    {
        if(hull)
        {
            b3DestroyHull(hull);
        }
    }
    for(auto& [key, hulls] : world->corpseHulls)
    {
        for(b3HullData* hull : hulls)
        {
            b3DestroyHull(hull);
        }
    }
    for(auto& [key, hulls] : world->gunPieces)
    {
        for(b3HullData* hull : hulls)
        {
            b3DestroyHull(hull);
        }
    }
    for(auto& [model, hulls] : world->moverHulls)
    {
        for(b3HullData* hull : hulls)
        {
            b3DestroyHull(hull);
        }
    }
    for(const World::RetiredHull& r : world->retiredHulls)
    {
        b3DestroyHull(r.hull);
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: world destroyed after %d steps\n", world->steps);
    }
    world.reset();
}
void buildWorld()
{
    world = za::makeUnique<World>();
    world->map = sv.worldmodel;
    world->generation = worldGeneration();
    world->m2u = units::metresToUnits();
    world->gravity = sv_gravity.value;
    world->friction = za::max(vr_throw_friction.value, 0.f);
    world->restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);

    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = b3Vec3{0.f, 0.f, -world->gravity / world->m2u};
    def.enableSleep = true;
    def.enableContinuous = true;
    // Gyroscopic torque on every body that is not exactly isotropic (upstream's default 0.2 leaves it out for near-
    // isotropic ones): the throw's spin alignment (spinAlign) relies on Box3D's gyroscopic step.
    def.isotropicInertiaTolerance = 0.f;
    // Its tasks on the game's pool (enqueueStepTask), with stepWorkers' workers (1: all of them on this thread).
    world->tasks.workers = stepWorkers(0, 1);
    def.workerCount = static_cast<uint32_t>(world->tasks.workers);
    def.enqueueTask = enqueueStepTask;
    def.finishTask = finishStepTask;
    def.userTaskContext = &world->tasks;
    // Room for the map's entities as bodies and a pile's contacts from the start: no growing (a reallocation and copy
    // of Box3D's arrays) in the frame a pile collapses.
    def.capacity.staticBodyCount = 1;
    def.capacity.staticShapeCount = 1;
    def.capacity.dynamicBodyCount = za::max(qcvm->num_edicts, 256);
    def.capacity.dynamicShapeCount = za::max(qcvm->num_edicts, 256) + 256;
    def.capacity.contactCount = 4096;
    world->id = b3CreateWorld(&def);
    b3World_SetCustomFilterCallback(world->id, shouldCollide, nullptr);
    b3World_SetPreSolveCallback(world->id, preSolve, nullptr);

    world->mesh = cachedWorldMesh(sv.worldmodel, world->m2u);
    if(world->mesh)
    {
        b3BodyDef body = b3DefaultBodyDef();
        body.type = b3_staticBody;
        body.userData = userOf(0);
        const b3BodyId id = b3CreateBody(world->id, &body);
        b3ShapeDef shape = shapeDef(0, catWorld, catProp | catCorpse);
        world->worldShape = b3CreateMeshShape(id, &shape, world->mesh, b3Vec3_one);
    }
    world->slots.resize(static_cast<size_t>(qcvm->num_edicts) + 64);
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: world for %s, %d triangles, %.1f units a metre\n", sv.worldmodel->name,
            world->mesh ? world->mesh->triangleCount : 0, world->m2u);
    }
}
[[nodiscard]] bool wanted()
{
    return fields().vr_rigid >= 0 && sv.worldmodel; // (a mod without .vr_rigid has no rigid bodies)
}

// A toolgun joint's body: a prop's (loose or pinned), null if it has none now.
[[nodiscard]] b3BodyId toolJointBody(int num)
{
    if(num <= 0 || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()) || EDICT_NUM(num)->free)
    {
        return b3_nullBodyId;
    }
    const Slot& s = world->slots[num];
    return s.kind == Kind::Prop || s.kind == Kind::Fixture || s.kind == Kind::Held ? s.body : b3_nullBodyId;
}

// Each toolgun joint made where it is missing (its bodies made again: a prop frozen or let go of, scaled, a new world),
// once both bodies are there and one of them moves (Box3D joins nothing to nothing that moves).
void syncToolJoints()
{
    for(ToolJointRecord& j : toolRecords.joints)
    {
        if(B3_IS_NON_NULL(j.id) && b3Joint_IsValid(j.id))
        {
            continue;
        }
        j.id = b3_nullJointId;
        const b3BodyId a = toolJointBody(j.a), b = toolJointBody(j.b);
        if(B3_IS_NULL(a) || B3_IS_NULL(b) ||
            (b3Body_GetType(a) != b3_dynamicBody && b3Body_GetType(b) != b3_dynamicBody))
        {
            continue;
        }
        const auto setBase = [&](b3JointDef& base) {
            base.bodyIdA = a;
            base.bodyIdB = b;
            base.localFrameA = j.frameA;
            base.localFrameB = j.frameB;
            base.collideConnected = j.kind == qvr::box3d::ToolJoint::Rope || j.kind == qvr::box3d::ToolJoint::Spring;
        };
        switch(j.kind)
        {
        case qvr::box3d::ToolJoint::Weld:
        {
            b3WeldJointDef jd = b3DefaultWeldJointDef();
            setBase(jd.base);
            j.id = b3CreateWeldJoint(world->id, &jd);
            break;
        }
        case qvr::box3d::ToolJoint::Ball:
        {
            b3SphericalJointDef jd = b3DefaultSphericalJointDef();
            setBase(jd.base);
            j.id = b3CreateSphericalJoint(world->id, &jd);
            break;
        }
        case qvr::box3d::ToolJoint::Hinge:
        {
            b3RevoluteJointDef jd = b3DefaultRevoluteJointDef();
            setBase(jd.base);
            j.id = b3CreateRevoluteJoint(world->id, &jd);
            break;
        }
        case qvr::box3d::ToolJoint::Slider:
        {
            b3PrismaticJointDef jd = b3DefaultPrismaticJointDef();
            setBase(jd.base);
            j.id = b3CreatePrismaticJoint(world->id, &jd);
            break;
        }
        case qvr::box3d::ToolJoint::Rope:
        case qvr::box3d::ToolJoint::Spring:
        {
            b3DistanceJointDef jd = b3DefaultDistanceJointDef();
            setBase(jd.base);
            jd.length = j.length;
            jd.enableSpring = true; // (a rope: no stiffness, held within its length by the limit)
            if(j.kind == qvr::box3d::ToolJoint::Rope)
            {
                jd.hertz = 0.f;
                jd.enableLimit = true;
                jd.minLength = 0.f;
                jd.maxLength = j.length;
            }
            else
            {
                jd.hertz = 2.f;
                jd.dampingRatio = 0.2f;
            }
            j.id = b3CreateDistanceJoint(world->id, &jd);
            break;
        }
        default: break;
        }
        b3Body_SetAwake(a, true);
        b3Body_SetAwake(b, true);
    }
}

namespace
{
// Where a rope holds a body, in its frame (units; the body's frame is its edict's origin and angles, its shapes the model
// as drawn: vr_box3d_props.cpp createBody): the model's point `p` as drawn.
[[nodiscard]] glm::vec3 drawnEnd(edict_t* e, const glm::vec3& p)
{
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const FieldOffsets& f = fields();
    return held::drawnPoint(model, fieldVec(e, f.model_scale), fieldVec(e, f.model_scale_origin), fieldVec(e, f.model_offset), p);
}
} // namespace

// The things lying about held to a weapon lying about (a thrown_weapon) by a rope, each naming the other by
// .vr_fuellink: the fuelled guns' tanks (QC vr_ad_enemyguns.qc "Fuel tanks"; classname vr_fueltank), from the gun's origin
// (its grip) to the tank's (its middle), slack to vr_adgun_tank_rope m (or as far apart as they are when it is made, a
// little more); the death sergeant's flails' heads (QC vr_ad_melee.qc "The flail lying about"; vr_flailhead), from the
// handle's tip (its made model's: admelee::tipOf) to the head's ring (+x ringReach from its ball's middle), slack to the
// chain's length, vr_flail_chain m (the held head's rope: vr_box3d_flail.cpp). Box3D's distance joint, no spring.
// Found again each frame, so a saved game, a new world or a body made again (a hand carrying either: a kinematic body)
// has its rope again; a pair gone, its joint goes.
void syncPropRopes()
{
    const int link = fields().vr_fuellink;
    if(link < 0)
    {
        return;
    }
    const int frame = ++world->fuelFrame;
    for(int n = svs.maxclients + 1; n < qcvm->num_edicts; n++)
    {
        edict_t* t = EDICT_NUM(n);
        if(t->free || fieldInt(t, link) == 0)
        {
            continue;
        }
        const char* cls = PR_GetString(t->v.classname);
        const bool tank = !strcmp(cls, "vr_fueltank");
        if(!tank && strcmp(cls, "vr_flailhead"))
        {
            continue;
        }
        const World::RopeKind kind = tank ? World::RopeKind::Tank : World::RopeKind::Flail;
        edict_t* g = PROG_TO_EDICT(fieldInt(t, link));
        const int gn = NUM_FOR_EDICT(g);
        if(gn <= svs.maxclients || g->free || fieldInt(g, link) != EDICT_TO_PROG(t) ||
            strcmp(PR_GetString(g->v.classname), "thrown_weapon") ||
            (!tank && strcmp(PR_GetString(g->v.model), qvr::admelee::flailModel)))
        {
            continue;
        }
        World::FuelRope* r = nullptr;
        for(World::FuelRope& x : world->fuelRopes)
        {
            if(x.tank == n && x.kind == kind)
            {
                r = &x;
                break;
            }
        }
        if(!r)
        {
            world->fuelRopes.pushBack(World::FuelRope{});
            r = &world->fuelRopes.back();
            r->tank = n;
            r->kind = kind;
        }
        r->seen = frame;
        const b3BodyId gb = toolJointBody(gn), tb = toolJointBody(n);
        const bool live = B3_IS_NON_NULL(r->id) && b3Joint_IsValid(r->id);
        if(live && r->gun == gn && B3_ID_EQUALS(gb, r->gunBody) && B3_ID_EQUALS(tb, r->tankBody))
        {
            continue;
        }
        if(live)
        {
            b3DestroyJoint(r->id, true);
        }
        r->id = b3_nullJointId;
        r->gun = gn;
        r->gunBody = gb;
        r->tankBody = tb;
        if(B3_IS_NULL(gb) || B3_IS_NULL(tb) || (b3Body_GetType(gb) != b3_dynamicBody && b3Body_GetType(tb) != b3_dynamicBody))
        {
            continue; // (made once both are there and one of them moves)
        }
        if(tank)
        {
            const float apart = glm::length(glmv(b3Body_GetPosition(gb)) - glmv(b3Body_GetPosition(tb)));
            r->length = za::max(za::max(vr_adgun_tank_rope.value, 0.1f), apart + 0.02f);
            r->gunEnd = r->tankEnd = glm::vec3{0.f};
        }
        else
        {
            glm::vec3 tip, hand;
            if(!qvr::admelee::tipOf(qvr::admelee::flailModel, tip, hand))
            {
                continue; // (no flail made: AD's data not there)
            }
            r->gunEnd = drawnEnd(g, tip);
            r->tankEnd = drawnEnd(t, glm::vec3{qvr::admelee::flailRingReach, 0.f, 0.f});
            r->length = za::clamp(vr_flail_chain.value, 0.1f, 2.f);
        }
        b3DistanceJointDef jd = b3DefaultDistanceJointDef();
        jd.base.bodyIdA = gb;
        jd.base.bodyIdB = tb;
        jd.base.localFrameA = b3Transform{world->toM(r->gunEnd), toB3(glm::quat{1.f, 0.f, 0.f, 0.f})};
        jd.base.localFrameB = b3Transform{world->toM(r->tankEnd), toB3(glm::quat{1.f, 0.f, 0.f, 0.f})};
        jd.base.collideConnected = false;
        jd.length = r->length;
        jd.enableSpring = true; // (a rope: no stiffness, held within its length by the limit; the toolgun's)
        jd.hertz = 0.f;
        jd.enableLimit = true;
        jd.minLength = 0.f;
        jd.maxLength = r->length;
        r->id = b3CreateDistanceJoint(world->id, &jd);
        b3Body_SetAwake(gb, true);
        b3Body_SetAwake(tb, true);
    }
    za::vectorEraseIf(world->fuelRopes, [frame](const World::FuelRope& r) {
        if(r.seen == frame)
        {
            return false;
        }
        if(B3_IS_NON_NULL(r.id) && b3Joint_IsValid(r.id))
        {
            b3DestroyJoint(r.id, true);
        }
        return true;
    });
}
} // namespace box3d::detail
