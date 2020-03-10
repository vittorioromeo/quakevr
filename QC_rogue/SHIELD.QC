// Shield
//

// ===================
// shield_think
// ===================
void() shield_think =
{
    if ( self.owner.shield_death_time < time)
    {
        self.owner.shield_entity = world;
        remove (self);
        return;
    }

    if ( (self.owner.shield_death_time - 0.25) <= time)
    {
        self.model = string_null;
    }
    else
    {
        setorigin ( self, self.owner.origin );
        self.v_angle = self.owner.v_angle;
        self.angles = self.owner.angles;
    }

    self.think = shield_think;
    self.nextthink = time + 0.05;
};

// ===================
// shield_spawn
// ===================
void(entity shieldOwner, vector dir) shield_spawn =
{
    local entity newShield;

    if ( shieldOwner.shield_death_time > time)
    {
        return;
    }

    newShield = spawn ();
    newShield.owner = shieldOwner;
    newShield.solid = SOLID_NOT;
    newShield.takedamage = DAMAGE_NO;
    newShield.movetype = MOVETYPE_NONE;
    newShield.classname = "power_shield";

    // time for shield to live
//    shieldOwner.shield_death_time = time + 0.5;
    shieldOwner.shield_death_time = time + 0.3;
    shieldOwner.shield_entity = newShield;

    newShield.v_angle = shieldOwner.v_angle;
    newShield.angles = shieldOwner.angles;
    setorigin (newShield, shieldOwner.origin);
    setmodel (newShield, "progs/p_shield.mdl");
    setsize (newShield, '0 0 0', '0 0 0');

    newShield.nextthink = time + 0.1;
    newShield.think = shield_think;
};


// ===================
// shield_hit
// ===================
float(entity targ, entity inflictor, entity attacker, float damage) shield_hit=
{
    local vector    dir;
    local float        save;
    local float        hitAngle;

    dir = inflictor.origin - targ.origin;
    save = vectoyaw ( dir );
    makevectors (targ.angles);
    hitAngle = save - vectoyaw ( v_forward );

    if(hitAngle > 90 && hitAngle < 270)
    {
        return damage;
    }
    else if(hitAngle < -90 && hitAngle > -270 )
    {
        return damage;
    }

    shield_spawn ( targ, dir);

    if (targ.shieldSoundTime < time)
    {
        sound (targ, CHAN_ITEM, "shield/hit.wav", 1, ATTN_NORM);
        targ.shieldSoundTime = time + 0.5;
    }

    if (inflictor.classname == "lava_spike")
        save = damage * 0.70;
    else
        save = damage * 0.30;

    return save;
};
