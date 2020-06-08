/*
===========================================================================

The Zerstörer!!

===========================================================================
*/

void() Z_idle = 
{
    self.frame = self.frame + 1;
    self.cnt = self.cnt + 1;

    if (self.frame >= 20)
        self.frame = 0;
    if (self.cnt > 40)
        self.cnt = 0;

    if (self.cnt < 20)
        setorigin(self, self.origin + '0 0 1');
    else
        setorigin(self, self.origin - '0 0 1');

    self.think = Z_idle;
    self.nextthink = time + 0.1;
};


void() zer_awake =
{
    self.solid = SOLID_BBOX;
    self.movetype = MOVETYPE_FLY;
    setsize (self, '-16 -16 -24', '16 16 54');
    setorigin(self, self.origin);
    setmodel (self, "progs/bbg.mdl");
    self.think = Z_idle;
    self.nextthink = time + 0.1;

    sound (self, CHAN_AUTO, "boss1/out1.wav", 1, ATTN_NORM);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, (self.origin_z - 8));    
};

/*QUAKED monster_zer (1 0 0) (-128 -128 -24) (128 128 256)
*/
void() monster_zer =
{
    if (deathmatch)
    {
        remove(self);
        return;
    }
    precache_model ("progs/bbg.mdl");
    precache_sound ("boss1/out1.wav");

    if (self.deathstring == "") self.deathstring = " was destroyed by the Zerstorer\n";

    self.use = zer_awake;
};

//=========================================================================

/* Finale explosive stuff */

void() do_lavasplash =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    remove(self);
};

void() func_lavasplash =
{
    self.use = do_lavasplash;
};

//===============

void() do_explode =
{
    //T_RadiusDamage (self, self.owner, 120, world);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    SUB_UseTargets ();

    BecomeExplosion ();
};

void() timed_exp =
{
    self.think = do_explode;
    self.nextthink = time + self.delay;
};

void() func_explode =
{
    self.owner = find(world, classname, "monster_zer");
    if (self.delay)
        self.use = timed_exp;
    else
        self.use = do_explode;
};