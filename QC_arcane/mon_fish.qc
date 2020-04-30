/*==============================================================================
FISH FISH FISHY! (remade version by ijed)
==============================================================================*/

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7
$frame attack8 attack9 attack10 attack11 attack12 attack13 attack14
$frame attack15 attack16 attack17 attack18 

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 

$frame swim1 swim2 swim3 swim4 swim5 swim6 swim7 swim8 
$frame swim9 swim10 swim11 swim12 swim13 swim14 swim15 swim16
$frame swim17 swim18 

$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8 pain9

// Duplicate set of animations for a smaller scaled down version
$frame attackS1 attackS2 attackS3 attackS4 attackS5 attackS6 attackS7
$frame attackS8 attackS9 attackS10 attackS11 attackS12 attackS13 attackS14
$frame attackS15 attackS16 attackS17 attackS18 

$frame deathS1 deathS2 deathS3 deathS4 deathS5 deathS6 deathS7 deathS8
$frame deathS9 deathS10 deathS11 deathS12 deathS13 deathS14 deathS15 deathS16
$frame deathS17 deathS18 deathS19 deathS20 deathS21 

$frame swimS1 swimS2 swimS3 swimS4 swimS5 swimS6 swimS7 swimS8 
$frame swimS9 swimS10 swimS11 swimS12 swimS13 swimS14 swimS15 swimS16
$frame swimS17 swimS18 

$frame painS1 painS2 painS3 painS4 painS5 painS6 painS7 painS8 painS9

float FISH_STAND = 1;
float FISH_WALK = 2;
float FISH_RUN = 4;

//============================================================================
void() f_swim = {
    // Do nothing if dead
    if (self.health < 1) return;
    
    self.nextthink = time + 0.1;
    if (self.walkframe == 1) monster_idle_sound();

    // Keep moving the animation frame forward
    if (self.state == FISH_RUN) self.walkframe = self.walkframe + 2;
    else self.walkframe = self.walkframe + 1;
    // Setup small/large frames
    if (self.spawnflags & MON_FISH_SMALL)
        self.frame = $swimS1 + self.walkframe;
    else self.frame = $swim1 + self.walkframe;
    // Loop around animation block
    if (self.walkframe > 17) self.walkframe = 0;

    if (self.state == FISH_STAND) ai_stand();
    else if (self.state == FISH_WALK) ai_walk(self.move_state_y);
    else ai_run(self.move_state_z);
};

//----------------------------------------------------------------------
void() f_stand1 = [ $swim1, f_swim ] { ai_stand();};
void() f_standS1 = [ $swimS1, f_swim ] { ai_stand(); };

void() f_stand = { 
    self.walkframe = 0; self.state = FISH_STAND; 
    if (self.spawnflags & MON_FISH_SMALL) f_standS1();
    else f_stand1();
};
    
//----------------------------------------------------------------------
void() f_walk1 = [ $swim1, f_swim ] { ai_walk(self.move_state_y);};
void() f_walkS1 = [ $swimS1, f_swim ] { ai_walk(self.move_state_y);};

void() f_walk = {
    self.walkframe = 0; self.state = FISH_WALK; 
    if (self.spawnflags & MON_FISH_SMALL) f_walkS1();
    else f_walk1();
};

//----------------------------------------------------------------------
void() f_run1 = [ $swim1, f_swim ] { ai_run(self.move_state_z);};
void() f_runS1 = [ $swimS1, f_swim ] { ai_run(self.move_state_z);};

void() f_run = {
    self.walkframe = 0; self.state = FISH_RUN; 
    if (self.spawnflags & MON_FISH_SMALL) f_runS1();
    else f_run1();
};

//============================================================================
void() fish_melee =
{
    local float ldmg;

    if (!self.enemy) return;                // removed before stroke
    ai_damagebreakable(5);                    // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEEFISH)) return;    // Too far away
        
    // Fish bites man! 1-6 Damage
    ldmg = (random() + random() + random()) * 2;
    if (ldmg < 1) ldmg = 1;
    sound (self, CHAN_VOICE, "fish/bite.wav", 1, ATTN_NORM);
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    
    // Check for poisonous attribute (new poison version)
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // Spawn some touch blood (no explicit direction)
    spawn_touchblood (self, self.enemy, ldmg*3);
};

//----------------------------------------------------------------------
void() f_attack2 = {
    // Do nothing if dead
    if (self.health < 1) return;
    self.nextthink = time + 0.1;

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.spawnflags & MON_FISH_SMALL)
        self.frame = $attackS1 + self.walkframe;
    else self.frame = $attack1 + self.walkframe;
    
    // Setup special frame attacks
    if (self.walkframe > 17) self.think = self.th_run;
    if (self.walkframe == 2 || self.walkframe == 8 || self.walkframe == 14) 
        fish_melee();
    else ai_charge(self.move_state_y);
};

//----------------------------------------------------------------------
void() f_attack1 = [ $attack1, f_attack2 ] {};
void() f_attackS1 = [ $attackS1, f_attack2 ] {};

void() f_attack = {
    self.walkframe = 0;
    ai_charge(self.move_state_y);
    if (self.spawnflags & MON_FISH_SMALL) f_attackS1();
    else f_attack1();
};

//============================================================================
void() f_pain2 = {
    self.nextthink = time + 0.1;
    
    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.spawnflags & MON_FISH_SMALL)
        self.frame = $painS1 + self.walkframe;
    else self.frame = $pain1 + self.walkframe;
    
    if (self.walkframe > 8) self.think = self.th_run;
    if (self.walkframe > 1) ai_pain(self.move_state_y/2);    
};

void() f_pain1 = [ $pain1, f_pain2 ] {};
void() f_painS1 = [ $painS1, f_pain2 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) fish_pain =
{
    if (self.health < 1) return;
    self.walkframe = 0; 
    if (self.spawnflags & MON_FISH_SMALL) f_painS1();
    else f_pain1();
};

//============================================================================
void() f_deathend = { monster_deadbody_check(); };

//----------------------------------------------------------------------
void() f_death2 = {
    self.nextthink = time + 0.1;
    
    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.spawnflags & MON_FISH_SMALL)
        self.frame = $deathS1 + self.walkframe;
    else self.frame = $death1 + self.walkframe;
    
    // Keep moving the animation frame forward
    if (self.walkframe < 3) monster_check_gib();
    if (self.walkframe > 2) self.solid = SOLID_NOT;
    if (self.walkframe == 19) monster_death_postcheck();
    if (self.walkframe > 19) f_deathend();
};

void() f_death1 = [ $death1, f_death2 ] {};
void() f_deathS1 = [ $deathS1, f_death2 ] {};

//----------------------------------------------------------------------
void() fish_die =
{
    // If fish in water, reduce self gravity to pretend to slowly sink
    if (pointcontents(self.origin) == CONTENT_WATER) self.gravity = 0.25;

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        self.walkframe = 0;
        sound (self, CHAN_VOICE, "fish/death.wav", 1, ATTN_NORM);
        if (self.spawnflags & MON_FISH_SMALL) f_deathS1();
        else f_death1();
    }
};

/*======================================================================
QUAKED monster_fish (1 0 0) (-16 -16 -24) (16 16 24) Ambush
======================================================================*/
void() monster_fish =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_fish.mdl";    // Using new model
    precache_model (self.mdl);

    self.idle_sound = "fish/idle.wav";
    self.sight_sound = self.idle_sound;
    precache_sound (self.idle_sound);

    precache_sound ("fish/death.wav");
    precache_sound ("fish/bite.wav");

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.gibhealth = -10;
    self.gibbed = FALSE;
    self.pain_flinch = 1;                // Always in pain
    self.steptype = FS_FLYING;            // It has fins!?!
    self.blockudeath = TRUE;            // No humanoid death sound
    self.meleeoffset = '24 0 -8';        // Bite attack offset
    self.height = 0;                    // reset enemytarget tracking
    if (!self.exactskin) self.randomskin = 4;
    self.deathstring = " was fed to the Rotfish\n";

    // Check for water sightline wakeup
    if (self.spawnflags & MON_LIQUIDBLOCK) self.liquidblock = TRUE;
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Tiny fish are easier to kill
    if (self.spawnflags & MON_FISH_SMALL) {
        if (self.bboxtype < 1) self.bboxtype = BBOX_FISHS;
        if (self.health < 1) self.health = 15;
        // The small fish moving really fast looks odd
        // reverted speed and movement back to original
        //self.move_state = '0 12 16';
        self.move_state = '0 8 12';
    }
    else {
        if (self.bboxtype < 1) self.bboxtype = BBOX_FISH;
        if (self.health < 1) self.health = 25;
        self.move_state = '0 8 12';
    }
    
    self.th_checkattack = FishCheckAttack;
    self.th_stand = f_stand;
    self.th_walk = f_walk;
    self.th_run = f_run;
    self.th_die = fish_die;
    self.th_pain = fish_pain;
    self.th_melee = f_attack;
    
    self.classtype = CT_MONFISH;
    self.classgroup = CG_FISH;
    self.classmove = MON_MOVESWIM;
    
    monster_start();
};

