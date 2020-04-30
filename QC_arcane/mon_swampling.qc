/*==============================================================================
SWAMPLINGS (Based on Voreling from Quoth - Kell/Necros/Preach)
==============================================================================*/
$frame idle1 idle2 idle3 idle4 idle5 idle6 idle7 idle8
$frame idle9 idle10 idle11 idle12 idle13

$frame idleup1 idleup2 idleup3 idleup4 idleup5 idleup6 

$frame drop1 drop2 drop3 drop4 drop5

$frame grow1 grow2 grow3 grow4 grow5 grow6 grow7 grow8 grow9 grow10

$frame walk1 walk2 walk3 walk4 walk5

$frame run1 run2 run3 run4

// Jumping up
$frame jump1 jump2 jump3 jump4 jump5 jump6

// Bite attack
$frame bite1 bite2 bite3 bite4 bite5 bite6 bite7

// Extremely short pain set
$frame pain1 pain2

// fall over and die
$frame death1 death2 death3 death4 death5 death6

// body flies up and then back to the ground
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7

// Not used, modelling templates
$frame base1 base2

//======================================================================
void()    swampling_idle1    =[    $idle1,    swampling_idle2    ] {monster_idle_sound();ai_stand();};
void()    swampling_idle2    =[    $idle2,    swampling_idle3    ] {ai_stand();};
void()    swampling_idle3    =[    $idle3,    swampling_idle4    ] {ai_stand();};
void()    swampling_idle4    =[    $idle4,    swampling_idle5    ] {ai_stand();};
void()    swampling_idle5    =[    $idle5,    swampling_idle6    ] {ai_stand();};
void()    swampling_idle6    =[    $idle6,    swampling_idle7    ] {ai_stand();};
void()    swampling_idle7    =[    $idle7,    swampling_idle8    ] {ai_stand();};
void()    swampling_idle8    =[    $idle8,    swampling_idle9    ] {ai_stand();};
void()    swampling_idle9    =[    $idle9,    swampling_idle10] {ai_stand();};
void()    swampling_idle10=[    $idle10,swampling_idle11] {ai_stand();};
void()    swampling_idle11=[    $idle11,swampling_idle12] {ai_stand();};
void()    swampling_idle12=[    $idle12,swampling_idle13] {ai_stand();};
void()    swampling_idle13=[    $idle13,swampling_idle1    ] {ai_stand();};

//======================================================================
void()    swampling_walk1    =[    $walk1,    swampling_walk2    ] {monster_idle_sound();ai_walk(4);};
void()    swampling_walk2    =[    $walk2,    swampling_walk3    ] {monster_footstep(FALSE); ai_walk(3);};
void()    swampling_walk3    =[    $walk3,    swampling_walk4    ] {ai_walk(4);};
void()    swampling_walk4    =[    $walk4,    swampling_walk5    ] {ai_walk(5);};
void()    swampling_walk5    =[    $walk5,    swampling_walk1    ] {ai_walk(5);};

//======================================================================
void() swampling_runpause =
{
    // Do nothing is not to fight or dead
    if (!self.enemy) return;
    if (self.health < 1) return;

    if (self.jump_flag < time) self.th_run();
    // Is the enemy too close? no more pausing, fight!
    self.enemydist = range_distance(self.enemy, FALSE);
    if (self.enemydist < MONAI_RUNPAUSE) self.th_run();
};

//----------------------------------------------------------------------
void()    swampling_runp1    =[    $idle1,    swampling_runp2    ] {swampling_runpause();};
void()    swampling_runp2    =[    $idle2,    swampling_runp3    ] {swampling_runpause();};
void()    swampling_runp3    =[    $idle3,    swampling_runp4    ] {swampling_runpause();};
void()    swampling_runp4    =[    $idle4,    swampling_runp5    ] {swampling_runpause();};
void()    swampling_runp5    =[    $idle5,    swampling_runp6    ] {swampling_runpause();};
void()    swampling_runp6    =[    $idle6,    swampling_runp7    ] {swampling_runpause();};
void()    swampling_runp7    =[    $idle7,    swampling_runp8    ] {swampling_runpause();};
void()    swampling_runp8    =[    $idle8,    swampling_runp9    ] {swampling_runpause();};
void()    swampling_runp9    =[    $idle9,    swampling_runp10    ] {swampling_runpause();};
void()    swampling_runp10    =[    $idle10,swampling_runp11    ] {swampling_runpause();};
void()    swampling_runp11    =[    $idle11,swampling_runp12    ] {swampling_runpause();};
void()    swampling_runp12    =[    $idle12,swampling_runp13    ] {swampling_runpause();};
void()    swampling_runp13    =[    $idle13,swampling_runp1    ] {swampling_runpause();};

//----------------------------------------------------------------------
void(float dist) swampling_checkpause =
{
    // Do nothing is not to fight or dead
    if (!self.enemy) return;
    if (self.health < 1) return;

    // make swamplings run in bursts of speed (reset every run animation cycle)
    self.movespeed = self.movespeed + 1;
    // Do run code to check for enemies
    ai_run(dist + self.movespeed);
    if (self.enemydist < MONAI_RUNPAUSE) return; // Too close

    // Random chance to stop and pause running
    if (self.movespeed > 7 && random() < 0.2) {
        self.jump_flag = time + random();
        self.think = swampling_runp1;
    }
};

//----------------------------------------------------------------------
void()    swampling_run1 =[ $run1, swampling_run2 ] {self.movespeed = 0;
    monster_idle_sound();
    swampling_checkpause(8);
    // swamplings have constant problems with weird angles (X/Z)
    // Just keep resetting them so they move normally
    ai_resetangles();
};
void()    swampling_run2 =[ $run2, swampling_run3 ] {monster_footstep(FALSE); swampling_checkpause(6);};
void()    swampling_run3 =[ $run3, swampling_run4 ] {swampling_checkpause(8);};
void()    swampling_run4 =[ $run4, swampling_run5 ] {swampling_checkpause(10);};
void()    swampling_run5 =[ $run1, swampling_run6 ] {swampling_checkpause(8);};
void()    swampling_run6 =[ $run2, swampling_run7 ] {monster_footstep(FALSE); swampling_checkpause(6);};
void()    swampling_run7 =[ $run3, swampling_run8 ] {swampling_checkpause(8);};
void()    swampling_run8 =[ $run4, swampling_run1 ] {swampling_checkpause(10);};

//======================================================================
void()    swampling_slide1    =[    $walk1,        swampling_slide2    ] {ai_run_slide(6); monster_idle_sound();};
void()    swampling_slide2    =[    $walk2,        swampling_slide3    ] {ai_run_slide(4);};
void()    swampling_slide3    =[    $walk3,        swampling_slide4    ] {ai_run_slide(6);};
void()    swampling_slide4    =[    $walk4,        swampling_slide5    ] {ai_run_slide(4);};
void()    swampling_slide5    =[    $walk5,        swampling_run1    ] {ai_run(4);};

//======================================================================
// swampling 2 - POISON SPIT FUNCTIONS (range)
//======================================================================
void(float sideang) swampling_spitacid =
{
    local    vector org, ang, dir, avel;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
        
    // Flash effect to show where bolt is coming from    
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (sideang < 0) sound (self, CHAN_WEAPON, "swampling/spit4.wav", 1, ATTN_NORM);

    makevectors (self.angles);    
    org = self.origin + attack_vector(self.attack_offset);

    // Create elevation angle and use makevectors to create projectile direction
    ang = vectoangles(self.enemy.origin - org);
    ang_x = -self.attack_elev;    // Negative = upwards angle 
    makevectors (ang);
    // fire spit in arc pattern (sideang)
    dir = (v_forward + v_right * sideang) * SPEED_SWAMPLING;    

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_SWAMP);
};

//----------------------------------------------------------------------
void()    swampling_spit1 = [ $pain1, swampling_spit2 ] {ai_face();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, SPEED_SWAMPLING); };
void()    swampling_spit2 = [ $pain2, swampling_spit3 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_SWAMPLING); };
void()    swampling_spit3 = [ $bite1, swampling_spit4 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_SWAMPLING); };
void()    swampling_spit4 = [ $bite2, swampling_spit5 ] {swampling_spitacid(-0.1);};
void()    swampling_spit5 = [ $bite3, swampling_spit6 ] {swampling_spitacid(0);};
void()    swampling_spit6 = [ $bite4, swampling_spit7 ] {swampling_spitacid(0.1);};
void()    swampling_spit7 = [ $bite5, swampling_spit8 ] {};
void()    swampling_spit8 = [ $bite6, swampling_spit9 ] {};
void()    swampling_spit9 = [ $bite7, swampling_run1 ] {};

//======================================================================
// BITE
//======================================================================
void() swampling_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);                                        // Get closer for extra bite
    ai_damagebreakable(10);                                // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESWAMPLING)) return;    // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    if (random() < 0.5) sound(self, CHAN_VOICE, "swampling/attackmunch.wav", TRUE, TRUE);
    else sound(self, CHAN_VOICE, "swampling/attacktear.wav", TRUE, TRUE);

    // Check for poisonous attribute (new poison version)
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // swampling bite (damage 1-9) is very weak
    ldmg = (random() + random() + random()) * 3;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    
    // Spawn some touch blood
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void()    swampling_bite1    =[    $bite1, swampling_bite2    ] {ai_face();};
// Start bite attack loop
void()    swampling_bite2    =[    $bite2, swampling_bite3 ] {ai_face();};
void()    swampling_bite3    =[    $bite3, swampling_bite4 ] {ai_face();};
void()    swampling_bite4 = [    $bite4, swampling_bite5 ] {swampling_melee();};
void()    swampling_bite5 = [    $bite5, swampling_bite6 ] {};
void()    swampling_bite6 =[    $bite6, swampling_bite7 ] {
    if (ai_checkmelee(MONAI_MELEESWAMPLING) && self.enemy.health > 0) self.think = swampling_bite2;};
// Exit bite attack loop
void()    swampling_bite7    =[    $bite7, swampling_run1    ] {};

//============================================================================
// JUMP FUNCTION (range)
//============================================================================
void()    swampling_JumpTouch =
{
    local float ldmg;

    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other swamplings with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 5 + 5*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            // Spawn some touch blood (no explicit direction)
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }

    // Is the swampling floating in the air?
    if (!checkbottom(self)) {
        // Is the swampling standing on something?
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------------
void() swampling_leap1    =[    $jump1, swampling_leap2    ] {ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    monster_idle_sound();
};
void() swampling_leap2    =[    $jump2, swampling_leap3    ] {ai_face();};
void() swampling_leap3    =[    $jump3, swampling_leap4    ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    self.touch = swampling_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * MONAI_JUMPSWAMPLINGDIST + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void() swampling_leap4    =[    $jump4, swampling_leap5    ] {};
void() swampling_leap5    =[    $jump5, swampling_leap6 ] {};    
void() swampling_leap6    =[    $jump6, swampling_leap7    ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = swampling_leap3;
    }
    self.oldorigin = self.origin;
};
//----------------------------------------------------------------------
void() swampling_leap7 =[ $jump3, swampling_leap8 ] {monster_footstep(FALSE);};
void() swampling_leap8 =[ $jump2, swampling_leap9 ] {monster_footstep(FALSE);};
void() swampling_leap9 =[ $jump1, swampling_run1    ] {ai_resetangles();};

//======================================================================
// CEILING swamplingS - Idle/Drop/Touch/Land functions
//======================================================================
void()    swampling_idleup1 =[ $idleup1, swampling_idleup2] {monster_idle_sound();ai_stand();};
void()    swampling_idleup2 =[ $idleup2, swampling_idleup3] {ai_stand();};
void()    swampling_idleup3 =[ $idleup3, swampling_idleup4] {ai_stand();};
void()    swampling_idleup4 =[ $idleup4, swampling_idleup5] {ai_stand();};
void()    swampling_idleup5 =[ $idleup5, swampling_idleup6] {ai_stand();};
void()    swampling_idleup6 =[ $idleup6, swampling_idleup1] {ai_stand();};

//----------------------------------------------------------------------
void() swampling_droptouch =
{
    // Check if landed on something that is not the ground?
    if (!checkbottom(self)) {
        // Is the swampling standing on something?
        if (self.flags & FL_ONGROUND) {
            self.flags = self.flags - FL_ONGROUND;
            self.origin_z = self.origin_z + 8;
            setorigin(self, self.origin);    // raise up
            self.attack_timer = time + 1;    // reset timer
            
            makevectors (self.angles);
            self.velocity = v_forward * 100 + '0 0 200';
        }
        return;
    }

    // No more flying, back to running
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setsize(self, self.bbmins, self.bbmaxs);
    
    // Reset view offset (based on bbox height)
    self.view_ofs = '0 0 0';
    self.view_ofs_z = self.maxs_z*0.5;

    self.touch = SUB_Null;            // No more jump touching
    FoundHuntTarget(TRUE);            // Setup goals and warn other monsters
    if (self.enemy.flags & FL_CLIENT) monster_sightsound();

    // Restore all think state functions (swampling is off the ceiling)
    self.th_stand = swampling_idle1;
    self.th_walk = swampling_walk1;
    self.th_run = swampling_run1; 
    self.th_slide = swampling_slide1;
    self.th_melee = swampling_bite1;

    // swampling 1 and 2 have different range attacks
    if (self.spawnflags & MON_SWAMPLING_LARGE) self.th_missile = swampling_spit1;
    else self.th_jump = swampling_leap1;

    // Back to running or standing around!
    if (!self.enemy) self.think = self.th_stand;
    else self.think = self.th_run;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    swampling_drop1    =[    $drop1,    swampling_drop2    ] {};
void()    swampling_drop2    =[    $drop2,    swampling_drop3    ] {};
void()    swampling_drop3    =[    $drop3,    swampling_drop4    ] {};
void()    swampling_drop4    =[    $drop4,    swampling_drop5    ] {};
void()    swampling_drop5    =[    $drop5,    swampling_drop5    ] {
    if (self.attack_timer < time || self.velocity_z == 0) swampling_droptouch();
};

//----------------------------------------------------------------------------
void() swampling_wakeup =
{
    // Dead already?
    if (self.health < 1) return;
        
    // Only call wakeup function once
    self.th_walk = self.th_run = self.th_slide = SUB_Null;

    // No longer need cling to ceiling spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_SWAMPLING_CEILING);

    self.flags = FL_MONSTER;            // reset flags
    
    if (engine == ENG_FITZ) self.origin_z = self.origin_z - 8;
    else self.origin_z = self.origin_z - 32;    // Unstick from ceiling
    setorigin(self, self.origin);        // Move down slightly
    
    self.movetype = MOVETYPE_TOSS;        // Affected by gravity
    self.solid = SOLID_SLIDEBOX;
    self.attack_timer = time + 1;        // Stuck timer

    self.classmove = MON_MOVEWALK;        // Back to walking/running 
    self.pain_finished = time + 1.5;    // No pain
    SUB_AttackFinished(2 + random());    // No attacking
        
    makevectors (self.angles);            // Move towards face direction
    self.velocity = v_forward * 50;        // Slight nudge forward

    self.touch = swampling_droptouch;    // Touch something?
    if (!self.jump_flag) 
    self.jump_flag = time + 1 + random()*2;    // Don't jump straight away
    swampling_drop1();                        // Turn around, cat tricks!
};

//======================================================================
// MINION - Grow and spin up from nothing
//======================================================================
void() swampling_growangle = {self.angles_y = self.angles_y + self.lefty;};
void() swampling_grow1 = [ $grow1, swampling_grow2 ] {};
void() swampling_grow2 = [ $grow2, swampling_grow3 ] {swampling_growangle();};
void() swampling_grow3 = [ $grow3, swampling_grow4 ] {swampling_growangle();};
void() swampling_grow4 = [ $grow4, swampling_grow5 ] {swampling_growangle();};
void() swampling_grow5 = [ $grow5, swampling_grow6 ] {swampling_growangle();};
void() swampling_grow6 = [ $grow6, swampling_grow7 ] {swampling_growangle();};
void() swampling_grow7 = [ $grow7, swampling_grow8 ] {swampling_growangle();};
void() swampling_grow8 = [ $grow8, swampling_grow9 ] {swampling_growangle();};
void() swampling_grow9 = [ $grow9, swampling_grow10] {swampling_growangle();};
void() swampling_grow10= [ $grow10, swampling_run1 ] {
    // Is the swampling stuck? cannot move?
    if (pointcontents(self.origin) == CONTENT_SOLID) {
        // Time to die!
        self.health = self.gibhealth;
        Killed(self, self);
    }
    else {
        // Finally spin back to original position
        self.angles_y = self.angles_y + self.lefty;
        // Setup goals and warn other monsters
        FoundHuntTarget(TRUE);

        // Restore all think state functions
        self.th_stand = swampling_idle1;
        self.th_walk = swampling_walk1;
        self.th_run = swampling_run1; 
        self.th_slide = swampling_slide1;
        self.th_melee = swampling_bite1;
        // swampling 1 and 2 have different range attacks
        if (self.spawnflags & MON_SWAMPLING_LARGE) self.th_missile = swampling_spit1;
        else {
            self.th_jump = swampling_leap1;
            self.th_jumpexit = swampling_leap7;
        }
    }
};
//----------------------------------------------------------------------------
void() swampling_grow =
{
    // Only call wakeup function once
    self.th_stand = self.th_walk = self.th_run  = SUB_Null;
    if (random() < 0.5) self.lefty = 36;
    else self.lefty = -36;
    monster_sightsound();
    swampling_grow1();
};

//============================================================================
void() swampling_pain1    =[    $pain1,    swampling_pain2    ] {};
void() swampling_pain2    =[    $pain2,    swampling_run1    ] {};

//----------------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) swampling_pain = 
{
    // Has the swampling been hit while on the ceiling?
    if (self.spawnflags & MON_SWAMPLING_CEILING) {
        self.pain_finished = time + 1;
        swampling_wakeup();
        return;
    }
        
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        self.pain_finished = time + 1;

        if (self.pain_check == 1) swampling_pain1();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.2;
            self.axhitme = 0;
            swampling_pain1();
        }
    }
};

//============================================================================
void() swampling_die1 =[    $death1, swampling_die2    ] {};
void() swampling_die2 =[    $death2, swampling_die3    ] {monster_check_gib();};
void() swampling_die3 =[    $death3, swampling_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() swampling_die4 =[    $death4, swampling_die5    ] {};
void() swampling_die5 =[    $death5, swampling_die6    ] {monster_death_postcheck();};
void() swampling_die6 =[    $death6, swampling_die6    ] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() swampling_dieB1 =[    $deathB1, swampling_dieB2    ] {};
void() swampling_dieB2 =[    $deathB2, swampling_dieB3    ] {monster_check_gib();};
void() swampling_dieB3 =[    $deathB3, swampling_dieB4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() swampling_dieB4 =[    $deathB4, swampling_dieB5    ] {};
void() swampling_dieB5 =[    $deathB5, swampling_dieB6    ] {};
void() swampling_dieB6 =[    $deathB6, swampling_dieB7    ] {monster_death_postcheck();};
void() swampling_dieB7 =[    $deathB7, swampling_dieB7    ] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() swampling_die = 
{
    // swamplings are small, gibs don't bounce far
    self.max_health = MON_NOGIBVELOCITY;

    // Has the swampling died while on the ceiling?
    if (self.spawnflags & MON_SWAMPLING_CEILING)
        self.gibondeath = TRUE;

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "swampling/death.wav", 1, ATTN_NORM);
        if (random() < 0.6) swampling_die1();
        else swampling_dieB1();
    }
};

/*======================================================================
/*QUAKED monster_swampling (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() monster_swampling =
{
    if (deathmatch) { remove(self); return; }

    if (self.spawnflags & MON_SWAMPLING_LARGE) {
        self.mdl = "progs/mon_swamplingp.mdl";
        self.gib1mdl = "progs/gib_swamplegp.mdl";    // Single Leg
    }
    else {
        self.mdl = "progs/mon_swampling.mdl";
        self.gib1mdl = "progs/gib_swampleg.mdl";    // Single Leg
    }

    precache_model (self.mdl);
    precache_model (self.gib1mdl);
    precache_model (MODEL_PROJ_SWAMP);            // Spit Projectile

    // IDLE/COMBAT and SIGHT sounds
    self.idle_sound = "swampling/idle1.wav";
    self.idle_sound2 = "swampling/idle3.wav";
    self.idle_soundcom = "swampling/idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);

    // death/pain/attack sounds
    precache_sound("swampling/death.wav");
    self.pain_sound = "swampling/pain3.wav";
    precache_sound(self.pain_sound);

    precache_sound("swampling/miss.wav");    // Spit misses
    precache_sound("swampling/spit4.wav");    // Spit attack
    precache_sound("swampling/attackmunch.wav");
    precache_sound("swampling/attacktear.wav");
    precache_sound("swampling/jumpland.wav");

    self.sight_sound = "swampling/sight2.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TINY;
    self.gibbed = FALSE;
    self.pain_flinch = 10;                // Always flinch
    self.steptype = FS_TYPELIGHT;
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '20 0 0';        // Bite attack offset
    self.attack_offset = '14 0 8';        // Used by large swampling, at jaws
    self.movespeed = 1;                    // Can never be a turret
    self.poisonous = TRUE;                // Always poisonous
    self.deathstring = " was bitten by a Swampling\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = SwamplingCheckAttack;
    self.th_pain = swampling_pain;
    self.th_die = swampling_die;

    self.classtype = CT_MONSWAMPLING;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;

    // Setup light/dark green swampling difference
    if (self.spawnflags & MON_SWAMPLING_LARGE) {
        if (self.health < 1) self.health = 60;
        self.gibhealth = -25;
        self.th_missile = swampling_spit1;
    }
    else {
        if (self.health < 1) self.health = 30;
        self.gibhealth = -20;
        self.th_jump = swampling_leap1;
        self.th_jumpexit = swampling_leap7;
    }

    //----------------------------------------------------------------------
    // Ceiling swamplings have special idle animation
    // and need to let go of the ceiling before resuming any
    // normal behaviour (most think functions are intercepted)
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_SWAMPLING_CEILING) {
        self.th_stand = self.th_walk = swampling_idleup1;
        self.th_run = self.th_slide = swampling_wakeup;
        self.th_melee = self.th_missile = self.th_jump = swampling_wakeup;
        // th_pain and th_die functions understand ceiling swamplings
    }
    // Default swampling behaviour functions
    else {
        self.th_stand = swampling_idle1;
        self.th_walk = swampling_walk1;
        self.th_run = swampling_run1;
        self.th_melee = swampling_bite1;
        self.th_slide = swampling_slide1;
    }
    
    monster_start();
};
