/*==============================================================================
SOLDIER / PLAYER
==============================================================================*/
$cd id1/models/soldier3
$origin 0 -6 24
$base base        
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10

$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11

$frame load1 load2 load3 load4 load5 load6 load7 load8 load9 load10 load11

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9 painb10
$frame painb11 painb12 painb13 painb14

$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8 painc9 painc10
$frame painc11 painc12 painc13

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6 shoot7 shoot8 shoot9

$frame walk_1 walk_2 walk_3 walk_4 walk_5 walk_6 walk_7 walk_8
$frame walk_9 walk_10 walk_11 walk_12 walk_13 walk_14 walk_15 walk_16
$frame walk_17 walk_18 walk_19 walk_20 walk_21 walk_22 walk_23 walk_24

void() army_fire;

//======================================================================
void()    army_stand1    =[    $stand1,    army_stand2    ] {monster_idle_sound(); ai_stand();};
void()    army_stand2    =[    $stand2,    army_stand3    ] {ai_stand();};
void()    army_stand3    =[    $stand3,    army_stand4    ] {ai_stand();};
void()    army_stand4    =[    $stand4,    army_stand5    ] {ai_stand();};
void()    army_stand5    =[    $stand5,    army_stand6    ] {ai_stand();};
void()    army_stand6    =[    $stand6,    army_stand7    ] {ai_stand();};
void()    army_stand7    =[    $stand7,    army_stand8    ] {ai_stand();};
void()    army_stand8    =[    $stand8,    army_stand1    ] {ai_stand();};

//======================================================================
void()    army_walk1    =[    $walk_1,    army_walk2    ] {monster_footstep(FALSE);ai_walk(1);};
void()    army_walk2    =[    $walk_2,    army_walk3    ] {monster_idle_sound();ai_walk(1);};
void()    army_walk3    =[    $walk_3,    army_walk4    ] {ai_walk(1);};
void()    army_walk4    =[    $walk_4,    army_walk5    ] {ai_walk(1);};
void()    army_walk5    =[    $walk_5,    army_walk6    ] {ai_walk(2);};
void()    army_walk6    =[    $walk_6,    army_walk7    ] {ai_walk(3);};
void()    army_walk7    =[    $walk_7,    army_walk8    ] {ai_walk(4);};
void()    army_walk8    =[    $walk_8,    army_walk9    ] {ai_walk(4);};
void()    army_walk9    =[    $walk_9,    army_walk10    ] {ai_walk(2);};
void()    army_walk10    =[    $walk_10,    army_walk11    ] {ai_walk(2);};
void()    army_walk11    =[    $walk_11,    army_walk12    ] {ai_walk(2);};
void()    army_walk12    =[    $walk_12,    army_walk13    ] {monster_footstep(FALSE);ai_walk(1);};
void()    army_walk13    =[    $walk_13,    army_walk14    ] {ai_walk(0);};
void()    army_walk14    =[    $walk_14,    army_walk15    ] {ai_walk(1);};
void()    army_walk15    =[    $walk_15,    army_walk16    ] {ai_walk(1);};
void()    army_walk16    =[    $walk_16,    army_walk17    ] {ai_walk(1);};
void()    army_walk17    =[    $walk_17,    army_walk18    ] {ai_walk(3);};
void()    army_walk18    =[    $walk_18,    army_walk19    ] {ai_walk(3);};
void()    army_walk19    =[    $walk_19,    army_walk20    ] {ai_walk(3);};
void()    army_walk20    =[    $walk_20,    army_walk21    ] {ai_walk(3);};
void()    army_walk21    =[    $walk_21,    army_walk22    ] {ai_walk(2);};
void()    army_walk22    =[    $walk_22,    army_walk23    ] {ai_walk(1);};
void()    army_walk23    =[    $walk_23,    army_walk24    ] {ai_walk(1);};
void()    army_walk24    =[    $walk_24,    army_walk1    ] {ai_walk(1);};

//======================================================================
void()    army_run1    =[    $run1,        army_run2    ] {monster_idle_sound();ai_run(11);};
void()    army_run2    =[    $run2,        army_run3    ] {monster_footstep(FALSE);ai_run(15);};
void()    army_run3    =[    $run3,        army_run4    ] {ai_run(10);};
void()    army_run4    =[    $run4,        army_run5    ] {ai_run(10);};
void()    army_run5    =[    $run5,        army_run6    ] {ai_run(8);};
void()    army_run6    =[    $run6,        army_run7    ] {monster_footstep(FALSE);ai_run(15);};
void()    army_run7    =[    $run7,        army_run8    ] {ai_run(10);};
void()    army_run8    =[    $run8,        army_run1    ] {ai_run(8);};

// Move backwards to create some space to fire a rocket
void()    army_runback1    =[    $run8,    army_runback2    ] {ai_backface(8);};
void()    army_runback2    =[    $run7,    army_runback3    ] {ai_backface(10);};
void()    army_runback3    =[    $run6,    army_runback4    ] {monster_footstep(FALSE);ai_backface(15);};
void()    army_runback4    =[    $run5,    army_runback5    ] {ai_backface(8);};
void()    army_runback5    =[    $run4,    army_runback6    ] {ai_backface(10);};
void()    army_runback6    =[    $run3,    army_runback7    ] {ai_backface(10);};
void()    army_runback7    =[    $run2,    army_runback8    ] {monster_footstep(FALSE);ai_backface(15);};
void()    army_runback8    =[    $run1,    army_run1    ] {ai_backface(11);};

//======================================================================
void() army_fire =
{
    if (!self.enemy) return;
    if (self.health < 1) return;

    ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "soldier/sattck1.wav", 1, ATTN_NORM);    
    
    // Choose between a projectile or hit-scan system
    if (query_configflag(SVR_SHOTGPROJ))
        FireBullets (QUANTITY_GRUNT, SPREAD_GRUNT);
    else {
        Launch_Shells(QUANTITY_GRUNT, SPREAD_GRUNT, CT_PROJ_SG);
        Launch_ShellCasing(1);
    }
};

//----------------------------------------------------------------------
void()    army_atk1    =[    $shoot1,    army_atk2    ] {ai_face();};
void()    army_atk2    =[    $shoot2,    army_atk3    ] {ai_face();};
void()    army_atk3    =[    $shoot3,    army_atk4    ] {ai_face();};
void()    army_atk4    =[    $shoot4,    army_atk5    ] {ai_face();};
void()    army_atk5    =[    $shoot5,    army_atk6    ] {ai_face();army_fire();};
void()    army_atk6    =[    $shoot6,    army_atk7    ] {};
void()    army_atk7    =[    $shoot7,    army_atk8    ] {ai_face();SUB_CheckRefire (army_atk1);};
void()    army_atk8    =[    $shoot8,    army_atk9    ] {};
void()    army_atk9    =[    $shoot9,    army_run1    ] {ai_face();};

//======================================================================
void() army_fire_rocket =
{
    local vector org, dir;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    
    // check for enemy origin (based on sight test)
    // if cannot see enemy fire at last known location
    ai_trackenemy();
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "soldier/rocket_fire.wav", 1, ATTN_NORM);    

    self.attack_speed = 250;
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(self.attack_track - org);
    
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_ARMY, self.attack_speed);
};

//----------------------------------------------------------------------
void() army_rocket_think =
{
    // Only do a limited amount of updates to the missile
    self.count = self.count + 1;
    if (self.count < (5+skill)) {
        // Keep ticking (slowly)
        self.nextthink = time + 0.1;
        // If enemy active, track them down (slow turn)
        if (self.enemy && self.count < 5) {
            // Speed up and steer towards enemy
            self.attack_speed = self.attack_speed + 100;
            self.attack_track = normalize(self.enemy.origin - self.origin);
            self.velocity = self.attack_track * self.attack_speed;
            self.angles = vectoangles(self.velocity);
        }
        // Fire at original location
        else self.velocity = self.velocity * 1.15;
    }
    else {
        // Remove missile after a while
        self.nextthink = time + LIFE_ROCKET;
        self.think = SUB_Remove;
    }
};

//----------------------------------------------------------------------
void()    army_rocket_atk1 =[    $load1,    army_load2    ] {ai_face();
    self.pain_finished = time + 1.3;
    self.attack_track = self.enemy.origin;
    sound (self, CHAN_WEAPON, "soldier/rocket_load.wav", 1, ATTN_NORM);};
void()    army_load2    =[    $load2,    army_load3    ] {ai_trackenemy();};
void()    army_load3    =[    $load3,    army_load4    ] {ai_trackenemy();};
void()    army_load4    =[    $load4,    army_load5    ] {ai_trackenemy();};
void()    army_load5    =[    $load5,    army_load6    ] {ai_trackenemy();};
void()    army_load6    =[    $load6,    army_load7    ] {ai_trackenemy();};
void()    army_load7    =[    $load7,    army_load8    ] {ai_trackenemy();};
void()    army_load8    =[    $load8,    army_load9    ] {ai_trackenemy();};
void()    army_load9    =[    $load9,    army_load10    ] {ai_trackenemy();};
void()    army_load10    =[    $load10,army_load11    ] {ai_trackenemy();};
void()    army_load11    =[    $load11,army_atkr5    ] {ai_trackenemy();};
void()    army_atkr5    =[    $shoot5,army_atkr6    ] {army_fire_rocket();};
void()    army_atkr6    =[    $shoot6,army_atkr7    ] {};
void()    army_atkr7    =[    $shoot7,army_atkr8    ] {SUB_CheckRefire (army_rocket_atk1);};
void()    army_atkr8    =[    $shoot8,army_atkr9    ] {};
void()    army_atkr9    =[    $shoot9,army_run1    ] {ai_face();};

//======================================================================
void()    army_grenade_atk1 =[    $shoot1, army_grenade_atk2    ] {ai_face();};
void()    army_grenade_atk2 =[    $shoot2, army_grenade_atk3    ] {ai_face();
    MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed);};
void()    army_grenade_atk3 =[    $shoot3, army_grenade_atk4    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed);};
void()    army_grenade_atk4 =[    $shoot4, army_grenade_atk5    ] {ai_face();
    self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed);};
void()    army_grenade_atk5 =[    $shoot5, army_grenade_atk6    ] {ai_face();
    makevectors(self.angles);
    self.pos1 = self.origin + attack_vector(self.attack_offset);
    MonsterFireGrenade(self.pos1, self.enemy.origin);
};
void()    army_grenade_atk6 =[    $shoot6, army_grenade_atk7    ] {};
void()    army_grenade_atk7 =[    $shoot7, army_grenade_atk8    ] {ai_face();};
void()    army_grenade_atk8 =[    $shoot8, army_grenade_atk9    ] {};
void()    army_grenade_atk9 =[    $shoot9, army_run1    ] {ai_face();};

//======================================================================
void() army_plasma_fire = 
{
    local vector org, vec;

    if (!self.enemy) return;
    if (self.health < 1) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "weapons/plasma_fire.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    vec = normalize(self.enemy.origin - self.origin);

    launch_plasma(org, vec, CT_PROJ_PLASMA, SPEED_PLASMA);
};

//----------------------------------------------------------------------
void()    army_plasma_atk1 =[    $shoot1, army_plasma_atk2    ] {
    self.pain_finished = time + 1.3;
    sound (self, CHAN_WEAPON, "enforcer/elim_pg_load.wav", 1, ATTN_NORM);};
void()    army_plasma_atk2 =[    $shoot2, army_plasma_atk3    ] {ai_face();};
void()    army_plasma_atk3 =[    $shoot3, army_plasma_atk4    ] {ai_face();};
void()    army_plasma_atk4 =[    $shoot4, army_plasma_atk5    ] {ai_face();};
void()    army_plasma_atk5 =[    $shoot5, army_plasma_atk6    ] {ai_face();army_plasma_fire();};
void()    army_plasma_atk6 =[    $shoot6, army_plasma_atk7    ] {};
void()    army_plasma_atk7 =[    $shoot7, army_plasma_atk8    ] {ai_face();SUB_CheckRefire (army_plasma_atk1);};
void()    army_plasma_atk8 =[    $shoot8, army_plasma_atk9    ] {};
void()    army_plasma_atk9 =[    $shoot9, army_run1    ] {ai_face();};

//======================================================================
void()    army_pain1    =[    $pain1,        army_pain2    ] {};
void()    army_pain2    =[    $pain2,        army_pain3    ] {};
void()    army_pain3    =[    $pain3,        army_pain4    ] {};
void()    army_pain4    =[    $pain4,        army_pain5    ] {};
void()    army_pain5    =[    $pain5,        army_pain6    ] {};
void()    army_pain6    =[    $pain6,        army_run1    ] {ai_pain(1);};

//----------------------------------------------------------------------
void()    army_painb1    =[    $painb1,    army_painb2    ] {};
void()    army_painb2    =[    $painb2,    army_painb3    ] {ai_painforward(13);};
void()    army_painb3    =[    $painb3,    army_painb4    ] {ai_painforward(9);};
void()    army_painb4    =[    $painb4,    army_painb5    ] {};
void()    army_painb5    =[    $painb5,    army_painb6    ] {};
void()    army_painb6    =[    $painb6,    army_painb7    ] {};
void()    army_painb7    =[    $painb7,    army_painb8    ] {};
void()    army_painb8    =[    $painb8,    army_painb9    ] {};
void()    army_painb9    =[    $painb9,    army_painb10] {};
void()    army_painb10=[    $painb10,    army_painb11] {};
void()    army_painb11=[    $painb11,    army_painb12] {monster_footstep(FALSE);};
void()    army_painb12=[    $painb12,    army_painb13] {ai_pain(2);};
void()    army_painb13=[    $painb13,    army_painb14] {};
void()    army_painb14=[    $painb14,    army_run1    ] {};

//----------------------------------------------------------------------
void()    army_painc1    =[    $painc1,    army_painc2    ] {};
void()    army_painc2    =[    $painc2,    army_painc3    ] {ai_pain(1);};
void()    army_painc3    =[    $painc3,    army_painc4    ] {};
void()    army_painc4    =[    $painc4,    army_painc5    ] {};
void()    army_painc5    =[    $painc5,    army_painc6    ] {ai_painforward(1);};
void()    army_painc6    =[    $painc6,    army_painc7    ] {ai_painforward(1);};
void()    army_painc7    =[    $painc7,    army_painc8    ] {};
void()    army_painc8    =[    $painc8,    army_painc9    ] {ai_pain(1);};
void()    army_painc9    =[    $painc9,    army_painc10] {monster_footstep(FALSE); ai_painforward(4);};
void()    army_painc10=[    $painc10,    army_painc11] {ai_painforward(3);};
void()    army_painc11=[    $painc11,    army_painc12] {ai_painforward(6);};
void()    army_painc12=[    $painc12,    army_painc13] {ai_painforward(8);};
void()    army_painc13=[    $painc13,    army_run1] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) army_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            self.lip = random();
            if (self.lip < 0.2) {
                self.pain_finished = time + 0.6;
                sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
                army_pain1();
            }
            else if (self.lip < 0.6) {
                self.pain_finished = time + 1.1;
                sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
                army_painb1();
            }
            else {
                self.pain_finished = time + 1.1;
                sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
                army_painc1();
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 1.3;
            self.axhitme = 0;
            // Pick a random pain sound
            if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
            else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
            // Pick a random long pain animation
            if (random() < 0.5) army_painb1();
            else army_painc1();
        }
    }
};

//======================================================================
void()    army_die1    =[    $death1,    army_die2    ] {};
void()    army_die2    =[    $death2,    army_die3    ] {monster_check_gib();};
void()    army_die3    =[    $death3,    army_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack();};
void()    army_die4    =[    $death4,    army_die5    ] {};
void()    army_die5    =[    $death5,    army_die6    ] {};
void()    army_die6    =[    $death6,    army_die7    ] {};
void()    army_die7    =[    $death7,    army_die8    ] {};
void()    army_die8    =[    $death8,    army_die9    ] {};
void()    army_die9    =[    $death9,    army_die10    ] {monster_death_postcheck();};
void()    army_die10    =[    $death10,    army_die10    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    army_cdie1    =[    $deathc1,    army_cdie2    ] {};
void()    army_cdie2    =[    $deathc2,    army_cdie3    ] {monster_check_gib();ai_back(5);};
void()    army_cdie3    =[    $deathc3,    army_cdie4    ] {monster_check_gib();
    self.solid = SOLID_NOT; if (!self.gibbed) DropBackpack(); ai_back(4);};
void()    army_cdie4    =[    $deathc4,    army_cdie5    ] {ai_back(13);};
void()    army_cdie5    =[    $deathc5,    army_cdie6    ] {ai_back(3);};
void()    army_cdie6    =[    $deathc6,    army_cdie7    ] {ai_back(4);};
void()    army_cdie7    =[    $deathc7,    army_cdie8    ] {};
void()    army_cdie8    =[    $deathc8,    army_cdie9    ] {};
void()    army_cdie9    =[    $deathc9,    army_cdie10    ] {};
void()    army_cdie10    =[    $deathc10,    army_cdie11    ] {monster_death_postcheck();};
void()    army_cdie11    =[    $deathc11,    army_cdie11    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() army_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "soldier/death1.wav", 1, ATTN_NORM);
        if (random() < 0.5) army_die1 ();        // Forward
        else army_cdie1 ();                                    // Backward/side
    }
};

/*======================================================================
/*QUAKED monster_army (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_army =
{    
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_soldier.mdl";
    self.headmdl = "progs/ad171/h_soldier.mdl";
    self.gib1mdl = "progs/ad171/w_soldiergun.mdl";    // Unique weapon
    self.gib2mdl = "progs/ad171/gib_soldfoot1.mdl";    // Upright foot
    self.gib3mdl = "progs/ad171/gib_soldfoot2.mdl";    // Fallen down foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.gib1sound = GIB_IMPACT_WOOD;
    if (random() < 0.5) self.gib1skin = 1;        // Bloody weapon
    if (random() < 0.5) self.gib2mdl = string_null;
    if (random() < 0.5) self.gib3mdl = string_null;

    self.idle_sound = "soldier/idle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("soldier/death1.wav");
    self.pain_sound = "soldier/pain1.wav";
    self.pain_sound2 = "soldier/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    precache_sound ("player/udeath.wav");    // gib death

    self.sight_sound = "soldier/sight1.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 30;
    self.gibhealth = -35;                // Easy to gib
    self.gibbed = FALSE;                // Still together
    self.pain_flinch = 20;                // Easy to pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.steptype = FS_TYPEMEDIUM;        // Average feet
    self.attack_offset = '12 8 12';        // End of Gun
    if (self.deathstring == "") self.deathstring = " was shot by a Grunt\n";    

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = SoldierCheckAttack;
    self.th_stand = army_stand1;
    self.th_walk = army_walk1;
    self.th_run = army_run1;
    if (!self.th_missile) self.th_missile = army_atk1;
    self.th_pain = army_pain;
    self.th_die = army_die;

    if (!self.classtype) {
        self.classtype = CT_MONARMY;
        if (self.ammo_shells < 1) self.ammo_shells = rint(2 + random()*4);
        precache_sound ("soldier/sattck1.wav");
        // If no skin override is defined, pick a random one instead
        if (!self.exactskin) self.randomskin = 4;
    }
    self.classgroup = CG_ARMY;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};

//----------------------------------------------------------------------
void() monster_army_rocket =
{    
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONARMYROCKET;
    if (self.health < 1) self.health = 45;
    // There are two red jacket skins to pick from
    if (random() < 0.5) self.exactskin = 4;
    else self.exactskin = 5;
    if (random() < 0.35 && self.ammo_rockets < 1) self.ammo_rockets = 1;

    // Extra load/fire sounds (from Quoth MOD)
    precache_sound ("soldier/rocket_load.wav");
    precache_sound ("soldier/rocket_fire.wav");
    precache_sound ("soldier/rocket_hit.wav");
    
    self.th_missile = army_rocket_atk1;
    self.th_updmissile = army_rocket_think;    // Update flying rocket
    self.th_melee = army_runback1;
    if (self.deathstring == "") self.deathstring = " was blown up by a Rocketeer\n";
    
    monster_army();
};

//----------------------------------------------------------------------
void() monster_army_grenade =
{    
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONARMYGRENADE;
    precache_model (MODEL_PROJ_GRENADE);
    
    if (self.health < 1) self.health = 60;
    // There are two green jacket skins to pick from
    if (random() < 0.5) self.exactskin = 6;
    else self.exactskin = 7;
    if (random() < 0.35 && self.ammo_rockets < 1) self.ammo_rockets = 1;

    self.th_missile = army_grenade_atk1;
    self.th_melee = army_runback1;
    // Grenade bounce off body
    self.bouncegrenade = TRUE;
    if (self.deathstring == "") self.deathstring = " was pineappled by a Grenadier\n";
    
    monster_army();
};

//----------------------------------------------------------------------
void() monster_army_plasma =
{    
    if (deathmatch) { remove(self); return; }
    
    self.classtype = CT_MONARMYPLASMA;
    precache_model (MODEL_PROJ_PLASMA);
    
    if (self.health < 1) self.health = 75;
    // There are two blue jacket skins to pick from
    if (random() < 0.5) self.exactskin = 8;
    else self.exactskin = 9;
    if (random() < 0.35 && self.ammo_cells < 1) self.ammo_cells = 1;

    // Extra load/fire sounds (from Quoth MOD)
    precache_sound ("enforcer/elim_pg_load.wav");
    precache_sound (SOUND_PLASMA_FIRE);
    precache_sound (SOUND_PLASMA_HIT);
    
    self.th_missile = army_plasma_atk1;
    if (self.deathstring == "") self.deathstring = " was fried by an Electrocutioner\n";
    
    monster_army();
};
