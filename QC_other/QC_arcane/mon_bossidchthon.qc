/*==============================================================================
Chthon (ID software Version)
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

entity    le1, le2;
float    lightning_end;

//===========================================================================
void() idchthon_tracking =
{
    // Find a player
    if (!self.enemy) {
        self.enemy = find(world, classname, "player");
        if (!self.enemy) self.enemy = world;
    }
    
    // Check for any no combat conditions
    if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
    else if (self.enemy.items & IT_INVISIBILITY) self.enemy = world;
    else if (intermission_running > 0) self.enemy = world;
    else if (self.enemy.health < 1) self.enemy = world;
    if (self.enemy) self.th_missile();
};

//----------------------------------------------------------------------
void() idchthon_idle1    =[    $walk1, idchthon_idle2 ] {idchthon_tracking();};
void() idchthon_idle2    =[    $walk2, idchthon_idle3 ] {};
void() idchthon_idle3    =[    $walk3, idchthon_idle4 ] {};
void() idchthon_idle4    =[    $walk4, idchthon_idle5 ] {};
void() idchthon_idle5    =[    $walk5, idchthon_idle6 ] {};
void() idchthon_idle6    =[    $walk6, idchthon_idle7 ] {};
void() idchthon_idle7    =[    $walk7, idchthon_idle8 ] {};
void() idchthon_idle8    =[    $walk8, idchthon_idle9 ] {};
void() idchthon_idle9    =[    $walk9, idchthon_idle10 ] {};
void() idchthon_idle10    =[    $walk10, idchthon_idle11 ] {idchthon_tracking();};
void() idchthon_idle11    =[    $walk11, idchthon_idle12 ] {};
void() idchthon_idle12    =[    $walk12, idchthon_idle13 ] {};
void() idchthon_idle13    =[    $walk13, idchthon_idle14 ] {};
void() idchthon_idle14    =[    $walk14, idchthon_idle15 ] {};
void() idchthon_idle15    =[    $walk15, idchthon_idle16 ] {};
void() idchthon_idle16    =[    $walk16, idchthon_idle17 ] {};
void() idchthon_idle17    =[    $walk17, idchthon_idle18 ] {};
void() idchthon_idle18    =[    $walk18, idchthon_idle19 ] {};
void() idchthon_idle19    =[    $walk19, idchthon_idle20 ] {};
void() idchthon_idle20    =[    $walk20, idchthon_idle21 ] {};
void() idchthon_idle21    =[    $walk21, idchthon_idle22 ] {idchthon_tracking();};
void() idchthon_idle22    =[    $walk22, idchthon_idle23 ] {};
void() idchthon_idle23    =[    $walk23, idchthon_idle24 ] {};
void() idchthon_idle24    =[    $walk24, idchthon_idle25 ] {};
void() idchthon_idle25    =[    $walk25, idchthon_idle26 ] {};
void() idchthon_idle26    =[    $walk26, idchthon_idle27 ] {};
void() idchthon_idle27    =[    $walk27, idchthon_idle28 ] {};
void() idchthon_idle28    =[    $walk28, idchthon_idle29 ] {};
void() idchthon_idle29    =[    $walk29, idchthon_idle30 ] {};
void() idchthon_idle30    =[    $walk30, idchthon_idle31 ] {};
void() idchthon_idle31    =[    $walk31, idchthon_idle1 ] {idchthon_tracking();};

//===========================================================================
void(vector orgofs) idchthon_missile =
{
    local vector offang, org, vec, dir, mdest, avel;
    local float projlen;

    // No enemy, player dead or intermission?
    if (!self.enemy || self.enemy.health < 1 || intermission_running > 0) {
        self.th_stand();
        return;
    }
    
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
    Launch_Missile (org, dir, avel, CT_PROJ_LAVA, self.attack_speed);
};

//----------------------------------------------------------------------
void() idchthon_face =
{
    // Find a player
    if (!self.enemy) {
        self.enemy = find(world, classname, "player");
        if (!self.enemy) self.enemy = world;
    }
    
    // Check for any no combat conditions
    if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
    else if (self.enemy.items & IT_INVISIBILITY) self.enemy = world;
    else if (intermission_running > 0) self.enemy = world;
    else if (self.enemy.health < 1) self.enemy = world;
    if (self.enemy) ai_face();
};

//----------------------------------------------------------------------
void() idchthon_missile1    =[    $attack1, idchthon_missile2 ] {idchthon_face();};
void() idchthon_missile2    =[    $attack2, idchthon_missile3 ] {idchthon_face();};
void() idchthon_missile3    =[    $attack3, idchthon_missile4 ] {idchthon_face();};
void() idchthon_missile4    =[    $attack4, idchthon_missile5 ] {idchthon_face();};
void() idchthon_missile5    =[    $attack5, idchthon_missile6 ] {idchthon_face();};
void() idchthon_missile6    =[    $attack6, idchthon_missile7 ] {idchthon_face();};
void() idchthon_missile7    =[    $attack7, idchthon_missile8 ] {idchthon_face();};
void() idchthon_missile8    =[    $attack8, idchthon_missile9 ] {idchthon_face();};
void() idchthon_missile9    =[    $attack9, idchthon_missile10 ] {idchthon_missile('100 100 200');};
void() idchthon_missile10    =[    $attack10, idchthon_missile11 ] {idchthon_face();};
void() idchthon_missile11    =[    $attack11, idchthon_missile12 ] {idchthon_face();};
void() idchthon_missile12    =[    $attack12, idchthon_missile13 ] {idchthon_face();};
void() idchthon_missile13    =[    $attack13, idchthon_missile14 ] {idchthon_face();};
void() idchthon_missile14    =[    $attack14, idchthon_missile15 ] {idchthon_face();};
void() idchthon_missile15    =[    $attack15, idchthon_missile16 ] {idchthon_face();};
void() idchthon_missile16    =[    $attack16, idchthon_missile17 ] {idchthon_face();};
void() idchthon_missile17    =[    $attack17, idchthon_missile18 ] {idchthon_face();};
void() idchthon_missile18    =[    $attack18, idchthon_missile19 ] {idchthon_face();};
void() idchthon_missile19    =[    $attack19, idchthon_missile20 ] {idchthon_face();};
void() idchthon_missile20    =[    $attack20, idchthon_missile21 ] {idchthon_missile('100 -100 200');};
void() idchthon_missile21    =[    $attack21, idchthon_missile22 ] {idchthon_face();};
void() idchthon_missile22    =[    $attack22, idchthon_missile23 ] {idchthon_face();};
void() idchthon_missile23    =[    $attack23, idchthon_missile1 ] {idchthon_face();};

//----------------------------------------------------------------------
void() idchthon_rise1    =[    $rise1, idchthon_rise2 ] {};
void() idchthon_rise2    =[    $rise2, idchthon_rise3 ] {
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);};
void() idchthon_rise3    =[    $rise3, idchthon_rise4 ] {};
void() idchthon_rise4    =[    $rise4, idchthon_rise5 ] {ai_face();};
void() idchthon_rise5    =[    $rise5, idchthon_rise6 ] {};
void() idchthon_rise6    =[    $rise6, idchthon_rise7 ] {ai_face();};
void() idchthon_rise7    =[    $rise7, idchthon_rise8 ] {};
void() idchthon_rise8    =[    $rise8, idchthon_rise9 ] {ai_face();};
void() idchthon_rise9    =[    $rise9, idchthon_rise10 ] {};
void() idchthon_rise10    =[    $rise10, idchthon_rise11 ] {ai_face();};
void() idchthon_rise11    =[    $rise11, idchthon_rise12 ] {};
void() idchthon_rise12    =[    $rise12, idchthon_rise13 ] {ai_face();};
void() idchthon_rise13    =[    $rise13, idchthon_rise14 ] {};
void() idchthon_rise14    =[    $rise14, idchthon_rise15 ] {ai_face();};
void() idchthon_rise15    =[    $rise15, idchthon_rise16 ] {};
void() idchthon_rise16    =[    $rise16, idchthon_rise17 ] {ai_face();};
void() idchthon_rise17    =[    $rise17, idchthon_missile1 ] {};

//----------------------------------------------------------------------
void() idchthon_awake =
{
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.takedamage = DAMAGE_NO;
    self.velocity = '0 0 0';

    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    if (skill == SKILL_EASY) self.health = 1;
    else self.health = 3;
    self.enemy = activator;

    // DP particle spark and smoke effect to Chthon 
    // original idea by Seven, green version added later by me
    if (ext_dppart) self.traileffectnum = particleeffectnum(self.dpp_name);
    
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    sound (self, CHAN_WEAPON, "chthon/lavasplash.wav", 1, ATTN_NORM);
    idchthon_rise1 ();
};

//===========================================================================
void() idchthon_shocka1 =[    $shocka1, idchthon_shocka2 ] {};
void() idchthon_shocka2 =[    $shocka2, idchthon_shocka3 ] {};
void() idchthon_shocka3 =[    $shocka3, idchthon_shocka4 ] {};
void() idchthon_shocka4 =[    $shocka4, idchthon_shocka5 ] {};
void() idchthon_shocka5 =[    $shocka5, idchthon_shocka6 ] {};
void() idchthon_shocka6 =[    $shocka6, idchthon_shocka7 ] {};
void() idchthon_shocka7 =[    $shocka7, idchthon_shocka8 ] {};
void() idchthon_shocka8 =[    $shocka8, idchthon_shocka9 ] {};
void() idchthon_shocka9 =[    $shocka9, idchthon_shocka10 ] {};
void() idchthon_shocka10 =[    $shocka10, idchthon_missile1 ] {};

//----------------------------------------------------------------------
void() idchthon_shockb1 =[    $shockb1, idchthon_shockb2 ] {};
void() idchthon_shockb2 =[    $shockb2, idchthon_shockb3 ] {};
void() idchthon_shockb3 =[    $shockb3, idchthon_shockb4 ] {};
void() idchthon_shockb4 =[    $shockb4, idchthon_shockb5 ] {};
void() idchthon_shockb5 =[    $shockb5, idchthon_shockb6 ] {};
void() idchthon_shockb6 =[    $shockb6, idchthon_shockb7 ] {};
void() idchthon_shockb7 =[    $shockb1, idchthon_shockb8 ] {};
void() idchthon_shockb8 =[    $shockb2, idchthon_shockb9 ] {};
void() idchthon_shockb9 =[    $shockb3, idchthon_shockb10 ] {};
void() idchthon_shockb10 =[    $shockb4, idchthon_missile1 ] {};

//----------------------------------------------------------------------
void() idchthon_shockc1 =[    $shockc1, idchthon_shockc2 ] {};
void() idchthon_shockc2 =[    $shockc2, idchthon_shockc3 ] {};
void() idchthon_shockc3 =[    $shockc3, idchthon_shockc4 ] {};
void() idchthon_shockc4 =[    $shockc4, idchthon_shockc5 ] {};
void() idchthon_shockc5 =[    $shockc5, idchthon_shockc6 ] {};
void() idchthon_shockc6 =[    $shockc6, idchthon_shockc7 ] {};
void() idchthon_shockc7 =[    $shockc7, idchthon_shockc8 ] {};
void() idchthon_shockc8 =[    $shockc8, idchthon_shockc9 ] {};
void() idchthon_shockc9 =[    $shockc9, idchthon_shockc10 ] {};
void() idchthon_shockc10 =[    $shockc10, idchthon_death1 ] {};

//----------------------------------------------------------------------
void() lightning_fire =
{
    local vector p1, p2;
    
    // done here, put the terminals back up
    if (time >= lightning_end) {
        self = le1;
        func_door_go_down ();
        self = le2;
        func_door_go_down ();
        return;
    }
    
    p1 = (le1.mins + le1.maxs) * 0.5;
    p1_z = le1.absmin_z - 16;
    
    p2 = (le2.mins + le2.maxs) * 0.5;
    p2_z = le2.absmin_z - 16;
    
    // compensate for length of bolt
    p2 = p2 - normalize(p2-p1)*100;

    self.nextthink = time + 0.1;
    self.think = lightning_fire;

    WriteByte (MSG_ALL, SVC_TEMPENTITY);
    WriteByte (MSG_ALL, TE_LIGHTNING3);
    WriteEntity (MSG_ALL, world);
    WriteCoord (MSG_ALL, p1_x);
    WriteCoord (MSG_ALL, p1_y);
    WriteCoord (MSG_ALL, p1_z);
    WriteCoord (MSG_ALL, p2_x);
    WriteCoord (MSG_ALL, p2_y);
    WriteCoord (MSG_ALL, p2_z);
};

//----------------------------------------------------------------------
void() lightning_use =
{
    if (lightning_end >= time + 1) return;

    le1 = find( world, target, "lightning");
    le2 = find( le1, target, "lightning");
    if (!le1 || !le2) {
        dprint ("\b[CHTHON]\b missing lightning targets\n");
        return;
    }
    
    // Check for lightning block alignment
    if (
     (le1.state != STATE_TOP && le1.state != STATE_BOTTOM)
    || (le2.state != STATE_TOP && le2.state != STATE_BOTTOM)
    || (le1.state != le2.state) )
    { return; }

    // don't let the electrodes go back up until the bolt is done
    le1.nextthink = -1;
    le2.nextthink = -1;
    lightning_end = time + 1;

    sound (self, CHAN_VOICE, "misc/power.wav", 1, ATTN_NORM);
    lightning_fire ();        

    // advance the boss pain if down
    self = find (world, classname, "monster_boss");
    if (!self) return;
    self.enemy = activator;
    if (le1.state == STATE_TOP && self.health > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        self.health = self.health - 1;
        if (self.health >= 2) idchthon_shocka1();
        else if (self.health == 1) idchthon_shockb1();
        else if (self.health == 0) idchthon_shockc1();
    }    
};

//===========================================================================
void() idchthon_death1 = [$death1, idchthon_death2] {
sound (self, CHAN_VOICE, "chthon/death.wav", 1, ATTN_NORM);};
void() idchthon_death2 = [$death2, idchthon_death3] {};
void() idchthon_death3 = [$death3, idchthon_death4] {};
void() idchthon_death4 = [$death4, idchthon_death5] {};
void() idchthon_death5 = [$death5, idchthon_death6] {};
void() idchthon_death6 = [$death6, idchthon_death7] {};
void() idchthon_death7 = [$death7, idchthon_death8] {};
void() idchthon_death8 = [$death8, idchthon_death9] {};
void() idchthon_death9 = [$death9, idchthon_death10] {
    sound (self, CHAN_BODY, "chthon/lavasplash.wav", 1, ATTN_NORM);
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
};
// No plans to support map hacks anymore, function name changed
void() idchthon_death10 = [$death9, idchthon_death10] {
    killed_monsters = killed_monsters + 1;
    WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
    SUB_UseTargets ();
    entity_hide(self);
};

/*======================================================================
 QUAKED monster_boss (1 0 0) (-128 -128 -24) (128 128 256)
======================================================================*/
void() monster_boss =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_bosschthon.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_LAVA);

    // Attack/throw/electric sound
    precache_sound ("chthon/attack1.wav");
    precache_sound ("weapons/rocket1i.wav");
    precache_sound ("misc/power.wav");
    
    self.pain_sound = "chthon/pain.wav";
    precache_sound (self.pain_sound);
    precache_sound ("chthon/death.wav");

    // Rise from lava roar + splash
    self.sight_sound = "chthon/sight1.wav";
    precache_sound (self.sight_sound);
    precache_sound ("chthon/lavasplash.wav");

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    // Use to be 128 square, reduced size to help with
    // radius/splash damage being more effective
    self.bbmins = '-80 -80 -24';    // has own entity setup
    self.bbmaxs = '80 80 256';
    self.bboxtype = BBOX_CUSTOM;
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)
    self.poisonous = FALSE;            // Cannot be poisonous

    self.health = 3;
    self.takedamage = DAMAGE_NO;    // Cannot be damaged
    self.bouncegrenade = TRUE;        // Grenades bounce off!
    self.dpp_name = "TR_BOSSCHTHON";// Lava smoke and sparks
    self.yaw_speed = 20;            // Fast turner
    self.deathstring = " was blown apart by Chthon\n";
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 1;
    self.resist_rockets = self.resist_cells = 1;

    self.th_stand = idchthon_idle1;
    self.th_walk = idchthon_idle1;
    self.th_run = idchthon_idle1;
    self.th_missile = idchthon_missile1;
    
    self.classtype = CT_MONIDCHTHON;
    self.classgroup = CG_BOSS;
    // Does not go through monster spawn functions
    total_monsters = total_monsters + 1;
    self.use = idchthon_awake;
};

/*======================================================================
/*QUAKED event_lightning (0 1 1) (-16 -16 -16) (16 16 16) x
Just for boss level. Used for killing Cthon.
-------- KEYS --------
-------- SPAWNFLAGS --------
-------- NOTES --------
Just for boss level. Used for killing Cthon.
======================================================================*/
void() event_lightning =
{
    self.use = lightning_use;
};

/*======================================================================
/*QUAKED misc_teleporttrain (0 0.5 0.8) (-8 -8 -8) (8 8 8) x
This is used for the final boss ONLY
Flying ball needed to teleport kill Shub-Niggurath
-------- KEYS --------
-------- SPAWNFLAGS --------
-------- NOTES --------
This is used for the final boss ONLY
Flying ball needed to teleport kill Shub-Niggurath
Originallu used bmodel train functions, setup to work like
the shalrath homing missile

======================================================================*/
void() teleporttrain_next;
void() teleporttrain_target =
{
    local vector dir;

    if (self.target != "") {
        // Find next target entity
        self.enemy = find (world, targetname, self.target);
        if (self.enemy) {
            // Work out distance and angle to turn towards
            dir = normalize(self.enemy.origin - self.origin);
            self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
            self.velocity = dir * self.speed;
    
            // Shift target to next target in the entity chain
            self.target = self.enemy.target;
            self.nextthink = time + 0.1;
            self.think = teleporttrain_next;
            return;
        }
    }
    // No target or entity to follow, full stop
    self.velocity = '0 0 0';
    self.enemy = world;
};

//----------------------------------------------------------------------
void() teleporttrain_next =
{
    if (!self.enemy) return;

    // Work out how close to the next target (self.enemy)        
    self.t_length = vlen(self.enemy.origin - self.origin);
    if (self.t_length < 64) teleporttrain_target();
    else {
        // Keep checking
        self.nextthink = time + 0.1;
        self.think = teleporttrain_next;
    }        
};

//----------------------------------------------------------------------
void() misc_teleporttrain =
{    
    self.mdl = "progs/teleport.mdl";
    precache_model (self.mdl);

    self.solid = SOLID_TRIGGER;
    // Push movetype cannot rotate, fly can
    //    self.movetype = MOVETYPE_PUSH;
    self.movetype = MOVETYPE_FLY;
    self.use = teleporttrain_target;
    self.avelocity = vecrand(50,250,FALSE);

    setmodel (self, self.mdl);
    setsize (self, VEC_ORIGIN , VEC_ORIGIN);

    if (!self.speed) self.speed = 100;
    if (self.target == "") {
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
    }
};
