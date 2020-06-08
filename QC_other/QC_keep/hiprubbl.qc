void () hipRubbleTouch = {
   if ( (self.ltime < self.pausetime) ) return ;
   if ( other.takedamage ) {
      T_Damage (other,self,self.owner,MOVETYPE_BOUNCE,DAMARMOR);
      sound (self,CHAN_WEAPON,"zombie/z_hit.wav",TRUE,ATTN_NORM);
      self.pausetime = (self.ltime + 0.100);
   }
};

void (string rubblename) hipThrowRubble = {
   local entity new;

   new = spawn ();
   new.origin = self.origin;
   setmodel (new,rubblename);
   setsize (new,VEC_ORIGIN,VEC_ORIGIN);
   new.velocity_x = (70.000 * crandom ());
   new.velocity_y = (70.000 * crandom ());
   new.velocity_z = (140.000 + (70.000 * random ()));
   new.movetype = MOVETYPE_BOUNCE;
   new.solid = SOLID_BBOX;
   new.avelocity_x = (random () * 600.000);
   new.avelocity_y = (random () * 600.000);
   new.avelocity_z = (random () * 600.000);
   new.think = SUB_Remove;
   new.touch = hipRubbleTouch;
   new.ltime = time;
   new.nextthink = ((time + 13.000) + (random () * MOVETYPE_BOUNCE));
   self.pausetime = time;
   new.frame = FALSE;
   new.flags = FALSE;
};

void () hip_rubble_use = {
   local float which;
   local float index;

   index = FALSE;
   do {
      which = self.cnt;
      if ( (self.cnt == FALSE) ) {
         which = (TRUE + (MOVETYPE_WALK * random ()));
         which = floor (which);
      }
      if ( (which == 1) ) {
         hipThrowRubble ("progs/rubble1.mdl");
      } else {
         if ( (which == 2) ) {
            hipThrowRubble ("progs/rubble3.mdl");
         } else {
            hipThrowRubble ("progs/rubble2.mdl");
         }
      }
      index = (index + TRUE);
   } while ( (index < self.count) );
};

void () func_rubble = {
   precache_model ("progs/rubble1.mdl");
   precache_model ("progs/rubble2.mdl");
   precache_model ("progs/rubble3.mdl");
   precache_sound ("zombie/z_hit.wav");
   self.classname = "rubble";
   self.cnt = FALSE;
   self.use = hip_rubble_use;
};

void () func_rubble1 = {
   precache_model ("progs/rubble1.mdl");
   precache_sound ("zombie/z_hit.wav");
   self.classname = "rubble1";
   self.cnt = TRUE;
   self.use = hip_rubble_use;
};

void () func_rubble2 = {
   precache_model ("progs/rubble3.mdl");
   precache_sound ("zombie/z_hit.wav");
   self.classname = "rubble2";
   self.cnt = FL_SWIM;
   self.use = hip_rubble_use;
};

void () func_rubble3 = {
   precache_model ("progs/rubble2.mdl");
   precache_sound ("zombie/z_hit.wav");
   self.classname = "rubble3";
   self.cnt = MOVETYPE_WALK;
   self.use = hip_rubble_use;
};