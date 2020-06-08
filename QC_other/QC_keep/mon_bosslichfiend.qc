/*==============================================================================
Lich Fiend from RMQ Project
==============================================================================*/
$frame brokenmesh

// Sway from left to right
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15 stand16
$frame stand17 stand18 stand19 stand20

// Slow walk with upright frame
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17

// Running with rigid shoulders, crouched down
$frame run1 run2 run3 run4 run5 run6 run7 run8 run9 run10

// Running with asymmetrical frame, left arm lower
$frame runB1 runB2 runB3 runB4 runB5 runB6 runB7 runB8
$frame runB9 runB10 runB11 runB12 runB13

// RangeA = left and right melee swing OR throw fireballs
$frame swing1 swing2 swing3 swing4 swing5 swing6 swing7 swing8
$frame swing9 swing10 swing11 swing12 swing13 swing14 swing15 swing16
$frame swing17 swing18 swing19 swing20

// RangeB = lift arms above head summoning sphere/voreballs
$frame sphere1 sphere2 sphere4 sphere5 sphere6 sphere7 sphere8
$frame sphere9 sphere10 sphere11 sphere12 sphere13 sphere14 sphere15 
$frame sphere16 sphere17 sphere18 sphere19 sphere20 sphere21 sphere22
$frame sphere23 sphere24 sphere25

// RangeC = crounch and rise up with arms outstretched (summon fiends)
$frame summon1 summon2 summon3 summon4 summon5 summon6 summon7 summon8
$frame summon9 summon10 summon11 summon12 summon13 summon14 summon15 summon16
$frame summon17 summon18 summon19 summon20 summon21 summon22 summon23 summon24
$frame summon25 summon26 summon27 summon28 summon29 summon30

// SpellB - Lift off ground with arms outstretched (regen/airstrike)
$frame regen1 regen2 regen3 regen4 regen5 regen6 regen7 regen8
$frame regen9 regen10 regen11 regen12 regen13 regen14 regen15
$frame regen16 regen17 regen18 regen19 regen20 regen21 regen22
$frame regen23 regen24 regen25 regen26 regen27 regen28 regen29
$frame regen30 regen31 regen32 regen33

float LFIEND_PHASE1 = 1;    // At the altar
float LFIEND_PHASE2 = 2;    // Fighting
float LFIEND_PHASE3 = 3;    // Spawning mode
float LFIEND_PHASE4 = 4;    // Death

//======================================================================
// Global functions
//======================================================================
// Special streamlined player find function
//----------------------------------------------------------------------
float() lfiend_FindTarget =
{
    local entity client;
    
    // Get the obvious exception(s) done first
    if (self.health < 1) return FALSE;
    if (intermission_running) return FALSE;

    // Find a client in current PVS
    client = checkclient ();
    
    // Go through all the exception(s)
    if (!client) return FALSE;
    if (!(client.flags & FL_CLIENT)) return FALSE;
    if (client.flags & FL_NOTARGET) return FALSE;
    if (client.items & IT_INVISIBILITY) return FALSE;
    
    // Check range and visibility of player
    enemy_vis = visible(client);
    if (!enemy_vis) return FALSE;
    if (!infront(client)) return FALSE;

    // Finally found something
    self.enemy = client;
    self.oldorigin = self.origin;        // Save origin
    self.goalentity = self.enemy;        // Focus on enemy
    // Setup turning angle towards new enemy
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);

    // We have a winner!
    return TRUE;    
};

//----------------------------------------------------------------------
// Setup wave HP and trigger boundaries
//----------------------------------------------------------------------
void() lfiend_WaveSetupHP =
{
    // Is there anymore boss waves left?
    if (self.bosswave >= self.bosswavetotal) {
        // Only one wave left (death is final trigger)
        self.health = self.bosswaveqty;
        self.bosswavetrig = -1000;
    }
    else {
        // Multiple waves are still left (reset hp+trigger)
        // Always reset HP to stop high DPS weapons trashing waves boundaries
        self.health = ((self.bosswavetotal - self.bosswave) + 1) * self.bosswaveqty;
        // The wave trigger is always one wave lower
        self.bosswavetrig = self.health - self.bosswaveqty;
    }
    // Debug messages for wave and health
    dprint("\b[BOSS]\b Wave ("); dprint(ftos(self.bosswave));
    dprint(" / "); dprint(ftos(self.bosswavetotal));
    dprint(") HP ("); dprint(ftos(self.health));
    dprint(") Trig ("); dprint(ftos(self.bosswavetrig));
    dprint(")\n");
};

//----------------------------------------------------------------------
// Check if HP reached next wave, spawn more fiends
//----------------------------------------------------------------------
float() lfiend_WaveCheck =
{
    // Check for boss wave boundary event
    if (self.health > 1 && self.health < self.bosswavetrig) {
        // Check for wave boundary triggers
        self.noise = "";
        if (self.bosswave == 1) self.noise = self.noise1;
        else if(self.bosswave == 2) self.noise = self.noise2;
        else if(self.bosswave == 3) self.noise = self.noise3;
        else if(self.bosswave == 4) self.noise = self.noise4;
        
        // Is there any trigger for the wave boundary?
        if (self.noise != "") trigger_strs(self.noise, self);
        self.th_jump();            // Summon fiends

        // Update Boss wave parameters (next wave!)
        self.bosswave = self.bosswave + 1;
        lfiend_WaveSetupHP();    // Reset trigger/hp
        return TRUE;
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check the tether system
//----------------------------------------------------------------------
float() lfiend_CheckTether =
{
    self.t_length = vlen(self.origin - self.movelast.origin);
    // Check the most obvious first, inside tether range?
    if (self.t_length < MONAI_MAXLFIEND) return FALSE;
    else {
        // If player or tether close to each other?
        if ( infront(self.movelast) && infront(self.enemy) )
            return FALSE;
        // Stop moving around
        else return TRUE;
    }
};
    
//----------------------------------------------------------------------
// Boss Fiend game play logic
//----------------------------------------------------------------------
void() LfiendCheckAttack = 
{
    //----------------------------------------------------------------------
    // Check Melee range and start clawing at the player
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_MELEELFIEND) {
        self.attack_state = AS_MELEE;
        return;
    }

    //----------------------------------------------------------------------
    // Range attacks (Fireballs, Giant Spheres and Firestorms)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        // Quick fireball attacks
        if (self.enemydist < MONAI_CLOSELFIEND && random() < 0.75) {
            // Skill 0=3s, 1=2.25s, 2=1.5s, 3=0.75s
            self.attack_speed = (4 - skill) * 0.75;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_MISSILE;
            return;
        }
        // Summon giant sphere of Doom!
        else {
            // Skill 0=4s, 1=3s, 2=2s, 3=1s
            self.attack_speed = (4 - skill) * 1;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_JUMP;
            return;
        }
    }
};

//======================================================================
// MONSTER STATES (stand, walk run, attack, pain and death!
//======================================================================
void() lfiend_stand1 =[ $stand1, lfiend_stand2] {monster_idle_sound();ai_stand();};
void() lfiend_stand2 =[ $stand2, lfiend_stand3] {ai_stand();};
void() lfiend_stand3 =[ $stand3, lfiend_stand4] {ai_stand();};
void() lfiend_stand4 =[ $stand4, lfiend_stand5] {ai_stand();};
void() lfiend_stand5 =[ $stand5, lfiend_stand6] {ai_stand();};
void() lfiend_stand6 =[ $stand6, lfiend_stand7] {ai_stand();};
void() lfiend_stand7 =[ $stand7, lfiend_stand8] {ai_stand();};
void() lfiend_stand8 =[ $stand8, lfiend_stand9] {ai_stand();};
void() lfiend_stand9 =[ $stand9, lfiend_stand10] {ai_stand();};
void() lfiend_stand10 =[ $stand10, lfiend_stand11] {ai_stand();};
void() lfiend_stand11 =[ $stand11, lfiend_stand12] {ai_stand();};
void() lfiend_stand12 =[ $stand12, lfiend_stand13] {ai_stand();};
void() lfiend_stand13 =[ $stand13, lfiend_stand14] {ai_stand();};
void() lfiend_stand14 =[ $stand14, lfiend_stand15] {ai_stand();};
void() lfiend_stand15 =[ $stand15, lfiend_stand16] {ai_stand();};
void() lfiend_stand16 =[ $stand16, lfiend_stand17] {ai_stand();};
void() lfiend_stand17 =[ $stand17, lfiend_stand18] {ai_stand();};
void() lfiend_stand18 =[ $stand18, lfiend_stand19] {ai_stand();};
void() lfiend_stand19 =[ $stand19, lfiend_stand20] {ai_stand();};
void() lfiend_stand20 =[ $stand20, lfiend_stand1] {ai_stand();};

//======================================================================
void() lfiend_walk1 =[ $walk1, lfiend_walk2] {monster_idle_sound();ai_walk(8);};
void() lfiend_walk2 =[ $walk2, lfiend_walk3] {ai_walk(8);};
void() lfiend_walk3 =[ $walk3, lfiend_walk4] {ai_walk(8);};
void() lfiend_walk4 =[ $walk4, lfiend_walk5] {ai_walk(8);};
void() lfiend_walk5 =[ $walk5, lfiend_walk6] {ai_walk(8);};
void() lfiend_walk6 =[ $walk6, lfiend_walk7] {ai_walk(8);};
void() lfiend_walk7 =[ $walk7, lfiend_walk8] {ai_walk(8);};
void() lfiend_walk8 =[ $walk8, lfiend_walk9] {ai_walk(8);};
void() lfiend_walk9 =[ $walk9, lfiend_walk10] {ai_walk(8);};
void() lfiend_walk10 =[ $walk10, lfiend_walk11] {ai_walk(8);};
void() lfiend_walk11 =[ $walk11, lfiend_walk12] {ai_walk(8);};
void() lfiend_walk12 =[ $walk12, lfiend_walk13] {ai_walk(8);};
void() lfiend_walk13 =[ $walk13, lfiend_walk14] {ai_walk(8);};
void() lfiend_walk14 =[ $walk14, lfiend_walk15] {ai_walk(8);};
void() lfiend_walk15 =[ $walk15, lfiend_walk16] {ai_walk(8);};
void() lfiend_walk16 =[ $walk16, lfiend_walk17] {ai_walk(8);};
void() lfiend_walk17 =[ $walk17, lfiend_walk1] {ai_walk(8);};

//======================================================================
void() lfiend_run1 =[ $run1, lfiend_run2] {monster_idle_sound();ai_run(8);};
void() lfiend_run2 =[ $run2, lfiend_run3] {ai_run(8);};
void() lfiend_run3 =[ $run3, lfiend_run4] {ai_run(8);};
void() lfiend_run4 =[ $run4, lfiend_run5] {ai_run(8);};
void() lfiend_run5 =[ $run5, lfiend_run6] {ai_run(8);};
void() lfiend_run6 =[ $run6, lfiend_run7] {ai_run(8);};
void() lfiend_run7 =[ $run7, lfiend_run8] {ai_run(8);};
void() lfiend_run8 =[ $run8, lfiend_run9] {ai_run(8);};
void() lfiend_run9 =[ $run9, lfiend_run10] {ai_run(8);};
void() lfiend_run10 =[ $run10, lfiend_run1] {ai_run(8);};

//----------------------------------------------------------------------
void() lfiend_runB1 =[ $runB1, lfiend_runB2] {monster_idle_sound();ai_run(8);};
void() lfiend_runB2 =[ $runB2, lfiend_runB3] {ai_run(8);};
void() lfiend_runB3 =[ $runB3, lfiend_runB4] {ai_run(8);};
void() lfiend_runB4 =[ $runB4, lfiend_runB5] {ai_run(8);};
void() lfiend_runB5 =[ $runB5, lfiend_runB6] {ai_run(8);};
void() lfiend_runB6 =[ $runB6, lfiend_runB7] {ai_run(8);};
void() lfiend_runB7 =[ $runB7, lfiend_runB8] {ai_run(8);};
void() lfiend_runB8 =[ $runB8, lfiend_runB9] {ai_run(8);};
void() lfiend_runB9 =[ $runB9, lfiend_runB10] {ai_run(8);};
void() lfiend_runB10 =[ $runB10, lfiend_runB11] {ai_run(8);};
void() lfiend_runB11 =[ $runB11, lfiend_runB12] {ai_run(8);};
void() lfiend_runB12 =[ $runB12, lfiend_runB13] {ai_run(8);};
void() lfiend_runB13 =[ $runB13, lfiend_runB1] {ai_run(8);};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() lfiend_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
    }
};

//----------------------------------------------------------------------
void() lfiend_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() lfiend_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
}

//======================================================================
// ATTACK A  - Double swing/swipe melee attack
// Animation - RangeA = left and right melee swing
//======================================================================
void() lfiend_attackA1 = {};

//======================================================================
// ATTACK B  - Fire a couple of fireballs
// Animation - RangeA = throw fireballs
//======================================================================
void() lfiend_attackB1 = {};

//======================================================================
// ATTACK C  - Sphere of Doom above head!
// Animation - RangeB = lift arms above head summoning sphere/voreballs
//======================================================================
void() lfiend_attackC1 = {};

//======================================================================
// ATTACK D  - Summon Fiends
// Animation - RangeC = crounch and rise up with arms outstretched
//======================================================================
void() lfiend_attackD1 = {};

//======================================================================
// ATTACK E  - Regen/Air Strike
// Animation - Lift off ground with arms outstretched
//======================================================================

//============================================================================
// PAIN and DEATH
//============================================================================
void() lfiend_painA1 = [ $runB1, lfiend_painA2 ] {};
void() lfiend_painA2 = [ $runB2, lfiend_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) lfiend_pain =
{
    // Finish with all attachments
    lfiend_finish_attachment();

    // Check for boss wave trigger events
    if (lfiend_WaveCheck() == TRUE) {self.th_jump(); return;}

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            lfiend_painA1 ();    // Short recoil
        }
    }
};

//============================================================================
// The Reaper comes for Slenderman!
//============================================================================
void() lfiend_explode = 
{
    // Check for any final trigger events
    if (self.message2 != "") trigger_strs(self.message2,self);
    
    // No more Boss!
    entity_hide(self);
    
    // Blue ID particle explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 35);
    WriteByte (MSG_BROADCAST, 8);
    
    // Classic sprite/DP explosion
    SpawnExplosion(EXPLODE_PLASMA_BIG, self.origin, "lfiend/explode_death.wav");

    // Gib explosive fountain!?!
    self.max_health = MON_GIBEXPLOSION;
    ThrowGib(4, 10 + rint(random()*3));
    ThrowGib(5, 10 + rint(random()*3));
};

//----------------------------------------------------------------------
void() lfiend_die1 = [ $regen1, lfiend_die2 ] {};
void() lfiend_die2 = [ $regen2, lfiend_die3 ] {};
void() lfiend_die3 = [ $regen3, lfiend_die4 ] {self.solid = SOLID_NOT;};
void() lfiend_die4 = [ $regen4, lfiend_die5 ] {};
void() lfiend_die5 = [ $regen5, lfiend_die6 ] {};
void() lfiend_die6 = [ $regen6, lfiend_die7 ] {};
void() lfiend_die7 = [ $regen7, lfiend_die8 ] {};
void() lfiend_die8 = [ $regen8, lfiend_die9 ] {};
void() lfiend_die9 = [ $regen9, lfiend_die10 ] {};
void() lfiend_die10 = [ $regen10, lfiend_die11 ] {};
void() lfiend_die11 = [ $regen11, lfiend_die12 ] {};
void() lfiend_die12 = [ $regen12, lfiend_die13 ] {};
void() lfiend_die13 = [ $regen13, lfiend_die14 ] {};
void() lfiend_die14 = [ $regen14, lfiend_die15 ] {};
void() lfiend_die15 = [ $regen15, lfiend_die15 ] {};

//----------------------------------------------------------------------
void() lfiend_die =
{
    self.deadflag = DEAD_DEAD;    // Boss Fiend super dead
    self.effects = 0;            // Remove effects on death
    lfiend_finish_attachment();    // Remove any attachments
    lfiend_remove_attachment();

    sound (self, CHAN_VOICE, "lfiend/death.wav", 1, ATTN_NORM);
    lfiend_die1 ();
};

//======================================================================
// SPECIAL PHASES
//======================================================================
void() lfiend_altar1 = {};

//----------------------------------------------------------------------
void() lfiend_rise1 = {};

//======================================================================
// Setup Fiend after trigger event
//======================================================================
void() lfiend_awake =
{
    self.use = SUB_Null;
    self.style = LFIEND_PHASE1;        // Teleport in
    self.flags = FL_MONSTER;        // Reset flag
    
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    self.takedamage = DAMAGE_NO;    // Still immune to damage
    self.yaw_speed = 20;            // Average Speed
    self.velocity = '0 0 0';        // Make sure stationary
    self.pain_longanim = FALSE;        // No axe advantage
    self.noinfighting = TRUE;        // No infighting
    self.count = skill+1;            // Total amount of minions

    // Setup Ammo Resistance
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0.75;
    
    //Resist_CheckRange(self);        // Double check values
    Resist_Save(self);                // Save for Later
    Resist_ChangeType(self,TRUE);    // resist everything

    // Setup boss waves and overall health
    self.bosswave = 1;
    self.bosswavetotal = 5;
    self.bosswaveqty = 500;
    self.max_health = self.bosswavetotal * self.bosswaveqty;
    // Setup boss wave HP + trigger event
    lfiend_WaveSetupHP();

    // Restore all think functions
    self.th_checkattack = LfiendCheckAttack;
    self.th_stand = lfiend_stand1;
    self.th_walk = lfiend_walk1;
    self.th_run = lfiend_run1;
    self.th_melee = lfiend_attackA1;
    self.th_missile = lfiend_attackB1;
    self.th_jump = lfiend_attackD1;
    self.th_pain = lfiend_pain;
    self.th_die = lfiend_die;

    self.pain_finished = time + 3;        // Make pain go away
    self.attack_finished = time + 2;    // Reset attack system
    self.gibhealth = -1000;                // Special death sequence

    // Setup player focus
    if (activator.flags & FL_CLIENT) {
        self.goalentity = self.enemy = activator;
    }
    else self.enemy = world;

    // Check there is space for Lich Fiend to spawn
    self.origin_z = self.origin_z + 1;
    droptofloor();
    if (!walkmove(0,0)) {
        // this condition should be a map spawn event only
        dprint ("\b[STUCK]\b "); dprint (self.classname);
        dprint (" at "); dprint (vtos(self.origin));
        dprint ("\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
    }

    // Make sure all death triggers are setup ready
    self.message2 = self.target;
    self.target = self.target2 = self.deathtarget = "";
};

/*======================================================================
 QUAKED monster_lichfiend (1 0 0) (-32 -32 -24) (32 32 128)
======================================================================*/
void() monster_lichfiend =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/ad171/mon_bossfiend.mdl";
    precache_model (self.mdl);

    // Typical idle, pain and sight sounds
    self.idle_sound = "lfiend/idle1.wav";
    self.idle_sound2 = "lfiend/idle2.wav";
    self.pain_sound = "lfiend/pain.wav";
    self.sight_sound = "lfiend/sight.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.pain_sound);
    precache_sound (self.sight_sound);
    precache_sound ("lfiend/death.wav");
    precache_sound ("lfiend/explode_death.wav");
    precache_sound ("lfiend/implode_death.wav");
    precache_sound ("lfiend/elec_arch1.wav");
    precache_sound ("lfiend/elec_arch2.wav");
    
    // Attack A - Melee
    // Attack C - Fireball
    // Attack C - Sphere
    // Attack D - Summon
    
    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.bbmins = '-32 -32 -24';    // Special Size
    self.bbmaxs = '32 32 80';
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)

    self.health = self.max_health = MEGADEATH;
    self.pain_finished = time + LARGE_TIMER;
    self.gibbed = FALSE;            // Still in one piece

    self.pain_flinch = 200;            // 1/2 Shambler level
    self.pain_timeout = 2;            // Stop constant pain
    self.steptype = FS_TYPELARGE;    // Shambler feet
    
    self.deadflag = DEAD_NO;                    // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.takedamage = DAMAGE_NO;    // Immune to damage
    if (self.deathstring == "")  self.deathstring = " was shredded by the Lich Fiend\n";

    self.classtype = CT_MONLICHFIEND;
    self.classgroup = CG_DEMON;
    self.classmove = MON_MOVEWALK;
    self.style = 0;
    
    // No targetname = no trigger!
    if (self.targetname == "") {
        dprint("\b[LICHFIEND]\b Missing trigger name!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }

    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);

    total_monsters = total_monsters + 1;
    self.use = lfiend_awake;
};
