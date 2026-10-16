// vr_propthink.cpp -- a force-grabbable thing's think in the engine (docs/vr-port/QC_VM.md, "Native prop think").
//
// QC forcegrabbable_think_impl (weapons.qc) runs for every thrown or dropped weapon, every pickup made an object and every
// bag, fifty times a second each: 5-18% of all the QuakeC's statements in a fight when it was QuakeC. The propthink
// builtin does its work here, step for step as that QuakeC did (forcegrabbable_think_qc, removed 2026-10-16 with its
// switch vr_prop_think_native and the vr_propthink_check that compared the two: identical over 200 made-up states a
// thing): the same float steps (QuakeC's constants are floats: 0.85f, 0.15f, 0.3f), the same random() draw (the
// server's stream), the same particle2 message, and the edict index told of .flags as QuakeC's store told it.

#include "vr_propthink.hpp"

#include "vr_cvars.hpp"
#include "vr_edictindex.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"

#include <math.h>
#include <stddef.h>
#include <string.h>

namespace qvr::propthink
{
namespace
{

// QuakeC's constants (QC defs.qc, vr_defs.qc).
constexpr float throwhitHit = 1.f;   // QVR_THROWHIT_HIT
constexpr int presetGunPickup = 8;   // QVR_PARTICLE_PRESET_GUNPICKUP
constexpr int flagsField = static_cast<int>(offsetof(entvars_t, flags) / 4);
// An angle further out than this to settle: brought into a turn first (fmodf) rather than ~180 or more turns of the
// settling's loop (an infinite one: NaN, set straight as a NaN is).
constexpr float farAngle = 65536.f;

// The VR fields the think uses, all found (a progs without one: nothing done).
[[nodiscard]] bool haveFields(const progs::FieldOffsets& f)
{
    return f.ammocounter >= 0 && f.vr_rigid >= 0 && f.throwhit >= 0 && f.vr_spin >= 0 && f.lastwatertime >= 0 &&
           f.throwstabilize >= 0 && f.throwstabilizedim >= 0;
}

// (As QC wpnthrow_impl_stabilize did:) the angle (`xAngle`, its field `out`) brought round by .throwstabilize towards the nearest
// of offset - 90 and offset - 270, and .throwstabilize less 0.3 (not below 0).
void stabilizeAngle(edict_t* self, float* stabilize, float xOffset, float xAngle, float* out)
{
    float angle = xAngle - xOffset;

    // Brought into [-360, 0].
    while(angle > 0.f)
    {
        *out = (xOffset + angle) - 360.f;
        angle = angle - 360.f;
    }
    while(angle < -360.f)
    {
        *out = (xOffset + angle) + 360.f;
        angle = angle + 360.f;
    }

    const float s = *stabilize;
    if(angle >= -360.f && angle <= -270.f)
    {
        *out = (xOffset + angle) + s;
        if((xAngle - xOffset) + s > -270.f)
        {
            *out = xOffset + -270.f;
        }
    }
    else if(angle > -270.f && angle <= -180.f)
    {
        *out = (xOffset + angle) - s;
        if((xAngle - xOffset) - s < -270.f)
        {
            *out = xOffset + -270.f;
        }
    }
    else if(angle > -180.f && angle <= -90.f)
    {
        *out = (xOffset + angle) + s;
        if((xAngle - xOffset) + s > -90.f)
        {
            *out = xOffset + -90.f;
        }
    }
    else if(angle > -90.f && angle <= 0.f)
    {
        *out = (xOffset + angle) - s;
        if((xAngle - xOffset) - s < -90.f)
        {
            *out = xOffset + -90.f;
        }
    }
    else
    {
        // (a NaN angle: set straight, and the spin with it)
        Con_DPrintf("wpnthrow_impl_stabilize: angle %g set to %g\n", static_cast<double>(angle), static_cast<double>(xOffset));
        *out = xOffset;
        self->v.avelocity[0] = self->v.avelocity[1] = self->v.avelocity[2] = 0.f;
    }

    if(*stabilize > 0.f)
    {
        *stabilize = *stabilize - 0.3f;
        if(*stabilize < 0.f)
        {
            *stabilize = 0.f;
        }
    }
}

// (As QC wpnthrow_stabilize did:) the angle .throwstabilizedim names (0 pitch round 90, 1 yaw, 2 roll round 0), unless it is
// settled already (no push left and in range).
void stabilize(edict_t* self, const progs::FieldOffsets& f)
{
    float* stab = &progs::fieldFloat(self, f.throwstabilize);
    const float dim = progs::fieldFloat(self, f.throwstabilizedim);
    float* ang = self->v.angles;
    if(dim == 0.f)
    {
        if(*stab == 0.f && ang[0] <= 90.f && ang[0] >= -270.f)
        {
            return;
        }
        stabilizeAngle(self, stab, 90.f, ang[0], &ang[0]);
    }
    else if(dim == 1.f)
    {
        if(*stab == 0.f && ang[1] <= 0.f && ang[1] >= -360.f)
        {
            return;
        }
        stabilizeAngle(self, stab, 0.f, ang[1], &ang[1]);
    }
    else if(dim == 2.f)
    {
        if(*stab == 0.f && ang[2] <= 0.f && ang[2] >= -360.f)
        {
            return;
        }
        stabilizeAngle(self, stab, 0.f, ang[2], &ang[2]);
    }
}

[[nodiscard]] float withFlag(float flags, int flag)
{
    return static_cast<float>(static_cast<int>(flags) | flag); // (QuakeC's OP_BITOR)
}

// The think on `self` (vr_weapondrop_particles_on: `particles`); false: nothing done (a progs without the fields).
[[nodiscard]] bool think(edict_t* self, float particles)
{
    const progs::FieldOffsets& f = progs::fields();
    if(!haveFields(f))
    {
        return false;
    }
    entvars_t& v = self->v;
    const float rigid = progs::fieldFloat(self, f.vr_rigid);
    if(rigid == 0.f)
    {
        // (neither is written before the settling reads them)
        const float dim = progs::fieldFloat(self, f.throwstabilizedim);
        const int axis = dim == 0.f ? 0 : dim == 1.f ? 1 : dim == 2.f ? 2 : -1;
        if(axis >= 0 && fabsf(v.angles[axis]) > farAngle)
        {
            v.angles[axis] = fmodf(v.angles[axis], 360.f);
        }
    }

    float& ammo = progs::fieldFloat(self, f.ammocounter);
    float& throwhit = progs::fieldFloat(self, f.throwhit);
    const float time = pr_global_struct->time;
    const float oldFlags = v.flags;

    // In water (not the "just spawned" waterlevel 1 of an entity never checked since).
    if(v.waterlevel >= 1.f && v.watertype <= static_cast<float>(CONTENTS_WATER))
    {
        if(ammo < 2.f)
        {
            ammo = ammo + 1.f;
        }
        else if(ammo > 0.f)
        {
            ammo = ammo - 1.f;
        }

        if(ammo > 1.f && rigid != 0.f)
        {
            // (rigid bodies float in the engine: vr_rigid.cpp)
            v.flags = withFlag(v.flags, FL_INWATER);
            throwhit = throwhitHit;
            v.solid = static_cast<float>(SOLID_NOT_BUT_TOUCHABLE);
        }
        else if(ammo > 1.f)
        {
            for(int i = 0; i < 2; i++)
            {
                if(fabsf(v.velocity[i]) > 10.f)
                {
                    v.velocity[i] = v.velocity[i] * 0.85f;
                }
                else
                {
                    v.velocity[i] = 0.f;
                }
            }
            v.velocity[2] = 45.f;
            v.flags = withFlag(v.flags, FL_INWATER);
            throwhit = throwhitHit;
            v.solid = static_cast<float>(SOLID_NOT_BUT_TOUCHABLE);
            v.avelocity[0] = v.avelocity[1] = v.avelocity[2] = 0.f;
            float* spin = progs::fieldPtr(self, f.vr_spin);
            spin[0] = spin[1] = spin[2] = 0.f;
        }
    }
    else if((time - progs::fieldFloat(self, f.lastwatertime)) > 0.15f)
    {
        v.flags = static_cast<float>(static_cast<int>(v.flags) & ~FL_INWATER); // (QuakeC's OP_BITAND)
        ammo = 0.f;
        v.waterlevel = 0.f;
    }
    else if(ammo > 0.f)
    {
        ammo = ammo - 1.f;
        v.waterlevel = 0.f;
    }

    if(v.flags != oldFlags)
    {
        edictindex::fieldStored(self, flagsField); // (FL_INWATER: QuakeC's store would have told it)
    }

    if(particles != 0.f && (static_cast<int>(v.flags) & (FL_ONGROUND | FL_INWATER)))
    {
        // (random() drawn first, as the QuakeC; its model checked as `!= string_null`: not empty)
        if(PR_RandomNumber() < 0.2f && PR_GetString(v.model)[0] != '\0')
        {
            float org[3]; // QC realorigin(self)
            for(int i = 0; i < 3; i++)
            {
                org[i] = (v.absmin[i] + v.absmax[i]) * 0.5f;
            }
            constexpr float still[3] = {0.f, 0.f, 0.f};
            progs::sendParticle2(org, still, presetGunPickup, 1);
        }
    }

    if(throwhit == throwhitHit && rigid == 0.f) // rigid ones settle in the engine
    {
        stabilize(self, f);
    }
    return true;
}

} // namespace

void PF_propthink()
{
    edict_t* self = PROG_TO_EDICT(pr_global_struct->self);
    const bool done = qcvm == &sv.qcvm && self != sv.qcvm.edicts && think(self, G_FLOAT(OFS_PARM0)); // (world: not)
    G_FLOAT(OFS_RETURN) = done ? 1.f : 0.f;
}

} // namespace qvr::propthink
