float MULTI_USE    = 1.000;
float INVISIBLE    = 2.000;

void () damagethreshold_killed = {
   self.health = self.max_health;
   activator = damage_attacker;
   self.takedamage = DAMAGE_NO;
   SUB_UseTargets ();
   self.takedamage = DAMAGE_YES;
   if ( !(self.spawnflags & MULTI_USE) ) remove (self);
};

void (entity inflictor, entity attacker, float damage) damagethreshold_pain = {
   self.health = self.max_health;
};

void () trigger_damagethreshold = {
   self.mangle = self.angles;
   self.angles = VEC_ORIGIN;
   self.classname = "damagethreshold";
   self.solid = SOLID_BSP;
   self.movetype = MOVETYPE_PUSH;
   setorigin (self,self.origin);
   setmodel (self,self.model);
   setsize (self,self.mins,self.maxs);
   if ( (self.spawnflags & INVISIBLE) ) self.model = string_null;
   if ( !self.health ) self.health = 60.000;
   self.max_health = self.health;
   self.takedamage = DAMAGE_YES;
   self.blocked = SUB_Null;
   self.th_pain = damagethreshold_pain;
   self.th_die = damagethreshold_killed;
};

void () func_breakawaywall = {
   self.mangle = self.angles;
   self.angles = VEC_ORIGIN;
   self.classname = "breakaway";
   self.solid = SOLID_BSP;
   self.movetype = MOVETYPE_PUSH;
   setorigin (self,self.origin);
   setmodel (self,self.model);
   setsize (self,self.mins,self.maxs);
   self.use = SUB_Remove;
};