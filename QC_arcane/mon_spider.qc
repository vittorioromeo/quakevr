/*==============================================================================
SPIDER (Hexen2 model by Raven Software)
==============================================================================*/
$frame idle1 idle2 idle3 idle4 idle5 idle6 idle7 idle8
$frame idle9 idle10 idle11 idle12 idle13

$frame idleup1 idleup2 idleup3 idleup4 idleup5 idleup6 idleup7 idleup8
$frame idleup9 idleup10 idleup11 idleup12 idleup13

$frame drop1 drop2 drop3 drop4 drop5 drop6

$frame grow1 grow2 grow3 grow4 grow5 grow6 grow7 grow8 grow9 grow10

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16

// Lift spider up on to back legs
$frame atkup1 atkup2 atkup3 atkup4 atkup5 atkup6 atkup7 atkup8
$frame atkup9 atkup10 atkup11 atkup12

// Forward attacking (looping)
$frame atkloop1 atkloop2 atkloop3 atkloop4 atkloop5 atkloop6 atkloop7 atkloop8
$frame atkloop9 atkloop10

// Jumping up
$frame jump1 jump2 jump3 jump4 jump5 jump6 jump7 jump8
$frame jump9 jump10 jump11 jump12 jump13 jump14 jump15 jump16

// on all legs (pain backwards)
$frame paindown1 paindown2 paindown3 paindown4 paindown5 paindown6 paindown7 paindown8
$frame paindown9 paindown10

// On back legs (pain backwards)
$frame painup1 painup2 painup3 painup4 painup5 painup6 painup7 painup8

// roll over death on back (looks odd)
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20

//======================================================================
void() spider_idle1 =[ $idle1, spider_idle2] {monster_idle_sound();ai_stand();};
void() spider_idle2 =[ $idle2, spider_idle3] {ai_stand();};
void() spider_idle3 =[ $idle3, spider_idle4] {ai_stand();};
void() spider_idle4 =[ $idle4, spider_idle5] {ai_stand();};
void() spider_idle5 =[ $idle5, spider_idle6] {ai_stand();};
void() spider_idle6 =[ $idle6, spider_idle7] {ai_stand();};
void() spider_idle7 =[ $idle7, spider_idle8] {ai_stand();};
void() spider_idle8 =[ $idle8, spider_idle9] {ai_stand();};
void() spider_idle9 =[ $idle9, spider_idle10] {ai_stand();};
void() spider_idle10 =[ $idle10, spider_idle11] {ai_stand();};
void() spider_idle11 =[ $idle11, spider_idle12] {ai_stand();};
void() spider_idle12 =[ $idle12, spider_idle13] {ai_stand();};
void() spider_idle13 =[ $idle13, spider_idle1] {ai_stand();};

//======================================================================
void() spider_walk1 =[ $walk1, spider_walk2] {monster_idle_sound();ai_walk(4);};
void() spider_walk2 =[ $walk3, spider_walk3] {monster_footstep(FALSE); ai_walk(3);};
void() spider_walk3 =[ $walk5, spider_walk4] {ai_walk(4);};
void() spider_walk4 =[ $walk7, spider_walk5] {ai_walk(5);};
void() spider_walk5 =[ $walk9, spider_walk6] {ai_walk(4);};
void() spider_walk6 =[ $walk11, spider_walk7] {monster_footstep(FALSE); ai_walk(3);};
void() spider_walk7 =[ $walk13, spider_walk8] {ai_walk(4);};
void() spider_walk8 =[ $walk15, spider_walk1] {ai_walk(5);};

//======================================================================
void() spider_runpause =
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
void()    spider_runp1    =[    $idle1,    spider_runp2    ] {spider_runpause();};
void()    spider_runp2    =[    $idle2,    spider_runp3    ] {spider_runpause();};
void()    spider_runp3    =[    $idle3,    spider_runp4    ] {spider_runpause();};
void()    spider_runp4    =[    $idle4,    spider_runp5    ] {spider_runpause();};
void()    spider_runp5    =[    $idle5,    spider_runp6    ] {spider_runpause();};
void()    spider_runp6    =[    $idle6,    spider_runp7    ] {spider_runpause();};
void()    spider_runp7    =[    $idle7,    spider_runp8    ] {spider_runpause();};
void()    spider_runp8    =[    $idle8,    spider_runp9    ] {spider_runpause();};
void()    spider_runp9    =[    $idle9,    spider_runp10    ] {spider_runpause();};
void()    spider_runp10    =[    $idle10,spider_runp11    ] {spider_runpause();};
void()    spider_runp11    =[    $idle11,spider_runp12    ] {spider_runpause();};
void()    spider_runp12    =[    $idle12,spider_runp13    ] {spider_runpause();};
void()    spider_runp13    =[    $idle13,spider_runp1    ] {spider_runpause();};

//----------------------------------------------------------------------
void(float dist) spider_checkpause =
{
    // Do nothing is not to fight or dead
    if (!self.enemy) return;
    if (self.health < 1) return;

    // make spider run in bursts of speed (reset every run animation cycle)
    self.movespeed = self.movespeed + 1;
    // Do run code to check for enemies
    ai_run(dist + self.movespeed);
    
    // Check if target is too close?
    if (self.enemydist < MONAI_RUNPAUSE) return;
    // Random chance to stop and pause running
    if (self.movespeed > 7 && random() < 0.2) {
        self.jump_flag = time + random();
        self.think = spider_runp1;
    }
};

//----------------------------------------------------------------------
void()    spider_run1 =[ $walk1, spider_run2 ] {self.movespeed = 0; monster_idle_sound();spider_checkpause(8);};
void()    spider_run2 =[ $walk3, spider_run3 ] {monster_footstep(FALSE); spider_checkpause(6);};
void()    spider_run3 =[ $walk5, spider_run4 ] {spider_checkpause(8);};
void()    spider_run4 =[ $walk7, spider_run5 ] {spider_checkpause(10);};
void()    spider_run5 =[ $walk9, spider_run6 ] {spider_checkpause(8);};
void()    spider_run6 =[ $walk11, spider_run7 ] {monster_footstep(FALSE); spider_checkpause(6);};
void()    spider_run7 =[ $walk13, spider_run8 ] {spider_checkpause(8);};
void()    spider_run8 =[ $walk15, spider_run1 ] {spider_checkpause(10);};

//======================================================================
void()    spider_slide1 =[ $walk1, spider_slide2 ] {ai_run_slide(6); monster_idle_sound();};
void()    spider_slide2 =[ $walk3, spider_slide3 ] {ai_run_slide(4);};
void()    spider_slide3 =[ $walk5, spider_slide4 ] {ai_run_slide(6);};
void()    spider_slide4 =[ $walk7, spider_slide5 ] {ai_run_slide(4);};
void()    spider_slide5 =[ $walk9, spider_slide6 ] {ai_run_slide(6);};
void()    spider_slide6 =[ $walk11, spider_slide7 ] {ai_run_slide(4);};
void()    spider_slide7 =[ $walk13, spider_slide8 ] {ai_run_slide(6);};
void()    spider_slide8 =[ $walk15, spider_run1 ] {ai_run(4);};

//======================================================================
// SPIDER 2 - ACID SPIT FUNCTIONS (range)
//======================================================================
void(float sideang) spider_spitacid =
{
    local    vector org, ang, dir, avel;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
        
    // Flash effect to show where bolt is coming from    
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (sideang < 0) sound (self, CHAN_WEAPON, "spider/hiss2.wav", 1, ATTN_NORM);

    makevectors (self.angles);    
    org = self.origin + attack_vector(self.attack_offset);

    // Create elevation angle and use makevectors to create projectile direction
    ang = vectoangles(self.enemy.origin - org);
    ang_x = -self.attack_elev;    // Negative = upwards angle 
    makevectors (ang);
    // fire spit in arc pattern (sideang)
    dir = (v_forward + v_right * sideang) * SPEED_SPIDER;    

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_SPID);
};

//----------------------------------------------------------------------
void()    spider_spit1    =[    $atkup1,        spider_spit2    ] {ai_face();};
void()    spider_spit2    =[    $atkup5,        spider_spit3    ] {ai_face();};
void()    spider_spit3    =[    $atkup7,        spider_spit4    ] {ai_face();};
void()    spider_spit4    =[    $atkup9,        spider_spit5 ] {ai_face();};
void()    spider_spit5    =[    $atkup11,        spider_spit6 ] {ai_face();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, SPEED_SPIDER); };
void()    spider_spit6    =[    $atkloop1,        spider_spit7 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_SPIDER); };
void()    spider_spit7    =[    $atkloop3,        spider_spit8 ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, SPEED_SPIDER); };
void()    spider_spit8 = [    $atkloop5,        spider_spit9 ] {spider_spitacid(-0.1);};
void()    spider_spit9 = [    $atkloop7,        spider_spit10 ] {spider_spitacid(0);};
void()    spider_spit10 =[    $atkloop9,        spider_spit11 ] {spider_spitacid(0.1);};
void()    spider_spit11    =[    $atkup7,        spider_spit12] {};
void()    spider_spit12    =[    $atkup5,        spider_run1    ] {};

//======================================================================
// BITE - Both spider types can bite if within range
//======================================================================
void() spider_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);                                    // Get closer for extra bite
    ai_damagebreakable(10);                            // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESPIDER)) return;    // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    if (random() < 0.5) sound(self, CHAN_WEAPON, "spider/attackmunch.wav", TRUE, TRUE);
    else sound(self, CHAN_WEAPON, "spider/attacktear.wav", TRUE, TRUE);

    // Spider bite (damage 1-9) is very weak
    ldmg = (random() + random() + random()) * 3;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Check for poisonous attribute (generally green spider)
    if (self.poisonous) PoisonDeBuff(self.enemy);
    
    // Spawn blood at mouth of spider
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void() spider_bite1 = [ $atkup1, spider_bite2 ] {ai_face();};
void() spider_bite2 = [ $atkup5, spider_bite3 ] {ai_face();};
void() spider_bite3 = [ $atkup7, spider_bite4 ] {ai_face();};
void() spider_bite4 = [ $atkup9, spider_bite5 ] {ai_face();};
void() spider_bite5 = [ $atkup11, spider_bite6 ] {ai_face();};
// Start bite attack loop
void() spider_bite6 = [ $atkloop1, spider_bite7 ] {ai_face();};
void() spider_bite7=[ $atkloop3, spider_bite8 ] {ai_face();};
void() spider_bite8 = [ $atkloop5, spider_bite9 ] {spider_melee();};
void() spider_bite9 = [ $atkloop7, spider_bite10 ] {};
void() spider_bite10 = [ $atkloop9, spider_bite11 ] {
    if (ai_checkmelee(MONAI_MELEESPIDER) && self.enemy.health > 0 && self.health > 0) 
        self.think = spider_bite6;};
// Exit bite attack loop
void() spider_bite11 = [ $atkup7, spider_bite12 ] {};
void() spider_bite12 = [ $atkup5, spider_run1 ] {};

//============================================================================
// SPIDER 1 - JUMP FUNCTION (range)
//============================================================================
void()    Spider_JumpTouch =
{
    local    float    ldmg;

    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other spiders with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 5 + 5*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            // Spawn some touch blood (no explicit direction)
            spawn_touchblood (self.enemy, self.enemy, ldmg*3);
        }
    }

    // Is the spider floating in the air?
    if (!checkbottom(self)) {
        // Is the spider standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------------
void() spider_leap1 =[ $jump1, spider_leap2 ] {ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    monster_idle_sound();
};
void() spider_leap2 =[ $jump3, spider_leap3 ] {ai_face();};
void() spider_leap3 =[ $jump5, spider_leap4 ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPTIMEOUT; // Stop jumping so much
    self.touch = Spider_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * MONAI_JUMPSPIDERDIST + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};
void() spider_leap4 =[ $jump7, spider_leap5 ] {};
void() spider_leap5 =[ $jump8, spider_leap6 ] {};    
void() spider_leap6 =[ $jump9, spider_leap7 ] {};
void() spider_leap7 =[ $jump10, spider_leap8 ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = spider_leap3;
    }
    self.oldorigin = self.origin;
};

//----------------------------------------------------------------------
void() spider_leap8 =[ $jump11, spider_leap9] {monster_footstep(FALSE);};
void() spider_leap9 =[ $jump13, spider_leap10] {monster_footstep(FALSE);};
void() spider_leap10 =[ $jump15, spider_run1] {ai_resetangles();};

//======================================================================
// CEILING SPIDERS - Idle/Drop/Touch/Land functions
//======================================================================
void()    spider_idleup1    =[    $idleup1,    spider_idleup2    ] {monster_idle_sound();ai_stand();};
void()    spider_idleup2    =[    $idleup2,    spider_idleup3    ] {ai_stand();};
void()    spider_idleup3    =[    $idleup3,    spider_idleup4    ] {ai_stand();};
void()    spider_idleup4    =[    $idleup4,    spider_idleup5    ] {ai_stand();};
void()    spider_idleup5    =[    $idleup5,    spider_idleup6    ] {ai_stand();};
void()    spider_idleup6    =[    $idleup6,    spider_idleup7    ] {ai_stand();};
void()    spider_idleup7    =[    $idleup7,    spider_idleup8    ] {ai_stand();};
void()    spider_idleup8    =[    $idleup8,    spider_idleup9    ] {ai_stand();};
void()    spider_idleup9    =[    $idleup9,    spider_idleup10    ] {ai_stand();};
void()    spider_idleup10    =[    $idleup10,    spider_idleup11    ] {ai_stand();};
void()    spider_idleup11    =[    $idleup11,    spider_idleup12    ] {ai_stand();};
void()    spider_idleup12    =[    $idleup12,    spider_idleup13    ] {ai_stand();};
void()    spider_idleup13    =[    $idleup13,    spider_idleup1    ] {ai_stand();};

//----------------------------------------------------------------------
void() spider_droptouch =
{
    // Check if landed on something that is not the ground?
    if (!checkbottom(self)) {
        // Is the spider standing on something?
        if (self.flags & FL_ONGROUND) {
            self.flags = self.flags - FL_ONGROUND;
            self.origin_z = self.origin_z + 8;
            setorigin(self, self.origin);        // raise up
            self.attack_timer = time + 1;        // reset timer
            
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

    // Restore all think state functions (spider is off the ceiling)
    self.th_stand = spider_idle1;
    self.th_walk = spider_walk1;
    self.th_run = spider_run1; 
    self.th_slide = spider_slide1;
    self.th_melee = spider_bite1;

    // Spider 1 and 2 have different range attacks
    if (self.spawnflags & MON_SPIDER_LARGE) self.th_missile = spider_spit1;
    else self.th_jump = spider_leap1;

    // Back to running or standing around!
    if (!self.enemy) self.think = self.th_stand;
    else self.think = self.th_run;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    spider_drop1 =[    $drop1,    spider_drop2 ] {};
void()    spider_drop2 =[    $drop2,    spider_drop3 ] {};
void()    spider_drop3 =[    $drop3,    spider_drop4 ] {};
void()    spider_drop4 =[    $drop4,    spider_drop5 ] {};
void()    spider_drop5 =[    $drop5,    spider_drop6 ] {};
void()    spider_drop6 =[    $drop6,    spider_drop6 ] {
    if (self.attack_timer < time || self.velocity_z == 0) spider_droptouch();
};

//----------------------------------------------------------------------------
void() spider_wakeup =
{
    // Dead already?
    if (self.health < 1) return;
        
    // Only call wakeup function once
    self.th_walk = self.th_run = self.th_slide = SUB_Null;

    // No longer need cling to ceiling spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_SPIDER_CEILING);

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

    self.touch = spider_droptouch;        // Touch something?
    if (!self.jump_flag) 
    self.jump_flag = time + 1 + random()*2;    // Don't jump straight away
    spider_drop1();                        // Turn around, cat tricks!
};

//======================================================================
// MINION - Grow and spin up from nothing
//======================================================================
void() spider_growangle = {self.angles_y = self.angles_y + self.lefty;};
void() spider_grow1 = [ $grow1, spider_grow2 ] {};
void() spider_grow2 = [ $grow2, spider_grow3 ] {spider_growangle();};
void() spider_grow3 = [ $grow3, spider_grow4 ] {spider_growangle();};
void() spider_grow4 = [ $grow4, spider_grow5 ] {spider_growangle();};
void() spider_grow5 = [ $grow5, spider_grow6 ] {spider_growangle();};
void() spider_grow6 = [ $grow6, spider_grow7 ] {spider_growangle();};
void() spider_grow7 = [ $grow7, spider_grow8 ] {spider_growangle();};
void() spider_grow8 = [ $grow8, spider_grow9 ] {spider_growangle();};
void() spider_grow9 = [ $grow9, spider_grow10] {spider_growangle();};
void() spider_grow10= [ $grow10, spider_run1 ] {
    // Is the spider stuck? cannot move?
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
        self.th_stand = spider_idle1;
        self.th_walk = spider_walk1;
        self.th_run = spider_run1; 
        self.th_slide = spider_slide1;
        self.th_melee = spider_bite1;
        // Spider 1 and 2 have different range attacks
        if (self.spawnflags & MON_SPIDER_LARGE) self.th_missile = spider_spit1;
        else {
            self.th_jump = spider_leap1;
            self.th_jumpexit = spider_leap9;
        }
    }
};

//----------------------------------------------------------------------------
void() spider_grow =
{
    // Only call wakeup function once
    self.th_stand = self.th_walk = self.th_run  = SUB_Null;
    if (random() < 0.5) self.lefty = 36;
    else self.lefty = -36;
    monster_sightsound();
    spider_grow1();
};

//============================================================================
void() spider_paind1    =[    $paindown1,    spider_paind2    ] {};
void() spider_paind2    =[    $paindown3,    spider_paind3    ] {};
void() spider_paind3    =[    $paindown5,    spider_paind4    ] {};
void() spider_paind4    =[    $paindown7,    spider_paind5    ] {};
void() spider_paind5    =[    $paindown9,    spider_run1    ] {};

//----------------------------------------------------------------------------
// Front legs up (in melee or missile attack)
void() spider_painu1    =[    $painup1,    spider_painu2    ] {};
void() spider_painu2    =[    $painup3,    spider_painu3    ] {};
void() spider_painu3    =[    $painup5,    spider_painu4    ] {};
void() spider_painu4    =[    $painup6,    spider_run1    ] {};

//----------------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) spider_pain = 
{
    // Has the spider been hit while on the ceiling?
    if (self.spawnflags & MON_SPIDER_CEILING) {
        self.pain_finished = time + 1;
        spider_wakeup();
        return;
    }
        
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        self.pain_finished = time + 1;

        if (self.pain_check == 1 || self.pain_check == 2) {
            if (self.pain_check == 2) {
                // reset axe hit and setup short pain recovery
                self.pain_finished = time + 0.4;
                self.axhitme = 0;
            }
            // Different types of pain animations based on current attack
            if ( self.frame > $atkup8 && self.frame < $atkloop10 ) spider_painu1();
            else spider_paind1();
        }
    }
};

//============================================================================
void() spider_die1        =[    $death1,    spider_die2    ] {};
void() spider_die2        =[    $death3,    spider_die3    ] {monster_check_gib();};
void() spider_die3        =[    $death5,    spider_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() spider_die4        =[    $death7,    spider_die5    ] {};
void() spider_die5        =[    $death9,    spider_die6    ] {};
void() spider_die6        =[    $death11,    spider_die7    ] {};
void() spider_die7        =[    $death13,    spider_die8    ] {};
void() spider_die8        =[    $death15,    spider_die9    ] {};
void() spider_die9        =[    $death17,    spider_die10] {monster_death_postcheck();};
void() spider_die10        =[    $death19,    spider_die10] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() spider_die = 
{
    // Spiders are small, gibs don't bounce far
    self.max_health = MON_NOGIBVELOCITY;

    // Has the spider died while on the ceiling?
    if (self.spawnflags & MON_SPIDER_CEILING)
        self.gibondeath = TRUE;

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "spider/death1.wav", 1, ATTN_NORM);
        spider_die1();
    }
};

/*======================================================================
/*QUAKED monster_spider (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() setup_spider;
void() monster_spider =
{
    if (deathmatch) { remove(self); return; }

    if (self.spawnflags & MON_SPIDER_LARGE) {
        self.mdl = "progs/mon_spiderg.mdl";
        self.headmdl = "progs/h_spiderg.mdl";        // Front Jaws
        self.gib1mdl = "progs/gib_spidlegg.mdl";    // Single Leg
        self.gib2mdl = "progs/gib_spidbodyg.mdl";    // Back Body

        precache_model (self.mdl);
        precache_model (self.headmdl);
        precache_model (self.gib1mdl);
        precache_model (self.gib2mdl);        
        precache_model (MODEL_PROJ_SPID);            // Spit Projectile
    }
    if (!(self.spawnflags & MON_SPIDER_LARGE) || 
        self.classtype == CT_CACHESPIDER) {
        self.mdl = "progs/mon_spiderb.mdl";
        self.headmdl = "progs/h_spiderb.mdl";        // Front Jaws
        self.gib1mdl = "progs/gib_spidlegb.mdl";    // Single Leg
        self.gib2mdl = "progs/gib_spidbodyb.mdl";    // Back Body

        precache_model (self.mdl);
        precache_model (self.headmdl);
        precache_model (self.gib1mdl);
        precache_model (self.gib2mdl);
    }

    // Head/Body and Special GIB models
    self.gib1frame = 9;

    // IDLE/COMBAT and SIGHT sounds
    self.idle_sound = "spider/idle1.wav";
    self.idle_sound2 = "spider/hiss2.wav";
    self.idle_soundcom = "spider/idle1.wav";
    self.idle_soundcom2 = "spider/hiss3.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);
    precache_sound (self.idle_soundcom2);

    // death/pain/attack sounds
    precache_sound("spider/death1.wav");
    self.pain_sound = "spider/pain1.wav";
    self.pain_sound2 = "spider/pain2.wav";
    precache_sound(self.pain_sound);
    precache_sound(self.pain_sound2);

    precache_sound("spider/miss.wav");
    precache_sound("spider/attackmunch.wav");
    precache_sound("spider/attacktear.wav");
    precache_sound("spider/jumpland.wav");

    self.sight_sound = "spider/hiss1.wav";
    precache_sound (self.sight_sound);

    // Cache spider is a special class sed for precache only
    if (self.classtype != CT_CACHESPIDER) setup_spider();
};

//----------------------------------------------------------------------------
void() monster_wraithminion = { 
    self.classtype = CT_CACHESPIDER; 
    self.spawnflags = MON_SPIDER_LARGE;    // Precache both spiders
    monster_spider();
};

//----------------------------------------------------------------------------
void() setup_spider =
{
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TINY;
    self.gibbed = FALSE;
    self.pain_flinch = 10;                // Always flinch
    self.steptype = FS_TYPELIGHT;
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '24 0 20';        // Bite attack offset
    self.attack_offset = '14 0 8';        // Used by green spider, at jaws
    self.movespeed = 1;                    // Can never be a turret
    self.deathstring = " was bitten by a Spider\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = SpiderCheckAttack;
    self.th_pain = spider_pain;
    self.th_die = spider_die;

    if(!self.classtype) self.classtype = CT_MONSPIDER;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;

    // Setup green/brown spider difference
    if (self.spawnflags & MON_SPIDER_LARGE) {
        if (self.health < 1) self.health = 75;
        self.gibhealth = -25;
        self.poisonous = TRUE;        // Always poisonous
        self.th_missile = spider_spit1;
    }
    else {
        if (self.health < 1) self.health = 30;
        self.gibhealth = -20;
        self.poisonous = FALSE;        // Large only are poisonous
        self.th_jump = spider_leap1;
        self.th_jumpexit = spider_leap9;
    }
    
    //----------------------------------------------------------------------
    // Ceiling spiders have special idle animation (rotated)
    // and need to let go of the ceiling before resuming any
    // normal behaviour (most think functions are intercepted)
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_SPIDER_CEILING) {
        self.th_stand = self.th_walk = spider_idleup1;
        self.th_run = self.th_slide = spider_wakeup;
        self.th_melee = self.th_missile = self.th_jump = spider_wakeup;
        // th_pain and th_die functions understand ceiling spiders
    }
    // Special spawning minion need to start spinning
    else if (self.classtype == CT_MINIONSPIDER) {
        self.th_stand = self.th_walk = self.th_run = spider_grow;
        self.th_melee = self.th_slide = SUB_Null;
        self.th_missile = self.th_jump = SUB_Null;
    }
    // Default spider behaviour functions
    else {
        self.th_stand = spider_idle1;
        self.th_walk = spider_walk1;
        self.th_run = spider_run1;
        self.th_melee = spider_bite1;
        self.th_slide = spider_slide1;
    }
    
    monster_start();
};

//----------------------------------------------------------------------------
// A code way to spawn spiders (requires monster_spiderspawn entity)
//----------------------------------------------------------------------------
void(vector minion_org, entity minion_targ) minion_spider =
{
    local entity minion;
    
    // Check if there is space to spawn entity
    if (entity_pcontent(minion_org)) return;
    
    update_minioncount(self.owner, 1);        // Update spawn counters
    
    minion = spawn();
    minion.classname = "monster_spider";    // For function searching
    setorigin(minion, minion_org);            // Move to new location
    minion.owner = self.owner;                // Spawner Parent Link
    self.owner = minion;                    // Stop gibs interacting with minion
    
    minion.effects = minion.flags = 0;        // make sure are blank
    minion.gibondeath = 1;                    // Always gib on death
    minion.classtype = CT_MINIONSPIDER;        // Special minion class
    minion.enemy = minion_targ;                // Target to attack
    minion.minion_active = TRUE;            // Minion flag
    minion.bodyfadeaway = TRUE;                // Get rid of body
    
    if (random() < 0.2) minion.spawnflags = MON_SPIDER_LARGE;
    else minion.spawnflags = 0;
    
    if (minion.spawnflags & MON_SPIDER_LARGE) {
        minion.mdl = "progs/mon_spiderg.mdl";
        minion.headmdl = "progs/h_spiderg.mdl";        // Front Jaws
        minion.gib1mdl = "progs/gib_spidlegg.mdl";    // Single Leg
        minion.gib2mdl = "progs/gib_spidbodyg.mdl";    // Back Body
    }
    else {    
        minion.mdl = "progs/mon_spiderb.mdl";
        minion.headmdl = "progs/h_spiderb.mdl";        // Front Jaws
        minion.gib1mdl = "progs/gib_spidlegb.mdl";    // Single Leg
        minion.gib2mdl = "progs/gib_spidbodyb.mdl";    // Back Body
    }

    minion.gib1frame = 9;
    minion.idle_sound = "spider/idle1.wav";
    minion.idle_sound2 = "spider/hiss2.wav";
    minion.idle_soundcom = "spider/idle1.wav";
    minion.idle_soundcom2 = "spider/hiss3.wav";
    minion.sight_sound = "spider/hiss1.wav";
    minion.pain_sound = "spider/pain1.wav";
    minion.pain_sound2 = "spider/pain2.wav";
    
    minion.nextthink = time + 0.01;
    minion.think = setup_spider;
};