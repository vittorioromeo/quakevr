/*==============================================================================
ICE GOLEM (Heavily Inspired by Ne_Ruins MOD) (Hexen2 Model)
==============================================================================*/

// (000) standing still with heavy breathing
$frame wait1 wait2 wait3 wait4 wait5 wait6 wait7 wait8
$frame wait9 wait10 wait11 wait12 wait13 wait14 wait15 wait16
$frame wait17 wait18 wait19 wait20 wait21 wait22

// (022) Transfer B
$frame tranB1 tranB2 tranB3 tranB4 tranB5 tranB6 tranB7 tranB8
$frame tranB9 tranB10 tranB11 tranB12 tranB13

// (035) statue to walking
$frame birth1 birth2 birth3 birth4 birth5 birth6 birth7 birth8
$frame birth9 birth10 birth11 birth12 birth13 birth14 birth15 birth16

// (051) Really slow walk
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame walk17 walk18 walk19 walk20 walk21 walk22 walk23 walk24
$frame walk25 walk26 walk27 walk28 walk29 walk30 walk31 walk32
$frame walk33 walk34

// (085) Running
$frame run1 run2 run3 run4 run5 run6 run7 run8
$frame run9 run10 run11 run12 run13 run14 run15 run16
$frame run17 run18 run19 run20 run21 run22 run23 run24

// (109) Right Fist Punch
$frame punch1 punch2 punch3 punch4 punch5 punch6 punch7 punch8
$frame punch9 punch10 punch11 punch12 punch13 punch14 punch15 punch16
$frame punch17 punch18 punch19 punch20 punch21 punch22 punch23 punch24

// (133) Right Fist Pound ground
$frame pound1 pound2 pound3 pound4 pound5 pound6 pound7 pound8
$frame pound9 pound10 pound11 pound12 pound13 pound14 pound15 pound16
$frame pound17 pound18 pound19 pound20 pound21 pound22 pound23 pound24

// (157) Death Forward
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 death22

// (179) Magic attack from torso
$frame magic1 magic2 magic3 magic4 magic5 magic6 magic7 magic8
$frame magic9 magic10 magic11 magic12 magic13 magic14 magic15 magic16
$frame magic17 magic18 magic19 magic20 magic21 magic22 magic23 magic24

// (203) Stomp Foot on ground
$frame stomp1 stomp2 stomp3 stomp4 stomp5 stomp6 stomp7 stomp8
$frame stomp9 stomp10 stomp11 stomp12 stomp13 stomp14 stomp15 stomp16
$frame stomp17 stomp18 stomp19 stomp20 stomp21 stomp22 stomp23 stomp24

// (227) Rush forward and knock player away
$frame knock1 knock2 knock3 knock4 knock5 knock6 knock7 knock8
$frame knock9 knock10 knock11 knock12 knock13 knock14 knock15 knock16
$frame knock17 knock18 knock19 knock20 knock21 knock22 knock23 knock24

float ICEG_PHASE1 = 1;    // Frozen in Ice
float ICEG_PHASE2 = 2;    // Fighting
float ICEG_PHASE3 = 3;    // Busting Columns
float ICEG_PHASE4 = 4;    // Death

//======================================================================
// Global functions
//======================================================================
// Special streamlined player find function
//----------------------------------------------------------------------
float() iceg_FindTarget =
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
void() iceg_WaveSetupHP =
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
// Check if HP reached next wave, turn and fire at targets
//----------------------------------------------------------------------
float() iceg_WaveCheck =
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
        if (self.noise != "") {
            self.attachment2 = find(world, targetname, self.noise);
            if (self.attachment2.classtype == CT_MONICEGOLEMWAVE) {
                // Spawn any adds to keep player busy
                if (self.attachment2.target != "") 
                    trigger_strs(self.attachment2.target, self);
                
                // Turn around and blow up breakables
                self.th_jump();
            }
        }

        // Update Boss wave parameters (next wave!)
        self.bosswave = self.bosswave + 1;
        iceg_WaveSetupHP();        // Reset trigger/hp
        return TRUE;
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check the tether system
//----------------------------------------------------------------------
float() iceg_CheckTether =
{
    self.t_length = vlen(self.origin - self.movelast.origin);
    // Check the most obvious first, inside tether range?
    if (self.t_length < self.tetherrange) return FALSE;
    else {
        // If player or tether close to each other?
        if (infront(self.movelast) && infront(self.enemy) )
            return FALSE;
        // Stop moving around
        else return TRUE;
    }
};
    
//----------------------------------------------------------------------
// Quick check for stomp/melee + fast range attack
//----------------------------------------------------------------------
void() iceg_CheckCombo =
{
    if (self.health < 1) return;
    if (self.enemy.health < 1) return;
    if (skill < SKILL_NORMAL) return;
    
    // If the enemy is visible, quickly go for range attack
    if (visxray(self.enemy, self.attack_offset, self.enemy.view_ofs, TRUE)) {
        self.attack_finished = time + ((4-skill)*0.5) + random();
        // Skip forward in magic attack
        self.think = self.th_charge;
    }
};

//----------------------------------------------------------------------
// Ice Golem game play logic
//----------------------------------------------------------------------
void() IcegCheckAttack = 
{
    //----------------------------------------------------------------------
    // Check Melee range and constantly fire
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_MELEEICEG) {
        self.attack_finished = time + 1;
        self.attack_state = AS_MELEE;
        return;
    }
    // Walk around a bit
    if (time < self.attack_finished) return;

    //----------------------------------------------------------------------
    // Floor stomp attack
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_STOMPICEG && random() < 0.5) {
        self.attack_finished = time + ((4-skill)*0.5) + random() + random();
        self.th_slide ();
        return;
    }
    
    //----------------------------------------------------------------------
    // Range attacks (O Rings + Ceiling Rubble)
    //----------------------------------------------------------------------
    if (!enemy_vis) return;
    
    // Does the monster have a clear shot to the player?
    // sightline can be blocked by other monsters
    if (!visblocked_wide(self.enemy, self.attack_offset, self.enemy.view_ofs)) {
        self.attack_finished = time + ((4-skill)*0.6) + random() + random();
        self.attack_state = AS_MISSILE;
    }
};

//======================================================================
// MONSTER STATES (stand, walk run, attack, pain and death!
//======================================================================
void() iceg_idleframe = 
{
    // Do nothing if dead
    if (self.health < 1) return;
    if (self.walkframe == 0) monster_idle_sound();

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 20) self.walkframe = 0;
    self.frame = $wait1 + self.walkframe;
    self.think = iceg_idleframe;
    // Tick slower for idle
    self.nextthink = time + 0.1;

    // Keep looking for a player
    iceg_FindTarget ();
};

//----------------------------------------------------------------------
void() iceg_stand1 = {self.walkframe = 0; iceg_idleframe();};

//============================================================================
void() iceg_walkframe =
{
    // Do nothing if dead
    if (self.health < 1) return;
    
    // Default walk speed (very slow)
    self.lip = 5;
    // Check for any special frame events
    if (self.walkframe == 0 || self.walkframe == 16) {
        self.lip = 0;
        monster_footstep(FALSE);
    }        
    else if (self.walkframe == 1 || self.walkframe == 17) self.lip = 4;
    else if (self.walkframe == 7 || self.walkframe == 24) 
        monster_idle_sound();

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 33) self.walkframe = 0;
    self.frame = $walk1 + self.walkframe;
    self.think = iceg_walkframe;
    // Hexen 2 models are designed for lower time cycle
    self.nextthink = time + 0.05;
    
    // Keep looking for a player
    iceg_FindTarget ();
    
    // Any feet movement?
    if (self.lip > 0) {
        // Check for tether system (cutom version)
        if (self.movelast) {
            if (iceg_CheckTether()) movetogoal(self.lip); 
        }
        else movetogoal(self.lip);        
    }
};

//----------------------------------------------------------------------
void() iceg_walk1 = { self.walkframe = 0; iceg_walkframe(); };

//============================================================================
void() iceg_runframe =
{
    if (self.style != ICEG_PHASE2) return;
    if (self.health < 1) return;
    
    // Check for boss wave trigger events
    if (iceg_WaveCheck() == TRUE) {self.th_jump(); return;}
    
    // Default run speed (very slow)
    self.lip = 8;
    // Check for any special frame events
    if (self.walkframe == 0 || self.walkframe == 12) {
        self.lip = 0;
        monster_footstep(FALSE);
    }        
    else if (self.walkframe == 1 || self.walkframe == 13) self.lip = 6;
    else if (self.walkframe == 6 || self.walkframe == 17) 
        monster_idle_sound();

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 23) self.walkframe = 0;
    self.frame = $run1 + self.walkframe;
    self.think = iceg_runframe;
    // Hexen 2 models are designed for lower time cycle
    self.nextthink = time + 0.05;
    
    // Check for tether system (cutom version)
    if (self.movelast) {
        if (iceg_CheckTether()) ai_run(0);
        else ai_run(self.lip);
    }
    else ai_run(self.lip);
};

//----------------------------------------------------------------------
void() iceg_run1 = { self.walkframe = 0; iceg_runframe(); };

//============================================================================
// MELEE - knockback the player with swipe
//============================================================================
void(float dist) iceg_mcharge = { 
    ai_charge(dist);
    self.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() iceg_knock1 = [ $knock1, iceg_knock2 ] {iceg_mcharge(5);};
void() iceg_knock2 = [ $knock2, iceg_knock3 ] {iceg_mcharge(6);};
void() iceg_knock3 = [ $knock3, iceg_knock4 ] {iceg_mcharge(7);};
void() iceg_knock4 = [ $knock4, iceg_knock5 ] {iceg_mcharge(8);};
void() iceg_knock5 = [ $knock5, iceg_knock6 ] {iceg_mcharge(9);};
void() iceg_knock6 = [ $knock7, iceg_knock7 ] {iceg_mcharge(10);};
void() iceg_knock7 = [ $knock6, iceg_knock8 ] {iceg_mcharge(9);};
void() iceg_knock8 = [ $knock8, iceg_knock9 ] {iceg_mcharge(8);};
void() iceg_knock9 = [ $knock9, iceg_knock10 ] {iceg_mcharge(7);};
void() iceg_knock10 = [ $knock10, iceg_knock11 ] {iceg_mcharge(6);};
void() iceg_knock11 = [ $knock11, iceg_knock12 ] {iceg_mcharge(5);monster_footstep(FALSE);};
void() iceg_knock12 = [ $knock12, iceg_knock13 ] {iceg_mcharge(5);
    sound (self, CHAN_VOICE, "golem/melee_swipe.wav", 1, ATTN_IDLE);};
void() iceg_knock13 = [ $knock13, iceg_knock14 ] {iceg_mcharge(4);};
void() iceg_knock14 = [ $knock14, iceg_knock15 ] {iceg_mcharge(3);};
void() iceg_knock15 = [ $knock15, iceg_knock16 ] {iceg_mcharge(2);};
void() iceg_knock16 = [ $knock16, iceg_knock17 ] {iceg_mcharge(1);
    self.meleehitsound = "golem/melee_punch.wav";
    self.meleecontact = TRUE;ai_meleesmash(30);
    // If punch made contact, lift the enemy off ground
    if (self.meleecontact == FALSE) {
        self.pos1 = self.enemy.origin - self.origin;
        self.pos1_z = 0;
        self.pos1 = normalize(self.pos1);
        self.enemy.velocity = self.enemy.velocity + self.pos1 * 400;
        self.enemy.velocity_z = self.enemy.velocity_z + 250;
        self.enemy.flags = self.enemy.flags - (self.enemy.flags & FL_ONGROUND);
    }
    self.meleecontact = FALSE;
};
void() iceg_knock17 = [ $knock17, iceg_knock18 ] {iceg_mcharge(1);};
void() iceg_knock18 = [ $knock18, iceg_knock19 ] {iceg_mcharge(2);};
void() iceg_knock19 = [ $knock19, iceg_knock20 ] {iceg_mcharge(3);};
void() iceg_knock20 = [ $knock20, iceg_knock21 ] {iceg_mcharge(4);};
void() iceg_knock21 = [ $knock21, iceg_knock22 ] {iceg_mcharge(5);};
void() iceg_knock22 = [ $knock22, iceg_knock23 ] {iceg_mcharge(5);};
void() iceg_knock23 = [ $knock23, iceg_knock24 ] {iceg_mcharge(5);};
void() iceg_knock24 = [ $knock24, iceg_run1 ] {iceg_mcharge(5); 
    // Check for combo Melee+quick range
    iceg_CheckCombo();
};

//============================================================================
// RANGE ATTACK (ICE SHARDS == rockets)
// Total shards to fire is based on skill level
// easy=3, normal=3, hard=4, nm=5
//============================================================================
void() iceg_shardanimate =
{
    // manually animate
    self.frame = self.frame + 1;
    self.lip = 1 - (self.frame / 11);
    self.lip = self.lip * 0.5;
    // Gradualy fade out
    self.alpha = self.lip;
    // Hit animaton frame boundary?
    if (self.frame > 10) entity_remove(self, 0.1);
    else self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() iceg_shardtrail =
{
    local entity oring;
    
    // Spawn a temporary entity
    oring = spawn();
    oring.owner = self;
    oring.solid = SOLID_NOT;            // No interaction with world
    oring.movetype = MOVETYPE_NONE;        // Static item, no movement
    
    // move to current projectile location
    setmodel(oring, MODEL_PROJ_RINGBLAST);
    setsize(oring, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(oring, self.origin);
    oring.alpha = 0.5;
    oring.skin = 3;
    
    // Match orientiation and start Oring animation
    self.pos2 = normalize(self.velocity);
    oring.angles = vectoangles(self.pos2);
    oring.think = iceg_shardanimate;
    oring.nextthink = time + 0.1;

    // Has the DP particle trail been setup yet?
    if (!self.pain_finished) {
        // only run this once
        self.pain_finished = TRUE;
        // Re-use plasma trail
        if (ext_dppart) {
            self.traileffectnum = particleeffectnum(DPP_TRPLASMA);
            self.effects = 0;
        }
    }
    else {
        // Classic (fitz/qs) engine particles only
        if (!ext_dppart) {
            self.oldorigin = crandom() * '1 1 1';
            // Silver colour
            self.lip = 40 + rint(random()*8);
            particle (self.origin, self.oldorigin, self.lip, 8 + rint(random()*8));
            // Yellow colour
            self.lip = 198 + rint(random()*4);
            particle (self.origin, self.oldorigin, self.lip, 4 + rint(random()*4));
        }
    }
    
    // Keep looping for O rings trails
    self.think = iceg_shardtrail;
    self.nextthink = time + 0.05 + random()*0.05;
};

//----------------------------------------------------------------------
void(entity targ) iceg_icestorm = 
{
    local float zpart;
    local vector org, targ_org, dir, yrand, zrand;

    // No enemy or dead? stop firing
    if (!targ) return;
    if (self.health < 1) return;

    // Play random firing sound
    self.lip = random();
    if (self.lip < 0.3) 
        sound (self, CHAN_WEAPON, "golem/iceshard1.wav", 1, ATTN_IDLE);
    else if (self.lip  < 0.6)
        sound (self, CHAN_WEAPON, "golem/iceshard2.wav", 1, ATTN_IDLE);
    else
        sound (self, CHAN_WEAPON, "golem/iceshard3.wav", 1, ATTN_IDLE);
    
    // Make sure facing the right direction
    ai_face();
    makevectors(self.angles);

    // Check if enemy is bmodel or point entity
    if (targ.bsporigin) targ_org = bmodel_origin(targ);
    else targ_org = targ.origin;

        // Spawn a mist of particles where projectiles came from
    org = self.origin + attack_vector('20 0 64');
    zpart = 20 + rint(random()*20);
    particle_explode(org, zpart, 1+random(), PARTICLE_BURST_BLUE, PARTICLE_BURST_UPWARD);

    // Randomly spawn projectiles from golems chest
    yrand = (crandom()*10)*v_right;
    zrand = (crandom()*10)*v_up;
    org = self.origin + attack_vector(self.attack_offset) + yrand + zrand;

    // Add some right/left bias to the final destination
    dir = (targ_org - org) + (v_right*(crandom()*50));
    dir = normalize (dir);

    // Variable speed based on skill level
    self.attack_speed = SPEED_ICEGSHARD + (skill * SPEED_ICEGSKILL);
    Launch_Missile (org, dir, '0 0 0', CT_PROJ_ICEG1, self.attack_speed);
};

//----------------------------------------------------------------------
void() iceg_magtime = {self.nextthink = time + 0.05;};

void() iceg_magic1 = [ $magic1, iceg_magic2 ] {iceg_magtime();ai_face();
    self.pain_finished = time + 1.5;
    sound (self, CHAN_WEAPON, "golem/icestorm.wav", 1, ATTN_NORM);
};
void() iceg_magic2 = [ $magic2, iceg_magic3 ] {iceg_magtime();};
void() iceg_magic3 = [ $magic3, iceg_magic4 ] {iceg_magtime();ai_face();};
void() iceg_magic4 = [ $magic4, iceg_magic5 ] {iceg_magtime();};
void() iceg_magic5 = [ $magic5, iceg_magic6 ] {iceg_magtime();ai_face();};
void() iceg_magic6 = [ $magic6, iceg_magic7 ] {iceg_magtime();ai_face();};
void() iceg_magic7 = [ $magic7, iceg_magic8 ] {iceg_magtime();ai_face();};
void() iceg_magic8 = [ $magic8, iceg_magic9 ] {iceg_magtime();};
void() iceg_magic9 = [ $magic9, iceg_magic10 ] {iceg_magtime();ai_face();};
void() iceg_magic10 = [ $magic10, iceg_magic11 ] {iceg_magtime();ai_face();};
void() iceg_magic11 = [ $magic11, iceg_magic12 ] {iceg_magtime();ai_face();};
void() iceg_magic12 = [ $magic12, iceg_magic13 ] {iceg_magtime();ai_face();};
void() iceg_magic13 = [ $magic13, iceg_magic14 ] {iceg_magtime();iceg_icestorm(self.enemy);};
void() iceg_magic14 = [ $magic14, iceg_magic15 ] {iceg_magtime();ai_face();};
void() iceg_magic15 = [ $magic15, iceg_magic16 ] {iceg_magtime();iceg_icestorm(self.enemy);};
void() iceg_magic16 = [ $magic16, iceg_magic17 ] {iceg_magtime();ai_face();};
void() iceg_magic17 = [ $magic17, iceg_magic18 ] {iceg_magtime();iceg_icestorm(self.enemy);};
void() iceg_magic18 = [ $magic18, iceg_magic19 ] {iceg_magtime();ai_face();};
void() iceg_magic19 = [ $magic19, iceg_magic20 ] {iceg_magtime();ai_face();
    if (skill > SKILL_NORMAL) iceg_icestorm(self.enemy);
};
void() iceg_magic20 = [ $magic20, iceg_magic21 ] {iceg_magtime();ai_face();};
void() iceg_magic21 = [ $magic21, iceg_magic22 ] {iceg_magtime();ai_face();
    if (skill > SKILL_HARD) iceg_icestorm(self.enemy);
};
void() iceg_magic22 = [ $magic22, iceg_magic23 ] {iceg_magtime();};
void() iceg_magic23 = [ $magic23, iceg_magic24 ] {iceg_magtime();};
void() iceg_magic24 = [ $magic24, iceg_run1 ] {iceg_magtime();};

//============================================================================
// RANGE ATTACK (Stomp ground and drop debris from ceiling)
//============================================================================
void() iceg_ceilingrubble = 
{
    // No enemy or dead?
    if (!self.enemy) return;
    if (self.health < 1) return;
    
    // Trace a line up from the enemy/player
    traceline(self.enemy.origin, self.enemy.origin+'0 0 1024', TRUE, self.enemy);
    self.pos2 = trace_endpos;
    // is there any space for rubble?
    if (vlen(self.enemy.origin - self.pos2) < 64) return;

    // Spawn a func_breakable_spawner entity
    // Manually setup all parameters
    // All sounds/models pre-cached with ice golem
    self.attachment = spawn();
    // Move the emitter slightly down from ceiling
    self.attachment.origin = trace_endpos - self.dest2;
    
    self.attachment.owner = self;
    self.attachment.style = BTYPE_ROCK;
    self.attachment.brkimpsound = BTYPE_ROCK;
    self.attachment.brkobjects = BTYPE_ROCK;
    self.attachment.brkvelbase = self.brkvelbase;
    self.attachment.brkveladd = self.brkveladd;
    self.attachment.brkavel = 200;
    self.attachment.brkfade = self.brkfade;
    self.attachment.noise = SOUND_BRK_ROCK;
    self.attachment.noise1 = SOUND_IMP_ROCK1;
    self.attachment.noise2 = SOUND_IMP_ROCK2;
    self.attachment.brkimpqty = 2;
    
    // Allow for custom rubble bmodels
    self.attachment.brkobj1 = self.brkobj1;
    self.attachment.brkobj2 = self.brkobj2;
    self.attachment.brkobjqty = 2;
    self.attachment.pos1 = self.dest1;
    self.attachment.cnt = 5 + random() * 5;
    self.attachment.count = 5 + random() * 5;
    self.attachment.movedir = '0 -2 0';
    self.attachment.angles = '0 0 0';
    
    // Check if the boss has dmg key set for rubble damage
    if (self.dmg > 0) self.attachment.spawnflags = BREAK_MOVEDIR | BREAK_DAMAGE;
    else self.attachment.spawnflags = BREAK_MOVEDIR;
    if (self.dmg > 0) self.attachment.dmg = self.dmg;
    else self.attachment.dmg = 2;
    
    // Default entity stuff (type/movement etc)
    self.attachment.classtype = CT_FUNCBREAKSPN;
    self.attachment.classgroup = CG_BREAKABLE;
    self.attachment.solid = SOLID_NOT;
    self.attachment.movetype = MOVETYPE_NONE;
    self.attachment.brkvol = self.brkvol;
    self.attachment.think = funcbreakable_use;
    self.attachment.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() iceg_footstomp = {self.nextthink = time + 0.05;};

//----------------------------------------------------------------------
void() iceg_stomp1 = [ $stomp1, iceg_stomp2 ] {iceg_footstomp();
    self.pain_finished = time + self.pain_timeout;};
void() iceg_stomp2 = [ $stomp2, iceg_stomp3 ] {iceg_footstomp();
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_IDLE);};
void() iceg_stomp3 = [ $stomp3, iceg_stomp4 ] {iceg_footstomp();};
void() iceg_stomp4 = [ $stomp4, iceg_stomp5 ] {iceg_footstomp();};
void() iceg_stomp5 = [ $stomp5, iceg_stomp6 ] {iceg_footstomp();monster_idle_sound();};
void() iceg_stomp6 = [ $stomp6, iceg_stomp7 ] {iceg_footstomp();};
void() iceg_stomp7 = [ $stomp7, iceg_stomp8 ] {iceg_footstomp();};
void() iceg_stomp8 = [ $stomp8, iceg_stomp9 ] {iceg_footstomp();};
void() iceg_stomp9 = [ $stomp9, iceg_stomp10 ] {iceg_footstomp();};
void() iceg_stomp10 = [ $stomp10, iceg_stomp11 ] {iceg_footstomp();};
void() iceg_stomp11 = [ $stomp11, iceg_stomp12 ] {iceg_footstomp();};
void() iceg_stomp12 = [ $stomp12, iceg_stomp13 ] {iceg_footstomp();};
void() iceg_stomp13 = [ $stomp13, iceg_stomp14 ] {
    iceg_footstomp(); self.meleehitsound = "golem/groundslam.wav";
    ai_shockwave(MONAI_STOMPIMPACT, MONAI_STOMPDAMAGE, MONAI_STOMPICERADIUS, MONAI_STOMPFORWARD, MONAI_STOMPUP);
    // spawn ceiling rubble above player/enemy
    iceg_ceilingrubble();
};
void() iceg_stomp14 = [ $stomp14, iceg_stomp15 ] {iceg_footstomp();};
void() iceg_stomp15 = [ $stomp15, iceg_stomp16 ] {iceg_footstomp();};
void() iceg_stomp16 = [ $stomp16, iceg_stomp17 ] {iceg_footstomp();monster_idle_sound();};
void() iceg_stomp17 = [ $stomp17, iceg_stomp18 ] {iceg_footstomp();};
void() iceg_stomp18 = [ $stomp18, iceg_stomp19 ] {iceg_footstomp();};
void() iceg_stomp19 = [ $stomp19, iceg_stomp20 ] {iceg_footstomp();};
void() iceg_stomp20 = [ $stomp20, iceg_stomp21 ] {iceg_footstomp();};
void() iceg_stomp21 = [ $stomp21, iceg_stomp22 ] {iceg_footstomp();};
void() iceg_stomp22 = [ $stomp22, iceg_stomp23 ] {iceg_footstomp();};
void() iceg_stomp23 = [ $stomp23, iceg_stomp24 ] {iceg_footstomp();};
void() iceg_stomp24 = [ $stomp24, iceg_run1 ] {iceg_footstomp();
    monster_footstep(FALSE);
    // Check for combo Stomp+quick range
    iceg_CheckCombo();
};

//============================================================================
// WAVE BOUNDARY - destroy some objects and spawn adds
//============================================================================
void() iceg_aiturn =
{
    // turn towards a target if one exists
    if (self.enemytarget) {
        // Check if enemy target is bmodel or point entity
        if (self.enemytarget.bsporigin)
            self.pos1 = bmodel_origin(self.enemytarget);
        else self.pos1 = self.enemytarget.origin;
        // Force turn towards new target
        self.ideal_yaw = vectoyaw(self.pos1 - self.origin);
        ChangeYaw ();

        // Check angle difference
        if ((self.ideal_yaw - 10) > self.angles_y) self.attack_elev = TRUE;
        else if ((self.ideal_yaw + 10)<self.angles_y) self.attack_elev = TRUE;
        else self.attack_elev = FALSE;
    }
};

//----------------------------------------------------------------------
void() iceg_waveface = {
    // Hexen 2 quicktime!
    self.nextthink = time + 0.05;
    iceg_aiturn();
};

//----------------------------------------------------------------------
void() iceg_waveb1 = [ $magic1, iceg_waveb2 ] {iceg_waveface();};
void() iceg_waveb2 = [ $magic2, iceg_waveb3 ] {iceg_waveface();};
void() iceg_waveb3 = [ $magic3, iceg_waveb4 ] {iceg_waveface();};
void() iceg_waveb4 = [ $magic4, iceg_waveb5 ] {iceg_waveface();};
void() iceg_waveb5 = [ $magic5, iceg_waveb6 ] {iceg_waveface();};
void() iceg_waveb6 = [ $magic6, iceg_waveb7 ] {iceg_waveface();};
void() iceg_waveb7 = [ $magic7, iceg_waveb8 ] {iceg_waveface();};
void() iceg_waveb8 = [ $magic8, iceg_waveb9 ] {iceg_waveface();};
void() iceg_waveb9 = [ $magic9, iceg_waveb10 ] {iceg_waveface();};

// Fire at first target
void() iceg_waveb10 = [ $magic10, iceg_waveb11 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb11 = [ $magic11, iceg_waveb12 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb12 = [ $magic12, iceg_waveb13 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb13 = [ $magic13, iceg_waveb14 ] {iceg_waveface();
    if (self.enemytarget) {
        iceg_icestorm(self.enemytarget);
        // Trigger the target entity
        trigger_ent(self.enemytarget, self);
        // Find next target and turn towards it
        self.enemytarget = find(world, targetname, self.attachment2.noise2);
    }
};

// Start turning towards next target
void() iceg_waveb14 = [ $magic14, iceg_waveb15 ] {iceg_waveface();};
void() iceg_waveb15 = [ $magic15, iceg_waveb16 ] {iceg_waveface();};
void() iceg_waveb16 = [ $magic16, iceg_waveb17 ] {iceg_waveface();};

// Fire at second target
void() iceg_waveb17 = [ $magic17, iceg_waveb18 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb18 = [ $magic18, iceg_waveb19 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb19 = [ $magic19, iceg_waveb20 ] {iceg_waveface();
    if (self.enemytarget) iceg_icestorm(self.enemytarget);};
void() iceg_waveb20 = [ $magic20, iceg_waveb21 ] {iceg_waveface();
    if (self.enemytarget) {
        iceg_icestorm(self.enemytarget);
        // Trigger the target entity
        trigger_ent(self.enemytarget, self);
    }    
};

// Stop turning
void() iceg_waveb21 = [ $magic21, iceg_waveb22 ] {iceg_waveface();};
void() iceg_waveb22 = [ $magic22, iceg_waveb23 ] {iceg_waveface();};
void() iceg_waveb23 = [ $magic23, iceg_waveb24 ] {iceg_waveface();};
void() iceg_waveb24 = [ $magic24, iceg_run1 ] {
    self.style = ICEG_PHASE2;
    Resist_ChangeType(self, FALSE);
    iceg_waveface();
    // Pause before next attack
    self.pain_finished = self.attack_finished = time + 2;
};

//----------------------------------------------------------------------
void() iceg_turnframe =
{
    // Do nothing if dead
    if (self.health < 1) return;
    
    // Check for any special frame events
    if (self.walkframe == 0 || self.walkframe == 16) monster_footstep(FALSE);

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 33) self.walkframe = 0;
    self.frame = $walk1 + self.walkframe;
    // Hexen 2 models are designed for lower time cycle
    self.nextthink = time + 0.05;
    
    // Facing the first target?
    iceg_aiturn();
    if (self.attack_elev == FALSE) self.think = iceg_waveb1;
    else self.think = iceg_turnframe;
};

//----------------------------------------------------------------------
void() iceg_waveboundary = 
{
    // Find a target to aim at (columns to blow up)
    self.enemytarget = find(world, targetname, self.attachment2.noise1);
    // First target seems to be dead
    if (!self.enemytarget) {
        self.enemytarget = find(world, targetname, self.attachment2.noise2);
        // Second target is missing, try current enemy
        if (!self.enemytarget) self.enemytarget = self.enemy;
    }
    
    // If nothing to shoot at, back to running!
    if (!self.enemytarget) {
        // Back to running!
        self.think = self.th_run;
        self.nextthink = time + 0.1;
    }
    else {
        // Time to blow stuff up, switch on resistance and roar!
        self.style = ICEG_PHASE3;
        Resist_ChangeType(self, TRUE);
        sound (self, CHAN_WEAPON, "golem/icestorm.wav", 1, ATTN_NORM);
        // Start turning towards target (using walk animation)
        self.walkframe = 0;
        iceg_turnframe();
    }
};

//============================================================================
// PAIN (no pain animations)
//============================================================================
void(entity inflictor, entity attacker, float damage) iceg_pain =
{
    // Check for boss wave trigger events
    if (iceg_WaveCheck() == TRUE) {self.th_jump(); return;}

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
    }
};

//============================================================================
// DEATH : Time to melt the heavy water GOLEM!
//============================================================================
void() iceg_fadeout =
{
    self.origin_z = self.origin_z - 0.2;    // Slowly sink into the ground (melting)
    self.alpha = 1 - ((time - self.ltime) / 1);
    if (random() < 0.1) SpawnProjectileSmoke(self.origin, 200, 100, 100);
    if (self.alpha <= 0) entity_hide(self);
    else self.nextthink = time + FADEMODEL_TIME;
};

//----------------------------------------------------------------------
void() iceg_startfade =
{
    // Check for any final trigger events
    if (self.message2 != "") trigger_strs(self.message2,self);

    // Setup ready for fade away ice->water
    self.frame = $death22;
    self.alpha = 0.99; 
    self.ltime = time + 2;
    self.think = iceg_fadeout;
    iceg_fadeout();
};

//----------------------------------------------------------------------
void(float fdist) iceg_brk =
{
    if (fdist > 0) ai_forward(fdist);
    if (random() < 0.2) SpawnProjectileSmoke(self.origin, 200, 50, 150);
    if (random() < 0.4) SpawnProjectileSmoke(self.origin, 300, 50, 150);
    if (random() < 0.6) SpawnProjectileSmoke(self.origin, 200, 50, 250);
    if (random() < 0.3) ThrowGib(11, 1);    // Small Golem rock
};

//----------------------------------------------------------------------
void() iceg_death1    = [ $death1, iceg_death2 ] {iceg_brk(0);};
void() iceg_death2    = [ $death2, iceg_death3 ] {iceg_brk(0);};
void() iceg_death3    = [ $death3, iceg_death4 ] {iceg_brk(2);};
void() iceg_death4    = [ $death4, iceg_death5 ] {iceg_brk(5);};
void() iceg_death5    = [ $death5, iceg_death6 ] {iceg_brk(5);};
void() iceg_death6    = [ $death6, iceg_death7 ] {iceg_brk(10);};
void() iceg_death7    = [ $death7, iceg_death8 ] {iceg_brk(12);monster_footstep(FALSE);};
void() iceg_death8    = [ $death8, iceg_death9 ] {iceg_brk(10);};
void() iceg_death9    = [ $death9, iceg_death10] {iceg_brk(0);};
void() iceg_death10 = [ $death10, iceg_death11] {iceg_brk(0);};
void() iceg_death11 = [ $death11, iceg_death12] {iceg_brk(0);};
void() iceg_death12 = [ $death12, iceg_death13] {iceg_brk(0);};
void() iceg_death13 = [ $death13, iceg_death14] {iceg_brk(0);};
void() iceg_death14 = [ $death14, iceg_death15] {iceg_brk(0);};
void() iceg_death15 = [ $death15, iceg_death16] {iceg_brk(4);};
void() iceg_death16 = [ $death16, iceg_death17] {iceg_brk(4);};
void() iceg_death17 = [ $death17, iceg_death18] {iceg_brk(4);};
void() iceg_death18 = [ $death18, iceg_death19] {iceg_brk(4);};
void() iceg_death19 = [ $death19, iceg_death20] {iceg_brk(4);};
void() iceg_death20 = [ $death20, iceg_death21] {iceg_brk(4);};
void() iceg_death21 = [ $death21, iceg_death22] {iceg_brk(4);};
void() iceg_death22 = [ $death22, iceg_startfade] {
    self.nextthink = time + 4 + random()*4;
    self.velocity = '0 0 0';
    self.movetype = MOVETYPE_NONE;
};

//----------------------------------------------------------------------
void() iceg_die =
{
    self.deadflag = DEAD_DEAD;    // Nour bites the dust
    self.effects = 0;            // Remove effects on death
    self.style = ICEG_PHASE4;    // Heavy Water
    self.max_health = MON_GIBEXPLOSION;

    sound (self, CHAN_BODY, "golem/death.wav", 1, ATTN_NORM);
    self.solid = SOLID_NOT;
    iceg_death1 ();
};

//============================================================================
// Wakeup From Statue form
//============================================================================
void() iceg_birth = {
    if (random() < 0.4) SpawnProjectileSmoke(self.origin, 200, 100, 100);
    if (random() < 0.8) SpawnProjectileSmoke(self.origin, 200, 50, 250);
    if (random() < 0.1) ThrowGib(11, 1);    // Small Golem rock
    self.nextthink = time + 0.05;
};

//----------------------------------------------------------------------
void() iceg_wake1 =[ $birth1, iceg_wake2 ] {
    self.use = SUB_Null;                // No more triggers
    self.style = ICEG_PHASE2;            // Free to fight
    self.yaw_speed = 20 + (skill * 4);    // Really Fast Speed

    // Setup player focus
    if (activator.flags & FL_CLIENT) {
        self.goalentity = self.enemy = activator;
    }
    else self.enemy = world;
    
    // Restore all think functions
    self.th_stand = iceg_stand1;
    self.th_walk = iceg_walk1;
    self.th_run = iceg_run1;
    self.th_melee = iceg_knock1;
    self.th_missile = iceg_magic1;
    self.th_updmissile = iceg_shardtrail;
    self.th_charge = iceg_magic7;
    self.th_slide = iceg_stomp1;
    self.th_jump = iceg_waveboundary;
    self.th_pain = iceg_pain;
    self.th_die = iceg_die;
    
    self.pain_finished = time + 3;        // Make pain go away
    self.attack_finished = time + 2;    // Reset attack system

    iceg_birth();
    sound (self, CHAN_BODY, "golem/wakestatue.wav", 1, ATTN_NORM);
};
void() iceg_wake2 =[ $birth2, iceg_wake3 ] {iceg_birth();ai_forward(2);};
void() iceg_wake3 =[ $birth3, iceg_wake4 ] {iceg_birth();ai_forward(3);};
void() iceg_wake4 =[ $birth4, iceg_wake5 ] {iceg_birth();ai_forward(4);};
void() iceg_wake5 =[ $birth5, iceg_wake6 ] {iceg_birth();ai_forward(3);};
void() iceg_wake6 =[ $birth6, iceg_wake7 ] {iceg_birth();ai_forward(3);};
void() iceg_wake7 =[ $birth7, iceg_wake8 ] {iceg_birth();ai_forward(1);};
void() iceg_wake8 =[ $birth8, iceg_wake9 ] {iceg_birth();monster_footstep(FALSE);};
void() iceg_wake9 =[ $birth9, iceg_wake10 ] {iceg_birth();};
void() iceg_wake10 =[ $birth10, iceg_wake11 ] {iceg_birth();};
void() iceg_wake11 =[ $birth11, iceg_wake12 ] {iceg_birth();monster_idle_sound();};
void() iceg_wake12 =[ $birth12, iceg_wake13 ] {iceg_birth();};
void() iceg_wake13 =[ $birth13, iceg_wake14 ] {iceg_birth();};
void() iceg_wake14 =[ $birth14, iceg_wake15 ] {iceg_birth();};
void() iceg_wake15 =[ $birth15, iceg_wake16 ] {iceg_birth();};
void() iceg_wake16 =[ $birth16, iceg_tranB1 ] {iceg_birth();};

void() iceg_tranB1 = [ $tranB1, iceg_tranB2 ] {
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);
    iceg_birth();
    ai_forward(8);
};
void() iceg_tranB2 = [ $tranB2, iceg_tranB3 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB3 = [ $tranB3, iceg_tranB4 ] {iceg_birth();ai_forward(5);monster_idle_sound();};
void() iceg_tranB4 = [ $tranB4, iceg_tranB5 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB5 = [ $tranB5, iceg_tranB6 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB6 = [ $tranB6, iceg_tranB7 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB7 = [ $tranB7, iceg_tranB8 ] {iceg_birth();ai_forward(5);monster_idle_sound();};
void() iceg_tranB8 = [ $tranB8, iceg_tranB9 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB9 = [ $tranB9, iceg_tranB10 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB10 = [ $tranB10, iceg_tranB11 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB11 = [ $tranB11, iceg_tranB12 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB12 = [ $tranB12, iceg_tranB13 ] {iceg_birth();ai_forward(5);};
void() iceg_tranB13 = [ $tranB13, iceg_run1 ] {
    monster_footstep(FALSE);
    self.takedamage = DAMAGE_YES;    // Can take damage
    self.style = ICEG_PHASE2;        // Time to fight!
    Resist_ChangeType(self,FALSE);    // restore resistance
    self.walkframe = 13;            // Mid way through set
    iceg_runframe();
};

//======================================================================
// Setup Ice Golem after trigger event
//======================================================================
void() iceg_setup =
{
    self.use = SUB_Null;        // No more triggers
    self.style = ICEG_PHASE1;    // Bursting intro sequence
    self.flags = FL_MONSTER;    // Reset flag

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    self.velocity = '0 0 0';        // Make sure stationary
    self.deadflag = DEAD_NO;        // used to stop death re-triggering
    self.no_liquiddmg = TRUE;        // no slime/lava damage
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.pain_longanim = FALSE;        // No axe advantage
    self.takedamage = DAMAGE_NO;    // No damage, statue time
    self.noinfighting = TRUE;        // No infighting
    self.skin = 1;                    // Frozen skin

    self.blockudeath = TRUE;        // No humanoid death
    self.bleedcolour = 16;            // Brown dust/dirt
    self.meleerange = MONAI_MELEEGOLEM;
    self.attack_offset = '10 0 112';    // Ice storm

    self.gibhealth = -1000;            // Special death sequence
    self.gibbed = FALSE;            // Still in one piece
    self.pain_finished = self.attack_finished = LARGE_TIMER;
    
    // Setup Ammo Resistance
    self.resist_shells = self.resist_cells = 0;
    self.resist_nails = self.resist_rockets = 0.75;
    self.reflectnails = TRUE;        // Reflect nail projectiles
    
    //Resist_CheckRange(self);        // Double check values
    Resist_Save(self);                // Save for Later
    Resist_ChangeType(self,TRUE);    // resist everything

    // Setup boss waves and overall health
    self.bosswave = 1;
    self.bosswavetotal = 5;
    self.bosswaveqty = 500;
    self.max_health = self.bosswavetotal * self.bosswaveqty;
    // Setup boss wave HP + trigger event
    iceg_WaveSetupHP();

    // default = No think functions
    self.th_checkattack = IcegCheckAttack;
    self.th_stand = self.th_walk = self.th_run = SUB_Null;
    self.th_missile = self.th_die = SUB_Null;
    self.th_pain = SUB_Null_pain;

    // Check there is space for Ice Golem to spawn
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

    // Find the tether marker (must be path_corner)
    if (self.tethertarget != "") {
        self.movelast = find(world,targetname,self.tethertarget);
        if (self.movelast.classtype != CT_PATHCORNER) {
            dprint("\b[ICEG]\b Tether marker not path_corner!\n");
            spawn_marker(self.movelast.origin, SPNMARK_YELLOW);
            self.movelast = world;
        }
        else spawn_marker(self.movelast.origin, SPNMARK_GREEN);
        // Setup default tetherrange
        if (!self.tetherrange) self.tetherrange = MONAI_MAXICEG;
    }
    // Make sure tethertarget is blank
    self.tethertarget = "";

    // Make sure all death triggers are setup ready
    self.message2 = self.target;
    self.target = self.target2 = self.deathtarget = "";

    // Wait for trigger event to be wakeup
    self.use = iceg_wake1;
};

/*======================================================================
QUAKED monster_icegolem (1 0.2 0) (-32 -32 -24) (32 32 128)
======================================================================*/
void() monster_icegolem =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_bossicegolem.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_RINGSHOCK);    // Ground shockwave
    precache_model (MODEL_PROJ_RINGBLAST);    // Ice ring shockwave
    precache_model (MODEL_PROJ_GROCK1);        // Golem rocks
    precache_model (MODEL_PROJ_GROCK2);
    precache_model (MODEL_PROJ_GSHARD);        // Ice shards
    
    self.gib1mdl = MODEL_PROJ_GROCK1;    // small golem rock
    self.gib2mdl = MODEL_PROJ_GROCK2;    // medium golem rock
    self.gib1frame = self.gib2frame = 9;

    // Idle1a-1b (long sound) idle2a-2b (short sound)
    self.idle_sound = "golem/idle1a.wav";
    self.idle_sound2 = "golem/idle1b.wav";
    self.idle_soundcom = "golem/idle2a.wav";
    self.idle_soundcom2 = "golem/idle2b.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);
    precache_sound (self.idle_soundcom2);
    
    // Break free of statue pose
    precache_sound ("golem/wakestatue.wav");
    
    // Several melee smack sounds and giant ground slam
    precache_sound ("golem/melee_swipe.wav");
    precache_sound ("golem/melee_punch.wav");
    precache_sound ("golem/melee_pound.wav");
    precache_sound ("golem/groundslam.wav");

    // Used for ceiling rubble breakables
    precache_sound (SOUND_BRK_ROCK);
    precache_sound (SOUND_IMP_ROCK1);
    precache_sound (SOUND_IMP_ROCK2);
    if (self.brkobj1 == "") self.brkobj1 = MODEL_BRK_ROCK1A;
    precache_model (self.brkobj1);
    if (self.brkobj2 == "") self.brkobj2 = MODEL_BRK_ROCK1B;
    precache_model (self.brkobj2);
    
    // Additional rubble emitter parameters
    if (CheckZeroVector(self.dest1)) self.dest1 = '80 8 1';
    if (CheckZeroVector(self.dest2)) self.dest2 = '0 0 16';
    if (CheckZeroVector(self.brkvol)) self.brkvol = '160 160 8';
    if (CheckZeroVector(self.brkvelbase)) self.brkvelbase = '0 0 50';
    if (CheckZeroVector(self.brkveladd)) self.brkveladd = '100 100 100';
    if (self.brkfade <= 0) self.brkfade = 4;
    
    // Ice Storm - range attack
    precache_sound ("golem/icestorm.wav");
    precache_sound ("golem/iceshard1.wav");
    precache_sound ("golem/iceshard2.wav");
    precache_sound ("golem/iceshard3.wav");
    precache_sound ("golem/iceshard_impact.wav");
    precache_model (SEXP_ICE_BIG);        // Shard impact (ne_ruin)
    
    self.pain_sound = "golem/pain1.wav";
    self.pain_sound2 = "golem/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    precache_sound ("golem/death.wav");

    // Roar!
    self.sight_sound = "golem/sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.bbmins = '-32 -32 -24';    // Special Size
    self.bbmaxs = '32 32 128';
    self.bboxtype = BBOX_CUSTOM;
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)
    self.poisonous = FALSE;            // Cannot be poisonous
    self.deathstring = " was shattered by the Ice Golem\n";

    self.steptype = FS_TYPEGIANT;    // Giant sound
    self.health = self.max_health = MEGADEATH;
    
    self.classtype = CT_MONICEGOLEM;
    self.classgroup = CG_STONE;
    self.classmove = MON_MOVEWALK;
    self.style = 0;
    
    // No targetname = no trigger!
    if (self.targetname == "") {
        dprint("\b[ICEGOLEM]\b Missing targetname name!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }

    total_monsters = total_monsters + 1;
    self.use = iceg_setup;
};

/*======================================================================
/*QUAKED monster_icegolem_wavetrig (0.75 0.25 1) (-32 -32 -8) (32 32 8) x
Target for boss column smash wave event
-------- KEYS --------
targetname : name of wave event (links to noise key on boss)
target     : trigger name of adds to spawn
noise1     : trigger name of column 1
noise2     : trigger name of column 2
-------- SPAWNFLAGS --------
-------- NOTES --------
Target for boss column smash wave event

======================================================================*/
void() monster_icegolem_wavetrig = 
{
    self.classtype = CT_MONICEGOLEMWAVE;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.takedamage = DAMAGE_NO;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
};
