/*==============================================================================
GARGOYLE / IMP (Hexen2 Model)
==============================================================================*/
// Fall on back to ground
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14

// Single attack routine, fling fireball
$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15 attack16
$frame attack17 attack18 attack19 attack20 attack21

// Classic idle
$frame fly1 fly2 fly3 fly4 fly5 fly6 fly7 fly8
$frame fly9 fly10 fly11 fly12 fly13 fly14 fly15 fly16
$frame fly17 fly18 fly19 fly20

// Special minion grow animation
// Removed excessive frames (0.05 timing)
$frame grow1 grow2 grow3 grow4 grow5 grow6 grow7 grow8 grow9 grow10

// Move from perch to flying (can be reversed)
$frame flyup1 flyup2 flyup3 flyup4 flyup5 flyup6 flyup7 flyup8
$frame flyup9 flyup10 flyup11 flyup12 flyup13 flyup14 flyup15 flyup16
$frame flyup17 flyup18 flyup19 flyup20 flyup21 flyup22 flyup23

// Idle animation for perching (sitting down)
$frame perch1 perch2 perch3 perch4 perch5 perch6 perch7 perch8
$frame perch9 perch10 perch11 perch12 perch13 perch14 perch15 perch16
$frame perch17 perch18 perch19 perch20 perch21 perch22 perch23 perch24

// Going into a nosedive
$frame swoop1 swoop2 swoop3 swoop4 swoop5 swoop6 swoop7 swoop8
$frame swoop9 swoop10 swoop11 swoop12 swoop13 swoop14 swoop15 swoop16
$frame swoop17 swoop18 swoop19 swoop20

// Swooshing through the air!
$frame swooploop1 swooploop2 swooploop3 swooploop4

// Nose up, back to fly idling
$frame swoopend1 swoopend2 swoopend3 swoopend4 swoopend5 swoopend6 swoopend7 swoopend8
$frame swoopend9 swoopend10 swoopend11 swoopend12 swoopend13 swoopend14 swoopend15

// Alternative to stop swooping
$frame swoopout1 swoopout2 swoopout3 swoopout4 swoopout5 swoopout6 swoopout7 swoopout8
$frame swoopout9 swoopout10 swoopout11 swoopout12 swoopout13 swoopout14 swoopout15

void() gargoylewings =
{ sound(self, CHAN_ITEM, "gargoyle/wings1.wav", 1, ATTN_IDLE); };
void() garg_fbfinish;

//======================================================================
void()    garg_idle1    =[    $fly2,    garg_idle2    ] {monster_idle_sound();ai_stand();};
void()    garg_idle2    =[    $fly4,    garg_idle3    ] {ai_stand();};
void()    garg_idle3    =[    $fly6,    garg_idle4    ] {gargoylewings();ai_stand();};
void()    garg_idle4    =[    $fly8,    garg_idle5    ] {ai_stand();};
void()    garg_idle5    =[    $fly10,    garg_idle6    ] {ai_stand();};
void()    garg_idle6    =[    $fly12,    garg_idle7    ] {ai_stand();};
void()    garg_idle7    =[    $fly14,    garg_idle8    ] {ai_stand();};
void()    garg_idle8    =[    $fly16,    garg_idle9    ] {ai_stand();};
void()    garg_idle9    =[    $fly18,    garg_idle10    ] {ai_stand();};
void()    garg_idle10    =[    $fly20,    garg_idle1    ] {ai_stand();};

//======================================================================
void()    garg_walk1    =[    $fly1,    garg_walk2    ] {monster_idle_sound();ai_walk(8);};
void()    garg_walk2    =[    $fly3,    garg_walk3    ] {ai_walk(8);};
void()    garg_walk3    =[    $fly5,    garg_walk4    ] {gargoylewings();ai_walk(8);};
void()    garg_walk4    =[    $fly7,    garg_walk5    ] {ai_walk(8);};
void()    garg_walk5    =[    $fly9,    garg_walk6    ] {ai_walk(8);};
void()    garg_walk6    =[    $fly11,    garg_walk7    ] {ai_walk(8);};
void()    garg_walk7    =[    $fly13,    garg_walk8    ] {ai_walk(8);};
void()    garg_walk8    =[    $fly15,    garg_walk9    ] {ai_walk(8);};
void()    garg_walk9    =[    $fly17,    garg_walk10    ] {ai_walk(8);};
void()    garg_walk10    =[    $fly19,    garg_walk1    ] {ai_walk(8);};

//======================================================================
void()    garg_run1    =[    $fly1,    garg_run2    ] {monster_idle_sound();
    if (self.attachment && self.attachment.state == STATE_ON) garg_fbfinish();
    // Switch direction of strafing (random duration)
    if (self.meleeattack < time) {
        self.meleeattack = time + 4 + (random() + random() + random() * 4);
        self.lefty = rint(1 - self.lefty);
    }
    ai_run(4);
};
void()    garg_run2    =[    $fly3,    garg_run3    ] {ai_run(5);};
void()    garg_run3    =[    $fly5,    garg_run4    ] {gargoylewings(); ai_run(6);};
void()    garg_run4    =[    $fly7,    garg_run5    ] {ai_run(7);};
void()    garg_run5    =[    $fly9,    garg_run6    ] {ai_run(8);};
void()    garg_run6    =[    $fly11,    garg_run7    ] {ai_run(4);};
void()    garg_run7    =[    $fly13,    garg_run8    ] {ai_run(5);};
void()    garg_run8    =[    $fly15,    garg_run9    ] {ai_run(6);};
void()    garg_run9    =[    $fly17,    garg_run10    ] {ai_run(7);};
void()    garg_run10    =[    $fly19,    garg_run1    ] {ai_run(8);};

//============================================================================
// Range attack
//============================================================================
void() garg_fbthrow =
{
    local vector org, dir;
    
    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(SUB_orgEnemyTarget() - org);
    self.attack_speed = SPEED_GARGMISSILE + (skill * SPEED_GARGSKILL);
    
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_GARG, self.attack_speed);
};

//----------------------------------------------------------------------
void() garg_fbfinish =
{
    setmodel(self.attachment, "");
    self.attachment.state = STATE_OFF;
};

//----------------------------------------------------------------------
void(float fbframe, vector fbofs) garg_fbupdate =
{
    if (self.health < 1) return;
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        if (self.poisonous) self.attachment.mdl = MODEL_PROJ_GARGBALLGRN;
        else self.attachment.mdl = MODEL_PROJ_GARGBALL;
        self.attachment.alpha = 0.85;
    }
    
    if (fbframe == 0) {
        self.attachment.state = STATE_ON;
        self.pain_finished = time + 1.5;        //don't go into pain whilst firing
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        sound (self, CHAN_WEAPON, "gargoyle/attack1.wav", 1, ATTN_NORM);
    }

    ai_face();                    // Turn towards enemy
    makevectors(self.angles);
    self.attachment.origin = self.origin + v_forward*fbofs_x + v_right*fbofs_y;
    self.attachment.origin_z = self.attachment.origin_z + fbofs_z;
    setorigin(self.attachment, self.attachment.origin);
    self.attachment.angles = self.angles;
    self.attachment.frame = fbframe;
};

//----------------------------------------------------------------------
// Original offset position for gargoyle fireball (designed for a sprite)
// 10 16 0, 4 24 2, -7 28 15, -25 26 28, -38 12 45, -38 9 45, -17 36 30, 24 16 10
// Changed the offset to match the new model instead
// Positioned the fireball to match the hand orientation
//----------------------------------------------------------------------
void() garg_fire1    =[ $attack2, garg_fire2 ] {garg_fbupdate(0,'10 16 -2');};
void() garg_fire2    =[ $attack4, garg_fire3 ] {garg_fbupdate(2,'0 20 -2');};
void() garg_fire3    =[ $attack6, garg_fire4 ] {garg_fbupdate(3,'-6 23 6');gargoylewings();};
void() garg_fire4    =[ $attack8, garg_fire5 ] {garg_fbupdate(4,'-16 26 18');};
void() garg_fire5    =[ $attack10, garg_fire6 ] {garg_fbupdate(5,'-29 18 38');};
void() garg_fire6    =[ $attack12, garg_fire7 ] {garg_fbupdate(4,'-34 9 45');};
void() garg_fire7    =[ $attack14, garg_fire8 ] {garg_fbupdate(3,'-32 22 38');};
void() garg_fire8    =[ $attack16, garg_fire9 ] {garg_fbfinish();garg_fbthrow();};
void() garg_fire9    =[ $attack18, garg_fire10 ] {ai_face();monster_idle_sound();};
void() garg_fire10    =[ $attack20, garg_run1 ] {ai_face();
    // If enemy really close, goto into a frenzy of fireball attacks
    self.enemydist = range_distance(SUB_entEnemyTarget(), FALSE);
    if (SUB_healthEnemyTarget() > 0 && self.enemydist < MON_RANGE_CLOSE) self.think = garg_fire1;
    else self.attack_state = AS_STRAIGHT;
};

//============================================================================
// PERCH + WAKEUP
//============================================================================
void()    garg_perch1    =[    $perch2,    garg_perch2    ] {monster_idle_sound();ai_stand();};
void()    garg_perch2    =[    $perch4,    garg_perch3    ] {ai_stand();};
void()    garg_perch3    =[    $perch6,    garg_perch4    ] {ai_stand();};
void()    garg_perch4    =[    $perch8,    garg_perch5    ] {ai_stand();};
void()    garg_perch5    =[    $perch10,    garg_perch6    ] {ai_stand();};
void()    garg_perch6    =[    $perch12,    garg_perch7    ] {ai_stand();};
void()    garg_perch7    =[    $perch14,    garg_perch8    ] {ai_stand();};
void()    garg_perch8    =[    $perch16,    garg_perch9    ] {ai_stand();};
void()    garg_perch9    =[    $perch18,    garg_perch10    ] {ai_stand();};
void()    garg_perch10=[    $perch20,    garg_perch11    ] {ai_stand();};
void()    garg_perch11=[    $perch22,    garg_perch12    ] {ai_stand();};
void()    garg_perch12=[    $perch24,    garg_perch1    ] {ai_stand();};

//----------------------------------------------------------------------
// Sightsound and very quick animation while moving forward
void()    garg_wakeup1    =[    $flyup2,    garg_wakeup2    ] {ai_forward(1);};
void()    garg_wakeup2    =[    $flyup4,    garg_wakeup3    ] {ai_forward(4);};
void()    garg_wakeup3    =[    $flyup6,    garg_wakeup4    ] {ai_forward(8);};
void()    garg_wakeup4    =[    $flyup14,    garg_wakeup5    ] {ai_forward(8);gargoylewings();};
void()    garg_wakeup5    =[    $flyup16,    garg_wakeup6    ] {ai_forward(4);};
void()    garg_wakeup6    =[    $flyup20,    garg_run1    ] {
    monster_targets();            // Find target (angry/path_corner)
    self.nextthink = time + 0.1;// Next frame
};

//----------------------------------------------------------------------
void() gargoyle_wakeup =
{
    // Dead already?
    if (self.health < 1) return;

    // No longer perched on the ground
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_GARGOYLE_PERCH);
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_STATUE);
    self.spawnstatue = FALSE;    // No longer stationary
    
    self.takedamage = DAMAGE_AIM;        // Can receive damage
    self.use = SUB_Null;                // no re-trigger
    
    // Restore all think state functions
    self.th_stand = garg_idle1;
    self.th_walk = garg_walk1;
    self.th_run = garg_run1;
    self.th_missile = garg_fire1;

    // Allow gargoyle to fly up, no more ground checks
    self.flags = self.flags | FL_FLY;
    self.movetype = MOVETYPE_STEP;
    self.pain_finished = time + 1.5;        // No pain
    self.attack_finished = 1.5 + random();    // No skill adjustment
    garg_wakeup1();
};

//----------------------------------------------------------------------
void() gargoyle_statue =
{
    if (random() < MON_IDLE_SOUND) sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
    
     self.think = gargoyle_statue;
     self.nextthink = time + 5 + random()*5;
};

//======================================================================
// MINION - Grow and spin up from nothing
//======================================================================
void() gargoyle_growangle = {self.angles_y = self.angles_y + self.lefty;};
void() gargoyle_grow1 = [ $grow1, gargoyle_grow2 ] {};
void() gargoyle_grow2 = [ $grow2, gargoyle_grow3 ] {gargoyle_growangle();};
void() gargoyle_grow3 = [ $grow3, gargoyle_grow4 ] {gargoyle_growangle();};
void() gargoyle_grow4 = [ $grow4, gargoyle_grow5 ] {gargoyle_growangle();};
void() gargoyle_grow5 = [ $grow5, gargoyle_grow6 ] {gargoyle_growangle();};
void() gargoyle_grow6 = [ $grow6, gargoyle_grow7 ] {gargoyle_growangle();};
void() gargoyle_grow7 = [ $grow7, gargoyle_grow8 ] {gargoyle_growangle();};
void() gargoyle_grow8 = [ $grow8, gargoyle_grow9 ] {gargoyle_growangle();};
void() gargoyle_grow9 = [ $grow9, gargoyle_grow10] {gargoyle_growangle();};
void() gargoyle_grow10= [ $grow10, garg_run1 ] {
    // Is the gargoyle stuck? cannot move?
    if (pointcontents(self.origin) == CONTENT_SOLID) {
        // Time to die!
        self.health = self.gibhealth;
        Killed(self, self);
    }
    else {
        // Finally spin back to original position
        self.angles_y = self.angles_y + self.lefty;
        // Setup goals and warn other monsters
        FoundHuntTarget(TRUE);

        // Restore all think state functions
        self.th_stand = garg_idle1;
        self.th_walk = garg_walk1;
        self.th_run = garg_run1;
        self.th_missile = garg_fire1;
    }
};
//----------------------------------------------------------------------------
void() gargoyle_grow =
{
    // Only call wakeup function once
    self.th_stand = self.th_walk = self.th_run  = SUB_Null;
    if (random() < 0.5) self.lefty = 36;
    else self.lefty = -36;
    monster_sightsound();
    gargoyle_grow1();
};

//============================================================================
// The gargoyle does not have a specific pain animation
// - Kinn created one from the death animation (Marcher progs)
void()    garg_pain1    =[    $death3,    garg_pain2    ] {};
void()    garg_pain2    =[    $death4,    garg_pain3    ] {};
void()    garg_pain3    =[    $death3,    garg_pain4    ] {};
void()    garg_pain4    =[    $death2,    garg_pain5    ] {};
void()    garg_pain5    =[    $death1,    garg_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) garg_pain =
{
    // Check for perching gargoyles, need special wakeup
    if (self.spawnflags & MON_GARGOYLE_PERCH) gargoyle_wakeup();
    // Statues don't feel pain!
    else if (self.spawnflags & MON_STATUE) return;
    else {
        // Check all pain conditions and set up what to do next
        monster_pain_check(attacker, damage);

        // make sure any fireball is hidden
        if (self.attachment) garg_fbfinish();

        // Any pain animation/sound required?
        if (self.pain_check > 0) {
            if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
            else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
            if (self.pain_check == 1 || self.pain_check == 2) {
                garg_pain1 ();
            }
        }
    }
};

//============================================================================
void()    garg_death1    =[    $death1,    garg_death2    ] {};
void()    garg_death2    =[    $death3,    garg_death3    ] {monster_check_gib();};
void()    garg_death3    =[    $death5,    garg_death4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    garg_death4    =[    $death7,    garg_death5    ] {};
void()    garg_death5    =[    $death9,    garg_death6    ] {};
void()    garg_death6    =[    $death11,    garg_death7    ] {};
void()    garg_death7    =[    $death13,    garg_death8    ] {monster_death_postcheck();};
void()    garg_death8    =[    $death14,    garg_death8    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() garg_die =
{
    // Statues don't die!
    if (self.spawnflags & MON_STATUE) return;
    
    // Make sure any wings sound is silenced
    sound (self, CHAN_ITEM, SOUND_EMPTY, 1, ATTN_NORM);

    // Check for perching gargoyles, need to blow up in gibs
    // In an awkward pose that does not go well into death animations
    if (self.spawnflags & MON_GARGOYLE_PERCH) self.gibondeath = TRUE;

    // If creating a Fireball, make it explode!
    if (self.attachment) {
        if (self.attachment.state == STATE_ON) {
            self.gibondeath = TRUE;
            SpawnProjectileSmoke(self.origin, 200, 50, 150);
            SpawnProjectileSmoke(self.origin, 200, 50, 150);
            if (self.poisonous) self.lip = PARTICLE_BURST_GREEN;
            else self.lip = PARTICLE_BURST_FIRE;
            particle_explode(self.origin-'0 0 16', 50+random()*50, 2, self.lip, PARTICLE_BURST_UPWARD);
            makevectors(self.angles);
            self.oldorigin = self.origin + v_forward*8;
            if (self.poisonous) self.lip = EXPLODE_POISON_BIG;
            else self.lip = EXPLODE_BIG;
            SpawnExplosion(self.lip, self.oldorigin, SOUND_REXP3);
        }
        // make sure any fireball is hidden
        garg_fbfinish();
    }

    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    
    // If not gibbed, then drop to ground
    if (!self.gibbed) { 
        sound (self, CHAN_VOICE, "gargoyle/death1.wav", 1, ATTN_NORM);
        self.flags = FL_MONSTER;                // reset all flag conditions
        self.movetype = MOVETYPE_TOSS;            // Affected by gravity
        self.velocity_x = -200 + 400*random();
        self.velocity_y = -200 + 400*random();
        self.velocity_z = 100 + 100*random();
        garg_death1 ();
    }
};

/*======================================================================
QUAKED monster_gargoyle (0.8 0 0.2) (-16 -16 -24) (16 16 40) AMBUSH STATUE PERCH NOSIGHT NOIDLE NOGFX STARTOFF ANGRY Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/mon_gargoyle.mdl"); }
Gargoyle, 120 health points.
-------- KEYS --------
-------- SPAWNFLAGS --------
STATUE : Stone statue until triggered (always starts in a perched position)
PERCH : Start in a sitting down, place 44 units out from perching edge, 4 units higher

======================================================================*/
void() setup_gargoyle;
void() monster_gargoyle =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_gargoyle.mdl";
    self.headmdl = "progs/h_gargoyle.mdl";
    self.gib1mdl = "progs/gib_gargwing1.mdl";    // Left wing
    self.gib2mdl = "progs/gib_gargwing2.mdl";    // Right wing

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_GARGBALL);        // FB growing 8 frames
    precache_model (MODEL_PROJ_GARGOYLE);        // red/smoke wiz projectile
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);

    self.idle_sound = "gargoyle/idle1.wav";
    self.idle_soundcom = "gargoyle/perch1.wav";
    self.idle_soundcom2 = "gargoyle/perch2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_soundcom);
    precache_sound (self.idle_soundcom2);

    precache_sound ("gargoyle/attack1.wav");    // Doom imp fireball!
    precache_sound ("gargoyle/swoop1.wav");
    precache_sound ("gargoyle/swoop2.wav");
    precache_sound ("gargoyle/swoop1hit.wav");
    precache_sound ("gargoyle/wings1.wav");
    
    self.pain_sound = "gargoyle/pain75.wav";
    self.pain_sound2 = "gargoyle/pain100.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound ("gargoyle/death1.wav");

    self.sight_sound = "gargoyle/sight1.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_GARGBALLGRN);    // Ball growing 8 frames
        precache_model (MODEL_PROJ_GARGOYLEGRN);    // acid/smoke wiz projectile
    }
    
    // Cache gargoyle is a special class used for precache only
    if (self.classtype != CT_CACHEGARGOYLE) setup_gargoyle();
};
    
//----------------------------------------------------------------------------
void() monster_minotaurminion = { 
    self.classtype = CT_CACHEGARGOYLE; 
    self.poisonous = TRUE;    // Cache poison stuff for minotaur spawns
    monster_gargoyle();
};

//----------------------------------------------------------------------------
void() setup_gargoyle =
{
    // Randomize gib parts
    if (random() < 0.5) self.gib1mdl = string_null;
    else if (random() < 0.5) self.gib2mdl = string_null;

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 120;
    self.gibhealth = -70;                // Need a powerful weapon to GIB
    self.gibbed = FALSE;                // no falling apart
    self.pain_flinch = 70;                // High pain threshold
    self.blockudeath = TRUE;            // no humanoid death sound
    if (self.height < 1) self.height = MONAI_ABOVEDIST; // Custom height
    self.attack_offset = '16 28 22';    // fireball start location
    self.deathstring = " was burned by a Gargoyle\n";
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = GargoyleCheckAttack;
    self.th_pain = garg_pain; 
    self.th_die = garg_die;
        
    if(!self.classtype) self.classtype = CT_MONGARGOYLE;
    if (!self.classgroup) self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEFLY;

    // Fix double spawnflags problem - default to statue
    if (self.spawnflags & MON_STATUE && self.spawnflags & MON_GARGOYLE_PERCH) {
        dprint("\b[GARGOYLE]\b Cannot be perch and statue!\n");
        self.spawnflags = self.spawnflags - MON_GARGOYLE_PERCH;
    }    
    
    //----------------------------------------------------------------------
    // Perched Gargoyles have special idle animation + idle sounds
    // The AI needs to be setup 40 units forward and 24 units up
    // to be positioned feet flat on perch surface
    if (self.spawnflags & MON_GARGOYLE_PERCH) {
        self.th_stand = self.th_walk = garg_perch1;
        self.th_run = self.th_missile = gargoyle_wakeup;
    }
    //----------------------------------------------------------------------
    // Statue Gargoyles setup in perch position with statue idle sounds
    // Randomly picks a frame position from perch animation set
    else if (self.spawnflags & MON_STATUE) {
        // Setup stone idle sounds
        self.idle_sound = "statue/idle1.wav";
        self.idle_sound2 = "statue/idle2.wav";
        precache_sound (self.idle_sound);
        precache_sound (self.idle_sound2);
        
        self.spawnstatue = TRUE;            // Start as a statue
        self.poisonous = FALSE;                // Cannot be status + poison
        self.th_stand = gargoyle_statue;    // Stone idle pose
        self.think1 = gargoyle_wakeup;        // Wakeup statue angry at player
        self.skin = 1;                        // Stone like skin
        self.gib1skin = self.gib2skin = 1;
        //self.resist_nails = 0.5;            // Stone resistance
        // Work out random starting pose
        self.pos1_x = $perch1;
        self.pos1_y = $perch24;
        self.pos1_z = fabs(self.pos1_y - self.pos1_x);
        self.frame = rint(self.pos1_x + random() * self.pos1_z);
    }
    //----------------------------------------------------------------------
    // Special spawning minion need to start spinning
    else if (self.classtype == CT_MINIONGARGOYLE) {
        self.th_stand = self.th_walk = self.th_run = gargoyle_grow;
        self.th_missile = SUB_Null;
    }
    //----------------------------------------------------------------------
    // Default gargoyle setup
    else {
        self.th_stand = garg_idle1;
        self.th_walk = garg_walk1;
        self.th_run = garg_run1;
        self.th_missile = garg_fire1;
    }
    
    monster_start();
};

//----------------------------------------------------------------------------
// A code way to spawn gargoyles (requires monster_minotaurminion entity)
//----------------------------------------------------------------------------
void(vector minion_org, entity minion_targ) minion_gargoyle =
{
    local entity minion;

    // Check if there is space to spawn entity
    if (entity_pcontent(minion_org)) return;
    
    // Self = minotaur, there is no egg 
    update_minioncount(self, 1);            // Update spawn counters
    
    minion = spawn();
    minion.classname = "monster_gargoyle";    // For function searching
    setorigin(minion, minion_org);            // Move to new location
    minion.owner = self;                    // Spawner Parent Link
    
    minion.effects = minion.flags = 0;        // make sure are blank
    minion.gibondeath = 1;                    // Always gib on death
    minion.classtype = CT_MINIONGARGOYLE;    // Special minion class
    minion.enemy = minion_targ;                // Target to attack
    minion.spawnflags = 0;
    minion.classgroup = CG_DEMON;            // Don't turn on master
    minion.minion_active = TRUE;            // Minion flag
    minion.bodyfadeaway = TRUE;                // Get rid of body
    
    // Check for poison gibs being cached first
    if (gibpoison && self.poisonous == TRUE) {
        minion.poisonous = self.poisonous;    // carry over from host spawner 
        minion.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
    }
    
    // Adopt parameters from host spawner
    minion.infightextra = self.infightextra;
    
    minion.mdl = "progs/mon_gargoyle.mdl";
    minion.headmdl = "progs/h_gargoyle.mdl";
    minion.gib1mdl = "progs/gib_gargwing1.mdl";    // Left wing
    minion.gib2mdl = "progs/gib_gargwing2.mdl";    // Right wing

    minion.pain_sound = "gargoyle/pain75.wav";
    minion.pain_sound2 = "gargoyle/pain100.wav";
    minion.idle_sound = "gargoyle/idle1.wav";
    minion.idle_soundcom = "gargoyle/perch1.wav";
    minion.idle_soundcom2 = "gargoyle/perch2.wav";
    minion.sight_sound = "gargoyle/sight1.wav";
    
    minion.nextthink = time + 0.01;
    minion.think = setup_gargoyle;
};