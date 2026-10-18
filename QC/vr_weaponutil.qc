//
//
//
// ----------------------------------------------------------------------------
// Implementation utilities
// ----------------------------------------------------------------------------

float() VR_WeaponUtil_Impl_GetEnemyDrops =
{
    return cvar_hget(cvarh_vr_enemy_drops);
}

float() VR_WeaponUtil_Impl_GetAmmoBoxDrops =
{
    return cvar_hget(cvarh_vr_ammobox_drops);
}

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float(float xWeaponId) VR_WeaponUtil_IsValidWeaponId =
{
    return (xWeaponId >= WID_FIST)
        && (xWeaponId <= WID_LAST);
}

float(entity xEnt, float xWeaponId) VR_WeaponUtil_EntHasWeapon =
{
    ASSERT(VR_WeaponUtil_IsValidWeaponId(xWeaponId));

    if(!VR_Pack_WeaponAvailable(xWeaponId)) { return FALSE; }

    if(xWeaponId == WID_FIST)
    {
        return TRUE;
    }

    return VR_ItemUtil_EntHasItem(xEnt, WeaponIdToItemId(xWeaponId));
}

float(entity xEnt, float xWeaponId) VR_WeaponUtil_EntIsEligibleForWeapon =
{
    if(xWeaponId == WID_FIST)    { return TRUE; }
    if(xWeaponId == WID_AXE)     { return TRUE; }
    if(xWeaponId == WID_SHOTGUN) { return TRUE; }

    return VR_WeaponUtil_EntHasWeapon(xEnt, xWeaponId);
}

void(entity xEnt, float xWeaponId) VR_WeaponUtil_EntAddWeapon =
{
    VR_ItemUtil_EntAddItem(xEnt, WeaponIdToItemId(xWeaponId));
}

void(entity xEnt, float xWeaponId) VR_WeaponUtil_EntDelWeapon =
{
    VR_ItemUtil_EntDelItem(xEnt, WeaponIdToItemId(xWeaponId));
}

//
//
//
// ----------------------------------------------------------------------------
// TODO: cleanup
// ----------------------------------------------------------------------------

entity() FindEntPlayerForDrop =
{
    if(VR_WeaponUtil_Impl_GetEnemyDrops() == QVR_ENEMYDROPS_DISABLE)
    {
        return world;
    }

    return find(world, classname, "player");
}

float(entity entEnemy, entity entPlayer, float xWeapon, float xChance) TryEnemyDrop =
{
    if(!VR_Pack_WeaponAvailable(xWeapon)) { return FALSE; }

    if(VR_WeaponUtil_Impl_GetEnemyDrops() == QVR_ENEMYDROPS_DISABLE)
    {
        return FALSE;
    }

    // (| of two truth values, 0 or 1.)
    float eligible = (VR_WeaponUtil_Impl_GetEnemyDrops() == QVR_ENEMYDROPS_ALWAYS) |
                     (VR_WeaponUtil_EntIsEligibleForWeapon(entPlayer, xWeapon));

    float roll = random(); // (the chance multiplier is ShouldDrop*'s, applied once)

    if(eligible && (roll < xChance))
    {
        CreateWeaponDrop(entEnemy, xWeapon, entEnemy.origin);
        return TRUE;
    }

    return FALSE;
}

float(entity entAmmoBox, entity entPlayer, float xWeapon, float xChance) TryAmmoBoxDrop =
{
    if(!VR_Pack_WeaponAvailable(xWeapon)) { return FALSE; }

    if(VR_WeaponUtil_Impl_GetAmmoBoxDrops() == QVR_AMMOBOXDROPS_DISABLE)
    {
        return FALSE;
    }

    // (| of two truth values, 0 or 1.)
    float eligible = (VR_WeaponUtil_Impl_GetAmmoBoxDrops() == QVR_AMMOBOXDROPS_ALWAYS) |
                     (VR_WeaponUtil_EntIsEligibleForWeapon(entPlayer, xWeapon));

    float roll = random(); // (the chance multiplier is ShouldDrop*'s, applied once)

    if(eligible && (roll < xChance))
    {
        // On top of the box, over its middle (its origin is a corner on the floor: a weapon made there was half in
        // the floor). It falls onto the box, a rigid body, and rests there or tumbles off.
        vector at = entAmmoBox.origin + (entAmmoBox.mins + entAmmoBox.maxs) * 0.5;
        at_z = entAmmoBox.origin_z + entAmmoBox.maxs_z + 10;
        CreateAmmoBoxWeaponDrop(entPlayer, xWeapon, at);
        return TRUE;
    }

    return FALSE;
}

float(float wid) WeaponIdToItemId =
{
    if(wid == WID_FIST)             { return IID_FIST; }
    if(wid == WID_GRAPPLE)          { return IID_GRAPPLE; }
    if(wid == WID_AXE)              { return IID_AXE; }
    if(wid == WID_MJOLNIR)          { return IID_MJOLNIR; }
    if(wid == WID_SHOTGUN)          { return IID_SHOTGUN; }
    if(wid == WID_SUPER_SHOTGUN)    { return IID_SUPER_SHOTGUN; }
    if(wid == WID_NAILGUN)          { return IID_NAILGUN; }
    if(wid == WID_SUPER_NAILGUN)    { return IID_SUPER_NAILGUN; }
    if(wid == WID_GRENADE_LAUNCHER) { return IID_GRENADE_LAUNCHER; }
    if(wid == WID_PROXIMITY_GUN)    { return IID_PROXIMITY_GUN; }
    if(wid == WID_ROCKET_LAUNCHER)  { return IID_ROCKET_LAUNCHER; }
    if(wid == WID_LIGHTNING)        { return IID_LIGHTNING; }
    if(wid == WID_LASER_CANNON)     { return IID_LASER_CANNON; }
    if(wid == WID_SWORD)            { return IID_SWORD; }
    if(wid == WID_CHAINSAW)         { return IID_CHAINSAW; }
    if(wid == WID_GRUNTGUN)         { return IID_GRUNTGUN; }
    if(wid == WID_ENFRIFLE)         { return IID_ENFRIFLE; }
    if(wid == WID_CROWBAR)          { return IID_CROWBAR; }
    if(wid == WID_SUPERAXE)         { return IID_SUPERAXE; }
    if(wid == WID_TOOLGUN)          { return IID_TOOLGUN; }

    if(VR_IsADGun(wid))             { return IID_ADROCKET + (wid - WID_ADROCKET); } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return -1;
}

string(float xWeaponFlags, string xPrimaryModel, string xSecondaryModel) SelectModelByFlags =
{
    if(VRHasBit(xWeaponFlags, QVR_WPNFLAG_USE_SECONDARY_AMMO))
    {
        return xSecondaryModel;
    }

    return xPrimaryModel;
}

string(float wid, float xWeaponFlags) WeaponIdToModel =
{
    xWeaponFlags = VR_Pack_WeaponFlags(wid, xWeaponFlags);
    if(wid == WID_FIST)             { return "progs/hand.mdl"; }
    if(wid == WID_GRAPPLE)          { return "progs/v_grpple.mdl"; }
    if(wid == WID_AXE)              { return "progs/v_axe.mdl"; }
    if(wid == WID_MJOLNIR)          { return "progs/v_hammer.mdl"; }
    if(wid == WID_SHOTGUN)          { return "progs/v_shot.mdl"; }
    if(wid == WID_SUPER_SHOTGUN)    { return "progs/v_shot2.mdl"; }
    if(wid == WID_NAILGUN)          { return SelectModelByFlags(xWeaponFlags, "progs/v_nail.mdl", "progs/v_lava.mdl"); }
    if(wid == WID_SUPER_NAILGUN)    { return SelectModelByFlags(xWeaponFlags, "progs/v_nail2.mdl", "progs/v_lava2.mdl"); }
    if(wid == WID_GRENADE_LAUNCHER) { return SelectModelByFlags(xWeaponFlags, "progs/v_rock.mdl", "progs/v_multi.mdl"); }
    if(wid == WID_PROXIMITY_GUN)    { return "progs/v_prox.mdl"; }
    if(wid == WID_ROCKET_LAUNCHER)  { return SelectModelByFlags(xWeaponFlags, "progs/v_rock2.mdl", "progs/v_multi2.mdl"); }
    if(wid == WID_LIGHTNING)        { return SelectModelByFlags(xWeaponFlags, "progs/v_light.mdl", "progs/v_plasma.mdl"); }
    if(wid == WID_LASER_CANNON)     { return "progs/v_laserg.mdl"; }
    if(wid == WID_SWORD && VR_ADMelee_Kind(xWeaponFlags)) { return VR_ADMelee_Model(VR_ADMelee_Kind(xWeaponFlags)); } // AD
    if(wid == WID_SWORD)            { return SelectModelByFlags(xWeaponFlags, "progs/v_ksword.mdl", "progs/v_hksword.mdl"); }
    if(wid == WID_CHAINSAW)         { return "progs/v_chainsaw.mdl"; }
    if(wid == WID_GRUNTGUN)         { return "progs/v_gruntgun.mdl"; }
    if(wid == WID_ENFRIFLE)         { return "progs/v_enfrifle.mdl"; }
    if(wid == WID_CROWBAR)          { return "progs/v_crowbar.mdl"; }
    if(wid == WID_SUPERAXE)         { return "owned/mg3/progs/v_hammer.mdl"; }
    if(wid == WID_TOOLGUN)          { return "progs/v_toolgun.mdl"; }

    if(VR_IsADGun(wid))             { return VR_ADGun_Model(wid); } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return "";
}

float(float xWeaponFlags, float xPrimaryAid, float xSecondaryAid) SelectAmmoByFlags =
{
    if(VRHasBit(xWeaponFlags, QVR_WPNFLAG_USE_SECONDARY_AMMO))
    {
        return xSecondaryAid;
    }

    return xPrimaryAid;
}

float(float wid, float xWeaponFlags) WeaponIdToAmmoId =
{
    xWeaponFlags = VR_Pack_WeaponFlags(wid, xWeaponFlags);
    if(wid == WID_FIST)             { return AID_NONE; }
    if(wid == WID_GRAPPLE)          { return AID_NONE; }
    if(wid == WID_AXE)              { return AID_NONE; }
    if(wid == WID_MJOLNIR)          { return AID_CELLS; }
    if(wid == WID_SHOTGUN)          { return AID_SHELLS; }
    if(wid == WID_SUPER_SHOTGUN)    { return AID_SHELLS; }
    if(wid == WID_NAILGUN)          { return SelectAmmoByFlags(xWeaponFlags, AID_NAILS, AID_LAVA_NAILS); }
    if(wid == WID_SUPER_NAILGUN)    { return SelectAmmoByFlags(xWeaponFlags, AID_NAILS, AID_LAVA_NAILS); }
    if(wid == WID_GRENADE_LAUNCHER) { return SelectAmmoByFlags(xWeaponFlags, AID_ROCKETS, AID_MULTI_ROCKETS); }
    if(wid == WID_PROXIMITY_GUN)    { return AID_ROCKETS; }
    if(wid == WID_ROCKET_LAUNCHER)  { return SelectAmmoByFlags(xWeaponFlags, AID_ROCKETS, AID_MULTI_ROCKETS); }
    if(wid == WID_LIGHTNING)        { return SelectAmmoByFlags(xWeaponFlags, AID_CELLS, AID_PLASMA); }
    if(wid == WID_LASER_CANNON)     { return AID_CELLS; }
    if(wid == WID_SWORD)            { return AID_NONE; }
    if(wid == WID_CHAINSAW)         { return AID_NONE; }
    if(wid == WID_GRUNTGUN)         { return AID_NONE; }
    if(wid == WID_ENFRIFLE)         { return AID_NONE; }
    if(wid == WID_CROWBAR)          { return AID_NONE; }
    if(wid == WID_SUPERAXE)         { return AID_CELLS; }
    if(wid == WID_TOOLGUN)          { return AID_NONE; }

    if(VR_IsADGun(wid))             { return AID_NONE; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return AID_NONE;
}

float(entity xPlayer, float wid, float xWeaponFlags) PlayerHasAmmoForWeapon =
{
    float ammoId;
    ammoId = WeaponIdToAmmoId(wid, xWeaponFlags);

    if(ammoId == AID_NONE)
    {
        return TRUE;
    }

    return VR_AmmoUtil_GetAmmoCountByAmmoId(xPlayer, ammoId) > 0;
}

float(float wid) WeaponIdToThrowMult =
{
    // (Used for the throw's velocity; the engine's weight has its own numbers.)

    if(wid == WID_FIST)             { return 1; }
    if(wid == WID_GRAPPLE)          { return 0.9; }
    if(wid == WID_AXE)              { return 0.9; }
    if(wid == WID_MJOLNIR)          { return 0.7; }
    if(wid == WID_SHOTGUN)          { return 0.8; }
    if(wid == WID_SUPER_SHOTGUN)    { return 0.8; }
    if(wid == WID_NAILGUN)          { return 0.8; }
    if(wid == WID_SUPER_NAILGUN)    { return 0.6; }
    if(wid == WID_GRENADE_LAUNCHER) { return 0.6; }
    if(wid == WID_PROXIMITY_GUN)    { return 0.6; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 0.6; }
    if(wid == WID_LIGHTNING)        { return 0.6; }
    if(wid == WID_LASER_CANNON)     { return 0.4; }
    if(wid == WID_SWORD)            { return 0.8; }
    if(wid == WID_CHAINSAW)         { return 0.4; }
    if(wid == WID_GRUNTGUN)         { return 0.7; }
    if(wid == WID_ENFRIFLE)         { return 0.6; }
    if(wid == WID_CROWBAR)          { return 0.8; }
    if(wid == WID_SUPERAXE)         { return 0.75; }
    if(wid == WID_TOOLGUN)          { return 0.8; }

    if(VR_IsADGun(wid))             { return 0.6; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return 1;
}

float(float wid) WeaponIdToThrowStabilizeDim =
{
    if(wid == WID_FIST)             { return 0; }
    if(wid == WID_GRAPPLE)          { return 0; }
    if(wid == WID_AXE)              { return 2; }
    if(wid == WID_MJOLNIR)          { return 2; }
    if(wid == WID_SHOTGUN)          { return 0; }
    if(wid == WID_SUPER_SHOTGUN)    { return 0; }
    if(wid == WID_NAILGUN)          { return 0; }
    if(wid == WID_SUPER_NAILGUN)    { return 0; }
    if(wid == WID_GRENADE_LAUNCHER) { return 0; }
    if(wid == WID_PROXIMITY_GUN)    { return 0; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 0; }
    if(wid == WID_LIGHTNING)        { return 0; }
    if(wid == WID_LASER_CANNON)     { return 0; }
    if(wid == WID_SWORD)            { return 0; }
    if(wid == WID_CHAINSAW)         { return 0; }
    if(wid == WID_GRUNTGUN)         { return 0; }
    if(wid == WID_ENFRIFLE)         { return 0; }
    if(wid == WID_CROWBAR)          { return 0; }
    if(wid == WID_SUPERAXE)         { return 2; }
    if(wid == WID_TOOLGUN)          { return 0; }

    if(VR_IsADGun(wid))             { return 0; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return 0;
}

float(float wid) WeaponIdToThrowDamage =
{
    if(wid == WID_FIST)             { return 20; }
    if(wid == WID_GRAPPLE)          { return 20; }
    if(wid == WID_AXE)              { return 35; }
    if(wid == WID_MJOLNIR)          { return 30; }
    if(wid == WID_SHOTGUN)          { return 20; }
    if(wid == WID_SUPER_SHOTGUN)    { return 20; }
    if(wid == WID_NAILGUN)          { return 20; }
    if(wid == WID_SUPER_NAILGUN)    { return 25; }
    if(wid == WID_GRENADE_LAUNCHER) { return 30; }
    if(wid == WID_PROXIMITY_GUN)    { return 30; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 30; }
    if(wid == WID_LIGHTNING)        { return 25; }
    if(wid == WID_LASER_CANNON)     { return 45; }
    if(wid == WID_SWORD)            { return 60; }
    if(wid == WID_CHAINSAW)         { return 50; }
    if(wid == WID_GRUNTGUN)         { return 20; }
    if(wid == WID_ENFRIFLE)         { return 25; }
    if(wid == WID_CROWBAR)          { return 40; }
    if(wid == WID_SUPERAXE)         { return 50; }
    if(wid == WID_TOOLGUN)          { return 10; }

    if(VR_IsADGun(wid))             { return 25; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return 0;
}

float(float wid) WeaponIdToThrowBounds =
{
    if(wid == WID_FIST)             { return 1; }
    if(wid == WID_GRAPPLE)          { return 1; }
    if(wid == WID_AXE)              { return 1; }
    if(wid == WID_MJOLNIR)          { return 1.1; }
    if(wid == WID_SHOTGUN)          { return 1.25; }
    if(wid == WID_SUPER_SHOTGUN)    { return 1.25; }
    if(wid == WID_NAILGUN)          { return 1.25; }
    if(wid == WID_SUPER_NAILGUN)    { return 1.5; }
    if(wid == WID_GRENADE_LAUNCHER) { return 1.5; }
    if(wid == WID_PROXIMITY_GUN)    { return 1.5; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 1.7; }
    if(wid == WID_LIGHTNING)        { return 1.5; }
    if(wid == WID_LASER_CANNON)     { return 2.4; }
    if(wid == WID_SWORD)            { return 2.4; }
    if(wid == WID_CHAINSAW)         { return 2.4; }
    if(wid == WID_GRUNTGUN)         { return 1.6; }
    if(wid == WID_ENFRIFLE)         { return 1.8; }
    if(wid == WID_CROWBAR)          { return 1.6; }
    if(wid == WID_SUPERAXE)         { return 1.4; }
    if(wid == WID_TOOLGUN)          { return 1.2; }

    if(VR_IsADGun(wid))             { return 1.8; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return 0;
}

float(string wpnClassname) WeaponClassNameToWeaponId =
{
    if(wpnClassname == "weapon_grapple")         { return WID_GRAPPLE; }
    if(wpnClassname == "weapon_axe")             { return WID_AXE; }
    if(wpnClassname == "weapon_mjolnir")         { return WID_MJOLNIR; }
    if(wpnClassname == "weapon_superaxe")        { return WID_SUPERAXE; }
    if(wpnClassname == "weapon_toolgun")         { return WID_TOOLGUN; }
    if(wpnClassname == "weapon_shotgun")         { return WID_SHOTGUN; }
    if(wpnClassname == "weapon_supershotgun")    { return WID_SUPER_SHOTGUN; }
    if(wpnClassname == "weapon_nailgun")         { return WID_NAILGUN; }
    if(wpnClassname == "weapon_supernailgun")    { return WID_SUPER_NAILGUN; }
    if(wpnClassname == "weapon_grenadelauncher") { return WID_GRENADE_LAUNCHER; }
    if(wpnClassname == "weapon_proximity_gun")   { return WID_PROXIMITY_GUN; }
    if(wpnClassname == "weapon_rocketlauncher")  { return WID_ROCKET_LAUNCHER; }
    if(wpnClassname == "weapon_lightning")       { return WID_LIGHTNING; }
    if(wpnClassname == "weapon_laser_gun")       { return WID_LASER_CANNON; }

    ASSERT(FALSE);
    return WID_FIST;
}

float(float wid) WeaponIdToInitialAmmoCount =
{
    if(wid == WID_FIST)             { return 0; }
    if(wid == WID_GRAPPLE)          { return 0; }
    if(wid == WID_AXE)              { return 0; }
    if(wid == WID_MJOLNIR)          { return 30; }
    if(wid == WID_SHOTGUN)          { return 5; }
    if(wid == WID_SUPER_SHOTGUN)    { return 5; }
    if(wid == WID_NAILGUN)          { return 30; }
    if(wid == WID_SUPER_NAILGUN)    { return 30; }
    if(wid == WID_GRENADE_LAUNCHER) { return 5; }
    if(wid == WID_PROXIMITY_GUN)    { return 6; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 5; }
    if(wid == WID_LIGHTNING)        { return 15; }
    if(wid == WID_LASER_CANNON)     { return 30; }
    if(wid == WID_SWORD)            { return 0; }
    if(wid == WID_CHAINSAW)         { return 0; }
    if(wid == WID_GRUNTGUN)         { return 0; }
    if(wid == WID_ENFRIFLE)         { return 0; }
    if(wid == WID_CROWBAR)          { return 0; }
    if(wid == WID_SUPERAXE)         { return 0; }
    if(wid == WID_TOOLGUN)          { return 0; }

    if(VR_IsADGun(wid))             { return 0; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return -1;
}

string(float wid) WeaponIdToWeaponName;

// Its name as this one is (its flags: AD support, an AD melee monster's weapon of the knight's sword's id has its own:
// vr_ad_guns_defs.qc).
string(float wid, float xWeaponFlags) WeaponNameOf =
{
    if(wid == WID_SWORD && VR_ADMelee_Kind(xWeaponFlags))
    {
        return VR_ADMelee_Name(VR_ADMelee_Kind(xWeaponFlags));
    }
    if(wid == WID_SWORD && (xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO))
    {
        return "Hell Knight's Sword";
    }
    return WeaponIdToWeaponName(wid);
};

string(float wid) WeaponIdToWeaponName =
{
    if(wid == WID_FIST)             { return "Fist"; }
    if(wid == WID_GRAPPLE)          { return "Grapple"; }
    if(wid == WID_AXE)              { return "Axe"; }
    if(wid == WID_MJOLNIR)          { return "Mjolnir"; }
    if(wid == WID_SHOTGUN)          { return "Shotgun"; }
    if(wid == WID_SUPER_SHOTGUN)    { return "Double-barrelled Shotgun"; }
    if(wid == WID_NAILGUN)          { return "Nailgun"; }
    if(wid == WID_SUPER_NAILGUN)    { return "Super Nailgun"; }
    if(wid == WID_GRENADE_LAUNCHER) { return "Grenade Launcher"; }
    if(wid == WID_PROXIMITY_GUN)    { return "Proximity Gun"; }
    if(wid == WID_ROCKET_LAUNCHER)  { return "Rocket Launcher"; }
    if(wid == WID_LIGHTNING)        { return "Thunderbolt"; }
    if(wid == WID_LASER_CANNON)     { return "Laser Cannon"; }
    if(wid == WID_SWORD)            { return "Knight's Sword"; }
    if(wid == WID_CHAINSAW)         { return "Ogre's Chainsaw"; }
    if(wid == WID_GRUNTGUN)         { return "Grunt Burst Rifle"; } // (our edited grunt's gun: a 3-round burst)
    if(wid == WID_ENFRIFLE)         { return "Enforcer's Laser Rifle"; }
    if(wid == WID_CROWBAR)          { return "Crowbar"; }
    if(wid == WID_SUPERAXE)         { return "Super Axe"; }
    if(wid == WID_TOOLGUN)          { return "Toolgun"; }

    if(VR_IsADGun(wid))             { return VR_ADGun_Name(wid); } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return "";
}

float(float wid) WeaponSupportsSecondaryAmmoFlag =
{
    if(wid == WID_NAILGUN)          { return TRUE; }
    if(wid == WID_SUPER_NAILGUN)    { return TRUE; }
    if(wid == WID_GRENADE_LAUNCHER) { return TRUE; }
    if(wid == WID_ROCKET_LAUNCHER)  { return TRUE; }
    if(wid == WID_LIGHTNING)        { return TRUE; }

    return FALSE;
}

float(float wid) WeaponIdToSupportsReload =
{
    if(wid == WID_FIST)             { return FALSE; }
    if(wid == WID_GRAPPLE)          { return FALSE; }
    if(wid == WID_AXE)              { return FALSE; }
    if(wid == WID_MJOLNIR)          { return FALSE; }
    if(wid == WID_SHOTGUN)          { return TRUE; }
    if(wid == WID_SUPER_SHOTGUN)    { return TRUE; }
    if(wid == WID_NAILGUN)          { return TRUE; }
    if(wid == WID_SUPER_NAILGUN)    { return TRUE; }
    if(wid == WID_GRENADE_LAUNCHER) { return TRUE; }
    if(wid == WID_PROXIMITY_GUN)    { return TRUE; }
    if(wid == WID_ROCKET_LAUNCHER)  { return TRUE; }
    if(wid == WID_LIGHTNING)        { return TRUE; }
    if(wid == WID_LASER_CANNON)     { return FALSE; }
    if(wid == WID_SWORD)            { return FALSE; }
    if(wid == WID_CHAINSAW)         { return FALSE; }
    if(wid == WID_GRUNTGUN)         { return FALSE; }
    if(wid == WID_ENFRIFLE)         { return FALSE; }
    if(wid == WID_CROWBAR)          { return FALSE; }
    if(wid == WID_SUPERAXE)         { return FALSE; }
    if(wid == WID_TOOLGUN)          { return FALSE; }

    if(VR_IsADGun(wid))             { return FALSE; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return -1;
}

// The reloading mode of the weapon `wid` (QVR_RELOADMODE_*): its own vr_reload_mode_<gun> (the Per-Weapon Reload
// Overrides; -1 follows), else vr_reload_mode. A weapon that doesn't reload takes the global mode.
float(float wid) VR_Reload_ModeOf =
{
    float own = -1;
    if(wid == WID_SHOTGUN)               { own = cvar_hget(cvarh_vr_reload_mode_shot); }
    else if(wid == WID_SUPER_SHOTGUN)    { own = cvar_hget(cvarh_vr_reload_mode_sshot); }
    else if(wid == WID_NAILGUN)          { own = cvar_hget(cvarh_vr_reload_mode_nail); }
    else if(wid == WID_SUPER_NAILGUN)    { own = cvar_hget(cvarh_vr_reload_mode_snail); }
    else if(wid == WID_GRENADE_LAUNCHER) { own = cvar_hget(cvarh_vr_reload_mode_gl); }
    else if(wid == WID_PROXIMITY_GUN)    { own = cvar_hget(cvarh_vr_reload_mode_prox); }
    else if(wid == WID_ROCKET_LAUNCHER)  { own = cvar_hget(cvarh_vr_reload_mode_rl); }
    else if(wid == WID_LIGHTNING)        { own = cvar_hget(cvarh_vr_reload_mode_light); }
    return own >= 0 ? own : cvar_hget(cvarh_vr_reload_mode);
}

// The same of the weapon `wid` whose record is `inst` (world: not known): AD support: the Widowmaker and the Plasma Gun
// (the super shotgun and the thunderbolt with AD's upgrade) have their own (vr_reload_mode_widow, _adplasma; -1 the
// global, not their plain guns').
float(float wid, entity inst) VR_Reload_ModeOfInst =
{
    float own = -1;
    if(wid == WID_SUPER_SHOTGUN && AD_Widowmaker(inst))    { own = cvar_hget(cvarh_vr_reload_mode_widow); }
    else if(wid == WID_LIGHTNING && AD_PlasmaGunInst(inst)) { own = cvar_hget(cvarh_vr_reload_mode_adplasma); }
    else                                                    { return VR_Reload_ModeOf(wid); }
    return own >= 0 ? own : cvar_hget(cvarh_vr_reload_mode);
}

// The highest reloading mode of any gun (the global mode and every override): whether the pouch, the magazines and the
// rest of the immersive reloading's furniture are wanted at all.
float() VR_Reload_BestMode =
{
    float best = cvar_hget(cvarh_vr_reload_mode);
    best = max(best, cvar_hget(cvarh_vr_reload_mode_shot));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_sshot));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_nail));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_snail));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_gl));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_prox));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_rl));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_light));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_widow));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_adplasma));
    return best;
}

float(float wid) WeaponIdToSupportsReloadFlicking =
{
    if(wid == WID_FIST)             { return FALSE; }
    if(wid == WID_GRAPPLE)          { return FALSE; }
    if(wid == WID_AXE)              { return FALSE; }
    if(wid == WID_MJOLNIR)          { return FALSE; }
    if(wid == WID_SHOTGUN)          { return FALSE; }
    if(wid == WID_SUPER_SHOTGUN)    { return TRUE; }
    if(wid == WID_NAILGUN)          { return FALSE; }
    if(wid == WID_SUPER_NAILGUN)    { return FALSE; }
    if(wid == WID_GRENADE_LAUNCHER) { return FALSE; }
    if(wid == WID_PROXIMITY_GUN)    { return FALSE; }
    if(wid == WID_ROCKET_LAUNCHER)  { return FALSE; }
    if(wid == WID_LIGHTNING)        { return FALSE; }
    if(wid == WID_LASER_CANNON)     { return FALSE; }
    if(wid == WID_SWORD)            { return FALSE; }
    if(wid == WID_CHAINSAW)         { return FALSE; }
    if(wid == WID_GRUNTGUN)         { return FALSE; }
    if(wid == WID_ENFRIFLE)         { return FALSE; }
    if(wid == WID_CROWBAR)          { return FALSE; }
    if(wid == WID_SUPERAXE)         { return FALSE; }
    if(wid == WID_TOOLGUN)          { return FALSE; }

    if(VR_IsADGun(wid))             { return FALSE; } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return -1;
}

float(float wid) WeaponIdToBaseClipSize =
{
    if(wid == WID_FIST)             { return 0; }
    if(wid == WID_GRAPPLE)          { return 0; }
    if(wid == WID_AXE)              { return 0; }
    if(wid == WID_MJOLNIR)          { return 0; }
    if(wid == WID_SHOTGUN)          { return 8; }
    if(wid == WID_SUPER_SHOTGUN)    { return 2; }
    if(wid == WID_NAILGUN)          { return 24; }
    if(wid == WID_SUPER_NAILGUN)    { return 36; }
    if(wid == WID_GRENADE_LAUNCHER) { return 4; }
    if(wid == WID_PROXIMITY_GUN)    { return 4; }
    if(wid == WID_ROCKET_LAUNCHER)  { return 4; }
    if(wid == WID_LIGHTNING)        { return 36; }
    if(wid == WID_LASER_CANNON)     { return 0; }
    if(wid == WID_SWORD)            { return 0; }
    if(wid == WID_CHAINSAW)         { return VR_CHAINSAW_TANK; }
    if(wid == WID_GRUNTGUN)         { return VR_GruntGun_ClipSize(); } // a multiple of 3 (vr_enemyguns.qc)
    if(wid == WID_ENFRIFLE)         { return cvar_hget(cvarh_vr_enfrifle_ammo); }
    if(wid == WID_CROWBAR)          { return 0; }
    if(wid == WID_SUPERAXE)         { return 0; }
    if(wid == WID_TOOLGUN)          { return 0; }

    if(VR_IsADGun(wid))             { return VR_ADGun_ClipSize(wid); } // AD support (vr_ad_guns_defs.qc)

    ASSERT(FALSE);
    return -1;
}

// The magazine size of the weapon `wid` whose record is `inst` (world: not known, a plain one): AD support: the
// Widowmaker's tube (vr_ad_widowmaker_tube), else its kind's.
float(float wid, entity inst) VR_ClipSizeOf =
{
    if(wid == WID_SUPER_SHOTGUN && AD_Widowmaker(inst))
    {
        return max(2, rint(cvar_hget(cvarh_vr_ad_widowmaker_tube)));
    }
    return WeaponIdToBaseClipSize(wid);
}

// The same of the weapon `p` holds in `xHand`.
float(entity p, float xHand) VR_ClipSizeIn =
{
    return VR_ClipSizeOf(VRGetEntWeapon(p, xHand), VRGetEntWeaponInst(p, xHand));
}
