void () dragon_fireball;
void () dragon_melee1;
float DDIE_FALLING    = 1.000;
float DDIE_STOPPED    = 2.000;
float DDIE_EXPLODED    = 3.000;
float DT_ROOM_A    = 1.000;
float DT_ROOM_B    = 2.000;
float DT_ROOM_C    = 4.000;
float DRAGON_ROGUE = 2;

void (string gibname, float dm) AccelerateGib = {
   local entity new;
   local float offset1;

   new = spawn ();
   new.origin = self.origin;
   setmodel (new,gibname);
   setsize (new,'-8.000 -8.000 -8.000','8.000 8.000 8.000');
   new.velocity = (-1.250 * self.velocity);
   makevectors (new.velocity);
   offset1 = ((random () * 300.000) - 150.000);
   new.velocity = (new.velocity + (v_right * offset1));
   offset1 = ((random () * 300.000) - 150.000);
   new.velocity = (new.velocity + (v_up * offset1));
   new.movetype = MOVETYPE_BOUNCE;
   new.solid = SOLID_NOT;
   new.avelocity_x = (random () * 600.000);
   new.avelocity_y = (random () * 600.000);
   new.avelocity_z = (random () * 600.000);
   new.think = SUB_Remove;
   new.ltime = time;
   new.nextthink = ((time + 10) + (random () * 10));
   new.frame = FALSE;
   new.flags = FALSE;
};

void (float gibCount) ViolentDeath = {
   while ( (gibCount > FALSE) ) {
      AccelerateGib (GIB1_BLOOD,(-4 * gibCount));
      AccelerateGib (GIB2_BLOOD,(-6 * gibCount));
      AccelerateGib (GIB3_BLOOD,(-8 * gibCount));
      gibCount = (gibCount - 3);
   }
};

void () dragon_stop_attack = {
   if (self.dragonAttacking == FALSE)
      return ;

   self.attack_finished = (time + (random () * FL_SWIM) );
   self.dragonAttacking = FALSE;
   traceline (self.origin,self.movetarget.origin,TRUE,world);
   if ( (trace_fraction == TRUE) )
      return ;
   else
      bprint ("Error: Dragon cannot get to next target!\n");
};

void () dragon_check_attack = {
   local vector vec;
   local float dot;

   if ( (self.dragonAttacking == TRUE) )
      return ;

   if ( (self.th_missile == SUB_Null) )
      return ;

   if ( (self.attack_finished > time) )
      return ;

   if ( (self.enemy.health < FALSE) )
      self.enemy = world;

   if ( (self.enemy.flags & FL_NOTARGET) )
      return ;

   if ( (self.enemy == world) ) {
      FindTarget ();
   } else {
      makevectors (self.angles);
      vec = normalize ((self.enemy.origin - self.origin));
      dot = (vec * v_forward);
      if ( (dot > 0.300) ) {
         traceline (self.origin,self.enemy.origin,TRUE,world);
         if ( (trace_fraction == TRUE) ) {
            dot = vlen ((self.enemy.origin - self.origin));
            if ( (dot < 350.000) ) {
               self.dragonAttacking = TRUE;
               self.think = dragon_melee1;
               return ;
            } else {
               self.dragonAttacking = TRUE;
               self.think = self.th_missile;
            }
         }
      }
   }
};

void (float dist) dragon_move = {
   local vector moveDir;
   local vector moveAngles;
   local float leftTurn;
   local float rightTurn;
   local float turnOffset;
   local entity oldEnemy;

   if ( (self.health < TRUE) ) { remove (self); return ; }
   if ( (self.dragonAttacking == FALSE) )
      dragon_check_attack ();

   oldEnemy = self.enemy;
   if ( (self.dragonAttacking == FALSE) ) {
      moveDir = (self.movetarget.origin - self.origin);
      moveAngles = vectoangles (moveDir);
      self.enemy = self.movetarget;
   } else {
      moveDir = (self.enemy.origin - self.origin);
      moveAngles = vectoangles (moveDir);
   }
   turnOffset = (self.angles_y - moveAngles_y);
   if ( (turnOffset != FALSE) ) {
      turnOffset = (180.000 - self.angles_y);
      leftTurn = (anglemod ((moveAngles_y + turnOffset)) - 180.000);
      rightTurn = (180.000 - anglemod ((moveAngles_y + turnOffset)));
      if ( (leftTurn < FALSE) ) {
         leftTurn = 360.000;
      } else {
         if ( (rightTurn < FALSE) )
            rightTurn = 360.000;
      }
      self.yaw_speed = MOVETYPE_BOUNCE;
      if ( (rightTurn < 180.000) ) {
         if ( (self.yaw_speed < rightTurn) ) {
            self.angles_y = (self.angles_y - self.yaw_speed);
         } else {
            self.angles_y = moveAngles_y;
         }
         if ( (rightTurn > MOVETYPE_FLY) ) {
            self.angles_z = (self.angles_z + MOVETYPE_FLY);
            if ( (self.angles_z > SVC_INTERMISSION) )
               self.angles_z = SVC_INTERMISSION;
         }
         turnOffset = rightTurn;
      } else {
         if ( (self.yaw_speed < rightTurn) ) {
            self.angles_y = (self.angles_y + self.yaw_speed);
         } else {
            self.angles_y = moveAngles_y;
         }
         if ( (leftTurn > MOVETYPE_FLY) ) {
            self.angles_z = (self.angles_z - MOVETYPE_FLY);
            if ( (self.angles_z < -30.000) )
               self.angles_z = -30.000;
         }
         turnOffset = leftTurn;
      }
   } else {
      if ( (self.angles_z != FALSE) ) {
         if ( (self.angles_z < CONTENT_LAVA) ) {
            self.angles_z = (self.angles_z + MOVETYPE_FLY);
         } else {
            if ( (self.angles_z < MOVETYPE_FLY) ) {
               self.angles_z = FALSE;
            } else {
               if ( (self.angles_z > MOVETYPE_FLY) )
                  self.angles_z = (self.angles_z - MOVETYPE_FLY);
            }
         }
      }
   }
   if ( (moveDir_z > MOVETYPE_FLY) ) {
      setorigin (self,(self.origin + '0.000 0.000 5.000'));
   } else {
      if ( (moveDir_z < CONTENT_LAVA) )
         setorigin (self,(self.origin - '0.000 0.000 5.000'));
   }
   moveAngles = self.origin;
   walkmove (self.angles_y,dist);
   if ( (moveAngles == self.origin) )
      movetogoal (dist);

   self.enemy = oldEnemy;
};

void () dragon_stand1 = [ 1.000, dragon_walk1 ] {};

void () dragon_walk1;
void () dragon_walk3;
void () dragon_walk5;
void () dragon_walk7;
void () dragon_walk9;
void () dragon_walk11;
void () dragon_walk13;

void () dragon_atk_a1 = [ 43.000, dragon_atk_a2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_a2 = [ 44.000, dragon_atk_a3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_a3 = [ 45.000, dragon_walk5 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_atk_b1 = [ 46.000, dragon_atk_b2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_b2 = [ 47.000, dragon_atk_b3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_b3 = [ 48.000, dragon_walk7 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_atk_c1 = [ 49.000, dragon_atk_c2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_c2 = [ 50.000, dragon_atk_c3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_c3 = [ 51.000, dragon_walk9 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_atk_d1 = [ 52.000, dragon_atk_d2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_d2 = [ 53.000, dragon_atk_d3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_d3 = [ 54.000, dragon_walk11 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_atk_e1 = [ 55.000, dragon_atk_e2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_e2 = [ 56.000, dragon_atk_e3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_e3 = [ 57.000, dragon_walk13 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_atk_f1 = [ 58.000, dragon_atk_f2 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_f2 = [ 59.000, dragon_atk_f3 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_f3 = [ 60.000, dragon_atk_f4 ] { dragon_move (17.000); dragon_fireball (); };
void () dragon_atk_f4 = [ 60.000, dragon_walk3 ] { dragon_move (17.000); dragon_fireball (); dragon_stop_attack (); };

void () dragon_walk1 = [ 1.000, dragon_walk2 ] {
   if ( self.dragonAttacking )
      dragon_stop_attack ();

   self.th_missile = dragon_atk_a1;
   self.dragonPainSequence = TRUE;
   dragon_move (17.000);
   if ( (random () < 0.200) )
      sound (self,CHAN_VOICE,"dragon/active.wav",TRUE,ATTN_IDLE);
};
void () dragon_walk2 = [ 2.000, dragon_walk3 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk3 = [ 3.000, dragon_walk4 ] { self.th_missile = dragon_atk_b1; dragon_move (17.000); self.dragonPainSequence = 2; };
void () dragon_walk4 = [ 4.000, dragon_walk5 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk5 = [ 5.000, dragon_walk6 ] { self.th_missile = dragon_atk_c1; dragon_move (17.000); self.dragonPainSequence = 3; };
void () dragon_walk6 = [ 6.000, dragon_walk7 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk7 = [ 7.000, dragon_walk8 ] { self.th_missile = dragon_atk_d1; dragon_move (17.000); self.dragonPainSequence = 4; };
void () dragon_walk8 = [ 8.000, dragon_walk9 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk9 = [ 9.000, dragon_walk10 ] { self.th_missile = dragon_atk_e1; dragon_move (17.000); self.dragonPainSequence = 5; };
void () dragon_walk10 = [ 10.000, dragon_walk11 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk11 = [ 11.000, dragon_walk12 ] { self.th_missile = dragon_atk_f1; dragon_move (17.000); self.dragonPainSequence = 6; };
void () dragon_walk12 = [ 12.000, dragon_walk13 ] { self.th_missile = SUB_Null; dragon_move (17.000); };
void () dragon_walk13 = [ 13.000, dragon_walk1 ] { self.th_missile = SUB_Null; dragon_move (17.000); self.dragonPainSequence = 1; };

void () FireballTouch = {
   local vector org;
   local vector rndpos;
   local float partcnt;
   local float partcolor;

   if (other == self.owner)
      return ;

   org = self.origin;
   org_z = (org_z+8);
   T_Damage (other,self,self.owner,3,DAMARMOR);
   sound (self,CHAN_WEAPON,"misc/quakeend.wav",TRUE,ATTN_NORM);
   partcnt = 40;
   partcolor = 232;
   while ( partcnt > FALSE ) {
      rndpos = org;
      rndpos_x = (rndpos_x + (random () * 32) );
      rndpos_y = (rndpos_y + (random () * 32) );
      rndpos_z = (rndpos_z + (random () * 32) );
      if ( (partcnt < 16) ) {
         rndpos_z = (rndpos_z + 4);
         partcolor = 228;
      }
      if ( (partcnt < 8) ) {
         rndpos_z = (rndpos_z + 8);
         partcolor = 224;
      }
      particle (rndpos,VEC_ORIGIN,partcolor,3);
      partcnt = (partcnt - TRUE);
   }
   remove (self);
};

void (vector org, vector dir) launch_fireball = {
   local entity fireball;
   local float fireSpeed;

   self.effects = (self.effects | EF_MUZZLEFLASH);
   fireball = spawn ();
   fireball.owner = self;
   fireball.movetype = MOVETYPE_FLYMISSILE;
   fireball.solid = SOLID_BBOX;
   fireball.classname = "fireball";
   setmodel (fireball,"progs/k_spike.mdl");
   setorigin (fireball,org);
   setsize (fireball,VEC_ORIGIN,VEC_ORIGIN);
   fireSpeed = ((random () * 400.000) + 200.000);
   fireball.velocity = (dir * fireSpeed);
   fireball.avelocity = '0.000 0.000 300.000';
   fireball.angles = vectoangles (fireball.velocity);
   fireball.enemy = self.enemy;
   fireball.touch = FireballTouch;
   fireball.nextthink = (time + MOVETYPE_TOSS);
   fireball.think = SUB_Remove;
};

void () dragon_fireball = {
   local float fireCount;
   local vector org;
   local vector dir;
   local float distortion;
   local float vdistortion;

   sound (self,CHAN_VOICE,"dragon/active.wav",TRUE,ATTN_NORM);
   makevectors (self.angles);
   org = ((self.origin + (v_forward * 112.000)) + (v_up * FL_MONSTER));
   dir = normalize ((self.enemy.origin - org));
   particle (org,VEC_ORIGIN,224,32);
   if ( (random () > 0.5) )
      fireCount = 30;
   else
      fireCount = 40;

   while ( (fireCount > FALSE) ) {
      if ( (random () > 0.8) )
         distortion = ((random () - 0.500) * 0.800);
      else
         distortion = ((random () - 0.500) * 0.250);

      if ( (random () > 0.8) )
         vdistortion = ((random () - 0.500) * 0.800);
      else
         vdistortion = ((random () - 0.500) * 0.250);

      dir = normalize ((self.enemy.origin - org));
      makevectors (dir);
      dir = (dir + (v_right * distortion));
      dir = (dir + (v_up * vdistortion));
      launch_fireball (org,dir);
      fireCount = (fireCount - TRUE);
   }
};

void () dragon_tail_touch = {
   local vector delta;
   local float tailDamage;

   if (!self.enemy)
      return ;

   if (!CanDamage(self.enemy,self))
      return ;

   ai_charge (10);
   delta = (self.enemy.origin - self.origin);
   tailDamage = vlen (delta);
   if ( (vlen (delta) > 150.000) )
      return ;

   tailDamage = ((random () * 30) + 15);
   T_Damage (self.enemy,self,self,tailDamage,DAMARMOR);
};

void () dragon_tail = {
   local float dist;
   local vector dir;

   if (!self.enemy)
      return ;

   if ( !CanDamage (self.enemy,self) )
      return ;

   dragon_move (10);
   dir = (self.enemy.origin - self.origin);
   dist = vlen (dir);
   if (dist <= 150.000) {
      T_Damage (self.enemy,self,self,18,DAMARMOR);
      dir = normalize (dir);
      self.enemy.velocity = (dir * 500.000);
      self.enemy.velocity_z = 350.000;
   }
   dragon_stop_attack ();
};

void () dragon_melee1 = [ 20.000, dragon_melee2 ] { dragon_move (12); };
void () dragon_melee2 = [ 21.000, dragon_melee3 ] { dragon_move (12); };
void () dragon_melee3 = [ 22.000, dragon_melee4 ] { dragon_move (12); };
void () dragon_melee4 = [ 23.000, dragon_melee5 ] { dragon_move (12); };
void () dragon_melee5 = [ 24.000, dragon_melee6 ] { dragon_move (12); };
void () dragon_melee6 = [ 25.000, dragon_melee7 ] { dragon_move (12); };
void () dragon_melee7 = [ 26.000, dragon_melee8 ] { dragon_move (12); };
void () dragon_melee8 = [ 27.000, dragon_melee9 ] { dragon_move (12); };
void () dragon_melee9 = [ 28.000, dragon_melee10 ] { dragon_move (12); };
void () dragon_melee10 = [ 29.000, dragon_melee11 ] { dragon_tail (); };
void () dragon_melee11 = [ 30.000, dragon_melee12 ] { dragon_move (10); };
void () dragon_melee12 = [ 31.000, dragon_melee13 ] { dragon_move (10); };
void () dragon_melee13 = [ 32.000, dragon_walk1 ] { dragon_move (10); };

void () dragon_painA1 = [ 62.000, dragon_painA2 ] { dragon_move (12); };
void () dragon_painA2 = [ 63.000, dragon_painA3 ] { dragon_move (12); };
void () dragon_painA3 = [ 64.000, dragon_walk5 ] { dragon_move (12); };

void () dragon_painB1 = [ 65.000, dragon_painB2 ] { dragon_move (12); };
void () dragon_painB2 = [ 66.000, dragon_painB3 ] { dragon_move (12); };
void () dragon_painB3 = [ 67.000, dragon_walk7 ] { dragon_move (12); };

void () dragon_painC1 = [ 68.000, dragon_painC2 ] { dragon_move (12); };
void () dragon_painC2 = [ 69.000, dragon_painC3 ] { dragon_move (12); };
void () dragon_painC3 = [ 70.000, dragon_walk9 ] { dragon_move (12); };

void () dragon_painD1 = [ 71.000, dragon_painD2 ] { dragon_move (12); };
void () dragon_painD2 = [ 72.000, dragon_painD3 ] { dragon_move (12); };
void () dragon_painD3 = [ 73.000, dragon_walk11 ] { dragon_move (12); };

void () dragon_painE1 = [ 74.000, dragon_painE2 ] { dragon_move (12); };
void () dragon_painE2 = [ 75.000, dragon_painE3 ] { dragon_move (12); };
void () dragon_painE3 = [ 76.000, dragon_walk13 ] { dragon_move (12); };

void () dragon_painF1 = [ 77.000, dragon_painF2 ] { dragon_move (12); };
void () dragon_painF2 = [ 78.000, dragon_painF3 ] { dragon_move (12); };
void () dragon_painF3 = [ 79.000, dragon_walk2 ] { dragon_move (12); };

void (entity inflictor, entity attacker, float damage) dragon_pain = {
   local float r;

   if (self.pain_finished > time)
      return ;

   r = random ();
   if ( (r < 0.250) ) {
      dragon_stop_attack ();
      sound (self,CHAN_VOICE,"dragon/pain.wav",TRUE,ATTN_NORM);
      self.pain_finished = (time + 2);
      if ( (self.dragonPainSequence == 1) ) {
         self.think = dragon_painA1;
      } else {
         if ( (self.dragonPainSequence == 2) ) {
            self.think = dragon_painF1;
         } else {
            if ( (self.dragonPainSequence == 3) ) {
               self.think = dragon_painE1;
            } else {
               if ( (self.dragonPainSequence == 4) ) {
                  self.think = dragon_painD1;
               } else {
                  if ( (self.dragonPainSequence == 5) ) {
                     self.think = dragon_painC1;
                  } else {
                     if ( (self.dragonPainSequence == 6) )
                        self.think = dragon_painB1;

                  }
               }
            }
         }
      }
   }
};

void () dragon_boom2 = {
   self.velocity = self.dragonLastVelocity;
   ViolentDeath (15);
   self.target = "dragondoor";
   SUB_UseTargets ();
   remove (self);
};

void () dragon_boom1 = {
   if (self.dragonDeathState > DDIE_STOPPED)
      return ;

   self.dragonDeathState = DDIE_EXPLODED;
   ThrowGib (11,1);
   ThrowGib (12,1);
   ThrowGib (13,1);
   sound (self,CHAN_BODY,"player/tornoff2.wav",TRUE,ATTN_NONE);
   self.think = dragon_boom2;
   self.nextthink = (time + 0.100);
};

void () dragon_explode = {
   if (self.dragonDeathState > DDIE_FALLING)
      return ;

   if ( ((vlen (self.velocity) < 100.000) || (self.flags & FL_INWATER)) ) {
      self.dragonDeathState = DDIE_STOPPED;
      dragon_boom1 ();
   } else {
      makevectors (self.angles);
      self.velocity = (self.velocity - (40 * v_up));
      self.dragonLastVelocity = self.velocity;
   }
};

void () dragon_squish = {
   if ( (other.classname == "player") ) {
      self.classname = "monster_dragon_dead";
        self.deathstring = " was squashed by the Dragon\n";
      T_Damage (other,self,self,200.000,DAMARMOR);
   }
   if ( (other == world) ) {
      self.velocity = VEC_ORIGIN;
      dragon_explode ();
   }
};

void () dragon_death1 = [ 80.000, dragon_death2 ] {
   if ( (self.dragonDeathState > FALSE) )
      return ;

   self.dragonDeathState = DDIE_FALLING;
   self.use = SUB_Null;
   makevectors (self.angles);
   self.velocity = ((300.000 * v_forward) - (40 * v_up));
   self.flags = (self.flags - (self.flags & FL_ONGROUND));
   setsize (self,VEC_HULL_MIN,VEC_HULL_MAX);
   self.touch = dragon_squish;
   sound (self,CHAN_VOICE,"dragon/death.wav",TRUE,ATTN_NONE);
   self.dragonAttacking = FALSE;
};
void () dragon_death2 = [ 81.000, dragon_death3 ] { dragon_explode (); };
void () dragon_death3 = [ 82.000, dragon_death4 ] { dragon_explode (); };
void () dragon_death4 = [ 83.000, dragon_death5 ] { dragon_explode (); };
void () dragon_death5 = [ 84.000, dragon_death6 ] { dragon_explode (); };
void () dragon_death6 = [ 85.000, dragon_death7 ] { dragon_explode (); };
void () dragon_death7 = [ 86.000, dragon_death8 ] { dragon_explode (); };
void () dragon_death8 = [ 87.000, dragon_death9 ] { dragon_explode (); };
void () dragon_death9 = [ 88.000, dragon_death10 ] { dragon_explode (); };
void () dragon_death10 = [ 89.000, dragon_death11 ] { dragon_explode (); };
void () dragon_death11 = [ 90.000, dragon_death12 ] { dragon_explode (); };
void () dragon_death12 = [ 91.000, dragon_death13 ] { dragon_explode (); };
void () dragon_death13 = [ 92.000, dragon_death14 ] { dragon_explode (); };
void () dragon_death14 = [ 93.000, dragon_death15 ] { dragon_explode (); };
void () dragon_death15 = [ 94.000, dragon_death16 ] { dragon_explode (); };
void () dragon_death16 = [ 95.000, dragon_death17 ] { dragon_explode (); };
void () dragon_death17 = [ 96.000, dragon_death18 ] { dragon_explode (); };
void () dragon_death18 = [ 97.000, dragon_death19 ] { dragon_explode (); };
void () dragon_death19 = [ 98.000, dragon_death20 ] { dragon_explode (); };
void () dragon_death20 = [ 99.000, dragon_death21 ] { dragon_explode (); };
void () dragon_death21 = [ 100.000, dragon_death21 ] { ViolentDeath (39.000); SUB_UseTargets (); remove (self); };

void () dragon_use = {
   if ( (self.health < TRUE) ) { self.use = SUB_Null; return ; }

   self.th_stand = dragon_walk1;
   self.th_walk = dragon_walk1;
   self.think = dragon_walk1;
   self.nextthink = (time + 0.100);
};

void () dragon_activate = {
   self.takedamage = DAMAGE_AIM;
   self.ideal_yaw = (self.angles * '0.000 1.000 0.000');
   if ( !self.yaw_speed )
      self.yaw_speed = 10;  // Slow turner

   self.view_ofs = '0.000 0.000 25.000';
   self.flags = (self.flags | FL_FLY);
   self.flags = (self.flags | FL_MONSTER);
   if ( !walkmove (FALSE,FALSE) ) {
      dprint ("flymonster in wall!\n");
      dprint (self.classname);
      dprint (" at ");
      dprint (vtos (self.origin));
      dprint ("\n");
   }
   if ( self.target ) {
      self.movetarget = find (world,targetname,self.target);
      self.goalentity = self.movetarget;
      if ( !self.movetarget ) {
         dprint ("Monster can't find target at ");
         dprint (vtos (self.origin));
         dprint ("\n");
      }
   }
   if ( self.targetname )
      self.use = dragon_use;
   else
      dragon_use ();
};

void () monster_dragon = {
   if ( deathmatch ) { remove (self); return ; }

   self.mdl = "progs/drake/dragon.mdl";
   precache_model ("progs/drake/dragon.mdl");
   precache_model ("progs/quoth2pt2full/fireball.mdl");
   self.gib1mdl = "progs/drake/drggib01.mdl";
   self.gib2mdl = "progs/drake/drggib02.mdl";
   self.gib3mdl = "progs/drake/drggib03.mdl";
   precache_model ("progs/drake/drggib01.mdl");
   precache_model ("progs/drake/drggib02.mdl");
   precache_model ("progs/drake/drggib03.mdl");
   precache_sound ("dragon/see.wav");
   precache_sound ("dragon/active.wav");
   precache_sound ("dragon/attack.wav");
   precache_sound ("misc/quakeend.wav");
   precache_sound ("misc/quakeend.wav");
   precache_sound ("dragon/pain.wav");
   precache_sound ("dragon/death.wav");
   self.dragonInRoom = DT_ROOM_A;
   self.dragonInTransit = FALSE;
   self.dragonAttacking = FALSE;
   self.playerInRoom = DT_ROOM_A;
   self.playerInTransit = FALSE;
   self.solid = SOLID_SLIDEBOX;
   self.movetype = MOVETYPE_STEP;
   setmodel (self,"progs/drake/dragon.mdl");
   setsize (self,VEC_HULL2_MIN,VEC_HULL2_MAX);
   if (self.health < 1) self.health = 4000;
   if (self.target == "") self.target = "dragondoor"; 
   self.th_run = dragon_walk1;
   self.th_pain = dragon_pain;
   self.th_die = dragon_death1;
   self.dragonPainSequence = TRUE;
   total_monsters = (total_monsters + TRUE);
   self.nextthink = (self.nextthink + 0.100);
    if (self.deathstring == "")  self.deathstring = " was annihilated by the Dragon\n";
   self.think = dragon_activate;
};

void () dragon_corner_touch = {
   if (other.movetarget != self)
      return ;

   if (other.classname != "monster_dragon")
      return ;

   other.movetarget = find (world,targetname,self.target);
   other.goalentity = other.movetarget;
   other.target = self.target;
   if (!other.movetarget)
      objerror ("dragon_corner: no target found");
};

void () dragon_corner = {
   if ( !self.targetname )
      objerror ("dragon_corner: no targetname");

   if ( !self.target )
      objerror ("dragon_corner: no target");

   self.solid = SOLID_TRIGGER;
   self.movetype = MOVETYPE_NONE;
   self.touch = dragon_corner_touch;
   self.modelindex = FALSE;
   self.model = "";
   setsize (self,'-16.000 -16.000 -16.000','16.000 16.000 16.000');
};