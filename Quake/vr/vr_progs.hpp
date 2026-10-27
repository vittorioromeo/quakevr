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

// Resolved for sv.qcvm each time progs are loaded.
struct Bindings
{
    bool isVrProgs{false};
    FieldOffsets fields;
    MeleeHistoryFields melee;

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
    func_t AxeStick_Stuck{0}; // a thrown axe's blade stuck in something (QC vr_axestick.qc; vr_axestick.cpp)
    // The ragdolls' per-frame QC (vr_box3d_step.cpp's callBodyHits, callRagImpacts, monsterFell; vr_box3d_ragdoll.cpp's
    // makeRagdollRoom): resolved here, not by name each frame (findFunction scans every function).
    func_t KdTrig_BodyHit{0};
    func_t RagImpact_Hit{0};
    func_t Flail_Hit{0}; // AD support: the death sergeant's flail's head striking (QC vr_ad_melee.qc; vr_box3d_flail.cpp)
    func_t Flail_Rattle{0}; // ... its chain rattling, the head swung fast
    func_t Monster_Fall{0};
    func_t Knockdown_MakeRoom{0};
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
