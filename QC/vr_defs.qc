#include "vr_macros.qc"

//
//
//
// ----------------------------------------------------------------------------
// Weapon flags
// ----------------------------------------------------------------------------

float QVR_WPNFLAG_USE_SECONDARY_AMMO = VRUTIL_POWER_OF_TWO(0);

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

//
//
//
// ----------------------------------------------------------------------------
// `.throwhit` values for thrown weapons
// ----------------------------------------------------------------------------

float QVR_THROWHIT_NEVER_HIT          = 0;
float QVR_THROWHIT_HIT                = 1;
float QVR_THROWHIT_FORCEGRAB          = 2;
float QVR_THROWHIT_FORCEGRAB_PARABOLA = 3;

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

//
//
//
// ----------------------------------------------------------------------------
// Enum values for `VrWeaponThrowMode'
// ----------------------------------------------------------------------------

float QVR_WEAPONTHROWMODE_IMMERSIVE      = 0;
float QVR_WEAPONTHROWMODE_DISAPPEARONHIT = 1;
float QVR_WEAPONTHROWMODE_DISCARD        = 2;

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
// Enum values for `VrForceGrabMode`
// ----------------------------------------------------------------------------

float QVR_VRFORCEGRABMODE_DISABLED = 0;
float QVR_VRFORCEGRABMODE_LINEAR = 1;
float QVR_VRFORCEGRABMODE_PARABOLA = 2;
float QVR_VRFORCEGRABMODE_INSTANT = 3;

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

//
//
//
// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

float readytime; // Time after loading when the player is considered ready.
                 // Prevents shooting, grabbing/dropping weapons.
                 // TODO VR: (P1): this should be per-client!

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

//
//
//
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
