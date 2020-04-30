/* Rubble QuickC program
   By Jim Dose'  9/15/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

void() hipRubbleTouch =
    {
    if( self.ltime < self.pausetime )
       return;

    if(other.takedamage)
        {
        T_Damage(other, self, self.owner, 10 );
        sound(self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
        self.pausetime = self.ltime + 0.1;
        }
    };

void(string rubblename) hipThrowRubble =
{
    local    entity new;

    new = spawn();
    new.origin = self.origin;
    setmodel (new, rubblename );
    setsize (new, '0 0 0', '0 0 0');
    new.velocity_x = 70 * crandom();
    new.velocity_y = 70 * crandom();
    new.velocity_z = 140 + 70 * random();
    new.movetype = MOVETYPE_BOUNCE;
    new.solid = SOLID_BBOX;
    new.avelocity_x = random()*600;
    new.avelocity_y = random()*600;
    new.avelocity_z = random()*600;
    new.think = SUB_Remove;
    new.touch = hipRubbleTouch;
    new.ltime = time;
    new.nextthink = time + 13 + random()*10;
    self.pausetime = time;
    new.frame = 0;
    new.flags = 0;
};

void() rubble_use =
    {
    local float which;
    local float index;

    index = 0;

    do
       {
        which = self.cnt;
        if( self.cnt == 0 )
            {
            which = 1 + 3*random();
            which = floor( which );
            }
        if( which == 1 )
           {
           hipThrowRubble( "progs/rubble1.mdl" );
           }
        else if( which == 2 )
           {
           hipThrowRubble( "progs/rubble3.mdl" );
           }
        else
           {
           hipThrowRubble( "progs/rubble2.mdl" );
           }
        index = index + 1;
        }
    while( index < self.count );
    };

/*QUAKED func_rubble (0.4 0.4 0.2) (0 0 0) (32 32 32)
  Spawns random sized rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble =
    {
    precache_model ("progs/rubble1.mdl");
    precache_model ("progs/rubble2.mdl");
    precache_model ("progs/rubble3.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble";
    self.cnt = 0;
    self.use = rubble_use;
    };

/*QUAKED func_rubble1 (0.4 0.4 0.2) (0 0 0) (8 8 8)
  Spawns small rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble1 =
    {
    precache_model ("progs/rubble1.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble1";
    self.cnt = 1;
    self.use = rubble_use;
    };

/*QUAKED func_rubble2 (0.4 0.4 0.2) (0 0 0) (16 16 16)
  Spawns medium rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble2 =
    {
    precache_model ("progs/rubble3.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble2";
    self.cnt = 2;
    self.use = rubble_use;
    };

/*QUAKED func_rubble3 (0.4 0.4 0.2) (0 0 0) (32 32 32)
  Spawns large rubble when triggered.

  "count" is the number of pieces of rubble to spawn.  Default is 1.
*/
void() func_rubble3 =
    {
    precache_model ("progs/rubble2.mdl");
    precache_sound("zombie/z_hit.wav");
    self.classname = "rubble3";
    self.cnt = 3;
    self.use = rubble_use;
    };

