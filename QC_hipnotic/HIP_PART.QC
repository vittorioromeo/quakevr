/* Particle effects QuickC program
   By Jim Dose'  9/19/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

//float START_OFF = 1;
float USE_COUNT = 1;

void () particlefield_XZ =
	{
	local vector pos;
   local vector start;
   local vector end;

   if ( ( self.spawnflags & USE_COUNT ) &&
      ( counter_GetCount( other ) != self.cnt ) )
      {
      return;
      }
//   dprint( "XZ\n" );

   self.ltime = time + 0.25;
   if ( self.noise )
      {
      sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }

   // Only show particles if client is visible.
   // This helps to keep network traffic down to a minimum.
   if (!checkclient() )
      return;

   start = self.dest1 + self.origin;
   end   = self.dest2 + self.origin;
   pos_y = start_y;
   pos_z = start_z;
   while( pos_z <= end_z )
		{
      pos_x = start_x;
      while( pos_x <= end_x )
			{
         particle ( pos, '0 0 0', self.color, self.count );
         pos_x = pos_x + 16;
			}
      pos_z = pos_z + 16;
		}
	};

void () particlefield_YZ =
	{
	local vector pos;
   local vector start;
   local vector end;

   if ( ( self.spawnflags & USE_COUNT ) &&
      ( counter_GetCount( other ) != self.cnt ) )
      {
      return;
      }

//   dprint( "YZ: " );
//   dprint( vtos( self.dest1 ) );
//   dprint( " - " );
//   dprint( vtos( self.dest2 ) );
//   dprint( "\n" );
   self.ltime = time + 0.25;
   if ( self.noise )
      {
      sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }

   // Only show particles if client is visible.
   // This helps to keep network traffic down to a minimum.
   if (!checkclient() )
      return;

   start = self.dest1 + self.origin;
   end   = self.dest2 + self.origin;
   pos_x = start_x;
   pos_z = start_z;
   while( pos_z < end_z )
		{
      pos_y = start_y;
      while( pos_y < end_y )
			{
         particle ( pos, '0 0 0', self.color, self.count );
			pos_y = pos_y + 16;
			}
		pos_z = pos_z + 16;
		}
	};

void () particlefield_XY =
	{
	local vector pos;
   local vector start;
   local vector end;

   if ( ( self.spawnflags & USE_COUNT ) &&
      ( counter_GetCount( other ) != self.cnt ) )
      {
      return;
      }

//   dprint( "XY\n" );
   self.ltime = time + 0.25;
   if ( self.noise )
      {
      sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }

   // Only show particles if client is visible.
   // This helps to keep network traffic down to a minimum.
   if (!checkclient() )
      return;


   start = self.dest1 + self.origin;
   end   = self.dest2 + self.origin;
   pos_x = start_x;
   pos_z = start_z;
   while( pos_x < end_x )
		{
      pos_y = start_y;
      while( pos_y < end_y )
			{
         particle ( pos, '0 0 0', self.color, self.count );
			pos_y = pos_y + 16;
			}
		pos_x = pos_x + 16;
		}
	};

void () particlefield_touch =
	{
   if ( !self.dmg )
      return;

   if ( time > self.ltime)
      return;

	if (time < self.attack_finished)
		return;
	self.attack_finished = time + 0.5;
	T_Damage (other, self, self, self.dmg);
   };

/*QUAKED func_particlefield (0 .5 .8) ? USE_COUNT
Creates a brief particle flash roughly the size of the defining
brush each time it is triggered.

USE_COUNT when the activator is a func_counter, the field will only
   activate when count is equal to "cnt".  Same as using a func_oncount
   to trigger.

"cnt" is the count to activate on when USE_COUNT is set.
"color" is the color of the particles.  Default is 192 (yellow).
"count" is the density of the particles.  Default is 2.
"noise" is the sound to play when triggered.  Do not use a looping sound here.
"dmg" is the amount of damage to cause when touched.
*/

void() func_particlefield =
	{
   if ( !self.color )
	   {
      self.color = 192;
	   }
   if ( self.count == 0 )
	   {
      self.count = 2;
	   }
   self.classname = "particlefield";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   setmodel (self, self.model);
   self.model = string_null;

   self.origin = ( self.mins + self.maxs ) * 0.5;
   setorigin (self, self.origin);
   self.dest = self.maxs - self.mins - '16 16 16';
   self.dest1 = self.mins + '8 8 8' - self.origin;
   self.dest2 = self.maxs + '7.9 7.9 7.9' - self.origin;
   setsize (self, self.mins, self.maxs);
   self.touch = particlefield_touch;
//   dprint( vtos( self.dest ) );
//   dprint( "  " );
   if ( self.dest_x > self.dest_z )
		{
      if ( self.dest_y > self.dest_z )
			{
//         dprint( "XY1 - " );
//         dprint( ftos( self.cnt ) );
//         dprint( "\n" );
         self.use = particlefield_XY;
         self.dest1_z = ( self.dest1_z + self.dest2_z ) / 2;
			}
		else
			{
//         dprint( "XZ1 - " );
//         dprint( ftos( self.cnt ) );
//         dprint( "\n" );
         self.use = particlefield_XZ;
         self.dest1_y = ( self.dest1_y + self.dest2_y ) / 2;
			}
		}
	else
		{
      if ( self.dest_y > self.dest_x )
         {
//         dprint( "YZ2 - " );
//         dprint( ftos( self.cnt ) );
//         dprint( "\n" );
         self.use = particlefield_YZ;
         self.dest1_x = ( self.dest1_x + self.dest2_x ) / 2;
			}
		else
			{
//         dprint( "XZ2 - " );
//         dprint( ftos( self.cnt ) );
//         dprint( "\n" );
         self.use = particlefield_XZ;
         self.dest1_y = ( self.dest1_y + self.dest2_y ) / 2;
			}
		}

   if ( self.noise )
      {
      precache_sound( self.noise );
      }
   self.ltime = time;
   };

void () blocker_touch =
	{
   if ( !self.dmg )
      return;

	if (time < self.attack_finished)
		return;
	self.attack_finished = time + 0.5;
	T_Damage (other, self, self, self.dmg);
   };

void () blocker_use =
	{
   if ( !self.state )
      {
      self.state = 1;
      setorigin( self, self.origin - '8000 8000 8000' );
      sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
      }
   else
      {
      self.state = 0;
      setorigin( self, self.origin + '8000 8000 8000' );
      sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
      }
   };

/*QUAKED func_togglewall (0 .5 .8) ? START_OFF
Creates a invisible wall that can be toggled on and off.

START_OFF wall doesn't block until triggered.

"noise" is the sound to play when wall is turned off.
"noise1" is the sound to play when wall is blocking.
"dmg" is the amount of damage to cause when touched.
*/

void() func_togglewall =
	{
   self.classname = "togglewall";
   self.movetype = MOVETYPE_PUSH;
   self.mdl = self.model;
   setmodel (self, self.model);
   setsize (self, self.mins, self.maxs);
   setorigin (self, self.origin);
   self.touch = blocker_touch;
   self.use = blocker_use;
   if ( !self.noise )
      {
      self.noise = ("misc/null.wav");
      }
   if ( !self.noise1 )
      {
      self.noise1 = ("misc/null.wav");
      }

   precache_sound( self.noise );
   precache_sound( self.noise1 );

   self.solid = SOLID_BSP;
   self.model = string_null;
   if ( self.spawnflags & START_OFF )
      {
      self.state = 0;
      setorigin( self, self.origin + '8000 8000 8000' );
      }
   else
      {
      self.state = 1;
      sound(self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);
      }
   };
