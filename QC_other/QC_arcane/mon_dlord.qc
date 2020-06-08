/*==============================================================================
 DEATH LORD (Originally from Quoth - Kell/Necros/Preach)
  * QC was created for AD mappers to play with in their own projects
  * Original Quoth model/sounds not included with main MOD
 
  Interesting QC traits
  * WALKS when insight of enemy and RUNS when out of sight
  * Lightning bolts reflect off body when shield active
  * When shield is active all damage is reduced by 50%
  * Melee attack is a shambler (half damage) like lightning attack
  * Melee attack is dis-used magic animation in original HK model
  * Range attack works like the rocketeer, except constant speed
  * Range attack is far from perfect tracking to allowing strafing
  
  QUOTH assets required to get this monster working in AD
    (model's in 'progs' and wav's in 'sound' sub directories)
  * hknight.mdl -> mon_dlord.mdl (rename file, skin 1 = dlord)
  * h_dlord.mdl -> h_dlord.mdl (only has 1 skin, thrown as custom gib)
  * dlord/idle.wav, dlord/sight.wav, dlord/pain.wav, dlord/death.wav
    
==============================================================================*/
// 000-008
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

// 009-028
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

// 029-036
$frame run1 run2 run3 run4 run5 run6 run7 run8

// 037-041
$frame pain1 pain2 pain3 pain4 pain5

// 042-053
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// 054-062
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

// 063-078 (not used)
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

// 079-092 Melee attack - lightning
$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

// 093-105 Range attack - spike balls
$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

// 106-111 (not used)
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// 112-121 (not used)
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

// 122-132 (not used)
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

// 133-154 (not used)
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

// 155-165 (not used)
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11

void() dlord_sword_sound;

//===========================================================================
void() dlord_shieldon =
{
    self.effects = EF_BRIGHTFIELD;    // Glowing yellow particle effect
    self.takedamage = DAMAGE_YES;    // Grenades bounce off
    self.bouncegrenade = TRUE;        // Warning to other monsters
    self.reflectlightning = TRUE;    // Reflect lightning strikes
    self.reflectplasma = TRUE;        // Reflect plasma projectiles
    self.reflectnails = TRUE;        // Reflect nail projectiles
    // Visually show resistance and reduce damage taken
    self.resist_shells = self.resist_nails = 0.5;
    self.resist_rockets = self.resist_cells = 0.5;
};

void() dlord_shieldoff =
{
    self.effects = FALSE;            // No engine particle effects active
    self.takedamage = DAMAGE_AIM;    // Grenades make contact
    self.bouncegrenade = FALSE;        // Reset to default
    self.reflectlightning = FALSE;    // Reflect lightning strikes
    self.reflectplasma = FALSE;        // Reflect plasma projectiles
    self.reflectnails = FALSE;        // Reflect nail projectiles
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
};

//===========================================================================
void()    dlord_stand1=[ $stand1, dlord_stand2] {monster_idle_sound(); ai_stand();};
void()    dlord_stand2=[ $stand2, dlord_stand3] {ai_stand();};
void()    dlord_stand3=[ $stand3, dlord_stand4] {ai_stand();};
void()    dlord_stand4=[ $stand4, dlord_stand5] {ai_stand();};
void()    dlord_stand5=[ $stand5, dlord_stand6] {ai_stand();};
void()    dlord_stand6=[ $stand6, dlord_stand7] {ai_stand();};
void()    dlord_stand7=[ $stand7, dlord_stand8] {ai_stand();};
void()    dlord_stand8=[ $stand8, dlord_stand9] {ai_stand();};
void()    dlord_stand9=[ $stand9, dlord_stand1] {ai_stand();};

//===========================================================================
void()    dlord_walk1    =[ $walk1,    dlord_walk2    ] {monster_idle_sound(); ai_walk(2);};
void()    dlord_walk2    =[ $walk2,    dlord_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    dlord_walk3    =[ $walk3,    dlord_walk4    ] {ai_walk(5);};
void()    dlord_walk4    =[ $walk4,    dlord_walk5    ] {ai_walk(4);};
void()    dlord_walk5    =[ $walk5,    dlord_walk6    ] {ai_walk(4);};
void()    dlord_walk6    =[ $walk6,    dlord_walk7    ] {ai_walk(2);};
void()    dlord_walk7    =[ $walk7,    dlord_walk8    ] {ai_walk(2);};
void()    dlord_walk8    =[ $walk8,    dlord_walk9    ] {ai_walk(3);};
void()    dlord_walk9    =[ $walk9,    dlord_walk10] {ai_walk(3);};
void()    dlord_walk10=[ $walk10, dlord_walk11] {ai_walk(4);};
void()    dlord_walk11=[ $walk11, dlord_walk12] {monster_footstep(FALSE); ai_walk(3);};
void()    dlord_walk12=[ $walk12, dlord_walk13] {ai_walk(4);};
void()    dlord_walk13=[ $walk13, dlord_walk14] {ai_walk(6);};
void()    dlord_walk14=[ $walk14, dlord_walk15] {ai_walk(2);};
void()    dlord_walk15=[ $walk15, dlord_walk16] {ai_walk(2);};
void()    dlord_walk16=[ $walk16, dlord_walk17] {ai_walk(4);};
void()    dlord_walk17=[ $walk17, dlord_walk18] {ai_walk(3);};
void()    dlord_walk18=[ $walk18, dlord_walk19] {ai_walk(3);};
void()    dlord_walk19=[ $walk19, dlord_walk20] {ai_walk(3);};
void()    dlord_walk20=[ $walk20, dlord_walk1    ] {ai_walk(2);};

//===========================================================================
// Two speed run animation based on enemy visibility
//===========================================================================
void()    dlord_slow1;
void()    dlord_fast1;

// run state has two animation speeds (single function required)
void(float movespd) dlord_run =
{
    // Block all checks if dead
    if (self.health < 1) return;
    // idle/shield functions work for both animation sets
    if (self.walkframe == 0) monster_idle_sound();
    if (self.walkframe == 1) dlord_shieldon();
        
    // Change animation set based on enemy visibilty
    // Ignore frame 0 for checks (endless loop)
    // Make sure animations don't switch too rapid (timer)
    if (self.enemy) {
        if (self.walkframe > 0 && self.attack_timer < time) {
            // Prevent rapid switching (attack_timer)
            self.attack_timer = time + 1 + random();
            enemy_vis = visible(self.enemy);
            if (!enemy_vis) self.think = dlord_fast1;
            else self.think = dlord_slow1;
        }
    }
    
    // Check run/attack functions
    ai_run(movespd);
    // Walk through animation sets (reset on frame 0)
    self.walkframe = self.walkframe + 1;
};

// Walk animation with 150% movement speed
//----------------------------------------------------------------------
void()    dlord_slow1    =[ $walk1,    dlord_slow2    ] {self.walkframe=0;dlord_run(3);};
void()    dlord_slow2    =[ $walk2,    dlord_slow3    ] {monster_footstep(FALSE);dlord_run(7.5);};
void()    dlord_slow3    =[ $walk3,    dlord_slow4    ] {dlord_run(7.5);};
void()    dlord_slow4    =[ $walk4,    dlord_slow5    ] {dlord_run(6);};
void()    dlord_slow5    =[ $walk5,    dlord_slow6    ] {dlord_run(6);};
void()    dlord_slow6    =[ $walk6,    dlord_slow7    ] {dlord_run(3);};
void()    dlord_slow7    =[ $walk7,    dlord_slow8    ] {dlord_run(3);};
void()    dlord_slow8    =[ $walk8,    dlord_slow9    ] {dlord_run(4.6);};
void()    dlord_slow9    =[ $walk9,    dlord_slow10] {dlord_run(4.5);};
void()    dlord_slow10=[ $walk10, dlord_slow11] {dlord_run(6);};
void()    dlord_slow11=[ $walk11, dlord_slow12] {monster_footstep(FALSE); dlord_run(4.5);};
void()    dlord_slow12=[ $walk12, dlord_slow13] {dlord_run(6);};
void()    dlord_slow13=[ $walk13, dlord_slow14] {dlord_run(9);};
void()    dlord_slow14=[ $walk14, dlord_slow15] {dlord_run(3);};
void()    dlord_slow15=[ $walk15, dlord_slow16] {dlord_run(3);};
void()    dlord_slow16=[ $walk16, dlord_slow17] {dlord_run(6);};
void()    dlord_slow17=[ $walk17, dlord_slow18] {dlord_run(4.5);};
void()    dlord_slow18=[ $walk18, dlord_slow19] {dlord_run(4.5);};
void()    dlord_slow19=[ $walk19, dlord_slow20] {dlord_run(4.5);};
void()    dlord_slow20=[ $walk20, dlord_slow1    ] {dlord_run(3);};

// Typical run animation (used when enemy out of sight)
//----------------------------------------------------------------------
void()    dlord_fast1 =[ $run1, dlord_fast2 ] {self.walkframe=0;dlord_run(20);};
void()    dlord_fast2 =[ $run2, dlord_fast3 ] {monster_footstep(FALSE); dlord_run(25);};
void()    dlord_fast3 =[ $run3, dlord_fast4 ] {dlord_run(18);};
void()    dlord_fast4 =[ $run4, dlord_fast5 ] {dlord_run(16);};
void()    dlord_fast5 =[ $run5, dlord_fast6 ] {dlord_run(14);};
void()    dlord_fast6 =[ $run6, dlord_fast7 ] {monster_footstep(FALSE); dlord_run(25);};
void()    dlord_fast7 =[ $run7, dlord_fast8 ] {dlord_run(21);};
void()    dlord_fast8 =[ $run8, dlord_fast1 ] {dlord_run(13);};

//============================================================================
// Melee attack (over head attack)
//----------------------------------------------------------------------
void(float zoffset) dlord_lightning =
{
    local vector org, dir, sword_offset;

    if (self.health < 1) return;    // Alive?
    if (!self.enemy) return;        // Something to fight?
    ai_damagebreakable(30);            // Damage any breakables
    
    ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;

    makevectors(self.angles);
    sword_offset = self.attack_offset; sword_offset_z = zoffset;
    org = self.origin + attack_vector(sword_offset);
    dir = self.enemy.origin + '0 0 16' - org;
    dir = normalize (dir);
    
    // Trace direct line of lightning infront
    self.enemydist = range_distance(self.enemy, FALSE);
    traceline (org, self.origin + dir*self.enemydist, TRUE, self);

    // Display lightning model segments
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING1);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);

    // Check for any breakables first
    if (ai_foundbreakable(self, trace_ent, FALSE)) {
        T_Damage (trace_ent, self, self, 80, DAMARMOR);
    }
    // Half Shambler lightning damage
    else LightningDamage (org, trace_endpos, self, 5);
};

//----------------------------------------------------------------------
void()    dlord_magica1 =[ $magica1, dlord_magica2 ] {ai_face();dlord_shieldoff();};
void()    dlord_magica2 =[ $magica2, dlord_magica3 ] {ai_face();};
void()    dlord_magica3 =[ $magica3, dlord_magica4 ] {ai_face();};
void()    dlord_magica4 =[ $magica4, dlord_magica5 ] {ai_face();};
void()    dlord_magica5 =[ $magica5, dlord_magica6 ] {ai_face();};
void()    dlord_magica6 =[ $magica6, dlord_magica7 ] {ai_face();};
void()    dlord_magica7 =[ $magica7, dlord_magica8 ] {
    sound(self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
    dlord_lightning(40);};
void()    dlord_magica8 =[ $magica8, dlord_magica9 ] {dlord_lightning(25);};
void()    dlord_magica9 =[ $magica9, dlord_magica10 ] {dlord_lightning(0);};
void()    dlord_magica10 =[ $magica10, dlord_magica11 ] {dlord_lightning(-20);};
void()    dlord_magica11 =[ $magica11, dlord_magica12 ] {dlord_lightning(-24);};
void()    dlord_magica12 =[ $magica12, dlord_magica13 ] {if (skill == SKILL_NIGHTMARE) dlord_lightning(-14);};
void()    dlord_magica13 =[ $magica13, dlord_magica14 ] {ai_face();};
void()    dlord_magica14 =[ $magica14, dlord_slow1 ] {ai_face();};

//============================================================================
// RANGE - SPIKEBALL (Fast spike damage)
//============================================================================
void() dlord_spike_think =
{
    // Only track enemies with a pulse!
    if (self.enemy.health < 1) return;
    
    // Only do a limited amount of updates
    self.count = self.count + 1;
    if (self.count < (5+skill)) {
        // Keep ticking (slowly)
        self.nextthink = time + 0.1;

        // Steer towards enemy (not very precise)
        self.pos1 = (self.enemy.origin - (self.origin + '0 0 -12')) * 0.6;
        self.pos2 = self.pos1 + self.velocity * 0.4;
        self.pos3 = normalize(self.pos2);
        self.velocity = self.pos3 * self.attack_speed;
        
        // Perfect angle correction, very frustrating
        // self.attack_track = normalize(self.enemy.origin - self.origin);
        // self.velocity = self.attack_track * self.attack_speed;
        // self.angles = vectoangles(self.velocity);
    }
    else {
        // Remove spikeball after a long while
        self.nextthink = time + LIFE_PROJECTILE;
        self.think = SUB_Remove;
    }
};

//----------------------------------------------------------------------
void() dlord_attackb =
{
    local vector org, dir, offang;
    
    // Keep adding up projectile counter
    self.attack_chance = self.attack_chance + 1;
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);    

    // Randomize projectile offset (similar to HK)
    if (random() < 0.4) self.weaponstate = 0;
    else {
        if (random() <= 0.7) self.weaponstate = -1;
        else self.weaponstate = 1;
    }

    // Keep tracking enemy origin (similar to wizards)
    ai_trackenemy();

    // Shift the firing angle 6 degree's left/right
    offang = vectoangles(self.attack_track - self.origin);
    offang_y = offang_y + (self.weaponstate * 6);
    makevectors (offang);

    // Calculate the origin (end of sword)
    org = self.origin + attack_vector(self.attack_offset);
    // Use previous firing angle to calculate firing direction
    dir = normalize (v_forward);
    dir_z = 0 - dir_z + (random() - 0.5) * 0.2;
    
    // Add skill level to firing speed calculation
    self.attack_speed = SPEED_DLORDMISSILE + (skill * SPEED_DLORDSKILL);
    launch_projectile (org, dir, CT_PROJ_DLORD, self.attack_speed);
}

//----------------------------------------------------------------------
void() dlord_magicb1 = [ $magicb1, dlord_magicb2 ] {
    self.attack_track = self.enemy.origin;
    self.attack_chance = self.attack_timer = 0;
    dlord_shieldoff();
};
void() dlord_magicb2 = [ $magicb2, dlord_magicb3 ] {ai_trackenemy();};
void() dlord_magicb3 = [ $magicb3, dlord_magicb4 ] {ai_trackenemy();};
void() dlord_magicb4 = [ $magicb4, dlord_magicb5 ] {ai_trackenemy();};
void() dlord_magicb5 = [ $magicb5, dlord_magicb6 ] {ai_trackenemy();};
void() dlord_magicb6 = [ $magicb6, dlord_magicb7 ] {ai_trackenemy();};
void() dlord_magicb7 = [ $magicb7, dlord_magicb8 ] {ai_trackenemy();};
void() dlord_magicb8 = [ $magicb8, dlord_magicb9 ] {ai_trackenemy();};
void() dlord_magicb9 = [ $magicb9, dlord_magicb10] {
    // Is the enemy active and self alive?
    if (self.enemy && self.health > 0) {
        dlord_attackb();    // Fire projectile
        
        // Anymore projectiles to fire?
        if (self.attack_chance < MONAI_DLORDMAX) {
            // Is the player still visible?
            if ( visxray(self.enemy, self.attack_offset, '0 0 0', FALSE) ) {
                self.attack_timer = time + 0.8;
            }
            // Is the enemy in view timer still active?
            if (self.attack_timer > time) self.think = dlord_magicb8;
        }
    }
};
void() dlord_magicb10 = [ $magicb10, dlord_magicb11 ] {};
void() dlord_magicb11 = [ $magicb11, dlord_magicb12 ] {};
void() dlord_magicb12 = [ $magicb12, dlord_magicb13 ] {};
void() dlord_magicb13 = [ $magicb13, dlord_slow1 ] {ai_face();};

//============================================================================
void()    dlord_pain1    =[ $pain1,    dlord_pain2    ] {};
void()    dlord_pain2    =[ $pain2,    dlord_pain3    ] {};
void()    dlord_pain3    =[ $pain3,    dlord_pain4    ] {};
void()    dlord_pain4    =[ $pain4,    dlord_pain5    ] {};
void()    dlord_pain5    =[ $pain5,    dlord_slow1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) dlord_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) dlord_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.5;
            self.axhitme = 0;
            dlord_pain1 ();
        }
    }
};

//============================================================================
void()    dlord_die1    =[    $death1,    dlord_die2    ] {ai_forward(10);};
void()    dlord_die2    =[    $death2,    dlord_die3    ] {monster_check_gib();ai_forward(8);};
void()    dlord_die3    =[    $death3,    dlord_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    dlord_die4    =[    $death4,    dlord_die5    ] {};
void()    dlord_die5    =[    $death5,    dlord_die6    ] {};
void()    dlord_die6    =[    $death6,    dlord_die7    ] {};
void()    dlord_die7    =[    $death7,    dlord_die8    ] {};
void()    dlord_die8    =[    $death8,    dlord_die9    ] {ai_forward(10);};
void()    dlord_die9    =[    $death9,    dlord_die10    ] {ai_forward(11);};
void()    dlord_die10    =[    $death10,    dlord_die11    ] {};
void()    dlord_die11    =[    $death11,    dlord_die12    ] {monster_death_postcheck();};
void()    dlord_die12    =[    $death12,    dlord_die12    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    dlord_dieb1    =[    $deathb1,    dlord_dieb2    ] {};
void()    dlord_dieb2    =[    $deathb2,    dlord_dieb3    ] {monster_check_gib();};
void()    dlord_dieb3    =[    $deathb3,    dlord_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    dlord_dieb4    =[    $deathb4,    dlord_dieb5    ] {};
void()    dlord_dieb5    =[    $deathb5,    dlord_dieb6    ] {};
void()    dlord_dieb6    =[    $deathb6,    dlord_dieb7    ] {};
void()    dlord_dieb7    =[    $deathb7,    dlord_dieb8    ] {};
void()    dlord_dieb8    =[    $deathb8,    dlord_dieb9    ] {monster_death_postcheck();};
void()    dlord_dieb9    =[    $deathb9,    dlord_dieb9    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() dlord_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    
    // regular death
    if (!self.gibbed) {
    sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        if (random() > 0.5) dlord_die1 ();    // Forward - standard
        else dlord_dieb1 ();                // Backward - standard
    }
};

//============================================================================
void() dlord_sword_sound =
{
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

/*======================================================================
 QUAKED monster_death_lord (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_death_lord =
{
    if (deathmatch) { remove(self); return; }

    // No head model defined
    self.mdl = "progs/mon_dlord.mdl";            // hknight.mdl (quoth)
    self.gib1mdl = "progs/h_dlord.mdl";            // h_dlord.mdl (quoth)
    self.gib2mdl = "progs/w_hknightsword.mdl";    // Unique sword
    self.gib3mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib4mdl = "progs/gib_knfoot_r.mdl";    // right foot
    
    // The Quoth head model only has 1 skin which does not match the
    // dlord model which has 2, so throw the head as a custom gib instead
    precache_model (self.mdl);
    precache_model (MODEL_PROJ_DLORD1);            // Copy of proj_serg.mdl
    precache_model (self.gib1mdl);                // Head gib (skin probs)
    precache_model (self.gib2mdl);                // Old HK Sword
    precache_model (self.gib3mdl);                // HK foot
    precache_model (self.gib4mdl);
    precache_model (MODEL_PROJ_NOUR3);            // Lightning bolt

    self.gib2sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib3mdl = string_null;
    if (random() < 0.5) self.gib4mdl = string_null;

    self.idle_sound = "dlord/idle.wav";
    precache_sound (self.idle_sound);

    // Firing of magic spikes (first 0.1s must loop)
    precache_sound ("hknight/attack1.wav");
    
    // TE_KNIGHTSPIKE = Magic spike hitting the wall
    // Has got to be in this location because the 
    // filename is hardcoded (engine.exe)
    precache_sound ("hknight/hit.wav");                

    // precache sight, pain and death (changes on type)
    self.sight_sound = "dlord/sight.wav";
    precache_sound (self.sight_sound);

    // Pain and Death and Taxes!
    self.pain_sound = "dlord/pain.wav";
    precache_sound (self.pain_sound);
    self.death_sound = "dlord/death.wav";
    precache_sound (self.death_sound);

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");
        
    self.solid = SOLID_NOT;                    // No interaction with world
    self.movetype = MOVETYPE_NONE;            // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 400;
    self.gibhealth = -50;                    // Not easy to gib
    self.gibbed = FALSE;                    // Still in one piece
    self.pain_flinch = 60;                    // Medium pain threshold
    self.pain_timeout = 2;                    // High pain threshold
    self.pain_longanim = TRUE;                // can be chopped with shadow axe
    self.steptype = FS_TYPEHEAVY;            // Heavy Hell Knight
    self.attack_offset = '20 0 24';            // End of sword
    self.exactskin = 1;                        // Rotting Lord skin
    self.poisonous = FALSE;                    // No support for this
    self.deathstring = " was executed by a Death Lord\n";
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    // Using alt magic and charge attacks
    self.th_checkattack = DLordCheckAttack;
    self.th_stand = dlord_stand1;        // Idling on feet
    self.th_walk = dlord_walk1;            // Walking
    self.th_run = dlord_slow1;            // Running
    self.th_melee = dlord_magica1;        // Sword attack
    self.th_missile = dlord_magicb1;    // Spike ball attack
    self.th_updmissile = dlord_spike_think;    // Update flying spikeball
    self.th_pain = dlord_pain;            // Stumble and pain
    self.th_die = dlord_die;            // Face meets floor
    
    self.classtype = CT_MONDLORD;
    self.classgroup = CG_DCAPTAIN;        // Classic tincan!
    self.classmove = MON_MOVEWALK;
    monster_start();
};
