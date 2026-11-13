//
//
//
// ----------------------------------------------------------------------------
// Mutator: Melee Only (vr_run_meleeonly; docs/vr-port/MUTATORS.md)
// ----------------------------------------------------------------------------
//
// No guns, and nothing to feed one (his rule, 2026-11-13: he force-grabbed a grunt's burst rifle and held it). A gun is
// a weapon that uses ammunition; Mjolnir (a hammer without its cells) and the chainsaw (its own tank) stay, as do the
// axe, swords, daggers, flails. A gun lying about (a map's pickup, AD's, one a monster or a grunt dropped, one thrown, a
// crate's flare gun, a func_weapon_grabbable's) and ammunition (a box, AD's, the horde's, a backpack with rounds in it)
// stay where they lie, props you can't use: no force grab (VR_Forcegrab_IsEligible, VR or flat), no hand grab
// (wpnthrow_handtouch_ent_hand, VR_Carry_Handtouch, VR_PickupObj_Handtouch), no walk-over or holster take (weapon_touch,
// ammo_touch, BackpackTouch...), no grab sparkles. A hand still disarms an armed grunt (its gun falls at your hand). One
// held anyway (a cheat, an old save) does not fire, only clicks (W_AttackImpl: VR_Run_RefusesFire). The run starts with
// the axe (Starting Weapon held at it: vr_run_startweapon.qc).

// (The enemies' single-use guns -- the grunt's burst rifle, the enforcer's laser rifle, AD's soldiers' -- and the flare
// gun use no reserve ammunition, their clip alone: guns all the same.)
float(float wid) VR_RunMeleeOnly_IsGun =
{
    if(wid == WID_MJOLNIR || wid == WID_CHAINSAW) return FALSE;
    return (WeaponIdToAmmoId(wid, 0) != AID_NONE) || VR_IsEnemyGun(wid) || wid == WID_FLAREGUN;
};

// The rule: weapon `wid` is refused (held, taken, grabbed) while Melee Only is on.
float(float wid) VR_RunMeleeOnly_Refuses =
{
    return cvar_hget(cvarh_vr_run_meleeonly) && VR_RunMeleeOnly_IsGun(wid);
};

// The same rule for a thing lying about, `e`: a gun (its weapon refused), ammunition or a backpack with rounds in it.
float(entity e) VR_RunMeleeOnly_RefusesEnt =
{
    if(!cvar_hget(cvarh_vr_run_meleeonly) || e == world)
    {
        return FALSE;
    }
    // (A map's pickup: its touch weapon_touch, placed its th_die (PlaceItem: itemTouch's), an object or not; its
    // .weapon impl_weapon_item's, AD's too.)
    if(e.classname == "thrown_weapon" || e.classname == "vr_ag_weapon" || e.handtouch == weapon_touch ||
       e.th_die == weapon_touch)
    {
        return VR_RunMeleeOnly_Refuses(e.weapon);
    }
    if(e.handtouch == BackpackTouch || e.carry_use == BackpackTouch || e.ad_touch2 == ad_BackpackTouch)
    {
        return Backpack_HasAmmo(e);
    }
    return VR_Reload_IsAmmoBox(e) || e.handtouch == ammo_touch || e.th_die == ammo_touch || e.ad_touch2 == ad_ammo_touch;
};
