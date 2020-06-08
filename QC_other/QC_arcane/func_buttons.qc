/*======================================================================
 BUTTON FUNCTIONS
======================================================================*/
float BUTTON_STARTDIS = 32;            // Button starts in disabled state
//----------------------------------------------------------------------

/*QUAKED func_button (0 .5 .8) ? x x x x x STARTDIS STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Button (bmodel) with two states
-------- KEYS --------
targetname : trigger entity (works with entity state system)
target : name of target(s) to trigger at the start of the wait phase
angle  : determines the move direction, use "360" for angle 0
speed  : Move Speed (def=40)
wait   : wait before returning (def=1, -1 = never return)
delay  : delay before button triggers
lip    : lip remaining at end of move. (def=4)
health : Can be damaged instead of touched
sounds : 0=steam metal, 1=wooden, 2=metallic, 3=in-out, 4=Silent, 5=Custom
noise  : custom sound - 1st movement
noise1 : custom sound - 2nd movement ('buttons/switch21.wav')
message: Centerprint Message to display when triggered
estate : Override starting entity state 1=ON, 2=OFF, 4=DISABLE
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
STARTDIS : Starts disabled and waits for trigger
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Button (bmodel) with two states
*/

void() func_button_return;
//----------------------------------------------------------------------
void() func_button_wait =
{
    self.state = STATE_TOP;
    if (self.estate & ESTATE_BLOCK) return;
    self.nextthink = self.ltime + self.wait;
    self.think = func_button_return;
    activator = self.enemy;
    SUB_UseTargets();
    self.frame = 1;            // use alternate textures
};

//----------------------------------------------------------------------
void() func_button_done =
{ 
    self.frame = 0;                    // use normal textures
    self.state = STATE_BOTTOM; 
    if (self.health) self.takedamage = DAMAGE_YES;
};

//----------------------------------------------------------------------
void() func_button_return =
{
    self.state = STATE_DOWN;
    if (self.estate & ESTATE_BLOCK) return;
    if (self.noise1) sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
    SUB_CalcMove (self.pos1, self.speed, func_button_done);
};

//----------------------------------------------------------------------
void() func_button_fire =
{
    if (self.estate & ESTATE_BLOCK) return;
    if (self.state == STATE_UP || self.state == STATE_TOP) return;

    self.state = STATE_UP;
    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    SUB_CalcMove (self.pos2, self.speed, func_button_wait);
};

//----------------------------------------------------------------------
void() func_button_use =
{
    // Deal with DELAY functionality first
    if (self.spawnflags & ENT_STARTOFF) self.estate_on();
    else {
        // Block USE functionality if state wrong
        if (self.estate & ESTATE_BLOCK) return;

        self.enemy = other;
        func_button_fire();
    }
};

//----------------------------------------------------------------------
// Default state for buttons, touch trigger
//----------------------------------------------------------------------
void() func_button_touch =
{
    if ( !(other.flags & FL_CLIENT) ) return;
    self.enemy = other;
    func_button_fire();
};

//----------------------------------------------------------------------
// Only active if health set on trigger, touch function disabled
//----------------------------------------------------------------------
void() func_button_killed =
{
    self.enemy = damage_attacker;
    self.health = self.max_health;
    self.takedamage = DAMAGE_NO;    // wil be reset upon return
    func_button_fire();
};

//----------------------------------------------------------------------
// do nothing, just don't come all the way back out
//----------------------------------------------------------------------
void() func_button_blocked = { };

//----------------------------------------------------------------------
void() func_button_on = 
{
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    // No longer need this spawnflag, remove it
    self.spawnflags = self.spawnflags - (self.spawnflags & ENT_STARTOFF);
    
    self.estate = ESTATE_ON;
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    
    // Reset health, state and frame back to default
    setorigin(self, self.pos1);
    self.state = STATE_BOTTOM;
    self.think = SUB_Null;
    self.frame = 0;

    // reset health and damage trigger
    if (self.max_health) {
        self.health = self.max_health;
        self.th_die = func_button_killed;
        self.takedamage = DAMAGE_YES;
    }
};

//----------------------------------------------------------------------
void() func_button_off = 
{
    // Stop re-triggering OFF state
    if (self.estate == ESTATE_OFF) return;

    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    
    // Reset health, position and frame back to default
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
    self.takedamage = DAMAGE_NO;
    setorigin(self, self.pos1);
    self.state = STATE_BOTTOM;
    self.think = SUB_Null;
    self.frame = 0;
};

//----------------------------------------------------------------------
void() func_button_disable =
{
    // Block damage function and show alternative texture
    self.estate = ESTATE_DISABLE;
    self.takedamage = DAMAGE_NO;
    self.frame = 1;
    // Make sure the button is moved to finished state
    SUB_CalcMove (self.pos2, self.speed, func_button_wait);
};

//=============================================================================
void() func_button =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Setup all empty/silent sound files first
    if (self.noise == "") self.noise = SOUND_EMPTY;

    // Default 0 = air button
    if (self.sounds == 0) self.noise = "buttons/airbut1.wav";
    else if (self.sounds == 1) self.noise = "buttons/switch21.wav";
    else if (self.sounds == 2) self.noise = "buttons/switch02.wav";
    else if (self.sounds == 3) self.noise = "buttons/switch04.wav";
    else if (self.sounds == 4) self.noise = SOUND_EMPTY;
    precache_sound (self.noise);
    
    if (self.noise1 != "") precache_sound (self.noise1);
    
    self.classtype = CT_FUNCBUTTON;
    self.classgroup = CG_FUNCMOVER;
    self.bsporigin = TRUE;
    self.mdl = self.model;
    SetMovedir ();    // Setup move direction base on angles

    // Default parameters and states
    if (!self.speed) self.speed = 40;
    if (!self.wait) self.wait = 1;
    if (!self.lip) self.lip = 4;
    self.state = STATE_BOTTOM;

    // Setup the buttons two different state positions
    // Got to add button to world for bmodel size parameter
    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.mdl);
    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);

    // Cannot have a button start off or disabled
    // with no targetname, how can it be actived!?!
    if (self.spawnflags & BUTTON_STARTDIS && self.targetname == "") {
        dprint("\b[BUTTON]\b Starting DISABLED with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    
    if (self.spawnflags & ENT_STARTOFF && self.targetname == "") {
        dprint("\b[BUTTON]\b Starting OFF with no targetname!\n");
        self.oldorigin = bmodel_origin(self);
        spawn_marker(self.oldorigin, SPNMARK_YELLOW);
        entity_hide(self);
        return;
    }    

    // Check for spawning conditions (nightmare, coop)
    // Needs to exist after entity has been added to work for BSPorigin
    if (check_nightmare() == TRUE) return;
    if (check_coop() == TRUE) return;

    // Can either shoot a button or touch it, never both
    if (self.health) {
        self.max_health = self.health;
        self.th_die = func_button_killed;
        self.takedamage = DAMAGE_YES;
    }
    // Important OR condition (touching only)
    else self.touch = func_button_touch;
    self.blocked = func_button_blocked;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = func_button_on;
    self.estate_off = func_button_off;
    self.estate_use = func_button_use;
    self.estate_disable = func_button_disable;

    // Check for the easy way to start a button disabled
    if (self.spawnflags & BUTTON_STARTDIS) self.estate_disable();
    // Check for starting override for entity state
    else if (self.estate & ESTATE_LOWER) {
        if (self.estate == 1) self.estate_on();
        else if (self.estate == 2) {
            // Make sure off function works, start with on state
            self.estate = ESTATE_ON;
            self.estate_off();
        }
        else if (self.estate == 4) self.estate_disable();
    }
    else {
        // Check for starting off or default=on
        if (self.spawnflags & ENT_STARTOFF) self.estate_off();
        else self.estate_on();
    }
};

