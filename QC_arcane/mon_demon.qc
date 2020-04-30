/*==============================================================================
DEMON
==============================================================================*/
$cd id1/models/demon3
$scale    0.8
$origin 0 0 24
$base base
$skin base

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9
$frame stand10 stand11 stand12 stand13

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8

$frame run1 run2 run3 run4 run5 run6

$frame leap1 leap2 leap3 leap4 leap5 leap6 leap7 leap8 leap9 leap10
$frame leap11 leap12

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

$frame attacka1 attacka2 attacka3 attacka4 attacka5 attacka6 attacka7 attacka8
$frame attacka9 attacka10 attacka11 attacka12 attacka13 attacka14 attacka15

//============================================================================
void()    demon1_stand1    =[    $stand1,    demon1_stand2    ] {monster_idle_sound(); ai_stand();};
void()    demon1_stand2    =[    $stand2,    demon1_stand3    ] {ai_stand();};
void()    demon1_stand3    =[    $stand3,    demon1_stand4    ] {ai_stand();};
void()    demon1_stand4    =[    $stand4,    demon1_stand5    ] {ai_stand();};
void()    demon1_stand5    =[    $stand5,    demon1_stand6    ] {ai_stand();};
void()    demon1_stand6    =[    $stand6,    demon1_stand7    ] {ai_stand();};
void()    demon1_stand7    =[    $stand7,    demon1_stand8    ] {ai_stand();};
void()    demon1_stand8    =[    $stand8,    demon1_stand9    ] {ai_stand();};
void()    demon1_stand9    =[    $stand9,    demon1_stand10    ] {ai_stand();};
void()    demon1_stand10    =[    $stand10,    demon1_stand11    ] {ai_stand();};
void()    demon1_stand11    =[    $stand11,    demon1_stand12    ] {ai_stand();};
void()    demon1_stand12    =[    $stand12,    demon1_stand13    ] {ai_stand();};
void()    demon1_stand13    =[    $stand13,    demon1_stand1    ] {ai_stand();};

//============================================================================
void()    demon1_walk1    =[    $walk1,        demon1_walk2    ] {
    monster_footstep(FALSE); monster_idle_sound(); ai_walk(8);};
void()    demon1_walk2    =[    $walk2,        demon1_walk3    ] {ai_walk(6);};
void()    demon1_walk3    =[    $walk3,        demon1_walk4    ] {ai_walk(6);};
void()    demon1_walk4    =[    $walk4,        demon1_walk5    ] {ai_walk(7);};
void()    demon1_walk5    =[    $walk5,        demon1_walk6    ] {monster_footstep(FALSE); ai_walk(4);};
void()    demon1_walk6    =[    $walk6,        demon1_walk7    ] {ai_walk(6);};
void()    demon1_walk7    =[    $walk7,        demon1_walk8    ] {ai_walk(10);};
void()    demon1_walk8    =[    $walk8,        demon1_walk1    ] {ai_walk(10);};

//============================================================================
void()    demon1_run1    =[    $run1,        demon1_run2    ] {monster_idle_sound(); ai_run(20);};
void()    demon1_run2    =[    $run2,        demon1_run3    ] {monster_footstep(FALSE); ai_run(15);};
void()    demon1_run3    =[    $run3,        demon1_run4    ] {ai_run(36);};
void()    demon1_run4    =[    $run4,        demon1_run5    ] {ai_run(20);};
void()    demon1_run5    =[    $run5,        demon1_run6    ] {monster_footstep(FALSE); ai_run(15);};
void()    demon1_run6    =[    $run6,        demon1_run1    ] {ai_run(36);};

//===========================================================================
// Melee damage function, called from animation frame set
// Standardized layout (matching ai_melee (ai.qc)
// Added each claw attack checks enemy distance (before no check)
// Added z depth check on attacks +/- 64 units
// Added claw miss sound (borrowed HK slash sound)
// Changed some explicit values to constant variables
//===========================================================================
void(float side) Demon_Claw =
{
    local float     ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    ai_face ();                        // Turn towards enemy target
    walkmove (self.ideal_yaw, 12);    // Get extra close to enemy target
    ai_damagebreakable(20);            // Damage any breakables

    // Moved the check for melee distance to here with takedamage
    // If the demon is outside of melee range then need miss swipe sound
    // same goes for enemies that cannot be damaged (world geo mostly)
    if (!ai_checkmelee(MONAI_MELEEDEMON) || !self.enemy.takedamage) {
        // Melee claw miss sound
        if (random() < 0.5) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    }
    else {
        // Check for poisonous attribute (new poison version)
        if (self.poisonous) PoisonDeBuff(self.enemy);

        // Melee claw hit sound
        sound (self, CHAN_WEAPON, "demon/dhit2.wav", 1, ATTN_NORM);
        ldmg = 10 + 5*random();
        T_Damage (self.enemy, self, self, ldmg, DAMARMOR);        
        SpawnMeatSpray (self, self.enemy, side);
    }
};

//----------------------------------------------------------------------
void()    demon1_atta1    =[    $attacka1,        demon1_atta2    ] {ai_charge(4);};
void()    demon1_atta2    =[    $attacka2,        demon1_atta3    ] {monster_footstep(FALSE); ai_charge(0);};
void()    demon1_atta3    =[    $attacka3,        demon1_atta4    ] {ai_charge(0);};
void()    demon1_atta4    =[    $attacka4,        demon1_atta5    ] {ai_charge(1);};
void()    demon1_atta5    =[    $attacka5,        demon1_atta6    ] {ai_charge(2); Demon_Claw(150);};
void()    demon1_atta6    =[    $attacka6,        demon1_atta7    ] {ai_charge(1);};
void()    demon1_atta7    =[    $attacka7,        demon1_atta8    ] {ai_charge(6);};
void()    demon1_atta8    =[    $attacka8,        demon1_atta9    ] {ai_charge(8);};
void()    demon1_atta9    =[    $attacka9,        demon1_atta10] {ai_charge(4);};
void()    demon1_atta10    =[    $attacka10,        demon1_atta11] {monster_footstep(FALSE); ai_charge(2);};
void()    demon1_atta11    =[    $attacka11,        demon1_atta12] {Demon_Claw(-150);};
void()    demon1_atta12    =[    $attacka12,        demon1_atta13] {ai_charge(5);};
void()    demon1_atta13    =[    $attacka13,        demon1_atta14] {ai_charge(8);};
void()    demon1_atta14    =[    $attacka14,        demon1_atta15] {ai_charge(4);};
void()    demon1_atta15    =[    $attacka15,        demon1_run1] {ai_charge(4);};

//===========================================================================
// JumpTouch setup from animation frame set
// Changed so that it does not keep going in a loop
// * Demon will jump again if stuck (only 2 jumps)
//===========================================================================
void() Demon_JumpTouch =
{
    local float ldmg;

    if (self.health < 1) return;
    ai_jumpbreakable(50);                // Damage any breakables
    self.touch = SUB_Null;                // No more touching
    self.count = self.count + 1;        // Total amount of touch jumps
    self.think = self.th_jumpexit;        // Exit frame

    // Keep track of how many times touched the same object
    if (self.jumptouch == other) self.jump_flag = time + MONAI_JUMPTIMEOUT;
    self.jumptouch = other;                // Keep track of touch target

    // Do not damage other demons with jump attacks
    if (self.classtype != other.classtype && other.takedamage) {
        if ( vlen(self.velocity) > 300 ) {
            ldmg = 40 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            SpawnMeatSpray(self, other, FALSE);
            SpawnMeatSpray(self, other, FALSE);
            SpawnMeatSpray(self, other, FALSE);
            
            // Check for poisonous attribute (new poison version)
            if (self.poisonous) PoisonDeBuff(self.enemy);
        }
    }

    // Is the demon floating in the air?
    if (!checkbottom(self)) {
        // Is the demon standing on something?
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void()    demon1_jump1    =[    $leap1,        demon1_jump2    ] {ai_face();
    self.jump_flag = time;    // No jump time limits
    sound (self, CHAN_VOICE, "demon/djump.wav", 1, ATTN_NORM);
};
void()    demon1_jump2    =[    $leap2,        demon1_jump3    ] {ai_face();};
void()    demon1_jump3    =[    $leap3,        demon1_jump4    ] {ai_face();};
void()    demon1_jump4    =[    $leap4,        demon1_jump5    ] {
    ai_face();
    self.jump_flag = time;    // No jump time limits
    self.touch = Demon_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * 600 + '0 0 250';
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void()    demon1_jump5    =[    $leap5,        demon1_jump6    ] {};
void()    demon1_jump6    =[    $leap6,        demon1_jump7    ] {};
void()    demon1_jump7    =[    $leap7,        demon1_jump8    ] {};
void()    demon1_jump8    =[     $leap8,        demon1_jump9    ] {};
void()    demon1_jump9    =[     $leap9,        demon1_jump10    ] {};
void()    demon1_jump10    =[     $leap10,    demon1_jump1    ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360; //random jump angle
        self.think = demon1_jump3;
    }
    self.oldorigin = self.origin;
};
//----------------------------------------------------------------------
void()    demon1_jump11    =[     $leap11,    demon1_jump12] {monster_footstep(FALSE); };
void()    demon1_jump12    =[     $leap12,    demon1_run1    ] {ai_resetangles();};

/*======================================================================
  PAIN
========================================================================*/
void()    demon1_pain1    =[    $pain1,        demon1_pain2    ] {};
void()    demon1_pain2    =[    $pain2,        demon1_pain3    ] {};
void()    demon1_pain3    =[    $pain3,        demon1_pain4    ] {};
void()    demon1_pain4    =[    $pain4,        demon1_pain5    ] {};
void()    demon1_pain5    =[    $pain5,        demon1_pain6    ] {};
void()    demon1_pain6    =[    $pain6,        demon1_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) demon1_pain =
{
    if (self.touch == Demon_JumpTouch) return;

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) demon1_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.6;
            self.axhitme = 0;
            demon1_pain1 ();
        }
    }
};

//============================================================================
void()    demon1_death1    =[    $death1, demon1_death2 ] {};
void()    demon1_death2    =[    $death2, demon1_death3 ] {monster_check_gib();};
void()    demon1_death3    =[    $death3, demon1_death4 ] {monster_check_gib();
self.solid = SOLID_NOT;};
void()    demon1_death4    =[    $death4, demon1_death5 ] {};
void()    demon1_death5    =[    $death5, demon1_death6 ] {};
void()    demon1_death6    =[    $death6, demon1_death7 ] {};
void()    demon1_death7    =[    $death7, demon1_death8 ] {};
void()    demon1_death8    =[    $death8, demon1_death9 ] {monster_death_postcheck();};
void()    demon1_death9    =[    $death9, demon1_death9 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() demon1_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "demon/ddeath.wav", 1, ATTN_NORM);
        demon1_death1 ();
    }
};

/*======================================================================
QUAKED monster_demon1 (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_demon1 =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_demon.mdl";
    self.headmdl = "progs/h_demon.mdl";
    self.gib1mdl = "progs/gib_dmleg1.mdl";        // Left leg
    self.gib2mdl = "progs/gib_dmleg2.mdl";        // Right leg
    self.gib3mdl = "progs/gib_dmtail.mdl";        // Tail
    self.gib4mdl = "progs/gib_dmclaw1.mdl";        // Claw 1
    self.gib5mdl = "progs/gib_dmclaw2.mdl";        // Claw 2

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);        // Always precache extra models
    precache_model (self.gib5mdl);        // regardless if picked or not
    
    // Randomly swap in demon claws instead of legs
    if (random() < 0.5) self.gib1mdl = self.gib4mdl;
    if (random() < 0.5) self.gib2mdl = self.gib5mdl;
    self.gib4mdl = ""; self.gib5mdl = "";

    self.idle_sound = "demon/idle1.wav";
    precache_sound (self.idle_sound);

    self.pain_sound = "demon/dpain1.wav";
    precache_sound (self.pain_sound);
    precache_sound ("demon/ddeath.wav");

    precache_sound ("demon/dhit2.wav");
    precache_sound ("demon/djump.wav");
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");

    self.sight_sound = "demon/sight2.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        self.exactskin = 1;
        self.gib1skin = self.gib2skin = self.gib3skin = 1;
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.idmins = VEC_HULL2_MIN;    // -32 -32 -24, 32 32 64
    self.idmaxs = VEC_HULL2_MAX;
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    if (self.health < 1) self.health = 300;
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 200;                // takes alot to pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 2;                // High pain threshold
    self.steptype = FS_TYPESLOW;        // Tiny feet/nails
    self.blockudeath = TRUE;            // no humanoid death sound
    self.meleeoffset = '32 0 10';        // Claw attack offset
    self.deathstring = " was eviscerated by a Fiend\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = DemonCheckAttack;
    self.th_stand = demon1_stand1;
    self.th_walk = demon1_walk1;
    self.th_run = demon1_run1;
    self.th_melee = demon1_atta1;        // one of two attacks
    self.th_jump = demon1_jump1;        // jump attack
    self.th_jumpexit = demon1_jump11;
    self.th_pain = demon1_pain;
    self.th_die = demon1_die;

    self.classtype = CT_MONDEMON;
    self.classgroup = CG_DEMON;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
