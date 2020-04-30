/*==============================================================================
Scorpion (Hexen2 model by Raven Software)
==============================================================================*/
$frame attackA1 attackA2 attackA3 attackA4 attackA5 attackA6 attackA7
$frame attackA8 attackA9 attackA10 attackA11 attackA12 attackA13 attackA14
$frame attackA15 attackA16 attackA17 attackA18 attackA19 attackA20 attackA21
$frame attackA22 attackA23 attackA24 attackA25

$frame attackB1 attackB2 attackB3 attackB4 attackB5 attackB6 attackB7
$frame attackB8 attackB9 attackB10 attackB11 attackB12 attackB13 attackB14
$frame attackB15 attackB16 attackB17 attackB18 attackB19 attackB20 attackB21
$frame attackB22 attackB23 attackB24 attackB25 attackB26 attackB27

$frame attackC1 attackC2 attackC3 attackC4 attackC5 attackC6 attackC7
$frame attackC8 attackC9 attackC10 attackC11 attackC12 attackC13 attackC14
$frame attackC15 attackC16 attackC17 attackC18 attackC19 attackC20 attackC21
$frame attackC22

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21

$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8
$frame pain9 pain10

$frame grow1 grow2 grow3 grow4 grow5 grow6 grow7 grow8 grow9 grow10

$frame ready1 ready2 ready3 ready4 ready5 ready6 ready7 ready8
$frame ready9 ready10 ready11

$frame step1 step2 step3 step4

$frame stop1 stop2 stop3 stop4 stop5 stop6 stop7 stop8 stop9

$frame wake1 wake2 wake3 wake4 wake5 wake6 wake7 wake8
$frame wake9 wake10 wake11 wake12 wake13 wake14 wake15 wake16
$frame wake17 wake18 wake19 wake20 wake21 wake22 wake23 wake24
$frame wake25 wake26 wake27 wake28 wake29 wake30

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16

//======================================================================
void()    pinch_idle1    =[    $ready6,    pinch_idle2 ] {monster_idle_sound();ai_stand();};
void()    pinch_idle2    =[    $ready7,    pinch_idle3 ] {ai_stand();};
void()    pinch_idle3    =[    $ready8,    pinch_idle4 ] {ai_stand();};
void()    pinch_idle4    =[    $ready9,    pinch_idle5 ] {ai_stand();};
void()    pinch_idle5    =[    $ready10,    pinch_idle6 ] {ai_stand();};
void()    pinch_idle6    =[    $ready11,    pinch_idle7 ] {ai_stand();};
void()    pinch_idle7    =[    $ready10,    pinch_idle8 ] {ai_stand();};
void()    pinch_idle8    =[    $ready9,    pinch_idle9 ] {ai_stand();};
void()    pinch_idle9    =[    $ready8,    pinch_idle10 ] {ai_stand();};
void()    pinch_idle10=[    $ready7,    pinch_idle1 ] {ai_stand();};

//======================================================================
void()    pinch_walk1    =[    $walk2,        pinch_walk2    ] {monster_idle_sound();ai_walk(3);};
void()    pinch_walk2    =[    $walk4,        pinch_walk3    ] {monster_footstep(FALSE); ai_walk(2);};
void()    pinch_walk3    =[    $walk6,        pinch_walk4    ] {ai_walk(3);};
void()    pinch_walk4    =[    $walk8,        pinch_walk5    ] {ai_walk(4);};
void()    pinch_walk5    =[    $walk10,    pinch_walk6    ] {ai_walk(3);};
void()    pinch_walk6    =[    $walk12,    pinch_walk7    ] {monster_footstep(FALSE); ai_walk(2);};
void()    pinch_walk7    =[    $walk14,    pinch_walk8    ] {ai_walk(3);};
void()    pinch_walk8    =[    $walk16,    pinch_walk1    ] {ai_walk(4);};

//======================================================================
void()    pinch_run1    =[    $walk1,        pinch_run2    ] {monster_idle_sound();ai_run(5);};
void()    pinch_run2    =[    $walk3,        pinch_run3    ] {monster_footstep(FALSE); ai_run(4);};
void()    pinch_run3    =[    $walk5,        pinch_run4    ] {ai_run(5);};
void()    pinch_run4    =[    $walk7,        pinch_run5    ] {ai_run(6);};
void()    pinch_run5    =[    $walk9,        pinch_run6    ] {ai_run(5);};
void()    pinch_run6    =[    $walk11,    pinch_run7    ] {monster_footstep(FALSE); ai_run(4);};
void()    pinch_run7    =[    $walk13,    pinch_run8    ] {ai_run(5);};
void()    pinch_run8    =[    $walk15,    pinch_run1    ] {ai_run(6);};

//======================================================================
// BITE Melee Range
//======================================================================
void(float side) pinch_melee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);                            // Get closer for extra bite
    ai_damagebreakable(10);                    // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESCORPION)) return;    // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    if (self.meleeattack < 1) sound(self, CHAN_WEAPON, "scorpion/clawattack1.wav", TRUE, TRUE);
    else sound(self, CHAN_WEAPON, "scorpion/clawattack2.wav", TRUE, TRUE);
    self.meleeattack = 1 - self.meleeattack;

    // Scorpion claw
    ldmg = (random() + random() + random()) * 4;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    SpawnMeatSpray (self, self.enemy, side);
};

//----------------------------------------------------------------------
void()    pinch_claw1    =[    $attackB2,        pinch_claw2    ] {ai_face();};
void()    pinch_claw2    =[    $attackB4,        pinch_claw3    ] {ai_face();};
void()    pinch_claw3    =[    $attackB6,        pinch_claw4    ] {ai_face();};
void()    pinch_claw4    =[    $attackB8,        pinch_claw5 ] {pinch_melee(-100);};
void()    pinch_claw5    =[    $attackB10,        pinch_claw6 ] {ai_face();};
void()    pinch_claw6    =[    $attackB12,        pinch_claw7 ] {ai_face();};
void()    pinch_claw7    =[    $attackB14,        pinch_claw8 ] {pinch_melee(100);};
void()    pinch_claw8 = [    $attackB16,        pinch_claw9 ] {};
void()    pinch_claw9 = [    $attackB18,        pinch_claw10 ] {};
void()    pinch_claw10 =[    $attackB20,        pinch_run1 ] {};

//============================================================================
// TAIL FUNCTION (range)
//============================================================================
float TAILOFFSET = 8;

//----------------------------------------------------------------------
void() pinch_tailsound =
{
    if (self.meleeattack < 1) sound(self, CHAN_WEAPON, "scorpion/tailfire1.wav", TRUE, TRUE);
    else sound(self, CHAN_WEAPON, "scorpion/tailfire2.wav", TRUE, TRUE);
    self.meleeattack = 1 - self.meleeattack;
};

//----------------------------------------------------------------------
void(vector tailofs) pinch_tailspike =
{
    local vector org, dir;

    if (!self.enemy) return;
    if (self.health < 1) return;
    
    ai_face();
    pinch_tailsound();
    self.attack_speed = SPEED_SCORPTAIL + (skill * SPEED_SCORPSKILL);
    makevectors(self.angles);

    // Generate spike at final animation position
    org = self.origin + attack_vector(tailofs);
    dir = self.enemy.origin + v_right*(TAILOFFSET*3);
    dir = normalize(dir - org);
    launch_projectile (org, dir, CT_PROJ_SCORP, self.attack_speed+random()*25);

    dir = self.enemy.origin;
    dir = normalize(dir - org);
    launch_projectile (org, dir, CT_PROJ_SCORP, self.attack_speed+random()*25);

    dir = self.enemy.origin - v_right*(TAILOFFSET*3);
    dir = normalize(dir - org);
    launch_projectile (org, dir, CT_PROJ_SCORP, self.attack_speed+random()*25);
}

//----------------------------------------------------------------------
void()    pinch_tail1    =[    $attackA2,        pinch_tail2    ] {ai_face();
    if (self.lefty < 1) sound (self, CHAN_VOICE, "scorpion/tailattack1.wav", 1, ATTN_NORM);
    else sound (self, CHAN_VOICE, "scorpion/tailattack2.wav", 1, ATTN_NORM);
    self.lefty = 1 - self.lefty;
};
void()    pinch_tail2    =[    $attackA4,        pinch_tail3    ] {ai_face();};
void()    pinch_tail3    =[    $attackA6,        pinch_tail4    ] {ai_face();};
void()    pinch_tail4    =[    $attackA8,        pinch_tail5 ] {ai_face();};
void()    pinch_tail5    =[    $attackA10,        pinch_tail6 ] {ai_face();};
void()    pinch_tail6    =[    $attackA12,        pinch_tail7 ] {ai_face();pinch_tailsound();};
void()    pinch_tail7    =[    $attackA14,        pinch_tail8 ] {pinch_tailspike('32 8 14');};
void()    pinch_tail8 = [    $attackA16,        pinch_tail9 ] {pinch_tailsound();};
void()    pinch_tail9 = [    $attackA18,        pinch_tail10 ] {ai_face();};
void()    pinch_tail10 =[    $attackA20,        pinch_tail11 ] {};
void()    pinch_tail11 =[    $attackA22,        pinch_run1 ] {ai_face();};

//============================================================================
// JUMP FUNCTION (range)
//============================================================================
void()    pinch_jumptouch =
{
    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    //Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other scorpions with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        sound (self, CHAN_VOICE, "scorpion/jumphit.wav", 1, ATTN_NORM);
        BleedDeBuff(other);
        spawn_touchblood (self, self.enemy, 100);
    }

    // Is the scorpion floating in the air?
    if (!checkbottom(self)) {
        // Is the scorpion standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------------
void() pinch_leap1    =[    $attackC1, pinch_leap2    ] {ai_face();
    self.jump_flag = time + MONAI_JUMPSCORPTIME;
    if (self.lefty < 1) sound (self, CHAN_VOICE, "scorpion/jumpattack1.wav", 1, ATTN_NORM);
    else sound (self, CHAN_VOICE, "scorpion/jumpattack2.wav", 1, ATTN_NORM);
    self.lefty = 1 - self.lefty;
};
void() pinch_leap2    =[    $attackC3, pinch_leap3    ] {ai_face(); };
void() pinch_leap3    =[    $attackC5, pinch_leap4    ] {ai_face();
    // This is a crazy idea, throw the scorpion like a grenade!
    // Using the same logic as the ogre grenade zware system
    self.attack_speed = 575 + random()*50;
    // Aiming the scorpion at the players head (+24 z axis)
    self.pos2 = self.enemy.origin + '0 0 24';
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.pos2, self.attack_speed);
    self.pos1 = vectoangles(self.pos2 - self.origin);
    self.pos1_x = -self.attack_elev;
    makevectors (self.pos1);
    self.velocity = v_forward * self.attack_speed;
    self.jump_flag = time + MONAI_JUMPSCORPTIME;
    self.touch = pinch_jumptouch;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void() pinch_leap4    =[    $attackC7, pinch_leap5    ] {ai_face();};
void() pinch_leap5    =[    $attackC8, pinch_leap6 ] {ai_face();};    
void() pinch_leap6    =[    $attackC9, pinch_leap8 ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = pinch_leap3;
    }
    self.oldorigin = self.origin;
};

//----------------------------------------------------------------------
void() pinch_leap8    =[    $attackC10, pinch_leap9 ] {ai_charge(4);};
void() pinch_leap9    =[    $attackC11, pinch_leap10] {ai_charge(4);};
void() pinch_leap10    =[    $attackC12, pinch_leap11] {ai_charge(4);};
void() pinch_leap11    =[    $attackC14, pinch_leap12] {ai_charge(4);};
void() pinch_leap12    =[    $attackC16, pinch_leap13] {ai_charge(4);};
void() pinch_leap13    =[    $attackC18, pinch_leap14] {
    if (ai_checkmelee(MONAI_MELEESCORPION) && self.enemy.health > 0) 
        self.think = pinch_claw2;
};
void() pinch_leap14    =[    $attackC20, pinch_run1 ] {};

//======================================================================
// MINION - Grow and spin up from nothing
//======================================================================
void() pinch_growangle = {self.angles_y = self.angles_y + self.lefty;};
void() pinch_grow1 = [ $grow1, pinch_grow2 ] {};
void() pinch_grow2 = [ $grow2, pinch_grow3 ] {pinch_growangle();};
void() pinch_grow3 = [ $grow3, pinch_grow4 ] {pinch_growangle();};
void() pinch_grow4 = [ $grow4, pinch_grow5 ] {pinch_growangle();};
void() pinch_grow5 = [ $grow5, pinch_grow6 ] {pinch_growangle();};
void() pinch_grow6 = [ $grow6, pinch_grow7 ] {pinch_growangle();};
void() pinch_grow7 = [ $grow7, pinch_grow8 ] {pinch_growangle();};
void() pinch_grow8 = [ $grow8, pinch_grow9 ] {pinch_growangle();};
void() pinch_grow9 = [ $grow9, pinch_grow10] {pinch_growangle();};
void() pinch_grow10= [ $grow10, pinch_run1 ] {
    // Is the scorpion stuck? cannot move?
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
        self.th_stand = pinch_idle1;
        self.th_walk = pinch_walk1;
        self.th_run = pinch_run1;
        self.th_melee = pinch_claw1;
        self.th_missile = pinch_tail1;
    }
};

//----------------------------------------------------------------------------
void() pinch_grow =
{
    // Only call wakeup function once
    self.th_stand = self.th_walk = self.th_run  = SUB_Null;
    if (random() < 0.5) self.lefty = 36;
    else self.lefty = -36;
    monster_sightsound();
    pinch_grow1();
};

//============================================================================
void() pinch_painA1    =[    $pain1,    pinch_painA2    ] {};
void() pinch_painA2    =[    $pain3,    pinch_painA3    ] {};
void() pinch_painA3    =[    $pain5,    pinch_painA4    ] {};
void() pinch_painA4    =[    $pain7,    pinch_painA5    ] {};
void() pinch_painA5    =[    $pain9,    pinch_run1        ] {};

void() pinch_painB1    =[    $pain2,    pinch_painB2    ] {};
void() pinch_painB2    =[    $pain4,    pinch_painB3    ] {};
void() pinch_painB3    =[    $pain6,    pinch_painB4    ] {};
void() pinch_painB4    =[    $pain8,    pinch_painB5    ] {};
void() pinch_painB5    =[    $pain10,pinch_run1        ] {};

//----------------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) pinch_pain = 
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        self.pain_finished = time + 1;

        if (self.pain_check == 1 || self.pain_check == 2) {
            // Different types of pain animations based on current attack
            if (random() < 0.5) pinch_painA1();
            else pinch_painB1();
        }
    }
};

//============================================================================
void() pinch_die1        =[    $death1,    pinch_die2    ] {};
void() pinch_die2        =[    $death3,    pinch_die3    ] {monster_check_gib();};
void() pinch_die3        =[    $death5,    pinch_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() pinch_die4        =[    $death7,    pinch_die5    ] {};
void() pinch_die5        =[    $death9,    pinch_die6    ] {};
void() pinch_die6        =[    $death11,    pinch_die7    ] {};
void() pinch_die7        =[    $death13,    pinch_die8    ] {};
void() pinch_die8        =[    $death15,    pinch_die9    ] {};
void() pinch_die9        =[    $death17,    pinch_die10] {};
void() pinch_die10        =[    $death19,    pinch_die11] {monster_death_postcheck();};
void() pinch_die11        =[    $death21,    pinch_die11] {monster_deadbody_check();};

//----------------------------------------------------------------------------
void() pinch_die = 
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "scorpion/death1.wav", 1, ATTN_NORM);
        pinch_die1();
    }
};

/*======================================================================
/*QUAKED monster_scorpion (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() setup_scorpion;
void() monster_scorpion =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_scorpion.mdl";
    self.headmdl = "progs/ad171/h_scorpion.mdl";        // Front Jaws
    self.gib1mdl = "progs/ad171/gib_scorpleg.mdl";    // Single Leg
    self.gib2mdl = "progs/ad171/gib_scorptail.mdl";    // Tail + Sting
    self.gib3mdl = "progs/ad171/gib_scorpclaw.mdl";    // Claw

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);        
    precache_model (self.gib3mdl);    

    // Random chance of 2 claws, or 2 legs
    if (random() < 0.5) self.gib2mdl = self.gib3mdl;
    else if (random() < 0.5) self.gib2mdl = self.gib1mdl;
    
    // IDLE/COMBAT and SIGHT sounds
    if (self.spawnflags & MON_SCORPION_STINGER) {
        self.idle_sound = "scorpion/idle1.wav";
        self.idle_sound2 = "scorpion/idle2.wav";
        self.sight_sound = "scorpion/sight1.wav";

        precache_sound("scorpion/jumpattack1.wav");
        precache_sound("scorpion/jumpattack2.wav");
        precache_sound("scorpion/jumphit.wav");
        if (self.health < 1) self.health = 120;
        self.gibhealth = -25;
        self.idlemoreoften = TRUE;        // More creepy idle sounds
        self.exactskin = 3;                // Black skin
    }
    else {
        precache_model (MODEL_PROJ_SCORP);        // Large green spike
        self.idle_sound = "scorpion/warning1.wav";
        self.idle_sound2 = "scorpion/warning2.wav";
        self.sight_sound = "scorpion/hiss.wav";

        precache_sound("scorpion/tailfire1.wav");
        precache_sound("scorpion/tailfire2.wav");
        precache_sound("scorpion/tailattack1.wav");
        precache_sound("scorpion/tailattack2.wav");
        if (self.health < 1) self.health = 80;
        self.gibhealth = -20;
        if (!self.exactskin) self.exactskin = rint((random()*3)-0.5);
    }
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.sight_sound);

    // death/pain/attack sounds
    precache_sound("scorpion/death1.wav");
    self.pain_sound = "scorpion/pain1.wav";
    self.pain_sound2 = "scorpion/pain2.wav";
    precache_sound(self.pain_sound);
    precache_sound(self.pain_sound2);

    precache_sound("scorpion/clawattack1.wav");
    precache_sound("scorpion/clawattack2.wav");

    // Cache scorpion is a special class used for precache only
    if (self.classtype != CT_CACHESCORPION) setup_scorpion();
};

//----------------------------------------------------------------------------
void() monster_wraithminion2 = { 
    self.classtype = CT_CACHESCORPION; 
    monster_scorpion();
};
    
//----------------------------------------------------------------------------
void() setup_scorpion =
{
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TINY;
    self.gibbed = FALSE;
    self.pain_flinch = 10;                // Always flinch
    self.yaw_speed = 30;                // Very fast turning (def=20)
    self.steptype = FS_TYPELIGHT;
    self.pain_longanim = FALSE;            // No long pain animation
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '32 0 10';        // Claw attack offset
    self.attack_offset = '32 8 14';        // Nail attack offset
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = ScorpionCheckAttack;
    self.th_pain = pinch_pain;
    self.th_die = pinch_die;

    // Special spawning minion need to start spinning
    if (self.classtype == CT_MINIONSCORPION) {
        self.th_stand = self.th_walk = self.th_run = pinch_grow;
        self.th_melee = self.th_missile = SUB_Null;
    }
    else {
        self.th_stand = pinch_idle1;
        self.th_walk = pinch_walk1;
        self.th_run = pinch_run1;
        self.th_melee = pinch_claw1;
        // Black stinger jumps and regulars range attack
        if (self.spawnflags & MON_SCORPION_STINGER) {
            self.th_jump = pinch_leap1;
            self.th_jumpexit = pinch_leap8;
        }
        else self.th_missile = pinch_tail1;
        
    }

    if ( (random () < 0.500) ) {
        if (self.deathstring == "") self.deathstring = " was stung by a Scorpion\n";
    } else {
        if (self.deathstring == "") self.deathstring = " was jabbed by a Scorpion\n";
    }
    
    self.gib1skin = self.gib2skin = self.gib3skin = self.exactskin;
    if(!self.classtype) self.classtype = CT_MONSCORPION;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};

//----------------------------------------------------------------------------
// A code way to spawn scorpion (requires monster_scorpionspawn entity)
//----------------------------------------------------------------------------
void(vector minion_org, entity minion_targ) minion_scorpion =
{
    local entity minion;
    
    // Check if there is space to spawn entity
    if (entity_pcontent(minion_org)) return;
    
    update_minioncount(self.owner, 1);        // Update spawn counters
    
    minion = spawn();
    minion.classname = "monster_scorpion";    // For function searching
    setorigin(minion, minion_org);            // Move to new location
    minion.owner = self.owner;                // Spawner Parent Link
    self.owner = minion;                    // Stop gibs interacting with minion
    
    minion.effects = minion.flags = 0;        // make sure are blank
    minion.gibondeath = 1;                    // Always gib on death
    minion.classtype = CT_MINIONSCORPION;    // Special minion class
    minion.enemy = minion_targ;                // Target to attack
    minion.minion_active = TRUE;            // Minion flag
    minion.bodyfadeaway = TRUE;                // Get rid of body
    
    minion.spawnflags = 0;
    
    minion.mdl = "progs/ad171/mon_scorpion.mdl";
    minion.headmdl = "progs/ad171/h_scorpion.mdl";        // Front Jaws
    minion.gib1mdl = "progs/ad171/gib_scorpleg.mdl";    // Single Leg
    minion.gib2mdl = "progs/ad171/gib_scorptail.mdl";    // Tail + Sting
    minion.gib3mdl = "progs/ad171/gib_scorpclaw.mdl";    // Claw
    minion.exactskin = rint((random()*3)-0.5);
    minion.gibhealth = -20;
    minion.health = 80;

    minion.idle_sound = "scorpion/warning1.wav";
    minion.idle_sound2 = "scorpion/warning2.wav";
    minion.sight_sound = "scorpion/hiss.wav";

    minion.pain_sound = "spider/pain1.wav";
    minion.pain_sound2 = "spider/pain2.wav";
    
    minion.nextthink = time + 0.01;
    minion.think = setup_scorpion;
};