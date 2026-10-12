// Arcane Dimensions' monsters with Quake VR's treatment (docs/vr-port/ARCANE_DIMENSIONS.md, section 17; the fields and
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
    VR_ADGun_Prepare(t); // (an armed soldier's gun: made and precached as it spawns, not at its death: vr_ad_enemyguns.qc)
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
    // Zombie knight (progs/mon_zombiek.mdl): no death of its own (AD's zombies die gibbed; Quake VR's beheading or a cut
    // limb kills it for good, VR_AD_ZombieKilled). Its knockdown gets up as AD's own (paine: down at paine12, up from 13).
    // Head: its rig's head 4.7 5.3 21.8 (headfit.py mon_zombiek 18 -3: 2.8 3.1 21.1, 7.0).
    else if(t == ad_CT_MONZOMBIEK)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR | VR_ADM_ZOMBIE, 4, 22, 6.5, cvarh_vr_corpse_health_zombie);
        VR_Knockdown_SetupOwn(cvarh_vr_knockdown_chance_zombie, 132, ad_zombiek_paine13);
    }
    // Crossbow knight (progs/mon_dcrossbow.mdl: deaths 225-235, 236-255). Head: headfit.py mon_dcrossbow 19 -2 8 (3.9 -1.4
    // 21.4, 6.0).
    else if(t == ad_CT_MONDCROSS)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 4.5, 21.5, 6, cvarh_vr_corpse_health_knight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_knight, 235, 225, 255, 236);
    }
    // Hunter ogres: the cleaver's (progs/mon_hogre.mdl: deaths S 154-163, A 164-173, B 174-187) and the mace's
    // (progs/mon_hogremac.mdl: A 132-141, B 142-155). Head: their rigs' (10.8 -0.5 27.0; 10.3 -0.8 26.3).
    else if(t == ad_CT_MONHOGRE)
    {
        VR_AD_Mon(VR_ADM_MELEE, 10.5, 27, 7.5, cvarh_vr_corpse_health_ogre);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_ogre, 173, 164, 187, 174);
    }
    else if(t == ad_CT_MONHOGREMAC)
    {
        VR_AD_Mon(VR_ADM_MELEE, 10, 26.5, 7.5, cvarh_vr_corpse_health_ogre);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_ogre, 141, 132, 155, 142);
    }
    // Spiders, green and brown (progs/mon_spiderg.mdl, mon_spiderb.mdl: one mesh; death 114-133): its head the front of
    // its body (5.3 -0.1 -15.4), below its origin; never beheaded (its head is its body's bone: one of its own was unstable).
    else if(t == ad_CT_MONSPIDER || t == ad_CT_MINIONSPIDER)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_NOHEAD, 6, -15, 6, cvarh_vr_corpse_health_dog);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_dog, 133, 114, -1, -1);
    }
    // Death guard (progs/mon_dguard.mdl: deaths A 195-206, B 207-226, C 227-240, D 241-255). Head: headfit.py mon_dguard
    // 20 0 7 (5.5 -0.2 22.6, 5.1).
    else if(t == ad_CT_MONDGUARD)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 6, 22.5, 5.5, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 206, 195, 226, 207);
    }
    // Skull wizard (progs/mon_skullwiz.mdl: its first frame its death's, 0-14; a robe, no legs). Head: its rig's 2.2 -3.3
    // 23.1, headfit.py mon_skullwiz 22 -2 7 (6.9 -1.0 27.0, 7.3).
    else if(t == ad_CT_MONSKULLW)
    {
        VR_AD_Mon(VR_ADM_NOHEAD, 5, 25, 7, cvarh_vr_corpse_health_grunt); // (its death throws its skull: never beheaded)
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_army, 14, 0, -1, -1);
    }
    // The hammer's hunter ogre (progs/mon_hogreham.mdl: deaths A 137-146, B 147-160). Head: its head cluster's middle
    // (7.0 -0.4 28.2).
    else if(t == ad_CT_MONHOGREHAM)
    {
        VR_AD_Mon(VR_ADM_MELEE, 7, 28.5, 7.5, cvarh_vr_corpse_health_ogre);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_ogre, 146, 137, 160, 147);
    }
    // Minotaur (progs/mon_minotaur.mdl: 86 units tall; its first frame a walk's; death 36-60). Head: its rig's 14.8 -12.5
    // 57.4 (turned to its right in that frame: a wider zone).
    else if(t == ad_CT_MONMINOTAUR)
    {
        VR_AD_Mon(VR_ADM_MELEE, 14, 58, 11, cvarh_vr_corpse_health_shambler);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_shambler, 60, 36, -1, -1);
    }
    // Death fury (progs/mon_dfury.mdl: two swords; deaths 224-240, 241-254). Head: headfit.py mon_dfury 30 -3 7 (3.7
    // -0.2 36.7, 8.0: its horned helmet).
    else if(t == ad_CT_MONDFURY)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 4, 36.5, 7, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 240, 224, 254, 241);
    }
    // AD's enforcers, the defender and the eliminator (progs/mon_enforcer.mdl, id's: deaths 41-54, 55-65). Head: its
    // head cluster's middle (-4.5 -1.3 29.0, behind its origin).
    else if(t == ad_CT_MONDEFENDER || t == ad_CT_MONELIMATOR || t == ad_CT_MONENFORCER)
    {
        VR_AD_Mon(VR_ADM_ARMOUR, -4, 29, 6, cvarh_vr_corpse_health_enforcer);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_enforcer, 54, 41, 65, 55);
    }
    // Flyers (no knockdown): the gargoyle (progs/mon_gargoyle.mdl, its first frame its death's, 0-13; the minotaur's
    // minions too) and the gaunt (progs/mon_gaunt.mdl: deaths 93-100, 101-108; the judicator's minions too). Heads: their
    // rigs' (9.4 -1.6 19.9; 7.6 0.0 2.6).
    else if(t == ad_CT_MONGARGOYLE || t == ad_CT_MINIONGARGOYLE)
    {
        VR_AD_Mon(VR_ADM_MELEE, 10, 21, 8, cvarh_vr_corpse_health_scrag);
    }
    else if(t == ad_CT_MONGAUNT || t == ad_CT_MINIONGAUNT)
    {
        VR_AD_Mon(0, 8, 3, 6, cvarh_vr_corpse_health_scrag);
    }
    // Swampling and voreling (progs/mon_swampling.mdl, mon_voreling.mdl and their purple skins: one small mesh, three
    // legs and a head on top; deaths 58-63, 64-70). Head: its rig's -0.5 0.0 -4.7.
    else if(t == ad_CT_MONSWAMPLING || t == ad_CT_MONVORELING || t == ad_CT_MINIONVORELING)
    {
        VR_AD_Mon(VR_ADM_MELEE, 0, -4.5, 4, cvarh_vr_corpse_health_dog);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_dog, 63, 58, 70, 64);
    }
    // Centurion (progs/mon_centurion.mdl: on its hover board, a flyer: no knockdown; deaths 18-31, 32-42). Head:
    // headfit.py mon_centurion 31 -5 7 (-0.2 -1.6 36.5, 5.7).
    else if(t == ad_CT_MONCENTURION)
    {
        VR_AD_Mon(VR_ADM_ARMOUR, 0, 36.5, 5.5, cvarh_vr_corpse_health_enforcer);
    }
    // Drole (progs/mon_drole.mdl: four arms; death 169-194). Head: its rig's 16.8 -0.3 51.1.
    else if(t == ad_CT_MONDROLE)
    {
        VR_AD_Mon(VR_ADM_MELEE, 17, 51, 7, cvarh_vr_corpse_health_shambler);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_shambler, 194, 169, -1, -1);
    }
    // Scorpion (progs/mon_scorpion.mdl: death 74-94). Head: its head cluster's 14.7 1.7 -9.7, below its origin; never
    // beheaded (its head moves with its body: no bone of its own).
    else if(t == ad_CT_MONSCORPION || t == ad_CT_MINIONSCORPION)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_NOHEAD, 14, -9.5, 6, cvarh_vr_corpse_health_scourge);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_dog, 94, 74, -1, -1);
    }
    // Stone golem (progs/mon_golem.mdl: 105 units tall; death 157-178). Head: headfit.py mon_golem 66 0 10 (7.6 0.4 74.3,
    // 10.2).
    else if(t == ad_CT_MONGOLEM)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 8, 74, 10, cvarh_vr_corpse_health_shambler);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_shambler, 178, 157, -1, -1);
    }
    // Death sergeant (progs/mon_dsergeant.mdl: the flail its own bone; deaths 223-234, 235-254). Head: headfit.py
    // mon_dsergeant 28 0 8 (6.6 -0.2 31.7, 7.5).
    else if(t == ad_CT_MONDSERG)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 7, 32, 7, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 234, 223, 254, 235);
    }
    // Bog lord (progs/mon_bossboglord.mdl: the shambler's mesh 1.3 times as big, his frames: death 83-93): the shambler's
    // head zone scaled (14, 57, 12 -> 18, 81, 15.5).
    else if(t == ad_CT_MONBOGLORD)
    {
        VR_AD_Mon(VR_ADM_MELEE, 18, 81, 15.5, cvarh_vr_corpse_health_shambler);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_shambler, 93, 83, -1, -1);
    }
    // Seeker (progs/mon_seeker.mdl: a walker of metal; deaths 59-70, B 185-196 of five). Head: its rig's 23.8 3.3 48.6.
    else if(t == ad_CT_MONSEEKER)
    {
        VR_AD_Mon(VR_ADM_ARMOUR, 24, 48.5, 5, cvarh_vr_corpse_health_enforcer);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_enforcer, 70, 59, 196, 185);
    }
    // Pyro and fumigator (progs/mon_pyro.mdl, mon_fumigator.mdl: one mesh, id's enforcer's frames with a tank and its
    // hose; deaths 41-54, 55-65). Head: headfit.py mon_pyro 24 -9 6 (-4.6 -1.0 29.1, 5.5).
    else if(t == ad_CT_MONPYRO || t == ad_CT_MONFUMIGATOR)
    {
        VR_AD_Mon(VR_ADM_ARMOUR, -4.5, 29, 5.5, cvarh_vr_corpse_health_enforcer);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_enforcer, 54, 41, 65, 55);
    }
    // Freddie (progs/mon_freddie.mdl: an ogre on machine legs; death 112-146). Head: its rig's 7.6 2.8 49.6.
    else if(t == ad_CT_MONFREDDIE)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 7.5, 49.5, 6, cvarh_vr_corpse_health_ogre);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_ogre, 146, 112, -1, -1);
    }
    // Justice knight (progs/mon_justice.mdl: deaths A 112-121, B 103-111). Head: headfit.py mon_justice 40 -6 9 (2.6 -2.7
    // 45.7, 7.8).
    else if(t == ad_CT_MONJUSTICE)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 3, 45.5, 7.5, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 121, 112, 111, 103);
    }
    // AD's eel (progs/mon_eel.mdl: a chain of six; death 6-20; a swimmer: no knockdown). Head: its rig's 17.6 0.0 -0.4.
    else if(t == ad_CT_MONEEL)
    {
        VR_AD_Mon(VR_ADM_MELEE, 18, 0, 4.5, cvarh_vr_corpse_health_eel);
    }
    // AD's hell knight (the point knight; progs/mon_hknight.mdl, id's model: deaths 42-53, 54-62). Head: its rig's 8.6
    // -1.0 33.0 (Quake VR's own death knight's zone: 6, 33, 7.2).
    else if(t == ad_CT_MONHELLK)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 8, 33, 7, cvarh_vr_corpse_health_hellknight);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_hknight, 53, 42, 62, 54);
    }
    // The ones that burst or shrink away at death (no ragdoll, no corpse): the wraith (its head zone: its rig's head
    // cluster, -1.1 -0.1 36.0), the lost soul and the skull wizard's skulls, the boil, Jim and his rocket brother; and the
    // fishing ogres, sitting scenery (their heads: -8.7 0.8 28.9).
    else if(t == ad_CT_MONWRAITH)
    {
        VR_AD_Mon(0, -1, 36, 6, 0);
    }
    else if(t == ad_CT_MONOGREFISH)
    {
        VR_AD_Mon(VR_ADM_NOHEAD, -8.5, 29, 7, 0);
    }
    else if(t == ad_CT_MONLOSTSOUL || t == ad_CT_MINIONLOSTSOUL || t == ad_CT_MONBOIL || t == ad_CT_MONJIM ||
            t == ad_CT_MONJIMROCKET)
    {
        VR_AD_Mon(0, 0, 0, 0, 0);
    }
    // Bosses: their ragdolls, heads and limbs, no knockdown and no corpse to gib (Quake VR's bosses have none). The
    // judicator (progs/mon_judge.mdl: the justice knight's frames; deaths A 112-121, B 103-111; headfit.py mon_judge 55 -6
    // 10: 3.5 -1.4 63.6, 11.3), the ice golem (the stone golem's mesh and frames), Nour (progs/mon_bossnour.mdl: a flyer;
    // death 39-47; its head cluster 8.4 -0.4 30.6). The steel claw is a spider (its mesh and frames).
    else if(t == ad_CT_MONJUDGE)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 4, 63.5, 9, 0);
    }
    else if(t == ad_CT_MONICEGOLEM || t == ad_CT_MONICEGOLEMWAVE)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR, 8, 74, 10, 0);
    }
    else if(t == ad_CT_MONNOUR)
    {
        VR_AD_Mon(0, 8, 30, 8, 0);
    }
    // AD's scrag (progs/mon_wizard.mdl, id's: Nour's minions; death 46-53; a flyer). Head: its head cluster 6.8 0.0 25.3.
    else if(t == ad_CT_MONWIZARD || t == ad_CT_MINIONWIZARD)
    {
        VR_AD_Mon(0, 7, 25, 6, cvarh_vr_corpse_health_scrag);
    }
    else if(t == ad_CT_MONSTEELCLAW)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ARMOUR | VR_ADM_NOHEAD, 6, -15, 6, cvarh_vr_corpse_health_dog);
        VR_Knockdown_SetupReverse(cvarh_vr_knockdown_chance_dog, 133, 114, -1, -1);
    }
    // AD's zombie and mummy (progs/mon_zombie.mdl: id's zombie, Rogue's mummy's mesh 0.83 times as big; AD's zombies die
    // gibbed, Quake VR's beheading or a cut limb kills them for good). Its knockdown gets up as AD's own (paine12, 173).
    else if(t == ad_CT_MONZOMBIE || t == ad_CT_MONMUMMY)
    {
        VR_AD_Mon(VR_ADM_MELEE | VR_ADM_ZOMBIE, 0, 21.5, 5.5, cvarh_vr_corpse_health_zombie);
        VR_Knockdown_SetupOwn(cvarh_vr_knockdown_chance_zombie, 173, ad_zombie_paine13);
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
    if(e.vr_ad_headr <= 0 || e.ad_headmdl == "" || (e.vr_ad_mon & VR_ADM_NOHEAD))
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

// A zombie's death for good, beheaded or cut (AD's own always gibs it): it lies as its ragdoll does (VR_Decap_ZombieDie's).
void() VR_AD_ZombieDie =
{
    ad_monster_death_precheck(); // (dead, no think; its health -1 gibs nothing)
    self.ad_inpain = 0;
    self.solid = SOLID_NOT;
};

void(entity e, entity by) VR_AD_ZombieKilled =
{
    e.ad_th_die = VR_AD_ZombieDie;
    ad_Killed(e, by);
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

entity vr_ad_inflictor; // the blow's inflictor (AD's T_Damage, before the damage): the highlight log's kill (VR_AD_Dying)

void(entity targ, entity inflictor) VR_AD_KdSave =
{
    vr_ad_inflictor = inflictor;
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
float vr_ad_hl_score; // the highlight log's kill (VR_Highlight_Kill: 0 none) and its detail, logged after the death
string vr_ad_hl_what;

// AD's Killed, self dying, before its death function: what Quake VR's Killed does then (combat.qc).
void(entity attacker) VR_AD_Dying =
{
    vr_ad_kd_down = FALSE;
    vr_ad_hl_score = 0;
    if(!self.vr_ad_mon)
    {
        return;
    }
    vr_ad_hl_score = VR_Highlight_Kill(self, vr_ad_inflictor, attacker, vr_ad_melee);
    vr_ad_hl_what = vr_hl_what;
    VR_Stealth_Died(self);
    VR_ADGun_Drop(); // its own gun, an armed soldier's (vr_ad_enemyguns.qc)
    self.vr_ai_gate = 0;
    VR_Parry_Unsway(self);
    VR_Corpse_DieWhole(self);
    VR_Decap_Killed(self); // (the armed blow beheads it or cuts a limb off; an explosion's pops limbs)
    if(self.vr_headless && (self.vr_ad_mon & VR_ADM_ZOMBIE))
    {
        self.ad_th_die = VR_AD_ZombieDie; // (its own death gibs it)
    }
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
    VR_Highlight_Killed(self, vr_ad_hl_score, vr_ad_hl_what);
    vr_ad_hl_score = 0;
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

// ad_monsters_gen.py: begin
string(float choice) VR_AD_DispenserModel =
{
    if(choice == 200) { return "progs/mon_dknight.mdl"; }
    if(choice == 201) { return "progs/mon_zombiek.mdl"; }
    if(choice == 202) { return "progs/mon_dcrossbow.mdl"; }
    if(choice == 203) { return "progs/mon_hogre.mdl"; }
    if(choice == 204) { return "progs/mon_spiderg.mdl"; }
    if(choice == 205) { return "progs/mon_dguard.mdl"; }
    if(choice == 206) { return "progs/mon_skullwiz.mdl"; }
    if(choice == 207) { return "progs/mon_lostsoul.mdl"; }
    if(choice == 208) { return "progs/mon_hogremac.mdl"; }
    if(choice == 209) { return "progs/mon_boil.mdl"; }
    if(choice == 210) { return "progs/mon_gaunt.mdl"; }
    if(choice == 211) { return "progs/mon_soldier.mdl"; }
    if(choice == 212) { return "progs/mon_enforcer.mdl"; }
    if(choice == 213) { return "progs/mon_hogreham.mdl"; }
    if(choice == 214) { return "progs/mon_gargoyle.mdl"; }
    if(choice == 215) { return "progs/mon_minotaur.mdl"; }
    if(choice == 216) { return "progs/mon_dfury.mdl"; }
    if(choice == 217) { return "progs/mon_enforcer.mdl"; }
    if(choice == 218) { return "progs/mon_jim.mdl"; }
    if(choice == 219) { return "progs/mon_swampling.mdl"; }
    if(choice == 220) { return "progs/mon_centurion.mdl"; }
    if(choice == 221) { return "progs/mon_voreling.mdl"; }
    if(choice == 222) { return "progs/mon_golem.mdl"; }
    if(choice == 223) { return "progs/mon_soldier.mdl"; }
    if(choice == 224) { return "progs/mon_wraith.mdl"; }
    if(choice == 225) { return "progs/mon_dsergeant.mdl"; }
    if(choice == 226) { return "progs/mon_drole.mdl"; }
    if(choice == 227) { return "progs/mon_soldier.mdl"; }
    if(choice == 228) { return "progs/mon_bossboglord.mdl"; }
    if(choice == 229) { return "progs/mon_scorpion.mdl"; }
    if(choice == 230) { return "progs/mon_seeker.mdl"; }
    if(choice == 231) { return "progs/mon_justice.mdl"; }
    if(choice == 232) { return "progs/mon_pyro.mdl"; }
    if(choice == 233) { return "progs/mon_eel.mdl"; }
    if(choice == 234) { return "progs/mon_freddie.mdl"; }
    if(choice == 235) { return "progs/mon_fumigator.mdl"; }
    if(choice == 236) { return "progs/mon_ogrefish.mdl"; }
    if(choice == 237) { return "progs/mon_bossnour.mdl"; }
    if(choice == 238) { return "progs/mon_zombie.mdl"; }
    if(choice == 239) { return "progs/mon_bossicegolem.mdl"; }
    if(choice == 241) { return "progs/mon_steelclaw.mdl"; }
    if(choice == 242) { return "progs/mon_judge.mdl"; }
    if(choice == 245) { return "progs/mon_wizard.mdl"; }
    if(choice == 250) { return "progs/mon_hknight.mdl"; }
    if(choice == 300) { return "progs/artifact_tome.mdl"; }
    if(choice == 301) { return "progs/artifact_sharp.mdl"; }
    if(choice == 302) { return "progs/artifact_piercer.mdl"; }
    if(choice == 303) { return "progs/artifact_airtank.mdl"; }
    if(choice == 304) { return "progs/artifact_lavashield.mdl"; }
    if(choice == 305) { return "progs/artifact_blastbelt.mdl"; }
    if(choice == 306) { return "progs/artifact_jumpboots.mdl"; }
    if(choice == 307) { return "progs/health_5.mdl"; }
    if(choice == 308) { return "progs/health_5.mdl"; }
    if(choice == 309) { return "progs/g_shadaxe.mdl"; }
    if(choice == 310) { return "progs/g_shot3.mdl"; }
    if(choice == 311) { return "progs/g_plasma.mdl"; }
    if(choice == 312) { return "progs/mon_soldier.mdl"; }
    if(choice == 313) { return "progs/mon_soldier.mdl"; }
    if(choice == 314) { return "progs/mon_soldier.mdl"; }
    if(choice == 315) { return "progs/mon_enforcer.mdl"; }
    if(choice == 316) { return "progs/mon_enforcer.mdl"; }
    if(choice == 317) { return "progs/mon_centurion.mdl"; }
    if(choice == 318) { return "progs/mon_pyro.mdl"; }
    if(choice == 319) { return "progs/mon_fumigator.mdl"; }
    return "";
};

float(float choice) VR_AD_DispenserSpawn =
{
    if(choice < 200 || choice >= 400)
    {
        return FALSE;
    }
    if(!cvar("vr_ad_mounted") || (!AD_Rules() && !cvar("vr_ad_everywhere")))
    {
        remove(self);
        return TRUE;
    }
    if(!AD_Rules()) { VR_AD_WorldOnDemand(); }
    if(choice == 200) { self.classname = "monster_dknight"; ad_monster_dknight(); }
    else if(choice == 201) { self.classname = "monster_zombiek"; ad_monster_zombiek(); }
    else if(choice == 202) { self.classname = "monster_dcrossbow"; ad_monster_dcrossbow(); }
    else if(choice == 203) { self.classname = "monster_hogre"; ad_monster_hogre(); }
    else if(choice == 204) { self.classname = "monster_spider"; ad_monster_spider(); }
    else if(choice == 205) { self.classname = "monster_dguard"; ad_monster_dguard(); }
    else if(choice == 206) { self.classname = "monster_skullwiz"; ad_monster_skullwiz(); }
    else if(choice == 207) { self.classname = "monster_lostsoul"; ad_monster_lostsoul(); }
    else if(choice == 208) { self.classname = "monster_hogremac"; ad_monster_hogremac(); }
    else if(choice == 209) { self.classname = "monster_boil"; ad_monster_boil(); }
    else if(choice == 210) { self.classname = "monster_gaunt"; ad_monster_gaunt(); }
    else if(choice == 211) { self.classname = "monster_army_rocket"; ad_monster_army_rocket(); }
    else if(choice == 212) { self.classname = "monster_defender"; ad_monster_defender(); }
    else if(choice == 213) { self.classname = "monster_hogreham"; ad_monster_hogreham(); }
    else if(choice == 214) { self.classname = "monster_gargoyle"; ad_monster_gargoyle(); }
    else if(choice == 215) { self.classname = "monster_minotaur"; ad_monster_minotaur(); }
    else if(choice == 216) { self.classname = "monster_dfury"; ad_monster_dfury(); }
    else if(choice == 217) { self.classname = "monster_eliminator"; ad_monster_eliminator(); }
    else if(choice == 218) { self.classname = "monster_jim"; ad_monster_jim(); }
    else if(choice == 219) { self.classname = "monster_swampling"; ad_monster_swampling(); }
    else if(choice == 220) { self.classname = "monster_centurion"; ad_monster_centurion(); }
    else if(choice == 221) { self.classname = "monster_voreling"; ad_monster_voreling(); }
    else if(choice == 222) { self.classname = "monster_golem"; ad_monster_golem(); }
    else if(choice == 223) { self.classname = "monster_army_plasma"; ad_monster_army_plasma(); }
    else if(choice == 224) { self.classname = "monster_wraith"; ad_monster_wraith(); }
    else if(choice == 225) { self.classname = "monster_dsergeant"; ad_monster_dsergeant(); }
    else if(choice == 226) { self.classname = "monster_drole"; ad_monster_drole(); }
    else if(choice == 227) { self.classname = "monster_army_grenade"; ad_monster_army_grenade(); }
    else if(choice == 228) { self.classname = "monster_boglord"; ad_monster_boglord(); }
    else if(choice == 229) { self.classname = "monster_scorpion"; ad_monster_scorpion(); }
    else if(choice == 230) { self.classname = "monster_seeker"; ad_monster_seeker(); }
    else if(choice == 231) { self.classname = "monster_justice"; ad_monster_justice(); }
    else if(choice == 232) { self.classname = "monster_pyro"; ad_monster_pyro(); }
    else if(choice == 233) { self.classname = "monster_eel"; ad_monster_eel(); }
    else if(choice == 234) { self.classname = "monster_freddie"; ad_monster_freddie(); }
    else if(choice == 235) { self.classname = "monster_fumigator"; ad_monster_fumigator(); }
    else if(choice == 236) { self.classname = "monster_ogre_fishing"; ad_monster_ogre_fishing(); }
    else if(choice == 237) { self.classname = "monster_nour"; ad_monster_nour(); }
    else if(choice == 238) { self.classname = "monster_mummy"; ad_monster_mummy(); }
    else if(choice == 239) { self.classname = "monster_icegolem"; ad_monster_icegolem(); }
    else if(choice == 241) { self.classname = "monster_steelclaw"; ad_monster_steelclaw(); }
    else if(choice == 242) { self.classname = "monster_judicator"; ad_monster_judicator(); }
    else if(choice == 245) { self.classname = "monster_wizard"; ad_monster_wizard(); }
    else if(choice == 250) { self.classname = "monster_hell_knight"; ad_monster_hell_knight(); }
    else if(choice == 300) { self.classname = "item_artifact_tomeofpower"; ad_item_artifact_tomeofpower(); }
    else if(choice == 301) { self.classname = "item_artifact_sharp_shooter"; ad_item_artifact_sharp_shooter(); }
    else if(choice == 302) { self.classname = "item_artifact_nail_piercer"; ad_item_artifact_nail_piercer(); }
    else if(choice == 303) { self.classname = "item_artifact_airtank"; ad_item_artifact_airtank(); }
    else if(choice == 304) { self.classname = "item_artifact_lavashield"; ad_item_artifact_lavashield(); }
    else if(choice == 305) { self.classname = "item_artifact_blastbelt"; ad_item_artifact_blastbelt(); }
    else if(choice == 306) { self.classname = "item_artifact_jumpboots"; ad_item_artifact_jumpboots(); }
    else if(choice == 307) { self.classname = "item_healthvial"; ad_item_healthvial(); }
    else if(choice == 308) { self.classname = "item_healthgem"; ad_item_healthgem(); }
    else if(choice == 309) { self.classname = "weapon_upgrade_axe"; weapon_upgrade_axe(); }
    else if(choice == 310) { self.classname = "weapon_upgrade_ssg"; weapon_upgrade_ssg(); }
    else if(choice == 311) { self.classname = "weapon_upgrade_lg"; weapon_upgrade_lg(); }
    else if(choice == 312) { self.classname = "func_weapon_grabbable"; self.weapon = 20; func_weapon_grabbable(); }
    else if(choice == 313) { self.classname = "func_weapon_grabbable"; self.weapon = 21; func_weapon_grabbable(); }
    else if(choice == 314) { self.classname = "func_weapon_grabbable"; self.weapon = 22; func_weapon_grabbable(); }
    else if(choice == 315) { self.classname = "func_weapon_grabbable"; self.weapon = 23; func_weapon_grabbable(); }
    else if(choice == 316) { self.classname = "func_weapon_grabbable"; self.weapon = 24; func_weapon_grabbable(); }
    else if(choice == 317) { self.classname = "func_weapon_grabbable"; self.weapon = 25; func_weapon_grabbable(); }
    else if(choice == 318) { self.classname = "func_weapon_grabbable"; self.weapon = 26; func_weapon_grabbable(); }
    else if(choice == 319) { self.classname = "func_weapon_grabbable"; self.weapon = 27; func_weapon_grabbable(); }
    else
    {
        dprint(sprintf("admon: no AD monster %g\n", choice));
        remove(self);
    }
    return TRUE;
};
// ad_monsters_gen.py: end
