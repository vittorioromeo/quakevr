// vr_progs.hpp -- Quake VR's view of the server QuakeC VM.
//
// The VR progs keep the vanilla system-defs layout (CRC 5927); everything VR-specific is an
// ordinary QC field, global or function that the engine resolves by name when progs load.
// A progs without them (e.g. vanilla id1) simply runs with VR gameplay disabled.

#pragma once

#include "vr_engine.hpp"

namespace qvr::progs
{

inline constexpr int firstExtSpawnParm = 17;
inline constexpr int lastExtSpawnParm = 74; // (51..56: Dawn of the Machine's upgrades and bloody weapons, QC/vr_mg3_defs.qc; 57..72: Arcane Dimensions' level state, QC/vr_ad_bridge.qc; 73: the magnetic holsters' bindings, QC/vr_magholster.qc; 74: bullet time's meter, QC/client.qc)
inline constexpr int numExtSpawnParms = lastExtSpawnParm - firstExtSpawnParm + 1;

// Offsets (in floats, from edict_t::v) of the VR entity fields; -1 when absent.
struct FieldOffsets
{
#define QVR_FIELD(name) int name{-1};
#include "vr_fields.inc"
#undef QVR_FIELD
};

// The melee history's fields (QC vr_melee.qc: a hand's blow keeps its last poses in a ring; the melee builtins read it):
// the arrays' first elements (FTEQCC names them "mh_hdt[0]"...), each VR_MELEE_HISTORY long (vr_melee_shared.h). `valid`
// when all were found, the arrays that long.
struct MeleeHistoryFields
{
    bool valid{false};
    int hfar{-1};   // .vector mh_hfar[]: the far end's velocity at each pose
    int hgrip{-1};  // .vector mh_hgrip[]: the grip's
    int hwrist{-1}; // .vector mh_hwrist[]: the wrist's
    int hdt{-1};    // .float mh_hdt[]: each pose's time step (0: none, the ring's end)
    int hi{-1};     // .float mh_hi: the newest pose's index
    int vgrip{-1};  // .vector mh_vgrip, mh_vfar: the grip's and the far end's velocity now
    int vfar{-1};
    int rgrip{-1};  // .vector mh_rgrip: the grip now (from the head, in the play space)
};

// One hand's fields of the player (handFields: index 1 the main hand's, 0 the off hand's), from FieldOffsets.
struct HandFields
{
    int pos{-1};          // .handpos / .offhandpos
    int rot{-1};          // .handrot / .offhandrot
    int held{-1};         // .mainhand_held / .offhand_held
    int muzzle{-1};       // .muzzlepos / .offmuzzlepos
    int shotRot{-1};      // .shotrot / .offshotrot
    int loadPortPos{-1};  // .loadportpos / .offloadportpos
    int loadPortAxis{-1}; // .loadportaxis / .offloadportaxis
    int loadPortFace{-1}; // .loadportface / .offloadportface
    int throwVel{-1};     // .handthrowvel / .offhandthrowvel
    int avel{-1};         // .handavel / .offhandavel
};

// Resolved for sv.qcvm each time progs are loaded.
struct Bindings
{
    bool isVrProgs{false};
    FieldOffsets fields;
    MeleeHistoryFields melee;
    HandFields hands[2]; // [0] the off hand's, [1] the main hand's

    func_t OnSpawnServerBeforeLoad{0};
    func_t OnSpawnServerAfterLoad{0};
    func_t OnLoadGame{0};
    func_t Motion_Sample{0}; // the motion recorder's (QC vr_motion.qc)
    func_t Motion_Equip{0};
    func_t Dummy_Replay{0}; // a motion take's strike of the training dummy (QC vr_dummy.qc)
    func_t Dummy_RetypeAll{0}; // the training dummies as vr_dummy_type has them, now (QC vr_dummy.qc)
    func_t Probe_Kinds{0}; // one of each kind of monster that can appear later, as the map spawns (QC vr_probe.qc)
    func_t Carry_Handtouch{0}; // a thing a hand can carry (QC vr_carry.qc VR_Carry_Setup): taken by the fist's touch
    func_t Scene_Clean{0}; // Debug > Cheats and Recording's cleanups (QC vr_cheats.qc; vr_cheats.cpp)
    func_t Ragdoll_Handtouch{0}; // a ragdoll's limb under a hand (QC vr_carry.qc; vr_box3d.cpp gives it to a ragdoll)
    // A carried prop's touch: QC forcegrabbable_touch and those that only call it (a rock's or a brick's, a crate's
    // piece's, a thrown weapon's: vr_physics.cpp's touch spares the calls that can do nothing).
    func_t propTouches[4]{};
    func_t ClipSizeOf{0}; // a weapon's own magazine size, by id and record (QC VR_ClipSizeOf: vr_server.cpp, the ammo screens' warnings)
    func_t AxeStick_Stuck{0}; // a thrown axe's blade stuck in something (QC vr_axestick.qc; vr_axestick.cpp)
    // The ragdolls' per-frame QC (vr_box3d_step.cpp's callBodyHits, callRagImpacts, monsterFell; vr_box3d_ragdoll.cpp's
    // makeRagdollRoom): resolved here, not by name each frame (findFunction scans every function).
    func_t KdTrig_BodyHit{0};
    func_t RagImpact_Hit{0};
    func_t Flail_Hit{0}; // AD support: the death sergeant's flail's head striking (QC vr_ad_melee.qc; vr_box3d_flail.cpp)
    func_t Flail_Rattle{0}; // ... its chain rattling, the head swung fast
    func_t Monster_Fall{0};
    func_t Knockdown_MakeRoom{0};
    func_t Stamina_Max{0};  // the stamina pool's size and what a player has left (QC vr_melee.qc: staminaMax, staminaLeft)
    func_t Stamina_Left{0};
    // The monster grab's QC (vr_foegrab.cpp's callQc and head zone), resolved here, not by name per call.
    struct FoeGrab
    {
        func_t HeadZone{0};
        func_t HeadLeft{0};
        func_t Released{0};
        func_t Strength{0};
        func_t Taken{0};
        func_t Throw{0};
        func_t HandOver{0};
        func_t Manhandle{0};
        func_t BreakFree{0};
        func_t NeckSnap{0};
        func_t Lift{0};
        func_t MonsterMass{0}; // VR_Grapple_MonsterMass
        func_t LegSweepDown{0}; // VR_LegSweep_Down
    } foegrab;
    // The arena's perk cards (QC vr_arena_perks.qc; vr_arenacards.cpp's poll and debug offer), resolved once per map.
    struct ArenaCards
    {
        func_t CardPick{0};
        func_t CardSerial{0};
        func_t CardCount{0};
        func_t CardBit{0};
        func_t Name{0};
        func_t What{0};
        func_t Model{0};
        func_t Colour{0};
        func_t CardTitle{0};
        func_t PoolSize{0};
        func_t CardsSeen{0};
        func_t DebugOffer{0};
    } arenaCards;

    float* spawnServerFromSaveFile{nullptr};
    // vr_music_track: the CD track target_vr_music switched to (0: the map's own); sent at a connect (a loaded game)
    float* musicTrack{nullptr};
    float* spawnLive{nullptr}; // vr_spawn_live: 1 round a spawn function run in a live game (box3d::spawnClass)
    float* playerTimeOffset{nullptr}; // vr_player_time_offset: the player's clock ahead of time (vr_timescale.cpp)
    float* handTimeScale{nullptr};    // vr_hand_time_scale: the hands' velocities to real time (timescale::handScale)
    float* extSpawnParms[numExtSpawnParms]{};
};

[[nodiscard]] const Bindings& bindings();

[[nodiscard]] inline const FieldOffsets& fields()
{
    return bindings().fields;
}

// Field accessors. Callers must check the offset is valid (isVrProgs guarantees the fields
// the engine needs; fall back gracefully for optional ones).
[[nodiscard]] inline float* fieldPtr(edict_t* ent, int ofs)
{
    return reinterpret_cast<float*>(&ent->v) + ofs;
}

[[nodiscard]] inline float& fieldFloat(edict_t* ent, int ofs)
{
    return *fieldPtr(ent, ofs);
}

[[nodiscard]] inline int& fieldInt(edict_t* ent, int ofs) // func_t, string_t, entity
{
    return *reinterpret_cast<int*>(fieldPtr(ent, ofs));
}

[[nodiscard]] inline float fieldFloatOr(edict_t* ent, int ofs, float fallback)
{
    return ofs >= 0 ? fieldFloat(ent, ofs) : fallback;
}

// Optional fields: absent ones read as zero, and are not written.
[[nodiscard]] inline glm::vec3 fieldVec(edict_t* ent, int ofs)
{
    if(ofs < 0)
    {
        return glm::vec3{0.f};
    }
    const float* v = fieldPtr(ent, ofs);
    return {v[0], v[1], v[2]};
}

// Whether `ent` is the firing range's training dummy standing (QC vr_dummy.qc: its classname is its enemy's,
// vr_dummy_type's; .vr_dummy_kind marks it).
[[nodiscard]] inline bool isTrainingDummy(edict_t* ent)
{
    return fieldFloatOr(ent, fields().vr_dummy_kind, 0.f) > 0.f;
}

// The class a motion take knows a target by: its classname, "vr_dummy" for the training dummy whatever enemy it stands
// as (the takes' "target" and mon_class: a take of any enemy type replays against the dummy).
[[nodiscard]] inline const char* targetClass(edict_t* ent)
{
    return isTrainingDummy(ent) ? "vr_dummy" : PR_GetString(ent->v.classname);
}

[[nodiscard]] inline func_t fieldFunc(edict_t* ent, int ofs)
{
    return ofs >= 0 ? static_cast<func_t>(fieldInt(ent, ofs)) : 0;
}

inline void setFieldVec(edict_t* ent, int ofs, const glm::vec3& v)
{
    if(ofs >= 0)
    {
        float* f = fieldPtr(ent, ofs);
        f[0] = v.x;
        f[1] = v.y;
        f[2] = v.z;
    }
}

inline void setFieldFloat(edict_t* ent, int ofs, float value)
{
    if(ofs >= 0)
    {
        fieldFloat(ent, ofs) = value;
    }
}

// A function of the current qcvm by name (0 when absent).
[[nodiscard]] func_t findFunction(const char* name);

// ---------------------------------------------------------------------------------------------------------------------
// Canonical engine-access helpers (docs/vr-port/CODE_STYLE.md, "Canonical helpers").

// The server's VM made current for a scope (RAII; as a console command or the host frame touching its edicts does).
struct ServerVm
{
    qcvm_t* old{nullptr};
    ServerVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~ServerVm() { PR_PopQCVM(old); }
    ServerVm(const ServerVm&) = delete;
    ServerVm& operator=(const ServerVm&) = delete;
};

// The local (listen) server's player, client 0's edict; null when not hosting or that client is not in the game.
[[nodiscard]] inline edict_t* localPlayer()
{
    return sv.active && svs.maxclients >= 1 && svs.clients[0].active ? svs.clients[0].edict : nullptr;
}

// The local server's edict `num` in use (null: not hosting, out of range or free). Any current VM.
[[nodiscard]] inline edict_t* edictAt(int num)
{
    if(!sv.active || num <= 0 || num >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* ed = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
    return ed->free ? nullptr : ed;
}

// fn(edict_t*) for each edict in use of the current VM, from number `first` up.
template <typename F>
void forEachEdict(int first, F&& fn)
{
    for(int i = first; i < qcvm->num_edicts; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free)
        {
            fn(e);
        }
    }
}

// The edict in use of the current VM from number `first` up whose distance(e) is the least below `limit` (strictly:
// the first such on a tie; distance returns `limit` or more to skip one), or null; `bestOut` its distance (else limit).
template <typename F>
[[nodiscard]] edict_t* nearestBy(int first, float limit, F&& distance, float* bestOut = nullptr)
{
    edict_t* best = nullptr;
    float bestDist = limit;
    forEachEdict(first, [&](edict_t* e) {
        const float d = distance(e);
        if(d < bestDist)
        {
            bestDist = d;
            best = e;
        }
    });
    if(bestOut)
    {
        *bestOut = bestDist;
    }
    return best;
}

// QC function `fn` of the current VM run with `self` the world and up to three float parameters (time set to the VM's);
// false (nothing run) when fn is 0. qcFloat, qcText and qcVec read its return (0, "", `fallback` when not run).
inline bool callQc(func_t fn, float a = 0.f, float b = 0.f, float c = 0.f)
{
    if(!fn)
    {
        return false;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM0) = a;
    G_FLOAT(OFS_PARM1) = b;
    G_FLOAT(OFS_PARM2) = c;
    PR_ExecuteProgram(fn);
    return true;
}

[[nodiscard]] inline float qcFloat(func_t fn, float a = 0.f, float b = 0.f, float c = 0.f)
{
    return callQc(fn, a, b, c) ? G_FLOAT(OFS_RETURN) : 0.f;
}

// The players' stamina pool (parries, shoves, blows and a climb's hang spend it), as QC owns it (vr_melee.qc
// VR_Stamina_Max, VR_Stamina_Left; his decision D8): its size (at least 1) and what `p` has left (0 .. staminaMax). On
// the server's VM; the caller's self, parameters and return are kept (a builtin's code may ask).
[[nodiscard]] float staminaMax();
[[nodiscard]] float staminaLeft(edict_t* p);

inline void qcText(char* dst, size_t size, func_t fn, float a = 0.f)
{
    const char* s = callQc(fn, a) ? PR_GetString(G_INT(OFS_RETURN)) : "";
    q_strlcpy(dst, s ? s : "", size);
}

// A builtin's (or a QC call's) vector parameter at `ofs` (OFS_PARM0...), its vector return now, and setting them.
[[nodiscard]] inline glm::vec3 vecParm(int ofs)
{
    const float* v = G_VECTOR(ofs);
    return {v[0], v[1], v[2]};
}

[[nodiscard]] inline glm::vec3 returnedVec()
{
    return vecParm(OFS_RETURN);
}

inline void setVecParm(int ofs, const glm::vec3& v)
{
    float* f = G_VECTOR(ofs);
    f[0] = v.x;
    f[1] = v.y;
    f[2] = v.z;
}

inline void returnVec(const glm::vec3& v)
{
    setVecParm(OFS_RETURN, v);
}

[[nodiscard]] inline glm::vec3 qcVec(func_t fn, float a = 0.f, const glm::vec3& fallback = glm::vec3{0.f})
{
    return callQc(fn, a) ? returnedVec() : fallback;
}

// The player's per-hand fields (the main hand's or the off hand's: `hand` nonzero is the main hand, as `hand ? main :
// off` reads), -1 when absent.
[[nodiscard]] inline const HandFields& handFields(int hand)
{
    return bindings().hands[hand ? 1 : 0];
}

// Whether the progs carry the QC tests (QC/vr_macros.qc QVR_TESTS; the release flavour leaves them out). True too while
// no progs are loaded (the main menu before any map): unknown, so nothing is claimed missing.
[[nodiscard]] bool qcTestsPresent();

// Binds the VR builtins declared "= #0" in QC (vr_builtins.cpp).
void bindBuiltins();

// Clears per-map builtin state (world text, ...) when a new server spawns.
void resetBuiltinState();
// ED_Free (VR_OnEdictFree): a wounded entity's removal sent to the clients (QVR_SVC_WOUNDCLEAR).
void onEdictFree(edict_t* ed);
// A wound on entity num's model at org going dir (unit), QVR_WOUND_* kind (QC vr_wounds.qc), amount 0..255, extra: QC's
// woundevent sent from the engine (a held stump's blood on the hand: vr_box3d.cpp).
void sendWound(int num, const float* org, const float* dir, int kind, int amount, int extra);
// QC's particle2(org, dir, preset, count) sent from the engine (a native of QuakeC: vr_propthink.cpp).
void sendParticle2(const float* org, const float* dir, int preset, int count);
// vr_test_remove <entity number>: the entity removed, as QC's remove() (tests: its slot reused).
void testRemove_f();
// vr_model_check [1]: every entity's .modelindex against its .model, and the client's models against the server's
// (saved games and late precaches: Misc/quakevr/precache/precache_test.sh).
void modelCheck_f();
// The server's frame: a loaded save's warning (another build's: VR_ReadSaveInfo) centre printed once the player is in;
// or the game code's (progs.dat not this build's: the check at each progs load).
void loadNoticeFrame();
// vr_gamecode: where the game code (progs.dat) comes from and whether it is this build's (said again), the base dirs and
// the search path (Debug > Diagnostics; a player's "guns only props" report).
void gameCode_f();

} // namespace qvr::progs

namespace qvr::progs { void propQueriesTest_f(); void forcegrabGridStats_f(); }
