// Arcane Dimensions' monsters with Quake VR's treatment (docs/vr-port/ARCANE_DIMENSIONS.md, section 17; decisions 1, 13
// and 14): the fields Quake VR's files read and the functions they and AD's patched code call, declared early. Defined in
// vr_ad_monsters.qc, after AD's files (they use AD's names). Every call from a shared file carries an "AD support" note.
//
// An AD monster is registered as it starts (AD's monster_start: VR_AD_MonsterSetup) with its data, by its class: its head
// zone (positional damage, head shots, beheading), its head gib, its corpse, its knockdown (a death played backwards),
// whether it strikes in melee (parries) and wears armour. Its ragdoll rig is the engine's (vr_ad_rigs.inc, by its model);
// its ragdoll settings are its nearest Quake VR class's (vr_ad_monsters.cpp). AD's own AI, pain and death run it; AD's
// T_Damage and Killed call Quake VR's hooks (ad_import.py's patches: VR_AD_KdSave, VR_AD_PainDown, VR_AD_Dying,
// VR_AD_Died, VR_AD_BodySolid).

float VR_ADM_ON = 1;     // registered (any AD monster VR_AD_MonsterSetup knows)
float VR_ADM_MELEE = 2;  // strikes in melee: parried (VR_Parry_MeleeMonster)
float VR_ADM_ARMOUR = 4; // in plate or metal: a blunt blow sounds on armour (VR_Hit_Armoured)
float VR_ADM_ZOMBIE = 8; // a zombie: only gibbing or beheading kills it for good
float VR_ADM_NOHEAD = 16; // never beheaded (its own death throws its head: the skull wizard's); its head zone kept
.float vr_ad_mon;        // VR_ADM_* bits; 0: not one of AD's monsters (or one with none of Quake VR's treatment)
.float vr_ad_headf;      // its head zone (PositionalHead): forward, up, radius (0: none) on its stand frame
.float vr_ad_headu;
.float vr_ad_headr;
.float vr_ad_headl;      // its head zone's middle to its left (0 for most: the zombie knight's head is off its middle)
.float vr_ad_corpseh;    // its corpse's health cvar (VR_Corpse_Parts' vr_corpse_health_<monster>); 0: no corpse gibbing

// Quake VR's side calls these.
float(entity e) VR_AD_PositionalHead;    // PositionalHead: its head zone; FALSE none
string(entity e) VR_AD_HeadModel;        // VR_Decap_HeadModel: its head gib ("" never beheaded)
float(entity e) VR_AD_CorpseParts;       // VR_Corpse_Parts: its corpse's head, gibs and health; FALSE none
void(entity e, entity attacker, float damage, float xStagger) VR_AD_Pain; // a bash's or a parry's stagger: AD's pain function
float(entity e) VR_AD_IsRunning;         // a knockdown's get-up: its think is its run (AD's th_run)
void(entity e, entity by) VR_AD_ZombieKilled; // VR_Decap_After: an AD zombie beheaded or cut: killed for good, lying
float(entity e, entity by) VR_AD_NeckSnapKilled; // VR_FoeGrab_NeckSnap: an AD monster killed by AD's Killed; FALSE not AD's
void() VR_AD_RunNow;                     // a knockdown's get-up over: self runs (AD's th_run)
string(float choice) VR_AD_DispenserModel; // VR_EnemyDispenser_Model: an AD monster (200..), "" none
float(float choice) VR_AD_DispenserSpawn;  // VR_EnemyDispenser_Spawn: TRUE when it was one
void(float choice) VR_AD_DispenserBossPrepare; // ... before it: a scripted boss (the judicator, the ice golem) named
void() VR_AD_DispenserBossWake;            // ... after it: that boss woken (its map's trigger's use), to fight now
void() VR_AD_LiveBossStart;                // AD_SpawnLive_Finish (the toolgun): such a boss spawned without a name, woken
#ifdef QVR_TESTS
void() AD_MonstersTestFrame;             // StartFrame: vr_ad_mtest (vr_ad_monsters_test.qc)
#endif

// AD's patched code calls these (ad_import.py MON_PATCHES; the renaming keeps every VR_AD_ name).
void() VR_AD_MonsterSetup;               // monster_start: self's data
void(entity targ, entity inflictor) VR_AD_KdSave; // T_Damage, before the damage: a knocked-down one's frame and think kept
float(entity targ) VR_AD_PainDown;       // T_Damage, before the pain: knocked down, no pain animation; TRUE done
void(entity attacker) VR_AD_Dying;       // Killed, before the death function (self dying): beheading, limbs, shock
void() VR_AD_Died;                       // Killed, after it: a knockdown's death at once, the corpse, the blood pool
void() VR_AD_BodySolid;                  // monster_death_postcheck: its body touchable, not a trigger (its ragdoll)
void() VR_AD_Gibbed;                     // monster_ThrowGib (self gibbed by AD's code): our gore, small gibs, limbs
void(entity g, float xNo) VR_AD_GibThrown; // ThrowGib: AD's gib or head `g` (kind xNo) made a prop of ours, as id's

// AD support: Quake VR's stealth AI on AD's monsters (vr_ad_stealth.qc, after AD's files; STEALTH.md, "Arcane
// Dimensions"). vr_stealth.qc asks these for a monster whose stl_ad says it is AD's:
float(entity m) VR_AD_StealthKind;       // 0 not AD's; 1 AD's (walks); 2 AD's, never walks (a turret); -1 AD's, excluded
float(entity m) VR_AD_StealthPassive;    // AD's passive mode on (it ignores him until a fight starts): no stealth rules
void(float xWalking) VR_AD_StealthAnim;  // self's stand (paused for good) or walk animation: AD's thinks
entity(entity m) VR_AD_StealthPath;      // its path (AD's movetarget)
void(entity m, entity xPath) VR_AD_StealthSetPath;
float(entity m) VR_AD_StealthOnPath;     // walking its path (a path corner, not paused for good)
void() VR_AD_StealthFound;               // self Hostile at self.enemy: AD's FoundTarget
void(entity m) VR_AD_StealthVoice;       // its "huh?": its idle sound, a little quieter
void(entity m) VR_AD_StealthTestReset;   // vr_stealth_test's reset: AD's own enemy, path and sight relay cleared
void() VR_AD_StealthTestUse;             // vr_stealth_test 7: AD's monster_use on self (activator)
// AD's patched AI calls these (ad_import.py FUNC_PATCHES).
float() VR_AD_StealthTick;               // FindTarget's start: Quake VR's tick; TRUE: Hostile now
float(entity client) VR_AD_StealthSight; // FindTarget: TRUE when the meter sees this client (a player, not AD's relay)
float(entity client) VR_AD_StealthSee;   // and its sighting: TRUE Hostile (AD's FoundTarget ran)
void() VR_AD_StealthHostile;             // FoundTarget, FoundHuntTarget: Hostile
float() VR_AD_StealthStand;              // ai_stand after FindTarget: TRUE the Alert state took the think
float(float dist) VR_AD_StealthWalk;     // ai_walk after FindTarget: the same
float(float xVis) VR_AD_StealthLose;     // ai_run: TRUE lost him a while (Alert)
