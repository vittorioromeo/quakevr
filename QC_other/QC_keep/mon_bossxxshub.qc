/*==============================================================================
OLD ONE / Shub-Niggurath (ID software Version)
==============================================================================*/
$frame idle1 idle2 idle3 idle4 idle5 idle6 idle7 idle8
$frame idle9 idle10 idle11 idle12 idle13 idle14 idle15 idle16
$frame idle17 idle18 idle19 idle20 idle21 idle22 idle23 idle24
$frame idle25 idle26 idle27 idle28 idle29 idle30 idle31 idle32
$frame idle33 idle34 idle35 idle36 idle37 idle38 idle39 idle40
$frame idle41 idle42 idle43 idle44 idle45 idle46

$frame shake1 shake2 shake3 shake4 shake5 shake6 shake7 shake8
$frame shake9 shake10 shake11 shake12 shake13 shake14 shake15
$frame shake16 shake17 shake18 shake19 shake20

float SHUB_PHASE1 = 1;  // Waiting for a trigger event
float SHUB_PHASE2 = 2;  // Fighting
float SHUB_PHASE3 = 3;  // Frenzy mode
float SHUB_PHASE4 = 4;    // Death

//======================================================================
// Global functions
//======================================================================
// Special streamlined player find function
//----------------------------------------------------------------------
float() xxshub_FindTarget =
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

    // Finally found something
    self.enemy = client;
    self.goalentity = self.enemy;

    // We have a winner!
    return TRUE;    
};

//----------------------------------------------------------------------
// Setup wave HP and trigger boundaries (one function to be consistent)
//----------------------------------------------------------------------
void() xxshub_WaveSetupHP =
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
float() xxshub_WaveCheck =
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
        xxshub_WaveSetupHP();        // Reset trigger/hp
        self.style = SHUB_PHASE3;    // Frenzy mode
        return TRUE;
    }
    return FALSE;
};

//===========================================================================
// IDLE state - flailing arms (only state really)
//===========================================================================
void() xxshub_idleframe =
{
    // Get the obvious exception(s) done first
    if (self.health < 1) return;
    
    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();
    else if (self.walkframe == 12) monster_idle_sound();
    else if (self.walkframe == 24) monster_idle_sound();
    else if (self.walkframe == 36) monster_idle_sound();
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 45) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = xxshub_idleframe;
    
    // Setup current animation frame
    self.frame = $idle1 + self.walkframe;

    // Check for HP trigger event
    if (xxshub_WaveCheck() == TRUE) self.th_missile();
    
    // Check for combat event
    if (self.style == SHUB_PHASE2) {
        // Check for any no combat conditions
        if (self.enemy.health < 1) self.enemy = world;
        else if (intermission_running > 0) self.enemy = world;
        else if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
        else if (self.enemy.items & IT_INVISIBILITY) self.enemy = world;

        // If no enemy, keep looking
        if (!self.enemy) xxshub_FindTarget();
        // Check for attack 1 - Homing Missile
        if (self.enemy) self.th_melee();
    }    
};

//----------------------------------------------------------------------
void() xxshub_idle ={ self.walkframe = 0; xxshub_idleframe(); };

//============================================================================
// Attack 1 : Fire Shub Homing Missile
//============================================================================
void() xxshub_attack1 =
{
    // Check sightline to player
    if (visblocked(self.enemy)) return;
    if (time < self.attack_finished) return;
    
    SUB_AttackFinished (2 + 2*random());
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "shub/attack1.wav", 1, ATTN_NORM);

    // Skill level adjustment
    self.attack_speed = self.pos1_x + (skill*self.pos1_y);
    // Spawn from front of Shub
    Launch_HomingMissile (self.dest1, '0 0 10', CT_PROJ_SHUB1, self.attack_speed);
};

//============================================================================
// Attack 2 : shake and spam grenades (end of wave event)
//============================================================================
void() xxshub_shakeattack = 
{
    local vector org, dir, ang, avel;
    
    // Custom grenade firing speed
    self.attack_speed = self.pos3_x;

    // Shoot grenades from top of shub
    makevectors(self.angles);
    org = self.origin + attack_vector(self.dest2);
    
    // Randomly shoot grenades in any directions
    ang = '0 0 0';
    ang_y = rint(random()*360);
    makevectors(ang);
    dir = v_forward * self.attack_speed;
    dir_z = self.pos3_y;
    
    // Bit of random spin and fire grenade
    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_SHUB2);
};

//----------------------------------------------------------------------
void() xxshub_shakeframe =
{
    // Randomnly fire grenades outward
    if (random() < 0.5) xxshub_shakeattack();
    
    if (self.walkframe == 0) {
        sound (self, CHAN_WEAPON, "shub/attack2.wav", 1, ATTN_NORM);
        self.effects = self.effects | EF_MUZZLEFLASH;
    }
    
    // Keep on looping around
    self.nextthink = time + 0.1;
    self.think = xxshub_shakeframe;

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 14) {
        self.walkframe = 0;
        self.count = self.count + 1;

        // has shub looped around 3 times?
        if (self.count > 2) {
            // Slight attack pause after the shake
            self.attack_finished = time + 2 + 2*random();
            Resist_ChangeType(self, FALSE);
            self.style = SHUB_PHASE2;
            self.think = self.th_stand;
        }
    }
    
    // Update frame and keep on shaking!
    else self.frame = $shake1 + self.walkframe;    
};

//----------------------------------------------------------------------
void() xxshub_attack2 =
{ 
    Resist_ChangeType(self, TRUE);
    self.walkframe = self.count = 0; 
    xxshub_shakeframe();
};

//============================================================================
// PAIN (ignored, just keeps on firing)
//============================================================================
void(entity inflictor, entity attacker, float damage) xxshub_pain =
{
    // Shub been hit, wakeup and start attacking
    if (self.style == SHUB_PHASE1) {self.use(); return;}
    self.pain_finished = time + 2 + random()*2;

    // Check for boss wave trigger events
    if (xxshub_WaveCheck() == TRUE) {self.th_missile(); return;}

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);
    
    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        // Spawn particle/smoke damage
        SpawnProjectileSmoke(inflictor.origin, 200, 50, 150);
        if (damage > 30) SpawnProjectileSmoke(inflictor.origin, 200, 50, 250);
        // random pick sound
        if (random() < 0.5)
            sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
    }
};

//===========================================================================
// FINAL DEATH - shake it baby and explode
//===========================================================================
void() xxshub_explode =
{
    // No more Boss!
    entity_hide(self);
    
    // Blue ID particle explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 35);
    WriteByte (MSG_BROADCAST, 8);
    
    // Classic sprite/DP explosion
    SpawnExplosion(EXPLODE_BIG, self.origin, "shub/explode_death.wav");

    // Gib explosive fountain!?!
    ThrowGib(11, 10 + rint(random()*10));    // large blobs
    ThrowGib(4, 5 + rint(random()*3));        // Some flesh bits
    ThrowGib(5, 10 + rint(random()*3));
};

//----------------------------------------------------------------------
void() xxshub_deathframe =
{
    // Throw debris out from top of shub
    self.pos3 = vecrand(100,100,TRUE);
    self.pos3_z = 128 + random()*128;
    SpawnProjectileSmoke(self.origin+self.pos3, 200, 50, 150);
    // Random chance of gib + explosion
    if (random() < 0.25)    
        SpawnExplosion(EXPLODE_MED, self.origin+self.pos3, SOUND_REXP3);

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 15 && self.count < 3) {
        self.walkframe = 0;
        self.count = self.count + 1;
    }
    
    if (self.walkframe > 19) xxshub_explode();
    // Update frame and keep on shaking!
    else {
        self.frame = $shake1 + self.walkframe;    
        // Keep on looping around
        self.nextthink = time + 0.1;
        self.think = xxshub_deathframe;
    }
};

//----------------------------------------------------------------------
void() xxshub_die =
{
    self.style = SHUB_PHASE4;        // Game Over for Shub!
    self.deadflag = DEAD_DEAD;        // the rock finally crashed
    self.effects = 0;                // Remove effects on death
    self.solid = SOLID_NOT;            // no longer need to block
    self.max_health = MON_GIBEXPLOSION;

    sound (self, CHAN_VOICE, "shub/death1.wav", 1, ATTN_NORM);
    self.walkframe = self.count = 0;
    xxshub_deathframe();
};

//----------------------------------------------------------------------
// Switch Shub into a fighting stance
//----------------------------------------------------------------------
void() xxshub_wakeup =
{
    // Time to fight!
    self.use = SUB_Null;            // No more triggers
    self.style = SHUB_PHASE2;        // Start fighting

    Resist_ChangeType(self, FALSE);    // Default resistance

    // Wakeup sound
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);
    
    // Restore pain thresholds and timers
    self.pain_flinch = 400;            // Shambler level
    self.pain_timeout = 2;            // Stop constant pain
    
    // Wait for wakeup sound to finish before combat
    self.pain_finished = time + 2 + random()*2;
    self.attack_finished = time + 2;
    
    // Check for player (use activator)
    if (activator.flags & FL_CLIENT) self.enemy = activator;
};    

//----------------------------------------------------------------------
// Setup Shub after trigger event
//----------------------------------------------------------------------
void() xxshub_start =
{
    self.use = SUB_Null;            // Suppress trigger events
    self.style = SHUB_PHASE1;        // Waiting for combat trigger
    self.flags = FL_MONSTER;        // Reset flag (no user settings)
    if (self.spawnflags & MON_SHUB_UPSIDE) self.flags = self.flags | FL_FLY;
    else self.flags = self.flags | FL_ONGROUND;

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    
    self.yaw_speed = 0;                // No turning speed
    self.velocity = '0 0 0';        // Make sure stationary
    self.deadflag = DEAD_NO;        // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.pain_longanim = FALSE;        // No axe advantage
    self.takedamage = DAMAGE_AIM;    // Can receive damage
    self.pain_flinch = 1;            // Pain is a start trigger
    
    self.movespeed = 1;                // Shub does not move!
    self.noinfighting = TRUE;        // No infighting
    self.gibhealth = -1000;            // Special death sequence
    self.pain_finished = self.attack_finished = 0;

    Resist_CheckRange(self);        // Double check values
    Resist_Save(self);                // Save for Later
    // Allow for a damage trigger (spawnflag option only)
    if (!(self.spawnflags & MON_SHUB_DMGTRIG))
        Resist_ChangeType(self, TRUE);

    // Setup boss waves and overall health
    if (self.bosswave < 1) self.bosswavetotal = 5;
    else self.bosswavetotal = self.bosswave;
    
    // Always start bosswave at 1
    self.bosswave = 1;
    if (self.bosswaveqty < 1) self.bosswaveqty = 500;
    self.max_health = self.bosswavetotal * self.bosswaveqty;
    // Setup boss wave HP + trigger event
    xxshub_WaveSetupHP();

    // Restore all think functions
    self.th_stand = xxshub_idle;
    self.th_walk = xxshub_idle;
    self.th_run = xxshub_idle;
    self.th_melee = xxshub_attack1;
    self.th_missile = xxshub_attack2;
    self.th_pain = xxshub_pain;
    self.th_die = xxshub_die;
    
    // Wait for trigger to start attacking
    self.use = xxshub_wakeup;
    self.th_stand();
};

/*============================================================================
 QUAKED monster_shub (1 0 0) (-128 -128 -24) (128 128 512)
============================================================================*/
void() monster_shub =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/oldone.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_SHUB1);        // Large Homing Missile
    precache_model (MODEL_PROJ_SHUB2);        // Large blob of blood
    
    // Final explosionm and grenades for attack 2
    self.gib1mdl = MODEL_PROJ_SHUB2;
    self.gib1frame = 9;
    
    self.idle_sound = "shub/idle1.wav";
    self.idle_sound2 = "shub/idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);

    // Att1 : Homing missile
    // Att2 : Grenade spam
    precache_sound ("shub/attack1.wav");
    precache_sound ("shub/attack2.wav");
    precache_sound ("shub/bounce.wav");
    
    self.pain_sound = "shub/pain1.wav";
    self.pain_sound2 = "shub/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound ("shub/death1.wav");
    precache_sound ("shub/explode_death.wav");
    
    self.sight_sound = "shub/sight1.wav";
    precache_sound (self.sight_sound);
    
    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    // Use to be 128 square, reduced size to help with
    // radius/splash damage being more effective
    self.bbmins = '-96 -96 -24';    // has own entity setup
    self.bbmaxs = '96 96 192';
    self.bboxtype = BBOX_CUSTOM;    // Custom BBox size
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)
    self.health = self.max_health = MEGADEATH;
    self.pain_finished = LARGE_TIMER;
    self.no_liquiddmg = TRUE;        // no slime/lava damage
    self.idlemoreoften = TRUE;        // More frequent idle sounds
    self.gibbed = FALSE;            // Still in one piece

    self.deadflag = DEAD_NO;                    // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.takedamage = DAMAGE_NO;                // Immune to damage

    // Attack 1 - Setup projectile speed (200 + 20*skill)
    if (CheckZeroVector(self.pos1)) self.pos1 = '200 20 0';
    // Attack 1 - Setup projectile damage (Base + Random, Splash)
    if (CheckZeroVector(self.pos2)) self.pos2 = '0 0 40';
    // Attack 2 - Setup projectile speed, splash damage
    if (CheckZeroVector(self.pos3)) self.pos3 = '300 500 40';

    // Attack 1 - Spawn location (offset from origin)
    if (CheckZeroVector(self.dest1)) self.dest1 = '0 0 128';
    // Attack 2 - Spawn location (offset from origin)
    if (CheckZeroVector(self.dest2)) self.dest2 = '0 0 128';

    if (self.spawnflags & MON_SHUB_UPSIDE) {
        self.bbmins = '-96 -96 -192';    // up side down version
        self.bbmaxs = '96 96 24';
    }    
    
    self.classtype = CT_MONXXSHUB;
    self.classgroup = CG_BOSS;
    self.style = 0;
    if (self.deathstring == "")  self.deathstring = " became one with Shub-Niggurath\n";
    // No targetname = no trigger!
    if (self.targetname == "") {
        dprint("\b[SHUB]\b Missing trigger name!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }
    
    // Does not go through monster spawn functions
    total_monsters = total_monsters + 1;
    
    // Default is no spawn delay, wait for trigger
    if (!(self.spawnflags & MON_SPAWN_DELAY)) {
        self.nextthink = time + 0.1 + random()*0.4;
        self.think = xxshub_start;
    }
    else self.use = xxshub_start;
};

/*============================================================================
 QUAKED monster_shubupsd (1 0 0) (-128 -128 -24) (128 128 512)
============================================================================*/
void() monster_shubupsd = 
{
    if (deathmatch) { remove(self); return; }

    // Attack 2 - Setup projectile speed, splash damage
    if (CheckZeroVector(self.pos3)) self.pos3 = '600 0 40';
    self.angles_x = 180;
    self.angles_y = anglemod(self.angles_y + 180);
    self.spawnflags = self.spawnflags | MON_SHUB_UPSIDE;
    if (self.deathstring == "")  self.deathstring = " became one with Shub-Niggurath in the Upside-Down\n";
    monster_shub();
};