/* Miscelanneous QuickC program
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() play_sound_use =
   {
   if (self.spawnflags & 1)
      {
      if (self.state == 0)
         {
         self.state = 1;
         sound (self, self.impulse, self.noise, self.volume, self.speed);
         }
      else
         {
         self.state = 0;
         sound (self, self.impulse, "misc/null.wav", self.volume, self.speed);
         }
      }
   else
      {
      sound (self, self.impulse, self.noise, self.volume, self.speed);
      }
   };

void() PlaySoundThink =
   {
   local float t;
   t = self.wait * random();
   if (t < self.delay)
      t = self.delay;
   self.nextthink = time + t;
   play_sound_use();
   };

/*QUAKED play_sound_triggered (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) toggle
play a sound when it is used
"toggle" determines whether sound should be stopped when triggered again
"volume" how loud (1 default full volume)
"noise" sound to play
"impulse" channel on which to play sound (0-7) (0 automatic is default)
"speed" attenuation factor
   -1 - no attenuation
    1 - normal
    2 - idle
    3 - static
*/
void() play_sound_triggered =
   {
   precache_sound (self.noise);
   precache_sound ("misc/null.wav");
   if (self.volume == 0)
      self.volume = 1;
   if (self.speed == 0)
      self.speed = 1;
   if (self.speed == -1)
      self.speed = 0;
   if (self.spawnflags & 1)
      if (self.impulse == 0)
         self.impulse = 7;
   self.use = play_sound_use;
   };

/*QUAKED play_sound (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
play a sound on a periodic basis
"volume" how loud (1 default full volume)
"noise" sound to play
"wait" random time between sounds (default 20)
"delay" minimum delay between sounds (default 2)
"impulse" channel on which to play sound (0-7) (0 automatic is default)
"speed" attenuation factor
   -1 - no attenuation
    1 - normal
    2 - idle
    3 - static
*/
void() play_sound =
   {
   local float t;

   play_sound_triggered();
   if (self.wait == 0)
      self.wait = 20;
   if (self.delay == 0)
      self.delay = 2;
   self.think = PlaySoundThink;
   t = self.wait * random();
   if (t < self.delay)
      t = self.delay;
   self.nextthink = time + t;
   };

/*QUAKED random_thunder (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
"wait" random time between strikes (default 20)
"delay" minimum delay between strikes (default 2)
"volume" how loud (1 default full volume)
"speed" attenuation factor
   -1 - no attenuation
    1 - normal
    2 - idle
    3 - static
*/
void() random_thunder =
   {
   self.noise = "ambience/thunder1.wav";
   play_sound();
   self.impulse = 6;
   };

/*QUAKED random_thunder_triggered (0.3 0.1 0.6) (-10 -10 -8) (10 10 8) toggle
"toggle" determines whether sound should be stopped when triggered again
"volume" how loud (1 default full volume)
"speed" attenuation factor
   -1 - no attenuation
    1 - normal
    2 - idle
    3 - static
*/
void() random_thunder_triggered =
   {
   self.noise = "ambience/thunder1.wav";
   play_sound_triggered();
   self.impulse = 6;
   };

/*QUAKED ambient_humming (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_humming =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/humming.wav");
   ambientsound (self.origin, "ambient/humming.wav", self.volume, ATTN_STATIC);
};

/*QUAKED ambient_rushing (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_rushing =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/rushing.wav");
   ambientsound (self.origin, "ambient/rushing.wav", self.volume, ATTN_STATIC);
};

/*QUAKED ambient_running_water (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_running_water =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/runwater.wav");
   ambientsound (self.origin, "ambient/runwater.wav", self.volume, ATTN_STATIC);
};

/*QUAKED ambient_fan_blowing (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_fan_blowing =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/fanblow.wav");
   ambientsound (self.origin, "ambient/fanblow.wav", self.volume, ATTN_STATIC);
};

/*QUAKED ambient_waterfall (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_waterfall =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/waterfal.wav");
   ambientsound (self.origin, "ambient/waterfal.wav", self.volume, ATTN_STATIC);
};

/*QUAKED ambient_riftpower (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
  "volume" how loud it should be (0.5 is default)
*/
void() ambient_riftpower =
{
   if (self.volume==0)
      self.volume = 0.5;
   precache_sound ("ambient/riftpowr.wav");
   ambientsound (self.origin, "ambient/riftpowr.wav", self.volume, ATTN_STATIC);
};


/*QUAKED info_command (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
 Stuffs a command into the console to allow map designers
 to set server variables.

 "message" is the command to send to the console.
*/

void() info_command =
   {
   if ( self.message )
      localcmd( self.message );
   };

void() effect_teleport_use =
   {
   WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TELEPORT);
   WriteCoord (MSG_BROADCAST, self.origin_x);
   WriteCoord (MSG_BROADCAST, self.origin_y);
   WriteCoord (MSG_BROADCAST, self.origin_z);

   sound (self, CHAN_VOICE, "misc/r_tele1.wav", 1, ATTN_NORM);
   };

/*QUAKED effect_teleport (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
 Create a teleport effect when triggered
*/

void() effect_teleport =
   {
   precache_sound("misc/r_tele1.wav");
   self.use = effect_teleport_use;
   };


/*
=============
camerathink

camera think function
==============
*/
/*
void() camerathink =
   {
   local entity   pl;
   local vector   d_diff;
   local vector   a_diff;
   local vector   p;
   local vector   a;
   local float    dist;
   local float    timeelapsed;

   timeelapsed = (time - self.cnt) * self.duration;
   if ( timeelapsed > 1 )
      timeelapsed = 1;

   p = self.oldorigin + (timeelapsed * (self.movetarget.origin - self.oldorigin));
   d_diff = p - self.origin;

   a = self.mangle + (timeelapsed * (self.movetarget.mangle - self.mangle));
   a_diff = a - self.angles;

   self.origin = self.origin + self.pos1 + (1.0 * d_diff);
   self.angles = self.v_angle = self.angles + self.avelocity + (1.0 * a_diff);

   self.nextthink = time + 0.1;
   };
*/
/*
=============
camerachase

camera chase function
==============
*/
/*
void() camerachase =
   {
   local float    timeelapsed;
   local vector v;
   local vector a;


   timeelapsed = time - self.cnt;
   if ( timeelapsed > 1 )
      timeelapsed = 1;
   self.cnt = time;

   self.velocity = (self.movetarget.origin - self.oldorigin)*(1/timeelapsed);
   self.oldorigin = self.movetarget.origin;
   if (self.state == 0) // setup
      {
      self.velocity = '0 0 0';
      self.cnt = time;
      }
   // calculate new camera position
   a = '0 0 0';
   a_y = vectoyaw( normalize( self.movetarget.origin - self.movetarget.movetarget.origin) );
   makevectors(a);
   self.origin = self.movetarget.origin + v_forward * self.distance;
   self.origin_z = self.origin_z + self.height;

   v = normalize( self.movetarget.origin - self.origin );
   a = self.angles;
   a_y = vectoyaw(v);
   v_x = v_y;
   v_y = 0 - v_z;
   a_x = vectoyaw(v);
   self.angles = a;
//   SetViewPoint(self, self);
//   UpdateCamera(self, self);
   SetViewAngle(self, a);

   self.think = camerachase;
   self.nextthink = time + 0.1;
   };
*/

/*
=============
camerafollow

camera follow function
==============
*/
/*
void() camerafollow =
   {
   local vector v;
   local vector a;

   v = normalize( self.movetarget.origin - self.origin );
   a = self.angles;
   a_y = vectoyaw(v);
   v_x = v_y;
   v_y = 0 - v_z;
   a_x = vectoyaw(v);
   self.angles = a;
   SetViewAngle(self, a);

   self.think = camerafollow;
   self.nextthink = time + 0.01;
   };
*/
/*
=============
t_cameratarget

a camera has reached a path trigger
==============
*/
/*
void() t_cameratarget =
{
local entity	temp;

	if (other.movetarget != self)
		return;

	temp = self;
	self = other;
	other = temp;

   self.angles = self.movetarget.mangle;
   self.movetarget = find (world, targetname, other.target);
   if (!self.movetarget)
      {
      self.nextthink = time + 999999;
      }
   else
      {
      local vector   d_diff;
      local vector   a_diff;
      local float    t;

      self.think = camerathink;
      self.duration = 1 / other.duration;
      self.cnt = time;

      self.oldorigin = self.origin;
      d_diff = self.movetarget.origin - self.oldorigin;
      self.pos1 = d_diff*self.duration;

      self.mangles = self.angles;
      a_diff = self.movetarget.mangle - self.mangle;
      self.avelocity = a_diff*self.duration;
      if (other.delay)
         {
         self.nextthink = time + other.delay;
         self.cnt = time + other.delay;
         }
      }
};
*/

/*QUAKED path_camera (0.5 0.3 0) (-8 -8 -8) (8 8 8)
path for a camera
"delay" delay to wait before proceeding to next segment;
"mangle" position the camera should be when it gets here
"duration" how long it should take to cover the distance
           (default 10 seconds)
*/
/*
void() path_camera =
{
	self.solid = SOLID_TRIGGER;
   self.touch = t_cameratarget;
   if (!self.duration)
      self.duration = 10;
	setsize (self, '-8 -8 -8', '8 8 8');
};
*/
float SVC_UPDATEENTITY = 128; // Net.Protocol 0x80
void(entity me, entity camera) UpdateCamera =
{
  msg_entity = me;                         // target of message
  WriteByte (MSG_ONE, SVC_UPDATEENTITY|15); // 0x80|1|2|4|8
  WriteByte (MSG_ONE, 64); // 0x40
  WriteEntity(MSG_ONE,camera);
  WriteCoord(MSG_ONE,camera.origin_x);
  WriteCoord(MSG_ONE,camera.origin_y);
  WriteCoord(MSG_ONE,camera.origin_z);
};


//void() effect_finale_think =
//   {
//   self.flags = self.flags - (self.flags & FL_ONGROUND);
//   self.angles = self.mangle;
//   self.v_angle = self.mangle;
//   self.fixangle = TRUE;
//   NezuUpdateCamera(self,self.trigger_field);
//   NezuSetViewPoint(self,self.trigger_field);
//   NezuSetViewAngle(self,self.mangle);
//   self.think = effect_finale_think;
//   self.nextthink = time + 0.05;
//   };


void() effect_finale_use =
{
   local entity   pos, pl, targ;
   local entity   temp;

   if (self.state == 1)
      return;

//   intermission_exittime = time + 10000000;  // never allow exit
//   intermission_running = 1;
   self.state = 1;

	// find the intermission spot
   pos = find (world, targetname, self.target);
   if (!pos)
      error ("no target in finale");

//   WriteByte (MSG_ALL, SVC_FINALE);
   WriteByte (MSG_ALL, SVC_CUTSCENE);
   WriteString (MSG_ALL, "");

   //setup decoy
   if (!self.spawnflags & 2)
      {
      if (self.spawnflags & 1)
         {
         pl = find (world, classname, "player");
         targ = find (world, targetname, self.mdl);
         become_decoy( targ.target, pl.origin );
         }
      else
         {
         targ = find (world, targetname, self.mdl);
         become_decoy( targ.target, targ.origin );
         }
      }

	pl = find (world, classname, "player");
	while (pl != world)
	{
      pl.view_ofs = '0 0 0';
		pl.angles = other.v_angle = pos.mangle;
      pl.mangle = pos.mangle;
      pl.fixangle = TRUE;     // turn this way immediately
      pl.trigger_field = self;
      pl.nextthink = time + 0.5;
      pl.takedamage = DAMAGE_NO;
		pl.solid = SOLID_NOT;
      pl.movetype = MOVETYPE_NONE;
		pl.modelindex = 0;
      setorigin (pl, pos.origin);
      pl.origin = pos.origin;
      pl.oldorigin = pos.origin;
      UpdateCamera(pl,pl);
      UpdateCamera(pl,pl.trigger_field);
      pl = find (pl, classname, "player");
	}

   // wait for next sequence
   if (self.spawnfunction)
      {
      self.nextthink = time + self.wait;
      self.think = self.spawnfunction;
      }
//   intermission_running = 0;
};
/*
 "count" 0 - static camera
       1 - path camera
       2 - chase camera
 "distance" distance from chase guy
         (default 100)
 "height" height distance from chase guy
         (default 64)
*/

/*QUAKED effect_finale (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) useplayer nodecoy
 start the finale sequence
 "target" the camera to go to.
 "mdl" if useplayer is specified, this is a
 path corner with target of the next
 path_corner to run to.
 if use player isn't specified this becomes
 the spawn point as well.
 "spawnfunction" next routine to run
 "delay" time to wait until running routine
 useplayer - use the current player as
             decoy location.
 nodecoy - no decoy, only the camera
*/

void() effect_finale =
   {
   if (deathmatch)
	{
		remove(self);
		return;
	}
   setorigin(self,self.origin);
   self.angles = self.mangle;
   self.use = effect_finale_use;
//   setsize(self,'-16 -16 -16','16 16 16');
//   self.touch = effect_finale_use;
//   InitTrigger ();
   self.state = 0;
   };

void() info_startendtext_use =
   {
   intermission_running = 1;
   ExitIntermission();
   };

/*QUAKED info_startendtext (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
 start the end text
*/

void() info_startendtext =
   {
   self.use = info_startendtext_use;
   };
