/*
==============================================================================

gremlin

==============================================================================
*/
$cd hipwork\models\grem
$origin 0 0 23
$scale 2
$base grembase.asc

$skin grem

$frame stand1.asc stand2.asc stand3.asc stand4.asc stand5.asc stand6.asc
$frame stand7.asc stand8.asc stand9.asc stand10.asc stand11.asc stand12.asc
$frame stand13.asc stand14.asc stand15.asc stand16.asc stand17.asc

$frame walk1.asc walk2.asc walk3.asc walk4.asc walk5.asc walk6.asc
$frame walk7.asc walk8.asc walk9.asc walk10.asc walk11.asc walk12.asc

$frame run1.asc run2.asc run3.asc run4.asc run5.asc run6.asc run7.asc run8.asc
$frame run9.asc run10.asc run11.asc run12.asc run13.asc run14.asc run15.asc

$frame jump1.asc jump2.asc jump3.asc jump4.asc jump5.asc jump6.asc jump7.asc
$frame jump8.asc jump9.asc jump10.asc jump11.asc jump12.asc jump13.asc jump14.asc
$frame jump15.asc jump16.asc

$frame attk1.asc attk2.asc attk3.asc attk4.asc attk5.asc attk6.asc
$frame attk7.asc attk8.asc attk9.asc attk10.asc attk11.asc

$frame maul1.asc maul2.asc maul3.asc maul4.asc maul5.asc maul6.asc
$frame maul7.asc maul8.asc maul9.asc maul10.asc maul11.asc maul12.asc maul13.asc

$frame spawn1.asc spawn2.asc spawn3.asc spawn4.asc spawn5.asc spawn6.asc

$frame look1.asc look2.asc look3.asc look4.asc look5.asc look6.asc look7.asc
$frame look8.asc look9.asc look10.asc

$frame pain1.asc pain2.asc pain3.asc pain4.asc

$frame death1.asc death2.asc death3.asc death4.asc death5.asc death6.asc death7.asc
$frame death8.asc death9.asc death10.asc death11.asc death12.asc

$frame flip1.asc flip2.asc flip3.asc flip4.asc flip5.asc flip6.asc flip7.asc
$frame flip8.asc

$frame lunge1.asc lunge2.asc lunge3.asc lunge4.asc lunge5.asc lunge6.asc lunge7.asc
$frame lunge8.asc lunge9.asc lunge10.asc lunge11.asc

$frame gfire1.asc gfire2.asc gfire3.asc gfire4.asc gfire5.asc gfire6.asc

$frame glook1.asc glook2.asc glook3.asc glook4.asc glook5.asc glook6.asc glook7.asc
$frame glook8.asc glook9.asc glook10.asc glook11.asc glook12.asc glook13.asc
$frame glook14.asc glook15.asc glook16.asc glook17.asc glook18.asc glook19.asc glook20.asc

$frame gpain1.asc gpain2.asc gpain3.asc

$frame grun1.asc grun2.asc grun3.asc grun4.asc grun5.asc grun6.asc grun7.asc
$frame grun8.asc grun9.asc grun10.asc grun11.asc grun12.asc grun13.asc grun14.asc grun15.asc

//============================================================================

float NumGremlins;
float NumSpawnGremlins;

/*
===========
GremlinAttemptWeaponSteal

see if we can steal enemy's weapon

============
*/
entity()GremlinFindVictim;
float() GremlinFindTarget;

float() GremlinAttemptWeaponSteal =
   {
   local vector delta;
   local entity tempself;
   local float best;
   local entity victim;
   local float amount;

   if (self.stoleweapon)
      {
      dprint("gremlin trying to steal a weapon again\n");
      return FALSE;
      }
   if (!(self.enemy.flags & FL_CLIENT))
      {
      return FALSE;
      }

   delta = (self.enemy.origin - self.origin);

   if (vlen(delta) > 100)
      return FALSE;
   if (random() < 0.5)
      return FALSE;
   //
   // we are within range so lets go for it
   //
   victim = self.enemy;
   best = victim.weapon;
   if (best == IT_AXE || best == IT_SHOTGUN || best == IT_MJOLNIR)
      return FALSE;
   // take that weapon from the entity
   victim.items = victim.items - (victim.items & best);
   // give it to our gremlin
   self.items = self.items | best;
   self.weapon = best;
   // take some ammo while we are at it
   self.items = self.items - ( self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS ) );
   if (best == IT_SUPER_SHOTGUN)
      {
      amount = victim.ammo_shells;
      if (amount > 20)
         amount = 20;
      victim.ammo_shells = victim.ammo_shells - amount;
      self.ammo_shells = self.ammo_shells + amount;
        self.items = self.items | IT_SHELLS;
      self.currentammo = self.ammo_shells;
      sprint (victim, "Gremlin stole your Super Shotgun\n");
      }
   else if (best == IT_NAILGUN)
      {
      amount = victim.ammo_nails;
      if (amount > 40)
         amount = 40;
      victim.ammo_nails = victim.ammo_nails - amount;
      self.ammo_nails = self.ammo_nails + amount;
      self.items = self.items | IT_NAILS;
      self.currentammo = self.ammo_nails;
      sprint (victim, "Gremlin stole your Nailgun\n");
      }
   else if (best == IT_SUPER_NAILGUN)
      {
      amount = victim.ammo_nails;
      if (amount > 40)
         amount = 40;
      victim.ammo_nails = victim.ammo_nails - amount;
      self.ammo_nails = self.ammo_nails + amount;
      self.items = self.items | IT_NAILS;
      self.currentammo = self.ammo_nails;
      sprint (victim, "Gremlin stole your Super Nailgun\n");
      }
   else if (best == IT_GRENADE_LAUNCHER)
      {
      amount = victim.ammo_rockets;
      if (amount > 5)
         amount = 5;
      victim.ammo_rockets = victim.ammo_rockets - amount;
      self.ammo_rockets = self.ammo_rockets + amount;
      self.items = self.items | IT_ROCKETS;
      self.currentammo = self.ammo_rockets;
      sprint (victim, "Gremlin stole your Grenade Launcher\n");
      }
   else if (best == IT_ROCKET_LAUNCHER)
      {
      amount = victim.ammo_rockets;
      if (amount > 5)
         amount = 5;
      victim.ammo_rockets = victim.ammo_rockets - amount;
      self.ammo_rockets = self.ammo_rockets + amount;
      self.items = self.items | IT_ROCKETS;
      self.currentammo = self.ammo_rockets;
      sprint (victim, "Gremlin stole your Rocket Launcher\n");
      }
   else if (best == IT_LIGHTNING)
      {
      amount = victim.ammo_cells;
      if (amount > 40)
         amount = 40;
      victim.ammo_cells = victim.ammo_cells - amount;
      self.ammo_cells = self.ammo_cells + amount;
      self.items = self.items | IT_CELLS;
      self.currentammo = self.ammo_cells;
      sprint (victim, "Gremlin stole your Lightning Gun\n");
      }
   else if (best == IT_LASER_CANNON)
      {
      amount = victim.ammo_cells;
      if (amount > 40)
         amount = 40;
      victim.ammo_cells = victim.ammo_cells - amount;
      self.ammo_cells = self.ammo_cells + amount;
      self.items = self.items | IT_CELLS;
      self.currentammo = self.ammo_cells;
      sprint (victim, "Gremlin stole your Laser Cannon\n");
      }
   else if (best == IT_PROXIMITY_GUN)
      {
      amount = victim.ammo_rockets;
      if (amount > 5)
         amount = 5;
      victim.ammo_rockets = victim.ammo_rockets - amount;
      self.ammo_rockets = self.ammo_rockets + amount;
      self.items = self.items | IT_ROCKETS;
      self.currentammo = self.ammo_rockets;
      sprint (victim, "Gremlin stole your Proximity Gun\n");
      }
   tempself = self;
   self = victim;
   self.weapon = W_BestWeapon ();
   W_SetCurrentAmmo ();
   self = tempself;

   // tag the gremlin as having stolen a weapon
   self.stoleweapon = TRUE;
   self.attack_finished = time;
   // don't fire the first shot at the person we stole the weapon from
   // all the time
   if (random()>0.65)
      self.lastvictim = victim;
   else
      self.lastvictim = self;
//   self.attack_state = AS_STRAIGHT;
   // find a recipient
   victim = GremlinFindVictim();
   if (victim != world)
      {
      self.enemy = victim;
      FoundTarget();
      self.attack_finished = time;
      self.search_time = time + 1.0;
      }

   return TRUE;
   };
//============================================================================

/*
===========
GremlinFindTarget

gremlin is currently not attacking anything, so try to find a target

============
*/
float() GremlinFindTarget =
   {
   local entity   head;
   local entity   gorge;
   local float    dist;
   local float    result;

   if ((self.stoleweapon==0) && time > self.wait)
      {
      self.wait = time + 1.0;
      dist = 2000;
      gorge = world;
      head = nextent(world);
      while (head!=world)
         {
         if ((head.health < 1) && (head.flags & (FL_MONSTER|FL_CLIENT)))
            {
            result = fabs(head.origin_z - self.origin_z);
            if ((visible(head)) && (result<80) && (head.gorging == FALSE) && (visible_distance<dist))
               {
               dist = visible_distance;
               gorge = head;
               }
            }
         head = nextent(head);
         }
      if ((gorge != world) && (dist < (700*random())))
         {
//         dprint("starting to gorge on ");
//         dprint(gorge.classname);
//         dprint("\n");
         self.oldenemy = self.enemy;
         self.gorging = TRUE;
         self.enemy = gorge;
         self.search_time = time + 4.0;
         FoundTarget();
         return TRUE;
         }
      }
   else if (self.stoleweapon)
      {
      head = GremlinFindVictim();
      if (head != world)
         {
         self.enemy = head;
         FoundTarget();
         self.attack_finished = time;
         self.search_time = time + 2.0;
         return TRUE;
         }
      }
   result = FindTarget();
   self.search_time = time + 2.0;
   return result;
   };
//============================================================================
/*
=============
gremlin_walk

The monster is walking it's beat
=============
*/
void(float dist) gremlin_walk =
   {
    movedist = dist;

    // check for noticing a player
   if (GremlinFindTarget ())
        return;

    movetogoal (dist);
   };

//============================================================================

/*
=============
gremlin_stand

The monster is staying in one place for a while, with slight angle turns
=============
*/
void() gremlin_stand =
   {
   if (FindTarget ())
        return;

    if (time > self.pausetime)
      {
        self.th_walk ();
        return;
      }
   };

//============================================================================
float() GremlinCheckNoAmmo;
void() gremlin_glook1;
/*
=============
gremlin_run

The monster has an enemy it is trying to kill
=============
*/
void(float dist) gremlin_run =
   {
   local entity oldtarget;
   local float r;
   local vector d;

   if (self.watertype == CONTENT_LAVA)
      {  // do damage
      T_Damage (self, world, world, 2000);
      }
   movedist = dist;
   if (self.stoleweapon)
      self.frame = self.frame + $grun1.asc - $run1.asc;

   if (self.gorging)
      {
      traceline(self.origin, self.enemy.origin, TRUE, self);
      if (trace_fraction != 1.0)
         {
         self.gorging = FALSE;
         return;
         }
      if (!visible(self.enemy))
         {
         self.gorging = FALSE;
         return;
         }
      r = vlen(self.enemy.origin - self.origin);
      if (r < 130)
         {
         ai_face();
         if (r < 45)
            {
            self.th_melee ();
            self.attack_state = AS_STRAIGHT;
            return;
            }
         else if (walkmove (self.angles_y, dist) == FALSE)
            {
            self.gorging = FALSE;
            return;
            }
         return;
         }
      movetogoal (dist);      // done in C code...
      }
   else
      {
      if (random()>0.97)
         {
         if (GremlinFindTarget())
            return;
         }
      // get away from player if we stole a weapon
      if (self.stoleweapon)
         {
         if (self.enemy.health < 0 && self.enemy.classname == "player")
            {
            gremlin_glook1();
            return;
            }
         if (!GremlinCheckNoAmmo())
            {
            if (self.t_length==1)
               {
               remove(self.trigger_field);
               self.goalentity = self.enemy;
               self.t_length = 0;
               }
            return;
            }
         r = vlen(self.enemy.origin - self.origin);
         d = normalize(self.origin-self.enemy.origin);
         if (self.t_length == 0)
            {
            if (r<150)
               {
               self.trigger_field = spawn();
               setsize(self.trigger_field,'-1 -1 -1','1 1 1');
               self.t_length = 1;
               }
            }
         if (self.t_length==1)
            {
            if (r > 250)
               {
               remove(self.trigger_field);
               self.goalentity = self.enemy;
               self.t_length = 0;
//               self.attack_state = AS_SLIDING;
               }
            else
               {
               if (r < 160)
                  {
                  local vector ang;
                  local float done;
                  local vector end;
                  local float c;

                  ang = vectoangles(d);
                  done = 0;
                  c = 0;
                  while (done == 0)
                     {
                     makevectors(ang);
                     end = self.enemy.origin + v_forward * 350;
                     traceline(self.enemy.origin,end,FALSE,self);
                     if (trace_fraction == 1.0)
                        {
                        traceline(self.origin,end,FALSE,self);
                        if (trace_fraction == 1.0)
                           done = 1;
                        }
                     ang_y = anglemod(ang_y + 36);
                     c = c + 1;
                     if (c == 10)
                        {
                        done = 1;
                        }
                     }
                  setorigin(self.trigger_field,end);
                  }
               self.goalentity = self.trigger_field;
               self.ideal_yaw = vectoyaw(normalize(self.goalentity.origin-self.origin));
               ChangeYaw ();
               movetogoal (dist);      // done in C code...
               self.nextthink = time + 0.1;
               return;
               }
            }
         }
      ai_run(dist);
      self.nextthink = time + 0.1;
      }
   };

//============================================================================

void()   Gremlin_JumpTouch;
void()   Gremlin_FlipTouch;
void(float side)  Gremlin_Melee;
void(float side)  Gremlin_Gorge;

void()   gremlin_stand1  =[ $stand1.asc, gremlin_stand2  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand2  =[ $stand2.asc, gremlin_stand3  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand3  =[ $stand3.asc, gremlin_stand4  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand4  =[ $stand4.asc, gremlin_stand5  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand5  =[ $stand5.asc, gremlin_stand6  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand6  =[ $stand6.asc, gremlin_stand7  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand7  =[ $stand7.asc, gremlin_stand8  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand8  =[ $stand8.asc, gremlin_stand9  ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand9  =[ $stand9.asc, gremlin_stand10 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand10 =[ $stand10.asc,   gremlin_stand11 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand11 =[ $stand11.asc,   gremlin_stand12 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand12 =[ $stand12.asc,   gremlin_stand13 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand13 =[ $stand13.asc,   gremlin_stand14 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand14 =[ $stand14.asc,   gremlin_stand15 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand15 =[ $stand15.asc,   gremlin_stand16 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand16 =[ $stand16.asc,   gremlin_stand17 ] {gremlin_stand();self.nextthink = time + 0.2;};
void()   gremlin_stand17 =[ $stand17.asc,   gremlin_stand1  ] {gremlin_stand();self.nextthink = time + 0.2;};

void()   gremlin_walk1 =[ $walk1.asc,      gremlin_walk2 ] {
if (random() < 0.1)
    sound (self, CHAN_VOICE, "grem/idle.wav", 1, ATTN_IDLE);
gremlin_walk(8);};
void()   gremlin_walk2 =[ $walk2.asc,      gremlin_walk3 ] {gremlin_walk(8);};
void()   gremlin_walk3 =[ $walk3.asc,      gremlin_walk4 ] {gremlin_walk(8);};
void()   gremlin_walk4 =[ $walk4.asc,      gremlin_walk5 ] {gremlin_walk(8);};
void()   gremlin_walk5 =[ $walk5.asc,      gremlin_walk6 ] {gremlin_walk(8);};
void()   gremlin_walk6 =[ $walk6.asc,      gremlin_walk7 ] {gremlin_walk(8);};
void()   gremlin_walk7 =[ $walk7.asc,      gremlin_walk8 ] {gremlin_walk(8);};
void()   gremlin_walk8 =[ $walk8.asc,      gremlin_walk9 ] {gremlin_walk(8);};
void()   gremlin_walk9 =[ $walk9.asc,      gremlin_walk10 ] {gremlin_walk(8);};
void()   gremlin_walk10 =[ $walk10.asc,      gremlin_walk11 ] {gremlin_walk(8);};
void()   gremlin_walk11 =[ $walk11.asc,      gremlin_walk12 ] {gremlin_walk(8);};
void()   gremlin_walk12 =[ $walk12.asc,      gremlin_walk1 ] {gremlin_walk(8);};

void()   gremlin_run1 =[ $run1.asc,      gremlin_run2 ] {
if (random() < 0.1)
    sound (self, CHAN_VOICE, "grem/idle.wav", 1, ATTN_IDLE);
gremlin_run(0);};
void()   gremlin_run2 =[ $run2.asc,      gremlin_run3 ] {gremlin_run(8);};
void()   gremlin_run3 =[ $run3.asc,      gremlin_run4 ] {gremlin_run(12);};
void()   gremlin_run4 =[ $run4.asc,      gremlin_run5 ] {gremlin_run(16);};
void()   gremlin_run5 =[ $run5.asc,      gremlin_run6 ] {gremlin_run(16);};
void()   gremlin_run6 =[ $run6.asc,      gremlin_run7 ] {gremlin_run(12);};
void()   gremlin_run7 =[ $run7.asc,      gremlin_run8 ] {gremlin_run(8);};
void()   gremlin_run8 =[ $run8.asc,      gremlin_run9 ] {gremlin_run(0);};
void()   gremlin_run9 =[ $run9.asc,      gremlin_run10 ] {gremlin_run(8);};
void()   gremlin_run10 =[ $run10.asc,      gremlin_run11 ] {gremlin_run(12);};
void()   gremlin_run11 =[ $run11.asc,      gremlin_run12 ] {gremlin_run(16);};
void()   gremlin_run12 =[ $run12.asc,      gremlin_run1 ] {gremlin_run(16);};
void()   gremlin_run13 =[ $run13.asc,      gremlin_run14 ] {gremlin_run(12);};
void()   gremlin_run14 =[ $run14.asc,      gremlin_run15 ] {gremlin_run(8);};
void()   gremlin_run15 =[ $run15.asc,      gremlin_run1 ] {gremlin_run(0);};

void()   gremlin_jump1   =[ $jump1.asc,     gremlin_jump2   ] {ai_face();};
void()   gremlin_jump2   =[ $jump2.asc,     gremlin_jump3   ] {ai_face();};
void()   gremlin_jump3   =[ $jump3.asc,     gremlin_jump4   ] {ai_face();};
void()   gremlin_jump4   =[ $jump4.asc,     gremlin_jump5   ] {ai_face();};
void()   gremlin_jump5   =[ $jump5.asc,     gremlin_jump6   ]
{
    ai_face();

   if (self.flags & FL_ONGROUND)
      {
      self.touch = Gremlin_JumpTouch;
      makevectors (self.angles);
      self.origin_z = self.origin_z + 1;
      self.velocity = v_forward * 300 + '0 0 300';
      self.flags = self.flags - FL_ONGROUND;
      }
   else
      {
      gremlin_run1();
      }
};
void()   gremlin_jump6   =[ $jump6.asc,     gremlin_jump7   ] {};
void()   gremlin_jump7   =[ $jump7.asc,     gremlin_jump8   ] {};
void()   gremlin_jump8   =[ $jump8.asc,     gremlin_jump9   ] {};
void()   gremlin_jump9   =[ $jump9.asc,     gremlin_jump10   ] {};
void()   gremlin_jump10   =[ $jump10.asc,     gremlin_jump11   ] {};
void()   gremlin_jump11   =[ $jump11.asc,     gremlin_jump1   ]
   {
   self.nextthink = time + 3;
   // if three seconds pass, assume gremlin is stuck and jump again
   };

void()   gremlin_jump12   =[ $jump12.asc,    gremlin_jump13  ] {};
void()   gremlin_jump13   =[ $jump13.asc,    gremlin_jump14  ] {};
void()   gremlin_jump14   =[ $jump14.asc,    gremlin_jump15  ] {};
void()   gremlin_jump15   =[ $jump15.asc,    gremlin_jump16  ] {};
void()   gremlin_jump16   =[ $jump16.asc,    gremlin_run1 ] {};

void()   gremlin_shot1 = [$gfire1.asc, gremlin_shot2   ] {self.effects = self.effects | EF_MUZZLEFLASH;};
void()   gremlin_shot2 = [$gfire2.asc, gremlin_shot3   ] {};
void()   gremlin_shot3 = [$gfire3.asc, gremlin_shot4   ] {};
void()   gremlin_shot4 = [$gfire4.asc, gremlin_shot5   ] {};
void()   gremlin_shot5 = [$gfire5.asc, gremlin_shot6   ] {};
void()   gremlin_shot6 = [$gfire6.asc, gremlin_run1   ] {};


//============================================================================
void(float ox) Gremlin_FireNailGun;

void() gremlin_nail1   =[$gfire1.asc, gremlin_nail2  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail2   =[$gfire1.asc, gremlin_nail3  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail3   =[$gfire1.asc, gremlin_nail4  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail4   =[$gfire1.asc, gremlin_nail5  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail5   =[$gfire1.asc, gremlin_nail6  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail6   =[$gfire1.asc, gremlin_nail7  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireNailGun(4);
   };
void() gremlin_nail7 = [$gfire1.asc, gremlin_run1   ] {};


void(float ox) Gremlin_FireLaserGun;

void() gremlin_laser1   =[$gfire1.asc, gremlin_laser2  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser2   =[$gfire1.asc, gremlin_laser3  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser3   =[$gfire1.asc, gremlin_laser4  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser4   =[$gfire1.asc, gremlin_laser5  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser5   =[$gfire1.asc, gremlin_laser6  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser6   =[$gfire1.asc, gremlin_laser7  ]
   {
    self.effects = self.effects | EF_MUZZLEFLASH;
   Gremlin_FireLaserGun(4);
   };
void() gremlin_laser7 = [$gfire1.asc, gremlin_run1   ] {};

//============================================================================

void() Gremlin_FireLightningGun;
void() gremlin_light1   =[$gfire1.asc, gremlin_light2  ]
   {
   Gremlin_FireLightningGun();
   };
void() gremlin_light2 = [$gfire1.asc, gremlin_light3   ]
   {
   Gremlin_FireLightningGun();
   };
void() gremlin_light3   =[$gfire1.asc, gremlin_light4  ]
   {
   Gremlin_FireLightningGun();
   };
void() gremlin_light4 = [$gfire1.asc, gremlin_light5   ]
{
   Gremlin_FireLightningGun();
};
void() gremlin_light5 = [$gfire1.asc, gremlin_run1   ] {};

//============================================================================

void()   gremlin_rocket1 = [$gfire1.asc, gremlin_rocket2   ] {self.effects = self.effects | EF_MUZZLEFLASH;};
void()   gremlin_rocket2 = [$gfire2.asc, gremlin_rocket3   ] {};
void()   gremlin_rocket3 = [$gfire3.asc, gremlin_rocket4   ] {};
void()   gremlin_rocket4 = [$gfire4.asc, gremlin_rocket5   ] {};
void()   gremlin_rocket5 = [$gfire5.asc, gremlin_rocket6   ] {};
void()   gremlin_rocket6 = [$gfire6.asc, gremlin_run1   ] {};

void()   gremlin_lunge1   =[ $lunge1.asc,     gremlin_lunge2   ] {ai_charge(0);};
void()   gremlin_lunge2   =[ $lunge2.asc,     gremlin_lunge3   ] {ai_charge(0);};
void()   gremlin_lunge3   =[ $lunge3.asc,     gremlin_lunge4   ] {ai_charge(0);};
void()   gremlin_lunge4   =[ $lunge4.asc,     gremlin_lunge5   ] {ai_charge(0);};
void()   gremlin_lunge5   =[ $lunge5.asc,     gremlin_lunge6   ] {ai_charge(0);};
void()   gremlin_lunge6   =[ $lunge6.asc,     gremlin_lunge7   ] {ai_charge(0);};
void()   gremlin_lunge7   =[ $lunge7.asc,     gremlin_lunge8   ] {ai_charge(15);};
void()   gremlin_lunge8   =[ $lunge8.asc,     gremlin_lunge9   ] {ai_charge(0);Gremlin_Melee(0);};
void()   gremlin_lunge9   =[ $lunge9.asc,     gremlin_lunge10] {ai_charge(0);};
void()   gremlin_lunge10  =[ $lunge10.asc,    gremlin_lunge11] {ai_charge(0);};
void()   gremlin_lunge11  =[ $lunge11.asc,    gremlin_run1] {ai_charge(0);};

void()   gremlin_claw1   =[ $attk1.asc,     gremlin_claw2   ] {ai_charge(0);};
void()   gremlin_claw2   =[ $attk2.asc,     gremlin_claw3   ] {ai_charge(0);};
void()   gremlin_claw3   =[ $attk3.asc,     gremlin_claw4   ] {ai_charge(0);};
void()   gremlin_claw4   =[ $attk4.asc,     gremlin_claw5   ] {ai_charge(0);};
void()   gremlin_claw5   =[ $attk5.asc,     gremlin_claw6   ] {ai_charge(0);};
void()   gremlin_claw6   =[ $attk6.asc,     gremlin_claw7   ] {ai_charge(0);Gremlin_Melee(200);};
void()   gremlin_claw7   =[ $attk7.asc,     gremlin_claw8   ] {ai_charge(15);};
void()   gremlin_claw8   =[ $attk8.asc,     gremlin_claw9   ] {ai_charge(0);};
void()   gremlin_claw9   =[ $attk9.asc,     gremlin_claw10] {ai_charge(0);};
void()   gremlin_claw10  =[ $attk10.asc,    gremlin_claw11] {ai_charge(0);};
void()   gremlin_claw11  =[ $attk11.asc,    gremlin_run1] {ai_charge(0);};

void()   gremlin_gorge1   =[ $maul1.asc,     gremlin_gorge2] {ai_charge(1);};
void()   gremlin_gorge2   =[ $maul2.asc,     gremlin_gorge3] {ai_charge(1);};
void()   gremlin_gorge3   =[ $maul3.asc,     gremlin_gorge4] {ai_charge(2);};
void()   gremlin_gorge4   =[ $maul4.asc,     gremlin_gorge5] {ai_charge(0);};
void()   gremlin_gorge5   =[ $maul5.asc,     gremlin_gorge6] {ai_charge(0);};
void()   gremlin_gorge6   =[ $maul6.asc,     gremlin_gorge7] {ai_charge(0);Gremlin_Gorge(200);};
void()   gremlin_gorge7   =[ $maul7.asc,     gremlin_gorge8] {ai_charge(0);};
void()   gremlin_gorge8   =[ $maul8.asc,     gremlin_gorge9] {ai_charge(0);Gremlin_Gorge(-200);};
void()   gremlin_gorge9   =[ $maul9.asc,     gremlin_gorge10] {ai_charge(0);};
void()   gremlin_gorge10   =[ $maul10.asc,     gremlin_gorge11] {ai_charge(0);};
void()   gremlin_gorge11   =[ $maul11.asc,     gremlin_gorge12] {ai_charge(0);};
void()   gremlin_gorge12   =[ $maul12.asc,     gremlin_gorge13] {ai_charge(0);};
void()   gremlin_gorge13   =[ $maul13.asc,     gremlin_gorge1] {ai_charge(0);};

void()   gremlin_look1   =[ $look1.asc,     gremlin_look2] {self.nextthink = time + 0.2;};
void()   gremlin_look2   =[ $look2.asc,     gremlin_look3] {self.nextthink = time + 0.2;};
void()   gremlin_look3   =[ $look3.asc,     gremlin_look4] {self.nextthink = time + 0.2;};
void()   gremlin_look4   =[ $look4.asc,     gremlin_look5] {self.nextthink = time + 0.2;};
void()   gremlin_look5   =[ $look5.asc,     gremlin_look6] {self.nextthink = time + 0.2;};
void()   gremlin_look6   =[ $look6.asc,     gremlin_look7] {self.nextthink = time + 0.2;};
void()   gremlin_look7   =[ $look7.asc,     gremlin_look8] {self.nextthink = time + 0.2;};
void()   gremlin_look8   =[ $look8.asc,     gremlin_look9] {self.nextthink = time + 0.2;};
void()   gremlin_look9   =[ $look9.asc,     gremlin_run1]
   {
   if (self.oldenemy.health > 0)
      {
      self.enemy = self.oldenemy;
      HuntTarget ();
      }
   else
      {
      if (self.movetarget)
         self.th_walk ();
      else
         self.th_stand ();
      }
   };


void() GremlinDropBackpack;
void()   gremlin_glook1   =[ $glook1.asc,     gremlin_glook2] {};
void()   gremlin_glook2   =[ $glook2.asc,     gremlin_glook3] {};
void()   gremlin_glook3   =[ $glook3.asc,     gremlin_glook4] {};
void()   gremlin_glook4   =[ $glook4.asc,     gremlin_glook5] {};
void()   gremlin_glook5   =[ $glook5.asc,     gremlin_glook6] {};
void()   gremlin_glook6   =[ $glook6.asc,     gremlin_glook7] {};
void()   gremlin_glook7   =[ $glook7.asc,     gremlin_glook8] {};
void()   gremlin_glook8   =[ $glook8.asc,     gremlin_glook9] {};
void()   gremlin_glook9   =[ $glook9.asc,     gremlin_glook10] {};
void()   gremlin_glook10   =[ $glook10.asc,     gremlin_glook11] {};
void()   gremlin_glook11   =[ $glook11.asc,     gremlin_glook12] {};
void()   gremlin_glook12   =[ $glook12.asc,     gremlin_glook13] {};
void()   gremlin_glook13   =[ $glook13.asc,     gremlin_glook14] {};
void()   gremlin_glook14   =[ $glook14.asc,     gremlin_glook15] {};
void()   gremlin_glook15   =[ $glook15.asc,     gremlin_glook16] {};
void()   gremlin_glook16   =[ $glook16.asc,     gremlin_glook17] {};
void()   gremlin_glook17   =[ $glook17.asc,     gremlin_glook18] {};
void()   gremlin_glook18   =[ $glook18.asc,     gremlin_glook19] {};
void()   gremlin_glook19   =[ $glook19.asc,     gremlin_glook20] {};
void()   gremlin_glook20   =[ $glook20.asc,     gremlin_glook20]
   {
   GremlinDropBackpack();
   self.stoleweapon = FALSE;
   if (self.oldenemy.health > 0)
      {
      self.enemy = self.oldenemy;
      HuntTarget ();
      }
   else
      {
      if (self.movetarget)
         self.th_walk ();
      else
         self.th_stand ();
      }
   };

void()   gremlin_pain1   =[ $pain1.asc,     gremlin_pain2   ] {ai_back(4);};
void()   gremlin_pain2   =[ $pain2.asc,     gremlin_pain3   ] {ai_back(4);};
void()   gremlin_pain3   =[ $pain3.asc,     gremlin_pain4   ] {ai_back(2);};
void()   gremlin_pain4   =[ $pain4.asc,     gremlin_run1 ] {};

void()   gremlin_gunpain1   =[ $gpain1.asc,     gremlin_gunpain2   ] {ai_back(4);};
void()   gremlin_gunpain2   =[ $gpain2.asc,     gremlin_gunpain3   ] {ai_back(2);};
void()   gremlin_gunpain3   =[ $gpain3.asc,     gremlin_run1 ] {};

void(entity attacker, float damage) gremlin_pain =
{
   local float r;

   if (random()<0.8)
      {
      self.gorging = FALSE;
      self.enemy = attacker;
      FoundTarget();
      }
   if (self.touch == Gremlin_JumpTouch)
        return;

    if (self.pain_finished > time)
        return;

    self.pain_finished = time + 1;
//   if (random()<0.5)
//      sound (self, CHAN_VOICE, "grem/pain2.wav", 1, ATTN_NORM);
//   else
   r = random();
   if (r<0.33)
      sound (self, CHAN_VOICE, "grem/pain1.wav", 1, ATTN_NORM);
   else if (r<0.66)
      sound (self, CHAN_VOICE, "grem/pain2.wav", 1, ATTN_NORM);
   else
      sound (self, CHAN_VOICE, "grem/pain3.wav", 1, ATTN_NORM);

   if (self.stoleweapon)
      {
      gremlin_gunpain1 ();
      }
   else
      {
      gremlin_pain1 ();
      }
};

void()   gremlin_spawn1 =[ $spawn1.asc,      gremlin_spawn2 ] {self.nextthink = time + 0.3;
   self.th_pain = SUB_Null;
//    sound (self, CHAN_VOICE, "grem/idle.wav", 1, ATTN_IDLE);
};
void()   gremlin_spawn2 =[ $spawn2.asc,      gremlin_spawn3 ] {self.nextthink = time + 0.3;};
void()   gremlin_spawn3 =[ $spawn3.asc,      gremlin_spawn4 ] {self.nextthink = time + 0.3;};
void()   gremlin_spawn4 =[ $spawn4.asc,      gremlin_spawn5 ] {self.nextthink = time + 0.3;};
void()   gremlin_spawn5 =[ $spawn5.asc,      gremlin_spawn6 ] {self.nextthink = time + 0.3;};
void()   gremlin_spawn6 =[ $spawn6.asc,      gremlin_run1 ] {
   self.th_pain = gremlin_pain;
};


void()   gremlin_die1    =[ $death1.asc,    gremlin_die2 ] {
sound (self, CHAN_VOICE, "grem/death.wav", 1, ATTN_NORM);};
void()   gremlin_die2    =[ $death2.asc,    gremlin_die3 ] {ai_forward(2);};
void()   gremlin_die3    =[ $death3.asc,    gremlin_die4 ] {ai_forward(1);};
void()   gremlin_die4    =[ $death4.asc,    gremlin_die5 ] {ai_forward(2);};
void()   gremlin_die5    =[ $death5.asc,    gremlin_die6 ] {ai_forward(1);};
void()   gremlin_die6    =[ $death6.asc,    gremlin_die7 ]
{self.solid = SOLID_NOT;};
void()   gremlin_die7    =[ $death7.asc,    gremlin_die8 ] {ai_forward(2);};
void()   gremlin_die8    =[ $death8.asc,    gremlin_die9 ] {ai_forward(1);};
void()   gremlin_die9    =[ $death9.asc,    gremlin_die10 ] {ai_forward(2);};
void()   gremlin_die10    =[ $death10.asc,    gremlin_die11 ] {ai_forward(1);};
void()   gremlin_die11    =[ $death11.asc,    gremlin_die12 ] {ai_forward(2);};
void()   gremlin_die12    =[ $death12.asc,    gremlin_die12 ] {};

void() gremlin_gib=
   {
   sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
   ThrowHead ("progs/h_grem.mdl", -35);
   ThrowGib ("progs/gib1.mdl", -35);
   ThrowGib ("progs/gib1.mdl", -35);
   ThrowGib ("progs/gib1.mdl", -35);
   };

void()   gremlin_flip1    =[ $flip1.asc,    gremlin_flip2 ]
   {
   ai_face();
    makevectors (self.angles);
   self.origin_z = self.origin_z + 1;
   self.velocity = '0 0 350' - (v_forward * 200);
    if (self.flags & FL_ONGROUND)
        self.flags = self.flags - FL_ONGROUND;
   sound (self, CHAN_VOICE, "grem/death.wav", 1, ATTN_NORM);
   };
void()   gremlin_flip2    =[ $flip2.asc,    gremlin_flip3 ] {ai_face();};
void()   gremlin_flip3    =[ $flip3.asc,    gremlin_flip4 ] {};
void()   gremlin_flip4    =[ $flip4.asc,    gremlin_flip5 ] {};
void()   gremlin_flip5    =[ $flip5.asc,    gremlin_flip6 ] {};
void()   gremlin_flip6    =[ $flip6.asc,    gremlin_flip7 ] {self.touch = Gremlin_FlipTouch;};
void()   gremlin_flip7    =[ $flip7.asc,    gremlin_gib ]
   {
   self.nextthink = time + 3;
   // if three seconds pass, assume gremlin is stuck and jump again
   };
void()   gremlin_flip8    =[ $flip8.asc,    gremlin_flip8 ]
   {
   self.solid = SOLID_NOT;
   };

/*
===============
GremlinDropBackpack
===============
*/
void() GremlinDropBackpack =
{
    local entity    item;

    item = spawn();
    item.origin = self.origin - '0 0 24';

   self.items = self.items - ( self.items & (IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS ) );
   item.items = self.items;

   if (item.items & IT_AXE)
        item.netname = "Axe";
   else if (item.items & IT_SHOTGUN)
        item.netname = "Shotgun";
   else if (item.items & IT_SUPER_SHOTGUN)
        item.netname = "Double-barrelled Shotgun";
   else if (item.items & IT_NAILGUN)
        item.netname = "Nailgun";
   else if (item.items & IT_SUPER_NAILGUN)
        item.netname = "Super Nailgun";
   else if (item.items & IT_GRENADE_LAUNCHER)
        item.netname = "Grenade Launcher";
   else if (item.items & IT_ROCKET_LAUNCHER)
        item.netname = "Rocket Launcher";
   else if (item.items & IT_LIGHTNING)
        item.netname = "Thunderbolt";
   else if (item.items & IT_LASER_CANNON)
      item.netname = "Laser Cannon";
   else if (item.items & IT_PROXIMITY_GUN)
      item.netname = "Proximity Gun";
   else if (item.items & IT_MJOLNIR)
      item.netname = "Mjolnir";
   else
        item.netname = "";

   item.ammo_shells = self.ammo_shells;
    item.ammo_nails = self.ammo_nails;
    item.ammo_rockets = self.ammo_rockets;
    item.ammo_cells = self.ammo_cells;
   if (item.ammo_shells < 0) item.ammo_shells = 0;
   if (item.ammo_nails < 0) item.ammo_nails = 0;
   if (item.ammo_rockets < 0) item.ammo_rockets = 0;
   if (item.ammo_cells < 0) item.ammo_cells = 0;

    item.velocity_z = 300;
    item.velocity_x = -100 + (random() * 200);
    item.velocity_y = -100 + (random() * 200);

    item.flags = FL_ITEM;
    item.solid = SOLID_TRIGGER;
    item.movetype = MOVETYPE_TOSS;
    setmodel (item, "progs/backpack.mdl");
    setsize (item, '-16 -16 0', '16 16 56');
    item.touch = BackpackTouch;

    item.nextthink = time + 120;    // remove after 2 minutes
    item.think = SUB_Remove;
};

void() gremlin_die =
{
   local vector   vec;
    local float        dot;

// check for gib
   if (self.items & (IT_SUPER_SHOTGUN|IT_NAILGUN|IT_SUPER_NAILGUN|IT_GRENADE_LAUNCHER|IT_ROCKET_LAUNCHER|IT_LIGHTNING|IT_LASER_CANNON|IT_PROXIMITY_GUN))
      {
      GremlinDropBackpack();
      self.stoleweapon = FALSE;
      }

   makevectors (self.angles);
   vec = normalize (damage_attacker.origin - self.origin);
    dot = vec * v_forward;

   if (self.health < -35)
      {
        sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
      ThrowHead ("progs/h_grem.mdl", self.health);
        ThrowGib ("progs/gib1.mdl", self.health);
        ThrowGib ("progs/gib1.mdl", self.health);
        ThrowGib ("progs/gib1.mdl", self.health);
        return;
      }
   else if (dot>0.7 && (random()<0.5) && (self.flags & FL_ONGROUND))
      {
      gremlin_flip1();
      return;
      }
// regular death
   gremlin_die1 ();
};


float() GremlinWeaponAttack;
void() Gremlin_MeleeAttack =
   {
   local float num;
   if (self.gorging)
      {
      gremlin_gorge1();
      }
   else
      {
      if (self.stoleweapon == 1)
         {
         objerror("gremlin meleeing with stolen weapon");
         }
      else if ((self.enemy.flags & FL_CLIENT) && random() < 0.4)
         {
         if (GremlinAttemptWeaponSteal())
            return;
         }
      num = random();
      if (num<0.3)
         {
         gremlin_claw1 ();
         }
      else if (num < 0.6)
         {
         gremlin_lunge1 ();
         }
      else
         {
         gremlin_claw1 ();
         }
      }
   };

/*
============
gremlin_recoil
============
*/
/*
void() gremlin_recoil =
   {
   self.nextthink = time + 0.1;
   self.flags = self.flags - FL_ONGROUND;
   if (vlen(self.velocity)<2)
      {
      self.movetype = self.spawnsolidtype;
      self.think = self.spawnthink;
      }
   };
*/
/*
============
GremlinRecoil
============
*/
/*
void(vector dir, float mag) GremlinRecoil =
   {
   self.spawnsolidtype = self.movetype;
   self.movetype = MOVETYPE_BOUNCE;
   self.spawnthink = self.think;
   self.think = gremlin_recoil;
   self.nextthink = time;
   self.velocity = mag * dir;
   self.velocity_z = self.velocity_z + 100;
   self.flags = self.flags - FL_ONGROUND;
   };
*/

/*
============
GremlinCheckNoAmmo

attack with a weapon
============
*/
float() GremlinCheckNoAmmo =
   {
    if (self.currentammo > 0)
        return TRUE;
   else
      {
      self.stoleweapon = FALSE;
      return FALSE;
      }
   };

/*
============
GremlinFindVictim

find a victim to shoot at
============
*/
entity() GremlinFindVictim =
   {
   local entity head;
   local entity selected;
   local float dist;
   local float head_dist;
   local float decision;

   self.search_time = time + 1.0;
// look in our immediate vicinity

   selected = world;
   dist = 1000;
   head = findradius(self.origin, 1000);
   while(head)
      {
      if(!(head.flags & FL_NOTARGET) && ((head.flags & FL_MONSTER) || (head.flags & FL_CLIENT)))
         {
         if (visible(head) && (head.health > 0) && (head !=self))
            {
            head_dist = vlen(head.origin-self.origin);
            if (head == self.lastvictim)
               head_dist = head_dist * 2;
            if (head.flags & FL_CLIENT)
               head_dist = head_dist / 1.5;
            if (head.classname == self.classname)
               head_dist = head_dist * 1.5;
            if (head_dist < dist)
               {
               selected = head;
               dist = head_dist;
               }
            }
         }
      head = head.chain;
      }
   self.lastvictim = selected;

   return selected;
   };

/*
============
Gremlin_FireRocket

fire a rocket
============
*/
void() Gremlin_FireRocket =
{
    local    entity missile, mpuff;
   local vector dir;

    self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
   self.effects = self.effects | EF_MUZZLEFLASH;

    sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";

// set missile speed

   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 * v_right + crandom()* 0.1 *v_up;
   missile.velocity = normalize(dir);
   missile.velocity = missile.velocity * 1000;
    missile.angles = vectoangles(missile.velocity);

    missile.touch = T_MissileTouch;

// set missile duration
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;

    setmodel (missile, "progs/missile.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, self.origin + v_forward*8 + '0 0 16');
//   GremlinRecoil(dir,-1000);
};

/*
============
Gremlin_FireNailGun

fire a nailgun
============
*/
void(float ox) Gremlin_FireNailGun =
{
   local vector dir;

   self.currentammo = self.ammo_nails = self.ammo_nails - 1;
   self.effects = self.effects | EF_MUZZLEFLASH;

   sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 * v_right + crandom()* 0.1 *v_up;
   dir = normalize(dir);
   launch_spike (self.origin + '0 0 16', dir);
};

/*
============
Gremlin_FireLaserGun

fire a laser cannon
============
*/
void(float ox) Gremlin_FireLaserGun =
{
   local vector dir;

   self.currentammo = self.ammo_cells = self.ammo_cells - 1;
   self.effects = self.effects | EF_MUZZLEFLASH;

   sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 * v_right + crandom()* 0.1 *v_up;
   dir = normalize(dir);
   HIP_LaunchLaser(self.origin + '0 0 16', dir, 0);
};

/*
============
Gremlin_FireShotGun

fire a shotgun
============
*/
void() Gremlin_FireShotGun =
{
   local vector dir;

   self.currentammo = self.ammo_shells = self.ammo_shells - 1;
   self.effects = self.effects | EF_MUZZLEFLASH;

   sound (self, CHAN_WEAPON, "weapons/guncock.wav", 1, ATTN_NORM);
   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 *v_right + crandom()* 0.1 *v_up;
   dir = normalize(dir);
   self.v_angle = vectoangles(dir);
   EnemyFireBullets (6, dir, '0.04 0.04 0');
};

/*
============
Gremlin_FireSuperShotGun

fire a shotgun
============
*/
void() Gremlin_FireSuperShotGun =
{
   local vector dir;

   self.currentammo = self.ammo_shells = self.ammo_shells - 2;
   self.effects = self.effects | EF_MUZZLEFLASH;

   sound (self ,CHAN_WEAPON, "weapons/shotgn2.wav", 1, ATTN_NORM);
   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.3 *v_right + crandom()* 0.3 *v_up;
   dir = normalize(dir);
   self.v_angle = vectoangles(dir);
   EnemyFireBullets (14, dir, '0.14 0.08 0');
};

/*
============
Gremlin_FireLightningGun

fire lightning gun
============
*/
void() Gremlin_FireLightningGun =
   {
    local    vector    org, dir;
   local float cells;

// explode if under water
   if (self.watertype <= CONTENT_WATER)
    {
        cells = self.ammo_cells;
        self.ammo_cells = 0;
      discharged = 1;
        T_RadiusDamage (self, self, 35*cells, world);
      discharged = 0;
//      W_SetCurrentAmmo ();
        return;
    }

   self.effects = self.effects | EF_MUZZLEFLASH;

    ai_face ();

   self.currentammo = self.ammo_cells = self.ammo_cells - 2;
   org = self.origin + '0 0 16';

    dir = self.enemy.origin + '0 0 16' - org;
    dir = normalize (dir);
   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 *v_right + crandom()* 0.1 *v_up;
   dir = normalize(dir);

    traceline (org, self.origin + dir*600, TRUE, self);

   WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);

   LightningDamage (org, trace_endpos+(dir*4), self, 30);
};

/*
================
GremlinFireProximityGrenade
================
*/
void() GremlinFireProximityGrenade =
{
   local entity missile;
   local vector dir;

   NumProximityGrenades = NumProximityGrenades + 1;
   self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;

    sound (self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);

    missile = spawn ();
   missile.owner = self;
   missile.lastvictim = self;
   missile.movetype = MOVETYPE_TOSS;
    missile.solid = SOLID_BBOX;
   missile.classname = "proximity_grenade";
   missile.takedamage = DAMAGE_NO;
   missile.health = 5;
   missile.state = 0;

// set missile speed

   dir = normalize(self.enemy.origin - self.origin);
   self.v_angle = vectoangles(dir);
   makevectors(self.v_angle);
   dir = dir + crandom()* 0.1 *v_right + crandom()* 0.1 *v_up;
   dir = normalize(dir);
   missile.velocity = dir * 600;
    missile.velocity_z = 200;

   missile.avelocity = '100 600 100';

    missile.angles = vectoangles(missile.velocity);

   missile.touch = ProximityGrenadeTouch;

// set missile duration
   missile.nextthink = time + 2;
   missile.delay = time + 15 + (10*random());
   missile.think = ProximityBomb;
   missile.th_die = ProximityGrenadeExplode;

   setmodel (missile, "progs/proxbomb.mdl");
   setorigin (missile, self.origin);
   setsize (missile, '-1 -1 -1', '1 1 1');
};

/*
============
GremlinWeaponAttack

attack with a weapon
============
*/
void()   gremlin_shot1;
void()   gremlin_nail1;
void()   gremlin_light1;
void()   gremlin_rocket1;

float() GremlinWeaponAttack =
   {
    local    float    r;
   local entity targ;
   local vector vec;


   if (!GremlinCheckNoAmmo ())
      return FALSE;

   self.show_hostile = time + 1; // wake monsters up

   if (self.weapon == IT_SHOTGUN)
      {
      gremlin_shot1 ();
      Gremlin_FireShotGun();
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_SUPER_SHOTGUN)
      {
      gremlin_shot1 ();
      Gremlin_FireSuperShotGun();
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_NAILGUN)
      {
      gremlin_nail3 ();
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_SUPER_NAILGUN)
      {
      gremlin_nail3 ();
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_GRENADE_LAUNCHER)
      {
      gremlin_rocket1();
      OgreFireGrenade();
      self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_ROCKET_LAUNCHER)
      {
      gremlin_rocket1();
      Gremlin_FireRocket();
      SUB_AttackFinished(1);
      }
    else if (self.weapon == IT_LIGHTNING)
      {
      gremlin_light1();
      SUB_AttackFinished(1);
      sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
      }
   else if (self.weapon == IT_LASER_CANNON)
      {
      gremlin_laser3();
      SUB_AttackFinished(1);
      }
   else if (self.weapon == IT_PROXIMITY_GUN)
      {
      gremlin_rocket1();
      GremlinFireProximityGrenade();
      SUB_AttackFinished(1);
      }
   return TRUE;
   };

void() Gremlin_MissileAttack =
   {
   if (self.stoleweapon)
      {
      if (GremlinWeaponAttack())
         return;
      else if ((random()<0.1) && (self.flags & FL_ONGROUND))
         {
         gremlin_jump1();
         return;
         }
      }
   if (self.flags & FL_ONGROUND)
      gremlin_jump1();
   };

/*QUAKED monster_gremlin (1 0 0) (-32 -32 -24) (32 32 64) Ambush

*/
void() monster_gremlin =
{
   if (deathmatch)
    {
        remove(self);
        return;
    }
   NumGremlins = NumGremlins + 1;
   precache_model ("progs/grem.mdl");
   precache_model ("progs/h_grem.mdl");

   precache_sound ("grem/death.wav");
   precache_sound ("grem/attack.wav");
   precache_sound ("demon/djump.wav");
   precache_sound ("demon/dhit2.wav");
   precache_sound ("grem/pain1.wav");
   precache_sound ("grem/pain2.wav");
   precache_sound ("grem/pain3.wav");
   precache_sound ("grem/idle.wav");
   precache_sound ("grem/sight1.wav");

   self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

   setmodel (self, "progs/grem.mdl");

   setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
   self.health = 100;
   self.max_health = 101;
   self.yaw_speed = 40;

   self.th_stand = gremlin_stand1;
   self.th_walk = gremlin_walk1;
   self.th_run = gremlin_run1;
   self.th_die = gremlin_die;
   self.th_melee = Gremlin_MeleeAttack;     // one of two attacks
   self.th_missile = Gremlin_MissileAttack; // check for random jump or firing of weapon
   self.th_pain = gremlin_pain;

    walkmonster_start();
};

float()  GremlinCheckAttack =
   {
    local vector    spot1, spot2;
    local entity    targ;
    local float        chance;

    targ = self.enemy;

   if (time < self.attack_finished)
      return FALSE;

// see if any entities are in the way of the shot
   spot1 = self.origin;// + self.view_ofs;
   spot2 = targ.origin;// + targ.view_ofs;

   if ((vlen(spot2 - spot1) <= 90) && (self.stoleweapon == 0))
      {
      self.attack_state = AS_MELEE;
      return TRUE;
      }
// missile attack
   chance = 0.03 + self.stoleweapon;
   if (random() < chance)
      {
      self.attack_state = AS_MISSILE;
      return TRUE;
      }
    return FALSE;
   };


//===========================================================================

void(float side)  Gremlin_Melee =
{
    local    float    ldmg;
    local vector    delta;

    ai_face ();

//   walkmove (self.ideal_yaw, 12);   // allow a little closing


    delta = self.enemy.origin - self.origin;

    if (vlen(delta) > 100)
        return;
    if (!CanDamage (self.enemy, self))
        return;

    sound (self, CHAN_WEAPON, "grem/attack.wav", 1, ATTN_NORM);
    ldmg = 10 + 5*random();
    T_Damage (self.enemy, self, self, ldmg);

    makevectors (self.angles);
    SpawnMeatSpray (self.origin + v_forward*16, side * v_right);
};

//===========================================================================
void(float dm) Gremlin_ThrowHead =
   {
   local string gibname;
   if (self.classname == "monster_ogre")
      gibname = "progs/h_ogre.mdl";
    else if (self.classname == "monster_knight")
      gibname = "progs/h_knight.mdl";
   else if (self.classname == "monster_shambler")
      gibname = "progs/h_shams.mdl";
   else if (self.classname == "monster_demon1")
      gibname = "progs/h_demon.mdl";
   else if (self.classname == "monster_wizard")
      gibname = "progs/h_wizard.mdl";
   else if (self.classname == "monster_zombie")
      gibname = "progs/h_zombie.mdl";
   else if (self.classname == "monster_dog")
      gibname = "progs/h_dog.mdl";
   else if (self.classname == "monster_hell_knight")
      gibname = "progs/h_hellkn.mdl";
   else if (self.classname == "monster_enforcer")
      gibname = "progs/h_mega.mdl";
   else if (self.classname == "monster_army")
      gibname = "progs/h_guard.mdl";
   else if (self.classname == "monster_shalrath")
      gibname = "progs/h_shal.mdl";
   else if (self.classname == "monster_gremlin")
      gibname = "progs/h_grem.mdl";
   else if (self.classname == "monster_scourge")
      gibname = "progs/h_scourg.mdl";
   else if (self.classname == "monster_fish")
      gibname = "progs/gib1.mdl";
   else
      gibname = "progs/h_player.mdl";
   ThrowHead(gibname,dm);
   };
//===========================================================================

/*
============
Gremlin_Damage

The damage is coming from inflictor, but get mad at attacker
============
*/
void(entity targ, entity inflictor, entity attacker, float damage) Gremlin_Damage=
{
// check for godmode or invincibility
    if (targ.flags & FL_GODMODE)
        return;
    if (targ.invincible_finished >= time)
    {
        if (self.invincible_sound < time)
        {
            sound (targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
            self.invincible_sound = time + 2;
        }
        return;
    }

// team play damage avoidance
    if ( (teamplay == 1) && (targ.team > 0)&&(targ.team == attacker.team) )
        return;

// do the damage
   targ.health = targ.health - damage;
   };

void() Gremlin_Split =
   {
   local entity grem;
   local entity temp;
   local entity head;
   local float done;
   local vector ang;
   local float c;
   local vector pos;
   local float proceed;

   if (NumSpawnGremlins >= (NumGremlins*2))
      return;
   done = 0;
   c = 0;
   ang = self.angles;
   while (done == 0)
      {
      makevectors(ang);
      pos = self.origin + (80 * v_forward);
      head = findradius(pos, 35);
      proceed = 1;
      while (head)
         {
         if ((head.health > 0) && (head.flags & (FL_MONSTER | FL_CLIENT)))
            proceed = 0;
         head = head.chain;
         }
      traceline(self.origin,pos,FALSE,self);
      if (trace_fraction == 1 && (proceed == 1))
         {
         traceline(self.origin,(pos-'40 40 0'),FALSE,self);
         if (trace_fraction == 1)
            {
            traceline(self.origin,(pos+'40 40 0'),FALSE,self);
            if (trace_fraction == 1)
               {
               traceline(self.origin,(pos + '0 0 64'),FALSE,self);
               if (trace_fraction == 1)
                  {
                  traceline(self.origin,(pos - '0 0 64'),FALSE,self);
                  if (trace_fraction != 1)
                     {
                     done = 1;
                     }
                  }
               }
            }
         }
      if (done == 0)
         {
         ang_y = ang_y + 36;
         c = c + 1;
         if (c==10)
            {
            return;
            }
         }
      }
   NumSpawnGremlins = NumSpawnGremlins + 1;
   grem = spawn();
   SUB_CopyEntity(self,grem);
   grem.solid = SOLID_SLIDEBOX;
   grem.movetype = MOVETYPE_STEP;
   setmodel (grem, "progs/grem.mdl");
   setsize (grem, VEC_HULL_MIN, VEC_HULL_MAX);
   if (self.health < 100) self.health = 100;
   grem.health = self.health / 2;
   self.health = self.health / 2;
//   grem.max_health = 101;
//   grem.gorging = FALSE;
   grem.stoleweapon = FALSE;
   grem.items = 0;
   total_monsters = total_monsters + 1;
   WriteByte (MSG_BROADCAST, SVC_UPDATESTAT);
   WriteByte (MSG_BROADCAST, STAT_TOTALMONSTERS);
   WriteLong (MSG_BROADCAST, total_monsters);
   setorigin(grem, pos);
   temp = self;
   self = grem;
   gremlin_spawn1();
   self.enemy = world;
   self.gorging = FALSE;
   self = temp;
   };

void(float side)  Gremlin_Gorge =
   {
    local    float    ldmg;
    local vector    delta;
   local entity temp;

//   ai_face ();
//   walkmove (self.ideal_yaw, 6);   // allow a little closing

    delta = self.enemy.origin - self.origin;
//   self.enemy.takedamage = DAMAGE_YES;

    sound (self, CHAN_WEAPON, "demon/dhit2.wav", 1, ATTN_NORM);
//    sound (self, CHAN_WEAPON, "grem/attack.wav", 1, ATTN_NORM);
   ldmg = 7+5*random();
   Gremlin_Damage (self.enemy, self, self, ldmg);

    makevectors (self.angles);
    SpawnMeatSpray (self.origin + v_forward*16, side * v_right);
   if (self.enemy.health < -200.0)
      {
      if (self.enemy.gorging==FALSE)
         {
         self.enemy.gorging = TRUE;
         sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NORM);
         temp = self;
         self = self.enemy;
         Gremlin_ThrowHead(-15);
//         ThrowGib ("progs/gib1.mdl", -15);
//         ThrowGib ("progs/gib2.mdl", -15);
//         ThrowGib ("progs/gib3.mdl", -15);
         self = temp;
         ldmg = 150 + 100*random();
         T_Heal(self,ldmg,FALSE);
//         if (self.health >= self.max_health)
            Gremlin_Split();
         }
      self.enemy = world;
      self.gorging = FALSE;
/*
      if (self.stoleweapon)
         {
         temp = GremlinFindVictim();
         if (temp != world)
            {
            self.enemy = temp;
            FoundTarget();
            self.search_time = time + 1.0;
//            return;
            }
         }
*/
//      FindTarget();
/*
      if (self.oldenemy.health > 0)
         {
         self.enemy = self.oldenemy;
         HuntTarget ();
         }
      else
         {
         if (self.movetarget)
            self.th_walk ();
         else
            self.th_stand ();
         }
*/
      gremlin_look1();
      }
   };


void()   Gremlin_JumpTouch =
{
    local    float    ldmg;

   if (self.health <= 0)
        return;

/*
    if (other.takedamage)
    {
        if ( vlen(self.velocity) > 400 )
        {
            ldmg = 40 + 10*random();
            T_Damage (other, self, self, ldmg);
        }
    }
*/
    if (!checkbottom(self))
      {
        if (self.flags & FL_ONGROUND)
         {
         self.touch = SUB_Null;
         self.think = gremlin_jump1;
         self.nextthink = time + 0.1;
         }
        return;    // not on ground yet
      }

    self.touch = SUB_Null;
   self.think = gremlin_jump12;
    self.nextthink = time + 0.1;
};

void()   Gremlin_FlipTouch =
{
    local    float    ldmg;

    if (!checkbottom(self))
      {
        if (self.flags & FL_ONGROUND)
         {
         self.touch = SUB_Null;
         self.think = gremlin_flip1;
         self.nextthink = time + 0.1;
         }
        return;    // not on ground yet
      }

    self.touch = SUB_Null;
   self.think = gremlin_flip8;
    self.nextthink = time + 0.1;
};
