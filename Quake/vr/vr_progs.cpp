// vr_progs.cpp -- binds the Quake VR QuakeC entry points, globals and spawn parms.

#include "vr_hitmodel.hpp"
#include "vr_hull.hpp"
#include "vr_progs.hpp"
#include "vr_edictindex.hpp"
#include "vr_engine.hpp"
#include "vr_box3d.hpp"
#include "vr_toolgun.hpp"
#include "vr_climb.hpp"
#include "vr_foegrab.hpp"
#include "vr_ropesim.hpp"
#include "vr_crates.hpp"
#include "vr_debris.hpp"
#include "vr_explosiondebris.hpp"
#include "vr_ledges.hpp"
#include "vr_limbmodel.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_physsound.hpp"
#include "vr_server.hpp"
#include "vr_walltorch.hpp"
#include "vr_flashlight.hpp"
#include "vr_props.hpp"
#include "vr_melee_shared.h"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

namespace qvr::progs
{
namespace
{

Bindings sv_bindings;

// Level-start values of parm17..parm56, per client (parm1..parm16 live in client_t).
za::Vector<float> extSpawnParms;

// Set while Host_Loadgame_f respawns the server, for `spawnServerFromSaveFile`.
bool loadingSaveGame = false;

// The saved game being loaded: its build and its model precache list (VR_ReadSaveInfo, before the old game ends; used
// by VR_OnLoadGame). Load state of the main thread, emptied when used.
struct SaveInfo
{
    bool hasTable = false;          // the save has its model list (`// qvr_model` lines: VR_SAVE_FORMAT 1 on)
    za::Vector<za::String> models;  // [the saved .modelindex] its model's name ("": none)
    za::String notice;              // a centre print for the player once in the loaded game (another build's save)
};
SaveInfo saveInfo;

[[nodiscard]] float* clientExtSpawnParms(int client)
{
    const za::SizeT needed = (client + 1) * numExtSpawnParms;
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

// An array field's first element, if the progs' array is `n` long, each element `stride` floats (FTEQCC names them
// "name[0]".."name[n-1]", in a row); -1 otherwise.
[[nodiscard]] int findArrayField(const char* name, int n, int stride)
{
    const int first = ED_FindFieldOffset(va("%s[0]", name));
    if(first < 0 || ED_FindFieldOffset(va("%s[%d]", name, n - 1)) != first + (n - 1) * stride ||
        ED_FindFieldOffset(va("%s[%d]", name, n)) >= 0)
    {
        return -1;
    }
    return first;
}

// The melee history's fields (vr_melee.qc), checked against vr_melee_shared.h: progs built with another
// VR_MELEE_HISTORY say so, and the melee builtins answer "no history" (no blow is wiggled, nothing came anywhere).
[[nodiscard]] MeleeHistoryFields findMeleeHistory()
{
    MeleeHistoryFields m;
    m.hfar = findArrayField("mh_hfar", VR_MELEE_HISTORY, 3);
    m.hgrip = findArrayField("mh_hgrip", VR_MELEE_HISTORY, 3);
    m.hwrist = findArrayField("mh_hwrist", VR_MELEE_HISTORY, 3);
    m.hdt = findArrayField("mh_hdt", VR_MELEE_HISTORY, 1);
    m.hi = ED_FindFieldOffset("mh_hi");
    m.vgrip = ED_FindFieldOffset("mh_vgrip");
    m.vfar = ED_FindFieldOffset("mh_vfar");
    m.rgrip = ED_FindFieldOffset("mh_rgrip");
    m.valid = m.hfar >= 0 && m.hgrip >= 0 && m.hwrist >= 0 && m.hdt >= 0 && m.hi >= 0 && m.vgrip >= 0 && m.vfar >= 0 &&
              m.rgrip >= 0;
    if(!m.valid)
    {
        Con_Warning("VR: the progs' melee history (mh_hfar, mh_hgrip, mh_hwrist, mh_hdt) is not %d long, as the engine's "
                    "(vr_melee_shared.h): rebuild the QC\n",
            VR_MELEE_HISTORY);
    }
    return m;
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
    VR_EdictIndex_ProgsLoaded();

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
        b.Dummy_RetypeAll = findFunction("VR_Dummy_RetypeAll");
        b.Probe_Kinds = findFunction("VR_Probe_Kinds");
        b.Carry_Handtouch = findFunction("VR_Carry_Handtouch");
        b.Ragdoll_Handtouch = findFunction("VR_Ragdoll_Handtouch");
        b.Scene_Clean = findFunction("VR_Scene_Clean");
        b.propTouches[0] = findFunction("forcegrabbable_touch");
        b.propTouches[1] = findFunction("VR_Debris_Touch");
        b.propTouches[2] = findFunction("VR_CratePiece_Touch");
        b.propTouches[3] = findFunction("wpnthrow_touch");

        const auto globalFloat = [](const char* name) -> float* {
            ddef_t* def = findGlobalDef(name);
            if(!def || (def->type & ~DEF_SAVEGLOBAL) != ev_float)
            {
                return nullptr;
            }

            return &qcvm->globals[def->ofs];
        };

        b.melee = findMeleeHistory();

        b.spawnServerFromSaveFile = globalFloat("spawnServerFromSaveFile");
        b.playerTimeOffset = globalFloat("vr_player_time_offset");
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
    qvr::toolgun::reset(); // (what its tools hold, by entity number)
    qvr::ropesim::reset();
    qvr::climb::reset();
    qvr::foegrab::reset();
    qvr::debris::reset();
    qvr::explosiondebris::serverReset(); // (a loaded game's chunks found again at its first frame)
    qvr::crates::reset();
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
    // The map load's builds still on the pool (a load that failed half-way) finished first: they read the old world.
    qvr::hull::finishLoads();
    qvr::box3d::finishLoads();
    qvr::hull::keepForReload(); // (before the caches below let go of them: kept for the same map's next load)
    // The registered scratch buffers given back (a one-off peak not kept for the next map), and the caches of the old
    // world's data (vr_mem.hpp).
    qvr::mem::on(qvr::mem::MapChange);
}

extern "C" void VR_OnSpawnServerBeforeLoad()
{
    resetServerWorld();
    VR_EdictIndex_Reset(); // (the edicts cleared)
    qvr::physics::precacheWaterSounds();
    qvr::physsound::precache(); // the props' knocks and scrapes, the climbing grab
    qvr::climb::precache();     // the mantle's grunts
    // The training dummy's attacks (parry practice; QC vr_dummy.qc) are off at every map load, a saved game's too.
    Cvar_SetQuick(&qvr::vr_dummy_attacks, "0");
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerBeforeLoad);
    qvr::hull::beforeLoad(); // the map as brushes and the compiled hulls, on the pool while the map spawns
    qvr::box3d::beforeLoad(); // the world's mesh, likewise
}

extern "C" void VR_OnEntitySpawned(edict_t* ent)
{
    qvr::hull::entitySpawned(ent); // a monster's compiled hull, as soon as its width is known
}

namespace
{

// While the probes spawn, QuakeC's random() draws from its own numbers, not the C library's: the map's own spawns and
// first frames draw the same numbers as without them (vr_bench_seed's runs the same; the firing range's weapons).
bool probing = false;
za::U32 probeRandom = 0x2545F491u;

// The entities QC's VR_Probe_Kinds made: one monster of each kind that can appear later on the map (the firing range's
// dispensers, the training dummies' types, monsters waiting for a trigger; vr_probe.qc). Made as the map ends
// spawning, so their models and sounds are precached, their limb models made and their compiled hulls built with the
// map's (none at their first appearance: 17-30 ms frames); removed before the map's first server frame.
// `pass`: QC's (1 the monsters waiting for a trigger, 2 the dispensers', dummies' and debug spawner's kinds); the
// entities made added to `probes`.
void probeKinds(za::Vector<int>& probes, int pass)
{
    if(!sv_bindings.Probe_Kinds || qvr::vr_probe_kinds.value == 0.f || !VR_AllowLatePrecache())
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    const za::SizeT made = probes.size();
    const int before = qcvm->num_edicts;
    za::Vector<unsigned char> wasFree;
    wasFree.resize(static_cast<za::SizeT>(before), 0);
    for(int i = 0; i < before; ++i)
    {
        wasFree[static_cast<za::SizeT>(i)] = EDICT_NUM(i)->free ? 1 : 0;
    }
    probing = true;
    probeRandom = 0x2545F491u + static_cast<za::U32>(pass); // (the same numbers each load)
    G_FLOAT(OFS_PARM0) = static_cast<float>(pass);
    callSpawnServerEntryPoint(sv_bindings.Probe_Kinds);
    probing = false;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
    {
        if(!EDICT_NUM(i)->free && (i >= before || wasFree[static_cast<za::SizeT>(i)] != 0) &&
            za::find(probes.begin(), probes.end(), i) == probes.end())
        {
            probes.pushBack(i); // (its own entities too: whatever its spawn function made)
        }
    }
    VR_TimeAdd("VR after spawn: kinds that can appear (probes)", Sys_DoubleTime() - t0);
    Con_DPrintf("probes: pass %d, %d entities for the kinds that can appear later, %.1f ms\n", pass,
        static_cast<int>(probes.size() - made), (Sys_DoubleTime() - t0) * 1000.0);
}

void probesEnd(const za::Vector<int>& probes)
{
    for(const int i : probes)
    {
        edict_t* ent = EDICT_NUM(i);
        if(!ent->free)
        {
            ED_Free(ent);
        }
    }
}

} // namespace

extern "C" void VR_OnSpawnServerSpawned()
{
    // The kinds that can appear later, made now (removed below): the map's monsters waiting for a trigger, then (after
    // the map's monsters' limbs, unless vr_limbs_prebuild 2) the dispensers', dummies' and debug spawner's kinds.
    za::Vector<int> probes;
    probeKinds(probes, 1);
    const bool allLimbs = qvr::vr_limbs_prebuild.value >= 2.f;
    if(!allLimbs)
    {
        qvr::limbmodel::prebuild(); // the limbs of the map's monsters (before serverinfo: in every client's list)
    }
    probeKinds(probes, 2);
    if(allLimbs)
    {
        qvr::limbmodel::prebuild();
    }
    qvr::hull::spawned(); // the monsters' compiled hulls (their widths now known), on the pool
    probesEnd(probes);
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

void qvr::progs::modelCheck_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_model_check [1]: every entity's .modelindex against its .model, the client's models against the "
                   "server's (a server; 1: each mismatch)\n");
        return;
    }
    const bool verbose = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) != 0;
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    int models = 0;
    uint32_t hash = 2166136261u; // FNV-1a of the list, in order (two sessions' lists compared)
    for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++, models++)
    {
        for(const char* c = sv.model_precache[i]; *c; c++)
        {
            hash = (hash ^ static_cast<unsigned char>(*c)) * 16777619u;
        }
        hash = (hash ^ 0u) * 16777619u;
    }
    int checked = 0;
    int wrong = 0;
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const int index = static_cast<int>(ent->v.modelindex);
        const char* name = ent->free || !ent->v.model ? "" : PR_GetString(ent->v.model);
        if(ent->free || index == 0 || !name[0])
        {
            continue;
        }
        checked++;
        const char* has = index > 0 && index < MAX_MODELS && sv.model_precache[index] ? sv.model_precache[index] : "(none)";
        // (the ring of shadows' eyes on the player's model: deliberate)
        if(strcmp(has, name) && !(num <= svs.maxclients && !strcmp(has, "progs/eyes.mdl")))
        {
            wrong++;
            if(verbose)
            {
                Con_Printf("  entity %d (%s): model %s, index %d = %s\n", num, PR_GetString(ent->v.classname), name, index,
                    has);
            }
        }
    }
    int clientWrong = -1; // (no client)
    if(cls.state == ca_connected && cls.signon == SIGNONS)
    {
        clientWrong = 0;
        for(int i = 1; i <= models; i++)
        {
            const qmodel_t* m = cl.model_precache[i];
            if(!m || strcmp(m->name, sv.model_precache[i]))
            {
                clientWrong++;
                if(verbose)
                {
                    Con_Printf("  model %d: server %s, client %s\n", i, sv.model_precache[i], m ? m->name : "(none)");
                }
            }
        }
    }
    Con_Printf("vr_model_check: %d models (list %08x), %d entities: %d wrong; client: %d wrong\n", models, hash, checked,
        wrong, clientWrong);
    PR_PopQCVM(oldvm);
}

extern "C" void VR_OnSpawnServerAfterLoad()
{
    // (each timed for vr_startup_times' map load)
    double t = Sys_DoubleTime();
    const auto timed = [&t](const char* what)
    {
        const double now = Sys_DoubleTime();
        VR_TimeAdd(what, now - t);
        t = now;
    };
    qvr::server::onSpawnServerAfterLoad();
    timed("VR after load: server state");
    qvr::debris::afterLoad();
    qvr::ledges::afterLoad(); // climbing's ledge map (with Climbing on)
    timed("VR after load: ledges");
    qvr::hitmodel::afterLoad(); // precise hits: every precached model's triangle hierarchy (no first-hit hitch)
    timed("VR after load: hit models");
    qvr::hull::afterLoad(); // the player's narrower box: the map as brushes (vr_hull_width)
    timed("VR after load: hulls");
    qvr::climb::reset();
    qvr::foegrab::reset();
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerAfterLoad);
    timed("VR after load: QuakeC");
    loadingSaveGame = false;
}

extern "C" void VR_OnBeginLoadGame()
{
    loadingSaveGame = true;
    VR_EdictIndex_Reset();
}

// The save's build: another build's is loaded (the models found again through the save's list, the fields by name),
// with a warning in the console and a centre print in the game; a save of a newer format than this build reads is
// refused (0). A save made before builds were written in it is loaded the old way (its models found by name), and said.
extern "C" int VR_ReadSaveInfo(const char* text, const char* relname)
{
    using qvr::progs::saveInfo;
    saveInfo.hasTable = false;
    saveInfo.models.clear();
    saveInfo.notice.clear();
    if(Q_atoi(text) == SAVEGAME_VERSION_KEX)
    {
        return 1; // (the re-release's)
    }
    const char* line = strstr(text, "\n// qvr_save ");
    if(!line)
    {
        Con_Warning("%s was saved by an older build (it names none): its models are found again by name\n", relname);
        saveInfo.notice = "Saved game from an older build\n(see the console)";
        return 1;
    }
    line++;
    int format = 0;
    unsigned int crc = 0;
    char build[128] = "?";
    sscanf(line, "// qvr_save %d progs %x build %127[^\r\n]", &format, &crc, build);
    if(format > VR_SAVE_FORMAT)
    {
        Con_Printf("ERROR: %s was saved by a newer build (%s, save format %d; this one, %s, reads up to %d): not loaded\n",
            relname, build, format, VR_BuildVersion(), VR_SAVE_FORMAT);
        SCR_CenterPrint("Saved game from a newer build:\nnot loaded (see the console)");
        return 0;
    }
    if(strcmp(build, VR_BuildVersion()) != 0)
    {
        const unsigned int now = sv.qcvm.progs ? sv.qcvm.crc : 0u;
        Con_Warning("%s was saved by build %s (progs %04x); this is build %s (progs %04x%s)\n", relname, build, crc,
            VR_BuildVersion(), now, now == crc ? ", the same" : "");
        saveInfo.notice = va("Saved game from another build\n%s", build);
    }
    for(const char* p = strchr(line, '\n'); p && !strncmp(p + 1, "// qvr_model ", 13); p = strchr(p + 1, '\n'))
    {
        const char* at = p + 1 + 13;
        char* end = nullptr;
        const long i = strtol(at, &end, 10);
        if(end == at || *end != ' ' || i <= 0 || i >= MAX_MODELS)
        {
            continue;
        }
        if(static_cast<long>(saveInfo.models.size()) <= i)
        {
            saveInfo.models.resize(static_cast<za::SizeT>(i + 1));
        }
        saveInfo.models[static_cast<za::SizeT>(i)] = za::String{end + 1, strcspn(end + 1, "\r\n")};
    }
    saveInfo.hasTable = true;
    return 1;
}

void qvr::progs::loadNoticeFrame()
{
    // (the loaded game's player in it: the save's warning where it is seen)
    if(saveInfo.notice.empty() || svs.maxclients < 1 || !svs.clients[0].spawned)
    {
        return;
    }
    MSG_WriteByte(&svs.clients[0].message, svc_centerprint);
    MSG_WriteString(&svs.clients[0].message, saveInfo.notice.cStr());
    saveInfo.notice.clear();
}

namespace
{

// Whether the entity reference at `val` (a prog edict offset) is a whole edict below `numEdicts`; if not, it is
// reset to the world with a developer warning naming `what` and `name`.
void checkEntityReference(int* val, int numEdicts, const char* what, const char* name)
{
    if(*val >= 0 && *val % qcvm->edict_size == 0 && *val / qcvm->edict_size < numEdicts)
    {
        return;
    }
    Con_DWarning("%s \"%s\" refers to entity %i, past the %i loaded: the world instead\n", what, name,
        *val / qcvm->edict_size, numEdicts);
    *val = 0;
}

} // namespace

// Host_Loadgame_f, after a saved game's edicts are parsed: every entity field and entity global must refer to one of
// its `numEdicts` edicts. A reference past them (a save written with an edict missing, or edited) would be a
// Host_Error ("NUM_FOR_EDICT: bad pointer") the first time the progs used it; it is reset to the world instead.
// References to free edicts are left alone, as in any Quake: the progs check them.
extern "C" void VR_CheckLoadedReferences(int numEdicts)
{
    for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        const ddef_t* d = &qcvm->globaldefs[i];
        if((d->type & ~DEF_SAVEGLOBAL) == ev_entity)
        {
            checkEntityReference(reinterpret_cast<int*>(qcvm->globals) + d->ofs, numEdicts, "global",
                PR_GetString(d->s_name));
        }
    }

    char what[32];
    for(int e = 0; e < numEdicts; e++)
    {
        edict_t* ed = EDICT_NUM(e);
        for(int i = 0; i < qcvm->progs->numfielddefs; i++)
        {
            const ddef_t* d = &qcvm->fielddefs[i];
            if((d->type & ~DEF_SAVEGLOBAL) != ev_entity)
            {
                continue;
            }
            q_snprintf(what, sizeof(what), "entity %i's", e);
            checkEntityReference(reinterpret_cast<int*>(&ed->v) + d->ofs, numEdicts, what, PR_GetString(d->s_name));
        }
    }
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


// Today's precache index of the model the save's list had at `saved` (precached now if need be); -1: the save's list
// has no model there (the value left alone).
[[nodiscard]] int remapSavedModel(int saved)
{
    if(saved <= 0 || saved >= static_cast<int>(saveInfo.models.size()) || saveInfo.models[saved].empty())
    {
        return -1;
    }
    const char* name = saveInfo.models[saved].cStr();
    const int index = modelPrecacheIndex(name);
    return index >= 0 ? index : VR_LatePrecacheModel(Hunk_Strdup(name, "precache")); // (the list keeps the pointer)
}

// A float that holds a model index (QC: `.modelindex`, the globals `modelindex_eyes` and the like, the fields that keep
// one, `.vr_corpse_model`, `.burn_model`, `.vr_stick_model`), given today's index for the same model.
[[nodiscard]] bool holdsModelIndex(const ddef_t& d)
{
    if((d.type & ~DEF_SAVEGLOBAL) != ev_float)
    {
        return false;
    }
    const char* name = PR_GetString(d.s_name);
    const size_t len = strlen(name);
    return !strcmp(name, "modelindex") || !strncmp(name, "modelindex_", 11) ||
           (len > 6 && !strcmp(name + len - 6, "_model"));
}

void remapModelValue(float& value, const char* what, int num)
{
    const int saved = static_cast<int>(value);
    if(static_cast<float>(saved) != value)
    {
        return;
    }
    const int index = remapSavedModel(saved);
    if(index >= 0 && index != saved)
    {
        Con_DPrintf("load: %s %d: model %d -> %d (%s)\n", what, num, saved, index, saveInfo.models[saved].cStr());
        value = static_cast<float>(index);
    }
}

// The saved .modelindex values index the precache list of the game that was saved, and the map loaded now precaches
// its models again, in its own order: the same only if every spawn function precached the same models in the same
// order (the training dummy precaches the enemy vr_dummy_type names: changed since, every model after it moved one way
// or the other, buttons and panels drawn as the vore's head or body), and without the models precached late (setmodel
// of one the map's spawn functions don't precache: the firing range's boxes, test spawns, limbs cut off, the dummy's new
// enemy), which the loaded map lacks or has elsewhere. Each value is turned into today's index for the model the save's
// list had there (precached now if need be: the client, connecting after the load, gets the whole list): every entity's
// .modelindex (the ring of shadows' eyes on the player's model too), and the globals and fields that keep one.
// A save made before the list was saved (no `// qvr_model` lines): the entity's .model names its model (the saved index
// is trusted only when it names that model), and the other values go by the list the entities' pairs make.
void rebindLoadedModels()
{
    if(!saveInfo.hasTable)
    {
        // (the saved index -> the .model of the entities that had it, the first one's: a legacy list)
        for(int num = 1; num < qcvm->num_edicts; num++)
        {
            edict_t* ent = EDICT_NUM(num);
            const int saved = static_cast<int>(ent->v.modelindex);
            const char* name = ent->free || !ent->v.model ? "" : PR_GetString(ent->v.model);
            if(saved <= 0 || saved >= MAX_MODELS || !name[0])
            {
                continue;
            }
            if(static_cast<int>(saveInfo.models.size()) <= saved)
            {
                saveInfo.models.resize(static_cast<za::SizeT>(saved + 1));
            }
            if(saveInfo.models[saved].empty())
            {
                saveInfo.models[saved] = name;
            }
        }
    }

    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(ent->free)
        {
            continue;
        }
        const int saved = static_cast<int>(ent->v.modelindex);
        for(int i = 0; i < qcvm->progs->numfielddefs; i++)
        {
            const ddef_t& d = qcvm->fielddefs[i];
            if(holdsModelIndex(d))
            {
                remapModelValue(reinterpret_cast<float*>(&ent->v)[d.ofs], "entity", num);
            }
        }
        // (a legacy save: the entity's own model's name wins over the list's guess)
        const char* name = ent->v.model ? PR_GetString(ent->v.model) : "";
        const int now = static_cast<int>(ent->v.modelindex);
        if(!saveInfo.hasTable && name[0] && now != 0 &&
            (now >= MAX_MODELS || !sv.model_precache[now] || strcmp(sv.model_precache[now], name)))
        {
            const int index = modelPrecacheIndex(name);
            ent->v.modelindex = static_cast<float>(za::max(index >= 0 ? index : VR_LatePrecacheModel(name), 0));
            Con_DPrintf("load: entity %d's model %s: index %d -> %d\n", num, name, saved, static_cast<int>(ent->v.modelindex));
        }
        if(static_cast<int>(ent->v.modelindex) != saved)
        {
            SV_LinkEdict(ent, false);
        }
    }
    for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
    {
        const ddef_t& d = qcvm->globaldefs[i];
        if(holdsModelIndex(d))
        {
            remapModelValue(qcvm->globals[d.ofs], "global", i);
        }
    }
    saveInfo.hasTable = false;
    saveInfo.models.clear();
    // Everything precached while loading is in the serverinfo that clients receive (none is connected yet).
    qvr::server::onSpawnServerAfterLoad();
}

} // namespace

extern "C" void VR_OnLoadGame()
{
    loadingSaveGame = false;
    VR_EdictIndex_Reset(); // (the edicts parsed from the save)

    // parm17..parm56 were restored with the other globals and still hold the level-start
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
    qvr::foegrab::reset();

    callEntryPoint(sv_bindings.OnLoadGame);
    qvr::flashlight::restoreState();
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
            const double t0 = Sys_DoubleTime(); // (its load: the frame's hitch, said with developer 1)
            sv.model_precache[i] = name;
            sv.models[i] = Mod_ForName(name, true);
            if(sv.state == ss_active) // (not the map's own: vr_limbs_prebuild's, as it loads)
            {
                Con_DPrintf("late precache: %s, %.1f ms\n", name, (Sys_DoubleTime() - t0) * 1000.0);
            }
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

// PF_random while the probes spawn (VR_OnSpawnServerSpawned): its own numbers, in 0..0x7fff as rand()'s; -1 otherwise.
extern "C" int VR_ProbeRandom()
{
    if(!probing)
    {
        return -1;
    }
    probeRandom = probeRandom * 1664525u + 1013904223u;
    return static_cast<int>((probeRandom >> 16) & 0x7fffu);
}

extern "C" int VR_AllowLatePrecache()
{
    return sv_bindings.isVrProgs;
}
