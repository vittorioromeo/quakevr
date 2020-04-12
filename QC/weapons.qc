/*
*/

// HIPNOTIC
//JIM
void( vector pos ) placebullethole
void (entity targ, entity inflictor, entity attacker, float damage) T_Damage;
void (entity targ, entity inflictor, entity attacker, float damage) T_Damage_VRMelee;
void (float xHand) player_run;
void(entity bomb, entity attacker, float rad, entity ignore) T_RadiusDamage;
void(vector org, vector vel, float damage) SpawnBlood;
void() SuperDamageSound;

// QUAKE VR
void(vector org, float damage) SpawnBloodSplash;
void() PainSound;

float(float it) ItemIdToWeaponId;
float(float wid) WeaponIdToItemId;
float(float wid, float wpnFlags) PlayerHasWeapon;

float() crandom =
{
    return 2*(random() - 0.5);
};

// QUAKE VR
void(float xHand, float xduration, float frequency, float amplitude) VRGunHaptic =
{
    haptic(xHand, 0.0, xduration, frequency, amplitude);
}

void(vector org) BroadcastGunshotEffect =
{
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_GUNSHOT);
    WriteCoord (MSG_BROADCAST, org_x);
    WriteCoord (MSG_BROADCAST, org_y);
    WriteCoord (MSG_BROADCAST, org_z);
}

vector() vrProjectileVelocity =
{
    local vector handv;

    return VEC_ORIGIN;

    // TODO VR: this only works when facing forwards. It needs to be revisited
    // the hand velocity vector is absolute and doesn't take the world into
    // account. It probably needs to be multiplied with the direction vector
    // of where the player is facing...?

    if (cvar("vr_enabled") == 1)
    {
        // VR: add hand velocity and player velocity to the final projectile
        // velocity, in order to carry momentum
        handv_x = self.handvel_x;
        handv_y = self.handvel_z  * -1;
        handv_z = self.handvel_y;
        return (handv * 50) + (self.velocity);
    }

    return VEC_ORIGIN;
};

vector(float xHand) VRGetHandPos =
{
    if(xHand == OFF_HAND) return self.offhandpos;
    else                  return self.handpos;
}

vector(float xHand) VRGetHandRot =
{
    if(xHand == OFF_HAND) return self.offhandrot;
    else                  return self.handrot;
}

vector(float xHand) VRGetHandVel =
{
    if(xHand == OFF_HAND) return self.offhandvel;
    else                  return self.handvel;
}

vector(float xHand) VRGetHandAVel =
{
    if(xHand == OFF_HAND) return self.offhandavel;
    else                  return self.handavel;
}

float(float xHand) VRMeleeHitSoundPlayed =
{
    if(xHand == OFF_HAND) return self.offhand_melee_hit_sound_played;
    else                  return self.melee_hit_sound_played;
}

void(float xHand, float v) VRMeleeSetHitSoundPlayed =
{
    if(xHand == OFF_HAND) self.offhand_melee_hit_sound_played = v;
    else                  self.melee_hit_sound_played = v;
}

float(float xHand) VRGunChannel =
{
    if(xHand == OFF_HAND) return CHAN_WEAPON2;
    else                  return CHAN_WEAPON;
}

// TODO VR:
float(float xHand) isAmmolessWeapon =
{
    if(xHand == OFF_HAND) return self.currentammo2 == AID_NONE;
    else                  return self.currentammo == AID_NONE;
}

void() updateAmmoCounters =
{
    if(self.currentammo != AID_NONE)
    {
        self.ammocounter = getCurrentAmmo(MAIN_HAND);
    }
    else
    {
        self.ammocounter = 0;
    }

    if(self.currentammo2 != AID_NONE)
    {
        self.ammocounter2 = getCurrentAmmo(OFF_HAND);
    }
    else
    {
        self.ammocounter2 = 0;
    }
}

void(float xHand, float v) setCurrentAmmo =
{
    local float idx;

    if(xHand == OFF_HAND) { idx = self.currentammo2; }
    else                  { idx = self.currentammo; }

         if(idx == AID_SHELLS)  { self.ammo_shells = v; }
    else if(idx == AID_NAILS)   { self.ammo_nails = v; }
    else if(idx == AID_ROCKETS) { self.ammo_rockets = v; }
    else if(idx == AID_CELLS)   { self.ammo_cells = v; }
    else
    {
        bprint("Attempting to set ammo of invalid type '");
        bprint(ftos(idx));
        bprint("', value '");
        bprint(ftos(v));
        bprint("'\n");
    }

    updateAmmoCounters();
}

float(float xHand) getCurrentAmmo =
{
    local float idx;

    if(xHand == OFF_HAND) { idx = self.currentammo2; }
    else                  { idx = self.currentammo; }

    if(idx == AID_SHELLS)  { return self.ammo_shells; }
    if(idx == AID_NAILS)   { return self.ammo_nails; }
    if(idx == AID_ROCKETS) { return self.ammo_rockets; }
    if(idx == AID_CELLS)   { return self.ammo_cells; }

    bprint("Attempting to get ammo of invalid type '");
    bprint(ftos(idx));
    bprint("'\n");
}

float() VRMeleeDmgQuadMult =
{
    if (self.super_damage_finished > time)
    {
        return 2.75;
    }

    return 1;
}

float(float velmag) VRMeleeStrengthMult =
{
    local float strength;
    strength = velmag - cvar("vr_melee_threshold");

    if (strength > 1.0)
    {
        strength = pow(strength, 1.5);
    }

    return strength;
}

float(float xHand, float v) VRMeleeDmg =
{
    local float tmp;

    if (cvar("vr_fakevr") == 1)
    {
        return 20;
    }

    if(xHand == OFF_HAND) tmp = self.offhandvelmag;
    else                  tmp = self.handvelmag;

    return (v * VRMeleeStrengthMult(tmp)) * cvar("vr_melee_dmg_multiplier") * VRMeleeDmgQuadMult();
}

float(float v) VRMeleeRange =
{
    local float mult;

    if (self.super_damage_finished > time)
    {
        mult = 1.25;
    }
    else
    {
        mult = 1;
    }

    if (cvar("vr_fakevr") == 1)
    {
        mult = mult * 3.5;
    }

    return v * cvar("vr_melee_range_multiplier") * mult;
}

void(float hand, float velmag) VRMeleeHaptic =
{
    local float power, amplitude, xduration;
    power = VRMeleeStrengthMult(velmag);

    amplitude = power / 10.0;
    if (amplitude > 1.0) { amplitude = 1.0; }
    else if (amplitude < 0.45) { amplitude = 0.45; }

    xduration = 0.35 + power / 18.0;
    if (xduration < 0.35) { xduration = 0.35; }
    else if (xduration > 0.9) { xduration = 0.9; }

    haptic(hand, 0.0, xduration, 75.0, amplitude);
}

float PositionalDamageFactor(float positionalDamageType)
{
    if (positionalDamageType == 0) // standard
    {
        return 1;
    }

    if (positionalDamageType == 1) // headshot
    {
        return 1.5;
    }

    if (positionalDamageType == 2) // extremities
    {
        return 0.35;
    }

    if (positionalDamageType == 3) // legs
    {
        return 0.6;
    }

    return 1;
}

// TODO VR: positional damage WIP, add cvar
float PositionalDamage(entity xTarget, vector xEndpos, vector xDirection) =
{
    local float supportHS;   // Target supports headshots.
    local float supportCD;   // Target supports core distance.
    local float maxh;        // Max horizontal dimension.
    local float diagonality; // How "diagonal" the hit was.
    local float headsize;    // Size of target head.
    local float head_hdepth; // Depth of head center in the model (horizontal).
    local float head_vdepth; // Depth of head center in the model (vertical).
    local vector headpos;    // Position of target head.
    local vector hitpos;     // Tweaked hit position.
    local vector hitdiff;    // Temp vector for hit differences.

    supportHS = 0;
    supportCD = 0;

    headpos_x = xTarget.origin_x;
    headpos_y = xTarget.origin_y;
    head_vdepth = 0;

    if (xTarget.classname == "monster_army")
    {
        headpos_z = xTarget.origin_z + 24;
        headsize = 3.2;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_ogre")
    {
        headpos_z = xTarget.origin_z + 37;
        headsize = 5;
        head_hdepth = 24;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_enforcer")
    {
        headpos_z = xTarget.origin_z + 26;
        headsize = 3.6;
        head_hdepth = 14;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_gremlin")
    {
        headpos_z = xTarget.origin_z + 16;
        headsize = 3.2;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_hell_knight")
    {
        headpos_z = xTarget.origin_z + 34;
        headsize = 3.6;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_knight")
    {
        headpos_z = xTarget.origin_z + 24;
        headsize = 3.1;
        head_hdepth = 12;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_shambler")
    {
        headpos_z = xTarget.origin_z + 60;
        headsize = 6;
        head_hdepth = 32;
        supportHS = 1;
        supportCD = 1;
    }
    else if (xTarget.classname == "monster_shalrath")
    {
        headpos_z = xTarget.origin_z + 54;
        headsize = 4.5;
        head_hdepth = 32;
        supportHS = 1;
        supportCD = 1;
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
    if (supportHS && vlen(hitpos - headpos) < headsize * 2)
    {
        return 1; // headshot
    }

    if (!supportCD)
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
void(float damage) spawn_touchblood;
/*
================
HIPNOTIC WEAPONS
================
*/
void() HIP_LaserTouch =
{
    local vector org;
    local vector spot1,spot2;
    local vector oldvel;
    local float r;
    local float adjDmg;
    local float posDmgType;

    self.owner = world;
    self.cnt = self.cnt + 1;
    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    oldvel = normalize(self.old_velocity);
    spot1 = self.origin - (16*oldvel);
    spot2 = self.origin + (16*oldvel);
    traceline (spot1, spot2, FALSE, self);  // see through other monsters
    self.origin = trace_endpos;

    org = self.origin;

    if (other.health)
    {
        if (self.lastvictim == other)
        {
            self.dmg = self.dmg / 2;
        }

        spawn_touchblood (self.dmg);

        makevectors (self.angles);
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = self.dmg * PositionalDamageFactor(posDmgType);

        T_Damage (other, self, self.lastvictim, adjDmg);
    }
    else if ((self.cnt == 3) || (random()<0.15))
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_GUNSHOT);
        WriteCoord (MSG_BROADCAST, org_x);
        WriteCoord (MSG_BROADCAST, org_y);
        WriteCoord (MSG_BROADCAST, org_z);
    }
    else
    {
       self.dmg = 0.9 * self.dmg;
       self.velocity = oldvel+(2*trace_plane_normal);
       self.velocity = normalize(self.velocity);
       self.velocity = self.speed * self.velocity;
       self.old_velocity = self.velocity;
       if (self.flags & FL_ONGROUND)
           self.flags = self.flags - FL_ONGROUND;
       r = random();
       sound (self, CHAN_WEAPON, "hipweap/laserric.wav", 1, ATTN_STATIC);
       return;
    }

    sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
    remove(self);
};

void() HIP_LaserThink =
{
    if (time > self.attack_finished)
    {
        remove(self);
        return;
    }

    if (self.flags & FL_ONGROUND)
    {
        self.flags = self.flags - FL_ONGROUND;
    }

    self.velocity = self.old_velocity;
    self.angles = vectoangles(self.velocity);
    self.nextthink = time + 0.1;
};

void(vector org, vector vec, float light) HIP_LaunchLaser =
{
    sound (self ,CHAN_WEAPON, "hipweap/laserg.wav", 1, ATTN_NORM);

    vec = normalize(vec);

    newmis = spawn();
    newmis.owner = self;
    newmis.classname = "hiplaser";
    newmis.lastvictim = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    if (light)
       newmis.effects = EF_DIMLIGHT;

    setmodel (newmis, "progs/lasrspik.mdl");
    setsize (newmis, '0 0 0', '0 0 0');

    setorigin (newmis, org);

    newmis.speed = 1750;
    newmis.dmg = 18;
    newmis.velocity = vec * newmis.speed;
    newmis.old_velocity = newmis.velocity;
    newmis.angles = vectoangles(newmis.velocity);
    newmis.avelocity = '0 0 400';

    newmis.nextthink = time;
    newmis.attack_finished = time + 5;
    newmis.think = HIP_LaserThink;
    newmis.touch = HIP_LaserTouch;
    newmis.count = 0;
};

// TODO VR: move
float(float xHand) VRFireButtonPressed =
{
    if(xHand == OFF_HAND) return self.button3;
    else                  return self.button0;
}

void(float xHand, float v) VRSetWeapon =
{
    if(xHand == OFF_HAND) self.weapon2 = v;
    else                  self.weapon = v;
}

float(entity e, float xHand) VRGetEntWeapon =
{
    if(xHand == OFF_HAND) return e.weapon2;
    else                  return e.weapon;
}

float(float xHand) VRGetWeapon =
{
    return VRGetEntWeapon(self, xHand);
}

float(entity e, float xHand) VRIsEntHandGrabbing =
{
    if(xHand == OFF_HAND) return e.offhand_grabbing;
    else                  return e.mainhand_grabbing;
}

float(float xHand) VRIsHandGrabbing =
{
    return VRIsEntHandGrabbing(self, xHand);
}

vector(float xHand) VRGetWeaponFireRot =
{
    return VRGetHandRot(xHand);
}

vector(float xHand) VRGetMuzzlePos =
{
    if(xHand == OFF_HAND) return self.offmuzzlepos;
    else                  return self.muzzlepos;
}

vector(float xHand) VRGetWeaponFireRotFwd =
{
    makevectors (VRGetWeaponFireRot(xHand));
    return v_forward;
}


/*
=================
HIP_FireLaser
=================
*/
void(float xHand, float stat) HIP_FireLaser =
{
    local vector org;
    local vector dir;
    local vector out;
    local float ofs;
    local float aofs;

    if (!VRFireButtonPressed(xHand))
    {
        player_run (xHand);
        return;
    }

    if (self.ammo_cells < 1)
    {
        // VRSetWeapon(xHand, W_BestWeapon());
        // W_SetCurrentAmmo ();
        return;
    }

    SuperDamageSound();
    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (VRGetWeaponFireRot(xHand));

    ofs = 6;
    out = v_forward;
    out_z = 0;
    out = normalize(out);
    org = VRGetMuzzlePos(xHand) + ((-1) * v_up) + (2*out);
    dir = aim (self, 1000);
    aofs = ofs * 0.15;
    if (stat == 0)
    {
        // TODO VR: deal with ammo
        setCurrentAmmo(xHand, self.ammo_cells = self.ammo_cells - 1);
        org = org + (aofs*v_right);
        HIP_LaunchLaser(org, dir, 0);
        org = org - (2*aofs*v_right);
        HIP_LaunchLaser(org, dir, 0);
    }
    else if (stat == 1)
    {
        setCurrentAmmo(xHand, self.ammo_cells = self.ammo_cells - 1);
        org = org + (ofs*(v_up * 0.4));
        if (random()<0.1)
        {
            HIP_LaunchLaser(org, dir, 1);
            newmis.dmg = 25;
        }
        else
        {
            HIP_LaunchLaser(org, dir, 0);
        }
    }

    self.punchangle_x = -1;
    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 1);
};

/*
=================
HIP_HammerDamage
=================
*/
void(vector p1, vector p2, entity from, float damage) HIP_HammerDamage =
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
//JIM
    if (trace_ent.takedamage && !trace_ent.wetsuit_finished )
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from, damage);
        if (self.classname == "player")
        {
            if (other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline (p1 + f, p2 + f, FALSE, self);
//JIM
    if (trace_ent != e1 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished )
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from, damage);
    }
    e2 = trace_ent;

    traceline (p1 - f, p2 - f, FALSE, self);
//JIM
    if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished )
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage (trace_ent, from, from, damage);
    }
};

void(vector vStart, vector vEnd) LightningParticles =
{
    particle2 (vStart, '0 0 0', 3 /* lightning preset */, 20);
    particle2 (vStart, '0 0 0', 4 /* smoke preset */, 1);
    particle2 (vStart, '0 0 0', 5 /* sparks preset */, 4);

    particle2 (vEnd, '0 0 0', 3 /* lightning preset */, 60);
    particle2 (vEnd, '0 0 0', 4 /* smoke preset */, 1);
    particle2 (vEnd, '0 0 0', 5 /* sparks preset */, 12);
}


void() HIP_LightningThink =
   {
   local vector org;
   local vector dst;
   local entity head;
   local entity selected;
   local float cur_dist;
   local float head_dist;
   local vector vec;
   local float dot;
   local float oldstate;
   local float dam;


   if (time > self.delay)
      {
      if (self.enemy != world)
         self.enemy.struck_by_mjolnir = 0;
      remove(self);
      return;
      }
   oldstate = self.state;
   if (self.state==0)
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
//            if (visible(head) && (head!=self.owner.owner) && (head.health>0))
            if ((visible(head)) && (head!=self.owner.owner) && (head.health>0))
               {
               head_dist = vlen(head.origin - self.lastvictim.origin);
               if ((head_dist<cur_dist) && (head.struck_by_mjolnir==0))
                  {
                  selected = head;
                  cur_dist = head_dist;
                  }
               }
            }
         head = head.chain;
         }
      if (selected != world)
         {
         self.state = 1;
         self.enemy = selected;
         self.enemy.struck_by_mjolnir = 1;
         }
      else
         {
         local vector   end;

         makevectors(self.v_angle);
         org = self.owner.origin;
         end = org + v_forward*200;
         end = end + (((400*random()) - 200) * v_right);

         traceline (org, end, TRUE, self);

         WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
         WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
         WriteEntity (MSG_BROADCAST, self);
         WriteByte (MSG_BROADCAST, 0); /* disambiguator */
         WriteCoord (MSG_BROADCAST, org_x);
         WriteCoord (MSG_BROADCAST, org_y);
         WriteCoord (MSG_BROADCAST, org_z);
         WriteCoord (MSG_BROADCAST, trace_endpos_x);
         WriteCoord (MSG_BROADCAST, trace_endpos_y);
         WriteCoord (MSG_BROADCAST, trace_endpos_z);

        LightningParticles(org, trace_endpos);

         self.nextthink = time + 0.1;
         return;
         }
      }
   org = self.lastvictim.origin;
   dst = self.enemy.absmin + 0.25*(self.enemy.absmax-self.enemy.absmin);
   dst = dst + (random()*0.5*(self.enemy.absmax-self.enemy.absmin));
   traceline (org, dst, TRUE, self.owner.owner);

   if (trace_fraction != 1.0 || self.enemy.health<=0)
      {
      self.enemy.struck_by_mjolnir = 0;
      self.state = 0;
      self.nextthink = time + 0.1;
      return;
      }
   WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
   WriteEntity (MSG_BROADCAST, self);
   WriteByte (MSG_BROADCAST, 0); /* disambiguator */
   WriteCoord (MSG_BROADCAST, org_x);
   WriteCoord (MSG_BROADCAST, org_y);
   WriteCoord (MSG_BROADCAST, org_z);
   WriteCoord (MSG_BROADCAST, trace_endpos_x);
   WriteCoord (MSG_BROADCAST, trace_endpos_y);
   WriteCoord (MSG_BROADCAST, trace_endpos_z);

   LightningParticles(org, trace_endpos);

   vec = normalize(self.enemy.origin - self.owner.origin);
   dot = vec * self.owner.movedir;
   head = self;
   self = self.owner.owner;
   if (oldstate==0)
      dam = 80;
   else
      dam = 30;
   if (dot>0.3)
      HIP_HammerDamage (org, trace_endpos, self, dam);
   else
      HIP_HammerDamage (org, trace_endpos, self, dam*0.5);
   self = head;

   self.nextthink = time + 0.2;
   };

void(entity prev, entity own, float dst) HIP_SpawnMjolnirLightning =
   {
   local entity light;
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
   local entity light;
   // spawn lightning base
   light = spawn();
//   light.origin = self.origin - '0 0 24' + (32*v_forward);
   light.origin = trace_endpos;
   light.flags = 0;
   light.owner = self;
   light.struck_by_mjolnir = 1;
   light.think = SUB_Remove;
   light.nextthink = time + 1;
   sound (light, CHAN_AUTO, "hipweap/mjolslap.wav", 1, ATTN_NORM);
   sound (light, CHAN_WEAPON, "hipweap/mjolhit.wav", 1, ATTN_NORM);
   makevectors(self.v_angle);
   light.movedir = v_forward;
   HIP_SpawnMjolnirLightning( light, light, 350 );
   HIP_SpawnMjolnirLightning( light, light, 350 );
   HIP_SpawnMjolnirLightning( light, light, 350 );
   HIP_SpawnMjolnirLightning( light, light, 350 );
   };

void(float xHand) HIP_FireMjolnirLightning =
{
    local float cells;

    // explode if under water
    if (self.waterlevel > 1)
    {
        cells = self.ammo_cells;
        self.ammo_cells = 0;
        discharged = 1;
        T_RadiusDamage (self, self, 35*cells, world);
        discharged = 0;
        W_SetCurrentAmmo ();
        return;
    }

    setCurrentAmmo(xHand, self.ammo_cells = self.ammo_cells - 15);

    HIP_SpawnMjolnirBase();
};

void(float xHand) HIP_FireMjolnir =
{
    local vector source;
    local vector org;
    local float  xdmg;

    xdmg = VRMeleeDmg(xHand, 1.15);

    makevectors(VRGetHandRot(xHand));
    source = VRGetHandPos(xHand);
    // TODO VR: improve this collision detection
    traceline (source, source + v_forward * VRMeleeRange(28), FALSE, self);
    if (trace_fraction == 1.0)
        return;

    org = trace_endpos - v_forward*4;

    if (trace_ent.takedamage)
    {
        VRMeleeHaptic(1, self.handvelmag);

        if (time >= getAttackFinished(xHand) && self.ammo_cells >= 15)
        {
            HIP_FireMjolnirLightning(xHand);
            setAttackFinished(xHand, time + 0.5);
        }

        if (trace_ent.classname == "monster_zombie")
            xdmg *= 1.25;

        trace_ent.axhitme = 1;
        SpawnBlood (org, v_forward, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
    }
    else
    {
        // hit wall
        if (trace_fraction != 1.0)
        {
            if (time >= getAttackFinished(xHand) && self.ammo_cells >= 15)
            {
                HIP_FireMjolnirLightning(xHand);
                setAttackFinished(xHand, time + 0.5);
            }

            if (VRMeleeHitSoundPlayed(xHand) == FALSE)
            {
                VRMeleeHaptic(1, self.handvelmag);
                sound (self, VRGunChannel(xHand), "hipweap/mjoltink.wav", 1, ATTN_NORM);
                BroadcastGunshotEffect(org);
                VRMeleeSetHitSoundPlayed(xHand, TRUE);
            }
        }
        else
        {
            if (VRMeleeHitSoundPlayed(xHand) == FALSE)
            {
                VRMeleeHaptic(1, self.handvelmag);
                sound (self, VRGunChannel(xHand), "knight/sword1.wav", 1, ATTN_NORM);
                VRMeleeSetHitSoundPlayed(xHand, TRUE);
            }
        }
    }
};


// called by worldspawn
void() W_Precache =
{
    precache_sound ("weapons/r_exp3.wav");    // new rocket explosion
    precache_sound ("weapons/rocket1i.wav");    // spike gun
    precache_sound ("weapons/sgun1.wav");
    precache_sound ("weapons/guncock.wav");    // player shotgun
    precache_sound ("weapons/ric1.wav");    // ricochet (used in c code)
    precache_sound ("weapons/ric2.wav");    // ricochet (used in c code)
    precache_sound ("weapons/ric3.wav");    // ricochet (used in c code)
    precache_sound ("weapons/spike2.wav");    // super spikes
    precache_sound ("weapons/tink1.wav");    // spikes tink (used in c code)
    precache_sound ("weapons/grenade.wav");    // grenade launcher
    precache_sound ("weapons/bounce.wav");        // grenade bounce
    precache_sound ("weapons/shotgn2.wav");    // super shotgun
    precache_sound ("knight/sword1.wav");    // gun melee

    // HIPNOTIC
    //MED
   precache_sound ("enforcer/enfstop.wav");   // laser cannon
//MED 11/06/96
   precache_sound ("hipweap/laserg.wav");   // laser cannon
   precache_sound ("hipweap/laserric.wav");   // laser cannon ricochet
   precache_sound ("hipweap/proxwarn.wav");   // proximity bomb
   precache_sound ("hipweap/proxbomb.wav");   // proximity bomb
//MED 11/19/96
   precache_sound ("hipweap/mjolhit.wav");   // mjolnir
   precache_sound ("hipweap/mjolslap.wav");   // mjolnir
   precache_sound ("hipweap/mjoltink.wav");   // mjolnir

    // ROGUE
    //ZOID--
    // grapple
    precache_sound("weapons/chain1.wav");
    precache_sound("pendulum/hit.wav");
//--ZOID

    // QUAKE VR
    precache_sound ("knight/sword1.wav");
    precache_sound ("knight/sword2.wav");
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
    local vector source;
    local vector org;
    local float  xdmg;

    xdmg = VRMeleeDmg(xHand, 0.95);

    makevectors(VRGetHandRot(xHand));
    source = VRGetHandPos(xHand);
    // TODO VR: improve this collision detection
    traceline (source - v_forward*24, source + v_forward * VRMeleeRange(28), FALSE, self);
    if (trace_fraction == 1.0)
        return;

    org = trace_endpos - v_forward*4;

    if (trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        SpawnBloodSplash (org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);

        if (VRMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHaptic(1, self.handvelmag);
            sound (self, VRGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
            VRMeleeSetHitSoundPlayed(xHand, TRUE);
        }
    }
    else
    {
        // hit wall
        if (VRMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHaptic(1, self.handvelmag);
            sound (self, VRGunChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
            VRMeleeSetHitSoundPlayed(xHand, TRUE);
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
    local    vector    source;
    local    vector    org;
    local    float    xdmg;

    xdmg = VRMeleeDmg(xHand, 0.5);

    makevectors(VRGetHandRot(xHand));
    source = VRGetHandPos(xHand);
    traceline (source - v_forward*24, source + v_forward * VRMeleeRange(26), FALSE, self);
    if (trace_fraction == 1.0)
    {
        // TODO VR: try muzzle position for longer guns
        source = self.muzzlepos;
        traceline (source - v_forward*8, source + v_forward * VRMeleeRange(8), FALSE, self);

        if (trace_fraction == 1.0)
        {
            return;
        }
    }

    org = trace_endpos - v_forward*4;

    if (trace_ent.takedamage)
    {
        trace_ent.axhitme = 1;
        SpawnBloodSplash (org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);

        if (VRMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHaptic(1, self.handvelmag);
            sound (self, VRGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
            VRMeleeSetHitSoundPlayed(xHand, TRUE);
        }
    }
    else
    {
        // hit wall
        if (VRMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHaptic(1, self.handvelmag);
            sound (self, VRGunChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
            VRMeleeSetHitSoundPlayed(xHand, TRUE);
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
    local vector source;
    local vector org;
    local float  xdmg;
    local float  playedsound;

    xdmg = VRMeleeDmg(xHand, 0.75);

    // TODO VR: good place to add effects
    // particle2 (self.offhandpos, '0 0 0', 3 /* lightning preset */, 60);
    // particle2 (self.offhandpos, '0 0 0', 4 /* smoke preset */, 1);
    // particle2 (self.offhandpos, '0 0 0', 5 /* sparks preset */, 12);

    makevectors (VRGetHandRot(xHand));
    source = VRGetHandPos(xHand);
    traceline (source - v_forward*24, source + v_forward * VRMeleeRange(26), FALSE, self);
    if (trace_fraction == 1.0)
        return;

    org = trace_endpos - v_forward*4;

    if (VRMeleeHitSoundPlayed(xHand) == FALSE)
    {
        sound (self, VRGunChannel(xHand), "fisthit.wav", 1.0, ATTN_NORM);
        playedsound = TRUE;
    }

    if (trace_ent.takedamage)
    {
        VRMeleeHaptic(0, self.offhandvelmag);
        trace_ent.axhitme = 1;
        SpawnBloodSplash (org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
    }
    else
    {
        // hit wall
        if (VRMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHaptic(0, self.offhandvelmag);
            PainSound();
            SpawnBloodSplash (org, xdmg);
            playedsound = TRUE;
        }

        BroadcastGunshotEffect(org);
    }

    if (playedsound)
    {
        VRMeleeSetHitSoundPlayed(xHand, TRUE);
    }
};


//============================================================================



vector() wall_velocity =
{
    local vector    vel;

    vel = normalize (self.velocity);
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
    local    entity missile;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_NOT;

    makevectors (self.angles);

    missile.velocity = vel;
    missile.velocity_z = missile.velocity_z + 250 + 50*random();

    missile.avelocity = '3000 1000 2000';

// set missile duration
    missile.nextthink = time + 1;
    missile.think = SUB_Remove;

    setmodel (missile, "progs/zom_gib.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, org);
};

/*
================
SpawnBlood
================
*/
void(vector org, vector vel, float damage) SpawnBlood =
{
    particle2 (org, vel * 0.2, 1 /* blood preset */, damage);
};

// QUAKE VR
/*
================
SpawnBloodSplash
================
*/
void(vector org, float damage) SpawnBloodSplash =
{
    local vector        pvel;

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
    local vector    vel;

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
    if (!multi_ent)
        return;
    T_Damage (multi_ent, self, self, multi_damage);
};

void(entity hit, float damage) AddMultiDamage =
{
    if (!hit)
        return;

    if (hit != multi_ent)
    {
        ApplyMultiDamage ();
        multi_damage = damage;
        multi_ent = hit;
    }
    else
        multi_damage = multi_damage + damage;
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
    local    vector    vel, org;

    vel = normalize(dir + v_up*crandom() + v_right*crandom());
    vel = vel + 2*trace_plane_normal;
    vel = vel * 200;

    org = trace_endpos - dir*4;

    if (trace_ent.takedamage)
    {
        SpawnBlood (org, vel*0.2, damage);
        AddMultiDamage (trace_ent, damage);
    }
    else
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
    local vector direction;
    local float adjDmg;
    local float posDmgType;

    // TODO VR: wrong?
    // makevectors(self.v_angle);
    makevectors(vectoangles(dir));

    ClearMultiDamage ();
    while (shotcount > 0)
    {
        direction = dir + crandom()*spread_x*v_right + crandom()*spread_y*v_up;

        traceline (src, src + direction*2048, FALSE, self);
        if (trace_fraction != 1.0)
        //JIM
        {
            posDmgType = PositionalDamage(trace_ent, trace_endpos, direction);
            adjDmg = 4 * PositionalDamageFactor(posDmgType);

            TraceAttack (adjDmg, direction);

            // HIPNOTIC
            if (trace_ent == world)
            {
                placebullethole( trace_endpos );
            }
        }

        shotcount = shotcount - 1;
    }
    ApplyMultiDamage ();
};

void(float xHand, float shotcount, vector dir, vector spread) FireBullets =
{
    FireBulletsImpl(shotcount, dir, spread, VRGetMuzzlePos(xHand));
}

void(float shotcount, vector dir, vector spread) EnemyFireBullets =
{
    local    vector    src;

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
    local vector dir;

    sound (self, VRGunChannel(xHand), "weapons/guncock.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    setCurrentAmmo(xHand, self.ammo_shells = self.ammo_shells - 1);

    dir = VRGetWeaponFireRotFwd(xHand);

    FireBullets (xHand, 6, dir, '0.01 0.01 0');

    VRGunHaptic(xHand, 0.25, 70, 0.75);

    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 1);
};


/*
================
W_FireSuperShotgun
================
*/
void(float xHand) W_FireSuperShotgun =
{
    local vector dir;

    if (getCurrentAmmo(xHand) == 1)
    {
        W_FireShotgun (xHand);
        return;
    }

    sound (self, VRGunChannel(xHand), "weapons/shotgn2.wav", 1, ATTN_NORM);

    self.punchangle_x = -4;

    setCurrentAmmo(xHand, self.ammo_shells = self.ammo_shells - 2);
    dir = VRGetWeaponFireRotFwd(xHand);
    FireBullets (xHand, 14, dir, '0.035 0.025 0');

    VRGunHaptic(xHand, 0.55, 75, 1.0);

    particle2 (VRGetMuzzlePos(xHand) - '0 2 0', '0 0 0', 6 /* gun smoke preset */, 1);
    particle2 (VRGetMuzzlePos(xHand) + '0 2 0', '0 0 0', 6 /* gun smoke preset */, 1);
};


/*
==============================================================================

ROCKETS

==============================================================================
*/

void()    s_explode1    =    [0,        s_explode2] {};
void()    s_explode2    =    [1,        s_explode3] {};
void()    s_explode3    =    [2,        s_explode4] {};
void()    s_explode4    =    [3,        s_explode5] {};
void()    s_explode5    =    [4,        s_explode6] {};
void()    s_explode6    =    [5,        SUB_Remove] {};

void() BecomeExplosion =
{
    self.movetype = MOVETYPE_NONE;
    self.velocity = '0 0 0';
    self.touch = SUB_Null;
    setmodel (self, "progs/s_explod.spr");
    self.solid = SOLID_NOT;
    s_explode1 ();
};

void() T_MissileTouch =
{
    local float    damg;

    if (other == self.owner)
        return;        // don't explode on owner

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    damg = 100 + random()*20;

    if (other.health)
    {
        if (other.classname == "monster_shambler")
            damg = damg * 0.5;    // mostly immune
        T_Damage (other, self, self.owner, damg );
    }

    // don't do radius damage to the other, because all the damage
    // was done in the impact
    T_RadiusDamage (self, self.owner, 120, other);

//    sound (self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
};



/*
================
W_FireRocket
================
*/
void(float xHand) W_FireRocket =
{
    local    entity missile;

    setCurrentAmmo(xHand, self.ammo_rockets = self.ammo_rockets - 1);

    sound (self, VRGunChannel(xHand), "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";

    // set missile speed
    missile.velocity = VRGetWeaponFireRotFwd(xHand) * 1000;
    missile.angles = vectoangles(missile.velocity);

    missile.touch = T_MissileTouch;
    missile.velocity = missile.velocity + vrProjectileVelocity();

    // set missile duration
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;

    setmodel (missile, "progs/missile.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, VRGetMuzzlePos(xHand));

    // HIPNOTIC
    //MED 11/09/96
    newmis = missile;

    VRGunHaptic(xHand, 0.5, 75, 1.0);

    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 3);
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
//JIM
    if (trace_ent.takedamage && !trace_ent.wetsuit_finished )
    {
        SpawnBloodSplash (trace_endpos, damage*5);
        T_Damage (trace_ent, from, from, damage);
        if (self.classname == "player")
        {
            if (other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline (p1 + f, p2 + f, FALSE, self);
//JIM
    if (trace_ent != e1 && trace_ent.takedamage &&
    !trace_ent.wetsuit_finished )
    {
        SpawnBloodSplash (trace_endpos, damage*5);
        T_Damage (trace_ent, from, from, damage);
    }
    e2 = trace_ent;

    traceline (p1 - f, p2 - f, FALSE, self);
//JIM
    if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished )
    {
        SpawnBloodSplash (trace_endpos, damage*5);
        T_Damage (trace_ent, from, from, damage);
    }
};


void(float xHand) W_FireLightning =
{
    local    vector        org;
    local    float        cells;

    if (self.ammo_cells < 1)
    {
        // VRSetWeapon(xHand, W_BestWeapon());
        // W_SetCurrentAmmo ();
        return;
    }
// explode if under water
    if (self.waterlevel > 1)
    {
        cells = self.ammo_cells;
        self.ammo_cells = 0;

    // HIPNOTIC
//MED 01/05/97 added discharge flag
        discharged = 1;
        T_RadiusDamage (self, self, 35*cells, world);
        discharged = 0;
        W_SetCurrentAmmo ();
        return;
    }

    if (self.t_width < time)
    {
        sound (self, VRGunChannel(xHand), "weapons/lhit.wav", 1, ATTN_NORM);
        self.t_width = time + 0.6;
    }

    self.punchangle_x = -2;
    setCurrentAmmo(xHand, self.ammo_cells = self.ammo_cells - 1);

    org = VRGetMuzzlePos(xHand);

    makevectors (VRGetHandRot(xHand));
    traceline (org, org + v_forward*600, TRUE, self);
    particle2 (trace_endpos, '0 0 0', 3 /* lightning preset */, 60);
    particle2 (trace_endpos, '0 0 0', 4 /* smoke preset */, 1);
    particle2 (trace_endpos, '0 0 0', 5 /* sparks preset */, 12);
    LightningDamage (org, trace_endpos + v_forward*4, self, 30);

    VRGunHaptic(xHand, 0.4, 175, 1.0);
    particle2 (org, '0 0 0', 6 /* gun smoke preset */, 1);
};


//=============================================================================


void() GrenadeExplode =
{
    T_RadiusDamage (self, self.owner, 120, world);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
};

void() GrenadeTouch =
{
    if (other == self.owner)
        return;        // don't explode on owner
    if (other.takedamage == DAMAGE_AIM)
    {
        GrenadeExplode();
        return;
    }
    sound (self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM);    // bounce sound
    if (self.velocity == '0 0 0')
        self.avelocity = '0 0 0';
};

/*
================
W_FireGrenade
================
*/
void(float xHand) W_FireGrenade =
{
    local entity missile;
    local vector tmpAngles;

    setCurrentAmmo(xHand, self.ammo_rockets = self.ammo_rockets - 1);

    sound (self, VRGunChannel(xHand), "weapons/grenade.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_BOUNCE;
    missile.solid = SOLID_BBOX;
    missile.classname = "grenade";

    // set missile speed
    tmpAngles = VRGetWeaponFireRot(xHand);
    makevectors (tmpAngles);

    if (cvar("vr_enabled") == 1)
    {
        if (tmpAngles_x)
        {
            missile.velocity = v_forward*600 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
        }

        missile.velocity = missile.velocity + vrProjectileVelocity();
    }
    else
    {
        if (tmpAngles_x)
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

    setmodel (missile, "progs/grenade.mdl");
    setsize (missile, '0 0 0', '0 0 0');
    setorigin (missile, VRGetMuzzlePos(xHand));

    VRGunHaptic(xHand, 0.4, 75, 1.0);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 2);
};


void() ProximityExplode =
{
    T_RadiusDamage (self, self.owner, 95, world);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
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
   if (other == self)
      return;
   if (other.classname == self.classname)
      return;
   self.movetype = MOVETYPE_TOSS;
   if (self.state == 1)
      return;
   if (vlen(other.velocity) > 0)
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }
   if (other.takedamage == DAMAGE_AIM)
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }
   sound (self, CHAN_WEAPON, "weapons/bounce.wav", 1, ATTN_NORM); // bounce sound
   self.movetype = MOVETYPE_NONE;
   setsize (self, '-8 -8 -8', '8 8 8');
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
   local entity   head;
   local float    blowup;

   if ((time > self.delay) || (NumProximityGrenades > 15) || (vlen(self.spawnmaster.velocity)>0) )
      {
      ProximityGrenadeExplode();
      self.think();
      return;
      }
   self.owner = world;
   self.takedamage = DAMAGE_YES;
   head = findradius(self.origin, 140);
   blowup = 0;

   while (head)
      {
      if ((head != self) && (head.health > 0) && (head.flags & (FL_CLIENT|FL_MONSTER)) && (head.classname!=self.classname))
         blowup = 1;
      if ((head.classname == self.classname) && (head.state==0))
         blowup = 1;
      traceline(self.origin,head.origin,TRUE,self);
      if (trace_fraction != 1.0)
         blowup = 0;
      if (blowup==1)
         {
         sound (self, CHAN_WEAPON, "hipweap/proxwarn.wav", 1, ATTN_NORM);
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
    local entity missile;
    local vector tmpAngles;

    NumProximityGrenades = NumProximityGrenades + 1;
    setCurrentAmmo(xHand, self.ammo_rockets = self.ammo_rockets - 1);

    sound (self, VRGunChannel(xHand), "hipweap/proxbomb.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    missile = spawn ();
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
    makevectors (tmpAngles);

    if (cvar("vr_enabled") == 1)
    {
        if (tmpAngles_x)
        {
            missile.velocity = v_forward*600 + crandom()*v_right*10 + crandom()*v_up*10;
        }
        else
        {
            missile.velocity = v_forward * 600;
        }

        missile.velocity = missile.velocity + vrProjectileVelocity();
    }
    else
    {
        if (tmpAngles_x)
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

    setmodel (missile, "progs/proxbomb.mdl");
    setorigin (missile, VRGetMuzzlePos(xHand));
    setsize (missile, '-1 -1 -1', '1 1 1');

    VRGunHaptic(xHand, 0.4, 75, 1.0);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 2);
};

//=============================================================================

void() spike_touch;
void() superspike_touch;


/*
===============
launch_spike

Used for both the player and the ogre
===============
*/
void(vector org, vector dir) launch_spike =
{
    newmis = spawn ();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;

    newmis.angles = vectoangles(dir);

    newmis.touch = spike_touch;
    newmis.classname = "spike";
    newmis.think = SUB_Remove;
    newmis.nextthink = time + 6;
    setmodel (newmis, "progs/spike.mdl");
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);
    setorigin (newmis, org);

    newmis.velocity = dir * 1750;
};

float(float xHand) W_FireSuperSpikes =
{
    local vector dir;

    sound (self, VRGunChannel(xHand), "weapons/spike2.wav", 1, ATTN_NORM);
    setAttackFinished(xHand, time + 0.2);
    setCurrentAmmo(xHand, self.ammo_nails = self.ammo_nails - 2);

    makevectors (VRGetWeaponFireRot(xHand));
    dir = v_forward;

    launch_spike (VRGetMuzzlePos(xHand), dir);
    newmis.touch = superspike_touch;
    setmodel (newmis, "progs/s_spike.mdl");
    setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);

    newmis.velocity = newmis.velocity + vrProjectileVelocity();

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2 (VRGetMuzzlePos(xHand), '0 0 0', 6 /* gun smoke preset */, 1);
};

void(float xHand, float ox) W_FireSpikes =
{
    local vector    dir;

    makevectors (VRGetWeaponFireRot(xHand));

    if (self.ammo_nails >= 2 && self.weapon == WID_SUPER_NAILGUN)
    {
        W_FireSuperSpikes (xHand);
        return;
    }

    if (self.ammo_nails < 1)
    {
        // VRSetWeapon(xHand, W_BestWeapon ());
        // W_SetCurrentAmmo ();
        return;
    }

    sound (self, VRGunChannel(xHand), "weapons/rocket1i.wav", 1, ATTN_NORM);
    setAttackFinished(xHand, time + 0.2);
    setCurrentAmmo(xHand, self.ammo_nails = self.ammo_nails - 1);
    dir = v_forward;
    launch_spike (VRGetMuzzlePos(xHand) + v_right*ox, dir);

    newmis.velocity = newmis.velocity + vrProjectileVelocity();

    self.punchangle_x = -2;

    VRGunHaptic(xHand, 0.05, 150, 1.0);
    particle2 (VRGetMuzzlePos(xHand) + v_right*ox, '0 0 0', 6 /* gun smoke preset */, 1);
};

void() spike_bullethole =
{
    if (trace_ent == world)
    {
        placebullethole(trace_endpos);
    }
}

.float hit_z;
void() spike_touch =
{
    local float posDmgType;
    local float adjDmg;

    if (other == self.owner)
        return;

    if (other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makevectors(self.angles);
    traceline (self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

// hit something that bleeds
    if (other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = 9 * PositionalDamageFactor(posDmgType);

        spawn_touchblood (adjDmg);
        T_Damage (other, self, self.owner, adjDmg);
    }
    else
    {
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);

        if (self.classname == "wizspike")
            WriteByte (MSG_BROADCAST, TE_WIZSPIKE);
        else if (self.classname == "knightspike")
            WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE);
        else
        {
            spike_bullethole();
            WriteByte (MSG_BROADCAST, TE_SPIKE);
        }

        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }

    remove(self);
};

void() superspike_touch =
{
    local float posDmgType;
    local float adjDmg;

    if (other == self.owner)
        return;

    if (other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if (pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    makevectors(self.angles);
    traceline (self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

// hit something that bleeds
    if (other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = 18 * PositionalDamageFactor(posDmgType);

        spawn_touchblood (adjDmg);
        T_Damage (other, self, self.owner, adjDmg);
    }
    else
    {
        spike_bullethole();
        WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
        WriteCoord (MSG_BROADCAST, self.origin_x);
        WriteCoord (MSG_BROADCAST, self.origin_y);
        WriteCoord (MSG_BROADCAST, self.origin_z);
    }

    remove(self);

};


/*
===============================================================================

PLAYER WEAPON USE

===============================================================================
*/

void(float xWeapon, float* xCurrentAmmo, string* xModel, float* xFrame) W_SetCurrentAmmoImpl =
{
    *xCurrentAmmo = WeaponIdToAmmoType(xWeapon);
    *xModel = WeaponIdToModel(xWeapon);
    *xFrame = 0;
};

void() W_SetCurrentAmmo =
{
    player_run (MAIN_HAND); // get out of any weapon firing states
    player_run (OFF_HAND);  // get out of any weapon firing states

    // TODO VR: this always sets both weaponframes, it should only set it if the weapon changed
    // It happens when putting a gun in a holster

    // TODO VR: upper body holsters like boneworks

    // TODO VR: animation bug while running and shooting

    W_SetCurrentAmmoImpl(self.weapon,
        &self.currentammo, &self.weaponmodel, &self.weaponframe);

    W_SetCurrentAmmoImpl(self.weapon2,
        &self.currentammo2, &self.weaponmodel2, &self.weaponframe2);

    updateAmmoCounters();
};

float() W_BestWeapon =
{
    local    float    it;

    it = self.items;

    if (self.waterlevel <= 1 && self.ammo_cells >= 1 && (it & IT_LIGHTNING) )
        return WID_LIGHTNING;
//MED
    if(self.ammo_cells >= 1 && (it & IT_LASER_CANNON) )
        return WID_LASER_CANNON;
    if(self.ammo_nails >= 2 && (it & IT_SUPER_NAILGUN) )
        return WID_SUPER_NAILGUN;
    if(self.ammo_shells >= 2 && (it & IT_SUPER_SHOTGUN) )
        return WID_SUPER_SHOTGUN;
    if(self.ammo_nails >= 1 && (it & IT_NAILGUN) )
        return WID_NAILGUN;
    if(self.ammo_shells >= 1 && (it & IT_SHOTGUN) )
        return WID_SHOTGUN;
//MED
    if( it & IT_MJOLNIR )
        return WID_MJOLNIR;

    return WID_AXE;
};

float(float xHand, float xWeapon, float* xOutWeapon) W_CheckNoAmmo =
{
    if (isAmmolessWeapon(xHand))
    {
        return TRUE;
    }

    if (getCurrentAmmo(xHand) > 0)
    {
        return TRUE;
    }

    // *xOutWeapon = W_BestWeapon ();
    // W_SetCurrentAmmo ();

    // drop the weapon down
    return FALSE;
};

/*
============
W_Attack

An attack impulse can be triggered now
============
*/
void(float xHand)    player_axe1;
void(float xHand)    player_axeb1;
void(float xHand)    player_axec1;
void(float xHand)    player_axed1;
void(float xHand)    player_shot1;
void(float xHand)    player_nail1;
void(float xHand)    player_light1;
void(float xHand)    player_rocket1;
// HIPNOTIC
//MED
void(float xHand)   player_laser1;
void(float xHand)   player_hammer1;
void(float xHand)   player_mjolnir1;
// ROGUE
//ZOID--
void(float xHand)  player_grapple1;
void(float xHand)  player_grapple3;
//--ZOID


void(float xHand, float xWeapon, float xAttackFinished,
     float* xOutWeapon, float* xOutAttackFinished) W_AttackImpl =
{
    local float r;

    if (!W_CheckNoAmmo (xHand, xWeapon, xOutWeapon))
    {
        if (time > xAttackFinished)
        {
            sound (self, VRGunChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
        }

        return;
    }

    makevectors(VRGetWeaponFireRot(xHand)); // calculate forward angle for velocity
    self.show_hostile = time + 1; // wake monsters up

    if (xWeapon == WID_AXE)
    {
        if(cvar("vr_enabled") == 0 || cvar("vr_fakevr") == 1)
        {
            sound (self, VRGunChannel(xHand), "weapons/ax1.wav", 1, ATTN_NORM);
            r = random();
            if (r < 0.25)
                player_axe1 (xHand);
            else if (r<0.5)
                player_axeb1 (xHand);
            else if (r<0.75)
                player_axec1 (xHand);
            else
                player_axed1 (xHand);
            *xOutAttackFinished = time + 0.5;
        }
    }
    else if (xWeapon == WID_SHOTGUN)
    {
        player_shot1 (xHand);
        W_FireShotgun (xHand);
        *xOutAttackFinished = time + 0.5;
    }
    else if (xWeapon == WID_SUPER_SHOTGUN)
    {
        player_shot1 (xHand);
        W_FireSuperShotgun (xHand);
        *xOutAttackFinished = time + 0.7;
    }
    else if (xWeapon == WID_NAILGUN)
    {
        player_nail1 (xHand);
    }
    else if (xWeapon == WID_SUPER_NAILGUN)
    {
        player_nail1 (xHand);
    }
    else if (xWeapon == WID_GRENADE_LAUNCHER)
    {
        player_rocket1(xHand);
        W_FireGrenade(xHand);
        *xOutAttackFinished = time + 0.6;
    }
    else if (xWeapon == WID_ROCKET_LAUNCHER)
    {
        player_rocket1(xHand);
        W_FireRocket(xHand);
        *xOutAttackFinished = time + 0.8;
    }
    else if (xWeapon == WID_LIGHTNING)
    {
        player_light1(xHand);
        *xOutAttackFinished = time + 0.1;
        sound (self, CHAN_AUTO, "weapons/lstart.wav", 1, ATTN_NORM);
    }
    // HYPNOTIC
//MED
    else if (xWeapon == WID_LASER_CANNON)
    {
        player_laser1(xHand);
    }
//MED
   else if (xWeapon == WID_MJOLNIR)
   {
        if(cvar("vr_enabled") == 0 || cvar("vr_fakevr") == 1)
        {
            if (self.ammo_cells < 30)
                player_hammer1(xHand);
            else
                player_mjolnir1(xHand);

            *xOutAttackFinished = time + 0.8;
        }
   }
//MED
    else if (xWeapon == WID_PROXIMITY_GUN)
    {
        player_rocket1(xHand);
        W_FireProximityGrenade(xHand);
        *xOutAttackFinished = time + 0.6;
    }
    else if (xWeapon == WID_GRAPPLE)
    {
        if (!getHookOut(xHand))
            player_grapple1(xHand);
        else
            player_grapple3(xHand);

        *xOutAttackFinished = time + 0.1;
    }
};

void() W_Attack =
{
    W_AttackImpl(MAIN_HAND, self.weapon, self.attack_finished,
        &self.weapon, &self.attack_finished);
};

void() W_OffHandAttack =
{
    W_AttackImpl(OFF_HAND, self.weapon2, self.offhand_attack_finished,
        &self.weapon2, &self.offhand_attack_finished);
};

/*
============
W_ChangeWeapon

============
*/
//MED 10/18/96 added hipweapons
void() W_ChangeWeapon =
{
    local float it, am, fl;
    local float oldimpulse;

    it = self.items;
    am = 0;

//MED
    if (self.impulse == 1)
    {
        fl = IT_AXE;
    }
    else if (self.impulse == 2)
    {
        fl = IT_SHOTGUN;
        if (self.ammo_shells < 1)
            am = 1;
    }
    else if (self.impulse == 3)
    {
        fl = IT_SUPER_SHOTGUN;
        if (self.ammo_shells < 2)
            am = 1;
    }
    else if (self.impulse == 4)
    {
        fl = IT_NAILGUN;
        if (self.ammo_nails < 1)
            am = 1;
    }
//MED
    else if (self.impulse == 5)
    {
        fl = IT_SUPER_NAILGUN;
        if (self.ammo_nails < 2)
            am = 1;

    }
//MED
    else if (self.impulse == 6)
    {
      if (self.weapon == WID_GRENADE_LAUNCHER)
      {
         fl = IT_PROXIMITY_GUN;
      }
      else
      {
         fl = IT_GRENADE_LAUNCHER;
      }
      if (self.ammo_rockets < 1)
          am = 1;
    }
    else if (self.impulse == 7)
    {
        fl = IT_ROCKET_LAUNCHER;
        if (self.ammo_rockets < 1)
            am = 1;
    }
//MED
    else if (self.impulse == 8)
    {
        fl = IT_LIGHTNING;
        if (self.ammo_cells < 1)
            am = 1;
    }
    else if (self.impulse == 225)
    {
        fl = IT_LASER_CANNON;
        if (self.ammo_cells < 1)
            am = 1;
    }
    else if (self.impulse == 226)
    {
        fl = IT_MJOLNIR;
    }

//MED
    oldimpulse = self.impulse;
    self.impulse = 0;

    if (!(self.items & fl))
    {
//MED
        if (fl == IT_GRENADE_LAUNCHER)
        {
            fl = IT_PROXIMITY_GUN;
            if (!(self.items & fl))
            {
                sprint (self, "no weapon.\n");
                return;
            }
            if (self.ammo_rockets < 1)
                am = 1;
            else
                am = 0;
        }
        else
        {
            sprint (self, "no weapon.\n");
            return;
        }
    }

    if (am)
    {    // don't have the ammo
        sprint (self, "not enough ammo.\n");
        return;
    }

//
// set weapon, set ammo
//
    self.weapon = ItemIdToWeaponId(fl);
    W_SetCurrentAmmo ();
};

/*
============
CheatCommand
============
*/
void() CheatCommand =
{
    if (deathmatch)
        return;

    self.ammo_rockets = 100;
    self.ammo_nails = 200;
    self.ammo_shells = 100;
    self.items = self.items |
        IT_AXE |
        IT_SHOTGUN |
        IT_SUPER_SHOTGUN |
        IT_NAILGUN |
        IT_SUPER_NAILGUN |
        IT_GRENADE_LAUNCHER |
        IT_ROCKET_LAUNCHER |
        IT_KEY1 | IT_KEY2;

    self.ammo_cells = 200;
    self.items = self.items | IT_LIGHTNING;

//MED
    self.items = self.items | IT_LASER_CANNON;
    self.items = self.items | IT_MJOLNIR;
    self.items = self.items | IT_PROXIMITY_GUN;

    self.weapon = WID_ROCKET_LAUNCHER;
    self.impulse = 0;
    W_SetCurrentAmmo ();
};

/*
============
CycleWeaponCommand

Go to the next weapon with ammo
============
*/
void(float xWeapon, float* xOutWeapon, float* xOutPrevWeapon) CycleWeaponCommand =
{
    local    float    it, am;

    it = self.items;
    self.impulse = 0;

    while (1)
    {
        am = 0;

        // TODO VR: fist removed
        if (xWeapon == WID_FIST)
        {
            xWeapon = WID_MJOLNIR;
        }
        else if (xWeapon == WID_MJOLNIR)
        {
            xWeapon = WID_AXE;
        }
        else if (xWeapon == WID_AXE)
        {
            xWeapon = WID_SHOTGUN;
            // if (self.ammo_shells < 1) { am = 1; }
        }
        else if (xWeapon == WID_SHOTGUN)
        {
            xWeapon = WID_SUPER_SHOTGUN;
            // if (self.ammo_shells < 2) { am = 1; }
        }
        else if (xWeapon == WID_SUPER_SHOTGUN)
        {
            xWeapon = WID_NAILGUN;
            // if (self.ammo_nails < 1) { am = 1; }
        }
        else if (xWeapon == WID_NAILGUN)
        {
            xWeapon = WID_SUPER_NAILGUN;
            // if (self.ammo_nails < 2) { am = 1; }
        }
        else if (xWeapon == WID_SUPER_NAILGUN)
        {
            xWeapon = WID_GRENADE_LAUNCHER;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_GRENADE_LAUNCHER)
        {
            xWeapon = WID_PROXIMITY_GUN;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_PROXIMITY_GUN)
        {
            xWeapon = WID_ROCKET_LAUNCHER;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_ROCKET_LAUNCHER)
        {
            xWeapon = WID_LIGHTNING;
            // if (self.ammo_cells < 1) { am = 1; }
        }
        else if (xWeapon == WID_LIGHTNING)
        {
            xWeapon = WID_LASER_CANNON;
            // if (self.ammo_cells < 1) { am = 1; }
        }
        else if (xWeapon == WID_LASER_CANNON)
        {
            xWeapon = WID_GRAPPLE;
        }
        else if (xWeapon == WID_GRAPPLE)
        {
            // TODO VR: fist removed
            // xWeapon = WID_FIST;
            xWeapon = WID_MJOLNIR;
        }

        *xOutWeapon = xWeapon;
        *xOutPrevWeapon = xWeapon;

        if (PlayerHasWeapon(xWeapon, it) && am == 0)
        {
            W_SetCurrentAmmo();
            return;
        }
    }
};

/*
============
CycleWeaponReverseCommand

Go to the prev weapon with ammo
============
*/
void(float xWeapon, float* xOutWeapon) CycleWeaponReverseCommand =
{
    local    float    it, am;

    it = self.items;
    self.impulse = 0;

    while (1)
    {
        am = 0;

        if (xWeapon == WID_FIST)
        {
            xWeapon = WID_GRAPPLE;
        }
        else if (xWeapon == WID_GRAPPLE)
        {
            xWeapon = WID_LASER_CANNON;
            // if (self.ammo_cells < 1) { am = 1; }
        }
        else if (xWeapon == WID_LASER_CANNON)
        {
            xWeapon = WID_LIGHTNING;
            // if (self.ammo_cells < 1) { am = 1; }
        }
        else if (xWeapon == WID_LIGHTNING)
        {
            xWeapon = WID_ROCKET_LAUNCHER;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_ROCKET_LAUNCHER)
        {
            xWeapon = WID_PROXIMITY_GUN;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_PROXIMITY_GUN)
        {
            xWeapon = WID_GRENADE_LAUNCHER;
            // if (self.ammo_rockets < 1) { am = 1; }
        }
        else if (xWeapon == WID_GRENADE_LAUNCHER)
        {
            xWeapon = WID_SUPER_NAILGUN;
            // if (self.ammo_nails < 2) { am = 1; }
        }
        else if (xWeapon == WID_SUPER_NAILGUN)
        {
            xWeapon = WID_NAILGUN;
            // if (self.ammo_nails < 1) { am = 1; }
        }
        else if (xWeapon == WID_NAILGUN)
        {
            xWeapon = WID_SUPER_SHOTGUN;
            // if (self.ammo_shells < 2) { am = 1; }
        }
        else if (xWeapon == WID_SUPER_SHOTGUN)
        {
            xWeapon = WID_SHOTGUN;
            // if (self.ammo_shells < 1) { am = 1; }
        }
        else if (xWeapon == WID_SHOTGUN)
        {
            xWeapon = WID_AXE;
        }
        else if (xWeapon == WID_AXE)
        {
            xWeapon = WID_MJOLNIR;
        }
        else if (xWeapon == WID_MJOLNIR)
        {
            // TODO VR: fist removed
            // xWeapon = WID_FIST;
            xWeapon = WID_GRAPPLE;
        }

        *xOutWeapon = xWeapon;

        if (PlayerHasWeapon(xWeapon, it) && am == 0)
        {
            W_SetCurrentAmmo();
            return;
        }
    }
};

// TODO VR: off hand weapon cycling (TODO dual-wielding)
void() CycleOffHandWeapon =
{
    if (self.weapon2 == WID_FIST)
    {
        self.weapon2 = WID_GRAPPLE;
    }
    else if (self.weapon2 == WID_GRAPPLE)
    {
        self.weapon2 = WID_SHOTGUN;
    }
    else if (self.weapon2 == WID_SHOTGUN)
    {
        self.weapon2 = WID_FIST;
    }

    W_SetCurrentAmmo();
}

/*
============
ServerflagsCommand

Just for development
============
*/
void() ServerflagsCommand =
{
    if (deathmatch)
        return;
    serverflags = serverflags * 2 + 1;
};

void() QuadCheat =
{
    if (deathmatch)
        return;
    self.super_time = 1;
    self.super_damage_finished = time + 30;
    self.items = self.items | IT_QUAD;
    dprint ("quad cheat\n");
};

// HYPNOTIC
//MED
void() WetsuitCheat =
    {
   if (deathmatch || coop)
        return;
   self.hipnotic_items = self.hipnotic_items | HIP_IT_WETSUIT;
    self.wetsuit_time = 1;
    self.wetsuit_finished = time + 30;
   bprint( "wetsuit cheat\n" );
    };

//MED
void() EmpathyShieldsCheat =
    {
   if (deathmatch || coop)
        return;
   self.hipnotic_items = self.hipnotic_items | HIP_IT_EMPATHY_SHIELDS;
   self.empathy_time = 1;
   self.empathy_finished = time + 30;
   bprint( "empathy shields cheat\n" );
    };

void() DumpEntities =
   {
   local entity   head;
   local float i;

   i = 1;
   head = nextent(world);
   while (head!=world)
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
   local entity   head;
   local float i;

   i = 1;
   head = nextent(world);
   while (head!=world)
      {
      if (head.health > 0)
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
   local entity   head;

   if (deathmatch || coop)
        return;
   bprint("Genocide!\n");
   head = nextent(world);
   while (head!=world)
      {
      if ((head.health > 0) && (head.flags & FL_MONSTER))
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
   if (dump_coord == 1)
      bprint("Dumping Player Location\n");
   };
void () DumpCoordinates =
   {
   local entity pl;
   pl = checkclient();
   if (pl)
      {
      bprint("Player: ");
      bprint(vtos(pl.origin));
      bprint("\n");
      }
   };
/*
============
ImpulseCommands

============
*/
void() ImpulseCommands =
{
    if (self.impulse >= 1 && self.impulse <= 8)
        W_ChangeWeapon ();

    if (self.impulse >= 225 && self.impulse <= 226)
        W_ChangeWeapon ();

    if (self.impulse == 9)
    {
        CheatCommand ();
    }

    // TODO VR:
    if (self.impulse == 10 && time >= self.attack_finished)
    {
        CycleWeaponCommand (self.weapon, &self.weapon, &self.prevweapon);
    }

    if (self.impulse == 11)
    {
        ServerflagsCommand ();
    }

    if (self.impulse == 12 && time >= self.offhand_attack_finished)
    {
        // TODO VR: CycleWeaponReverseCommand ();
        CycleWeaponCommand (self.weapon2, &self.weapon2, &self.prevweapon2);
    }

    // TODO VR:
    if (self.impulse == 13)
    {
        CycleOffHandWeapon ();
    }

//JIM
    if ( self.impulse == 200 )
        WetsuitCheat();
//MED
    if ( self.impulse == 201 )
        EmpathyShieldsCheat();
    if ( self.impulse == 205 )
        Genocide();
    if ( self.impulse == 206 )
        ToggleDump();
    if ( self.impulse == 202 )
        DumpEntities();
    if ( self.impulse == 203 )
        DumpLiveEntities();
    if (self.impulse == 255)
        QuadCheat ();
    if (dump_coord == 1)
        DumpCoordinates();
    self.impulse = 0;
};

/*
============
W_WeaponFrame

Called every frame so impulse events can be handled as well as possible
============
*/
void(vector xOrigin, float xHand) PlayerLightningGunEffect =
{
    makevectors(VRGetWeaponFireRot(xHand));
    traceline (xOrigin, xOrigin + v_forward*600, TRUE, self);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity (MSG_BROADCAST, self);
    WriteByte (MSG_BROADCAST, xHand); /* disambiguator */
    WriteCoord (MSG_BROADCAST, xOrigin_x);
    WriteCoord (MSG_BROADCAST, xOrigin_y);
    WriteCoord (MSG_BROADCAST, xOrigin_z);
    WriteCoord (MSG_BROADCAST, trace_endpos_x);
    WriteCoord (MSG_BROADCAST, trace_endpos_y);
    WriteCoord (MSG_BROADCAST, trace_endpos_z);
}

void() W_WeaponFrame =
{
    local vector org;

    if (self.button0)
    {
        // Lightning effect (per-frame)
        if (self.weapon == WID_LIGHTNING && self.ammo_cells > 0)
        {
            PlayerLightningGunEffect(self.muzzlepos, MAIN_HAND);
        }
    }

    if (time < self.attack_finished)
    {
        return;
    }

    // check for attack
    if (self.button0)
    {
        SuperDamageSound ();
        W_Attack ();
    }
};

/*
============
W_OffHandWeaponFrame

// TODO VR:
============
*/
void() W_OffHandWeaponFrame =
{
    local vector org;

    if (self.button3)
    {
        // Lightning effect (per-frame)
        if (self.weapon2 == WID_LIGHTNING && self.ammo_cells > 0)
        {
            PlayerLightningGunEffect(self.offmuzzlepos, OFF_HAND);
        }
    }

    if (time < self.offhand_attack_finished)
    {
        return;
    }

    // check for attack
    if (self.button3)
    {
        SuperDamageSound ();
        W_OffHandAttack ();
    }
};

float(float xHolster) getHolsterWeapon =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweapon0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweapon1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweapon2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweapon3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweapon4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweapon5;

    bprint("Invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

void(float xHolster, float v) setHolsterWeapon =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweapon0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweapon1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweapon2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweapon3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweapon4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweapon5 = v; return; }

    bprint("Invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

string(float xHolster) getHolsterWeaponModel =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  return self.holsterweaponmodel0;
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) return self.holsterweaponmodel1;
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       return self.holsterweaponmodel2;
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      return self.holsterweaponmodel3;
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     return self.holsterweaponmodel4;
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    return self.holsterweaponmodel5;

    bprint("Invalid holster ID: ");
    bprint(ftos(xHolster));
    bprint("\n");
}

void(float xHolster, string v) setHolsterWeaponModel =
{
    if(xHolster == QVR_HS_LEFT_SHOULDER_HOLSTER)  { self.holsterweaponmodel0 = v; return; }
    if(xHolster == QVR_HS_RIGHT_SHOULDER_HOLSTER) { self.holsterweaponmodel1 = v; return; }
    if(xHolster == QVR_HS_LEFT_HIP_HOLSTER)       { self.holsterweaponmodel2 = v; return; }
    if(xHolster == QVR_HS_RIGHT_HIP_HOLSTER)      { self.holsterweaponmodel3 = v; return; }
    if(xHolster == QVR_HS_LEFT_UPPER_HOLSTER)     { self.holsterweaponmodel4 = v; return; }
    if(xHolster == QVR_HS_RIGHT_UPPER_HOLSTER)    { self.holsterweaponmodel5 = v; return; }

    bprint("Invalid holster ID: ");
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
    local float angle;
    angle = xAngle - xOffset;

    // Bring angle in range [-360, 0]
    if(angle > 0)
    {
        *xOutAngle = xOffset + angle - 360;
        angle = angle - 360;
    }
    else if(angle < -360)
    {
        *xOutAngle = xOffset + angle + 360;
        angle = angle + 360;
    }

    if (angle >= -360 && angle <= -270)
    {
        // All these offset calculations are needed because I can't read back
        // from `xOutAngle`...
        *xOutAngle = xOffset + angle + self.throwstabilize;
        if(xAngle - xOffset + self.throwstabilize > -270)
        {
            *xOutAngle = xOffset + -270;
        }
    }
    else if (angle > -270 && angle <= -180)
    {
        *xOutAngle = xOffset + angle - self.throwstabilize;
        if(xAngle - xOffset - self.throwstabilize < -270)
        {
            *xOutAngle = xOffset + -270;
        }
    }
    else if (angle > -180 && angle <= -90)
    {
        *xOutAngle = xOffset + angle + self.throwstabilize;
        if(xAngle - xOffset + self.throwstabilize > -90)
        {
            *xOutAngle = xOffset + -90;
        }
    }
    else if (angle > -90 && angle <= 0)
    {
        *xOutAngle = xOffset + angle - self.throwstabilize;
        if(xAngle - xOffset - self.throwstabilize < -90)
        {
            *xOutAngle = xOffset + -90;
        }
    }
    else
    {
        // TODO VR: still triggers sometimes
        bprint("uncovered angle case: '");
        bprint(ftos(angle));
        bprint("'\n");
    }

    if (self.throwstabilize > 0)
    {
        self.throwstabilize = self.throwstabilize - 0.3;

        if (self.throwstabilize < 0)
        {
            self.throwstabilize = 0;
        }
    }
}

void() wpnthrow_think =
{
    if (self.throwhit == TRUE)
    {
        if(self.throwstabilizedim == 0)
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

    self.nextthink = time + 0.02;
}

void() wpnthrow_touch =
{
    local float posDmgType;
    local float adjDmg;
    local vector dir;

    if (other == self.owner)
        return;

    if (other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if (pointcontents(self.origin) == CONTENT_SKY)
        return;

    // makevectors(self.angles);
    // traceline (self.origin - v_forward * 8, self.origin + v_forward * 8, FALSE, self);

    // hit something that bleeds
    if (self.throwhit == FALSE && other.takedamage)
    {
        sound (self, CHAN_OTHER, "fisthit.wav", 1, ATTN_NORM);

        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);

        // TODO VR: calculate based on mult and velocity
        adjDmg = 20 * PositionalDamageFactor(posDmgType);

        spawn_touchblood (adjDmg);
        T_Damage (other, self, self.owner, adjDmg);

        setsize (self, self.mins * 5, self.maxs * 5);
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        self.throwhit = TRUE;
        return;
    }

    // hit wall
    if (self.throwhit == FALSE)
    {
        sound (self, CHAN_OTHER, "player/axhit2.wav", 1, ATTN_NORM);
        particle2 (self.origin, '0 0 0', 0 /* bullet puff preset */, 24);

        setsize (self, self.mins * 5, self.maxs * 5);
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        self.throwhit = TRUE;
    }

    if (vlen(self.velocity) > 1)
    {
        dir = normalize(self.velocity);
        dir_z = 0;

        traceline (self.origin - dir * 2, self.origin + dir * 8, FALSE, self);
        if (trace_fraction != 1.0)
        {
            setorigin(self, trace_endpos - dir * 8);
        }

        dir = normalize(self.velocity);
        dir_x = 0;
        dir_y = 0;

        traceline (self.origin - dir * 2, self.origin + dir * 2, FALSE, self);
        if (trace_fraction != 1.0)
        {
            setorigin(self, trace_endpos - dir * 2);
            self.velocity = '0 0 0';
            self.avelocity = '0 0 0';
        }

    }
};

void(float* xOutEntWeapon) wpnthrow_handtouch_impl =
{
    sound (self, CHAN_OTHER, "weapons/pkup.wav", 1, ATTN_NORM);
    *xOutEntWeapon = self.weapon;
    remove(self);

    self = self.handtouch_ent;
    W_SetCurrentAmmo();
}

void() wpnthrow_handtouch =
{
    // bprint("handtouch: ");
    // bprint(ftos(self.handtouch_hand));
    // bprint("\n");

    if(self.handtouch_hand == OFF_HAND &&
       self.handtouch_ent.offhand_grabbing &&
       self.handtouch_ent.weapon2 == WID_FIST)
    {
        wpnthrow_handtouch_impl(&self.handtouch_ent.weapon2);
    }
    else if(self.handtouch_hand == MAIN_HAND &&
            self.handtouch_ent.mainhand_grabbing &&
            self.handtouch_ent.weapon == WID_FIST)
    {
        wpnthrow_handtouch_impl(&self.handtouch_ent.weapon);
    }
}

void(float xHand) VRHolsterHaptic =
{
    if(cvar("vr_holster_haptics") == 1)
    {
        VRGunHaptic(xHand, 0.1, 75, 0.02);
    }
}

void(float xHand,
     float xWeapon, float* xOutWeapon,
     float xGrabbing, float xAttackFinished, float xHotspot,
     float xOtherWeapon, float* xOutOtherWeapon, float xOtherAttackFinished, float xOtherGrabbing) DoHandImpl =
{
    local float tmp;
    local vector tmp_handavel;
    local vector tmp_bounds;

    if (xWeapon != WID_FIST && !xGrabbing && time >= xAttackFinished)
    {
        if(xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB)
        {
            // 1 and 2 ignored because you can only dual wield with no weapon out
            // TODO VR: what to do?

            sound (self, CHAN_BODY, "knight/sword2.wav", 0.7, ATTN_NORM);

            // TODO VR: throwing axe lol
            //if (xWeapon == WID_AXE)
            {
                newmis = spawn ();
                newmis.owner = self;
                newmis.movetype = MOVETYPE_TOSS;
                newmis.solid = SOLID_BBOX;

                newmis.throwhit = FALSE;
                newmis.throwstabilize = 8;
                newmis.throwstabilizedim = WeaponIdToThrowStabilizeDim(xWeapon);

                newmis.angles = VRGetHandRot(xHand);
                newmis.angles_x = newmis.angles_x * -1;

                newmis.touch = wpnthrow_touch;
                newmis.handtouch = wpnthrow_handtouch;

                newmis.classname = "thrown_weapon";

                makevectors(VRGetHandVel(xHand));
                tmp_handavel = VRGetHandAVel(xHand);
                newmis.avelocity = tmp_handavel_x * v_forward * 30
                                 + tmp_handavel_y * v_right * -30
                                 + tmp_handavel_z * v_up * -30;

                newmis.think = wpnthrow_think;
                newmis.nextthink = time + 0.02;
                newmis.weapon = xWeapon;

                tmp = WeaponIdToThrowBounds(xWeapon);
                tmp_bounds_x = tmp;
                tmp_bounds_y = tmp;
                tmp_bounds_z = tmp;

                setmodel (newmis, WeaponIdToModel(xWeapon));
                setsize (newmis, -tmp_bounds, tmp_bounds);
                setorigin (newmis, VRGetHandPos(xHand));

                newmis.velocity = VRGetHandVel(xHand) * 120 * WeaponIdToThrowMult(xWeapon)
                                + self.velocity;
            }

            *xOutWeapon = WID_FIST;
            W_SetCurrentAmmo();
        }
        else if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) == WID_FIST)
            {
                sound (self, CHAN_BODY, "weapons/holster1.wav", 1, ATTN_NORM);

                setHolsterWeapon(xHotspot, xWeapon);
                setHolsterWeaponModel(xHotspot, WeaponIdToModel(xWeapon));
                *xOutWeapon = WID_FIST;
                W_SetCurrentAmmo();
            }
        }
        else if(xHotspot == QVR_HS_HAND_SWITCH)
        {
            if(xOtherGrabbing && xOtherWeapon == WID_FIST && time >= xOtherAttackFinished)
            {
                sound (self, CHAN_BODY, "knight/sword2.wav", 0.7, ATTN_NORM);

                *xOutOtherWeapon = xWeapon;
                *xOutWeapon = WID_FIST;

                W_SetCurrentAmmo();
            }
        }
    }
    else if (xWeapon != WID_FIST && xGrabbing && time >= xAttackFinished)
    {
        // TODO VR: cvar to enable/disable
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) == WID_FIST)
            {
                VRHolsterHaptic(xHand);
            }
        }
    }
    else if (xWeapon == WID_FIST && xGrabbing)
    {
        if(xHotspot == QVR_HS_NONE)
        {
            // TODO VR: what to do?
        }
        else if(xHotspot == QVR_HS_OFFHAND_2H_GRAB)
        {
            // TODO VR: what to do?
        }
        else if(xHotspot == QVR_HS_MAINHAND_2H_GRAB)
        {
            // TODO VR: what to do?
        }
        else if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) != WID_FIST)
            {
                sound (self, CHAN_BODY, "weapons/holster0.wav", 1, ATTN_NORM);

                *xOutWeapon = getHolsterWeapon(xHotspot);
                setHolsterWeapon(xHotspot, WID_FIST);
                setHolsterWeaponModel(xHotspot, WeaponIdToModel(WID_FIST));
                W_SetCurrentAmmo();
            }
        }
    }
    else if (xWeapon == WID_FIST && !xGrabbing)
    {
        // TODO VR: cvar to enable/disable
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) != WID_FIST)
            {
                VRHolsterHaptic(xHand);
            }
        }
    }
}

// TODO VR:
void() W_Frame =
{
    if (self.impulse)
    {
        ImpulseCommands ();
    }

    // TODO VR:
    DoHandImpl(OFF_HAND,
               self.weapon2, &self.weapon2,
               self.offhand_grabbing, self.offhand_attack_finished, self.offhand_hotspot,
               self.weapon, &self.weapon, self.attack_finished, self.mainhand_grabbing);

    DoHandImpl(MAIN_HAND,
               self.weapon, &self.weapon,
               self.mainhand_grabbing, self.attack_finished, self.mainhand_hotspot,
               self.weapon2, &self.weapon2, self.offhand_attack_finished, self.offhand_grabbing);

    // bprint(ftos(self.offhand_grabbing));
    // bprint(" | ");
    // bprint(ftos(self.mainhand_grabbing));
    // bprint(" ||| ");
    // bprint(ftos(self.offhand_hotspot));
    // bprint(" | ");
    // bprint(ftos(self.mainhand_hotspot));
    // bprint(" ||| ");
    // bprint(ftos(self.weapon2));
    // bprint(" | ");
    // bprint(ftos(self.weapon));
    // bprint("\n");
}

/*
========
SuperDamageSound

Plays sound if needed
========
*/
void() SuperDamageSound =
{
    if (self.super_damage_finished > time)
    {
        if (self.super_sound < time)
        {
            self.super_sound = time + 1;
            sound (self, CHAN_BODY, "items/damage3.wav", 1, ATTN_NORM);
        }
    }
    return;
};


// TODO VR: stop switching weapons automatically, it makes no sense anymore.
// Also dont automatically switch a weapon if ammo is out, player might want to
// melee or throw it away. Consider spawning previously obtained weapons along
// side ammo boxes or from enemies, so that they can be recovered if player
// throws them away.

// TODO VR: remove backpack spinning animation. Consider limiting weapons to
// 6 (4 holsters + hands) to balance gameplay and dual wielding.

// TODO VR: not sure why weapons cannot be juggled. too small size? problems
// with handtouch?

// TODO VR: still need to fix oldwater...

