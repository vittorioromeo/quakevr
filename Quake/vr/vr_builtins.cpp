// vr_builtins.cpp -- Quake VR QuakeC builtins.
//
// The QC declares these as `= #0`; after Ironwail's own by-name binding they are still
// unbound, so they are given numbers from a private range here and bound by name.

#include "vr_progs.hpp"
#include "vr_box3d.hpp"
#include "vr_carry2h.hpp"
#include "vr_debris.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_motion.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_ropesim.hpp"
#include "vr_server.hpp"
#include "vr_worldtext.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"
#include "vr_weight.hpp"

#include <algorithm>
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

// Where the force grab takes an entity, in the world (vector(entity e) forcegrabpoint): a weapon by its handle, its
// origin (weapons::heldAtOrigin: it arrives in the hand as it is held), anything else by its drawn middle.
void PF_forcegrabpoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const glm::vec3 c = weapons::heldAtOrigin(model) ? glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]}
                                                    : physics::modelCentre(e);
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
// Files

// float(string path) fileexists: whether a file is in the game's search path. Precaching a missing
// model is an error, so the QC checks first for data that may not be installed (the mission packs'
// monsters: vrfiringrange's monster buttons).
void PF_fileexists()
{
    G_FLOAT(OFS_RETURN) = COM_FileExists(G_STRING(OFS_PARM0), nullptr) ? 1.f : 0.f;
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

// A wound on a model (vr_wounds.cpp): void(entity e, vector org, vector dir, float kind, float amount, float extra)
// woundevent. `e` hit at `org` going `dir` (unit), QVR_WOUND_* `kind`, `amount` (damage, 0..255), `extra` (pellets;
// a liquid's: its surface's height is org_z). Unreliable, to every client, as particles.
// The entities sent a wound this map: their removal is sent (onEdictFree).
std::vector<std::uint8_t> woundsSent;

void PF_woundevent()
{
    const int num = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const float* org = G_VECTOR(OFS_PARM1);
    const float* dir = G_VECTOR(OFS_PARM2);
    const int kind = static_cast<int>(G_FLOAT(OFS_PARM3));
    const int amount = static_cast<int>(G_FLOAT(OFS_PARM4) + 0.5f);
    const int extra = static_cast<int>(G_FLOAT(OFS_PARM5));

    if(sv.state != ss_active || sv.datagram.cursize > MAX_DATAGRAM - 32)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_WOUND);
    MSG_WriteShort(&sv.datagram, num);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, org[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-127, static_cast<int>(dir[i] * 127.f), 127));
    }
    MSG_WriteByte(&sv.datagram, CLAMP(0, kind, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, amount, 255));
    MSG_WriteByte(&sv.datagram, CLAMP(0, extra, 255));
    if(num >= static_cast<int>(woundsSent.size()))
    {
        woundsSent.resize(static_cast<std::size_t>(num) + 1, 0);
    }
    woundsSent[static_cast<std::size_t>(num)] = 1;
}

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

// entity findflags(entity start, .float field, float flags): the next entity after `start` (in edict order, free ones
// skipped) whose `field` has any of `flags` set; world when there is none (DP's extension). The frame's loops over the
// monsters (VR_Liquids_Frame, VR_Wounds_Frame) step through them alone, instead of every entity in the VM.
void PF_findflags()
{
    const int from = NUM_FOR_EDICT(G_EDICT(OFS_PARM0));
    const int field = G_INT(OFS_PARM1);
    const int flags = static_cast<int>(G_FLOAT(OFS_PARM2));
    for(int i = from + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && (static_cast<int>(E_FLOAT(e, field)) & flags))
        {
            G_INT(OFS_RETURN) = EDICT_TO_PROG(e);
            return;
        }
    }
    G_INT(OFS_RETURN) = EDICT_TO_PROG(qcvm->edicts);
}

// vector liquidentry(vector start, vector end): where the segment first goes into water, slime or
// lava from the open (or out of it: a shot from under water), on the surface; `start` itself if it
// crosses none. Leaves the trace globals alone (a shot's trace is still being used).
void PF_liquidentry()
{
    const float* start = G_VECTOR(OFS_PARM0);
    const float* end = G_VECTOR(OFS_PARM1);
    glm::vec3 at{start[0], start[1], start[2]};
    const bool crossed = physics::liquidEntry(at, {end[0], end[1], end[2]}, at);
    if(developer.value >= 3)
    {
        Con_Printf("liquidentry %.0f %.0f %.0f -> %.0f %.0f %.0f: %s\n", start[0], start[1], start[2], end[0], end[1], end[2],
            crossed ? "crosses" : "no");
    }
    float* out = G_VECTOR(OFS_RETURN);
    for(int i = 0; i < 3; i++)
    {
        out[i] = crossed ? at[i] : start[i];
    }
}

// void watersplash(vector org, vector dir, float strength, float sound): a splash on a liquid's
// surface at `org`, something going `dir` into it `strength` hard (4 a shot .. 50 a body), and its
// sound (QVR_SPLASH_*: none, a shot's plip, a thing's splash by strength).
void PF_watersplash()
{
    const float* org = G_VECTOR(OFS_PARM0);
    const float* dir = G_VECTOR(OFS_PARM1);
    physics::waterSplash({org[0], org[1], org[2]}, {dir[0], dir[1], dir[2]}, G_FLOAT(OFS_PARM2),
        static_cast<physics::SplashSound>(CLAMP(0, static_cast<int>(G_FLOAT(OFS_PARM3)), 2)));
}

// haptic(hand, delay, duration, frequency, amplitude), for the `self` player.
void PF_haptic()
{
    server::sendHaptic(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2), G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4));
}

// carryangles(e, handangles, grab): a held object's angles, turning with the hand (vr_rigid.cpp). At a grab (a regrip:
// VR_Carry_Regrip) its place in the hand is kept as it is now (its .carry_offset set before), and its grip's settings
// still move it from there (vr_grip.hpp).
void PF_carryangles()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const bool grab = static_cast<int>(G_FLOAT(OFS_PARM2)) != 0;
    physics::carryAngles(e, G_VECTOR(OFS_PARM1), grab, G_VECTOR(OFS_RETURN));
    const FieldOffsets& f = fields();
    if(grab && f.carry_offset >= 0)
    {
        grip::serverKeep(e, G_VECTOR(OFS_PARM1), fieldVec(e, f.carry_offset));
    }
}

// vector(entity e, entity player, float hand, float lefthand, vector handangles, vector offset) carrygrip: as `player`'s
// `hand` (cVR_MainHand or cVR_OffHand; lefthand: the left one) at `handangles` takes `e`, whose origin is at `offset` in
// the hand (forward, right, up: where it was taken, as .carry_offset): its place in the hand by its Held Object Offsets'
// grip (vr_grip.hpp): its turn with the hand kept (its angles set now) and its origin's place returned (.carry_offset).
void PF_carrygrip()
{
    const float* o = G_VECTOR(OFS_PARM5);
    const glm::vec3 v = grip::serverTake(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)),
        G_FLOAT(OFS_PARM3) != 0.f, G_VECTOR(OFS_PARM4), glm::vec3{o[0], o[1], o[2]});
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handangles, vector offset) carryplace: each frame `e` is held in one hand: its angles set
// (turning with the hand) and its origin's place in the hand returned (.carry_offset, given as `offset`): placed again
// at once when its Held Object Offsets changed (vr_grip.hpp).
void PF_carryplace()
{
    const float* o = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = grip::serverFrame(G_EDICT(OFS_PARM0), G_VECTOR(OFS_PARM1), glm::vec3{o[0], o[1], o[2]});
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, vector handpos, vector palm) carryfit: how far to move an object a hand grips so
// that it sits against the palm, not sunk into the fist (vr_held.cpp; vr_held_surface_fit).
void PF_carryfit()
{
    const float* h = G_VECTOR(OFS_PARM1);
    const float* p = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = held::surfaceFit(G_EDICT(OFS_PARM0), glm::vec3{h[0], h[1], h[2]}, glm::vec3{p[0], p[1], p[2]});
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// vector(entity e, entity player, float grab) carry2h: a prop held in both hands (vr_carry2h.cpp): at the second
// hand's grip (grab 1) the grips are kept and its origin returned; after, where it goes (its angles set at once).
void PF_carry2h()
{
    const glm::vec3 v = carry2h::serverPlace(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), G_FLOAT(OFS_PARM2) != 0.f);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// float(entity e, entity player) carry2hoff: which hands of `player` have been pulled off `e`, held in both (bit 1 the off
// hand, 2 the main hand; vr_carry2h.cpp detached).
void PF_carry2hoff()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(carry2h::detached(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1)));
}

// float(entity e, vector point, float hand) carryreach: whether the `self` player's `hand` can take hold of `e` (its fist
// touches its drawn surface, as a hand touching it could), for a carried one (not solid).
// physicsblast(at, damage): T_RadiusDamage's explosion throws the rigid bodies round it (vr_box3d.cpp; Box3D only).
void PF_physicsblast()
{
    const float* p = G_VECTOR(OFS_PARM0);
    box3d::blast(glm::vec3{p[0], p[1], p[2]}, G_FLOAT(OFS_PARM1));
}

// float(entity e, string key) propvalue: a setting of the prop `e` is (Held Object Offsets, vr_props.inc, by its
// model): "mass" its mass in kg (the setting, else Box3D's), "throw" how much of the hand's throw it keeps (the
// setting, else from its mass), any other key its value (its default for a model with no settings; 0 for no key).
void PF_propvalue()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const char* name = G_STRING(OFS_PARM1);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model ? props::slotForModel(model) : -1;
    const props::Key key = props::keyByName(name);
    float out = 0.f;
    if(key == props::Key::Mass)
    {
        out = box3d::propMass(e);
    }
    else if(key == props::Key::Throw)
    {
        out = props::throwScale(slot, box3d::propMass(e));
    }
    else if(key != props::Key::Count && key != props::Key::ID)
    {
        out = props::value(slot, key);
    }
    else
    {
        Con_DPrintf("propvalue: no key \"%s\"\n", name);
    }
    G_FLOAT(OFS_RETURN) = out;
}

// float(string model, string key) weaponvalue: a setting of the weapon drawn with `model` ("progs/v_rock2.mdl"; Weapon
// Offsets and Weapon Weights, vr_weapons.inc, as its cvars name it: "w_mass" kg, "w_meleedmg", "w_throwdmg"...), what
// it inherits followed (0: no such weapon or key).
void PF_weaponvalue()
{
    const int slot = weapons::slotForName(G_STRING(OFS_PARM0));
    const weapons::Key key = weapons::keyByName(G_STRING(OFS_PARM1));
    G_FLOAT(OFS_RETURN) = slot >= 0 && key != weapons::Key::Count ? weapons::value(slot, key) : 0.f;
}

// float(float mass) weightdamage: the damage multiplier of a melee blow or a throw by a thing of `mass` kg (weight and
// damage, vr_weight_damage_*; 1 for no mass). float(float mass) weightleniency: the factor on the speed thresholds of
// its strikes and throws (heavy leniency, vr_weight_lenient*; 1 for anything up to vr_weight_lenient_from).
void PF_weightdamage()
{
    G_FLOAT(OFS_RETURN) = weight::damageMultiplier(G_FLOAT(OFS_PARM0));
}

void PF_weightleniency()
{
    G_FLOAT(OFS_RETURN) = weight::leniency(G_FLOAT(OFS_PARM0));
}

// vector(entity e, vector handangles, float lefthand) propgrip: a prop held the same way every time (Grip Mode 1): from
// now on it turns with the hand as its Grip Pitch, Yaw and Roll say (its angles set now), and its origin's place in
// the hand is returned (forward, right, up, as .carry_offset: Grip X, Y (left) and Z; the left hand's mirrored).
void PF_propgrip()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* handAngles = G_VECTOR(OFS_PARM1);
    const bool left = G_FLOAT(OFS_PARM2) != 0.f;
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model ? props::slotForModel(model) : -1;
    using props::Key;
    glm::vec3 place{props::value(slot, Key::GripX), props::value(slot, Key::GripY), props::value(slot, Key::GripZ)};
    glm::vec3 turn{props::value(slot, Key::GripPitch), props::value(slot, Key::GripYaw), props::value(slot, Key::GripRoll)};
    if(left)
    {
        place.y = -place.y;
        turn.y = -turn.y;
        turn.z = -turn.z;
    }
    // Pitch up for every model (vr_grip.cpp's fixed grip; carrygrip is what the carry uses now).
    const float angles[3]{-turn.x, turn.y, turn.z};
    physics::setCarryTurn(e, handAngles, held::axesFromAngles(angles, true));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = place.x;
    out[1] = -place.y;
    out[2] = place.z;
}

// vector(entity e, vector angles, vector point) modelpoint: where the point `point` of `e`'s model (units in its model's
// axes: x forward, y left, z up) is from its origin when it is turned by `angles` (as the renderer turns it: an alias
// model's pitch is inverted) and scaled as it is drawn. The melee's line of a carried club (a wall torch's head, a
// brick's end: vr_melee.qc).
void PF_modelpoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* angles = G_VECTOR(OFS_PARM1);
    const float* p = G_VECTOR(OFS_PARM2);
    const int index = static_cast<int>(e->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    // Scaled as it is drawn: the networked scale (an offset from 1) about its origin (a rock's or a brick's size,
    // vr_debris.cpp); an unscaled model (a wall torch) as it is.
    const FieldOffsets& f = fields();
    const glm::vec3 scale = fieldVec(e, f.model_scale), about = fieldVec(e, f.model_scale_origin);
    const glm::vec3 point = (glm::vec3{p[0], p[1], p[2]} - about) * (glm::vec3{1.f} + scale) + about;
    const glm::vec3 v = held::axesFromAngles(angles, model && model->type == mod_brush) * point;
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// A push of a hand on a prop (physicspush(e, at, velocity)): the point `at` of the rigid body `e` gets at least the
// velocity's speed along it (Box3D: an impulse there, so a tall box pushed high tips over; vr_box3d.cpp). False if it is
// not Box3D's.
void PF_physicspush()
{
    const float* at = G_VECTOR(OFS_PARM1);
    const float* v = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = box3d::push(G_EDICT(OFS_PARM0), glm::vec3{at[0], at[1], at[2]}, glm::vec3{v[0], v[1], v[2]}) ? 1.f : 0.f;
}

// The grappling hook's physical rope (vr_ropesim.cpp).
// float ropestep(entity hook, vector gun, vector game, vector end, float length, entity skipA, entity skipB): steps it
// (once a frame), the taut path's length.
void PF_ropestep()
{
    const auto vec = [](int ofs) {
        const float* v = G_VECTOR(ofs);
        return glm::vec3{v[0], v[1], v[2]};
    };
    const int skipA = NUM_FOR_EDICT(G_EDICT(OFS_PARM5));
    const int skipB = NUM_FOR_EDICT(G_EDICT(OFS_PARM6));
    G_FLOAT(OFS_RETURN) =
        ropesim::step(G_EDICT(OFS_PARM0), vec(OFS_PARM1), vec(OFS_PARM2), vec(OFS_PARM3), G_FLOAT(OFS_PARM4), skipA, skipB).path;
}

// vector ropepivot(entity hook, float which): the taut path's first corner from the game's end (0), from the hook (1), or
// (2) its length beyond the first, beyond the second, and in all.
void PF_ropepivot()
{
    const ropesim::Shape& s = ropesim::shape(G_EDICT(OFS_PARM0));
    const int which = static_cast<int>(G_FLOAT(OFS_PARM1));
    const glm::vec3 v = which == 0 ? s.pivotA : which == 1 ? s.pivotB : glm::vec3{s.beyondA, s.beyondB, s.path};
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// void ropesend(entity hook, entity owner, float beamId): its points to the clients, for that beam.
void PF_ropesend()
{
    ropesim::send(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)));
}

void PF_carryreach()
{
    // The hand's place (the point given, OFS_PARM1) and angles are the `self` player's move's.
    G_FLOAT(OFS_RETURN) =
        carry2h::reaches(G_EDICT(OFS_PARM0), PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM2))) ? 1.f : 0.f;
}

// handimpact(hand, strength, dir): knock the `self` player's drawn hand (a parried blow).
void PF_handimpact()
{
    server::sendHandImpact(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1),
        G_VECTOR(OFS_PARM2));
}

// catchblend(hand, e, org, ang): the `self` player's `hand` has just caught `e` (a force grab), which was at `org`
// turned `ang`: its client eases the weapon from there into the hand (vr_drawblend.cpp).
void PF_catchblend()
{
    server::sendCatchBlend(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        NUM_FOR_EDICT(G_EDICT(OFS_PARM1)), G_VECTOR(OFS_PARM2), G_VECTOR(OFS_PARM3));
}

// ejectcasings(hand, kind, count, delay, flags): spent casings out of the `self` player's weapon.
void PF_ejectcasings()
{
    server::sendEject(PROG_TO_EDICT(pr_global_struct->self), static_cast<int>(G_FLOAT(OFS_PARM0)),
        static_cast<int>(G_FLOAT(OFS_PARM1)), static_cast<int>(G_FLOAT(OFS_PARM2)), static_cast<int>(G_FLOAT(OFS_PARM4)),
        G_FLOAT(OFS_PARM3));
}

// ----------------------------------------------------------------------------
// The motion recorder (vr_motion.cpp; QC vr_motion.qc): kept only while a take is recorded or played.

// void(string kind, string sub, float hand, float value, vector at, entity targ, string detail) motionevent
void PF_motionevent()
{
    motion::qcEvent(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)), G_FLOAT(OFS_PARM3),
        G_VECTOR(OFS_PARM4), G_EDICT(OFS_PARM5), G_STRING(OFS_PARM6));
}

// void(float hand, vector at, string name) motionpoint
void PF_motionpoint()
{
    motion::qcPoint(static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_PARM1), G_STRING(OFS_PARM2));
}

// void(string key, vector value) motionvalue
void PF_motionvalue()
{
    motion::qcValue(G_STRING(OFS_PARM0), G_VECTOR(OFS_PARM1));
}

// Round 21: the weapons' hotspots (where the other hand may hold them; vr_weapons.hpp), as the local player's view
// draws them this frame. vector(entity player, float hand, float index) weaponhotspot: hotspot `index` (0..3) of the
// weapon in `hand` (cVR_MainHand, cVR_OffHand): a grip's point, a blade grip's middle, in the world ('0 0 0' if none);
// float(entity player, float hand, float index, float what) weaponhotspotinfo: its type (what 0: 0 none, 1 grip,
// 2 blade), bias in units (1), a blade's share of the way from the hand to the tip (2). Only the local player's (the
// server's first client, as its view draws it); others' are none.
[[nodiscard]] view::WeaponHotspot localHotspot()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    if(!sv.active || cls.state != ca_connected || NUM_FOR_EDICT(player) != 1)
    {
        return {};
    }
    return view::weaponHotspot(static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1, static_cast<int>(G_FLOAT(OFS_PARM2)));
}

void PF_weaponhotspot()
{
    const view::WeaponHotspot h = localHotspot();
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = h.pos.x;
    out[1] = h.pos.y;
    out[2] = h.pos.z;
}

void PF_weaponhotspotinfo()
{
    const view::WeaponHotspot h = localHotspot();
    const int what = static_cast<int>(G_FLOAT(OFS_PARM3));
    G_FLOAT(OFS_RETURN) = what == 0 ? static_cast<float>(h.type) : what == 1 ? h.bias : h.share;
}

// Rocks and bricks lying about (vr_debris.cpp, QC vr_debris.qc). float() debrisplan: places them (after the map's
// entities), how many; string(float i) debrismodel: piece i's model (to set); float(entity e, float i) debrisput: puts
// piece i on e (its model set): its skin, size, turn, place resting on the floor and box; its kind (1 a rock, 2 a
// brick; 0 none).
void PF_debrisplan()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::plan());
}

void PF_debrismodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(debris::modelOf(static_cast<int>(G_FLOAT(OFS_PARM0))));
}

void PF_debrisput()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::put(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// Precise hit detection (vr_hitmodel.cpp). hitmodel_target(e): whether e's model is what is hit (the option on, a
// monster or corpse with a Quake model).
void PF_hitmodel_target()
{
    G_FLOAT(OFS_RETURN) = hitmodel::target(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

// hitmodel_segment(e, a, b, radius, class): the segment a..b, a sphere of radius `radius` plus the class's tolerance
// (0 guns, 1 grapple, 2 melee, 3 thrown), against e's model as drawn: where along it it first meets it (0: it starts
// in it), -1 missed; -2: e's model is not tested (the option off, not a target): its box as before. The point on the
// model: hitmodel_rest's (the last hit).
void PF_hitmodel_segment()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    const float radius = std::max(0.f, G_FLOAT(OFS_PARM3));
    const auto c = static_cast<hitmodel::Class>(std::clamp(static_cast<int>(G_FLOAT(OFS_PARM4)), 0, 3));
    if(!hitmodel::target(e))
    {
        G_FLOAT(OFS_RETURN) = -2.f;
        return;
    }
    hitmodel::Hit h;
    const bool hit = hitmodel::segment(e, glm::vec3{a[0], a[1], a[2]}, glm::vec3{b[0], b[1], b[2]}, radius + hitmodel::tolerance(c), 1.f, c, h);
    G_FLOAT(OFS_RETURN) = hit ? h.t : -1.f;
}

// hitmodel_rest(e, p): the point p on e's drawn model, as it is on the model standing (its stand frame), placed at its
// origin and turned with its yaw (positional damage's head sphere and regions); p itself if e is no target.
void PF_hitmodel_rest()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* p = G_VECTOR(OFS_PARM1);
    glm::vec3 out{p[0], p[1], p[2]};
    hitmodel::restPoint(e, out, out);
    float* r = G_VECTOR(OFS_RETURN);
    r[0] = out.x;
    r[1] = out.y;
    r[2] = out.z;
}

struct VrBuiltin
{
    const char* name;
    builtin_t func;
};

constexpr VrBuiltin vrBuiltins[] = {
    {"makeforward", PF_makeforward},
    {"weaponhotspot", PF_weaponhotspot},
    {"weaponhotspotinfo", PF_weaponhotspotinfo},
    {"modelbounds", PF_modelbounds},
    {"modelcentre", PF_modelcentre},
    {"forcegrabpoint", PF_forcegrabpoint},
    {"catchblend", PF_catchblend},
    {"physicsblast", PF_physicsblast},
    {"propvalue", PF_propvalue},
    {"propgrip", PF_propgrip},
    {"weaponvalue", PF_weaponvalue},
    {"weightdamage", PF_weightdamage},
    {"weightleniency", PF_weightleniency},
    {"debrisplan", PF_debrisplan},
    {"debrismodel", PF_debrismodel},
    {"debrisput", PF_debrisput},
    {"modelpoint", PF_modelpoint},
    {"physicspush", PF_physicspush},
    {"ropestep", PF_ropestep},
    {"ropepivot", PF_ropepivot},
    {"ropesend", PF_ropesend},
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
    {"woundevent", PF_woundevent},
    {"haptic", PF_haptic},
    {"handimpact", PF_handimpact},
    {"carryangles", PF_carryangles},
    {"carrygrip", PF_carrygrip},
    {"carryplace", PF_carryplace},
    {"carryfit", PF_carryfit},
    {"carry2h", PF_carry2h},
    {"carryreach", PF_carryreach},
    {"carry2hoff", PF_carry2hoff},
    {"floattext", PF_floattext},
    {"ejectcasings", PF_ejectcasings},
    {"findflags", PF_findflags},
    {"liquidentry", PF_liquidentry},
    {"watersplash", PF_watersplash},
    {"fileexists", PF_fileexists},
    {"motionevent", PF_motionevent},
    {"motionpoint", PF_motionpoint},
    {"motionvalue", PF_motionvalue},
    {"hitmodel_target", PF_hitmodel_target},
    {"hitmodel_segment", PF_hitmodel_segment},
    {"hitmodel_rest", PF_hitmodel_rest},
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

void onEdictFree(edict_t* ed)
{
    // An entity removed: what the server keeps by its number is forgotten (physics::forgetEntity), and for a wounded one
    // the clients free its mask, else the next entity put in its slot (edicts are reused), with the same model (a
    // monster from the same spawner, a gib), would show its wounds. Reliable: once, at its end.
    if(qcvm != &sv.qcvm || !sv.active)
    {
        return;
    }
    // Checked: ED_Free runs for any edict, one a saved game's parse frees among them.
    const int num = NUM_FOR_EDICT_CHECKED(ed);
    if(num > 0)
    {
        physics::forgetEntity(num);
        ropesim::forget(num);
    }
    if(num <= 0 || num >= static_cast<int>(woundsSent.size()) || !woundsSent[static_cast<std::size_t>(num)])
    {
        return;
    }
    woundsSent[static_cast<std::size_t>(num)] = 0;
    if(sv.reliable_datagram.cursize + 4 > sv.reliable_datagram.maxsize)
    {
        return;
    }
    MSG_WriteByte(&sv.reliable_datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.reliable_datagram, protocol::QVR_SVC_WOUNDCLEAR);
    MSG_WriteShort(&sv.reliable_datagram, num);
}

void resetBuiltinState()
{
    cvarHandles.clear();
    woundsSent.clear();
    worldtext::serverReset();
}

} // namespace qvr::progs
