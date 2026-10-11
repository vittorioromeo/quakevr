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

// The vanilla monsters' projectiles AD gives a speed by skill (AD_MonsterSpeed).
float AD_PROJ_HKNIGHT = 1; // the hell knight's spikes (AD: 200 + 50 a skill level; Quake: 300)
float AD_PROJ_WIZARD = 2;  // the scrag's (AD: 500 + 50; Quake: 600)
float AD_PROJ_OGRE = 3;    // the ogre's grenades (AD: 500 + 50; Quake: 600)
float AD_PROJ_ZOMBIE = 4;  // the zombie's flesh (AD: 550 + 25; Quake: 600)

// Quake VR's side (vr_ad_bridge.qc): each one does nothing outside AD's rules unless it says otherwise.
float(float idSpeed, float kind) AD_MonsterSpeed; // a vanilla monster's projectile: AD's speed for the skill by the Projectile Rules, or idSpeed
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

// AD's data is there for this map (AD_WorldSpawn: VR_Pack_AD and its folder mounted): AD's artifacts, pickups and
// debuffs may be met in any map (section 16).
nosave float vr_ad_data;

// The follow-ups of decisions 16-30 (section 16; vr_ad_items.qc, vr_ad_player.qc): each does nothing without AD's
// data or the artifact, debuff or rule it is for.
vector(vector spread) AD_SharpSpread;  // FireBullets: the Sharp Shooter's spread
void(entity xBody) AD_NailPierce;      // spike_touch, superspike_touch: the Nail Piercer's nail on through the body
float(entity targ, entity inflictor, float damage) AD_ArtifactDamage; // T_DamageDeal: the Blast Belt (-1: none taken)
float(entity p) AD_LavaShield;         // WaterMove: TRUE: AD's Lava Shield keeps the lava's damage off
float(entity p) AD_LavaShieldOn;       // VR_Burn_LavaPlayers: the same, no reminder
float() AD_JumpBoots;                  // PlayerJump: AD's Jump Boots' jumps; TRUE: taken
void() AD_WorldDefaults;               // AD_WorldSpawn, outside AD maps: AD's defaults for its entities and debuffs
void() AD_ArtifactsTick;               // AD_PlayerPostThink: the artifacts' time and the HUD's readings
void() AD_DebuffsTick;                 // AD_PlayerPostThink: AD's debuffs (poison, bleeding; burning is ours)
void(float request, entity p) AD_RulesTest; // vr_ad_test 10 and up (AD_Test): the follow-ups' tests

// AD's ammo resistance (decision 18; vr_ad_player.qc AD_Resist): the kind of ammunition the damage now dealt is, where
// the inflictor does not tell (VR_LightningHit: cells); 0 worked out from the inflictor and the hit kind.
float AD_AMMO_SHELLS = 1;
float AD_AMMO_NAILS = 2;
float AD_AMMO_ROCKETS = 3;
float AD_AMMO_CELLS = 4;
float vr_ad_ammo;
float vr_ad_ourdamage; // AD's code deals this damage to one of ours (VR_AD_DamageOurs): AD resisted it already
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) AD_Resist; // T_DamageImpl

// AD's Evil skill (decision 30; vr_ad_player.qc): Nightmare plus vr_ad_evil, while AD's data is there, in any map.
float vr_ad_evil_on;   // this game is Evil (saved: a loaded game's choice sets vr_ad_evil again)
float() AD_Evil;       // VR_BaseHealth, VR_MegaHealthCap, AD_AxeScale: Evil plays now
void(float s) VR_AD_SetSkill; // AD's skill choices (reviewed patches): skill 4 is Nightmare plus vr_ad_evil
void() AD_EvilStart;           // AD_WorldSpawn
void() AD_EvilFrame;           // AD_Frame
void() AD_LoadedGame;          // OnLoadGame: the loaded game's choice

// AD's side: the reviewed patches of Misc/quakevr/ad_import.py call these (never renamed).
float() VR_AD_Extensions;
void(float a) VR_AD_WaterAlpha;
float(entity client, float density, vector colour) VR_AD_Fog;
float(entity targ, entity inflictor, entity attacker, float damage, float checkarmor) VR_AD_DamageOurs;
void(entity targ, entity inflictor, entity attacker, float take) VR_AD_Hit;
float() VR_AD_PathCornerTouch;
float(entity pellet) VR_AD_PelletDamage;
float() VR_AD_WeaponTouch;
float(entity xItem) VR_AD_ArtifactItems;
.float vr_climb_ladder; // a ladder's volume, climbed by hand (the engine's vr_climb.cpp ladderHold; VR_AD_LadderSpawned)
void() VR_AD_LadderSpawned;
float(entity targ) VR_AD_BurnDebuff;
float(entity xFire, entity xVictim) AD_BurnHurt; // VR_Burn_Think: an AD debuff's fire's damage (TRUE: dealt, ours not)
