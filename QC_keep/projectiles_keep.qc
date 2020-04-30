void (vector loc, float rad, float damage, float dur, float pause, float vol) multi_explosion;

void() big_explosion_think = {
    self.frame = self.frame + 1;
    if (self.frame > 16)
        SUB_Remove();

    self.nextthink = time + 0.05;
    self.think = big_explosion_think;
};

void(vector org, float snd) spawn_big_explosion = {
    local entity e;

    e = spawn();
    e.movetype = MOVETYPE_NONE;
    e.solid = SOLID_NOT;
    setmodel(e, "progs/bigexp.spr");
    setorigin(e, org);
    if (snd == 1)
        sound(e, CHAN_AUTO, "weapons/expl1.wav", 0.2, ATTN_NORM);

    e.alpha = 0.6;
    e.effects = 32;
    e.nextthink = time + 0.05;
    e.think = big_explosion_think;
};

void() small_explosion_think = {
    self.frame = self.frame + 1;
    if (self.frame > 13)
        SUB_Remove();

    self.nextthink = time + 0.05;
    self.think = small_explosion_think;
};

void(vector org, float snd) spawn_small_explosion = {
    local entity e;

    e = spawn();
    e.movetype = MOVETYPE_NONE;
    e.solid = SOLID_NOT;
    setmodel(e, "progs/smlexp.spr");
    setorigin(e, org);
    if (snd == 1)
        sound(e, CHAN_AUTO, "weapons/expl1.wav", 0.2, ATTN_NORM);

    e.alpha = 0.6;
    e.effects = 32;
    e.nextthink = time + 0.05;
    e.think = small_explosion_think;
};

//----------Hipnotic weapon functions-----------------------------------
void () HIP_LaserTouch = {
   local vector org;
   local vector spot1;
   local vector spot2;
   local vector oldvel;
   local float r;

   self.owner = world;
   self.cnt = (self.cnt + TRUE);
   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }
   oldvel = normalize (self.old_velocity);
   spot1 = (self.origin - (FL_INWATER * oldvel));
   spot2 = (self.origin + (FL_INWATER * oldvel));
   traceline (spot1,spot2,FALSE,self);
   self.origin = trace_endpos;
   org = self.origin;
   if ( other.health ) {
      if ( (self.lastvictim == other) ) {
         self.dmg = (self.dmg / FL_SWIM);
      }
      spawn_touchblood (self, other, self.dmg);
      T_Damage (other,self,self.lastvictim,self.dmg,DAMARMOR);
   } else {
      if ( ((self.cnt == 3) || (random () < 0.150)) ) {
         WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
         WriteByte (MSG_BROADCAST,TE_GUNSHOT);
         WriteCoord (MSG_BROADCAST,org_x);
         WriteCoord (MSG_BROADCAST,org_y);
         WriteCoord (MSG_BROADCAST,org_z);
      } else {
         self.dmg = (0.900 * self.dmg);
         self.velocity = (oldvel + (FL_SWIM * trace_plane_normal));
         self.velocity = normalize (self.velocity);
         self.velocity = (self.speed * self.velocity);
         self.old_velocity = self.velocity;
         if ( (self.flags & FL_ONGROUND) )
            self.flags = (self.flags - FL_ONGROUND);

         r = random ();
         sound (self,CHAN_WEAPON,"hipweap/laserric.wav",TRUE,ATTN_STATIC);
         return ;
      }
   }
   sound (self,CHAN_WEAPON,"enforcer/enfstop.wav",TRUE,ATTN_STATIC);
   remove (self);
};

void () HIP_LaserThink = {
   if ( (time > self.attack_finished) ) { remove (self); return; }
   if ( (self.flags & FL_ONGROUND) )
      self.flags = (self.flags - FL_ONGROUND);

   self.velocity = self.old_velocity;
   self.angles = vectoangles (self.velocity);
   self.nextthink = (time + 0.100);
};

void (vector org, vector vec, float light) HIP_LaunchLaser = {
    if (self.perms | IT_BLASTER && self.weapon == IT_SHOTGUN) {
        sound (self,CHAN_WEAPON,"enforcer/enfire.wav",TRUE,ATTN_NORM);
    } else {
        sound (self,CHAN_WEAPON,"hipweap/laserg.wav",TRUE,ATTN_NORM);
    }
   vec = normalize (vec);
   newmis = spawn ();
   newmis.owner = self;
   newmis.classname = "hiplaser";
   newmis.lastvictim = self;
   newmis.movetype = MOVETYPE_FLYMISSILE;
   newmis.solid = SOLID_BBOX;
   if ( light )
      newmis.effects = EF_DIMLIGHT;

    if (self.perms | IT_BLASTER && self.weapon == IT_SHOTGUN) {
        setmodel (newmis,"progs/ad171/proj_laz.mdl");
    } else {
        setmodel (newmis,"progs/hipnotic/lasrspik.mdl");
    }
   setsize (newmis,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (newmis,org);
   newmis.speed = 1000.000;
   newmis.dmg = 18.000;
    if (self.perms | IT_BLASTER && self.weapon == IT_SHOTGUN) newmis.dmg = 24; // slight boost for blaster to at least match AD shotgun boost
   newmis.velocity = (vec * newmis.speed);
   newmis.old_velocity = newmis.velocity;
   newmis.angles = vectoangles (newmis.velocity);
   newmis.avelocity = '0.000 0.000 400.000';
   newmis.nextthink = time;
   newmis.attack_finished = (time + MOVETYPE_FLY);
   newmis.think = HIP_LaserThink;
   newmis.touch = HIP_LaserTouch;
   newmis.count = FALSE;
};

void (vector p1, vector p2, entity from, float damage) HIP_HammerDamage = {
   local entity e1;
   local entity e2;
   local vector f;

   f = (p2 - p1);
   normalize (f);
   f_x = (FALSE - f_y);
   f_y = f_x;
   f_z = FALSE;
   f = (f * FL_INWATER);
   e2 = world;
   e1 = world;
   traceline (p1,p2,FALSE,self);
   if ( (trace_ent.takedamage && !trace_ent.wetsuit_finished) ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * MOVETYPE_STEP));
      T_Damage (trace_ent,from,from,damage,DAMARMOR);
      if ( (self.classname == "player") ) {
         if ( (other.classname == "player") )
            trace_ent.velocity_z = (trace_ent.velocity_z + 400.000);
      }
   }
   e1 = trace_ent;
   traceline ((p1 + f),(p2 + f),FALSE,self);
   if ( (((trace_ent != e1) && trace_ent.takedamage) && !trace_ent.wetsuit_finished) ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * MOVETYPE_STEP));
      T_Damage (trace_ent,from,from,damage, DAMARMOR);
   }
   e2 = trace_ent;
   traceline ((p1 - f),(p2 - f),FALSE,self);
   if ( ((((trace_ent != e1) && (trace_ent != e2)) && trace_ent.takedamage) && !trace_ent.wetsuit_finished) ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * MOVETYPE_STEP));
      T_Damage (trace_ent,from,from,damage, DAMARMOR);
   }
};

void () HIP_LightningThink = {
   local vector org;
   local vector dst;
   local entity head;
   local entity selected;
   local float cur_dist;
   local float head_dist;
   local vector vec;
   local float dot;
   local float oldstate;
   local float dam;
   local vector end;

   if ( (time > self.delay) ) {
      if ( (self.enemy != world) )
         self.enemy.struck_by_mjolnir = FALSE;

      remove (self);
      return ;
   }
   oldstate = self.state;

   // Multi-lightning effect of Mjolnir
   if ( (self.state == FALSE) ) {
      self.enemy = world;
      selected = world;
      cur_dist = self.distance;
      head = findradius (self.owner.origin,self.distance);
      while ( head ) {
         if ( (!(head.flags & FL_NOTARGET) && ((head.flags & FL_MONSTER) || (head.flags & FL_CLIENT))) ) {
            visible (head);
            if ( ((visible (head) && (head != self.owner.owner)) && (head.health > FALSE)) ) {
               head_dist = vlen ((head.origin - self.lastvictim.origin));
               if ( ((head_dist < cur_dist) && (head.struck_by_mjolnir == FALSE)) ) {
                  selected = head;
                  cur_dist = head_dist;
               }
            }
         }
         head = head.chain;
      }
      if ( (selected != world) ) {
         self.state = TRUE;
         self.enemy = selected;
         self.enemy.struck_by_mjolnir = TRUE;
      } else {
         makevectors (self.v_angle);
         org = self.owner.origin;
         end = (org + (v_forward * 200));
         end = (end + (((400.000 * random ()) - 200) * v_right));
         traceline (org,end,TRUE,self);
         WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
         WriteByte (MSG_BROADCAST,TE_LIGHTNING2);
         WriteEntity (MSG_BROADCAST,self);
         WriteCoord (MSG_BROADCAST,org_x);
         WriteCoord (MSG_BROADCAST,org_y);
         WriteCoord (MSG_BROADCAST,org_z);
         WriteCoord (MSG_BROADCAST,trace_endpos_x);
         WriteCoord (MSG_BROADCAST,trace_endpos_y);
         WriteCoord (MSG_BROADCAST,trace_endpos_z);
         self.nextthink = (time + 0.100);
         return ;
      }
   }
   org = self.lastvictim.origin;
   dst = (self.enemy.absmin + (0.250 * (self.enemy.absmax - self.enemy.absmin)));
   dst = (dst + ((random () * 0.500) * (self.enemy.absmax - self.enemy.absmin)));
   traceline (org,dst,TRUE,self.owner.owner);
   if ( ((trace_fraction != TRUE) || (self.enemy.health <= FALSE)) ) {
      self.enemy.struck_by_mjolnir = FALSE;
      self.state = FALSE;
      self.nextthink = (time + 0.100);
      return ;
   }
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_LIGHTNING2);
   WriteEntity (MSG_BROADCAST,self);
   WriteCoord (MSG_BROADCAST,org_x);
   WriteCoord (MSG_BROADCAST,org_y);
   WriteCoord (MSG_BROADCAST,org_z);
   WriteCoord (MSG_BROADCAST,trace_endpos_x);
   WriteCoord (MSG_BROADCAST,trace_endpos_y);
   WriteCoord (MSG_BROADCAST,trace_endpos_z);
   vec = normalize ((self.enemy.origin - self.owner.origin));
   dot = (vec * self.owner.movedir);
   head = self;
   self = self.owner.owner;
   if ( (oldstate == FALSE) ) {
      dam = 80;
   } else {
      dam = 30;
   }
   if ( (dot > 0.300) ) {
      HIP_HammerDamage (org,trace_endpos,self,dam);
   } else {
      HIP_HammerDamage (org,trace_endpos,self,(dam * 0.500));
   }
   self = head;
   self.nextthink = (time + 0.200);
};

void (entity prev, entity own, float dst) HIP_SpawnMjolnirLightning = {
   local entity light;

   light = spawn ();
   light.delay = (time + 0.800);
   light.state = FALSE;
   light.lastvictim = prev;
   light.distance = dst;
   light.owner = own;
   light.v_angle = self.angles;
   light.v_angle_x = FALSE;
   light.v_angle_z = FALSE;
   light.origin = own.origin;
   light.think = HIP_LightningThink;
   light.nextthink = time;
};

void () HIP_SpawnMjolnirBase = {
   local entity light;

   light = spawn ();
   light.origin = trace_endpos;
   light.flags = FALSE;
   light.owner = self;
   light.struck_by_mjolnir = TRUE;
   light.think = SUB_Remove;
   light.nextthink = (time + TRUE);
   sound (light,CHAN_AUTO,"hipweap/mjolslap.wav",TRUE,ATTN_NORM);
   sound (light,CHAN_WEAPON,"hipweap/mjolhit.wav",TRUE,ATTN_NORM);
   makevectors (self.v_angle);
   light.movedir = v_forward;
   HIP_SpawnMjolnirLightning (light,light,350.000);
   HIP_SpawnMjolnirLightning (light,light,350.000);
   HIP_SpawnMjolnirLightning (light,light,350.000);
   HIP_SpawnMjolnirLightning (light,light,350.000);
};

void () HIP_FireMjolnirLightning = {
   local float cells;

   if ( (self.waterlevel > TRUE) ) {
      cells = self.ammo_cells;
      self.ammo_cells = FALSE;
      discharged = TRUE;
      T_RadiusDamage (self,self,(35.000 * cells),world, DAMAGEALL);
      discharged = FALSE;
      W_SetCurrentAmmo (self);
      return ;
   }
   self.ammo_cells = (self.ammo_cells - 15.000);
   self.currentammo = (self.currentammo - 15.000);
   HIP_SpawnMjolnirBase ();
};

void () ProximityExplode = {
   T_RadiusDamage (self,self.owner,95.000,world, DAMAGEALL);
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   BecomeExplosion ();
};

void () ProximityGrenadeExplode = {
   self.takedamage = DAMAGE_NO;
   NumProximityGrenades = (NumProximityGrenades - TRUE);
   self.deathtype = "exploding";
   self.nextthink = (time + 0.100);
   self.owner = self.lastvictim;
   self.think = ProximityExplode;
};

void () ProximityGrenadeTouch = {
   if ( (other == self) )
      return ;

   if ( (other.classname == self.classname) )
      return ;

   self.movetype = MOVETYPE_TOSS;
   if ( (self.state == TRUE) )
      return ;

   if ( (vlen (other.velocity) > FALSE) ) {
      ProximityGrenadeExplode ();
      self.think ();
      return ;
   }
   if ( (other.takedamage == DAMAGE_AIM) ) {
      ProximityGrenadeExplode ();
      self.think ();
      return ;
   }
   sound (self,CHAN_WEAPON,"weapons/bounce.wav",TRUE,ATTN_NORM);
   self.movetype = MOVETYPE_NONE;
   setsize (self,'-8.000 -8.000 -8.000','8.000 8.000 8.000');
   self.state = TRUE;
   self.spawnmaster = other;
};

void () ProximityBomb = {
   local entity head;
   local float blowup;

   if ( (((time > self.delay) || (NumProximityGrenades > 64.000)) || (vlen (self.spawnmaster.velocity) > FALSE)) ) {
      ProximityGrenadeExplode ();
      self.think ();
      return ;
   }
   self.owner = world;
   self.takedamage = DAMAGE_YES;
   head = findradius (self.origin,140.000);
   blowup = FALSE;
   while ( head ) {
      if ( ((((head != self) && (head.health > FALSE)) && (head.flags & (FL_CLIENT | FL_MONSTER))) && (head.classname != self.classname)) )
         blowup = TRUE;

      if ( ((head.classname == self.classname) && (head.state == FALSE)) )
         blowup = TRUE;

      traceline (self.origin,head.origin,TRUE,self);
      if ( (trace_fraction != TRUE) )
         blowup = FALSE;

      if ( (blowup == TRUE) ) {
         sound (self,CHAN_WEAPON,"hipweap/proxwarn.wav",TRUE,ATTN_NORM);
         ProximityGrenadeExplode ();
         self.nextthink = (time + 0.500);
         return;
      }
      head = head.chain;
   }
   self.nextthink = (time + 0.250);
};
//----------------------------------------------------------------------

//----------Rogue weapon functions--------------------------------------
void () lavaspike_touch = {
   local float old_armortype;
   local float old_armorvalue;
   local float old_armormask;

   if ( (other == self.owner) )
      return ;

   if ( (other.solid == SOLID_TRIGGER) )
      return ;

   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }
   if ( other.takedamage ) {
      spawn_touchblood (self, other, 9);
      if ( (other.classname == "player") ) {
         old_armortype = other.armortype;
         old_armorvalue = other.armorvalue;
         old_armormask = (other.items2 & ((IT2_ARMOR1 | IT2_ARMOR2) | IT2_ARMOR3));
         other.armortype = FALSE;
         other.armorvalue = FALSE;
         T_Damage (other,self,self.owner,9,DAMARMOR);
         other.armortype = old_armortype;
         other.armorvalue = old_armorvalue;
         other.items2 = (other.items2 | old_armormask);
      } else {
         if ( (other.classname != "monster_lava_man") )
            T_Damage (other,self,self.owner,15,DAMARMOR);
      }
   } else {
      WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
      if ( (self.classname == "wizspike") ) {
         WriteByte (MSG_BROADCAST,TE_WIZSPIKE);
      } else {
         if ( (self.classname == "knightspike") ) {
            WriteByte (MSG_BROADCAST,TE_KNIGHTSPIKE);
         } else {
            WriteByte (MSG_BROADCAST,TE_SPIKE);
         }
      }
      WriteCoord (MSG_BROADCAST,self.origin_x);
      WriteCoord (MSG_BROADCAST,self.origin_y);
      WriteCoord (MSG_BROADCAST,self.origin_z);
   }
   remove (self);
};

void () superlavaspike_touch = {
   local float old_armortype;

   if ( (other == self.owner) )
      return ;

   if ( (other.solid == SOLID_TRIGGER) )
      return ;

   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }
   if ( other.takedamage ) {
      spawn_touchblood (self, other, 18.000);
      if ( (other.classname == "player") ) {
         old_armortype = other.armortype;
         other.armortype = (other.armortype * 0.500);
         T_Damage (other,self,self.owner,18.000,DAMARMOR);
         if ( (other.armortype != FALSE) )
            other.armortype = old_armortype;
      } else {
         if ( (other.classname != "monster_lava_man") )
            T_Damage (other,self,self.owner,30,DAMARMOR);
      }
   } else {
      WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
      WriteByte (MSG_BROADCAST,TE_SUPERSPIKE);
      WriteCoord (MSG_BROADCAST,self.origin_x);
      WriteCoord (MSG_BROADCAST,self.origin_y);
      WriteCoord (MSG_BROADCAST,self.origin_z);
   }
   remove (self);
};

void (vector org, vector dir) launch_lava_spike = {
   newmis = spawn ();
   newmis.owner = self;
   newmis.movetype = MOVETYPE_FLYMISSILE;
   newmis.solid = SOLID_BBOX;
   newmis.angles = vectoangles (dir);
   newmis.touch = lavaspike_touch;
   newmis.classtype = CT_PROJ_LAVASPIK;
   newmis.think = SUB_Remove;
   newmis.nextthink = (time + MOVETYPE_TOSS);
   setmodel (newmis,"progs/lspike.mdl");
   setsize (newmis,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (newmis,org);
   newmis.velocity = (dir * 1000.000);
};

void () MiniGrenadeExplode = {
   if ( (self.owner.classname == "player") ) {
      T_RadiusDamage (self,self.owner,90.000,world,DAMAGEALL);
   } else {
      T_RadiusDamage (self,self.owner,60.000,world,DAMAGEALL);
   }
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   //WriteByte (MSG_BROADCAST,230.000);
   //WriteByte (MSG_BROADCAST,MOVETYPE_FLY);
   BecomeExplosion ();
};

void () MultiGrenadeTouch;

void (float offsetAngle) MiniGrenadeLaunch = {
   local entity missile;
   local float tempRand;

   missile = spawn ();
   missile.owner = self.owner;
   missile.movetype = MOVETYPE_BOUNCE;
   missile.solid = SOLID_BBOX;
   missile.classname = "MiniGrenade";
   missile.v_angle = self.v_angle;
   missile.v_angle_y = (missile.v_angle_y + offsetAngle);
   makevectors (missile.v_angle);
   missile.velocity = ((v_forward * 100.000) + (v_up * 400));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_forward));
   tempRand = ((crandom () * 40) - 20.000);
   missile.velocity = (missile.velocity + (tempRand * v_right));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_up));
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = MultiGrenadeTouch;
   setmodel (missile,"progs/rogue/mervup.mdl");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,self.origin);
   missile.nextthink = ((time + TRUE) + (crandom () * 0.500));
   missile.think = MiniGrenadeExplode;
};

void () MultiGrenadeExplode = {
   MiniGrenadeLaunch (FALSE);
   MiniGrenadeLaunch (72.000);
   MiniGrenadeLaunch (144.000);
   MiniGrenadeLaunch (216.000);
   MiniGrenadeLaunch (288.000);
   remove (self);
};

void () MultiGrenadeTouch = {
   if ( (other == self.owner) )
      return ;

   if ( (other.takedamage == DAMAGE_AIM) ) {
      if ( (self.classname == "MiniGrenade") ) {
         MiniGrenadeExplode ();
      } else {
         if ( (self.owner.classname == "player") ) {
            GrenadeExplode ();
         } else {
            MiniGrenadeExplode ();
         }
      }
      return ;
   }
   sound (self,CHAN_WEAPON,"weapons/bounce.wav",TRUE,ATTN_NORM);
   if ( (self.velocity == VEC_ORIGIN) )
      self.avelocity = VEC_ORIGIN;
};

void () MultiRocketExplode = {
   local float damg;

   if (self.owner.classtype == CT_MONGREMLIN) {
      damg = 30 + random()*10;
   } else {
      damg = (60.000 + (random () * 15));
   }
   if ( other.health )
      T_Damage (other,self,self.owner,damg,DAMARMOR);

   T_RadiusDamage (self,self.owner,75.000,other,DAMAGEALL);
   self.origin = (self.origin - (FL_CLIENT * normalize (self.velocity)));
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   BecomeExplosion ();
};

void () MultiRocketTouch = {
   if ( (other == self.owner) )
      return ;

   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }

   MultiRocketExplode ();
};

void () HomingMissileThink = {
   local vector dir;

   if ( (self.enemy.health < TRUE) ) {
      remove (self);
      return ;
   }
   dir = normalize ((self.enemy.origin - self.origin));
   self.velocity = (dir * 1000.000);
   self.nextthink = (time + 0.100);
   self.think = HomingMissileThink;
};

void () HomingMissileAcquire = {
   local vector oldVelocity;
   local vector aimangle;

   if ( (self.delay < time) ) {
      MultiRocketExplode ();
      return ;
   }
   oldVelocity = self.velocity;
   makevectors (self.v_angle);
   self.velocity = aim (self,1000.000);
   self.velocity = (self.velocity * 1000.000);
   aimangle = (self.origin + self.velocity);
   traceline (self.origin,aimangle,FALSE,self);
   if ( (trace_fraction < TRUE) ) {
      if ( (trace_ent.flags & FL_MONSTER) ) {
         self.enemy = trace_ent;
         HomingMissileThink ();
         return ;
      }
   }
   self.velocity = oldVelocity;
   self.v_angle = vectoangles (self.velocity);
   self.angles = self.v_angle;
   self.think = HomingMissileAcquire;
   self.nextthink = (time + 0.200);
};

void (float offset, float frameNum) MultiRocketLaunch = {
   local entity missile;
   local vector aimangle;

   missile = spawn ();
   missile.owner = self;
   missile.movetype = MOVETYPE_FLYMISSILE;
   missile.solid = SOLID_BBOX;
   missile.classname = "MultiRocket";
   missile.delay = (time + MOVETYPE_STEP);
   missile.frame = frameNum;
   missile.touch = MultiRocketTouch;
   if ( (deathmatch || coop) ) {
      setmodel (missile,"progs/rockup_d.mdl");
   } else {
      setmodel (missile,"progs/rockup.mdl");
   }
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,((self.origin + (v_forward * FL_CLIENT)) + '0.000 0.000 16.000'));
   if ( (coop || deathmatch) ) {
      aimangle = self.v_angle;
      aimangle_y = (aimangle_y + (offset * 0.660));
      makevectors (aimangle);
      missile.velocity = aim (self,1000.000);
      missile.velocity = (missile.velocity * 1000.000);
      missile.angles = vectoangles (missile.velocity);
      missile.think = MultiRocketExplode;
      missile.nextthink = (time + 4);
   } else {
      makevectors (self.v_angle);
      missile.velocity = ((v_forward * 1000.000) - ((v_right * offset) * FL_CLIENT));
      missile.angles = vectoangles (missile.velocity);
      missile.v_angle = self.v_angle;
      aimangle = (missile.origin + missile.velocity);
      traceline (missile.origin,aimangle,FALSE,self);
      if ( (trace_fraction < TRUE) ) {
         if ( (trace_ent.flags & FL_MONSTER) ) {
            missile.enemy = trace_ent;
            missile.think = HomingMissileThink;
            return ;
         }
      }
      missile.think = HomingMissileAcquire;
      missile.nextthink = (time + 0.100);
   }
};

void (vector p1, vector p2, entity from, float damage) PlasmaDamage = {
   local entity e1;
   local entity e2;
   local vector f;

   f = (p2 - p1);
   normalize (f);
   f_x = (FALSE - f_y);
   f_y = f_x;
   f_z = FALSE;
   f = (f * FL_INWATER);
   e2 = world;
   e1 = world;
   traceline (p1,p2,FALSE,self);
   if ( trace_ent.takedamage ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * 4));
      T_Damage (trace_ent,from,from.owner,damage,DAMARMOR);
      if ( (self.classname == "player") ) {
         if ( (other.classname == "player") )
            trace_ent.velocity_z = (trace_ent.velocity_z + 400);
      }
   }
   e1 = trace_ent;
   traceline ((p1 + f),(p2 + f),FALSE,self);
   if ( ((trace_ent != e1) && trace_ent.takedamage) ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * 4));
      T_Damage (trace_ent,from,from.owner,damage,DAMARMOR);
   }
   e2 = trace_ent;
   traceline ((p1 - f),(p2 - f),FALSE,self);
   if ( (((trace_ent != e1) && (trace_ent != e2)) && trace_ent.takedamage) ) {
      particle (trace_endpos,'0.000 0.000 100.000',225.000,(damage * 4));
      T_Damage (trace_ent,from,from.owner,damage,DAMARMOR);
   }
};

void (entity current, float doDamage) PlasmaDischarge = {
    float dmg;
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_LIGHTNING2);
   WriteEntity (MSG_BROADCAST,current);
   WriteCoord (MSG_BROADCAST,current.origin_x);
   WriteCoord (MSG_BROADCAST,current.origin_y);
   WriteCoord (MSG_BROADCAST,current.origin_z);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   sound (self,CHAN_VOICE,"weapons/lhit.wav",TRUE,ATTN_NORM);
   if ( (doDamage == TRUE) ) {
    if (self.owner.classtype == CT_MONGREMLIN) {
        dmg = 20;
    } else {
        dmg = 50;
    }
      PlasmaDamage (self.origin,current.origin,self,dmg);
   }
};

void () PlasmaGroundOut = {
   local entity current;
   local entity start;
   local float monstersHit;

   monstersHit = FALSE;
   current = findradius (self.origin,320.000);
   start = current;
   while ( (monstersHit < 5) ) {
      if ( ((current.flags & FL_MONSTER) || (current.classname == "player")) ) {
         if ( (current != self.owner) ) {
            traceline (self.origin,current.origin,TRUE,world);
            if ( (trace_fraction == TRUE) ) {
               monstersHit = (monstersHit + TRUE);
               PlasmaDischarge (current,TRUE);
            }
         }
      }
      current = current.chain;
      if ( ((start == current) || !current) )
         return ;
   }
};

void () PlasmaTouch = {
   local float damg,raddmg;

   if ( (other == self.owner) )
      return ;

   if ( (pointcontents (self.origin) == CONTENT_SKY) ) {
      remove (self);
      return ;
   }
   damg = (80.000 + (random () * 20.000));
   raddmg = 70;
   if (self.owner.classtype == CT_MONGREMLIN) { damg = damg * 0.5; raddmg = 35;}
   sound (self,CHAN_WEAPON,"plasma/explode.wav",TRUE,ATTN_NORM);
   if ( other.health ) {
      T_Damage (other,self,self.owner,damg,DAMARMOR);
   }

   
   T_RadiusDamage (self,self.owner,raddmg,other,DAMAGEALL);
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_EXPLOSION2);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   WriteByte (MSG_BROADCAST,244.000);
   WriteByte (MSG_BROADCAST,MOVETYPE_WALK);
   PlasmaGroundOut ();
   remove (self);
};
//----------------------------------------------------------------------

void () s_firballexplode5 = [ 5.000, s_firballexplode6 ] {};
void () s_firballexplode6 = [ 6.000, s_firballexplode7 ] {};
void () s_firballexplode7 = [ 7.000, s_firballexplode8 ] {};
void () s_firballexplode8 = [ 8.000, s_firballexplode9 ] {};
void () s_firballexplode9 = [ 9.000, s_firballexplode10 ] {};
void () s_firballexplode10 = [ 10.000, s_firballexplode11 ] {};
void () s_firballexplode11 = [ 11.000, s_firballexplode12 ] {};
void () s_firballexplode12 = [ 12.000, s_firballexplode13 ] {};
void () s_firballexplode13 = [ 13.000, SUB_Remove ] {};

void () s_firespurtexplode1 = [ 1.000, s_firespurtexplode2 ] {};
void () s_firespurtexplode2 = [ 2.000, s_firespurtexplode3 ] {};
void () s_firespurtexplode3 = [ 3.000, s_firespurtexplode4 ] {};
void () s_firespurtexplode4 = [ 4.000, s_firespurtexplode5 ] {};
void () s_firespurtexplode5 = [ 5.000, s_firespurtexplode6 ] {};
void () s_firespurtexplode6 = [ 6.000, s_firespurtexplode7 ] {};
void () s_firespurtexplode7 = [ 7.000, s_firespurtexplode8 ] {};
void () s_firespurtexplode8 = [ 8.000, s_firespurtexplode9 ] {};
void () s_firespurtexplode9 = [ 9.000, s_firespurtexplode10 ] {};
void () s_firespurtexplode10 = [ 10.000, s_firespurtexplode11 ] {};
void () s_firespurtexplode11 = [ 11.000, s_firespurtexplode12 ] {};
void () s_firespurtexplode12 = [ 12.000, s_firespurtexplode13 ] {};
void () s_firespurtexplode13 = [ 13.000, SUB_Remove ] {};

void () BecomeFireSpurt = {
   T_RadiusDamage (self,self.owner,40,world,DAMAGEALL);
   self.touch = SUB_Null;
   setmodel (self,"progs/medexp.spr");
   sound (self,CHAN_WEAPON,"pyro/fstop.wav",TRUE,ATTN_NORM);
   s_firespurtexplode1();
};

void () BecomeFireBall = {
   T_RadiusDamage (self,self.owner,40,world,DAMAGEALL);
   self.touch = SUB_Null;
   setmodel (self,"progs/smlexp.spr");
   sound (self,CHAN_WEAPON,"pyro/fstop.wav",TRUE,ATTN_NORM);
   s_firballexplode5();
};

void () FireBallVerticalLaunch = {
   local entity missile;
   local float tempRand;

   missile = spawn ();
   missile.owner = self.owner;
   missile.movetype = MOVETYPE_BOUNCE;
   missile.solid = SOLID_BBOX;
   missile.classname = "Dragonfire";
   missile.v_angle = self.v_angle;
   missile.v_angle_z = (missile.v_angle_z - 90);
   makevectors (missile.v_angle);
   missile.velocity = ((v_forward * 15) + (v_up * 160));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_forward));
   tempRand = ((crandom () * 40) - 20.000);
   missile.velocity = (missile.velocity + (tempRand * v_right));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_up));
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = BecomeFireBall;
   missile.frame = 6;
   setmodel (missile,"progs/smlexp.spr");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,self.origin);
   missile.nextthink = ((time + TRUE) + (crandom () * 0.500));
   missile.think = BecomeFireBall;
};

void (float offsetAngle) FireBallSpurtLaunch = {
   local entity missile;
   local float tempRand;

   missile = spawn ();
   missile.owner = self.owner;
   missile.movetype = MOVETYPE_FLYMISSILE;
   missile.solid = SOLID_BBOX;
   missile.classname = "Dragonfire";
   missile.v_angle = self.v_angle;
   missile.v_angle_y = (missile.v_angle_y + offsetAngle);
   makevectors (missile.v_angle);
   missile.velocity = ((v_forward * 150.000) + (v_up * 2.000));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_forward));
   tempRand = ((crandom () * 40) - 20.000);
   missile.velocity = (missile.velocity + (tempRand * v_right));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_up));
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = BecomeFireSpurt;
   missile.frame = 6;
   setmodel (missile,"progs/medexp.spr");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,self.origin);
   missile.nextthink = ((time + TRUE) + (crandom () * 0.500));
   missile.think = BecomeFireSpurt;
};

void (float offsetAngle) MiniFireBallLaunch = {
   local entity missile;
   local float tempRand;

   missile = spawn ();
   missile.owner = self.owner;
   missile.movetype = MOVETYPE_BOUNCE;
   missile.solid = SOLID_BBOX;
   missile.classname = "Dragonfire";
   missile.v_angle = self.v_angle;
   missile.v_angle_y = (missile.v_angle_y + offsetAngle);
   makevectors (missile.v_angle);
   missile.velocity = ((v_forward * 100.000) + (v_up * 40));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_forward));
   tempRand = ((crandom () * 40) - 20.000);
   missile.velocity = (missile.velocity + (tempRand * v_right));
   tempRand = ((crandom () * 60.000) - 30);
   missile.velocity = (missile.velocity + (tempRand * v_up));
   missile.avelocity = '300.000 300.000 300.000';
   missile.angles = vectoangles (missile.velocity);
   missile.touch = BecomeFireBall;
   missile.frame = 6;
   setmodel (missile,"progs/smlexp.spr");
   setsize (missile,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (missile,self.origin);
   missile.nextthink = ((time + TRUE) + (crandom () * 0.500));
   missile.think = BecomeFireBall;
};

void () DragonFire_Touch = {

   if (pointcontents(self.origin) == CONTENT_SKY) {
      remove(self);
      return;
   }
   if (other.takedamage)
      T_Damage (other,self,self.owner,12,DAMARMOR);

   BecomeFireBall();
};

void () DragonFireExplode = {
   if (self.ammo_rockets > FALSE) {
      T_RadiusDamage (self,self.owner,50,world,DAMAGEALL);
      if (self.ammo_rockets == 6)
         self.frame = 6;

      if (self.ammo_rockets == 4)
         self.frame = 7;

      if (self.ammo_rockets == 3)
         self.frame = 8;

      if (self.ammo_rockets == 2)
         self.frame = 9;

      self.ammo_rockets = (self.ammo_rockets - TRUE);
      self.think = DragonFireExplode;
      self.nextthink = (time + 0.250);
   } else {
      //MiniFireBallLaunch (FALSE);
      //MiniFireBallLaunch (72);
      //MiniFireBallLaunch (144);
      //MiniFireBallLaunch (216);
      //MiniFireBallLaunch (288);
      //FireBallSpurtLaunch (35);
      //FireBallSpurtLaunch (100);
      //FireBallSpurtLaunch (192);
      //FireBallSpurtLaunch (240);
      //FireBallSpurtLaunch (315);
      //FireBallVerticalLaunch();
      BecomeFireBall();
   }
};

void () Fire_DragonFire = {
   self.ammo_rockets = (self.ammo_rockets - TRUE);  // REMOVE
   self.currentammo = (self.currentammo - TRUE);    // REMOVE                    // REMOVE
   sound (self,CHAN_WEAPON,"pyro/flame.wav",TRUE,ATTN_NORM);
   newmis = spawn();
   newmis.owner = self;
   newmis.movetype = MOVETYPE_FLYMISSILE;
   newmis.classname = "Dragonfire";
   newmis.solid = SOLID_NOT;
   setmodel (newmis,"progs/smlexp.spr");
   setsize (newmis,VEC_ORIGIN,VEC_ORIGIN);
   setorigin (newmis,self.origin);
   newmis.frame = 5;
   self.punchangle_x = CONTENT_SOLID;
   makevectors (self.v_angle);
   if ( self.v_angle_x ) {
      newmis.velocity = (((v_forward * 600.000) + ((crandom () * v_right) * 10)) + ((crandom () * v_up) * 10));
   } else {
      newmis.velocity = aim (self,10000.000);
      newmis.velocity = (newmis.velocity * 600.000);
      newmis.velocity_z = 200.000;
   }
   newmis.avelocity = '300.000 300.000 300.000';
   newmis.effects = EF_DIMLIGHT;
   newmis.angles = vectoangles (newmis.velocity);
   newmis.ammo_rockets = 10;
   newmis.touch = DragonFire_Touch;
   newmis.nextthink = (time + 0.50);
   newmis.think = DragonFireExplode;
};

//###########################################
//#### PHASED PLASMA CANNON (BEAM WEAPON) ###
//###########################################
// been tones down so that it doesn't require
// as much stuff as the original did.

void()    beamhit1    =    [0,        beamhit2] {};
void()    beamhit2    =    [1,        beamhit3] {};
void()    beamhit3    =    [2,        beamhit4] {};
void()    beamhit4    =    [3,        beamhit5] {};
void()    beamhit5    =    [4,        SUB_Remove] {};

void() BecomeBeamHit =
{
    self.movetype = MOVETYPE_NONE;
    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    setmodel (self, "progs/beamhit.spr");
    self.solid = SOLID_NOT;
    beamhit1 ();
};

void() beamring1 =    [1,        beamring2] {self.nextthink = time + 0.05;};
void() beamring2 =    [2,        beamring3] {self.nextthink = time + 0.05;};
void() beamring3 =    [4,        beamring4] {self.nextthink = time + 0.05;};
void() beamring4 =    [6,        beamring5] {self.nextthink = time + 0.05;};
void() beamring5 =    [8,        beamring6] {self.nextthink = time + 0.05;};
void() beamring6 =    [10,    beamring7] {self.nextthink = time + 0.05;};
void() beamring7 =    [12,    beamring8] {self.nextthink = time + 0.05;};
void() beamring8 =    [14,    beamring9] {self.nextthink = time + 0.05;};
void() beamring9 =    [16,    SUB_Remove] {self.nextthink = time + 0.05;};

void() d_beamring1 =    [2,        d_beamring2] {};
void() d_beamring2 =    [4,        d_beamring3] {};
void() d_beamring3 =    [8,        d_beamring4] {};
void() d_beamring4 =    [12,    d_beamring5] {};
void() d_beamring5 =    [16,    SUB_Remove] {};

void(vector ringorg) MakeBeamRing =
{
    local entity ringent;
    
    ringent = spawn();
    ringent.movetype = MOVETYPE_NONE;
    setmodel (ringent, "progs/custents/dring.spr");
    ringent.solid = SOLID_NOT;
    setorigin(ringent, ringorg);
    ringent.angles = '90 0 0';
    ringent.frame = 0;
    if(deathmatch)
        ringent.think = d_beamring1;
    else
    {
        ringent.nextthink = time + 0.05;
        ringent.think = beamring1;
    }

    ringent = spawn();
    ringent.movetype = MOVETYPE_NONE;
    setmodel (ringent, "progs/custents/dring.spr");
    ringent.solid = SOLID_NOT;
    setorigin(ringent, ringorg);
    ringent.angles = '-90 0 0';
    ringent.frame = 0;
    if(deathmatch)
        ringent.think = d_beamring1;
    else
    {
        ringent.nextthink = time + 0.05;
        ringent.think = beamring1;
    }
};

void() beam_touch =
{
    local float totaldmg;

    if (other == self.owner)
        return;

    if (other.solid == SOLID_TRIGGER)
        return;   // trigger field, do nothing

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

// hit something that bleeds
    if (other.takedamage)
    {
        totaldmg = self.currentammo * 1.25;
        totaldmg = ceil(totaldmg);
        // totally decimate the dude if doesn't have enough health
        if(other.health <= totaldmg)
        {
            T_Damage (other, self, self.owner, 99999,DAMARMOR);
            if(other.health <= 0)
            {
                MakeBeamRing(other.origin);

                WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
                WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
                WriteCoord (MSG_BROADCAST, other.origin_x);
                WriteCoord (MSG_BROADCAST, other.origin_y);
                WriteCoord (MSG_BROADCAST, other.origin_z);

                remove(self);
                return;
            }
        }
        else
            T_Damage (other, self, self.owner, totaldmg,DAMARMOR);
    }

    sound (self, CHAN_ITEM, "turret/beamhit.wav", 1, ATTN_NORM);
    BecomeBeamHit();
};

void(float burstcharge) beam_water_burst=
{
    local entity current;
    local float totaldmg;
    local vector destvec;

    current = world;

    sound (self, CHAN_WEAPON, "hipweap/mjolhit.wav", 1, ATTN_NORM);
    do
    {
        current = nextent(current);

        if (current.takedamage)
        {
//            destvec = current.origin + (current.mins + current.maxs)*0.5;
            destvec = current.origin;
            traceline(self.origin, destvec, TRUE, self);
            if(trace_fraction == 1 && current.waterlevel > 0)
            {
                totaldmg = vlen(self.origin - destvec) * 0.3;
                totaldmg = burstcharge * burstcharge / totaldmg;
                // check to see if the thing gets killed
                if(current.health <= totaldmg)
                {
                    T_Damage (current, self, self, 99999,DAMARMOR);
                    if(other.health <= 0)
                    {
                        MakeBeamRing(other.origin);

                        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
                        WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
                        WriteCoord (MSG_BROADCAST, other.origin_x);
                        WriteCoord (MSG_BROADCAST, other.origin_y);
                        WriteCoord (MSG_BROADCAST, other.origin_z);
                    }
                }
                else
                    T_Damage (current, self, self, totaldmg,DAMARMOR);
            }
        }
    } while(current);
};

void() beam_generate=
{
    local entity beamseg;

    beamseg = spawn ();
    beamseg.owner = self.owner;
    beamseg.solid = SOLID_BBOX;
    setmodel (beamseg, "progs/custents/beam.mdl");
    setsize (beamseg, '0 0 0', '0 0 0');
    makevectors(self.owner.v_angle);
    if(self.owner.flags & FL_CLIENT)
    {
        self.owner.attack_finished = time + 0.5;
        setorigin (beamseg, self.owner.origin + v_forward*12 + self.owner.dest2);
        beamseg.velocity = aim(self.owner, 800) * 800;
    }
    else if(self.owner.flags & FL_MONSTER)
    {
        setorigin (beamseg, self.owner.origin);
        beamseg.velocity = normalize(self.owner.enemy.origin - self.owner.origin) * 800;
    }
    else
    {
        setorigin (beamseg, self.owner.origin);
        beamseg.velocity = self.owner.movedir * 800;
    }

    beamseg.angles = vectoangles(beamseg.velocity);
    beamseg.speed = 800;
    beamseg.classname = "beam";
    beamseg.movetype = MOVETYPE_FLYMISSILE;
    beamseg.currentammo = self.currentammo;
    beamseg.touch = beam_touch;
    beamseg.think = SUB_Remove;
    beamseg.nextthink = time + 6;

    sound (self.owner, CHAN_WEAPON, "weapons/lstart.wav", 1, ATTN_NORM);
    self.currentammo = self.currentammo / 2;

    if((self.owner.flags & FL_CLIENT) && self.deadflag == DEAD_NO) {
        self.owner.punchangle_x = -6;
        self.owner.velocity = self.velocity - v_forward * self.currentammo * 2;
    }

    if(self.currentammo < 1)
        remove(self);

    self.nextthink = time + 0.05;
};

void() W_FireBeam = {
    // explode if under water
    if (self.waterlevel > 1) {
        beam_water_burst(self.ammo_cells*2);
        self.currentammo = self.ammo_cells = 0;
        return;
    }

    newmis = spawn();
    newmis.solid = SOLID_NOT;
    newmis.movetype = MOVETYPE_FLY;
    // amount of damage done is based on how much cell ammo you've got
    newmis.currentammo = self.currentammo;
    setorigin(newmis, self.origin);
    newmis.owner = self;
    newmis.think = beam_generate;
    newmis.nextthink = time + 0.05;

    // like the damage, the ammo used is according to how much you've got
    self.currentammo = self.ammo_cells = floor(self.ammo_cells * 0.75);
};

void() Laser2_Touch =
 {

    local vector org;


    
if (other == self.owner)

        return;        // don't explode on owner



    if (pointcontents(self.origin) == CONTENT_SKY)
    {

        remove(self);

        return;

    }

    
sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);

    org = self.origin - 8*normalize(self.velocity);


    if (other.health) 
{

        SpawnBlood (self.enemy, org,v_forward, 9);

        T_Damage (other, self, self.owner, 9, DAMARMOR);

    }

    remove(self);

};
