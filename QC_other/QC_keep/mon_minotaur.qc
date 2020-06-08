/*==============================================================================
MINOTAUR (Hexen2 model by Raven Software) (The Shuffler from Marcher MOD)
also known as "The Nethergoat: Spawnmaster of the Elder World"
==============================================================================*/

// Slap RIGHT - 7, 9, 11, 13, 15, 17
// Slap LEFT  - 19, 21, 23, 1, 3, 5
$frame bwalk1 bwalk2 bwalk3 bwalk4 bwalk5 bwalk6 bwalk7 bwalk8
$frame bwalk9 bwalk10 bwalk11 bwalk12 bwalk13 bwalk14 bwalk15 bwalk16
$frame bwalk17 bwalk18 bwalk19 bwalk20 bwalk21 bwalk22 bwalk23 bwalk24

// (024) Charging - Head down
$frame charge1 charge2 charge3 charge4 charge5 charge6 charge7 charge8
$frame charge9 charge10 charge11 charge12

// (036) Fall forward onto knees
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 death22 death23 death24
$frame death25

// (061) Charging - swiping arm and biting
$frame gore1 gore2 gore3 gore4 gore5 gore6 gore7 gore8
$frame gore9 gore10 gore11 gore12

// (073) Stationary - howl at the moon
$frame howl1 howl2 howl3 howl4 howl5 howl6 howl7 howl8
$frame howl9 howl10 howl11 howl12 howl13 howl14 howl15 howl16
$frame howl17 howl18 howl19 howl20 howl21 howl22 howl23 howl24
$frame howl25 howl26 howl27 howl28 howl29 howl30 howl31 howl32
$frame howl33 howl34 howl35 howl36

// (109) Small leap/jump
$frame jump1 jump2 jump3 jump4 jump5 jump6 jump7 jump8
$frame jump9 jump10 jump11 jump12 jump13 jump14 jump15 jump16
$frame jump17 jump18 jump19 jump20 jump21 jump22 jump23 jump24

// (133) Quick pain flinch backward
$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8

// (141) Stationary - shake head/arms
$frame shake1 shake2 shake3 shake4 shake5 shake6 shake7 shake8
$frame shake9 shake10 shake11 shake12 shake13 shake14 shake15 shake16
$frame shake17 shake18 shake19 shake20

// (161) Stationary - fire projectile
$frame magic1 magic2 magic3 magic4 magic5 magic6 magic7 magic8
$frame magic9 magic10 magic11 magic12 magic13 magic14 magic15 magic16
$frame magic17 magic18 magic19 magic20 magic21 magic22 magic23 magic24
$frame magic25 magic26 magic27 magic28 magic29 magic30 magic31 magic32

// Looks terrible, not used
// (193) Slide sideways (LEFT)
// (198) Slide sideways (RIGHT)
$frame slideL1 slideL2 slideL3 slideL4 slideL5
$frame slideR1 slideR2 slideR3 slideR4 slideR5

// (203) Idle/wait
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8
$frame stand9 stand10 stand11 stand12 stand13 stand14 stand15 stand16
$frame stand17 stand18 stand19 stand20 stand21 stand22 stand23 stand24

// (227) Walking
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16
$frame walk17 walk18 walk19 walk20 walk21 walk22 walk23 walk24

//============================================================================
void() mino_stand1 =[ $stand1, mino_stand2 ] {monster_idle_sound();ai_stand();};
void() mino_stand2 =[ $stand3, mino_stand3 ] {ai_stand();};
void() mino_stand3 =[ $stand5, mino_stand4 ] {ai_stand();};
void() mino_stand4 =[ $stand7, mino_stand5 ] {ai_stand();};
void() mino_stand5 =[ $stand9, mino_stand6 ] {ai_stand();};
void() mino_stand6 =[ $stand11, mino_stand7 ] {ai_stand();};
void() mino_stand7 =[ $stand13, mino_stand8 ] {ai_stand();};
void() mino_stand8 =[ $stand15, mino_stand9 ] {ai_stand();};
void() mino_stand9 =[ $stand17, mino_stand10 ] {ai_stand();};
void() mino_stand10 =[ $stand19, mino_stand11 ] {ai_stand();};
void() mino_stand11 =[ $stand21, mino_stand12 ] {ai_stand();};
void() mino_stand12 =[ $stand23, mino_stand1 ] {ai_stand();};

//============================================================================
void() mino_walk1 =[ $walk1, mino_walk2 ] {monster_idle_sound();ai_walk(12);};
void() mino_walk2 =[ $walk3, mino_walk3 ] {ai_walk(12);};
void() mino_walk3 =[ $walk5, mino_walk4 ] {ai_walk(9);};
void() mino_walk4 =[ $walk7, mino_walk5 ] {ai_walk(10);monster_footstep(FALSE);};
void() mino_walk5 =[ $walk9, mino_walk6 ] {ai_walk(4);};
void() mino_walk6 =[ $walk11, mino_walk7 ] {ai_walk(9);};
void() mino_walk7 =[ $walk13, mino_walk8 ] {ai_walk(12);};
void() mino_walk8 =[ $walk15, mino_walk9 ] {ai_walk(12);};
void() mino_walk9 =[ $walk17, mino_walk10 ] {ai_walk(9);};
void() mino_walk10 =[ $walk19, mino_walk11 ] {ai_walk(10);monster_footstep(FALSE);};
void() mino_walk11 =[ $walk21, mino_walk12 ] {ai_walk(4);};
void() mino_walk12 =[ $walk23, mino_walk1 ] {ai_walk(9);};

//============================================================================
// Charge/run/gore!
//============================================================================
void() mino_run;

void() mino_rage1 =[ $charge2, mino_rage2 ] {monster_idle_sound();ai_run(34);};
void() mino_rage2 =[ $charge4, mino_rage3 ] {ai_run(28);monster_footstep(FALSE);};
void() mino_rage3 =[ $charge6, mino_rage4 ] {ai_run(26);};
void() mino_rage4 =[ $charge8, mino_rage5 ] {ai_run(34);};
void() mino_rage5 =[ $charge10, mino_rage6 ] {ai_run(28);monster_footstep(FALSE);};
void() mino_rage6 =[ $charge12, mino_run ] {ai_run(26);};

//----------------------------------------------------------------------
void() mino_run1 =[ $walk1, mino_run2 ] {monster_idle_sound();ai_run(12);};
void() mino_run2 =[ $walk3, mino_run3 ] {ai_run(12);};
void() mino_run3 =[ $walk5, mino_run4 ] {ai_run(9);};
void() mino_run4 =[ $walk7, mino_run5 ] {ai_run(10);monster_footstep(FALSE);};
void() mino_run5 =[ $walk9, mino_run6 ] {ai_run(4);};
void() mino_run6 =[ $walk11, mino_run7 ] {ai_run(9);};
void() mino_run7 =[ $walk13, mino_run8 ] {ai_run(12);};
void() mino_run8 =[ $walk15, mino_run9 ] {ai_run(12);};
void() mino_run9 =[ $walk17, mino_run10 ] {ai_run(9);};
void() mino_run10 =[ $walk19, mino_run11 ] {ai_run(10);monster_footstep(FALSE);};
void() mino_run11 =[ $walk21, mino_run12 ] {ai_run(4);};
void() mino_run12 =[ $walk23, mino_run ] {ai_run(9);};

//----------------------------------------------------------------------
void() mino_run =
{
    if (self.attack_rage) mino_rage1();
    else mino_run1();
};

//----------------------------------------------------------------------
// Short howl animation to switch to rage mode
//----------------------------------------------------------------------
void() mino_howl1 =[ $howl1, mino_howl2 ] {
    sound (self, CHAN_WEAPON, "minotaur/attack1long.wav", 1, ATTN_NORM);};
void() mino_howl2 =[ $howl3, mino_howl3 ] {};
void() mino_howl3 =[ $howl5, mino_howl4 ] {};
void() mino_howl4 =[ $howl7, mino_howl5 ] {};
void() mino_howl5 =[ $howl9, mino_howl6 ] {};
void() mino_howl6 =[ $howl19, mino_howl7 ] {};
void() mino_howl7 =[ $howl21, mino_howl8 ] {};
void() mino_howl8 =[ $howl23, mino_howl9 ] {};
void() mino_howl9 =[ $howl25, mino_howl10 ] {};
void() mino_howl10 =[ $howl27, mino_howl11 ] {};
void() mino_howl11 =[ $howl29, mino_howl12 ] {};
void() mino_howl12 =[ $howl31, mino_run ] {};

//============================================================================
// MELEE ATTACK (claws)
//============================================================================
void(float side) mino_claw =
{
    local float ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;
    ai_face ();                        // Turn towards enemy target
    ai_damagebreakable(50);            // Damage any breakables

    if (!ai_checkmelee(MONAI_MELEEMINOTAUR) || !self.enemy.takedamage) {
        // Melee claw miss sound
        sound (self, CHAN_WEAPON, "minotaur/swipe.wav", 1, ATTN_NORM);
    }
    else {
        // Melee claw hit sound
        if (self.lefty) sound (self, CHAN_WEAPON, "minotaur/strike1.wav", 1, ATTN_NORM);
        else sound (self, CHAN_WEAPON, "minotaur/strike2.wav", 1, ATTN_NORM);
        self.lefty = 1 - self.lefty;
        ldmg = 12 + 6*random();
        // Rage attack is once per animation frame, more deadly
        if (self.attack_rage) ldmg = ldmg*2;
        T_Damage (self.enemy, self, self, ldmg, DAMARMOR);        
        SpawnMeatSpray (self, self.enemy, side);
    }
};

//----------------------------------------------------------------------
// Slap LEFT  - 19, 21, 23, 1, 3, 5
// Slap RIGHT - 7, 9, 11, 13, 15, 17
void() mino_slapL1 =[ $bwalk19, mino_slapL2 ] {ai_charge(10);monster_footstep(FALSE);};
void() mino_slapL2 =[ $bwalk21, mino_slapL3 ] {ai_charge(4);};
void() mino_slapL3 =[ $bwalk23, mino_slapL4 ] {ai_charge(9);};
void() mino_slapL4 =[ $bwalk1, mino_slapL5 ] {ai_charge(12);mino_claw(-250);};
void() mino_slapL5 =[ $bwalk3, mino_slapL6 ] {ai_charge(12);};
void() mino_slapL6 =[ $bwalk5, mino_slapR1 ] {ai_charge(9);};

void() mino_slapR1 =[ $bwalk7, mino_slapR2 ] {ai_charge(10);monster_footstep(FALSE);};
void() mino_slapR2 =[ $bwalk9, mino_slapR3 ] {ai_charge(4);};
void() mino_slapR3 =[ $bwalk11, mino_slapR4 ] {ai_charge(9);};
void() mino_slapR4 =[ $bwalk13, mino_slapR5 ] {ai_charge(12);mino_claw(250);};
void() mino_slapR5 =[ $bwalk15, mino_slapR6 ] {ai_charge(12);};
void() mino_slapR6 =[ $bwalk17, mino_run ] {ai_charge(9);
    // Check if enemy is close, alive and visible before doing more melee
    if (random() < 0.5 && ai_checkmelee(MONAI_MELEEMINOTAUR) && 
        self.enemy.health > 0 && visible(self.enemy)) self.think = mino_slapL1;
};

//----------------------------------------------------------------------
// Rage melee attack
void() mino_gore1 =[ $gore2, mino_gore2 ] {ai_charge(34);};
void() mino_gore2 =[ $gore4, mino_gore3 ] {ai_charge(28);monster_footstep(FALSE);};
void() mino_gore3 =[ $gore6, mino_gore4 ] {ai_charge(26);mino_claw(-250);};
void() mino_gore4 =[ $gore8, mino_gore5 ] {ai_charge(34);};
void() mino_gore5 =[ $gore10, mino_gore6 ] {ai_charge(28);monster_footstep(FALSE);};
void() mino_gore6 =[ $gore12, mino_run ] {ai_charge(26);};

//----------------------------------------------------------------------
void() mino_melee =
{
    // Switch to a faster more deadly melee attack if in rage mode
    if (self.attack_rage) mino_gore1();
    else {
        // Randomly start left or right and then loop
        if (random() < 0.5) mino_slapR1();
        else mino_slapL1();
    }
};

//============================================================================
// JUMP ATTACK - pre-rage only
//============================================================================
void() mino_JumpTouch =
{
    local float ldmg;

    if (self.health < 1) return;
    ai_jumpbreakable(30);            // Damage any breakables
    self.touch = SUB_Null;            // No more touching
    self.count = self.count + 1;    // Total amount of touch jumps
    self.think = self.th_jumpexit;    // Exit frame

    // Keep track of how many times touched the same object
    if (self.jumptouch == other) self.jump_flag = time + MONAI_JUMPTIMEOUT;
    self.jumptouch = other;            // Keep track of touch target

    if ( CanDamage(other, self) ) {
        if ( vlen(self.velocity) > 300 ) {
          ldmg = 20 + 10*random();
            T_Damage (other, self, self, ldmg, DAMARMOR);    
            spawn_touchblood (self, self.enemy, ldmg*3);
        }
    }

    // Is the minotaur floating in the air?
    if (!checkbottom(self)) {
        // Is the minotaur standing on something?        
        if (self.flags & FL_ONGROUND) {    
            // Do an extra jump if got the count
            if (self.count < 2)  self.think = self.th_jump;
        }
    }

    // Next timer
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() mino_jump1 =[ $jump1, mino_jump2 ] {
    self.jump_flag = time + MONAI_JUMPMINOTIME;
    // Use combat idle grunt sounds (can get annoying otherwise)
    if (random() < 0.5) sound (self, CHAN_VOICE, self.idle_soundcom, 1, ATTN_NORM);
    else sound (self, CHAN_VOICE, self.idle_soundcom2, 1, ATTN_NORM);
};
void() mino_jump2 =[ $jump3, mino_jump3 ] {ai_face();};
void() mino_jump3 =[ $jump5, mino_jump4 ] {ai_face();};
void() mino_jump4 =[ $jump7, mino_jump5 ] {
    ai_face();
    self.jump_flag = time + MONAI_JUMPMINOTIME;
    self.touch = mino_JumpTouch;
    makevectors (self.angles);
    self.velocity = v_forward * 450 + '0 0 250';
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.oldorigin = self.origin;
};

// Flying through the air waiting to touch something!
void() mino_jump5 =[ $jump9, mino_jump6 ] {ai_face();};
void() mino_jump6 =[ $jump11, mino_jump7 ] {};
void() mino_jump7 =[ $jump13, mino_jump8 ] {ai_face();};
void() mino_jump8 =[ $jump15, mino_jump8 ] {
    // Double check monster is still falling?
    if (CheckZeroVector(self.velocity) || self.oldorigin == self.origin) {
        self.ideal_yaw = random() * 360;     //random jump angle
        self.think = mino_jump4;            // Keep jumping
    }
    self.oldorigin = self.origin;
};
//----------------------------------------------------------------------
void() mino_jump10 =[ $jump19, mino_jump12 ] {ai_charge(8);monster_footstep(FALSE);};
void() mino_jump12 =[ $jump23, mino_run ] {ai_charge(10);ai_resetangles();
    // If close enough start melee ELSE return to run cycle
    if (ai_checkmelee(MONAI_MELEEMINOTAUR) && self.enemy.health > 0) 
        self.think = mino_melee;
};

//============================================================================
// RANGE ATTACK - Plasma Bolts (rocket)
//============================================================================
void() minotaur_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = MODEL_PROJ_MBALL;
    }
    if (!self.attachment2) {
        self.attachment2 = spawn();
        self.attachment2.owner = self;
        self.attachment2.classtype = CT_ATTACHMENT;
        self.attachment2.mdl = MODEL_PROJ_MBALL;
    }
};

//----------------------------------------------------------------------
void() minotaur_finish_attachment =
{
    if (self.attachment) {
        setmodel(self.attachment, "");
        self.attachment.state = STATE_OFF;
    }
    if (self.attachment2) {
        setmodel(self.attachment2, "");
        self.attachment2.state = STATE_OFF;
    }
};

//----------------------------------------------------------------------
void() minotaur_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
    if (self.attachment2) {
        self.attachment2.think = SUB_Remove;
        self.attachment2.nextthink = time + 0.1;
    }
};

//----------------------------------------------------------------------
void(entity handent, vector orgofs, float dbframe) minotaur_handball =
{
    local vector org;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (dbframe == 0) {
        handent.state = STATE_ON;
        setorigin(handent, self.origin);
        setmodel(handent, handent.mdl);
        setsize (handent, VEC_ORIGIN, VEC_ORIGIN);
        handent.movetype = MOVETYPE_NONE;
        handent.solid = SOLID_NOT;
        handent.skin = self.exactskin;
        handent.alpha = 0.85;
    }
    
    // Generate attachment in hand (left)
    // makevectors set in previous function (minotaur_hands)
    org = self.origin + attack_vector(orgofs);
    setorigin(handent, org);
    handent.angles_y = rint(random()*359);
    handent.frame = dbframe;
};

//----------------------------------------------------------------------
// There is a ball for each hand with different origins
//----------------------------------------------------------------------
void(vector leftofs, vector rightofs, float dbframe) minotaur_hands =
{
    if (self.health < 1) return;
    
    // Keep turning towards enemy
    ai_face();
    makevectors(self.angles);
    
    // Generate a ball in each hand
    minotaur_handball(self.attachment, leftofs, dbframe);
    minotaur_handball(self.attachment2, rightofs, dbframe);
};

//----------------------------------------------------------------------
// Fire a volley of plasma/poison bolts at the enemy
//----------------------------------------------------------------------
void(vector orgofs, float angofs) minotaur_fire =
{
    local vector org, dir;

    if (!self.enemy) return;
    if (self.health < 1) return;

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_MINOBOLT + (skill * SPEED_MINOSKILL);
    makevectors(self.angles);
    org = self.origin + attack_vector(orgofs);
    dir = (self.enemy.origin - org) + (v_right*(angofs*30));
    dir = normalize (dir);

    // The spawning minotaur has poison spikes instead
    if (self.spawnflags & MON_MINOTAUR_MINIONS)
        launch_projectile(org, dir, CT_PROJ_MPOISON, self.attack_speed);
    else {
        // Projectile behaves like plasma (light/trail/impact effects)
        // The correct projectile model is switched in launch function
        launch_plasma(org, dir, CT_PROJ_PLASMA, self.attack_speed);
    }
};

//----------------------------------------------------------------------
// Main projectile plasma/poison attack of both types of minotaur
//----------------------------------------------------------------------
void() mino_magic1 =[ $magic1, mino_magic2 ] {ai_face();
    if (random() < 3) sound (self, CHAN_BODY, "minotaur/attack1start.wav", 1, ATTN_NORM);};
void() mino_magic2 =[ $magic3, mino_magic3 ] {minotaur_hands('34 0 44','-20 30 20',0);};
void() mino_magic3 =[ $magic5, mino_magic4 ] {minotaur_hands('30 -12 64','-20 44 28',1);};
void() mino_magic4 =[ $magic7, mino_magic5 ] {minotaur_hands('15 -16 88','-10 56 48',2);};
void() mino_magic5 =[ $magic9, mino_magic6 ] {minotaur_hands('0 -40 80','-10 48 76',3);};
void() mino_magic6 =[ $magic11, mino_magic7 ] {minotaur_hands('16 -36 72','-12 44 80',4);};
void() mino_magic7 =[ $magic13, mino_magic8 ] {minotaur_hands('36 -20 56','24 32 68',5);};

void() mino_magic8 =[ $magic15, mino_magic9 ] {
    sound (self, CHAN_WEAPON, "minotaur/bolt_fire.wav", 1, ATTN_NORM);
    minotaur_hands('40 -3 46','40 1 44',6);minotaur_fire('40 0 45',-2);};
void() mino_magic9 =[ $magic17, mino_magic10 ] {ai_back(8);
    minotaur_hands('32 -2 44','32 2 44',5);minotaur_fire('32 0 44',-1);};
void() mino_magic10 =[ $magic19, mino_magic11 ] {ai_back(4);
    minotaur_hands('20 0 48','20 2 46',4);minotaur_fire('20 0 47',0);};
void() mino_magic11 =[ $magic21, mino_magic12 ] {ai_forward(4);monster_footstep(FALSE);
    minotaur_hands('28 0 50','28 2 48',3);minotaur_fire('28 0 49',1);};
void() mino_magic12 =[ $magic23, mino_magic13 ] {ai_forward(8);
    minotaur_hands('44 0 46','44 2 44',2);minotaur_fire('44 0 45',2);};

void() mino_magic13 =[ $magic25, mino_magic14 ] {
    // Remove hand attachments and setup particle explosion
    // Create a cloud of particles which slowly raise upwards
    minotaur_finish_attachment();
    ai_face();
    makevectors(self.angles);
    self.finalangle = self.origin + (v_forward * 44) + (v_up * 32);
    // Spawn a mist of blue/green particles where projectiles came from
    particle_explode(self.finalangle, 50, 3, self.part_style, PARTICLE_BURST_MINOTAUR);
    // Random chance of final growl at player
    if (random() < 0.3) sound (self, CHAN_BODY, "minotaur/attack1end.wav", 1, ATTN_NORM);
};
void() mino_magic14 =[ $magic27, mino_magic15 ] {};
void() mino_magic15 =[ $magic29, mino_magic16 ] {ai_face();};
void() mino_magic16 =[ $magic31, mino_run ] {ai_face();};

//----------------------------------------------------------------------
// Spawn particles from body that float upwards during summons
//----------------------------------------------------------------------
void() mino_sumeffect = 
{ particle_debuff(self.origin+'0 0 64', 16, rint(4+random()*4), PARTICLE_BURST_GREEN);};

//----------------------------------------------------------------------
// Display a green ball where the gargoyle is spawning
//----------------------------------------------------------------------
void(float dbframe) mino_sumball =
{
    // Double check the space checks worked
    if (self.aflag == FALSE) return;
    
    // Frame 0 is start of the sequence (move everything into place)
    if (dbframe == 0) {
        self.attachment.state = STATE_ON;
        setorigin(self.attachment, self.pos2);
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        self.attachment.skin = self.exactskin;
    }
    
    // rotate and fade the ball as it grows
    self.attachment.angles_y = rint(random()*359);
    self.attachment.frame = dbframe;
    self.attachment.lip = 0.3 + ((7-dbframe) * 0.1);
    self.attachment.alpha = self.attachment.lip;
    particle_explode(self.pos2, 5+random()*10, 3, self.part_style, PARTICLE_BURST_MINOTAUR);
};

//----------------------------------------------------------------------
// Raise up head and howl at the sky and spawn green particles from body
// Always check if there is space to spawn a gargoyle first
// Block monster pain function so that the summons is completed
//----------------------------------------------------------------------
void() mino_summon1 =[ $howl1, mino_summon2 ] {ai_face();self.pain_finished = time + 2;};
void() mino_summon2 =[ $howl3, mino_summon3 ] {ai_face();};
void() mino_summon3 =[ $howl5, mino_summon4 ] {ai_face();mino_sumeffect();
    sound (self, CHAN_BODY, "minotaur/attack1long.wav", 1, ATTN_NORM);};
void() mino_summon4 =[ $howl7, mino_summon5 ] {mino_sumeffect();};
void() mino_summon5 =[ $howl9, mino_summon6 ] {mino_sumeffect();};
void() mino_summon6 =[ $howl11, mino_summon7 ] {mino_sumeffect();};
void() mino_summon7 =[ $howl13, mino_summon8 ] {mino_sumeffect();
    // Find out if there is enough space to spawn a gargoyle
    // Findradius version
    makevectors(self.angles);
    self.pos1 = self.origin + self.view_ofs + v_forward*64 + v_up*32;
    self.aflag = find_minionspace(self.pos1);
    
    // If the spawn locaiton all clear, spawn something!
    if (self.aflag == TRUE) {
        minion_gargoyle(self.pos1, self.enemy);
        self.pos2 = self.pos1 - '0 0 16';    // Origin of gargoyle
        mino_sumball(0);
    }
};
void() mino_summon8 =[ $howl15, mino_summon9 ] {mino_sumeffect();mino_sumball(1);};
void() mino_summon9 =[ $howl17, mino_summon10 ] {mino_sumeffect();mino_sumball(2);};
void() mino_summon10 =[ $howl19, mino_summon11 ] {mino_sumeffect();mino_sumball(3);};
void() mino_summon11 =[ $howl21, mino_summon12 ] {mino_sumeffect();mino_sumball(4);};
void() mino_summon12 =[ $howl23, mino_summon13 ] {mino_sumeffect();mino_sumball(5);};
void() mino_summon13 =[ $howl25, mino_summon14 ] {mino_sumeffect();mino_sumball(6);};
void() mino_summon14 =[ $howl27, mino_summon15 ] {mino_sumeffect();mino_sumball(7);};
void() mino_summon15 =[ $howl29, mino_summon16 ] {minotaur_finish_attachment();
    if (self.aflag == TRUE) particle_explode(self.pos1, 50, 3, self.part_style, PARTICLE_BURST_MINOTAUR);};
void() mino_summon16 =[ $howl31, mino_summon17 ] {};
void() mino_summon17 =[ $howl33, mino_summon18 ] {ai_face();};
void() mino_summon18 =[ $howl35, mino_run ] {ai_face();SUB_AttackFinished (1 + 2*random());};

//----------------------------------------------------------------------
// Both type of minotaurs go through the same magic function
//----------------------------------------------------------------------
void() mino_magic =
{
    local entity miniondef;
    
    // Make sure the attachments are setup ready
    minotaur_create_attachment();

    if (self.spawnflags & MON_MINOTAUR_MINIONS && !self.minion_active) {
        // Check for minion template first
        miniondef = find(world,classname,"monster_minotaurminion");
        if (miniondef.classtype == CT_CACHEGARGOYLE) setup_minionsupport();
        else {
            // If template no available, warn and remove feature
            dprint("\b[MINOTAUR]\b Cannot find minion template!\n");
            self.spawnflags = self.spawnflags - MON_MINOTAUR_MINIONS;
        }
    }
    
    // Is the target the player or monster?
    if (self.enemy.flags & FL_CLIENT) {
        if (self.spawnflags & MON_MINOTAUR_MINIONS) {
            // has the gargoyle limit been reached?
            if (query_minionactive(self) == TRUE) mino_summon1();
            else {
                if (self.minion_baseattack > 0 ) mino_magic1();
                else mino_run();
            }
        }
        // White Minotaur attacks with plasma
        else mino_magic1();
        
    }
    // Always attack monsters with plasma/poison projectiles
    else mino_magic1();
};

//============================================================================
// CHARGE ATTACK - just been hit with a long pain animation (pre-rage only)
//============================================================================
void(float dist) mino_chargecheck =
{
    // turn, face and pursue enemy
    ai_charge(dist);
    // Close enough for melee combat?
    if (ai_checkmelee(MONAI_MELEEMINOTAUR)) self.th_melee();
    // Enemy dead? run out of stamina or enemy not infront?
    if (self.enemy.health < 1) self.think = mino_run;
    else if (self.attack_finished < time) self.think = mino_run;
    else if (!infront(self.enemy)) self.think = mino_run;
};

//----------------------------------------------------------------------
void() mino_charge1 =[ $charge1, mino_charge2 ] {mino_chargecheck(34);};
void() mino_charge2 =[ $charge3, mino_charge3 ] {mino_chargecheck(28);monster_footstep(FALSE);};
void() mino_charge3 =[ $charge5, mino_charge4 ] {mino_chargecheck(26);};
void() mino_charge4 =[ $charge7, mino_charge5 ] {mino_chargecheck(34);};
void() mino_charge5 =[ $charge9, mino_charge6 ] {mino_chargecheck(28);monster_footstep(FALSE);};
void() mino_charge6 =[ $charge11, mino_charge1 ] {mino_chargecheck(26);};

//----------------------------------------------------------------------
void() mino_charge =
{
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    self.attack_finished = time + 2 + random();
    mino_charge1();
};

//============================================================================
// (133) Quick pain flinch backward
void() mino_pain1 = [ $pain1, mino_pain2 ] {};
void() mino_pain2 = [ $pain2, mino_pain3 ] {};
void() mino_pain3 = [ $pain3, mino_pain4 ] {};
void() mino_pain4 = [ $pain4, mino_pain5 ] {};
void() mino_pain5 = [ $pain5, mino_pain6 ] {};
void() mino_pain6 = [ $pain6, mino_pain7 ] {};
void() mino_pain7 = [ $pain7, mino_pain8 ] {};
void() mino_pain8 = [ $pain8, mino_run ] {};

//----------------------------------------------------------------------
// (141) Stationary - shake head/arms
// Block the pain and then start charging like crazy
void() mino_shake1 = [ $shake1, mino_shake2 ] {self.pain_finished = time + 2;};
void() mino_shake2 = [ $shake3, mino_shake3 ] {};
void() mino_shake3 = [ $shake5, mino_shake4 ] {};
void() mino_shake4 = [ $shake7, mino_shake5 ] {};
void() mino_shake5 = [ $shake9, mino_shake6 ] {};
void() mino_shake6 = [ $shake11, mino_shake7 ] {};
void() mino_shake7 = [ $shake13, mino_shake8 ] {};
void() mino_shake8 = [ $shake15, mino_shake9 ] {};
void() mino_shake9 = [ $shake17, mino_shake10 ] {};
void() mino_shake10 = [ $shake19, mino_run ] {
    // If pre-rage and can see enemy, start chasing
    if (self.attack_rage == FALSE && infront(self.enemy)) self.think = mino_charge;
};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) mino_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);
    minotaur_finish_attachment();

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        self.lip = random();
        if (self.lip < 0.5) sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);

        if (self.pain_check == 1) {
            // Minion spawning minotaur always goes short pain cycle
            if (self.spawnflags & MON_MINOTAUR_MINIONS) mino_pain1();
            else {
                if (self.lip < 0.4) mino_pain1();
                else mino_shake1();
            }
        }
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.8;
            self.axhitme = 0;
            mino_pain1();
        }
    }
};

//============================================================================
void() mino_death1 = [ $death1, mino_death2 ] {};
void() mino_death2 = [ $death3, mino_death3 ] {ai_forward(2);monster_check_gib();};
void() mino_death3 = [ $death5, mino_death4 ] {ai_forward(4);monster_check_gib();
self.solid = SOLID_NOT;};
void() mino_death4 = [ $death7, mino_death5 ] {ai_forward(8);};
void() mino_death5 = [ $death9, mino_death6 ] {ai_forward(10);};
void() mino_death6 = [ $death11, mino_death7 ] {ai_forward(11);};
void() mino_death7 = [ $death13, mino_death8 ] {ai_forward(11);};
void() mino_death8 = [ $death15, mino_death9 ] {ai_forward(10);};
void() mino_death9 = [ $death17, mino_death10 ] {ai_forward(8);};
void() mino_death10 = [ $death19, mino_death11 ] {ai_forward(4);};
void() mino_death11 = [ $death21, mino_death12 ] {ai_forward(7);};
void() mino_death12 = [ $death23, mino_death13 ] {ai_forward(4);};
void() mino_death13 = [ $death25, mino_death14 ] {monster_death_postcheck();};
void() mino_death14 = [ $death25, mino_death14 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() mino_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    minotaur_remove_attachment();
    //delete_minionspace();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "minotaur/death.wav", 1, ATTN_NORM);
        mino_death1 ();
    }
};

/*======================================================================
QUAKED monster_minotaur (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_minotaur =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_minotaur.mdl";
    self.headmdl = "progs/ad171/h_minotaur.mdl";        // Large head
    self.gib1mdl = "progs/ad171/gib_minoclaw1.mdl";    // Left claw
    self.gib2mdl = "progs/ad171/gib_minoclaw2.mdl";    // Right claw
    self.gib3mdl = "progs/ad171/gib_minoleg1.mdl";    // foot
    self.gib4mdl = "progs/ad171/gib_minoleg2.mdl";    // foot

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_MBALL);        // Creating attachment
    precache_model (MODEL_PROJ_MPLASMA);    // Plasma bolt
    precache_model (MODEL_PROJ_MPOISON);    // Poison bolt
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);

    // Randomly swap claws and legs around
    self.lip = random();
    if (self.lip < 0.25) self.gib1mdl = self.gib4mdl;
    else if (self.lip < 0.5) self.gib2mdl = self.gib4mdl;
    else if (self.lip < 0.75) self.gib3mdl = self.gib4mdl;

    self.idle_sound = "minotaur/idle1.wav";
    self.idle_sound2 = "minotaur/idle2.wav";
    self.idle_soundcom = "minotaur/idle1com.wav";
    self.idle_soundcom2 = "minotaur/idle2com.wav";
    precache_sound (self.idle_sound);
    precache_sound (self.idle_sound2);
    precache_sound (self.idle_soundcom);
    precache_sound (self.idle_soundcom2);

    precache_sound ("minotaur/swipe.wav");        // claw misses
    precache_sound ("minotaur/strike1.wav");    // claw hit1
    precache_sound ("minotaur/strike2.wav");    // claw hit2

    precache_sound ("minotaur/attack1start.wav");    // Short roar
    precache_sound ("minotaur/attack1end.wav");        // Short roar
    precache_sound ("minotaur/attack1long.wav");    // Enter rage mode
    precache_sound ("minotaur/bolt_fire.wav");        // fire projectile
    precache_sound (SOUND_PLASMA_HIT);                // impact sounds

    precache_sound ("minotaur/death.wav");
    self.pain_sound = "minotaur/pain1.wav";
    self.pain_sound2 = "minotaur/pain2.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);

    self.sight_sound = "minotaur/sight1.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GIANT;
    if (self.health < 1) self.health = 500;
    self.gibhealth = -60;            // Same as Shambler
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 200;            // Demon level
    self.pain_longanim = TRUE;        // can be chopped with shadow axe
    self.pain_timeout = 2;            // Does not repeat pain
    // Double damage to other monsters
    if (!self.infightextra) self.infightextra = 2;
    self.steptype = FS_TYPELARGE;    // Giant sound
    self.blockudeath = TRUE;        // No humanoid death
    self.part_style = PARTICLE_BURST_BLUE;
    if (self.deathstring == "")  self.deathstring = " was gored by a Minotaur\n";

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = 0; self.resist_cells = 0.5;

    self.th_checkattack = MinotaurCheckAttack;
    self.th_stand = mino_stand1;
    self.th_walk = mino_walk1;
    self.th_run = mino_run;
    self.th_melee = mino_melee;
    self.th_missile = mino_magic;
    self.th_pain = mino_pain;
    self.th_die = mino_die;
    self.th_jump = mino_jump1;
    self.th_jumpexit = mino_jump10;
    self.th_charge = mino_howl1;
    
    self.classtype = CT_MONMINOTAUR;
    self.classgroup = CG_DEMON;
    self.classmove = MON_MOVEWALK;
    
    // Special darker version for gargoyle spawner
    // Update all gib models and check/set poison debuff
    if (self.spawnflags & MON_MINOTAUR_MINIONS) {
        self.exactskin = 1;        // Black skin + yellow eyes
        if (self.poisonous < 0) self.poisonous = FALSE;    // Poison debuff
        else self.poisonous = TRUE;
        self.gib1skin = self.gib2skin = self.gib3skin = 1;
        self.part_style = PARTICLE_BURST_GREEN;
        // Reduce minions down to 3, gargoyles are tougher to kill
        if (!self.minion_maxcount) self.minion_maxcount = 3;
    }
            
    monster_start();
};

void() monster_shuffler = {
    if (self.deathstring == "")  self.deathstring = " was gored by a Gauroch\n"; // from Kinn
    monster_minotaur();
};