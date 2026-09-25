
float() crandom =
{
    return 2 * (random() - 0.5);
};

// QUAKE VR
void(float xHand, float xduration, float frequency, float amplitude) VRGunHaptic =
{
    if(!self.ishuman)
    {
        return;
    }

    haptic(xHand, 0.0, xduration, frequency, amplitude);
}

void(vector org) BroadcastGunshotEffect =
{
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_GUNSHOT);
    WriteVec3(MSG_BROADCAST, org);
}

vector(float xHand, float factor) vrProjectileVelocity =
{
    if(VRHandsTracked(self))
    {
        // VR: Add hand velocity and player velocity to the final projectile
        // velocity, in order to carry momentum
        return (VRGetHandVel(xHand) * (110 * factor)) +
               ((self.velocity * 0.8) * pow(factor, 0.4));
    }

    return VEC_ORIGIN;
}

void(float xHand, float xAid) updateAmmoCounter =
{
    if(xAid == AID_NONE)
    {
        VRSetAmmoCounter(xHand, 0);
        VRSetWeaponClipSize(xHand, 0);
    }
    else
    {
        VRSetAmmoCounter(xHand, VR_AmmoUtil_GetAmmoCountByAmmoId(self, xAid));
        VRSetWeaponClipSize(xHand, WeaponIdToBaseClipSize(VRGetWeapon(xHand)));
    }
}

float(float xHand) getCurrentAmmoCount =
{
    float ammoId = WeaponIdToAmmoId(VRGetWeapon(xHand), VRGetWeaponFlags(xHand));
    return VR_AmmoUtil_GetAmmoCountByAmmoId(self, ammoId);
}

float(float xHand) getCurrentAmmoOrClip =
{
    if(VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        return VRGetWeaponClip(xHand);
    }
    else
    {
        return getCurrentAmmoCount(xHand);
    }
}

void() updateAmmoCounters =
{
    updateAmmoCounter(cVR_OffHand, VRGetCurrentAmmo(cVR_OffHand));
    updateAmmoCounter(cVR_MainHand, VRGetCurrentAmmo(cVR_MainHand));
}

void(float xHand, float xAid, float xValue) addAmmoAndUpdateCounter =
{
    VR_AmmoUtil_AddAmmoCountByAmmoId(self, xAid, xValue);
    updateAmmoCounters();
    // updateAmmoCounter(xHand, xAid);
}

void(float xHand, float xAid, float xValue) delAmmoAndUpdateCounter =
{
    VR_AmmoUtil_DelAmmoCountByAmmoId(self, xAid, xValue);
    updateAmmoCounters();
    // updateAmmoCounter(xHand, xAid);
}

void(float xHand, float xAid, float xValue) delAmmoOrClipAndUpdateCounter =
{
    if(VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - xValue);

        if(VRGetWeaponClip(xHand) < 0)
        {
            bprint("ERROR: weapon clip less than zero\n");
            VRSetWeaponClip(xHand, 0);
        }
    }
    else
    {
        delAmmoAndUpdateCounter(xHand, xAid, xValue);
    }
}

float() VRMeleeDmgQuadMult =
{
    if(self.super_damage_finished > time)
    {
        return 2.75;
    }

    return 1;
}

// Melee: a swing of a hand faster than vr_melee_speed (m/s), in any direction, hits what is ahead
// of the hand (and of the weapon's end) along its motion, once per swing: the hand must slow down
// again before it hits anything else. The damage is the weapon's base at vr_melee_speed, growing
// with the speed up to three times that; a punch (the fist moving along its own forward, knuckles
// first) does vr_melee_punch_mult more (W_FistMelee).

// How hard the hand swings: its speed over vr_melee_speed, up to 3.
float(float velmag) VRMeleeStrengthMult =
{
    float threshold = max(0.1, cvar_hget(cvarh_vr_melee_speed));
    return min(3, velmag / threshold);
}

float(float xHand, float xBase) VRMeleeDmg =
{
    if(!VRHandsTracked(self) || !self.ishuman)
    {
        return 20;
    }

    return xBase * VRMeleeStrengthMult(VRGetHandVelMag(xHand)) * cvar_hget(cvarh_vr_melee_dmg_multiplier) *
           VRMeleeDmgQuadMult();
}

float(float v) VRMeleeRange =
{
    float mult;

    if(self.super_damage_finished > time)
    {
        mult = 1.35;
    }
    else
    {
        mult = 1.10;
    }

    if(!VRHandsTracked(self) || !self.ishuman)
    {
        mult = mult * 3.5;
    }

    return v * cvar_hget(cvarh_vr_melee_range_multiplier) * mult;
}

void(float hand, float velmag) VRMeleeHaptic =
{
    if(!self.ishuman)
    {
        return;
    }

    // Stronger and longer for harder swings.
    float power = VRMeleeStrengthMult(velmag); // 1 .. 3
    float amplitude = min(1.0, 0.35 + 0.25 * power);
    float xduration = min(0.9, 0.25 + 0.15 * power);

    VRGunHaptic(hand, xduration, 75.0, amplitude);
}

void (float xHand) VRMeleeHapticByHand =
{
    VRMeleeHaptic(xHand, VRGetHandVelMag(xHand));
};

float PositionalDamageFactor(float positionalDamageType)
{
    if(positionalDamageType == 0) // standard
    {
        return 1;
    }

    if(positionalDamageType == 1) // headshot
    {
        return cvar_hget(cvarh_vr_headshot_mult);
    }

    if(positionalDamageType == 2) // extremities
    {
        return cvar_hget(cvarh_vr_limbshot_mult);
    }

    if(positionalDamageType == 3) // legs
    {
        return cvar_hget(cvarh_vr_legshot_mult);
    }

    return 1;
}

// The head of a target that supports headshots, in its own frame: `forward` of its origin along its
// facing, `up` above it, `radius` its size. Measured on the models' standing frames (the centroid of
// their top vertices), with the old heights as a check. FALSE for targets without a head to aim at.
float xHeadForward;
float xHeadUp;
float xHeadRadius;
float(entity xTarget) PositionalHead =
{
    string c = xTarget.classname;
    if(c == "monster_army")             { xHeadForward = 3;  xHeadUp = 23; xHeadRadius = 7.2; return TRUE; }
    if(c == "monster_ogre")             { xHeadForward = 9;  xHeadUp = 33; xHeadRadius = 10;  return TRUE; }
    if(c == "monster_enforcer")         { xHeadForward = 2;  xHeadUp = 27; xHeadRadius = 7.4; return TRUE; }
    if(c == "monster_gremlin")          { xHeadForward = 7;  xHeadUp = 12; xHeadRadius = 6.4; return TRUE; }
    if(c == "monster_hell_knight")      { xHeadForward = 6;  xHeadUp = 33; xHeadRadius = 7.2; return TRUE; }
    if(c == "monster_knight")           { xHeadForward = 4;  xHeadUp = 22; xHeadRadius = 6.2; return TRUE; }
    if(c == "monster_shambler")         { xHeadForward = 14; xHeadUp = 57; xHeadRadius = 12;  return TRUE; }
    if(c == "monster_shalrath")         { xHeadForward = 1;  xHeadUp = 34; xHeadRadius = 9;   return TRUE; }
    if(c == "monster_zombie")           { xHeadForward = 0;  xHeadUp = 22; xHeadRadius = 6;   return TRUE; }
    if(c == "monster_wizard")           { xHeadForward = 6;  xHeadUp = 25; xHeadRadius = 7;   return TRUE; }
    if(c == "monster_mummy")            { xHeadForward = 0;  xHeadUp = 30; xHeadRadius = 6.5; return TRUE; }
    if(c == "player")                   { xHeadForward = 4;  xHeadUp = 23; xHeadRadius = 7.2; return TRUE; }
    return FALSE;
};

// Where a shot entering `xTarget` at `xEndpos` (a point on its box, from a trace) along
// `xDirection` hits: 1 the head, 2 an extremity (far from its middle), 3 the legs, 0 the body.
// The shot is a ray through the target: the head is a sphere turned with the target's facing
// (heads sit forward of the origin: a shambler's by 14 units), so hits are found the same whatever
// the angle, rather than by pushing the entry point a fixed depth into the box. A headshot also
// gives its feedback (PositionalHeadshotFeedback).
float PositionalDamage(entity xTarget, vector xEndpos, vector xDirection) =
{
    if(cvar_hget(cvarh_vr_positional_damage) == 0 || !PositionalHead(xTarget))
    {
        return 0;
    }

    vector d = normalize(xDirection);
    if(d == '0 0 0')
    {
        return 0;
    }

    float yaw = xTarget.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vector head = xTarget.origin + facing * xHeadForward + '0 0 1' * xHeadUp;

    // The ray's closest approach to the head, not behind where it entered.
    float t = max(0, (head - xEndpos) * d);
    if(vlen(head - (xEndpos + d * t)) < xHeadRadius)
    {
        PositionalHeadshotFeedback(xTarget);
        return 1; // headshot
    }

    // The ray's closest approach to the target's vertical axis: how far out, and how high.
    vector dh = d;
    dh_z = 0;
    float th = 0;
    if(vlen(dh) > 0.01)
    {
        vector toAxis = xTarget.origin - xEndpos;
        toAxis_z = 0;
        th = max(0, (toAxis * dh) / (dh * dh));
    }
    vector q = xEndpos + d * th;
    vector off = q - xTarget.origin;
    off_z = 0;

    if(vlen(off) > (xTarget.maxs_x - xTarget.mins_x) * 0.35)
    {
        return 2; // extremities
    }

    if(q_z < xTarget.origin_z)
    {
        return 3; // legs
    }

    return 0; // standard
}

// A headshot's feedback to the player who made it (vr_headshot_sound, the volume), once per frame (a
// shotgun's pellets): a sharp crack (Misc/quakevr/make_sounds.py) at the player, so that it is heard
// over the gunfire.
float positionalHeadshotTime;
void(entity xTarget) PositionalHeadshotFeedback =
{
    float v = cvar_hget(cvarh_vr_headshot_sound);
    if(v <= 0 || time == positionalHeadshotTime)
    {
        return;
    }
    positionalHeadshotTime = time;

    entity shooter = self;
    if(shooter.classname != "player")
    {
        shooter = self.owner;
    }
    if(shooter.classname != "player" || shooter == xTarget)
    {
        return;
    }

    sound(shooter, CHAN_AUTO, "vr/headshot.wav", min(1, v), ATTN_NORM);
};

// HIPNOTIC
//MED 10/18/96
/*
================
HIPNOTIC WEAPONS
================
*/
void() HIP_LaserTouch =
{
    vector org;
    vector spot1,spot2;
    vector oldvel;
    float r;
    float adjDmg;
    float posDmgType;

    self.owner = world;
    self.cnt = self.cnt + 1;
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    oldvel = normalize(self.old_velocity);
    spot1 = self.origin - (16*oldvel);
    spot2 = self.origin + (16*oldvel);
    traceline(spot1, spot2, FALSE, self);  // see through other monsters
    self.origin = trace_endpos;

    org = self.origin;

    if(other.health)
    {
        if(self.lastvictim == other)
        {
            self.dmg = self.dmg / 2;
        }

        spawn_touchblood(self.dmg);

        makeforward(self.angles);
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = self.dmg * PositionalDamageFactor(posDmgType);

        T_Damage(other, self, self.lastvictim, adjDmg);
    }
    else if((self.cnt == 3) || (random()<0.15))
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_GUNSHOT);
        WriteVec3(MSG_BROADCAST, org);
    }
    else
    {
       self.dmg = 0.9 * self.dmg;
       self.velocity = oldvel+(2*trace_plane_normal);
       self.velocity = normalize(self.velocity);
       self.velocity = self.speed * self.velocity;
       self.old_velocity = self.velocity;
       if(self.flags & FL_ONGROUND)
           self.flags = self.flags - FL_ONGROUND;
       r = random();
       sound(self, CHAN_WEAPON, "hipweap/laserric.wav", 1, ATTN_STATIC);
       return;
    }

    sound(self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
    remove(self);
};

void() HIP_LaserThink =
{
    if(time > self.attack_finished)
    {
        remove(self);
        return;
    }

    if(self.flags & FL_ONGROUND)
    {
        self.flags = self.flags - FL_ONGROUND;
    }

    self.velocity = self.old_velocity;
    self.angles = vectoangles(self.velocity);
    self.nextthink = time + 0.1;
};

entity(vector org, vector vec, vector extraVel, float light) HIP_LaunchLaser =
{
    sound(self, CHAN_WEAPON, "hipweap/laserg.wav", 1, ATTN_NORM);

    vec = normalize(vec);

    entity newmis;
    newmis = spawn();
    newmis.owner = self;
    newmis.classname = "hiplaser";
    newmis.lastvictim = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    if(light)
       newmis.effects = EF_DIMLIGHT;

    setmodel(newmis, "progs/lasrspik.mdl");
    setsize(newmis, '0 0 0', '0 0 0');

    setorigin(newmis, org);

    newmis.speed = 1750;
    newmis.dmg = 18;
    newmis.velocity = vec * newmis.speed + extraVel;
    newmis.old_velocity = newmis.velocity;
    newmis.angles = vectoangles(newmis.velocity);
    newmis.avelocity = '0 0 400';

    newmis.nextthink = time;
    newmis.attack_finished = time + 5;
    newmis.think = HIP_LaserThink;
    newmis.touch = HIP_LaserTouch;
    newmis.count = 0;

    return newmis;
};

/*
=================
HIP_FireLaser
=================
*/
void(float xHand, float stat) HIP_FireLaser =
{
    vector org;
    vector dir;
    vector out;
    float ofs;
    float aofs;

    if(!VRGetFireButtonPressed(xHand))
    {
        player_run(xHand);
        return;
    }

    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }

    SuperDamageSound();
    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors(VRGetWeaponFireRot(xHand));

    ofs = 6;
    out = v_forward;
    out_z = 0;
    out = normalize(out);
    org = VRGetMuzzlePos(xHand) + ((-1) * v_up) + (2*out);
    dir = VRGetWeaponFireRotFwd(xHand);
    aofs = ofs * 0.15;

    entity newmis;
    vector extraVel;

    extraVel = vrProjectileVelocity(xHand, 0.9);

    if(stat == 0)
    {
        delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 1);

        org = org + (aofs*v_right);
        newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
        org = org - (2*aofs*v_right);
        newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
    }
    else if(stat == 1)
    {
        delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 1);

        org = org + (ofs*(v_up * 0.4));

        if(random() < 0.1)
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 1);
            newmis.dmg = 25;
        }
        else
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
        }
    }

    self.punchangle_x = -1;
    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
};

/*
=================
HIP_HammerDamage
=================
*/
void(vector p1, vector p2, entity from, float damage) HIP_HammerDamage =
{
    entity        e1, e2;
    vector        f;

    f = p2 - p1;
    normalize(f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;

    traceline(p1, p2, FALSE, self);
//JIM
    if(trace_ent.takedamage && !trace_ent.wetsuit_finished)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage(trace_ent, from, from, damage);
        if(self.classname == "player")
        {
            if(other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline(p1 + f, p2 + f, FALSE, self);
//JIM
    if(trace_ent != e1 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage(trace_ent, from, from, damage);
    }
    e2 = trace_ent;

    traceline(p1 - f, p2 - f, FALSE, self);
//JIM
    if(trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage(trace_ent, from, from, damage);
    }
};

void(vector vStart, vector vEnd) LightningParticles =
{
    particle2(vStart, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 20);
    particle2(vStart, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
    particle2(vStart, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 4);

    particle2(vEnd, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 60);
    particle2(vEnd, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
    particle2(vEnd, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 12);
}


void() HIP_LightningThink =
   {
   vector org;
   vector dst;
   entity head;
   entity selected;
   float cur_dist;
   float head_dist;
   vector vec;
   float dot;
   float oldstate;
   float dam;


   if(time > self.delay)
      {
      if(self.enemy != world)
         self.enemy.struck_by_mjolnir = 0;
      remove(self);
      return;
      }
   oldstate = self.state;
   if(self.state==0)
      {
      // look in our immediate vicinity
      self.enemy = world;
      selected = world;
      cur_dist = self.distance;
      head = findradius(self.owner.origin, self.distance);
      while(head)
         {
         if(!(head.flags & FL_NOTARGET) && ((head.flags & FL_MONSTER) || (head.flags & FL_CLIENT)))
            {
//            if(visible(head) && (head!=self.owner.owner) && (head.health>0))
            if((visible(head)) && (head!=self.owner.owner) && (head.health>0))
               {
               head_dist = vlen(head.origin - self.lastvictim.origin);
               if((head_dist<cur_dist) && (head.struck_by_mjolnir==0))
                  {
                  selected = head;
                  cur_dist = head_dist;
                  }
               }
            }
         head = head.chain;
         }
      if(selected != world)
         {
         self.state = 1;
         self.enemy = selected;
         self.enemy.struck_by_mjolnir = 1;
         }
      else
         {
         vector   end;

         makevectors(self.v_angle);
         org = self.owner.origin;
         end = org + v_forward*200;
         end = end + (((400*random()) - 200) * v_right);

         traceline(org, end, TRUE, self);

         WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
         WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
         WriteEntity(MSG_BROADCAST, self);
         WriteByte(MSG_BROADCAST, 0); /* disambiguator */
         WriteVec3(MSG_BROADCAST, org);
         WriteVec3(MSG_BROADCAST, trace_endpos);

        LightningParticles(org, trace_endpos);

         self.nextthink = time + 0.1;
         return;
         }
      }
   org = self.lastvictim.origin;
   dst = self.enemy.absmin + 0.25*(self.enemy.absmax-self.enemy.absmin);
   dst = dst + (random()*0.5*(self.enemy.absmax-self.enemy.absmin));
   traceline(org, dst, TRUE, self.owner.owner);

   if(trace_fraction != 1.0 || self.enemy.health<=0)
      {
      self.enemy.struck_by_mjolnir = 0;
      self.state = 0;
      self.nextthink = time + 0.1;
      return;
      }
   WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
   WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
   WriteEntity(MSG_BROADCAST, self);
   WriteByte(MSG_BROADCAST, 0); /* disambiguator */
   WriteVec3(MSG_BROADCAST, org);
   WriteVec3(MSG_BROADCAST, trace_endpos);

   LightningParticles(org, trace_endpos);

   vec = normalize(self.enemy.origin - self.owner.origin);
   dot = vec * self.owner.movedir;
   head = self;
   self = self.owner.owner;
   if(oldstate==0)
      dam = 80;
   else
      dam = 30;
   if(dot>0.3)
      HIP_HammerDamage(org, trace_endpos, self, dam);
   else
      HIP_HammerDamage(org, trace_endpos, self, dam*0.5);
   self = head;

   self.nextthink = time + 0.2;
   };

void(entity prev, entity own, float dst) HIP_SpawnMjolnirLightning =
   {
   entity light;
   // spawn actual lightning
   light = spawn();
   light.delay = time + 0.8;
   light.state = 0;
   light.lastvictim = prev;
   light.distance = dst;
   light.owner = own;
   light.v_angle = self.angles;
   light.v_angle_x = 0;
   light.v_angle_z = 0;
   light.origin = own.origin;
   light.think = HIP_LightningThink;
   light.nextthink = time;
   };

void() HIP_SpawnMjolnirBase =
   {
   entity light;
   // spawn lightning base
   light = spawn();
//   light.origin = self.origin - '0 0 24' + (32*v_forward);
   light.origin = trace_endpos;
   light.flags = 0;
   light.owner = self;
   light.struck_by_mjolnir = 1;
   light.think = SUB_Remove;
   light.nextthink = time + 1;
   sound(light, CHAN_AUTO, "hipweap/mjolslap.wav", 1, ATTN_NORM);
   sound(light, CHAN_WEAPON, "hipweap/mjolhit.wav", 1, ATTN_NORM);
   makevectors(self.v_angle);
   light.movedir = v_forward;
   HIP_SpawnMjolnirLightning(light, light, 350);
   HIP_SpawnMjolnirLightning(light, light, 350);
   HIP_SpawnMjolnirLightning(light, light, 350);
   HIP_SpawnMjolnirLightning(light, light, 350);
   };

void(float xHand) HIP_FireMjolnirLightning =
{
    float cells;

    // explode if under water
    if(self.waterlevel > 1)
    {
        cells = self.ammo_cells;
        self.ammo_cells = 0;
        discharged = 1;
        T_RadiusDamage(self, self, 35*cells, world);
        discharged = 0;
        W_SetCurrentAmmo();
        return;
    }

    delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 15);

    HIP_SpawnMjolnirBase();
};

// What a swing of `xHand` hits: ahead of the hand, and of the weapon's end, along the hand's
// motion; things that bleed first. Sets the trace_ globals, and v_forward to the motion.
float(float xHand, float xRange, float xMuzzleRange) VRDoMeleeAttackTracelineFor =
{
    float range = VRMeleeRange(xRange);
    float muzzleRange = VRMeleeRange(xRange + xMuzzleRange);
    vector handPos = VRGetHandPos(xHand);
    vector muzzlePos = VRGetMuzzlePos(xHand);

    vector dir = normalize(VRGetHandVel(xHand));
    if(dir == '0 0 0')
    {
        makevectors(VRGetHandRot(xHand));
        dir = v_forward;
    }

    // A swing going downwards (reaching for a hip holster to reload) hits only things that
    // bleed: not the floor or a wall on its way.
    float passes = 2;
    if(dir_z < -0.35)
    {
        passes = 1;
    }

    for(float anything = 0; anything < passes; ++anything)
    {
        traceline(handPos - dir * 6, handPos + dir * range, FALSE, self);
        if(trace_fraction != 1.0 && (anything || trace_ent.takedamage))
        {
            v_forward = dir;
            return TRUE;
        }

        traceline(muzzlePos - dir * 6, muzzlePos + dir * muzzleRange, FALSE, self);
        if(trace_fraction != 1.0 && (anything || trace_ent.takedamage))
        {
            v_forward = dir;
            return TRUE;
        }
    }

    v_forward = dir;
    return FALSE;
}

void(float xHand) HIP_FireMjolnir =
{
    float xdmg = VRMeleeDmg(xHand, 25);

    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 12))
    {
        return;
    }

    vector org = trace_endpos - v_forward*4;

    if(trace_ent.takedamage)
    {
        VRMeleeHapticByHand(xHand);

        if(time >= VRGetAttackFinished(xHand) && self.ammo_cells >= 15)
        {
            HIP_FireMjolnirLightning(xHand);
            VRSetAttackFinished(xHand, time + 0.5);
        }

        if(trace_ent.classname == "monster_zombie")
        {
            xdmg *= 1.25;
        }

        trace_ent.axhitme = 1;
        SpawnBlood (org, v_forward, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    else
    {
        // hit wall
        if(trace_fraction != 1.0)
        {
            if(time >= VRGetAttackFinished(xHand) && self.ammo_cells >= 15)
            {
                HIP_FireMjolnirLightning(xHand);
                VRSetAttackFinished(xHand, time + 0.5);
            }

            if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
            {
                VRMeleeHapticByHand(xHand);
                sound(self, VRGetGunChannel(xHand), "hipweap/mjoltink.wav", 1, ATTN_NORM);
                BroadcastGunshotEffect(org);
                VRSetMeleeHitSoundPlayed(xHand, TRUE);
            }
        }
        else
        {
            if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
            {
                VRMeleeHapticByHand(xHand);
                sound(self, VRGetGunChannel(xHand), "knight/sword1.wav", 1, ATTN_NORM);
                VRSetMeleeHitSoundPlayed(xHand, TRUE);
            }
        }
    }
};


// called by worldspawn
void() W_Precache =
{
    precache_sound("weapons/r_exp3.wav");    // new rocket explosion
    precache_sound("weapons/rocket1i.wav");    // spike gun
    precache_sound("weapons/sgun1.wav");
    precache_sound("weapons/guncock.wav");    // player shotgun
    precache_sound("weapons/ric1.wav");    // ricochet (used in c code)
    precache_sound("weapons/ric2.wav");    // ricochet (used in c code)
    precache_sound("weapons/ric3.wav");    // ricochet (used in c code)
    precache_sound("weapons/spike2.wav");    // super spikes
    precache_sound("weapons/tink1.wav");    // spikes tink (used in c code)
    precache_sound("weapons/grenade.wav");    // grenade launcher
    precache_sound("weapons/bounce.wav");        // grenade bounce
    precache_sound("weapons/shotgn2.wav");    // super shotgun
    precache_sound("knight/sword1.wav");    // gun melee

    // HIPNOTIC
    //MED
   precache_sound("enforcer/enfstop.wav");   // laser cannon
//MED 11/06/96
   precache_sound("hipweap/laserg.wav");   // laser cannon
   precache_sound("hipweap/laserric.wav");   // laser cannon ricochet
   precache_sound("hipweap/proxwarn.wav");   // proximity bomb
   precache_sound("hipweap/proxbomb.wav");   // proximity bomb
//MED 11/19/96
   precache_sound("hipweap/mjolhit.wav");   // mjolnir
   precache_sound("hipweap/mjolslap.wav");   // mjolnir
   precache_sound("hipweap/mjoltink.wav");   // mjolnir

    // ROGUE
    //ZOID--
    // grapple
    precache_sound("weapons/chain1.wav");
    precache_sound("pendulum/hit.wav");
    precache_sound("lavagun/snail.wav");        // lava nail gun cooldown
//--ZOID

    // QUAKE VR
    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");
    precache_sound("weapons/holster0.wav");
    precache_sound("weapons/holster1.wav");
};

/*
================
W_FireAxe
================
*/
void(float xHand) W_FireAxe =
{
    vector org;
    float  xdmg;

    // HONEY
    entity zombie;
    float  zombie_dist;
    float  bleed; // TODO VR: (P2) apply bleed mechanic to other attacks
    bleed = FALSE;

    xdmg = VRMeleeDmg(xHand, 20);

    // HONEY
    // Check if we maybe should gib some zombies for fun!
    if(VR_GameUtil_EnableHoneyExtensions())
    {
        // Why on earth did I choose .wad?
        zombie = find(world, wad, ZOMBIE_ONGROUND);

        while(zombie)
        {
            if(zombie.classname == "monster_zombie")
            {
                zombie_dist = vlen(self.origin  - zombie.origin);

                if(zombie_dist <= 64)
                {
                    if(infront(zombie))
                    {
                        zombie.wad = "";
                        SpawnBloodSplash(zombie.origin, 20);
                        zombie.health = 0;
                        Killed(zombie, self);
                        bleed = TRUE;
                    }
                }
            }

            zombie = find(zombie, wad, ZOMBIE_ONGROUND);
        }
    }

    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 13))
    {
        return;
    }

    org = trace_endpos - v_forward*4;

    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;

        // HONEY
        self.damage_weapon = IT_AXE;

        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            SpawnBloodSplash(org, xdmg);
            bleed = TRUE;
        }

        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            sound(self, VRGetGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
    }

    if(!bleed)
    {
        // hit wall
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            sound(self, VRGetGunChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }

        BroadcastGunshotEffect(org);
    }
};


//============================================================================

/*
================
W_GunMelee
================
*/
void(float xHand) W_GunMelee =
{
    vector    org;
    float    xdmg;

    xdmg = VRMeleeDmg(xHand, 12);

    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 12))
    {
        return;
    }

    org = trace_endpos - v_forward*4;

    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        SpawnBloodSplash (org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);

        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            sound(self, VRGetGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
    }
    else
    {
        // hit wall
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            sound(self, VRGetGunChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }

        BroadcastGunshotEffect(org);
    }
};


//============================================================================


/*
================
W_FistMelee
================
*/
// A knight's sword (WID_SWORD): the axe's swing, longer and harder (vr_sword_damage_mult); the
// hell knight's (weaponflags) harder still.
void(float xHand) W_SwordMelee =
{
    float xdmg = VRMeleeDmg(xHand, 20) * cvar_hget(cvarh_vr_sword_damage_mult);
    if(VRHasBit(VRGetWeaponFlags(xHand), QVR_WPNFLAG_USE_SECONDARY_AMMO))
    {
        xdmg = xdmg * 1.25;
    }

    if(!VRDoMeleeAttackTracelineFor(xHand, 22, 30))
    {
        return;
    }

    vector org = trace_endpos - v_forward * 4;
    if(trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        self.damage_weapon = IT_AXE;
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            SpawnBloodSplash(org, xdmg);
        }
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            sound(self, VRGetGunChannel(xHand), "knight/sword2.wav", 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    // A wall: a clang and sparks.
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        sound(self, VRGetGunChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    particle2(org, trace_plane_normal * 60, QVR_PARTICLE_PRESET_SPARKS, 8);
};

// A knight drops its sword when it dies (vr_sword_drop): a weapon lying where its hand was.
void(float xHellKnight) VR_DropKnightSword =
{
    if(random() >= cvar_hget(cvarh_vr_sword_drop))
    {
        return;
    }
    makevectors(self.angles);
    vector at = self.origin + v_forward * 10 + v_right * 8 + '0 0 8';
    vector vel = v_forward * 60 + v_right * (crandom() * 40) + '0 0 120';
    CreateThrownWeapon(self, WID_SWORD, xHellKnight ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0, 0,
        at, self.angles, vel, randomVec(60),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_SWORD));
};

void(float xHand) W_FistMelee =
{
    vector org;
    float xdmg = VRMeleeDmg(xHand, 10);
    float playedsound = FALSE;

    // A box in hand is an improvised weapon (vr_carry.qc).
    if(VR_Carry_IsHolding(self, xHand))
    {
        xdmg = xdmg * cvar_hget(cvarh_vr_carry_melee_mult);
    }

    // A punch: the fist moving along its own forward.
    makevectors(VRGetHandRot(xHand));
    if(normalize(VRGetHandVel(xHand)) * v_forward >= 0.6)
    {
        xdmg = xdmg * cvar_hget(cvarh_vr_melee_punch_mult);
    }

    if(!VRDoMeleeAttackTracelineFor(xHand, 16, 8))
    {
        return;
    }

    org = trace_endpos - v_forward*4;

    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        sound(self, VRGetGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
        playedsound = TRUE;
    }

    if(trace_ent.takedamage)
    {
        VRMeleeHapticByHand(xHand);
        trace_ent.axhitme = 1;
        SpawnBloodSplash (org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
    }
    else
    {
        // hit wall
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            PainSound();
            SpawnBloodSplash (org, xdmg);
            playedsound = TRUE;
        }

        BroadcastGunshotEffect(org);
    }

    if(playedsound)
    {
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
};


//============================================================================



vector() wall_velocity =
{
    vector    vel;

    vel = normalize(self.velocity);
    vel = normalize(vel + v_up*(random()- 0.5) + v_right*(random()- 0.5));
    vel = vel + 2*trace_plane_normal;
    vel = vel * 200;

    return vel;
};


/*
================
SpawnMeatSpray
================
*/
void(vector org, vector vel) SpawnMeatSpray =
{
    entity missile;

    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_NOT;

    makevectors(self.angles);

    missile.velocity = vel;
    missile.velocity_z = missile.velocity_z + 250 + 50*random();

    missile.avelocity = '3000 1000 2000';

// set missile duration
    missile.nextthink = time + 1;
    missile.think = SUB_Remove;

    setmodel(missile, "progs/zom_gib.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, org);
};

/*
================
SpawnBlood
================
*/
void(vector org, vector vel, float damage) SpawnBlood =
{
    particle2(org, vel * 0.2, QVR_PARTICLE_PRESET_BLOOD, damage);
};

// QUAKE VR
/*
================
SpawnBloodSplash
================
*/
void(vector org, float damage) SpawnBloodSplash =
{
    vector pvel;

    pvel_x = crandom() * 240;
    pvel_y = crandom() * 240;
    pvel_z = random() * 120 + 100;

    SpawnBlood(org, pvel, damage);
};




/*
================
spawn_touchblood
================
*/
void(float damage) spawn_touchblood =
{
    vector    vel;

    vel = wall_velocity () * 0.2;
    SpawnBlood (self.origin + vel * 0.01, vel, damage);
};


/*
================
SpawnChunk
================
*/
void(vector org, vector vel) SpawnChunk =
{
    particle (org, vel * 0.02, 0, 10);
};

/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

entity    multi_ent;
float    multi_damage;

void() ClearMultiDamage =
{
    multi_ent = world;
    multi_damage = 0;
};

void() ApplyMultiDamage =
{
    if(!multi_ent)
        return;
    T_Damage(multi_ent, self, self, multi_damage);
};

float(entity hit, float damage) AddMultiDamage =
{
    if(!hit)
        return FALSE;

    if(hit != multi_ent)
    {
        ApplyMultiDamage();
        multi_damage = damage;
        multi_ent = hit;
    }
    else
        multi_damage = multi_damage + damage;

    // HONEY
    return !((hit.flags & FL_SPECIFICDAMAGE) && !(hit.items & self.damage_weapon));
};

/*
==============================================================================

BULLETS

==============================================================================
*/

/*
================
TraceAttack
================
*/
void(float damage, vector dir) TraceAttack =
{
    vector vel, org;

    // HONEY
    float bleed;
    bleed = FALSE;

    vel = normalize(dir + v_up*crandom() + v_right*crandom());
    vel = vel + 2*trace_plane_normal;
    vel = vel * 200;

    org = trace_endpos - dir*4;

    if(trace_ent.takedamage)
    {
        if(AddMultiDamage(trace_ent, damage))
        {
            SpawnBlood (org, vel*0.2, damage);
            bleed = TRUE;
        }
    }

    if(!bleed)
    {
        BroadcastGunshotEffect(org);
    }
};


/*
================
FireBullets

Used by shotgun, and super shotgun
Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void(float shotcount, vector dir, vector spread, vector src) FireBulletsImpl =
{
    vector direction;
    float adjDmg;
    float posDmgType;

    makevectors(vectoangles(dir));

    ClearMultiDamage();
    while(shotcount > 0)
    {
        direction = dir + crandom()*spread_x*v_right + crandom()*spread_y*v_up;

        traceline(src, src + direction*2048, FALSE, self);
        if(trace_fraction != 1.0)
        //JIM
        {
            posDmgType = PositionalDamage(trace_ent, trace_endpos, direction);
            adjDmg = 4 * PositionalDamageFactor(posDmgType);

            TraceAttack (adjDmg, direction);

            // HIPNOTIC
            if(trace_ent == world)
            {
                placebullethole(trace_endpos);
            }
        }

        shotcount = shotcount - 1;
    }
    ApplyMultiDamage();
};

void(float xHand, float shotcount, vector dir, vector spread) FireBullets =
{
    if(VRGet2HAiming(self))
    {
        spread *= cvar_hget(cvarh_vr_2h_spread_reduction);
    }

    FireBulletsImpl(shotcount, dir, spread, VRGetMuzzlePos(xHand));
}

void(float shotcount, vector dir, vector spread) EnemyFireBullets =
{
    vector    src;

    src = self.origin + v_forward*10;
    src_z = self.absmin_z + self.size_z * 0.7;

    FireBulletsImpl(shotcount, dir, spread, src);
};

/*
================
W_FireShotgun
================
*/
void(float xHand) W_FireShotgun =
{
    vector dir;

    sound(self, VRGetGunChannel(xHand), "weapons/guncock.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    delAmmoOrClipAndUpdateCounter(xHand, AID_SHELLS, 1);

    dir = VRGetWeaponFireRotFwd(xHand);

    FireBullets(xHand, 6, dir, '0.01 0.01 0');

    VRGunHaptic(xHand, 0.25, 70, 0.75);

    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);

    // HONEY
    self.damage_weapon = IT_SHOTGUN;
};


/*
================
W_FireSuperShotgun
================
*/
void(float xHand) W_FireSuperShotgun =
{
    vector dir;

    if(getCurrentAmmoOrClip(xHand) == 1)
    {
        W_FireShotgun(xHand);
        return;
    }

    sound(self, VRGetGunChannel(xHand), "weapons/shotgn2.wav", 1, ATTN_NORM);

    self.punchangle_x = -4;

    delAmmoOrClipAndUpdateCounter(xHand, AID_SHELLS, 2);
    dir = VRGetWeaponFireRotFwd(xHand);
    FireBullets(xHand, 14, dir, '0.035 0.025 0');

    VRGunHaptic(xHand, 0.55, 75, 1.0);

    particle2(VRGetMuzzlePos(xHand) - '0 2 0', '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    particle2(VRGetMuzzlePos(xHand) + '0 2 0', '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);

    // HONEY
    self.damage_weapon = IT_SUPER_SHOTGUN;
};


/*
==============================================================================

ROCKETS

==============================================================================
*/

void() s_explode1 = [0, s_explode2] { }
void() s_explode2 = [1, s_explode3] { }
void() s_explode3 = [2, s_explode4] { }
void() s_explode4 = [3, s_explode5] { }
void() s_explode5 = [4, s_explode6] { }
void() s_explode6 = [5, SUB_Remove] { }

void() BecomeExplosion =
{
    self.movetype = MOVETYPE_NONE;
    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    setmodel(self, "progs/s_explod.spr");
    self.solid = SOLID_NOT;
    s_explode1();
};

void() T_MissileTouch =
{
    float    damg;

    if(other == self.owner)
        return;        // don't explode on owner

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    damg = 100 + random()*20;

    // HONEY
    self.owner.damage_weapon = IT_ROCKET_LAUNCHER;

    if(other.health)
    {
        if(other.classname == "monster_shambler")
            damg = damg * 0.5;    // mostly immune
        T_Damage(other, self, self.owner, damg);
    }

    // don't do radius damage to the other, because all the damage
    // was done in the impact
    T_RadiusDamage(self, self.owner, 120, other);

//    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);

    BecomeExplosion();
};



/*
================
W_FireRocket
================
*/
entity(float xHand) W_FireRocket =
{
    entity missile;

    delAmmoOrClipAndUpdateCounter(xHand, AID_ROCKETS, 1);

    sound(self, VRGetGunChannel(xHand), "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";

    // set missile speed
    missile.velocity = VRGetWeaponFireRotFwd(xHand) * 1000;
    missile.angles = vectoangles(missile.velocity);

    missile.touch = T_MissileTouch;
    missile.velocity += vrProjectileVelocity(xHand, 0.4);

    // set missile duration
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;

    setmodel(missile, "progs/missile.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, VRGetMuzzlePos(xHand));

    // HIPNOTIC
    //MED 11/09/96

    VRGunHaptic(xHand, 0.5, 75, 1.0);

    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 3);

    return missile;
};

/*
===============================================================================

LIGHTNING

===============================================================================
*/

/*
=================
LightningDamage
=================
*/
void(vector p1, vector p2, entity from, float damage) LightningDamage =
{
    entity        e1, e2;
    vector        f;

    f = p2 - p1;
    normalize(f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;

    // HONEY
    self.damage_weapon = IT_LIGHTNING;

    traceline(p1, p2, FALSE, self);
//JIM
    if(trace_ent.takedamage && !trace_ent.wetsuit_finished)
    {
        if(T_Damage(trace_ent, from, from, damage))
        {
            SpawnBloodSplash (trace_endpos, damage*5);
        }

        if(self.classname == "player")
        {
            if(other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline(p1 + f, p2 + f, FALSE, self);
//JIM
    if(trace_ent != e1 && trace_ent.takedamage &&
    !trace_ent.wetsuit_finished)
    {
        if(T_Damage(trace_ent, from, from, damage))
        {
            SpawnBloodSplash (trace_endpos, damage*5);
        }
    }
    e2 = trace_ent;

    traceline(p1 - f, p2 - f, FALSE, self);
//JIM
    if(trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished)
    {
        if(T_Damage(trace_ent, from, from, damage))
        {
            SpawnBloodSplash (trace_endpos, damage*5);
        }
    }
};


void(float xHand) W_FireLightning =
{
    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }

    // explode if under water
    if(self.waterlevel > 1)
    {
        float cells = self.ammo_cells;
        self.ammo_cells = 0;

        // HIPNOTIC
        //MED 01/05/97 added discharge flag
        discharged = 1;
        T_RadiusDamage(self, self, 35 * cells, world);
        discharged = 0;
        W_SetCurrentAmmo();

        // HONEY
        self.damage_weapon = IT_LIGHTNING;

        return;
    }

    if(self.t_width < time)
    {
        sound(self, VRGetGunChannel(xHand), "weapons/lhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.6;
    }

    self.punchangle_x = -2;
    delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 1);

    vector org = VRGetMuzzlePos(xHand);

    makeforward(VRGetHandRot(xHand));
    traceline(org, org + v_forward*600, TRUE, self);
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 60);
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 12);
    LightningDamage(org, trace_endpos + v_forward*4, self, 30);

    VRGunHaptic(xHand, 0.4, 175, 1.0);
    particle2(org, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
};


//=============================================================================


void() GrenadeExplode =
{
    T_RadiusDamage(self, self.owner, 120, world);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);

    BecomeExplosion();
};

void() GrenadeTouch =
{
    if(other == self.owner)
        return;        // don't explode on owner

    // HONEY
    self.owner.damage_weapon = IT_GRENADE_LAUNCHER;

    if(other.takedamage == DAMAGE_AIM)
    {
        if(!((other.flags & FL_SPECIFICDAMAGE) && !(other.items & IT_GRENADE_LAUNCHER))) // HONEY
        {
            GrenadeExplode();
        }

        return;
    }

    if(vlen(self.velocity) > 10)
    {
        sound(self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM);    // bounce sound
    }

    if(self.velocity == '0 0 0')
        self.avelocity = '0 0 0';
};

/*
================
W_FireGrenade
================
*/
void(float xHand) W_FireGrenade =
{
    entity missile;
    vector tmpAngles;

    delAmmoOrClipAndUpdateCounter(xHand, AID_ROCKETS, 1);

    sound(self, VRGetGunChannel(xHand), "weapons/grenade.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_BBOX;
    missile.classname = "grenade";

    // set missile speed
    tmpAngles = VRGetWeaponFireRot(xHand);
    makevectors(tmpAngles);

    if(VRHandsTracked(self))
    {
        if(tmpAngles_x)
        {
            missile.velocity = v_forward*600 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
        }

        missile.velocity += vrProjectileVelocity(xHand, 0.6);
    }
    else
    {
        if(tmpAngles_x)
        {
            missile.velocity = v_forward*600 + v_up * 200 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
            missile.velocity_z = 200;
        }
    }

    missile.avelocity = '300 300 300';
    missile.angles = vectoangles(missile.velocity);
    missile.touch = GrenadeTouch;

    // set missile duration
    missile.nextthink = time + 2.5;
    missile.think = GrenadeExplode;

    setmodel(missile, "progs/grenade.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, VRGetMuzzlePos(xHand));

    VRGunHaptic(xHand, 0.4, 75, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);
};

void() ProximityExplode =
{
    T_RadiusDamage(self, self.owner, 95, world);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);

    BecomeExplosion();
};

//MED 10/31/96
float NumProximityGrenades;
/*
================
ProximityGrenadeExplode
================
*/
void() ProximityGrenadeExplode =
   {
   self.takedamage = DAMAGE_NO;
   NumProximityGrenades = NumProximityGrenades - 1;
   self.deathtype = "exploding";
   self.nextthink = time + 0.1;
   self.owner = self.lastvictim;
   self.think = ProximityExplode;
   };

/*
================
ProximityGrenadeTouch
================
*/
void() ProximityGrenadeTouch =
   {
   if(other == self)
      return;
   if(other.classname == self.classname)
      return;
   self.movetype = MOVETYPE_TOSS;
   if(self.state == 1)
      return;
   if(vlen(other.velocity) > 0)
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }
   if(other.takedamage == DAMAGE_AIM)
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }

        sound(self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM); // bounce sound

   self.movetype = MOVETYPE_NONE;
   setsize(self, '-8 -8 -8', '8 8 8');
   self.state = 1;
   self.spawnmaster = other;
   };

/*
================
ProximityBomb
================
*/
void() ProximityBomb =
   {
   entity   head;
   float    blowup;

   if((time > self.delay) || (NumProximityGrenades > 15) || (vlen(self.spawnmaster.velocity)>0))
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }
   self.owner = world;
   self.takedamage = DAMAGE_YES;
   head = findradius(self.origin, 140);
   blowup = 0;

   while(head)
      {
      if((head != self) && (head.health > 0) && (head.flags & (FL_CLIENT|FL_MONSTER)) && (head.classname!=self.classname))
         blowup = 1;
      if((head.classname == self.classname) && (head.state==0))
         blowup = 1;
      traceline(self.origin,head.origin,TRUE,self);
      if(trace_fraction != 1.0)
         blowup = 0;
      if(blowup==1)
         {
         sound(self, CHAN_WEAPON, "hipweap/proxwarn.wav", 1, ATTN_NORM);
         ProximityGrenadeExplode();
         self.nextthink = time + 0.5;
         return;
         }
      head = head.chain;
      }
   self.nextthink = time + 0.25;
   };

/*
================
W_FireProximityGrenade
================
*/
void(float xHand) W_FireProximityGrenade =
{
    entity missile;
    vector tmpAngles;

    NumProximityGrenades = NumProximityGrenades + 1;
    delAmmoOrClipAndUpdateCounter(xHand, AID_ROCKETS, 1);

    sound(self, VRGetGunChannel(xHand), "hipweap/proxbomb.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn();
    missile.owner = self;
    missile.lastvictim = self;
    missile.movetype = MOVETYPE_TOSS;
    missile.solid = SOLID_BBOX;
    missile.classname = "proximity_grenade";
    missile.takedamage = DAMAGE_NO;
    missile.health = 5;
    missile.state = 0;

    // set missile speed
    tmpAngles = VRGetWeaponFireRot(xHand);
    makevectors(tmpAngles);

    if(VRHandsTracked(self))
    {
        if(tmpAngles_x)
        {
            missile.velocity = v_forward*600 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
        }

        missile.velocity += vrProjectileVelocity(xHand, 0.4);
    }
    else
    {
        if(tmpAngles_x)
        {
            missile.velocity = v_forward*600 + v_up * 200 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
            missile.velocity_z = 200;
        }
    }

    missile.avelocity = '100 600 100';

    missile.angles = vectoangles(missile.velocity);

    missile.touch = ProximityGrenadeTouch;

    // set missile duration
    missile.nextthink = time + 2;
    missile.delay = time + 15 + (10*random());
    missile.think = ProximityBomb;
    missile.th_die = ProximityGrenadeExplode;

    setmodel(missile, "progs/proxbomb.mdl");
    setorigin(missile, VRGetMuzzlePos(xHand));
    setsize(missile, '-1 -1 -1', '1 1 1');

    VRGunHaptic(xHand, 0.4, 75, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);
};

//=============================================================================

/*
===============
launch_spike

Used for both the player and the ogre
===============
*/
entity(vector org, vector dir) launch_spike =
{
    entity newmis;

    newmis = spawn();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;

    newmis.angles = vectoangles(dir);

    newmis.touch = spike_touch;
    newmis.classname = "spike";
    newmis.think = SUB_Remove;
    newmis.nextthink = time + 6;
    setmodel(newmis, "progs/spike.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(newmis, org);

    newmis.velocity = dir * 1750;
    return newmis;
};

void(float xHand) W_FireSuperSpikes =
{
    vector dir;
    entity newmis;

    sound(self, VRGetGunChannel(xHand), "weapons/spike2.wav", 1, ATTN_NORM);
    VRSetAttackFinished(xHand, time + 0.2);
    delAmmoOrClipAndUpdateCounter(xHand, AID_NAILS, 2);

    makeforward(VRGetWeaponFireRot(xHand));
    dir = v_forward;

    newmis = launch_spike(VRGetMuzzlePos(xHand), dir);
    newmis.touch = superspike_touch;
    setmodel(newmis, "progs/s_spike.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
};

void(float xHand, float ox) W_FireSpikes =
{
    vector dir;
    entity newmis;

    makevectors(VRGetWeaponFireRot(xHand));

    if(getCurrentAmmoOrClip(xHand) >= 2 && VRGetWeapon(xHand) == WID_SUPER_NAILGUN)
    {
        W_FireSuperSpikes(xHand);
        return;
    }

    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }

    sound(self, VRGetGunChannel(xHand), "weapons/rocket1i.wav", 1, ATTN_NORM);
    VRSetAttackFinished(xHand, time + 0.2);
    delAmmoOrClipAndUpdateCounter(xHand, AID_NAILS, 1);
    dir = v_forward;
    newmis = launch_spike(VRGetMuzzlePos(xHand) + v_right*ox, dir);

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2(VRGetMuzzlePos(xHand) + v_right*ox, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
};

void() spike_bullethole =
{
    if(trace_ent == world)
    {
        placebullethole(trace_endpos);
    }
}

// According to Spike, mdls render wrongly in Quake 1. Their pitch angles are
// flipped (relative to the view, bsp models, or sprites).
void(vector ang) makevectors_mdl =
{
    ang_x *= -1;
    makevectors(ang);
}

void(vector ang) makeforward_mdl =
{
    ang_x *= -1;
    makeforward(ang);
}

void() spike_touch =
{
    float posDmgType;
    float adjDmg;

    // HONEY
    float bleed;
    bleed = FALSE;

    if(other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makeforward_mdl(self.angles);
    traceline(self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = 9 * PositionalDamageFactor(posDmgType);

        if(T_Damage(other, self, self.owner, adjDmg))
        {
            spawn_touchblood (adjDmg);
            bleed = TRUE;
        }
    }

    if(!bleed)
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);

        if(self.classname == "wizspike")
            WriteByte(MSG_BROADCAST, TE_WIZSPIKE);
        else if(self.classname == "knightspike")
            WriteByte(MSG_BROADCAST, TE_KNIGHTSPIKE);
        else
        {
            spike_bullethole();
            WriteByte(MSG_BROADCAST, TE_SPIKE);
        }

        WriteVec3(MSG_BROADCAST, self.origin);
    }

    remove(self);
};

void() superspike_touch =
{
    float posDmgType;
    float adjDmg;

    // HONEY
    float bleed;
    bleed = FALSE;

    if(other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makeforward_mdl(self.angles);
    traceline(self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = 18 * PositionalDamageFactor(posDmgType);

        if(T_Damage(other, self, self.owner, adjDmg))
        {
            spawn_touchblood (adjDmg);
        }
    }

    if(!bleed)
    {
        spike_bullethole();
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_SUPERSPIKE);
        WriteVec3(MSG_BROADCAST, self.origin);
    }

    remove(self);

};


/*
===============================================================================

PLAYER WEAPON USE

===============================================================================
*/

void(float xWeapon, float xWeaponFlags,
     float* xCurrentAmmo, string* xModel, float* xFrame) W_SetCurrentAmmoImpl =
{
    *xCurrentAmmo = WeaponIdToAmmoId(xWeapon, xWeaponFlags);
    *xModel = WeaponIdToModel(xWeapon, xWeaponFlags);
    *xFrame = 0;
};

void(float xHand, float xWeapon, float xWeaponFlags,
     float* xOutCurrentAmmo, string* xOutWeaponModel, float* xOutWeaponFrame)
     W_SetCurrentAmmoForImpl =
{
    player_run(xHand); // get out of any weapon firing states
    VRSetWeaponFrame(xHand, 0);

    W_SetCurrentAmmoImpl(xWeapon, xWeaponFlags,
        xOutCurrentAmmo, xOutWeaponModel, xOutWeaponFrame);
}

void(float xHand) W_SetCurrentAmmoFor =
{
    if(xHand == cVR_MainHand)
    {
        W_SetCurrentAmmoForImpl(xHand, self.weapon, self.weaponflags,
            &self.currentammo, &self.weaponmodel, &self.weaponframe);
    }
    else if(xHand == cVR_OffHand)
    {
        W_SetCurrentAmmoForImpl(xHand, self.weapon2, self.weaponflags2,
            &self.currentammo2, &self.weaponmodel2, &self.weaponframe2);
    }

    updateAmmoCounters();
};

void() W_SetCurrentAmmo =
{
    W_SetCurrentAmmoFor(cVR_MainHand);
    W_SetCurrentAmmoFor(cVR_OffHand);
};

float(float xHand) W_CheckNoAmmo =
{
    if(VRIsAmmolessWeapon(xHand))
    {
        return TRUE;
    }

    if(getCurrentAmmoOrClip(xHand) > 0)
    {
        return TRUE;
    }

    // drop the weapon down
    return FALSE;
};

/*
============
W_Attack

An attack impulse can be triggered now
============
*/

void(float xHand, float xWeapon, float xWeaponFlags, float xAttackFinished,
     float* xOutWeapon, float* xOutAttackFinished) W_AttackImpl =
{
    if(!W_CheckNoAmmo(xHand))
    {
        if(time > xAttackFinished)
        {
            sound(self, CHAN_AUTO, "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
        }

        return;
    }

    makevectors(VRGetWeaponFireRot(xHand)); // calculate forward angle for velocity
    self.show_hostile = time + 1; // wake monsters up

    // ZOID--
    RuneApplyBlackNoise(self); // make rune noise
    //--ZOID

    if(xWeapon == WID_AXE || xWeapon == WID_SWORD)
    {
        if(!VRHandsTracked(self) ||
           !self.ishuman)
        {
            sound(self, VRGetGunChannel(xHand), "weapons/ax1.wav", 1, ATTN_NORM);
            float r = random();
            if(r < 0.25)
                player_axe1(xHand);
            else if(r<0.5)
                player_axeb1(xHand);
            else if(r<0.75)
                player_axec1(xHand);
            else
                player_axed1(xHand);
            *xOutAttackFinished = time + RuneApplyHell(0.5, self);
        }
    }
    else if(xWeapon == WID_SHOTGUN)
    {
        player_shot1(xHand);
        W_FireShotgun(xHand);
        *xOutAttackFinished = time + RuneApplyHell(0.5, self);
    }
    else if(xWeapon == WID_SUPER_SHOTGUN)
    {
        player_shot1(xHand);
        W_FireSuperShotgun(xHand);
        *xOutAttackFinished = time + RuneApplyHell(0.7, self);
    }
    else if(xWeapon == WID_NAILGUN || xWeapon == WID_SUPER_NAILGUN)
    {
        if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
        {
            VRSetLavaGunFired(xHand, 1);
            player_lava_nail1(xHand);
        }
        else
        {
            player_nail1(xHand);
        }
    }
    else if(xWeapon == WID_GRENADE_LAUNCHER)
    {
        player_rocket1(xHand);

        if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
        {
            W_FireMultiGrenade(xHand);
        }
        else
        {
            W_FireGrenade(xHand);
        }

        *xOutAttackFinished = time + RuneApplyHell(0.6, self);
    }
    else if(xWeapon == WID_ROCKET_LAUNCHER)
    {
        player_rocket1(xHand);

        if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
        {
            W_FireMultiRocket(xHand);
        }
        else
        {
            W_FireRocket(xHand);
        }

        *xOutAttackFinished = time + RuneApplyHell(0.8, self);
    }
    else if(xWeapon == WID_LIGHTNING)
    {
        player_light1(xHand);

        if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
        {
            *xOutAttackFinished = time + RuneApplyHell(1.0, self);
        }
        else
        {
            *xOutAttackFinished = time + 0.1;
        }

        sound(self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
    }
    // HYPNOTIC
//MED
    else if(xWeapon == WID_LASER_CANNON)
    {
        player_laser1(xHand);
    }
//MED
   else if(xWeapon == WID_MJOLNIR)
   {
        if(!VRHandsTracked(self) ||
           !self.ishuman)
        {
            if(self.ammo_cells < 30)
            {
                player_hammer1(xHand);
            }
            else
            {
                player_mjolnir1(xHand);
            }

            *xOutAttackFinished = time + 0.8;
        }
   }
//MED
    else if(xWeapon == WID_PROXIMITY_GUN)
    {
        player_rocket1(xHand);
        W_FireProximityGrenade(xHand);
        *xOutAttackFinished = time + 0.6;
    }
    else if(xWeapon == WID_GRAPPLE)
    {
        if(!getHookOut(xHand))
            player_grapple1(xHand);
        else
            player_grapple3(xHand);

        *xOutAttackFinished = time + 0.1;
    }
};

void() W_Attack =
{
    W_AttackImpl(cVR_MainHand, self.weapon, self.weaponflags, self.attack_finished,
        &self.weapon, &self.attack_finished);
};

void() W_OffHandAttack =
{
    W_AttackImpl(cVR_OffHand, self.weapon2, self.weaponflags2, self.offhand_attack_finished,
        &self.weapon2, &self.offhand_attack_finished);
};

/*
============
W_ChangeWeapon

============
*/
//MED 10/18/96 added hipweapons
// TODO VR: (P1) rewrite to use new item system or remove, check if bots use this
void() W_ChangeWeapon =
{
    float am = 0;
    float wid = WID_FIST;

    if(self.impulse == 1)
    {
        wid = WID_AXE;
    }
    else if(self.impulse == 2)
    {
        wid = WID_SHOTGUN;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) < 1) { am = 1; }
    }
    else if(self.impulse == 3)
    {
        wid = WID_SUPER_SHOTGUN;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) < 2) { am = 1; }
    }
    else if(self.impulse == 4)
    {
        wid = WID_NAILGUN;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) < 1) { am = 1; }
    }
    else if(self.impulse == 5)
    {
        wid = WID_SUPER_NAILGUN;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_NAILS) < 2) { am = 1; }
    }
    else if(self.impulse == 6)
    {
        if(self.weapon == WID_GRENADE_LAUNCHER)
        {
           wid = WID_PROXIMITY_GUN;
        }
        else
        {
           wid = WID_GRENADE_LAUNCHER;
        }

        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) < 1) { am = 1; }
    }
    else if(self.impulse == 7)
    {
        wid = WID_ROCKET_LAUNCHER;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) < 1) { am = 1; }
    }
    else if(self.impulse == 8)
    {
        wid = WID_LIGHTNING;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_CELLS) < 1) { am = 1; }
    }
    else if(self.impulse == 225)
    {
        wid = WID_LASER_CANNON;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_CELLS) < 1) { am = 1; }
    }
    else if(self.impulse == 226)
    {
        wid = WID_MJOLNIR;
    }

    self.impulse = 0;

    if(!VR_ItemUtil_EntHasItem(self, WeaponIdToItemId(wid)))
    {
        if(wid == WID_GRENADE_LAUNCHER)
        {
            wid = WID_PROXIMITY_GUN;

            if(!VR_ItemUtil_EntHasItem(self, WeaponIdToItemId(wid)))
            {
                sprint(self, "no weapon.\n");
                return;
            }

            if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_ROCKETS) < 1)
            {
                am = 1;
            }
            else
            {
                am = 0;
            }
        }
        else
        {
            sprint(self, "no weapon.\n");
            return;
        }
    }

    if(am)
    {
        // don't have the ammo
        sprint(self, "not enough ammo.\n");
        return;
    }

    // set weapon, set ammo
    self.weapon = wid;
    W_SetCurrentAmmo();
};

/*
============
CheatCommand
============
*/
void() CheatCommand =
{
    if(deathmatch)
        return;

    self.ammo_rockets = 100;
    self.ammo_nails = 200;
    self.ammo_shells = 100;
    self.ammo_cells = 200;

    // ROGUE
    self.ammo_lava_nails = 200;
    self.ammo_multi_rockets = 100;
    self.ammo_plasma = 100;

    VR_WeaponUtil_EntAddWeapon(self, WID_GRAPPLE);
    VR_WeaponUtil_EntAddWeapon(self, WID_AXE);
    VR_WeaponUtil_EntAddWeapon(self, WID_SHOTGUN);
    VR_WeaponUtil_EntAddWeapon(self, WID_SUPER_SHOTGUN);
    VR_WeaponUtil_EntAddWeapon(self, WID_NAILGUN);
    VR_WeaponUtil_EntAddWeapon(self, WID_SUPER_NAILGUN);
    VR_WeaponUtil_EntAddWeapon(self, WID_GRENADE_LAUNCHER);
    VR_WeaponUtil_EntAddWeapon(self, WID_ROCKET_LAUNCHER);
    VR_WeaponUtil_EntAddWeapon(self, WID_LIGHTNING);
    VR_WeaponUtil_EntAddWeapon(self, WID_LASER_CANNON);
    VR_WeaponUtil_EntAddWeapon(self, WID_MJOLNIR);
    VR_WeaponUtil_EntAddWeapon(self, WID_PROXIMITY_GUN);

    VR_ItemUtil_EntAddItem(self, IID_KEY1);
    VR_ItemUtil_EntAddItem(self, IID_KEY2);

    self.impulse = 0;
    W_SetCurrentAmmo();
};

float(entity xPlayer, float xDir, float xWeapon,
      float xWeaponFlags, float* xOutWeaponFlags) GetCycledWeapon
{
    // Inclusive boundaries of cyclable weapon ids.
    float minWeapon = WID_GRAPPLE;
    float maxWeapon = WID_LASER_CANNON;

    // Just for safety.
    float maxIterations = maxWeapon + 1;

    // TODO VR: (P1) set this via cvar depending on weapon mode?
    float skipEmptyWeapons = FALSE;

    // Used to avoid infinite loops.
    float originalWeapon = xWeapon;

    // Find the next suitable weapon.
    float resultWeapon = originalWeapon;
    for(float iterations = 0; iterations < maxIterations; ++iterations)
    {

        xWeapon = xWeapon + xDir;

        if(xWeapon > maxWeapon)      { xWeapon = minWeapon; }
        else if(xWeapon < minWeapon) { xWeapon = maxWeapon; }

        // If we get to the original weapon after cycling, break.
        if(xWeapon == originalWeapon)
        {
            break;
        }

        // Do not select unavailable weapons.
        if(!VR_WeaponUtil_EntHasWeapon(xPlayer, xWeapon))
        {
            continue;
        }

        // Skip weapons without ammo if desired.
        if(skipEmptyWeapons &&
           !PlayerHasAmmoForWeapon(xPlayer, xWeapon, 0 /* weaponflags */))
        {
            continue;
        }

        // At this point, we have a suitable weapon.
        resultWeapon = xWeapon;
        break;
    }

    *xOutWeaponFlags = 0;
    return resultWeapon;
}

float() getWeaponCycleMode =
{
    if(!VRHandsTracked(self) || !self.ishuman)
    {
        return QVR_WEAPONCYCLEMODE_ALLOWED;
    }

    return cvar_hget(cvarh_vr_weapon_cycle_mode);
}

void(float xHand,
     float xWeapon, float* xOutWeapon,
     float xWeaponFlags, float* xOutWeaponFlags,
     float xDir) CycleWeaponCommandImpl =
{
    float newWeapon;

    self.impulse = 0;
    newWeapon = GetCycledWeapon(self, xDir, xWeapon, xWeaponFlags, xOutWeaponFlags);

    if(newWeapon != xWeapon)
    {
        *xOutWeapon = newWeapon;

        W_SetCurrentAmmoFor(xHand);
    }
};

/*
============
CycleWeaponCommand

Go to the next weapon with ammo
============
*/
void(float xHand,
     float xWeapon, float* xOutWeapon,
     float xWeaponFlags, float *xOutWeaponFlags) CycleWeaponCommand =
{
    CycleWeaponCommandImpl(xHand,
        xWeapon, xOutWeapon,
        xWeaponFlags, xOutWeaponFlags,
        1);
};

/*
============
CycleWeaponReverseCommand

Go to the prev weapon with ammo
============
*/
void(float xHand,
     float xWeapon, float* xOutWeapon,
     float xWeaponFlags, float *xOutWeaponFlags) CycleWeaponReverseCommand =
{
    CycleWeaponCommandImpl(xHand,
        xWeapon, xOutWeapon,
        xWeaponFlags, xOutWeaponFlags,
        -1);
};

/*
============
ServerflagsCommand

Just for development
============
*/
void() ServerflagsCommand =
{
    if(deathmatch)
        return;
    serverflags = serverflags * 2 + 1;
};

void() QuadCheat =
{
    if(deathmatch)
        return;

    self.super_time = 1;
    self.super_damage_finished = time + 30;
    VR_ItemUtil_EntAddItem(self, IID_QUAD);
};

void() InvisibilityCheat =
{
    if(deathmatch)
        return;

    self.invisible_time = 1;
    self.invisible_finished = time + 30;
    VR_ItemUtil_EntAddItem(self, IID_INVISIBILITY);
};

// HYPNOTIC
//MED
void() WetsuitCheat =
    {
   if(deathmatch || coop)
        return;
    VR_ItemUtil_EntAddItem(self, IID_WETSUIT);

    self.wetsuit_time = 1;
    self.wetsuit_finished = time + 30;
   bprint("wetsuit cheat\n");
    };

//MED
void() EmpathyShieldsCheat =
    {
   if(deathmatch || coop)
        return;
    VR_ItemUtil_EntAddItem(self, IID_EMPATHY_SHIELDS);
   self.empathy_time = 1;
   self.empathy_finished = time + 30;
   bprint("empathy shields cheat\n");
    };

void() DumpEntities =
   {
   entity   head;
   float i;

   i = 1;
   head = nextent(world);
   while(head!=world)
      {
      dprint(ftos(i));
      dprint(" ");
      dprint(head.classname);
      dprint("\n");
      head = nextent(head);
      i = i + 1;
      }
   };

void() DumpLiveEntities =
   {
   entity   head;
   float i;

   i = 1;
   head = nextent(world);
   while(head!=world)
      {
      if(head.health > 0)
         {
         dprint(ftos(i));
         dprint(" ");
         dprint(head.classname);
         dprint(" ");
         dprint(vtos(head.origin));
         dprint("\n");
         dprint("--------------------\n");
         }
      head = nextent(head);
      i = i + 1;
      }
   };

void() Genocide =
   {
   entity   head;

   if(deathmatch || coop)
        return;
   bprint("Genocide!\n");
   head = nextent(world);
   while(head!=world)
      {
      if((head.health > 0) && (head.flags & FL_MONSTER))
         {
         T_Damage(head,world,world,head.health+10);
         }
      head = nextent(head);
      }
   };

float dump_coord;
void () ToggleDump =
   {
   dump_coord = 1 - dump_coord;
   if(dump_coord == 1)
      bprint("Dumping Player Location\n");
   };
void () DumpCoordinates =
   {
   entity pl;
   pl = checkclient();
   if(pl)
      {
      bprint("Player: ");
      bprint(vtos(pl.origin));
      bprint("\n");
      }
   };


void() CheatSpawnAllWeaponDrops =
{
    CheatCommand();

    CreateWeaponDrop(self, WID_GRAPPLE, self.origin);
    CreateWeaponDrop(self, WID_AXE, self.origin);
    CreateWeaponDrop(self, WID_MJOLNIR, self.origin);
    CreateWeaponDrop(self, WID_SHOTGUN, self.origin);
    CreateWeaponDrop(self, WID_SUPER_SHOTGUN, self.origin);
    CreateWeaponDrop(self, WID_NAILGUN, self.origin);
    CreateWeaponDrop(self, WID_SUPER_NAILGUN, self.origin);
    CreateWeaponDrop(self, WID_GRENADE_LAUNCHER, self.origin);
    CreateWeaponDrop(self, WID_PROXIMITY_GUN, self.origin);
    CreateWeaponDrop(self, WID_ROCKET_LAUNCHER, self.origin);
    CreateWeaponDrop(self, WID_LIGHTNING, self.origin);
    CreateWeaponDrop(self, WID_LASER_CANNON, self.origin);
}

void(float wid) CheatSpawnWeaponDrop =
{
    CreateWeaponDrop(self, wid, self.origin);
}

/*
============
ImpulseCommands

============
*/
void(float xHand) ToggleSecondaryAmmo =
{
    float weapon = VRGetWeapon(xHand);

    if(!WeaponSupportsSecondaryAmmoFlag(weapon))
    {
        return;
    }

    VRTryUnloadWeapon(self, xHand, weapon,
        VRGetWeaponFlags(xHand), VRGetWeaponClip(xHand),
        VRGetEntWeaponClipPtr(self, xHand));

    VRGunHaptic(VRGetOtherHand(xHand), 0.3, 75, 1.0);
    sound(self, VRGetGunChannel(xHand), "buttons/switch02.wav", 0.5, ATTN_NORM);

    VRSetWeaponFlags(xHand, VRGetWeaponFlags(xHand) ^ QVR_WPNFLAG_USE_SECONDARY_AMMO);
    W_SetCurrentAmmoFor(xHand);
}

void() ImpulseCommands =
{
    // Development and tests (single player): impulse 150 + a weapon id puts that weapon, loaded, in
    // the main hand; 170 + id in the off hand.
    if(!deathmatch && self.impulse >= 150 && self.impulse < 190)
    {
        float cheatHand = cVR_MainHand;
        float cheatWid = self.impulse - 150;
        if(self.impulse >= 170)
        {
            cheatHand = cVR_OffHand;
            cheatWid = self.impulse - 170;
        }
        VR_WeaponUtil_EntAddWeapon(self, cheatWid);
        VRSetEntWeapon(self, cheatHand, cheatWid);
        VRSetEntWeaponFlags(self, cheatHand, 0);
        VRSetEntWeaponClip(self, cheatHand, WeaponIdToBaseClipSize(cheatWid));
        W_SetCurrentAmmoFor(cheatHand);
        self.impulse = 0;
        return;
    }

    if(self.impulse >= 1 && self.impulse <= 8)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            W_ChangeWeapon();
        }
    }

    if(self.impulse == 42)
    {
        ToggleSecondaryAmmo(cVR_OffHand);
    }

    if(self.impulse == 43)
    {
        ToggleSecondaryAmmo(cVR_MainHand);
    }

    if(self.impulse >= 225 && self.impulse <= 226)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            W_ChangeWeapon();
        }
    }

    if(self.impulse == 9)
    {
        CheatCommand();
    }

    if(self.impulse == 11)
    {
        ServerflagsCommand();
    }

    if(self.impulse == 14)
    {
        CheatSpawnAllWeaponDrops();
    }

    if(self.impulse == 17)
    {
        CheatSpawnWeaponDrop(WID_GRAPPLE);
    }




    if(self.impulse == 10 && time >= self.attack_finished)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            CycleWeaponCommand(cVR_MainHand,
                self.weapon, &self.weapon,
                self.weaponflags, &self.weaponflags);
        }
    }

    if(self.impulse == 12 && time >= self.offhand_attack_finished)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            CycleWeaponCommand(cVR_OffHand,
                self.weapon2, &self.weapon2,
                self.weaponflags2, &self.weaponflags2);
        }
    }

    if(self.impulse == 15 && time >= self.attack_finished)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            CycleWeaponReverseCommand(cVR_MainHand,
                self.weapon, &self.weapon,
                self.weaponflags, &self.weaponflags);
        }
    }

    if(self.impulse == 16 && time >= self.offhand_attack_finished)
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            CycleWeaponReverseCommand(cVR_OffHand,
                self.weapon2, &self.weapon2,
                self.weaponflags2, &self.weaponflags2);
        }
    }

    // TODO VR: (P2) rogue teamplay impulses
    /*
    // ROGUE
    //ZOID--
    //teamplay stuff
    else if(self.impulse == 20)
        TossBackpack();
    else if(self.impulse == 21)
        TossWeapon();
    else if(self.impulse == 22)
    {
        if(deathmatch && teamplay >= TEAM_CTF)
            W_ChangeWeapon();
    }
    else if(self.impulse == 23)
        TeamFlagStatusReport();
    //--ZOID
    */

//JIM
    if(self.impulse == 200)
        WetsuitCheat();
//MED
    if(self.impulse == 201)
        EmpathyShieldsCheat();
    if(self.impulse == 205)
        Genocide();
    if(self.impulse == 206)
        ToggleDump();
    if(self.impulse == 202)
        DumpEntities();
    if(self.impulse == 203)
        DumpLiveEntities();
    if(self.impulse == 254)
        InvisibilityCheat();
    if(self.impulse == 255)
        QuadCheat();
    if(self.impulse == 243 && !deathmatch && !coop) // QVR: a backpack of your ammo ahead (physics tests)
    {
        vector playerOrigin = self.origin;
        makevectors(self.v_angle);
        self.origin = playerOrigin + v_forward * 64 + '0 0 16';
        DropBackpack();
        self.origin = playerOrigin;
    }
    if(dump_coord == 1)
        DumpCoordinates();
    self.impulse = 0;
};

void(vector xOrigin, float xHand) PlayerLightningGunEffect =
{
    makeforward(VRGetWeaponFireRot(xHand));
    traceline(xOrigin, xOrigin + v_forward*600, TRUE, self);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, xHand); /* disambiguator */
    WriteVec3(MSG_BROADCAST, xOrigin);
    WriteVec3(MSG_BROADCAST, trace_endpos);
}

void(float xHand, float xButton) FrameDrawLightningTrail =
{
    // Lightning effect (per-frame)
    if(xButton &&
       VRGetWeapon(xHand) == WID_LIGHTNING &&
       (!(VRGetWeaponFlags(xHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO)) &&
       getCurrentAmmoOrClip(xHand) > 0)
    {
        PlayerLightningGunEffect(VRGetMuzzlePos(xHand), xHand);
    }
}

void(float xHand) W_WeaponFrameImpl =
{
    FrameDrawLightningTrail(xHand, VRGetFireButtonPressed(xHand));

    if(!deathmatch && time < readytime)
    {
        // Do not do anything if the player is not "ready" yet, after spawning.
        return;
    }

    if(time < VRGetAttackFinished(xHand))
    {
        return;
    }

    if(VRGetLavaGunFired(xHand))
    {
        if(!VRGetFireButtonPressed(xHand) &&
           (VRGetWeapon(xHand) == WID_NAILGUN || VRGetWeapon(xHand) == WID_SUPER_NAILGUN))
        {
            sound(self, CHAN_AUTO, "lavagun/snail.wav", 1, ATTN_NORM);
            particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 6);
            VRSetLavaGunFired(xHand, 0);
        }
    }

    // check for attack
    if(VRGetFireButtonPressed(xHand) && !VR_Forcegrab_IsHandBusy(self, xHand))
    {
        // TODO VR: (P2) what about when we don't have any weapon equipped? Or
        // when we have axe equipped?
        SuperDamageSound();

        if(xHand == cVR_OffHand)
        {
            W_OffHandAttack();
        }
        else
        {
            W_Attack();
        }
    }
}

// ---

float(float xHolster) getHolsterWeapon =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return self.holsterweapon0; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return self.holsterweapon1; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { return self.holsterweapon2; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { return self.holsterweapon3; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { return self.holsterweapon4; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { return self.holsterweapon5; }

    bprint("`getHolsterWeapon`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    // Bogus, should never happen.
    return self.holsterweapon0;
}

void(float xHolster, float v) setHolsterWeapon =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweapon0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweapon1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweapon2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweapon3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweapon4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweapon5 = v; return; }

    bprint("`setHolsterWeapon`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

// ---

float(float xHolster) getHolsterWeaponClip =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return self.holsterweaponclip0; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return self.holsterweaponclip1; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { return self.holsterweaponclip2; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { return self.holsterweaponclip3; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { return self.holsterweaponclip4; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { return self.holsterweaponclip5; }

    bprint("`getHolsterWeaponClip`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    // Bogus, should never happen.
    return 0;
}

void(float xHolster, float v) setHolsterWeaponClip =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweaponclip0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweaponclip1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweaponclip2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweaponclip3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweaponclip4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweaponclip5 = v; return; }

    bprint("`setHolsterWeaponClip`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

// ---

void(float xHolster, string v) setHolsterWeaponModel =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweaponmodel0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweaponmodel1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweaponmodel2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweaponmodel3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweaponmodel4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweaponmodel5 = v; return; }

    bprint("`setHolsterWeaponModel`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

// ---

float(float xHolster) getHolsterWeaponFlags =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweaponflags0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweaponflags1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweaponflags2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweaponflags3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweaponflags4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweaponflags5;

    bprint("`getHolsterWeaponFlags`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    // Bogus, should never happen.
    return self.holsterweaponflags0;
}

void(float xHolster, float v) setHolsterWeaponFlags =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweaponflags0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweaponflags1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweaponflags2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweaponflags3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweaponflags4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweaponflags5 = v; return; }

    bprint("`setHolsterWeaponFlags`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

float(float xHotspot) isHolsterHotspot =
{
    return xHotspot == QVR_HS_LEFT_SHOULDER_HOLSTER ||
           xHotspot == QVR_HS_RIGHT_SHOULDER_HOLSTER ||
           xHotspot == QVR_HS_LEFT_HIP_HOLSTER ||
           xHotspot == QVR_HS_RIGHT_HIP_HOLSTER ||
           xHotspot == QVR_HS_LEFT_UPPER_HOLSTER ||
           xHotspot == QVR_HS_RIGHT_UPPER_HOLSTER;
}

float(float xHotspot) isReloadHolsterHotspot =
{
    if(cvar_hget(cvarh_vr_reload_mode) == QVR_RELOADMODE_ALLHOLSTERS)
    {
        return TRUE;
    }

    ASSERT(cvar_hget(cvarh_vr_reload_mode) == QVR_RELOADMODE_HIPHOLSTERS);

    return xHotspot == QVR_HS_LEFT_HIP_HOLSTER ||
           xHotspot == QVR_HS_RIGHT_HIP_HOLSTER;
}

void(float xOffset, float xAngle, float* xOutAngle) wpnthrow_impl_stabilize =
{
    float angle;
    angle = xAngle - xOffset;

    // Bring angle in range [-360, 0]
    while(angle > 0)
    {
        *xOutAngle = (xOffset + angle) - 360;
        angle = angle - 360;
    }

    while(angle < -360)
    {
        *xOutAngle = (xOffset + angle) + 360;
        angle = angle + 360;
    }

    if(angle >= -360 && angle <= -270)
    {
        // All these offset calculations are needed because I can't read back
        // from `xOutAngle`...
        *xOutAngle = (xOffset + angle) + self.throwstabilize;
        if((xAngle - xOffset) + self.throwstabilize > -270)
        {
            *xOutAngle = xOffset + -270;
        }
    }
    else if(angle > -270 && angle <= -180)
    {
        *xOutAngle = (xOffset + angle) - self.throwstabilize;
        if((xAngle - xOffset) - self.throwstabilize < -270)
        {
            *xOutAngle = xOffset + -270;
        }
    }
    else if(angle > -180 && angle <= -90)
    {
        *xOutAngle = (xOffset + angle) + self.throwstabilize;
        if((xAngle - xOffset) + self.throwstabilize > -90)
        {
            *xOutAngle = xOffset + -90;
        }
    }
    else if(angle > -90 && angle <= 0)
    {
        *xOutAngle = (xOffset + angle) - self.throwstabilize;
        if((xAngle - xOffset) - self.throwstabilize < -90)
        {
            *xOutAngle = xOffset + -90;
        }
    }
    else
    {
        // TODO VR: (P1) still triggers sometimes
        bprint("`wpnthrow_impl_stabilize`: Uncovered angle case: '");
        bprint(ftos(angle));
        bprint("'\n");
    }

    if(self.throwstabilize > 0)
    {
        self.throwstabilize = self.throwstabilize - 0.3;

        if(self.throwstabilize < 0)
        {
            self.throwstabilize = 0;
        }
    }
}

void() wpnthrow_stabilize =
{
    if(self.throwstabilizedim == -1)
    {
        // Intentionally empty.
    }
    else if(self.throwstabilizedim == 0)
    {
        wpnthrow_impl_stabilize(90, self.angles_x, &self.angles_x);
    }
    else if(self.throwstabilizedim == 1)
    {
        wpnthrow_impl_stabilize(0, self.angles_y, &self.angles_y);
    }
    else if(self.throwstabilizedim == 2)
    {
        wpnthrow_impl_stabilize(0, self.angles_z, &self.angles_z);
    }
}

void() forcegrabbable_think_impl =
{
    // In water (not the "just spawned" waterlevel 1 of an entity never checked since).
    if(self.waterlevel >= 1 && self.watertype <= CONTENT_WATER)
    {
        if(self.ammocounter < 2)
        {
            self.ammocounter += 1;
        }
        else if (self.ammocounter > 0)
        {
            self.ammocounter -= 1;
        }

        if(self.ammocounter > 1)
        {
            if(fabs(self.velocity_x) > 10)
            {
                self.velocity_x *= 0.85;
            }
            else
            {
                self.velocity_x = 0;
            }

            if(fabs(self.velocity_y) > 10)
            {
                self.velocity_y *= 0.85;
            }
            else
            {
                self.velocity_y = 0;
            }

            self.velocity_z = 45;
            self.flags = self.flags | FL_INWATER;
            self.throwhit = QVR_THROWHIT_HIT;
            self.solid = SOLID_NOT_BUT_TOUCHABLE;
            self.avelocity = '0 0 0';
            self.vr_spin = '0 0 0';
        }
    }
    else if((time - self.lastwatertime) > 0.15)
    {
        self.flags = self.flags & ~FL_INWATER;
        self.ammocounter = 0;
        self.waterlevel = 0;
    }
    else if (self.ammocounter > 0)
    {
        self.ammocounter -= 1;
        self.waterlevel = 0;
    }

    if(cvar_hget(cvarh_vr_weapondrop_particles) && (self.flags & (FL_ONGROUND | FL_INWATER)))
    {
        if(random() < 0.2 && self.model != string_null)
        {
            particle2(realorigin(self), '0 0 0', QVR_PARTICLE_PRESET_GUNPICKUP, 1);
        }
    }

    if(self.throwhit == QVR_THROWHIT_HIT && !self.vr_rigid) // rigid ones settle in the engine
    {
        wpnthrow_stabilize();
    }
}

void() forcegrabbable_think =
{
    forcegrabbable_think_impl();
    self.nextthink = time + 0.02;
}

void() wpnthrow_think =
{
    if(deathmatch && time > GetThrownWeaponTimeout(self))
    {
        SUB_Remove();
        return;
    }

    forcegrabbable_think_impl();
    self.nextthink = time + 0.02;
}

void() forcegrabbable_touch =
{
    if(self.owner != world && other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    // A thrown weapon's true-scale gravity lasted for its flight only; rigid ones keep it (the
    // engine bounces them: a sudden Quake gravity after the first touch looked wrong).
    if(!self.vr_rigid)
    {
        self.gravity = 0;
    }

    // if(pointcontents(self.origin) == CONTENT_SKY)
    //     return;

    if(self.throwhit == QVR_THROWHIT_NEVER_HIT)
    {
        float dmgSpeed = self.throwdmgspeed;
        if(!dmgSpeed) dmgSpeed = 700;

        float finalDmg = ((vlen(self.velocity) / dmgSpeed) * GetThrownWeaponBaseDamage(self)) *
                   cvar_hget(cvarh_vr_weapon_throw_damage_mult);

        // hit something that bleeds
        if(GetThrownWeaponDealDamage(self) && other.takedamage)
        {
            sound(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);

            // Where it struck, and going which way (not the trace globals, stale in a touch).
            float posDmgType = PositionalDamage(other, self.origin, normalize(self.velocity));
            float adjDmg = finalDmg * PositionalDamageFactor(posDmgType);

            spawn_touchblood(adjDmg);
            T_Damage(other, self, self.owner, adjDmg);
        }
        // hit wall
        else
        {
            if(GetThrownWeaponMetalNoiseOnTouch(self))
            {
                sound(self, CHAN_AUTO, "player/axhit2.wav", 0.65, ATTN_NORM);
            }

            particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 24);
        }

        if(GetThrownWeaponDisappearOnHit(self) == TRUE)
        {
            remove(self);
        }

        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        self.throwhit = QVR_THROWHIT_HIT;
    }

    // ghetto collision detection/resolution (rigid ones bounce in the engine)
    if(!self.vr_rigid && !(self.flags & FL_ONGROUND) && vlen(self.velocity) > 2.0)
    {
        vector dir = normalize(self.velocity);
        dir_z = 0;

        traceline(self.origin - dir * 2, self.origin + dir * 8, FALSE, self);
        if(trace_fraction != 1.0)
        {
            setorigin(self, trace_endpos - dir * 8);
        }

        dir = normalize(self.velocity);
        dir_x = 0;
        dir_y = 0;

        traceline(self.origin - dir * 2, self.origin + dir * 2, FALSE, self);
        if(trace_fraction != 1.0)
        {
            setorigin(self, trace_endpos - dir * 2);
            self.velocity = '0 0 0';
            self.avelocity = '0 0 0';
        }
    }
}

void() wpnthrow_touch =
{
    forcegrabbable_touch();
}

void(float xHand, entity ent, entity entPlayer,
     float* xOutEntWeapon, float* xOutEntWeaponFlags, float* xOutEntWeaponClip)
     wpnthrow_handtouch_impl =
{
    sound(ent, CHAN_AUTO, "weapons/pkup.wav", 1, ATTN_NORM);
    VRGunHaptic(xHand, 0.3, 75, 1.0);

    *xOutEntWeapon = ent.weapon;
    *xOutEntWeaponFlags = ent.weaponflags;
    *xOutEntWeaponClip = ent.weaponclip;
    remove(ent);

    self = entPlayer;
    W_SetCurrentAmmo();
}

float(entity ent, float xHand) wpnthrow_handtouch_ent_hand =
{
    entity entPlayer = ent.handtouch_ent;

    if(!VRIsHandEmpty(entPlayer, xHand) || VR_Carry_IsHolding(entPlayer, xHand))
    {
        return FALSE;
    }

    float pStartedGrabbing = VR_HandGrabUtil_StartedHandGrabbing(entPlayer, xHand);
    float* pWeaponPtr = VRGetEntWeaponPtr(entPlayer, xHand);
    float* pWeaponFlagsPtr = VRGetEntWeaponFlagsPtr(entPlayer, xHand);
    float* pWeaponClipPtr = VRGetEntWeaponClipPtr(entPlayer, xHand);

    float performWeaponSwitch = FALSE;

    // Must catch it: the grab started as it touches, or a force grab caught in time.
    if(VRGetHandtouchHand(ent) == xHand && (pStartedGrabbing || forcegrabCatching))
    {
        performWeaponSwitch = TRUE;
    }

    if(performWeaponSwitch)
    {
        wpnthrow_handtouch_impl(xHand, ent, entPlayer,
                                pWeaponPtr, pWeaponFlagsPtr, pWeaponClipPtr);
        return TRUE;
    }

    return FALSE;
}

void(entity entWpn) wpnthrow_handtouch_ent =
{
    if(VRIsHandtouchHandFake(entWpn))
    {
        // Happens with VR Body Interactions enabled.

        // TODO VR: (P0) test interactions between force grabs and VR body
        // interactions!

        // TODO VR: (P2) cvar to choose what happens?
        // return;
    }

    if(wpnthrow_handtouch_ent_hand(entWpn, cVR_OffHand)) { return; }
    if(wpnthrow_handtouch_ent_hand(entWpn, cVR_MainHand)) { return; }
}

void() wpnthrow_handtouch =
{
    wpnthrow_handtouch_ent(self);
}

float(float xHolster) VRHolsterToIndex =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return 0; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return 1; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { return 2; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { return 3; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { return 4; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { return 5; }

    bprint("`VRHolsterToIndex`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    return 0;
}

string(float xHolster) VRGetHolsterName =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { return "Left shoulder"; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { return "Right shoulder"; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { return "Left thigh"; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { return "Right thigh"; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { return "Left waist"; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { return "Right waist"; }

    bprint("`VRGetHolsterName`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    return 0;
}

float(entity xEntPlayer, float xHolster) VRIsHolsterHovered =
{
    return xEntPlayer.holsterhover[VRHolsterToIndex(xHolster)];
}

void(entity xEntPlayer, float xHolster, float xValue) VRSetHolsterHovered =
{
    xEntPlayer.holsterhover[VRHolsterToIndex(xHolster)] = xValue;
}

float(entity xEntPlayer, float xHolster) VRIsHolsterPrevHovered =
{
    return xEntPlayer.holsterprevhover[VRHolsterToIndex(xHolster)];
}

void(entity xEntPlayer, float xHolster, float xValue) VRSetHolsterPrevHovered =
{
    xEntPlayer.holsterprevhover[VRHolsterToIndex(xHolster)] = xValue;
}

void(entity xEntPlayer) UpdateHolsterHover =
{
    float i;

    for(i = QVR_HS_LEFT_SHOULDER_HOLSTER; i <= QVR_HS_RIGHT_UPPER_HOLSTER; ++i)
    {
        if(!isHolsterHotspot(i))
        {
            continue;
        }

        VRSetHolsterPrevHovered(xEntPlayer, i, VRIsHolsterHovered(xEntPlayer, i));
        VRSetHolsterHovered(xEntPlayer, i, FALSE);
    }
}

void(entity xEntPlayer, float xHand, float xHolster) VRHolsterHaptic =
{
    if(!xEntPlayer.ishuman)
    {
        return;
    }

    float holsterHapticsMode;
    holsterHapticsMode = cvar_hget(cvarh_vr_holster_haptics);

    if(holsterHapticsMode == 1 /* continuous */)
    {
        VRGunHaptic(xHand, 0.1, 75, 0.02);
    }
    else if(!VRIsHolsterPrevHovered(xEntPlayer, xHolster) &&
            VRIsHolsterHovered(xEntPlayer, xHolster) &&
            holsterHapticsMode == 2 /* once */)
    {
        VRGunHaptic(xHand, 0.2, 75, 0.2);
    }
}

void(entity xEntPlayer, float xHand, float xHolster) VRHolsterHapticBuzz =
{
    if(!xEntPlayer.ishuman)
    {
        return;
    }

    VRGunHaptic(xHand, 0.1, 75, 0.5);
}

void(entity ent, float xValue) SetThrownWeaponMetalNoiseOnTouch =
{
    ent.fixangle = xValue;
}

float(entity ent) GetThrownWeaponMetalNoiseOnTouch =
{
    return ent.fixangle;
}

void(entity ent, float xValue) SetThrownWeaponDisappearOnHit =
{
    ent.lip = xValue;
}

float(entity ent) GetThrownWeaponDisappearOnHit =
{
    return ent.lip;
}

void(entity ent, float xValue) SetThrownWeaponDealDamage =
{
    ent.health = xValue;
}

float(entity ent) GetThrownWeaponDealDamage =
{
    return ent.health;
}

void(entity ent, float xValue) SetThrownWeaponBaseDamage =
{
    ent.frags = xValue;
}

float(entity ent) GetThrownWeaponBaseDamage =
{
    return ent.frags;
}

// (Not .health: that is the "deal damage" flag.)
void(entity ent, float xValue) SetThrownWeaponTimeout =
{
    ent.thrownweapon_timeout = xValue;
}

float(entity ent) GetThrownWeaponTimeout =
{
    return ent.thrownweapon_timeout;
}

void MakeGrabbable(entity newmis)
{
    newmis.movetype = MOVETYPE_TOSS;
    newmis.solid = SOLID_BBOX;

    newmis.throwhit = QVR_THROWHIT_NEVER_HIT;
    newmis.throwstabilizedim = -1;

    newmis.flags |= FL_EASYHANDTOUCH;

    SetThrownWeaponMetalNoiseOnTouch(newmis, FALSE);
}

void MakeThrown(entity newmis,
    entity thrower, float xWeapon, float xWeaponFlags, float xWeaponClip,
    vector xOrigin, vector xRotation,
    vector xThrowVel, vector xAVel,
    float xDisappearOnHit,
    float xDealDamage,
    float xBaseDamage)
{
    MakeGrabbable(newmis);
    newmis.solid = SOLID_NOT_BUT_TOUCHABLE;

    SetThrownWeaponDisappearOnHit(newmis, xDisappearOnHit);
    SetThrownWeaponDealDamage(newmis, xDealDamage);
    SetThrownWeaponBaseDamage(newmis, xBaseDamage);
    SetThrownWeaponMetalNoiseOnTouch(newmis, TRUE);

    newmis.owner = thrower;

    newmis.throwstabilize = 8;
    newmis.throwstabilizedim = WeaponIdToThrowStabilizeDim(xWeapon);

    newmis.angles = xRotation;
    newmis.angles_x = newmis.angles_x * -1;

    newmis.touch = wpnthrow_touch;
    newmis.handtouch = wpnthrow_handtouch;

    newmis.classname = "thrown_weapon";
    newmis.netname = WeaponIdToWeaponName(xWeapon);

    newmis.avelocity = xAVel;

    newmis.waterlevel = 0;
    newmis.ammocounter = 0; // water frame counter
    newmis.think = wpnthrow_think;
    newmis.nextthink = time + 0.02;

    SetThrownWeaponTimeout(newmis, time + 10); // removal time (timeout) in deathmatch

    newmis.weapon = xWeapon;
    newmis.weaponflags = xWeaponFlags;
    newmis.weaponclip = xWeaponClip;

    // TODO VR: (P1) fix laser cannon bounds
    float tmp = WeaponIdToThrowBounds(xWeapon);

    vector tmp_bounds;
    tmp_bounds_x = tmp;
    tmp_bounds_y = tmp;
    tmp_bounds_z = tmp;

    setmodel(newmis, WeaponIdToModel(xWeapon, xWeaponFlags));
    setsize(newmis, -tmp_bounds, tmp_bounds);
    setorigin(newmis, xOrigin);

    newmis.velocity = xThrowVel + thrower.velocity;
}

// The old throw parameters, still used for drops and dispensers: `xThrowVel` scaled by 120
// units/s, and a spin derived from `xAVel` around the throw direction.
vector(float xWeapon, vector xThrowVel) LegacyThrowVelocity =
{
    return xThrowVel * 120 * WeaponIdToThrowMult(xWeapon);
}

vector(vector xThrowVel, vector xAVel) LegacyThrowSpin =
{
    makevectors(xThrowVel);
    return xAVel_x * v_forward * 30
         + xAVel_y * v_right * -30
         + xAVel_z * v_up * -30;
}

float() VRMetersToUnits =
{
    return cvar_hget(cvarh_vr_world_scale) / (1.5 * 0.0254);
}

entity(entity thrower, float xWeapon, float xWeaponFlags, float xWeaponClip,
    vector xOrigin, vector xRotation,
    vector xThrowVel, vector xAVel,
    float xDisappearOnHit,
    float xDealDamage,
    float xBaseDamage) CreateThrownWeapon =
{
    entity newmis = spawn();

    MakeThrown(newmis,
        thrower, xWeapon, xWeaponFlags, xWeaponClip,
        xOrigin, xRotation,
        xThrowVel, xAVel,
        xDisappearOnHit,
        xDealDamage,
        xBaseDamage);

    return newmis;
}

vector randomVec(float range)
{
    vector res;

    res_x = crandom() * range;
    res_y = crandom() * range;
    res_z = crandom() * range;

    return res;
}

void(entity thrower, float xWeapon, vector xOrigin) CreateWeaponDrop =
{
    vector throwVel;

    throwVel = randomVec(0.9);
    throwVel_z = 1.1 + random() * 1.1;

    CreateThrownWeapon(thrower, xWeapon, 0 /* weaponflags */,
        WeaponIdToBaseClipSize(xWeapon) /* weaponclip */,
        xOrigin, randomVec(180),
        LegacyThrowVelocity(xWeapon, throwVel), LegacyThrowSpin(throwVel, randomVec(20)),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWeapon)
    );
}

// TODO VR: (P2) improve?
void(entity thrower, float xWeapon, vector xOrigin) CreateAmmoBoxWeaponDrop =
{
    vector throwVel;

    throwVel = randomVec(0.1);
    throwVel_z = 0.2 + random() * 0.1;

    CreateThrownWeapon(thrower, xWeapon, 0 /* weaponflags */,
        WeaponIdToBaseClipSize(xWeapon) /* weaponclip */,
        xOrigin, randomVec(180),
        LegacyThrowVelocity(xWeapon, throwVel), LegacyThrowSpin(throwVel, randomVec(20)),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWeapon)
    );
}

float() getWeaponMode =
{
    return cvar_hget(cvarh_vr_holster_mode);
}

// Speeds up to vr_throw_gain_lo m/s leave the hand as they are (drops, passes); above, smoothly
// more, up to vr_throw_gain_max times at vr_throw_gain_hi (docs/vr-port/THROWING.md 3.2).
float(float xSpeed) VRThrowGain =
{
    float lo = cvar_hget(cvarh_vr_throw_gain_lo);
    float hi = cvar_hget(cvarh_vr_throw_gain_hi);
    float gainMax = cvar_hget(cvarh_vr_throw_gain_max);

    if(hi <= lo)
    {
        if(xSpeed >= hi)
        {
            return gainMax;
        }
        return 1;
    }

    float t = min(1, max(0, (xSpeed - lo) / (hi - lo)));
    t = t * t * (3 - 2 * t); // smoothstep
    return 1 + (gainMax - 1) * t;
}

// Angle between two unit vectors, in degrees.
float(vector xA, vector xB) VRAngleBetween =
{
    vector c;
    c_x = xA_y * xB_z - xA_z * xB_y;
    c_y = xA_z * xB_x - xA_x * xB_z;
    c_z = xA_x * xB_y - xA_y * xB_x;
    return atan2(vlen(c), xA * xB) * (180 / 3.14159265);
}

// Optional aim assist (vr_throw_assist, docs/vr-port/THROWING.md 3.3): bends a throw towards the
// best target near its direction (a living monster or breakable in sight, within
// vr_throw_assist_cone degrees and vr_throw_assist_range units), onto the low ballistic arc that
// reaches it at the throw's speed (or up to vr_throw_assist_speed faster), by
// vr_throw_assist_strength (fully within vr_throw_assist_full degrees, fading to the cone's edge).
vector(entity xThrower, vector xOrigin, vector xVel, float xGravity) VRThrowAssist =
{
    if(!cvar_hget(cvarh_vr_throw_assist))
    {
        return xVel;
    }

    float speed = vlen(xVel);
    if(speed < 1 || xGravity <= 0)
    {
        return xVel;
    }

    vector dir = normalize(xVel);
    float cone = cvar_hget(cvarh_vr_throw_assist_cone);
    float full = cvar_hget(cvarh_vr_throw_assist_full);
    float range = cvar_hget(cvarh_vr_throw_assist_range);

    vector flatVel = xVel;
    flatVel_z = 0;
    float flatSpeed = vlen(flatVel);
    if(flatSpeed < 1)
    {
        flatSpeed = 1;
    }

    entity best = world;
    float bestScore = 1000000;
    float bestAngle = 0;
    vector bestPos = '0 0 0';

    entity e = findradius(xOrigin, range);
    while(e)
    {
        // A living monster or breakable.
        if(e != xThrower && e.takedamage && e.health > 0 && ((e.flags & FL_MONSTER) || e.solid == SOLID_BSP) &&
           e.classname != "player")
        {
            vector c = (e.absmin + e.absmax) * 0.5;

            // Where it will be when the throw gets there.
            vector flat = c - xOrigin;
            flat_z = 0;
            c = c + e.velocity * (vlen(flat) / flatSpeed);

            float a = VRAngleBetween(dir, normalize(c - xOrigin));
            if(a <= cone)
            {
                traceline(xOrigin, c, TRUE, xThrower);
                if(trace_fraction == 1 || trace_ent == e)
                {
                    float score = a / cone + 0.2 * vlen(c - xOrigin) / range;
                    if(score < bestScore)
                    {
                        bestScore = score;
                        best = e;
                        bestAngle = a;
                        bestPos = c;
                    }
                }
            }
        }

        e = e.chain;
    }

    if(best == world)
    {
        return xVel;
    }

    // The low arc to bestPos at speed s: tan(theta) = (s^2 - sqrt(s^4 - g (g x^2 + 2 y s^2))) / (g x).
    vector d = bestPos - xOrigin;
    vector flatD = d;
    flatD_z = 0;
    float x = vlen(flatD);
    float y = d_z;
    float g = xGravity;

    float s = speed;
    float disc = s * s * s * s - g * (g * x * x + 2 * y * s * s);
    if(disc < 0)
    {
        s = speed * (1 + cvar_hget(cvarh_vr_throw_assist_speed));
        disc = s * s * s * s - g * (g * x * x + 2 * y * s * s);
    }

    vector aim;
    if(disc >= 0 && x > 1)
    {
        float theta = atan2(s * s - sqrt(disc), g * x);
        aim = normalize(flatD) * cos(theta) + '0 0 1' * sin(theta);
    }
    else
    {
        // Out of reach: only turn towards it, keeping the throw's elevation.
        s = speed;
        aim = normalize(flatD) * (flatSpeed / speed);
        aim_z = dir_z;
        aim = normalize(aim);
    }

    float k = cvar_hget(cvarh_vr_throw_assist_strength);
    if(bestAngle > full && cone > full)
    {
        k = k * (cone - bestAngle) / (cone - full);
    }

    if(cvar("developer"))
    {
        particle2(bestPos, '0 0 0', QVR_PARTICLE_PRESET_GUNPICKUP, 8);
    }

    return normalize(dir * (1 - k) + aim * k) * (speed + (s - speed) * k);
}

// The hand's spin (radians per second, world axes) for a thrown weapon, at most
// vr_throw_spin_max: wrist-flick noise must not make helicopters.
vector(vector xAVel) VRThrowSpinCapped =
{
    float rate = vlen(xAVel);
    float spinMax = cvar_hget(cvarh_vr_throw_spin_max);
    if(rate > spinMax && rate > 0)
    {
        return xAVel * (spinMax / rate);
    }
    return xAVel;
}

float() getWeaponThrowMode =
{
    return cvar_hget(cvarh_vr_weapon_throw_mode);
}

void(entity ent, float xHand, float xWeapon, float xWeaponFlags, float xWeaponClip) DropWeaponInHand =
{
    if(VRIsHandEmpty(ent, xHand))
    {
        return;
    }

    float twoHThrowVelMult;
    vector throwVel;
    if(time - self.last_2h_time < 0.3)
    {
        twoHThrowVelMult = cvar_hget(cvarh_vr_2h_throw_velocity_mult);
        throwVel = VRGetHandThrowVel(xHand) + VRGetHandThrowVel(VRGetOtherHand(xHand));
        throwVel /= 2.0;
        throwVel *= twoHThrowVelMult;
    }
    else
    {
        twoHThrowVelMult = 1.0;
        throwVel = VRGetHandThrowVel(xHand);
    }


    // The hand's velocity (m/s) at true scale: the weapon leaves the hand as fast as the hand
    // moved, a little more for real throws (VRThrowGain), a little less for heavy weapons, and
    // falls at vr_throw_gravity rather than Quake's 2.5g.
    float m2u = VRMetersToUnits();
    float weight = 1 + (WeaponIdToThrowMult(xWeapon) - 1) * cvar_hget(cvarh_vr_throw_weight_influence);
    vector throwVelUnits = throwVel * m2u * VRThrowGain(vlen(throwVel)) * weight *
                           cvar_hget(cvarh_vr_weapon_throw_velocity_mult);

    float throwGravity = cvar_hget(cvarh_sv_gravity);
    if(cvar_hget(cvarh_vr_throw_gravity) > 0)
    {
        throwGravity = cvar_hget(cvarh_vr_throw_gravity) * m2u;
    }

    // It left the hand a moment ago (the release lags the throw, and the move carrying it is
    // later still): start where it would be by now, not where the hand followed through to.
    vector handNow = VRGetHandPos(xHand);
    vector origin = handNow;
    float age = VRGetHandThrowAge(xHand);
    if(VRGetHandThrowPos(xHand) != '0 0 0' && age >= 0 && age < 0.15)
    {
        origin = VRGetHandThrowPos(xHand) + throwVelUnits * age;
        origin_z = origin_z - 0.5 * throwGravity * age * age;
        throwVelUnits_z = throwVelUnits_z - throwGravity * age;

        traceline(handNow, origin, TRUE, ent); // never inside a wall
        origin = trace_endpos;
        if(trace_fraction < 1)
        {
            origin = origin - normalize(origin - handNow) * 2;
        }
    }

    throwVelUnits = VRThrowAssist(ent, origin, throwVelUnits, throwGravity);

    entity thrown = world;
    if(getWeaponThrowMode() == QVR_WEAPONTHROWMODE_IMMERSIVE)
    {
        thrown = CreateThrownWeapon(ent, xWeapon, xWeaponFlags, xWeaponClip,
            origin, VRGetHandRot(xHand),
            throwVelUnits, '0 0 0',
            FALSE /* disappear on hit */,
            TRUE  /* deal damage */,
            WeaponIdToThrowDamage(xWeapon));
    }
    else if(getWeaponThrowMode() == QVR_WEAPONTHROWMODE_DISAPPEARONHIT)
    {
        thrown = CreateThrownWeapon(ent, xWeapon, xWeaponFlags, xWeaponClip,
            origin, VRGetHandRot(xHand),
            throwVelUnits, '0 0 0',
            TRUE /* disappear on hit */,
            TRUE /* deal damage */,
            WeaponIdToThrowDamage(xWeapon));
    }

    if(thrown != world)
    {
        // The same damage for the same hand speed as the old x120 scaling.
        thrown.throwdmgspeed = 700 * m2u / 120;

        if(cvar_hget(cvarh_sv_gravity) > 0)
        {
            thrown.gravity = throwGravity / cvar_hget(cvarh_sv_gravity);
        }

        // Flies, spins, bounces and settles as a rigid body (engine: vr_rigid.cpp).
        thrown.vr_rigid = 1;
        thrown.vr_spin = VRThrowSpinCapped(VRGetHandAVel(xHand));

        dprint("thrown: velocity "); dprint(vtos(thrown.velocity));
        dprint(" gravity "); dprint(ftos(thrown.gravity));
        dprint(" spin "); dprint(vtos(thrown.vr_spin));
        dprint(" age "); dprint(ftos(age)); dprint("\n");
    }
    // (QVR_WEAPONTHROWMODE_DISCARD: nothing is thrown, the weapon is gone.)

    sound(ent, CHAN_BODY, "knight/sword2.wav", 0.7, ATTN_NORM);
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmoFor(xHand);
}

void(entity entPlayer, float xHotspot, float xWeapon, float xWeaponFlags, float xWeaponClip)
VRPutWeaponInHolster =
{
    sound(entPlayer, CHAN_BODY, "weapons/holster1.wav", 1, ATTN_NORM);

    setHolsterWeapon(xHotspot, xWeapon);
    setHolsterWeaponModel(xHotspot, WeaponIdToModel(xWeapon, xWeaponFlags));
    setHolsterWeaponFlags(xHotspot, xWeaponFlags);
    setHolsterWeaponClip(xHotspot, xWeaponClip);
}

void(entity entPlayer, float xHand,
    float xWeapon, float xWeaponFlags, float xWeaponClip, float* xOutWeaponClip)
VRReloadWeapon =
{
    VRGunHaptic(xHand, 0.25, 70, 1.0);
    sound(entPlayer, CHAN_BODY, "reload1.wav", 1, ATTN_NORM);

    float ammoId = WeaponIdToAmmoId(xWeapon, xWeaponFlags);
    float missingAmmo = WeaponIdToBaseClipSize(xWeapon) - xWeaponClip;
    float remainingAmmo = VR_AmmoUtil_GetAmmoCountByAmmoId(entPlayer, ammoId);
    float clampedMissingAmmo = clamp(missingAmmo, 0, remainingAmmo);

    delAmmoAndUpdateCounter(xHand, ammoId, clampedMissingAmmo);
    *xOutWeaponClip = xWeaponClip + clampedMissingAmmo;
}

void(entity entPlayer, float xHand,
    float xWeapon, float xWeaponFlags, float xWeaponClip, float* xOutWeaponClip)
VRUnloadWeapon =
{
    VRGunHaptic(xHand, 0.25, 70, 1.0);
    sound(entPlayer, CHAN_BODY, "reload1.wav", 1, ATTN_NORM);

    float ammoId = WeaponIdToAmmoId(xWeapon, xWeaponFlags);

    addAmmoAndUpdateCounter(xHand, ammoId, xWeaponClip);
    *xOutWeaponClip = 0;
}

void(entity entPlayer, float xHand,
     float xWeapon, float xWeaponFlags, float xWeaponClip, float* xOutWeaponClip)
VRTryReloadWeapon =
{
    if(VRIsWeaponReloadingEnabledFor(entPlayer, xHand) &&
       (xWeaponClip < WeaponIdToBaseClipSize(xWeapon)) &&
       PlayerHasAmmoForWeapon(entPlayer, xWeapon, xWeaponFlags))
    {
        VRReloadWeapon(entPlayer, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
    }
}

void(entity entPlayer, float xHand,
     float xWeapon, float xWeaponFlags, float xWeaponClip, float* xOutWeaponClip)
VRTryUnloadWeapon =
{
    if(VRIsWeaponReloadingEnabledFor(entPlayer, xHand) && xWeaponClip > 0)
    {
        VRUnloadWeapon(entPlayer, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
    }
}

// Whether the weapon in `xHand` is let go of (to be thrown, holstered or switched to the other
// hand): with vr_weapon_grip_mode 0 whenever its grip is not held; with 1 ("sticky") only once
// the grip is let go a second time since the weapon came to the hand.
float(entity ent, float xHand, float xWeapon) VRWeaponLetGo =
{
    float grabbing = VR_HandGrabUtil_IsHandGrabbing(ent, xHand);
    if(cvar_hget(cvarh_vr_weapon_grip_mode) != 1)
    {
        return !grabbing;
    }

    float bit = 1;
    float lastWeapon = ent.stickyweaponoff;
    if(xHand == cVR_MainHand)
    {
        bit = 2;
        lastWeapon = ent.stickyweaponmain;
        ent.stickyweaponmain = xWeapon;
    }
    else
    {
        ent.stickyweaponoff = xWeapon;
    }

    if(VRIsHandEmpty(ent, xHand) || xWeapon != lastWeapon)
    {
        // A new weapon (or none): start over.
        ent.stickyarmed &= ~bit;
        ent.stickypending &= ~bit;
    }

    if(grabbing)
    {
        ent.stickypending &= ~bit;
    }
    else
    {
        if(VR_HandGrabUtil_IsHandPrevGrabbing(ent, xHand) && (ent.stickyarmed & bit))
        {
            ent.stickypending |= bit;
        }

        ent.stickyarmed |= bit;
    }

    return !grabbing && (ent.stickypending & bit);
}

void(entity ent, float xHand) VRWeaponLetGoDone =
{
    if(xHand == cVR_MainHand) ent.stickypending &= ~2;
    else                      ent.stickypending &= ~1;
}

void(entity ent, float xHand,
     float xWeapon, float* xOutWeapon,
     float xWeaponFlags, float* xOutWeaponFlags,
     float xAttackFinished, float xHotspot,
     float xOtherWeapon, float* xOutOtherWeapon,
     float xOtherWeaponFlags, float* xOutOtherWeaponFlags,
     float xWeaponClip, float* xOutWeaponClip,
     float xOtherWeaponClip, float* xOutOtherWeaponClip,
     float xOtherAttackFinished) DoHandImpl =
{
    float letGo = VRWeaponLetGo(ent, xHand, xWeapon);

    float handGrabbing = VR_HandGrabUtil_IsHandGrabbing(ent, xHand);
    float handPrevGrabbing = VR_HandGrabUtil_IsHandPrevGrabbing(ent, xHand);
    float otherHandGrabbing = VR_HandGrabUtil_IsHandGrabbing(ent, VRGetOtherHand(xHand));
    float handEmpty = VRIsHandEmpty(ent, xHand);
    float otherHandEmpty = VRIsHandEmpty(ent, VRGetOtherHand(xHand));
    float triggerPressed = VRGetEntFireButtonPressed(ent, xHand);
    float startedHandGrabbing = VR_HandGrabUtil_StartedHandGrabbing(ent, xHand);
    float handReloadFlicking = VR_HandGrabUtil_IsHandReloadFlicking(ent, xHand);

    if(!deathmatch && time < readytime)
    {
        // Do not do anything if the player is not "ready" yet, after spawning.
        return;
    }

    if(VRIsEntHandReloadingBit(ent, xHand))
    {
        VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
    }

    if(isHolsterHotspot(xHotspot))
    {
        VRSetHolsterHovered(ent, xHotspot, TRUE);
    }

    // Holding a weapon, but not grabbing the controller.
    if(!handEmpty && letGo && time >= xAttackFinished)
    {
        VRWeaponLetGoDone(ent, xHand);

        // No hotspot, or 2H hotspot (unusable).
        // (2H hotspots ignored because you can only dual wield with no weapon out.)
        if(xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB)
        {
            DropWeaponInHand(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip);
        }
        // Put a weapon in a holster.
        else if(isHolsterHotspot(xHotspot))
        {
            // In "immersive" mode, weapons can only be placed in an empty holster.
            // Holstering a weapon frees your hand up.
            if(getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE)
            {
                if(getHolsterWeapon(xHotspot) == WID_FIST)
                {
                    VRPutWeaponInHolster(ent, xHotspot, xWeapon, xWeaponFlags, xWeaponClip);

                    VRSetHandEmpty(ent, xHand);
                    W_SetCurrentAmmoFor(xHand);
                }
                else if(handPrevGrabbing)
                {
                    sprint(ent, VRGetHolsterName(xHotspot));
                    sprint(ent, " holster is not empty.\n");

                    VRHolsterHapticBuzz(ent, xHand, xHotspot);
                }
            }
            // In "quick slot" mode, weapons can be placed in any holster.
            // Holstering a weapon does not free your hand up.
            else if(getWeaponMode() == QVR_WEAPONMODE_CYCLEQUICKSLOT)
            {
                if(getHolsterWeapon(xHotspot) != xWeapon)
                {
                    sound(ent, CHAN_BODY, "weapons/holster1.wav", 1, ATTN_NORM);

                    VRPutWeaponInHolster(ent, xHotspot, xWeapon, xWeaponFlags, xWeaponClip);
                }
            }
        }
        // Swap weapon between hands.
        else if(xHotspot == QVR_HS_HAND_SWITCH)
        {
            if(otherHandGrabbing && otherHandEmpty && time >= xOtherAttackFinished)
            {
                sound(ent, CHAN_BODY, "knight/sword2.wav", 0.7, ATTN_NORM);

                *xOutOtherWeapon = xWeapon;
                *xOutOtherWeaponFlags = xWeaponFlags;
                *xOutOtherWeaponClip = xWeaponClip;
                VRSetHandEmpty(ent, xHand);

                W_SetCurrentAmmo();
            }
        }
    }
    // Holding a weapon and hovering an holster. Possible intention to holster or reload.
    else if(!handEmpty && !letGo) // holding it: gripping, or (vr_weapon_grip_mode 1) not let go of
    {
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) == WID_FIST && time >= xAttackFinished)
            {
                VRHolsterHaptic(ent, xHand, xHotspot);
            }

            if(VRIsWeaponReloadingEnabledFor(ent, xHand) && isReloadHolsterHotspot(xHotspot))
            {
                VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
            }
        }
        else if(handReloadFlicking)
        {
            if(VRIsWeaponReloadingEnabledFor(ent, xHand) && VRIsWeaponReloadFlickingEnabledFor(ent, xHand))
            {
                VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
            }
        }
    }
    // Not holding a weapon, grabbing the controller, and pressing the trigger.
    else if(handEmpty && triggerPressed)
    {
        // Force grab: W_Frame_Forcegrab (vr_wpnforcegrab.qc).
    }
    // Not holding a weapon and hovering an holster. Possible intention to unholster.
    else if(handEmpty && !handGrabbing)
    {
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) != WID_FIST)
            {
                VRHolsterHaptic(ent, xHand, xHotspot);
            }
        }
    }
    // Not holding a weapon (nor carrying a box) and started grabbing the controller.
    else if(handEmpty && startedHandGrabbing && !VR_Carry_IsHolding(ent, xHand))
    {
        // Not a holster. Do nothing.
        if(xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB ||
           xHotspot == QVR_HS_HAND_SWITCH)
        {
            // Intentionally left empty.
        }
        // Attempt unholstering weapon.
        else if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) != WID_FIST)
            {
                sound(ent, CHAN_BODY, "weapons/holster0.wav", 1, ATTN_NORM);
                *xOutWeapon = getHolsterWeapon(xHotspot);
                *xOutWeaponFlags = getHolsterWeaponFlags(xHotspot);
                *xOutWeaponClip = getHolsterWeaponClip(xHotspot);

                // In "immersive" mode, unholstering removes a weapon from a holster.
                if(getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE)
                {
                    setHolsterWeapon(xHotspot, WID_FIST);
                    setHolsterWeaponModel(xHotspot, WeaponIdToModel(WID_FIST, 0 /* weaponflags */));
                    setHolsterWeaponFlags(xHotspot, 0);
                    setHolsterWeaponClip(xHotspot, 0);
                }
                // In "quick slot" mode, unholstering does not remove a weapon from a holster.
                else if(getWeaponMode() == QVR_WEAPONMODE_CYCLEQUICKSLOT)
                {
                    // Intentionally left empty.
                }

                W_SetCurrentAmmoFor(xHand);
            }
        }
    }
}

void() W_Frame =
{
    if(self.impulse)
    {
        ImpulseCommands();
    }

    VR_Carry_HandFrame(cVR_MainHand);
    VR_Carry_HandFrame(cVR_OffHand);

    UpdateHolsterHover(self);

    if(VRGet2HAiming(self))
    {
        // Used for 2H throws.
        self.last_2h_time = time;
    }

    DoHandImpl(self, cVR_OffHand,
               self.weapon2, &self.weapon2,
               self.weaponflags2, &self.weaponflags2,
               self.offhand_attack_finished, self.offhand_hotspot,
               self.weapon, &self.weapon,
               self.weaponflags, &self.weaponflags,
               self.weaponclip2, &self.weaponclip2,
               self.weaponclip, &self.weaponclip,
               self.attack_finished);

    DoHandImpl(self, cVR_MainHand,
               self.weapon, &self.weapon,
               self.weaponflags, &self.weaponflags,
               self.attack_finished, self.mainhand_hotspot,
               self.weapon2, &self.weapon2,
               self.weaponflags2, &self.weaponflags2,
               self.weaponclip, &self.weaponclip,
               self.weaponclip2, &self.weaponclip2,
               self.offhand_attack_finished);

    W_Frame_Forcegrab();
}

/*
========
SuperDamageSound

Plays sound if needed
========
*/
void() SuperDamageSound =
{
    if(self.super_damage_finished > time)
    {
        if(self.super_sound < time)
        {
            self.super_sound = time + 1;
            sound(self, CHAN_BODY, "items/damage3.wav", 1, ATTN_NORM);
        }
    }
};


// TODO VR: (P0): Been playing the latest build in co-op.
//  We have a blast for about 15 minutes and then the player who is the
//  server will usually get a "packet overflow" error and crash.
//  Any easy work arounds to solve that issue?
// TODO: test with developer mode on, see if can reduce packet size
// TODO: VOIP seems to be the culprit!

// TODO VR: (P0): I tower I've the enemys and when I scale myself smaller
//  the hands break
