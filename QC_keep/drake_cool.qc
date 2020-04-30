// Special effects.  Some of which are used by the Bane.

//==========================================================================
//  Shadows

// Trailing shadows, like Johnny Cage shadow kick, or Akuma teleport.
void() bane_shadow_think =
{
    self.alpha = self.alpha - 0.2;   //0.25
    if (self.alpha <= 0.001)
        remove (self);
    else
        self.nextthink = time + 0.1;
};

void() bane_shadow =
{
// Don't leave shadow afterimages if invisible.
    if (!self.modelindex)
        return;
    //if (self.modelindex == mindex_inviso)
        //return;

// Spawn an image.
    local   entity  ent;

    ent = spawn();
    ent.movetype = MOVETYPE_NONE;
    ent.velocity = '0 0 0';
    ent.touch    = SUB_Null;
    setmodel (ent, self.model);
    setorigin (ent, self.origin);
    ent.solid    = SOLID_NOT;
    ent.angles   = self.angles;
    ent.frame    = self.frame;
    if (self.classname == "monster_bane")
        ent.skin     = self.skin + 1;   // Greyscale version of Bane.
    else
        ent.skin     = self.skin;
    if (self.alpha)
        ent.alpha    = self.alpha;
    else
        ent.alpha    = 1;
    ent.think    = bane_shadow_think;
    SUB_Think (ent, ent.think);
};


//==========================================================================
//  Earthquakes

// Based on Hexen2 earthquake code, modified to behave more like Quoth's.
// Works for me.

// Hexen2 quakes inflicted the least damage and knockback at the center
// of a quake, and the most at the edge.  In Quoth, this is the reverse.
//
// Changed to behave more like Quoth's quake behavior.
//
// More damage at center like in Quoth, reverse of Hexen2.
//

// Checks if entity is unable to fly.
float(entity ent) Grounded =
{
    if (ent.movetype != MOVETYPE_FLY)
        if (ent.movetype != MOVETYPE_NOCLIP)
            if (ent.movetype != MOVETYPE_FLYMISSILE)
                if (!(ent.flags & FL_FLY))
                    if (!ent.wing_finished)  // Boots provide stability.
                        return TRUE;

    return FALSE;   // I can fly!
};

// Returns TRUE if n = 1,3,6,10,15,etc.
float(float n) Quake_Ping =
{
    local   float   add, sum;

    if (n > 15)
        return FALSE;

    sum = 1;  add = 2;
    while (sum < n)
    {
        sum = sum + add;
        add = add + 1;
    }
    return (sum == n);
};

void() Quake_Think = [ 0, Quake_Think ]
{
    local   entity  head;
    local   vector  org, dir;
    local   float   radius;
    local   float   magnitude;     // Richter
    local   float   dist;
    local   float   frac;
    local   float   up, upper;
    local   float   damage, dflags;
    local   float   hit_me, hurt_me;
    local   float   dizzy;
// Everyone will have their view shaken.  After all, if the very earth
// shakes, the view will shake even if the viewer can remain perfectly still.

// Flyers and ghosts should be immune to damage.
// Flyers, ghosts, and immobile objects should not be thrown around.
// The blessed may avoid damage and chaos.

    if (self.delay <= time)
    {
        if (!self.spawnflags)
            sound (self, CHAN_VOICE, "misc/quakeend.wav", 1, ATTN_NORM);
        remove (self);
        return;
    }

// Set damage counters to emulate Quoth's earthquake behavior.
// That is, hit on 1st, 3rd, 5th, 7th, and 9th of the whole quake.
// Update:  My method, hit on 1st, 3rd, 6th, 10th, and 15th frames.
    hurt_me = FALSE;
    self.count = self.count + 1;
//    if ((self.count & 1) && (self.count < 10))  // Quoth method.
    // My method.
    if (!(self.style & DF_NO_KNOCK))    // This line is a hack for gug death.
        if (Quake_Ping (self.count))
            hurt_me = TRUE;

// Damage flags:
// No React  - So monsters don't get mad at unknown attacker.
// No Knock  - Quake adds velocity, so don't let T_Damage kb stack.
    dflags = DF_NO_REACT | DF_NO_KNOCK;
    frac = (self.delay - time) / self.wait;     // Fraction, from 0 to 1.
    radius = self.distance;
// Update:  Quake gets weaker in Hexen2.  Not in Quoth, and not here.
    magnitude = self.dmg;   // * frac;

    if (radius)
        head = findradius (self.origin, radius);
    else
        head = findradius (self.origin, 15000);     // Pick up everyone!
    while (head)
    {
        if (head.modelindex)
        if ((head.solid >= SOLID_TRIGGER) && (head.solid != SOLID_BSP))
        if (head.mass && (head.mass <= MASS_HEAVY))
        {
            hit_me = TRUE;
            if (head == self.owner)
            {   if (self.style)     // Safe or blessed.
                    hit_me = FALSE;
            }
            //else if (self.style & DF_BLESSED)
            //{   //if (Ally_Check (head, self.owner))
                //    hit_me = FALSE;
            //}

            dizzy = TRUE;
            if (head.flags & FL_MONSTER)
                if (!head.fighting)
                    dizzy = FALSE;    // Don't drift -- take damage only.

            org = Midpoint (head);
            if (radius)
                dist = 1 - (vlen(self.origin - org) / radius);
            else
                dist = 1;
            upper = 80;     // One-tenth of normal gravity.

        // Shake view.  Only creatures can see.
            if (head.flags & FLx_CREATURE)
            {
                if (head.classname == "player")
                {   // FIXME:  Cameras in cutscene won't shake.
                    upper = 100;
                    // FIXME:  Should this vary by magnitude?
                    head.punchangle_x = crandom() * self.mangle_x;
                    head.punchangle_y = crandom() * self.mangle_y;
                    head.punchangle_z = crandom() * self.mangle_z;
                    // Update 8/2/09:  Dampen tilt if enabled.
// DISABLED... for now.
//                    if (0)
//                        head.punchangle = head.punchangle * frac;
//                    if (0)
//                        head.punchangle = head.punchangle * dist;
                }
                else if (head.flags & FL_MONSTER)
                {
                    if (dizzy)
                    if (!IsDead (head))
                    {   // Mess with their yaw to feign disorientation.
                        head.angles_y = head.angles_y + (crandom() * 20);
                        head.angles_y = anglemod (head.angles_y);
                        // Unlike Hexen2, don't bother with pitch and roll.
                    }
                }
            }

        // Check for damage and velocity.  Flyers and ghosts are immune,
        // as is the attacker and possibly his allies.  Swimming in the
        // water won't save thee.
            if (hit_me)
            if (Grounded (head))
            {
                damage = magnitude * dist;

            // Inflict damage.  If head is not a designated flyer, take
            // damage even if not onground so clever players cannot
            // escape damage by jumping like a pogo-stick.
            //
            // Note:  Hexen2 had 20% chance per think of inflicting damage.
            // In Quoth, quake caused damage up to five times in first sec.
            //
            // 5/7/2009:  Call damage function only if there's damage or
            // target is a monster.  Monsters may flinch from zero damage,
            // and flinching works well for disorientation effects.
                if (hurt_me)
                    if (damage || (head.flags & FL_MONSTER))
                        T_Damage (head, self, self.owner, damage, DAMARMOR);

            // Add movement.
            // Note:  Too much trouble discriminating which objects can be
            // moved without problems, so move only creatures for now.
                if ((head.flags & FLx_CREATURE) || (head.xf2 & XF2_QUAKE))
                {
                    if (head.flags & FL_ONGROUND)
                    {   // Bump target up a bit.  We need this for players at
                        // least for impaired movement.  Monsters will thump
                        // a lot, if z is too high, and that sucks.  Hmmm...
                        up = ((random() * 100) + 25) * dist;
                        if (up > upper)
                            up = upper;
                        head.velocity_z = head.velocity_z + up;
                    }
//                    if (1)
//                    {   // Move toward/away from epicenter.
//                        dir = org;
//                        dir_z = self.origin_z;
//                        dir = normalize (self.origin - dir);    // Toward
//                        dir = normalize (dir - self.origin);    // Away
//                    }
//                    else
                    {   // Random drifting.
                        dir = '0 1 0' * (random() * 360);
                        makevectors (dir);
                        dir = v_forward;
                    }
                    // dir_z is already 0, so no need for 'dir_z = 0;'.
                    // Note:  vel*25 from Hexen2 is too much, so use *8.
                    // Update:  Monster does not drift if not fighting.
                    if (dizzy)
                        head.velocity = head.velocity + (dir * (damage * 8 * random()));
                    head.flags = head.flags - (head.flags & FL_ONGROUND);
                }
            }
        }
        head = head.chain;
    }

// Zer quakes ping once every 0.15 sec, but I'll use standard 10 fps
// in Drake.  Quakes in Quoth seem to ping at least as quickly.
// DISABLED -- 10 fps stuff done in anim. macro above.
//    self.nextthink = time + 0.1;
};

// Note:  Quake can ping for 'damage' up to five times if it lasts
// long enough, at least 1.5 seconds.
void(entity attacker, float radius, float damage, vector spot, float dflags,
    float secs, vector tilt)  Quake_Spawn =
{
    local   entity  quaker;
    local   float   attn;   //, magnitude;
    local   string  dtype;

    radius = fabs(radius);
//    magnitude = damage / 5;     // Was 'radius / (radius * 5 / damage)'.
    if (radius)
    {
// Update 5/9/2009:  Disabled default damage because we may want none!
//        if (damage < 1)
//            magnitude = 10;     // Default of 50 damage at ground zero.
        attn = 1000 / (radius + 200);
        if (attn > 4)
            attn = 4;   // Max allowed by engine.
    }
    else    // Quake hits whole level.
        attn = ATTN_NONE;

// Inflictor is 'self'.  If inflictor is attacker, don't harm him.
    if (self == attacker)
    {
        dflags = dflags | DF_SAFE;
        dtype = "quake";
    }
    else
    {   // Assume quake was spawned by a missile, so inherit its attack type.
        // Watch out for types that cause special deaths, such as fire.
        dtype = self.classname;
    }

// Spawn quake entity.
    quaker = spawn();
    quaker.movetype = MOVETYPE_NONE;
    quaker.velocity = '0 0 0';
    quaker.touch    = SUB_Null;
    quaker.solid    = SOLID_NOT;
    quaker.classname= "quake";
    quaker.deathtype= dtype;
    quaker.owner    = attacker;
    quaker.wait     = secs;
    quaker.delay    = time + quaker.wait;
    quaker.count    = 0;        // Make sure it begins at zero.
    // FIXME:  Expand style?
    quaker.style    = dflags & (DF_NO_KNOCK | DF_SAFE | DF_BLESSED);
    quaker.distance = radius;
    quaker.dmg      = damage;   //magnitude;
    quaker.mangle   = tilt;
    quaker.nextthink= 0.01;
    quaker.think    = Quake_Think;
    quaker.spawnflags   = dflags & DF_TIMED;    // Silent if true.
    setsize (quaker, '0 0 0', '0 0 0');        
    setorigin (quaker, spot);

// Update 4/9/2009:  Variable attenuation instead of ATTN_NORM.
    if (!quaker.spawnflags)
        sound (quaker, CHAN_VOICE, "misc/quake.wav", 1, attn);
};

void(float radius, float damage, vector spot, float dflags, float secs)
    Quake_Self =
    {Quake_Spawn (self, radius, damage, spot, dflags, secs, VEC_QUAKE);};

// Standard earthquake attack used by monsters, namely earth baron and gug.
// Note:  Gugs cause up to 45-50 damage with each quake.  Earth baron,
// as a boss who chooses attacks randomly, does a max of 30 damage per
// quake.  30 damage is what the shambler causes with his lightning if
// it catches a target out in the open.
void(vector spot) Quake_Attack =
    {Quake_Self (800, 6, spot, DF_NONE, 3);};

// Shake entire level.
void(float damage, vector spot, float secs) Quake_Level =
    {Quake_Self (0, damage, spot, DF_NONE, secs);};


//- - - - - - - - - - - - - - - - - - -
// Update 6/4/09:  Imported Zerstorer's earthquake trigger entity for
// use in Tronyn's map, toum.bsp.  Code was modified and adapted to
// work with the earthquake code above.

/* ------------------------------ */
/* |       trigger_quake          | */
/* ------------------------------ */
/*QUAKED trigger_quake (.5 .5 .5) (-8 -8 -8) (8 8 32) NOTOUCH
Starts an earthquake.  Players view will shake for amount of time you
put in "wait".  Place the richter scale of the quake in the "dmg" field.
It will also trigger all targets like a 'trigger_once'.
*/

void() tq_remove = {SUB_UseTargets();  SUB_Remove();};

void(entity who) tq_go =
{
    if (who.health <= 0 || who.classname != "player")
        return;

// Use Drake's quake code instead.
    Quake_Self (0, 0, VEC_ORIGIN, DF_NONE, self.wait);
    tq_remove ();
};

void() tq_use = {tq_go (activator);};
void() tq_touch = {tq_go (other);};

void() trigger_quake =
{
    if (deathmatch /*|| coop*/)
    {   // This will use then remove the quake.
        self.think      = tq_remove;
        self.nextthink  = time + 0.2;
        return;
    }

// Disabled because sounds were precached by worldspawn already.
//    precache_sound ("misc/quake.wav");
//    precache_sound ("misc/quakeend.wav");
    InitTrigger ();

    self.use = tq_use;
    if (!(self.spawnflags & 1))   // PM:  Added notouch flag.
        self.touch = tq_touch;
};


//==========================================================================
//  Wrath of God

// WOG stands for 'Wrath Of God'.
float   WOG_DAMAGE  = 80;
float   WOG_RADIUS  = 140;  // Should be 'damage+60'.

// Checks if the sky is above a given point.
float(vector org) UpIsSky =
{
    local   float   pc;

    traceline (org, org + '0 0 2048', TRUE, world);
    pc = pointcontents (trace_endpos);
    if (trace_fraction < 1)
        return (pc == CONTENT_SKY);
    return ((pc == CONTENT_EMPTY) || (pc == CONTENT_SKY));
};

float(entity targ, entity attacker) Divine_Trace =
{
    if (targ.flags & FLx_CREATURE)
    if (!targ.deadflag)
    if (targ.solid)
    if (targ.modelindex)
    //if (!Ally_Check (targ, attacker))
// Replaced 'CanDamage' with an eye-to-eye visibility check.
//    if (CanDamage (targ, attacker))
    {
        local   vector  v1, v2;

        v1 = attacker.origin + attacker.view_ofs;
        v2 = targ.origin + targ.view_ofs;
        traceline (v1, v2, TRUE, attacker);   // see through other monsters
        if (trace_fraction == 1)
        {
            if (UpIsSky (targ.origin))
            {
                local   float   dist;

                dist = vlen (targ.origin - attacker.origin);
                if (dist >= WOG_RADIUS)
                    if (dist <= 2000)
                        return TRUE;
            }
        }
    }
    return FALSE;
};

// Like CheckPowerups, except this sends a lethal bolt of lightning down
// to 'ent'.
void(entity ent) Divine_Check =
{
    if (ent.divine_finished)
    if (ent.divine_finished < time)
    {
        local   entity  attacker;

        attacker = ent.divine_attacker;

        ent.divine_finished = 0;
        ent.divine_attacker = world;

        if (attacker.divine_hits > 0)
        if (Divine_Trace (ent, attacker))
        {   // Lightning can hit the target.
/* DISABLED -- (see below)
            // This plus ghost touch code causes overflow crash in huge maps
            // in FitzQuake and derivatives (e.g., QuakeSpasm).
            //
            // This would prevent lightning from striking if the blast
            // could hit allies.
            local   entity  head;

            head = findradius (ent.origin, WOG_RADIUS);
            while (head)
            {
                if (head != attacker)
                    if (head.flags & FLx_CREATURE)
                        if (Ally_Check (head, attacker))
                            if (CanDamage (head, ent))
                                return;     // No friendly fire allowed.
                head = head.chain;
            }
*/
            // Send the lightning down.  Since a diety is attacking on
            // behalf of the attacker, damage is indirect, hence proxy flag.
            // Update 6/19/2011:  Added BLESSED flag to avoid blasting
            //  allies, since the above ally check has been disabled.
            Thunderstrike_Ex (attacker, ent.origin, WOG_DAMAGE, DF_PROXY | DF_BLESSED, "divine");
            LightZero_Set (20, 0, -4);
            if (attacker)
                attacker.divine_hits = attacker.divine_hits - 1;
        }
    }
};

// Spawns a mini lightning storm.  The attacker is 'self'.
// 'shots' = Maximum bolts called down.
// 'dur' = How long the darkness lasts.
float(float shots, float dur) Divine_Wrath =
{
    local   entity  head;
    local   float   hits;
    local   vector  vx, dir;

    if (!self)
        return FALSE;
    if (self.flags & FL_CLIENT)
        vx = self.v_angle;
    else
        vx = self.angles;
    makevectors (vx);
    vx = v_forward;

    hits = 0;
    head = nextent (world);
    while (head)
    {
        if (Divine_Trace (head, self))
        {
            dir = normalize (head.origin - self.origin);
            if (dir * vx > 0.7071)      // Must be within fov 90.
            {   // Send a bolt down.  Proxy damage due to divine intervention.
                // Note:  The latest attacker overrides previous attackers.
                head.divine_attacker = self;
                head.divine_finished = hits * 0.1 + time;
                hits = hits + 1;
            }
        }
        head = nextent (head);
    }
    if (hits)   // Set maximum hits an attacker may score.
    {
        if (!shots)
            shots = 255;
        if (!lightzero_finished)
            sound (world, CHAN_AUTO, "bane/thunder.wav", 1, ATTN_NONE);
        LightZero_Set (-1, dur, -2);
        self.divine_hits = shots;   // Number of bolts in slam, so match that.
    }

    return hits;
};

//- - - - - - - - -
// Triggered lightning strikes based on Divine* functions.
void() divine_use =
{
    local   entity  swap;
    local   float   hits, stop;

    if (self.attack_finished > time)
        return;     // Previous attack unfinished.

    swap = self;

// Spawn dummy entity to act as the attacker.
// Dummy is placed where activator is so dummy can see targets.
    self = spawn();
    //self.alignment  = activator.alignment;  // Don't hit activator.
    self.angles     = activator.angles;     // Use to find activator's targets.
    setorigin (self, activator.origin);
    hits = Divine_Wrath (swap.cnt, 1);
    stop = time + 0.2 + (hits * 0.1);
    if (hits)
    {   // Lightning strike is delayed, so delay dummy removal.
        self.nextthink  = stop;
        self.think      = SUB_Remove;
    }
    else
    {   // No lightning strikes, so just flash and thunder ominously.
        sound (world, CHAN_AUTO, "bane/thunder.wav", 1, ATTN_NONE);
        LightZero_Set (LIGHTZERO_MAX, 0, -4);
        remove (self);
    }

// Restore self.
    self = swap;
    self.attack_finished = stop;

// Now function like trigger_relay.
    SUB_UseTargets ();
};

void() trigger_smite =
{
    precache_sound ("bane/thunder.wav");
    precache_sound ("hipweap/mjolhit.wav");

    self.attack_finished = 0;
    if (!self.cnt)
        self.cnt = 1;
    self.use = divine_use;
};
//- - - - - - - - -


//==========================================================================
//  Thunderstorm

// DISABLED! -- Code for Bane-style storms usable by players.
//.entity     stormed;        // Storm's current target saved by attacker.

// Sets storm's next target.
//entity(entity attacker) Storm_NextEnemy =
//{
//    local   entity  targ;
//
//    targ = nextent (attacker.stormed);
//    if (!targ)
//        targ = nextent (world);
//    attacker.stormed = targ;
//    do
//    {
//        if (Divine_Trace (targ, attacker))
//            {attacker.stormed = targ;  return targ;}
//        targ = nextent (targ);
//        if (!targ)
//            targ = nextent (world);
//    } while (targ != attacker.stormed);
//
//    return world;
//};

// Updates thunderstorm's targeting data.
void() bane_storm_target =
{
//    if (self.owner.classname == "player")
//        self.enemy = Storm_NextEnemy (self.owner);
//    else
    {   // Monsters cycle through their known enemies.
        if (self.enemy == self.owner.enemy)
        {   if (self.owner.oldenemy)
                self.enemy = self.owner.oldenemy;
        }
        else
            self.enemy = self.owner.enemy;
    }
    self.dest = self.enemy.origin;
};

void() bane_storm_end =
{
    if (self.owner)
        if (self.owner.owner == self)
            self.owner.owner = world;
    self.owner = world;
    SUB_Remove ();
};

void() bane_storm1  =[ 0, bane_storm2  ] {self.nextthink = time + 0.3;};
void() bane_storm2  =[ 0, bane_storm3  ] {self.nextthink = time + 0.4; self.dest = self.enemy.origin;};
void() bane_storm3  =[ 0, bane_storm4  ] {
    // Abort if spellcasting was interrupted.
    if (( self.owner.frame <= self.waitmin ) || ( self.owner.frame >= self.waitmax ) )
        {self.think = bane_storm_end;  return;}
};
void() bane_storm4  =[ 0, bane_storm5  ]
{
// First, check if we should even call an airstrike.
    local   float   zot;

    zot = FALSE;
    if (self.enemy)
    {
        local   vector  p1, p2;

//        if (self.ryuflags & RYU_ALERT)
//            self.dest = self.enemy.origin;

        if (UpIsSky (self.dest))
            zot = TRUE;
        else
        {   // Under shelter, so find another point nearby that isn't.
            local   float   loop;

            loop = 5;
            p1 = self.dest;
            while (loop > 0)
            {
                loop = loop - 1;
                p1_x = crandom() * 256 + self.dest_x;
                p1_y = crandom() * 256 + self.dest_y;
                traceline (self.dest, p1, TRUE, self);
                if (trace_fraction >= 1.0)
                    if (UpIsSky (p1))
                        {self.dest = p1;  zot = TRUE;  loop = 0;}
            }
        }
        // Note:  Bane can't hurt himself with Tstorm.
        if (self.owner)
            if (self.owner.classname != "monster_bane")
            {
                p1 = self.owner.origin;
                p2 = self.dest;
                p2_z = p1_z;
                if (vlen(p2 - p1) < (self.dmg + 60))    //Was 150.
                    zot = FALSE;    // Too close.
            }
    }

// Now start for real.
    self.cnt = self.cnt - 1;
    if (zot)
    {
        LightZero_Set (16, self.delay, 0);
        Thunderstrike (self.owner, self.dest, self.dmg);
        self.pain_finished = time + 3;
    }
    else
    {   // No bolt -- just flash and thunder.
        zot = random();
        if (zot < 0.5)
            zot = 0.5;
        else
            LightZero_Set (13, self.delay, 0);
        if (random() < 0.5)
            sound (self, CHAN_AUTO, "ambience/thunder1.wav", zot, ATTN_NONE);
        else
            sound (self, CHAN_AUTO, "ambience/thunder2.wav", zot, ATTN_NONE);
        self.pain_finished = time + 5;
    }
    bane_storm_target ();
    self.attack_finished = time + self.delay;
};
void() bane_storm5 =[ 0, bane_storm6 ] {LightZero_Set (12, self.delay, -4); self.nextthink = self.attack_finished;};
void() bane_storm6 =[ 0, bane_storm_end  ]
{
    if (self.owner.deadflag >= DEAD_DEAD)
        return;     // Spell is broken.

    if (self.cnt > 0)
    {
        local   float   abort;

        abort = FALSE;
        if (random() < self.volume)
            abort = TRUE;
        if (!self.enemy)
            if (self.pain_finished > time)
                abort = TRUE;

        if (abort)
        {
            LightZero_Set (-1, self.delay, -4);
            self.cnt        = self.cnt - 1;
            self.think      = bane_storm6;
            self.nextthink  = time + self.delay;
            bane_storm_target ();
        }
        else
            bane_storm4 ();
    }
};

void(float frmin, float frmax) bane_storm =
{
    local   entity  magic;

// FIXME:  Check if 'self' already has a storm active.
    magic = spawn();
    setorigin (magic, self.origin);     // Sound comes from here.
    magic.nextthink = self.nextthink;   // Synch with sword chop anim.
    magic.think = bane_storm1;

    magic.owner = self;
    magic.enemy = self.enemy;
    magic.delay = 0.5;      // Delay between strikes.
    magic.cnt   = 6;        // Max number of zaps.  Each is 0.5 sec apart.
    magic.dmg   = 80;       // Used to be 90.
    magic.classname = "lightning";
    magic.waitmin = frmin;
    magic.waitmax = frmax;
    magic.volume  = 0.5;    // 50% chance of no bolt after the first.
    sound (magic, CHAN_AUTO, "bane/thunder.wav", 1, ATTN_NONE);
    magic.pain_finished = time + 3;
    magic.attack_finished = 0;

    if (self)
        self.owner = magic;

    LightZero_Set (-1, 1, -1);
};

float() bane_has_storm =
{
    if (self.owner)
    {
        if (self.owner.owner == self)
           return TRUE;
        // ELSE ... entity is probably a clone and got it from its parent.
    }
    return FALSE;
};

/* DISABLED! -- Old storm proc.
// Checks if 'self' can activate a thunderstorm.  Used by clients wielding
// the Mjolnir.  Also activated by storm cheat.
float() ActivateThunderStorm =
{
    if (!self.owner)
    {
        local   entity  e;

        e = nextent (world);
        while (e)
        {   // If even one target is found, start the storm.
            if (Divine_Trace (e, self))
            {
                bane_storm (-1, 256);   // Uninterruptable storm.
                if (self.flags & FL_CLIENT)
                {
                    self.owner.enemy    = Storm_NextEnemy (self);
                    self.owner.cnt      = 10;   // Override default of 6.
                    self.owner.ryuflags = RYU_ALERT;    // Perfect accuracy.
                }
                return TRUE;
            }
            e = nextent (e);
        }
    }
    return FALSE;
};
*/