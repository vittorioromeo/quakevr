#include "vr_macros.qc"

//
//
//
// ----------------------------------------------------------------------------
// Weapon flags
// ----------------------------------------------------------------------------

float QVR_WPNFLAG_USE_SECONDARY_AMMO = VRUTIL_POWER_OF_TWO(0);
// A gun left hanging from its foregrip by the hand-off (vr_2h_handoff): drawn as it was held, unusable
// until a hand takes its handle (QVR_HS_CARRIED_GRIP). Never kept outside a hand.
float QVR_WPNFLAG_FOREGRIP_CARRIED = VRUTIL_POWER_OF_TWO(1);
// The ogres' chainsaw (vr_chainsaw.qc): its engine runs (started by its cord), and its chain runs (the engine running and
// the trigger held: the engine lets its bar sink into monsters, vr_modelcollide.cpp). A hand's state: never kept outside a
// hand (holstered or thrown, the engine stops).
float QVR_WPNFLAG_SAW_RUNNING = VRUTIL_POWER_OF_TWO(2);
float QVR_WPNFLAG_SAW_CHAIN = VRUTIL_POWER_OF_TWO(3);
float QVR_WPNFLAG_SAW_STATE = 12; // both (QVR_WPNFLAG_SAW_RUNNING | QVR_WPNFLAG_SAW_CHAIN): stripped as it leaves the hands
// Immersive reloading's magazine guns (vr_reload.qc): no magazine in (ejected); travels with the gun (holsters, throws, the
// level's parms) as its flags do. The engine draws the attached magazine of a gun without it (vr_view.cpp; a weapon
// prop's: U_QVR_NOMAG).
float QVR_WPNFLAG_NOMAG = VRUTIL_POWER_OF_TWO(4);
// Immersive reloading's super shotgun (vr_reload.qc, phase 2b): broken open (drawn open, it can't fire, its chambers take
// shells by hand), and the spent shells still in its barrels (fired, not yet thrown out by opening it: 0, 1 or 2, in two
// bits: QVR_WPNFLAG_SSG_SPENT_ONE times the count). Both travel with the gun as its flags do (vr_view.cpp draws it open).
float QVR_WPNFLAG_SSG_OPEN = VRUTIL_POWER_OF_TWO(5);
float QVR_WPNFLAG_SSG_SPENT_ONE = VRUTIL_POWER_OF_TWO(6);
float QVR_WPNFLAG_SSG_SPENT = 192; // (both bits: 64 and 128)
// (Bits 8-11: an AD melee weapon's kind, QVR_WPNFLAG_SWORD_KIND, on WID_SWORD only.)
// AD support: a flamethrower or poison sprayer whose tank burst (vr_ad_enemyguns.qc VR_FuelTank_Burst): spent, never a
// tank again (none lying about, none on the back, no hose: the engine's vr_adtank.cpp). Travels with the gun as its flags do.
float QVR_WPNFLAG_TANK_BURST = VRUTIL_POWER_OF_TWO(12);

//
//
//
// ----------------------------------------------------------------------------
// Hand constants
// ----------------------------------------------------------------------------

float cVR_OffHand  = 0;
float cVR_MainHand = 1;
float cVR_FakeHand = 2;

//
//
//
// ----------------------------------------------------------------------------
// Item ID categories
// ----------------------------------------------------------------------------

float IID_CATEGORY_INVALID = 0;
float IID_CATEGORY_ITEMS = 1;
float IID_CATEGORY_HIPNOTIC_ITEMS = 2;
float IID_CATEGORY_ROGUE_ITEMS = 3;

//
//
//
// ----------------------------------------------------------------------------
// Item IDs
// ----------------------------------------------------------------------------

float IID_NONE               = 0;
float IID_ARMOR1             = 1;
float IID_ARMOR2             = 2;
float IID_ARMOR3             = 3;
float IID_SUPERHEALTH        = 4;
float IID_KEY1               = 5;
float IID_KEY2               = 6;
float IID_INVISIBILITY       = 7;
float IID_INVULNERABILITY    = 8;
float IID_SUIT               = 9;
float IID_QUAD               = 10;
float IID_WETSUIT            = 11; // HIPNOTIC
float IID_EMPATHY_SHIELDS    = 12; // HIPNOTIC
float IID_HORN_OF_CONJURING  = 13; // HIPNOTIC
float IID_FIST               = 14;
float IID_SHOTGUN            = 15;
float IID_SUPER_SHOTGUN      = 16;
float IID_NAILGUN            = 17;
float IID_SUPER_NAILGUN      = 18;
float IID_GRENADE_LAUNCHER   = 19;
float IID_ROCKET_LAUNCHER    = 20;
float IID_LIGHTNING          = 21;
float IID_MJOLNIR            = 22;
float IID_SHELLS             = 23;
float IID_NAILS              = 24;
float IID_ROCKETS            = 25;
float IID_CELLS              = 26;
float IID_AXE                = 27;
float IID_PROXIMITY_GUN      = 28;
float IID_LASER_CANNON       = 29;
float IID_LAVA_NAILGUN       = 30; // ROGUE.
float IID_LAVA_SUPER_NAILGUN = 31; // ROGUE.
float IID_MULTI_GRENADE      = 32; // ROGUE.
float IID_MULTI_ROCKET       = 33; // ROGUE.
float IID_PLASMA_GUN         = 34; // ROGUE.
float IID_GRAPPLE            = 35; // ROGUE.
float IID_LAVA_NAILS         = 36; // ROGUE.
float IID_PLASMA_AMMO        = 37; // ROGUE.
float IID_MULTI_ROCKETS      = 38; // ROGUE.
float IID_SHIELD             = 39; // ROGUE.
float IID_ANTIGRAV           = 40; // ROGUE.
float IID_EARTHQUAKE         = 41; // ROGUE.
float IID_V_SPHERE           = 42; // ROGUE.
float IID_SWORD              = 43; // VR.
float IID_CHAINSAW           = 44; // VR: an ogre's chainsaw.
float IID_GRUNTGUN           = 45; // VR: a grunt's shotgun.
float IID_ENFRIFLE           = 46; // VR: an enforcer's laser rifle.
float IID_CROWBAR            = 47; // VR: the crowbar.
float IID_SUPERAXE           = 48; // MG3: Dawn of the Machine's Super Axe (vr_mg3_weapons.qc).
float IID_TOOLGUN            = 49; // VR: the toolgun (vr_toolgun.qc; a debug and sandbox tool).
float IID_ADROCKET           = 50; // AD support: the AD soldiers' guns (WID_ADROCKET..WID_ADFUMIGATOR, in order).
float IID_ADGRENADE          = 51;
float IID_ADPLASMA           = 52;
float IID_ADDEFENDER         = 53;
float IID_ADELIMINATOR       = 54;
float IID_ADCENTURION        = 55;
float IID_ADPYRO             = 56;
float IID_ADFUMIGATOR        = 57;
float IID_ADCROSSBOW         = 58; // AD support: the crossbow knight's crossbow (WID_ADCROSSBOW)
float IID_NAILPISTOL         = 59; // VR: the nail pistol (vr_nailpistol.qc).
float IID_FLAREGUN           = 60; // VR: the flare gun (vr_flaregun.qc).
float IID_REVOLVER           = 61; // VR: the heavy revolver (vr_revolver.qc).
float IID_LAST               = 61; // the last item id

//
//
//
// ----------------------------------------------------------------------------
// Weapon IDs
// ----------------------------------------------------------------------------

float WID_FIST             = 0;
float WID_GRAPPLE          = 1;
float WID_AXE              = 2;
float WID_MJOLNIR          = 3;  // HIPNOTIC.
float WID_SHOTGUN          = 4;
float WID_SUPER_SHOTGUN    = 5;
float WID_NAILGUN          = 6;
float WID_SUPER_NAILGUN    = 7;
float WID_GRENADE_LAUNCHER = 8;
float WID_PROXIMITY_GUN    = 9;  // HIPNOTIC.
float WID_ROCKET_LAUNCHER  = 10;
float WID_LIGHTNING        = 11;
float WID_LASER_CANNON     = 12; // HIPNOTIC.
float WID_SWORD            = 13; // VR: a knight's sword (weaponflags QVR_WPNFLAG_USE_SECONDARY_AMMO: the hell knight's)
float WID_CHAINSAW         = 14; // VR: an ogre's chainsaw (vr_chainsaw.qc; its clip is its fuel)
float WID_GRUNTGUN         = 15; // VR: a grunt's shotgun (vr_enemyguns.qc; its clip is its only ammo)
float WID_ENFRIFLE         = 16; // VR: an enforcer's laser rifle (vr_enemyguns.qc; its clip is its only ammo)
float WID_CROWBAR          = 17; // VR: the crowbar (vr_crowbar.qc; a melee weapon, one hand or two, as the swords)
float WID_SUPERAXE         = 18; // MG3: Dawn of the Machine's Super Axe (vr_mg3_weapons.qc; MG3's model read in place, any campaign)
float WID_TOOLGUN          = 19; // VR: the toolgun (vr_toolgun.qc, the engine's vr_toolgun.cpp: no ammo, its tools the engine's)
// AD support: Arcane Dimensions' armed soldiers' own guns, dropped as they die (vr_ad_enemyguns.qc; single use, their
// clip their only ammo, as WID_GRUNTGUN's). Their models are made from AD's monster models in place (the engine's
// vr_adguns.cpp: "<monster model>#gun<skin>"), so they exist only while AD's data is installed.
float WID_ADROCKET         = 20; // AD: the rocket soldier's launcher
float WID_ADGRENADE        = 21; // AD: the grenade soldier's launcher
float WID_ADPLASMA         = 22; // AD: the plasma soldier's rifle
float WID_ADDEFENDER       = 23; // AD: the defender's shotgun
float WID_ADELIMINATOR     = 24; // AD: gone (an older save's eliminator's plasma gun: the centurion's, VR_ADGun_LegacyIds)
float WID_ADCENTURION      = 25; // AD: the centurion's plasma gun
float WID_ADPYRO           = 26; // AD: the pyro's flamethrower
float WID_ADFUMIGATOR      = 27; // AD: the fumigator's poison sprayer
float WID_ADCROSSBOW       = 28; // AD: the crossbow knight's crossbow (vr_ad_enemyguns.qc: AD's bolts; its own bolts only)
// VR: the nail pistol (vr_nailpistol.qc; Misc/quakevr/models_src/nailpistol): one-handed, semi-automatic, a 12-round
// magazine in its grip fed from your nails; weapon settings slot 36 (vr_weapons.inc); weapon_nailpistol.
float WID_NAILPISTOL       = 29;
// VR: the flare gun (vr_flaregun.qc; Misc/quakevr/models_src/flaregun): disposable, two flares (one a barrel), its clip
// its only ammo (AID_NONE), thrown away spent; weapon settings slot 37 (vr_weapons.inc); weapon_flaregun.
float WID_FLAREGUN         = 30;
// VR: the heavy revolver (vr_revolver.qc; Misc/quakevr/models_src/revolver): one-handed, double action, six chambers
// of shells fired as slugs; its cylinder swings out to load by hand; weapon settings slot 38 (vr_weapons.inc).
float WID_REVOLVER         = 31;
float WID_LAST             = 31; // the last weapon id
float vr_pellet_damage; // FireBulletsImpl's damage a pellet while a shotgun fires (yours, Weapon Damage; a grunt's, vr_enemyguns.qc; 0: id's 4)
// And while a heavy revolver fires (vr_revolver.qc): its slug's damage times vr_pellet_heavy_mult into a monster of
// vr_pellet_heavy_health or more full health (0: none). The last pellet's damage dealt, its part and what it hit (tests).
float vr_pellet_heavy_mult;
float vr_pellet_heavy_health;
float vr_pellet_last;
float vr_pellet_last_part;
entity vr_pellet_last_ent;
// FireBulletsImpl's tracers (the tracer builtin): the hand that fires (-1 a monster's gun) and where they start (its muzzle).
float vr_tracer_hand;
vector vr_tracer_from;
float VR_CHAINSAW_TANK     = 100; // a chainsaw's full tank (its clip: fuel, in percent)

//
//
//
// ----------------------------------------------------------------------------
// Ammo IDs
// ----------------------------------------------------------------------------

float AID_NONE          = 0;
float AID_SHELLS        = 1;
float AID_NAILS         = 2;
float AID_ROCKETS       = 3;
float AID_CELLS         = 4;
float AID_LAVA_NAILS    = 5; // ROGUE.
float AID_MULTI_ROCKETS = 6; // ROGUE.
float AID_PLASMA        = 7; // ROGUE.

//
//
//
// ----------------------------------------------------------------------------
// Spawnflags
// ----------------------------------------------------------------------------

float SPAWNFLAG_SUPERSPIKE             = VRUTIL_POWER_OF_TWO(0);  // HIPNOTIC.
float SPAWNFLAG_LASER                  = VRUTIL_POWER_OF_TWO(1);  // HIPNOTIC.
float SPAWNFLAG_LAVABALL               = VRUTIL_POWER_OF_TWO(2);  // HIPNOTIC.
float SPAWNFLAG_ROCKET                 = VRUTIL_POWER_OF_TWO(3);  // HIPNOTIC.
float SPAWNFLAG_SILENT                 = VRUTIL_POWER_OF_TWO(4);  // HIPNOTIC.
float SPAWNFLAG_LAVASPIKE              = VRUTIL_POWER_OF_TWO(5);  // ROGUE.
float SPAWNFLAG_SUPERLAVA              = VRUTIL_POWER_OF_TWO(6);  // ROGUE.
float SPAWNFLAG_LAVASKILL              = VRUTIL_POWER_OF_TWO(7);  // ROGUE.
float SPAWNFLAG_FIREBALL               = VRUTIL_POWER_OF_TWO(8);  // ROGUE.
float SPAWNFLAG_TBABY_HIPHOP           = VRUTIL_POWER_OF_TWO(9);  // HONEY.
float SPAWNFLAG_TBABY_LIVID            = VRUTIL_POWER_OF_TWO(10); // HONEY.
float SPAWNFLAG_TRIGGER_SPAWN_TFOG     = VRUTIL_POWER_OF_TWO(11); // HONEY.
float SPAWNFLAG_REMOVE_CORPSE          = VRUTIL_POWER_OF_TWO(12); // HONEY.
float SPAWNFLAG_TRIGGER_SPAWN_ANGRY    = VRUTIL_POWER_OF_TWO(13); // HONEY.
float SPAWNFLAG_TRIGGER_SPAWN          = VRUTIL_POWER_OF_TWO(14); // HONEY.
float SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE = VRUTIL_POWER_OF_TWO(15); // HONEY.
float SPAWNFLAG_SF_SILENT_WAKEUP       = VRUTIL_POWER_OF_TWO(16); // HONEY.
float SPAWNFLAG_SF_AI_BBOX_CHECK       = VRUTIL_POWER_OF_TWO(17); // HONEY.

//
//
//
// ----------------------------------------------------------------------------
// Hotspots
// ----------------------------------------------------------------------------

float QVR_HS_NONE                   = 0;
float QVR_HS_OFFHAND_2H_GRAB        = 1; // 2H grab - helper offhand
float QVR_HS_MAINHAND_2H_GRAB       = 2; // 2H grab - helper mainhand
float QVR_HS_LEFT_SHOULDER_HOLSTER  = 3;
float QVR_HS_RIGHT_SHOULDER_HOLSTER = 4;
float QVR_HS_LEFT_HIP_HOLSTER       = 5;
float QVR_HS_RIGHT_HIP_HOLSTER      = 6;
float QVR_HS_HAND_SWITCH            = 7;
float QVR_HS_LEFT_UPPER_HOLSTER     = 8;
float QVR_HS_RIGHT_UPPER_HOLSTER    = 9;
float QVR_HS_CARRIED_GRIP           = 10; // the handle of the gun the other hand carries by its foregrip
float QVR_HS_GRENADE_POUCH          = 11; // the grenade pouch at the small of the back (vr_handgrenade: vr_grenade.qc)
float QVR_HS_AMMO_POUCH             = 12; // the ammo pouch on the front of the belt (vr_reload_mode 3: vr_reload.qc)
float QVR_HS_MAGAZINE               = 13;
float QVR_CFX_INTO_PROP             = 241; // and into the gun lying about given as its entity (vr_reload.qc VR_Reload_IntoProp)
float QVR_CFX_INTO_HAND             = 242; // into the hand that took it (vr_carry_autocollect: vr_carry.qc VR_Carry_AutoCollect; vr_collectfx.cpp)
float QVR_CFX_INTO_GUN              = 240; // collectfx's "hotspot" for a round slid into the gun in the other hand (vr_reload.qc; vr_collectfx.cpp) // the magazine attached to the other hand's gun (vr_reload_mode 3: vr_reload.qc)

//
//
//
// ----------------------------------------------------------------------------
// `.throwhit` values for thrown weapons
// ----------------------------------------------------------------------------

float QVR_THROWHIT_NEVER_HIT          = 0;
float QVR_THROWHIT_HIT                = 1;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrHolsterMode'
// ----------------------------------------------------------------------------

float QVR_WEAPONMODE_IMMERSIVE      = 0;
float QVR_WEAPONMODE_CYCLEQUICKSLOT = 1;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrReloadMode'
// ----------------------------------------------------------------------------

float QVR_RELOADMODE_NONE        = 0;
float QVR_RELOADMODE_ALLHOLSTERS = 1;
float QVR_RELOADMODE_HIPHOLSTERS = 2;
// Manual-immersive (docs/vr-port/RELOAD.md): the guns that load by hand (VR_Reload_ManualFor, vr_reload.qc) take
// their ammo from the front pouch, one round or magazine at a time; the others reload at the hip holsters, as 2.
float QVR_RELOADMODE_IMMERSIVE   = 3;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrWeaponCycleMode'
// ----------------------------------------------------------------------------

float QVR_WEAPONCYCLEMODE_DISABLED = 0;
float QVR_WEAPONCYCLEMODE_ALLOWED  = 1;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrMeleeBloodlust'
// ----------------------------------------------------------------------------

float QVR_MELEEBLOODLUST_ENABLED  = 0;
float QVR_MELEEBLOODLUST_DISABLED = 1;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrEnemyDrops`
// ----------------------------------------------------------------------------

float QVR_ENEMYDROPS_WHENELIGIBLE = 0;
float QVR_ENEMYDROPS_ALWAYS = 1;
float QVR_ENEMYDROPS_DISABLE = 2;

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrAmmoBoxDrops`
// ----------------------------------------------------------------------------

float QVR_AMMOBOXDROPS_WHENELIGIBLE = 0;
float QVR_AMMOBOXDROPS_ALWAYS = 1;
float QVR_AMMOBOXDROPS_DISABLE = 2;

//
//
//
// ----------------------------------------------------------------------------
// Flags for VR bits: `vrbits0`
// ----------------------------------------------------------------------------

float QVR_VRBITS0_TELEPORTING                 = VRUTIL_POWER_OF_TWO(0);
float QVR_VRBITS0_OFFHAND_GRABBING            = VRUTIL_POWER_OF_TWO(1);
float QVR_VRBITS0_OFFHAND_PREVGRABBING        = VRUTIL_POWER_OF_TWO(2);
float QVR_VRBITS0_MAINHAND_GRABBING           = VRUTIL_POWER_OF_TWO(3);
float QVR_VRBITS0_MAINHAND_PREVGRABBING       = VRUTIL_POWER_OF_TWO(4);
float QVR_VRBITS0_2H_AIMING                   = VRUTIL_POWER_OF_TWO(5);
float QVR_VRBITS0_OFFHAND_RELOADING           = VRUTIL_POWER_OF_TWO(6);
float QVR_VRBITS0_OFFHAND_PREVRELOADING       = VRUTIL_POWER_OF_TWO(7);
float QVR_VRBITS0_MAINHAND_RELOADING          = VRUTIL_POWER_OF_TWO(8);
float QVR_VRBITS0_MAINHAND_PREVRELOADING      = VRUTIL_POWER_OF_TWO(9);
float QVR_VRBITS0_OFFHAND_RELOADFLICKING      = VRUTIL_POWER_OF_TWO(10);
float QVR_VRBITS0_OFFHAND_PREVRELOADFLICKING  = VRUTIL_POWER_OF_TWO(11);
float QVR_VRBITS0_MAINHAND_RELOADFLICKING     = VRUTIL_POWER_OF_TWO(12);
float QVR_VRBITS0_MAINHAND_PREVRELOADFLICKING = VRUTIL_POWER_OF_TWO(13);
float QVR_VRBITS0_HANDSTRACKED              = VRUTIL_POWER_OF_TWO(14); // set by the engine: real VR hands
float QVR_VRBITS0_OFFHAND_CLIMBING         = VRUTIL_POWER_OF_TWO(15); // set by the engine: the hand holds a ledge (vr_climb)
float QVR_VRBITS0_MAINHAND_CLIMBING        = VRUTIL_POWER_OF_TWO(16);
float QVR_VRBITS0_OFFHAND_BUSY             = VRUTIL_POWER_OF_TWO(17); // set by the engine: the hand holds the flashlight (client-side): no force grab
float QVR_VRBITS0_MAINHAND_BUSY            = VRUTIL_POWER_OF_TWO(18);
float QVR_VRBITS0_OFFHAND_SECONDARY        = VRUTIL_POWER_OF_TWO(19); // set by the engine: the hand's B/Y held (in the game): the grapple reels
float QVR_VRBITS0_MAINHAND_SECONDARY       = VRUTIL_POWER_OF_TWO(20);
float QVR_VRBITS0_OFFHAND_PRIMARY          = VRUTIL_POWER_OF_TWO(21); // set by the engine: the hand's A/X held (in the game): the grapple unreels (in the air)
float QVR_VRBITS0_MAINHAND_PRIMARY         = VRUTIL_POWER_OF_TWO(22);
float QVR_VRBITS0_CROUCHED                 = VRUTIL_POWER_OF_TWO(23); // set by the engine: the player has a crouched box (vr_crouch_hull: vr_hull.cpp)

//
//
//
// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

float readytime; // Time after loading when the player is considered ready.
                 // Prevents shooting, grabbing/dropping weapons. (One for every client: BACKLOG.md,
                 // "Old QuakeC notes".)

//
//
//
// ----------------------------------------------------------------------------
// Particle Presets
// ----------------------------------------------------------------------------

float QVR_PARTICLE_PRESET_BULLETPUFF    = 0;
float QVR_PARTICLE_PRESET_BLOOD         = 1;
float QVR_PARTICLE_PRESET_EXPLOSION     = 2;
float QVR_PARTICLE_PRESET_LIGHTNING     = 3;
float QVR_PARTICLE_PRESET_SMOKE         = 4;
float QVR_PARTICLE_PRESET_SPARKS        = 5;
float QVR_PARTICLE_PRESET_GUNSMOKE      = 6;
float QVR_PARTICLE_PRESET_TELEPORT      = 7;
float QVR_PARTICLE_PRESET_GUNPICKUP     = 8;
float QVR_PARTICLE_PRESET_GUNFORCEGRAB  = 9;
float QVR_PARTICLE_PRESET_LAVASPIKE     = 10;
float QVR_PARTICLE_PRESET_BIGSMOKE      = 11;
float QVR_PARTICLE_PRESET_BLOODTRAIL    = 13; // drops thrown along `dir` (its length: up to 25x as fast) and falling: a gib's trail, a stump's spurts (vr_decap.qc)
float QVR_PARTICLE_PRESET_WOODDUST      = 15; // a crate or a piece of one breaking: wood dust and splinters (vr_crates.qc)
float QVR_PARTICLE_PRESET_SAWSMOKE      = 16; // a running chainsaw's faint exhaust (vr_chainsaw.qc): count puffs, dir their way out
float QVR_PARTICLE_PRESET_CONTACTSPARKS = 17; // a thunderbolt cell making or breaking contact (vr_reload.qc): count small blue-white sparks
float QVR_PARTICLE_PRESET_BATTERYSMOKE  = 18; // a spent thunderbolt cell smoking (vr_reload.qc): count thin grey wisps
// AD support (vr_ad_enemyguns.qc): flamethrowers' fire and poison tongues carried along (dir: velocity / 100), a poisoned
// body's green puffs (dir: its half size / 16), an electric gun's sparks and arcs (dir: a direction).
float QVR_PARTICLE_PRESET_ADFLAME        = 19;
float QVR_PARTICLE_PRESET_ADPOISON       = 20;
float QVR_PARTICLE_PRESET_POISONPUFF     = 21;
float QVR_PARTICLE_PRESET_ELECTRICSPARKS = 22;
float QVR_PARTICLE_PRESET_ADFLAMEHIT     = 23; // a flamethrower's flame spent on a wall: its scorch, a tongue of fire (vr_ad_enemyguns.qc)
float QVR_PARTICLE_PRESET_STEAM          = 24; // fire put out by water: count puffs of white steam (a flame, a lava nail going in)
// A fuel tank's burst field (vr_ad_enemyguns.qc VR_TankField_Look): org a spot on the ground, dir (spread / 8, size, alpha):
float QVR_PARTICLE_PRESET_TANKFIRE       = 25; // count tongues of fire rising straight up, embers
float QVR_PARTICLE_PRESET_TANKSMOKE      = 26; // count puffs of dark smoke off the flames' tops
float QVR_PARTICLE_PRESET_TANKCLOUD      = 27; // count big puffs of the poison cloud's green smoke

//
//
//
// ----------------------------------------------------------------------------
// Survival Arena (vr_arena.qc): the wave director's hooks. world.qc's worldspawn and StartFrame call the first
// two, combat.qc's monster_die the third. None of them does anything unless the map is "vrarena" or vr_arena is
// set; the tests' beat (VR_ARENA_Test_Frame) is declared in world.qc, under QVR_TESTS.
// ----------------------------------------------------------------------------

void() VR_ARENA_WorldSpawn;
void() VR_ARENA_Frame;
void() VR_ARENA_MonsterDie;

// ----------------------------------------------------------------------------
// Item Names
// ----------------------------------------------------------------------------

string ITEM_NAME_SHELLS = "item_shells";
string ITEM_NAME_SPIKES = "item_spikes";
string ITEM_NAME_ROCKETS = "item_rockets";
string ITEM_NAME_LAVA_SPIKES = "item_lava_spikes";
string ITEM_NAME_MULTI_ROCKETS = "item_multi_rockets";
string ITEM_NAME_PLASMA = "item_plasma";
string ITEM_NAME_CELLS = "item_cells";
string ITEM_NAME_ARMOR1 = "item_armor1";
string ITEM_NAME_ARMOR2 = "item_armor2";
string ITEM_NAME_ARMORINV = "item_armorInv";
