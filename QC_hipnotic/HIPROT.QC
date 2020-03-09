/* Rotate QuickC program
   By Jim Dose'  10/17/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

float STATE_ACTIVE      = 0;
float STATE_INACTIVE    = 1;
float STATE_SPEEDINGUP  = 2;
float STATE_SLOWINGDOWN = 3;

float STATE_CLOSED   = 4;
float STATE_OPEN     = 5;
float STATE_OPENING  = 6;
float STATE_CLOSING  = 7;

float STATE_WAIT = 0;
float STATE_MOVE = 1;
float STATE_STOP = 2;
float STATE_FIND = 3;
float STATE_NEXT = 4;

float OBJECT_ROTATE = 0;
float OBJECT_MOVEWALL  = 1;
float OBJECT_SETORIGIN = 2;

float TOGGLE   = 1;
float START_ON = 2;

float ROTATION = 1;
float ANGLES   = 2;
float STOP     = 4;
float NO_ROTATE = 8;
float DAMAGE   = 16;
float MOVETIME = 32;
float SET_DAMAGE = 64;

float VISIBLE = 1;
float TOUCH = 2;
float NONBLOCKING = 4;

float STAYOPEN = 1;

/*QUAKED info_rotate (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as the point of rotation for rotatable objects.
*/
void() info_rotate =
{
// remove self after a little while, to make sure that entities that
// have targeted it have had a chance to spawn
   self.nextthink = time + 2;
   self.think = SUB_Remove;
};

void() RotateTargets =
   {
   local entity ent;
   local vector vx;
   local vector vy;
   local vector vz;
   local vector org;

   makevectors (self.angles);

   ent = find( world, targetname, self.target);
   while( ent )
      {
      if ( ent.rotate_type == OBJECT_SETORIGIN )
         {
         org = ent.oldorigin;
         vx = ( v_forward * org_x );
         vy = ( v_right   * org_y );
         vy = vy * -1;
         vz = ( v_up      * org_z );
         ent.neworigin = vx + vy + vz;
         setorigin( ent, ent.neworigin + self.origin );
         }
      else if ( ent.rotate_type == OBJECT_ROTATE )
         {
         ent.angles = self.angles;
         org = ent.oldorigin;
         vx = ( v_forward * org_x );
         vy = ( v_right   * org_y );
         vy = vy * -1;
         vz = ( v_up      * org_z );
         ent.neworigin = vx + vy + vz;
         setorigin( ent, ent.neworigin + self.origin );
         }
      else
         {
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

void() RotateTargetsFinal =
   {
   local entity ent;

   ent = find( world, targetname, self.target);
   while( ent )
      {
      ent.velocity = '0 0 0';
      if ( ent.rotate_type == OBJECT_ROTATE )
         {
         ent.angles = self.angles;
         }
      ent = find( ent, targetname, self.target);
      }
   };

void() SetTargetOrigin =
   {
   local entity ent;

   ent = find( world, targetname, self.target);
   while( ent )
      {
      if ( ent.rotate_type == OBJECT_MOVEWALL )
         {
         setorigin( ent, self.origin - self.oldorigin +
            (ent.neworigin - ent.oldorigin) );
         }
      else
         {
         setorigin( ent, ent.neworigin + self.origin );
         }
      ent = find( ent, targetname, self.target);
      }
   };

void() LinkRotateTargets =
   {
   local entity ent;
   local vector tempvec;

   self.oldorigin = self.origin;
   ent = find( world, targetname, self.target);
   while( ent )
      {
      if ( ent.classname == "rotate_object" )
         {
         ent.rotate_type = OBJECT_ROTATE;
         ent.oldorigin = ent.origin - self.oldorigin;
         ent.neworigin = ent.origin - self.oldorigin;
         ent.owner = self;
         }
      else if ( ent.classname == "func_movewall" )
         {
         ent.rotate_type = OBJECT_MOVEWALL;
         tempvec = ( ent.absmin + ent.absmax ) * 0.5;
         ent.oldorigin = tempvec - self.oldorigin;
         ent.neworigin = ent.oldorigin;
         ent.owner = self;
         }
      else
         {
         ent.rotate_type = OBJECT_SETORIGIN;
         ent.oldorigin = ent.origin - self.oldorigin;
         ent.neworigin = ent.origin - self.oldorigin;
         }
      ent = find (ent, targetname, self.target);
      }
   };

void( float amount ) SetDamageOnTargets =
   {
   local entity ent;

   ent = find( world, targetname, self.target);
   while( ent )
      {
      if ( ent.classname == "trigger_hurt" )
         {
         hurt_setdamage( ent, amount );
         }
      else if ( ent.classname == "func_movewall" )
         {
         ent.dmg = amount;
         }
      ent = find( ent, targetname, self.target);
      }
   };


//************************************************
//
// Simple continual rotatation
//
//************************************************

void() rotate_entity_think =
   {
   local float t;

   t = time - self.ltime;
   self.ltime = time;

   if ( self.state == STATE_SPEEDINGUP )
      {
      self.count = self.count + self.cnt * t;
      if ( self.count > 1 )
         {
         self.count = 1;
         }

      // get rate of rotation
      t = t * self.count;
      }
   else if ( self.state == STATE_SLOWINGDOWN )
      {
      self.count = self.count - self.cnt * t;
      if ( self.count < 0 )
         {
         RotateTargetsFinal();
         self.state = STATE_INACTIVE;
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

void() rotate_entity_use =
   {
   // change to alternate textures
	self.frame = 1 - self.frame;

   if ( self.state == STATE_ACTIVE )
      {
      if ( self.spawnflags & TOGGLE )
         {
         if ( self.speed )
            {
            self.count = 1;
            self.state = STATE_SLOWINGDOWN;
            }
         else
            {
            self.state = STATE_INACTIVE;
            self.think = SUB_Null;
            }
         }
      }
   else if ( self.state == STATE_INACTIVE )
      {
      self.think = rotate_entity_think;
      self.nextthink = time + 0.02;
      self.ltime = time;
      if ( self.speed )
         {
         self.count = 0;
         self.state = STATE_SPEEDINGUP;
         }
      else
         {
         self.state = STATE_ACTIVE;
         }
      }
   else if ( self.state == STATE_SPEEDINGUP )
      {
      if ( self.spawnflags & TOGGLE )
         {
         self.state = STATE_SLOWINGDOWN;
         }
      }
   else
      {
      self.state = STATE_SPEEDINGUP;
      }
   };

void() rotate_entity_firstthink =
   {
   LinkRotateTargets();
   if ( self.spawnflags & START_ON )
      {
      self.state = STATE_ACTIVE;
      self.think = rotate_entity_think;
      self.nextthink = time + 0.02;
      self.ltime = time;
      }
   else
      {
      self.state = STATE_INACTIVE;
      self.think = SUB_Null;
      }
   self.use = rotate_entity_use;
   };

/*QUAKED func_rotate_entity (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE START_ON
Creates an entity that continually rotates.  Can be toggled on and
off if targeted.

TOGGLE = allows the rotation to be toggled on/off

START_ON = wether the entity is spinning when spawned.  If TOGGLE is 0, entity can be turned on, but not off.

If "deathtype" is set with a string, this is the message that will appear when a player is killed by the train.

"rotate" is the rate to rotate.
"target" is the center of rotation.
"speed"  is how long the entity takes to go from standing still to full speed and vice-versa.
*/

void() func_rotate_entity =
   {
   self.solid    = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;

   setmodel (self, self.model);
   setsize( self, self.mins, self.maxs );

   if ( self.speed != 0 )
      {
      self.cnt = 1 / self.speed;
      }

   self.think = rotate_entity_firstthink;
   self.nextthink = time + 0.1;
   self.ltime = time;
   };

//************************************************
//
// Train with rotation functionality
//
//************************************************

/*QUAKED path_rotate (0.5 0.3 0) (-8 -8 -8) (8 8 8) ROTATION ANGLES STOP NO_ROTATE DAMAGE MOVETIME SET_DAMAGE
 Path for rotate_train.

 ROTATION tells train to rotate at rate specified by "rotate".  Use '0 0 0' to stop rotation.

 ANGLES tells train to rotate to the angles specified by "angles" while traveling to this path_rotate.  Use values < 0 or > 360 to guarantee that it turns in a certain direction.  Having this flag set automatically clears any rotation.

 STOP tells the train to stop and wait to be retriggered.

 NO_ROTATE tells the train to stop rotating when waiting to be triggered.

 DAMAGE tells the train to cause damage based on "dmg".

 MOVETIME tells the train to interpret "speed" as the length of time to take moving from one corner to another.

 SET_DAMAGE tells the train to set all targets damage to "dmg"

 "noise" contains the name of the sound to play when train stops.
 "noise1" contains the name of the sound to play when train moves.
 "event" is a target to trigger when train arrives at path_rotate.
*/
void() path_rotate =
   {
   if ( self.noise )
      {
      precache_sound( self.noise );
      }
   if ( self.noise1 )
      {
      precache_sound( self.noise1 );
      }
   };


void() rotate_train;
void() rotate_train_next;
void() rotate_train_find;

void() rotate_train_think =
   {
   local float t;
   local float timeelapsed;

   t = time - self.ltime;
   self.ltime = time;

   if ( ( self.endtime ) && ( time >= self.endtime ) )
      {
      self.endtime = 0;
      if ( self.state == STATE_MOVE )
         {
         setorigin(self, self.finaldest);
         self.velocity = '0 0 0';
         }

      if (self.think1)
         self.think1();
      }
   else
      {
      timeelapsed = (time - self.cnt) * self.duration;
      if ( timeelapsed > 1 )
         timeelapsed = 1;
      setorigin( self, self.dest1 + ( self.dest2 * timeelapsed ) );
      }

   self.angles = self.angles + ( self.rotate * t );
   self.angles = SUB_NormalizeAngles( self.angles );
   RotateTargets();

   self.nextthink = time + 0.02;
   };

void() rotate_train_use =
   {
   if (self.think1 != rotate_train_find)
		{
		if ( self.velocity != '0 0 0' )
			return;		// already activated
      if ( self.think1 )
         {
         self.think1();
         }
      }
   };

void() rotate_train_wait =
	{
   self.state = STATE_WAIT;

   if ( self.goalentity.noise )
      {
      sound (self, CHAN_VOICE, self.goalentity.noise, 1, ATTN_NORM);
      }
   else
      {
      sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }
   if ( self.goalentity.spawnflags & ANGLES )
      {
      self.rotate = '0 0 0';
      self.angles = self.finalangle;
      }
   if ( self.goalentity.spawnflags & NO_ROTATE )
      {
      self.rotate = '0 0 0';
      }
   self.endtime = self.ltime + self.goalentity.wait;
   self.think1 = rotate_train_next;
   };

void() rotate_train_stop =
	{
   self.state = STATE_STOP;

   if ( self.goalentity.noise )
      {
      sound (self, CHAN_VOICE, self.goalentity.noise, 1, ATTN_NORM);
      }
   else
      {
      sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }
   if ( self.goalentity.spawnflags & ANGLES )
      {
      self.rotate = '0 0 0';
      self.angles = self.finalangle;
      }
   if ( self.goalentity.spawnflags & NO_ROTATE )
      {
      self.rotate = '0 0 0';
      }

   self.dmg = 0;
   self.think1 = rotate_train_next;
   };

void() rotate_train_next =
{
   local entity targ;
   local entity current;
   local vector   vdestdelta;
   local float    len, traveltime, div;
   local string temp;

   self.state = STATE_NEXT;

   current = self.goalentity;
   targ = find (world, targetname, self.path );
   if ( targ.classname != "path_rotate" )
      objerror( "Next target is not path_rotate" );

   if ( self.goalentity.noise1 )
      {
      self.noise1 = self.goalentity.noise1;
      }
   sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);

   self.goalentity = targ;
   self.path = targ.target;
   if (!self.path )
      objerror ("rotate_train_next: no next target");

   if ( targ.spawnflags & STOP )
      {
      self.think1 = rotate_train_stop;
      }
   else if (targ.wait)
		{
      self.think1 = rotate_train_wait;
      }
	else
		{
      self.think1 = rotate_train_next;
		}

   if ( current.event )
      {
      // Trigger any events that should happen at the corner.
      temp = self.target;
      self.target = current.event;
      self.message = current.message;
      SUB_UseTargets();
      self.target = temp;
      self.message = string_null;
      }

   if ( current.spawnflags & ANGLES )
      {
      self.rotate = '0 0 0';
      self.angles = self.finalangle;
      }

   if ( current.spawnflags & ROTATION )
      {
      self.rotate = current.rotate;
      }

   if ( current.spawnflags & DAMAGE )
      {
      self.dmg = current.dmg;
      }

   if ( current.spawnflags & SET_DAMAGE )
      {
      SetDamageOnTargets( current.dmg );
      }

   if ( current.speed == -1 )
		{
		// Warp to the next path_corner
      setorigin( self, targ.origin );
      self.endtime = self.ltime + 0.01;
      SetTargetOrigin();

      if ( targ.spawnflags & ANGLES )
         {
         self.angles = targ.angles;
         }

      self.duration = 1;         // 1 / duration
      self.cnt = time;           // start time
      self.dest2 = '0 0 0';      // delta
      self.dest1 = self.origin;  // original position
      self.finaldest = self.origin;
      }
	else
		{
      self.state = STATE_MOVE;

      self.finaldest = targ.origin;
      if (self.finaldest == self.origin)
         {
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

      if ( current.spawnflags & MOVETIME )
         {
         traveltime = current.speed;
         }
      else
         {
         // check if there's a speed change
         if (current.speed>0)
            self.speed = current.speed;

         if (!self.speed)
            objerror("No speed is defined!");

         // divide by speed to get time to reach dest
         traveltime = len / self.speed;
         }

      if (traveltime < 0.1)
         {
         self.velocity = '0 0 0';
         self.endtime = self.ltime + 0.1;
         if ( targ.spawnflags & ANGLES )
            {
            self.angles = targ.angles;
            }
         return;
         }

      // qcc won't take vec/float
      div = 1 / traveltime;

      if ( targ.spawnflags & ANGLES )
         {
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

void() rotate_train_find =
   {
   local entity targ;

   self.state = STATE_FIND;

   LinkRotateTargets();

   // the first target is the point of rotation.
   // the second target is the path.
   targ = find ( world, targetname, self.path);
   if ( targ.classname != "path_rotate" )
      objerror( "Next target is not path_rotate" );

   // Save the current entity
   self.goalentity = targ;

   if ( targ.spawnflags & ANGLES )
      {
      self.angles = targ.angles;
      self.finalangle = SUB_NormalizeAngles( targ.angles );
      }

   self.path = targ.target;
   setorigin (self, targ.origin );
   SetTargetOrigin();
   RotateTargetsFinal();
   self.think1 = rotate_train_next;
   if (!self.targetname)
      {
      // not triggered, so start immediately
      self.endtime = self.ltime + 0.1;
      }
   else
      {
      self.endtime = 0;
      }

   self.duration = 1;        // 1 / duration
   self.cnt = time;          // start time
   self.dest2 = '0 0 0';     // delta
   self.dest1 = self.origin; // original position
   };

/*QUAKED func_rotate_train (0 .5 .8) (-8 -8 -8) (8 8 8)
In path_rotate, set speed to be the new speed of the train after it reaches
the path change.  If speed is -1, the train will warp directly to the next
path change after the specified wait time.  If MOVETIME is set on the
path_rotate, the train to interprets "speed" as the length of time to
take moving from one corner to another.

"noise" contains the name of the sound to play when train stops.
"noise1" contains the name of the sound to play when train moves.
Both "noise" and "noise1" defaults depend upon "sounds" variable and
can be overridden by the "noise" and "noise1" variable in path_rotate.

Also in path_rotate, if STOP is set, the train will wait until it is
retriggered before moving on to the next goal.

Trains are moving platforms that players can ride.
"path" specifies the first path_rotate and is the starting position.
If the train is the target of a button or trigger, it will not begin moving until activated.
The func_rotate_train entity is the center of rotation of all objects targeted by it.

If "deathtype" is set with a string, this is the message that will appear when a player is killed by the train.

speed	default 100
dmg      default  0
sounds
1) ratchet metal
*/

void() rotate_train =
   {
   objerror ("rotate_train entities should be changed to rotate_object with\nfunc_rotate_train controllers\n");
   };

void() func_rotate_train =
   {
   if (!self.speed)
		self.speed = 100;
	if (!self.target)
      objerror ("rotate_train without a target");

   if ( !self.noise )
      {
      if (self.sounds == 0)
         {
         self.noise = ("misc/null.wav");
         }

      if (self.sounds == 1)
         {
         self.noise = ("plats/train2.wav");
         }
      }
   if ( !self.noise1 )
      {
      if (self.sounds == 0)
         {
         self.noise1 = ("misc/null.wav");
         }
      if (self.sounds == 1)
         {
         self.noise1 = ("plats/train1.wav");
         }
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

// start trains on the second frame, to make sure their targets have had
// a chance to spawn
   self.ltime = time;
	self.nextthink = self.ltime + 0.1;
   self.endtime = self.ltime + 0.1;
   self.think = rotate_train_think;
   self.think1 = rotate_train_find;
   self.state = STATE_FIND;

   self.duration = 1;        // 1 / duration
   self.cnt = 0.1;           // start time
   self.dest2 = '0 0 0';     // delta
   self.dest1 = self.origin; // original position


   self.flags = self.flags | FL_ONGROUND;
   };

//************************************************
//
// Moving clip walls
//
//************************************************

void() rotate_door_reversedirection;
void() rotate_door_group_reversedirection;

void() movewall_touch =
   {
   if (time < self.owner.attack_finished)
		return;

   if ( self.dmg )
      {
      T_Damage (other, self, self.owner, self.dmg);
      self.owner.attack_finished = time + 0.5;
      }
   else if ( self.owner.dmg )
      {
      T_Damage (other, self, self.owner, self.owner.dmg);
      self.owner.attack_finished = time + 0.5;
      }
   };

void() movewall_blocked =
   {
   local entity temp;

   if (time < self.owner.attack_finished)
		return;

   self.owner.attack_finished = time + 0.5;

   if ( self.owner.classname == "func_rotate_door" )
      {
      temp = self;
      self = self.owner;
      rotate_door_group_reversedirection();
      self = temp;
      }

   if ( self.dmg )
      {
      T_Damage (other, self, self.owner, self.dmg);
      self.owner.attack_finished = time + 0.5;
      }
   else if ( self.owner.dmg )
      {
      T_Damage (other, self, self.owner, self.owner.dmg);
      self.owner.attack_finished = time + 0.5;
      }
   };

void() movewall_think =
   {
   self.ltime = time;
   self.nextthink = time + 0.02;
   };

/*QUAKED func_movewall (0 .5 .8) ? VISIBLE TOUCH NONBLOCKING
Used to emulate collision on rotating objects.

VISIBLE causes brush to be displayed.

TOUCH specifies whether to cause damage when touched by player.

NONBLOCKING makes the brush non-solid.  This is useless if VISIBLE is set.

"dmg" specifies the damage to cause when touched or blocked.
*/
void() func_movewall =
   {
   self.angles = '0 0 0';
   self.movetype = MOVETYPE_PUSH;
   if ( self.spawnflags & NONBLOCKING )
      {
      self.solid = SOLID_NOT;
      }
   else
      {
      self.solid = SOLID_BSP;
      self.blocked = movewall_blocked;
      }
   if ( self.spawnflags & TOUCH )
      {
      self.touch = movewall_touch;
      }
   setmodel (self,self.model);
   if ( !( self.spawnflags & VISIBLE ) )
      {
      self.model = string_null;
      }
   self.think = movewall_think;
   self.nextthink = time + 0.02;
   self.ltime = time;
   };

/*QUAKED rotate_object (0 .5 .8) ?
This defines an object to be rotated.  Used as the target of func_rotate_door.
*/
void() rotate_object =
   {
   self.classname = "rotate_object";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   setmodel (self,self.model);
   setsize( self, self.mins, self.maxs );
   self.think = SUB_Null;
   };

//************************************************
//
// Rotating doors
//
//************************************************

void() rotate_door_think2 =
   {
   local float t;

   t = time - self.ltime;
   self.ltime = time;

   // change to alternate textures
   self.frame = 1 - self.frame;

   self.angles = self.dest;

   if ( self.state == STATE_OPENING )
      {
      self.state = STATE_OPEN;
      }
   else
      {
      if ( self.spawnflags & STAYOPEN )
         {
         rotate_door_group_reversedirection();
         return;
         }
      self.state = STATE_CLOSED;
      }

   sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
   self.think = SUB_Null;

   RotateTargetsFinal();
   };

void() rotate_door_think =
   {
   local float t;

   t = time - self.ltime;
   self.ltime = time;

   if ( time < self.endtime )
      {
      self.angles = self.angles + ( self.rotate * t );
      RotateTargets();
      }
   else
      {
      self.angles = self.dest;
      RotateTargets();
      self.think = rotate_door_think2;
      }

   self.nextthink = time + 0.01;
   };

void() rotate_door_reversedirection =
   {
   local vector start;

   // change to alternate textures
	self.frame = 1 - self.frame;

   if ( self.state == STATE_CLOSING )
      {
      start = self.dest1;
      self.dest = self.dest2;
      self.state = STATE_OPENING;
      }
   else
      {
      start = self.dest2;
      self.dest = self.dest1;
      self.state = STATE_CLOSING;
      }

   sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);

   self.rotate = ( self.dest - start ) * ( 1 / self.speed );
   self.think = rotate_door_think;
   self.nextthink = time + 0.02;
   self.endtime = time + self.speed - ( self.endtime - time );
   self.ltime = time;
   };

void() rotate_door_group_reversedirection =
   {
   local string name;

   // tell all associated rotaters to reverse direction
   if ( self.group )
      {
      name = self.group;
      self = find( world, group, name);
      while( self )
         {
         rotate_door_reversedirection();
         self = find( self, group, name);
         }
      }
   else
      {
      rotate_door_reversedirection();
      }
   };

void() rotate_door_use =
   {
   local entity t;
   local vector start;

   if ( ( self.state != STATE_OPEN ) && ( self.state != STATE_CLOSED ) )
      return;

   if ( !self.cnt )
      {
      self.cnt = 1;
      LinkRotateTargets();
      }

   // change to alternate textures
	self.frame = 1 - self.frame;

   if ( self.state == STATE_CLOSED )
      {
      start = self.dest1;
      self.dest = self.dest2;
      self.state = STATE_OPENING;
      }
   else
      {
      start = self.dest2;
      self.dest = self.dest1;
      self.state = STATE_CLOSING;
      }

   sound(self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);

   self.rotate = ( self.dest - start ) * ( 1 / self.speed );
   self.think = rotate_door_think;
   self.nextthink = time + 0.01;
   self.endtime = time + self.speed;
   self.ltime = time;
   };


/*QUAKED func_rotate_door (0 .5 .8) (-8 -8 -8) (8 8 8) STAYOPEN
Creates a door that rotates between two positions around a point of
rotation each time it's triggered.

STAYOPEN tells the door to reopen after closing.  This prevents a trigger-
once door from closing again when it's blocked.

"dmg" specifies the damage to cause when blocked.  Defaults to 2.  Negative numbers indicate no damage.
"speed" specifies how the time it takes to rotate

"sounds"
1) medieval (default)
2) metal
3) base
*/

void() func_rotate_door =
   {
   if (!self.target)
      {
      objerror( "rotate_door without target." );
      }

   self.dest1 = '0 0 0';
   self.dest2 = self.angles;
   self.angles = self.dest1;

   // default to 2 seconds
   if ( !self.speed )
      {
      self.speed = 2;
      }

   self.cnt = 0;

   if (!self.dmg)
		self.dmg = 2;
   else if ( self.dmg < 0 )
      {
      self.dmg = 0;
      }

   if (self.sounds == 0)
      self.sounds = 1;
	if (self.sounds == 1)
	{
		precache_sound ("doors/latch2.wav");
		precache_sound ("doors/winch2.wav");
		precache_sound ("doors/drclos4.wav");
		self.noise1 = "doors/latch2.wav";
		self.noise2 = "doors/winch2.wav";
		self.noise3 = "doors/drclos4.wav";
	}
	if (self.sounds == 2)
	{
		precache_sound ("doors/airdoor1.wav");
		precache_sound ("doors/airdoor2.wav");
		self.noise2 = "doors/airdoor1.wav";
		self.noise1 = "doors/airdoor2.wav";
		self.noise3 = "doors/airdoor2.wav";
	}
	if (self.sounds == 3)
	{
		precache_sound ("doors/basesec1.wav");
		precache_sound ("doors/basesec2.wav");
		self.noise2 = "doors/basesec1.wav";
		self.noise1 = "doors/basesec2.wav";
		self.noise3 = "doors/basesec2.wav";
	}

   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   setmodel (self, self.model);
   setorigin( self, self.origin );
   setsize( self, self.mins, self.maxs );
   self.state = STATE_CLOSED;
   self.use = rotate_door_use;
   self.think = SUB_Null;
   };
