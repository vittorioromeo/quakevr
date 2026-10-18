// vr_ad_guns_defs.qc -- AD support: Arcane Dimensions' armed soldiers' own guns (WID_ADROCKET..WID_ADFUMIGATOR, and the
// crossbow knight's crossbow WID_ADCROSSBOW; vr_defs.qc): what the weapon tables (vr_weaponutil.qc) answer for them. Early: those tables come before AD's files.
// Their firing, drops and projectiles are vr_ad_enemyguns.qc's (after AD's files: it uses AD's names).
//
// Each is a single-use enemy weapon as the grunts' burst rifle is (vr_enemyguns.qc): its clip is its only ammo (AID_NONE:
// nothing refills it, no reload), it clicks empty and is still a club. Every number of the player's use of them is a
// setting, vr_adgun_<gun>_<what> (VR Settings > Combat > Enemy Weapons > Arcane Dimensions' Guns), for balancing: the
// monsters' own attacks are AD's and unchanged.
//
// Their models are AD's own monsters' guns, cut out of AD's monster models where they are installed (the engine's
// vr_adguns.cpp: "<monster model>#gun<skin>", the gun of the soldier's kind in that soldier's skin; nothing of AD's is in
// Quake VR's files). Without AD's data Quake VR's own enemy guns stand in for guns made then (a saved game loaded
// elsewhere keeps the "#gun" names in its precache list: the engine loads the stand-in's file under such a name, as it
// does for an AD release whose models differ from the measured one: vr_adguns.cpp derivedFile).

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

// The soldiers' (AD's mon_soldier.mdl: Quake VR's grunt's mesh) and the enforcers' kinds (the rest): which of Quake VR's
// own enemy guns stands in without AD's data.
float(float wid) VR_ADGun_SoldierKind =
{
    return wid == WID_ADROCKET || wid == WID_ADGRENADE || wid == WID_ADPLASMA;
};

// Its model: the gun of the soldier who drops it, in his skin (the first of his kind's pair or three; vr_adguns.cpp makes
// it from AD's model), or Quake VR's stand-in without AD's data.
string(float wid) VR_ADGun_Model =
{
    if(!cvar("vr_ad_mounted"))
    {
        return VR_ADGun_SoldierKind(wid) ? "progs/v_gruntgun.mdl" : "progs/v_enfrifle.mdl";
    }
    if(wid == WID_ADROCKET)     { return "progs/mon_soldier.mdl#gun4"; }
    if(wid == WID_ADGRENADE)    { return "progs/mon_soldier.mdl#gun6"; }
    if(wid == WID_ADPLASMA)     { return "progs/mon_soldier.mdl#gun8"; }
    if(wid == WID_ADDEFENDER)   { return "progs/mon_enforcer.mdl#gun3"; }
    if(wid == WID_ADELIMINATOR) { return "progs/mon_enforcer.mdl#gun6"; }
    if(wid == WID_ADCENTURION)  { return "progs/mon_centurion.mdl#gun0"; }
    if(wid == WID_ADPYRO)       { return "progs/mon_pyro.mdl#gun0"; }
    if(wid == WID_ADCROSSBOW)   { return "progs/w_dcrossbow.mdl#gun0"; } // (AD's own, its gibs': turned to face +x, vr_adguns.cpp)
    return "progs/mon_fumigator.mdl#gun0";
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

// vr_ad_enemyguns.qc (after AD's files) defines these.
float(float xHand, float xWeapon, float xAttackFinished, float* xOutAttackFinished) VR_ADGun_Attack; // VR_EnemyGun_Attack: TRUE fired
void(float xHand) VR_ADGun_Frame;   // VR_EnemyGuns_Frame: a hand's flamethrower stream's end
void() VR_ADGun_Drop;               // VR_AD_Dying: self, an armed AD soldier, drops its own gun
void(entity e, float xTakes) VR_ADGun_LimbCut; // VR_Limb_Cut: a limb cut off or popped (the bones it took): Freddie's gun falls
void(float xClass) VR_ADGun_Prepare; // VR_AD_MonsterSetup: an armed AD soldier's gun model made and precached now
void(float wid) VR_ADGun_Placed;     // VR_WeaponGrabbable_Make: one of these guns laid (a map's, the toolgun's)
void(float request, entity p) AD_GunTest; // vr_ad_test 70..89: the AD soldiers' guns' tests
