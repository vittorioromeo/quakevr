
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
    if(VR_EnemyGun_Counter(xHand)) // a grunt's shotgun, an enforcer's rifle: its clip (vr_enemyguns.qc)
    {
        return;
    }
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

// A weapon's base damage (VR Settings > Combat > Weapon Damage; vr_dmg_*): the setting's for a shot or a projectile
// `xOwner` fired as a player (or a bot); `xStock` (id's, or the mission pack's) for a monster's or a trap's.
float(entity xOwner, float xCvarh, float xStock) VR_WpnDmg =
{
    if(xOwner.flags & FL_CLIENT)
    {
        return cvar_hget(xCvarh);
    }

    return xStock;
};

// And as a multiplier of the stock damage: for a weapon whose setting also scales its other numbers (a rocket's blast
// over its direct hit, a laser's rare harder bolt).
float(entity xOwner, float xCvarh, float xStock) VR_WpnDmgScale =
{
    return VR_WpnDmg(xOwner, xCvarh, xStock) / xStock;
};

// Quad Damage's extra for melee (vr_quad_melee_damage), on top of the x4 T_Damage gives every damage under Quad.
float() VRMeleeDmgQuadMult =
{
    if(self.super_damage_finished > time)
    {
        return cvar_hget(cvarh_vr_quad_melee_damage);
    }

    return 1;
}

// Melee: a part of what a hand holds swept into something at vr_melee_speed or more (vr_melee.qc; PlayerVRMeleeImpl
// in client.qc), in any direction. The damage is the weapon's base times the blow's strength (VR_Melee_Decide: the
// striking part's speed and the kind of blow; 1 at twice vr_melee_speed, at most VR_MELEE_STRONGEST), a counter's
// after a parry (VR_Blow_Counter; vr_counter_damage), short of stamina less (VR_Blow_Exhaust; vr_strike_stamina), and
// what the hand holds: its own Melee Damage x times the weight's curve (VR_Weight_MeleeMult: Weapon Weights, Held Object
// Weights; vr_weight_damage_*).

float(float xHand, float xBase) VRMeleeDmg =
{
    if(!VRHandsTracked(self) || !self.ishuman)
    {
        return 20;
    }

    return xBase * VR_Blow_Strength(xHand) * VR_Blow_Counter(xHand) * VR_Blow_Exhaust(xHand) *
           VR_Weight_MeleeMult(self, xHand) * cvar_hget(cvarh_vr_melee_dmg_multiplier) * VRMeleeDmgQuadMult();
}

float(float v) VRMeleeRange =
{
    float mult;

    mult = 1.10;
    if(self.super_damage_finished > time)
    {
        mult = mult * cvar_hget(cvarh_vr_quad_melee_range); // Quad's extra reach (vr_quad_melee_range)
    }

    if(!VRHandsTracked(self) || !self.ishuman)
    {
        mult = mult * 3.5;
    }

    return v * cvar_hget(cvarh_vr_melee_range_multiplier) * mult;
}

// Stronger and longer for harder blows (`power`: the blow's strength, about 0.5 .. 2).
void(float hand, float power) VRMeleeHaptic =
{
    if(!self.ishuman)
    {
        return;
    }

    power = max(0.5, power);
    float amplitude = min(1.0, 0.35 + 0.25 * power);
    float xduration = min(0.9, 0.25 + 0.15 * power);

    VRGunHaptic(hand, xduration, 75.0, amplitude);
}

void (float xHand) VRMeleeHapticByHand =
{
    VRMeleeHaptic(xHand, VR_Blow_Strength(xHand));
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
    if(c == "vr_dummy")                 { xHeadForward = 3;  xHeadUp = 23; xHeadRadius = 7.2; return TRUE; } // a grunt
    return FALSE;
};

float VR_HEAD_MARGIN = 3.5; // units: the head spheres are fitted to the top vertices' middle; a head's surface lies up to 3.5 beyond (an ogre's back of the head)

// Where a shot entering `xTarget` at `xEndpos` (a point on its box, from a trace) along
// `xDirection` hits: 1 the head, 2 an extremity (far from its middle), 3 the legs, 0 the body; -1
// when positional damage does not apply (vr_positional_damage 0, a target without a head).
// The shot is a ray through the target: the head is a sphere turned with the target's facing
// (heads sit forward of the origin: a shambler's by 14 units), so hits are found the same whatever
// the angle, rather than by pushing the entry point a fixed depth into the box.
float PositionalRegion(entity xTarget, vector xEndpos, vector xDirection) =
{
    // A monster's head thrown off (vr_gib) keeps its classname, not its head.
    if(cvar_hget(cvarh_vr_positional_damage) == 0 || (xTarget.vr_gib && !(xTarget.flags & FL_CLIENT)) ||
       !PositionalHead(xTarget))
    {
        return -1;
    }

    vector d = normalize(xDirection);
    if(d == '0 0 0')
    {
        return -1;
    }

    float yaw = xTarget.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vector head = xTarget.origin + facing * xHeadForward + '0 0 1' * xHeadUp;

    // Precise hits (vr_hit_precise): the shot struck the model as drawn (at xEndpos, within its tolerance of it). Where
    // that place is on the model standing (hitmodel_rest: the same place of the same triangle in its stand frame, at its
    // origin, turned with its yaw) decides: in the head sphere (and VR_HEAD_MARGIN: the sphere is fitted to the head's
    // middle), far out from its middle, below its origin. A monster crouched, in pain or lying dead keeps its head.
    if(hitmodel_target(xTarget))
    {
        vector rest = hitmodel_rest(xTarget, xEndpos);
        if(vlen(rest - head) < xHeadRadius + VR_HEAD_MARGIN)
        {
            return 1; // headshot
        }
        // Out to its sides (not forwards: a grunt's hands and gun are in front of its chest, a front shot's body).
        vector restSide;
        restSide_x = -facing_y;
        restSide_y = facing_x;
        restSide_z = 0;
        if(fabs((rest - xTarget.origin) * restSide) > (xTarget.maxs_x - xTarget.mins_x) * 0.35)
        {
            return 2; // extremities
        }
        if(rest_z < xTarget.origin_z)
        {
            return 3; // legs
        }
        return 0; // standard
    }

    // The ray's closest approach to the head, not behind where it entered.
    float t = max(0, (head - xEndpos) * d);
    if(vlen(head - (xEndpos + d * t)) < xHeadRadius)
    {
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

// PositionalRegion for a shot's damage (0, the body, when it does not apply). A headshot also
// gives its feedback (PositionalHeadshotFeedback). The answer is kept for the training dummy's
// report (vr_dummy.qc).
float PositionalDamage(entity xTarget, vector xEndpos, vector xDirection) =
{
    float region = PositionalRegion(xTarget, xEndpos, xDirection);
    if(region == 1)
    {
        PositionalHeadshotFeedback(xTarget);
    }

    vr_hitpart = region;
    vr_hitpart_target = xTarget;
    vr_hitpart_time = time;
    vr_hitpart_pos = xEndpos;
    return max(0, region);
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
    if(shooter.classname != "player" && self.classname == "hiplaser")
    {
        shooter = self.lastvictim; // a laser cannon's bolt: its owner is cleared as it touches (it may bounce back)
    }
    if(shooter.classname != "player" || shooter == xTarget)
    {
        return;
    }

    sound(shooter, CHAN_AUTO, "vr/headshot.wav", min(1, v), ATTN_NORM);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("headshot sound: %s by %s\n", xTarget.classname, self.classname));
    }
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
    VR_TouchTrace(spot1, spot2);  // see through other monsters
    self.origin = trace_endpos;
    VR_ShotPush_Laser(other, self.origin, oldvel); // VR: a prop it hits pushed (vr_shot_push)

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

// VR: a shot from a muzzle inside what it shoots. Pushed into a grunt's head, a gun is drawn stopped at the head's
// surface (vr_model_collide) while its tracked muzzle, where shots start, is past it, inside the grunt's box; and a
// Quake trace that starts inside a box doesn't report it (the shot went through). So a hand's shots are traced, and its
// projectiles placed, from behind the muzzle along the shot's own line: from where that line passes the hand (the
// grip). What lies between there and the muzzle (along the weapon's own length) is hit as what lies past it is; a wall
// there stops the shot there (a gun poked through a thin wall doesn't shoot from behind it).
//
// The point behind `xFrom` (a muzzle, or a point beside it a projectile starts from) on the line along `xDir`, level
// with the hand: `xFrom` itself if the hand isn't behind it or isn't near (a bot's). A wall between the hand and that
// point: the hand.
vector(float xHand, vector xFrom, vector xDir) VR_ShotBackPoint =
{
    vector hand = VRGetHandPos(xHand);
    float back = (xFrom - hand) * xDir;
    if(back <= 0 || vlen(xFrom - hand) > 96)
    {
        return xFrom;
    }
    vector at = xFrom - xDir * back;
    traceline(hand, at, TRUE, self);
    if(trace_fraction < 1 || trace_startsolid)
    {
        return hand;
    }
    return at;
};

// A hand's projectile, just placed at its muzzle: if anything solid lies along the weapon between the grip and there (a
// monster the muzzle is inside, a wall it poked through), it starts behind it instead. A monster (or anything that takes
// damage) is met at its surface this frame, as a shot meets it (VR_ShotImpacts): flown from behind, a missile starting
// that close would begin inside the monster's box grown for missiles (15 units), and the physics would report the touch
// only after a whole frame's flight, past it (its hit placed beyond it). The globals v_forward, v_right and v_up are left
// as they were (the trace ones are not).
.vector vr_shot_to;  // pending (vr_shot_pending, chained by vr_shot_next): the point it left the muzzle at,
.float vr_shot_time; // this frame
.entity vr_shot_next;
entity vr_shot_pending;

// A missile's precise hits (MOVE_HITMODEL and its class, from .vr_hitclass; builtins.qc), for the traces that stand in
// for its flight: the monsters' models, as its moves meet them.
float(entity xMissile) VR_HitModelFlags =
{
    if(xMissile.vr_hitclass < 0)
    {
        return 0;
    }
    return MOVE_HITMODEL + 1024 * xMissile.vr_hitclass;
};

void(entity xMissile, float xHand) VR_ShotPlace =
{
    vector at = xMissile.origin;
    vector from = VR_ShotBackPoint(xHand, at, VRForwardOf(VRGetWeaponFireRot(xHand)));
    if(from == at)
    {
        return;
    }
    traceline(from, at, VR_HitModelFlags(xMissile), self);
    if(trace_fraction < 1 || (trace_startsolid && trace_ent != world))
    {
        setorigin(xMissile, from);
        if(trace_ent != world && trace_ent.takedamage)
        {
            xMissile.vr_shot_to = at;
            xMissile.vr_shot_time = time;
            xMissile.vr_shot_next = vr_shot_pending;
            vr_shot_pending = xMissile;
        }
    }
};

// The projectiles placed this frame with something that takes damage along the weapon (VR_ShotPlace) touch what they
// meet there now (the first one's hit may have shoved it), where they meet it: at the end of the player's frame
// (PlayerPostThink), after its weapons fired and before the projectiles move.
void() VR_ShotImpacts =
{
    entity player = self;
    entity oldOther = other;
    entity m = vr_shot_pending;
    vr_shot_pending = world;
    while(m != world)
    {
        entity next = m.vr_shot_next;
        m.vr_shot_next = world;
        if(m.vr_shot_time == time && m.touch && m.solid != SOLID_NOT)
        {
            traceline(m.origin, m.vr_shot_to, VR_HitModelFlags(m), player);
            if((trace_fraction < 1 || trace_startsolid) && trace_ent != world && trace_ent.takedamage &&
               trace_ent.solid != SOLID_NOT)
            {
                entity target = trace_ent;
                if(trace_fraction < 1)
                {
                    setorigin(m, trace_endpos - normalize(m.vr_shot_to - m.origin) * 0.25); // just before it
                }
                self = m;
                other = target;
                self.touch();
            }
        }
        m = next;
    }
    self = player;
    other = oldOther;
};

// VR: a projectile's touch traces back along its flight to find where it struck. Started inside another box (the
// shooter's own, a missile fired point blank: VR_ShotImpacts; a monster beside the one struck), that trace reports the
// box it started in, not what was struck: traced again from the projectile itself.
void(vector xFrom, vector xTo) VR_TouchTrace =
{
    traceline(xFrom, xTo, VR_HitModelFlags(self), self);
    if(trace_startsolid && trace_ent != other && trace_ent != world)
    {
        traceline(self.origin, xTo, VR_HitModelFlags(self), self);
    }
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
    newmis.dmg = VR_WpnDmg(self, cvarh_vr_dmg_laser, 18);
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
        VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
        org = org - (2*aofs*v_right);
        newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
        VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    }
    else if(stat == 1)
    {
        delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 1);

        org = org + (ofs*(v_up * 0.4));

        if(random() < 0.1)
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 1);
            VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
            newmis.dmg = 25 * VR_WpnDmgScale(self, cvarh_vr_dmg_laser, 18);
        }
        else
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
            VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
        }
    }

    self.punchangle_x = -1;
    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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

    traceline(p1, p2, MOVE_HITMODEL, self);
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

    traceline(p1 + f, p2 + f, MOVE_HITMODEL, self);
//JIM
    if(trace_ent != e1 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished)
    {
        particle (trace_endpos, '0 0 100', 225, damage*4);
        T_Damage(trace_ent, from, from, damage);
    }
    e2 = trace_ent;

    traceline(p1 - f, p2 - f, MOVE_HITMODEL, self);
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
   dam = VR_WpnDmg(self, cvarh_vr_dmg_mjolnir_lightning, 80); // (self: the player now)
   if(oldstate!=0)
      dam = dam * (30 / 80);
   VR_SetHitKind(QVR_HITKIND_SHOT, -1, trace_endpos, normalize(trace_endpos - org));
   vr_hitweapon = "Mjolnir's lightning";
   if(dot>0.3)
      HIP_HammerDamage(org, trace_endpos, self, dam);
   else
      HIP_HammerDamage(org, trace_endpos, self, dam*0.5);
   VR_ClearHitKind();
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

float(float xHand, vector xAt, float xCells, float xWeapon) VR_LGWater_ShockAt;

// xAt: where it struck (its head).
void(float xHand, vector xAt) HIP_FireMjolnirLightning =
{
    float cells;

    // VR: in water, the lightning gun's shock (vr_lg_water; ROUND21.md, "Lightning gun in water"): its head or the hand
    // in a liquid, or the body waist deep. Its 15 cells spent.
    if(cvar_hget(cvarh_vr_lg_water) && VR_LGWater_ShockAt(xHand, xAt, 15, IT_MJOLNIR))
    {
        return;
    }

    // explode if under water (Quake's, vr_lg_water 0)
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

// A swing (tagged by PlayerVRMeleeImpl) struck at trace_endpos, going along `xDir`: kept for the
// training dummy's report (vr_dummy.qc).
void(vector xDir) VRMeleeHitAt =
{
    if(vr_hitkind == QVR_HITKIND_MELEE)
    {
        vr_hitpos = trace_endpos;
        vr_hitdir = xDir;
    }
};

// Whether the last traceline of a swing struck something that bleeds, or (`xAnything`) anything. A
// line starting inside a monster (the fist already in it)
// strikes it at `xAt`.
float(vector xAt, float xAnything) VRMeleeTraceHit =
{
    if(trace_fraction == 1.0 && trace_startsolid && trace_ent != world && trace_ent.takedamage)
    {
        trace_fraction = 0;
        trace_endpos = xAt;
        return TRUE;
    }
    return trace_fraction != 1.0 && (xAnything || trace_ent.takedamage);
};

// What a swing of `xHand` hits. Tracked hands: what the blow's striking points passed through in
// its stroke (VR_Blow_Contact: swept frame to frame, vr_juice.qc, "Striking points"). Otherwise (a
// flat-screen swing): ahead of the hand, and of the weapon's end, along the hand's motion. Things
// that bleed first. Sets the trace_ globals, and v_forward to the motion.
float(float xHand, float xRange, float xMuzzleRange) VRDoMeleeAttackTracelineFor =
{
    if(VRHandsTracked(self) && self.ishuman)
    {
        if(!VR_Blow_Contact(xHand))
        {
            return FALSE;
        }
        VRMeleeHitAt(v_forward);
        return TRUE;
    }

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
        traceline(handPos - dir * 6, handPos + dir * range, MOVE_HITMODEL_MELEE, self);
        if(VRMeleeTraceHit(handPos, anything))
        {
            v_forward = dir;
            VRMeleeHitAt(dir);
            return TRUE;
        }

        traceline(muzzlePos - dir * 6, muzzlePos + dir * muzzleRange, MOVE_HITMODEL_MELEE, self);
        if(VRMeleeTraceHit(muzzlePos, anything))
        {
            v_forward = dir;
            VRMeleeHitAt(dir);
            return TRUE;
        }
    }

    v_forward = dir;
    return FALSE;
}

void(float xHand) HIP_FireMjolnir =
{
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_mjolnir));

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
            HIP_FireMjolnirLightning(xHand, org);
            VRSetAttackFinished(xHand, time + 0.5);
        }

        if(trace_ent.classname == "monster_zombie")
        {
            xdmg *= 1.25;
        }

        trace_ent.axhitme = 1;
        SpawnBlood (org, v_forward, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
        if(vr_melee_blunt && VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound(""), 1, ATTN_NORM); // the head's hit has none
        }
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    else
    {
        // hit wall
        if(trace_fraction != 1.0)
        {
            if(time >= VRGetAttackFinished(xHand) && self.ammo_cells >= 15)
            {
                HIP_FireMjolnirLightning(xHand, org);
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
    precache_sound("vr/grapple_reel.wav"); // the reel winding in (vr_grapple.qc)
    precache_sound("vr/grapple_unreel.wav"); // the unreel paying out (vr_grapple.qc)
    precache_sound("vr/grapple_taut.wav"); // the rope snapping taut
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

    xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_axe));

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
            sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound("fisthit.wav"), 1.0, ATTN_NORM);
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

    xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_gun_bash));

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
            sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound("fisthit.wav"), 1.0, ATTN_NORM);
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
            sound(self, VRGetGunChannel(xHand), VR_Melee_HitSound("knight/sword2.wav"), 1.0, ATTN_NORM);
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

// The ogres' chainsaw (WID_CHAINSAW) swung: a melee weapon's blow (the axe's), its bar striking (vr_melee.qc
// VR_MTHING_SAW); its weight makes it land harder (VR_Weight_MeleeMult). Its sounds go on a channel of their own: the
// engine's sound is on the weapon's (vr_chainsaw.qc). A running chain also cuts what the bar is in (VR_Chainsaw_Cut).
void(float xHand) W_ChainsawMelee =
{
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_chainsaw_swing));
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
            sound(self, CHAN_AUTO, VR_Melee_HitSound("fisthit.wav"), 1.0, ATTN_NORM);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    // A wall: a clang and sparks.
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        sound(self, CHAN_AUTO, "player/axhit2.wav", 1, ATTN_NORM);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    particle2(org, trace_plane_normal * 60, QVR_PARTICLE_PRESET_SPARKS, 8);
};

// A knight or hell knight always drops its sword when it dies (gibbed or not; a statue has none): a weapon lying
// where its hand was.
void(float xHellKnight) VR_DropKnightSword =
{
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
    float xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_fist));
    float playedsound = FALSE;

    // A wall torch in hand is a club (vr_walltorch.qc).
    if(VR_WallTorch_Melee(xHand))
    {
        return;
    }

    // A box, a rock or a brick in hand is an improvised weapon (vr_carry.qc VR_Carry_PunchMult). Tracked hands have it in
    // their blow's strength already (VR_Melee_Decide), which VRMeleeDmg multiplies by: not twice.
    entity held = VR_Carry_Held(self, xHand);
    if(held != world && (!VRHandsTracked(self) || !self.ishuman))
    {
        xdmg = xdmg * VR_Carry_PunchMult(self, xHand);
    }

    // (A straight punch's vr_melee_punch_mult is in the blow's strength: VR_Blow_Weight.)
    if(!VRDoMeleeAttackTracelineFor(xHand, 16, 8))
    {
        return;
    }

    org = trace_endpos - v_forward*4;

    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        sound(self, VRGetGunChannel(xHand), VR_Debris_Is(held) ? VR_Debris_Sound(held) : "fisthit.wav", 1.0, ATTN_NORM);
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
    // VR: what it touched is wood (a crate, an explosive box): splinters, not blood (vr_crates.qc).
    if(VR_Crate_Wooden(other))
    {
        VR_Crate_Splinters(self.origin + vel * 0.01, vel, damage);
        return;
    }
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

// VR: the pellets on multi_ent, by where they struck (PositionalDamage), for the training dummy's
// report (vr_dummy.qc).
float multi_pellets;
float multi_pellets_head;
float multi_pellets_limb;
float multi_pellets_leg;

void() ClearMultiDamagePellets =
{
    multi_pellets = 0;
    multi_pellets_head = 0;
    multi_pellets_limb = 0;
    multi_pellets_leg = 0;
};

void() ClearMultiDamage =
{
    multi_ent = world;
    multi_damage = 0;
    ClearMultiDamagePellets();
};

void() ApplyMultiDamage =
{
    if(!multi_ent)
        return;

    vr_hitpellets = multi_pellets;
    vr_hitpellets_head = multi_pellets_head;
    vr_hitpellets_limb = multi_pellets_limb;
    vr_hitpellets_leg = multi_pellets_leg;
    T_Damage(multi_ent, self, self, multi_damage);
    vr_hitpellets = 0;
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
        ClearMultiDamagePellets();
    }
    else
        multi_damage = multi_damage + damage;

    multi_pellets = multi_pellets + 1;
    if(vr_hitpart_target == hit && vr_hitpart_time == time)
    {
        if(vr_hitpart == 1)      multi_pellets_head = multi_pellets_head + 1;
        else if(vr_hitpart == 2) multi_pellets_limb = multi_pellets_limb + 1;
        else if(vr_hitpart == 3) multi_pellets_leg = multi_pellets_leg + 1;
    }

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
// VR: shots push the loose props they hit (vr_shot_push; ROUND21.md, "Shots push props"). A Box3D prop a shot's trace
// meets (an explosive box, a gib) or passes through (a health box, a weapon lying about: traces and nails go through the
// pickups) is pushed where the shot goes in, as a thing of the kind's push (N s) over vr_shot_push_speed (kg) meeting it
// at that speed (physicspush's mass): a heavy box takes about the push's momentum and barely moves, a light thing flies at
// most that fast. Hit high, a box tips over. Explosions push props already (physicsblast).
float QVR_SHOTPUSH_PELLET = 1;
float QVR_SHOTPUSH_NAIL = 2;
float QVR_SHOTPUSH_SUPERNAIL = 3;
float QVR_SHOTPUSH_LIGHTNING = 4;

.float vr_shot_push_kind; // a nail's or laser's (VR_ShotPush_Launch): what it pushes with; 0: nothing (it has pushed)
.vector vr_shot_from;     // and where it was when it last looked for the props it flew through

// The kind's push (N s), times the global multiplier.
float(float xKind) VR_ShotPush_Momentum =
{
    float push = cvar_hget(cvarh_vr_shot_push);
    if(xKind == QVR_SHOTPUSH_PELLET)    { return push * cvar_hget(cvarh_vr_shot_push_pellet); }
    if(xKind == QVR_SHOTPUSH_NAIL)      { return push * cvar_hget(cvarh_vr_shot_push_nail); }
    if(xKind == QVR_SHOTPUSH_SUPERNAIL) { return push * cvar_hget(cvarh_vr_shot_push_supernail); }
    if(xKind == QVR_SHOTPUSH_LIGHTNING) { return push * cvar_hget(cvarh_vr_shot_push_lightning); }
    return 0;
};

// A shot from `xFrom` along `xDir` that stopped at `xAt` on `xHit` (world: a wall, or nothing): `xHit` pushed there if it
// is a prop, else the first prop it passed through on the way. `xScale`: times the kind's push. What it pushed, or world.
// The trace globals are kept.
entity(entity xHit, vector xFrom, vector xAt, vector xDir, float xKind, float xScale) VR_ShotPush =
{
    float momentum = VR_ShotPush_Momentum(xKind) * xScale;
    float speed = cvar_hget(cvarh_vr_shot_push_speed);
    if(momentum <= 0 || speed <= 0 || xDir == '0 0 0')
    {
        return world;
    }
    vector vel = normalize(xDir) * (speed * VRMetersToUnits());
    float mass = momentum / speed;
    entity pushed = world;
    if(xHit != world && physicspush(xHit, xAt, vel, mass))
    {
        pushed = xHit;
    }
    else
    {
        pushed = physicsshot(xFrom, xAt, vel, mass);
    }
    // vr_debug_shots: each push (and each lightning bolt's end, pushing or not).
    if((pushed != world || xKind == QVR_SHOTPUSH_LIGHTNING) && cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("shot push: kind %d, %.1f N s, from %v to %v: %s\n", xKind, momentum, xFrom, xAt,
            pushed == world ? "nothing" : pushed.classname));
    }
    return pushed;
};

// A flying nail or laser looks for the props it flew through since the last frame (think2, every frame), until it has
// pushed one.
void() VR_ShotPush_Fly =
{
    if(!self.vr_shot_push_kind)
    {
        return;
    }
    if(VR_ShotPush(world, self.vr_shot_from, self.origin, self.velocity, self.vr_shot_push_kind, 1) != world)
    {
        self.vr_shot_push_kind = 0;
        return;
    }
    self.vr_shot_from = self.origin;
    self.nextthink2 = time;
};

// A nail or laser just launched pushes as `xKind` (its touch calls VR_ShotPush_Touch).
void(entity xShot, float xKind) VR_ShotPush_Launch =
{
    xShot.vr_shot_push_kind = xKind;
    xShot.vr_shot_from = xShot.origin;
    xShot.think2 = VR_ShotPush_Fly;
    xShot.nextthink2 = time;
};

// The laser cannon's bolt meeting `xHit` at `xAt` going along `xDir` (it bounces: each prop it meets pushed as a nail).
void(entity xHit, vector xAt, vector xDir) VR_ShotPush_Laser =
{
    VR_ShotPush(xHit, xAt, xAt, xDir, QVR_SHOTPUSH_NAIL, 1);
};

// Its touch (self: the nail, other: what it met): that pushed, or a prop it flew through since the last frame.
void() VR_ShotPush_Touch =
{
    if(self.vr_shot_push_kind)
    {
        if(VR_ShotPush(other, self.vr_shot_from, self.origin, self.velocity, self.vr_shot_push_kind, 1) == world &&
           cvar_hget(cvarh_vr_debug_shots))
        {
            dprint(sprintf("shot push: kind %d met %s at %v: nothing pushed\n", self.vr_shot_push_kind, other.classname,
                self.origin));
        }
        self.vr_shot_push_kind = 0;
    }
};

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
        if(AddMultiDamage(trace_ent, damage) && trace_ent.classname != "hook") // (a grappling hook shot off: no blood)
        {
            if(VR_Crate_Wooden(trace_ent))
            {
                VR_Crate_Splinters(org, vel * 0.2, damage); // VR: wood (a crate, an explosive box): splinters
            }
            else
            {
                SpawnBlood (org, vel*0.2, damage);
                VR_Wound_Pellet(trace_ent, trace_endpos, dir, damage); // VR: painted on its model (vr_wounds.qc)
            }
            bleed = TRUE;
        }
    }

    if(!bleed)
    {
        BroadcastGunshotEffect(org);
    }
};


// VR: a shot crossing the surface of water, slime or lava splashes there, with a plip (vr_water_splash,
// vr_water_sounds). The trace globals stay as the shot's.
void(vector xFrom, vector xTo) VR_WaterShotSplash =
{
    local vector at;
    at = liquidentry(xFrom, xTo);
    if(at != xFrom)
        watersplash(at, normalize(xTo - xFrom), 4, QVR_SPLASH_SHOT);
};

/*
================
FireBullets

Used by shotgun, and super shotgun
Go to the trouble of combining multiple pellets into a single damage call.
================
*/
// VR: `xRange` units from `src`; `xStartHits`: a pellet whose trace starts inside a monster's (or anything damageable's)
// box hits it there (a trace doesn't report the box it starts in): a hand's shots, traced from its grip (FireBullets).
void(float shotcount, vector dir, vector spread, vector src, float xRange, float xStartHits) FireBulletsImpl =
{
    vector direction;
    float adjDmg;
    float posDmgType;

    makevectors(vectoangles(dir));

    // vr_debug_shots: what the pellets hit.
    float debugShots = cvar_hget(cvarh_vr_debug_shots);
    float hitsTarget = 0, hitsHead = 0, hitsWorld = 0, misses = 0;
    string targetName = "";

    ClearMultiDamage();
    while(shotcount > 0)
    {
        direction = dir + crandom()*spread_x*v_right + crandom()*spread_y*v_up;

        traceline(src, src + direction*xRange, MOVE_HITGIBS | MOVE_HITMODEL, self); // VR: gibs and heads too (vr_gib_destroy), monsters' models (vr_hit_precise),
        if(xStartHits && trace_startsolid && trace_ent != world && trace_ent.takedamage)
        {
            trace_fraction = 0; // started inside it: hit there
            trace_endpos = src;
            trace_plane_normal = -direction;
        }
        tracer(self, vr_tracer_hand, vr_tracer_from, trace_endpos); // a bullet tracer, from the muzzle (vr_weaponfx.cpp)
        if(!(self.flags & FL_CLIENT))
        {
            VR_Crate_MonsterShot(src, src + direction*xRange); // a crate held up in the way
        }
        VR_ShotPush(trace_ent, src, trace_endpos, direction, QVR_SHOTPUSH_PELLET, 1); // the prop it hits or goes through
        VR_Gib_TraceHeld(src, src + direction*xRange);               // and one held in the other hand
        VR_WaterShotSplash(src, trace_endpos);                        // a splash where it goes into water
        if(debugShots)
        {
            if(trace_fraction == 1.0) { misses++; }
            else if(trace_ent == world) { hitsWorld++; }
            else { hitsTarget++; targetName = trace_ent.classname; }
        }
        if(trace_fraction != 1.0)
        //JIM
        {
            posDmgType = PositionalDamage(trace_ent, trace_endpos, direction);
            if(debugShots && posDmgType == 1) { hitsHead++; }
            adjDmg = (vr_pellet_damage ? vr_pellet_damage : 4) * PositionalDamageFactor(posDmgType);

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
    if(debugShots)
    {
        dprint(sprintf("shot: from %v dir %v: %d on %s (%d head), %d world, %d nothing\n", src, dir, hitsTarget,
            targetName, hitsHead, hitsWorld, misses));
    }
};

void(float xHand, float shotcount, vector dir, vector spread) FireBullets =
{
    if(VRGet2HAiming(self))
    {
        spread *= cvar_hget(cvarh_vr_2h_spread_reduction);
    }

    // From behind the muzzle, along the shot's line (VR_ShotBackPoint): what the muzzle is inside is hit too.
    vector muzzle = VRGetMuzzlePos(xHand);
    vector src = VR_ShotBackPoint(xHand, muzzle, dir);

    VR_SetHitKind(QVR_HITKIND_SHOT, xHand, '0 0 0', dir);
    vr_tracer_hand = xHand;
    vr_tracer_from = muzzle;
    FireBulletsImpl(shotcount, dir, spread, src, 2048 + vlen(muzzle - src), TRUE);
    VR_ClearHitKind();
}

void(float shotcount, vector dir, vector spread) EnemyFireBullets =
{
    vector    src;

    src = self.origin + v_forward*10;
    src_z = self.absmin_z + self.size_z * 0.7;
    dir = VR_Crate_ShotAim(src, dir, self.enemy); // no higher than a player crouched really stands

    weaponfired(self, -1); // VR: its gun's muzzle flash (a grunt's: vr_weaponfx.cpp)
    vr_tracer_hand = -1;
    vr_tracer_from = src; // (the clients start a grunt's tracers at its gun's muzzle)
    FireBulletsImpl(shotcount, dir, spread, src, 2048, FALSE);
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

    vr_pellet_damage = cvar_hget(cvarh_vr_dmg_shotgun); // (Weapon Damage)
    FireBullets(xHand, 6, dir, '0.01 0.01 0');
    vr_pellet_damage = 0;

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.25, 70, 0.75);

    // The spent shell out of the port as the gun cycles (vr_shells.cpp). The super shotgun's (one
    // barrel fired) stays in until it is broken open to reload -- unless it reloads itself.
    if(VRGetWeapon(xHand) == WID_SHOTGUN)
    {
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, 1, 0.22, 0);
    }
    else if(VRGetWeapon(xHand) == WID_SUPER_SHOTGUN && !VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, 1, 0.45, 0);
    }

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
    vr_pellet_damage = cvar_hget(cvarh_vr_dmg_super_shotgun); // (Weapon Damage)
    FireBullets(xHand, 14, dir, '0.035 0.025 0');
    vr_pellet_damage = 0;

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.55, 75, 1.0);

    // Without reloading it reloads itself: the two spent shells come out as it would open.
    if(!VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, 2, 0.45, 0);
    }

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

    float rocketScale = VR_WpnDmgScale(self.owner, cvarh_vr_dmg_rocket, 100); // (Weapon Damage)
    damg = (100 + random()*20) * rocketScale;

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
    T_RadiusDamage(self, self.owner, 120 * rocketScale, other);

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
    VR_ShotPlace(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)

    // HIPNOTIC
    //MED 11/09/96

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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

    traceline(p1, p2, MOVE_HITGIBS | MOVE_HITMODEL, self); // VR: gibs and heads too (vr_gib_destroy), monsters' models (vr_hit_precise),
    // VR: the prop the beam hits or goes through, pushed (a player's bolt: 30; a shambler's by its damage).
    VR_ShotPush(trace_ent, p1, trace_endpos, p2 - p1, QVR_SHOTPUSH_LIGHTNING, damage / 30);
    VR_Gib_TraceHeld(p1, p2);              // and one held in the other hand
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

    traceline(p1 + f, p2 + f, MOVE_HITGIBS | MOVE_HITMODEL, self);
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

    traceline(p1 - f, p2 - f, MOVE_HITGIBS | MOVE_HITMODEL, self);
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


// VR: the lightning gun in water (vr_lg_water; ROUND21.md, "Lightning gun in water"). Quake's discharge (every cell at
// once, 35 each, round you, with the body waist deep) is replaced. Fired with the body waist deep, or the hand or its
// muzzle in a liquid, it shocks you (vr_lg_water_self_damage): both hands drop what they hold (a weapon falls as a prop,
// a carried thing is let go; the holsters keep theirs), and the shock spreads through the water from the muzzle to what
// is in it (vr_lg_water_damage, fading out to vr_lg_water_radius). Fired from the open into water, each bolt electrifies
// the water round the point it goes in (vr_lg_water_tick_damage, fading out the same way). The engine draws them
// (watershock(); vr_shock.cpp).

float(float xContents) VR_LGWater_IsLiquid =
{
    return xContents <= CONTENT_WATER && xContents >= CONTENT_LAVA;
};

// Whether `e` is in a liquid: a player by its waterlevel (the feet in it), anything else by the bottom of its box or
// its middle.
float(entity e) VR_LGWater_InLiquid =
{
    if(e.flags & FL_CLIENT)
    {
        return e.waterlevel >= 1 && VR_LGWater_IsLiquid(e.watertype);
    }
    vector mid = (e.absmin + e.absmax) * 0.5;
    if(VR_LGWater_IsLiquid(pointcontents(mid)))
    {
        return TRUE;
    }
    mid_z = e.absmin_z + 2;
    return VR_LGWater_IsLiquid(pointcontents(mid));
};

// The shock through the water from `xAt`: what can be hurt, in a liquid, within `xRadius` of it (its middle) and not
// behind a wall, takes `xDamage` at the middle fading out to nothing at the radius (vr_lg_water_falloff: the power of
// the share of the radius left). Not `xSkip`. How many were hurt.
float(vector xAt, float xDamage, float xRadius, entity xSkip) VR_LGWater_Spread =
{
    float falloff = max(0, cvar_hget(cvarh_vr_lg_water_falloff));
    float hurt = 0;
    discharged = 1; // a wetsuit keeps it off (T_DamageImpl)
    entity head = findradius(xAt, xRadius);
    while(head)
    {
        if(head != xSkip && head.takedamage && VR_LGWater_InLiquid(head))
        {
            vector mid = (head.absmin + head.absmax) * 0.5;
            float share = 1 - min(1, vlen(mid - xAt) / xRadius);
            float damage = xDamage;
            if(falloff > 0)
            {
                damage = xDamage * pow(share, falloff);
            }
            traceline(xAt, mid, TRUE, world);
            if(damage >= 1 && trace_fraction == 1)
            {
                dprint(sprintf("lgwater: %s takes %g at %g units\n", head.classname, damage, vlen(mid - xAt)));
                particle2(mid, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 6);
                T_Damage(head, self, self, damage);
                hurt = hurt + 1;
            }
        }
        head = head.chain;
    }
    discharged = 0;
    return hurt;
};

// Both hands let go: a weapon falls (not handed to the other hand), what they carry is let go of.
void() VR_LGWater_DropAll =
{
    VR_Carry_Dead(); // what is carried, in one hand or both, drops with the motion it had
    for(float hand = 0; hand < 2; ++hand)
    {
        if(!VRIsHandWeaponless(self, hand))
        {
            DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand),
                VRGetEntWeaponClip(self, hand));
        }
        player_run(hand); // (the lightning gun's loop stops)
    }
};

// Fired with the body waist deep or `xHand` (or `xAt`: the lightning gun's muzzle, Mjolnir's head where it struck) in
// a liquid: the shock from `xAt`, `xCells` spent, `xWeapon` (IT_*) the damage's. FALSE if not (fire as usual).
float(float xHand, vector xAt, float xCells, float xWeapon) VR_LGWater_ShockAt =
{
    vector muzzle = xAt;
    float handIn = VR_LGWater_IsLiquid(pointcontents(VRGetHandPos(xHand))) || VR_LGWater_IsLiquid(pointcontents(muzzle));
    float bodyIn = self.waterlevel > 1;
    if(!handIn && !bodyIn)
    {
        return FALSE;
    }

    delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, xCells); // the bolt that shorts out
    self.damage_weapon = xWeapon; // HONEY

    float radius = max(1, cvar_hget(cvarh_vr_lg_water_radius));
    dprint(sprintf("lgwater: shock (hand %g in %g, body %g, waterlevel %g, weapon %g)\n", xHand, handIn, bodyIn,
        self.waterlevel, xWeapon));
    sound(self, CHAN_AUTO, "misc/power.wav", 1, ATTN_NORM);
    sound(self, CHAN_BODY, "weapons/lhit.wav", 1, ATTN_NORM);
    particle2(muzzle, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 60);
    particle2(muzzle, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 24);
    watershock(QVR_SHOCK_SELF, muzzle, radius, 1.2);
    watershock(QVR_SHOCK_BURST, muzzle, radius, 0.6);
    haptic(cVR_MainHand, 0, 0.6, 120, 1);
    haptic(cVR_OffHand, 0, 0.6, 120, 1);
    self.punchangle_x = -6;

    VR_LGWater_DropAll();
    VR_LGWater_Spread(muzzle, cvar_hget(cvarh_vr_lg_water_damage), radius, self);

    float selfDamage = cvar_hget(cvarh_vr_lg_water_self_damage);
    float before = self.health;
    discharged = 1;
    T_Damage(self, self, self, selfDamage);
    discharged = 0;
    dprint(sprintf("lgwater: you take %g: health %g -> %g\n", selfDamage, before, self.health));
    return TRUE;
};

float(float xHand) VR_LGWater_Shock =
{
    return VR_LGWater_ShockAt(xHand, VRGetMuzzlePos(xHand), 1, IT_LIGHTNING);
};

// A bolt from the open: if it goes into a liquid between `xMuzzle` and `xEnd`, the water round that point is
// electrified this bolt. Leaves the trace globals as they were.
void(vector xMuzzle, vector xEnd) VR_LGWater_Beam =
{
    vector at = liquidentry(xMuzzle, xEnd);
    if(at == xMuzzle)
    {
        return;
    }

    vector oldEnd = trace_endpos;
    entity oldEnt = trace_ent;
    float oldFraction = trace_fraction;

    float radius = max(1, cvar_hget(cvarh_vr_lg_water_radius));
    dprint(sprintf("lgwater: electrified at %v\n", at));
    watershock(QVR_SHOCK_SURFACE, at, radius, 0.3);
    VR_LGWater_Spread(at, cvar_hget(cvarh_vr_lg_water_tick_damage), radius, world);

    trace_endpos = oldEnd;
    trace_ent = oldEnt;
    trace_fraction = oldFraction;
};

// VR: a bolt from the open into water, slime or lava splashes where it goes in (NOTES.md vrfiringrange_2026-10-01_00-20),
// harder than a shot's (8: between a shot's 4 and a hand's), each bolt; its plip at most every 0.3 s (a bolt is every 0.1).
// The trace globals stay as the bolt's.
.float vr_lg_splash_time;
void(vector xMuzzle, vector xEnd) VR_LGWater_Splash =
{
    vector at = liquidentry(xMuzzle, xEnd);
    if(at == xMuzzle)
    {
        return;
    }
    float snd = QVR_SPLASH_SILENT;
    if(time >= self.vr_lg_splash_time)
    {
        snd = QVR_SPLASH_SHOT;
        self.vr_lg_splash_time = time + 0.3;
    }
    dprint(sprintf("lgsplash: at %v%s\n", at, snd == QVR_SPLASH_SHOT ? ", plip" : ""));
    watersplash(at, normalize(xEnd - xMuzzle), 8, snd);
};

void(float xHand) W_FireLightning =
{
    if(getCurrentAmmoOrClip(xHand) < 1)
    {
        return;
    }

    // VR: in water, the shock (vr_lg_water).
    if(cvar_hget(cvarh_vr_lg_water) && VR_LGWater_Shock(xHand))
    {
        return;
    }

    // explode if under water (Quake's, vr_lg_water 0)
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

    // Along the shot's line, from behind the muzzle (VR_ShotBackPoint): what the muzzle is inside is struck too.
    makeforward(VRGetWeaponFireRot(xHand));
    vector from = VR_ShotBackPoint(xHand, org, v_forward);
    traceline(from, org + v_forward*600, TRUE, self);
    if(cvar_hget(cvarh_vr_lg_water))
    {
        VR_LGWater_Beam(org, trace_endpos); // into water from the open: electrified
    }
    VR_LGWater_Splash(org, trace_endpos); // and a splash where it goes in
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 60);
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
    particle2(trace_endpos, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 12);
    VR_SetHitKind(QVR_HITKIND_SHOT, xHand, '0 0 0', v_forward);
    LightningDamage(from, trace_endpos + v_forward*4, self, cvar_hget(cvarh_vr_dmg_lightning));
    VR_ClearHitKind();

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.4, 175, 1.0);
    particle2(org, '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
};


//=============================================================================


void() GrenadeExplode =
{
    T_RadiusDamage(self, self.owner, VR_WpnDmg(self.owner, cvarh_vr_dmg_grenade, 120), world);

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
    VR_ShotPlace(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    VR_Grenade_Make(missile, GrenadeExplode, 2.5, TRUE); // QVR: catchable too with vr_grenade_catch 2

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.4, 75, 1.0);
    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 2);
};

void() ProximityExplode =
{
    T_RadiusDamage(self, self.owner, VR_WpnDmg(self.owner, cvarh_vr_dmg_proximity, 95), world);

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
    VR_ShotPlace(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    setsize(missile, '-1 -1 -1', '1 1 1');

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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
    VR_ShotPush_Launch(newmis, QVR_SHOTPUSH_NAIL); // VR: pushes the prop it hits or flies through
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
    VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    newmis.touch = superspike_touch;
    newmis.vr_shot_push_kind = QVR_SHOTPUSH_SUPERNAIL;
    setmodel(newmis, "progs/s_spike.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);

    self.punchangle_x = -2;

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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
    VR_ShotPlace(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);

    self.punchangle_x = -2;

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
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

    VR_ShotPush_Touch(); // VR: the prop it hit (or flew through) pushed

    makeforward_mdl(self.angles);
    VR_TouchTrace(self.origin - v_forward * 8, self.origin + v_forward * 8);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_nail, 9) * PositionalDamageFactor(posDmgType);

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

    VR_ShotPush_Touch(); // VR: the prop it hit (or flew through) pushed

    makeforward_mdl(self.angles);
    VR_TouchTrace(self.origin - v_forward * 8, self.origin + v_forward * 8);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        posDmgType = PositionalDamage(trace_ent, trace_endpos, v_forward);
        adjDmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_super_nail, 18) * PositionalDamageFactor(posDmgType);

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
    // Mjolnir without its cells is still a hammer: no click, as the trigger closes the hand round its handle (the
    // author holds it as a real hammer; NOTES.md vrfiringrange_2026-09-29_21-55-59). The motion's blows strike with
    // it; without tracked hands it swings as the hammer (below, fewer than 30 cells).
    // The chainsaw's trigger runs its chain (vr_chainsaw.qc VR_Chainsaw_Frame, every frame): no attack here.
    if(xWeapon == WID_CHAINSAW)
    {
        return;
    }

    // A grunt's shotgun or an enforcer's rifle: from its own ammo, its clip (vr_enemyguns.qc).
    if(VR_IsEnemyGun(xWeapon))
    {
        VR_EnemyGun_Attack(xHand, xWeapon, xAttackFinished, xOutAttackFinished);
        return;
    }

    float emptyHammer = xWeapon == WID_MJOLNIR && !W_CheckNoAmmo(xHand);
    if(emptyHammer && VRHandsTracked(self) && self.ishuman)
    {
        return;
    }

    if(!emptyHammer && !W_CheckNoAmmo(xHand))
    {
        if(time > xAttackFinished)
        {
            sound(self, CHAN_AUTO, "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
            if(cvar_hget(cvarh_vr_debug_shots))
            {
                dprint(sprintf("empty click: hand %g, weapon %g\n", xHand, xWeapon));
            }
        }

        return;
    }

    makevectors(VRGetWeaponFireRot(xHand)); // calculate forward angle for velocity
    self.show_hostile = time + 1; // wake monsters up

    // ZOID--
    RuneApplyBlackNoise(self); // make rune noise
    //--ZOID

    if(xWeapon == WID_AXE || xWeapon == WID_SWORD || xWeapon == WID_CROWBAR)
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
        SuperDamageSound();
        player_shot1(xHand);
        W_FireShotgun(xHand);
        *xOutAttackFinished = time + RuneApplyHell(0.5, self);
    }
    else if(xWeapon == WID_SUPER_SHOTGUN)
    {
        SuperDamageSound();
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
        SuperDamageSound();
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
        SuperDamageSound();
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
        SuperDamageSound();
        player_rocket1(xHand);
        W_FireProximityGrenade(xHand);
        *xOutAttackFinished = time + 0.6;
    }
    else if(xWeapon == WID_GRAPPLE)
    {
        // A hook that came off by itself (what it bit died or went) fires again only on a new pull (vr_grapple.qc).
        if(!getHookOut(xHand) && VR_Grapple_MustRelease(xHand))
            return;
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
    // VR: the new weapon is held, not carried by its foregrip (a hand's state: the hand-off).
    self.weaponflags = self.weaponflags - (self.weaponflags & QVR_WPNFLAG_FOREGRIP_CARRIED);
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

float vr_test_armor_kind;   // impulse 250: the next armour it drops (0 green, 1 yellow, 2 red)
float vr_test_held_kind;    // impulse 252: the next gib or head it puts in the off hand (round 21)
entity vr_test_armor_last;  // and the last it dropped (impulse 251)

// QVR: precise hit detection's test (impulse 238; ROUND21 "Precise hit detection (models, not boxes)"): at the monster
// (or corpse) spawned last, level from the player's side, each kind of test with Quake's boxes and then with its model:
// rays through its box's corner columns (2 units in from its edges: the empty part), at its middle, a ring of 24 at its
// head and 24 at its chest (their positional regions), and the same segments as melee blows (hitmodel_segment, 3 units
// thick) and thrown things (tracebox, vr_throw_hitbox 6). "on" counts what met the target; "behind" what went on to
// the world. Nothing is damaged.
float(entity xTarget, vector xFrom, vector xTo, float xFlags) VR_Test_HitRay =
{
    traceline(xFrom, xTo, xFlags, self);
    return trace_ent == xTarget;
};

void(entity t, float xPrecise) VR_Test_HitsMode =
{
    float saved = cvar("vr_hit_precise");
    cvar_set("vr_hit_precise", ftos(xPrecise));
    vector lo = t.origin + t.mins;
    vector hi = t.origin + t.maxs;
    vector c = (lo + hi) * 0.5;
    vector dir = c - self.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector side;
    side_x = -dir_y;
    side_y = dir_x;
    side_z = 0;
    float halfw = (t.maxs_x - t.mins_x) * 0.5 - 2;
    float halfh = (t.maxs_z - t.mins_z) * 0.5 - 2;
    float gunFlags = MOVE_HITGIBS | MOVE_HITMODEL;
    float onC = 0, behindC = 0, onB = 0, meleeC = 0, meleeB = 0, thrownC = 0, thrownB = 0, grappleC = 0, grappleB = 0;
    float k;
    vector at;
    for(k = 0; k < 6; ++k)
    {
        float sx = 1, sz = 1;
        if(k & 1) { sx = -1; }
        if(k >= 4) { sz = 0; } else if(k >= 2) { sz = -0.5; } // (low: beside the legs, above the floor)
        at = c + side * halfw * sx + '0 0 1' * halfh * sz;
        if(VR_Test_HitRay(t, at - dir * 64, at + dir * 128, gunFlags)) { onC++; }
        else if(trace_fraction < 1 && trace_ent == world) { behindC++; }
        if(VR_Test_HitRay(t, at - dir * 64, at + dir * 128, MOVE_HITMODEL_GRAPPLE)) { grappleC++; }
        float mt;
        if(xPrecise && hitmodel_target(t)) { mt = hitmodel_segment(t, at - dir * 24, at + dir * 24, 3, HITCLASS_MELEE); }
        else { mt = VR_SegmentBox(at - dir * 24, at + dir * 24, lo - '3 3 3', hi + '3 3 3'); }
        if(mt >= 0) { meleeC++; }
        tracebox(at - dir * 64, '-6 -6 -6', '6 6 6', at + dir * 128, MOVE_HITMODEL_THROWN, self);
        if(trace_ent == t) { thrownC++; }
    }
    // Its middle (chest height: a third of the way down from the top) and its head.
    float heads = 0, headOn = 0, chestOn = 0, chestHead = 0, chestLimb = 0, chestLegs = 0;
    vector chest = c + '0 0 1' * halfh * 0.35;
    if(VR_Test_HitRay(t, chest - dir * 64, chest + dir * 128, gunFlags)) { onB++; }
    dprint(sprintf("hittest %s at %v size %v %v, solid %d;", t.classname, t.origin, t.mins, t.maxs, t.solid));
    dprint(sprintf(" the middle ray from %v met %s (at %v) at %v", chest - dir * 64, trace_ent.classname, trace_ent.origin, trace_endpos));
    dprint(sprintf(" (%f, startsolid %d, its solid %d)", trace_fraction, trace_startsolid, trace_ent.solid));
    dprint("\n");
    if(VR_Test_HitRay(t, chest - dir * 64, chest + dir * 128, MOVE_HITMODEL_GRAPPLE)) { grappleB++; }
    float mb;
    if(xPrecise && hitmodel_target(t)) { mb = hitmodel_segment(t, chest - dir * 24, chest + dir * 24, 3, HITCLASS_MELEE); }
    else { mb = VR_SegmentBox(chest - dir * 24, chest + dir * 24, lo - '3 3 3', hi + '3 3 3'); }
    if(mb >= 0) { meleeB++; }
    tracebox(chest - dir * 64, '-6 -6 -6', '6 6 6', chest + dir * 128, MOVE_HITMODEL_THROWN, self);
    if(trace_ent == t) { thrownB++; }
    float hasHead = PositionalHead(t);
    vector head = t.origin + '0 0 1' * xHeadUp;
    float yaw = t.angles_y * (3.14159265 / 180);
    head_x = head_x + cos(yaw) * xHeadForward;
    head_y = head_y + sin(yaw) * xHeadForward;
    for(k = 0; k < 24; ++k)
    {
        float a = k * (3.14159265 * 2 / 24);
        vector r;
        r_x = cos(a);
        r_y = sin(a);
        r_z = 0;
        if(hasHead && VR_Test_HitRay(t, head - r * 64, head + r * 128, gunFlags))
        {
            headOn++;
            if(PositionalRegion(t, trace_endpos, r) == 1) { heads++; }
            if(xPrecise && cvar("developer") > 1)
            {
                dprint(sprintf("hittest head ray %d: the rest point %f from the head's middle (radius %f)\n", k,
                    vlen(hitmodel_rest(t, trace_endpos) - head), xHeadRadius));
            }
        }
        if(VR_Test_HitRay(t, chest - r * 64, chest + r * 128, gunFlags))
        {
            chestOn++;
            float reg = PositionalRegion(t, trace_endpos, r);
            if(reg == 1) { chestHead++; } else if(reg == 2) { chestLimb++; } else if(reg == 3) { chestLegs++; }
        }
    }
    string mode = "boxes";
    if(xPrecise) { mode = "model"; }
    dprint(sprintf("hittest %s %s: corner rays %d/6 on it (%d on to the world), middle %d/1;", t.classname, mode, onC, behindC, onB));
    dprint(sprintf(" grapple corners %d/6, middle %d/1; melee corners %d/6, middle %d/1;", grappleC, grappleB, meleeC, meleeB));
    dprint(sprintf(" thrown corners %d/6, middle %d/1; head ring %d/24 on it (%d headshots);", thrownC, thrownB, headOn, heads));
    dprint(sprintf(" chest ring %d/24 on it (%d head, %d limb, %d legs)\n", chestOn, chestHead, chestLimb, chestLegs));
    cvar_set("vr_hit_precise", ftos(saved));
};

void() VR_Test_Hits =
{
    // The monster (or corpse) spawned last (impulse 241).
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self)
        {
            t = e;
        }
        e = nextent(e);
    }
    if(t == world)
    {
        dprint("hittest: no monster\n");
        return;
    }
    VR_Test_HitsMode(t, 0);
    VR_Test_HitsMode(t, 1);
    // Then removed, with anything else there (the next one is tested where it stood).
    vector at = t.origin;
    e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self && vlen(e.origin - at) < 64)
        {
            remove(e);
        }
        e = nextent(e);
    }
};


// Projectiles through the same corner columns and at the middle (impulse 237 a nail, 236 a rocket, 234 a nail with the
// grappling hook's class, one at a time, each
// through the next of the 6 columns, then the middle; 235 prints what the last touched): real missiles, as the engine
// flies them (MOVE_MISSILE, with the precise hits of the option as it is set). The target takes no damage (no pain
// poses, no shove), and is put back where it stood.
entity vr_test_target;
vector vr_test_home;
float vr_test_shot;
float vr_test_on;
float vr_test_world;
float vr_test_other;
void() VR_Test_MissileTouch =
{
    if(other == vr_test_target) { vr_test_on++; }
    else if(other == world) { vr_test_world++; }
    else { vr_test_other++; }
    if(self.classname == "spike") { spike_touch(); }
    else { T_MissileTouch(); }
};

void(float xRockets) VR_Test_Missiles =
{
    entity t = world;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e != self) { t = e; }
        e = nextent(e);
    }
    if(t == world) { dprint("hittest: no monster\n"); return; }
    if(t != vr_test_target)
    {
        vr_test_target = t;
        vr_test_home = t.origin;
        vr_test_shot = 0;
    }
    setorigin(t, vr_test_home);
    t.velocity = '0 0 0';
    t.takedamage = DAMAGE_NO; // (no pain, no shove: it stands as it was)
    vr_test_on = vr_test_world = vr_test_other = 0;
    vector lo = t.origin + t.mins;
    vector hi = t.origin + t.maxs;
    vector c = (lo + hi) * 0.5;
    vector dir = c - self.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector side;
    side_x = -dir_y;
    side_y = dir_x;
    side_z = 0;
    float halfw = (t.maxs_x - t.mins_x) * 0.5 - 2;
    float halfh = (t.maxs_z - t.mins_z) * 0.5 - 2;
    float k = vr_test_shot;
    vr_test_shot = (vr_test_shot + 1) % 7;
    float sx = 1, sz = 1;
    if(k & 1) { sx = -1; }
    if(k >= 4) { sz = 0; } else if(k >= 2) { sz = -0.5; }
    vector at = c + side * halfw * sx + '0 0 1' * halfh * sz;
    if(k == 6) { at = c + '0 0 1' * halfh * 0.35; } // the middle (chest height)
    entity m = launch_spike(at - dir * 64, dir); // (in front of the player's hands, which it would meet)
    if(xRockets == 2)
    {
        m.vr_hitclass = HITCLASS_GRAPPLE; // the grappling hook's class (its tolerance): a hook's hit test
    }
    else if(xRockets)
    {
        m.classname = "missile";
        setmodel(m, "progs/missile.mdl");
        setsize(m, '0 0 0', '0 0 0');
        m.velocity = dir * 1000;
    }
    m.touch = VR_Test_MissileTouch;
    string kind = "nail";
    if(xRockets == 2) { kind = "hook"; } else if(xRockets) { kind = "rocket"; }
    dprint(sprintf("hittest %s: %s %d (precise %d):", t.classname, kind, k, cvar("vr_hit_precise")));
};

void() VR_Test_MissilesReport =
{
    string what = "nothing";
    if(vr_test_on) { what = "it"; } else if(vr_test_world) { what = "the world"; } else if(vr_test_other) { what = "something else"; }
    dprint(sprintf(" touched %s\n", what));
};

// vr_physics_fire (vr_box3d.cpp): a missile or a shot of yours from your eyes at the world point xAt, flying straight
// at it (shape tests: missiles and shots against a turned solid prop; vr_debug_missiles prints where a missile hits,
// vr_debug_shots what a pellet hit). xKind: 0 a rocket, 1 a nail, 2 a grenade, 3 a super nail, 4 an enforcer's laser,
// 10 a shotgun pellet (the burst rifle's and the lightning's trace: the same). The missile, or world for a shot.
entity(float xKind, vector xAt) VR_Test_Fire =
{
    vector from = self.origin + self.view_ofs;
    vector dir = normalize(xAt - from);
    from = from + dir * 24; // (ahead of your hands)
    if(xKind == 10)
    {
        traceline(from, from + dir * 2048, MOVE_HITGIBS | MOVE_HITMODEL, self); // (a pellet's: FireBulletsImpl)
        string what = "nothing";
        if(trace_ent != world) { what = trace_ent.classname; } else if(trace_fraction < 1) { what = "the world"; }
        dprint(sprintf("test fire: shot from %.3f %.3f %.3f dir %.5f %.5f %.5f ", from_x, from_y, from_z, dir_x, dir_y, dir_z));
        dprint(sprintf("hit %s at %.3f %.3f %.3f\n", what, trace_endpos_x, trace_endpos_y, trace_endpos_z));
        vr_tracer_hand = -1;
        vr_tracer_from = from;
        FireBulletsImpl(1, dir, '0 0 0', from, 2048, FALSE); // (and the shot itself)
        return world;
    }
    entity m;
    if(xKind == 2)
    {
        m = spawn();
        m.owner = self;
        m.movetype = MOVETYPE_BOUNCE;
        m.solid = SOLID_BBOX;
        m.classname = "grenade";
        m.touch = GrenadeTouch;
        m.nextthink = time + 2.5;
        m.think = GrenadeExplode;
        setmodel(m, "progs/grenade.mdl");
        setsize(m, '0 0 0', '0 0 0');
        setorigin(m, from);
        m.velocity = dir * 600;
    }
    else if(xKind == 4)
    {
        m = LaunchLaser(from, dir);
    }
    else
    {
        m = launch_spike(from, dir);
        if(xKind == 0)
        {
            m.classname = "missile";
            m.touch = T_MissileTouch;
            m.vr_shot_push_kind = 0;
            setmodel(m, "progs/missile.mdl");
            m.velocity = dir * 1000;
        }
        else if(xKind == 3)
        {
            m.touch = superspike_touch;
            m.vr_shot_push_kind = QVR_SHOTPUSH_SUPERNAIL;
            setmodel(m, "progs/s_spike.mdl");
        }
        setsize(m, '0 0 0', '0 0 0');
    }
    m.angles = vectoangles(m.velocity);
    return m;
};

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
        self.impulse = 0;
        if(cheatWid > WID_LAST)
        {
            return; // no such weapon (it broke the hand's weapon state)
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

    // A weapon's button pressed (the other hand's fingertip on it): its secondary ammo; the grappling hook's detach
    // (vr_grapple.qc). 44/45: the grappling gun's front button, the reel-in.
    if(self.impulse == 42)
    {
        if(VRGetWeapon(cVR_OffHand) == WID_GRAPPLE) VR_Grapple_Detach(cVR_OffHand);
        else                                         ToggleSecondaryAmmo(cVR_OffHand);
    }

    if(self.impulse == 43)
    {
        if(VRGetWeapon(cVR_MainHand) == WID_GRAPPLE) VR_Grapple_Detach(cVR_MainHand);
        else                                          ToggleSecondaryAmmo(cVR_MainHand);
    }

    if(self.impulse == 44 && VRGetWeapon(cVR_OffHand) == WID_GRAPPLE)
    {
        VR_Grapple_QuickRelease(cVR_OffHand);
    }

    if(self.impulse == 45 && VRGetWeapon(cVR_MainHand) == WID_GRAPPLE)
    {
        VR_Grapple_QuickRelease(cVR_MainHand);
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




    // The hand's B/Y also reels a grappling hook in (vr_grapple.qc: its own gun's, or the other hand's when its own has
    // none out): while it drives one, it cycles nothing.
    if(self.impulse == 10 && time >= self.attack_finished && !VR_Grapple_ButtonsBusy(cVR_MainHand))
    {
        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            CycleWeaponCommand(cVR_MainHand,
                self.weapon, &self.weapon,
                self.weaponflags, &self.weaponflags);
        }
    }

    if(self.impulse == 12 && time >= self.offhand_attack_finished && !VR_Grapple_ButtonsBusy(cVR_OffHand))
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
    if(self.impulse == 244 && !deathmatch && !coop) // QVR: a grunt 64 units ahead (shove and ledge tests)
    {
        makevectors('0 1 0' * self.v_angle_y);
        vector gruntAt = self.origin + v_forward * 64;
        entity player = self;
        self = spawn();
        self.classname = "monster_army";
        setorigin(self, gruntAt);
        self.angles_y = player.v_angle_y + 180;
        monster_army();
        self = player;
    }
    if(self.impulse == 238 && !deathmatch && !coop) // QVR: precise hit detection's test (VR_Test_Hits)
    {
        VR_Test_Hits();
    }
    if(self.impulse == 237 && !deathmatch && !coop) // QVR: and with nails (VR_Test_Missiles),
    {
        VR_Test_Missiles(FALSE);
    }
    if(self.impulse == 236 && !deathmatch && !coop) // rockets,
    {
        VR_Test_Missiles(TRUE);
    }
    if(self.impulse == 234 && !deathmatch && !coop) // a nail with the grappling hook's class,
    {
        VR_Test_Missiles(2);
    }
    if(self.impulse == 235 && !deathmatch && !coop) // and what they touched
    {
        VR_Test_MissilesReport();
    }
    if(self.impulse == 239) // QVR: every grappling hook's state, its gun's place and its rope (vr_grapple.qc)
    {
        VR_Grapple_Report();
    }
    if(self.impulse == 227 && !deathmatch && !coop) // QVR test: the chainsaws in your hands nearly empty (vr_chainsaw.qc)
    {
        VR_Chainsaw_Impulse(TRUE);
    }
    if(self.impulse == 228) // QVR: the chainsaws in your hands: fuel, engine, cord (vr_chainsaw.qc)
    {
        VR_Chainsaw_Impulse(FALSE);
    }
    if(self.impulse == 229 && !deathmatch && !coop) // QVR test: the chainsaw lying nearest you into an empty hand
    {
        VR_Chainsaw_TakeNearest();
    }
    if(self.impulse == 216 && !deathmatch && !coop) // QVR test: the crowbar lying nearest you into an empty hand
    {
        VR_Crowbar_TakeNearest();
    }
    if(self.impulse == 217 && !deathmatch && !coop) // QVR test: a crowbar dropped ahead of you (VR_Crowbar_Spawn)
    {
        VR_Crowbar_SpawnAhead();
    }
    if(self.impulse == 218) // QVR: each crowbar lying about and in your hands (vr_crowbar.qc)
    {
        VR_Crowbar_Report();
    }
    if(self.impulse == 212 && !deathmatch && !coop) // QVR test: the enemy gun lying nearest you into an empty hand
    {
        VR_EnemyGuns_TakeNearest();
    }
    if(self.impulse == 213) // QVR: the enemy guns in your hands and their ammo (vr_enemyguns.qc)
    {
        VR_EnemyGuns_Impulse(FALSE);
    }
    if(self.impulse == 214 && !deathmatch && !coop) // QVR test: the enemy guns in your hands left with one shot
    {
        VR_EnemyGuns_Impulse(TRUE);
    }
    if(self.impulse == 215 && !deathmatch && !coop) // QVR test: Mjolnir in the main hand strikes its lightning now
    {
        // (as a blow with 15 cells does, from its head: VRGetMuzzlePos; in water, the shock)
        if(VRGetEntWeapon(self, cVR_MainHand) == WID_MJOLNIR && self.ammo_cells >= 15)
        {
            HIP_FireMjolnirLightning(cVR_MainHand, VRGetMuzzlePos(cVR_MainHand));
        }
        else
        {
            bprint(sprintf("impulse 215: no Mjolnir in the main hand (weapon %g), or under 15 cells (%g)\n",
                VRGetEntWeapon(self, cVR_MainHand), self.ammo_cells));
        }
    }
    if(self.impulse == 230 && !deathmatch && !coop) // QVR test: the chainsaws in your hands started (no cord pulled)
    {
        VR_Chainsaw_StartAll();
    }
    if(self.impulse == 232 && !deathmatch && !coop) // QVR test: the nearest prop flung at a monster or you (VR_Test_Fling)
    {
        VR_Test_Fling();
    }
    if(self.impulse == 231 && !deathmatch && !coop) // QVR test: an empty hand takes the nearest grappling gun lying about
    {
        VR_Grapple_TestTake();
    }
    if(self.impulse == 233 && !deathmatch && !coop) // QVR test: you touch what the hooks are in (a pickup is taken)
    {
        VR_Grapple_TestTouch();
    }
    if(self.impulse == 209 && !deathmatch && !coop) // QVR test: an axe thrown ahead (vr_test_axe; vr_axestick.qc)
    {
        VR_AxeStick_TestThrow();
    }
    if(self.impulse == 207 && !deathmatch && !coop) // QVR test: your main hand on the nearest stuck axe's handle
    {
        VR_AxeStick_TestReach();
    }
    if(self.impulse == 208) // QVR test: each thrown axe, stuck or not (vr_axestick.qc)
    {
        VR_AxeStick_TestReport();
    }
    if(self.impulse == 241 && !deathmatch && !coop) // QVR: a monster or a box ahead, facing you (vr_test_spawn; weapons against models)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector spawnAt = self.origin + v_forward * cvar("vr_test_spawn_dist");
        float spawnKind = cvar("vr_test_spawn");
        entity spawnPlayer = self;
        entity spawned = world;
        if(spawnKind == 105 || spawnKind == 106)
        {
            traceline(spawnAt, spawnAt - '0 0 256', TRUE, self); // (on the floor)
            spawnAt = trace_endpos;
        }
        if(spawnKind == 105)
        {
            // An ogre's head lying on the floor (gib destroying tests): an ogre gibbed, its head (the ogre itself) put
            // there.
            entity ogreDispenser = spawn();
            ogreDispenser.weapon = 1;
            setorigin(ogreDispenser, spawnAt);
            self = ogreDispenser;
            activator = spawnPlayer;
            func_enemy_dispenser_use();
            spawned = self;
            self = spawnPlayer;
            remove(ogreDispenser);
            if(spawned != ogreDispenser)
            {
                spawned.takedamage = DAMAGE_AIM;
                T_Damage(spawned, world, world, spawned.health + 200);
                setorigin(spawned, spawnAt + '0 0 1');
                spawned.velocity = '0 0 0';
                spawned.avelocity = '0 0 0';
                dprint(sprintf("test spawn: %s's head at %v, box %v .. %v, gib %g\n", spawned.classname, spawned.origin,
                    spawned.mins, spawned.maxs, spawned.vr_gib));
            }
            spawned = world;
        }
        else if(spawnKind == 106)
        {
            // A gib lying on the floor (gib destroying tests).
            self = spawn();
            setorigin(self, spawnAt + '0 0 1');
            ThrowGib("progs/gib1.mdl", -10);
            entity lying = find(world, classname, "");
            while(lying)
            {
                if(lying.vr_gib && lying.ltime == time && lying.carry_player == world && lying.origin == self.origin)
                {
                    lying.velocity = '0 0 0';
                    lying.avelocity = '0 0 0';
                    dprint(sprintf("test spawn: a gib at %v, box %v .. %v\n", lying.origin, lying.mins, lying.maxs));
                }
                lying = find(lying, classname, "");
            }
            remove(self);
            self = spawnPlayer;
        }
        else if(spawnKind >= 107 && spawnKind <= 109)
        {
            // A crate (vr_crates.qc), turned 30 degrees off facing you; 109: a large one with a small one on it.
            traceline(spawnAt + '0 0 8', spawnAt - '0 0 256', TRUE, self); // (on the floor)
            VR_Crate_TestSpawn(trace_endpos, spawnKind == 107 ? 1 : 2, spawnPlayer.v_viewangle_y + 30, spawnKind == 109);
        }
        else if(spawnKind >= 100)
        {
            self = spawn();
            setorigin(self, spawnAt - '16 16 0'); // a box's origin is its corner: its middle there
            if(spawnKind == 100)      { item_health(); }
            else if(spawnKind == 102) { misc_explobox(); }  // 40 kg (grappling hook tests)
            else if(spawnKind == 103) { misc_explobox2(); } // 25 kg
            else if(spawnKind == 104) { misc_explobox(); self.takedamage = DAMAGE_NO; } // one that never blows up (shot pushes)
            else                      { item_shells(); }
            if((spawnKind == 100 || spawnKind == 101) && !VRPlaceItemNow()) // (its final size from the first frame)
            {
                self = world;
            }
            spawned = self;
            float spawnYaw = cvar("vr_test_spawn_yaw");
            float spawnTilt = cvar("vr_test_spawn_tilt");
            if(spawnYaw != 0 || spawnTilt != 0)
            {
                // Loose (a physics prop), turned about its middle and tipped, its lowest corner on the floor where it
                // stood (grappling hook tests: a turned box's empty corners).
                vector spawnMid = (self.mins + self.maxs) * 0.5;
                vector spawnAt2 = self.origin + spawnMid;
                float spawnFloor = self.origin_z + self.mins_z;
                VR_Carry_Loose(self);
                self.angles = '0 1 0' * (spawnPlayer.v_viewangle_y + spawnYaw) + '0 0 1' * spawnTilt;
                vector spawnTurned = spawnAt2 - modelpoint(self, self.angles, spawnMid);
                float spawnLow = 999999;
                for(float c = 0; c < 8; c++)
                {
                    vector corner = self.mins;
                    if(c & 1) { corner_x = self.maxs_x; }
                    if(c & 2) { corner_y = self.maxs_y; }
                    if(c & 4) { corner_z = self.maxs_z; }
                    corner = modelpoint(self, self.angles, corner);
                    spawnLow = min(spawnLow, spawnTurned_z + corner_z);
                }
                spawnTurned_z = spawnTurned_z + spawnFloor + 0.5 - spawnLow;
                setorigin(self, spawnTurned);
                self.velocity = '0 0 0';
                dprint(sprintf("test spawn: %s turned %v at %v, its middle at %v\n", self.classname, self.angles, self.origin,
                    self.origin + modelpoint(self, self.angles, spawnMid)));
            }
        }
        else
        {
            // The firing range's dispenser, as if its button were pressed (the same numbers).
            entity dispenser = spawn();
            dispenser.weapon = spawnKind;
            setorigin(dispenser, spawnAt);
            self = dispenser;
            activator = spawnPlayer;
            func_enemy_dispenser_use();
            if(self != dispenser)
            {
                spawned = self;
                spawned.angles_y = spawnPlayer.v_viewangle_y + 180;
            }
            remove(dispenser);
        }
        self = spawnPlayer;
        if(spawned != world && spawnKind < 100 && cvar("vr_test_spawn_dead"))
        {
            spawned.takedamage = DAMAGE_AIM; // (walkmonster_start gives it a moment later)
            // 2: gibbed (its head left lying: the flies' tests, VR_Head_StopFlies)
            T_Damage(spawned, world, world, spawned.health + (cvar("vr_test_spawn_dead") >= 2 ? 100 : 1));
        }
    }
    if(self.impulse == 245 && !deathmatch && !coop) // QVR: a gib in the empty off hand (held gib tests)
    {
        ThrowGib("progs/gib1.mdl", -10);
        entity gib = find(world, classname, "");
        entity newest = world;
        while(gib)
        {
            if(gib.vr_gib && gib.carry_player == world && gib.ltime == time && gib.model == "progs/gib1.mdl")
            {
                newest = gib;
            }
            gib = find(gib, classname, "");
        }
        if(newest != world && VRIsHandEmpty(self, cVR_OffHand))
        {
            setorigin(newest, VRGetHandPos(cVR_OffHand));
            VR_Carry_Start(self, cVR_OffHand, newest);
        }
    }
    // QVR: a gib or a head in the empty off hand, each in turn (fitted hands tests, round 21).
    if(self.impulse == 252 && !deathmatch && !coop && VRIsHandEmpty(self, cVR_OffHand))
    {
        string testModel = "progs/gib1.mdl";
        if(vr_test_held_kind == 1) testModel = "progs/h_player.mdl";
        else if(vr_test_held_kind == 2) testModel = "progs/gib2.mdl";
        else if(vr_test_held_kind == 3) testModel = "progs/gib3.mdl";
        else if(vr_test_held_kind == 4) testModel = "progs/h_guard.mdl";
        else if(vr_test_held_kind == 5) testModel = "progs/h_ogre.mdl";
        else if(vr_test_held_kind == 6) testModel = "progs/h_knight.mdl";
        else if(vr_test_held_kind == 7) testModel = "progs/h_zombie.mdl";
        else if(vr_test_held_kind == 8) testModel = "progs/h_demon.mdl";
        vr_test_held_kind = vr_test_held_kind >= 8 ? 0 : vr_test_held_kind + 1;
        entity testGib = spawn();
        setmodel(testGib, testModel);
        testGib.movetype = MOVETYPE_BOUNCE;
        testGib.solid = SOLID_NOT;
        testGib.think = SUB_Remove;
        testGib.nextthink = time + 60;
        VR_MakeGibGrabbable(testGib);
        setorigin(testGib, VRGetHandPos(cVR_OffHand));
        VR_Carry_Start(self, cVR_OffHand, testGib);
        dprint("test: "); dprint(testModel); dprint(" in the off hand\n");
    }
    // QVR: prints the held weapons' hotspots (weaponhotspot, weaponhotspotinfo; round 21).
    if(self.impulse == 253)
    {
        float testHand = 0;
        while(testHand < 2)
        {
            float testIndex = 0;
            while(testIndex < 4)
            {
                float testType = weaponhotspotinfo(self, testHand, testIndex, 0);
                if(testType)
                {
                    dprint("hotspot: hand "); dprint(ftos(testHand)); dprint(" #"); dprint(ftos(testIndex));
                    dprint(" type "); dprint(ftos(testType)); dprint(" at "); dprint(vtos(weaponhotspot(self, testHand, testIndex)));
                    dprint(" bias "); dprint(ftos(weaponhotspotinfo(self, testHand, testIndex, 1)));
                    dprint(" share "); dprint(ftos(weaponhotspotinfo(self, testHand, testIndex, 2))); dprint("\n");
                }
                testIndex = testIndex + 1;
            }
            testHand = testHand + 1;
        }
    }
    // QVR: a monster's projectile from 300 units away at your face, for batting and catching tests (vr_deflect,
    // vr_grenade.qc): 246 the kind vr_test_projectile says (0 a knight's spike at 600 units/s, 1 an enforcer's laser,
    // 2 a scrag's spit, 3 a vore's ball, 4 an ogre's grenade, 5 a zombie's flesh; the lobbed ones on the arc that
    // reaches you), from vr_test_projectile_side degrees to your left of ahead; 247 a hell knight's spike (300 u/s).
    if((self.impulse == 246 || self.impulse == 247) && !deathmatch && !coop)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector aim = VRGetHeadPos(self) + v_forward * 16 - '0 0 10';
        float side = cvar("vr_test_projectile_side") * 0.0174533;
        vector away = v_forward * cos(side) - v_right * sin(side);
        entity shooter = spawn();
        shooter.classname = "monster_test_shooter";
        shooter.flags = FL_MONSTER;
        shooter.enemy = self;
        setorigin(shooter, aim + away * 300);
        shooter.angles = vectoangles(away * -1);
        shooter.think = SUB_Remove;
        shooter.nextthink = time + 6;
        entity player = self;
        float kind = 0;
        if(player.impulse == 246)
        {
            kind = cvar("vr_test_projectile");
        }
        vector dir = normalize(aim - shooter.origin);
        self = shooter;
        entity shot = world;
        if(kind == 1)
        {
            shot = LaunchLaser(shooter.origin, dir);
        }
        else if(kind == 2)
        {
            shot = launch_spike(shooter.origin, dir);
            shot.velocity = dir * 600;
            shot.classname = "wizspike";
            setmodel(shot, "progs/w_spike.mdl");
            setsize(shot, '0 0 0', '0 0 0');
        }
        else if(kind == 3)
        {
            setorigin(shooter, shooter.origin - '0 0 10'); // (it fires from 10 units above itself)
            ShalMissile();
        }
        else if(kind == 4 || kind == 5)
        {
            if(kind == 4)
            {
                OgreFireGrenade();
            }
            else
            {
                makevectors(shooter.angles);
                ZombieFireGrenade('0 0 24'); // (from its origin)
            }
            for(shot = nextent(world); shot; shot = nextent(shot))
            {
                if(shot.owner == shooter)
                {
                    shot.velocity = VR_Lob_Velocity(shot.origin, aim, VR_LOB_SPEED, shooter); // (at your face)
                    shot.vr_gren_lastvel = shot.velocity;
                    break;
                }
            }
        }
        else
        {
            shot = launch_spike(shooter.origin, dir);
            shot.velocity = dir * (player.impulse == 246 ? 600 : 300);
            shot.classname = "knightspike";
        }
        self = player;
        dprint(sprintf("test: projectile %g from %v at %v\n", kind, shooter.origin, aim));
    }
    // QVR: the nearest ogre or zombie throws at you now (grenade and lob tests; vr_grenade.qc); an ogre a multi-grenade
    // with vr_test_projectile 6.
    if(self.impulse == 240 && !deathmatch && !coop)
    {
        entity lobber = world;
        float lobberDist = 4000;
        for(entity near = findradius(self.origin, 4000); near; near = near.chain)
        {
            if((near.flags & FL_MONSTER) && near.health > 0 && vlen(near.origin - self.origin) < lobberDist &&
               (substring(near.classname, 0, 12) == "monster_ogre" || near.classname == "monster_zombie"))
            {
                lobber = near;
                lobberDist = vlen(near.origin - self.origin);
            }
        }
        if(lobber != world)
        {
            entity lobPlayer = self;
            self = lobber;
            self.enemy = lobPlayer;
            self.angles_y = vectoyaw(lobPlayer.origin - self.origin);
            dprint(sprintf("test: %s at %v throws at you at %v\n", self.classname, self.origin, lobPlayer.origin));
            makevectors(self.angles);
            if(self.classname == "monster_zombie")
            {
                ZombieFireGrenade('-10 -22 30');
            }
            else
            {
                float lobFlags = self.spawnflags;
                if(cvar("vr_test_projectile") == 6)
                {
                    self.spawnflags = self.spawnflags | OGRE_BOSS; // (a multi-grenade)
                }
                OgreFireGrenade();
                self.spawnflags = lobFlags;
            }
            self = lobPlayer;
        }
    }
    if(self.impulse == 242 && !deathmatch && !coop) // QVR: the nearest melee monster strikes you now (parry tests; vr_melee.qc)
    {
        VR_Parry_TestBlow();
    }
    if(self.impulse == 248 && !deathmatch && !coop) // QVR: a knight 64 units ahead (parry tests)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector knightAt = self.origin + v_forward * 64;
        entity knightPlayer = self;
        self = spawn();
        self.classname = "monster_knight";
        setorigin(self, knightAt);
        self.angles_y = knightPlayer.v_viewangle_y + 180;
        monster_knight();
        self = knightPlayer;
    }
    if(self.impulse == 249 && !deathmatch && !coop) // QVR: does the pose parry a blow from ahead? (developer 1)
    {
        VR_Parry_Probe();
    }
    if(self.impulse == 250 && !deathmatch && !coop) // QVR: an armour 24 units ahead: green, yellow, red in turn (wearable armour tests)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector armorAt = self.origin + v_forward * 24;
        entity armorPlayer = self;
        float armorKind = vr_test_armor_kind;
        vr_test_armor_kind = armorKind >= 2 ? 0 : armorKind + 1;
        self = spawn();
        setorigin(self, armorAt);
        self.angles_y = armorPlayer.v_viewangle_y + 90;
        if(armorKind == 0)
        {
            self.classname = ITEM_NAME_ARMOR1;
            item_armor1();
        }
        else if(armorKind == 1)
        {
            self.classname = ITEM_NAME_ARMOR2;
            item_armor2();
        }
        else
        {
            self.classname = ITEM_NAME_ARMORINV;
            item_armorInv();
        }
        dprint("test: "); dprint(self.classname); dprint(" at "); dprint(vtos(armorAt)); dprint(", head ");
        dprint(vtos(VRGetHeadPos(armorPlayer))); dprint(" yaw "); dprint(ftos(armorPlayer.v_viewangle_y)); dprint(", hands ");
        dprint(vtos(armorPlayer.offhandpos)); dprint(" "); dprint(vtos(armorPlayer.handpos)); dprint("\n");
        vr_test_armor_last = self;
        self = armorPlayer;
    }
    // QVR: the last armour impulse 250 dropped comes back 3 s after it is worn (deathmatch's respawn, sped up).
    if(self.impulse == 251 && !deathmatch && !coop && vr_test_armor_last != world)
    {
        vr_test_armor_last.wait = 3;
    }
    if(dump_coord == 1)
        DumpCoordinates();
    self.impulse = 0;
};

void(vector xOrigin, float xHand) PlayerLightningGunEffect =
{
    makeforward(VRGetWeaponFireRot(xHand));
    traceline(VR_ShotBackPoint(xHand, xOrigin, v_forward), xOrigin + v_forward*600, TRUE, self); // as W_FireLightning

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, xHand); /* disambiguator */
    WriteVec3(MSG_BROADCAST, xOrigin);
    WriteVec3(MSG_BROADCAST, trace_endpos);
}

float(entity ent, float xHand) VRWeaponLeavingHand;

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
    // A gun hanging from its foregrip (the hand-off) is carried, not held: it does not fire.
    if(VRIsHandCarryingByForegrip(self, xHand))
    {
        return;
    }

    // Let go of this frame (W_Frame's DoHandImpl, later in the frame, throws or holsters it): no shot as it leaves.
    if(VRWeaponLeavingHand(self, xHand))
    {
        return;
    }

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
        // Quad's sound: not here (the trigger), but where damage is dealt: each gun's shot, a blow landing
        // (VR_QuadSound, combat.qc).
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

// Settled (no push left, throwstabilize 0, and the angle already in [offset - 360, offset]), the
// stabilizing would leave the angle as it is: most thinks of a weapon lying about, so skipped.
void() wpnthrow_stabilize =
{
    float dim = self.throwstabilizedim;
    if(dim == 0)
    {
        if(!self.throwstabilize && self.angles_x <= 90 && self.angles_x >= -270)
        {
            return;
        }
        wpnthrow_impl_stabilize(90, self.angles_x, &self.angles_x);
    }
    else if(dim == 1)
    {
        if(!self.throwstabilize && self.angles_y <= 0 && self.angles_y >= -360)
        {
            return;
        }
        wpnthrow_impl_stabilize(0, self.angles_y, &self.angles_y);
    }
    else if(dim == 2)
    {
        if(!self.throwstabilize && self.angles_z <= 0 && self.angles_z >= -360)
        {
            return;
        }
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

        // Rigid bodies float in the engine (vr_rigid.cpp: lift by how deep they are, water drag).
        if(self.ammocounter > 1 && self.vr_rigid)
        {
            self.flags = self.flags | FL_INWATER;
            self.throwhit = QVR_THROWHIT_HIT;
            self.solid = SOLID_NOT_BUT_TOUCHABLE;
        }
        else if(self.ammocounter > 1)
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

    if(vr_weapondrop_particles_on && (self.flags & (FL_ONGROUND | FL_INWATER))) // (vr_cvars.qc, once a frame)
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

    if(VR_AxeStick_IsStuck(self))
    {
        VR_AxeStick_Think(); // stuck in something by its blade (vr_axestick.qc)
        if(self.think == wpnthrow_think)
        {
            self.nextthink = time + 0.02;
        }
        return;
    }

    forcegrabbable_think_impl();
    self.nextthink = time + 0.02;
}

// Tests (impulse 232; Debug > Tests > Flung Props): the loose prop nearest you (a rigid body not carried) sent at
// vr_test_fling_speed m/s at the nearest monster (vr_test_fling_at 1: at you), from 40 units off the side of its box
// facing you, at its middle's height, not thrown by anyone (VR_Prop_Flung). vr_test_fling_away 1: from inside the edge
// of its box, away from it (a batted prop leaving you, still in your box).
void() VR_Test_Fling =
{
    entity p = self;
    entity target = p;
    float best;
    entity e;
    if(!cvar("vr_test_fling_at"))
    {
        target = world;
        best = 100000;
        for(e = nextent(world); e != world; e = nextent(e))
        {
            if(e.flags & FL_MONSTER && e.health > 0 && vlen(e.origin - p.origin) < best)
            {
                best = vlen(e.origin - p.origin);
                target = e;
            }
        }
    }
    entity prop = world;
    best = 512;
    for(e = nextent(world); e != world; e = nextent(e))
    {
        if(e.vr_rigid && e.carry_player == world && (e.touch == forcegrabbable_touch || e.vr_impact) &&
           vlen(e.origin - p.origin) < best)
        {
            best = vlen(e.origin - p.origin);
            prop = e;
        }
    }
    if(target == world || prop == world)
    {
        dprint("fling test: no monster or no prop\n");
        return;
    }
    vector mid = 0.5 * (target.absmin + target.absmax);
    vector dir;
    if(target == p)
    {
        makevectors('0 1 0' * p.v_viewangle_y);
        dir = -1 * v_forward; // (towards you from ahead)
    }
    else
    {
        dir = mid - p.origin;
        dir_z = 0;
        dir = normalize(dir);
    }
    float half = 0.5 * max(target.size_x, target.size_y);
    float speed = cvar("vr_test_fling_speed") * VRMetersToUnits();
    vector at = mid - dir * (half + 40);
    vector vel = dir * speed;
    if(cvar("vr_test_fling_away"))
    {
        at = mid - dir * (half - 4);
        vel = -1 * vel;
    }
    prop.owner = world;
    prop.throwhit = 0;
    prop.carry_letgo = world;
    prop.vr_fling_hit = 0;
    prop.fg_state = 0;
    prop.flags = prop.flags - (prop.flags & FL_ONGROUND);
    setorigin(prop, at - 0.5 * (prop.mins + prop.maxs));
    prop.velocity = vel;
    dprint(sprintf("fling test: %s (%.1f kg) at %s, %.1f m/s (%.0f u/s)%s; its health %.0f\n", prop.classname,
        VR_Thrown_Mass(prop), target.classname, cvar("vr_test_fling_speed"), speed,
        cvar("vr_test_fling_away") ? ", away from it" : "", target.health));
};

void() forcegrabbable_touch =
{
    if(self.owner != world && other == self.owner)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    // Not thrown (or its throw over), but as fast into a monster or player as a throw: it hurts as one (VR_Prop_Flung:
    // a flail on the grapple's rope, a batted or knocked prop; their speed towards each other, never a resting contact,
    // nor one leaving it: a prop batted away still overlaps your box, and touchNearby's hit box meets you as it starts).
    VR_Prop_Flung(VR_Prop_Approach(self, other, self.velocity - other.velocity), VR_Prop_Approach(self, other, self.velocity));

    // A thrown weapon's true-scale gravity lasted for its flight only; rigid ones keep it (the
    // engine bounces them: a sudden Quake gravity after the first touch looked wrong).
    if(!self.vr_rigid)
    {
        self.gravity = 0;
    }

    // if(pointcontents(self.origin) == CONTENT_SKY)
    //     return;

    // Too slow to hurt what bleeds (come to rest against it, or pushed slowly into it; a rigid
    // body's landing never touches the world, so it can still be "thrown"): no blow, no sound, no
    // blood, and the throw is over.
    // (A heavy thing's lower: it can't be thrown as fast. VR_Thrown_Lenient.)
    float lenient = VR_Thrown_Lenient(self);
    if(self.throwhit == QVR_THROWHIT_NEVER_HIT && other.takedamage &&
       vlen(self.velocity) < cvar_hget(cvarh_vr_throw_hit_min_speed) * lenient)
    {
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        self.throwhit = QVR_THROWHIT_HIT;
    }

    if(self.throwhit == QVR_THROWHIT_NEVER_HIT)
    {
        float finalDmg = VR_Thrown_Damage(self, vlen(self.velocity), lenient);

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
    if(VR_AxeStick_IsStuck(self))
    {
        return; // stuck in something by its blade: bodies walking into it do nothing (vr_axestick.qc)
    }
    forcegrabbable_touch();
}

void(float xHand, entity ent, entity entPlayer,
     float* xOutEntWeapon, float* xOutEntWeaponFlags, float* xOutEntWeaponClip)
     wpnthrow_handtouch_impl =
{
    sound(ent, CHAN_AUTO, "weapons/pkup.wav", 1, ATTN_NORM);
    VRGunHaptic(xHand, 0.3, 75, 1.0);
    if(VR_AxeStick_IsStuck(ent))
    {
        VR_AxeStick_PulledOut(ent); // out of what it was stuck in (vr_axestick.qc)
    }

    if(cvar("developer"))
    {
        dprint(sprintf("weapon: %s taken into the %s hand, %g units from its handle\n", ent.netname,
            xHand == cVR_MainHand ? "main" : "off", vlen(VRGetEntHandPos(entPlayer, xHand) - ent.origin)));
    }

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

    if(!VRIsHandEmpty(entPlayer, xHand))
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
    if(cvar("vr_debug_axestick") >= 2 && VR_AxeStick_IsStuck(self))
    {
        dprint(sprintf("axestick: a hand (%g) on the stuck %s, grabbing %g\n", VRGetHandtouchHand(self), self.netname,
            VR_HandGrabUtil_StartedHandGrabbing(self.handtouch_ent, VRGetHandtouchHand(self))));
    }
    wpnthrow_handtouch_ent(self);
}

// A holster hotspot (QVR_HS_LEFT_SHOULDER_HOLSTER 3 .. QVR_HS_RIGHT_UPPER_HOLSTER 9, but for
// QVR_HS_HAND_SWITCH 7) as an index 0 .. 5 into holsterhover/holsterprevhover.
inline float(float xHolster) VRHolsterToIndex =
{
    return xHolster - QVR_HS_LEFT_SHOULDER_HOLSTER - (xHolster > QVR_HS_HAND_SWITCH);
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

// Once a frame: this frame's hovers become the last frame's (constant indices: plain fields, not
// the array accessor functions a variable index calls).
void(entity xEntPlayer) UpdateHolsterHover =
{
    xEntPlayer.holsterprevhover[0] = xEntPlayer.holsterhover[0];
    xEntPlayer.holsterprevhover[1] = xEntPlayer.holsterhover[1];
    xEntPlayer.holsterprevhover[2] = xEntPlayer.holsterhover[2];
    xEntPlayer.holsterprevhover[3] = xEntPlayer.holsterhover[3];
    xEntPlayer.holsterprevhover[4] = xEntPlayer.holsterhover[4];
    xEntPlayer.holsterprevhover[5] = xEntPlayer.holsterhover[5];
    xEntPlayer.holsterhover[0] = FALSE;
    xEntPlayer.holsterhover[1] = FALSE;
    xEntPlayer.holsterhover[2] = FALSE;
    xEntPlayer.holsterhover[3] = FALSE;
    xEntPlayer.holsterhover[4] = FALSE;
    xEntPlayer.holsterhover[5] = FALSE;
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
        // VR: a burst of vr_holster_haptic_time seconds (a quarter of one), strong as the hand
        // arrives and fading: three pulses, each taking over from the one before (a new vibration
        // replaces the one playing), so it is felt as one buzz dying away. (It was a faint 0.2
        // amplitude, hard to tell.)
        float t = max(0.05, cvar_hget(cvarh_vr_holster_haptic_time));
        haptic(xHand, 0.0, t * 0.45, 80, 0.65);
        haptic(xHand, t * 0.4, t * 0.35, 80, 0.4);
        haptic(xHand, t * 0.7, t * 0.3, 80, 0.2);
        dprint("holster haptic: hand "); dprint(ftos(xHand)); dprint(", holster "); dprint(ftos(xHolster));
        dprint(", "); dprint(ftos(rint(t * 1000))); dprint(" ms\n");
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

vector(vector xAVel) VRThrowSpinCapped;

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

    // A rigid body from the start (vr_box3d.cpp): it lands and rests on its drawn shape. As Quake's
    // toss (a monster's dropped gun, a weapon from an ammo box, the firing range's weapons) it landed on its box, a
    // cube round its origin that its model, turned any way, reaches well out of: it lay up to 20 units into the floor.
    // Its tumble goes on as the body's spin (radians a second; the throw's cap).
    newmis.vr_rigid = 1;
    newmis.vr_spin = VRThrowSpinCapped(xAVel * 0.0175);
    newmis.avelocity = '0 0 0';
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

// `xVelScale`: times the throw's velocity and spin (1: thrown; vr_weight_drop_velocity: wrenched out, the grip lost
// rather than a throw, which the throw assist does not aim either).
void(entity ent, float xHand, float xWeapon, float xWeaponFlags, float xWeaponClip, float xVelScale) DropWeaponInHandScaled =
{
    if(VRIsHandWeaponless(ent, xHand))
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
    vector asThrown = throwVel * m2u * VRThrowGain(vlen(throwVel)) * weight *
                      cvar_hget(cvarh_vr_weapon_throw_velocity_mult);
    vector throwVelUnits = asThrown * xVelScale;

    float throwGravity = cvar_hget(cvarh_sv_gravity);
    if(cvar_hget(cvarh_vr_throw_gravity) > 0)
    {
        throwGravity = cvar_hget(cvarh_vr_throw_gravity) * m2u;
    }

    // It left the hand a moment ago (the release lags the throw, and the move carrying it is
    // later still, by a frame or two: more at a low frame rate): start where it would be by now, not
    // where the hand followed through to. (It used to start at the hand when that was over 0.15 s, which
    // a low frame rate reached: those throws came out of the follow-through, lower and behind.) What it
    // would have flown into on the way, it starts at.
    vector handNow = VRGetHandPos(xHand);
    vector origin = handNow;
    float age = min(0.3, VRGetHandThrowAge(xHand));
    if(VRGetHandThrowPos(xHand) != '0 0 0' && age >= 0)
    {
        vector from = VRGetHandThrowPos(xHand);
        origin = from + throwVelUnits * age;
        origin_z = origin_z - 0.5 * throwGravity * age * age;
        throwVelUnits_z = throwVelUnits_z - throwGravity * age;

        traceline(from, origin, MOVE_HITMODEL_THROWN, ent);
        if(trace_fraction < 1 && trace_ent.takedamage && trace_ent != world)
        {
            origin = trace_endpos - normalize(origin - from) * 2; // a monster on the way: hit it next
        }
        traceline(handNow, origin, TRUE, ent); // never inside a wall
        origin = trace_endpos;
        if(trace_fraction < 1)
        {
            origin = origin - normalize(origin - handNow) * 2;
        }
    }

    if(xVelScale == 1)
    {
        throwVelUnits = VRThrowAssist(ent, origin, throwVelUnits, throwGravity);
    }

    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED); // a hand's state, not the weapon's
    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_SAW_STATE); // a chainsaw's engine stops

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
        thrown.vr_spin = VRThrowSpinCapped(VRGetHandAVel(xHand)) * xVelScale;

        // ("released": how long after the throw's estimate it left the hand, uncapped: throw_trigger_test.sh.)
        dprint(sprintf("thrown: velocity %v gravity %.2f spin %v age %.3f released %.3f scale %g of %v\n",
            thrown.velocity, thrown.gravity, thrown.vr_spin, age, VRGetHandThrowAge(xHand), xVelScale, asThrown));
        // (Its spin's axis against the hand's left, about which a wrist flick turns it end over end, its tip going
        // down: 0 for that, 90 for a spin about the handle or a cartwheel, 180 backwards: throw_spin_test.sh.)
        makevectors(VRGetHandRot(xHand));
        float spinRate = vlen(thrown.vr_spin);
        float spinCos = spinRate > 0.01 ? max(-1, min(1, normalize(thrown.vr_spin) * -v_right)) : 1;
        float spinOff = atan2(sqrt(1 - spinCos * spinCos), spinCos) * 57.29578;
        dprint(sprintf("thrown spin: %.2f rad/s, %.1f deg off end over end; hand %v, angles %v\n", spinRate, spinOff,
            VRGetHandRot(xHand), thrown.angles));
    }
    // (QVR_WEAPONTHROWMODE_DISCARD: nothing is thrown, the weapon is gone.)

    sound(ent, CHAN_BODY, "knight/sword2.wav", 0.7, ATTN_NORM);
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmoFor(xHand);
}

void(entity ent, float xHand, float xWeapon, float xWeaponFlags, float xWeaponClip) DropWeaponInHand =
{
    DropWeaponInHandScaled(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, 1);
}

void(entity entPlayer, float xHotspot, float xWeapon, float xWeaponFlags, float xWeaponClip)
VRPutWeaponInHolster =
{
    sound(entPlayer, CHAN_BODY, "weapons/holster1.wav", 1, ATTN_NORM);

    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED); // a hand's state, not the weapon's
    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_SAW_STATE); // a chainsaw's engine stops
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

    // The super shotgun, broken open, throws out its spent shells: flung by a flick reload
    // (vr_shells.cpp).
    if(xWeapon == WID_SUPER_SHOTGUN && missingAmmo > 0)
    {
        entity oself = self;
        self = entPlayer;
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, clamp(missingAmmo, 1, 2), 0,
            VR_HandGrabUtil_IsHandReloadFlicking(entPlayer, xHand) ? QVR_EJECT_FLICK : 0);
        self = oself;
    }
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

    if(VRIsHandWeaponless(ent, xHand) || xWeapon != lastWeapon)
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

// Where a weapon let go of falls (or flies) from the hand, rather than going in a holster or to the other hand
// (2H hotspots: unusable, as you can only dual wield with no weapon out; the grenade pouch holds grenades only).
float(float xHotspot) VRLetGoDropsAt =
{
    return xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB ||
           xHotspot == QVR_HS_CARRIED_GRIP ||
           xHotspot == QVR_HS_GRENADE_POUCH;
}

// Whether the weapon in `xHand` leaves it this frame (DoHandImpl's let-go: thrown, dropped or holstered), as
// VRWeaponLetGo will say, without its side effects. W_WeaponFrameImpl runs earlier in the frame: a trigger held through
// a throw fires no last shot as the weapon leaves.
float(entity ent, float xHand) VRWeaponLeavingHand =
{
    if(VRIsHandWeaponless(ent, xHand) || VR_HandGrabUtil_IsHandGrabbing(ent, xHand))
    {
        return FALSE;
    }

    if(cvar_hget(cvarh_vr_weapon_grip_mode) == 1)
    {
        float bit = xHand == cVR_MainHand ? 2 : 1;
        float lastWeapon = xHand == cVR_MainHand ? ent.stickyweaponmain : ent.stickyweaponoff;
        float armedNow = VR_HandGrabUtil_IsHandPrevGrabbing(ent, xHand) && (ent.stickyarmed & bit) &&
                         VRGetEntWeapon(ent, xHand) == lastWeapon;
        if(!(ent.stickypending & bit) && !armedNow)
        {
            return FALSE;
        }
    }

    float hotspot = xHand == cVR_MainHand ? ent.mainhand_hotspot : ent.offhand_hotspot;
    return VRLetGoDropsAt(hotspot) ||
           (isHolsterHotspot(hotspot) && getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE &&
            getHolsterWeapon(hotspot) == WID_FIST);
}

// A weapon has left `xHand` (thrown, dropped, holstered, handed over, wrenched out): its firing animation stops there
// (a nailgun's chain would fire on from the empty hand; the fist would show the gun's frames).
void(entity ent, float xHand) VRHandStopFiring =
{
    entity oself = self;
    self = ent;
    player_run(xHand);
    self = oself;
}

// Hand-off (vr_2h_handoff, docs/vr-port/ROUND16.md): the hand holding a weapon two-handed lets go while
// the other, empty hand still grips it (steadying it: VRGet2HAiming, or a moment ago). The weapon
// stays in that hand instead of falling: a sword (held by its grip with both hands) simply changes
// hands; a gun, held by its foregrip, hangs from it as it was held (QVR_WPNFLAG_FOREGRIP_CARRIED: the
// client draws it there; it doesn't fire or reload) until a hand takes its handle again. A hand that
// holds a weapon of its own is never helping, so dual wielding keeps dropping as before.
float(entity ent, float xHand, float xWeapon, float xWeaponFlags, float xWeaponClip) VRTryHandOff =
{
    float other = VRGetOtherHand(xHand);
    if(!cvar_hget(cvarh_vr_2h_handoff) ||
       (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED) ||
       !(VRGet2HAiming(ent) || time - ent.last_2h_time < 0.1) ||
       !VR_HandGrabUtil_IsHandGrabbing(ent, other) ||
       !VRIsHandEmpty(ent, other))
    {
        return FALSE;
    }

    float flags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED);
    if(xWeapon != WID_SWORD && xWeapon != WID_CROWBAR)
    {
        flags = flags | QVR_WPNFLAG_FOREGRIP_CARRIED;
    }

    VRSetEntWeapon(ent, other, xWeapon);
    VRSetEntWeaponFlags(ent, other, flags);
    VRSetEntWeaponClip(ent, other, xWeaponClip);
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmo();

    sound(ent, CHAN_BODY, "weapons/holster0.wav", 0.5, ATTN_NORM);
    VRGunHaptic(other, 0.08, 75, 0.6);
    return TRUE;
}

// The empty `xHand` closes on the handle of the gun the other hand carries by its foregrip: it holds
// the gun again, and the other hand is back on the foregrip (steadying it, if it still grips there).
void(entity ent, float xHand) VRTakeCarriedWeapon =
{
    float other = VRGetOtherHand(xHand);
    float flags = VRGetEntWeaponFlags(ent, other);

    VRSetEntWeapon(ent, xHand, VRGetEntWeapon(ent, other));
    VRSetEntWeaponFlags(ent, xHand, flags - (flags & QVR_WPNFLAG_FOREGRIP_CARRIED));
    VRSetEntWeaponClip(ent, xHand, VRGetEntWeaponClip(ent, other));
    VRSetHandEmpty(ent, other);
    W_SetCurrentAmmo();

    sound(ent, CHAN_BODY, "weapons/holster0.wav", 0.5, ATTN_NORM);
    VRGunHaptic(xHand, 0.08, 75, 0.6);
}

// Wrenched out (the engine's vr_weight_drop, vr_weight.cpp: experimental): the client says a hand's controller turned
// too fast for the heavy weapon it holds (a snap or a twist of the wrist). .handdrop's bits, each for a moment.
float VR_HANDDROP_OFF = 1;
float VR_HANDDROP_MAIN = 2;
void(float xHand) VRWeightWrenchFrame =
{
    float bit = xHand == cVR_MainHand ? VR_HANDDROP_MAIN : VR_HANDDROP_OFF;
    if(!(self.handdrop & bit) || self.health <= 0 || VRIsHandWeaponless(self, xHand))
    {
        return;
    }
    float wid = VRGetEntWeapon(self, xHand);
    float flags = VRGetEntWeaponFlags(self, xHand);
    float clip = VRGetEntWeaponClip(self, xHand);
    // Held two-handed, the other hand keeps it (the hand-off); else it falls: the grip lost, slower than a throw
    // (vr_weight_drop_velocity; NOTES.md vrfiringrange_2026-10-01_00-10).
    if(!VRTryHandOff(self, xHand, wid, flags, clip))
    {
        DropWeaponInHandScaled(self, xHand, wid, flags, clip, max(0, cvar_hget(cvarh_vr_weight_drop_velocity)));
    }
    VRHandStopFiring(self, xHand);
    VRGunHaptic(xHand, 0.15, 60, 1);
    sprint(self, "Too heavy for that: it is wrenched out of your hand!\n");
    dprint(sprintf("weight drop: weapon %g wrenched out of hand %g\n", wid, xHand));
};

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
    // (No weapon: the weapon's branches below. Empty: nothing carried either, what may take a weapon.)
    float handWeaponless = VRIsHandWeaponless(ent, xHand);
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

    float carried = !handWeaponless && (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED);

    if(VRIsEntHandReloadingBit(ent, xHand) && !carried)
    {
        VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
    }

    if(isHolsterHotspot(xHotspot))
    {
        VRSetHolsterHovered(ent, xHotspot, TRUE);
    }

    // Holding a weapon, but not grabbing the controller. At once, even mid-shot (the trigger held through a throw): it
    // used to wait for the shot's attack_finished, so a held trigger (a dry fire's click, a nailgun's chain) kept the
    // weapon in the hand until then, and it then jumped out to where the throw would have taken it
    // (NOTES.md e2m1_2026-09-30_23-41). Its firing animation stops below.
    if(!handWeaponless && letGo)
    {
        VRWeaponLetGoDone(ent, xHand);

        if(VRLetGoDropsAt(xHotspot))
        {
            // Held two-handed: the other hand keeps it (the hand-off). Otherwise it falls, or flies.
            if(!VRTryHandOff(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip))
            {
                DropWeaponInHand(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip);
            }
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
                *xOutOtherWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED);
                *xOutOtherWeaponClip = xWeaponClip;
                VRSetHandEmpty(ent, xHand);

                W_SetCurrentAmmo();
            }
        }

        if(VRGetEntWeapon(ent, xHand) != xWeapon)
        {
            VRHandStopFiring(ent, xHand);
        }
    }
    // Holding a weapon and hovering an holster. Possible intention to holster or reload.
    else if(!handWeaponless && !letGo) // holding it: gripping, or (vr_weapon_grip_mode 1) not let go of
    {
        if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) == WID_FIST && time >= xAttackFinished)
            {
                VRHolsterHaptic(ent, xHand, xHotspot);
            }

            if(VRIsWeaponReloadingEnabledFor(ent, xHand) && isReloadHolsterHotspot(xHotspot) && !carried)
            {
                VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags, xWeaponClip, xOutWeaponClip);
            }
        }
        else if(handReloadFlicking && !carried)
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
    else if(handEmpty && startedHandGrabbing)
    {
        // The handle of the gun the other hand carries by its foregrip: take it.
        if(xHotspot == QVR_HS_CARRIED_GRIP)
        {
            if(VRIsHandCarryingByForegrip(ent, VRGetOtherHand(xHand)))
            {
                VRTakeCarriedWeapon(ent, xHand);
            }
        }
        // Not a holster. Do nothing.
        else if(xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB ||
           xHotspot == QVR_HS_HAND_SWITCH ||
           xHotspot == QVR_HS_GRENADE_POUCH) // (a grenade from the pouch: VR_HandGrenade_HandFrame)
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

    VR_Carry_TwoHandFrame();
    VR_Carry_HandFrame(cVR_MainHand);
    VR_Carry_HandFrame(cVR_OffHand);
    VR_WallTorch_HandFrame(cVR_MainHand); // a torch gripped on its wall pulled out (vr_walltorch.qc)
    VR_WallTorch_HandFrame(cVR_OffHand);
    VR_HandGrenade_HandFrame(cVR_MainHand); // a grenade taken from the pouch at the small of the back (vr_grenade.qc)
    VR_HandGrenade_HandFrame(cVR_OffHand);

    UpdateHolsterHover(self);

    if(VRGet2HAiming(self))
    {
        // Used for 2H throws.
        self.last_2h_time = time;
    }

    VRWeightWrenchFrame(cVR_MainHand);
    VRWeightWrenchFrame(cVR_OffHand);

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
// A gun's shot under Quad (VR_QuadSound, combat.qc).
void() SuperDamageSound =
{
    VR_QuadSound(self, "a shot");
};


// TODO VR: (P0): Been playing the latest build in co-op.
//  We have a blast for about 15 minutes and then the player who is the
//  server will usually get a "packet overflow" error and crash.
//  Any easy work arounds to solve that issue?
// TODO: test with developer mode on, see if can reduce packet size
// TODO: VOIP seems to be the culprit!

// TODO VR: (P0): I tower I've the enemys and when I scale myself smaller
//  the hands break
