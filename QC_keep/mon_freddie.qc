/*==============================================================================
FREDDIE (Edie from Quoth - Kell/Necros/Preach)
==============================================================================*/
// Run/Walk same animation
$frame run1 run2 run3 run4 run5 run6 run7 run8
$frame run9 run10 run11 run12 run13 run14 run15 run16

// Run and fire nail/laser gun at same time
$frame rfire1 rfire2 rfire3 rfire4 rfire5 rfire6 rfire7 rfire8
$frame rfire9 rfire10 rfire11 rfire12 rfire13 rfire14 rfire15 rfire16

// Stand and fire nail/laser gun
$frame sfire1 sfire2 sfire3 sfire4 sfire5 sfire6 sfire7 sfire8
$frame sfire9 sfire10 sfire11 sfire12 sfire13 sfire14 sfire15 sfire16
$frame sfire17 sfire18 sfire19 sfire20 sfire21 sfire22 sfire23 sfire24
$frame sfire25 sfire26 sfire27 sfire28

// Pain forward
$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8
$frame pain9 pain10 pain11 pain12

// Pain Backward (short)
$frame painb1 painb2 painb3 painb4 painb5

// Quick forward swing of chainsaw arm
$frame fswing1 fswing2 fswing3 fswing4 fswing5 fswing6 fswing7 fswing8
$frame fswing9 fswing10 fswing11

// Quick smack with elbow of chainsaw arm
$frame fswinge1 fswinge2 fswinge3 fswinge4 fswinge5 fswinge6 fswinge7
$frame fswinge8 fswinge9 fswinge10

// Slow back swing of chainsaw arm (left to right)
$frame bswing1 bswing2 bswing3 bswing4 bswing5 bswing6 bswing7 bswing8
$frame bswing9 bswing10 bswing11 bswing12 bswing13 bswing14

// Extremely long death animation!
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14 death15 death16
$frame death17 death18 death19 death20 death21 death22 death23 death24
$frame death25 death26 death27 death28 death29 death30 death31 death32
$frame death33 death34 death35

// Export frames (ignored)
$frame base1 base2

// Standing / idle animation
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8

//============================================================================
void() fred_stand1 =[ $stand1, fred_stand2 ] {monster_idle_sound(); ai_stand();};
void() fred_stand2 =[ $stand2, fred_stand3 ] {ai_stand();};
void() fred_stand3 =[ $stand3, fred_stand4 ] {ai_stand();};
void() fred_stand4 =[ $stand4, fred_stand5 ] {ai_stand();};
void() fred_stand5 =[ $stand5, fred_stand6 ] {ai_stand();};
void() fred_stand6 =[ $stand6, fred_stand7 ] {ai_stand();};
void() fred_stand7 =[ $stand7, fred_stand8 ] {ai_stand();};
void() fred_stand8 =[ $stand8, fred_stand1 ] {ai_stand();};

//============================================================================
void() fred_walk1 = [ $run1, fred_walk2 ] {monster_idle_sound(); ai_walk(20);};
void() fred_walk2 = [ $run2, fred_walk3 ] {monster_footstep(FALSE);ai_walk(16);};
void() fred_walk3 = [ $run3, fred_walk4 ] {ai_walk(1);};
void() fred_walk4 = [ $run4, fred_walk5 ] {ai_walk(2);};
void() fred_walk5 = [ $run5, fred_walk6 ] {ai_walk(4);};
void() fred_walk6 = [ $run6, fred_walk7 ] {ai_walk(8);};
void() fred_walk7 = [ $run7, fred_walk8 ] {ai_walk(4);};
void() fred_walk8 = [ $run8, fred_walk9 ] {ai_walk(16);};
void() fred_walk9 = [ $run9, fred_walk10 ] {ai_walk(20);};
void() fred_walk10 = [ $run10, fred_walk11 ] {monster_footstep(FALSE);ai_walk(16);};
void() fred_walk11 = [ $run11, fred_walk12 ] {ai_walk(1);};
void() fred_walk12 = [ $run12, fred_walk13 ] {ai_walk(2);};
void() fred_walk13 = [ $run13, fred_walk14 ] {ai_walk(4);};
void() fred_walk14 = [ $run14, fred_walk15 ] {ai_walk(8);};
void() fred_walk15 = [ $run15, fred_walk16 ] {ai_walk(4);};
void() fred_walk16 = [ $run16, fred_walk1 ] {ai_walk(16);};

//===========================================================================
// RANGE ATTACK - Fires spikes or laser bolts
//===========================================================================
void(vector weapofs) fred_fireweapon =
{
    local vector org, dir, vec;
    if (self.health < 1) return;

    // Lit up that barrel
    self.effects = self.effects | EF_MUZZLEFLASH;
    // Nail and laser use the same firing speed
    self.attack_speed = SPEED_FREDDIEPROJ + (skill * SPEED_FREDDIESKILL);
    // turn and face your enemy!
    makevectors (self.angles);
    org = self.origin + attack_vector(weapofs);
    
    // The mini burst mode (run+fire) can do tracking
    if (self.attack_timer && skill > SKILL_NORMAL) {
        // More precise fire, track player velocity
        // Reset the tracking on first shot, get better over time
        if (self.cnt == 1) 
            self.attack_track = SUB_Tracking(self.enemy.origin, weapofs, self.enemy, self.attack_speed);
        else self.attack_track = SUB_Tracking(self.attack_track, weapofs, self.enemy, self.attack_speed);
        dir = normalize(self.attack_track - org);
    }
    else {
        // Standard mode spray bullets at player
        vec = v_right*(crandom()*(20+random()*50));
        dir = normalize((self.enemy.origin + vec) - org);
    }

    // Switch projectile type based on spawnflag
    if (self.spawnflags & MON_FREDDIE_LASER) {
        sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);    
        launch_projectile(org, dir, CT_PROJ_LASER, self.attack_speed);
    }
    else {
        sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
        launch_projectile(org, dir, CT_PROJ_MONNG, self.attack_speed);
    }
};

// Start the initial wind up of the weapon first
//----------------------------------------------------------------------
void() fred_sfire1 = [ $sfire1, fred_sfire2 ] {ai_face();
    self.pain_finished = time + 0.5 + (skill*0.5);
    sound (self, CHAN_WEAPON, "freddie/stfire.wav", 1, ATTN_NORM);};
void() fred_sfire2 = [ $sfire2, fred_sfire3 ] {ai_face();};
void() fred_sfire3 = [ $sfire3, fred_sfire4 ] {ai_face();};
void() fred_sfire4 = [ $sfire4, fred_sfire5 ] {ai_face();};
void() fred_sfire5 = [ $sfire5, fred_sfire6 ] {ai_face();monster_footstep(FALSE);};
void() fred_sfire6 = [ $sfire6, fred_sfire7 ] {ai_face();};
void() fred_sfire7 = [ $sfire7, fred_sfire8 ] {ai_face();};
void() fred_sfire8 = [ $sfire8, fred_sfire9 ] {ai_face();
    self.cnt = 0;
    // Vary bullet burst based on skill
    self.attack_chance = 4 + skill*2;
};

// Fire the weapon in bursts
//----------------------------------------------------------------------
void(vector weapofs) fred_standfire =
{
    // Always check AI health before locking in function
    if (self.health < 1) return;
    
    // Turn before offset check
    ai_face();

    // If no enemy or enemy dead then stop attack
    if (!self.enemy || self.enemy.health < 1) self.cnt = 10;
    // Check if can see enemy? (use weapon offset)
    else if (visxray(self.enemy, weapofs, '0 0 0', FALSE))
        self.cnt = self.cnt + 1;
    // Reset counter and keep firing
    else self.cnt = 0;
    
    // If fired a burst of bullets at enemy, stop loop
    if (self.cnt > self.attack_chance) self.think = self.th_charge;
    else fred_fireweapon(weapofs);
};

// Cycle through barrel firing
// Easy = 7 shots, Normal = 11 shots, Hard/NM = 15 shots
//----------------------------------------------------------------------
void() fred_sfire9 = [ $sfire9, fred_sfire10 ] {fred_standfire('45 -10 20');};
void() fred_sfire10 = [ $sfire10, fred_sfire11 ] {fred_standfire('45 -10 20');};
void() fred_sfire11 = [ $sfire11, fred_sfire12 ] {fred_standfire('45 -10 22');};
void() fred_sfire12 = [ $sfire12, fred_sfire13 ] {fred_standfire('45 -10 22');};
void() fred_sfire13 = [ $sfire13, fred_sfire14 ] {fred_standfire('45 -12 23');};
void() fred_sfire14 = [ $sfire14, fred_sfire15 ] {fred_standfire('42 -12 25');};
void() fred_sfire15 = [ $sfire15, fred_sfire16 ] {fred_standfire('42 -14 23');
    // Always double check health before changing think
    if (self.health > 0 && skill == SKILL_EASY) self.think = self.th_charge;};
void() fred_sfire16 = [ $sfire16, fred_sfire17 ] {fred_standfire('38 -14 27');};
void() fred_sfire17 = [ $sfire17, fred_sfire18 ] {fred_standfire('38 -17 26');};
void() fred_sfire18 = [ $sfire18, fred_sfire19 ] {fred_standfire('36 -17 28');};
void() fred_sfire19 = [ $sfire19, fred_sfire20 ] {fred_standfire('36 -17 26');
    // Always double check health before changing think
    if (self.health > 0 && skill == SKILL_NORMAL) self.think = self.th_charge;};
void() fred_sfire20 = [ $sfire20, fred_sfire21 ] {fred_standfire('30 -17 28');};
void() fred_sfire21 = [ $sfire21, fred_sfire22 ] {fred_standfire('30 -18 25');};
void() fred_sfire22 = [ $sfire22, fred_sfire23 ] {fred_standfire('25 -18 27');};
void() fred_sfire23 = [ $sfire23, fred_sfire24 ] {fred_standfire('25 -18 27');};

// Finish the wind down of weapon
void() fred_sfire24 = [ $sfire24, fred_sfire25 ] {};
void() fred_sfire25 = [ $sfire25, fred_sfire26 ] {};
void() fred_sfire26 = [ $sfire26, fred_sfire27 ] {};
void() fred_sfire27 = [ $sfire27, fred_run ] {};

//============================================================================
// freddie has a run while firing and can switch seamless
// between the two if the player is within sight
//============================================================================
void() fred_runframe =
{
    // If dead, no more updates
    if (self.health < 1) return;
    // If enemy is dead or missing no firing
    if (!self.enemy || self.enemy.health < 1) self.attack_timer = FALSE;

    if (self.attack_timer) self.frame = $rfire1 + self.walkframe;
    else self.frame = $run1 + self.walkframe;

    // Beginning of animation block
    if (self.walkframe == 0) monster_idle_sound();
    else if (self.walkframe == 1) monster_footstep(FALSE);
    else if (self.walkframe == 10) monster_footstep(FALSE);

    if (self.walkframe == 0) {self.distance = 24;self.attack_offset = '50 -20 32';}
    else if (self.walkframe == 1) {self.distance = 19;self.attack_offset = '43 -18 33';}
    else if (self.walkframe == 2) {self.distance = 1;self.attack_offset = '53 -20 27';}
    else if (self.walkframe == 3) {self.distance = 2;self.attack_offset = '47 -19 31';}
    else if (self.walkframe == 4) {self.distance = 5;self.attack_offset = '54 -17 29';}
    else if (self.walkframe == 5) {self.distance = 10;self.attack_offset = '47 -17 35';}
    else if (self.walkframe == 6) {self.distance = 5;self.attack_offset = '53 -15 32';}
    else if (self.walkframe == 7) {self.distance = 5;self.attack_offset = '47 -15 36';}
    else if (self.walkframe == 8) {self.distance = 24;self.attack_offset = '50 -13 32';}
    else if (self.walkframe == 9) {self.distance = 19;self.attack_offset = '46 -15 33';}
    else if (self.walkframe == 10) {self.distance = 1;self.attack_offset = '51 -14 33';}
    else if (self.walkframe == 11) {self.distance = 2;self.attack_offset = '47 -14 32';}
    else if (self.walkframe == 12) {self.distance = 5;self.attack_offset = '52 -14 33';}
    else if (self.walkframe == 13) {self.distance = 10;self.attack_offset = '47 -14 36';}
    else if (self.walkframe == 14) {self.distance = 5;self.attack_offset = '49 -14 35';}
    else if (self.walkframe == 15) {self.distance = 5;self.attack_offset = '45 -15 34';}

    // Run + Fire time?
    if (self.attack_timer) {
        // Keep turning towards the enemy
        ai_face();
        // Fire a small burst at the enemy
        self.cnt = self.cnt + 1;
        if (self.cnt > 2) self.attack_timer = FALSE;
        else fred_fireweapon(self.attack_offset);
    }
    
    // Move frame forward, check for conditions
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 15) self.walkframe = 0;
    self.nextthink = time + 0.1;
    self.think = fred_runframe;

    ai_run(self.distance);
};

//----------------------------------------------------------------------
void() fred_run = { self.walkframe = 0; fred_runframe(); };

//===========================================================================
// MELEE ATTACK - Various back and forth blade arm attacks
//===========================================================================
void(float side) fred_blade =
{
    local float ldmg;
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    ai_damagebreakable(20);                            // Damage any breakables
    if (!ai_checkmelee(self.meleerange)) return;    // Too far away

    // Can the target bleed? - generate blood
    if (!CanDamage (self.enemy, self)) return;
    
    sound (self, CHAN_WEAPON, "freddie/mangle.wav", 1, ATTN_NORM);
    ldmg = (random() + random() + random()) * 20;
    T_Damage (self.enemy, self, self, ldmg, DAMARMOR);
    SpawnMeatSpray (self, self.enemy, side);
    SpawnMeatSpray (self, self.enemy, side);
};

// Alternative ending of melee attack (chance of extra damage)
//----------------------------------------------------------------------
void() fred_bswing1 = [ $bswing1, fred_bswing2 ] {ai_faceforward(20);
    sound (self, CHAN_WEAPON, "freddie/sawstart.wav", 1, ATTN_NORM);};
void() fred_bswing2 = [ $bswing2, fred_bswing3 ] {ai_faceforward(12);};
void() fred_bswing3 = [ $bswing3, fred_bswing4 ] {ai_faceforward(4);};
void() fred_bswing4 = [ $bswing4, fred_bswing5 ] {ai_faceforward(2);monster_footstep(FALSE);};
void() fred_bswing5 = [ $bswing5, fred_bswing6 ] {ai_faceforward(6);fred_blade(200);};
void() fred_bswing6 = [ $bswing6, fred_bswing7 ] {ai_faceforward(4);};
void() fred_bswing7 = [ $bswing7, fred_bswing8 ] {ai_faceforward(4);};
void() fred_bswing8 = [ $bswing8, fred_bswing9 ] {ai_faceforward(8);};
void() fred_bswing9 = [ $bswing9, fred_bswing10 ] {ai_faceforward(8);monster_footstep(FALSE);};
void() fred_bswing10 = [ $bswing10, fred_bswing11 ] {ai_faceforward(8);};
void() fred_bswing11 = [ $bswing11, fred_bswing12 ] {ai_faceforward(8);};
void() fred_bswing12 = [ $bswing12, fred_bswing13 ] {ai_faceforward(12);};
void() fred_bswing13 = [ $bswing13, fred_bswing14 ] {ai_faceforward(10);};
void() fred_bswing14 = [ $bswing14, fred_run ] {ai_faceforward(10);};

// Start of melee attack
//----------------------------------------------------------------------
void() fred_fswing1 = [ $fswing1, fred_fswing2 ] {ai_faceforward(8);
    sound (self, CHAN_WEAPON, "freddie/sawstart.wav", 1, ATTN_NORM);};
void() fred_fswing2 = [ $fswing2, fred_fswing3 ] {ai_faceforward(16); monster_footstep(FALSE);};
void() fred_fswing3 = [ $fswing3, fred_fswing4 ] {ai_faceforward(10);
        self.nextthink = time + 0.09;};
void() fred_fswing4 = [ $fswing4, fred_fswing5 ] {ai_faceforward(4);
        self.nextthink = time + 0.08; monster_footstep(FALSE);};
void() fred_fswing5 = [ $fswing5, fred_fswing6 ] {ai_faceforward(2);
        self.nextthink = time + 0.07;};
void() fred_fswing6 = [ $fswing6, fred_fswing7 ] {ai_faceforward(1);
        self.nextthink = time + 0.07;};
void() fred_fswing7 = [ $fswing7, fred_fswing8 ] {ai_faceforward(1);
        self.nextthink = time + 0.06;};
void() fred_fswing8 = [ $fswing8, fred_fswing9 ] {ai_faceforward(2);fred_blade(-200);
        self.nextthink = time + 0.05;};
void() fred_fswing9 = [ $fswing9, fred_fswing10 ] {ai_faceforward(2);};
void() fred_fswing10 = [ $fswing10, fred_fswing11 ] {ai_faceforward(2);};
void() fred_fswing11 = [ $fswing11, fred_fswinge1 ] {
    // Always check for enemy and health before changing think
    if (self.enemy && self.enemy.health > 1 && self.health > 0) {
        // Check if within range to do a second (back) swing attack
        if (ai_checkmelee(self.meleerange)) self.think = fred_bswing1;
    }
};

// End of melee attack
//----------------------------------------------------------------------
void() fred_fswinge1 = [ $fswinge1, fred_fswinge2 ] {ai_faceforward(2);};
void() fred_fswinge2 = [ $fswinge2, fred_fswinge3 ] {ai_faceforward(2);};
void() fred_fswinge3 = [ $fswinge3, fred_fswinge4 ] {ai_faceforward(3);};
void() fred_fswinge4 = [ $fswinge4, fred_fswinge5 ] {ai_faceforward(4);};
void() fred_fswinge5 = [ $fswinge5, fred_fswinge6 ] {ai_faceforward(8);};
void() fred_fswinge6 = [ $fswinge6, fred_fswinge7 ] {ai_faceforward(12);};
void() fred_fswinge7 = [ $fswinge7, fred_fswinge8 ] {ai_faceforward(16);};
void() fred_fswinge8 = [ $fswinge8, fred_fswinge9 ] {ai_faceforward(12);};
void() fred_fswinge9 = [ $fswinge9, fred_fswinge10 ] {ai_faceforward(16);};
void() fred_fswinge10 = [ $fswinge10, fred_run ] {
    ai_faceforward(16);monster_footstep(FALSE);};

//============================================================================
void() fred_pain1 = [ $pain1, fred_pain2 ] {};
void() fred_pain2 = [ $pain2, fred_pain3 ] {ai_pain(10);};
void() fred_pain3 = [ $pain3, fred_pain4 ] {ai_pain(9);};
void() fred_pain4 = [ $pain4, fred_pain5 ] {ai_pain(4);};
void() fred_pain5 = [ $pain5, fred_pain6 ] {};
void() fred_pain6 = [ $pain6, fred_pain7 ] {};
void() fred_pain7 = [ $pain7, fred_pain8 ] {};
void() fred_pain8 = [ $pain8, fred_pain9 ] {};
void() fred_pain9 = [ $pain9, fred_pain10 ] {};
void() fred_pain10 = [ $pain10, fred_pain11 ] {};
void() fred_pain11 = [ $pain11, fred_pain12 ] {};
void() fred_pain12 = [ $pain12, fred_run ] {};

//----------------------------------------------------------------------
void() fred_painb1 = [ $painb1, fred_painb2 ] {};
void() fred_painb2 = [ $painb2, fred_painb3 ] {};
void() fred_painb3 = [ $painb3, fred_painb4 ] {};
void() fred_painb4 = [ $painb4, fred_painb5 ] {};
void() fred_painb5 = [ $painb5, fred_run ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) fred_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        if (random() < 0.2) {
            sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
            self.pain_finished = time + 3 + random()*2;
            fred_pain1();
        }
        else {
            sound (self, CHAN_VOICE, self.pain_sound2, 1, ATTN_NORM);
            self.pain_finished = time + 2 + random()*2;
            fred_painb1();
        }
    }
};

//============================================================================
void() fred_death1 = [ $death1, fred_death2 ] {};
void() fred_death2 = [ $death2, fred_death3 ] {monster_check_gib();};
void() fred_death3 = [ $death3, fred_death4 ] {monster_check_gib();
self.solid = SOLID_NOT;};
void() fred_death4 = [ $death4, fred_death5 ] {};
void() fred_death5 = [ $death5, fred_death6 ] {};
void() fred_death6 = [ $death6, fred_death7 ] {};
void() fred_death7 = [ $death7, fred_death8 ] {};
void() fred_death8 = [ $death8, fred_death9 ] {};
void() fred_death9 = [ $death9, fred_death10 ] {};
void() fred_death10 = [ $death10, fred_death11 ] {};
void() fred_death11 = [ $death11, fred_death12 ] {};
void() fred_death12 = [ $death12, fred_death13 ] {};
void() fred_death13 = [ $death13, fred_death14 ] {};
void() fred_death14 = [ $death14, fred_death15 ] {};
void() fred_death15 = [ $death15, fred_death16 ] {};
void() fred_death16 = [ $death16, fred_death17 ] {};
void() fred_death17 = [ $death17, fred_death18 ] {};
void() fred_death18 = [ $death18, fred_death19 ] {};
void() fred_death19 = [ $death19, fred_death20 ] {};
void() fred_death20 = [ $death20, fred_death21 ] {};
void() fred_death21 = [ $death21, fred_death22 ] {};
void() fred_death22 = [ $death22, fred_death23 ] {};
void() fred_death23 = [ $death23, fred_death24 ] {};
void() fred_death24 = [ $death24, fred_death25 ] {};
void() fred_death25 = [ $death25, fred_death26 ] {};
void() fred_death26 = [ $death26, fred_death27 ] {};
void() fred_death27 = [ $death27, fred_death28 ] {};
void() fred_death28 = [ $death28, fred_death29 ] {};
void() fred_death29 = [ $death29, fred_death30 ] {};
void() fred_death30 = [ $death30, fred_death31 ] {};
void() fred_death31 = [ $death31, fred_death32 ] {};
void() fred_death32 = [ $death32, fred_death33 ] {};
void() fred_death33 = [ $death33, fred_death34 ] {};
void() fred_death34 = [ $death34, fred_death35 ] {monster_death_postcheck();};
void() fred_death35 = [ $death35, fred_death35 ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() fred_explode =
{
    // no more FREDDIE!
    entity_hide (self);
    // Make sure gibs go flying up
    self.max_health = MON_GIBFOUNTAIN;
    self.health = -100;

    // ID style explosion
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    T_RadiusDamage (self, self, self.death_dmg, world, DAMAGEALL);
    
    // Sprite + Dust
    SpawnExplosion(EXPLODE_BIG, self.origin, self.death_sound);
    SpawnProjectileSmoke(self.origin, 200, 250, 100);
    SpawnProjectileSmoke(self.origin, 200, 250, 100);

    // Regular blood like gibs
    ThrowGib(4, 2 + rint(random()*4));
    ThrowGib(5, 2 + rint(random()*4));
    ThrowGib(25, 1);
    // Metal and custom body parts
    self.gibtype = GIBTYPE_METAL;
    ThrowGib(11, 1 + rint(random()*2));
    ThrowGib(12, 2 + rint(random()*2));
    ThrowGib(13, 2 + rint(random()*2));
};

//----------------------------------------------------------------------
void() fred_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    // regular death
    if (!self.gibbed) {
        sound (self, CHAN_VOICE, "freddie/death.wav", 1, ATTN_NORM);
        fred_death1 ();
    }
};

/*======================================================================
QUAKED monster_freddie (1 0 0) (-32 -32 -24) (32 32 64) Ambush
======================================================================*/
void() monster_freddie =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/ad171/mon_freddie.mdl";            // was edie.mdl
    self.headmdl = "progs/h_ogre.mdl";            // ID ogre
    self.gib1mdl = "progs/ad171/gib_freddiejunk.mdl";    // wreckage.mdl
    self.gib2mdl = "progs/ad171/gib_metal1.mdl";        // Breakable metal
    self.gib3mdl = "progs/ad171/gib_metal3.mdl";        // Breakable metal

    precache_model (self.mdl);
    precache_model (self.headmdl);        // ID Ogre head    
    // Gib Junk - 0=Axle, 1=bolt, 2=chip, 3=wing
    precache_model (self.gib1mdl);        // Multiple chunks of junk
    self.gib1frame = 3;                    // 4 frames choose from
    precache_model (self.gib2mdl);        // Generic metal1_2
    precache_model (self.gib3mdl);        // Generic metal1_2
    
    self.idle_sound = "freddie/idle1.wav";
    precache_sound (self.idle_sound);

    // Melee sounds, swing and impact
    precache_sound("freddie/sawstart.wav");
    precache_sound("freddie/mangle.wav");

    // Default attack - red glowing nails
    precache_model (MODEL_PROJ_NGRED);
    precache_sound("freddie/stfire.wav");

    // Alternative attack - lasers!?!
    if (self.spawnflags & MON_FREDDIE_LASER) {
        precache_model (MODEL_PROJ_LASER);
        precache_sound("seeker/range_laser.wav");
        precache_sound ("enforcer/enfire.wav");
        precache_sound ("enforcer/enfstop.wav");
    }

    precache_sound ("freddie/death.wav");
    self.death_sound = "seeker/explode_major.wav";
    precache_sound (self.death_sound);
    self.pain_sound = "freddie/pain.wav";
    self.pain_sound2 = "freddie/painshrt.wav";
    precache_sound (self.pain_sound);
    precache_sound (self.pain_sound2);
    
    self.sight_sound = "freddie/sight.wav";
    precache_sound (self.sight_sound);

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GIANT;
    if (self.health < 1) self.health = 500;
    self.gibhealth = -80;                // Hard to gib
    self.gibbed = FALSE;
    self.pain_flinch = 200;                // takes alot to pain
    self.pain_longanim = FALSE;            // cannot be chopped with shadow axe
    if (!self.infightextra) self.infightextra = 2;    // 2x infighting dmg
    self.meleerange = MONAI_MELEEFREDDIE;    // Blade range
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_FREDDIE;
    self.pain_timeout = 2;                // High pain threshold
    self.blockudeath = TRUE;            // no humanoid death sound
    self.attack_offset = '0 0 0';        // Run + Gun vector

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    if (self.deathstring == "") self.deathstring = " was exterminated by Freddie\n";
    
    // Custom feet sounds (specific order)
    self.stepc1 = "freddie/step.wav";
    self.stepc2 = "freddie/step2.wav";
    self.stepc3 = "freddie/step.wav";
    self.stepc4 = "freddie/step2.wav";
    self.stepc5 = "freddie/step3.wav";
    precache_sound (self.stepc1);
    precache_sound (self.stepc2);
    precache_sound (self.stepc5);
    self.steptype = FS_TYPECUSTOM;

    self.th_checkattack = FreddieCheckAttack;
    self.th_stand = fred_stand1;
    self.th_walk = fred_walk1;
    self.th_run = fred_run;
    self.th_melee = fred_fswing1;        // Blade arm
    self.th_missile = fred_sfire1;        // Spike/Laser bolts
    self.th_charge = fred_sfire24;        // End of barrel weapon
    self.th_pain = fred_pain;
    self.th_die = fred_die;
    self.th_gibdie = fred_explode;

    self.hittype = HIT_METAL;

    self.classtype = CT_MONFREDDIE;
    self.classgroup = CG_OGRE;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};

// Re-direct all Quoth Edie requests to Freddie!
void() monster_edie = { monster_freddie(); }
