// vr_box3d.cpp -- the rigid bodies in Box3D (vr_physics_engine 1); see vr_box3d.hpp and docs/vr-port/ROUND21.md,
// "Box3D physics".
//
// Quake VR's own solver (vr_rigid.cpp, vr_physics_engine 0) moves each rigid body on its own against the BSP and
// the solid entities' boxes: bodies never meet, so nothing stacks. Here every body lives in one Box3D world (Erin
// Catto's engine, Quake/vr/external/box3d), stepped once a server frame at the end of SV_Physics, after every
// entity has thought and moved (VR_PhysicsFrameEnd):
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
//   kind. Carried ones (in a hand, or both) are kinematic, following the hand, so they push other props.
// - Box3D is authoritative for props: their origin, angles, velocity (.velocity, the centre of mass's), spin
//   (.vr_spin, rad/s) and sleep (FL_ONGROUND and its groundentity) are written back every frame. What QC changes
//   (a throw, a nudge, a force grab's drop, a knock, a teleport, keepInWorld's put-back) is seen against what was
//   written last and fed in, waking the body.
// - The old solver's behaviours are kept: the monsters' hit box along a thrown thing's flight (vr_throw_hitbox),
//   touches of what props hit (QC's damage), water (lift by depth, drag, floating flat, the bob), the splashes (the
//   water transition), vr_throw_restitution and vr_throw_friction as the materials, vr_throw_spin_drag.
//
// Single-threaded: the world has one worker and no task callbacks, which Box3D runs serially (each task inline:
// b3DefaultAddTaskFcn in physics_world.c); no scheduler, no threads. Deterministic: the same calls in the same
// order (entities in edict order) give the same result.

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"

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

// Collision categories: props collide with everything; the rest only with props.
constexpr uint64_t catWorld = 1;
constexpr uint64_t catMover = 2;
constexpr uint64_t catActor = 4;
constexpr uint64_t catPlayer = 8;
constexpr uint64_t catProp = 16;
constexpr uint64_t catHeld = 32;

enum class Kind : uint8_t
{
    None,
    Prop,   // a rigid body: dynamic
    Held,   // a prop carried in a hand (or two): kinematic, following it
    Mover,  // a brush entity: kinematic
    Actor,  // a monster or another solid box: kinematic
    Player, // a player's body: a kinematic capsule
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

// The prop's box as drawn, in its axes from its origin (vr_rigid.cpp's localBox: the networked scale and offset,
// the weapon scaling; never thinner than a unit).
void localBox(edict_t* ent, qmodel_t* model, glm::vec3& lo, glm::vec3& hi)
{
    if(!model || model->type != mod_alias)
    {
        lo = vec(ent->v.mins);
        hi = vec(ent->v.maxs);
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

// Water, as vr_rigid.cpp: things float (items, backpacks, thrown weapons: 60% under at rest); gibs sink, slowly.
constexpr float floatDensity = 1.f / 0.6f; // relative to water's (the lift's scale)
constexpr float sinkDensity = 0.5f;

[[nodiscard]] float waterDensity(edict_t* ent)
{
    const bool gib = hasFlag(ent, physics::FL_FORCEGRABBABLE) && !hasFlag(ent, FL_ITEM);
    return gib ? sinkDensity : floatDensity;
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

// Densities (kg/m^3) of the props' hulls: only their ratios matter (what knocks what how far).
[[nodiscard]] float densityOf(edict_t* ent, const qmodel_t* model)
{
    if(model->type == mod_brush)
    {
        return 400.f; // ammo and health boxes: full of shells, nails, cells, medkits
    }
    if(!strcmp(PR_GetString(ent->v.classname), "thrown_weapon"))
    {
        return 700.f; // guns and blades (their hulls are partly air)
    }
    if(hasFlag(ent, FL_ITEM))
    {
        return strstr(model->name, "armor") ? 600.f : 250.f; // armour; backpacks
    }
    return 1000.f; // gibs and heads: flesh
}

// Soft things (backpacks, gibs, heads) land with a thud: no bounce, and their tumble dies away fast on the ground (a
// rigid hull of a backpack lands on an edge and tumbles down a gentle slope like a crate).
[[nodiscard]] bool isSoft(edict_t* ent, const qmodel_t* model)
{
    return model->type == mod_alias && strcmp(PR_GetString(ent->v.classname), "thrown_weapon") && !strstr(model->name, "armor");
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
    float gravityScale{1.f};
    bool asleep{false};
    bool wet{false};      // floating: kept awake (it bobs)
    bool bullet{false};   // fast: continuous collision against other props too
    bool soft{false};     // isSoft
    bool brush{false};    // angles as a brush model's
};

// The hulls made for models, by what they were made from (and the world's scale).
struct PropHullKey
{
    const qmodel_t* model;
    int frame;
    std::array<float, 6> box;
    auto operator<=>(const PropHullKey&) const = default;
};

struct World
{
    b3WorldId id{};
    const qmodel_t* map{nullptr};
    float m2u{1.f};      // units a metre
    float gravity{0.f};  // sv_gravity at the last update
    float friction{-1.f}, restitution{-1.f};
    b3MeshData* mesh{nullptr};
    b3ShapeId worldShape{b3_nullShapeId};
    std::vector<Slot> slots; // by edict number
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
[[nodiscard]] b3MeshData* worldMesh(const qmodel_t* map, float m2u)
{
    std::vector<int32_t> remap(static_cast<size_t>(map->numvertexes), -1);
    std::vector<b3Vec3> vertices;
    std::vector<int32_t> indices;
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB))
        {
            continue;
        }
        glm::vec3 normal = vec(surf.plane->normal);
        if(surf.flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        const auto vertex = [&](int k) {
            const int e = map->surfedges[surf.firstedge + k];
            return static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
        };
        const auto position = [&](int v) { return vec(map->vertexes[v].position); };
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
        const int a = vertex(0);
        for(int k = 2; k < surf.numedges; k++)
        {
            int b = vertex(k - 1), c = vertex(k);
            const glm::vec3 n = glm::cross(position(b) - position(a), position(c) - position(a));
            if(glm::length(n) < 1e-3f)
            {
                continue; // degenerate
            }
            if(glm::dot(n, normal) < 0.f)
            {
                std::swap(b, c);
            }
            indices.push_back(index(a));
            indices.push_back(index(b));
            indices.push_back(index(c));
        }
    }
    if(indices.empty())
    {
        return nullptr;
    }
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
    return b3CreateMesh(&def, nullptr, 0);
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
    thread_local std::vector<glm::vec3> vertices;
    if(held::drawnVertices(ent, vertices) && vertices.size() >= 4)
    {
        std::vector<b3Vec3> points;
        points.reserve(vertices.size());
        for(const glm::vec3& v : vertices)
        {
            points.push_back(world->toM(v));
        }
        // A weapon's shape in detail (it rests on its side, its grip, its magazine); the rest a little blockier
        // (a backpack, a gib: a rounded hull rolls down a slope, a real one's give stops it).
        const bool weapon = !strcmp(PR_GetString(ent->v.classname), "thrown_weapon");
        hull = b3CreateHull(points.data(), static_cast<int>(points.size()), weapon ? 32 : 16);
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
    b3ShapeDef def = shapeDef(num, held ? catHeld : catProp, held ? catProp : catWorld | catMover | catActor | catPlayer | catProp | catHeld);
    def.density = densityOf(ent, model);
    def.baseMaterial.restitution = isSoft(ent, model) ? 0.f : CLAMP(0.f, vr_throw_restitution.value, 1.f);
    def.enableContactEvents = !held;
    def.enableHitEvents = !held;
    if(b3HullData* hull = propHull(ent, model, lo, hi))
    {
        b3CreateHullShape(body, &def, hull);
        return;
    }
    const glm::vec3 half = (hi - lo) * 0.5f / world->m2u;
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
    if(isRigid(ent) && (movetype == MOVETYPE_TOSS || movetype == MOVETYPE_BOUNCE))
    {
        return model->type == mod_alias || model->type == mod_brush ? Kind::Prop : Kind::None;
    }
    const int solid = static_cast<int>(ent->v.solid);
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

// Whether the body made for `s` no longer fits the entity (its model, frame, scale or box changed).
[[nodiscard]] bool stale(edict_t* ent, const Slot& s)
{
    switch(s.kind)
    {
    case Kind::Prop:
    case Kind::Held:
        return s.model != modelOf(ent) || s.frame != static_cast<int>(ent->v.frame) || s.scale != scaleFields(ent);
    case Kind::Mover: return s.model != modelOf(ent);
    case Kind::Actor: return s.mins != vec(ent->v.mins) || s.maxs != vec(ent->v.maxs);
    case Kind::Player: return s.radius != playerRadius() || s.mins != vec(ent->v.mins) || s.maxs != vec(ent->v.maxs);
    default: return false;
    }
}

void writeProp(edict_t* ent, Slot& s);

void createBody(edict_t* ent, int num, Slot& s, Kind kind)
{
    qmodel_t* model = modelOf(ent);
    s.kind = kind;
    s.model = model;
    s.frame = static_cast<int>(ent->v.frame);
    s.scale = scaleFields(ent);
    s.brush = model && model->type == mod_brush;
    s.soft = model && isSoft(ent, model);
    s.origin = vec(ent->v.origin);
    s.angles = vec(ent->v.angles);

    b3BodyDef def = b3DefaultBodyDef();
    def.userData = userOf(num);
    def.position = world->toM(s.origin);
    def.rotation = kind == Kind::Actor || kind == Kind::Player ? b3Quat_identity : toB3(turnOf(ent->v.angles, s.brush));
    def.type = kind == Kind::Prop ? b3_dynamicBody : b3_kinematicBody;

    if(kind == Kind::Prop)
    {
        const bool resting = hasFlag(ent, FL_ONGROUND) && glm::length(vec(ent->v.velocity)) <= 1.f;
        def.isAwake = !resting;
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
        const glm::vec3 half = (s.maxs - s.mins) * 0.5f / world->m2u;
        const b3BoxHull box = b3MakeOffsetBoxHull(half.x, half.y, half.z, world->toM((s.mins + s.maxs) * 0.5f));
        const b3ShapeDef def2 = shapeDef(num, catActor, catProp);
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
        const b3ShapeDef def2 = shapeDef(num, catPlayer, catProp);
        b3CreateCapsuleShape(s.body, &def2, &capsule);
        break;
    }
    default: break;
    }

    if(kind == Kind::Prop)
    {
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
    const bool moved = origin != s.origin || angles != s.angles;
    if(!moved && !b3Body_IsAwake(s.body))
    {
        return;
    }
    const b3Quat rot = s.kind == Kind::Actor || s.kind == Kind::Player ? b3Quat_identity : toB3(turnOf(ent->v.angles, s.brush));
    if(glm::distance(origin, s.origin) > 64.f)
    {
        b3Body_SetTransform(s.body, world->toM(origin), rot);
        b3Body_SetLinearVelocity(s.body, b3Vec3_zero);
        b3Body_SetAngularVelocity(s.body, b3Vec3_zero);
    }
    else
    {
        b3Body_SetTargetTransform(s.body, b3WorldTransform{world->toM(origin), rot}, dt, true);
    }
    s.origin = origin;
    s.angles = angles;
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
    thread_local std::vector<uint8_t> carried;
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

    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        Slot& s = slotOf(num);
        const Kind want = ent->free ? Kind::None : kindOf(ent, num, carried);
        if(want != s.kind || (want != Kind::None && stale(ent, s)))
        {
            destroyBody(s);
            if(want != Kind::None)
            {
                createBody(ent, num, s, want);
            }
            continue;
        }
        switch(s.kind)
        {
        case Kind::Prop: feedProp(ent, s); break;
        case Kind::Held:
        case Kind::Mover:
        case Kind::Actor:
        case Kind::Player: follow(ent, s, dt); break;
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
// (vr_throw_hitbox) along its flight this frame (vr_rigid.cpp's touchNearby).
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
    const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NORMAL, ent);
    edict_t* hit = tr.ent;
    if(!hit || hit == qcvm->edicts || hit->free || hit == PROG_TO_EDICT(ent->v.owner) || hit->v.takedamage == 0.f)
    {
        return;
    }
    SV_Impact(ent, hit);
}

// Before the step, for each awake prop: the hit box along its flight, then the water (vr_rigid.cpp's waterStep,
// once a frame: the lift by how deep it is, the drag, floating flat, the bob).
void beforeStep(float dt)
{
    const float g = sv_gravity.value;
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts; num++)
    {
        Slot& s = world->slots[num];
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

        // Fast (a throw), its continuous collision takes in the other props too: Box3D's is only against the world
        // and kinematic bodies otherwise, and a box thrown at 15 m/s crosses a stacked box in a step.
        const bool fast = glm::length(vel) / world->m2u * dt > 0.2f * b3Body_GetMinExtent(s.body);
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
        // they balance, nothing moves and the velocity stays nought); the drag is taken off the velocity first, over
        // the frame (the old solver's per substep, the same per second).
        const float gs = g * s.gravityScale;
        const float lift = gs * density * part * bob / world->m2u; // m/s^2
        b3Body_ApplyForceToCenter(s.body, b3Vec3{0.f, 0.f, b3Body_GetMass(s.body) * lift}, true);
        const float stiffness = gs * density / (2.f * halfHeight);
        const float restingPart = std::min(1.f, 1.f / density);
        const float drag = 2.4f * std::sqrt(std::max(stiffness, 0.f)) * std::min(1.f, part / restingPart);
        vel.z *= std::exp(-drag * dt);
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
void writeProp(edict_t* ent, Slot& s)
{
    const FieldOffsets& f = fields();
    const bool asleep = !b3Body_IsAwake(s.body);
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
    store(velocity, ent->v.velocity);
    VectorCopy(vec3_origin, ent->v.avelocity);
    setFieldVec(ent, f.vr_spin, spin);
    setFieldFloat(ent, f.vr_rest, asleep ? 1.f : 0.f); // (vr_rigid.cpp's sleep timer: at rest, never negative)
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
                b3Shape_SetRestitution(shapes[i], restitution);
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
    if(world->mesh)
    {
        b3DestroyMesh(world->mesh);
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
    world->id = b3CreateWorld(&def);

    world->mesh = worldMesh(sv.worldmodel, world->m2u);
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
        setFieldFloat(e, fields().vr_rest, 0.f);
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
    setFieldFloat(e, f.vr_rest, 0.f);
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
    Con_Printf("%d, engine %s:\n", static_cast<int>(list.size()), vr_physics_engine.value == 1 && world ? "Box3D" : "Quake VR");
    for(edict_t* e : list)
    {
        const int num = NUM_FOR_EDICT(e);
        const char* body = world && num < static_cast<int>(world->slots.size()) ? kindName(world->slots[num].kind) : "-";
        Con_Printf("  %d %s %.1f %.1f %.1f angles %.0f %.0f %.0f vel %.0f %s (%s)%s\n", num, PR_GetString(e->v.classname), e->v.origin[0],
            e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2], VectorLength(e->v.velocity),
            hasFlag(e, FL_ONGROUND) ? "asleep" : "awake", body, e->v.takedamage ? va(" health %.0f", e->v.health) : "");
    }
}

void registerCommands()
{
    static bool registered = false;
    if(!registered)
    {
        registered = true;
        Cmd_AddCommand("vr_physics_stack", stack_f);
        Cmd_AddCommand("vr_physics_pyramid", pyramid_f);
        Cmd_AddCommand("vr_physics_list", list_f);
        Cmd_AddCommand("vr_physics_loose", loose_f);
        Cmd_AddCommand("vr_physics_pile", pile_f);
        Cmd_AddCommand("vr_physics_hash", hash_f);
    }
}

[[nodiscard]] bool wanted()
{
    return vr_physics_engine.value == 1.f && fields().vr_rigid >= 0 && sv.worldmodel;
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

} // namespace qvr::box3d

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
        destroyWorld(); // vr_physics_engine 0: the entities hold every body's state, the old solver goes on from it
        return;
    }
    const float dt = static_cast<float>(host_frametime);
    if(dt <= 0.f)
    {
        return;
    }
    QVR_PROFILE("box3d");
    if(!world || world->map != sv.worldmodel || world->m2u != units::metresToUnits())
    {
        destroyWorld();
        buildWorld();
    }
    const double t0 = Sys_DoubleTime();
    updateSettings();
    {
        QVR_PROFILE("box3d sync");
        syncEntities(dt);
    }
    {
        QVR_PROFILE("box3d water and hits");
        beforeStep(dt);
    }
    const double t1 = Sys_DoubleTime();

    // Box3D's step, in pieces of at most 1/45 s (a slow server frame).
    std::vector<std::pair<int, int>> impacts;
    const int pieces = std::max(1, static_cast<int>(std::ceil(dt * 45.f - 0.01f)));
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    {
        QVR_PROFILE("box3d step");
        for(int i = 0; i < pieces; i++)
        {
            b3World_Step(world->id, dt / static_cast<float>(pieces), substeps);
            world->steps++;
            touches(impacts);
        }
    }
    const double t2 = Sys_DoubleTime();
    if(vr_debug_box3d.value && (t2 - t0) * 1000.0 > 2.0)
    {
        const b3Counters c = b3World_GetCounters(world->id);
        const b3Profile p = b3World_GetProfile(world->id);
        Con_Printf("box3d: slow frame %.2f ms (sync and water %.2f, step %.2f: collide %.2f, solve %.2f, continuous %.2f), %d "
                   "bodies (%d awake), %d contacts\n",
            (t2 - t0) * 1000.0, (t1 - t0) * 1000.0, (t2 - t1) * 1000.0, p.collide, p.solve, p.bullets, c.bodyCount,
            b3World_GetAwakeBodyCount(world->id), c.contactCount);
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
        if(!ea->free && !eb->free)
        {
            SV_Impact(ea, eb);
        }
    }
}
