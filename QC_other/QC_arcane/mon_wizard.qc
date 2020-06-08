/*==============================================================================
WIZARD
==============================================================================*/

$cd id1/models/a_wizard
$origin 0 0 24
$base wizbase    
$skin wizbase

$frame hover1 hover2 hover3 hover4 hover5 hover6 hover7 hover8
$frame hover9 hover10 hover11 hover12 hover13 hover14 hover15

$frame fly1 fly2 fly3 fly4 fly5 fly6 fly7 fly8 fly9 fly10
$frame fly11 fly12 fly13 fly14

$frame magatt1 magatt2 magatt3 magatt4 magatt5 magatt6 magatt7
$frame magatt8 magatt9 magatt10 magatt11 magatt12 magatt13

$frame pain1 pain2 pain3 pain4

$frame death1 death2 death3 death4 death5 death6 death7 death8

//============================================================================
void()    wiz_stand1    =[    $hover1,        wiz_stand2    ] {monster_idle_sound(); ai_stand();};
void()    wiz_stand2    =[    $hover2,        wiz_stand3    ] {ai_stand();};
void()    wiz_stand3    =[    $hover3,        wiz_stand4    ] {ai_stand();};
void()    wiz_stand4    =[    $hover4,        wiz_stand5    ] {ai_stand();};
void()    wiz_stand5    =[    $hover5,        wiz_stand6    ] {ai_stand();};
void()    wiz_stand6    =[    $hover6,        wiz_stand7    ] {ai_stand();};
void()    wiz_stand7    =[    $hover7,        wiz_stand8    ] {ai_stand();};
void()    wiz_stand8    =[    $hover8,        wiz_stand1    ] {ai_stand();};

//============================================================================
void()    wiz_walk1    =[    $hover1,        wiz_walk2    ] {monster_idle_sound(); ai_walk(8);};
void()    wiz_walk2    =[    $hover2,        wiz_walk3    ] {ai_walk(8);};
void()    wiz_walk3    =[    $hover3,        wiz_walk4    ] {ai_walk(8);};
void()    wiz_walk4    =[    $hover4,        wiz_walk5    ] {ai_walk(8);};
void()    wiz_walk5    =[    $hover5,        wiz_walk6    ] {ai_walk(8);};
void()    wiz_walk6    =[    $hover6,        wiz_walk7    ] {ai_walk(8);};
void()    wiz_walk7    =[    $hover7,        wiz_walk8    ] {ai_walk(8);};
void()    wiz_walk8    =[    $hover8,        wiz_walk1    ] {ai_walk(8);};

//============================================================================
void()    wiz_side1    =[    $hover1,        wiz_side2    ] {monster_idle_sound(); ai_run(8);};
void()    wiz_side2    =[    $hover2,        wiz_side3    ] {ai_run(8);};
void()    wiz_side3    =[    $hover3,        wiz_side4    ] {ai_run(8);};
void()    wiz_side4    =[    $hover4,        wiz_side5    ] {ai_run(8);};
void()    wiz_side5    =[    $hover5,        wiz_side6    ] {ai_run(8);};
void()    wiz_side6    =[    $hover6,        wiz_side7    ] {ai_run(8);};
void()    wiz_side7    =[    $hover7,        wiz_side8    ] {ai_run(8);};
void()    wiz_side8    =[    $hover8,        wiz_side1    ] {ai_run(8);};

//============================================================================
void()    wiz_run1    =[    $fly1,        wiz_run2    ] {monster_idle_sound(); ai_run(16);};
void()    wiz_run2    =[    $fly2,        wiz_run3    ] {ai_run(16);};
void()    wiz_run3    =[    $fly3,        wiz_run4    ] {ai_run(16);};
void()    wiz_run4    =[    $fly4,        wiz_run5    ] {ai_run(16);};
void()    wiz_run5    =[    $fly5,        wiz_run6    ] {ai_run(16);};
void()    wiz_run6    =[    $fly6,        wiz_run7    ] {ai_run(16);};
void()    wiz_run7    =[    $fly7,        wiz_run8    ] {ai_run(16);};
void()    wiz_run8    =[    $fly8,        wiz_run9    ] {ai_run(16);};
void()    wiz_run9    =[    $fly9,        wiz_run10    ] {ai_run(16);};
void()    wiz_run10    =[    $fly10,        wiz_run11    ] {ai_run(16);};
void()    wiz_run11    =[    $fly11,        wiz_run12    ] {ai_run(16);};
void()    wiz_run12    =[    $fly12,        wiz_run13    ] {ai_run(16);};
void()    wiz_run13    =[    $fly13,        wiz_run14    ] {ai_run(16);};
void()    wiz_run14    =[    $fly14,        wiz_run1    ] {ai_run(16);};

//============================================================================
// Range attack
//============================================================================
void()    WizardAttackFinished =
{
    SUB_AttackFinished(2);
    if (enemy_range >= RANGE_MID || !enemy_vis) {
        self.attack_state = AS_STRAIGHT;
        self.think = wiz_run1;
    }
    else {
        self.attack_state = AS_SLIDING;
        self.think = wiz_side1;
    }
};

//----------------------------------------------------------------------
// Fire spit missile towards player position
// The missile is setup and defined beforehand
//----------------------------------------------------------------------
void() Wiz_FastFire =
{
    local vector vec, dst;
    local entity twizard, tmissile;

    // Is the wizard still alive?
    if (self.owner.health > 0) {
        // Setup correct entities
        tmissile = self;
        twizard = self.owner;
        // Switch to the actual wizard
        self = twizard;
        
        // Add glow effect to wizard while firing    
        self.effects = self.effects | EF_MUZZLEFLASH;

        // Check for any enemytarget setup (above spawnflag)
        makevectors (SUB_angEnemyTarget());
        // Work out origin and offset from missile, not wizard
        dst = SUB_orgEnemyTarget() - 13 * tmissile.movedir;
        vec = normalize(dst - tmissile.origin);
        
        self.attack_speed = SPEED_WIZSPIKE + (skill * SPEED_WIZSKILL);
        // make sure sound is played on the wizard, not the missile
        sound (self, CHAN_WEAPON, "wizard/wattack.wav", 1, ATTN_NORM);
        // Launch projectile expects self = firing monster
        launch_projectile (tmissile.origin, vec, CT_PROJ_WIZ, self.attack_speed);
        // Switch back to missile entity for removal
        self = tmissile;
    }

    remove (self);
};

//----------------------------------------------------------------------
// Create two spit missiles ready for firing later (delay 0.6, 1.0)
// This code is hoping the player will strafe into the second shot
//----------------------------------------------------------------------
void() Wiz_StartFast =
{
    local entity missile;
    
    sound (self, CHAN_WEAPON, "wizard/wattack.wav", 1, ATTN_NORM);
    self.v_angle = self.angles;
    makevectors (self.angles);

    missile = spawn ();
    missile.owner = self;
    missile.nextthink = time + 0.6;
    setsize (missile, '0 0 0', '0 0 0');        
    setorigin (missile, self.origin + self.attack_offset + v_forward*14 + v_right*14);
    missile.enemy = SUB_entEnemyTarget();
    missile.nextthink = time + 0.8;
    missile.think = Wiz_FastFire;
    missile.movedir = v_right;

    missile = spawn ();
    missile.owner = self;
    missile.nextthink = time + 1;
    setsize (missile, '0 0 0', '0 0 0');        
    setorigin (missile, self.origin + self.attack_offset + v_forward*14 + v_right* -14);
    missile.enemy = SUB_entEnemyTarget();
    missile.nextthink = time + 0.3;
    missile.think = Wiz_FastFire;
    missile.movedir = VEC_ORIGIN - v_right;
};

//----------------------------------------------------------------------
void()    wiz_fast1  =[ $magatt1, wiz_fast2 ] {ai_face();Wiz_StartFast();};
void()    wiz_fast2  =[ $magatt2, wiz_fast3 ] {ai_face();};
void()    wiz_fast3  =[ $magatt3, wiz_fast4 ] {ai_face();};
void()    wiz_fast4  =[ $magatt4, wiz_fast5 ] {ai_face();};
void()    wiz_fast5  =[ $magatt5, wiz_fast6 ] {ai_face();};
void()    wiz_fast6  =[ $magatt6, wiz_fast7 ] {ai_face();};
void()    wiz_fast7  =[ $magatt5, wiz_fast8 ] {ai_face();};
void()    wiz_fast8  =[ $magatt4, wiz_fast9 ] {ai_face();};
void()    wiz_fast9  =[ $magatt3, wiz_fast10] {ai_face();};
void()    wiz_fast10 =[ $magatt2, wiz_run1  ] {WizardAttackFinished (); ai_face();};

//============================================================================
void()    wiz_wakeup =[ $hover1, wiz_run1 ] {
    // Is the wizard stuck? cannot move?
    if (pointcontents(self.origin) == CONTENT_SOLID) {
        // Time to die!
        self.health = self.gibhealth;
        Killed(self, self);
    }
    else {
        monster_sightsound();
        // Setup goals and warn other monsters
        FoundHuntTarget(TRUE);
        // Restore all think state functions
        self.th_stand = wiz_stand1;
        self.th_walk = wiz_walk1;
        self.th_run = wiz_run1;
        self.th_missile = wiz_fast1;
        self.th_slide = wiz_side1;
    }
};

//============================================================================
void() wiz_pain1 =[ $pain1, wiz_pain2] {};
void() wiz_pain2 =[ $pain2, wiz_pain3] {};
void() wiz_pain3 =[ $pain3, wiz_pain4] {};
void() wiz_pain4 =[ $pain4, wiz_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) wiz_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) wiz_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.4;
            self.axhitme = 0;
            wiz_pain1 ();
        }
    }
};

//============================================================================
void() wiz_death1 =[ $death1, wiz_death2 ] {};
void() wiz_death2 =[ $death2, wiz_death3 ] {monster_check_gib();};
void() wiz_death3 =[ $death3, wiz_death4 ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() wiz_death4 =[ $death4, wiz_death5 ] {};
void() wiz_death5 =[ $death5, wiz_death6 ] {};
void() wiz_death6 =[ $death6, wiz_death7 ] {};
void() wiz_death7 =[ $death7, wiz_death8 ] {monster_death_postcheck();};
void() wiz_death8 =[ $death8, wiz_death8 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() wiz_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) { 
        sound (self, CHAN_VOICE, "wizard/wdeath.wav", 1, ATTN_NORM);
        self.velocity_x = -200 + 400*random();
        self.velocity_y = -200 + 400*random();
        self.velocity_z = 100 + 100*random();
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        wiz_death1 ();
    }
};

/*======================================================================
QUAKED monster_wizard (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() setup_wizard;
void() monster_wizard =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_wizard.mdl";
    self.headmdl = "progs/h_wizard.mdl";
    self.gib1mdl = "progs/gib_wzarm1.mdl";    // Left arm/stump
    self.gib2mdl = "progs/gib_wzarm2.mdl";    // Right arm/stump
    self.gib3mdl = "progs/gib_wztail.mdl";    // Tail section
    
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_WIZ);        // Originally progs/w_spike.mdl
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.idle_sound = "wizard/widle1.wav";
    self.idle_sound2 = "wizard/widle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);

    precache_sound ("wizard/wdeath.wav");
    self.pain_sound = "wizard/wpain.wav";
    precache_sound (self.pain_sound);

    precache_sound ("wizard/hit.wav");        // used by c code
    precache_sound ("wizard/wattack.wav");

    self.sight_sound = "wizard/wsight.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        self.exactskin = 1;                // Greener version
        self.gib1skin = self.gib2skin = self.gib3skin = 1;
    }

    // Cache gargoyle is a special class used for precache only
    if (self.classtype != CT_CACHEWIZARD) setup_wizard();
};

//----------------------------------------------------------------------------
void() monster_nourminion = { 
    self.classtype = CT_CACHEWIZARD; 
    self.poisonous = TRUE;    // Cache poison stuff (just in case)
    monster_wizard();
};

//----------------------------------------------------------------------------
void() setup_wizard =
{
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 80;
    self.gibhealth = -30;
    self.pain_flinch = 70;                // high pain threshold
    self.pain_longanim = TRUE;            // Has long pain animation for Shadow Axe
    self.blockudeath = TRUE;            // no humanoid death sound
    if (self.height < 1) self.height = MONAI_ABOVEDIST; // Custom height
    self.attack_offset = '0 0 30';        // Acid spit start location
    self.deathstring = " was wrecked by a Wizard\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Allow wizards to keep a certain distance above the enemy
    if (self.spawnflags & MON_WIZARD_ABOVE) {
        self.th_checkattack = GargoyleCheckAttack;
    }
    else {
        // Default behaviour, really high distance checks
        self.th_checkattack = WizardCheckAttack;
        self.enemymaxdist = TRUE;
    }
    self.th_pain = wiz_pain;
    self.th_die = wiz_die;
        
    if(!self.classtype) self.classtype = CT_MONWIZARD;
    if (!self.classgroup) self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEFLY;
    
    if (self.classtype == CT_MINIONWIZARD) {
        self.th_stand = self.th_walk = self.th_run = wiz_wakeup;
        self.th_missile = self.th_slide = wiz_wakeup;
    }
    else {
        self.th_stand = wiz_stand1;
        self.th_walk = wiz_walk1;
        self.th_run = wiz_run1;
        self.th_missile = wiz_fast1;
        self.th_slide = wiz_side1;
    }

    monster_start();
};

//----------------------------------------------------------------------------
// A code way to spawn wizards (requires monster_nourminion entity)
//----------------------------------------------------------------------------
void(vector minion_org, entity minion_targ, float minion_sflag) minion_wizard =
{
    local entity minion;

    // Check if there is space to spawn entity
    if (entity_pcontent(minion_org)) return;
    
    // Self = minotaur, there is no egg 
    update_minioncount(self, 1);            // Update spawn counters
    
    minion = spawn();
    minion.classname = "monster_wizard";    // For function searching
    setorigin(minion, minion_org);            // Move to new location
    minion.owner = self;                    // Spawner Parent Link
    
    minion.effects = minion.flags = 0;        // make sure are blank
    minion.gibondeath = 1;                    // Always gib on death
    minion.classtype = CT_MINIONWIZARD;        // Special minion class
    minion.enemy = minion_targ;                // Target to attack
    minion.spawnflags = minion_sflag;        // Spawnflags
    minion.classgroup = CG_WIZARD;            // Don't turn on master
    minion.minion_active = TRUE;            // Minion flag
    minion.bodyfadeaway = TRUE;                // Get rid of body
    
    // Check for poison gibs being cached first
    if (gibpoison && self.poisonous == TRUE) {
        minion.poisonous = self.poisonous;    // carry over from host spawner 
        minion.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
    }

    minion.mdl = "progs/mon_wizard.mdl";
    minion.headmdl = "progs/h_wizard.mdl";
    minion.gib1mdl = "progs/gib_wzarm1.mdl";    // Left arm/stump
    minion.gib2mdl = "progs/gib_wzarm2.mdl";    // Right arm/stump
    minion.gib3mdl = "progs/gib_wztail.mdl";    // Tail section

    minion.pain_sound = "wizard/wpain.wav";
    minion.idle_sound = "wizard/widle1.wav";
    minion.idle_sound2 = "wizard/widle2.wav";
    minion.sight_sound = "wizard/wsight.wav";
    
    minion.nextthink = time + 0.01;
    minion.think = setup_wizard;
};