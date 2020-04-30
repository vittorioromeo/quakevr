$frame eswim1 eswim2 eswim3 eswim4 eswim5 eswim6

$frame edeath1 edeath2 edeath3 edeath4 edeath5 edeath6 edeath7 edeath8
$frame edeath9 edeath10 edeath11 edeath12 edeath13 edeath14 edeath15

void (entity inflictor, entity attacker, float damage) eel_pain1;

void () eel_pitch_change = {
   if ( (pointcontents (self.origin) != CONTENT_WATER) ) {
      droptofloor ();
      T_Damage (self,world,world,MOVETYPE_TOSS,DAMARMOR);
      return ;
   }
   if ( (time < self.delay) )
      return ;

   if ( (self.weapon > MOVETYPE_BOUNCE) )
      self.weapon = -10.000;

   if ( (self.weapon < FALSE) ) {
      self.angles_x = (self.angles_x - 1.500);
   } else {
      if ( (self.weapon > FALSE) )
         self.angles_x = (self.angles_x + 1.500);
   }
   self.weapon = (self.weapon + TRUE);
};

void () eel_stand1 = [ 0.000, eel_stand2 ] {
   ai_stand ();
   eel_pitch_change ();
};
void () eel_stand2 = [ 1.000, eel_stand3 ] {
   ai_stand ();
   eel_pitch_change ();
};
void () eel_stand3 = [ 2.000, eel_stand4 ] {
   ai_stand ();
   eel_pitch_change ();
};
void () eel_stand4 = [ 3.000, eel_stand5 ] {
   ai_stand ();
   eel_pitch_change ();
};
void () eel_stand5 = [ 4.000, eel_stand6 ] {
   ai_stand ();
   eel_pitch_change ();
};
void () eel_stand6 = [ 5.000, eel_stand1 ] {
   ai_stand ();
   eel_pitch_change ();
};

void () eel_walk1 = [ 0.000, eel_walk2 ] {
   if ( (random () < 0.200) )
      sound (self,CHAN_VOICE,"eel/eactive1.wav",TRUE,ATTN_IDLE);

   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};
void () eel_walk2 = [ 1.000, eel_walk3 ] {
   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};
void () eel_walk3 = [ 2.000, eel_walk4 ] {
   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};
void () eel_walk4 = [ 3.000, eel_walk5 ] {
   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};
void () eel_walk5 = [ 4.000, eel_walk6 ] {
   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};
void () eel_walk6 = [ 5.000, eel_walk1 ] {
   ai_walk (MOVETYPE_TOSS);
   eel_pitch_change ();
};

void () eel_run1 = [ 0.000, eel_run2 ] {
   if ( (random () < 0.400) )
      sound (self,CHAN_VOICE,"eel/eactive1.wav",TRUE,ATTN_IDLE);

   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};
void () eel_run2 = [ 1.000, eel_run3 ] {
   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};
void () eel_run3 = [ 2.000, eel_run4 ] {
   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};
void () eel_run4 = [ 3.000, eel_run5 ] {
   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};
void () eel_run5 = [ 4.000, eel_run6 ] {
   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};
void () eel_run6 = [ 5.000, eel_run1 ] {
   ai_run (MOVETYPE_BOUNCE);
   eel_pitch_change ();
};


//============================================================================
// RANGE - Plasma from mouth
//============================================================================
void() eel_plasma =
{
    local vector org, dir;

    // Check if can fire bolt of plasma
    if (visblocked_wide(self.enemy, self.attack_offset, '0 0 0')) return;    
    
    self.effects = self.effects | EF_MUZZLEFLASH;
    self.attack_speed = SPEED_EELBOLT + (skill * SPEED_EELSKILL);

    makevectors (self.angles);
    org = self.origin + attack_vector(self.attack_offset);
    dir = normalize(self.enemy.origin - self.origin);

    launch_plasma(org, dir, CT_PROJ_PLASMA, self.attack_speed);
};

void () eel_melee = {
   local float attackOK;

   attackOK = TRUE;
   traceline (self.origin,self.enemy.origin,FALSE,self);
   if ( (trace_ent != self.enemy) )
      attackOK = FALSE;

   if ( (trace_inopen && trace_inwater) )
      attackOK = FALSE;

   if ( attackOK )
      T_EELZap (self,self,45.000, world, DAMAGEALL);

   self.skin = FALSE;
   self.effects = FALSE;
};

void () eel_attack1 = [ 0.000, eel_attack2 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack2 = [ 1.000, eel_attack3 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack3 = [ 2.000, eel_attack4 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack4 = [ 3.000, eel_attack5 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack5 = [ 4.000, eel_attack6 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack6 = [ 5.000, eel_attack7 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack7 = [ 0.000, eel_attack8 ] {
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack8 = [ 1.000, eel_attack9 ] {
   self.effects = EF_DIMLIGHT;
   self.skin = TRUE;
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
   sound (self,CHAN_WEAPON,"eel/eatt1.wav",TRUE,ATTN_NORM);
};
void () eel_attack9 = [ 2.000, eel_attack10 ] {
   self.skin = FL_SWIM;
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack10 = [ 3.000, eel_attack11 ] {
   self.skin = MOVETYPE_WALK;
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack11 = [ 4.000, eel_attack12 ] {
   self.effects = EF_BRIGHTLIGHT;
   self.skin = MOVETYPE_STEP;
   ai_charge (FL_CLIENT);
   eel_pitch_change ();
};
void () eel_attack12 = [ 5.000, eel_run1 ] {
   self.skin = MOVETYPE_FLY;
   eel_melee ();
};

void() eel_attack2_1 = [ $eswim1, eel_attack2_2 ] {self.skin = 1;ai_face();
    sound (self, CHAN_VOICE, "eel/attack1.wav", 1, ATTN_NORM);};
void() eel_attack2_2 = [ $eswim2, eel_attack2_3 ] {self.skin = 2;ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;};
void() eel_attack2_3 = [ $eswim3, eel_attack2_4 ] {self.skin = 3;ai_face();};
void() eel_attack2_4 = [ $eswim4, eel_attack2_5 ] {self.skin = 4;ai_face();
    self.effects = self.effects | EF_MUZZLEFLASH;};
void() eel_attack2_5 = [ $eswim5, eel_attack2_6 ] {self.skin = 5;ai_face();};
void() eel_attack2_6 = [ $eswim6, eel_attack2_7 ] {
    self.skin = 4; ai_face(); eel_plasma();};
void() eel_attack2_7 = [ $eswim1, eel_attack2_8 ] {self.skin = 5;};
void() eel_attack2_8 = [ $eswim2, eel_attack2_9 ] {self.skin = 4;};
void() eel_attack2_9 = [ $eswim3, eel_attack2_10 ] {self.skin = 3;};
void() eel_attack2_10 = [ $eswim4, eel_attack2_11 ] {self.skin = 2;};
void() eel_attack2_11 = [ $eswim5, eel_attack2_12 ] {self.skin = 1;ai_face();};
void() eel_attack2_12 = [ $eswim6, eel_run1 ] {self.skin = 0;ai_face();};

void () eel_death1 = [ 6.000, eel_death2 ] {
   self.skin = FALSE;
   self.effects = FALSE;
   sound (self,CHAN_VOICE,"eel/edie3r.wav",TRUE,ATTN_NORM);
};

void () eel_death2 = [ 7.000, eel_death3 ] { monster_check_gib(); };
void () eel_death3 = [ 8.000, eel_death4 ] { monster_check_gib(); };
void () eel_death4 = [ 9.000, eel_death5 ] {};
void () eel_death5 = [ 8.000, eel_death6 ] {};
void () eel_death6 = [ 7.000, eel_death7 ] {};
void () eel_death7 = [ 6.000, eel_death8 ] {};
void () eel_death8 = [ 7.000, eel_death9 ] {};
void () eel_death9 = [ 8.000, eel_death10 ] {};
void () eel_death10 = [ 9.000, eel_death11 ] {};
void () eel_death11 = [ 10.000, eel_death12 ] { self.flags = (self.flags - FL_SWIM); };
void () eel_death12 = [ 11.000, eel_death13 ] {};
void () eel_death13 = [ 12.000, eel_death14 ] {};
void () eel_death14 = [ 13.000, eel_death15 ] {};
void () eel_death15 = [ 14.000, eel_death16 ] { droptofloor (); };
void () eel_death16 = [ 15.000, eel_death16 ] {
   self.solid = SOLID_NOT;
   monster_death_postcheck();
};

void () eel_death = {
   if (pointcontents(self.origin) == CONTENT_WATER) self.gravity = 0.25;
   monster_death_precheck();
   if (!self.gibbed) { eel_death1 (); }
};

void (entity inflictor, entity attacker, float damage) eel_pain1 = [ 6.000, eel_pain2 ] {
   if ( (self.pain_finished > time) )
      return ;

   self.pain_finished = (time + TRUE);
   sound (self,CHAN_VOICE,"eel/epain3.wav",TRUE,ATTN_NORM);
   self.skin = FALSE;
};
void () eel_pain2 = [ 7.000, eel_pain3 ] {};
void () eel_pain3 = [ 8.000, eel_pain4 ] {};
void () eel_pain4 = [ 9.000, eel_pain5 ] {};
void () eel_pain5 = [ 8.000, eel_pain6 ] {};
void () eel_pain6 = [ 7.000, eel_pain7 ] {};
void () eel_pain7 = [ 6.000, eel_run1 ] {};

void () monster_eel = {
   if ( deathmatch ) { remove(self); return ; }

   self.mdl = "progs/ad171/mon_eel.mdl";
   self.headmdl = "progs/ad171/eelgib.mdl";
   precache_model (self.mdl);
   precache_model ("progs/ad171/eelgib.mdl");
   precache_model ("progs/ad171/gib1.mdl");
   precache_model ("progs/ad171/proj_plasma.mdl");
   precache_sound ("eel/edie3r.wav");
   precache_sound ("eel/epain3.wav");
   precache_sound ("eel/eactive1.wav");
   precache_sound ("eel/eatt1.wav");
   precache_sound ("eel/eelc5.wav");
   precache_sound ("eel/attack1.wav");

    // Check for poisonous entity flag
    if (self.poisonous) {
        precache_poisongibs();            // precache gibs
        self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
        precache_model (MODEL_PROJ_PLASMAGRN);
    }

   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   if (self.bboxtype < 1) self.bboxtype = BBOX_EEL;
   if (self.health < 1) self.health = 60;
   self.gibhealth = -25;
   self.gibbed = FALSE;
   self.blockudeath = TRUE;
   self.pain_flinch = 10;    // Easy response to pain
   self.steptype = FS_FLYING;        // It has fins!?!

    // Check for water sightline wakeup
    if (self.spawnflags & MON_LIQUIDBLOCK) self.liquidblock = TRUE;

   // Always reset Ammo Resistance to be consistent
   self.resist_shells = self.resist_nails = 0;
   self.resist_rockets = self.resist_cells = 0;

   self.th_stand = eel_stand1;
   self.th_walk = eel_walk1;
   self.th_run = eel_run1;
   self.th_die = eel_death;
   self.th_pain = eel_pain1;
   self.th_melee = eel_attack1;
   self.th_missile = eel_attack2_1;

   self.classtype = CT_MONEEL;
   self.classgroup = CG_FISH;
   self.classmove = MON_MOVESWIM;
    if (self.deathstring == "")  self.deathstring = " was electrified by an Eel\n";

   monster_start ();
};
