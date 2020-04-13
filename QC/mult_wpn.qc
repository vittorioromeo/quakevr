// multi_weapons.qc
// pmack
// sept 96

//=============================================================================
// Multi Grenade Code
//=============================================================================
void() MultiGrenadeTouch;

//================================
//================================
void() MiniGrenadeExplode =
{
    if ( self.owner.classname == "player")
        T_RadiusDamage (self, self.owner, 90, world);
    else
        T_RadiusDamage (self, self.owner, 60, world);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 230);
    WriteByte (MSG_BROADCAST, 5);

    BecomeExplosion ();
};

//================================
//================================
void(float offsetAngle) MiniGrenadeLaunch =
{
    local    entity missile;
    local    float    tempRand;

    missile = spawn ();
    missile.owner = self.owner;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_BBOX;
    missile.classname = "MiniGrenade";

// set missile speed
    missile.v_angle = self.v_angle;
    missile.v_angle_y = missile.v_angle_y + offsetAngle;
    makevectors (missile.v_angle);

        missile.velocity = v_forward*100 + v_up*400;
        tempRand = (crandom()*60) - 30;
        missile.velocity = missile.velocity + tempRand * v_forward;
        tempRand = (crandom()*40) - 20;
        missile.velocity = missile.velocity + tempRand * v_right;
        tempRand = (crandom()*60) - 30;
        missile.velocity = missile.velocity + tempRand * v_up;

    missile.avelocity = '300 300 300';
    missile.angles = vectoangles(missile.velocity);
    missile.touch = MultiGrenadeTouch;

    setmodel (missile, "progs/mervup.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, self.origin);

// set missile duration
    missile.nextthink = time + 1 + (crandom() * 0.5);
    missile.think = MiniGrenadeExplode;
};

//================================
//================================
void() MultiGrenadeExplode =
{
    MiniGrenadeLaunch(0);
    MiniGrenadeLaunch(72);
    MiniGrenadeLaunch(144);
    MiniGrenadeLaunch(216);
    MiniGrenadeLaunch(288);

    remove (self);
};

//================================
//================================
void() MultiGrenadeTouch =
{
    if (other == self.owner)
        return;        // don't explode on owner
    if (other.takedamage == DAMAGE_AIM)
    {
        if (self.classname == "MiniGrenade")
            MiniGrenadeExplode();
        else
        {
            if (self.owner.classname == "player")
                GrenadeExplode();
            else
                MiniGrenadeExplode();
        }
        return;
    }
    // bounce sound
    sound (self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM);
    if (self.velocity == '0 0 0')
        self.avelocity = '0 0 0';
};

//================================
//================================
void() W_FireMultiGrenade =
{
    local    entity missile;

    // TODO VR: (P2) rogue
    // self.currentammo = self.ammo_multi_rockets = self.ammo_multi_rockets - 1;
    self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;

    // TODO VR: (P2) rogue
    // UpdateAmmoCounts (self);

    sound (self, CHAN_WEAPON, "weapons/grenade.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_BBOX;
    missile.classname = "MultiGrenade";

// set missile speed
    makevectors (self.v_angle);
    if (self.v_angle_x)
        missile.velocity = v_forward*600 + v_up * 200 + crandom()*v_right*10 + crandom()*v_up*10;
    else
    {
        missile.velocity = aim(self, 10000);
        missile.velocity = missile.velocity * 600;
        missile.velocity_z = 200;
    }

    missile.avelocity = '300 300 300';
    missile.angles = vectoangles(missile.velocity);
    missile.touch = MultiGrenadeTouch;

// set missile duration
    missile.nextthink = time + 1;
    missile.think = MultiGrenadeExplode;

    setmodel (missile, "progs/mervup.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, self.origin);
};

//=============================================================================
// Multi Rocket Code
//=============================================================================

//================================
//================================
void() MultiRocketExplode =
{
    local float    damg;

//    Stock Single Rocket Damage...
//    damg = 100 + random()*20;

    damg = 60 + random()*15;

    if (other.health)
    {
        if (other.classname == "monster_shambler")
            damg = damg * 0.5;    // mostly immune
        if (other.classname == "monster_dragon")
            damg = damg * 0.5;    // mostly immune
        T_Damage (other, self, self.owner, damg );
    }

    // don't do radius damage to the other, because all the damage
    // was done in the impact
//    Stock single rocket damage.
//    T_RadiusDamage (self, self.owner, 120, other);

    T_RadiusDamage (self, self.owner, 75, other);

//    sound (self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
};

//================================
//================================
void() MultiRocketTouch =
{
    if (other == self.owner)
        return;        // don't explode on owner

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    MultiRocketExplode ();
};


//================================
//================================
void() HomingMissileThink =
{
    local vector    dir;

    if (self.enemy.health < 1)
    {
        remove(self);
        return;
    }

    dir = normalize(self.enemy.origin - self.origin);
    self.velocity = dir * 1000;
    self.nextthink = time + 0.1;
    self.think = HomingMissileThink;
};


//================================
//================================
void() HomingMissileAcquire =
{
    local vector    oldVelocity;
    local vector    aimangle;

    if ( self.delay < time )
    {
        MultiRocketExplode ();
        return;
    }

    oldVelocity = self.velocity;
    makevectors (self.v_angle);
    self.velocity = aim (self, 1000);
    self.velocity = self.velocity * 1000;

    aimangle = self.origin + self.velocity;
    traceline ( self.origin, aimangle, FALSE, self );
    if (trace_fraction < 1)
    {
        if (trace_ent.flags & FL_MONSTER)
        {
            self.enemy = trace_ent;
            HomingMissileThink();
            return;
        }
    }

    self.velocity = oldVelocity;
    self.v_angle = vectoangles ( self.velocity );
    self.angles = self.v_angle;
    self.think = HomingMissileAcquire;
    self.nextthink = time + 0.2;
};

//================================
//================================
void(float offset, float frameNum) MultiRocketLaunch =
{
    local entity     missile;
    local vector    aimangle;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "MultiRocket";
    missile.delay = time + 4;
    missile.frame = frameNum;
    missile.touch = MultiRocketTouch;

    if (deathmatch || coop)
        setmodel (missile, "progs/rockup_d.mdl");
    else
        setmodel (missile, "progs/rockup.mdl");

    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, self.origin + v_forward*8 + '0 0 16');

    if ( coop || deathmatch)
    {
        aimangle = self.v_angle;
        aimangle_y = aimangle_y + (offset * 0.66);
        makevectors (aimangle);
        missile.velocity = aim(self, 1000);
        missile.velocity = missile.velocity * 1000;
        missile.angles = vectoangles(missile.velocity);

        missile.think = MultiRocketExplode;
        missile.nextthink = time + 4;
    }
    else
    {
        makevectors (self.v_angle);
        missile.velocity = v_forward * 1000 - v_right*offset*8;
        missile.angles = vectoangles(missile.velocity);
        missile.v_angle = self.v_angle;

        aimangle = missile.origin + missile.velocity;
        traceline ( missile.origin, aimangle, FALSE, self );
        if (trace_fraction < 1)
        {
            if (trace_ent.flags & FL_MONSTER)
            {
                missile.enemy = trace_ent;
                missile.think = HomingMissileThink;
                return;
            }
        }

        missile.think = HomingMissileAcquire;
        missile.nextthink = time + 0.1;
    }
};

//================================
//================================
void() W_FireMultiRocket =
{
    // TODO VR: (P2) rogue
    // self.currentammo = self.ammo_multi_rockets = self.ammo_multi_rockets - 1;
    self.currentammo = self.ammo_rockets = self.ammo_rockets - 1;

    // TODO VR: (P2) rogue
    // UpdateAmmoCounts (self);

    sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    MultiRocketLaunch ( -10, 2 );
    MultiRocketLaunch ( -5, 3 );
    MultiRocketLaunch ( 5, 0 );
    MultiRocketLaunch ( 10, 1 );
};

//=============================================================================
// Plasma Gun Code
//=============================================================================
void(vector p1, vector p2, entity from, float damage) PlasmaDamage =
{
    local entity        e1, e2;
    local vector        f;

    f = p2 - p1;
    normalize (f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;

    traceline (p1, p2, FALSE, self);
    if (trace_ent.takedamage)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from.owner, damage);
        if (self.classname == "player")
        {
            if (other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline (p1 + f, p2 + f, FALSE, self);
    if (trace_ent != e1 && trace_ent.takedamage)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from.owner, damage);
    }
    e2 = trace_ent;

    traceline (p1 - f, p2 - f, FALSE, self);
    if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from.owner, damage);
    }
};

//================================
//================================
void(entity current, float doDamage) PlasmaDischarge =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity (MSG_BROADCAST, current);
    WriteByte (MSG_BROADCAST, 0); /* disambiguator */
    WriteCoord (MSG_BROADCAST, current.origin_x);
    WriteCoord (MSG_BROADCAST, current.origin_y);
    WriteCoord (MSG_BROADCAST, current.origin_z);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    sound (self, CHAN_VOICE, "weapons/lhit.wav", 1, ATTN_NORM);
    if (doDamage == 1)
        PlasmaDamage (self.origin, current.origin, self, 50);
};

//================================
//================================
void() PlasmaGroundOut =
{
    local entity     current, start;
    local float        monstersHit;

    monstersHit = 0;
    current = findradius ( self.origin, 320 );
    start = current;
    while ( monstersHit < 5 )
    {
        if ( current.flags & FL_MONSTER || current.classname == "player")
        {
            if ( current != self.owner )
            {
                traceline ( self.origin, current.origin, TRUE, world );
                if (trace_fraction == 1)
                {
                    monstersHit = monstersHit + 1;
                    PlasmaDischarge ( current, 1 );
                }
            }
        }
        current = current.chain;
        if (start == current || !current)
            return;
    }
};

//================================
//================================
void() PlasmaTouch =
{
    local float    damg;

    if (other == self.owner)
        return;        // don't explode on owner

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    damg = 80 + random()*20;
    sound (self, CHAN_WEAPON, "plasma/explode.wav", 1, ATTN_NORM);

    if (other.health)
    {
        if (other.classname == "monster_shambler")
            damg = damg * 0.5;    // mostly immune
        T_Damage (other, self, self.owner, damg );
    }

    // don't do radius damage to the other, because all the damage
    // was done in the impact
    T_RadiusDamage (self, self.owner, 70, other);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    WriteByte (MSG_BROADCAST, 244);
    WriteByte (MSG_BROADCAST, 3);

    PlasmaGroundOut();
    remove (self);
};

//================================
//================================
void() PlasmaLaunch =
{
    self.velocity = normalize(self.velocity);
    self.velocity = self.velocity * 1250;

    self.nextthink = time + 5;
    self.think = SUB_Remove;
};

void(vector org, vector dir) launch_plasma =
{
    local entity    missile;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "plasma";

// set missile speed
    missile.velocity = dir * 0.01;
    missile.avelocity = '300 300 300';
    missile.angles = vectoangles(missile.velocity);
    missile.touch = PlasmaTouch;

    setmodel (missile, "progs/plasma.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, org);

    sound (missile, CHAN_WEAPON, "plasma/flight.wav", 1, ATTN_NORM);
    if (!deathmatch && !coop)
        missile.effects = EF_BRIGHTLIGHT;

// set missile duration
    missile.think = PlasmaLaunch;
    missile.nextthink = time + 0.1;
};

//================================
//================================

// TODO VR: (P2) rogue
/*
void() W_FirePlasma =
{
    local float        cells;
    local vector     dir;

    if (self.ammo_plasma < 1)
    {
        self.weapon = W_BestWeapon ();
        W_SetCurrentAmmo ();
        return;
    }

// explode if under water
    if (self.waterlevel > 1)
    {
        cells = self.ammo_plasma;
        self.ammo_plasma = 0;
        W_SetCurrentAmmo ();
        T_RadiusDamage (self, self, 35*cells, world);
        return;
    }
    self.currentammo = self.ammo_plasma = self.ammo_plasma - 1;

    // TODO VR: (P2) rogue
    // UpdateAmmoCounts (self);
    sound (self, CHAN_WEAPON, "plasma/fire.wav", 0.5, ATTN_NORM);
    self.punchangle_x = -2;

    makevectors (self.v_angle);
    dir = aim ( self, 1000 );
    launch_plasma (self.origin + v_forward*24 + '0 0 16', dir);
};
*/
