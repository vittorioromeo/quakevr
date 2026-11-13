//
//
//
// ----------------------------------------------------------------------------
// Mutator: Starting Weapon (vr_run_startweapon; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// What a new game starts with (SetNewParms's loadout, changed at its end: VR_Run_NewParms): 0 the shotgun and axe
// (Quake's), 1 the heavy revolver and axe (the mod's pistol: vr_revolver.qc, in the right hip holster, loaded from the
// 25 shells as the shotgun is), 2 the axe alone, 3 bare fists. Melee Only (vr_run_meleeonly) holds it at the axe.

float RUN_SW_SHOTGUN  = 0;
float RUN_SW_REVOLVER = 1;
float RUN_SW_AXE      = 2;
float RUN_SW_UNARMED  = 3;

float() VR_RunStartWeapon_Choice =
{
    if(cvar_hget(cvarh_vr_run_meleeonly)) return RUN_SW_AXE;
    return cvar_hget(cvarh_vr_run_startweapon);
};

void() VR_RunStartWeapon_NewParms =
{
    float w = VR_RunStartWeapon_Choice();
    if(w == RUN_SW_SHOTGUN) return;
    parm4 = parm4 + parm36; // (the shotgun's loaded shells back with the rest)
    parm36 = 0;
    if(w == RUN_SW_REVOLVER)
    {
        parm1 = IT_AXE;
        parm17 = parm17 | HIP_IT_REVOLVER;
        parm14 = WID_REVOLVER;
        if(VR_Reload_ManualFor(parm14) && !VR_FlatReload_NewGameCycles())
        {
            parm36 = min(WeaponIdToBaseClipSize(parm14), parm4);
            parm4 = parm4 - parm36;
        }
        return;
    }
    parm14 = WID_FIST;
    if(w == RUN_SW_AXE)
    {
        parm1 = IT_AXE;
        return;
    }
    parm1 = 0; // (unarmed: the fists)
    parm13 = WID_FIST;
};
