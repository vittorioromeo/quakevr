/* Rubble QuickC program
   By Jim Dose'  9/15/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() hipRubbleTouch =
{
    if(self.lip || vlen(self.velocity) < 10.0)
    {
        return;
    }

    if(other.takedamage)
    {
        T_Damage(other, self, self.owner, 10);
        sound(self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
        self.lip = TRUE;
    }
}

void(string rubblename) hipThrowRubble =
{
    local entity newent;

    newent = spawn();
    newent.origin = self.origin;
    setmodel(newent, rubblename);
    setsize(newent, '0 0 0', '0 0 0');
    newent.velocity_x = 70 * crandom();
    newent.velocity_y = 70 * crandom();
    newent.velocity_z = 140 + 70 * random();
    newent.movetype = MOVETYPE_BOUNCE;
    newent.solid = SOLID_BBOX;
    newent.avelocity_x = random()*600;
    newent.avelocity_y = random()*600;
    newent.avelocity_z = random()*600;
    newent.think = SUB_Remove;
    newent.touch = hipRubbleTouch;
    newent.nextthink = time + 13 + random()*10;
    newent.frame = 0;
    newent.flags = 0;
    newent.lip = FALSE; // hit someone yet?
}

void() hip_rubble_use =
{
    local float which;
    local float index;

    index = 0;

    do
    {
        which = self.cnt;

        if(self.cnt == 0)
        {
            which = 1 + 3*random();
            which = floor(which);
        }

        if(which == 1)
        {
           hipThrowRubble("progs/rubble1.mdl");
        }
        else if(which == 2)
        {
           hipThrowRubble("progs/rubble3.mdl");
        }
        else
        {
           hipThrowRubble("progs/rubble2.mdl");
        }

        index = index + 1;
    }
    while(index < self.count);
}

/*QUAKED func_rubble (0.4 0.4 0.2) (0 0 0) (32 32 32)
  Spawns random sized rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble =
{
    precache_model("progs/rubble1.mdl");
    precache_model("progs/rubble2.mdl");
    precache_model("progs/rubble3.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble";
    self.cnt = 0;
    self.use = hip_rubble_use;
}

/*QUAKED func_rubble1 (0.4 0.4 0.2) (0 0 0) (8 8 8)
  Spawns small rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble1 =
{
    precache_model("progs/rubble1.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble1";
    self.cnt = 1;
    self.use = hip_rubble_use;
}

/*QUAKED func_rubble2 (0.4 0.4 0.2) (0 0 0) (16 16 16)
  Spawns medium rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble2 =
{
    precache_model("progs/rubble3.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble2";
    self.cnt = 2;
    self.use = hip_rubble_use;
}

/*QUAKED func_rubble3 (0.4 0.4 0.2) (0 0 0) (32 32 32)
  Spawns large rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble3 =
{
    precache_model("progs/rubble2.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble3";
    self.cnt = 3;
    self.use = hip_rubble_use;
}

