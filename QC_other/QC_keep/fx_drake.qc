void() T_MissileTouch;

// This creates an explosion sprite entity from scratch.
void(vector spot) MakeExplosion =
{
    local   entity  swap;

    swap = self;

    self = spawn();
    setsize (self, '0 0 0', '0 0 0');        
    setorigin (self, spot);
    BecomeExplosion ();

    self = swap;
};

//======================================
//  Standard Rockets
//======================================
// PM:  Replaced owner with master for all missiles because of reflection.
void() T_MissileExplode = {
    if (!self.dmg)
        self.dmg = 120;

    if (self.enemy)     // Don't check health, just if there is a target.
    {
        // History lesson:  Doom's rockets did 1d8*20 (20-160) damage on
        // a direct hit, and did full radius damage to everyone.
        // Cyberdemon took full damage from direct hits, but none from
        // any splash.
        local   float   damage;

        if (self.worldtype == MDAM_TWENTY)
        {
            if (self.dmg < 20)
                damage = random() * self.dmg;
            else
                damage = (self.dmg - 20) + (random() * 20);
        }
        else if (self.worldtype == MDAM_FULL)
            damage = self.dmg;
        else if (self.worldtype == MDAM_GRENADE)
            damage = SUB_ExploDamage (other, self, self.dmg);
        else
            damage = SUB_RandomDamage (self.dmg);
    // PM:  For rewritten shal-rath missiles.
        if (self.style & DF_ZKILL)
            if (self.enemy.classname == "monster_zombie")
                damage = self.enemy.health + 50;
    //- - - - - - - - -
        T_Damage (self.enemy, self, self.master, damage, DAMARMOR);
    }
    // Don't do radius damage to the enemy, because all the damage
    // was done in the impact.
    T_RadiusDamage (self, self.master, self.dmg, self.enemy, DAMAGEALL);
// DISABLED -- Not sure I want to do fragmentation this way.
//    if (self.th_win)
//        self.th_win ();   // For fragmenting attacks such as gugs' bilebombs.

// PM:  Don't set origin directly during a touch function because of
// possible link scrambling.  May cause FitzQuake to crash otherwise.
    self.origin = self.origin - 8*normalize(self.velocity);
    if (self.think1) {
        // Alternate explosion code defined, so use that instead.
        self.think1();
    } else {
        // Standard explosion procedure.
        Rocket_Explosion1 (self.origin);
        BecomeExplosion2 ();
    }
// PM:  In classic Doom, target took damage from both impact AND explosion.
//  The only exception was if the target was either the cyberdemon or
//  spider mastermind, both of whom had immunity to splash damage.
};

//==========================================================================
//  Light Zero

// Custom addition by PM.  Used by storm code, most of it in 'drake_cool.qc'.
//
// Since much of the level is lit up using lightstyle 0, changing it
// will make much of the level brighter or darker.  Useful for lightning
// storm effects.

float   lightzero_finished;
float   lightzero_level;
float   lightzero_rate;

float   LIGHT_ZERO          = 0;    // Lightstyle 0.
float   LIGHTZERO_NORMAL    = 12;   // Use for light "m".
float   LIGHTZERO_MIN       = 4;    // Minimum light when storming.
float   LIGHTZERO_MAX       = 25;   // Maximum light allowed.

// Calls 'lightstyle(0, <foo>)'.
void() LightZero_Update =
{
//    dprint(ftos(lightzero_level)); dprint("\n");
    lightzero_level = floor (lightzero_level);
    if (lightzero_level < 5)
    {
        if (lightzero_level < 1)
            lightstyle(LIGHT_ZERO, "a");
        else if (lightzero_level == 1)
            lightstyle(LIGHT_ZERO, "b");
        else if (lightzero_level == 2)
            lightstyle(LIGHT_ZERO, "c");
        else if (lightzero_level == 3)
            lightstyle(LIGHT_ZERO, "d");
        else //if (lightzero_level == 4)
            lightstyle(LIGHT_ZERO, "e");
    }
    else if (lightzero_level < 10)
    {
        if (lightzero_level == 5)
            lightstyle(LIGHT_ZERO, "f");
        else if (lightzero_level == 6)
            lightstyle(LIGHT_ZERO, "g");
        else if (lightzero_level == 7)
            lightstyle(LIGHT_ZERO, "h");
        else if (lightzero_level == 8)
            lightstyle(LIGHT_ZERO, "i");
        else //if (lightzero_level == 9)
            lightstyle(LIGHT_ZERO, "j");
    }
    else if (lightzero_level < 15)
    {
        if (lightzero_level == 10)
            lightstyle(LIGHT_ZERO, "k");
        else if (lightzero_level == 11)
            lightstyle(LIGHT_ZERO, "l");
        else if (lightzero_level == 12)
            lightstyle(LIGHT_ZERO, "m");
        else if (lightzero_level == 13)
            lightstyle(LIGHT_ZERO, "n");
        else //if (lightzero_level == 14)
            lightstyle(LIGHT_ZERO, "o");
    }
    else if (lightzero_level < 20)
    {
        if (lightzero_level == 15)
            lightstyle(LIGHT_ZERO, "p");
        else if (lightzero_level == 16)
            lightstyle(LIGHT_ZERO, "q");
        else if (lightzero_level == 17)
            lightstyle(LIGHT_ZERO, "r");
        else if (lightzero_level == 18)
            lightstyle(LIGHT_ZERO, "s");
        else //if (lightzero_level == 19)
            lightstyle(LIGHT_ZERO, "t");
    }
    else
    {
        if (lightzero_level == 20)
            lightstyle(LIGHT_ZERO, "u");
        else if (lightzero_level == 21)
            lightstyle(LIGHT_ZERO, "v");
        else if (lightzero_level == 22)
            lightstyle(LIGHT_ZERO, "w");
        else if (lightzero_level == 23)
            lightstyle(LIGHT_ZERO, "x");
        else if (lightzero_level == 24)
            lightstyle(LIGHT_ZERO, "y");
        else //if (lightzero_level == 25)
            lightstyle(LIGHT_ZERO, "z");
    }
};

void() LightZero_Go =
{
    if (!lightzero_finished)
        return;

    if (lightzero_finished > time)
    {
        lightzero_level = lightzero_level + lightzero_rate;
        if (lightzero_level < LIGHTZERO_MIN)
            lightzero_level = LIGHTZERO_MIN;
        else if (lightzero_level > LIGHTZERO_MAX)
            lightzero_level = LIGHTZERO_MAX;
    }
    else
    {   // Restore normal lighting.
        if (lightzero_level > LIGHTZERO_NORMAL)
        {   // Was brighter.
            lightzero_level = lightzero_level - 4;
            if (lightzero_level < LIGHTZERO_NORMAL)
                lightzero_level = LIGHTZERO_NORMAL;
        }
        else if (lightzero_level < LIGHTZERO_NORMAL)
        {   // Was darker.
            lightzero_level = lightzero_level + 2;
            if (lightzero_level > LIGHTZERO_NORMAL)
                lightzero_level = LIGHTZERO_NORMAL;
        }
        if (lightzero_level == LIGHTZERO_NORMAL)
            lightzero_finished = 0;     // End it!
    }

    LightZero_Update ();
};

void(float lite, float add, float rate) LightZero_Set =
{
    lightzero_rate = rate;
    if (lite == -1)
    {
        if (lightzero_finished)
            lite = lightzero_level;
        else
            lite = LIGHTZERO_NORMAL;
    }
    add = time + add;
    if (lightzero_finished < add)
        lightzero_finished = add;
    if (lightzero_level != lite)
    {
        lightzero_level = lite;
        LightZero_Update ();
    }
};

//==========================================================================
//  "Storm" Lightning Strike  (a.k.a. Airstrike)

//------------------------------------------------------------------------//
//  This creates a blank entity that will zap targets in the water
//  within a radius for damage.
//
//  Update:  Added fields 'dflags' and 'dtype' for upgraded storms.
//------------------------------------------------------------------------//
void(entity attacker, vector spot, float damage, entity ignore,
    float dflags, string dtype)  Zapper_Spawn =
{
    local   entity  zap;

    zap = spawn();
    zap.movetype  = MOVETYPE_NONE;
    zap.velocity  = '0 0 0';
    zap.touch     = SUB_Null;
    zap.solid     = SOLID_NOT;
    zap.owner     = world;
    setall (zap, "", '0 0 0', '0 0 0', spot);   // invisible

    Rocket_Explosion2 (zap.origin, 244, 3);
    T_RadiusDamage (zap, attacker, damage, ignore, DAMAGEALL);

    remove (zap);
};

//------------------------------------------------------------------------//
//  This creates a lightning bolt that flashes down from the sky and
//  strikes with explosive impact.
//------------------------------------------------------------------------//
void(entity attacker, vector spot, float damage, float dflags, string dtype)
    Thunderstrike_Ex =
{
    local   vector  air, start, end;
    local   entity  targ, ignore;

    air = '0 0 2048';   // Max limit of lightning draw length possible.
    traceline (spot, spot + air, TRUE, world);
    start = trace_endpos - '0 0 2';

    // WARNING:  Using 'world' as last traceline argument does not work.
    //   Trying to do so causes the trace to ignore all, even if the
    //   third argument is FALSE.
    //
    // Create dummy entity to serve as ignore for the traceline.
    // That way, the lightning strike can hit even the attacker.
    local   entity  zap;

    zap = spawn();
    zap.movetype  = MOVETYPE_NONE;
    zap.velocity  = '0 0 0';
    zap.touch     = SUB_Null;
    zap.solid     = SOLID_NOT;
    zap.owner     = world;
    setall (zap, "", '0 0 0', '0 0 0', spot);   // invisible

    // Execute trace.
    //GhostTouch_On ();
    traceline (start, spot - air, FALSE, zap);
    targ = trace_ent;
    //GhostTouch_Off ();
    // Don't remove zap now, we need it later for thunder effects.

    // Proceed with traceline results.
    if (targ.takedamage)
    {
        // Direct hit.
        end = Midpoint (targ);
        ignore = targ;

        // Trace up from target center to new point in the sky.
        traceline (end, end + air, TRUE, world);
        start = trace_endpos - '0 0 2';

        // Damage the target.
        // NOTE:  Reflection won't save the target from the wrath of god.
        local   float   points;

        points = SUB_RandomDamage (damage);
        BoltDamage_Ex (targ, attacker, attacker, end, points, dflags, dtype);
    }
    else
    {
        end = trace_endpos;
        ignore = world;
    }

    // Create lightning and thunder effects.
    setorigin (zap, end);
    sound (zap, CHAN_AUTO, "hipweap/mjolhit.wav", 1, 0.5);  // It's loud.
    Tent_Beam (TE_LIGHTNING1, zap, end, start);    // Draw big bolt upward.
    remove (zap);   // Dummy served its purpose, now begone.

    if (pointcontents(end) == CONTENT_SKY)
        return;     // Don't explode if it hits sky below.

    // Create explosion on impact.
    Zapper_Spawn (attacker, end, damage, ignore, dflags, dtype);
    ShockWave (end + '0 0 4', 8, 4, "progs/drake/s_wave.spr");
};

void(entity attacker, vector spot, float damage) Thunderstrike =
    {Thunderstrike_Ex (attacker, spot, damage, DF_NONE, "lightning");};

void() Ice_Touch =
{
    local   float   chunks, damage;
    local   vector  spot;

    if (pointcontents(self.origin) <= CONTENT_LAVA)     // Lava or sky.
        {remove(self);  return;}
    //if (Reflected ())
        //return;

    self.touch = SUB_Null;      // Stack overflow prevention.
    chunks = TRUE;
    damage = self.dmg;
    if (other.takedamage)
    {   // Check cold resistance here instead of in the damage functions
        // because it affects how much blood is spawned.
        if (damage > 0)
        {
            spawn_touchblood (self, other, damage);
            T_Damage (other, self, self.owner, damage, DAMARMOR);
            chunks = FALSE;
        }
    }
    if (chunks)
    {   // Ice chunks.
        spawn_touchblood (self, other, 10);      // Ice chunks.
        sound (self, CHAN_WEAPON, "dragon/icehit.wav", 1, ATTN_NORM);
    }

// Become a mini-shockwave if close enough to the wall.
// In case of standard engines, just display a ring topside only.
    spot = self.origin;
    traceline (spot, spot + self.velocity, TRUE, world);  // Ignore creatures.
    if (trace_fraction >= 1.0)
        {remove (self);  return;}   // Too far from a solid surface.
    spot = trace_endpos + trace_plane_normal * 2;
    self.angles = vectoangles(trace_plane_normal);
    self.angles_y = anglemod(self.angles_y + 180);
    setorigin (self, spot);
    self.alpha = 0;
    BecomeAnyExplosion (RING2_ICE, 3, "progs/s_wave2.spr");
};

void() Ice_Think =
{
    Meteor_Trail (39, 2, FALSE);    // Function in 'fire.qc'.
    if (self.delay <= time)
        {remove (self); return;}
    if (pointcontents(self.origin) == CONTENT_LAVA)
        {remove (self); return;}

    self.nextthink = time + 0.1;
};

void(vector start, vector dir, float sped) Ice_Launch =
{
    newmis = spawn ();
    newmis.master       = newmis.owner      = self;
    newmis.movetype     = MOVETYPE_FLYMISSILE;
    newmis.solid        = SOLID_BBOX;
    newmis.classname    = "ice";
// set missile speed
    newmis.speed        = sped;
    newmis.velocity     = dir * sped;
    newmis.angles       = vectoangles(newmis.velocity);
// set missile damage
    newmis.dmg          = 9;
//    newmis.enemy        = targ;
    newmis.style        = DF_COLD | DF_SAFE;
// set missile special stuff
//    newmis.lefty        = 0;
    newmis.dest         = dir;
    newmis.oldorigin    = start;    // For proper particle trail drawing.
    newmis.alpha        = ALPHA_ICE;
// set missile duration
    newmis.touch        = Ice_Touch;
    newmis.delay        = time + 5;     // Total duration.  As spikes.
    newmis.nextthink    = 0.01;
    newmis.think        = Ice_Think;

    setall (newmis, "progs/ice.mdl", '0 0 0', '0 0 0', start);

//    launch_spike (start, dir);
//    setform (newmis, "progs/ice.mdl", VEC_ORIGIN, VEC_ORIGIN);        
//    newmis.style     = TE_KNIGHTSPIKE;
//    newmis.classname = "ice";
//    newmis.velocity  = dir*sped;
};

//==========================================================================
//  Iceballs -- Used by white wyrm.

// Iceball explosion effects, called by T_MissileTouch.
void() Iceball_Explode =
{
    sound (self, CHAN_AUTO, "weapons/shatter.wav", 1, ATTN_NORM);
    Tent_Explosion2 (self.origin, 32, 16);
    ShockWave (self.origin, RING_ICE, 3, "progs/drake/s_wave.spr");
    BecomeIcyExp ();
};

// Thinking for iceballs.  Unlike shards, balls explode instead of melting.
void() Iceball_Think =
{
// Bigger ice chunks, so spawn a beam of frost.
    Meteor_Trail (39, 2, TRUE);     // Function in 'fire.qc'.
    if (self.delay > time)
        if (pointcontents(self.origin) != CONTENT_LAVA)
            {self.nextthink = time + 0.1;  return;}

// Time to blow up.
    T_MissileTouch ();
};

// Launch an explosive icy spikeball.
void(vector start, vector dir, float sped) Iceball_Launch =
{
    Ice_Launch (start, dir, sped);

    setmodel (newmis, "progs/drake/iceball.mdl");
    setsize (newmis, '0 0 0', '0 0 0');

    newmis.avelocity = '300 300 300';
    newmis.dmg      = 60;
    newmis.alpha    = ALPHA_ICEBALL;
    newmis.touch    = T_MissileTouch;
    newmis.think    = Iceball_Think;
    newmis.think1   = Iceball_Explode;
};

void(vector start, vector dir, float sped, float damage) Iceball_Toss =
{
    Ice_Launch (start, dir, sped);

    setmodel (newmis, "progs/drake/iceball.mdl");
    setsize (newmis, '0 0 0', '0 0 0');

    newmis.movetype = MOVETYPE_TOSS;
    newmis.avelocity = '300 300 300';
    newmis.dmg      = damage;
    newmis.alpha    = ALPHA_ICEBALL;
    newmis.touch    = T_MissileTouch;
    newmis.think    = Iceball_Think;
    newmis.think1   = Iceball_Explode;
};

//==========================================================================
//  Ice Mines

// Treat destroyed mines as barrels.
void() IceMine_Die =
{
    if (self.trigger_field)
        {remove (self.trigger_field);  self.trigger_field = world;}
    self.th_die= SUB_Null;      // Don't 'die' again if damaged more.
    self.touch = SUB_Null;      // Stack overflow prevention.
    self.solid = SOLID_NOT;
    self.enemy = world;
    self.nextthink = 0.01;
    self.think = T_MissileExplode;
};

void(entity targ) IceMine_Explode =
{
    if (self.trigger_field)
        {remove (self.trigger_field);  self.trigger_field = world;}
    self.touch = SUB_Null;      // Stack overflow prevention.
    self.enemy = targ;          // For modified T_MissileExplode.
    T_MissileExplode ();
};

void() IceMine_Touch =
{
// Target must either be shootable or a creature.
    if (other.takedamage != DAMAGE_AIM)
        if (!(other.flags & FLx_CREATURE))
            return;
// Don't explode on attacker.
    if (other == self.master)
        return;

// Removed sky and reflection checks because they do not apply to land mines.
    IceMine_Explode (other);
};

void() mine1 =[ 1, mine2    ] {};
void() mine2 =[ 2, mine3    ] {
    local   float   pc;

    pc = pointcontents(self.origin);
    if (pc == CONTENT_LAVA)
        {IceMine_Explode (world);  return;}
    if (self.delay > time)
        self.think = mine2;

    if (self.flags & FL_ONGROUND)
        return;     // Attached to the ground.

// Float check.  Ice is less dense than water.
    if ((pc == CONTENT_WATER) || (pc == CONTENT_SLIME))
    {   // Float to the surface;
        self.movetype = MOVETYPE_FLY;
//        self.flags = self.flags - (self.flags & FL_ONGROUND);
        self.velocity_z = self.velocity_z + 48;
        if (self.velocity_z > 48)
            self.velocity_z = 48;
    }
    else
        self.movetype = MOVETYPE_TOSS;
};
void() mine3 =[ 3, SUB_Null ] {IceMine_Explode (world);};

// Made so monsters who step on the iceball will explode it.
void() IceMineTrigger_Touch =
{
    local   entity  ice, swap;

    ice = self.owner;

    if (!ice || (ice == self))
        {remove (self);  return;}

    if (!(other.flags & FL_MONSTER))
        return;     // Other things should be able touch the iceball itself.
//    if (other == ice.owner)
//        return;     // Iceball owner is immune.
    if (other == ice.master)
        return;     // Iceball owner is immune.
    if (!(ice.flags & FL_ONGROUND) || (ice.velocity != '0 0 0'))
        return;     // Anything can touch iceball if it's moving.

// Set off the mine!
    if (other)
        other.flags = other.flags - (other.flags & FL_ONGROUND);
    self.touch = SUB_Null;
    swap = self;
    self = ice;

    self.trigger_field = world;
    self.touch = SUB_Null;      // Stack overflow prevention.
    self.enemy = other;         // For modified T_MissileExplode.
    self.owner = other;         // Pass through the mine.
    T_MissileExplode ();

    self = swap;
    remove (self);
};

void() IceMineTrigger_Think =[ 0, IceMineTrigger_Think ]
{
    if (self.owner)
        setorigin (self, self.owner.origin);
};

void(vector start) IceMine_Spawn =
{
    local   entity  ice, swap;

// Spawn the ice mine.
    ice = spawn ();
    ice.owner        = world;
    ice.master       = self;
    ice.movetype     = MOVETYPE_TOSS;
    ice.solid        = SOLID_BBOX;
    ice.classname    = "ice";
// set missile speed
    ice.velocity     = '0 0 0';
    ice.avelocity    = '0 0 0';
    ice.angles       = '0 0 0';
// set missile damage
    ice.dmg          = 90;
    ice.style        = DF_COLD;
// set miscellaneous
    ice.health       = 25;   // Barrels use 20 hp.
    ice.takedamage   = DAMAGE_YES;
    ice.bloodtype    = CHUNKS_ICE;
    ice.hittype      = HIT_STONE;
    ice.th_die       = IceMine_Die;
    //ice.resist_cold  = 1;    // Immune to cold.
    //ice.resist_fire  = -1;   // Fire melts ice!
// set missile duration
    ice.touch        = IceMine_Touch;
    ice.think1       = Iceball_Explode;
    ice.frame        = 0;
    ice.alpha        = ALPHA_ICEBALL;
    ice.think        = mine1;
    ice.nextthink    = time + 0.1;
    ice.delay        = time + 30;

    setall (ice, "progs/drake/iceball.mdl", '-16 -16 0', '16 16 16', start);

// Set the mine on the floor.
//    if (pointcontents(start) == CONTENT_EMPTY)
    {
        swap = self;
        self = ice;
        droptofloor();
        self = swap;
    }

// Spawn touch field.
// With solid_bbox and movetype_toss, monsters can walk on the iceball
// without exploding it, which is unfair.  Therefore, place a trigger
// field over the iceball so anytime a monster tries to walk on the ice,
// the iceball will explode as expected.
    swap = spawn ();
    swap.master     = swap.owner    = ice;
    swap.movetype   = MOVETYPE_NONE;
    swap.solid      = SOLID_TRIGGER;
    swap.nextthink  = time + 0.1;
    swap.think      = IceMineTrigger_Think;
    swap.touch      = IceMineTrigger_Touch;
    setall (swap, "", '-16 -16 0', '16 16 32', start);

    ice.trigger_field  = swap;      // Link touch field to the iceball.
    newmis = ice;       // In case we need to modify the mine later.

// Spawning effects.
    sound (ice, CHAN_AUTO, "weapons/freeze.wav", 1, ATTN_NORM);
    ShockWave (ice.origin + '0 0 4', RING_ICE, 3, "progs/drake/s_wave.spr");
};

//==========================================================================
//  Tornado -- Used by the wind baron.

// Sucks in targets near the cone, and damages those already in it.
void() Tornado_Attack =
{
    local   entity  head;
    local   vector  delta, spot, vel;
    local   float   hit;

// Note:  Walls offer no protection against the baron's tornado.
    head = findradius (self.origin, 256);
    while (head)
    {
        if (head != self)
        if (head != self.master)
        if (head.origin_z + head.maxs_z >= self.origin_z)   // Head must be above spawnpoint.
        {
            // Get horizontal distance between target and center.
            spot = head.origin;  spot_z = self.origin_z;
            delta = self.origin - spot;     // delta_z should be 0.

            // First, inflict some damage.
            hit = (vlen(delta) < 100);
            if (hit)
                T_Damage (head, self, self.master, self.dmg, DAMARMOR);

            // Next, suck the target into the vortex if possible.
            if ( ((head.flags & FLx_CREATURE) && (head.mass && (head.mass <= MASS_HEAVY)))
            || (head.xfl & XFL_WIND) )
            {
                if (hit || (head.flags & FL_MONSTER))
                {   // Sweep the target off its feet.
                    if (!hit)
                        if (head.flags & FL_ONGROUND)
                            if (head.velocity_z < 80)
                                head.velocity_z = 80;
                    // Walkers lose control of movement.
                    head.flags = head.flags - (head.flags & FL_ONGROUND);
                }

                vel = head.velocity;
                vel_x = vel_x * 0.5;
                vel_y = vel_y * 0.5;
                if (hit)
                {
                    vel_z = vel_z * 0.5;
                    if (vel_z < 150)
                        vel_z = 150;    // Float up and away.
                }
                head.velocity = vel + normalize(delta) * 100;
            }
        }

        head = head.chain;
    }
};

// Thinking for the smoke particles in the tornado.
void() tornado2 =[ 0, tornado2 ]
{
    if (self.delay <= time)
    {
        remove (self);
        return;
    }

    local   vector  spot, trig, dir;

    self.speed = self.speed + 8;
    self.height = self.height + 24;
    self.cnt = anglemod (self.cnt + 47);
    spot = self.oldorigin;
    spot_z = spot_z + self.height;
    trig = CoSine (self.cnt);
    dir = (('1 0 0' * self.speed) * trig_x) + (('0 1 0' * self.speed) * trig_y);
    spot = spot + dir;

    setorigin (self, spot);
};

// Think function for the tornado.  Spawns all of its effects.
void() tornado1 =[ 0, tornado1 ]
{
    if (self.delay <= time)
    {
        super_active = FALSE;
//        StopSound (self, 6);
        remove (self);
        return;
    }

// FIXME:  Use different sound that loops?
    if (self.t_width < time)
    {
        self.t_width = time + 1;
        sound (self, CHAN_AUTO, "baron/tornado.wav", 1, ATTN_NORM);
    }

    Tornado_Attack ();

    self.height = (self.height + 3) & 7;
    self.cnt = anglemod(self.cnt + 107);

    newmis = spawn();
    newmis.owner      = self;
    newmis.movetype   = MOVETYPE_NOCLIP;
    newmis.solid      = SOLID_NOT;
    newmis.velocity   = '0 0 0';
    newmis.nextthink  = 0.01;
    newmis.delay      = time + 1.2;     // FIXME:  Use T+1?
    if (newmis.delay > self.delay)
        newmis.delay      = self.delay;     // Last no longer than parent.
    newmis.think      = tornado2;
    newmis.cnt        = self.cnt + self.height;
    newmis.oldorigin  = self.origin;
    setall (newmis, "progs/smoke.mdl", '0 0 0', '0 0 0', self.origin);
};

// Spawns an immobile tornado that sucks in and damages targets.
void(vector start) Tornado_Spawn =
{
    if (super_active)
        return;     // Uses many edicts and particles, only one at a time.

    traceline (start, start - '0 0 256', TRUE, self);
    if (trace_fraction == 1)
        return;     // No ground.

    start = trace_endpos + '0 0 2';
    super_active = TRUE;    // Only one big effect at a time please.

    local   entity  wind;

// Spawn the tornado.
    wind = spawn ();
    wind.owner        = world;
    wind.master       = self;
    wind.movetype     = MOVETYPE_NONE;    // Immobile.  No MOVETYPE_FLY.
    wind.solid        = SOLID_NOT;
    wind.classname    = "tornado";
// set missile speed
    wind.velocity     = '0 0 0';
// set missile damage
    wind.dmg          = 2;      // 20/sec * 5 sec = 100.
// set miscellaneous
    wind.takedamage   = DAMAGE_NO;
// set missile duration
    wind.touch        = SUB_Null;
    wind.frame        = 0;
    wind.think        = tornado1;
    wind.nextthink    = time + 0.1;
    wind.delay        = time + 5;
    wind.t_width      = 0;

    setall (wind, "", '0 0 0', '0 0 0', start);

// Spawning effects.
    ShockWave (wind.origin + '0 0 4', RING_QUAKE, 4, "progs/drake/s_wave.spr");
};

// Was named 'Knightball_Explode'.
void() Orange_Explosion =
{
    local   float   r;

    r = random() * 3;
    if (r < 1)
        Rocket_Explosion2 (self.origin, 224, 16);
    else if (r < 2)
        Rocket_Explosion2 (self.origin, 230, 8);
    else
        Rocket_Explosion2 (self.origin, 232, 8);
    BecomeExplosion ();
};

//==========================================================================
//  Teleglitter

// NOTE:  We should not emulate Heretic's algorithm for teleglitter spawners
// because that produces too many edicts, which can cause packet overflow
// in standard engines.  Therefore, particles are sometimes spawned instead
// of sprites, and glitter sprites start with some velocity so they can
// collide with the ceiling sooner and disappear.

void() glitter_think =
{
    if (self.frame >= self.waitmax)
    {
        if (self.cnt > 5)   // 5 rewinds give 3 sec. life -- long enough.
            {remove (self);  return;}
        self.cnt = self.cnt + 1;
        self.frame = self.waitmin;
    }
    else
        self.frame = self.frame + 1;

    if (self.height)
    {   // Accelerate.
        self.speed = self.speed + self.height;
        self.velocity = self.movedir * self.speed;
    }

    self.nextthink = time + self.delay;
};

void() make_glitter =
{
// Show some fireworks.
    local   vector  spot, vel;
    local   float   rgb;

    spot_x = self.absmin_x + (random() * self.size_x);
    spot_y = self.absmin_y + (random() * self.size_y);
    spot_z = self.absmin_z + (random() * self.size_z);
    vel = self.movedir * self.speed;
    rgb = self.count;   //(serverflags & 15) * 16;

    if (self.lefty)
    {
    // Show particles only if the client can see the spawn point.
    // We need this check because in software mode (WinQuake),
    // a client can see particles through non-world wall entities
    // if said client is a long distance away.
        local   float   go;

        go = TRUE;
    // Don't try this in netgames.
        if (!deathmatch) {
            local   entity  targ;

            targ = checkclient ();
            if (targ) {
                traceline (targ.origin + targ.view_ofs, spot, TRUE, targ);
                if (trace_fraction < 1)
                    go = FALSE;
            } else
                go = FALSE;
        }
        if (go) {
            if (rgb > 251)
                rgb = 9;
            else if (rgb > 246)
                rgb = 225;
            else if (rgb > 243)
                rgb = 41;
            else if (rgb > 239)
                rgb = 193;
            particle (spot, vel*0.1, rgb, 10);
        }
    }
    else
    {   // Heretic-style teleglitter.
        if (rgb > 251)
            rgb = 90;
        else if (rgb > 246)
            rgb = 85;
        else if (rgb > 243)
            rgb = 80;
        else
            rgb = floor(rgb / 16) * 5;

        newmis = spawn();
        setmodel (newmis, "progs/drake/s_spark.spr");
        setorigin (newmis, spot);
        newmis.movetype     = MOVETYPE_FLY;
        newmis.solid        = SOLID_TRIGGER;
        newmis.velocity     = vel;
        newmis.delay        = 0.1;
        newmis.nextthink    = time + newmis.delay;
        newmis.think        = glitter_think;
        newmis.touch        = SUB_Remove;
        newmis.classname    = "glitter";
        newmis.frame        = rgb;
        newmis.cnt          = 0;
        // Note:  If glitter start splashing, change size to '0 0 0'.
        setsize (newmis, '-1 -1 -1', '1 1 1');
        newmis.movedir      = self.movedir;
        newmis.speed        = self.speed;
        newmis.waitmin      = newmis.frame;
        newmis.waitmax      = newmis.frame + 4;
        newmis.height       = self.height;
    }
    self.lefty = !self.lefty;   // Alternate between particles and sprites.

    self.nextthink      = time + self.wait;
    self.think          = make_glitter;
};

//============
// --> QUAKED misc_glitter (0 .5 .8) (-8 -8 -8) (8 8 8)
// Heretic-style teleglitter generator.  Use on exit or teleport pads.
// Otherwise, similar idea to air_bubbles.
//============
void() misc_glitter = {
    if (self.classname == "misc_glitter") {
        // Merely a glitter spawner.
        if (deathmatch)
            {remove (self); return;}
        setsize (self, '-16 -16 -8', '16 16 48');
    } else {
        // Teleporter destination with glitter spawnflag on.
        if (deathmatch)
            return;
        // Destinations are set 27 units above placement, so subtract
        // that much from new bbox mins/maxs_z to get proper bounds.
        // Update:  z - 26 should do.
        setsize (self, '-16 -16 -34', '16 16 22');
        //self.angles = '0 0 0';
    }
    precache_model ("progs/drake/s_spark.spr");

    if (!self.wait)
        self.wait   = 0.1;
    if (!self.count)        // Palette color.
        self.count  = 244;  // Default is bright electric blue.
    if (!self.speed)
        self.speed  = 100;
    if (!self.height)
        self.height = 50;   // Acceleration per frame for teleglitter.

// Sparks should fly up by default.  An angle of 0 is assumed to mean
// undefined, so to force east flight path, use a yaw of 360 instead.
    if (self.angles != '0 0 0')
        SetMovedir ();
    else
        self.movedir = '0 0 1';

    self.nextthink  = time + 1;
    self.think      = make_glitter;
};

//- - - - - - - - -
// Creates a glitter spawner.  Called by spawn functions of other entities
// such as teleporter and changelevel triggers, and used by them to create
// glittering teleporter pads like in Heretic.
//- - - - - - - - -
void() Glitter_Spawner =
{
    if (deathmatch)
        return;     // Disabled, so don't waste time spawning an entity.

    local   entity  test;
    local   vector  spot;

    spot = Anypoint (self, '0.5 0.5 0');    //Midpoint (self);
    spot_z = spot_z + 8;
    test = spawn();
    test.classname  = "misc_glitter";
    test.count      = self.count;       // Glitter color
    setorigin (test, spot);
    SUB_Think (test, misc_glitter);
};