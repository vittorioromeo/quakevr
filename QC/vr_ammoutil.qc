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

    ASSERT(FALSE);
    return 0;
}

float(entity xEnt, float xAid) VR_AmmoUtil_GetAmmoCountByAmmoId =
{
    if(!VR_Pack_AmmoAvailable(xAid)) { return 0; }

    if(xAid == AID_SHELLS)        { return xEnt.ammo_shells; }
    if(xAid == AID_NAILS)         { return xEnt.ammo_nails; }
    if(xAid == AID_ROCKETS)       { return xEnt.ammo_rockets; }
    if(xAid == AID_CELLS)         { return xEnt.ammo_cells; }
    if(xAid == AID_LAVA_NAILS)    { return xEnt.ammo_lava_nails; }
    if(xAid == AID_MULTI_ROCKETS) { return xEnt.ammo_multi_rockets; }
    if(xAid == AID_PLASMA)        { return xEnt.ammo_plasma; }

    ASSERT(FALSE);
    return 0;
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_SetAmmoCountByAmmoId =
{
    if(!VR_Pack_AmmoAvailable(xAid)) { return; }

    if(xAid == AID_SHELLS)        { xEnt.ammo_shells = xValue; return; }
    if(xAid == AID_NAILS)         { xEnt.ammo_nails = xValue; return; }
    if(xAid == AID_ROCKETS)       { xEnt.ammo_rockets = xValue; return; }
    if(xAid == AID_CELLS)         { xEnt.ammo_cells = xValue; return; }
    if(xAid == AID_LAVA_NAILS)    { xEnt.ammo_lava_nails = xValue; return; }
    if(xAid == AID_MULTI_ROCKETS) { xEnt.ammo_multi_rockets = xValue; return; }
    if(xAid == AID_PLASMA)        { xEnt.ammo_plasma = xValue; return; }

    ASSERT(FALSE);
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_AddAmmoCountByAmmoId =
{
    if(!VR_Pack_AmmoAvailable(xAid)) { return; }

    if(xAid == AID_SHELLS)        { xEnt.ammo_shells = xEnt.ammo_shells + xValue; return; }
    if(xAid == AID_NAILS)         { xEnt.ammo_nails = xEnt.ammo_nails + xValue; return; }
    if(xAid == AID_ROCKETS)       { xEnt.ammo_rockets = xEnt.ammo_rockets + xValue; return; }
    if(xAid == AID_CELLS)         { xEnt.ammo_cells = xEnt.ammo_cells + xValue; return; }
    if(xAid == AID_LAVA_NAILS)    { xEnt.ammo_lava_nails = xEnt.ammo_lava_nails + xValue; return; }
    if(xAid == AID_MULTI_ROCKETS) { xEnt.ammo_multi_rockets = xEnt.ammo_multi_rockets + xValue; return; }
    if(xAid == AID_PLASMA)        { xEnt.ammo_plasma = xEnt.ammo_plasma + xValue; return; }

    ASSERT(FALSE);
}

string(float xAid) VR_AmmoUtil_AmmoIdToItemName =
{
    if(xAid == AID_SHELLS)        { return ITEM_NAME_SHELLS; }
    if(xAid == AID_NAILS)         { return ITEM_NAME_SPIKES; }
    if(xAid == AID_ROCKETS)       { return ITEM_NAME_ROCKETS; }
    if(xAid == AID_CELLS)         { return ITEM_NAME_CELLS; }
    if(xAid == AID_LAVA_NAILS)    { return ITEM_NAME_LAVA_SPIKES; }
    if(xAid == AID_MULTI_ROCKETS) { return ITEM_NAME_MULTI_ROCKETS; }
    if(xAid == AID_PLASMA)        { return ITEM_NAME_PLASMA; }

    ASSERT(FALSE);
    return NULLSTR;
}

string(float xAid) VR_AmmoUtil_AmmoIdToNetname =
{
    if(xAid == AID_SHELLS)        { return "shells"; }
    if(xAid == AID_NAILS)         { return "nails"; }
    if(xAid == AID_ROCKETS)       { return "rockets"; }
    if(xAid == AID_CELLS)         { return "cells"; }
    if(xAid == AID_LAVA_NAILS)    { return "lava nails"; }
    if(xAid == AID_MULTI_ROCKETS) { return "multi rockets"; }
    if(xAid == AID_PLASMA)        { return "plasma"; }

    ASSERT(FALSE);
    return NULLSTR;
}

void(entity xEnt, float xAid, float xValue) VR_AmmoUtil_DelAmmoCountByAmmoId =
{
    VR_AmmoUtil_AddAmmoCountByAmmoId(xEnt, xAid, -xValue);

    if(VR_AmmoUtil_GetAmmoCountByAmmoId(xEnt, xAid) < 0)
    {
        bprint("ERROR: ammo count less than zero!\n");
    }
}

// ----------------------------------------------------------------------------
// Capacities: how much of each ammunition and health a player can carry (MG3_PLAN.md 2.2). Every cap goes through
// these: Quake's own numbers everywhere but Dawn of the Machine, whose bases are lower and grow with its upgrades
// (vr_mg3_defs.qc). The player is the argument for when a campaign's capacities are each player's own.
// ----------------------------------------------------------------------------

float(entity xEnt, float xAid) VR_MaxAmmo =
{
    if(xAid >= AID_SHELLS && xAid <= AID_CELLS && MG3_Campaign()) { return MG3_Capacity(xAid); }
    if(xAid == AID_NAILS || xAid == AID_LAVA_NAILS) { return 200; }
    return 100;
}

// A new player's health (SetNewParms): Dimension of the Past and Dimension of the Machine's Nightmare start with 50,
// Dawn of the Machine's single player always does (before its upgrades).
float() VR_BaseHealth =
{
    if(deathmatch) { return 100; }
    if(MG3_Campaign()) { return MG3_BASE_HEALTH; }
    return ((cvar("vr_campaign") == 3 || cvar("vr_campaign") == 4) && skill == 3) ? 50 : 100;
}

// The most health ordinary healing gives (.max_health), and what a level change carries at most.
float(entity xEnt) VR_MaxHealth =
{
    if(MG3_Campaign()) { return MG3_Capacity(MG3_UPGRADE_HEALTH); }
    return VR_BaseHealth();
}

// The most health anything gives, megahealth included (Dawn of the Machine raised it).
float(entity xEnt) VR_MegaHealthCap =
{
    return MG3_Campaign() ? 500 : 250;
}
