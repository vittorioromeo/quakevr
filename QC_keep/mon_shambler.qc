/*==============================================================================
SHAMBLER
==============================================================================*/
$cd id1/models/shams
$origin 0 0 24
$base base        
$skin base

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9
$frame stand10 stand11 stand12 stand13 stand14 stand15 stand16 stand17

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 
$frame walk8 walk9 walk10 walk11 walk12

$frame    run1 run2 run3 run4 run5 run6

$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 
$frame smash8 smash9 smash10 smash11 smash12

$frame swingr1 swingr2 swingr3 swingr4 swingr5 
$frame swingr6 swingr7 swingr8 swingr9

$frame swingl1 swingl2 swingl3 swingl4 swingl5 
$frame swingl6 swingl7 swingl8 swingl9

$frame magic1 magic2 magic3 magic4 magic5 
$frame magic6 magic7 magic8 magic9 magic10 magic11 magic12

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame death1 death2 death3 death4 death5 death6 
$frame death7 death8 death9 death10 death11

void() sham_swingr1;

//======================================================================
void() sham_stand1    =[    $stand1,    sham_stand2    ] {ai_stand();};
void() sham_stand2    =[    $stand2,    sham_stand3    ] {ai_stand();};
void() sham_stand3    =[    $stand3,    sham_stand4    ] {ai_stand();};
void() sham_stand4    =[    $stand4,    sham_stand5    ] {ai_stand();};
void() sham_stand5    =[    $stand5,    sham_stand6    ] {ai_stand();};
void() sham_stand6    =[    $stand6,    sham_stand7    ] {ai_stand();};
void() sham_stand7    =[    $stand7,    sham_stand8    ] {ai_stand();};
void() sham_stand8    =[    $stand8,    sham_stand9    ] {ai_stand();};
void() sham_stand9    =[    $stand9,    sham_stand10] {ai_stand();};
void() sham_stand10    =[    $stand10,    sham_stand11] {ai_stand();};
void() sham_stand11    =[    $stand11,    sham_stand12] {ai_stand();};
void() sham_stand12    =[    $stand12,    sham_stand13] {ai_stand();};
void() sham_stand13    =[    $stand13,    sham_stand14] {ai_stand();};
void() sham_stand14    =[    $stand14,    sham_stand15] {ai_stand();};
void() sham_stand15    =[    $stand15,    sham_stand16] {ai_stand();};
void() sham_stand16    =[    $stand16,    sham_stand17] {ai_stand();};
void() sham_stand17    =[    $stand17,    sham_stand1    ] {ai_stand();};

//======================================================================
void() sham_walk1    =[ $walk1, sham_walk2 ] {monster_idle_sound(); ai_walk(10);};
void() sham_walk2    =[ $walk2, sham_walk3 ] {monster_footstep(FALSE); ai_walk(9);};
void() sham_walk3    =[ $walk3, sham_walk4 ] {ai_walk(9);};
void() sham_walk4    =[ $walk4, sham_walk5 ] {ai_walk(5);};
void() sham_walk5    =[ $walk5, sham_walk6 ] {ai_walk(6);};
void() sham_walk6    =[ $walk6, sham_walk7 ] {ai_walk(12);};
void() sham_walk7    =[ $walk7, sham_walk8 ] {monster_footstep(FALSE); ai_walk(8);};
void() sham_walk8    =[ $walk8, sham_walk9 ] {ai_walk(3);};
void() sham_walk9    =[ $walk9, sham_walk10] {ai_walk(13);};
void() sham_walk10    =[ $walk10, sham_walk11] {ai_walk(9);};
void() sham_walk11    =[ $walk11, sham_walk12] {ai_walk(7);};
void() sham_walk12    =[ $walk12, sham_walk1 ] {ai_walk(7);};

//======================================================================
void() sham_run1 =[ $run1, sham_run2 ] {monster_idle_sound(); ai_run(20);};
void() sham_run2 =[ $run2, sham_run3 ] {monster_footstep(FALSE); ai_run(24);};
void() sham_run3 =[ $run3, sham_run4 ] {ai_run(20);};
void() sham_run4 =[ $run4, sham_run5 ] {ai_run(20);};
void() sham_run5 =[ $run5, sham_run6 ] {monster_footstep(FALSE); ai_run(24);};
void() sham_run6 =[ $run6, sham_run1 ] {ai_run(20);};

//======================================================================
// MELEE ATTACK 1 - Claw and Smash
// Merged shambler melee attacks (over head smash and side claw)
// Standardized to test vlen and zaxis height difference better
//======================================================================
float SHAM_MSMASH = 1;
float SHAM_MCLAW = 2;

//----------------------------------------------------------------------
void(float melee_attack, float side) sham_meleeattack =
{
    local float ldmg, dmg_multiplier;
    
    if (!self.enemy) return;                    // Something to fight?
    ai_damagebreakable(40);                        // Damage any breakables
    if (!ai_checkmelee(MONAI_MELEESHAM)) return;// Too far away
    dmg_multiplier = 1;

    // Get closer to enemy
    if (melee_attack == SHAM_MSMASH) ai_charge(0);        // Stationary attack
    else if (melee_attack == SHAM_MCLAW) ai_charge(10);    // Swing forward

    // Can the target bleed? - generate red/green blood
    if (!CanDamage (self.enemy, self)) return;
    
    // Check for poisonous attribute (new poison version)
    if (self.poisonous) PoisonDeBuff(self.enemy);

    // Shambler over head claw smash to ground (slow and high damage)
    // If this is infighting do more damage as it will look more impressive
    // Also if this damage is enough to kill in a single blow, gib for effect
    if (melee_attack == SHAM_MSMASH) {
        sound (self, CHAN_VOICE, self.meleehitsound, 1, ATTN_NORM);
        if (self.enemy.flags & FL_MONSTER) dmg_multiplier = dmg_multiplier * 2;
        ldmg = (random() + random() + random()) * (40*dmg_multiplier);
        if (self.enemy.health < ldmg) ldmg = ldmg*3;
        T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    
        // Lots of blood and gore
        SpawnMeatSpray (self, self.enemy, crandom() * 100);
        SpawnMeatSpray (self, self.enemy, crandom() * 100);
    }
    // Shambler side swipe (left/right and half damage)
    else if (melee_attack == SHAM_MCLAW) {
        sound (self, CHAN_VOICE, self.meleehitsound, 1, ATTN_NORM);
        ldmg = (random() + random() + random()) * 20;
        T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
        SpawnMeatSpray (self, self.enemy, side);
    }
};

//----------------------------------------------------------------------
void() sham_smash1     =[      $smash1,       sham_smash2    ] {
sound (self, CHAN_VOICE, "shambler/melee1.wav", 1, ATTN_NORM); ai_charge(2);};
void() sham_smash2     =[      $smash2,       sham_smash3    ] {monster_footstep(FALSE); ai_charge(6);};
void() sham_smash3     =[      $smash3,       sham_smash4    ] {ai_charge(6);};
void() sham_smash4     =[      $smash4,       sham_smash5    ] {ai_charge(5);};
void() sham_smash5     =[      $smash5,       sham_smash6    ] {ai_charge(4);};
void() sham_smash6     =[      $smash6,       sham_smash7    ] {ai_charge(1);};
void() sham_smash7     =[      $smash7,       sham_smash8    ] {ai_charge(0);};
void() sham_smash8     =[      $smash8,       sham_smash9    ] {ai_charge(0);};
void() sham_smash9     =[      $smash9,       sham_smash10   ] {ai_charge(0);};
void() sham_smash10    =[      $smash10,      sham_smash11   ] {sham_meleeattack(SHAM_MSMASH,0);};
void() sham_smash11    =[      $smash11,      sham_smash12   ] {ai_charge(5);};
void() sham_smash12    =[      $smash12,      sham_run1       ] {ai_charge(4);};

//----------------------------------------------------------------------
void() sham_swingl1    =[      $swingl1,      sham_swingl2   ] {
sound (self, CHAN_VOICE, "shambler/melee2.wav", 1, ATTN_NORM); ai_charge(5);};
void() sham_swingl2 =[      $swingl2,      sham_swingl3   ] {monster_footstep(FALSE); ai_charge(3);};
void() sham_swingl3 =[      $swingl3,      sham_swingl4   ] {ai_charge(7);};
void() sham_swingl4 =[      $swingl4,      sham_swingl5   ] {ai_charge(3);};
void() sham_swingl5 =[      $swingl5,      sham_swingl6   ] {ai_charge(7);};
void() sham_swingl6 =[      $swingl6,      sham_swingl7   ] {ai_charge(9);};
void() sham_swingl7 =[      $swingl7,      sham_swingl8   ] {ai_charge(5); sham_meleeattack(SHAM_MCLAW,250);};
void() sham_swingl8 =[      $swingl8,      sham_swingl9   ] {ai_charge(4);};
void() sham_swingl9 =[      $swingl9,      sham_run1  ] { ai_charge(8);
if (ai_checkmelee(MONAI_MELEESHAM) && self.enemy.health > 0) self.think = sham_swingr1;};

//----------------------------------------------------------------------
void() sham_swingr1    =[      $swingr1,      sham_swingr2   ] {
sound (self, CHAN_VOICE, "shambler/melee1.wav", 1, ATTN_NORM); ai_charge(1);};
void() sham_swingr2    =[      $swingr2,      sham_swingr3   ] {monster_footstep(FALSE); ai_charge(8);};
void() sham_swingr3 =[      $swingr3,      sham_swingr4   ] {ai_charge(14);};
void() sham_swingr4 =[      $swingr4,      sham_swingr5   ] {ai_charge(7);};
void() sham_swingr5 =[      $swingr5,      sham_swingr6   ] {ai_charge(3);};
void() sham_swingr6 =[      $swingr6,      sham_swingr7   ] {ai_charge(6);};
void() sham_swingr7 =[      $swingr7,      sham_swingr8   ] {ai_charge(6); sham_meleeattack(SHAM_MCLAW,-250);};
void() sham_swingr8 =[      $swingr8,      sham_swingr9   ] {ai_charge(3);};
void() sham_swingr9 =[      $swingr9,      sham_run1  ] { ai_charge(10);
if (ai_checkmelee(MONAI_MELEESHAM) && self.enemy.health > 0) self.think = sham_swingl1;};

//----------------------------------------------------------------------
void() sham_melee =
{
    self.meleeattack = random();
    // Try to start with a smash
    if (self.meleeattack > 0.6 || self.health == self.max_health) sham_smash1 ();
    else if (self.meleeattack > 0.3) sham_swingr1 ();
    else sham_swingl1 ();
};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() sham_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.alpha = 0.85;
    }
};

//----------------------------------------------------------------------
void() sham_finish_attachment =
{
    if (self.attachment) setmodel(self.attachment, "");
};

//----------------------------------------------------------------------
void() sham_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
};

//======================================================================
// RANGE ATTACK 1 - classic hitscan lightning
//======================================================================
void() sham_lightning =
{
    local vector org, dir;
    if (self.health < 1) return;
    
    ai_face ();
    self.effects = self.effects | EF_MUZZLEFLASH;
    sham_finish_attachment();

    makevectors(self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = self.enemy.origin + '0 0 16' - org;
    dir = normalize (dir);

    // Trace direct line of lightning infront of shambler
    traceline (org, self.origin + dir*MONAI_SHAMRANGE, TRUE, self);

    // Display lightning model segments
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING1);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);

    // Check for any breakables first
    if (ai_foundbreakable(self, trace_ent, FALSE)) {
        T_Damage (trace_ent, self, self, 80, DAMARMOR);
    }
    // Standard player lightning damage function
    else LightningDamage (org, trace_endpos, self, 10);
};

//----------------------------------------------------------------------
void(vector leftofs, vector rightofs, float litframe) sham_casting =
{
    local vector dir;
    local float litpoint;
    if (self.health < 1) return;

    // Check if attachment has been setup yet
    if (!self.attachment) sham_create_attachment();

    // Frame 0 is start of the sequence (move everything into place)
    if (litframe == 0) {
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, MODEL_PROJ_SHAMLIT);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
    }

    // Turn toward enemy
    ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;
    
    // Place orb in the center of the electricity
    setorigin(self.attachment, self.origin);
    self.attachment.angles = self.angles;
    self.attachment.frame = litframe;
    
    // Work out each hand position
    makevectors(self.angles);
    self.pos1 = self.origin + attack_vector(leftofs);
    self.pos2 = self.origin + attack_vector(rightofs);
    
    // Spawn some particles randomly along the lighting beam
    dir = normalize(self.pos2 - self.pos1);
    litpoint = vlen(self.pos2 - self.pos1) * random();
    self.pos3 = self.pos1 + (dir*litpoint);
    particle_explode(self.pos3, 5+random()*5, 0.2+random(), PARTICLE_BURST_BLUE, PARTICLE_BURST_SKULLUP);
};

//----------------------------------------------------------------------
void() sham_magic1 =[ $magic1, sham_magic2 ] {ai_face();
    sound (self, CHAN_WEAPON, "shambler/sattck1.wav", 1, ATTN_NORM);};
void() sham_magic2 =[ $magic2, sham_magic3 ] {
    sham_casting('0 43 61','16 -4 75',0);};
void() sham_magic3 =[ $magic3, sham_magic3b ] {ai_face();
    sham_casting('-16 42 69','7 0 80',1);};
void() sham_magic3b =[ $magic3, sham_magic4 ] {ai_face();
    sham_casting('-14 38 68','8 6 78',2);};
void() sham_magic4 =[ $magic4, sham_magic5 ] {monster_footstep(FALSE);
    sham_casting('-22 35 75','6 3 80',3);};
void() sham_magic5 =[ $magic5, sham_magic6 ] {
    sham_casting('-3 26 88','19 -9 81',4);};
void() sham_magic6 =[ $magic6, sham_magic9 ] {sham_lightning();
sound (self, CHAN_WEAPON, "shambler/sboom.wav", 1, ATTN_NORM);};
void() sham_magic9 =[ $magic9, sham_magic10 ] {sham_lightning();};
void() sham_magic10=[ $magic10,sham_magic11 ] {sham_lightning();monster_footstep(FALSE);};
void() sham_magic11=[ $magic11,sham_magic12 ] {
    if (skill == SKILL_NIGHTMARE) sham_lightning();};
void() sham_magic12 =[ $magic12, sham_run1] {};

//============================================================================
void() sham_pain1       =[      $pain1, sham_pain2      ] {};
void() sham_pain2       =[      $pain2, sham_pain3      ] {};
void() sham_pain3       =[      $pain3, sham_pain4      ] {};
void() sham_pain4       =[      $pain4, sham_pain5      ] {};
void() sham_pain5       =[      $pain5, sham_pain6      ] {};
void() sham_pain6       =[      $pain6, sham_run1      ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage)    sham_pain =
{
    // Finish with all attachments
    sham_finish_attachment();

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) sham_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.6;
            self.axhitme = 0;
            sham_pain1 ();
        }
    }
};

//============================================================================
void() sham_death1  =[ $death1,  sham_death2  ] {};
void() sham_death2  =[ $death2,  sham_death3  ] {monster_check_gib();};
void() sham_death3  =[ $death3,  sham_death4  ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() sham_death4  =[ $death4,  sham_death5  ] {};
void() sham_death5  =[ $death5,  sham_death6  ] {};
void() sham_death6  =[ $death6,  sham_death7  ] {};
void() sham_death7  =[ $death7,  sham_death8  ] {};
void() sham_death8  =[ $death8,  sham_death9  ] {};
void() sham_death9  =[ $death9,  sham_death10 ] {};
void() sham_death10 =[ $death10, sham_death11 ] {monster_death_postcheck();};
void() sham_death11 =[ $death11, sham_death11 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() sham_die =
{
    // Finish and remove all attachments
    sham_finish_attachment();
    sham_remove_attachment();

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

// regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "shambler/sdeath.wav", 1, ATTN_NORM);
        sham_death1 ();
    }
};

//----------------------------------------------------------------------
// Special wakeup animation for attacking/breaking something infront
// The monster should be setup facing the right direction before call
// Starts in large overhead swing downwards (no damage checks)
//----------------------------------------------------------------------
void() sham_wakeup1 =[ $smash7,        sham_wakeup2 ] {
    sound (self, CHAN_VOICE, "shambler/melee1.wav", 1, ATTN_NORM);};
void() sham_wakeup2 =[ $smash9,        sham_wakeup3 ] {};
void() sham_wakeup3 =[ $smash10,    sham_wakeup4 ] {};
void() sham_wakeup4 =[ $smash11,    sham_wakeup5 ] {};
void() sham_wakeup5 =[ $smash12,    sham_run1 ] {};

/*======================================================================
QUAKED monster_shambler (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_shambler =
{
    if (deathmatch) { remove(self); return; }

    if (self.poisonous) {
        self.mdl = "progs/ad171/mon_shambler.mdl";
        self.headmdl = "progs/ad171/h_shambler.mdl";        // Large head
        self.gib1mdl = "progs/ad171/gib_blclaw1.mdl";        // Left claw
        self.gib2mdl = "progs/ad171/gib_blclaw2.mdl";        // Right claw
        self.gib3mdl = "progs/ad171/gib_blfoot1.mdl";        // foot
        self.gib4mdl = "progs/ad171/gib_blfoot2.mdl";        // foot
        if (self.deathstring == "") self.deathstring = " was smashed by a Poisonous Shambler\n";
    }
    else {
        self.mdl = "progs/ad171/mon_shambler.mdl";
        self.headmdl = "progs/ad171/h_shambler.mdl";        // Large head
        self.gib1mdl = "progs/ad171/gib_shclaw1.mdl";        // Left claw
        self.gib2mdl = "progs/ad171/gib_shclaw2.mdl";        // Right claw
        self.gib3mdl = "progs/ad171/gib_shfoot1.mdl";        // foot
        self.gib4mdl = "progs/ad171/gib_shfoot2.mdl";        // foot
        if (self.deathstring == "") self.deathstring = " was smashed by a Shambler\n";
    }

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_SHAMLIT);
    precache_model ("progs/s_light.mdl");
    precache_model ("progs/bolt.mdl");
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);
    
    // Randomly swap claws and feet around
    self.lip = random();
    if (self.lip < 0.25) self.gib1mdl = self.gib4mdl;
    else if (self.lip < 0.5) self.gib2mdl = self.gib4mdl;
    else if (self.lip < 0.75) self.gib3mdl = self.gib4mdl;

    self.idle_sound = "shambler/sidle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("shambler/sdeath.wav");
    self.pain_sound = "shambler/shurt2.wav";
    precache_sound (self.pain_sound);

    precache_sound ("shambler/sattck1.wav");
    precache_sound ("shambler/sboom.wav");
    precache_sound ("shambler/melee1.wav");
    precache_sound ("shambler/melee2.wav");
    
    self.meleehitsound = "shambler/smack.wav";
    precache_sound (self.meleehitsound);

    self.sight_sound = "shambler/ssight.wav";
    precache_sound (self.sight_sound);
    
    // Precache extra green small gibs
    if (self.poisonous) {
        precache_poisongibs();
        self.gibtype = GIBTYPE_POISON;
        self.exactskin = 1;
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_MASSIVE;
    if (self.health < 1) self.health = 600;
    self.gibhealth = -60;
    self.gibbed = FALSE;                // In one peice
    self.pain_flinch = 400;                // Really low chance of pain
    self.pain_longanim = TRUE;            // can be chopped with shadow axe
    self.pain_timeout = 2;                // Long pause on pain
    self.steptype = FS_TYPELARGE;        // Large stomping feet
    self.blockudeath = TRUE;            // No player gib sound
    self.attack_offset = '0 0 40';        // Between hands

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = 0.5; self.resist_cells = 0;

    self.th_checkattack = ShamCheckAttack;
    self.th_stand = sham_stand1;
    self.th_walk = sham_walk1;
    self.th_run = sham_run1;
    self.th_die = sham_die;
    self.th_melee = sham_melee;
    self.th_missile = sham_magic1;
    self.th_pain = sham_pain;
    self.th_wakeup = sham_wakeup1;

    self.classtype = CT_MONSHAM;
    self.classgroup = CG_SHAM;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
