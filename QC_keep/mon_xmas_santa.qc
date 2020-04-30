/*==============================================================================
XMAS Santa - From Twisted Christmas Mod by Twisted Matrix
==============================================================================*/

// (000 - 005) Standing
$frame stand1 stand2 stand3 stand4 stand5 stand6

// (006 - 011) Running
$frame run1 run2 run3 run4 run5 run6

// (012 - 017) Lounging Evil Santa 
$frame lounge1 lounge2 lounge3 lounge4 lounge5 lounge6

// (018 - 027) Victory at player's death!
$frame victory1 victory2 victory3 victory4 victory5 victory6
$frame victory7 victory8 victory9 victory10

// (028 - 031) Gun Range attack
$frame attack1 attack2 attack3 attack4

// (032 - 041) Sack Melee attack
$frame sack1 sack2 sack3 sack4 sack5 sack6 sack7 sack8 sack9 sack10

// (042 - 046) Pain A
$frame painA1 painA2 painA3 painA4 painA5

// (047 - 051) Pain B
$frame painB1 painB2 painB3 painB4 painB5

// (052 - 061) Death FORWARD (180 spin)
$frame death1 death2 death3 death4 death5 death6
$frame death7 death8 death9 death10

// (062 - 070) Death BACKWARD (180 spin)
$frame bdeath1 bdeath2 bdeath3 bdeath4 bdeath5 bdeath6
$frame bdeath7 bdeath8 bdeath9 bdeath10

// (071 - 077) Sitting Good Santa 1
$frame sitA1 sitA2 sitA3 sitA4 sitA5 sitA6 sitA7

// (078 - 083) Sitting Good Santa 2
$frame sitB1 sitB2 sitB3 sitB4 sitB5 sitB6

// (084 - 086) Misc modelling stuff
$frame base1 base2 base3

//======================================================================
// Sitting down animation (located below model baseline)
// There is no blend animation for this to go from sitting to standing
// Original MOD had this animation as a standing alternative
// Really looked odd when the monster jumped straight to running
/*
void() santa_lounge1 = [ $lounge1, santa_lounge2 ] {monster_idle_sound();ai_stand();};
void() santa_lounge2 = [ $lounge2, santa_lounge3 ] {ai_stand();};
void() santa_lounge3 = [ $lounge3, santa_lounge4 ] {ai_stand();};
void() santa_lounge4 = [ $lounge4, santa_lounge5 ] {ai_stand();};
void() santa_lounge5 = [ $lounge5, santa_lounge6 ] {ai_stand();};
void() santa_lounge6 = [ $lounge6, santa_lounge1 ] {ai_stand();}; */

//======================================================================
// A scaled down good santa sitting shaking his head
//======================================================================
void() santa_seatA1 = [ $sitA1, santa_seatA2 ] {};
void() santa_seatA2 = [ $sitA2, santa_seatA3 ] {};
void() santa_seatA3 = [ $sitA3, santa_seatA4 ] {};
void() santa_seatA4 = [ $sitA4, santa_seatA5 ] {};
void() santa_seatA5 = [ $sitA5, santa_seatA6 ] {};
void() santa_seatA6 = [ $sitA6, santa_seatA7 ] {};
void() santa_seatA7 = [ $sitA7, santa_seatA1 ] {self.th_stand();};

//----------------------------------------------------------------------
void() santa_seatB1 = [ $sitB1, santa_seatB2 ] {};
void() santa_seatB2 = [ $sitB2, santa_seatB3 ] {};
void() santa_seatB3 = [ $sitB3, santa_seatB4 ] {};
void() santa_seatB4 = [ $sitB4, santa_seatB5 ] {};
void() santa_seatB5 = [ $sitB5, santa_seatB6 ] {};
void() santa_seatB6 = [ $sitB6, santa_seatB1 ] {self.th_stand();};

//----------------------------------------------------------------------
void() santa_trigger =
{
    // Trigger Once
    if (self.waitmin > time) return;
    self.waitmin = LARGE_TIMER;
    self.use = SUB_Null;
    
    // "My dear boy, you saved me!"
    sound(self, CHAN_WEAPON, self.noise2, 1, ATTN_NORM);
    
    // Fire all triggers/sounds/messages
    SUB_UseTargets();
    
    // Fade out santa (+delete model)
    self.think = model_fade;
    self.nextthink = time + 2 + random();
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
void() santa_seated =
{
    // Keep resetting health/touch function
    self.health = self.max_health;
    self.use = santa_trigger;
    if (random() < 0.5) santa_seatA1();
    else santa_seatB1();
};

//======================================================================
void() santa_stand1 = [ $stand1, santa_stand2 ] {monster_idle_sound();ai_stand();};
void() santa_stand2 = [ $stand2, santa_stand3 ] {ai_stand();};
void() santa_stand3 = [ $stand3, santa_stand4 ] {ai_stand();};
void() santa_stand4 = [ $stand4, santa_stand5 ] {ai_stand();};
void() santa_stand5 = [ $stand5, santa_stand6 ] {ai_stand();};
void() santa_stand6 = [ $stand6, santa_stand1 ] {ai_stand();};

//======================================================================
void() santa_walk1 = [ $run1, santa_walk2 ] {monster_idle_sound();ai_walk(15);}
void() santa_walk2 = [ $run2, santa_walk3 ] {ai_walk(17);};
void() santa_walk3 = [ $run3, santa_walk4 ] {monster_footstep(FALSE);ai_walk(15);};
void() santa_walk4 = [ $run4, santa_walk5 ] {ai_walk(15);};
void() santa_walk5 = [ $run5, santa_walk6 ] {ai_walk(17);};
void() santa_walk6 = [ $run6, santa_walk1 ] {monster_footstep(FALSE);ai_walk(15);};

//======================================================================
void() santa_run1 = [ $run1, santa_run2 ] {monster_idle_sound();ai_run(20);}
void() santa_run2 = [ $run2, santa_run3 ] {ai_run(24);};
void() santa_run3 = [ $run3, santa_run4 ] {monster_footstep(FALSE);ai_run(20);};
void() santa_run4 = [ $run4, santa_run5 ] {ai_run(20);};
void() santa_run5 = [ $run5, santa_run6 ] {ai_run(24);};
void() santa_run6 = [ $run6, santa_run1 ] {monster_footstep(FALSE);ai_run(20);};

//======================================================================
// MELEE : Sack of Presents
//======================================================================
void() SantaMelee =
{
    local float ldmg;

    if (!self.enemy) return;
    if (self.health < 1) return;
        
    ai_charge(10);    // + ai_face()
    ai_damagebreakable(50);
    if (!ai_checkmelee(MONAI_MELEESANTA)) return;

    // Can the target bleed?
    if (!self.enemy.takedamage) return;

    sound(self, CHAN_WEAPON, "xmas/santa/sack_hit.wav", 1, ATTN_NORM);

    // Sack attack is strong (1-60)
    ldmg = (random() + random() + random()) * 20;
    if (ldmg < 1) ldmg = 1;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);

    // Spawn blood at sack
    spawn_touchblood (self, self.enemy, ldmg*3);
    // Lots of blood and gore
    SpawnMeatSpray (self, self.enemy, crandom() * 100);
    SpawnMeatSpray (self, self.enemy, crandom() * 100);
};

//----------------------------------------------------------------------
void() santa_sack1 = [ $sack1, santa_sack2 ] {ai_face();};
void() santa_sack2 = [ $sack2, santa_sack3 ] {};
void() santa_sack3 = [ $sack3, santa_sack4 ] {ai_face();};
void() santa_sack4 = [ $sack4, santa_sack5 ] {ai_face();
    // This sound is exactly 0.4s long, with hit to complete
    sound(self, CHAN_WEAPON, "xmas/santa/sack_swing.wav", 1, ATTN_NORM);
};
void() santa_sack5 = [ $sack5, santa_sack6 ] {ai_charge(10);};
void() santa_sack6 = [ $sack6, santa_sack7 ] {ai_charge(10);};
void() santa_sack7 = [ $sack7, santa_sack8 ] {ai_charge(10);};
void() santa_sack8 = [ $sack8, santa_sack9 ] {SantaMelee();};
void() santa_sack9 = [ $sack9, santa_sack10 ] {};
void() santa_sack10 = [ $sack10, santa_run1 ] {
    // Check if snowman is within range to attack again
    if (ai_checkmelee(MONAI_MELEESANTA) && self.enemy.health > 0) {
        if (self.health > 0) self.think = santa_sack3;
    }
};

//======================================================================
// RANGE : Rocket Gun
//======================================================================
void() Santa_FireSnow =
{
    local vector org, dir, spot1, spot2;
    
    // Keep adding up projectile counter
    self.attack_chance = self.attack_chance + 1;
    
    // Lit up that barrel
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_SANTAPROJ + (skill * SPEED_SANTASKILL);
    sound (self, CHAN_WEAPON, "xmas/santa/fire.wav", 1, ATTN_NORM);    

    // Keep tracking enemy origin (similar to wizards)
    ai_trackenemy();

    // Calculate the center of the gun
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    // Work out projectil direction
    dir = normalize(self.attack_track - org);
    
    // Barrel 1
    spot1 = org + v_right*6;
    spot2 = normalize((self.attack_track+v_right*6) - spot1);
    launch_projectile (spot1, spot2, CT_PROJ_SANTA, self.attack_speed);
    
    // Barrel 2
    spot1 = org - v_right*6;
    spot2 = normalize((self.attack_track-v_right*6) - spot1);
    launch_projectile (spot1, spot2, CT_PROJ_SANTA, self.attack_speed);
};

//----------------------------------------------------------------------
void() santa_snow1 = [ $attack1, santa_snow2 ] {ai_trackenemy();
    // First 0.2s is repeatable to get machine gun sound
    sound(self, CHAN_WEAPON, "xmas/santa/fire.wav", 1, ATTN_NORM);
};
void() santa_snow2 = [ $attack2, santa_snow3 ] {ai_trackenemy();};
void() santa_snow3 = [ $attack3, santa_snow4 ] {Santa_FireSnow();};
void() santa_snow4 = [ $attack4, santa_run1 ] {ai_trackenemy();
    // Anymore projectiles to fire?
    if (self.attack_chance < MONAI_SANTAMAX) {
        // Is the player still visible?
        if ( visxray(self.enemy, self.attack_offset, '0 0 0', FALSE) ) {
            self.attack_timer = time + 0.8;
        }
        // Is the enemy in view timer still active?
        if (self.attack_timer > time) self.think = santa_snow1;
    }    
};

void() santa_snowstart =
{
    self.attack_track = self.enemy.origin;
    self.attack_chance = self.attack_timer = 0;
    santa_snow1();
};

//======================================================================
// PAIN : quick reflection
//======================================================================
void() santa_painA1 = [ $painA1, santa_painA2 ] {};
void() santa_painA2 = [ $painA2, santa_painA3 ] {};
void() santa_painA3 = [ $painA3, santa_painA4 ] {};
void() santa_painA4 = [ $painA4, santa_painA5 ] {};
void() santa_painA5 = [ $painA5, santa_run1 ] {};

//----------------------------------------------------------------------
void() santa_painB1 = [ $painB1, santa_painB2 ] {};
void() santa_painB2 = [ $painB2, santa_painB3 ] {};
void() santa_painB3 = [ $painB3, santa_painB4 ] {};
void() santa_painB4 = [ $painB4, santa_painB5 ] {};
void() santa_painB5 = [ $painB5, santa_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) santa_pain =
{
    // Good santa cannot feel pain or die
    if (self.spawnflags & MON_SANTA_GOOD) return;
    
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (random() < 0.8) santa_painA1 ();
            else {
                santa_painB1 ();
                self.pain_finished = time + 2;
            }
        }
    }
};

//======================================================================
// Typical forward/backward death animation
//======================================================================
void() santa_die1 =[ $death1, santa_die2] {};
void() santa_die2 =[ $death2, santa_die3] {monster_check_gib();};
void() santa_die3 =[ $death3, santa_die4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() santa_die4 =[ $death4, santa_die5] {};
void() santa_die5 =[ $death5, santa_die6] {};
void() santa_die6 =[ $death6, santa_die7] {};
void() santa_die7 =[ $death7, santa_die8] {};
void() santa_die8 =[ $death8, santa_die9] {};
void() santa_die9 =[ $death9, santa_die10] {monster_death_postcheck();};
void() santa_die10 =[ $death10, santa_die10] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() santa_dieb1 =[ $bdeath1, santa_dieb2] {};
void() santa_dieb2 =[ $bdeath2, santa_dieb3] {monster_check_gib();};
void() santa_dieb3 =[ $bdeath3, santa_dieb4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() santa_dieb4 =[ $bdeath4, santa_dieb5] {};
void() santa_dieb5 =[ $bdeath5, santa_dieb6] {};
void() santa_dieb6 =[ $bdeath6, santa_dieb7] {};
void() santa_dieb7 =[ $bdeath7, santa_dieb8] {};
void() santa_dieb8 =[ $bdeath8, santa_dieb9] {ai_back(4);monster_death_postcheck();};
void() santa_dieb9 =[ $bdeath9, santa_dieb9] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() santa_die =
{
    // Good santa cannot feel pain or die
    if (self.spawnflags & MON_SANTA_GOOD) return;
    
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);

        // Pick a random fall direction
        if (random() > 0.5) santa_die1 ();
        else santa_dieb1 ();
    }
};

/*======================================================================
QUAKED monster_santa (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_xmas_santa =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/xmas/mon_santa.mdl";        // Big Sack Man!
    self.headmdl = "progs/xmas/h_santa.mdl";    // Pointy Face
    self.gib1mdl = "progs/xmas/gib_santagun.mdl";
    self.gib2mdl = "progs/xmas/gib_santasack.mdl";
    self.gib3mdl = "progs/xmas/gib_snowball.mdl";    // Snowballs
    self.gib1sound = GIB_IMPACT_WOOD;
    self.gib2sound = GIB_IMPACT_WOOD;

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (MODEL_PROJ_SNOWBALL);

    // Idle/Sight sounds for GOOD/EVIL santa
    if (self.spawnflags & MON_SANTA_GOOD) {
        self.idle_sound = "xmas/santa/good_hohoho.wav";
        precache_sound (self.idle_sound);
        self.sight_sound = "xmas/santa/good_hohoho.wav";
        precache_sound (self.sight_sound);    
        // Touch trigger - "My dear boy, you saved me!"
        self.noise2 = "xmas/santa/good_saved.wav";
        precache_sound (self.noise2);
    }
    else {
        // Evil santa has combat idle sounds
        self.idle_sound = "xmas/santa/hohoho.wav";    // Slow ho-ho-ho
        self.idle_sound2 = "xmas/santa/laugh1.wav";    // long laugh
        precache_sound (self.idle_sound);
        precache_sound (self.idle_sound2);
        self.idle_soundcom = "xmas/santa/laugh3_com.wav";
        self.idle_soundcom2 = "xmas/santa/hohoho.wav";
        precache_sound (self.idle_soundcom);
        precache_sound (self.idle_soundcom2);
        // Sight sounds - "Come sit on santa's lap little human!"
        self.sight_sound = "xmas/santa/sight.wav";
        precache_sound (self.sight_sound);
    }
    
    // Attack melee/range
    precache_sound ("xmas/santa/sack_swing.wav");
    precache_sound ("xmas/santa/sack_hit.wav");
    precache_sound ("xmas/santa/fire.wav");
    
    // precache sight, pain and death (changes on type)
    self.pain_sound = "xmas/santa/pain1.wav";
    self.pain_sound2 = "xmas/santa/pain2.wav";
    // Fading out slow ho-ho-ho death sound
    self.death_sound = "xmas/santa/death2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound (self.death_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)        

    if (self.health < 1) self.health = 1200;
    self.gibhealth = -60;            // Difficult to gib
    self.blockudeath = TRUE;        // no humanoid death sound
    self.gibbed = FALSE;            // Still in one peice
    self.pain_flinch = 200;            // Strong pain threshold
    self.pain_timeout = 2;            // Stop constant pain
    self.infightextra = 8;            // Does not like infighting
    self.pain_longanim = FALSE;        // No long pain animation
    self.no_liquiddmg = TRUE;        // no slime/lava damage
    self.steptype = FS_TYPEHEAVY;    // Giant boots!
    self.poisonous = FALSE;            // Not Supported
    self.attack_offset = '28 22 26';// End of gun
    self.attack_rage = TRUE;        // Not in combat yet
    self.deathstring = " was wrapped up by Santa\n";
    
    // Setup Ammo Resistance
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0.5;
    self.reflectlightning = TRUE;    // Reflect lightning strikes
    self.reflectplasma = TRUE;        // Reflect plasma projectiles

    // Pain/Death functions
    self.th_pain = santa_pain;
    self.th_die = santa_die;

    // Check for GOOD Santa setup?
    if (self.spawnflags & MON_SANTA_GOOD) {
        // No targetname = no trigger!
        if (self.targetname == "") {
            dprint("\b[GOOD SANTA]\b Missing targetname name!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            return;
        }
        // Setup default rescue message
        self.noise = SOUND_EMPTY;
        if (self.message == "")
             self.message = "You have rescued Santa!\nfrom the claws of hell!\n";
         // Nobody should be trying to kill good santa, WTF!?!
        self.resist_shells = self.resist_nails = 1.0;
        self.resist_rockets = self.resist_cells = 1.0;
        self.nomonstercount = TRUE;
        self.health = self.max_health = MEGADEATH;
        self.gibhealth = MON_NEVERGIB;
        self.exactskin = 1;
        // Direct every state to the sit function
        self.th_stand = santa_seated;
        self.th_walk = santa_seated;
        self.th_run = santa_seated;
        self.waitmin = 0;
    }
    else {
        // Default EVIL Santa!
        self.th_checkattack = SantaCheckAttack;
        self.th_stand = santa_stand1;
        self.th_walk = santa_walk1;
        self.th_run = santa_run1;
        self.th_melee = santa_sack1;
        self.th_missile = santa_snowstart;
    }
        
    self.classtype = CT_MONSANTA;
    self.classgroup = CG_XMAS;
    self.classmove = MON_MOVEWALK;

    monster_start();    
};