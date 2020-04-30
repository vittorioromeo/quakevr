/*==============================================================================
Eidolon Demon from Hexen2 / Altar of Storms MOD
==============================================================================*/
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame walk17 walk18 walk19 walk20 walk21 walk22 walk23 walk24

$frame spell1 spell2 spell3 spell4 spell5 spell6 spell7 spell8
$frame spell9 spell10 spell11 spell12 spell13 spell14 spell15 spell16
$frame spell17 spell18 spell19 spell20

$frame howl1 howl2 howl3 howl4 howl5 howl6 howl7 howl8
$frame howl9 howl10 howl11 howl12 howl13 howl14 howl15 howl16
$frame howl17 howl18 howl19 howl20 howl21 howl22 howl23 howl24
$frame howl25 howl26 howl27 howl28

$frame crouch1 crouch2 crouch3 crouch4 crouch5 crouch6 crouch7 crouch8
$frame crouch9 crouch10 crouch11 crouch12 crouch13 crouch14 crouch15 crouch16
$frame crouch17 crouch18 crouch19 crouch20 crouch21 crouch22 crouch23 crouch24
$frame crouch25 crouch26 crouch27 crouch28 crouch29 crouch30

$frame crWait1 crWait2 crWait3 crWait4 crWait5 crWait6 crWait7 crWait8
$frame crWait9 crWait10 crWait11 crWait12 crWait13 crWait14 crWait15

$frame grow1 grow2 grow3 grow4 grow5 grow6 grow7 grow8
$frame grow9 grow10 grow11 grow12 grow13 grow14 grow15 grow16
$frame grow17 grow18 grow19 grow20 grow21 grow22 grow23 grow24

$frame breath1 breath2 breath3 breath4 breath5 breath6 breath7 breath8
$frame breath9 breath10 breath11 breath12 breath13 breath14 breath15 breath16
$frame breath17 breath18 breath19 breath20 breath21 breath22 breath23 breath24
$frame breath25 breath26 breath27 breath28 breath29 breath30 breath31 breath32
$frame breath33 breath34 breath35 breath36 breath37 breath38 breath39 breath40
$frame breath41 breath42 breath43

$frame power1 power2 power3 power4 power5 power6 power7 power8
$frame power9 power10 power11 power12 power13 power14 power15 power16
$frame power17 power18 power19 power20

$frame painA1 painA2 painA3 painA4 painA5 painA6 painA7 painA8 painA9
$frame painB1 painB2 painB3 painB4 painB5 painB6 painB7 painB8
$frame painB9 painB10 painB11 painB12 painB13 painB14 painB15 painB16
$frame painB17 painB18 painB19 painB20

float EIDOLON_PHASE1 = 1;    // Trapped in cage
float EIDOLON_PHASE2 = 2;    // Fighting

//======================================================================
// Global functions
//======================================================================
// Work out angle difference between player and boss
//----------------------------------------------------------------------
float(vector targorg, float angdiff) eidolon_angcheck =
{
    self.ideal_yaw = vectoyaw(targorg - self.origin);
    if ((self.ideal_yaw - angdiff) > self.angles_y) return TRUE;
    else if ((self.ideal_yaw + angdiff)<self.angles_y) return TRUE;
    else return FALSE;
};

//----------------------------------------------------------------------
// Need own find player function because of arena size
// Also setup all parameters ready to start combat quicker
//----------------------------------------------------------------------
float() eidolon_FindTarget =
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

    // Stage 1 is trapped, boss cannot attack or run
    if (self.style == EIDOLON_PHASE1) return FALSE;

    // Move straight into run+attack
    self.nextthink = time + 0.1;
    self.think = self.th_run;        // Start running
    SUB_AttackFinished (1);            // wait a while before first attack

    // We have a winner!
    return TRUE;    
};

//----------------------------------------------------------------------
// Setup wave HP and trigger boundaries
//----------------------------------------------------------------------
void() eidolon_WaveSetupHP =
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
// Check if HP reached next wave, walk to center and fire targets
//----------------------------------------------------------------------
float() eidolon_WaveCheck =
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
            if (self.attachment2.classtype == CT_MONEIDOLONWAVE) {
                // Spawn any adds to keep player busy
                if (self.attachment2.target != "") 
                    trigger_strs(self.attachment2.target, self);
                
                // Turn around and fire at crystal
                self.th_jump();
            }
        }

        // Update Boss wave parameters (next wave!)
        self.bosswave = self.bosswave + 1;
        eidolon_WaveSetupHP();        // Reset trigger/hp
        return TRUE;
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Eidolon game play logic
//----------------------------------------------------------------------
void() EidolonCheckAttack = 
{
    // Check for boss wave trigger events
    if (eidolon_WaveCheck() == TRUE) return;

    //----------------------------------------------------------------------
    // Check the tether system
    //----------------------------------------------------------------------
    self.t_length = vlen(self.origin - self.movelast.origin);
    self.t_width = vlen(self.enemy.origin - self.movelast.origin);
    // Is the boss at maximum range?
    if (self.t_length > MONAI_MAXEIDO) {
        // Can the boss see the player?
        if (enemy_vis) self.attack_state = AS_MELEE;
        else {
            // At max range + cannot see player, exit combat
            self.enemytarget = self.enemy;
            self.enemy = world;
            self.goalentity = self.movetarget = self.movelast;
            self.attack_state = AS_STRAIGHT;
            self.think = self.th_walk;
        }
        return;
    }
        
    //----------------------------------------------------------------------
    // Check Melee range and constantly fire
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_CLOSEEIDO) {
        self.attack_state = AS_MELEE;
        return;
    }

    //----------------------------------------------------------------------
    // Range attacks (Boulder + Rock Storm)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        // Intense rock storm attack
        if (self.enemydist < MONAI_MEDIUMEIDO && random() < 0.75) {
            // Skill 0=3s, 1=2.25s, 2=1.5s, 3=0.75s
            self.attack_speed = (4 - skill) * 0.75;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_MELEE;
            return;
        }
        // Large boulder attack
        else {
            // Skill 0=4s, 1=3s, 2=2s, 3=1s
            self.attack_speed = (4 - skill) * 1;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_MISSILE;
            return;
        }
    }
};

//======================================================================
// MONSTER STATES (stand, walk run, attack, pain and death!
//======================================================================
void() eidolon_idle =
{
    // Do nothing if dead
    if (self.health < 1) return;
    if (self.walkframe == 1) monster_idle_sound();
    self.nextthink = time + 0.05;

    // Keep moving the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 14) self.walkframe = 0;
    self.frame = $stand1 + self.walkframe;
    
    // Keep looking for a player
    eidolon_FindTarget ();
    
    if (self.style == EIDOLON_PHASE1) {
        // Slow animation speed, trapped inside cage
        self.nextthink = time + 0.1;
        // Found a player, now track them around the room
        if (self.enemy.flags & FL_CLIENT) {
            if (eidolon_angcheck(self.enemy.origin, 60)) 
                self.think = self.th_walk;
        }
    }
};

//----------------------------------------------------------------------
void() eidolon_stand1 = [ $stand1, eidolon_idle ] {self.walkframe = 0;};

//======================================================================
// There is only one set of animations for walk and run and
// the only different is the final AI state (walk/run) 
// This is a hexen2 model so it should run at 20fps not 10fps
//----------------------------------------------------------------------
float EIDOLON_WALK = 2;
float EIDOLON_RUN = 4;

//----------------------------------------------------------------------
void(float fstate) eidolon_move =
{
    // Do nothing if dead
    if (self.health < 1) return;
    self.nextthink = time + 0.05;
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 23) self.walkframe = 0;
    self.frame = $walk1 + self.walkframe;
    
    // Check for any special frame events
    if (self.frame == $walk1) monster_idle_sound();
    if (self.frame == $walk2 || self.frame == $walk14)
        monster_footstep(FALSE);
    
    // Phase 1 (caged) uses walk to turn around
    if (self.style == EIDOLON_PHASE1) {
        // Wrong angle difference? keep turning
        if (eidolon_angcheck(self.enemy.origin, 10)) ChangeYaw();
        else {
            // Close the beginning or the end of the animation?
            if (self.walkframe < 4 || self.walkframe > 20) 
                self.think = self.th_stand;
        }
    }
    // Phase 2 (free) walking back to center
    else if (self.style == EIDOLON_PHASE2 && fstate == EIDOLON_WALK) {
        // Check for player
        if (eidolon_FindTarget ()) return;

        // Calculate distance to origin location (notouch function)
        self.enemydist = vlen(self.movelast.origin - self.origin);
        // Reached origin point yet?
        if (self.enemydist < MONAI_SPAWNEIDO) {
            // Still remember the old player?
            if (self.enemytarget.flags & FL_CLIENT) {
                self.enemy = self.enemytarget;
                self.enemytarget = world;
            }
            // Keep turning towards old player
            if (eidolon_angcheck(self.enemy.origin, 10)) ChangeYaw();
            else self.think = self.th_stand;
        }
        // Still not reached center
        else movetogoal(6);
    }
    // Time to walk or run?
    else if (fstate == EIDOLON_RUN && self.enemy) ai_run(6);
};

//----------------------------------------------------------------------
void() eidolon_walk1 = [ $walk1, eidolon_walk1 ] {eidolon_move(EIDOLON_WALK);};
void() eidolon_run1 = [ $walk1, eidolon_run1 ] {eidolon_move(EIDOLON_RUN);};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() eidolon_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_EIDO1;
    }
};

//----------------------------------------------------------------------
void() eidolon_setup_attachment = 
{
    // Is monster dead or dying?
    if (self.health < 1) return;

    // Check if attachment has been setup yet
    if (!self.attachment) eidolon_create_attachment();

    // Move attachment into place ready for use
    self.attachment.movetype = MOVETYPE_NONE;
    self.attachment.solid = SOLID_NOT;
    setmodel(self.attachment, MODEL_EMPTY);
    setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(self.attachment, self.origin);
    // Using empty model, so reset angle, frame and skin.
    self.attachment.angles = self.angles;
    self.attachment.frame = self.attachment.skin = 0;
};

//----------------------------------------------------------------------
void() eidolon_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() eidolon_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
}

//======================================================================
// ATTACK A  - Fire large explosive rock (grenade)
// Animation - Spell, put right hand up and infront
//======================================================================
void(vector rockofs) Fire_attackA =
{
    local float foffset, offset, shotcount;
    local vector org, sorg, dir, offang, avel;

    // Finished with growing rock
    eidolon_finish_attachment();

    if (!self.enemy) return;
    if (self.health < 1) return;

    makevectors(self.angles);
    org = self.origin + attack_vector(rockofs);
    self.effects = self.effects | EF_MUZZLEFLASH;

    // create an arc of rockets fanning outwards from source
    foffset = 8;
    offset = -foffset; shotcount = 3;
    while (shotcount > 0) {
    // based angle of projectiles on straight line between source and target
        offang = vectoangles ((self.enemy.origin-'0 0 24') - org);
        offang_y = offang_y + offset;
        makevectors (offang);
    
        // Randomly spawn projectiles around central point
        sorg = org + v_up * (5 + random() * 10);
        
        // Straight line velocity from source to target
        dir = normalize (v_forward);
        dir_z = 0 - dir_z + (random() - 0.5)*0.1;    // Slight wiggle up/down
        avel = vecrand(100,200,FALSE);
        
        // Variable speed based on skill level with extra random spice
        self.attack_speed = SPEED_EIDOROCK + (skill * SPEED_EIDOSKILL) + random()*10;
        Launch_Missile (sorg, dir, avel, CT_PROJ_EIDO1, self.attack_speed );
        offset = offset + foffset;
        shotcount = shotcount - 1;
    }
};

//----------------------------------------------------------------------
void(vector rockofs, float rockframe) Setup_attackA =
{
    local vector org;

    // Check if attachment has been setup yet
    if (!self.attachment) eidolon_create_attachment();
    
    // Frame 0 is start of the sequence (move everything into place)
    if (rockframe == 0) {
        self.attachment.state = STATE_ON;
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;

        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);

        // Start throwing rock sound
        sound (self, CHAN_WEAPON, "eidolon/attacka.wav", 1, ATTN_NORM);
    }

    // Turn towards enemy and update model attachment
    // The offset is based on the Eidolon facing forward
    makevectors(self.angles);
    org = self.origin + attack_vector(rockofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.angles_x = random()*360;
    self.attachment.frame = rockframe;
    // Random explosion like particles from ball as it grows
    particle_explode(self.attachment.origin, 10+random()*10, 0.5+random(), PARTICLE_BURST_WHITE, 0);
};

//----------------------------------------------------------------------
void() eidolon_attackA2 = {
    // Do nothing if dead
    if (self.health < 0) return;

    self.nextthink = time + 0.05;
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 19) self.think = self.th_run;
    self.frame = $spell1 + self.walkframe;
    
    // Keep turning towards the player
    if (self.frame < $spell14) ai_face();
    
    // Check the state of the projectile (grow/fire)
    if (self.frame == $spell3) eidolon_setup_attachment();
    else if (self.frame == $spell4) Setup_attackA('60 18 64',0);
    else if (self.frame == $spell5) Setup_attackA('62 18 68',1);
    else if (self.frame == $spell6) Setup_attackA('64 18 72',2);
    else if (self.frame == $spell7) Setup_attackA('68 18 78',3);
    else if (self.frame == $spell8) Setup_attackA('72 18 86',4);
    else if (self.frame == $spell9) Setup_attackA('74 18 96',5);
    else if (self.frame == $spell10) Setup_attackA('70 18 104',6);
    else if (self.frame == $spell11) Setup_attackA('72 18 112',7);
    else if (self.frame == $spell12) Setup_attackA('73 18 112',7);
    else if (self.frame == $spell13) Fire_attackA('73 18 112');
};

//----------------------------------------------------------------------
void() eidolon_attackA1 = [ $spell1, eidolon_attackA2 ] {ai_face();
    self.pain_finished = time + 1.5;
    self.walkframe = 0;
};

//======================================================================
// ATTACK B - Drops rocks randomly from above player
// Animation - Howl, raise arms up and then pull down
//======================================================================
void() eidolon_attackB1 =
{
};

//======================================================================
// ATTACK C - Projectile vomit rocks and dust at the player
// Animation - breathe, swings arms back and push head forward
//======================================================================
void(vector rockofs) Fire_attackC =
{
    local vector org, dir, avel;

    if (!self.enemy) return;
    if (self.health < 1) return;

    makevectors(self.angles);
    // Spawn a mist of particles where projectiles came from
    org = self.origin + attack_vector(rockofs) + v_right*(crandom()*8) + v_up*(crandom()*16);

    dir = normalize(self.enemy.origin - org);
    particle_explode(org, 5 + rint(random()*5), 2, PARTICLE_BURST_WHITE, PARTICLE_BURST_UPWARD);
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_EIDOSPIKE + (skill * SPEED_EIDOSKILLSP);
    
    launch_projectile (org, dir, CT_PROJ_GROCK, self.attack_speed);
};

//----------------------------------------------------------------------
void() eidolon_attackC2 = {
    // Do nothing if dead
    if (self.health < 0) return;

    self.nextthink = time + 0.05;
    // Keep turning towards the player
    ai_face();
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 43) self.think = self.th_run;
    self.frame = $breath1 + self.walkframe;
    
    // Start demon roar and then rocks!?!
    if (self.frame == $breath4)
        sound (self, CHAN_WEAPON, "eidolon/attackc.wav", 1, ATTN_NORM);
    
    // Rock breath!?!
    if (self.frame > $breath17 && self.frame < $breath35)
        Fire_attackC('56 0 44');
};

//----------------------------------------------------------------------
void() eidolon_attackC1 = [ $breath1, eidolon_attackC2 ] {ai_face();
    self.pain_finished = time + 2;
    self.walkframe = 0;
};

//======================================================================
// ATTACK D - Throw a bunch of rocks upward towards the player
// Animation - death/grow, crouch down and swing arms upward
//======================================================================
void() eidolon_attackD1 =
{
};

//======================================================================
// ATTACK E - Summon monsters from blocks of stone
// Animation - power, lean head back, put hands up right infront
//======================================================================
void(vector rockofs, float rockframe) Setup_attackE =
{
    local vector org;

    // Check if attachment has been setup yet
    if (!self.attachment) eidolon_create_attachment();
    
    // Frame 0 is start of the sequence (move everything into place)
    if (rockframe == 0) {
        self.attachment.state = STATE_ON;
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.skin = 1;

        // Start long growl
        sound (self, CHAN_VOICE, "eidolon/attacke.wav", 1, ATTN_NORM);
        sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
    }
    
    if (rockframe == 7) {
        // Goodbye little rock!
        eidolon_finish_attachment();
        // Wakeup crystal and golems
        if (self.attachment2.target2 != "") 
                    trigger_strs(self.attachment2.target2, self);
    }
    else {
        // Turn towards enemy and update model attachment
        // The offset is based on the Eidolon facing forward
        makevectors(self.angles);
        org = self.origin + attack_vector(rockofs);
        setorigin(self.attachment, org);
        self.attachment.angles = self.angles;
        self.attachment.angles_x = random()*360;
        self.attachment.frame = rockframe;
        // Random explosion like particles from ball as it grows
        particle_explode(self.attachment.origin, 10+random()*10, 0.5+random(), PARTICLE_BURST_BLUE, 0);

        // Generate the lightning effect
        self.effects = self.effects | EF_MUZZLEFLASH;
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity (MSG_BROADCAST, self);
        WriteCoord (MSG_BROADCAST, self.attachment.origin_x);
        WriteCoord (MSG_BROADCAST, self.attachment.origin_y);
        WriteCoord (MSG_BROADCAST, self.attachment.origin_z);
        WriteCoord (MSG_BROADCAST, self.attachment2.origin_x);
        WriteCoord (MSG_BROADCAST, self.attachment2.origin_y);
        WriteCoord (MSG_BROADCAST, self.attachment2.origin_z);
    }
};

//----------------------------------------------------------------------
void() eidolon_attackE_firing = {
    self.nextthink = time + 0.1;
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    // is the lightning spell finished?
    if (self.walkframe > 19) {
        self.think = self.th_run;
        self.walkframe = 0;
        Resist_ChangeType(self, FALSE);
        self.takedamage = DAMAGE_YES;
        self.goalentity = self.enemy;
    }
    self.frame = $power1 + self.walkframe;

    // Check the state of the projectile (grow/fire)
    if (self.frame == $power7) Setup_attackE('68 16 84',0);
    else if (self.frame == $power8) Setup_attackE('72 16 92',1);
    else if (self.frame == $power9) Setup_attackE('74 16 94',2);
    else if (self.frame == $power10) Setup_attackE('76 16 98',3);
    else if (self.frame == $power11) Setup_attackE('76 16 102',4);
    else if (self.frame == $power12) Setup_attackE('78 16 104',5);
    else if (self.frame == $power13) Setup_attackE('80 16 104',7);
}

//----------------------------------------------------------------------
void() eidolon_attackE2 = [ $power1, eidolon_attackE_firing ] {
    self.walkframe = 0;
};

//----------------------------------------------------------------------
void() eidolon_attackE_turning = {
    self.nextthink = time + 0.05;
    
    // Turn towards the power crystal
    if (eidolon_angcheck(self.attachment2.origin,5)) ChangeYaw();
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 23) {
        self.think = eidolon_attackE2;
        self.walkframe = 0;
    }
    self.frame = $walk1 + self.walkframe;
    
    // Check for any special frame events
    if (self.frame == $walk2 || self.frame == $walk14)
        monster_footstep(FALSE);
    
    // Move slowly closer to crystal or backward!
    self.enemydist = range_distance(self.attachment2, FALSE);
    if (self.enemydist > MONAI_CLOSEEIDO) movetogoal(6);
    else walkmove (self.ideal_yaw - 180, 3);
};

//----------------------------------------------------------------------
void() eidolon_attackE1 = [ $walk1, eidolon_attackE_turning ] {
    Resist_ChangeType(self, TRUE);
    self.takedamage = DAMAGE_NO;
    self.goalentity = self.attachment2;
    self.yaw_speed = 6;        // Turning slowly
    self.walkframe = 0;        // Reset animation counter
    
    // Check angle difference betweem boss and crystal
    // If within 5 degree's tolerance, skip turning
    if (!eidolon_angcheck(self.attachment2.origin,5)) 
        self.think = eidolon_attackE2;
};

//============================================================================
// PAIN and DEATH
//============================================================================
void() eidolon_painA2 =
{
    // Do nothing if dead
    if (self.health < 1) return;
    self.nextthink = time + 0.1;

    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 8) {
        self.frame = $painA8;
        self.think = eidolon_run1;
    }
    else self.frame = $painA1 + self.walkframe;
};

//----------------------------------------------------------------------
void() eidolon_painB2 =
{
    // Do nothing if dead
    if (self.health < 1) return;
    self.nextthink = time + 0.05;

    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 19) {
        self.frame = $painB20;
        self.think = eidolon_run1;
    }
    else self.frame = $painB1 + self.walkframe;
};

//----------------------------------------------------------------------
void() eidolon_painA1 = [ $painA1, eidolon_painA2 ] {self.walkframe = 0;};
void() eidolon_painB1 = [ $painB1, eidolon_painB2 ] {self.walkframe = 0;};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) eidolon_pain =
{
    // Finish with all attachments
    eidolon_finish_attachment();

    // Check for boss wave trigger events
    if (eidolon_WaveCheck() == TRUE) return;

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (random() < 0.7) eidolon_painA1 ();    // Short recoil
            else {
                eidolon_painB1 ();                    // Long recoil
                self.pain_finished = time + 2;        // long animation
            }
        }
    }
};

//============================================================================
// Oh no rocky has died!?!
//============================================================================
void() eidolon_explode =
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
    SpawnExplosion(EXPLODE_PLASMA_BIG, self.origin, "eidolon/explode_death.wav");

    // Gib explosive fountain!?!
    self.max_health = MON_GIBEXPLOSION;
    ThrowGib(11, 10 + rint(random()*10));    // Small Golem rock
    ThrowGib(12, 10 + rint(random()*10));    // Large Golem rock
    ThrowGib(4, 5 + rint(random()*3));        // Some flesh bits
    ThrowGib(5, 5 + rint(random()*3));
};

//----------------------------------------------------------------------
void() eidolon_die2 = 
{
    self.nextthink = time + 0.1;
    if (self.frame == $crouch3) self.solid = SOLID_NOT;
    
    // Keep moveing the animation frame forward
    if (self.walkframe < 30) {
        self.walkframe = self.walkframe + 1;
        self.frame = $crouch1 + self.walkframe;
    }
    else {
        // Make sure final frame is set
        self.frame = $crouch30;

        // Spawn implosion effect
        if (random() < 0.5)
            particle_implode(self.origin, 20+random()*20, 100, 100, PARTICLE_BURST_WHITE);
        // Spawn particle smoke and explosive smoke
        else {
            SpawnProjectileSmoke(self.origin, 150, 100, 300);
            particle_dust(self.origin, 10+random()*10, PARTICLE_BURST_WHITE);
        }
    
        // Check for any random lightning strikes outward
        if (random() < 0.5) {
            self.pos1 = self.origin - '0 0 16';
            self.pos2 = vecrand(0,50,TRUE);
            self.pos2_z = 25 + random()*25;    // Always up!
            self.pos3 = normalize(self.pos2);
            traceline(self.pos1, self.pos1 + self.pos3 * 600, FALSE, self);
            
            // Create lightning bolt
            WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
            WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
            WriteEntity (MSG_BROADCAST, self);
            WriteCoord (MSG_BROADCAST, self.origin_x);
            WriteCoord (MSG_BROADCAST, self.origin_y);
            WriteCoord (MSG_BROADCAST, self.origin_z);
            WriteCoord (MSG_BROADCAST, trace_endpos_x);
            WriteCoord (MSG_BROADCAST, trace_endpos_y);
            WriteCoord (MSG_BROADCAST, trace_endpos_z);

            // Play some arching lightning sounds
            if (random() < 0.5) {
                if (random() < 0.5) sound (self, CHAN_BODY, "eidolon/elec_arch1.wav", 1, ATTN_NORM);
                else sound (self, CHAN_BODY, "eidolon/elec_arch2.wav", 1, ATTN_NORM);
            }
        }
    
        // Cycle through end phase
        self.lip = self.lip + 1;
        self.nextthink = time + 0.1;    // Slow down to regular time
        if (self.lip == 26) sound (self, CHAN_AUTO, "eidolon/implode_death.wav", 1, ATTN_NONE);
        if (self.lip > 40) eidolon_explode();
    }
};

//----------------------------------------------------------------------
void() eidolon_die1 = [ $crouch1, eidolon_die2 ] {
    self.walkframe = self.lip = 0;
};

//----------------------------------------------------------------------
void() eidolon_die =
{
    self.deadflag = DEAD_DEAD;        // the rock finally crashed
    self.effects = 0;                // Remove effects on death
    eidolon_finish_attachment();    // Remove any attachments
    eidolon_remove_attachment();

    sound (self, CHAN_VOICE, "eidolon/death.wav", 1, ATTN_NORM);
    eidolon_die1 ();
};

//======================================================================
// Roar and break free from cage!
//======================================================================
void() eidolon_free2 =
{
    // Do nothing if dead
    if (self.health < 0) return;

    self.nextthink = time + 0.1;
    
    // Keep moveing the animation frame forward
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 27) {
        self.yaw_speed = 20;            // Average Speed
        Resist_ChangeType(self, FALSE);    // Default resistance
        self.think = self.th_run;        // Start running!
    }
    self.frame = $howl1 + self.walkframe;
};

//----------------------------------------------------------------------
void() eidolon_free1 = [ $howl1, eidolon_free2 ] {
    self.takedamage = DAMAGE_YES;        // Enable resistance effects
    sound (self, CHAN_VOICE, self.sight_sound, 1, ATTN_NORM);
    self.walkframe = 0;
};

//======================================================================
// Setup Eidolon after trigger event
//======================================================================
void() eidolon_wakeup =
{
    self.use = SUB_Null;            // No more triggers
    self.style = EIDOLON_PHASE2;    // Free to fight
    
    // Restore all think functions
    self.th_stand = eidolon_stand1;
    self.th_walk = eidolon_walk1;
    self.th_run = eidolon_run1;
    self.th_melee = eidolon_attackC1;
    self.th_missile = eidolon_attackA1;
    self.th_jump = eidolon_attackE1;
    self.th_pain = eidolon_pain;
    self.th_die = eidolon_die;
    
    self.pain_finished = time + 3;        // Make pain go away
    self.attack_finished = time + 2;    // Reset attack system

    // Time for Eidolon to break free!
    self.think = eidolon_free1;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() eidolon_start =
{
    self.use = SUB_Null;            // Suppress trigger events
    self.style = EIDOLON_PHASE1;    // Trapped in cage
    self.flags = FL_MONSTER;        // Reset flag (no user settings)
    
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    setorigin(self, self.origin);
    
    self.yaw_speed = 6;                // Really slow while in cage
    self.velocity = '0 0 0';        // Make sure stationary
    self.deadflag = DEAD_NO;        // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    self.pain_longanim = FALSE;        // No axe advantage
    self.takedamage = DAMAGE_NO;    // Immune to damage
    
    self.gibhealth = -1000;            // Special death sequence
    self.pain_finished = self.attack_finished = 0;
    
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
    eidolon_WaveSetupHP();

    // default = No think functions
    self.th_checkattack = EidolonCheckAttack;
    self.th_stand = self.th_walk = self.th_run = SUB_Null;
    self.th_missile = self.th_die = SUB_Null;
    self.th_pain = SUB_Null_pain;
    
    // Check there is space for Eidolon to spawn
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

    // Tether point, save for later
    self.movelast = spawn();
    self.movelast.owner = self;
    self.movelast.classtype = CT_MONEIDOLON;
    self.movelast.movetype = MOVETYPE_NONE;
    self.movelast.solid = SOLID_NOT;
    self.movelast.origin = self.origin;
    setsize (self.movelast, VEC_ORIGIN, VEC_ORIGIN);

    // Make sure all death triggers are setup ready
    self.message2 = self.target;
    self.target = self.target2 = self.deathtarget = "";

    // Can only stand around and walk on the spot
    self.th_stand = eidolon_stand1;
    self.th_walk = self.th_run = eidolon_walk1;
    
    // Wait for trigger event to be free from cage
    self.think = self.th_stand;
    self.nextthink = time + 0.1;
    self.use = eidolon_wakeup;
};

/*======================================================================
 QUAKED monster_eidolon (1 0 0) (-32 -32 -24) (32 32 128)
======================================================================*/
void() monster_eidolon =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/mon_bosseidolon.mdl";
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_EIDO1);
    precache_model (MODEL_PROJ_GROCK1);
    precache_model (MODEL_PROJ_GROCK2);

    self.gib1mdl = MODEL_PROJ_GROCK1;    // small golem rock
    self.gib2mdl = MODEL_PROJ_GROCK2;    // medium golem rock
    self.gib1frame = self.gib2frame = 9;

    // sight/pain/death sounds
    self.idle_sound = "eidolon/idle1.wav";
    self.idle_sound2 = "eidolon/idle2.wav";
    self.pain_sound = "eidolon/pain.wav";
    self.sight_sound = "eidolon/sight.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.pain_sound);
    precache_sound (self.sight_sound);

    // Final death sequence
    precache_sound ("eidolon/death.wav");
    precache_sound ("eidolon/explode_death.wav");
    precache_sound ("eidolon/implode_death.wav");
    precache_sound ("eidolon/elec_arch1.wav");
    precache_sound ("eidolon/elec_arch2.wav");
    
    // Attack A - Throw Boulder - range attack
    precache_sound ("eidolon/attacka.wav");
    precache_sound ("eidolon/rock_hit1.wav");
    precache_sound ("eidolon/rock_hit2.wav");

    // Attack C - Rockstorm - range attack
    precache_sound ("eidolon/attackc.wav");
    precache_sound (SOUND_IMP_ROCK1);
    precache_sound (SOUND_IMP_ROCK2);
    precache_sound (SOUND_IMP_ROCK3);
    precache_sound (SOUND_IMP_ROCK4);

    // Attack E - Lightning at summoning crystal - range attack
    precache_sound ("eidolon/attacke.wav");
    
    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.bbmins = '-32 -32 -24';    // Special Size
    self.bbmaxs = '32 32 128';
    self.bboxtype = BBOX_CUSTOM;
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)
    self.poisonous = FALSE;            // Cannot be poisonous
    self.deathstring = " was pulverized by the Sculptor\n";

    self.health = self.max_health = MEGADEATH;
    self.pain_finished = time + LARGE_TIMER;
    self.takedamage = DAMAGE_NO;    // Immune to damage
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 200;            // 1/2 Shambler level
    self.pain_timeout = 2;            // Stop constant pain
    self.infightextra = 3;            // Crazy damage, noinfighting
    self.skin = 1;                    // New rocky skin

    // Custom feet sounds
    self.stepc1 = "eidolon/footstep1.wav";
    self.stepc2 = "eidolon/footstep2.wav";
    self.stepc3 = "eidolon/footstep3.wav";
    self.stepc4 = "eidolon/footstep4.wav";
    self.stepc5 = "eidolon/footstep1.wav";
    precache_sound (self.stepc1);
    precache_sound (self.stepc2);
    precache_sound (self.stepc3);
    precache_sound (self.stepc4);
    precache_sound (self.stepc5);
    self.steptype = FS_TYPECUSTOM;
    
    self.classtype = CT_MONEIDOLON;
    self.classgroup = CG_STONE;
    self.classmove = MON_MOVEWALK;
    self.style = 0;
    
    // No targetname = no trigger!
    if (self.targetname == "") {
        dprint("\b[EIDO_BOSS]\b Missing trigger name!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        return;
    }

    total_monsters = total_monsters + 1;
    self.use = eidolon_start;
};

/*======================================================================
/*QUAKED monster_eidolon_wavetrig (0.75 0.25 1) (-32 -32 -4) (32 32 4) x
Target for boss lightning wave event
-------- KEYS --------
targetname : name of wave event (links to noise key on boss)
target     : trigger name of adds to spawn
target2    : trigger name of crystal event
noise      : targetname of collision entity
-------- SPAWNFLAGS --------
-------- NOTES --------
Target for boss lightning wave event

======================================================================*/
void() monster_eidolon_wavetrig = 
{
    self.classtype = CT_MONEIDOLONWAVE;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.takedamage = DAMAGE_NO;
    setsize (self, VEC_ORIGIN, VEC_ORIGIN);
};

