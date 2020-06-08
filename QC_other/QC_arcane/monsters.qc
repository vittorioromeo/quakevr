/*======================================================================
 MONSTER FUNCTIONS
======================================================================*/

float MONAI_ZOMBIEFLR = 1;        // start on floor
float MONAI_ZOMBIEUPB = 2;        // zombie painb - 28 frames (9 = on ground)
float MONAI_ZOMBIEUPD = 4;        // knight paind - 35 frames (12 = on ground)
float MONAI_ZOMBIEUPE = 8;        // knight + zombie paine - 30 frames (12 = on ground)
float MONAI_ZOMBGRDTIMER = 2;    // Zombie onground timer (between checking)
float MONAI_ZOMBGRDBLOCK = 300;    // 0.1 * 300 = 30s
float MONAI_ZOMBIELOW_DAM = 9;    // Pain threshold for animations    
float MONAI_ZOMBIEHGH_DAM = 28;    // Has to be higher than SG (7x4)

void(float frameno) zombie_onground;
void(float frameno) zombiek_onground;

//----------------------------------------------------------------------
// Pre-defined bounding boxes for monsters
// Use 'bboxtype' to specify one of these
//----------------------------------------------------------------------
float BBOX_TINY = 1;        // -16 -16 -24, 16 16 16
float BBOX_SHORT = 4;        // -16 -16 -24, 16 16 32
float BBOX_TALL = 5;        // -16 -16 -24, 16 16 40
float BBOX_WIDE = 7;        // -24 -24 -24, 24 24 40
float BBOX_GIANT = 8;        // -24 -24 -24, 24 24 64
float BBOX_MASSIVE = 10;    // -32 -32 -24, 32 32 64
float BBOX_GOLEM = 15;        // -24 -24 -24, 24 24 72
float BBOX_DOG = 20;        // -20 -20 -24, 20 20 16
float BBOX_FISH = 25;        // -16 -16 -24, 16 16 24
float BBOX_FISHS = 26;        // -12 -12 -16, 12 12 16
float BBOX_EEL = 27;        // -16 -16 -16, 16 16 16
float BBOX_HYDRA = 30;        // -20 -20 -16, 20 20 16
float BBOX_CUSTOM = 99;        // Custom size set already

/*======================================================================
 monster_use - trigger/make angry
======================================================================*/
void() monster_use =
{
    // Busy with another enemy or dead?
    if (self.enemy) return;
    if (self.health < 1) return;
    
    // Spiders/Vorelings can be setup on the ceilings
    // need to trigger/drop instead them before anything else
    if (self.classtype == CT_MONSPIDER && self.spawnflags & MON_SPIDER_CEILING ||
        self.classtype == CT_MONSWAMPLING && self.spawnflags & MON_SWAMPLING_CEILING ||
        self.classtype == CT_MONVORELING && self.spawnflags & MON_VORELING_CEILING) {
        // Start with activator as enemy
        self.enemy = activator;
        // Work through exceptions
        if (activator.classtype != CT_PLAYER) self.enemy = world;
        if (activator.items & IT_INVISIBILITY) self.enemy = world;
        if (activator.flags & FL_NOTARGET) self.enemy = world;
        // Drop from ceiling
        self.th_run();
        return;
    }
    
    // Player exceptions
    if (activator.classtype != CT_PLAYER) return;
    if (activator.items & IT_INVISIBILITY) return;
    if (activator.flags & FL_NOTARGET) return;

    // Monster angry! Hunt enemy!?!
    self.enemy = activator;

    // If wakeup trigger setup do not wait, special animation
    // This is really designed to go with a breakable trigger
    if (self.wakeuptrigger && self.th_wakeup) {
        self.wakeuptrigger = FALSE;    // Trigger no longer needed, reset
        FoundHuntTarget(TRUE);        // Setup goals and warn other monsters
        self.th_wakeup();
    }
    else {
        // delay reaction (wait one frame) so if the monster
        // has been teleported the sound is heard correctly
        self.nextthink = time + 0.1;
        self.think = FoundTarget;
    }
};

/*======================================================================
 liquid_check
  for some reason monsters never take damage from liquids
    This functions checks for liquid content and applies a
    modifier damage (they die too slow otherwise)
    * If a monster is in liquid there is often no way out!
    * Moves the content check to the monsters feet (shallow liquid)
======================================================================*/
void() monster_liquid_check =
{
    local float monster_dmg;
    local vector monster_feet;
    
    // This function is for monsters only (high damage)
    if (self.no_liquiddmg) return;
    if (query_configflag(SVR_LIQDAM)) return;
    if ( !(self.flags & FL_MONSTER)) return;
    if (self.health < 1) return;
    if (self.liquidcheck > time) return;
        
    // Reset liquid damage timer
    self.liquidcheck = time + LIQUID_TIMER;
    
    // Check for liquid at monsters feet (bottom of bounding box)
    monster_feet = self.origin; 
    monster_feet_z = monster_feet_z + self.mins_z;
    self.liquidbase = pointcontents(monster_feet);

    // Setup standard damage for liquid types
    if (self.liquidbase == CONTENT_SLIME) monster_dmg = SLIME_DAMAGE * MON_MULTIPLIER;
    else if (self.liquidbase == CONTENT_LAVA) monster_dmg = LAVA_DAMAGE * MON_MULTIPLIER;
    else return;

    // Gib monster if about to die
    if (self.health < monster_dmg + 5) {
        monster_dmg = self.health + 5;
        self.gibondeath = 1;
    }

    // Liquid damage
    self.pain_finished = 0;        // Always pain
    T_Damage (self, world, world, monster_dmg, DAMARMOR);
};

/*======================================================================
 monster_idle_sound
 - use one routine so more stuff can be added easily
 - easier to add any exceptions this way
 - zombies have their own unique idle routine
 - tarby added using sight sound
======================================================================*/
void() monster_idle_sound =
{
    // No monsters, dead and timer not reset?
    if ( !(self.flags & FL_MONSTER) ) return;
    if (self.health < 1) return;
    if (self.idletimer > time) return;
    if (self.spawnflags & MON_SPAWN_NOIDLE) return;

    self.idletimer = time + 5 + (random() * 3);
    if (!self.idlemoreoften && random() > MON_IDLE_SOUND) return;
        
    // Is the monster active in combat (special idle sound)
    if (self.enemy && self.idle_soundcom != "") {
        if (self.idle_soundcom2 != "") {
            if (random() < 0.5) sound (self, CHAN_VOICE, self.idle_soundcom, 1, ATTN_NORM);
            else sound (self, CHAN_VOICE, self.idle_soundcom2, 1, ATTN_NORM);
        }
        else sound (self, CHAN_VOICE, self.idle_soundcom, 1, ATTN_NORM);
    }
    else {
        // setup each monster with unique idle sounds (easier and quicker)
        if (self.idle_sound2 != "") {
            if (random() < 0.5) sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
            else sound (self, CHAN_VOICE, self.idle_sound2, 1, ATTN_NORM);
        }
        else sound (self, CHAN_VOICE, self.idle_sound, 1, ATTN_NORM);
    }
};

/*======================================================================
 monster_sightsound
  - Switched sound channel to CHAN_BODY so death always stops it
  - Randomnly get sound file cut off using voice channel
======================================================================*/
void() monster_sightsound =
{
    local float    rsnd;

    // No monsters, dead and timer not reset?
    if ( !(self.flags & FL_MONSTER) ) return;
    if (self.health < 1) return;
    if (!self.sight_sound) return;
    if (self.spawnflags & MON_SPAWN_NOSIGHT) return;
    if (intermission_running > 0) return;        // intermission or finale

    // Only do a sight sound when necessary, otherwise it overlaps and gets messy    
    if (self.sight_timeout < time) {
        self.sight_timeout = time + MON_SIGHTSOUND;        // Use pre-defined reset values
    
        rsnd = random();
        // The id enforcer has four sight sounds, which is a bit excessive
        // Check through sight strings to find out quantity
        if (self.sight_count == 2) {
            if (rsnd < 0.5) sound (self, CHAN_BODY, self.sight_sound, 1, ATTN_NORM);
            else sound (self, CHAN_BODY, self.sight_sound2, 1, ATTN_NORM);
        }
        else if (self.sight_count == 3) {
            if (rsnd < 0.3) sound (self, CHAN_BODY, self.sight_sound, 1, ATTN_NORM);
            else if (rsnd < 0.6) sound (self, CHAN_BODY, self.sight_sound2, 1, ATTN_NORM);
            else sound (self, CHAN_BODY, self.sight_sound3, 1, ATTN_NORM);
        }
        else if (self.sight_count == 4) {
            if (rsnd < 0.25) sound (self, CHAN_BODY, self.sight_sound, 1, ATTN_NORM);
            else if (rsnd < 0.5) sound (self, CHAN_BODY, self.sight_sound2, 1, ATTN_NORM);
            else if (rsnd < 0.75) sound (self, CHAN_BODY, self.sight_sound3, 1, ATTN_NORM);
            else sound (self, CHAN_BODY, self.sight_sound4, 1, ATTN_NORM);
        }
        // setup each monster with unique sight sounds (easier and quicker)
        else sound (self, CHAN_BODY, self.sight_sound, 1, ATTN_NORM);
    }
};

/*======================================================================
 monster_footstep
  This function will play a footstep sound
  * Types : Slow, Drag, Light, Medium, Heavy, Large
  * called from animation blocks to sync with sound
======================================================================*/
void(float altfoot) monster_footstep = 
{
    local float footstep, footstepnext, footvol;
    local string footstepwav;
    
    if (query_configflag(SVR_FOOTSTEP)) return;    // Default(0) = ON
    if (self.health < 1) return;
    if (self.movetype == MOVETYPE_NOCLIP) return;
    if (self.watertype < CONTENT_EMPTY) return;
    if (self.steptype == FS_FLYING) return;
    
    // Wide volume range for player
    if (self.flags & FL_CLIENT) footvol = 0.2 + random()*0.6;
    // feetsteps need to be louder during combat
    else if (self.enemy) footvol = 1;
    // Dogs have too many legs making noise, reduce volume    
    else if (self.classtype == CT_MONDOG) footvol = 0.2 + random()*0.6;
    // Slight random volume level out of combat
    else footvol = 0.7 + random()*0.3;
    
    // Decide which foot sound to play, long live clubfoot!
    if (altfoot) {
        // Switch to ALTernative foot sound
        footstep = self.altsteptype;
        if (self.altsteplast < 1) self.altsteplast = rint(1 + random()*4);

        // Increase footstep sound index by one 
        footstepnext = rint(self.altsteplast + 1);
        // Every loop around, randomly reset
        if (footstepnext > 5) footstepnext = rint(1 + random()*4);
        self.altsteplast = footstepnext;
    }
    else {
        // Setup last footstep sound
        footstep = self.steptype;
        if (self.steplast < 1) self.steplast = rint(1 + random()*4);

        // Player is randomly selected footsteps
        if (self.flags & FL_CLIENT) {
            // Quake random function is not really super random
            // Probably a bad seed starting point for function
            // Use 3 randoms to create something more random!
            footstepnext = rint(self.steplast + random() + random() + random());
            if (footstepnext > 5) footstepnext = footstepnext - 5;
        }
        else {
            // Increase footstep sound index by one 
            footstepnext = rint(self.steplast + 1);
            // Every loop around, randomly reset
            if (footstepnext > 5) footstepnext = rint(1 + random()*4);
        }

        // update last footstep index
        self.steplast = footstepnext;
    }
    
    // Cycle through all footstep types and work out correct sound file
    // All footstep types reduced down to 5 possible choices (speed things up)
    // Could store sound files with entity and cycle round quicker, req more memory
    // re-checking the sound file every footstep is costly on time
    // Luckly most footstep sounds are several frames apart (too noisy as well)
    if (footstep == FS_TYPELIGHT) {
        // Light heal/ paw sound
        if (footstepnext < 2) footstepwav = SOUND_FS_LIGHT1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_LIGHT2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_LIGHT3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_LIGHT4;
        else footstepwav = SOUND_FS_LIGHT5;
    }
    else if (footstep == FS_TYPEMEDIUM) {
        // Average foot/boot sound
        if (footstepnext < 2) footstepwav = SOUND_FS_MEDIUM1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_MEDIUM2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_MEDIUM3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_MEDIUM4;
        else footstepwav = SOUND_FS_MEDIUM5;
    }
    else if (footstep == FS_TYPEHEAVY) {
        // Heavy foot with slight echo
        if (footstepnext < 2) footstepwav = SOUND_FS_HEAVY1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_HEAVY2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_HEAVY3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_HEAVY4;
        else footstepwav = SOUND_FS_HEAVY5;
    }
    else if (footstep == FS_TYPELARGE) {
        // Large foot with large echo
        if (footstepnext < 2) footstepwav = SOUND_FS_LARGE1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_LARGE2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_LARGE3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_LARGE4;
        else footstepwav = SOUND_FS_LARGE5;
    }
    else if (footstep == FS_TYPEGIANT) {
        // Giant foot with long echo
        if (footstepnext < 2) footstepwav = SOUND_FS_GIANT1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_GIANT2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_GIANT3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_GIANT4;
        else footstepwav = SOUND_FS_GIANT5;
    }
    else if (footstep == FS_TYPECUSTOM) {
        // Custom feet sounds (usually boss type creatures)
        if (footstepnext < 2) footstepwav = self.stepc1;
        else if (footstepnext < 3) footstepwav = self.stepc2;
        else if (footstepnext < 4) footstepwav = self.stepc3;
        else if (footstepnext < 5) footstepwav = self.stepc4;
        else footstepwav = self.stepc5;
    }
    else if (footstep == FS_TYPEDRAG) {
        // Small scraping foot on ground
        if (footstepnext < 2) footstepwav = SOUND_FS_DRAG1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_DRAG2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_DRAG3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_DRAG4;
        else footstepwav = SOUND_FS_DRAG5;
    }
    // FS_TYPESLOW (default)
    else {
        // Souless shoe foot sound
        if (footstepnext < 2) footstepwav = SOUND_FS_SLOW1;
        else if (footstepnext < 3) footstepwav = SOUND_FS_SLOW2;
        else if (footstepnext < 4) footstepwav = SOUND_FS_SLOW3;
        else if (footstepnext < 5) footstepwav = SOUND_FS_SLOW4;
        else footstepwav = SOUND_FS_SLOW5;
    }
    
    // Play the sound (large feet need to be heard further away)
    if (footstep == FS_TYPELARGE) sound (self, CHAN_FEET, footstepwav, footvol, ATTN_FEETL);
    else sound (self, CHAN_FEET, footstepwav, footvol, ATTN_FEET);
};

//======================================================================
// PRE/POST CHECK conditions for monster death
//======================================================================
void() monster_death_precheck =
{
    if (self.flags & FL_MONSTER) {
        // Check for minion monster and update parent counters
        if (self.owner.minion_active) update_minioncount(self.owner, -1);

        self.deadflag = DEAD_DEAD;    // Its dead jim!
        self.effects = 0;            // Remove effects on death
        self.think = SUB_Null;        // No more thinking/animation
        self.nextthink = -1;        // Never fire think
        
        monster_check_gib();        // Check for gib state
    }
};

//----------------------------------------------------------------------
void() monster_fade =
{
    // Make sure model/entity is removed
    self.height = 0;
    self.think = model_fade;
    self.nextthink = time + 0.1;
    self.ltime = self.nextthink;
};

//----------------------------------------------------------------------
void() monster_death_postcheck =
{
    local float bodytimer;
    
    self.blockudeath = TRUE;    // Body is dead, no human death noise
    if (!self.gibbed) self.bodyonflr = MON_ONFLR;

    // Allow for the dead body/head to touch triggers (void/hurt)
    setorigin(self, self.origin);
    self.solid = SOLID_TRIGGER;
    self.touch = SUB_Null;
    
    // Check for global or individual body fading mechanic
    if (self.bodyfadeaway > 0 || map_bodyfadeaway > 0) {
        if (map_bodyfadeaway > 0) bodytimer = map_bodyfadeaway;
        else bodytimer = self.bodyfadeaway;
        self.pain_finished = time + 10 + random() * bodytimer;
    }
    // Body sticks around
    else self.pain_finished = time + LARGE_TIMER;
};

//----------------------------------------------------------------------
void() monster_deadbody_check =
{
    // Remove/Fade out body if timer is finished
    if (self.pain_finished < time) { monster_fade(); return; }
    // Exit condition, body no longer has any interaction
    if (self.deadflag == DEAD_FINISHED) return;
    
    // Check for deadflag first, if touched something this will be changed
    if (self.deadflag == DEAD_REMOVE || self.deadflag == DEAD_EXPLODE
        && self.gibbed == FALSE) {
        // make sure touch,solid and body axe interaction are off
        self.touch = SUB_Null;
        self.solid = SOLID_NOT;
        self.bodyonflr = "";
        self.gibhealth = TRUE;
        // Only two options available, gib explode or remove
        if (self.deadflag == DEAD_EXPLODE) self.think = monster_ThrowGib;
        // Replace animation frame group think function
        else self.think = SUB_Remove;
        self.nextthink = time + 0.1;
        // Prevent this function from running again
        self.deadflag = DEAD_FINISHED;
    }
    else {
        // Check global map variable first (default = off)
        // Check floor below dead body (global function)
        if (map_bodyflrcheck == TRUE)
            ent_floorcheck(self, FLOOR_TRACE_MONSTER);

        // Keep checking for dead body/head conditions
        self.think = monster_deadbody_check;
        self.nextthink = time + 0.1;    
    }
};

/*======================================================================
 monster_death_use
  - When a mosnter dies, it fires all of its targets with the current
        enemy as activator.
======================================================================*/
void() monster_death_use =
{
    // fall to ground or stop swimming
    if (self.flags & FL_FLY) self.flags = self.flags - FL_FLY;
    if (self.flags & FL_SWIM) self.flags = self.flags - FL_SWIM;

    // Always use a deathtarget if one defined
    if (self.deathtarget != "") {
        // Validate deathtarget exists before firing it
        self.movelast = find(world, targetname, self.deathtarget);
        // Deathtarget valid, switch around and fire targets
        if (self.movelast) self.target = self.deathtarget;
    }

    // Is there no target defined?
    if (self.target == "") return;

    activator = self.enemy;
    SUB_UseTargets ();
};

/*======================================================================
 monster_paincheck
  - Tests all pain conditions and returns what to do
  * 0 = Nothing
  * 1 = Sound + Animation
  * 2 = Sound + Long Animation
  * 3 = Sound ONLY
======================================================================*/
void(entity attacker, float damage) monster_pain_check =
{
    self.pain_check = 0;    // Reset pain check
    
    // already dying, don't go into pain frame
    if (self.health < 1) self.pain_check = 0;
    // The new axe forces monsters into long pain animations (if they exist)
    else if (attacker.moditems & IT_UPGRADE_AXE && self.axhitme > 0 && self.pain_longanim) self.pain_check = 2;
    // always go into pain frame if it has been a while (first hit = pain)
    else if (time - self.pain_finstate > PAIN_ALWAY) self.pain_check = 1;
    // Dangerous liquids should kill the monster and keeping them in
    // constant pain animations which makes them look better than doing nothing!
    else if (self.liquidbase == CONTENT_SLIME || self.liquidbase == CONTENT_LAVA) self.pain_check = 1;
    // If being attacked by a world object (shooter, electricity, bmodel) in pain
    else if (attacker == world) self.pain_check = 1;
    // Random chance to flinch and not ignore the pain (play sound only)
    else if (random()* self.pain_flinch > damage ) self.pain_check = 3;
    // DEFAULT  : pain animation + sound (last condition)
    else self.pain_check = 1;
};

/*======================================================================
 monster_targets (second part of setup function)
  - Checks for any targets to get angry at or stand/walk around
======================================================================*/
void() monster_targets =
{
    // Reset everything first
    self.enemy = self.goalentity = self.movetarget = world;

    //----------------------------------------------------------------------
    // * Monsters can spawn angry at the player/activator
    // * the target key can point to a path_corner
    // * If the target key points at another monster they will infight
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_SPAWN_ANGRY || self.angrytarget) {
        // Check if the activator is a player? if not find a player
        if (activator.flags & FL_CLIENT) self.enemy = activator;
        else self.enemy = checkclient ();
        
        // Double check enemy is player and has notarget or wearing RoS?
        if (self.enemy.flags & FL_CLIENT) {
            if (self.enemy.flags & FL_NOTARGET) self.enemy = world;
            else if (self.enemy.items & IT_INVISIBILITY) self.enemy = world;
        }
        // If the activator not a player, reset to world
        // Cannot get angry at triggers, use angrytarget for infighting
        else self.enemy = world;
            
        // If an alternative angry target defined, find it and attack!    
        if (self.angrytarget) {
            self.oldenemy = find(world, targetname, self.angrytarget);
            // Is the attack target a monster and spawned/active (take damage)
            if (self.oldenemy.flags & FL_MONSTER && self.oldenemy.takedamage != DAMAGE_NO)
                self.enemy = self.oldenemy;
        }

        // Check the enemy is a player/monster and alive
        if (self.enemy.flags & (FL_CLIENT | FL_MONSTER) && self.enemy.health > 0) {
            self.nextthink = time + 0.1;
            self.think = FoundTarget;
            monster_sightsound ();    // Wake up sound
            return;
        }
    }
    else {
        // If the monster has a target check if path_corner or something to attack?
        // Do nothing with patrol paths if spawning angry with the player
        if (self.target != "") {
            self.movetarget = find(world, targetname, self.target);
            // Cannot chase a target if setup as a turret
            if (self.movetarget && self.movespeed != -1) {
                if (self.movetarget.classtype == CT_PATHCORNER) {
                    self.goalentity = self.movetarget;
                    self.ideal_yaw = vectoyaw(self.goalentity.origin - self.origin);
                    self.nextthink = time + 0.1 + random()*0.5;
                    self.think = self.th_walk;
                    return;
                }
            }
        }
    }

    // no angrytarget, enemy or path corner, stand around waiting
    self.nextthink = time + 0.1 + random()*0.5;
    self.think = self.th_stand;                    // Stand around
};

/*======================================================================
 monster_setup
  - Setup monster ready for action
======================================================================*/
void() monster_spawn =
{
    //----------------------------------------------------------------------
    // Check for Axe / Shotgun / LG upgrade monster spawn exceptions?
    if (self.upgrade_axe || self.upgrade_ssg || self.upgrade_lg) {
        // Has ANY player (server test not individual) 
        // picked up the relevant upgrade weapons?
        if (self.upgrade_axe && !query_configflag(SVR_UPDAXE) ) return;
        if (self.upgrade_ssg && !query_configflag(SVR_UPDSSG) ) return;
        if (self.upgrade_lg && !query_configflag(SVR_UPDLG) ) return;

        // Update monster count (not added to monster count until spawned)
        total_monsters = total_monsters + 1;
        update_hud_totals(HUD_MONSTERS);
    }

    //----------------------------------------------------------------------
    // Check if the monster can spawn without telefragging something
    if (self.spawnnotelefrag > 0) {
        // Switch on world interaction
        self.solid = SOLID_SLIDEBOX;
        self.movetype = MOVETYPE_STEP;
        // restore model/bbox to get world collision
        setmodel(self, self.mdl);
        setsize (self, self.bbmins, self.bbmaxs);
        // reset origin, just in case been moved since last
        setorigin(self, self.oldorigin);
        
        // Make sure movement flag types are correct
        if (self.classmove == MON_MOVEFLY ) 
            self.flags = self.flags | FL_FLY;
        else if (self.classmove == MON_MOVESWIM ) 
            self.flags = self.flags | FL_SWIM;

        // Can the monster walk without collision?
        if (!walkmove (0, 0)) {
            dprint("BLOCKED "); dprint(self.classname); dprint(" ");
            // Switch off monster and wait again
            self.solid = SOLID_NOT;
            setmodel(self,"");
            setsize(self, VEC_ORIGIN, VEC_ORIGIN);
            self.think = monster_spawn;
            self.nextthink = time + 0.1 + random()*0.5;
            return;
        }
        else {
            // make sure monster is not displayed yet
            // might have a nosight check to do
            self.solid = SOLID_NOT;
            setmodel(self,"");
            setsize(self, VEC_ORIGIN, VEC_ORIGIN);
        }
    }

    //----------------------------------------------------------------------
    // Check for spawnnosight conditions
    if (self.spawnnosight > 0) {
        // Setup maximum time limit for spawning regardless
        if (self.attack_finished == FALSE) {
            // Default is 30s (check for override condition first)
            if (self.spawnnosighttime < 1) self.spawnnosighttime = 30;
            self.attack_finished = time + self.spawnnosighttime;
        }
        
        // Reset sight condition first and find client in PVS
        self.lefty = FALSE;
        self.oldenemy = checkclient();
        // Did the client PVS check return an entity?
        if (self.oldenemy) {
            // is the client a player?
            if (self.oldenemy.flags & FL_CLIENT) {
                // Check if the player can be seen?
                enemy_vis = visible(self.oldenemy);
                if (enemy_vis) self.lefty = TRUE;
                // Check for an insight spawn distance
                if (self.spawnnosight > 1) {
                    // Find distance (3d vector distance)
                    self.enemydist = range_distance(self.oldenemy, FALSE);
                    // check for player being too close for spawn
                    if (self.enemydist > self.spawnnosight)
                        self.lefty = FALSE;
                }
            }
            // Checks for notarget/inv player conditions
            // disabled - not really necessary as a spawn condition
            //if (self.oldenemy.flags & FL_NOTARGET) self.lefty = FALSE;
            //if (self.oldenemy.items & IT_INVISIBILITY) self.lefty = FALSE;
        }
        
        // Can the player see the spawn location? wait instead
        if (self.lefty == TRUE && self.attack_finished > time) {
            self.think = monster_spawn;
            self.nextthink = time + 0.1;
            return;
        }
    }
        
    //----------------------------------------------------------------------
    // Time to finally spawn the monster!?! All conditions met!
    
    if (self.think1) self.use = self.think1;    // different use function?
    else self.use = monster_use;                // default trigger event
    self.solid = SOLID_SLIDEBOX;                // Standard monster movement
    self.movetype = MOVETYPE_STEP;                // Standard monster movement
    setmodel(self, self.mdl);                    // Setup model
    self.skin = self.skin_override;                // Restore any skins
    self.frame = self.frame_override;            // Restore any frames
    setsize (self, self.bbmins, self.bbmaxs);    // Restore BB size

    // Check for delay spawn monster count
    if (self.delaymonstercount) {
        // Reset both monster count conditions
        self.nomonstercount = self.delaymonstercount = 0;
        total_monsters = total_monsters + 1;
        update_hud_totals(HUD_MONSTERS);
    }
    
    // Restore any effect flags settings (various dlight glows)
    if (self.savedeffects > 0) self.effects = self.savedeffects;
    
    // Should grenades bounce off the body?
    if (self.bouncegrenade) self.takedamage = DAMAGE_YES;
    else self.takedamage = DAMAGE_AIM;            // Can receive damage

    self.velocity = '0 0 0';                    // Make sure stationary
    self.deadflag = DEAD_NO;                    // used to stop death re-triggering
    self.liquidbase = self.liquidcheck = 0;        // Used for liquid content damage
    self.dmgcombined = self.dmgtimeframe = 0;    // combined damage over 0.1s
    if (!self.pain_flinch) self.pain_flinch = self.health;
    if (!self.switchoverride) self.switchoverride = 1;

    // Double check that all ammo resistance are within range
    Resist_CheckRange(self);

    // Check for tether system (special target field)
    setup_tethersystem();
    
    // Long time before first idle sound (except scorpions!)
    if (self.idletimer < 1) {
        if (self.classtype == CT_MONSCORPION) self.idletimer = time + 0.1 + random();
        else self.idletimer = time + 4 + (random() * 4);
    }
    
    // Reset all enemy tracking entities
    if (!self.enemy) self.enemy = self.movetarget = self.goalentity = world;
    self.pausetime = LARGE_TIMER;

    // Setup pain tolerence level based on current skill level
    if (!self.pain_timeout) self.pain_timeout = 1;    // Default
    if (skill == SKILL_HARD) self.pain_timeout = self.pain_timeout + 1;
    if (skill == SKILL_NIGHTMARE) self.pain_timeout = self.pain_timeout + 3;

    //----------------------------------------------------------------------
    // Setup different skin options
    //----------------------------------------------------------------------
    if (self.exactskin > 0) self.skin = rint(self.exactskin);
    else if (self.randomskin > 1) self.skin = rint(random()*(self.randomskin-1));
    if (self.skin < 0) self.skin = 0;    // Double check no negatives

    //----------------------------------------------------------------------
    self.ideal_yaw = self.angles * '0 1 0';
    if (self.classmove == MON_MOVEWALK ) {        // Walking Monsters
        if (!self.yaw_speed) self.yaw_speed = 20;
    }
    else if (self.classmove == MON_MOVEFLY ) {    // Flying Monsters
        if (!self.yaw_speed) self.yaw_speed = 15;
        self.flags = self.flags | FL_FLY;
    }
    else if (self.classmove == MON_MOVESWIM ) {    // Swimming Monsters    
        if (!self.yaw_speed) self.yaw_speed = 10;
        self.flags = self.flags | FL_SWIM;
    }

    // Not all bounding boxes extend up very high, make sure the view_ofs
    // is relevant to where the top of the bounding box is
    if (self.view_ofs_z == 0) {
        if (self.maxs_z <= MON_VIEWOFS) self.view_ofs_z = self.maxs_z*0.5;
        else self.view_ofs_z = MON_VIEWOFS;
    }
    
    //----------------------------------------------------------------------
    // Is the monster a zombie (type) lying on the floor? (starting pose)
    // Angles key used for specific facing direction, 0 = random setup
    // This function check is before any droptofloor or content checks
    // It will allow onfloor monsters to exist in tight spaces, shallow graves
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_ONFLOOR && self.classgroup == CG_ZOMBIE) {
        if (self.classtype == CT_MONZOMBIEK) zombiek_onground(MONAI_ZOMBIEFLR);
        else zombie_onground(MONAI_ZOMBIEFLR);
        return;
    }

    //----------------------------------------------------------------------
    // Special minions (start small and grow fast)
    // CT_MINIONSPIDER, CT_MINIONVORELING, CT_MINIONGARGOYLE
    //----------------------------------------------------------------------
    if (self.minion_active) {
        self.angles = vectoangles(self.enemy.origin - self.origin);
        self.angles_x = self.angles_z = 0;
        update_hud_totals(HUD_MONSTERS);
        self.pain_finished = time + 1.2;
        self.th_stand();
        return;
    }    
    
    //----------------------------------------------------------------------
    // Perched Gargoyles/Gaunt have special idle animation (sitting)
    // and need to fly up before resuming any normal behaviour 
    // (most think functions are intercepted)
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONGARGOYLE && self.spawnflags & MON_GARGOYLE_PERCH ||
        self.classtype == CT_MONGAUNT && self.spawnflags & MON_GAUNT_PERCH ) 
    {
        self.flags = self.flags | FL_FLY;    // Stop any ground checks
        self.th_stand();
        return;
    }

    //----------------------------------------------------------------------
    // Ceiling critters have special idle animation (rotated)
    // and need to let go of the ceiling before resuming any
    // normal behaviour (most think functions are intercepted)
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONSPIDER && self.spawnflags & MON_SPIDER_CEILING ||
        self.classtype == CT_MONSWAMPLING && self.spawnflags & MON_SWAMPLING_CEILING ||
        self.classtype == CT_MONVORELING && self.spawnflags & MON_VORELING_CEILING) {
        // Work out where the ceiling is (traceline upwards)
        traceline (self.origin, self.origin+'0 0 4096', TRUE, self);
        
        // Check for empty content before moving
        if (pointcontents(trace_endpos) == CONTENT_EMPTY) {
            self.flags = self.flags | FL_FLY;    // Stop any ground checks
            self.classmove = MON_MOVEFLY;        // Avoid ground check function
            self.origin = trace_endpos;            // Move critter to ceiling
            setorigin(self, self.origin);
            // If the view_ofs is not underneath the critter, no sight
            // functions will work (checkclient for example)
            self.view_ofs = '0 0 -24';
            self.yaw_speed = 20;        // Ground yaw speed
            setsize(self, '-16 -16 -24', '16 16 0');
        }
        else {
            dprint("\b[MONSTER]\b Trying to place on ceiling, no space!\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
            return;
        }
    }
    
    //----------------------------------------------------------------------
    // Check to see if monster (walking) are stuck in geometry
    if (self.classmove == MON_MOVEWALK) {
        self.origin_z = self.origin_z + 1;
        droptofloor();
        if (!walkmove(0,0)) {
            // Tempoaraily use lip variable for stuck condition
            self.lip = TRUE;
            // Some monster placement in the original ID maps NEED original Bounding Boxes
            // this is something that cannot be easily fixed, so go back to original BB
            // and test the monster placement again
            if (CheckZeroVector(self.idmins) == FALSE) {
                self.bbmins = self.idmins; self.bbmaxs = self.idmaxs;
                setsize (self, self.bbmins, self.bbmaxs);    // Restore ID BB size
                self.origin_z = self.origin_z + 1;
                droptofloor();
                // re-test monster movement using ID Bounding Box
                if (walkmove(0,0)) self.lip = FALSE;
            }
            
            // It seems that some mappers want to spawn stuff in mid air
            // and let it drop naturally over time (>1 frame)
            // check for empty point content at origin as final test
            if (pointcontents(self.origin) == CONTENT_EMPTY) self.lip = FALSE;
            
            // Is the monster stuck with new/old bounding box?
            // Stuck monsters contribute towards level monster totals
            if (self.lip) {
                // If the monster is stuck and delay spawned, gib instead
                if (self.spawnflags & MON_SPAWN_DELAY) {
                    self.health = self.gibhealth;
                    Killed(self, self);
                }
                else {
                    // this condition should be a map spawn event only
                    dprint ("\b[STUCK]\b "); dprint (self.classname);
                    dprint (" at "); dprint (vtos(self.origin));
                    dprint ("\n");
                    spawn_marker(self.origin, SPNMARK_YELLOW);
                    remove(self);
                }
                return;
            }
        }
    }

    //----------------------------------------------------------------------
    // Telefrag (kill) anything at monster position
    // Phased monsters cannot telefrag or produce spawn effects
    if (!self.bodyphased) {
        // New spawn condition via entity key nospawndamage
        // This is to prevent excessive damage to breakables
        // Monsters can spawn in very wierd situations (inside things)
        // if spawning damage is disabled (spawn_tdeath)
        if (self.nospawndamage == 0) 
            spawn_tdeath(self.origin, self);    // 50K damage!
        
        // Don't show spawning effect if delay spawn + nogfx
        if (self.spawnflags & MON_SPAWN_DELAY && !(self.spawnflags & MON_SPAWN_NOGFX)) 
            spawn_tfog(self.origin);
    }

    //----------------------------------------------------------------------
    // Hell Knights can be setup to point at a target and fire lightning
    // This uses the magicB attack by ID software that was never used
    // When the Hell Knight recieves damage from the player or triggered
    // will revert back to normal walking/talking/fighting monster!
    // ** Has unique use/think function
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONHELLK && self.spawnflags & MON_POINT_KNIGHT) {
        self.nextthink = time + 0.1 + random()*0.5;
        self.think = self.th_stand;            // statue function
        return;
    }

    //----------------------------------------------------------------------
    // Knights, Hell Knights, Golems and Gargoyles can start as statues
    // They are frozen in an selected pose and wake up on trigger
    // Have different skin, high pain resistance and stone gibs!
    // Added double check for statue spawnflag, just in case
    // ** Has unique use/think function
    //----------------------------------------------------------------------
    if (self.spawnstatue && self.spawnflags & MON_STATUE) {
        self.takedamage = DAMAGE_NO;        // No damage till wakeup
        self.nextthink = time + 0.1 + random()*0.5;
        self.think = self.th_stand;            // statue function
        return;
    }

    //----------------------------------------------------------------------
    // Start in special standby mode with a blue halo glowing shield
    // Will wait for a trigger before reverted back to normal gameplay
    // ** Has unique use/think function
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONSEEKER && self.spawnflags & MON_SEEK_SHIELD) {
        self.takedamage = DAMAGE_NO;        // No damage till wakeup
        self.nextthink = time + 0.1 + random()*0.5;
        self.think = self.th_stand;            // statue function
        return;
    }
    
    //----------------------------------------------------------------------
    // Once the skull wizard is setup on the ground, phase out body
    //----------------------------------------------------------------------
    if (self.classtype == CT_MONSKULLW && self.bodyphased == MONAI_SKULLWINVIS) {
        self.takedamage = DAMAGE_NO;
        self.solid = SOLID_NOT;
        setmodel(self,"");
    }
    //----------------------------------------------------------------------
    // No more spawn exception check for targets
    monster_targets();
};

//----------------------------------------------------------------------
// Setup bounding box for monster (based on types)
//----------------------------------------------------------------------
void() monster_bbox = 
{
    if (self.bboxtype == BBOX_CUSTOM) return;
    else if (self.bboxtype == BBOX_TINY)
        // Lost Soul, Scorpion, Spider, Voreling
        { self.bbmins = '-16 -16 -24'; self.bbmaxs = '16 16 16'; }
    else if (self.bboxtype == BBOX_SHORT)
        // Player, Death Guards, Knights, Crossbow Knights, 
        // Army, Army_Rocket, Army_Grenade, Army_Plasma, Jim
        // Zombie, Poison Zombie, Zombie Knight
        { self.bbmins = '-16 -16 -24'; self.bbmaxs = '16 16 32'; }
    else if (self.bboxtype == BBOX_TALL)
        // Hell Knight, Death Knight, Fury Knight, Sergeant
        // Enforcer, Defender, Eliminator, Pyro, Centurion
        // Skull Wizard, Wizard, Gargoyles, Tarbaby, Wraith
        { self.bbmins = '-16 -16 -24'; self.bbmaxs = '16 16 40'; }
    else if (self.bboxtype == BBOX_WIDE)
        // Demon, Ogre, Hunter Ogre, _Mace, _Hammer, Shalrath
        { self.bbmins = '-24 -24 -24'; self.bbmaxs = '24 24 40'; }
    else if (self.bboxtype == BBOX_GIANT)
        // Drole, Minotaur
        { self.bbmins = '-24 -24 -24'; self.bbmaxs = '24 24 64'; }
    else if (self.bboxtype == BBOX_MASSIVE)
        // Shambler, ID Ogre
        { self.bbmins = '-32 -32 -24'; self.bbmaxs = '32 32 64'; }

        // Some monsters are custom sizes
    else if (self.bboxtype == BBOX_DOG)
        { self.bbmins = '-20 -20 -24'; self.bbmaxs = '20 20 16'; }
    else if (self.bboxtype == BBOX_FISH)
        { self.bbmins = '-16 -16 -24'; self.bbmaxs = '16 16 24'; }
    else if (self.bboxtype == BBOX_FISHS)
        { self.bbmins = '-12 -12 -14'; self.bbmaxs = '12 12 14'; }
    else if (self.bboxtype == BBOX_EEL)
        { self.bbmins = '-16 -16 -16'; self.bbmaxs = '16 16 16'; }
    else if (self.bboxtype == BBOX_HYDRA)
        { self.bbmins = '-20 -20 -16'; self.bbmaxs = '20 20 16'; }
    else if (self.bboxtype == BBOX_GOLEM)
        { self.bbmins = '-28 -28 -24'; self.bbmaxs = '28 28 80'; }
    
    // default bounding box = TALL
    else { self.bbmins = '-16 -16 -24'; self.bbmaxs = '16 16 40'; }
};

/*======================================================================
 walkmonster_start
  - Main entry point for ALL monster routines
======================================================================*/
void() monster_start =
{
    self.flags = FL_MONSTER;            // Always reset this flag
    self.skin_override = self.skin;        // Save for later
    self.frame_override = self.frame;
    
    // Check for spawning conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
    
    // Warning if effects flag is active before spawning
    if (self.effects) {
        dprint("\b[MONSTER]\b Effects flag active\n");
        self.savedeffects = self.effects;
    }
    // Reset effects flag because some engines will show effects
    // This is especially obvious for delay spawned monsters
    self.effects = 0;

    self.oldorigin = self.origin;        // Save origin
    self.max_health = self.health;        // Save max health
    if (!self.gibhealth) self.gibhealth = 0 - self.health;    // Default gib health
    if (self.turrethealth < 0 || self.turrethealth > 1) self.turrethealth = 0.5;

    // Check if jump function has been disabled?
    if (self.jump_flag < 0) self.jump_flag = LARGE_TIMER;
    
    // Default attack function and class group
    if (!self.th_checkattack) self.th_checkattack = CheckAttack;
    if (!self.classgroup) self.classgroup = CG_MONSTERS;
    
    // Highlight monsters with forced no_zaware entity key
    if (self.no_zaware && developer > 0 && !query_configflag(SVR_DEVHELPER)) 
        spawn_marker(self.origin+'0 0 32', SPNMARK_WHITE);

    // Setup bounding box based on presets
    monster_bbox();
    // Cannot have multiple upgrade restrictions on monsters
    remove_duplicate_upgrades();

    // Cannot delay spawn a monster if nothing can trigger it!?!
    if (self.spawnflags & MON_SPAWN_DELAY && self.targetname == "") {
        dprint("\b[MONSTER]\b Cannot delay spawn without targetname!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    
    // Check for Axe / Shotgun upgrade monster exceptions?
    // Don't add these kind of monster to the count until spawned
    if (self.upgrade_axe || self.upgrade_ssg || self.upgrade_lg) {
        if ( !(self.spawnflags & MON_SPAWN_DELAY) ) {
            dprint("\b[MONSTER]\b need spawn delay for axe/shotgun/lg\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
            return;
        }
        // make sure the monster has no nomonstercount exceptions
        if (self.nomonstercount) self.nomonstercount = 0;
    }
    else {
        // Allow mappers to spawn monsters that don't affect monster count
        // I know this can be a dangerous if used incorrectly, but good for statues
        // delaymonstercount = update count on spawn instead of death
        if (self.nomonstercount > 0 || self.delaymonstercount > 0)
        {
            if (developer > 1) {
                dprint("\b[MONSTER]\b ("); dprint(self.targetname);
                dprint(") - no monster count\n");
            }
        }
        // Default state - update monster total
        // HUD update done with client setup
        else total_monsters = total_monsters + 1;
    }
    
    //----------------------------------------------------------------------
    // Detect monster armour map hack
    if (self.armorvalue || self.armortype) {
    // This hack really should be stopped, no point upsetting mappers at this point
    //        self.armorvalue = self.armortype = 0;
        dprint("\b[MAPHACKS]\b Using armor on monsters, use health key instead!\n");
    }

    if (self.spawnflags & MON_SPAWN_DELAY) {
        setmodel(self, string_null);
        self.solid = SOLID_NOT;                // No world interaction
        self.use = monster_spawn;            // Wait for trigger
        
        if (developer > 0 && !query_configflag(SVR_DEVHELPER)) {
            self.movetype = MOVETYPE_NONE;
            self.solid = SOLID_NOT;
            setmodel(self, MODEL_BROKEN);
            if (self.nomonstercount == 1) self.skin = SPNMARK_GREEN;
            else self.skin = SPNMARK_BLUE;
            self.frame = 0;
        }
    }
    else {
        // Variable start delay on all monsters to reduce packet errors
        self.nextthink = time + 0.1 + random()*0.4;
        self.think = monster_spawn;
    }
};

//======================================================================
// Map hacks are not supported in this MOD
// There is no use in pretending otherwise!
//======================================================================
void() walkmonster_start_go = { remove(self); return; };
void() flymonster_start_go = { remove(self); return; };
void() swimmonster_start_go = { remove(self); return; };

/*======================================================================
/*QUAKED monster_x (1 0 0) (x y z) (x y z) AMBUSH x x NOSIGHT NOIDLE NOGFX STARTOFF ANGRY Not_Easy Not_Normal Not_Hard Not_DM
XXX, x health points.
-------- KEYS --------
targetname : monster/trigger name
target : Starting position or path_corner to walk towards (always define)
target2 : Additional trigger function (need target to be defined as well)
angrytarget : something (monster/player) to get angry at once spawned
deathtarget : entity to trigger upon death (useful if target field already in use)
health : Override default health settings
exactskin : Override default skin selection of 0 (no error checking)
upgrade_ssg : 1 = will only spawn if shotgun upgrade active on server
upgrade_axe : 1 = will only spawn if axe upgrade active on server
upgrade_lg : 1 = will only spawn if lightning gun upgrade active on server
nomonstercount : will not be included in any monster count functionality
infightextra : Damage multiplier for infighting damage
pain_ignore : 1 = Ignore pain when hit by other monsters
noinfighting : Will not react to any infighting (it can look stupid)
no_liquiddmg : Blocks all liquid (slime/lava) damage checks
no_zaware : All Z Aware projectiles will be disabled
bboxtype : Change bbox 1=Tiny,4=Short,5=Tall,7=Wide,8=Giant,10=Massive
gibondeath : 1 = always explode in a shower of gibs on death
bodyfadeaway : Time (secs) before body/head will fade away (default=0)
movespeed : -1 = no movement(turret), =0/1 free movement (default)
turrethealth : = 0.0->1.0; % of HP when monster turret is released 
turrettarget : Target(s) to fire when turret % HP is released

--- Knights ---
frame: statue frame to be frozen in. (default 44)
STATUE : Stone statue until triggered
NOTFROZEN : Will start active (works with statue spawnflag)

--- Hell Knight ---
frame: statue frame to be frozen in. (default 73)
STATUE : Stone statue until triggered
NOTFROZEN : Will start active (works with statue spawnflag)

--- Crossbow Knight ---
SNIPER : no max range limitations for enemies (sniper mode)
TRACKING : Enable tracking for the firing of bolts (really hard)

--- Golem ---
frame: statue frame to be frozen in. (default 48)
STATUE : Stone statue until triggered

--- Tarbaby ---
death_dmg : Damage on Death (def=120)
poisonous : 0=Jump attack (default, 1=Poison attack
exactskin : 0-1=Blue, 2-3=Green1, 4-5=Green2, 6-7=Green3

--- Skull Wizards ---
bodyphased   : Spawn phased out and wait to see player
bodystatic   : Prevents skull wizard from teleporting

-------- SPAWNFLAGS --------
AMBUSH : the monster will only wake up on seeing the player, not by another monster
NOSIGHT : No sight sound
NOIDLE : No idle sound
NOGFX : No spawn effect or sound when triggered
STARTOFF : Trigger Spawn
ANGRY : Trigger Spawn angry at the player
-------- NOTES --------
XXX, x health points.
======================================================================*/
