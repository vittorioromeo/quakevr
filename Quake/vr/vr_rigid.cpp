// vr_rigid.cpp -- thrown weapons and dropped backpacks as rigid bodies (entities whose QC sets
// .vr_rigid), run by SV_Physics_Toss instead of Quake's toss.
//
// Quake collides a toss entity as an axis-aligned box against the world, and the BSP only has
// hulls for a point, a player and a shambler: a thrown weapon's small box collided as a point
// at its centre, so the gun around it sank into floors and slopes and poked through walls. It
// also spun with Euler-angle rates (tumbling wildly as soon as the spin mixed axes) and was
// stabilized in fixed angle steps (which wiggled).
//
// Here, as in general-purpose physics engines:
// - The body is an oriented box, the drawn model's bounds (with the weapon scaling the client
//   draws it with), of uniform density: its inertia comes from its size.
// - It moves with a linear velocity (.velocity) and a true angular velocity (.vr_spin, radians
//   per second, world axes), integrated as a rotation, in substeps short enough for its size.
// - Collisions: every substep, each corner of the box that is inside a surface (seen from the
//   centre) or about to reach one (swept as a point, which the BSP traces exactly, also against
//   other entities' boxes) becomes a contact; the contacts are solved together with sequential
//   impulses (accumulated and clamped, Coulomb friction in its cone: vr_throw_friction), bounce
//   with vr_throw_restitution on real impacts, and push penetrating corners back out over a few
//   steps (by a velocity used for the step's move only, never kept: split impulses). A body pushed
//   out on opposite sides at once (wedged in a gap smaller than it) is moved the least way out.
//   A contact margin keeps resting corners in contact, and static friction holds slow
//   bodies on floors flatter than their friction angle. So a weapon lands on a corner, tips
//   over and comes to rest on a side, on floors and slopes alike.
// - At rest (in contact, slow, for a while) the body sleeps: Quake's FL_ONGROUND, moved along
//   by lifts; it wakes when what holds it goes away or when QC gives it a velocity.
// - On the way, a box of half-size vr_throw_hitbox finds monsters the thin corners would slip
//   past, so throws that look like hits are hits.
// - With vr_props_collide, props also collide with each other and stack: "Stacking" below (islands
//   of bodies solved together; a body with no neighbour still moves by rigidToss alone).

#include "vr_box3d.hpp"
#include "vr_boxbox.hpp"
#include "vr_carry2h.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstring>

using namespace qvr;
using namespace qvr::progs;

namespace
{

[[nodiscard]] glm::vec3 toGlm(const vec3_t v)
{
    return {v[0], v[1], v[2]};
}

void fromGlm(const glm::vec3& g, vec3_t v)
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

// Whether `ent` is drawn as a brush model (the ammo and health boxes, maps/b_*.bsp).
[[nodiscard]] bool brushModel(edict_t* ent)
{
    const qmodel_t* model = modelOf(ent);
    return model && model->type == mod_brush;
}

// Model axes of an entity's angles as the renderer turns it (brush and alias models differ in
// pitch), and back: see vr_held.hpp.
[[nodiscard]] glm::mat3 axesFromAngles(const vec3_t angles, bool brush)
{
    return held::axesFromAngles(angles, brush);
}

void anglesFromAxes(const glm::mat3& m, vec3_t out, bool brush)
{
    held::anglesFromAxes(m, out, brush);
}

[[nodiscard]] glm::mat3 orthonormalize(const glm::mat3& m)
{
    const glm::vec3 x = glm::normalize(m[0]);
    const glm::vec3 y = glm::normalize(m[1] - x * glm::dot(x, m[1]));
    return glm::mat3{x, y, glm::cross(x, y)};
}

[[nodiscard]] glm::mat3 turned(const glm::mat3& axes, const glm::vec3& spin, float dt)
{
    const float rate = glm::length(spin);
    if(rate < 1e-5f)
    {
        return axes;
    }
    return orthonormalize(glm::mat3_cast(glm::angleAxis(rate * dt, spin / rate)) * axes);
}

// The box the model is drawn in, in its axes, relative to the entity's origin (as vr_render.cpp
// transforms alias models: the networked scale about model_scale_origin, the weapon scaling,
// then the model's own, the post scale and the networked offset on raw vertices).
void localBox(edict_t* ent, glm::vec3& lo, glm::vec3& hi)
{
    qmodel_t* model = modelOf(ent);
    if(!model || model->type != mod_alias)
    {
        lo = toGlm(ent->v.mins);
        hi = toGlm(ent->v.maxs);
        return;
    }

    const FieldOffsets& f = fields();
    held::modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);

    // Never thinner than a unit, so that the corners span a volume.
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{0.5f});
    lo = centre - half;
    hi = centre + half;
}

[[nodiscard]] float gravityOf(edict_t* ent)
{
    const eval_t* val = GetEdictFieldValue(ent, qcvm->extfields.gravity);
    return (val && val->_float ? val->_float : 1.f) * sv_gravity.value;
}

// Monsters and other damageable entities the corners would pass, within the larger hit box.
void touchNearby(edict_t* ent, const glm::vec3& from, const glm::vec3& to)
{
    const float half = vr_throw_hitbox.value;
    if(half <= 0.f)
    {
        return;
    }

    vec3_t start, end, mins{-half, -half, -half}, maxs{half, half, half};
    fromGlm(from, start);
    fromGlm(to, end);
    const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NORMAL, ent);
    edict_t* hit = tr.ent;
    if(!hit || hit == qcvm->edicts || hit->free || hit == PROG_TO_EDICT(ent->v.owner) || hit->v.takedamage == 0.f)
    {
        return;
    }
    SV_Impact(ent, hit);
}

// A corner's trace. Players are left out: their box is Quake's, much wider than a body in VR, and
// a hand reaching for something at the feet has it inside the box. A trace starting in a player's
// box reports only that (the floor under it is lost), so a body there fell through the floor as
// soon as it moved (a hand pressing on it from above pushed it in, and it stayed sunk). What a
// thrown body hits is found by touchNearby.
[[nodiscard]] trace_t pointTrace(const glm::vec3& from, const glm::vec3& to, edict_t* pass)
{
    vec3_t start, end;
    fromGlm(from, start);
    fromGlm(to, end);

    float solids[MAX_SCOREBOARD];
    const int clients = std::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD));
    for(int i = 0; i < clients; i++)
    {
        edict_t* client = EDICT_NUM(i + 1);
        solids[i] = client->v.solid;
        client->v.solid = SOLID_NOT;
    }
    const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, pass);
    for(int i = 0; i < clients; i++)
    {
        EDICT_NUM(i + 1)->v.solid = solids[i];
    }
    return tr;
}

struct Body
{
    edict_t* ent;
    glm::vec3 com;       // centre of mass (the box's centre), world
    glm::vec3 comLocal;  // ... in the model's axes, from the origin
    glm::mat3 rot;
    glm::vec3 vel;       // units per second
    glm::vec3 spin;      // radians per second, world axes
    glm::vec3 half;      // box half-extents
    glm::vec3 invInertia; // body axes, for unit mass

    std::array<glm::vec3, 8> corners() const // offsets from the centre of mass, world
    {
        std::array<glm::vec3, 8> out;
        for(int i = 0; i < 8; i++)
        {
            const glm::vec3 c{(i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z};
            out[i] = rot * c;
        }
        return out;
    }

    [[nodiscard]] glm::vec3 applyInvInertia(const glm::vec3& v) const
    {
        const glm::vec3 local = glm::transpose(rot) * v;
        return rot * (local * invInertia);
    }

    // Impulse `j` at `r` (from the centre of mass).
    void impulse(const glm::vec3& r, const glm::vec3& j)
    {
        vel += j;
        spin += applyInvInertia(glm::cross(r, j));
    }

    [[nodiscard]] float effectiveMass(const glm::vec3& r, const glm::vec3& dir) const
    {
        return 1.f / (1.f + glm::dot(dir, glm::cross(applyInvInertia(glm::cross(r, dir)), r)));
    }
};

// A body going to sleep lies flat and on the floor, not in it: the contacts push corners out over
// a few steps, and a body slowed to rest can stop with an edge still a unit or so inside, a little
// tilted. A side within 8 degrees of the floor is turned flat on it (about its lowest point), then
// the body is lifted until its lowest corner is just on the floor.
void settle(Body& b)
{
    std::array<glm::vec3, 8> corners = b.corners();
    int lowest = 0;
    for(int i = 1; i < 8; i++)
    {
        if(corners[i].z < corners[lowest].z)
        {
            lowest = i;
        }
    }

    // The floor under the lowest corner, from above it.
    const glm::vec3 low = b.com + corners[lowest];
    const trace_t floor = pointTrace(glm::vec3{low.x, low.y, b.com.z}, low - glm::vec3{0.f, 0.f, 4.f}, b.ent);
    if(floor.startsolid || floor.fraction >= 1.f || floor.plane.normal[2] < 0.7f)
    {
        return;
    }
    const glm::vec3 n = toGlm(floor.plane.normal);

    // The body axis nearest the floor's normal (either way), turned onto it.
    int axis = 0;
    float best = 0.f;
    for(int i = 0; i < 3; i++)
    {
        const float d = std::abs(glm::dot(b.rot[i], n));
        if(d > best)
        {
            best = d;
            axis = i;
        }
    }
    const glm::vec3 up = glm::dot(b.rot[axis], n) < 0.f ? -b.rot[axis] : b.rot[axis];
    if(best < 0.9999f && best > std::cos(glm::radians(8.f)))
    {
        const glm::quat q = glm::angleAxis(std::acos(std::min(1.f, glm::dot(up, n))), glm::normalize(glm::cross(up, n)));
        b.rot = orthonormalize(glm::mat3_cast(q) * b.rot);
        b.com = low + q * (b.com - low);
        corners = b.corners();
    }

    // Lift the lowest corner onto the floor (along the normal).
    float depth = 0.f;
    const float floorD = glm::dot(toGlm(floor.endpos), n);
    for(const glm::vec3& r : corners)
    {
        depth = std::max(depth, floorD - glm::dot(b.com + r, n));
    }
    if(depth > 0.f && depth < 4.f)
    {
        b.com += n * (depth + 0.05f);
    }
}

// Whether the lowest corners rest on something (the body may sleep on it, or must wake); `by`: the
// first thing found under it.
[[nodiscard]] bool supported(const Body& b, edict_t** ground, edict_t** by = nullptr)
{
    const std::array<glm::vec3, 8> corners = b.corners();
    float lowest = 1e9f;
    for(const glm::vec3& r : corners)
    {
        lowest = std::min(lowest, r.z);
    }
    for(const glm::vec3& r : corners)
    {
        if(r.z > lowest + 1.f)
        {
            continue;
        }
        const glm::vec3 p = b.com + r;
        const trace_t tr = pointTrace(p, p - glm::vec3{0.f, 0.f, 2.f}, b.ent);
        if(tr.fraction < 1.f || tr.startsolid)
        {
            if(ground)
            {
                *ground = tr.ent ? tr.ent : qcvm->edicts;
            }
            if(by)
            {
                *by = tr.ent;
            }
            return true;
        }
    }
    return false;
}

struct Contact
{
    glm::vec3 r{0.f}; // from the centre of mass
    glm::vec3 n{0.f}; // surface normal, away from it
    glm::vec3 t1{0.f}, t2{0.f};
    float target{0.f}; // wanted normal velocity: > 0 bounces, < 0 lets it approach
    float push{0.f};   // a corner inside: how fast it is moved out (not kept as velocity)
    float massN{0.f}, massT1{0.f}, massT2{0.f};
    float accN{0.f}, accT1{0.f}, accT2{0.f}, accPush{0.f};
    edict_t* ent{nullptr};
};

// Water. Things float (items, backpacks, thrown weapons: 60% under at rest); gibs sink, slowly.
// How much of the body is under water comes from its box's height against the water's surface
// (found under its centre), so the lift grows smoothly as it goes in -- not a point in or out of
// the water, which made floating things jump up and drop back forever. The water's drag damps
// the bob critically (it comes to rest at the surface, bobbing a little), the spin and the drift,
// and a floating body turns to lie flat on a side.
constexpr float floatDensity = 1.f / 0.6f; // relative to water's
constexpr float sinkDensity = 0.5f;

[[nodiscard]] float waterDensity(edict_t* ent)
{
    const int flags = static_cast<int>(ent->v.flags);
    const bool gib = (flags & physics::FL_FORCEGRABBABLE) && !(flags & FL_ITEM);
    return gib ? sinkDensity : floatDensity;
}

[[nodiscard]] bool wet(float x, float y, float z)
{
    vec3_t p{x, y, z};
    const int contents = SV_PointContents(p);
    return contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA;
}

// The part of the body under water (0 to 1), and its half-height as turned.
[[nodiscard]] float submerged(const Body& b, float& halfHeight)
{
    halfHeight = std::abs(b.rot[0].z) * b.half.x + std::abs(b.rot[1].z) * b.half.y + std::abs(b.rot[2].z) * b.half.z;
    const float lo = b.com.z - halfHeight;
    const float hi = b.com.z + halfHeight;
    float under = lo;
    if(!wet(b.com.x, b.com.y, lo))
    {
        if(!wet(b.com.x, b.com.y, b.com.z))
        {
            return 0.f;
        }
        under = b.com.z; // the bottom in a floor
    }
    if(wet(b.com.x, b.com.y, hi))
    {
        return 1.f;
    }
    float above = hi;
    for(int i = 0; i < 12; i++)
    {
        const float mid = (under + above) * 0.5f;
        (wet(b.com.x, b.com.y, mid) ? under : above) = mid;
    }
    return CLAMP(0.f, ((under + above) * 0.5f - lo) / std::max(hi - lo, 0.01f), 1.f);
}

// One substep of the water's lift and drag. The body bobs a little (by entity, a few seconds).
void waterStep(Body& b, float g, float density, float h)
{
    float halfHeight = 0.f;
    const float s = submerged(b, halfHeight);
    if(s <= 0.f)
    {
        return;
    }
    const bool floats = density > 1.f;
    const float bob = floats ? 1.f + 0.04f * std::sin(static_cast<float>(qcvm->time) * 2.1f + static_cast<float>(NUM_FOR_EDICT(b.ent))) : 1.f;
    b.vel.z += g * density * s * bob * h;

    // The float is a spring (lift per unit of depth): its drag is a little over critical once
    // it is in as deep as it floats.
    const float stiffness = g * density / (2.f * std::max(halfHeight, 0.5f));
    const float restingPart = std::min(1.f, 1.f / density);
    const float drag = 2.4f * std::sqrt(stiffness) * std::min(1.f, s / restingPart);
    b.vel.z *= std::exp(-drag * h);
    const float drift = std::exp(-1.5f * s * h);
    b.vel.x *= drift;
    b.vel.y *= drift;
    b.spin *= std::exp(-8.f * s * h);

    // Floating, the side nearest the surface turns up to it.
    if(floats)
    {
        int axis = 0;
        for(int i = 1; i < 3; i++)
        {
            if(std::abs(b.rot[i].z) > std::abs(b.rot[axis].z))
            {
                axis = i;
            }
        }
        const glm::vec3 up = b.rot[axis].z < 0.f ? -b.rot[axis] : b.rot[axis];
        b.spin += glm::cross(up, glm::vec3{0.f, 0.f, 1.f}) * (20.f * s * h);
    }
}

// A body asleep asks every frame whether it still rests on something and how much of it is under
// water: against the world (static: its BSP, its liquids) the answers stay while the body stays where
// it is, the same box. Kept per entity: what they were asked for, and the answers.
struct RestMemo
{
    const qmodel_t* world{nullptr};
    glm::vec3 origin{0.f}, angles{0.f}, lo{0.f}, hi{0.f};
    bool onWorld{false};  // a corner rests on the world (whatever else comes there, it still rests)
    float under{-1.f};    // submerged(), -1: not asked yet
    float halfHeight{0.f};
};
std::vector<RestMemo> restMemos; // by entity number

// A rigid body's water transition check (SV_CheckWaterTransition): where, and what it left.
struct WaterMemo
{
    const qmodel_t* world{nullptr};
    glm::vec3 origin{0.f};
    float watertype{0.f}, waterlevel{0.f};
};
std::vector<WaterMemo> waterMemos; // by entity number

// Bodies wedged: pushed out of surfaces on opposite sides (a box taller than the gap it is in, a corner
// squeezed into a crack), which no push along those normals can resolve. By entity number.
struct Wedge
{
    float time{0.f};     // how long it has been wedged
    bool held{false};    // no way out: held still where it was (at), until moved or pushed or
    double retry{0.0};   // ... it is looked for again
    glm::vec3 at{0.f};
};
std::vector<Wedge> wedges;

[[nodiscard]] Wedge& wedgeOf(edict_t* ent)
{
    const size_t num = static_cast<size_t>(NUM_FOR_EDICT(ent));
    if(num >= wedges.size())
    {
        wedges.resize(num + 64);
    }
    return wedges[num];
}

// Whether the box (shrunk by a unit: the contacts push out of shallow overlaps themselves) is out of
// the world's solid at its centre, corners, edges' middles and faces' middles.
[[nodiscard]] bool boxFree(const Body& b, const glm::vec3& com)
{
    const glm::vec3 half = glm::max(b.half - glm::vec3{1.f}, glm::vec3{0.f});
    for(int x = -1; x <= 1; x++)
    {
        for(int y = -1; y <= 1; y++)
        {
            for(int z = -1; z <= 1; z++)
            {
                const glm::vec3 p = com + b.rot * (half * glm::vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
                vec3_t v{p.x, p.y, p.z};
                if(SV_PointContents(v) == CONTENTS_SOLID)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

// A wedged body is moved the least way out, along the world's axes or its own (within its size): where
// it can move freely again. False if there is none (it is then held still).
[[nodiscard]] bool unwedge(Body& b)
{
    const float reach = 2.f * std::max({b.half.x, b.half.y, b.half.z}) + 2.f;
    const glm::vec3 axes[6] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, b.rot[0], b.rot[1], b.rot[2]};
    for(float distance = 1.f; distance <= reach; distance += 1.f)
    {
        for(const glm::vec3& axis : axes)
        {
            for(const float sign : {1.f, -1.f})
            {
                const glm::vec3 to = b.com + axis * (sign * distance);
                // Along a clear line, not through a wall to its other side.
                const trace_t line = pointTrace(b.com, to, b.ent);
                if(line.fraction >= 1.f && !line.startsolid && boxFree(b, to))
                {
                    b.com = to;
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] WaterMemo& waterMemo(edict_t* ent)
{
    const size_t num = static_cast<size_t>(NUM_FOR_EDICT(ent));
    if(num >= waterMemos.size())
    {
        waterMemos.resize(num + 64);
    }
    return waterMemos[num];
}

[[nodiscard]] RestMemo& restMemo(edict_t* ent, const glm::vec3& lo, const glm::vec3& hi)
{
    const size_t num = static_cast<size_t>(NUM_FOR_EDICT(ent));
    if(num >= restMemos.size())
    {
        restMemos.resize(num + 64);
    }
    RestMemo& m = restMemos[num];
    const glm::vec3 origin = toGlm(ent->v.origin), angles = toGlm(ent->v.angles);
    if(m.world != sv.worldmodel || m.origin != origin || m.angles != angles || m.lo != lo || m.hi != hi)
    {
        m = RestMemo{sv.worldmodel, origin, angles, lo, hi};
    }
    return m;
}

// A corner's contact with the world over a step of h (see rigidToss), or false: inside a surface (from the
// centre, the corner is behind it), or about to reach one within the step (speculative: the solver lets it
// come exactly up to it), looking `reach` further than its own move (a body alone: a quarter unit; one in a
// stack further, as the others may push it that far within the step). Within `hold` of the surface it touches.
[[nodiscard]] bool cornerContact(const Body& b, const glm::vec3& r, float h, float g, float restitution, edict_t* ent, Contact& c,
    float reach = 0.25f, float hold = 0.5f)
{
    const glm::vec3 p = b.com + r;
    c = Contact{};
    c.r = r;

    const trace_t inside = pointTrace(b.com, p, ent);
    if(!inside.startsolid && inside.fraction < 1.f)
    {
        c.n = toGlm(inside.plane.normal);
        const float depth = glm::dot(p - toGlm(inside.endpos), -c.n);
        c.push = std::min(depth, 4.f) * 0.25f / h; // out over a few steps
        c.ent = inside.ent;
    }
    else
    {
        const glm::vec3 move = (b.vel + glm::cross(b.spin, r)) * h;
        const float length = glm::length(move);
        if(length < 1e-4f)
        {
            return false;
        }
        const trace_t ahead = pointTrace(p, p + move + move / length * reach, ent);
        if(ahead.startsolid || ahead.fraction >= 1.f)
        {
            return false;
        }
        c.n = toGlm(ahead.plane.normal);
        if(glm::dot(move, c.n) >= 0.f)
        {
            return false;
        }
        // Within the slop (traces keep an epsilon off surfaces) it already touches: it
        // must not come closer, so it is held (and friction acts) instead of sliding.
        const float gap = std::max(0.f, glm::dot(p - toGlm(ahead.endpos), c.n));
        c.target = gap < hold ? 0.f : -(gap - hold) / h;
        c.ent = ahead.ent;
    }

    // Bouncing: a real impact (faster than gravity alone would give) comes back at
    // vr_throw_restitution of its speed.
    const float vn = glm::dot(b.vel + glm::cross(b.spin, r), c.n);
    if(vn < -g * h * 3.f)
    {
        c.target = std::max(c.target, -restitution * vn);
    }

    c.massN = b.effectiveMass(r, c.n);
    c.t1 = glm::normalize(glm::abs(c.n.z) < 0.9f ? glm::cross(c.n, glm::vec3{0.f, 0.f, 1.f})
                                                  : glm::cross(c.n, glm::vec3{1.f, 0.f, 0.f}));
    c.t2 = glm::cross(c.n, c.t1);
    c.massT1 = b.effectiveMass(r, c.t1);
    c.massT2 = b.effectiveMass(r, c.t2);
    return true;
}

// Wedged for a moment (dt more this frame): out the least way, else held still, not dithering between the two
// pushes.
void resolveWedge(edict_t* ent, Body& b, bool wedged, float dt, bool brush)
{
    Wedge& wedge = wedgeOf(ent);
    wedge.time = wedged || wedge.held ? wedge.time + dt : 0.f;
    if(wedge.time > 0.1f)
    {
        const glm::vec3 was = b.com;
        const bool out = unwedge(b);
        if(vr_debug_throw.value >= 3.f)
        {
            Con_Printf("rigid %d: wedged at %.1f %.1f %.1f, %s\n", NUM_FOR_EDICT(ent), was.x, was.y, was.z,
                out ? "moved out" : "held still");
        }
        if(!out)
        {
            b.com = toGlm(ent->v.origin) + axesFromAngles(ent->v.angles, brush) * b.comLocal;
            b.rot = axesFromAngles(ent->v.angles, brush);
        }
        b.vel = glm::vec3{0.f};
        b.spin = glm::vec3{0.f};
        wedge = out ? Wedge{} : Wedge{wedge.time, true, qcvm->time + 0.5, b.com - b.rot * b.comLocal};
    }
}

// The move of a rigid body (a .vr_rigid toss or bounce entity) over this frame.
void rigidToss(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const float dt = static_cast<float>(host_frametime);
    if(dt <= 0.f)
    {
        return;
    }
    VectorCopy(vec3_origin, ent->v.avelocity);

    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    // Asleep where it was, still: the memo's answers (below) keep it asleep before anything else is worked out.
    if(static_cast<int>(ent->v.flags) & FL_ONGROUND)
    {
        const RestMemo& memo = restMemo(ent, lo, hi);
        const float density = waterDensity(ent);
        if(memo.onWorld && (density <= 1.f || (memo.under >= 0.f && density * memo.under <= 1.02f)) &&
           glm::length(toGlm(ent->v.velocity)) <= 1.f && fieldFloat(ent, f.vr_rest) >= 0.f)
        {
            return;
        }
    }

    // Wedged with no way out: held still (not asleep: nothing holds it up) until something moves it.
    if(Wedge& w = wedgeOf(ent); w.held)
    {
        if(glm::length(toGlm(ent->v.velocity)) > 1.f || toGlm(ent->v.origin) != w.at)
        {
            w = Wedge{};
        }
        else if(qcvm->time < w.retry)
        {
            setFieldVec(ent, f.vr_spin, glm::vec3{0.f});
            return;
        }
    }

    Body b;
    b.ent = ent;
    b.half = (hi - lo) * 0.5f;
    b.comLocal = (lo + hi) * 0.5f;
    const bool brush = brushModel(ent);
    b.rot = axesFromAngles(ent->v.angles, brush);
    b.com = toGlm(ent->v.origin) + b.rot * b.comLocal;
    b.vel = toGlm(ent->v.velocity);
    b.spin = fieldVec(ent, f.vr_spin);
    const glm::vec3 size = b.half * 2.f;
    b.invInertia = 12.f / glm::vec3{size.y * size.y + size.z * size.z, size.x * size.x + size.z * size.z,
                              size.x * size.x + size.y * size.y};

    // Asleep: stays until what holds it goes away, or QC moves it. A body Quake's own toss put to
    // rest (QC sets .vr_rest -1 as it makes it rigid: a gib's first landing) settles here first:
    // Quake left it lying by an unturned box, sunk into the floor or hanging off a ledge.
    if(static_cast<int>(ent->v.flags) & FL_ONGROUND)
    {
        RestMemo& memo = restMemo(ent, lo, hi);
        const bool pushed = glm::length(b.vel) > 1.f;
        const float density = waterDensity(ent);
        if(density > 1.f && memo.under < 0.f)
        {
            memo.under = submerged(b, memo.halfHeight);
        }
        const bool lifted = density > 1.f && density * memo.under > 1.02f; // deeper than it floats
        if(!pushed && !lifted && fieldFloat(ent, f.vr_rest) >= 0.f)
        {
            edict_t* by = nullptr;
            if(memo.onWorld || supported(b, nullptr, &by))
            {
                memo.onWorld = by == qcvm->edicts || memo.onWorld;
                return;
            }
        }
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
        fieldFloat(ent, f.vr_rest) = 0.f;
    }

    SV_CheckVelocity(ent);
    b.vel = toGlm(ent->v.velocity);

    const float g = gravityOf(ent);
    const float restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);
    const float friction = std::max(vr_throw_friction.value, 0.f);
    const float m2u = units::metresToUnits();
    const float minHalf = std::max(0.5f, std::min({b.half.x, b.half.y, b.half.z}));

    // Substeps: each moves at most the box's thinnest half-extent and turns at most 0.25 radians.
    const float travel = glm::length(b.vel) * dt / minHalf + glm::length(b.spin) * dt / 0.25f;
    const int steps = CLAMP(1, static_cast<int>(std::ceil(travel)), 8);
    const float h = dt / static_cast<float>(steps);

    bool contact = false;
    bool wedged = false;
    bool floorContact = false;
    edict_t* floorEnt = nullptr;

    const float density = waterDensity(ent);
    for(int step = 0; step < steps; step++)
    {
        b.vel.z -= g * h;
        waterStep(b, g, density, h);

        b.spin *= std::exp(-std::max(vr_throw_spin_drag.value, 0.f) * h);

        touchNearby(ent, b.com, b.com + b.vel * h);
        if(ent->free)
        {
            return;
        }

        // Contacts, corner by corner (cornerContact).
        std::array<Contact, 8> contacts;
        int count = 0;
        for(const glm::vec3& r : b.corners())
        {
            if(Contact c; cornerContact(b, r, h, g, restitution, ent, c))
            {
                contacts[count++] = c;
            }
        }

        if(vr_debug_throw.value >= 4.f)
        {
            Con_Printf("  c%d step %d com %.1f %.1f %.1f half %.1f %.1f %.1f up %.2f %.2f %.2f\n", NUM_FOR_EDICT(ent), step, b.com.x,
                b.com.y, b.com.z, b.half.x, b.half.y, b.half.z, b.rot[2].x, b.rot[2].y, b.rot[2].z);
            for(int i = 0; i < count; i++)
            {
                const Contact& c = contacts[i];
                Con_Printf("  c%d step %d r %.1f %.1f %.1f n %.2f %.2f %.2f target %.1f push %.1f\n", NUM_FOR_EDICT(ent), step,
                    c.r.x, c.r.y, c.r.z, c.n.x, c.n.y, c.n.z, c.target, c.push);
            }
        }

        // Sequential impulses (accumulated and clamped: pushes only, friction within its cone).
        for(int iteration = 0; iteration < 10 && count > 0; iteration++)
        {
            for(int i = 0; i < count; i++)
            {
                Contact& c = contacts[i];
                const glm::vec3 v = b.vel + glm::cross(b.spin, c.r);

                const float dn = (c.target - glm::dot(v, c.n)) * c.massN;
                const float accN = std::max(c.accN + dn, 0.f);
                b.impulse(c.r, c.n * (accN - c.accN));
                c.accN = accN;

                const float limit = friction * c.accN;
                const glm::vec3 v2 = b.vel + glm::cross(b.spin, c.r);
                const float acc1 = CLAMP(-limit, c.accT1 - glm::dot(v2, c.t1) * c.massT1, limit);
                b.impulse(c.r, c.t1 * (acc1 - c.accT1));
                c.accT1 = acc1;
                const glm::vec3 v3 = b.vel + glm::cross(b.spin, c.r);
                const float acc2 = CLAMP(-limit, c.accT2 - glm::dot(v3, c.t2) * c.massT2, limit);
                b.impulse(c.r, c.t2 * (acc2 - c.accT2));
                c.accT2 = acc2;
            }
        }

        // Corners inside a surface are moved out by a velocity of their own, solved the same way (pushes
        // only) but used for this step's move alone ("split impulses"): pushed out as velocity, a body
        // wedged in a corner or released into a wall kept the push, flew off spinning, struck the
        // other side and was pushed back, over and over.
        glm::vec3 pushVel{0.f}, pushSpin{0.f};
        for(int i = 0; i < count && !wedged; i++)
        {
            for(int j = i + 1; j < count && !wedged; j++)
            {
                wedged = contacts[i].push > 0.f && contacts[j].push > 0.f && glm::dot(contacts[i].n, contacts[j].n) < -0.5f;
            }
        }
        for(int iteration = 0; iteration < 10 && count > 0; iteration++)
        {
            for(int i = 0; i < count; i++)
            {
                Contact& c = contacts[i];
                if(c.push <= 0.f)
                {
                    continue;
                }
                const float vn = glm::dot(pushVel + glm::cross(pushSpin, c.r), c.n);
                const float acc = std::max(c.accPush + (c.push - vn) * c.massN, 0.f);
                const glm::vec3 j = c.n * (acc - c.accPush);
                pushVel += j;
                pushSpin += b.applyInvInertia(glm::cross(c.r, j));
                c.accPush = acc;
            }
        }

        glm::vec3 floorNormal{0.f};
        for(int i = 0; i < count; i++)
        {
            const Contact& c = contacts[i];
            contact = true;
            if(c.n.z > 0.7f)
            {
                floorContact = true;
                floorEnt = c.ent ? c.ent : qcvm->edicts;
                floorNormal += c.n;
            }
            if(c.ent && c.ent != qcvm->edicts && c.accN > 0.f)
            {
                SV_Impact(ent, c.ent);
                if(ent->free)
                {
                    return;
                }
            }
        }

        // Static friction: slow on a floor flatter than the friction angle, it stops sliding.
        if(glm::length(floorNormal) > 0.f)
        {
            const glm::vec3 fn = glm::normalize(floorNormal);
            const glm::vec3 slide = b.vel - fn * glm::dot(b.vel, fn);
            if(glm::length(slide) < 0.8f * m2u && fn.z >= 1.f / std::sqrt(1.f + friction * friction))
            {
                b.vel -= slide;
            }
        }

        if(count > 0)
        {
            // Rolling resistance: slow ones on the ground come to a stop instead of rocking.
            const bool slow = glm::length(b.vel) < 0.5f * m2u;
            b.spin *= std::exp((slow ? -6.f : -1.f) * h);
        }

        b.com += (b.vel + pushVel) * h;
        b.rot = turned(b.rot, b.spin + pushSpin, h);
    }

    resolveWedge(ent, b, wedged, dt, brush);

    // Sleep once in floor contact and slow for a moment.
    float& rest = fieldFloat(ent, f.vr_rest);
    const bool slow = glm::length(b.vel) < 0.15f * m2u && glm::length(b.spin) < 1.f;
    rest = floorContact && slow ? rest + dt : 0.f;
    if(rest > 0.3f && supported(b, &floorEnt))
    {
        b.vel = glm::vec3{0.f};
        b.spin = glm::vec3{0.f};
        settle(b);
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
        edict_t* ground = floorEnt ? floorEnt : qcvm->edicts;
        ent->v.groundentity = EDICT_TO_PROG(ground);
    }

    fromGlm(b.com - b.rot * b.comLocal, ent->v.origin);
    anglesFromAxes(b.rot, ent->v.angles, brush);
    fromGlm(b.vel, ent->v.velocity);
    setFieldVec(ent, f.vr_spin, b.spin);
    SV_LinkEdict(ent, true);

    if(vr_debug_throw.value >= 3.f)
    {
        Con_Printf("rigid %d: origin %.1f %.1f %.1f, %.0f u/s, spin %.1f rad/s, %s\n", NUM_FOR_EDICT(ent), ent->v.origin[0],
            ent->v.origin[1], ent->v.origin[2], glm::length(b.vel), glm::length(b.spin),
            (static_cast<int>(ent->v.flags) & FL_ONGROUND) ? "asleep" : contact ? "in contact" : "flying");
    }
}

// ---------------------------------------------------------------------------------------------------------
// Stacking: props against each other (vr_props_collide 1: boxes and items, 2: gibs and heads too).
//
// Props are touch triggers (SOLID_NOT_BUT_TOUCHABLE: a hand or a player walking over one takes it), which
// traces go through, so each body above collides with the world alone and props passed through each other.
// With vr_props_collide, all of them are stepped together once a frame, at the first one's turn in
// SV_Physics (stack()):
// - Broad phase: each body's world box (grown by its motion over the frame), sorted and swept along x.
// - Islands: bodies near each other (and awake) are solved together; a body alone is moved by rigidToss
//   above, exactly as without this setting.
// - Narrow phase: oriented box against oriented box (vr_boxbox.hpp), up to 4 contacts (a box resting flat on
//   another rests on the 4 corners of their overlap), speculative within a margin of the pair's motion.
//   The world's contacts are rigidToss's, corner by corner.
// - Solver: sequential impulses on both bodies of a contact, equal and opposite, weighed by mass (box volume
//   times a density by kind) and inertia; Coulomb friction; restitution on real impacts; warm started from
//   the last step's impulses (matched by corner for the world, by place on the body for two bodies); an
//   allowed penetration of stackSlop, beyond which contacts push out by split impulses (as rigidToss: a
//   velocity used for the step's move only).
// - Sleep: an island sleeps together once all its bodies have rested for a moment on something (and one of
//   them on the world). Asleep, its bodies are Quake's FL_ONGROUND and cost nothing but a check that none of
//   them moved; a lift carries the whole island (all of them ride on it); anything that moves one of them
//   (QC's velocity, a push, a pickup, the floor going away) wakes them all. An awake body touching a sleeping
//   one wakes its island.
// - Carried props (and those held in both hands) are kinematic: they push the others with the hand's motion
//   and nothing pushes them back (infinite mass).

constexpr float stackSlop = 0.15f;     // allowed penetration between two bodies (units)
constexpr float stackSleepTime = 0.3f; // resting this long (every body of the island) to sleep
// A corner this near the world touches it (a body alone: half a unit). Nearer in an island: its world contacts look
// further ahead (they may be pushed), and a stack would stand on the air of the whole half unit.
constexpr float stackHold = 0.1f;

// QC's .vr_gib (gibs and heads), looked up once a map (-2: not yet).
int gibField = -2;

[[nodiscard]] bool isGib(edict_t* ent)
{
    if(gibField == -2)
    {
        gibField = ED_FindFieldOffset("vr_gib");
    }
    return gibField >= 0 && GetEdictFieldValue(ent, gibField)->_float != 0.f;
}

[[nodiscard]] int collideMode()
{
    return CLAMP(0, static_cast<int>(vr_props_collide.value), 2);
}

[[nodiscard]] bool collidesWithProps(edict_t* ent, int mode)
{
    return mode >= 2 || (mode == 1 && !isGib(ent));
}

// Relative densities: an ammo or health box (a crate) 1; a backpack, mostly cloth, much lighter; armour and
// weapons (steel) heavier. With the box's volume, the mass.
[[nodiscard]] float densityOf(edict_t* ent)
{
    if(brushModel(ent) || isGib(ent))
    {
        return 1.f;
    }
    const qmodel_t* model = modelOf(ent);
    const char* name = model ? model->name : "";
    if(strstr(name, "backpack"))
    {
        return 0.35f;
    }
    if(strstr(name, "armor"))
    {
        return 1.5f;
    }
    return 2.5f;
}

struct StackBody
{
    Body b;
    glm::vec3 lo{0.f}, hi{0.f};
    glm::vec3 boxLo{0.f}, boxHi{0.f}; // the world box for the broad phase
    glm::vec3 pushVel{0.f}, pushSpin{0.f};
    glm::vec3 floorNormal{0.f};      // the world's floors under it (this step)
    edict_t* floorEnt{nullptr};
    float invMass{0.f};
    float g{0.f};
    bool brush{false};
    bool shaped{false}; // shape() done this frame
    bool kinematic{false};
    bool awake{false};
    bool dead{false};
    bool contact{false}, floorContact{false}, supports{false}, kinTouch{false}, wedged{false};
    bool onWorld{false}; // falling asleep: stands on the world
    int island{-1};
    int sleepGroup{0};   // falling asleep: its island asleep

    [[nodiscard]] bool dynamic() const { return !kinematic && !dead; }

    [[nodiscard]] boxbox::Box box() const { return {b.com, b.rot, b.half}; }
};

struct StackContact
{
    int a{-1}, b{-1};     // bodies (b -1: the world); n pushes a (and b the other way)
    glm::vec3 ra{0.f}, rb{0.f};
    glm::vec3 n{0.f}, t1{0.f}, t2{0.f};
    float target{0.f}, push{0.f};
    float massN{0.f}, massT1{0.f}, massT2{0.f};
    float accN{0.f}, accT1{0.f}, accT2{0.f}, accPush{0.f};
    edict_t* ent{nullptr}; // the world's contacts: what was hit
    int feature{-1};        // the world's contacts: the corner
    glm::vec3 local{0.f};   // two bodies: the place on body a (its axes, from its centre)
    // Per row (0 the normal, 1 and 2 the tangents), fixed over the step: r x dir on each body, and the spin a unit
    // impulse along it gives each (its inverse mass and inertia), so an iteration is dot products and adds.
    std::array<glm::vec3, 3> angA{}, angB{}, turnA{}, turnB{};
};

[[nodiscard]] const glm::vec3& rowDir(const StackContact& c, int k)
{
    return k == 0 ? c.n : k == 1 ? c.t1 : c.t2;
}

// Warm starting: each pair's (and each body's with the world) impulses of the last step.
struct CachedPoint
{
    glm::vec3 local{0.f};
    glm::vec3 n{0.f};
    glm::vec3 friction{0.f}; // world
    float accN{0.f};
    int feature{-1};
};
struct CachedManifold
{
    std::array<CachedPoint, 8> pts{};
    int count{0};
    float h{0.f};
    uint64_t stamp{0};
};

// A sleeping island: its bodies (entity numbers) where they fell asleep.
struct SleepGroup
{
    struct Member
    {
        int num;
        glm::vec3 origin, angles;
        bool onWorld; // rests on the world (or a solid entity), not only on other bodies
    };
    std::vector<Member> members;
};

// Where a body in contact has stayed (within stillReach and stillTurn), and for how long: one held in place
// by its contacts that never slows down (a limit cycle: a box lying across a ledge the world's corner contacts
// don't see, rocking on it) rests all the same, so its island can sleep.
struct StillPose
{
    glm::vec3 com{0.f}, x{0.f}, z{0.f};
    float time{0.f};
};
constexpr float stillReach = 0.5f;        // units
constexpr float stillTurn = 0.99863f;     // cos 3 degrees
constexpr float stillTime = 1.f;          // seconds

// Carried props: their last pose, for their velocity.
struct KinematicPose
{
    glm::vec3 com{0.f};
    glm::mat3 rot{1.f};
    uint64_t frame{0};
};

struct StackState
{
    double time{-1.0};
    const qmodel_t* world{nullptr};
    uint64_t frame{0};
    uint64_t stamp{0};
    int carryField{-2}; // QC's .carry_player (-2: not looked up yet)
    std::vector<StackBody> bodies;
    std::vector<int> index;   // by entity number: in bodies, or -1
    std::vector<uint64_t> handled; // by entity number: the frame stack() stepped it in
    std::vector<int> groupOf; // by entity number: its sleep group, or 0
    std::unordered_map<int, SleepGroup> groups;
    int nextGroup{1};
    std::unordered_map<uint64_t, CachedManifold> manifolds;
    std::unordered_map<int, KinematicPose> kinematics;
    std::vector<StillPose> still; // by entity number
    std::vector<std::pair<int, int>> overlaps; // broad phase pairs (body indices)
    std::vector<std::pair<int, int>> pairs;    // ... that take part (at least one awake)
    std::vector<StackContact> contacts;
    std::vector<std::pair<int, int>> touching; // pairs in contact in the step (within half a unit)
    std::vector<int> parent; // islands (union-find)
};
StackState stackState;

[[nodiscard]] uint64_t pairKey(edict_t* a, edict_t* b)
{
    return (static_cast<uint64_t>(NUM_FOR_EDICT(a)) << 32) | static_cast<uint64_t>(b ? NUM_FOR_EDICT(b) : 0);
}

[[nodiscard]] int findRoot(std::vector<int>& parent, int i)
{
    while(parent[i] != i)
    {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

// A body's box, turn, place, inertia and mass, from its entity: worked out once a frame, only when needed (all asleep,
// a frame costs little more than finding them).
void shape(StackBody& body)
{
    if(body.shaped)
    {
        return;
    }
    body.shaped = true;
    Body& b = body.b;
    edict_t* ent = b.ent;
    localBox(ent, body.lo, body.hi);
    b.half = (body.hi - body.lo) * 0.5f;
    b.comLocal = (body.lo + body.hi) * 0.5f;
    body.brush = brushModel(ent);
    b.rot = axesFromAngles(ent->v.angles, body.brush);
    b.com = toGlm(ent->v.origin) + b.rot * b.comLocal;
    const glm::vec3 size = b.half * 2.f;
    b.invInertia = 12.f / glm::vec3{size.y * size.y + size.z * size.z, size.x * size.x + size.z * size.z, size.x * size.x + size.y * size.y};
    body.invMass = body.kinematic ? 0.f : 1.f / std::max(densityOf(ent) * size.x * size.y * size.z / 4096.f, 1e-3f);
}

void wakeBody(edict_t* ent)
{
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    fieldFloat(ent, fields().vr_rest) = 0.f;
}

void wakeGroup(int id)
{
    StackState& s = stackState;
    const auto it = s.groups.find(id);
    if(it == s.groups.end())
    {
        return;
    }
    for(const SleepGroup::Member& m : it->second.members)
    {
        if(m.num < static_cast<int>(s.groupOf.size()) && s.groupOf[m.num] == id)
        {
            s.groupOf[m.num] = 0;
            edict_t* ent = EDICT_NUM(m.num);
            if(!ent->free)
            {
                wakeBody(ent);
                const int i = m.num < static_cast<int>(s.index.size()) ? s.index[m.num] : -1;
                if(i >= 0)
                {
                    s.bodies[i].awake = true;
                }
            }
        }
    }
    if(vr_debug_throw.value >= 3.f)
    {
        Con_Printf("stack: island %d wakes (%d bodies)\n", id, static_cast<int>(it->second.members.size()));
    }
    s.groups.erase(it);
}

// Wakes a body, and its island if it sleeps in one.
void wake(StackBody& body)
{
    StackState& s = stackState;
    const int num = NUM_FOR_EDICT(body.b.ent);
    if(num < static_cast<int>(s.groupOf.size()) && s.groupOf[num] != 0)
    {
        wakeGroup(s.groupOf[num]);
    }
    wakeBody(body.b.ent);
    body.awake = true;
}

// Whether a sleeping body still rests on the world as it did (rigidToss's check asleep: the memo, water that
// rose, the floor gone).
[[nodiscard]] bool restsOnWorld(StackBody& body)
{
    shape(body);
    edict_t* ent = body.b.ent;
    RestMemo& memo = restMemo(ent, body.lo, body.hi);
    const float density = waterDensity(ent);
    if(density > 1.f && memo.under < 0.f)
    {
        memo.under = submerged(body.b, memo.halfHeight);
    }
    if(density > 1.f && density * memo.under > 1.02f)
    {
        return false;
    }
    edict_t* by = nullptr;
    if(memo.onWorld || supported(body.b, nullptr, &by))
    {
        memo.onWorld = by == qcvm->edicts || memo.onWorld;
        return true;
    }
    return false;
}

// The sleeping islands: still where they fell asleep, or all moved alike (a lift carrying them: they stay
// asleep), or woken.
void checkGroups()
{
    StackState& s = stackState;
    std::vector<int> broken;
    for(auto& [id, group] : s.groups)
    {
        bool wakeIt = false, moved = false, lost = false, same = true;
        glm::vec3 delta{0.f};
        for(size_t k = 0; k < group.members.size() && !wakeIt; k++)
        {
            const SleepGroup::Member& m = group.members[k];
            const int i = m.num < static_cast<int>(s.index.size()) ? s.index[m.num] : -1;
            edict_t* ent = EDICT_NUM(m.num);
            if(i < 0 || s.bodies[i].kinematic || s.groupOf[m.num] != id)
            {
                wakeIt = true; // taken, carried, removed
                break;
            }
            if(glm::length(toGlm(ent->v.velocity)) > 1.f || fieldFloat(ent, fields().vr_rest) < 0.f)
            {
                wakeIt = true; // QC moved it
                break;
            }
            lost = lost || !(static_cast<int>(ent->v.flags) & FL_ONGROUND);
            const glm::vec3 d = toGlm(ent->v.origin) - m.origin;
            if(toGlm(ent->v.angles) != m.angles)
            {
                wakeIt = true;
                break;
            }
            if(k == 0)
            {
                delta = d;
            }
            same = same && glm::length(d - delta) < 0.01f;
            moved = moved || d != glm::vec3{0.f};
        }

        if(!wakeIt && (moved || lost))
        {
            // Carried along by a pusher (Quake's SV_PushMove moves every body whose ground it is, and takes
            // their FL_ONGROUND): all the same way, it stays asleep there. Else (a push of one, QC waking one
            // without moving it) the island wakes.
            wakeIt = !same || delta == glm::vec3{0.f};
            if(!wakeIt)
            {
                for(SleepGroup::Member& m : group.members)
                {
                    edict_t* ent = EDICT_NUM(m.num);
                    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
                    m.origin = toGlm(ent->v.origin);
                    s.bodies[s.index[m.num]].awake = false; // (gathered awake: the push took its FL_ONGROUND)
                }
            }
        }
        for(size_t k = 0; k < group.members.size() && !wakeIt; k++)
        {
            if(group.members[k].onWorld && !restsOnWorld(s.bodies[s.index[group.members[k].num]]))
            {
                wakeIt = true; // its floor went away (a lift, a door), or water came up
            }
        }
        if(wakeIt)
        {
            broken.push_back(id);
        }
    }
    for(const int id : broken)
    {
        wakeGroup(id);
    }
}

// The world box of a body, grown by its motion over the frame.
void boundBody(StackBody& body, float dt)
{
    const Body& b = body.b;
    glm::vec3 extent{0.f};
    for(int i = 0; i < 3; i++)
    {
        extent += glm::abs(b.rot[i]) * b.half[i];
    }
    const bool moving = body.awake || body.kinematic;
    const float grow = 1.f + (moving ? (glm::length(b.vel) + glm::length(b.spin) * glm::length(b.half)) * dt : 0.f);
    body.boxLo = b.com - extent - glm::vec3{grow};
    body.boxHi = b.com + extent + glm::vec3{grow};
}

// The rows' fixed parts and effective masses (0 if nothing can move along the normal).
void prepare(StackContact& c)
{
    StackState& s = stackState;
    const StackBody& a = s.bodies[c.a];
    const StackBody* b = c.b >= 0 ? &s.bodies[c.b] : nullptr;
    float* masses[3] = {&c.massN, &c.massT1, &c.massT2};
    for(int k = 0; k < 3; k++)
    {
        const glm::vec3& dir = rowDir(c, k);
        c.angA[k] = glm::cross(c.ra, dir);
        c.turnA[k] = a.b.applyInvInertia(c.angA[k]) * a.invMass;
        float inverse = a.invMass + glm::dot(c.angA[k], c.turnA[k]);
        if(b)
        {
            c.angB[k] = glm::cross(c.rb, dir);
            c.turnB[k] = b->b.applyInvInertia(c.angB[k]) * b->invMass;
            inverse += b->invMass + glm::dot(c.angB[k], c.turnB[k]);
        }
        *masses[k] = inverse > 0.f ? 1.f / inverse : 0.f;
    }
}

// Velocity of a relative to b along row k (split: the push's velocities).
[[nodiscard]] float rowVelocity(const StackContact& c, int k, bool split)
{
    StackState& s = stackState;
    const StackBody& a = s.bodies[c.a];
    const glm::vec3& dir = rowDir(c, k);
    float v = glm::dot(split ? a.pushVel : a.b.vel, dir) + glm::dot(split ? a.pushSpin : a.b.spin, c.angA[k]);
    if(c.b >= 0)
    {
        const StackBody& b = s.bodies[c.b];
        v -= glm::dot(split ? b.pushVel : b.b.vel, dir) + glm::dot(split ? b.pushSpin : b.b.spin, c.angB[k]);
    }
    return v;
}

// Impulse `amount` along row k: on a, and the opposite on b.
void rowImpulse(const StackContact& c, int k, float amount, bool split)
{
    StackState& s = stackState;
    StackBody& a = s.bodies[c.a];
    const glm::vec3& dir = rowDir(c, k);
    (split ? a.pushVel : a.b.vel) += dir * (amount * a.invMass);
    (split ? a.pushSpin : a.b.spin) += c.turnA[k] * amount;
    if(c.b >= 0)
    {
        StackBody& b = s.bodies[c.b];
        (split ? b.pushVel : b.b.vel) -= dir * (amount * b.invMass);
        (split ? b.pushSpin : b.b.spin) -= c.turnB[k] * amount;
    }
}

// The contacts between bodies i and j (i's box A, j's B) for a step of h.
void pairContacts(int i, int j, float h, float restitution)
{
    StackState& s = stackState;
    const StackBody& A = s.bodies[i];
    const StackBody& B = s.bodies[j];
    const float reach = (glm::length(A.b.vel - B.b.vel) + glm::length(A.b.spin) * glm::length(A.b.half) +
                         glm::length(B.b.spin) * glm::length(B.b.half)) * h;
    boxbox::Manifold m;
    // Every corner of the overlap (up to 8): a box turned on another rests on all of them, not on 4 of them, which
    // left sides unsupported that it sank to, within the slop (a stack of turned boxes leaned a degree a box).
    if(!boxbox::collide(A.box(), B.box(), 0.5f + std::min(reach, 64.f), m, 8))
    {
        return;
    }

    for(int k = 0; k < m.count; k++)
    {
        if(m.pts[k].depth >= -0.5f)
        {
            s.touching.emplace_back(i, j);
            break;
        }
    }

    // B is pushed along n: it is contact's body a.
    for(int k = 0; k < m.count; k++)
    {
        const boxbox::Point& p = m.pts[k];
        StackContact c;
        c.a = j;
        c.b = i;
        c.n = m.n;
        c.ra = p.p - B.b.com;
        c.rb = p.p - A.b.com;
        c.local = glm::transpose(B.b.rot) * c.ra;
        const glm::vec3 v = (B.b.vel + glm::cross(B.b.spin, c.ra)) - (A.b.vel + glm::cross(A.b.spin, c.rb));
        const float vn = glm::dot(v, c.n);
        // In contact: it must not come closer; apart: it may close the gap within the step.
        c.target = p.depth >= 0.f ? 0.f : p.depth / h;
        // A real impact that reaches it within the step bounces.
        if(vn < -std::max(A.g, B.g) * h * 3.f && (p.depth >= 0.f || vn * h <= p.depth))
        {
            c.target = std::max(c.target, -restitution * vn);
        }
        c.push = p.depth > stackSlop ? std::min(p.depth - stackSlop, 4.f) * 0.25f / h : 0.f;
        c.t1 = glm::normalize(glm::abs(c.n.z) < 0.9f ? glm::cross(c.n, glm::vec3{0.f, 0.f, 1.f})
                                                      : glm::cross(c.n, glm::vec3{1.f, 0.f, 0.f}));
        c.t2 = glm::cross(c.n, c.t1);
        prepare(c);
        if(c.massN > 0.f)
        {
            s.contacts.push_back(c);
        }
    }
}

// The last step's impulses for this step's contacts.
void warmStart(float h)
{
    StackState& s = stackState;
    for(StackContact& c : s.contacts)
    {
        StackBody& a = s.bodies[c.a];
        edict_t* other = c.b >= 0 ? s.bodies[c.b].b.ent : nullptr;
        const auto it = s.manifolds.find(pairKey(a.b.ent, other));
        if(it == s.manifolds.end() || it->second.h <= 0.f)
        {
            continue;
        }
        const CachedManifold& cached = it->second;
        const float tolerance = std::max(1.f, 0.15f * std::min({a.b.half.x, a.b.half.y, a.b.half.z}));
        const CachedPoint* match = nullptr;
        float best = tolerance * tolerance;
        for(int k = 0; k < cached.count; k++)
        {
            const CachedPoint& p = cached.pts[k];
            if(glm::dot(p.n, c.n) < 0.9f)
            {
                continue;
            }
            if(c.feature >= 0)
            {
                if(p.feature == c.feature)
                {
                    match = &p;
                    break;
                }
                continue;
            }
            const glm::vec3 d = p.local - c.local;
            if(glm::dot(d, d) < best)
            {
                best = glm::dot(d, d);
                match = &p;
            }
        }
        if(!match)
        {
            continue;
        }
        const float scale = h / cached.h;
        c.accN = match->accN * scale;
        const float limit = std::max(vr_throw_friction.value, 0.f) * c.accN;
        c.accT1 = CLAMP(-limit, glm::dot(match->friction, c.t1) * scale, limit);
        c.accT2 = CLAMP(-limit, glm::dot(match->friction, c.t2) * scale, limit);
        rowImpulse(c, 0, c.accN, false);
        rowImpulse(c, 1, c.accT1, false);
        rowImpulse(c, 2, c.accT2, false);
    }
}

void storeImpulses(float h)
{
    StackState& s = stackState;
    s.stamp++;
    for(const StackContact& c : s.contacts)
    {
        const StackBody& a = s.bodies[c.a];
        edict_t* other = c.b >= 0 ? s.bodies[c.b].b.ent : nullptr;
        CachedManifold& m = s.manifolds[pairKey(a.b.ent, other)];
        if(m.stamp != s.stamp)
        {
            m.stamp = s.stamp;
            m.count = 0;
            m.h = h;
        }
        if(m.count < static_cast<int>(m.pts.size()))
        {
            m.pts[m.count++] = CachedPoint{c.local, c.n, c.t1 * c.accT1 + c.t2 * c.accT2, c.accN, c.feature};
        }
    }
}

// Steps an island (bodies `members`, the pairs among them and with carried props) over the frame, together.
void solveIsland(const std::vector<int>& members, const std::vector<std::pair<int, int>>& pairs, float dt)
{
    StackState& s = stackState;
    const FieldOffsets& f = fields();
    const float restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);
    const float friction = std::max(vr_throw_friction.value, 0.f);
    const float m2u = units::metresToUnits();

    int steps = 1;
    for(const int i : members)
    {
        StackBody& body = s.bodies[i];
        edict_t* ent = body.b.ent;
        VectorCopy(vec3_origin, ent->v.avelocity);
        if(Wedge& w = wedgeOf(ent); w.held)
        {
            w = Wedge{}; // held still alone; the others may move it
        }
        SV_CheckVelocity(ent);
        body.b.vel = toGlm(ent->v.velocity);
        body.g = gravityOf(ent);
        body.floorEnt = nullptr;
        body.contact = body.floorContact = body.supports = body.kinTouch = body.wedged = false;
        const float minHalf = std::max(0.5f, std::min({body.b.half.x, body.b.half.y, body.b.half.z}));
        const float travel = glm::length(body.b.vel) * dt / minHalf + glm::length(body.b.spin) * dt / 0.25f;
        steps = std::max(steps, CLAMP(1, static_cast<int>(std::ceil(travel)), 8));
    }
    // A big pile collapsing (tens of bodies, one of them flung fast) would take every body through the fast one's
    // substeps: at most 64 body-steps (2 substeps from 32 bodies up). The contacts are speculative over the step (the
    // pairs' margin covers their motion, the world's traces follow each corner's move), so nothing passes through;
    // only a fast spin is turned in coarser steps.
    steps = std::min(steps, std::max(2, 64 / static_cast<int>(members.size())));
    const float h = dt / static_cast<float>(steps);
    // Warm started: more for a taller stack (what it carries comes down through each body), up to 24 for 8 bodies;
    // a bigger pile (rarely a stack) gets 16, where more cost much and show little.
    const int n = static_cast<int>(members.size());
    const int iterations = n <= 8 ? 8 + 2 * n : 16;

    for(int step = 0; step < steps; step++)
    {
        for(const int i : members)
        {
            StackBody& body = s.bodies[i];
            if(body.dead)
            {
                continue;
            }
            Body& b = body.b;
            b.vel.z -= body.g * h;
            waterStep(b, body.g, waterDensity(b.ent), h);
            b.spin *= std::exp(-std::max(vr_throw_spin_drag.value, 0.f) * h);
            touchNearby(b.ent, b.com, b.com + b.vel * h);
            body.dead = b.ent->free;
            body.pushVel = body.pushSpin = body.floorNormal = glm::vec3{0.f};
        }

        // Contacts: the world's, corner by corner (as rigidToss), and the pairs'. The world's look as far ahead as
        // the fastest of the island (or a hand's prop) could push a body within the step (a box landing on a stack
        // pushes the ones under it towards the floor at once).
        float fastest = 0.f;
        for(const auto& [i, j] : pairs)
        {
            for(const int k : {i, j})
            {
                const Body& b = s.bodies[k].b;
                fastest = std::max(fastest, glm::length(b.vel) + glm::length(b.spin) * glm::length(b.half));
            }
        }
        const float reach = 0.25f + std::min(fastest * h, 8.f);
        s.contacts.clear();
        s.touching.clear();
        {
            QVR_PROFILE("props world contacts");
            for(const int i : members)
            {
                StackBody& body = s.bodies[i];
                if(body.dead)
                {
                    continue;
                }
                const size_t first = s.contacts.size();
                const std::array<glm::vec3, 8> corners = body.b.corners();
                for(int k = 0; k < 8; k++)
                {
                    Contact c;
                    if(!cornerContact(body.b, corners[k], h, body.g, restitution, body.b.ent, c, reach, stackHold))
                    {
                        continue;
                    }
                    StackContact sc;
                    sc.a = i;
                    sc.ra = c.r;
                    sc.n = c.n;
                    sc.t1 = c.t1;
                    sc.t2 = c.t2;
                    sc.target = c.target;
                    sc.push = c.push;
                    sc.ent = c.ent;
                    sc.feature = k;
                    prepare(sc);
                    s.contacts.push_back(sc);
                }
                // Pushed out of the world on opposite sides at once: wedged (as rigidToss).
                for(size_t x = first; x < s.contacts.size() && !body.wedged; x++)
                {
                    for(size_t y = x + 1; y < s.contacts.size() && !body.wedged; y++)
                    {
                        const StackContact& c1 = s.contacts[x];
                        const StackContact& c2 = s.contacts[y];
                        body.wedged = c1.push > 0.f && c2.push > 0.f && glm::dot(c1.n, c2.n) < -0.5f;
                    }
                }
            }
        }
        {
            QVR_PROFILE("props pair contacts");
            for(const auto& [i, j] : pairs)
            {
                if(!s.bodies[i].dead && !s.bodies[j].dead)
                {
                    pairContacts(i, j, h, restitution);
                }
            }
        }
        QVR_PROFILE("props solve");
        warmStart(h);

        // Sequential impulses, on both bodies of a pair; until no contact's velocity changes by more than
        // settled (a resting island, warm started, is there in a few).
        constexpr float settled = 0.02f; // units per second
        const size_t contactCount = s.contacts.size();
        for(int iteration = 0; iteration < iterations; iteration++)
        {
            float largest = 0.f;
            // Forward, then backward (symmetric Gauss-Seidel): a contact solved first in every sweep takes more than
            // its share, and a stack leans towards it.
            for(size_t x = 0; x < contactCount; x++)
            {
                StackContact& c = s.contacts[iteration % 2 ? contactCount - 1 - x : x];
                const float accN = std::max(c.accN + (c.target - rowVelocity(c, 0, false)) * c.massN, 0.f);
                rowImpulse(c, 0, accN - c.accN, false);
                largest = std::max(largest, std::abs(accN - c.accN) / c.massN);
                c.accN = accN;

                const float limit = friction * c.accN;
                if(limit <= 0.f && c.accT1 == 0.f && c.accT2 == 0.f)
                {
                    continue; // not pressing: no friction
                }
                const float acc1 = CLAMP(-limit, c.accT1 - rowVelocity(c, 1, false) * c.massT1, limit);
                rowImpulse(c, 1, acc1 - c.accT1, false);
                const float acc2 = CLAMP(-limit, c.accT2 - rowVelocity(c, 2, false) * c.massT2, limit);
                rowImpulse(c, 2, acc2 - c.accT2, false);
                largest = std::max({largest, std::abs(acc1 - c.accT1) / c.massT1, std::abs(acc2 - c.accT2) / c.massT2});
                c.accT1 = acc1;
                c.accT2 = acc2;
            }
            if(largest < settled)
            {
                break;
            }
        }

        // Out of the world and of each other by split impulses (the step's move only).
        for(int iteration = 0; iteration < iterations; iteration++)
        {
            float largest = 0.f;
            for(size_t x = 0; x < contactCount; x++)
            {
                StackContact& c = s.contacts[iteration % 2 ? contactCount - 1 - x : x];
                if(c.push <= 0.f)
                {
                    continue;
                }
                const float acc = std::max(c.accPush + (c.push - rowVelocity(c, 0, true)) * c.massN, 0.f);
                rowImpulse(c, 0, acc - c.accPush, true);
                largest = std::max(largest, std::abs(acc - c.accPush) / c.massN);
                c.accPush = acc;
            }
            if(largest < settled)
            {
                break;
            }
        }
        storeImpulses(h);
        if(vr_debug_throw.value >= 4.f)
        {
            for(const StackContact& c : s.contacts)
            {
                Con_Printf("  s%d-%d step %d r %.1f %.1f %.1f n %.2f %.2f %.2f target %.1f push %.1f impulse %.2f\n",
                    NUM_FOR_EDICT(s.bodies[c.a].b.ent), c.b >= 0 ? NUM_FOR_EDICT(s.bodies[c.b].b.ent) : 0, step, c.ra.x, c.ra.y,
                    c.ra.z, c.n.x, c.n.y, c.n.z, c.target, c.push, c.accN);
            }
        }

        // What each body touched, what it stands on.
        for(const StackContact& c : s.contacts)
        {
            StackBody& a = s.bodies[c.a];
            StackBody* b = c.b >= 0 ? &s.bodies[c.b] : nullptr;
            a.contact = true;
            if(!b)
            {
                if(c.n.z > 0.7f)
                {
                    a.floorContact = true;
                    a.floorEnt = c.ent ? c.ent : qcvm->edicts;
                    a.floorNormal += c.n;
                }
                if(c.ent && c.ent != qcvm->edicts && c.accN > 0.f && !a.dead)
                {
                    SV_Impact(a.b.ent, c.ent);
                    a.dead = a.b.ent->free;
                }
                continue;
            }
            b->contact = true;
            a.kinTouch = a.kinTouch || b->kinematic;
            b->kinTouch = b->kinTouch || a.kinematic;
            if(c.n.z > 0.7f)
            {
                a.floorContact = true; // a rests on b
                b->supports = true;
            }
            else if(c.n.z < -0.7f)
            {
                b->floorContact = true;
                a.supports = true;
            }
        }

        for(const int i : members)
        {
            StackBody& body = s.bodies[i];
            if(body.dead)
            {
                continue;
            }
            Body& b = body.b;
            // Static friction on the world's floors, as rigidToss.
            if(glm::length(body.floorNormal) > 0.f)
            {
                const glm::vec3 fn = glm::normalize(body.floorNormal);
                const glm::vec3 slide = b.vel - fn * glm::dot(b.vel, fn);
                if(glm::length(slide) < 0.8f * m2u && fn.z >= 1.f / std::sqrt(1.f + friction * friction))
                {
                    b.vel -= slide;
                }
            }
            if(body.contact)
            {
                const bool slow = glm::length(b.vel) < 0.5f * m2u;
                b.spin *= std::exp((slow ? -6.f : -1.f) * h);
            }
            b.com += (b.vel + body.pushVel) * h;
            b.rot = turned(b.rot, b.spin + body.pushSpin, h);
        }
    }

    // Wedged, rest.
    for(const int i : members)
    {
        StackBody& body = s.bodies[i];
        if(body.dead)
        {
            continue;
        }
        resolveWedge(body.b.ent, body.b, body.wedged, dt, body.brush);
        float& rest = fieldFloat(body.b.ent, f.vr_rest);
        const bool slow = glm::length(body.b.vel) < 0.15f * m2u && glm::length(body.b.spin) < 1.f;
        const size_t num = static_cast<size_t>(NUM_FOR_EDICT(body.b.ent));
        if(num >= s.still.size())
        {
            s.still.resize(num + 64);
        }
        StillPose& still = s.still[num];
        const Body& b = body.b;
        if(body.contact && !body.kinTouch && glm::length(b.com - still.com) < stillReach && glm::dot(b.rot[0], still.x) > stillTurn &&
           glm::dot(b.rot[2], still.z) > stillTurn)
        {
            still.time += dt;
        }
        else
        {
            still = StillPose{b.com, b.rot[0], b.rot[2], 0.f};
        }
        // At rest on something, whatever its slope (a body leaning on others rests on steep contacts), for a moment;
        // or held in one place by its contacts for longer.
        rest = body.contact && (slow || still.time > stillTime) && !body.kinTouch ? rest + dt : 0.f;
        if(vr_debug_throw.value >= 4.f)
        {
            Con_Printf("  s%d floor %d supports %d hand %d rest %.2f vel %.2f spin %.2f\n", NUM_FOR_EDICT(body.b.ent), body.floorContact,
                body.supports, body.kinTouch, rest, glm::length(body.b.vel), glm::length(body.b.spin));
        }
    }

    // Sleep, by the bodies touching each other (not all those near each other: a body that can't rest, one lying
    // across a ledge, keeps only what touches it awake). A part sleeps if all its bodies rest and one of them
    // stands on the world.
    for(const int i : members)
    {
        s.parent[i] = i;
        s.bodies[i].sleepGroup = 0;
        s.bodies[i].onWorld = false;
    }
    for(const auto& [i, j] : s.touching)
    {
        if(!s.bodies[i].kinematic && !s.bodies[j].kinematic)
        {
            s.parent[findRoot(s.parent, i)] = findRoot(s.parent, j);
        }
    }
    std::vector<int> part;
    for(const int root : members)
    {
        if(findRoot(s.parent, root) != root)
        {
            continue;
        }
        part.clear();
        for(const int i : members)
        {
            if(findRoot(s.parent, i) == root && !s.bodies[i].dead)
            {
                part.push_back(i);
            }
        }
        bool sleep = !part.empty();
        for(const int i : part)
        {
            sleep = sleep && fieldFloat(s.bodies[i].b.ent, f.vr_rest) > stackSleepTime;
        }
        if(!sleep)
        {
            continue;
        }
        edict_t* ground = nullptr;
        bool oneGround = true;
        int base = -1, bases = 0;
        for(const int i : part)
        {
            StackBody& body = s.bodies[i];
            edict_t* under = nullptr;
            if(supported(body.b, &under))
            {
                body.onWorld = true;
                oneGround = oneGround && (!ground || ground == under);
                ground = under;
                base = i;
                bases++;
            }
        }
        if(vr_debug_throw.value >= 4.f)
        {
            Con_Printf("  part of %d: resting, %s\n", static_cast<int>(part.size()), bases ? "on the world: sleeps" : "not on the world");
        }
        if(!bases)
        {
            continue;
        }

        // Resting on the world by one body (a stack): that body is settled as one alone would be (flat on the floor,
        // not in it), and the others move with it, rigidly (they keep their places on it). Else each body on the
        // world that carries none is.
        if(bases == 1)
        {
            Body& b = s.bodies[base].b;
            const glm::vec3 com = b.com;
            const glm::mat3 rot = b.rot;
            settle(b);
            const glm::mat3 turn = b.rot * glm::transpose(rot);
            for(const int i : part)
            {
                if(i != base)
                {
                    Body& other = s.bodies[i].b;
                    other.com = b.com + turn * (other.com - com);
                    other.rot = orthonormalize(turn * other.rot);
                }
            }
        }
        else
        {
            for(const int i : part)
            {
                if(s.bodies[i].onWorld && !s.bodies[i].supports)
                {
                    settle(s.bodies[i].b);
                }
            }
        }

        // All of them on one pusher (a lift): they all ride it (SV_PushMove moves what stands on it).
        edict_t* groundAll = oneGround && ground && static_cast<int>(ground->v.movetype) == MOVETYPE_PUSH ? ground : qcvm->edicts;
        const int group = s.nextGroup++;
        s.groups[group] = SleepGroup{};
        for(const int i : part)
        {
            StackBody& body = s.bodies[i];
            edict_t* ent = body.b.ent;
            body.sleepGroup = group;
            body.b.vel = glm::vec3{0.f};
            body.b.spin = glm::vec3{0.f};
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            ent->v.groundentity = EDICT_TO_PROG(groundAll);
        }
        if(vr_debug_throw.value >= 3.f)
        {
            Con_Printf("stack: island %d sleeps (%d bodies)\n", group, static_cast<int>(part.size()));
        }
    }

    for(const int i : members)
    {
        StackBody& body = s.bodies[i];
        edict_t* ent = body.b.ent;
        if(body.dead || ent->free)
        {
            continue;
        }
        Body& b = body.b;
        fromGlm(b.com - b.rot * b.comLocal, ent->v.origin);
        anglesFromAxes(b.rot, ent->v.angles, body.brush);
        fromGlm(b.vel, ent->v.velocity);
        setFieldVec(ent, f.vr_spin, b.spin);
        SV_LinkEdict(ent, true);
        if(body.sleepGroup)
        {
            const int num = NUM_FOR_EDICT(ent);
            s.groupOf[num] = body.sleepGroup;
            s.groups[body.sleepGroup].members.push_back(SleepGroup::Member{num, toGlm(ent->v.origin), toGlm(ent->v.angles), body.onWorld});
        }
        if(vr_debug_throw.value >= 3.f)
        {
            Con_Printf("rigid %d: origin %.1f %.1f %.1f, %.0f u/s, spin %.1f rad/s, %s (island of %d, %d steps)\n", NUM_FOR_EDICT(ent),
                ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], glm::length(b.vel), glm::length(b.spin),
                body.sleepGroup ? "asleep" : body.contact ? "in contact" : "flying", static_cast<int>(members.size()), steps);
        }
    }
}

// Every prop that collides with the others, stepped together, once a frame.
void stack(float dt)
{
    QVR_PROFILE("props collide");
    StackState& s = stackState;
    const FieldOffsets& f = fields();
    const int mode = collideMode();
    s.frame++;

    if(s.carryField == -2)
    {
        s.carryField = ED_FindFieldOffset("carry_player");
    }

    // The bodies: rigid props, and carried ones (kinematic).
    const int count = qcvm->num_edicts;
    s.bodies.clear();
    s.index.assign(static_cast<size_t>(count), -1);
    if(s.groupOf.size() < static_cast<size_t>(count))
    {
        s.groupOf.resize(static_cast<size_t>(count) + 64, 0);
        s.handled.resize(static_cast<size_t>(count) + 64, 0);
    }
    for(int num = svs.maxclients + 1; num < count; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(ent->free || ent->v.modelindex <= 0.f || !collidesWithProps(ent, mode))
        {
            continue;
        }
        const int movetype = static_cast<int>(ent->v.movetype);
        const bool rigid = fieldFloat(ent, f.vr_rigid) != 0.f && (movetype == MOVETYPE_TOSS || movetype == MOVETYPE_BOUNCE);
        const bool carried = !rigid && movetype == MOVETYPE_NONE && s.carryField >= 0 &&
                             (static_cast<int>(ent->v.flags) & (FL_ITEM | physics::FL_FORCEGRABBABLE)) &&
                             GetEdictFieldValue(ent, s.carryField)->edict != 0;
        if(!rigid && !carried)
        {
            continue;
        }

        StackBody body;
        Body& b = body.b;
        b.ent = ent;
        body.kinematic = carried;
        if(carried)
        {
            shape(body);
            // Its motion since the last frame (none the first frame it is carried, or after a jump).
            KinematicPose& last = s.kinematics[num];
            b.vel = glm::vec3{0.f};
            b.spin = glm::vec3{0.f};
            if(last.frame == s.frame - 1 && glm::length(b.com - last.com) < 64.f)
            {
                b.vel = (b.com - last.com) / dt;
                const glm::quat q = glm::quat_cast(b.rot * glm::transpose(last.rot));
                const float angle = 2.f * std::acos(std::min(1.f, std::abs(q.w)));
                const glm::vec3 axis{q.x, q.y, q.z};
                if(glm::length(axis) > 1e-6f)
                {
                    b.spin = glm::normalize(axis) * (q.w < 0.f ? -angle : angle) / dt;
                }
            }
            last = KinematicPose{b.com, b.rot, s.frame};
            body.invMass = 0.f;
            body.awake = glm::length(b.vel) > 1.f || glm::length(b.spin) > 0.05f;
        }
        else
        {
            b.vel = toGlm(ent->v.velocity);
            b.spin = fieldVec(ent, f.vr_spin);
            body.awake = !(static_cast<int>(ent->v.flags) & FL_ONGROUND) || fieldFloat(ent, f.vr_rest) < 0.f;
            s.handled[num] = s.frame;
        }
        s.index[num] = static_cast<int>(s.bodies.size());
        s.bodies.push_back(body);
    }

    // Islands asleep: still, carried along (asleep again), or woken.
    checkGroups();
    bool anyAwake = false;
    for(const StackBody& body : s.bodies)
    {
        anyAwake = anyAwake || body.awake;
    }

    // Broad phase: sort and sweep along x. Nothing awake (or moved by a hand): nothing to do beyond this.
    s.overlaps.clear();
    s.pairs.clear();
    if(anyAwake)
    {
        for(StackBody& body : s.bodies)
        {
            shape(body);
        }
        std::vector<int> order(s.bodies.size());
        for(size_t i = 0; i < order.size(); i++)
        {
            order[i] = static_cast<int>(i);
            boundBody(s.bodies[i], dt);
        }
        std::sort(order.begin(), order.end(), [&](int x, int y) { return s.bodies[x].boxLo.x < s.bodies[y].boxLo.x; });
        for(size_t x = 0; x < order.size(); x++)
        {
            const StackBody& A = s.bodies[order[x]];
            for(size_t y = x + 1; y < order.size(); y++)
            {
                const StackBody& B = s.bodies[order[y]];
                if(B.boxLo.x > A.boxHi.x)
                {
                    break;
                }
                if(A.kinematic && B.kinematic)
                {
                    continue;
                }
                if(B.boxLo.y <= A.boxHi.y && A.boxLo.y <= B.boxHi.y && B.boxLo.z <= A.boxHi.z && A.boxLo.z <= B.boxHi.z)
                {
                    // In entity order (bodies are gathered so), whatever the sort: the same pair the same way every
                    // frame, so its contacts (and their warm starting) keep their sides.
                    s.overlaps.emplace_back(std::min(order[x], order[y]), std::max(order[x], order[y]));
                }
            }
        }

        // Pairs with a body awake take part; a sleeping body an awake one (or a moving hand's) reaches wakes, with
        // its island, which may wake more.
        std::vector<char> used(s.overlaps.size(), 0);
        for(bool changed = true; changed;)
        {
            changed = false;
            for(size_t k = 0; k < s.overlaps.size(); k++)
            {
                if(used[k])
                {
                    continue;
                }
                StackBody& A = s.bodies[s.overlaps[k].first];
                StackBody& B = s.bodies[s.overlaps[k].second];
                if(!A.awake && !B.awake)
                {
                    continue; // both asleep, or a sleeping body and a still hand
                }
                StackBody* sleeper = !A.kinematic && !A.awake ? &A : !B.kinematic && !B.awake ? &B : nullptr;
                if(sleeper)
                {
                    const float reach = (glm::length(A.b.vel - B.b.vel) + glm::length(A.b.spin) * glm::length(A.b.half) +
                                         glm::length(B.b.spin) * glm::length(B.b.half)) * dt;
                    boxbox::Manifold m;
                    if(!boxbox::collide(A.box(), B.box(), 0.5f + std::min(reach, 64.f), m))
                    {
                        continue;
                    }
                    wake(*sleeper);
                    changed = true;
                }
                used[k] = 1;
                s.pairs.push_back(s.overlaps[k]);
            }
        }
    }

    // Islands: awake bodies joined by pairs (carried props join none: nothing moves them).
    s.parent.resize(s.bodies.size());
    for(size_t i = 0; i < s.bodies.size(); i++)
    {
        s.parent[i] = static_cast<int>(i);
        s.bodies[i].island = -1;
    }
    std::vector<char> paired(s.bodies.size(), 0);
    for(const auto& [i, j] : s.pairs)
    {
        paired[i] = paired[j] = 1;
        if(!s.bodies[i].kinematic && !s.bodies[j].kinematic)
        {
            s.parent[findRoot(s.parent, i)] = findRoot(s.parent, j);
        }
    }
    std::unordered_map<int, std::vector<int>> islands;
    for(size_t i = 0; i < s.bodies.size(); i++)
    {
        StackBody& body = s.bodies[i];
        if(body.kinematic)
        {
            continue;
        }
        const int num = NUM_FOR_EDICT(body.b.ent);
        if(!paired[i])
        {
            // Alone: rigidToss (awake, or asleep by itself). Asleep in an island: nothing.
            if(s.groupOf[num] == 0)
            {
                rigidToss(body.b.ent);
            }
            continue;
        }
        body.island = findRoot(s.parent, static_cast<int>(i));
        islands[body.island].push_back(static_cast<int>(i));
    }
    std::vector<std::pair<int, int>> islandPairs;
    for(auto& [root, members] : islands)
    {
        islandPairs.clear();
        for(const auto& [i, j] : s.pairs)
        {
            const int island = s.bodies[i].kinematic ? s.bodies[j].island : s.bodies[i].island;
            if(island == root)
            {
                islandPairs.emplace_back(i, j);
            }
        }
        solveIsland(members, islandPairs, dt);
    }

    // Forget what wasn't touched this frame.
    for(auto it = s.manifolds.begin(); it != s.manifolds.end();)
    {
        it = s.stamp - it->second.stamp > 64 ? s.manifolds.erase(it) : std::next(it);
    }
    for(auto it = s.kinematics.begin(); it != s.kinematics.end();)
    {
        it = it->second.frame != s.frame ? s.kinematics.erase(it) : std::next(it);
    }
}

// Whether stack() moves this rigid body (it is then moved once a frame with all the others, at the first one's
// turn); false: rigidToss alone, as ever.
[[nodiscard]] bool stacked(edict_t* ent)
{
    const int mode = collideMode();
    StackState& s = stackState;
    if(mode == 0)
    {
        if(!s.groups.empty())
        {
            s.groups.clear();
            std::fill(s.groupOf.begin(), s.groupOf.end(), 0);
        }
        return false;
    }
    if(!collidesWithProps(ent, mode))
    {
        return false;
    }
    const float dt = static_cast<float>(host_frametime);
    if(dt <= 0.f)
    {
        return true;
    }
    if(s.time != qcvm->time || s.world != sv.worldmodel)
    {
        s.time = qcvm->time;
        s.world = sv.worldmodel;
        stack(dt);
    }
    const size_t num = static_cast<size_t>(NUM_FOR_EDICT(ent));
    return num < s.handled.size() && s.handled[num] == s.frame;
}

// Items must never fall out of the world. Quake lets an entity whose box is buried in the level
// (a trace that starts and ends in solid) move freely, and it falls forever: a small ammo box
// clips with the player-sized hull, which a force grab that ends at a hand near a wall or a low
// ceiling can bury. Rigid bodies collide by their corners from their centre, and fall through
// the same way when their centre is inside. So the last place each item or rigid body was free is
// kept, and one found buried goes back there, still.
struct FreePlace
{
    glm::vec3 origin{0.f};
    bool valid = false;
};
std::vector<FreePlace> freePlaces; // by entity number, for this server

[[nodiscard]] bool buried(edict_t* ent, bool rigid)
{
    if(rigid)
    {
        glm::vec3 lo, hi;
        localBox(ent, lo, hi);
        const glm::vec3 centre = toGlm(ent->v.origin) + axesFromAngles(ent->v.angles, brushModel(ent)) * ((lo + hi) * 0.5f);
        vec3_t c{centre.x, centre.y, centre.z};
        return SV_PointContents(c) == CONTENTS_SOLID;
    }
    const trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent);
    return tr.allsolid;
}

void keepInWorld(edict_t* ent, bool rigid)
{
    const bool item = (static_cast<int>(ent->v.flags) & (FL_ITEM | physics::FL_FORCEGRABBABLE)) != 0;
    if(!rigid && !item)
    {
        return;
    }

    const int num = NUM_FOR_EDICT(ent);
    if(num >= static_cast<int>(freePlaces.size()))
    {
        freePlaces.resize(static_cast<size_t>(num) + 64);
    }
    FreePlace& place = freePlaces[num];

    // Resting where it was last found free: whether it is buried changes nothing (buried or not, it
    // stays and its free place is this one), so it is not asked (a trace every frame for every item).
    if(place.valid && (static_cast<int>(ent->v.flags) & FL_ONGROUND) && toGlm(ent->v.origin) == place.origin)
    {
        return;
    }
    if(!buried(ent, rigid))
    {
        place.origin = toGlm(ent->v.origin);
        place.valid = true;
        return;
    }
    // Resting on the ground it does not move (Quake skips it): only one moving is at risk.
    if(!place.valid || (static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        return;
    }
    // Already put back there and still buried (a door or a lift moved in): moving it back again would
    // loop every frame. Leave it to fall or be moved.
    if(toGlm(ent->v.origin) == place.origin)
    {
        place.valid = false;
        return;
    }
    Con_DPrintf("VR: %s buried at %.0f %.0f %.0f, back to %.0f %.0f %.0f\n", PR_GetString(ent->v.classname), ent->v.origin[0],
        ent->v.origin[1], ent->v.origin[2], place.origin.x, place.origin.y, place.origin.z);
    fromGlm(place.origin, ent->v.origin);
    VectorCopy(vec3_origin, ent->v.velocity);
    VectorCopy(vec3_origin, ent->v.avelocity);
    if(rigid)
    {
        setFieldVec(ent, fields().vr_spin, glm::vec3{0.f}); // put back still, not spinning on
    }
    SV_LinkEdict(ent, false);
}

// Held objects (QC's carryangles): the object keeps the turn it had in the hand when gripped. At
// the grip (`grab`), the object's rotation relative to the hand's is kept; after, the object's
// angles are the hand's rotation times that, in the object's own convention (brush or alias).
std::unordered_map<int, glm::mat3> carried; // entity -> its axes in the hand's frame

// For physics tests: "vr_rigid_place <entity> <x> <y> <z> [<pitch> <yaw> <roll> [<vx> <vy> <vz> [<sx> <sy> <sz>]]]"
// puts a rigid body there, turned so, moving so and spinning so (radians per second), awake;
// "vr_rigid_place <entity> main|off [<forward> <left> <up> [<pitch> <yaw> <roll>]]" at the first player's hand
// (and so far from it along its axes), still. <entity>: its number, a classname (the newest), or "new" (the
// newest rigid body: a backpack just dropped).
void place_f()
{
    if(!sv.active || Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_rigid_place <number | classname | new> <x> <y> <z> [<pitch> <yaw> <roll> [<vx> <vy> <vz> [<sx> <sy> <sz>]]]\n"
                   "       vr_rigid_place <number | classname | new> main|off [<forward> <left> <up> [<pitch> <yaw> <roll>]]\n");
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    struct Restore
    {
        qcvm_t* vm;
        ~Restore() { PR_PopQCVM(vm); }
    } restore{oldVm};
    const FieldOffsets& f = fields();
    edict_t* ent = nullptr;
    const char* which = Cmd_Argv(1);
    if(which[0] >= '0' && which[0] <= '9')
    {
        const int num = Q_atoi(which);
        ent = num > 0 && num < qcvm->num_edicts ? EDICT_NUM(num) : nullptr;
    }
    else
    {
        for(int i = qcvm->num_edicts - 1; i > 0 && !ent; i--)
        {
            edict_t* e = EDICT_NUM(i);
            const bool newest = !strcmp(which, "new") && f.vr_rigid >= 0 && fieldFloat(e, f.vr_rigid) != 0.f;
            if(!e->free && (newest || !strcmp(PR_GetString(e->v.classname), which)))
            {
                ent = e;
            }
        }
    }
    if(!ent || ent->free)
    {
        Con_Printf("vr_rigid_place: no such entity\n");
        return;
    }
    const auto arg = [](int i) { return i < Cmd_Argc() ? Q_atof(Cmd_Argv(i)) : 0.f; };
    if(const bool main = !strcmp(Cmd_Argv(2), "main"); main || !strcmp(Cmd_Argv(2), "off"))
    {
        // At the first player's hand, and so far along its forward, left and up.
        edict_t* player = EDICT_NUM(1);
        const glm::vec3 angles = fieldVec(player, main ? f.handrot : f.offhandrot);
        const glm::vec3 at = fieldVec(player, main ? f.handpos : f.offhandpos) +
                             held::axesFromAngles(&angles[0], true) * glm::vec3{arg(3), arg(4), arg(5)};
        fromGlm(at, ent->v.origin);
        if(Cmd_Argc() >= 9)
        {
            for(int i = 0; i < 3; i++)
            {
                ent->v.angles[i] = arg(6 + i);
            }
        }
        VectorCopy(vec3_origin, ent->v.velocity);
        setFieldVec(ent, f.vr_spin, glm::vec3{0.f});
        SV_LinkEdict(ent, false);
        Con_Printf("vr_rigid_place: %d %s at the %s hand\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), main ? "main" : "off");
        return;
    }
    if(Cmd_Argc() < 5)
    {
        Con_Printf("vr_rigid_place: where?\n");
        return;
    }
    for(int i = 0; i < 3; i++)
    {
        ent->v.origin[i] = arg(2 + i);
        ent->v.angles[i] = arg(5 + i);
        ent->v.velocity[i] = arg(8 + i);
    }
    setFieldVec(ent, f.vr_spin, glm::vec3{arg(11), arg(12), arg(13)});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    fieldFloat(ent, f.vr_rest) = 0.f;
    SV_LinkEdict(ent, false);
    Con_Printf("vr_rigid_place: %d %s\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname));
}

// For physics tests: "vr_rigid_dump [<classname>]" prints every rigid body (or those of a classname): its place,
// turn, speed, and whether it sleeps (alone, or in which island).
void dump_f()
{
    if(!sv.active)
    {
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    const FieldOffsets& f = fields();
    const char* only = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
    int shown = 0;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || f.vr_rigid < 0 || fieldFloat(e, f.vr_rigid) == 0.f || (only && strcmp(PR_GetString(e->v.classname), only)))
        {
            continue;
        }
        const int group = static_cast<size_t>(i) < stackState.groupOf.size() ? stackState.groupOf[i] : 0;
        Con_Printf("body %d %s: origin %.2f %.2f %.2f angles %.2f %.2f %.2f speed %.2f %s", i, PR_GetString(e->v.classname),
            e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2],
            VectorLength(e->v.velocity), (static_cast<int>(e->v.flags) & FL_ONGROUND) || group ? "asleep" : "awake");
        if(glm::length(fieldVec(e, f.vr_spin)) > 0.f)
        {
            Con_Printf(" spin %.2f", glm::length(fieldVec(e, f.vr_spin)));
        }
        if(group)
        {
            Con_Printf(" (island %d)", group);
        }
        Con_Printf("\n");
        shown++;
    }
    Con_Printf("vr_rigid_dump: %d bodies, %d islands asleep, time %.2f\n", shown, static_cast<int>(stackState.groups.size()), qcvm->time);
    PR_PopQCVM(oldVm);
}

} // namespace

// SV_Physics_Toss, after the think: the whole move of a rigid body. Water transitions are
// tracked on every path (asleep too): else an entity's "just spawned" waterlevel 1 stays, and
// the QC takes it for floating. Items and rigid bodies are kept in the world first.
extern "C" int VR_RigidToss(edict_t* ent)
{
    QVR_PROFILE("rigid bodies");
    if(static bool registered = false; !registered) // a test command (no init hook of its own)
    {
        registered = true;
        Cmd_AddCommand("vr_rigid_place", place_f);
        Cmd_AddCommand("vr_rigid_dump", dump_f);
    }
    const FieldOffsets& f = fields();
    const bool rigid = f.vr_rigid >= 0 && fieldFloat(ent, f.vr_rigid) != 0.f;
    keepInWorld(ent, rigid);

    // Its first move: SV_CheckWaterTransition takes the contents it ends in as where it spawned, so a
    // rocket or a nail fired into water from close by never went in (no splash, vr_physics.cpp). One
    // starting in the open is in the open.
    if(ent->v.watertype == 0.f && SV_PointContents(ent->v.origin) == CONTENTS_EMPTY)
    {
        ent->v.watertype = CONTENTS_EMPTY;
    }

    const int movetype = static_cast<int>(ent->v.movetype);
    if(!rigid || (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE))
    {
        physics::predictWaterEntry(ent); // its splash, if it goes into water in this move
        return 0;
    }

    if(box3d::toss(ent)) // vr_physics_engine 1: Box3D moves it, with the others, at the frame's end (vr_box3d.cpp)
    {
        return 1;
    }
    if(!stacked(ent)) // vr_physics_engine 0 with vr_props_collide: stepped with the others, at the first one's turn
    {
        rigidToss(ent);
    }
    if(!ent->free)
    {
        // Where it was after the last check, as that check left it: the contents there (the world's) are the
        // same, and the check would change nothing (no crossing).
        WaterMemo& w = waterMemo(ent);
        const glm::vec3 origin = toGlm(ent->v.origin);
        if(w.world != sv.worldmodel || w.origin != origin || w.watertype != ent->v.watertype ||
           w.waterlevel != ent->v.waterlevel)
        {
            SV_CheckWaterTransition(ent);
            w = {sv.worldmodel, origin, ent->v.watertype, ent->v.waterlevel};
        }
    }
    return 1;
}

namespace qvr::physics
{

void carryAngles(edict_t* ent, const float* handAngles, bool grab, float* out)
{
    const bool brush = brushModel(ent);
    const glm::mat3 hand = axesFromAngles(handAngles, true); // view angles: pitch as a brush model's
    const int num = NUM_FOR_EDICT(ent);
    const auto it = carried.find(num);
    if(grab || it == carried.end())
    {
        carried[num] = glm::transpose(hand) * axesFromAngles(ent->v.angles, brush);
        VectorCopy(ent->v.angles, out);
        return;
    }
    anglesFromAxes(orthonormalize(hand * it->second), out, brush);
}

bool pointInModelBox(edict_t* ent, const glm::vec3& p, float margin)
{
    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    const glm::mat3 axes = axesFromAngles(ent->v.angles, brushModel(ent));
    const glm::vec3 local = glm::transpose(axes) * (p - toGlm(ent->v.origin));
    // Thin things (a dropped gun) at least 6 units thick, so a hand can still find them.
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{3.f}) + glm::vec3{margin};
    return glm::all(glm::lessThanEqual(glm::abs(local - (lo + hi) * 0.5f), half));
}

glm::vec3 modelCentre(edict_t* ent)
{
    if(!modelOf(ent))
    {
        return toGlm(ent->v.origin) + (toGlm(ent->v.mins) + toGlm(ent->v.maxs)) * 0.5f;
    }
    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    return toGlm(ent->v.origin) + axesFromAngles(ent->v.angles, brushModel(ent)) * ((lo + hi) * 0.5f);
}

void resetRigidBodies()
{
    freePlaces.clear();
    restMemos.clear();
    waterMemos.clear();
    wedges.clear();
    carried.clear();
    stackState = StackState{};
    gibField = -2;
    carry2h::resetServer();
}

} // namespace qvr::physics
