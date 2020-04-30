/*======================================================================
Client Map Variables
--------------------

Allows for binary (0/1) map variables to be stored with the map save
system (parm1-16) by the engine. The top 6 parm variables are never used
and can easily be turned into variables for the map to use.

======================================================================*/
float MAPV_BITS = 22;            // Bits used in each parm block
float MAPV_BLCK = 6;            // Blocks used (total parm's)
float MAPV_TOTAL = 132;            // Maximum amount of variables

float MAPV_QUERY = 1;            // Point entity - Query value
float MAPV_UPDATE = 2;            // Point entity - Update value
float MAPV_TOGGLE = 4;            // Point entity - Toggle value
float MAPV_MONSTERS = 32;        // Monsters can use bmodel trigger

float bitflags[24];                // Bitflag values
float mapvar[6];                // Map variables stored in savefile
void() trigger_bmodel_sounds;
void() trigger_bmodel_setup;
void() trigger_cdtrack_change;
void(entity e) trigger_skybox_change;

//======================================================================
/*QUAKED trigger_mapvar_query (.5 .7 1) (-8 -8 -16) (8 8 16) x
Trigger target(s) based on mapvar
-------- KEYS --------
targetname : trigger entity (works with entity state system)
count   : Map Variable to check (range checks)
target  : Map variable == 0 fire these target(s)
target2 : Map variable == 1 fire these target(s)
noise   : aframe switchable object
wait    : -1 = trigger once
-------- SPAWNFLAGS --------
-------- NOTES --------
Trigger target(s) based on mapvar
//-----------------------------------------------------------------------------
/*QUAKED trigger_mapvar_update (.5 .7 1) (-8 -8 -16) (8 8 16) x
Update the value of mapvar
-------- KEYS --------
targetname : trigger entity (works with entity state system)
count   : Map Variable to check (range checks)
cnt     : Value (0/1)
target  : Map variable == 0 fire these target(s)
target2 : Map variable == 1 fire these target(s)
noise   : aframe switchable object
wait    : -1 = trigger once
-------- SPAWNFLAGS --------
-------- NOTES --------
Update the value of mapvar
//-----------------------------------------------------------------------------
/*QUAKED trigger_mapvar_toggle (.5 .7 1) (-8 -8 -16) (8 8 16) x
Toggle the value of mapvar
-------- KEYS --------
targetname : trigger entity (works with entity state system)
count   : Map Variable to check (range checks)
target  : Map variable == 0 fire these target(s)
target2 : Map variable == 1 fire these target(s)
noise   : aframe switchable object
wait    : -1 = trigger once
-------- SPAWNFLAGS --------
-------- NOTES --------
Toggle the value of mapvar
======================================================================*/

// Setup the bitflag array (used in world.qc)
void() mapvar_setuparray =
{
    local float bitindex, bitpow;
    
    // Setup loop and first bit
    bitindex = 0; bitpow = 1;
    while (bitindex < 24) {
        bitflags[bitindex] = bitpow;
        bitpow = bitpow * 2;
        bitindex = bitindex + 1;
    }
};

//----------------------------------------------------------------------
// Reset all map variable banks to zero (used in client.qc)
void() mapvar_reset =
{
    local float mapv_bank;
    
    mapv_bank = 0;
    while (mapv_bank < MAPV_BLCK) {
        mapvar[mapv_bank] = 0;
        mapv_bank = mapv_bank + 1;
    }
};

//----------------------------------------------------------------------
// Read map variable from block/bits parms and return value
float(float mapv_indx) query_mapvar =
{
    local float mapv_bank, mapv_cell, mapv_bit;
    
    // Work out parm and bitflag index
    mapv_bank = floor(mapv_indx/MAPV_BITS);
    mapv_cell = mapv_indx - (mapv_bank*MAPV_BITS);
    // Error check return value
    if (mapv_bank < 0 || mapv_bank >= MAPV_BLCK) mapv_bank = 0;
    if (mapv_cell < 0 || mapv_cell >= MAPV_BITS) mapv_cell = 0;
    // Find bitflag mask value
    mapv_bit = mapvar[mapv_bank] & bitflags[mapv_cell];
    
    /* Debug info (not used anymore)
    dprint("Number ("); dprint(ftos(mapv_indx));
    dprint(") B/C ("); dprint(ftos(mapv_bank));
    dprint(" "); dprint(ftos(mapv_cell));
    dprint(") = ("); dprint(ftos(mapv_bit));
    dprint(")\n"); */

    if (mapv_bit > 0) return TRUE;
    else return FALSE;
};

//----------------------------------------------------------------------
// Update map variable from block/bits parms
void(float mapv_indx, float mapv_val) update_mapvar =
{
    local float mapv_bank, mapv_cell;
    
    // Work out parm and bitflag index
    mapv_bank = floor(mapv_indx/MAPV_BITS);
    mapv_cell = mapv_indx - (mapv_bank*MAPV_BITS);
    // Error check return value
    if (mapv_bank < 0 || mapv_bank >= MAPV_BLCK) mapv_bank = 0;
    if (mapv_cell < 0 || mapv_cell >= MAPV_BITS) mapv_cell = 0;
    
    // Remove any existing bitflag value
    mapvar[mapv_bank] = mapvar[mapv_bank] - (mapvar[mapv_bank] & bitflags[mapv_cell]);

    // Is the bit value TRUE?
    if (mapv_val == TRUE) {
        mapvar[mapv_bank] = mapvar[mapv_bank] | bitflags[mapv_cell];
    }
};

//----------------------------------------------------------------------
// Set a range of map variables (used in triggers.qc)
void(vector mapv_range) mapvar_range =
{
    local vector mapv_dest;
    local float mapv_swap, mapv_loop;
    
    // Remove any negative or fractions
    mapv_dest_x = fabs(rint(mapv_range_x));
    mapv_dest_y = fabs(rint(mapv_range_y));
    mapv_dest_z = fabs(rint(mapv_range_z));

    // Check for upper variable range
    if (mapv_dest_x >= (MAPV_BLCK*MAPV_BITS))
        mapv_dest_x = (MAPV_BLCK*MAPV_BITS) - 1;
    if (mapv_dest_y >= (MAPV_BLCK*MAPV_BITS))
        mapv_dest_y = (MAPV_BLCK*MAPV_BITS) - 1;
    if (mapv_dest_z > 1) mapv_dest_z = 1;

    // Check for reverse order on range
    if (mapv_dest_x > mapv_dest_y) {
        mapv_swap = mapv_dest_x;
        mapv_dest_x = mapv_dest_y;
        mapv_dest_y = mapv_swap;
    }

    // Count through range
    mapv_loop = mapv_dest_x;
    while (mapv_loop <= mapv_dest_y) {
        // Change map variable
        update_mapvar(mapv_loop, mapv_dest_z);
        mapv_loop = mapv_loop + 1;
    }
};

//----------------------------------------------------------------------
// Display the value of all map variable parm blocks
void() display_mapvar =
{
    local float loopbank, loopcell, loopspace, loopval;
    
    // Default return conditions (must be player and developer)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }
    // Has the map variable system been setup?
    if (!mapvar_cvar) {
        sprint(self,"\b[MAPVAR]\b System has not been setup yet!\n");
        return;
    }

    // Reset impulse and initialize variables
    self.impulse = 0;
    loopbank = loopcell = 0;

    dprint("\n\b[MAPVAR]\b Current System Values\n"); 
    dprint("--------------------------------------\n");
    while (loopbank < MAPV_BLCK) {
        dprint("Bank "); dprint(ftos(loopbank)); dprint(" - ");
        loopcell = 0; loopspace = 4;
        while (loopcell < MAPV_BITS) {
            loopval = query_mapvar((loopbank*MAPV_BITS)+loopcell);
            dprint(ftos(loopval));
            
            loopcell = loopcell+1;
            if (loopcell == loopspace) {
                dprint(" "); 
                loopspace = loopspace+4;
            }
        }
        dprint("\n");
        loopbank = loopbank+1;
    }
    dprint("--------------------------------------\n");
};

//----------------------------------------------------------------------
void() trigger_mapvar_base_use =
{
    // Block entity state exceptions
    if (self.estate & ESTATE_BLOCK) return;
    // Is the trigger blocked? (trigger_once)
    if (self.attack_finished > time) return;

    // UPDATE : Map Variable
    if (self.style == MAPV_UPDATE) {
        self.lefty = self.cnt;
        update_mapvar(self.count, self.cnt);
    }
    // TOGGLE : map variable
    else if (self.style == MAPV_TOGGLE) {
        self.lefty = query_mapvar(self.count);
        self.lefty = 1 - self.lefty;
        update_mapvar(self.count, self.lefty);
    }
    // QUERY : map variable
    else {
        self.lefty = query_mapvar(self.count);
    }

    // Check for On/Off target(s)
    if (self.lefty == FALSE) {
        if (self.target != "") trigger_strs(self.target, activator);
    }
    else {
        if (self.target2 != "") trigger_strs(self.target2, activator);
    }

    // Check for switchable bmodel
    if (self.noise != "") {
        // Check if entity exists first
        if (!self.oldenemy) 
            self.oldenemy = find(world, targetname, self.noise);
        // Only work with Bmodel entities (usually aframes)
        if (self.oldenemy.bsporigin) {
            // Flashing (ON) is +0frame ; Static (OFF) is +aframe
            if (self.lefty == TRUE) self.oldenemy.frame = 0;
            else self.oldenemy.frame = 1;
        }
    }

    // Check for fire once condition and wait timer
    if (self.wait < 0) self.attack_finished = LARGE_TIMER;
    else self.attack_finished = time + self.wait;
};

//----------------------------------------------------------------------
void() trigger_mapvar_base =
{
    // Check for any missing keys
    if (self.targetname == "") dprint("\b[MAPVAR]\b Missing targetname!\n");
    // Remove any negative or fractions
    self.count = fabs(rint(self.count));
    
    // Check for upper variable range
    if (self.count >= (MAPV_BLCK*MAPV_BITS))
        self.count = (MAPV_BLCK*MAPV_BITS) - 1;
    
    // Check for bitflag value range
    if (self.cnt < FALSE || self.cnt > TRUE) self.cnt = FALSE;
    
    // Setup Entity State functionality
    if (self.targetname != "") self.use = entity_state_use;
    self.estate_use = trigger_mapvar_base_use;
    self.estate = ESTATE_ON;
};

//----------------------------------------------------------------------
void() trigger_mapvar_query =
{
    self.classtype = CT_MAPVAR;
    self.style = MAPV_QUERY;
    trigger_mapvar_base();
};

//----------------------------------------------------------------------
void() trigger_mapvar_update =
{
    self.classtype = CT_MAPVAR;
    self.style = MAPV_UPDATE;
    trigger_mapvar_base();
};

//----------------------------------------------------------------------
void() trigger_mapvar_toggle =
{
    self.classtype = CT_MAPVAR;
    self.style = MAPV_TOGGLE;    
    trigger_mapvar_base();
};

//======================================================================
/*QUAKED trigger_mapvar_multiple (0.5 0 0.5) ? x x x x x MONSTERS STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
Touch trigger target(s) based on mapvar
------- KEYS --------
targetname : trigger entity (works with entity state system)
count   : Map Variable to check (range checks)
target  : Map variable == 0 fire these target(s)
message : Map variable == 0 message to display
target2 : Map variable == 1 fire these target(s)
message2: Map variable == 1 message to display
health  : Can be damaged instead of touched
wait    : time between re-triggering
delay   : delay before firing (after being triggered)
angle   : Facing Direction for trigger to work, use "360" for angle 0.
sounds  : 1=Secret,2=talk(def),3=switch,4=silent,5=custom,6=secret2
noise   : custom sound to play when triggered
-------- SPAWNFLAGS --------
MONSTER  : can be touched/triggered by monsters
STARTOFF : Requires trigger to activate
------- NOTES --------
Touch trigger target(s) based on mapvar
======================================================================*/
void() trigger_mapvar_multiple_fire = 
{
    if (self.attack_finished > time) return;    // Trigger once?

    // Find out map variable value
    self.lefty = query_mapvar(self.count);
    // Switch around target/message strings
    if (self.lefty == FALSE) {
        self.target = self.idle_sound;
        self.message = self.idle_soundcom;
    }
    else {
        self.target = self.idle_sound2;
        self.message = self.idle_soundcom2;
    }
    
    // Play the sound ON the trigger, NOT the activator
    if (self.noise != "") sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    // Original trigger_multiple activator behaviour
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
void() trigger_mapvar_multiple =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors

    // Remove excessive spawnflags not used
    self.lefty = 0;
    if (self.spawnflags & MAPV_MONSTERS) self.lefty = self.lefty | MAPV_MONSTERS;
    if (self.spawnflags & ENT_STARTOFF) self.lefty = self.lefty | ENT_STARTOFF;
    self.spawnflags = self.lefty;

    // Remove any negative or fractions
    self.count = fabs(rint(self.count));
    
    // Check for upper variable range
    if (self.count >= (MAPV_BLCK*MAPV_BITS))
        self.count = (MAPV_BLCK*MAPV_BITS) - 1;

    // Typical bmodel trigger setup
    trigger_bmodel_sounds();
    self.classtype = CT_MAPVAR;
    InitTrigger ();
    if (!self.wait) self.wait = 0.2;
    
    // Save all entity for later
    self.idle_sound = self.target;
    self.idle_sound2 = self.target2;
    self.idle_soundcom = self.message;
    self.idle_soundcom2 = self.message2;
    
    // Reset all strings to prevent SUB_UseTargets errors
    self.target = ""; self.target2 = "";
    self.message = ""; self.message2 = "";
    
    // Setup Entity State functionality
    self.estate_fire = trigger_mapvar_multiple_fire;
    trigger_bmodel_setup();    
};