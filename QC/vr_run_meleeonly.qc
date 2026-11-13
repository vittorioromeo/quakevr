//
//
//
// ----------------------------------------------------------------------------
// Mutator: Melee Only (vr_run_meleeonly; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// No guns: a gun's pickup is left where it lies (weapon_touch: VR_Run_RefusesPickup) and a gun in the hand does not
// fire, only clicks (W_AttackImpl: VR_Run_RefusesFire: one held from a dead armed grunt, a dropped one). A gun is a
// weapon that uses ammunition; Mjolnir (a hammer without its cells) and the chainsaw (its own tank) stay. Ammunition is
// still picked up (it does nothing). The run starts with the axe (Starting Weapon held at it: vr_run_startweapon.qc).

float(float wid) VR_RunMeleeOnly_IsGun =
{
    if(wid == WID_MJOLNIR || wid == WID_CHAINSAW) return FALSE;
    return (WeaponIdToAmmoId(wid, 0) != AID_NONE);
};

float(float wid) VR_RunMeleeOnly_Refuses =
{
    return cvar_hget(cvarh_vr_run_meleeonly) && VR_RunMeleeOnly_IsGun(wid);
};
