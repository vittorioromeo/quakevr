/*
==============================================================================
Automated Gun Turret
by Carl Glave aka. General WarT'
for Quake: The Apocalypse
==============================================================================
*/

$cd apoc/progs/turret
$origin 0 0 10
$base base        
$skin base

// The tilt according to the angle of that frame's gun tilt
$frame tilt0 tilt6 tilt12 tilt18
$frame tilt24 tilt30 tilt36 tilt42
$frame tilt48 tilt54 tilt60 tilt66
$frame tilt72 tilt78 tilt84 tilt90

$frame retract1 retract2 retract3 retract4 retract5 retract6 retract7

//=============================================================================

// returns the appropriate frame for the passed pitch
float(float pitch) turret_frame=
{return floor(pitch/5.625);};

// returns the appropriate frame for while firing
float(float pitch) turret_fireframe=
{return (turret_frame(pitch) + 23);};

// Check to see if the turret will fire
float() CheckTurretAttack =
{
    local vector    spot1, spot2;    
    local entity    targ;
    local float        chance;

    targ = self.enemy;
    
// see if any entities are in the way of the shot
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;

    traceline (spot1, spot2, FALSE, self);

    if (trace_ent != targ)
        return FALSE;        // don't have a clear shot
            
    if (trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents

    if (time < self.attack_finished)
        return FALSE;
        
    if (enemy_range == RANGE_MELEE)
        chance = 0.9;
    else if (enemy_range == RANGE_NEAR)
        chance = 0.5;
    else if (enemy_range == RANGE_MID)
        chance = 0.2;
    else
        chance = 0.05;

    if (random () < chance)
    {
        self.th_missile();
        if(self.spawnflags & TUR_LASERS)
            SUB_AttackFinished (1.5 + 2*random());
        if(self.spawnflags & TUR_ROCKETS)
            SUB_AttackFinished (1.5 + 3*random());
        if(self.spawnflags & TUR_PPC)
            SUB_AttackFinished (2 + 2*random());
        else
            SUB_AttackFinished (1 + 1.5*random());
        return TRUE;
    }

    return FALSE;
};

void() turret_face =
{
    local vector tmpvec;
    local float idealframe;

    tmpvec = vectoangles(self.origin - (self.enemy.origin + self.enemy.view_ofs));
    if(tmpvec_x > self.rotate_type)
    {
        if(tmpvec_x > (self.rotate_type + 6))
            self.rotate_type = self.rotate_type + 6;
        else
            self.rotate_type = tmpvec_x;
    }
    else if(tmpvec_x < self.rotate_type)
    {
        if(tmpvec_x < (self.rotate_type - 6))
            self.rotate_type = self.rotate_type - 6;
        else
            self.rotate_type = tmpvec_x;
    }

    self.frame = turret_frame(self.rotate_type);

    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    ChangeYaw ();
};

//=============================================================================



//=============================================================================

void() turret_active;
void() turret_ret2;

void() turret_still =    [ $tilt0, turret_still]
{
    self.frame = turret_frame(self.rotate_type);

    FindTarget ();
};

void() turret_ret1 =    [ $retract1, turret_ret1]
{
    if(self.frame >= 15)
    {
        turret_ret2();
        return;
    }

    self.frame = self.frame + 1;
    self.nextthink = time + 0.05;
};
void() turret_ret2 =    [ $retract1, turret_ret3] {self.nextthink = time + 0.05;};
void() turret_ret3 =    [ $retract2, turret_ret4] {self.nextthink = time + 0.05;};
void() turret_ret4 =    [ $retract3, turret_ret5] {self.nextthink = time + 0.05;};
void() turret_ret5 =    [ $retract4, turret_ret6]
{self.solid = SOLID_NOT;self.nextthink = time + 0.05;};
void() turret_ret6 =    [ $retract5, turret_ret7] {self.nextthink = time + 0.05;};
void() turret_ret7 =    [ $retract6, turret_ret8] {self.nextthink = time + 0.05;};
void() turret_ret8 =    [ $retract7, turret_retracted]
{if(!self.currentammo) self.think = SUB_Null;self.nextthink = time + 0.05;};

void() turret_unret1 =    [ $retract6, turret_unret2] {self.nextthink = time + 0.05;};
void() turret_unret2 =    [ $retract5, turret_unret3] {self.nextthink = time + 0.05;};
void() turret_unret3 =    [ $retract4, turret_unret4] {self.nextthink = time + 0.05;};
void() turret_unret4 =    [ $retract3, turret_unret5]
{
    self.solid = SOLID_SLIDEBOX;
    setmodel (self, "progs/turret.mdl");
    self.nextthink = time + 0.05;
};
void() turret_unret5 =    [ $retract2, turret_unret6] {self.nextthink = time + 0.05;};
void() turret_unret6 =    [ $retract1, turret_unret7] {self.nextthink = time + 0.05;};
void() turret_unret7 =    [ $tilt0, turret_unret7]
{
    if(self.frame > (turret_frame(self.rotate_type) + 1))
        self.frame = self.frame - 2;
    else
    {
        self.frame = turret_frame(self.rotate_type);
        turret_active();
    }
};

void() turret_retracted =    [ $retract7, turret_retracted]
{
    if(FindTarget ())
        turret_unret1();
};

void() turret_active =    [ $tilt0, turret_active]
{
    enemy_infront = infront(self.enemy);
    enemy_range = range(self.enemy);
    enemy_yaw = vectoyaw(self.enemy.origin - self.origin);

    self.frame = turret_frame(self.rotate_type);

    local    vector    delta;
    local    float    axis;
    local    float    direct, ang_rint, ang_floor, ang_ceil;

// see if the enemy is dead
    if (self.enemy.health <= 0 || self.enemy.deadflag != 0)
    {
        self.enemy = world;
    // FIXME: look all around for other targets
        if (self.oldenemy.health > 0)
        {
            self.enemy = self.oldenemy;
            HuntTarget ();
        }
        else
        {
            self.th_stand();
            return;
        }
    }

      self.show_hostile = time + 1;       // wake up other monsters

// check knowledge of enemy
    enemy_vis = visible(self.enemy);
    if (enemy_vis)
        self.search_time = time + 5;

// look for other coop players
   if (coop && self.search_time < time)
    {
        if (FindTarget ())
            return;
    }

    if (self.attack_state == AS_MISSILE || self.attack_state == AS_MELEE)
    {
        ai_run_missile ();
        return;
    }

    CheckTurretAttack ();
};

//=============================================================================

void() turret_bullet1 = [ $tilt0, turret_bullet2]
{
    local vector dir;

    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "turret/plasbult.wav", 1, ATTN_NORM);

      self.effects = self.effects | EF_MUZZLEFLASH;

    dir = self.enemy.origin - self.enemy.velocity*0.3*random();
    dir = normalize (dir - self.origin);
    
    FireBullets (2, dir, '0 0 0');
//    FireLaser(self.origin + v_forward*8 + v_right*4, dir, 3, self.color);
//    FireLaser(self.origin + v_forward*8 - v_right*4, dir, 3, self.color);
};
void() turret_bullet2 = [ $tilt0, turret_bullet3]
{turret_face();self.nextthink = time + 0.05;};
void() turret_bullet3 = [ $tilt0, turret_bullet4]
{
    local vector dir;

    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "turret/plasbult.wav", 1, ATTN_NORM);

      self.effects = self.effects | EF_MUZZLEFLASH;

    dir = self.enemy.origin - self.enemy.velocity*0.3*random();
    dir = normalize (dir - self.origin);
    
    FireBullets (2, dir, '0 0 0');
//    FireLaser(self.origin + v_forward*8 + v_right*4, dir, 3, self.color);
//    FireLaser(self.origin + v_forward*8 - v_right*4, dir, 3, self.color);
};
void() turret_bullet4 = [ $tilt0, turret_bullet5]
{turret_face();self.nextthink = time + 0.05;};
void() turret_bullet5 = [ $tilt0, turret_active]
{
    local vector dir;

    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "turret/plasbult.wav", 1, ATTN_NORM);

      self.effects = self.effects | EF_MUZZLEFLASH;

    dir = self.enemy.origin - self.enemy.velocity*0.3*random();
    dir = normalize (dir - self.origin);
    
    FireBullets (2, dir, '0 0 0');
//    FireLaser(self.origin + v_forward*8 + v_right*4, dir, 3, self.color);
//    FireLaser(self.origin + v_forward*8 - v_right*4, dir, 3, self.color);
};

void() turret_laser1 =[ $tilt0, turret_laser2]
{
    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);
      self.effects = self.effects | EF_MUZZLEFLASH;

    newmis = launch_missile("progs/laser.mdl", self.origin, '600 0 0');
    target_missile(newmis, self.enemy.origin, 1);
};
void() turret_laser2 =[ $tilt0, turret_laser3] {turret_face();};
void() turret_laser3 =[ $tilt0, turret_laser4] {turret_face();};
void() turret_laser4 =[ $tilt0, turret_laser5] {turret_face();};
void() turret_laser5 =[ $tilt0, turret_active]
{
    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "enforcer/enfire.wav", 1, ATTN_NORM);
      self.effects = self.effects | EF_MUZZLEFLASH;

    newmis = launch_missile("progs/laser.mdl", self.origin, '600 0 0');
    target_missile(newmis, self.enemy.origin, 1);
};

void() turret_rocket1 =[ $tilt0, turret_active]
{
    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

    sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);
      self.effects = self.effects | EF_MUZZLEFLASH;

    newmis = launch_missile("progs/missile.mdl", self.origin, '1000 0 0');
    target_missile(newmis, self.enemy.origin, 1);
    setdmg_missile(newmis, 15, 15);
};

void() turret_beam1 = [ $tilt0, turret_beam2]
{
    turret_face();
    sound (self, CHAN_WEAPON, "turret/beamchrg.wav", 1, ATTN_NORM);
    self.nextthink = time + 0.2;
};
void() turret_beam2 = [ $tilt0, turret_beam3]
{
    sound (self, CHAN_WEAPON, "turret/beamchrg.wav", 1, ATTN_NORM);
    turret_face();
    self.nextthink = time + 0.2;
};
void() turret_beam3 = [ $tilt0, turret_beam4]
{
    sound (self, CHAN_WEAPON, "turret/beamchrg.wav", 1, ATTN_NORM);
    turret_face();
    self.nextthink = time + 0.2;
};
void() turret_beam4 = [ $tilt0, turret_beam5]
{
    turret_face();
    self.frame = turret_fireframe(self.rotate_type);

// explode if under water
    if (self.waterlevel > 1)
    {
        beam_water_burst(20);
        return;
    }

    newmis = spawn();
    newmis.solid = SOLID_NOT;
    newmis.movetype = MOVETYPE_FLY;
    newmis.currentammo = 10;
    setorigin(newmis, self.origin);
    newmis.owner = self;
    newmis.think = beam_generate;
    newmis.nextthink = time + 0.05;
};
void() turret_beam5 = [ $tilt0, turret_beam6] {turret_face();};
void() turret_beam6 = [ $tilt0, turret_beam7] {turret_face();};
void() turret_beam7 = [ $tilt0, turret_active] {turret_face();};

void() turret_fire2;
void() turret_fire1 =    [ $tilt0, turret_fire2]
{
    turret_face();

    if(infront(self.enemy))
        self.think = turret_fire2;
    self.nextthink = time + 0.05;
};
void() turret_fire2 =    [ $tilt0, turret_fire3]
{
    turret_face();
    self.nextthink = time + 0.05;
};
void() turret_fire3 =    [ $tilt0, turret_fire4]
{
    turret_face();
    self.nextthink = time + 0.05;
};
void() turret_fire4 =    [ $tilt0, turret_fire4]
{
    turret_face();

    if(self.spawnflags & TUR_LASERS)
        turret_laser1();
    else if(self.spawnflags & TUR_ROCKETS)
        turret_rocket1();
    else if(self.spawnflags & TUR_PPC)
        turret_beam1();
    else
        turret_bullet1();
};

//=============================================================================

void(entity attacker, float damage)    turret_pain =
{
    if (self.pain_finished > time)
        return;
    self.pain_finished = time + 1;

    sound (self, CHAN_VOICE, "turret/beamchrg.wav", 1, ATTN_NORM);
    ThrowGib ("progs/turetgib.mdl", 0 - damage);
};

void() turret_die=
{
    local entity dedtur;

    self.use = SUB_Null;

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    dedtur = spawn();
    dedtur.solid = SOLID_NOT;
    dedtur.movetype = MOVETYPE_NONE;
    dedtur.angles = self.angles;
    setorigin(dedtur, self.origin);
    setmodel (dedtur, "progs/dedturet.mdl");
    setsize (self, '-10 -10 -3', '10 10 12');
    dedtur.skin = self.skin;

    ThrowHead ("progs/h_turret.mdl", self.health);
    ThrowGib ("progs/turetgib.mdl", self.health);
    ThrowGib ("progs/turetgib.mdl", self.health);
    ThrowGib ("progs/turetgib.mdl", self.health);
    ThrowGib ("progs/turetgib.mdl", self.health);
};

void() turret_use=
{
    if(self.currentammo)
    {
        self.currentammo = FALSE;
        if((self.spawnflags & TUR_RETRACT) || (self.spawnflags & TUR_OFFRET))
            turret_ret1();
        else
            self.think = SUB_Null;
    }
    else
    {
        self.currentammo = TRUE;
        if((self.spawnflags & TUR_RETRACT) || (self.spawnflags & TUR_OFFRET))
            turret_unret1();
        else
            turret_still();
    }
};

void() turret_setup=
{
    local vector endorg;

    traceline(self.origin, self.origin + '0 0 2000', TRUE, self);

    endorg = trace_endpos;
    endorg_z = endorg_z - 12;
    setorigin(self, endorg);

    // decide how to start according to the spawnflags
    if(self.spawnflags & TUR_STARTOFF)
    {
        if((self.spawnflags & TUR_RETRACT) || (self.spawnflags & TUR_OFFRET))
        {
            self.frame = $retract7;
            self.solid = SOLID_NOT;
        }
        else
            self.frame = turret_frame(self.rotate_type);

        self.currentammo = FALSE;
        self.think = SUB_Null;
    }
    else
    {
        if(self.spawnflags & TUR_RETRACT)
        {
            self.solid = SOLID_NOT;
            turret_retracted();
        }
        else
            turret_still();

        self.nextthink = self.nextthink + random()*0.5;
    }
};

void() monster_turret=
{
    precache_model ("progs/turret.mdl");
    precache_model ("progs/h_turret.mdl");
    precache_model ("progs/dedturet.mdl");
    precache_model ("progs/turetgib.mdl");

    precache_sound ("turret/beamchrg.wav");
    precache_sound ("turret/beammode.wav");

    if(self.spawnflags & TUR_LASERS)
    {
        precache_model ("progs/laser.mdl");
        precache_sound ("enforcer/enfire.wav");
        precache_sound ("enforcer/enfstop.wav");
    }
    else if(self.spawnflags & TUR_LASERS)
    {
        precache_model ("progs/missile.mdl");
        precache_sound ("weapons/sgun1.wav");
    }
    else if(self.spawnflags & TUR_PPC)
    {
        precache_model ("progs/beam.mdl");
        precache_sound ("turret/beamhit.wav");
        precache_sound ("hipweap/mjolhit.wav");
    }
    else
        precache_sound ("turret/plasbult.wav");

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel (self, "progs/turret.mdl");
    if (self.deathstring == "") self.deathstring = " was blasted by a Turret\n";

    setsize (self, '-12 -12 -6', '12 12 12');
    if(self.health < 1)
        self.health = 100;
    self.max_health = self.health;
    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);
    if(self.rotate_type < 0 || self.rotate_type > 90)
        self.rotate_type = 0;
    self.currentammo = TRUE;

    if(self.spawnflags & TUR_RETRACT)
    {
        self.th_stand = turret_ret1;
        self.th_walk = turret_ret1;
    }
    else
    {
        self.th_stand = turret_still;
        self.th_walk = turret_still;
    }
    self.th_run = turret_active;
    self.th_pain = turret_pain;
    self.th_die = turret_die;
    self.th_missile = turret_fire1;

    total_monsters = total_monsters + 1;

    self.takedamage = DAMAGE_AIM;

    self.hittype    = HIT_METAL;
    self.ideal_yaw = self.angles * '0 1 0';
    self.yaw_speed = 30;
    self.view_ofs = '0 0 -0.1';
    self.use = turret_use;

    self.flags = self.flags | FL_FLY;
    self.flags = self.flags | FL_MONSTER;
    self.pausetime = 99999999;

    self.nextthink = self.nextthink + random()*0.5;
      self.think = turret_setup;
};
