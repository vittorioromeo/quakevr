#include "vr_alloccount.h"
#include "vr_tracy.h"
// vr_box3d_step.cpp -- the step: before it (spin align, lifts, liquids, slides), after it (sounds, falls, impacts,
// touches) and VR_PhysicsFrameEnd.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_decals.hpp"

namespace box3d::detail
{
namespace
{
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
// or a gib, round, a tenth: any turn across its length will do, and the middle one's is kept).
void spinAlign(Slot& s, float dt)
{
    const float strength = za::max(vr_throw_spin_align.value, 0.f) * spinAlignOf(s.model);
    if(strength <= 0.f || dt <= 0.f)
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
    if(k <= 0.f || rate < 1e-3f)
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
            if(!s.chunk) // (a chunk touches nothing: its box along its flight is only a cost)
            {
                touchNearby(ent, com, com + vel * dt);
            }
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
            if(s.flight)
            {
                spinAlign(s, dt);
            }
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
        if(s.lift != 0.f && (s.kind == Kind::Prop || s.kind == Kind::Corpse) && B3_IS_NON_NULL(s.body) && b3Body_IsValid(s.body))
        {
            b3Body_ApplyForceToCenter(s.body, b3Vec3{0.f, 0.f, s.lift}, false);
        }
    }
    for(const RagdollBodies& r : world->ragdolls) // (ragdollsInLiquids')
    {
        const bool driven = r.driveForceAt == qcvm->time; // (driveActive's, this frame)
        for(int b = 0; r.num > 0 && b < r.count; b++)
        {
            const auto i = static_cast<za::SizeT>(b);
            const b3BodyId body = r.body[i];
            if(partCut(r, b) || !b3Body_IsValid(body))
            {
                continue;
            }
            if(r.lift[i] != 0.f)
            {
                b3Body_ApplyForceToCenter(body, b3Vec3{0.f, 0.f, r.lift[i]}, false);
            }
            if(driven && r.driveForce[i] != glm::vec3{0.f})
            {
                b3Body_ApplyForceToCenter(body, b3v(r.driveForce[i]), false); // (awake: drive() woke it)
            }
        }
    }
}
} // namespace

// Ragdolls and corpses in water, slime and lava (NOTES.md start_2026-10-09_15-27-59; before, they fell through a pool at
// full gravity and landed on its floor at full speed): each part lifted by how deep it is in it, its weight times the
// liquid's Float (vr_ragdoll_float_water, _slime, _lava) at full depth (above 1 it floats, below it sinks; the torso
// floats more than the head and limbs, so a body floats face down, and an armoured knight less: it sinks), and dragged:
// its motion and spin damped at the liquid's Drag (vr_ragdoll_drag_*, 1/s) by how deep it is, more the faster it goes
// (twice at 60 units/s), so a fall into it is braked as it goes in (the splash) and a sink is slow. A knocked-down
// monster's landing under it is cushioned too (liquidFallShare).
// What liquid is at `p` (CONTENTS_WATER, _SLIME, _LAVA; else what is there), as wetFrom tells it.
[[nodiscard]] int liquidAt(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return VR_LiquidContents(sv.worldmodel, v, SV_PointContents(v)); // (SV_PointContents: a current is water)
}

namespace
{
// The liquid's Float and Drag (false: not a liquid).
[[nodiscard]] bool liquidFeel(int contents, float& lift, float& drag)
{
    switch(contents)
    {
        case CONTENTS_WATER: lift = vr_ragdoll_float_water.value, drag = vr_ragdoll_drag_water.value; return true;
        case CONTENTS_SLIME: lift = vr_ragdoll_float_slime.value, drag = vr_ragdoll_drag_slime.value; return true;
        case CONTENTS_LAVA: lift = vr_ragdoll_float_lava.value, drag = vr_ragdoll_drag_lava.value; return true;
        default: return false;
    }
}

// A body's own Float, times the liquid's: armour sinks (the knights, the death knights, the enforcers).
[[nodiscard]] float ownFloat(const edict_t* ent)
{
    const char* name = ad::vrClass(PR_GetString(ent->v.classname)); // AD support: AD's knights in plate sink as knights
    if(!strcmp(name, "monster_knight") || !strcmp(name, "monster_hell_knight") || !strcmp(name, "monster_ranged_knight"))
    {
        return 0.85f;
    }
    return !strcmp(name, "monster_enforcer") ? 0.92f : 1.f;
}

// A ragdoll's torso (its pelvis, its chest or spine): it floats more (the lungs) than its head and limbs.
[[nodiscard]] bool torsoPart(const ragdoll::Rig& rig, int b)
{
    const char* name = rig.bones[b].name;
    return b == 0 || !strncmp(name, "chest", 5) || !strncmp(name, "spine", 5) || !strncmp(name, "torso", 5) ||
           !strncmp(name, "belly", 5);
}

// One awake body in a liquid: its lift for this step (applied; returned for liftAgain: N up) and its drag. 0 out of any.
[[nodiscard]] float bodyInLiquid(b3BodyId body, float own, float g, float dt)
{
    const b3AABB box = b3Body_ComputeAABB(body);
    const float lo = box.lowerBound.z * world->m2u, hi = box.upperBound.z * world->m2u;
    const glm::vec3 com = world->toU(b3Body_GetWorldCenter(body));
    const float part = submerged(com, lo, hi);
    if(part <= 0.f)
    {
        return 0.f;
    }
    float lift = 0.f, drag = 0.f;
    if(!liquidFeel(liquidAt(glm::vec3{com.x, com.y, lo + 1.f}), lift, drag) && !liquidFeel(liquidAt(com), lift, drag))
    {
        return 0.f;
    }
    const float force = b3Body_GetMass(body) * g * za::max(lift, 0.f) * own * part / world->m2u;
    if(force > 0.f)
    {
        b3Body_ApplyForceToCenter(body, b3Vec3{0.f, 0.f, force}, false);
    }
    if(drag > 0.f)
    {
        const glm::vec3 vel = world->toU(b3Body_GetLinearVelocity(body));
        const float k = drag * part * (1.f + glm::length(vel) / 60.f);
        b3Body_SetLinearVelocity(body, world->toM(vel * za::exp(-k * dt)));
        b3Body_SetAngularVelocity(body, b3v(glmv(b3Body_GetAngularVelocity(body)) * za::exp(-2.f * k * dt)));
    }
    return force;
}

void ragdollsInLiquids(float dt)
{
    const float g = sv_gravity.value;
    if(vr_ragdoll_float_water.value <= 0.f && vr_ragdoll_float_slime.value <= 0.f && vr_ragdoll_float_lava.value <= 0.f &&
       vr_ragdoll_drag_water.value <= 0.f && vr_ragdoll_drag_slime.value <= 0.f && vr_ragdoll_drag_lava.value <= 0.f)
    {
        for(RagdollBodies& r : world->ragdolls)
        {
            r.lift = {};
        }
        return; // (all off: as before)
    }
    for(RagdollBodies& r : world->ragdolls)
    {
        r.lift = {};
        if(r.num <= 0 || r.num >= qcvm->num_edicts || !r.rig || g <= 0.f)
        {
            continue;
        }
        const float own = ownFloat(EDICT_NUM(r.num));
        for(int b = 0; b < r.count; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(!partCut(r, b) && b3Body_IsValid(body) && b3Body_IsAwake(body))
            {
                r.lift[static_cast<za::SizeT>(b)] = bodyInLiquid(body, own * (torsoPart(*r.rig, b) ? 1.15f : 0.9f), g, dt);
            }
        }
        if(vr_debug_ragdoll.value >= 2.f && r.lift[0] != 0.f) // (its pelvis in a liquid: the sink speed tests')
        {
            const glm::vec3 p = world->toU(b3Body_GetWorldCenter(r.body[0])), v = world->toU(b3Body_GetLinearVelocity(r.body[0]));
            Con_Printf("ragdoll %d in liquid %d: pelvis z %.1f, %.1f u/s up, %.1f across\n", r.num, liquidAt(p), p.z, v.z,
                glm::length(glm::vec2{v.x, v.y}));
        }
    }
    // (A corpse's one body: Corpse Collision's pushable corpses, ragdolls off.)
    for(int num = 1; num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts && g > 0.f; num++)
    {
        Slot& s = world->slots[num];
        if(s.kind == Kind::Corpse && s.corpseDynamic && s.ragdoll < 0 && b3Body_IsValid(s.body) && b3Body_IsAwake(s.body))
        {
            s.lift = bodyInLiquid(s.body, ownFloat(EDICT_NUM(num)), g, dt);
        }
    }
}

// The share of a monster's landing speed at `p` (on what it lands) its fall damage is measured at: liquid over it
// cushions it, none under vr_liquid_fall_cushion units of it, all with none (NOTES.md start_2026-10-09_15-27-59: thrown
// into a pool, a knocked-down grunt took the whole fall, or gibbed).
[[nodiscard]] float liquidFallShare(const glm::vec3& p)
{
    const float cushion = vr_liquid_fall_cushion.value;
    if(cushion <= 0.f || !sv.worldmodel)
    {
        return 1.f;
    }
    return 1.f - submerged(p, p.z, p.z + cushion);
}
} // namespace

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

namespace
{
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
        if(e.approachSpeed < least || !b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || chunkPair(e.shapeIdA, e.shapeIdB))
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
            // A hard prop's hard hit on the static world dents it (vr_decal_melee_prop_speed: a throw's; the normal out of
            // the world, from A to B the event's).
            if(b == 0 && (s.sound == physsound::Material::Wood || s.sound == physsound::Material::Metal ||
                             s.sound == physsound::Material::Stone || s.sound == physsound::Material::Brick))
            {
                const glm::vec3 n{static_cast<float>(e.normal.x), static_cast<float>(e.normal.y), static_cast<float>(e.normal.z)};
                decals::propHit(at, side ? n : -n, e.approachSpeed, b3Body_GetMass(s.body));
            }
        }
    }
}

// u/s: a ragdoll part this fast is shaking itself apart, not thrown (a judo throw's fastest part about 900, a rocket's
// blast's a couple of thousand; the mummy's tucked flesh shook its parts to 4000-12000).
constexpr float unstableSpeed = 5000.f;

// Before a piece of the step: each ragdoll's middle's speed down (RagdollBodies::fallDown; its parts' velocities by their
// masses, a loose weapon not counted) and what the piece's `dt` of gravity adds, what its landings in that piece are
// measured against (touches).
void noteFallSpeeds(float dt)
{
    const float gravity = za::max(-b3World_GetGravity(world->id).z, 0.f) * dt;
    for(RagdollBodies& r : world->ragdolls)
    {
        r.fallDown = r.bodySpeed = r.bodyMass = 0.f;
        if(r.num == 0 || !r.rig)
        {
            continue;
        }
        float mass = 0.f, down = 0.f;
        b3Vec3 momentum = b3Vec3_zero;
        for(int b = 0; b < r.count; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || r.rig->bones[b].joint == ragdoll::Joint::Loose || B3_IS_NULL(body) || !b3Body_IsValid(body))
            {
                continue;
            }
            const float m = b3Body_GetMass(body);
            mass += m;
            down -= b3Body_GetLinearVelocity(body).z * m;
            momentum = b3MulAdd(momentum, m, b3Body_GetLinearVelocity(body));
        }
        r.bodyMass = mass;
        r.bodySpeed = mass > 0.f ? b3Length(momentum) / mass : 0.f;
        // A part far faster than anything throws one (unstableSpeed): the ragdoll shaking itself apart (a judo-thrown
        // mummy's tucked flesh, welded in its chest, whirling: ragdoll::derive's looseSpeck). Said once a ragdoll, nothing
        // changed (its landings still count; the cause is to be found).
        for(int b = 0; b < r.count && !r.warnedUnstable; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || B3_IS_NULL(body) || !b3Body_IsValid(body))
            {
                continue;
            }
            const float speed = b3Length(b3Body_GetLinearVelocity(body)) * world->m2u;
            if(speed < unstableSpeed)
            {
                continue;
            }
            r.warnedUnstable = true;
            const char* say = "ragdoll: %d %s's %s at %.0f u/s (spin %.0f rad/s, %.2f kg): unstable?\n";
            const float spin = b3Length(b3Body_GetAngularVelocity(body));
            const char* cls = PR_GetString(EDICT_NUM(r.num)->v.classname);
            if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
            {
                Con_Printf(say, r.num, cls, r.rig->bones[b].name, speed, spin, b3Body_GetMass(body));
            }
            else
            {
                Con_DPrintf(say, r.num, cls, r.rig->bones[b].name, speed, spin, b3Body_GetMass(body));
            }
        }
        r.fallDown = mass > 0.f ? za::max(down / mass + gravity, 0.f) : 0.f;
    }
}

// Impacts (vr_ragimpact; QC vr_ragimpact.qc): a ragdoll's part striking the level (the world, a door, a fixture) at
// vr_ragimpact_min_speed or more (vr_ragimpact_held_speed for a part a hand holds), each part's hardest this frame by its
// speed times the mass behind it. Its speed: the part's approach, no faster than vr_ragimpact_whip times its body's before
// the step (noteFallSpeeds: a limb or a welded sword whipping into the floor on a throw is no blow), unless a hand holds
// the part (it drives it: a head held and slammed, the body dragged after it). The mass behind it: the part's, plus
// vr_ragimpact_body of the rest of its body, plus vr_ragimpact_held_mass for a hand's (your arm's) driving it. Not a
// loose weapon, nor a body staggering or getting up (its steps). The QC judges which bodies count and deals the damage
// (callRagImpacts).
void noteRagImpacts(const b3ContactEvents& events)
{
    const float least = za::max(vr_ragimpact_min_speed.value, 0.5f);
    const float leastHeld = za::max(vr_ragimpact_held_speed.value, 0.25f);
    if(vr_ragimpact.value == 0.f)
    {
        return;
    }
    const auto kindAt = [](int n) { return n > 0 && n < static_cast<int>(world->slots.size()) ? world->slots[n].kind : Kind::None; };
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(e.approachSpeed < za::min(least, leastHeld) || !b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB))
        {
            continue;
        }
        for(int side = 0; side < 2; side++)
        {
            const b3ShapeId shape = side ? e.shapeIdB : e.shapeIdA;
            const int a = numOf(shape), b = numOf(side ? e.shapeIdA : e.shapeIdB);
            const Kind level = kindAt(b);
            if(kindAt(a) != Kind::Corpse || world->slots[a].ragdoll < 0 || (b != 0 && level != Kind::Mover && level != Kind::Fixture))
            {
                continue;
            }
            const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(world->slots[a].ragdoll)];
            if(!r.rig || (r.active.on && (r.active.rise.on || r.active.stagger.on)))
            {
                continue;
            }
            const b3BodyId body = b3Shape_GetBody(shape);
            int part = -1;
            for(int k = 0; k < r.count && part < 0; k++)
            {
                if(B3_ID_EQUALS(r.body[static_cast<za::SizeT>(k)], body))
                {
                    part = k;
                }
            }
            if(part < 0 || r.rig->bones[part].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            // (A part a hand holds is driven by the hand, not whipped: a head held and slammed into a wall, the body
            // dragged after it slowly.)
            const bool held = za::findIf(world->ragdollGrabs.begin(), world->ragdollGrabs.end(),
                                  [a, part](const World::RagdollGrab& g) { return g.num == a && g.part == part; }) !=
                              world->ragdollGrabs.end();
            const float partMass = b3Body_GetMass(body);
            const float speed =
                held ? e.approachSpeed : za::min(e.approachSpeed, r.bodySpeed * za::max(vr_ragimpact_whip.value, 1.f));
            const float mass = partMass + za::clamp(vr_ragimpact_body.value, 0.f, 1.f) * za::max(r.bodyMass - partMass, 0.f) +
                               (held ? za::clamp(vr_ragimpact_held_mass.value, 0.f, 200.f) : 0.f);
            if(speed < (held ? leastHeld : least))
            {
                continue;
            }
            const Impact hit{a, world->toU(e.point), speed, e.approachSpeed, mass, partMass, r.bodyMass, held, part};
            auto it = za::findIf(world->ragImpacts.begin(), world->ragImpacts.end(),
                [a, part](const Impact& s) { return s.num == a && s.part == part; });
            if(it == world->ragImpacts.end())
            {
                world->ragImpacts.pushBack(hit);
            }
            else if(speed * mass > it->speed * it->mass)
            {
                *it = hit;
            }
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
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || chunkPair(e.shapeIdA, e.shapeIdB))
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
        // A projectile prop (QC VR_Proj_Mark: a live grenade) meeting a ragdoll touches its body: a living monster
        // knocked down (an active ragdoll) is struck as standing (a grenade goes off on it), a corpse is bounced off.
        else if(kindAt(a) == Kind::Prop && other == Kind::Corpse &&
                 fields().vr_projectile >= 0 && fieldFloat(EDICT_NUM(a), fields().vr_projectile) != 0.f)
        {
            out.emplaceBack(a, b);
        }
    }
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || e.approachSpeed * world->m2u < 60.f ||
            chunkPair(e.shapeIdA, e.shapeIdB))
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
            // (Its body getting up or staggering, rolling over and stepping: no fall.)
            if(const ragactive::State& act = world->ragdolls[static_cast<za::SizeT>(world->slots[a].ragdoll)].active;
                act.on && (act.rise.on || act.stagger.on))
            {
                continue;
            }
            // The part's own hit, no harder than the whole body came down (noteFallSpeeds): a light part at the end of a
            // turning body (a knight's sword welded in his hand, a fiend's tail, a hand) whips into the floor several times
            // faster than the body falls (a two-hand throw on flat ground: a knight's sword at 860-900 u/s, its body
            // under 130; "a 500 unit drop", 140 damage of its 75 health). A real drop brings all of it down that fast.
            // (Landing under water, slime or lava: cushioned by it, liquidFallShare.)
            const float hit = e.approachSpeed * za::fabs(e.normal.z);
            const float body = world->ragdolls[static_cast<za::SizeT>(world->slots[a].ragdoll)].fallDown;
            const float speed = za::min(hit, body) * world->m2u *
                                liquidFallShare(world->toU(e.point) + glm::vec3{0.f, 0.f, 1.f});
            if(vr_knockdown_debug.value >= 2.f && hit * world->m2u >= 150.f)
            {
                Con_Printf("ragdoll fall: %d %s hit the level at %.0f u/s, its body coming down at %.0f: lands at %.0f\n", a,
                    PR_GetString(EDICT_NUM(a)->v.classname), hit * world->m2u, body * world->m2u, speed);
            }
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
    noteRagImpacts(events);
    // A prop that wants to know how hard it hits (.vr_impact): its hardest hit on anything (the level too).
    const int impactField = fields().vr_impact;
    if(impactField < 0)
    {
        return;
    }
    for(int i = 0; i < events.hitCount; i++)
    {
        const b3ContactHitEvent& e = events.hitEvents[i];
        if(!b3Shape_IsValid(e.shapeIdA) || !b3Shape_IsValid(e.shapeIdB) || e.approachSpeed < 2.f || chunkPair(e.shapeIdA, e.shapeIdB))
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

// The ragdolls' parts' hardest strikes on the level this frame (noteRagImpacts, a part's each), to QC's
// VR_RagImpact_Hit(entity m, vector at, float speed, float mass, float raw, float held): it judges and deals them.
void callRagImpacts()
{
    const func_t fn = world->ragImpacts.empty() ? 0 : qvr::progs::findFunction("VR_RagImpact_Hit");
    for(const Impact& hit : world->ragImpacts)
    {
        if(!fn || hit.num >= qcvm->num_edicts)
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(hit.num);
        if(ent->free)
        {
            continue;
        }
        if(vr_knockdown_debug.value >= 2.f)
        {
            Con_Printf("ragdoll impact: %d %s at %.1f m/s (%.1f raw), %.1f kg behind it (its part %.1f of %.1f%s)\n", hit.num,
                PR_GetString(ent->v.classname), hit.speed, hit.raw, hit.mass, hit.partMass, hit.bodyMass, hit.held ? ", held" : "");
        }
        QcCallGuard guard;
        pr_global_struct->time = qcvm->time;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
        G_FLOAT(OFS_PARM1 + 0) = hit.at.x;
        G_FLOAT(OFS_PARM1 + 1) = hit.at.y;
        G_FLOAT(OFS_PARM1 + 2) = hit.at.z;
        G_FLOAT(OFS_PARM2) = hit.speed;
        G_FLOAT(OFS_PARM3) = hit.mass;
        G_FLOAT(OFS_PARM4) = hit.raw;
        G_FLOAT(OFS_PARM5) = hit.held ? 1.f : 0.f;
        PR_ExecuteProgram(fn);
    }
    world->ragImpacts.clear();
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
} // namespace

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

// Whether `round` passes through the gun lying about as `gun` to its load point (QC .vr_ammo_passgun: vr_reload.qc
// VR_Reload_PropsFrame), as a loose round passes through a held gun (reachSkips).
[[nodiscard]] bool passesGun(int round, int gun)
{
    const FieldOffsets& f = fields();
    if(f.vr_ammo_passgun < 0 || round <= svs.maxclients || gun <= svs.maxclients || round >= qcvm->num_edicts ||
        gun >= qcvm->num_edicts)
    {
        return false;
    }
    return fieldInt(EDICT_NUM(round), f.vr_ammo_passgun) == EDICT_TO_PROG(EDICT_NUM(gun));
}
} // namespace box3d::detail

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
        if(world)
        {
            world->climbPulls.clear();
        }
        return;
    }
    ragtest::frame(); // (the ragdoll tests under way, vr_ragdoll_act: their next steps, whether each began)
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
        QVR_TRACY_CALL("box3d sync entities", syncEntities(dt));
        QVR_TRACY_CALL("box3d sync hands", syncHands(dt));
        QVR_TRACY_CALL("box3d sync ragdoll grabs", syncRagdollGrabs(dt));
        if(!world->ragdollsInside.empty())
        {
            pruneRagdollsInside();
        }
        QVR_TRACY_CALL("box3d note throws", noteThrows());
        QVR_TRACY_CALL("box3d sync reach", syncReach(dt));
        QVR_TRACY_CALL("box3d sync portal copies", syncPortalCopies(dt));
        QVR_TRACY_CALL("box3d sync tool joints", syncToolJoints());
    }
    const double tSync = Sys_DoubleTime();
    {
        QVR_PROFILE("box3d water and hits");
        QVR_TRACY_CALL("box3d before step", beforeStep(dt));
        QVR_TRACY_CALL("box3d ragdolls in liquids", ragdollsInLiquids(dt));
        QVR_TRACY_CALL("box3d before standing", beforeStanding());
        QVR_TRACY_CALL("box3d shove bumped", shoveBumped(dt));
        QVR_TRACY_CALL("box3d nudge walked ragdolls", nudgeWalkedRagdolls(dt));
        QVR_TRACY_CALL("box3d unstick props", unstickProps());
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
            pressClimbing();
            noteFallSpeeds(dt / static_cast<float>(pieces));
            world->tasks.next.storeRelaxed(0);
            applySimdWidth(world->id);
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
    world->climbPulls.clear(); // (this frame's: climbPull)
    carryRagdolls(); // (through teleporters)
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
    {
        QVR_TRACY_SCOPE(tracyScope, "box3d publish bodies");
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
    }
    const double tAfter = Sys_DoubleTime();

    {
        QVR_TRACY_SCOPE(tracyScope, "box3d after callbacks");
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
        callRagImpacts();
        watchInside();
        physsound::frameEnd(); // the frame's knocks and scrapes

    }

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

extern "C" void VR_MonsterFell(edict_t* ent, float speed)
{
    if(ent)
    {
        // A standing monster landing (SV_Physics_Step): its feet's liquid cushions it (liquidFallShare).
        monsterFell(ent, speed * liquidFallShare(glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2] + ent->v.mins[2] + 1.f}));
    }
}
