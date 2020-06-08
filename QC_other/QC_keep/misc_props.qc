/*======================================================================
QUAKED misc_model (1 .5 .25) (-16 -16 -16) (16 16 16) x COLLISION x x x x STARTOFF x
MDL files that can be setup with specific frame/skin and animate groups
-------- KEYS --------
mdl : specify model to load, full path (progs/ad171/misc_candle1.mdl)  falls back to model if mdl not present
targetname : toggle state (use trigger ent for exact state)
angle  : facing angle (-1 = random position)
angles : 'pitch roll yaw' up/down, angle, tilt left/right 
ideal_yaw : = 1 Setup model with random Y axis rotation
pos1 : used for selection of frame(s) has several setups
  X=0, Y=0, Z=exact frame number
  X->Y, Z=0 Randomly pick a frame from the X,Y range
  X->Y, Z=-1 Animate between the X,Y range, can forward or backward setup
pos2 : used for the selection of skin(s) has several setups
  X=0, Y=0, Z=exact skin number
  X->Y, Z=0 Randomly pick a skin from the X,Y range
-------- SPAWNFLAGS --------
COLLISION : model bbox collision enabled
STARTOFF : Always Starts off and waits for trigger
-------- NOTES --------
MDL files that can be setup with specific frame/skin and animate groups

======================================================================*/
float MISC_STATIC = 1;

void() misc_model_loop =
{
    if (self.estate & ESTATE_BLOCK) return;
    
    self.height = self.height + self.lip;
    if (self.lip > 0 && self.height > self.pos1_y) self.height = self.pos1_x;
    else if (self.lip < 0 && self.height < self.pos1_y) self.height = self.pos1_x;
    
    self.frame = self.height;
    self.think = misc_model_loop;
    self.nextthink = time + self.speed;
};

//----------------------------------------------------------------------
void() misc_model_on =
{
    // Check if there is any spawn/on delay setup
    // Useful for delay spawning models on to platform/doors
    if (self.delay > 0) {
        // Feed back into this function
        self.think = misc_model_on;
        self.nextthink = time + self.delay;
        // The delay only works once
        self.delay = 0;
        return;
    }

    self.estate = ESTATE_ON;

    setmodel (self, self.mdl);
    // Restore model/size and check if collision needed
    if (self.spawnflags & MISC_COLLISION) {
        self.solid = SOLID_BBOX;
        setsize (self, self.mins , self.maxs);
    }
    else setsize (self, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(self, self.oldorigin + self.view_ofs);

    if (self.spawnflags & MISC_MOVEMENT) {
        // Turn misc model into an item (can work with plats/doors)
        self.movetype = MOVETYPE_TOSS;
        self.solid = SOLID_TRIGGER;
        setsize (self, VEC_ORIGIN, VEC_ORIGIN);
        self.velocity = '0 0 0';
        // Make sure the misc model starts on the ground
        self.origin_z = self.origin_z + 6;
        if (!droptofloor()) {
            dprint ("\n\b[Model]\b "); dprint (self.mdl);
            dprint (" stuck at ("); dprint (vtos(self.origin)); dprint (")\n");
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
            return;
        } 
        // Make sure no touch function is active
        self.touch = SUB_Null;
    }

    // Setup skin number (range or exact)    
    self.skin = self.pos2_z;

    // Check to see if we should make this static
    if (self.spawnflags & ENT_SPNSTATIC) {
        if (self.pos1_z)
            self.frame = self.pos1_z;

        makestatic(self);
    } else {
        // Check for manual animation loops
        if (self.pos1_z == -1) {
            self.height = self.pos1_x;
            if (self.pos1_x < self.pos1_y) self.lip = 1;
            else self.lip = -1;
            // Manually animate model
            misc_model_loop();
        } else self.frame = self.pos1_z;  // Exact frame number
    }
};

//----------------------------------------------------------------------
void() misc_model_off =
{
    // Turn off model/world interaction
    self.estate = ESTATE_OFF;
    self.model = string_null;
    self.solid = SOLID_NOT;
};

//----------------------------------------------------------------------
void() misc_model = {
    // Is the model defined using the noise key?
    if (!self.mdl) {
        if (self.model != "") {
            self.mdl = self.model; // if .model was accidentally used and mdl is not set, use .model instead
        } else {
            spawn_marker(self.origin, SPNMARK_YELLOW);
            remove(self);
            return;
        }
    }

    precache_model (self.mdl);
    self.classtype = CT_MISCMODEL;
    self.classgroup = CG_MISCENT;
    self.solid = SOLID_NOT;                // No world interaction
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.oldorigin = self.origin;        // Store for later
    
    // Override angles with mangle
    if (!self.mangle) {    
        // Setting the angle key in the editor to UP/DOWN = random rotation    
        if (self.angles_y < 0) self.angles_y = rint(random()*360);
        self.mangle = self.angles;            // Save for later
    }

    if (!self.speed) self.speed = 0.1;    // Manual animation tick speed
        
    // Has a frame range been defined?
    if (self.pos1_x != self.pos1_y) {
        // Make sure the range is the right way around
        // X has to be the lowest number of the two (X/Y)
        if (self.pos1_x > self.pos1_y) {
            self.frame_box = self.pos1_x;
            self.pos1_x = self.pos1_y;
            self.pos1_y = self.frame_box;
        }
        // Randomly pick frame number from a range? 
        // Work out random different and add to X base
        if (self.pos1_z == 0) {
            // Double check lower limit is not negative
            if (self.pos1_x < 0) self.pos1_x = 0;
            // Work out random range first and then add to base
            self.frame_box = fabs(self.pos1_y - self.pos1_x);
            self.pos1_z = self.pos1_x + rint(random()*self.frame_box);
            // Double check the frame is within the specified range
            if (self.pos1_z < self.pos1_x) self.pos1_z = self.pos1_x;
            if (self.pos1_z > self.pos1_y) self.pos1_z = self.pos1_y;
        }
        // Manual frame animation required
        else self.pos1_z = -1;
    } else {
        // If no exact frame bas specified, reset frame to default = 0
        if (self.pos1_z < 1) self.pos1 = '0 0 0';
    }
        
    // Has a skin range been defined?
    if (self.pos2_x != self.pos2_y) {
        self.pos2_z = rint( random() * fabs(self.pos2_y - self.pos2_x) );
    } else {
        // If no exact frame bas specified, reset frame to default = 0
        if (self.pos2_z < 1) self.pos2 = '0 0 0';
    }

    // Cannot have static and movement at the same time!
    if (self.spawnflags & MISC_MOVEMENT) {
        self.spawnflags = self.spawnflags - (self.spawnflags & ENT_SPNSTATIC);
    }

    // Setup random rotation
    if (self.ideal_yaw > 0 && query_configflag(SVR_ITEMROTATE) == FALSE ) 
        self.angles_y = rint(random()*359);
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_on = misc_model_on;
    self.estate_off = misc_model_off;
    if (self.spawnflags & ENT_STARTOFF) entity_state_off();
    else entity_state_on();
};

// Make quoth users happy!
void () mapobject_custom = {
    misc_model();
};

void () misc_misc = {
    misc_model();
};
