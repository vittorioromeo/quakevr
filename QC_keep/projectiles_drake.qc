void() Axe_Watch;
//==========================================================================
//  Archer Knight Arrows

// Update 11/24/09:  Constants moved to defs.

// Magic arrow explosion code.
void() AkArrow_Explode =
{
    self.count = self.count & 3;
    sound (self, CHAN_AUTO, "archer/explode.wav", 1, ATTN_NORM);
    Colored_Explosion (self.origin, self.count);
    BecomeFancy (X80_SPARK_ORANGE + (self.count * 6), 6);
};

void() AkArrow_Magic_Touch =
{
// Other, more powerful and explosive arrow types.
    if (self.worldtype)
    {
        if (other.flags & FLx_CREATURE)
        //if (!Ally_Check (other, self.master))
        //if (!HasReflection (other, self.origin, 0))
        if (other.mass && (other.mass <= MASS_SUPER))
        {   // Knockback effect.
            local   float   sped;

            sped = self.speed;
            if (other.mass == MASS_SUPER)
                sped = sped / 2;
            other.velocity = other.velocity + (self.movedir * sped);
            if (other.flags & FL_ONGROUND)
            {
                other.flags = other.flags - FL_ONGROUND;
                if (other.velocity_z < 150)
                    other.velocity_z = 150;
            }
        }
    }
    T_MissileTouch ();
};

// Called when an arrow hits something.
void() AkArrow_Wood_Touch =
{
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}
    //if (Reflected ())
        //return;

    self.touch = SUB_Null;      // Stack overflow prevention.
    if (other.takedamage)
    {
        spawn_touchblood (self, other, self.dmg);
        T_Damage (other, self, self.master, self.dmg,DAMARMOR);
        sound (self, CHAN_WEAPON, "archer/arrow1.wav", 1, ATTN_IDLE);
        remove (self);
        return;
    }
    if (other.solid != SOLID_BSP)   // e.g., shielded mega enforcer.
    {   // Break on impact.
        local   vector  org;

        org = self.origin - 8*normalize(self.velocity);
        sound (self, CHAN_WEAPON, "archer/arrowbrk.wav", 1, ATTN_IDLE);
        Tent_Point (TE_GUNSHOT, org);
        remove (self);
        return;
    }
// Imbed into the surface.
    if (random() < 0.5)
        sound (self, CHAN_WEAPON, "archer/arrow2.wav", 1, ATTN_IDLE);
    else
        sound (self, CHAN_WEAPON, "archer/arrow3.wav", 1, ATTN_IDLE);
    self.movetype = MOVETYPE_NONE;
    self.velocity = self.avelocity = '0 0 0';
    // Reset think so arrow sticks in wall for a moment then vanishes.
    self.nextthink= time + 1.5;
    self.think    = SUB_Remove;
    self.touch    = SUB_Remove;
    if (other)
    {   // Hit something that may move later.  Watch the axe... er, arrow.
        self.enemy      = other;
        self.delay      = self.nextthink;
        self.nextthink  = time + 0.100;
        self.think      = Axe_Watch;
    }
};

void() AkArrow_Think =
{
    if (self.delay <= time)
        {remove (self); return;}

    self.speed   = vlen (self.velocity);
    self.movedir = self.dest = normalize (self.velocity);
    self.angles  = vectoangles(self.movedir);
    if (self.cnt)
        Meteor_Trail (self.cnt, 1, FALSE);

    self.nextthink = time + 0.05;

// PM:  Energy arrows can blast ghosts.
    //if (self.worldtype)
        //Ghost_Touch ();
};

void(vector start, vector dir, float type) AkArrow_Launch =
{
    newmis = spawn ();
    newmis.master       = newmis.owner        = self;
    newmis.movetype     = MOVETYPE_FLYMISSILE;
    newmis.solid        = SOLID_BBOX;
    newmis.classname    = "arrow";
// set missile speed
    newmis.movedir      = dir;
//    if (ARROW_FLY)
    {
        newmis.speed        = AKARROW_SPEED_FLY;
        newmis.velocity     = dir * newmis.speed;
    }
//    else
//    {
//        newmis.speed        = AKARROW_SPEED_TOSS;
//        newmis.velocity     = dir * newmis.speed;
//        newmis.velocity_z   = newmis.velocity_z + 200;
//        newmis.movetype     = MOVETYPE_TOSS;
//    }
    newmis.avelocity    = '0 0 0';
    newmis.avelocity_z  = newmis.speed;     // Makes arrow model spiral.
    newmis.angles       = vectoangles(newmis.velocity);
    newmis.dest         = dir;      // for 'spawn_touchblood'.
// set missile damage
// Damage: green = 5-7, red = 8-12, gold = 13-17
// For Quake, do x*3 so that orange = 15-20, gold = 25-35, blue = 40-50
// Update/Drake:  Green = 9, Red = 18, Yellow/Orange = 30.
    if (type & 2)
    {   // Energy arrow.
        type = newmis.count = 0;    // Was LF_YELLOW.
        Laser_Color (newmis, type);
        newmis.worldtype    = TRUE;     // FIXME:  Conflict with rocket code.
        newmis.dmg          = 30;
        newmis.think1       = AkArrow_Explode;
    // For more explosive arrow types, use safe flag so attacker doesn't
    // blow himself up.  Arrows don't explode in Hexen2, but I am making
    // them so in Drake.
    // Update 4/13/2009:  Removed lightning flag.
        newmis.style        = DF_SAFE;
        newmis.cnt          = newmis.waitmax;
        newmis.effects      = newmis.effects | EF_DIMLIGHT;
        newmis.oldorigin    = start;
        newmis.skin         = ARROW_MAGIC + type;  // Yes, use + not |.
    }
    else
    {
        newmis.worldtype    = FALSE;
        newmis.xfl          = newmis.xfl | XFL_WIND;
        // Normal wooden arrow.
        newmis.cnt          = 0;
        if (type & 4)
            newmis.dmg          = 9;    // Goblin arrow.
        else if (type & 1)
            newmis.dmg          = 18;   // Lord arrow.
        else
            newmis.dmg          = 15;   // Orc arrow.
        newmis.skin         = type & 1;
    }
    newmis.enemy        = world;
// set missile duration
    newmis.touch        = AkArrow_Magic_Touch;
    newmis.delay        = time + 5;         // Total duration
    newmis.nextthink    = time + 0.1;
    newmis.think        = AkArrow_Think;
    if (!newmis.cnt)
    {
        newmis.touch        = AkArrow_Wood_Touch;
//        if (ARROW_FLY)
        {
            newmis.nextthink    = newmis.delay;
            newmis.think        = SUB_Remove;
        }
    }

    if (self.classname == "monster_rebel")  // FIXME:  Crossbow bandit hack.
        newmis.frame        = 1;
    setall (newmis, "progs/drake/arrow.mdl", '0 0 0', '0 0 0', start);
};

//==========================================================================
//  Throwing Axe

// This removes the axe if its surface moves or if its been out too long.
// Update:  Also called by goblin arrow code for the same purpose.
void() Axe_Watch =
{
    if (self.delay <= time)
        remove (self);
    else if (vlen (self.enemy.velocity) > 0)
        remove (self);      // Surface moved, so remove the axe now.
    else
        self.nextthink = time + 0.100;
};

//==========================================================================
//  Burst Lightning

//
//  The "Burst" is a form of chain lightning cast at a target.  If the
//  target is struck, and there are more targets near the one first struck,
//  secondary bolts will erupt from the primary target and strike as many
//  extra targets as possible.
//
//  This form of chain lightning is based from the description of the
//  Chain Lightning spell in the 3rd edition of Dungeons and Dragons.
//

//------------------------------------------------------------------------//
//  This returns the endpoint where a bolt of chain lightning should
//  strike.  The spot returned is usually the center of the target struck.
//------------------------------------------------------------------------//
vector(entity attacker, entity targ, vector start, vector end)
    ChainLit_Point =
{
    local   vector  spot;

    spot = Midpoint (targ);
    traceline (start, spot, FALSE, attacker);
    if (trace_ent == targ)
    {
        // We can hit the center.  Find the best endpoint of the bolt
        // so that it looks good when drawn.  This point is located
        // between the targer's center and where the bolt hit.

        local   vector  dir;
        local   float   d_impact, d_ideal, dist;

        dir = spot - start;
        d_impact = vlen (spot - trace_endpos);
        d_ideal = vlen (dir);
        dir = normalize (dir);
    // Bolts in beam TEs are drawn in intervals of 30.
        dist = floor (d_ideal / 30) * 30;
        if (dist >= d_impact)
            end = start + dir*dist;
        else
        {
            spot = start + dir*(dist+30);
            if (spot_x >= targ.absmin_x)
              if (spot_x <= targ.absmax_x)
                if (spot_y >= targ.absmin_y)
                  if (spot_y <= targ.absmax_y)
                    if (spot_z >= targ.absmin_z)
                      if (spot_z <= targ.absmax_z)
                        end = spot;
        }
    }

    return end;
};

// Called within 'BurstLit_Chain' below.
float(entity targ, entity inflictor, entity attacker, float damage,
    vector spot, string dtype, float bang)  BurstLit_Trace =
{
    local   vector  end;

    if (targ.flags & FL_CLIENT)
        end = targ.origin;      // Use origin because of TE beam code for clients.
    else
        end = Midpoint (targ);
    //- - - - -
    // Update:  Traceline must hit intended target!
    // Targets can take damage from one bolt once because they
    // can neither stack nor pierce.
    //
    // Note:  Ghost touch code must be called before calling
    // this function if ghosts are to be hit.
    //- - - - -
    traceline (spot, end, FALSE, inflictor);    // 3rd was TRUE.
    if (trace_ent == targ)
    {
        // Draw from source point to target point, except for players.
        // For players, draw from target point to source point
        // because engine always draws from origin in case of player.
        if (targ.flags & FL_CLIENT)
            Tent_Beam (TE_LIGHTNING2, targ, end, spot);
        else
            Tent_Beam (TE_LIGHTNING2, targ, spot, end);
        if (bang == 2)
            sound (inflictor, CHAN_VOICE, "weapons/lstart.wav", 1, ATTN_NORM);
        else if (bang)
            sound (inflictor, CHAN_VOICE, "weapons/lhit.wav", 1, ATTN_NORM);
        end = trace_endpos;     // Use where bolt hit for blood spot.
        BoltDamage (targ, inflictor, attacker, end, damage, dtype);
        return TRUE;
    }
    return FALSE;
};

// Shoots multiple lightning bolts at nearby targets from a common point.
// Used by burst lightning gun.  Also used by plasma discharge.
void(entity inflictor, entity attacker, float damage, entity ignore,
    vector spot, string dtype, vector pak, float bang)  BurstLit_Chain =
{
    local   entity  head, saved;
    local   float   hits;
//    local   vector  end;

    local   float   nff, radius, hi;

    nff = pak_x;
    radius = pak_y;
    hi = pak_z;

    hits = 0;
    saved = world;
    head = findradius (spot, radius);
    while (head)
    {
    // Notarget/stealth won't save you from discharge.
        if (head.takedamage == DAMAGE_AIM)      // Was flags & FLx_CREATURE.
        if (head != attacker)
        if (head != ignore)
        if (head.resist_cells < 1)     // Don't waste hits on immune targets.
        //if (!HasReflection (head, spot, damage))    // Too much work to bounce here, so pass!
        //if (!(nff && Ally_Check (head, attacker)))  // nff = No Friendly Fire.
        {
            // Go to the trouble of zapping live, highly vulnerable targets
            // first instead of dead or resistant targets.
            if (head.deadflag || (head.health <= 0) || (head.resist_cells > 0))
            {   // Dead or resistant -- save it for later.
                head.chain2 = saved;
                saved = head;
            }
            else if (BurstLit_Trace (head, inflictor, attacker, damage, spot, dtype, bang))
            {
                hits = hits + 1;
                if (hits >= hi)
                    return;
            }
        }
        head = head.chain;
    }
// Now we zap the saved targets because we ran out of other targets.
    while (saved)
    {
        if (BurstLit_Trace (saved, inflictor, attacker, damage, spot, dtype, bang))
        {
            hits = hits + 1;
            if (hits >= hi)
                return;
        }
        saved = saved.chain2;
    }
};

// Fires a fat bolt of lightning, then splits into three thinner bolts
// after hitting a target.
void(vector p1, vector p2, entity attacker, float damage, float reach)
    BurstLit_Bolt =
{
//- - - - - - - - -
// 'inflictor' = Don't hit this entity during the traceline.
// 'src'       = From whom the lightning gets drawn from.
// 'targ'      = Target by traceline.
//- - - - - - - - -
    local   entity  inflictor, src, targ;
    local   vector  pak;
    local   float   boing;

    pak_x = TRUE;       // Friendly fire off.
    pak_y = reach;
    pak_z = 3;          // Max number of targets.

    inflictor = src = attacker;

    //GhostTouch_On ();
//- - - - - - - - -
    while (src)
    {
    // Scan for a hit.
        reach = vlen(p2 - p1);
        traceline (p1, p2, FALSE, inflictor);
// DISABLED -- Auto-aimed bolts that stray too far to the side look bad.
//        if (trace_fraction == 1.0)      // Try again with a fatter trace.
//            traceline (p1, p2, 2, inflictor);
        p2 = trace_endpos;
        targ = trace_ent;

    // Check for reflection.
        boing = FALSE;
        //if (targ)
            //if (HasReflection (targ, p2, damage))
                //boing = TRUE;

    // Draw beam.
        if (targ.takedamage)    // Hit something?  Aim bolt at its center.
            if (!boing)
                p2 = ChainLit_Point (attacker, targ, p1, p2);
        Tent_Beam (TE_LIGHTNING1, src, p1, p2);   // Use the fat beam.
        src = world;    // Unless reflected later, end loop when done.

    // Do hit effects and damage.
        /*if (boing)
        {
            ReflectSound (targ);
            if (src != lightning)
            {
                reach = reach - vlen(p2 - p1);  // Get remaining dist.
                p1 = p2;
                p2 = p2 + (normalize(p2 - targ.origin) * reach);
                inflictor = targ;
                src = lightning;
            }
        }
        else
        {*/
            if (targ.takedamage)
            {
                // Direct hit does full damage and splits the beam.
                BoltDamage (targ, inflictor, attacker, p2, damage, "thunderbolt");
                BurstLit_Chain (targ, attacker, damage * 2 / 3, targ, p2, "thunderbolt", pak, FALSE);
            }
        //}
    }
//- - - - - - - - -
    //GhostTouch_Off ();
};

// Called by exploding plasma balls.  Also by Mjolnir.
void() RyuPlasma_Discharge =
{
// Reference for DoE plasma discharge stats.
// BurstLit_Chain (self, self.owner, 50, world, self.origin, self.classname, '0 320 5', TRUE);
    local   vector  pak;
    local string damagetype = "";

    pak_x = self.style;
    pak_y = self.distance;
    pak_z = self.count;     //5;

    BurstLit_Chain (self, self.owner, (self.dmg*0.5), self.enemy, self.origin, damagetype, pak, TRUE);
};

void() RyuPlasma_Explode =
{
    if (!self.dmg)
        self.dmg = 100;
    if (!self.distance)
        self.distance = 320;
    if (!self.count)
        self.count = 5;

    if (self.enemy)
    {
        local   float   dmg1, dmg2;

        dmg1 = (0.8 + random()*0.2) * self.dmg;   // Was '80 + random()*20'.
    // PM:  Add discharge damage to impact damage, then ignore other in
    // the discharge call.  Saves a hit in the discharge for someone else.
        dmg2 = self.dmg * 0.5;

        if (self.enemy.classname == "monster_shambler")
            dmg1 = dmg1 * 0.5;      // Resists explosion damage.
        else if (self.classname == "monster_dragon")
        {
            if (self.enemy.skin == 2)     //SKIN_BLUE
            {   // Resists electrical damage.
                dmg1 = dmg1 * 0.5;
                dmg2 = dmg2 * 0.5;
            }
        }

        T_Damage (self.enemy, self, self.owner, (dmg1 + dmg2), DAMARMOR);
    }
    // don't do radius damage to the other, because all the damage
    // was done in the impact
    // Note:  Radius damage was 70.
    T_RadiusDamage (self, self.owner, self.dmg*0.7, self.enemy, DAMAGEALL);

    sound (self, CHAN_AUTO, "plasma/explode.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);
    setorigin (self, self.origin);      // PM:  Do it right.
    Tent_Explosion2 (self.origin, 244, 3);
    RyuPlasma_Discharge ();

    BecomeAnyExplosion (138, 6, "progs/drake/s_fancy.spr");
};

void() RyuPlasma_Touch =
{
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self);  return;}

    self.enemy = other;
    RyuPlasma_Explode ();
};

void() RyuPlasma_Think =
{
    local   float   pc;

    if (self.delay <= time)
        {remove (self);  return;}
    pc = pointcontents(self.origin);
    if ((pc <= CONTENT_WATER) && (pc >= CONTENT_LAVA))
        {RyuPlasma_Explode();  return;}

    particle (self.origin, self.velocity * 0.005, 9, self.waitmin);   //2
    particle (self.origin, self.velocity * 0.01, 41, self.waitmax);   //4
    self.nextthink = time + 0.05;
};

void(vector start, vector dir, float sped, float damage) RyuPlasma_Launch =
{
    newmis = spawn ();
    newmis.owner        = self;
    newmis.movetype     = MOVETYPE_FLYMISSILE;
    newmis.solid        = SOLID_BBOX;
    newmis.classname    = "plasma";
// set missile speed
    newmis.movedir      = dir;
    newmis.speed        = sped;
    newmis.velocity     = dir * sped;
    newmis.avelocity    = '300 300 300';
    newmis.angles       = vectoangles(newmis.velocity);
// set missile damage
    newmis.dmg          = damage;
    //effects set below.
    newmis.distance     = damage + 220;     // 320 when dmg == 100.
    newmis.enemy        = world;
    newmis.style        = FALSE;    // TRUE is 'no friendly fire' discharge.
    // Note:  Mjolnir has a range of 350.
// set missile duration
    newmis.touch        = RyuPlasma_Touch;
    newmis.delay        = time + 5;     // Total duration.  As spikes.
    newmis.nextthink    = 0.01;
    newmis.think        = RyuPlasma_Think;

    // Big plasma balls from DoE.
    newmis.waitmin      = 2;
    newmis.waitmax      = 4;
    newmis.effects      = EF_DIMLIGHT;  // DoE used EF_BRIGHTLIGHT.
    setmodel (newmis, "progs/plasma.mdl");
    setsize (newmis, '0 0 0', '0 0 0');
    setorigin (newmis, start);
};


//==========================================================================
//  Acid Bomb -- Used by green dragon.

void() Acidball_Touch =
{
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}

    self.touch = SUB_Null;
    self.enemy = other;

    if (!self.dmg)
        self.dmg = 120;

    // Don't bother with green dragon acid resistance.
    if (self.enemy)     // Don't check health, just if there is a target.
    {   // Someone took a direct hit.
        local   float   damage;

        damage = self.dmg / 6 * (5 + random());
        T_Damage (self.enemy, self, self.owner, damage, DAMARMOR);
    }
    // Don't do radius damage to the enemy, because all the damage
    // was done in the impact.
    T_RadiusDamage (self, self.owner, self.dmg, self.enemy,DAMAGEALL);

// PM:  Don't set origin directly during a touch function because of
// possible link scrambling.  May cause FitzQuake to crash otherwise.
    self.origin = self.origin - 8*normalize(self.velocity);
    Tent_Explosion2 (self.origin, 48, 16);
    BecomeAnyExplosion (115, 5, "progs/drake/s_fancy.spr");
};

void(vector start, vector dir, float sped, float damage) Acidball_Launch =
{
    newmis = spawn ();
    newmis.owner        = self;
    newmis.movetype     = MOVETYPE_TOSS;
    newmis.solid        = SOLID_BBOX;
    newmis.classname    = "acid";
// set missile speed
    newmis.speed        = sped;
    newmis.velocity     = dir * sped;
    newmis.avelocity    = '200 100 300';
    newmis.angles       = vectoangles(newmis.velocity);
// set missile damage
    newmis.dmg          = damage;
// set missile special stuff
    newmis.dest         = dir;
// set missile duration
    newmis.touch        = Acidball_Touch;
    newmis.nextthink    = time + 5;
    newmis.think        = SUB_Remove;

    setmodel (newmis, "progs/drake/acidbal1.mdl");
    setsize (newmis, '0 0 0', '0 0 0');
    setorigin (newmis, start);
};


//==========================================================================
//  Acid -- Used by scrag and green dragon.

void() Acid_Think =
{
    if (self.delay <= time)
        {remove (self); return;}

//    self.angles = normalize (self.velocity);
//    self.angles = vectoangles (self.angles);
    self.angles = vectoangles (self.velocity);
    self.nextthink = time + 0.1;
};

void(vector start, vector dir, float sped) Acid_Launch = { launch_projectile(start, dir, CT_PROJ_WIZ, sped);  };

void(vector start, vector dir, float sped) Acid_Toss =
{
    Acid_Launch (start, dir, sped);

    newmis.movetype  = MOVETYPE_TOSS;
    newmis.speed     = sped;
    newmis.velocity  = (dir * sped) + '0 0 200';
    newmis.angles    = vectoangles (newmis.velocity);
    newmis.delay     = newmis.nextthink;
    newmis.nextthink = time + 0.1;
    newmis.think     = Acid_Think;
};

//==========================================================================
//  Meat -- Used by zombie and nightshade (wraith dragon)

// Formerly 'ZombieGrenadeTouch'
void() Meat_Touch =
{
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}
    //if (Reflected ())
     //   return;

    if (other.takedamage)
    {
        if (!self.dmg)
            self.dmg = 10;
        self.touch = SUB_Null;      // Stack overflow prevention.
//        if (cranked)
//            Poison_Add (other, 1);      // Give fatal illness like in movies.
        T_Damage (other, self, self.master, self.dmg, DAMARMOR);
        sound (self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_NORM);
        remove (self);
        return;
    }
    sound (self, CHAN_WEAPON, "zombie/z_miss.wav", 1, ATTN_NORM);    // bounce sound
    self.velocity = self.avelocity = '0 0 0';
    self.touch = SUB_Remove;
};

// This was formerly 'ZombieFireGrenade'.
void(vector start, vector dir) Meat_Launch =
{
    newmis = spawn ();
    newmis.master       = newmis.owner      = self;
    newmis.movetype     = MOVETYPE_BOUNCE;
    newmis.solid        = SOLID_BBOX;
//    newmis.classname    = "meat";
// set missile speed
    newmis.speed        = 600;
    newmis.velocity     = dir * 600;
    newmis.velocity_z   = newmis.velocity_z + 200;  // Used to be 200 only.
    newmis.avelocity    = '3000 1000 2000';
// set missile damage
    newmis.dmg          = 10;
    if (self.classname == "monster_mummy")
        newmis.dmg          = random() * 15 + 15;
// set missile duration
    newmis.touch        = Meat_Touch;
    newmis.nextthink    = time + 2.5;
    newmis.think        = SUB_Remove;

    setall (newmis, "progs/zom_gib.mdl", '0 0 0', '0 0 0', start);
};

//==========================================================================
//  Magic Missiles -- Used by hell knight, baron, and nemesant.

// Moved to 'weapons.qc'.


//==========================================================================
//  Rain Storm -- Used by the elemental boss barons.

//- - - - - - - - -
// Various missile functions for the rainstorm.
void() RainStorm_Ice =
{
// Use flymissile instead of toss for the missile clip bonus.
// Then set speed to a reasonably fast value to use for terminal velocity.
    Ice_Launch (trace_endpos, '0 0 -1', 1000);
    newmis.master   = newmis.owner  = self.owner;
    newmis.delay    = time + 2.5;
    newmis.dmg      = self.dmg;
};

// DISABLED -- Unused.
//void() RainStorm_Fire =
//{
//    Fireball_Launch (self.owner, trace_endpos, '0 0 -1', 1000, self.dmg);
//    newmis.delay    = time + 2.5;
//    newmis.noise    = "hknight/hit.wav";
//    newmis.style    = newmis.style | DF_SAFE;
//};

// DISABLED -- Unused.
//void() RainStorm_Steel =
//{
//    launch_superspike (trace_endpos, '0 0 -1');
//    newmis.master   = newmis.owner  = self.owner;
//    newmis.nextthink= time + 2.5;
//    newmis.dmg      = self.dmg;
//};
//- - - - - - - - -

void() RainStorm_Think =
{
    if (self.delay <= time)
    {
        super_active = 0;
        remove (self);
        return;
    }

// Find a point to spawn a rain drop.
    local   vector  p1, p2, dir;

    p1 = self.enemy.origin;
    p2_x = crandom() * self.distance + p1_x;
    p2_y = crandom() * self.distance + p1_y;
    p2_z = p1_z;

    traceline (p1, p2, TRUE, self);
    p2 = trace_endpos;
    if (trace_fraction < 1)
    {   // Move away from the wall a tiny bit.
        dir = normalize (p2 - p1);
        p2 = p2 - dir*2;
    }
    traceline (p2, p2 + '0 0 2048', TRUE, self);
    // Move endpoint away from the ceiling.
    // We can't pass vectors to a void() function, so use trace_endpos.
    if (trace_fraction < 1)
        trace_endpos = trace_endpos - '0 0 2';

    if (self.think1)
        self.think1 ();
    else
        {remove (self);  return;}

    self.nextthink = time + self.wait;
};

// Spawns a missile spawner that drops shots on a target from the ceiling.
void(entity targ, entity attacker, float damage, float fps, float dist,
    float limit, void() thinkst) RainStorm =
{
    if (limit)
    if (super_active)   // FIXME:  Check this only if attacker is a monster.
        return;

    super_active = 1;       // This chews through a lot of edicts.

    if (!fps)
        fps = 10;
    if (!dist)
        dist = 64;

    newmis = spawn();
    setorigin (newmis, '0 0 0');

    newmis.owner    = attacker;
    newmis.enemy    = targ;
    newmis.delay    = time + 5;
    newmis.think    = RainStorm_Think;
    newmis.think1   = thinkst;
    newmis.dmg      = damage;
    newmis.wait     = 1 / fps;
    newmis.distance = dist;
    newmis.nextthink= 0.01;
};

//==========================================================================
//  Knight Fireball -- Used by hell knight and baron.

void(vector start, vector dir) KnightBall_Launch =
{
    local   entity  missile;

    missile = spawn ();
    missile.master      = missile.owner     = self;
    missile.movetype    = MOVETYPE_FLYMISSILE;
    missile.solid       = SOLID_BBOX;
    missile.classname   = "fireball";
// set missile speed
    missile.speed       = 1000;
    missile.velocity    = dir * 1000;
    missile.angles      = vectoangles(missile.velocity);
    missile.avelocity   = '0 0 0';
    missile.avelocity_z = random()*2000 - 1000;
// set missile damage
    missile.dmg         = 60;
    missile.enemy       = world;
    missile.style       = DF_ROCKET;    // Shamblers take half damage.
// set missile duration
    missile.touch       = T_MissileTouch;
    missile.nextthink   = time + 5;
    missile.think       = T_MissileExplode;
    missile.think1      = Orange_Explosion;

    missile.frame       = 2;
    setall (missile, "progs/drake/k_ball.mdl", '0 0 0', '0 0 0', start);

    newmis = missile;
};

void(vector st, float lo) M_FireKnight =
{
    local   vector  p1, p2, dir;
    local   entity  targ;

//    if (cranked)
//        targ = self.enemy;
//    else
        targ = world;
    makevectors (self.angles);
    p1 = self.origin + (v_forward * st_x) + (v_right * st_y) + (v_up * st_z);
    p2 = Aimpoint (self.enemy);
    if (lo)
        p2_z = self.enemy.absmin_z + 1;
    dir = Maim_Line (p1, p2, 1000, targ, '0 0 0');

    self.effects = self.effects | EF_MUZZLEFLASH;
    sound (self, CHAN_WEAPON, "baron/fireball.wav", 1, ATTN_NORM);

    KnightBall_Launch (p1, dir);
};

//================
// W_FireSaw
//================
void(entity targ, vector dir) Attack_Saw =
{
    local   vector  spot;
    local vector source, org, vec2;
    local float src_dist, dotprod, axedmg;
    local entity onflr, tself, tother;

    source = self.origin + '0 0 16';        // Move source point up body

    // See if there are any bodies lying around
    onflr = find(world, bodyonflr, MON_ONFLR);
    while (onflr) {
        src_dist = vlen(source - onflr.origin);
        if (src_dist < RANGE_CHOPAXE) {            // Is the body close to the player?
            org = onflr.origin - '0 0 16';        // move origin close to floor
            makevectors (self.v_angle);            // Calculate viewing angle
            vec2 = normalize (org - self.origin);
            dotprod = vec2 * v_forward;
            if (dotprod > 0.6) {                // Is the body infront of the player
                onflr.origin = org;                // Move gib closer to floor
                onflr.bodyonflr = string_null;    // no more body gibbing
                tself = self; tother = other;    // save self/other
                self = onflr; other = tself;    // switch around
                if (onflr.classgroup == CG_ZOMBIE) {
                    self.health = 0;
                    Killed(onflr, tself);
                }
                else {
                    self.max_health = MON_NOGIBVELOCITY; // use minimal velocity
                    monster_ThrowGib();            // throw parts
                }
                
                self = tself; other = tother;    // switch back
                SpawnBlood (onflr, onflr.origin, '0 0 50', axedmg*4);
                return;                                                                // Finished
            }
        }
        // See if there are anymore bodies close by
        onflr = find(onflr,bodyonflr, MON_ONFLR);
    }

    spot = self.origin + '0 0 16';      // 'source'
    traceline (spot, spot + dir*64, FALSE, self);
    if (trace_fraction == 1.0)
        return;

    // This next check is for AI-controlled monsters aiming at 'targ',
    // which should equal 'self.enemy'.
    if (targ)
        if (trace_ent != targ)
            return;     // Missed target.

    spot = trace_endpos - dir*4;        // 'org'
    targ = trace_ent;
    if (targ.takedamage) {
        local   vector  vec;
        local   float   damage, dflags, pain;
        local   string  dtype;

        dtype = "chainsaw";     // Normal kill.
        dflags = DF_SCAR | DF_MELEE;
        damage = 40;
        if (trace_fraction <= 0.5) {
            // Deep hit.
            dtype = "saw_decap";    // Chop off the head.
            if (targ.xsf & XSF_BODY_DOUBLE)     // Chop up flimsy bodies.
                if (targ.max_health < 50)
                    if (targ.health > damage)
                        damage = targ.health;
        }

        // Suck life like Heretic's necro gauntlets.
        //if (self.tome_finished) {
        //    if (damage < 40)
        //        damage = 40;
        //    dflags = dflags | DF_LEECH;
        //}

    // Blood!
        vec = ((random()*300) - 150) * v_right + (100 * v_forward);
        SpawnBlood (targ, trace_endpos, '0 0 5', damage);
        if (self.weaponframe & 1)
            SpawnMeatSprayDrake (self.origin + dir*16, vec, targ.bloodtype);
    // Damage!
        pain = FeelsPain (targ);
        T_Damage (targ, self, self, damage, DAMARMOR);
        if (targ.solid != SOLID_BSP)
            targ.velocity = targ.velocity * 0.5;
    // Sounds.
        sound (self, CHAN_WEAPON, "weapons/sawguts.wav", 1, ATTN_NORM);
        if (targ.hittype == HIT_METAL || targ.spawnflags & MON_OGRE_GREEN || targ.spawnflags & MON_HOGRE_METAL)
            if (pain && (targ.pain_finished > time))
                sound (self, CHAN_AUTO, "weapons/clang.wav", 1, ATTN_NORM);

        if (targ.hittype == HIT_STONE)
            if (pain && (targ.pain_finished > time))
                sound (self, CHAN_AUTO, "weapons/axhitwal.wav", 1, ATTN_NORM);

        self.punchangle_x = -8;
    }
    else
    {   // Hit wall.
        sound (self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);
        Tent_Point (TE_GUNSHOT, spot);
    }
};

void() W_FireSaw = {
    if (cutscene || intermission_running)
        return;     // Abort if in intermission.
    makevectors (self.v_angle);
    Attack_Saw (world, v_forward);
};