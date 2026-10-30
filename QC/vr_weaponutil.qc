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

// The nail pistol (vr_nailpistol.qc, after these tables).
float() VR_NailPistol_MagSize;
float(float xWeapon, float xDir) VR_NailPistol_CycleStep;
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_NailPistol_Attack;
void() VR_NailPistol_PlayerFrame;
// The flare gun (vr_flaregun.qc, after these tables).
float() VR_FlareGun_Barrels;
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_FlareGun_Attack;
void() VR_FlareGun_PlayerFrame;
float(float xHand) VR_FlareGun_Counter;
float(float xClip) VR_FlareGun_Skin;
// The heavy revolver (vr_revolver.qc, after these tables).
float() VR_Revolver_Chambers;
float(float xWeapon, float xDir) VR_Revolver_CycleStep;
void(float xHand, float xAttackFinished, float* xOutAttackFinished) VR_Revolver_Attack;
void() VR_Revolver_PlayerFrame;
// The Survival Arena (vr_arena.qc, its perks' file after it: VR_ARK_Running is "a run is on this map now").
float() VR_ARK_Running;

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

    // A Survival Arena run (VR_ARK_Running, vr_arena.qc): its waves are the map's only source of guns -- an arena map
    // has no authored weapon pickups, and a run begins with the player stripped to the fists -- so what a foe or an
    // ammo box carries is not filtered by what the player happens to hold. A weapon this pack does not have is out of
    // it too (AD's soldiers' guns without AD's data, MG3's super axe without MG3's data); and the drop settings'
    // "never" still stands -- both drop helpers return on it before they ever ask this.
    if(VR_ARK_Running()) { return VR_Pack_WeaponAvailable(xWeaponId); }

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

    return VR_Player();
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

// The weapon table: one row per weapon id, WID_FIST .. WID_LAST in order (Quake/vr/vr_weapon_ids.h; a new weapon is a
// new row here). What each WeaponIdTo* below answers is its column. The AD guns' rows (WID_ADROCKET .. WID_ADCROSSBOW)
// hold what all of them share; their model, name and clip are vr_ad_guns_defs.qc's. A clip a setting or a function sizes
// is answered before the table (WeaponIdToBaseClipSize: its column is 0 there).
#define VR_WROW_SECONDARY 1 // takes QVR_WPNFLAG_USE_SECONDARY_AMMO (WeaponSupportsSecondaryAmmoFlag)
#define VR_WROW_RELOAD    2 // WeaponIdToSupportsReload
#define VR_WROW_FLICK     4 // WeaponIdToSupportsReloadFlicking
#define VR_WROW_STABILIZE 8 // WeaponIdToThrowStabilizeDim 2 (spun about its length as it flies), else 0
#define VR_WROW_OK(wid) ((wid) >= WID_FIST && (wid) <= WID_LAST)

typedef struct
{
    string classname;  // its map pickup (WeaponClassNameToWeaponId; "" none)
    float item;        // WeaponIdToItemId
    float ammo;        // WeaponIdToAmmoId
    float ammo2;       // the same with QVR_WPNFLAG_USE_SECONDARY_AMMO (its own when it has no second)
    string model;      // WeaponIdToModel
    string model2;     // the same with QVR_WPNFLAG_USE_SECONDARY_AMMO
    string name;       // WeaponIdToWeaponName
    float throwMult;   // WeaponIdToThrowMult (the throw's velocity; the engine's weight has its own numbers)
    float throwDamage; // WeaponIdToThrowDamage
    float throwBounds; // WeaponIdToThrowBounds
    float initialAmmo; // WeaponIdToInitialAmmoCount
    float clip;        // WeaponIdToBaseClipSize
    float flags;       // VR_WROW_*
} vr_weapon_row;

vr_weapon_row VR_WEAPONS[WID_LAST + 1] =
{
    // classname, item, ammo, ammo2, model, model2, name, throw mult, throw damage, throw bounds, initial ammo, clip, flags
    { "", IID_FIST, AID_NONE, AID_NONE, "progs/hand.mdl", "progs/hand.mdl", "Fist", 1, 20, 1, 0, 0, 0 },
    { "weapon_grapple", IID_GRAPPLE, AID_NONE, AID_NONE, "progs/v_grpple.mdl", "progs/v_grpple.mdl", "Grapple", 0.9, 20, 1, 0, 0, 0 },
    { "weapon_axe", IID_AXE, AID_NONE, AID_NONE, "progs/v_axe.mdl", "progs/v_axe.mdl", "Axe", 0.9, 35, 1, 0, 0, VR_WROW_STABILIZE },
    { "weapon_mjolnir", IID_MJOLNIR, AID_CELLS, AID_CELLS, "progs/v_hammer.mdl", "progs/v_hammer.mdl", "Mjolnir", 0.7, 30, 1.1, 30, 0, VR_WROW_STABILIZE },
    { "weapon_shotgun", IID_SHOTGUN, AID_SHELLS, AID_SHELLS, "progs/v_shot.mdl", "progs/v_shot.mdl", "Shotgun", 0.8, 20, 1.25, 5, 8, VR_WROW_RELOAD },
    { "weapon_supershotgun", IID_SUPER_SHOTGUN, AID_SHELLS, AID_SHELLS, "progs/v_shot2.mdl", "progs/v_shot2.mdl", "Double-barrelled Shotgun", 0.8, 20, 1.25, 5, 2, VR_WROW_RELOAD | VR_WROW_FLICK },
    { "weapon_nailgun", IID_NAILGUN, AID_NAILS, AID_LAVA_NAILS, "progs/v_nail.mdl", "progs/v_lava.mdl", "Nailgun", 0.8, 20, 1.25, 30, 24, VR_WROW_RELOAD | VR_WROW_SECONDARY },
    { "weapon_supernailgun", IID_SUPER_NAILGUN, AID_NAILS, AID_LAVA_NAILS, "progs/v_nail2.mdl", "progs/v_lava2.mdl", "Super Nailgun", 0.6, 25, 1.5, 30, 36, VR_WROW_RELOAD | VR_WROW_SECONDARY },
    { "weapon_grenadelauncher", IID_GRENADE_LAUNCHER, AID_ROCKETS, AID_MULTI_ROCKETS, "progs/v_rock.mdl", "progs/v_multi.mdl", "Grenade Launcher", 0.6, 30, 1.5, 5, 4, VR_WROW_RELOAD | VR_WROW_SECONDARY },
    { "weapon_proximity_gun", IID_PROXIMITY_GUN, AID_ROCKETS, AID_ROCKETS, "progs/v_prox.mdl", "progs/v_prox.mdl", "Proximity Gun", 0.6, 30, 1.5, 6, 4, VR_WROW_RELOAD },
    { "weapon_rocketlauncher", IID_ROCKET_LAUNCHER, AID_ROCKETS, AID_MULTI_ROCKETS, "progs/v_rock2.mdl", "progs/v_multi2.mdl", "Rocket Launcher", 0.6, 30, 1.7, 5, 4, VR_WROW_RELOAD | VR_WROW_SECONDARY },
    { "weapon_lightning", IID_LIGHTNING, AID_CELLS, AID_PLASMA, "progs/v_light.mdl", "progs/v_plasma.mdl", "Thunderbolt", 0.6, 25, 1.5, 15, 36, VR_WROW_RELOAD | VR_WROW_SECONDARY },
    { "weapon_laser_gun", IID_LASER_CANNON, AID_CELLS, AID_CELLS, "progs/v_laserg.mdl", "progs/v_laserg.mdl", "Laser Cannon", 0.4, 45, 2.4, 30, 0, 0 },
    // (the sword: AD's melee monsters' weapons have their own model and name, VR_ADMelee_*)
    { "", IID_SWORD, AID_NONE, AID_NONE, "progs/v_ksword.mdl", "progs/v_hksword.mdl", "Knight's Sword", 0.8, 60, 2.4, 0, 0, 0 },
    { "", IID_CHAINSAW, AID_NONE, AID_NONE, "progs/v_chainsaw.mdl", "progs/v_chainsaw.mdl", "Ogre's Chainsaw", 0.4, 50, 2.4, 0, VR_CHAINSAW_TANK, 0 },
    // (our edited grunt's gun: a 3-round burst; its clip VR_GruntGun_ClipSize, a multiple of 3, vr_enemyguns.qc)
    { "", IID_GRUNTGUN, AID_NONE, AID_NONE, "progs/v_gruntgun.mdl", "progs/v_gruntgun.mdl", "Grunt Burst Rifle", 0.7, 20, 1.6, 0, 0, 0 },
    { "", IID_ENFRIFLE, AID_NONE, AID_NONE, "progs/v_enfrifle.mdl", "progs/v_enfrifle.mdl", "Enforcer's Laser Rifle", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_CROWBAR, AID_NONE, AID_NONE, "progs/v_crowbar.mdl", "progs/v_crowbar.mdl", "Crowbar", 0.8, 40, 1.6, 0, 0, 0 },
    { "weapon_superaxe", IID_SUPERAXE, AID_CELLS, AID_CELLS, "owned/mg3/progs/v_hammer.mdl", "owned/mg3/progs/v_hammer.mdl", "Super Axe", 0.75, 50, 1.4, 0, 0, VR_WROW_STABILIZE },
    { "weapon_toolgun", IID_TOOLGUN, AID_NONE, AID_NONE, "progs/v_toolgun.mdl", "progs/v_toolgun.mdl", "Toolgun", 0.8, 10, 1.2, 0, 0, 0 },
    // AD support (vr_ad_guns_defs.qc: VR_ADGun_Model, VR_ADGun_Name, VR_ADGun_ClipSize)
    { "", IID_ADROCKET, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADGRENADE, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADPLASMA, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADDEFENDER, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADELIMINATOR, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADCENTURION, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADPYRO, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADFUMIGATOR, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "", IID_ADCROSSBOW, AID_NONE, AID_NONE, "", "", "", 0.6, 25, 1.8, 0, 0, 0 },
    { "weapon_nailpistol", IID_NAILPISTOL, AID_NAILS, AID_NAILS, "progs/v_nailpistol.mdl", "progs/v_nailpistol.mdl", "Nail Pistol", 0.9, 12, 1.0, 24, 0, VR_WROW_RELOAD },
    { "weapon_flaregun", IID_FLAREGUN, AID_NONE, AID_NONE, "progs/v_flaregun.mdl", "progs/v_flaregun.mdl", "Flare Gun", 0.9, 10, 1.0, 0, 0, 0 },
    { "weapon_revolver", IID_REVOLVER, AID_SHELLS, AID_SHELLS, "progs/v_revolver.mdl", "progs/v_revolver.mdl", "Heavy Revolver", 0.85, 15, 1.0, 12, 0, VR_WROW_RELOAD }
};

float(float wid) WeaponIdToItemId =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return -1; }
    return VR_WEAPONS[wid].item;
}

string(float wid, float xWeaponFlags) WeaponIdToModel =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return ""; }
    xWeaponFlags = VR_Pack_WeaponFlags(wid, xWeaponFlags);
    if(wid == WID_SWORD && VR_ADMelee_Kind(xWeaponFlags)) { return VR_ADMelee_Model(VR_ADMelee_Kind(xWeaponFlags)); } // AD
    if(VR_IsADGun(wid)) { return VR_ADGun_Model(wid); } // AD support (vr_ad_guns_defs.qc)
    if(VRHasBit(xWeaponFlags, QVR_WPNFLAG_USE_SECONDARY_AMMO)) { return VR_WEAPONS[wid].model2; }
    return VR_WEAPONS[wid].model;
}

float(float wid, float xWeaponFlags) WeaponIdToAmmoId =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return AID_NONE; }
    xWeaponFlags = VR_Pack_WeaponFlags(wid, xWeaponFlags);
    if(VRHasBit(xWeaponFlags, QVR_WPNFLAG_USE_SECONDARY_AMMO)) { return VR_WEAPONS[wid].ammo2; }
    return VR_WEAPONS[wid].ammo;
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
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return 1; }
    return VR_WEAPONS[wid].throwMult;
}

float(float wid) WeaponIdToThrowStabilizeDim =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return 0; }
    return (VR_WEAPONS[wid].flags & VR_WROW_STABILIZE) ? 2 : 0;
}

float(float wid) WeaponIdToThrowDamage =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return 0; }
    return VR_WEAPONS[wid].throwDamage;
}

float(float wid) WeaponIdToThrowBounds =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return 0; }
    return VR_WEAPONS[wid].throwBounds;
}

float(string wpnClassname) WeaponClassNameToWeaponId =
{
    if(wpnClassname != "")
    {
        for(float wid = WID_FIST; wid <= WID_LAST; wid++)
        {
            if(VR_WEAPONS[wid].classname == wpnClassname) { return wid; }
        }
    }

    ASSERT(FALSE);
    return WID_FIST;
}

float(float wid) WeaponIdToInitialAmmoCount =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return -1; }
    return VR_WEAPONS[wid].initialAmmo;
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
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return ""; }
    if(VR_IsADGun(wid)) { return VR_ADGun_Name(wid); } // AD support (vr_ad_guns_defs.qc)
    return VR_WEAPONS[wid].name;
}

float(float wid) WeaponSupportsSecondaryAmmoFlag =
{
    return VR_WROW_OK(wid) && (VR_WEAPONS[wid].flags & VR_WROW_SECONDARY);
}

float(float wid) WeaponIdToSupportsReload =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return -1; }
    return (VR_WEAPONS[wid].flags & VR_WROW_RELOAD) ? TRUE : FALSE;
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
    else if(wid == WID_NAILPISTOL)       { own = cvar_hget(cvarh_vr_reload_mode_npistol); }
    else if(wid == WID_REVOLVER)         { own = cvar_hget(cvarh_vr_reload_mode_revolver); }
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
    best = max(best, cvar_hget(cvarh_vr_reload_mode_npistol));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_revolver));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_widow));
    best = max(best, cvar_hget(cvarh_vr_reload_mode_adplasma));
    return best;
}

float(float wid) WeaponIdToSupportsReloadFlicking =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return -1; }
    return (VR_WEAPONS[wid].flags & VR_WROW_FLICK) ? TRUE : FALSE;
}

float(float wid) WeaponIdToBaseClipSize =
{
    if(!VR_WROW_OK(wid)) { ASSERT(FALSE); return -1; }
    // The clips a setting or a function sizes (their column is 0).
    if(wid == WID_GRUNTGUN)   { return VR_GruntGun_ClipSize(); } // a multiple of 3 (vr_enemyguns.qc)
    if(wid == WID_ENFRIFLE)   { return cvar_hget(cvarh_vr_enfrifle_ammo); }
    if(wid == WID_NAILPISTOL) { return VR_NailPistol_MagSize(); }
    if(wid == WID_FLAREGUN)   { return VR_FlareGun_Barrels(); }
    if(wid == WID_REVOLVER)   { return VR_Revolver_Chambers(); }
    if(VR_IsADGun(wid))       { return VR_ADGun_ClipSize(wid); } // AD support (vr_ad_guns_defs.qc)
    return VR_WEAPONS[wid].clip;
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

// Whether weapon `wid` (its flags `xFlags`) goes into a holster (immersive, or a quick slot's copy): not a huge one, which
// is carried by hand or left (AD's hunter ogres' hammer and mace: VR_ADMelee_Holsterable). The holster refuses it
// (weapons.qc VRHolsterRefused: let go at a holster, it falls; a quick slot keeps none).
float(float wid, float xFlags) WeaponIdToHolsterable =
{
    if(wid == WID_SWORD)
    {
        return VR_ADMelee_Holsterable(VR_ADMelee_Kind(xFlags));
    }
    return TRUE;
};

