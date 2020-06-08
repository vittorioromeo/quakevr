/*======================================================================
 TRIGGER STATE FUNCTIONS
======================================================================*/
float TRIG_NOTOUCH = 1;            // Touch functionality is disabled
float TRIG_DEVMODE = 4;            // Will only trigger in dev model
float TRIG_INVIEW = 8;            // player has to see trigger
float TRIG_MODCHECK = 16;        // Will remove this entity if THIS mod is active
float TRIG_MONSTERS = 32;        // Can be touched/triggered by monsters
// float ENT_STARTOFF = 64;        // Global spawnflags setting

// Added by functions, no map entity interaction
float TRIG_NODAMAGE = 16384;    // Block trigger damage functionality
float TRIG_ALWAYTOUCH = 32768;    // Always allow touch function to exist

//----------------------------------------------------------------------
void() trigger_bmodel_sounds =
{
    // Used by lots of different triggers for standard sounds
    if (self.sounds == 1) self.noise = "misc/secret.wav";
    else if (self.sounds == 2) self.noise = SOUND_TALK;
    else if (self.sounds == 3) self.noise = "misc/trigger1.wav";
    else if (self.sounds == 4) self.noise = SOUND_EMPTY;
    else if (self.sounds == 6) self.noise = "misc/secret3.wav";
    // precache any sounds that have been defined
    if (self.noise != "") precache_sound(self.noise);
};

//----------------------------------------------------------------------
// Special sight trigger, Is the player *looking* at this trigger
// This sight trigger cannot be disabled or turned off atm
//----------------------------------------------------------------------
void() trigger_bmodel_inview =
{
    local entity client, tself;
    local float play_dist, play_range, play_angle;

    // Is the trigger blocked? (trigger_once)
    if (self.attack_finished > time) return;
    
    // Checkclient needs an proper entity origin, bmodels are '0 0 0'
    // Create a temporary entity to switch around too for checkclient
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.oldorigin = bmodel_origin(self);
        setorigin(self.attachment, self.oldorigin);
    }
    
    // Swap around self/attachment so checkclient can work properly
    tself = self; self = self.attachment;
    client = checkclient ();        // Find a client in current PVS
    self = tself;
    
    // Found a player to check sight/distance for?
    if (client.flags & FL_CLIENT) {
        if (visxray(client, VEC_ORIGIN, VEC_ORIGIN, FALSE)) {
            // Work out vector length between entity and player    
            play_dist = vlen(self.oldorigin - client.origin);
            // Is there a distance check active? if not make sure it will pass
            if (!self.t_length) play_range = play_dist + 1;
            else play_range = self.t_length;
            
            // The angle the player is facing towards the trigger (45 = forward)
            play_angle = viewangle(self.oldorigin, client.origin, 45, TRUE, client.v_angle_y);
        
            // Check distance is correct and facing angle focused
            if (play_dist < play_range && play_angle > 30 && play_angle < 60) {
                self.wait = -1;
                self.bmodel_act = activator;
                self.estate_fire();
                return;
            }
        }
    }

    // Keep checking, 0.1 standard entity timing
    self.nextthink = time + 0.1;
    self.think = trigger_bmodel_inview;
};

//----------------------------------------------------------------------
//  the trigger was just touched/killed/used
//  self.bmodel_act should be set to the activator so it can be held 
//  through a delay so wait for the delay time before firing
//----------------------------------------------------------------------
void() trigger_bmodel_fire =
{
    // Is the trigger blocked? (trigger_once)
    if (self.attack_finished > time) return;
    
    // Check for developer mode only triggers
    if (self.spawnflags & TRIG_DEVMODE && developer == 0) {
        // This feature only work for ONCE/MULTIPLE triggers
        if (self.classtype == CT_TRIGONCE || 
            self.classtype == CT_TRIGMULTI) return;
    }
    
    //----------------------------------------------------------------------
    // The Shotgun and Axe upgrades can make maps too easy, allow for
    // triggers to not fire if the key is TRUE and inventory empty
    //----------------------------------------------------------------------
    if ( self.bmodel_act.flags & FL_CLIENT ) {
        if (self.upgrade_axe && !(self.bmodel_act.moditems & IT_UPGRADE_AXE)) return;
        if (self.upgrade_ssg && !(self.bmodel_act.moditems & IT_UPGRADE_SSG) ) return;
        if (self.upgrade_lg && !(self.bmodel_act.moditems & IT_UPGRADE_LG) ) return;
    }

    // Trigger secrets only work for players
    if (self.classtype == CT_TRIGSECRET) {
        // If trigger_secret has NOTOUCH then check activator
        if (!(self.bmodel_act.flags & FL_CLIENT)) return;
        // The trigger can be reset, so double check
        if (self.count > 0) {
            found_secrets = found_secrets + self.count;
            WriteByte (MSG_ALL, SVC_FOUNDSECRET);
            self.count = 0;    // Secrets only work once
        }
    }

    // Play the sound ON the trigger, NOT the activator
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    //----------------------------------------------------------------------
    // One of the downside to trigger_multi and trigger_once is that they
    // reset the activator global variable each time they are used.
    // This can causes problems with triggers that require client references
    // for centerprint/sound cues (like trigger_secret)
    // The way to get around this problem is to use trigger_relay as this does
    // not change the activator variable
    //
    activator = self.bmodel_act;
    SUB_UseTargets();

    // Is the trigger repeatable?
    if (self.wait > 0) {
        self.attack_finished = time + self.wait;
        self.nextthink = self.attack_finished;
        self.think = self.estate_on;
    }
    else {
        // TRIGGER_ONCE functionality
        self.attack_finished = LARGE_TIMER;
        self.estate_off();
    }
};

//----------------------------------------------------------------------
void() trigger_bmodel_use =
{
    // There is no client test here because this can come from
    // multiple entity types, the activator is passed on
    if (self.estate & ESTATE_BLOCK) return;
    self.bmodel_act = activator;
    self.estate_fire();
};

//----------------------------------------------------------------------
void() trigger_bmodel_touch =
{
    // This is a client touch event, triggers that require
    // other entity types need to have their own touch event
    if (self.estate & ESTATE_BLOCK) return;
    if ( !(self.spawnflags & TRIG_MONSTERS) && !(other.flags & FL_CLIENT) ) return;
    
    // Did the trigger have an angle key setup?
    if (CheckZeroVector(self.movedir) == FALSE) {
        makevectors (other.angles);
        if (v_forward * self.movedir < 0) return;        
    }
    
    self.bmodel_act = other;
    self.estate_fire();
};

//----------------------------------------------------------------------
void() trigger_bmodel_anytouch =
{
    // This will allow anything to touch it
    if (self.estate & ESTATE_BLOCK) return;
    self.bmodel_act = other;
    self.estate_fire();
};

//----------------------------------------------------------------------
void() trigger_bmodel_killed =
{
    // The proto button setup of triggers using health
    // Always reset health regardless if blocked or not
    self.health = self.max_health;
    if (self.estate & ESTATE_BLOCK) return;
    self.takedamage = DAMAGE_NO;
    self.bmodel_act = damage_attacker; 
    self.estate_fire();
};

//----------------------------------------------------------------------
// Setup touch/damage/bounding box functionality
//----------------------------------------------------------------------
void() trigger_bmodel_restore =
{
    // Damage functionality
    if (self.max_health > 0 && !(self.spawnflags & TRIG_NODAMAGE) ) {
        self.health = self.max_health;
        self.takedamage = DAMAGE_YES;
        self.solid = SOLID_BBOX;
    }
    // The extra option (ALWAYSTOUCH) was added because some triggers
    // have re-used spawnflag 1 for other purposes and they still need
    // touch founctionality
    else if ( !(self.spawnflags & TRIG_NOTOUCH) || self.spawnflags & TRIG_ALWAYTOUCH)
        self.solid = SOLID_TRIGGER;
    // Turn off touch functionality
    else self.solid = SOLID_NOT;

    // Restore bounding box (dev testing visual thing)
    setsize (self, self.bbmins, self.bbmaxs);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
};

//----------------------------------------------------------------------
void() trigger_bmodel_delay =
{
    if (self.estate == ESTATE_DISABLE) return;
    
    // Remove the START OFF functionality
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    // Reset USE and trigger_once conditions
    self.estate_use = trigger_bmodel_use;
    self.attack_finished = 0;
    self.estate = ESTATE_ON;

    // Check if the player needs to be inview before triggering?
    // It would be weird to have this on multiple/secret triggers
    if (self.classtype == CT_TRIGONCE && self.spawnflags & TRIG_INVIEW) {
        // Restore bounding box (dev testing visual thing)
        setsize (self, self.bbmins, self.bbmaxs);
        // Inview triggers are not touchable and cannot be damaged
        self.spawnflags = self.spawnflags | TRIG_NOTOUCH | TRIG_NODAMAGE;
        self.estate_use = trigger_bmodel_inview;
        self.estate_use();
        return;
    }
    
    //----------------------------------------------------------------------
    // I would have never of guessed this was used throughout the original
    // ID maps and there are countless cases of trigger_multiple entities
    // being used like buttons that can be damaged but not touched.
    // I get the impression this was a prototype stage of development where
    // everyone was waiting for func_button functionality to come online.
    //
    if (self.health && !(self.spawnflags & TRIG_NODAMAGE)) {
        self.max_health = self.health;            // save health for later
        self.th_die = trigger_bmodel_killed;    // damage trigger
    }
    else {
        // Setup touch functionality
        if ( !(self.spawnflags & TRIG_NOTOUCH) || self.spawnflags & TRIG_ALWAYTOUCH) {
            if (!self.touch) self.touch = trigger_bmodel_touch;
        }
    }
    
    // Setup touch/damage/bounding box functionality
    trigger_bmodel_restore();
};

//----------------------------------------------------------------------
void() trigger_bmodel_on =
{
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_NONE;

    // Check for delayed/trigger_once functionality?
    if (self.spawnflags & ENT_STARTOFF || self.attack_finished > time)
        self.solid = SOLID_NOT;
    else {
        // Setup touch/damage/bounding box functionality
        trigger_bmodel_restore();
    }
};

//----------------------------------------------------------------------
void() trigger_bmodel_off =
{
    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setsize(self, VEC_ORIGIN, VEC_ORIGIN);
    self.estate_disable();
};

//----------------------------------------------------------------------
void() trigger_bmodel_reset =
{
    // Reset trigger_once conditions
    self.attack_finished = 0;
    self.estate_on();
};

//----------------------------------------------------------------------
void() trigger_bmodel_disable =
{
    // Shutdown trigger completely
    self.takedamage = DAMAGE_NO;
    self.think = SUB_Null;
};

//----------------------------------------------------------------------
void() trigger_bmodel_default =
{
    self.estate_on = trigger_bmodel_on;
    self.estate_off = trigger_bmodel_off;
    if (!self.estate_fire) self.estate_fire = trigger_bmodel_fire;
    self.estate_disable = trigger_bmodel_disable;
    self.estate_reset = trigger_bmodel_reset;
    self.estate = ESTATE_ON;
};

//----------------------------------------------------------------------
void() trigger_bmodel_setup =
{
    trigger_bmodel_default();
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_bmodel_delay;

    if (self.spawnflags & ENT_STARTOFF) {
        self.estate_use = trigger_bmodel_delay;
        self.estate_on = trigger_bmodel_delay;
        self.estate_off();
    }
    // The original functionality of the trigger is to pass through
    else self.estate_use();
};

//----------------------------------------------------------------------
// Re-direction for map hacks (not used normally)
//----------------------------------------------------------------------
void() multi_trigger = { 
    trigger_bmodel_default();
    trigger_bmodel_fire();
};
void() multi_killed = { 
    trigger_bmodel_default();
    trigger_bmodel_killed();
};
void() multi_use = {
    trigger_bmodel_default();
    trigger_bmodel_use();
};
void() multi_touch = {
    trigger_bmodel_default();
    trigger_bmodel_touch();
};
void() multi_wait = {
    if (self.max_health) {
        self.health = self.max_health;
        self.takedamage = DAMAGE_YES;
        self.solid = SOLID_BBOX;
    }
};
