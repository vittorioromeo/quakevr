void (float AttackNumber) lavaman_missile;

void () lavaman_hunt = {
   local entity possibleEnemy;
   local vector viewPoint;

   if ( (self.enemy.health <= FALSE) ) {
      viewPoint = (self.origin + '0.000 0.000 96.000');
      possibleEnemy = find (self.enemy,classname,"player");
      traceline (viewPoint,possibleEnemy.origin,TRUE,world);
      if ( (trace_fraction == TRUE) )
         self.enemy = possibleEnemy;
   }
   if ( self.enemy ) {
      ai_face ();
      self.movetarget = self.enemy;
      self.goalentity = self.enemy;
   }
};

void () lavaman_rise1 = [ 0.000, lavaman_rise2 ] { sound (self,CHAN_WEAPON,"boss1/out1.wav",TRUE,ATTN_NORM); };
void () lavaman_rise2 = [ 1.000, lavaman_rise3 ] { sound (self,CHAN_VOICE,"boss1/sight1.wav",TRUE,ATTN_NORM); };
void () lavaman_rise3 = [ 2.000, lavaman_rise4 ] {};
void () lavaman_rise4 = [ 3.000, lavaman_rise5 ] {};
void () lavaman_rise5 = [ 4.000, lavaman_rise6 ] {};
void () lavaman_rise6 = [ 5.000, lavaman_rise7 ] {};
void () lavaman_rise7 = [ 6.000, lavaman_rise8 ] {};
void () lavaman_rise8 = [ 7.000, lavaman_rise9 ] {};
void () lavaman_rise9 = [ 8.000, lavaman_rise10 ] {};
void () lavaman_rise10 = [ 9.000, lavaman_rise11 ] {};
void () lavaman_rise11 = [ 10.000, lavaman_rise12 ] {};
void () lavaman_rise12 = [ 11.000, lavaman_rise13 ] {};
void () lavaman_rise13 = [ 12.000, lavaman_rise14 ] {};
void () lavaman_rise14 = [ 13.000, lavaman_rise15 ] {};
void () lavaman_rise15 = [ 14.000, lavaman_rise16 ] {};
void () lavaman_rise16 = [ 15.000, lavaman_rise17 ] {};
void () lavaman_rise17 = [ 16.000, lavaman_fire1 ] {};

void () lavaman_fire1;

void () lavaman_stand = {
   //if ( self.enemy ) {
   //   LavaManCheckAttack ();
   //} else {
   //   lavaman_hunt ();
   //}
   ai_stand ();
};

void () lavaman_idle1 = [ 17.000, lavaman_idle2 ] {
   if ( (random () < 0.200) )
      sound (self,CHAN_VOICE,"boss1/sight1.wav",TRUE,ATTN_NORM);
};

void () lavaman_idle2 = [ 18.000, lavaman_idle3 ] { lavaman_stand (); };
void () lavaman_idle3 = [ 19.000, lavaman_idle4 ] { lavaman_stand (); };
void () lavaman_idle4 = [ 20.000, lavaman_idle5 ] { lavaman_stand (); };
void () lavaman_idle5 = [ 21.000, lavaman_idle6 ] { lavaman_stand (); };
void () lavaman_idle6 = [ 20.000, lavaman_idle7 ] { lavaman_stand (); };
void () lavaman_idle7 = [ 19.000, lavaman_idle8 ] { lavaman_stand (); };
void () lavaman_idle8 = [ 18.000, lavaman_idle9 ] { lavaman_stand (); };
void () lavaman_idle9 = [ 17.000, lavaman_idle1 ] { lavaman_stand (); };

void () lavaman_walk = {
   //if ( self.enemy ) {
   //   LavaManCheckAttack ();
   //} else {
   //   lavaman_hunt ();
   //}
   ai_walk (FL_SWIM);
};

void () lavaman_walk1 = [ 17.000, lavaman_walk2 ] { lavaman_walk (); };
void () lavaman_walk2 = [ 18.000, lavaman_walk3 ] { lavaman_walk (); };
void () lavaman_walk3 = [ 19.000, lavaman_walk4 ] { lavaman_walk (); };
void () lavaman_walk4 = [ 20.000, lavaman_walk5 ] { lavaman_walk (); };
void () lavaman_walk5 = [ 21.000, lavaman_walk6 ] { lavaman_walk (); };
void () lavaman_walk6 = [ 22.000, lavaman_walk7 ] { lavaman_walk (); };
void () lavaman_walk7 = [ 23.000, lavaman_walk8 ] { lavaman_walk (); };
void () lavaman_walk8 = [ 24.000, lavaman_walk9 ] { lavaman_walk (); };
void () lavaman_walk9 = [ 25.000, lavaman_walk10 ] { lavaman_walk (); };
void () lavaman_walk10 = [ 26.000, lavaman_walk11 ] { lavaman_walk (); };
void () lavaman_walk11 = [ 27.000, lavaman_walk12 ] { lavaman_walk (); };
void () lavaman_walk12 = [ 28.000, lavaman_walk13 ] { lavaman_walk (); };
void () lavaman_walk13 = [ 29.000, lavaman_walk14 ] { lavaman_walk (); };
void () lavaman_walk14 = [ 30.000, lavaman_walk15 ] { lavaman_walk (); };
void () lavaman_walk15 = [ 31.000, lavaman_walk16 ] { lavaman_walk (); };
void () lavaman_walk16 = [ 32.000, lavaman_walk17 ] { lavaman_walk (); };
void () lavaman_walk17 = [ 33.000, lavaman_walk18 ] { lavaman_walk (); };
void () lavaman_walk18 = [ 34.000, lavaman_walk19 ] { lavaman_walk (); };
void () lavaman_walk19 = [ 35.000, lavaman_walk20 ] { lavaman_walk (); };
void () lavaman_walk20 = [ 36.000, lavaman_walk21 ] { lavaman_walk (); };
void () lavaman_walk21 = [ 37.000, lavaman_walk22 ] { lavaman_walk (); };
void () lavaman_walk22 = [ 38.000, lavaman_walk23 ] { lavaman_walk (); };
void () lavaman_walk23 = [ 39.000, lavaman_walk24 ] { lavaman_walk (); };
void () lavaman_walk24 = [ 40.000, lavaman_walk25 ] { lavaman_walk (); };
void () lavaman_walk25 = [ 41.000, lavaman_walk26 ] { lavaman_walk (); };
void () lavaman_walk26 = [ 42.000, lavaman_walk27 ] { lavaman_walk (); };
void () lavaman_walk27 = [ 43.000, lavaman_walk28 ] { lavaman_walk (); };
void () lavaman_walk28 = [ 44.000, lavaman_walk29 ] { lavaman_walk (); };
void () lavaman_walk29 = [ 45.000, lavaman_walk30 ] { lavaman_walk (); };
void () lavaman_walk30 = [ 46.000, lavaman_walk31 ] { lavaman_walk (); };
void () lavaman_walk31 = [ 47.000, lavaman_walk1 ] {};

void () lavaman_run = {
   //if ( self.enemy ) {
   //   LavaManCheckAttack ();
   //} else {
   //   lavaman_hunt ();
   //}
   ai_run (FL_SWIM);
};

void () lavaman_run1 = [ 17.000, lavaman_run2 ] { lavaman_run (); };
void () lavaman_run2 = [ 18.000, lavaman_run3 ] { lavaman_run (); };
void () lavaman_run3 = [ 19.000, lavaman_run4 ] { lavaman_run (); };
void () lavaman_run4 = [ 20.000, lavaman_run5 ] { lavaman_run (); };
void () lavaman_run5 = [ 21.000, lavaman_run6 ] { lavaman_run (); };
void () lavaman_run6 = [ 22.000, lavaman_run7 ] { lavaman_run (); };
void () lavaman_run7 = [ 23.000, lavaman_run8 ] { lavaman_run (); };
void () lavaman_run8 = [ 24.000, lavaman_run9 ] { lavaman_run (); };
void () lavaman_run9 = [ 25.000, lavaman_run10 ] { lavaman_run (); };
void () lavaman_run10 = [ 26.000, lavaman_run11 ] { lavaman_run (); };
void () lavaman_run11 = [ 27.000, lavaman_run12 ] { lavaman_run (); };
void () lavaman_run12 = [ 28.000, lavaman_run13 ] { lavaman_run (); };
void () lavaman_run13 = [ 29.000, lavaman_run14 ] { lavaman_run (); };
void () lavaman_run14 = [ 30.000, lavaman_run15 ] { lavaman_run (); };
void () lavaman_run15 = [ 31.000, lavaman_run16 ] { lavaman_run (); };
void () lavaman_run16 = [ 32.000, lavaman_run17 ] { lavaman_run (); };
void () lavaman_run17 = [ 33.000, lavaman_run18 ] { lavaman_run (); };
void () lavaman_run18 = [ 34.000, lavaman_run19 ] { lavaman_run (); };
void () lavaman_run19 = [ 35.000, lavaman_run20 ] { lavaman_run (); };
void () lavaman_run20 = [ 36.000, lavaman_run21 ] { lavaman_run (); };
void () lavaman_run21 = [ 37.000, lavaman_run22 ] { lavaman_run (); };
void () lavaman_run22 = [ 38.000, lavaman_run23 ] { lavaman_run (); };
void () lavaman_run23 = [ 39.000, lavaman_run24 ] { lavaman_run (); };
void () lavaman_run24 = [ 40.000, lavaman_run25 ] { lavaman_run (); };
void () lavaman_run25 = [ 41.000, lavaman_run26 ] { lavaman_run (); };
void () lavaman_run26 = [ 42.000, lavaman_run27 ] { lavaman_run (); };
void () lavaman_run27 = [ 43.000, lavaman_run28 ] { lavaman_run (); };
void () lavaman_run28 = [ 44.000, lavaman_run29 ] { lavaman_run (); };
void () lavaman_run29 = [ 45.000, lavaman_run30 ] { lavaman_run (); };
void () lavaman_run30 = [ 46.000, lavaman_run31 ] { lavaman_run (); };
void () lavaman_run31 = [ 47.000, lavaman_run1 ] {};

void () lavaman_fire1 = [ 57.000, lavaman_fire2 ] { ai_face (); };
void () lavaman_fire2 = [ 58.000, lavaman_fire3 ] { ai_charge (TRUE); };
void () lavaman_fire3 = [ 59.000, lavaman_fire4 ] { ai_face (); };
void () lavaman_fire4 = [ 60.000, lavaman_fire5 ] { ai_face (); };
void () lavaman_fire5 = [ 61.000, lavaman_fire6 ] { ai_charge (TRUE); };
void () lavaman_fire6 = [ 62.000, lavaman_fire7 ] { ai_face (); };
void () lavaman_fire7 = [ 63.000, lavaman_fire8 ] { lavaman_missile (TRUE); };
void () lavaman_fire8 = [ 64.000, lavaman_fire9 ] { ai_face (); };
void () lavaman_fire9 = [ 65.000, lavaman_fire10 ] { ai_face (); };
void () lavaman_fire10 = [ 66.000, lavaman_fire11 ] { ai_charge (TRUE); };
void () lavaman_fire11 = [ 67.000, lavaman_fire12 ] { ai_face (); };
void () lavaman_fire12 = [ 68.000, lavaman_fire13 ] { ai_charge (TRUE); };
void () lavaman_fire13 = [ 69.000, lavaman_fire14 ] { ai_face (); };
void () lavaman_fire14 = [ 70.000, lavaman_fire15 ] { ai_face (); };
void () lavaman_fire15 = [ 71.000, lavaman_fire16 ] { ai_charge (TRUE); };
void () lavaman_fire16 = [ 72.000, lavaman_fire17 ] { ai_charge (TRUE); };
void () lavaman_fire17 = [ 73.000, lavaman_fire18 ] { ai_face (); };
void () lavaman_fire18 = [ 74.000, lavaman_fire19 ] { lavaman_missile (FL_SWIM); };
void () lavaman_fire19 = [ 75.000, lavaman_fire20 ] { ai_face (); };
void () lavaman_fire20 = [ 76.000, lavaman_fire21 ] { ai_face (); };
void () lavaman_fire21 = [ 77.000, lavaman_fire22 ] { ai_face (); };
void () lavaman_fire22 = [ 78.000, lavaman_fire23 ] { ai_charge (TRUE); };
void () lavaman_fire23 = [ 79.000, lavaman_run1 ] {};

void () lavaman_shocka1 = [ 80.000, lavaman_shocka2 ] {};
void () lavaman_shocka2 = [ 81.000, lavaman_shocka3 ] {};
void () lavaman_shocka3 = [ 82.000, lavaman_shocka4 ] {};
void () lavaman_shocka4 = [ 83.000, lavaman_shocka5 ] {};
void () lavaman_shocka5 = [ 84.000, lavaman_shocka6 ] {};
void () lavaman_shocka6 = [ 85.000, lavaman_shocka7 ] {};
void () lavaman_shocka7 = [ 86.000, lavaman_shocka8 ] {};
void () lavaman_shocka8 = [ 87.000, lavaman_shocka9 ] {};
void () lavaman_shocka9 = [ 88.000, lavaman_shocka10 ] {};
void () lavaman_shocka10 = [ 89.000, lavaman_run1 ] {};

void (entity inflictor, entity attacker, float damage) lavaman_pain = {
   if ( (self.pain_finished > time) )
      return ;

   if ( (random () < 0.050) ) {
      self.pain_finished = (time + FL_SWIM);
      lavaman_shocka1 ();
   }
};

void () lavaman_death1 = [ 48.000, lavaman_death2 ] { sound (self,CHAN_VOICE,"boss1/death.wav",TRUE,ATTN_NORM); };
void () lavaman_death2 = [ 49.000, lavaman_death3 ] {};
void () lavaman_death3 = [ 50.000, lavaman_death4 ] {};
void () lavaman_death4 = [ 51.000, lavaman_death5 ] {};
void () lavaman_death5 = [ 52.000, lavaman_death6 ] {};
void () lavaman_death6 = [ 53.000, lavaman_death7 ] { self.solid = SOLID_NOT; };
void () lavaman_death7 = [ 54.000, lavaman_death8 ] {};
void () lavaman_death8 = [ 55.000, lavaman_death9 ] {};
void () lavaman_death9 = [ 56.000, lavaman_death10 ] {
   sound (self,CHAN_BODY,"boss1/out1.wav",TRUE,ATTN_NORM);
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_LAVASPLASH);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
};
void () lavaman_death10 = [ 56.000, lavaman_death10 ] {
   killed_monsters = killed_monsters + 1;
   WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
   SUB_UseTargets ();
   remove (self);
};

void () lavaman_touch = {
   local float damg;

   if ( (other == self.owner) )
      return ;

   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }
   damg = 40;
   if ( other.health ) {
      if ( (other.classname == "monster_shambler") )
         damg = (damg * 0.500);

      T_Damage (other,self,self.owner,damg, DAMARMOR);
   }
   T_RadiusDamage (self,self.owner,40,world,DAMAGEALL);
   self.origin = (self.origin - (FL_CLIENT * normalize (self.velocity)));
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   BecomeExplosion ();
};

void (float AttackNumber) lavaman_missile = {
   local entity fireball;
   local vector org;
   local vector vec;
   local vector d;
   local float t;

   makevectors (self.angles);
   if ( (AttackNumber == TRUE) ) {
      org = (((self.origin + (v_forward * 40)) + (v_right * 65.000)) + (v_up * 130.000));
   } else {
      if ( (AttackNumber == FL_SWIM) )
         org = (((self.origin + (v_forward * 40)) - (v_right * 75.000)) + (v_up * 125.000));
   }
   d = self.enemy.origin;
   t = vlen ((d - org));
   t = (t / 380.000);
   if ( (t > 1.750) )
      t = 1.750;

   if ( (t < TRUE) )
      t = TRUE;

   vec = normalize ((d - org));
   fireball = spawn ();
   fireball.owner = self;
   fireball.movetype = MOVETYPE_BOUNCE;
   fireball.solid = SOLID_BBOX;
   fireball.angles = vectoangles (vec);
   fireball.touch = lavaman_touch;
   fireball.classname = "lavaman_ball";
   fireball.skin = 0;
   setmodel (fireball,MODEL_PROJ_LAVA);
   setsize (fireball,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (fireball,org);
   fireball.velocity = ((vec * 600.000) * t);
   fireball.velocity_z = (fireball.velocity_z + (200.000 * t));
   fireball.avelocity = '200.000 100.000 300.000';
   fireball.think = SUB_Remove;
   fireball.nextthink = (time + MOVETYPE_TOSS);
   sound (self,CHAN_WEAPON,"boss1/throw.wav",TRUE,ATTN_NORM);
   if ( (self.enemy.health <= FALSE) )
      lavaman_idle1 ();
};

void () lavaman_awake = {
   self.takedamage = DAMAGE_AIM;
   self.solid = SOLID_SLIDEBOX;
   self.movetype = MOVETYPE_STEP;
   self.ideal_yaw = (self.angles * '0.000 1.000 0.000');
   //setsize (self,'-24 -24 -24','24 24 64');
   self.view_ofs = '0.000 0.000 48.000';
   self.th_stand = lavaman_idle1;
   self.th_walk = lavaman_walk1;
   self.th_run = lavaman_run1;
   self.th_missile = lavaman_fire1;
   self.th_pain = lavaman_pain;
   self.th_die = lavaman_death1;
   self.th_melee = lavaman_fire1;
   self.attack_offset = (self.origin + '0.000 0.000 96.000');
   self.th_checkattack = LavaManCheckAttack;
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_LAVASPLASH);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   lavaman_rise1 ();
};

void () monster_lava_man = {
   if ( deathmatch ) { remove(self); return; }

   self.mdl = "progs/rogue/mon_lavaman.mdl";
   precache_model (self.mdl);
   precache_model (MODEL_PROJ_LAVA);
   precache_sound ("boss1/out1.wav");
   precache_sound ("boss1/sight1.wav");
   precache_sound ("boss1/throw.wav");
   precache_sound ("boss1/pain.wav");
   precache_sound ("boss1/death.wav");

   self.sight_sound = "boss1/sight1.wav";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   if (self.bboxtype < 1) self.bboxtype = BBOX_GIANT;
   if (self.health < 1) self.health = 1500;
   if (self.yaw_speed < 1) self.yaw_speed = 20.000;  // Lumbering turn
   self.gibhealth = MON_NEVERGIB;
   self.gibbed = FALSE;
   self.pain_flinch = 80;
   self.blockudeath = TRUE;

   // Always reset Ammo Resistance to be consistent
   self.resist_shells = self.resist_nails = 0;
   self.resist_rockets = self.resist_cells = 0;

   self.th_stand = lavaman_awake;
   self.th_walk = lavaman_awake;
   self.th_run = lavaman_awake;

   self.classtype = CT_MONLAVAMAN;
   self.classgroup = CG_DEMON;
   self.classmove = MON_MOVEWALK;
   droptofloor();
   self.origin = self.origin + '0 0 32';
    if (self.deathstring == "")  self.deathstring = " fries in Hephaestus' fury\n";
   monster_start ();
};
