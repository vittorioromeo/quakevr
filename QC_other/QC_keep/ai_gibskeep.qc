void() monster_death_postcheck;

//======================================================================
// StartGib function for Rogue Overlord Wrath
//======================================================================
entity (string gibname) StartGib = {
   local entity new;

   new = spawn ();
   new.origin = self.origin;
   setmodel (new,gibname);
   setsize (new,VEC_ORIGIN,VEC_ORIGIN);
   new.movetype = MOVETYPE_BOUNCE;
   new.solid = SOLID_NOT;
   new.think = SUB_Remove;
   new.ltime = time;
   new.nextthink = ((time + 10) + (random () * 10));
   new.frame = FALSE;
   new.flags = FALSE;
   return (new);
};

//----------------------------------------------------------------------
void() ThrowHead = {
    setmodel (self, self.headmdl);
    self.frame = 0;
    self.nextthink = -1;
    self.movetype = MOVETYPE_BOUNCE;
    self.takedamage = DAMAGE_NO;
    self.solid = SOLID_NOT;
    // Hide the entity if it hits void, don't remove it
    self.classgroup = CG_MISCENT;
    self.view_ofs = '0 0 8';
    setsize (self, '-16 -16 0', '16 16 32');
    
    self.velocity_x = 50 * crandom();
    self.velocity_y = 50 * crandom();
    self.velocity_z = 200 + 100 * random();

    self.avelocity = '0 0 0';
    self.avelocity_y = 200 + random() * 200;
    self.angles_y = self.ideal_yaw = random() * 360;

    //----------------------------------------------------------------------
    // Check for water content, adjust for water friction
    if (pointcontents(gibitem.oldorigin) == CONTENT_WATER) {
        gibitem.gravity = 0.25;
        gibitem.velocity_z = 50 + 50 * random();
    }

    self.flags = self.flags - (self.flags & FL_ONGROUND);
    monster_death_postcheck();
};