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
//   steps. A contact margin keeps resting corners in contact, and static friction holds slow
//   bodies on floors flatter than their friction angle. So a weapon lands on a corner, tips
//   over and comes to rest on a side, on floors and slopes alike.
// - At rest (in contact, slow, for a while) the body sleeps: Quake's FL_ONGROUND, moved along
//   by lifts; it wakes when what holds it goes away or when QC gives it a velocity.
// - On the way, a box of half-size vr_throw_hitbox finds monsters the thin corners would slip
//   past, so throws that look like hits are hits.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_weapons.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <vector>
#include <cmath>

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

// Model axes (x forward, y left, z up) of an alias entity's angles, whose pitch is inverted.
[[nodiscard]] glm::mat3 axesFromAngles(const vec3_t angles)
{
    vec3_t a{-angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    return glm::mat3{toGlm(f), -toGlm(r), toGlm(u)};
}

void anglesFromAxes(const glm::mat3& m, vec3_t out)
{
    const glm::vec3 a = hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
    out[0] = -a.x;
    out[1] = a.y;
    out[2] = a.z;
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
    const int index = static_cast<int>(ent->v.modelindex);
    qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model || model->type != mod_alias)
    {
        lo = toGlm(ent->v.mins);
        hi = toGlm(ent->v.maxs);
        return;
    }

    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const FieldOffsets& f = fields();
    const glm::vec3 netScale = glm::vec3{1.f} + fieldVec(ent, f.model_scale);
    const glm::vec3 netScaleOrigin = fieldVec(ent, f.model_scale_origin);
    const glm::vec3 netOffset = fieldVec(ent, f.model_offset);
    const glm::vec3 so{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 hs{hdr->scale[0], hdr->scale[1], hdr->scale[2]};

    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 v{(i & 1) ? model->maxs[0] : model->mins[0], (i & 2) ? model->maxs[1] : model->mins[1],
            (i & 4) ? model->maxs[2] : model->mins[2]};
        glm::vec3 raw = (v - so) / hs;
        glm::vec3 p = raw;
        if(t.active)
        {
            p = (raw + netOffset) * t.scale;
            p = so + hs * p;
            p = (t.offset + p) * t.k;
        }
        else
        {
            p = so + hs * (raw + netOffset);
        }
        p = netScaleOrigin + (p - netScaleOrigin) * netScale;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }

    // Never thinner than a unit, so that the corners span a volume.
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{0.5f});
    lo = centre - half;
    hi = centre + half;
}

[[nodiscard]] float gravityOf(edict_t* ent)
{
    const eval_t* val = GetEdictFieldValueByName(ent, "gravity");
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

[[nodiscard]] trace_t pointTrace(const glm::vec3& from, const glm::vec3& to, edict_t* pass)
{
    vec3_t start, end;
    fromGlm(from, start);
    fromGlm(to, end);
    return SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, pass);
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

    // Impulse j along `dir` at `r` (from the centre of mass).
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

// Whether the lowest corners rest on something (the body may sleep on it, or must wake).
[[nodiscard]] bool supported(Body& b, edict_t** ground)
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
    float target{0.f}; // wanted normal velocity: >= 0 pushes out or bounces, < 0 lets it approach
    float massN{0.f}, massT1{0.f}, massT2{0.f};
    float accN{0.f}, accT1{0.f}, accT2{0.f};
    edict_t* ent{nullptr};
};

} // namespace

// SV_Physics_Toss, after thinking: nonzero if this moved the entity.
namespace
{

[[nodiscard]] bool rigidToss(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const int movetype = static_cast<int>(ent->v.movetype);
    if(f.vr_rigid < 0 || fieldFloat(ent, f.vr_rigid) == 0.f || (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE))
    {
        return 0;
    }

    const float dt = static_cast<float>(host_frametime);
    if(dt <= 0.f)
    {
        return 1;
    }
    VectorCopy(vec3_origin, ent->v.avelocity);

    Body b;
    b.ent = ent;
    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    b.half = (hi - lo) * 0.5f;
    b.comLocal = (lo + hi) * 0.5f;
    b.rot = axesFromAngles(ent->v.angles);
    b.com = toGlm(ent->v.origin) + b.rot * b.comLocal;
    b.vel = toGlm(ent->v.velocity);
    b.spin = fieldVec(ent, f.vr_spin);
    const glm::vec3 size = b.half * 2.f;
    b.invInertia = 12.f / glm::vec3{size.y * size.y + size.z * size.z, size.x * size.x + size.z * size.z,
                              size.x * size.x + size.y * size.y};

    const auto store = [&] {
        fromGlm(b.com - b.rot * b.comLocal, ent->v.origin);
        anglesFromAxes(b.rot, ent->v.angles);
        fromGlm(b.vel, ent->v.velocity);
        setFieldVec(ent, f.vr_spin, b.spin);
        SV_LinkEdict(ent, true);
    };

    // Asleep: stays until what holds it goes away, or QC moves it.
    if(static_cast<int>(ent->v.flags) & FL_ONGROUND)
    {
        const bool pushed = glm::length(b.vel) > 1.f;
        if(!pushed && supported(b, nullptr))
        {
            return 1;
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

    // Substeps: at most half the box's thinnest half-extent and 0.25 radians each.
    const float travel = glm::length(b.vel) * dt / minHalf + glm::length(b.spin) * dt / 0.25f;
    const int steps = CLAMP(1, static_cast<int>(std::ceil(travel)), 8);
    const float h = dt / static_cast<float>(steps);

    bool contact = false;
    bool floorContact = false;
    edict_t* floorEnt = nullptr;

    for(int step = 0; step < steps; step++)
    {
        b.vel.z -= g * h;

        const float drag = std::max(vr_throw_spin_drag.value, 0.f) + (ent->v.waterlevel > 0.f ? 3.f : 0.f);
        b.spin *= std::exp(-drag * h);

        touchNearby(ent, b.com, b.com + b.vel * h);
        if(ent->free)
        {
            return 1;
        }

        // Contacts, corner by corner: inside a surface (from the centre, the corner is behind
        // it), or about to reach one within this step (speculative: the solver lets it come
        // exactly up to it).
        std::array<Contact, 8> contacts;
        int count = 0;
        for(const glm::vec3& r : b.corners())
        {
            const glm::vec3 p = b.com + r;
            Contact c;
            c.r = r;

            const trace_t inside = pointTrace(b.com, p, ent);
            if(!inside.startsolid && inside.fraction < 1.f)
            {
                c.n = toGlm(inside.plane.normal);
                const float depth = glm::dot(p - toGlm(inside.endpos), -c.n);
                c.target = std::min(depth, 4.f) * 0.3f / h; // push out over a few steps
                c.ent = inside.ent;
            }
            else
            {
                const glm::vec3 move = (b.vel + glm::cross(b.spin, r)) * h;
                const float length = glm::length(move);
                if(length < 1e-4f)
                {
                    continue;
                }
                const trace_t ahead = pointTrace(p, p + move + move / length * 0.25f, ent);
                if(ahead.startsolid || ahead.fraction >= 1.f)
                {
                    continue;
                }
                c.n = toGlm(ahead.plane.normal);
                if(glm::dot(move, c.n) >= 0.f)
                {
                    continue;
                }
                // Within the slop (traces keep an epsilon off surfaces) it already touches: it
                // must not come closer, so it is held (and friction acts) instead of sliding.
                const float gap = std::max(0.f, glm::dot(p - toGlm(ahead.endpos), c.n));
                c.target = gap < 0.5f ? 0.f : -(gap - 0.5f) / h;
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
            contacts[count++] = c;
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
                    return 1;
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

        b.com += b.vel * h;
        b.rot = turned(b.rot, b.spin, h);
    }

    // Sleep once in floor contact and slow for a moment.
    float& rest = fieldFloat(ent, f.vr_rest);
    const bool slow = glm::length(b.vel) < 0.15f * m2u && glm::length(b.spin) < 1.f;
    rest = floorContact && slow ? rest + dt : 0.f;
    if(rest > 0.3f && supported(b, &floorEnt))
    {
        b.vel = glm::vec3{0.f};
        b.spin = glm::vec3{0.f};
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
        edict_t* ground = floorEnt ? floorEnt : qcvm->edicts;
        ent->v.groundentity = EDICT_TO_PROG(ground);
    }

    store();

    if(vr_debug_throw.value >= 3.f)
    {
        Con_Printf("rigid %d: origin %.1f %.1f %.1f, %.0f u/s, spin %.1f rad/s, %s\n", NUM_FOR_EDICT(ent), ent->v.origin[0],
            ent->v.origin[1], ent->v.origin[2], glm::length(b.vel), glm::length(b.spin),
            (static_cast<int>(ent->v.flags) & FL_ONGROUND) ? "asleep" : contact ? "in contact" : "flying");
    }
    return 1;
}

} // namespace

namespace
{

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
std::vector<FreePlace> freePlaces;
const qmodel_t* freePlacesWorld = nullptr;

[[nodiscard]] bool buried(edict_t* ent, bool rigid)
{
    if(rigid)
    {
        glm::vec3 lo, hi;
        localBox(ent, lo, hi);
        const glm::vec3 centre = toGlm(ent->v.origin) + axesFromAngles(ent->v.angles) * ((lo + hi) * 0.5f);
        vec3_t c{centre.x, centre.y, centre.z};
        return SV_PointContents(c) == CONTENTS_SOLID;
    }
    const trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent);
    return tr.allsolid;
}

void keepInWorld(edict_t* ent, bool rigid)
{
    const bool item = (static_cast<int>(ent->v.flags) & (FL_ITEM | (1 << 15))) != 0; // FL_ITEM, QC's FL_FORCEGRABBABLE
    if(!rigid && !item)
    {
        return;
    }

    if(freePlacesWorld != sv.worldmodel)
    {
        freePlaces.clear();
        freePlacesWorld = sv.worldmodel;
    }
    const int num = NUM_FOR_EDICT(ent);
    if(num >= static_cast<int>(freePlaces.size()))
    {
        freePlaces.resize(static_cast<size_t>(num) + 64);
    }
    FreePlace& place = freePlaces[num];

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
    Con_DPrintf("VR: %s buried at %.0f %.0f %.0f, back to %.0f %.0f %.0f\n", PR_GetString(ent->v.classname), ent->v.origin[0],
        ent->v.origin[1], ent->v.origin[2], place.origin.x, place.origin.y, place.origin.z);
    fromGlm(place.origin, ent->v.origin);
    VectorCopy(vec3_origin, ent->v.velocity);
    VectorCopy(vec3_origin, ent->v.avelocity);
    SV_LinkEdict(ent, false);
}

} // namespace

// SV_Physics_Toss, after the think: the whole move of a rigid body. Water transitions are
// tracked on every path (asleep too): else an entity's "just spawned" waterlevel 1 stays, and
// the QC takes it for floating. Items and rigid bodies are kept in the world first.
extern "C" int VR_RigidToss(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const bool rigid = f.vr_rigid >= 0 && fieldFloat(ent, f.vr_rigid) != 0.f;
    keepInWorld(ent, rigid);
    if(ent->free)
    {
        return 1;
    }

    if(!rigidToss(ent))
    {
        return 0;
    }
    if(!ent->free)
    {
        SV_CheckWaterTransition(ent);
    }
    return 1;
}
