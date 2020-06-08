/*==============================================================================
KNIGHT
==============================================================================*/
$cd id1/models/knight
$origin 0 0 24
$base base
$skin badass3

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame runb1 runb2 runb3 runb4 runb5 runb6 runb7 runb8

$frame runattack1 runattack2 runattack3 runattack4 runattack5
$frame runattack6 runattack7 runattack8 runattack9 runattack10
$frame runattack11

$frame pain1 pain2 pain3

$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9
$frame painb10 painb11

//$frame attackdummy
$frame attackb1 attackb2 attackb3 attackb4 attackb5
$frame attackb6 attackb7 attackb8 attackb9 attackb10

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9 deathb10 deathb11

// New - standing idleA - look left
$frame idlea1 idlea2 idlea3 idlea4 idlea5 idlea6 idlea7 idlea8
$frame idlea9 idlea10 idlea11 idlea12 idlea13 idlea14 idlea15 idlea16 

// New - standing idleB - look right
$frame idleb1 idleb2 idleb3 idleb4 idleb5 idleb6 idleb7 idleb8
$frame idleb9 idleb10 idleb11 idleb12 idleb13 idleb14

// New - Standing to standing upright
$frame attnin1 attnin2 attnin3 attnin4

// Standing upright
$frame attn1 attn2 attn3 attn4

// New - Standing upright to standing
$frame attnout1 attnout2 attnout3 attnout4

// Standing to Knee down
$frame kneeldn1 kneeldn2 kneeldn3 kneeldn4 kneeldn5 kneeldn6
$frame kneeldn7 kneeldn8 kneeldn9

// New - Kneeling idle
$frame kneel1 kneel2 kneel3 kneel4 kneel5 kneel6

// New - Kneeling down to Standing up
$frame kneelup1 kneelup2 kneelup3 kneelup4 kneelup5 kneelup6 
$frame kneelup7 kneelup8 kneelup9 kneelup10 kneelup11

// New - Kneeling down to running
$frame kneelrun1 kneelrun2 kneelrun3 kneelrun4 kneelrun5
$frame kneelrun6 kneelrun7 kneelrun8

void() knight_sword_sound;
void() knight_walk1;
void() knight_run1;

//===========================================================================
void()    knight_idlea1    =[    $idlea1,    knight_idlea2    ] {ai_stand();};
void()    knight_idlea2    =[    $idlea2,    knight_idlea3    ] {ai_stand();};
void()    knight_idlea3    =[    $idlea3,    knight_idlea4    ] {ai_stand();};
void()    knight_idlea4    =[    $idlea4,    knight_idlea5    ] {ai_stand();};
void()    knight_idlea5    =[    $idlea5,    knight_idlea6    ] {ai_stand();};
void()    knight_idlea6    =[    $idlea6,    knight_idlea7    ] {ai_stand();};
void()    knight_idlea7    =[    $idlea7,    knight_idlea8    ] {ai_stand();};
void()    knight_idlea8    =[    $idlea8,    knight_idlea9    ] {ai_stand();};
void()    knight_idlea9    =[    $idlea9,    knight_idlea10    ] {ai_stand();};
void()    knight_idlea10    =[    $idlea10,    knight_idlea11    ] {ai_stand();};
void()    knight_idlea11    =[    $idlea11,    knight_idlea12    ] {ai_stand();};
void()    knight_idlea12    =[    $idlea12,    knight_idlea13    ] {ai_stand();};
void()    knight_idlea13    =[    $idlea13,    knight_idlea14    ] {ai_stand();};
void()    knight_idlea14    =[    $idlea14,    knight_idlea15    ] {ai_stand();};
void()    knight_idlea15    =[    $idlea15,    knight_idlea16    ] {ai_stand();};
void()    knight_idlea16    =[    $idlea16,    knight_stand1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    knight_idleb1    =[    $idleb1,    knight_idleb2    ] {ai_stand();};
void()    knight_idleb2    =[    $idleb2,    knight_idleb3    ] {ai_stand();};
void()    knight_idleb3    =[    $idleb3,    knight_idleb4    ] {ai_stand();};
void()    knight_idleb4    =[    $idleb4,    knight_idleb5    ] {ai_stand();};
void()    knight_idleb5    =[    $idleb5,    knight_idleb6    ] {ai_stand();};
void()    knight_idleb6    =[    $idleb6,    knight_idleb7    ] {ai_stand();};
void()    knight_idleb7    =[    $idleb7,    knight_idleb8    ] {ai_stand();};
void()    knight_idleb8    =[    $idleb8,    knight_idleb9    ] {ai_stand();};
void()    knight_idleb9    =[    $idleb9,    knight_idleb10    ] {ai_stand();};
void()    knight_idleb10    =[    $idleb10,    knight_idleb11    ] {ai_stand();};
void()    knight_idleb11    =[    $idleb11,    knight_idleb12    ] {ai_stand();};
void()    knight_idleb12    =[    $idleb12,    knight_idleb13    ] {ai_stand();};
void()    knight_idleb13    =[    $idleb13,    knight_idleb14    ] {ai_stand();};
void()    knight_idleb14    =[    $idleb14,    knight_stand1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    knight_kneelup1    =[    $kneelup1,    knight_kneelup2    ] {ai_stand();};
void()    knight_kneelup2    =[    $kneelup2,    knight_kneelup3    ] {ai_stand();};
void()    knight_kneelup3    =[    $kneelup3,    knight_kneelup4    ] {ai_stand();};
void()    knight_kneelup4    =[    $kneelup4,    knight_kneelup5    ] {ai_stand();};
void()    knight_kneelup5    =[    $kneelup5,    knight_kneelup6    ] {ai_stand();};
void()    knight_kneelup6    =[    $kneelup6,    knight_kneelup7    ] {ai_stand();};
void()    knight_kneelup7    =[    $kneelup7,    knight_kneelup8    ] {ai_stand();};
void()    knight_kneelup8    =[    $kneelup8,    knight_kneelup9    ] {ai_stand();};
void()    knight_kneelup9    =[    $kneelup9,    knight_kneelup10] {ai_stand();};
void()    knight_kneelup10=[    $kneelup10,    knight_kneelup11] {ai_stand();};
void()    knight_kneelup11=[    $kneelup11,    knight_stand1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    knight_kneel1    =[    $kneel1,    knight_kneel2    ] {ai_stand();monster_idle_sound();};
void()    knight_kneel2    =[    $kneel2,    knight_kneel3    ] {ai_stand();};
void()    knight_kneel3    =[    $kneel3,    knight_kneel4    ] {ai_stand();};
void()    knight_kneel4    =[    $kneel4,    knight_kneel5    ] {ai_stand();};
void()    knight_kneel5    =[    $kneel5,    knight_kneel6    ] {ai_stand();};
void()    knight_kneel6    =[    $kneel6,    knight_kneel1    ] {ai_stand();
    if (random() < MON_IDLE_ANIMATION && self.weaponswitch < time) 
        self.think = knight_kneelup1;    // Start standing up
};

//----------------------------------------------------------------------
void()    knight_kneeldn1    =[    $kneeldn1,    knight_kneeldn2    ] {ai_stand();};
void()    knight_kneeldn2    =[    $kneeldn2,    knight_kneeldn3    ] {ai_stand();};
void()    knight_kneeldn3    =[    $kneeldn3,    knight_kneeldn4    ] {ai_stand();};
void()    knight_kneeldn4    =[    $kneeldn4,    knight_kneeldn5    ] {ai_stand();};
void()    knight_kneeldn5    =[    $kneeldn5,    knight_kneeldn6    ] {ai_stand();};
void()    knight_kneeldn6    =[    $kneeldn6,    knight_kneeldn7    ] {ai_stand();};
void()    knight_kneeldn7    =[    $kneeldn7,    knight_kneeldn8    ] {ai_stand();};
void()    knight_kneeldn8    =[    $kneeldn8,    knight_kneeldn9    ] {ai_stand();};
void()    knight_kneeldn9    =[    $kneeldn9,    knight_kneel1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    knight_attn1    =[    $attn1,        knight_attn2    ] {ai_stand();monster_idle_sound();};
void()    knight_attn2    =[    $attn2,        knight_attn3    ] {ai_stand();};
void()    knight_attn3    =[    $attn3,        knight_attn4    ] {ai_stand();};
void()    knight_attn4    =[    $attn4,        knight_attn1    ] {ai_stand();};

//----------------------------------------------------------------------
void()    knight_stand1    =[    $stand1,    knight_stand2    ] {ai_stand();monster_idle_sound();};
void()    knight_stand2    =[    $stand2,    knight_stand3    ] {ai_stand();};
void()    knight_stand3    =[    $stand3,    knight_stand4    ] {ai_stand();};
void()    knight_stand4    =[    $stand4,    knight_stand5    ] {ai_stand();};
void()    knight_stand5    =[    $stand5,    knight_stand6    ] {ai_stand();};
void()    knight_stand6    =[    $stand6,    knight_stand7    ] {ai_stand();};
void()    knight_stand7    =[    $stand7,    knight_stand8    ] {ai_stand();};
void()    knight_stand8    =[    $stand8,    knight_stand9    ] {ai_stand();};
void()    knight_stand9    =[    $stand9,    knight_stand1    ] {
    self.idlebusy = FALSE; 
    if (random() < MON_IDLE_ANIMATION && self.weaponswitch < time) {
        // Slow down the repeating of idle animations
        self.weaponswitch = time + 1 + random()*3;
        self.idlebusy = TRUE; 

        self.lefty = 1 - self.lefty;
        if (self.lefty == 1) self.think = knight_idlea1;    // Look left
        else self.think = knight_idleb1;                    // Look right
    }
};

//===========================================================================
void()    knight_walk1    =[    $walk1,        knight_walk2    ] {
    monster_footstep(FALSE); monster_idle_sound(); ai_walk(3);};
void()    knight_walk2    =[    $walk2,        knight_walk3    ] {ai_walk(2);};
void()    knight_walk3    =[    $walk3,        knight_walk4    ] {ai_walk(3);};
void()    knight_walk4    =[    $walk4,        knight_walk5    ] {ai_walk(4);};
void()    knight_walk5    =[    $walk5,        knight_walk6    ] {ai_walk(3);};
void()    knight_walk6    =[    $walk6,        knight_walk7    ] {ai_walk(3);};
void()    knight_walk7    =[    $walk7,        knight_walk8    ] {ai_walk(3);};
void()    knight_walk8    =[    $walk8,        knight_walk9    ] {monster_footstep(FALSE); ai_walk(4);};
void()    knight_walk9    =[    $walk9,        knight_walk10    ] {ai_walk(3);};
void()    knight_walk10    =[    $walk10,    knight_walk11    ] {ai_walk(3);};
void()    knight_walk11    =[    $walk11,    knight_walk12    ] {ai_walk(2);};
void()    knight_walk12    =[    $walk12,    knight_walk13    ] {ai_walk(3);};
void()    knight_walk13    =[    $walk13,    knight_walk14    ] {ai_walk(4);};
void()    knight_walk14    =[    $walk14,    knight_walk1    ] {ai_walk(3);};

//===========================================================================
void()    knight_run1    =[    $runb1,        knight_run2    ] {monster_idle_sound(); ai_run(16);};
void()    knight_run2    =[    $runb2,        knight_run3    ] {monster_footstep(FALSE); ai_run(20);};
void()    knight_run3    =[    $runb3,        knight_run4    ] {ai_run(13);};
void()    knight_run4    =[    $runb4,        knight_run5    ] {ai_run(7);};
void()    knight_run5    =[    $runb5,        knight_run6    ] {ai_run(16);};
void()    knight_run6    =[    $runb6,        knight_run7    ] {monster_footstep(FALSE); ai_run(20);};
void()    knight_run7    =[    $runb7,        knight_run8    ] {ai_run(14);};
void()    knight_run8    =[    $runb8,        knight_run1    ] {ai_run(6);};

//============================================================================
void()    knight_runatk1    =[    $runattack1,        knight_runatk2    ] { ai_charge(20); };
void()    knight_runatk2    =[    $runattack2,    knight_runatk3    ] {ai_charge_side();};
void()    knight_runatk3    =[    $runattack3,    knight_runatk4    ] {ai_charge_side();};
void()    knight_runatk4    =[    $runattack4,    knight_runatk5    ] {knight_sword_sound(); ai_charge_side();};
void()    knight_runatk5    =[    $runattack5,    knight_runatk6    ] {ai_melee_side();};
void()    knight_runatk6    =[    $runattack6,    knight_runatk7    ] {monster_footstep(FALSE); ai_melee_side();};
void()    knight_runatk7    =[    $runattack7,    knight_runatk8    ] {ai_melee_side();};
void()    knight_runatk8    =[    $runattack8,    knight_runatk9    ] {ai_melee_side();};
void()    knight_runatk9    =[    $runattack9,    knight_runatk10    ] {ai_melee_side();};
void()    knight_runatk10    =[    $runattack10,    knight_runatk11    ] {monster_footstep(FALSE); ai_charge_side();};
void()    knight_runatk11    =[    $runattack11,    knight_run1    ] {ai_charge(10);};

//----------------------------------------------------------------------
void()    knight_atk1    =[    $attackb1,        knight_atk2    ] {ai_charge(0);};
void()    knight_atk2    =[    $attackb2,        knight_atk3    ] {ai_charge(7);};
void()    knight_atk3    =[    $attackb3,        knight_atk4    ] {ai_charge(4);};
void()    knight_atk4    =[    $attackb4,        knight_atk5    ] {knight_sword_sound(); ai_charge(0);};
void()    knight_atk5    =[    $attackb5,        knight_atk6    ] {monster_footstep(FALSE); ai_charge(3);};
void()    knight_atk6    =[    $attackb6,        knight_atk7    ] {ai_charge(4); ai_melee();};
void()    knight_atk7    =[    $attackb7,        knight_atk8    ] {ai_charge(1); ai_melee();};
void()    knight_atk8    =[    $attackb8,        knight_atk9    ] {ai_charge(3); ai_melee();};
void()    knight_atk9    =[    $attackb9,        knight_atk10] {ai_charge(1);};
void()    knight_atk10=[    $attackb10,        knight_run1    ] {ai_charge(5);};

//=============================================================================
void() knight_melee =
{
    if (ai_checkmelee(MONAI_MELEEKNIGHT)) knight_atk1 ();
    else knight_runatk1 ();
};

//===========================================================================
void()    knight_pain1    =[    $pain1,        knight_pain2    ] {};
void()    knight_pain2    =[    $pain2,        knight_pain3    ] {};
void()    knight_pain3    =[    $pain3,        knight_run1    ] {};

void()    knight_painb1    =[    $painb1,    knight_painb2    ] {ai_painforward(0);};
void()    knight_painb2    =[    $painb2,    knight_painb3    ] {ai_painforward(3);};
void()    knight_painb3    =[    $painb3,    knight_painb4    ] {};
void()    knight_painb4    =[    $painb4,    knight_painb5    ] {monster_footstep(FALSE);};
void()    knight_painb5    =[    $painb5,    knight_painb6    ] {ai_painforward(2);};
void()    knight_painb6    =[    $painb6,    knight_painb7    ] {ai_painforward(4);};
void()    knight_painb7    =[    $painb7,    knight_painb8    ] {ai_painforward(2);};
void()    knight_painb8    =[    $painb8,    knight_painb9    ] {ai_painforward(5);monster_footstep(FALSE);};
void()    knight_painb9    =[    $painb9,    knight_painb10    ] {ai_painforward(5);};
void()    knight_painb10    =[    $painb10,    knight_painb11    ] {ai_painforward(0);};
void()    knight_painb11    =[    $painb11,    knight_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) knight_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) {
            // Randomly pick which pain animation to play
            if (random() < 0.85) knight_pain1 ();    // classic, body recoil
            else {
                self.pain_finished = time + 2;        // long animation
                knight_painb1 ();                    // Cool stumble, hand over mouth
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.axhitme = 0;
            self.pain_finished = time + 1.1;
            knight_painb1 ();
        }
    }
};

//============================================================================
void() knight_die1 =[ $death1, knight_die2] {};
void() knight_die2 =[ $death2, knight_die3] {monster_check_gib();};
void() knight_die3 =[ $death3, knight_die4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() knight_die4 =[ $death4, knight_die5] {};
void() knight_die5 =[ $death5, knight_die6] {};
void() knight_die6 =[ $death6, knight_die7] {};
void() knight_die7 =[ $death7, knight_die8] {};
void() knight_die8 =[ $death8, knight_die9] {};
void() knight_die9 =[ $death9, knight_die10] {monster_death_postcheck();};
void() knight_die10 =[ $death10, knight_die10] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() knight_dieb1 =[ $deathb1, knight_dieb2] {};
void() knight_dieb2 =[ $deathb2, knight_dieb3] {monster_check_gib();};
void() knight_dieb3 =[ $deathb3, knight_dieb4] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() knight_dieb4 =[ $deathb4, knight_dieb5] {};
void() knight_dieb5 =[ $deathb5, knight_dieb6] {};
void() knight_dieb6 =[ $deathb6, knight_dieb7] {};
void() knight_dieb7 =[ $deathb7, knight_dieb8] {};
void() knight_dieb8 =[ $deathb8, knight_dieb9] {};
void() knight_dieb9 =[ $deathb9, knight_dieb10] {};
void() knight_dieb10 =[ $deathb10, knight_dieb11] {monster_death_postcheck();};
void() knight_dieb11 =[ $deathb11, knight_dieb11] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() knight_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        if (random() > 0.5) knight_die1 ();        // Forward
        else knight_dieb1 ();                    // Backward
    }
};

//============================================================================
void() knight_sword_sound =
{
    if (random() > 0.5) sound (self, CHAN_WEAPON, "knight/sword2.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "knight/sword1.wav", 1, ATTN_NORM);
};

//============================================================================
void() knight_aistates =
{
    self.th_stand = knight_stand1;        // Idling on feet
    self.th_walk = knight_walk1;        // Walking
    self.th_run = knight_run1;            // Running
    self.th_melee = knight_melee;        // Sword attack
    self.th_pain = knight_pain;            // Stumble and pain
    self.th_die = knight_die;            // Face meets floor
};

//----------------------------------------------------------------------
void() knight_wakeup =
{
    // Check for sudden death!
    if (self.health < 0) return;
    
    self.use = SUB_Null;                // no re-trigger
    self.takedamage = DAMAGE_AIM;        // Can receive damage
    self.spawnstatue = FALSE;            // No longer stationary

    knight_aistates();                    // Restore AI state functions
    monster_targets();                    // Check for targets
};

//----------------------------------------------------------------------
void() knight_statue =
{
    local float targ_angle, new_frame;

    // Check for sudden death!
    if (self.health < 0) return;
    
    if (random() < MON_IDLE_SOUND && self.pos1_z > 1) {
        new_frame = TRUE;
        self.oldenemy = checkclient();
        if (self.oldenemy.flags & FL_CLIENT) {
            targ_angle  = viewangle(self.origin, self.oldenemy.origin, 0, TRUE, self.oldenemy.v_angle_y);
            if (targ_angle < 135 || targ_angle > 225) new_frame = FALSE;
        }
        self.lip = self.pos1_x + rint(random()*self.pos1_z);
        if (new_frame) {
            sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
            self.frame = self.lip;
        }
    }
    
     self.think = knight_statue;
     self.nextthink = time + 5 + random()*5;
};

/*======================================================================
 QUAKED monster_knight (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_knight =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_knight.mdl";
    self.headmdl = "progs/h_knight.mdl";        // Large head
    self.gib1mdl = "progs/w_knightsword.mdl";    // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";    // right foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    
    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = "";
    if (random() < 0.5) self.gib3mdl = "";

    //----------------------------------------------------------------------
    if (self.spawnflags & MON_STATUE) {
        self.idle_sound = "statue/idle1.wav";
        self.idle_sound2 = "statue/idle2.wav";
        self.idle_soundcom = "statue/comidle.wav";
        precache_sound (self.idle_sound);
        precache_sound (self.idle_sound2);
        precache_sound (self.idle_soundcom);
        
        self.sight_sound = "statue/sight.wav";
        self.pain_sound = "statue/hurt.wav";
        self.death_sound = "statue/death.wav";
        self.gib1skin = 1;                    // Stoney sword        
        precache_stonegibs();                // Cache gib models
        self.gibtype = GIBTYPE_STONE;        // Stoney gibs!
        self.gib2mdl = self.gib3mdl = string_null;
    }
    else {
        self.idle_sound = "knight/idle.wav";
        precache_sound (self.idle_sound);
        
        self.sight_sound = "knight/ksight.wav";
        self.pain_sound = "knight/khurt.wav";
        self.death_sound = "knight/kdeath.wav";
        
        // Check for poisonous entity flag
        if (self.poisonous) {
            precache_poisongibs();            // precache gibs
            self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
            if (!self.exactskin) self.exactskin = 3;    // Swampy skin
        }
    }

    // precache sight, pain and death (changes on type)
    precache_sound (self.sight_sound);
    precache_sound (self.pain_sound);
    precache_sound (self.death_sound);
    
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_SHORT;
    if (self.health < 1) self.health = 75;
    self.gibhealth = -40;                // Not easy to gib
    self.gibbed = FALSE;                // Still in one peice
    self.pain_flinch = 20;                // Light pain threshold
    self.pain_longanim = TRUE;            // Can do long stagger animation
    self.steptype = FS_TYPEMEDIUM;        // Light soldier feet
    self.skin = 0;                        // Always reset (use exactskin)
    self.th_die = knight_die;            // Face meets floor
    self.deathstring = " was slashed by a Knight\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    
    //----------------------------------------------------------------------
    // Detect statues and setup different parameters
    // Tougher skin, less pain, heavy feet etc    
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_STATUE) {
        self.classgroup = CG_STONE;        // Stone hates metal!
        self.poisonous = FALSE;            // Cannot be poisonous and stone!
        self.resist_nails = 0.5;        // Tough skin for nails
        self.reflectnails = TRUE;        // Reflect nail projectiles
        if (!self.exactskin) self.exactskin = 4;    // Stone skin
        self.pain_flinch = 40;            // Doubled
        self.pain_longanim = FALSE;        // resistant to shadow axe
        self.pain_timeout = 2;            // HK pain threshold
        self.steptype = FS_TYPEHEAVY;    // heavy stone feet!
        self.no_liquiddmg = TRUE;        // Immune to slime/lava
        self.deathstring = " was smashed by a Stone Knight\n";
        
        // Allow knights to start with a stone skin, but not be frozen!
        if (self.spawnflags & MON_NOTFROZEN) {
            self.spawnflags = self.spawnflags - MON_STATUE;
            knight_aistates();            // setup AI state functions
        }
        else {
            self.spawnstatue = TRUE;        // Start as a statue
            self.th_stand = knight_statue;    // Stone idle sound
            self.think1 = knight_wakeup;    // Wakeup function when triggered

            // statues can change position when the player is not looking
            // Specify a start/finish frame using pos1 or use frame
            if (!self.pos1_x) {
                self.pos1_x = $attackb1;    // Default pose (attacking)
                self.pos1_y = $attackb10;
            }
            // A frame has been specified, create a small range from that
            if (self.frame > 0) {
                self.pos1_x = self.pos1_y = self.frame;
            }
            // Work out range, make sure no negative values
            self.pos1_z = fabs(self.pos1_y - self.pos1_x);
            self.frame = self.pos1_x;        // Setup frame / pose        
        }
    }
    else {
        // The new knight model by Lunaran has 4 available skins to
        // choose from. The local option will always override the
        // worldspawn option.
        // Added -1 option which will randomly pick a skin
        if (self.exactskin < 0) self.randomskin = 4;
        if (!self.exactskin && world.knight_defskin > 0)
            self.exactskin = world.knight_defskin;

        self.pain_longanim = TRUE;        // can be chopped with shadow axe
        self.classgroup = CG_KNIGHT;    // Classic tincan!
        knight_aistates();                // setup AI state functions
    }

    self.classtype = CT_MONKNIGHT;
    self.classmove = MON_MOVEWALK;
    monster_start();    
};
