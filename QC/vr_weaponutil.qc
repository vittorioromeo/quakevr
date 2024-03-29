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
        && (xWeaponId <= WID_LASER_CANNON);
}

float(entity xEnt, float xWeaponId) VR_WeaponUtil_EntHasWeapon =
{
    ASSERT(VR_WeaponUtil_IsValidWeaponId(xWeaponId));

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
    if(VR_WeaponUtil_Impl_GetEnemyDrops() == QVR_ENEMYDROPS_DISABLE)
    {
        return FALSE;
    }

    // TODO VR: (P2) bitwise or is the only one that works here, || does not work. Why?
    float eligible = (VR_WeaponUtil_Impl_GetEnemyDrops() == QVR_ENEMYDROPS_ALWAYS) |
                     (VR_WeaponUtil_EntIsEligibleForWeapon(entPlayer, xWeapon));

    float chance = xChance * cvar_hget(cvarh_vr_enemy_drops_chance_mult);

    float roll = random();

    if(eligible && (roll < chance))
    {
        CreateWeaponDrop(entEnemy, xWeapon, entEnemy.origin);
        return TRUE;
    }

    return FALSE;
}

// TODO VR: (P2) code repetition, but create ammo box uses player ent
float(entity entAmmoBox, entity entPlayer, float xWeapon, float xChance) TryAmmoBoxDrop =
{
    if(VR_WeaponUtil_Impl_GetAmmoBoxDrops() == QVR_AMMOBOXDROPS_DISABLE)
    {
        return FALSE;
    }

    // TODO VR: (P2) bitwise or is the only one that works here, || does not work. Why?
    float eligible = (VR_WeaponUtil_Impl_GetAmmoBoxDrops() == QVR_AMMOBOXDROPS_ALWAYS) |
                     (VR_WeaponUtil_EntIsEligibleForWeapon(entPlayer, xWeapon));

    float chance = xChance * cvar_hget(cvarh_vr_ammobox_drops_chance_mult);

    float roll = random();

    if(eligible && (roll < chance))
    {
        CreateAmmoBoxWeaponDrop(entPlayer, xWeapon, entAmmoBox.origin);
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

    ASSERT(FALSE);
    return -1;
}

float(float it) ItemIdToWeaponId =
{
    if(it == IID_FIST)              { return WID_FIST; }
    if(it == IID_GRAPPLE)           { return WID_GRAPPLE; }
    if(it == IID_AXE)               { return WID_AXE; }
    if(it == IID_MJOLNIR)           { return WID_MJOLNIR; }
    if(it == IID_SHOTGUN)           { return WID_SHOTGUN; }
    if(it == IID_SUPER_SHOTGUN)     { return WID_SUPER_SHOTGUN; }
    if(it == IID_NAILGUN)           { return WID_NAILGUN; }
    if(it == IID_SUPER_NAILGUN)     { return WID_SUPER_NAILGUN; }
    if(it == IID_GRENADE_LAUNCHER)  { return WID_GRENADE_LAUNCHER; }
    if(it == IID_PROXIMITY_GUN)     { return WID_PROXIMITY_GUN; }
    if(it == IID_ROCKET_LAUNCHER)   { return WID_ROCKET_LAUNCHER; }
    if(it == IID_LIGHTNING)         { return WID_LIGHTNING; }
    if(it == IID_LASER_CANNON)      { return WID_LASER_CANNON; }

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
    // TODO VR: (P2) used for velocity, but weight is used on the calculations
    // on the C++ side of things

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

    ASSERT(FALSE);
    return 0;
}

float(string wpnClassname) WeaponClassNameToWeaponId =
{
    if(wpnClassname == "weapon_grapple")         { return WID_GRAPPLE; }
    if(wpnClassname == "weapon_axe")             { return WID_AXE; }
    if(wpnClassname == "weapon_mjolnir")         { return WID_MJOLNIR; }
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

    ASSERT(FALSE);
    return -1;
}

// TODO VR: (P2) use for DropBackpack, GremlinDropBackpack, netname, etc
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

    ASSERT(FALSE);
    return -1;
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

    ASSERT(FALSE);
    return -1;
}
