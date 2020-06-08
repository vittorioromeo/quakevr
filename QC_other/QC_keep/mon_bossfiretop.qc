/*==============================================================================
Chthon (Firetop Mountain Version)
==============================================================================*/
$frame rise1 rise2 rise3 rise4 rise5 rise6 rise7 rise8 rise9 rise10
$frame rise11 rise12 rise13 rise14 rise15 rise16 rise17 

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15
$frame walk16 walk17 walk18 walk19 walk20 walk21 walk22
$frame walk23 walk24 walk25 walk26 walk27 walk28 walk29 walk30 walk31

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15
$frame attack16 attack17 attack18 attack19 attack20 attack21 attack22
$frame attack23

$frame shocka1 shocka2 shocka3 shocka4 shocka5 shocka6 shocka7 shocka8
$frame shocka9 shocka10 

$frame shockb1 shockb2 shockb3 shockb4 shockb5 shockb6

$frame shockc1 shockc2 shockc3 shockc4 shockc5 shockc6 shockc7 shockc8 
$frame shockc9 shockc10

//===========================================================================
void(float ftop_mode) ftop_track =
{
    // Find a player
    if (self.enemy == world) {
        self.enemy = find(world, classname, "player");
        // reset enemy if no player found!
        if (!self.enemy) self.enemy = world;
    }
    
    // Check for any no combat conditions
    if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
    else if (intermission_running > 0) self.enemy = world;
    else if (self.enemy.health < 1) self.enemy = world;
        
    // Got an enemy?
    // Either attack or turn towards
    if (self.enemy) {
        if (ftop_mode == TRUE) self.th_missile();
        else ai_face();
    }
    
    // Update particle effect around base of Chthon
    makevectors(self.angles);
    particle_debuff(self.origin+v_forward*32, 96, rint(10+random()*40), PARTICLE_BURST_FIRE);
};

//----------------------------------------------------------------------
void() ftop_idle1    =[    $walk1, ftop_idle2 ] {ftop_track(TRUE);};
void() ftop_idle2    =[    $walk2, ftop_idle3 ] {ftop_track(TRUE);};
void() ftop_idle3    =[    $walk3, ftop_idle4 ] {ftop_track(TRUE);};
void() ftop_idle4    =[    $walk4, ftop_idle5 ] {ftop_track(TRUE);};
void() ftop_idle5    =[    $walk5, ftop_idle6 ] {ftop_track(TRUE);};
void() ftop_idle6    =[    $walk6, ftop_idle7 ] {ftop_track(TRUE);};
void() ftop_idle7    =[    $walk7, ftop_idle8 ] {ftop_track(TRUE);};
void() ftop_idle8    =[    $walk8, ftop_idle9 ] {ftop_track(TRUE);};
void() ftop_idle9    =[    $walk9, ftop_idle10 ] {ftop_track(TRUE);};
void() ftop_idle10    =[    $walk10, ftop_idle11 ] {ftop_track(TRUE);};
void() ftop_idle11    =[    $walk11, ftop_idle12 ] {ftop_track(TRUE);};
void() ftop_idle12    =[    $walk12, ftop_idle13 ] {ftop_track(TRUE);};
void() ftop_idle13    =[    $walk13, ftop_idle14 ] {ftop_track(TRUE);};
void() ftop_idle14    =[    $walk14, ftop_idle15 ] {ftop_track(TRUE);};
void() ftop_idle15    =[    $walk15, ftop_idle16 ] {ftop_track(TRUE);};
void() ftop_idle16    =[    $walk16, ftop_idle17 ] {ftop_track(TRUE);};
void() ftop_idle17    =[    $walk17, ftop_idle18 ] {ftop_track(TRUE);};
void() ftop_idle18    =[    $walk18, ftop_idle19 ] {ftop_track(TRUE);};
void() ftop_idle19    =[    $walk19, ftop_idle20 ] {ftop_track(TRUE);};
void() ftop_idle20    =[    $walk20, ftop_idle21 ] {ftop_track(TRUE);};
void() ftop_idle21    =[    $walk21, ftop_idle22 ] {ftop_track(TRUE);};
void() ftop_idle22    =[    $walk22, ftop_idle23 ] {ftop_track(TRUE);};
void() ftop_idle23    =[    $walk23, ftop_idle24 ] {ftop_track(TRUE);};
void() ftop_idle24    =[    $walk24, ftop_idle25 ] {ftop_track(TRUE);};
void() ftop_idle25    =[    $walk25, ftop_idle26 ] {ftop_track(TRUE);};
void() ftop_idle26    =[    $walk26, ftop_idle27 ] {ftop_track(TRUE);};
void() ftop_idle27    =[    $walk27, ftop_idle28 ] {ftop_track(TRUE);};
void() ftop_idle28    =[    $walk28, ftop_idle29 ] {ftop_track(TRUE);};
void() ftop_idle29    =[    $walk29, ftop_idle30 ] {ftop_track(TRUE);};
void() ftop_idle30    =[    $walk30, ftop_idle31 ] {ftop_track(TRUE);};
void() ftop_idle31    =[    $walk31, ftop_idle1 ] {ftop_track(TRUE);};

//===========================================================================
void(vector orgofs) ftop_fire =
{
    local vector offang, org, vec, dir, mdest, avel;
    local float projlen;

    // No enemy or dead?
    if (!self.enemy) return;
    if (self.enemy.health < 1) return;
    
    sound (self, CHAN_WEAPON, "chthon/attack1.wav", 1, ATTN_NORM);
    offang = vectoangles (self.enemy.origin - self.origin);    
    makevectors (offang);
    org = self.origin + attack_vector(orgofs);
    
    // Skill level adjustment (easy=250, normal=300, hard=350, nm=400)
    self.attack_speed = SPEED_LAVABALL + (skill*SPEED_LAVASKILL);
    
    // Lead the missile on hard mode (This formula is not perfect) 
    // There are plenty of missiles that go in strange directions, 
    // especially if the player strafes a lot from side to side.
    if (skill > SKILL_NORMAL) {
        projlen = vlen(self.enemy.origin - org) / self.attack_speed;
        vec = self.enemy.velocity;
        vec_z = 0;
        mdest = self.enemy.origin + projlen * vec;        
    }
    else mdest = self.enemy.origin;
    
    dir = normalize (mdest - org);
    avel = vecrand(100,200,FALSE);
    Launch_Missile (org, dir, avel, CT_PROJ_FIRETOP, self.attack_speed);
};

//----------------------------------------------------------------------
void() ftop_attack1    =[    $attack1, ftop_attack2 ] {ftop_track(FALSE);};
void() ftop_attack2    =[    $attack2, ftop_attack3 ] {ftop_track(FALSE);};
void() ftop_attack3    =[    $attack3, ftop_attack4 ] {ftop_track(FALSE);};
void() ftop_attack4    =[    $attack4, ftop_attack5 ] {ftop_track(FALSE);};
void() ftop_attack5    =[    $attack5, ftop_attack6 ] {ftop_track(FALSE);};
void() ftop_attack6    =[    $attack6, ftop_attack7 ] {ftop_track(FALSE);};
void() ftop_attack7    =[    $attack7, ftop_attack8 ] {ftop_track(FALSE);};
void() ftop_attack8    =[    $attack8, ftop_attack9 ] {ftop_fire('100 50 250');};
void() ftop_attack9    =[    $attack9, ftop_attack10 ] {ftop_track(FALSE);};
void() ftop_attack10=[    $attack10, ftop_attack11 ] {ftop_track(FALSE);};
void() ftop_attack11=[    $attack11, ftop_attack12 ] {ftop_track(FALSE);};
void() ftop_attack12=[    $attack12, ftop_attack13 ] {ftop_track(FALSE);};
void() ftop_attack13=[    $attack13, ftop_attack14 ] {ftop_track(FALSE);};
void() ftop_attack14=[    $attack14, ftop_attack15 ] {ftop_track(FALSE);};
void() ftop_attack15=[    $attack15, ftop_attack16 ] {ftop_track(FALSE);};
void() ftop_attack16=[    $attack16, ftop_attack17 ] {ftop_track(FALSE);};
void() ftop_attack17=[    $attack17, ftop_attack18 ] {ftop_track(FALSE);};
void() ftop_attack18=[    $attack18, ftop_attack19 ] {ftop_track(FALSE);};
void() ftop_attack19=[    $attack19, ftop_attack20 ] {ftop_fire('100 -50 250');};
void() ftop_attack20=[    $attack20, ftop_attack21 ] {ftop_track(FALSE);};
void() ftop_attack21=[    $attack21, ftop_attack22 ] {ftop_track(FALSE);};
void() ftop_attack22=[    $attack22, ftop_attack23 ] {ftop_track(FALSE);};
void() ftop_attack23=[    $attack23, ftop_attack1 ] {ftop_track(FALSE);
    // If no enemy go back to idling in a pool of lava!
    if (self.enemy == world) self.think = self.th_stand;
};

//----------------------------------------------------------------------
void() ftop_lightning =
{
    if (!self.movetarget2) return;
    if (!self.movetarget3) return;

    self.pos1 = self.movetarget2.origin;
    self.pos2 = self.movetarget3.origin;
    
    // compensate for length of bolt
    self.pos2 = self.pos2 - normalize(self.pos2 - self.pos1) * 100;
    
    // Generate the lightning effect
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING3);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, self.pos1_x);
    WriteCoord (MSG_BROADCAST, self.pos1_y);
    WriteCoord (MSG_BROADCAST, self.pos1_z);
    WriteCoord (MSG_BROADCAST, self.pos2_x);
    WriteCoord (MSG_BROADCAST, self.pos2_y);
    WriteCoord (MSG_BROADCAST, self.pos2_z);
};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) ftop_pain = {
    self.health = self.max_health;
};

//----------------------------------------------------------------------
void() ftop_death1 = [$death1, ftop_death2] {
    ftop_lightning();
    sound (self, CHAN_VOICE, "chthon/death1.wav", 1, ATTN_NORM);
    if (self.noise3 != "") trigger_strs(self.noise3, self);
};
void() ftop_death2 = [$death2, ftop_death3] {ftop_lightning();};
void() ftop_death3 = [$death3, ftop_death4] {ftop_lightning();};
void() ftop_death4 = [$death4, ftop_death5] {};
void() ftop_death5 = [$death5, ftop_death6] {};
void() ftop_death6 = [$death6, ftop_death7] {};
void() ftop_death7 = [$death7, ftop_death8] {};
void() ftop_death8 = [$death8, ftop_death9] {};
void() ftop_death9 = [$death9, ftop_death10] {
    sound (self, CHAN_BODY, "chthon/lavasplash.wav", 1, ATTN_NORM);
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
};
void() ftop_death10 = [$death9, ftop_death11] {
    killed_monsters = killed_monsters + 1;
    WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
    entity_hide(self);
};
void() ftop_death11 = { remove(self); };

//----------------------------------------------------------------------
void() ftop_shocka1 =[    $shocka1, ftop_shocka2 ] {ftop_lightning();};
void() ftop_shocka2 =[    $shocka2, ftop_shocka3 ] {ftop_lightning();};
void() ftop_shocka3 =[    $shocka3, ftop_shocka4 ] {ftop_lightning();};
void() ftop_shocka4 =[    $shocka4, ftop_shocka5 ] {ftop_lightning();};
void() ftop_shocka5 =[    $shocka5, ftop_shocka6 ] {ftop_lightning();};
void() ftop_shocka6 =[    $shocka6, ftop_shocka7 ] {ftop_lightning();};
void() ftop_shocka7 =[    $shocka7, ftop_shocka8 ] {ftop_lightning();};
void() ftop_shocka8 =[    $shocka8, ftop_shocka9 ] {ftop_lightning();};
void() ftop_shocka9 =[    $shocka9, ftop_shocka10 ] {ftop_lightning();};
void() ftop_shocka10 =[    $shocka10, ftop_death1 ] {ftop_lightning();
    if (self.attack_finished > time) self.think = ftop_shocka1;};

//----------------------------------------------------------------------
void() ftop_shockb1 =[    $shockb1, ftop_shockb2 ] {ftop_lightning();};
void() ftop_shockb2 =[    $shockb2, ftop_shockb3 ] {ftop_lightning();};
void() ftop_shockb3 =[    $shockb3, ftop_shockb4 ] {ftop_lightning();};
void() ftop_shockb4 =[    $shockb4, ftop_shockb5 ] {ftop_lightning();};
void() ftop_shockb5 =[    $shockb5, ftop_shockb6 ] {ftop_lightning();};
void() ftop_shockb6 =[    $shockb6, ftop_shockb7 ] {ftop_lightning();};
void() ftop_shockb7 =[    $shockb1, ftop_shockb8 ] {ftop_lightning();};
void() ftop_shockb8 =[    $shockb2, ftop_shockb9 ] {ftop_lightning();};
void() ftop_shockb9 =[    $shockb3, ftop_shockb10 ] {ftop_lightning();};
void() ftop_shockb10 =[    $shockb4, ftop_death1 ] {ftop_lightning();
    if (self.attack_finished > time) self.think = ftop_shockb1;};

//----------------------------------------------------------------------
void() ftop_shockc1 =[    $shockc1, ftop_shockc2 ] {ftop_lightning();};
void() ftop_shockc2 =[    $shockc2, ftop_shockc3 ] {ftop_lightning();};
void() ftop_shockc3 =[    $shockc3, ftop_shockc4 ] {ftop_lightning();};
void() ftop_shockc4 =[    $shockc4, ftop_shockc5 ] {ftop_lightning();};
void() ftop_shockc5 =[    $shockc5, ftop_shockc6 ] {ftop_lightning();};
void() ftop_shockc6 =[    $shockc6, ftop_shockc7 ] {ftop_lightning();};
void() ftop_shockc7 =[    $shockc7, ftop_shockc8 ] {ftop_lightning();};
void() ftop_shockc8 =[    $shockc8, ftop_shockc9 ] {ftop_lightning();};
void() ftop_shockc9 =[    $shockc9, ftop_shockc10 ] {ftop_lightning();};
void() ftop_shockc10 =[    $shockc10, ftop_death1 ] {ftop_lightning();
    if (self.attack_finished > time) self.think = ftop_shockc1;};

//----------------------------------------------------------------------
void() ftop_deathsequence =
{
    self.use = SUB_Null;
    self.enemy = world;
    self.takedamage = DAMAGE_NO;

    // Find electrode start/end points
    if (self.noise1 != "") 
        self.movetarget2 = find(world, targetname, self.noise1);
    if (self.noise2 != "") 
        self.movetarget3 = find(world, targetname, self.noise2);
    
    sound (self, CHAN_WEAPON, "misc/power.wav", 1, ATTN_NORM);
    sound (self, CHAN_VOICE, "chthon/pain1.wav", 1, ATTN_NORM);

    self.attack_finished = time + 1.5;
    self.lip = random();
    if (self.lip < 0.3) ftop_shocka1();
    else if (self.lip < 0.6) ftop_shockb1();
    else ftop_shockc1();
};

//===========================================================================
void() ftop_rise1    =[    $rise1, ftop_rise2 ] {
    sound (self, CHAN_WEAPON, "chthon/lavasplash.wav", 1, ATTN_NORM);};
void() ftop_rise2    =[    $rise2, ftop_rise3 ] {
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);};
void() ftop_rise3    =[    $rise3, ftop_rise4 ] {};
void() ftop_rise4    =[    $rise4, ftop_rise5 ] {};
void() ftop_rise5    =[    $rise5, ftop_rise6 ] {};
void() ftop_rise6    =[    $rise6, ftop_rise7 ] {};
void() ftop_rise7    =[    $rise7, ftop_rise8 ] {};
void() ftop_rise8    =[    $rise8, ftop_rise9 ] {};
void() ftop_rise9    =[    $rise9, ftop_rise10 ] {};
void() ftop_rise10    =[    $rise10, ftop_rise11 ] {};
void() ftop_rise11    =[    $rise11, ftop_rise12 ] {};
void() ftop_rise12    =[    $rise12, ftop_rise13 ] {};
void() ftop_rise13    =[    $rise13, ftop_rise14 ] {};
void() ftop_rise14    =[    $rise14, ftop_rise15 ] {};
void() ftop_rise15    =[    $rise15, ftop_rise16 ] {};
void() ftop_rise16    =[    $rise16, ftop_rise17 ] {};
void() ftop_rise17    =[    $rise17, ftop_attack1 ] {
    self.takedamage = DAMAGE_YES;
    self.th_stand = ftop_idle1;
    self.th_run = ftop_attack1;
    self.th_missile = ftop_attack1;
    self.th_pain = ftop_pain;
    self.th_die = ftop_deathsequence;
};

//----------------------------------------------------------------------
void() ftop_wakeup =
{
    self.skin = 1;        // Dark lava skin
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.velocity = '0 0 0';
    self.flags = FL_MONSTER;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    self.enemy = activator;            // Setup trigger enemy
    self.use = ftop_deathsequence;    // Final death trigger event
    
    // DP particle spark and smoke effect to Chthon, originally done by Seven
    if (ext_dppart) self.traileffectnum = particleeffectnum("TR_BOSSCHTHON");
    
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    ftop_rise1 ();
};

/*======================================================================
 QUAKED monster_firetopboss (1 0.25 0.25) (-128 -128 -24) (128 128 256)
======================================================================*/
void() monster_firetopboss =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_bosschthon.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_LAVA);

    precache_sound ("chthon/attack1.wav");
    precache_sound ("weapons/rocket1i.wav");
    precache_sound ("misc/power.wav");

    self.pain_sound = "chthon/pain1.wav";
    precache_sound ("chthon/pain1.wav");
    precache_sound ("chthon/death1.wav");

    // Rise from lava roar + splash
    self.sight_sound = "chthon/sight1.wav";
    precache_sound (self.sight_sound);
    precache_sound ("chthon/lavasplash.wav");

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.bbmins = '-128 -128 -24';        // has own entity setup
    self.bbmaxs = '128 128 256';
    self.bboxtype = BBOX_CUSTOM;
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)

    self.health = self.max_health = MEGADEATH;
    self.takedamage = DAMAGE_NO;
    self.classtype = CT_MONFIRETOP;
    self.classgroup = CG_BOSS;
    self.infightextra = 8;
    self.pain_ignore = 1;
    self.yaw_speed = 20;

    if (self.deathstring == "")  self.deathstring = " was destroyed by the Firetop Guardian\n";

    // Always reset Ammo Resistance to be consistent
    // The shamblers infront of the boss are suppose to be killed
    // by the boss and they fire lightning (cell damage)
    // If the boss is 100% ammo resist == no infighting
    // The health is reset everytime pain is received
    self.resist_shells = self.resist_nails = 1;
    self.resist_rockets = self.resist_cells = 0.9;
    
    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);
    total_monsters = total_monsters + 1;
    self.use = ftop_wakeup;
};