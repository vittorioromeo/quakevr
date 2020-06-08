/*==============================================================================
VORELINGS (Originally from Quoth - Kell/Necros/Preach)
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
void()    voreling_idle1    =[    $idle1,    voreling_idle2    ] {monster_idle_sound();ai_stand();};
void()    voreling_idle2    =[    $idle2,    voreling_idle3    ] {ai_stand();};
void()    voreling_idle3    =[    $idle3,    voreling_idle4    ] {ai_stand();};
void()    voreling_idle4    =[    $idle4,    voreling_idle5    ] {ai_stand();};
void()    voreling_idle5    =[    $idle5,    voreling_idle6    ] {ai_stand();};
void()    voreling_idle6    =[    $idle6,    voreling_idle7    ] {ai_stand();};
void()    voreling_idle7    =[    $idle7,    voreling_idle8    ] {ai_stand();};
void()    voreling_idle8    =[    $idle8,    voreling_idle9    ] {ai_stand();};
void()    voreling_idle9    =[    $idle9,    voreling_idle10    ] {ai_stand();};
void()    voreling_idle10    =[    $idle10,voreling_idle11    ] {ai_stand();};
void()    voreling_idle11    =[    $idle11,voreling_idle12    ] {ai_stand();};
void()    voreling_idle12    =[    $idle12,voreling_idle13    ] {ai_stand();};
void()    voreling_idle13    =[    $idle13,voreling_idle1    ] {ai_stand();};

//======================================================================
void()    voreling_walk1    =[    $walk1,    voreling_walk2    ] {monster_idle_sound();ai_walk(4);};
void()    voreling_walk2    =[    $walk2,    voreling_walk3    ] {monster_footstep(FALSE); ai_walk(3);};
void()    voreling_walk3    =[    $walk3,    voreling_walk4    ] {ai_walk(4);};
void()    voreling_walk4    =[    $walk4,    voreling_walk5    ] {ai_walk(5);};
void()    voreling_walk5    =[    $walk5,    voreling_walk1    ] {ai_walk(5);};

//======================================================================
void() voreling_runpause =
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
void()    voreling_runp1    =[    $idle1,    voreling_runp2    ] {voreling_runpause();};
void()    voreling_runp2    =[    $idle2,    voreling_runp3    ] {voreling_runpause();};
void()    voreling_runp3    =[    $idle3,    voreling_runp4    ] {voreling_runpause();};
void()    voreling_runp4    =[    $idle4,    voreling_runp5    ] {voreling_runpause();};
void()    voreling_runp5    =[    $idle5,    voreling_runp6    ] {voreling_runpause();};
void()    voreling_runp6    =[    $idle6,    voreling_runp7    ] {voreling_runpause();};
void()    voreling_runp7    =[    $idle7,    voreling_runp8    ] {voreling_runpause();};
void()    voreling_runp8    =[    $idle8,    voreling_runp9    ] {voreling_runpause();};
void()    voreling_runp9    =[    $idle9,    voreling_runp10    ] {voreling_runpause();};
void()    voreling_runp10    =[    $idle10,voreling_runp11    ] {voreling_runpause();};
void()    voreling_runp11    =[    $idle11,voreling_runp12    ] {voreling_runpause();};
void()    voreling_runp12    =[    $idle12,voreling_runp13    ] {voreling_runpause();};
void()    voreling_runp13    =[    $idle13,voreling_runp1    ] {voreling_runpause();};

//----------------------------------------------------------------------
void(float dist) voreling_checkpause =
{
    // Do nothing is not to fight or dead
    if (!self.enemy) return;
    if (self.health < 1) return;

    // make vorelings run in bursts of speed (reset every run animation cycle)
    self.movespeed = self.movespeed + 1;
    // Do run code to check for enemies
    ai_run(dist + self.movespeed);
    if (self.enemydist < MONAI_RUNPAUSE) return; // Too close

    // Random chance to stop and pause running
    if (self.movespeed > 7 && random() < 0.2) {
        self.jump_flag = time + random();
        self.think = voreling_runp1;
    }
};

//----------------------------------------------------------------------
void()    voreling_run1 =[ $run1, voreling_run2 ] {self.movespeed = 0;
    monster_idle_sound();
    voreling_checkpause(8);
    // Vorelings have constant problems with weird angles (X/Z)
    // Just keep resetting them so they move normally
    ai_resetangles();
};
void()    voreling_run2 =[ $run2, voreling_run3 ] {monster_footstep(FALSE); voreling_checkpause(6);};
void()    voreling_run3 =[ $run3, voreling_run4 ] {voreling_checkpause(8);};
void()    voreling_run4 =[ $run4, voreling_run5 ] {voreling_checkpause(10);};
void()    voreling_run5 =[ $run1, voreling_run6 ] {voreling_checkpause(8);};
void()    voreling_run6 =[ $run2, voreling_run7 ] {monster_footstep(FALSE); voreling_checkpause(6);};
void()    voreling_run7 =[ $run3, voreling_run8 ] {voreling_checkpause(8);};
void()    voreling_run8 =[ $run4, voreling_run1 ] {voreling_checkpause(10);};

//======================================================================
void()    voreling_slide1    =[    $walk1,        voreling_slide2    ] {ai_run_slide(6); monster_idle_sound();};
void()    voreling_slide2    =[    $walk2,        voreling_slide3    ] {ai_run_slide(4);};
void()    voreling_slide3    =[    $walk3,        voreling_slide4    ] {ai_run_slide(6);};
void()    voreling_slide4    =[    $walk4,        voreling_slide5    ] {ai_run_slide(4);};
void()    voreling_slide5    =[    $walk5,        voreling_run1    ] {ai_run(4);};

//======================================================================
// VORELING 2 - PURPLE SPIT FUNCTIONS (range)
//======================================================================
void(float sideang) voreling_spitacid =
{
    local    vector org, ang, dir, avel;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
        
    // Flash effect to show where bolt is coming from    
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (sideang < 0) sound (self, CHAN_WEAPON, "voreling/hiss2.wav", 1, ATTN_NORM);

    makevectors (self.angles);    
    org = self.origin + attack_vector(self.attack_offset);

    // Create elevation angle and use makevectors to create projectile direction
    ang = vectoangles(self.enemy.origin - org);
    ang_x = -self.attack_elev;    // Negative = upwards angle 
    makevectors (ang);
    // fire spit in arc pattern (sideang)
    dir = (v_forward + v_right * sideang) * SPEED_VORELING;    

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_VORE);
};

//----------------------------------------------------------------------
void()    voreling_spit1 = [ $pain1, voreling_spit2 ] {ai_face();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, SPEED_VORELING); };
void()    voreling_spit2 = [ $pain2, voreling_spit3 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_VORELING); };
void()    voreling_spit3 = [ $bite1, voreling_spit4 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_VORELING); };
void()    voreling_spit4 = [ $bite2, voreling_spit5 ] {voreling_spitacid(-0.1);};
void()    voreling_spit5 = [ $bite3, voreling_spit6 ] {voreling_spitacid(0);};
void()    voreling_spit6 = [ $bite4, voreling_spit7 ] {voreling_spitacid(0.1);};
void()    voreling_spit7 = [ $bite5, voreling_spit8 ] {};
void()    voreling_spit8 = [ $bite6, voreling_spit9 ] {};
void()    voreling_spit9 = [ $bite7, voreling_run1 ] {};

//======================================================================
// BITE
//======================================================================
void() voreling_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);                                        // Get closer for extra bite
    ai_damagebreakable(10);                                // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEEVORELING)) return;    // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    if (random() < 0.5) sound(self, CHAN_VOICE, "voreling/attackmunch.wav", TRUE, TRUE);
    else sound(self, CHAN_VOICE, "voreling/attacktear.wav", TRUE, TRUE);

    // Voreling bite is very weak
    ldmg = (random() + random() + random()) * 3;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    
    // Spawn some touch blood
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void()    voreling_bite1    =[    $bite1, voreling_bite2    ] {ai_face();};
// Start bite attack loop
void()    voreling_bite2    =[    $bite2, voreling_bite3 ] {ai_face();};
void()    voreling_bite3    =[    $bite3, voreling_bite4 ] {ai_face();};
void()    voreling_bite4 = [    $bite4, voreling_bite5 ] {voreling_melee();};
void()    voreling_bite5 = [    $bite5, voreling_bite6 ] {};
void()    voreling_bite6 =[    $bite6, voreling_bite7 ] {
    if (ai_checkmelee(MONAI_MELEEVORELING) && self.enemy.health > 0) self.think = voreling_bite2;};
// Exit bite attack loop
void()    voreling_bite7    =[    $bite7, voreling_run1    ] {};

//============================================================================
// JUMP FUNCTION (range)
//============================================================================
void()    Voreling_JumpTouch =
{
    local float ldmg;

    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other vorelings with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 5 + 5*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            // Spawn some touch blood (no explicit direction)
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }

    // Is the voreling floating in the air?
    if (!checkbottom(self)) {
        // Is the voreling standing on something?
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------------
void() voreling_leap1    =[    $jump1, voreling_leap2    ] {ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    monster_idle_sound();
};
void() voreling_leap2    =[    $jump2, voreling_leap3    ] {ai_face();};
void() voreling_leap3    =[    $jump3, voreling_leap4    ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    self.touch = Voreling_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * MONAI_JUMPVORELINGDIST + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void() voreling_leap4    =[    $jump4, voreling_leap5    ] {};
void() voreling_leap5    =[    $jump5, voreling_leap6 ] {};    
void() voreling_leap6    =[    $jump6, voreling_leap7    ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = voreling_leap3;
    }
    self.oldorigin = self.origin;
};
//----------------------------------------------------------------------
void() voreling_leap7 =[ $jump3, voreling_leap8 ] {monster_footstep(FALSE);};
void() voreling_leap8 =[ $jump2, voreling_leap9 ] {monster_footstep(FALSE);};
void() voreling_leap9 =[ $jump1, voreling_run1    ] {ai_resetangles();};

//======================================================================
// CEILING VORELINGS - Idle/Drop/Touch/Land functions
//======================================================================
void()    voreling_idleup1 =[ $idleup1, voreling_idleup2] {monster_idle_sound();ai_stand();};
void()    voreling_idleup2 =[ $idleup2, voreling_idleup3] {ai_stand();};
void()    voreling_idleup3 =[ $idleup3, voreling_idleup4] {ai_stand();};
void()    voreling_idleup4 =[ $idleup4, voreling_idleup5] {ai_stand();};
void()    voreling_idleup5 =[ $idleup5, voreling_idleup6] {ai_stand();};
void()    voreling_idleup6 =[ $idleup6, voreling_idleup1] {ai_stand();};

//----------------------------------------------------------------------
void() voreling_droptouch =
{
    // Check if landed on something that is not the ground?
    if (!checkbottom(self)) {
        // Is the voreling standing on something?
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
    self.view_ofs = '0 0 24';        // Restore view offset

    self.touch = SUB_Null;            // No more jump touching
    FoundHuntTarget(TRUE);            // Setup goals and warn other monsters
    if (self.enemy.flags & FL_CLIENT) monster_sightsound();

    // Restore all think state functions (voreling is off the ceiling)
    self.th_stand = voreling_idle1;
    self.th_walk = voreling_walk1;
    self.th_run = voreling_run1; 
    self.th_slide = voreling_slide1;
    self.th_melee = voreling_bite1;

    // Voreling 1 and 2 have different range attacks
    if (self.spawnflags & MON_VORELING_LARGE) self.th_missile = voreling_spit1;
    else self.th_jump = voreling_leap1;

    // Back to running or standing around!
    if (!self.enemy) self.think = self.th_stand;
    else self.think = self.th_run;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    voreling_drop1    =[    $drop1,    voreling_drop2    ] {};
void()    voreling_drop2    =[    $drop2,    voreling_drop3    ] {};
void()    voreling_drop3    =[    $drop3,    voreling_drop4    ] {};
void()    voreling_drop4    =[    $drop4,    voreling_drop5    ] {};
void()    voreling_drop5    =[    $drop5,    voreling_drop5    ] {
    if (self.attack_timer < time || self.velocity_z == 0) voreling_droptouch();
};

//----------------------------------------------------------------------------
void() voreling_wakeup =
{
    // Dead already?
    if (self.health < 1) return;
        
    // Only call wakeup function once
    self.th_walk = self.th_run = self.th_slide = SUB_Null;

    // No longer need cling to ceiling spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_VORELING_CEILING);

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

    self.touch = voreling_droptouch;    // Touch something?
    if (!self.jump_flag) 
    self.jump_flag = time + 1 + random()*2;    // Don't jump straight away
    voreling_drop1();                        // Turn around, cat tricks!
};

//======================================================================
// MINION - Grow and spin up from nothing
//======================================================================
void() voreling_growangle = {self.angles_y = self.angles_y + self.lefty;};
void() voreling_grow1 = [ $grow1, voreling_grow2 ] {};
void() voreling_grow2 = [ $grow2, voreling_grow3 ] {voreling_growangle();};
void() voreling_grow3 = [ $grow3, voreling_grow4 ] {voreling_growangle();};
void() voreling_grow4 = [ $grow4, voreling_grow5 ] {voreling_growangle();};
void() voreling_grow5 = [ $grow5, voreling_grow6 ] {voreling_growangle();};
void() voreling_grow6 = [ $grow6, voreling_grow7 ] {voreling_growangle();};
void() voreling_grow7 = [ $grow7, voreling_grow8 ] {voreling_growangle();};
void() voreling_grow8 = [ $grow8, voreling_grow9 ] {voreling_growangle();};
void() voreling_grow9 = [ $grow9, voreling_grow10] {voreling_growangle();};
void() voreling_grow10= [ $grow10, voreling_run1 ] {
    // Is the voreling stuck? cannot move?
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
        self.th_stand = voreling_idle1;
        self.th_walk = voreling_walk1;
        self.th_run = voreling_run1; 
        self.th_slide = voreling_slide1;
        self.th_melee = voreling_bite1;
        // Voreling 1 and 2 have different range attacks
        if (self.spawnflags & MON_VORELING_LARGE) self.th_missile = voreling_spit1;
        else {
            self.th_jump = voreling_leap1;
            self.th_jumpexit = voreling_leap7;
        }
    }
};
//----------------------------------------------------------------------------
void() voreling_grow =
{
    // Only call wakeup function once
    self.th_stand = self.th_walk = self.th_run  = SUB_Null;
    if (random() < 0.5) self.lefty = 36;
    else self.lefty = -36;
    monster_sightsound();
    voreling_grow1();
};

//============================================================================
void() voreling_pain1    =[    $pain1,    voreling_pain2    ] {};
void() voreling_pain2    =[    $pain2,    voreling_run1    ] {};

//----------------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) voreling_pain = 
{
    // Has the voreling been hit while on the ceiling?
    if (self.spawnflags & MON_VORELING_CEILING) {
        self.pain_finished = time + 1;
        voreling_wakeup();
        return;
    }
        
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        self.pain_finished = time + 1;

        if (self.pain_check == 1) voreling_pain1();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.2;
            self.axhitme = 0;
            voreling_pain1();
        }
    }
};

//============================================================================
void() voreling_die1 =[    $death1, voreling_die2    ] {};
void() voreling_die2 =[    $death2, voreling_die3    ] {monster_check_gib();};
void() voreling_die3 =[    $death3, voreling_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() voreling_die4 =[    $death4, voreling_die5    ] {};
void() voreling_die5 =[    $death5, voreling_die6    ] {monster_death_postcheck();};
void() voreling_die6 =[    $death6, voreling_die6    ] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() voreling_dieB1 =[    $deathB1, voreling_dieB2    ] {};
void() voreling_dieB2 =[    $deathB2, voreling_dieB3    ] {monster_check_gib();};
void() voreling_dieB3 =[    $deathB3, voreling_dieB4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() voreling_dieB4 =[    $deathB4, voreling_dieB5    ] {};
void() voreling_dieB5 =[    $deathB5, voreling_dieB6    ] {};
void() voreling_dieB6 =[    $deathB6, voreling_dieB7    ] {monster_death_postcheck();};
void() voreling_dieB7 =[    $deathB7, voreling_dieB7    ] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() voreling_die = 
{
    // Vorelings are small, gibs don't bounce far
    self.max_health = MON_NOGIBVELOCITY;

    // Has the voreling died while on the ceiling?
    if (self.spawnflags & MON_VORELING_CEILING)
        self.gibondeath = TRUE;

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "voreling/death.wav", 1, ATTN_NORM);
        if (random() < 0.6) voreling_die1();
        else voreling_dieB1();
    }
};

/*======================================================================
/*QUAKED monster_voreling (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() setup_voreling;
void() monster_voreling =
{
    if (deathmatch) { remove(self); return; }

    if (self.spawnflags & MON_VORELING_LARGE) {
        self.mdl = "progs/ad171/mon_vorelingp.mdl";
        self.gib1mdl = "progs/ad171/gib_vorelegp.mdl";    // Single Leg

        precache_model (self.mdl);
        precache_model (self.gib1mdl);
        precache_model (MODEL_PROJ_VORE);            // Spit Projectile
    }
    if (!(self.spawnflags & MON_VORELING_LARGE) || 
        self.classtype == CT_CACHEVORELING) {
        self.mdl = "progs/ad171/mon_voreling.mdl";
        self.gib1mdl = "progs/ad171/gib_voreleg.mdl";        // Single Leg

        precache_model (self.mdl);
        precache_model (self.gib1mdl);
    }

    // IDLE/COMBAT and SIGHT sounds
    self.idle_sound = "voreling/idle.wav";
    precache_sound (self.idle_sound);

    // death/pain/attack sounds
    precache_sound("voreling/death.wav");
    self.pain_sound = "voreling/pain.wav";
    precache_sound(self.pain_sound);

    precache_sound("voreling/miss.wav");    // Spit misses
    precache_sound("voreling/hiss2.wav");    // Spit attack
    precache_sound("voreling/attackmunch.wav");
    precache_sound("voreling/attacktear.wav");
    precache_sound("voreling/jumpland.wav");

    self.sight_sound = "voreling/sight.wav";
    precache_sound (self.sight_sound);

    // Cache voreling is a special class used for precache only
    if (self.classtype != CT_CACHEVORELING) setup_voreling();
};

//----------------------------------------------------------------------------
void() monster_shalrathminion = { 
    self.classtype = CT_CACHEVORELING; 
    self.spawnflags = MON_VORELING_LARGE;    // Precache both vorelings
    monster_voreling();
};

//----------------------------------------------------------------------------
void() setup_voreling =
{
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TINY;
    self.gibbed = FALSE;
    self.pain_flinch = 10;                // Always flinch
    self.steptype = FS_TYPELIGHT;
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '20 0 0';        // Bite attack offset
    self.attack_offset = '14 0 8';        // Used by purple voreling, at jaws
    self.movespeed = 1;                    // Can never be a turret

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was nibbled by a Voreling\n";

    self.th_checkattack = VorelingCheckAttack;
    self.th_pain = voreling_pain;
    self.th_die = voreling_die;

    if(!self.classtype) self.classtype = CT_MONVORELING;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;

    // Setup white/purple voreling difference
    if (self.spawnflags & MON_VORELING_LARGE) {
        if (self.health < 1) self.health = 60;
        self.gibhealth = -25;
        self.th_missile = voreling_spit1;
    }
    else {
        if (self.health < 1) self.health = 30;
        self.gibhealth = -20;
        self.th_jump = voreling_leap1;
        self.th_jumpexit = voreling_leap7;
    }

    //----------------------------------------------------------------------
    // Ceiling vorelings have special idle animation
    // and need to let go of the ceiling before resuming any
    // normal behaviour (most think functions are intercepted)
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_VORELING_CEILING) {
        self.th_stand = self.th_walk = voreling_idleup1;
        self.th_run = self.th_slide = voreling_wakeup;
        self.th_melee = self.th_missile = self.th_jump = voreling_wakeup;
        // th_pain and th_die functions understand ceiling vorelings
    }
    // Special spawning minion need to start spinning
    else if (self.classtype == CT_MINIONVORELING) {
        self.th_stand = self.th_walk = self.th_run = voreling_grow;
        self.th_melee = self.th_slide = SUB_Null;
        self.th_missile = self.th_jump = SUB_Null;
    }
    // Default voreling behaviour functions
    else {
        self.th_stand = voreling_idle1;
        self.th_walk = voreling_walk1;
        self.th_run = voreling_run1;
        self.th_melee = voreling_bite1;
        self.th_slide = voreling_slide1;
    }
    
    monster_start();
};

//----------------------------------------------------------------------------
// A code way to spawn vorelings (requires monster_shalrathminion entity)
//----------------------------------------------------------------------------
void(vector minion_org, entity minion_targ) minion_voreling =
{
    local entity minion;

    // Check if there is space to spawn entity
    if (entity_pcontent(minion_org)) return;
    
    update_minioncount(self.owner, 1);        // Update spawn counters
    
    minion = spawn();
    minion.classname = "monster_voreling";    // For function searching
    setorigin(minion, minion_org);            // Move to new location
    minion.owner = self.owner;                // Spawner Parent Link
    self.owner = minion;                    // Stop gibs interacting with minion
    
    minion.effects = minion.flags = 0;        // make sure are blank
    minion.gibondeath = 1;                    // Always gib on death
    minion.classtype = CT_MINIONVORELING;    // Special minion class
    minion.enemy = minion_targ;                // Target to attack
    minion.spawnflags = 0;
    minion.minion_active = TRUE;            // Minion flag
    minion.bodyfadeaway = TRUE;                // Get rid of body
    
    if (random() < 0.2) minion.spawnflags = MON_VORELING_LARGE;
    else minion.spawnflags = 0;

    if (minion.spawnflags & MON_VORELING_LARGE) {
        minion.mdl = "progs/ad171/mon_vorelingp.mdl";        // Purpler horror
        minion.gib1mdl = "progs/ad171/gib_vorelegp.mdl";    // Single Leg
    }
    else {    
        minion.mdl = "progs/ad171/mon_voreling.mdl";        // Creamy delight
        minion.gib1mdl = "progs/ad171/gib_voreleg.mdl";    // Single Leg
    }
    
    minion.idle_sound = "voreling/idle.wav";
    minion.sight_sound = "voreling/sight.wav";
    minion.pain_sound = "voreling/pain.wav";
    
    minion.nextthink = time + 0.01;
    minion.think = setup_voreling;
};