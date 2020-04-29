/* Water QuickC program
   By Jim Dose'  12/16/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() bobbingwater_think =
   {
   local vector ang;

   self.count = self.count + self.speed * ( time - self.ltime );
   if ( self.count > 360 )
      {
      self.count = self.count - 360;
      }
   ang_x = self.count;
   ang_y = 0;
   ang_z = 0;
   makevectors( ang );
   self.origin_z = v_forward_z * self.cnt;
   setorigin( self, self.origin );
   self.ltime = time;
   self.nextthink = time + 0.02;
   };

/*QUAKED func_bobbingwater (0 .5 .8) ?
Used to emulate water.  To use, create a thin water brush and center it
on the water line of the body of water to bob.  The amount of the bob
is the depth of the brush.

"speed" is how long in seconds it takes the brush to do one full bob.
*/
void() func_bobbingwater =
   {
   self.angles = '0 0 0';
   self.movetype = MOVETYPE_STEP;
   self.solid = SOLID_NOT;
   setmodel (self,self.model);
   self.think = bobbingwater_think;

   self.count = 0;
   self.cnt = self.size_z / 2;
   if ( !self.speed )
      {
      self.speed = 4;
      }

   self.speed = 360 / self.speed;

   self.nextthink = time + 0.02;
   self.ltime = time;
   };
