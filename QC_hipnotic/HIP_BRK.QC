/* Breakaway walls QuickC program
   By Jim Dose'  9/11/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

float MULTI_USE = 1;
float INVISIBLE = 2;

void() damagethreshold_killed =
	{
	self.health = self.max_health;

//   self.solid = SOLID_NOT;
   activator = damage_attacker;
	self.takedamage = DAMAGE_NO;
	SUB_UseTargets ();
	self.takedamage = DAMAGE_YES;

	if ( !( self.spawnflags & MULTI_USE ) )
	   {
	   remove( self );
	   }
	};

void() damagethreshold_pain =
	{
	self.health = self.max_health;
	};

/*QUAKED trigger_damagethreshold (0 .5 .8) ? MULTI_USE INVISIBLE
Triggers only when a threshold of damage is exceeded.
When used in conjunction with func_breakawaywall, allows
walls that may be destroyed with a rocket blast.

MULTI_USE tells the trigger to not to remove itself after
being fired.  Allows the trigger to be used multiple times.

INVISIBLE tells the trigger to not be visible.

"health" specifies how much damage must occur before trigger fires.
Default is 60.

*/

void() trigger_damagethreshold =

{
	self.mangle = self.angles;
	self.angles = '0 0 0';

	self.classname = "damagethreshold";
   self.solid = SOLID_BSP;
   self.movetype = MOVETYPE_PUSH;
   setorigin (self, self.origin);
	setmodel (self, self.model);
	setsize (self, self.mins , self.maxs);
   if ( self.spawnflags & INVISIBLE )
      {
      self.model = string_null;
      }

	if (!self.health)
		{
		self.health = 60;
		}
	self.max_health = self.health;
	self.takedamage = DAMAGE_YES;

	self.blocked = SUB_Null;
	self.th_pain = damagethreshold_pain;
	self.th_die  = damagethreshold_killed;
	};

/*QUAKED func_breakawaywall (0 .5 .8) ?
Special walltype that removes itself when triggered.
*/

void() func_breakawaywall =
	{
	self.mangle = self.angles;
	self.angles = '0 0 0';

	self.classname = "breakaway";
	self.solid = SOLID_BSP;
	self.movetype = MOVETYPE_PUSH;
	setorigin (self, self.origin);
	setmodel (self, self.model);
	setsize (self, self.mins , self.maxs);
	self.use = SUB_Remove;
	};
