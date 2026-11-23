// dw_items.qc -- Dwell support (DWELL.md P2a): Dwell's powerups (Haste, Berserk, Wetsuit), its weapons as placeholders
// on our player (P3 makes them VR weapons), and its world entities (misc_crystal, func_fade, info_storycamera).
//
// Copyright (C) the Dwell team (2023: Fairweather, Khreathor, bmFbr, Shamblernaut, VoidForce and the others), whose
// entities these reimplement on Copper (Lunaran, GPL-2) and Quake VR; hand-written from Dwell's behaviour, not its code.
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public
// License version 2 or later. Dwell's models and sounds are read from the player's install, never shipped.

// The unit vector at yaw `deg` (degrees) on the horizontal plane.
vector(float deg) DW_Yaw =
{
    return '1 0 0' * cos(deg * 0.0174533) + '0 1 0' * sin(deg * 0.0174533);
};

// ---------------------------------------------------------------------------------------------------------------------
// Powerups: Haste (20 s: weapons refire in 0.6 of their time, RuneApplyHell), Berserk (30 s: melee blows 11 times as
// hard, half the damage taken: DW_DamageScale), Wetsuit (60 s: breathing and 1.3 times the speed under water).

const float DW_PW_HASTE = 1;
const float DW_PW_BERSERK = 2;
const float DW_PW_WETSUIT = 4;

void() dw_powerup_touch =
{
    if(other.classname != "player" || other.health <= 0) { return; }
    item_taken = TRUE; // (itemTouch: its targets fire)
    VRItemPickupHaptic(other);
    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other, "\n");
    sound(other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    if(self.cnt == DW_PW_HASTE) { other.dw_haste_finished = time + 20; }
    else if(self.cnt == DW_PW_BERSERK)
    {
        other.dw_berserk_finished = time + 30;
        sprint(other, "You begin frothing with a berserker's rage...\n");
    }
    else { other.dw_wetsuit_finished = time + 60; }
    other.dw_warned = other.dw_warned - (other.dw_warned & self.cnt);
    self.solid = SOLID_NOT;
    self.model = string_null;
    if(deathmatch)
    {
        self.mdl = self.model;
        self.nextthink = time + 60;
        self.think = SUB_regen;
    }
    activator = other;
    SUB_UseTargets();
};

void(string mdl, string nm, string snd, float kind) dw_powerup =
{
    if(!DW_Data()) { remove(self); return; }
    precache_model(mdl);
    precache_sound(snd);
    self.noise = snd;
    self.netname = nm;
    self.cnt = kind;
    self.handtouch = dw_powerup_touch;
    setmodel(self, mdl);
    setsize(self, '-16 -16 -24', '16 16 32');
    StartItem();
};

/*dw:QUAKED item_artifact_haste (0 .5 .8) (-16 -16 -24) (16 16 32)
Dwell's Haste: 20 s, weapons refire in 0.6 of their time.
*/
void() dw_item_artifact_haste = { dw_powerup("progs/e2/item_haste.mdl", "Haste", "items/haste1.wav", DW_PW_HASTE); };
/*dw:QUAKED item_artifact_berserk (0 .5 .8) (-16 -16 -24) (16 16 32)
Dwell's Berserk: 30 s, melee 11 times as hard, half the damage taken.
*/
void() dw_item_artifact_berserk = { dw_powerup("progs/item_berserk.mdl", "Berserk", "items/axe_powerup.wav", DW_PW_BERSERK); };
void() dw_item_artifact_wetsuit = { dw_powerup("progs/item_diving.mdl", "Wetsuit", "items/wetsuit.wav", DW_PW_WETSUIT); };

// CheckPowerups: their ends (said 3 s before), the glow while Haste or Berserk lasts, the Wetsuit's breath.
void(float f, float bit, string what) dw_powerup_end =
{
    if(!f) { return; }
    if(f < time + 3 && f > time && !(self.dw_warned & bit))
    {
        self.dw_warned = self.dw_warned | bit;
        sprint(self, what);
        sprint(self, " is wearing off\n");
    }
};
// The screen tint, Dwell's v_cshift colours (its client.qc: both orange, Berserk red, Haste yellow; the Wetsuit none)
// through AD's tint (VR_AD_Tint: a stat the engine eases in, times VR Settings' Screen Tints), never over AD's own.
void() dw_powerup_tint =
{
    float h = self.dw_haste_finished > time, b = self.dw_berserk_finished > time;
    if(h || b)
    {
        if(self.vr_ad_tintden && !self.vr_dw_tinted) { return; } // (an AD debuff's own: kept)
        if(h && b)  { VR_AD_Tint(self, '255 128 0', 20, FALSE); }
        else if(b)  { VR_AD_Tint(self, '255 0 0', 20, FALSE); }
        else        { VR_AD_Tint(self, '255 255 0', 10, FALSE); }
        self.vr_dw_tinted = TRUE;
    }
    else if(self.vr_dw_tinted)
    {
        VR_AD_Tint(self, '0 0 0', 0, FALSE);
        self.vr_dw_tinted = FALSE;
    }
};

void() DW_PowerupFrame =
{
    if(!self.dw_haste_finished && !self.dw_berserk_finished && !self.dw_wetsuit_finished)
    {
        self.vr_dw_hud = 0;
        if(self.vr_dw_tinted) { dw_powerup_tint(); }
        return;
    }
    dw_powerup_end(self.dw_haste_finished, DW_PW_HASTE, "Haste");
    dw_powerup_end(self.dw_berserk_finished, DW_PW_BERSERK, "Berserk");
    dw_powerup_end(self.dw_wetsuit_finished, DW_PW_WETSUIT, "The Wetsuit");
    if(self.dw_haste_finished < time) { self.dw_haste_finished = 0; }
    if(self.dw_berserk_finished < time) { self.dw_berserk_finished = 0; }
    if(self.dw_wetsuit_finished < time) { self.dw_wetsuit_finished = 0; }
    dw_powerup_tint();
    // The wrist gadget's row (and the flat visor's): each one's seconds left, 8 bits each (vr_gadget.cpp infoRows).
    self.vr_dw_hud = (self.dw_haste_finished ? min(255, ceil(self.dw_haste_finished - time)) : 0) +
                     (self.dw_berserk_finished ? min(255, ceil(self.dw_berserk_finished - time)) : 0) * 256 +
                     (self.dw_wetsuit_finished ? min(255, ceil(self.dw_wetsuit_finished - time)) : 0) * 65536;
    if(self.dw_haste_finished || self.dw_berserk_finished) { self.effects = self.effects | EF_DIMLIGHT; }
    else if(!(self.items & IT_QUAD) && !(self.items & IT_INVULNERABILITY)) { self.effects = self.effects - (self.effects & EF_DIMLIGHT); }
    if(self.dw_wetsuit_finished && self.waterlevel >= 3)
    {
        self.air_finished = time + 12;
        if(!(self.flags & FL_ONGROUND)) { self.velocity = self.velocity * 1.03; } // (a gentle push: 1.3 at its cap)
        if(vlen(self.velocity) > 1.3 * cvar("sv_maxspeed")) { self.velocity = normalize(self.velocity) * 1.3 * cvar("sv_maxspeed"); }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Weapons (placeholders, DWELL.md P3): the pickup gives one of our weapons with Dwell's variant on its instance; the
// variant fires with Dwell's numbers. Rotary Shotgun: a super shotgun firing 20 pellets of 2 ('0.1 0.02' spread), a
// shell a shot, 5 a second. Crystal Lance: a lightning gun firing a piercing crystal beam, 150 to each body it passes,
// 6 cells, every 1.4 s. (Haste: 0.6 of each.) Dwell's Rifle (weapon_rifle) is not made: no Dwell map places it.

void(string mdl, float wid, float xVar, string nm) dw_weapon =
{
    if(!DW_Data()) { remove(self); return; }
    if(!VR_Pack_WeaponAvailable(wid)) { remove(self); return; }
    impl_weapon_item("progs/g_shot.mdl", wid);
    precache_model(mdl);
    setmodel(self, mdl);
    setsize(self, '-8 -8 -4', '8 8 38');
    self.vr_dw_var = xVar;
    self.netname = nm;
};

/*dw:QUAKED weapon_quad_shotgun (0 .5 .8) (-16 -16 0) (16 16 32)
Dwell's Rotary Shotgun (for now a super shotgun firing Dwell's 20 pellets).
*/
void() dw_weapon_quad_shotgun = { dw_weapon("progs/g_shot3.mdl", WID_SUPER_SHOTGUN, DW_VAR_ROTARY, "Rotary Shotgun"); };
/*dw:QUAKED weapon_railgun (0 .5 .8) (-16 -16 0) (16 16 32)
Dwell's Crystal Lance (for now a lightning gun firing Dwell's piercing beam: 150 each, 6 cells).
*/
void() dw_weapon_railgun = { dw_weapon("progs/g_rail.mdl", WID_LIGHTNING, DW_VAR_LANCE, "Crystal Lance"); };

// The toolgun's Dwell weapons: the weapon laid as a prop (func_weapon_grabbable: a rigid body like every other weapon
// there) with its variant on its record, not the floating pickup (his note 2026-11-21; AD_UpgradeProp, the same).
void(float xWid, float xVar) DW_WeaponProp =
{
    if(!DW_Data() || !VR_Pack_WeaponAvailable(xWid)) { remove(self); return; }
    self.classname = "func_weapon_grabbable";
    self.weapon = xWid;
    vr_grab_dwvar = xVar;
    func_weapon_grabbable();
    vr_grab_dwvar = 0;
};

// (the toolgun's entries: Quake VR/vr_toolgun_packs.inc, by these names)
void() toolgun_dw_rotary = { DW_WeaponProp(WID_SUPER_SHOTGUN, DW_VAR_ROTARY); };
void() toolgun_dw_lance = { DW_WeaponProp(WID_LIGHTNING, DW_VAR_LANCE); };

// weapon_touch: the pickup's variant on the weapon it gave (none: a plain one; one or the other: no AD upgrade on it).
void(entity xPickup, entity p, float xHand) DW_VariantGiven =
{
    entity inst = VRGetEntWeaponInst(p, xHand);
    if(inst == world) { return; }
    inst.wi_dwvar = xPickup.vr_dw_var;
    if(inst.wi_dwvar) { inst.wi_adup = 0; }
    // The Rotary Shotgun comes with its tube full, as the Widowmaker it handles as (AD_UpgradeGiven; AD_Widowmaker).
    if(inst.wi_dwvar == DW_VAR_ROTARY && p.ishuman && VRIsWeaponReloadingEnabledForInst(p, WID_SUPER_SHOTGUN, inst))
    {
        VRSetEntWeaponClip(p, xHand, VR_ClipSizeOf(WID_SUPER_SHOTGUN, inst));
    }
};

// A variant's name (an armed grunt's prop, the log).
string(float xVar) DW_VariantName =
{
    if(xVar == DW_VAR_ROTARY) { return "Rotary Shotgun"; }
    if(xVar == DW_VAR_LANCE) { return "Crystal Lance"; }
    return "";
};

// Dwell's weapons drawn as themselves (DWELL.md 9.6): their VR models, made from Dwell's own view models as they load
// (engine vr_adweapons.cpp: Dwell's v_shot3.mdl, v_rail.mdl read in place from its folder, laid as the weapon they are
// a variant of; weapons.txt slots 52-53), never written anywhere. Without Dwell's data: ours.
float() DW_LancePole;
float() DW_RotaryGauntlet;
string(float xWid) DW_VRModelOf =
{
    if(xWid == WID_SUPER_SHOTGUN) { return DW_RotaryGauntlet() ? "progs/dw_rotary.mdl#vr" : "progs/dw_shot3.mdl#vr"; }
    if(xWid == WID_LIGHTNING) { return DW_LancePole() ? "progs/dw_lance.mdl#vr" : "progs/dw_rail.mdl#vr"; }
    return "";
};

// The Crystal Lance held as a lance (dwellwpn2; vr_dw_lance_pole, 1 by default): its model made from Dwell's world model
// (a staff with the crystal head, the lightning gun's grip under its pole; slot 55), the other hand anywhere on its pole
// (the engine's pole grip), its crystal head's blow a blade's (DW_LanceMelee); 0: the pistol from Dwell's view model.
float() DW_LancePole = { return cvar_hget(cvarh_vr_dw_lance_pole) != 0; };

// The Rotary Shotgun held as a gauntlet (dwellwpn3; vr_dw_rotary_gauntlet, 1 by default): its model laid round the super
// nailgun's hand (slot 56: the hand inside its housing), its shell port on the housing's inner side (the engine's load
// point: the Widowmaker's tube, a handful a touch); 0: the super shotgun's grip under it (slot 52).
float() DW_RotaryGauntlet = { return cvar_hget(cvarh_vr_dw_rotary_gauntlet) != 0; };

// The Crystal Lance's blow held as a lance (VR_Melee_Deal; the melee sweep along its blow line to its crystal head, slot
// 55's bl_*): the axe's damage (vr_dmg_axe, before the swing's speed and weight: VRMeleeDmg), a blade's sounds, blood
// and wall chop (W_FireAxe's), where W_GunMelee clubs.
// A thrust (dwellweap2; docs/vr-port/POLEWEAPONS.md "Thrust"): the blow read as a stab (VR_Melee_Decide: driven along
// its line, far end first) lands with the crystal head alone (the point that struck at or past vr_dw_lance_head of the
// line from the hand to the tip): its damage times vr_dw_lance_thrust (the stab's damage already grows with the speed
// along the line, VRMeleeDmg); a stab on the shaft short of the head is a poke, vr_dw_lance_poke of it.
void(float xHand) DW_LanceMelee =
{
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_axe));
    entity b = VR_Blow(xHand);
    if(b.mh_kind == VR_MKIND_STAB)
    {
        float head = (b.mh_c_f >= cvar_hget(cvarh_vr_dw_lance_head));
        xdmg = xdmg * (head ? cvar_hget(cvarh_vr_dw_lance_thrust) : cvar_hget(cvarh_vr_dw_lance_poke));
        dprint(sprintf("dwlance: a thrust, %s (%.2f of the line, %.1f m/s): %.0f damage\n",
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
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "fisthit.wav", TRUE), 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }
    VR_Melee_WallMark(xHand, xdmg, FALSE); // its head's chop (vr_decal_melee)
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    BroadcastGunshotEffect(org);
};

// The weapon record `xInst` is the Crystal Lance held as a lance: no magazine (dwellweap; his note
// vrtesthall_2026-11-22_15-38-38: "it shouldn't have a magazine like the laser cannon"): every shot's 6 cells taken from
// the pouch's (VRIsWeaponReloadingEnabledForInst never reloads it; its eject button free for the grip's flip, vr_revgrip.qc;
// the cord from its butt to the pouch planned in docs/vr-port/POLEWEAPONS.md).
float(entity xInst) DW_LanceFedFromPouch =
{
    return xInst.wi_dwvar == DW_VAR_LANCE && DW_LancePole();
};

// The weapon in `xHand` is the Crystal Lance held as a lance.
float(float xHand) DW_LanceHeld =
{
    if(!DW_LancePole() || VRGetEntWeapon(self, xHand) != WID_LIGHTNING) { return FALSE; }
    entity inst = VRGetEntWeaponInst(self, xHand);
    return inst != world && inst.wi_dwvar == DW_VAR_LANCE;
};

nosave float dw_vrmodels_made; // the models precached this server (bits 1 2 4 8 16; a loaded game precaches them again)

// The model a weapon of kind `xWid` with record `xInst` is drawn with when it is Dwell's ("": not Dwell's, or no data).
string(float xWid, entity xInst) DW_WeaponModel =
{
    if(xInst == world || !xInst.wi_dwvar || DW_VariantFor(xWid) != xInst.wi_dwvar || !DW_Data()) { return ""; }
    string m = DW_VRModelOf(xWid);
    float bit = pow(2, xInst.wi_dwvar - 1);
    if(xInst.wi_dwvar == DW_VAR_LANCE && DW_LancePole()) { bit = 8; } // (its own model: either may be drawn this server)
    if(xInst.wi_dwvar == DW_VAR_ROTARY && DW_RotaryGauntlet()) { bit = 16; } // (the same)
    if(!(dw_vrmodels_made & bit))
    {
        dw_vrmodels_made = dw_vrmodels_made | bit;
        entity e = spawn();
        setmodel(e, m); // (setmodel precaches: a hand's or a holster's model string needs it too)
        remove(e);
    }
    return m;
};

// The variant a weapon of kind `xWid` can be (0: none).
float(float xWid) DW_VariantFor =
{
    if(xWid == WID_SUPER_SHOTGUN) { return DW_VAR_ROTARY; }
    if(xWid == WID_LIGHTNING) { return DW_VAR_LANCE; }
    return 0;
};

// SetChangeParms: the carried weapons' variants, 2 bits each (DW_VAR_* 0-3), the hands' then the holsters' (16 bits).
float(entity p) DW_PackWeaponVariants =
{
    float packed = 0, k = 1;
    for(float i = 0; i < 8; i++)
    {
        entity inst = AD_CarriedInst(p, i);
        if(inst != world) { packed = packed + (inst.wi_dwvar & 3) * k; }
        k = k * 4;
    }
    return packed;
};

// DecodeLevelParms, the records carried: their variants back (a weapon of another kind: none).
void(entity p, float xPacked) DW_UnpackWeaponVariants =
{
    float wids[8];
    wids[0] = p.weapon; wids[1] = p.weapon2; wids[2] = p.holsterweapon0; wids[3] = p.holsterweapon1;
    wids[4] = p.holsterweapon2; wids[5] = p.holsterweapon3; wids[6] = p.holsterweapon4; wids[7] = p.holsterweapon5;
    for(float i = 0; i < 8; i++)
    {
        float code = xPacked - floor(xPacked / 4) * 4;
        xPacked = floor(xPacked / 4);
        entity inst = AD_CarriedInst(p, i);
        if(inst == world) { continue; }
        inst.wi_dwvar = code == DW_VariantFor(wids[i]) ? code : 0;
        if(inst.wi_dwvar) { inst.wi_adup = 0; }
    }
};

// The Crystal Lance's beam: through every body on its line (8 at most), 150 to each, thrown back.
void(float xHand) dw_fire_lance =
{
    vector src = VRGetMuzzlePos(xHand);
    vector dir = VRGetWeaponFireRotFwd(xHand);
    vector start = src;
    entity ignore = self;
    float n;
    for(n = 0; n < 8; n++)
    {
        traceline(src, src + dir * 8192, FALSE, ignore);
        if(!trace_ent.takedamage || trace_ent == world) { break; }
        entity hit = trace_ent;
        vector end = trace_endpos;
        T_Damage(hit, self, self, 150);
        if(hit.health <= 0) { hit.velocity = hit.velocity + dir * 400 + '0 0 175'; }
        // (dwellweap: the bolt crackling through it, violet crystal sparks where it enters and leaves)
        particle(end, dir * 30, 148 + rint(random() * 6), 40);
        particle(end + dir * 24, dir * 60, 148 + rint(random() * 6), 24);
        ignore = hit;
        src = end + dir * 4;
    }
    particle(trace_endpos - dir * 2, dir * -40, 148 + rint(random() * 6), 32); // (and where it stops)
    dprint(sprintf("dwlance: the beam through %g bodies, %.0f long\n", n, vlen(trace_endpos - start)));
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity(MSG_BROADCAST, dw_beam_prop ? dw_beam_prop : self); // (an armed grunt's: from its gun, as its lightning)
    WriteByte(MSG_BROADCAST, dw_beam_prop ? 0 : xHand); /* disambiguator (the beam per hand: without it the client misread the message) */
    WriteVec3(MSG_BROADCAST, start);
    WriteVec3(MSG_BROADCAST, trace_endpos);
};

// W_Fire_Shotgun, W_Fire_SuperShotgun, W_Fire_Lightning: the weapon in `xHand` is Dwell's variant: its shot and its
// refire. TRUE: fired (or clicked empty).
float(float xHand, float* xOutAttackFinished) DW_FireVariant =
{
    entity inst = VRGetEntWeaponInst(self, xHand);
    float v = inst != world ? inst.wi_dwvar : 0;
    if(v != DW_VAR_ROTARY && v != DW_VAR_LANCE) { return FALSE; } // (an older save's retired Rifle, 3: a plain shotgun)
    vector dir = VRGetWeaponFireRotFwd(xHand);
    if(v == DW_VAR_LANCE)
    {
        if(!delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 6))
        {
            sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.3;
            return TRUE;
        }
        sound(self, VRGetGunChannel(xHand), "weapons/lhit.wav", 1, ATTN_NORM);
        dw_fire_lance(xHand);
        dw_variant_shots++;
        self.punchangle_x = -3;
        weaponfired(self, xHand);
        VRGunHaptic(xHand, 0.7, 80, 1.0);
        *xOutAttackFinished = time + RuneApplyHell(1.4, self);
        return TRUE;
    }
    if(!delAmmoOrClipAndUpdateCounter(xHand, AID_SHELLS, 1))
    {
        sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
        *xOutAttackFinished = time + 0.3;
        return TRUE;
    }
    // The Rotary Shotgun.
    sound(self, VRGetGunChannel(xHand), "weapons/shotgn2.wav", 1, ATTN_NORM);
    self.punchangle_x = -2;
    vr_pellet_damage = 2;
    vr_decap_weapon = QVR_DECAP_SSG;
    FireBullets(xHand, 20, dir, '0.1 0.02 0');
    *xOutAttackFinished = time + RuneApplyHell(0.2, self);
    vr_decap_weapon = 0;
    vr_pellet_damage = 0;
    dw_variant_shots++;
    weaponfired(self, xHand);
    VRGunHaptic(xHand, 0.4, 70, 0.8);
    ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, 1, 0.3, 0);
    return TRUE;
};

// The Crystal Lance as a charged shot (lancecharge; his note 2026-11-23: the trigger held repeated the beam's look while
// its damage came in pulses). The trigger held charges it over vr_dw_lance_charge s (the client gathers violet sparks
// and a growing light at its crystal tip, with a rising hum: vr_lancecharge.cpp); let go short of full, the charge
// fades out (no shot, no cells); let go full, it fires (DW_FireVariant through W_Attack: 6 cells, the beam, which the
// clients leave lingering vr_dw_lance_bolt s). Every client hears of each charge begun, let go short or fired
// (QVR_SVC_LANCECHARGE: [short player][byte hand][byte 1 charging, 0 let go short, 2 fired][byte its seconds * 50]),
// so the others see it on his copy's Lance. vr_dw_lance_charge 0: it fires as the trigger is pulled (each 1.4 s held).
// An armed grunt's Lance fires at once (VR_AG_ShootProp calls DW_FireVariant itself).
.float dw_lc0, dw_lc1;     // each hand's charge (0 .. 1)
.float dw_lc_t0, dw_lc_t1; // when it was last advanced (a frame skipped longer: forgotten)
.float dw_lc_s0, dw_lc_s1; // when it began (the developer print's time to full)

// The weapon in `xHand` is the Crystal Lance (its gun's shot: the primary ammo).
float(float xHand) DW_LanceInHand =
{
    if(VRGetWeapon(xHand) != WID_LIGHTNING || (VRGetWeaponFlags(xHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO)) { return FALSE; }
    entity inst = VRGetEntWeaponInst(self, xHand);
    return inst != world && inst.wi_dwvar == DW_VAR_LANCE;
};

float(float xHand) DW_LanceCharged =
{
    return cvar_hget(cvarh_vr_dw_lance_charge) > 0 && DW_LanceInHand(xHand);
};

// Its 6 cells are there (as delAmmoOrClipAndUpdateCounter pays them: the clip, else the pouch).
float(float xHand) DW_LanceCanPay =
{
    float have = VRIsWeaponReloadingEnabledFor(self, xHand) ? VRGetWeaponClip(xHand) : VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_CELLS);
    return have >= 6;
};

void(float xHand, float xState) DW_LanceChargeSend =
{
    WriteByte(MSG_ALL, 39); // svc_quakevr
    WriteByte(MSG_ALL, 56); // QVR_SVC_LANCECHARGE
    WriteEntity(MSG_ALL, self);
    WriteByte(MSG_ALL, xHand);
    WriteByte(MSG_ALL, xState);
    WriteByte(MSG_ALL, rint(min(255, max(1, cvar_hget(cvarh_vr_dw_lance_charge) * 50))));
};

// W_WeaponFrameImpl, each frame the hand may attack (its refire over), the Lance in `xHand` (DW_LanceCharged), its
// trigger `xPressed`: the charge advanced, begun or let go. TRUE: attack now (let go fully charged). The trigger held
// without its 6 cells: no charge, the empty click.
float(float xHand, float xPressed) DW_LanceChargeFrame =
{
    float off = xHand == cVR_OffHand;
    float charge = off ? self.dw_lc0 : self.dw_lc1;
    float last = off ? self.dw_lc_t0 : self.dw_lc_t1;
    float began = off ? self.dw_lc_s0 : self.dw_lc_s1;
    float dt = time - last;
    if(dt > 0.25 || dt < 0) { charge = 0; } // (not advanced since: the refire, the weapon away, a load)
    float full = charge >= 1;
    float fire = FALSE;
    if(xPressed && !DW_LanceCanPay(xHand))
    {
        if(charge > 0) { DW_LanceChargeSend(xHand, 0); dw_lc_cancels++; }
        charge = 0;
        // The empty click, as W_AttackImpl's (its W_CheckNoAmmo lets a few cells through: the shot then refused).
        sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
        VRSetAttackFinished(xHand, time + 0.5);
    }
    else if(xPressed)
    {
        if(charge <= 0)
        {
            DW_LanceChargeSend(xHand, 1);
            dw_lc_starts++;
            began = time;
            dprint(sprintf("dwlance: charging (hand %g, %.2f s)\n", xHand, cvar_hget(cvarh_vr_dw_lance_charge)));
        }
        // (full vr_dw_lance_charge s after it began; above 0 from its first frame: begun)
        charge = min(1, max(0.001, (time - began) / max(0.05, cvar_hget(cvarh_vr_dw_lance_charge))));
        if(charge >= 1 && !full)
        {
            VRGunHaptic(xHand, 0.12, 120, 0.5); // (charged: a tick in the hand)
            dprint(sprintf("dwlance: charged (hand %g) after %.3f s\n", xHand, time - began));
        }
    }
    else if(charge > 0)
    {
        if(full)
        {
            DW_LanceChargeSend(xHand, 2);
            dw_lc_fires++;
            fire = TRUE;
            dprint(sprintf("dwlance: let go charged (hand %g): the shot\n", xHand));
        }
        else
        {
            DW_LanceChargeSend(xHand, 0);
            dw_lc_cancels++;
            dprint(sprintf("dwlance: let go at %.2f (hand %g): no shot\n", charge, xHand));
        }
        charge = 0;
    }
    if(off) { self.dw_lc0 = charge; self.dw_lc_t0 = time; self.dw_lc_s0 = began; }
    else    { self.dw_lc1 = charge; self.dw_lc_t1 = time; self.dw_lc_s1 = began; }
    return fire;
};

// ---------------------------------------------------------------------------------------------------------------------
// World: misc_crystal (a crystal cluster's model by its style 1-8, turned by `mangle`), func_fade (a brush fading in as
// a player comes near: `alpha` near, `alpha2` far, between 192 and 384 units; used, it fades out or back), and
// info_storycamera (used, its message and its sound: Dwell's camera flight is P4's).

/*dw:QUAKED misc_crystal (0 .5 .8) (-8 -8 -8) (8 8 8)
Dwell: a crystal cluster.
style: 1-8, its shape
mangle: its angles
*/
void() dw_misc_crystal =
{
    if(!DW_Data()) { remove(self); return; }
    string m = "progs/e2/crystals/crystal_small1.bsp";
    if(self.style == 2) { m = "progs/e2/crystals/crystal_small2.bsp"; }
    else if(self.style == 3) { m = "progs/e2/crystals/crystal_smallclust1.bsp"; }
    else if(self.style == 4) { m = "progs/e2/crystals/crystal_med1.bsp"; }
    else if(self.style == 5) { m = "progs/e2/crystals/crystal_med2.bsp"; }
    else if(self.style == 6) { m = "progs/e2/crystals/crystal_medclust1.bsp"; }
    else if(self.style == 7) { m = "progs/e2/crystals/crystal_medclust2.bsp"; }
    else if(self.style == 8) { m = "progs/e2/crystals/crystal_chonk1.bsp"; }
    else if(self.style == 9) { m = "progs/e2/crystals/crystal_med1.bsp"; }
    if(self.mangle != '0 0 0') { self.angles = self.mangle; }
    if(self.angles == '0 -1 0') { self.angles = '-90 0 0'; }
    else if(self.angles == '0 -2 0') { self.angles = '90 0 0'; }
    precache_model(m);
    setmodel(self, m);
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
};

void() dw_fade_think =
{
    self.nextthink = time + 0.05;
    entity p = find(world, classname, "player");
    float near = 1000000;
    while(p)
    {
        near = min(near, vlen(p.origin - self.oldorigin));
        p = find(p, classname, "player");
    }
    float f = clamp(1 - (near - 192) / 192, 0, 1); // (1 within 192 units, 0 beyond 384)
    if(self.state) { f = 0; }
    self.dw_dist = self.dw_dist + clamp(f - self.dw_dist, -self.lip, self.lip); // (its speed: a step a think)
    self.alpha = self.cu_alpha2 + (self.dw_f0 - self.cu_alpha2) * self.dw_dist;
    if(self.alpha <= 0.01) { self.alpha = 0.01; }
};
void() dw_fade_use =
{
    self.state = !self.state;
};
/*dw:QUAKED func_fade (0 .5 .8) ?
Dwell: a brush fading in as a player comes near (192-384 units).
alpha: near
alpha2: far
speed: fade speed
*/
void() dw_func_fade =
{
    if(!DW_Data()) { remove(self); return; }
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NOCLIP;
    setmodel(self, self.model);
    self.dw_f0 = self.alpha ? self.alpha : 1;
    self.cu_alpha2 = self.cu_alpha2 + 0.000001;
    self.alpha = self.cu_alpha2;
    self.oldorigin = (self.absmin + self.absmax) * 0.5;
    self.lip = self.speed ? clamp(0.05 * self.speed, 0.01, 1) : 1;
    self.state = self.spawnflags & 2 ? TRUE : FALSE;
    self.use = dw_fade_use;
    self.think = dw_fade_think;
    self.nextthink = time + 0.1;
};

// Its pages (message, then noise, noise1..noise4: Dwell keeps the story's text there) to whoever used it, 5 s each,
// then its targets.
void() dw_storycam_page =
{
    string s = self.cnt == 0 ? self.message : self.cnt == 1 ? self.noise : self.cnt == 2 ? self.noise1 :
               self.cnt == 3 ? self.noise2 : self.cnt == 4 ? self.noise3 : self.cnt == 5 ? self.noise4 : "";
    if(s == "" || !self.enemy)
    {
        activator = self.enemy;
        SUB_UseTargets();
        return;
    }
    centerprint(self.enemy, s);
    self.cnt = self.cnt + 1;
    self.think = dw_storycam_page;
    self.nextthink = time + 5;
};
void() dw_storycam_use =
{
    self.enemy = activator;
    self.cnt = 0;
    dw_storycam_page();
};
/*dw:QUAKED info_storycamera (0 .5 .8) (-8 -8 -8) (8 8 8)
Dwell: used, shows its pages (message, noise, noise1..noise4), 5 s each, then fires its targets.
*/
void() dw_info_storycamera =
{
    if(!DW_Data()) { remove(self); return; }
    self.use = dw_storycam_use;
};
