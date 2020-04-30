void () mummy_stand1 = [ 0.000, mummy_stand2 ] { ai_stand (); };
void () mummy_stand2 = [ 1.000, mummy_stand3 ] { ai_stand (); };
void () mummy_stand3 = [ 2.000, mummy_stand4 ] { ai_stand (); };
void () mummy_stand4 = [ 3.000, mummy_stand5 ] { ai_stand (); };
void () mummy_stand5 = [ 4.000, mummy_stand6 ] { ai_stand (); };
void () mummy_stand6 = [ 5.000, mummy_stand7 ] { ai_stand (); };
void () mummy_stand7 = [ 6.000, mummy_stand8 ] { ai_stand (); };
void () mummy_stand8 = [ 7.000, mummy_stand9 ] { ai_stand (); };
void () mummy_stand9 = [ 8.000, mummy_stand10 ] { ai_stand (); };
void () mummy_stand10 = [ 9.000, mummy_stand11 ] { ai_stand (); };
void () mummy_stand11 = [ 10.000, mummy_stand12 ] { ai_stand (); };
void () mummy_stand12 = [ 11.000, mummy_stand13 ] { ai_stand (); };
void () mummy_stand13 = [ 12.000, mummy_stand14 ] { ai_stand (); };
void () mummy_stand14 = [ 13.000, mummy_stand15 ] { ai_stand (); };
void () mummy_stand15 = [ 14.000, mummy_stand1 ] { ai_stand (); };

void () mummy_walk1 = [ 15.000, mummy_walk2 ] { ai_walk (0); };
void () mummy_walk2 = [ 16.000, mummy_walk3 ] { ai_walk (3); };
void () mummy_walk3 = [ 17.000, mummy_walk4 ] { ai_walk (5); };
void () mummy_walk4 = [ 18.000, mummy_walk5 ] { ai_walk (4); };
void () mummy_walk5 = [ 19.000, mummy_walk6 ] { ai_walk (2); };
void () mummy_walk6 = [ 20.000, mummy_walk7 ] { ai_walk (0); };
void () mummy_walk7 = [ 21.000, mummy_walk8 ] { ai_walk (0); };
void () mummy_walk8 = [ 22.000, mummy_walk9 ] { ai_walk (0); };
void () mummy_walk9 = [ 23.000, mummy_walk10 ] { ai_walk (0); };
void () mummy_walk10 = [ 24.000, mummy_walk11 ] { ai_walk (0); };
void () mummy_walk11 = [ 25.000, mummy_walk12 ] { ai_walk (3); };
void () mummy_walk12 = [ 26.000, mummy_walk13 ] { ai_walk (4); };
void () mummy_walk13 = [ 27.000, mummy_walk14 ] { ai_walk (2); };
void () mummy_walk14 = [ 28.000, mummy_walk15 ] { ai_walk (0); };
void () mummy_walk15 = [ 29.000, mummy_walk16 ] { ai_walk (0); };
void () mummy_walk16 = [ 30.000, mummy_walk17 ] { ai_walk (0); };
void () mummy_walk17 = [ 31.000, mummy_walk18 ] { ai_walk (0); };
void () mummy_walk18 = [ 32.000, mummy_walk19 ] { ai_walk (0); };
void () mummy_walk19 = [ 33.000, mummy_walk1 ] {
   ai_walk (0);
   if ( (random () < 0.200) )
      sound (self,CHAN_VOICE,"zombie/z_idle.wav",TRUE,ATTN_IDLE);
};

void () mummy_run1 = [ 34.000, mummy_run2 ] {
   ai_run (2);
   self.inpain = FALSE;
};
void () mummy_run2 = [ 35.000, mummy_run3 ] { ai_run (2); };
void () mummy_run3 = [ 36.000, mummy_run4 ] { ai_run (0); };
void () mummy_run4 = [ 37.000, mummy_run5 ] { ai_run (2); };
void () mummy_run5 = [ 38.000, mummy_run6 ] { ai_run (4); };
void () mummy_run6 = [ 39.000, mummy_run7 ] { ai_run (6); };
void () mummy_run7 = [ 40.000, mummy_run8 ] { ai_run (8); };
void () mummy_run8 = [ 41.000, mummy_run9 ] { ai_run (8); };
void () mummy_run9 = [ 42.000, mummy_run10 ] { ai_run (4); };
void () mummy_run10 = [ 43.000, mummy_run11 ] { ai_run (0); };
void () mummy_run11 = [ 44.000, mummy_run12 ] { ai_run (0); };
void () mummy_run12 = [ 45.000, mummy_run13 ] { ai_run (0); };
void () mummy_run13 = [ 46.000, mummy_run14 ] { ai_run (4); };
void () mummy_run14 = [ 47.000, mummy_run15 ] { ai_run (8); };
void () mummy_run15 = [ 48.000, mummy_run16 ] { ai_run (12); };
void () mummy_run16 = [ 49.000, mummy_run17 ] { ai_run (14); };
void () mummy_run17 = [ 50.000, mummy_run18 ] { ai_run (6); };
void () mummy_run18 = [ 51.000, mummy_run1 ] {
   ai_run (16);
   if ( (random () < 0.200) )
      sound (self,CHAN_VOICE,"zombie/z_idle.wav",TRUE,ATTN_IDLE);

   if ( (random () > 0.800) )
      sound (self,CHAN_VOICE,"zombie/z_idle1.wav",TRUE,ATTN_IDLE);
};

void () mummyGrenadeTouch = {
   local float mummyDamage;

   if ( (other == self.owner) )
      return;

   if ( other.takedamage ) {
      mummyDamage = (15 + (random () * 15));
      T_Damage (other,self,self.owner,mummyDamage,DAMARMOR);
      sound (self,CHAN_WEAPON,"zombie/z_hit.wav",TRUE,ATTN_NORM);
      remove (self);
      return;
   }
   sound (self,CHAN_WEAPON,"zombie/z_miss.wav",TRUE,ATTN_NORM);
   self.velocity = VEC_ORIGIN;
   self.avelocity = VEC_ORIGIN;
   self.touch = SUB_Remove;
};

void (vector st) mummyFireGrenade = {
   local entity missile;
   local vector org;

   sound (self,CHAN_WEAPON,"zombie/z_shot1.wav",TRUE,ATTN_NORM);
   missile = spawn ();
   missile.owner = self;
   missile.movetype = MOVETYPE_BOUNCE;
   missile.solid = SOLID_BBOX;
   org = (((self.origin + (st_x * v_forward)) + (st_y * v_right)) + ((st_z - 24.000) * v_up));
   makevectors (self.angles);
   missile.velocity = normalize ((self.enemy.origin - org));
   missile.velocity = (missile.velocity * 600.000);
   missile.velocity_z = 200.000;
   missile.avelocity = '3000.000 1000.000 2000.000';
   missile.touch = mummyGrenadeTouch;
   missile.nextthink = (time + 2.500);
   missile.think = SUB_Remove;
   setmodel (missile,"progs/zom_gib.mdl");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,org);
};

void () mummy_atta1 = [ 52.000, mummy_atta2 ] { ai_face (); };
void () mummy_atta2 = [ 53.000, mummy_atta3 ] { ai_face (); };
void () mummy_atta3 = [ 54.000, mummy_atta4 ] { ai_face (); };
void () mummy_atta4 = [ 55.000, mummy_atta5 ] { ai_face (); };
void () mummy_atta5 = [ 56.000, mummy_atta6 ] { ai_face (); };
void () mummy_atta6 = [ 57.000, mummy_atta7 ] { ai_face (); };
void () mummy_atta7 = [ 58.000, mummy_atta8 ] { ai_face (); };
void () mummy_atta8 = [ 59.000, mummy_atta9 ] { ai_face (); };
void () mummy_atta9 = [ 60.000, mummy_atta10 ] { ai_face (); };
void () mummy_atta10 = [ 61.000, mummy_atta11 ] { ai_face (); };
void () mummy_atta11 = [ 62.000, mummy_atta12 ] { ai_face (); };
void () mummy_atta12 = [ 63.000, mummy_atta13 ] { ai_face (); };
void () mummy_atta13 = [ 64.000, mummy_run1 ] {
   ai_face ();
   mummyFireGrenade ('-10.000 -22.000 30.000');
};

void () mummy_attb1 = [ 65.000, mummy_attb2 ] { ai_face (); };
void () mummy_attb2 = [ 66.000, mummy_attb3 ] { ai_face (); };
void () mummy_attb3 = [ 67.000, mummy_attb4 ] { ai_face (); };
void () mummy_attb4 = [ 68.000, mummy_attb5 ] { ai_face (); };
void () mummy_attb5 = [ 69.000, mummy_attb6 ] { ai_face (); };
void () mummy_attb6 = [ 70.000, mummy_attb7 ] { ai_face (); };
void () mummy_attb7 = [ 71.000, mummy_attb8 ] { ai_face (); };
void () mummy_attb8 = [ 72.000, mummy_attb9 ] { ai_face (); };
void () mummy_attb9 = [ 73.000, mummy_attb10 ] { ai_face (); };
void () mummy_attb10 = [ 74.000, mummy_attb11 ] { ai_face (); };
void () mummy_attb11 = [ 75.000, mummy_attb12 ] { ai_face (); };
void () mummy_attb12 = [ 76.000, mummy_attb13 ] { ai_face (); };
void () mummy_attb13 = [ 77.000, mummy_attb14 ] { ai_face (); };
void () mummy_attb14 = [ 77.000, mummy_run1 ] {
   ai_face ();
   mummyFireGrenade ('-10.000 -24.000 29.000');
};

void () mummy_attc1 = [ 79.000, mummy_attc2 ] { ai_face (); };
void () mummy_attc2 = [ 80.000, mummy_attc3 ] { ai_face (); };
void () mummy_attc3 = [ 81.000, mummy_attc4 ] { ai_face (); };
void () mummy_attc4 = [ 82.000, mummy_attc5 ] { ai_face (); };
void () mummy_attc5 = [ 83.000, mummy_attc6 ] { ai_face (); };
void () mummy_attc6 = [ 84.000, mummy_attc7 ] { ai_face (); };
void () mummy_attc7 = [ 85.000, mummy_attc8 ] { ai_face (); };
void () mummy_attc8 = [ 86.000, mummy_attc9 ] { ai_face (); };
void () mummy_attc9 = [ 87.000, mummy_attc10 ] { ai_face (); };
void () mummy_attc10 = [ 88.000, mummy_attc11 ] { ai_face (); };
void () mummy_attc11 = [ 89.000, mummy_attc12 ] { ai_face (); };
void () mummy_attc12 = [ 90.000, mummy_run1 ] {
   ai_face ();
   mummyFireGrenade ('-12.000 -19.000 29.000');
};

void () mummy_missile = {
   local float r;

   r = random ();
   if ( (r < 0.300) ) {
      mummy_atta1 ();
   } else {
      if ( (r < 0.600) ) {
         mummy_attb1 ();
      } else {
         mummy_attc1 ();
      }
   }
};

void () mummy_paina1 = [ 91.000, mummy_paina2 ] { sound (self,CHAN_VOICE,"zombie/z_pain.wav",TRUE,ATTN_NORM); };
void () mummy_paina2 = [ 92.000, mummy_paina3 ] { ai_painforward (3); };
void () mummy_paina3 = [ 93.000, mummy_paina4 ] { ai_painforward (1); };
void () mummy_paina4 = [ 94.000, mummy_paina5 ] { ai_pain (1); };
void () mummy_paina5 = [ 95.000, mummy_paina6 ] { ai_pain (3); };
void () mummy_paina6 = [ 96.000, mummy_paina7 ] { ai_pain (1); };
void () mummy_paina7 = [ 97.000, mummy_paina8 ] {};
void () mummy_paina8 = [ 98.000, mummy_paina9 ] {};
void () mummy_paina9 = [ 99.000, mummy_paina10 ] {};
void () mummy_paina10 = [ 100.000, mummy_paina11 ] {};
void () mummy_paina11 = [ 101.000, mummy_paina12 ] {};
void () mummy_paina12 = [ 102.000, mummy_run1 ] {};

void () mummy_painb1 = [ 103.000, mummy_painb2 ] { sound (self,CHAN_VOICE,"zombie/z_pain1.wav",TRUE,ATTN_NORM); };
void () mummy_painb2 = [ 104.000, mummy_painb3 ] { ai_pain (2); };
void () mummy_painb3 = [ 105.000, mummy_painb4 ] { ai_pain (8); };
void () mummy_painb4 = [ 106.000, mummy_painb5 ] { ai_pain (6); };
void () mummy_painb5 = [ 107.000, mummy_painb6 ] { ai_pain (2); };
void () mummy_painb6 = [ 108.000, mummy_painb7 ] {};
void () mummy_painb7 = [ 109.000, mummy_painb8 ] {};
void () mummy_painb8 = [ 110.000, mummy_painb9 ] {};
void () mummy_painb9 = [ 111.000, mummy_painb10 ] { sound (self,CHAN_BODY,"zombie/z_fall.wav",TRUE,ATTN_NORM); };
void () mummy_painb10 = [ 112.000, mummy_painb11 ] {};
void () mummy_painb11 = [ 113.000, mummy_painb12 ] {};
void () mummy_painb12 = [ 114.000, mummy_painb13 ] {};
void () mummy_painb13 = [ 115.000, mummy_painb14 ] {};
void () mummy_painb14 = [ 116.000, mummy_painb15 ] {};
void () mummy_painb15 = [ 117.000, mummy_painb16 ] {};
void () mummy_painb16 = [ 118.000, mummy_painb17 ] {};
void () mummy_painb17 = [ 119.000, mummy_painb18 ] {};
void () mummy_painb18 = [ 120.000, mummy_painb19 ] {};
void () mummy_painb19 = [ 121.000, mummy_painb20 ] {};
void () mummy_painb20 = [ 122.000, mummy_painb21 ] {};
void () mummy_painb21 = [ 123.000, mummy_painb22 ] {};
void () mummy_painb22 = [ 124.000, mummy_painb23 ] {};
void () mummy_painb23 = [ 125.000, mummy_painb24 ] {};
void () mummy_painb24 = [ 126.000, mummy_painb25 ] {};
void () mummy_painb25 = [ 127.000, mummy_painb26 ] { ai_painforward (1); };
void () mummy_painb26 = [ 128.000, mummy_painb27 ] {};
void () mummy_painb27 = [ 129.000, mummy_painb28 ] {};
void () mummy_painb28 = [ 130.000, mummy_run1 ] {};

void () mummy_painc1 = [ 131.000, mummy_painc2 ] { sound (self,CHAN_VOICE,"zombie/z_pain1.wav",TRUE,ATTN_NORM); };
void () mummy_painc2 = [ 132.000, mummy_painc3 ] {};
void () mummy_painc3 = [ 133.000, mummy_painc4 ] { ai_pain (3); };
void () mummy_painc4 = [ 134.000, mummy_painc5 ] { ai_pain (1); };
void () mummy_painc5 = [ 135.000, mummy_painc6 ] {};
void () mummy_painc6 = [ 136.000, mummy_painc7 ] {};
void () mummy_painc7 = [ 137.000, mummy_painc8 ] {};
void () mummy_painc8 = [ 138.000, mummy_painc9 ] {};
void () mummy_painc9 = [ 139.000, mummy_painc10 ] {};
void () mummy_painc10 = [ 140.000, mummy_painc11 ] {};
void () mummy_painc11 = [ 141.000, mummy_painc12 ] { ai_painforward (1); };
void () mummy_painc12 = [ 142.000, mummy_painc13 ] { ai_painforward (1); };
void () mummy_painc13 = [ 143.000, mummy_painc14 ] {};
void () mummy_painc14 = [ 144.000, mummy_painc15 ] {};
void () mummy_painc15 = [ 145.000, mummy_painc16 ] {};
void () mummy_painc16 = [ 146.000, mummy_painc17 ] {};
void () mummy_painc17 = [ 147.000, mummy_painc18 ] {};
void () mummy_painc18 = [ 148.000, mummy_run1 ] {};

void () mummy_paind1 = [ 149.000, mummy_paind2 ] { sound (self,CHAN_VOICE,"zombie/z_pain.wav",TRUE,ATTN_NORM); };
void () mummy_paind2 = [ 150.000, mummy_paind3 ] {};
void () mummy_paind3 = [ 151.000, mummy_paind4 ] {};
void () mummy_paind4 = [ 152.000, mummy_paind5 ] {};
void () mummy_paind5 = [ 153.000, mummy_paind6 ] {};
void () mummy_paind6 = [ 154.000, mummy_paind7 ] {};
void () mummy_paind7 = [ 155.000, mummy_paind8 ] {};
void () mummy_paind8 = [ 156.000, mummy_paind9 ] {};
void () mummy_paind9 = [ 157.000, mummy_paind10 ] { ai_pain (1); };
void () mummy_paind10 = [ 158.000, mummy_paind11 ] {};
void () mummy_paind11 = [ 159.000, mummy_paind12 ] {};
void () mummy_paind12 = [ 160.000, mummy_paind13 ] {};
void () mummy_paind13 = [ 161.000, mummy_run1 ] {};

void () mummy_paine1 = [ 162.000, mummy_paine2 ] { sound (self,CHAN_VOICE,"zombie/z_pain.wav",TRUE,ATTN_NORM); };
void () mummy_paine2 = [ 163.000, mummy_paine3 ] { ai_pain (8); };
void () mummy_paine3 = [ 164.000, mummy_paine4 ] { ai_pain (5); };
void () mummy_paine4 = [ 165.000, mummy_paine5 ] { ai_pain (3); };
void () mummy_paine5 = [ 166.000, mummy_paine6 ] { ai_pain (1); };
void () mummy_paine6 = [ 167.000, mummy_paine7 ] { ai_pain (2); };
void () mummy_paine7 = [ 168.000, mummy_paine8 ] { ai_pain (1); };
void () mummy_paine8 = [ 169.000, mummy_paine9 ] { ai_pain (1); };
void () mummy_paine9 = [ 170.000, mummy_paine10 ] { ai_pain (2); };
void () mummy_paine10 = [ 171.000, mummy_paine11 ] {
   sound (self,CHAN_BODY,"zombie/z_fall.wav",TRUE,ATTN_NORM);
   self.solid = SOLID_NOT;
};
void () mummy_paine11 = [ 172.000, mummy_paine12 ] { self.nextthink = (self.nextthink + 5); };
void () mummy_paine12 = [ 173.000, mummy_paine13 ] {
   sound (self,CHAN_VOICE,"zombie/z_idle.wav",TRUE,ATTN_IDLE);
   setsize (self,VEC_HULL_MIN,VEC_HULL_MAX);
   self.solid = SOLID_SLIDEBOX;
   if ( !walkmove (FALSE,FALSE) ) {
      self.think = mummy_paine11;
      self.solid = SOLID_NOT;
      return ;
   }
};
void () mummy_paine13 = [ 174.000, mummy_paine14 ] {};
void () mummy_paine14 = [ 175.000, mummy_paine15 ] {};
void () mummy_paine15 = [ 176.000, mummy_paine16 ] {};
void () mummy_paine16 = [ 177.000, mummy_paine17 ] {};
void () mummy_paine17 = [ 178.000, mummy_paine18 ] {};
void () mummy_paine18 = [ 179.000, mummy_paine19 ] {};
void () mummy_paine19 = [ 180.000, mummy_paine20 ] {};
void () mummy_paine20 = [ 181.000, mummy_paine21 ] {};
void () mummy_paine21 = [ 182.000, mummy_paine22 ] {};
void () mummy_paine22 = [ 183.000, mummy_paine23 ] {};
void () mummy_paine23 = [ 184.000, mummy_paine24 ] {};
void () mummy_paine24 = [ 185.000, mummy_paine25 ] {};
void () mummy_paine25 = [ 186.000, mummy_paine26 ] { ai_painforward (5); };
void () mummy_paine26 = [ 187.000, mummy_paine27 ] { ai_painforward (3); };
void () mummy_paine27 = [ 188.000, mummy_paine28 ] { ai_painforward (1); };
void () mummy_paine28 = [ 189.000, mummy_paine29 ] { ai_pain (1); };
void () mummy_paine29 = [ 190.000, mummy_paine30 ] {};
void () mummy_paine30 = [ 191.000, mummy_run1 ] {};

void () mummy_die = {
   self.health = -35.000;
   sound (self,CHAN_VOICE,"zombie/z_gib.wav",TRUE,ATTN_NORM);
   ThrowHead();
   ThrowGib (1,2);
   ThrowGib (2,2);
   ThrowGib (3,2);
   self.gibbed = TRUE;
};

void (entity inflictor, entity attacker, float take) mummy_pain = {
   local float r;

   if ( (self.pain_finished > time) )
      return ;

   r = random ();
   if ( (r > 0.240) )
      return ;

   self.pain_finished = (time + 2.500);
   if ( (r < 0.060) ) {
      mummy_paina1 ();
   } else {
      if ( (r < 0.120) ) {
         mummy_painb1 ();
      } else {
         if ( (r < 0.180) ) {
            mummy_painc1 ();
         } else {
            mummy_paind1 ();
         }
      }
   }
};

void () mummy_sleep = [ 172.000, mummy_sleep ] { ai_stand (); };

void () mummy_wake = {
   self.th_stand = mummy_stand1;
   self.th_walk = mummy_walk1;
   self.th_pain = mummy_pain;
   self.th_run = mummy_run1;
   self.th_missile = mummy_missile;
   mummy_paine12 ();
};

void (entity inflictor, entity attacker, float damage) mummy_wake_pain = { mummy_wake (); };

float MUMMY_STRONG = 4.000;

void () monster_mummy = {
   if ( deathmatch ) { remove (self); return; }

   self.mdl = "progs/rogue/mon_mummy.mdl";
   precache_model (self.mdl);
   self.headmdl = "progs/h_zombie.mdl";
   precache_model (self.headmdl);
   precache_model ("progs/zom_gib.mdl");
   precache_sound ("zombie/z_idle.wav");
   precache_sound ("zombie/z_idle1.wav");
   precache_sound ("zombie/z_shot1.wav");
   precache_sound ("zombie/z_gib.wav");
   precache_sound ("zombie/z_pain.wav");
   precache_sound ("zombie/z_pain1.wav");
   precache_sound ("zombie/z_fall.wav");
   precache_sound ("zombie/z_miss.wav");
   precache_sound ("zombie/z_hit.wav");
   precache_sound ("zombie/idle_w2.wav");

   self.pain_sound = "zombie/z_pain.wav";
   self.pain_sound2 = "zombie/z_pain1.wav";
   self.sight_sound = "zombie/z_idle.wav";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   self.bboxtype = BBOX_SHORT;
   if ( (self.spawnflags & MUMMY_STRONG) ) {
      if (self.health < 1) self.health = 1000.000;
   } else {
      if (self.health < 1) self.health = 500.000;
   }
   self.gibhealth = -60;
   self.gibbed = FALSE;
   self.pain_flinch = MONAI_ZOMBIEHGH_DAM;
   self.steptype = FS_TYPEDRAG;
   self.altsteptype = FS_TYPEDRAG;
   self.pain_timeout = -5;
   self.inpain = 0;
   self.count = 0;

   // Always reset Ammo Resistance to be consistent
   self.resist_shells = self.resist_nails = 0;
   self.resist_rockets = self.resist_cells = 0;

   self.th_stand = mummy_stand1;
   self.th_walk = mummy_walk1;
   self.th_run = mummy_run1;
   self.th_pain = mummy_pain;
   self.th_die = mummy_die;
   self.th_missile = mummy_missile;
   self.classtype = CT_MONMUMMY;
   self.classgroup = CG_ZOMBIE;
   self.classmove = MON_MOVEWALK;
   if ( (self.spawnflags & 4) ) {
      setsize (self,VEC_HULL_MIN,'16.000 16.000 -16.000');
      self.solid = SOLID_NOT;
      self.th_stand = mummy_sleep;
      self.th_run = mummy_wake;
      self.th_walk = mummy_wake;
      self.th_pain = mummy_wake_pain;
      self.th_missile = mummy_wake;
   }

    if (self.deathstring == "")  self.deathstring = " was mummified by a Mummy\n";
   monster_start();
};
