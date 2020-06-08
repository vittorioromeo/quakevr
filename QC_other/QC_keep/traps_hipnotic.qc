void () trap_tesla_switched_use = { self.state = (TRUE - self.state); if ( (self.state == TRUE) ) self.nextthink = (time + 0.1); };

void () trap_tesla_scan = {
   local entity head;
   local entity prev;

   tesla_numtargets = FALSE;
   head = findradius (self.origin,self.distance);
   while ( head ) {
      if ( (!(head.flags & FL_NOTARGET) && (head.flags & self.cnt)) ) {
         visible (head);
         if ( ((visible (head) && (head.health > FALSE)) && (head.struck_by_mjolnir == FALSE)) ) {
            if ( (tesla_numtargets == FALSE) ) {
               tesla_target = head;
            } else {
               prev.next_ent = head;
            }
            tesla_numtargets = (tesla_numtargets + TRUE);
            prev = head;
            if ( (tesla_numtargets == self.count) ) return ;
         }
      }
      head = head.chain;
   }
};

void () TeslaLightningThink = {
   self.owner.attack_state = FL_SWIM;
   if ( (time > self.delay) ) {
      self.enemy.struck_by_mjolnir = FALSE;
      remove (self);
      return ;
   }
   traceline (self.origin,self.enemy.origin,TRUE,self);
   vlen ((self.origin - self.enemy.origin));
   if ( (((trace_fraction != TRUE) || (self.enemy.health <= FALSE)) || (vlen ((self.origin - self.enemy.origin)) > (self.distance + MOVETYPE_BOUNCE))) ) {
      self.enemy.struck_by_mjolnir = FALSE;
      remove (self);
      return ;
   }
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_LIGHTNING2);
   WriteEntity (MSG_BROADCAST,self);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   WriteCoord (MSG_BROADCAST,trace_endpos_x);
   WriteCoord (MSG_BROADCAST,trace_endpos_y);
   WriteCoord (MSG_BROADCAST,trace_endpos_z);
   LightningDamage (self.origin,trace_endpos,self.lastvictim,self.dmg);
   self.nextthink = (time + 0.100);
};

void (entity targ) SpawnTeslaLightning = {
   local entity lgt;

   lgt = spawn ();
   if ( (self.duration > FALSE) ) {
      lgt.delay = (time + self.duration);
   } else {
      lgt.delay = (time + 9999.000);
   }
   lgt.enemy = targ;
   targ.struck_by_mjolnir = TRUE;
   lgt.distance = self.distance;
   lgt.owner = self;
   lgt.lastvictim = self.lastvictim;
   lgt.dmg = self.dmg;
   lgt.origin = self.origin;
   lgt.think = TeslaLightningThink;
   lgt.nextthink = time;
   lgt.deathtype = self.deathtype;
};

void () trap_tesla_think = {
   if ( (self.state == 0) ) { self.nextthink = (time + 0.250); return; }
   if ( (self.attack_state == 0) ) {
      self.think = trap_tesla_think;
      trap_tesla_scan ();
      if ( (tesla_numtargets > FALSE) ) {
         if ( (self.wait > FALSE) ) sound (self,CHAN_AUTO,"misc/tesla.wav",TRUE,ATTN_NORM);
         self.attack_state = TRUE;
         self.nextthink = (time + self.wait);
         return ;
      }
      self.nextthink = (time + 0.250);
      if ((self.delay > FALSE) && (time > self.search_time) ) self.attack_state = MOVETYPE_WALK;
   } else {
      if ( (self.attack_state == TRUE) ) {
         trap_tesla_scan ();
         while ( (tesla_numtargets > FALSE) ) {
            sound (self,CHAN_AUTO,"hipweap/mjolhit.wav",TRUE,ATTN_NORM);
            SpawnTeslaLightning (tesla_target);
            tesla_target = tesla_target.next_ent;
            tesla_numtargets = (tesla_numtargets - TRUE);
         }
         self.attack_state = FL_SWIM;
         self.nextthink = (time + TRUE);
      } else {
         if ( (self.attack_state == FL_SWIM) ) {
            self.attack_state = MOVETYPE_WALK;
            self.nextthink = (time + 0.200);
         } else {
            if ( (self.attack_state == MOVETYPE_WALK) ) {
               self.attack_state = FALSE;
               self.nextthink = (time + 0.100);
               if ( (self.classname == "trap_gods_wrath") ) {
                  self.nextthink = CONTENT_EMPTY;
               }
            }
         }
      }
   }
};

void () trap_tesla_coil = {
   precache_sound ("misc/tesla.wav");
   precache_sound ("hipweap/mjolhit.wav");
   if ( (self.wait == FALSE) ) self.wait = FL_SWIM;
   if ( (self.dmg == FALSE) ) self.dmg = (FL_SWIM + (MOVETYPE_FLY * cvar ("skill")));
   if ( (self.duration == FALSE) ) self.duration = CONTENT_EMPTY;
   if ( (self.distance == FALSE) ) self.distance = 600.000;
   if ( (self.spawnflags & TRUE) ) { self.cnt = (FL_CLIENT | FL_MONSTER); } else { self.cnt = FL_CLIENT; }
   self.use = trap_tesla_switched_use;
   if ( (self.delay == FALSE) ) self.delay = CONTENT_EMPTY;
   self.nextthink = (time + random ());
   self.think = trap_tesla_think;
   self.lastvictim = world;
   tesla_numtargets = FALSE;
   self.attack_state = FALSE;
   self.deathtype = "is electrocuted";
};

void () trap_gods_wrath_use = {
   if ( (self.attack_state == FALSE) ) {
      self.search_time = (time + self.delay);
      self.lastvictim = activator;
      trap_tesla_think ();
   }
};

void () trap_gods_wrath = {
   if ( (self.delay == FALSE) ) self.delay = MOVETYPE_FLY;
   trap_tesla_coil ();
   self.wait = FALSE;
   self.state = TRUE;
   self.nextthink = CONTENT_EMPTY;
   self.deathtype = "suffers the wrath of God";
   self.use = trap_gods_wrath_use;
};

void () trap_gravity_touch = {
   if ( (self.attack_finished > time) ) return ;
   if ( other.takedamage ) { T_Damage (other,self,self,self.dmg,DAMARMOR); self.attack_finished = (time + 0.200); }
};

void () trap_gravity_think = {
   local vector vel;
   local vector dir;
   local vector delta;

   self.ltime = time;
   trap_tesla_scan ();
   while ( (tesla_numtargets > FALSE) ) {
      delta = (self.origin - tesla_target.origin);
      dir = normalize (delta);
      vel = (dir * self.speed);
      if ( ((tesla_target.wetsuit_finished > time) && (self.spawnflags & UNDERWATER)) ) vel = (vel * 0.600);
      tesla_target.velocity = (tesla_target.velocity + vel);
      tesla_target = tesla_target.next_ent;
      tesla_numtargets = (tesla_numtargets - TRUE);
   }
   self.nextthink = (time + 0.100);
};

void () trap_gravity_well = {
   self.solid = SOLID_TRIGGER;
   self.movetype = MOVETYPE_NONE;
   setsize (self,'-16.000 -16.000 -16.000','16.000 16.000 16.000');
   if ( (self.dmg == FALSE) ) self.dmg = 10000.000;
   if ( (self.speed == FALSE) ) self.speed = 210.000;
   if ( (self.distance == FALSE) ) self.distance = 600.000;
   if ( (self.spawnflags & TRUE) ) { self.cnt = (FL_CLIENT | FL_MONSTER); } else { self.cnt = FL_CLIENT; }
   self.attack_finished = FALSE;
   self.think = trap_gravity_think;
   self.touch = trap_gravity_touch;
   self.lastvictim = world;
   tesla_numtargets = FALSE;
   self.nextthink = (time + 0.100);
   self.ltime = time;
};