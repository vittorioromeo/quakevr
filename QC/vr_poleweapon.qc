// vr_poleweapon.qc -- pole weapons' shared game code (docs/vr-port/POLEWEAPONS.md; ROUND25.md "spear"): Dwell's Crystal
// Lance (the lightning gun's record with its variant, vr_dw_lance_pole) and Quake VR's spear (the knight's sword of its
// kind, VR_SWORDKIND_SPEAR). Both are a pole held as a spear (their weapons.txt slots: Other Hand Anywhere 3, the engine's
// pole grip, flat play's spear pose and thrust, the flip grip) and strike with one blow (VR_PoleMelee): what differs is
// data (the head's place on the line: the slot's Blade Start, else the Lance's vr_dw_lance_head) and what each is besides
// (the Lance a gun: its charge, bolt, cord and cells, dw_items.qc; the spear none: a sword's no ammo, throw and embed).

float(float xHand) DW_LanceHeld; // (dwell/dw_items.qc)

// The weapon in `xHand` is the spear.
float(float xHand) VR_SpearHeld =
{
    return VRGetEntWeapon(self, xHand) == WID_SWORD && VR_ADMelee_Kind(VRGetEntWeaponFlags(self, xHand)) == VR_SWORDKIND_SPEAR;
};

// Weapon `xWid` (its flags `xFlags`) is a pole weapon by its data: its weapons.txt slot's Other Hand Anywhere 3 (the
// engine's twohand::poleWeapon). Armed humanoids hold one in both hands and thrust it (vr_armedgrunt_pose.qc VR_AGP_Melee).
float(float xWid, float xFlags) VR_PoleWeapon =
{
    return weaponvalue(WeaponIdToModel(xWid, xFlags), "anygrip_mode") == 3;
};

// The weapon in `xHand` is a pole weapon whose blow is VR_PoleMelee (client.qc's melee dispatch).
float(float xHand) VR_PoleHeld =
{
    return VR_SpearHeld(xHand) || DW_LanceHeld(xHand);
};

// Where its head begins on the line from the hand to the tip (a share of it): its slot's Blade Start (bl_from: the spear's
// head on its socket), else the Lance's vr_dw_lance_head (its crystal head; its slot has none).
float(float xHand) VR_PoleHeadFrom =
{
    if(VR_SpearHeld(xHand))
    {
        float from = weaponvalue(WeaponIdToModel(WID_SWORD, VRGetEntWeaponFlags(self, xHand)), "bl_from");
        if(from > 0) { return from; }
    }
    return cvar_hget(cvarh_vr_dw_lance_head);
};

// A pole weapon's blow (VR_Melee_Deal; the melee sweep along its blow line to its head, its slot's bl_*): the axe's
// damage (vr_dmg_axe, before the swing's speed and weight: VRMeleeDmg), a blade's sounds, blood and wall chop
// (W_FireAxe's), where W_GunMelee clubs.
// A thrust (dwellweap2; docs/vr-port/POLEWEAPONS.md "Thrust"): the blow read as a stab (VR_Melee_Decide: driven along
// its line, far end first) lands with the head alone (the point that struck at or past VR_PoleHeadFrom of the line from
// the hand to the tip): its damage times vr_dw_lance_thrust (the stab's damage already grows with the speed along the
// line, VRMeleeDmg); a stab on the shaft short of the head is a poke, vr_dw_lance_poke of it. (The two multipliers are
// every pole weapon's: tuning the Lance's thrust tunes the spear's.)
// The shaft (spearblunt; POLEWEAPONS.md "The shaft"): a blow landing with it, short of the head, or with the butt behind the
// hand (VR_Melee_PoleLayout's blunt points: the blow's pommel kind) is a staff's: blunt (VR_Melee_Deal's vr_melee_blunt:
// the hit's class, wounds, knockdowns, the knock's sound, a round dent; no stab, no embed), a gun's club damage
// (vr_dmg_gun_bash) and its blunt layer; its leg sweeps are the engine's (vr_legsweep.cpp: the head a cut, the shaft
// sweeps).
void(float xHand) VR_PoleMelee =
{
    entity b = VR_Blow(xHand);
    float shaft = b.mh_kind == VR_MKIND_POMMEL;
    float xdmg = VRMeleeDmg(xHand, cvar_hget(shaft ? cvarh_vr_dmg_gun_bash : cvarh_vr_dmg_axe));
    if(shaft)
    {
        dprint(sprintf("%s: %s, a blunt blow (%.2f of the line, %.1f m/s): %.0f damage\n", VR_SpearHeld(xHand) ? "spear" : "dwlance",
            b.mh_part, b.mh_c_f, b.mh_speed, xdmg));
    }
    if(b.mh_kind == VR_MKIND_STAB)
    {
        float head = (b.mh_c_f >= VR_PoleHeadFrom(xHand));
        xdmg = xdmg * (head ? cvar_hget(cvarh_vr_dw_lance_thrust) : cvar_hget(cvarh_vr_dw_lance_poke));
        dprint(sprintf("%s: a thrust, %s (%.2f of the line, %.1f m/s): %.0f damage\n", VR_SpearHeld(xHand) ? "spear" : "dwlance",
            head ? "the head" : "the shaft, a poke", b.mh_c_f, b.mh_speed, xdmg));
    }
    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 13)) { return; }
    vector org = trace_endpos - v_forward * 4;
    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg)) { VR_HitBloodSplash(victim, org, xdmg); }
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "fisthit.wav", !shaft), 1.0, ATTN_NORM);
            if(shaft)
            {
                VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), victim); // (a gun's butt's, a club's: of what it struck)
            }
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }
    VR_Melee_WallMark(xHand, xdmg, shaft); // its head's chop, its shaft's round dent (vr_decal_melee)
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    BroadcastGunshotEffect(org);
};
