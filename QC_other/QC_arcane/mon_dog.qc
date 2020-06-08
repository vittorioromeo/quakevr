/*==============================================================================
DOG
==============================================================================*/
$cd id1/models/dog
$origin 0 0 24
$base base
$skin skin

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8 deathb9

$frame pain1 pain2 pain3 pain4 pain5 pain6
$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8
$frame painb9 painb10 painb11 painb12 painb13 painb14 painb15 painb16

$frame run1 run2 run3 run4 run5 run6 run7 run8 run9 run10 run11 run12

$frame leap1 leap2 leap3 leap4 leap5 leap6 leap7 leap8 leap9

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8

// Idle 1 - Growling
$frame idlea1 idlea2 idlea3 idlea4 idlea5 idlea6 idlea7 idlea8
$frame idlea9 idlea10 idlea11 idlea12 idlea13 idlea14 idlea15 idlea16
$frame idlea17 idlea18

// Idle 2 - Sniffing ground
$frame idleb1 idleb2 idleb3 idleb4 idleb5 idleb6 idleb7 idleb8
$frame idleb9 idleb10 idleb11 idleb12 idleb13 idleb14

// Walk B - walking and sniffing
$frame walkb1 walkb2 walkb3 walkb4 walkb5 walkb6 walkb7 walkb8

// Idle 3 - Howling at the moon
$frame idlec1 idlec2 idlec3 idlec4 idlec5 idlec6 idlec7 idlec8
$frame idlec9 idlec10 idlec11 idlec12 idlec13 idlec14

void() dog_walkb1;

//============================================================================
void() dog_idlea1    =[    $idlea1,    dog_idlea2    ] {self.idlebusy = TRUE; 
    monster_idle_sound(); ai_stand();};
void() dog_idlea2    =[    $idlea2,    dog_idlea3    ] {ai_stand();};
void() dog_idlea3    =[    $idlea3,    dog_idlea4    ] {ai_stand();};
void() dog_idlea4    =[    $idlea4,    dog_idlea5    ] {ai_stand();};
void() dog_idlea5    =[    $idlea5,    dog_idlea6    ] {ai_stand();};
void() dog_idlea6    =[    $idlea6,    dog_idlea7    ] {ai_stand();};
void() dog_idlea7    =[    $idlea7,    dog_idlea8    ] {ai_stand();};
void() dog_idlea8    =[    $idlea8,    dog_idlea9    ] {ai_stand();};
void() dog_idlea9    =[    $idlea9,    dog_idlea10    ] {ai_stand();};
void() dog_idlea10=[    $idlea10,    dog_idlea11    ] {ai_stand();};
void() dog_idlea11=[    $idlea11,    dog_idlea12    ] {ai_stand();};
void() dog_idlea12=[    $idlea12,    dog_idlea13    ] {ai_stand();};
void() dog_idlea13=[    $idlea13,    dog_idlea14    ] {ai_stand();};
void() dog_idlea14=[    $idlea14,    dog_idlea15    ] {ai_stand();};
void() dog_idlea15=[    $idlea15,    dog_idlea16    ] {ai_stand();};
void() dog_idlea16=[    $idlea16,    dog_idlea17    ] {ai_stand();};
void() dog_idlea17=[    $idlea17,    dog_idlea18    ] {ai_stand();};
void() dog_idlea18=[    $idlea18,    dog_stand1    ] {ai_stand();};

//============================================================================
void() dog_idleb1    =[    $idleb1,    dog_idleb2    ] {self.idlebusy = TRUE; 
    monster_idle_sound(); ai_stand();};
void() dog_idleb2    =[    $idleb2,    dog_idleb3    ] {ai_stand();};
void() dog_idleb3    =[    $idleb3,    dog_idleb4    ] {ai_stand();};
void() dog_idleb4    =[    $idleb4,    dog_idleb5    ] {ai_stand();};
void() dog_idleb5    =[    $idleb5,    dog_idleb6    ] {ai_stand();};
void() dog_idleb6    =[    $idleb6,    dog_idleb7    ] {ai_stand();};
void() dog_idleb7    =[    $idleb7,    dog_idleb8    ] {ai_stand();};
void() dog_idleb8    =[    $idleb8,    dog_idleb9    ] {ai_stand();};
void() dog_idleb9    =[    $idleb9,    dog_idleb10    ] {ai_stand();};
void() dog_idleb10=[    $idleb10,    dog_idleb11    ] {ai_stand();};
void() dog_idleb11=[    $idleb11,    dog_idleb12    ] {ai_stand();};
void() dog_idleb12=[    $idleb12,    dog_idleb13    ] {ai_stand();};
void() dog_idleb13=[    $idleb13,    dog_idleb14    ] {ai_stand();};
void() dog_idleb14=[    $idleb14,    dog_stand1    ] {ai_stand();};

//============================================================================
void() dog_idlec1    =[    $idlec1,    dog_idlec2    ] {self.idlebusy = TRUE; 
    monster_idle_sound(); ai_stand();};
void() dog_idlec2    =[    $idlec2,    dog_idlec3    ] {ai_stand();};
void() dog_idlec3    =[    $idlec3,    dog_idlec4    ] {ai_stand();};
void() dog_idlec4    =[    $idlec4,    dog_idlec5    ] {ai_stand();};
void() dog_idlec5    =[    $idlec5,    dog_idlec6    ] {ai_stand();};
void() dog_idlec6    =[    $idlec6,    dog_idlec7    ] {ai_stand();};
void() dog_idlec7    =[    $idlec7,    dog_idlec8    ] {ai_stand();};
void() dog_idlec8    =[    $idlec8,    dog_idlec9    ] {ai_stand();};
void() dog_idlec9    =[    $idlec9,    dog_idlec10    ] {ai_stand();};
void() dog_idlec10=[    $idlec10,    dog_idlec11    ] {ai_stand();};
void() dog_idlec11=[    $idlec11,    dog_idlec12    ] {ai_stand();};
void() dog_idlec12=[    $idlec12,    dog_idlec13    ] {ai_stand();};
void() dog_idlec13=[    $idlec13,    dog_idlec14    ] {ai_stand();};
void() dog_idlec14=[    $idlec14,    dog_stand1    ] {ai_stand();};

//============================================================================
void() dog_stand1    =[    $stand1,    dog_stand2    ] {self.idlebusy = FALSE; ai_stand();};
void() dog_stand2    =[    $stand2,    dog_stand3    ] {ai_stand();};
void() dog_stand3    =[    $stand3,    dog_stand4    ] {ai_stand();};
void() dog_stand4    =[    $stand4,    dog_stand5    ] {ai_stand();};
void() dog_stand5    =[    $stand5,    dog_stand6    ] {ai_stand();};
void() dog_stand6    =[    $stand6,    dog_stand7    ] {ai_stand();};
void() dog_stand7    =[    $stand7,    dog_stand8    ] {ai_stand();};
void() dog_stand8    =[    $stand8,    dog_stand9    ] {ai_stand();};
void() dog_stand9    =[    $stand9,    dog_stand1    ] {
    if (random() < MON_IDLE_ANIMATION) {
        self.lip = random();
        if (self.lip < 0.2) self.think = dog_idlec1;            // Howling at moon
        else if (self.lip < 0.4) self.think = dog_idleb1;    // sniff ground
        else self.think = dog_idlea1;                                            // Growling
    }
    ai_stand();
};

//============================================================================
void() dog_walk1    =[    $walk1 ,    dog_walk2    ] {monster_idle_sound(); ai_walk(8);};
void() dog_walk2    =[    $walk2 ,    dog_walk3    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walk3    =[    $walk3 ,    dog_walk4    ] {ai_walk(8);};
void() dog_walk4    =[    $walk4 ,    dog_walk5    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walk5    =[    $walk5 ,    dog_walk6    ] {ai_walk(8);};
void() dog_walk6    =[    $walk6 ,    dog_walk7    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walk7    =[    $walk7 ,    dog_walk8    ] {ai_walk(8);};
void() dog_walk8    =[    $walk8 ,    dog_walk1    ] {
    // Random chance the dog will sniff the ground when walking/patrolling
    if (random() < MON_IDLE_ANIMATION) self.think = dog_walkb1;
    monster_footstep(FALSE); ai_walk(8);
};

//----------------------------------------------------------------------
void() dog_walkb1    =[    $walkb1 ,    dog_walkb2    ] {self.idlebusy = TRUE; monster_idle_sound();ai_walk(8);};
void() dog_walkb2    =[    $walkb2 ,    dog_walkb3    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walkb3    =[    $walkb3 ,    dog_walkb4    ] {ai_walk(8);};
void() dog_walkb4    =[    $walkb4 ,    dog_walkb5    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walkb5    =[    $walkb5 ,    dog_walkb6    ] {ai_walk(8);};
void() dog_walkb6    =[    $walkb6 ,    dog_walkb7    ] {monster_footstep(FALSE); ai_walk(8);};
void() dog_walkb7    =[    $walkb7 ,    dog_walkb8    ] {ai_walk(8);};
void() dog_walkb8    =[    $walkb8 ,    dog_walk1    ] {monster_footstep(FALSE); ai_walk(8);};

//============================================================================
void() dog_run1        =[    $run1  ,    dog_run2    ] {monster_idle_sound(); ai_run(16);};
void() dog_run2        =[    $run2  ,    dog_run3    ] {monster_footstep(FALSE); ai_run(32);};
void() dog_run3        =[    $run3  ,    dog_run4    ] {ai_run(32);};
void() dog_run4        =[    $run4  ,    dog_run5    ] {monster_footstep(FALSE); ai_run(20);};
void() dog_run5        =[    $run5  ,    dog_run6    ] {ai_run(64);};
void() dog_run6        =[    $run6  ,    dog_run7    ] {ai_run(32);};
void() dog_run7        =[    $run7  ,    dog_run8    ] {monster_footstep(FALSE); ai_run(16);};
void() dog_run8        =[    $run8  ,    dog_run9    ] {ai_run(32);};
void() dog_run9        =[    $run9  ,    dog_run10    ] {ai_run(32);};
void() dog_run10    =[    $run10  ,    dog_run11    ] {monster_footstep(FALSE); ai_run(20);};
void() dog_run11    =[    $run11  ,    dog_run12    ] {ai_run(64);};
void() dog_run12    =[    $run12  ,    dog_run1    ] {ai_run(32);};

//===========================================================================
// melee - dog_bite
// Standardized layout (matching ai_melee (aisubs.qc)
// Added z depth check on attacks +/- 64 units
// Added extra blood (touch version, no arc)
// Changed some explicit values to constant variables
//===========================================================================
void() dog_bite =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
    
    ai_charge(10);                                // Get closer for extra bite
    ai_damagebreakable(10);                        // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEEDOG)) return;    // Too far away

    // Can the target bleed?
    if (!self.enemy.takedamage) return;
    
    // Dog bite is nasty damage (1-24)
    ldmg = (random() + random() + random()) * 8;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    
    // Check for poisonous attribute (new poison version)
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // Spawn some blood from mouth
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void() dog_atta1    =[    $attack1,    dog_atta2    ] {ai_charge(10);};
void() dog_atta2    =[    $attack2,    dog_atta3    ] {ai_charge(10);};
void() dog_atta3    =[    $attack3,    dog_atta4    ] {ai_charge(10);};
void() dog_atta4    =[    $attack4,    dog_atta5    ] {
sound (self, CHAN_VOICE, "dog/dattack1.wav", 1, ATTN_NORM);
dog_bite();};
void() dog_atta5    =[    $attack5,    dog_atta6    ] {ai_charge(10);};
void() dog_atta6    =[    $attack6,    dog_atta7    ] {ai_charge(10);};
void() dog_atta7    =[    $attack7,    dog_atta8    ] {monster_footstep(FALSE);ai_charge(10);};
void() dog_atta8    =[    $attack8,    dog_run1    ] {ai_charge(10);};

//============================================================================
// range - springy feet
//============================================================================
void() Dog_JumpTouch =
{
    local    float    ldmg;

    if (self.health < 1) return;
    ai_jumpbreakable(20);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame
    
    // Keep track of how many times touched the same object
    if (self.jumptouch == other) self.jump_flag = time + MONAI_JUMPTIMEOUT;
    self.jumptouch = other;            // Keep track of touch target

    // Do not damage other dogs with jump attacks
    // Prevents packs from killing themselves
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 10 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            // Spawn some touch blood (no explicit direction)
            spawn_touchblood (self.enemy, self.enemy, ldmg*3);

            // Check for poisonous attribute (new poison version)
            if (self.poisonous) PoisonDeBuff(self.enemy);
        }
    }

    // Is the dog floating in the air?
    if (!checkbottom(self)) {
        // Is the dog standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() dog_leap1    =[    $leap1,        dog_leap2    ] {ai_face();};
void() dog_leap2    =[    $leap2,        dog_leap3    ] {
    ai_face();
    self.jump_flag = time;    // No jump time limits
    self.touch = Dog_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * 300 + '0 0 200';
    self.origin_z = self.origin_z + 4;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void() dog_leap3    =[    $leap3,        dog_leap4    ] {};
void() dog_leap4    =[    $leap4,        dog_leap5    ] {};
void() dog_leap5    =[    $leap5,        dog_leap6    ] {};
void() dog_leap6    =[    $leap6,        dog_leap6    ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = dog_leap2;
    }
    self.oldorigin = self.origin;
};
void() dog_leap7    =[    $leap7,        dog_leap8    ] {monster_footstep(FALSE);};
void() dog_leap8    =[    $leap8,        dog_leap9    ] {monster_footstep(FALSE);};
void() dog_leap9    =[    $leap9,        dog_run1    ] {ai_resetangles();};

/*======================================================================
  PAIN
========================================================================*/
void() dog_pain1    =[    $pain1 ,    dog_pain2    ] {};
void() dog_pain2    =[    $pain2 ,    dog_pain3    ] {};
void() dog_pain3    =[    $pain3 ,    dog_pain4    ] {};
void() dog_pain4    =[    $pain4 ,    dog_pain5    ] {};
void() dog_pain5    =[    $pain5 ,    dog_pain6    ] {};
void() dog_pain6    =[    $pain6 ,    dog_run1    ] {monster_footstep(FALSE);};

//----------------------------------------------------------------------
void() dog_painb1    =[    $painb1 ,    dog_painb2    ] {};
void() dog_painb2    =[    $painb2 ,    dog_painb3    ] {};
void() dog_painb3    =[    $painb3 ,    dog_painb4    ] {ai_pain(4);};
void() dog_painb4    =[    $painb4 ,    dog_painb5    ] {ai_pain(12);};
void() dog_painb5    =[    $painb5 ,    dog_painb6    ] {ai_pain(12);};
void() dog_painb6    =[    $painb6 ,    dog_painb7    ] {ai_pain(2);};
void() dog_painb7    =[    $painb7 ,    dog_painb8    ] {};
void() dog_painb8    =[    $painb8 ,    dog_painb9    ] {ai_pain(4);};
void() dog_painb9    =[    $painb9 ,    dog_painb10    ] {};
void() dog_painb10    =[    $painb10 ,    dog_painb11    ] {ai_pain(10);};
void() dog_painb11    =[    $painb11 ,    dog_painb12    ] {monster_footstep(FALSE);};
void() dog_painb12    =[    $painb12 ,    dog_painb13    ] {monster_footstep(FALSE);};
void() dog_painb13    =[    $painb13 ,    dog_painb14    ] {};
void() dog_painb14    =[    $painb14 ,    dog_painb15    ] {};
void() dog_painb15    =[    $painb15 ,    dog_painb16    ] {};
void() dog_painb16    =[    $painb16 ,    dog_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    dog_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (random() > 0.5) dog_pain1 ();
            else {
                dog_painb1 ();                        // roll over
                self.pain_finished = time + 2;        // long animation
            }
        }
    }
};

//============================================================================
void() dog_die1        =[    $death1,    dog_die2    ] {};
void() dog_die2        =[    $death2,    dog_die3    ] {monster_check_gib();};
void() dog_die3        =[    $death3,    dog_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() dog_die4        =[    $death4,    dog_die5    ] {};
void() dog_die5        =[    $death5,    dog_die6    ] {};
void() dog_die6        =[    $death6,    dog_die7    ] {};
void() dog_die7        =[    $death7,    dog_die8    ] {};
void() dog_die8        =[    $death8,    dog_die9    ] {monster_death_postcheck();};
void() dog_die9        =[    $death9,    dog_die9    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dog_dieb1        =[    $deathb1,    dog_dieb2    ] {};
void() dog_dieb2        =[    $deathb2,    dog_dieb3    ] {monster_check_gib();};
void() dog_dieb3        =[    $deathb3,    dog_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() dog_dieb4        =[    $deathb4,    dog_dieb5    ] {};
void() dog_dieb5        =[    $deathb5,    dog_dieb6    ] {};
void() dog_dieb6        =[    $deathb6,    dog_dieb7    ] {};
void() dog_dieb7        =[    $deathb7,    dog_dieb8    ] {};
void() dog_dieb8        =[    $deathb8,    dog_dieb9    ] {monster_death_postcheck();};
void() dog_dieb9        =[    $deathb9,    dog_dieb9    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dog_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "dog/ddeath.wav", 1, ATTN_NORM);

        if (random() > 0.5) dog_die1 ();
        else dog_dieb1 ();
    }
};

/*======================================================================
/*QUAKED monster_dog (1 0 0) (-32 -32 -24) (32 32 40) Ambush
======================================================================*/
void() monster_dog =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_dog.mdl";
    self.headmdl = "progs/h_dog.mdl";
    precache_model (self.mdl);
    precache_model (self.headmdl);

    self.idle_sound = "dog/idle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("dog/dattack1.wav");
    precache_sound ("dog/ddeath.wav");
    self.pain_sound = "dog/dpain1.wav";
    precache_sound (self.pain_sound);

    self.sight_sound = "dog/dsight.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
    }

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.idmins = '-32 -32 -24';        // Original mega large bbox
    self.idmaxs = '32 32 40';
    if (self.bboxtype < 1) self.bboxtype = BBOX_DOG;
    if (self.health < 1) self.health = 25;
    self.gibhealth = -35;
    self.gibbed = FALSE;
    self.pain_flinch = 10;                // Easy response to pain
    self.steptype = FS_TYPESLOW;        // Tiny feet
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '24 0 10';        // Bite attack offset
    self.deathstring = " was mauled by a Rottweiler\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DogCheckAttack;
    self.th_stand = dog_stand1;
    self.th_walk = dog_walk1;
    self.th_run = dog_run1;
    self.th_pain = dog_pain;
    self.th_die = dog_die;
    self.th_melee = dog_atta1;
    self.th_jump = dog_leap1;
    self.th_jumpexit = dog_leap8;

    self.classtype = CT_MONDOG;
    self.classgroup = CG_DOG;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
