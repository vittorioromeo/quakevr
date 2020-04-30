/*======================================================================
 Rotate QuickC program by Jim Dose'  10/17/96
 Copyright (c)1996 Hipnotic Interactive, Inc.
 
 * All the functions and defs are self contained in this file
 * Should make it easier to distribute as no other qc file required
 * Collapsed and condensed this QC file as much as possible
 * Renamed all the constants so they don't conflict
 * It was a cool idea for its time (1996) but a horrible solution nowadays
   It would be much better to do this in the engine (rotation collision)
   Unfortunately most popular quake clients (QS/Fitz) refuse to add it
 * Added target2 to func_rotate_door to allow alternative start positions
 
------------------------------------------------------------------------
/*QUAKED info_rotate (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as the point of rotation for rotatable objects.
 
/*QUAKED func_rotate_entity (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE START_ON
Creates an entity that continually rotates.  Can be toggled on and
off if targeted.
 
/*QUAKED path_rotate (0.5 0.3 0) (-8 -8 -8) (8 8 8) ROTATION ANGLES STOP NO_ROTATE DAMAGE MOVETIME SET_DAMAGE
 Path for rotate_train.
 
/*QUAKED func_rotate_train (0 .5 .8) (-8 -8 -8) (8 8 8) x
Trains are moving platforms that players can ride.
If the train is the target of a button or trigger, it will not begin 
moving until activated.
The func_rotate_train entity is the center of rotation of all objects 
targeted by it.

/*QUAKED func_movewall (0 .5 .8) ? VISIBLE TOUCH NONBLOCKING
Used to emulate collision on rotating objects.

/*QUAKED rotate_object (0 .5 .8) ?
This defines an object to be rotated.
Used as the target of func_rotate_door.

/*QUAKED func_rotate_door (0 .5 .8) (-8 -8 -8) (8 8 8) STAYOPEN
Creates a door that rotates between two positions around a point of
rotation each time it's triggered.

------------------------------------------------------------------------
Most modern compilers will expect the rotate_object to always
target a info_rotate (entity) so that it can sort out texture offset
All func_rotating objects DO NOT start at 0,0,0 like other bmodels

------------------------------------------------------------------------
Explaination of func_rotating objects (by Mechtech)

func_rotating_train (controller for rotation)
targetname -> trigger to start/activate train
target -> rotate_object (bmodel) AND rotate_movewall (bmodel)
path-> path_rotate (entity chain)

rotate_object (train brushwork)
target -> info_rotate (entity, center point for rotation)

rotate_movewall (pretends to do collision)

path_rotate (entity chain for train to follow)
targetname -> Used for reference
target -> next entity in chain to follow

------------------------------------------------------------------------
Func rotating doors cannot be moved on start so extra key added

func_rotate_door (controller for rotation)
targetname -> trigger to start/activate two state door
target -> rotate_object (bmodel) AND rotate_movewall (bmodel)
target2 -> info_rotate (entity) starting location for bmodel

======================================================================*/

// from hipdefs.qc in hipnotic/ritual source qc files
.vector neworigin;
.vector rotate;
.float  endtime;
.float  rotate_type;
.string path;
.string group;
.string event;
.float  duration;
.vector dest;    // Could not find this in the source, added

//----------------------------------------------------------------------
// from hipsubs.qc in hipnotic/ritual source qc files
vector ( vector ang ) SUB_NormalizeAngles =
{
   while( ang_x > 360 ) ang_x = ang_x - 360;
   while( ang_x < 0 ) ang_x = ang_x + 360;
   while( ang_y > 360 ) ang_y = ang_y - 360;
   while( ang_y < 0 ) ang_y = ang_y + 360;
   while( ang_z > 360 ) ang_z = ang_z - 360;
   while( ang_z < 0 ) ang_z = ang_z + 360;
   return ang;
};

//----------------------------------------------------------------------
// from triggers.qc in hipnotic/ritual source qc files
void( entity ent, float amount ) hurt_setdamage =
{
   ent.dmg = amount;
   if ( !amount ) ent.solid = SOLID_NOT;
   else ent.solid = SOLID_TRIGGER;
   ent.nextthink = -1;
};

//======================================================================
// Renamed all the constants so they don't conflict with anything else
// They were mostly setup as single words (which is not a good idea)
//======================================================================
float RSTATE_ACTIVE      = 0;
float RSTATE_INACTIVE    = 1;
float RSTATE_SPEEDINGUP  = 2;
float RSTATE_SLOWINGDOWN = 3;

float RSTATE_CLOSED   = 4;
float RSTATE_OPEN     = 5;
float RSTATE_OPENING  = 6;
float RSTATE_CLOSING  = 7;

float RSTATE_WAIT = 0;
float RSTATE_MOVE = 1;
float RSTATE_STOP = 2;
float RSTATE_FIND = 3;
float RSTATE_NEXT = 4;

float OBJECT_ROTATE = 0;
float OBJECT_MOVEWALL  = 1;
float OBJECT_SETORIGIN = 2;

float ROTENTITY_TOGGLE   = 1;
float ROTENTITY_START_ON = 2;

float TRAIN_ROTATION = 1;
float TRAIN_ANGLES   = 2;
float TRAIN_STOP     = 4;
float TRAIN_NO_ROTATE = 8;
float TRAIN_DAMAGE   = 16;
float TRAIN_MOVETIME = 32;
float TRAIN_SET_DAMAGE = 64;

float MOVEWALL_VISIBLE = 1;
float MOVEWALL_TOUCH = 2;
float MOVEWALL_NONBLOCKING = 4;

float DOOR_STAYOPEN = 1;

//======================================================================
/*QUAKED info_rotate (0 0.5 0) (-4 -4 -4) (4 4 4) x
center point for rotatable objects
-------- KEYS --------
-------- SPAWNFLAGS --------
-------- NOTES --------
center point for rotatable objects
Removes self 5s after spawning
*/
//======================================================================

void() info_rotate =
{
    // remove self after a little while, to make sure that entities that
    // have targeted it have had a chance to spawn
    // Not sure why this entity is removed, maybe there was entity
    // limits in the old days, but that is long gone!
    // Leaving entity in map, no advantage to removal anymore
    //self.nextthink = time + 5;
    //self.think = SUB_Remove;
};

//----------------------------------------------------------------------
void() RotateTargets =
{
   local entity ent;
   local vector vx;
   local vector vy;
   local vector vz;
   local vector org;

   makevectors (self.angles);

    ent = find( world, targetname, self.target);
    while( ent ) {
        if ( ent.rotate_type == OBJECT_SETORIGIN ) {
            org = ent.oldorigin;
            vx = ( v_forward * org_x );
            vy = ( v_right   * org_y );
            vy = vy * -1;
            vz = ( v_up      * org_z );
            ent.neworigin = vx + vy + vz;
            setorigin( ent, ent.neworigin + self.origin );
        }
        else if ( ent.rotate_type == OBJECT_ROTATE ) {
            ent.angles = self.angles;
            org = ent.oldorigin;
            vx = ( v_forward * org_x );
            vy = ( v_right   * org_y );
            vy = vy * -1;
            vz = ( v_up      * org_z );
            ent.neworigin = vx + vy + vz;
            setorigin( ent, ent.neworigin + self.origin );
        }
        else {
            org = ent.oldorigin;
            vx = ( v_forward * org_x );
            vy = ( v_right   * org_y );
            vy = vy * -1;
            vz = ( v_up      * org_z );
            ent.neworigin = vx + vy + vz;
            ent.neworigin = self.origin - self.oldorigin + (ent.neworigin - ent.oldorigin);
            ent.velocity = (ent.neworigin-ent.origin)*25;
        }
        ent = find( ent, targetname, self.target);
    }
};

//----------------------------------------------------------------------
void() RotateTargetsFinal =
{
    local entity ent;

    ent = find( world, targetname, self.target);
    while( ent ) {
        ent.velocity = '0 0 0';
        if ( ent.rotate_type == OBJECT_ROTATE ) ent.angles = self.angles;
        ent = find( ent, targetname, self.target);
    }
};

//----------------------------------------------------------------------
void() SetTargetOrigin =
{
    local entity ent;

    ent = find( world, targetname, self.target);
    while( ent ) {
        if ( ent.rotate_type == OBJECT_MOVEWALL ) {
            setorigin( ent, self.origin - self.oldorigin +
            (ent.neworigin - ent.oldorigin) );
        }
        else {
            setorigin( ent, ent.neworigin + self.origin );
        }
        ent = find( ent, targetname, self.target);
    }
};

//----------------------------------------------------------------------
void() LinkRotateTargets =
{
    local entity ent;
    local vector tempvec;

    self.oldorigin = self.origin;
    ent = find( world, targetname, self.target);
    while( ent ) {
        if ( ent.classname == "rotate_object" ) {
            ent.rotate_type = OBJECT_ROTATE;
            ent.oldorigin = ent.origin - self.oldorigin;
            ent.neworigin = ent.origin - self.oldorigin;
            ent.owner = self;
        }
        else if ( ent.classname == "func_movewall" ) {
            ent.rotate_type = OBJECT_MOVEWALL;
            tempvec = ( ent.absmin + ent.absmax ) * 0.5;
            ent.oldorigin = tempvec - self.oldorigin;
            ent.neworigin = ent.oldorigin;
            ent.owner = self;
        }
        else {
            ent.rotate_type = OBJECT_SETORIGIN;
            ent.oldorigin = ent.origin - self.oldorigin;
            ent.neworigin = ent.origin - self.oldorigin;
        }
        ent = find (ent, targetname, self.target);
    }
};

//----------------------------------------------------------------------
void( float amount ) SetDamageOnTargets =
{
    local entity ent;

    ent = find( world, targetname, self.target);
    while( ent ) {
        if ( ent.classname == "trigger_hurt" ) hurt_setdamage( ent, amount );
        else if ( ent.classname == "func_movewall" ) ent.dmg = amount;
        ent = find( ent, targetname, self.target);
    }
};


//----------------------------------------------------------------------
// Simple continual rotatation
//----------------------------------------------------------------------
void() rotate_entity_think =
{
    local float t;

    t = time - self.ltime;
    self.ltime = time;

    if ( self.state == RSTATE_SPEEDINGUP ) {
        self.count = self.count + self.cnt * t;
        if ( self.count > 1 ) self.count = 1;

        // get rate of rotation
        t = t * self.count;
    }
    else if ( self.state == RSTATE_SLOWINGDOWN ) {
        self.count = self.count - self.cnt * t;
        if ( self.count < 0 ) {
            RotateTargetsFinal();
            self.state = RSTATE_INACTIVE;
            self.think = SUB_Null;
            return;
        }

        // get rate of rotation
        t = t * self.count;
    }

    self.angles = self.angles + ( self.rotate * t );
    self.angles = SUB_NormalizeAngles( self.angles );
    RotateTargets();
    self.nextthink = time + 0.02;
};

//----------------------------------------------------------------------
void() rotate_entity_use =
{
    // change to alternate textures
    self.frame = 1 - self.frame;

    if ( self.state == RSTATE_ACTIVE ) {
        if ( self.spawnflags & ROTENTITY_TOGGLE ) {
            if ( self.speed ) {
                self.count = 1;
                self.state = RSTATE_SLOWINGDOWN;
            }
            else {
                self.state = RSTATE_INACTIVE;
                self.think = SUB_Null;
            }
        }
    }
    else if ( self.state == RSTATE_INACTIVE ) {
        self.think = rotate_entity_think;
        self.nextthink = time + 0.02;
        self.ltime = time;
        if ( self.speed ) {
            self.count = 0;
            self.state = RSTATE_SPEEDINGUP;
        }
        else {
            self.state = RSTATE_ACTIVE;
        }
    }
    else if ( self.state == RSTATE_SPEEDINGUP ) {
        if ( self.spawnflags & ROTENTITY_TOGGLE ) {
            self.state = RSTATE_SLOWINGDOWN;
        }
    }
    else {
        self.state = RSTATE_SPEEDINGUP;
    }
};

//----------------------------------------------------------------------
void() rotate_entity_firstthink =
{
    LinkRotateTargets();
    if ( self.spawnflags & ROTENTITY_START_ON ) {
        self.state = RSTATE_ACTIVE;
        self.think = rotate_entity_think;
        self.nextthink = time + 0.02;
        self.ltime = time;
    }
    else {
        self.state = RSTATE_INACTIVE;
        self.think = SUB_Null;
    }
    self.use = rotate_entity_use;
};

//======================================================================
/*QUAKED func_rotate_entity (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE STARTON
Basic rotating object
-------- KEYS --------
target : points to center of rotation entity
rotate : The rate of rotation in degrees
speed  : time taken to go from zero to full speed and vice-versa
-------- SPAWNFLAGS --------
TOGGLE  : allows the rotation to be toggled on/off
STARTON : whether the entity is spinning when spawned.  
If TOGGLE is 0, entity can be turned on, but not off.
-------- NOTES --------
Basic rotating object
*/
//======================================================================
void() func_rotate_entity =
{
    self.solid    = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;

    setmodel (self, self.model);
    setsize( self, self.mins, self.maxs );

    if ( self.speed != 0 ) self.cnt = 1 / self.speed;

    self.think = rotate_entity_firstthink;
    self.nextthink = time + 0.1;
    self.ltime = time;
};

//======================================================================
/*QUAKED path_rotate (0.5 0.3 0) (-8 -8 -8) (8 8 8) ROTATION ANGLES STOP NO_ROTATE DAMAGE MOVETIME SET_DAMAGE
Corner Path for rotate_train
-------- KEYS --------
event  : is a target to trigger when train arrives at path_rotate
noise  : contains the name of the sound to play when train stops
noise1 : contains the name of the sound to play when train moves
speed  : set speed to be the new speed of the train after it reaches
  the path change.  If speed is -1, the train will warp directly to the next
  path change after the specified wait time.  If MOVETIME is set on the
  path_rotate, the train to interprets "speed" as the length of time to
  take moving from one corner to another.
-------- SPAWNFLAGS --------
ROTATION  : Rotate train at rate specified by 'rotate', use '0 0 0' to stop
ANGLES    : Rotate to the angles specified by 'angles'
  while traveling to this path_rotate.  Use values < 0 or > 360 to 
  guarantee that it turns in a certain direction.  Having this flag 
  set automatically clears any rotation
STOP      : stop train and wait to be retriggered
NO_ROTATE : stop train rotating when waiting to be triggered
DAMAGE    : Cause damage based on 'dmg'
MOVETIME  : Interpret 'speed' as the length of time to take moving
SET_DAMAGE: Set all targets damage to 'dmg'
-------- NOTES --------
Corner Path for rotate_train
*/
//======================================================================
void() rotate_train;
void() rotate_train_next;
void() rotate_train_find;

//----------------------------------------------------------------------
void() path_rotate =
{
    if ( self.noise ) precache_sound( self.noise );
    if ( self.noise1 )     precache_sound( self.noise1 );
};

//----------------------------------------------------------------------
void() rotate_train_think =
{
    local float t;
    local float timeelapsed;

    t = time - self.ltime;
    self.ltime = time;

    if ( ( self.endtime ) && ( time >= self.endtime ) ) {
        self.endtime = 0;
        if ( self.state == RSTATE_MOVE ) {
            setorigin(self, self.finaldest);
            self.velocity = '0 0 0';
        }

        if (self.think1) self.think1();
    }
    else {
        timeelapsed = (time - self.cnt) * self.duration;
        if ( timeelapsed > 1 ) timeelapsed = 1;
        setorigin( self, self.dest1 + ( self.dest2 * timeelapsed ) );
    }

    self.angles = self.angles + ( self.rotate * t );
    self.angles = SUB_NormalizeAngles( self.angles );
    RotateTargets();

    self.nextthink = time + 0.02;
};

//----------------------------------------------------------------------
void() rotate_train_use =
{
    if (self.think1 != rotate_train_find) {
        if ( self.velocity != '0 0 0' ) return;        // already activated
        if ( self.think1 ) self.think1();
    }
};

//----------------------------------------------------------------------
void() rotate_train_wait =
{
    self.state = RSTATE_WAIT;

    if ( self.goalentity.noise ) {
        sound (self, CHAN_VOICE, self.goalentity.noise, 1, ATTN_NORM);
    }
    else sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    if ( self.goalentity.spawnflags & TRAIN_ANGLES ) {
        self.rotate = '0 0 0';
        self.angles = self.finalangle;
    }
    
    if ( self.goalentity.spawnflags & TRAIN_NO_ROTATE ) self.rotate = '0 0 0';

    self.endtime = self.ltime + self.goalentity.wait;
    self.think1 = rotate_train_next;
};

//----------------------------------------------------------------------
void() rotate_train_stop =
{
    self.state = RSTATE_STOP;

    if ( self.goalentity.noise ) {
        sound (self, CHAN_VOICE, self.goalentity.noise, 1, ATTN_NORM);
    }
    else sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    if ( self.goalentity.spawnflags & TRAIN_ANGLES ) {
        self.rotate = '0 0 0';
        self.angles = self.finalangle;
    }
    if ( self.goalentity.spawnflags & TRAIN_NO_ROTATE ) self.rotate = '0 0 0';

    self.dmg = 0;
    self.think1 = rotate_train_next;
};

//----------------------------------------------------------------------
void() rotate_train_next =
{
    local entity targ;
    local entity current;
    local vector   vdestdelta;
    local float    len, traveltime, div;
    local string temp;

    self.state = RSTATE_NEXT;

    current = self.goalentity;
    targ = find (world, targetname, self.path );
    if ( targ.classname != "path_rotate" )
        dprint("\b[ROT TRAIN]\b Next target is not path_rotate\n");

    if ( self.goalentity.noise1 ) self.noise1 = self.goalentity.noise1;
    sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);

    self.goalentity = targ;
    self.path = targ.target;
    if (!self.path ) dprint("\b[ROT TRAIN]\b No next target\n");

    if ( targ.spawnflags & TRAIN_STOP ) self.think1 = rotate_train_stop;
    else if (targ.wait) self.think1 = rotate_train_wait;
    else self.think1 = rotate_train_next;

    if ( current.event ) {
        // Trigger any events that should happen at the corner.
        temp = self.target;
        self.target = current.event;
        self.message = current.message;
        SUB_UseTargets();
        self.target = temp;
        self.message = string_null;
    }

    if ( current.spawnflags & TRAIN_ANGLES ) {
        self.rotate = '0 0 0';
        self.angles = self.finalangle;
    }

    if ( current.spawnflags & TRAIN_ROTATION ) self.rotate = current.rotate;
    if ( current.spawnflags & TRAIN_DAMAGE ) self.dmg = current.dmg;
    if ( current.spawnflags & TRAIN_SET_DAMAGE ) SetDamageOnTargets( current.dmg );

    if ( current.speed == -1 ) {
        // Warp to the next path_corner
        setorigin( self, targ.origin );
        self.endtime = self.ltime + 0.01;
        SetTargetOrigin();

        if ( targ.spawnflags & TRAIN_ANGLES ) self.angles = targ.angles;

        self.duration = 1;         // 1 / duration
        self.cnt = time;           // start time
        self.dest2 = '0 0 0';      // delta
        self.dest1 = self.origin;  // original position
        self.finaldest = self.origin;
    }
    else {
        self.state = RSTATE_MOVE;

        self.finaldest = targ.origin;
        if (self.finaldest == self.origin) {
            self.velocity = '0 0 0';
            self.endtime = self.ltime + 0.1;

            self.duration = 1;        // 1 / duration
            self.cnt = time;          // start time
            self.dest2 = '0 0 0';     // delta
            self.dest1 = self.origin; // original position
            self.finaldest = self.origin;
            return;
        }
        // set destdelta to the vector needed to move
        vdestdelta = self.finaldest - self.origin;

        // calculate length of vector
        len = vlen (vdestdelta);

        if ( current.spawnflags & TRAIN_MOVETIME ) traveltime = current.speed;
        else {
            // check if there's a speed change
            if (current.speed>0) self.speed = current.speed;
            if (!self.speed) dprint("\b[ROT TRAIN]\b No speed is defined!\n");

            // divide by speed to get time to reach dest
            traveltime = len / self.speed;
        }

        if (traveltime < 0.1) {
            self.velocity = '0 0 0';
            self.endtime = self.ltime + 0.1;
            if ( targ.spawnflags & TRAIN_ANGLES ) self.angles = targ.angles;
            return;
        }

        // qcc won't take vec/float
        div = 1 / traveltime;

        if ( targ.spawnflags & TRAIN_ANGLES ) {
            self.finalangle = SUB_NormalizeAngles( targ.angles );
            self.rotate = ( targ.angles - self.angles ) * div;
        }

        // set endtime to trigger a think when dest is reached
        self.endtime = self.ltime + traveltime;

        // scale the destdelta vector by the time spent traveling to get velocity
        self.velocity = vdestdelta * div;

        self.duration = div;      // 1 / duration
        self.cnt = time;          // start time
        self.dest2 = vdestdelta;  // delta
        self.dest1 = self.origin; // original position
    }
};

//----------------------------------------------------------------------
void() rotate_train_find =
{
    local entity targ;

    self.state = RSTATE_FIND;
    LinkRotateTargets();

    // the first target is the point of rotation.
    // the second target is the path.
    targ = find ( world, targetname, self.path);
    if ( targ.classname != "path_rotate" )
        dprint("\b[ROT TRAIN]\b Next target is not path_rotate\n");

    // Save the current entity
    self.goalentity = targ;

    if ( targ.spawnflags & TRAIN_ANGLES ) {
        self.angles = targ.angles;
        self.finalangle = SUB_NormalizeAngles( targ.angles );
    }

    self.path = targ.target;
    setorigin (self, targ.origin );
    SetTargetOrigin();
    RotateTargetsFinal();
    self.think1 = rotate_train_next;
    if (!self.targetname) {
        // not triggered, so start immediately
        self.endtime = self.ltime + 0.1;
    }
    else self.endtime = 0;

    self.duration = 1;        // 1 / duration
    self.cnt = time;          // start time
    self.dest2 = '0 0 0';     // delta
    self.dest1 = self.origin; // original position
};

//======================================================================
/*QUAKED func_rotate_train (0 .5 .8) (-8 -8 -8) (8 8 8) x
Trains are moving platforms that players can ride
-------- KEYS --------
path   : Starts at'path_rotate' position
speed  : travel speed (def=100)
dmg    : blocking damage (def=0)
sounds : 0=Silent, 1=ratchet metal
noise  : sound to play when train stops
noise1 : sound to play when train moves
-------- SPAWNFLAGS --------
-------- NOTES --------
Trains are moving platforms that players can ride
If the train is the target of a button or trigger, it will for trigger
The func_rotate_train entity is the center of rotation of all objects 
targeted by it.

*/
//======================================================================
void() rotate_train =
{
    dprint("\b[ROTATE]\b rotate_train not supported anymore!\n");
};

//----------------------------------------------------------------------
void() func_rotate_train =
{
    if (!self.speed) self.speed = 100;
    if (!self.target) dprint("\b[ROT TRAIN]\b missing target!\n");

    if ( !self.noise ) {
        if (self.sounds == 0) self.noise = SOUND_EMPTY;
        if (self.sounds == 1) self.noise = "plats/train2.wav";
    }
    if ( !self.noise1 ) {
        if (self.sounds == 0) self.noise1 = SOUND_EMPTY;
        if (self.sounds == 1) self.noise1 = "plats/train1.wav";
    }

    precache_sound( self.noise );
    precache_sound( self.noise1 );

    self.cnt = 1;
    self.solid    = SOLID_NOT;
    self.movetype = MOVETYPE_STEP;
    self.use = rotate_train_use;

    setmodel (self, self.model);
    setsize (self, self.mins, self.maxs);
    setorigin (self, self.origin);

    // start trains on the second frame, to make sure their 
    // targets have had a chance to spawn
    self.ltime = time;
    self.nextthink = self.ltime + 0.1;
    self.endtime = self.ltime + 0.1;
    self.think = rotate_train_think;
    self.think1 = rotate_train_find;
    self.state = RSTATE_FIND;

    self.duration = 1;        // 1 / duration
    self.cnt = 0.1;           // start time
    self.dest2 = '0 0 0';     // delta
    self.dest1 = self.origin; // original position

    self.flags = self.flags | FL_ONGROUND;
};

//----------------------------------------------------------------------
// Moving clip walls
//----------------------------------------------------------------------
void() rotate_door_reversedirection;
void() rotate_door_group_reversedirection;

void() movewall_touch =
{
    if (time < self.owner.attack_finished) return;

    if ( self.dmg ) {
        T_Damage (other, self, self.owner, self.dmg, DAMARMOR);
        self.owner.attack_finished = time + 0.5;
    }
    else if ( self.owner.dmg ) {
        T_Damage (other, self, self.owner, self.owner.dmg, DAMARMOR);
        self.owner.attack_finished = time + 0.5;
    }
};

//----------------------------------------------------------------------
void() movewall_blocked =
{
    local entity temp;

    if (time < self.owner.attack_finished) return;

    self.owner.attack_finished = time + 0.5;

    if ( self.owner.classname == "func_rotate_door" ) {
        temp = self;
        self = self.owner;
        rotate_door_group_reversedirection();
        self = temp;
    }

    if ( self.dmg ) {
        T_Damage (other, self, self.owner, self.dmg, DAMARMOR);
        self.owner.attack_finished = time + 0.5;
    }
    else if ( self.owner.dmg ) {
        T_Damage (other, self, self.owner, self.owner.dmg, DAMARMOR);
        self.owner.attack_finished = time + 0.5;
    }
};

//----------------------------------------------------------------------
void() movewall_think =
{
    self.ltime = time;
    self.nextthink = time + 0.02;
};

//======================================================================
/*QUAKED func_movewall (0 .5 .8) ? VISIBLE TOUCH NONBLOCKING
Emulate collision on rotating objects
-------- KEYS --------
dmg : damage to cause when touched or blocked
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
VISIBLE     : Causes brush to be displayed
TOUCH       : Cause damage when touched by player
NONBLOCKING : makes the brush non-solid,  This is useless if VISIBLE is set
-------- NOTES --------
Emulate collision on rotating objects
*/
//======================================================================
void() func_movewall =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors
    
    self.angles = '0 0 0';
    self.movetype = MOVETYPE_PUSH;
    
    if ( self.spawnflags & MOVEWALL_NONBLOCKING ) self.solid = SOLID_NOT;
    else {
        self.solid = SOLID_BSP;
        self.blocked = movewall_blocked;
    }
    
    if ( self.spawnflags & MOVEWALL_TOUCH ) {
        self.touch = movewall_touch;
    }
    
    setmodel (self,self.model);
    if ( !( self.spawnflags & MOVEWALL_VISIBLE ) ) self.model = string_null;

    self.think = movewall_think;
    self.nextthink = time + 0.02;
    self.ltime = time;
};

//======================================================================
/*QUAKED rotate_object (0 .5 .8) ?
Defines a bmodel object to be rotated
-------- KEYS --------
targetname : name of bmodel object
target     : rotating point entities to link too
_dirt       : -1 = will be excluded from dirtmapping
_minlight   : Minimum light level for any surface of the brush model
_mincolor   : Minimum light color for any surface (def='1 1 1' RGB)
_shadow     : Will cast shadows on other models and itself
_shadowself : Will cast shadows on itself
-------- SPAWNFLAGS --------
-------- NOTES --------
Defines a bmodel object to be rotated
Used as the target of func_rotate_door
*/
//======================================================================
void() rotate_object_move =
{
    local entity ent;
    
    ent = find( world, targetname, self.target2);
    if (ent.classname == "info_rotate") {
        // Move origin to new location
        self.oldorigin = ent.origin;
        setorigin( self, self.oldorigin );
    }
};

//----------------------------------------------------------------------
void() rotate_object =
{
    if (check_bmodel_keys()) return;    // Check for bmodel errors
    
    self.classname = "rotate_object";
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.bsporigin = TRUE;        // bmodel origin 0,0,0
    setmodel (self,self.model);
    setsize( self, self.mins, self.maxs );
    
    // Check for a target (info_rotate) to move the object
    if (self.target2 != "") {
        self.think = rotate_object_move;
        self.nextthink = time + 1;
    }
    else self.think = SUB_Null;
};

//----------------------------------------------------------------------
void() rotate_door_think2 =
{
    local float t;

    t = time - self.ltime;
    self.ltime = time;

    // change to alternate textures
    self.frame = 1 - self.frame;
    self.angles = self.dest;

    if ( self.state == RSTATE_OPENING ) self.state = RSTATE_OPEN;
    else {
        if ( self.spawnflags & DOOR_STAYOPEN ) {
            rotate_door_group_reversedirection();
            return;
        }
        self.state = RSTATE_CLOSED;
    }

    sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
    self.think = SUB_Null;

    RotateTargetsFinal();
};

//----------------------------------------------------------------------
void() rotate_door_think =
{
    local float t;

    t = time - self.ltime;
    self.ltime = time;

    if ( time < self.endtime ) {
        self.angles = self.angles + ( self.rotate * t );
        RotateTargets();
    }
    else {
        self.angles = self.dest;
        RotateTargets();
        self.think = rotate_door_think2;
    }

    self.nextthink = time + 0.01;
};

//----------------------------------------------------------------------
void() rotate_door_reversedirection =
{
    local vector start;

    // change to alternate textures
    self.frame = 1 - self.frame;

    if ( self.state == RSTATE_CLOSING ) {
        start = self.dest1;
        self.dest = self.dest2;
        self.state = RSTATE_OPENING;
    }
    else {
        start = self.dest2;
        self.dest = self.dest1;
        self.state = RSTATE_CLOSING;
    }

    sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);

    self.rotate = ( self.dest - start ) * ( 1 / self.speed );
    self.think = rotate_door_think;
    self.nextthink = time + 0.02;
    self.endtime = time + self.speed - ( self.endtime - time );
    self.ltime = time;
};

//----------------------------------------------------------------------
void() rotate_door_group_reversedirection =
{
    local string name;

    // tell all associated rotaters to reverse direction
    if ( self.group ) {
        name = self.group;
        self = find( world, group, name);
        while( self ) {
            rotate_door_reversedirection();
            self = find( self, group, name);
        }
    }
    else rotate_door_reversedirection();
};

//----------------------------------------------------------------------
void() rotate_door_use =
{
    // local entity t;
    local vector start;

    if ( ( self.state != RSTATE_OPEN ) && ( self.state != RSTATE_CLOSED ) ) return;

    if ( !self.cnt ) {
        self.cnt = 1;
        LinkRotateTargets();
    }

    // change to alternate textures
    self.frame = 1 - self.frame;

    if ( self.state == RSTATE_CLOSED ) {
        start = self.dest1;
        self.dest = self.dest2;
        self.state = RSTATE_OPENING;
    }
    else {
        start = self.dest2;
        self.dest = self.dest1;
        self.state = RSTATE_CLOSING;
    }

    sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);

    self.rotate = ( self.dest - start ) * ( 1 / self.speed );
    self.think = rotate_door_think;
    self.nextthink = time + 0.01;
    self.endtime = time + self.speed;
    self.ltime = time;
};

//======================================================================
/*QUAKED func_rotate_door (0 .5 .8) (-8 -8 -8) (8 8 8) STAYOPEN
Rotating door moving around a point of rotation
-------- KEYS --------
target  : Points to the rotate_object bmodel entity
target2 : Move on spawn to this entity (info_rotate) location
dmg     : Damage caused when blocked (def=2) negative numbers =no damage
speed   : Time taken to rotate open/close
sounds  : 1=medieval 2=metal 3=base 4=Silent (def=1)
-------- SPAWNFLAGS --------
STAYOPEN : Reopen after closing, stops a once only door from closing if blocked
-------- NOTES --------
Rotating door moving around a point of rotation
*/
//======================================================================
void() func_rotate_door =
{
    if (!self.target) dprint("\b[ROT DOOR]\b missing target!\n");

    self.dest1 = '0 0 0';
    self.dest2 = self.angles;
    self.angles = self.dest1;

    // default to 2 seconds
    if ( !self.speed ) self.speed = 2;
    self.cnt = 0;

    if (!self.dmg) self.dmg = 2;
    else if ( self.dmg < 0 ) self.dmg = 0;

    if (self.sounds == 0) self.sounds = 1;
    if (self.sounds == 1) {
        self.noise1 = "doors/latch2.wav";
        self.noise2 = "doors/winch2.wav";
        self.noise3 = "doors/drclos4.wav";
    }
    if (self.sounds == 2) {
        self.noise2 = "doors/airdoor1.wav";
        self.noise1 = "doors/airdoor2.wav";
        self.noise3 = "doors/airdoor2.wav";
    }
    if (self.sounds == 3) {
        self.noise2 = "doors/basesec1.wav";
        self.noise1 = "doors/basesec2.wav";
        self.noise3 = "doors/basesec2.wav";
    }
    if (self.sounds == 4) {
        self.noise1 = SOUND_EMPTY;
        self.noise2 = SOUND_EMPTY;
        self.noise3 = SOUND_EMPTY;
    }
    
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    precache_sound (self.noise3);

    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setmodel (self, self.model);
    setorigin( self, self.origin );
    setsize( self, self.mins, self.maxs );
    self.state = RSTATE_CLOSED;
    self.use = rotate_door_use;
    self.think = SUB_Null;
};
