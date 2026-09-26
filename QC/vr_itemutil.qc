//
//
//
// ----------------------------------------------------------------------------
// Implementation utilities
// ----------------------------------------------------------------------------

void(string xFnName, float xItemId) VR_ItemUtil_Impl_PrintInvalidItemId =
{
    bprint("`");
    bprint(xFnName);
    bprint("`: invalid item id '");
    bprint(ftos(xItemId));
    bprint("'\n");
}

void(string xFnName, float xItemId) VR_ItemUtil_Impl_PrintInvalidItemCategory =
{
    bprint("`");
    bprint(xFnName);
    bprint("`: invalid item category for item '");
    bprint(ftos(xItemId));
    bprint("'\n");
}

// Indexed by item id (IID_*, vr_defs.qc): its bit, and which of .items, .hipnotic_items and
// .rogue_items holds it (tables: an array accessor's few comparisons, not a chain of 43).
float vr_itemutil_flag[44] =
{
    0, // 0 IID_NONE
    IT_ARMOR1, // 1 IID_ARMOR1
    IT_ARMOR2, // 2 IID_ARMOR2
    IT_ARMOR3, // 3 IID_ARMOR3
    IT_SUPERHEALTH, // 4 IID_SUPERHEALTH
    IT_KEY1, // 5 IID_KEY1
    IT_KEY2, // 6 IID_KEY2
    IT_INVISIBILITY, // 7 IID_INVISIBILITY
    IT_INVULNERABILITY, // 8 IID_INVULNERABILITY
    IT_SUIT, // 9 IID_SUIT
    IT_QUAD, // 10 IID_QUAD
    HIP_IT_WETSUIT, // 11 IID_WETSUIT
    HIP_IT_EMPATHY_SHIELDS, // 12 IID_EMPATHY_SHIELDS
    HIP_IT_HORN_OF_CONJURING, // 13 IID_HORN_OF_CONJURING
    IT_FIST, // 14 IID_FIST
    IT_SHOTGUN, // 15 IID_SHOTGUN
    IT_SUPER_SHOTGUN, // 16 IID_SUPER_SHOTGUN
    IT_NAILGUN, // 17 IID_NAILGUN
    IT_SUPER_NAILGUN, // 18 IID_SUPER_NAILGUN
    IT_GRENADE_LAUNCHER, // 19 IID_GRENADE_LAUNCHER
    IT_ROCKET_LAUNCHER, // 20 IID_ROCKET_LAUNCHER
    IT_LIGHTNING, // 21 IID_LIGHTNING
    IT_MJOLNIR, // 22 IID_MJOLNIR
    IT_SHELLS, // 23 IID_SHELLS
    IT_NAILS, // 24 IID_NAILS
    IT_ROCKETS, // 25 IID_ROCKETS
    IT_CELLS, // 26 IID_CELLS
    IT_AXE, // 27 IID_AXE
    HIP_IT_PROXIMITY_GUN, // 28 IID_PROXIMITY_GUN
    IT_LASER_CANNON, // 29 IID_LASER_CANNON
    ROGUE_IT_LAVA_NAILGUN, // 30 IID_LAVA_NAILGUN
    ROGUE_IT_LAVA_SUPER_NAILGUN, // 31 IID_LAVA_SUPER_NAILGUN
    ROGUE_IT_MULTI_GRENADE, // 32 IID_MULTI_GRENADE
    ROGUE_IT_MULTI_ROCKET, // 33 IID_MULTI_ROCKET
    ROGUE_IT_PLASMA_GUN, // 34 IID_PLASMA_GUN
    ROGUE_IT_GRAPPLE, // 35 IID_GRAPPLE
    ROGUE_IT_LAVA_NAILS, // 36 IID_LAVA_NAILS
    ROGUE_IT_PLASMA_AMMO, // 37 IID_PLASMA_AMMO
    ROGUE_IT_MULTI_ROCKETS, // 38 IID_MULTI_ROCKETS
    ROGUE_IT_SHIELD, // 39 IID_SHIELD
    ROGUE_IT_ANTIGRAV, // 40 IID_ANTIGRAV
    ROGUE_IT_EARTHQUAKE, // 41 IID_EARTHQUAKE
    ROGUE_IT_V_SPHERE, // 42 IID_V_SPHERE
    HIP_IT_SWORD // 43 IID_SWORD
};

float vr_itemutil_category[44] =
{
    IID_CATEGORY_INVALID, // 0 IID_NONE
    IID_CATEGORY_ITEMS, // 1 IID_ARMOR1
    IID_CATEGORY_ITEMS, // 2 IID_ARMOR2
    IID_CATEGORY_ITEMS, // 3 IID_ARMOR3
    IID_CATEGORY_ITEMS, // 4 IID_SUPERHEALTH
    IID_CATEGORY_ITEMS, // 5 IID_KEY1
    IID_CATEGORY_ITEMS, // 6 IID_KEY2
    IID_CATEGORY_ITEMS, // 7 IID_INVISIBILITY
    IID_CATEGORY_ITEMS, // 8 IID_INVULNERABILITY
    IID_CATEGORY_ITEMS, // 9 IID_SUIT
    IID_CATEGORY_ITEMS, // 10 IID_QUAD
    IID_CATEGORY_HIPNOTIC_ITEMS, // 11 IID_WETSUIT
    IID_CATEGORY_HIPNOTIC_ITEMS, // 12 IID_EMPATHY_SHIELDS
    IID_CATEGORY_HIPNOTIC_ITEMS, // 13 IID_HORN_OF_CONJURING
    IID_CATEGORY_ITEMS, // 14 IID_FIST
    IID_CATEGORY_ITEMS, // 15 IID_SHOTGUN
    IID_CATEGORY_ITEMS, // 16 IID_SUPER_SHOTGUN
    IID_CATEGORY_ITEMS, // 17 IID_NAILGUN
    IID_CATEGORY_ITEMS, // 18 IID_SUPER_NAILGUN
    IID_CATEGORY_ITEMS, // 19 IID_GRENADE_LAUNCHER
    IID_CATEGORY_ITEMS, // 20 IID_ROCKET_LAUNCHER
    IID_CATEGORY_ITEMS, // 21 IID_LIGHTNING
    IID_CATEGORY_ITEMS, // 22 IID_MJOLNIR
    IID_CATEGORY_ITEMS, // 23 IID_SHELLS
    IID_CATEGORY_ITEMS, // 24 IID_NAILS
    IID_CATEGORY_ITEMS, // 25 IID_ROCKETS
    IID_CATEGORY_ITEMS, // 26 IID_CELLS
    IID_CATEGORY_ITEMS, // 27 IID_AXE
    IID_CATEGORY_HIPNOTIC_ITEMS, // 28 IID_PROXIMITY_GUN
    IID_CATEGORY_ITEMS, // 29 IID_LASER_CANNON
    IID_CATEGORY_ROGUE_ITEMS, // 30 IID_LAVA_NAILGUN
    IID_CATEGORY_ROGUE_ITEMS, // 31 IID_LAVA_SUPER_NAILGUN
    IID_CATEGORY_ROGUE_ITEMS, // 32 IID_MULTI_GRENADE
    IID_CATEGORY_ROGUE_ITEMS, // 33 IID_MULTI_ROCKET
    IID_CATEGORY_ROGUE_ITEMS, // 34 IID_PLASMA_GUN
    IID_CATEGORY_ROGUE_ITEMS, // 35 IID_GRAPPLE
    IID_CATEGORY_ROGUE_ITEMS, // 36 IID_LAVA_NAILS
    IID_CATEGORY_ROGUE_ITEMS, // 37 IID_PLASMA_AMMO
    IID_CATEGORY_ROGUE_ITEMS, // 38 IID_MULTI_ROCKETS
    IID_CATEGORY_ROGUE_ITEMS, // 39 IID_SHIELD
    IID_CATEGORY_ROGUE_ITEMS, // 40 IID_ANTIGRAV
    IID_CATEGORY_ROGUE_ITEMS, // 41 IID_EARTHQUAKE
    IID_CATEGORY_ROGUE_ITEMS, // 42 IID_V_SPHERE
    IID_CATEGORY_HIPNOTIC_ITEMS // 43 IID_SWORD
};

float(float xItemId) VR_ItemUtil_Impl_ItemIdToFlag =
{
    if(xItemId < IID_ARMOR1 || xItemId > IID_SWORD)
    {
        VR_ItemUtil_Impl_PrintInvalidItemId("VR_ItemUtil_Impl_ItemIdToFlag", xItemId);
        return 0;
    }
    return vr_itemutil_flag[xItemId];
}

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float(float xItemId) VR_ItemUtil_ItemIdToItemCategory =
{
    if(xItemId < IID_ARMOR1 || xItemId > IID_SWORD)
    {
        VR_ItemUtil_Impl_PrintInvalidItemId("VR_ItemUtil_ItemIdToItemCategory", xItemId);
        return IID_CATEGORY_INVALID;
    }
    return vr_itemutil_category[xItemId];
}

float(entity xEnt, float xItemId) VR_ItemUtil_EntHasItem =
{
    float itemCategory, itemFlag;
    itemCategory = VR_ItemUtil_ItemIdToItemCategory(xItemId);
    itemFlag = VR_ItemUtil_Impl_ItemIdToFlag(xItemId);


    if(itemCategory == IID_CATEGORY_ITEMS)
    {
        return VRHasBit(xEnt.items, itemFlag);
    }

    if(itemCategory == IID_CATEGORY_HIPNOTIC_ITEMS)
    {
        return VRHasBit(xEnt.hipnotic_items, itemFlag);
    }

    if(itemCategory == IID_CATEGORY_ROGUE_ITEMS)
    {
        return VRHasBit(xEnt.rogue_items, itemFlag);
    }

    VR_ItemUtil_Impl_PrintInvalidItemCategory("VR_ItemUtil_EntHasItem",
                                              xItemId);

    return FALSE;
}

void(entity xEnt, float xItemId, float xOwned) VR_ItemUtil_EntSetItem =
{
    float itemCategory, itemFlag;
    itemCategory = VR_ItemUtil_ItemIdToItemCategory(xItemId);
    itemFlag = VR_ItemUtil_Impl_ItemIdToFlag(xItemId);

    if(itemCategory == IID_CATEGORY_ITEMS)
    {
        xEnt.items = VRSetBit(xEnt.items, itemFlag, xOwned);
        return;
    }

    if(itemCategory == IID_CATEGORY_HIPNOTIC_ITEMS)
    {
        xEnt.hipnotic_items = VRSetBit(xEnt.hipnotic_items, itemFlag, xOwned);
        return;
    }

    if(itemCategory == IID_CATEGORY_ROGUE_ITEMS)
    {
        xEnt.rogue_items = VRSetBit(xEnt.rogue_items, itemFlag, xOwned);
        return;
    }

    VR_ItemUtil_Impl_PrintInvalidItemCategory("VR_ItemUtil_EntSetItem",
                                              xItemId);
}

void(entity xEnt, float xItemId) VR_ItemUtil_EntAddItem =
{
    VR_ItemUtil_EntSetItem(xEnt, xItemId, TRUE);
}

void(entity xEnt, float xItemId) VR_ItemUtil_EntDelItem =
{
    VR_ItemUtil_EntSetItem(xEnt, xItemId, FALSE);
}

void(entity xEnt) VR_ItemUtil_EntClearItems =
{
    xEnt.items = 0;
    xEnt.hipnotic_items = 0;
    xEnt.rogue_items = 0;
}
