/*==============================================================================
Nouronihar (Based on a monster by Lunaran)
==============================================================================*/
// Original wizard idle
$frame idle1 idle2 idle3 idle4 idle5 idle6 idle7 idle8
$frame idle9 idle10 idle11

// Leaning forward flying pose
$frame fly1 fly2 fly3 fly4 fly5 fly6 fly7 fly8 
$frame fly9 fly10 fly11 fly12 fly13

// A = Short recoil to right, B = Long recoil to left
$frame paina1 paina2 paina3 paina4 paina5 paina6
$frame painb1 painb2 painb3 painb4 painb5 painb6 painb7 painb8 painb9

// Very quick fall down backward death
$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

// A - Typical spit/spike firing from all arms
$frame shota1 shota2 shota3 shota4 shota5 shota6 shota7 shota8
$frame shota9 shota10 shota11

// B - Long spit/spike firing from all arms
$frame shotb1 shotb2 shotb3 shotb4 shotb5 shotb6 shotb7 shotb8
$frame shotb9 shotb10 shotb11 shotb12 shotb13 shotb14 shotb15 shotb16 shotb17

// C - Slowly flick all arms upward (big attack)
$frame shotc1 shotc2 shotc3 shotc4 shotc5 shotc6 shotc7 shotc8
$frame shotc9 shotc10 shotc11 shotc12 shotc13 shotc14 shotc15 shotc16
$frame shotc17 shotc18 shotc19 shotc20 shotc21 shotc22 shotc23

// Rise upward and flee from the scene
$frame flee1 flee2 flee3 flee4 flee5 flee6 flee7 flee8
$frame flee9 flee10 flee11 flee12 flee13 flee14 flee15 flee16
$frame flee17 flee18

// Long reveal of extra arms, pretending to be wizard at first
$frame reveal1 reveal2 reveal3 reveal4 reveal5 reveal6 reveal7 reveal8
$frame reveal9 reveal10 reveal11 reveal12 reveal13 reveal14 reveal15 reveal16
$frame reveal17 reveal18 reveal19 reveal20 reveal21 reveal22 reveal23 reveal24
$frame reveal25 reveal26 reveal27 reveal28 reveal29 reveal30 reveal31 reveal32
$frame reveal33 reveal34 reveal35 reveal36

float NOUR_PHASE0 = -1;    // No waves
float NOUR_PHASE1 = 1;    // Burst through floor
float NOUR_PHASE2 = 2;    // Fighting
float NOUR_PHASE3 = 3;    // Frenzy mode
float NOUR_PHASE4 = 4;    // Death

//======================================================================
// Global functions
//======================================================================
// Special streamlined player find function
//----------------------------------------------------------------------
float() nour_FindTarget =
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
void() nour_WaveSetupHP =
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
// Check if HP has reached next boss wave trigger event
//----------------------------------------------------------------------
float() nour_WaveCheck =
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

        // Update Boss wave parameters (next wave!)
        self.bosswave = self.bosswave + 1;
        nour_WaveSetupHP();
        self.style = NOUR_PHASE3;    // Frenzy mode
        return TRUE;
    }
    return FALSE;
};

//----------------------------------------------------------------------
// Check the tether system
//----------------------------------------------------------------------
float() nour_CheckTether =
{
    // Check for boss mode
    if (!(self.spawnflags & MON_NOUR_BOSS)) return FALSE;

    self.t_length = vlen(self.origin - self.movelast.origin);
    // Check the most obvious first, inside tether range?
    if (self.t_length < MONAI_MAXNOUR) return FALSE;
    else {
        // If player or tether close to each other?
        if (infront(self.movelast) && infront(SUB_entEnemyTarget()) )
            return FALSE;
        // Stop moving around
        else return TRUE;
    }
};
    
//----------------------------------------------------------------------
// Nour game play logic
//----------------------------------------------------------------------
void() NourCheckAttack = 
{
    //----------------------------------------------------------------------
    // setup enemytarget if one is not active
    //----------------------------------------------------------------------
    if (self.enemy.classtype != CT_ENEMYTARGET) {
        SUB_setupEnemyTarget(self.enemy, self.height, MONAI_ABOVETIMER);
        if (self.enemytarget) self.enemy = self.enemytarget;
    }

    //----------------------------------------------------------------------
    // Check Melee range and constantly fire
    //----------------------------------------------------------------------
    if (self.enemydist < MONAI_MELEENOUR) {
        self.attack_state = AS_MELEE;
        return;
    }
    
    //----------------------------------------------------------------------
    // Range attacks (Spit and Bomb)
    //----------------------------------------------------------------------
    // Only range attack if cooldown has finished
    if (time > self.attack_finished) {
        // Fast/intense nail/spit attack
        if (self.enemydist < MONAI_CLOSENOUR) {
            // Skill 0=3s, 1=2.25s, 2=1.5s, 3=0.75s
            self.attack_speed = (4 - skill) * 0.75;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_MELEE;
            return;
        }
        // Large rocket bomb attack with floor damage
        else {
            // Skill 0=4s, 1=3s, 2=2s, 3=1s
            self.attack_speed = (4 - skill) * 1;
            self.attack_finished = time + self.attack_speed + random();
            self.attack_state = AS_MISSILE;
            return;
        }
    }

    //----------------------------------------------------------------------
    // Maintain distance (strafe)
    //----------------------------------------------------------------------
    if (enemy_range >= RANGE_MID || !enemy_vis) {
        if (self.attack_state != AS_STRAIGHT) self.attack_state = AS_STRAIGHT;
    }
    else self.attack_state = AS_SLIDING;
};

//======================================================================
// MONSTER STATES (stand, walk run, attack, pain and death!
//======================================================================
void() nour_stand1    =[    $idle1, nour_stand2] {monster_idle_sound();ai_stand();};
void() nour_stand2    =[    $idle2, nour_stand3] {ai_stand();};
void() nour_stand3    =[    $idle3, nour_stand4] {ai_stand();};
void() nour_stand4    =[    $idle4, nour_stand5] {ai_stand();};
void() nour_stand5    =[    $idle5, nour_stand6] {ai_stand();};
void() nour_stand6    =[    $idle6, nour_stand7] {ai_stand();};
void() nour_stand7    =[    $idle7, nour_stand8] {ai_stand();};
void() nour_stand8    =[    $idle8, nour_stand9] {ai_stand();};
void() nour_stand9    =[    $idle9, nour_stand10] {ai_stand();};
void() nour_stand10    =[    $idle10, nour_stand11] {ai_stand();};
void() nour_stand11    =[    $idle11, nour_stand1] {ai_stand();};

//======================================================================
void() nour_walk1    =[    $idle1, nour_walk2] {monster_idle_sound();ai_walk(8);};
void() nour_walk2    =[    $idle2, nour_walk3] {ai_walk(8);};
void() nour_walk3    =[    $idle3, nour_walk4] {ai_walk(8);};
void() nour_walk4    =[    $idle4, nour_walk5] {ai_walk(8);};
void() nour_walk5    =[    $idle5, nour_walk6] {ai_walk(8);};
void() nour_walk6    =[    $idle6, nour_walk7] {ai_walk(8);};
void() nour_walk7    =[    $idle7, nour_walk8] {ai_walk(8);};
void() nour_walk8    =[    $idle8, nour_walk9] {ai_walk(8);};
void() nour_walk9    =[    $idle9, nour_walk10] {ai_walk(8);};
void() nour_walk10    =[    $idle10, nour_walk11] {ai_walk(8);};
void() nour_walk11    =[    $idle11, nour_walk1] {ai_walk(8);};

//======================================================================
void() nour_slideframe =
{
    // Check for boss mode (phase and wave check)
    if (self.spawnflags & MON_NOUR_BOSS) {
        if (self.style != NOUR_PHASE2) return;

        // Check for boss wave trigger events
        if (nour_WaveCheck() == TRUE) {self.th_jump(); return;}
    }
    
    // Dead monster?
    if (self.health < 1) return;
    if (self.walkframe == 0) monster_idle_sound();

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 10) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = nour_slideframe;
    
    // Setup current animation frame
    self.frame = $idle1 + self.walkframe;
    self.ideal_yaw = vectoyaw(SUB_orgEnemyTarget() - self.origin);
    
    // Check the BOSS tether system
    if (nour_CheckTether()) ai_run(0);
    else ai_run(8);
};

//----------------------------------------------------------------------
void() nour_slide1 = {self.walkframe = 0; nour_slideframe();};

//======================================================================
// RUN state - chasing the player
//======================================================================
void() nour_runframe =
{
    // Check for boss mode (phase and wave check)
    if (self.spawnflags & MON_NOUR_BOSS) {
        if (self.style != NOUR_PHASE2) return;

        // Check for boss wave trigger events
        if (nour_WaveCheck() == TRUE) {self.th_jump(); return;}
    }
    
    if (self.health < 1) return;
    if (self.walkframe == 0) monster_idle_sound();
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 12) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = nour_runframe;
    
    // Setup current animation frame
    self.frame = $fly1 + self.walkframe;
    self.ideal_yaw = vectoyaw(SUB_orgEnemyTarget() - self.origin);
    
    // Check the BOSS tether system
    if (nour_CheckTether()) ai_run(0);
    else ai_run(16);
};

//----------------------------------------------------------------------
void() nour_run1 = {self.walkframe = 0; nour_runframe();};

//============================================================================
// Attachment management (create, finish and delete)
//============================================================================
void() nour_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_NOUR2B;
        self.attachment.alpha = 0.85;
    }
};

//----------------------------------------------------------------------
void() nour_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() nour_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
}

//======================================================================
// ATTACK A - Fire multiple spit/spikes
// Block pain function from interrupting magic attack
//======================================================================
void(vector offset) nour_spike = 
{
    local vector dtarget, starget, ftarget, sorg, forg, dir;
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_NOURSPIKE + (skill * SPEED_NOURSKILL);

    ai_face();
    // Work out direction line from self to enemy
    dtarget = SUB_orgEnemyTarget() - self.origin;
    dtarget = vectoangles(dtarget);
    makevectors (dtarget);
    
    // Work out correct source/target origins
    starget = self.origin + v_forward * 16 + v_up * offset_z;
    ftarget = SUB_orgEnemyTarget() + '0 0 16';

    // Add left/right offset
    sorg = starget + v_right * offset_x;
    forg = ftarget + v_right * offset_x;
    dir = normalize(forg - sorg);
    launch_projectile (sorg, dir, CT_PROJ_NOUR1, self.attack_speed);
    
    sorg = starget - v_right * offset_x;
    forg = ftarget - v_right * offset_x;
    dir = normalize(forg - sorg);
    launch_projectile (sorg, dir, CT_PROJ_NOUR1, self.attack_speed);
};

//----------------------------------------------------------------------
void() nour_atta1    =[    $shota1, nour_atta2] {ai_face();
    self.pain_finished = time + 1;
    sound (self, CHAN_WEAPON, "nour/attack1.wav", 1, ATTN_NORM);
};
void() nour_atta2    =[ $shota2, nour_atta3] {ai_face();};
void() nour_atta3    =[ $shota3, nour_atta4] {ai_face();};
void() nour_atta4    =[ $shota4, nour_atta5] {nour_spike('24,0,16');};
void() nour_atta5    =[ $shota5, nour_atta6] {nour_spike('20 0 8');};
void() nour_atta6    =[ $shota6, nour_atta7] {nour_spike('16 0 0');};
void() nour_atta7    =[ $shota7, nour_atta8] {nour_spike('12 0 -8');};
void() nour_atta8    =[ $shota8, nour_atta9] {nour_spike('8 0 -16');};
void() nour_atta9    =[ $shota9, nour_atta10] {};
void() nour_atta10    =[ $shota10, nour_atta11] {};
void() nour_atta11    =[ $shota11, nour_run1] {};

//======================================================================
// ATTACK B - Fire one large blob (grenade) of spit
//======================================================================
void(vector bombofs) Fire_NourBomb =
{
    local vector org, torg, ang, dir, avel;

    // Finished with bomb effect
    nour_finish_attachment();
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "nour/attack1.wav", 1, ATTN_NORM);
    
    ai_face();
    makevectors(self.angles);
    org = self.origin + attack_vector(bombofs);
    torg = SUB_orgEnemyTarget();

    self.attack_speed = SPEED_NOURBOMB;
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, org, torg, self.attack_speed);
    ang = vectoangles(torg - org);
    ang_x = -self.attack_elev;
    makevectors (ang);
    dir = v_forward * self.attack_speed;

    avel = vecrand(100,200,FALSE);
    Launch_Grenade(org, dir, avel, CT_PROJ_NOUR2);
};

//----------------------------------------------------------------------
void(vector bombofs, float bombframe) Setup_NourBomb =
{
    local vector org;
    if (self.health < 1) return;
    
    // Check if attachment has been setup yet
    if (!self.attachment) nour_create_attachment();
    
    // Frame 0 is start of the sequence (move everything into place)
    if (bombframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        self.attachment.skin = 0;

        // Start growing bomb sound
        sound (self, CHAN_WEAPON, "nour/attack2.wav", 1, ATTN_NORM);
    }

    // Turn towards enemy and update model attachment
    // The offset is based on the nour facing forward
    ai_face();
    makevectors(self.angles);
    org = self.origin + attack_vector(bombofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.angles_x = random()*360;
    self.attachment.frame = bombframe;
    // Random explosion like particles from ball as it grows
    particle_explode(self.attachment.origin, 10+random()*10, 0.5+random(), PARTICLE_BURST_YELLOW, 0);
};

//----------------------------------------------------------------------
void() nour_attb1    =[ $shotb1, nour_attb2] {ai_face();
    self.pain_finished = time + 1.5;
};
void() nour_attb2    =[ $shotb2, nour_attb3] {ai_face();};
void() nour_attb3    =[ $shotb3, nour_attb4] {ai_face();};
void() nour_attb4    =[ $shotb4, nour_attb5] {ai_face();};
void() nour_attb5    =[ $shotb5, nour_attb6] {Setup_NourBomb('24 0 0',0);};
void() nour_attb6    =[ $shotb6, nour_attb7] {Setup_NourBomb('24 0 -2',1);};
void() nour_attb7    =[ $shotb7, nour_attb8] {Setup_NourBomb('24 0 -4',2);};
void() nour_attb8    =[ $shotb8, nour_attb9] {Setup_NourBomb('24 0 -8',3);};
void() nour_attb9    =[ $shotb9, nour_attb10] {Setup_NourBomb('24 0 -12',4);};
void() nour_attb10    =[ $shotb10, nour_attb11] {Setup_NourBomb('24 0 -16',5);};
void() nour_attb11    =[ $shotb11, nour_attb12] {Setup_NourBomb('24 0 -12',6);};
void() nour_attb12    =[ $shotb12, nour_attb13] {Fire_NourBomb('24 0 -12');};
void() nour_attb13    =[ $shotb13, nour_attb14] {};
void() nour_attb14    =[ $shotb14, nour_attb15] {};
void() nour_attb15    =[ $shotb15, nour_attb16] {};
void() nour_attb16    =[ $shotb16, nour_attb17] {ai_face();};
void() nour_attb17    =[ $shotb17, nour_run1] {ai_face();};

//======================================================================
// ATTACK C - Summon wizards around boss
//======================================================================
void() Spawn_NourFog =
{
    // Randomly pick from teleport sounds
    self.lip = random() * 5;
    if (self.lip < 1) self.noise = "misc/r_tele1.wav";
    else if (self.lip < 2) self.noise = "misc/r_tele2.wav";
    else if (self.lip < 3) self.noise = "misc/r_tele3.wav";
    else if (self.lip < 4) self.noise = "misc/r_tele4.wav";
    else self.noise = "misc/r_tele5.wav";
    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    
    // Show ID teleport particle effect
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_TELEPORT);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
};

//----------------------------------------------------------------------
// Fire random plasma spikes while summoning wizard minions
//----------------------------------------------------------------------
void(vector minofs) Fire_NourPlasma =
{
    local vector org;

    makevectors(self.angles);
    org = self.origin + attack_vector(minofs);
    self.pos2 = vecrand(0,50,TRUE);
    self.pos3 = normalize(self.pos2);
    self.attack_speed = SPEED_NOURSPIKE + (random() * SPEED_NOURSKILL);
    launch_plasma(org, self.pos3, CT_MONNOUR, self.attack_speed);
};

//----------------------------------------------------------------------
void(vector minofs) Spawn_NourMin =
{
    // Finished with bomb effect
    nour_finish_attachment();
    Fire_NourPlasma(minofs);
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    // Cycle through spawn targets and spawn Tfog + Wizard
    self.lip = self.count;
    while(self.lip > 0) {
        self.attachment3.think = Spawn_NourFog;
        self.attachment3.nextthink = time + 0.01 + random()*0.3;

        // If the spawn locaiton all clear, spawn something!
        self.pos1 = self.attachment3.origin;
        self.aflag = find_minionspace(self.pos1);            
        if (self.aflag == TRUE) {
            // Check for any spawnflags on spawn point
            if (random() < 0.5)
                minion_wizard(self.pos1, self.enemy, MON_WIZARD_ABOVE);
            else minion_wizard(self.pos1, self.enemy, 0);
        }
        
        // Next spawn point
        self.attachment3 = self.attachment3.entchain;
        self.lip = self.lip - 1;
    }
};

//----------------------------------------------------------------------
void(vector minofs, float minframe) Setup_NourMin =
{
    local vector org;
    if (self.health < 1) return;
    
    // Check if attachment has been setup yet
    if (!self.attachment) nour_create_attachment();
    
    // Frame 0 is start of the sequence (move everything into place)
    if (minframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.origin);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        self.attachment.skin = 1;

        // Start Spawning sound
        sound (self, CHAN_WEAPON, "nour/attack2.wav", 1, ATTN_NORM);
    }

    // The offset is based on the nour facing forward
    makevectors(self.angles);
    org = self.origin + attack_vector(minofs);
    setorigin(self.attachment, org);
    self.attachment.angles = self.angles;
    self.attachment.angles_x = random()*360;
    self.attachment.frame = minframe;
    // Slowly suck in particles to energy ball
    particle_implode(org, 20+random()*20, 75, 75, PARTICLE_BURST_BLUE);

    // Cycle through spawn targets and fire Lightning per frame
    self.lip = self.count;
    while(self.lip > 0) {
        // Traceline from spawn point to random direction
        self.pos1 = self.attachment3.origin - org;
        self.pos2 = vecrand(0,50,TRUE);
        self.pos3 = normalize(self.pos1 + self.pos2);
        traceline(org, org + self.pos3 * 600, FALSE, self);
        
        // Create lightning bolt from source to target
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity (MSG_BROADCAST, self.attachment3);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
        WriteCoord (MSG_BROADCAST, trace_endpos_x);
        WriteCoord (MSG_BROADCAST, trace_endpos_y);
        WriteCoord (MSG_BROADCAST, trace_endpos_z);
        
        // Play some arching lightning sounds
        if (random() < 0.5) {
            if (random() < 0.5) sound (self.attachment3, CHAN_BODY, "nour/elec_arch1.wav", 1, ATTN_NORM);
            else sound (self.attachment3, CHAN_BODY, "nour/elec_arch2.wav", 1, ATTN_NORM);
        }

        // Next spawn point
        self.attachment3 = self.attachment3.entchain;
        self.lip = self.lip - 1;
    }
    
    // Generate a random bolt of electricity
    Fire_NourPlasma(minofs);
};

//----------------------------------------------------------------------
void() nour_attc1    =[ $shotc1, nour_attc2] {ai_face();
    // Make sure boss mode active for this attack
    if (!(self.spawnflags & MON_NOUR_BOSS)) self.think = self.th_run;

    // Block all damage while summoning stuff
    Resist_ChangeType(self, TRUE);
    
    // Reset attack state so it does not keep spawning
    self.attack_state = AS_STRAIGHT;
    self.lip = self.count;
    while (self.lip > 0) {
        self.cnt = 4;    // Try 4 times, no infinite loops
        while (self.cnt > 0) {
            // Work out 2 empty spaces around Nour for minions
            self.pos2 = vecrand(0,200,TRUE);
            self.pos2_z = 16;    // Spawn higher than boss
            self.pos1 = self.origin + self.pos2;
            // Check for available space for mininon
            self.aflag = find_minionspace(self.pos1);
            self.cnt = self.cnt - 1;
            // Space is right, save origin
            if (self.aflag == TRUE) self.cnt = -10;
        }
        // Found space, store origin for later
        if (self.cnt == -10) self.attachment3.origin = self.pos1;
        else self.attachment3.origin = self.origin;
        
        // Next spawn point
        self.attachment3 = self.attachment3.entchain;
        self.lip = self.lip - 1;
    }
};
void() nour_attc2    =[ $shotc2, nour_attc3] {ai_face();};
void() nour_attc3    =[ $shotc3, nour_attc4] {ai_face();};
void() nour_attc4    =[ $shotc4, nour_attc5] {Setup_NourMin('24 0 -24',0);};
void() nour_attc5    =[ $shotc5, nour_attc6] {Setup_NourMin('24 0 -24',1);};
void() nour_attc6    =[ $shotc6, nour_attc7] {Setup_NourMin('24 0 -22',2);};
void() nour_attc7    =[ $shotc7, nour_attc8] {Setup_NourMin('24 0 -20',3);};
void() nour_attc8    =[ $shotc8, nour_attc9] {Setup_NourMin('24 0 -16',4);};
void() nour_attc9    =[ $shotc9, nour_attc10] {Setup_NourMin('24 0 -16',5);};
void() nour_attc10    =[ $shotc10, nour_attc11] {Setup_NourMin('24 0 -12',6);};
void() nour_attc11    =[ $shotc11, nour_attc12] {Setup_NourMin('24 0 -8',6);};
void() nour_attc12    =[ $shotc12, nour_attc13] {Setup_NourMin('24 0 -4',7);};
void() nour_attc13    =[ $shotc13, nour_attc14] {Setup_NourMin('24 0 -2',7);};
void() nour_attc14    =[ $shotc14, nour_attc15] {Spawn_NourMin('24 0 0');};
void() nour_attc15    =[ $shotc15, nour_attc16] {Fire_NourPlasma('24 0 0');};
void() nour_attc16    =[ $shotc16, nour_attc17] {};
void() nour_attc17    =[ $shotc17, nour_attc18] {};
void() nour_attc18    =[ $shotc18, nour_attc19] {};
void() nour_attc19    =[ $shotc19, nour_attc20] {};
void() nour_attc20    =[ $shotc20, nour_attc21] {};
void() nour_attc21    =[ $shotc21, nour_attc22] {};
void() nour_attc22    =[ $shotc22, nour_attc23] {ai_face();};
void() nour_attc23    =[ $shotc23, nour_run1] {ai_face();
    // Restore ammo resistance to default
    Resist_ChangeType(self, FALSE);
    self.style = NOUR_PHASE2;    // Fight mode
};

//============================================================================
// PAIN and DEATH
//============================================================================
void() nour_paina1 = [ $paina1, nour_paina2 ] {};
void() nour_paina2 = [ $paina2, nour_paina3 ] {};
void() nour_paina3 = [ $paina3, nour_paina4 ] {};
void() nour_paina4 = [ $paina4, nour_paina5 ] {};
void() nour_paina5 = [ $paina5, nour_paina6 ] {};
void() nour_paina6 = [ $paina6, nour_run1 ] {};

//----------------------------------------------------------------------
void() nour_painb1 = [ $painb1, nour_painb2 ] {};
void() nour_painb2 = [ $painb2, nour_painb3 ] {};
void() nour_painb3 = [ $painb3, nour_painb4 ] {};
void() nour_painb4 = [ $painb4, nour_painb5 ] {};
void() nour_painb5 = [ $painb5, nour_painb6 ] {};
void() nour_painb6 = [ $painb6, nour_painb7 ] {};
void() nour_painb7 = [ $painb7, nour_painb8 ] {};
void() nour_painb8 = [ $painb8, nour_painb9 ] {};
void() nour_painb9 = [ $painb9, nour_run1 ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) nour_pain =
{
    // Finish with all attachments
    nour_finish_attachment();

    if (self.spawnflags & MON_NOUR_BOSS) {
        // Check for boss wave trigger events
        if (nour_WaveCheck() == TRUE) {self.th_jump(); return;}
    }

    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1 || self.pain_check == 2) {
            // Randomly pick which pain animation to play
            if (random() < 0.8) nour_paina1 ();        // Short recoil right
            else {
                nour_painb1 ();                        // Long recoil left
                self.pain_finished = time + 2;        // long animation
            }
        }
    }
};

//============================================================================
// Death comes to us all, even Nour!
//============================================================================
void() nour_explode =
{
    // Check for any final trigger events
    if (self.message2 != "") trigger_strs(self.message2,self);
    
    // No more Nour!
    entity_hide(self);
    
    // Blue ID particle explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 35);
    WriteByte (MSG_BROADCAST, 8);
    // Check for any death radius damage
    if (self.death_dmg > 0) 
        T_RadiusDamage (self, self, self.death_dmg, world, DAMAGEALL);
    
    // Classic sprite/DP explosion
    SpawnExplosion(EXPLODE_PLASMA_BIG, self.origin, "nour/explode_death.wav");

    // Gib explosive fountain!?!
    self.max_health = MON_GIBEXPLOSION;
    ThrowGib(11, 2);    // Left Arm
    ThrowGib(12, 2);    // Right Arm
    ThrowGib(13, 1);    // Tail
    ThrowGib(4, 10 + rint(random()*3));
    ThrowGib(5, 10 + rint(random()*3));
};

//----------------------------------------------------------------------
void() nour_die1 = [ $death1, nour_die2 ] {};
void() nour_die2 = [ $death2, nour_die3 ] {};
void() nour_die3 = [ $death3, nour_die4 ] {self.solid = SOLID_NOT;};
void() nour_die4 = [ $death4, nour_die5 ] {};
void() nour_die5 = [ $death5, nour_die6 ] {};
void() nour_die6 = [ $death6, nour_die7 ] {};
void() nour_die7 = [ $death7, nour_die8 ] {};
void() nour_die8 = [ $death8, nour_die9 ] {self.lip = 0;};
void() nour_die9 = [ $death9, nour_die9 ] {
    // Spawn implosion effect
    if (random() < 0.5)
        particle_implode(self.origin, 20+random()*20, 100, 100, PARTICLE_BURST_BLUE);
    // Spawn particle smoke and explosive smoke
    else {
        SpawnProjectileSmoke(self.origin, 150, 100, 300);
        particle_dust(self.origin, 10+random()*10, PARTICLE_BURST_BLUE);
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
            if (random() < 0.5) sound (self, CHAN_BODY, "nour/elec_arch1.wav", 1, ATTN_NORM);
            else sound (self, CHAN_BODY, "nour/elec_arch2.wav", 1, ATTN_NORM);
        }
    }
    // Check for a random bolt of plasma
    if (random() < 0.2) Fire_NourPlasma('0 0 -16');
    
    // Cycle through end phase
    self.lip = self.lip + 1;
    if (self.lip == 26) sound (self, CHAN_AUTO, "nour/implode_death.wav", 1, ATTN_NONE);
    if (self.lip == 40) nour_explode();
};

//----------------------------------------------------------------------
void() nour_die =
{
    self.deadflag = DEAD_DEAD;    // Nour bites the dust
    self.effects = 0;            // Remove effects on death
    nour_finish_attachment();    // Remove any attachments
    nour_remove_attachment();

    sound (self, CHAN_VOICE, "nour/death.wav", 1, ATTN_NORM);
    self.velocity_x = -200 + 400*random();
    self.velocity_y = -200 + 400*random();
    self.velocity_z = 100 + 100*random();
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    nour_die1 ();
};

//======================================================================
// REVEAL PHASE (coming out of the ground)
//======================================================================
void() nour_revealframe = 
{
    // Beginning of animation block
    if (self.walkframe == 3) 
        sound (self, CHAN_BODY, "nour/reveal.wav", 1, ATTN_NORM);
    else if (self.walkframe == 23)
        sound (self, CHAN_VOICE, "nour/idle1.wav", 1, ATTN_NORM);
    
    // Work out remaining distance to go
    self.movedir = self.movetarget2.origin - self.origin;
    self.t_length = vlen(self.movedir);
    // Calc initial velocity boast (burst action)
    self.lip = (self.pausetime - time) / 3.6;
    self.t_width = 1 + (self.lip/2);

    // If time or distance too small, stop moving
    if (self.lip < 0.1 || self.t_length < 2) {
        self.velocity = '0 0 0';
    }
    // Update velocity every frame
    else self.velocity = self.movedir * self.t_width;
    
    // Turn towards player
    if (self.enemy) ai_face();

    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    self.nextthink = time + 0.1;
    // Time to exit reveal?
    if (self.walkframe > 35) {
        self.walkframe = 0;                // Reset just in case
        self.height = MONAI_ABOVEDIST;    // Enemytarget distance above
        self.movetype = MOVETYPE_STEP;    // back to regular movement
        self.takedamage = DAMAGE_AIM;    // Can take damage
        self.style = NOUR_PHASE2;        // Time to fight!
        Resist_ChangeType(self,FALSE);    // restore resistance
        self.think = nour_run1;            // Straight to running
        self.frame = $fly1;
        FoundHuntTarget(FALSE);            // make sure tracking player
    }
    else {
        self.think = nour_revealframe;
        // Setup current animation frame
        self.frame = $reveal1 + self.walkframe;
    }
};

//----------------------------------------------------------------------
void() nour_reveal1    = {
    self.pausetime = time + 3.6;
    self.walkframe = 0;
    nour_revealframe();
};

//======================================================================
// Create wizard minion entity chain
// Entities are created so that the spawning space can be tested
// before spawning the minions for possible collisions
//======================================================================
float() nour_minionsetup =
{
    local entity min_prev, min_first;
    
    self.lip = self.count;
    min_prev = min_first = world;
    
    // Cycle through minion list
    while(self.lip > 0) {
        newmis = spawn();
        newmis.origin = self.origin;
        newmis.owner = self;
        // Is this the first pass through loop?
        if (!min_first) min_first = newmis;
        // Any previous entities created?
        if (!min_prev) min_prev = newmis;
        else {
            // Link previous to current entity
            // and move previous forward in chain
            min_prev.entchain = newmis;
            min_prev = newmis;
        }
        // Keep on looping
        self.lip = self.lip - 1;
    }
    // Close loop
    if (min_first) {
        newmis.entchain = min_first;
        self.attachment3 = min_first;
        return FALSE;
    }
    else return TRUE;
};

//======================================================================
// Setup Nour after trigger event
//======================================================================
void() nour_awake =
{
    // make sure boss mode selected
    if (!(self.spawnflags & MON_NOUR_BOSS)) return;

    self.use = SUB_Null;            // No more triggers
    self.style = NOUR_PHASE1;        // Bursting intro sequence
    self.flags = FL_MONSTER | FL_FLY;    // Reset flag
    monster_bbox();                    // Setup bounding box

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_FLY;
    setmodel(self, self.mdl);                    // Setup model
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size
    
    self.takedamage = DAMAGE_NO;    // Still immune to damage
    self.yaw_speed = 20;            // Average Speed
    self.velocity = '0 0 0';        // Make sure stationary
    self.pain_longanim = FALSE;        // No axe advantage
    self.noinfighting = TRUE;        // No infighting
    self.count = skill+1;            // Total amount of minions
    // skill modifier : Easy = 1, Normal = 2, Hard/NM = 3
    if (self.count > 3) self.count = 3;
    // Setup minion chain entity system
    if (nour_minionsetup()) {
        dprint("\b[NOUR]\b Cannot create minion chain!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }
        
    //Resist_CheckRange(self);        // Double check values
    Resist_Save(self);                // Save for Later
    Resist_ChangeType(self,TRUE);    // resist everything

    // Reset all combat flags and counters
    self.deadflag = DEAD_NO;
    self.liquidbase = self.liquidcheck = 0;
    self.dmgcombined = self.dmgtimeframe = 0;

    // Setup boss waves and overall health
    self.bosswave = 1;
    self.bosswavetotal = 4;
    self.bosswaveqty = 500;
    self.max_health = self.bosswavetotal * self.bosswaveqty;
    // Setup boss wave HP + trigger event
    nour_WaveSetupHP();

    self.pain_finished = time + 3;        // Make pain go away
    self.attack_finished = time + 2;    // Reset attack system
    
    // Setup player focus
    if (activator.flags & FL_CLIENT) {
        self.goalentity = self.enemy = activator;
    }
    else self.enemy = world;

    // Trigger all spawning events
    trigger_strs(self.message, self);
    self.message = "";
    
    // Find spawning (move) destination
    self.movetarget2 = find(world,targetname,self.message2);
    if (!self.movetarget2) {
        dprint("\b[NOUR]\b Missing spawn destination!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }
    
    // Make sure all death triggers are setup ready
    self.message2 = self.target;
    self.target = self.target2 = self.deathtarget = "";

    // Check for tether system (special target field)
    if (self.tethertarget == "") {
        dprint("\b[NOUR]\b Missing tether marker!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }
    // Find the tether marker (must be path_corner)
    self.movelast = find(world,targetname,self.tethertarget);
    if (self.movelast.classtype != CT_PATHCORNER) {
        dprint("\b[NOUR]\b Tether marker not path_corner!\n");
        spawn_marker(self.movelast.origin, SPNMARK_YELLOW);
        self.movelast = world;
    }
    // Make sure tethertarget is blank
    self.tethertarget = "";
    
    // Time to rise from the ground
    nour_reveal1();
};

/*======================================================================
 QUAKED monster_nouronihar (1 0 0) (-16 -16 -24) (16 16 40)
======================================================================*/
void() monster_nour =
{
    if (deathmatch) { remove(self); return; }
    
    self.mdl = "progs/mon_bossnour.mdl";
    self.headmdl = "progs/h_wizard.mdl";
    self.gib1mdl = "progs/gib_wzarm1.mdl";    // Left arm/stump
    self.gib2mdl = "progs/gib_wzarm2.mdl";    // Right arm/stump
    self.gib3mdl = "progs/gib_wztail.mdl";    // Tail section
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_WIZ);    // Originally progs/w_spike.mdl
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (MODEL_PROJ_NOUR1);    // Spit
    precache_model (MODEL_PROJ_NOUR2B);    // Growing ball
    precache_model (MODEL_PROJ_NOUR2P);    // Warping projectile
    precache_model (MODEL_PROJ_NOUR2S);    // Explosive pieces
    precache_model (MODEL_PROJ_NOUR3);    // Lightning bolt
    precache_model (SBURST_POISON);        // Poison Burst

    // sight/pain/death sounds
    self.idle_sound = "nour/idle1.wav";
    self.pain_sound = "nour/pain.wav";
    self.sight_sound = "nour/sight.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.pain_sound);
    precache_sound (self.sight_sound);
    precache_sound ("nour/reveal.wav");

    // Final death sequence
    precache_sound ("nour/death.wav");
    precache_sound ("nour/explode_death.wav");
    precache_sound ("nour/elec_arch1.wav");
    precache_sound ("nour/elec_arch2.wav");
    precache_sound ("nour/implode_death.wav");

    // Attack A - Wizard style spit
    // Attack B - Large explosive ball
    precache_sound ("nour/attack1.wav");
    precache_sound ("nour/attack2.wav");
    precache_sound ("nour/explode2.wav");
    precache_sound ("nour/bounce.wav");
    
    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    self.bboxtype = BBOX_WIDE;        // Wraith Body
    self.bossflag = TRUE;            // Boss flag (like FL_MONSTER)        

    self.gibhealth = MON_NEVERGIB;    // Cannot be gibbed by weapons
    self.gibbed = FALSE;            // Starts complete
    self.pain_flinch = 200;            // Strong Wizard level
    self.pain_timeout = 2;            // Stop constant pain
    self.infightextra = 8;            // Does not like infighting
    self.steptype = FS_FLYING;        // No foot sound
    self.blockudeath = TRUE;        // No player gib sound
    self.pain_longanim = FALSE;        // cannot be chopped with shadow axe
    self.no_liquiddmg = TRUE;        // no slime/lava damage
    if (self.height < 1) self.height = MONAI_ABOVEDIST; // Custom height
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_NOUR;
    self.poisonous = FALSE;            // Cannot be poisonous
    self.style = NOUR_PHASE0;        // No waves setup
    self.deathstring = " was picked apart by the Scragmother\n";

    // Setup Ammo Resistance
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = 0; self.resist_cells = 1;
    self.reflectlightning = TRUE;    // Reflect lightning strikes
    self.reflectplasma = TRUE;        // Reflect plasma projectiles

    // Restore all think functions
    self.th_checkattack = NourCheckAttack;
    self.th_stand = nour_stand1;
    self.th_walk = nour_walk1;
    self.th_run = nour_run1;
    self.th_slide = nour_slide1;
    self.th_melee = nour_atta1;
    self.th_missile = nour_attb1;
    self.th_jump = nour_attc1;
    self.th_pain = nour_pain;
    self.th_die = nour_die;
        
    self.classtype = CT_MONNOUR;
    self.classgroup = CG_WIZARD;
    self.classmove = MON_MOVEFLY;
    
    // Check for boss version of Nouronihar?
    if (self.spawnflags & MON_NOUR_BOSS) {
        // Reset spawnflag, other values not required
        self.spawnflags = MON_NOUR_BOSS;
        self.health = self.max_health = MEGADEATH;
        self.pain_finished = LARGE_TIMER;        
        self.takedamage = DAMAGE_NO;    // Immune to damage
        
        // No targetname = no trigger!
        if (self.targetname == "") {
            dprint("\b[NOUR]\b Missing targetname name!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            return;
        }
        // Missing spawn trigger
        if (self.message == "") {
            dprint("\b[NOUR]\b Missing spawn (message) trigger!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            return;
        }
        // Missing rise path corner
        if (self.message2 == "") {
            dprint("\b[NOUR]\b Missing spawn (message2) path corner!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            return;
        }

        total_monsters = total_monsters + 1;
        self.use = nour_awake;
    }
    else {
        // Regular monster with large HP
        if (self.health < 1) self.health = 1200;
        // Regular Nour is not 100% resistant
        self.resist_cells = 0.75;
        monster_start();
    }
};
