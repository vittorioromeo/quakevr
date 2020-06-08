/*======================================================================
 SECRET DOOR FUNCTIONS
======================================================================*/
float SECRET_OPEN_ONCE = 1;        // stays open
float SECRET_1ST_LEFT = 2;        // 1st move is left of arrow
float SECRET_1ST_DOWN = 4;        // 1st move is down from arrow
float SECRET_NO_SHOOT = 8;        // only opened by trigger
float SECRET_ALWAYS_SHOOT = 16;    // shootable even if targeted

/*======================================================================
/*QUAKED func_door_secret (0 .5 .8) ? OPEN_ONCE 1ST_LEFT 1ST_DOWN NO_SHOOT ALWAYS_SHOOT x STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
A Door (bmodel) with 2 states and multiple stages
-------- KEYS --------
targetname : = "" door opens with damage, != "" requires trigger to open
if using entity state system set ALWAYS_SHOOT spawnflags to fix this problem
target    : name of target(s) to trigger at the start opening sequence
angle     : determines the opening direction, use "360" for angle 0
speed     : movement speed (default = 50)
yaw_speed : 2nd movement speed (default = speed)
t_width   : First Distance, override Width to move back (or height if going down)
t_length  : Second Distance, override Length to move sideways
wait      : wait before returning (default = 5)
dmg       : damage to inflict when blocked. (default = 2)
message   : centerprint message when touched (removed when opened)
sounds    : 1 =  medieval 2 = metal 3 = base(def) 4 = Silent 5 = custom
locksounds: 0 = talktalk 1 = short rattle, 2 = medium rattle, 3 = long rattle
noise     : custom sound - door locked sound (targetname+message)
noise1    : custom sound - Open
noise2    : custom sound - Opening (looping)
noise3    : custom sound - Closed
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
ONCE_ONLY : Stays Open
1ST_LEFT  : 1st move is left of arrow
1ST_DOWN  : 1st move is down from arrow
NO_SHOOT  : only opened by trigger
ALWAYS_SHOOT : Always Shootable, even if targeted
STARTOFF  : Starts off and waits for trigger
-------- NOTES --------
A Door (bmodel) with 2 states and multiple stages

======================================================================*/

//----------------------------------------------------------------------
// Touch message ONLY! (every 2s repeat)
//----------------------------------------------------------------------
void() func_door_secret_touch =
{
    if (self.estate & ESTATE_BLOCK) return;
    if ( !(other.flags & FL_CLIENT) ) return;
    if (self.attack_finished > time) return;

    self.attack_finished = time + 2;
    
    if (self.message) {
        centerprint (other, self.message);
        sound (other, CHAN_BODY, self.noise, 1, ATTN_NORM);
    }
};

//----------------------------------------------------------------------
// FINAL move back (reset door ready for use again)
//----------------------------------------------------------------------
void () func_door_secret_done =
{
    self.state = STATE_BOTTOM;
    if (self.estate == ESTATE_OFF) return;
    // Add back takedamage if door is suppose to be shootable!
    if (self.spawnflags & SECRET_ALWAYS_SHOOT) self.takedamage = DAMAGE_YES;
    sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
};

//----------------------------------------------------------------------
void () func_door_secret_move6 =
{
    if (self.estate == ESTATE_OFF) return;
    sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
    SUB_CalcMove(self.dest0, self.yaw_speed, func_door_secret_done);
};

//----------------------------------------------------------------------
// Wait 1 second...
void () func_door_secret_move5 = 
{
    if (self.estate == ESTATE_OFF) return;
    self.nextthink = self.ltime + 1.0;
    self.think = func_door_secret_move6;
    sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
};

//----------------------------------------------------------------------
// Move backward...
void () func_door_secret_move4 =
{
    if (self.estate == ESTATE_OFF) return;
    sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
    SUB_CalcMove(self.dest1, self.yaw_speed, func_door_secret_move5);        
};

//----------------------------------------------------------------------
// Wait here until time to go back...
void () func_door_secret_move3 =
{
    if (self.estate == ESTATE_OFF) return;
    sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
    if (!(self.spawnflags & SECRET_OPEN_ONCE)) {
        self.nextthink = self.ltime + self.wait;
        self.think = func_door_secret_move4;
    }
};

//----------------------------------------------------------------------
// Start moving sideways w/sound...
void () func_door_secret_move2 =
{
    if (self.estate == ESTATE_OFF) return;
    sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
    SUB_CalcMove(self.dest2, self.speed, func_door_secret_move3);
};

//----------------------------------------------------------------------
// Wait after first movement...
void () func_door_secret_move1 = 
{
    if (self.estate == ESTATE_OFF) return;
    self.nextthink = self.ltime + 1.0;
    self.think = func_door_secret_move2;
    sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
};

//----------------------------------------------------------------------
void () func_door_secret_use =
{
    // Deal with STARTOFF functionality first
    if (self.spawnflags & ENT_STARTOFF) self.estate_on();
    else {
        // Block USE functionality if state wrong or door moving
        if (self.estate & ESTATE_BLOCK) return;
        if (self.state != STATE_BOTTOM) return;

        self.state = STATE_UP;
        self.takedamage = DAMAGE_NO;
        self.velocity = '0 0 0';

        // no more touch message and fire targets
        self.message = string_null;
        SUB_UseTargets();
        
        // Make a sound, wait a little...
        sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
        self.nextthink = self.ltime + 0.1;

        // Start FIRST move (t_width)
        SUB_CalcMove(self.dest1, self.speed, func_door_secret_move1);
        sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
    }
};

//----------------------------------------------------------------------
void() func_door_secret_killed =
{
    self.health = self.max_health;
    func_door_secret_use ();
};

//----------------------------------------------------------------------
void() func_door_secret_on =
{
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);

    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    self.velocity = '0 0 0';
    
    // trigger once doors?
    if ( self.spawnflags & SECRET_OPEN_ONCE ) {
        // if the door is open, do nothing
        if (self.state == STATE_TOP) self.max_health = 0;
    }
    else {
        // Reset health, state and frame back to default
        setorigin(self, self.dest0);
        self.state = STATE_BOTTOM;
        self.think = SUB_Null;
    }
        
    // reset health and damage trigger
    if (self.max_health > 0) {
        self.health = self.max_health;
        self.takedamage = DAMAGE_YES;
    }
};

//----------------------------------------------------------------------
void() func_door_secret_off =
{
    // Stop re-triggering OFF state
    if (self.estate == ESTATE_OFF) return;

    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    self.velocity = '0 0 0';
    
    // Reset health, position and frame back to default
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
    self.takedamage = DAMAGE_NO;
    self.think = SUB_Null;
    
    // trigger once door?
    if ( self.spawnflags & SECRET_OPEN_ONCE ) {
        // if the door is moving, go to final position
        if (self.state == STATE_TOP) setorigin(self, self.dest2);
    }
    else {
        // Reset back to the beginning
        setorigin(self, self.dest0);
        self.state = STATE_BOTTOM;
    }
};

//----------------------------------------------------------------------
void() func_door_secret_disable =
{
    // Block damage function
    self.takedamage = DAMAGE_NO;
};

//----------------------------------------------------------------------
void() func_door_secret_reset =
{
    // If the door is still OFF then leave it
    if (self.spawnflags & ENT_STARTOFF) return;
    
    // reset to original position
    setorigin(self, self.dest0);
    self.state = STATE_BOTTOM;
    self.attack_finished = 0;
    
    // Restore door to ON state
    self.estate = ESTATE_OFF;
    self.estate_on();
    self.think = SUB_Null;

};

//----------------------------------------------------------------------
void () func_door_secret_blocked =
{
    if (self.estate == ESTATE_OFF) return;
    if (time < self.attack_finished) return;
    self.attack_finished = time + 0.5;
    T_Damage (other, self, self, self.dmg, DAMARMOR);
};

//----------------------------------------------------------------------
void () func_door_secret =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors
    
    // Setup all empty/silent sound files first
    if (self.noise == "") self.noise = SOUND_EMPTY;
    if (self.noise1 == "") self.noise1 = SOUND_EMPTY;
    if (self.noise2 == "") self.noise2 = SOUND_EMPTY;
    if (self.noise3 == "") self.noise3 = SOUND_EMPTY;
        
    // Default = 3 - Medieval
    if (self.sounds == 0 || self.sounds == 3) {
        self.noise1 = "doors/basesec2.wav";
        self.noise2 = "doors/basesec1.wav";
        self.noise3 = "doors/basesec2.wav";
    }
    else if (self.sounds == 1) {
        self.noise1 = "doors/latch2.wav";
        self.noise2 = "doors/winch2.wav";
        self.noise3 = "doors/drclos4.wav";
    }
    else if (self.sounds == 2) {
        self.noise1 = "doors/airdoor2.wav";
        self.noise2 = "doors/airdoor1.wav";
        self.noise3 = "doors/airdoor2.wav";
    }

    // locked sound (targetname + message)
    if (self.noise == "") self.noise = SOUND_TALK;
    if (self.locksounds == 1) self.noise = "doors/rattle1.wav";
    else if (self.locksounds == 2) self.noise = "doors/rattle2.wav";
    else if (self.locksounds == 3) self.noise = "doors/rattle6.wav";

    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    precache_sound (self.noise3);

    self.classtype = CT_FUNCSECDOOR;
    self.classgroup = CG_FUNCMOVER;
    self.bsporigin = TRUE;
    self.mdl = self.model;
    // Save any angle value first (original behaviour)
    self.mangle = self.angles;

    // Allow for angles up/down by using movedir instead
    if (CheckZeroVector(self.angles)) self.angles = '0 360 0';
    SetMovedir ();    // Setup move direction base on angles

    if(!self.speed)    self.speed = 50;    // Movement speed (per phase)
    if(!self.yaw_speed) self.yaw_speed = self.speed;
    if (!self.wait) self.wait = 5;        // 5 seconds before closing
    if (!self.dmg) self.dmg = 2;
    self.state = STATE_BOTTOM;

    // Add sec door to world to work out movement
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    self.dest0 = self.origin;
    setorigin (self, self.origin);    
    setsize (self, self.mins , self.maxs);

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;
    
    if (self.message != "") self.touch = func_door_secret_touch;
    self.blocked = func_door_secret_blocked;

    // Damn annoying that the targetname is being used like this because
    // there could have been a better way of doing this functionality
    // == "" Secret_door can be damaged to be opened
    // != "" Secret door cannot be damaged and requires trigger to work
    //
    if (self.targetname == "" && !(self.spawnflags & SECRET_NO_SHOOT)) {
        self.spawnflags = self.spawnflags | SECRET_ALWAYS_SHOOT;
    }
    
    // Finally if the spawnflag is set, then let the shooting begin!
    if (self.spawnflags & SECRET_ALWAYS_SHOOT) {
        self.health = self.max_health = 1;
        self.takedamage = DAMAGE_YES;
        self.th_die = func_door_secret_killed;
    }

    // Calculate the sec door movement during spawn stage
    // This was originally done when the sec door was used
    self.lip = 1 - (self.spawnflags & SECRET_1ST_LEFT);    // 1 or -1
    // Original worked out v_forward based on angle/mangle
    // Using movedir instead so up/down directions can be used
    // makevectors(self.mangle);

    // Work out FIRST (t_width) direction to move
    if (!self.t_width) {
        if (self.spawnflags & SECRET_1ST_DOWN) self. t_width = fabs(v_up * self.size);
        else self. t_width = fabs(v_right * self.size);
    }
    if (self.spawnflags & SECRET_1ST_DOWN) self.dest1 = self.origin - v_up * self.t_width;
    else self.dest1 = self.origin + v_right * (self.t_width * self.lip);
        
    // Work out SECOND (t_length) direction to move
    // using pre-defined movedir instead of v_forward
    if (!self.t_length) self.t_length = fabs(self.movedir * self.size);
    self.dest2 = self.dest1 + self.movedir * self.t_length;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_door_secret_on;
    self.estate_off = func_door_secret_off;
    self.estate_use = func_door_secret_use;
    self.estate_reset = func_door_secret_reset;
    self.estate_disable = func_door_secret_disable;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};
