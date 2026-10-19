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

// AD support: independent safe feature scope; does not change map detection or required world logic.
float(float scope) VR_AD_FeatureScope =
{
    return cvar("vr_ad_mounted") && (scope == 2 || (scope == 1 && AD_Rules()));
};

// AD's weapon upgrades' scope (Weapon Upgrades, vr_ad_upgrades): 1 ("With AD Content") is AD maps, and any map while
// Additional Content is Everywhere (vr_ad_everywhere): an upgraded weapon is AD's content, keeping its rules wherever it
// is (decision 9), its pickups placed or spawned (the toolgun) in any map. 2 any map; 0 none.
float() AD_UpgradeScope =
{
    float s = cvar("vr_ad_upgrades");
    return (s == 1 && cvar("vr_ad_everywhere")) ? 2 : s;
};
void() AD_WeaponsOnDemand; // AD's weapons in a map outside AD's rules: their sounds (vr_ad_enemyguns.qc)
string(float ammo, float big, string fallback) AD_AmmoModel;
void(vector xSize) AD_AmmoBoxFit;

// (VR_OtherFlags, whose meaning the map's spawnflags have: vr_spawnflags.qc, after this file.)

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
float(entity e) AD_HasMelee;         // Show Melee Arcs (fight.qc): one of AD's monsters with a melee attack
float(entity e) AD_HasMissile;       // Show Aim Cones (vr_aimcone.qc): one of AD's monsters with a ranged attack
float(entity e) AD_IsBoss;           // the aim cone (vr_aimcone.qc): one of AD's bosses (its shots left alone)
// AD's weapon upgrades (decision 10): every question goes through AD_WeaponUpgraded(player, weapon record, bit).
float(entity xPlayer, entity xInst, float xBit) AD_WeaponUpgraded;
float(float xHand) AD_AxeScale;      // W_FireAxe: the Shadow Axe's blow of the axe in xHand (1 without it)
void(float xHand, entity xVictim) AD_AxeMark; // W_FireAxe: AD's mark of the axe that hit (.ad_axhitme: 2 the Shadow Axe)
void(float xHand, entity b) AD_ShadowAxeChop; // VR_Melee_Strike: the Shadow Axe chops the bodies lying on the floor
float(entity p, float xBit) AD_CarriesUpgrade; // p carries a weapon with AD's upgrade xBit (a hand's or a holster's)
void(float request, entity p) AD_UpgradesTest; // vr_ad_test 140 (vr_ad_upgrades_test.qc)
string(float xWid, float xFlags, entity xInst) AD_WeaponModel; // a weapon's model: an upgraded one's AD VR model (vr_ad_bridge.qc)
float(entity xInst) AD_Widowmaker;   // the super shotgun of record xInst is the Widowmaker (its tube, its port: vr_reload.qc)
float(entity p, float xHand) AD_WidowmakerIn; // the same of the gun p holds in xHand
void(float xHand) AD_FireWidowmaker; // W_Attack: its shot (a shell from its tube)
float(float xHand, float xFlags) AD_PlasmaGun; // W_Attack: the lightning gun in xHand is AD's Plasma Gun
float(entity xInst) AD_PlasmaGunInst; // the lightning gun of record xInst is the Plasma Gun (its reload override)
void(float xHand) AD_FirePlasma;     // W_Attack: its bolt
void(entity xPickup, entity p, float xHand) AD_UpgradeGiven; // weapon_touch: its weapon given to p's hand xHand: the upgrade on it
float(entity p) AD_PackWeaponUpgrades;   // SetChangeParms: the upgrades of p's weapons (hands, holsters) as one parm
void(entity p, float xPacked) AD_UnpackWeaponUpgrades; // DecodeLevelParms: ... back on the weapons carried
void(entity p) VR_ADGun_EnsureCarried;   // DecodeLevelParms: AD soldiers' guns carried from the last level precached
float() VR_ADGun_LegacyWorld;            // OnLoadGame: an older save's eliminator's guns the centurion's (vr_ad_enemyguns.qc)
void(entity p) AD_UpgradesToWeapons;     // PutClientInServer, OnLoadGame: a player's own upgrades (before 2026-10-12) onto his weapons
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
float() VR_AD_ShadowAxeHit;                  // monsters.qc's pain check: a Shadow Axe's blow (vr_ad_bridge.qc)
float(entity p, float xBit) VR_AD_HasUpgrade; // triggers_states.qc's upgrade_* keys: p carries the upgrade
void(float a) VR_AD_WaterAlpha;
float(entity client, float density, vector colour) VR_AD_Fog;
float(entity targ, entity inflictor, entity attacker, float damage, float checkarmor) VR_AD_DamageOurs;
void(entity targ, entity inflictor, entity attacker, float take) VR_AD_Hit;
void(float xSplash) VR_AD_Splash; // (vr_kdtrig.qc: AD's radius damage, for the explosions' ragdoll trigger)
float() VR_AD_PathCornerTouch;
float(entity pellet) VR_AD_PelletDamage;
void(entity victim, entity pellet, float damage) VR_AD_PelletHit;
void(entity pellet) VR_AD_PelletPrepare;
void(entity pellet) VR_AD_PelletLaunch;
float(entity shooter, float speed) VR_AD_PelletSpeed;
void(entity missile) VR_AD_Ordnance;
void(entity flame, float poison) VR_ADGun_StreamFx;        // a flamethrower's flame (a pyro's, a fumigator's): our fire too
void(entity t, float xAlive, vector xAt, vector xDir) VR_ADGun_BoltShock; // a plasma rifle's bolt's hit: electrocuted
float(entity bolt) VR_ADGun_IsElectric;                   // a plasma rifle's bolt (the soldiers', eliminators', centurions')
void(vector org, vector dir, float xHit) VR_ADGun_BoltFx;  // ... its muzzle's or its hit's sparks and arcs
void(vector org, vector dir, float poison, float water) VR_ADGun_StreamMuzzle; // a flamethrower's muzzle: fire, smoke
void(entity xVictim, entity xFrom, entity xBy, float xDps, float xTime) VR_ADGun_Poison; // poisoned for a while: damage, green puffs
float(entity targ) VR_AD_Skybox;                  // AD's trigger_skybox: its sky by svc_skybox to every client; TRUE: done
float(entity client, float density) VR_AD_SkyFog; // AD's sky fog: the player's STAT_QVR_ADSKYFOG; TRUE: done
float() VR_AD_WeaponTouch;
float(entity targ) VR_AD_SetCurrentAmmo;
float(entity xItem) VR_AD_ArtifactItems;
.float vr_climb_ladder; // a ladder's volume, climbed by hand (the engine's vr_climb.cpp ladderHold; VR_AD_LadderSpawned)
void() VR_AD_LadderSpawned;
vector(entity e) VR_AD_ViewOfs; // AD's sight (reviewed patches): a crouched VR player's real eyes (ai.qc VR_SightOfs)
float(entity targ) VR_AD_BurnDebuff;
float(entity targ, vector colour, float density, float fade) VR_AD_Tint; // AD's v_cshift (vr_ad_player.qc)
float(entity xFire, entity xVictim) AD_BurnHurt; // VR_Burn_Think: an AD debuff's fire's damage (TRUE: dealt, ours not)
