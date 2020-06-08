/*======================================================================
 MINIONS FUNCTIONS

 Minion Support functions
  Setup and update minion counters centralized to reduce errors
  monster_death_precheck remove from the counter (dying event)
  minion_voreling, minion_spider add to the counter (spawn event)
  
======================================================================*/

.float        minion_baseattack;    // Block base attack within melee range
.float        minion_count;        // Current active amount of minions
.float        minion_maxcount;    // Maximum amount of active minions
.float        minion_trigger;        // Spawn counter for trigger event
.float        minion_maxtrigger;    // Spawn total for trigger event
.string        miniontarget;        // targetname string for trigger event
.float        minion_throwspeed;  // Starting forward speed of ball
.float        minion_throwextra;    // Extra random forward speed of ball
.float        minion_throwside;    // Extra sideways momentum (+/-)
.entity        minion_test;        // Test entity for minion collision
float MIN_MAXACTIVE = 5;        // Maximum amount active at once
float MIN_MAXTRIGGER = 5;        // How many spawns before triggering
float MIN_THROWSPD = 200;        // Monster jump speed
float MIN_THROWRND = 50;        // Some randomness
float MIN_THROWSIDE = 50;        // Sideways movement (+/-)

void(vector org, vector dir, vector avel, float proj_type) Launch_Grenade;

//----------------------------------------------------------------------
void() setup_minionsupport =
{
    // Reset all counters
    self.minion_active = TRUE;
    self.minion_count = 0;
    self.minion_trigger = 0;

    // Setup default max counters (check for existing values)
    if (!self.minion_maxcount) self.minion_maxcount = MIN_MAXACTIVE;
    if (!self.minion_maxtrigger) self.minion_maxtrigger = MIN_MAXTRIGGER;
    
    // Chance to fire original (base) attack of monster
    if (!self.minion_baseattack) self.minion_baseattack = TRUE;

    // Throw speed of minion ball/egg
    if (!self.minion_throwspeed) self.minion_throwspeed = MIN_THROWSPD;
    if (!self.minion_throwextra) self.minion_throwextra = MIN_THROWRND;
    if (!self.minion_throwside) self.minion_throwside = MIN_THROWSIDE;
};

//----------------------------------------------------------------------
void(entity msource, float countupd) update_minioncount =
{
    if (!msource.minion_active) return;
    // Add to the active minion counters (check range limits)
    msource.minion_count = msource.minion_count + countupd;
    if (msource.minion_count < 0) msource.minion_count = 0;
    if (msource.minion_count > msource.minion_maxcount)
        msource.minion_count = msource.minion_maxcount;

    // Check for trigger events (ignore death triggers)
    if (countupd > 0) {
        msource.minion_trigger = msource.minion_trigger + countupd;
        if (msource.minion_trigger >= msource.minion_maxtrigger) {
            msource.minion_trigger = 0;
            // Check for trigger event string?
            if (msource.miniontarget != "") {
                trigger_strs(msource.miniontarget, msource);
            }
        }
    }
};

//----------------------------------------------------------------------
// Check if anymore minions can be spawned at the moment
//----------------------------------------------------------------------
float(entity msource) query_minionactive =
{
    if (!msource.minion_active) return NEGATIVE;
    if (msource.minion_count < msource.minion_maxcount) return TRUE;
    else return FALSE;
};

//======================================================================
// Check if there is space to spawn a minion
// 
// Version 1 - use a touch trigger to detect players
// Version 2 - use a findradius command to find players
//======================================================================
void() delete_minionspace = { if (self.minion_test) entity_remove(self.minion_test,0.1); }
void() touch_minionspace = { if (other.flags & FL_CLIENT) self.aflag = TRUE; };

//----------------------------------------------------------------------
// Touch trigger version for detecting client/players too close
//----------------------------------------------------------------------
void(vector org, vector minspace, vector maxspace) setup_minionspace =
{
    // Create a touch trigger if one does not exist
    if (!self.minion_test) self.minion_test = spawn();
    // reset all parameters
    self.minion_test.movetype = MOVETYPE_NONE;
    self.minion_test.solid = SOLID_TRIGGER;
    setmodel(self.minion_test, MODEL_EMPTY);
    self.minion_test.skin = self.minion_test.frame = 0;
    self.minion_test.flags = 0;

    // Move touch trigger to correct location
    setorigin(self.minion_test, org);
    setsize (self.minion_test, minspace, maxspace);
    // Enable touch function and reset collision flag
    self.minion_test.touch = touch_minionspace;
    self.minion_test.aflag = FALSE;
};

float() test_minionspace =
{
    // Quick exit if no touch entity exists
    if (!self.minion_test) return FALSE;

    // Basic world collision test for origin/min/max of spawn
    if (pointcontents(self.minion_test.origin) != CONTENT_EMPTY) return FALSE;
    if (pointcontents(self.minion_test.origin+'-16 -16 -24') != CONTENT_EMPTY) return FALSE;
    if (pointcontents(self.minion_test.origin+'16 16 40') != CONTENT_EMPTY) return FALSE;
    
    // Switch off touch function and check for collision
    self.minion_test.touch = SUB_Null;
    if (self.minion_test.aflag == TRUE) return FALSE;
    else return TRUE;
};

//----------------------------------------------------------------------
// Findradius version for detecting client/players too close
//----------------------------------------------------------------------
float(vector org) find_minionspace =
{
    local entity minstuff;
    local float foundclient;

    // Basic world collision test for origin/min/max of spawn
    if (pointcontents(org) != CONTENT_EMPTY) return FALSE;
    if (pointcontents(org+'-16 -16 -24') != CONTENT_EMPTY) return FALSE;
    if (pointcontents(org+'16 16 40') != CONTENT_EMPTY) return FALSE;
    
    foundclient = TRUE;
    // The player can cover crazy amount of distance very fast
    // Extend radius out further to catch running players
    minstuff = findradius(org, MONAI_RANGESUMMON);
    while (minstuff) {
        if (minstuff.flags & FL_CLIENT) {
            foundclient = FALSE;
            minstuff = world;
        }
        minstuff = minstuff.chain;
    }
    return foundclient;
};

/*======================================================================
 MINION EGGS
 * Spawns a projecile (egg) at the player
 * Wait for egg to bounce and settle
 * Spawn (rotating) minion inside egg and then explode
======================================================================*/
void(vector minion_org, entity minion_targ) minion_spider;
void(vector minion_org, entity minion_targ) minion_scorpion;
void(vector minion_org, entity minion_targ) minion_voreling;

//----------------------------------------------------------------------------
void() Explode_Egg =
{
    // Remove egg and throw gibs up as minion grows
    setmodel (self, "");
    if (self.classgroup == CT_MONWRAITH) self.gib1mdl = MODEL_PROJ_WSHELL; 
    else if (self.classgroup == CT_MONSHAL) self.gib1mdl = MODEL_PROJ_SSHELL; 
    self.gib1frame = 9;
    self.max_health = MON_GIBFOUNTAIN;
    ThrowGib(11,rint(2+random()*4));
    
    // Safely remove egg
    self.think = SUB_Remove;
    self.nextthink = time + 0.1;
};    

//----------------------------------------------------------------------
// Once the egg has landed via Grenade Touch function
// The minion is spawned and the egg is set to explode
//----------------------------------------------------------------------
void() Hatch_Egg =
{
    // Block this functions
    if (self.state == STATE_DISABLED) return;
    // Has the egg been marked for removal?
    if (self.touchedvoid) return;

    // Its possible for the egg to get stuck, setup death timer
    if (!self.gibbed) {
        self.gibbed = TRUE;
        self.attack_finished = time + 6;
    }
    // Egg has been stuck for too long, destroy it
    if (self.attack_finished < time) {
        self.state = STATE_DISABLED;
        self.think = Explode_Egg;
        self.nextthink = time + 0.1;
        return;
    }

    // Turn off touch function and any movement
    self.touch = SUB_Null;
    self.movetype = MOVETYPE_NONE;
    self.takedamage = DAMAGE_NO;
    self.velocity = '0 0 0';
    
    // Check if the egg can hatch? turn on collision
    // setup new bounds and extend upwards for testing
    setsize (self, VEC_HULLT_MIN, VEC_HULLT_MAX);
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_TOSS;
    self.oldorigin = self.origin;
    self.origin_z = self.origin_z + 8;
    droptofloor();
    
    // Can the egg hatch yet? Solid content?
    if (!walkmove(0,0) || pointcontents(self.origin) == CONTENT_SOLID) {
        self.origin = self.oldorigin;
        self.think = Hatch_Egg;
        self.nextthink = time + 1 + random();
        self.solid = SOLID_NOT;        // turn off again
        return;
    }
    
    // Setup egg ready for minion
    self.state = STATE_DISABLED;

    // Setup spawn location for minion
    traceline (self.origin+'0 0 128', self.origin-'0 0 256', TRUE, world);
    self.oldorigin = trace_endpos + '0 0 32';
    
    // Spawn minion inside of egg model
    // self.owner is re-assigned to minion to avoid shell gibs interacting
    if (self.owner.classtype == CT_MONWRAITH) {
        if (self.owner.spawnflags & MON_WRAITH_SCORPIONS) minion_scorpion(self.oldorigin, self.enemy);
        else minion_spider(self.oldorigin, self.enemy);
    }
    else if (self.owner.classtype == CT_MONSHAL) minion_voreling(self.oldorigin, self.enemy);

    // Skip a couple of frames
    self.think = Explode_Egg;
    self.nextthink = time + 0.4;
};

//----------------------------------------------------------------------
// Work out direction speed of egg and then launch it!
//----------------------------------------------------------------------
void(vector eggofs) Create_Egg =
{
    local vector org, dir;
    local float egg_speed;
    if (self.health < 1) return;
    
    // Turn towards player and drop the egg!
    makevectors(self.angles);
    org = self.origin + attack_vector(eggofs);
    egg_speed = self.minion_throwspeed + (random() * self.minion_throwextra);
    dir = (v_forward * egg_speed) + (v_right * (crandom()*self.minion_throwside));
    Launch_Grenade(org,dir,'0 0 0',CT_PROJ_MEGG);
    SUB_AttackFinished (2 + random());
};
