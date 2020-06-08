/*======================================================================
 TRAP PENDULUM
======================================================================*/
float PEND_MAXFRAME = 12;    // Frames 0-12
float PEND_ANIMGROUP = 13;    // Size of animation group
float PEND_MIDFRAME = 7;    // Fastest think frame

float PEND_REVERSE = 1;        // Start at frame 12 instead of 0
float PEND_ONESWING = 2;    // Do one swing cycle and stop (back+forth)
float PEND_SHORT = 16;        // Short pole version (128 instead of 192)
float PEND_XSWING = 32;        // Swing X axis (def=Y)

//----------------------------------------------------------------------
// The array is setup from world.qc before anything loads
//----------------------------------------------------------------------
vector pend_X[26], pend_Y[26], pend_short[26];

//----------------------------------------------------------------------
// As the pendulum swings the bounding box is adjusted around the blade
// Some swings have the bbox ahead of the blade to catch impacts better
// bbox values taken from the original pendulum from Rogue mission pack
// Using view_ofs because the model has been rebuilt/reskinned
//----------------------------------------------------------------------
void() pendulum_setuparray =
{
    local float loopc, loopd, tempx;
    local vector tbuffer;
    
    pend_Y[0] = '-8 -176 48';         pend_Y[1] = '8 -120 128';
    pend_Y[2] = '-8 -172 12';        pend_Y[3] = '8 -112 88';
    pend_Y[4] = '-8 -160 -22';        pend_Y[5] = '8 -96 50';
    pend_Y[6] = '-8 -138 -51';        pend_Y[7] = '8 -70 17';
    pend_Y[8] = '-8 -110 -72';        pend_Y[9] = '8 -38 -8';
    pend_Y[10] = '-8 -76 -83';        pend_Y[11] = '8 0 -23';
    pend_Y[12] = '-8 -40 -88';        pend_Y[13] = '8 40 -32';
    pend_Y[14] = '-8 0 -83';        pend_Y[15] = '8 76 -23';
    pend_Y[16] = '-8 38 -72';        pend_Y[17] = '8 100 -8';
    pend_Y[18] = '-8 70 -51';        pend_Y[19] = '8 138 17';
    pend_Y[20] = '-8 96 -22';        pend_Y[21] = '8 160 50';
    pend_Y[22] = '-8 112 12';        pend_Y[23] = '8 172 88';
    pend_Y[24] = '-8 120 48';        pend_Y[25] = '8 176 128';
    
    // 128 unit pendulum - Y Swing (default)
    pend_short[0] = '0 64 -108';    pend_short[1] = '0 56 -92';
    pend_short[2] = '0 56 -76';        pend_short[3] = '0 48 -60';
    pend_short[4] = '0 36 -52';        pend_short[5] = '0 16 -44';
    pend_short[6] = '0 0 -44';        pend_short[7] = '0 -16 -44';
    pend_short[8] = '0 -32 -52';    pend_short[9] = '0 -48 -60';
    pend_short[10] = '0 -56 -76';    pend_short[11] = '0 -56 -92';
    pend_short[12] = '0 -64 -108';
    
    loopc = loopd = 0;
    
    while (loopc < PEND_ANIMGROUP) {
        // Generate X axis vector offset from Y axis data (short pendulum)
        tbuffer = pend_short[loopc];        // Read vector offset
        tempx = tbuffer_x;                    // Tempoary store
        tbuffer_x = tbuffer_y;                // Swap around
        tbuffer_y = tempx;                    // Restore temp
        pend_short[PEND_ANIMGROUP + loopc] = tbuffer;
        
        // Generate X axis swing min/max vector from Y axis data
        tbuffer = pend_Y[loopd];            // min bbox
        tempx = tbuffer_x;
        tbuffer_x = tbuffer_y;
        tbuffer_y = tempx;
        pend_X[loopd] = tbuffer;        
        tbuffer = pend_Y[loopd+1];            // max bbox
        tempx = tbuffer_x;
        tbuffer_x = tbuffer_y;
        tbuffer_y = tempx;
        pend_X[loopd+1] = tbuffer;    
        
        loopc = loopc + 1;
        loopd = loopd + 2;
    }
};

//----------------------------------------------------------------------
// The original rogue pendulum adjusted the bbox every animation frame
// FTEQCC supports arrays and the previous method can now be done easier
// Also sets the nextthink time to adjust for swing momentum
//----------------------------------------------------------------------
void(float frame_seq) trap_pendsize = {
    self.cnt = frame_seq * 2;
    
    // Short pendulum needs a different offset each frame
    if (self.spawnflags & PEND_SHORT) {
        if (self.spawnflags & PEND_XSWING) self.view_ofs = pend_short[PEND_ANIMGROUP+frame_seq];
        else self.view_ofs = pend_short[frame_seq];
    }

    // X/Y swing axis have different bbox vectors
    if (self.spawnflags & PEND_XSWING) {
        self.pos1 = pend_X[self.cnt] + self.view_ofs;
        self.pos2 = pend_X[self.cnt + 1] + self.view_ofs;
    }
    else {
        self.pos1 = pend_Y[self.cnt] + self.view_ofs;
        self.pos2 = pend_Y[self.cnt + 1] + self.view_ofs;
    }
    setsize( self, self.pos1, self.pos2 );
    
    if (self.think) {
        // Change nextthink time to simulate the pendulum going fast/slow from momentum
        self.cnt = 0.05 + (fabs(PEND_MIDFRAME - (frame_seq + 1)) * 0.02);
        self.nextthink = time + self.cnt;
    }
};

//----------------------------------------------------------------------
// Pendulum animation frames
//----------------------------------------------------------------------
$frame pend_frame1 pend_frame2 pend_frame3 pend_frame4 pend_frame5 pend_frame6 pend_frame7
$frame pend_frame8 pend_frame9 pend_frame10 pend_frame11 pend_frame12 pend_frame13

// Stationary idle for beginning and ending
void() pend_idle1 =[ $pend_frame1, pend_idle1 ] {};
void() pend_idle21 =[ $pend_frame13, pend_idle21 ] {};

//----------------------------------------------------------------------
// Pendulum Swing (one cycle is 26 frames)
//----------------------------------------------------------------------
void() pend_swing1 =[ $pend_frame1, pend_swing2 ] {
    // Stop the pendulum if state is OFF / DISABLED
    if (self.estate & ESTATE_BLOCK) self.think = pend_idle1;
    else if (self.state == STATE_OFF) self.think = pend_idle1;
    else {
        self.count = self.count + 1;
        if (self.spawnflags & PEND_ONESWING && self.count > 2) {
            self.state = STATE_OFF;
            self.think = pend_idle1;
        }
        else {
            trap_pendsize(0);    // Move bbox to frame 0
            self.aflag = 1;        // Direction of pendulum blade (touch function)
        }
    }
};
void() pend_swing2 =[ $pend_frame2, pend_swing3 ] { trap_pendsize(1);};
void() pend_swing3 =[ $pend_frame3, pend_swing4 ] { trap_pendsize(2);}; 
void() pend_swing4 =[ $pend_frame4, pend_swing5 ] { trap_pendsize(3);
sound ( self, CHAN_VOICE, self.noise1, 0.5, ATTN_NORM);};
void() pend_swing5 =[ $pend_frame5,        pend_swing6 ] {trap_pendsize(4);};
void() pend_swing6 =[ $pend_frame6,        pend_swing7 ] {trap_pendsize(5);};
void() pend_swing7 =[ $pend_frame7,        pend_swing8 ] {trap_pendsize(6);};
void() pend_swing8 =[ $pend_frame8,        pend_swing9 ] {trap_pendsize(7);};
void() pend_swing9 =[ $pend_frame9,        pend_swing10 ] {trap_pendsize(8);};
void() pend_swing10 =[ $pend_frame10,    pend_swing11 ] {trap_pendsize(9);};
void() pend_swing11 =[ $pend_frame11,    pend_swing12 ] {trap_pendsize(10);};
void() pend_swing12 =[ $pend_frame12,    pend_swing13 ] {trap_pendsize(11);};
// One extra frame pause at end of sequence
void() pend_swing13 =[ $pend_frame13,    pend_swing21 ] {trap_pendsize(12);};

//----------------------------------------------------------------------
void() pend_swing21 =[ $pend_frame13,    pend_swing22 ] {
    // Stop the pendulum if state is OFF / DISABLED
    if (self.estate & ESTATE_BLOCK) self.think = pend_idle21;
    else if (self.state == STATE_OFF) self.think = pend_idle21;
    else {
        self.count = self.count + 1;
        if (self.spawnflags & PEND_ONESWING && self.count > 2) {
            self.state = STATE_OFF;
            self.think = pend_idle21;
        }
        else {
            trap_pendsize(12);    // Move bbox to frame 12
            self.aflag = -1;        // Direction of pendulum blade (touch function)
        }
    }
};
void() pend_swing22 =[ $pend_frame12,    pend_swing23 ] {trap_pendsize(11);};
void() pend_swing23 =[ $pend_frame11,    pend_swing24 ] {trap_pendsize(10);};
void() pend_swing24 =[ $pend_frame10,    pend_swing25 ] {trap_pendsize(9);
sound ( self, CHAN_VOICE, self.noise1, 0.5, ATTN_NORM);};
void() pend_swing25 =[ $pend_frame9,    pend_swing26 ] {trap_pendsize(8);};
void() pend_swing26 =[ $pend_frame8,    pend_swing27 ] {trap_pendsize(7);};
void() pend_swing27 =[ $pend_frame7,    pend_swing28 ] {trap_pendsize(6);};
void() pend_swing28 =[ $pend_frame6,    pend_swing29 ] {trap_pendsize(5);};
void() pend_swing29 =[ $pend_frame5,    pend_swing30 ] {trap_pendsize(4);};
void() pend_swing30 =[ $pend_frame4,    pend_swing31 ] {trap_pendsize(3);};
void() pend_swing31 =[ $pend_frame3,    pend_swing32 ] {trap_pendsize(2);};
void() pend_swing32 =[ $pend_frame2,    pend_swing33 ] {trap_pendsize(1);};
// One extra frame pause at end of sequence
void() pend_swing33 =[ $pend_frame1,    pend_swing1 ] {trap_pendsize(0);};

//----------------------------------------------------------------------
// if something has touched the pendulum, damage and bounce away
//----------------------------------------------------------------------
void() trap_pendulum_touch =
{
    // Blade can still hurt if touched, except when OFF = not visible
    if (self.estate & ESTATE_OFF) return;
    // Double check if dead monster body!
    if (trigger_check_body(other,DEAD_EXPLODE)) return;
    if (other.health < 1 || other.takedamage == DAMAGE_NO) return;
    if (self.attack_finished > time) return;
    if (self.waitmin > 0) self.attack_finished = time + self.waitmin;

    // only play hit sound once per second
    if (self.pain_finished < time) {
        sound ( self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
        self.pain_finished = time + 1;
    }
    
    // Instant death for monsters, they can get knocked into wierd locations
    if (other.flags & FL_MONSTER) 
        T_Damage (other, self, self, other.max_health + 100, DAMARMOR);
    else 
        // Small damage but can be lethal because of how often touch runs
        T_Damage (other, self, self, self.dmg, DAMARMOR);

    // Fling the player/object away from blade
    // Setting aflag in animation frame to specify direction
    if (self.spawnflags & PEND_XSWING) other.velocity_x = self.aflag * 250;
    else other.velocity_y = self.aflag * 250;
    other.velocity_z = 200;        // Little upward lift
    
    // Blood and gore at object location not pendulum
    SpawnMeatSpray (other, other, crandom() * 200);
};

//----------------------------------------------------------------------
void() trap_pendulum_fire =
{
    // Swing counter
    self.count = 0;
    // Make sure starting frame is start or finish
    if (self.frame != 0 && self.frame != PEND_MAXFRAME)
        self.frame = self.frame_override;
    
    // Setup next frame to update swing
    if (self.frame == 0) self.think = pend_swing1;
    else self.think = pend_swing13;
    self.nextthink = time + 0.01 + self.wait;
};

//----------------------------------------------------------------------
void() trap_pendulum_use =
{
    // Deal with STARTOFF functionality first
    if (self.spawnflags & ENT_STARTOFF) {
        // Remove spawnflag and switch ON entity
        self.spawnflags = self.spawnflags - ENT_STARTOFF;
        self.state = STATE_OFF;    // use toggle function
        self.estate_on();        // switch on
    }

    // Block USE functionality if state wrong
    if (self.estate & ESTATE_BLOCK) return;

    // Toggle state of pendulum to start/stop
    if (self.state == STATE_OFF) {
        self.state = STATE_ON;
        trap_pendulum_fire();
    }
    else self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() trap_pendulum_on =
{
    // Turn on model/world interaction
    self.estate = ESTATE_ON;    
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_NONE;
    setmodel (self, self.mdl);
    setsize (self, self.mins , self.maxs);
    trap_pendsize(self.frame);
};

//----------------------------------------------------------------------
void() trap_pendulum_off =
{
    self.estate = ESTATE_OFF;
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel (self, "");
    self.frame = self.frame_override;
    self.state = STATE_OFF;
    self.think = SUB_Null;
};

//----------------------------------------------------------------------
void() trap_pendulum_disable =
{
    self.state = STATE_OFF;
};

//----------------------------------------------------------------------
void() trap_pendlong =
{
    // Short/long version of the model
    if (self.spawnflags & PEND_SHORT) self.mdl = "progs/ad171/trap_pendshort.mdl";
    else self.mdl = "progs/ad171/trap_pendlong.mdl";
    precache_model (self.mdl);
    
    self.view_ofs = '0 0 -108';    // Offset bbox vectors (new model different location)
    self.oldorigin = '0 0 0';    // Entity is correct location

    self.noise1 = "traps/pend_swing.wav";
    self.noise2 = "traps/pend_hit.wav";
    precache_sound (self.noise1);
    precache_sound (self.noise2);

    self.classtype = CT_PENDULUM;
    if (self.dmg == 0) self.dmg = 5;    // Default touch damage
    if (self.dmg < 0) self.dmg = 0;        // Use -1 for no damage option
    if (self.wait <= 0) self.wait = 0;    // Default starting delay
    if (!self.waitmin) self.waitmin = 0.5;    // Damage throttle
    self.state = STATE_OFF;

    // Error - Check for targetname for trigger event
    if (self.targetname == "") {
        dprint("\b[PENDULUM]\b missing targetname, cannot be triggered!\n");
    }
    
    // Rotate model and swing on X axis instead
    if (self.spawnflags & PEND_XSWING) self.angles = '0 270 0';
    else self.angles = '0 0 0';

    // Start the opposite way around (frame 13 instead of 1)
    // This also affects one swing parameter
    if (self.spawnflags & PEND_REVERSE) self.frame_override = PEND_MAXFRAME;
    else self.frame_override = 0;
    self.frame = self.frame_override;

    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = trap_pendulum_on;
    self.estate_off = trap_pendulum_off;
    self.estate_use = trap_pendulum_use;
    self.estate_disable = trap_pendulum_disable;
    self.touch = trap_pendulum_touch;
    if (self.spawnflags & ENT_STARTOFF) self.estate_off();
    else self.estate_on();
};

//----------------------------------------------------------------------
void() trap_pendlongx =
{
    self.spawnflags = self.spawnflags | PEND_XSWING;
    trap_pendlong();
}

//----------------------------------------------------------------------
void() trap_pendshort =
{
    self.spawnflags = self.spawnflags | PEND_SHORT;
    trap_pendlong();
}

//----------------------------------------------------------------------
void() trap_pendshortx =
{
    self.spawnflags = self.spawnflags | PEND_SHORT;
    self.spawnflags = self.spawnflags | PEND_XSWING;
    trap_pendlong();
}

//-----------------------------------------------------------------------------
/*QUAKED trap_pendlong (0.5 0.75 0) (-8 -192 -24) (8 0 24) REVERSE ONESWING x x x x STARTOFF x
Long (192 units) Pendulum (From Rogue mission pack)
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
dmg      : Damage to do for each contact, def=5
waitmin  : Damage throttle to touch function, def=0.5s
wait     : amount of time (seconds) before starting swinging. def=0
-------- SPAWNFLAGS --------
REVERSE  : Starts at frame 12 instead 0
ONESWING : Will do a single swing (back + forth) when triggered
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Long (192 units) Pendulum (From Rogue mission pack)
*/

//-----------------------------------------------------------------------------
/*QUAKED trap_pendlongx (0.5 0.75 0) (-192 -8 -24) (0 8 24) REVERSE ONESWING x x x x STARTOFF x
Long (192 units) Pendulum working on X axis only
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
dmg      : Damage to do for each contact, def=5
waitmin  : Damage throttle to touch function, def=0.5s
wait     : amount of time (seconds) before starting swinging. def=0
-------- SPAWNFLAGS --------
REVERSE  : Starts at frame 12 instead 0
ONESWING : Will do a single swing (back + forth) when triggered
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Long (192 units) Pendulum working on X axis only
*/

//-----------------------------------------------------------------------------
/*QUAKED trap_pendshort (0.5 0.75 0) (-8 -192 -24) (8 0 24) REVERSE ONESWING x x x x STARTOFF x
Short (128 units) Pendulum
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
dmg      : Damage to do for each contact, def=5
waitmin  : Damage throttle to touch function, def=0.5s
wait     : amount of time (seconds) before starting swinging. def=0
-------- SPAWNFLAGS --------
REVERSE  : Starts at frame 12 instead 0
ONESWING : Will do a single swing (back + forth) when triggered
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Short (128 units) Pendulum
*/

//-----------------------------------------------------------------------------
/*QUAKED trap_pendshortx (0.5 0.75 0) (-192 -8 -24) (0 8 24) REVERSE ONESWING x x x x STARTOFF x
Short (128 units) Pendulum working on X axis only
-------- KEYS --------
targetname : toggle state (use trigger ent for exact state)
dmg      : Damage to do for each contact, def=5
waitmin  : Damage throttle to touch function, def=0.5s
wait     : amount of time (seconds) before starting swinging. def=0
-------- SPAWNFLAGS --------
REVERSE  : Starts at frame 12 instead 0
ONESWING : Will do a single swing (back + forth) when triggered
STARTOFF : Starts off and waits for trigger
-------- NOTES --------
Short (128 units) Pendulum working on X axis only
*/
