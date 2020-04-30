/*=============================================================================
CENTURION - Originally from Rubicon2 by John Fitzgibbons
==============================================================================*/
$cd id1/models/enforcer
$origin 0 -6 24
$base base        
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

$frame attack1 attack2 attack3 attack4 attack5 attack6
$frame attack7 attack8 attack9 attack10

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14

$frame fdeath1 fdeath2 fdeath3 fdeath4 fdeath5 fdeath6 fdeath7 fdeath8
$frame fdeath9 fdeath10 fdeath11

$frame paina1 paina2 paina3 paina4

$frame painb1 painb2 painb3 painb4 painb5

$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8

$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8
$frame paind9 paind10 paind11 paind12 paind13 paind14 paind15 paind16
$frame paind17 paind18 paind19

void() centurionhover =
{
    if (self.waitmin < time) {
        self.waitmin = time + 4 + random()*4;
        sound(self, CHAN_ITEM, "enforcer/cent_hover.wav", 1, ATTN_IDLE);
    }
};

//============================================================================
void()    cent_stand1    =[    $stand1,    cent_stand2    ] {
    centurionhover();monster_idle_sound();ai_stand();};
void()    cent_stand2    =[    $stand2,    cent_stand3    ] {ai_stand();};
void()    cent_stand3    =[    $stand3,    cent_stand4    ] {ai_stand();};
void()    cent_stand4    =[    $stand4,    cent_stand5    ] {ai_stand();};
void()    cent_stand5    =[    $stand5,    cent_stand6    ] {ai_stand();};
void()    cent_stand6    =[    $stand6,    cent_stand7    ] {ai_stand();};
void()    cent_stand7    =[    $stand7,    cent_stand8    ] {ai_stand();};
void()    cent_stand8    =[    $stand8,    cent_stand1    ] {ai_stand();};

//============================================================================
void()    cent_walk1    =[    $stand1,    cent_walk2    ] {
    centurionhover();monster_idle_sound();ai_walk(4);};
void()    cent_walk2    =[    $stand2,    cent_walk3    ] {ai_walk(4);};
void()    cent_walk3    =[    $stand3,    cent_walk4    ] {ai_walk(4);};
void()    cent_walk4    =[    $stand4,    cent_walk5    ] {ai_walk(4);};
void()    cent_walk5    =[    $stand5,    cent_walk6    ] {ai_walk(4);};
void()    cent_walk6    =[    $stand6,    cent_walk7    ] {ai_walk(4);};
void()    cent_walk7    =[    $stand7,    cent_walk8    ] {ai_walk(4);};
void()    cent_walk8    =[    $stand8,    cent_walk1    ] {ai_walk(4);};

//============================================================================
void()    cent_run1    =[    $stand1,    cent_run2    ] {
    centurionhover();monster_idle_sound(); 
    if (self.meleeattack < time) {
        self.meleeattack = time + 4 + (random() * 4);
        self.lefty = rint(1 - self.lefty);
    }
    ai_run(8);};
void()    cent_run2    =[    $stand2,    cent_run3    ] {ai_run(8);};
void()    cent_run3    =[    $stand3,    cent_run4    ] {ai_run(8);};
void()    cent_run4    =[    $stand4,     cent_run5    ] {ai_run(8);};
void()    cent_run5    =[    $stand5,    cent_run6    ] {ai_run(8);};
void()    cent_run6    =[    $stand6,    cent_run7    ] {ai_run(8);};
void()    cent_run7    =[    $stand7,    cent_run8    ] {ai_run(8);};
void()    cent_run8    =[    $stand8,    cent_run1    ] {ai_run(8);};

//============================================================================
void() cent_fire =
{
    local vector org, vec;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, SOUND_PLASMA_FIRE, 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    // Fire plasma projectile at players feet
    vec = SUB_orgEnemyTarget() - '0 0 12';
    vec = normalize(vec - self.origin);

    launch_plasma(org, vec, CT_MONCENTURION, SPEED_PLASMA);
};

//----------------------------------------------------------------------
void()    cent_atk1    =[    $attack1,    cent_atk2    ] {
    sound (self, CHAN_WEAPON, "enforcer/elim_pg_load.wav", 1, ATTN_NORM);};
void()    cent_atk2    =[    $attack2,    cent_atk3    ] {ai_face();};
void()    cent_atk3    =[    $attack3,    cent_atk4    ] {ai_face();};
void()    cent_atk4    =[    $attack4,    cent_atk5    ] {ai_face();};
void()    cent_atk5    =[    $attack5,    cent_atk6    ] {ai_face();};
void()    cent_atk6    =[    $attack6,    cent_atk7    ] {ai_face();
    if (self.health > 1) cent_fire();};
void()    cent_atk7    =[    $attack7,    cent_atk8    ] {};
void()    cent_atk8    =[    $attack8,    cent_atk9    ] {};
void()    cent_atk9    =[    $attack5,    cent_atk10    ] {ai_face();};
void()    cent_atk10    =[    $attack6,    cent_atk11    ] {ai_face();
    if (self.health > 1) cent_fire();};
void()    cent_atk11    =[    $attack7,    cent_atk12    ] {};
void()    cent_atk12    =[    $attack8,    cent_atk13    ] {};
void()    cent_atk13    =[    $attack9,    cent_atk14    ] {ai_face();};
void()    cent_atk14    =[    $attack10,    cent_run1    ] {ai_face();SUB_CheckRefire (cent_atk1);};

//============================================================================
void()    cent_paina1    =[    $paina1,    cent_paina2    ] {};
void()    cent_paina2    =[    $paina2,    cent_paina3    ] {};
void()    cent_paina3    =[    $paina3,    cent_paina4    ] {};
void()    cent_paina4    =[    $paina4,    cent_run1    ] {};

//----------------------------------------------------------------------
void()    cent_painb1    =[    $painb1,    cent_painb2    ] {};
void()    cent_painb2    =[    $painb2,    cent_painb3    ] {};
void()    cent_painb3    =[    $painb3,    cent_painb4    ] {};
void()    cent_painb4    =[    $painb4,    cent_painb5    ] {};
void()    cent_painb5    =[    $painb5,    cent_run1    ] {};

//----------------------------------------------------------------------
void()    cent_painc1    =[    $painc1,    cent_painc2    ] {};
void()    cent_painc2    =[    $painc2,    cent_painc3    ] {};
void()    cent_painc3    =[    $painc3,    cent_painc4    ] {};
void()    cent_painc4    =[    $painc4,    cent_painc5    ] {};
void()    cent_painc5    =[    $painc5,    cent_painc6    ] {};
void()    cent_painc6    =[    $painc6,    cent_painc7    ] {};
void()    cent_painc7    =[    $painc7,    cent_painc8    ] {};
void()    cent_painc8    =[    $painc8,    cent_run1    ] {};

//----------------------------------------------------------------------
void()    cent_paind1    =[    $paind1,    cent_paind2    ] {};
void()    cent_paind2    =[    $paind2,    cent_paind3    ] {};
void()    cent_paind3    =[    $paind3,    cent_paind4    ] {};
void()    cent_paind4    =[    $paind4,    cent_paind5    ] {ai_painforward(2);};
void()    cent_paind5    =[    $paind5,    cent_paind6    ] {ai_painforward(1);};
void()    cent_paind6    =[    $paind6,    cent_paind7    ] {};
void()    cent_paind7    =[    $paind7,    cent_paind8    ] {};
void()    cent_paind8    =[    $paind8,    cent_paind9    ] {};
void()    cent_paind9    =[    $paind9,    cent_paind10 ] {};
void()    cent_paind10=[    $paind10,    cent_paind11 ] {};
void()    cent_paind11=[    $paind11,    cent_paind12 ] {ai_painforward(1);};
void()    cent_paind12=[    $paind12,    cent_paind13 ] {ai_painforward(1);};
void()    cent_paind13=[    $paind13,    cent_paind14 ] {ai_painforward(1);};
void()    cent_paind14=[    $paind14,    cent_paind15 ] {};
void()    cent_paind15=[    $paind15,    cent_paind16 ] {};
void()    cent_paind16=[    $paind16,    cent_paind17 ] {ai_pain(1);};
void()    cent_paind17=[    $paind17,    cent_paind18 ] {ai_pain(1);};
void()    cent_paind18=[    $paind18,    cent_paind19 ] {};
void()    cent_paind19=[    $paind19,    cent_run1     ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) cent_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        self.lip = random ();
        if (self.lip < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
    
        // Not using long pain animation (need to be agile)
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (self.lip < 0.3) cent_paina1 ();
            else if (self.lip < 0.6) cent_painb1 ();
            else cent_painc1 ();
        }
    }
};

//============================================================================
// Every death frame spawn some smoke and detect impact!
void() cent_crash =
{
    if (self.state) {
        // Spawn projectile smoke trails or smoke sprites
        if (random() < 0.5) SpawnProjectileSmoke(self.origin, 150, 50, 150);
        else {
            self.oldorigin = self.origin + vecrand(0,32,TRUE);
            SpawnExplosion(EXPLODE_BURST_SMOKE, self.oldorigin, "");
        }
        // Once on ground, thud sound and no more smoke
        if (self.flags & FL_ONGROUND) {
            sound (self, CHAN_WEAPON, GIB_SOUND_WOOD, 1, ATTN_NORM);
            self.state = FALSE;
        }
    }
};

//----------------------------------------------------------------------
void()    cent_die1    =[    $death1,    cent_die2    ] {cent_crash();};
void()    cent_die2    =[    $death2,    cent_die3    ] {monster_check_gib();cent_crash();};
void()    cent_die3    =[    $death3,    cent_die4    ] {monster_check_gib();
self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();cent_crash();};
void()    cent_die4    =[    $death4,    cent_die5    ] {cent_crash();};
void()    cent_die5    =[    $death5,    cent_die6    ] {cent_crash();};
void()    cent_die6    =[    $death6,    cent_die7    ] {cent_crash();};
void()    cent_die7    =[    $death7,    cent_die8    ] {cent_crash();};
void()    cent_die8    =[    $death8,    cent_die9    ] {cent_crash();};
void()    cent_die9    =[    $death9,    cent_die10    ] {cent_crash();};
void()    cent_die10    =[    $death10,    cent_die11    ] {cent_crash();};
void()    cent_die11    =[    $death11,    cent_die12    ] {cent_crash();};
void()    cent_die12    =[    $death12,    cent_die13    ] {cent_crash();};
void()    cent_die13    =[    $death13,    cent_die14    ] {monster_death_postcheck();cent_crash();};
void()    cent_die14    =[    $death14,    cent_die14    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    cent_fdie1    =[    $fdeath1,    cent_fdie2    ] {cent_crash();};
void()    cent_fdie2    =[    $fdeath2,    cent_fdie3    ] {monster_check_gib();cent_crash();};
void()    cent_fdie3    =[    $fdeath3,    cent_fdie4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();cent_crash();};
void()    cent_fdie4    =[    $fdeath4,    cent_fdie5    ] {cent_crash();};
void()    cent_fdie5    =[    $fdeath5,    cent_fdie6    ] {cent_crash();};
void()    cent_fdie6    =[    $fdeath6,    cent_fdie7    ] {cent_crash();};
void()    cent_fdie7    =[    $fdeath7,    cent_fdie8    ] {cent_crash();};
void()    cent_fdie8    =[    $fdeath8,    cent_fdie9    ] {cent_crash();};
void()    cent_fdie9    =[    $fdeath9,    cent_fdie10    ] {cent_crash();};
void()    cent_fdie10    =[    $fdeath10,    cent_fdie11    ] {monster_death_postcheck();cent_crash();};
void()    cent_fdie11    =[    $fdeath11,    cent_fdie11    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() cent_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // Kill hoverboard sound
    sound(self, CHAN_ITEM, SOUND_EMPTY, 1, ATTN_IDLE);
    
    // regular death
    if (!self.gibbed) {
        self.state = TRUE;
        sound (self, CHAN_VOICE, "enforcer/death1.wav", 1, ATTN_NORM);
        // Make sure Centurion falls to the ground (like wizards)
        self.velocity_x = -200 + 400*random();
        self.velocity_y = -200 + 400*random();
        self.velocity_z = 100 + 100*random();
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        // Spawn a big sprite explosion + sound
        SpawnExplosion(EXPLODE_BIG, self.origin, SOUND_REXP3);
        if (random() > 0.5) cent_die1 ();
        else cent_fdie1 ();
    }
};

/*======================================================================
 QUAKED monster_centurion (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_centurion =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/mon_centurion.mdl";
    self.headmdl = "progs/h_centurion.mdl";
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_PLASMA);

    precache_sound ("enforcer/death1.wav");
    self.pain_sound = "enforcer/pain1.wav";
    self.pain_sound2 = "enforcer/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    self.idle_sound = "enforcer/idle1.wav";
    precache_sound (self.idle_sound);
    self.sight_sound = "enforcer/sight1.wav";
    precache_sound (self.sight_sound);
    precache_sound ("enforcer/sight2.wav");
    precache_sound ("enforcer/sight3.wav");
    precache_sound ("enforcer/sight4.wav");

    // Extra load/fire sounds (from Quoth MOD)
    precache_sound ("enforcer/elim_pg_load.wav");
    precache_sound (SOUND_PLASMA_FIRE);
    precache_sound (SOUND_PLASMA_HIT);
    
    // Originally from rubicon2 (re-processed)
    precache_sound ("enforcer/cent_hover.wav");
    precache_sound (GIB_SOUND_WOOD);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_PLASMAGRN);
    }
    
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 120;
    self.gibhealth = -35;
    self.gibbed = FALSE;
    self.yaw_speed = 35;                // Can turn really fast
    self.pain_flinch = 60;                // high pain tolerance
    self.pain_longanim = FALSE;            // Not using D pain, too slow
    self.steptype = FS_FLYING;            // Silent feet
    if (self.height < 1) self.height = MONAI_ABOVEDIST; // Custom height
    if (!self.exactskin) self.randomskin = 6;
    self.ammo_cells = rint(random()*3);
    self.attack_offset = '28 8.5 24';
    self.deathstring = " was vaporized by a Centurion\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    self.th_checkattack = CenturionCheckAttack;
    self.th_stand = cent_stand1;
    self.th_walk = cent_walk1;
    self.th_run = cent_run1;
    self.th_pain = cent_pain;
    self.th_die = cent_die;
    self.th_missile = cent_atk1;

    self.classtype = CT_MONCENTURION;
    self.classgroup = CG_ENFORCER;
    self.classmove = MON_MOVEFLY;
    monster_start();
};