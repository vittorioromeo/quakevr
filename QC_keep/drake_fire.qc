//==========================================================================
//  Frame Data

//  Frame Macros for modified 'flame2.mdl'.
$cd id1/models/flame2
$origin 0 0 12
$base base        
$skin skin

// These are grouped scenes.
$frame  flame
$frame  flameb
// Next one is the firewall.  Added for SoE's hell lord.
$frame  flamec

// These scenes are not grouped.
$frame  ember1 ember2 ember3 ember4 ember5 ember6


void(float ents) Ember_Shower;

//==========================================================================
//  Animation

//============
// Flame animation on modified 'flame2.mdl'.
//============
void() m_ember1 =[ $ember1, m_ember2   ] {};
void() m_ember2 =[ $ember2, m_ember3   ] {};
void() m_ember3 =[ $ember3, m_ember4   ] {};
void() m_ember4 =[ $ember4, m_ember5   ] {};
void() m_ember5 =[ $ember5, SUB_Remove ] {};
//============

// This makes flaming embers rise from the burning victim 'ent'.
void(entity ent) MakeFlame =
{
    local   entity  fire;       // The new fire that will be created.
    local   vector  spot;       // The spot where the new fire will appear.
    local   float   r;

    // Used to be in a loop.
    spot   = ent.size * 0.5;
    spot_x = spot_x * (random() - 0.5);
    spot_y = spot_y * (random() - 0.5);
    spot_z = spot_z * (random() - 0.5);
    spot   = spot + Midpoint (ent);

    // Create the fire.
    fire = spawn();

    fire.frame      = $ember1;
    fire.solid      = SOLID_NOT;
    fire.takedamage = DAMAGE_NO;
    fire.movetype   = MOVETYPE_FLY;
    fire.velocity   = '0 0 100';
    fire.owner      = ent;

    setall (fire, "progs/drake/flame2.mdl", '0 0 0', '0 0 0', spot);

    // Animate the flame.
    r = random() * 3;
    if (r > 2)
        SUB_Think (fire, m_ember3);
    else if (r > 1)
        SUB_Think (fire, m_ember2);
    else
        SUB_Think (fire, m_ember1);
};

// This creates a small puff of smoke.
void(vector spot, float density) MakeSmoke =
{
    particle (spot, '0 0 100', 1, density);
};


//==========================================================================
//  Delayed Fire Damage

//- - - - - - - - -
// Thinking for the delayed damage entity spawned by 'BecomeBigBang'.
void() DelayedFireDamage_Think =
{
    T_RadiusDamage (self, self.master, self.dmg, world, DAMAGEALL);
    self.count = self.count - 1;
    if (self.count > 0)
        self.nextthink = time + 0.2;
    else
        remove (self);
};

// Spawn a separate entity for delayed damage so that I don't need to clog
// the generic explosion code or write yet another special explosion scene.
void(entity attacker, vector spot, float damage, float dflags, string dtype)
    DelayedFireDamage_Spawn =
{
    local   entity  fire;
    local   float   radius, attn;

    if (!damage)
        damage = 60;    // High enough damage near big fireball sprite.
    radius = 100;
    attn = damage / radius;
    if (attn > ATTN_DAMAGE)
        attn = ATTN_DAMAGE;

    fire = spawn();
    fire.master   = attacker;   //self.master;
    fire.classname= dtype;      //self.classname;
    fire.movetype = MOVETYPE_NONE;
    fire.velocity = '0 0 0';
    fire.touch    = SUB_Null;
    fire.solid    = SOLID_NOT;
    fire.style    = DF_ROCKET | DFx_FIREBALL | dflags;
    fire.count    = 3;
    fire.dmg      = damage;
    fire.distance = radius;
    fire.volume   = attn;
    fire.nextthink= time + 0.1;
    fire.think    = DelayedFireDamage_Think;
    setall (fire, "", '0 0 0', '0 0 0', spot);  //self.origin);
};
//- - - - - - - - -

void() BecomeBigBangExplosion =
{
    // Make a small flame cloud to make explosion look bigger.
    if (coop || deathmatch)
        Ember_Shower (5);   // Make fewer embers in netgames.
    else
        Ember_Shower (10);

    Rocket_Explosion1 (self.origin);
    FlameWave ();
    BecomeTheBigFire ();    // Was BecomeExplosion.
};

// This creates a bigger fiery explosion like the one in the game Blood.
void() BecomeBigBang =
{
    local   float   dflags;

    dflags = 0;     //self.style & (DF_SAFE|DF_BLESSED);

// Delayed damage effects.
// Note:  A 0.8 sec quake pings for damage three times.
    DelayedFireDamage_Spawn (self.master, self.origin, self.dmg / 3, dflags, self.classname);
    Quake_Spawn (self.master, self.dmg + 60, 5, self.origin, dflags, 0.8, VEC_QUAKE);

// The rest of the explosion effect.
    BecomeBigBangExplosion ();
};
//- - - - - - - - -

// Spawns a huge fiery explosion at a point.
// 'dmg1' = Explosion damage.
// 'dmg2' = Secondary fire damage.  Pings multiple times.
void(entity attacker, vector spot, float dmg1, float dmg2,
    float dflags, string dtype)  Fireworks =
{
    local   entity  fire;
    local   float   rflags;

    rflags = DF_ROCKET | DFx_FIREBALL | (dflags & (DF_SAFE|DF_BLESSED));

    fire = spawn ();
    fire.master     = fire.owner    = attacker;
    fire.movetype   = MOVETYPE_NONE;
    fire.solid      = SOLID_NOT;
    fire.classname  = dtype;    //"fire";
    setorigin (fire, spot);

    T_RadiusDamage (fire, attacker, dmg1, world, DAMAGEALL);

    sound (fire, CHAN_AUTO, "nemesant/bigboom.wav", 1, ATTN_NORM);

    DelayedFireDamage_Spawn (attacker, spot, dmg2, dflags, dtype);
    Quake_Spawn (self, dmg1 + 60, 5, fire.origin, dflags, 0.8, VEC_QUAKE);
    SUB_Think (fire, BecomeBigBangExplosion);
};


//==========================================================================
//  Explosion Effects

// Checks if a given point in space is in water.
float(vector spot) VecInWater = {return (pointcontents(spot) <= CONTENT_WATER);};

// This enforces air resistance on embers and updates their angles.
// NOTE:  Modified flame has ember scene; frames start at 2, ends at 7.
void() Ember_Think =[ $ember1, Ember_Think ]
{
    if (self.delay <= time)
    {
        if (self.walkframe >= 5)
            {remove (self); return;}
        self.walkframe = self.walkframe + 1;
    }
    self.frame = $ember1 + self.walkframe;

    local   vector  dir;

    // Update angles to its current direction.
    dir = normalize (self.velocity);
    self.angles = vectoangles(dir) + '90 0 0';

    // Cheap hack -- apply air resistance by reducing velocity.
    self.velocity = self.velocity * 0.75;
};

// Variant of Ember_Think.  Used for smoke trail entities.
void() Smoke_Think =[ 0, Smoke_Think ]
{
    if (self.delay <= time)
        {remove (self); return;}

    // Cheap hack -- apply air resistance by reducing velocity.
    self.velocity = self.velocity * 0.75;
};

// Throws a shower of embers from 'self'.
void(float ents) Ember_Shower =
{
    local   entity  fire;
    local   vector  dir;
    local   float   loop;

    loop = ents * 6;
    while (loop > 0)
    {
        // Get initial direction.
        dir_x = (random() - 0.5) * 180;
        dir_y = random() * 360;
        dir_z = 0;
        makevectors (dir);
        dir = v_forward;

        traceline (self.origin, self.origin + dir*64, FALSE, other);
        if (trace_fraction == 1.0)
        {
            // Ember will not impact the wall immediately.
            fire = spawn();
        //- - - - - - - - -
            fire.walkframe  = random() * 2 + 2.5;   //4.5;
            fire.walkframe  = floor(fire.walkframe);

            fire.frame      = $ember1 + fire.walkframe;
            fire.think      = Ember_Think;
        //- - - - - - - - -
            fire.solid      = SOLID_BBOX;
            fire.movetype   = MOVETYPE_TOSS;
            fire.owner      = other;

            fire.velocity   = dir * (200 + random() * 800);
            fire.angles     = vectoangles(dir) + '90 0 0';

            fire.delay      = time + 0.5 + random();    // Time when it fades.

            fire.touch      = SUB_Remove;
            fire.nextthink  = time + 0.1;

            setall (fire, "progs/drake/flame2.mdl", '0 0 0', '0 0 0', self.origin);

            ents = ents - 1;
            if (ents <= 0)
                return;
        }

        loop = loop - 1;
    }
};

// Spawns an ember and smoke in most of its think frames.
void() Smokey_Think =
{
    if (self.wait > time)
        if (self.enemy.modelindex)
    {
        // Make a thin trail of smoke above the flame, plus a few embers.
        local   float   ftime;

        ftime = floor(time * 10);
        if (ftime & 1)
        {
            local   vector  spot;

            spot = Midpoint (self.enemy);
            MakeSmoke (spot, 5);
            if (ftime & 2)
                {if (!VecInWater (spot))  MakeFlame (self.enemy);}
        }
        self.nextthink = time + 0.1;
        return;
    }
    remove (self);
};

// This creates an entity that produces smoke and a few embers around
// a given target 'ent'.  Used on targets that die from burning.
void(entity ent) Smokey =
{
    // Create the fire.
    newmis = spawn();

    newmis.solid      = SOLID_NOT;
    newmis.takedamage = DAMAGE_NO;
    newmis.enemy      = ent;
    newmis.wait       = time + 1 + random() * 2;
    newmis.nextthink  = 0.01;
    newmis.think      = Smokey_Think;

    setall (newmis, "", '0 0 0', '0 0 0', VEC_ORIGIN);
};


//==========================================================================
//  Burn Control

// Constants.  (Update 5/30/09:  Moved to defs.)

//------------------------------------------------------------------------//
//  This checks if 'ent' is on fire and burning.
//  Note:  world.burner should return FALSE, so don't check ent == world.
//------------------------------------------------------------------------//
float(entity ent) IsBurning = {if (ent.burner) return TRUE; return FALSE;};

//------------------------------------------------------------------------//
//  This extinguishes the fire burning on a player (or other entity).
//  This function should be called whenever the fire burning on an
//  entity must be removed immediately.
//
//  Examples who and when this should be called for:
//  * All players (and their minions) when the level ends.
//  * Anytime a player respawns after dying (from the 'kill' command).
//
//  Assumptions (i.e., leaps of faith):
//  * If ent == world, then ent.burner is always world (FALSE).
//  * If ent.burner is not the world, then ent.burner really is a flame.
//     If not, bad things will likely happen.
//
//  Update:  Since burning is back to the old Napalm style, always use
//  this function to remove any flame pointed by ent.burner.
//------------------------------------------------------------------------//
void(entity ent) Burn_Null =
{
    if (ent.burner)
    {   // Note:  non-zero cnt means flame is underwater.
        if (ent.modelindex && !ent.burner.cnt)
        {   // Transform flame into a small explosion.
            ent.burner.effects = EF_MUZZLEFLASH;    // No more dynamic light.
            SUB_Think (ent.burner, BecomeExplosion);
        }
        else
        {   // Disappear right now.
            remove (ent.burner);
        }
        ent.burner = world;
    }
};

//------------------------------------------------------------------------//
// Returns the point where a flame should burn (hopefully on its target).
//------------------------------------------------------------------------//
vector(entity ent) Burn_Point =
{
    if (ent.view_ofs_z)
    {
        makevectors (ent.angles);
        if ((ent.burn_ofs_x) || (ent.burn_ofs_z))
            return ent.origin + (v_forward*ent.burn_ofs_x) + (v_up*ent.burn_ofs_z);
        else
            return ent.origin + (v_forward*4) + (v_up*16);
    }
    return Midpoint (ent);  // Some entities have origin at 0,0,0.
};

//------------------------------------------------------------------------//
// Checks if ent is immune to burning.
// IMPORTANT NOTE:  Burn immunity is NOT the same as fire immunity.
//------------------------------------------------------------------------//
float(entity ent) Burn_Not =
{
    if (!ent.modelindex)
        return TRUE;    // Treated as non-existant.
    if (!ent.takedamage)
        return TRUE;    // This includes head gibs and mega-enforcer shields.
    //if (ent.resist_fire >= 1)
    //    return TRUE;    // Immune to fire.
// Items that prevent burning.
    if (ent.invincible_finished)
        return TRUE;
    if (ent.radsuit_finished)
        return TRUE;
    if (ent.cross_finished)     // Update 1/6/10:  Cross added.
        return TRUE;
//- - - - - - - - - - - - - -
    if (ent.solid == SOLID_BSP)
        return TRUE;    // Flames don't look good on these ents.
    if (ent.xfl & XFL_NO_BURN)
        return TRUE;    // Immunity flag.

    return FALSE;
};

//------------------------------------------------------------------------//
//  This checks if the death flame should stop burning.
//------------------------------------------------------------------------//
float() DeathFlame_End =
{
    if (self.owner.deadflag >= DEAD_DEAD)
        return TRUE;    // Stop burning once the enemy is really dead.
    // If solid-only burn is on, then stop once enemy becomes non-solid.
    if (self.style)
    {
        if (self.style != self.owner.modelindex)
            return TRUE;
    }
    else if (self.owner.solid == SOLID_NOT)
        return TRUE;

    return FALSE;   // Keep on burning.
};

//------------------------------------------------------------------------//
//  This checks if the fire should shed light.
//
//  Dynamic lights slow Quake like nothing else, so turn off the flame's
//  light if the entity burning has light of its own, or turn on the
//  light if the entity has none.
//
//  Also, Quake has an upper limit for dynamic lights -- exceed it and
//  the game will crash.
//------------------------------------------------------------------------//
void() Burn_Light =
{
// Don't shed light if the target is shedding light of its own.
    if (self.owner.effects & (EF_DIMLIGHT | EF_BRIGHTLIGHT))
        self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    else
        self.effects = self.effects | EF_DIMLIGHT;
};

//------------------------------------------------------------------------//
//  This makes the fire cause damage to whom it burns.
//------------------------------------------------------------------------//
void() Burn_Burn =
{
    Burn_Light ();    // Control flame's dynamic light.
    if (self.delay <= time)
    {   // Burn the target.
        local   float   damage;
        local   entity  targ;

        targ = self.owner;
        damage = BURN_PER_SECOND;
        if (damage > self.dmg)
            damage = self.dmg;

        if (damage > 0)     // Negative damage prevention.
        {
            if (targ.health > 0)     // Don't let burn damage gib the dead.
            {
            // Update 5/13/10:  More damage to zombies to overcome regeneration.
                local   float   burn;

                burn = damage;
                if (targ.xfl & XFL_ZOMBIE)
                    burn = burn + 2;    // Overcome regeneration.
                T_Damage (targ, targ, self.master, burn, DAMARMOR);
            }
            // Note:  Negative radius for the constant damage.
            T_RadiusDamage (self, self.master, damage, self.owner, DAMAGEALL);
        }

        // Subtract from total... after the damage is done because I want
        // the pain sounds done first before tampering with the flame.
        if (!self.deadflag)
        {
            self.dmg = self.dmg - damage;
            if (self.dmg < 1)
                {Burn_Null (targ); return;}
        }
        // The fire can burn again after a moment.
        // Note:  findradius is called once per burning entity, and that
        // can mean many times in horde combat scenarios.  To minimize
        // frequent calling, inflict damage once every second, much like
        // slime bathing, even though I wanted more frequent calls.
        self.delay = time + 1;
    }
};

//------------------------------------------------------------------------//
//  This rather large function is responsible for updating flame status.
//  It checks if the flame should be snuffed.  If not, this also moves
//  said flame on its target and checks for damage.
//------------------------------------------------------------------------//
void() Burn_Think =
{
    local   entity  targ;
    local   vector  spot;
    local   float   pc;

    targ = self.owner;
    if (!targ)
        {remove (self); return;}
    if (targ.burner != self)      // This should not happen.
        {Burn_Null (targ); return;}

// From here on in either mode, the flame goes through a four step process:
// * Check for flame removal.
// * Move the flame so that it sticks to its target.
// * Pick behavior based on its new location (e.g., water snuffs most fires).
// * If flame is still burning, check for damage to targets.
    if (self.deadflag)
    {   // Death mode uses different burn rules.
        if (DeathFlame_End ())
        {   // Done burning.
            if (self.model != self.mdl)
                self.cnt = 1;   // Flame disappears instead of exploding.
            Smokey (targ);
            Burn_Null (targ);
            return;
        }

        // Make the flame stick to its target.
        spot = Burn_Point (self.owner);
        setorigin (self, spot);

        // Enemy is already dead so damaging it is unnecessary.
        // Flame has different effects depending on its contents.
        if (VecInWater (spot))
        {   // Hide the flame and make more steam in its place.
            // Think of it as a fire so hot even water can't snuff it fully.
            if (self.model == self.mdl)
            {
                setmodel (self, "");
                sound (self, CHAN_AUTO, "player/slimbrn2.wav", 1, ATTN_NORM);
            }
            MakeSmoke (spot, 50);
        }
        else
        {   // Flame is not in the water.
            if (self.model != self.mdl)
            {   // Flare back up.
                MakeExplosion (spot);
                setmodel (self, self.mdl);
            }
            Burn_Burn ();
        }
    }
    else
    {   // Standard burn mode.
//- - - - - - - - -
// If enemy is dead, snuff current flame and drop a smaller, fullbright
// flame that does not shed light.  The reason for doing this is because
// the big flame sheds light, and few if anything can slow down Quake as
// much as excessive dynamic lighting.  So in the interest of good gameplay,
// remove big flames once the enemy dies.
//
// Update:  Dropping flames are gone because, after a kill, aggressive
// players could collide with the flame before it could hit the ground
// and, as a result, take unnecessary damage.  Instead, a smoker entity
// bound to a slain entity puffs smoke and a few embers for a second or two.
//- - - - - - - - -
        if (targ.health < 1)
            {Smokey (targ);  Burn_Null (targ);  return;}

    // Check if the flame should be snuffed.
        if (self.dmg < 1)
            {Burn_Null (targ); return;}     // Out of fuel.
        if (Burn_Not (targ))
            {Burn_Null (targ); return;}

        spot = Burn_Point (targ);
        setorigin (self, spot);     // Move the flame.

        // Check if flame is in the water.
        pc = pointcontents (spot);
        if (pc <= CONTENT_WATER)
        {   // Flame was submerged under some liquid so remove it now.
            // In water or slime, make some steam too.  In lava or sky,
            // just disappear.
            self.cnt = 1;   // Flame disappears instead of exploding.
            Burn_Null (targ);   // Sever link between flame and its victim.
            if (pc >= CONTENT_SLIME)
            {
                sound (targ, CHAN_AUTO, "player/slimbrn2.wav", 1, ATTN_NORM);
                MakeSmoke (spot, 50);
            }
            return;
        }

    // Keep on burning.
        Burn_Burn ();
    }
    self.nextthink = time + 0.100;
};

//------------------------------------------------------------------------//
//  This keeps the fire on an entity burning.
//------------------------------------------------------------------------//
void(entity ent) Burn_Check =
{
    if (ent.burner)
        SUB_Think (ent.burner, Burn_Think);
};

//------------------------------------------------------------------------//
//  This spawns a large flame on the entity to burn.
//  Should only be called by 'Burn_Add' or 'Burn_Killer'.
//------------------------------------------------------------------------//
void(entity victim, entity attacker, float damage) Burn_New =
{
    local   vector  spot;

    if (!victim)
        return;     // Don't try to burn the world.
    if (victim.burner)
        return;     // Already burning.

    if (damage)
    {   // Assume normal mode -- called by 'Burn_Add'.
        if (victim.health <= 0)
            return;     // Dead!  Don't add more dynamic light to slow game.
        if (Burn_Not (victim))
            return;     // Target can't ignite, so stop.  Assumes Napalm
                        //  (one of my old QC mods) style burns.
        spot = Burn_Point (victim);
        if (VecInWater (spot))
            return;     // Flame will be out as soon as it spawns, so abort.

        if (victim.flags & FL_CLIENT)   // Hack:  Reset pain for players
            victim.pain_finished = 0;   //  so they can scream now.
    }
    else
    {   // Assume death mode -- called by 'Burn_Killer'.
        spot = Burn_Point (victim);
    }

    MakeExplosion (spot);   // Flare up with a bang.

// Spawn burner entity.
    newmis = spawn();
    newmis.solid        = SOLID_NOT;
    newmis.takedamage   = DAMAGE_NO;
    newmis.frame        = $flameb;  // Use a *big* flame.
    newmis.effects      = EF_DIMLIGHT;
    newmis.master       = attacker;
    newmis.owner        = victim;
    newmis.classname    = "fire";
    newmis.delay        = time + 1;
    newmis.dmg          = damage;       // Initial damage.
    newmis.nextthink    = 0.01;
    newmis.think        = Burn_Think;
    setall (newmis, "progs/drake/flame2.mdl", '0 0 0', '0 0 0', spot);
    // Added the following to differentiate between normal and death modes.
    newmis.mdl          = newmis.model; // Only used by death mode code.
    newmis.style        = 0;            // Ditto, once again.
    newmis.deadflag     = 0;            // Use standard burn rules.

// Link target to the fire.
    victim.burner = newmis;
};

//------------------------------------------------------------------------//
//  This sets the victim on fire by adding burn damage.
//
//  Update:  Flames do not stack, so if the victim is already burning,
//  keep the longer burning fire only.  I made the change so I can have
//  flames that can spread to others yet not sustain each other
//  indefinitely.  This style is similar to the Rabies skill in Diablo II.
//------------------------------------------------------------------------//
void(entity victim, entity attacker, float burn) Burn_Add =
{
    // Check for fire immunity.
    if (!victim.takedamage)
        return;     // Immune to damage.
    if (burn < 1)
        return;     // No fuel.

    if (victim.burner)
    {
        if (victim.burner.deadflag)
            return;     // In death mode, so do nothing.
        if (burn < victim.burner.dmg)
            return;     // New fire doesn't last as long, so ignore it.

        if (victim.burner.master != attacker)
        {   // New attacker steals the old fire.
            MakeExplosion (victim.burner.origin);
            victim.burner.master = attacker;
        }
        victim.burner.dmg = burn;
    }
    else
    {   // Victim is not on fire, so set him alight.
        Burn_New (victim, attacker, burn);
        if (!victim.burner)
            return;     // Failed to ignite target.
    }
};

//------------------------------------------------------------------------//
//  This spawns a large burning flame on a dying target.  This should
//  be called ONLY from the target's 'th_die()' function.
//
//  NOTE:  Assumes 'self' == dying victim to be burned!
//------------------------------------------------------------------------//
void() Burn_Killer =
{
    local   entity  fire;       // The flame entity to be used.

    if (self.burner)
    {   // Flame already exists so use that.
        fire = self.burner;
        if (fire.deadflag)
            return;     // Already set up.
        fire.master     = self.enemy;   // Set by 'Killed' to victim's slayer.
    }
    else
    {   // Spawn a new flame.
        Burn_New (self, self.enemy, 0);
        fire = self.burner;
        if (!fire)
            return;     // Failed to spawn.
    }
// Properties common to new or borrowed flames.
    fire.deadflag   = 1;
    fire.dmg        = MAX_BURN_DAMAGE;
    if (self.solid == SOLID_NOT)
        fire.style      = self.modelindex;
};


//==========================================================================
//  Fireballs

//  NOTE:  Fireballs are flames turned on its side.  In the official
//         GLQuake, this results in a shadow cast somewhere in the air.
//
//  One solution would be to add a fireball scene.  However, that causes
//  animation problems for embers in source ports that support frame
//  interpolation.  In addition, some of the source ports don't let
//  flames cast shadows (which is good).  Hence, no fireball scene.
//
//  Update:  Some ports automatically substitute 'flame2.mdl' with
//  other non-model graphics (decals?), and the projectiles appear more
//  like long flame trails, which look really bad.  Therefore, allow
//  the option for alternate fireball models.  Such models do not look
//  good in vanilla GLQuake or some other ports, but it's better than
//  nothing.

float   FIREBALL_RADIUS = 70;

//------------------------------------------------------------------------//
//  This creates steam that has the same velocity of the fireball that
//  spawned it.
//------------------------------------------------------------------------//
void(entity fire) Fireball_Steam =
{
    local   float   sped;
    local   vector  vel;

    sped = vlen (fire.velocity) * 0.01;
    vel = normalize (fire.velocity) * sped;

    sound (fire, CHAN_AUTO, "player/slimbrn2.wav", 1, ATTN_NORM);
    particle (fire.origin, vel, 1, 50);
};

//------------------------------------------------------------------------//
//  Check if fireball is in the water and if so remove it.
//  Returns TRUE if fireball was removed.
//------------------------------------------------------------------------//
float(entity fire) Fireball_InWater =
{
    local   float   pc;

    pc = pointcontents (fire.origin);
    if (pc <= CONTENT_WATER)
    if (pc != CONTENT_SKY)      // 8/24/09:  So fire can rain from the sky.
    {
        if ((pc == CONTENT_WATER) || (pc == CONTENT_SLIME))
            Fireball_Steam (fire);
        remove (fire);
        return TRUE;
    }
    return FALSE;
};

//------------------------------------------------------------------------//
//  This is the end of a fireball's think phase.
//------------------------------------------------------------------------//
void() Fireball_EndFrame =
{
    if (Fireball_InWater (self))
        return;

    // Check if flame should burn things near it.
    if (self.wait)
        if (self.wait <= time)
        {   // Burn nearby enemies.
        //- - - - - - - - - -
        // This is so dragons don't hurt themselves with their own flames.
        // Works for players with dragonscale and monsters alike.
            local   entity  ignore;

            //if (self.owner.resist_fire > 0)
            //    ignore = self.owner;
            //else
                ignore = world;
        //- - - - - - - - - -
            // Note:  It's a tiny flame so just do damage, nothing fancy.
            // Note#2:  Negative radius for the constant damage.
            T_RadiusDamage (self, self.owner, 1, ignore, DAMAGEALL);
            self.wait = time + random();
        }

    // Fire is still burning, check again on the next frame.
    self.nextthink  = time + 0.1;
};

//------------------------------------------------------------------------//
//  This checks when to put out the fire.  The fire is put out when
//  the burning entity finds water or when the fire is done burning.
//------------------------------------------------------------------------//
void() FloorFire_Think =
{
    if (self.delay <= time)
    {   // The fire burns itself out after a period of time.
        MakeSmoke (self.origin, 50);
        remove (self);
        return;
    }
    Fireball_EndFrame ();
};

//------------------------------------------------------------------------//
//  Spawns a small flame that burns on the ground, if there is ground.
//  If no ground is found, no flame is spawned.  Mostly for show.
//  Can inflict minor damage to those standing near the flame.
//------------------------------------------------------------------------//
void(entity attacker, entity victim, vector spot) FloorFire =
{
    local   entity  swap;

    swap = self;
    self = spawn();
    self.movetype   = MOVETYPE_TOSS;
    self.solid      = SOLID_NOT;
    self.owner      = victim;       // Use 'world' to hit anything.
    setall (self, "", '0 0 0', '0 0 0', spot);
    if (droptofloor ())
    {
        self.classname  = "fire";
        self.angles     = '0 0 0';
        self.velocity   = '0 0 0';
        self.owner      = 
        self.master     = attacker;   // Remember who really owns the flame.
        self.delay      = random() * 3 + 2 + time;
        self.wait       =
        self.nextthink  = 0.01;
        self.think      = FloorFire_Think;
        setall (self, "progs/drake/flame2.mdl", '-4 -4 -12', '4 4 16', self.origin + '0 0 12');
    }
    else
    {
        remove (self);
    }
    self = swap;
};

//------------------------------------------------------------------------//
//  This checks if a flame should drop after a fireball impact.
//------------------------------------------------------------------------//
float(entity fire, float chance) FloorFire_ZoneCheck =
{
    if (random() < chance)
    {
        if (VecInWater (fire.origin))
            return FALSE;       // Don't drop underwater.

        // Drop flame if it is not inside or directly above the owner.
        local   entity  ent;
        local   vector  spot;

        ent = fire.owner;
        spot = fire.origin;

        // Fire is 4 wide, plus 16 for possible missile clip bouns.
        if (spot_x > (ent.absmax_x + 21))  return TRUE;
        if (spot_y > (ent.absmax_y + 21))  return TRUE;
        if (spot_x < (ent.absmin_x - 21))  return TRUE;
        if (spot_y < (ent.absmin_y - 21))  return TRUE;
        if (spot_z < (ent.absmin_z - 21))  return TRUE;
    }

    return FALSE;
};

//------------------------------------------------------------------------//
//  This checks if a fireball impacts the floor.
//------------------------------------------------------------------------//
float() FloorFire_FloorCheck =
{
    local   vector  p1, p2, vel;
    local   float   dot;

    vel = normalize (self.velocity);
    p1 = self.origin;
    p2 = p1 + (vel * 64);
    traceline (p1, p2, TRUE, world);
    vel = trace_plane_normal;
    dot = vel * '0 0 1';

    return (dot > 0.7071);   // Pass only if slope is less than 45 degrees.
};

//------------------------------------------------------------------------//
//  Attempts to spawn a small flame on the ground.  Called by fireballs'
//  touch functions.
//------------------------------------------------------------------------//
void(entity fire, float chance) FloorFire_SpawnCheck =
{
    if (FloorFire_ZoneCheck (fire, chance))
        if (FloorFire_FloorCheck ())
            FloorFire (fire.master, fire, fire.origin);
};

//------------------------------------------------------------------------//
//  Called when fireball impacts.  On impact, it explodes and inflicts
//  damage.
//------------------------------------------------------------------------//
void() Fireball_Touch =
{
    // If it is in the sky, disappear.
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}

    // If the flame is in water, dissolve into steam.
    self.velocity = normalize(self.velocity);
    if (Fireball_InWater (self))
        return;

    // NEW:  Rebound if impacting a reflective surface.
    //if (Reflected ())
    //    return;

    self.touch = SUB_Null;      // Stack overflow prevention.

    // hit something that bleeds
    // Update 8/30/09:  Attenuation = Damage / Radius.
    T_RadiusDamage (self, self.master, self.dmg, other, DAMAGEALL);
    self.origin = self.origin - 8*normalize(self.velocity);
    if (other.takedamage)
    {
        MakeFlame (other);
        T_Damage (other, self, self.master, self.dmg, DAMARMOR);
    }
    else
        FloorFire_SpawnCheck (self, 0.1);

    self.effects = self.effects | EF_MUZZLEFLASH;
    if (self.noise)
        sound (self, CHAN_AUTO, self.noise, 1, ATTN_NORM);
    BecomeExplosion ();
};

//------------------------------------------------------------------------//
//  This updates the fireball's angles as it flies through the air.
//------------------------------------------------------------------------//
void() Fireball_Update =
{
    local   vector  dir;

    dir = normalize (self.velocity);
    self.angles = vectoangles(dir) + self.pos1;
// Update:  Use variable 'self.pos1' instead of always '90 0 0'.
};

//------------------------------------------------------------------------//
//  This gradually shrinks the fireball into nothing as it flies.
//------------------------------------------------------------------------//
void() Fireball_Fade =
{
    if (self.frame >= self.delay)
        {remove (self);  return;}
    self.frame = self.frame + 1;
    Fireball_Update ();
    Fireball_EndFrame ();
};

//------------------------------------------------------------------------//
//  This checks the fireball as it flies through the air.
//------------------------------------------------------------------------//
void() Fireball_Think =
{
    if (self.delay <= time)
    {
        // Make the fireball fade into nothing.  Takes 3 frames to fade.
        self.frame  = $ember2;
        self.delay  = $ember4;
        self.think  = Fireball_Fade;
    }
    Fireball_Update ();
    Fireball_EndFrame ();
};

//------------------------------------------------------------------------//
//  Create and launch a small fireball.
//------------------------------------------------------------------------//
void(entity attacker, vector start, vector dir, float sped, float damage)
    Fireball_Launch =
{
    local   entity  fire;
    local   vector  ofs;
    local   string  text;

//    if (qrack)
//        {ofs = '0 0 0'; text = "progs/drake/flame2a.mdl";}
//    else
//        {ofs = '90 0 0'; text = "progs/drake/flame2.mdl";}
    ofs = '90 0 0';
    text = "progs/drake/flame2.mdl";
    fire = spawn();
    fire.movetype   = MOVETYPE_FLYMISSILE;
    fire.solid      = SOLID_BBOX;
    fire.frame      = $flame;
    fire.angles     = vectoangles(dir) + ofs;
    fire.speed      = sped;
    fire.velocity   = dir * sped;
    fire.touch      = Fireball_Touch;
    fire.nextthink  = 0.01;
    fire.think      = Fireball_Think;
    fire.master     = fire.owner    = attacker;
    fire.delay      = time + 5;     // Was (8000 / sped), like in Quake2.
    fire.wait       = 0;
    fire.dmg        = damage;
    fire.classname  = "fire";
    fire.style      = DFx_FIREBALL;
    fire.pos1       = ofs;
    setall (fire, text, '0 0 0', '0 0 0', start);

    // So fireball can be changed later.
    newmis = fire;
};


//==========================================================================
//  Super-sized Fireballs (a.k.a. Meteor)

//------------------------------------------------------------------------//
//  This fizzles then removes the meteor.
//------------------------------------------------------------------------//
void(entity fire) Meteor_Fizzle =
{
    local   float   pc;

    pc = pointcontents (fire.origin);
    if ((pc == CONTENT_WATER) || (pc == CONTENT_SLIME))
        Fireball_Steam (fire);
    remove (fire);
};

//------------------------------------------------------------------------//
//  The fireball explodes then creates a small cloud of fire.
//------------------------------------------------------------------------//
void() Meteor_Explode =
{
    // Do splash damage first then explode.
    T_RadiusDamage (self, self.master, self.dmg, self.enemy, DAMAGEALL);
    self.origin = self.origin - 8*normalize(self.velocity);

    if (self.enemy.takedamage)
    {   // Someone took a direct hit.
        local   float   damage;

        if (self.style & DF_BURN)   // Always ignite if burn is enabled.
            Burn_Add (other, self.master, MAX_BURN_DAMAGE);
        damage = SUB_RandomDamage (self.dmg);
        T_Damage (self.enemy, self, self.master, damage, DAMARMOR);
    }
    else
        FloorFire_SpawnCheck (self, 1);

    // Make a fiery explosion.
    if (self.dmg >= 80)
        Rocket_Explosion1 (self.origin);
    else
        Rocket_Explosion2 (self.origin, 96, 16);
    BecomeExplosion2 ();
};

//------------------------------------------------------------------------//
//  The large fireball explodes on contact and inflicts severe damage
//  within a radius.
//------------------------------------------------------------------------//
void() Meteor_Touch =
{
// Destroy fireball if it has no power or if it touches the sky.
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}
    if (self.dmg <= 0)
        {Meteor_Fizzle (self); return;}

// NEW:  Rebound if impacting a reflective surface.
    //if (Reflected_Damage (COURAGE_DAMAGE))
    //    return;

// Do the damage.
    self.touch = SUB_Null;      // Stack overflow prevention.
    self.enemy = other;
    Meteor_Explode ();
};

//------------------------------------------------------------------------//
//  This checks the big fireball.  It loses strength if it travels
//  through water.  While active, the fireball leaves behind a trail
//  of steam or embers.
//------------------------------------------------------------------------//
void() Meteor_Think =
{
//- - - - - - - - -
// Particle trail.
    local   float   rgb;
    local   float   density;

    // Default trail is steam.
    rgb = 6;
    density = 16;

    if (VecInWater (self.origin))
    {   // Fireball loses strength in water.  Loses 10 damage per frame.
        // Used to be 10% damage in Dragons.
        self.dmg = self.dmg - 10;
        if (self.dmg < 0)
            self.dmg = 0;
    }
    else if (!VecInWater (self.oldorigin))
    {   // Fireball has not been in water lately, so make an ember trail.
        rgb = 233;
        density = 8;
    }
    Meteor_Trail (rgb, density, TRUE);
//- - - - - - - - -
    Fireball_Update ();

    // Fizzle if it lost all power.
    if (self.dmg <= 0)
        {Meteor_Fizzle (self); return;}

    // The fireball will explode after a period of time.
    if (self.delay < time)
        {Meteor_Explode (); return;}

    // Fire is still burning, check again.
    self.nextthink  = time + 0.1;
};

//------------------------------------------------------------------------//
//  This creates a large, highly explosive fireball.
//------------------------------------------------------------------------//
void(entity attacker, vector start, vector dir, float sped, float damage)
    Meteor_Launch =
{
    local   entity  fire;
    local   vector  ofs;
    local   string  text;

//    if (qrack)
//        {ofs = '0 0 0'; text = "progs/drake/flame2a.mdl";}
//    else
//        {ofs = '90 0 0'; text = "progs/drake/flame2.mdl";}
    ofs = '90 0 0';
    text = "progs/drake/flame2.mdl";
    fire = spawn();
    fire.movetype   = MOVETYPE_FLYMISSILE;
    fire.solid      = SOLID_BBOX;
    fire.frame      = $flameb;      // Use a *big* flame.
    fire.angles     = vectoangles(dir) + ofs;
    fire.speed      = sped;
    fire.velocity   = dir * sped;
    fire.touch      = Meteor_Touch;
    fire.nextthink  = 0.01;
    fire.think      = Meteor_Think;
    fire.master     = fire.owner    = attacker;
    fire.enemy      = world;
    fire.delay      = time + 5;     // Was (8000 / sped), like in Quake2.
    fire.wait       = 0;
    fire.dmg        = damage;
    fire.classname  = "fire";
    fire.style      = DFx_FIREBALL | DF_ROCKET;
    fire.pos1       = ofs;
    // Added rocket type flag so shamblers take less damage.

    fire.effects    = fire.effects | EF_DIMLIGHT;
    fire.oldorigin  = start;

    setall (fire, text, '0 0 0', '0 0 0', start);

    // So fireball can be changed later.
    newmis = fire;
};


//==========================================================================
//  Firewalls

//
//  The firewall is divided into two major parts:  the spawner and the
//  flames.  The spawner is an invisible missile that spawns flames
//  on the ground below it.  The flames are stationary flame missiles
//  that burn whoever touches them.
//
//  Code derived (and modified) from mummy.hc in Hexen2.
//  In Hexen2, Each flame spawned has 17 frames and lasted about 0.85 sec.
//  With 17 frames, each wall had up to 17 edicts per firewall, and that
//  is way too much for comfort.  In Quake, each for firewall has about
//  seven flames -- a bit high, but hardly any worse than the hell knight
//  missile spray.  To compensate, the damage is higher than in Hexen2.
//
//  Functions below:
//   Firelink_*** = Flames in the firewall.
//   Firewall_*** = Invisible fire spawning missile.
//

float   SPEED_FIREWALL  = 400;      // Don't change!

// Animate the flame links in the firewall.
void() Firelink_Think = [ $flamec, Firelink_Think ]
{
    self.walkframe = self.walkframe + 1;
    if (self.walkframe > 3)     // FIXME:  Use 2 for Drake.
        remove (self);
    else
        particle (self.origin + '0 0 17', '0 0 25', self.volume, self.cnt);
};

// Oh dear!  Someone crossed the firewall.
void() Firelink_Touch =
{
    local   float   damage;

    if (self.wait > time)
        return;     // Already burnt someone recently.
    if (!other.takedamage)
        return;
    if (other == self.owner)
        return;     // Don't harm attacker.

    if (self.dmg < 1)
        damage = 1;
    else
        damage = (random() + 1) * self.dmg;
    T_Damage (other, self, self.master, damage, DAMARMOR);
    self.wait = time + 0.1;    // Wait until then to damage again.
// Play burning sounds.
    if (self.t_width < time)
    {
        sound (self, CHAN_BODY, "weapons/fwallhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.5;
    }
};

// Assumes 'self' is the firewall spawner missile.
// 'hot' = Spawn fire if TRUE, or spawn steam if FALSE.
void(vector spot, float hot) Firelink_Spawn =
{
    local   entity  fire;

    fire = spawn();
    fire.owner      = self.owner;
    fire.master     = self.master;
    fire.angles     = self.angles;
    fire.movetype   = MOVETYPE_FLYMISSILE;  // Use for the clip bonus.
    fire.solid      = SOLID_TRIGGER;
    fire.frame      = $flamec;
    fire.walkframe  = 0;
    fire.wait       = 0;    // Time to burn on touch.
    if (hot)
    {
        fire.volume     = 233;  // Particle color - embers.
        fire.cnt        = 1;    // Particle count.
        // 17/7 = 2.42857.  Hexen2 dmg was 3.
        fire.dmg        = ceil(self.dmg * 0.075);
        setmodel (fire, "progs/drake/flame2.mdl");
    }
    else    // It's in liquid so no flame model.
    {
        fire.volume     = 1;    // Steam
        fire.cnt        = 5;
        fire.dmg        = 0;    // Hot spot, minor damage.
        fire.modelindex = 0;    // No model.
    }
    setsize (fire, '0 0 0', '0 0 0');        
    setorigin (fire, spot);

    fire.nextthink  = time + 0.05;
    fire.think      = Firelink_Think;
    fire.touch      = Firelink_Touch;
};

void() Firewall_Think =
{
    local   float   pc;
    local   vector  spot;

    if (self.delay <= time)
        {remove (self);  return;}   // Time's up.
    pc = pointcontents (self.origin);
    if (pc == CONTENT_SOLID)
        {remove (self);  return;}   // In the void.

    traceline (self.origin, self.origin - '0 0 600', TRUE, self);
    spot = trace_endpos;
    pc = pointcontents (spot);

// Spawn a flame only if there is open space on the ground.
    if (trace_fraction < 1)
    if (pc != CONTENT_SOLID)
    if (pc != CONTENT_SKY)
        Firelink_Spawn (spot, (pc == CONTENT_EMPTY));
// The rest of the spawner missile think.
    if (self.enemy)
    {   // Steer toward the enemy.
        spot = self.enemy.origin + self.enemy.view_ofs;
//        if (1)
//        {   // Change pitch and yaw.  Aim_Lead plots intercept point.
//            spot = Aim_Lead (self.origin, spot, SPEED_FIREWALL, self.enemy);
//            Missile_Face (spot);    // Update angles toward desired point.
//        }
//        else
        {   // Change yaw only.  Aim_Line plots intercept course.
            // Use 'normalize(spot - self.origin)' instead to follow directly.
            spot = Aim_Line (self.origin, spot, SPEED_FIREWALL, self.enemy);
            self.ideal_yaw = vectoyaw(spot);
            self.ideal_yaw = anglemod(self.ideal_yaw);
            ChangeYaw ();
        }
        // Update direction.
        spot = self.angles;
        spot_x = 0 - spot_x;
        makevectors (spot);
        self.movedir  = v_forward;
    }
    self.velocity   = self.movedir * self.speed;
    self.angles     = vectoangles(self.velocity);
    self.nextthink  = time + 0.05;
};

// Explode like Maulotaur firewall.
void() Firewall_Touch =
{
    local   float   pass, pc;
    local   vector  spot;

// DISABLED -- Spawner is not solid_trigger anymore.
//    if (other == self.owner)
//        return;     // Seems trigger entities can touch its 'owner'.

    pass = 0;
// If explosion cannot hit the target, don't explode.
    traceline (self.origin, self.origin - '0 0 600', TRUE, self);
    if (trace_fraction == 1)
        pass = 1;     // No flame on ground.
    else
    {
        spot = trace_endpos;
        pc = pointcontents (spot);
        if ((pc != CONTENT_EMPTY) && (pc != CONTENT_LAVA))
            pass = 1;     // Flame in void, water, or sky.
        else if (other.flags & FLx_CREATURE)
        {
            if (fabs(other.origin_z - spot_z) > (self.dmg + 40))
                pass = 1;   // Too far away to damage.
            else
            {
                local   vector  mp;

                mp = Midpoint (other);
                traceline (spot, mp, TRUE, self);
                if (trace_fraction < 1)
                    pass = 1;     // Blocked
            }
        }
    }
    if (pass)
    {   // Explosion ineffective, convert to notouch version.
        self.movetype     = MOVETYPE_NOCLIP;
        self.solid        = SOLID_NOT;
        self.touch        = SUB_Null;
        return;
    }

    // PM:  Don't reflect firewall.

// Go boom.
    self.touch = SUB_Null;      // Stack overflow prevention.
    self.enemy = world;
    // FIXME:  Should spawn a dummy to explode at spot, then remove self.
    setorigin (self, spot);     // FIXME:  Maybe this is bad if SOLID_TRIGGER?
    T_MissileExplode ();
};

void(vector start, vector dir, float boom, entity targ) Firewall_Spawn =
{
    newmis = spawn ();
    newmis.master       = newmis.owner      = self;
    if (boom)
    {   // Explode if head impacts, similar to Maulotaur version.
        newmis.movetype     = MOVETYPE_FLYMISSILE;
        newmis.solid        = SOLID_BBOX;
        // Can't use solid_trigger because of touch link breakage on contact.
        // ...at least if we don't jump through hoops to avoid breakage.
    }
    else
    {   // Pass through all.
        newmis.movetype     = MOVETYPE_NOCLIP;
        newmis.solid        = SOLID_NOT;
    }
    newmis.classname    = "fire";   // Use this so zombies burn on death.
// set missile speed
    newmis.speed        = SPEED_FIREWALL;
    newmis.movedir      = dir;
    newmis.velocity     = dir * SPEED_FIREWALL;
    newmis.angles       = vectoangles(newmis.velocity);
// set missile damage
    newmis.dmg          = 80;   // Use 120 for Maulotaur strength.
    newmis.enemy        = targ;
    newmis.yaw_speed    = 5;    // Thinks 20/sec, effective yaw_speed of 10.
// set missile special stuff
    newmis.style        = DFx_FIREBALL | DF_ROCKET | DF_SAFE;
// set missile duration
    newmis.delay        = time + 3;     // Hexen2 used 2.5, not enough here.
    newmis.nextthink    = time + 0.05;
    newmis.think        = Firewall_Think;
    if (newmis.solid)
        newmis.touch        = Firewall_Touch;

    setall (newmis, "", '0 0 0', '0 0 0', start);   // Missile is invisible.

    sound (newmis, CHAN_WEAPON, "weapons/fwall.wav", 1, ATTN_NORM);
};

// Launch a firewall.  Designed for use by hell lords.
void() M_Firewall =
{
    local   entity  targ;
    local   vector  org, dir;

    self.effects = self.effects | EF_MUZZLEFLASH;

    if (skill >= 2)     // As with Chthon, lead on Hard and up.
        targ = self.enemy;
    else
        targ = world;
    org = self.origin + self.view_ofs;
    dir = self.enemy.origin + self.enemy.view_ofs;
    dir = Maim_Line (org, dir, SPEED_FIREWALL, targ, '0 0 0');
    Firewall_Spawn (org, dir, 1, world);
};

// Checks if the firewall is effective against an enemy.
// Assumed to be called during ai_run and self.th_missile().
float() M_FirewallCheck =
{
    if (self.h2olevel)
        return FALSE;   // Spawnpoint underwater.
    if (!(self.flags & FL_ONGROUND))
        return FALSE;   // Not on the ground.
    if (self.enemy)
    {
        if (self.enemy.h2olevel)
            return FALSE;       // Underwater.
        if (vlen(self.enemy.origin - self.origin) > 1000)
            return FALSE;       // Too far away.
        if (!(self.enemy.flags & FL_ONGROUND))
        {
            traceline (self.enemy.origin, self.enemy.origin - '0 0 100', TRUE, self.enemy);
            if (trace_fraction == 1)
                return FALSE;   // High above the ground.
        }
    }
    return TRUE;    // Fire away.
};


//===========================/  END OF FILE  /===========================//
