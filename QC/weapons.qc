
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
    if(cvar_hget(cvarh_vr_enabled) == 1)
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
    }
    else
    {
        VRSetAmmoCounter(xHand, VR_AmmoUtil_GetAmmoCountByAmmoId(self, xAid));
    }
}

float(float xHand) getCurrentAmmoCount =
{
    return VR_AmmoUtil_GetAmmoCountByAmmoId(self, VRGetCurrentAmmo(xHand));
}

float(float xHand) getCurrentAmmoOrClip =
{
    if(self.ishuman && VRIsWeaponReloadingEnabled() && WeaponIdToSupportsReload(VRGetWeapon(xHand)))
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

void(float xHand, float xAid, float xValue) delAmmoAndUpdateCounter =
{
    VR_AmmoUtil_DelAmmoCountByAmmoId(self, xAid, xValue);
    updateAmmoCounters();
    // updateAmmoCounter(xHand, xAid);
}

void(float xHand, float xAid, float xValue) delAmmoOrClipAndUpdateCounter =
{
    if(self.ishuman && VRIsWeaponReloadingEnabled() && WeaponIdToSupportsReload(VRGetWeapon(xHand)))
    {
        VRSetWeaponClip(xHand, VRGetWeaponClip(xHand) - xValue);
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

float(float velmag) VRMeleeStrengthMult =
{
    float strength;
    strength = velmag - cvar_hget(cvarh_vr_melee_threshold);

    if(strength > 1.0)
    {
        strength = pow(strength, 1.5);
    }

    return strength;
}

float(float xHand, float v, float xBase) VRMeleeDmg =
{
    float tmp;

    if(cvar_hget(cvarh_vr_fakevr) == 1 || !self.ishuman)
    {
        return 20;
    }

    if(xHand == cVR_OffHand) tmp = self.offhandvelmag;
    else                     tmp = self.handvelmag;

    return xBase + (v * VRMeleeStrengthMult(tmp)) * cvar_hget(cvarh_vr_melee_dmg_multiplier) * VRMeleeDmgQuadMult();
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

    if(cvar_hget(cvarh_vr_fakevr) == 1 || !self.ishuman)
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

    float power, amplitude, xduration;
    power = VRMeleeStrengthMult(velmag);

    amplitude = power / 10.0;
    if(amplitude > 1.0)       { amplitude = 1.0; }
    else if(amplitude < 0.45) { amplitude = 0.45; }

    xduration = 0.35 + power / 18.0;
    if(xduration < 0.35)     { xduration = 0.35; }
    else if(xduration > 0.9) { xduration = 0.9; }

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
        return 1.5;
    }

    if(positionalDamageType == 2) // extremities
    {
        return 0.35;
    }

    if(positionalDamageType == 3) // legs
    {
        return 0.6;
    }

    return 1;
}

float PositionalDamage(entity xTarget, vector xEndpos, vector xDirection) =
{
    float supportHS;   // Target supports headshots.
    float supportCD;   // Target supports core distance.
    float maxh;        // Max horizontal dimension.
    float diagonality; // How "diagonal" the hit was.
    float headsize;    // Size of target head.
    float head_hdepth; // Depth of head center in the model (horizontal).
    float head_vdepth; // Depth of head center in the model (vertical).
    vector headpos;    // Position of target head.
    vector hitpos;     // Tweaked hit position.
    vector hitdiff;    // Temp vector for hit differences.

    if(cvar_hget(cvarh_vr_positional_damage) == 0)
    {
        return 0;
    }

    supportHS = 0;
    supportCD = 0;

    headpos_x = xTarget.origin_x;
    headpos_y = xTarget.origin_y;
    head_vdepth = 0;

    if(xTarget.classname == "monster_army")
    {
        headpos_z = xTarget.origin_z + 25;
        headsize = 3.6;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_ogre")
    {
        headpos_z = xTarget.origin_z + 37;
        headsize = 5;
        head_hdepth = 24;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_enforcer")
    {
        headpos_z = xTarget.origin_z + 26;
        headsize = 3.7;
        head_hdepth = 14;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_gremlin")
    {
        headpos_z = xTarget.origin_z + 16;
        headsize = 3.2;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_hell_knight")
    {
        headpos_z = xTarget.origin_z + 34;
        headsize = 3.6;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_knight")
    {
        headpos_z = xTarget.origin_z + 24;
        headsize = 3.1;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_shambler")
    {
        headpos_z = xTarget.origin_z + 60;
        headsize = 6;
        head_hdepth = 32;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "monster_shalrath")
    {
        headpos_z = xTarget.origin_z + 54;
        headsize = 4.5;
        head_hdepth = 32;
        supportHS = 1;
        supportCD = 1;
    }
    else if(xTarget.classname == "player")
    {
        headpos_z = xTarget.origin_z + 25;
        headsize = 3.6;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else
    {
        return 0;
    }

    // Since `xEndpos` is from a trace, it always hits one of the AABB planes,
    // so to compensate we check how "diagonal" the hit was and move the hit
    // position more to reach the center and check distance from the head more
    // accurately. This helps shots coming at a 45 degree angle to the AABB.
    maxh = max(fabs(xDirection_x), fabs(xDirection_y));
    maxh = max(maxh, fabs(xDirection_z));
    diagonality = (1 - max(maxh, 0.7)) * 3.33; // Returns a value closer to 1 the more diagonal the shot is.
    hitpos_x = xEndpos_x + xDirection_x * (head_hdepth + (1.414 * diagonality)); // Approx of square diagonal ratio.
    hitpos_y = xEndpos_y + xDirection_y * (head_hdepth + (1.414 * diagonality));
    hitpos_z = xEndpos_z + xDirection_z * head_vdepth;

    // Check for headshot.
    if(supportHS && vlen(hitpos - headpos) < headsize * 2)
    {
        return 1; // headshot
    }

    if(!supportCD)
    {
        return 0; // standard
    }

    // Check for core shot.
    hitdiff = hitpos - xTarget.origin;
    hitdiff_z = 0;

    if(vlen(hitdiff) > (xTarget.maxs_x - xTarget.mins_x) * 0.35)
    {
        return 2; // extremities
    }

    if(hitpos_z < xTarget.origin_z)
    {
        return 3; // legs
    }

    return 0; // standard
}

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

    if(self.ammo_cells < 1)
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

float(vector xHandRot, vector xHandPos, vector xMuzzlePos,
      float xRange, float xMuzzleRange) VRDoMeleeAttackTraceline =
{
    float range = VRMeleeRange(xRange);
    float muzzleRange = VRMeleeRange(xRange + xMuzzleRange);

    makevectors(xHandRot);

    // Prioritize entities taking damage:
    traceline(xHandPos - v_forward*6, xHandPos + v_forward * range, FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    traceline(xMuzzlePos - v_forward*6, xMuzzlePos + v_forward * muzzleRange, FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    traceline(xHandPos - v_up*3, xHandPos + v_up * (range / 1.5), FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    traceline(xMuzzlePos - v_up*3, xMuzzlePos + v_up * (muzzleRange / 1.5), FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    traceline(xHandPos + v_up*3, xHandPos - v_up * (range / 1.5), FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    traceline(xMuzzlePos + v_up*3, xMuzzlePos - v_up * (muzzleRange / 1.5), FALSE, self);
    if(trace_fraction != 1.0 && trace_ent.takedamage) { return TRUE; }

    // Then test the same traces, but do not prioritize:
    traceline(xHandPos - v_forward*6, xHandPos + v_forward * range, FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    traceline(xMuzzlePos - v_forward*6, xMuzzlePos + v_forward * muzzleRange, FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    traceline(xHandPos - v_up*3, xHandPos + v_up * (range / 2.0), FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    traceline(xMuzzlePos - v_up*3, xMuzzlePos + v_up * (muzzleRange / 2.0), FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    traceline(xHandPos + v_up*3, xHandPos - v_up * (range / 2.0), FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    traceline(xMuzzlePos + v_up*3, xMuzzlePos - v_up * (muzzleRange / 2.0), FALSE, self);
    if(trace_fraction != 1.0) { return TRUE; }

    return FALSE;
}

float(float xHand, float xRange, float xMuzzleRange) VRDoMeleeAttackTracelineFor =
{
    return VRDoMeleeAttackTraceline(
        VRGetHandRot(xHand), VRGetHandPos(xHand), VRGetMuzzlePos(xHand), xRange, xMuzzleRange);
}

void(float xHand) HIP_FireMjolnir =
{
    float xdmg = VRMeleeDmg(xHand, 1.10, 5);

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
            xdmg *= 1.25;

        trace_ent.axhitme = 1;
        SpawnBlood (org, v_forward, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
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

    xdmg = VRMeleeDmg(xHand, 1.00, 7);

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

    xdmg = VRMeleeDmg(xHand, 0.75, 3);

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
void(float xHand) W_FistMelee =
{
    vector org;
    float xdmg = VRMeleeDmg(xHand, 0.75, 3);
    float playedsound = FALSE;

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
    // TODO VR (P1): remove debug print
    bprint("hand:");
    bprint(ftos(xHand));
    bprint(" | ");

    bprint("clip:");
    bprint(ftos(VRGetWeaponClip(xHand)));
    bprint(" | ");

    bprint("camm:");
    bprint(ftos(VRGetCurrentAmmo(xHand)));
    bprint("\n");

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
    vector        org;
    float        cells;

    if(self.ammo_cells < 1)
    {
        return;
    }

    // explode if under water
    if(self.waterlevel > 1)
    {
        cells = self.ammo_cells;
        self.ammo_cells = 0;

        // HIPNOTIC
        //MED 01/05/97 added discharge flag
        discharged = 1;
        T_RadiusDamage(self, self, 35*cells, world);
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

    org = VRGetMuzzlePos(xHand);

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

    if(cvar_hget(cvarh_vr_enabled) == 1)
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

    if(cvar_hget(cvarh_vr_enabled) == 1)
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

    if(self.ammo_nails >= 2 && VRGetWeapon(xHand) == WID_SUPER_NAILGUN)
    {
        W_FireSuperSpikes(xHand);
        return;
    }

    if(self.ammo_nails < 1)
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

.float hit_z;
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

            // TODO VR (P1): remove debug print
            bprint("hand:");
            bprint(ftos(xHand));
            bprint(" | ");

            bprint("clip:");
            bprint(ftos(VRGetWeaponClip(xHand)));
            bprint(" | ");

            bprint("camm:");
            bprint(ftos(VRGetCurrentAmmo(xHand)));
            bprint("\n");
        }

        return;
    }

    makevectors(VRGetWeaponFireRot(xHand)); // calculate forward angle for velocity
    self.show_hostile = time + 1; // wake monsters up

    // ZOID--
    RuneApplyBlackNoise(self); // make rune noise
    //--ZOID

    if(xWeapon == WID_AXE)
    {
        if(cvar_hget(cvarh_vr_enabled) == 0 ||
           cvar_hget(cvarh_vr_fakevr) == 1 ||
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
        if(cvar_hget(cvarh_vr_enabled) == 0 ||
           cvar_hget(cvarh_vr_fakevr) == 1 ||
           !self.ishuman)
        {
            if(self.ammo_cells < 30)
                player_hammer1(xHand);
            else
                player_mjolnir1(xHand);

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
    if(cvar_hget(cvarh_vr_fakevr) == 1 || !self.ishuman)
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
    if(!WeaponSupportsSecondaryAmmoFlag(VRGetWeapon(xHand)))
    {
        return;
    }

    VRGunHaptic(VRGetOtherHand(xHand), 0.3, 75, 1.0);
    sound(self, VRGetGunChannel(xHand), "buttons/switch02.wav", 0.5, ATTN_NORM);

    VRSetWeaponFlags(xHand, VRGetWeaponFlags(xHand) ^ QVR_WPNFLAG_USE_SECONDARY_AMMO);
    W_SetCurrentAmmoFor(xHand);
}

void() ImpulseCommands =
{
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
       self.ammo_cells > 0)
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
    if(VRGetFireButtonPressed(xHand) &&
       !VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
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
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweapon0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweapon1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweapon2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweapon3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweapon4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweapon5;

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
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweaponclip0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweaponclip1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweaponclip2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweaponclip3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweaponclip4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweaponclip5;

    bprint("`getHolsterWeaponClip`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    // Bogus, should never happen.
    return self.holsterweapon0;
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

string(float xHolster) getHolsterWeaponModel =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweaponmodel0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweaponmodel1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweaponmodel2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweaponmodel3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweaponmodel4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweaponmodel5;

    bprint("`getHolsterWeaponModel`: invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");

    // Bogus, should never happen.
    return self.holsterweaponmodel0;
}

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
    if(self.throwhit == QVR_THROWHIT_FORCEGRAB &&
       time > GetThrownWeaponForcegrabTime(self) + 0.4)
    {
        // After a long linear forcegrab, restore the movement type to "toss".

        self.throwhit = QVR_THROWHIT_NEVER_HIT;
        self.movetype = MOVETYPE_TOSS;
    }

    if(self.throwhit == QVR_THROWHIT_FORCEGRAB_PARABOLA)
    {
        vector handDiff = VRGetEntHandPos(self.enemy, self.thinkArg) - self.origin;
        self.velocity += normalize(handDiff);
    }

    if(self.waterlevel == 1)
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
    }
    else if((time - self.lastwatertime) > 0.15)
    {
        self.flags = self.flags & ~FL_INWATER;
    }

    if(cvar_hget(cvarh_vr_weapondrop_particles) && (self.flags & (FL_ONGROUND | FL_INWATER)))
    {
        if(random() < 0.2 && self.model != string_null)
        {
            particle2(realorigin(self), '0 0 0', QVR_PARTICLE_PRESET_GUNPICKUP, 1);
        }
    }

    if(self.throwhit == QVR_THROWHIT_HIT)
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

    // if(pointcontents(self.origin) == CONTENT_SKY)
    //     return;

    if(self.throwhit == QVR_THROWHIT_NEVER_HIT)
    {
        float finalDmg = ((vlen(self.velocity) / 700) * GetThrownWeaponBaseDamage(self)) *
                   cvar_hget(cvarh_vr_weapon_throw_damage_mult);

        // hit something that bleeds
        if(GetThrownWeaponDealDamage(self) && other.takedamage)
        {
            sound(self, CHAN_AUTO, "fisthit.wav", 1, ATTN_NORM);

            float posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
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
    else if(self.throwhit == QVR_THROWHIT_FORCEGRAB &&
            time > GetThrownWeaponForcegrabTime(self) + 0.1)
    {
        // After a linear forcegrab, restore the movement type to "toss".

        self.throwhit = QVR_THROWHIT_HIT;
        self.movetype = MOVETYPE_TOSS;
    }
    else if(self.throwhit == QVR_THROWHIT_FORCEGRAB_PARABOLA ||
            time > GetThrownWeaponForcegrabTime(self) + 1.0)
    {
        // After a parabola forcegrab, restore the movement type to "toss".

        self.throwhit = QVR_THROWHIT_HIT;
        self.movetype = MOVETYPE_TOSS;
    }

    // ghetto collision detection/resolution
    if(!(self.flags & FL_ONGROUND) && vlen(self.velocity) > 2.0)
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

void(entity ent, entity entPlayer, float xHand) wpnthrow_forcegrab_instant_ent =
{
    if(xHand == cVR_OffHand && VRIsHandEmpty(entPlayer, cVR_OffHand))
    {
        wpnthrow_handtouch_impl(cVR_OffHand, ent, entPlayer,
            &entPlayer.weapon2, &entPlayer.weaponflags2, &entPlayer.weaponclip2);
    }
    else if(xHand == cVR_MainHand && VRIsHandEmpty(entPlayer, cVR_MainHand))
    {
        wpnthrow_handtouch_impl(cVR_MainHand, ent, entPlayer,
            &entPlayer.weapon, &entPlayer.weaponflags, &entPlayer.weaponclip);
    }
}

float(entity ent, float xHand) wpnthrow_handtouch_ent_hand =
{
    entity entPlayer = ent.handtouch_ent;

    if(!VRIsHandEmpty(entPlayer, xHand))
    {
        return FALSE;
    }

    float pForceGrabbing = VR_ForceGrabUtil_IsEntForceGrabbing(entPlayer, xHand);
    float pStartedGrabbing = VR_HandGrabUtil_StartedHandGrabbing(entPlayer, xHand);
    float* pWeaponPtr = VRGetEntWeaponPtr(entPlayer, xHand);
    float* pWeaponFlagsPtr = VRGetEntWeaponFlagsPtr(entPlayer, xHand);
    float* pWeaponClipPtr = VRGetEntWeaponClipPtr(entPlayer, xHand);

    float performWeaponSwitch = FALSE;

    // Guaranteed grab (linear force grab)
    if(ent.throwhit == QVR_THROWHIT_FORCEGRAB)
    {
        if(VRGetHandtouchHand(ent) == xHand || pForceGrabbing &&
           (GetThrownWeaponForcegrabHand(ent) == xHand))
        {
            performWeaponSwitch = TRUE;
        }
    }
    else // Must catch it
    {
        if(VRGetHandtouchHand(ent) == xHand && pStartedGrabbing)
        {
            performWeaponSwitch = TRUE;
        }
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

void(entity ent, float xValue) SetThrownWeaponForcegrabTime =
{
    ent.lefty = xValue;
}

float(entity ent) GetThrownWeaponForcegrabTime =
{
    return ent.lefty;
}

void(entity ent, float xValue) SetThrownWeaponForcegrabHand =
{
    ent.deadflag = xValue;
}

float(entity ent) GetThrownWeaponForcegrabHand =
{
    return ent.deadflag;
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

void(entity ent, float xValue) SetThrownWeaponTimeout =
{
    ent.health = xValue;
}

float(entity ent) GetThrownWeaponTimeout =
{
    return ent.health;
}

void MakeGrabbable(entity newmis)
{
    newmis.movetype = MOVETYPE_TOSS;
    newmis.solid = SOLID_BBOX;

    newmis.throwhit = QVR_THROWHIT_NEVER_HIT;
    newmis.throwstabilizedim = -1;

    newmis.flags = newmis.flags | FL_EASYHANDTOUCH;

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

    makevectors(xThrowVel);
    vector tmp_handavel = xAVel;
    newmis.avelocity = tmp_handavel_x * v_forward * 30
                     + tmp_handavel_y * v_right * -30
                     + tmp_handavel_z * v_up * -30;

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

    newmis.velocity = (xThrowVel * 120 * WeaponIdToThrowMult(xWeapon)) * cvar_hget(cvarh_vr_weapon_throw_velocity_mult)
                    + thrower.velocity;
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
        throwVel, randomVec(20),
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
        throwVel, randomVec(20),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWeapon)
    );
}

float() getWeaponMode =
{
    return cvar_hget(cvarh_vr_holster_mode);
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


    if(getWeaponThrowMode() == QVR_WEAPONTHROWMODE_IMMERSIVE)
    {
        CreateThrownWeapon(ent, xWeapon, xWeaponFlags, xWeaponClip,
            VRGetHandPos(xHand), VRGetHandRot(xHand),
            throwVel, VRGetHandAVel(xHand),
            FALSE /* disappear on hit */,
            TRUE  /* deal damage */,
            WeaponIdToThrowDamage(xWeapon));
    }
    else if(getWeaponThrowMode() == QVR_WEAPONTHROWMODE_DISAPPEARONHIT)
    {
        CreateThrownWeapon(ent, xWeapon, xWeaponFlags, xWeaponClip,
            VRGetHandPos(xHand), VRGetHandRot(xHand),
            throwVel, VRGetHandAVel(xHand),
            TRUE /* disappear on hit */,
            TRUE /* deal damage */,
            WeaponIdToThrowDamage(xWeapon));
    }
    else if(getWeaponThrowMode() == QVR_WEAPONTHROWMODE_DISCARD)
    {
        // Intentionally left empty.
    }

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
    float handGrabbing = VR_HandGrabUtil_IsHandGrabbing(ent, xHand);
    float handPrevGrabbing = VR_HandGrabUtil_IsHandPrevGrabbing(ent, xHand);
    float otherHandGrabbing = VR_HandGrabUtil_IsHandGrabbing(ent, VRGetOtherHand(xHand));
    float handEmpty = VRIsHandEmpty(ent, xHand);
    float otherHandEmpty = VRIsHandEmpty(ent, VRGetOtherHand(xHand));
    float triggerPressed = VRGetEntFireButtonPressed(ent, xHand);
    float startedHandGrabbing = VR_HandGrabUtil_StartedHandGrabbing(ent, xHand);

    vector handPos = VRGetHandPos(xHand);
    vector handRot = VRGetHandRot(xHand);

    if(!deathmatch && time < readytime)
    {
        // Do not do anything if the player is not "ready" yet, after spawning.
        return;
    }

    if(isHolsterHotspot(xHotspot))
    {
        VRSetHolsterHovered(ent, xHotspot, TRUE);
    }

    // Holding a weapon, but not grabbing the controller.
    if(!handEmpty && !handGrabbing && time >= xAttackFinished)
    {
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
    // Holding a weapon and hovering an holster. Possible intention to holster.
    else if(!handEmpty && handGrabbing && time >= xAttackFinished)
    {
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) == WID_FIST)
            {
                VRHolsterHaptic(ent, xHand, xHotspot);
            }

            // TODO VR (P1): reload mechanic
            if((xWeaponClip < WeaponIdToBaseClipSize(xWeapon)) &&
               PlayerHasAmmoForWeapon(ent, xWeapon, xWeaponFlags))
            {
                sound(ent, CHAN_BODY, "reload1.wav", 1, ATTN_NORM);

                float missingAmmo = WeaponIdToBaseClipSize(xWeapon) - xWeaponClip;
                delAmmoAndUpdateCounter(xHand, WeaponIdToAmmoId(xWeapon, xWeaponFlags), missingAmmo);
                *xOutWeaponClip = xWeaponClip + missingAmmo;

                // TODO VR (P1): remove debug print
                bprint("reloading clip to:");
                bprint(ftos(xWeaponClip + missingAmmo));
                bprint("\n");
            }
        }
    }
    // Not holding a weapon, grabbing the controller, and pressing the trigger.
    else if(handEmpty && triggerPressed)
    {
        if(VR_ForcegrabUtil_GetForcegrabMode() == QVR_VRFORCEGRABMODE_DISABLED)
        {
            // Intentionally left empty.
        }
        else if(VR_ForcegrabUtil_GetForcegrabMode() == QVR_VRFORCEGRABMODE_PARABOLA)
        {
            VR_ForcegrabUtil_Impl_DoForcegrabParabola(xHand, handPos, handRot);
        }
        else if(handGrabbing && VR_ForcegrabUtil_GetForcegrabMode() == QVR_VRFORCEGRABMODE_LINEAR)
        {
            VR_ForcegrabUtil_Impl_DoForcegrabLinear(xHand, handPos, handRot);
        }
        else if(handGrabbing && VR_ForcegrabUtil_GetForcegrabMode() == QVR_VRFORCEGRABMODE_INSTANT)
        {
            VR_ForcegrabUtil_Impl_DoForcegrabInstant(xHand, handPos, handRot);
        }
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
    // Not holding a weapon and started grabbing the controller.
    else if(handEmpty && startedHandGrabbing)
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

void(float xHand) W_Frame_Forcegrab_Impl =
{
    if(!VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
    {
        ParticlesAndHapticsForEligibleForceGrabWeapons(
            xHand, VRGetHandPos(xHand), VRGetHandRot(xHand));
    }

    // Refresh force grab until trigger is released
    if(VRGetFireButtonPressed(xHand) &&
       VR_ForceGrabUtil_IsEntForceGrabbing(self, xHand))
    {
        VR_ForceGrabUtil_EntStartForceGrabbing(self, xHand);
    }
}

void() W_Frame_Forcegrab =
{
    if(VR_ForcegrabUtil_GetForcegrabMode() == QVR_VRFORCEGRABMODE_DISABLED)
    {
        VR_ForceGrabUtil_EntStopForceGrabbing(self, cVR_MainHand);
        VR_ForceGrabUtil_EntStopForceGrabbing(self, cVR_OffHand);
        return;
    }

    W_Frame_Forcegrab_Impl(cVR_MainHand);
    W_Frame_Forcegrab_Impl(cVR_OffHand);
}

void() W_Frame =
{
    if(self.impulse)
    {
        ImpulseCommands();
    }

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

// TODO VR: (P2) remove backpack spinning animation.

