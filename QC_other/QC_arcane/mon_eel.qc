/*==============================================================================
EEL by Rogue Entertainment
==============================================================================*/
$frame eswim1 eswim2 eswim3 eswim4 eswim5 eswim6

$frame edeath1 edeath2 edeath3 edeath4 edeath5 edeath6 edeath7 edeath8
$frame edeath9 edeath10 edeath11 edeath12 edeath13 edeath14 edeath15

float EEL_STAND = 1;
float EEL_WALK = 2;
float EEL_RUN = 4;

//=============================================================================
void() eel_swim = {
    // Do nothing if dead
    if (self.health < 1) return;
    
    self.skin = 0;        // Reset skin if come from combat
    self.nextthink = time + 0.1;
    if (self.walkframe == 1) monster_idle_sound();

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 5) self.walkframe = 0;
    self.frame = $eswim1 + self.walkframe;
    
    if (self.state == EEL_STAND) ai_stand();
    else if (self.state == EEL_WALK) ai_walk(6);
    else ai_run(10);
};

//----------------------------------------------------------------------
void() eel_stand = [ $eswim1, eel_swim ] {
    self.walkframe = 0; self.state = EEL_STAND; eel_swim();};
void() eel_walk = [ $eswim1, eel_swim ] {
    self.walkframe = 0; self.state = EEL_WALK; eel_swim();};
void() eel_run = [ $eswim1, eel_swim ] {
    self.walkframe = 0; self.state = EEL_RUN; eel_swim();};

//============================================================================
// RANGE - Plasma from mouth
//============================================================================
void() eel_plasma =
{
    local vector org, dir;

    // Check if can fire bolt of plasma
    if (visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) return;    
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_EELBOLT + (skill * SPEED_EELSKILL);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(self.enemy.origin - self.origin);

    launch_plasma(org, dir, CT_MONEEL, self.attack_speed);
};

//----------------------------------------------------------------------
void() eel_attack1 = [ $eswim1, eel_attack2 ] {self.skin = 1;ai_face();
    sound (self, CHAN_VOICE, "eel/attack1.wav", 1, ATTN_NORM);};
void() eel_attack2 = [ $eswim2, eel_attack3 ] {self.skin = 2;ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;};
void() eel_attack3 = [ $eswim3, eel_attack4 ] {self.skin = 3;ai_face();};
void() eel_attack4 = [ $eswim4, eel_attack5 ] {self.skin = 4;ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;};
void() eel_attack5 = [ $eswim5, eel_attack6 ] {self.skin = 5;ai_face();};
void() eel_attack6 = [ $eswim6, eel_attack7 ] {
    self.skin = 4; ai_face(); eel_plasma();};
void() eel_attack7 = [ $eswim1, eel_attack8 ] {self.skin = 5;};
void() eel_attack8 = [ $eswim2, eel_attack9 ] {self.skin = 4;};
void() eel_attack9 = [ $eswim3, eel_attack10 ] {self.skin = 3;};
void() eel_attack10 = [ $eswim4, eel_attack11 ] {self.skin = 2;};
void() eel_attack11 = [ $eswim5, eel_attack12 ] {self.skin = 1;ai_face();};
void() eel_attack12 = [ $eswim6, eel_run ] {self.skin = 0;ai_face();};

//=============================================================================
// Pain & Death
//=============================================================================
void() eel_pain1 = [ $edeath1, eel_pain2] {};
void() eel_pain2 = [ $edeath2, eel_pain3] {};
void() eel_pain3 = [ $edeath3, eel_pain4] {};
void() eel_pain4 = [ $edeath4, eel_pain5] {};
void() eel_pain5 = [ $edeath3, eel_pain6] {};
void() eel_pain6 = [ $edeath2, eel_pain7] {};
void() eel_pain7 = [ $edeath1, eel_run] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) eel_pain =
{
    // Reset combat skin
    self.skin = 0;

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        eel_pain1();
    }
};

//=============================================================================
void() eel_die1 = [ $edeath1, eel_die2 ] {};
void() eel_die2 = [ $edeath2, eel_die3 ] {monster_check_gib();};
void() eel_die3 = [ $edeath3, eel_die4 ] {monster_check_gib();self.solid = SOLID_NOT;};
void() eel_die4 = [ $edeath4, eel_die5 ] {
    // make sure the eel drops to the ground
    self.movetype = MOVETYPE_TOSS;
    self.velocity_z = 10 + random()*10;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
};
void() eel_die5 = [ $edeath5, eel_die6 ] {};
void() eel_die6 = [ $edeath6, eel_die7 ] {};
void() eel_die7 = [ $edeath7, eel_die8 ] {};
void() eel_die8 = [ $edeath8, eel_die9 ] {};
void() eel_die9 = [ $edeath9, eel_die10 ] {};
void() eel_die10 = [ $edeath10, eel_die11 ] {};
void() eel_die11 = [ $edeath11, eel_die12 ] {};
void() eel_die12 = [ $edeath12, eel_die13 ] {};
void() eel_die13 = [ $edeath13, eel_die14 ] {};
void() eel_die14 = [ $edeath14, eel_die15 ] {monster_death_postcheck();};
void() eel_die15 = [ $edeath15, eel_die15 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() eel_die =
{
    // Reset combat skin
    self.skin = 0;
    
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        // reduce self gravity to pretend to slowly sink
        if (pointcontents(self.origin) == CONTENT_WATER) self.gravity = 0.25;

        sound (self, CHAN_BODY, "eel/death1.wav", 1, ATTN_NORM);

        // Death animation not at bottom of bounding box
        self.bbmins = '-12 -12 -1';
        self.bbmaxs = '12 12 12';
        setsize(self, self.bbmins, self.bbmaxs);
        
        // With the bounding box changed for the animation the
        // gib system now needs an offset to cope!
        self.max_health = MON_GIBFOUNTAIN;
        self.gib_ofs = '0 0 32';
        // Wiggle and keel over
        eel_die1 ();
    }
};

/*======================================================================
/*QUAKED monster_eel (1 0 0) (-16 -16 -16) (16 16 16)
======================================================================*/
void() monster_eel =
{
    // ------------------------------------------------------------------
    // ** Removed from MOD **
    // Uses copyright content from Rogue Software & not fun to play with!
    // This script works perfectly fine with Rogue Software assets
    // Anyone can use this monster for their own MOD!
    // ------------------------------------------------------------------
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_eel.mdl";
    self.headmdl = "progs/h_eel.mdl";
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_PLASMA);        // Plasma spike/bolt

    self.idle_sound = "eel/idle1.wav";
    precache_sound (self.idle_sound);

    precache_sound ("eel/attack1.wav");
    precache_sound ("eel/death1.wav");
    self.pain_sound = "eel/pain1.wav";
    precache_sound (self.pain_sound);

    self.sight_sound = "eel/sight1.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_PLASMAGRN);
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_EEL;
    if (self.health < 1) self.health = 35;
    self.gibhealth = -25;
    self.gibbed = FALSE;
    self.pain_flinch = 10;            // Easy response to pain
    self.steptype = FS_FLYING;        // It has no sea legs!
    self.blockudeath = TRUE;        // No humanoid death sound
    self.attack_offset = '20 0 -2';    // Plasma start location
    self.deathstring = " was electrified by an Eel\n";

    // Check for water sightline wakeup
    if (self.spawnflags & MON_LIQUIDBLOCK) self.liquidblock = TRUE;
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = EelCheckAttack;
    self.th_stand = eel_stand;
    self.th_walk = eel_walk;
    self.th_run = eel_run;
    self.th_pain = eel_pain;
    self.th_die = eel_die;
    self.th_missile = eel_attack1;

    self.classtype = CT_MONEEL;
    self.classgroup = CG_FISH;
    self.classmove = MON_MOVESWIM;
    
    monster_start();
};