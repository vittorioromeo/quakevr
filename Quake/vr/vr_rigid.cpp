// vr_rigid.cpp -- thrown weapons fly, bounce and come to rest as rigid bodies.
//
// Quake tosses objects with Euler-angle rates (.avelocity added to pitch, yaw and roll), which
// only spins sensibly around one axis: a hand's real spin, mixed across axes, came out as wild
// tumbling. They also stop dead on the first floor they touch, and the old QC stabilized their
// angles in fixed steps, which wiggled. For entities whose QC sets .vr_rigid, SV_Physics_Toss
// runs this instead:
// - .vr_spin is a true angular velocity (radians per second, world axes), integrated as a
//   rotation, slowed by vr_throw_spin_drag in the air (more in water);
// - gravity is the entity's own (.gravity: QC keeps it true to scale for thrown weapons);
// - hits bounce with vr_throw_restitution and Coulomb friction (vr_throw_friction), so objects
//   hop, slide and come to rest;
// - resting, they turn onto their nearest flat side (never on end) at vr_throw_settle_rate and
//   then keep still;
// - on the way, a box of half-size vr_throw_hitbox finds monsters (and anything that takes
//   damage) the small box would slip past, so throws that look like hits are hits.

#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>

extern "C"
{
    // sv_phys.c (not declared in any header).
    void SV_CheckVelocity(edict_t* ent);
    void SV_AddGravity(edict_t* ent);
    trace_t SV_PushEntity(edict_t* ent, vec3_t push);
    void SV_CheckWaterTransition(edict_t* ent);
    void SV_Impact(edict_t* e1, edict_t* e2);
    int VR_TossKeepsGround(edict_t* ent);
}

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

[[nodiscard]] float metersToUnits()
{
    return vr_world_scale.value / 0.0381f;
}

// Resting: turn onto the nearest flat side, the side (y) or the top or bottom (z) facing up;
// never on the ends (x), which long weapons do not stand on.
void settle(edict_t* ent, float dt)
{
    glm::mat3 axes = axesFromAngles(ent->v.angles);
    const glm::vec3 up{0.f, 0.f, 1.f};

    glm::vec3 best = axes[1];
    const std::array<glm::vec3, 4> candidates{axes[1], -axes[1], axes[2], -axes[2]};
    for(const glm::vec3& c : candidates)
    {
        if(glm::dot(c, up) > glm::dot(best, up))
        {
            best = c;
        }
    }

    const float cosAngle = CLAMP(-1.f, glm::dot(best, up), 1.f);
    const float angle = std::acos(cosAngle);
    if(angle < glm::radians(0.5f))
    {
        return;
    }

    glm::vec3 axis = glm::cross(best, up);
    if(glm::length(axis) < 1e-5f)
    {
        axis = axes[0];
    }
    const float step = angle * (1.f - std::exp(-std::max(vr_throw_settle_rate.value, 0.f) * dt));
    axes = glm::mat3_cast(glm::angleAxis(step, glm::normalize(axis))) * axes;
    anglesFromAxes(axes, ent->v.angles);
}

// Monsters and other damageable entities the (small) box would pass, within the larger hit box.
void touchNearby(edict_t* ent, const glm::vec3& move)
{
    const float half = vr_throw_hitbox.value;
    if(half <= 0.f)
    {
        return;
    }

    vec3_t mins{-half, -half, -half}, maxs{half, half, half}, end;
    fromGlm(toGlm(ent->v.origin) + move, end);
    const trace_t tr = SV_Move(ent->v.origin, mins, maxs, end, MOVE_NORMAL, ent);
    edict_t* hit = tr.ent;
    if(!hit || hit == qcvm->edicts || hit->free || hit == PROG_TO_EDICT(ent->v.owner) || hit->v.takedamage == 0.f)
    {
        return;
    }
    SV_Impact(ent, hit);
}

} // namespace

// SV_Physics_Toss, after thinking: nonzero if this moved the entity.
extern "C" int VR_RigidToss(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const int movetype = static_cast<int>(ent->v.movetype);
    if(f.vr_rigid < 0 || fieldFloat(ent, f.vr_rigid) == 0.f || (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE))
    {
        return 0;
    }

    const float dt = static_cast<float>(host_frametime);
    glm::vec3 spin{0.f};
    if(f.vr_spin >= 0)
    {
        const float* s = fieldPtr(ent, f.vr_spin);
        spin = {s[0], s[1], s[2]};
    }
    const auto storeSpin = [&] {
        if(f.vr_spin >= 0)
        {
            float* s = fieldPtr(ent, f.vr_spin);
            s[0] = spin.x;
            s[1] = spin.y;
            s[2] = spin.z;
        }
    };
    VectorCopy(vec3_origin, ent->v.avelocity);

    if(static_cast<int>(ent->v.flags) & FL_ONGROUND)
    {
        if(VR_TossKeepsGround(ent))
        {
            spin = glm::vec3{0.f};
            storeSpin();
            settle(ent, dt);
            return 1;
        }
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    }

    SV_CheckVelocity(ent);
    SV_AddGravity(ent);

    // Spin: slowed by the air (and much more by water), turning the entity.
    const float drag = std::max(vr_throw_spin_drag.value, 0.f) + (ent->v.waterlevel > 0.f ? 3.f : 0.f);
    spin *= std::exp(-drag * dt);
    if(const float rate = glm::length(spin); rate > 1e-4f)
    {
        const glm::mat3 axes = glm::mat3_cast(glm::angleAxis(rate * dt, spin / rate)) * axesFromAngles(ent->v.angles);
        anglesFromAxes(axes, ent->v.angles);
    }

    const float restitution = CLAMP(0.f, vr_throw_restitution.value, 1.f);
    const float friction = std::max(vr_throw_friction.value, 0.f);
    const float restSpeed = 0.8f * metersToUnits();

    float left = dt;
    for(int bump = 0; bump < 3 && left > 0.f; bump++)
    {
        glm::vec3 vel = toGlm(ent->v.velocity);
        const glm::vec3 move = vel * left;
        touchNearby(ent, move);
        if(ent->free)
        {
            return 1;
        }

        vec3_t push;
        fromGlm(move, push);
        const trace_t tr = SV_PushEntity(ent, push);
        if(ent->free)
        {
            return 1;
        }
        if(tr.fraction >= 1.f)
        {
            break;
        }
        left *= 1.f - tr.fraction;

        // The touch may have changed the velocity (QC): bounce what is left of it.
        vel = toGlm(ent->v.velocity);
        const glm::vec3 n = toGlm(tr.plane.normal);
        const float vn = glm::dot(vel, n);
        if(vn < 0.f)
        {
            const glm::vec3 normal = n * vn;
            glm::vec3 tangent = vel - normal;
            const float impulse = (1.f + restitution) * -vn;
            if(const float t = glm::length(tangent); t > 1e-4f)
            {
                tangent *= std::max(0.f, 1.f - friction * impulse / t);
            }
            vel = tangent - normal * restitution;
            spin *= 0.5f; // the knock takes much of the spin
        }

        if(n.z > 0.7f && glm::length(vel) < restSpeed)
        {
            // At rest on this floor.
            vel = glm::vec3{0.f};
            spin = glm::vec3{0.f};
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            ent->v.groundentity = EDICT_TO_PROG(tr.ent);
            fromGlm(vel, ent->v.velocity);
            break;
        }
        fromGlm(vel, ent->v.velocity);
    }

    storeSpin();
    SV_CheckWaterTransition(ent);
    return 1;
}
