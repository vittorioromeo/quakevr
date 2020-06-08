/*======================================================================
 PLATFORM FUNCTIONS
======================================================================*/
float PLAT_LOW_TRIGGER = 1;
float PLAT_MIN_TRIGGER = 4;
float PLAT_START_LOWER = 8;
float PLAT_START_OPEN = 16;
float PLAT_STARTDIS = 32;        // Platform starts in disabled state

/*======================================================================
/*QUAKED func_plat (0 .5 .8) ? LOW_TRIGGER x MIN_TRIGGER START_LOWER START_OPEN x STARTOFF x
A Platform (bmodel) with 2 states
-------- KEYS --------
targetname : = "" plat start low, != "" plat starts high and reqs trigger to work
if using entity state system set START_LOWER spawnflags to fix this problem
speed  : moving speed (def=150)
height : determines the distance to move, instead of bmodel bounds
sounds : 1=Base, 0 or 2=Medieval (def), 4=Silent, 5=Custom sounds
noise  : custom sound - moving (looping)
noise1 : custom sound - Stopped
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
LOW_TRIGGER : touch trigger will not keep the platform at the top, will reset
MIN_TRIGGER : uses the min bounding box (instead of max) to calculate trigger
START_LOWER : will start the platform lower regardless of targetname
START_OPEN : Used for lighting issues, place at bottom position
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Platforms are always created in the upper position, so they can be lit better.
If targetname is defined the func_plat starts high and is locked until triggered
Once the func_plat has been triggered, it will become a normal platform.

======================================================================*/

void() func_plat_hit_bottom =
{
    if (self.estate == ESTATE_OFF) return;
    self.state = STATE_BOTTOM;
    sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
};

//----------------------------------------------------------------------
void() func_plat_go_down =
{
    if (self.estate == ESTATE_OFF) return;
    self.state = STATE_DOWN;
    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    SUB_CalcMove (self.pos2, self.speed, func_plat_hit_bottom);
};

//----------------------------------------------------------------------
void() func_plat_hit_top =
{
    if (self.estate == ESTATE_OFF) return;
    self.state = STATE_TOP;
    sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
    self.think = func_plat_go_down;
    self.nextthink = self.ltime + 3;
};

//----------------------------------------------------------------------
void() func_plat_go_up =
{
    if (self.estate == ESTATE_OFF) return;
    self.state = STATE_UP;
    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    SUB_CalcMove (self.pos1, self.speed, func_plat_hit_top);
};

//----------------------------------------------------------------------
void() func_plat_trigger_touch =
{
    if (self.owner.estate & ESTATE_BLOCK) return;
    if (self.owner.state == STATE_DISABLED) return;
    if (!(other.flags & FL_CLIENT)) return;
    if (other.health <= 0) return;

    // Switch the platform entity
    self = self.owner;
    if (self.state == STATE_BOTTOM) func_plat_go_up ();
    else if (self.state == STATE_TOP)
        self.nextthink = self.ltime + 1;    // delay going down
};

//----------------------------------------------------------------------
void() func_plat_trigger_spawn =
{
    local entity trigger;
    local vector tmin, tmax;

    trigger = spawn();
    trigger.owner = self;
    trigger.touch = func_plat_trigger_touch;
    trigger.movetype = MOVETYPE_NONE;
    trigger.solid = SOLID_TRIGGER;
    
    tmin = self.mins + '25 25 0';
    tmax = self.maxs - '25 25 -8';

    // The ID method is to take the maximum size of the platform
    // and subtract the height. This fails if the platform
    // is moving around a central height point. The Alternative
    // method uses the min bounding box value instead of max
    // This is not 100% capatible with id maps, so hence extra option!
    if (self.spawnflags & PLAT_MIN_TRIGGER) tmin_z = tmin_z - (self.height + 8);
    else tmin_z = tmax_z - (self.pos1_z - self.pos2_z + 8);

    if (self.spawnflags & PLAT_LOW_TRIGGER) tmax_z = tmin_z + 8;
    
    
    if (self.size_x <= 50) {
        tmin_x = (self.mins_x + self.maxs_x) / 2;
        tmax_x = tmin_x + 1;
    }
    if (self.size_y <= 50) {
        tmin_y = (self.mins_y + self.maxs_y) / 2;
        tmax_y = tmin_y + 1;
    }
    
    setsize (trigger, tmin, tmax);
};

//----------------------------------------------------------------------
void() func_plat_use =
{
    // Deal with START OFF functionality first
    if (self.spawnflags & ENT_STARTOFF) self.estate_on();
    else {
        // Block USE functionality if state wrong
        if (self.estate & ESTATE_BLOCK) return;
        if (self.state == STATE_DISABLED) func_plat_go_down();
    }
};

//----------------------------------------------------------------------
void() func_plat_on =
{
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);
    
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);

    if (self.state != STATE_DISABLED) {
        // Reset health, state and frame back to default
        setorigin(self, self.pos2);
        self.state = STATE_BOTTOM;
        self.think = SUB_Null;
    }

    // If platform started disable, release to lower position
    if (self.spawnflags & PLAT_STARTDIS) {
        // No longer need this spawnflag, remove it
        self.spawnflags = self.spawnflags - (self.spawnflags & PLAT_STARTDIS);
        if (self.state == STATE_DISABLED) func_plat_go_down();
    }    
};

//----------------------------------------------------------------------
void() func_plat_off =
{
    // Stop re-triggering OFF state
    if (self.estate == ESTATE_OFF) return;

    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    
    // Stop all movement sounds
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);

    if (self.state != STATE_DISABLED) {
        // Reset health, position and frame back to default
        setorigin(self, self.pos2);
        self.state = STATE_BOTTOM;
        self.think = SUB_Null;
    }
};

//----------------------------------------------------------------------
void() func_plat_disable =
{
    // Lock platform in current location
    self.estate = ESTATE_DISABLE;
};

//----------------------------------------------------------------------
void() func_plat_blocked =
{
    T_Damage (other, self, self, 1, DAMARMOR);
    
    if (self.state == STATE_UP) func_plat_go_down ();
    else if (self.state == STATE_DOWN) func_plat_go_up ();
};

//----------------------------------------------------------------------
void() func_plat =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors
    
    // default sound = medieval
    if (self.sounds == 0) self.sounds = 2;

    if (self.sounds == 1) {
        self.noise = "plats/plat1.wav";
        self.noise1 = "plats/plat2.wav";
    }
    else if (self.sounds == 2) {
        self.noise = "plats/medplat1.wav";
        self.noise1 = "plats/medplat2.wav";
    }
    else {
        // sounds 4 = silent, sounds 5 = custom
        if (self.noise == "" || self.sounds == 4) self.noise = SOUND_EMPTY;
        if (self.noise1 == "" || self.sounds == 4) self.noise1 = SOUND_EMPTY;
    }
    precache_sound (self.noise);
    precache_sound (self.noise1);
    
    self.classtype = CT_FUNCPLAT;
    self.classgroup = CG_FUNCMOVER;
    self.bsporigin = TRUE;
    self.mdl = self.model;
    // angles has to be 0 0 0 otherwise brush model is twisted
    // save angles to mangle for use later by movement code
    self.mangle = self.angles;
    self.angles = '0 0 0';

    // This is not used anywhere, old code ideas from ID
    if (!self.t_length) self.t_length = 80;
    if (!self.t_width) self.t_width = 10;
    if (!self.speed) self.speed = 150;
    self.state = STATE_BOTTOM;    // Default state (lower)

    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    setorigin (self, self.origin);    
    setmodel (self, self.mdl);
    setsize (self, self.mins , self.maxs);

    // Cannot have a platform start off or disabled
    // with no targetname, how can it be actived!?!
    if (self.spawnflags & PLAT_STARTDIS && self.targetname == "") {
        dprint("\b[PLAT]\b Starting DISABLED with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    
    if (self.spawnflags & ENT_STARTOFF && self.targetname == "") {
        dprint("\b[PLAT]\b Starting OFF with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
    
    // Move the platform up to open position (lighting issues)
    // Need to update mins/maxs so that spawn trigger is correct
    // otherwise it will be in original position and spawn low
    if (self.spawnflags & PLAT_START_OPEN) {
        self.origin_z = self.origin_z + self.height;
        self.mins_z = self.mins_z + self.height;
        self.maxs_z = self.maxs_z + self.height;
    }

    // pos1 is the top position, pos2 is the bottom
    self.pos1 = self.pos2 = self.origin;
    // self.height cannot be a negative, min/maxs will be back-to-front
    if (self.height) self.pos2_z = self.origin_z - fabs(self.height);
    else self.pos2_z = self.origin_z - self.size_z + 8;

    // Setup touch trigger and block function
    func_plat_trigger_spawn ();
    self.blocked = func_plat_blocked;
    
    // Damn annoying that the targetname is being used like this because
    // there could have been a better way to do this type of functionality
    // == "" Platform starts in lower position (default)
    // != "" Platform starts in upper position and requires trigger to work
    //
    if (self.targetname != "" && !(self.spawnflags & PLAT_START_LOWER) )
        self.state = STATE_DISABLED;
    else setorigin (self, self.pos2);    // Start in lower position

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_plat_on;
    self.estate_off = func_plat_off;
    self.estate_use = func_plat_use;
    self.estate_disable = func_plat_disable;
    
    // Check for the easy way to start a platform disabled
    if (self.spawnflags & PLAT_STARTDIS) self.estate_disable();
    // Check for starting override for entity state
    else if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

//----------------------------------------------------------------------
// Re-direction for map hacks (not used normally)
//----------------------------------------------------------------------
void() plat_hit_bottom = { func_plat_hit_bottom(); };
