/*
==============================================================================

PLAYER decoy

==============================================================================
*/

$cd id1/models/player_4
$origin 0 -6 24
$base base
$skin skin

//
// running
//
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6

$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

//
// standing
//
$frame stand1 stand2 stand3 stand4 stand5

$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12


void()   decoy_stand1 =[  $axstnd1,   decoy_stand1  ]
   {
   ChangeYaw();
   if (self.walkframe >= 5)
      self.walkframe = 0;
   self.frame = $stand1 + self.walkframe;
	self.walkframe = self.walkframe + 1;
	if (time > self.pausetime)
		self.th_walk ();
   };

void()   decoy_walk1 =[  $rockrun1,  decoy_walk1  ]
   {
//   local vector a;
   movetogoal (12);
//   a = '0 0 0';
//   a_y = self.ideal_yaw;
//   makevectors(a);
//   self.velocity = 1*v_forward;
   self.weaponframe=0;
   if (self.walkframe == 6)
      self.walkframe = 0;
   if (self.walkframe == 2 || self.walkframe == 5)
      {
      local float r;
      r = random();
      if (r < 0.14)
         sound (self, CHAN_VOICE, "misc/foot1.wav", 0.5, ATTN_NORM);
      else if (r < 0.29)
         sound (self, CHAN_VOICE, "misc/foot2.wav", 0.5, ATTN_NORM);
      else if (r < 0.43)
         sound (self, CHAN_VOICE, "misc/foot3.wav", 0.5, ATTN_NORM);
      else if (r < 0.58)
         sound (self, CHAN_VOICE, "misc/foot4.wav", 0.5, ATTN_NORM);
      else if (r < 0.72)
         sound (self, CHAN_VOICE, "misc/foot5.wav", 0.5, ATTN_NORM);
      else if (r < 0.86)
         sound (self, CHAN_VOICE, "misc/foot6.wav", 0.5, ATTN_NORM);
      else
         sound (self, CHAN_VOICE, "misc/foot7.wav", 0.5, ATTN_NORM);
      }
   self.frame = self.frame + self.walkframe;
	self.walkframe = self.walkframe + 1;
   };

void(entity decoy) setup_decoy =
{
   local entity pl;

   decoy.classname = "monster_decoy";
   setmodel (decoy, "progs/player.mdl");
   setsize (decoy, VEC_HULL_MIN, VEC_HULL_MAX);
   decoy.view_ofs = '0 0 22';

   decoy.solid = SOLID_SLIDEBOX;
   decoy.movetype = MOVETYPE_STEP;

   decoy.health = 3000000;

   decoy.th_stand = decoy_stand1;
   decoy.th_walk = decoy_walk1;
   decoy.th_run = decoy_walk1;
   decoy.th_missile = decoy_stand1;
   decoy.th_pain = decoy_stand1;
   decoy.th_die = decoy_stand1;

   pl = find (world, classname, "player");
   decoy.colormap = pl.colormap;
};

/*QUAKED monster_decoy (1 0 0) (-16 -16 -24) (16 16 40)
*/
void() monster_decoy =
{
   if (deathmatch)
	{
		remove(self);
		return;
	}
   setup_decoy(self);

   walkmonster_start ();
   total_monsters = total_monsters - 1;
};

void(string targ, vector orig) become_decoy =
{
   local entity pl;
   local entity decoy;
   local entity temp;

   decoy = hipdecoy = spawn();

   temp = self;
   self = decoy;

   setup_decoy(decoy);

   setorigin(decoy, orig);

   decoy.origin = orig;

   decoy.target = targ;

   decoy.takedamage = DAMAGE_AIM;

   decoy.ideal_yaw = decoy.angles * '0 1 0';
   if (!decoy.yaw_speed)
      decoy.yaw_speed = 20;
   decoy.use = monster_use;

   decoy.flags = decoy.flags | FL_MONSTER;

   if (decoy.target)
	{
      decoy.goalentity = decoy.movetarget = find(world, targetname, decoy.target);
      decoy.ideal_yaw = vectoyaw(decoy.goalentity.origin - decoy.origin);
      if (!decoy.movetarget)
		{
			dprint ("Monster can't find target at ");
         dprint (vtos(decoy.origin));
			dprint ("\n");
		}
// this used to be an objerror
      if (decoy.movetarget.classname == "path_corner")
         decoy.th_walk ();
		else
         decoy.pausetime = 99999999;
         decoy.th_stand ();
	}
	else
	{
      decoy.pausetime = 99999999;
      decoy.th_stand ();
	}

// spread think times so they don't all happen at same time
   decoy.nextthink = decoy.nextthink + random()*0.5;
   self = temp;
};
