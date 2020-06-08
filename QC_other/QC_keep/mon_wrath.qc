void () WrathMissileTouch;
void () WrathHome;
void (float AttackNumber) WrathMissile;

void () wrath_stand1 = [ 1.000, wrath_stand1 ] {
    if (random() < 0.1) monster_idle_sound();
    ai_stand ();
};

void () wrath_walk01 = [ 1.000, wrath_walk02 ] { monster_idle_sound(); ai_walk (8); };
void () wrath_walk02 = [ 2.000, wrath_walk03 ] { ai_walk (8); };
void () wrath_walk03 = [ 3.000, wrath_walk04 ] { ai_walk (8); };
void () wrath_walk04 = [ 4.000, wrath_walk05 ] { ai_walk (8); };
void () wrath_walk05 = [ 5.000, wrath_walk06 ] { ai_walk (8); };
void () wrath_walk06 = [ 6.000, wrath_walk07 ] { ai_walk (8); };
void () wrath_walk07 = [ 7.000, wrath_walk08 ] { ai_walk (8); };
void () wrath_walk08 = [ 8.000, wrath_walk09 ] { ai_walk (8); };
void () wrath_walk09 = [ 9.000, wrath_walk10 ] { ai_walk (8); };
void () wrath_walk10 = [ 10.000, wrath_walk11 ] { ai_walk (8); };
void () wrath_walk11 = [ 11.000, wrath_walk12 ] { ai_walk (8); };
void () wrath_walk12 = [ 12.000, wrath_walk01 ] { ai_walk (8); };

void () wrath_run01 = [ 1.000, wrath_run02 ] { monster_idle_sound(); ai_run (12); };
void () wrath_run02 = [ 2.000, wrath_run03 ] { ai_run (12); };
void () wrath_run03 = [ 3.000, wrath_run04 ] { ai_run (12); };
void () wrath_run04 = [ 4.000, wrath_run05 ] { ai_run (12); };
void () wrath_run05 = [ 5.000, wrath_run06 ] { ai_run (12); };
void () wrath_run06 = [ 6.000, wrath_run07 ] { ai_run (12); };
void () wrath_run07 = [ 7.000, wrath_run08 ] { ai_run (12); };
void () wrath_run08 = [ 8.000, wrath_run09 ] { ai_run (12); };
void () wrath_run09 = [ 9.000, wrath_run10 ] { ai_run (12); };
void () wrath_run10 = [ 10.000, wrath_run11 ] { ai_run (12); };
void () wrath_run11 = [ 11.000, wrath_run12 ] { ai_run (12); };
void () wrath_run12 = [ 12.000, wrath_run01 ] { ai_run (12); };

void () wrath_at_a01 = [ 13.000, wrath_at_a02 ] { ai_charge (12); };
void () wrath_at_a02 = [ 14.000, wrath_at_a03 ] { ai_charge (12); };
void () wrath_at_a03 = [ 15.000, wrath_at_a04 ] { ai_charge (12); };
void () wrath_at_a04 = [ 16.000, wrath_at_a05 ] { ai_charge (12); };
void () wrath_at_a05 = [ 17.000, wrath_at_a06 ] { ai_charge (12); };
void () wrath_at_a06 = [ 18.000, wrath_at_a07 ] { ai_charge (12); };
void () wrath_at_a07 = [ 19.000, wrath_at_a08 ] { ai_charge (12); };
void () wrath_at_a08 = [ 20.000, wrath_at_a09 ] { ai_charge (12); };
void () wrath_at_a09 = [ 21.000, wrath_at_a10 ] { ai_charge (12); };
void () wrath_at_a10 = [ 22.000, wrath_at_a11 ] { ai_charge (12); };
void () wrath_at_a11 = [ 23.000, wrath_at_a12 ] { WrathMissile (1); };
void () wrath_at_a12 = [ 24.000, wrath_at_a13 ] { ai_charge (12); };
void () wrath_at_a13 = [ 25.000, wrath_at_a14 ] { ai_charge (12); };
void () wrath_at_a14 = [ 26.000, wrath_run01 ] { ai_charge (12); };

void () wrath_at_b01 = [ 27.000, wrath_at_b02 ] { ai_charge (12); };
void () wrath_at_b02 = [ 28.000, wrath_at_b03 ] { ai_charge (12); };
void () wrath_at_b03 = [ 29.000, wrath_at_b04 ] { ai_charge (12); };
void () wrath_at_b04 = [ 30.000, wrath_at_b05 ] { ai_charge (12); };
void () wrath_at_b05 = [ 31.000, wrath_at_b06 ] { ai_charge (12); };
void () wrath_at_b06 = [ 32.000, wrath_at_b07 ] { WrathMissile (2); };
void () wrath_at_b07 = [ 33.000, wrath_at_b08 ] { ai_charge (12); };
void () wrath_at_b08 = [ 34.000, wrath_at_b09 ] { ai_charge (12); };
void () wrath_at_b09 = [ 35.000, wrath_at_b10 ] { ai_charge (12); };
void () wrath_at_b10 = [ 36.000, wrath_at_b11 ] { ai_charge (12); };
void () wrath_at_b11 = [ 37.000, wrath_at_b12 ] { ai_charge (12); };
void () wrath_at_b12 = [ 38.000, wrath_at_b13 ] { ai_charge (12); };
void () wrath_at_b13 = [ 39.000, wrath_run01 ] { ai_charge (12); };

void () wrath_at_c01 = [ 40.000, wrath_at_c02 ] { ai_charge (12); };
void () wrath_at_c02 = [ 41.000, wrath_at_c03 ] { ai_charge (12); };
void () wrath_at_c03 = [ 42.000, wrath_at_c04 ] { ai_charge (12); };
void () wrath_at_c04 = [ 43.000, wrath_at_c05 ] { ai_charge (12); };
void () wrath_at_c05 = [ 44.000, wrath_at_c06 ] { ai_charge (12); };
void () wrath_at_c06 = [ 45.000, wrath_at_c07 ] { ai_charge (12); };
void () wrath_at_c07 = [ 46.000, wrath_at_c08 ] { WrathMissile (3); };
void () wrath_at_c08 = [ 47.000, wrath_at_c09 ] { ai_charge (12); };
void () wrath_at_c09 = [ 48.000, wrath_at_c10 ] { ai_charge (12); };
void () wrath_at_c10 = [ 49.000, wrath_at_c11 ] { ai_charge (12); };
void () wrath_at_c11 = [ 50.000, wrath_at_c12 ] { ai_charge (12); };
void () wrath_at_c12 = [ 51.000, wrath_at_c13 ] { ai_charge (12); };
void () wrath_at_c13 = [ 52.000, wrath_at_c14 ] { ai_charge (12); };
void () wrath_at_c14 = [ 53.000, wrath_at_c15 ] { ai_charge (12); };
void () wrath_at_c15 = [ 54.000, wrath_run01 ] { ai_charge (12); };

void () wrath_attack = {
   local float r;

   r = random ();
   if ( (r < 0.250) ) {
      wrath_at_a01 ();
   } else {
      if ( (r < 0.650) ) {
         wrath_at_b01 ();
      } else {
         wrath_at_c01 ();
      }
   }
   sound (self,CHAN_VOICE,"wrath/watt.wav",TRUE,ATTN_NORM);
};

void () wrath_pn_a01 = [ 55.000, wrath_pn_a02 ] {};
void () wrath_pn_a02 = [ 56.000, wrath_pn_a03 ] {};
void () wrath_pn_a03 = [ 57.000, wrath_pn_a04 ] {};
void () wrath_pn_a04 = [ 58.000, wrath_pn_a05 ] {};
void () wrath_pn_a05 = [ 59.000, wrath_pn_a06 ] {};
void () wrath_pn_a06 = [ 60.000, wrath_run01 ] {};

void () wrath_pn_b01 = [ 61.000, wrath_pn_b02 ] {};
void () wrath_pn_b02 = [ 62.000, wrath_pn_b03 ] {};
void () wrath_pn_b03 = [ 63.000, wrath_pn_b04 ] {};
void () wrath_pn_b04 = [ 64.000, wrath_pn_b05 ] {};
void () wrath_pn_b05 = [ 65.000, wrath_pn_b06 ] {};
void () wrath_pn_b06 = [ 66.000, wrath_pn_b07 ] {};
void () wrath_pn_b07 = [ 67.000, wrath_pn_b08 ] {};
void () wrath_pn_b08 = [ 68.000, wrath_pn_b09 ] {};
void () wrath_pn_b09 = [ 69.000, wrath_pn_b10 ] {};
void () wrath_pn_b10 = [ 70.000, wrath_pn_b11 ] {};
void () wrath_pn_b11 = [ 71.000, wrath_run01 ] {};

void (entity inflictor, entity attacker, float damage) wrath_pain = {
   local float r;

   if ( (self.pain_finished > time) )
      return ;

   r = random ();
   if ( (r > 0.100) ) {
      self.pain_finished = (time + 0.500);
      return ;
   }
   if ( (r < 0.070) ) {
      wrath_pn_a01 ();
   } else {
      wrath_pn_b01 ();
   }
   self.pain_finished = (time + 3);
   sound (self,CHAN_VOICE,"wrath/wpain.wav",TRUE,ATTN_NORM);
};

void () wrath_die02 = [ 73.000, wrath_die03 ] {};
void () wrath_die03 = [ 74.000, wrath_die04 ] {};
void () wrath_die04 = [ 75.000, wrath_die05 ] {};
void () wrath_die05 = [ 76.000, wrath_die07 ] {};
void () wrath_die07 = [ 78.000, wrath_die09 ] {};
void () wrath_die09 = [ 80.000, wrath_die11 ] {};
void () wrath_die11 = [ 82.000, wrath_die13 ] {};
void () wrath_die13 = [ 84.000, wrath_die15 ] {};
void () wrath_die15 = [ 86.000, wrath_die15 ] {
     // Throw gibs, but create more XY outward velocity
    self.max_health = MON_XYGIBVELOCITY;
    monster_ThrowGib();
    // Move the explosion up to the middle/top of the body
    // Use to be a regular explosion, switched to tarbaby instead
    self.origin = self.origin + '0 0 24';
    T_RadiusDamage (self, self, 80, world, IGNORECLASS);
    
    // Big purple explosion
    if (ext_dppart) 
        pointparticles(particleeffectnum(DPP_WRAITHEXPLODE), self.origin, '0 0 0', 1);
    else {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);    // Hard coded explosion
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }
    
    // Play original explosion sound or replacement
    sound(self, CHAN_WEAPON, SOUND_REXP3, 1, ATTN_NORM);
    // Stop thinking and hide entity (prevent wraith_death15 loop)
    self.nextthink = LARGE_TIMER;
    self.think = SUB_Null;
    entity_hide(self);
};

void () wrath_die01 = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    self.solid = SOLID_NOT;

    sound (self,CHAN_VOICE,"wrath/wdthc.wav",TRUE,ATTN_NORM);

    // Make sure body stays floating up where it finally dies
    // and do not drop to the ground (reset by monster_death_use func)
    self.flags = self.flags | FL_FLY;
    self.movetype = MOVETYPE_FLY;
    wrath_die02();
};

void () monster_wrath = {
   if ( deathmatch ) { remove (self); return; }

   self.mdl = "progs/rogue/wrath.mdl";
   self.gib1mdl = "progs/drake/wrthgib1.mdl";
   self.gib2mdl = "progs/drake/wrthgib2.mdl";
   self.gib3mdl = "progs/drake/wrthgib3.mdl";

   precache_model ("progs/rogue/wrath.mdl");
   precache_model ("progs/drake/w_ball.mdl");
   precache_model ("progs/drake/wrthgib1.mdl");
   precache_model ("progs/drake/wrthgib2.mdl");
   precache_model ("progs/drake/wrthgib3.mdl");
   precache_sound ("wrath/wsee.wav");
   precache_sound ("wrath/watt.wav");

   self.pain_sound = "wrath/wpain.wav";
   precache_sound ("wrath/wpain.wav");

   precache_sound ("wrath/wdthc.wav");

   self.idle_sound = "wraith/idle1.wav";
   self.idle_sound2 = "wraith/idle2.wav";
   self.idle_soundcom = "wraith/idle3.wav";
   precache_sound (self.idle_sound);
   precache_sound (self.idle_sound2);
   precache_sound (self.idle_soundcom);

   self.sight_sound = "wrath/wsee.wav";
   precache_sound (self.sight_sound);

   self.solid = SOLID_NOT;                // No interaction with world
   self.movetype = MOVETYPE_NONE;        // Static item, no movement
   if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
   if (self.health < 1) self.health = 400;

   self.yaw_speed = 35.000; // Can turn really fast
   self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
   self.gibbed = FALSE;                // not falling apart
   self.pain_flinch = 70;                // High pain threshold
   self.blockudeath = TRUE;            // no humanoid death sound
   self.height = MONAI_ABOVEDIST;        // Enemytarget distance above

   // Always reset Ammo Resistance to be consistent
   self.resist_shells = self.resist_nails = 0;
   self.resist_rockets = self.resist_cells = 0;

   self.th_stand = wrath_stand1;
   self.th_walk = wrath_walk01;
   self.th_run = wrath_run01;
   self.th_missile = wrath_attack;
   self.th_pain = wrath_pain;
   self.th_die = wrath_die01;

   self.classtype = CT_MONWRAITH;
   self.classgroup = CG_SPIDER;
   self.classmove = MON_MOVEFLY;
    if (self.deathstring == "")  self.deathstring = " was disintegrated by a Wrath\n";

   monster_start ();
};

void (float AttackNumber) WrathMissile = {
   local entity missile;
   local vector dir;
   local vector org;
   local float dist;
   local float flytime;

   dir = normalize (((self.enemy.origin + '0.000 0.000 10.000') - self.origin));
   dist = vlen ((self.enemy.origin - self.origin));
   flytime = (dist * 0.002);
   if ( (flytime < 0.100) )
      flytime = 0.100;

   self.effects = (self.effects | EF_MUZZLEFLASH);
   makevectors (self.angles);
   missile = spawn ();
   missile.owner = self;
   missile.solid = SOLID_BBOX;
   missile.movetype = MOVETYPE_FLYMISSILE;
   setmodel (missile,"progs/drake/w_ball.mdl");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   if ( (AttackNumber == 1) ) {
      org = ((self.origin + (v_forward * 20.000)) + (v_up * 12));
   } else {
      if ( (AttackNumber == 2) ) {
         org = ((self.origin + (v_forward * 18.000)) + (v_up * 10));
      } else {
         if ( (AttackNumber == 3) ) {
            org = (((self.origin + (v_forward * 12)) + (v_up * 12)) + (v_right * 20.000));
         } else {
            if ( (AttackNumber == 4) )
               org = ((self.origin + (v_forward * 20.000)) + (v_up * 16));
         }
      }
   }
   setorigin (missile,org);
   missile.velocity = (dir * 400);
   missile.avelocity = '300.000 300.000 300.000';
   missile.enemy = self.enemy;
   missile.touch = WrathMissileTouch;
   missile.nextthink = (time + 0.100);
   missile.think = WrathHome;
   self.attack_finished = (time + 2);
};

void () WrathHome = {
   if ( (self.enemy.health < TRUE) ) {
      remove (self);
      return ;
   }
   if ( (skill == 3) ) {
      ai_track (self.enemy,550.000);
   } else {
      ai_track (self.enemy,400);
   }
   self.nextthink = (time + 0.100);
   self.think = WrathHome;
};

void () WrathMissileTouch = {
   if ( (((other == self.owner) || (other.classname == "monster_wrath")) || (other.classname == "monster_super_wrath")) ) {
      remove (self);
      return ;
   }
   if ( (other.classname == "monster_zombie") )
      T_Damage (other,self,self,110.000,DAMARMOR);

   T_RadiusDamage (self,self.owner,20.000,world,DAMAGEALL);
   sound (self,CHAN_WEAPON,"weapons/r_exp3.wav",TRUE,ATTN_NORM);
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   self.velocity = VEC_ORIGIN;
   self.touch = SUB_Null;
   setmodel (self,"progs/drake/s_explod.spr");
   self.solid = SOLID_NOT;
   s_explode1 ();
};
