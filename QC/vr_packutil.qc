// Saved native campaign context; engine restores it before map/resource spawn.
float vr_save_campaign;
// Saved precache layout: 1 + Hipnotic + 2*Rogue. It must match when a save is restored.
float vr_save_packmask;

// Optional mission packs. The engine validates their own archives before mounting them.
float() VR_Pack_Hipnotic = { return cvar("vr_hipnotic_available"); };
float() VR_Pack_Rogue = { return cvar("vr_rogue_available"); };

float(float wid) VR_Pack_WeaponAvailable =
{
    if(wid == WID_GRAPPLE) { return VR_Pack_Rogue(); }
    if(wid == WID_MJOLNIR || wid == WID_PROXIMITY_GUN || wid == WID_LASER_CANNON)
    {
        return VR_Pack_Hipnotic();
    }
    return TRUE;
};

float(float wid, float flags) VR_Pack_WeaponFlags =
{
    if(!VR_Pack_Rogue() && (wid == WID_NAILGUN || wid == WID_SUPER_NAILGUN ||
        wid == WID_GRENADE_LAUNCHER || wid == WID_ROCKET_LAUNCHER || wid == WID_LIGHTNING))
    {
        return flags - (flags & QVR_WPNFLAG_USE_SECONDARY_AMMO);
    }
    return flags;
};

float(float iid) VR_Pack_ItemAvailable =
{
    if(iid >= IID_LAVA_NAILGUN && iid <= IID_V_SPHERE) { return VR_Pack_Rogue(); }
    if((iid >= IID_WETSUIT && iid <= IID_HORN_OF_CONJURING) || iid == IID_MJOLNIR ||
        iid == IID_PROXIMITY_GUN || iid == IID_LASER_CANNON) { return VR_Pack_Hipnotic(); }
    return TRUE;
};

float(float available, string pack) VR_Pack_RequireSpawn =
{
    if(available) { return TRUE; }
    dprint(sprintf("VR: unavailable %s entity %s skipped\n", pack, self.classname));
    remove(self);
    return FALSE;
};
