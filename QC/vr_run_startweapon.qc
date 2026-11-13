//
//
//
// ----------------------------------------------------------------------------
// Mutators: Starting gun and Starting melee weapon (vr_run_startgun, vr_run_startmelee; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// What a new game starts with (SetNewParms's loadout, changed at its end: VR_Run_NewParms). The gun (the hip holster):
// 0 the shotgun (Quake's), 1 the nail pistol (the mod's, with 50 nails and its magazine full), 2 the heavy revolver
// (loaded from the 25 shells as the shotgun is), 3 nothing. The melee weapon (the other holster): 0 the axe, 1 the
// dagger (the mod's), 2 nothing. Nothing and nothing: bare fists (punching and grabbing as ever). Melee Only
// (vr_run_meleeonly) holds the gun at nothing (the melee weapon stays the player's choice).

float RUN_SG_SHOTGUN    = 0;
float RUN_SG_NAILPISTOL = 1;
float RUN_SG_REVOLVER   = 2;
float RUN_SG_NOTHING    = 3;

float RUN_SM_AXE     = 0;
float RUN_SM_DAGGER  = 1;
float RUN_SM_NOTHING = 2;

float() VR_RunStartGun_Choice =
{
    if(cvar_hget(cvarh_vr_run_meleeonly)) return RUN_SG_NOTHING;
    return cvar_hget(cvarh_vr_run_startgun);
};

float() VR_RunStartMelee_Choice =
{
    return cvar_hget(cvarh_vr_run_startmelee);
};

void() VR_RunStartWeapon_NewParms =
{
    float g = VR_RunStartGun_Choice();
    float m = VR_RunStartMelee_Choice();
    if(g == RUN_SG_SHOTGUN && m == RUN_SM_AXE) return;
    if(g != RUN_SG_SHOTGUN)
    {
        parm4 = parm4 + parm36; // (the shotgun's loaded shells back with the rest)
        parm36 = 0;
    }
    float items = 0;
    if(g == RUN_SG_SHOTGUN) items = items | IT_SHOTGUN;
    if(m == RUN_SM_AXE) items = items | IT_AXE;
    parm1 = items;
    if(g == RUN_SG_NAILPISTOL)
    {
        parm14 = WID_NAILPISTOL;
        parm17 = parm17 | HIP_IT_NAILPISTOL;
        parm5 = max(parm5, 50);
        parm36 = VR_NailPistol_MagSize();
    }
    else if(g == RUN_SG_REVOLVER)
    {
        parm14 = WID_REVOLVER;
        parm17 = parm17 | HIP_IT_REVOLVER;
        if(VR_Reload_ManualFor(parm14) && !VR_FlatReload_NewGameCycles())
        {
            parm36 = min(WeaponIdToBaseClipSize(parm14), parm4);
            parm4 = parm4 - parm36;
        }
    }
    else if(g == RUN_SG_NOTHING)
    {
        parm14 = WID_FIST;
    }
    if(m == RUN_SM_AXE)
    {
        parm13 = WID_AXE;
    }
    else if(m == RUN_SM_DAGGER)
    {
        parm13 = WID_SWORD;
        parm27 = VR_ADMelee_Flags(VR_SWORDKIND_DAGGER);
        parm17 = parm17 | HIP_IT_SWORD;
    }
    else
    {
        parm13 = WID_FIST; // (nothing: bare fists)
    }
};
