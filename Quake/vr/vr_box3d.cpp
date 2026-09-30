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
// - The players' hands are kinematic spheres at their fists that push solid props (syncHands, vr_box3d_hand_push).
// - Players stand on solid props (vr_box3d_player_stand, "Standing on props": capsuleStandsOn, beforeStanding,
//   pressStanding, rideStanding): ground to Quake's movement, their weight pressing, carried as the prop moves. Their
//   boxes meet a solid prop's drawn box as turned, as a round column (vr_box3d_player_shape: VR_PropClip, propShape).
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
//
// Single-threaded: the world has one worker and no task callbacks, which Box3D runs serially (each task inline:
// b3DefaultAddTaskFcn in physics_world.c); no scheduler, no threads. Deterministic: the same calls in the same
// order (entities in edict order) give the same result.

#include "vr_box3d.hpp"
#include "vr_hitmodel.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"
#include "vr_weapons.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include <box3d/box3d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// The sync's buffers (the server's frame: the main thread; Box3D steps with one worker, the caller's).
struct Box3dScratch
{
    std::vector<glm::vec3> propVerts;   // a prop's drawn vertices, its hull made (propHull)
    std::vector<glm::vec3> actorVerts;  // an actor's (actorHull)
    std::vector<glm::vec3> corners;     // a body's shapes' corners (floorDepth)
    std::vector<uint8_t> carried;       // by edict: carried by a player (syncEntities)
    auto members() { return std::tie(propVerts, actorVerts, corners, carried); }
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
constexpr uint64_t propMask = catWorld | catMover | catActor | catPlayer | catProp | catHeld | catFixture;

enum class Kind : uint8_t
{
    None,
    Prop,   // a rigid body: dynamic
    Held,   // a prop carried in a hand (or two): kinematic, following it
    Mover,  // a brush entity: kinematic
    Actor,  // a monster or another solid box: kinematic
    Player, // a player's body: a kinematic capsule
    Fixture, // a pickup that is not a rigid body (hanging in the air, on a rack): kinematic, its drawn hull
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
    const bool wood = index > 0 && index < MAX_MODELS && sv.models[index] && !strcmp(sv.models[index]->name, "progs/vrtorch.mdl");
    if(const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr)
    {
        if(const float stone = props::stoneDensity(model); stone > 0.f)
        {
            return 1000.f / stone; // a rock or a brick (vr_debris.cpp) sinks as stone does, faster than a gib
        }
    }
    return gib && !wood ? sinkDensity : floatDensity; // (a taken wall torch is wood: it floats)
}

[[nodiscard]] bool wetAt(float x, float y, float z)
{
    vec3_t p{x, y, z};
    const int contents = SV_PointContents(p);
    return contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA;
}

// The part of the column from `lo` to `hi` (z) at `c` under water (0 to 1), by the BSP's liquid leaves.
[[nodiscard]] float submerged(const glm::vec3& c, float lo, float hi)
{
    float under = lo;
    if(!wetAt(c.x, c.y, lo))
    {
        if(!wetAt(c.x, c.y, c.z))
        {
            return 0.f;
        }
        under = c.z; // the bottom in a floor
    }
    if(wetAt(c.x, c.y, hi))
    {
        return 1.f;
    }
    float above = hi;
    for(int i = 0; i < 12; i++)
    {
        const float mid = (under + above) * 0.5f;
        (wetAt(c.x, c.y, mid) ? under : above) = mid;
    }
    return CLAMP(0.f, ((under + above) * 0.5f - lo) / std::max(hi - lo, 0.01f), 1.f);
}

// Weapons (thrown, dropped, or a map's weapon pickup) and keys: hard, detailed shapes.
[[nodiscard]] bool isWeaponLike(edict_t* ent)
{
    const char* name = PR_GetString(ent->v.classname);
    return !strcmp(name, "thrown_weapon") || !strncmp(name, "weapon_", 7) || !strncmp(name, "item_key", 8);
}

// A live grenade that can be caught (QC vr_grenade.qc: an ogre's, a multi-grenade ogre's, or the player's with
// vr_grenade_catch 2) as a rigid body: Quake's grenade models. Hard and heavy, it bounces as Quake's grenades did, and
// never meets its thrower's own body (Quake's rule for a missile and its owner: it leaves the ogre it is thrown from).
[[nodiscard]] bool isGrenade(const qmodel_t* model)
{
    return model->type == mod_alias && (!strcmp(model->name, "progs/grenade.mdl") || !strcmp(model->name, "progs/mervup.mdl"));
}

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
        return strstr(model->name, "armor") ? 600.f : 250.f; // armour; backpacks
    }
    return 1000.f; // gibs and heads: flesh
}

// A Mass set for the prop's model (Held Object Offsets, vr_props.inc), kg; 0: none (its volume times its density).
[[nodiscard]] float massSetting(const qmodel_t* model)
{
    return model ? std::max(props::valueFor(model, props::Key::Mass), 0.f) : 0.f;
}

// Soft things (backpacks, gibs, heads) land with a thud: no bounce, and their tumble dies away fast on the ground (a
// rigid hull of a backpack lands on an edge and tumbles down a gentle slope like a crate).
[[nodiscard]] bool isSoft(edict_t* ent, const qmodel_t* model)
{
    return model->type == mod_alias && !isWeaponLike(ent) && !strstr(model->name, "armor") && !isGrenade(model) &&
        props::stoneDensity(model) <= 0.f; // (rocks and bricks are hard)
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
    std::array<float, 9> scale{};   // props: model_scale, model_scale_origin, model_offset
    glm::vec3 mins{0.f}, maxs{0.f}; // actors: Quake's box; props: the drawn box
    float radius{0.f};              // players: the capsule's

    // The entity as last written (props) or seen (kinematic ones): QC's changes are the differences.
    glm::vec3 origin{0.f}, angles{0.f}, velocity{0.f}, spin{0.f};
    glm::vec3 arrival{0.f}; // props: the velocity this frame's step began with (0 asleep): what a touch after it sees
    float gravityScale{1.f};
    float massSetting{0.f}; // props: the Mass set for its model when it was made (vr_props.inc; 0: none)
    float size{1.f};        // props: its model's Size when it was made (Held Object Offsets: props::drawnSize)
    bool asleep{false};
    bool wet{false};      // floating: kept awake (it bobs)
    bool bullet{false};   // fast: continuous collision against other props too
    bool soft{false};     // isSoft
    bool brush{false};    // angles as a brush model's
    bool spins{false};    // a fixture drawn spinning (an EF_ROTATE model: the map's pickups): its shape turns with it
    double born{0.0};     // the server's time its body was made (a prop: thrown, let go of, launched)
    int pushedStep{-100}; // props: the last step a hand's body pushed it (limitPushes: a hit, then a shove)
    const b3HullData* hull{nullptr}; // actors: the hull at rest (actorHull), nullptr for Quake's box

    // Props, held and fixtures: the settings (shapeGeneration) and the entity's box its drawn box and Mass were last
    // found the same at (stale): looked at again only when one of them changes.
    unsigned checkedGeneration{0};
    glm::vec3 checkedMins{0.f}, checkedMaxs{0.f};
    bool checkedSolid{false};
};

// The hulls made for models, by what they were made from (and the world's scale).
struct PropHullKey
{
    const qmodel_t* model;
    int frame;
    std::array<float, 6> box;
    auto operator<=>(const PropHullKey&) const = default;
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

struct World
{
    b3WorldId id{};
    const qmodel_t* map{nullptr};
    float m2u{1.f};      // units a metre
    float gravity{0.f};  // sv_gravity at the last update
    float friction{-1.f}, restitution{-1.f};
    b3MeshData* mesh{nullptr}; // (meshCache's)
    b3ShapeId worldShape{b3_nullShapeId};
    std::vector<std::pair<int, int>> impacts; // the step's touches (kept: no allocation a frame)
    std::vector<Shock> shocks; // props with a .vr_impact hitting something this frame (the hardest hit each)
    std::vector<Pushed> pushed; // the props near the hands' bodies before this step (kept: no allocation a frame)
    std::vector<Slot> slots; // by edict number
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
        std::vector<int> ignore; // props the reach body passes through until clear of them (inside it as it was made)
        int held{0};             // the prop the hand carried last frame (0: none)
        float weaponMass{0.f};   // the held weapon's own mass (kg: Weapon Weights' Mass; 0: none, or not a weapon)
        std::vector<glm::vec4> fist; // the push body's spheres (the drawn fist, in the hand's frame; empty: the one sphere)
    };
    std::vector<std::array<HandBody, 2>> hands;
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
        int skipped{0};         // the hands' contacts it passed through (steps)
    };
    std::vector<Grace> graces;
    std::vector<int> made; // the props whose bodies were made this frame (createBody): what may have been thrown
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
    std::vector<Stand> stands;
    // A player's move blocked by a solid prop's side this frame (VR_PlayerBumps): he shoves it (shoveBumped).
    struct Bump
    {
        int player{0};
        int num{0};
        glm::vec3 dir{0.f};     // horizontal, into the prop
        float speed{0.f};       // units/s, his speed into it
    };
    std::vector<Bump> bumps;
    std::map<PropHullKey, b3HullData*> propHulls;      // nullptr: no hull (a box instead)
    std::map<const qmodel_t*, std::vector<b3HullData*>> moverHulls;
    int steps{0};

    [[nodiscard]] b3Vec3 toM(const glm::vec3& u) const { return b3Vec3{u.x / m2u, u.y / m2u, u.z / m2u}; }
    [[nodiscard]] glm::vec3 toU(const b3Vec3& m) const { return glm::vec3{m.x, m.y, m.z} * m2u; }
};

std::unique_ptr<World> world;

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

void destroyBody(Slot& s)
{
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

[[nodiscard]] b3MeshData* worldMesh(const qmodel_t* map, float m2u, MeshStats& stats)
{
    const double t0 = Sys_DoubleTime();
    struct Face
    {
        int first, count; // into corners
        glm::vec3 normal;
    };
    std::vector<Face> faces;
    std::vector<int> corners; // BSP vertex numbers, each face's outline in order
    std::vector<uint8_t> used(static_cast<size_t>(map->numvertexes), 0);
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
        faces.push_back({static_cast<int>(corners.size()), surf.numedges, normal});
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
            corners.push_back(v);
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
    std::vector<std::pair<uint64_t, int>> grid;
    for(int v = 0; v < map->numvertexes; v++)
    {
        if(used[static_cast<size_t>(v)])
        {
            grid.emplace_back(keyOf(cellOf(position(v))), v);
        }
    }
    std::sort(grid.begin(), grid.end());

    std::vector<int32_t> remap(static_cast<size_t>(map->numvertexes), -1);
    std::vector<b3Vec3> vertices;
    std::vector<int32_t> indices;
    const auto index = [&](int v) {
        int32_t& r = remap[static_cast<size_t>(v)];
        if(r < 0)
        {
            r = static_cast<int32_t>(vertices.size());
            const glm::vec3 p = position(v) / m2u;
            vertices.push_back(b3Vec3{p.x, p.y, p.z});
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
            std::swap(b, c);
        }
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    };

    std::vector<int> outline;
    std::vector<uint8_t> corner; // per outline entry: one of the face's own corners (else a T-junction put in)
    std::vector<std::pair<float, int>> between;
    const bool junctions = vr_box3d_mesh_junctions.value != 0.f;
    for(const Face& f : faces)
    {
        outline.clear();
        corner.clear();
        bool split = false;
        for(int k = 0; k < f.count; k++)
        {
            const int a = corners[static_cast<size_t>(f.first + k)], b = corners[static_cast<size_t>(f.first + (k + 1) % f.count)];
            outline.push_back(a);
            corner.push_back(1);
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
            // The cells along the edge (and those round them), in steps of half a cell.
            between.clear();
            const int steps = std::max(1, static_cast<int>(std::ceil(length / (cell * 0.5f))));
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
                            const uint64_t key = keyOf(c + glm::ivec3{dx, dy, dz});
                            for(auto it = std::lower_bound(grid.begin(), grid.end(), std::make_pair(key, INT32_MIN));
                                it != grid.end() && it->first == key; ++it)
                            {
                                const int v = it->second;
                                if(v == a || v == b)
                                {
                                    continue;
                                }
                                const glm::vec3 p = position(v);
                                const float t = glm::dot(p - pa, dir);
                                if(t > 2.f * onEdge && t < length - 2.f * onEdge && glm::length(p - (pa + dir * t)) <= onEdge)
                                {
                                    between.emplace_back(t, v);
                                }
                            }
                        }
                    }
                }
            }
            if(between.empty())
            {
                continue;
            }
            std::sort(between.begin(), between.end());
            float lastT = 0.f;
            for(const auto& [t, v] : between)
            {
                if(t - lastT < 2.f * onEdge)
                {
                    continue; // (found from two cells, or two BSP vertices at one place)
                }
                lastT = t;
                outline.push_back(v);
                corner.push_back(0);
                stats.junctions++;
                split = true;
            }
        }
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
        vertices.push_back(b3Vec3{middle.x / m2u, middle.y / m2u, middle.z / m2u});
        for(size_t k = 0; k < outline.size(); k++)
        {
            const int a = outline[k], b = outline[(k + 1) % outline.size()];
            triangle(m, index(a), index(b), middle, position(a), position(b), f.normal);
        }
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
std::vector<MeshCache> meshCache; // this map's, the latest used last
constexpr size_t meshCacheScales = 4;

[[nodiscard]] bool sameMap(const MeshCache& c, const qmodel_t* map)
{
    return !strcmp(c.name, map->name) && c.vertexes == map->numvertexes && c.surfaces == map->numsurfaces &&
           c.junctions == (vr_box3d_mesh_junctions.value != 0.f);
}

[[nodiscard]] MeshCache* findMesh(const qmodel_t* map, float m2u)
{
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
[[nodiscard]] b3MeshData* cachedWorldMesh(const qmodel_t* map, float m2u)
{
    if(MeshCache* found = findMesh(map, m2u))
    {
        std::rotate(found, found + 1, meshCache.data() + meshCache.size()); // the latest used last
        return meshCache.back().mesh;
    }
    // Another map's (or other settings'), and the least recently used scale beyond the few kept, let go.
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
    MeshCache& c = meshCache.emplace_back();
    q_strlcpy(c.name, map->name, sizeof(c.name));
    c.vertexes = map->numvertexes;
    c.surfaces = map->numsurfaces;
    c.m2u = m2u;
    c.junctions = vr_box3d_mesh_junctions.value != 0.f;
    c.mesh = worldMesh(map, m2u, c.stats);
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: the mesh of %s: %d faces, %d triangles, %d T-junctions joined in %d faces (%d fanned from the middle), %.1f ms\n", map->name,
            c.stats.faces, c.stats.triangles, c.stats.junctions, c.stats.junctionFaces, c.stats.middleFans, c.stats.ms);
    }
    return c.mesh;
}

// A convex region as the intersection of half-spaces dot(n, p) <= d: its corners (triples of planes meeting inside
// all of them).
struct HalfSpace
{
    glm::dvec3 n;
    double d;
};

[[nodiscard]] std::vector<b3Vec3> corners(const std::vector<HalfSpace>& planes, float m2u)
{
    std::vector<glm::dvec3> points;
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
                if(std::abs(det) < 1e-9)
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
                if(inside && std::none_of(points.begin(), points.end(), [&](const glm::dvec3& q) { return glm::distance(p, q) < 0.05; }))
                {
                    points.push_back(p);
                }
            }
        }
    }
    std::vector<b3Vec3> out;
    out.reserve(points.size());
    for(const glm::dvec3& p : points)
    {
        out.push_back(b3Vec3{static_cast<float>(p.x / m2u), static_cast<float>(p.y / m2u), static_cast<float>(p.z / m2u)});
    }
    return out;
}

// The solid leaves of a brush model's hull 0 (the drawn brushes' space, split by the BSP), each a convex region.
void solidLeaves(const hull_t& hull, int num, std::vector<HalfSpace>& path, float m2u, std::vector<b3HullData*>& out, int depth)
{
    if(num < 0)
    {
        if(num == CONTENTS_SOLID)
        {
            const std::vector<b3Vec3> points = corners(path, m2u);
            if(points.size() >= 4)
            {
                if(b3HullData* h = b3CreateHull(points.data(), static_cast<int>(points.size()), 64))
                {
                    out.push_back(h);
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
    path.push_back({-n, -static_cast<double>(plane.dist)}); // in front: dot(n, p) >= dist
    solidLeaves(hull, node.children[0], path, m2u, out, depth + 1);
    path.back() = {n, static_cast<double>(plane.dist)}; // behind
    solidLeaves(hull, node.children[1], path, m2u, out, depth + 1);
    path.pop_back();
}

[[nodiscard]] const std::vector<b3HullData*>& moverHulls(qmodel_t* model)
{
    auto it = world->moverHulls.find(model);
    if(it != world->moverHulls.end())
    {
        return it->second;
    }
    std::vector<b3HullData*> hulls;
    const hull_t& hull = model->hulls[0];
    if(hull.clipnodes && hull.planes)
    {
        std::vector<HalfSpace> path;
        for(int i = 0; i < 3; i++) // the model's bounds close the leaves open to the outside
        {
            glm::dvec3 n{0.0};
            n[i] = 1.0;
            path.push_back({n, static_cast<double>(model->maxs[i]) + 1.0});
            path.push_back({-n, -static_cast<double>(model->mins[i]) + 1.0});
        }
        solidLeaves(hull, hull.firstclipnode, path, world->m2u, hulls, 0);
    }
    if(hulls.empty()) // nothing solid found: its bounds
    {
        const glm::vec3 lo = vec(model->mins), hi = vec(model->maxs);
        std::array<b3Vec3, 8> points;
        for(int i = 0; i < 8; i++)
        {
            points[i] = world->toM(glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z});
        }
        if(b3HullData* h = b3CreateHull(points.data(), 8, 8))
        {
            hulls.push_back(h);
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: %s: %d convex pieces\n", model->name, static_cast<int>(hulls.size()));
    }
    return world->moverHulls.emplace(model, std::move(hulls)).first->second;
}

// How far the drawn corners reach out of a hull made of some of them (the most any is beyond one of its faces).
[[nodiscard]] float hullShortfall(const b3HullData* hull, const std::vector<b3Vec3>& points)
{
    const b3Plane* planes = b3GetHullPlanes(hull);
    float most = 0.f;
    for(const b3Vec3& p : points)
    {
        float out = -1e9f;
        for(int i = 0; i < hull->faceCount; i++)
        {
            out = std::max(out, b3Dot(planes[i].normal, p) - planes[i].offset);
        }
        most = std::max(most, out);
    }
    return most;
}

// A hull of the drawn corners (metres) with at least `budget` vertices, more as needed to leave none of them further
// out than hullTolerance units (Box3D's limit is 128).
constexpr float hullTolerance = 0.2f;
[[nodiscard]] b3HullData* fittedHull(const std::vector<b3Vec3>& points, int budget)
{
    b3HullData* hull = b3CreateHull(points.data(), static_cast<int>(points.size()), budget);
    while(hull && budget < B3_MAX_HULL_VERTICES && hull->vertexCount >= budget &&
          hullShortfall(hull, points) * world->m2u > hullTolerance)
    {
        budget = std::min(budget * 3 / 2, B3_MAX_HULL_VERTICES);
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
    std::vector<glm::vec3>& vertices = scratch.propVerts;
    if(held::drawnVertices(ent, vertices) && vertices.size() >= 4)
    {
        std::vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.push_back(world->toM(v));
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
    std::vector<glm::vec3>& vertices = scratch.actorVerts;
    const float frame = ent->v.frame;
    ent->v.frame = 0.f;
    const bool drawn = held::drawnVertices(ent, vertices);
    ent->v.frame = frame;
    if(drawn && vertices.size() >= 4)
    {
        std::vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.push_back(world->toM(v));
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
    def.baseMaterial.friction = std::max(vr_throw_friction.value, 0.f);
    def.baseMaterial.restitution = 0.f;
    def.density = 1.f;
    return def;
}

// The prop's shapes on `body`: its hull or its box.
void addPropShapes(edict_t* ent, int num, qmodel_t* model, const glm::vec3& lo, const glm::vec3& hi, b3BodyId body, bool held)
{
    b3ShapeDef def = shapeDef(num, held ? catHeld : catProp, held ? catProp : propMask);
    if(!held && isSolidProp(ent))
    {
        def.filter.categoryBits |= catSolid; // pushed and tipped by the hands' bodies
        def.filter.maskBits |= catHand;
    }
    else if(!held)
    {
        def.filter.maskBits |= catReach; // pushed and held up by the empty hands and the held weapons (syncReach)
    }
    def.density = densityOf(ent, model);
    def.baseMaterial.restitution = isSoft(ent, model) ? 0.f : CLAMP(0.f, vr_throw_restitution.value, 1.f);
    def.enableContactEvents = !held;
    def.enableHitEvents = !held;
    if(isGrenade(model))
    {
        def.baseMaterial.restitution = grenadeRestitution;
        def.enableCustomFiltering = !held; // (shouldCollide: not with its thrower)
    }
    b3HullData* hull = propHull(ent, model, lo, hi);
    const glm::vec3 half = (hi - lo) * 0.5f / world->m2u;
    // A Mass set for its model: the density that gives it (Held Object Offsets).
    if(const float mass = massSetting(model); mass > 0.f)
    {
        const float volume = hull ? hull->volume : 8.f * half.x * half.y * half.z;
        def.density = mass / std::max(volume, 1e-6f);
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

[[nodiscard]] Kind kindOf(edict_t* ent, int num, const std::vector<uint8_t>& carried)
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

[[nodiscard]] std::array<float, 9> scaleFields(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const glm::vec3 a = fieldVec(ent, f.model_scale), b = fieldVec(ent, f.model_scale_origin), c = fieldVec(ent, f.model_offset);
    return {a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z};
}

[[nodiscard]] float playerRadius()
{
    return std::max(vr_box3d_player_radius.value, 1.f) * 0.01f; // metres
}

// What a prop's drawn box and Mass are made from besides its entity (localBox, massSetting): the weapon and prop
// settings (their changes counted), the scales and offsets every weapon's drawing follows, Quake VR's protocol. Its
// count goes up whenever any of them changes (updateShapeGeneration, once a frame).
struct ShapeInputs
{
    unsigned weapons{0}, props{0};
    bool quakevr{false};
    float scales[7]{};
    bool operator==(const ShapeInputs& o) const
    {
        return weapons == o.weapons && props == o.props && quakevr == o.quakevr && !memcmp(scales, o.scales, sizeof(scales));
    }
};
ShapeInputs shapeInputs;
unsigned shapeGeneration = 1;

void updateShapeGeneration()
{
    const ShapeInputs now{weapons::settingsGeneration(), props::settingsGeneration(),
        (cl.protocolflags & PRFL_QUAKEVR) || (sv.active && (sv.protocolflags & PRFL_QUAKEVR)),
        {vr_world_scale.value, vr_gunmodelscale.value, vr_gunmodely.value, vr_leg_holster_model_scale.value,
            vr_leg_holster_model_x_offset.value, vr_leg_holster_model_y_offset.value, vr_leg_holster_model_z_offset.value}};
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
        if(s.massSetting != massSetting(model))
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
    if(s.kind == Kind::Player || (s.kind == Kind::Actor && !s.hull))
    {
        return b3Quat_identity;
    }
    if(s.kind == Kind::Actor)
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
    std::vector<glm::vec3>& corners = scratch.corners;
    corners.clear();
    float lo = 1e9f, hi = -1e9f;
    for(int i = 0; i < n; i++)
    {
        const b3HullData* hull = b3Shape_GetType(shapes[i]) == b3_hullShape ? b3Shape_GetHull(shapes[i]) : nullptr;
        const b3Vec3* points = hull ? b3GetHullPoints(hull) : nullptr;
        for(int k = 0; points && k < hull->vertexCount; k++)
        {
            const glm::vec3 p = world->toU(b3Add(b3RotateVector(xf.q, points[k]), xf.p));
            corners.push_back(p);
            lo = std::min(lo, p.z);
            hi = std::max(hi, p.z);
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
            depth = std::max(depth, tr.endpos[2] - c.z);
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
    const float lift = std::min(depth + 0.05f, 64.f);
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
    s.model = model;
    s.hull = kind == Kind::Actor && model && model->type == mod_alias ? actorHull(ent, model) : nullptr;
    s.frame = static_cast<int>(ent->v.frame);
    s.scale = scaleFields(ent);
    s.brush = model && model->type == mod_brush;
    s.spins = kind == Kind::Fixture && model && (model->flags & EF_ROTATE);
    s.massSetting = massSetting(model);
    s.soft = model && isSoft(ent, model);
    s.origin = vec(ent->v.origin);
    s.angles = vec(ent->v.angles);
    s.born = qcvm->time;
    if(kind == Kind::Prop)
    {
        world->made.push_back(num); // (noteThrows)
    }

    b3BodyDef def = b3DefaultBodyDef();
    def.userData = userOf(num);
    def.position = world->toM(s.origin);
    def.rotation = rotationOf(ent, s);
    def.type = kind == Kind::Prop ? b3_dynamicBody : b3_kinematicBody;

    if(kind == Kind::Prop)
    {
        const bool resting = hasFlag(ent, FL_ONGROUND) && glm::length(vec(ent->v.velocity)) <= 1.f;
        def.isAwake = !resting || resized;
        s.gravityScale = gravityScaleOf(ent);
        def.gravityScale = s.gravityScale;
        def.angularDamping = std::max(vr_throw_spin_drag.value, 0.f);
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
        const b3ShapeDef def2 = shapeDef(num, catMover, catProp);
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
        const float bottom = s.mins.z / world->m2u + r, top = std::max(s.maxs.z / world->m2u - r, bottom + 0.01f);
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
        const float most = std::max(vr_box3d_player_push_speed.value, 0.1f) * world->m2u * dt;
        if(s.kind == Kind::Player && distance > most)
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
    return std::max(linear, turn);
}

// Whether prop `num` passes through the hands' bodies of client `player` now (a throw's grace).
[[nodiscard]] bool graced(int player, int num)
{
    for(World::Grace& g : world->graces)
    {
        if(g.num == num && g.player == player && g.skips && qcvm->time < g.until)
        {
            g.skipped++;
            return true;
        }
    }
    return false;
}

// Once a frame, before the hands' bodies follow the hands (syncReach: `hb.held` is still what each hand carried last
// frame): the throws of the props made this frame (createBody), and the graces over. A throw: a prop let go of by a
// hand (thrown or dropped), or one whose .owner is a player (a thrown weapon: CreateThrownWeapon; a box thrown hard:
// VR_Carry_Release), from the hand nearest it.
void noteThrows()
{
    const float grace = std::max(vr_box3d_throw_grace.value, 0.f);
    const bool debug = vr_debug_box3d.value != 0.f;
    std::erase_if(world->graces, [debug](const World::Grace& g) {
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
                glm::length(v), glm::length(v - fall - g.velocity), glm::length(w), glm::length(w - g.spin), g.skipped);
        }
        return true;
    });
    if(grace <= 0.f && !debug)
    {
        world->made.clear();
        return;
    }
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
        const b3AABB box = b3Body_ComputeAABB(s.body);
        const float reach = std::min(0.5f * glm::length(glmv(box.upperBound) - glmv(box.lowerBound)), 0.5f); // m
        int thrower = 0;
        float speed = 0.f;
        for(int i = 1; i <= svs.maxclients && i < static_cast<int>(world->hands.size()); i++)
        {
            for(int h = 0; h < 2; h++)
            {
                if(world->hands[static_cast<size_t>(i)][static_cast<size_t>(h)].held == num)
                {
                    thrower = i;
                    speed = std::max(speed, throwSpeed(EDICT_NUM(i), h, reach));
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
            world->graces.push_back(g);
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
[[nodiscard]] bool sameFist(const std::vector<glm::vec4>& a, const std::vector<glm::vec4>& b)
{
    if(a.size() != b.size())
    {
        return false;
    }
    for(std::size_t k = 0; k < a.size(); k++)
    {
        const glm::vec4 d = glm::abs(a[k] - b[k]);
        if(std::max(std::max(d.x, d.y), std::max(d.z, d.w)) > 0.05f)
        {
            return false;
        }
    }
    return true;
}

void syncHands(float dt)
{
    QVR_PROFILE("box3d hands");
    const FieldOffsets& f = fields();
    world->hands.resize(static_cast<size_t>(svs.maxclients) + 1);
    const float radius = 0.045f; // m: a fist's
    static const std::vector<glm::vec4> noFist;
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
            const std::vector<glm::vec4>& fist = i == 1 && vr_box3d_hand_push_fist.value ? held::fist(h) : noFist;
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
                    for(std::size_t k = 0; k < fist.size() && !overlaps; k++)
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
                const float most = std::max(vr_box3d_hand_push_speed.value, 0.1f) * world->m2u * dt;
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
    return slot >= 0 ? std::max(weapons::value(slot, weapons::Key::Mass), 0.f) : 0.f;
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
        if(!d.model || realtime - d.when > 0.5 || strcmp(d.model->name, name) != 0)
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
    std::array<b3ShapeId, 4> shapes;
    const int count = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
    std::array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
    for(int i = 0; i < count; i++)
    {
        int n = 0;
        float radius = 0.f;
        if(b3Shape_GetType(shapes[i]) == b3_hullShape)
        {
            const b3HullData* hull = b3Shape_GetHull(shapes[i]);
            const b3Vec3* p = b3GetHullPoints(hull);
            const b3Vec3 c = hull->center;
            n = std::min(hull->vertexCount, static_cast<int>(points.size()));
            for(int k = 0; k < n; k++)
            {
                // Each corner in by the sink along each axis (not past the middle).
                b3Vec3 q = p[k];
                q.x = q.x > c.x ? std::max(c.x, q.x - reachSink) : std::min(c.x, q.x + reachSink);
                q.y = q.y > c.y ? std::max(c.y, q.y - reachSink) : std::min(c.y, q.y + reachSink);
                q.z = q.z > c.z ? std::max(c.z, q.z - reachSink) : std::min(c.z, q.z + reachSink);
                points[static_cast<size_t>(k)] = b3RotateVector(xf.q, q);
            }
        }
        else if(b3Shape_GetType(shapes[i]) == b3_capsuleShape)
        {
            const b3Capsule c = b3Shape_GetCapsule(shapes[i]);
            points[0] = b3RotateVector(xf.q, c.center1);
            points[1] = b3RotateVector(xf.q, c.center2);
            n = 2;
            radius = std::max(0.f, c.radius - reachSink);
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
            std::vector<int>& ignore;
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
                    std::find(c.ignore.begin(), c.ignore.end(), num) == c.ignore.end())
                {
                    c.ignore.push_back(num);
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
    b3ShapeDef shape = shapeDef(i, category, catProp);
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
        std::vector<glm::vec3>& vertices = scratch.propVerts;
        if(held::modelVertices(key.model, key.mirrored, vertices) && vertices.size() >= 4)
        {
            std::vector<b3Vec3> points;
            points.reserve(vertices.size());
            for(const glm::vec3& v : vertices)
            {
                points.push_back(world->toM(v));
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
        static const char* const names[] = {"none", "open hand", "fist", "weapon", "capsule"};
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
// (sunk in it as it was made: its ignore list), what its player just threw (graced), an empty hand a grenade (reachMeets), a prop flying to a hand (a force
// grab's pull: .fg_state 1), the player's own grenade in its first quarter second (leaving the launcher's muzzle,
// inside the gun).
[[nodiscard]] bool reachSkips(const World::HandBody& hb, int player, int num)
{
    if(std::find(hb.ignore.begin(), hb.ignore.end(), num) != hb.ignore.end() || graced(player, num))
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
        keep = std::min(keep, vr_box3d_push_force.value * dt / (m * gain));
    }
    return keep;
}

// A hand's body's mass: an empty hand's vr_box3d_hand_mass; a weapon's its own (Weapon Weights: Mass) and the arm's
// behind it (vr_box3d_weapon_arm_mass), so the heavy ones (the rocket launcher, the hammer) bat harder than the light.
[[nodiscard]] float reachMass(const World::HandBody& hb)
{
    const bool weapon = hb.key.what == World::ReachKey::Weapon || hb.key.what == World::ReachKey::Capsule;
    return weapon ? std::max(vr_box3d_weapon_arm_mass.value, 0.f) + hb.weaponMass : std::max(vr_box3d_hand_mass.value, 0.f);
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
    std::array<b3ShapeId, 1> shapes;
    if(b3Body_GetShapes(hb.reach, shapes.data(), 1) < 1)
    {
        return;
    }
    // The shape's points (its frame, m) and radius.
    std::array<b3Vec3, 64> local;
    int count = 0;
    float radius = 0.f;
    if(b3Shape_GetType(shapes[0]) == b3_hullShape)
    {
        const b3HullData* hull = b3Shape_GetHull(shapes[0]);
        count = std::min(hull->vertexCount, static_cast<int>(local.size()));
        std::copy_n(b3GetHullPoints(hull), count, local.begin());
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
        most = std::max(most, glm::distance(p0 + from.rot * v, p1 + to.rot * v));
    }
    const int pieces = std::min(static_cast<int>(std::ceil(most / piece)), 24);
    if(pieces <= 1 || dt <= 0.f)
    {
        return;
    }
    const auto poseAt = [&](int k) {
        const float t = static_cast<float>(k) / static_cast<float>(pieces);
        return ReachPose{glm::mix(from.pos, to.pos, t), glm::slerp(from.rot, to.rot, t)};
    };
    // What it meets over the points of two poses (their convex hull: the swept piece), relative to the second's place.
    std::vector<int> found;
    const uint64_t category = b3Shape_GetFilter(shapes[0]).categoryBits;
    const auto meets = [&](const ReachPose& a, const ReachPose& b) {
        std::array<b3Vec3, 128> points;
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
                auto& out = *static_cast<std::vector<int>*>(context);
                const int num = numOf(shape);
                if(std::find(out.begin(), out.end(), num) == out.end())
                {
                    out.push_back(num);
                }
                return true;
            },
            &found);
    };
    meets(from, from);
    std::vector<int> struck = found; // (touching it at the start: Box3D's contact)

    // The body's motion over the step: its origin's velocity, and its spin (m/s, rad/s).
    const glm::vec3 linear = (p1 - p0) / dt;
    glm::quat turn = to.rot * glm::inverse(from.rot);
    if(turn.w < 0.f)
    {
        turn = -turn;
    }
    const float angle = 2.f * std::acos(std::min(turn.w, 1.f));
    const float sine = std::sqrt(std::max(0.f, 1.f - turn.w * turn.w));
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
                std::find(struck.begin(), struck.end(), num) != struck.end() || reachSkips(hb, player, num))
            {
                continue;
            }
            struck.push_back(num);
            const b3BodyId prop = world->slots[num].body;
            std::array<b3ShapeId, 1> propShape;
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
                std::erase_if(hb.ignore, [&](int num) {
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
            const bool moved = glm::distance(at, pos) > 0.01f || std::fabs(glm::dot(fromB3(now.q), rot)) < 0.999999f;
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

// The entities, every server frame in edict order: each one's body made, moved, fed or destroyed.
void syncEntities(float dt)
{
    const FieldOffsets& f = fields();
    std::vector<uint8_t>& carried = scratch.carried;
    carried.assign(static_cast<size_t>(qcvm->num_edicts), 0);
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
                if(n > svs.maxclients && n < qcvm->num_edicts)
                {
                    carried[n] = 1;
                }
            }
        }
    }

    updateShapeGeneration();
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        Slot& s = slotOf(num);
        const Kind want = ent->free ? Kind::None : kindOf(ent, num, carried);
        if(want != s.kind || (want != Kind::None && stale(ent, s)))
        {
            const bool resized = want == Kind::Prop && s.kind == Kind::Prop && s.size != props::drawnSize(modelOf(ent));
            destroyBody(s);
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
        default: break;
        }
    }
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

// Before the step, for each awake prop: the hit box along its flight, then the water (the old solver's waterStep,
// once a frame: the lift by how deep it is, the drag, floating flat, the bob).
void beforeStep(float dt)
{
    const float g = sv_gravity.value;
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
        s.arrival = glm::vec3{0.f};
        if(s.kind != Kind::Prop || !b3Body_IsAwake(s.body))
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(num);
        const glm::vec3 com = world->toU(b3Body_GetWorldCenter(s.body));
        glm::vec3 vel = world->toU(b3Body_GetLinearVelocity(s.body));

        if(glm::length(vel) > 1.f)
        {
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
        if(b3Body_GetContactCapacity(s.body) > 0)
        {
            std::array<b3ContactData, 8> contacts;
            if(b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size())) > 0)
            {
                const bool slow = glm::length(vel) < 0.5f * world->m2u;
                const b3Vec3 w = b3Body_GetAngularVelocity(s.body);
                const float k = std::exp((slow || s.soft ? -6.f : -1.f) * (s.soft ? 2.f : 1.f) * dt);
                b3Body_SetAngularVelocity(s.body, b3Vec3{w.x * k, w.y * k, w.z * k});
            }
        }

        const b3AABB box = b3Body_ComputeAABB(s.body);
        const float lo = box.lowerBound.z * world->m2u, hi = box.upperBound.z * world->m2u;
        const float part = g > 0.f ? submerged(com, lo, hi) : 0.f;
        const bool wet = part > 0.f;
        if(wet != s.wet)
        {
            b3Body_EnableSleep(s.body, !wet); // floating, it bobs
            s.wet = wet;
        }
        if(!wet)
        {
            continue;
        }
        const float density = waterDensity(ent);
        const bool floats = density > 1.f;
        const float halfHeight = std::max((hi - lo) * 0.5f, 0.5f);
        const float bob = floats ? 1.f + 0.04f * std::sin(static_cast<float>(qcvm->time) * 2.1f + static_cast<float>(num)) : 1.f;
        // The lift is a force through the step, against the gravity (Box3D's, in the same sub-steps: at rest, where
        // they balance, nothing moves and the velocity stays nought). The drag is on the velocity before the step, so
        // that after the step's lift and gravity it is the old solver's: the drag on (velocity + lift - gravity),
        // which gives the same terminal speed up or down (a box that dived deep rises as fast as before).
        const float gs = g * s.gravityScale;
        const float lift = gs * density * part * bob / world->m2u; // m/s^2
        b3Body_ApplyForceToCenter(s.body, b3Vec3{0.f, 0.f, b3Body_GetMass(s.body) * lift}, true);
        const float stiffness = gs * density / (2.f * halfHeight);
        const float restingPart = std::min(1.f, 1.f / density);
        const float drag = 2.4f * std::sqrt(std::max(stiffness, 0.f)) * std::min(1.f, part / restingPart);
        const float e = std::exp(-drag * dt);
        vel.z = vel.z * e + (e - 1.f) * (gs * density * part * bob - gs) * dt;
        const float drift = std::exp(-1.5f * part * dt);
        vel.x *= drift;
        vel.y *= drift;
        glm::vec3 spin = glmv(b3Body_GetAngularVelocity(s.body)) * std::exp(-8.f * part * dt);
        if(floats) // the side nearest the surface turns up to it
        {
            const glm::mat3 axes = glm::mat3_cast(fromB3(b3Body_GetRotation(s.body)));
            int axis = 0;
            for(int i = 1; i < 3; i++)
            {
                if(std::abs(axes[i].z) > std::abs(axes[axis].z))
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

// What a prop that falls asleep rests on (its groundentity): the body under it (a contact whose normal points up
// from the other to it), else the world.
[[nodiscard]] edict_t* supportOf(const Slot& s)
{
    std::array<b3ContactData, 16> contacts;
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

// The step's touches, as the old solver's: a prop meeting another entity's body (a monster, a door) touches it
// (QC's damage, sounds); props hitting each other hard touch each other (a thrown box into a pile: its knock, the
// throw over). Landing on the world touches nothing (as before).
void touches(std::vector<std::pair<int, int>>& out)
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
            std::swap(a, b);
        }
        const Kind other = kindAt(b);
        if(kindAt(a) == Kind::Prop && (other == Kind::Mover || other == Kind::Actor))
        {
            out.emplace_back(a, b);
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
            out.emplace_back(a, b);
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
            auto it = std::find_if(world->shocks.begin(), world->shocks.end(), [a](const Shock& s) { return s.num == a; });
            if(it == world->shocks.end())
            {
                world->shocks.push_back({a, b, e.approachSpeed});
            }
            else if(e.approachSpeed > it->speed)
            {
                *it = {a, b, e.approachSpeed};
            }
        }
    }
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
    const bool aReach = (b3Shape_GetFilter(a).categoryBits & catReach) != 0;
    if(aReach || (b3Shape_GetFilter(b).categoryBits & catReach) != 0)
    {
        return reachMeets(aReach ? a : b, aReach ? b : a);
    }
    const int na = numOf(a), nb = numOf(b);
    if(na <= 0 || nb <= 0 || na >= qcvm->num_edicts || nb >= qcvm->num_edicts)
    {
        return true;
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

// Box3D's pre-solve, for the fists' spheres' contacts (none with what their player just threw: graced) and the reach
// bodies' (their shapes ask for it), each step: none with what the body skips
// (reachSkips), nor holding up what is heavier than vr_box3d_hand_hold_mass (the prop on top: the normal from the reach
// body within 60 degrees of up); pushed from the side or below, it still is.
bool preSolve(b3ShapeId a, b3ShapeId b, b3Pos point, b3Vec3 normal, void*)
{
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
        return !graced(numOf(aFist ? a : b), numOf(aFist ? b : a)); // (a fist's sphere: noteThrows)
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
    // The normal turned to point from the body to the prop (towards the prop's centre: Box3D's contacts and its
    // continuous collision give it either way round).
    const b3BodyId reachBody = b3Shape_GetBody(reach), propBody = b3Shape_GetBody(other);
    const glm::vec3 at = glmv(point);
    glm::vec3 n = glmv(normal);
    if(glm::dot(n, glmv(b3Body_GetWorldCenter(propBody)) - at) < 0.f)
    {
        n = -n;
    }
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
    const float most = vr_box3d_hand_hold_mass.value;
    const float up = n.z;
    return most <= 0.f || up < 0.5f || b3Body_GetMass(b3Shape_GetBody(other)) <= most;
}

// Pushes by mass (pushShare): the mass behind a hand's body `shape` (kg), 0 for none (a kinematic body's full push: not
// a hand's, or the setting 0). A carried prop: its own mass and each hand's holding it.
[[nodiscard]] float pusherMass(b3ShapeId shape)
{
    const uint64_t category = b3Shape_GetFilter(shape).categoryBits;
    const float hand = std::max(vr_box3d_hand_mass.value, 0.f);
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
        return hand * static_cast<float>(std::max(hands, 1)) + qvr::box3d::propMass(EDICT_NUM(num));
    }
    return 0.f;
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
        filter.maskBits = catProp;
        b3World_OverlapAABB(world->id, box, filter,
            [](b3ShapeId shape, void*) {
                const int num = numOf(shape);
                if(num <= 0 || num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
                {
                    return true;
                }
                auto& out = world->pushed;
                if(std::none_of(out.begin(), out.end(), [num](const Pushed& p) { return p.num == num; }))
                {
                    const b3BodyId prop = world->slots[num].body;
                    out.push_back({num, glmv(b3Body_GetLinearVelocity(prop)), glmv(b3Body_GetAngularVelocity(prop))});
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
    std::array<b3ContactData, 16> contacts;
    for(const Pushed& p : world->pushed)
    {
        const Slot& s = world->slots[static_cast<size_t>(p.num)];
        if(s.kind != Kind::Prop || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const float m = b3Body_GetMass(s.body);
        const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        float pusher = 0.f, strongest = 0.f;
        bool full = false;
        glm::vec3 normal{0.f};
        std::array<b3BodyId, 8> counted;
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
                if(std::none_of(counted.begin(), counted.begin() + static_cast<std::ptrdiff_t>(countedN),
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
    const float g = sv_gravity.value;
    if(g != world->gravity)
    {
        b3World_SetGravity(world->id, b3Vec3{0.f, 0.f, -g / world->m2u});
        world->gravity = g;
    }
    const float friction = std::max(vr_throw_friction.value, 0.f), restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);
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
    std::array<b3ShapeId, 64> shapes;
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
            b3Body_SetAngularDamping(s.body, std::max(vr_throw_spin_drag.value, 0.f));
        }
    }
}

void destroyWorld()
{
    if(!world)
    {
        return;
    }
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
    world = std::make_unique<World>();
    world->map = sv.worldmodel;
    world->m2u = units::metresToUnits();
    world->gravity = sv_gravity.value;
    world->friction = std::max(vr_throw_friction.value, 0.f);
    world->restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);

    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = b3Vec3{0.f, 0.f, -world->gravity / world->m2u};
    def.enableSleep = true;
    def.enableContinuous = true;
    // One worker and no task callbacks: Box3D's serial path runs every task inline on this thread (no scheduler,
    // no threads).
    def.workerCount = 1;
    def.enqueueTask = nullptr;
    def.finishTask = nullptr;
    def.userTaskContext = nullptr;
    // Room for the map's entities as bodies and a pile's contacts from the start: no growing (a reallocation and copy
    // of Box3D's arrays) in the frame a pile collapses.
    def.capacity.staticBodyCount = 1;
    def.capacity.staticShapeCount = 1;
    def.capacity.dynamicBodyCount = std::max(qcvm->num_edicts, 256);
    def.capacity.dynamicShapeCount = std::max(qcvm->num_edicts, 256) + 256;
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
        b3ShapeDef shape = shapeDef(0, catWorld, catProp);
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
[[nodiscard]] std::vector<edict_t*> entitiesNamed(const char* which)
{
    std::vector<edict_t*> out;
    if(which[0] >= '0' && which[0] <= '9')
    {
        const int num = Q_atoi(which);
        if(num > 0 && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
        {
            out.push_back(EDICT_NUM(num));
        }
        return out;
    }
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && (!strcmp(which, "props") ? isRigid(e) && modelOf(e) : !strcmp(PR_GetString(e->v.classname), which)))
        {
            out.push_back(e);
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
    std::vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t count = std::min(list.size(), static_cast<size_t>(std::max(0, Q_atoi(Cmd_Argv(2)))));
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
    std::vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const int rows = std::max(1, Q_atoi(Cmd_Argv(2)));
    const glm::vec3 centre{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float yaw = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 0.f;
    const glm::vec3 side{std::cos(glm::radians(yaw + 90.f)), std::sin(glm::radians(yaw + 90.f)), 0.f};
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
            height = std::max(height, hi.z - lo.z);
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
    const std::vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t per = static_cast<size_t>(std::max(1, Q_atoi(Cmd_Argv(2))));
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
    MSG_WriteByte(&sv.datagram, svc_temp_entity);
    MSG_WriteByte(&sv.datagram, TE_EXPLOSION);
    for(const float c : at)
    {
        MSG_WriteCoord(&sv.datagram, c, sv.protocolflags);
    }
    Con_Printf("vr_physics_blast: %.0f at %.0f %.0f %.0f\n", damage, at[0], at[1], at[2]);
}

// vr_physics_hash: a hash of every rigid body's origin, angles, velocity and spin, bit for bit (determinism tests: two
// runs of the same script print the same).
void hash_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    uint64_t h = 1469598103934665603ull;
    int count = 0;
    for(edict_t* e : entitiesNamed("props"))
    {
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

// vr_physics_list [<classname | props>]: the rigid bodies (or those), where they are and how they move.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    const std::vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
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
                model && massSetting(model) > 0.f ? " (Held Object Offsets)" : "",
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
                drawn = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            }
            Con_Printf("    size x%.2f: body %.1f %.1f %.1f, box %.1f %.1f %.1f, drawn x%.3f\n",
                model ? qvr::props::drawnSize(model) : 1.f, body.x, body.y, body.z, e->v.size[0], e->v.size[1], e->v.size[2],
                drawn);
        }
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
    const std::vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
    std::vector<glm::vec3> vertices; // (a debug command's: made each call)
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
            low = std::min(low, v.z);
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
                sunk = std::max(sunk, tr.endpos[2] - v.z);
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
                    hullLow = std::min(hullLow, (p.z + xf.p.z) * world->m2u);
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
            worst = std::max(worst, sunk);
            total += sunk;
            counted++;
        }
    }
    Con_Printf("vr_physics_sink: %d on a floor, sunk %.2f on average, %.2f at most\n", counted,
        counted ? total / static_cast<float>(counted) : 0.f, worst);
}

// vr_physics_player [onto <number | classname>]: the first player's origin, velocity and ground (FL_ONGROUND,
// .groundentity) and what is under his feet, for tests of standing on props; with onto, first put on top of that
// entity's box (the first of them), still, noclip off.
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
        const std::vector<edict_t*> list = entitiesNamed(Cmd_Argv(2));
        if(list.empty())
        {
            Con_Printf("vr_physics_player: no %s\n", Cmd_Argv(2));
            return;
        }
        const edict_t* e = list.front();
        p->v.origin[0] = (e->v.absmin[0] + e->v.absmax[0]) * 0.5f;
        p->v.origin[1] = (e->v.absmin[1] + e->v.absmax[1]) * 0.5f;
        p->v.origin[2] = e->v.absmax[2] - p->v.mins[2] + 1.f;
        VectorCopy(vec3_origin, p->v.velocity);
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

void registerCommands()
{
    static bool registered = false;
    if(!registered)
    {
        registered = true;
        Cmd_AddCommand("vr_physics_player", player_f);
        Cmd_AddCommand("vr_physics_stack", stack_f);
        Cmd_AddCommand("vr_physics_pyramid", pyramid_f);
        Cmd_AddCommand("vr_physics_list", list_f);
        Cmd_AddCommand("vr_physics_loose", loose_f);
        Cmd_AddCommand("vr_physics_pile", pile_f);
        Cmd_AddCommand("vr_physics_hash", hash_f);
        Cmd_AddCommand("vr_physics_blast", blast_f);
        Cmd_AddCommand("vr_physics_sink", sink_f);
        Cmd_AddCommand("vr_physics_inlevel", inLevel_f);
        Cmd_AddCommand("vr_physics_approach", approach_f);
        Cmd_AddCommand("vr_physics_spawn", spawn_f);
        Cmd_AddCommand("vr_physics_forcegrab", forcegrabCheck_f);
    }
}

[[nodiscard]] bool wanted()
{
    return fields().vr_rigid >= 0 && sv.worldmodel; // (a mod without .vr_rigid has no rigid bodies)
}

} // namespace

namespace qvr::box3d
{

bool toss(edict_t* ent)
{
    (void)ent;
    registerCommands();
    return wanted();
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
        const float mass = std::max(b3Body_GetMass(s.body), 1e-3f);
        const float speed = std::min(4.f * points * CLAMP(0.5f, std::sqrt(referenceMass / mass), 2.f), 600.f); // u/s
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
            lines::line(centre + r * glm::vec3{std::cos(t0), std::sin(t0), 0.f}, centre + r * glm::vec3{std::cos(t1), std::sin(t1), 0.f}, width,
                colour, colour);
        }
    }
    for(int i = 0; i < 4; i++)
    {
        const float t = glm::half_pi<float>() * static_cast<float>(i);
        const glm::vec3 o = r * glm::vec3{std::cos(t), std::sin(t), 0.f};
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
    std::array<b3ShapeId, 64> shapes;
    std::array<b3ContactData, 16> contacts;
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
    if(!world || num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Prop)
    {
        return false;
    }
    const b3BodyId body = world->slots[num].body;
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
    if(vr_debug_box3d.value)
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
    glm::vec3 rel = (v - relativeTo) * std::clamp(keep, 0.f, 1.f);
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
    const float ks = std::clamp(keepSpin, 0.f, 1.f);
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
    if(const float mass = massSetting(model); mass > 0.f)
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
    return (hull ? hull->volume : size.x * size.y * size.z) * densityOf(ent, model);
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
    hasBody = world && num > 0 && num < static_cast<int>(world->slots.size()) &&
              B3_IS_NON_NULL(world->slots[static_cast<std::size_t>(num)].body);
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
    const b3ShapeProxy proxy{&zero, 1, std::max(radius, 0.01f) / world->m2u};
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
    const Slot& s = world->slots[static_cast<std::size_t>(num)];
    const World::HandBody& hb = world->hands[static_cast<std::size_t>(player)][static_cast<std::size_t>(hand)];
    if(s.kind != Kind::Prop || !b3Body_IsValid(s.body))
    {
        return false;
    }
    std::array<b3ContactData, 16> contacts;
    const int count = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& c = contacts[static_cast<std::size_t>(i)];
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
    filter.categoryBits = catProp;
    filter.maskBits = catWorld | catMover;
    return filter;
}

[[nodiscard]] std::array<b3Vec3, 8> boxCorners(const glm::vec3& lo, const glm::vec3& hi, const glm::quat& rot, float in)
{
    std::array<b3Vec3, 8> out;
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
    // (`in` units inside its surface: touching isn't in.)
    const std::array<b3Vec3, 8> corners = boxCorners(lo, hi, rot, in);
    const b3ShapeProxy proxy{corners.data(), 8, 0.f};
    bool any = false;
    b3World_OverlapShape(world->id, world->toM(at), &proxy, levelFilter(),
        [](b3ShapeId, void* context) {
            *static_cast<bool*>(context) = true;
            return false;
        },
        &any);
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
        const float most = 0.5f * std::min({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z});
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
    const float mass = std::max(vr_box3d_player_mass.value, 0.f);
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

// Before the step: each solid prop a player walked into this frame (VR_PlayerBumps: Quake's move stopped at its side)
// is shoved, as Source's player shadow shoves what it walks into: towards his pace (vr_box3d_player_push_speed at full
// stick) shared by their masses (vr_box3d_player_shove of his vr_box3d_player_mass, and its own: a light box goes at
// nearly his pace, a heavy one slower), with at most that share of his weight (a force: a prop too heavy for it to
// overcome the floor's friction doesn't move). His next move follows it.
void shoveBumped(float dt)
{
    const float share = vr_box3d_player_shove.value;
    const float mp = std::max(vr_box3d_player_mass.value, 1.f);
    const float g = -b3World_GetGravity(world->id).z;
    for(const World::Bump& b : world->bumps)
    {
        if(share <= 0.f || !solidProp(b.num))
        {
            continue;
        }
        const b3BodyId body = world->slots[b.num].body;
        const float mb = std::max(b3Body_GetMass(body), 0.1f);
        // His effort: his speed into it, against a frame's acceleration at full stick (Quake stopped him at it last
        // frame: what he has is what this frame's move gave him, less the ground's friction), up to all of it.
        const float full = std::max(0.5f * sv_accelerate.value * sv_maxspeed.value * dt, 1.f);
        const float u = std::max(vr_box3d_player_push_speed.value, 0.1f) * std::min(b.speed / full, 1.f);
        const float target = u * share * mp / (share * mp + mb); // (his shove's mass and its own)
        const float along = glm::dot(glmv(b3Body_GetLinearVelocity(body)), b.dir);
        if(target <= along)
        {
            continue;
        }
        const float impulse = std::min(mb * (target - along), share * mp * g * dt); // N s
        // (Low, a quarter of its height up, under its centre: the floor's friction, at its bottom, doesn't tip a tall box
        // over as pushed at its centre it did.)
        edict_t* ent = EDICT_NUM(b.num);
        const b3Pos centre = b3Body_GetWorldCenter(body);
        const glm::vec3 at{static_cast<float>(centre.x), static_cast<float>(centre.y), (ent->v.absmin[2] + 0.25f * (ent->v.absmax[2] - ent->v.absmin[2])) / world->m2u};
        b3Body_ApplyLinearImpulse(body, b3v(b.dir * impulse), b3v(at), true);
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: player %d walks into %d at %.0f u/s: shoved with %.1f N s (%.0f kg, to %.2f m/s)\n", b.player,
                b.num, b.speed, impulse, mb, target);
        }
    }
    world->bumps.clear();
}

// Before the step: a solid prop in a player's body (toppled onto him, shoved into him; its drawn shape in his box, less
// a unit), not the one he stands on, is pushed out of it, away from his middle (never down), at 1 m/s at least; his moves
// aren't blocked by it meanwhile (VR_PropLetsOut). Source pushes a prop stuck in the player out likewise.
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
        std::array<b3Vec3, 8> corners;
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
            std::array<int, 8> nums;
            int count;
        } q{standingOn(player), {}, 0};
        b3World_OverlapShape(world->id, world->toM(origin), &proxy, filter,
            [](b3ShapeId shape, void* context) {
                Query& oq = *static_cast<Query*>(context);
                const int num = numOf(shape);
                if(num != oq.ground && solidProp(num) && oq.count < static_cast<int>(oq.nums.size()) &&
                   std::find(oq.nums.begin(), oq.nums.begin() + oq.count, num) == oq.nums.begin() + oq.count)
                {
                    oq.nums[static_cast<size_t>(oq.count++)] = num;
                }
                return true;
            },
            &q);
        const glm::vec3 middle = origin + 0.5f * (glm::vec3{mins[0], mins[1], mins[2]} + glm::vec3{maxs[0], maxs[1], maxs[2]});
        for(int k = 0; k < q.count; k++)
        {
            const b3BodyId body = world->slots[q.nums[static_cast<size_t>(k)]].body;
            glm::vec3 away = world->toU(b3Body_GetWorldCenter(body)) - middle;
            away.z = std::max(away.z, 0.f);
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
    return std::clamp(1.f - down / weightFadeSpeed, 0.f, 1.f);
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
            const float share = std::min(dt, st.pushLeft) / dt * underFeet(pushed, st.pushLocal);
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
                const float most = std::min(mass, 0.5f * b3Body_GetMass(body));
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
    std::array<glm::dvec3, 3> axes{};
    glm::dvec3 half{0.0};
    std::array<glm::dvec2, columnSides> ring{}; // the player's column round his origin: its corners across ...
    double bottom{0.0}, top{0.0};               // ... from his feet to his head
    std::array<glm::dvec3, 2 * (4 + columnSides / 2 + 3 * (1 + columnSides / 2))> normals{};
    int count{0};
};

void addNormal(PropShape& p, glm::dvec3 n)
{
    const double length = glm::length(n);
    if(length < 1e-6)
    {
        return;
    }
    n /= length;
    for(int k = 0; k < 3; k++)
    {
        n[k] = std::abs(n[k]) < 1e-7 ? 0.0 : n[k]; // (a wall's normal is flat: Quake's step up needs normal z 0)
    }
    n = glm::normalize(n);
    p.normals[static_cast<size_t>(p.count++)] = n;
    p.normals[static_cast<size_t>(p.count++)] = -n;
}

// The prop `touch` (slot s) and the player's box (mins, maxs from his origin) as they meet.
[[nodiscard]] PropShape propShape(edict_t* touch, const Slot& s, const float* mins, const float* maxs)
{
    PropShape p;
    const glm::mat3 axes = held::axesFromAngles(touch->v.angles, s.brush);
    const glm::vec3 mid = (s.mins + s.maxs) * 0.5f;
    p.centre = glm::dvec3{vec(touch->v.origin)} + glm::dvec3{axes * mid};
    p.half = glm::dvec3{(s.maxs - s.mins) * 0.5f};
    int up = 0;
    for(int k = 0; k < 3; k++)
    {
        p.axes[static_cast<size_t>(k)] = glm::normalize(glm::dvec3{axes[k]});
        if(std::abs(axes[k].z) > std::abs(axes[up].z))
        {
            up = k;
        }
    }
    // The column turned with the prop's sides (its yaw): an upright box's faces meet its flats, at exactly his half-width.
    const glm::dvec3 side = p.axes[static_cast<size_t>((up + 1) % 3)];
    const double yaw = std::atan2(side.y, side.x);
    const double r = 0.5 * std::min(maxs[0] - mins[0], maxs[1] - mins[1]);
    const double corner = r / std::cos(glm::pi<double>() / columnSides);
    const glm::dvec2 at{0.5 * (mins[0] + maxs[0]), 0.5 * (mins[1] + maxs[1])};
    for(int k = 0; k < columnSides; k++)
    {
        const double a = yaw + (k + 0.5) * 2.0 * glm::pi<double>() / columnSides;
        p.ring[static_cast<size_t>(k)] = at + corner * glm::dvec2{std::cos(a), std::sin(a)};
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
        const glm::dvec3 flat{std::cos(a), std::sin(a), 0.0}, edge{-std::sin(a), std::cos(a), 0.0};
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
        box += p.half[k] * std::abs(glm::dot(n, p.axes[static_cast<size_t>(k)]));
    }
    double ring = -1e300;
    for(const glm::dvec2& v : p.ring)
    {
        ring = std::max(ring, -(n.x * v.x + n.y * v.y));
    }
    return box + ring + std::max(-n.z * p.bottom, -n.z * p.top);
}

// Quake 3's brush trace of the player's origin from start to end against the sum's half-spaces. False: missed.
[[nodiscard]] bool traceProp(const PropShape& p, const glm::dvec3& start, const glm::dvec3& end, trace_t& trace)
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
            leave = std::min(leave, (d1 + traceEpsilon) / (d1 - d2));
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
    const double f = std::max(enter, 0.0);
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
            out[i] = std::max(std::abs(t) - p.half[i], 0.0);
        }
        best = std::min(best, glm::length(out));
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
    const std::vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
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
        vec3_t s{static_cast<float>(shape.centre.x) + std::cos(a) * reach, static_cast<float>(shape.centre.y) + std::sin(a) * reach, z};
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
        lo = std::min(lo, gap);
        hi = std::max(hi, gap);
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

} // namespace

// SV_FlyMove: a solid prop is ground to a player (vr_box3d_player_stand) as a brush is.
extern "C" int VR_StandsOn(edict_t* ent, edict_t* ground)
{
    if(!world || !vr_box3d_player_stand.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(ground);
    return num >= 1 && num <= svs.maxclients && g > svs.maxclients && g < static_cast<int>(world->slots.size()) &&
           world->slots[g].kind == Kind::Prop && static_cast<int>(ground->v.solid) == SOLID_BBOX;
}

// SV_FlyMove: a player's move stopped by a solid prop's side (not its top: he stands on that) shoves it (shoveBumped).
extern "C" void VR_PlayerBumps(edict_t* ent, edict_t* other, const float* normal)
{
    if(!world || vr_box3d_player_shove.value <= 0.f)
    {
        return;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(other);
    if(num < 1 || num > svs.maxclients || !solidProp(g) || normal[2] < -0.7f)
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
    world->bumps.push_back({num, g, dir, speed});
}

// SV_ClipToLinks: a player's move that starts inside a solid prop's box (toppled onto him) goes on through it, not the
// one he stands on (riding it, rideStanding steps him up out of it).
extern "C" int VR_PropLetsOut(edict_t* mover, edict_t* touch)
{
    if(!world || !vr_box3d_player_unstick.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover), g = NUM_FOR_EDICT(touch);
    return num >= 1 && num <= svs.maxclients && solidProp(g) &&
           !((static_cast<int>(mover->v.flags) & FL_ONGROUND) && PROG_TO_EDICT(mover->v.groundentity) == touch);
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

extern "C" int VR_PushSkips(edict_t* ent)
{
    if(!world)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent);
    return num < static_cast<int>(world->slots.size()) && world->slots[num].kind == Kind::Prop;
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
    if(!world || world->map != sv.worldmodel || world->m2u != units::metresToUnits() ||
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
    updateSettings();
    {
        QVR_PROFILE("box3d sync");
        syncEntities(dt);
        syncHands(dt);
        noteThrows();
        syncReach(dt);
    }
    {
        QVR_PROFILE("box3d water and hits");
        beforeStep(dt);
        beforeStanding();
        shoveBumped(dt);
        unstickProps();
    }
    const double t1 = Sys_DoubleTime();

    // Box3D's step, in pieces of at most 1/45 s (a slow server frame).
    std::vector<std::pair<int, int>>& impacts = world->impacts;
    impacts.clear();
    world->shocks.clear();
    // (At most three: after a hitch (a level's load, a saved game, a slow frame; Quake's frame time is at most a tenth of
    // a second) the step catches up in pieces of up to 1/30 s rather than adding more steps to the slow frame.)
    const int pieces = CLAMP(1, static_cast<int>(std::ceil(dt * 45.f - 0.01f)), 3);
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    {
        QVR_PROFILE("box3d step");
        for(int i = 0; i < pieces; i++)
        {
            notePushed(dt / static_cast<float>(pieces));
            pressStanding(dt / static_cast<float>(pieces));
            b3World_Step(world->id, dt / static_cast<float>(pieces), substeps);
            limitPushes(dt / static_cast<float>(pieces));
            world->steps++;
            touches(impacts);
        }
    }
    const double t2 = Sys_DoubleTime();
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

    // The props into their entities (the awake ones, and those that just fell asleep).
    QVR_PROFILE("box3d write");
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
        if(s.kind == Kind::Prop && (!s.asleep || b3Body_IsAwake(s.body)))
        {
            edict_t* ent = EDICT_NUM(num);
            writeProp(ent, s);
            if(ent->free)
            {
                destroyBody(s);
            }
        }
    }
    rideStanding();

    // Then what they touched, in the step's order, each pair once.
    for(size_t i = 0; i < impacts.size(); i++)
    {
        const auto [a, b] = impacts[i];
        if(std::find(impacts.begin(), impacts.begin() + static_cast<std::ptrdiff_t>(i), impacts[i]) != impacts.begin() + static_cast<std::ptrdiff_t>(i))
        {
            continue;
        }
        edict_t* ea = EDICT_NUM(a);
        edict_t* eb = EDICT_NUM(b);
        if(ea->free || eb->free)
        {
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

} // namespace qvr::box3d
