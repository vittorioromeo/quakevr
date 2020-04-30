/*======================================================================
 PRESSURE SWITCH FUNCTIONS
 
 * Originally started as an extension of the func_button entity
 * Had too many spawning/setup conditions and was made into own entity
 * Having the trigger, touch and movement on one entity did not work
 * Split the trigger/touch into a separate spawn function, not v flexible
 * Entity Detection has to be volume, only players have floor touch bbox
 * Added a separate trigger/touch entity to detect diff entity types
 * Trigger needed to keep track of multiple touching entities at once
 * Detecting intersection volumes without a touch function is cpu intense
  
======================================================================*/
float PSWITCH_STARTDIS = 32;        // Switch starts in disabled state
//----------------------------------------------------------------------

/*QUAKED func_pressureswitch (0 .5 .8) ? x x x x x x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Pressure switch (bmodel) with touch state only
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target : name of target(s) to trigger (only works with pressure relays)
angle  : Always moves downward (needs to reflect gravity pressure)
speed  : Move Speed (def=40)
lip    : lip remaining at end of move. (def=4)
sounds : 0=steam metal, 1=wooden, 2=metallic, 3=in-out, 4=Silent, 5=Custom
noise  : custom sound - 1st movement
noise1 : custom sound - 2nd movement ('buttons/switch21.wav')
message: Centerprint Message to display when triggered
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Pressure switch (bmodel) with touch state only
*/

//----------------------------------------------------------------------
void() func_pswitch_ready =
{
    // Check for entity state changes
    if (self.estate & ESTATE_BLOCK) return;

    self.frame = 0;
    self.state = STATE_UP;
};

//----------------------------------------------------------------------
void() func_pswitch_return =
{
    // Check for entity state changes
    // Do not block disable (let it move up)
    if (self.estate == ESTATE_OFF) return;

    if (self.state == STATE_DOWN) {
        if (self.presstimersound < time) {
            self.presstimersound = time + 1;
            // 2nd movement sound - going up
            sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
        }
        self.ltime = time;
        self.state = STATE_MOVING;
        SUB_CalcMove (self.pos1, self.speed, func_pswitch_ready);
    }
};

//----------------------------------------------------------------------
void() func_pswitch_finished =
{
    // Check for any completed message
    if (self.message != "") {
        // Write message to all clients
        msg_entity = self;
        WriteByte (MSG_ALL, SVC_CENTERPRINT);
        WriteString (MSG_ALL, self.message);
        if (self.wait < 0) self.message = "";
    }
    
    // Check for any completed trigger(s)
    if (self.target != "") {
        trigger_strs(self.target, world);
        if (self.wait < 0) self.target = "";
    }

    self.frame = 1;
    self.state = STATE_DOWN;

    // Check for entity state changes
    if (self.estate & ESTATE_BLOCK) return;

    self.think = func_pswitch_return;
    self.nextthink = time + 0.2;
};

//----------------------------------------------------------------------
void() func_pswitch_use = 
{
    // Check for entity state changes
    if (self.estate & ESTATE_BLOCK) return;
    // Block trigger events if moving pressure plate
    if (self.state == STATE_MOVING) return;

    // Is the pressure plate UP?
    if (self.state == STATE_UP) {
        if (self.presstimersound < time) {
            self.presstimersound = time + 1;
            // 1st movement sound - going down
            sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
        }
        // Move pressure pad into down position
        self.ltime = time;
        self.state = STATE_MOVING;
        SUB_CalcMove (self.pos2, self.speed, func_pswitch_finished);
    }
    // Is the pressure plate DOWN?
    else if (self.state == STATE_DOWN) {
        // Keep waiting in the down position
        self.think = func_pswitch_return;
        self.nextthink = time + 0.2;
    }
};

//----------------------------------------------------------------------
void() func_pswitch_on = 
{
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    // Add to world pressure plate
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    
    // Reset state and frame back to default
    setorigin(self, self.pos1);
    self.state = STATE_UP;
    self.think = SUB_Null;
    self.frame = 0;
};

//----------------------------------------------------------------------
void() func_pswitch_off = 
{
    // Stop re-triggering OFF state
    if (self.estate == ESTATE_OFF) return;

    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    
    // Reset position and frame back to default
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
    setorigin(self, self.pos1);
    self.state = STATE_UP;
    self.think = SUB_Null;
    self.frame = 0;
};

//----------------------------------------------------------------------
void() func_pressureswitch =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Setup all empty/silent sound files first
    if (self.noise == "") self.noise = SOUND_EMPTY;

    // Default 0 = air (base style) button
    if (self.sounds == 0) self.noise = "buttons/airbut1.wav";
    else if (self.sounds == 1) self.noise = "buttons/switch21.wav";
    else if (self.sounds == 2) self.noise = "buttons/switch02.wav";
    else if (self.sounds == 3) self.noise = "buttons/switch04.wav";
    else if (self.sounds == 4) self.noise = SOUND_EMPTY;
    precache_sound (self.noise);
    
    if (self.noise1 == "") self.noise1 = self.noise;
    precache_sound (self.noise1);
    
    self.classtype = CT_FUNCPSWITCH;
    self.classgroup = CG_FUNCMOVER;
    self.bsporigin = TRUE;
    self.mdl = self.model;
    
    // Pressure switches always move up/down
    self.movedir = '0 0 -1';
    self.angles = '0 0 0';

    // Default parameters and states
    if (!self.speed) self.speed = 40;
    if (!self.lip) self.lip = 4;
    self.state = STATE_UP;

    self.health = self.max_health = 0;
    self.takedamage = DAMAGE_NO;
    self.wait = -1;

    // Reset all pressure switch variables
    self.presstype = self.presstimersound = 0;

    // Setup the pressure switch two different state positions
    // Got to add switch to world for bmodel size parameter
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);

    // Cannot have a pressure switch start off or disabled
    // with no targetname, how can it be actived!?!
    if (self.spawnflags & BUTTON_STARTDIS && self.targetname == "") {
        dprint("\b[PSWITCH]\b Starting DISABLED with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    
    if (self.spawnflags & ENT_STARTOFF && self.targetname == "") {
        dprint("\b[PSWITCH]\b Starting OFF with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_pswitch_on;
    self.estate_off = func_pswitch_off;
    self.estate_use = func_pswitch_use;

    // Check for starting off or default=on
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};
