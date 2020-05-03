//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

float(float xAid) VR_AmmoUtil_AmmoIdToItemId =
{
    if(xAid == AID_SHELLS)        { return IID_SHELLS; }
    if(xAid == AID_NAILS)         { return IID_NAILS; }
    if(xAid == AID_ROCKETS)       { return IID_ROCKETS; }
    if(xAid == AID_CELLS)         { return IID_CELLS; }
    if(xAid == AID_LAVA_NAILS)    { return IID_LAVA_NAILS; }
    if(xAid == AID_MULTI_ROCKETS) { return IID_MULTI_ROCKETS; }
    if(xAid == AID_PLASMA)        { return IID_PLASMA_AMMO; }

    bprint("`VR_AmmoUtil_AmmoIdToItemId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");

    return 0;
}

float(entity xEnt, float xAid) VR_AmmoUtil_GetAmmoCountByAmmoId =
{
    if(xAid == AID_SHELLS)        { return xEnt.ammo_shells; }
    if(xAid == AID_NAILS)         { return xEnt.ammo_nails; }
    if(xAid == AID_ROCKETS)       { return xEnt.ammo_rockets; }
    if(xAid == AID_CELLS)         { return xEnt.ammo_cells; }
    if(xAid == AID_LAVA_NAILS)    { return xEnt.ammo_lava_nails; }
    if(xAid == AID_MULTI_ROCKETS) { return xEnt.ammo_multi_rockets; }
    if(xAid == AID_PLASMA)        { return xEnt.ammo_plasma; }

    bprint("`VR_AmmoUtil_GetAmmoCountByAmmoId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");

    return 0;
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_SetAmmoCountByAmmoId =
{
    if(xAid == AID_SHELLS)        { xEnt.ammo_shells = xValue; return; }
    if(xAid == AID_NAILS)         { xEnt.ammo_nails = xValue; return; }
    if(xAid == AID_ROCKETS)       { xEnt.ammo_rockets = xValue; return; }
    if(xAid == AID_CELLS)         { xEnt.ammo_cells = xValue; return; }
    if(xAid == AID_LAVA_NAILS)    { xEnt.ammo_lava_nails = xValue; return; }
    if(xAid == AID_MULTI_ROCKETS) { xEnt.ammo_multi_rockets = xValue; return; }
    if(xAid == AID_PLASMA)        { xEnt.ammo_plasma = xValue; return; }

    bprint("`VR_AmmoUtil_SetAmmoCountByAmmoId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_AddAmmoCountByAmmoId =
{
    if(xAid == AID_SHELLS)        { xEnt.ammo_shells = xEnt.ammo_shells + xValue; return; }
    if(xAid == AID_NAILS)         { xEnt.ammo_nails = xEnt.ammo_nails + xValue; return; }
    if(xAid == AID_ROCKETS)       { xEnt.ammo_rockets = xEnt.ammo_rockets + xValue; return; }
    if(xAid == AID_CELLS)         { xEnt.ammo_cells = xEnt.ammo_cells + xValue; return; }
    if(xAid == AID_LAVA_NAILS)    { xEnt.ammo_lava_nails = xEnt.ammo_lava_nails + xValue; return; }
    if(xAid == AID_MULTI_ROCKETS) { xEnt.ammo_multi_rockets = xEnt.ammo_multi_rockets + xValue; return; }
    if(xAid == AID_PLASMA)        { xEnt.ammo_plasma = xEnt.ammo_plasma + xValue; return; }

    bprint("`VR_AmmoUtil_AddAmmoCountByAmmoId`: invalid ammo id '");
    bprint(ftos(xAid));
    bprint("'\n");
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_DelAmmoCountByAmmoId =
{
    VR_AmmoUtil_AddAmmoCountByAmmoId(xEnt, xAid, -xValue);
}
