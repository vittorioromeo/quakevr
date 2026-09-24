// vr_progs.cpp -- binds the Quake VR QuakeC entry points, globals and spawn parms.

#include "vr_progs.hpp"
#include "vr_server.hpp"

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

} // namespace

const Bindings& bindings()
{
    return sv_bindings;
}

ddef_t* findGlobalDef(const char* name)
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
        sv.protocolflags |= PRFL_QUAKEVR;
        Con_DPrintf("VR: Quake VR progs detected\n");
    }

    sv_bindings = b;
}

extern "C" void VR_OnSpawnServerBeforeLoad()
{
    resetBuiltinState();
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerBeforeLoad);
}

extern "C" void VR_OnSpawnServerAfterLoad()
{
    qvr::server::onSpawnServerAfterLoad();
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerAfterLoad);
    loadingSaveGame = false;
}

extern "C" void VR_OnBeginLoadGame()
{
    loadingSaveGame = true;
}

extern "C" void VR_OnLoadGame()
{
    loadingSaveGame = false;

    // parm17..parm40 were restored with the other globals and still hold the level-start
    // values (QC only rewrites them in SetNewParms/SetChangeParms), so they become the
    // single-player client's stored parms, like parm1..16 do from the savegame header.
    VR_StoreSpawnParms(0);

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
