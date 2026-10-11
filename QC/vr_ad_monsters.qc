// Arcane Dimensions' monsters with Quake VR's treatment (docs/vr-port/ARCANE_DIMENSIONS.md, section 16; the fields and
// declarations: vr_ad_mondefs.qc). After AD's files: it uses their names.
//
// What each AD monster gets, and where:
// - its ragdoll on death (the engine: vr_ad_rigs.inc, by its model), its limbs cut and its head cut or popped off
//   (vr_limbs.qc, vr_decap.qc: through VR_AD_Dying, from AD's Killed), its head zone (PositionalHead: head shots, melee
//   head hits) and head gib (AD's own head model);
// - knockdowns (vr_knockdown.qc: a shove's chance as its nearest Quake VR class's, the get-up a death played backwards),
//   its pain kept off while it lies (VR_AD_KdSave, VR_AD_PainDown), killed lying: its death at once, the ragdoll stays;
// - shoves and bashes (VR_Push: any monster), a bash's stagger and a parry's (AD's pain function: VR_AD_Pain);
// - parries of its melee (VR_ADM_MELEE), armour's sound (VR_ADM_ARMOUR);
// - its corpse gibbed as Quake VR's (VR_Corpse_Parts: AD's head and its unique gib), gore pools, small gibs, wounds and
//   damage numbers (VR_AD_Hit, AD's T_Damage), sneak attacks and the damage settings (AD_Damage).

// ---------------------------------------------------------------------------------------------------------------------
// The data by class

// Self's: its bits (VR_ADM_*), head zone (forward, up, radius: Misc/quakevr/ragdoll/headfit.py on its model's stand
// frame) and corpse health's cvar (its nearest Quake VR class's).
void(float xBits, float xForward, float xUp, float xRadius, float xCorpseH) VR_AD_Mon =
{
    self.vr_ad_mon = VR_ADM_ON | xBits;
    self.vr_ad_headf = xForward;
    self.vr_ad_headu = xUp;
    self.vr_ad_headr = xRadius;
    self.vr_ad_corpseh = xCorpseH;
};

// AD's monster_start (self the monster, its class set): its data. A class not here gets none of Quake VR's treatment
// but what any monster gets (hit hooks, shoves).
void() VR_AD_MonsterSetup =
{
    float t = self.ad_classtype;
    self.vr_ad_mon = 0;
    // Death knight (progs/mon_dknight.mdl: deaths 212-223, 224-243). Head zone: headfit.py mon_dknight 27 1 7 (middle
    // 6.5 0.3 31.3, radius 7.3; its rig's head 8.2 -0.3 32.3).
    if(t == ad_CT_MONDKNIGHT)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 7, 32, 7, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 223, 212, 243, 224);
    }
    // AD's soldiers (progs/mon_soldier.mdl: Quake VR's grunt's mesh and id's frames: deaths 8-17, 18-28): the grunt's head
    // zone (PositionalHead's monster_army).
    else if(t == ad_CT_MONARMYROCKET || t == ad_CT_MONARMYGRENADE || t == ad_CT_MONARMYPLASMA || t == ad_CT_MONARMY)
    {
        VR_AD_Mon(0, 3, 23, 7.2, cvarh_vr_corpse_health_grunt);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_army, 17, 8, 28, 18);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Quake VR's side

float(entity e) VR_AD_PositionalHead =
{
    if(e.vr_ad_headr <= 0)
    {
        return FALSE;
    }
    xHeadForward = e.vr_ad_headf;
    xHeadUp = e.vr_ad_headu;
    xHeadRadius = e.vr_ad_headr;
    return TRUE;
};

// Its head gib: AD's own (the head AD's gibbing throws), if it has a head zone.
string(entity e) VR_AD_HeadModel =
{
    if(e.vr_ad_headr <= 0 || e.ad_headmdl == "")
    {
        return "";
    }
    return e.ad_headmdl;
};

// Its corpse: AD's head, its unique gib (a knight's sword, an ogre's weapon) first, Quake's gibs.
float(entity e) VR_AD_CorpseParts =
{
    if(!e.vr_ad_corpseh)
    {
        return FALSE;
    }
    vr_corpse_head = e.ad_headmdl;
    if(e.ad_gib1mdl != "")
    {
        vr_corpse_gib1 = e.ad_gib1mdl;
    }
    vr_corpse_healthh = e.vr_ad_corpseh;
    return TRUE;
};

// A bash's or a parry's stagger: its pain function, as AD's T_Damage calls it (pain blocked no longer: AD's own flinch
// rules then say whether it plays a pain animation); a bash's keeps it from attacking for `xStagger` seconds.
void(entity e, entity attacker, float damage, float xStagger) VR_AD_Pain =
{
    if(!e.ad_th_pain || e.health <= 0)
    {
        return;
    }
    entity oldSelf = self;
    self = e;
    e.ad_pain_finished = 0;
    e.ad_th_pain(attacker, attacker, damage);
    if(xStagger > 0)
    {
        e.ad_attack_finished = max(e.ad_attack_finished, time + xStagger);
    }
    self = oldSelf;
};

float(entity e) VR_AD_IsRunning =
{
    return e.vr_ad_mon && e.ad_th_run && e.think == e.ad_th_run;
};

void() VR_AD_RunNow =
{
    self.think = self.ad_th_run;
    self.nextthink = time + 0.1;
};

// ---------------------------------------------------------------------------------------------------------------------
// AD's side (ad_import.py MON_PATCHES)

// A knocked-down one's state before AD's T_Damage reacts (its FoundTarget, its pain), put back by VR_AD_PainDown.
entity vr_ad_kd_ent;
float vr_ad_kd_frame;
float vr_ad_kd_next;
var void() vr_ad_kd_think;

void(entity targ) VR_AD_KdSave =
{
    if(!targ.vr_knockdown)
    {
        return;
    }
    vr_ad_kd_ent = targ;
    vr_ad_kd_frame = targ.frame;
    vr_ad_kd_think = targ.think;
    vr_ad_kd_next = targ.nextthink;
};

// Knocked down or getting up (vr_knockdown.qc): no pain animation (the ragdoll, or its get-up playing on), its pain's
// sound; down, each hit keeps it down a little longer (as T_DamageDeal's).
float(entity targ) VR_AD_PainDown =
{
    if(!targ.vr_knockdown || targ != vr_ad_kd_ent)
    {
        return FALSE;
    }
    vr_ad_kd_ent = world;
    targ.frame = vr_ad_kd_frame;
    targ.think = vr_ad_kd_think;
    targ.nextthink = vr_ad_kd_next;
    if(targ.vr_knockdown == 1)
    {
        targ.vr_kd_until = min(max(targ.vr_kd_until, time) + cvar_hget(cvarh_vr_knockdown_hit_time),
            time + cvar_hget(cvarh_vr_knockdown_time_max));
    }
    if(targ.ad_pain_sound != "" && targ.ad_pain_finished < time)
    {
        sound(targ, CHAN_VOICE, targ.ad_pain_sound, 1, ATTN_NORM);
        targ.ad_pain_finished = time + 0.6;
    }
    return TRUE;
};

float vr_ad_kd_down;  // dying knocked down (a ragdoll already): its death code at once after (VR_AD_Died)
float vr_ad_kd_model;

// AD's Killed, self dying, before its death function: what Quake VR's Killed does then (combat.qc).
void(entity attacker) VR_AD_Dying =
{
    vr_ad_kd_down = FALSE;
    if(!self.vr_ad_mon)
    {
        return;
    }
    VR_Stealth_Died(self);
    self.vr_ai_gate = 0;
    VR_Parry_Unsway(self);
    VR_Corpse_DieWhole(self);
    VR_Decap_Killed(self); // (the armed blow beheads it or cuts a limb off; an explosion's pops limbs)
    VR_Shock_Killed(self);
    vr_ad_kd_down = self.vr_knockdown == 1;
    vr_ad_kd_model = self.modelindex;
    self.vr_knockdown = 0;
};

// ... after it.
void() VR_AD_Died =
{
    if(!self.vr_ad_mon)
    {
        return;
    }
    if(vr_ad_kd_down && self.modelindex == vr_ad_kd_model && !self.ad_gibbed)
    {
        VR_Knockdown_FastForwardDeath(); // (its ragdoll stays as it lies)
    }
    vr_ad_kd_down = FALSE;
    if(!self.ad_gibbed)
    {
        VR_Corpse_Watch(self);
        if(self.vr_corpse == 1 && self.solid == SOLID_NOT_BUT_TOUCHABLE && cvar_hget(cvarh_vr_corpse_dying))
        {
            VR_Corpse_ArmDying(self); // (its death over already: knocked down, VR_AD_BodySolid made it so)
        }
        VR_Gore_Killed(self);
    }
};

// Its death's last frame: AD makes the body a trigger (to touch the void's and hurt triggers); Quake VR's corpses and
// ragdolls are touchable, not solid.
void() VR_AD_BodySolid =
{
    if(self.vr_ad_mon && !self.ad_gibbed)
    {
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        if(self.vr_corpse == 1 && cvar_hget(cvarh_vr_corpse_dying))
        {
            VR_Corpse_ArmDying(self); // (not solid now: as Quake VR's when its death code makes it so, VR_Corpse_Think2)
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// The debug spawner (Debug > Tests, vr_test_spawn N with impulse 241; func_enemy_dispenser's "weapon"): AD's monsters
// from 200, in the order of their use in AD's maps. In a map played by AD's rules only (AD's world setup).

string(float choice) VR_AD_DispenserModel =
{
    if(choice == 200) { return "progs/mon_dknight.mdl"; }
    if(choice == 211) { return "progs/mon_soldier.mdl"; }
    if(choice == 223) { return "progs/mon_soldier.mdl"; }
    if(choice == 227) { return "progs/mon_soldier.mdl"; }
    return "";
};

float(float choice) VR_AD_DispenserSpawn =
{
    if(choice < 200 || choice >= 300)
    {
        return FALSE;
    }
    if(!AD_Rules())
    {
        dprint("admon: AD's monsters spawn in a map played by AD's rules only\n");
        remove(self);
        return TRUE;
    }
         if(choice == 200) { self.classname = "monster_dknight"; ad_monster_dknight(); }
    else if(choice == 211) { self.classname = "monster_army_rocket"; ad_monster_army_rocket(); }
    else if(choice == 223) { self.classname = "monster_army_plasma"; ad_monster_army_plasma(); }
    else if(choice == 227) { self.classname = "monster_army_grenade"; ad_monster_army_grenade(); }
    else
    {
        dprint(sprintf("admon: no AD monster %g\n", choice));
        remove(self);
    }
    return TRUE;
};
