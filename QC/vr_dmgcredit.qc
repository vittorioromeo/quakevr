// vr_dmgcredit.qc -- the weapon credited for a blow (Honey's FL_SPECIFICDAMAGE: a target with it takes damage only from
// the weapons in its .items, as IT_* bits; buttons.qc sets it where a map gives a button .items).
//
// Whatever deals the damage carries the credit in its .damage_weapon (an IT_* bit; 0: no weapon), stamped once, where
// it is made: a player's projectile, beam or thrown weapon from the weapon in the hand that made it (VR_ShotLaunched,
// FireBullets, MakeThrown...), what one makes (an explosion, a cluster's bomblets, a chain of lightning, a poison) from
// it, and a player himself for his hitscan shots and melee blows (he is their inflictor). T_Damage reads the credit of
// its inflictor only (VR_DamageCredit_Accepts), never the attacker's last weapon. A monster's own shots and anything
// else unstamped credit nothing (a grunt's: Honey's IT_SHOTGUN on the grunt, orig_mon_soldier.qc).

.float damage_weapon; // the IT_* bit of the weapon what deals this entity's damage is credited with (0: none)

// The credit of a weapon id (WID_*): its IT_* bit, the variants (lava nails, multi grenades and rockets, the plasma
// guns, the AD soldiers' guns) as the weapon they stand in for. 0: no weapon to credit (fist, grapple, toolgun, AD's
// flamethrowers).
float(float wid) WeaponIdToDamageCredit =
{
    if(wid == WID_SHOTGUN || wid == WID_GRUNTGUN)                                    { return IT_SHOTGUN; }
    if(wid == WID_SUPER_SHOTGUN || wid == WID_ADDEFENDER)                            { return IT_SUPER_SHOTGUN; }
    if(wid == WID_NAILGUN || wid == WID_NAILPISTOL)                                  { return IT_NAILGUN; }
    if(wid == WID_SUPER_NAILGUN)                                                     { return IT_SUPER_NAILGUN; }
    if(wid == WID_GRENADE_LAUNCHER || wid == WID_PROXIMITY_GUN || wid == WID_ADGRENADE) { return IT_GRENADE_LAUNCHER; }
    if(wid == WID_ROCKET_LAUNCHER || wid == WID_ADROCKET)                            { return IT_ROCKET_LAUNCHER; }
    if(wid == WID_LIGHTNING || wid == WID_ADPLASMA || wid == WID_ADELIMINATOR || wid == WID_ADCENTURION)
                                                                                     { return IT_LIGHTNING; }
    if(wid == WID_LASER_CANNON || wid == WID_ENFRIFLE)                               { return IT_LASER_CANNON; }
    if(wid == WID_MJOLNIR)                                                           { return IT_MJOLNIR; }
    if(wid == WID_AXE || wid == WID_SWORD || wid == WID_CHAINSAW || wid == WID_CROWBAR || wid == WID_SUPERAXE)
                                                                                     { return IT_AXE; }
    return 0;
};

// `xDealer` credited to weapon id `wid`.
void(entity xDealer, float wid) VR_DamageCredit_SetWeapon =
{
    xDealer.damage_weapon = WeaponIdToDamageCredit(wid);
};

// `xDealer` (a projectile, a beam, or the shooter himself for a hitscan shot) credited to the weapon in `xShooter`'s
// hand `xHand`.
void(entity xDealer, entity xShooter, float xHand) VR_DamageCredit_FromHand =
{
    VR_DamageCredit_SetWeapon(xDealer, VRGetEntWeapon(xShooter, xHand));
};

// `xDealer` made by `xFrom` (an explosion or bomblet by its missile, a link of a lightning chain by the last): its credit.
void(entity xDealer, entity xFrom) VR_DamageCredit_Inherit =
{
    xDealer.damage_weapon = xFrom.damage_weapon;
};

// Whether `xTarget` takes damage dealt by `xInflictor`: always, unless it takes only some weapons' (FL_SPECIFICDAMAGE)
// and `xInflictor`'s credit is none of them.
float(entity xTarget, entity xInflictor) VR_DamageCredit_Accepts =
{
    if(!(xTarget.flags & FL_SPECIFICDAMAGE))
    {
        return TRUE;
    }
    return (xInflictor.damage_weapon & xTarget.items) != 0;
};
