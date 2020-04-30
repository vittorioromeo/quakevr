/*======================================================================
Entity State System
-------------------

When Quake first came out there were strict limitations on how many entities
could exist and be active at the same time. The quick fix solution to this
problem was to use the killtargets key so that entities could be removed. 
This was a very powerful feature with very few restrictions on what it 
could affect or destroy at the same time.

The idea of the entity state system is to create a way to safely switch
entities on, off or have then temporarily disabled. This will help prevent
situations where entity chains are broken or strange errors occur because
the touch function does not have an entity anymore.

The entity state is a new key on entities which is designed to work in a
passive mode until it is activated. All the entities listed below have a
default value so that any existing map will still work as before.

There are two methods for changing the entity state and the first method
is an extra key on triggers entities (trigger _once, _multi, _relay etc)

The ON state (default) will allow the entity to work as designed, be
toggled and physically exist as per its setup.

The OFF state will block all of the designed functionality and hide 
the entity from interaction with the player.

The DISABLE state blocks any toggle ability or entity functionality
and turns off any visual aids like animated textures.
  
The second method is via a brand new set of entities which can affect 
multiple targets at once and are much easier (visually) to see what is
going on because most editors draw target lines. 

======================================================================*/

//----------------------------------------------------------------------
// These entity states are monitored and updated by the entity
void() entity_state_on = { 
    if (self.estate_on) self.estate_on(); 
    else self.estate = ESTATE_ON;
};
void() entity_state_off = { 
    if (self.estate_off) self.estate_off();
    else self.estate = ESTATE_OFF;
};
void() entity_state_disable = { 
    if (self.estate_disable) self.estate_disable();
    self.estate = ESTATE_DISABLE;
};
void() entity_state_reset = { 
    if (self.estate_reset) self.estate_reset();
};
void() entity_state_aframe = {
    // Only switch frames if the entity has a function defintion
    // This will restrict to entities designed for this change
    if (self.estate_aframe) {
        if (other.state == 0) self.frame = 0;
        else self.frame = 1;
    }
};

//----------------------------------------------------------------------
void() entity_state_use =
{
    // Does the firing entity have specific state requests?
    if (other.estate_trigger) {
        if (other.estate_trigger & ESTATE_ON) entity_state_on();
        else if (other.estate_trigger & ESTATE_OFF) entity_state_off();
        else if (other.estate_trigger & ESTATE_DISABLE) entity_state_disable();
        else if (other.estate_trigger & ESTATE_RESET) entity_state_reset();
        else if (other.estate_trigger & ESTATE_AFRAME) entity_state_aframe();
    }
    else {
        // Check if disabled first
        if (self.estate == ESTATE_DISABLE) return;
        
        // Check for USE function
        if (self.estate_use) self.estate_use();
        // entity has a TOGGLE function
        else if (self.estate == ESTATE_OFF) entity_state_on();
        else entity_state_off();
    }
};

//======================================================================
/*QUAKED trigger_entitystate_x (.7 .5 1) (-8 -8 -16) (8 8 16) x
Switch the target(s) entity state X
-------- KEYS --------
target : target entities to switch x
target2 : more target(s) to affect
-------- SPAWNFLAGS --------
-------- NOTES --------
Switch the target(s) entity state X
======================================================================*/
void(string targstr) trigger_entitystate_target =
{
    local entity t, stemp, otemp;
    
    // Start looking for targets to update
    t = find(world, targetname, targstr);
    stemp = self; otemp = other;    // Store self/other
    while(t) {
        // Entity states cannot be applied to players or monsters
        if ( !(t.flags & FL_CLIENT) && !(t.flags & FL_MONSTER)) {
            // Only change the state to ON/OFF/DISABLE
            if (t.use != SUB_Null) {
                if (t.use) {
                    self = t; other = stemp;        // Switch self/other
                    entity_state_use();                // Update entity state
                    self = stemp; other = otemp;    // Restore self/other
                }
            }
        }
        // Are there anymore targets left in the list?
        t = find(t, targetname, targstr);
    }
};

//----------------------------------------------------------------------
void() trigger_entitystate_use =
{
    // Check for multiple target commands
    if (self.target != "") trigger_entitystate_target(self.target);
    if (self.target2 != "") trigger_entitystate_target(self.target2);
    if (self.target3 != "") trigger_entitystate_target(self.target3);
    if (self.target4 != "") trigger_entitystate_target(self.target4);
};    

//----------------------------------------------------------------------
void() trigger_entitystate_off =
{
    self.classtype = CT_ESTATE;
    if (!self.target) dprint("\b[ENTSTATE]\b Missing target!\n");
    if (self.targetname != "") self.use = trigger_entitystate_use;
    self.estate_trigger = ESTATE_OFF;
};

//----------------------------------------------------------------------
void() trigger_entitystate_on =
{
    self.classtype = CT_ESTATE;
    if (!self.target) dprint("\b[ENTSTATE]\b Missing target!\n");
    if (self.targetname != "") self.use = trigger_entitystate_use;
    self.estate_trigger = ESTATE_ON;
};

//----------------------------------------------------------------------
void() trigger_entitystate_disable =
{
    self.classtype = CT_ESTATE;
    if (!self.target) dprint("\b[ENTSTATE]\b Missing target!\n");
    if (self.targetname != "") self.use = trigger_entitystate_use;
    self.estate_trigger = ESTATE_DISABLE;
};

//----------------------------------------------------------------------
void() trigger_entitystate_reset =
{
    self.classtype = CT_ESTATE;
    if (!self.target) dprint("\b[ENTSTATE]\b Missing target!\n");
    if (self.targetname != "") self.use = trigger_entitystate_use;
    self.estate_trigger = ESTATE_RESET;
};

//----------------------------------------------------------------------
void() trigger_entitystate_aframe =
{
    self.classtype = CT_ESTATE;
    if (!self.target) dprint("\b[ENTSTATE]\b Missing target!\n");
    if (self.targetname != "") self.use = trigger_entitystate_use;
    self.estate_trigger = ESTATE_AFRAME;
};
