float USE_PARTICLES    = 1.000;

void () exploder_fire = {
   local entity temp;

   temp = self;
   activator = other;
   SUB_UseTargets ();
   self = temp;
   other = self;
   if ( (self.dmg < 120) ) {
      sound (self,CHAN_AUTO,"drake/misc/shortexp.wav",self.volume,self.speed);
   } else {
      sound (self,CHAN_AUTO,"drake/misc/longexpl.wav",self.volume,self.speed);
   }
   T_RadiusDamage (self,self.owner,self.dmg,other,DAMAGEALL);
   if ( (self.spawnflags & USE_PARTICLES) ) {
      WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
      WriteByte (MSG_BROADCAST,TE_EXPLOSION);
      WriteCoord (MSG_BROADCAST,self.origin_x);
      WriteCoord (MSG_BROADCAST,self.origin_y);
      WriteCoord (MSG_BROADCAST,self.origin_z);
   }
   BecomeExplosion ();
};

void () exploder_use = {
   if ( self.delay ) {
      self.nextthink = (time + self.delay);
      self.delay = FALSE;
      self.think = exploder_fire;
   } else {
      exploder_fire ();
   }
};

void () func_exploder = {
   precache_sound ("drake/misc/shortexp.wav");
   precache_sound ("drake/misc/longexpl.wav");
   self.classname = "exploder";
   self.use = exploder_use;
   if ( (self.dmg == FALSE) ) self.dmg = 120;
   if ( (self.dmg < FALSE) ) self.dmg = FALSE;
   if ( (self.speed == FALSE) ) self.speed = TRUE;
   if ( (self.volume == FALSE) ) self.volume = TRUE;
};

void () multi_exploder_fire = {
   local entity temp;
   local entity expl;

   self.nextthink = (time + self.wait);
   if ( (self.state == FALSE) ) {
      self.state = TRUE;
      self.duration = (time + self.duration);
      temp = self;
      activator = other;
      SUB_UseTargets ();
      self = temp;
      other = self;
   }
   if ( (time > self.duration) ) {
      remove (self);
      return ;
   }
   expl = spawn ();
   expl.owner = self.owner;
   expl.dmg = self.dmg;
   random ();
   expl.origin_x = (self.absmin_x + (random () * (self.absmax_x - self.absmin_x)));
   random ();
   expl.origin_y = (self.absmin_y + (random () * (self.absmax_y - self.absmin_y)));
   random ();
   expl.origin_z = (self.absmin_z + (random () * (self.absmax_z - self.absmin_z)));
   sound (expl,CHAN_VOICE,"drake/misc/shortexp.wav",self.volume,self.speed);
   T_RadiusDamage (expl,self.owner,self.dmg,other,DAMAGEALL);
   if ( (self.spawnflags & USE_PARTICLES) ) {
      WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
      WriteByte (MSG_BROADCAST,TE_EXPLOSION);
      WriteCoord (MSG_BROADCAST,expl.origin_x);
      WriteCoord (MSG_BROADCAST,expl.origin_y);
      WriteCoord (MSG_BROADCAST,expl.origin_z);
   }
   temp = self;
   self = expl;
   BecomeExplosion ();
   self = temp;
};

void (vector loc, float rad, float damage, float dur, float pause, float vol) multi_explosion = {
   local entity temp;

   temp = self;
   self = spawn ();
   self.origin = loc;
   self.dmg = damage;
   self.duration = dur;
   self.wait = pause;
   self.owner = world;
   self.absmin = (self.origin - (rad * '1.000 1.000 1.000'));
   self.absmax = (self.origin + (rad * '1.000 1.000 1.000'));
   self.think = multi_exploder_fire;
   self.volume = vol;
   multi_exploder_fire ();
   self = temp;
};

void () multi_exploder_use = {
   if ( self.delay ) {
      self.nextthink = (time + self.delay);
      self.delay = FALSE;
      self.think = multi_exploder_fire;
   } else {
      self.think = multi_exploder_fire;
      multi_exploder_fire ();
   }
};

void () func_multi_exploder = {
   precache_sound ("drake/misc/shortexp.wav");
   precache_sound ("drake/misc/longexpl.wav");
   self.classname = "exploder";
   self.use = multi_exploder_use;
   setmodel (self,self.model);
   self.movetype = MOVETYPE_NONE;
   self.modelindex = FALSE;
   self.model = "";
   if ( (self.dmg == FALSE) ) self.dmg = 120;
   if ( (self.dmg < FALSE) )  self.dmg = FALSE;
   if ( (self.duration == FALSE) ) self.duration = TRUE;
   if ( (self.speed == FALSE) ) self.speed = TRUE;
   if ( (self.volume == FALSE) ) self.volume = 0.500;
   if ( (self.wait == FALSE) ) self.wait = 0.250;
   self.state = FALSE;
};