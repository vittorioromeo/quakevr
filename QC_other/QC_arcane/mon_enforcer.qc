/*==============================================================================
ENFORCER
==============================================================================*/
$cd id1/models/enforcer
$origin 0 -6 24
$base base        
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16

$frame run1 run2 run3 run4 run5 run6 run7 run8

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

//============================================================================
void()    enf_stand1    =[    $stand1,    enf_stand2    ] {monster_idle_sound(); ai_stand();};
void()    enf_stand2    =[    $stand2,    enf_stand3    ] {ai_stand();};
void()    enf_stand3    =[    $stand3,    enf_stand4    ] {ai_stand();};
void()    enf_stand4    =[    $stand4,    enf_stand5    ] {ai_stand();};
void()    enf_stand5    =[    $stand5,    enf_stand6    ] {ai_stand();};
void()    enf_stand6    =[    $stand6,    enf_stand7    ] {ai_stand();};
void()    enf_stand7    =[    $stand7,    enf_stand1    ] {ai_stand();};

//============================================================================
void()    enf_walk1    =[    $walk1,        enf_walk2    ] {monster_idle_sound(); ai_walk(2);};
void()    enf_walk2    =[    $walk2,        enf_walk3    ] {monster_footstep(FALSE); ai_walk(4);};
void()    enf_walk3    =[    $walk3,        enf_walk4    ] {ai_walk(4);};
void()    enf_walk4    =[    $walk4,        enf_walk5    ] {ai_walk(3);};
void()    enf_walk5    =[    $walk5,        enf_walk6    ] {ai_walk(1);};
void()    enf_walk6    =[    $walk6,        enf_walk7    ] {ai_walk(2);};
void()    enf_walk7    =[    $walk7,        enf_walk8    ] {ai_walk(2);};
void()    enf_walk8    =[    $walk8,        enf_walk9    ] {ai_walk(1);};
void()    enf_walk9    =[    $walk9,        enf_walk10    ] {ai_walk(2);};
void()    enf_walk10    =[    $walk10,    enf_walk11    ] {monster_footstep(FALSE); ai_walk(4);};
void()    enf_walk11    =[    $walk11,    enf_walk12    ] {ai_walk(4);};
void()    enf_walk12    =[    $walk12,    enf_walk13    ] {ai_walk(1);};
void()    enf_walk13    =[    $walk13,    enf_walk14    ] {ai_walk(2);};
void()    enf_walk14    =[    $walk14,    enf_walk15    ] {ai_walk(3);};
void()    enf_walk15    =[    $walk15,    enf_walk16    ] {ai_walk(4);};
void()    enf_walk16    =[    $walk16,    enf_walk1    ] {ai_walk(2);};

//============================================================================
void()    enf_run1    =[    $run1,        enf_run2    ] {monster_idle_sound(); ai_run(18);};
void()    enf_run2    =[    $run2,        enf_run3    ] {ai_run(14);};
void()    enf_run3    =[    $run3,        enf_run4    ] {monster_footstep(FALSE); ai_run(7);};
void()    enf_run4    =[    $run4,        enf_run5    ] {ai_run(12);};
void()    enf_run5    =[    $run5,        enf_run6    ] {ai_run(14);};
void()    enf_run6    =[    $run6,        enf_run7    ] {ai_run(14);};
void()    enf_run7    =[    $run7,        enf_run8    ] {monster_footstep(FALSE); ai_run(7);};
void()    enf_run8    =[    $run8,        enf_run1    ] {ai_run(11);};

//============================================================================
// - Basic enforcer (laser attack)
//----------------------------------------------------------------------
void() enforcer_fire =
{
    local vector org, vec;

    if (!self.enemy) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    vec = normalize(self.enemy.origin - self.origin);

    launch_projectile(org, vec, CT_PROJ_LASER, SPEED_LASER);
};

//----------------------------------------------------------------------
void()    enf_atk1    =[    $attack1,    enf_atk2    ] {};
void()    enf_atk2    =[    $attack2,    enf_atk3    ] {monster_footstep(FALSE);ai_face();};
void()    enf_atk3    =[    $attack3,    enf_atk4    ] {ai_face();};
void()    enf_atk4    =[    $attack4,    enf_atk5    ] {ai_face();};
void()    enf_atk5    =[    $attack5,    enf_atk6    ] {ai_face();};
void()    enf_atk6    =[    $attack6,    enf_atk7    ] {
    if (self.health > 1) enforcer_fire();};
void()    enf_atk7    =[    $attack7,    enf_atk8    ] {};
void()    enf_atk8    =[    $attack8,    enf_atk9    ] {};
void()    enf_atk9    =[    $attack5,    enf_atk10    ] {ai_face();};
void()    enf_atk10    =[    $attack6,    enf_atk11    ] {ai_face();
    if (self.health > 1) enforcer_fire();};
void()    enf_atk11    =[    $attack7,    enf_atk12    ] {};
void()    enf_atk12    =[    $attack8,    enf_atk13    ] {};
void()    enf_atk13    =[    $attack9,    enf_atk14    ] {ai_face();};
void()    enf_atk14    =[    $attack10,    enf_run1    ] {ai_face();SUB_CheckRefire (enf_atk1);};

//============================================================================
// - Defender enforcer (ssg/gl attack)
//----------------------------------------------------------------------
void() defender_ssg =
{
    if (!self.enemy) return;
    if (self.health < 1) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "enforcer/def_ssg_fire.wav", 1, ATTN_NORM);    
    
    // Choose between a projectile or hit-scan system
    if (query_configflag(SVR_SHOTGPROJ)) 
        FireBullets (QUANTITY_DEFENDER, SPREAD_DEF);
    else {
        Launch_Shells(QUANTITY_DEFENDER, SPREAD_DEF, CT_PROJ_SG);
        Launch_ShellCasing(2);
    }
};

//----------------------------------------------------------------------
void()    def_atkssg1    =[    $attack1,    def_atkssg2    ] {
    sound (self, CHAN_WEAPON, "enforcer/def_ssg_load.wav", 1, ATTN_NORM);};
void()    def_atkssg2    =[    $attack2,    def_atkssg3    ] {monster_footstep(FALSE);ai_face();};
void()    def_atkssg3    =[    $attack3,    def_atkssg4    ] {ai_face();};
void()    def_atkssg4    =[    $attack4,    def_atkssg5    ] {ai_face();};
void()    def_atkssg5    =[    $attack5,    def_atkssg6    ] {ai_face();};
void()    def_atkssg6    =[    $attack6,    def_atkssg7    ] {ai_face();
    if (self.health > 1) defender_ssg();};
void()    def_atkssg7    =[    $attack7,    def_atkssg8    ] {};
void()    def_atkssg8    =[    $attack8,    def_atkssg9    ] {};
void()    def_atkssg9    =[    $attack9,    def_atkssg10] {ai_face();};
void()    def_atkssg10=[    $attack10,    enf_run1    ] {SUB_CheckRefire (def_atkssg1);};

//----------------------------------------------------------------------
void()    def_atkgl1    =[    $attack1,    def_atkgl2    ] {ai_face();};
void()    def_atkgl2    =[    $attack2,    def_atkgl3    ] {monster_footstep(FALSE);ai_face();};
void()    def_atkgl3    =[    $attack3,    def_atkgl4    ] {ai_face();
    MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed); };
void()    def_atkgl4    =[    $attack4,    def_atkgl5    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void()    def_atkgl5    =[    $attack5,    def_atkgl6    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void()    def_atkgl6    =[    $attack6,    def_atkgl7    ] {ai_face();
    makevectors(self.angles);
    self.pos1 = self.origin + attack_vector(self.attack_offset);
    MonsterFireGrenade(self.pos1, self.enemy.origin);
};
void()    def_atkgl7    =[    $attack7,    def_atkgl8    ] {};
void()    def_atkgl8    =[    $attack8,    def_atkgl9    ] {};
void()    def_atkgl9    =[    $attack9,    def_atkgl10] {ai_face();};
void()    def_atkgl10=[    $attack10,    enf_run1    ]{ai_face();};


//============================================================================
// - Eliminator enforcer (plasma attack)
//----------------------------------------------------------------------
void() eliminator_fire =
{
    local vector org, vec;

    if (!self.enemy) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "weapons/plasma_fire.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    vec = normalize(self.enemy.origin - self.origin);

    launch_plasma(org, vec, CT_MONELIMATOR, SPEED_PLASMA);
};

//----------------------------------------------------------------------
void()    elim_atk1    =[    $attack1,    elim_atk2    ] {
    sound (self, CHAN_WEAPON, "enforcer/elim_pg_load.wav", 1, ATTN_NORM);};
void()    elim_atk2    =[    $attack2,    elim_atk3    ] {monster_footstep(FALSE);ai_face();};
void()    elim_atk3    =[    $attack3,    elim_atk4    ] {ai_face();};
void()    elim_atk4    =[    $attack4,    elim_atk5    ] {ai_face();};
void()    elim_atk5    =[    $attack5,    elim_atk6    ] {ai_face();};
void()    elim_atk6    =[    $attack6,    elim_atk7    ] {ai_face();
    if (self.health > 1) eliminator_fire();};
void()    elim_atk7    =[    $attack7,    elim_atk8    ] {};
void()    elim_atk8    =[    $attack8,    elim_atk9    ] {};
void()    elim_atk9    =[    $attack5,    elim_atk10    ] {ai_face();};
void()    elim_atk10    =[    $attack6,    elim_atk11    ] {ai_face();
    if (self.health > 1) eliminator_fire();};
void()    elim_atk11    =[    $attack7,    elim_atk12    ] {};
void()    elim_atk12    =[    $attack8,    elim_atk13    ] {};
void()    elim_atk13    =[    $attack9,    elim_atk14    ] {ai_face();};
void()    elim_atk14    =[    $attack10,    enf_run1    ] {ai_face();SUB_CheckRefire (elim_atk1);};

//============================================================================
void()    enf_paina1    =[    $paina1,    enf_paina2    ] {};
void()    enf_paina2    =[    $paina2,    enf_paina3    ] {};
void()    enf_paina3    =[    $paina3,    enf_paina4    ] {};
void()    enf_paina4    =[    $paina4,    enf_run1    ] {};

//----------------------------------------------------------------------
void()    enf_painb1    =[    $painb1,    enf_painb2    ] {};
void()    enf_painb2    =[    $painb2,    enf_painb3    ] {};
void()    enf_painb3    =[    $painb3,    enf_painb4    ] {};
void()    enf_painb4    =[    $painb4,    enf_painb5    ] {};
void()    enf_painb5    =[    $painb5,    enf_run1    ] {};

//----------------------------------------------------------------------
void()    enf_painc1    =[    $painc1,    enf_painc2    ] {};
void()    enf_painc2    =[    $painc2,    enf_painc3    ] {monster_footstep(FALSE);};
void()    enf_painc3    =[    $painc3,    enf_painc4    ] {};
void()    enf_painc4    =[    $painc4,    enf_painc5    ] {};
void()    enf_painc5    =[    $painc5,    enf_painc6    ] {};
void()    enf_painc6    =[    $painc6,    enf_painc7    ] {};
void()    enf_painc7    =[    $painc7,    enf_painc8    ] {};
void()    enf_painc8    =[    $painc8,    enf_run1    ] {};

//----------------------------------------------------------------------
void()    enf_paind1    =[    $paind1,    enf_paind2    ] {};
void()    enf_paind2    =[    $paind2,    enf_paind3    ] {};
void()    enf_paind3    =[    $paind3,    enf_paind4    ] {monster_footstep(FALSE);};
void()    enf_paind4    =[    $paind4,    enf_paind5    ] {ai_painforward(2);};
void()    enf_paind5    =[    $paind5,    enf_paind6    ] {ai_painforward(1);};
void()    enf_paind6    =[    $paind6,    enf_paind7    ] {};
void()    enf_paind7    =[    $paind7,    enf_paind8    ] {};
void()    enf_paind8    =[    $paind8,    enf_paind9    ] {};
void()    enf_paind9    =[    $paind9,    enf_paind10    ] {};
void()    enf_paind10    =[    $paind10,    enf_paind11    ] {};
void()    enf_paind11    =[    $paind11,    enf_paind12    ] {ai_painforward(1);};
void()    enf_paind12    =[    $paind12,    enf_paind13    ] {ai_painforward(1);};
void()    enf_paind13    =[    $paind13,    enf_paind14    ] {ai_painforward(1);};
void()    enf_paind14    =[    $paind14,    enf_paind15    ] {};
void()    enf_paind15    =[    $paind15,    enf_paind16    ] {};
void()    enf_paind16    =[    $paind16,    enf_paind17    ] {ai_pain(1);};
void()    enf_paind17    =[    $paind17,    enf_paind18    ] {ai_pain(1);};
void()    enf_paind18    =[    $paind18,    enf_paind19    ] {};
void()    enf_paind19    =[    $paind19,    enf_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) enf_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        self.lip = random ();
        if (self.lip < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
    
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (self.lip < 0.2) enf_paina1 ();
            else if (self.lip < 0.4) enf_painb1 ();
            else if (self.lip < 0.7) enf_painc1 ();
            else {
                self.pain_finished = time + 2;
                enf_paind1 ();
            }
        }
        // The new axe forces monsters into long pain animations
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.9;
            self.axhitme = 0;
            enf_paind1 ();
        }
    }
};

//============================================================================
void()    enf_die1    =[    $death1,    enf_die2    ] {};
void()    enf_die2    =[    $death2,    enf_die3    ] {monster_check_gib();};
void()    enf_die3    =[    $death3,    enf_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();};
void()    enf_die4    =[    $death4,    enf_die5    ] {ai_forward(14);};
void()    enf_die5    =[    $death5,    enf_die6    ] {ai_forward(2);};
void()    enf_die6    =[    $death6,    enf_die7    ] {};
void()    enf_die7    =[    $death7,    enf_die8    ] {};
void()    enf_die8    =[    $death8,    enf_die9    ] {};
void()    enf_die9    =[    $death9,    enf_die10    ] {ai_forward(3);};
void()    enf_die10    =[    $death10,    enf_die11    ] {ai_forward(5);};
void()    enf_die11    =[    $death11,    enf_die12    ] {ai_forward(5);};
void()    enf_die12    =[    $death12,    enf_die13    ] {ai_forward(5);};
void()    enf_die13    =[    $death13,    enf_die14    ] {monster_death_postcheck();};
void()    enf_die14    =[    $death14,    enf_die14    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    enf_fdie1    =[    $fdeath1,    enf_fdie2    ] {};
void()    enf_fdie2    =[    $fdeath2,    enf_fdie3    ] {monster_check_gib();};
void()    enf_fdie3    =[    $fdeath3,    enf_fdie4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();};
void()    enf_fdie4    =[    $fdeath4,    enf_fdie5    ] {};
void()    enf_fdie5    =[    $fdeath5,    enf_fdie6    ] {};
void()    enf_fdie6    =[    $fdeath6,    enf_fdie7    ] {};
void()    enf_fdie7    =[    $fdeath7,    enf_fdie8    ] {};
void()    enf_fdie8    =[    $fdeath8,    enf_fdie9    ] {};
void()    enf_fdie9    =[    $fdeath9,    enf_fdie10    ] {};
void()    enf_fdie10    =[    $fdeath10,    enf_fdie11    ] {monster_death_postcheck();};
void()    enf_fdie11    =[    $fdeath11,    enf_fdie11    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() enf_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "enforcer/death1.wav", 1, ATTN_NORM);
        if (random() > 0.5) enf_die1 ();
        else enf_fdie1 ();
    }
};

/*======================================================================
 QUAKED monster_enforcer (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_base_enforcer =
{
    self.mdl = "progs/mon_enforcer.mdl";
    self.headmdl = "progs/h_enforcer.mdl";
    self.gib1mdl = "progs/w_enforcergun.mdl";    // Unique weapon
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    
    self.gib1sound = GIB_IMPACT_WOOD;
    precache_sound ("enforcer/death1.wav");
    self.pain_sound = "enforcer/pain1.wav";
    self.pain_sound2 = "enforcer/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    self.gibhealth = -35;
    self.gibbed = FALSE;
    self.pain_flinch = 20;                // low pain threshold
    self.pain_longanim = TRUE;            // Long pain animation for axe
    self.steptype = FS_TYPEHEAVY;        // heavy player
    self.attack_offset = '28 8.5 16';    // End of Gun
    if (self.deathstring == "")
        self.deathstring = " was blasted by an Enforcer\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    if (self.classtype == CT_MONELIMATOR) self.resist_cells = 0.5;
    else if (self.classtype == CT_MONDEFENDER) self.resist_rockets = 0.5;
    
    self.th_stand = enf_stand1;
    self.th_walk = enf_walk1;
    self.th_run = enf_run1;
    self.th_pain = enf_pain;
    self.th_die = enf_die;

    self.classgroup = CG_ENFORCER;
    self.classmove = MON_MOVEWALK;
    monster_start();
};

//----------------------------------------------------------------------
void() monster_enforcer =
{
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONENFORCER;
    precache_model (MODEL_PROJ_LASER); // Originally - progs/laser.mdl
    
    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_LASERGRN);
    }
    
    if (self.health < 1) self.health = 80;
    self.ammo_cells = rint(1 + random()*2);
    self.exactskin = rint((random()*3)-0.5);
    self.gib1skin = 0;

    self.idle_sound = "enforcer/idle1.wav";
    precache_sound (self.idle_sound);
    self.sight_count = 4;
    self.sight_sound = "enforcer/sight1.wav";
    self.sight_sound2 = "enforcer/sight2.wav";
    self.sight_sound3 = "enforcer/sight3.wav";
    self.sight_sound4 = "enforcer/sight4.wav";
    precache_sound (self.sight_sound);
    precache_sound (self.sight_sound2);
    precache_sound (self.sight_sound3);
    precache_sound (self.sight_sound4);

    // Projectile fire and impact (used in projectiles)
    precache_sound ("enforcer/enfire.wav");
    precache_sound ("enforcer/enfstop.wav");
    
    self.th_missile = enf_atk1;

    monster_base_enforcer();
}
    
//----------------------------------------------------------------------
void() monster_defender =
{
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONDEFENDER;
    precache_model (MODEL_PROJ_GRENADE);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_GRENADEGRN);
    }
    
    if (self.health < 1) self.health = 100;
    self.ammo_shells = rint(4 + random()*4);
    self.exactskin = 3 + rint((random()*3)-0.5);
    self.gib1skin = 1;

    self.idle_sound = "enforcer/def_idle1.wav";
    self.idle_sound2 = "enforcer/def_idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    self.sight_sound = "enforcer/def_sight1.wav";
    precache_sound (self.sight_sound);

    // Extra load/fire sounds (from Quoth MOD)
    precache_sound ("enforcer/def_ssg_load.wav");
    precache_sound ("enforcer/def_ssg_fire.wav");
    
    self.th_missile = def_atkgl1;
    self.th_melee = def_atkssg1;
    self.th_checkattack = DefenderCheckAttack;
    // Grenade bounce off body
    self.bouncegrenade = TRUE;
    self.deathstring = " was destroyed by a Defender\n";

    monster_base_enforcer();
};

//----------------------------------------------------------------------
void() monster_eliminator =
{
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONELIMATOR;
    precache_model (MODEL_PROJ_PLASMA);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_PLASMAGRN);
    }

    if (self.health < 1) self.health = 120;
    self.ammo_cells = rint(2 + random()*2);
    self.exactskin = 6 + rint((random()*3)-0.5);
    self.gib1skin = 2;

    self.idle_sound = "enforcer/elim_idle1.wav";
    self.idle_sound2 = "enforcer/elim_idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    self.sight_sound = "enforcer/elim_sight1.wav";
    precache_sound (self.sight_sound);

    // Extra load/fire sounds (from Quoth MOD)
    precache_sound ("enforcer/elim_pg_load.wav");
    precache_sound (SOUND_PLASMA_FIRE);
    precache_sound (SOUND_PLASMA_HIT);
    
    self.th_missile = elim_atk1;
    self.th_checkattack = EliminatorCheckAttack;
    self.deathstring = " was assasinated by a Eliminator\n";

    monster_base_enforcer();
};