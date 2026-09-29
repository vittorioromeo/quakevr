// vr_progs.cpp -- binds the Quake VR QuakeC entry points, globals and spawn parms.

#include "vr_hitmodel.hpp"
#include "vr_progs.hpp"
#include "vr_engine.hpp"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_debris.hpp"
#include "vr_ledges.hpp"
#include "vr_cvars.hpp"
#include "vr_physics.hpp"
#include "vr_server.hpp"
#include "vr_walltorch.hpp"
#include "vr_props.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace qvr::progs
{
namespace
{

Bindings sv_bindings;

// Level-start values of parm17..parm40, per client (parm1..parm16 live in client_t).
std::vector<float> extSpawnParms;

// Set while Host_Loadgame_f respawns the server, for `spawnServerFromSaveFile`.
bool loadingSaveGame = false;

[[nodiscard]] float* clientExtSpawnParms(int client)
{
    const std::size_t needed = (client + 1) * numExtSpawnParms;
    if(extSpawnParms.size() < needed)
    {
        extSpawnParms.resize(needed, 0.f);
    }

    return &extSpawnParms[client * numExtSpawnParms];
}

void callEntryPoint(func_t fn)
{
    if(!fn)
    {
        return;
    }

    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(qcvm->edicts);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    PR_ExecuteProgram(fn);
}

void callSpawnServerEntryPoint(func_t fn)
{
    if(sv_bindings.spawnServerFromSaveFile)
    {
        *sv_bindings.spawnServerFromSaveFile = loadingSaveGame ? 1.f : 0.f;
    }

    callEntryPoint(fn);
}

[[nodiscard]] ddef_t* findGlobalDef(const char* name)
{
    for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        ddef_t* def = &qcvm->globaldefs[i];
        if(!strcmp(PR_GetString(def->s_name), name))
        {
            return def;
        }
    }

    return nullptr;
}

} // namespace

const Bindings& bindings()
{
    return sv_bindings;
}

func_t findFunction(const char* name)
{
    for(int i = 0; i < qcvm->progs->numfunctions; i++)
    {
        if(!strcmp(PR_GetString(qcvm->functions[i].s_name), name))
        {
            return i;
        }
    }

    return 0;
}

} // namespace qvr::progs

using namespace qvr::progs;

extern "C" void VR_OnProgsLoaded()
{
    if(qcvm != &sv.qcvm)
    {
        return;
    }

    Bindings b;

    b.isVrProgs = ED_FindFieldOffset("handpos") >= 0;
    if(b.isVrProgs)
    {
#define QVR_FIELD(name) b.fields.name = ED_FindFieldOffset(#name);
#include "vr_fields.inc"
#undef QVR_FIELD

        b.OnSpawnServerBeforeLoad = findFunction("OnSpawnServerBeforeLoad");
        b.OnSpawnServerAfterLoad = findFunction("OnSpawnServerAfterLoad");
        b.OnLoadGame = findFunction("OnLoadGame");
        b.Motion_Sample = findFunction("VR_Motion_Sample");
        b.Motion_Equip = findFunction("VR_Motion_Equip");
        b.Dummy_Replay = findFunction("VR_Dummy_Replay");

        const auto globalFloat = [](const char* name) -> float* {
            ddef_t* def = findGlobalDef(name);
            if(!def || (def->type & ~DEF_SAVEGLOBAL) != ev_float)
            {
                return nullptr;
            }

            return &qcvm->globals[def->ofs];
        };

        b.spawnServerFromSaveFile = globalFloat("spawnServerFromSaveFile");
        for(int i = 0; i < numExtSpawnParms; i++)
        {
            b.extSpawnParms[i] = globalFloat(va("parm%d", firstExtSpawnParm + i));
        }

        bindBuiltins();

        // The VR protocol extends RMQ (see vr_protocol.hpp).
        sv.protocol = PROTOCOL_RMQ;
        sv.protocolflags |= PRFL_QUAKEVR | PRFL_QUAKEVR_PROGS;
        Con_DPrintf("VR: Quake VR progs detected\n");
    }
    else if(cls.state != ca_dedicated)
    {
        // Another mod's progs, on the player's own server: VR in compatibility mode. The client
        // renders, tracks and aims with its hand (the hand's angles are .v_angle); the engine
        // moves the player by the head, walks the room, teleports, and fires the mod's weapons
        // from the gun (vr_physics.cpp). A dedicated server keeps the plain protocol, for any client.
        sv.protocol = PROTOCOL_RMQ;
        sv.protocolflags |= PRFL_QUAKEVR;
        Con_DPrintf("VR: progs without Quake VR's gameplay: compatibility mode\n");
    }

    sv_bindings = b;
}

namespace
{

// The world reset: every server subsystem's state of the old world forgotten (a new map, a changelevel, a loaded game,
// a disconnect). What the server keeps across frames about entities is kept by entity number and forgotten here and
// when the entity is removed (onEdictFree); an entity number kept past a load would name another entity of the loaded
// world. A subsystem with such state adds its reset here.
void resetServerWorld()
{
    qvr::server::resetClients();
    qvr::physics::resetRigidBodies(); // (with the two-handed holds)
    qvr::box3d::reset();
    qvr::climb::reset();
    qvr::debris::reset();
    qvr::props::resetModelCache(); // (the models' names may be others' now)
    qvr::hitmodel::reset();
    resetBuiltinState();
}

} // namespace

extern "C" void VR_OnClearMemory()
{
    // Host_ClearMemory (SV_SpawnServer: every map and loaded game; CL_ClearState without a local server): the edicts,
    // cl_entities and the models are about to be freed with the hunk. The server's and the client's VR state of the old
    // world goes first, so that nothing keeps a pointer into the freed hunk (the client's again at the new serverinfo,
    // VR_OnClientClearState; the server's at VR_OnSpawnServerBeforeLoad).
    resetServerWorld();
    if(cls.state != ca_dedicated)
    {
        VR_OnClientClearState();
    }
}

extern "C" void VR_OnSpawnServerBeforeLoad()
{
    resetServerWorld();
    qvr::physics::precacheWaterSounds();
    // The training dummy's attacks (parry practice; QC vr_dummy.qc) are off at every map load, a saved game's too.
    Cvar_SetQuick(&qvr::vr_dummy_attacks, "0");
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerBeforeLoad);
}

extern "C" void VR_OnEdictFree(edict_t* ed)
{
    qvr::progs::onEdictFree(ed);
}

void qvr::progs::testRemove_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("vr_test_remove <entity number>: removes it, as QC's remove() (a server)\n");
        return;
    }
    const int num = Q_atoi(Cmd_Argv(1));
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    if(num > svs.maxclients && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
    {
        Con_Printf("vr_test_remove: entity %d (%s) removed\n", num, PR_GetString(EDICT_NUM(num)->v.classname));
        ED_Free(EDICT_NUM(num));
    }
    else
    {
        Con_Printf("vr_test_remove: no entity %d to remove\n", num);
    }
    PR_PopQCVM(oldvm);
}

extern "C" void VR_OnSpawnServerAfterLoad()
{
    qvr::server::onSpawnServerAfterLoad();
    qvr::debris::afterLoad();
    qvr::ledges::afterLoad(); // climbing's ledge map (with Climbing on)
    qvr::hitmodel::afterLoad(); // precise hits: every precached model's triangle hierarchy (no first-hit hitch)
    qvr::climb::reset();
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerAfterLoad);
    loadingSaveGame = false;
}

extern "C" void VR_OnBeginLoadGame()
{
    loadingSaveGame = true;
}

namespace
{

[[nodiscard]] int modelPrecacheIndex(const char* name)
{
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            return i;
        }
    }
    return -1;
}

// A saved game's entities whose models were precached late (setmodel of a model the map's spawn functions don't
// precache: the firing range's dispensed boxes, test spawns, wall torches): the loaded map's precache list lacks them, so
// the saved .modelindex names no model (the entity had no body, no collision model: an explosive box pulled by the
// grappling hook stood still) or, taken by another late precache since, another one. Such an entity's model is found
// again by its name, precached now if need be (the client, connecting after the load, gets the whole list). A saved
// index that names a precached model other than the entity's .model is left alone when that model is precached too
// (a deliberate index: the ring of shadows' eyes on the player's model).
void rebindLoadedModels()
{
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const int saved = static_cast<int>(ent->v.modelindex);
        if(ent->free || saved == 0 || !ent->v.model)
        {
            continue;
        }
        const char* name = PR_GetString(ent->v.model);
        const bool savedValid = saved > 0 && saved < MAX_MODELS && sv.model_precache[saved];
        if(!name[0] || (savedValid && !strcmp(sv.model_precache[saved], name)))
        {
            continue;
        }
        int index = modelPrecacheIndex(name);
        if(savedValid && index >= 0)
        {
            continue;
        }
        if(index < 0)
        {
            index = VR_LatePrecacheModel(name);
        }
        Con_DPrintf("load: entity %d's model %s: index %d -> %d\n", num, name, saved, std::max(index, 0));
        ent->v.modelindex = static_cast<float>(std::max(index, 0));
        SV_LinkEdict(ent, false);
    }
    // Everything precached while loading is in the serverinfo that clients receive (none is connected yet).
    qvr::server::onSpawnServerAfterLoad();
}

} // namespace

extern "C" void VR_OnLoadGame()
{
    loadingSaveGame = false;

    // parm17..parm40 were restored with the other globals and still hold the level-start
    // values (QC only rewrites them in SetNewParms/SetChangeParms), so they become the
    // single-player client's stored parms, like parm1..16 do from the savegame header.
    VR_StoreSpawnParms(0);

    // The saved edicts replaced the map's own, numbers and all: what SV_SpawnServer's two frames made for the map's
    // entities (Box3D's bodies, the free places and turns in the hand) would be taken for the loaded ones'. Built
    // again from the loaded edicts (Box3D at its next step; the world's mesh is cached).
    qvr::box3d::reset();
    qvr::physics::resetRigidBodies();

    rebindLoadedModels();

    qvr::walltorch::restoreAfterLoad(); // the map's wall torches a save made before they were entities lacks
    qvr::climb::reset();                // (holds on the loaded game's entities: none)

    callEntryPoint(sv_bindings.OnLoadGame);
}

extern "C" void VR_StoreSpawnParms(int client)
{
    float* stored = clientExtSpawnParms(client);
    for(int i = 0; i < numExtSpawnParms; i++)
    {
        stored[i] = sv_bindings.extSpawnParms[i] ? *sv_bindings.extSpawnParms[i] : 0.f;
    }
}

extern "C" void VR_RestoreSpawnParms(int client)
{
    const float* stored = clientExtSpawnParms(client);
    for(int i = 0; i < numExtSpawnParms; i++)
    {
        if(sv_bindings.extSpawnParms[i])
        {
            *sv_bindings.extSpawnParms[i] = stored[i];
        }
    }
}

extern "C" int VR_LatePrecacheModel(const char* name)
{
    if(!sv_bindings.isVrProgs)
    {
        return -1;
    }

    for(int i = 0; i < MAX_MODELS; i++)
    {
        if(!sv.model_precache[i])
        {
            // Clients learn about it from VR_ServerFrameEnd.
            sv.model_precache[i] = name;
            sv.models[i] = Mod_ForName(name, true);
            return i;
        }

        if(!strcmp(sv.model_precache[i], name))
        {
            return i;
        }
    }

    PR_RunError("VR_LatePrecacheModel: overflow");
    return -1;
}

extern "C" int VR_AllowLatePrecache()
{
    return sv_bindings.isVrProgs;
}
