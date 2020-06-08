void () sword_stand1 = [ 0.000, sword_stand1 ] { ai_stand (); };

void () sword_run1 = [ 1.000, sword_run2 ] {
   self.effects = EF_DIMLIGHT;
   ai_run (14);
};
void () sword_run2 = [ 2.000, sword_run3 ] { ai_run (14); };
void () sword_run3 = [ 3.000, sword_run4 ] { ai_run (14); };
void () sword_run4 = [ 4.000, sword_run5 ] { ai_run (14); };
void () sword_run5 = [ 5.000, sword_run6 ] { ai_run (14); };
void () sword_run6 = [ 6.000, sword_run7 ] { ai_run (14); };
void () sword_run7 = [ 7.000, sword_run8 ] { ai_run (14); };
void () sword_run8 = [ 8.000, sword_run1 ] { ai_run (14); };

void () sword_atk1 = [ 9.000, sword_atk2 ] {
   sound (self,FALSE,"knight/sword1.wav",TRUE,TRUE);
   ai_charge (14);
};
void () sword_atk2 = [ 10.000, sword_atk3 ] { ai_charge (14); };
void () sword_atk3 = [ 11.000, sword_atk4 ] { ai_charge (14); };
void () sword_atk4 = [ 12.000, sword_atk5 ] { ai_charge (14); };
void () sword_atk5 = [ 13.000, sword_atk6 ] { ai_melee (); };
void () sword_atk6 = [ 14.000, sword_atk7 ] { ai_melee (); };
void () sword_atk7 = [ 15.000, sword_atk8 ] { ai_melee (); };
void () sword_atk8 = [ 16.000, sword_atk9 ] { ai_charge (14); };
void () sword_atk9 = [ 17.000, sword_atk10 ] { ai_charge (14); };
void () sword_atk10 = [ 18.000, sword_run1 ] { ai_charge (14); };

void () sword_die1 = [ 19.000, sword_die2 ] {};
void () sword_die2 = [ 20.000, sword_die3 ] { monster_check_gib(); };
void () sword_die3 = [ 21.000, sword_die4 ] { self.solid = SOLID_NOT; monster_check_gib(); };
void () sword_die4 = [ 22.000, sword_die5 ] {};
void () sword_die5 = [ 23.000, sword_die6 ] {};
void () sword_die6 = [ 24.000, sword_die7 ] {};
void () sword_die7 = [ 25.000, sword_die8 ] { sound (self,TRUE,"player/axhit2.wav",0.500,ATTN_NORM); };
void () sword_die8 = [ 26.000, sword_die9 ] {};
void () sword_die9 = [ 27.000, sword_die10 ] { monster_death_postcheck(); };
void () sword_die10 = [ 28.000, sword_die10 ] {};

void () sword_dieb1 = [ 29.000, sword_dieb2 ] {};
void () sword_dieb2 = [ 30.000, sword_dieb3 ] { monster_check_gib(); };
void () sword_dieb3 = [ 31.000, sword_dieb4 ] { self.solid = SOLID_NOT; monster_check_gib(); };
void () sword_dieb4 = [ 32.000, sword_dieb5 ] {};
void () sword_dieb5 = [ 33.000, sword_dieb6 ] {};
void () sword_dieb6 = [ 34.000, sword_dieb7 ] {};
void () sword_dieb7 = [ 35.000, sword_dieb8 ] { sound (self,TRUE,"player/axhit2.wav",0.500,ATTN_NORM); };
void () sword_dieb8 = [ 36.000, sword_dieb9 ] {};
void () sword_dieb9 = [ 37.000, sword_dieb10 ] {};
void () sword_dieb10 = [ 38.000, sword_dieb11 ] { monster_death_postcheck(); };
void () sword_dieb11 = [ 39.000, sword_dieb11 ] {};

void () sword_die = {
   monster_death_precheck();
   if (!self.gibbed) {
      if ( (random () < 0.500) ) {
         sword_die1 ();
      } else {
         sword_dieb1 ();
      }
   }
};

void () sword_pause = {
   self.th_run = sword_run1;
   self.think = sword_run1;
   self.nextthink = (time + self.delay);
   self.delay = FALSE;
};

void (entity inflictor,entity attacker, float damage) sword_pain = {
   self.th_run = sword_run1;
   self.think = sword_run1;
   self.nextthink = (time + 0.100);
   self.delay = FALSE;
   self.th_pain = SUB_Null_pain;
};

void () monster_sword = {
   if (deathmatch) { remove (self); return; }

   self.mdl = "progs/rogue/mon_sword.mdl";
   precache_model (self.mdl);
   precache_sound ("knight/sword1.wav");
   precache_sound ("knight/ksight.wav");
   precache_sound ("player/axhit2.wav");

   self.sight_sound = "knight/ksight.wav";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
   if (self.health < 1) self.health = 150;
   self.gibhealth = -1000000;
   self.gibbed = FALSE;
   self.steptype = FS_FLYING;

   if ( !self.delay )
      self.delay = 0.1;

   // Always reset Ammo Resistance to be consistent
   self.resist_shells = self.resist_nails = 0;
   self.resist_rockets = self.resist_cells = 0;

   self.classtype = CT_MONINVISSWORD;
   self.classgroup = CG_DKNIGHT;
   self.classmove = MON_MOVEWALK;
    self.hittype    = HIT_METAL;

   self.th_stand = sword_stand1;
   self.th_walk = sword_stand1;
   self.th_run = sword_pause;
   self.th_melee = sword_atk1;
   self.th_die = sword_die;
   self.th_pain = sword_pain;
   self.classmove = MON_MOVEWALK;
    if (self.deathstring == "")  self.deathstring = " was slit open by a Phantom Swordsman\n";
   monster_start ();
};
