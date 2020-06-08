float USE_COUNT    = 1.000;
float START_OFF    = 1.000;

void () particlefield_XZ = {
   local vector pos;
   local vector start;
   local vector end;

   if ( ((self.spawnflags & USE_COUNT) && (counter_GetCount (other) != self.cnt)) ) return ;
   self.ltime = (time + 0.250);
   if ( self.noise ) sound (self,CHAN_VOICE,self.noise,TRUE,ATTN_NORM);
   if ( !checkclient () ) return ;
   start = (self.dest1 + self.origin);
   end = (self.dest2 + self.origin);
   pos_y = start_y;
   pos_z = start_z;
   while ( (pos_z <= end_z) ) {
      pos_x = start_x;
      while ( (pos_x <= end_x) ) {
         particle (pos,'0 0 0',self.col,self.count);
         pos_x = (pos_x + FL_INWATER);
      }
      pos_z = (pos_z + FL_INWATER);
   }
};

void () particlefield_YZ = {
   local vector pos;
   local vector start;
   local vector end;

   if ( ((self.spawnflags & USE_COUNT) && (counter_GetCount (other) != self.cnt)) ) return;
   self.ltime = (time + 0.250);
   if ( self.noise ) sound (self,CHAN_VOICE,self.noise,TRUE,ATTN_NORM);
   if ( !checkclient () ) return;
   start = (self.dest1 + self.origin);
   end = (self.dest2 + self.origin);
   pos_x = start_x;
   pos_z = start_z;
   while ( (pos_z < end_z) ) {
      pos_y = start_y;
      while ( (pos_y < end_y) ) {
         particle (pos,VEC_ORIGIN,self.col,self.count);
         pos_y = (pos_y + FL_INWATER);
      }
      pos_z = (pos_z + FL_INWATER);
   }
};

void () particlefield_XY = {
   local vector pos;
   local vector start;
   local vector end;

   if ( ((self.spawnflags & USE_COUNT) && (counter_GetCount (other) != self.cnt)) ) return ;
   self.ltime = (time + 0.250);
   if ( self.noise ) sound (self,CHAN_VOICE,self.noise,TRUE,ATTN_NORM);
   if ( !checkclient () ) return ;
   start = (self.dest1 + self.origin);
   end = (self.dest2 + self.origin);
   pos_x = start_x;
   pos_z = start_z;
   while ( (pos_x < end_x) ) {
      pos_y = start_y;
      while ( (pos_y < end_y) ) {
         particle (pos,VEC_ORIGIN,self.col,self.count);
         pos_y = (pos_y + FL_INWATER);
      }
      pos_x = (pos_x + FL_INWATER);
   }
};

void () particlefield_touch = {
   if ( !self.dmg ) return;
   if ( (time > self.ltime) ) return;
   if ( (time < self.attack_finished) ) return;
   self.attack_finished = (time + 0.500);
   T_Damage (other,self,self,self.dmg,DAMARMOR);
};

void () func_particlefield = {
   if ( !self.col ) self.col = 192.000;
   if ( (self.count == FALSE) ) self.count = FL_SWIM;
   self.classname = "particlefield";
   self.solid = SOLID_NOT;
   self.movetype = MOVETYPE_NONE;
   setmodel (self,self.model);
   self.model = string_null;
   self.origin = ((self.mins + self.maxs) * 0.500);
   setorigin (self,self.origin);
   self.dest = ((self.maxs - self.mins) - '16.000 16.000 16.000');
   self.dest1 = ((self.mins + '8.000 8.000 8.000') - self.origin);
   self.dest2 = ((self.maxs + '7.900 7.900 7.900') - self.origin);
   setsize (self,self.mins,self.maxs);
   self.touch = particlefield_touch;
   if ( (self.dest_x > self.dest_z) ) {
      if ( (self.dest_y > self.dest_z) ) {
         self.use = particlefield_XY;
         self.dest1_z = ((self.dest1_z + self.dest2_z) / FL_SWIM);
      } else {
         self.use = particlefield_XZ;
     self.dest1_y = ((self.dest1_y + self.dest2_y) / FL_SWIM);
      }
   } else {
      if ( (self.dest_y > self.dest_x) ) {
         self.use = particlefield_YZ;
         self.dest1_x = ((self.dest1_x + self.dest2_x) / FL_SWIM);
      } else {
         self.use = particlefield_XZ;
         self.dest1_y = ((self.dest1_y + self.dest2_y) / FL_SWIM);
      }
   }
   if ( self.noise ) precache_sound (self.noise);
   self.ltime = time;
};

void () blocker_touch = {
   if ( !self.dmg ) return ;
   if ( (time < self.attack_finished) ) return ;
   self.attack_finished = (time + 0.500);
   T_Damage (other,self,self,self.dmg,DAMARMOR);
};

void () blocker_use = {
   if ( !self.state ) {
      self.state = TRUE;
      setorigin (self,(self.origin - '8000.000 8000.000 8000.000'));
      sound (self,CHAN_VOICE,self.noise1,TRUE,ATTN_NORM);
   } else {
      self.state = FALSE;
      setorigin (self,(self.origin + '8000.000 8000.000 8000.000'));
      sound (self,CHAN_VOICE,self.noise,TRUE,ATTN_NORM);
   }
};

void () func_togglewall = {
   self.classname = "togglewall";
   self.movetype = MOVETYPE_PUSH;
   self.mdl = self.model;
   setmodel (self,self.model);
   setsize (self,self.mins,self.maxs);
   setorigin (self,self.origin);
   self.touch = blocker_touch;
   self.use = blocker_use;
   if ( !self.noise ) self.noise = "misc/null.wav";
   if ( !self.noise1 ) self.noise1 = "misc/null.wav";
   precache_sound (self.noise);
   precache_sound (self.noise1);
   self.solid = SOLID_BSP;
   self.model = string_null;
   if ( (self.spawnflags & START_OFF) ) {
      self.state = FALSE;
      setorigin (self,(self.origin + '8000.000 8000.000 8000.000'));
   } else {
      self.state = TRUE;
      sound (self,CHAN_VOICE,self.noise1,TRUE,ATTN_NORM);
   }
};