// vr_progs.cpp -- binds the Quake VR QuakeC entry points, globals and spawn parms.

#include "vr_ad.hpp"
#include "vr_hitmodel.hpp"
#include "vr_hull.hpp"
#include "vr_nav.hpp"
#include "vr_brushes.hpp"
#include "vr_permdecals.hpp"
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
#include "vr_diskcache.hpp"
#include "vr_explosiondebris.hpp"
#include "vr_ledges.hpp"
#include "vr_limbmodel.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_footsteps.hpp"
#include "vr_physsound.hpp"
#include "vr_server.hpp"
#include "vr_srvrandom.hpp"
#include "vr_walltorch.hpp"
#include "vr_flashlight.hpp"
#include "vr_props.hpp"
#include "vr_rigstudio.hpp"
#include "vr_runsetup.hpp"
#include "vr_weaponstudio.hpp"
#include "vr_scenario.hpp"
#include "vr_tips.hpp"
#include "vr_melee_shared.h"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include <string.h>

namespace qvr::progs
{
namespace
{

Bindings sv_bindings;

// Level-start values of parm17..parm72, per client (parm1..parm16 live in client_t).
za::Vector<float> extSpawnParms;

// Set while Host_Loadgame_f respawns the server, for `spawnServerFromSaveFile`.
bool loadingSaveGame = false;

// The saved game being loaded: its build and its model precache list (VR_ReadSaveInfo, before the old game ends; used
// by VR_OnLoadGame). Load state of the main thread, emptied when used.
struct SaveInfo
{
    za::Vector<za::String> models;  // [the saved .modelindex] its model's name ("": none; `// qvr_model` lines)
    za::Vector<za::String> sounds;  // the saved sound precache list (`// qvr_sound` lines)
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
    // The VM's name table (pr_edict.c ED_FindFunction: hashed once the progs are loaded): several callers look up by
    // name each step (vr_box3d_step.cpp's callbacks), and the scan over every function's name cost ~0.03 ms a call.
    if(qcvm->ht_functions.capacity > 0)
    {
        const dfunction_t* f = VR_ED_FindFunction(name);
        return f ? static_cast<func_t>(f - qcvm->functions) : 0;
    }
    for(int i = 0; i < qcvm->progs->numfunctions; i++)
    {
        if(!strcmp(PR_GetString(qcvm->functions[i].s_name), name))
        {
            return i;
        }
    }

    return 0;
}

namespace
{
// qcTestsPresent's last verdict (true until progs are seen: nothing is claimed missing).
struct QcTestsVerdict
{
    bool valid;
    int crc;
    int functions;
    bool present;
};
QcTestsVerdict qcTestsLast{false, 0, 0, true};
} // namespace

bool qcTestsPresent()
{
    // (VR_TestImpulse: the test impulses' entry, QC/vr_impulse_test.qc, in every flavour with the tests.)
    // The menu builds outside the game's frames (qcvm is null there): the server's progs are looked in, the verdict kept
    // per progs (its CRC and function count) so a menu frame costs nothing.
    if(!sv.active || !sv.qcvm.progs)
    {
        return qcTestsLast.present;
    }
    const dprograms_t* const p = sv.qcvm.progs;
    if(!qcTestsLast.valid || qcTestsLast.crc != p->crc || qcTestsLast.functions != p->numfunctions)
    {
        qcvm_t* const old = qcvm;
        if(old != &sv.qcvm)
        {
            if(old)
            {
                PR_SwitchQCVM(nullptr);
            }
            PR_SwitchQCVM(&sv.qcvm);
        }
        qcTestsLast = {true, p->crc, p->numfunctions, findFunction("VR_TestImpulse") != 0};
        if(old != &sv.qcvm)
        {
            PR_SwitchQCVM(nullptr);
            if(old)
            {
                PR_SwitchQCVM(old);
            }
        }
    }
    return qcTestsLast.present;
}

namespace
{
// A QC function of one entity parameter (0: none) run for its float return, the caller's self, first parameter and
// return kept (staminaMax, staminaLeft: asked from builtins' code too).
[[nodiscard]] float askQc(func_t fn, edict_t* e)
{
    if(!fn)
    {
        return 0.f;
    }
    const int self = pr_global_struct->self;
    const int parm = G_INT(OFS_PARM0);
    const float ret[3]{G_FLOAT(OFS_RETURN), G_FLOAT(OFS_RETURN + 1), G_FLOAT(OFS_RETURN + 2)};
    pr_global_struct->self = EDICT_TO_PROG(qcvm->edicts);
    G_INT(OFS_PARM0) = e ? EDICT_TO_PROG(e) : 0;
    PR_ExecuteProgram(fn);
    const float out = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = self;
    G_INT(OFS_PARM0) = parm;
    G_FLOAT(OFS_RETURN) = ret[0], G_FLOAT(OFS_RETURN + 1) = ret[1], G_FLOAT(OFS_RETURN + 2) = ret[2];
    return out;
}
} // namespace

bool globalVec(const char* name, glm::vec3& out)
{
    if(!sv.active || !sv.qcvm.progs)
    {
        return false;
    }
    const qcvm_t& vm = sv.qcvm; // (the server's progs: whichever VM is current)
    for(int i = 0; i < vm.progs->numglobaldefs; i++)
    {
        const ddef_t& def = vm.globaldefs[i];
        if((def.type & ~DEF_SAVEGLOBAL) == ev_vector && def.s_name >= 0 && def.s_name < vm.stringssize && !strcmp(vm.strings + def.s_name, name))
        {
            const float* v = &vm.globals[def.ofs];
            out = {v[0], v[1], v[2]};
            return true;
        }
    }
    return false;
}

float staminaMax() { return za::max(1.f, askQc(bindings().Stamina_Max, nullptr)); }

float staminaLeft(edict_t* p) { return za::clamp(askQc(bindings().Stamina_Left, p), 0.f, staminaMax()); }

bool staminaOn() { return askQc(bindings().Stamina_On, nullptr) != 0.f; }

float staminaEffort(edict_t* p, float cost, const char* what)
{
    const func_t fn = bindings().Stamina_Effort;
    if(!fn || !p)
    {
        return 1.f;
    }
    const int self = pr_global_struct->self;
    const float parm0 = G_FLOAT(OFS_PARM0);
    const int parm1 = G_INT(OFS_PARM1);
    const float ret[3]{G_FLOAT(OFS_RETURN), G_FLOAT(OFS_RETURN + 1), G_FLOAT(OFS_RETURN + 2)};
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(p);
    G_FLOAT(OFS_PARM0) = cost;
    G_INT(OFS_PARM1) = PR_SetEngineString(what);
    PR_ExecuteProgram(fn);
    const float out = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = self;
    G_FLOAT(OFS_PARM0) = parm0;
    G_INT(OFS_PARM1) = parm1;
    G_FLOAT(OFS_RETURN) = ret[0], G_FLOAT(OFS_RETURN + 1) = ret[1], G_FLOAT(OFS_RETURN + 2) = ret[2];
    return za::clamp(out, 0.f, 1.f);
}

} // namespace qvr::progs

using namespace qvr::progs;

namespace
{

// The game code's check (checkGameCode): what was said last (its source, kind and missing fields), said again only when
// that changes (the progs load at every map); and its centre print for the player, once in the game (loadNoticeFrame).
za::String gameCodeChecked;
za::String gameCodeNotice;

// Whether the progs.dat loaded is this build's (the 1.0.0 report "guns can only be grabbed as props", a player who had
// built a pre-release: another folder's progs.dat, or id's, ran with this engine). Said in the console (a condump has
// it) and, when it is not, centre-printed once: a progs.dat that is not Quake VR's (id's, from id1 or a mission pack:
// Quake VR's gameplay off, weapons only props) while quakevr is the game; or Quake VR's but older than this engine (some
// of the VR fields it binds by name missing: another build's quakevr folder).
void checkGameCode(const Bindings& b)
{
    if(!COM_FileExists("progs.dat", nullptr))
    {
        return;
    }
    char source[MAX_OSPATH];
    if(Sys_FileType(com_filesource) == FS_ENT_DIRECTORY)
    {
        q_snprintf(source, sizeof(source), "%s/progs.dat", com_filesource);
    }
    else
    {
        q_strlcpy(source, com_filesource, sizeof(source));
    }
    int total = 0;
    int missing = 0;
    const char* firstMissing = "";
    if(b.isVrProgs)
    {
#define QVR_FIELD(name)                                                                                                \
    ++total;                                                                                                           \
    if(b.fields.name < 0 && !missing++)                                                                                \
    {                                                                                                                  \
        firstMissing = #name;                                                                                          \
    }
#include "vr_fields.inc"
#undef QVR_FIELD
    }
    const char* key = va("%s|%d|%d", source, b.isVrProgs ? 1 : 0, missing);
    if(gameCodeChecked == key)
    {
        return;
    }
    gameCodeChecked = za::String{key};
    // (quakevr the game: the last of the game dirs. A mod over it, "-game quakevr -game <mod>", brings its own progs.)
    const char* games = com_gamenames;
    const char* last = strrchr(games, ';');
    const bool vrGame = !q_strcasecmp(last ? last + 1 : games, "quakevr");
    Con_Printf("[skipnotify]VR: game code %s: %s\n", source,
        !b.isVrProgs ? "not Quake VR's" : missing ? "Quake VR's, older than this engine" : "Quake VR's, this build's");
    if(!b.isVrProgs && vrGame)
    {
        Con_Warning("VR: this progs.dat is not Quake VR's game code (%s): Quake VR's gameplay is off, weapons are only "
                    "props. The quakevr folder searched has none: start Quake VR with the shortcut its installer made, "
                    "or from the folder it is installed in; \"path\" lists the folders searched.\n",
            source);
        gameCodeNotice = za::String{"Quake VR's game code was not found\nWeapons are only props: start Quake VR\n"
                                    "with its installer's shortcut (console: path)"};
    }
    else if(missing)
    {
        Con_Warning("VR: this progs.dat is older than this engine (%s: %d of the %d VR fields missing, %s the first): "
                    "another build's quakevr folder, not this one's; weapons and more misbehave. Start Quake VR with "
                    "the shortcut its installer made, or update or remove that folder; \"path\" lists the folders "
                    "searched.\n",
            source, missing, total, firstMissing);
        gameCodeNotice = za::String{"Quake VR's game code is from another build\nWeapons may misbehave: start Quake VR\n"
                                    "with its installer's shortcut (console: path)"};
    }
}

} // namespace

extern "C" void VR_OnProgsLoaded()
{
    if(qcvm != &sv.qcvm)
    {
        return;
    }
    VR_EdictIndex_ProgsLoaded();

    Bindings b;

    b.isVrProgs = ED_FindFieldOffset("handpos") >= 0;
    if(b.isVrProgs && !qcvm->progs32)
    {
        // Ours compiles to the 32-bit format (QC/defs.qc's fte32 pragma; qvr_checks progs_format fails the build
        // otherwise): a 16-bit one came from a stock FTEQCC, and stops compiling at 65,536 globals.
        Con_Warning("progs.dat is the 16-bit format: compile the QC with Tools/fteqcc (its fte32 flag)\n");
    }
    if(b.isVrProgs)
    {
#define QVR_FIELD(name) b.fields.name = ED_FindFieldOffset(#name);
#include "vr_fields.inc"
#undef QVR_FIELD
        const FieldOffsets& f = b.fields;
        b.hands[0] = HandFields{f.offhandpos, f.offhandrot, f.offhand_held, f.offmuzzlepos, f.offshotrot,
            f.offloadportpos, f.offloadportaxis, f.offloadportface, f.offhandthrowvel, f.offhandavel};
        b.hands[1] = HandFields{f.handpos, f.handrot, f.mainhand_held, f.muzzlepos, f.shotrot, f.loadportpos,
            f.loadportaxis, f.loadportface, f.handthrowvel, f.handavel};

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
        b.AxeStick_Stuck = findFunction("VR_AxeStick_Stuck");
        b.AxeStick_Plate = findFunction("VR_AxeStick_Plate");
        b.ClipSizeOf = findFunction("VR_ClipSizeOf");
        b.RunRefusesEnt = findFunction("VR_Run_RefusesEnt");
        b.KdTrig_BodyHit = findFunction("VR_KdTrig_BodyHit");
        b.RagImpact_Hit = findFunction("VR_RagImpact_Hit");
        b.Flail_Hit = findFunction("VR_Flail_Hit");
        b.Flail_Rattle = findFunction("VR_Flail_Rattle");
        b.Monster_Fall = findFunction("VR_Monster_Fall");
        b.Knockdown_MakeRoom = findFunction("VR_Knockdown_MakeRoom");
        b.Stamina_Max = findFunction("VR_Stamina_Max");
        b.Stamina_Left = findFunction("VR_Stamina_Left");
        b.Stamina_On = findFunction("VR_Stamina_On");
        b.Stamina_Effort = findFunction("VR_Stamina_Effort");
        b.foegrab.HeadZone = findFunction("VR_FoeGrab_HeadZone");
        b.foegrab.HeadLeft = findFunction("VR_FoeGrab_HeadLeft");
        b.foegrab.Released = findFunction("VR_FoeGrab_Released");
        b.foegrab.Strength = findFunction("VR_FoeGrab_Strength");
        b.foegrab.Taken = findFunction("VR_FoeGrab_Taken");
        b.foegrab.Throw = findFunction("VR_FoeGrab_Throw");
        b.foegrab.HandOver = findFunction("VR_FoeGrab_HandOver");
        b.foegrab.Manhandle = findFunction("VR_FoeGrab_Manhandle");
        b.foegrab.BreakFree = findFunction("VR_FoeGrab_BreakFree");
        b.foegrab.NeckSnap = findFunction("VR_FoeGrab_NeckSnap");
        b.foegrab.Lift = findFunction("VR_FoeGrab_Lift");
        b.foegrab.MonsterMass = findFunction("VR_Grapple_MonsterMass");
        b.foegrab.LegSweepDown = findFunction("VR_LegSweep_Down");
        b.arenaCards.CardPick = findFunction("VR_ARK_CardPick");
        b.arenaCards.CardSerial = findFunction("VR_ARK_CardSerial");
        b.arenaCards.CardCount = findFunction("VR_ARK_CardCount");
        b.arenaCards.CardBit = findFunction("VR_ARK_CardBit");
        b.arenaCards.Name = findFunction("VR_ARK_Name");
        b.arenaCards.What = findFunction("VR_ARK_What");
        b.arenaCards.Model = findFunction("VR_ARK_Model");
        b.arenaCards.Colour = findFunction("VR_ARK_Colour");
        b.arenaCards.CardTitle = findFunction("VR_ARK_CardTitle");
        b.arenaCards.PoolSize = findFunction("VR_ARK_PoolSize");
        b.arenaCards.CardsSeen = findFunction("VR_ARK_CardsSeen");
        b.arenaCards.DebugOffer = findFunction("VR_ARK_DebugOffer");

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
        b.handTimeScale = globalFloat("vr_hand_time_scale");
        b.musicTrack = globalFloat("vr_music_track");
        b.mapContentSeed = globalFloat("vr_map_content_seed");
        b.spawnLive = globalFloat("vr_spawn_live");
        b.arenaWave = globalFloat("ar_wave");
        b.arenaWaveAt = globalFloat("ar_wave_at");
        b.arenaPresetWaves = globalFloat("ar_preset_n");
        b.agrHold = globalFloat("vr_agr_hold");
        const auto globalVector = [](const char* name) -> float* {
            ddef_t* def = findGlobalDef(name);
            return def && (def->type & ~DEF_SAVEGLOBAL) == ev_vector ? &qcvm->globals[def->ofs] : nullptr;
        };
        constexpr const char* agkParts[] = {"vr_agk_k_rp", "vr_agk_k_rr", "vr_agk_k_lp", "vr_agk_k_lr", "vr_agk_k_er",
            "vr_agk_k_el", "vr_agk_k_t", "vr_agk_k_gp", "vr_agk_k_gr"};
        static_assert(sizeof(agkParts) / sizeof(agkParts[0]) == sizeof(Bindings::agkPart) / sizeof(float*));
        for(size_t i = 0; i < sizeof(agkParts) / sizeof(agkParts[0]); i++)
        {
            b.agkPart[i] = globalVector(agkParts[i]);
        }
        b.agDrawn[0] = globalVector("vr_ag_df_o");
        b.agDrawn[1] = globalVector("vr_ag_df_fw");
        b.agDrawn[2] = globalVector("vr_ag_df_up");
        b.agExtent[0] = globalVector("vr_ag_wx_back");
        b.agExtent[1] = globalVector("vr_ag_wx_front");
        b.agPose[Bindings::agpWr] = globalFloat("vr_agp_wr");
        b.agPose[Bindings::agpV] = globalVector("vr_agp_v");
        b.agPose[Bindings::agpFw] = globalVector("vr_agp_fw");
        b.agPose[Bindings::agpUp] = globalVector("vr_agp_up");
        b.agPose[Bindings::agpF] = globalVector("vr_agp_f");
        b.agPose[Bindings::agpR] = globalVector("vr_agp_r");
        b.agPose[Bindings::agpWo] = globalFloat("vr_agp_wo");
        b.agPose[Bindings::agpWl] = globalFloat("vr_agp_wl");
        b.agPose[Bindings::agpVl] = globalVector("vr_agp_vl");
        b.agPose[Bindings::agpFwl] = globalVector("vr_agp_fwl");
        b.agPose[Bindings::agpUpl] = globalVector("vr_agp_upl");
        b.agPose[Bindings::agpTq] = globalVector("vr_agp_tq");
        b.agPose[Bindings::agpElr] = globalVector("vr_agp_elr");
        b.agPose[Bindings::agpEll] = globalVector("vr_agp_ell");
        b.agPose[Bindings::agkGp] = globalVector("vr_agk_gp");
        b.agPose[Bindings::agkGr] = globalVector("vr_agk_gr");
        b.agPose[Bindings::agkLp] = globalVector("vr_agk_lp");
        b.agPose[Bindings::agkLr] = globalVector("vr_agk_lr");
        b.agPose[Bindings::agpWaist] = globalVector("vr_agp_waist");
        b.blowLine[0] = globalVector("vr_blow_a");
        b.blowLine[1] = globalVector("vr_blow_b");
        b.blowLine[2] = globalVector("vr_blow_more");
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

    checkGameCode(b);
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
    qvr::brushes::reset(); // (the boxes made in the game: the map's)
    qvr::ropesim::reset();
    qvr::climb::reset();
    qvr::foegrab::reset();
    qvr::debris::reset();
    qvr::explosiondebris::serverReset(); // (a loaded game's chunks found again at its first frame)
    qvr::crates::reset();
    qvr::props::resetModelCache(); // (the models' names may be others' now)
    qvr::hitmodel::reset();
    qvr::scenario::worldReset(); // (a checklist test scene ends at another map or a load, not at its own)
    resetBuiltinState();
}

// The pool's jobs that read the world model (its pointer captured at their post), finished: the hull's brushes and
// trees (the load's and those compiled in play), box3d's world mesh, the navmeshes' tile builds.
void joinWorldReaders()
{
    qvr::hull::finishLoads();
    qvr::box3d::finishLoads();
    qvr::nav::finishLoads();
}

} // namespace

extern "C" void VR_OnShutdownServer()
{
    // Host_ShutdownServer: a game or campaign switch (COM_SwitchGameInternal) frees the models next (Mod_ResetAll)
    // without Host_ClearMemory, so the pool's builds that read the world model are joined here too.
    joinWorldReaders();
}

extern "C" void VR_OnModelsResetting()
{
    // COM_SwitchGameInternal, before Mod_ResetAll: Host_ShutdownServer skips VR_OnShutdownServer when the server was
    // not active (a map load that failed half-way), its builds maybe still on the pool.
    joinWorldReaders();
}

extern "C" void VR_OnClearMemory()
{
    // Host_ClearMemory (SV_SpawnServer: every map and loaded game; CL_ClearState without a local server): the edicts,
    // cl_entities and the models are about to be freed with the hunk. The server's and the client's VR state of the old
    // world goes first, so that nothing keeps a pointer into the freed hunk (the client's again at the new serverinfo,
    // VR_OnClientClearState; the server's at VR_OnSpawnServerBeforeLoad).
    resetServerWorld();
    qvr::diskcache::setInPlay(false); // a load from here (the cache's misses aren't "in play" until it ends)
    if(cls.state != ca_dedicated)
    {
        VR_OnClientClearState();
    }
    // The map load's builds still on the pool (a load that failed half-way) finished first: they read the old world.
    joinWorldReaders();
    qvr::hull::keepForReload(); // (before the caches below let go of them: kept for the same map's next load)
    // The registered scratch buffers given back (a one-off peak not kept for the next map), and the caches of the old
    // world's data (vr_mem.hpp).
    qvr::mem::on(qvr::mem::MapChange);
}

extern "C" void VR_OnSpawnServerBeforeLoad()
{
    resetServerWorld();
    qvr::weaponstudio::beforeLoad(); // (its preview humanoid gone with the edicts)
    qvr::rigstudio::beforeLoad(); // (its subject gone with the edicts: staged again in the new map)
    qvr::ad::beforeLoad(); // vr_ad_rules for this map, before anything of it spawns (and QC's hook reads it)
    VR_EdictIndex_Reset(); // (the edicts cleared)
    qvr::physics::precacheWaterSounds();
    qvr::physsound::precache(); // the props' knocks and scrapes, the climbing grab
    qvr::footsteps::precache(); // the enemies' footsteps
    qvr::climb::precache();     // the mantle's grunts
    // The training dummy's attacks (parry practice; QC vr_dummy.qc) are off at every map load, a saved game's too.
    Cvar_SetQuick(&qvr::vr_dummy_attacks, "0");
    // The map's content seed (vr_fresh_map_seeds), for QC's rolls as the map spawns and the planners at its first frame.
    qvr::runsetup::beforeMapLoad(loadingSaveGame); // (a game begun without a run's start: one seed from its first map on)
    qvr::srvrandom::chooseMapSeed(loadingSaveGame);
    if(sv_bindings.mapContentSeed)
    {
        *sv_bindings.mapContentSeed = static_cast<float>(qvr::srvrandom::mapSeed());
    }
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
    // The run's mutators' map hook (QC VR_Run_MapSpawned: Randomized Pickups): every entity of the map spawned, precaches
    // still open. Not a saved game's load (its entities come from the save). Its parameter: the weapons the player enters
    // with (his level-start parm1: a changelevel, a restart), 0 in a new game (not connected yet).
    if(!loadingSaveGame)
    {
        if(dfunction_t* fn = VR_ED_FindFunction("VR_Run_MapSpawned"))
        {
            const float held = svs.clients && svs.clients[0].active ? svs.clients[0].spawn_parms[0] : 0.f;
            (void)callQc(static_cast<func_t>(fn - qcvm->functions), held);
        }
    }

    // The kinds that can appear later, made now (removed below): the map's monsters waiting for a trigger, then (after
    // the map's monsters' limbs, unless vr_limbs_prebuild 2) the dispensers', dummies' and debug spawner's kinds.
    za::Vector<int> probes;
    qvr::limbmodel::newMap(); // (spawnFrame's kinds: those spawned from now on that prebuild didn't make)
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
    progs::ServerVm serverVm;
    if(num > svs.maxclients && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
    {
        Con_Printf("vr_test_remove: entity %d (%s) removed\n", num, PR_GetString(EDICT_NUM(num)->v.classname));
        ED_Free(EDICT_NUM(num));
    }
    else
    {
        Con_Printf("vr_test_remove: no entity %d to remove\n", num);
    }
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
    progs::ServerVm serverVm;
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
    qvr::nav::afterLoad(); // the monsters' navmeshes: gathered here, built (or read) on the pool
    timed("VR after load: navmesh");
    qvr::climb::reset();
    qvr::foegrab::reset();
    qvr::tips::serverMapStarted(loadingSaveGame); // (a tutorial's tips forgotten at its start: _vr_tips_reset_on_start)
    callSpawnServerEntryPoint(sv_bindings.OnSpawnServerAfterLoad);
    timed("VR after load: QuakeC");
    loadingSaveGame = false;
}

// The CD track a connecting client is told (svc_serverinfo's svc_cdtrack): the one the map's QC switched to
// (target_vr_music: vr_music_track, kept in a saved game), else the map's own (worldspawn's "sounds"). A campaign's
// own track (vr_music.cpp) the player doesn't have (a save from another install): the map's.
extern "C" int VR_ServerMusicTrack(int mapTrack)
{
    const float* track = sv_bindings.musicTrack;
    if(!track || *track < 1.f || *track > 255.f) { return mapTrack; }
    const int t = static_cast<int>(*track);
    return t < 128 || BGM_TrackAvailable(t) ? t : mapTrack;
}

extern "C" void VR_OnBeginLoadGame()
{
    loadingSaveGame = true;
    qvr::runsetup::onLoadGame(); // the saved run's choices (its difficulty, presets, mutators: vr_runsetup.cpp)
    VR_EdictIndex_Reset();
}

// The save's build: another build's is loaded (the models found again through the save's list, the fields by name),
// with a warning in the console and a centre print in the game; a save of a newer format than this build reads is
// refused (0), and so is one without the `// qvr_save` line (every Quake VR save since 2026-10-06 has it): an older
// build's, another engine's or progs', the re-release's; its model indices can't be trusted.
extern "C" int VR_ReadSaveInfo(const char* text, const char* relname)
{
    using qvr::progs::saveInfo;
    saveInfo.models.clear();
    saveInfo.sounds.clear();
    saveInfo.notice.clear();
    const char* line = strstr(text, "\n// qvr_save ");
    if(!line)
    {
        Con_Printf("ERROR: %s has no \"// qvr_save\" line: a save of a Quake VR build before 2026-10-06, of another "
                   "engine or of the re-release, which this build can't load: not loaded\n", relname);
        SCR_CenterPrint("Saved game from an older build\nor another game: not loaded\n(see the console)");
        return 0;
    }
    line++;
    int format = 0;
    unsigned int crc = 0;
    char build[128] = "?";
    sscanf(line, "// qvr_save %d progs %x build %127[^\r\n]", &format, &crc, build);
    qvr::brushes::readSaveLines(text); // (made again after the load: restoreAfterLoad)
    qvr::permdecals::readSaveLines(text); // (the permanent decals: baked again once its map has loaded)
    qvr::runsetup::readSaveLines(text);   // (the run's choices: set when the load goes ahead, VR_OnBeginLoadGame)
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
    const char* p = strchr(line, '\n');
    for(; p && !strncmp(p + 1, "// qvr_model ", 13); p = strchr(p + 1, '\n'))
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
    // The sound precache list, its lines together after the context's (saves before 2026-10-20 have none: their late
    // sounds are lost)
    for(p = strstr(line, "\n// qvr_sound "); p && !strncmp(p + 1, "// qvr_sound ", 13); p = strchr(p + 1, '\n'))
    {
        const char* at = p + 1 + 13;
        const size_t length = strcspn(at, "\r\n");
        if(length > 0 && length < MAX_QPATH)
        {
            saveInfo.sounds.pushBack(za::String{at, length});
        }
    }
    return 1;
}

void qvr::progs::gameCode_f()
{
    const char* vrRoot = VR_GameDirectoryRoot("quakevr", 0);
    for(int i = 0; i < com_numbasedirs; i++)
    {
        Con_Printf("base dir %d: %s%s\n", i + 1, com_basedirs[i],
            vrRoot == com_basedirs[i] ? " (its quakevr is the one searched)" : "");
    }
    Con_Printf("game dirs: %s\n", com_gamenames);
    if(!sv.active)
    {
        Con_Printf("game code: checked when a map loads (no game running)\n");
    }
    else
    {
        const bool noticed = gameCodeNotice.empty(); // (its centre print not again)
        gameCodeChecked.clear();                     // (said again)
        checkGameCode(sv_bindings);
        if(noticed)
        {
            gameCodeNotice.clear();
        }
    }
    Cbuf_InsertText("path\n");
}

void qvr::progs::loadNoticeFrame()
{
    // (the loaded game's player in it: the save's warning where it is seen; or the game code's, checkGameCode)
    za::String& notice = !saveInfo.notice.empty() ? saveInfo.notice : gameCodeNotice;
    if(notice.empty() || svs.maxclients < 1 || !svs.clients[0].spawned)
    {
        return;
    }
    MSG_WriteByte(&svs.clients[0].message, svc_centerprint);
    MSG_WriteString(&svs.clients[0].message, notice.cStr());
    notice.clear();
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

// A model named by a string that the engine sends by its precache index (SV_ModelIndex), precached now if it is not:
// its index, or 0 (no model: a file that can't be loaded, warned about) when it can't be. Loads it first, so a missing
// file is never an error. The name is copied (the list keeps the pointer; a QC string may not last).
[[nodiscard]] int ensureModelPrecached(const char* name)
{
    if(!name || !name[0] || name[0] == '*')
    {
        return 0; // (none, or a brush model: the map's, precached with it)
    }
    const int index = modelPrecacheIndex(name);
    if(index >= 0)
    {
        return index;
    }
    if(!VR_AllowLatePrecache() || !Mod_ForName(name, false))
    {
        Con_Warning("model %s can't be loaded: drawn as none\n", name);
        return 0;
    }
    return za::max(VR_LatePrecacheModel(Hunk_Strdup(name, "precache")), 0);
}

// The string fields whose model the engine sends by index: the hands' (.weaponmodel, .weaponmodel2) and the
// holsters' (.holsterweaponmodel0..5).
[[nodiscard]] bool namesSentModel(const ddef_t& d)
{
    if((d.type & ~DEF_SAVEGLOBAL) != ev_string)
    {
        return false;
    }
    const char* name = PR_GetString(d.s_name);
    return !strcmp(name, "weaponmodel") || !strcmp(name, "weaponmodel2") || !strncmp(name, "holsterweaponmodel", 18);
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

// The sounds the saved game had precached that the map loaded now doesn't: the ones precached late (a monster spawned
// mid-game by the test spawner, the toolgun or a delayed spawner, whose spawn function precached its sounds then) are
// precached again, or its sounds would not play in the loaded game (SV_StartSound: "not precached"). Sounds go by name,
// so only the list's names matter, not their order.
void restoreLateSounds()
{
    if(!VR_AllowLatePrecache())
    {
        saveInfo.sounds.clear();
        return;
    }
    int added = 0;
    for(const za::String& name : saveInfo.sounds)
    {
        int i = 1;
        for(; i < MAX_SOUNDS && sv.sound_precache[i]; i++)
        {
            if(!strcmp(sv.sound_precache[i], name.cStr()))
            {
                break;
            }
        }
        if(i >= MAX_SOUNDS)
        {
            Con_Warning("load: sound precache full: %s not precached\n", name.cStr());
            break;
        }
        if(!sv.sound_precache[i])
        {
            sv.sound_precache[i] = Hunk_Strdup(name.cStr(), "precache"); // (the list keeps the pointer)
            added++;
        }
    }
    if(added > 0)
    {
        Con_DPrintf("load: %d late sound precache(s) restored\n", added);
    }
    saveInfo.sounds.clear();
}

// Our models renamed since a save could have been made: the old name, today's. The Thunderbolt's plasma variant and its
// cell's parts were Rogue's name, progs/v_plasma.mdl, which shadowed AD's Plasma Gun (and Rogue's own) in the lookup:
// in a save made before, that name is always ours.
struct RenamedModel
{
    const char* before;
    const char* now;
};
constexpr RenamedModel renamedModels[] = {
    {"progs/v_plasma.mdl", "progs/v_light_plasma.mdl"},
    {"progs/vr_mag_on_v_plasma.mdl", "progs/vr_mag_on_v_light_plasma.mdl"},
    {"progs/vr_magwell_on_v_plasma.mdl", "progs/vr_magwell_on_v_light_plasma.mdl"},
};

[[nodiscard]] bool saveListHas(const char* name)
{
    for(const za::String& m : saveInfo.models)
    {
        if(!q_strcasecmp(m.cStr(), name))
        {
            return true;
        }
    }
    return false;
}

// A save made before a rename (its model list has the old name and not the new one; every game since precaches the new
// one, world.qc): the old name becomes today's in the save's model list and in every string field and global that
// holds it exactly (the hands' .weaponmodel, the holsters', .model), so the loaded game names the model it had. A save
// made since keeps its names: there "progs/v_plasma.mdl" is Rogue's or AD's own file.
void renameOldModels()
{
    for(const RenamedModel& r : renamedModels)
    {
        if(!saveListHas(r.before) || saveListHas(r.now))
        {
            continue;
        }
        for(za::String& m : saveInfo.models)
        {
            if(!q_strcasecmp(m.cStr(), r.before))
            {
                m = za::String{r.now};
            }
        }
        int strings = 0;
        const auto rename = [&](string_t& s) {
            if(s && !q_strcasecmp(PR_GetString(s), r.before))
            {
                s = PR_SetEngineString(r.now); // (a literal: lasts)
                strings++;
            }
        };
        for(int num = 1; num < qcvm->num_edicts; num++)
        {
            edict_t* ent = EDICT_NUM(num);
            if(ent->free)
            {
                continue;
            }
            for(int i = 0; i < qcvm->progs->numfielddefs; i++)
            {
                const ddef_t& d = qcvm->fielddefs[i];
                if((d.type & ~DEF_SAVEGLOBAL) == ev_string)
                {
                    rename(reinterpret_cast<string_t*>(&ent->v)[d.ofs]);
                }
            }
        }
        for(int i = 0; i < qcvm->progs->numglobaldefs; i++)
        {
            const ddef_t& d = qcvm->globaldefs[i];
            if((d.type & ~DEF_SAVEGLOBAL) == ev_string)
            {
                rename(reinterpret_cast<string_t*>(qcvm->globals)[d.ofs]);
            }
        }
        Con_Printf("load: an older save: %s is now %s (%d strings)\n", r.before, r.now, strings);
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
// (A save without the list, made before it was saved, is refused: VR_ReadSaveInfo.)
void rebindLoadedModels()
{
    renameOldModels();
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
            else if(namesSentModel(d))
            {
                // A hand's or holster's model, set by name without setmodel (precached late in the saved game: a
                // derived "#vr" model, the Widowmaker's): precached with the load, before the first stats name it.
                (void)ensureModelPrecached(PR_GetString(reinterpret_cast<int*>(&ent->v)[d.ofs]));
            }
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
    saveInfo.models.clear();
    restoreLateSounds();
    // Everything precached while loading is in the serverinfo that clients receive (none is connected yet).
    qvr::server::onSpawnServerAfterLoad();
}

} // namespace

extern "C" void VR_OnLoadGame()
{
    loadingSaveGame = false;
    VR_EdictIndex_Reset(); // (the edicts parsed from the save)

    // parm17..parm72 were restored with the other globals and still hold the level-start
    // values (QC only rewrites them in SetNewParms/SetChangeParms), so they become the
    // single-player client's stored parms, like parm1..16 do from the savegame header.
    VR_StoreSpawnParms(0);
    // The saved game's map content seed (a saved global; 0 in a save made before them: the map's name alone).
    qvr::srvrandom::restoreMapSeed(sv_bindings.mapContentSeed ? static_cast<za::U32>(za::max(*sv_bindings.mapContentSeed, 0.f)) : 0u);

    // The saved edicts replaced the map's own, numbers and all: what SV_SpawnServer's two frames made for the map's
    // entities (Box3D's bodies, the free places and turns in the hand) would be taken for the loaded ones'. Built
    // again from the loaded edicts (Box3D at its next step; the world's mesh is cached).
    qvr::box3d::reset();
    qvr::physics::resetRigidBodies();

    rebindLoadedModels();
    qvr::brushes::restoreAfterLoad(); // (the saved game's brushes on their entities, their models by the names just bound)

    qvr::walltorch::restoreAfterLoad(); // the map's wall torches the save lacks (made with vr_walltorch 0, now on)
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
            // (a limb model in play: its skins' and normal maps' lookups answered from the map load's listings)
            const bool derived = strstr(name, qvr::limbmodel::suffix) != nullptr;
            if(derived)
            {
                VR_FileCacheDerived(1);
            }
            sv.models[i] = Mod_ForName(name, true);
            if(derived)
            {
                VR_FileCacheDerived(0);
            }
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

extern "C" int VR_ModelIndexNotPrecached(const char* name)
{
    if(!VR_AllowLatePrecache() || !sv.active)
    {
        return -1;
    }
    // A model named by a hand's, a holster's or an entity's string that no path precached (it set the name without
    // setmodel, precache_model or its own ensure): precached now, or none, instead of ending the game. A bug to fix
    // where the name was set (vr_load_models_test checks no load or give needs this).
    Con_Warning("SV_ModelIndex: model %s not precached: precached late\n", name);
    return ensureModelPrecached(name);
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
