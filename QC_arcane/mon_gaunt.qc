/*==============================================================================
 GAUNT (Originally from Quoth - Kell/Necros/Preach)
 - Big thank you to necros for the his code base for this monster
==============================================================================*/
// (000) Perched on a ledge (idling)
$frame perch1 perch2 perch3 perch4 perch5 perch6 perch7 perch8
$frame perch9 perch10 perch11 perch12 perch13 perch14 perch15 perch16

// (016) Lift off from perch position
$frame launch1 launch2 launch3 launch4 launch5 launch6 launch7 launch8

// (024) Idling in mid air
$frame hover1 hover2 hover3 hover4 hover5 hover6 hover7 hover8
$frame hover9 hover10 hover11 hover12 hover13 hover14 hover15 hover16
$frame hover17 hover18 hover19 hover20

// (044) range attack (lightning/plasma)
$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8 
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15 attack16 
$frame attack17 attack18 attack19 attack20

// (064) Start of dive attack
$frame swoopStart1 swoopStart2 swoopStart3 swoopStart4
$frame swoopStart5 swoopStart6 swoopStart7 swoopStart8

// (072) Looped version of dive attack
$frame swoop1 swoop2 swoop3 swoop4 swoop5

// (077) Something, not sure what
$frame drop1 drop2 drop3 drop4 drop5

// (082) Swing wing around infront
$frame painA1 painA2 painA3 painA4 painA5 painA6

// (088) Knock head backward
$frame painB1 painB2 painB3 painB4 painB5

// (093) Fall backwards
$frame deathA1 deathA2 deathA3 deathA4 deathA5 deathA6 deathA7 deathA8

// (101) Fall forward
$frame deathB1 deathB2 deathB3 deathB4 deathB5 deathB6 deathB7 deathB8

void() gauntwings =
{ sound(self, CHAN_ITEM, "gaunt/wings1.wav", 1, ATTN_IDLE); };
void() gaunt_elecfinish;

//============================================================================
void() gaunt_idle1 = [ $hover1, gaunt_idle2 ] {monster_idle_sound();ai_stand();};
void() gaunt_idle2 = [ $hover2, gaunt_idle3 ] {ai_stand();};
void() gaunt_idle3 = [ $hover3, gaunt_idle4 ] {ai_stand();};
void() gaunt_idle4 = [ $hover4, gaunt_idle5 ] {ai_stand();};
void() gaunt_idle5 = [ $hover5, gaunt_idle6 ] {gauntwings();ai_stand();};
void() gaunt_idle6 = [ $hover6, gaunt_idle7 ] {ai_stand();};
void() gaunt_idle7 = [ $hover7, gaunt_idle8 ] {ai_stand();};
void() gaunt_idle8 = [ $hover8, gaunt_idle9 ] {ai_stand();};
void() gaunt_idle9 = [ $hover9, gaunt_idle10 ] {ai_stand();};
void() gaunt_idle10 = [ $hover10, gaunt_idle11 ] {ai_stand();};
void() gaunt_idle11 = [ $hover11, gaunt_idle12 ] {ai_stand();};
void() gaunt_idle12 = [ $hover12, gaunt_idle13 ] {ai_stand();};
void() gaunt_idle13 = [ $hover13, gaunt_idle14 ] {ai_stand();};
void() gaunt_idle14 = [ $hover14, gaunt_idle15 ] {ai_stand();};
void() gaunt_idle15 = [ $hover15, gaunt_idle16 ] {gauntwings();ai_stand();};
void() gaunt_idle16 = [ $hover16, gaunt_idle17 ] {ai_stand();};
void() gaunt_idle17 = [ $hover17, gaunt_idle18 ] {ai_stand();};
void() gaunt_idle18 = [ $hover18, gaunt_idle19 ] {ai_stand();};
void() gaunt_idle19 = [ $hover19, gaunt_idle20 ] {ai_stand();};
void() gaunt_idle20 = [ $hover20, gaunt_idle1 ] {ai_stand();};

//============================================================================
void() gaunt_walk1 = [ $hover1, gaunt_walk2 ] {monster_idle_sound();ai_walk(8);};
void() gaunt_walk2 = [ $hover2, gaunt_walk3 ] {ai_walk(8);};
void() gaunt_walk3 = [ $hover3, gaunt_walk4 ] {gauntwings();ai_walk(8);};
void() gaunt_walk4 = [ $hover4, gaunt_walk5 ] {ai_walk(8);};
void() gaunt_walk5 = [ $hover5, gaunt_walk6 ] {ai_walk(8);};
void() gaunt_walk6 = [ $hover6, gaunt_walk7 ] {ai_walk(8);};
void() gaunt_walk7 = [ $hover7, gaunt_walk8 ] {ai_walk(8);};
void() gaunt_walk8 = [ $hover8, gaunt_walk9 ] {ai_walk(8);};
void() gaunt_walk9 = [ $hover9, gaunt_walk10 ] {ai_walk(8);};
void() gaunt_walk10 = [ $hover10, gaunt_walk11 ] {ai_walk(8);};
void() gaunt_walk11 = [ $hover11, gaunt_walk12 ] {ai_walk(8);};
void() gaunt_walk12 = [ $hover12, gaunt_walk13 ] {ai_walk(8);};
void() gaunt_walk13 = [ $hover13, gaunt_walk14 ] {ai_walk(8);};
void() gaunt_walk14 = [ $hover14, gaunt_walk15 ] {ai_walk(8);};
void() gaunt_walk15 = [ $hover15, gaunt_walk16 ] {gauntwings();ai_walk(8);};
void() gaunt_walk16 = [ $hover16, gaunt_walk17 ] {ai_walk(8);};
void() gaunt_walk17 = [ $hover17, gaunt_walk18 ] {ai_walk(8);};
void() gaunt_walk18 = [ $hover18, gaunt_walk19 ] {ai_walk(8);};
void() gaunt_walk19 = [ $hover19, gaunt_walk20 ] {ai_walk(8);};
void() gaunt_walk20 = [ $hover20, gaunt_walk1 ] {ai_walk(8);};

//============================================================================
void() gaunt_run1 = [ $hover1, gaunt_run2 ] {monster_idle_sound();
    if (self.attachment && self.attachment.state == STATE_ON) gaunt_elecfinish();
    // Switch direction of strafing (random duration)
    if (self.meleeattack < time) {
        self.meleeattack = time + 4 + (random() + random() + random() * 4);
        self.lefty = rint(1 - self.lefty);
    }
    ai_run(8);
};
void() gaunt_run2 = [ $hover2, gaunt_run3 ] {ai_run(8);};
void() gaunt_run3 = [ $hover3, gaunt_run4 ] {ai_run(8);};
void() gaunt_run4 = [ $hover4, gaunt_run5 ] {ai_run(8);};
void() gaunt_run5 = [ $hover5, gaunt_run6 ] {gauntwings();ai_run(8);};
void() gaunt_run6 = [ $hover6, gaunt_run7 ] {ai_run(8);};
void() gaunt_run7 = [ $hover7, gaunt_run8 ] {ai_run(8);};
void() gaunt_run8 = [ $hover8, gaunt_run9 ] {ai_run(8);};
void() gaunt_run9 = [ $hover9, gaunt_run10 ] {ai_run(8);};
void() gaunt_run10 = [ $hover10, gaunt_run11 ] {ai_run(8);};
void() gaunt_run11 = [ $hover11, gaunt_run12 ] {ai_run(8);};
void() gaunt_run12 = [ $hover12, gaunt_run13 ] {ai_run(8);};
void() gaunt_run13 = [ $hover13, gaunt_run14 ] {ai_run(8);};
void() gaunt_run14 = [ $hover14, gaunt_run15 ] {ai_run(8);};
void() gaunt_run15 = [ $hover15, gaunt_run16 ] {gauntwings();ai_run(8);};
void() gaunt_run16 = [ $hover16, gaunt_run17 ] {ai_run(8);};
void() gaunt_run17 = [ $hover17, gaunt_run18 ] {ai_run(8);};
void() gaunt_run18 = [ $hover18, gaunt_run19 ] {ai_run(8);};
void() gaunt_run19 = [ $hover19, gaunt_run20 ] {ai_run(8);};
void() gaunt_run20 = [ $hover20, gaunt_run1 ] {ai_run(8);};

//============================================================================
// RANGE - Plasma from arc infront
//============================================================================
void() gaunt_plasma =
{
    local vector org, dir;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "gaunt/plasma_fire.wav", 1, ATTN_NORM);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(SUB_orgEnemyTarget() - self.origin);
    self.attack_speed = SPEED_GAUNTBOLT + (skill * SPEED_GAUNTSKILL);

    launch_plasma(org, dir, CT_MONGAUNT, self.attack_speed);
};

//----------------------------------------------------------------------
void() gaunt_elecfinish =
{
    setmodel(self.attachment, "");
    self.attachment.state = STATE_OFF;
};

//----------------------------------------------------------------------
void(float elecframe) gaunt_elecupdate =
{
    if (self.health < 1) return;
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = self.weaponglow;
        if (self.poisonous) self.attachment.skin = 1;
    }
    
    if (elecframe == 0) {
        self.attachment.state = STATE_ON;
        self.pain_finished = time + 1.2;        //don't go into pain whilst firing
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        sound (self, CHAN_WEAPON, "gaunt/plasma_start.wav", 1, ATTN_NORM);
    }

    ai_face();                    // Turn towards enemy
    makevectors(self.angles);
    setorigin(self.attachment, self.origin);
    self.attachment.angles = self.angles;
    self.attachment.frame = elecframe;
};

//----------------------------------------------------------------------
void() gaunt_fire1 = [ $attack1, gaunt_fire2 ] {gaunt_elecupdate(0);};
void() gaunt_fire2 = [ $attack2, gaunt_fire3 ] {gaunt_elecupdate(1);};
void() gaunt_fire3 = [ $attack3, gaunt_fire4 ] {gaunt_elecupdate(2);};
void() gaunt_fire4 = [ $attack4, gaunt_fire5 ] {gaunt_elecupdate(3);};
void() gaunt_fire5 = [ $attack5, gaunt_fire6 ] {gaunt_elecupdate(4);gauntwings();};
void() gaunt_fire6 = [ $attack6, gaunt_fire7 ] {gaunt_elecupdate(5);gaunt_plasma();};
void() gaunt_fire7 = [ $attack7, gaunt_fire8 ] {gaunt_elecupdate(6);};
void() gaunt_fire8 = [ $attack8, gaunt_fire9 ] {gaunt_elecupdate(7);gaunt_plasma();};
void() gaunt_fire9 = [ $attack9, gaunt_fire10 ] {gaunt_elecupdate(8);};
void() gaunt_fire10 = [ $attack10, gaunt_fire11 ] {ai_face();
    if (skill > SKILL_NORMAL) gaunt_plasma();
    gaunt_elecfinish();};
void() gaunt_fire11 = [ $attack11, gaunt_fire12 ] {};
void() gaunt_fire12 = [ $attack12, gaunt_fire13 ] {};
void() gaunt_fire13 = [ $attack13, gaunt_fire14 ] {};
void() gaunt_fire14 = [ $attack14, gaunt_fire15 ] {};
void() gaunt_fire15 = [ $attack15, gaunt_fire16 ] {gauntwings();};
void() gaunt_fire16 = [ $attack16, gaunt_fire17 ] {};
void() gaunt_fire17 = [ $attack17, gaunt_fire18 ] {};
void() gaunt_fire18 = [ $attack18, gaunt_fire19 ] {ai_face();};
void() gaunt_fire19 = [ $attack19, gaunt_fire20 ] {ai_face();};
void() gaunt_fire20 = [ $attack20, gaunt_run1 ] {};

//============================================================================
// PERCH + WAKEUP
//============================================================================
void() gaunt_perch1 = [ $perch1, gaunt_perch2 ] {monster_idle_sound();ai_stand();};
void() gaunt_perch2 = [ $perch2, gaunt_perch3 ] {ai_stand();};
void() gaunt_perch3 = [ $perch3, gaunt_perch4 ] {ai_stand();};
void() gaunt_perch4 = [ $perch4, gaunt_perch5 ] {ai_stand();};
void() gaunt_perch5 = [ $perch5, gaunt_perch6 ] {ai_stand();};
void() gaunt_perch6 = [ $perch6, gaunt_perch7 ] {ai_stand();};
void() gaunt_perch7 = [ $perch7, gaunt_perch8 ] {ai_stand();};
void() gaunt_perch8 = [ $perch8, gaunt_perch9 ] {ai_stand();};
void() gaunt_perch9 = [ $perch9, gaunt_perch10 ] {ai_stand();};
void() gaunt_perch10 = [ $perch10, gaunt_perch11 ] {ai_stand();};
void() gaunt_perch11 = [ $perch11, gaunt_perch12 ] {ai_stand();};
void() gaunt_perch12 = [ $perch12, gaunt_perch13 ] {ai_stand();};
void() gaunt_perch13 = [ $perch13, gaunt_perch14 ] {ai_stand();};
void() gaunt_perch14 = [ $perch14, gaunt_perch15 ] {ai_stand();};
void() gaunt_perch15 = [ $perch15, gaunt_perch16 ] {ai_stand();};
void() gaunt_perch16 = [ $perch16, gaunt_perch1 ] {ai_stand();};

//----------------------------------------------------------------------
// Sightsound and very quick animation while moving forward
void() gaunt_wakeup1 = [ $launch1, gaunt_wakeup2 ] {monster_sightsound();};
void() gaunt_wakeup2 = [ $launch2, gaunt_wakeup3 ] {ai_forward(4);};
void() gaunt_wakeup3 = [ $launch3, gaunt_wakeup4 ] {ai_forward(4);};
void() gaunt_wakeup4 = [ $launch4, gaunt_wakeup5 ] {ai_forward(4);};
void() gaunt_wakeup5 = [ $launch5, gaunt_wakeup6 ] {ai_forward(4);};
void() gaunt_wakeup6 = [ $launch6, gaunt_wakeup7 ] {gauntwings();ai_forward(4);};
void() gaunt_wakeup7 = [ $launch7, gaunt_wakeup8 ] {ai_forward(4);};
void() gaunt_wakeup8 = [ $launch8, gaunt_run1 ] {
    FoundHuntTarget(TRUE);            // Setup goals and warn other monsters
};

//----------------------------------------------------------------------
void() gaunt_wakeup =
{
    // Dead already?
    if (self.health < 1) return;

    // No longer need perch spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_GAUNT_PERCH);
    self.spawnstatue = FALSE;        // No longer stationary

    self.takedamage = DAMAGE_AIM;        // Can receive damage
    self.use = SUB_Null;                // no re-trigger
    
    // Restore all think state functions
    self.th_stand = gaunt_idle1;
    self.th_walk = gaunt_walk1;
    self.th_run = gaunt_run1;
    self.th_missile = gaunt_fire1;

    // Allow gaunt to fly up, no more ground checks
    self.flags = self.flags | FL_FLY;
    self.movetype = MOVETYPE_STEP;
    self.pain_finished = time + 1.5;        // No pain
    self.attack_finished = 1.5 + random();    // No skill adjustment
    gaunt_wakeup1();
};

//============================================================================
// PAIN
//============================================================================
// Long - Swing wing around infront
void()     gaunt_painA1 =[ $painA1, gaunt_painA2 ] {};
void()     gaunt_painA2 =[ $painA2, gaunt_painA3 ] {};
void()     gaunt_painA3 =[ $painA3, gaunt_painA4 ] {};
void()     gaunt_painA4 =[ $painA4, gaunt_painA5 ] {};
void()     gaunt_painA5 =[ $painA5, gaunt_painA6 ] {};
void()     gaunt_painA6 =[ $painA6, gaunt_run1 ] {};

// Short - Knock head backward
void()     gaunt_painB1 =[ $painB1, gaunt_painB2 ] {};
void()     gaunt_painB2 =[ $painB2, gaunt_painB3 ] {};
void()     gaunt_painB3 =[ $painB3, gaunt_painB4 ] {};
void()     gaunt_painB4 =[ $painB4, gaunt_painB5 ] {};
void()     gaunt_painB5 =[ $painB5, gaunt_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) gaunt_pain =
{
    // Check for perching gaunt, need special wakeup
    if (self.spawnflags & MON_GARGOYLE_PERCH) gaunt_wakeup();
    else {
        // Check all pain conditions and set up what to do next
        monster_pain_check(attacker, damage);

        // make sure any electricity is hidden
        if (self.attachment) gaunt_elecfinish();

        // Any pain animation/sound required?
        if (self.pain_check > 0) {
            sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
            if (self.pain_check == 1 || self.pain_check == 2) {
                if (self.lip < 0.4) gaunt_painB1();    // Long
                else gaunt_painA1();                // Short
            }
        }
    }
};

//============================================================================
void() gaunt_deathA1 = [ $deathA1, gaunt_deathA2 ] {};
void() gaunt_deathA2 = [ $deathA2, gaunt_deathA3 ] {monster_check_gib();};
void() gaunt_deathA3 = [ $deathA3, gaunt_deathA4 ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() gaunt_deathA4 = [ $deathA4, gaunt_deathA5 ] {};
void() gaunt_deathA5 = [ $deathA5, gaunt_deathA6 ] {};
void() gaunt_deathA6 = [ $deathA6, gaunt_deathA7 ] {};
void() gaunt_deathA7 = [ $deathA7, gaunt_deathA8 ] {monster_death_postcheck();};
void() gaunt_deathA8 = [ $deathA8, gaunt_deathA8 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() gaunt_deathB1 = [ $deathB1, gaunt_deathB2 ] {};
void() gaunt_deathB2 = [ $deathB2, gaunt_deathB3 ] {monster_check_gib();};
void() gaunt_deathB3 = [ $deathB3, gaunt_deathB4 ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void() gaunt_deathB4 = [ $deathB4, gaunt_deathB5 ] {};
void() gaunt_deathB5 = [ $deathB5, gaunt_deathB6 ] {};
void() gaunt_deathB6 = [ $deathB6, gaunt_deathB7 ] {};
void() gaunt_deathB7 = [ $deathB7, gaunt_deathB8 ] {monster_death_postcheck();};
void() gaunt_deathB8 = [ $deathB8, gaunt_deathB8 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() gaunt_die =
{
    // Check for perching gaunt, need to blow up in gibs
    // In an awkward pose that does not go well into death animations
    if (self.spawnflags & MON_GAUNT_PERCH) self.gibondeath = TRUE;

    // Make sure any wings sound is silenced
    sound (self, CHAN_ITEM, SOUND_EMPTY, 1, ATTN_NORM);
    
    // If casting electricity, make it explode!
    if (self.attachment) {
        if (self.attachment.state == STATE_ON) {
            self.gibondeath = TRUE;
            SpawnProjectileSmoke(self.origin, 200, 50, 150);
            SpawnProjectileSmoke(self.origin, 200, 50, 150);
            if (self.poisonous) self.lip = PARTICLE_BURST_GREEN;
            else self.lip = PARTICLE_BURST_BLUE;
            particle_explode(self.origin-'0 0 16', 50+random()*50, 2, self.lip, PARTICLE_BURST_UPWARD);
            makevectors(self.angles);
            self.oldorigin = self.origin + v_forward*8;
            if (self.poisonous) self.lip = EXPLODE_POISON_BIG;
            else self.lip = EXPLODE_PLASMA_BIG;
            SpawnExplosion(self.lip, self.oldorigin, SOUND_REXP3);
        }
        // make sure any electricity is hidden
        gaunt_elecfinish();
    }
    
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "gaunt/death.wav", 1, ATTN_NORM);
        self.flags = FL_MONSTER;                // reset all flag conditions
        self.movetype = MOVETYPE_TOSS;            // Affected by gravity
        self.velocity_x = -200 + 400*random();
        self.velocity_y = -200 + 400*random();
        self.velocity_z = 100 + 100*random();
        if (random() < 0.5) gaunt_deathA1();
        else gaunt_deathB1();
    }
};

/*======================================================================
QUAKED monster_gaunt (1 0.2 0) (-16 -16 -24) (16 16 24)
======================================================================*/
void() monster_gaunt =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_gaunt.mdl";
    self.headmdl = "progs/h_gaunt.mdl";
    self.weaponglow = "progs/mon_gaunt_elec.mdl";

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (self.weaponglow);        // Lightning effect (2 colour)
    precache_model (MODEL_PROJ_GAPLASMA);    // Plasma spike/bolt

    self.idle_sound = "gaunt/idle1.wav";
    self.idle_sound2 = "gaunt/idle3.wav";
    self.idle_soundcom = "gaunt/idle2.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);

    precache_sound ("gaunt/wings1.wav");
    precache_sound ("gaunt/plasma_start.wav");
    precache_sound ("gaunt/plasma_fire.wav");
    
    self.pain_sound = "gaunt/pain.wav";
    precache_sound (self.pain_sound);
    precache_sound ("gaunt/death.wav");

    self.sight_sound = "gaunt/sight.wav";
    precache_sound (self.sight_sound);

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_GAPLASMAGRN);
    }
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 120;
    self.gibhealth = -70;                // Need a powerful weapon to GIB
    self.gibbed = FALSE;                // no falling apart
    self.pain_flinch = 70;                // High pain threshold
    self.blockudeath = TRUE;            // no humanoid death sound
    self.height = MONAI_ABOVEDIST;        // Enemytarget distance above
    self.attack_offset = '8 0 -4';        // Plasma start location
    self.deathstring = " was blasted by a Gaunt\n";

    self.th_checkattack = GauntCheckAttack;
    self.th_pain = gaunt_pain; 
    self.th_die = gaunt_die;
        
    //----------------------------------------------------------------------
    // Perched Gaunt have special idle animation + idle sounds
    if (self.spawnflags & MON_GAUNT_PERCH) {
        self.th_stand = self.th_walk = gaunt_perch1;
        self.th_run = self.th_missile = gaunt_wakeup;
    }
    //----------------------------------------------------------------------
    // Default gaunt setup
    else {
        self.th_stand = gaunt_idle1;
        self.th_walk = gaunt_walk1;
        self.th_run = gaunt_run1;
        self.th_missile = gaunt_fire1;
    }

    self.classtype = CT_MONGAUNT;
    self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEFLY;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    monster_start();
};
