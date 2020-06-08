/*==============================================================================
Chthon (Custom Version)
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

float CHTHON_PHASE1 = 1;    // Rising from Lava
float CHTHON_PHASE2 = 2;    // Fighting
float CHTHON_PHASE3 = 3;    // Frenzy mode
float CHTHON_PHASE4 = 4;    // Death

//======================================================================
// Global functions
//======================================================================
// Special streamlined player find function
//----------------------------------------------------------------------
float() xxchthon_FindTarget =
{
    local entity client;
    
    // Get the obvious exception(s) done first
    if (self.health < 1) return FALSE;
    if (intermission_running) return FALSE;

    // Find a client in current PVS
    client = checkclient ();
    
    // Go through all the exception(s)
    if (!client) return FALSE;
    if (!(client.flags & FL_CLIENT)) return FALSE;
    if (client.flags & FL_NOTARGET) return FALSE;
    if (client.items & IT_INVISIBILITY) return FALSE;
    
    // Check range and visibility of player
    enemy_vis = visible(client);
    if (!enemy_vis) return FALSE;
    if (!infront(client)) return FALSE;

    // Finally found something
    self.enemy = client;
    self.oldorigin = self.origin;        // Save origin
    self.goalentity = self.enemy;        // Focus on enemy
    // Setup turning angle towards new enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);

    // We have a winner!
    return TRUE;    
};

//----------------------------------------------------------------------
// Setup wave HP and trigger boundaries
//----------------------------------------------------------------------
void() xxchthon_WaveSetupHP =
{
    // Is there anymore boss waves left?
    if (self.bosswave >= self.bosswavetotal) {
        // Only one wave left (death is final trigger)
        self.health = self.bosswaveqty;
        self.bosswavetrig = -1000;
    }
    else {
        // Multiple waves are still left (reset hp+trigger)
        // Always reset HP to stop high DPS weapons trashing waves boundaries
        self.health = ((self.bosswavetotal - self.bosswave) + 1) * self.bosswaveqty;
        // The wave trigger is always one wave lower
        self.bosswavetrig = self.health - self.bosswaveqty;
    }
    // Debug messages for wave and health
    dprint("\b[BOSS]\b Wave ("); dprint(ftos(self.bosswave));
    dprint(" / "); dprint(ftos(self.bosswavetotal));
    dprint(") HP ("); dprint(ftos(self.health));
    dprint(") Trig ("); dprint(ftos(self.bosswavetrig));
    dprint(")\n");
};

//----------------------------------------------------------------------
// Check if HP has reached next boss wave trigger event
//----------------------------------------------------------------------
float() xxchthon_WaveCheck =
{
    // Check for boss wave boundary event
    if (self.health > 1 && self.health < self.bosswavetrig) {
        // Check for wave boundary triggers
        self.noise = "";
        if (self.bosswave == 1) self.noise = self.noise1;
        else if(self.bosswave == 2) self.noise = self.noise2;
        else if(self.bosswave == 3) self.noise = self.noise3;
        else if(self.bosswave == 4) self.noise = self.noise4;
        
        // Is there any trigger for the wave boundary?
        if (self.noise != "") trigger_strs(self.noise, self);

        // Update Boss wave parameters (next wave!)
        self.bosswave = self.bosswave + 1;
        xxchthon_WaveSetupHP();
        self.style = CHTHON_PHASE3;    // Frenzy mode
        return TRUE;
    }
    return FALSE;
};

//===========================================================================
// IDLE state - waiting for player
//===========================================================================
void() xxchthon_idleframe =
{
    // Get the obvious exception(s) done first
    if (self.health < 1) return;
    
    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 30) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = xxchthon_idleframe;
    
    // Setup current animation frame
    self.frame = $walk1 + self.walkframe;

    // Check for HP trigger event
    if (xxchthon_WaveCheck() == TRUE) self.th_jump();
    // Keep checking for players
    else if (xxchthon_FindTarget()) self.th_missile();
};

//----------------------------------------------------------------------
void() xxchthon_idle ={ self.walkframe = 0; xxchthon_idleframe(); };

//===========================================================================
// ATTACK 1 - Constant fireballs
//===========================================================================
void(vector orgofs) xxchthon_missile =
{
    local vector offang, org, vec, dir, mdest, avel;
    local float projlen;

    // No enemy or player dead?
    if (!self.enemy) return;
    
    sound (self, CHAN_WEAPON, "chthon/attack1.wav", 1, ATTN_NORM);
    offang = vectoangles (self.enemy.origin - self.origin);    
    makevectors (offang);
    org = self.origin + attack_vector(orgofs);
    
    // Skill level adjustment
    self.attack_speed = self.pos1_x + (skill*self.pos1_y);
    
    // Lead the missile on hard mode (This formula is not perfect) 
    // There are plenty of missiles that go in strange directions, 
    // especially if the player strafes a lot from side to side.
    if (skill > SKILL_HARD) {
        projlen = vlen(self.enemy.origin - org) / self.attack_speed;
        vec = self.enemy.velocity;
        vec_z = 0;
        mdest = self.enemy.origin + projlen * vec;        
    }
    else mdest = self.enemy.origin;
    
    dir = normalize (mdest - org);
    avel = vecrand(100,200,FALSE);
    Launch_Missile (org, dir, avel, CT_PROJ_CHTHON, self.attack_speed);
};

//----------------------------------------------------------------------
void() xxchthon_attackframe =
{
    // Get the obvious exception(s) done first
    if (self.health < 1) return;
    
    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();
    else if (self.walkframe == 7) xxchthon_missile('100 100 200');
    else if (self.walkframe == 18) xxchthon_missile('100 -100 200');

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 22) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = xxchthon_attackframe;
    
    // Setup current animation frame
    self.frame = $attack1 + self.walkframe;

    // Check for HP trigger event
    if (xxchthon_WaveCheck() == TRUE) self.th_jump();
    // Keep checking for players
    else {
        // Keep checking for player and turning towards them
        // Check for any no combat conditions
        if (self.enemy.health < 1) self.enemy = world;
        else if (intermission_running > 0) self.enemy = world;
        else if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
        else if (self.enemy.items & IT_INVISIBILITY) self.enemy = world;
        
        // No enemy? back to idle, else keep turning
        if (!self.enemy) self.th_stand();
        else ai_face();
    }
};

//----------------------------------------------------------------------
void() xxchthon_attack ={ self.walkframe = 0; xxchthon_attackframe(); };

//============================================================================
// Re-using the original shock animations for end of wave event
//============================================================================
void() xxchthon_shakeattack =
{
    local vector org, ang, dir, avel;
    
    // Custom grenade firing speed
    self.attack_speed = self.pos3_x;

    // Shoot grenades from top of shub
    makevectors(self.angles);
    org = self.origin + attack_vector(self.dest2);

    // Randomly shoot grenades in any directions
    ang = '0 0 0';
    ang_y = rint(random()*360);
    makevectors(ang);
    // Use classic grenade angle system, not Z aware
    dir = v_forward * self.attack_speed;
    dir_z = self.pos3_y;
    
    // A bit of random spin and fire!
    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_CHTHON2);
};

//----------------------------------------------------------------------
void() xxchthon_shakeframe =
{
    // Randomnly fire grenades outward
    if (random() < 0.75) xxchthon_shakeattack();
    
    // Keep on looping around
    self.nextthink = time + 0.1;
    self.think = xxchthon_shakeframe;
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 9) {
        self.walkframe = 0;
        self.count = self.count + 1;
        self.inpain = self.inpain + 1;
        
        // has chthon looped around 3 times?
        if (self.count > 1) {
            Resist_ChangeType(self, FALSE);
            self.style = CHTHON_PHASE2;
            // Straight back to throwing fireballs!
            self.think = self.th_missile;
        }
    }

    // Update current frames (2 sets of animations)
    if (self.inpain == 0) self.frame = $shocka1 + self.walkframe;
    else self.frame = $shockc1 + self.walkframe;
};

//----------------------------------------------------------------------
void() xxchthon_shake =
{
    // Start roaring and shaking
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);
    Resist_ChangeType(self, TRUE);
    self.walkframe = self.count = self.inpain = 0;
    xxchthon_shakeframe();
};

//============================================================================
// PAIN and DEATH
//============================================================================
void(entity inflictor, entity attacker, float damage) xxchthon_pain =
{

    // Check for boss wave trigger events
    if (xxchthon_WaveCheck() == TRUE) {self.th_jump(); return;}
    self.pain_finished = time + 2 + random()*2;

    // Spawn particle/smoke damage
    SpawnProjectileSmoke(inflictor.origin, 200, 50, 150);
    if (damage > 30) SpawnProjectileSmoke(inflictor.origin, 200, 50, 250);
    // The pain sound not linked to animation, so less useful
    if (random() < 0.2 && damage > 30)
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
};

//============================================================================
// DEATH - Fall down and explode
//============================================================================
void() xxchthon_lavasplash =
{
    sound (self, CHAN_BODY, "chthon/lavasplash.wav", 1, ATTN_NORM);
    if (ext_dppart) {
        // Two different types of liquid splash (only DP)
        if (self.spawnflags & MON_CHTHON_GREEN) 
            pointparticles(particleeffectnum("TE_SLIMESPLASH"), self.origin, '0 0 0', 1);
        else
            pointparticles(particleeffectnum("TE_LAVASPLASH"), self.origin, '0 0 0', 1);
    }
    else {
        // Classic Fitz/QS engine effects (hard coded)
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }
};

//----------------------------------------------------------------------
void() xxchthon_explode = 
{
    // No more Boss!
    entity_hide(self);

    // Gib explosive fountain!?!
    self.max_health = MON_GIBEXPLOSION;
    ThrowGib(11, 15 + rint(random()*10));    // lava explosions
};

//----------------------------------------------------------------------
void() xxchthon_explosion =
{
    // Pick random location above/center of chthin
    self.oldorigin = self.origin + vecrand(0,100,TRUE);
    self.oldorigin_z = self.origin_z + 50 + random()*100;

    // Spawn smoke, explosion and lava gib!
    SpawnProjectileSmoke(self.oldorigin, 100, 400, 100);
    if (random() < 0.5) self.lip = EXPLODE_MED;
    else self.lip = EXPLODE_BIG;
    SpawnExplosion(self.lip, self.oldorigin, "");
    ThrowGib(11, 1);
};

//----------------------------------------------------------------------
void() xxchthon_death1 = [$death1, xxchthon_death2] {};
void() xxchthon_death2 = [$death2, xxchthon_death3] {xxchthon_explosion();};
void() xxchthon_death3 = [$death3, xxchthon_death4] {xxchthon_explosion();};
void() xxchthon_death4 = [$death4, xxchthon_death5] {xxchthon_explosion();};
void() xxchthon_death5 = [$death5, xxchthon_death6] {xxchthon_explosion();};
void() xxchthon_death6 = [$death6, xxchthon_death7] {xxchthon_explosion();};
void() xxchthon_death7 = [$death7, xxchthon_death8] {xxchthon_explosion();};
void() xxchthon_death8 = [$death8, xxchthon_death9] {xxchthon_explosion();};
void() xxchthon_death9 = [$death9, xxchthon_explode] {xxchthon_lavasplash();};

//----------------------------------------------------------------------
void() xxchthon_die =
{
    self.style = CHTHON_PHASE4;        // Game Over for Chthon!
    self.deadflag = DEAD_DEAD;        // the rock finally crashed
    self.effects = 0;                // Remove effects on death
    self.solid = SOLID_NOT;            // no longer need to block
    self.max_health = MON_GIBEXPLOSION;

    sound (self, CHAN_VOICE, "chthon/death1.wav", 1, ATTN_NORM);
    xxchthon_death1 ();
};

//============================================================================
// RISE up from the swirling liquid
//============================================================================
void() xxchthon_rise1    =[    $rise1, xxchthon_rise2 ] {xxchthon_lavasplash();};
void() xxchthon_rise2    =[    $rise2, xxchthon_rise3 ] {
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);};
void() xxchthon_rise3    =[    $rise3, xxchthon_rise4 ] {};
void() xxchthon_rise4    =[    $rise4, xxchthon_rise5 ] {ai_face();};
void() xxchthon_rise5    =[    $rise5, xxchthon_rise6 ] {};
void() xxchthon_rise6    =[    $rise6, xxchthon_rise7 ] {ai_face();};
void() xxchthon_rise7    =[    $rise7, xxchthon_rise8 ] {};
void() xxchthon_rise8    =[    $rise8, xxchthon_rise9 ] {ai_face();};
void() xxchthon_rise9    =[    $rise9, xxchthon_rise10 ] {};
void() xxchthon_rise10    =[    $rise10, xxchthon_rise11 ] {ai_face();};
void() xxchthon_rise11    =[    $rise11, xxchthon_rise12 ] {};
void() xxchthon_rise12    =[    $rise12, xxchthon_rise13 ] {ai_face();};
void() xxchthon_rise13    =[    $rise13, xxchthon_rise14 ] {};
void() xxchthon_rise14    =[    $rise14, xxchthon_rise15 ] {ai_face();};
void() xxchthon_rise15    =[    $rise15, xxchthon_rise16 ] {};
void() xxchthon_rise16    =[    $rise16, xxchthon_rise17 ] {ai_face();};
void() xxchthon_rise17    =[    $rise17, xxchthon_attack ] {
    self.style = CHTHON_PHASE2;        // Start fighting
    self.takedamage = DAMAGE_AIM;
};

//======================================================================
// Setup Chthon after trigger event
//======================================================================
void() xxchthon_awake =
{
    self.use = SUB_Null;            // No more triggers
    self.style = CHTHON_PHASE1;        // Rising from Lava
    self.flags = FL_MONSTER;        // Reset flag (no user settings)

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    self.takedamage = DAMAGE_NO;    // Still immune to damage
    self.yaw_speed = 20;            // Average Speed
    self.velocity = '0 0 0';        // Make sure stationary
    self.pain_longanim = FALSE;        // No axe advantage
    self.noinfighting = TRUE;        // No infighting

    Resist_CheckRange(self);        // Double check values
    Resist_Save(self);                // Save for Later
        
    // Setup boss waves and overall health
    if (self.bosswave < 1) self.bosswavetotal = 5;
    else self.bosswavetotal = self.bosswave;
    
    // Always start bosswave at 1
    self.bosswave = 1;
    if (self.bosswaveqty < 1) self.bosswaveqty = 500;
    self.max_health = self.bosswavetotal * self.bosswaveqty;
    // Setup boss wave HP + trigger event
    xxchthon_WaveSetupHP();

    // Restore all think functions
    self.th_stand = xxchthon_idle;
    self.th_walk = xxchthon_idle;
    self.th_run = xxchthon_idle;
    self.th_missile = xxchthon_attack;
    self.th_jump = xxchthon_shake;
    self.th_pain = xxchthon_pain;
    self.th_die = xxchthon_die;
    
    self.pain_finished = time + 3;        // Make pain go away
    self.attack_finished = time + 2;    // Reset attack system
    self.gibhealth = -1000;                // Special death sequence
    self.enemy = activator;

    // DP particle spark and smoke effect to Chthon 
    // original idea by Seven, green version added later by me
    if (ext_dppart) self.traileffectnum = particleeffectnum(self.dpp_name);

    // Time to rise from lava
    xxchthon_rise1 ();
};

/*======================================================================
 QUAKED monster_chthon (1 0 0) (-128 -128 -24) (128 128 256)
======================================================================*/
void() monster_chthon =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_bosschthon.mdl";
    precache_model (self.mdl);
    precache_model(MODEL_PROJ_CHTHON1);
    
    // Final explosion (10 distorted balls of lava/slime)
    self.gib1mdl = MODEL_PROJ_CHTHON1;
    self.gib1frame = 9;
    
    // Load up slime version and switch gibs
    if (self.spawnflags & MON_CHTHON_GREEN) {
        self.skin = self.gib1skin = 2;
        precache_model (MODEL_PROJ_SLIME);
        self.dpp_name = "TR_BOSSCHTHONGRN";
        self.gibtype = GIBTYPE_POISON;
        self.poisonous = TRUE;
        if (self.deathstring == "") self.deathstring = " was blown apart by Poison Chthon\n";
    }
    else {
        // Check for new red/fire skin
        if (self.spawnflags & MON_CHTHON_RED) {
            self.skin = self.gib1skin = 1;
            if (self.deathstring == "") self.deathstring = " was blown apart by Blood Chthon\n";
        } else {
            if (self.deathstring == "") self.deathstring = " was blown apart by Chthon\n";
        }
        precache_model (MODEL_PROJ_LAVA);
        self.dpp_name = "TR_BOSSCHTHON";
    }

    self.idle_sound = "chthon/idle1.wav";
    precache_sound (self.idle_sound);

    // Attack/throw/electric sound
    precache_sound ("chthon/attack1.wav");
    precache_sound ("weapons/rocket1i.wav");
    precache_sound ("chthon/bounce.wav");
    
    self.pain_sound = "chthon/pain1.wav";
    precache_sound (self.pain_sound);
    precache_sound ("chthon/death1.wav");

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
    
    self.health = self.max_health = MEGADEATH;
    self.pain_finished = LARGE_TIMER;
    self.no_liquiddmg = TRUE;        // no slime/lava damage
    self.gibbed = FALSE;            // Still in one piece

    self.pain_flinch = 400;            // Shambler level
    self.pain_timeout = 2;            // Stop constant pain

    self.deadflag = DEAD_NO;                    // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.takedamage = DAMAGE_NO;    // Immune to damage
    if (self.deathstring == "")  self.deathstring = " was blown apart by Chthon\n";
    
    // Setup projectile speed (250 + 50*skill)
    if (CheckZeroVector(self.pos1)) self.pos1 = '250 50 0';
    // Setup projectile damage (Base + Random, Splash)
    if (CheckZeroVector(self.pos2)) self.pos2 = DAMAGE_RLPLAYER;
    // Attack 2 - Setup projectile speed, splash damage
    if (CheckZeroVector(self.pos3)) self.pos3 = '300 500 40';
    // Attack 2 - Spawn location (offset from origin)
    if (CheckZeroVector(self.dest2)) self.dest2 = '0 0 96';
    
    self.classtype = CT_MONXXCHTHON;
    self.classgroup = CG_BOSS;
    self.style = 0;
    
    // No targetname = no trigger!
    if (self.targetname == "") {
        dprint("\b[CHTHON]\b Missing trigger name!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);
    // Does not go through monster spawn functions
    total_monsters = total_monsters + 1;
    self.use = xxchthon_awake;
};
