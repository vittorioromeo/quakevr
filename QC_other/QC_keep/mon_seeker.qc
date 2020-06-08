/*==============================================================================
Seeker (Heavily inspired by Hunter from RRP - ijed)
==============================================================================*/
// Slow moving run/walk animation
$frame run1 run2 run3 run4 run5 run6 run7 run8
$frame run9 run10 run11 run12 run13 run14 run15 run16 run17 run18 run19

// stationary fire range attack (Not used)
$frame fire1 fire2 fire3 fire4 fire5 fire6 fire7 fire8
$frame fire9 fire10 fire11 fire12 fire13 fire14 fire15 fire16
$frame fire17 fire18 fire19 fire20 fire21 fire22 fire23 fire24
$frame fire25 fire26 fire27 fire28 fire29 fire30

// right arm punch
$frame punch1 punch2 punch3 punch4 punch5 punch6 punch7 punch8
$frame punch9 punch10

// fall backwards and die
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// Stationary classic idle
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15 stand16
$frame stand17 stand18 stand19

// Run with LEFT arm missing
$frame runL1 runL2 runL3 runL4 runL5 runL6 runL7 runL8
$frame runL9 runL10 runL11 runL12 runL13 runL14 runL15 runL16
$frame runL17 runL18 runL19

// Run with RIGHT arm missing
$frame runR1 runR2 runR3 runR4 runR5 runR6 runR7 runR8
$frame runR9 runR10 runR11 runR12 runR13 runR14 runR15 runR16
$frame runR17 runR18 runR19

// Attack with LEFT arm missing
$frame fireL1 fireL2 fireL3 fireL4 fireL5 fireL6 fireL7 fireL8
$frame fireL9 fireL10 fireL11

// Attack with RIGHT arm missing
$frame fireR1 fireR2 fireR3 fireR4 fireR5 fireR6 fireR7 fireR8
$frame fireR9 fireR10 fireR11

// Attack with BOTH arms active (NEW)
$frame fireB1 fireB2 fireB3 fireB4 fireB5 fireB6 fireB7 fireB8
$frame fireB9 fireB10 fireB11

// Death with LEFT arm missing
$frame deathL1 deathL2 deathL3 deathL4 deathL5 deathL6 deathL7 deathL8
$frame deathL9 deathL10 deathL11 deathL12

// Death with RIGHT arm missing
$frame deathR1 deathR2 deathR3 deathR4 deathR5 deathR6 deathR7 deathR8
$frame deathR9 deathR10 deathR11 deathR12

// Death with BOTH arm missing (special event)
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8
$frame deathB9 deathB10 deathB11 deathB12

// Death with HEAD missing (special event)
$frame deathH1 deathH2 deathH3 deathH4 deathH5 deathH6 deathH7 deathH8
$frame deathH9 deathH10 deathH11 deathH12

// Idle with LEFT arm missing
$frame standL1 standL2 standL3 standL4 standL5 standL6 standL7 standL8
$frame standL9 standL10 standL11 standL12 standL13 standL14 standL15 standL16
$frame standL17 standL18 standL19

// Idle with RIGHT arm missing
$frame standR1 standR2 standR3 standR4 standR5 standR6 standR7 standR8
$frame standR9 standR10 standR11 standR12 standR13 standR14 standR15 standR16
$frame standR17 standR18 standR19

float SEEKER_ARMS_BOTH = 0;
float SEEKER_ARMS_LEFT = 1;
float SEEKER_ARMS_RIGHT = 2;
float SEEKER_ARMSLESS = 3;
float SEEKER_HEADLESS = 4;

float SEEKER_IDLE = 0;
float SEEKER_WALK = 1;
float SEEKER_FIRE = 2;
float SEEKER_DIE = 3;

//============================================================================
// Spark effect from missing arm joint
// Low chance out of combat (idle state)
//============================================================================
void()    seek_sparks_fade1 = [0, seek_sparks_fade2] {self.alpha = 0.8; self.nextthink = time + 0.05;};
void()    seek_sparks_fade2 = [0, seek_sparks_fade3] {self.alpha = 0.6; self.nextthink = time + 0.05;};
void()    seek_sparks_fade3 = [0, seek_sparks_fade4] {self.alpha = 0.4; self.nextthink = time + 0.05;};
void()    seek_sparks_fade4 = [0, SUB_Remove] {self.alpha = 0.2; self.nextthink = time + 0.05;};

void() seek_spark =
{
    local float loopvar;
    local entity spark;
    
    if (self.state == SEEKER_ARMS_BOTH) return;
    // Less chance of sparks when not in combat
    if (random() > 0.1 && !self.enemy) return;
    if (random() > 0.4 && self.enemy) return;
    
    // Light flash and sparky sound
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_BODY, self.noise3, 1, ATTN_STATIC);
    makevectors (self.angles);
    
    // Change origin and spark direction for each arm
    if (self.state == SEEKER_ARMS_LEFT) {
        self.pos1 = self.origin + attack_vector('-4 -30 48');
        self.pos2 = -v_right*25;
    }
    else {
        self.pos1 = self.origin + attack_vector('-4 30 48');
        self.pos2 = v_right*25;
    }

    // Work out how many sparks to spawn
    loopvar = rint(5 + random()*5);
    while (loopvar > 0) {
        spark = spawn();
        spark.classtype = CT_TEMPSPARK;
        spark.classgroup = CG_TEMPENT;
        spark.owner = self;
        spark.movetype = MOVETYPE_BOUNCE;
        spark.solid = SOLID_TRIGGER;
        setmodel (spark, self.noise2);
        setorigin (spark, self.pos1);
        setsize (spark, VEC_ORIGIN, VEC_ORIGIN);
        spark.skin = rint(random()*3);
        spark.gravity = 0.3;
        spark.velocity = vecrand(0,25,TRUE);
        spark.velocity = spark.velocity + self.pos2;
        spark.avelocity = '300 300 300';
        spark.nextthink = time + 0.5 + 1.5*random();
        spark.think = seek_sparks_fade1;
        
        // If DP engine active remove particle shadow
        if (engine == ENG_DPEXT) spark.effects = spark.effects + EF_NOSHADOW;

        // Keep looping for more sparks
        loopvar = loopvar - 1;
    }
};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() seek_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
    }
};

//----------------------------------------------------------------------
void() seek_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() seek_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
}

//----------------------------------------------------------------------
void() seek_setup_attachment = 
{
    // Check if attachment has been setup yet
    if (!self.attachment) seek_create_attachment();
    self.attachment.state = STATE_ON;
    setmodel(self.attachment, self.weaponglow);
    self.attachment.skin = 0;
}

//----------------------------------------------------------------------
void() seek_update_attachment =
{
    setorigin(self.attachment, self.origin);
    self.attachment.angles = self.angles;
    self.attachment.frame = self.frame;
    self.attachment.alpha = 0.3 + random()*0.7;
};

//============================================================================
void() seek_standframe =
{
    // type of animation state
    self.attack_timer = SEEKER_IDLE;

    // Check for arm status
    if (self.state == SEEKER_ARMS_LEFT) self.frame = $standL1 + self.walkframe;
    else if (self.state == SEEKER_ARMS_RIGHT) self.frame = $standR1 + self.walkframe;
    else self.frame = $stand1 + self.walkframe;

    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();
    else if (self.walkframe == 4) seek_spark();
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 18) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = seek_standframe;

    if (self.spawnflags & MON_SEEK_SHIELD) seek_update_attachment();
    else ai_stand();    
};

//----------------------------------------------------------------------
void() seek_stand = { 
    self.walkframe = 0; 
    if (self.spawnflags & MON_SEEK_SHIELD) seek_setup_attachment(); 
    seek_standframe();
};

//============================================================================
void() seek_walkframe =
{
    // If dead, no more updates
    if (self.health < 1) return;

    // type of animation state
    self.attack_timer = SEEKER_WALK;

    // Check for arm status
    if (self.state == SEEKER_ARMS_LEFT) self.frame = $runL1 + self.walkframe;
    else if (self.state == SEEKER_ARMS_RIGHT) self.frame = $runR1 + self.walkframe;
    else self.frame = $run1 + self.walkframe;
    
    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();
    if (self.walkframe == 2 || self.walkframe == 12) monster_footstep(FALSE);
    if (self.walkframe == 4) seek_spark();
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 18) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = seek_walkframe;

    if (self.walkframe == 1) self.distance = 0;
    else if (self.walkframe == 2) self.distance = 5;
    else if (self.walkframe == 3) self.distance = 5;
    else if (self.walkframe == 4) self.distance = 15;
    else if (self.walkframe == 5) self.distance = 5;
    
    else if (self.walkframe == 6) self.distance = 7;
    else if (self.walkframe == 7) self.distance = 12;
    else if (self.walkframe == 8) self.distance = 7;
    else if (self.walkframe == 9) self.distance = 5;
    else if (self.walkframe == 10) self.distance = 3;
    
    else if (self.walkframe == 11) self.distance = 0;
    else if (self.walkframe == 12) self.distance = 5;
    else if (self.walkframe == 13) self.distance = 5;
    else if (self.walkframe == 14) self.distance = 15;
    else if (self.walkframe == 15) self.distance = 3;
    
    else if (self.walkframe == 16) self.distance = 7;
    else if (self.walkframe == 17) self.distance = 12;
    else if (self.walkframe == 18) self.distance = 7;
    else if (self.walkframe == 0) self.distance = 8;
    
    if (self.lefty == FALSE) ai_walk(self.distance);
    else ai_run(self.distance*2);
};

//----------------------------------------------------------------------
void() seek_walk = { self.walkframe = 0; self.lefty = FALSE; seek_walkframe(); };
void() seek_run = { self.walkframe = 0; self.lefty = TRUE; seek_walkframe(); };

//============================================================================
// Melee attack
//============================================================================
void() seek_punch =
{
    local float ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    ai_face ();                        // Turn towards enemy target
    ai_damagebreakable(50);            // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESEEKER)) return;// Too far away

    // Melee hit sound
    sound (self, CHAN_WEAPON, "seeker/punch1.wav", 1, ATTN_NORM);
    ldmg = (random() + random() + random()) * 20;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    SpawnMeatSpray (self, self.enemy, -150);
    SpawnMeatSpray (self, self.enemy, -150);
};

//----------------------------------------------------------------------
void() seek_melee1 = [ $punch1, seek_melee2 ] {ai_face();
    self.pain_finished = time + 1;    // Block pain animation
    sound (self, CHAN_VOICE, "seeker/melee1.wav", 1, ATTN_NORM);};
void() seek_melee2 = [ $punch2, seek_melee3 ] {ai_charge(15);};
void() seek_melee3 = [ $punch3, seek_melee4 ] {ai_charge(20);};
void() seek_melee4 = [ $punch4, seek_melee5 ] {ai_charge(25);};
void() seek_melee5 = [ $punch5, seek_melee6 ] {ai_charge(30);};
void() seek_melee6 = [ $punch6, seek_melee7 ] {ai_charge(30);};
void() seek_melee7 = [ $punch7, seek_melee8 ] {ai_charge(30);seek_punch();};
void() seek_melee8 = [ $punch8, seek_melee9 ] {};
void() seek_melee9 = [ $punch9, seek_melee10 ] {};
void() seek_melee10 = [ $punch10, seek_run ] {};

//============================================================================
// Range attack - 3 rockets from each arm
//============================================================================
void(vector leftofs, vector rightofs) seek_attack =
{
    local vector org, dir, vec;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    // Setup different speeds for projectiles (rockets/laz0rs)
    if (self.weaponstate == TRUE)
        self.attack_speed = SPEED_SEEKLAZ + (skill * SPEED_SEEKLAZSKILL);
    else
        self.attack_speed = SPEED_SEEKROCK + (skill * SPEED_SEEKROCKSKILL);
    
    // turn and face your enemy!
    ai_face();
    makevectors (self.angles);

    // Does the LEFT arm exist?
    if (self.state == SEEKER_ARMS_BOTH || self.state == SEEKER_ARMS_LEFT) { 
        // Fire left arm rocket
        org = self.origin + attack_vector(leftofs);
        // Fire rocket slightly to the LEFT of player
        vec = v_right*(10+random()*10);
        dir = normalize((self.enemy.origin + vec) - org);
        // Double check nothing (world/monsters) is blocking the rocket
        if (visxray(self.enemy, leftofs, '0 0 0', FALSE)) {
            if (self.weaponstate == TRUE) {
                if (skill > SKILL_NORMAL) {
                    sound (self, CHAN_WEAPON, SOUND_PLASMA_FIRE, 1, ATTN_NORM);
                    launch_plasma(org, dir, CT_PROJ_PLASMA, self.attack_speed);
                }
                else {
                    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);    
                    launch_projectile(org, dir, CT_PROJ_LASER, self.attack_speed);
                }
            }
            else {
                sound (self, CHAN_WEAPON, "seeker/rocket_fire.wav", 1, ATTN_NORM);    
                Launch_Missile (org, dir, '0 0 0', CT_PROJ_SEEKER, self.attack_speed);
            }
        }
    }

    // Does the RIGHT arm exist?
    if (self.state == SEEKER_ARMS_BOTH || self.state == SEEKER_ARMS_RIGHT) { 
        org = self.origin + attack_vector(rightofs);
        // Fire rocket slightly to the RIGHT of player
        vec = v_right*(10+random()*10);
        dir = normalize((self.enemy.origin - vec) - org);
        // Double check nothing (world/monsters) is blocking the rocket
        if (visxray(self.enemy, rightofs, '0 0 0', FALSE)) {
            if (self.weaponstate == TRUE) {
                if (skill > SKILL_NORMAL) {
                    sound (self, CHAN_WEAPON, SOUND_PLASMA_FIRE, 1, ATTN_NORM);
                    launch_plasma(org, dir, CT_PROJ_PLASMA, self.attack_speed);
                }
                else {
                    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);    
                    launch_projectile(org, dir, CT_PROJ_LASER, self.attack_speed);
                }
            }
            else {
                sound (self, CHAN_WEAPON, "seeker/rocket_fire.wav", 1, ATTN_NORM);    
                Launch_Missile (org, dir, '0 0 0', CT_PROJ_SEEKER, self.attack_speed);
            }
        }
    }
};

//----------------------------------------------------------------------
void() seek_fireframe =
{
    // If dead, no more updates
    if (self.health < 1) return;

    // type of animation state
    self.attack_timer = SEEKER_FIRE;

    // Check for arm status
    if (self.state == SEEKER_ARMS_LEFT) self.frame = $fireL1 + self.walkframe;
    else if (self.state == SEEKER_ARMS_RIGHT) self.frame = $fireR1 + self.walkframe;
    else self.frame = $fireB1 + self.walkframe;

    // Animation trigger events
    if (self.walkframe == 0) {
        // Work out which attack to do? lasers or rockets
        self.enemydist = range_distance(self.enemy, TRUE);
        if (self.enemydist < MONAI_RANGESWSEEKER) {
            // Upgrade lasers to plasma with hard skill+
            if (skill > SKILL_NORMAL) 
                sound (self, CHAN_VOICE, "seeker/range_plasma.wav", 1, ATTN_NORM);
            // Lasers at close range ONLY
            else sound (self, CHAN_VOICE, "seeker/range_laser.wav", 1, ATTN_NORM);
            self.weaponstate = TRUE;
        }
        else {
            sound (self, CHAN_VOICE, "seeker/range_rocket.wav", 1, ATTN_NORM);
            self.weaponstate = FALSE;
        }
    }
    else if (self.walkframe == 5) seek_attack('40 33 30','25 -23 25');
    else if (self.walkframe == 7) seek_attack('33 30 28','23 -30 25');
    else if (self.walkframe == 9) seek_attack('33 36 27','35 -26 26');
    else ai_face();

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    self.nextthink = time + 0.1;
    if (self.walkframe > 10) self.think = seek_run;
    else self.think = seek_fireframe;
};

//----------------------------------------------------------------------
void() seek_fire = { self.walkframe = 0; seek_fireframe(); };

//============================================================================
// This robot does not feel pain, just exploding debris
//============================================================================
void() seek_arm_explode =
{
    particle_dust(self.pos2, 10+random()*10, PARTICLE_BURST_YELLOW);
    SpawnExplosion(EXPLODE_BIG, self.pos2, "seeker/explode_major.wav");
    SpawnProjectileSmoke(self.pos2, 150, 50, 150);
    SpawnProjectileSmoke(self.pos2, 150, 50, 150);
    SpawnProjectileSmoke(self.pos2, 150, 50, 150);
    // Throw 2 small metal blob + arm
    self.gib1origin = self.gib2origin = self.pos2;
    ThrowGib(11, 2);
    ThrowGib(12, 1);
    
    // Make sure previous function is restored so correct
    // arm animation is setup after one has been destroyed
    if (self.attack_timer == SEEKER_WALK) self.think = seek_walkframe;
    else if (self.attack_timer == SEEKER_FIRE) self.think = seek_fireframe;
    else self.think = self.th_run;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) seek_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);

        // There are no checks for shadow axe damage (pain_check = 1/2)
        if (self.state == SEEKER_ARMS_BOTH) {
            // Check for the chance to blow one of the arms off
            self.pos1_x = self.health / self.max_health;
            self.pos1_y = skill / 10;
            self.pos1_z = self.pos1_x + self.pos1_y;
//            dprint("Damage ("); dprint(ftos(damage));
//            dprint(") Chance ("); dprint(ftos(self.pos1_z));
//            dprint(")\n");

            // Check for body damage and total damage done
            // Is there a chance one of the arms will fall off?
            if (damage > 25 && random() > self.pos1_z) {
                if (random() > 0.5) {
                    self.state = SEEKER_ARMS_LEFT;
                    makevectors (self.angles);
                    self.pos2 = self.origin + attack_vector('0 -35 44');
                    seek_arm_explode();
                }
                else {
                    self.state = SEEKER_ARMS_RIGHT;
                    makevectors (self.angles);
                    self.pos2 = self.origin + attack_vector('0 35 44');
                    seek_arm_explode();
                }
            }
            else {
                // Spawn dust and sprite explosion
                particle_dust(inflictor.origin, 10+random()*10, PARTICLE_BURST_YELLOW);
                SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
                SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
                SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
            }
        }
        else {
            // Once down to one arm, just sparks and dust
            particle_dust(inflictor.origin, 10+random()*10, PARTICLE_BURST_YELLOW);
            SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
            SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
            SpawnProjectileSmoke(inflictor.origin, 150, 50, 150);
        }
    }
};

//============================================================================
void() seek_explode =
{
    // Hide and Seek
    entity_hide(self);
    
    // Regular ID particle explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    T_RadiusDamage (self, self, self.death_dmg, world, DAMAGEALL);
    
    // Classic sprite/DP explosion
    SpawnExplosion(EXPLODE_BIG, self.origin, "seeker/explode_major.wav");
    SpawnProjectileSmoke(self.origin, 150, 150, 150);
    SpawnProjectileSmoke(self.origin, 150, 150, 150);
    SpawnProjectileSmoke(self.origin, 150, 150, 150);

    // Throw a couple of random body pieces
    if (random() < 0.5) ThrowGib(11, 1);
    else ThrowGib(11, 2);
    // Lets throw our arms in the air like we don't care!
    if (self.state != SEEKER_ARMSLESS) ThrowGib(12, 1);
    // Spew random metal1_2 rubble up
    ThrowGib(13, 5 + rint(random()*5));
    ThrowGib(14, 5 + rint(random()*5));
    // If head still on shoulders, throw that as well!
    if (self.state != SEEKER_HEADLESS) ThrowGib(25, 1);
};

//----------------------------------------------------------------------
void() seek_dieframe =
{
    // type of animation state
    self.attack_timer = SEEKER_DIE;

    if (self.walkframe == 2) self.solid = SOLID_NOT;

    // Check for arm status
    if (self.state == SEEKER_ARMS_LEFT) self.frame = $deathL1 + self.walkframe;
    else if (self.state == SEEKER_ARMS_RIGHT) self.frame = $deathR1 + self.walkframe;
    else if (self.state == SEEKER_ARMSLESS) self.frame = $deathB1 + self.walkframe;
    else if (self.state == SEEKER_HEADLESS) self.frame = $deathH1 + self.walkframe;
    else self.frame = $death1 + self.walkframe;
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe < 12) {
        self.nextthink = time + 0.1;
        self.think = seek_dieframe;
    }
    else {
        // Explode after a while
        self.nextthink = time + 3 + random()*2;
        self.think = seek_explode;
    }

    // Spawn some extra smoke trails
    if (self.walkframe > 1 && random() < 0.5) {
        // make sure the origin is coming out of the mech body
        self.pos1 = self.origin + (crandom()*'16 16 0') + (random()*'0 0 24');
        SpawnProjectileSmoke(self.pos1, 200, 250, 100);
        SpawnProjectileSmoke(self.pos1, 200, 250, 100);
        if (random() < 0.5)
            SpawnExplosion(EXPLODE_MED, self.pos1, "seeker/explode_minor.wav");
    }
};

//----------------------------------------------------------------------
void() seek_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // Long death sound with comedy bing at the end!
    sound (self, CHAN_VOICE, "seeker/death1.wav", 1, ATTN_NORM);
    // reset custom origins for body/arm gib parts
    self.gib1origin = self.gib2origin = '0 0 0';
    self.max_health = MON_GIBFOUNTAIN;
    self.health = -100;
    
    // Spawn dust and sprite explosion from chest area
    makevectors(self.angles);
    self.pos1 = self.origin + attack_vector('8 0 24');
    particle_dust(self.pos1, 10+random()*10, PARTICLE_BURST_YELLOW);
    SpawnExplosion(EXPLODE_BIG, self.pos1, "seeker/explode_major.wav");
    SpawnProjectileSmoke(self.pos1, 200, 250, 100);
    
    // Random chance of head flying off body with both arms!
    if (self.state == SEEKER_ARMS_BOTH && random() > 0.6) {
        self.state = SEEKER_HEADLESS;
        ThrowGib(25, 1);
    }
    // Random chance that remaining left/right arm falls off
    else if (self.state < SEEKER_ARMSLESS && random() > 0.9) {
        self.state = SEEKER_ARMSLESS;
        ThrowGib(12, 1);
    }    
    
    // Start death sequence
    self.walkframe = 0;
    seek_dieframe ();
};

//============================================================================
void() seek_shield = 
{
    // Setup particle emitter for shield effect
    self.part_active = PARTICLE_STYLE_FFIELD;
    self.spr_frame = PARTICLE_BURST_BLUE;
    self.part_vol = '24 24 32';        // Bounding box volume
    self.part_limit = 100;            // Large amount of particles
    self.part_life = 4;                // Longer lifetime
    self.part_ofs = '0 0 32';        // Move up from monster origin
    self.part_vel = '0 0 4';        // Move up from monster origin
    self.part_emitter = spawn_pemitter(self, self, self.part_active, PARTICLE_START_ON);
    seek_stand();
};

//----------------------------------------------------------------------
void() seek_wakeup =
{
    // Check for sudden death!
    if (self.health < 0) return;
    
    self.use = SUB_Null;                // no re-trigger
    seek_finish_attachment();            // Remove shield + spawnflag
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_SEEK_SHIELD);
    
    // Switch off shield sound
    sound (self, CHAN_WEAPON, "ambience/power_off.wav", 1, ATTN_NORM);
    // Switch OFF any particle emitter
    if (self.part_emitter) misc_particle_off(self.part_emitter);
    
    // Check for any trigger off events (trigger_once)
    if (self.target2 != "") trigger_strs(self.target2, self);
    self.target2 = "";
    
    // Restore all think functions
    self.th_stand = seek_stand;
    self.th_walk = seek_walk;
    self.th_run = seek_run;
    self.th_melee = seek_melee1;        // Punch attack
    self.th_missile = seek_fire;        // Rockets incoming!
    self.th_pain = seek_pain;
    self.th_die = seek_die;
    self.takedamage = DAMAGE_YES;        // Can receive damage
    monster_targets();                    // Check for targets
};

/*======================================================================
QUAKED monster_seeker (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_seeker =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_seeker.mdl";
    self.headmdl = "progs/ad171/h_seeker.mdl";
    self.weaponglow = "progs/ad171/mon_seeker_glow.mdl";
    // 4 poses 0=generic blob, 1=generic square, 2=generic blob2, 3=arm
    self.gib1mdl = "progs/ad171/gib_seekchunk.mdl";    // types 0-2
    self.gib2mdl = "progs/ad171/gib_seekarm.mdl";        // arm only
    self.gib3mdl = "progs/ad171/gib_metal1.mdl";        // Breakable metal
    self.gib4mdl = "progs/ad171/gib_metal3.mdl";        // Breakable metal

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.weaponglow);    // Glowing blue shield
    precache_model (self.gib1mdl);        // gib chunks
    precache_model (self.gib2mdl);        // Arms
    precache_model (self.gib3mdl);        // Generic metal1_2
    precache_model (self.gib4mdl);        // Generic metal1_2
    self.gib1frame = 2;                    // 3 frames choose from
    self.gibtype = GIBTYPE_METAL;        // Metal impact sounds
    
    self.idle_sound = "seeker/idle1.wav";
    self.idle_sound2 = "seeker/idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);

    self.pain_sound = "seeker/pain1.wav";
    self.pain_sound2 = "seeker/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    precache_sound ("seeker/death1.wav");
    precache_sound ("seeker/explode_minor.wav");
    precache_sound ("seeker/explode_major.wav");
    // Power off sound for shield setup
    precache_sound ("ambience/power_off.wav");
    
    // Range attack 1 - rockets
    precache_sound("seeker/range_rocket.wav");
    precache_sound("seeker/rocket_fire.wav");
    precache_sound("seeker/rocket_hit.wav");
    // Range attack 2 - lasers
    precache_model (MODEL_PROJ_LASER);
    precache_sound("seeker/range_laser.wav");
    precache_sound ("enforcer/enfire.wav");
    precache_sound ("enforcer/enfstop.wav");
    // Range attack 3 - plasma
    precache_model (MODEL_PROJ_PLASMA);
    precache_sound ("seeker/range_plasma.wav");
    precache_sound (SOUND_PLASMA_FIRE);
    precache_sound (SOUND_PLASMA_HIT);

    // Melee attack - giant punch
    precache_sound ("seeker/melee1.wav");
    precache_sound ("seeker/punch1.wav");
    
    // Sparking from broken arm
    self.noise2 = "progs/ad171/misc_spark.mdl";
    self.noise3 = "misc/spark.wav";
    precache_model (self.noise2);
    precache_sound (self.noise3);
    
    self.sight_sound = "seeker/sight1.wav";
    precache_sound (self.sight_sound);

    // Check for any custom arm setups
    if (self.state < SEEKER_ARMS_LEFT || self.state > SEEKER_ARMS_RIGHT)
        self.state = SEEKER_ARMS_BOTH;    // Both arms are active

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GIANT;
    if (self.health < 1) {
        // if spawns with one arm, start with less HP
        if (self.state == SEEKER_ARMS_BOTH) self.health = 500;
        else self.health = 400;
    }

    if (self.deathstring == "") self.deathstring = " was destroyed by a Seeker\n";
    
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.gibbed = FALSE;                // Starts in one piece
    self.pain_flinch = 400;                // takes alot to pain
    self.pain_timeout = 2;                // High pain threshold
    self.blockudeath = TRUE;            // no humanoid death sound
    self.pain_longanim = FALSE;            // cannot be chopped with shadow axe
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_SEEKER;
    self.bouncegrenade = TRUE;            // Grenades bounce
    self.bleedcolour = MON_BCOLOR_WHITE;    // Grey dust

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = 0.5; self.resist_cells = 0;
    
    // Custom feet sounds (specific order)
    self.stepc1 = "seeker/footstep1.wav";
    self.stepc2 = "seeker/footstep2.wav";
    self.stepc3 = "seeker/footstep1.wav";
    self.stepc4 = "seeker/footstep2.wav";
    self.stepc5 = "seeker/footstep3.wav";
    precache_sound (self.stepc1);
    precache_sound (self.stepc2);
    precache_sound (self.stepc3);
    precache_sound (self.stepc4);
    precache_sound (self.stepc5);
    self.steptype = FS_TYPECUSTOM;
    self.hittype    = HIT_METAL;

    if (self.spawnflags & MON_SEEK_SHIELD) {
        self.th_stand = seek_shield;    // Setup Shield
        self.think1 = seek_wakeup;        // Wakeup function when triggered
    }
    else {
        self.th_stand = seek_stand;
        self.th_walk = seek_walk;
        self.th_run = seek_run;
        self.th_melee = seek_melee1;    // Punch attack
        self.th_missile = seek_fire;    // Rockets incoming!
        self.th_pain = seek_pain;
        self.th_die = seek_die;
    }

    self.th_checkattack = SeekerCheckAttack;
    self.classtype = CT_MONSEEKER;
    self.classgroup = CG_ROBOT;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
