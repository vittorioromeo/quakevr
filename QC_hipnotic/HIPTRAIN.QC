/* Train QuickC program
   By Jim Dose'  9/11/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() train_blocked;
void() func_train;
void() hip_train_next;
void() hip_func_train_find;

void() hip_train_use =
{
	if (self.think != hip_func_train_find)
		{
		if ( self.velocity != '0 0 0' )
			return;		// already activated
		}
	hip_train_next();
};

void() hip_train_wait =
	{
	if (self.wait)
		{
      sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      if ( self.wait != -1 )
			{
			self.nextthink = self.ltime + self.wait;
			self.wait = 0;
         }
		}
	else
		self.nextthink = self.ltime + 0.1;

	self.think = hip_train_next;
	};


void() hip_train_next =
{
	local entity	targ;
	local float		current;
   local string   temp;

	// Get the speed of the current path_corner.
	// (we must save this off at each path change since
	// we don't have a pointer to the current path_corner).
	current = self.cnt;

	targ = find (world, targetname, self.target);

	// Save the speed in cnt for later use
	self.cnt = targ.speed;
	self.target = targ.target;
	if (!self.target)
		objerror ("hip_train_next: no next target");

	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);

	self.wait = targ.wait;
 	if (targ.wait)
		{
		self.think = hip_train_wait;
		}
	else
		{
		self.think = hip_train_next;
		}

   if ( self.goalentity.event )
      {
      // Trigger any events that should happen at the corner.
      temp = self.target;
      self.target = self.goalentity.event;
      self.message = self.goalentity.message;
      SUB_UseTargets();
      self.target = temp;
      self.message = string_null;
      }

   // Save the current entity
   self.goalentity = targ;

	if ( current == -1 )
		{
		// Warp to the next path_corner
      setorigin (self, targ.origin - self.mins );
		self.nextthink = self.ltime + 0.01;
		}
	else
		{
		// check if there's a speed change
		if (current>0)
	    	self.speed = current;

		// travel to the next path change
		SUB_CalcMove (targ.origin - self.mins, self.speed, self.think );
		}
	};

void() hip_func_train_find =

{
	local entity	targ;

	targ = find (world, targetname, self.target);

   // Save the current entity
   self.goalentity = targ;

   // Save the speed in cnt for later use
	self.cnt = targ.speed;

	self.target = targ.target;
	setorigin (self, targ.origin - self.mins);
	if (!self.targetname)
	{	// not triggered, so start immediately
		self.nextthink = self.ltime + 0.1;
		self.think = hip_train_next;
	}
};

/*QUAKED func_train2 (0 .5 .8) ?
This is a modification of the standard func_train entity.
It is functionally equivalent, except that it removes a slight delay that
would occur after each path entry, and it adds a speed variable to the
path_corner entity.

"noise" contains the name of the sound to play when train stops.
"noise1" contains the name of the sound to play when train moves.
Both "noise" and "noise1" defaults depend upon "sounds" variable.

In path_corner, set speed to be the new speed of the train after it reaches
the path change.  If speed is -1, the train will warp directly to the next
path change after the specified wait time.

Also in path_corner, if wait is set to -1, the train will wait until it is
retriggered before moving on to the next goal.

Here is a reiteration of the func_train docs:

Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal

*/
void() func_train2 =
{
	if (!self.speed)
		self.speed = 100;
	if (!self.target)
		objerror ("func_train without a target");
	if (!self.dmg)
		self.dmg = 2;

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
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	self.blocked = train_blocked;
   self.use = hip_train_use;
	self.classname = "train2";

	setmodel (self, self.model);
	setsize (self, self.mins , self.maxs);
	setorigin (self, self.origin);

// start trains on the second frame, to make sure their targets have had
// a chance to spawn
	self.nextthink = self.ltime + 0.1;
   self.think = hip_func_train_find;
};
