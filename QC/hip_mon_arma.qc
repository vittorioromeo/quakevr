/*
==============================================================================

armagon

==============================================================================
*/


$cd hipwork\models\armagon
$origin 0 0 24
$base bodybase.asc

$skin armab

$frame lwalk1.asc lwalk2.asc lwalk3.asc lwalk4.asc lwalk5.asc lwalk6.asc lwalk7.asc
$frame lwalk8.asc lwalk9.asc lwalk10.asc lwalk11.asc lwalk12.asc

$frame lw0fir1.asc lw0fir2.asc lw0fir3.asc lw0fir4.asc lw0fir5.asc lw0fir6.asc
$frame lw0fir7.asc lw0fir8.asc lw0fir9.asc lw0fir10.asc lw0fir11.asc lw0fir12.asc
$frame lw0fir13.asc

$frame lw1fir1.asc lw1fir2.asc lw1fir3.asc lw1fir4.asc lw1fir5.asc lw1fir6.asc
$frame lw1fir7.asc lw1fir8.asc lw1fir9.asc lw1fir10.asc lw1fir11.asc lw1fir12.asc
$frame lw1fir13.asc lw1fir14.asc lw1fir15.asc

$frame lw2fir1.asc lw2fir2.asc lw2fir3.asc lw2fir4.asc lw2fir5.asc lw2fir6.asc
$frame lw2fir7.asc lw2fir8.asc lw2fir9.asc lw2fir10.asc lw2fir11.asc lw2fir12.asc

$frame lsfire1.asc lsfire2.asc lsfire3.asc lsfire4.asc lsfire5.asc lsfire6.asc
$frame lsfire7.asc lsfire8.asc lsfire9.asc lsfire10.asc lsfire11.asc lsfire12.asc

$frame lstand1.asc lstand2.asc lstand3.asc lstand4.asc lstand5.asc lstand6.asc
$frame lstand7.asc lstand8.asc lstand9.asc lstand10.asc lstand11.asc lstand12.asc
$frame lstand13.asc lstand14.asc lstand15.asc lstand16.asc lstand17.asc
$frame lstand18.asc lstand19.asc lstand20.asc

$frame tranl1.asc tranl2.asc

$frame ldie1.asc ldie2.asc ldie3.asc ldie4.asc ldie5.asc ldie6.asc
$frame ldie7.asc ldie8.asc ldie9.asc ldie10.asc ldie11.asc ldie12.asc

float ATTN_ARMAGON = 0.5;
float SERVO_VOLUME = 0.5;

void(float ofs, float turn) armagon_launch_missile =
   {
   local vector   src, vec, targ;
   local vector   ang;
   local entity   missile;
   local float    dist;

   ang = self.angles;
   ang_y = ang_y + self.fixangle;
   if(turn==1)
      {
      ang_y = ang_y + 165;
      }
   else if(turn==2)
      {
      ang_y = ang_y - 165;
      }
   makevectors(ang);
   src = self.origin + '0 0 66' + v_right*ofs + v_forward*84;
   targ = self.enemy.origin + self.enemy.view_ofs;
   if(skill!=0)
      {
      dist = vlen(targ-src);
      targ = targ + (self.enemy.velocity * (dist/1000));
      }
   vec = (targ - src);
   vec = normalize(vec);
   dist = vec*v_forward;
   if(dist<self.worldtype)
      vec=v_forward;
   self.effects = self.effects | EF_MUZZLEFLASH;

    sound(self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";

// set missile speed
   missile.velocity = vec * 1000;
   missile.angles = vectoangles(vec);
    missile.touch = T_MissileTouch;

// set missile duration
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;

    setmodel(missile, "progs/missile.mdl");
    setsize(missile, '0 0 0', '0 0 0');
   setorigin(missile, src);
   };

void(float ofs) armagon_launch_laser =
   {
   local vector   src, vec, targ;
   local float    dist;
   local vector   ang;

   ang = self.angles;
   ang_y = ang_y + self.fixangle;
   makevectors(ang);
   src = self.origin + '0 0 66' + v_right*ofs + v_forward*84;
   targ = self.enemy.origin + self.enemy.view_ofs;
   if(skill!=0)
      {
      dist = vlen(targ-src);
      targ = targ + (self.enemy.velocity * (dist/1000));
      }
   vec = (targ - src);
   vec = normalize(vec);
   dist = vec*v_forward;
   if(dist<self.worldtype)
      vec=v_forward;
   self.effects = self.effects | EF_MUZZLEFLASH;

   HIP_LaunchLaser(src, vec, 0);
   };

void(float angdelta, float delta) armagon_turn =
   {
   if(fabs(angdelta)<10)
      self.fixangle = delta;
   else
      {
      if(angdelta > 5)
         {
         self.fixangle = self.fixangle + 9;
         }
      else if(angdelta < -5)
         {
         self.fixangle = self.fixangle - 9;
         }
      else
         {
         self.fixangle = delta;
         }
      }
   };
void() armagon_overleft1;
void() armagon_overright1;

void() armagon_think =
   {
   local float delta;
   local float angdelta;
   setorigin(self.trigger_field, self.origin);
   self.trigger_field.angles = self.angles;
   self.trigger_field.frame = self.frame;
   self.trigger_field.angles_y = self.trigger_field.angles_y + self.fixangle;
   enemy_yaw = vectoyaw(self.enemy.origin - self.origin);
   self.ideal_yaw = enemy_yaw;
   delta = self.ideal_yaw - self.angles_y;
   self.cnt = 0;
   if(delta > 180) delta = delta - 360;
   if(delta < -180) delta = delta + 360;
   if(fabs(delta) > 90)
      {
      delta = 0;
      self.cnt = 1;
      }
   angdelta = delta-self.fixangle;
   armagon_turn(angdelta,delta);
//   self.nextthink = time + 0.1;
   if(self.health < 0)
      return;
   if(time > self.super_time)
      {
      local float r;

      self.super_time = time + 3;
      if(random() < 0.5)
         {
         r = random();
         if(r < 0.25)
            sound(self, CHAN_VOICE, "armagon/idle1.wav", 1, ATTN_ARMAGON);
         else if(r < 0.5)
            sound(self, CHAN_VOICE, "armagon/idle2.wav", 1, ATTN_ARMAGON);
         else if(r < 0.75)
            sound(self, CHAN_VOICE, "armagon/idle3.wav", 1, ATTN_ARMAGON);
         else
            sound(self, CHAN_VOICE, "armagon/idle4.wav", 1, ATTN_ARMAGON);
         }
      }
   };

void() armagon_walkthink =
   {
   local float delta;
   local float angdelta;
   local entity client;
   setorigin(self.trigger_field, self.origin);
   self.trigger_field.angles = self.angles;
   self.trigger_field.frame = self.frame;
   self.trigger_field.angles_y = self.trigger_field.angles_y + self.fixangle;
   ChangeYaw();
   delta = 0;
   self.cnt = 0;
   if(delta > 180) delta = delta - 360;
   if(delta < -180) delta = delta + 360;
   if(fabs(delta) > 90)
      {
      delta = 0;
      self.cnt = 1;
      }
   angdelta = delta-self.fixangle;
   armagon_turn(angdelta,delta);
   if(self.health < 0)
      return;
   if(time > self.super_time)
      {
      local float r;

      self.super_time = time + 3;
      if(random() < 0.5)
         {
         r = random();
         if(r < 0.25)
            sound(self, CHAN_VOICE, "armagon/idle1.wav", 1, ATTN_ARMAGON);
         else if(r < 0.5)
            sound(self, CHAN_VOICE, "armagon/idle2.wav", 1, ATTN_ARMAGON);
         else if(r < 0.75)
            sound(self, CHAN_VOICE, "armagon/idle3.wav", 1, ATTN_ARMAGON);
         else
            sound(self, CHAN_VOICE, "armagon/idle4.wav", 1, ATTN_ARMAGON);
         }
      }
   client = checkclient();
   if(!client)
      return;  // current check entity isn't in PVS
   if(visible(client))
      {
      self.enemy = client;
      FoundTarget();
      }
   };

void() armagon_overleft_think =
   {
   local float delta;
   local float angdelta;

   ChangeYaw();
   walkmove (self.angles_y, 14);
   setorigin(self.trigger_field, self.origin);
   self.trigger_field.angles = self.angles;
   self.trigger_field.frame = self.frame;
   self.trigger_field.angles_y = self.trigger_field.angles_y + self.fixangle;
   if(self.count == 0)
      {
      enemy_yaw = vectoyaw(self.enemy.origin - self.origin);
      self.ideal_yaw = enemy_yaw;
      delta = self.ideal_yaw - self.angles_y - 165;
      if(delta > 180) delta = delta - 360;
      if(delta < -180) delta = delta + 360;
      angdelta = delta-self.fixangle;
      }
   else if(self.count == 1)
      {
      armagon_launch_missile(40,1);
      return;
      }
   else
      {
      delta = 0;
      angdelta = delta-self.fixangle;
      }
   armagon_turn(angdelta,delta);
//   self.nextthink = time + 0.1;
   };

void() armagon_overright_think =
   {
   local float delta;
   local float angdelta;

   ChangeYaw();
   walkmove (self.angles_y, 14);
   setorigin(self.trigger_field, self.origin);
   self.trigger_field.angles = self.angles;
   self.trigger_field.frame = self.frame;
   self.trigger_field.angles_y = self.trigger_field.angles_y + self.fixangle;
   if(self.count == 0)
      {
      enemy_yaw = vectoyaw(self.enemy.origin - self.origin);
      self.ideal_yaw = enemy_yaw;
      delta = self.ideal_yaw - self.angles_y + 165;
      if(delta > 180) delta = delta - 360;
      if(delta < -180) delta = delta + 360;
      angdelta = delta-self.fixangle;
      }
   else if(self.count == 1)
      {
      armagon_launch_missile(-40,2);
      return;
      }
   else
      {
      delta = 0;
      angdelta = delta-self.fixangle;
      }
   armagon_turn(angdelta,delta);
//   self.nextthink = time + 0.1;
   };

void() armagon_stand_attack;

void()   armagon_stand1  =[ $lstand1.asc, armagon_stand2  ] {ai_stand();armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand2  =[ $lstand2.asc, armagon_stand3  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand3  =[ $lstand3.asc, armagon_stand4  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand4  =[ $lstand4.asc, armagon_stand5  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand5  =[ $lstand5.asc, armagon_stand6  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand6  =[ $lstand6.asc, armagon_stand7  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand7  =[ $lstand7.asc, armagon_stand8  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand8  =[ $lstand8.asc, armagon_stand9  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand9  =[ $lstand9.asc, armagon_stand10  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand10  =[ $lstand10.asc, armagon_stand11  ] {ai_stand();armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand11  =[ $lstand11.asc, armagon_stand12  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand12  =[ $lstand12.asc, armagon_stand13  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand13  =[ $lstand13.asc, armagon_stand14  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand14  =[ $lstand14.asc, armagon_stand15  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand15  =[ $lstand15.asc, armagon_stand16  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand16  =[ $lstand16.asc, armagon_stand17  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand17  =[ $lstand17.asc, armagon_stand18  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand18  =[ $lstand18.asc, armagon_stand19  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand19  =[ $lstand19.asc, armagon_stand20  ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_stand20  =[ $lstand20.asc, armagon_stand1  ] {ai_stand();armagon_think();self.nextthink = time + 0.2;};

void()   armagon_walk1    =[ $lwalk1.asc  , armagon_walk2    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk2    =[ $lwalk2.asc  , armagon_walk3    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk3    =[ $lwalk3.asc  , armagon_walk4    ]
   {
   sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk4    =[ $lwalk4.asc  , armagon_walk5    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk5    =[ $lwalk5.asc  , armagon_walk6    ]
   {
   sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk6    =[ $lwalk6.asc  , armagon_walk7    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk7    =[ $lwalk7.asc  , armagon_walk8    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk8    =[ $lwalk8.asc  , armagon_walk9    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk9    =[ $lwalk9.asc  , armagon_walk10    ]
   {
   sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk10    =[ $lwalk10.asc  , armagon_walk11    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk11    =[ $lwalk11.asc  , armagon_walk12    ]
   {
   sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);
   movetogoal(14);
   armagon_walkthink();
   };
void()   armagon_walk12    =[ $lwalk12.asc  , armagon_walk1    ]
   {
   movetogoal(14);
   armagon_walkthink();
   };

void()   armagon_run1    =[ $lwalk1.asc  , armagon_run2    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run2    =[ $lwalk2.asc  , armagon_run3    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run3    =[ $lwalk3.asc  , armagon_run4    ]
   {
   sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run4    =[ $lwalk4.asc  , armagon_run5    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run5    =[ $lwalk5.asc  , armagon_run6    ]
   {
   sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run6    =[ $lwalk6.asc  , armagon_run7    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run7    =[ $lwalk7.asc  , armagon_run8    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run8    =[ $lwalk8.asc  , armagon_run9    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run9    =[ $lwalk9.asc  , armagon_run10    ]
   {
   sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run10    =[ $lwalk10.asc  , armagon_run11    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run11    =[ $lwalk11.asc  , armagon_run12    ]
   {
   sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   };
void()   armagon_run12    =[ $lwalk12.asc  , armagon_run1    ]
   {
   ChangeYaw();
   RUN_STRAIGHT = 1;
   ai_run(14);
   armagon_think();
   if((self.cnt == 1) && (time > self.attack_finished))
      {
      local float delta;
      delta = self.ideal_yaw - self.angles_y;
      if(delta > 180) delta = delta - 360;
      if(delta < -180) delta = delta + 360;
      if(delta>0)
         self.think = armagon_overleft1;
      else
         self.think = armagon_overright1;
      return;
      }
   if(self.lefty == 1)
      {
      self.lefty = 0;
      self.think = self.th_missile;
      }
   };

void()   armagon_watk1    =[ $lw0fir1.asc, armagon_watk2  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk2    =[ $lw0fir2.asc, armagon_watk3  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk3    =[ $lw0fir3.asc, armagon_watk4  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk4    =[ $lw0fir4.asc, armagon_watk5  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk5    =[ $lw0fir5.asc, armagon_watk6  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk6    =[ $lw0fir6.asc, armagon_watk7  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();armagon_launch_missile(40,0);};
void()   armagon_watk7    =[ $lw0fir7.asc, armagon_watk8  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk8    =[ $lw0fir8.asc, armagon_watk9  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk9    =[ $lw0fir9.asc, armagon_watk10  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk10    =[ $lw0fir10.asc, armagon_watk11  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk11    =[ $lw0fir11.asc, armagon_watk13  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();armagon_launch_missile(-40,0);};
//void()   armagon_watk12    =[ $lw0fir12.asc, armagon_watk13  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_watk13    =[ $lw0fir12.asc, armagon_run1  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();SUB_AttackFinished (1.0);};

void()   armagon_wlaseratk1    =[ $lw0fir1.asc, armagon_wlaseratk2  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk2    =[ $lw0fir2.asc, armagon_wlaseratk3  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk3    =[ $lw0fir3.asc, armagon_wlaseratk4  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk4    =[ $lw0fir4.asc, armagon_wlaseratk5  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk5    =[ $lw0fir5.asc, armagon_wlaseratk6  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk6    =[ $lw0fir6.asc, armagon_wlaseratk7  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();armagon_launch_laser(40);};
void()   armagon_wlaseratk7    =[ $lw0fir7.asc, armagon_wlaseratk8  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk8    =[ $lw0fir8.asc, armagon_wlaseratk9  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk9    =[ $lw0fir9.asc, armagon_wlaseratk10  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk10    =[ $lw0fir10.asc, armagon_wlaseratk11  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk11    =[ $lw0fir11.asc, armagon_wlaseratk13  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();armagon_launch_laser(-40);};
//void()   armagon_wlaseratk12    =[ $lw0fir12.asc, armagon_wlaseratk13  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();};
void()   armagon_wlaseratk13    =[ $lw0fir12.asc, armagon_run1  ] {ChangeYaw();walkmove (self.angles_y, 14);armagon_think();SUB_AttackFinished (1.0);};

void()   armagon_overleft1    =[ $lw1fir1.asc, armagon_overleft2  ] {self.count=0;armagon_overleft_think();};
void()   armagon_overleft2    =[ $lw1fir2.asc, armagon_overleft3  ] {armagon_overleft_think();};
void()   armagon_overleft3    =[ $lw1fir3.asc, armagon_overleft4  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);armagon_overleft_think();};
void()   armagon_overleft4    =[ $lw1fir4.asc, armagon_overleft5  ] {armagon_overleft_think();};
void()   armagon_overleft5    =[ $lw1fir5.asc, armagon_overleft6  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);armagon_overleft_think();};
void()   armagon_overleft6    =[ $lw1fir6.asc, armagon_overleft7  ] {armagon_overleft_think();};
void()   armagon_overleft7    =[ $lw1fir7.asc, armagon_overleft8  ] {armagon_overleft_think();};
void()   armagon_overleft8    =[ $lw1fir8.asc, armagon_overleft9  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);armagon_overleft_think();};
void()   armagon_overleft9    =[ $lw1fir9.asc, armagon_overleft10  ] {armagon_overleft_think();};
void()   armagon_overleft10    =[ $lw1fir10.asc, armagon_overleft11  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);armagon_overleft_think();};
void()   armagon_overleft11    =[ $lw1fir11.asc, armagon_overleft12  ] {self.count=1;armagon_overleft_think();};
void()   armagon_overleft12    =[ $lw1fir12.asc, armagon_overleft13  ] {self.count=2;armagon_overleft_think();};
void()   armagon_overleft13    =[ $lw1fir13.asc, armagon_overleft14  ] {armagon_overleft_think();};
void()   armagon_overleft14    =[ $lw1fir14.asc, armagon_overleft15  ] {armagon_overleft_think();};
void()   armagon_overleft15    =[ $lw1fir15.asc, armagon_run1  ] {armagon_overleft_think();SUB_AttackFinished (1.0);};

void()   armagon_overright1    =[ $lw2fir1.asc, armagon_overright2  ] {self.count=0;armagon_overright_think();};
void()   armagon_overright2    =[ $lw2fir2.asc, armagon_overright3  ] {armagon_overright_think();};
void()   armagon_overright3    =[ $lw2fir3.asc, armagon_overright4  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);armagon_overright_think();};
void()   armagon_overright4    =[ $lw2fir4.asc, armagon_overright5  ] {armagon_overright_think();};
void()   armagon_overright5    =[ $lw2fir5.asc, armagon_overright6  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);self.count=1;armagon_overright_think();};
void()   armagon_overright6    =[ $lw2fir6.asc, armagon_overright7  ] {self.count=2;armagon_overright_think();};
void()   armagon_overright7    =[ $lw2fir7.asc, armagon_overright8  ] {armagon_overright_think();};
void()   armagon_overright8    =[ $lw2fir8.asc, armagon_overright9  ] {sound(self, 7, "armagon/servo.wav", SERVO_VOLUME, ATTN_ARMAGON);armagon_overright_think();};
void()   armagon_overright9    =[ $lw2fir9.asc, armagon_overright10  ] {armagon_overright_think();};
void()   armagon_overright10    =[ $lw2fir10.asc, armagon_overright11  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);armagon_overright_think();};
void()   armagon_overright11    =[ $lw2fir11.asc, armagon_overright12  ] {armagon_overright_think();};
void()   armagon_overright12    =[ $lw2fir12.asc, armagon_run1  ] {armagon_overright_think();SUB_AttackFinished (1.0);};

void()   armagon_plant1  =[ $lstand1.asc, armagon_plant1  ] {armagon_stand_attack();armagon_think();};

void()   armagon_stop1  =[ $tranl1.asc, armagon_stop2  ] {armagon_think();};
void()   armagon_stop2  =[ $tranl2.asc, armagon_plant1  ] {sound(self, 6, "armagon/footfall.wav",1,ATTN_ARMAGON);armagon_think();};



void()   armagon_satk1    =[ $lsfire1.asc, armagon_satk2  ] {armagon_think();};
void()   armagon_satk2    =[ $lsfire2.asc, armagon_satk3  ] {armagon_think();};
void()   armagon_satk3    =[ $lsfire3.asc, armagon_satk4  ] {armagon_think();};
void()   armagon_satk4    =[ $lsfire4.asc, armagon_satk5  ] {armagon_think();};
void()   armagon_satk5    =[ $lsfire5.asc, armagon_satk6  ] {armagon_think();};
void()   armagon_satk6    =[ $lsfire6.asc, armagon_satk7  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);armagon_think();};
void()   armagon_satk7    =[ $lsfire7.asc, armagon_satk8  ] {armagon_think();};
void()   armagon_satk8    =[ $lsfire8.asc, armagon_satk9  ] {armagon_think();};
void()   armagon_satk9    =[ $lsfire9.asc, armagon_satk10 ] {armagon_think();armagon_launch_missile(40,0);armagon_launch_missile(-40,0);};
void()   armagon_satk10   =[ $lsfire10.asc, armagon_satk11 ] {armagon_think();};
void()   armagon_satk11   =[ $lsfire9.asc, armagon_satk12 ] {armagon_think();armagon_launch_missile(40,0);armagon_launch_missile(-40,0);};
void()   armagon_satk12   =[ $lsfire10.asc, armagon_satk13 ] {armagon_think();};
void()   armagon_satk13   =[ $lsfire9.asc, armagon_satk14 ] {armagon_think();armagon_launch_missile(40,0);armagon_launch_missile(-40,0);};
void()   armagon_satk14   =[ $lsfire10.asc, armagon_satk15 ] {armagon_think();};
void()   armagon_satk15   =[ $lsfire11.asc, armagon_satk16 ] {armagon_think();};
void()   armagon_satk16   =[ $lsfire12.asc, armagon_plant1   ] {armagon_think();SUB_AttackFinished (0.3);};

void()   armagon_slaser1    =[ $lsfire1.asc, armagon_slaser2  ] {armagon_think();};
void()   armagon_slaser2    =[ $lsfire2.asc, armagon_slaser3  ] {armagon_think();};
void()   armagon_slaser3    =[ $lsfire3.asc, armagon_slaser4  ] {armagon_think();};
void()   armagon_slaser4    =[ $lsfire4.asc, armagon_slaser5  ] {armagon_think();};
void()   armagon_slaser5    =[ $lsfire5.asc, armagon_slaser6  ] {armagon_think();};
void()   armagon_slaser6    =[ $lsfire6.asc, armagon_slaser7  ] {sound(self, 6, "armagon/footfall.wav", 1, ATTN_ARMAGON);armagon_think();};
void()   armagon_slaser7    =[ $lsfire7.asc, armagon_slaser8  ] {armagon_think();};
void()   armagon_slaser8    =[ $lsfire8.asc, armagon_slaser9  ] {armagon_think();};
void()   armagon_slaser9    =[ $lsfire9.asc, armagon_slaser10 ] {armagon_think();armagon_launch_laser(40);armagon_launch_laser(-40);};
void()   armagon_slaser10   =[ $lsfire10.asc, armagon_slaser11 ] {armagon_think();};
void()   armagon_slaser11    =[ $lsfire9.asc, armagon_slaser12 ] {armagon_think();armagon_launch_laser(40);armagon_launch_laser(-40);};
void()   armagon_slaser12   =[ $lsfire10.asc, armagon_slaser13 ] {armagon_think();};
void()   armagon_slaser13    =[ $lsfire9.asc, armagon_slaser14 ] {armagon_think();armagon_launch_laser(40);armagon_launch_laser(-40);};
void()   armagon_slaser14   =[ $lsfire10.asc, armagon_slaser15 ] {armagon_think();};
void()   armagon_slaser15    =[ $lsfire9.asc, armagon_slaser16 ] {armagon_think();armagon_launch_laser(40);armagon_launch_laser(-40);};
void()   armagon_slaser16   =[ $lsfire10.asc, armagon_slaser17 ] {armagon_think();};
void()   armagon_slaser17    =[ $lsfire9.asc, armagon_slaser18 ] {armagon_think();armagon_launch_laser(40);armagon_launch_laser(-40);};
void()   armagon_slaser18   =[ $lsfire10.asc, armagon_slaser19 ] {armagon_think();};
void()   armagon_slaser19   =[ $lsfire11.asc, armagon_slaser20 ] {armagon_think();};
void()   armagon_slaser20   =[ $lsfire12.asc, armagon_plant1   ] {armagon_think();SUB_AttackFinished (0.3);};

void()   armagon_body_explode1;

void()   armagon_die1    =[ $tranl1.asc, armagon_die2    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die2    =[ $tranl2.asc, armagon_die3    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die3    =[ $ldie1.asc, armagon_die4    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die4    =[ $ldie2.asc, armagon_die5    ]
   {
   armagon_think();
   multi_explosion( self.origin + '0 0 48', 48, 10, 6, 0.3, 0.3);
   sound(self, CHAN_AUTO, "armagon/death.wav", 1, ATTN_NONE);
   self.nextthink = time + 0.2;
   };
void()   armagon_die5    =[ $ldie3.asc, armagon_die6    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die6    =[ $ldie4.asc, armagon_die7    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die7    =[ $ldie5.asc, armagon_die8    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die8    =[ $ldie6.asc, armagon_die9    ]
   {
   armagon_think();
   self.nextthink = time + 2;
   };
void()   armagon_die9    =[ $ldie7.asc, armagon_die10    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die10    =[ $ldie8.asc, armagon_die11    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die11    =[ $ldie9.asc, armagon_die12    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die12    =[ $ldie10.asc, armagon_die13    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die13    =[ $ldie11.asc, armagon_die14    ] {armagon_think();self.nextthink = time + 0.2;};
void()   armagon_die14    =[ $ldie12.asc, armagon_die14    ]
   {
   local entity body;

   armagon_think();
   multi_explosion( self.origin + '0 0 80', 20, 10, 3, 0.1, 0.5);
   // legs
   self.nextthink = -1;
   self.movetype = MOVETYPE_NONE;
    self.takedamage = DAMAGE_NO;
   self.solid = SOLID_NOT;
   setsize(self, '-32 -32 -24', '32 32 32' );
   self.flags = 0;
   self.gorging = TRUE;
   self.wait = time + 5;

   //body
   body = self.trigger_field;
   body.nextthink = -1;
   self.movetype = MOVETYPE_NONE;
   body.takedamage = DAMAGE_NO;
   body.solid = SOLID_NOT;
//   setsize(body, '-32 -32 30', '32 32 64' );
   body.think = armagon_body_explode1;
   body.nextthink = time + 0.1;
   body.gorging = TRUE;
   };

void()   armagon_body_explode2 =
   {
   sound(self , CHAN_AUTO, "misc/longexpl.wav", 1, ATTN_ARMAGON);
   self.think = armagon_body_explode2;
   ThrowGib ("progs/gib1.mdl", -200);
   ThrowGib ("progs/gib2.mdl", -200);
   ThrowGib ("progs/gib3.mdl", -200);
   ThrowGib ("progs/gib1.mdl", -200);
   ThrowGib ("progs/gib2.mdl", -200);
   ThrowGib ("progs/gib3.mdl", -200);
   ThrowGib ("progs/gib1.mdl", -200);
   ThrowGib ("progs/gib2.mdl", -200);
   ThrowGib ("progs/gib3.mdl", -200);
   self.movetype = MOVETYPE_NONE;
   setmodel(self, "progs/s_explod.spr");
   self.solid = SOLID_NOT;
   self.nextthink = time + 0.1;
   s_explode1();
   };

void()   armagon_body_explode1 =
   {

   self.think = armagon_body_explode1;
   self.nextthink = time + 0.1;
   if(self.cnt == 0)
      {
      self.count = 0;
      }
   if(self.cnt < 25)
      {
      if(self.cnt > self.count)
         {
         ThrowGib ("progs/gib1.mdl", -100);
         ThrowGib ("progs/gib2.mdl", -100);
         ThrowGib ("progs/gib3.mdl", -100);
         self.count = self.cnt + 1;
         }
      self.cnt = self.cnt + 1;
      }
   else
      {
      self.cnt = 0;
      self.think = armagon_body_explode2;
      }
   };

void(entity attacker, float damage) armagon_pain =
   {
   if(self.health <= 0)
        return;        // allready dying, don't go into pain frame

   if(damage < 25)
      return;
    if(self.pain_finished > time)
        return;
    self.pain_finished = time + 2;

   sound(self, CHAN_VOICE, "armagon/pain.wav", 1, ATTN_NORM);
   };



void() armagon_missile_attack =
   {
   if(random()<0.5)
      armagon_watk1();
   else
      armagon_wlaseratk1();
//   self.nextthink = time + 0.1;
   };

void() armagon_repulse_attack =
   {
   armagon_think();
   if(self.state == 0)
      {
      SUB_AttackFinished (0.5);
      sound(self, CHAN_BODY, "armagon/repel.wav", 1, ATTN_NORM);
      self.state = 1;
      return;
      }
   else if(self.state == 1)
      {
      local entity head;
      local vector dir;

      // look in our immediate vicinity
      head = findradius(self.origin, 300);
      while(head)
         {
         if(!(head.flags & FL_NOTARGET) && (head.flags & FL_CLIENT))
            {
            if(visible(head) && (head.health > 0))
               {
               dir = head.origin - (self.origin - '0 0 24');
               dir = normalize(dir);
               head.velocity = head.velocity + dir*1500;
               }
            }
         head = head.chain;
         }
      T_RadiusDamage(self, self, 60, self);
      self.state = 0;
      SUB_AttackFinished (0.1);
      }
//   self.nextthink = time + 0.1;
   };

void() armagon_stand_attack =
   {
   local vector   spot1, spot2;

// see if any entities are in the way of the shot
   spot1 = self.origin + self.view_ofs;
   spot2 = self.enemy.origin + self.enemy.view_ofs;

   traceline(spot1, spot2, FALSE, self);

   if(trace_ent != self.enemy)
      {
      armagon_run1();
      return;
      }

    if(trace_inopen && trace_inwater)
      {
      armagon_run1();
      return;
      }

   if(time < self.attack_finished)
      {
      return;
      }

   if((vlen(spot2-spot1) < 200) && (self.enemy.classname == "player"))
      {
      armagon_repulse_attack();
      return;
      }

   self.state = 0;

   if(vlen(spot2-spot1) > 450)
      {
      armagon_run1();
      return;
      }

   if(random()<0.5)
      armagon_satk1();
   else
      armagon_slaser1();

   if(self.cnt == 1)
      {
      armagon_run1();
      return;
      }
   };


float()  ArmagonCheckAttack =
   {
    local vector    spot1, spot2;
    local entity    targ;
   local float    ang;
   local float    delta;
   local float    dist;

   self.lefty = 0;

    targ = self.enemy;

// see if any entities are in the way of the shot
   spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    traceline(spot1, spot2, FALSE, self);

   if((trace_ent != targ) && (!self.charmed))
      return FALSE;     // don't have a clear shot

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

   if(time < self.attack_finished)
        return FALSE;

   ang = self.angles_y + self.fixangle;
   delta = self.ideal_yaw - ang;
   dist = vlen(spot2-spot1);
   if((fabs(delta) > 10 && (dist > 200)) || (self.enemy.classname != "player"))
      return FALSE;
   if(dist < 400)
      {  // melee attack
      self.th_melee();
      return TRUE;
        }
// missile attack
   self.lefty = 1;
//   self.th_missile();
//   SUB_AttackFinished (1.5*random());
   return FALSE;
   };


/*QUAKED monster_armagon (1 0 0) (-32 -32 -24) (32 32 64) Ambush
*/
void() monster_armagon
{
   local entity body;
   local float sk;
   if(deathmatch)
    {
        remove(self);
        return;
    }

   self.lefty = 0;

   sk = cvar("skill");

   precache_model("progs/armabody.mdl");
   precache_model("progs/armalegs.mdl");

   precache_sound("armagon/footfall.wav");
   precache_sound("armagon/servo.wav");
//   precache_sound("armagon/footfall2.wav");
   precache_sound("armagon/death.wav");
//   precache_sound("armagon/death2.wav");
   precache_sound("armagon/pain.wav");
   precache_sound("armagon/repel.wav");
//   precache_sound("armagon/idle.wav");
   precache_sound("armagon/sight.wav");
   precache_sound("armagon/idle1.wav");
   precache_sound("armagon/idle2.wav");
   precache_sound("armagon/idle3.wav");
   precache_sound("armagon/idle4.wav");

   body = spawn();
   body.origin = self.origin;
   body.origin = body.origin - '0 0 64';
//  SUB_CopyEntity(self,body);

   self.solid = SOLID_SLIDEBOX;
   self.fixangle = 0;
    self.movetype = MOVETYPE_STEP;
   body.solid = SOLID_NOT;
   body.movetype = MOVETYPE_STEP;
   self.trigger_field = body;
   body.trigger_field = self;

   setmodel(body, "progs/armabody.mdl");
   setmodel(self, "progs/armalegs.mdl");

   setsize(self, '-48 -48 -24', '48 48 84' );
   setorigin(self, self.origin);
   setorigin(body, body.origin);
   if(sk==0)
      {
      self.yaw_speed = 5;
      self.health = 2000;
      self.worldtype = 0.9;
      }
   else if(sk==1)
      {
      self.yaw_speed = 9;
      self.health = 2500;
      self.worldtype = 0.85;
      }
   else
      {
      self.yaw_speed = 12;
      self.health = 3500;
      self.worldtype = 0.75;
      }
   self.state = 0;
   self.super_time = 0;
   self.endtime = 0;

   self.th_stand = armagon_stand1;
   self.th_walk = armagon_walk1;
   self.th_run = armagon_run1;
   self.th_die = armagon_die1;
   self.th_missile = armagon_missile_attack;
   self.th_melee = armagon_stop1;
   self.th_pain = armagon_pain;
   walkmonster_start();
};
