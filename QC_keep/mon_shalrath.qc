/*==============================================================================
SHAL-RATH (VORE)
==============================================================================*/
$cd id1/models/shalrath
$origin 0 0 24
$base base
$skin skin
$scale 0.7

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11

$frame pain1 pain2 pain3 pain4 pain5 

$frame death1 death2 death3 death4 death5 death6 death7

$frame    walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame    walk11 walk12

//============================================================================
// This is no stand idle! just uses frame 1 of walk
//============================================================================
void()    shal_stand    =[    $walk1,    shal_stand    ] {ai_stand();};

//============================================================================
void() shal_walk1    =[    $walk2,        shal_walk2    ] {monster_idle_sound(); ai_walk(6);};
void() shal_walk2    =[    $walk3,        shal_walk3    ] {ai_walk(4);};
void() shal_walk3    =[    $walk4,        shal_walk4    ] {monster_footstep(FALSE); ai_walk(0);};
void() shal_walk4    =[    $walk5,        shal_walk5    ] {ai_walk(0);};
void() shal_walk5    =[    $walk6,        shal_walk6    ] {ai_walk(0);};
void() shal_walk6    =[    $walk7,        shal_walk7    ] {monster_footstep(FALSE); ai_walk(0);};
void() shal_walk7    =[    $walk8,        shal_walk8    ] {ai_walk(5);};
void() shal_walk8    =[    $walk9,        shal_walk9    ] {ai_walk(6);};
void() shal_walk9    =[    $walk10,    shal_walk10    ] {ai_walk(5);};
void() shal_walk10    =[    $walk11,    shal_walk11    ] {monster_footstep(FALSE); ai_walk(0);};
void() shal_walk11    =[    $walk12,    shal_walk12    ] {ai_walk(4);};
void() shal_walk12    =[    $walk1,        shal_walk1    ] {ai_walk(5);};

//============================================================================
// run = walk (identical frames and movement speed!)
void() shal_run1    =[    $walk2,        shal_run2    ] {monster_idle_sound(); ai_run(6);};    
void() shal_run2    =[    $walk3,        shal_run3    ] {ai_run(4);};
void() shal_run3    =[    $walk4,        shal_run4    ] {monster_footstep(FALSE); ai_run(0);};
void() shal_run4    =[    $walk5,        shal_run5    ] {ai_run(0);};
void() shal_run5    =[    $walk6,        shal_run6    ] {ai_run(0);};
void() shal_run6    =[    $walk7,        shal_run7    ] {monster_footstep(FALSE); ai_run(0);};
void() shal_run7    =[    $walk8,        shal_run8    ] {ai_run(5);};
void() shal_run8    =[    $walk9,        shal_run9    ] {ai_run(6);};
void() shal_run9    =[    $walk10,    shal_run10    ] {monster_footstep(FALSE); ai_run(5);};
void() shal_run10    =[    $walk11,    shal_run11    ] {ai_run(0);};
void() shal_run11    =[    $walk12,    shal_run12    ] {ai_run(4);};
void() shal_run12    =[    $walk1,        shal_run1    ] {ai_run(5);};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() shalrath_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_SBALL;
        self.attachment.alpha = 0.85;
    }
};

//----------------------------------------------------------------------
void() shalrath_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() shalrath_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
};

//======================================================================
// RANGE ATTACK 1 - Homing Missile Attack
//======================================================================
void() ShalMissile =
{
    // Check if there is a clear line of sight
    // The original behaviour is ruined by this final check
    // Baiting shalraths to fire will generate more voreballs flying
    // around and projectiles to dodge, best leave this working
    //if ( !(visxray(self.enemy, '0 8 20', '0 0 10', FALSE)) ) return;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "shalrath/attack2.wav", 1, ATTN_NORM);
    self.attack_speed = SPEED_SHALMISSILE + (skill * SPEED_SHALSKILL);
    // Original vector location 0 0 10, new 0 8 36
    Launch_HomingMissile ('0 8 20', '0 0 10', CT_PROJ_SHAL, self.attack_speed);
};

//----------------------------------------------------------------------
void(vector hmofs, float hmframe) Setup_ShalHome =
{
    local vector org;
    if (self.health < 1) return;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (hmframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, MODEL_PROJ_SHOME);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
    }
    // Double check for pain interruption
    else if (self.attachment.state == STATE_OFF) return;

    // Turn towards enemy and update model attachment
    // The offset is based on the shalrath facing forward
    ai_face();
    makevectors(self.angles);
    org = self.origin + attack_vector(hmofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.frame = hmframe;
};

//----------------------------------------------------------------------
// it would have been cool to have the homing missile form slowing in the
// shalrath hand but the animation is just weird, the arm bends backward
// into a really strange position which makes the forming missile look
// terrible, restricted the grow function to later frames only
// 2 24 18, -8 24 18, -18 22 17, -28 18 16 (arm swings round and back)
// -26 12 20, -18 10 30, 12 8 36 ( arms swings up and forward)
//
void() shal_attack1        =[ $attack1,    shal_attack2 ] {ai_face();
    sound (self, CHAN_WEAPON, "shalrath/attack.wav", 1, ATTN_NORM);};
void() shal_attack2        =[ $attack2,    shal_attack3 ] {ai_face();};
void() shal_attack3        =[ $attack3,    shal_attack4 ] {ai_face();};
void() shal_attack4        =[ $attack4,    shal_attack5 ] {ai_face();};
void() shal_attack5        =[ $attack5,    shal_attack6 ] {ai_face();};
void() shal_attack6        =[ $attack6,    shal_attack7 ] {Setup_ShalHome('-26 12 20',0);};
void() shal_attack7        =[ $attack7,    shal_attack8 ] {Setup_ShalHome('-18 10 30',2);};
void() shal_attack8        =[ $attack8,    shal_attack9 ] {Setup_ShalHome('12 8 36',7);};
void() shal_attack9        =[ $attack9,    shal_attack10] {shalrath_finish_attachment();ShalMissile();};
void() shal_attack10    =[ $attack10,    shal_attack11] {};
void() shal_attack11    =[ $attack11,    shal_run1    ] {};

//======================================================================
// RANGE ATTACK 2 - spawn voreling minion (optional)
//
// Creates a ball of green goo and throws it towards the player
// Once the ball has landed, hatch/explode and create baby voreling
//======================================================================
void(vector eggofs, float eggframe) Setup_ShalrathEgg =
{
    local vector org;
    if (self.health < 1) return;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (eggframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, MODEL_PROJ_SBALL);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;

        self.pain_finished = time + 1;
        sound (self, CHAN_WEAPON, "shalrath/attack3.wav", 1, ATTN_NORM);
    }
    // Double check for pain interruption
    else if (self.attachment.state == STATE_OFF) return;

    // Turn towards enemy and update model attachment
    // The offset is based on the shalrath facing forward
    ai_face();
    makevectors(self.angles);
    org = self.origin + attack_vector(eggofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.frame = eggframe;
};

//----------------------------------------------------------------------
void() shal_minion1        =[ $attack1,    shal_minion2 ] {ai_face();};
void() shal_minion2        =[ $attack2,    shal_minion3 ] {Setup_ShalrathEgg('2 24 18',0);};
void() shal_minion3        =[ $attack3,    shal_minion4 ] {Setup_ShalrathEgg('-8 24 18',1);};
void() shal_minion4        =[ $attack4,    shal_minion5 ] {Setup_ShalrathEgg('-18 22 17',1);};
void() shal_minion5        =[ $attack5,    shal_minion6 ] {Setup_ShalrathEgg('-28 18 16',2);};
void() shal_minion6        =[ $attack6,    shal_minion7 ] {Setup_ShalrathEgg('-26 12 20',3);};
void() shal_minion7        =[ $attack7,    shal_minion8 ] {Setup_ShalrathEgg('-18 10 30',4);};
void() shal_minion8        =[ $attack8,    shal_minion9 ] {Setup_ShalrathEgg('12 8 36',5);};
void() shal_minion9        =[ $attack9,    shal_minion10] {shalrath_finish_attachment();Create_Egg('12 8 36');};
void() shal_minion10    =[ $attack10,    shal_minion11] {};
void() shal_minion11    =[ $attack11,    shal_run1    ] {};

//----------------------------------------------------------------------
void() shal_magic =
{
    local entity miniondef;
    
    // Make sure the attachments are setup ready
    shalrath_create_attachment();

    if (self.spawnflags & MON_SHALRATH_MINIONS && !self.minion_active) {
        // Check for minion template first
        miniondef = find(world,classname,"monster_shalrathminion");
        if (miniondef.classtype == CT_CACHEVORELING) setup_minionsupport();
        else {
            // If template no available, warn and remove feature
            dprint("\b[SHALRATH]\b Cannot find minion template!\n");
            self.spawnflags = self.spawnflags - MON_SHALRATH_MINIONS;
        }
    }
    
    // Is the target the player or monster?
    if (self.enemy.flags & FL_CLIENT) {
        if (self.spawnflags & MON_SHALRATH_MINIONS) {
            // This is an odd location for an melee check but the shalrath has
            // no melee function and any check (ShalCheckAttack) done is for
            // long range and moving backwards if attacking is blocked
            // If the shalrath is a minion spawner and the player is too close
            // then use the old base attack (voreballs) this can be switched off
            //
            self.enemydist = range_distance(self.enemy, TRUE);
            // If the player is too close (melee distance) try old base attack
            if (self.minion_baseattack > 0 && self.enemydist < MON_RANGE_MELEE) shal_attack1();
            else {
                // has the voreling limit been reached?
                if (query_minionactive(self) == TRUE) shal_minion1();
                else self.think = self.th_run;
            }
        }
        // Only has one type of attack, homing missiles
        else shal_attack1();
        
    }
    // Attack monsters with Homing Missiles
    else shal_attack1();
};

//============================================================================
void() shal_pain1 =[ $pain1, shal_pain2 ] {};
void() shal_pain2 =[ $pain2, shal_pain3 ] {shalrath_finish_attachment();};
void() shal_pain3 =[ $pain3, shal_pain4 ] {};
void() shal_pain4 =[ $pain4, shal_pain5 ] {};
void() shal_pain5 =[ $pain5, shal_run1  ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) shal_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Hide any attachments
    shalrath_finish_attachment();
    
    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) shal_pain1();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.5;
            self.axhitme = 0;
            shal_pain1();
        }
    }
};

//============================================================================
void() shal_death1 =[ $death1, shal_death2 ] {};
void() shal_death2 =[ $death2, shal_death3 ] {monster_check_gib();};
void() shal_death3 =[ $death3, shal_death4 ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() shal_death4 =[ $death4, shal_death5 ] {};
void() shal_death5 =[ $death5, shal_death6 ] {};
void() shal_death6 =[ $death6, shal_death7 ] {monster_death_postcheck();};
void() shal_death7 =[ $death7, shal_death7 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() shal_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // Make sure any attachments are removed
    shalrath_remove_attachment();

    // Normal death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "shalrath/death.wav", 1, ATTN_NORM);
        shal_death1();
    }
};

/*======================================================================
 QUAKED monster_shalrath (1 0 0) (-32 -32 -24) (32 32 48) Ambush
======================================================================*/
void() monster_shalrath =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_shalrath.mdl";    // Extra skin
    self.headmdl = "progs/ad171/h_shalrath.mdl";    // Matching new skin

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_SHAL);        // Original - progs/v_spike.mdl
    precache_model (MODEL_PROJ_SHOME);        // Homing missile growing 8 frames
    precache_model (MODEL_PROJ_SBALL);        // ball growing 8 frames
    precache_model (MODEL_PROJ_SEGG);        // ball turned into egg
    precache_model (MODEL_PROJ_SSHELL);        // Gibs from Egg

    self.idle_sound = "shalrath/idle.wav";
    precache_sound (self.idle_sound);

    precache_sound ("shalrath/death.wav");
    self.pain_sound = "shalrath/pain.wav";
    precache_sound (self.pain_sound);

    precache_sound ("shalrath/attack.wav");    // Start of attack
    precache_sound ("shalrath/attack2.wav");    // Firing missile
    precache_sound ("shalrath/attack3.wav");    // Minions
    precache_sound ("shalrath/bounce.wav");        // Egg bounce
    
    self.sight_sound = "shalrath/sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.idmins = VEC_HULL2_MIN;    // -32 -32 -24, 32 32 64
    self.idmaxs = VEC_HULL2_MAX;
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    if (self.health < 1) self.health = 400;
    self.gibhealth = -90;            // Rarely gib (explosives required)
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 70;            // take a lot of damage before pain
    self.pain_longanim = TRUE;        // can be chopped with shadow axe
    self.pain_timeout = 3;            // Really high pain tolerance
    self.steptype = FS_TYPELIGHT;    // Light tap feet
    self.blockudeath = TRUE;        // no humanoid death sound

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was exploded by a Vore\n";

    self.th_checkattack = ShalCheckAttack;
    self.th_stand = shal_stand;
    self.th_walk = shal_walk1;
    self.th_run = shal_run1;
    self.th_missile = shal_magic;
    self.th_pain = shal_pain;
    self.th_die = shal_die;

    // Special green skin for spider wraiths
    if (self.spawnflags & MON_SHALRATH_MINIONS) self.exactskin = 2;
        
    self.classtype = CT_MONSHAL;
    self.classgroup = CG_SPIDER;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};
