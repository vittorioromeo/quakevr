// Arcane Dimensions support (docs/vr-port/ARCANE_DIMENSIONS.md, sections 5 and 15): the two switches, and the bridge's
// functions declared early, for Quake VR's files (combat.qc, client.qc, weapons.qc...) and AD's imported code (QC/ad/,
// whose reviewed patches call the VR_AD_ ones) to call. They are defined in vr_ad_bridge.qc, after AD's files, as they
// use AD's names. Everything AD-specific is there; a call from shared code carries an "AD support" comment.

// AD's data is installed and is the latest release (the engine's vr_ad_status, vr_ad.cpp).
float() VR_Pack_AD =
{
    return cvar("vr_ad_status") == 1;
};

// This map plays by AD's rules: the AD campaign, or a map the engine found made for AD before it spawned (vr_ad_rules,
// vr_ad.cpp). It is set for every spawn, a saved game's too, from the map's own entities.
float() AD_Rules =
{
    return cvar("vr_ad_rules") != 0;
};

// The map's spawnflags mean another ruleset's flags (MG's: Dimension of the Past, of the Machine, Dawn of the Machine;
// or AD's): the id/Hipnotic/Rogue/Honey meanings Quake VR's monsters and entities give them are off (statues, trigger
// spawns, silent wake-ups...). AD support: AD's flags 2/4/8/16... are AD's own in AD maps (decision 19).
float() VR_OtherFlags =
{
    return MG_WorldCampaign() || AD_Rules();
};

// Quake VR's side (vr_ad_bridge.qc): each one does nothing outside AD's rules unless it says otherwise.
void() AD_WorldSpawn;                // worldspawn's end: AD's world setup
void() AD_SyncActivator;             // monster_use: AD's triggers' activator as ours, in an AD map
void() AD_ShareActivator;            // SUB_UseTargets: our activator as AD's, in an AD map
void() AD_Frame;                     // StartFrame
void() AD_PlayerSpawned;             // PutClientInServer's end
void() AD_PlayerPreThink;            // PlayerPreThink
void() AD_PlayerPostThink;           // PlayerPostThink, after the weapons
float() AD_PlayerLanded;             // PlayerPostThink's landing: TRUE when AD's fall rules took it
entity() AD_SelectSpawnPoint;        // SelectSpawnPoint: AD's spawn point (info_player_start2 by startspawn2), or world
float() AD_SetChangeParms;           // SetChangeParms (first): AD's level state into parm57..72; TRUE: an inventory reset (done)
void() AD_DecodeLevelParms;          // DecodeLevelParms: AD's worldspawn inventory keys and map variables
void() AD_ChangelevelSpawn;          // trigger_changelevel: AD's keys (no map: this map; start off; ad_tears' exits)
float() AD_ChangelevelTouch;         // changelevel_touch: FALSE when the exit is off (AD's entity state)
float(void() start) AD_MonsterPrepare; // walk/fly/swimmonster_start: AD's spawn flags and keys on our monsters
void(entity targ, entity attacker) AD_MonsterKilled; // Killed: AD's keys on our monsters (deathtarget)
float(entity e) AD_Run;              // AD's code owns this entity's damage, pain and death (AD's monsters, breakables)
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) AD_Damage; // T_DamageImpl: to AD's
float(entity p) AD_Pellets;          // whether `p`'s shotguns fire AD's projectile pellets (vr_ad_projectiles)
void(float xHand, float shotcount, vector dir, vector spread, vector muzzle) AD_FirePellets; // FireBullets
float(float shotcount, vector dir, vector spread, vector src) AD_EnemyPellets; // EnemyFireBullets: TRUE when AD's pellets went
float(entity e) AD_TestWaiting;      // vr_test_monsters: one of AD's monsters waiting for its trigger
float(entity p) AD_TestWake;         // vr_test_monsters: AD's monster (self) hunts p; TRUE: it was one
float() AD_AxeScale;                 // W_FireAxe: the Shadow Axe's blow (1 without it)
float(float pellets) AD_WidowmakerPellets; // W_FireSuperShotgun: the Widowmaker's 21 (a reserve shell taken), or pellets
float() AD_Widowmaker;               // vr_reload.qc: self's super shotgun is the Widowmaker (reloaded at the hip)
float(float xFlags) AD_PlasmaGun;    // W_Attack: the lightning gun is AD's Plasma Gun
void(float xHand) AD_FirePlasma;     // W_Attack: its bolt
void(entity p) AD_UpgradePickedUp;   // weapon_touch: an AD upgrade's pickup
float(entity p) AD_MegaRot;          // health_touch: AD's megahealth rot in an AD map; TRUE: taken
void(entity xItem) AD_ItemTaken;     // VR_PickupObj_Taken: AD's item respawn in an AD map
float() AD_ItemStartOff;             // StartItem: AD's STARTOFF item waits for its trigger; TRUE: it waits

// AD's side: the reviewed patches of Misc/quakevr/ad_import.py call these (never renamed).
float() VR_AD_Extensions;
void(float a) VR_AD_WaterAlpha;
float(entity client, float density, vector colour) VR_AD_Fog;
float(entity targ, entity inflictor, entity attacker, float damage, float checkarmor) VR_AD_DamageOurs;
void(entity targ, entity inflictor, entity attacker, float take) VR_AD_Hit;
float() VR_AD_PathCornerTouch;
float(entity pellet) VR_AD_PelletDamage;
float() VR_AD_WeaponTouch;
