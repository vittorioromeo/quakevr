// vr_builtins.cpp -- Quake VR QuakeC builtins.
//
// The QC declares these as `= #0`; after Ironwail's own by-name binding they are still
// unbound, so they are given numbers from a private range here and bound by name.

#include "vr_progs.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_worldtext.hpp"

#include <vector>

namespace qvr::progs
{
namespace
{

// Ironwail numbers its unnumbered builtins downwards from MAX_BUILTINS - 2.
constexpr int firstVrBuiltin = 1000;

// ----------------------------------------------------------------------------
// Math and vectors

void PF_makeforward()
{
    vec3_t right, up;
    AngleVectors(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, right, up);
}

// The bounds of an entity's model (its mins with `max` 0, maxs otherwise), in model space:
// vector(entity e, float max) modelbounds.
void PF_modelbounds()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    const bool max = G_FLOAT(OFS_PARM1) != 0.f;
    float* out = G_VECTOR(OFS_RETURN);
    VectorCopy(vec3_origin, out);

    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(model)
    {
        VectorCopy(max ? model->maxs : model->mins, out);
    }
}

// The middle of an entity's drawn model, turned with it, in the world (vr_rigid.cpp): vector(entity
// e) modelcentre. Dropped weapons and backpacks are drawn well off their box's middle.
void PF_modelcentre()
{
    const glm::vec3 c = physics::modelCentre(G_EDICT(OFS_PARM0));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = c.x;
    out[1] = c.y;
    out[2] = c.z;
}

// void(vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity forent) tracebox:
// traceline with a box (the engine's own collision, hulls and all); sets the trace_ globals.
void PF_tracebox()
{
    const trace_t trace = SV_Move(G_VECTOR(OFS_PARM0), G_VECTOR(OFS_PARM1), G_VECTOR(OFS_PARM2),
        G_VECTOR(OFS_PARM3), static_cast<int>(G_FLOAT(OFS_PARM4)), G_EDICT(OFS_PARM5));

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    VectorCopy(trace.endpos, pr_global_struct->trace_endpos);
    VectorCopy(trace.plane.normal, pr_global_struct->trace_plane_normal);
    pr_global_struct->trace_plane_dist = trace.plane.dist;
    edict_t* hit = trace.ent ? trace.ent : qcvm->edicts;
    pr_global_struct->trace_ent = EDICT_TO_PROG(hit);
}

// ----------------------------------------------------------------------------
// Cvar handles: QC resolves cvar names once per map, then reads them by index.

std::vector<cvar_t*> cvarHandles;

[[nodiscard]] cvar_t* cvarFromHandle(float handle)
{
    const int i = static_cast<int>(handle);
    if(i < 0 || i >= static_cast<int>(cvarHandles.size()))
    {
        Con_DPrintf("VR: invalid cvar handle %d\n", i);
        return nullptr;
    }

    return cvarHandles[i];
}

void PF_cvar_hmake()
{
    const char* name = G_STRING(OFS_PARM0);
    cvar_t* var = Cvar_FindVar(name);
    if(!var)
    {
        Con_Printf("VR: cvar_hmake: unknown cvar \"%s\"\n", name);
        G_FLOAT(OFS_RETURN) = -1.f;
        return;
    }

    cvarHandles.push_back(var);
    G_FLOAT(OFS_RETURN) = static_cast<float>(cvarHandles.size() - 1);
}

void PF_cvar_hget()
{
    cvar_t* var = cvarFromHandle(G_FLOAT(OFS_PARM0));
    G_FLOAT(OFS_RETURN) = var ? var->value : 0.f;
}

void PF_cvar_hclear()
{
    cvarHandles.clear();
}

// ----------------------------------------------------------------------------
// World text (see vr_worldtext.hpp)

[[nodiscard]] int worldTextHandle()
{
    return static_cast<int>(G_FLOAT(OFS_PARM0));
}

[[nodiscard]] glm::vec3 vecParm1()
{
    const float* v = G_VECTOR(OFS_PARM1);
    return {v[0], v[1], v[2]};
}

void PF_worldtext_hmake()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(worldtext::serverMake());
}

void PF_worldtext_hsettext()
{
    worldtext::serverSetText(worldTextHandle(), G_STRING(OFS_PARM1));
}

void PF_worldtext_hsetpos()
{
    worldtext::serverSetPos(worldTextHandle(), vecParm1());
}

void PF_worldtext_hsetangles()
{
    worldtext::serverSetAngles(worldTextHandle(), vecParm1());
}

void PF_worldtext_hsethalign()
{
    worldtext::serverSetHAlign(
        worldTextHandle(), static_cast<worldtext::HAlign>(static_cast<int>(G_FLOAT(OFS_PARM1))));
}

void PF_worldtext_hsetscale()
{
    worldtext::serverSetScale(worldTextHandle(), G_FLOAT(OFS_PARM1));
}

// floattext(origin, text, colour, scale): a text that rises from `origin` and fades, facing
// whoever looks (a damage number), shown to every client.
void PF_floattext()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float* color = G_VECTOR(OFS_PARM2);
    worldtext::serverFloatText({org[0], org[1], org[2]}, G_STRING(OFS_PARM1), {color[0], color[1], color[2]},
        G_FLOAT(OFS_PARM3));
}

// ----------------------------------------------------------------------------
// Messages

// Same destinations as Ironwail's (static) WriteDest in pr_cmds.c.
[[nodiscard]] sizebuf_t* writeDest()
{
    enum
    {
        MSG_BROADCAST = 0,
        MSG_ONE = 1,
        MSG_ALL = 2,
        MSG_INIT = 3
    };

    switch(static_cast<int>(G_FLOAT(OFS_PARM0)))
    {
        case MSG_BROADCAST: return &sv.datagram;
        case MSG_ONE:
        {
            edict_t* ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
            const int entnum = NUM_FOR_EDICT(ent);
            if(entnum < 1 || entnum > svs.maxclients)
            {
                PR_RunError("WriteDest: not a client");
            }
            return &svs.clients[entnum - 1].message;
        }
        case MSG_ALL: return &sv.reliable_datagram;
        case MSG_INIT: return sv.signon;
        default: PR_RunError("WriteDest: bad destination"); return nullptr;
    }
}

void PF_WriteVec3()
{
    sizebuf_t* dest = writeDest();
    const float* v = G_VECTOR(OFS_PARM1);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(dest, v[i], sv.protocolflags);
    }
}

// ----------------------------------------------------------------------------
// Effects and the player's hands

// particle2(origin, direction, preset, count): unreliable, like vanilla particle().
void PF_particle2()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float* dir = G_VECTOR(OFS_PARM1);
    const int preset = static_cast<int>(G_FLOAT(OFS_PARM2));
    const int count = static_cast<int>(G_FLOAT(OFS_PARM3));

    if(sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_PARTICLE2);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, org[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-128, static_cast<int>(dir[i] * 16.f), 127));
    }
    MSG_WriteByte(&sv.datagram, preset);
    MSG_WriteShort(&sv.datagram, count);
}

// haptic(hand, delay, duration, frequency, amplitude), for the `self` player.
void PF_haptic()
{
    server::sendHaptic(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

// carryangles(e, handangles, grab): a held object's angles, turning with the hand (vr_rigid.cpp).
void PF_carryangles()
{
    physics::carryAngles(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)) != 0,
        G_VECTOR(OFS_RETURN));
}

// handimpact(hand, strength, dir): knock the `self` player's drawn hand (a parried blow).
void PF_handimpact()
{
    server::sendHandImpact(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1),
        G_VECTOR(OFS_PARM2));
}

struct VrBuiltin
{
    const char* name;
    builtin_t func;
};

constexpr VrBuiltin vrBuiltins[] = {
    {"makeforward", PF_makeforward},
    {"modelbounds", PF_modelbounds},
    {"modelcentre", PF_modelcentre},
    {"tracebox", PF_tracebox},
    {"cvar_hmake", PF_cvar_hmake},
    {"cvar_hget", PF_cvar_hget},
    {"cvar_hclear", PF_cvar_hclear},
    {"worldtext_hmake", PF_worldtext_hmake},
    {"worldtext_hsettext", PF_worldtext_hsettext},
    {"worldtext_hsetpos", PF_worldtext_hsetpos},
    {"worldtext_hsetangles", PF_worldtext_hsetangles},
    {"worldtext_hsethalign", PF_worldtext_hsethalign},
    {"worldtext_hsetscale", PF_worldtext_hsetscale},
    {"WriteVec3", PF_WriteVec3},
    {"particle2", PF_particle2},
    {"haptic", PF_haptic},
    {"handimpact", PF_handimpact},
    {"carryangles", PF_carryangles},
    {"floattext", PF_floattext},
};

static_assert(firstVrBuiltin + std::size(vrBuiltins) < MAX_BUILTINS - 200,
    "VR builtins must not overlap Ironwail's downward-allocated builtins");

} // namespace

void bindBuiltins()
{
    int number = firstVrBuiltin;
    for(const VrBuiltin& b : vrBuiltins)
    {
        if(qcvm->builtins[number] != qcvm->builtins[0])
        {
            Sys_Error("VR builtin #%d (%s) collides with an engine builtin", number, b.name);
        }

        qcvm->builtins[number] = b.func;
        qcvm->builtin_ext[number] = STD_QC;

        if(const func_t f = findFunction(b.name))
        {
            dfunction_t& fn = qcvm->functions[f];
            if(fn.first_statement == 0)
            {
                fn.first_statement = -number;
            }
        }

        number++;
    }
}

void resetBuiltinState()
{
    cvarHandles.clear();
    worldtext::serverReset();
}

} // namespace qvr::progs
