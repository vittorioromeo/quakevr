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

#include "vr_modelmetadata.hpp"
#include "vr_box3d.hpp"
#include "vr_main.hpp"
#include "vr_portals.hpp"
#include "vr_axestick.hpp"
#include "vr_hitmodel.hpp"
#include "vr_jobs.hpp"
#include "vr_cvars.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"
#include "vr_limbmodel.hpp"
#include "vr_ragdoll.hpp"
#include "vr_weapons.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/LowerBound.hpp"
#include "Zancle/Algorithm/Rotate.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/UpperBound.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

#include <box3d/box3d.h>

#include <string.h>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// The sync's buffers (the server's frame: the main thread; Box3D steps with one worker, the caller's).
struct Box3dScratch
{
    za::Vector<glm::vec3> propVerts;   // a prop's drawn vertices, its hull made (propHull)
    za::Vector<glm::vec3> actorVerts;  // an actor's (actorHull)
    za::Vector<glm::vec3> corners;     // a body's shapes' corners (floorDepth)
    za::Vector<uint8_t> carried;       // by edict: carried by a player (syncEntities)
    za::Vector<b3ContactData> pushContacts; // a pushed prop's touching contacts, all of them (limitPushes)
    za::Vector<glm::vec4> palmPatch;   // a hand's palm, points on its skin (handFit: a limb taken)
    auto members() { return qvr::mem::list(propVerts, actorVerts, corners, carried, pushContacts, palmPatch); }
};
mem::Scratch<Box3dScratch> scratch{"box3d"};

// Collision categories: props collide with everything; the rest only with props.
constexpr uint64_t catWorld = 1;
constexpr uint64_t catMover = 2;
constexpr uint64_t catActor = 4;
constexpr uint64_t catPlayer = 8;
constexpr uint64_t catProp = 16;
constexpr uint64_t catHeld = 32;
constexpr uint64_t catFixture = 64;
constexpr uint64_t catHand = 128;  // the players' hands (vr_box3d_hand_push): only against solid props
constexpr uint64_t catSolid = 256; // a solid prop's (.vr_rigid 2: an explosive box) shapes, as well as catProp
constexpr uint64_t catReachHand = 512;    // an empty hand's body (vr_box3d_hand_props): the loose props but grenades
constexpr uint64_t catReachWeapon = 1024; // a held weapon's body (vr_box3d_weapon_push): the loose props
constexpr uint64_t catReach = catReachHand | catReachWeapon;
constexpr uint64_t catCorpse = 2048; // a corpse's body (vr_corpse_collide): what meets it is its own mask's (corpseMask)
constexpr uint64_t propMask = catWorld | catMover | catActor | catPlayer | catProp | catHeld | catFixture | catCorpse;

enum class Kind : uint8_t
{
    None,
    Prop,   // a rigid body: dynamic
    Held,   // a prop carried in a hand (or two): kinematic, following it
    Mover,  // a brush entity: kinematic
    Actor,  // a monster or another solid box: kinematic
    Player, // a player's body: a kinematic capsule
    Fixture, // a pickup that is not a rigid body (hanging in the air, on a rack): kinematic, its drawn hull
    Corpse,  // a dead monster lying still (vr_corpse_collide): kinematic (Fixed) or a heavy dynamic body (Pushable)
};

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

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

void store(const glm::vec3& g, float* v)
{
    v[0] = g.x;
    v[1] = g.y;
    v[2] = g.z;
}

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
        lo = glm::vec3{model->mins[0], model->mins[1], model->mins[2]}; // (its Quake box follows its turn: solidBox)
        hi = glm::vec3{model->maxs[0], model->maxs[1], model->maxs[2]};
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

// Water, as the old solver's: things float (items, backpacks, thrown weapons: 60% under at rest); gibs sink, slowly.
constexpr float floatDensity = 1.f / 0.6f; // relative to water's (the lift's scale)
constexpr float sinkDensity = 0.5f;

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
    if(contents <= CONTENTS_CURRENT_0 && contents >= CONTENTS_CURRENT_DOWN)
    {
        contents = CONTENTS_WATER;
    }
    contents = VR_LiquidContents(sv.worldmodel, p, contents);
    return contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA;
}

// The part of the column from `lo` to `hi` (z) at `c` under water (0 to 1), by the BSP's liquid leaves. Every point
// it tries is on one column: the hull's walk down to that column's node is done once (MG3_PLAN.md M3-28: a body's
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

constexpr float smallPropSleepThreshold = 0.15f; // m/s: a prop with its own mass (a small gib) sleeps under it (Box3D's: 0.05)

// A small gib (vr_smallgibs.qc): a prop with its own mass (.vr_prop_mass).
[[nodiscard]] bool isSmallGib(edict_t* ent);
// A weapon or backpack a monster dropped (.vr_monster_drop): s since; -1 for anything else.
[[nodiscard]] float monsterDropAge(edict_t* ent);

constexpr float grenadeRestitution = 0.45f; // (Quake's bounce: 0.5; a steel ball on stone)

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

// ---------------------------------------------------------------------------------------------------------------------
// Box3D's world

struct Slot // what one edict is in the world (by its number)
{
    Kind kind{Kind::None};
    b3BodyId body{b3_nullBodyId};

    // What the body was made from: made again when it changes.
    const qmodel_t* model{nullptr};
    int frame{0};
    za::Array<float, 9> scale{};   // props: model_scale, model_scale_origin, model_offset
    glm::vec3 mins{0.f}, maxs{0.f}; // actors: Quake's box; props: the drawn box
    float radius{0.f};              // players: the capsule's

    // The entity as last written (props) or seen (kinematic ones): QC's changes are the differences.
    glm::vec3 origin{0.f}, angles{0.f}, velocity{0.f}, spin{0.f};
    glm::vec3 arrival{0.f}; // props: the velocity this frame's step began with (0 asleep): what a touch after it sees
    float gravityScale{1.f};
    float massSetting{0.f}; // props: the Mass set for its model when it was made (vr_props.inc; 0: none)
    float massScale{1.f};   // props: its estimate's Mass x when it was made (props::massScale)
    float size{1.f};        // props: its model's Size when it was made (Held Object Offsets: props::drawnSize)
    bool asleep{false};
    bool wet{false};      // in water (its lift and drag: beforeStep)
    bool sleepless{false}; // Box3D's sleep off: floating (it bobs), or sinking through the water (beforeStep)
    float lift{0.f};       // N up: its lift in water this frame (beforeStep), given again in each piece of the step
    bool bullet{false};   // fast: continuous collision against other props too
    bool soft{false};     // isSoft
    bool brush{false};    // angles as a brush model's
    bool spins{false};    // a fixture drawn spinning (an EF_ROTATE model: the map's pickups): its shape turns with it
    physsound::Material sound{physsound::Material::None}; // what its knocks and scrapes sound like (vr_physsound.cpp)
    double born{0.0};     // the server's time its body was made (a prop: thrown, let go of, launched)
    int pushedStep{-100}; // props: the last step a hand's body pushed it (limitPushes: a hit, then a shove)
    bool flight{false};   // props: thrown by a hand and not yet touched anything (noteThrows; spinAlign)
    bool flightLogged{false}; // (vr_debug_spin_align: its first step printed; the last step's spin and how far off)
    float flightOff{0.f}, flightRate{0.f};
    const b3HullData* hull{nullptr}; // actors: the hull at rest (actorHull), nullptr for Quake's box
    // Corpses (vr_corpse_collide): the setting and the mask (corpseMask) its body was made with, and whether it is
    // dynamic (Pushable) and fitted to its pose (its hulls: corpseHulls) or a box.
    int corpseMode{0};
    uint64_t corpseMask{0};
    bool corpseDynamic{false};
    bool corpseFitted{false};
    float corpseFriction{0.f};
    int ragdoll{-1}; // a ragdoll (vr_ragdoll): its parts in World::ragdolls (body: its pelvis); -1 none
    // A pushable corpse's middle after the last step (carryCorpses: through a slipgate; its body made again as its
    // frame changes is the same corpse).
    glm::vec3 lastMid{0.f};
    bool midKnown{false};

    // Props, held and fixtures: the settings (shapeGeneration) and the entity's box its drawn box and Mass were last
    // found the same at (stale): looked at again only when one of them changes.
    unsigned checkedGeneration{0};
    glm::vec3 checkedMins{0.f}, checkedMaxs{0.f};
    bool checkedSolid{false};
};

// A ragdoll's parts (vr_ragdoll; "Ragdolls" below): one body per bone of its rig. num 0: a free entry.
struct RagdollBodies
{
    int num{0};
    const ragdoll::Rig* rig{nullptr};
    float scale{1.f};
    int count{0};
    za::Array<b3BodyId, ragdoll::maxBones> body{};
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // its entity's yaw when it was made (the frames' bones turned by it)
    double born{0.0};
    // Its head cut off (cutHead; ROUND21.md, "Decapitation"): those bones (bits) have no body nor joint now; their
    // entries in `body` name the part they were cut from (the loops over the parts stay valid: those that add up or push
    // per part skip them, partCut). And the head as it was cut (ragdollCut): its middle, turn, launch and spin (world).
    // Limb gore (cutLimb): any limb so; the head* fields are the last piece cut, `lastCut` its joint, `lastCutBones` what it
    // took.
    uint32_t cut{0};
    glm::vec3 headMid{0.f}, headVel{0.f}, headSpin{0.f};
    glm::quat headRot{1.f, 0.f, 0.f, 0.f};
    int lastCut{-1};
    uint32_t lastCutBones{0};
    // Its parts' motion times this at its next step (feedRagdoll, after the frame's knocks): a head pop's body barely
    // moves (ragdollDecap's settle, vr_decap_pop_body_speed); 1 none pending. Only the knock is scaled: each part's own
    // motion (metres, radians: the monster's walk and its animation's, or a corpse's as it lay; vr_decap_own_motion) is
    // kept.
    float settle{1.f};
    za::Array<glm::vec3, ragdoll::maxBones> ownLin{}, ownAng{};
    // A living monster knocked down (vr_knockdown; "Knockdowns" below): its loose pieces (a weapon) welded to the part
    // that held them, until it gets up or dies.
    bool knocked{false};
    za::Array<glm::quat, ragdoll::maxBones> struggleRest{};
    bool struggleReady{false};
    int shockJerk{-1}; // shocked (shockRagdoll): the jerk it last bucked or not at
    // Its pelvis after the last step (carryRagdolls: the step it goes in through a slipgate carries it whole).
    glm::vec3 lastPelvis{0.f};
    bool pelvisKnown{false};
};

// A knocked-down monster getting up ("Knockdowns"): its ragdoll's last pose blended into its animation as it plays.
struct Recovery
{
    int num{0};
    const ragdoll::Rig* rig{nullptr};
    float scale{1.f};
    int count{0};
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    double start{0.0};
    float duration{0.35f};
    int frame{-1}, prevFrame{-1};
    double changed{0.0}; // the server time its frame changed (the think that changed it: due then)
    double due{0.0};     // its next think, as of the last step
};

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

// The hulls made for models, by what they were made from (and the world's scale).
struct PropHullKey
{
    const qmodel_t* model;
    int frame;
    za::Array<float, 6> box;
    bool operator==(const PropHullKey&) const = default;
};

struct PropHullKeyHash
{
    [[nodiscard]] za::U64 operator()(const PropHullKey& k) const
    {
        za::U64 h = ankerl::unordered_dense::hash<const void*>{}(k.model) ^ (static_cast<za::U64>(static_cast<za::U32>(k.frame)) * 0x9E3779B97F4A7C15ull);
        for(const float f : k.box)
        {
            // (+ 0.f: -0 and +0 are one key, as they are equal)
            h = (h ^ ZA_BIT_CAST(za::U32, f + 0.f)) * 0x100000001B3ull;
        }
        return h;
    }
};

// A prop's hardest hit in a frame, for its .vr_impact (an explosive box dropped or thrown).
struct Shock
{
    int num{0};
    int other{0}; // what it hit (0: the level)
    float speed{0.f}; // m/s, the contact points' approach
};

// A prop near a hand's body before a step, and its motion then (notePushed, limitPushes).
struct Pushed
{
    int num{0};
    glm::vec3 velocity{0.f}, spin{0.f}; // m/s, rad/s
};

// Box3D on the pool (vr_box3d_threads; ROUND21.md, "Box3D on the pool"): a task of Box3D's step
// (b3EnqueueTaskCallback) as a jobs::Task, run by a worker of the game's pool or by the stepping thread as it waits for
// it (finishStepTask). The worker borrows the stepping thread's QuakeC VM (qcvm and pr_global_struct are thread-local):
// the callbacks it may call (shouldCollide, preSolve) read the entities.
struct StepTask
{
    jobs::Task task;
    b3TaskCallback* fn{nullptr};
    void* context{nullptr};
    qcvm_t* vm{nullptr};
    globalvars_t* globals{nullptr};
};

// A world's (its b3WorldDef's userTaskContext): its workers and the tasks of the step under way.
struct StepTasks
{
    int workers{1}; // Box3D's (stepWorkers; 1: the step on the stepping thread alone)
    za::Array<StepTask, B3_MAX_TASKS + 8> tasks; // a step's (Box3D enqueues at most B3_MAX_TASKS)
    za::Atomic<int> next{0};                      // the step's next in `tasks` (0 before each step)
};

// vr_physics_frametime's phases of a server frame's physics (VR_PhysicsFrameEnd's, and SV_Physics's round them).
enum FramePhase
{
    PhaseServerPhysics, // SV_Physics from the world's turn to the end of Box3D's frame: the entities' thinks and moves too
    PhaseBox3D,         // VR_PhysicsFrameEnd, all of it
    PhaseSync,          // the entities into their bodies (syncEntities, hands, reach)
    PhaseBefore,        // the water and the hit boxes before the step (beforeStep, standing, shoves)
    PhaseStep,          // the step's pieces with their touches (notePushed .. traceGibContacts)
    PhaseSolver,        // b3World_Step alone
    PhaseWrite,         // the bodies into their entities (writeProp, writeRagdoll, writeCorpse)
    PhaseAfter,         // the touches' QC (SV_Impact), the shocks, watchInside, the sounds
    framePhases
};
constexpr const char* framePhaseNames[framePhases] = {"server physics", "box3d", "sync", "before", "step", "solver", "write", "after"};

struct World
{
    b3WorldId id{};
    StepTasks tasks;
    // vr_physics_steptime: the steps' time since it last printed.
    double stepTime{0.0}, stepTimeMax{0.0}; // s, a frame's
    int stepFrames{0};
    za::U64 stepAwake{0}; // the awake bodies, summed over the frames
    // vr_physics_steptime: each frame's step since it last printed (its median, 95th percentile; by awake bodies).
    struct StepSample
    {
        float ms{0.f};
        int awake{0}; // the awake bodies as the step began (what stepWorkers saw)
    };
    za::Vector<StepSample> stepSamples;
    b3Profile stepProfile{}; // Box3D's own (ms), summed over the steps (vr_physics_steptime bins)
    // vr_physics_frametime: each server frame's physics by phase (ms; framePhaseNames) since it last printed.
    struct FrameSample
    {
        float ms[framePhases]{};
        int awake{0}; // the awake bodies after the step
    };
    za::Vector<FrameSample> frameSamples;
    const qmodel_t* map{nullptr};
    int generation{-1};
    float m2u{1.f};      // units a metre
    float gravity{0.f};  // sv_gravity at the last update
    float friction{-1.f}, restitution{-1.f};
    b3MeshData* mesh{nullptr}; // (meshCache's)
    b3ShapeId worldShape{b3_nullShapeId};
    za::Vector<za::Pair<int, int>> impacts; // the step's touches (kept: no allocation a frame)
    za::Vector<Shock> shocks; // props with a .vr_impact hitting something this frame (the hardest hit each)
    za::Vector<Shock> falls;  // knocked-down monsters' ragdolls hitting the level this frame (the hardest, vertically)
    za::Vector<Pushed> pushed; // the props near the hands' bodies before this step (kept: no allocation a frame)
    struct PortalCopy
    {
        b3BodyId original{b3_nullBodyId}, copy{b3_nullBodyId};
        glm::mat3 turn{1.f};
        glm::vec3 shift{0.f}, velocity{0.f}, spin{0.f};
        glm::vec4 here{0.f}, there{0.f};
        bool dynamic{false};
    };
    za::Vector<PortalCopy> portalCopies;
    za::Vector<Slot> slots; // by edict number
    // The players' hands (by client, [0] off, [1] main): kinematic spheres at their fists that push solid props; and
    // their reach bodies (syncReach): the empty hand's, or the held weapon's.
    struct ReachKey
    {
        enum What : uint8_t
        {
            None,
            Open,    // an open hand: its box (the palm, the fingers)
            Fist,    // a fist (the grip held)
            Weapon,  // the held weapon's drawn hull (view::drawnWeapon)
            Capsule, // a held weapon not drawn here (another player's): the hand to its muzzle
        };
        What what{None};
        const qmodel_t* model{nullptr};
        bool mirrored{false};
        unsigned generation{0};
        float scale{0.f};
        glm::vec3 muzzle{0.f}; // (Capsule) in the hand's frame
        bool operator==(const ReachKey&) const = default;
    };
    struct HandBody
    {
        b3BodyId body{b3_nullBodyId};
        bool armed{false};
        glm::vec3 at{0.f};
        b3BodyId reach{b3_nullBodyId};
        ReachKey key{};
        za::Vector<int> ignore; // props the reach body passes through until clear of them (inside it as it was made)
        int held{0};             // the prop the hand carried last frame (0: none)
        float weaponMass{0.f};   // the held weapon's own mass (kg: Weapon Weights' Mass; 0: none, or not a weapon)
        za::Vector<glm::vec4> fist; // the push body's spheres (the drawn fist, in the hand's frame; empty: the one sphere)
    };
    za::Vector<za::Array<HandBody, 2>> hands;
    za::Vector<glm::vec3> walkOrigins; // physical walking also nudges nonblocking corpses
    // Throws (vr_box3d_throw_grace, noteThrows): what was thrown passes through its thrower's hands' bodies a moment.
    struct Grace
    {
        int num{0};             // the thing thrown
        int player{0};          // its thrower (client)
        double born{0.0};       // its body's (Slot::born): the same body still
        double until{0.0};      // the server's time it meets the hands again
        bool skips{true};       // false: only watched (vr_debug_box3d with the grace off: its numbers, for comparing)
        glm::vec3 velocity{0.f}; // m/s, rad/s: as it left the hand
        glm::vec3 spin{0.f};
        // The hands' contacts it passed through (steps): counted by preSolve, on Box3D's workers (relaxed; copied with
        // the grace).
        struct Count
        {
            za::Atomic<int> n{0};
            Count() = default;
            Count(const Count& o) noexcept : n{o.n.loadRelaxed()} {}
            Count& operator=(const Count& o) noexcept
            {
                n.storeRelaxed(o.n.loadRelaxed());
                return *this;
            }
        } skipped;
    };
    za::Vector<Grace> graces;
    za::Vector<int> made; // the props whose bodies were made this frame (createBody): what may have been thrown
    // Small gibs and monster drops whose bodies were made inside a monster's or a player's (noteBornInside): they pass
    // through it until clear of it (shouldCollide), as a hand's reach body passes through what was inside it
    // (HandBody::ignore).
    struct Inside
    {
        int prop{0};      // the small gib or drop
        int other{0};     // the monster or player
        double born{0.0}; // the small gib's body's (Slot::born): the same body still
    };
    za::Vector<Inside> inside;
    // The players standing on solid props (by client; vr_box3d_player_stand, "Standing on props" below).
    struct Stand
    {
        int ground{0};          // the prop stood on at the frame's end (0: none): this frame's, then the last's
        glm::vec3 feet{0.f};    // the point stood on (units): under the player's origin, at its feet, in the prop's box
        b3Vec3 local{};         // that point in the prop's body before this frame's step
        float top{0.f};         // the top of the prop's Quake box then (units): what his box stands on
        // A jump's push on the prop jumped from, spread over jumpPushTime (a force, not a blow: see beforeStanding).
        int pushed{0};          // the prop (0: none)
        b3Vec3 pushLocal{};     // where, in its body
        float pushForce{0.f};   // N, down
        float pushLeft{0.f};    // s
    };
    za::Vector<Stand> stands;
    // A player's move blocked by a solid prop's side this frame (VR_PlayerBumps): he shoves it (shoveBumped).
    struct Bump
    {
        int player{0};
        int num{0};
        glm::vec3 dir{0.f};     // horizontal, into the prop
        float speed{0.f};       // units/s, his speed into it
    };
    za::Vector<Bump> bumps;
    ankerl::unordered_dense::map<PropHullKey, b3HullData*, PropHullKeyHash> propHulls; // nullptr: no hull (a box instead)
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<b3HullData*>> moverHulls;
    // Corpses' hulls fitted to their pose (corpseHulls): by model, frame and scale.
    ankerl::unordered_dense::map<PropHullKey, za::Vector<b3HullData*>, PropHullKeyHash> corpseHulls;
    // By edict: a dead monster's frame and model, and since when (the server's time) it has had them (watchCorpses: a
    // death animation over, for a corpse the QC doesn't say lies still); -1: not a dead monster.
    struct CorpseWatch
    {
        float frame{-1.f};
        float model{0.f};
        double since{-1.0};
        glm::vec3 origin{0.f}, last{0.f}; // where it is, and where it was the server frame before (createRagdoll)
        double changed{-1.0};              // when its frame last changed from one seen (not as it was first seen: a load)
        bool dying{false}; // seen dead but still solid (its frame and origin kept): the frame it stops being solid in
                           // is known to be new or not (the fiend's death6: SOLID_NOT and the frame together)
    };
    za::Vector<CorpseWatch> corpseWatch;
    // By edict: a monster's (live or dying) last three places its origin changed to, and when (watchCorpses; its own
    // motion: ownMotion), and its velocity the frame before (what the frame's knock added to it); n how many are known.
    struct MonsterMotion
    {
        za::Array<glm::vec3, 3> at{};
        za::Array<double, 3> when{};
        int n{0};
        glm::vec3 vel{0.f};
    };
    za::Vector<MonsterMotion> motion;
    za::Vector<RagdollBodies> ragdolls; // (Slot::ragdoll)
    // A full ragdoll pool (vr_ragdoll_max): the corpses waiting for QC to make room (VR_Knockdown_MakeRoom), run after
    // syncEntities' edict loop rather than inside it; and the ones it refused, left as corpses until they are gone.
    za::Vector<int> roomWanted, roomRefused;
    bool syncingEntities{false};
    za::Vector<Recovery> recoveries;    // knocked-down monsters getting up (Knockdowns)
    // Hands holding a ragdoll's limb (vr_ragdoll_grab; "Ragdolls"): a kinematic body at the hand and a motor joint to
    // the limb; or a force grab's pull flying the limb to the hand (no joint yet).
    struct RagdollGrab
    {
        int player{0};
        int hand{0}; // QC's: 0 the off hand, 1 the main
        int num{0};  // the ragdoll's entity
        int part{0};
        b3BodyId anchor{b3_nullBodyId};
        b3JointId joint{b3_nullJointId};
        bool pulling{false};
        double arrive{0.0};    // a pull: when it is due at the hand
        double farSince{-1.0}; // held: since when the limb is far from the hand (stuck behind something: let go of)
        glm::vec3 grip{0.f};   // a pull: the limb's point flown to the hand (its body's space, m)
    };
    za::Vector<RagdollGrab> ragdollGrabs;
    // Ragdolls made in each other (two grunts dying on one spot, vr_ragdoll_collide_each): they pass through each other
    // until no part of one is in the other (pruneRagdollsInside), as a small gib made in a body does.
    struct RagdollPair
    {
        int a{0}, b{0};
    };
    za::Vector<RagdollPair> ragdollsInside;
    // The last blasts (box3d::blast), for the ragdolls made just after them (createRagdoll: a grunt a blast killed).
    struct RecentBlast
    {
        glm::vec3 at{0.f};
        float damage{0.f};
        double time{-1e9};
    };
    za::Array<RecentBlast, 8> recentBlasts{};
    int nextBlast{0};
    bool ragdollsWarmed{false}; // the map's ragdoll models rigged (syncEntities: at its first frame with vr_ragdoll on)
    int steps{0};

    [[nodiscard]] b3Vec3 toM(const glm::vec3& u) const { return b3Vec3{u.x / m2u, u.y / m2u, u.z / m2u}; }
    [[nodiscard]] glm::vec3 toU(const b3Vec3& m) const { return glm::vec3{m.x, m.y, m.z} * m2u; }
};

za::UniquePtr<World> world;
constexpr za::SizeT stepSamplesMax = 1u << 16; // vr_physics_steptime's frames kept (15 minutes at 72 Hz)
double serverPhysicsStart = 0.0; // SV_Physics's world's turn this frame (Sys_DoubleTime; noteServerPhysicsStart)
bool frameTiming = false;        // vr_physics_frametime asked for once: its samples kept from then on
double frameLastWall = 0.0;      // vr_physics_frametime: the wall clock when it last printed
int frameLastHostFrame = 0;      // and at which host frame

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

// Box3D's b3EnqueueTaskCallback (`user`: the world's StepTasks): on the pool; with one worker, no pool or past the bound, here and now
// (nullptr: nothing to finish).
void* enqueueStepTask(b3TaskCallback* fn, void* context, void* user, const char*)
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
void finishStepTask(void* task, void*)
{
    static_cast<StepTask*>(task)->task.wait();
}

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

[[nodiscard]] b3Quat toB3(const glm::quat& q)
{
    return b3Quat{b3Vec3{q.x, q.y, q.z}, q.w};
}

[[nodiscard]] glm::quat fromB3(const b3Quat& q)
{
    return glm::normalize(glm::quat{q.s, q.v.x, q.v.y, q.v.z});
}

[[nodiscard]] b3Vec3 b3v(const glm::vec3& v)
{
    return b3Vec3{v.x, v.y, v.z};
}

[[nodiscard]] glm::vec3 glmv(const b3Vec3& v)
{
    return glm::vec3{v.x, v.y, v.z};
}

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

void dropGrabsOf(int num); // (below: "Ragdolls")

void destroyBody(Slot& s)
{
    if(s.ragdoll >= 0 && s.ragdoll < static_cast<int>(world->ragdolls.size()))
    {
        // A ragdoll's parts (their joints go with them), the hands holding it, and the client's drawing of it.
        RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
        dropGrabsOf(r.num);
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

// The world model's faces as a triangle mesh (metres), sky and liquids left out, wound counter-clockwise seen from
// the open side (a face's plane, flipped for SURF_PLANEBACK). Vertices shared by the BSP's faces stay shared, so
// Box3D finds the triangles' neighbours (identifyEdges: no bumps at inner edges).
//
// T-junctions: the BSP splits faces where they meet others, and a face's edge often has another face's corner in its
// middle (one of the BSP's shared vertices, but not the face's). Two triangles then share no edge there, and a box
// sliding over the seam can catch on it. Such corners are put into the edge (every drawn face's edges are checked
// against the corners near them), and a face that got any is fanned from a new vertex in its middle, so that every
// piece of its outline is a triangle's edge, shared with the neighbour's.
struct MeshStats
{
    int faces{0}, triangles{0}, junctions{0}, junctionFaces{0}, middleFans{0};
    double ms{0.0};
};

jobs::Site meshSite{"box3d world mesh"}; // (its parallelFor: vr_jobs_sites)

// (Only reads the map: made on the game's thread pool while the map spawns, see beforeLoad.)
[[nodiscard]] b3MeshData* worldMesh(const qmodel_t* map, float m2u, bool junctions, MeshStats& stats)
{
    const double t0 = Sys_DoubleTime();
    struct Face
    {
        int first, count; // into corners
        glm::vec3 normal;
    };
    za::Vector<Face> faces;
    za::Vector<int> corners; // BSP vertex numbers, each face's outline in order
    za::Vector<uint8_t> used(static_cast<size_t>(map->numvertexes), 0);
    const auto position = [&](int v) { return vec(map->vertexes[v].position); };
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB) || surf.numedges < 3)
        {
            continue;
        }
        glm::vec3 normal = vec(surf.plane->normal);
        if(surf.flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        faces.pushBack({static_cast<int>(corners.size()), surf.numedges, normal});
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
            corners.pushBack(v);
            used[static_cast<size_t>(v)] = 1;
        }
    }

    // The corners in a grid (cells of `cell` units, sorted by cell), to find those near an edge.
    constexpr float cell = 64.f;
    constexpr float onEdge = 0.1f; // units from the edge's line: in it
    const auto cellOf = [](const glm::vec3& p) { return glm::ivec3{glm::floor(p / cell)}; };
    const auto keyOf = [](const glm::ivec3& c) {
        const auto part = [](int v) { return static_cast<uint64_t>(static_cast<uint32_t>(v) & 0x1fffffu); };
        return (part(c.x) << 42) | (part(c.y) << 21) | part(c.z);
    };
    za::Vector<za::Pair<uint64_t, int>> grid;
    for(int v = 0; v < map->numvertexes; v++)
    {
        if(used[static_cast<size_t>(v)])
        {
            grid.emplaceBack(keyOf(cellOf(position(v))), v);
        }
    }
    za::quickSort(grid.begin(), grid.end());
    // Each cell's run in the grid (looked up by key: a binary search of the grid for each of 27 cells round each step
    // along each edge was most of vrstart's 570 ms mesh, 90k faces).
    ankerl::unordered_dense::map<uint64_t, za::Pair<int, int>> cellRuns;
    cellRuns.reserve(grid.size());
    for(za::SizeT i = 0; i < grid.size();)
    {
        za::SizeT j = i + 1;
        while(j < grid.size() && grid[j].first == grid[i].first)
        {
            ++j;
        }
        cellRuns.emplace(grid[i].first, za::makePair(static_cast<int>(i), static_cast<int>(j)));
        i = j;
    }

    za::Vector<int32_t> remap(static_cast<size_t>(map->numvertexes), -1);
    za::Vector<b3Vec3> vertices;
    za::Vector<int32_t> indices;
    const auto index = [&](int v) {
        int32_t& r = remap[static_cast<size_t>(v)];
        if(r < 0)
        {
            r = static_cast<int32_t>(vertices.size());
            const glm::vec3 p = position(v) / m2u;
            vertices.pushBack(b3Vec3{p.x, p.y, p.z});
        }
        return r;
    };
    const auto triangle = [&](int32_t a, int32_t b, int32_t c, const glm::vec3& pa, const glm::vec3& pb, const glm::vec3& pc,
                              const glm::vec3& normal) {
        const glm::vec3 n = glm::cross(pb - pa, pc - pa);
        if(glm::length(n) < 1e-3f)
        {
            return; // degenerate
        }
        if(glm::dot(n, normal) < 0.f)
        {
            za::genericSwap(b, c);
        }
        indices.pushBack(a);
        indices.pushBack(b);
        indices.pushBack(c);
    };

    // A face's outline: its corners, and the T-junctions put into its edges. Found for runs of faces at once on the pool
    // (each run's outlines one after the other; read only: the map, the grid), then made into triangles in the faces'
    // order, as on one thread (the same mesh; vrstart's 90k faces: 430 ms of searching on one thread).
    struct Outlines
    {
        za::Vector<int> outline;
        za::Vector<uint8_t> corner; // per outline entry: one of the face's own corners (else a T-junction put in)
        za::Vector<int> start;      // [face of the run]: its outline's first entry (and one past the last's end)
        za::Vector<uint8_t> split;  // [face of the run]: it got a T-junction
        int junctions{0};
    };
    const auto findOutline = [&](const Face& f, Outlines& o, za::Vector<za::Pair<float, int>>& between,
                                 za::Vector<uint64_t>& nearCells) {
        za::Vector<int>& outline = o.outline;
        za::Vector<uint8_t>& corner = o.corner;
        bool split = false;
        for(int k = 0; k < f.count; k++)
        {
            const int a = corners[static_cast<size_t>(f.first + k)], b = corners[static_cast<size_t>(f.first + (k + 1) % f.count)];
            outline.pushBack(a);
            corner.pushBack(1);
            if(!junctions)
            {
                continue;
            }
            const glm::vec3 pa = position(a), pb = position(b);
            const glm::vec3 d = pb - pa;
            const float length = glm::length(d);
            if(length < 4.f * onEdge)
            {
                continue;
            }
            const glm::vec3 dir = d / length;
            // The cells along the edge (and those round them), in steps of half a cell; each looked in once (a corner
            // found twice was found at the same t, and dropped as one below either way).
            between.clear();
            nearCells.clear();
            const int steps = za::max(1, static_cast<int>(za::ceil(length / (cell * 0.5f))));
            glm::ivec3 last{INT32_MIN};
            for(int step = 0; step <= steps; step++)
            {
                const glm::ivec3 c = cellOf(pa + d * (static_cast<float>(step) / static_cast<float>(steps)));
                if(c == last)
                {
                    continue;
                }
                last = c;
                for(int dz = -1; dz <= 1; dz++)
                {
                    for(int dy = -1; dy <= 1; dy++)
                    {
                        for(int dx = -1; dx <= 1; dx++)
                        {
                            nearCells.pushBack(keyOf(c + glm::ivec3{dx, dy, dz}));
                        }
                    }
                }
            }
            za::quickSort(nearCells.begin(), nearCells.end());
            for(za::SizeT i = 0; i < nearCells.size(); ++i)
            {
                if(i > 0 && nearCells[i] == nearCells[i - 1])
                {
                    continue;
                }
                const auto run = cellRuns.find(nearCells[i]);
                if(run == cellRuns.end())
                {
                    continue;
                }
                for(int g = run->second.first; g < run->second.second; ++g)
                {
                    const int v = grid[static_cast<za::SizeT>(g)].second;
                    if(v == a || v == b)
                    {
                        continue;
                    }
                    const glm::vec3 p = position(v);
                    const float t = glm::dot(p - pa, dir);
                    if(t > 2.f * onEdge && t < length - 2.f * onEdge && glm::length(p - (pa + dir * t)) <= onEdge)
                    {
                        between.emplaceBack(t, v);
                    }
                }
            }
            if(between.empty())
            {
                continue;
            }
            za::quickSort(between.begin(), between.end());
            float lastT = 0.f;
            for(const auto& [t, v] : between)
            {
                if(t - lastT < 2.f * onEdge)
                {
                    continue; // (found from two cells, or two BSP vertices at one place)
                }
                lastT = t;
                outline.pushBack(v);
                corner.pushBack(0);
                o.junctions++;
                split = true;
            }
        }
        return split;
    };
    constexpr za::SizeT runFaces = 1024;
    za::Vector<Outlines> runs((faces.size() + runFaces - 1) / runFaces);
    jobs::parallelFor(meshSite, runs.size(), 1,
        [&](za::SizeT begin, za::SizeT end)
        {
            za::Vector<za::Pair<float, int>> between;
            za::Vector<uint64_t> nearCells;
            for(za::SizeT r = begin; r < end; ++r)
            {
                Outlines& o = runs[r];
                const za::SizeT last = za::min(faces.size(), (r + 1) * runFaces);
                for(za::SizeT i = r * runFaces; i < last; ++i)
                {
                    o.start.pushBack(static_cast<int>(o.outline.size()));
                    o.split.pushBack(findOutline(faces[i], o, between, nearCells) ? 1 : 0);
                }
                o.start.pushBack(static_cast<int>(o.outline.size()));
            }
        });
    za::Vector<int> outline;
    za::Vector<uint8_t> corner;
    for(za::SizeT i = 0; i < faces.size(); ++i)
    {
        const Face& f = faces[i];
        const Outlines& o = runs[i / runFaces];
        const za::SizeT j = i % runFaces;
        const auto first = static_cast<za::SizeT>(o.start[j]), past = static_cast<za::SizeT>(o.start[j + 1]);
        outline.clear();
        corner.clear();
        outline.emplaceBackRange(o.outline.data() + first, past - first);
        corner.emplaceBackRange(o.corner.data() + first, past - first);
        const bool split = o.split[j] != 0;
        stats.faces++;
        if(!split)
        {
            const int a = outline[0];
            for(size_t k = 2; k < outline.size(); k++)
            {
                triangle(index(a), index(outline[k - 1]), index(outline[k]), position(a), position(outline[k - 1]), position(outline[k]),
                    f.normal);
            }
            continue;
        }
        // Fanned from one of its corners whose two edges have no junction in them (the fan's triangles then have every
        // piece of the outline as an edge, none of them flat), else from a new vertex in its middle.
        stats.junctionFaces++;
        const size_t n = outline.size();
        size_t from = n;
        for(size_t k = 0; k < n && from == n; k++)
        {
            if(corner[k] && corner[(k + 1) % n] && corner[(k + n - 1) % n])
            {
                from = k;
            }
        }
        if(from < n)
        {
            const int a = outline[from];
            for(size_t k = 2; k < n; k++)
            {
                const int b = outline[(from + k - 1) % n], c = outline[(from + k) % n];
                triangle(index(a), index(b), index(c), position(a), position(b), position(c), f.normal);
            }
            continue;
        }
        stats.middleFans++;
        glm::vec3 middle{0.f};
        for(const int v : outline)
        {
            middle += position(v);
        }
        middle /= static_cast<float>(outline.size());
        const int32_t m = static_cast<int32_t>(vertices.size());
        vertices.pushBack(b3Vec3{middle.x / m2u, middle.y / m2u, middle.z / m2u});
        for(size_t k = 0; k < outline.size(); k++)
        {
            const int a = outline[k], b = outline[(k + 1) % outline.size()];
            triangle(m, index(a), index(b), middle, position(a), position(b), f.normal);
        }
    }
    for(const Outlines& o : runs)
    {
        stats.junctions += o.junctions;
    }
    stats.triangles = static_cast<int>(indices.size() / 3);
    b3MeshData* mesh = nullptr;
    if(!indices.empty())
    {
        b3MeshDef def{};
        def.vertices = vertices.data();
        def.stride = 0;
        def.indices = indices.data();
        def.materialIndices = nullptr;
        def.vertexCount = static_cast<int>(vertices.size());
        def.triangleCount = static_cast<int>(indices.size() / 3);
        def.weldVertices = false;
        def.weldTolerance = 0.f;
        def.useMedianSplit = false;
        def.identifyEdges = true;
        def.clockWiseWinding = false;
        mesh = b3CreateMesh(&def, nullptr, 0);
    }
    stats.ms = (Sys_DoubleTime() - t0) * 1000.0;
    return mesh;
}

// The world's mesh is made once per map and world scale and kept while the map is (a saved game loaded, a restart:
// Box3D's world is made again, not the mesh). A few scales are kept: a take's replay (vr_motion_play) sets the world
// scale it was recorded at and puts the user's back, and each eval take loads the map at the user's scale first; the
// mesh (in metres) isn't made again for either (35 ms each, twice per take).
struct MeshCache
{
    char name[MAX_QPATH]{};
    int vertexes{0}, surfaces{0};
    float m2u{0.f};
    bool junctions{true};
    b3MeshData* mesh{nullptr};
    MeshStats stats;
};
za::Vector<MeshCache> meshCache; // this map's, the latest used last
constexpr size_t meshCacheScales = 4;

[[nodiscard]] bool sameMap(const MeshCache& c, const qmodel_t* map)
{
    return !strcmp(c.name, map->name) && c.vertexes == map->numvertexes && c.surfaces == map->numsurfaces &&
           c.junctions == (vr_box3d_mesh_junctions.value != 0.f);
}

void settleMesh();

[[nodiscard]] MeshCache* findMesh(const qmodel_t* map, float m2u)
{
    settleMesh(); // (the one beforeLoad made, if any)
    for(MeshCache& c : meshCache)
    {
        if(c.m2u == m2u && sameMap(c, map))
        {
            return &c;
        }
    }
    return nullptr;
}

// Whether `mesh` is this map's, as the settings want it (else the world is made again: destroyed first, as its shape
// uses the mesh).
[[nodiscard]] bool meshCurrent(const qmodel_t* map, float m2u, const b3MeshData* mesh)
{
    const MeshCache* c = findMesh(map, m2u);
    return c && c->mesh == mesh;
}

// (Only with the world destroyed: a mesh let go may be the one its shape used.)
// A mesh made, into the cache (the latest used last): another map's (or other settings'), and the least recently used
// scale beyond the few kept, let go first.
MeshCache& addMesh(const MeshCache& made, const qmodel_t* map)
{
    for(auto it = meshCache.begin(); it != meshCache.end();)
    {
        if(!sameMap(*it, map) || meshCache.size() >= meshCacheScales)
        {
            if(it->mesh)
            {
                b3DestroyMesh(it->mesh);
            }
            it = meshCache.erase(it);
        }
        else
        {
            ++it;
        }
    }
    MeshCache& c = meshCache.emplaceBack(made);
    if(vr_debug_box3d.value)
    {
        // (its bytes' hash, FNV-1a: the same made on the pool or not)
        uint32_t hash = 2166136261u;
        const auto* bytes = reinterpret_cast<const unsigned char*>(c.mesh);
        for(int32_t i = 0; c.mesh && i < c.mesh->byteCount; i++)
        {
            hash = (hash ^ bytes[i]) * 16777619u;
        }
        Con_Printf("box3d: the mesh of %s: %d faces, %d triangles, %d T-junctions joined in %d faces (%d fanned from the middle), %.1f ms, hash %08x\n", c.name,
            c.stats.faces, c.stats.triangles, c.stats.junctions, c.stats.junctionFaces, c.stats.middleFans, c.stats.ms,
            static_cast<unsigned>(hash));
    }
    return c;
}

// The map's mesh made on the game's thread pool while the map spawns (beforeLoad), for the cache: put there at the
// first look at the cache after (findMesh), or when the map's memory is about to go (finishLoads).
struct PendingMesh
{
    jobs::Future<b3MeshData*> job;
    MeshCache key; // its map, scale and settings (its mesh: the job's)
    const qmodel_t* map{nullptr};
};
PendingMesh pendingMesh;

void settleMesh()
{
    if(!pendingMesh.job.valid())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    MeshCache made = pendingMesh.key;
    made.mesh = pendingMesh.job.get();
    made.stats = pendingMesh.key.stats;
    VR_TimeAdd("box3d: the world's mesh, waited for (made on the pool)", Sys_DoubleTime() - t0);
    (void)addMesh(made, pendingMesh.map);
}

[[nodiscard]] b3MeshData* cachedWorldMesh(const qmodel_t* map, float m2u)
{
    if(MeshCache* found = findMesh(map, m2u))
    {
        za::rotate(found, found + 1, meshCache.data() + meshCache.size()); // the latest used last
        return meshCache.back().mesh;
    }
    MeshCache made;
    q_strlcpy(made.name, map->name, sizeof(made.name));
    made.vertexes = map->numvertexes;
    made.surfaces = map->numsurfaces;
    made.m2u = m2u;
    made.junctions = vr_box3d_mesh_junctions.value != 0.f;
    made.mesh = worldMesh(map, m2u, made.junctions, made.stats);
    return addMesh(made, map).mesh;
}

// A convex region as the intersection of half-spaces dot(n, p) <= d: its corners (triples of planes meeting inside
// all of them).
struct HalfSpace
{
    glm::dvec3 n;
    double d;
};

[[nodiscard]] za::Vector<b3Vec3> corners(const za::Vector<HalfSpace>& planes, float m2u)
{
    za::Vector<glm::dvec3> points;
    const size_t count = planes.size();
    for(size_t i = 0; i < count; i++)
    {
        for(size_t j = i + 1; j < count; j++)
        {
            const glm::dvec3 ij = glm::cross(planes[i].n, planes[j].n);
            if(glm::dot(ij, ij) < 1e-12)
            {
                continue;
            }
            for(size_t k = j + 1; k < count; k++)
            {
                const double det = glm::dot(ij, planes[k].n);
                if(za::abs(det) < 1e-9)
                {
                    continue;
                }
                const glm::dvec3 p = (glm::cross(planes[j].n, planes[k].n) * planes[i].d +
                                         glm::cross(planes[k].n, planes[i].n) * planes[j].d + ij * planes[k].d) /
                                     det;
                bool inside = true;
                for(size_t m = 0; m < count && inside; m++)
                {
                    inside = glm::dot(planes[m].n, p) <= planes[m].d + 0.01;
                }
                if(inside && !za::anyOf(points.begin(), points.end(), [&](const glm::dvec3& q) { return glm::distance(p, q) < 0.05; }))
                {
                    points.pushBack(p);
                }
            }
        }
    }
    za::Vector<b3Vec3> out;
    out.reserve(points.size());
    for(const glm::dvec3& p : points)
    {
        out.pushBack(b3Vec3{static_cast<float>(p.x / m2u), static_cast<float>(p.y / m2u), static_cast<float>(p.z / m2u)});
    }
    return out;
}

// The solid leaves of a brush model's hull 0 (the drawn brushes' space, split by the BSP), each a convex region.
void solidLeaves(const hull_t& hull, int num, za::Vector<HalfSpace>& path, float m2u, za::Vector<b3HullData*>& out, int depth)
{
    if(num < 0)
    {
        if(num == CONTENTS_SOLID)
        {
            const za::Vector<b3Vec3> points = corners(path, m2u);
            if(points.size() >= 4)
            {
                if(b3HullData* h = b3CreateHull(points.data(), static_cast<int>(points.size()), 64))
                {
                    out.pushBack(h);
                }
            }
        }
        return;
    }
    if(depth > 100 || num > hull.lastclipnode)
    {
        return;
    }
    const mclipnode_t& node = hull.clipnodes[num];
    const mplane_t& plane = hull.planes[node.planenum];
    const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
    path.pushBack({-n, -static_cast<double>(plane.dist)}); // in front: dot(n, p) >= dist
    solidLeaves(hull, node.children[0], path, m2u, out, depth + 1);
    path.back() = {n, static_cast<double>(plane.dist)}; // behind
    solidLeaves(hull, node.children[1], path, m2u, out, depth + 1);
    path.popBack();
}

[[nodiscard]] const za::Vector<b3HullData*>& moverHulls(qmodel_t* model)
{
    auto it = world->moverHulls.find(model);
    if(it != world->moverHulls.end())
    {
        return it->second;
    }
    za::Vector<b3HullData*> hulls;
    const hull_t& hull = model->hulls[0];
    if(hull.clipnodes && hull.planes)
    {
        za::Vector<HalfSpace> path;
        for(int i = 0; i < 3; i++) // the model's bounds close the leaves open to the outside
        {
            glm::dvec3 n{0.0};
            n[i] = 1.0;
            path.pushBack({n, static_cast<double>(model->maxs[i]) + 1.0});
            path.pushBack({-n, -static_cast<double>(model->mins[i]) + 1.0});
        }
        solidLeaves(hull, hull.firstclipnode, path, world->m2u, hulls, 0);
    }
    if(hulls.empty()) // nothing solid found: its bounds
    {
        const glm::vec3 lo = vec(model->mins), hi = vec(model->maxs);
        za::Array<b3Vec3, 8> points;
        for(int i = 0; i < 8; i++)
        {
            points[i] = world->toM(glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z});
        }
        if(b3HullData* h = b3CreateHull(points.data(), 8, 8))
        {
            hulls.pushBack(h);
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s: %d convex pieces\n", model->name, static_cast<int>(hulls.size()));
    }
    return world->moverHulls.emplace(model, ZA_MOVE(hulls)).first->second;
}

// How far the drawn corners reach out of a hull made of some of them (the most any is beyond one of its faces).
[[nodiscard]] float hullShortfall(const b3HullData* hull, const za::Vector<b3Vec3>& points)
{
    const b3Plane* planes = b3GetHullPlanes(hull);
    float most = 0.f;
    for(const b3Vec3& p : points)
    {
        float out = -1e9f;
        for(int i = 0; i < hull->faceCount; i++)
        {
            out = za::max(out, b3Dot(planes[i].normal, p) - planes[i].offset);
        }
        most = za::max(most, out);
    }
    return most;
}

// A hull of the drawn corners (metres) with at least `budget` vertices, more as needed to leave none of them further
// out than hullTolerance units (Box3D's limit is 128).
constexpr float hullTolerance = 0.2f;
[[nodiscard]] b3HullData* fittedHull(const za::Vector<b3Vec3>& points, int budget)
{
    b3HullData* hull = b3CreateHull(points.data(), static_cast<int>(points.size()), budget);
    while(hull && budget < B3_MAX_HULL_VERTICES && hull->vertexCount >= budget &&
          hullShortfall(hull, points) * world->m2u > hullTolerance)
    {
        budget = za::min(budget * 3 / 2, B3_MAX_HULL_VERTICES);
        b3HullData* more = b3CreateHull(points.data(), static_cast<int>(points.size()), budget);
        if(!more)
        {
            break;
        }
        b3DestroyHull(hull);
        hull = more;
    }
    return hull;
}

// The prop's hull: the convex hull of its drawn surface (an alias model's frame, a brush model's faces: the ammo and
// health boxes as drawn, not Quake's padded box), or none (nullptr: its box).
[[nodiscard]] b3HullData* propHull(edict_t* ent, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi)
{
    const PropHullKey key{model, static_cast<int>(ent->v.frame), {lo.x, lo.y, lo.z, hi.x, hi.y, hi.z}};
    auto it = world->propHulls.find(key);
    if(it != world->propHulls.end())
    {
        return it->second;
    }
    b3HullData* hull = nullptr;
    za::Vector<glm::vec3>& vertices = scratch.propVerts;
    if(held::drawnVertices(ent, vertices) && vertices.size() >= 4)
    {
        za::Vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.pushBack(world->toM(v));
        }
        // A weapon's shape in detail (it rests on its side, its grip, its magazine); the rest a little blockier
        // (a backpack, a gib: a rounded hull rolls down a slope, a real one's give stops it). But never short of
        // the drawn surface by more than hullTolerance: the hull is a subset of the drawn corners (Box3D's quickhull
        // stops at the budget), and a corner left out is drawn inside what the body rests on (a corpse's 16-corner
        // hull left its drawn back 1.3 units in the floor). The budget grows until every drawn corner is within it.
        hull = fittedHull(points, isWeaponLike(ent) ? 32 : 16);
        // A flat model's hull is thinner than Box3D can collide well: its box instead.
        if(hull && hull->innerRadius * world->m2u < 0.25f)
        {
            b3DestroyHull(hull);
            hull = nullptr;
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s frame %d: %s\n", model->name, key.frame,
            hull ? va("a hull of %d vertices, %.0f cm^3", hull->vertexCount, hull->volume * 1e6f) : "a box");
    }
    world->propHulls.emplace(key, hull);
    return hull;
}

// A monster's (or another solid alias model's) hull: its drawn model at rest (frame 0, its first stand frame), as
// props' are (propHull's cache, frame -1), turned with its yaw: props rest against the monster, not against Quake's
// box round it (a grunt's is 32 units wide, its drawn body half that). The one shape for all its frames: a body's
// shape made again as it animates would lose its contacts (their warm start, and a new touch each time). nullptr: its
// box.
[[nodiscard]] b3HullData* actorHull(edict_t* ent, qmodel_t* model)
{
    const PropHullKey key{model, -1, {}};
    auto it = world->propHulls.find(key);
    if(it != world->propHulls.end())
    {
        return it->second;
    }
    b3HullData* hull = nullptr;
    za::Vector<glm::vec3>& vertices = scratch.actorVerts;
    const float frame = ent->v.frame;
    ent->v.frame = 0.f;
    const bool drawn = held::drawnVertices(ent, vertices);
    ent->v.frame = frame;
    if(drawn && vertices.size() >= 4)
    {
        za::Vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.pushBack(world->toM(v));
        }
        hull = b3CreateHull(points.data(), static_cast<int>(points.size()), 24);
        if(hull && hull->innerRadius * world->m2u < 2.f)
        {
            b3DestroyHull(hull); // a thin thing (a sprite-like model): Quake's box
            hull = nullptr;
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s at rest: %s\n", model->name,
            hull ? va("a hull of %d vertices, %.0f litres", hull->vertexCount, hull->volume * 1e3f) : "its box");
    }
    world->propHulls.emplace(key, hull);
    return hull;
}

[[nodiscard]] b3ShapeDef shapeDef(int num, uint64_t category, uint64_t mask)
{
    b3ShapeDef def = b3DefaultShapeDef();
    def.userData = userOf(num);
    def.filter.categoryBits = category;
    def.filter.maskBits = mask;
    def.baseMaterial.friction = za::max(vr_throw_friction.value, 0.f);
    def.baseMaterial.restitution = 0.f;
    def.density = 1.f;
    return def;
}

// The prop's shapes on `body`: its hull or its box.
void addPropShapes(edict_t* ent, int num, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi, b3BodyId body, bool held)
{
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
    if(hull)
    {
        b3CreateHullShape(body, &def, hull);
        return;
    }
    const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((lo + hi) * 0.5f));
    b3CreateHullShape(body, &def, &box.base);
}

[[nodiscard]] bool isRigid(edict_t* ent)
{
    const int ofs = fields().vr_rigid;
    return ofs >= 0 && fieldFloat(ent, ofs) != 0.f;
}

[[nodiscard]] bool isCorpse(edict_t* ent, int num); // (below)
[[nodiscard]] int corpseMode();
[[nodiscard]] bool wantsRagdoll(edict_t* ent, int num); // (below: "Ragdolls")

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
constexpr float corpseTop = 24.f;    // units: a corpse's box no taller (VR_Corpse_Arm's lying box's most)
constexpr float corpseStep = 16.f;   // units: a corpse a player or monster steps over is this high to them (Quake's step: 18)
constexpr float corpsePiece = 24.f;  // units: the pieces of a fitted shape about this long (corpseHulls)

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
    za::Array<b3ShapeId, 8> shapes;
    const int count = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(s.body);
    za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int k = 0; k < count; k++)
    {
        if(b3Shape_GetType(shapes[k]) != b3_hullShape)
        {
            continue;
        }
        const b3HullData* hull = b3Shape_GetHull(shapes[k]);
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
        const b3ShapeProxy proxy{points.data(), n, 0.f};
        b3World_OverlapShape(world->id, xf.p, &proxy, filter,
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

// ----------------------------------------------------------------------------
// Ragdolls (vr_ragdoll; experimental, the monsters with a rig: vr_ragdoll.cpp seedTables; ROUND21.md, "Ragdolls" to
// "Ragdolls 4"). A dying grunt (dead, not solid: his
// death code's third frame on), once his death animation is vr_ragdoll_start of the way through, becomes a ragdoll: one
// dynamic body per bone of his rig (vr_ragdoll.cpp: derived from his model's animation), made where his drawn frame has
// each bone and moving as the animation moved it, jointed at the rig's pivots (balls with cone and twist limits, hinges
// with a range; a motor holding each still up to vr_ragdoll_joint_friction: joint friction), a loose piece (his
// shotgun) a body of its own. A corpse's kind, categories, mask and passes (Kind::Corpse, catCorpse, corpseMask,
// shouldCollide, noteCorpseInside), its own parts not meeting each other (its group). Its entity follows its pelvis
// (writeRagdoll: origin, its box round all the parts), touchable and not solid as before: shots, blows, flames, gibbing
// and corpse damage are the QC's; a shot's push (physicspush), a blast (physicsblast) and a QC knock move the parts.
// The client draws its mesh skinned to the parts (vr_ragdoll.cpp: published after each step). At most vr_ragdoll_max:
// more dead grunts lie as corpses. A loaded game's: made again from the frame he lies in.

constexpr double recentBlastTime = 0.6; // s: a blast this recent throws a ragdoll made now (createRagdoll)

// Its settings: the global ones (Gibs and Corpses > Ragdolls), each replaced for a monster class by its own when that is
// not -1 (Ragdolls > Grunt: vr_ragdoll_army_*; > Knight: vr_ragdoll_knight_*). A row a class.
enum class Tune : uint8_t
{
    Start,
    Mass,
    Friction,
    JointFriction,
    JointStiffness,
    Limits,
    Damping,
    Blast,
    Inherit,
    Count
};

struct RagdollClass
{
    const char* classname;
    // The head its QC throws (ThrowHead): its prop's Mass -1 weighs what this class's head cut off its ragdoll does
    // (headPropMass). The mummy's is the zombie's model, weighed as the zombie's.
    const char* head;
    za::Array<cvar_t*, static_cast<za::SizeT>(Tune::Count)> own;
};

const za::Array<cvar_t*, static_cast<za::SizeT>(Tune::Count)> ragdollGlobals{&vr_ragdoll_start, &vr_ragdoll_mass,
    &vr_ragdoll_friction, &vr_ragdoll_joint_friction, &vr_ragdoll_joint_stiffness, &vr_ragdoll_limits, &vr_ragdoll_damping,
    &vr_ragdoll_blast, &vr_ragdoll_inherit};

const RagdollClass ragdollClasses[] = {
    {"monster_army", "progs/h_guard.mdl",
        {&vr_ragdoll_army_start, &vr_ragdoll_army_mass, &vr_ragdoll_army_friction, &vr_ragdoll_army_joint_friction,
            &vr_ragdoll_army_joint_stiffness, &vr_ragdoll_army_limits, &vr_ragdoll_army_damping, &vr_ragdoll_army_blast,
            &vr_ragdoll_army_inherit}},
    {"monster_knight", "progs/h_knight.mdl",
        {&vr_ragdoll_knight_start, &vr_ragdoll_knight_mass, &vr_ragdoll_knight_friction,
            &vr_ragdoll_knight_joint_friction, &vr_ragdoll_knight_joint_stiffness, &vr_ragdoll_knight_limits,
            &vr_ragdoll_knight_damping, &vr_ragdoll_knight_blast, &vr_ragdoll_knight_inherit}},
    {"monster_ogre", "progs/h_ogre.mdl",
        {&vr_ragdoll_ogre_start, &vr_ragdoll_ogre_mass, &vr_ragdoll_ogre_friction, &vr_ragdoll_ogre_joint_friction,
            &vr_ragdoll_ogre_joint_stiffness, &vr_ragdoll_ogre_limits, &vr_ragdoll_ogre_damping, &vr_ragdoll_ogre_blast,
            &vr_ragdoll_ogre_inherit}},
    {"monster_ogre_marksman", "progs/h_ogre.mdl",
        {&vr_ragdoll_ogre_start, &vr_ragdoll_ogre_mass, &vr_ragdoll_ogre_friction, &vr_ragdoll_ogre_joint_friction,
            &vr_ragdoll_ogre_joint_stiffness, &vr_ragdoll_ogre_limits, &vr_ragdoll_ogre_damping, &vr_ragdoll_ogre_blast,
            &vr_ragdoll_ogre_inherit}},
    {"monster_enforcer", "progs/h_mega.mdl",
        {&vr_ragdoll_enforcer_start, &vr_ragdoll_enforcer_mass, &vr_ragdoll_enforcer_friction,
            &vr_ragdoll_enforcer_joint_friction, &vr_ragdoll_enforcer_joint_stiffness, &vr_ragdoll_enforcer_limits,
            &vr_ragdoll_enforcer_damping, &vr_ragdoll_enforcer_blast, &vr_ragdoll_enforcer_inherit}},
    {"monster_hell_knight", "progs/h_hellkn.mdl",
        {&vr_ragdoll_hknight_start, &vr_ragdoll_hknight_mass, &vr_ragdoll_hknight_friction,
            &vr_ragdoll_hknight_joint_friction, &vr_ragdoll_hknight_joint_stiffness, &vr_ragdoll_hknight_limits,
            &vr_ragdoll_hknight_damping, &vr_ragdoll_hknight_blast, &vr_ragdoll_hknight_inherit}},
    {"monster_dog", "progs/h_dog.mdl",
        {&vr_ragdoll_dog_start, &vr_ragdoll_dog_mass, &vr_ragdoll_dog_friction, &vr_ragdoll_dog_joint_friction,
            &vr_ragdoll_dog_joint_stiffness, &vr_ragdoll_dog_limits, &vr_ragdoll_dog_damping, &vr_ragdoll_dog_blast,
            &vr_ragdoll_dog_inherit}},
    {"monster_wizard", "progs/h_wizard.mdl",
        {&vr_ragdoll_wizard_start, &vr_ragdoll_wizard_mass, &vr_ragdoll_wizard_friction,
            &vr_ragdoll_wizard_joint_friction, &vr_ragdoll_wizard_joint_stiffness, &vr_ragdoll_wizard_limits,
            &vr_ragdoll_wizard_damping, &vr_ragdoll_wizard_blast, &vr_ragdoll_wizard_inherit}},
    {"monster_zombie", "progs/h_zombie.mdl",
        {&vr_ragdoll_zombie_start, &vr_ragdoll_zombie_mass, &vr_ragdoll_zombie_friction,
            &vr_ragdoll_zombie_joint_friction, &vr_ragdoll_zombie_joint_stiffness, &vr_ragdoll_zombie_limits,
            &vr_ragdoll_zombie_damping, &vr_ragdoll_zombie_blast, &vr_ragdoll_zombie_inherit}},
    {"monster_demon1", "progs/h_demon.mdl",
        {&vr_ragdoll_demon_start, &vr_ragdoll_demon_mass, &vr_ragdoll_demon_friction, &vr_ragdoll_demon_joint_friction,
            &vr_ragdoll_demon_joint_stiffness, &vr_ragdoll_demon_limits, &vr_ragdoll_demon_damping,
            &vr_ragdoll_demon_blast, &vr_ragdoll_demon_inherit}},
    {"monster_shambler", "progs/h_shams.mdl",
        {&vr_ragdoll_shambler_start, &vr_ragdoll_shambler_mass, &vr_ragdoll_shambler_friction,
            &vr_ragdoll_shambler_joint_friction, &vr_ragdoll_shambler_joint_stiffness, &vr_ragdoll_shambler_limits,
            &vr_ragdoll_shambler_damping, &vr_ragdoll_shambler_blast, &vr_ragdoll_shambler_inherit}},
    {"monster_gremlin", "progs/h_grem.mdl",
        {&vr_ragdoll_gremlin_start, &vr_ragdoll_gremlin_mass, &vr_ragdoll_gremlin_friction,
            &vr_ragdoll_gremlin_joint_friction, &vr_ragdoll_gremlin_joint_stiffness, &vr_ragdoll_gremlin_limits,
            &vr_ragdoll_gremlin_damping, &vr_ragdoll_gremlin_blast, &vr_ragdoll_gremlin_inherit}},
    // Dawn of the Machine's ranged knight (QC vr_mg3_rknight.qc): a death knight's build, the death knight's settings.
    {"monster_ranged_knight", "progs/h_hellkn.mdl",
        {&vr_ragdoll_hknight_start, &vr_ragdoll_hknight_mass, &vr_ragdoll_hknight_friction,
            &vr_ragdoll_hknight_joint_friction, &vr_ragdoll_hknight_joint_stiffness, &vr_ragdoll_hknight_limits,
            &vr_ragdoll_hknight_damping, &vr_ragdoll_hknight_blast, &vr_ragdoll_hknight_inherit}},
    {"monster_mummy", nullptr,
        {&vr_ragdoll_mummy_start, &vr_ragdoll_mummy_mass, &vr_ragdoll_mummy_friction, &vr_ragdoll_mummy_joint_friction,
            &vr_ragdoll_mummy_joint_stiffness, &vr_ragdoll_mummy_limits, &vr_ragdoll_mummy_damping,
            &vr_ragdoll_mummy_blast, &vr_ragdoll_mummy_inherit}},
    {"monster_shalrath", "progs/h_shal.mdl",
        {&vr_ragdoll_vore_start, &vr_ragdoll_vore_mass, &vr_ragdoll_vore_friction, &vr_ragdoll_vore_joint_friction,
            &vr_ragdoll_vore_joint_stiffness, &vr_ragdoll_vore_limits, &vr_ragdoll_vore_damping, &vr_ragdoll_vore_blast,
            &vr_ragdoll_vore_inherit}},
    // Dawn of the Machine's super shambler: the shambler's settings (Ragdoll Settings > Shambler) and head gib.
    {"monster_super_shambler", "progs/h_shams.mdl",
        {&vr_ragdoll_shambler_start, &vr_ragdoll_shambler_mass, &vr_ragdoll_shambler_friction,
            &vr_ragdoll_shambler_joint_friction, &vr_ragdoll_shambler_joint_stiffness, &vr_ragdoll_shambler_limits,
            &vr_ragdoll_shambler_damping, &vr_ragdoll_shambler_blast, &vr_ragdoll_shambler_inherit}},
    {"monster_scourge", "progs/h_scourg.mdl",
        {&vr_ragdoll_centroid_start, &vr_ragdoll_centroid_mass, &vr_ragdoll_centroid_friction,
            &vr_ragdoll_centroid_joint_friction, &vr_ragdoll_centroid_joint_stiffness, &vr_ragdoll_centroid_limits,
            &vr_ragdoll_centroid_damping, &vr_ragdoll_centroid_blast, &vr_ragdoll_centroid_inherit}},
};

// The setting `t` for `ent`'s ragdoll: its class's own, else the global one.
[[nodiscard]] float tune(const edict_t* ent, Tune t)
{
    const auto i = static_cast<za::SizeT>(t);
    const char* classname = PR_GetString(ent->v.classname);
    for(const RagdollClass& c : ragdollClasses)
    {
        if(!strcmp(classname, c.classname) && c.own[i]->value >= 0.f)
        {
            return c.own[i]->value;
        }
    }
    return ragdollGlobals[i]->value;
}

void blastRagdoll(const RagdollBodies& r, const glm::vec3& at, float damage); // (below)

[[nodiscard]] int ragdollCount()
{
    int n = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        n += r.num > 0 ? 1 : 0;
    }
    return n;
}

[[nodiscard]] bool wantsRagdoll(edict_t* ent, int num)
{
    const bool knocked = knockedDown(ent);
    if(vr_ragdoll.value < 1.f || num <= svs.maxclients || !(knocked || deadMonster(ent)) || hasFlag(ent, FL_SWIM))
    {
        return false;
    }
    qmodel_t* model = modelOf(ent);
    if(!ragdoll::eligible(model))
    {
        return false;
    }
    if(num < static_cast<int>(world->slots.size()) && world->slots[num].ragdoll >= 0)
    {
        return true; // (already one: its model is checked by stale)
    }
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    if(!rig)
    {
        return false;
    }
    if(knocked)
    {
        return true; // (knocked down: at once, whatever Most Ragdolls: the QC made room, vr_knockdown.qc)
    }
    const float progress = ragdoll::deathProgress(*rig, static_cast<int>(ent->v.frame));
    // Beheaded: its ragdoll at once; a saved game's is made again so too.
    const bool headless = (fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f && rig->head >= 0) ||
                          fieldFloatOr(ent, fields().vr_limbcut, 0.f) != 0.f; // (a limb cut off: Limb gore)
    const bool limp = progress >= za::clamp(tune(ent, Tune::Start), 0.f, 1.f) || fieldFloatOr(ent, fields().vr_corpse, 0.f) >= 2.f ||
                      (progress < 0.f && isCorpse(ent, num)) || headless;
    return limp && vr_ragdoll_max.value >= 1.f;
}

// A rotation that takes +z to `d` (unit).
[[nodiscard]] glm::quat zTo(const glm::vec3& d)
{
    const glm::vec3 z{0.f, 0.f, 1.f};
    const float c = glm::dot(z, d);
    if(c < -0.9999f)
    {
        return glm::quat{0.f, 1.f, 0.f, 0.f};
    }
    const glm::vec3 axis = glm::cross(z, d);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}

[[nodiscard]] RagdollBodies* ragdollOf(int num); // (below)

[[nodiscard]] bool isRagdollSlot(int num)
{
    return num < static_cast<int>(world->slots.size()) && world->slots[num].ragdoll >= 0;
}

// Whether a part of ragdoll `r` is in one of edict `other`'s ragdoll's (their shapes, a centimetre less: parts only
// touching are not in each other).
[[nodiscard]] bool ragdollIn(const RagdollBodies& r, int other)
{
    struct Query
    {
        int other;
        bool found;
    } q{other, false};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catCorpse;
    filter.maskBits = catCorpse;
    constexpr float sink = 0.01f; // m
    for(int b = 0; b < r.count && !q.found; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        za::Array<b3ShapeId, 4> shapes;
        const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
        const b3WorldTransform xf = b3Body_GetTransform(body);
        for(int k = 0; k < count && !q.found; k++)
        {
            za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
            int n = 0;
            float radius = 0.f;
            if(b3Shape_GetType(shapes[k]) == b3_hullShape)
            {
                const b3HullData* hull = b3Shape_GetHull(shapes[k]);
                const b3Vec3* p = b3GetHullPoints(hull);
                const b3Vec3 c = hull->center;
                n = za::min(hull->vertexCount, static_cast<int>(points.size()));
                for(int v = 0; v < n; v++)
                {
                    b3Vec3 o = p[v];
                    o.x = o.x > c.x ? za::max(c.x, o.x - sink) : za::min(c.x, o.x + sink);
                    o.y = o.y > c.y ? za::max(c.y, o.y - sink) : za::min(c.y, o.y + sink);
                    o.z = o.z > c.z ? za::max(c.z, o.z - sink) : za::min(c.z, o.z + sink);
                    points[static_cast<za::SizeT>(v)] = b3RotateVector(xf.q, o);
                }
            }
            else if(b3Shape_GetType(shapes[k]) == b3_capsuleShape)
            {
                const b3Capsule cap = b3Shape_GetCapsule(shapes[k]);
                points[0] = b3RotateVector(xf.q, cap.center1);
                points[1] = b3RotateVector(xf.q, cap.center2);
                n = 2;
                radius = za::max(cap.radius - sink, 0.f);
            }
            if(n == 0)
            {
                continue;
            }
            const b3ShapeProxy proxy{points.data(), n, radius};
            b3World_OverlapShape(world->id, xf.p, &proxy, filter,
                [](b3ShapeId shape, void* raw) {
                    auto* qq = static_cast<Query*>(raw);
                    if(numOf(shape) == qq->other)
                    {
                        qq->found = true;
                        return false;
                    }
                    return true;
                },
                &q);
        }
    }
    return q.found;
}

// Whether edicts `a` and `b` are ragdolls made in each other (they pass through each other: shouldCollide).
[[nodiscard]] bool ragdollsInside(int a, int b)
{
    return za::anyOf(world->ragdollsInside.begin(), world->ragdollsInside.end(),
        [a, b](const World::RagdollPair& p) { return (p.a == a && p.b == b) || (p.a == b && p.b == a); });
}

// Once a frame: the ragdolls made in each other that are clear of each other now (or gone) meet again.
void pruneRagdollsInside()
{
    za::vectorEraseIf(world->ragdollsInside, [](const World::RagdollPair& p) {
        const RagdollBodies* a = ragdollOf(p.a);
        return !a || !ragdollOf(p.b) || !ragdollIn(*a, p.b);
    });
}

bool cutHead(RagdollBodies& r, edict_t* ent, const glm::vec3& blade, bool launch); // (below)
bool cutLimb(RagdollBodies& r, edict_t* ent, int bone, const glm::vec3& blade, bool launch);

// The QC globals a call into QC from the engine's middle (a builtin, the physics) may clobber and its caller may still
// read: the parameters and return value, self, other, msg_entity and the last trace's results. (Not the rest: Killed's
// counters, killed_monsters and the like, are meant to change.)
struct QcCallGuard
{
    globalvars_t saved;
    QcCallGuard() { memcpy(&saved, pr_global_struct, sizeof saved); }
    ~QcCallGuard()
    {
        globalvars_t& g = *pr_global_struct;
        memcpy(g.pad, saved.pad, sizeof g.pad);
        g.self = saved.self;
        g.other = saved.other;
        g.msg_entity = saved.msg_entity;
        g.trace_allsolid = saved.trace_allsolid;
        g.trace_startsolid = saved.trace_startsolid;
        g.trace_fraction = saved.trace_fraction;
        VectorCopy(saved.trace_endpos, g.trace_endpos);
        VectorCopy(saved.trace_plane_normal, g.trace_plane_normal);
        g.trace_plane_dist = saved.trace_plane_dist;
        g.trace_ent = saved.trace_ent;
        g.trace_inopen = saved.trace_inopen;
        g.trace_inwater = saved.trace_inwater;
    }
};

[[nodiscard]] bool roomRefused(int num)
{
    return za::anyOf(world->roomRefused.begin(), world->roomRefused.end(), [&](int n) { return n == num; });
}

// QC's VR_Knockdown_MakeRoom for `ent` (a ragdoll retired, a knockdown ended): whether a ragdoll may be made now. Its
// removed edicts' bodies go at once.
bool makeRagdollRoom(edict_t* ent)
{
    const func_t room = qvr::progs::findFunction("VR_Knockdown_MakeRoom");
    if(!room) { return false; }
    bool madeRoom = false;
    {
        QcCallGuard guard;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
        PR_ExecuteProgram(room);
        madeRoom = G_FLOAT(OFS_RETURN) != 0.f;
    }
    for(RagdollBodies& old : world->ragdolls)
    {
        if(old.num > 0 && EDICT_NUM(old.num)->free)
        {
            destroyBody(world->slots[old.num]);
        }
    }
    return madeRoom;
}

// Part `bone`'s hull of its vertices (rest units times `k`: metres), simplified to at most 20 corners (Box3D's hulls have
// at most 128 half edges); one that can't be made, its vertices' box (null: no vertices). `volume` (m^3): the hull's,
// and its capsule's. The ragdoll's parts weigh their share of its mass by it (makeRagdoll), and so does a limb cut off
// (limbMass).
[[nodiscard]] b3HullData* boneHull(const ragdoll::Bone& bone, float k, float& volume)
{
    za::Array<b3Vec3, 128> pts;
    const int n = za::min(static_cast<int>(bone.points.size()), static_cast<int>(pts.size()));
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 p = bone.points[static_cast<za::SizeT>(i)] * k;
        pts[static_cast<za::SizeT>(i)] = b3Vec3{p.x, p.y, p.z};
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    b3HullData* hull = nullptr;
    for(const int corners : {20, 12, 8})
    {
        hull = n >= 4 ? b3CreateHull(pts.data(), n, corners) : nullptr;
        if(hull)
        {
            break;
        }
    }
    if(!hull && n > 0)
    {
        const glm::vec3 c = (lo + hi) * 0.5f, h = glm::max((hi - lo) * 0.5f, glm::vec3{0.01f});
        za::Array<b3Vec3, 8> box;
        for(int i = 0; i < 8; i++)
        {
            box[static_cast<za::SizeT>(i)] = b3Vec3{c.x + (i & 1 ? h.x : -h.x), c.y + (i & 2 ? h.y : -h.y), c.z + (i & 4 ? h.z : -h.z)};
        }
        hull = b3CreateHull(box.data(), 8, 8);
    }
    float v = hull ? hull->volume : 0.f;
    if(bone.capsule > 0.f)
    {
        const float rad = bone.capsule * k, len = glm::length(bone.end - bone.pivot) * k;
        v += glm::pi<float>() * rad * rad * (len + 4.f / 3.f * rad);
    }
    volume = v;
    return hull;
}

// The ragdoll's parts made for `ent` where its frame has them; false (nothing made) without a rig. `now`: made at once
// from whatever frame it is in (beheaded: ragdollDecap), its loose piece hidden (what it held: its death code drops it).
bool createRagdoll(edict_t* ent, int num, Slot& s, bool now = false)
{
    qmodel_t* model = modelOf(ent);
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    if(!rig)
    {
        return false;
    }
    // All deaths use the same replacement policy as knockdowns. A full pool must not
    // strand a newly killed monster in its animated death pose.
    if(ragdollCount() >= static_cast<int>(za::max(1.f, vr_ragdoll_max.value)))
    {
        if(roomRefused(num))
        {
            return false; // (refused once: a corpse till it is gone)
        }
        if(world->syncingEntities)
        {
            // In syncEntities' edict loop: QC (which removes and spawns edicts, and kills) runs after it; the ragdoll
            // is made the next frame.
            if(!za::anyOf(world->roomWanted.begin(), world->roomWanted.end(), [&](int n) { return n == num; }))
            {
                world->roomWanted.pushBack(num);
            }
            return false;
        }
        if(!makeRagdollRoom(ent))
        {
            world->roomRefused.pushBack(num);
            return false;
        }
    }
    int index = -1;
    for(int i = 0; i < static_cast<int>(world->ragdolls.size()); i++)
    {
        if(world->ragdolls[static_cast<za::SizeT>(i)].num == 0)
        {
            index = i;
            break;
        }
    }
    if(index < 0)
    {
        index = static_cast<int>(world->ragdolls.size());
        world->ragdolls.pushBack(RagdollBodies{});
    }
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(index)];
    r = RagdollBodies{};
    r.num = num;
    r.rig = rig;
    // Its model's scale (a mapper's model_scale on the monster; uniform: its x), as it is drawn.
    r.scale = za::clamp(1.f + fieldVec(ent, fields().model_scale).x, 0.25f, 4.f);
    r.born = qcvm->time;
    r.knocked = knockedDown(ent);
    const float mass = za::max(tune(ent, Tune::Mass), 1.f), friction = za::max(tune(ent, Tune::Friction), 0.f);

    s.kind = Kind::Corpse;
    s.model = model;
    s.frame = static_cast<int>(ent->v.frame);
    s.corpseMode = corpseMode();
    s.corpseMask = corpseMask();
    s.corpseFitted = true;
    s.corpseDynamic = true;
    s.massSetting = mass;
    s.corpseFriction = friction;
    s.ragdoll = index;
    s.born = qcvm->time;

    // Where the frame has each bone, and how the animation moved it over its last frame (vr_ragdoll_inherit).
    const int frame = static_cast<int>(ent->v.frame);
    const int pose = ragdoll::poseOfFrame(model, frame);
    const float progress = ragdoll::deathProgress(*rig, frame);
    const int prevPose = progress > 0.f ? ragdoll::poseOfFrame(model, frame - 1) : pose;
    // Where the client draws him now: as his frame has just changed (this server frame), still the frame before (its lerp
    // to this one not begun); else this frame. The parts are made there, moving as the animation went on (the switch
    // unseen: the ragdoll's first frame is the animated mesh's last; vr_debug_ragdoll compares them).
    const bool justChanged = num < static_cast<int>(world->corpseWatch.size()) &&
                             world->corpseWatch[static_cast<za::SizeT>(num)].changed >= 0.0 &&
                             qcvm->time - world->corpseWatch[static_cast<za::SizeT>(num)].changed < 1e-6;
    const int drawnPose = progress > 0.f && justChanged ? prevPose : pose;
    // (And where it was drawn: the frame before's origin, his death code may have moved him with the frame: ai_back.)
    const glm::vec3 drawnOrigin = drawnPose != pose ? world->corpseWatch[static_cast<za::SizeT>(num)].last : vec(ent->v.origin);
    const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
    r.turn = turn;
    const glm::vec3 origin = vec(ent->v.origin);
    const float k = r.scale / world->m2u; // rest units -> body metres
    const float inherit = za::clamp(tune(ent, Tune::Inherit), 0.f, 2.f) / 0.1f; // (a frame is 0.1 s)
    // Beheaded (now: ragdollDecap) with vr_decap_own_motion: its own motion (its walk, ownMotion: a stepping monster's
    // velocity is none) kept apart from what this frame added to its velocity (the blow's knock), which a head pop scales.
    const bool own = now && vr_decap_own_motion.value != 0.f && num < static_cast<int>(world->motion.size());
    const glm::vec3 walk = own ? ownMotion(num) : glm::vec3{0.f};
    const glm::vec3 entVel = own ? walk + vec(ent->v.velocity) - world->motion[static_cast<za::SizeT>(num)].vel : vec(ent->v.velocity);
    // A loose piece the frame hides (all its vertices at one point: the grunt's shotgun in his death frames, dropped as
    // a weapon of its own, vr_monstermods.cpp) gets no body and stays hidden (published collapsed): the last ones.
    r.count = rig->numBones;
    const bool headless = fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f || fieldFloatOr(ent, fields().vr_limbcut, 0.f) != 0.f;
    while(r.count > 1 && rig->bones[r.count - 1].joint == ragdoll::Joint::Loose &&
          (now || headless || ragdoll::collapsed(*rig, pose, r.count - 1)))
    {
        r.count--;
    }

    // The parts' shapes and volumes first (their density: vr_ragdoll_mass over the body's; a loose piece weighs
    // looseMass): each part's hull of its vertices (boneHull).
    za::Array<b3HullData*, ragdoll::maxBones> hulls{};
    za::Array<float, ragdoll::maxBones> volumes{};
    float volume = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        float v = 0.f;
        hulls[static_cast<za::SizeT>(b)] = boneHull(bone, k, v);
        volumes[static_cast<za::SizeT>(b)] = v;
        volume += bone.joint == ragdoll::Joint::Loose ? 0.f : v;
    }
    constexpr float looseMass = 3.f; // kg: a loose piece (his shotgun)
    const float density = mass / za::max(volume, 1e-6f);

    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        glm::quat rot, prot;
        glm::vec3 pos, ppos;
        ragdoll::bonePose(*rig, pose, b, rot, pos);
        ragdoll::bonePose(*rig, prevPose, b, prot, ppos);
        glm::quat drot;
        glm::vec3 dpos;
        ragdoll::bonePose(*rig, drawnPose, b, drot, dpos);
        b3BodyDef def = b3DefaultBodyDef();
        def.userData = userOf(num);
        def.type = b3_dynamicBody;
        def.position = world->toM(drawnOrigin + turn * (dpos * r.scale));
        def.rotation = toB3(glm::normalize(turn * drot));
        def.linearDamping = 0.05f;
        def.angularDamping = za::max(tune(ent, Tune::Damping), 0.f);
        // Its motion: the middle of its vertices from the previous frame's place to this one's, and its turn.
        glm::vec3 mid{0.f};
        for(const glm::vec3& p : bone.points)
        {
            mid += p;
        }
        mid /= static_cast<float>(za::max<za::SizeT>(bone.points.size(), 1));
        const glm::vec3 now = turn * ((rot * mid + pos) * r.scale), before = turn * ((prot * mid + ppos) * r.scale);
        def.linearVelocity = world->toM(entVel + (now - before) * inherit);
        const glm::quat dq = glm::normalize(turn * (rot * glm::inverse(prot)) * glm::inverse(turn));
        const float angle = 2.f * za::acos(za::clamp(za::abs(dq.w), 0.f, 1.f));
        const glm::vec3 im{dq.x, dq.y, dq.z};
        const glm::vec3 axis = glm::length(im) > 1e-6f ? glm::normalize(im) * (dq.w < 0.f ? -1.f : 1.f) : glm::vec3{0.f};
        def.angularVelocity = b3v(axis * (angle * inherit));
        if(own)
        {
            r.ownLin[static_cast<za::SizeT>(b)] = glmv(world->toM(walk + (now - before) * inherit));
            r.ownAng[static_cast<za::SizeT>(b)] = axis * (angle * inherit);
        }
        const b3BodyId body = b3CreateBody(world->id, &def);
        r.body[static_cast<za::SizeT>(b)] = body;

        // (Other ragdolls too, vr_ragdoll_collide_each: theirs have catCorpse in their masks, a corpse's not.)
        b3ShapeDef sd = shapeDef(num, catCorpse, s.corpseMask | (vr_ragdoll_collide_each.value ? catCorpse : 0));
        sd.enableCustomFiltering = true; // (shouldCollide: thrown things, vr_corpse_collide_thrown; what it is made in)
        sd.filter.groupIndex = -num;     // (its own parts never meet)
        sd.enableHitEvents = true;       // (a knocked-down monster's fall damage: touches, callFalls)
        sd.baseMaterial.friction = friction;
        sd.density = bone.joint == ragdoll::Joint::Loose ? looseMass / za::max(volumes[static_cast<za::SizeT>(b)], 1e-6f) : density;
        if(b3HullData* hull = hulls[static_cast<za::SizeT>(b)])
        {
            b3CreateHullShape(body, &sd, hull);
            b3DestroyHull(hull); // (the world keeps its own copy)
        }
        else if(bone.capsule <= 0.f)
        {
            const b3BoxHull box = b3MakeCubeHull(1.f * k); // (no vertices: never so, but never a part without mass)
            b3CreateHullShape(body, &sd, &box.base);
        }
        if(bone.capsule > 0.f)
        {
            const glm::vec3 a = bone.pivot * k, c = bone.end * k;
            const b3Capsule capsule{b3Vec3{a.x, a.y, a.z}, b3Vec3{c.x, c.y, c.z}, bone.capsule * k};
            b3CreateCapsuleShape(body, &sd, &capsule);
        }
    }
    s.body = r.body[0];

    // The joints, at the rig's pivots: where the parent and the child have it in this frame (between them: the
    // animation's limbs stretch a little), in each one's rest space; their frames' z the bone's rest direction (a
    // ball: its cone about it) or the hinge's axis.
    const float jointFriction = za::max(tune(ent, Tune::JointFriction), 0.f);
    const float stiffness = za::max(tune(ent, Tune::JointStiffness), 0.f);
    const float limits = za::clamp(tune(ent, Tune::Limits), 0.05f, 3.f);
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        if(bone.parent < 0 || (bone.joint != ragdoll::Joint::Ball && bone.joint != ragdoll::Joint::Hinge))
        {
            continue;
        }
        glm::quat rp, rc;
        glm::vec3 tp, tc;
        ragdoll::bonePose(*rig, drawnPose, bone.parent, rp, tp);
        ragdoll::bonePose(*rig, drawnPose, b, rc, tc);
        const glm::vec3 at = ((rp * bone.pivot + tp) + (rc * bone.pivot + tc)) * 0.5f;
        const glm::vec3 inParent = glm::inverse(rp) * (at - tp) * k, inChild = glm::inverse(rc) * (at - tc) * k;
        const glm::vec3 dir = glm::length(bone.end - bone.pivot) > 1e-3f ? glm::normalize(bone.end - bone.pivot) : glm::vec3{0.f, 0.f, 1.f};
        const glm::quat frame = bone.joint == ragdoll::Joint::Hinge ? zTo(glm::normalize(bone.hinge)) : zTo(dir);
        const auto setBase = [&](b3JointDef& base) {
            base.bodyIdA = r.body[static_cast<za::SizeT>(bone.parent)];
            base.bodyIdB = r.body[static_cast<za::SizeT>(b)];
            base.localFrameA = b3Transform{b3v(inParent), toB3(frame)};
            base.localFrameB = b3Transform{b3v(inChild), toB3(frame)};
            base.collideConnected = false;
        };
        if(bone.joint == ragdoll::Joint::Ball)
        {
            b3SphericalJointDef jd = b3DefaultSphericalJointDef();
            setBase(jd.base);
            jd.enableConeLimit = true;
            jd.coneAngle = za::min(bone.cone * limits, glm::pi<float>());
            jd.enableTwistLimit = true;
            jd.lowerTwistAngle = -za::min(bone.twist * limits, 0.99f * glm::pi<float>());
            jd.upperTwistAngle = za::min(bone.twist * limits, 0.99f * glm::pi<float>());
            jd.enableMotor = jointFriction > 0.f;
            jd.maxMotorTorque = jointFriction;
            jd.enableSpring = stiffness > 0.f; // (towards the rest pose: how he stood)
            jd.hertz = stiffness;
            jd.dampingRatio = 1.f;
            (void)b3CreateSphericalJoint(world->id, &jd);
        }
        else
        {
            b3RevoluteJointDef jd = b3DefaultRevoluteJointDef();
            setBase(jd.base);
            jd.enableLimit = true;
            jd.lowerAngle = za::max(bone.lower * limits, -0.99f * glm::pi<float>());
            jd.upperAngle = za::min(bone.upper * limits, 0.99f * glm::pi<float>());
            jd.enableMotor = jointFriction > 0.f;
            jd.maxMotorTorque = jointFriction;
            jd.enableSpring = stiffness > 0.f;
            jd.hertz = stiffness;
            jd.dampingRatio = 1.f;
            (void)b3CreateRevoluteJoint(world->id, &jd);
        }
    }
    // Knocked down (vr_knockdown_weld): a loose piece (his gun) welded where it is to the part nearest it, his hand's.
    if(r.knocked && vr_knockdown_weld.value != 0.f)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(rig->bones[b].joint != ragdoll::Joint::Loose)
            {
                continue;
            }
            glm::quat lr;
            glm::vec3 lt;
            ragdoll::bonePose(*rig, drawnPose, b, lr, lt);
            glm::vec3 mid{0.f};
            for(const glm::vec3& p : rig->bones[b].points)
            {
                mid += p;
            }
            mid = lr * (mid / static_cast<float>(za::max<za::SizeT>(rig->bones[b].points.size(), 1))) + lt;
            int best = -1;
            float bestD = 1e30f;
            for(int o = 0; o < r.count; o++)
            {
                if(rig->bones[o].joint == ragdoll::Joint::Loose)
                {
                    continue;
                }
                glm::quat orot;
                glm::vec3 opos;
                ragdoll::bonePose(*rig, drawnPose, o, orot, opos);
                for(const glm::vec3& p : rig->bones[o].points)
                {
                    const float d = glm::distance(orot * p + opos, mid);
                    if(d < bestD)
                    {
                        bestD = d;
                        best = o;
                    }
                }
            }
            if(best < 0)
            {
                continue;
            }
            const b3BodyId a = r.body[static_cast<za::SizeT>(best)], c = r.body[static_cast<za::SizeT>(b)];
            const b3WorldTransform xa = b3Body_GetTransform(a), xc = b3Body_GetTransform(c);
            const glm::quat qa = fromB3(xa.q), qc = fromB3(xc.q);
            const glm::vec3 pa = glmv(xa.p), pc = glmv(xc.p);
            b3WeldJointDef jd = b3DefaultWeldJointDef();
            jd.base.bodyIdA = a;
            jd.base.bodyIdB = c;
            jd.base.localFrameA = b3Transform{b3v(glm::inverse(qa) * (pc - pa)), toB3(glm::normalize(glm::inverse(qa) * qc))};
            jd.base.localFrameB = b3Transform{b3Vec3_zero, b3Quat_identity};
            jd.base.collideConnected = false;
            (void)b3CreateWeldJoint(world->id, &jd);
        }
    }
    // Made in another ragdoll: they pass through each other until clear (ragdollsInside).
    for(const RagdollBodies& o : world->ragdolls)
    {
        if(vr_ragdoll_collide_each.value && o.num > 0 && o.num != num && ragdollIn(r, o.num))
        {
            world->ragdollsInside.pushBack(World::RagdollPair{num, o.num});
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d made in ragdoll %d: they pass through each other until clear\n", num, o.num);
            }
        }
    }
    for(int b = 0; b < r.count; b++)
    {
        Slot part = s; // (noteCorpseInside reads the slot's body: each part's in turn)
        part.body = r.body[static_cast<za::SizeT>(b)];
        noteCorpseInside(num, part);
    }
    // A blast that has just killed him (or hit him as he died) throws him now.
    for(const World::RecentBlast& rb : world->recentBlasts)
    {
        if(rb.damage > 0.f && qcvm->time - rb.time < recentBlastTime)
        {
            blastRagdoll(r, rb.at, rb.damage);
        }
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    s.origin = origin;
    s.asleep = false;
    // Beheaded before (a saved game's): made headless again.
    if(headless)
    {
        // Its limbs cut off before (Limb gore: .vr_limbcut, the bones; read first: each cut writes it): each cut's joint
        // (a cut bone whose parent wasn't).
        const auto limbs = static_cast<uint32_t>(za::max(fieldFloatOr(ent, fields().vr_limbcut, 0.f), 0.f));
        if(fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f)
        {
            (void)cutHead(r, ent, glm::vec3{0.f}, false);
        }
        for(int b = 1; b < r.count; b++)
        {
            const int parent = rig->bones[b].parent;
            if((limbs & (1u << b)) && parent >= 0 && !(limbs & (1u << parent)) && !partCut(r, b))
            {
                (void)cutLimb(r, ent, b, glm::vec3{0.f}, false);
            }
        }
    }
    // Its limbs taken by hand (vr_ragdoll_grab; QC's VR_Ragdoll_Handtouch), unless the QC gave it a hand touch of its own.
    if(const func_t touch = bindings().Ragdoll_Handtouch; touch && fields().handtouch >= 0 && !fieldFunc(ent, fields().handtouch))
    {
        fieldInt(ent, fields().handtouch) = static_cast<int>(touch);
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d %s limp at frame %d (%.0f%% of its death; made in pose %d%s): %d parts, %.1f kg, %.0f litres, %d ragdolls\n", num,
            PR_GetString(ent->v.classname), frame, progress * 100.f, drawnPose, drawnPose != pose ? ", the frame before's" : "",
            r.count, mass, volume * 1e3f, ragdollCount());
    }
    return true;
}

// A QC's knock (.velocity set: T_Damage, a blast's push) goes to all the parts; a QC's move far away (a teleport) takes
// them along.
void feedRagdoll(edict_t* ent, Slot& s)
{
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    // A small coordinated curl of the chest and nod of the head, about the pose
    // in which the body settled. Arms and legs follow through their joints.
    // Relative pose feedback bounds the motion rather than adding random torque.
    if(knockedDown(ent) && ent->v.health > 0.f && vr_knockdown_wiggle.value > 0.f && qcvm->time > r.born + 0.6)
    {
        const float t = static_cast<float>(qcvm->time - r.born);
        const float frequency = za::clamp(vr_knockdown_wiggle_frequency.value, 0.1f, 5.f);
        const float strength = za::clamp(vr_knockdown_wiggle.value, 0.f, 3.f);
        const float pause = za::clamp(vr_knockdown_wiggle_pause.value, 0.f, 5.f);
        const float burst = pause == 0.f ? 1.f : za::max(0.f, za::sin(t * 6.2831853f / (pause + 1.f)));
        for(int b = 1; b < r.count; ++b)
        {
            const ragdoll::Bone& bone = r.rig->bones[b];
            if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) ||
                (bone.role != modelmeta::BoneRole::Chest && bone.role != modelmeta::BoneRole::Head)) { continue; }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
            const glm::quat qp = fromB3(b3Body_GetTransform(parent).q), qb = fromB3(b3Body_GetTransform(body).q);
            const glm::quat relative = glm::inverse(qp) * qb;
            if(!r.struggleReady) { r.struggleRest[static_cast<za::SizeT>(b)] = relative; }
            const float angle = glm::radians(5.f) * strength * burst * burst *
                za::sin(t * frequency * 6.2831853f);
            const glm::quat target = qp * r.struggleRest[static_cast<za::SizeT>(b)] *
                glm::angleAxis(angle, glm::vec3{0.f, 1.f, 0.f});
            glm::quat delta = glm::normalize(target * glm::inverse(qb));
            if(delta.w < 0.f) { delta = -delta; }
            const glm::vec3 error{delta.x * 2.f, delta.y * 2.f, delta.z * 2.f};
            // A throw or grab must not pull it back to an obsolete resting pose.
            if(glm::length(error) > glm::radians(15.f))
            {
                r.struggleRest[static_cast<za::SizeT>(b)] = relative;
                continue;
            }
            const glm::vec3 speed = glmv(b3Body_GetAngularVelocity(body)) - glmv(b3Body_GetAngularVelocity(parent));
            glm::vec3 torque = error * 15.f - speed * 1.5f;
            const float errorLength = glm::length(error);
            if(errorLength > 1e-6f)
                torque += error / errorLength * tune(ent, Tune::JointFriction) * 1.35f *
                    za::clamp(errorLength / 0.005f, 0.f, 1.f);
            const float magnitude = glm::length(torque);
            const float limit = za::max(0.5f, tune(ent, Tune::JointFriction) * 1.8f);
            if(magnitude > limit) { torque *= limit / magnitude; }
            b3Body_ApplyTorque(body, b3v(torque), true);
            b3Body_ApplyTorque(parent, b3v(-torque), true);
        }
        r.struggleReady = true;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 moved = origin - s.origin;
    if(glm::length(moved) > 32.f)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3WorldTransform xf = b3Body_GetTransform(body);
            b3Body_SetTransform(body, b3v(glmv(xf.p) + glmv(world->toM(moved))), xf.q);
            b3Body_SetAwake(body, true);
        }
        s.origin = origin;
    }
    const glm::vec3 v = vec(ent->v.velocity);
    if(glm::length(v) > 1.f)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + glmv(world->toM(v))));
            b3Body_SetAwake(body, true);
        }
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d knocked by QC: %.0f u/s\n", NUM_FOR_EDICT(ent), glm::length(v));
        }
        ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    }
    if(r.settle != 1.f)
    {
        // (A head pop: the blast's knock and pushes, all of this frame's, in; the body keeps only this share of them, and
        // its own motion, ownLin, in full.)
        const float k = za::max(r.settle, 0.f);
        float before = 0.f, after = 0.f;
        glm::vec3 pelvisOwn{0.f}, pelvisAfter{0.f};
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const glm::vec3 lin = glmv(b3Body_GetLinearVelocity(body));
            const glm::vec3 own = r.ownLin[static_cast<za::SizeT>(b)], ownAng = r.ownAng[static_cast<za::SizeT>(b)];
            const glm::vec3 kept = own + (lin - own) * k;
            before = za::max(before, glm::length(lin) * world->m2u);
            after = za::max(after, glm::length(kept) * world->m2u);
            if(b == 0)
            {
                pelvisOwn = own * world->m2u;
                pelvisAfter = kept * world->m2u;
            }
            b3Body_SetLinearVelocity(body, b3v(kept));
            b3Body_SetAngularVelocity(body, b3v(ownAng + (glmv(b3Body_GetAngularVelocity(body)) - ownAng) * k));
        }
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d settled after its head popped: fastest part %.0f -> %.0f u/s; pelvis %.0f u/s (its own "
                       "motion %.0f u/s, %.0f level)\n", NUM_FOR_EDICT(ent), before, after, glm::length(pelvisAfter),
                glm::length(pelvisOwn), glm::length(glm::vec2{pelvisOwn}));
        }
        r.settle = 1.f;
    }
}

// ---- Shocked ragdolls (vr_shock_seizure; QC vr_shock.qc) ----
// A dead body the lightning struck (alive or dead: its shock goes on through its death) convulses while it is shocked
// (.vr_shock_until, over .vr_shock_len), as long as its arcs crawl over it (vr_shock.cpp drawBodyDeath): each
// limb is driven, relative to the part it hangs from, towards a turning speed that jerks one way then back (shockPulse a
// second), about an axis of its own that changes every few jerks, the chest now and then bucking up. Driven as a speed
// (not a force) and shared between the two parts by their masses, it is bounded (never faster than shockLimbSpeed times
// vr_shock_seizure), it pushes the body nowhere as a whole (what turns a limb turns its parent back) and it eases off as
// the arcs thin out, letting go over the last moment: the body falls still as the arcs end.

constexpr float shockPulse = 9.f;      // jerks a second
constexpr float shockLimbSpeed = 14.f; // rad/s: a limb's turning speed at vr_shock_seizure 1, the shock fresh
constexpr float shockGrip = 25.f;      // 1/s: how fast a limb takes that speed
constexpr float shockBuck = 0.9f;      // m/s: the chest's buck up at vr_shock_seizure 1, fresh
constexpr float shockBuckChance = 0.2f; // a jerk's chance to buck
constexpr float shockFlop = 3.f;        // m/s: a limb's flop up off the floor at vr_shock_seizure 1, fresh
constexpr float shockFlopChance = 0.45f; // a limb's chance to flop at each jerk
constexpr float shockLetGo = 0.4f;       // s: the convulsions let go over the shock's last moment

[[nodiscard]] float shockHash(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) / static_cast<float>(1u << 24);
}

// How shocked `ent` is now: 1 fresh .. 0 over (0: not).
[[nodiscard]] float shockLeft(edict_t* ent)
{
    if(fields().vr_shock_until < 0)
    {
        return 0.f;
    }
    const double until = fieldFloat(ent, fields().vr_shock_until);
    if(until <= qcvm->time)
    {
        return 0.f;
    }
    const double len = za::max(0.1, static_cast<double>(fieldFloatOr(ent, fields().vr_shock_len, 1.f)));
    return static_cast<float>(za::clamp((until - qcvm->time) / len, 0.0, 1.0));
}

// A part's turning inertia (kg m^2, as a ball's) about a point `lever` metres off its middle.
[[nodiscard]] float shockInertia(b3BodyId body, float lever)
{
    const b3Matrix3 i = b3Body_GetLocalRotationalInertia(body);
    return (i.cx.x + i.cy.y + i.cz.z) / 3.f + b3Body_GetMass(body) * lever * lever;
}

void shockRagdoll(edict_t* ent, Slot& s, float dt)
{
    const float left = shockLeft(ent);
    const float strength = za::clamp(vr_shock_seizure.value, 0.f, 3.f);
    if(left <= 0.f || strength <= 0.f || dt <= 0.f)
    {
        return;
    }
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    // Easing off as the arcs do (1 fresh .. 0.45 worn off), let go over the last shockLetGo seconds: still as they end.
    const float t = static_cast<float>(qcvm->time);
    const float secondsLeft = static_cast<float>(fieldFloat(ent, fields().vr_shock_until) - qcvm->time);
    const float x = za::clamp(secondsLeft / shockLetGo, 0.f, 1.f);
    const float fade = (0.45f + 0.55f * left) * x * x * (3.f - 2.f * x);
    const float amp = shockLimbSpeed * strength * fade;
    const float release = za::min(1.f, fade * 4.f); // (letting go as it ends)
    const float friction = za::max(tune(ent, Tune::JointFriction), 0.f);
    float spun = 0.f, pushed = 0.f;
    int driven = 0, awakeParts = 0;
    const uint32_t num = static_cast<uint32_t>(r.num);
    const uint32_t jerk = static_cast<uint32_t>(za::max(0.f, t * shockPulse));
    for(int b = 1; b < r.count; ++b)
    {
        const ragdoll::Bone& bone = r.rig->bones[b];
        if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) || bone.joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        const uint32_t ub = static_cast<uint32_t>(b);
        // An axis of its own for three jerks at a time, back and forth along it (each limb out of step with the next).
        const uint32_t phase = jerk + static_cast<uint32_t>(shockHash(num, ub, 7u) * 3.f);
        const glm::vec3 axis = glm::normalize(glm::vec3{shockHash(num, ub, phase / 3u * 3u + 1u) - 0.5f,
            shockHash(num, ub, phase / 3u * 3u + 2u) - 0.5f, shockHash(num, ub, phase / 3u * 3u + 3u) - 0.5f} + 1e-3f);
        const float sign = (phase & 1u) ? 1.f : -1.f;
        const float size = (0.45f + 0.55f * shockHash(num, ub, phase * 5u + 11u)) *
                           (bone.role == modelmeta::BoneRole::Chest ? 0.5f : 1.f);
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
        const glm::vec3 rel = glmv(b3Body_GetAngularVelocity(body)) - glmv(b3Body_GetAngularVelocity(parent));
        const glm::vec3 error = axis * (sign * size * amp) - rel;
        // The pair's inertia about the joint (each part's own, and its mass half the way between their middles off it).
        const float half = 0.5f * glm::length(glmv(b3Body_GetWorldCenter(body)) - glmv(b3Body_GetWorldCenter(parent)));
        const float ib = shockInertia(body, half), ip = shockInertia(parent, half);
        const float pair = ib * ip / za::max(ib + ip, 1e-6f);
        glm::vec3 torque = error * (pair * shockGrip);
        const float miss = glm::length(error);
        if(miss > 1e-4f)
        {
            torque += error / miss * (friction * za::min(1.f, miss / 0.5f)); // (the joint's friction overcome)
        }
        torque *= release;
        const float most = (pair * shockGrip * amp * 2.f + friction * 1.5f) * release;
        if(const float m = glm::length(torque); m > most)
        {
            torque *= most / m;
        }
        b3Body_ApplyTorque(body, b3v(torque), true);
        b3Body_ApplyTorque(parent, b3v(-torque), true);
        spun += glm::length(rel);
        pushed += glm::length(torque);
        awakeParts += b3Body_IsAwake(body) ? 1 : 0;
        ++driven;
    }
    if(vr_debug_ragdoll.value >= 2.f && (jerk % 9u) == 0u && static_cast<int>(jerk) != r.shockJerk)
    {
        Con_Printf("ragdoll: %d shocked: left %.2f, speed %.1f rad/s wanted, %.1f mean; torque %.2f N m mean (friction %.2f), %d of %d awake, dt %.4f\n", r.num, left, amp,
            driven ? spun / static_cast<float>(driven) : 0.f, driven ? pushed / static_cast<float>(driven) : 0.f, friction,
            awakeParts, driven, dt);
    }
    // Each jerk, as it starts: some limbs flop up off the floor (a lying limb's own drive can't overcome the floor's grip
    // on it), the part they hang from pushed the other way (the floor takes it: the body as a whole stays where it lies);
    // now and then the chest bucks up.
    if(static_cast<int>(jerk) != r.shockJerk)
    {
        r.shockJerk = static_cast<int>(jerk);
        const bool buck = shockHash(num, 99u, jerk) < shockBuckChance;
        for(int b = 1; b < r.count; ++b)
        {
            const ragdoll::Bone& bone = r.rig->bones[b];
            if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) || bone.joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            const uint32_t ub = static_cast<uint32_t>(b);
            const bool chest = bone.role == modelmeta::BoneRole::Chest;
            if(chest ? !buck : shockHash(num, ub, jerk * 7u + 3u) >= shockFlopChance)
            {
                continue;
            }
            const float speed = (chest ? shockBuck : shockFlop) * strength * fade * (0.6f + 0.4f * shockHash(num, ub, jerk * 7u + 4u));
            const glm::vec3 kick{(shockHash(num, ub, jerk * 7u + 5u) - 0.5f) * 0.6f * speed,
                (shockHash(num, ub, jerk * 7u + 6u) - 0.5f) * 0.6f * speed, speed};
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
            const float mb = b3Body_GetMass(body), mp = za::max(b3Body_GetMass(parent), 1e-4f);
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + kick));
            b3Body_SetLinearVelocity(parent, b3v(glmv(b3Body_GetLinearVelocity(parent)) - kick * za::min(1.f, mb / mp)));
            b3Body_SetAwake(body, true);
            if(vr_debug_ragdoll.value >= 3.f)
            {
                Con_Printf("ragdoll: %d part %d (mass %.1f, parent's %.1f) flops %.2f m/s, now %.2f\n", r.num, b, mb, mp,
                    glm::length(kick), glm::length(glmv(b3Body_GetLinearVelocity(body))));
            }
        }
    }
}

// After the step: the entity where its frame would have its pelvis where the ragdoll has it (a saved game's ragdoll is
// made again from its frame there: lying where it lay), its box round its parts (shots and blows meet the ragdoll's
// box, then its mesh: vr_hitmodel.cpp; then the QC's corpse damage), on the ground and still to Quake; the parts
// published for the client.
void writeRagdoll(edict_t* ent, Slot& s)
{
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    const ragdoll::Rig& rig = *r.rig;
    // Knocked down, then killed where it lay ("Knockdowns"): its loose pieces (its weapon, welded) go, as a dead one's
    // death frames hide them (the QC drops the weapon itself: vr_enemyguns.qc and the like).
    if(r.knocked && ent->v.health <= 0.f)
    {
        r.knocked = false;
        while(r.count > 1 && rig.bones[r.count - 1].joint == ragdoll::Joint::Loose)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(r.count - 1)];
            if(B3_IS_NON_NULL(body) && b3Body_IsValid(body))
            {
                b3DestroyBody(body);
            }
            r.body[static_cast<za::SizeT>(r.count - 1)] = b3_nullBodyId;
            r.count--;
        }
    }
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    glm::vec3 lo{1e30f}, hi{-1e30f};
    bool awake = false;
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3WorldTransform xf = b3Body_GetTransform(body);
        rot[static_cast<za::SizeT>(b)] = fromB3(xf.q);
        pos[static_cast<za::SizeT>(b)] = world->toU(xf.p);
        awake = awake || b3Body_IsAwake(body);
        if(rig.bones[b].joint == ragdoll::Joint::Loose || partCut(r, b))
        {
            continue; // (his shotgun lying apart: not in his box)
        }
        const b3AABB box = b3Body_ComputeAABB(body);
        lo = glm::min(lo, world->toU(box.lowerBound));
        hi = glm::max(hi, world->toU(box.upperBound));
    }
    glm::quat frameRot;
    glm::vec3 framePos;
    ragdoll::bonePose(rig, ragdoll::poseOfFrame(r.rig->model, static_cast<int>(ent->v.frame)), 0, frameRot, framePos);
    const glm::vec3 origin = pos[0] - r.turn * (framePos * r.scale);
    if(origin.z < world->map->mins[2] - 1024.f)
    {
        // (Fallen out of the world: it stops there, as a prop.)
        for(int b = 0; b < r.count; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b3Vec3_zero);
            b3Body_SetAngularVelocity(body, b3Vec3_zero);
            b3Body_SetAwake(body, false);
        }
    }
    store(origin, ent->v.origin);
    if(lo.x < hi.x)
    {
        const glm::vec3 mins = lo - origin, maxs = hi - origin;
        if(glm::any(glm::greaterThan(glm::abs(mins - vec(ent->v.mins)), glm::vec3{0.5f})) ||
            glm::any(glm::greaterThan(glm::abs(maxs - vec(ent->v.maxs)), glm::vec3{0.5f})))
        {
            store(mins, ent->v.mins);
            store(maxs, ent->v.maxs);
            store(maxs - mins, ent->v.size);
        }
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    setFlag(ent, FL_ONGROUND, true);
    SV_LinkEdict(ent, false);
    s.origin = origin;
    s.asleep = !awake;
    // The local player's hands holding its limbs (drawn following them between steps: vr_ragdoll_held_local), and the
    // time of the message after this step (qcvm->time is advanced after the physics: SV_Physics).
    int heldBy = 0;
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        if(g.num == r.num && !g.pulling && g.player == cl.viewentity && (g.hand == 0 || g.hand == 1))
        {
            heldBy |= 1 << g.hand;
        }
    }
    ragdoll::publish(r.num, r.rig, r.count, rot.data(), pos.data(), r.scale, qcvm->time + host_frametime, heldBy, r.cut);
}

// ----------------------------------------------------------------------------
// Knockdowns (vr_knockdown; QC vr_knockdown.qc; Combat > Knockdowns). A shove can knock a living monster with a rig down:
// the QC makes it touchable and not solid, .vr_knockdown 1, and its ragdoll is made at once (ragdollKnockdown) from the
// frame it is drawn in, its gun welded in its hand (vr_knockdown_weld). It is a ragdoll as a dead one is (hit on its
// limbs, grabbed, pushed, thrown; killed there, it simply stays one: its gun goes, the QC drops it). To get up
// (ragdollGetUp) the QC names the frame its get-up starts from (its own fall-and-rise frames' lowest, or a death's last
// to play backwards; two to choose from): the frame is fitted to how the body lies (its turn about the vertical and
// where, by its joints; the pelvis's tilt picks between two), room to stand is looked for there and around it
// (vr_knockdown_search: never through a wall, a floor or a ceiling), the hands holding it let go, and its parts go. Its
// last pose is then blended into the animation as it plays (vr_knockdown_blend: a Recovery, published as a ragdoll is
// until the blend is done).

[[nodiscard]] float smooth01(float t)
{
    t = za::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Bone `b` of `rig` animated in frame pose `pose`, for an entity at `origin` turned by `turn`: p_world = rot * (scale *
// p_rest) + pos, as the ragdolls' bodies.
void animatedBone(const ragdoll::Rig& rig, int pose, int b, const glm::quat& turn, const glm::vec3& origin, float scale,
    glm::quat& rot, glm::vec3& pos)
{
    glm::quat r;
    glm::vec3 t;
    ragdoll::bonePose(rig, pose, b, r, t);
    rot = glm::normalize(turn * r);
    pos = origin + turn * (t * scale);
}

// The getting-up monsters' poses, after each step: their ragdolls' last blended into their animation (its frames lerped
// as the client lerps them), published as ragdolls; done, the animated model is drawn again.
void updateRecoveries()
{
    for(za::SizeT i = 0; i < world->recoveries.size();)
    {
        Recovery& rec = world->recoveries[i];
        edict_t* ent = rec.num < qcvm->num_edicts ? EDICT_NUM(rec.num) : nullptr;
        const float t = static_cast<float>((qcvm->time - rec.start) / za::max(static_cast<double>(rec.duration), 1e-3));
        const bool knocked = ent && !ent->free && knockedDown(ent);
        if(!ent || ent->free || modelOf(ent) != rec.rig->model || ent->v.health <= 0.f || knocked || t >= 1.f)
        {
            if(!knocked && !isRagdollSlot(rec.num))
            {
                ragdoll::unpublish(rec.num);
            }
            world->recoveries.eraseAt(i);
            continue;
        }
        // Its frames lerped as the client lerps them once it is drawn animated again, by the server's times: from the
        // think that set the frame (due at the last step: the step's thinks run between its time and the next step's) to
        // the next, and to the time of the message it is drawn at. (The lerp from this step's time over the get-up's
        // interval lagged by a step: each frame's end was skipped, and in bullet time, the thinks slowed, it held still.)
        const int frame = static_cast<int>(ent->v.frame);
        const double now = qcvm->time + host_frametime;
        if(frame != rec.frame)
        {
            rec.prevFrame = rec.frame < 0 ? frame : rec.frame;
            rec.frame = frame;
            rec.changed = rec.due > qcvm->time && rec.due <= now ? rec.due : qcvm->time;
        }
        rec.due = ent->v.nextthink;
        const double interval = rec.due > rec.changed ? rec.due - rec.changed
                                                      : 0.1 / za::clamp(static_cast<double>(vr_knockdown_getup_speed.value), 0.1, 10.0);
        const float lerp = za::clamp(static_cast<float>((now - rec.changed) / interval), 0.f, 1.f);
        const int pose1 = ragdoll::poseOfFrame(rec.rig->model, rec.prevFrame), pose2 = ragdoll::poseOfFrame(rec.rig->model, rec.frame);
        const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
        const glm::vec3 origin = vec(ent->v.origin);
        const float w = smooth01(t);
        za::Array<glm::quat, ragdoll::maxBones> rot{};
        za::Array<glm::vec3, ragdoll::maxBones> pos{};
        for(int b = 0; b < rec.count; b++)
        {
            const za::SizeT k = static_cast<za::SizeT>(b);
            glm::quat r1, r2;
            glm::vec3 p1, p2;
            animatedBone(*rec.rig, pose1, b, turn, origin, rec.scale, r1, p1);
            animatedBone(*rec.rig, pose2, b, turn, origin, rec.scale, r2, p2);
            const glm::quat ar = glm::slerp(r1, r2, lerp);
            const glm::vec3 ap = glm::mix(p1, p2, lerp);
            // (The joint's place blended in a line and the turn about it: no swing about the model's origin.)
            const glm::vec3 pivot = rec.rig->bones[b].pivot * rec.scale;
            const glm::vec3 fromPivot = rec.rot[k] * pivot + rec.pos[k], toPivot = ar * pivot + ap;
            rot[k] = glm::normalize(glm::slerp(rec.rot[k], ar, w));
            pos[k] = glm::mix(fromPivot, toPivot, w) - rot[k] * pivot;
        }
        ragdoll::publish(rec.num, rec.rig, rec.count, rot.data(), pos.data(), rec.scale, qcvm->time + host_frametime, 0, 0);
        i++;
    }
}

// How well frame `frame` of `rig`, turned by `yaw` (out) about the vertical and put at `origin` (out), fits ragdoll `r`
// as it lies: its joints' places (a 2D fit of the turn, their middles matched) and the pelvis's turn. Lower is better.
[[nodiscard]] float fitFrame(const RagdollBodies& r, int frame, float& yaw, glm::vec3& origin)
{
    const ragdoll::Rig& rig = *r.rig;
    const int pose = ragdoll::poseOfFrame(rig.model, frame);
    za::Array<glm::vec3, ragdoll::maxBones> model{}, worldP{};
    int n = 0;
    glm::vec3 cm{0.f}, cw{0.f};
    for(int b = 0; b < r.count; b++)
    {
        if(rig.bones[b].joint == ragdoll::Joint::Loose || partCut(r, b))
        {
            continue;
        }
        glm::quat ar;
        glm::vec3 at;
        ragdoll::bonePose(rig, pose, b, ar, at);
        const b3WorldTransform xf = b3Body_GetTransform(r.body[static_cast<za::SizeT>(b)]);
        const glm::vec3 pivot = rig.bones[b].pivot * r.scale;
        model[static_cast<za::SizeT>(n)] = ar * pivot + at * r.scale;
        worldP[static_cast<za::SizeT>(n)] = fromB3(xf.q) * pivot + world->toU(xf.p);
        cm += model[static_cast<za::SizeT>(n)];
        cw += worldP[static_cast<za::SizeT>(n)];
        n++;
    }
    if(n == 0)
    {
        return 1e30f;
    }
    cm /= static_cast<float>(n);
    cw /= static_cast<float>(n);
    float sc = 0.f, cc = 0.f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 m = model[static_cast<za::SizeT>(i)] - cm, w = worldP[static_cast<za::SizeT>(i)] - cw;
        cc += m.x * w.x + m.y * w.y;
        sc += m.x * w.y - m.y * w.x;
    }
    yaw = za::atan2(sc, cc);
    const glm::quat turn = glm::angleAxis(yaw, glm::vec3{0.f, 0.f, 1.f});
    float err = 0.f;
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 d = turn * (model[static_cast<za::SizeT>(i)] - cm) - (worldP[static_cast<za::SizeT>(i)] - cw);
        err += glm::dot(d, d);
    }
    err /= static_cast<float>(n);
    origin = cw - turn * cm;
    // The pelvis's turn as drawn against the frame's (a body on its back or on its front).
    glm::quat pr;
    glm::vec3 pt;
    ragdoll::bonePose(rig, pose, 0, pr, pt);
    const glm::quat drawn = fromB3(b3Body_GetTransform(r.body[0]).q);
    const float c = za::clamp(za::abs(glm::dot(glm::normalize(turn * pr), drawn)), 0.f, 1.f);
    const float angle = 2.f * za::acos(c);
    constexpr float unitsPerRadian = 12.f;
    return err + (angle * unitsPerRadian) * (angle * unitsPerRadian);
}

// Where a monster of box mins..maxs can stand near `at` (its middle there), its ragdoll lying from `from` (its pelvis):
// `at`, else around it, out to `range` units in rings; each place reached from `from` without crossing a wall, on a
// floor within a step of the one under the body (never one above or below it), its box clear (of the world, monsters
// and players). False if none.
[[nodiscard]] bool standSpot(edict_t* ent, const glm::vec3& from, const glm::vec3& at, const glm::vec3& mins,
    const glm::vec3& maxs, float range, glm::vec3& out)
{
    const auto point = [](const glm::vec3& a, const glm::vec3& b, edict_t* pass) {
        vec3_t va{a.x, a.y, a.z}, vb{b.x, b.y, b.z};
        return SV_Move(va, vec3_origin, vec3_origin, vb, MOVE_NOMONSTERS, pass);
    };
    const glm::vec3 eye = from + glm::vec3{0.f, 0.f, 2.f};
    // The floor under the body.
    trace_t down = point(eye, eye - glm::vec3{0.f, 0.f, 128.f}, ent);
    const float floorZ = down.fraction < 1.f ? down.endpos[2] : from.z - 24.f;
    const float height = eye.z;
    const auto tryAt = [&](const glm::vec3& c) {
        const glm::vec3 level{c.x, c.y, height};
        if(point(eye, level, ent).fraction < 1.f)
        {
            return false; // (a wall between)
        }
        const trace_t tr = point(level, level - glm::vec3{0.f, 0.f, height - floorZ + 32.f}, ent);
        if(tr.fraction >= 1.f || tr.plane.normal[2] < 0.7f || za::abs(tr.endpos[2] - floorZ) > 24.f)
        {
            return false; // (no floor near the body's: a ledge, a drop, a step too high)
        }
        vec3_t o{c.x, c.y, tr.endpos[2] - mins.z + 0.25f};
        vec3_t lo{mins.x, mins.y, mins.z}, hi{maxs.x, maxs.y, maxs.z};
        const trace_t box = SV_Move(o, lo, hi, o, MOVE_NORMAL, ent);
        if(box.startsolid || box.allsolid)
        {
            return false;
        }
        out = glm::vec3{o[0], o[1], o[2]};
        return true;
    };
    if(tryAt(at))
    {
        return true;
    }
    constexpr float step = 8.f;
    for(float radius = step; radius <= za::max(range, 0.f) + 0.01f; radius += step)
    {
        const int dirs = za::clamp(static_cast<int>(glm::two_pi<float>() * radius / step), 8, 32);
        for(int d = 0; d < dirs; d++)
        {
            const float a = glm::two_pi<float>() * (static_cast<float>(d) + (static_cast<int>(radius / step) % 2 ? 0.5f : 0.f)) /
                            static_cast<float>(dirs);
            if(tryAt(at + glm::vec3{za::cos(a), za::sin(a), 0.f} * radius))
            {
                return true;
            }
        }
    }
    return false;
}

// A blast of `damage` at `at` (box3d::blast) on a ragdoll's parts: each thrown as a prop of the whole ragdoll's mass
// would be (thrown together, the nearer parts harder: it tumbles), if the blast sees its middle.
void blastRagdoll(const RagdollBodies& r, const glm::vec3& at, float damage)
{
    const float reach = damage + 40.f;
    constexpr float referenceMass = 6.f; // kg (blast's)
    float total = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        total += partCut(r, b) ? 0.f : b3Body_GetMass(r.body[static_cast<za::SizeT>(b)]);
    }
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(body));
        const float distance = glm::distance(centre, at);
        const float points = damage - 0.5f * distance;
        if(distance > reach || points <= 0.f)
        {
            continue;
        }
        vec3_t from, to;
        store(at, from);
        store(centre, to);
        const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, nullptr);
        if(tr.fraction < 1.f || tr.startsolid)
        {
            continue;
        }
        glm::vec3 dir = distance > 0.01f ? (centre - at) / distance : glm::vec3{0.f, 0.f, 1.f};
        dir = glm::normalize(dir + glm::vec3{0.f, 0.f, 0.35f});
        const float speed = za::max(tune(EDICT_NUM(r.num), Tune::Blast), 0.f) *
                            za::min(4.f * points * CLAMP(0.5f, za::sqrt(referenceMass / za::max(total, 1e-3f)), 2.f), 600.f);
        b3Body_ApplyLinearImpulse(body, world->toM(dir * (speed * b3Body_GetMass(body))), b3Body_GetWorldCenter(body), true);
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d part %d blasted: %.0f points, %.0f u/s\n", r.num, b, points, speed);
        }
    }
}

// The part of edict `num`'s ragdoll nearest `at` (units); a null body if it isn't one.
[[nodiscard]] b3BodyId ragdollPartNear(int num, const glm::vec3& at)
{
    if(num <= 0 || num >= static_cast<int>(world->slots.size()) || world->slots[num].ragdoll < 0)
    {
        return b3_nullBodyId;
    }
    const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
    b3BodyId best = b3_nullBodyId;
    float bestD = 1e30f;
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
        const float d = glm::distance(glm::clamp(at, lo, hi), at) + 0.01f * glm::distance((lo + hi) * 0.5f, at);
        if(d < bestD)
        {
            bestD = d;
            best = body;
        }
    }
    return best;
}

// ---- Decapitation (ROUND21.md, "Decapitation"; QC vr_decap.qc) ----
// A slash at the neck cuts the head off: the rig's head bone and the bones on it (ragdoll::headBones: a rottweiler's jaw)
// lose their bodies (Box3D takes their joints with them) and their vertices are drawn at the neck (vr_ragdoll.cpp
// neckOf). The head flies off as a gib of its own (QC: ragdollCut): from where the head part was, turned as it was, at
// its velocity plus vr_decap_head_speed of the blade's and vr_decap_head_lift up, spinning as the blade's push at its
// base turns it (the share of the blade's speed across the neck over the head's lever from the neck).

constexpr float headSpinMost = 12.f; // rad/s: a cut head's spin at most (about two turns a second)
constexpr float neckReach = 4.f;     // units (times its scale): a hit this near the neck is on the head (ragdollHeadAt)

void endGrab(int index); // (below: "Holding a ragdoll's limb")

// Cuts the limb at joint `bone` off `r` (Limb gore: the bone and every bone on it not cut yet; the head: its bones,
// ragdoll::headBones); `launch`: the piece launched by `blade` (units/s), else (a saved game's) only cut. False if it
// isn't a limb of its rig or it is cut already. The piece as it was cut (ragdollCut 0-3): its middle (the head's: its
// own bone's vertices'; a limb's: ragdoll::limbMiddle of what it takes, its model's origin), turned as `bone` was.
bool cutLimb(RagdollBodies& r, edict_t* ent, int bone, const glm::vec3& blade, bool launch)
{
    const ragdoll::Rig& rig = *r.rig;
    const bool isHead = bone == rig.head && bone >= 0;
    if(!isHead && !ragdoll::limbJoint(rig, bone))
    {
        return false;
    }
    const uint32_t bones = (isHead ? ragdoll::headBones(rig) : ragdoll::limbBones(rig, bone)) & ~r.cut;
    const int head = bone;
    if(!bones || head >= r.count || (r.cut & (1u << head)))
    {
        return false;
    }
    const int parent = ragdoll::uncutParent(rig, head, r.cut);
    if(parent < 0)
    {
        return false;
    }
    // The piece as it is (its middle), and the joint (its pivot on the part it is cut from).
    const b3BodyId hb = r.body[static_cast<za::SizeT>(head)];
    const b3WorldTransform hx = b3Body_GetTransform(hb);
    const glm::vec3 mid = ragdoll::limbMiddle(rig, isHead ? (1u << head) : bones);
    r.headRot = fromB3(hx.q);
    r.headMid = r.headRot * (mid * r.scale) + world->toU(hx.p);
    r.lastCut = bone;
    r.lastCutBones = bones;
    const b3WorldTransform px = b3Body_GetTransform(r.body[static_cast<za::SizeT>(parent)]);
    const glm::vec3 neck = fromB3(px.q) * (rig.bones[head].pivot * r.scale) + world->toU(px.p);
    r.headVel = world->toU(b3Body_GetLinearVelocity(hb));
    r.headSpin = glmv(b3Body_GetAngularVelocity(hb));
    if(launch)
    {
        const float share = za::max(vr_decap_head_speed.value, 0.f);
        r.headVel += blade * share + glm::vec3{0.f, 0.f, za::max(vr_decap_head_lift.value, 0.f) * (isHead ? 1.f : 0.5f)};
        const float lever = glm::distance(r.headMid, neck);
        const glm::vec3 n = lever > 0.1f ? (r.headMid - neck) / lever : glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 across = blade - n * glm::dot(blade, n);
        if(glm::length(across) > 1.f)
        {
            // (Pushed at its base, across the neck: it turns about base x push.)
            r.headSpin += glm::normalize(glm::cross(n, across)) *
                          za::min(glm::length(across) * share / za::max(lever, 2.f), headSpinMost);
        }
    }
    // The hands holding (or pulling) it let go: their joints go with its body.
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.num == r.num && g.part >= 0 && g.part < ragdoll::maxBones && (bones & (1u << g.part)))
        {
            endGrab(i);
        }
    }
    for(int b = 0; b < r.count; b++)
    {
        if(bones & (1u << b))
        {
            b3DestroyBody(r.body[static_cast<za::SizeT>(b)]);
        }
    }
    r.cut |= bones;
    for(int b = 0; b < r.count; b++)
    {
        if(r.cut & (1u << b)) // (every cut bone: one cut before may have named a part cut now)
        {
            r.body[static_cast<za::SizeT>(b)] = r.body[static_cast<za::SizeT>(ragdoll::uncutParent(rig, b, r.cut))];
        }
        else
        {
            b3Body_SetAwake(r.body[static_cast<za::SizeT>(b)], true);
        }
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d %s %s cut off%s: at %.1f %.1f %.1f, %.0f u/s, spin %.1f rad/s; %d parts left\n", r.num,
            PR_GetString(ent->v.classname), rig.bones[bone].name, launch ? "" : " (made so again)", r.headMid.x, r.headMid.y,
            r.headMid.z, glm::length(r.headVel), glm::length(r.headSpin), partsLeft(r));
    }
    if(fields().vr_limbcut >= 0)
    {
        fieldFloat(ent, fields().vr_limbcut) = static_cast<float>(r.cut);
    }
    return true;
}

// Cuts `r`'s head off (cutLimb at its rig's head).
bool cutHead(RagdollBodies& r, edict_t* ent, const glm::vec3& blade, bool launch)
{
    return r.rig->head >= 0 && cutLimb(r, ent, r.rig->head, blade, launch);
}

// ---- Holding a ragdoll's limb (vr_ragdoll_grab) ----
// A hand gripping on a limb (QC's VR_Ragdoll_Handtouch: ragdollgrab) holds it by a kinematic body at the hand and a
// motor joint to the limb: its spring (at most vr_ragdoll_grab_force) keeps the limb as it was in the hand, so the limb
// follows the hand through the joint chain and the rest of the body hangs from it, swings and is dragged; two hands, two
// joints. Let go of (ragdollrelease), the limb keeps the hand's throw (vr_ragdoll_throw). A force grab's pull
// (ragdollpull) flies the limb nearest the hand's aim to the hand, homing as a pulled prop does, the body dragged after
// it; caught (the grip held as it arrives: ragdollgrab again), it is held as above.

constexpr float grabHertz = 8.f;        // the hold's spring (linear; its turn half that)
constexpr float grabLever = 0.04f;      // m: its most torque, the most force at this lever
constexpr float grabStuck = 40.f;       // units: a limb this far from its hand
constexpr double grabStuckTime = 0.5;   // s: this long is let go of (behind a wall, a door shut on it)
constexpr double pullLate = 0.4;        // s: a pull not caught this long after it was due drops
constexpr float pullMostSpeed = 1500.f; // units/s

[[nodiscard]] int grabIndex(int player, int hand)
{
    for(int i = 0; i < static_cast<int>(world->ragdollGrabs.size()); i++)
    {
        const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.player == player && g.hand == hand)
        {
            return i;
        }
    }
    return -1;
}

void endGrab(int index)
{
    World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    if(B3_IS_NON_NULL(g.joint) && b3Joint_IsValid(g.joint))
    {
        b3DestroyJoint(g.joint, true);
    }
    if(B3_IS_NON_NULL(g.anchor) && b3Body_IsValid(g.anchor))
    {
        b3DestroyBody(g.anchor);
    }
    world->ragdollGrabs.erase(world->ragdollGrabs.begin() + index);
}

void dropGrabsOf(int num)
{
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        if(world->ragdollGrabs[static_cast<za::SizeT>(i)].num == num)
        {
            endGrab(i);
        }
    }
}

// The ragdoll of edict `num`, or null.
[[nodiscard]] RagdollBodies* ragdollOf(int num)
{
    if(!world || world->generation != worldGeneration() || num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) || world->slots[num].ragdoll < 0)
    {
        return nullptr;
    }
    return &world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
}

// Hand `hand` (QC's: 1 the main) of `player`: its point and its turn (world units).
void handPose(edict_t* player, int hand, glm::vec3& at, glm::quat& turn)
{
    const FieldOffsets& f = fields();
    at = fieldVec(player, hand ? f.handpos : f.offhandpos);
    const glm::vec3 angles = fieldVec(player, hand ? f.handrot : f.offhandrot);
    turn = glm::normalize(glm::quat_cast(held::axesFromAngles(&angles[0], true)));
}

// The part of ragdoll `r` whose surface is nearest `at` (units), and how far (units); -1 if none.
[[nodiscard]] int nearestPart(const RagdollBodies& r, const glm::vec3& at, float& distance)
{
    int best = -1;
    distance = 1e30f;
    for(int b = 0; b < r.count; b++)
    {
        b3Vec3 closest;
        const float d = b3Body_GetClosestPoint(r.body[static_cast<za::SizeT>(b)], &closest, world->toM(at)) * world->m2u;
        if(d < distance)
        {
            distance = d;
            best = b;
        }
    }
    return best;
}

// The hand moved along `n` (world) from `at`: the first `t` (units, within `reach`) where `points` (in the hand's frame,
// turned by `axes`; w their radius) go from clear of `part` to touching it, or, from in it, the first where they are all
// clear of it (`from`: g(0), how far the nearest is from the limb). False if there is none.
[[nodiscard]] bool touchAlong(b3BodyId part, const glm::vec3& at, const glm::mat3& axes, const glm::vec3& n,
    const za::Vector<glm::vec4>& points, float reach, float& t, float& from)
{
    // The points moved `s` units along the normal: how far the nearest is from the limb (negative: in it, as deep as its
    // radius at most: the part's closest point to a point in it is the point itself).
    const auto gap = [&](float s) {
        float g = 1e30f;
        for(const glm::vec4& p : points)
        {
            b3Vec3 closest;
            const glm::vec3 c = at + axes * glm::vec3{p} + n * s;
            g = za::min(g, b3Body_GetClosestPoint(part, &closest, world->toM(c)) * world->m2u - p.w);
        }
        return g;
    };
    constexpr float step = 0.25f; // units
    from = gap(0.f);
    const bool clear = from > 0.f;
    const float dir = clear ? 1.f : -1.f; // towards the limb from a gap, back out of it from in it
    float lo = 0.f, hi = 0.f;
    bool found = false;
    for(float s = step; s <= reach + step * 0.5f && !found; s += step)
    {
        if((gap(dir * s) > 0.f) != clear)
        {
            lo = s - step;
            hi = s;
            found = true;
        }
    }
    if(!found)
    {
        return false;
    }
    for(int i = 0; i < 12; i++) // (to a few thousandths of a unit)
    {
        const float mid = (lo + hi) * 0.5f;
        ((gap(dir * mid) > 0.f) == clear ? lo : hi) = mid;
    }
    t = dir * (clear ? lo : hi); // (touching: the side of the crossing clear of it)
    return true;
}

// The hand on the limb (vr_ragdoll_grab_fit; as a carried prop against the fingers: vr_held_surface_fit; ROUND21.md,
// "Hands on held ragdolls"). The limb taken where the hand's middle was held that point at the hand, a few units short
// of the limb (the hand reaches vr_ragdoll_grab_reach) or sunk into it (a caught limb, by its middle): the hand was drawn
// off the limb, or in it, its fingers closed on nothing. Instead the hand is moved along the way its palm faces
// (grip::handFrame) until its palm's skin (a patch of it round its middle) rests on the limb, or, if the limb is beside
// the palm, until the closed hand (held::fist) touches it: towards it from a gap, back out of it from in it, less
// vr_held_fit_gap; the point held at the hand is that many units on, so the limb is held on the palm and the fingers
// close round it (vr_view.cpp). Unchanged if no move within the reach (and a little more) does it (the back of the hand
// on it). `held`: the limb's point the hand would hold without it (world); the world point to hold at the hand, and how
// far the hand moved along its palm's normal (`moved`).
[[nodiscard]] glm::vec3 handFit(int pnum, int hand, const glm::vec3& at, const glm::quat& turn, b3BodyId part,
    const glm::vec3& held, float* moved = nullptr)
{
    if(moved)
    {
        *moved = 0.f;
    }
    const za::Vector<glm::vec4>& fist = held::fist(hand);
    if(vr_ragdoll_grab_fit.value <= 0.f || hand < 0 || hand > 1 || pnum != 1 || cls.state == ca_dedicated || fist.empty())
    {
        return held; // (the hand's frame and fist are the local player's: measured by the client)
    }
    const grip::HandFrame f = grip::handFrame(hand, hand == 0, true);
    const glm::vec3 nLocal = glm::normalize(f.palmNormal);
    const glm::vec3 n = glm::normalize(turn * nLocal);
    const glm::mat3 axes = glm::mat3_cast(turn);
    const float reach = za::max(vr_ragdoll_grab_reach.value, 0.f) + 4.f;
    // The palm's skin: its middle and a patch round it, 4 cm across the palm either way (along the grip channel) and
    // 4.5 along it (to the fingers, to the wrist).
    const float cm = 0.01f * units::metresToUnits();
    const glm::vec3 across = glm::normalize(f.channelDir - nLocal * glm::dot(f.channelDir, nLocal));
    const glm::vec3 along = glm::cross(nLocal, across);
    za::Vector<glm::vec4>& palm = scratch.palmPatch;
    palm.clear();
    for(int i = -2; i <= 2; i++)
    {
        for(int j = -2; j <= 2; j++)
        {
            const glm::vec3 p = f.palm + across * (2.f * cm * static_cast<float>(i)) + along * (2.25f * cm * static_cast<float>(j));
            palm.pushBack(glm::vec4{p, 0.05f});
        }
    }
    float t = 0.f, from = 0.f;
    const char* by = "the palm";
    if(!touchAlong(part, at, axes, n, palm, reach, t, from))
    {
        by = "the fist";
        if(!touchAlong(part, at, axes, n, fist, reach, t, from))
        {
            by = nullptr;
        }
    }
    if(vr_debug_ragdoll.value >= 2.f)
    {
        Con_Printf("ragdoll: hand fit: the palm's normal %.2f %.2f %.2f; %s\n", n.x, n.y, n.z,
            by ? va("%s %.1f units %s the limb, moved %.1f along it", by, za::fabs(from), from > 0.f ? "from" : "into", t)
               : "no move within the reach rests it on the limb");
    }
    if(!by)
    {
        return held;
    }
    t -= vr_held_fit_gap.value * cm;
    if(moved)
    {
        *moved = t;
    }
    return at + n * t;
}

// Grab `g`'s hold: its body at the hand, its joint to the limb holding the limb's point `grip` (its body's space, m) at
// the hand, the limb turned in the hand as it is now; `grip` moved for the limb to rest on the palm (handFit).
void startHold(World::RagdollGrab& g, edict_t* player, const RagdollBodies& r, b3Vec3 grip)
{
    glm::vec3 at;
    glm::quat turn;
    handPose(player, g.hand, at, turn);
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(g.part)];
        float moved = 0.f;
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(body, grip));
        grip = b3Body_GetLocalPoint(body, world->toM(handFit(g.player, g.hand, at, turn, body, held, &moved)));
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d part %d held %.1f units from the hand's middle, moved %.1f units along the palm's normal "
                       "(vr_ragdoll_grab_fit)\n",
                g.num, g.part, glm::distance(held, at), moved);
        }
    }
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = world->toM(at);
    def.rotation = toB3(turn);
    def.userData = userOf(g.player);
    g.anchor = b3CreateBody(world->id, &def);
    const b3BodyId part = r.body[static_cast<za::SizeT>(g.part)];
    const glm::quat partTurn = fromB3(b3Body_GetRotation(part));
    const float force = za::max(vr_ragdoll_grab_force.value, 0.f);
    b3MotorJointDef jd = b3DefaultMotorJointDef();
    jd.base.bodyIdA = g.anchor;
    jd.base.bodyIdB = part;
    jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
    jd.base.localFrameB = b3Transform{grip, toB3(glm::normalize(glm::inverse(partTurn) * turn))};
    jd.base.collideConnected = false;
    jd.linearVelocity = b3Vec3_zero;
    jd.angularVelocity = b3Vec3_zero;
    jd.maxVelocityForce = 0.f;
    jd.maxVelocityTorque = 0.f;
    jd.linearHertz = grabHertz;
    jd.linearDampingRatio = 1.f;
    jd.maxSpringForce = force;
    jd.angularHertz = grabHertz * 0.5f;
    jd.angularDampingRatio = 1.f;
    jd.maxSpringTorque = force * grabLever;
    g.joint = b3CreateMotorJoint(world->id, &jd);
    g.pulling = false;
    g.farSince = -1.0;
    for(int b = 0; b < r.count; b++)
    {
        b3Body_SetAwake(r.body[static_cast<za::SizeT>(b)], true);
    }
}

// ragdollgrab: hand `hand` of `player` takes the limb of `corpse`'s ragdoll it is on (within vr_ragdoll_grab_reach of
// its surface), or catches the limb its force grab pulls (QC tested its reach: vr_forcegrab_catch_radius).
bool grabRagdoll(edict_t* corpse, edict_t* player, int hand)
{
    const int pnum = NUM_FOR_EDICT(player);
    if(!world || vr_ragdoll_grab.value < 1.f || pnum < 1 || pnum > svs.maxclients)
    {
        return false;
    }
    const int existing = grabIndex(pnum, hand);
    if(existing >= 0)
    {
        World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(existing)];
        const RagdollBodies* r = ragdollOf(g.num);
        if(!g.pulling || !r || g.part >= r->count)
        {
            return false; // (holding already)
        }
        // Caught: its flight stops in the hand (as a pulled prop's: VR_Forcegrab_Catch), the rest of him swinging on.
        for(int b = 0; b < r->count; b++)
        {
            const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b == g.part ? b3Vec3_zero : b3v(glmv(b3Body_GetLinearVelocity(body)) * 0.25f));
        }
        startHold(g, player, *r, b3v(g.grip));
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d part %d caught by hand %d\n", g.num, g.part, hand);
        }
        return true;
    }
    const int num = NUM_FOR_EDICT(corpse);
    const RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        return false;
    }
    glm::vec3 at;
    glm::quat turn;
    handPose(player, hand, at, turn);
    float distance = 0.f;
    const int part = nearestPart(*r, at, distance);
    if(part < 0 || distance > za::max(vr_ragdoll_grab_reach.value, 0.f))
    {
        return false;
    }
    World::RagdollGrab g;
    g.player = pnum;
    g.hand = hand;
    g.num = num;
    g.part = part;
    startHold(g, player, *r, b3Body_GetLocalPoint(r->body[static_cast<za::SizeT>(part)], world->toM(at)));
    world->ragdollGrabs.pushBack(g);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) taken by hand %d, %.1f units from its surface\n", num, part, r->rig->bones[part].name,
            hand, distance);
    }
    return true;
}

// ragdollpull: hand `hand` of `player` force grabs `corpse`'s ragdoll by the limb nearest its aim; it is due at the hand
// in `flight` seconds.
bool pullRagdoll(edict_t* corpse, edict_t* player, int hand, float flight)
{
    const int pnum = NUM_FOR_EDICT(player), num = NUM_FOR_EDICT(corpse);
    const RagdollBodies* r = ragdollOf(num);
    if(!r || vr_ragdoll_grab.value < 2.f || pnum < 1 || pnum > svs.maxclients || grabIndex(pnum, hand) >= 0)
    {
        return false;
    }
    glm::vec3 at;
    glm::quat turn;
    handPose(player, hand, at, turn);
    const glm::vec3 aim = turn * glm::vec3{1.f, 0.f, 0.f};
    int best = -1;
    float bestD = 1e30f;
    for(int b = 0; b < r->count; b++)
    {
        if(r->rig->bones[b].joint == ragdoll::Joint::Loose)
        {
            continue; // (his shotgun lying apart)
        }
        const glm::vec3 c = world->toU(b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(b)]));
        const float along = za::max(glm::dot(c - at, aim), 0.f);
        const float d = glm::distance(c, at + aim * along);
        if(d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    if(best < 0)
    {
        return false;
    }
    World::RagdollGrab g;
    g.player = pnum;
    g.hand = hand;
    g.num = num;
    g.part = best;
    g.pulling = true;
    g.arrive = qcvm->time + za::max(flight, 0.05f);
    g.grip = glmv(b3Body_GetLocalCenter(r->body[static_cast<za::SizeT>(best)]));
    world->ragdollGrabs.pushBack(g);
    for(int b = 0; b < r->count; b++)
    {
        b3Body_SetAwake(r->body[static_cast<za::SizeT>(b)], true);
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) pulled by hand %d, due in %.2f s\n", num, best, r->rig->bones[best].name, hand, flight);
    }
    return true;
}

// ragdollrelease: hand `hand` of `player` lets go: a held limb keeps `velocity` (units/s: the hand's throw, times
// vr_ragdoll_throw) where it is faster than it; a pull ends (the body falls, a little of its speed kept).
void releaseRagdoll(edict_t* player, int hand, const glm::vec3& velocity)
{
    const int index = grabIndex(NUM_FOR_EDICT(player), hand);
    if(index < 0)
    {
        return;
    }
    const World::RagdollGrab g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    endGrab(index);
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count)
    {
        return;
    }
    const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
    if(g.pulling)
    {
        for(int b = 0; b < r->count; b++)
        {
            const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) * 0.3f));
        }
        return;
    }
    const glm::vec3 v = glmv(world->toM(velocity * za::max(vr_ragdoll_throw.value, 0.f)));
    const glm::vec3 now = glmv(b3Body_GetLinearVelocity(part));
    const float speed = glm::length(v);
    if(speed > 0.01f)
    {
        // Its speed along the throw at least the throw's (what the pull already gave it is kept across it).
        const glm::vec3 dir = v / speed;
        const float along = glm::dot(now, dir);
        if(along < speed)
        {
            b3Body_SetLinearVelocity(part, b3v(now + dir * (speed - along)));
        }
    }
    b3Body_SetAwake(part, true);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d let go of by hand %d at %.0f u/s (the throw %.0f)\n", g.num, g.part, hand,
            glm::length(glmv(b3Body_GetLinearVelocity(part))) * world->m2u, speed * world->m2u);
    }
}

// Before the step: each hold's body to its hand (a hold whose limb stays far from the hand is let go of: QC sees it
// gone, ragdollheld), each pull's limb homing on its hand; those of what is gone ended.
void syncRagdollGrabs(float dt)
{
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        edict_t* player = g.player < qcvm->num_edicts ? EDICT_NUM(g.player) : nullptr;
        const RagdollBodies* r = ragdollOf(g.num);
        if(!player || player->free || player->v.health <= 0.f || !svs.clients[g.player - 1].active || !r || g.part >= r->count ||
           vr_ragdoll_grab.value < 1.f)
        {
            endGrab(i);
            continue;
        }
        // (A hold the QC no longer has: its hand holds something else, or nothing.)
        const int heldOfs = g.hand ? fields().mainhand_held : fields().offhand_held;
        if(!g.pulling && heldOfs >= 0 && fieldInt(player, heldOfs) != EDICT_TO_PROG(EDICT_NUM(g.num)))
        {
            endGrab(i);
            continue;
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, g.hand, at, turn);
        const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
        if(g.pulling)
        {
            const double remaining = g.arrive - qcvm->time;
            if(remaining < -pullLate)
            {
                endGrab(i); // (missed: it lies where it fell)
                continue;
            }
            if(remaining > 0.0)
            {
                const glm::vec3 grip = world->toU(b3Body_GetWorldPoint(part, b3v(g.grip)));
                glm::vec3 v = (at - grip) / static_cast<float>(za::max(remaining, static_cast<double>(dt)));
                if(glm::length(v) > pullMostSpeed)
                {
                    v = glm::normalize(v) * pullMostSpeed;
                }
                // The limb flies to the hand; the rest of him after it, a little looser (it flops).
                const glm::vec3 want = glmv(world->toM(v));
                for(int b = 0; b < r->count; b++)
                {
                    const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
                    const glm::vec3 own = glmv(b3Body_GetLinearVelocity(body));
                    b3Body_SetLinearVelocity(body, b3v(b == g.part ? want : own + (want - own) * 0.8f));
                    b3Body_SetAwake(body, true);
                }
            }
            continue;
        }
        if(B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor) || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
        {
            endGrab(i);
            continue;
        }
        const float force = za::max(vr_ragdoll_grab_force.value, 0.f);
        b3MotorJoint_SetMaxSpringForce(g.joint, force);
        b3MotorJoint_SetMaxSpringTorque(g.joint, force * grabLever);
        const glm::vec3 from = world->toU(b3Body_GetPosition(g.anchor));
        if(glm::distance(from, at) > 0.5f * world->m2u)
        {
            b3Body_SetTransform(g.anchor, world->toM(at), toB3(turn)); // (a jump: a teleport, the hand taken back)
            b3Body_SetLinearVelocity(g.anchor, b3Vec3_zero);
        }
        else
        {
            b3Body_SetTargetTransform(g.anchor, b3WorldTransform{world->toM(at), toB3(turn)}, dt, true);
        }
        const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(part, frameB.p));
        if(glm::distance(held, at) > grabStuck)
        {
            if(g.farSince < 0.0)
            {
                g.farSince = qcvm->time;
            }
            else if(qcvm->time - g.farSince > grabStuckTime)
            {
                if(vr_debug_ragdoll.value)
                {
                    Con_Printf("ragdoll: %d part %d left behind by hand %d (%.0f units)\n", g.num, g.part, g.hand, glm::distance(held, at));
                }
                endGrab(i);
                continue;
            }
        }
        else
        {
            g.farSince = -1.0;
        }
        b3Body_SetAwake(part, true);
    }
}

// The distance (units) from hand `hand` of `player` to the point of the limb it holds or pulls; -1 if none.
[[nodiscard]] float grabReach(int player, int hand)
{
    const int index = grabIndex(player, hand);
    if(index < 0)
    {
        return -1.f;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count)
    {
        return -1.f;
    }
    glm::vec3 at;
    glm::quat turn;
    handPose(EDICT_NUM(player), hand, at, turn);
    const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
    const b3Vec3 local = g.pulling || B3_IS_NULL(g.joint) ? b3v(g.grip) : b3Joint_GetLocalFrameB(g.joint).p;
    return glm::distance(world->toU(b3Body_GetWorldPoint(part, local)), at);
}

// ---- Points on a ragdoll's limbs (flames: vr_burning.qc's ragdollbone, ragdollpoint) ----

// The part of edict `num`'s ragdoll whose surface is nearest `at` (units); -1 if it isn't one.
[[nodiscard]] int ragdollBoneNear(int num, const glm::vec3& at)
{
    const RagdollBodies* r = ragdollOf(num);
    float distance = 0.f;
    return r ? nearestPart(*r, at, distance) : -1;
}

// A point of part `bone` of edict `num`'s ragdoll: from the world into the part's space (units), or back (toWorld). The
// point itself if it isn't one.
[[nodiscard]] glm::vec3 box3dRagdollPoint(int num, int bone, const glm::vec3& p, bool toWorld)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r || bone < 0 || bone >= r->count)
    {
        return p;
    }
    const b3BodyId body = r->body[static_cast<za::SizeT>(bone)];
    if(toWorld)
    {
        return world->toU(b3Body_GetWorldPoint(body, b3v(p / world->m2u)));
    }
    return glmv(b3Body_GetLocalPoint(body, world->toM(p))) * world->m2u;
}

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
unsigned shapeGeneration = 1;

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
    case Kind::Mover: return s.model != modelOf(ent);
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

void writeProp(edict_t* ent, Slot& s);

// The yaw a spinning pickup is drawn at (cl_main.c's bobjrotate for EF_ROTATE models: 100 degrees a second), by the
// server's clock (the client's trails it by at most a frame).
[[nodiscard]] float spinYaw()
{
    return anglemod(static_cast<float>(100.0 * sv.qcvm.time));
}

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

// How far the prop's shapes (their corners, where its body is) are sunk into a floor: the deepest of its lower half's
// corners inside a solid, measured to the upward-facing surface above it, if that surface is below the prop's middle
// (not a table top it lies under). 0 if none is.
[[nodiscard]] float floorDepth(edict_t* ent, b3BodyId body)
{
    const b3WorldTransform xf = b3Body_GetTransform(body);
    b3ShapeId shapes[4];
    const int n = b3Body_GetShapes(body, shapes, 4);
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
    s.flightLogged = false;
    s.model = model;
    s.hull = kind == Kind::Actor && model && model->type == mod_alias ? actorHull(ent, model) : nullptr;
    s.frame = static_cast<int>(ent->v.frame);
    s.scale = scaleFields(ent);
    s.brush = model && model->type == mod_brush;
    s.spins = kind == Kind::Fixture && model &&
              ((model->flags & EF_ROTATE) || fieldFloatOr(ent, fields().vr_pickup_spin, 0.f) != 0.f); // (a weapon pickup drawn as its prop)
    s.massSetting = massSetting(ent, model);
    s.massScale = props::massScale(model);
    s.soft = model && isSoft(ent, model);
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
    const glm::vec3 origin = vec(ent->v.origin), angles = vec(ent->v.angles);
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

// ---------------------------------------------------------------------------------------------------------------------
// Throws (vr_box3d_throw_grace; ROUND21.md, "Throws leave the hand clean"): a thing thrown (a weapon, a prop let go of)
// left the hand's bodies (the open hand or fist it becomes, the fists' spheres) touching or sunk in them, and the hand,
// still moving (a wrist snap turning it, the follow-through catching up), knocked it as it left: the wrong way, the
// wrong spin. Thrown faster than vr_box3d_throw_grace_speed, it passes through both of its thrower's hands' bodies for
// vr_box3d_throw_grace seconds, and through its thrower's body (vr_box3d_throw_grace_body: an overhead wrist snap threw
// a box down into the player's capsule). A slower release (the hand opened, held still) keeps meeting them: it rests on the palm.

// How fast hand `h` of `player` throws a thing reaching `reach` m from its middle (m/s): the hand's throw estimate (as
// QC throws with: VRGetHandThrowVel), or the thing's tip turned by the hand's spin (a wrist snap), whichever is faster.
[[nodiscard]] float throwSpeed(edict_t* player, int h, float reach)
{
    const FieldOffsets& f = fields();
    const int velOfs = h ? f.handthrowvel : f.offhandthrowvel;
    const int spinOfs = h ? f.handavel : f.offhandavel;
    const float linear = velOfs >= 0 ? glm::length(fieldVec(player, velOfs)) : 0.f;
    const float turn = spinOfs >= 0 ? glm::length(fieldVec(player, spinOfs)) * reach : 0.f;
    return za::max(linear, turn);
}

// Whether prop `num` passes through the hands' bodies of client `player` now (a throw's grace).
[[nodiscard]] bool graced(int player, int num)
{
    for(World::Grace& g : world->graces)
    {
        if(g.num == num && g.player == player && g.skips && qcvm->time < g.until)
        {
            g.skipped.n.fetchAddRelaxed(1);
            return true;
        }
    }
    return false;
}

bool isSmallGib(edict_t* ent)
{
    const int f = fields().vr_prop_mass;
    return f >= 0 && fieldFloat(ent, f) > 0.f;
}

float monsterDropAge(edict_t* ent)
{
    const int f = fields().vr_monster_drop;
    const float made = f >= 0 ? fieldFloat(ent, f) : 0.f;
    return made > 0.f ? static_cast<float>(qcvm->time - made) : -1.f;
}

// A small gib's age (s since it was made: its QC .vr_sgib), -1 for anything else.
[[nodiscard]] float smallGibAge(edict_t* ent)
{
    const int f = fields().vr_sgib;
    const float made = f >= 0 ? fieldFloat(ent, f) : 0.f;
    return made > 0.f ? static_cast<float>(qcvm->time - made) : -1.f;
}

// Whether entity `num` is a small gib just torn out (vr_smallgibs_blow_grace): the hands' and weapons' reach bodies and
// held things pass through it (reachSkips, shouldCollide), as the QC's blows, nudges and batting do
// (VR_SmallGib_BlowSpared): the blade that tore it out went on through it and batted it away.
[[nodiscard]] bool smallGibSpared(int num)
{
    if(num <= svs.maxclients || num >= qcvm->num_edicts)
    {
        return false;
    }
    const float age = smallGibAge(EDICT_NUM(num));
    return age >= 0.f && age < vr_smallgibs_blow_grace.value;
}

// vr_smallgibs_trace (tests; ROUND21.md, "Small gibs batted by the blade"): after each step, the bodies touching each small
// gib in its first second that aren't the world's or other props': a hand's or a weapon's reach body, a fist, a held
// thing, a monster, a player.
void traceGibContacts()
{
    if(!vr_smallgibs_trace.value)
    {
        return;
    }
    const int idField = fields().vr_sgib_id;
    za::Array<b3ContactData, 16> contacts;
    for(int num = svs.maxclients + 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Prop || B3_IS_NULL(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        const float age = smallGibAge(ent);
        if(age < 0.f || age > 1.f)
        {
            continue;
        }
        const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        for(int i = 0; i < count; i++)
        {
            const b3ContactData& c = contacts[i];
            const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), s.body);
            const b3ShapeId other = isA ? c.shapeIdB : c.shapeIdA;
            bool touching = false;
            for(int m = 0; m < c.manifoldCount; m++)
            {
                touching = touching || c.manifolds[m].pointCount > 0;
            }
            const uint64_t cat = b3Shape_GetFilter(other).categoryBits;
            const char* what = (cat & catReach) ? "reach body" : (cat & catHand) ? "fist" : (cat & catHeld) ? "held thing"
                             : (cat & catActor) ? "monster" : (cat & catPlayer) ? "player" : nullptr;
            if(!touching || !what)
            {
                continue;
            }
            Con_Printf("sgibtrace: #%.0f %.3f s: Box3D contact with %s %d (%s), %.0f u/s\n",
                idField >= 0 ? fieldFloat(ent, idField) : 0.f, age, what, numOf(other),
                PR_GetString(EDICT_NUM(za::max(0, numOf(other)))->v.classname),
                glm::length(glmv(b3Body_GetLinearVelocity(s.body))) * world->m2u);
        }
    }
}

[[nodiscard]] bool boxesApart(const b3AABB& a, const b3AABB& b)
{
    return a.upperBound.x < b.lowerBound.x || b.upperBound.x < a.lowerBound.x || a.upperBound.y < b.lowerBound.y ||
           b.upperBound.y < a.lowerBound.y || a.upperBound.z < b.lowerBound.z || b.upperBound.z < a.lowerBound.z;
}

// A small gib's body, just made (at the end of its grace, vr_smallgibs_grace: a rigid body from then), inside a
// monster's or a player's body, or a hand's or held weapon's reach body (torn out where the blade or the chainsaw's
// chain struck): it passes through each until clear of it, as a reach body passes through what was inside it as it was
// made (ignoreInside). Box3D pushed it out at 2 to 3 m/s, many times what Melee and Chainsaw Speed gave it (NOTES.md
// vrfiringrange_2026-10-02_19-11-19 and 19-18-12; ROUND21.md, "Small gibs pushed out of the body they came from").
// A body its shape is sunk in (deeper than 1 cm, not one it is only touching: a gib made just outside a monster
// meets it, Pass Through the Body 0); a reach body its box overlaps, a little grown (a blade a step away still passes).
// A monster's drop (a weapon, a backpack) made inside the player standing on it, or the monster, likewise
// (vr_prop_drop_pass_inside): pushed out, it flew into him at 280 u/s. `corpsesOnly`: any other prop just made (dropped
// through a corpse to the floor under it, let go of in one) passes through the corpses it is in (vr_corpse_collide).
void noteBornInside(int num, const Slot& s, bool corpsesOnly = false)
{
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catProp;
    filter.maskBits = corpsesOnly ? catCorpse : catActor | catPlayer | catCorpse;
    struct Context
    {
        int num;
        double born;
    } context{num, s.born};
    constexpr float sink = 0.01f; // m (reachSink's)
    za::Array<b3ShapeId, 4> shapes;
    const int count = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(s.body);
    za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int k = 0; k < count; k++)
    {
        if(b3Shape_GetType(shapes[k]) != b3_hullShape)
        {
            continue;
        }
        const b3HullData* hull = b3Shape_GetHull(shapes[k]);
        const b3Vec3* p = b3GetHullPoints(hull);
        const b3Vec3 c = hull->center;
        const int n = za::min(hull->vertexCount, static_cast<int>(points.size()));
        for(int v = 0; v < n; v++)
        {
            b3Vec3 q = p[v]; // (each corner in by the sink along each axis, not past the middle: as ignoreInside)
            q.x = q.x > c.x ? za::max(c.x, q.x - sink) : za::min(c.x, q.x + sink);
            q.y = q.y > c.y ? za::max(c.y, q.y - sink) : za::min(c.y, q.y + sink);
            q.z = q.z > c.z ? za::max(c.z, q.z - sink) : za::min(c.z, q.z + sink);
            points[static_cast<size_t>(v)] = b3RotateVector(xf.q, q);
        }
        const b3ShapeProxy proxy{points.data(), n, 0.f};
        b3World_OverlapShape(world->id, xf.p, &proxy, filter,
            [](b3ShapeId shape, void* raw) {
                const auto& c = *static_cast<const Context*>(raw);
                const int other = numOf(shape);
                auto& in = world->inside;
                if(other > 0 && other != c.num &&
                    !za::anyOf(in.begin(), in.end(), [&](const World::Inside& i) { return i.prop == c.num && i.other == other; }))
                {
                    in.pushBack({c.num, other, c.born});
                }
                return true;
            },
            &context);
    }
    constexpr float margin = 0.05f; // m
    if(corpsesOnly)
    {
        if(vr_debug_box3d.value &&
            za::anyOf(world->inside.begin(), world->inside.end(), [num](const World::Inside& i) { return i.prop == num; }))
        {
            Con_Printf("box3d: %d %s made inside a corpse: passes through until clear\n", num, PR_GetString(EDICT_NUM(num)->v.classname));
        }
        return;
    }
    b3AABB box = b3Body_ComputeAABB(s.body);
    box.lowerBound = b3v(glmv(box.lowerBound) - glm::vec3{margin});
    box.upperBound = b3v(glmv(box.upperBound) + glm::vec3{margin});
    int reaches = 0;
    for(size_t i = 1; i < world->hands.size() && i <= static_cast<size_t>(svs.maxclients); i++)
    {
        for(World::HandBody& hb : world->hands[i])
        {
            if(!B3_IS_NULL(hb.reach) && !boxesApart(box, b3Body_ComputeAABB(hb.reach)) &&
                za::find(hb.ignore.begin(), hb.ignore.end(), num) == hb.ignore.end())
            {
                hb.ignore.pushBack(num);
                reaches++;
            }
        }
    }
    if(vr_debug_box3d.value)
    {
        int bodies = 0;
        for(const World::Inside& i : world->inside)
        {
            bodies += i.prop == num;
        }
        if(bodies || reaches)
        {
            Con_Printf("box3d: %s %d made inside %d bodies and %d hands' or weapons': passes through until clear\n",
                isSmallGib(EDICT_NUM(num)) ? "small gib" : "monster drop", num, bodies, reaches);
        }
    }
}

// The small gibs made inside a body that are clear of it now (their boxes apart), or gone, or held: they meet it again.
void pruneInside()
{
    za::vectorEraseIf(world->inside, [](const World::Inside& i) {
        const int n = static_cast<int>(world->slots.size());
        if(i.prop >= n || i.other >= n || i.prop >= qcvm->num_edicts || i.other >= qcvm->num_edicts)
        {
            return true;
        }
        const Slot& p = world->slots[static_cast<size_t>(i.prop)];
        const Slot& o = world->slots[static_cast<size_t>(i.other)];
        if(p.kind != Kind::Prop || p.born != i.born || B3_IS_NULL(p.body) ||
            (o.kind != Kind::Actor && o.kind != Kind::Player && o.kind != Kind::Corpse) || B3_IS_NULL(o.body))
        {
            return true;
        }
        return boxesApart(b3Body_ComputeAABB(p.body), b3Body_ComputeAABB(o.body));
    });
}

// Whether entities `a` and `b` don't meet: a small gib and the body it was made inside (noteBornInside).
[[nodiscard]] bool bornInside(int a, int b)
{
    return za::anyOf(world->inside.begin(), world->inside.end(),
        [a, b](const World::Inside& i) { return (i.prop == a && i.other == b) || (i.prop == b && i.other == a); });
}

// Once a frame, before the hands' bodies follow the hands (syncReach: `hb.held` is still what each hand carried last
// frame): the throws of the props made this frame (createBody), and the graces over. A throw: a prop let go of by a
// hand (thrown or dropped), or one whose .owner is a player (a thrown weapon: CreateThrownWeapon; a box thrown hard:
// VR_Carry_Release), from the hand nearest it.
void noteThrows()
{
    const float grace = za::max(vr_box3d_throw_grace.value, 0.f);
    const bool debug = vr_debug_box3d.value != 0.f;
    pruneInside();
    za::vectorEraseIf(world->graces, [debug](const World::Grace& g) {
        const bool same = g.num < static_cast<int>(world->slots.size()) && world->slots[g.num].kind == Kind::Prop &&
                          world->slots[g.num].born == g.born;
        if(same && qcvm->time < g.until)
        {
            return false;
        }
        if(debug && same)
        {
            // Its velocity and spin now against as it left (gravity taken out): what the hands' bodies changed.
            const Slot& s = world->slots[g.num];
            const float t = static_cast<float>(qcvm->time - g.born);
            const glm::vec3 fall{0.f, 0.f, -world->gravity / world->m2u * s.gravityScale * t};
            const glm::vec3 v = glmv(b3Body_GetLinearVelocity(s.body)), w = glmv(b3Body_GetAngularVelocity(s.body));
            Con_Printf("box3d: throw %s for %d %s after %.2f s: velocity %.2f m/s, changed by %.2f (gravity out); spin "
                       "%.2f rad/s, changed by %.2f; %d hand contacts passed through\n",
                g.skips ? "grace over" : "watched", g.num, PR_GetString(EDICT_NUM(g.num)->v.classname), t,
                glm::length(v), glm::length(v - fall - g.velocity), glm::length(w), glm::length(w - g.spin), g.skipped.n.loadRelaxed());
        }
        return true;
    });
    const FieldOffsets& f = fields();
    for(const int num : world->made)
    {
        if(num <= svs.maxclients || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()))
        {
            continue;
        }
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Prop || s.born != qcvm->time || B3_IS_NULL(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        // A monster's drop just made (as it died: not a body made again later, after a hold) inside a body (the player
        // standing on the monster, the monster): Box3D flung it out of him at 280 u/s and it hurt him
        // (vr_prop_drop_pass_inside; ROUND21.md, "Monster drops flung into you").
        const float dropAge = monsterDropAge(ent);
        if((vr_smallgibs_pass_inside.value && isSmallGib(ent)) ||
            (vr_prop_drop_pass_inside.value && dropAge >= 0.f && dropAge < 0.1f))
        {
            noteBornInside(num, s);
        }
        else if(corpseMode() != 0)
        {
            noteBornInside(num, s, true);
        }
        const b3AABB box = b3Body_ComputeAABB(s.body);
        const float reach = za::min(0.5f * glm::length(glmv(box.upperBound) - glmv(box.lowerBound)), 0.5f); // m
        int thrower = 0;
        float speed = 0.f;
        for(int i = 1; i <= svs.maxclients && i < static_cast<int>(world->hands.size()); i++)
        {
            for(int h = 0; h < 2; h++)
            {
                if(world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)].held == num)
                {
                    thrower = i;
                    speed = za::max(speed, throwSpeed(EDICT_NUM(i), h, reach));
                }
            }
        }
        const int owner = NUM_FOR_EDICT(PROG_TO_EDICT(ent->v.owner));
        if(thrower == 0 && owner >= 1 && owner <= svs.maxclients && f.handpos >= 0 && f.offhandpos >= 0)
        {
            edict_t* player = EDICT_NUM(owner);
            const glm::vec3 at = vec(ent->v.origin);
            const int h = glm::distance(at, fieldVec(player, f.handpos)) <= glm::distance(at, fieldVec(player, f.offhandpos));
            thrower = owner;
            speed = throwSpeed(player, h, reach);
        }
        if(thrower == 0)
        {
            continue;
        }
        world->slots[num].flight = true; // (its spin aligns in the air: spinAlign)
        if(grace <= 0.f && !debug)
        {
            continue;
        }
        const bool fast = speed >= vr_box3d_throw_grace_speed.value;
        if(debug)
        {
            Con_Printf("box3d: %d %s thrown by %d at %.2f m/s (reach %.2f m): %s\n", num, PR_GetString(ent->v.classname),
                thrower, speed, reach,
                !fast ? "a slow release, it meets the hands" : grace > 0.f ? "passes through the hands" : "watched");
        }
        if(fast || debug)
        {
            World::Grace g;
            g.num = num;
            g.player = thrower;
            g.born = s.born;
            g.skips = fast && grace > 0.f;
            g.until = qcvm->time + (g.skips ? grace : 0.2);
            g.velocity = glmv(b3Body_GetLinearVelocity(s.body));
            g.spin = glmv(b3Body_GetAngularVelocity(s.body));
            world->graces.pushBack(g);
        }
    }
    world->made.clear();
}

// The players' hands as kinematic spheres at their fists (vr_box3d_hand_push): a hand pushes a solid prop (an
// explosive box) as it presses on it, where it presses (a tall box pushed high tips over, a stack pushed low slides),
// continuously while it is in contact, Box3D's friction and contacts doing the rest. Only against solid props (ammo boxes
// and the like are nudged by QC's touches, as before). A hand is not armed while it carries something, nor again until
// its sphere is clear of every solid prop (a box let go of isn't shoved away by the hand inside it).
// With vr_box3d_hand_push_fist, the listen server's own player's hands are their drawn fists instead (held::fist: the
// grab's spheres), turned with the hand: a box is pushed where the fist meets it, and the fist that pushes it touches it
// to take it (the sphere at the hand's point met a box up to 7 cm before the drawn fist did).
// The same fist within a twentieth of a unit (the view makes it again each frame, through the hand's placement).
[[nodiscard]] bool sameFist(const za::Vector<glm::vec4>& a, const za::Vector<glm::vec4>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(za::SizeT k = 0; k < a.size(); k++)
    {
        const glm::vec4 d = glm::abs(a[k] - b[k]);
        if(za::max(za::max(d.x, d.y), za::max(d.z, d.w)) > 0.05f)
        {
            return false;
        }
    }
    return true;
}

const za::Vector<glm::vec4> noFist; // (syncHands: a hand that pushes no fist's spheres)

void syncHands(float dt)
{
    QVR_PROFILE("box3d hands");
    const FieldOffsets& f = fields();
    world->hands.resize(static_cast<size_t>(svs.maxclients) + 1);
    const float radius = 0.045f; // m: a fist's
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        const bool live = vr_box3d_hand_push.value && !player->free && svs.clients[i - 1].active && player->v.health > 0.f &&
                          f.handpos >= 0 && fieldFloatOr(player, f.ishuman, 0.f) != 0.f;
        for(int h = 0; h < 2; h++)
        {
            World::HandBody& hb = world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)];
            const int heldOfs = h ? f.mainhand_held : f.offhand_held;
            const bool holding = heldOfs >= 0 && fieldInt(player, heldOfs) > 0;
            // The fist's middle: a radius behind the hand's point (the front of the fist), along its angles.
            const glm::vec3 point = fieldVec(player, h ? f.handpos : f.offhandpos);
            const glm::vec3 angles = fieldVec(player, h ? f.handrot : f.offhandrot);
            vec3_t in{angles.x, angles.y, angles.z}, fwd, right, up;
            AngleVectors(in, fwd, right, up);
            const za::Vector<glm::vec4>& fist = i == 1 && vr_box3d_hand_push_fist.value ? held::fist(h) : noFist;
            const bool byFist = !fist.empty();
            const glm::vec3 at = byFist ? point : point - vec(fwd) * (radius * world->m2u);
            const b3Quat turn = byFist ? toB3(glm::quat_cast(held::axesFromAngles(&angles[0], true))) : b3Quat_identity;
            if(B3_IS_NON_NULL(hb.body) && !sameFist(hb.fist, fist)) // the fist changed (its scale, the hand model): made again
            {
                b3DestroyBody(hb.body);
                hb.body = b3_nullBodyId;
                hb.armed = false;
            }
            if(!live || point == glm::vec3{0.f})
            {
                if(B3_IS_NON_NULL(hb.body))
                {
                    b3DestroyBody(hb.body);
                    hb = World::HandBody{};
                }
                continue;
            }
            if(B3_IS_NULL(hb.body))
            {
                b3BodyDef def = b3DefaultBodyDef();
                def.type = b3_kinematicBody;
                def.position = world->toM(at);
                def.userData = userOf(i);
                def.rotation = turn;
                hb.body = b3CreateBody(world->id, &def);
                b3ShapeDef shape = shapeDef(i, catHand, catSolid);
                shape.enablePreSolveEvents = true; // (preSolve: not what its player just threw)
                hb.fist = fist;
                if(byFist)
                {
                    for(const glm::vec4& s : fist)
                    {
                        const b3Sphere sphere{world->toM(glm::vec3{s}), s.w / world->m2u};
                        b3CreateSphereShape(hb.body, &shape, &sphere);
                    }
                }
                else
                {
                    const b3Sphere sphere{b3Vec3_zero, radius};
                    b3CreateSphereShape(hb.body, &shape, &sphere);
                }
                b3Body_Disable(hb.body);
                hb.armed = false;
                hb.at = at;
            }
            // Armed only when empty and clear of solid props (it would shove what it is in).
            bool want = !holding;
            if(want && !hb.armed)
            {
                const b3Vec3 zero = b3Vec3_zero;
                b3QueryFilter filter = b3DefaultQueryFilter();
                filter.categoryBits = catHand;
                filter.maskBits = catSolid;
                bool overlaps = false;
                const auto test = [&](const glm::vec3& centre, float r) {
                    const b3ShapeProxy proxy{&zero, 1, r};
                    b3World_OverlapShape(world->id, world->toM(centre), &proxy, filter,
                        [](b3ShapeId, void* context) {
                            *static_cast<bool*>(context) = true;
                            return false;
                        },
                        &overlaps);
                };
                if(byFist)
                {
                    const glm::mat3 axes = held::axesFromAngles(&angles[0], true);
                    for(za::SizeT k = 0; k < fist.size() && !overlaps; k++)
                    {
                        test(point + axes * glm::vec3{fist[k]}, fist[k].w / world->m2u);
                    }
                }
                else
                {
                    test(at, radius);
                }
                want = !overlaps;
            }
            if(want != hb.armed)
            {
                hb.armed = want;
                if(want)
                {
                    b3Body_Enable(hb.body);
                    b3Body_SetTransform(hb.body, world->toM(at), turn);
                    b3Body_SetLinearVelocity(hb.body, b3Vec3_zero);
                    hb.at = at;
                }
                else
                {
                    b3Body_Disable(hb.body);
                }
            }
            if(hb.armed)
            {
                // As a player's body: it shoves at most at vr_box3d_hand_push_speed, jumping the rest of its move (a
                // weapon taken moves the hand at once, a punch goes faster than a heavy box should fly), and what it
                // then overlaps is eased out of it.
                const float distance = glm::distance(at, hb.at);
                const float most = za::max(vr_box3d_hand_push_speed.value, 0.1f) * world->m2u * dt;
                if(distance > 0.5f * world->m2u) // a jump
                {
                    b3Body_SetTransform(hb.body, world->toM(at), turn);
                    b3Body_SetLinearVelocity(hb.body, b3Vec3_zero);
                }
                else
                {
                    if(distance > most)
                    {
                        b3Body_SetTransform(hb.body, world->toM(at - (at - hb.at) * (most / distance)), turn);
                    }
                    b3Body_SetTargetTransform(hb.body, b3WorldTransform{world->toM(at), turn}, dt, true);
                }
            }
            hb.at = at;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The reach bodies (ROUND21.md, "Hands and weapons as bodies"): each hand's own body besides the sphere above, following
// the hand at full speed as a carried prop does. Empty (vr_box3d_hand_props), the open hand (a box: the palm and the
// fingers) or the fist; holding a weapon (vr_box3d_weapon_push), the weapon's drawn hull (the local player's, as the
// view draws it: view::drawnWeapon; another player's, a capsule from the hand to the muzzle). They push the loose props
// (not solid ones: the sphere and the guns' nudge push those, as before) and hold them up: a thing let go of on an open
// palm stays there, one balances on a gun. Kinematic, not dynamic: the drawn hand is where the player's real hand is,
// whatever it meets (a body jointed to it would lag behind it, stop where it presses, and push back on nothing the
// player feels), and it bats a grenade away as hard as it swings.

// The empty hand's boxes (the drawn hand at the defaults, vr_dumpview's open hand and fist): cm from the hand's point
// in the main hand's frame (forward, left, up; the off hand's mirrored). The open hand's top is its palm (its middle is
// 0.8 cm right of the point): what rests there lies on the palm, not in the relaxed fingers' curl. It reaches 2 cm past
// the hand's upper edge (the index finger's side, -0.3 cm): a thing let go of from the fist is held at the hand's point,
// at that edge, and would roll off the palm turned up (the thumb and index hold it there).
struct HandBox
{
    glm::vec3 lo, hi;
};
constexpr HandBox openHandBox{{-15.5f, -4.4f, -12.f}, {5.5f, -0.8f, 2.f}};
constexpr HandBox fistBox{{-16.f, -4.3f, -12.1f}, {-0.9f, 3.2f, -3.1f}};
constexpr float reachSink = 0.01f;     // m: sunk no deeper in a new reach body, a prop rests on it; deeper, passes through
constexpr float reachCapsule = 0.015f; // m: another player's weapon's radius
constexpr float restUp = 0.3f;         // a prop rests on a hand where their contact's normal (up into it) is this steep (restsOnHand)

// The hand whose reach body `body` is (of client `player`), or nullptr.
[[nodiscard]] World::HandBody* reachOf(b3BodyId body, int player)
{
    if(player < 1 || player >= static_cast<int>(world->hands.size()))
    {
        return nullptr;
    }
    for(const World::PortalCopy& copy : world->portalCopies)
    {
        if(B3_ID_EQUALS(copy.copy, body)) { body = copy.original; break; }
    }
    for(World::HandBody& hb : world->hands[static_cast<size_t>(player)])
    {
        if(B3_IS_NON_NULL(hb.reach) && B3_ID_EQUALS(hb.reach, body))
        {
            return &hb;
        }
    }
    return nullptr;
}

// The weapon `player`'s hand `h` holds: its own mass (kg, Weapon Weights: Mass; 0: none, or not a weapon).
[[nodiscard]] float heldWeaponMass(edict_t* player, int h)
{
    const FieldOffsets& f = fields();
    const char* name = h ? PR_GetString(player->v.weaponmodel)
                         : (f.weaponmodel2 >= 0 ? PR_GetString(fieldInt(player, f.weaponmodel2)) : "");
    const int slot = name && *name ? weapons::slotForName(name) : -1;
    return slot >= 0 ? za::max(weapons::value(slot, weapons::Key::Mass), 0.f) : 0.f;
}

// What hand `h` of client `i` is now (`last`: what it was), and for a drawn weapon its entity in the hand's frame.
[[nodiscard]] World::ReachKey reachKey(edict_t* player, int i, int h, const World::ReachKey& last, const glm::vec3& point,
    const glm::vec3& angles, glm::mat4& inHand)
{
    using Key = World::ReachKey;
    const FieldOffsets& f = fields();
    Key key;
    const int heldOfs = h ? f.mainhand_held : f.offhand_held;
    if(heldOfs >= 0 && fieldInt(player, heldOfs) > 0)
    {
        return key; // (what it carries is a body of its own: Kind::Held)
    }
    const float weapon = h ? player->v.weapon : fieldFloatOr(player, f.weapon2, 0.f);
    if(weapon == 0.f) // QC's WID_FIST
    {
        if(!vr_box3d_hand_props.value)
        {
            return key;
        }
        const int bits = static_cast<int>(fieldFloatOr(player, f.vrbits0, 0.f));
        key.what = (bits & (h ? 8 : 2)) ? Key::Fist : Key::Open; // (QC's QVR_VRBITS0_*HAND_GRABBING)
        key.scale = weapons::offsetScale();
        return key;
    }
    if(!vr_box3d_weapon_push.value)
    {
        return key;
    }
    const int modelOfs = f.weaponmodel2;
    const char* name = h ? PR_GetString(player->v.weaponmodel)
                         : (modelOfs >= 0 ? PR_GetString(fieldInt(player, modelOfs)) : "");
    if(i == 1 && cls.state == ca_connected) // the local player: the weapon as drawn
    {
        const view::DrawnWeapon& d = view::drawnWeapon(h);
        if(!d.model || vr_gametime - d.when > 0.5 || strcmp(d.model->name, name) != 0)
        {
            return key; // (not drawn yet)
        }
        key.what = Key::Weapon;
        key.model = d.model;
        key.mirrored = d.mirrored;
        key.generation = shapeGeneration;
        inHand = d.inHand;
        return key;
    }
    const int muzzleOfs = h ? f.muzzlepos : f.offmuzzlepos;
    const glm::vec3 muzzle = muzzleOfs >= 0 ? fieldVec(player, muzzleOfs) : point;
    if(glm::distance(muzzle, point) < 1.f)
    {
        return key;
    }
    key.what = Key::Capsule;
    key.muzzle = glm::transpose(held::axesFromAngles(&angles[0], true)) * (muzzle - point);
    if(last.what == Key::Capsule && glm::distance(last.muzzle, key.muzzle) < 2.f)
    {
        key.muzzle = last.muzzle; // (the same capsule: it follows the gun's muzzle within 2 units)
    }
    return key;
}

// What the new reach body of `hb` has sunk in it deeper than reachSink: passed through until clear of it (a thing thrown
// from the hand, a weapon let go of, a gun taken where props lie). What is only touching it rests on it, and so does
// what the empty hand let go of this frame without throwing it (`letGo`: slower than 1 m/s; not a weapon): it is eased
// out of the hand, onto the palm turned up.
void ignoreInside(World::HandBody& hb, uint64_t category, int letGo)
{
    hb.ignore.clear();
    za::Array<b3ShapeId, 4> shapes;
    const int count = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
    za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int i = 0; i < count; i++)
    {
        int n = 0;
        float radius = 0.f;
        if(b3Shape_GetType(shapes[i]) == b3_hullShape)
        {
            const b3HullData* hull = b3Shape_GetHull(shapes[i]);
            const b3Vec3* p = b3GetHullPoints(hull);
            const b3Vec3 c = hull->center;
            n = za::min(hull->vertexCount, static_cast<int>(points.size()));
            for(int k = 0; k < n; k++)
            {
                // Each corner in by the sink along each axis (not past the middle).
                b3Vec3 q = p[k];
                q.x = q.x > c.x ? za::max(c.x, q.x - reachSink) : za::min(c.x, q.x + reachSink);
                q.y = q.y > c.y ? za::max(c.y, q.y - reachSink) : za::min(c.y, q.y + reachSink);
                q.z = q.z > c.z ? za::max(c.z, q.z - reachSink) : za::min(c.z, q.z + reachSink);
                points[static_cast<size_t>(k)] = b3RotateVector(xf.q, q);
            }
        }
        else if(b3Shape_GetType(shapes[i]) == b3_capsuleShape)
        {
            const b3Capsule c = b3Shape_GetCapsule(shapes[i]);
            points[0] = b3RotateVector(xf.q, c.center1);
            points[1] = b3RotateVector(xf.q, c.center2);
            n = 2;
            radius = za::max(0.f, c.radius - reachSink);
        }
        if(n == 0)
        {
            continue;
        }
        const b3ShapeProxy proxy{points.data(), n, radius};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = category;
        filter.maskBits = catProp;
        struct Context
        {
            za::Vector<int>& ignore;
            int placed; // (let go of onto the hand)
        } context{hb.ignore, 0};
        if(letGo > 0 && letGo < static_cast<int>(world->slots.size()) && world->slots[letGo].kind == Kind::Prop &&
            !isWeaponLike(EDICT_NUM(letGo)) && b3Length(b3Body_GetLinearVelocity(world->slots[letGo].body)) < 1.f)
        {
            context.placed = letGo;
        }
        b3World_OverlapShape(world->id, xf.p, &proxy, filter,
            [](b3ShapeId shape, void* raw) {
                auto& c = *static_cast<Context*>(raw);
                const int num = numOf(shape);
                if(num > 0 && num != c.placed && num < static_cast<int>(world->slots.size()) &&
                    world->slots[num].kind == Kind::Prop &&
                    za::find(c.ignore.begin(), c.ignore.end(), num) == c.ignore.end())
                {
                    c.ignore.pushBack(num);
                }
                return true;
            },
            &context);
    }
    if(vr_debug_box3d.value && !hb.ignore.empty())
    {
        Con_Printf("box3d: a reach body made with %d props sunk in it: it passes through them until clear\n",
            static_cast<int>(hb.ignore.size()));
    }
}

// The reach body of `hb` made again for `key` at `pos`, `rot` (none for None); `letGo`: what the hand let go of.
void makeReach(
    World::HandBody& hb, int i, int h, const World::ReachKey& key, const glm::vec3& pos, const glm::quat& rot, int letGo)
{
    using Key = World::ReachKey;
    if(B3_IS_NON_NULL(hb.reach))
    {
        b3DestroyBody(hb.reach);
        hb.reach = b3_nullBodyId;
    }
    hb.ignore.clear();
    if(key.what == Key::None)
    {
        return;
    }
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = world->toM(pos);
    def.rotation = toB3(rot);
    def.userData = userOf(i);
    hb.reach = b3CreateBody(world->id, &def);
    const bool weapon = key.what == Key::Weapon || key.what == Key::Capsule;
    const uint64_t category = weapon ? catReachWeapon : catReachHand;
    b3ShapeDef shape = shapeDef(i, category, catProp | catCorpse);
    shape.enableCustomFiltering = true; // (shouldCollide: reachMeets)
    shape.enablePreSolveEvents = true;  // (preSolve)
    switch(key.what)
    {
    case Key::Open:
    case Key::Fist:
    {
        const HandBox& b = key.what == Key::Open ? openHandBox : fistBox;
        glm::vec3 lo = b.lo, hi = b.hi;
        if(h == 0) // the off hand: mirrored
        {
            lo.y = -b.hi.y;
            hi.y = -b.lo.y;
        }
        const float cm = 0.01f * key.scale; // metres
        const glm::vec3 half = (hi - lo) * 0.5f * cm, centre = (lo + hi) * 0.5f * cm;
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, b3v(centre));
        b3CreateHullShape(hb.reach, &shape, &box.base);
        break;
    }
    case Key::Weapon:
    {
        za::Vector<glm::vec3>& vertices = scratch.propVerts;
        if(held::modelVertices(key.model, key.mirrored, vertices) && vertices.size() >= 4)
        {
            za::Vector<b3Vec3> points;
            points.reserve(vertices.size());
            for(const glm::vec3& v : vertices)
            {
                points.pushBack(world->toM(v));
            }
            if(b3HullData* hull = fittedHull(points, 32))
            {
                b3CreateHullShape(hb.reach, &shape, hull); // (the world keeps its own copy)
                if(vr_debug_box3d.value)
                {
                    Con_Printf("box3d: %s hand's weapon %s: a hull of %d vertices\n", h ? "main" : "off", key.model->name,
                        hull->vertexCount);
                }
                b3DestroyHull(hull);
            }
        }
        break;
    }
    case Key::Capsule:
    {
        const b3Capsule capsule{b3Vec3_zero, world->toM(key.muzzle), reachCapsule};
        b3CreateCapsuleShape(hb.reach, &shape, &capsule);
        break;
    }
    default: break;
    }
    if(vr_debug_box3d.value)
    {
        static constexpr const char* names[] = {"none", "open hand", "fist", "weapon", "capsule"};
        const glm::vec3 palm = glm::mat3_cast(rot) * glm::vec3{0.f, h ? 1.f : -1.f, 0.f};
        const b3AABB box = b3Body_ComputeAABB(hb.reach);
        const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
        Con_Printf("box3d: %s hand's reach body: %s at %.1f %.1f %.1f (%.1f %.1f %.1f to %.1f %.1f %.1f), the palm facing "
                   "%.2f %.2f %.2f\n",
            h ? "main" : "off", names[key.what], pos.x, pos.y, pos.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, palm.x, palm.y,
            palm.z);
    }
    ignoreInside(hb, category, weapon ? 0 : letGo);
}

// Whether the reach body of `hb` (of client `player`) passes through prop `num` now: what it passes through until clear
// (sunk in it as it was made: its ignore list), what its player just threw (graced), a small gib just torn out
// (smallGibSpared), an empty hand a grenade (reachMeets), a prop flying to a hand (a force grab's pull: .fg_state 1),
// the player's own grenade in its first quarter second (leaving the launcher's muzzle, inside the gun).
[[nodiscard]] bool reachSkips(const World::HandBody& hb, int player, int num)
{
    if(za::find(hb.ignore.begin(), hb.ignore.end(), num) != hb.ignore.end() || graced(player, num) || smallGibSpared(num))
    {
        return true;
    }
    if(num <= svs.maxclients || num >= qcvm->num_edicts || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[num];
    if(s.kind != Kind::Prop)
    {
        return false;
    }
    const bool grenade = isGrenade(s.model);
    if(grenade && hb.key.what != World::ReachKey::Weapon && hb.key.what != World::ReachKey::Capsule)
    {
        return true;
    }
    edict_t* e = EDICT_NUM(num);
    if(const int fg = fields().fg_state; fg >= 0 && fieldFloat(e, fg) == 1.f)
    {
        return true;
    }
    // A loose round at the loading port of this hand's gun, lying the way it goes in (QC vr_reload.qc
    // VR_Reload_LooseFrame: .vr_ammo_passer the player, .vr_ammo_pass the hands, 1 off, 2 main): it passes through the
    // gun's hull, which covers the port (the receiver's opening, the well), to reach it.
    if(const FieldOffsets& f = fields(); f.vr_ammo_pass >= 0 && f.vr_ammo_passer >= 0 &&
        (hb.key.what == World::ReachKey::Weapon || hb.key.what == World::ReachKey::Capsule))
    {
        const int hands = static_cast<int>(fieldFloat(e, f.vr_ammo_pass));
        const bool main = player < static_cast<int>(world->hands.size()) && &hb == &world->hands[player][1];
        if((hands & (main ? 2 : 1)) && fieldInt(e, f.vr_ammo_passer) == EDICT_TO_PROG(EDICT_NUM(player)))
        {
            return true;
        }
    }
    return grenade && e->v.owner == EDICT_TO_PROG(EDICT_NUM(player)) && qcvm->time - s.born < 0.25;
}

struct ReachPose
{
    glm::vec3 pos; // units
    glm::quat rot;
};

// Pushes by mass (ROUND21.md, "Pushes by mass"): the hands, their weapons and what they carry are kinematic bodies (of
// no give), which knock whatever they meet as if it weighed nothing: a flick of the wrist sent a 40 kg box across the
// room. Each is given a mass instead, what an arm puts behind it: an empty hand's vr_box3d_hand_mass, a held weapon's
// own (Weapon Weights: Mass) and vr_box3d_weapon_arm_mass, a carried prop's own and each hand's holding it. A prop of mass `m` struck by `pusher` kg keeps
// pusher / (pusher + m) of the velocity the kinematic body gave it (the share of two masses meeting: a 0.4 kg grenade
// most of it, a 40 kg box a tenth). 1: no limit (the setting 0).
[[nodiscard]] float pushShare(float pusher, float m)
{
    return pusher > 0.f && m > 0.f ? pusher / (pusher + m) : 1.f;
}

// How much of the velocity `gain` (m/s) a hand's body of `pusher` kg gives the prop `s` of `m` kg in this step it keeps:
// the first push a hit (pushShare), the steps after (pushed in the step before, or struck by a swing this frame: the body
// kinematic goes on at the hand's speed, which a real arm would not) a shove: no more than the arm's force
// (vr_box3d_push_force) gives its mass in the step, so that a heavy prop moves as far as it is pushed and stops, and a
// light one is carried along. Notes the push (Slot::pushedStep).
[[nodiscard]] float pushKeep(Slot& s, float pusher, float m, float gain, float dt)
{
    const bool shove = s.pushedStep >= world->steps - 1;
    s.pushedStep = world->steps;
    float keep = pushShare(pusher, m);
    if(shove && pusher > 0.f && m > 0.f && gain > 0.f && vr_box3d_push_force.value > 0.f)
    {
        keep = za::min(keep, vr_box3d_push_force.value * dt / (m * gain));
    }
    return keep;
}

// A hand's body's mass: an empty hand's vr_box3d_hand_mass; a weapon's its own (Weapon Weights: Mass) and the arm's
// behind it (vr_box3d_weapon_arm_mass), so the heavy ones (the rocket launcher, the hammer) bat harder than the light.
[[nodiscard]] float reachMass(const World::HandBody& hb)
{
    const bool weapon = hb.key.what == World::ReachKey::Weapon || hb.key.what == World::ReachKey::Capsule;
    return weapon ? za::max(vr_box3d_weapon_arm_mass.value, 0.f) + hb.weaponMass : za::max(vr_box3d_hand_mass.value, 0.f);
}

// A fast move of the reach body of `hb` over a step `dt` (a swing): Box3D collides once a step, at its start, so a
// kinematic body moving further in a step than a thin thing is across jumps over it (an axe swung at 10 m/s moves 11 cm
// a frame; a grenade is a few cm across). Its shape is swept from `from` to `to` in pieces moving no point of it more than
// 1.5 cm (Box3D's contacts reach 2 cm ahead); each prop it meets there that it doesn't touch at `from` is struck as the
// step would have: where the body's surface first meets it, its velocity along the surface's normal (the prop's centre
// from its nearest point) made the body's point's there and more by the prop's restitution, as off anything a
// kinematic body (of no give) hits. It then leaves ahead of the body, which goes on through where it was.
void sweepReach(const World::HandBody& hb, int player, const ReachPose& from, const ReachPose& to, float dt)
{
    constexpr float piece = 0.015f; // m
    za::Array<b3ShapeId, 1> shapes;
    if(b3Body_GetShapes(hb.reach, shapes.data(), 1) < 1)
    {
        return;
    }
    // The shape's points (its frame, m) and radius.
    za::Array<b3Vec3, 64> local;
    int count = 0;
    float radius = 0.f;
    if(b3Shape_GetType(shapes[0]) == b3_hullShape)
    {
        const b3HullData* hull = b3Shape_GetHull(shapes[0]);
        count = za::min(hull->vertexCount, static_cast<int>(local.size()));
        za::copy(b3GetHullPoints(hull), b3GetHullPoints(hull) + count, local.begin());
    }
    else if(b3Shape_GetType(shapes[0]) == b3_capsuleShape)
    {
        const b3Capsule c = b3Shape_GetCapsule(shapes[0]);
        local[0] = c.center1;
        local[1] = c.center2;
        count = 2;
        radius = c.radius;
    }
    if(count == 0)
    {
        return;
    }
    const glm::vec3 p0 = glmv(world->toM(from.pos)), p1 = glmv(world->toM(to.pos));
    float most = 0.f;
    for(int i = 0; i < count; i++)
    {
        const glm::vec3 v = glmv(local[static_cast<size_t>(i)]);
        most = za::max(most, glm::distance(p0 + from.rot * v, p1 + to.rot * v));
    }
    const int pieces = za::min(static_cast<int>(za::ceil(most / piece)), 24);
    if(pieces <= 1 || dt <= 0.f)
    {
        return;
    }
    const auto poseAt = [&](int k) {
        const float t = static_cast<float>(k) / static_cast<float>(pieces);
        return ReachPose{glm::mix(from.pos, to.pos, t), glm::slerp(from.rot, to.rot, t)};
    };
    // What it meets over the points of two poses (their convex hull: the swept piece), relative to the second's place.
    za::Vector<int> found;
    const uint64_t category = b3Shape_GetFilter(shapes[0]).categoryBits;
    const auto meets = [&](const ReachPose& a, const ReachPose& b) {
        za::Array<b3Vec3, 128> points;
        const glm::vec3 origin = glmv(world->toM(b.pos)), shift = glmv(world->toM(a.pos)) - origin;
        for(int i = 0; i < count; i++)
        {
            const glm::vec3 v = glmv(local[static_cast<size_t>(i)]);
            points[static_cast<size_t>(i)] = b3v(b.rot * v);
            points[static_cast<size_t>(count + i)] = b3v(shift + a.rot * v);
        }
        const b3ShapeProxy proxy{points.data(), 2 * count, radius};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = category;
        filter.maskBits = catProp;
        found.clear();
        b3World_OverlapShape(world->id, b3v(origin), &proxy, filter,
            [](b3ShapeId shape, void* context) {
                auto& out = *static_cast<za::Vector<int>*>(context);
                const int num = numOf(shape);
                if(za::find(out.begin(), out.end(), num) == out.end())
                {
                    out.pushBack(num);
                }
                return true;
            },
            &found);
    };
    meets(from, from);
    za::Vector<int> struck = found; // (touching it at the start: Box3D's contact)

    // The body's motion over the step: its origin's velocity, and its spin (m/s, rad/s).
    const glm::vec3 linear = (p1 - p0) / dt;
    glm::quat turn = to.rot * glm::inverse(from.rot);
    if(turn.w < 0.f)
    {
        turn = -turn;
    }
    const float angle = 2.f * za::acos(za::min(turn.w, 1.f));
    const float sine = za::sqrt(za::max(0.f, 1.f - turn.w * turn.w));
    const glm::vec3 spin = sine > 1e-6f ? glm::vec3{turn.x, turn.y, turn.z} / sine * (angle / dt) : glm::vec3{0.f};
    const glm::quat rot0 = from.rot;
    for(int k = 1; k <= pieces; k++)
    {
        const ReachPose a = poseAt(k - 1);
        meets(a, poseAt(k));
        for(const int num : found)
        {
            if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) ||
                world->slots[num].kind != Kind::Prop ||
                za::find(struck.begin(), struck.end(), num) != struck.end() || reachSkips(hb, player, num))
            {
                continue;
            }
            struck.pushBack(num);
            const b3BodyId prop = world->slots[num].body;
            za::Array<b3ShapeId, 1> propShape;
            if(b3Body_GetShapes(prop, propShape.data(), 1) < 1)
            {
                continue;
            }
            // Where the body's surface is nearest the prop's centre with the body at `a` (asked of the shape where it is
            // now, `from`: the centre taken into its frame there and back).
            const glm::vec3 pa = glmv(world->toM(a.pos));
            const glm::vec3 centre = glmv(b3Body_GetWorldCenter(prop));
            const glm::vec3 inBody = glm::inverse(a.rot) * (centre - pa);
            const glm::vec3 target0 = p0 + rot0 * inBody;
            const glm::vec3 near0 = glmv(b3Shape_GetClosestPoint(shapes[0], b3v(target0)));
            const glm::quat back = a.rot * glm::inverse(rot0);
            const glm::vec3 at = pa + back * (near0 - p0);
            const glm::vec3 point = linear + glm::cross(spin, at - pa);
            const glm::vec3 was = glmv(b3Body_GetLinearVelocity(prop));
            glm::vec3 normal = back * (target0 - near0);
            if(glm::length(normal) < 1e-4f) // (its centre inside the body: along the body's motion there)
            {
                normal = point - was;
            }
            if(glm::length(normal) < 1e-6f)
            {
                continue;
            }
            normal = glm::normalize(normal);
            const float closing = glm::dot(point - was, normal);
            if(closing <= 0.f)
            {
                continue;
            }
            const float restitution = b3Shape_GetRestitution(propShape[0]);
            const float gain = closing * (1.f + restitution);
            const float share = pushKeep(world->slots[num], reachMass(hb), b3Body_GetMass(prop), gain, dt); // (by mass)
            const glm::vec3 now = was + normal * (gain * share);
            b3Body_SetLinearVelocity(prop, b3v(now));
            b3Body_SetAwake(prop, true);
            if(vr_smallgibs_trace.value && smallGibAge(EDICT_NUM(num)) >= 0.f && smallGibAge(EDICT_NUM(num)) <= 1.f)
            {
                const int idField = fields().vr_sgib_id;
                Con_Printf("sgibtrace: #%.0f %.3f s: struck by a swing's sweep (player %d), %.0f u/s after\n",
                    idField >= 0 ? fieldFloat(EDICT_NUM(num), idField) : 0.f, smallGibAge(EDICT_NUM(num)), player,
                    glm::length(now) * world->m2u);
            }
            if(vr_debug_box3d.value)
            {
                Con_Printf("box3d: a reach body's swing (%.1f cm and %.1f degrees this frame, piece %d of %d) strikes %d %s "
                           "at %.0f u/s (x%.2f by mass): %.0f u/s after (%.0f %.0f %.0f)\n",
                    most * 100.f, glm::degrees(angle), k, pieces, num, PR_GetString(EDICT_NUM(num)->v.classname),
                    closing * world->m2u, share, glm::length(now) * world->m2u, now.x * world->m2u, now.y * world->m2u,
                    now.z * world->m2u);
            }
        }
    }
}

void syncReach(float dt)
{
    QVR_PROFILE("box3d reach");
    using Key = World::ReachKey;
    const FieldOffsets& f = fields();
    world->hands.resize(static_cast<size_t>(svs.maxclients) + 1);
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        const bool live = !player->free && svs.clients[i - 1].active && player->v.health > 0.f && f.handpos >= 0 &&
                          fieldFloatOr(player, f.ishuman, 0.f) != 0.f;
        for(int h = 0; h < 2; h++)
        {
            World::HandBody& hb = world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)];
            const glm::vec3 point = live ? fieldVec(player, h ? f.handpos : f.offhandpos) : glm::vec3{0.f};
            const glm::vec3 angles = live ? fieldVec(player, h ? f.handrot : f.offhandrot) : glm::vec3{0.f};
            glm::mat4 inHand{1.f};
            const Key key = live && point != glm::vec3{0.f} ? reachKey(player, i, h, hb.key, point, angles, inHand) : Key{};
            hb.weaponMass = key.what == Key::Weapon || key.what == Key::Capsule ? heldWeaponMass(player, h) : 0.f;

            // Its frame: the hand's (forward, left, up), or the drawn weapon's entity.
            glm::mat4 frame{held::axesFromAngles(&angles[0], true)};
            frame[3] = glm::vec4{point, 1.f};
            if(key.what == Key::Weapon)
            {
                frame = frame * inHand;
            }
            const glm::vec3 pos{frame[3]};
            const glm::quat rot = glm::normalize(glm::quat_cast(glm::mat3{frame}));
            // What it carries (to know what it lets go of).
            const int heldOfs = h ? f.mainhand_held : f.offhand_held;
            const int held = live && heldOfs >= 0 && fieldInt(player, heldOfs) > 0
                                 ? NUM_FOR_EDICT(PROG_TO_EDICT(fieldInt(player, heldOfs)))
                                 : 0;
            const int letGo = held == 0 ? hb.held : 0;
            hb.held = held;
            if(!(key == hb.key))
            {
                makeReach(hb, i, h, key, pos, rot, letGo);
                hb.key = key;
                continue;
            }
            if(B3_IS_NULL(hb.reach))
            {
                continue;
            }

            // What it passes through, until clear of it.
            if(!hb.ignore.empty())
            {
                const b3AABB mine = b3Body_ComputeAABB(hb.reach);
                za::vectorEraseIf(hb.ignore, [&](int num) {
                    if(num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
                    {
                        return true;
                    }
                    const b3AABB theirs = b3Body_ComputeAABB(world->slots[num].body);
                    return mine.upperBound.x < theirs.lowerBound.x || theirs.upperBound.x < mine.lowerBound.x ||
                           mine.upperBound.y < theirs.lowerBound.y || theirs.upperBound.y < mine.lowerBound.y ||
                           mine.upperBound.z < theirs.lowerBound.z || theirs.upperBound.z < mine.lowerBound.z;
                });
            }

            // As a carried prop's body: the velocity that gets it where the hand is by the end of the step, or there at
            // once for a jump (a teleport).
            const b3WorldTransform now = b3Body_GetTransform(hb.reach);
            const glm::vec3 at = world->toU(now.p);
            const bool moved = glm::distance(at, pos) > 0.01f || za::fabs(glm::dot(fromB3(now.q), rot)) < 0.999999f;
            if(!moved && !b3Body_IsAwake(hb.reach))
            {
                continue;
            }
            if(glm::distance(at, pos) > 0.5f * world->m2u)
            {
                b3Body_SetTransform(hb.reach, world->toM(pos), toB3(rot));
                b3Body_SetLinearVelocity(hb.reach, b3Vec3_zero);
                b3Body_SetAngularVelocity(hb.reach, b3Vec3_zero);
            }
            else
            {
                b3Body_SetTargetTransform(hb.reach, b3WorldTransform{world->toM(pos), toB3(rot)}, dt, true);
                if(vr_debug_box3d.value >= 2.f)
                {
                    Con_Printf("box3d: %s reach body at %.1f %.1f %.1f, the hand's at %.1f %.1f %.1f: %.0f u/s\n",
                        h ? "main" : "off", at.x, at.y, at.z, pos.x, pos.y, pos.z,
                        glm::length(world->toU(b3Body_GetLinearVelocity(hb.reach))));
                }
                // A swing that would jump over what it meets strikes it (sweepReach).
                sweepReach(hb, i, ReachPose{at, fromB3(now.q)}, ReachPose{pos, rot}, dt);
            }
        }
    }
}

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
    watchCorpses();
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
                shockRagdoll(ent, s, dt);
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

// Monsters and other damageable things a thrown prop's thin shape would slip past, within the larger hit box
// (vr_throw_hitbox) along its flight this frame (the old solver's touchNearby). While its throw lasts (QC's .throwhit
// QVR_THROWHIT_NEVER_HIT, 0), also loose gibs and heads that take damage (MOVE_HITGIBS, as missiles meet them; not
// held ones, which are SOLID_NOT): they are props, and Box3D reports two props meeting after the step, when the bounce
// has already spent the thrown thing's speed, so the throw's hit was too slow to hurt (forcegrabbable_touch).
void touchNearby(edict_t* ent, const glm::vec3& from, const glm::vec3& to)
{
    const float half = vr_throw_hitbox.value;
    if(half <= 0.f)
    {
        return;
    }
    vec3_t start, end, mins{-half, -half, -half}, maxs{half, half, half};
    store(from, start);
    store(to, end);
    const int throwhit = fields().throwhit;
    const bool thrown = throwhit >= 0 && fieldFloat(ent, throwhit) == 0.f;
    // Precise hits (vr_hit_precise, vr_hitmodel.cpp): a monster is met at its model as drawn, grown by the hit box's
    // half-size and the thrown tolerance, not at its box.
    const int precise = MOVE_HITMODEL | (static_cast<int>(hitmodel::Class::Thrown) << MOVE_HITMODEL_CLASS_SHIFT);
    const trace_t tr = SV_Move(start, mins, maxs, end, (thrown ? (MOVE_NORMAL | MOVE_HITGIBS) : MOVE_NORMAL) | precise, ent);
    edict_t* hit = tr.ent;
    if(!hit || hit == qcvm->edicts || hit->free || hit == PROG_TO_EDICT(ent->v.owner) || hit->v.takedamage == 0.f)
    {
        return;
    }
    SV_Impact(ent, hit);
}

// A symmetric 3x3's eigenvalues (ascending) and unit eigenvectors (the columns of `vectors`, in the same order): Jacobi's
// rotations.
void eigenSymmetric(const glm::mat3& m, glm::vec3& values, glm::mat3& vectors)
{
    float a[3][3];
    float v[3][3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            a[i][j] = m[j][i];
        }
    }
    for(int sweep = 0; sweep < 16; sweep++)
    {
        const float off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        const float diag = a[0][0] * a[0][0] + a[1][1] * a[1][1] + a[2][2] * a[2][2];
        if(off <= 1e-14f * diag)
        {
            break;
        }
        for(int p = 0; p < 2; p++)
        {
            for(int q = p + 1; q < 3; q++)
            {
                if(a[p][q] == 0.f)
                {
                    continue;
                }
                const float theta = (a[q][q] - a[p][p]) / (2.f * a[p][q]);
                const float t = (theta >= 0.f ? 1.f : -1.f) / (za::abs(theta) + za::sqrt(theta * theta + 1.f));
                const float c = 1.f / za::sqrt(t * t + 1.f), sn = t * c;
                for(int k = 0; k < 3; k++) // a J: its columns p and q
                {
                    const float akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - sn * akq;
                    a[k][q] = sn * akp + c * akq;
                }
                for(int k = 0; k < 3; k++) // J^T a: its rows p and q
                {
                    const float apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - sn * aqk;
                    a[q][k] = sn * apk + c * aqk;
                }
                for(int k = 0; k < 3; k++) // v J
                {
                    const float vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - sn * vkq;
                    v[k][q] = sn * vkp + c * vkq;
                }
            }
        }
    }
    int order[3]{0, 1, 2};
    za::quickSort(order, order + za::getArraySize(order), [&](int x, int y) { return a[x][x] < a[y][y]; });
    for(int i = 0; i < 3; i++)
    {
        values[i] = a[order[i]][order[i]];
        vectors[i] = glm::normalize(glm::vec3{v[0][order[i]], v[1][order[i]], v[2][order[i]]});
    }
}

// Times Spin Alignment for this thing thrown: its weapon's (Weapon Weights) or its own (Held Object Weights).
[[nodiscard]] float spinAlignOf(const qmodel_t* model)
{
    if(!model)
    {
        return 1.f;
    }
    if(const int slot = weapons::slotForModel(model); slot >= 0)
    {
        return za::max(weapons::value(slot, weapons::Key::SpinAlign), 0.f);
    }
    return za::max(props::valueFor(model, props::Key::SpinAlign), 0.f);
}

// A throw in the air (Slot::flight: from the hand until it first touches anything, so Box3D's contacts are left alone):
// its spin turned towards end over end about its steadiest axis, as a thrown axe, knife or rod settles in the air.
// Its body's inertia (its hull's) has three principal axes: a spin about the one it resists turning about the most (an
// axe's: square to its blade's plane, end over end) is stable; about the middle one, unstable (the tennis racket's
// flip, which Box3D's gyroscopic term reproduces); about the least (along the handle), stable only without losses. A
// real throw loses a little to the air and to flexing and so settles on the first; here the spin about the other two
// dies away at vr_throw_spin_align a second times how long it is ((1 - least / most inertia)^2: a box or a ball keeps
// its tumble), and ten times faster for a flat one, whose steadiest axis stands out (a blade: an axe, a sword; a rod
// or a gib, round, a tenth: any turn across its length will do, and the middle one's is kept). `end`: the flight is
// over (the debug's last word).
void spinAlign(edict_t* ent, Slot& s, float dt, bool end)
{
    const int debug = static_cast<int>(vr_debug_spin_align.value);
    const float strength = za::max(vr_throw_spin_align.value, 0.f) * spinAlignOf(s.model);
    if((strength <= 0.f && !debug) || dt <= 0.f)
    {
        return;
    }
    const b3Matrix3 li = b3Body_GetLocalRotationalInertia(s.body);
    const glm::mat3 inertia{glmv(li.cx), glmv(li.cy), glmv(li.cz)};
    glm::vec3 moments;
    glm::mat3 axes;
    eigenSymmetric(inertia, moments, axes);
    if(moments.z <= 0.f)
    {
        return;
    }
    const float elong = 1.f - za::max(moments.x, 0.f) / moments.z;
    const float distinct = (moments.z - moments.y) / moments.z;
    const float flat = za::clamp((distinct - 0.02f) / 0.1f, 0.f, 1.f);
    const float k = strength * elong * elong * (0.1f + 0.9f * flat);
    const float middle = k * flat;
    const glm::mat3 r = glm::mat3_cast(fromB3(b3Body_GetRotation(s.body)));
    const glm::vec3 w = glmv(b3Body_GetAngularVelocity(s.body));
    const float rate = glm::length(w);
    const glm::vec3 local = glm::transpose(r) * w;
    glm::vec3 c{glm::dot(local, axes[0]), glm::dot(local, axes[1]), glm::dot(local, axes[2])};
    if(debug)
    {
        const float t = static_cast<float>(qcvm->time - s.born);
        const float off = rate > 1e-4f ? glm::degrees(za::acos(za::clamp(za::abs(c.z) / rate, 0.f, 1.f))) : 0.f;
        if(end)
        {
            // (Its spin now is the touch's: the flight's last step's.)
            Con_Printf("spinalign: %d %s flight over at %.3f s: spin %.2f rad/s, %.1f deg off its steadiest axis\n",
                NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), t, s.flightRate, s.flightOff);
        }
        else if(debug >= 2 || !s.flightLogged)
        {
            Con_Printf("spinalign: %d %s %.3f s: spin %.2f rad/s, %.1f deg off its steadiest axis (about it %.2f, the "
                       "middle %.2f, the least %.2f); inertia %.3g %.3g %.3g, long %.2f, distinct %.2f, rates %.2f %.2f/s\n",
                NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), t, rate, off, c.z, c.y, c.x, moments.x, moments.y,
                moments.z, elong, distinct, k, middle);
            s.flightLogged = true;
        }
        s.flightOff = off;
        s.flightRate = rate;
    }
    if(end || k <= 0.f || rate < 1e-3f)
    {
        return;
    }
    // Its angular momentum's size is kept (what the air and flexing take is energy, not momentum): a spin moved onto
    // the steadiest axis is slower by the ratio of the inertias (an axe spun about its handle ends turning end over end
    // at a fifth of the rate).
    const float momentum = glm::length(moments * c);
    c.x *= za::exp(-k * dt);
    c.y *= za::exp(-middle * dt);
    const float left = glm::length(moments * c);
    if(left < 1e-9f)
    {
        return;
    }
    c *= za::min(momentum / left, 1.5f);
    b3Body_SetAngularVelocity(s.body, b3v(r * (axes[0] * c.x + axes[1] * c.y + axes[2] * c.z)));
}

// Before the step, for each awake prop: the hit box along its flight, then the water (the old solver's waterStep,
// once a frame: the lift by how deep it is, the drag, floating flat, the bob).
void beforeStep(float dt)
{
    const float g = sv_gravity.value;
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
        s.arrival = glm::vec3{0.f};
        s.lift = 0.f;
        if(s.kind != Kind::Prop || !b3Body_IsAwake(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        const glm::vec3 com = world->toU(b3Body_GetWorldCenter(s.body));
        glm::vec3 vel = world->toU(b3Body_GetLinearVelocity(s.body));

        if(glm::length(vel) > 1.f)
        {
            // A thrown axe whose blade goes into something this step, blade first, sticks in it (vr_axestick.cpp): it is
            // no longer a prop.
            if(axestick::beforeStep(ent, com, vel, glmv(b3Body_GetAngularVelocity(s.body)), dt))
            {
                destroyBody(s);
                continue;
            }
            touchNearby(ent, com, com + vel * dt);
            if(ent->free || kindOf(ent, num, {}) != Kind::Prop)
            {
                destroyBody(s); // gone, or no longer a prop (the next frame sees what it is)
                continue;
            }
            if(vec(ent->v.velocity) != s.velocity || fieldVec(ent, fields().vr_spin) != s.spin)
            {
                feedProp(ent, s); // the touch changed its flight
                vel = world->toU(b3Body_GetLinearVelocity(s.body));
            }
        }
        s.arrival = vel;

        // Fast (a throw: more than a third of its thickness a step), its continuous collision takes in the other props
        // too: Box3D's is only against the world and kinematic bodies otherwise, and a box thrown at 15 m/s crosses a
        // stacked box in a step. (A fifth before: most of a toppling pile's boxes were bullets, a third of its step.)
        const bool fast = glm::length(vel) / world->m2u * dt > 0.35f * b3Body_GetMinExtent(s.body);
        if(fast != s.bullet)
        {
            b3Body_SetBullet(s.body, fast);
            s.bullet = fast;
        }

        // Rolling resistance (the old solver's): touching something, the spin dies away, fast once it is slow, so a
        // thing comes to rest instead of rocking or rolling on (Box3D's own is for spheres and capsules only).
        bool touching = false;
        if(b3Body_GetContactCapacity(s.body) > 0)
        {
            za::Array<b3ContactData, 8> contacts;
            if(b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size())) > 0)
            {
                touching = true;
                const bool slow = glm::length(vel) < 0.5f * world->m2u;
                const b3Vec3 w = b3Body_GetAngularVelocity(s.body);
                const float k = za::exp((slow || s.soft ? -6.f : -1.f) * (s.soft ? 2.f : 1.f) * dt);
                b3Body_SetAngularVelocity(s.body, b3Vec3{w.x * k, w.y * k, w.z * k});
            }
        }

        // A throw's flight, until it first touches something (or the water): its spin turned towards end over end
        // about its steadiest axis (spinAlign). Box3D's contacts stay as they are.
        if(s.flight)
        {
            s.flight = !touching && !s.wet;
            spinAlign(ent, s, dt, !s.flight);
        }

        const b3AABB box = b3Body_ComputeAABB(s.body);
        const float lo = box.lowerBound.z * world->m2u, hi = box.upperBound.z * world->m2u;
        const float part = g > 0.f ? submerged(com, lo, hi) : 0.f;
        const bool wet = part > 0.f;
        s.wet = wet;
        const float density = wet ? waterDensity(ent) : 0.f;
        const bool floats = density > 1.f;
        // Floating, it bobs: never asleep. Sunk, it may sleep once it rests on something (the pool's floor, another
        // prop): there its lift and drag only hold it still, as on land (a pool full of rocks no longer stepped every
        // frame); not while it sinks (a slow one would fall asleep half way down).
        const bool sleepless = wet && (floats || !touching);
        if(sleepless != s.sleepless)
        {
            b3Body_EnableSleep(s.body, !sleepless);
            s.sleepless = sleepless;
        }
        if(!wet)
        {
            continue;
        }
        const float halfHeight = za::max((hi - lo) * 0.5f, 0.5f);
        const float bob = floats ? 1.f + 0.04f * za::sin(static_cast<float>(qcvm->time) * 2.1f + static_cast<float>(num)) : 1.f;
        // The lift is a force through the step, against the gravity (Box3D's, in the same sub-steps: at rest, where
        // they balance, nothing moves and the velocity stays nought). The drag is on the velocity before the step, so
        // that after the step's lift and gravity it is the old solver's: the drag on (velocity + lift - gravity),
        // which gives the same terminal speed up or down (a box that dived deep rises as fast as before).
        const float gs = g * s.gravityScale;
        const float lift = gs * density * part * bob / world->m2u; // m/s^2
        s.lift = b3Body_GetMass(s.body) * lift;
        b3Body_ApplyForceToCenter(s.body, b3Vec3{0.f, 0.f, s.lift}, true);
        const float stiffness = gs * density / (2.f * halfHeight);
        const float restingPart = za::min(1.f, 1.f / density);
        const float drag = 2.4f * za::sqrt(za::max(stiffness, 0.f)) * za::min(1.f, part / restingPart);
        const float e = za::exp(-drag * dt);
        vel.z = vel.z * e + (e - 1.f) * (gs * density * part * bob - gs) * dt;
        const float drift = za::exp(-1.5f * part * dt);
        vel.x *= drift;
        vel.y *= drift;
        glm::vec3 spin = glmv(b3Body_GetAngularVelocity(s.body)) * za::exp(-8.f * part * dt);
        if(floats) // the side nearest the surface turns up to it
        {
            const glm::mat3 axes = glm::mat3_cast(fromB3(b3Body_GetRotation(s.body)));
            int axis = 0;
            for(int i = 1; i < 3; i++)
            {
                if(za::abs(axes[i].z) > za::abs(axes[axis].z))
                {
                    axis = i;
                }
            }
            const glm::vec3 up = axes[axis].z < 0.f ? -axes[axis] : axes[axis];
            spin += glm::cross(up, glm::vec3{0.f, 0.f, 1.f}) * (20.f * part * dt);
        }
        b3Body_SetLinearVelocity(s.body, world->toM(vel));
        b3Body_SetAngularVelocity(s.body, b3v(spin));
    }
}

// The props' lift in water (beforeStep's) given again, for the step's pieces after the first: Box3D clears a body's
// forces after each step, so a slow frame's later pieces had the gravity without the lift (at a 20 Hz server's three
// pieces a frame, floating crates sank to the bottom).
void liftAgain()
{
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        const Slot& s = world->slots[num];
        if(s.lift != 0.f && s.kind == Kind::Prop && B3_IS_NON_NULL(s.body))
        {
            b3Body_ApplyForceToCenter(s.body, b3Vec3{0.f, 0.f, s.lift}, false);
        }
    }
}

// What a prop that falls asleep rests on (its groundentity): the body under it (a contact whose normal points up
// from the other to it), else the world.
[[nodiscard]] edict_t* supportOf(const Slot& s)
{
    za::Array<b3ContactData, 16> contacts;
    const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
    edict_t* best = qcvm->edicts;
    float bestUp = 0.7f;
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& c = contacts[i];
        const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), s.body);
        const b3ShapeId other = isA ? c.shapeIdB : c.shapeIdA;
        for(int m = 0; m < c.manifoldCount; m++)
        {
            if(c.manifolds[m].pointCount <= 0)
            {
                continue;
            }
            const float up = isA ? -c.manifolds[m].normal.z : c.manifolds[m].normal.z; // from the other to it
            if(up > bestUp)
            {
                bestUp = up;
                const int n = numOf(other);
                best = n > 0 && n < qcvm->num_edicts ? EDICT_NUM(n) : qcvm->edicts;
            }
        }
    }
    return best;
}

// A prop sliding after the step (the physics sounds' scrapes, vr_physsound.cpp): of its touching contacts with the level,
// a door, a fixture or another prop (not a body or a hand), the manifold it slides on hardest: its touching points'
// speed along the contact relative to the other body (rolling gives none: each point's own velocity, spin and all), and
// how hard it is pressed there (the step's normal impulse over its weight's: 1 lying on it, less grazing a wall).
// `dt`: the step's (the frame's last piece).
// `ignore`: the categories not slid on as well (a body's: other bodies and its own parts). False: no slide.
[[nodiscard]] bool bodySlide(b3BodyId body, float dt, uint64_t ignore, float& bestSlip, float& bestPress)
{
    za::Array<b3ContactData, 16> contacts;
    const int count = b3Body_GetContactData(body, contacts.data(), static_cast<int>(contacts.size()));
    if(count <= 0)
    {
        return false;
    }
    const glm::vec3 v = glmv(b3Body_GetLinearVelocity(body)), w = glmv(b3Body_GetAngularVelocity(body));
    const float mass = b3Body_GetMass(body);
    const float weightImpulse = mass * (world->gravity / world->m2u) * dt;
    bestSlip = bestPress = 0.f;
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& c = contacts[i];
        const bool isA = B3_ID_EQUALS(b3Shape_GetBody(c.shapeIdA), body);
        const b3ShapeId other = isA ? c.shapeIdB : c.shapeIdA;
        if(b3Shape_GetFilter(other).categoryBits & (catPlayer | catActor | catHand | catReach | ignore))
        {
            continue;
        }
        const b3BodyId ob = b3Shape_GetBody(other);
        const bool moves = b3Body_GetType(ob) != b3_staticBody;
        const glm::vec3 ov = moves ? glmv(b3Body_GetLinearVelocity(ob)) : glm::vec3{0.f};
        const glm::vec3 ow = moves ? glmv(b3Body_GetAngularVelocity(ob)) : glm::vec3{0.f};
        for(int m = 0; m < c.manifoldCount; m++)
        {
            const b3Manifold& mf = c.manifolds[m];
            const glm::vec3 n = glmv(mf.normal);
            float slip = 0.f, impulse = 0.f;
            int points = 0;
            for(int k = 0; k < mf.pointCount; k++)
            {
                const b3ManifoldPoint& p = mf.points[k];
                if(p.separation > 0.005f || p.totalNormalImpulse <= 0.f)
                {
                    continue; // (speculative: not touching, or not pressed)
                }
                const glm::vec3 ra = glmv(isA ? p.anchorA : p.anchorB), rb = glmv(isA ? p.anchorB : p.anchorA);
                const glm::vec3 rel = v + glm::cross(w, ra) - (ov + glm::cross(ow, rb));
                slip += glm::length(rel - n * glm::dot(rel, n));
                impulse += p.totalNormalImpulse;
                points++;
            }
            if(points == 0)
            {
                continue;
            }
            slip /= static_cast<float>(points);
            const float press = weightImpulse > 0.f ? impulse / weightImpulse : 0.f;
            if(slip * za::min(press, 1.f) > bestSlip * za::min(bestPress, 1.f))
            {
                bestSlip = slip;
                bestPress = press;
            }
        }
    }
    return bestSlip > 0.f;
}

void noteSlide(int num, const Slot& s, float dt)
{
    float slip = 0.f, press = 0.f;
    if(bodySlide(s.body, dt, 0, slip, press))
    {
        physsound::slide(num, s.sound, b3Body_GetMass(s.body), slip, press, s.origin);
    }
}

// A body's slide (AUDIO_REVIEW.md row 2, the drag): the ragdoll's part sliding hardest on the level, a door, a fixture
// or a prop (not on another body nor its own parts, a hand or a player), or a pushable corpse's, as flesh
// (physsound::slide's `body`: Flesh's soft scrapes, vr_physsound_bodies loud; its weight the sliding part's), so a body
// dragged by a limb, shoved along or sliding down a slope shuffles. Only its awake parts, and only while the body as a
// whole goes along the floor (its parts' mass-weighted level speed at least bodyDragSpeed): a ragdoll crumpling as it
// dies or settling, its limbs flopping, is silent (its knocks are heard: soundHits).
constexpr float bodyDragSpeed = 0.4f; // m/s

void noteBodySlide(int num, const Slot& s, float dt)
{
    float bestSlip = 0.f, bestPress = 0.f, bestMass = 0.f, massSum = 0.f;
    glm::vec3 at = s.origin, momentum{0.f};
    const auto consider = [&](b3BodyId body) {
        float slip = 0.f, press = 0.f;
        if(B3_IS_NULL(body) || !b3Body_IsValid(body) || !b3Body_IsAwake(body))
        {
            return;
        }
        const float m = b3Body_GetMass(body);
        const glm::vec3 v = glmv(b3Body_GetLinearVelocity(body));
        momentum += glm::vec3{v.x, v.y, 0.f} * m;
        massSum += m;
        if(!bodySlide(body, dt, catCorpse, slip, press))
        {
            return;
        }
        if(slip * za::min(press, 1.f) > bestSlip * za::min(bestPress, 1.f))
        {
            bestSlip = slip;
            bestPress = press;
            bestMass = b3Body_GetMass(body);
            at = world->toU(b3Body_GetPosition(body));
        }
    };
    if(s.ragdoll >= 0)
    {
        const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
        for(int b = 0; b < r.count; b++)
        {
            if(!partCut(r, b))
            {
                consider(r.body[static_cast<za::SizeT>(b)]);
            }
        }
    }
    else
    {
        consider(s.body);
    }
    if(bestSlip > 0.f && massSum > 0.f && glm::length(momentum) >= bodyDragSpeed * massSum)
    {
        physsound::slide(num, physsound::Material::Flesh, bestMass, bestSlip, bestPress, at, true);
    }
}

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
    SV_LinkEdict(ent, true); // (its triggers)
    if(!ent->free && origin != was)
    {
        SV_CheckWaterTransition(ent);
    }
    if(vr_debug_box3d.value >= 2 && !asleep)
    {
        Con_Printf("box3d: %d at %.1f %.1f %.1f, %.0f u/s (%.1f %.1f %.1f), spin %.1f rad/s\n", NUM_FOR_EDICT(ent), origin.x, origin.y,
            origin.z, glm::length(velocity), velocity.x, velocity.y, velocity.z, glm::length(spin));
    }
}

// The step's hits for the physics sounds (vr_physsound.cpp): a prop's against the level, a door, a fixture, another prop,
// a hand's or a held weapon's body (each prop of a pair its own); a ragdoll's parts' and a pushable corpse's, as flesh
// (each body one knock at a time: physsound::hit's `body`); not a monster's or a player's body (their touches have
// QC's sounds). Cheap: Box3D reports only the contacts that met faster than its hit threshold (1 m/s).
void soundHits(const b3ContactEvents& events)
{
    const float least = za::max(vr_physsound_min_speed.value, 1.f);
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(e.approachSpeed < least || !b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB))
        {
            continue;
        }
        for(int side = 0; side < 2; side++)
        {
            const b3ShapeId self = side ? e.shapeIdB : e.shapeIdA, other = side ? e.shapeIdA : e.shapeIdB;
            const int a = numOf(self), b = numOf(other);
            if(a <= 0 || a >= static_cast<int>(world->slots.size()) || a == b)
            {
                continue;
            }
            const Slot& s = world->slots[a];
            // A body: a ragdoll's part (its own mass: a torso's thud, a hand's squish) or a pushable corpse, as flesh.
            const bool body = s.kind == Kind::Corpse && (s.ragdoll >= 0 || s.corpseDynamic);
            if(s.kind != Kind::Prop && !body)
            {
                continue;
            }
            const Kind ok = b > 0 && b < static_cast<int>(world->slots.size()) ? world->slots[b].kind : Kind::None;
            if(ok == Kind::Actor || (b3Shape_GetFilter(other).categoryBits & catPlayer))
            {
                continue;
            }
            if(body && ok == Kind::Corpse && b < a && (world->slots[b].ragdoll >= 0 || world->slots[b].corpseDynamic))
            {
                continue; // (two bodies meeting: one knock, the lower-numbered one's, not one each)
            }
            const glm::vec3 at{static_cast<float>(e.point.x) * world->m2u, static_cast<float>(e.point.y) * world->m2u,
                static_cast<float>(e.point.z) * world->m2u};
            if(body)
            {
                physsound::hit(a, physsound::Material::Flesh, b3Body_GetMass(b3Shape_GetBody(self)), e.approachSpeed, at, true);
                continue;
            }
            physsound::hit(a, s.sound, b3Body_GetMass(s.body), e.approachSpeed, at);
        }
    }
}

// The step's touches, as the old solver's: a prop meeting another entity's body (a monster, a door) touches it
// (QC's damage, sounds); props hitting each other hard touch each other (a thrown box into a pile: its knock, the
// throw over). Landing on the world touches nothing (as before).
void touches(za::Vector<za::Pair<int, int>>& out)
{
    const b3ContactEvents events = b3World_GetContactEvents(world->id);
    const auto kindAt = [](int n) { return n > 0 && n < static_cast<int>(world->slots.size()) ? world->slots[n].kind : Kind::None; };
    for(int i = 0; i < events.beginCount; i++)
    {
        const b3ContactBeginTouchEvent& e = events.beginEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB))
        {
            continue;
        }
        int a = numOf(e.shapeIdA), b = numOf(e.shapeIdB);
        if(kindAt(a) != Kind::Prop)
        {
            za::genericSwap(a, b);
        }
        const Kind other = kindAt(b);
        if(kindAt(a) == Kind::Prop && (other == Kind::Mover || other == Kind::Actor))
        {
            out.emplaceBack(a, b);
        }
    }
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || e.approachSpeed * world->m2u < 60.f)
        {
            continue;
        }
        const int a = numOf(e.shapeIdA), b = numOf(e.shapeIdB);
        if(kindAt(a) == Kind::Prop && kindAt(b) == Kind::Prop)
        {
            out.emplaceBack(a, b);
        }
    }
    soundHits(events);
    // A ragdoll landing on the level (the world, a door, a fixture): its hardest hit this frame, along the vertical
    // (QC's VR_Monster_Fall judges it: a knocked-down monster alive takes fall damage; a dead one's ignored).
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB))
        {
            continue;
        }
        for(int side = 0; side < 2; side++)
        {
            const int a = numOf(side ? e.shapeIdB : e.shapeIdA), b = numOf(side ? e.shapeIdA : e.shapeIdB);
            const Kind level = kindAt(b);
            if(kindAt(a) != Kind::Corpse || world->slots[a].ragdoll < 0 ||
                (b != 0 && level != Kind::Mover && level != Kind::Fixture))
            {
                continue;
            }
            const float speed = e.approachSpeed * za::fabs(e.normal.z) * world->m2u;
            auto it = za::findIf(world->falls.begin(), world->falls.end(), [a](const Shock& s) { return s.num == a; });
            if(it == world->falls.end())
            {
                world->falls.pushBack({a, b, speed});
            }
            else if(speed > it->speed)
            {
                *it = {a, b, speed};
            }
        }
    }
    // A prop that wants to know how hard it hits (.vr_impact): its hardest hit on anything (the level too).
    const int impactField = fields().vr_impact;
    if(impactField < 0)
    {
        return;
    }
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || e.approachSpeed < 2.f)
        {
            continue;
        }
        for(int side = 0; side < 2; side++)
        {
            const int a = numOf(side ? e.shapeIdB : e.shapeIdA), b = numOf(side ? e.shapeIdA : e.shapeIdB);
            // (Not against a player's body or hands: a shove or a bump doesn't break it.)
            if(kindAt(a) != Kind::Prop || !fieldFunc(EDICT_NUM(a), impactField) || (b >= 1 && b <= svs.maxclients))
            {
                continue;
            }
            // Against a monster (a kinematic body following its steps: a walk's or a run's 8-20 units in one frame, a
            // body several metres a second fast at the step's start), only the prop's own share of the approach: a
            // monster walking into a crate pushes it, neither breaking it nor hurt by it (NOTES.md e1m1_2026-10-01
            // 02-45-57, 02-50-44); a crate thrown into a monster standing still hits it as hard as before.
            float speed = e.approachSpeed;
            if(kindAt(b) == Kind::Actor && b3Body_IsValid(world->slots[b].body))
            {
                // (The normal points from shape A to shape B; the monster's speed towards the prop.)
                const float along = b3Dot(b3Body_GetWorldPointVelocity(world->slots[b].body, e.point), e.normal);
                speed -= za::max(side ? along : -along, 0.f);
                if(speed < 2.f)
                {
                    continue;
                }
            }
            auto it = za::findIf(world->shocks.begin(), world->shocks.end(), [a](const Shock& s) { return s.num == a; });
            if(it == world->shocks.end())
            {
                world->shocks.pushBack({a, b, speed});
            }
            else if(speed > it->speed)
            {
                *it = {a, b, speed};
            }
        }
    }
}

// A live monster's fall (QC's VR_Monster_Fall: its fall damage) at `speed` units/s down.
void monsterFell(edict_t* ent, float speed)
{
    const func_t fn = qvr::progs::findFunction("VR_Monster_Fall");
    if(!fn || ent->free || ent->v.health <= 0.f || !(static_cast<int>(ent->v.flags) & FL_MONSTER))
    {
        return;
    }
    QcCallGuard guard;
    pr_global_struct->time = qcvm->time;
    G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
    G_FLOAT(OFS_PARM1) = speed;
    PR_ExecuteProgram(fn);
}

// The knocked-down monsters' ragdolls' landings this frame (touches), after the physics' loops.
void callFalls()
{
    for(const Shock& fall : world->falls)
    {
        if(fall.num < qcvm->num_edicts)
        {
            monsterFell(EDICT_NUM(fall.num), fall.speed);
        }
    }
    world->falls.clear();
}

// The props' hardest hits this frame, to their .vr_impact (QC: an explosive box hit hard enough takes damage).
void callShocks()
{
    const int impactField = fields().vr_impact;
    for(const Shock& shock : world->shocks)
    {
        if(shock.num >= qcvm->num_edicts || shock.other >= qcvm->num_edicts)
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(shock.num);
        edict_t* other = EDICT_NUM(shock.other);
        const func_t fn = ent->free || other->free ? 0 : fieldFunc(ent, impactField);
        if(!fn || world->slots[shock.num].kind != Kind::Prop)
        {
            continue;
        }
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d %s hit %d %s at %.1f m/s\n", shock.num, PR_GetString(ent->v.classname), shock.other,
                PR_GetString(other->v.classname), shock.speed);
        }
        const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
        pr_global_struct->self = EDICT_TO_PROG(ent);
        pr_global_struct->other = EDICT_TO_PROG(other);
        pr_global_struct->time = qcvm->time;
        G_FLOAT(OFS_PARM0) = shock.speed;
        PR_ExecuteProgram(fn);
        pr_global_struct->self = oldSelf;
        pr_global_struct->other = oldOther;
    }
    world->shocks.clear();
}

// Box3D's custom filter, for the shapes that ask for it (a live grenade's, addPropShapes): a grenade doesn't meet
// its thrower's body (its .owner: the ogre it leaves, or the player who threw it back), as Quake's missiles don't.
// Asked when two shapes' boxes first overlap: a grenade thrown back at the ogre that threw it meets it.
// Whether a reach body's shape meets prop `other`'s (asked by shouldCollide, once as their boxes begin to overlap): an
// empty hand no grenade (an open palm catches it, a fist knocks it away: vr_grenade.qc).
[[nodiscard]] bool reachMeets(b3ShapeId reach, b3ShapeId other)
{
    const int num = numOf(other);
    if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
    {
        return true;
    }
    const Slot& s = world->slots[num];
    return !(s.kind == Kind::Prop && (b3Shape_GetFilter(reach).categoryBits & catReachHand) && isGrenade(s.model));
}

bool shouldCollide(b3ShapeId a, b3ShapeId b, void*)
{
    if(((b3Shape_GetFilter(a).categoryBits & catHeld) && smallGibSpared(numOf(b))) ||
        ((b3Shape_GetFilter(b).categoryBits & catHeld) && smallGibSpared(numOf(a))))
    {
        return false; // (a held thing and a small gib just torn out: smallGibSpared)
    }
    const bool aReach = (b3Shape_GetFilter(a).categoryBits & catReach) != 0;
    if(aReach || (b3Shape_GetFilter(b).categoryBits & catReach) != 0)
    {
        return reachMeets(aReach ? a : b, aReach ? b : a);
    }
    const int na = numOf(a), nb = numOf(b);
    if(na > 0 && na == nb) { return false; }
    if(na <= 0 || nb <= 0 || na >= qcvm->num_edicts || nb >= qcvm->num_edicts)
    {
        return true;
    }
    if(!world->inside.empty() && bornInside(na, nb))
    {
        return false; // (a small gib made inside this body: noteBornInside)
    }
    if(!world->ragdollsInside.empty() && ragdollsInside(na, nb))
    {
        return false; // (ragdolls made in each other: createRagdoll)
    }
    // A corpse and a loose prop: as the settings say, by whether it flies from a throw (vr_corpse_collide_thrown) or not
    // (vr_corpse_collide_props).
    const Kind ka = na < static_cast<int>(world->slots.size()) ? world->slots[na].kind : Kind::None;
    const Kind kb = nb < static_cast<int>(world->slots.size()) ? world->slots[nb].kind : Kind::None;
    if((ka == Kind::Corpse && kb == Kind::Prop) || (kb == Kind::Corpse && ka == Kind::Prop))
    {
        const Slot& prop = world->slots[ka == Kind::Prop ? na : nb];
        return (prop.flight ? vr_corpse_collide_thrown.value : vr_corpse_collide_props.value) != 0.f;
    }
    const edict_t* ea = EDICT_NUM(na);
    const edict_t* eb = EDICT_NUM(nb);
    return ea->v.owner != EDICT_TO_PROG(eb) && eb->v.owner != EDICT_TO_PROG(ea);
}

// Standing on props (vr_box3d_player_stand), as Source's players stand on its VPhysics objects: Quake's movement
// is the player's (the world first: its traces are authoritative), and a solid prop (SOLID_BBOX: an explosive box) is
// ground to it like a brush (VR_StandsOn: FL_ONGROUND, so the player has friction and can jump); the player's capsule
// doesn't touch the prop under its feet (Box3D would have that kinematic body shove it, dragging it along by friction
// as the player walks on it: the prop slid away under him); instead the player's weight presses where he stands
// (vr_box3d_player_mass), the prop carries him as it moves (ride), and a jump pushes it away
// (vr_box3d_player_jump_push).
// See ROUND21.md, "Standing on props".
//
// Whether player capsule `player` stands on (or steps off) prop `other` at this contact: a solid prop met by the
// capsule's lower half-sphere, or below it. No contact then (preSolve).
[[nodiscard]] bool capsuleStandsOn(b3ShapeId player, b3ShapeId other, b3Pos point, b3Vec3 normal)
{
    const int num = numOf(other), client = numOf(player);
    if(!vr_box3d_player_stand.value || num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) ||
       world->slots[num].kind != Kind::Prop || client < 1 || client > svs.maxclients ||
       static_cast<int>(EDICT_NUM(num)->v.solid) != SOLID_BBOX)
    {
        return false;
    }
    const Slot& ps = world->slots[static_cast<size_t>(client)];
    const b3BodyId body = b3Shape_GetBody(player);
    const b3Vec3 centre = b3Body_GetPosition(body);
    const float feet = centre.z + ps.mins.z / world->m2u; // m
    glm::vec3 n = glmv(normal); // turned to point from the prop to the player
    if(glm::dot(n, glmv(centre) - glmv(b3Body_GetWorldCenter(b3Shape_GetBody(other)))) < 0.f)
    {
        n = -n;
    }
    return point.z < feet + ps.radius || n.z > 0.5f;
}

// A contact's normal turned to point from a hand's body to the prop (towards the prop's centre: Box3D's contacts and its
// continuous collision give it either way round).
[[nodiscard]] glm::vec3 towardsProp(b3BodyId prop, b3Pos point, b3Vec3 normal)
{
    const glm::vec3 n = glmv(normal);
    return glm::dot(n, glmv(b3Body_GetWorldCenter(prop)) - glmv(point)) < 0.f ? -n : n;
}

// Whether a hand's body may hold `prop` up where it meets it (normal `n`, from the body to the prop): not a prop heavier
// than vr_box3d_hand_hold_mass on top of it (the normal within 60 degrees of up), which slips through it.
[[nodiscard]] bool mayHoldUp(b3BodyId prop, const glm::vec3& n)
{
    const float most = vr_box3d_hand_hold_mass.value;
    return most <= 0.f || n.z < 0.5f || b3Body_GetMass(prop) <= most;
}

// Box3D's pre-solve, for the fists' contacts and the reach bodies' (their shapes ask for it), each step: none with what
// their player just threw (graced) or what the reach body skips (reachSkips), nor holding up what is heavier than
// vr_box3d_hand_hold_mass (the prop on top: the normal from the hand's body within 60 degrees of up); pushed from the
// side or below, it still is. (A fist that held up the big box toppled onto its player, its drawn fist dozens of
// kinematic spheres, wedged it into the wall: Box3D threw it off at 10 to 15 m/s. ROUND21.md, "Props regressions".)
bool preSolve(b3ShapeId a, b3ShapeId b, b3Pos point, b3Vec3 normal, void*)
{
    // The level's mesh asks too, point by point (a local change to Box3D's mesh contacts: a prop passing through a
    // slipgate must not meet the wall behind it): only the portal copies' clip planes concern it.
    const bool level = B3_ID_EQUALS(a, world->worldShape) || B3_ID_EQUALS(b, world->worldShape);
    if(level && world->portalCopies.empty()) { return true; }
    for(const World::PortalCopy& copy : world->portalCopies)
    {
        for(const b3ShapeId shape : {a, b})
        {
            const b3BodyId body = b3Shape_GetBody(shape);
            const glm::vec4* clip = B3_ID_EQUALS(body, copy.original) ? &copy.here : B3_ID_EQUALS(body, copy.copy) ? &copy.there : nullptr;
            if(clip && glm::dot(*clip, glm::vec4{world->toU(point), 1.f}) < -0.03125f) { return false; }
        }
    }
    if(level) { return true; }

    const bool aPlayer = (b3Shape_GetFilter(a).categoryBits & catPlayer) != 0;
    if(aPlayer || (b3Shape_GetFilter(b).categoryBits & catPlayer) != 0)
    {
        const b3ShapeId capsule = aPlayer ? a : b, other = aPlayer ? b : a;
        if(vr_box3d_throw_grace_body.value && graced(numOf(capsule), numOf(other)))
        {
            return false; // (what its player just threw: noteThrows)
        }
        return !capsuleStandsOn(capsule, other, point, normal);
    }
    const bool aFist = (b3Shape_GetFilter(a).categoryBits & catHand) != 0;
    if(aFist || (b3Shape_GetFilter(b).categoryBits & catHand) != 0)
    {
        const b3ShapeId fist = aFist ? a : b, prop = aFist ? b : a;
        if(graced(numOf(fist), numOf(prop))) // (noteThrows)
        {
            return false;
        }
        const b3BodyId propBody = b3Shape_GetBody(prop);
        return mayHoldUp(propBody, towardsProp(propBody, point, normal));
    }
    const bool aReach = (b3Shape_GetFilter(a).categoryBits & catReach) != 0;
    const b3ShapeId reach = aReach ? a : b, other = aReach ? b : a;
    if((b3Shape_GetFilter(reach).categoryBits & catReach) == 0)
    {
        return true;
    }
    const World::HandBody* hb = reachOf(b3Shape_GetBody(reach), numOf(reach));
    if(hb && reachSkips(*hb, numOf(reach), numOf(other)))
    {
        return false;
    }
    const b3BodyId reachBody = b3Shape_GetBody(reach), propBody = b3Shape_GetBody(other);
    const glm::vec3 n = towardsProp(propBody, point, normal);
    // Moving apart faster than 0.5 m/s (a prop just struck, flying off ahead of the body: sweepReach): no contact, which
    // Box3D's continuous collision would otherwise make of the body it leaves (stopping the prop there).
    const glm::vec3 relative =
        glmv(b3Body_GetWorldPointVelocity(propBody, point)) - glmv(b3Body_GetWorldPointVelocity(reachBody, point));
    const float apart = glm::dot(relative, n);
    if(vr_debug_box3d.value >= 2.f)
    {
        Con_Printf("box3d: reach contact with %d: normal %.2f %.2f %.2f, apart at %.2f m/s\n", numOf(other), n.x, n.y, n.z,
            apart);
    }
    if(apart > 0.5f)
    {
        return false;
    }
    return mayHoldUp(propBody, n);
}

// The crossing half has a body in the other room. Both shapes are clipped
// by their contact points, so the wall behind the aperture cannot catch it.
// Dynamic copies return their contact impulses, rotated, to their owner.
// A ragdoll's parts are copied together, through the gate its parts' box goes into (a corpse falling or thrown into a
// gate: its limbs past the plane meet the far room, not the wall behind the gate), until its pelvis crosses and it is
// carried whole (carryRagdolls).
void syncPortalCopies(float dt)
{
    for(const World::PortalCopy& c : world->portalCopies)
    {
        if(b3Body_IsValid(c.copy)) { b3DestroyBody(c.copy); }
    }
    world->portalCopies.clear();
    // Its box over this step too (where its motion takes it: a thrown prop goes from short of the gate to the wall
    // behind it in one step, and continuous collision met that wall before any copy was made: it bounced off), with
    // a few units' slack for a tumbling box's corners round the aperture.
    const auto sweptBox = [dt](b3BodyId body, glm::vec3& lo, glm::vec3& hi) {
        const b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 step = glmv(b3Body_GetLinearVelocity(body)) * (1.5f * dt);
        lo = glm::min(lo, glm::min(glmv(box.lowerBound), glmv(box.lowerBound) + step));
        hi = glm::max(hi, glm::max(glmv(box.upperBound), glmv(box.upperBound) + step));
    };
    const auto addThrough = [&](b3BodyId body, const portals::LightGate& gate) {
        World::PortalCopy c;
        c.original = body;
        c.turn = gate.turn;
        c.shift = gate.to - gate.turn * gate.from;
        c.here = glm::vec4{gate.normal, -gate.dist};
        const glm::vec3 normal = -gate.turn * gate.normal;
        c.there = glm::vec4{normal, -glm::dot(normal, gate.to)};
        c.dynamic = b3Body_GetType(body) == b3_dynamicBody;
        b3BodyDef def = b3DefaultBodyDef();
        def.type = c.dynamic ? b3_dynamicBody : b3_kinematicBody;
        def.gravityScale = 0.f;
        const b3WorldTransform pose = b3Body_GetTransform(body);
        def.position = world->toM(gate.turn * world->toU(pose.p) + c.shift);
        def.rotation = toB3(glm::normalize(glm::quat_cast(gate.turn) * fromB3(pose.q)));
        def.userData = b3Body_GetUserData(body);
        c.copy = b3CreateBody(world->id, &def);
        za::Array<b3ShapeId, 16> shapes;
        const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            const b3ShapeId original = shapes[i];
            b3ShapeDef shape = b3DefaultShapeDef();
            shape.userData = b3Shape_GetUserData(original);
            shape.filter = b3Shape_GetFilter(original);
            shape.density = b3Shape_GetDensity(original);
            shape.baseMaterial.friction = b3Shape_GetFriction(original);
            shape.baseMaterial.restitution = b3Shape_GetRestitution(original);
            shape.enablePreSolveEvents = true;
            shape.enableCustomFiltering = true;
            b3Shape_EnablePreSolveEvents(original, true);
            switch(b3Shape_GetType(original))
            {
            case b3_hullShape: b3CreateHullShape(c.copy, &shape, b3Shape_GetHull(original)); break;
            case b3_sphereShape: { const b3Sphere v = b3Shape_GetSphere(original); b3CreateSphereShape(c.copy, &shape, &v); break; }
            case b3_capsuleShape: { const b3Capsule v = b3Shape_GetCapsule(original); b3CreateCapsuleShape(c.copy, &shape, &v); break; }
            default: break;
            }
        }
        world->portalCopies.pushBack(c);
    };
    const auto add = [&](b3BodyId body) {
        if(B3_IS_NULL(body) || !b3Body_IsValid(body)) { return; }
        glm::vec3 lo{1e30f}, hi{-1e30f};
        sweptBox(body, lo, hi);
        portals::LightGate gate;
        const glm::vec3 velocity = world->toU(b3Body_GetLinearVelocity(body));
        if(!portals::splitBounds(world->toU(b3v(lo)), world->toU(b3v(hi)), gate, 4.f, &velocity)) { return; }
        addThrough(body, gate);
    };
    for(const Slot& slot : world->slots)
    {
        // (A dead monster's body (vr_corpse_collide) too; a ragdoll's parts below.)
        if(slot.kind == Kind::Held || slot.kind == Kind::Prop || (slot.kind == Kind::Corpse && slot.ragdoll < 0))
        {
            add(slot.body);
        }
    }
    // A ragdoll: the gate one of its parts goes into (its box across the plane, over the aperture), and a copy through it
    // of each of its parts over that aperture, in front of the plane or past it (one against the frame beside it has
    // none: the frame stops it, as it would a prop).
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num == 0 || r.count == 0 || B3_IS_NULL(r.body[0]) || !b3Body_IsValid(r.body[0])) { continue; }
        // A part's box over the step, in units, a few units in (lying on the floor its box reaches a little into it,
        // under a gate's bottom edge).
        const auto partBox = [&](int b, glm::vec3& ulo, glm::vec3& uhi) {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || B3_IS_NULL(body) || !b3Body_IsValid(body)) { return false; }
            glm::vec3 lo{1e30f}, hi{-1e30f};
            sweptBox(body, lo, hi);
            ulo = world->toU(b3v(lo));
            uhi = world->toU(b3v(hi));
            const glm::vec3 inset = glm::min(glm::vec3{4.f}, (uhi - ulo) * 0.25f);
            ulo += inset;
            uhi -= inset;
            return true;
        };
        portals::LightGate gate;
        const glm::vec3 velocity = world->toU(b3Body_GetLinearVelocity(r.body[0]));
        bool found = false;
        for(int b = 0; b < r.count && !found; b++)
        {
            glm::vec3 ulo, uhi;
            found = partBox(b, ulo, uhi) && portals::splitBounds(ulo, uhi, gate, 4.f, &velocity);
        }
        if(!found) { continue; }
        for(int b = 0; b < r.count; b++)
        {
            glm::vec3 ulo, uhi;
            if(!partBox(b, ulo, uhi)) { continue; }
            bool over = true;
            for(int c = 0; c < 8 && over; c++)
            {
                const glm::vec3 p{(c & 1) ? uhi.x : ulo.x, (c & 2) ? uhi.y : ulo.y, (c & 4) ? uhi.z : ulo.z};
                const glm::vec3 onPlane = p - gate.normal * (glm::dot(gate.normal, p) - gate.dist);
                over = !glm::any(glm::lessThan(onPlane, gate.mins - 4.f)) && !glm::any(glm::greaterThan(onPlane, gate.maxs + 4.f));
            }
            if(over) { addThrough(r.body[static_cast<za::SizeT>(b)], gate); }
        }
    }
    for(const auto& pair : world->hands)
    {
        for(const World::HandBody& hand : pair) { add(hand.body); add(hand.reach); }
    }
}

// After the step: a ragdoll whose pelvis went in through a slipgate's aperture is carried whole, every part by the gate's
// mapping (where, how turned, how it moves), as VR_PortalToss carries what flies: its joints stay as they were, and its
// parts still behind the plane meet the room they are in through their portal copies (syncPortalCopies).
void carryRagdolls()
{
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num == 0 || r.count == 0 || B3_IS_NULL(r.body[0]) || !b3Body_IsValid(r.body[0]))
        {
            r.pelvisKnown = false;
            continue;
        }
        glm::vec3 pelvis = world->toU(b3Body_GetTransform(r.body[0]).p);
        portals::LightGate gate;
        if(r.pelvisKnown && portals::crossedGate(r.lastPelvis, pelvis, gate))
        {
            const glm::vec3 shift = gate.to - gate.turn * gate.from;
            const glm::quat q = glm::quat_cast(gate.turn);
            for(int b = 0; b < r.count; b++)
            {
                const za::SizeT i = static_cast<za::SizeT>(b);
                const b3BodyId body = r.body[i];
                if(partCut(r, b) || B3_IS_NULL(body) || !b3Body_IsValid(body)) { continue; }
                const b3WorldTransform pose = b3Body_GetTransform(body);
                b3Body_SetTransform(body, world->toM(gate.turn * world->toU(pose.p) + shift), toB3(glm::normalize(q * fromB3(pose.q))));
                b3Body_SetLinearVelocity(body, b3v(gate.turn * glmv(b3Body_GetLinearVelocity(body))));
                b3Body_SetAngularVelocity(body, b3v(gate.turn * glmv(b3Body_GetAngularVelocity(body))));
                r.ownLin[i] = gate.turn * r.ownLin[i];
                r.ownAng[i] = gate.turn * r.ownAng[i];
            }
            r.turn = glm::normalize(q * r.turn);
            r.headMid = gate.turn * r.headMid + shift;
            r.headVel = gate.turn * r.headVel;
            r.headSpin = gate.turn * r.headSpin;
            r.headRot = glm::normalize(q * r.headRot);
            edict_t* ent = r.num > 0 && r.num < qcvm->num_edicts ? EDICT_NUM(r.num) : nullptr;
            if(ent && !ent->free)
            {
                ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + glm::degrees(std::atan2(gate.turn[0][1], gate.turn[0][0])));
            }
            const glm::vec3 to = gate.turn * pelvis + shift;
            Con_DPrintf("ragdoll: %d carried through a slipgate: pelvis %.1f %.1f %.1f -> %.1f %.1f %.1f\n", r.num, pelvis.x,
                pelvis.y, pelvis.z, to.x, to.y, to.z);
            pelvis = to;
        }
        r.lastPelvis = pelvis;
        r.pelvisKnown = true;
    }
}

// After the step: a dead monster's own body (vr_corpse_collide's pushable kinds, not a ragdoll) whose middle went in
// through a slipgate's aperture is carried whole by the gate's mapping, as carryRagdolls carries a ragdoll; until then
// its portal copy kept the wall behind the gate from stopping it (syncPortalCopies). writeCorpse then moves its entity.
void carryCorpses()
{
    for(za::SizeT num = 0; num < world->slots.size(); num++)
    {
        Slot& s = world->slots[num];
        if(s.kind != Kind::Corpse || s.ragdoll >= 0 || !s.corpseDynamic || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
        {
            s.midKnown = false;
            continue;
        }
        glm::vec3 mid = world->toU(b3Body_GetWorldCenter(s.body));
        portals::LightGate gate;
        if(s.midKnown && portals::crossedGate(s.lastMid, mid, gate))
        {
            const glm::vec3 shift = gate.to - gate.turn * gate.from;
            const glm::quat q = glm::quat_cast(gate.turn);
            const b3WorldTransform pose = b3Body_GetTransform(s.body);
            // (A box, not fitted to its pose, stays square to the world: rotationOf; its yaw turns below.)
            const glm::quat turned = s.corpseFitted ? glm::normalize(q * fromB3(pose.q)) : fromB3(pose.q);
            b3Body_SetTransform(s.body, world->toM(gate.turn * world->toU(pose.p) + shift), toB3(turned));
            b3Body_SetLinearVelocity(s.body, b3v(gate.turn * glmv(b3Body_GetLinearVelocity(s.body))));
            b3Body_SetAngularVelocity(s.body, b3v(gate.turn * glmv(b3Body_GetAngularVelocity(s.body))));
            b3Body_SetAwake(s.body, true);
            edict_t* ent = static_cast<int>(num) < qcvm->num_edicts ? EDICT_NUM(static_cast<int>(num)) : nullptr;
            if(ent && !ent->free && !s.corpseFitted)
            {
                ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + glm::degrees(std::atan2(gate.turn[0][1], gate.turn[0][0])));
            }
            s.asleep = false; // (written this frame: writeCorpse)
            const glm::vec3 to = gate.turn * mid + shift;
            Con_DPrintf("corpse: %d carried through a slipgate: middle %.1f %.1f %.1f -> %.1f %.1f %.1f\n", static_cast<int>(num),
                mid.x, mid.y, mid.z, to.x, to.y, to.z);
            mid = to;
        }
        s.lastMid = mid;
        s.midKnown = true;
    }
}

void feedPortalCopies()
{
    for(World::PortalCopy& c : world->portalCopies)
    {
        if(!b3Body_IsValid(c.original)) { continue; }
        const b3WorldTransform pose = b3Body_GetTransform(c.original);
        b3Body_SetTransform(c.copy, world->toM(c.turn * world->toU(pose.p) + c.shift),
            toB3(glm::normalize(glm::quat_cast(c.turn) * fromB3(pose.q))));
        c.velocity = c.turn * glmv(b3Body_GetLinearVelocity(c.original));
        c.spin = c.turn * glmv(b3Body_GetAngularVelocity(c.original));
        b3Body_SetLinearVelocity(c.copy, b3v(c.velocity));
        b3Body_SetAngularVelocity(c.copy, b3v(c.spin));
    }
}

void finishPortalCopies()
{
    for(const World::PortalCopy& c : world->portalCopies)
    {
        if(!c.dynamic || !b3Body_IsValid(c.original)) { continue; }
        const glm::mat3 back = glm::transpose(c.turn);
        b3Body_SetLinearVelocity(c.original, b3v(glmv(b3Body_GetLinearVelocity(c.original)) +
            back * (glmv(b3Body_GetLinearVelocity(c.copy)) - c.velocity)));
        b3Body_SetAngularVelocity(c.original, b3v(glmv(b3Body_GetAngularVelocity(c.original)) +
            back * (glmv(b3Body_GetAngularVelocity(c.copy)) - c.spin)));
    }
}

// Pushes by mass (pushShare): the mass behind a hand's body `shape` (kg), 0 for none (a kinematic body's full push: not
// a hand's, or the setting 0). A carried prop: its own mass and each hand's holding it.
[[nodiscard]] float pusherMass(b3ShapeId shape)
{
    const uint64_t category = b3Shape_GetFilter(shape).categoryBits;
    const float hand = za::max(vr_box3d_hand_mass.value, 0.f);
    if(category & catReach)
    {
        const World::HandBody* hb = reachOf(b3Shape_GetBody(shape), numOf(shape));
        return hb ? reachMass(*hb) : 0.f;
    }
    if(category & catHand)
    {
        return hand;
    }
    if((category & catHeld) && hand > 0.f)
    {
        const int num = numOf(shape);
        int hands = 0;
        for(const auto& player : world->hands)
        {
            for(const World::HandBody& hb : player)
            {
                hands += hb.held == num ? 1 : 0;
            }
        }
        return hand * static_cast<float>(za::max(hands, 1)) + qvr::box3d::propMass(EDICT_NUM(num));
    }
    return 0.f;
}

// What the hands' bodies push by mass (notePushed, limitPushes): a loose prop, a pushable corpse.
[[nodiscard]] bool pushedByMass(const Slot& s)
{
    return s.kind == Kind::Prop || (s.kind == Kind::Corpse && s.corpseDynamic);
}

// Before a step: the props near the hands' bodies (the reach bodies, the fists, the carried props) and their motion.
void notePushed(float dt)
{
    world->pushed.clear();
    if(vr_box3d_hand_mass.value <= 0.f && vr_box3d_weapon_arm_mass.value <= 0.f)
    {
        return;
    }
    const auto gather = [dt](b3BodyId body) {
        if(B3_IS_NULL(body) || !b3Body_IsValid(body))
        {
            return;
        }
        // Its box, grown by how far it goes this step (and 5 cm).
        b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 lo = glmv(box.lowerBound), hi = glmv(box.upperBound);
        const float reach = 0.05f + (glm::length(glmv(b3Body_GetLinearVelocity(body))) +
                                        glm::length(glmv(b3Body_GetAngularVelocity(body))) * 0.5f * glm::length(hi - lo)) *
                                        dt;
        box.lowerBound = b3v(lo - glm::vec3{reach});
        box.upperBound = b3v(hi + glm::vec3{reach});
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.maskBits = catProp | catCorpse;
        b3World_OverlapAABB(world->id, box, filter,
            [](b3ShapeId shape, void*) {
                const int num = numOf(shape);
                if(num <= 0 || num >= static_cast<int>(world->slots.size()) || !pushedByMass(world->slots[num]))
                {
                    return true;
                }
                auto& out = world->pushed;
                if(!za::anyOf(out.begin(), out.end(), [num](const Pushed& p) { return p.num == num; }))
                {
                    const b3BodyId prop = world->slots[num].body;
                    out.pushBack({num, glmv(b3Body_GetLinearVelocity(prop)), glmv(b3Body_GetAngularVelocity(prop))});
                }
                return true;
            },
            nullptr);
    };
    for(const auto& player : world->hands)
    {
        for(const World::HandBody& hb : player)
        {
            gather(hb.reach);
            gather(hb.body);
        }
    }
    for(const Slot& s : world->slots)
    {
        if(s.kind == Kind::Held)
        {
            gather(s.body);
        }
    }
}

// After a step: what a hand's body pushed (a contact that pushed, the body moving into it at 0.3 m/s or more) keeps
// only pushShare of the velocity (and of the spin) the step gave it, if away from the body: the masses of all the
// hands' bodies pushing it together against its own; pushed on, as a shove (pushKeep). A hand held still holds things up
// and stops them at full strength.
void limitPushes(float dt)
{
    za::Vector<b3ContactData>& contacts = scratch.pushContacts;
    for(const Pushed& p : world->pushed)
    {
        const Slot& s = world->slots[static_cast<size_t>(p.num)];
        if(!pushedByMass(s) || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const float m = b3Body_GetMass(s.body);
        // Every contact, not the first few: a drawn fist (vr_box3d_hand_push_fist) is a body of a sphere per joint, and
        // pressed on a box its spheres touch it in dozens of places, the ones that push often not among the first 16 (it
        // then pushed at full strength, as a kinematic body: walked into, a tall box tipped; landing on you, it flew off).
        contacts.resize(static_cast<za::SizeT>(za::max(b3Body_GetContactCapacity(s.body), 1)));
        const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        float pusher = 0.f, strongest = 0.f;
        bool full = false;
        glm::vec3 normal{0.f};
        za::Array<b3BodyId, 8> counted;
        size_t countedN = 0;
        for(int c = 0; c < count && !full; c++)
        {
            const b3ContactData& cd = contacts[static_cast<size_t>(c)];
            const bool propA = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), s.body);
            const b3ShapeId other = propA ? cd.shapeIdB : cd.shapeIdA;
            if((b3Shape_GetFilter(other).categoryBits & (catReach | catHand | catHeld)) == 0)
            {
                continue;
            }
            const b3BodyId body = b3Shape_GetBody(other);
            for(int i = 0; i < cd.manifoldCount; i++)
            {
                const b3Manifold& mf = cd.manifolds[i];
                float impulse = 0.f;
                glm::vec3 at{0.f};
                for(int k = 0; k < mf.pointCount; k++)
                {
                    const b3ManifoldPoint& mp = mf.points[k];
                    impulse += mp.totalNormalImpulse;
                    at += glmv(propA ? mp.anchorA : mp.anchorB);
                }
                if(impulse <= 0.f || mf.pointCount == 0)
                {
                    continue;
                }
                // The normal from the body to the prop (Box3D's points from A to B), and the body's motion into it there.
                const glm::vec3 n = propA ? -glmv(mf.normal) : glmv(mf.normal);
                const glm::vec3 point = glmv(b3Body_GetWorldCenter(s.body)) + at / static_cast<float>(mf.pointCount);
                if(glm::dot(glmv(b3Body_GetWorldPointVelocity(body, b3v(point))), n) < 0.3f)
                {
                    continue;
                }
                const float mass = pusherMass(other);
                if(mass <= 0.f)
                {
                    full = true;
                    break;
                }
                if(!za::anyOf(counted.begin(), counted.begin() + static_cast<za::PtrDiffT>(countedN),
                       [body](b3BodyId b) { return B3_ID_EQUALS(b, body); }))
                {
                    pusher += mass;
                    if(countedN < counted.size())
                    {
                        counted[countedN++] = body;
                    }
                }
                if(impulse > strongest)
                {
                    strongest = impulse;
                    normal = n;
                }
            }
        }
        if(full || pusher <= 0.f)
        {
            continue;
        }
        Slot& pushed = world->slots[static_cast<size_t>(p.num)];
        const bool shove = pushed.pushedStep >= world->steps - 1;
        const glm::vec3 v = glmv(b3Body_GetLinearVelocity(s.body)), w = glmv(b3Body_GetAngularVelocity(s.body));
        const float gained = glm::dot(v - p.velocity, normal);
        const float keep = pushKeep(pushed, pusher, m, glm::length(v - p.velocity), dt);
        if(gained <= 0.f || keep >= 1.f)
        {
            continue;
        }
        const glm::vec3 now = p.velocity + (v - p.velocity) * keep;
        b3Body_SetLinearVelocity(s.body, b3v(now));
        b3Body_SetAngularVelocity(s.body, b3v(p.spin + (w - p.spin) * keep));
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d %s (%.1f kg) %s by %.1f kg: %.0f u/s gained, x%.2f: %.0f u/s\n", p.num,
                PR_GetString(EDICT_NUM(p.num)->v.classname), m, shove ? "shoved" : "hit", pusher, gained * world->m2u, keep,
                glm::length(now) * world->m2u);
        }
    }
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
    ragdoll::unpublishAll();
    if(b3World_IsValid(world->id))
    {
        b3DestroyWorld(world->id);
    }
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
    for(auto& [model, hulls] : world->moverHulls)
    {
        for(b3HullData* hull : hulls)
        {
            b3DestroyHull(hull);
        }
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
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && (!strcmp(which, "props") ? isRigid(e) && modelOf(e) : !strcmp(PR_GetString(e->v.classname), which)))
        {
            out.pushBack(e);
        }
    }
    return out;
}

struct VmScope
{
    qcvm_t* old{nullptr};
    VmScope() { PR_PushQCVM(&sv.qcvm, &old); }
    ~VmScope() { PR_PopQCVM(old); }
};

// vr_physics_loose <number | classname>: made a loose physics object, as a hand's knock makes it (QC's
// VR_Carry_Loose: a toss, rigid, touchable), where it is: a hanging armour, a pickup. For tests.
void loose_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_physics_loose <number | classname>\n");
        return;
    }
    const VmScope vm;
    for(edict_t* e : entitiesNamed(Cmd_Argv(1)))
    {
        e->v.movetype = MOVETYPE_TOSS;
        e->v.solid = SOLID_NOT_BUT_TOUCHABLE;
        setFieldFloat(e, fields().vr_rigid, 1.f);
        setFlag(e, FL_ONGROUND, false);
        SV_LinkEdict(e, false);
        Con_Printf("vr_physics_loose: %d %s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname));
    }
}

void placeStill(edict_t* e, const glm::vec3& at, float yaw)
{
    const FieldOffsets& f = fields();
    store(at, e->v.origin);
    e->v.angles[0] = 0.f;
    e->v.angles[1] = yaw;
    e->v.angles[2] = 0.f;
    VectorCopy(vec3_origin, e->v.velocity);
    VectorCopy(vec3_origin, e->v.avelocity);
    setFieldVec(e, f.vr_spin, glm::vec3{0.f});
    setFlag(e, FL_ONGROUND, false);
    SV_LinkEdict(e, false);
}

// vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]: the first `count` of them in a
// column from x y z (their bottoms `gap` units apart, 0.5 by default), still, level, awake.
void stack_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]\n");
        return;
    }
    const VmScope vm;
    za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t count = za::min(list.size(), static_cast<size_t>(za::max(0, Q_atoi(Cmd_Argv(2)))));
    glm::vec3 at{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float yaw = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 0.f;
    const float gap = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) : 0.5f;
    for(size_t i = 0; i < count; i++)
    {
        edict_t* e = list[i];
        glm::vec3 lo, hi;
        localBox(e, modelOf(e), lo, hi);
        placeStill(e, at - glm::vec3{0.f, 0.f, lo.z}, yaw);
        Con_Printf("vr_physics_stack: %d %s at %.1f %.1f %.1f\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname), e->v.origin[0],
            e->v.origin[1], e->v.origin[2]);
        at.z += hi.z - lo.z + gap;
    }
}

// vr_physics_pyramid <number | classname | props> <rows> <x> <y> <z> [<yaw>]: a pyramid along the yaw's side, `rows`
// at the bottom (rows (rows + 1) / 2 of them), its middle at x y, standing on z.
void pyramid_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_pyramid <number | classname | props> <rows> <x> <y> <z> [<yaw>]\n");
        return;
    }
    const VmScope vm;
    za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const int rows = za::max(1, Q_atoi(Cmd_Argv(2)));
    const glm::vec3 centre{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float yaw = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 0.f;
    const glm::vec3 side{za::cos(glm::radians(yaw + 90.f)), za::sin(glm::radians(yaw + 90.f)), 0.f};
    size_t next = 0;
    float z = centre.z;
    for(int row = 0; row < rows && next < list.size(); row++)
    {
        const int n = rows - row;
        float height = 0.f;
        for(int k = 0; k < n && next < list.size(); k++)
        {
            edict_t* e = list[next++];
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            const float width = (hi.y - lo.y) + 1.f;
            const glm::vec3 at = centre + side * ((static_cast<float>(k) - (n - 1) * 0.5f) * width);
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.25f}, yaw);
            height = za::max(height, hi.z - lo.z);
            Con_Printf("vr_physics_pyramid: %d %s at %.1f %.1f %.1f\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname), e->v.origin[0],
                e->v.origin[1], e->v.origin[2]);
        }
        z += height + 0.5f;
    }
}

// vr_physics_pile <number | classname | props> <per column> <x> <y> <z> [<spacing>]: all of them in columns of
// `per column` (as vr_physics_stack), side by side along x, `spacing` units apart (24): towers that fall into a pile.
void pile_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_pile <number | classname | props> <per column> <x> <y> <z> [<spacing>]\n");
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t per = static_cast<size_t>(za::max(1, Q_atoi(Cmd_Argv(2))));
    const glm::vec3 base{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float spacing = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 24.f;
    glm::vec3 at = base;
    for(size_t i = 0; i < list.size(); i++)
    {
        if(i % per == 0)
        {
            at = base + glm::vec3{spacing * static_cast<float>(i / per), 0.f, 0.f};
        }
        edict_t* e = list[i];
        glm::vec3 lo, hi;
        localBox(e, modelOf(e), lo, hi);
        placeStill(e, at - glm::vec3{0.f, 0.f, lo.z}, static_cast<float>((i * 37) % 90));
        at.z += hi.z - lo.z + 0.5f;
    }
    Con_Printf("vr_physics_pile: %d in %d columns\n", static_cast<int>(list.size()), static_cast<int>((list.size() + per - 1) / per));
}

// The rocks and bricks a big pile is made of (vr_debris.qc's models, precached on every map): rocks, then bricks.
constexpr const char* pileModels[] = {"progs/vr_rock1.mdl", "progs/vr_rock2.mdl", "progs/vr_rock3.mdl", "progs/vr_rock4.mdl",
    "progs/vr_rock5.mdl", "progs/vr_brick1.mdl", "progs/vr_brick2.mdl", "progs/vr_brick3.mdl", "progs/vr_brick4.mdl"};
constexpr int pileRocks = 5;
constexpr int pileBricks = 4;

// What the physics stress tests spawn carries this bit in its spawnflags (no map sets it; the spawn functions read only
// their low bits), so that vr_physics_clearpiles takes away theirs and nothing the map placed. 2^20: exact as a float.
constexpr int stressTagBit = 1 << 20;

enum class PileKind
{
    Debris, // rocks and bricks in turn
    Rocks,
    Bricks,
    Crates, // walls of small crates
    Mixed,  // rocks and bricks, every fourth column small crates
};

struct PileKindName
{
    const char* name;
    PileKind kind;
};

constexpr PileKindName pileKindNames[] = {{"debris", PileKind::Debris}, {"rocks", PileKind::Rocks},
    {"bricks", PileKind::Bricks}, {"crates", PileKind::Crates}, {"mixed", PileKind::Mixed}};

// One prop of the stress tests spawned at `at` by its spawn function `fn` (`model` given to it if not null), tagged
// (stressTagBit). Null if it removed itself.
edict_t* spawnStressProp(func_t fn, const char* classname, const char* model, const glm::vec3& at)
{
    edict_t* e = ED_Alloc();
    store(at, e->v.origin);
    e->v.classname = PR_SetEngineString(classname);
    if(model)
    {
        e->v.model = PR_SetEngineString(model);
    }
    e->v.spawnflags = static_cast<float>(stressTagBit);
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    PR_ExecuteProgram(fn); // rests it on the floor below (vr_debris_piece, vr_crate)
    return e->free ? nullptr : e;
}

// vr_physics_bigpile [<count>] [<distance>] [debris | rocks | bricks | crates | mixed]: a big pile of props for Box3D on
// the pool (NOTES.md vrfiringrange_2026-10-01_16-36-40, _22-46-00; Debug > Tests > Physics Stress). `count` props
// (vr_test_pile_count, 500, or for crates vr_test_pile_crates, 80; at most 2000) `distance` units (96) ahead of the
// first player:
// - debris (the default), rocks, bricks, mixed: in columns of 10 (small crates: 3) on a square grid 20 units apart (36
//   with crates), each prop set a little further along than the one under it, so that the columns topple into one pile:
//   more awake bodies than Physics Threads From (vr_box3d_threads_bodies, 150) while they fall and settle. Mixed: rocks
//   and bricks, every fourth column small crates.
// - crates: walls of small crates facing you, 8 wide and 5 high, each next one 48 units behind.
// Each is tagged (stressTagBit): vr_physics_clearpiles takes them away. vr_physics_steptime (Physics Step Time) before
// and after gives the step's time; vr_physics_mtbench the same alone.
void bigPile_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_bigpile [<count>] [<distance>] [debris | rocks | bricks | crates | mixed] (in a game)\n");
        return;
    }
    PileKind kind = PileKind::Debris;
    int numbers = 0;
    int count = -1;
    float distance = 96.f;
    for(int a = 1; a < Cmd_Argc(); a++)
    {
        const char* arg = Cmd_Argv(a);
        if((arg[0] >= '0' && arg[0] <= '9') || arg[0] == '.' || arg[0] == '-')
        {
            if(numbers++ == 0)
            {
                count = CLAMP(1, Q_atoi(arg), 2000);
            }
            else
            {
                distance = static_cast<float>(Q_atof(arg));
            }
            continue;
        }
        bool known = false;
        for(const PileKindName& k : pileKindNames)
        {
            if(!q_strcasecmp(k.name, arg))
            {
                kind = k.kind;
                known = true;
            }
        }
        if(!known)
        {
            Con_Printf("vr_physics_bigpile: no kind %s (debris, rocks, bricks, crates, mixed)\n", arg);
            return;
        }
    }
    if(count < 0)
    {
        const float v = kind == PileKind::Crates ? vr_test_pile_crates.value : vr_test_pile_count.value;
        count = CLAMP(1, static_cast<int>(v), 2000);
    }
    const VmScope vm;
    const func_t pieceFn = qvr::progs::findFunction("vr_debris_piece");
    const func_t crateFn = qvr::progs::findFunction("vr_crate");
    if((kind != PileKind::Crates && !pieceFn) || ((kind == PileKind::Crates || kind == PileKind::Mixed) && !crateFn))
    {
        Con_Printf("vr_physics_bigpile: no vr_debris_piece or vr_crate in the progs\n");
        return;
    }
    edict_t* player = EDICT_NUM(1);
    vec3_t yaw{0.f, player->v.angles[1], 0.f};
    vec3_t forward, right, up;
    AngleVectors(yaw, forward, right, up);
    const glm::vec3 fwd{forward[0], forward[1], 0.f};
    const glm::vec3 side{right[0], right[1], 0.f};
    const glm::vec3 origin = vec(player->v.origin);
    int made = 0;

    if(kind == PileKind::Crates)
    {
        constexpr int wide = 8;
        constexpr int high = 5;
        constexpr float wallGap = 48.f;
        const int across = za::min(count, wide);
        float floorZ[wide] = {}; // the floor under each column of this wall (where crateplace rested its bottom crate)
        float step = 33.f;       // a crate's width and a unit between (from the first crate)
        for(int i = 0; i < count; i++)
        {
            const int wall = i / (across * high);
            const int inWall = i % (across * high);
            const int column = inWall % across;
            const int row = inWall / across;
            const float cx = static_cast<float>(column) - static_cast<float>(across - 1) * 0.5f;
            const glm::vec3 at = origin + fwd * (distance + static_cast<float>(wall) * wallGap) + side * (cx * step);
            edict_t* e = spawnStressProp(crateFn, "vr_crate", nullptr, at);
            if(!e)
            {
                continue;
            }
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            if(i == 0)
            {
                step = hi.x - lo.x + 1.f;
            }
            if(row == 0)
            {
                floorZ[column] = e->v.absmin[2];
            }
            const float z = floorZ[column] + static_cast<float>(row) * (hi.z - lo.z + 0.5f);
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.5f}, player->v.angles[1]);
            made++;
        }
        Con_Printf("vr_physics_bigpile: %d of %d crates in walls of %d x %d ahead (vr_physics_clearpiles: away)\n", made,
            count, across, high);
        return;
    }

    // Columns of props (pieces 10, a crate column 3) on a square grid.
    const bool mixed = kind == PileKind::Mixed;
    const float spacing = mixed ? 36.f : 20.f;
    constexpr float lean = 1.5f; // units each prop is set along from the one under it
    const auto crateColumn = [mixed](int column) { return mixed && column % 4 == 3; };
    int columns = 0;
    for(int left = count; left > 0; columns++)
    {
        left -= crateColumn(columns) ? 3 : 10;
    }
    const int across = za::max(1, static_cast<int>(glm::ceil(za::sqrt(static_cast<float>(columns)))));
    const int firstModel = kind == PileKind::Bricks ? pileRocks : 0;
    const int models = kind == PileKind::Rocks ? pileRocks : kind == PileKind::Bricks ? pileBricks : pileRocks + pileBricks;
    int column = 0;
    int row = 0;
    int pieces = 0;
    float z = 0.f; // the bottom of the next prop of this column
    for(int i = 0; i < count; i++)
    {
        const bool crate = crateColumn(column);
        const float cx = static_cast<float>(column % across) - static_cast<float>(across - 1) * 0.5f;
        const float cy = static_cast<float>(column / across);
        const glm::vec3 base = origin + fwd * (distance + cy * spacing) + side * (cx * spacing);
        edict_t* e = crate ? spawnStressProp(crateFn, "vr_crate", nullptr, base)
                           : spawnStressProp(pieceFn, "vr_debris_piece", pileModels[firstModel + pieces++ % models], base);
        if(e)
        {
            if(row == 0)
            {
                z = e->v.absmin[2]; // the floor under the column
            }
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            const glm::vec3 at = base + fwd * (lean * static_cast<float>(row));
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.5f}, player->v.angles[1] + static_cast<float>((i * 37) % 90));
            z += hi.z - lo.z + 0.5f;
            made++;
        }
        if(++row >= (crate ? 3 : 10))
        {
            row = 0;
            column++;
        }
    }
    Con_Printf("vr_physics_bigpile: %d of %d props in %d columns ahead (vr_physics_steptime: the step's time; "
               "vr_physics_clearpiles: away)\n",
        made, count, columns);
}

// vr_physics_clearpiles: every prop the stress tests spawned (vr_physics_bigpile: tagged) taken away, and every broken
// crate's piece (theirs or not), as QC's remove() does. Debug > Tests > Physics Stress: Clear the Piles.
void clearPiles_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_physics_clearpiles: in a game\n");
        return;
    }
    const VmScope vm;
    int removed = 0;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        const bool tagged = (static_cast<int>(e->v.spawnflags) & stressTagBit) != 0;
        if(tagged || !strcmp(PR_GetString(e->v.classname), "vr_crate_piece"))
        {
            ED_Free(e);
            removed++;
        }
    }
    Con_Printf("vr_physics_clearpiles: %d removed\n", removed);
}

// vr_physics_blast <x> <y> <z> [<damage>]: an explosion there, from the world (QC's T_RadiusDamage, 120 by default: a
// rocket's; and its effect): what it does to monsters, gibs and the props (physicsblast). For tests.
void blast_f()
{
    if(!sv.active || Cmd_Argc() < 4)
    {
        Con_Printf("usage: vr_physics_blast <x> <y> <z> [<damage>]\n");
        return;
    }
    const VmScope vm;
    dfunction_t* fn = nullptr;
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        if(!strcmp(PR_GetString(qcvm->functions[i].s_name), "T_RadiusDamage"))
        {
            fn = &qcvm->functions[i];
        }
    }
    if(!fn)
    {
        Con_Printf("vr_physics_blast: no T_RadiusDamage in the progs\n");
        return;
    }
    const float at[3] = {static_cast<float>(Q_atof(Cmd_Argv(1))), static_cast<float>(Q_atof(Cmd_Argv(2))), static_cast<float>(Q_atof(Cmd_Argv(3)))};
    const float damage = Cmd_Argc() > 4 ? static_cast<float>(Q_atof(Cmd_Argv(4))) : 120.f;
    edict_t* e = ED_Alloc();
    VectorCopy(at, e->v.origin);
    SV_LinkEdict(e, false);
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM2) = damage;
    G_INT(OFS_PARM3) = EDICT_TO_PROG(qcvm->edicts);
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
    ED_Free(e);
    if(sv.datagram.cursize <= MAX_DATAGRAM - 16) // (an unreliable message: none when full)
    {
        MSG_WriteByte(&sv.datagram, svc_temp_entity);
        MSG_WriteByte(&sv.datagram, TE_EXPLOSION);
        for(const float c : at)
        {
            MSG_WriteCoord(&sv.datagram, c, sv.protocolflags);
        }
        VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    }
    Con_Printf("vr_physics_blast: %.0f at %.0f %.0f %.0f\n", damage, at[0], at[1], at[2]);
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
    const VmScope vm;
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

// The q quantile (0..1) of sorted samples (0: none).
[[nodiscard]] double quantile(const za::Vector<float>& sorted, double q)
{
    return sorted.empty() ? 0.0
                          : static_cast<double>(sorted[za::min(static_cast<za::SizeT>(q * static_cast<double>(sorted.size())),
                                sorted.size() - 1)]);
}

// vr_physics_steptime [bins]: Box3D's step time a frame since it last printed (the average, the median, the 95th
// percentile and the worst, ms), the awake bodies on average, and its workers (vr_box3d_threads); then starts counting
// again. A bench: run it, play, run it again (Misc/quakevr/box3dmt/pilebench.py). `bins`: also the frames by the awake
// bodies as their step began (what Physics Threads From compares), each range's frames, median and 95th percentile; and
// Box3D's own profile (ms a frame: where the step went).
void steptime_f()
{
    if(!world)
    {
        Con_Printf("vr_physics_steptime: no physics world\n");
        return;
    }
    const int n = za::max(world->stepFrames, 1);
    za::Vector<float> ms;
    ms.reserve(world->stepSamples.size());
    for(const World::StepSample& x : world->stepSamples)
    {
        ms.pushBack(x.ms);
    }
    za::quickSort(ms.begin(), ms.end());
    const za::SizeT over = static_cast<za::SizeT>(ms.end() - za::upperBound(ms.begin(), ms.end(), 2.f));
    Con_Printf("vr_physics_steptime: %d frames, step %.3f ms (median %.3f, p95 %.3f, p99 %.3f, worst %.3f; %d over 2 ms), "
               "%.1f bodies awake, %d bodies, %d workers\n",
        world->stepFrames, world->stepTime * 1000.0 / n, quantile(ms, 0.5), quantile(ms, 0.95), quantile(ms, 0.99),
        world->stepTimeMax * 1000.0, static_cast<int>(over), static_cast<double>(world->stepAwake) / n,
        b3World_GetCounters(world->id).bodyCount, world->tasks.workers);
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "bins"))
    {
        constexpr int edges[] = {0, 10, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000, 100000};
        for(za::SizeT b = 0; b + 1 < sizeof(edges) / sizeof(edges[0]); b++)
        {
            ms.clear();
            for(const World::StepSample& x : world->stepSamples)
            {
                if(x.awake >= edges[b] && x.awake < edges[b + 1])
                {
                    ms.pushBack(x.ms);
                }
            }
            if(ms.empty())
            {
                continue;
            }
            za::quickSort(ms.begin(), ms.end());
            Con_Printf("vr_physics_steptime bin %d %d: %d frames, median %.4f, p95 %.4f\n", edges[b], edges[b + 1],
                static_cast<int>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95));
        }
        const b3Profile& p = world->stepProfile;
        const float k = 1.f / static_cast<float>(n);
        Con_Printf("vr_physics_steptime profile: step %.3f pairs %.3f collide %.3f solve %.3f (setup %.3f constraints %.3f "
                   "[prepare %.3f intvel %.3f warm %.3f impulses %.3f intpos %.3f relax %.3f restitution %.3f store %.3f] "
                   "transforms %.3f split %.3f refit %.3f bullets %.3f sleep %.3f)\n",
            p.step * k, p.pairs * k, p.collide * k, p.solve * k, p.solverSetup * k, p.constraints * k,
            p.prepareConstraints * k, p.integrateVelocities * k, p.warmStart * k, p.solveImpulses * k,
            p.integratePositions * k, p.relaxImpulses * k, p.restitution * k, p.storeImpulses * k, p.transforms * k,
            p.splitIslands * k, p.refit * k, p.bullets * k, p.sleepIslands * k);
    }
    world->stepProfile = b3Profile{};
    world->stepSamples.clear();
    world->stepTime = 0.0;
    world->stepTimeMax = 0.0;
    world->stepFrames = 0;
    world->stepAwake = 0;
}

// vr_physics_frametime [<label>]: each server frame's physics since it last printed, by phase (framePhaseNames; ms: the
// mean, median, 95th and 99th percentiles and the worst), with the wall-clock time and the host frames between the two
// calls (the whole frame's cost, with vr_fixed_frames: a test run's same frames); then starts counting again (the first
// call starts it: no samples kept before). A bench: Misc/quakevr/physbench/physbench.py.
void frametime_f()
{
    frameTiming = true;
    const double now = Sys_DoubleTime();
    const double wall = frameLastWall > 0.0 ? now - frameLastWall : 0.0;
    const int hostFrames = host_framecount - frameLastHostFrame;
    frameLastWall = now;
    frameLastHostFrame = host_framecount;
    const char* label = Cmd_Argc() > 1 ? Cmd_Argv(1) : "-";
    if(!world)
    {
        Con_Printf("vr_physics_frametime %s: no physics world\n", label);
        return;
    }
    const za::Vector<World::FrameSample>& samples = world->frameSamples;
    double awake = 0.0;
    for(const World::FrameSample& x : samples)
    {
        awake += x.awake;
    }
    const b3Counters c = b3World_GetCounters(world->id);
    Con_Printf("vr_physics_frametime %s: %d frames, %d host frames in %.3f s (%.4f ms a host frame), %.1f awake, %d bodies, "
               "%d contacts\n",
        label, static_cast<int>(samples.size()), hostFrames, wall, hostFrames > 0 ? wall * 1000.0 / hostFrames : 0.0,
        samples.empty() ? 0.0 : awake / static_cast<double>(samples.size()), c.bodyCount, c.contactCount);
    za::Vector<float> ms;
    ms.reserve(samples.size());
    for(int p = 0; p < framePhases; p++)
    {
        ms.clear();
        double sum = 0.0;
        for(const World::FrameSample& x : samples)
        {
            ms.pushBack(x.ms[p]);
            sum += x.ms[p];
        }
        za::quickSort(ms.begin(), ms.end());
        Con_Printf("vr_physics_frametime %s %s: mean %.4f median %.4f p95 %.4f p99 %.4f worst %.4f\n", label,
            framePhaseNames[p], ms.empty() ? 0.0 : sum / static_cast<double>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95),
            quantile(ms, 0.99), ms.empty() ? 0.0 : static_cast<double>(ms.back()));
    }
    world->frameSamples.clear();
}

// vr_physics_mtbench [<bodies> [<steps>]]: Box3D on the pool, alone (ROUND21.md, "Box3D on the pool"). A world of its
// own: a floor, `bodies` boxes (60 cm and 90 cm cubes: a small explosive box, a crate; 200) in leaning columns of 10 that
// topple into a pile, stepped `steps` times (300) at 1/72 s with vr_box3d_substeps, from the same start with each worker
// count (1, 2, 3, 4, 6, 8, 16 and all the pool's, as far as the pool goes): each count's step time, its awake bodies and
// a hash of every body's place, turn and velocities after, which must be the same for every count.
void mtbench_f()
{
    const int bodies = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 5000) : 200;
    const int steps = Cmd_Argc() > 2 ? CLAMP(1, Q_atoi(Cmd_Argv(2)), 100000) : 300;
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    const int most = jobs::pool() ? za::min(jobs::workers() + 1, B3_MAX_WORKERS) : 1;
    za::Vector<int> counts;
    for(const int n : {1, 2, 3, 4, 6, 8, 16})
    {
        if(n < most)
        {
            counts.pushBack(n);
        }
    }
    counts.pushBack(most);
    const za::UniquePtr<StepTasks> tasks = za::makeUnique<StepTasks>();
    za::U64 first = 0;
    bool same = true;
    for(const int workers : counts)
    {
        tasks->workers = workers;
        b3WorldDef def = b3DefaultWorldDef();
        def.gravity = b3Vec3{0.f, 0.f, -9.81f};
        def.enableSleep = true;
        def.enableContinuous = true;
        def.workerCount = static_cast<uint32_t>(workers);
        def.enqueueTask = enqueueStepTask;
        def.finishTask = finishStepTask;
        def.userTaskContext = tasks.get();
        const b3WorldId id = b3CreateWorld(&def);
        b3BodyDef floorDef = b3DefaultBodyDef();
        floorDef.position = b3Pos{0.f, 0.f, -0.5f};
        const b3ShapeDef shape = b3DefaultShapeDef();
        const b3BoxHull floorHull = b3MakeBoxHull(100.f, 100.f, 0.5f);
        b3CreateHullShape(b3CreateBody(id, &floorDef), &shape, &floorHull.base);
        const b3BoxHull small = b3MakeCubeHull(0.3f), crate = b3MakeCubeHull(0.45f);
        za::Vector<b3BodyId> made;
        const int columns = (bodies + 9) / 10, side = za::max(1, static_cast<int>(za::ceil(za::sqrt(static_cast<float>(columns)))));
        for(int i = 0; i < bodies; i++)
        {
            const int column = i / 10, level = i % 10;
            const bool big = column % 2 == 1;
            const float half = big ? 0.45f : 0.3f;
            b3BodyDef body = b3DefaultBodyDef();
            body.type = b3_dynamicBody;
            // Each level 10 cm further along x than the one under it, and turned (as vr_physics_pile): they lean and fall.
            body.position = b3Pos{static_cast<float>(column % side) * 2.5f + static_cast<float>(level) * 0.1f,
                static_cast<float>(column / side) * 2.5f, 0.02f + static_cast<float>(level) * (2.f * half + 0.02f) + half};
            body.rotation = b3MakeQuatFromAxisAngle(b3Vec3{0.f, 0.f, 1.f}, static_cast<float>((i * 37) % 90) * (3.14159265f / 180.f));
            const b3BodyId b = b3CreateBody(id, &body);
            b3CreateHullShape(b, &shape, big ? &crate.base : &small.base);
            made.pushBack(b);
        }
        double time = 0.0;
        za::U64 awake = 0;
        for(int i = 0; i < steps; i++)
        {
            tasks->next.storeRelaxed(0);
            const double t0 = Sys_DoubleTime();
            b3World_Step(id, 1.f / 72.f, substeps);
            time += Sys_DoubleTime() - t0;
            awake += static_cast<za::U64>(b3World_GetAwakeBodyCount(id));
        }
        za::U64 h = 1469598103934665603ull;
        const auto mix = [&h](const void* p, size_t n)
        {
            for(size_t k = 0; k < n; k++)
            {
                h = (h ^ static_cast<const unsigned char*>(p)[k]) * 1099511628211ull;
            }
        };
        for(const b3BodyId b : made)
        {
            const b3WorldTransform x = b3Body_GetTransform(b);
            const b3Vec3 v[2] = {b3Body_GetLinearVelocity(b), b3Body_GetAngularVelocity(b)};
            mix(&x, sizeof(x));
            mix(v, sizeof(v));
        }
        b3DestroyWorld(id);
        if(workers == counts[0])
        {
            first = h;
        }
        same = same && h == first;
        Con_Printf("vr_physics_mtbench: %d bodies, %d steps of %d: %2d workers %.3f ms a step, %.1f awake, hash %016llx\n",
            bodies, steps, substeps, workers, time * 1000.0 / steps, static_cast<double>(awake) / steps,
            static_cast<unsigned long long>(h));
    }
    Con_Printf("vr_physics_mtbench: %s\n", same ? "the same with every worker count" : "DIFFERENT between worker counts");
}

// vr_corpse_list: the corpses in the physics (vr_corpse_collide): each one's body (fixed or pushable, its box or its
// fitted hulls, its mass), where it lies and turns, and what touches it (props, held things and hands' bodies, the level).
// vr_ragdoll_list [1|2]: the ragdolls (vr_ragdoll): each one's entity, how long since it went limp, its parts (awake), where
// its pelvis is and how high its parts' lowest and highest points are; 1: each part, and the hands holding or pulling it
// (how far its point is from the hand); 2: also its flames (how far from its limbs' surface).
void ragdollList_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_ragdoll_list: no Box3D world\n");
        return;
    }
    const VmScope vm;
    int count = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        count++;
        int awake = 0;
        float mass = 0.f;
        glm::vec3 lo{1e30f}, hi{-1e30f};
        int parts = 0;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            parts++;
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            awake += b3Body_IsAwake(body) ? 1 : 0;
            mass += b3Body_GetMass(body);
            const b3AABB box = b3Body_ComputeAABB(body);
            lo = glm::min(lo, world->toU(box.lowerBound));
            hi = glm::max(hi, world->toU(box.upperBound));
        }
        edict_t* ent = EDICT_NUM(r.num);
        const glm::vec3 p = world->toU(b3Body_GetWorldCenter(r.body[0]));
        Con_Printf("ragdoll %d %s: %.1f s limp, %d parts (%d awake), %.1f kg, pelvis %.1f %.1f %.1f, parts z %.1f .. %.1f, frame %d%s\n", r.num,
            PR_GetString(ent->v.classname), qcvm->time - r.born, parts, awake, mass, p.x, p.y, p.z, lo.z, hi.z,
            static_cast<int>(ent->v.frame), r.cut ? ", headless" : "");
        if(Cmd_Argc() > 1)
        {
            for(int b = 0; b < r.count; b++)
            {
                if(partCut(r, b))
                {
                    Con_Printf("  %2d %-11s cut off\n", b, r.rig->bones[b].name);
                    continue;
                }
                const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
                const glm::vec3 c = world->toU(b3Body_GetWorldCenter(body));
                const glm::vec3 v = world->toU(b3Body_GetLinearVelocity(body));
                Con_Printf("  %2d %-11s %5.2f kg at %7.1f %7.1f %7.1f, %5.1f u/s%s\n", b, r.rig->bones[b].name, b3Body_GetMass(body), c.x, c.y,
                    c.z, glm::length(v), b3Body_IsAwake(body) ? "" : " (asleep)");
            }
        }
        // vr_ragdoll_list 2: the flames on it (vr_burning.qc's, within its box): how far each is from its limbs' surface.
        if(Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) >= 2)
        {
            int flames = 0;
            float sum = 0.f, most = 0.f;
            for(int e = svs.maxclients + 1; e < qcvm->num_edicts; e++)
            {
                edict_t* fl = EDICT_NUM(e);
                const glm::vec3 at = vec(fl->v.origin);
                if(fl->free || strcmp(PR_GetString(fl->v.classname), "vr_burn_flame") ||
                   glm::any(glm::lessThan(at, lo - glm::vec3{16.f})) || glm::any(glm::greaterThan(at, hi + glm::vec3{16.f})))
                {
                    continue;
                }
                float distance = 0.f;
                (void)nearestPart(r, at, distance);
                flames++;
                sum += distance;
                most = za::max(most, distance);
            }
            Con_Printf("  %d flames on it: %.1f units from its limbs' surface on average, %.1f at most\n", flames,
                sum / static_cast<float>(za::max(flames, 1)), most);
        }
        for(const World::RagdollGrab& g : world->ragdollGrabs)
        {
            if(g.num == r.num)
            {
                Con_Printf("  %s by client %d's %s hand: part %d (%s), %.1f units from the hand\n", g.pulling ? "pulled" : "held",
                    g.player, g.hand ? "main" : "off", g.part, r.rig->bones[g.part].name, grabReach(g.player, g.hand));
            }
        }
    }
    Con_Printf("vr_ragdoll_list: %d ragdolls (vr_ragdoll %d, at most %d); Box3D %d bodies, %d awake\n", count, static_cast<int>(vr_ragdoll.value),
        static_cast<int>(vr_ragdoll_max.value), b3World_GetCounters(world->id).bodyCount, b3World_GetAwakeBodyCount(world->id));
}

// vr_ragdoll_blast_test [damage]: a blast's push alone (no damage: nothing gibbed) 24 units beside the ragdoll nearest
// the player, on his side: it is thrown away from him (Debug > Tests).
void ragdollBlastTest_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        Con_Printf("vr_ragdoll_blast_test: no Box3D world\n");
        return;
    }
    const VmScope vm;
    const glm::vec3 player = vec(svs.clients[0].edict->v.origin);
    const RagdollBodies* best = nullptr;
    float bestD = 1e30f;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && glm::distance(world->toU(b3Body_GetWorldCenter(r.body[0])), player) < bestD)
        {
            bestD = glm::distance(world->toU(b3Body_GetWorldCenter(r.body[0])), player);
            best = &r;
        }
    }
    if(!best)
    {
        Con_Printf("vr_ragdoll_blast_test: no ragdoll\n");
        return;
    }
    const float damage = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 80.f;
    const glm::vec3 c = world->toU(b3Body_GetWorldCenter(best->body[0]));
    glm::vec3 side = player - c;
    side.z = 0.f;
    side = glm::length(side) > 0.01f ? glm::normalize(side) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 at = c + side * 24.f;
    blastRagdoll(*best, at, damage);
    Con_Printf("vr_ragdoll_blast_test: %.0f at %.0f %.0f %.0f beside ragdoll %d\n", damage, at.x, at.y, at.z, best->num);
}

void corpseList_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_corpse_list: no Box3D world\n");
        return;
    }
    const VmScope vm;
    static constexpr const char* modes[] = {"none", "fixed box", "pushable box", "fixed pose", "pushable pose"};
    int count = 0;
    za::Vector<b3ContactData>& contacts = scratch.pushContacts;
    for(int num = 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Corpse || B3_IS_NULL(s.body))
        {
            continue;
        }
        count++;
        edict_t* e = EDICT_NUM(num);
        contacts.resize(static_cast<za::SizeT>(za::max(b3Body_GetContactCapacity(s.body), 1)));
        const int n = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        int props = 0, hands = 0, level = 0;
        for(int c = 0; c < n; c++)
        {
            const b3ContactData& cd = contacts[static_cast<size_t>(c)];
            if(cd.manifoldCount == 0 || cd.manifolds[0].pointCount == 0)
            {
                continue;
            }
            const b3ShapeId other = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), s.body) ? cd.shapeIdB : cd.shapeIdA;
            const uint64_t category = b3Shape_GetFilter(other).categoryBits;
            props += (category & catProp) ? 1 : 0;
            hands += (category & (catHeld | catReach)) ? 1 : 0;
            level += (category & (catWorld | catMover)) ? 1 : 0;
        }
        Con_Printf("  %d %s: %s, %d shapes, %.0f kg, at %.1f %.1f %.1f yaw %.0f, %s; touching %d props, %d held/hands, %d level\n",
            num, PR_GetString(e->v.classname), modes[CLAMP(0, s.corpseMode, 4)], b3Body_GetShapeCount(s.body),
            s.corpseDynamic ? b3Body_GetMass(s.body) : 0.f, e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[1],
            b3Body_IsAwake(s.body) ? "awake" : "asleep", props, hands, level);
    }
    Con_Printf("vr_corpse_list: %d corpses in the physics (vr_corpse_collide %d)\n", count, corpseMode());
}

// vr_corpse_drop [<height>]: the loose prop nearest the first player put `height` units (32) over the top of the corpse
// nearest him, at rest, to fall on it. For tests (Debug menu: Drop the Nearest Prop on the Nearest Corpse).
void corpseDrop_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        return;
    }
    const VmScope vm;
    const glm::vec3 eye = vec(EDICT_NUM(1)->v.origin);
    int corpse = 0, prop = 0;
    float corpseD = 1e9f, propD = 1e9f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        const float d = glm::distance(eye, vec(EDICT_NUM(num)->v.origin));
        if(s.kind == Kind::Corpse && d < corpseD)
        {
            corpse = num;
            corpseD = d;
        }
        else if(s.kind == Kind::Prop && d < propD)
        {
            prop = num;
            propD = d;
        }
    }
    if(!corpse || !prop)
    {
        Con_Printf("vr_corpse_drop: no %s\n", corpse ? "loose prop" : "corpse in the physics");
        return;
    }
    edict_t* c = EDICT_NUM(corpse);
    edict_t* p = EDICT_NUM(prop);
    const float height = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 32.f;
    const b3AABB box = b3Body_ComputeAABB(world->slots[corpse].body);
    const glm::vec3 top = (world->toU(box.lowerBound) + world->toU(box.upperBound)) * 0.5f;
    const float bottom = world->toU(box.upperBound).z + height;
    const glm::vec3 to{top.x - 0.5f * (p->v.mins[0] + p->v.maxs[0]), top.y - 0.5f * (p->v.mins[1] + p->v.maxs[1]), bottom - p->v.mins[2]};
    store(to, p->v.origin);
    p->v.velocity[0] = p->v.velocity[1] = p->v.velocity[2] = 0.f;
    setFlag(p, FL_ONGROUND, false);
    SV_LinkEdict(p, false);
    Con_Printf("vr_corpse_drop: %d %s put over corpse %d %s (its top at %.1f) at %.1f %.1f %.1f\n", prop,
        PR_GetString(p->v.classname), corpse, PR_GetString(c->v.classname), world->toU(box.upperBound).z, to.x, to.y, to.z);
}

// vr_physics_list [<classname | props>]: the rigid bodies (or those), where they are and how they move.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
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
    za::Array<b3ShapeId, 4> shapes;
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
    const VmScope vm;
    const float cm = 100.f / world->m2u;
    const auto box = [&](const char* what, const glm::vec3& lo, const glm::vec3& hi) {
        const glm::vec3 s = hi - lo;
        Con_Printf("    %s %.2f x %.2f x %.2f units (%.1f x %.1f x %.1f cm)\n", what, s.x, s.y, s.z, s.x * cm, s.y * cm,
            s.z * cm);
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
            box("Quake box      ", glm::vec3{e->v.mins[0], e->v.mins[1], e->v.mins[2]},
                glm::vec3{e->v.maxs[0], e->v.maxs[1], e->v.maxs[2]});
        }
    }
    if(world->hands.size() < 2)
    {
        return;
    }
    const FieldOffsets& f = fields();
    edict_t* player = EDICT_NUM(1);
    for(int h = 0; h < 2; h++)
    {
        const World::HandBody& hb = world->hands[1][static_cast<size_t>(h)];
        if(B3_IS_NULL(hb.reach) || hb.key.what != World::ReachKey::Weapon)
        {
            continue;
        }
        const glm::vec3 port = fieldVec(player, h ? f.loadportpos : f.offloadportpos);
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
    const VmScope vm;
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
// the map would place it there. For tests.
void spawn_f()
{
    if(!sv.active || Cmd_Argc() < 2 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_spawn <classname> [<distance> [<left>]]\n");
        return;
    }
    const VmScope vm;
    const func_t fn = qvr::progs::findFunction(Cmd_Argv(1));
    if(!fn)
    {
        Con_Printf("vr_physics_spawn: no spawn function %s\n", Cmd_Argv(1));
        return;
    }
    edict_t* player = EDICT_NUM(1);
    vec3_t yaw{0.f, player->v.angles[1], 0.f};
    vec3_t forward, right, up;
    AngleVectors(yaw, forward, right, up);
    const float distance = Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 48.f;
    const float left = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : 0.f;
    edict_t* e = ED_Alloc();
    for(int i = 0; i < 3; i++)
    {
        e->v.origin[i] = player->v.origin[i] + forward[i] * distance - right[i] * left;
    }
    char* name = nullptr;
    const int s = PR_AllocString(static_cast<int>(strlen(Cmd_Argv(1))) + 1, &name);
    strcpy(name, Cmd_Argv(1));
    e->v.classname = s;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    PR_ExecuteProgram(fn);
    if(!e->free)
    {
        SV_LinkEdict(e, false);
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
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    za::Vector<edict_t*> list;
    if(!strcmp(Cmd_Argv(1), "nearest"))
    {
        if(const int found = box3d::nearestProp())
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
    const VmScope vm;
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
            b3ShapeId shapes[8];
            const int n = b3Body_GetShapes(body, shapes, 8);
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

// vr_physics_player [onto <number | classname>]: the first player's origin, velocity and ground (FL_ONGROUND,
// .groundentity) and what is under his feet, for tests of standing on props; with onto, first put on top of that
// entity's box (the first of them), still, noclip off; with near, on the floor beside it (see below).
void playerOverProp(edict_t* p); // (below, with the players' shape against props)

void player_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        return;
    }
    const VmScope vm;
    edict_t* p = EDICT_NUM(1);
    if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "onto"))
    {
        const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(2));
        if(list.empty())
        {
            Con_Printf("vr_physics_player: no %s\n", Cmd_Argv(2));
            return;
        }
        const edict_t* e = list.front();
        p->v.origin[0] = (e->v.absmin[0] + e->v.absmax[0]) * 0.5f;
        p->v.origin[1] = (e->v.absmin[1] + e->v.absmax[1]) * 0.5f;
        // (onto <entity> <height>: his feet that far over its box's top, 1 by default; under 0, into it: his feet in a lying
        // box's top, as it rocks up into them.)
        p->v.origin[2] = e->v.absmax[2] - p->v.mins[2] + (Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 1.f);
        VectorCopy(vec3_origin, p->v.velocity);
        VectorCopy(p->v.origin, p->v.oldorigin); // (SV_CheckStuck's last free spot: here, not where he was before)
        p->v.movetype = MOVETYPE_WALK;
        setFlag(p, FL_ONGROUND, false);
        SV_LinkEdict(p, false);
    }
    else if(Cmd_Argc() > 4 && !strcmp(Cmd_Argv(1), "near"))
    {
        // near <number | classname> <direction> <distance>: on the floor the entity's box stands on, <distance> units
        // from its middle towards <direction> (degrees of yaw), facing it (a run-up at a prop from a side).
        const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(2));
        if(list.empty())
        {
            Con_Printf("vr_physics_player: no %s\n", Cmd_Argv(2));
            return;
        }
        const edict_t* e = list.front();
        const float a = glm::radians(Q_atof(Cmd_Argv(3))), d = Q_atof(Cmd_Argv(4));
        p->v.origin[0] = (e->v.absmin[0] + e->v.absmax[0]) * 0.5f + d * za::cos(a);
        p->v.origin[1] = (e->v.absmin[1] + e->v.absmax[1]) * 0.5f + d * za::sin(a);
        p->v.origin[2] = e->v.absmin[2] - p->v.mins[2] + 1.f;
        p->v.angles[0] = p->v.angles[2] = 0.f;
        p->v.angles[1] = anglemod(Q_atof(Cmd_Argv(3)) + 180.f);
        p->v.fixangle = 1;
        VectorCopy(vec3_origin, p->v.velocity);
        VectorCopy(p->v.origin, p->v.oldorigin); // (SV_CheckStuck's last free spot: here, not where he was before)
        p->v.movetype = MOVETYPE_WALK;
        setFlag(p, FL_ONGROUND, false);
        SV_LinkEdict(p, false);
    }
    edict_t* g = PROG_TO_EDICT(p->v.groundentity);
    Con_Printf("vr_physics_player: %.2f s at %.2f %.2f %.2f vel %.1f %.1f %.1f %s on %d %s\n", qcvm->time, p->v.origin[0], p->v.origin[1],
        p->v.origin[2], p->v.velocity[0], p->v.velocity[1], p->v.velocity[2],
        (static_cast<int>(p->v.flags) & FL_ONGROUND) ? "grounded" : "in the air", NUM_FOR_EDICT(g),
        g == qcvm->edicts ? "the world" : PR_GetString(g->v.classname));
    // What is under the feet: the player's box moved 2 units down.
    vec3_t down{p->v.origin[0], p->v.origin[1], p->v.origin[2] - 2.f};
    const trace_t tr = SV_Move(p->v.origin, p->v.mins, p->v.maxs, down, MOVE_NORMAL, p);
    Con_Printf("vr_physics_player: below: fraction %.3f%s%s normal z %.2f on %d, movetype %d\n", tr.fraction,
        tr.startsolid ? " startsolid" : "", tr.allsolid ? " allsolid" : "", tr.plane.normal[2], tr.ent ? NUM_FOR_EDICT(tr.ent) : -1,
        static_cast<int>(p->v.movetype));
    playerOverProp(p); // (below: the players' shape against props)
}

void inLevel_f(); // (below: the Box3D queries it uses are)
void approach_f(); // (below, with the players' shape against props)
void shotBench_f(); // (below, with the shots' shape against props)
void fire_f();
void knockdownTest_f();
void shockCheck_f();
void inside_f();
void watchInside();

bool commandsRegistered = false; // (registerCommands: once)

void portalInfo_f()
{
    const VmScope vm;
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

void registerCommands()
{
    bool& registered = commandsRegistered;
    if(!registered)
    {
        registered = true;
        b3SetAllocator(heapAllocate, heapFree);
        Cmd_AddCommand("vr_physics_player", player_f);
        Cmd_AddCommand("vr_physics_stack", stack_f);
        Cmd_AddCommand("vr_physics_pyramid", pyramid_f);
        Cmd_AddCommand("vr_physics_list", list_f);
        Cmd_AddCommand("vr_physics_shapes", shapes_f);
        Cmd_AddCommand("vr_physics_loose", loose_f);
        Cmd_AddCommand("vr_physics_pile", pile_f);
        Cmd_AddCommand("vr_physics_bigpile", bigPile_f);
        Cmd_AddCommand("vr_physics_clearpiles", clearPiles_f);
        Cmd_AddCommand("vr_physics_hash", hash_f);
        Cmd_AddCommand("vr_physics_steptime", steptime_f);
        Cmd_AddCommand("vr_physics_frametime", frametime_f);
        Cmd_AddCommand("vr_physics_mtbench", mtbench_f);
        Cmd_AddCommand("vr_physics_blast", blast_f);
        Cmd_AddCommand("vr_physics_sink", sink_f);
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
        Cmd_AddCommand("vr_ragdoll_list", ragdollList_f);
        Cmd_AddCommand("vr_ragdoll_info", ragdoll::info_f);
        Cmd_AddCommand("vr_limb_models", limbmodel::info_f);
        Cmd_AddCommand("vr_drawn_motion_test", ragdoll::motionTest_f);
        Cmd_AddCommand("vr_ragdoll_blast_test", ragdollBlastTest_f);
    }
}

[[nodiscard]] bool wanted()
{
    return fields().vr_rigid >= 0 && sv.worldmodel; // (a mod without .vr_rigid has no rigid bodies)
}

} // namespace

namespace qvr::box3d
{

void beforeLoad()
{
    finishLoads();
    const qmodel_t* map = sv.worldmodel;
    if(!wanted() || !map || map->type != mod_brush)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    if(findMesh(map, m2u))
    {
        return; // (kept: the same map again)
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
    for(const RagdollBodies& r : world->ragdolls)
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

namespace
{

// vr_debug_physics_shapes: a body's colour by what it is and does.
[[nodiscard]] glm::vec4 shapeColour(const Slot& s, bool awake)
{
    switch(s.kind)
    {
    case Kind::Prop:
        if(!awake)
        {
            return {0.35f, 0.5f, 1.f, 0.9f}; // asleep: blue
        }
        return s.bullet ? glm::vec4{1.f, 1.f, 1.f, 1.f} : glm::vec4{0.2f, 1.f, 0.3f, 1.f}; // awake: green (fast: white)
    case Kind::Held: return {1.f, 0.9f, 0.1f, 1.f};    // held: yellow
    case Kind::Mover: return {0.9f, 0.3f, 1.f, 0.7f};  // doors, plats: purple
    case Kind::Actor: return {1.f, 0.45f, 0.1f, 0.9f}; // monsters: orange
    case Kind::Player: return {0.2f, 0.9f, 1.f, 0.6f}; // players: cyan
    case Kind::Fixture: return {0.75f, 0.75f, 0.75f, 0.8f}; // pickups hanging: grey
    case Kind::Corpse:
        return s.corpseDynamic && awake ? glm::vec4{1.f, 0.4f, 0.6f, 1.f} : glm::vec4{0.7f, 0.2f, 0.3f, 0.9f}; // corpses: dark red (pushed: pink)
    default: return {1.f, 0.f, 0.f, 1.f};
    }
}

void drawHull(const b3HullData* hull, const b3WorldTransform& xf, const glm::vec4& colour, float width)
{
    const b3Vec3* points = b3GetHullPoints(hull);
    const b3HullHalfEdge* edges = b3GetHullEdges(hull);
    for(int i = 0; i < hull->edgeCount; i++)
    {
        const b3HullHalfEdge& e = edges[i];
        if(e.twin < i)
        {
            continue; // each edge once
        }
        const glm::vec3 a = world->toU(b3TransformPoint(xf, points[e.origin]));
        const glm::vec3 b = world->toU(b3TransformPoint(xf, points[edges[e.twin].origin]));
        lines::line(a, b, width, colour, colour);
    }
}

void drawCapsule(const b3Capsule& c, const b3WorldTransform& xf, const glm::vec4& colour, float width)
{
    const glm::vec3 a = world->toU(b3TransformPoint(xf, c.center1)), b = world->toU(b3TransformPoint(xf, c.center2));
    const float r = c.radius * world->m2u;
    constexpr int sides = 12;
    for(const glm::vec3& centre : {a, b})
    {
        for(int i = 0; i < sides; i++)
        {
            const float t0 = 2.f * glm::pi<float>() * static_cast<float>(i) / sides, t1 = 2.f * glm::pi<float>() * static_cast<float>(i + 1) / sides;
            lines::line(centre + r * glm::vec3{za::cos(t0), za::sin(t0), 0.f}, centre + r * glm::vec3{za::cos(t1), za::sin(t1), 0.f}, width,
                colour, colour);
        }
    }
    for(int i = 0; i < 4; i++)
    {
        const float t = glm::half_pi<float>() * static_cast<float>(i);
        const glm::vec3 o = r * glm::vec3{za::cos(t), za::sin(t), 0.f};
        lines::line(a + o, b + o, width, colour, colour);
    }
}

} // namespace

void debugDraw()
{
    if(!world || !sv.active)
    {
        return;
    }
    za::Array<b3ShapeId, 64> shapes;
    za::Array<b3ContactData, 16> contacts;
    constexpr float width = 0.15f;
    for(int num = 1; num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind == Kind::None || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const bool awake = b3Body_IsAwake(s.body);
        glm::vec4 colour = shapeColour(s, awake);
        if(s.kind == Kind::Player && num == cl.viewentity)
        {
            colour.a *= 0.4f; // your own, round you: faint
        }
        const b3WorldTransform xf = b3Body_GetTransform(s.body);
        const int count = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            switch(b3Shape_GetType(shapes[i]))
            {
            case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, colour, width); break;
            case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, colour, width); break;
            default: break;
            }
        }
        if(s.kind != Kind::Prop && s.kind != Kind::Held)
        {
            continue;
        }
        // The centre of mass, and (awake) where it touches things: red dots, pressed into them (or held apart: pink).
        lines::point(world->toU(b3Body_GetWorldCenter(s.body)), 0.8f, colour);
        if(!awake)
        {
            continue;
        }
        const int touching = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        for(int c = 0; c < touching; c++)
        {
            const b3ContactData& d = contacts[c];
            const bool isA = B3_ID_EQUALS(b3Shape_GetBody(d.shapeIdA), s.body);
            const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(s.body));
            for(int m = 0; m < d.manifoldCount; m++)
            {
                for(int p = 0; p < d.manifolds[m].pointCount; p++)
                {
                    const b3ManifoldPoint& mp = d.manifolds[m].points[p];
                    const glm::vec3 at = centre + world->toU(isA ? mp.anchorA : mp.anchorB);
                    lines::point(at, 0.7f, mp.separation < 0.f ? glm::vec4{1.f, 0.1f, 0.1f, 1.f} : glm::vec4{1.f, 0.5f, 0.7f, 1.f});
                }
            }
        }
    }
    // The ragdolls' parts but their pelvis (drawn above, with their slot): a corpse's colour.
    for(const RagdollBodies& r : world->ragdolls)
    {
        for(int b = 1; b < r.count && r.num > 0; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || !b3Body_IsValid(body))
            {
                continue;
            }
            const glm::vec4 colour = b3Body_IsAwake(body) ? glm::vec4{1.f, 0.4f, 0.6f, 1.f} : glm::vec4{0.7f, 0.2f, 0.3f, 0.9f};
            const b3WorldTransform xf = b3Body_GetTransform(body);
            const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
            for(int i = 0; i < count; i++)
            {
                switch(b3Shape_GetType(shapes[i]))
                {
                case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, colour, width); break;
                case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, colour, width); break;
                default: break;
                }
            }
        }
    }
    // The hands' and weapons' reach bodies (syncReach): white-blue.
    const glm::vec4 reachColour{0.6f, 0.85f, 1.f, 0.7f};
    for(const auto& both : world->hands)
    {
        for(const World::HandBody& hb : both)
        {
            if(B3_IS_NULL(hb.reach) || !b3Body_IsValid(hb.reach))
            {
                continue;
            }
            const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
            const int count = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
            for(int i = 0; i < count; i++)
            {
                switch(b3Shape_GetType(shapes[i]))
                {
                case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, reachColour, width); break;
                case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, reachColour, width); break;
                default: break;
                }
            }
        }
    }
}

} // namespace qvr::box3d

namespace qvr::box3d
{

bool push(edict_t* ent, const glm::vec3& at, const glm::vec3& velocity, float pusherMass)
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
    const float speed = glm::length(velocity) / world->m2u; // m/s
    if(speed < 1e-3f)
    {
        return true;
    }
    const glm::vec3 d = velocity / glm::length(velocity);
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
    // (A pusher of a mass: the impulse of the two meeting, the pusher's inverse mass added.)
    const float j = (speed - along) / (k + (pusherMass > 0.f ? 1.f / pusherMass : 0.f));
    b3Body_ApplyLinearImpulse(body, b3Vec3{d.x * j, d.y * j, d.z * j}, point, true);
    world->slots[num].asleep = false;
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
    za::Array<b3ShapeId, 8> shapes;
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
    za::Array<b3ShapeId, 8> shapes;
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

// A prop's own box in its axes (units, from its origin): a brush model's own (held, its entity's box is the one round
// all its turns), else the drawn one its body was made with.
void ownBox(const Slot& s, glm::vec3& lo, glm::vec3& hi)
{
    if(s.model && s.model->type == mod_brush)
    {
        lo = glm::vec3{s.model->mins[0], s.model->mins[1], s.model->mins[2]};
        hi = glm::vec3{s.model->maxs[0], s.model->maxs[1], s.model->maxs[2]};
        return;
    }
    lo = s.mins;
    hi = s.maxs;
}

[[nodiscard]] bool boxInLevel(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& at, const glm::quat& rot, float in = 1.f)
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

} // namespace

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

} // namespace qvr::box3d

namespace
{

// vr_physics_inlevel [<number | classname | props>]: how far each prop's box (its drawn one, as it is turned) is inside
// the level's solids (the world and its brush entities), to half a unit; 0: clear (touching isn't in). A prop held in
// both hands pushed into a wall (vr_carry_two_hands_solid) should stay at 0.
void inLevel_f()
{
    if(!sv.active || !world)
    {
        return;
    }
    const VmScope vm;
    for(edict_t* e : entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props"))
    {
        const int num = NUM_FOR_EDICT(e);
        if(num >= static_cast<int>(world->slots.size()))
        {
            continue;
        }
        const Slot& s = world->slots[static_cast<size_t>(num)];
        glm::vec3 lo, hi;
        qvr::box3d::ownBox(s, lo, hi);
        if(B3_IS_NULL(s.body) || hi.x <= lo.x)
        {
            continue;
        }
        const glm::quat rot = turnOf(e->v.angles, s.brush);
        const glm::vec3 at = vec(e->v.origin);
        const float most = 0.5f * za::min(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
        float depth = 0.f;
        for(float in = 0.5f; in < most; in += 0.5f)
        {
            if(!qvr::box3d::boxInLevel(lo, hi, at, rot, in))
            {
                break;
            }
            depth = in;
        }
        Con_Printf("vr_physics_inlevel: %d %s at %.1f %.1f %.1f: in the level by %.1f units%s (its box %.1f %.1f %.1f to %.1f %.1f %.1f)\n",
            num, PR_GetString(e->v.classname), at.x, at.y, at.z, depth, s.kind == Kind::Held ? " (held)" : "", lo.x, lo.y,
            lo.z, hi.x, hi.y, hi.z);
    }
}

// Standing on props (vr_box3d_player_stand; see capsuleStandsOn), each frame after the players have moved.

// How long a jump pushes the prop jumped from (s): the legs' push, which Quake's jump gives at once.
constexpr float jumpPushTime = 0.15f;

// How fast a solid prop in a player's body is pushed out of it at least (m/s; unstickProps).
constexpr float unstickSpeed = 1.f;

// The solid prop client `player` stands on now (FL_ONGROUND on a Box3D prop: VR_StandsOn made it ground), else 0.
[[nodiscard]] int standingOn(edict_t* player)
{
    if(!(static_cast<int>(player->v.flags) & FL_ONGROUND) || static_cast<int>(player->v.movetype) != MOVETYPE_WALK ||
       player->v.health <= 0.f)
    {
        return 0;
    }
    edict_t* ground = PROG_TO_EDICT(player->v.groundentity);
    const int num = NUM_FOR_EDICT(ground);
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && !ground->free &&
                   world->slots[num].kind == Kind::Prop && B3_IS_NON_NULL(world->slots[num].body)
               ? num
               : 0;
}

// The body of prop `num` if it still is one (null: not).
[[nodiscard]] b3BodyId standBody(int num)
{
    if(num <= 0 || num >= static_cast<int>(world->slots.size()) || num >= qcvm->num_edicts || EDICT_NUM(num)->free ||
       world->slots[num].kind != Kind::Prop)
    {
        return b3_nullBodyId;
    }
    return world->slots[num].body;
}

// Before the step: where each player stands (in the prop's body, to carry him after it), a prop just landed on woken
// (to feel the weight), a jump's push on the prop jumped from.
void beforeStanding()
{
    world->stands.resize(static_cast<size_t>(svs.maxclients) + 1);
    const float mass = za::max(vr_box3d_player_mass.value, 0.f);
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        World::Stand& st = world->stands[static_cast<size_t>(i)];
        const bool live = vr_box3d_player_stand.value && !player->free && svs.clients[i - 1].active;
        const int num = live ? standingOn(player) : 0;
        if(!num)
        {
            // Off it with a jump (up at a jump's speed from standing on it last frame): the prop takes a share of the
            // push, down where the feet were. As a force over jumpPushTime (pressStanding), as legs push: a blow (an
            // impulse at once) was a hit on what the prop stands on, at the speed it gave the prop (a small explosive
            // box, pushed at its edge, hit the floor at 12 m/s and blew up: ROUND21.md, "Standing on props 2").
            const b3BodyId was = standBody(st.ground);
            if(live && B3_IS_NON_NULL(was) && player->v.velocity[2] > 100.f && vr_box3d_player_jump_push.value > 0.f &&
               mass > 0.f)
            {
                const float impulse = mass * player->v.velocity[2] / world->m2u * vr_box3d_player_jump_push.value; // N s
                st.pushed = st.ground;
                st.pushLocal = st.local;
                st.pushForce = impulse / jumpPushTime;
                st.pushLeft = jumpPushTime;
                b3Body_SetAwake(was, true);
                if(vr_debug_box3d.value)
                {
                    Con_Printf("box3d: player %d jumps off %d, pushing it with %.0f N s\n", i, st.ground, impulse);
                }
            }
            st.ground = 0;
            continue;
        }
        const b3BodyId body = world->slots[num].body;
        if(num != st.ground && mass > 0.f)
        {
            b3Body_SetAwake(body, true); // (landed on it: it feels the weight, then sleeps again once still)
        }
        if(vr_debug_box3d.value && num != st.ground)
        {
            Con_Printf("box3d: player %d stands on %d %s\n", i, num, PR_GetString(EDICT_NUM(num)->v.classname));
        }
        st.ground = num;
        st.feet = glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2] + player->v.mins[2]};
        // (In the prop's drawn box: his weight on its top even where his box overhangs its edge, which a prop standing
        // on its own doesn't tip over from.)
        const Slot& s = world->slots[num];
        const glm::vec3 local =
            glm::clamp(glmv(b3Body_GetLocalPoint(body, world->toM(st.feet))), s.mins / world->m2u, s.maxs / world->m2u);
        st.local = b3v(local);
        st.feet = world->toU(b3Body_GetWorldPoint(body, st.local));
        st.top = EDICT_NUM(num)->v.absmax[2];
        if(vr_debug_box3d.value >= 2.f)
        {
            Con_Printf("box3d: player %d on %d at %.2f %.2f %.2f in it (m)\n", i, num, local.x, local.y, local.z);
        }
    }
}

} // namespace

extern "C" {
extern cvar_t sv_accelerate; // (sv_user.c)
extern cvar_t sv_maxspeed;
}

namespace
{

// Whether prop `num` is a solid prop (an explosive box) with a body: what players stand on, shove and are let out of.
[[nodiscard]] bool solidProp(int num)
{
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts &&
           world->slots[num].kind == Kind::Prop && B3_IS_NON_NULL(world->slots[num].body) && !EDICT_NUM(num)->free &&
           static_cast<int>(EDICT_NUM(num)->v.solid) == SOLID_BBOX;
}

// Whether `num` is a pushable corpse with a body (vr_corpse_collide 2, 4): a player walking into it shoves it as a solid
// prop (VR_PlayerBumps, shoveBumped), when he meets it (vr_corpse_collide_player).
[[nodiscard]] bool pushableCorpse(int num)
{
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts &&
           world->slots[num].kind == Kind::Corpse && world->slots[num].corpseDynamic && B3_IS_NON_NULL(world->slots[num].body);
}

// A nonblocking corpse still feels feet walking through it. Apply a gentle,
// bounded horizontal nudge to nearby limbs without changing player collision.
void nudgeWalkedRagdolls(float dt)
{
    world->walkOrigins.resize(svs.maxclients + 1);
    for(int player = 1; player <= svs.maxclients; player++)
    {
        edict_t* e = EDICT_NUM(player);
        const glm::vec3 origin = vec(e->v.origin);
        glm::vec3& previous = world->walkOrigins[static_cast<size_t>(player)];
        const glm::vec3 walked{origin.x - previous.x, origin.y - previous.y, 0.f};
        const bool physical = previous != glm::vec3{0.f} && glm::length(walked) < world->m2u;
        previous = origin;
        if(e->free || e->v.health <= 0.f) { continue; }
        glm::vec3 pace{e->v.velocity[0], e->v.velocity[1], 0.f};
        if(physical && dt > 0.f && glm::length(walked) / dt > glm::length(pace)) { pace = walked / dt; }
        const float speed = glm::length(pace) / world->m2u;
        if(speed < 0.05f) { continue; }
        pace = glm::normalize(pace);
        const glm::vec3 lo = vec(e->v.absmin), hi = vec(e->v.absmax);
        for(size_t num = svs.maxclients + 1; num < world->slots.size(); num++)
        {
            const RagdollBodies* r = ragdollOf(static_cast<int>(num));
            if(!r || !world->slots[num].corpseDynamic) { continue; }
            for(int part = 0; part < r->count; part++)
            {
                const b3BodyId b = r->body[static_cast<za::SizeT>(part)];
                const b3AABB box = b3Body_ComputeAABB(b);
                const glm::vec3 a = world->toU(box.lowerBound), z = world->toU(box.upperBound);
                if(a.x > hi.x || z.x < lo.x || a.y > hi.y || z.y < lo.y || a.z > hi.z || z.z < lo.z - 2.f) { continue; }
                const float along = glm::dot(glmv(b3Body_GetLinearVelocity(b)), pace);
                const float target = za::min(speed * 0.3f, 0.6f);
                if(along < target)
                {
                    const float change = za::min(target - along, 3.f * dt);
                    b3Body_ApplyLinearImpulseToCenter(b, b3v(pace * (b3Body_GetMass(b) * change)), true);
                }
            }
        }
    }
}

// Before the step: each solid prop a player walked into this frame (VR_PlayerBumps: Quake's move stopped at its side)
// is shoved, as Source's player shadow shoves what it walks into: towards his pace (vr_box3d_player_push_speed at full
// stick) shared by their masses (vr_box3d_player_shove of his vr_box3d_player_mass, and its own: a light box goes at
// nearly his pace, a heavy one slower), with at most that share of his weight (a force: a prop too heavy for it to
// overcome the floor's friction doesn't move). His next move follows it.
void shoveBumped(float dt)
{
    const float share = vr_box3d_player_shove.value;
    const float mp = za::max(vr_box3d_player_mass.value, 1.f);
    const float g = -b3World_GetGravity(world->id).z;
    for(const World::Bump& b : world->bumps)
    {
        if(share <= 0.f || (!solidProp(b.num) && !pushableCorpse(b.num)))
        {
            continue;
        }
        const b3BodyId body = world->slots[b.num].body;
        const RagdollBodies* rag = ragdollOf(b.num); // (a ragdoll: shoved whole, each part its share)
        float mb = 0.f;
        for(int k = 0; k < (rag ? rag->count : 1); k++)
        {
            mb += b3Body_GetMass(rag ? rag->body[static_cast<za::SizeT>(k)] : body);
        }
        mb = za::max(mb, 0.1f);
        // His effort: his speed into it, against a frame's acceleration at full stick (Quake stopped him at it last
        // frame: what he has is what this frame's move gave him, less the ground's friction), up to all of it.
        const float full = za::max(0.5f * sv_accelerate.value * sv_maxspeed.value * dt, 1.f);
        const float u = za::max(vr_box3d_player_push_speed.value, 0.1f) * za::min(b.speed / full, 1.f);
        const float target = u * share * mp / (share * mp + mb); // (his shove's mass and its own)
        const float along = glm::dot(glmv(b3Body_GetLinearVelocity(body)), b.dir);
        if(target <= along)
        {
            continue;
        }
        const float impulse = za::min(mb * (target - along), share * mp * g * dt); // N s
        // (Low, a quarter of its height up, under its centre: the floor's friction, at its bottom, doesn't tip a tall box
        // over as pushed at its centre it did.)
        edict_t* ent = EDICT_NUM(b.num);
        const b3Pos centre = b3Body_GetWorldCenter(body);
        const glm::vec3 at{static_cast<float>(centre.x), static_cast<float>(centre.y), (ent->v.absmin[2] + 0.25f * (ent->v.absmax[2] - ent->v.absmin[2])) / world->m2u};
        if(rag)
        {
            for(int k = 0; k < rag->count; k++)
            {
                const b3BodyId part = rag->body[static_cast<za::SizeT>(k)];
                b3Body_ApplyLinearImpulse(part, b3v(b.dir * (impulse * b3Body_GetMass(part) / mb)), b3Body_GetWorldCenter(part), true);
            }
        }
        else
        {
            b3Body_ApplyLinearImpulse(body, b3v(b.dir * impulse), b3v(at), true);
        }
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: player %d walks into %d at %.0f u/s: shoved with %.1f N s (%.0f kg, to %.2f m/s)\n", b.player,
                b.num, b.speed, impulse, mb, target);
        }
    }
    world->bumps.clear();
}

[[nodiscard]] bool feetInTop(edict_t* p, edict_t* prop); // (below, with the players' shape against props)

// Before the step: a solid prop in a player's body (toppled onto him, shoved into him; its drawn shape in his box, less
// a unit), not the one he stands on nor one his feet are a little into the top of, is pushed out of it, away from his
// middle (never down), at 1 m/s at least; his moves aren't blocked by it meanwhile (VR_PropLetsOut). Source pushes a
// prop stuck in the player out likewise.
void unstickProps()
{
    if(!vr_box3d_player_unstick.value)
    {
        return;
    }
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        if(player->free || !svs.clients[i - 1].active || player->v.health <= 0.f ||
           static_cast<int>(player->v.movetype) == MOVETYPE_NOCLIP)
        {
            continue;
        }
        vec3_t mins, maxs;
        if(!VR_HullEntBox(player, player->v.mins, player->v.maxs, mins, maxs))
        {
            VectorCopy(player->v.mins, mins);
            VectorCopy(player->v.maxs, maxs);
        }
        const glm::vec3 lo = glm::vec3{mins[0], mins[1], mins[2]} + glm::vec3{1.f}, hi = glm::vec3{maxs[0], maxs[1], maxs[2]} - glm::vec3{1.f};
        const glm::vec3 origin = vec(player->v.origin);
        za::Array<b3Vec3, 8> corners;
        for(int k = 0; k < 8; k++)
        {
            corners[static_cast<size_t>(k)] = world->toM(glm::vec3{k & 1 ? hi.x : lo.x, k & 2 ? hi.y : lo.y, k & 4 ? hi.z : lo.z});
        }
        const b3ShapeProxy proxy{corners.data(), 8, 0.f};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = catPlayer;
        filter.maskBits = catSolid;
        struct Query
        {
            int ground;
            za::Array<int, 8> nums;
            int count;
        } q{standingOn(player), {}, 0};
        b3World_OverlapShape(world->id, world->toM(origin), &proxy, filter,
            [](b3ShapeId shape, void* context) {
                Query& oq = *static_cast<Query*>(context);
                const int num = numOf(shape);
                if(num != oq.ground && solidProp(num) && oq.count < static_cast<int>(oq.nums.size()) &&
                   za::find(oq.nums.begin(), oq.nums.begin() + oq.count, num) == oq.nums.begin() + oq.count)
                {
                    oq.nums[static_cast<size_t>(oq.count++)] = num;
                }
                return true;
            },
            &q);
        const glm::vec3 middle = origin + 0.5f * (glm::vec3{mins[0], mins[1], mins[2]} + glm::vec3{maxs[0], maxs[1], maxs[2]});
        for(int k = 0; k < q.count; k++)
        {
            if(vr_box3d_player_hold.value && feetInTop(player, EDICT_NUM(q.nums[static_cast<size_t>(k)])))
            {
                continue; // (he is on it: put back on top, SV_CheckStuck)
            }
            const b3BodyId body = world->slots[q.nums[static_cast<size_t>(k)]].body;
            glm::vec3 away = world->toU(b3Body_GetWorldCenter(body)) - middle;
            away.z = za::max(away.z, 0.f);
            away = glm::length(away) > 0.1f ? glm::normalize(away) : glm::vec3{0.f, 0.f, 1.f};
            const float along = glm::dot(glmv(b3Body_GetLinearVelocity(body)), away);
            if(along < unstickSpeed)
            {
                b3Body_ApplyLinearImpulseToCenter(body, b3v(away * (unstickSpeed - along) * b3Body_GetMass(body)), true);
            }
            if(vr_debug_box3d.value)
            {
                Con_Printf("box3d: %d in player %d's body: pushed out (%.2f %.2f %.2f)\n", q.nums[static_cast<size_t>(k)], i,
                    away.x, away.y, away.z);
            }
        }
    }
}

// How much of a player's weight (or jump) the prop under his feet takes, by how fast the point he stands on goes down
// (local, in the prop's body): all of it while it holds (at rest, rocking, sinking under him), less as it gives way,
// none once it drops at weightFadeSpeed (a box tipping over, or sliding off what held it, as a tilted box on a slope
// or on another prop does). His feet don't push what falls away under them: his weight on a tipping box made it land
// harder than it could by falling (a tall box's top lands at 11 m/s by itself, 14 blows it up; ROUND21.md, "Standing
// on props 3").
constexpr float weightFadeSpeed = 1.f; // m/s

[[nodiscard]] float underFeet(b3BodyId body, b3Vec3 local)
{
    const float down = -b3Body_GetLocalPointVelocity(body, local).z;
    return za::clamp(1.f - down / weightFadeSpeed, 0.f, 1.f);
}

// Each piece of the step: the weight of each player standing on a prop (vr_box3d_player_mass) where he stands. Not
// waking it: a prop asleep under him stays so (a stack at rest stays still). On a floating prop, at most half the
// prop's own, through its centre: the water lifts a prop by its weight, not its volume, and at its centre (beforeStep),
// so a player heavier than a box would sink it, and his weight on its top would capsize a tall one; so it floats nine
// tenths under, upright.
void pressStanding(float dt)
{
    // A jump's push, until its time is spent (beforeStanding). A force, like the weight: a prop on the floor is held up
    // by it, with no blow (a hit) on it; a floating one is pushed under.
    for(World::Stand& st : world->stands)
    {
        const b3BodyId pushed = standBody(st.pushed);
        if(st.pushLeft > 0.f && B3_IS_NON_NULL(pushed))
        {
            const float share = za::min(dt, st.pushLeft) / dt * underFeet(pushed, st.pushLocal);
            b3Body_ApplyForce(pushed, b3Vec3{0.f, 0.f, -st.pushForce * share}, b3Body_GetWorldPoint(pushed, st.pushLocal), true);
            st.pushLeft -= dt;
        }
        else
        {
            st.pushed = 0;
            st.pushLeft = 0.f;
        }
    }
    const float mass = vr_box3d_player_mass.value;
    if(mass <= 0.f)
    {
        return;
    }
    const float g = -b3World_GetGravity(world->id).z; // m/s^2
    for(const World::Stand& st : world->stands)
    {
        const b3BodyId body = standBody(st.ground);
        if(B3_IS_NON_NULL(body))
        {
            if(world->slots[st.ground].wet)
            {
                const float most = za::min(mass, 0.5f * b3Body_GetMass(body));
                b3Body_ApplyForceToCenter(body, b3Vec3{0.f, 0.f, -most * g}, false);
            }
            else
            {
                b3Body_ApplyForce(body, b3Vec3{0.f, 0.f, -mass * g * underFeet(body, st.local)}, world->toM(st.feet), false);
            }
        }
    }
}

// After the step: each player standing on a prop that moved is carried across with the point he stands on (its turn
// too, but not his view: VR), and up or down with the top of the prop's Quake box, which his box stands on (solidBox:
// round the prop as it turns, so a box tipping over lowers him with its box, not with the point, which goes down its
// side). His box is traced there, the world (and every other entity) stopping it, never from inside a wall; if the prop
// turned so that its box now holds him, he steps up out of it.
void rideStanding()
{
    for(int i = 1; i < static_cast<int>(world->stands.size()) && i < qcvm->num_edicts; i++)
    {
        World::Stand& st = world->stands[static_cast<size_t>(i)];
        const b3BodyId body = standBody(st.ground);
        if(B3_IS_NULL(body))
        {
            continue;
        }
        edict_t* player = EDICT_NUM(i);
        edict_t* ground = EDICT_NUM(st.ground);
        const glm::vec3 to = world->toU(b3Body_GetWorldPoint(body, st.local));
        // (Meeting its drawn shape, vr_box3d_player_shape, he stands on its face: up or down with the point.)
        const glm::vec3 move{to.x - st.feet.x, to.y - st.feet.y,
            vr_box3d_player_shape.value ? to.z - st.feet.z : ground->v.absmax[2] - st.top};
        const float distance = glm::length(move);
        if(distance < 0.01f || distance > 64.f)
        {
            continue;
        }
        const float solid = ground->v.solid;
        ground->v.solid = SOLID_NOT; // (the prop has moved already: the player's box meets it where it is now)
        vec3_t end;
        for(int k = 0; k < 3; k++)
        {
            end[k] = player->v.origin[k] + move[k];
        }
        const trace_t tr = SV_Move(player->v.origin, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player);
        ground->v.solid = solid;
        if(tr.allsolid || tr.startsolid)
        {
            continue;
        }
        VectorCopy(tr.endpos, player->v.origin);
        const trace_t in = SV_Move(player->v.origin, player->v.mins, player->v.maxs, player->v.origin, MOVE_NORMAL, player);
        if(in.startsolid && in.ent == ground)
        {
            vec3_t up;
            VectorCopy(player->v.origin, up);
            up[2] += 18.f; // (a step)
            const trace_t down = SV_Move(up, player->v.mins, player->v.maxs, player->v.origin, MOVE_NORMAL, player);
            if(!down.startsolid)
            {
                VectorCopy(down.endpos, player->v.origin);
            }
        }
        SV_LinkEdict(player, true);
        if(vr_debug_box3d.value >= 2.f)
        {
            Con_Printf("box3d: player %d carried %.2f %.2f %.2f by %d\n", i, move.x, move.y, move.z, st.ground);
        }
    }
}

// Players against solid props' shapes (vr_box3d_player_shape; ROUND21.md, "Standing on props 3"). Quake met a solid
// prop with the upright box round it (solidBox): a tilted box was taller and wider than drawn (landing on it, the player
// stood in the air over its face; jumping onto it, he met its box's top corner), and walking into a box turned 45
// degrees stopped him further from its face than from one lying with the grid. Now the player's own box meets the prop's
// drawn box as it is turned, and as a round column (a 16-sided prism: his width across its flats, his box's height,
// flat-bottomed as his box is), so he stops as far from a box's face turned any way as from a wall's (his box's
// half-width). The trace is the column's origin against the Minkowski sum of the prop's box and the column, as Quake
// traces its hulls: the half-spaces of every face normal and edge-pair normal of the two, entered and left along the
// move, stopped DIST_EPSILON short (Quake 3's brush trace).

constexpr int columnSides = 16;
constexpr double traceEpsilon = 0.03125; // DIST_EPSILON (world.c): how far short of the surface a trace stops

struct PropShape
{
    glm::dvec3 centre{0.0};      // the prop's drawn box: its centre, its axes, half its size
    za::Array<glm::dvec3, 3> axes{};
    glm::dvec3 half{0.0};
    za::Array<glm::dvec2, columnSides> ring{}; // the player's column round his origin: its corners across ...
    double bottom{0.0}, top{0.0};               // ... from his feet to his head
    za::Array<glm::dvec3, 2 * (4 + columnSides / 2 + 3 * (1 + columnSides / 2))> normals{};
    int count{0};
};

// Shots and missiles against solid props' shapes (vr_box3d_shot_shape; ROUND21.md, "Missiles meet a turned prop's
// shape"): a shot's or a missile's box (a point, mostly) swept against the prop's drawn box as it is turned, as Quake
// traces a box against a hull: the half-spaces of their Minkowski sum (the prop's 3 axes; with a box, the world's 3 and
// the 9 edge pairs too), exact and cheap (a point: 6 planes).
struct ShotShape
{
    glm::dvec3 centre{0.0}; // the prop's drawn box (less the mover's box's offset from its origin): its centre, axes, half its size
    za::Array<glm::dvec3, 3> axes{};
    glm::dvec3 half{0.0};
    glm::dvec3 ext{0.0}; // half the mover's box
    za::Array<glm::dvec3, 2 * 15> normals{};
    int count{0};
};

template <typename Shape>
void addNormal(Shape& p, glm::dvec3 n)
{
    const double length = glm::length(n);
    if(length < 1e-6)
    {
        return;
    }
    n /= length;
    for(int k = 0; k < 3; k++)
    {
        n[k] = za::abs(n[k]) < 1e-7 ? 0.0 : n[k]; // (a wall's normal is flat: Quake's step up needs normal z 0)
    }
    n = glm::normalize(n);
    p.normals[static_cast<size_t>(p.count++)] = n;
    p.normals[static_cast<size_t>(p.count++)] = -n;
}

// The prop `touch` (slot s) and the player's box (mins, maxs from his origin) as they meet.
[[nodiscard]] PropShape propShapeAt(const glm::vec3& origin, const glm::mat3& axes, const Slot& s, const float* mins, const float* maxs);

[[nodiscard]] PropShape propShape(edict_t* touch, const Slot& s, const float* mins, const float* maxs)
{
    return propShapeAt(vec(touch->v.origin), held::axesFromAngles(touch->v.angles, s.brush), s, mins, maxs);
}

// The same with the prop at origin, turned to axes (its body's pose: vr_physics_inside).
[[nodiscard]] PropShape propShapeAt(const glm::vec3& origin, const glm::mat3& axes, const Slot& s, const float* mins, const float* maxs)
{
    PropShape p;
    const glm::vec3 mid = (s.mins + s.maxs) * 0.5f;
    p.centre = glm::dvec3{origin} + glm::dvec3{axes * mid};
    p.half = glm::dvec3{(s.maxs - s.mins) * 0.5f};
    int up = 0;
    for(int k = 0; k < 3; k++)
    {
        p.axes[static_cast<size_t>(k)] = glm::normalize(glm::dvec3{axes[k]});
        if(za::abs(axes[k].z) > za::abs(axes[up].z))
        {
            up = k;
        }
    }
    // The column turned with the prop's sides (its yaw): an upright box's faces meet its flats, at exactly his half-width.
    const glm::dvec3 side = p.axes[static_cast<size_t>((up + 1) % 3)];
    const double yaw = za::atan2(side.y, side.x);
    const double r = 0.5 * za::min(maxs[0] - mins[0], maxs[1] - mins[1]);
    const double corner = r / za::cos(glm::pi<double>() / columnSides);
    const glm::dvec2 at{0.5 * (mins[0] + maxs[0]), 0.5 * (mins[1] + maxs[1])};
    for(int k = 0; k < columnSides; k++)
    {
        const double a = yaw + (k + 0.5) * 2.0 * glm::pi<double>() / columnSides;
        p.ring[static_cast<size_t>(k)] = at + corner * glm::dvec2{za::cos(a), za::sin(a)};
    }
    p.bottom = mins[2];
    p.top = maxs[2];
    const glm::dvec3 z{0.0, 0.0, 1.0};
    for(const glm::dvec3& a : p.axes)
    {
        addNormal(p, a);
        addNormal(p, glm::cross(a, z));
    }
    addNormal(p, z);
    for(int k = 0; k < columnSides / 2; k++)
    {
        const double a = yaw + k * 2.0 * glm::pi<double>() / columnSides;
        const glm::dvec3 flat{za::cos(a), za::sin(a), 0.0}, edge{-za::sin(a), za::cos(a), 0.0};
        addNormal(p, flat);
        for(const glm::dvec3& b : p.axes)
        {
            addNormal(p, glm::cross(b, edge));
        }
    }
    return p;
}

// How far the player's origin may go along n: the support of the prop's box less the column's (their Minkowski sum).
[[nodiscard]] double propSupport(const PropShape& p, const glm::dvec3& n)
{
    double box = glm::dot(n, p.centre);
    for(int k = 0; k < 3; k++)
    {
        box += p.half[k] * za::abs(glm::dot(n, p.axes[static_cast<size_t>(k)]));
    }
    double ring = -1e300;
    for(const glm::dvec2& v : p.ring)
    {
        ring = za::max(ring, -(n.x * v.x + n.y * v.y));
    }
    return box + ring + za::max(-n.z * p.bottom, -n.z * p.top);
}

// How far a shot's origin may go along n: the prop's box's support plus the shot's box's.
[[nodiscard]] double propSupport(const ShotShape& p, const glm::dvec3& n)
{
    double h = glm::dot(n, p.centre);
    for(int k = 0; k < 3; k++)
    {
        h += p.half[k] * za::abs(glm::dot(n, p.axes[static_cast<size_t>(k)])) + p.ext[k] * za::abs(n[k]);
    }
    return h;
}

// Quake 3's brush trace of the player's origin (or a shot's) from start to end against the sum's half-spaces. False:
// missed.
template <typename Shape>
[[nodiscard]] bool traceProp(const Shape& p, const glm::dvec3& start, const glm::dvec3& end, trace_t& trace)
{
    double enter = -1.0, leave = 1.0;
    bool startOut = false, getOut = false;
    glm::dvec3 normal{0.0, 0.0, 1.0};
    double dist = 0.0;
    for(int k = 0; k < p.count; k++)
    {
        const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
        const double h = propSupport(p, n);
        const double d1 = glm::dot(n, start) - h, d2 = glm::dot(n, end) - h;
        getOut = getOut || d2 > 0.0;
        startOut = startOut || d1 > 0.0;
        if(d1 > 0.0 && (d2 >= traceEpsilon || d2 >= d1))
        {
            return false; // (in front of this face all along)
        }
        if(d1 <= 0.0 && d2 <= 0.0)
        {
            continue;
        }
        if(d1 > d2)
        {
            const double f = (d1 - traceEpsilon) / (d1 - d2);
            if(f > enter)
            {
                enter = f;
                normal = n;
                dist = h;
            }
        }
        else
        {
            leave = za::min(leave, (d1 + traceEpsilon) / (d1 - d2));
        }
    }
    if(!startOut)
    {
        // Started inside: as Quake's hull trace, the move isn't cut (VR_PropLetsOut may let him out).
        trace.startsolid = true;
        trace.allsolid = !getOut;
        trace.fraction = 1.f;
        return true;
    }
    if(enter >= leave || enter <= -1.0)
    {
        return false;
    }
    const double f = za::max(enter, 0.0);
    trace.fraction = static_cast<float>(f);
    for(int k = 0; k < 3; k++)
    {
        trace.endpos[k] = static_cast<float>(start[k] + f * (end[k] - start[k]));
        trace.plane.normal[k] = static_cast<float>(normal[k]);
    }
    trace.plane.dist = static_cast<float>(dist);
    return true;
}

// The shortest distance from the player's upright centre line (origin, his box's height) to the prop's drawn box.
[[nodiscard]] double columnGap(const PropShape& p, const glm::dvec3& origin)
{
    double best = 1e300;
    for(int k = 0; k <= 32; k++)
    {
        const glm::dvec3 at{origin.x, origin.y, origin.z + p.bottom + (p.top - p.bottom) * k / 32.0};
        const glm::dvec3 d = at - p.centre;
        glm::dvec3 out{0.0};
        for(int i = 0; i < 3; i++)
        {
            const double t = glm::dot(d, p.axes[static_cast<size_t>(i)]);
            out[i] = za::max(za::abs(t) - p.half[i], 0.0);
        }
        best = za::min(best, glm::length(out));
    }
    return best;
}

// vr_physics_player: how far under the feet of player p the solid prop he stands on is, as drawn and as its upright
// box (with vr_box3d_player_shape he stands on the first; off, on the second).
void playerOverProp(edict_t* p)
{
    edict_t* g = PROG_TO_EDICT(p->v.groundentity);
    if(!world || !(static_cast<int>(p->v.flags) & FL_ONGROUND) || !solidProp(NUM_FOR_EDICT(g)))
    {
        return;
    }
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    const PropShape shape = propShape(g, world->slots[NUM_FOR_EDICT(g)], mins, maxs);
    trace_t tr{};
    tr.fraction = 1.f;
    const glm::dvec3 from{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    const bool hit = traceProp(shape, from, from - glm::dvec3{0.0, 0.0, 64.0}, tr);
    Con_Printf("vr_physics_player: on %d: its drawn shape %s under your feet, its upright box's top %.2f\n", NUM_FOR_EDICT(g),
        !hit ? "not" : tr.startsolid ? "in them" : va("%.2f units", tr.fraction * 64.f), p->v.origin[2] + p->v.mins[2] - g->v.absmax[2]);
}

// vr_physics_approach [number | classname]: the first player's box moved (SV_Move, as play moves it) at a solid prop
// (the first explosive box) from 16 directions round it, level, its feet 2 units over the prop's bottom: how far his
// centre stops from its drawn box, turned (vr_hull_approach for walls: his box's half-width).
void approach_f()
{
    if(!sv.active || svs.maxclients < 1 || !world)
    {
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
    edict_t* prop = list.empty() ? nullptr : list.front();
    if(!prop || !solidProp(NUM_FOR_EDICT(prop)))
    {
        Con_Printf("vr_physics_approach: no solid prop %s\n", Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
        return;
    }
    edict_t* p = EDICT_NUM(1);
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    const PropShape shape = propShape(prop, world->slots[NUM_FOR_EDICT(prop)], mins, maxs);
    const float reach = glm::length(glm::vec3{shape.half}) + 48.f;
    const float z = prop->v.absmin[2] - p->v.mins[2] + 2.f;
    int met = 0;
    double lo = 1e300, hi = -1e300;
    for(int k = 0; k < 16; k++)
    {
        const float a = static_cast<float>(k) * glm::pi<float>() / 8.f;
        vec3_t s{static_cast<float>(shape.centre.x) + za::cos(a) * reach, static_cast<float>(shape.centre.y) + za::sin(a) * reach, z};
        vec3_t e{static_cast<float>(shape.centre.x), static_cast<float>(shape.centre.y), z};
        if(SV_Move(s, p->v.mins, p->v.maxs, s, MOVE_NORMAL, p).startsolid)
        {
            continue;
        }
        const trace_t tr = SV_Move(s, p->v.mins, p->v.maxs, e, MOVE_NORMAL, p);
        if(tr.ent != prop)
        {
            Con_Printf("vr_physics_approach: from %.1f deg stopped by %s first\n", k * 22.5f,
                tr.ent ? PR_GetString(tr.ent->v.classname) : "nothing");
            continue;
        }
        const double gap = columnGap(shape, glm::dvec3{tr.endpos[0], tr.endpos[1], tr.endpos[2]});
        lo = za::min(lo, gap);
        hi = za::max(hi, gap);
        met++;
        if(vr_debug_box3d.value)
        {
            Con_Printf("vr_physics_approach: from %.1f deg: %.2f units, normal %.2f %.2f %.2f\n", k * 22.5f, gap,
                tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]);
        }
    }
    Con_Printf("vr_physics_approach: %d %s (angles %.1f %.1f %.1f): %d of 16 met it, your centre %.2f to %.2f units from its "
               "face (your half-width %.1f; its shape %s)\n",
        NUM_FOR_EDICT(prop), PR_GetString(prop->v.classname), prop->v.angles[0], prop->v.angles[1], prop->v.angles[2], met,
        met ? lo : 0.0, met ? hi : 0.0, 0.5f * (maxs[0] - mins[0]), vr_box3d_player_shape.value ? "as drawn" : "off: its box");
}

// vr_physics_inside [1 | 0]: a test's watch on players passing into solid props (ROUND21.md, "Phasing through a
// toppled box"). Each frame, a player's origin inside a solid prop's drawn box grown by his column (where none of his
// moves may go, vr_box3d_player_shape) is counted, and the first frame of each time in is printed.
struct InsideWatch
{
    bool on{false};
    int frames{0}, inside{0}, times{0}, longest{0};
    double deepest{0.0};
    za::Array<int, MAX_SCOREBOARD + 1> in{}, run{}; // (the prop each player is in, 0: none; for how many frames)
};
InsideWatch insideWatch;

// How deep the player's origin o is inside the sum (negative: outside, by that much along the nearest face).
[[nodiscard]] double insideDepth(const PropShape& p, const glm::dvec3& o)
{
    double depth = 1e300;
    for(int k = 0; k < p.count; k++)
    {
        const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
        depth = za::min(depth, propSupport(p, n) - glm::dot(n, o));
    }
    return depth;
}

// How deep a player's origin is inside solid prop `prop` (his box mins, maxs as he meets it; shaped: its drawn shape,
// else its Quake box), and the nearest way out (the face of their sum it is nearest; negative: outside by that much).
[[nodiscard]] double depthIn(edict_t* prop, const float* origin, const float* mins, const float* maxs, bool shaped, glm::dvec3& out)
{
    const glm::dvec3 o{origin[0], origin[1], origin[2]};
    double depth = 1e300;
    const auto face = [&](const glm::dvec3& n, double d) {
        if(d < depth)
        {
            depth = d;
            out = n;
        }
    };
    if(shaped)
    {
        const PropShape p = propShape(prop, world->slots[static_cast<size_t>(NUM_FOR_EDICT(prop))], mins, maxs);
        for(int k = 0; k < p.count; k++)
        {
            const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
            face(n, propSupport(p, n) - glm::dot(n, o));
        }
    }
    else
    {
        for(int k = 0; k < 3; k++) // (Quake's box against its box: their sum's six faces)
        {
            glm::dvec3 n{0.0};
            n[k] = 1.0;
            face(n, prop->v.origin[k] + prop->v.maxs[k] - mins[k] - o[k]);
            face(-n, o[k] - (prop->v.origin[k] + prop->v.mins[k] - maxs[k]));
        }
    }
    return depth;
}

// A player inside a solid prop with his feet only a little into its top: the nearest way out is up, through a face he
// would stand on, at most a step. He landed on it, or it rose or turned into his feet (rocking under him, woken by his
// weight), and is on it, not under it (ROUND21.md, "Phasing through a toppled box").
[[nodiscard]] bool feetInTop(edict_t* prop, const float* origin, const float* mins, const float* maxs, bool shaped)
{
    glm::dvec3 out{0.0};
    const double depth = depthIn(prop, origin, mins, maxs, shaped, out);
    return out.z > 0.7 && depth <= VR_StepSize(18.f);
}

// The same for player p where he is, with his box as his moves meet prop (vr_hull_ent_width).
[[nodiscard]] bool feetInTop(edict_t* p, edict_t* prop)
{
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs) || !VR_HullNarrowsAgainst(p, prop))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    return feetInTop(prop, p->v.origin, mins, maxs, vr_box3d_player_shape.value != 0.f);
}

void watchInside()
{
    if(!insideWatch.on || !world || !vr_box3d_player_shape.value)
    {
        return;
    }
    insideWatch.frames++;
    for(int num = 1; num <= svs.maxclients && num < qcvm->num_edicts; num++)
    {
        edict_t* p = EDICT_NUM(num);
        if(p->free || static_cast<int>(p->v.solid) != SOLID_SLIDEBOX || static_cast<int>(p->v.movetype) == MOVETYPE_NOCLIP)
        {
            continue;
        }
        vec3_t mins, maxs;
        if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
        {
            VectorCopy(p->v.mins, mins);
            VectorCopy(p->v.maxs, maxs);
        }
        int in = 0;
        double depth = 0.0, traced = 0.0;
        for(int g = svs.maxclients + 1; g < static_cast<int>(world->slots.size()) && g < qcvm->num_edicts && !in; g++)
        {
            edict_t* e = EDICT_NUM(g);
            if(!solidProp(g) || e->free)
            {
                continue;
            }
            // (Its body as it is, not the entity's origin and angles its shape is traced from.)
            const Slot& s = world->slots[static_cast<size_t>(g)];
            const b3WorldTransform xf = b3Body_GetTransform(s.body);
            const PropShape shape = propShapeAt(world->toU(xf.p), glm::mat3_cast(fromB3(xf.q)), s, mins, maxs);
            depth = insideDepth(shape, glm::dvec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]});
            in = depth > 0.01 ? g : 0;
            if(in)
            {
                traced = insideDepth(propShape(e, s, mins, maxs), glm::dvec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]});
            }
        }
        const size_t k = static_cast<size_t>(num);
        if(in)
        {
            insideWatch.inside++;
            insideWatch.deepest = za::max(insideWatch.deepest, depth);
            if(insideWatch.in[k] != in)
            {
                insideWatch.times++;
                edict_t* e = EDICT_NUM(in);
                edict_t* g = PROG_TO_EDICT(p->v.groundentity);
                Con_Printf("vr_physics_inside: %.2f s player %d in %d by %.2f (its traced shape: %.2f) at %.2f %.2f %.2f vel %.0f %.0f %.0f, "
                           "%s on %d; it at %.2f %.2f %.2f angles %.2f %.2f %.2f, %s\n",
                    qcvm->time, num, in, depth, traced, p->v.origin[0], p->v.origin[1], p->v.origin[2], p->v.velocity[0], p->v.velocity[1],
                    p->v.velocity[2], (static_cast<int>(p->v.flags) & FL_ONGROUND) ? "grounded" : "in the air", NUM_FOR_EDICT(g),
                    e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2],
                    world->slots[static_cast<size_t>(in)].asleep ? "asleep" : "awake");
            }
        }
        insideWatch.in[k] = in;
        insideWatch.run[k] = in ? insideWatch.run[k] + 1 : 0;
        insideWatch.longest = za::max(insideWatch.longest, insideWatch.run[k]);
    }
}

void inside_f()
{
    if(Cmd_Argc() > 1)
    {
        insideWatch = InsideWatch{};
        insideWatch.on = Q_atoi(Cmd_Argv(1)) != 0;
        Con_Printf("vr_physics_inside: watch %s\n", insideWatch.on ? "on" : "off");
        return;
    }
    Con_Printf("vr_physics_inside: %s: %d times in a prop, %d of %d frames (at most %d in a row), deepest %.2f units\n",
        insideWatch.on ? "on" : "off", insideWatch.times, insideWatch.inside, insideWatch.frames, insideWatch.longest,
        insideWatch.deepest);
}

} // namespace

// SV_FlyMove: a solid prop is ground to a player (vr_box3d_player_stand) as a brush is, where its face (met at `normal`)
// is no steeper than vr_box3d_player_slope. Steeper, he slides off it, not friction holding him: with Quake's limit (a
// normal's z over 0.7, 45.6 degrees), on a box tilted 33 degrees he stood, creeping down at 7 units a second, its own
// weight on it (ROUND21.md, "Sliding off steep boxes").
extern "C" int VR_Box3DSteps(void)
{
    return world ? world->steps : 0;
}

extern "C" int VR_StandsOn(edict_t* ent, edict_t* ground, const float* normal)
{
    if(!world)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(ground);
    // A corpse he meets (vr_corpse_collide_player): ground, as a step.
    if(num >= 1 && num <= svs.maxclients && g > svs.maxclients && g < static_cast<int>(world->slots.size()) &&
        world->slots[g].kind == Kind::Corpse && vr_corpse_collide_player.value > 0.f)
    {
        return 1;
    }
    if(!vr_box3d_player_stand.value)
    {
        return 0;
    }
    const float steepest = za::clamp(vr_box3d_player_slope.value, 0.f, 90.f);
    return num >= 1 && num <= svs.maxclients && g > svs.maxclients && g < static_cast<int>(world->slots.size()) &&
           world->slots[g].kind == Kind::Prop && static_cast<int>(ground->v.solid) == SOLID_BBOX &&
           normal[2] >= za::cos(glm::radians(steepest)) - 0.0001f;
}

// SV_ClipToLinks: a body's move (a player's, a monster's) and a corpse in its way (vr_corpse_collide_player, _monsters):
// the box it meets (from the corpse's origin) is its body's as it lies (its fitted shape's or its lying box, as turned) in
// its Quake box,
// no taller than a step (corpseStep: Step Over; a monster always) or than corpseTop (Solid). Not if the move starts in it
// (it fell or was pushed onto him, he stood where it came to lie): he walks out of it.
extern "C" int VR_CorpseBox(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    float* boxmins, float* boxmaxs)
{
    if(!world || !mover)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(touch);
    if(num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Corpse ||
        B3_IS_NULL(world->slots[num].body) || (corpseMode() == 0 && world->slots[num].ragdoll < 0))
    {
        return 0; // (a ragdoll is one whatever Corpse Collision is)
    }
    const int m = NUM_FOR_EDICT(mover);
    int how = 0;
    if(m >= 1 && m <= svs.maxclients)
    {
        how = static_cast<int>(vr_corpse_collide_player.value);
    }
    else if(hasFlag(mover, FL_MONSTER) && mover->v.health > 0.f && vr_corpse_collide_monsters.value)
    {
        how = 1;
    }
    if(how <= 0)
    {
        return 0;
    }
    const b3AABB box = b3Body_ComputeAABB(world->slots[num].body);
    const glm::vec3 origin = vec(touch->v.origin);
    // (Within its Quake box: what Quake's moves look for it in, SV_ClipToLinks's broad phase and its area's links. A
    // ragdoll's is its parts' box: writeRagdoll.)
    const bool rag = world->slots[num].ragdoll >= 0;
    const glm::vec3 lo = rag ? vec(touch->v.mins) : glm::max(world->toU(box.lowerBound) - origin, vec(touch->v.mins));
    glm::vec3 hi = rag ? vec(touch->v.maxs) : glm::min(world->toU(box.upperBound) - origin, vec(touch->v.maxs));
    hi.z = za::min(hi.z, lo.z + (how == 1 ? corpseStep : corpseTop));
    if(glm::any(glm::lessThanEqual(hi - lo, glm::vec3{1.f})))
    {
        return 0;
    }
    // His feet a little into its top (walking down a slope onto it; a probe of his box a unit lower): its top is at his
    // feet, he is on it.
    constexpr float in = 0.1f, onTop = 3.f; // units
    const float feet = start[2] + mins[2] - origin.z;
    if(feet < hi.z && feet >= hi.z - onTop)
    {
        hi.z = za::max(feet, lo.z + 1.f);
    }
    for(int i = 0; i < 3; i++)
    {
        if(start[i] + maxs[i] <= origin[i] + lo[i] + in || start[i] + mins[i] >= origin[i] + hi[i] - in)
        {
            store(lo, boxmins);
            store(hi, boxmaxs);
            return 1;
        }
    }
    return 0;
}

// SV_FlyMove: a player's move stopped by a solid prop's side (not its top: he stands on that) shoves it (shoveBumped).
extern "C" void VR_PlayerBumps(edict_t* ent, edict_t* other, const float* normal)
{
    if(!world || vr_box3d_player_shove.value <= 0.f)
    {
        return;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(other);
    if(num < 1 || num > svs.maxclients || (!solidProp(g) && !pushableCorpse(g)) || normal[2] < -0.7f)
    {
        return;
    }
    // (Not the one he stands on: walking into a steep part of it, a tilted box's upper end, he would shove it from under
    // himself, and ride it on.)
    if(num < static_cast<int>(world->stands.size()) && world->stands[static_cast<size_t>(num)].ground == g)
    {
        return;
    }
    glm::vec3 dir{-normal[0], -normal[1], 0.f};
    if(glm::length(dir) < 0.3f)
    {
        return;
    }
    dir = glm::normalize(dir);
    const float speed = ent->v.velocity[0] * dir.x + ent->v.velocity[1] * dir.y;
    if(speed < 1.f)
    {
        return;
    }
    for(World::Bump& b : world->bumps)
    {
        if(b.player == num && b.num == g)
        {
            if(speed > b.speed)
            {
                b.dir = dir;
                b.speed = speed;
            }
            return;
        }
    }
    world->bumps.pushBack({num, g, dir, speed});
}

// A prop wedged in the level (its box still in it 2 units inside its surface: Box3D can't push it out, of the level nor
// of a player in it). A player in one isn't held (VR_PropLetsOut): it would hold him for good. (The big box toppled
// onto the player against a wall stayed 9 units in it a time in four, the player under it walking on the spot; ROUND21.md,
// "Props regressions".)
[[nodiscard]] bool wedgedInLevel(int num)
{
    if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[static_cast<size_t>(num)];
    glm::vec3 lo, hi;
    qvr::box3d::ownBox(s, lo, hi);
    if(B3_IS_NULL(s.body) || hi.x <= lo.x)
    {
        return false;
    }
    const edict_t* ent = EDICT_NUM(num);
    return qvr::box3d::boxInLevel(lo, hi, vec(ent->v.origin), turnOf(ent->v.angles, s.brush), 2.f);
}

// SV_ClipToLinks: a player's move that starts inside a solid prop's box (toppled onto him) goes on, out of it; not in the
// one he stands on (riding it, rideStanding steps him up out of it), nor one his feet are a little into the top of
// (feetInTop: landed on it, or it rocked up into him; he's held, and SV_CheckStuck puts him back on top); and only a
// move that takes him no deeper into it (vr_box3d_player_hold). Letting every move through, he fell into a box that
// rocked up into his feet, or a side that rocked into him, and went on through it: "Phasing through a toppled box",
// ROUND21.md.
extern "C" int VR_PropLetsOut(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* end, int shaped)
{
    if(!world || !vr_box3d_player_unstick.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover), g = NUM_FOR_EDICT(touch);
    if(num < 1 || num > svs.maxclients || !solidProp(g) ||
        ((static_cast<int>(mover->v.flags) & FL_ONGROUND) && PROG_TO_EDICT(mover->v.groundentity) == touch))
    {
        return 0;
    }
    if(vr_box3d_player_hold.value)
    {
        vec3_t entmins, entmaxs;
        const bool narrow = VR_HullEntBox(mover, mins, maxs, entmins, entmaxs) && VR_HullNarrowsAgainst(mover, touch);
        const float* m = narrow ? entmins : mins;
        const float* M = narrow ? entmaxs : maxs;
        glm::dvec3 out{0.0}, to{0.0};
        const double depth = depthIn(touch, start, m, M, shaped != 0, out);
        if(depthIn(touch, end, m, M, shaped != 0, to) > depth + 0.01 && !wedgedInLevel(g))
        {
            return 0; // (deeper: held, the move is stopped)
        }
        if(out.z > 0.7 && depth <= VR_StepSize(18.f)) // (feetInTop)
        {
            return 0;
        }
    }
    return 1;
}

// SV_ClipToLinks, Quake's owner rule (an entity's traces pass its own missiles): a player's traces (his moves, his shots,
// his hands') meet a solid prop even when it is his (vr_box3d_player_hold). A box let go of fast enough to be a throw
// (VR_Carry_Release; a heavy one at a gentle 1.6 m/s: a two-handed topple let go of on its way down) was his from then
// on, and he walked through it until he took it again (the author's note vrfiringrange_2026-10-01_00-33). Starting
// inside it (let go of against his body) he is let out as from any solid prop (VR_PropLetsOut), and it is pushed out of
// him (unstickProps).
extern "C" int VR_OwnPropMeets(edict_t* mover, edict_t* touch)
{
    if(!world || !vr_box3d_player_hold.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover);
    return num >= 1 && num <= svs.maxclients && solidProp(NUM_FOR_EDICT(touch));
}

// SV_ClipToLinks: a player's own box (his move's; mins, maxs) against a solid prop meets its drawn box as it is turned,
// as a round column (boxmins, boxmaxs: his box against entities, vr_hull_ent_width). See propShape.
extern "C" int VR_PropClip(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* boxmins, const float* boxmaxs, const float* end, trace_t* trace)
{
    if(!world || !vr_box3d_player_shape.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover), g = NUM_FOR_EDICT(touch);
    const auto same = [](const float* a, const float* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; };
    if(num < 1 || num > svs.maxclients || !solidProp(g) || !same(mins, mover->v.mins) || !same(maxs, mover->v.maxs) ||
       boxmaxs[0] - boxmins[0] <= 0.f)
    {
        return 0;
    }
    const PropShape p = propShape(touch, world->slots[g], boxmins, boxmaxs);
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.f;
    VectorCopy(end, trace->endpos);
    if(traceProp(p, glm::dvec3{start[0], start[1], start[2]}, glm::dvec3{end[0], end[1], end[2]}, *trace))
    {
        trace->ent = touch;
    }
    trace->inopen = !trace->allsolid;
    return 1;
}

namespace
{

// Whether `mover` is a flying or thrown thing: a rocket, a nail, a grenade, a laser, a Vore pod, the grappling hook, a gib
// (MOVETYPE_FLY, FLYMISSILE, BOUNCE, TOSS, GIB); not a player or a monster (in the air), nor a physics body (Box3D's: a
// prop, one held, a pickup's fixture).
[[nodiscard]] bool flyingThing(edict_t* mover)
{
    if(!mover || mover == qcvm->edicts || (static_cast<int>(mover->v.flags) & (FL_CLIENT | FL_MONSTER)))
    {
        return false;
    }
    const int m = static_cast<int>(mover->v.movetype);
    if(m != MOVETYPE_FLY && m != MOVETYPE_FLYMISSILE && m != MOVETYPE_BOUNCE && m != MOVETYPE_TOSS && m != MOVETYPE_GIB)
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(mover);
    if(num < static_cast<int>(world->slots.size()))
    {
        const Kind k = world->slots[static_cast<size_t>(num)].kind;
        return k != Kind::Prop && k != Kind::Held && k != Kind::Fixture;
    }
    return true;
}

// Whether a trace or move (SV_Move's type, with its flags) by `mover` is a shot's or a missile's: a missile's move
// (MOVE_MISSILE, or any of a flying thing's), or a gun's or the grappling hook's trace (MOVE_HITMODEL of their class, or
// MOVE_HITGIBS alone: the pellets, the lightning, the burst rifle, the laser cannon). Not a melee blow's or a thrown
// weapon's trace (their classes): those keep the box.
[[nodiscard]] bool shotMove(edict_t* mover, int type)
{
    if((type & ~(MOVE_HITGIBS | MOVE_HITMODEL | MOVE_HITMODEL_CLASS)) == MOVE_MISSILE || flyingThing(mover))
    {
        return true;
    }
    if(type & MOVE_HITMODEL)
    {
        return ((type & MOVE_HITMODEL_CLASS) >> MOVE_HITMODEL_CLASS_SHIFT) <= 1; // (guns 0, the grappling hook 1)
    }
    return (type & MOVE_HITGIBS) != 0;
}

// The shot's box (mins, maxs from its origin) against prop `touch` (slot s) as it stands.
[[nodiscard]] ShotShape shotShape(const edict_t* touch, const Slot& s, const float* mins, const float* maxs)
{
    ShotShape p;
    const glm::mat3 axes = held::axesFromAngles(touch->v.angles, s.brush);
    const glm::vec3 mid = (s.mins + s.maxs) * 0.5f;
    const glm::dvec3 offset{0.5 * (mins[0] + maxs[0]), 0.5 * (mins[1] + maxs[1]), 0.5 * (mins[2] + maxs[2])};
    p.centre = glm::dvec3{vec(touch->v.origin)} + glm::dvec3{axes * mid} - offset;
    p.half = glm::dvec3{(s.maxs - s.mins) * 0.5f};
    p.ext = glm::dvec3{0.5 * (maxs[0] - mins[0]), 0.5 * (maxs[1] - mins[1]), 0.5 * (maxs[2] - mins[2])};
    for(int k = 0; k < 3; k++)
    {
        p.axes[static_cast<size_t>(k)] = glm::normalize(glm::dvec3{axes[k]});
        addNormal(p, p.axes[static_cast<size_t>(k)]);
    }
    if(p.ext.x > 0.0 || p.ext.y > 0.0 || p.ext.z > 0.0)
    {
        for(int w = 0; w < 3; w++)
        {
            glm::dvec3 e{0.0};
            e[w] = 1.0;
            addNormal(p, e);
            for(const glm::dvec3& a : p.axes)
            {
                addNormal(p, glm::cross(a, e)); // (parallel ones are skipped)
            }
        }
    }
    return p;
}

} // namespace

// SV_ClipToLinks: a shot or a missile (shotMove: rockets, nails, grenades, lasers, Vore pods, the grappling hook; the
// guns' traces) against a solid prop (an explosive box) meets its drawn box as it is turned, not Quake's box round it: a
// turned box's empty corners let it by, and it is met at its real surface (vr_box3d_shot_shape; the flying hook always,
// as before it: ROUND21.md, the grapple5 section).
extern "C" int VR_PropShotClip(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* end, int type, trace_t* trace)
{
    if(!world)
    {
        return 0;
    }
    const int g = NUM_FOR_EDICT(touch);
    if(!solidProp(g))
    {
        return 0;
    }
    const bool point = mins[0] == 0.f && mins[1] == 0.f && mins[2] == 0.f && maxs[0] == 0.f && maxs[1] == 0.f && maxs[2] == 0.f;
    const bool hook = point && mover && fieldFloatOr(mover, fields().vr_hitclass, 0.f) == 1.f && flyingThing(mover);
    if(!hook && (!vr_box3d_shot_shape.value || !shotMove(mover, type)))
    {
        return 0;
    }
    const ShotShape p = shotShape(touch, world->slots[static_cast<size_t>(g)], mins, maxs);
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.f;
    VectorCopy(end, trace->endpos);
    if(traceProp(p, glm::dvec3{start[0], start[1], start[2]}, glm::dvec3{end[0], end[1], end[2]}, *trace))
    {
        trace->ent = touch;
    }
    trace->inopen = !trace->allsolid;
    return 1;
}

// SV_PushEntity, a flying thing's move that met `other` (vr_debug_missiles): what, where, the surface's normal; a solid
// prop: its drawn box as it stands (centre, half sizes, axes).
extern "C" void VR_MissileHitDebug(edict_t* ent, edict_t* other, const trace_t* trace)
{
    if(!vr_debug_missiles.value || !world || !flyingThing(ent))
    {
        return;
    }
    Con_Printf("missile hit: %s %d hit %s %d at %.2f %.2f %.2f normal %.3f %.3f %.3f\n", PR_GetString(ent->v.classname),
        NUM_FOR_EDICT(ent), other == qcvm->edicts ? "the world" : PR_GetString(other->v.classname), NUM_FOR_EDICT(other),
        static_cast<double>(trace->endpos[0]), static_cast<double>(trace->endpos[1]), static_cast<double>(trace->endpos[2]),
        static_cast<double>(trace->plane.normal[0]), static_cast<double>(trace->plane.normal[1]),
        static_cast<double>(trace->plane.normal[2]));
    const int g = NUM_FOR_EDICT(other);
    if(solidProp(g))
    {
        const float zero[3]{0.f, 0.f, 0.f};
        const ShotShape p = shotShape(other, world->slots[static_cast<size_t>(g)], zero, zero);
        Con_Printf("missile hit: prop %d centre %.3f %.3f %.3f half %.3f %.3f %.3f\n", g, p.centre.x, p.centre.y, p.centre.z,
            p.half.x, p.half.y, p.half.z);
        for(int k = 0; k < 3; k++)
        {
            const glm::dvec3& a = p.axes[static_cast<size_t>(k)];
            Con_Printf("missile hit: prop %d axis %d %.5f %.5f %.5f\n", g, k, a.x, a.y, a.z);
        }
    }
}

namespace
{

// vr_physics_shotbench [<count>]: the cost of a shot's or a missile's clip against the first solid prop (vr_box3d_shot_shape):
// `count` lines (100000 by default) across its box round it, each as a point's shape clip alone, and as a missile's
// whole trace (SV_Move, MOVE_MISSILE: the world and every entity near it) with the option on and off. For tests.
void shotBench_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_physics_shotbench: no physics world\n");
        return;
    }
    const VmScope vm;
    int g = 0;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts && !g; i++)
    {
        g = solidProp(i) ? i : 0;
    }
    if(!g)
    {
        Con_Printf("vr_physics_shotbench: no solid prop\n");
        return;
    }
    edict_t* prop = EDICT_NUM(g);
    const int count = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 100000;
    // The lines: from 64 units outside its box round it, through a point inside that box, on (a fixed sequence).
    const glm::vec3 lo = vec(prop->v.absmin), hi = vec(prop->v.absmax), mid = (lo + hi) * 0.5f;
    const auto line = [&](int i, vec3_t a, vec3_t b) {
        const float u = static_cast<float>((i * 7919) % 1000) / 1000.f, v = static_cast<float>((i * 104729) % 1000) / 1000.f;
        const float yaw = static_cast<float>(i) * 2.399963f;
        const glm::vec3 through = lo + (hi - lo) * glm::vec3{u, v, 0.5f * (u + v)};
        const glm::vec3 dir{za::cos(yaw), za::sin(yaw), 0.3f * (v - 0.5f)};
        const float reach = glm::length(hi - mid) + 64.f;
        for(int k = 0; k < 3; k++)
        {
            a[k] = through[k] - dir[k] * reach;
            b[k] = through[k] + dir[k] * reach;
        }
    };
    const float zero[3]{0.f, 0.f, 0.f};
    int hits = 0;
    double t0 = Sys_DoubleTime();
    for(int i = 0; i < count; i++)
    {
        vec3_t a, b;
        line(i, a, b);
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        const ShotShape p = shotShape(prop, world->slots[static_cast<size_t>(g)], zero, zero);
        hits += traceProp(p, glm::dvec3{a[0], a[1], a[2]}, glm::dvec3{b[0], b[1], b[2]}, tr) ? 1 : 0;
    }
    const double clip = (Sys_DoubleTime() - t0) * 1e9 / count;
    const float saved = vr_box3d_shot_shape.value;
    double move[2]{0.0, 0.0};
    int met[2]{0, 0};
    for(int on = 0; on < 2; on++)
    {
        Cvar_SetValueQuick(&vr_box3d_shot_shape, static_cast<float>(on));
        t0 = Sys_DoubleTime();
        for(int i = 0; i < count; i++)
        {
            vec3_t a, b, m{0.f, 0.f, 0.f};
            line(i, a, b);
            met[on] += SV_Move(a, m, m, b, MOVE_MISSILE, nullptr).ent == prop ? 1 : 0;
        }
        move[on] = (Sys_DoubleTime() - t0) * 1e9 / count;
    }
    Cvar_SetValueQuick(&vr_box3d_shot_shape, saved);
    Con_Printf("vr_physics_shotbench: prop %d, %d lines: a point's shape clip %.0f ns (%d hit its shape); a missile's whole "
               "trace %.0f ns with the shape (%d met it), %.0f ns with its box (%d met it)\n",
        g, count, clip, hits, move[1], met[1], move[0], met[0]);
}

// vr_physics_fire <kind> [<x> <y> <z>]: a missile or a shot of yours (QC's VR_Test_Fire), from your eyes at the world
// point, or at the middle of the nearest solid prop: 0 a rocket, 1 a nail, 2 a grenade, 3 a super nail, 4 an enforcer's
// laser, 10 a shotgun pellet (the burst rifle's, the lightning's trace). vr_debug_missiles prints where a missile hits. For
// tests (shots against a turned prop's shape).
void fire_f()
{
    if(!sv.active || Cmd_Argc() < 2 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_fire <kind> [<x> <y> <z>]\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    dfunction_t* fn = nullptr;
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        fn = !strcmp(PR_GetString(qcvm->functions[i].s_name), "VR_Test_Fire") ? &qcvm->functions[i] : nullptr;
    }
    if(!fn || player->free)
    {
        Con_Printf("vr_physics_fire: no VR_Test_Fire in the progs, or no player\n");
        return;
    }
    glm::vec3 at{0.f};
    if(Cmd_Argc() >= 5)
    {
        at = glm::vec3{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    }
    else
    {
        float best = 1e30f;
        for(int i = svs.maxclients + 1; world && i < qcvm->num_edicts; i++)
        {
            if(!solidProp(i))
            {
                continue;
            }
            const float zero[3]{0.f, 0.f, 0.f};
            const glm::vec3 c{shotShape(EDICT_NUM(i), world->slots[static_cast<size_t>(i)], zero, zero).centre};
            const float d = glm::distance(c, vec(player->v.origin));
            if(d < best)
            {
                best = d;
                at = c;
            }
        }
        if(best >= 1e30f)
        {
            Con_Printf("vr_physics_fire: no solid prop to fire at\n");
            return;
        }
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM0) = static_cast<float>(Q_atof(Cmd_Argv(1)));
    G_FLOAT(OFS_PARM1 + 0) = at.x;
    G_FLOAT(OFS_PARM1 + 1) = at.y;
    G_FLOAT(OFS_PARM1 + 2) = at.z;
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
    edict_t* shot = PROG_TO_EDICT(G_INT(OFS_RETURN));
    if(shot != qcvm->edicts)
    {
        Con_Printf("vr_physics_fire: %s %d from %.3f %.3f %.3f vel %.3f %.3f %.3f\n", PR_GetString(shot->v.classname),
            NUM_FOR_EDICT(shot), static_cast<double>(shot->v.origin[0]), static_cast<double>(shot->v.origin[1]),
            static_cast<double>(shot->v.origin[2]), static_cast<double>(shot->v.velocity[0]),
            static_cast<double>(shot->v.velocity[1]), static_cast<double>(shot->v.velocity[2]));
    }
}

// vr_shock_ragdoll_check: each ragdoll's shock (left: 1 fresh .. 0), its limbs' turning speed relative to their parents
// (mean, rad/s), its fastest part (units/s), its joints' worst separation (units: stretched limbs) and where its pelvis is.
// For tests (vr_shock.qc).
void shockCheck_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_shock_ragdoll_check: no game\n");
        return;
    }
    const VmScope vm;
    int count = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        float relative = 0.f, fastest = 0.f, separation = 0.f;
        int limbs = 0;
        for(int b = 0; b < r.count; ++b)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            fastest = za::max(fastest, glm::length(glmv(b3Body_GetLinearVelocity(body))) * world->m2u);
            const int parent = r.rig->bones[b].parent;
            if(parent >= 0 && !partCut(r, parent) && r.rig->bones[b].joint != ragdoll::Joint::Loose)
            {
                relative += glm::length(glmv(b3Body_GetAngularVelocity(body)) -
                                        glmv(b3Body_GetAngularVelocity(r.body[static_cast<za::SizeT>(parent)])));
                ++limbs;
            }
            b3JointId joints[8];
            const int n = b3Body_GetJoints(body, joints, 8);
            for(int j = 0; j < n; ++j)
            {
                separation = za::max(separation, b3Joint_GetLinearSeparation(joints[j]) * world->m2u);
            }
        }
        const glm::vec3 pelvis = world->toU(b3Body_GetWorldCenter(r.body[0]));
        Con_Printf("shockcheck: entity=%d left=%.2f limbs=%d relspin=%.2f fastest=%.0f stretch=%.2f pelvis=%.0f %.0f %.0f\n",
            r.num, shockLeft(EDICT_NUM(r.num)), limbs, limbs ? relative / static_cast<float>(limbs) : 0.f, fastest,
            separation, pelvis.x, pelvis.y, pelvis.z);
        ++count;
    }
    Con_Printf("shockcheck: ragdolls=%d\n", count);
}

// vr_knockdown_test <mode>: QC's VR_Knockdown_Test, as the first player: 0 knocks the nearest monster down (whatever the
// chance), 1 gets the knocked-down ones up now, 2 hits the nearest knocked-down one, 3 kills it, 4 gibs it, 5 lists the
// monsters. For tests.
void knockdownTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_knockdown_test <mode> (in a game)\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    dfunction_t* fn = nullptr;
    if(Cmd_Argc() >= 2 && Q_atof(Cmd_Argv(1)) == 9 && world)
    {
        float angular = 0.f;
        float deflection = 0.f;
        int bodies = 0;
        for(const RagdollBodies& r : world->ragdolls)
        {
            if(r.num <= 0 || !knockedDown(EDICT_NUM(r.num)) || EDICT_NUM(r.num)->v.health <= 0.f) { continue; }
            for(int b = 0; b < r.count; ++b)
            {
                if(partCut(r, b)) { continue; }
                angular += glm::length(glmv(b3Body_GetAngularVelocity(r.body[static_cast<za::SizeT>(b)])));
                const ragdoll::Bone& bone = r.rig->bones[b];
                if(r.struggleReady && bone.parent >= 0 && !partCut(r, bone.parent) &&
                    (bone.role == modelmeta::BoneRole::Chest || bone.role == modelmeta::BoneRole::Head))
                {
                    const glm::quat relative = glm::inverse(fromB3(b3Body_GetTransform(r.body[bone.parent]).q)) *
                        fromB3(b3Body_GetTransform(r.body[b]).q);
                    const glm::quat delta = glm::normalize(relative * glm::inverse(r.struggleRest[b]));
                    deflection = za::max(deflection, glm::degrees(2.f * glm::acos(za::clamp(za::fabs(delta.w), 0.f, 1.f))));
                }
                ++bodies;
            }
        }
        Con_Printf("wigglecheck: strength=%.2f parts=%d angular=%.4f rad/s\n", vr_knockdown_wiggle.value,
            bodies, bodies ? angular / bodies : 0.f);
        Con_Printf("strugglecheck: strength=%.2f deflection=%.3f degrees\n", vr_knockdown_wiggle.value, deflection);
        return;
    }
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        fn = !strcmp(PR_GetString(qcvm->functions[i].s_name), "VR_Knockdown_Test") ? &qcvm->functions[i] : nullptr;
    }
    if(!fn || player->free)
    {
        Con_Printf("vr_knockdown_test: no VR_Knockdown_Test in the progs, or no player\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM0) = Cmd_Argc() >= 2 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 0.f;
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
}

} // namespace

extern "C" int VR_PushSkips(edict_t* ent)
{
    if(!world)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    return num < static_cast<int>(world->slots.size()) &&
           (world->slots[num].kind == Kind::Prop || (world->slots[num].kind == Kind::Corpse && world->slots[num].corpseDynamic));
}

extern "C" void VR_PhysicsFrameEnd(void)
{
    registerCommands();
    if(!wanted())
    {
        destroyWorld();
        return;
    }
    const float dt = static_cast<float>(host_frametime);
    if(dt <= 0.f)
    {
        return;
    }
    QVR_PROFILE("box3d");
    if(!world || world->map != sv.worldmodel || world->generation != worldGeneration() || world->m2u != units::metresToUnits() ||
       !meshCurrent(sv.worldmodel, world->m2u, world->mesh))
    {
        const double b0 = Sys_DoubleTime();
        destroyWorld();
        buildWorld();
        if(vr_debug_box3d.value || developer.value)
        {
            Con_Printf("box3d: world built in %.2f ms\n", (Sys_DoubleTime() - b0) * 1000.0);
        }
    }
    const double t0 = Sys_DoubleTime();
    World::FrameSample sample;
    updateSettings();
    {
        QVR_PROFILE("box3d sync");
        syncEntities(dt);
        syncHands(dt);
        syncRagdollGrabs(dt);
        if(!world->ragdollsInside.empty())
        {
            pruneRagdollsInside();
        }
        noteThrows();
        syncReach(dt);
        syncPortalCopies(dt);
    }
    const double tSync = Sys_DoubleTime();
    {
        QVR_PROFILE("box3d water and hits");
        beforeStep(dt);
        beforeStanding();
        shoveBumped(dt);
        nudgeWalkedRagdolls(dt);
        unstickProps();
    }
    const double t1 = Sys_DoubleTime();
    const int awakeBefore = b3World_GetAwakeBodyCount(world->id);

    // Box3D's step, in pieces of at most 1/45 s (a slow server frame).
    za::Vector<za::Pair<int, int>>& impacts = world->impacts;
    impacts.clear();
    world->shocks.clear();
    // (At most three: after a hitch (a level's load, a saved game, a slow frame; Quake's frame time is at most a tenth of
    // a second) the step catches up in pieces of up to 1/30 s rather than adding more steps to the slow frame.)
    const int pieces = CLAMP(1, static_cast<int>(za::ceil(dt * 45.f - 0.01f)), 3);
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    {
        QVR_PROFILE("box3d step");
        for(int i = 0; i < pieces; i++)
        {
            if(i > 0)
            {
                liftAgain(); // (Box3D clears the forces after each step)
            }
            feedPortalCopies();
            notePushed(dt / static_cast<float>(pieces));
            pressStanding(dt / static_cast<float>(pieces));
            world->tasks.next.storeRelaxed(0);
            const double s0 = Sys_DoubleTime();
            b3World_Step(world->id, dt / static_cast<float>(pieces), substeps);
            sample.ms[PhaseSolver] += static_cast<float>((Sys_DoubleTime() - s0) * 1000.0);
            addProfile(world->stepProfile, b3World_GetProfile(world->id));
            finishPortalCopies();
            limitPushes(dt / static_cast<float>(pieces));
            world->steps++;
            touches(impacts);
            traceGibContacts();
        }
    }
    carryRagdolls(); // (through slipgates)
    carryCorpses();
    const double t2 = Sys_DoubleTime();
    world->stepTime += t2 - t1;
    world->stepTimeMax = za::max(world->stepTimeMax, t2 - t1);
    world->stepFrames++;
    world->stepAwake += static_cast<za::U64>(b3World_GetAwakeBodyCount(world->id));
    if(world->stepSamples.size() < stepSamplesMax)
    {
        world->stepSamples.pushBack(World::StepSample{static_cast<float>((t2 - t1) * 1000.0), awakeBefore});
    }
    // Slow frames (over a millisecond; vr_debug_box3d 3: over 0.2) with Box3D's own profile and counts.
    if((vr_debug_box3d.value || developer.value) && (t2 - t0) * 1000.0 > (vr_debug_box3d.value >= 3.f ? 0.2 : 1.0))
    {
        const b3Counters c = b3World_GetCounters(world->id);
        const b3Profile p = b3World_GetProfile(world->id);
        Con_Printf("box3d: slow frame %.2f ms (sync and water %.2f, step %.2f: pairs %.2f collide %.2f solve %.2f [setup %.2f "
                   "constraints %.2f split %.2f sleep %.2f transforms %.2f refit %.2f] continuous %.2f), %d bodies (%d awake), %d "
                   "contacts, %d pieces\n",
            (t2 - t0) * 1000.0, (t1 - t0) * 1000.0, (t2 - t1) * 1000.0, p.pairs, p.collide, p.solve, p.solverSetup, p.constraints,
            p.splitIslands, p.sleepIslands, p.transforms, p.refit, p.bullets, c.bodyCount, b3World_GetAwakeBodyCount(world->id),
            c.contactCount, pieces);
    }

    // The props into their entities (the awake ones, and those that just fell asleep), and the awake ones' slides (the
    // physics sounds' scrapes).
    QVR_PROFILE("box3d write");
    const double tWrite = Sys_DoubleTime();
    updateRecoveries(); // (knocked-down monsters getting up: Knockdowns)
    const bool scrapes = physsound::scrapesWanted();
    const bool bodyScrapes = scrapes && vr_physsound_bodies.value > 0.f;
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
        if(s.ragdoll >= 0)
        {
            writeRagdoll(EDICT_NUM(num), s); // (every frame: a part may be awake in an island of its own, his shotgun)
            if(bodyScrapes && s.ragdoll >= 0 && !EDICT_NUM(num)->free)
            {
                noteBodySlide(num, s, dt / static_cast<float>(pieces));
            }
            continue;
        }
        if(s.kind == Kind::Corpse && s.corpseDynamic && (!s.asleep || b3Body_IsAwake(s.body)))
        {
            writeCorpse(EDICT_NUM(num), s);
            if(bodyScrapes && !s.asleep && !EDICT_NUM(num)->free)
            {
                noteBodySlide(num, s, dt / static_cast<float>(pieces));
            }
            continue;
        }
        if(s.kind == Kind::Prop && (!s.asleep || b3Body_IsAwake(s.body)))
        {
            edict_t* ent = EDICT_NUM(num);
            writeProp(ent, s);
            if(ent->free)
            {
                destroyBody(s);
            }
            else if(scrapes && !s.asleep && s.sound != physsound::Material::None)
            {
                noteSlide(num, s, dt / static_cast<float>(pieces));
            }
        }
    }
    rideStanding();
    const double tAfter = Sys_DoubleTime();

    // Then what they touched, in the step's order, each pair once.
    for(size_t i = 0; i < impacts.size(); i++)
    {
        const auto [a, b] = impacts[i];
        if(za::find(impacts.begin(), impacts.begin() + static_cast<za::PtrDiffT>(i), impacts[i]) != impacts.begin() + static_cast<za::PtrDiffT>(i))
        {
            continue;
        }
        edict_t* ea = EDICT_NUM(a);
        edict_t* eb = EDICT_NUM(b);
        if(ea->free || eb->free)
        {
            continue;
        }
        if(world->slots[a].kind == Kind::Prop && world->slots[b].kind == Kind::Mover)
        {
            // A prop meeting a door, a lift or a button: as two props below, its touch sees the speed it came at, not
            // what the bounce left (a rock thrown at a wall button presses it from vr_button_throw_speed: QC buttons.qc).
            const glm::vec3 after = vec(ea->v.velocity), before = world->slots[a].arrival;
            const bool faster = glm::length(before) > glm::length(after);
            if(faster)
            {
                store(before, ea->v.velocity);
            }
            SV_Impact(ea, eb);
            if(faster && !ea->free && vec(ea->v.velocity) == before)
            {
                store(after, ea->v.velocity);
            }
            continue;
        }
        if(world->slots[a].kind != Kind::Prop || world->slots[b].kind != Kind::Prop)
        {
            SV_Impact(ea, eb);
            continue;
        }
        // Two props (a thrown weapon, box or gib meeting a gib or a head lying about, what takes damage): they touch
        // after the step, whose bounce has spent the thrown one's speed, so its hit was too slow to hurt
        // (forcegrabbable_touch: vr_throw_hit_min_speed, the damage by speed). The touch sees the velocities they came
        // with (the step's start), as a monster's (touchNearby, before the step) does; what the touch sets stays, else
        // the step's are put back.
        const glm::vec3 afterA = vec(ea->v.velocity), afterB = vec(eb->v.velocity);
        const glm::vec3 beforeA = world->slots[a].arrival, beforeB = world->slots[b].arrival;
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d %s and %d %s touch, at %.0f and %.0f u/s (%.0f and %.0f after the step)\n", a,
                PR_GetString(ea->v.classname), b, PR_GetString(eb->v.classname), glm::length(beforeA), glm::length(beforeB),
                glm::length(afterA), glm::length(afterB));
        }
        store(beforeA, ea->v.velocity);
        store(beforeB, eb->v.velocity);
        SV_Impact(ea, eb);
        if(!ea->free && vec(ea->v.velocity) == beforeA)
        {
            store(afterA, ea->v.velocity);
        }
        if(!eb->free && vec(eb->v.velocity) == beforeB)
        {
            store(afterB, eb->v.velocity);
        }
    }
    callShocks();
    callFalls();
    watchInside();
    physsound::frameEnd(); // the frame's knocks and scrapes

    const double t3 = Sys_DoubleTime();
    if(frameTiming && world->frameSamples.size() < stepSamplesMax)
    {
        const auto ms = [](double a, double b) { return static_cast<float>((b - a) * 1000.0); };
        sample.ms[PhaseServerPhysics] = serverPhysicsStart > 0.0 && serverPhysicsStart <= t0 ? ms(serverPhysicsStart, t3) : 0.f;
        sample.ms[PhaseBox3D] = ms(t0, t3);
        sample.ms[PhaseSync] = ms(t0, tSync);
        sample.ms[PhaseBefore] = ms(tSync, t1);
        sample.ms[PhaseStep] = ms(t1, t2);
        sample.ms[PhaseWrite] = ms(tWrite, tAfter);
        sample.ms[PhaseAfter] = ms(tAfter, t3);
        sample.awake = b3World_GetAwakeBodyCount(world->id);
        world->frameSamples.pushBack(sample);
    }
    serverPhysicsStart = 0.0;
}

namespace qvr::box3d
{

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

int nearestProp()
{
    if(!world || !sv.active || svs.maxclients < 1)
    {
        return 0;
    }
    const VmScope vm;
    const edict_t* player = EDICT_NUM(1);
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

bool ragdollKnockdown(edict_t* ent)
{
    if(!canRagdoll(ent) || !knockedDown(ent))
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(ent);
    for(za::SizeT i = 0; i < world->recoveries.size(); i++)
    {
        if(world->recoveries[i].num == num)
        {
            world->recoveries.eraseAt(i); // (knocked down again as it got up)
            break;
        }
    }
    Slot& s = slotOf(num);
    if(s.ragdoll >= 0)
    {
        return true;
    }
    destroyBody(s);
    const glm::vec3 vel = vec(ent->v.velocity);
    if(!createRagdoll(ent, num, s))
    {
        return false;
    }
    (void)vel;
    writeRagdoll(ent, s); // (drawn as one at once)
    if(vr_knockdown_debug.value)
    {
        Con_Printf("knockdown: %d %s down as a ragdoll (%d ragdolls)\n", num, PR_GetString(ent->v.classname), ragdollCount());
    }
    return true;
}

int ragdollGetUp(edict_t* ent, int frameA, int frameB, const glm::vec3& mins, const glm::vec3& maxs, float range)
{
    if(!world || !ent || ent->free)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(!r || !r->rig)
    {
        return 0;
    }
    const ragdoll::Rig& rig = *r->rig;
    float yawA = 0.f, yawB = 0.f;
    glm::vec3 atA{0.f}, atB{0.f};
    const float errA = frameA >= 0 ? fitFrame(*r, frameA, yawA, atA) : 1e30f;
    const float errB = frameB >= 0 ? fitFrame(*r, frameB, yawB, atB) : 1e30f;
    if(errA >= 1e29f && errB >= 1e29f)
    {
        return 0;
    }
    const int which = errB < errA ? 2 : 1;
    const float yaw = which == 2 ? yawB : yawA;
    const glm::vec3 at = which == 2 ? atB : atA;
    const b3WorldTransform pelvis = b3Body_GetTransform(r->body[0]);
    const glm::vec3 from = fromB3(pelvis.q) * (rig.bones[0].pivot * r->scale) + world->toU(pelvis.p);
    glm::vec3 spot;
    if(!standSpot(ent, from, at, mins, maxs, range, spot))
    {
        if(vr_knockdown_debug.value)
        {
            Con_Printf("knockdown: %d no room to get up within %.0f units: stays down\n", num, range);
        }
        return 0;
    }

    // Its last pose kept for the blend, the hands holding it let go, its parts gone.
    Recovery rec;
    rec.num = num;
    rec.rig = r->rig;
    rec.scale = r->scale;
    rec.count = r->count;
    for(int b = 0; b < r->count; b++)
    {
        const b3WorldTransform xf = b3Body_GetTransform(r->body[static_cast<za::SizeT>(b)]);
        rec.rot[static_cast<za::SizeT>(b)] = fromB3(xf.q);
        rec.pos[static_cast<za::SizeT>(b)] = world->toU(xf.p);
    }
    rec.start = qcvm->time;
    if(vr_knockdown_debug.value >= 2.f)
    {
        ragdoll::watchGetup(num, qcvm->time, qcvm->time + 2.5);
    }
    rec.duration = za::max(vr_knockdown_blend.value, 0.f);
    destroyBody(slotOf(num));
    world->recoveries.pushBack(rec);
    if(fields().vr_knockdown >= 0)
    {
        fieldFloat(ent, fields().vr_knockdown) = 2.f;
    }
    store(spot, ent->v.origin);
    ent->v.angles[0] = ent->v.angles[2] = 0.f;
    ent->v.angles[1] = glm::degrees(yaw);
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    // (Drawn as it lay until the step's blend: no frame of its animated model in between.)
    ragdoll::publish(num, rec.rig, rec.count, rec.rot.data(), rec.pos.data(), rec.scale, qcvm->time + host_frametime, 0, 0);
    if(vr_knockdown_debug.value)
    {
        const float other = which == 2 ? errA : errB;
        Con_Printf("knockdown: %d gets up from frame %d (fit %.0f, the other's %.0f), %.0f units from where it lay, yaw %.0f\n",
            num, which == 2 ? frameB : frameA, which == 2 ? errB : errA, other < 1e29f ? other : -1.f,
            glm::length(glm::vec2{spot - at}), glm::degrees(yaw));
    }
    return which;
}


bool ragdollReach(int num, const glm::vec3& at)
{
    const RagdollBodies* r = ragdollOf(num);
    float distance = 0.f;
    const int part = r && vr_ragdoll_grab.value >= 1.f ? nearestPart(*r, at, distance) : -1;
    if(part >= 0 && vr_debug_ragdoll.value >= 2.f)
    {
        Con_Printf("ragdoll: %d: a hand %.1f units from part %d\n", num, distance, part);
    }
    return part >= 0 && distance <= za::max(vr_ragdoll_grab_reach.value, 0.f);
}

bool ragdollGrab(edict_t* corpse, edict_t* player, int hand)
{
    return grabRagdoll(corpse, player, hand);
}

bool ragdollPull(edict_t* corpse, edict_t* player, int hand, float flight)
{
    return world && pullRagdoll(corpse, player, hand, flight);
}

void ragdollRelease(edict_t* player, int hand, const glm::vec3& velocity)
{
    if(world)
    {
        releaseRagdoll(player, hand, velocity);
    }
}

int ragdollHeld(edict_t* player, int hand)
{
    if(!world)
    {
        return 0;
    }
    const int index = grabIndex(NUM_FOR_EDICT(player), hand);
    return index < 0 ? 0 : world->ragdollGrabs[static_cast<za::SizeT>(index)].pulling ? 2 : 1;
}

float ragdollHandReach(edict_t* player, int hand)
{
    return world ? grabReach(NUM_FOR_EDICT(player), hand) : -1.f;
}

bool ragdollHold(int player, int hand, RagdollHold& out)
{
    out = RagdollHold{};
    const int index = world ? grabIndex(player, hand) : -1;
    if(index < 0)
    {
        return false;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    const RagdollBodies* r = ragdollOf(g.num);
    if(g.pulling || !r || g.part >= r->count || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return false;
    }
    // (The joint holds its frame B, on the part, at the anchor, the hand: the part's frame is ragdoll::publish's.)
    const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
    out.num = g.num;
    out.part = g.part;
    out.at = glmv(frameB.p) * world->m2u;
    out.turn = fromB3(frameB.q);
    return true;
}

int ragdollPartCentre(const glm::vec3& from, int part, glm::vec3& out)
{
    if(!world)
    {
        return 0;
    }
    const RagdollBodies* best = nullptr;
    float bestD = 1e30f;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && r.count > 0)
        {
            const float d = glm::distance(world->toU(b3Body_GetPosition(r.body[0])), from);
            if(d < bestD)
            {
                bestD = d;
                best = &r;
            }
        }
    }
    if(best && part < 0) // (the part nearest `from`)
    {
        float distance = 0.f;
        part = nearestPart(*best, from, distance);
    }
    if(!best || part < 0 || part >= best->count)
    {
        return 0;
    }
    out = world->toU(b3Body_GetWorldCenter(best->body[static_cast<za::SizeT>(part)]));
    return best->num;
}

int ragdollBone(int num, const glm::vec3& at)
{
    return ragdollBoneNear(num, at);
}

glm::vec3 ragdollPoint(int num, int bone, const glm::vec3& p, bool toWorld)
{
    return box3dRagdollPoint(num, bone, p, toWorld);
}

// ---- Limb gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc) ----
// Every limb as the head: its joint (ragdoll::limbJoint) cut (cutLimb), its bones' bodies gone, the piece flying off as
// a gib of its own (QC: its model vr_limbmodel.cpp's, placed by ragdollCut 0-3) or popped; the stump bleeds.

bool ragdollCutLimb(edict_t* ent, int bone, const glm::vec3& blade, float settle)
{
    if(!world || vr_ragdoll.value < 1.f)
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        // (A live monster just killed, a dying one, a corpse: its ragdoll now, from the frame it is in.)
        if(num <= svs.maxclients || ent->free || !deadMonster(ent) || hasFlag(ent, FL_SWIM))
        {
            return false;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(modelOf(ent));
        const int b = rig ? (bone < 0 ? rig->head : bone) : -1;
        if(!rig || b < 0 || (b != rig->head && !ragdoll::limbJoint(*rig, b)))
        {
            return false;
        }
        Slot& s = slotOf(num);
        destroyBody(s);
        if(!createRagdoll(ent, num, s, true))
        {
            return false;
        }
        r = ragdollOf(num);
    }
    else
    {
        // (A corpse's: its own motion as it lies or falls; the frame's knocks reach it after, feedRagdoll.)
        const bool own = vr_decap_own_motion.value != 0.f;
        for(int b = 0; b < r->count; b++)
        {
            const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
            r->ownLin[static_cast<za::SizeT>(b)] = own && !partCut(*r, b) ? glmv(b3Body_GetLinearVelocity(body)) : glm::vec3{0.f};
            r->ownAng[static_cast<za::SizeT>(b)] = own && !partCut(*r, b) ? glmv(b3Body_GetAngularVelocity(body)) : glm::vec3{0.f};
        }
    }
    const int b = r ? (bone < 0 ? r->rig->head : bone) : -1;
    if(!r || b < 0 || !cutLimb(*r, ent, b, blade, true))
    {
        return false;
    }
    r->settle = za::min(r->settle, settle);
    if(vr_debug_ragdoll.value)
    {
        const glm::vec3 pelvis = world->toU(b3Body_GetLinearVelocity(r->body[0]));
        Con_Printf("ragdoll: %d %s cut off: its own motion %.0f u/s (pelvis; %.0f level), its pelvis at %.0f u/s now\n", num,
            r->rig->bones[b].name, glm::length(r->ownLin[0] * world->m2u), glm::length(glm::vec2{r->ownLin[0] * world->m2u}),
            glm::length(pelvis));
    }
    if(b == r->rig->head && fields().vr_headless >= 0)
    {
        fieldFloat(ent, fields().vr_headless) = 1.f;
    }
    writeRagdoll(ent, slotOf(num)); // (drawn without it at once)
    return true;
}

bool ragdollDecap(edict_t* ent, const glm::vec3& blade, float settle)
{
    return ragdollCutLimb(ent, -1, blade, settle);
}

glm::vec3 ragdollCut(int num, int what, int bone)
{
    const RagdollBodies* r = ragdollOf(num);
    if(r && bone < 0)
    {
        bone = r->rig->head;
    }
    if(r && what == 6 && bone >= 0 && bone < r->count && !partCut(*r, bone))
    {
        // (The piece's middle now, on: the tests' cut at it.)
        const uint32_t bones = bone == r->rig->head ? (1u << bone) : ragdoll::limbBones(*r->rig, bone) & ~r->cut;
        const glm::vec3 mid = ragdoll::limbMiddle(*r->rig, bones);
        const b3WorldTransform hx = b3Body_GetTransform(r->body[static_cast<za::SizeT>(bone)]);
        return fromB3(hx.q) * (mid * r->scale) + world->toU(hx.p);
    }
    if(r && what == 7)
    {
        return glm::vec3{static_cast<float>(r->cut), static_cast<float>(r->lastCut), static_cast<float>(partsLeft(*r))};
    }
    if(r && what == 8)
    {
        return glm::vec3{static_cast<float>(r->lastCutBones), 0.f, 0.f};
    }
    if(!r || !r->cut)
    {
        return glm::vec3{0.f};
    }
    switch(what)
    {
    case 0: return r->headMid;
    case 1:
    {
        glm::vec3 a;
        held::anglesFromAxes(glm::mat3_cast(r->headRot), &a[0], false);
        return a;
    }
    case 2: return r->headVel;
    case 3: return r->headSpin;
    case 4:
    case 5:
    {
        const ragdoll::Rig& rig = *r->rig;
        if(bone < 0 || bone >= rig.numBones)
        {
            return glm::vec3{0.f};
        }
        const int parent = ragdoll::uncutParent(rig, bone, r->cut);
        if(parent < 0)
        {
            return glm::vec3{0.f};
        }
        const b3WorldTransform px = b3Body_GetTransform(r->body[static_cast<za::SizeT>(parent)]);
        const glm::quat q = fromB3(px.q);
        const ragdoll::Bone& piece = rig.bones[bone];
        if(what == 4)
        {
            return q * (piece.pivot * r->scale) + world->toU(px.p);
        }
        const glm::vec3 d = piece.end - piece.pivot;
        return glm::length(d) > 1e-3f ? q * glm::normalize(d) : q * glm::vec3{0.f, 0.f, 1.f};
    }
    default: return glm::vec3{0.f};
    }
}

// `ent`'s bones as drawn now: p_world = rot[b] * (scale * p_rest) + pos[b]: a ragdoll's bodies (the cut bones and the
// hidden ones in `cut`), else its frame's pose where it stands (yaw only). False: no rig.
[[nodiscard]] bool posedBones(edict_t* ent, const ragdoll::Rig*& rig, za::Array<glm::quat, ragdoll::maxBones>& rot,
    za::Array<glm::vec3, ragdoll::maxBones>& pos, float& scale, uint32_t& cut)
{
    const int num = NUM_FOR_EDICT(ent);
    if(world)
    {
        if(const RagdollBodies* r = ragdollOf(num))
        {
            rig = r->rig;
            scale = r->scale;
            cut = r->cut;
            for(int b = 0; b < rig->numBones; b++)
            {
                if(b >= r->count)
                {
                    cut |= 1u << b;
                    continue;
                }
                const b3WorldTransform x = b3Body_GetTransform(r->body[static_cast<za::SizeT>(b)]);
                rot[static_cast<za::SizeT>(b)] = fromB3(x.q);
                pos[static_cast<za::SizeT>(b)] = world->toU(x.p);
            }
            return true;
        }
    }
    qmodel_t* model = modelOf(ent);
    rig = model ? ragdoll::rigFor(model) : nullptr;
    if(!rig)
    {
        return false;
    }
    scale = za::clamp(1.f + fieldVec(ent, fields().model_scale).x, 0.25f, 4.f);
    cut = static_cast<uint32_t>(za::max(fieldFloatOr(ent, fields().vr_limbcut, 0.f), 0.f));
    const int pose = ragdoll::poseOfFrame(model, static_cast<int>(ent->v.frame));
    const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 origin = vec(ent->v.origin);
    for(int b = 0; b < rig->numBones; b++)
    {
        glm::quat q;
        glm::vec3 p;
        ragdoll::bonePose(*rig, pose, b, q, p);
        rot[static_cast<za::SizeT>(b)] = turn * q;
        pos[static_cast<za::SizeT>(b)] = origin + turn * (p * scale);
    }
    return true;
}

int limbAt(edict_t* ent, const glm::vec3& at)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return -2;
    }
    const auto place = [&](int b, const glm::vec3& p) {
        return rot[static_cast<za::SizeT>(b)] * (p * scale) + pos[static_cast<za::SizeT>(b)];
    };
    // The bone struck: its vertex nearest.
    int hit = -1;
    float best = 1e30f;
    for(int b = 0; b < rig->numBones; b++)
    {
        if(cut & (1u << b))
        {
            continue;
        }
        for(const glm::vec3& p : rig->bones[b].points)
        {
            const float d = glm::distance(place(b, p), at);
            if(d < best)
            {
                best = d;
                hit = b;
            }
        }
    }
    if(hit < 0)
    {
        return -1;
    }
    if(ragdoll::headBones(*rig) & (1u << hit))
    {
        return rig->head;
    }
    if(!ragdoll::limbJoint(*rig, hit))
    {
        return -1; // (the torso, a loose piece)
    }
    // The nearest joint: the bone's own (its pivot) or one of its children's.
    int joint = hit;
    float jd = glm::distance(place(hit, rig->bones[hit].pivot), at);
    for(int c = hit + 1; c < rig->numBones; c++)
    {
        if(rig->bones[c].parent == hit && !(cut & (1u << c)) && ragdoll::limbJoint(*rig, c))
        {
            const float d = glm::distance(place(c, rig->bones[c].pivot), at);
            if(d < jd)
            {
                jd = d;
                joint = c;
            }
        }
    }
    return joint;
}

uint32_t limbInfo(edict_t* ent, int what, int bone)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return what == 3 ? ~0u : 0u;
    }
    const uint32_t head = ragdoll::headBones(*rig);
    switch(what)
    {
    case 0: // the limb joints not cut (not the head's)
    {
        uint32_t m = 0;
        for(int b = 0; b < rig->numBones; b++)
        {
            m |= ragdoll::limbJoint(*rig, b) && !(head & (1u << b)) && !(cut & (1u << b)) ? 1u << b : 0u;
        }
        return m;
    }
    case 1: return head;
    case 2: return cut;
    case 3: return static_cast<uint32_t>(rig->head);
    case 4: return bone >= 0 && bone < rig->numBones ? (bone == rig->head ? head : ragdoll::limbBones(*rig, bone)) & ~cut : 0u;
    case 5: return bone >= 0 && bone < rig->numBones ? static_cast<uint32_t>(rig->bones[bone].parent) : ~0u;
    default: return 0u;
    }
}

const char* limbModel(edict_t* ent, int bone)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    const int index = static_cast<int>(ent->v.modelindex);
    if(!posedBones(ent, rig, rot, pos, scale, cut) || index <= 0 || index >= MAX_MODELS || !sv.model_precache[index] ||
        !ragdoll::limbJoint(*rig, bone))
    {
        return "";
    }
    const char* base = sv.model_precache[index];
    const uint32_t all = ragdoll::limbBones(*rig, bone);
    // (After the cut: the piece cut off is what it took then, `lastCutBones`.)
    const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    uint32_t bones = all & ~cut;
    if(r && r->lastCut == bone && r->lastCutBones)
    {
        bones = r->lastCutBones;
    }
    if(!bones || !limbmodel::available(base, bone, bones == all ? 0u : bones))
    {
        return "";
    }
    return limbmodel::keptName(base, bone, bones == all ? 0u : bones);
}

// Limb gore: what part `b` of `rig` weighs cut off, as a share of its ragdoll's mass (limbPiece), by the kind of limb its
// name says (the author: a limb far lighter than its body, a hand lighter than a thigh): a person's (an arm 6.3%: the
// upper arm 3.5, the forearm 2, the hand 0.8; a leg 15.5%: the thigh 9.5, the shin 4.5, the foot 1.5; the head 7, a jaw
// 1 of it), two arms' and two legs' shares split among as many as the rig has (a rottweiler's four legs, the centroid's
// six); a part the rig hasn't (a forearm without a hand of its own, an arm without its forearm) weighs on the one that
// has it. Tails and anything else: their hulls' share of the body's volume (as the ragdoll's own parts weigh); a loose
// piece (a gun) nothing.
namespace limbmass
{
enum class Kind : uint8_t { Head, Jaw, UpperArm, Forearm, Hand, Arm, Thigh, Shin, Foot, Leg, Other };
constexpr float headShare = 0.07f, jawShare = 0.01f;
constexpr float upperArmShare = 0.035f, forearmShare = 0.02f, handShare = 0.008f;
constexpr float thighShare = 0.095f, shinShare = 0.045f, footShare = 0.015f;

[[nodiscard]] Kind kindOf(const char* name)
{
    const auto is = [name](const char* prefix) { return !strncmp(name, prefix, strlen(prefix)); };
    if(is("head")) { return Kind::Head; }
    if(is("jaw")) { return Kind::Jaw; }
    if(is("upperarm")) { return Kind::UpperArm; }
    if(is("forearm")) { return Kind::Forearm; }
    if(is("hand") || is("claw")) { return Kind::Hand; }
    if(is("arm")) { return Kind::Arm; } // (upper arm and forearm in one: the knight's, the scrag's)
    if(is("thigh") || is("upperleg")) { return Kind::Thigh; }
    if(is("shin") || is("lowerleg")) { return Kind::Shin; }
    if(is("foot")) { return Kind::Foot; }
    if(is("leg")) { return Kind::Leg; } // (a whole leg in one: the centroid's)
    return Kind::Other; // (tails, the torso)
}
} // namespace limbmass

[[nodiscard]] float limbMassShare(const ragdoll::Rig& rig, int b)
{
    using limbmass::Kind;
    const ragdoll::Bone& bone = rig.bones[b];
    if(bone.joint == ragdoll::Joint::Loose)
    {
        return 0.f;
    }
    const auto hasChild = [&](Kind k) {
        for(int c = b + 1; c < rig.numBones; c++)
        {
            if(rig.bones[c].parent == b && limbmass::kindOf(rig.bones[c].name) == k)
            {
                return true;
            }
        }
        return false;
    };
    int arms = 0, legs = 0;
    for(int c = 0; c < rig.numBones; c++)
    {
        const Kind k = limbmass::kindOf(rig.bones[c].name);
        arms += k == Kind::UpperArm || k == Kind::Arm ? 1 : 0;
        legs += k == Kind::Thigh || k == Kind::Leg ? 1 : 0;
    }
    const float arm = 2.f / static_cast<float>(za::max(arms, 1)), leg = 2.f / static_cast<float>(za::max(legs, 1));
    using namespace limbmass;
    const float hand = hasChild(Kind::Hand) ? 0.f : handShare;
    switch(kindOf(bone.name))
    {
    case Kind::Head: return headShare - (hasChild(Kind::Jaw) ? jawShare : 0.f);
    case Kind::Jaw: return jawShare;
    case Kind::UpperArm: return arm * (upperArmShare + (hasChild(Kind::Forearm) ? 0.f : forearmShare + handShare));
    case Kind::Forearm: return arm * (forearmShare + hand);
    case Kind::Arm: return arm * (upperArmShare + forearmShare + hand);
    case Kind::Hand: return arm * handShare;
    case Kind::Thigh: return leg * (thighShare + (hasChild(Kind::Shin) ? 0.f : shinShare + footShare));
    case Kind::Shin: return leg * (shinShare + (hasChild(Kind::Foot) ? 0.f : footShare));
    case Kind::Foot: return leg * footShare;
    case Kind::Leg: return leg * (thighShare + shinShare + footShare);
    case Kind::Other: break;
    }
    // (Its hull's share of the body's volume, the loose pieces not: as the ragdoll's parts weigh, createRagdoll.)
    float own = 0.f, whole = 0.f;
    for(int c = 0; c < rig.numBones; c++)
    {
        if(rig.bones[c].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        float v = 0.f;
        if(b3HullData* hull = boneHull(rig.bones[c], 1.f, v))
        {
            b3DestroyHull(hull);
        }
        whole += v;
        own += c == b ? v : 0.f;
    }
    return whole > 0.f ? own / whole : 0.f;
}

glm::vec3 limbPiece(edict_t* ent, int bone, int what)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return glm::vec3{0.f};
    }
    bone = bone < 0 ? rig->head : bone;
    if(bone < 0 || bone >= rig->numBones || (bone != rig->head && !ragdoll::limbJoint(*rig, bone)))
    {
        return glm::vec3{0.f};
    }
    // The piece: what a cut there takes now (a body gibbed whole: the limb as it is); just cut, what it took then (as
    // limbModel's).
    uint32_t bones = (bone == rig->head ? ragdoll::headBones(*rig) : ragdoll::limbBones(*rig, bone)) & ~cut;
    const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(r && r->lastCut == bone && r->lastCutBones && (cut & (1u << bone)))
    {
        bones = r->lastCutBones;
    }
    if(what == 1)
    {
        // (Its model's middle: a limb's model's origin, vr_limbmodel.cpp; the head's, its own bone's, ragdollCut 0.)
        return rig->bones[bone].pivot - ragdoll::limbMiddle(*rig, bone == rig->head ? (1u << bone) : bones);
    }
    float share = 0.f;
    for(int b = 0; b < rig->numBones; b++)
    {
        share += (bones & (1u << b)) ? limbMassShare(*rig, b) : 0.f;
    }
    const float mass = za::max(tune(ent, Tune::Mass), 1.f);
    return glm::vec3{mass * share, mass, share};
}

namespace
{
[[nodiscard]] const RagdollClass* headClass(const char* model)
{
    for(const RagdollClass& c : ragdollClasses)
    {
        if(model && c.head && !strcmp(model, c.head))
        {
            return &c;
        }
    }
    return nullptr;
}
} // namespace

bool monsterHead(const char* model)
{
    return headClass(model) != nullptr;
}

float headPropMass(const char* model)
{
    // As a head cut off its ragdoll (limbPiece, then the QC's VR_Limb_SetMass): the class's mass times the head's
    // share (every rig's head and jaw: headShare) times vr_limbs_mass_scale. Its scale 0 (cut pieces weighed by their
    // volume): 0, estimated as well.
    const RagdollClass* c = headClass(model);
    const float scale = vr_limbs_mass_scale.value;
    if(!c || scale <= 0.f)
    {
        return 0.f;
    }
    const auto i = static_cast<za::SizeT>(Tune::Mass);
    const float mass = za::max(c->own[i]->value >= 0.f ? c->own[i]->value : ragdollGlobals[i]->value, 1.f);
    return za::max(mass * limbmass::headShare * scale, 0.05f);
}

glm::vec3 limbPlace(edict_t* ent, int bone, int what)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut) || bone < 0 || bone >= rig->numBones)
    {
        return glm::vec3{0.f};
    }
    const glm::quat q = rot[static_cast<za::SizeT>(bone)];
    if(what == 1)
    {
        glm::vec3 a;
        held::anglesFromAxes(glm::mat3_cast(q), &a[0], false);
        return a;
    }
    if(what == 2)
    {
        const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
        return r && bone < r->count ? world->toU(b3Body_GetLinearVelocity(r->body[static_cast<za::SizeT>(bone)])) : vec(ent->v.velocity);
    }
    const uint32_t bones = (bone == rig->head ? (1u << bone) : ragdoll::limbBones(*rig, bone)) & ~cut;
    if(what == 3)
    {
        // (A point on its own bone's surface: its vertex nearest that bone's middle; the tests' hit.)
        const glm::vec3 mid = ragdoll::limbMiddle(*rig, 1u << bone);
        glm::vec3 best = mid;
        float bestD = 1e30f;
        for(const glm::vec3& p : rig->bones[bone].points)
        {
            const float d = glm::distance(p, mid);
            if(d < bestD)
            {
                bestD = d;
                best = p;
            }
        }
        return q * (best * scale) + pos[static_cast<za::SizeT>(bone)];
    }
    return q * (ragdoll::limbMiddle(*rig, bones) * scale) + pos[static_cast<za::SizeT>(bone)];
}

int ragdollHeadAt(int num, const glm::vec3& at, float neck)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        return -1;
    }
    const ragdoll::Rig& rig = *r->rig;
    const uint32_t bones = ragdoll::headBones(rig);
    if(!bones || rig.head >= r->count || (r->cut & bones))
    {
        return 0;
    }
    float distance = 0.f;
    const int part = nearestPart(*r, at, distance);
    if(part >= 0 && (bones & (1u << part)))
    {
        return 1;
    }
    const int parent = rig.bones[rig.head].parent;
    if(parent < 0)
    {
        return 0;
    }
    const b3WorldTransform px = b3Body_GetTransform(r->body[static_cast<za::SizeT>(parent)]);
    const glm::vec3 neckAt = fromB3(px.q) * (rig.bones[rig.head].pivot * r->scale) + world->toU(px.p);
    return glm::distance(neckAt, at) <= za::max(neckReach * r->scale, neck) ? 1 : 0;
}

} // namespace qvr::box3d

extern "C" void VR_MonsterFell(edict_t* ent, float speed)
{
    if(ent)
    {
        monsterFell(ent, speed);
    }
}
