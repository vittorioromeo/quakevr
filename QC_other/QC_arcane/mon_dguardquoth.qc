/*==============================================================================
 DEATH GUARD (Originally from Quoth - Kell/Necros/Preach)
  * QC was created for AD mappers to play with in their own projects
  * Original Quoth model/sounds not included with main MOD
 
  Interesting QC traits
  * Uses the Char B animation to get closer to enemies quicker
  * Melee/Range attacks are dis-used magic animations
  * Range attack does not track enemies when not in sight
  * Melee attack is single high damage animation frame
  * Has large amount of sight sounds like enforcers
  
  QUOTH assets required to get this monster working in AD
    (model's in 'progs' and wav's in 'sound' sub directories)
  * dguard.mdl -> mon_dguardq.mdl (rename file)
  * h_dguard.mdl -> h_dguardq.mdl (rename file)
  * fireball.mdl -> proj_dguardq.mdl (rename file)
  * dguard/idle.wav, dguard/fire.wav, dguard/hit.wav, dguard/slash.wav
  * dguard/slhit.wav, dguard/slmiss1.wav, dguard/slmiss2.wav
  * dguard/sight1.wav, dguard/sight2.wav, dguard/sight3.wav, dguard/sight4.wav
  * dguard/pain1.wav, dguard/pain2.wav, dguard/death1.wav, dguard/death2.wav

==============================================================================*/
// 000-008
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

// 009-028
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

// 029-036
$frame run1 run2 run3 run4 run5 run6 run7 run8

// 037-041
$frame pain1 pain2 pain3 pain4 pain5

// 042-053
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// 054-062
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

// 063-078 (not used)
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

// 079-092 (Melee swing over head attack)
$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

// 093-105 (Magic Fireball attack)
$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

// 106-111 (Movement between range and melee)
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// 112-121 (not used)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

// 122-132 (not used)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

// 133-154 (not used)
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

// 155-165 (not used)
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11

void() dguardq_sword_sound;

//===========================================================================
void()    dguardq_stand1    =[    $stand1,    dguardq_stand2    ] {monster_idle_sound(); ai_stand();};
void()    dguardq_stand2    =[    $stand2,    dguardq_stand3    ] {ai_stand();};
void()    dguardq_stand3    =[    $stand3,    dguardq_stand4    ] {ai_stand();};
void()    dguardq_stand4    =[    $stand4,    dguardq_stand5    ] {ai_stand();};
void()    dguardq_stand5    =[    $stand5,    dguardq_stand6    ] {ai_stand();};
void()    dguardq_stand6    =[    $stand6,    dguardq_stand7    ] {ai_stand();};
void()    dguardq_stand7    =[    $stand7,    dguardq_stand8    ] {ai_stand();};
void()    dguardq_stand8    =[    $stand8,    dguardq_stand9    ] {ai_stand();};
void()    dguardq_stand9    =[    $stand9,    dguardq_stand1    ] {ai_stand();};

//===========================================================================
void()    dguardq_walk1    =[    $walk1,        dguardq_walk2    ] {monster_idle_sound(); ai_walk(2);};
void()    dguardq_walk2    =[    $walk2,        dguardq_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    dguardq_walk3    =[    $walk3,        dguardq_walk4    ] {ai_walk(5);};
void()    dguardq_walk4    =[    $walk4,        dguardq_walk5    ] {ai_walk(4);};
void()    dguardq_walk5    =[    $walk5,        dguardq_walk6    ] {ai_walk(4);};
void()    dguardq_walk6    =[    $walk6,        dguardq_walk7    ] {ai_walk(2);};
void()    dguardq_walk7    =[    $walk7,        dguardq_walk8    ] {ai_walk(2);};
void()    dguardq_walk8    =[    $walk8,        dguardq_walk9    ] {ai_walk(3);};
void()    dguardq_walk9    =[    $walk9,        dguardq_walk10    ] {ai_walk(3);};
void()    dguardq_walk10    =[    $walk10,    dguardq_walk11    ] {ai_walk(4);};
void()    dguardq_walk11    =[    $walk11,    dguardq_walk12    ] {monster_footstep(FALSE); ai_walk(3);};
void()    dguardq_walk12    =[    $walk12,    dguardq_walk13    ] {ai_walk(4);};
void()    dguardq_walk13    =[    $walk13,    dguardq_walk14    ] {ai_walk(6);};
void()    dguardq_walk14    =[    $walk14,    dguardq_walk15    ] {ai_walk(2);};
void()    dguardq_walk15    =[    $walk15,    dguardq_walk16    ] {ai_walk(2);};
void()    dguardq_walk16    =[    $walk16,    dguardq_walk17    ] {ai_walk(4);};
void()    dguardq_walk17    =[    $walk17,    dguardq_walk18    ] {ai_walk(3);};
void()    dguardq_walk18    =[    $walk18,    dguardq_walk19    ] {ai_walk(3);};
void()    dguardq_walk19    =[    $walk19,    dguardq_walk20    ] {ai_walk(3);};
void()    dguardq_walk20    =[    $walk20,    dguardq_walk1    ] {ai_walk(2);};

//----------------------------------------------------------------------
void()    dguardq_run1 =[ $run1, dguardq_run2 ] {monster_idle_sound();ai_run(20);};
void()    dguardq_run2 =[ $run2, dguardq_run3 ] {monster_footstep(FALSE); ai_run(25);};
void()    dguardq_run3 =[ $run3, dguardq_run4 ] {ai_run(18);};
void()    dguardq_run4 =[ $run4, dguardq_run5 ] {ai_run(16);};
void()    dguardq_run5 =[ $run5, dguardq_run6 ] {ai_run(14);};
void()    dguardq_run6 =[ $run6, dguardq_run7 ] {monster_footstep(FALSE); ai_run(25);};
void()    dguardq_run7 =[ $run7, dguardq_run8 ] {ai_run(21);};
void()    dguardq_run8 =[ $run8, dguardq_run1 ] {ai_run(13);};

//============================================================================
// Melee attack (over head attack)
//----------------------------------------------------------------------
void() dguardq_smashforward = { ai_face(); ai_forward(1); };

//----------------------------------------------------------------------
void() dguardq_meleesmash =
{
    local float melee_dmg;

    melee_dmg = 30;
    // Always Damage any breakables
    ai_damagebreakable(melee_dmg * 3);
    
    // Check range and can damage? (bleed)
    if (ai_checkmelee(MONAI_MELEEDGUARDQ) && CanDamage (self.enemy, self)) {
        // If the enemy health is less than the melee damage, gib them
        if (self.enemy.health < melee_dmg) melee_dmg = melee_dmg * 3;
        T_Damage (self.enemy, self, self, melee_dmg, DAMARMOR);
        
        // Lots of blood and gore
        SpawnMeatSpray (self, self.enemy, crandom() * 100);
        SpawnMeatSpray (self, self.enemy, crandom() * 100);
        
        // Check for poisonous blades!
        if (self.poisonous) PoisonDeBuff(self.enemy);

        // Hit something, impact noise
        sound(self, CHAN_WEAPON, "dguard/slhit.wav", 1, ATTN_NORM);
    }
    else {
        // Missed enemy - clang!
        if (random() < 0.5) 
            sound(self, CHAN_WEAPON, "dguard/slmiss1.wav", 1, ATTN_NORM);
        else sound(self, CHAN_WEAPON, "dguard/slmiss2.wav", 1, ATTN_NORM);
    }
};

//----------------------------------------------------------------------
void()    dguardq_smash1 =[    $magica1,    dguardq_smash2    ] {
    // Always reset state (prevents double melee attack)
    self.attack_state = AS_STRAIGHT;
    sound(self, CHAN_VOICE, "dguard/slash.wav", 1, ATTN_NORM);
    dguardq_smashforward();
};
void()    dguardq_smash2 =[    $magica2,    dguardq_smash3    ] {dguardq_smashforward();};
void()    dguardq_smash3 =[    $magica3,    dguardq_smash4    ] {dguardq_smashforward();};
void()    dguardq_smash4 =[    $magica4,    dguardq_smash5    ] {dguardq_smashforward();};
void()    dguardq_smash5 =[    $magica5,    dguardq_smash6    ] {dguardq_smashforward();};
void()    dguardq_smash6 =[    $magica6,    dguardq_smash7    ] {dguardq_smashforward();};
void()    dguardq_smash7 =[    $magica7,    dguardq_smash8    ] {dguardq_smashforward();};
void()    dguardq_smash8 =[    $magica8,    dguardq_smash9    ] {dguardq_smashforward();};
void()    dguardq_smash9 =[    $magica9,    dguardq_smash10 ] {dguardq_smashforward();};
void()    dguardq_smash10 =[    $magica10,    dguardq_smash11 ] {
    dguardq_smashforward();
    dguardq_meleesmash();
};
void()    dguardq_smash11 =[    $magica11,    dguardq_smash12 ] {};
void()    dguardq_smash12 =[    $magica12,    dguardq_smash13 ] {};
void()    dguardq_smash13 =[    $magica13,    dguardq_smash14 ] {};
void()    dguardq_smash14 =[    $magica14,    dguardq_run1    ] {ai_face();};

//============================================================================
// Melee charging attack (only uses B)
//============================================================================
void() dguardq_char_b1 = [ $char_b1, dguardq_char_b2 ] {ai_charge(24); ai_melee();};
void() dguardq_char_b2 = [ $char_b2, dguardq_char_b3 ] {dguardq_sword_sound(); ai_charge(29); ai_melee();};
void() dguardq_char_b3 = [ $char_b3, dguardq_char_b4 ] {ai_charge(22); ai_melee();};
void() dguardq_char_b4 = [ $char_b4, dguardq_char_b5 ] {monster_footstep(FALSE); ai_charge(20); ai_melee();};
void() dguardq_char_b5 = [ $char_b5, dguardq_char_b6 ] {ai_charge(18); ai_melee();};
void() dguardq_char_b6 = [ $char_b6, dguardq_run1 ] {ai_charge(20); ai_melee();};

//============================================================================
// RANGE - FIREBALL (Slow and high damage)
//============================================================================
void() dguardq_fireball =
{
    local vector org, dir, vec;
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    // If cannot see enemy, instantly go back to run animation
    if ( !visxray(SUB_entEnemyTarget(), self.attack_offset, '0 0 -16', FALSE) ) {
        self.think = self.th_run;
    }
    
    // Light up knight and work out attack angle
    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);

    // Aim low to catch players with splash damage
    dir = self.attack_track - '0 0 16';
    vec = normalize(dir - org);
    
    // Variable rocket speed ( hard = baseline )
    self.attack_speed = SPEED_DGQFB + (skill * SPEED_DGQFBSKILL);
    Launch_Missile (org, vec, '0 0 0', CT_PROJ_DGUARDQ, self.attack_speed);
};
//----------------------------------------------------------------------
// Magic attack B (pointing sword) - Fireball
void()    dguardq_magicb1 =[    $magicb1,    dguardq_magicb2    ] {
    self.attack_track = self.enemy.origin;
    sound(self, CHAN_WEAPON, "dguard/fire.wav", 1, ATTN_NORM);
};
void()    dguardq_magicb2 =[    $magicb2,    dguardq_magicb3    ] {ai_trackenemy();};
void()    dguardq_magicb3 =[    $magicb3,    dguardq_magicb4    ] {ai_trackenemy();};
void()    dguardq_magicb4 =[    $magicb4,    dguardq_magicb5    ] {ai_trackenemy();};
void()    dguardq_magicb5 =[    $magicb5,    dguardq_magicb6    ] {ai_trackenemy();};
void()    dguardq_magicb6 =[    $magicb6,    dguardq_magicb7    ] {ai_trackenemy();};
void()    dguardq_magicb7 =[    $magicb7,    dguardq_magicb8    ] {ai_trackenemy();};
void()    dguardq_magicb8 =[    $magicb8,    dguardq_magicb9    ] {ai_trackenemy();};
void()    dguardq_magicb9 =[    $magicb9,    dguardq_magicb10] {
    ai_trackenemy();dguardq_fireball();};
void()    dguardq_magicb10 =[    $magicb10,    dguardq_magicb11] {};
void()    dguardq_magicb11 =[    $magicb11,    dguardq_magicb12] {};
void()    dguardq_magicb12 =[    $magicb12,    dguardq_magicb13] {};
void()    dguardq_magicb13 =[    $magicb13,    dguardq_run1] {ai_face();};

//============================================================================
void()    dguardq_pain1    =[    $pain1,        dguardq_pain2    ] {};
void()    dguardq_pain2    =[    $pain2,        dguardq_pain3    ] {};
void()    dguardq_pain3    =[    $pain3,        dguardq_pain4    ] {};
void()    dguardq_pain4    =[    $pain4,        dguardq_pain5    ] {};
void()    dguardq_pain5    =[    $pain5,        dguardq_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) dguardq_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) 
            sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
        if (self.pain_check == 1) dguardq_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.5;
            self.axhitme = 0;
            dguardq_pain1 ();
        }
    }
};

//============================================================================
void()    dguardq_die1    =[    $death1,    dguardq_die2    ] {ai_forward(10);};
void()    dguardq_die2    =[    $death2,    dguardq_die3    ] {monster_check_gib();ai_forward(8);};
void()    dguardq_die3    =[    $death3,    dguardq_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dguardq_die4    =[    $death4,    dguardq_die5    ] {};
void()    dguardq_die5    =[    $death5,    dguardq_die6    ] {};
void()    dguardq_die6    =[    $death6,    dguardq_die7    ] {};
void()    dguardq_die7    =[    $death7,    dguardq_die8    ] {};
void()    dguardq_die8    =[    $death8,    dguardq_die9    ] {ai_forward(10);};
void()    dguardq_die9    =[    $death9,    dguardq_die10    ] {ai_forward(11);};
void()    dguardq_die10    =[    $death10,    dguardq_die11    ] {};
void()    dguardq_die11    =[    $death11,    dguardq_die12    ] {monster_death_postcheck();};
void()    dguardq_die12    =[    $death12,    dguardq_die12    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    dguardq_dieb1    =[    $deathb1,    dguardq_dieb2    ] {};
void()    dguardq_dieb2    =[    $deathb2,    dguardq_dieb3    ] {monster_check_gib();};
void()    dguardq_dieb3    =[    $deathb3,    dguardq_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dguardq_dieb4    =[    $deathb4,    dguardq_dieb5    ] {};
void()    dguardq_dieb5    =[    $deathb5,    dguardq_dieb6    ] {};
void()    dguardq_dieb6    =[    $deathb6,    dguardq_dieb7    ] {};
void()    dguardq_dieb7    =[    $deathb7,    dguardq_dieb8    ] {};
void()    dguardq_dieb8    =[    $deathb8,    dguardq_dieb9    ] {monster_death_postcheck();};
void()    dguardq_dieb9    =[    $deathb9,    dguardq_dieb9    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dguardq_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    
    // regular death
    if (!self.gibbed) {
    sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        if (random() > 0.5) dguardq_die1 ();    // Forward - standard
        else dguardq_dieb1 ();                    // Backward - standard
    }
};

//============================================================================
void() dguardq_sword_sound =
{
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

/*======================================================================
 QUAKED monster_death_guard (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_death_guard =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_dguardq.mdl";            // dguard.mdl (quoth)
    self.headmdl = "progs/h_dguardq.mdl";        // h_dguard.mdl (quoth)
    self.gib1mdl = "progs/w_hknightsword.mdl";    // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";    // right foot
    
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_DGUARDQ);        // fireball.mdl (quoth)
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = string_null;
    if (random() < 0.5) self.gib3mdl = string_null;

    self.idle_sound = "dguard/idle.wav";
    precache_sound (self.idle_sound);

    // precache sight, pain and death (changes on type)
    self.sight_count = 4;
    self.sight_sound = "dguard/sight1.wav";
    self.sight_sound2 = "dguard/sight2.wav";
    self.sight_sound3 = "dguard/sight3.wav";
    self.sight_sound4 = "dguard/sight4.wav";
    precache_sound (self.sight_sound);
    precache_sound (self.sight_sound2);
    precache_sound (self.sight_sound3);
    precache_sound (self.sight_sound4);

    // Pain and Death and Taxes!
    self.pain_sound = "dguard/pain1.wav";
    self.pain_sound2 = "dguard/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    self.death_sound = "dguard/death1.wav";
    self.death_sound2 = "dguard/death2.wav";
    precache_sound (self.death_sound);
    precache_sound (self.death_sound2);

    // New fireball fire/impact sound
    precache_sound ("dguard/fire.wav");
    precache_sound ("dguard/hit.wav");

    // New sword sweep/slash sounds
    precache_sound ("dguard/slash.wav");
    precache_sound ("dguard/slhit.wav");
    precache_sound ("dguard/slmiss1.wav");
    precache_sound ("dguard/slmiss2.wav");

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");
        
    self.solid = SOLID_NOT;                    // No interaction with world
    self.movetype = MOVETYPE_NONE;            // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 125;
    self.gibhealth = -40;                    // Not easy to gib
    self.gibbed = FALSE;                    // Still in one piece
    self.pain_flinch = 40;                    // Medium pain threshold
    self.pain_timeout = 1.25;                // Higher than a knight
    self.pain_longanim = TRUE;                // can be chopped with shadow axe
    self.steptype = FS_TYPEMEDIUM;            // Lighter than Hell Knight
    self.attack_offset = '20 0 24';            // End of sword
    self.poisonous = FALSE;                    // No support for this
    self.deathstring = " was slaughtered by a Death Guard\n";
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Using alt magic and charge attacks
    self.th_checkattack = DGuardQCheckAttack;
    self.th_stand = dguardq_stand1;        // Idling on feet
    self.th_walk = dguardq_walk1;        // Walking
    self.th_run = dguardq_run1;            // Running
    self.th_melee = dguardq_smash1;        // Sword attack
    self.th_charge = dguardq_char_b1;    // Charge attack
    self.th_missile = dguardq_magicb1;    // Fireball
    self.th_pain = dguardq_pain;        // Stumble and pain
    self.th_die = dguardq_die;            // Face meets floor
    
    self.classtype = CT_MONDGUARDQ;
    self.classgroup = CG_DKNIGHT;        // Classic tincan!
    self.classmove = MON_MOVEWALK;
    monster_start();
};
