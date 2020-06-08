/*======================================================================
 Flying units maintain the same Z axis as their enemy targets
 This makes it impossible to have flying units above the player
 because the engine is constantly moving the monster downwards
 
 This system replaces the enemy target with a fake one which
 floats above the real enemy target. This fake marker can be
 seen if developer 1 is active, otherwise hidden
 
 The engine will track the fake enemytarget and gameplay functions
 can carry on using the real enemy target for calculations
 
 All the gameplay functions should use these self.enemy wrappers
 defined below to make sure they return the correct enemy!
 
======================================================================*/
// Detects enemytarget and returns real ENEMY angles
// Returns the correct takedamage flag, useful for function tests
float() SUB_takedEnemyTarget =
{
    if (!self.enemy) return -1;
    if (self.enemy.classtype == CT_ENEMYTARGET) return (self.enemy.enemy.takedamage);
    else return (self.enemy.takedamage);
};

//----------------------------------------------------------------------
// Detects enemytarget and returns for flags bitwise operation
// Returns the correct entity flags, useful for function tests
float(float bitflag) SUB_flagsEnemyTarget =
{
    if (!self.enemy) return -1;
    if (self.enemy.classtype == CT_ENEMYTARGET) return (self.enemy.enemy.flags & bitflag);
    else return (self.enemy.flags & bitflag);
};

//----------------------------------------------------------------------
// Detects enemytarget and returns real ENEMY angles
// Returns the correct entity angles, useful for function tests
vector() SUB_angEnemyTarget =
{
    if (!self.enemy) return VEC_ORIGIN;
    if (self.enemy.classtype == CT_ENEMYTARGET) return self.enemy.enemy.angles;
    else return self.enemy.angles;
};

//----------------------------------------------------------------------
// Detects enemytarget and returns real ENEMY health
// Returns the correct entity health, useful for function tests
float() SUB_healthEnemyTarget =
{
    if (!self.enemy) return -1;
    if (self.enemy.classtype == CT_ENEMYTARGET) return self.enemy.enemy.health;
    else return self.enemy.health;
};

//----------------------------------------------------------------------
// Detects enemytarget and returns real ENEMY origin
// Returns the correct entity origin, useful for function tests
vector() SUB_orgEnemyTarget =
{
    if (!self.enemy) return VEC_ORIGIN;
    if (self.enemy.classtype == CT_ENEMYTARGET) return self.enemy.enemy.origin;
    else return self.enemy.origin;
};

//----------------------------------------------------------------------
// Detects enemytarget and returns the real ENEMY entity
// Returns the correct entity, useful for function tests
entity() SUB_entEnemyTarget =
{
    if (!self.enemy) return world;
    if (self.enemy.classtype == CT_ENEMYTARGET) return self.enemy.enemy;
    else return self.enemy;
};

//----------------------------------------------------------------------
// Returns TRUE if enemytarget system active
// A simple test to see if enemytarget system is active
float() SUB_EnemyTarget =
{
    if (!self.enemy) return -1;
    if (self.enemy.classtype == CT_ENEMYTARGET) return TRUE;
    else return FALSE;
};

//----------------------------------------------------------------------
// Self = monster; Will switch enemy and enemytarget around (restore)
// Its a good idea to do this when the enemytarget is out of sight
// Will restore the enemy to the ground height (useful for doorways)
void() SUB_switchEnemyTarget =
{
    if (self.enemytarget.classtype != CT_ENEMYTARGET) return;
    self.enemytarget.state = STATE_OFF;
    self.enemy = self.enemytarget.enemy;
};

//----------------------------------------------------------------------
// Self = monster; Will remove enemytarget function safely (delete)
// The enemytarget system should take care of itself and safely delete
// This is a brute force version that will make sure, "its dead jim!"
void() SUB_removeEnemyTarget =
{
    if (self.enemytarget.classtype != CT_ENEMYTARGET) return;
    self.enemytarget.state = STATE_OFF;
    self.enemytarget.think = SUB_Remove;
    self.enemytarget.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
// Function called all the time when the enemytarget is active
void() SUB_updateEnemyTarget =
{
    // OWNER DIED - Safely remove this entity
    if (self.owner.health < 1) {
        self.state = STATE_OFF;
        self.think = SUB_Remove;
        self.nextthink = time + 0.1;
    }
    // TARGET DIED - Stop tracking, wait for another enemy
    else if (self.enemy.health < 1) {
        self.state = STATE_OFF;
    }
    // ENEMY ACTIVE - Update position and check for height change
    else if (self.state == STATE_ON) {
        // Is there any random variance to the Z location?
        if (self.waitmin < time && self.wait > 0) {
            self.waitmin = time + self.wait + random()*self.wait;
            // Check if the enemy is close? Go to maximum height if inside melee range
            self.enemydist = range_distance(SUB_entEnemyTarget(), FALSE);
            if (self.enemydist < MONAI_ABOVEMELEE) self.view_ofs_z = self.height;
            else self.view_ofs_z = (self.height*0.25) + ((self.height*0.75) * random());
        }
        
        // Check for solid/sky origin content
        self.lip = pointcontents(self.enemy.origin + self.view_ofs);
        if (self.lip == CONTENT_SOLID || self.lip == CONTENT_SKY) {
            // Traceline upwards to nearest point
            traceline(self.enemy.origin, self.enemy.origin + self.view_ofs, TRUE, self);
            setorigin(self,trace_endpos);
        }
        else setorigin(self, self.enemy.origin + self.view_ofs);
        
        // Next update time tick (def=0.1s)
        self.think = SUB_updateEnemyTarget;
        self.nextthink = time + 0.1;
    }
};

//----------------------------------------------------------------------
void(entity targ, float zoffset, float rndtimer) SUB_setupEnemyTarget =
{
    // Don't try to get higher than other flying units
    // Two monsters with the same abilty will constantly rise up
    if (targ.movetype == MOVETYPE_FLY) return;
    
    // Create the enemytarget if one does not exist
    if (!self.enemytarget) {
        self.enemytarget = spawn();
        self.enemytarget.owner = self;
        self.enemytarget.classtype = CT_ENEMYTARGET;

        // Show enemytarget system in devmode ONLY (dev helper)
        if (developer > 0 && !query_configflag(SVR_DEVHELPER)) {
            self.enemytarget.mdl = MODEL_BROKEN;
            setmodel(self.enemytarget, self.enemytarget.mdl);
        }
        setsize (self.enemytarget, VEC_ORIGIN, VEC_ORIGIN);
        self.enemytarget.movetype = MOVETYPE_NONE;
        self.enemytarget.solid = SOLID_NOT;
        // Plenty of functions test for health and damage
        self.enemytarget.health = LARGE_TIMER;
        self.enemytarget.takedamage = DAMAGE_YES;
    }
    
    // If attacking a monster stay up high out its way, else move up and down
    if (targ.flags & FL_MONSTER) self.enemytarget.wait = 0;
    else if (self.bossflag) self.enemytarget.wait = 0;
    else self.enemytarget.wait = rndtimer;

    // Setup parameters and enable update routine
    self.enemytarget.state = STATE_ON;
    self.enemytarget.height = self.enemytarget.view_ofs_z = zoffset;
    self.enemytarget.enemy = targ;
    self.enemytarget.think = SUB_updateEnemyTarget;
    self.enemytarget.nextthink = time + 0.1;
};