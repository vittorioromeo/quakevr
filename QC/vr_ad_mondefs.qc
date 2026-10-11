// Arcane Dimensions' monsters with Quake VR's treatment (docs/vr-port/ARCANE_DIMENSIONS.md, section 16; decisions 1, 13
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
.float vr_ad_mon;        // VR_ADM_* bits; 0: not one of AD's monsters (or one with none of Quake VR's treatment)
.float vr_ad_headf;      // its head zone (PositionalHead): forward, up, radius (0: none) on its stand frame
.float vr_ad_headu;
.float vr_ad_headr;
.float vr_ad_corpseh;    // its corpse's health cvar (VR_Corpse_Parts' vr_corpse_health_<monster>); 0: no corpse gibbing

// Quake VR's side calls these.
float(entity e) VR_AD_PositionalHead;    // PositionalHead: its head zone; FALSE none
string(entity e) VR_AD_HeadModel;        // VR_Decap_HeadModel: its head gib ("" never beheaded)
float(entity e) VR_AD_CorpseParts;       // VR_Corpse_Parts: its corpse's head, gibs and health; FALSE none
void(entity e, entity attacker, float damage, float xStagger) VR_AD_Pain; // a bash's or a parry's stagger: AD's pain function
float(entity e) VR_AD_IsRunning;         // a knockdown's get-up: its think is its run (AD's th_run)
void() VR_AD_RunNow;                     // a knockdown's get-up over: self runs (AD's th_run)
string(float choice) VR_AD_DispenserModel; // VR_EnemyDispenser_Model: an AD monster (200..), "" none
float(float choice) VR_AD_DispenserSpawn;  // VR_EnemyDispenser_Spawn: TRUE when it was one
void() AD_MonstersTestFrame;             // StartFrame: vr_ad_mtest (vr_ad_monsters_test.qc)

// AD's patched code calls these (ad_import.py MON_PATCHES; the renaming keeps every VR_AD_ name).
void() VR_AD_MonsterSetup;               // monster_start: self's data
void(entity targ) VR_AD_KdSave;          // T_Damage, before the damage: a knocked-down one's frame and think kept
float(entity targ) VR_AD_PainDown;       // T_Damage, before the pain: knocked down, no pain animation; TRUE done
void(entity attacker) VR_AD_Dying;       // Killed, before the death function (self dying): beheading, limbs, shock
void() VR_AD_Died;                       // Killed, after it: a knockdown's death at once, the corpse, the blood pool
void() VR_AD_BodySolid;                  // monster_death_postcheck: its body touchable, not a trigger (its ragdoll)
