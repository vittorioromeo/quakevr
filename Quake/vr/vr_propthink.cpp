// vr_propthink.cpp -- a force-grabbable thing's think in the engine (docs/vr-port/QC_VM.md, "Native prop think").
//
// QC forcegrabbable_think_impl (weapons.qc) runs for every thrown or dropped weapon, every pickup made an object and every
// bag, fifty times a second each: 5-18% of all the QuakeC's statements in a fight. The propthink builtin does its work
// here, step for step as forcegrabbable_think_qc (its QuakeC, kept as the fallback): the same fields read and written in
// the same float steps (QuakeC's constants are floats: 0.85f, 0.15f, 0.3f), the same random() draw when QuakeC draws it
// (PR_RandomNumber: the server's stream), the same particle2 message, and the edict index told of .flags as QuakeC's
// store tells it. It returns FALSE having done nothing when vr_prop_think_native is 0, when the progs lack a field, or when
// the settling after a throw would bring round an angle so far out (or infinite) that QuakeC's loop would run long (or
// forever, to the runaway error): the QuakeC runs then, as before.
//
// vr_propthink_check [states] (Debug > Profiling): on each force-grabbable thing in the map, as it is and in `states`
// made-up states (in and out of water, on the ground, thrown, angles round every edge, NaN), runs the QuakeC and the
// native from the same state and compares the whole edict, the server's random stream and the particle message sent;
// everything is put back after.

#include "vr_propthink.hpp"

#include "vr_cvars.hpp"
#include "vr_edictindex.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "vr_srvrandom.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

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
// An angle further out than this (or infinite) to settle: the QuakeC brings it round (up to ~180 turns of its loop here).
constexpr float farAngle = 65536.f;

// The thinks done here and those left to the QuakeC since the last vr_propthink_check (the server's frame: main thread).
struct Counts
{
    za::I64 native{0};
    za::I64 declined{0};
};
Counts counts;

// The VR fields the think uses, all found (a progs without one: the QuakeC runs).
[[nodiscard]] bool haveFields(const progs::FieldOffsets& f)
{
    return f.ammocounter >= 0 && f.vr_rigid >= 0 && f.throwhit >= 0 && f.vr_spin >= 0 && f.lastwatertime >= 0 &&
           f.throwstabilize >= 0 && f.throwstabilizedim >= 0;
}

// QC wpnthrow_impl_stabilize: the angle (`xAngle`, its field `out`) brought round by .throwstabilize towards the nearest
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

// QC wpnthrow_stabilize: the angle .throwstabilizedim names (0 pitch round 90, 1 yaw, 2 roll round 0), unless it is
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

// forcegrabbable_think_qc on `self` (vr_weapondrop_particles_on: `particles`); false: nothing done, the QuakeC's to run.
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
            return false;
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

// ----------------------------------------------------------------------------
// vr_propthink_check

[[nodiscard]] int pick(za::FastNonCryptoRng& rng, int n)
{
    return static_cast<int>(rng.next() % static_cast<za::U64>(n));
}

template <za::SizeT N>
[[nodiscard]] float pickOf(za::FastNonCryptoRng& rng, const float (&values)[N])
{
    return values[pick(rng, static_cast<int>(N))];
}

// A made-up state for `e`: every branch of the think, its edges and NaN.
void makeUp(edict_t* e, const progs::FieldOffsets& f, za::FastNonCryptoRng& rng)
{
    const float nan = NAN;
    static constexpr float waterlevels[] = {0.f, 1.f, 2.f, 3.f};
    static constexpr float watertypes[] = {0.f, -1.f, -2.f, -3.f, -4.f, -5.f};
    static constexpr float ammos[] = {-1.f, 0.f, 0.5f, 1.f, 1.5f, 2.f, 3.f};
    static constexpr float speeds[] = {0.f, 10.f, -10.f, 10.0001f, -11.f, 300.f, -250.3f, 9.99f, 45.f};
    static constexpr float sinceWater[] = {-1.f, -0.15f, -0.1500001f, -0.149f, 0.f, 0.5f, 100.f};
    static constexpr int flagSets[] = {0, FL_INWATER, FL_ONGROUND, FL_INWATER | FL_ONGROUND};
    static constexpr float throwhits[] = {0.f, 1.f, 2.f};
    static constexpr float stabs[] = {0.f, 0.1f, 0.29f, 0.3f, 5.f, 30.f, -1.f};
    static constexpr float dims[] = {0.f, 1.f, 2.f, 3.f, -1.f};
    static constexpr float angles[] = {0.f, 1.f, -1.f, 90.f, 90.0001f, -90.f, -180.f, -180.5f, -270.f, -270.5f, -360.f,
        -360.1f, 359.f, 721.5f, -1000.f, 4000.f, 100000.f, -70000.f, 12.25f, -95.f};

    entvars_t& v = e->v;
    v.waterlevel = pickOf(rng, waterlevels);
    v.watertype = pickOf(rng, watertypes);
    progs::fieldFloat(e, f.ammocounter) = pickOf(rng, ammos);
    progs::fieldFloat(e, f.vr_rigid) = pick(rng, 3) == 0 ? 1.f : 0.f;
    for(int i = 0; i < 3; i++)
    {
        v.velocity[i] = pick(rng, 12) == 0 ? nan : pickOf(rng, speeds);
        v.avelocity[i] = pickOf(rng, speeds);
        progs::fieldPtr(e, f.vr_spin)[i] = pickOf(rng, speeds);
        v.angles[i] = pick(rng, 15) == 0 ? nan : pick(rng, 4) == 0 ? static_cast<float>(pick(rng, 4001) - 2000) * 0.37f
                                                                    : pickOf(rng, angles);
    }
    progs::fieldFloat(e, f.lastwatertime) = pr_global_struct->time + pickOf(rng, sinceWater);
    v.flags = static_cast<float>((static_cast<int>(v.flags) & ~(FL_INWATER | FL_ONGROUND)) | flagSets[pick(rng, 4)]);
    progs::fieldFloat(e, f.throwhit) = pickOf(rng, throwhits);
    progs::fieldFloat(e, f.throwstabilize) = pickOf(rng, stabs);
    progs::fieldFloat(e, f.throwstabilizedim) = pickOf(rng, dims);
    v.solid = static_cast<float>(pick(rng, 6));
    if(pick(rng, 5) == 0)
    {
        v.model = 0; // (no model: no dust)
    }
}

// The name of the field at float `ofs` of an edict (a vector's component: its name and the component).
void printField(int ofs)
{
    for(int i = 0; i < qcvm->progs->numfielddefs; i++)
    {
        const ddef_t& d = qcvm->fielddefs[i];
        const int type = d.type & ~DEF_SAVEGLOBAL;
        if(d.ofs == ofs || (type == ev_vector && ofs > d.ofs && ofs < d.ofs + 3))
        {
            Con_Printf("%s%s", PR_GetString(d.s_name), type == ev_vector ? (ofs == d.ofs ? "_x" : ofs == d.ofs + 1 ? "_y" : "_z") : "");
            return;
        }
    }
    Con_Printf("field %d", ofs);
}

// A float global of the progs by name (nullptr when it has none).
[[nodiscard]] float* globalFloat(const char* name)
{
    for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        const ddef_t& d = qcvm->globaldefs[i];
        if((d.type & ~DEF_SAVEGLOBAL) == ev_float && !strcmp(PR_GetString(d.s_name), name))
        {
            return &qcvm->globals[d.ofs];
        }
    }
    return nullptr;
}

struct Result
{
    za::Vector<unsigned char> fields;
    za::Vector<unsigned char> message;
    za::U64 nextRandom{0};
};

void copyInto(za::Vector<unsigned char>& out, const void* from, int bytes)
{
    out.resize(static_cast<za::SizeT>(bytes));
    if(bytes > 0)
    {
        memcpy(out.data(), from, static_cast<size_t>(bytes));
    }
}

// The edict, the next number of the server's stream and the broadcast written after a run.
void capture(edict_t* e, int bytes, Result& out)
{
    copyInto(out.fields, &e->v, bytes);
    copyInto(out.message, sv.datagram.data, sv.datagram.cursize);
    za::FastNonCryptoRng copy = srvrandom::stream();
    out.nextRandom = copy.next();
}

void check_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_propthink_check [states]: forcegrabbable_think_impl's QuakeC and native compared on each force-grabbable "
                   "thing in the map, as it is and in made-up states (default 200; a server)\n");
        return;
    }
    const int states = Cmd_Argc() > 1 ? q_max(0, Q_atoi(Cmd_Argv(1))) : 200;
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);

    const progs::FieldOffsets& f = progs::fields();
    const func_t qc = progs::findFunction("forcegrabbable_think_qc");
    const func_t thinks[] = {progs::findFunction("forcegrabbable_think"), progs::findFunction("forcegrabbable_item_think"),
        progs::findFunction("wpnthrow_think")};
    float* particlesOn = globalFloat("vr_weapondrop_particles_on"); // (what the QuakeC reads; set for each state)
    if(!qc || !haveFields(f) || !particlesOn)
    {
        Con_Printf("vr_propthink_check: the progs lack forcegrabbable_think_qc or a field\n");
        PR_PopQCVM(oldvm);
        return;
    }

    const int bytes = qcvm->progs->entityfields * 4;
    const int oldSelf = pr_global_struct->self;
    const float oldParticles = *particlesOn;
    const int oldCursize = sv.datagram.cursize;
    const za::FastNonCryptoRng oldStream = srvrandom::stream();
    za::FastNonCryptoRng made{7u};
    za::Vector<unsigned char> original;
    za::Vector<unsigned char> start;
    Result quakec;
    Result native;
    int things = 0, runs = 0, same = 0, differ = 0, declined = 0;

    for(int n = 1; n < qcvm->num_edicts; n++)
    {
        edict_t* e = EDICT_NUM(n);
        const func_t fn = e->free ? 0 : static_cast<func_t>(e->v.think);
        if(!fn || (fn != thinks[0] && fn != thinks[1] && fn != thinks[2]))
        {
            continue;
        }
        things++;
        unsigned char* p = reinterpret_cast<unsigned char*>(&e->v);
        copyInto(original, p, bytes);
        for(int s = 0; s <= states; s++)
        {
            memcpy(p, original.data(), static_cast<size_t>(bytes));
            float particles = oldParticles;
            if(s > 0)
            {
                makeUp(e, f, made);
                particles = pick(made, 4) == 0 ? 0.f : 1.f;
            }
            copyInto(start, p, bytes);
            const za::FastNonCryptoRng before = srvrandom::stream();

            // The QuakeC (as forcegrabbable_think_impl with vr_prop_think_native 0).
            sv.datagram.cursize = 0;
            pr_global_struct->self = EDICT_TO_PROG(e);
            *particlesOn = particles;
            PR_ExecuteProgram(qc);
            capture(e, bytes, quakec);

            // The native (and the QuakeC where it declines, as forcegrabbable_think_impl).
            memcpy(p, start.data(), static_cast<size_t>(bytes));
            VR_EdictIndex_Touch(e);
            srvrandom::setStream(before);
            sv.datagram.cursize = 0;
            pr_global_struct->self = EDICT_TO_PROG(e);
            if(!think(e, particles))
            {
                declined++;
                PR_ExecuteProgram(qc);
            }
            capture(e, bytes, native);
            srvrandom::setStream(before);
            runs++;

            const bool fieldsSame = !memcmp(quakec.fields.data(), native.fields.data(), static_cast<size_t>(bytes));
            const bool messageSame = quakec.message.size() == native.message.size() &&
                                     (quakec.message.empty() || !memcmp(quakec.message.data(), native.message.data(), quakec.message.size()));
            if(fieldsSame && messageSame && quakec.nextRandom == native.nextRandom)
            {
                same++;
                continue;
            }
            if(differ++ < 8)
            {
                Con_Printf("vr_propthink_check: entity %d (%s) state %d differs:", n, PR_GetString(e->v.classname), s);
                for(int i = 0; i < bytes / 4; i++)
                {
                    float a, b;
                    memcpy(&a, quakec.fields.data() + i * 4, 4);
                    memcpy(&b, native.fields.data() + i * 4, 4);
                    if(memcmp(&a, &b, 4))
                    {
                        Con_Printf(" ");
                        printField(i);
                        Con_Printf(" %g/%g", static_cast<double>(a), static_cast<double>(b));
                    }
                }
                Con_Printf("%s%s\n", messageSame ? "" : " (particle message)", quakec.nextRandom == native.nextRandom ? "" : " (random stream)");
            }
        }
        memcpy(p, original.data(), static_cast<size_t>(bytes));
        VR_EdictIndex_Touch(e);
    }

    srvrandom::setStream(oldStream);
    sv.datagram.cursize = oldCursize; // (the frame's broadcast, sent already: cleared as the next frame starts)
    pr_global_struct->self = oldSelf;
    *particlesOn = oldParticles;
    PR_PopQCVM(oldvm);

    Con_Printf("vr_propthink_check: %d things, %d states run: %d identical, %d differ (%d declined, the QuakeC ran); "
               "thinks since the last check: %lld native, %lld left to the QuakeC\n",
        things, runs, same, differ, declined, static_cast<long long>(counts.native), static_cast<long long>(counts.declined));
    counts = Counts{};
}

} // namespace

void PF_propthink()
{
    edict_t* self = PROG_TO_EDICT(pr_global_struct->self);
    bool done = false;
    if(vr_prop_think_native.value != 0.f && qcvm == &sv.qcvm && self != sv.qcvm.edicts) // (world: the QuakeC's error)
    {
        done = think(self, G_FLOAT(OFS_PARM0));
        (done ? counts.native : counts.declined)++;
    }
    G_FLOAT(OFS_RETURN) = done ? 1.f : 0.f;
}

void registerCommands()
{
    Cmd_AddCommand("vr_propthink_check", check_f);
}

} // namespace qvr::propthink
