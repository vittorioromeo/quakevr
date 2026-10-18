// vr_ad_guns_defs.qc -- AD support: Arcane Dimensions' armed soldiers' own guns (WID_ADROCKET..WID_ADFUMIGATOR, and the
// crossbow knight's crossbow WID_ADCROSSBOW; vr_defs.qc): what the weapon tables (vr_weaponutil.qc) answer for them. Early: those tables come before AD's files.
// Their firing, drops and projectiles are vr_ad_enemyguns.qc's (after AD's files: it uses AD's names).
//
// Each is a single-use enemy weapon as the grunts' burst rifle is (vr_enemyguns.qc): its clip is its only ammo (AID_NONE:
// nothing refills it, no reload), it clicks empty and is still a club. Every number of the player's use of them is a
// setting, vr_adgun_<gun>_<what> (VR Settings > Combat > Enemy Weapons > Arcane Dimensions' Guns), for balancing: the
// monsters' own attacks are AD's and unchanged.
//
// Their models are variants of Quake VR's burst rifle (the engine's vr_adgunart.cpp: "progs/v_gruntgun.mdl#adv<n>", made
// from our own v_gruntgun.mdl as it loads, each gun with its own add-ons and skin; the crossbow is AD's own). The older
// models cut out of AD's monsters ("<monster model>#gun<skin>", vr_adguns.cpp) still load for a saved game naming them.

float(float wid) VR_IsADGun =
{
    return wid >= WID_ADROCKET && wid <= WID_ADCROSSBOW;
};

// VR_Pack_WeaponAvailable: AD's soldiers' guns are carried (kept, given) only while AD's data is loaded (their models
// AD's).
float() VR_ADGun_Available =
{
    return cvar("vr_ad_mounted") != 0;
};

// The gun's settings' name part (vr_adgun_<key>_*).
string(float wid) VR_ADGun_Key =
{
    if(wid == WID_ADROCKET)     { return "rocket"; }
    if(wid == WID_ADGRENADE)    { return "grenade"; }
    if(wid == WID_ADPLASMA)     { return "plasma"; }
    if(wid == WID_ADDEFENDER)   { return "defender"; }
    if(wid == WID_ADELIMINATOR) { return "eliminator"; }
    if(wid == WID_ADCENTURION)  { return "centurion"; }
    if(wid == WID_ADPYRO)       { return "pyro"; }
    if(wid == WID_ADCROSSBOW)   { return "crossbow"; }
    return "fumigator";
};

// One of its settings: vr_adgun_<key>_<what>.
float(float wid, string what) VR_ADGun_Value =
{
    return cvar(sprintf("vr_adgun_%s_%s", VR_ADGun_Key(wid), what));
};

string(float wid) VR_ADGun_Name =
{
    if(wid == WID_ADROCKET)     { return "Rocket Soldier's Launcher"; }
    if(wid == WID_ADGRENADE)    { return "Grenade Soldier's Launcher"; }
    if(wid == WID_ADPLASMA)     { return "Plasma Soldier's Rifle"; }
    if(wid == WID_ADDEFENDER)   { return "Defender's Shotgun"; }
    if(wid == WID_ADELIMINATOR) { return "Eliminator's Plasma Gun"; }
    if(wid == WID_ADCENTURION)  { return "Centurion's Plasma Gun"; }
    if(wid == WID_ADPYRO)       { return "Pyro's Flamethrower"; }
    if(wid == WID_ADCROSSBOW)   { return "Crossbow Knight's Crossbow"; }
    return "Fumigator's Poison Sprayer";
};

// Its model: our burst rifle's variant for that gun (the engine's vr_adgunart.cpp makes "progs/v_gruntgun.mdl#adv<n>",
// n its place from WID_ADROCKET, from Quake VR's own v_gruntgun.mdl and v_rock.mdl as it loads: each gun its own add-ons
// and skin). The older "<monster model>#gun<skin>" models cut from AD's monsters (vr_adguns.cpp) still load for saved
// games that name them. The crossbow is AD's own.
string(float wid) VR_ADGun_Model =
{
    // (constants: the precache keeps the string)
    if(wid == WID_ADROCKET)     { return "progs/v_gruntgun.mdl#adv0"; }
    if(wid == WID_ADGRENADE)    { return "progs/v_gruntgun.mdl#adv1"; }
    if(wid == WID_ADPLASMA)     { return "progs/v_gruntgun.mdl#adv2"; }
    if(wid == WID_ADDEFENDER)   { return "progs/v_gruntgun.mdl#adv3"; }
    if(wid == WID_ADELIMINATOR) { return "progs/v_gruntgun.mdl#adv4"; }
    if(wid == WID_ADCENTURION)  { return "progs/v_gruntgun.mdl#adv5"; }
    if(wid == WID_ADPYRO)       { return "progs/v_gruntgun.mdl#adv6"; }
    if(wid == WID_ADCROSSBOW)   { return "progs/w_dcrossbow.mdl#gun0"; } // (AD's own, its gibs': turned to face +x, vr_adguns.cpp)
    return "progs/v_gruntgun.mdl#adv7";
};

// A full clip: the rounds (or the flamethrowers' fuel, a tick's worth a unit) it holds as it drops.
float(float wid) VR_ADGun_ClipSize =
{
    if(wid == WID_ADPYRO || wid == WID_ADFUMIGATOR)
    {
        return max(1, rint(VR_ADGun_Value(wid, "fuel")));
    }
    return max(1, rint(VR_ADGun_Value(wid, "ammo")));
};

// ---------------------------------------------------------------------------------------------------------------------
// AD's melee monsters' weapons (vr_ad_melee.qc drops them): the knight's sword (WID_SWORD: its swing, its two-hand grip,
// its holsters) with a kind in its weapon flags, bits 8-10 (the grapple's hook tag's bits: a grapple's only, so a
// level change keeps a sword's kind: DecodeLevelParms). 0: Quake VR's knight's or hell knight's sword. Their models are
// AD's own weapon models laid as the knight's sword (the engine's vr_admelee.cpp: "<AD weapon model>#melee"; nothing of
// AD's in Quake VR's files); without AD's data the knight's sword stands in. The hammer and the mace are heavy (Weapon
// Weights: their mass slows a one-handed swing; vr_admelee_heavy_1h: its damage) and blunt (VR_Melee_Thing: an axe's
// head that cuts no heads off; client.qc: a blunt blow's sounds and wounds).
float QVR_WPNFLAG_SWORD_KIND_UNIT = 256;
float QVR_WPNFLAG_SWORD_KIND = 1792; // (bits 8-10: 256 .. 1792)
float VR_ADMELEE_FURY = 1;     // the death fury's sword (it drops two: progs/w_dfurysword1.mdl)
float VR_ADMELEE_DKNIGHT = 2;  // the death knight's (progs/w_dknightsword.mdl)
float VR_ADMELEE_ZKNIGHT = 3;  // the zombie knight's (progs/w_zombiesword.mdl)
float VR_ADMELEE_DGUARD = 4;   // the death guard's (progs/w_dguardsword.mdl)
float VR_ADMELEE_HAMMER = 5;   // the hammer hunter ogre's (progs/w_ogreham.mdl)
float VR_ADMELEE_MACE = 6;     // the mace hunter ogre's (progs/w_ogremac.mdl)
float VR_ADMELEE_LAST = 6;

// A sword's kind (0: Quake VR's own), from its weapon flags.
float(float xFlags) VR_ADMelee_Kind =
{
    return floor((xFlags & QVR_WPNFLAG_SWORD_KIND) / QVR_WPNFLAG_SWORD_KIND_UNIT);
};

// The weapon flags of a kind's sword.
float(float xKind) VR_ADMelee_Flags =
{
    return xKind * QVR_WPNFLAG_SWORD_KIND_UNIT;
};

float(float xKind) VR_ADMelee_Heavy =
{
    return xKind == VR_ADMELEE_HAMMER || xKind == VR_ADMELEE_MACE;
};

// Its model (vr_admelee.cpp makes it from AD's), or the knight's sword without AD's data.
string(float xKind) VR_ADMelee_Model =
{
    if(!cvar("vr_ad_mounted"))
    {
        return "progs/v_ksword.mdl";
    }
    if(xKind == VR_ADMELEE_FURY)     { return "progs/w_dfurysword1.mdl#melee"; }
    if(xKind == VR_ADMELEE_DKNIGHT)  { return "progs/w_dknightsword.mdl#melee"; }
    if(xKind == VR_ADMELEE_ZKNIGHT)  { return "progs/w_zombiesword.mdl#melee"; }
    if(xKind == VR_ADMELEE_DGUARD)   { return "progs/w_dguardsword.mdl#melee"; }
    if(xKind == VR_ADMELEE_HAMMER)   { return "progs/w_ogreham.mdl#melee"; }
    return "progs/w_ogremac.mdl#melee";
};

string(float xKind) VR_ADMelee_Name =
{
    if(xKind == VR_ADMELEE_FURY)     { return "Death Fury's Sword"; }
    if(xKind == VR_ADMELEE_DKNIGHT)  { return "Death Knight's Sword"; }
    if(xKind == VR_ADMELEE_ZKNIGHT)  { return "Zombie Knight's Sword"; }
    if(xKind == VR_ADMELEE_DGUARD)   { return "Death Guard's Sword"; }
    if(xKind == VR_ADMELEE_HAMMER)   { return "Hunter Ogre's Hammer"; }
    return "Hunter Ogre's Mace";
};

// vr_ad_melee.qc (after AD's files) defines these.
void() VR_ADMelee_Drop;                          // VR_AD_Dying: self, an AD melee monster, drops its weapons
void(float xClass) VR_ADMelee_Prepare;           // VR_AD_MonsterSetup: its weapons' models made and precached now
void(float wid, float xFlags) VR_ADMelee_Placed; // a sword of a kind laid or carried: its model made and precached
void(entity p) VR_ADMelee_EnsureCarried;         // DecodeLevelParms: p's carried ones made and precached
void(float request, entity p) AD_MeleeTest;      // vr_ad_test 110..122: the AD melee weapons' tests

// vr_ad_enemyguns.qc (after AD's files) defines these.
float(float xHand, float xWeapon, float xAttackFinished, float* xOutAttackFinished) VR_ADGun_Attack; // VR_EnemyGun_Attack: TRUE fired
void(float xHand) VR_ADGun_Frame;   // VR_EnemyGuns_Frame: a hand's flamethrower stream's end
void() VR_ADGun_Drop;               // VR_AD_Dying: self, an armed AD soldier, drops its own gun
void(entity e, float xTakes) VR_ADGun_LimbCut; // VR_Limb_Cut: a limb cut off or popped (the bones it took): Freddie's gun falls
void(float xClass) VR_ADGun_Prepare; // VR_AD_MonsterSetup: an armed AD soldier's gun model made and precached now
void(float wid) VR_ADGun_Placed;     // VR_WeaponGrabbable_Make: one of these guns laid (a map's, the toolgun's)
void(float request, entity p) AD_GunTest; // vr_ad_test 70..89: the AD soldiers' guns' tests
