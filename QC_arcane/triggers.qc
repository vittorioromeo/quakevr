/*======================================================================
 TRIGGER FUNCTIONS
======================================================================*/
float TRIG_SPAWNBUBBLES = 2;    // Produce bubbles
float TRIG_SECRETNOMSG = 2;        // No default message
float TRIG_TELEPLAYER = 1;        // Player only
float TRIG_TELESILENT = 2;        // No teleport sound
float TRIG_TELEALWAYSON = 4;    // Trigger teleports starts on
float TRIG_HURTMONSTER = 4;        // Only hurt monsters
float TRIG_HURTFALLING = 32;    // Only hurt if player falling
float TRIG_PUSHONCE = 1;        // Switch off after one use
float TRIG_PUSHSILENT = 2;        // No wind sound for player
float TRIG_PUSHNOMONSTER = 4;    // Monsters cannot use this push trigger
float TRIG_CONVMONSTER = 2;        // Conveyor moving monsters
float TRIG_CONVITEM = 4;        // Conveyor moving items
float TRIG_CONVPUSHABLE = 8;    // Conveyor moving pushables
float TRIG_CMAPNOPAUSE = 1;        // No info screen, next map
float TRIG_CMAPRESETINV = 2;    // Reset inventory (shotgun+25shells)
float TRIG_CMAPSECSPAWN = 4;    // Spawn at the second spawn point
float TRIG_MONJUMPFLY = 2;        // Will affect flying units
float TRIG_MONJUMPSWIM = 4;        // Will affect swimming units
float TRIG_TSOUNDWGEO = 4;        // World Geometry interaction
float TRIG_TSOUNDDRAIN = 8;        // Drain effect when disabled
float TRIG_VOIDNOCLIENT = 1;    // Clients will suffer no damage
float TRIG_VOIDNOMONSTER = 2;    // Monsters will be immune to the void
float TRIG_VOIDNOAMMO = 4;        // Ammo projectiles will pass through
float TRIG_VOIDNOGG = 8;        // Minion eggs will ignore the void
float TRIG_VOIDNOTEMP = 16;        // Temporary entities will exist longer
float TRIG_VOIDNOITEM = 32;        // All items will carry on as before

//======================================================================
// If a dead body has touched a trigger, setup removal
//======================================================================
float(entity body_ent, float body_death) trigger_check_body =
{
    if (body_ent.flags & FL_MONSTER && body_ent.deadflag == DEAD_DEAD) {
        body_ent.deadflag = body_death;
        body_ent.touchedvoid = TRUE;
        return TRUE;
    }
    return FALSE;
};

//======================================================================
// Setup/Spawn/Remove bubbles inside a trigger volume
//======================================================================
void() trigger_setup_bubbles =
{
    // Quake compilers can add the style key to any entity connected
    // to dynamic lights, this is usually 32-64 in value and should
    // not interfere, but never use style > 32 for any values
    if (self.style == 1) self.mdl = "progs/s_bubble_grey.spr";
    else if (self.style == 2) self.mdl = "progs/s_bubble_brnd1.spr";
    else if (self.style == 4) self.mdl = "progs/s_bubble_grn1.spr";
    else if (self.style == 5) self.mdl = "progs/s_bubble_red1.spr";
    else if (self.style == 6) self.mdl = "progs/s_bubble_brnd2.spr";
    else if (self.style == 7) self.mdl = "progs/s_bubble_pinkyel.spr";
    else if (self.style == 8) self.mdl = "progs/s_bubble_brnl1.spr";
    else if (self.style == 9) self.mdl = "progs/s_bubble_purp1.spr";
    else if (self.style == 10) self.mdl = "progs/s_bubble_purp2.spr";
    else if (self.style == 11) self.mdl = "progs/s_bubble_brnl2.spr";
    else if (self.style == 12) self.mdl = "progs/s_bubble_grn2.spr";
    else if (self.style == 13) self.mdl = "progs/s_bubble_yellow.spr";
    else if (self.style == 14) self.mdl = "progs/s_bubble_blue2.spr";
    else if (self.style == 15) self.mdl = "progs/s_bubble_red2.spr";
    else self.mdl = "progs/s_bubble_blue1.spr";
    precache_model(self.mdl);
    
    // Setup defaults for healing bubbles
    self.waitmin = self.bubble_count = 0;        // Reset counters
    if (!self.count) self.count = 5;            // max active bubbles
    if (!self.height) self.height = self.size_z;
    if (self.height < 64) self.height = 64;        // min top of volume
    self.oldorigin = bmodel_origin(self);
    self.oldorigin_z = self.mins_z;
    self.t_width = (self.size_x/2);
    self.t_length = (self.size_y/2);
    // Default spawn rate for bubbles
    if (self.yaw_speed <= 0) self.yaw_speed = 0.5;
};

//----------------------------------------------------------------------
void() trigger_remove_bubble =
{
    if (other == self.owner) return;            // Touching self, do nothing
    if (other.solid == SOLID_TRIGGER) return;    // trigger field, do nothing

    self.touch = SUB_Null;
    if (self.owner.bubble_count > 0) 
        self.owner.bubble_count = self.owner.bubble_count - 1;
    remove(self);
};

//----------------------------------------------------------------------
void() trigger_update_bubble =
{
    // The size of the trigger is the default height for the bubbles
    // to raise up, or The height can be specified as a value

    self.lefty = fabs(self.origin_z - self.owner.oldorigin_z);
    if (self.lefty > self.owner.height) { 
        trigger_remove_bubble(); 
    }
    else {
        // Wobble on the X / Y axis as the bubble raises up
        self.velocity_x = self.velocity_y = 0;
        if (random() < 0.5) self.velocity_x = self.velocity_x + crandom()*4;
        else self.velocity_y = self.velocity_y + crandom()*4;
        // Make the bubble raise faster by random amounts
        self.velocity_z = self.velocity_z + random();
        // Keep updating bubble, 15 updates is the limit
        self.nextthink = time + 0.5 + random()*0.5;
        if (self.count < 15) self.count = self.count + 1;
        else trigger_remove_bubble();
    }
};

//----------------------------------------------------------------------
void() trigger_spawn_bubbles =
{
    local entity bubble;
    
    if (self.estate & ESTATE_BLOCK) return;
    if ( !(self.spawnflags & TRIG_SPAWNBUBBLES) ) return;

    if (self.bubble_count < self.count) {
        self.bubble_count = self.bubble_count + 1;

        bubble = spawn();
        bubble.owner = self;
        bubble.classname = "misc_bubble";
        bubble.classtype = CT_BUBBLE;
        bubble.classgroup = CG_TEMPENT;
        bubble.movetype = MOVETYPE_NOCLIP;
        bubble.solid = SOLID_TRIGGER;
        setmodel(bubble, self.mdl);
        bubble.frame = rint(random() * 3);        // Light/dark colours
        setsize (bubble, VEC_ORIGIN, VEC_ORIGIN);
        bubble.origin_x = self.oldorigin_x + crandom()*self.t_width;
        bubble.origin_y = self.oldorigin_y + crandom()*self.t_length;
        bubble.origin_z = self.oldorigin_z;
        setorigin(bubble, bubble.origin);
        bubble.velocity = vecrand(0,5,TRUE);
        bubble.velocity_z = 10 + random()*15;
        bubble.count = rint(random()*4);

        bubble.nextthink = time + 0.5 + random()*0.5;
        bubble.think = trigger_update_bubble;
        bubble.touch = trigger_remove_bubble;
    }

    // Keep spawning bubbles until told not too!
    self.think = trigger_spawn_bubbles;
    self.nextthink = time + self.yaw_speed + random()*self.yaw_speed;
};

/*======================================================================
/*QUAKED trigger_multiple (0.5 0 0.5) ? NOTOUCH x DEVMODE x MODCHECK MONSTERS STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Variable sized bmodel that uses multiple times
------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : trigger all these targets
upgrade_ssg : = 1 will only trigger if shotgun upgrade active on server
upgrade_axe : = 1 will only trigger if axe upgrade active on server
upgrade_lg  : = 1 will only trigger if lightning gun upgrade active on server
health  : Can be damaged instead of touched
wait    : time between re-triggering
delay   : delay before firing (after being triggered)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
sounds  : 1=Secret,2=talk(def),3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
message : message to display when triggered
-------- SPAWNFLAGS --------
NOTOUCH : can only be triggered via other entities
DEVMODE  : Will only trigger if developer mode active
MODCHECK : Will remove this entity if THIS mod is active
MONSTER  : can be touched/triggered by monsters
STARTOFF : Requires trigger to activate
------- NOTES --------
Variable sized bmodel that uses multiple times

======================================================================*/
void() trigger_multiple =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    if (self.spawnflags & TRIG_MODCHECK) { remove(self); return; }
    trigger_bmodel_sounds();            // Precache any sounds
    self.classtype = CT_TRIGMULTI;
    InitTrigger ();
    if (!self.wait) self.wait = 0.2;
    
    // Setup Entity State functionality
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_once (0.5 0 0.5) ? NOTOUCH x DEVMODE INVIEW MODCHECK MONSTER STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Variable sized bmodel that uses once
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : trigger all these targets
upgrade_ssg : = 1 will only trigger if shotgun upgrade active on server
upgrade_axe : = 1 will only trigger if axe upgrade active on server
upgrade_lg  : = 1 will only trigger if lightning gun upgrade active on server
health  : Can be damaged instead of touched
wait    : Always -1
delay : delay before firing (after being triggered)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
sounds  : 1=Secret,2=talk(def),3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
message : message to display when triggered
t_length : Inview distance (less than) to activate trigger
-------- SPAWNFLAGS --------
NOTOUCH  : can only be triggered via other entities
DEVMODE  : Will only trigger if developer mode active
INVIEW   : Player has to be infront and look at trigger (>30 & <60)
MODCHECK : Will remove this entity if THIS mod is active
MONSTER  : can be touched/triggered by monsters
STARTOFF : Requires trigger to activate
-------- NOTES --------
Variable sized bmodel that uses once

======================================================================*/
void() trigger_once =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    if (self.spawnflags & TRIG_MODCHECK) { remove(self); return; }
    trigger_bmodel_sounds();            // Precache any sounds
    self.classtype = CT_TRIGONCE;
    InitTrigger ();
    self.wait = -1;

    // Inview triggers need to start delayed, high cpu functions
    if (self.spawnflags & TRIG_INVIEW) {
        self.spawnflags = self.spawnflags | ENT_STARTOFF;
        // Cannot have inview triggers touched by monsters
        self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);
    }

    // Setup Entity State functionality
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_secret (.5 0 .5) ? NOTOUCH NOMSG x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Update map secret counter
------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : name of target(s) to trigger
upgrade_ssg : = 1 will only trigger if shotgun upgrade active on server
upgrade_axe : = 1 will only trigger if axe upgrade active on server
upgrade_lg  : = 1 will only trigger if lightning gun upgrade active on server
health  : Can be damaged instead of touched
wait    : Always -1
angle   : Facing Direction for trigger to work, use 360 for angle 0.
sounds  : 1=Secret(def),2=talk,3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
message : message to display when triggered
-------- SPAWNFLAGS --------
NOTOUCH : can only be triggered via other entities
NOMSG   : Remove/Block any trigger secret message
STARTOFF : Requires trigger to activate
------- NOTES --------
Update map secret counter

======================================================================*/
void() trigger_secret =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    if (self.sounds == 0) self.sounds = 1;
    trigger_bmodel_sounds();            // Precache any sounds
    self.classtype = CT_TRIGSECRET;
    InitTrigger ();
    self.wait = -1;    // Trigger ONCE
    self.count = 1;    // Add 1 secret

    total_secrets = total_secrets + self.count;
    if (!self.message) self.message = "You found a secret area!";
    if (self.spawnflags & TRIG_SECRETNOMSG) self.message = "";

    // Cannot have secret triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);

    // Setup Entity State functionality
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_teleport (0.5 0 0.5) ? PLAYER_ONLY SILENT STARTON x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Teleport player/monsters to target location
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target : Points to info_teleport_destination entity
wait   : -1 = trigger once condition (def=0)
speed  : forward momentum speed after teleporting (def=300)
volume : teleporter hum sound volume (def=0.5)
noise  : custom sound to play when active (must be looped, def=hum1.wav)
waitmin : the length of the custom sound (def=3.622 for hum1.wav)
-------- SPAWNFLAGS --------
PLAYER_ONLY : Can only be used by players (nothing else)
SILENT      : No teleporter hum sound regardless of state
STARTON     : Will start active regardless of targetname setting
STARTOFF    : Starts off and waits for trigger
-------- NOTES --------
Teleport player/monsters to target location
if targetname is setup, the teleporter requires a trigger to activate
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_teleport_sound =
{
    // Has the original bmodel trigger been deleted!?!
    if (!self.owner) {
        sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
        return;
    }

    // The Quake sound system is really basic and dumb! :)
    // Originally the sound of the teleporter was setup as an ambientsound,
    // which always play, loop and never can be turned off!
    //
    // Normal sounds are ONLY active when the player is in the same portal!
    // This is probably an optimization (engine) thing to save time 
    // constantly checking attenuation levels
    //
    // The teleporter sound is manually looped because of portal problems
    // and that the trigger can be turned on / off.
    // The waitmin parameter has to match the length of the sound, otherwise
    // the looping will literally sound odd
    //

    // Keep checking for any updates
    self.nextthink = time + 0.1;
    self.think = trigger_teleport_sound;

    // Wait for trigger to start before doing anything
    if (self.owner.spawnflags & ENT_STARTOFF) return;
    
    // Has the trigger been switched off recently?
    if (self.owner.estate != self.estate) {
        // Check for OFF and disabled together
        if (self.owner.estate & ESTATE_BLOCK) {
            self.estate = self.owner.estate;
            sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
            self.fly_sound = LARGE_TIMER;
            return;
        }
        else {
            // trigger has been switched on, reset sound
            self.estate = self.owner.estate;
            self.fly_sound = 0;
        }
    }
    
    // Play the sound if loop has finished or been reset
    if (self.fly_sound < time ) {
        self.fly_sound = time + self.owner.waitmin;
        sound (self, CHAN_VOICE, self.owner.noise, self.owner.volume, ATTN_STATIC);
    }
};

//----------------------------------------------------------------------
// This is used for the closet monster setup where the trigger is fired
// and then anything touching the trigger 0.2s later is teleported
// This forces all touch triggers in a map to check what they are touching!
//----------------------------------------------------------------------
void() trigger_teleport_use =
{
    // Deal with START OFF functionality first
    if (self.spawnflags & ENT_STARTOFF) {
        // Remove start off flag and switch on entity
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
        self.estate_on();
        return;
    }
    // Block entity state DISABLE
    if (self.estate & ESTATE_DISABLE) return;
    if (self.estate & ESTATE_OFF) self.estate_on();

    // Force retouch is really hard on the engine because EVERY trigger
    // in the map is re-checked for any touching objects
    force_retouch = 2;
    self.nextthink = time + 0.2;
    self.think = SUB_Null;
};

//----------------------------------------------------------------------
void() trigger_teleport_on =
{
    self.estate = ESTATE_ON;
    self.state = STATE_ON;
};

//----------------------------------------------------------------------
void() trigger_teleport_off =
{
    self.estate = ESTATE_OFF;
    self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() trigger_teleport_touch =
{
    if (self.estate & ESTATE_BLOCK) return;        // Entity off/disabled?
    if (self.spawnflags & ENT_STARTOFF) return;    // Starts off?
    if (self.attack_finished > time) return;    // Trigger once?
    if (self.state == STATE_OFF) return;        // Waiting for trigger?

    // Is the teleporter designed for players only?
    if (self.spawnflags & TRIG_TELEPLAYER && !(other.flags & FL_CLIENT)) return;
    // only teleport living creatures and monsters (mostly)
    if (other.health < 1 || other.solid != SOLID_SLIDEBOX) return;
    
    // Find teleporter target
    if (!self.goalentity) {
        self.goalentity = find (world, targetname, self.target);
        if (!self.goalentity) {
            dprint("\b[TELEPORT_TOUCH]\b Cannot find target\n");
            return;
        }
        // New feature, fire targets on teleporter destination
        if (self.goalentity.classtype == CT_MISCTELEPORT && self.goalentity.target != "") {
            trigger_strs(self.goalentity.target, other);    // Fire all targets
            self.goalentity.target = "";                    // only work once
        }
        // This stuff never changes, might as well generate it now
        makevectors (self.goalentity.mangle);
        self.goalentity.movedir = v_forward;
        self.goalentity.pos1 = self.goalentity.origin + 32 * self.goalentity.movedir;
    }
        
    // Check for a trigger_once condition
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }

    // Fire all targets on trigger teleporter entity (ID code)
    // not sure why? the target *should* be pointing at info_destination
    // Could use killtarget or message strings
    SUB_UseTargets ();

    // put a tfog where the player was
    spawn_tfog (other.origin);

    // spawn a tfog flash in front of the destination
    spawn_tfog (self.goalentity.pos1);
    spawn_tdeath(self.goalentity.origin, other);

    // Add a little forward momentum to teleporting entity
    // This is usually used for gib/telefrag effects
    if (!other.health) {
        other.origin = self.goalentity.origin;
        other.velocity = (self.goalentity.movedir * other.velocity_x) + (self.goalentity.movedir * other.velocity_y);
        return;
    }

    // Move/rotate teleporting entity to new location
    setorigin (other, self.goalentity.origin);
    other.angles = self.goalentity.mangle;
    
    // If player, rotate (immediately) and push forward
    if (other.flags & FL_CLIENT) {
        other.fixangle = 1;                    // turn this way immediately
        other.teleport_time = time + 0.7;    // Special state for engine
        other.velocity = self.goalentity.movedir * self.speed;
    }
    // Telporting entities need to check for ground
    other.flags = other.flags - (other.flags & FL_ONGROUND);
};

//----------------------------------------------------------------------
void() trigger_teleport =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Always pre-cache telporter teleporter sound
    if (self.noise == "") self.noise = "ambience/hum1.wav";
    precache_sound (self.noise);
    // The length of the custom sound cannot be < 0.1s
    if (self.waitmin <= 0.1) self.waitmin = 3.622;

    self.classtype = CT_TRIGTELEPORT;
    InitTrigger ();

    // A trigger teleporter going nowhere!?!
    // Need to initialize trigger to get mins/maxs for origin
    if (self.target == "") {
        dprint("\b[TRIGGER_TELEPORT]\b Missing target\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        remove(self);
        return;
    }

    // Spawnflag 1 is used for something else (notouch duplicate)
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    if (!self.volume) self.volume = 0.5;
    if (!self.speed) self.speed = 300;
    // Save this for later (sound emitter)
    self.oldorigin = bmodel_origin(self);

    // Trigger teleport is a special case in the way it is setup because
    // is it abused for monster closets, where the trigger is always active
    self.touch = trigger_teleport_touch;
    self.solid = SOLID_TRIGGER;
    setsize (self, self.bbmins, self.bbmaxs);
    
    // Setup Entity State functionality
    self.estate_on = trigger_teleport_on;
    self.estate_off = trigger_teleport_off;
    self.estate_use = trigger_teleport_use;
    self.use = entity_state_use;
    
    // Damn annoying that the targetname is being used like this because
    // there could have been a better way to do this type of functionality
    // == "" teleporter works fine (starts on)
    // != "" teleporter requires trigger activation
    if (self.spawnflags & TRIG_TELEALWAYSON) self.estate = ESTATE_ON;
    else if (self.spawnflags & ENT_STARTOFF) self.estate = ESTATE_OFF;
    else if (self.targetname != "") self.estate = ESTATE_OFF;
    else self.estate = ESTATE_ON;
    
    // Setup sound emitter for teleporter state
    if ( !(self.spawnflags & TRIG_TELESILENT) ) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.estate = -1;
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
        setorigin(self.attachment, self.oldorigin);
        //setmodel(self.attachment, MODEL_BROKEN);
        self.attachment.nextthink = time + 1;
        self.attachment.think = trigger_teleport_sound;
    }
};

/*======================================================================
/*QUAKED trigger_changelevel (0.5 0 0.5) ? NO_INTERMIS RESETINV x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Finish current map, show intermission screen and loads next
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target     : name of target(s) to trigger before intermission
map        : The name of next map (e.g. e1m1) default=same map
startspawn2: Special unique number (1-7) which must match info_player_start2
-------- SPAWNFLAGS --------
NO_INTERMIS : No Intermission screen 
RESETINV    : Reset player inventory to default (Shotgun+Shells)
STARTOFF    : Starts off and waits for trigger
-------- NOTES --------
Finish current map, show intermission screen and loads next
This entity cannot be damaged and is always touchable once activated

 ======================================================================*/
void() trigger_changelevel_finish =
{
    intermission_running = 1;
    
    // enforce a wait time before allowing changelevel
    if (deathmatch) intermission_exittime = time + 5;
    else intermission_exittime = time + 2;

    WriteByte (MSG_ALL, SVC_CDTRACK);        // Update CD track
    WriteByte (MSG_ALL, SVC_UPDATESTAT);    // Update stats, total kills etc
    WriteByte (MSG_ALL, SVC_UPDATESTAT);    // Update twice?
    StartIntermissionCamera();                // Setup intermission camera(s)
    WriteByte (MSG_ALL, SVC_INTERMISSION);    // Start intermission (lock movement)
};

//----------------------------------------------------------------------
void() trigger_changelevel_fire =
{
    if (self.attack_finished > time) return;
    if ( !(self.bmodel_act.flags & FL_CLIENT) ) return;
    if ( self.bmodel_act.health < 1 ) return;

    self.attack_finished = LARGE_TIMER;

    // Check for map variables on exit
    if (!CheckZeroVector(self.mapvar_update))
        mapvar_range(self.mapvar_update);
    
    // Reset player inventory back to ID default (shotgun+25shells)
    if (self.spawnflags & TRIG_CMAPRESETINV) {
        dprint("\b[CHANGELVL]\b Resetting client inventory!\n"); 
        self.bmodel_act.health = HEAL_PLAYMAX;
        self.bmodel_act.armortype = self.bmodel_act.armorvalue = 0;
        self.bmodel_act.ammo_shells = DEF_SHELLS;
        self.bmodel_act.ammo_nails = self.bmodel_act.ammo_rockets = self.bmodel_act.ammo_cells = 0;
        self.bmodel_act.items = IT_SHOTGUN | IT_AXE;
        self.bmodel_act.moditems = 0;
        self.bmodel_act.weapon = IT_SHOTGUN;
    }
    
    // Some crazy DM parameters to prevent teleport exit
    if ((cvar("noexit") == 1) || ((cvar("noexit") == 2) && (mapname != "start"))) {
        T_Damage (self.bmodel_act, self, self, MEGADEATH, NOARMOR); return;
    }
    // More crazy DM stuff
    if (coop || deathmatch) {
        bprint (self.bmodel_act.netname);
        bprint (" exited the level\n");
    }

    // is there a special info_player_start2 location setup?
    // This will also reset any values back to zero (default)
    if (self.startspawn2 < 1 && self.startspawn2 > 7) self.startspawn2 = 0;
    update_configflag(SVR_SPAWN_BIT1, floor( (self.startspawn2 & 1) / 1) );
    update_configflag(SVR_SPAWN_BIT2, floor( (self.startspawn2 & 2) / 2) );
    update_configflag(SVR_SPAWN_BIT3, floor( (self.startspawn2 & 4) / 4) );
        
    // *change* if map key not defined, reload current map again
    if (self.map) nextmap = self.map;
    SUB_UseTargets ();

    // If no intermission, go straight to next map
    if ( (self.spawnflags & TRIG_CMAPNOPAUSE) && (deathmatch == 0) ) {
        GotoNextMap();
        return;
    }
    
    // we can't move people right now, because touch functions are called
    // in the middle of C movement code, so set a think time to do it
    self.think = trigger_changelevel_finish;
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
void() trigger_changelevel_setup =
{
    // If map not defined, use current mapname instead
    if (!self.map) self.map = mapname;
};

//----------------------------------------------------------------------
// Re-direction for map hacks (not used normally)
//----------------------------------------------------------------------
void() changelevel_touch = { trigger_changelevel_fire();};

//----------------------------------------------------------------------
void() trigger_changelevel =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGLEVEL;
    InitTrigger ();
    // Spawnflag 1 is used for something else (notouch duplicate)
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    // Cannot have change level triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_fire = trigger_changelevel_fire;
    trigger_bmodel_setup();
    
    // Wait for everything to spawn before checking map name
    self.think = trigger_changelevel_setup;
    self.nextthink = time + 1;
};

/*======================================================================
/*QUAKED trigger_setskill (0.5 0 0.5) ? NOTOUCH x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Sets player skill level (via console)
-------- KEYS --------
targetname : trigger entity (works with entity state system)
health  : Can be damaged instead of touched
wait    : time between re-triggering (def=0.2s, -1=once)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
message : Skill Level - 0 = easy, 1 = normal, 2 = hard, 3 = nightmare
-------- SPAWNFLAGS --------
NOTOUCH  : can only be triggered via other entities
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Sets player skill level (via console)

======================================================================*/
void() trigger_setskill_fire =
{
    if (self.attack_finished > time) return;

    // There is no client check for the skill level change so that
    // the use functionality can work from trigger chains
    cvar_set ("skill", self.message);

    // Is the trigger repeatable?
    if (self.wait > 0) {
        self.attack_finished = time + self.wait;
        self.nextthink = self.attack_finished;
        self.think = self.estate_on;
    }
    // block trigger and turn off (trigger_once)
    else {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_setskill =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGSKILLS;
    InitTrigger ();
    
    if (!self.wait) self.wait = 1;
    if (self.message == "") self.message = "0";

    // Cannot have skill triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);

    // Setup Entity State functionality
    self.estate_fire = trigger_setskill_fire;
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_rune (0.5 0 0.5) ? E1 E2 E3 E4 x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A Trigger that fires once if the player has certain runes
-------- KEYS --------
targetname : trigger entity (works with entity state system)
health  : Can be damaged instead of touched
angle   : Facing Direction for trigger to work, use "360" for angle 0.
target  : trigger to fire if player has MIXTURE of runes
noise1  : trigger to fire if player has rune 1
noise2  : trigger to fire if player has rune 2
noise3  : trigger to fire if player has rune 3
noise4  : trigger to fire if player has rune 4
wait    : = -1 Only trigger once if the player has runes
-------- SPAWNFLAGS --------
E1 : Episode 1
E2 : Episode 2
E3 : Episode 3
E4 : Episode 4
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
There are two ways this trigger can be used, a single check for multiple
runes using target key OR individual triggers for runes using noise 1-4 keys
This trigger is designed to work once when rune conditions are met

======================================================================*/
void() trigger_rune_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    // Is the trigger blocked? (trigger_once)
    if (self.attack_finished > time) return;
    // Stop the trigger constantly firing
    self.attack_finished = time + self.waitmin;

    // Check for single rune trigger
    if (self.target) {
        if (query_configflag(SVR_RUNE_ALL) & self.customkey == self.customkey) {
            trigger_strs(self.target, self.bmodel_act);
            if (self.wait < 0) self.attack_finished = LARGE_TIMER;
        }
    }
    else {
        // Check for multiple rune triggers
        if (query_configflag(self.customkey & SVR_RUNE_KEY1) == SVR_RUNE_KEY1 && self.noise1 != "") {
            trigger_strs(self.noise1, self.bmodel_act);
            if (self.wait < 0) self.attack_finished = LARGE_TIMER;
        }
        if (query_configflag(self.customkey & SVR_RUNE_KEY2) == SVR_RUNE_KEY2 && self.noise2 != "") {
            trigger_strs(self.noise2, self.bmodel_act);
            if (self.wait < 0) self.attack_finished = LARGE_TIMER;
        }
        if (query_configflag(self.customkey & SVR_RUNE_KEY3) == SVR_RUNE_KEY3 && self.noise3 != "") {
            trigger_strs(self.noise3, self.bmodel_act);
            if (self.wait < 0) self.attack_finished = LARGE_TIMER;
        }
        if (query_configflag(self.customkey & SVR_RUNE_KEY4) == SVR_RUNE_KEY4 && self.noise4 != "") {
            trigger_strs(self.noise4, self.bmodel_act);
            if (self.wait < 0) self.attack_finished = LARGE_TIMER;
        }
    }

    // The attack_finished is set by the RUNE condition being TRUE
    // The trigger is designed to be trigger_once (no need to check wait)
    // The trigger needs to meet a rune condition before switching off
    if (self.attack_finished == LARGE_TIMER) self.estate_off();
};

//----------------------------------------------------------------------
void() trigger_rune =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGRUNES;
    // Spawnflag 1 is used for something else (notouch duplicate)
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH;
    InitTrigger ();
    if (self.waitmin <=0) self.waitmin = 1;

    // Cannot have rune triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);
    // Calculate the rune key selection
    self.customkey = self.spawnflags & SVR_RUNE_ALL;
    
    // Cannot do anything with the trigger if no runes are selected
    if (self.customkey == 0) {
        dprint("\b[TRIG_RUNE]\b No runes setup!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        remove(self);
        return;        
    }

    // Setup Entity State functionality
    self.estate_fire = trigger_rune_fire;
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_push (0.5 0 0.5) ? PUSH_ONCE SILENT NOMONSTER x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Push the Player and Grenades!
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target : target entity for custom direction
angle  : direction of push (-2 is down, -1 up)
angles : Pitch Yaw Roll (up/down, angle, tilt left/right)
speed  : Speed of push direction (def=1000)
-------- SPAWNFLAGS --------
PUSH_ONCE : trigger_once functionality
SILENT    : No wind sound for player
NOMONSTER : Monsters cannot be pushed by this trigger
STARTOFF  : Starts off and waits for trigger
-------- NOTES --------
Push the Player, player/ogre grenades and minion eggs!
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_push_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    // Check for any dead monster bodies (no exceptions)
    if (trigger_check_body(self.bmodel_act,DEAD_EXPLODE)) return;
    // Check for no monster exception
    if (self.spawnflags & TRIG_PUSHNOMONSTER && self.bmodel_act.flags & FL_MONSTER) return;

    // Cool feature is that it will push grenades!
    // Added player, ogre and minion eggs to the catch
    if (self.bmodel_act.classtype == CT_PROJ_GL || 
        self.bmodel_act.classtype == CT_PROJ_GLMON ||
        self.bmodel_act.classtype == CT_PROJ_MEGG)
        self.bmodel_act.velocity = self.speed * self.movedir * 10;
    // Standard push for all bmodel_act living entities
    else if (self.bmodel_act.health > 0) {
        self.bmodel_act.velocity = self.speed * self.movedir * 10;
        // Play wind sound for the player
        if (self.bmodel_act.classtype == CT_PLAYER) {
            if (self.bmodel_act.fly_sound < time) {
                self.bmodel_act.fly_sound = time + 1.5;
                sound (self.bmodel_act, CHAN_AUTO, self.noise, 1, ATTN_NORM);
            }
        }
    }
    
    // Setup to trigger push once?
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
// Map hack reference
void() trigger_push_touch = { self.bmodel_act = other; trigger_push_fire(); }

//----------------------------------------------------------------------
void() trigger_push =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGPUSH;
    InitTrigger ();
    // Spawnflag 1 is used for something else (notouch duplicate)
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    if (!self.speed) self.speed = 1000;

    // Setup default wind sound if no custom sound found
    // else if silent trigger, use misc/empty sound
    if ( !(self.spawnflags & TRIG_PUSHSILENT) && self.noise == "")
        self.noise = "ambience/windfly.wav";
    if (self.noise == "") self.noise = SOUND_EMPTY;
    precache_sound (self.noise);
    
    if (self.spawnflags & TRIG_PUSHONCE) self.wait = -1;

    // Setup Entity State functionality
    self.estate_fire = trigger_push_fire;
    self.touch = trigger_bmodel_anytouch;
    trigger_bmodel_setup();

    // If target is setup, calculate new facing angle
    if (self.target != "") {
        self.nextthink = time + 2;
        self.think = TargetMovedir;
    }
};

//----------------------------------------------------------------------
// Removed - No map used the feature
//----------------------------------------------------------------------
void() trigger_conveyor = { remove(self); };

/*======================================================================
 Player ladder (originally from Rubicon2 codebase by JohnFitz)
 - This is a very simple system, jump to attach to the ladder brush
 - move up down via jumpping (hook in preplayer code)
 - Added multiple climbing sounds (works with player footsound state)
 - Modified to have on/off/toggle state via triggers
 - Downsides to system, there is no abilty to go down a ladder

/*======================================================================
/*QUAKED trigger_ladder (.5 .5 .5) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Invisible brush based ladder (jump key to climb)
-------- KEYS --------
targetname : trigger entity (works with entity state system)
angle    : direction player must be facing to climb ladder (required)
waitmin  : time between climb sound (def = depends on sound type)
speed    : velocity speed to climb ladder (def=160)
sounds   : 1=metal, 2=wood, 3=rope, 4=silent, 5=custom (def=wood)
noise1-4 : custom sounds to play when climbing ladder
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Invisible brush based ladder (jump key to climb)
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_ladder_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'

    // Ladders ONLY work with players because of client.qc (PlayerPreThink)
    if (self.estate & ESTATE_BLOCK) return;
    if ( !(self.bmodel_act.flags & FL_CLIENT) ) return;
    if ( self.bmodel_act.health < 1 ) return;

    // Don't stick underwater, or in the middle of a waterjump
    if (self.bmodel_act.waterlevel > 1) return;
    if (self.bmodel_act.flags & FL_WATERJUMP) return;

    self.bmodel_act.onladder = 1;        // Add everytime the player touches volume
    self.bmodel_act.entladder = self;    // Link back to play sounds
};

//----------------------------------------------------------------------
void() trigger_ladder =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Default = wood
    if (!self.sounds) self.sounds = 2;
    if (self.sounds == 1) {                // Metal
        if(!self.waitmin) self.waitmin = 0.45;
        self.noise1 = "player/ladmetal1.wav";
        self.noise2 = "player/ladmetal2.wav";
        self.noise3 = "player/ladmetal3.wav";
        self.noise4 = "player/ladmetal4.wav";
    }
    else if (self.sounds == 2) {        // Wood
        if(!self.waitmin) self.waitmin = 0.4;
        self.noise1 = "player/ladwood1.wav";
        self.noise2 = "player/ladwood2.wav";
        self.noise3 = "player/ladwood3.wav";
        self.noise4 = "player/ladwood4.wav";
    }
    else if (self.sounds == 3) {        // Old Rope
        if(!self.waitmin) self.waitmin = 0.7;
        self.noise1 = "player/ladrope1.wav";
        self.noise2 = "player/ladrope2.wav";
        self.noise3 = "player/ladrope3.wav";
        self.noise4 = "player/ladrope4.wav";
    }
    else {
        // Custom or empty
        if (!self.waitmin) self.waitmin = 0.5;
        if (self.noise1 == "") self.noise1 = SOUND_EMPTY;
        if (self.noise2 == "") self.noise2 = SOUND_EMPTY;
        if (self.noise3 == "") self.noise3 = SOUND_EMPTY;
        if (self.noise4 == "") self.noise4 = SOUND_EMPTY;
    }
    precache_sound(self.noise1);
    precache_sound(self.noise2);
    precache_sound(self.noise3);
    precache_sound(self.noise4);
    
    self.classtype = CT_TRIGLADDER;
    InitTrigger ();
    if (!self.speed) self.speed = 160;
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;

    // Cannot have ladder triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);

    // Setup Entity State functionality
    self.estate_fire = trigger_ladder_fire;
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_giveitems (0.5 0 0.5) ? NOTOUCH x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Variable sized bmodel used to give items to player
The target items only work if delay spawned spawnflag is set
The pickup sound can be turned off by adding sounds=4 to item
The target items will not respawn or work more than once
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : Points to all items to give to the player
wait    : (def) -1 = will only fire targets once
angle   : Facing Direction for trigger to work, use "360" for angle 0.
sounds  : 1=Secret,2=talk(def),3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
message : message to display when triggered
-------- SPAWNFLAGS --------
NOTOUCH  : can only be triggered via other entities
STARTOFF : Requires trigger to activate
-------- NOTES --------
Variable sized bmodel used to give items to activator
The target items only work if delay spawned spawnflag is set
The pickup sound can be turned off by adding sounds=4 to item
The target items will not respawn or work more than once

======================================================================*/
void() trigger_giveitem_fire =
{
    local entity stemp;

    // Is the trigger blocked? (trigger_once)
    if (self.attack_finished > time) return;
    // Activator can only be the player
    if (!(self.bmodel_act.flags & FL_CLIENT)) return;
    if (self.bmodel_act.health < 1) return;
    if (self.target == "") return;

    // Save for later and reset activator
    stemp = self; activator = self.bmodel_act;

    // Play the sound ON the activator and display message
    if (self.noise != "") sound (activator, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    if (self.message != "") centerprint (activator, self.message);
    
    // Search entity list for targets
    self.enemy = find (world, targetname, self.target);
    
    while(self.enemy) {
        // This only works with items
        if (self.enemy.flags & FL_ITEM) {
            // Only works with items that start off
            if (self.enemy.spawnflags & ENT_STARTOFF) {
                if (self.enemy.touch2 != SUB_Null) {
                    // Switch to item for touch function
                    other = self.bmodel_act;
                    activator = self.bmodel_act;
                    self = self.enemy; 
                    // make sure items never respawn and silent pickup
                    self.spawnflags = self.spawnflags - (self.spawnflags & ITEM_RESPAWN);
                    self.respawn_time = -1;
                    self.respawn_trig = FALSE;
                    // item should always be floating (no drop to floor functions)
                    self.spawnflags = self.spawnflags | ITEM_FLOATING;
                    // give trigger sound overrides all items
                    if (stemp.sounds != 4) self.noise = SOUND_EMPTY;
                    // Use original touch function
                    self.touch2 ();
                    self = stemp;
                    
                }
            }
        }
        // Are there anymore targets left in the list?
        self.enemy = find (self.enemy, targetname, self.target);
    }
    
    // always TRIGGER_ONCE functionality
    self.attack_finished = LARGE_TIMER;
};

//----------------------------------------------------------------------
void() trigger_giveitems =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    if (self.sounds == 0) self.sounds = 4;    // Default = silence
    trigger_bmodel_sounds();                // Precache any sounds
    self.classtype = CT_TRIGGIVEITEM;
    InitTrigger ();
    
    // Always trigger once functionality
    self.wait = -1;
    
    // Cannot have trigger give items touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);

    // Setup Entity State functionality
    self.estate_fire = trigger_giveitem_fire;
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_fog (.5 .5 .5) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Trigger change global fog to new value over time
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : name of target(s) to trigger
speed   : time (secs) to fade from current to new (-1 = instant, 2s = default)
wait    : time between re-triggering (def=2s, -1=once)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
fog_density : new fog density (def=0.5, -1=debug mode)
fog_colour  : new fog colours (def=0.1 0.1 0.1)
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Trigger change global fog to new value over time

======================================================================*/
void() trigger_fog_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;

    if (query_configflag(SVR_NOFOGCMDS)) {
        dprint("\b[FOG]\b quake.rc disabled, removing!\n");
        self.attack_finished = LARGE_TIMER;
        entity_remove(self, 0.1);
        return;
    }
    
    // Print dev message if worldspawn not setup correctly
    // First fog blend will be weird otherwise
    if (!fog_active) {
        if (self.pain_finished < time) {
            dprint("\b[FOG]\b Missing density + colour on worldspawn!\n");
            dprint("\b[FOG]\b First fog blend requires worldspawn setup\n");
        }
        self.pain_finished = time + 1;
    }    
    
    // Crazy test option, random fog!
    if (self.lefty) {
        self.fog_density = random();
        self.fog_colour_x = random();
        self.fog_colour_y = random();
        self.fog_colour_z = random();
        self.speed = 1 + random()*4;
    }
    
    // Update global fog controller
    fade_fog(self.fog_density, self.fog_colour, self.speed);
    SUB_UseTargets();
    
    // Is the trigger repeatable?
    if (self.wait > 0) {
        if (self.wait < self.speed) self.wait = self.speed + 0.1;
        self.attack_finished = time + self.wait;
        self.nextthink = self.attack_finished;
        self.think = self.estate_on;
    }
    // block trigger and turn off (trigger_once)
    else {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_fog_waitforcontrol =
{
    if (fog_control && fog_active) {
        // Setup Entity State functionality
        self.estate_fire = trigger_fog_fire;
        trigger_bmodel_setup();
    }
    else {
        self.think = trigger_fog_waitforcontrol;
        self.nextthink = time + 0.1 + random();
    }
};

//----------------------------------------------------------------------
void() trigger_fog =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGFOG;
    InitTrigger ();

    // A fade time < minimum fade time = instant change
    if (self.speed == 0) self.speed = 2;
    if (self.speed < FADEFOG_TIME) self.speed = FADEFOG_TIME;
    if (self.wait == 0) self.wait = 2;
    self.attack_finished = 0;
    
    // Cannot have fog triggers touched by monsters
    self.spawnflags = self.spawnflags - (self.spawnflags & TRIG_MONSTERS);

    // Check for debug test mode (random density/colour)
    if (self.fog_density < 0) self.lefty = TRUE;
    // Default density/colour
    if (!self.fog_density) self.fog_density = 0.1;
    if (CheckZeroVector(self.fog_colour)) self.fog_colour = '0.1 0.1 0.1';

    trigger_fog_waitforcontrol();
};

/*======================================================================
/*QUAKED trigger_monsternojump (.5 .5 .5) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Stop monsters from using jump attack
-------- KEYS --------
targetname : trigger entity (works with entity state system)
wait   : -1 = trigger_once functionality
delay  : time to delay jump attack by (def=0.5s)
waitmin: Re-trigger timer to stop touch flooding (def=0.1s)
noise1 : specify classname that CAN use this trigger (noise1=monster_dog)
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Stop monsters from using jump attack

======================================================================*/
void() trigger_monsternojump_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    
    // Block non-monsters
    if (!(self.bmodel_act.flags & FL_MONSTER)) return;

    // Is there any classname exception setup?
    if (self.noise1 != "") {
        if (self.bmodel_act.classname != self.noise1) return;
    }
    
    // Check for any extra triggers, fire them once!
    if (self.target != "") {
        trigger_strs(self.target, activator);
        self.target = "";
    }
    
    // Update jump flag to block jump attacks
    self.bmodel_act.jump_flag = time + self.delay;
    // Restrict the trigger to 0.1s re-triggering
    self.attack_finished = time + self.waitmin;

    // Setup to trigger push once?
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_monsternojump =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGNOJUMP;
    if (!self.delay) self.delay = 0.5;
    if (!self.waitmin) self.waitmin = 0.1;
    // Work out dimensions of trigger
    InitTrigger ();
        
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    
    // Setup Entity State functionality
    self.estate_fire = trigger_monsternojump_fire;
    self.touch = trigger_bmodel_anytouch;
    trigger_bmodel_setup();

};

//----------------------------------------------------------------------
// Re-direct because it was renamed to be more consistent
void() trigger_nomonjump = { trigger_monsternojump(); };

/*======================================================================
/*QUAKED trigger_monsterdrop (0 .5 .5) (-8 -8 -8) (8 8 8) x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Drop monster(s) to floor
-------- KEYS --------
targetname : trigger entity (works with entity state system)
wait   : -1 = trigger_once functionality
height : the speed thrown upwards (def 50)
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Drop monster(s) to floor
======================================================================*/

void() trigger_monsterdrop_use =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    if (self.target == "") return;
    
    // Build initial list from world
    self.oldenemy = find(world, targetname, self.target);
    while (self.oldenemy) {
        // only works with monsters
        if (self.oldenemy.flags & FL_MONSTER) {
            // Double check for swim/fly types
            if (!(self.oldenemy.flags & (FL_FLY || FL_SWIM))) {
                // Got to be onground already
                if (self.oldenemy.flags & FL_ONGROUND) {
                    // Set monster in motion
                    self.oldenemy.flags = self.oldenemy.flags - FL_ONGROUND;
                    self.oldenemy.velocity_z = self.height;
                }
            }
        }
        // Find next monster in list
        self.oldenemy = find(self.oldenemy, targetname, self.target);
    }
    // Setup to trigger once functionality?
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_monsterdrop =
{
    self.classtype = CT_TRIGMONDROP;
    if (!self.height) self.height = 50;
        
    // Check for firing conditions (nightmare, coop)
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_monsterdrop_use;
    self.estate = ESTATE_ON;
};

/*======================================================================
/*QUAKED trigger_monsterjump (.5 .5 .5) ? x FLYING SWIMMING x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Push monsters in a certain direction
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target : target entity for custom direction
wait   : -1 = trigger_once functionality
angle  : direction of push (-2 is down, -1 up)
speed  : the speed thrown forward (def 200)
height : the speed thrown upwards (def 200)
noise1 : specify classname that CAN use this trigger (noise1=monster_dog)
-------- SPAWNFLAGS --------
FLYING   : Will affect flying monsters
SWIMMING  : Will affect swimming monsters
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Push monsters in a certain direction
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_monsterjump_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;
    
    // Cannot move spawning statues
    if (self.bmodel_act.spawnstatue) return;
    // Check monster is not jumping already
    if (self.bmodel_act.jump_flag > time) return;
    
    // Block non-monsters and fly/swim if spawnflag not set
    if (!(self.bmodel_act.flags & FL_MONSTER)) return;
    if (!(self.spawnflags & TRIG_MONJUMPFLY) && self.bmodel_act.flags & FL_FLY) return;
    if (!(self.spawnflags & TRIG_MONJUMPSWIM) && self.bmodel_act.flags & FL_SWIM) return;

    // Is there any classname exception setup?
    if (self.noise1 != "") {
        if (self.bmodel_act.classname != self.noise1) return;
        // extra special condition for enraged drole's
        if (self.bmodel_act.classtype == CT_MONDROLE && !self.bmodel_act.attack_rage) return;
    }
    
    // Check for any triggers, fire them once!
    if (self.target != "") {
        trigger_strs(self.target, activator);
        self.target = "";
    }
    
    // Flying/Swimming monsters only need a push in the right direction
    if (self.bmodel_act.flags & FL_FLY || self.bmodel_act.flags & FL_SWIM) {
        self.bmodel_act.velocity = self.movedir * self.speed;
    }
    else {
        // set XY even if not on ground, so the jump will clear lips
        self.bmodel_act.velocity_x = self.movedir_x * self.speed;
        self.bmodel_act.velocity_y = self.movedir_y * self.speed;
        // If monster on the ground, lift them up    
        if (self.bmodel_act.flags & FL_ONGROUND ) {
            self.bmodel_act.flags = self.bmodel_act.flags - FL_ONGROUND;
            self.bmodel_act.velocity_z = self.height;
        }
    }

    // Setup to trigger push once?
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_monsterjump =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGMONJUMP;
    if (!self.speed) self.speed = 200;
    if (!self.height) self.height = 200;
    if (self.angles_y == 0) self.angles_y = 360;
    // Work out facing angle
    InitTrigger ();
        
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    
    // Setup Entity State functionality
    self.estate_fire = trigger_monsterjump_fire;
    self.touch = trigger_bmodel_anytouch;
    trigger_bmodel_setup();

    // If target is setup, calculate new facing angle
    if (self.target != "") {
        self.nextthink = time + 2;
        self.think = TargetMovedir;
    }
};

//----------------------------------------------------------------------
void() trigger_drolejump =
{
    self.noise1 = "monster_drole";
    trigger_monsterjump();
};

/*======================================================================
/*QUAKED trigger_monsterturret (0.5 0.3 0) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Temporarily turn a monster into a turret
-------- KEYS --------
targetname : trigger entity (works with entity state system)
wait   : -1 = Only trigger a monster turret function once
count  : random chance to pause; constant = -1, def = 0.25, range = 0 - 1
noise1 : only works with this type of monster (monster_ogre)
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Temporarily turn a monster into a turret
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_monsterturret_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'

    local entity tself;
    
    if (self.estate & ESTATE_BLOCK) return;
    if (self.attack_finished > time) return;    // trigger_once functionality
    if (!(self.bmodel_act.flags & FL_MONSTER)) return;    // ONLY Monsters!
    if (self.bmodel_act.health < 1) return;        // Dead things cannot be turrets!
    if (self.bmodel_act.movespeed < 0) return;    // Already a turret!?!
    if (!self.bmodel_act.enemy) return;            // This is a combat node only
    if (self.bmodel_act.turretactive) return;    // Already using a turret position
    if (!self.bmodel_act.th_missile) return;    // Only works if got range attack
    if (self.noise1 != "") { if (self.noise1 != self.bmodel_act.classname) return; }

    // Switch to monster
    tself = self;
    self = self.bmodel_act; other = tself;
    //  check enemy visibility and direction
    if (visible(self.enemy) && infront(self.enemy)) {
        // Link the monster to the turret (used by ai_run)
        self.turretactive = other;
    }

    self = tself;
    // Setup to trigger push once?
    if (self.wait < 0) {
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_monsterturret =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGMONTURRET;
    if (self.count < 0) self.count = -1;
    else if (self.count == 0 || self.count > 1) self.count = 0.25;
    InitTrigger ();
    
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    
    // Setup Entity State functionality
    self.estate_fire = trigger_monsterturret_fire;
    self.touch = trigger_bmodel_anytouch;
    trigger_bmodel_setup();
};

/*======================================================================
/*QUAKED trigger_hurt (.5 .5 .5) ? x BUBBLES MONSTER_ONLY x MODCHECK FALLING STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Will hurt any touching entity
-------- KEYS --------
targetname : trigger entity (works with entity state system)
dmg     : damage from contact with trigger (def=5)
wait    : time between pain contact (def=1s)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
speed   : used by falling spawnflag for velocity check (def=300)
height  : Maximum travel distance up for bubbles (default trigger size)
count   : total amount of active bubbles at once (default 5)
style   : 1-15 (grey,brown1,blue1,green1,red1,brown2,pinkyel,brown3,purp1,purp2,brown4,green2,yellow,blue2,red2)
-------- SPAWNFLAGS --------
BUBBLES  : Spawn bubbles within trigger volume when active
MONSTER_ONLY : Will only affect monsters
MODCHECK : Will remove this entity if THIS mod is active
FALLING  : Only hurts if the player is falling (speed=velocity)
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Will hurt any touching entity that can take damage
This entity cannot be damaged and is always touchable once activated

======================================================================*/
void() trigger_hurt_fire =
{
    // This is after bmodel _use, _killed and _touch, so any reference
    // to the trigger activator has to go through 'bmodel_act'
    if (self.attack_finished > time) return;
    if (self.bmodel_act.takedamage == DAMAGE_NO) return;
    if (self.spawnflags & TRIG_HURTMONSTER && !(self.bmodel_act.flags & FL_MONSTER)) return;
    // Check for falling damage conditions (player + flying + speeding)
    if (self.spawnflags & TRIG_HURTFALLING) {
        // Only affects players and monsters
        if (!(self.bmodel_act.flags & FL_MONSTER) &&
            !(self.bmodel_act.flags & FL_CLIENT)) return;
        if (self.bmodel_act.flags & FL_ONGROUND) return;
        // Can't kill something dead already!
        if (self.bmodel_act.health < 1) return;
        if (fabs(self.bmodel_act.velocity_z) < self.speed) return;
    }
    
    // Check for any dead monster bodies (no exceptions)
    if (trigger_check_body(self.bmodel_act,DEAD_EXPLODE)) return;

    // Check for godmode players taking screenshots!
    if (self.bmodel_act.flags & FL_CLIENT && self.bmodel_act.flags & FL_GODMODE) return;
    
    // Block touch function based on wait time
    self.attack_finished = time + self.wait;
    T_Damage (self.bmodel_act, self, self, self.dmg, DAMARMOR);
};

//----------------------------------------------------------------------
void() trigger_hurt_on =
{
    self.estate = ESTATE_ON;
    self.solid = SOLID_TRIGGER;
    // Restore bounding box (dev testing visual thing)
    setsize (self, self.bbmins, self.bbmaxs);
    // Spawn bubbles inside volume brush
    trigger_spawn_bubbles();
};

//----------------------------------------------------------------------
void() trigger_hurt =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    if (self.spawnflags & TRIG_MODCHECK) { remove(self); return; }
    self.classtype = CT_TRIGHURT;
    InitTrigger ();
    // No trigger damage functionality and always touchable!
    self.spawnflags = self.spawnflags | TRIG_ALWAYTOUCH | TRIG_NODAMAGE;
    if (self.dmg < 0) self.dmg = 0;
    else if (self.dmg == 0) self.dmg = 5;
    if (self.wait <= 0) self.wait = 1;

    // Setup bubble model/counter/volume
    if (self.spawnflags & TRIG_SPAWNBUBBLES) trigger_setup_bubbles();
    
    // Setup default falling damage speed
    if (self.spawnflags & TRIG_HURTFALLING && self.speed < 1) self.speed = 300;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = trigger_hurt_on;
    self.estate_off = trigger_bmodel_off;
    self.estate_disable = trigger_bmodel_disable;
    if (self.dmg > 0) {
        self.estate_fire = trigger_hurt_fire;
        self.touch = trigger_bmodel_anytouch;
    }

    // Switch on OR off?
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

/*======================================================================
/*QUAKED trigger_heal (.5 .5 .5) ? x BUBBLES x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Heals any player who touches this trigger
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target  : trigger once when players stands in trigger (start cycle)
target2 : trigger once when healing expires (finish cycle)
wait    : time between healing (default 1s)
healamount : amount to heal each time touched (default 10)
max_health : total amount to heal (default 50, -1=no limit)
sounds  : 1=heal_15, 2=heal_25, 3=heal_100, 4=respawn, 5=custom (default 1)
noise   : Custom sound for healing
lip     : All messages are silent to the player (heal/expire) 1=block heal 2=block all
message : centerprints when players stands in trigger
message2 : centerprints when healing function has expired
height  : Maximum travel distance up for bubbles (default trigger size)
count   : total amount of active bubbles at once (default 5)
style   : 1-15 (grey,brown1,blue1,green1,red1,brown2,pinkyel,brown3,purp1,purp2,brown4,green2,yellow,blue2,red2)
yaw_speed : spawning rate (def=0.5) for bubbles (speed + random() x speed)
-------- SPAWNFLAGS --------
BUBBLES  : Spawn bubbles within trigger volume when active
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Heals any player who touches this trigger, can be triggered on/off and produces 
bubbles within the bounding box of the trigger when spawnflag enabled

======================================================================*/
void() trigger_heal_touch =
{
    if (self.estate & ESTATE_BLOCK) return;        // Function off/disabled
    if (self.attack_finished > time) return;    // Touch blocked (temporary)
    if ( !(other.flags & FL_CLIENT) ) return;    // Only works with clients
    if (other.health < 1 ) return;                // Cannot heal, target is dead
    if (self.health < 1) return;                // Run out of health
    
    // Do not constantly check healing, use wait
    self.attack_finished = time + self.wait;

    // Can the pool heal the player?
    if (!T_Heal(other, self.healamount, 0)) return;

    // Healing sound        
    sound (other, CHAN_BODY, self.noise, 1, ATTN_NORM);
    // Has the pool run out of health?
    if (self.max_health > 0) self.health = self.health - self.healamount;
        
    // Has the healing trigger expired?
    if (self.health < 1) {
        // Only block message lip=1 healing 2=all messages
        if (self.lip < 2) centerprint (other, self.message2);
        // Fire any targets once (finish of healing cycle)
        if (self.target2 != "") {trigger_strs(self.target2, other);self.target2 = "";}
        // Switch off healing trigger
        entity_state_off();
    }
    else {
        // Fire any targets once (start of healing cycle)
        if (self.target != "") {trigger_strs(self.target, other);self.target = "";}
        // Display healing message (check for lip block)
        if (!self.lip) centerprint (other, self.message);
    }
};    

//----------------------------------------------------------------------
void() trigger_heal_on =
{
    if (self.health > 0) {
        self.estate = ESTATE_ON;
        self.solid = SOLID_TRIGGER;
        // Restore bounding box (dev testing visual thing)
        setsize (self, self.bbmins, self.bbmaxs);
        // Spawn bubbles inside volume brush
        trigger_spawn_bubbles();
    }
};

//----------------------------------------------------------------------
void() trigger_heal_reset =
{
    if (self.max_health > 0) {
        // Reset health, targets and switch on entity 
        self.health = self.max_health;
        if (self.noise1 != "") self.target = self.noise1;
        if (self.noise2 != "") self.target2 = self.noise2;
        self.estate_on();
    }
};

//----------------------------------------------------------------------
void() trigger_heal =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGHEAL;
    InitTrigger ();

    // Setup default healing sound
    if (self.sounds == 2) self.noise = SOUND_HEAL25;
    else if (self.sounds == 3) self.noise = SOUND_HEAL100;
    else if (self.sounds == 4) self.noise = SOUND_RESPAWN;
    else if (self.sounds == 5 && self.noise == "") self.noise = SOUND_HEAL15;
    else self.noise = SOUND_HEAL15;
    precache_sound(self.noise);

    // Setup bubble model/counter/volume
    if (self.spawnflags & TRIG_SPAWNBUBBLES) trigger_setup_bubbles();

    if (!self.wait) self.wait = 1;                    // Default trigger time
    if (self.healamount < 1) self.healamount = 10;    // Quantity to heal each touch trigger
    if (!self.max_health) self.max_health = 50;        // Default max healing
    // Cannot have healamount large than max, need to cap healamount
    if (self.max_health > 0 && self.max_health < self.healamount) 
        self.healamount = self.max_health;

    if (self.max_health < 0) self.health = 100;        // max < 0 = Infinite healing
    else self.health = self.max_health;                // Reset total ready
        
    if (!self.message) self.message = "You feel the effects of the healing pool";
    if (!self.message2) self.message2 = "The Healing Pool has expired!";
    
    // Save any trigger names for reset events    
    if (self.target != "" ) self.noise1 = self.target;
    if (self.target2 != "" ) self.noise2 = self.target2;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = trigger_heal_on;
    self.estate_off = trigger_bmodel_off;
    self.estate_disable = trigger_bmodel_disable;
    self.estate_reset = trigger_heal_reset;
    self.touch = trigger_heal_touch;

    // Switch on OR off?
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

/*======================================================================
/*QUAKED trigger_touchsound (.5 .5 .5) ? x x WORLDGEO DRAIN x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Plays sounds when touched by the player
------- KEYS --------
targetname : trigger entity (works with entity state system)
sounds     : 1=Water (def) 2=Slime 3=Lava 4=silent 5=custom
noise      : Custom trigger touch sound
noise1     : Custom trigger exit sound
noise2     : Custom draining sound
speed      : Time (def=1.5s) to drain liquid
yaw_speed  : Vertical drain speed (def=0.05)
water_alpha: Alpha value for liquid (override worldspawn)
-------- SPAWNFLAGS --------
WORLDGEO : Will draw bmodel (not just a trigger)
DRAIN    : Drain effect when trigger_disabled
STARTOFF : Requires trigger to activate
------- NOTES --------
Plays sounds when touched by the player

======================================================================*/
void() trigger_tsound_touch =
{
    if (self.estate & ESTATE_BLOCK) return;        // Function off/disabled
    if (self.attack_finished > time) return;    // Touch blocked (temporary)
    if ( !(other.flags & FL_CLIENT) ) return;    // Only works with clients
    if (other.health < 1 ) return;                // other is dead

    if (other.touchedliquid < time)
        sound (other, CHAN_BODY, self.noise, 1, ATTN_NORM);
    other.touchedliquid = time + 0.1;
    other.touchedsound = self.noise1;
    self.attack_finished = time + 0.05;
};

//----------------------------------------------------------------------
void() trigger_tsound_on =
{
    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);
    
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_TRIGGER;
    self.origin = self.oldorigin;
    self.alpha = self.water_alpha;
    if (self.spawnflags & TRIG_TSOUNDWGEO) setmodel (self, self.mdl);
};

//----------------------------------------------------------------------
void() trigger_tsound_fade =
{
    // Take draining time and divide by time passed
    self.lip = self.speed - ((time - self.ltime) / self.speed);
    // Start at current alpha value and move towards 0
    self.alpha = self.lip * (self.water_alpha / self.speed);
    // Slowly sink into the ground as alpha fading
    self.origin_z = self.origin_z - (self.speed * self.yaw_speed);
    // Exit condition
    if (self.alpha <= 0) {
        self.alpha = 0.01;
        self.modelindex = 0;        // Make sure no model
        self.model = "";
        self.estate = ESTATE_OFF;
        return;
    }
    // Keep on loop (using minimum QS time segment)
    self.nextthink = time + FADEMODEL_TIME;
};

//----------------------------------------------------------------------
void() trigger_tsound_drain =
{ sound(self, CHAN_AUTO, self.owner.noise2, 1, ATTN_NORM); };

//----------------------------------------------------------------------
void() trigger_tsound_disable =
{
    if (self.estate & ESTATE_BLOCK) return;
    self.solid = SOLID_NOT;
    self.estate = ESTATE_DISABLE;
    
    if (self.spawnflags & TRIG_TSOUNDDRAIN) {
        // Allow for bmodel timer to alpha correctly
        self.ltime = time;
        // Gradually fade and lower bmodel
        self.think = trigger_tsound_fade;
        self.nextthink = time + FADEMODEL_TIME;
        // Start playing draining sound
        self.sound_emitter.think = trigger_tsound_drain;
        self.sound_emitter.nextthink = time + self.super_time;
    }
};

//----------------------------------------------------------------------
void() trigger_tsound_syncalpha =
{
    // Allow for trigger to override global water alpha
    if (self.water_alpha > 0) self.alpha = self.water_alpha;
    else if (liquid_alpha > 0) self.alpha = liquid_alpha;
    else self.alpha = 1;
    // Save this value for later
    self.water_alpha = self.alpha;
    if (!(self.spawnflags & ENT_STARTOFF)) self.estate_on();
};

//----------------------------------------------------------------------
void() trigger_touchsound =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGTSOUND;
    self.mdl = self.model;
    InitTrigger ();
    
    if (!self.speed) self.speed = 1.5;
    if (!self.yaw_speed) self.yaw_speed = 0.05;
    if (self.super_time < 0.1) self.super_time = 0.1;
    self.attack_finished = 0;
    self.oldorigin = self.origin;

    // Default sounds (water)
    if (self.sounds < 1) self.sounds = 1;
    
    // Precache all sounds
    if (self.sounds == 1) {
        if (self.noise == "") self.noise = "player/inh2o.wav";
        if (self.noise1 == "") self.noise1 = "misc/outwater.wav";
    }
    else if (self.sounds == 2) {
        if (self.noise == "") self.noise = "player/slimbrn2.wav";
        if (self.noise1 == "") self.noise1 = "misc/outwater.wav";
    }
    else if (self.sounds == 3) {
        if (self.noise == "") self.noise = "player/inlava.wav";
        if (self.noise1 == "") self.noise1 = "misc/outwater.wav";
    }
    // make sure there is always a sound (empty) defined
    if (self.noise == "") self.noise = SOUND_EMPTY;
    if (self.noise1 == "") self.noise1 = SOUND_EMPTY;

    precache_sound(self.noise);
    precache_sound(self.noise1);    
    
    if (self.spawnflags & TRIG_TSOUNDDRAIN) {
        if (self.noise2 == "") self.noise2 = "ambience/liquid_drain.wav";
        precache_sound(self.noise2);
        // Create an entity to play drain sound
        // bmodel origins are at 0,0,0 so never play properly
        self.sound_emitter = spawn();
        self.sound_emitter.owner = self;
        self.sound_emitter.origin = bmodel_origin(self) + '0 0 32';
        self.sound_emitter.solid = SOLID_NOT;
        self.sound_emitter.movetype = MOVETYPE_NONE;
        setmodel(self.sound_emitter, MODEL_EMPTY);
        setorigin(self.sound_emitter, self.sound_emitter.origin);
    }
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = trigger_tsound_on;
    self.estate_off = trigger_bmodel_off;
    self.estate_disable = trigger_tsound_disable;
    self.touch = trigger_tsound_touch;

    // Switch off?
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    
    // Sync the water alpha console variable
    self.think = trigger_tsound_syncalpha;
    self.nextthink = time + 0.1 + random();
};

/*======================================================================
/*QUAKED trigger_void (.5 .5 .5) ? NO_CLIENT NO_MONSTER NO_AMMO NO_EGG NO_TEMP NO_ITEM STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Garbage collector for bottom of skyboxes
------- KEYS --------
targetname : trigger entity (works with entity state system)
-------- SPAWNFLAGS --------
NO_CLIENT  : Ignore clients (anything flagged as a client)
NO_MONSTER : Ignore monsters (anything flagged as a monster)
NO_AMMO    : Ignore ammo types (all ammo projectile types)
NO_EGG     : Ignore minion eggs (affects shalrath, wraiths)
NO_TEMP    : Ignore temporary ents (gibs, sparks, smoke)
NO_ITEM    : Ignore items (weapons,armor,keys,runes,powerups)
STARTOFF : Requires trigger to activate
------- NOTES --------
Garbage collector for bottom of skyboxes
======================================================================*/
void() trigger_void_touch =
{
    // Exception, entity state and particles!
    if (self.estate & ESTATE_BLOCK) return;
    if (other.classtype == CT_PARTICLE) return;

    // Check for any dead monster bodies (no exceptions)
    if (trigger_check_body(other,DEAD_REMOVE)) return;
    
    // Process spawnflag exceptions
    if (self.spawnflags & TRIG_VOIDNOCLIENT && other.flags & FL_CLIENT) return;
    if (self.spawnflags & TRIG_VOIDNOMONSTER && other.flags & FL_MONSTER) return;
    if (self.spawnflags & TRIG_VOIDNOAMMO) {
        if (other.classgroup == CG_PROJALL) return;
        else if (other.classgroup == CG_PROJSHELLS) return;
        else if (other.classgroup == CG_PROJNAILS) return;
        else if (other.classgroup == CG_PROJROCKETS) return;
        else if (other.classgroup == CG_PROJCELLS) return;
    }
    if (self.spawnflags & TRIG_VOIDNOGG && other.classgroup == CG_MINIONEGG) return;
    if (self.spawnflags & TRIG_VOIDNOTEMP && other.classgroup == CG_TEMPENT) return;
    if (self.spawnflags & TRIG_VOIDNOITEM) {
        if (other.classgroup == CG_WEAPON) return;
        else if (other.classgroup == CG_AMMOITEM) return;
        else if (other.classgroup == CG_ARMOR) return;
        else if (other.classgroup == CG_KEY) return;
        else if (other.classgroup == CG_RUNE) return;
        else if (other.classgroup == CG_ARTIFACT) return;
    }
    
    // flag touching entity, so other functions will ignore it
    other.touchedvoid = TRUE;
    
    // Let monsters and clients die through their own functions
    if (other.flags & FL_CLIENT || other.flags & FL_MONSTER)
        T_Damage (other, self, self, other.health+8, NOARMOR);
    else {
        // Remove all ammo projectiles on contact
        // Hide other items types (may have particles active)
        if (other.classgroup == CG_PROJALL) entity_remove(other,0.1);
        else if (other.classgroup == CG_PROJSHELLS) entity_remove(other,0.1);
        else if (other.classgroup == CG_PROJNAILS) entity_remove(other,0.1);
        else if (other.classgroup == CG_PROJROCKETS) entity_remove(other,0.1);
        else if (other.classgroup == CG_PROJCELLS) entity_remove(other,0.1);
        else if (other.classgroup == CG_MINIONEGG) entity_remove(other,0.1);
        else if (other.classgroup == CG_TEMPENT) entity_remove(other,0.1);
        else if (other.classgroup == CG_WEAPON) entity_hide(other);
        else if (other.classgroup == CG_AMMOITEM) {
            entity_hide(other);
            // Hide any shell/nail lids
            if (other.attachment) entity_hide(other.attachment);
        }
        else if (other.classgroup == CG_ARMOR) entity_hide(other);
        else if (other.classgroup == CG_KEY) entity_hide(other);
        else if (other.classgroup == CG_RUNE) entity_hide(other);
        else if (other.classgroup == CG_ARTIFACT) entity_hide(other);
        else if (other.classgroup == CG_MISCENT) entity_hide(other);
        else if (other.classgroup == CG_BREAKABLE) entity_hide(other);
    }
};

//----------------------------------------------------------------------
void() trigger_void =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    self.classtype = CT_TRIGVOID;
    InitTrigger ();
    setsize (self, self.bbmins, self.bbmaxs);

    // Setup Entity State functionality
    // This does not work via typical trigger bmodel paths
    // This trigger is special, needs to work straight away
    // There is only a toggle/on/off state function
    if (self.targetname != "") self.use = entity_state_use;
    self.touch = trigger_void_touch;
    if (self.spawnflags & ENT_STARTOFF) self.estate = ESTATE_OFF;
    else self.estate = ESTATE_ON;
};
