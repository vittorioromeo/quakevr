/*======================================================================
 TRAP Sawblade
 * Originally based on code from Rogue Software
 * Extra code support from lunaran
======================================================================*/
float SAW_STARTON = 1;
float SAW_XAXIS = 16;
float SAW_REVERSE = 32;

/*======================================================================
/*QUAKED trap_sawbladex (0 .5 .8) (-18 -18 -18) (18 18 18) STARTON x x x x REVERSE STARTOFF x
Rotating Saw Blade
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
target   : Name of first path_corner to start at (instant move)
speed    : Speed to follow path, def=100
yaw_speed : Rotation speed for Saw Blade (def=180)
dmg      : Damage to do for each contact, def=4
waitmin  : Damage throttle for ON touch function, def=0.1s
height   : Damage throttle for OFF touch function, def=1s
lip      : Deceleration time; def=2s, =-1 instant stop
sounds   : 0=Silent, 1=Woodmill, 5=Custom sounds
noise    : custom sound - stopped
noise1   : custom sound - moving (looping)
-------- SPAWNFLAGS --------
STARTON  : Start moving straight away if targetname is used
REVERSE  : Start going backward through path_corner chain
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Rotating Saw Blade

======================================================================*/

void() trap_sawblade_touch =
{
    // Blade can still hurt if touched, except when OFF = not visible
    if (self.estate & ESTATE_OFF) return;
    // Double check if dead monster body!
    if (trigger_check_body(other,DEAD_EXPLODE)) return;
    if (other.health < 1 || other.takedamage == DAMAGE_NO) return;
    if (self.attack_finished > time) return;
    
    // --- lunaran -------------------
    // Different touch damage based on blade state
    if (self.state == STATE_ON) {
        if (self.waitmin > 0) self.attack_finished = time + self.waitmin;
        // Hit sounded added by Lunaran, added sound string check
        if (self.noise2 !="") sound (self, CHAN_AUTO, self.noise2, 1, 1);
    }
    else {
        if (self.height > 0) self.attack_finished = time + self.height;
    }
        
    // Instant death for monsters
    if (other.flags & FL_MONSTER)
        T_Damage (other, self, self, other.max_health + 100, DAMARMOR);
    else 
        // Small damage but can be lethal because of how often touch runs
        T_Damage (other, self, self, self.dmg, DAMARMOR);

    // Blood and gore at object location not sawblade
    SpawnMeatSpray ( other, other, crandom() * 200);
};

//----------------------------------------------------------------------
// New slowdown/stop function by lunaran
//----------------------------------------------------------------------
void() trap_sawblade_stopthink =
{
    float remaining;
    
    // If slow down time expired, make sure blade has stopped
    if (self.pain_finished <= time) {
        self.velocity = self.avelocity = '0 0 0';
        return;
    }
    
    remaining = (self.pain_finished - time) / self.lip;
    
    //----------------------------------------------------------------------
    // quit moving if we've hit the next pathcorner
    // it'd be cooler to continue on the track as we decelerate but this way
    // we don't have to muck about in the func_train code
    //----------------------------------------------------------------------
    if (self.rad_time && self.rad_time <= time) {
        self.velocity = '0 0 0';
        setorigin(self, self.finaldest);
        self.rad_time = 0;
    }
    
    // ramp down from original speed and spinrate over self.lip seconds
    if (CheckZeroVector(self.velocity) == FALSE)
        self.velocity = normalize(self.velocity) * self.speed * remaining;
    self.avelocity = normalize(self.avelocity) * self.yaw_speed * remaining;
    
    // Double tick time to smooth out slowdown
    self.nextthink = time + 0.05;
}

//----------------------------------------------------------------------
// New slowdown/stop function by lunaran
//----------------------------------------------------------------------
void() trap_sawblade_stop =
{
    float d, timeToPathCorner;
    
    // Play wind down stop sound
    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    
    // save the time we become completely still in pain_finished
    self.pain_finished = time + self.lip;
    
    // calculate when we'll hit the next pathcorner (if ever)
    // so we can stop there
    timeToPathCorner = vlen(self.origin-self.finaldest) / self.speed;
    
    // linear deceleration = stopping distance of exactly 1/2 speed*stoptime
    if (self.lip * 0.5 > timeToPathCorner ) {
        // linear deceleration = exponential time taken relative to distance
        d = mathlib_sqrt( 1 - ( 2 * timeToPathCorner / self.lip ) );
        d = self.lip * ( 1 - d );
        
        // save the time in rad_time
        self.rad_time = time + d;
    }
    self.think = trap_sawblade_stopthink;
    self.think();
};

//----------------------------------------------------------------------
void() trap_sawblade_use =
{
    // Block entity state OFF + DISABLE
    if (self.estate & ESTATE_BLOCK) return;

    // Toggle function
    if (self.state == STATE_OFF) {
        self.state = STATE_ON;
        // use special train resume function
        func_train_resume();
    }
    else {
        self.state = STATE_OFF;
        trap_sawblade_stop();
    }
};

//----------------------------------------------------------------------
void() trap_sawblade_setup =
{
    // Deal with START OFF functionality first
    if (self.spawnflags & ENT_STARTOFF) {
        // Remove start off flag and switch on entity
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
    }

    self.estate_on();                    // Switch on entity
    func_train_startcorner();            // Move to first corner
    self.estate_use = trap_sawblade_use;// Switch ON sawblade
    self.estate_fire = func_train_next;    // Move to next path corner
    self.state = STATE_OFF;                // sawblade is OFF, waiting
    
    // Sharp pointy edges!
    self.touch = trap_sawblade_touch;

    // Did the first corner (owner) exist?
    if (self.owner && self.spawnflags & SAW_STARTON) {
        self.nextthink = time + 0.1;
        self.think = self.estate_use;
    }
};

//----------------------------------------------------------------------
void() trap_sawblade_on =
{
    // Stop re-triggering ON state
    if (self.estate == ESTATE_ON) return;

    // Turn on model/world interaction
    self.estate = ESTATE_ON;                // Switch on entity
    self.solid = SOLID_TRIGGER;                // Keep on cutting!
    self.movetype = MOVETYPE_NOCLIP;        // Free movement
    setmodel (self, self.mdl);                // Show bmodel/model
    setsize (self, self.bbmins , self.bbmaxs);    // Use pre-defined size
    // Stop any current sounds, new sound will play in func_train function
    sound (self, CHAN_VOICE, SOUND_EMPTY, 1, ATTN_NORM);
    self.velocity = self.avelocity = '0 0 0';    // reset velocity/rotation
};

//----------------------------------------------------------------------
void() trap_sawbladey =
{
    // 0 = No sound, 1 = buzzzzz, 5 = custom
    if (self.sounds > 0) {
        if (self.sounds == 1) {
            self.noise = ("traps/sawblade_off.wav");
            self.noise1 = ("traps/sawblade_on.wav");
            self.noise2 = ("traps/sawstrike1.wav");    // lunaran
        }
        else if (self.sounds == 5) {
            if (self.noise == "") self.noise = SOUND_EMPTY;
            if (self.noise1 == "") self.noise1 = SOUND_EMPTY;
            if (self.noise2 == "") self.noise2 = SOUND_EMPTY;    // lunaran
        }    
        precache_sound (self.noise);
        precache_sound (self.noise1);
        precache_sound (self.noise2);    // lunaran
    }

    self.mdl = "progs/trap_sawblade.mdl";
    precache_model (self.mdl);
    
    self.classtype = CT_SAWBLADE;
    self.state = STATE_OFF;
    self.volume = 0.5;
    
    // Error - Check for first path corner target
    if (self.target == "") {
        dprint("\b[SAWBLADE]\b missing path corner target!\n");
    }

    // Setup orientiation and bounding box parameters
    // required for state on and touch function
    if (self.spawnflags & SAW_XAXIS) {
        self.angles = '0 0 0';
        self.bbmins = '-20 -2 -20';
        self.bbmaxs = '20 2 20';
    }
    else {
        self.angles = '0 90 0';
        self.bbmins = '-2 -20 -20';
        self.bbmaxs = '2 20 20';
    }
    
    if (!self.dmg) self.dmg = 4;            // Default touch damage
    if (!self.waitmin) self.waitmin = 0.1;    // Damage throttle ON state
    if (self.speed < 1) self.speed = 100;    // Default path speed
    self.wait = self.delay = 0;                // reset wait and delay
    
    // --- lunaran ----------------------
    if (!self.height) self.height = 1;         // Damage throttle OFF state
    if (!self.lip) self.lip = 2;            // Deceleration time, -1 for none
    if (self.lip < 0) self.lip = 0;            // Convert -1 switch to 0 time
    
    // Setup default rotation speed
    if (!self.yaw_speed) self.yaw_speed = 180;
    self.v_angle = '0 0 0';
    self.v_angle_x = self.yaw_speed;
    
    // Save spawn location
    self.dest0 = self.finaldest = self.origin;
    if (self.target == "") self.owner = self;
    self.bsporigin = FALSE;

    // Cannot have start ON and OFF together, remove OFF state!
    if (self.spawnflags & SAW_STARTON && self.spawnflags & ENT_STARTOFF)
        self.spawnflags = self.spawnflags - ENT_STARTOFF;

    // Setup train direction (switchable via path corners)
    if (self.spawnflags & SAW_REVERSE) self.direction = 1;
    else self.direction = 0;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = trap_sawblade_on;
    self.estate_off = func_train_off;
    self.estate_use = trap_sawblade_setup;
    self.estate_reset = func_train_reset;
    self.estate_disable = func_train_disable;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else {
        self.nextthink = time + 0.1;
        self.think = self.estate_use;
    }
};

//----------------------------------------------------------------------
void() trap_sawbladex =
{
    self.spawnflags = self.spawnflags | SAW_XAXIS;
    trap_sawbladey();
};