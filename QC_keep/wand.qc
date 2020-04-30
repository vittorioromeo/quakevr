//==========================================================================
//
//  ZAPPER (Flare Gun or the Ruby Wand)
//
//==========================================================================

//==========================================================================
//  Flares

// Flares are spawned from explosions caused by the zapper when fired
// at maximum power.

void() FlareGlitter_Think =
{
    self.frame = self.frame + 4;
    if (self.flags & FL_ONGROUND)
        self.nextthink = time + 0.1;
    else
        self.nextthink = time + 0.2;
    self.think = SUB_Remove;
};

void() FlareGlitter =
{
    local   vector  ang;

    ang_x = random() * 90;      // Aim downward.
    if (self.flags & FL_ONGROUND)
        ang_x = 0 - ang_x;
    ang_y = random() * 360;
    ang_z = 0;
    makevectors (ang);

    newmis = spawn();
    newmis.movetype     = MOVETYPE_BOUNCE;
    newmis.solid        = SOLID_NOT;        // Was SOLID_TRIGGER;
    newmis.velocity     = v_forward * 150;
    newmis.gravity      = 0.5;      // Don't fall as quickly.
    newmis.classname    = "glitter";
//    newmis.touch        = SUB_Remove;
// FIXME:  Merge with explosion sprite to save a model slot?
    newmis.frame        = 70;
    if (self.flags & FL_ONGROUND)
        newmis.nextthink    = time + 0.1;
    else
        newmis.nextthink    = time + 0.2;
    newmis.think        = FlareGlitter_Think;

    setall (newmis, "progs/drake/s_spark.spr", '0 0 0', '0 0 0', self.origin);
};

void() Flare_Think =
{
    if (self.delay <= time)
    {   if (self.effects & EF_BRIGHTLIGHT)
            {self.effects = EF_DIMLIGHT;  self.delay = time + 2;}   // Was T+3.
        else
            {remove (self);  return;}
    }

    self.nextthink  = time + 0.1;
    self.think      = Flare_Think;

    if (self.lefty)
        FlareGlitter ();
    else
    {   // Spawn some particles at the source.
        local   float   den;

        den = ceil(self.delay - time) * 4;
        particle (self.origin, '0 0 0', 232, den);
    }
    self.lefty = !self.lefty;
};

// Activates the spark generator that was spawned recently.
void() Flare_Activate =
{
    self.velocity     = '0 0 -10';
    self.effects      = EF_BRIGHTLIGHT;     //EF_DIMLIGHT;
    self.delay        = time + 3;
    Flare_Think ();
};

// Spawns an invisible spark generator.  Because we want the explosion
// sprite visible without extra stuff in the way, the generator isn't
// active until a few frames later, when the explosion is almost over.
void(vector start) Flare_Spawn =
{
// Effect is harmless, so owner can be anyone.
    newmis = spawn ();
    newmis.master       = newmis.owner     = world;
    newmis.movetype     = MOVETYPE_FLY;
    newmis.solid        = SOLID_NOT;
    newmis.classname    = "flare";
// set newmis duration
    newmis.nextthink    = time + 0.4;
    newmis.think        = Flare_Activate;

    setall (newmis, "progs/null.spr", '0 0 0', '0 0 0', start); //was null_256.spr
};

// Returns the ammo charge level.  Called by the equivalent of CheckPowerups.
// Charging notes, for normal mode flare gun:
// * Charging can be done only while holding the weapon.
// * The time to charge a super shot is double the time needed for
//  a full spray of mini-shots.  For example, if it takes one second
//  to charge a spray of three shots, it takes one more second for the
//  super shot.
float(entity ent) Zapper_Level =
{
    local   float   tnt;

    if (ent.attack_finished > time)
        return 0;       // Can't shoot yet.
    if (ent.tome_finished)
    {   ent.zapper_time = 0;    // In case tome wears off before discharge.
        return WAND_FULL + 2;
    }
    if (time < ent.zapper_time)
        return 0;       // Don't let rollover mess up counters.

// 0-1 = 1, 1-2 = 2, 2-3 = 3, 3-5 = 4.
    tnt = ceil((time - ent.zapper_time) / WAND_DELAY);
    if (tnt >= (WAND_FULL * 2 - 1))     // e.g., best 3 out of 5.
        return WAND_FULL + 1;
    if (tnt > WAND_FULL)
        return WAND_FULL;
    return tnt; 
};

void() WandSpark_Touch = {
    if (pointcontents(self.origin) == CONTENT_SKY)
        {remove (self); return;}
    //if (Reflected ())
        //return;

    self.touch = SUB_Null;      // Stack overflow prevention.

// hit something that bleeds
    if (other.takedamage)
        T_Damage (other, self, self.master, self.dmg, DAMARMOR);

    self.origin = self.origin - 8*normalize(self.velocity);
    BecomeFancy (X32_CUT_ORANGE, 4);
};

// Note:  'shots' is number of hitscan shots.
// Does not include the two freebie side missiles.
void(vector start, vector dir, float shots) FireSparks = {
    local   entity  targ, attacker;
    local   vector  end, end2, d1;
    local   float   damage, dist, mid, fr, sparx;
//    local   float   fx;
    local   string  dtype;
    local   vector  glitter;

    attacker = self;
    dtype = "sparks";
    damage = WAND_DAMAGE;
    dist = WAND_RANGE;
    fr = X32_SPARK_ORANGE;
    sparx = TRUE;
    // Note:  Changed 225 to 97 so blood won't spawn in ports.
    glitter = '233 97 1';
//    fx = EF_MUZZLEFLASH;

// Check for ToP override.
    if (shots < 1)
    {
        shots = 5;
        damage = 5;     // Heretic damage is d8 per shot.
        dist = 2048;
        fr = X32_CUT_ORANGE;
        sparx = FALSE;
        glitter_z = 0;
//        fx = 0;

    // Fire the two side projectiles.
    // Remember, spikes lasts up to six seconds.
        d1_x = (shots - 1) * 1.40625;  d1_y = 0;  d1_z = 1;
        end = Aim_Scatter (dir, d1);
        //Knightspike_Launch (start, end, 600);   //18*35=630.
        launch_projectile (start, end, CT_PROJ_HKN, 600);
        newmis.dmg = damage;
        newmis.touch = WandSpark_Touch;
    newmis.classtype = CT_WANDSPARK;
        if (shots > 1)
        {
            d1_x = 0 - d1_x;
            end = Aim_Scatter (dir, d1);
            //Knightspike_Launch (start, end, 600);
            launch_projectile (start, end, CT_PROJ_HKN, 600);
            newmis.dmg = damage;
            newmis.touch = WandSpark_Touch;
            newmis.classtype = CT_WANDSPARK;
        }
    }

// Start multi-trace attack.  Uses MultiDamage_* functions.
    mid = (shots + 1) * 0.5;
    MultiDamage_Clear ();
    while (shots > 0)
    {
        if (mid == 1)
            d1 = dir;
        else
        {
            d1_x = (shots - mid) * 2.8125;  d1_y = crandom()*0.5;  d1_z = 1;
            d1 = Aim_Scatter (dir, d1);
        }
        Lead_Hitscan (start, d1, dist, damage, attacker, glitter);
        targ = trace_ent;
        end = trace_endpos;
        end2 = end - d1*8;
        // Note:  Reflection merely stops the beam.
        if (trace_fraction < 1)
        {
            MultiDamage_Add (targ, damage, 0, dtype);
            self = spawn();
//            self.effects = fx;
            setall (self, "progs/null.spr", '0 0 0', '0 0 0', end2); //was null_256
            // Play sizzle sounds at reduced volume and attenuation.
            sound (self, CHAN_AUTO, "enforcer/enfstop.wav", 0.5, ATTN_STATIC);
            BecomeFancy (fr, 4);
            self = attacker;
        }
        shots = shots - 1;
    }
    MultiDamage_Apply (dtype);
};

// Shoots one big explosive shot.
// If multiple shots are desired, rewrite to use explosive shotgun code.
void(vector start, vector dir) FireBigSpark =
{
// Doom damage d3*5.  Heretic damage d8+6 (ToP d8*7).
    local   entity  targ, attacker;
    local   vector  end, end2;
    local   float   damage, splash;

    attacker = self;
    splash = WAND_DAMAGE * (WAND_FULL + 1);
    damage = SUB_RandomDamage (splash);
    Lead_Hitscan (start, dir, WAND_RANGE, damage, attacker, '233 97 4');
    targ = trace_ent;
    end = trace_endpos;
    end2 = end - dir*8;

    if (targ)
    {
        if (targ.classname == "monster_zombie")
        {   // Update 11/2/09:  Let max charge obliterate (unarmored) zombies.
            if (damage < targ.health)
                damage = targ.health;
        }
        T_Damage (targ, attacker, attacker, damage, DAMARMOR);
    }
    self = spawn();
    setall (self, "progs/null.spr", '0 0 0', '0 0 0', end); // was null_256.spr
    self.classtype = CT_WANDBIGSPARK;
    T_RadiusDamage (self, attacker, splash, targ, DAMAGEALL);
    self.origin = end2;
    Rocket_Explosion2 (end2, 224, 16);
    BecomeFancy (X80_SPARK_ORANGE, 6);
    self = attacker;

    Flare_Spawn (end2);
};

void() W_FireSparks = {
    local   vector  dir, org;
    local   float   charge;

    // Using IT2_CROSS for the wand
    if (self.weapon == IT2_CROSS)
        charge = self.currentammo;
    else
        charge = 0;     // This shouldn't happen, but if so...
// Update 4/5/10:  Use all accumulated energy.
    if (charge > WAND_FULL + 1)
        charge = WAND_FULL + 1;
    self.currentammo = 0;
    self.zapper_time = time + WAND_DELAY;
    self.attack_finished = time + WAND_DELAY;

    // Was "weapons/gldhit.wav".
    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);
    // Since weapon is a magic wand, no recoil effects are needed.

    makevectors (self.v_angle);
    org = self.origin + v_forward*8 + '0 0 16';
    dir = aim(self, 100000);

    //GhostTouch_On ();
    if (self.tome_finished)
        FireSparks (org, dir, 0);
    else if (charge <= WAND_FULL)
    {
        if (!charge)
        {   // Player is holding attack button down for rapid-fire.
            // Scatter shots a bit like in Heretic.
            local   vector  err;
    
            err_x = random();  err_x = err_x - random();  err_x = err_x * 2;
            err_y = crandom();  // Was random() - 0.5;
            err_z = 1;
            dir = Aim_Scatter (dir, err);
            charge = 1;
        }
        FireSparks (org, dir, charge);
    }
    else
        FireBigSpark (org, dir);
    //GhostTouch_Off ();
};


//===========================/  END OF FILE  /===========================//
