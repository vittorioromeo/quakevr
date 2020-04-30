//
//
// ----------------------------------------------------------------------------
// VR: Hand constants.
float cVR_OffHand  = 0;
float cVR_MainHand = 1;
float cVR_FakeHand = 2;

//
//
// ----------------------------------------------------------------------------
// VR: Item ID categories.
float IID_CATEGORY_INVALID = -1;
float IID_CATEGORY_ITEMS = 0;
float IID_CATEGORY_HIPNOTIC_ITEMS = 1;

//
//
// ----------------------------------------------------------------------------
// VR: Item IDs.
// TODO VR: (P1) use:
float IID_ARMOR1            = 0;
float IID_ARMOR2            = 1;
float IID_ARMOR3            = 2;
float IID_SUPERHEALTH       = 3;
float IID_KEY1              = 4;
float IID_KEY2              = 5;
float IID_INVISIBILITY      = 6;
float IID_INVULNERABILITY   = 7;
float IID_SUIT              = 8;
float IID_QUAD              = 9;
float IID_WETSUIT           = 10; // Scourge of Armagon (`hipnotic`).
float IID_EMPATHY_SHIELDS   = 11; // Scourge of Armagon (`hipnotic`).
float IID_HORN_OF_CONJURING = 12; // Scourge of Armagon (`hipnotic`).
float IID_FIST              = 13;
float IID_SHOTGUN           = 14;
float IID_SUPER_SHOTGUN     = 15;
float IID_NAILGUN           = 16;
float IID_SUPER_NAILGUN     = 17;
float IID_GRENADE_LAUNCHER  = 18;
float IID_ROCKET_LAUNCHER   = 19;
float IID_LIGHTNING         = 20;
float IID_MJOLNIR           = 21;
float IID_SHELLS            = 22;
float IID_NAILS             = 23;
float IID_ROCKETS           = 24;
float IID_CELLS             = 25;
float IID_AXE               = 26;
float IID_PROXIMITY_GUN     = 27;
float IID_LASER_CANNON      = 28;

//
//
// ----------------------------------------------------------------------------
// VR: Weapon IDs.
float WID_FIST             = 0;
float WID_GRAPPLE          = 1;
float WID_AXE              = 2;
float WID_MJOLNIR          = 3;
float WID_SHOTGUN          = 4;
float WID_SUPER_SHOTGUN    = 5;
float WID_NAILGUN          = 6;
float WID_SUPER_NAILGUN    = 7;
float WID_GRENADE_LAUNCHER = 8;
float WID_PROXIMITY_GUN    = 9;
float WID_ROCKET_LAUNCHER  = 10;
float WID_LIGHTNING        = 11;
float WID_LASER_CANNON     = 12;

//
//
// ----------------------------------------------------------------------------
// VR: Ammo IDs.
float AID_SHELLS  = 0;
float AID_NAILS   = 1;
float AID_ROCKETS = 2;
float AID_CELLS   = 3;
float AID_NONE    = 4;

//
//
// ----------------------------------------------------------------------------
// VR: Hotspots.
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
// ----------------------------------------------------------------------------
// VR: `.throwhit` values for thrown weapons.
float QVR_THROWHIT_NEVER_HIT = 0;
float QVR_THROWHIT_HIT       = 1;
float QVR_THROWHIT_FORCEGRAB = 2;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrHolsterMode'.
float QVR_WEAPONMODE_IMMERSIVE      = 0;
float QVR_WEAPONMODE_CYCLEQUICKSLOT = 1;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrWeaponThrowMode'.
float QVR_WEAPONTHROWMODE_IMMERSIVE      = 0;
float QVR_WEAPONTHROWMODE_DISAPPEARONHIT = 1;
float QVR_WEAPONTHROWMODE_DISCARD        = 2;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrWeaponCycleMode'.
float QVR_WEAPONCYCLEMODE_DISABLED = 0;
float QVR_WEAPONCYCLEMODE_ALLOWED  = 1;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrMeleeBloodlust'.
float QVR_MELEEBLOODLUST_ENABLED  = 0;
float QVR_MELEEBLOODLUST_DISABLED = 1;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrEnemyDrops`.
float QVR_ENEMYDROPS_WHENELIGIBLE = 0;
float QVR_ENEMYDROPS_ALWAYS = 1;
float QVR_ENEMYDROPS_DISABLE = 2;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrAmmoBoxDrops`.
float QVR_AMMOBOXDROPS_WHENELIGIBLE = 0;
float QVR_AMMOBOXDROPS_ALWAYS = 1;
float QVR_AMMOBOXDROPS_DISABLE = 2;

//
//
// ----------------------------------------------------------------------------
// VR: Enum values for `VrForceGrabMode`.
float QVR_VRFORCEGRABMODE_DISABLED = 0;
float QVR_VRFORCEGRABMODE_PARABOLA = 1;
float QVR_VRFORCEGRABMODE_LINEAR = 2;
float QVR_VRFORCEGRABMODE_INSTANT = 3;

//
//
// ----------------------------------------------------------------------------
// VR: Globals.
float readytime; // Time after loading when the player is considered ready.
                 // TODO VR: (P2) what does this affect?
