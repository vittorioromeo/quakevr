
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
    if(VR_FlareGun_Counter(xHand)) // the flare gun: its flares left (vr_flaregun.qc)
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
        VRSetWeaponClipSize(xHand, VR_ClipSizeIn(self, xHand)); // (AD support: the Widowmaker's tube)
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
// `xOwner` fired as a player (or a bot) or as an armed humanoid (vr_armedgrunt.qc: your weapon in its hands, its damage
// to you times vr_agrunt_damage_scale; it had the stock's, a second cut on Quake VR's own guns: the heavy revolver's 15
// for your 26, the nail pistol's 9 for 12, the flare gun's 6 for 10); `xStock` (id's, or the mission pack's) for another
// monster's or a trap's.
float(entity xOwner, float xCvarh, float xStock) VR_WpnDmg =
{
    if((xOwner.flags & FL_CLIENT) || xOwner.vr_ag)
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
    // An incandescent blade's blow hits harder (vr_hotblade_damage more base damage: vr_hotblade.qc).
    xBase = xBase + VR_HotBlade_Extra(self, xHand);
    if(!VRHandsTracked(self) || !self.ishuman)
    {
        return 20 + VR_HotBlade_Extra(self, xHand);
    }

    // The damage caps bound this blow's strength (times its zone's multiplier) when it lands (T_Damage_VRMelee).
    vr_mdmg_core = VR_Blow(xHand).mh_core;
    vr_mdmg_hand = xHand;
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
// `left`: to its left (0 but for a head off its middle: AD's zombie knight's), the head's middle at origin +
// facing * forward + VR_HeadSide(facing) + up.
float xHeadForward;
float xHeadUp;
float xHeadRadius;
float xHeadLeft;
vector(vector xFacing) VR_HeadSide =
{
    vector s = '0 0 0';
    s_x = -xFacing_y;
    s_y = xFacing_x;
    return s * xHeadLeft;
};
float(entity xTarget) PositionalHead =
{
    string c = xTarget.classname;
    xHeadLeft = 0;
    if(xTarget.vr_mg3_mon && MG3_PositionalHead(xTarget)) { return TRUE; } // (QVR: Dawn of the Machine's own models: vr_mg3_ogre.qc)
    if(xTarget.vr_ad_mon) { return VR_AD_PositionalHead(xTarget); } // AD support: AD's monsters' heads (vr_ad_monsters.qc)
    if(c == "monster_army")             { xHeadForward = 3;  xHeadUp = 23; xHeadRadius = 7.2; return TRUE; }
    if(c == "monster_ogre" || c == "monster_ogre_marksman") { xHeadForward = 9; xHeadUp = 33; xHeadRadius = 10; return TRUE; }
    if(c == "monster_enforcer")         { xHeadForward = 2;  xHeadUp = 27; xHeadRadius = 7.4; return TRUE; }
    if(c == "monster_gremlin")          { xHeadForward = 7;  xHeadUp = 12; xHeadRadius = 6.4; return TRUE; }
    if(c == "monster_hell_knight")      { xHeadForward = 6;  xHeadUp = 33; xHeadRadius = 7.2; return TRUE; }
    if(c == "monster_knight")           { xHeadForward = 4;  xHeadUp = 22; xHeadRadius = 6.2; return TRUE; }
    if(c == "monster_shambler")         { xHeadForward = 14; xHeadUp = 57; xHeadRadius = 12;  return TRUE; }
    // Dawn of the Machine's super shambler (MG3's model, $stand1: its face at the hump's front, 33-72 up; its rig's head).
    if(c == "monster_super_shambler")   { xHeadForward = 30; xHeadUp = 53; xHeadRadius = 13;  return TRUE; }
    // A vore's: Quake VR's own model (progs/shalrath.mdl, taller than id's): his head 40-53 above his origin (round 21).
    if(c == "monster_shalrath")         { xHeadForward = 0;  xHeadUp = 45; xHeadRadius = 8;   return TRUE; }
    if(c == "monster_zombie")           { xHeadForward = 0;  xHeadUp = 22; xHeadRadius = 6;   return TRUE; }
    if(c == "monster_wizard")           { xHeadForward = 6;  xHeadUp = 25; xHeadRadius = 7;   return TRUE; }
    if(c == "monster_mummy")            { xHeadForward = 0;  xHeadUp = 30; xHeadRadius = 6.5; return TRUE; }
    // A fiend's ($stand1: crouched, his head thrust forward, below his origin; decapitation's zone since round 21). Below
    // his origin it is his legs' zone too: Head Priority (vr_hit_head_priority, on by default) gives it to the head.
    if(c == "monster_demon1")           { xHeadForward = 27; xHeadUp = -5; xHeadRadius = 8;   return TRUE; }
    // A centroid's (Hipnotic's monster_scourge, $stand1): the front of its body, low and wide, below its origin (its legs'
    // zone too: Head Priority, as the fiend's).
    if(c == "monster_scourge")          { xHeadForward = 11; xHeadUp = -9; xHeadRadius = 8;   return TRUE; }
    // A rottweiler's: his model's frames are unnamed, so his rest pose (hitmodel_rest's stand frame) is the first, $attack1
    // (lunging, his head thrust forward and level with his origin; decapitation's zone since round 20, its one source now).
    if(c == "monster_dog")              { xHeadForward = 23; xHeadUp = 1;  xHeadRadius = 7;   return TRUE; }
    if(c == "player")                   { xHeadForward = 4;  xHeadUp = 23; xHeadRadius = 7.2; return TRUE; }
    // Dawn of the Machine's lava man (MG3's model, $walk1: its head forward of its shoulders, 61-79 up; Rogue's has none).
    if(c == "monster_lava_man" && xTarget.mg3_lavaman) { xHeadForward = 31; xHeadUp = 70; xHeadRadius = 13; return TRUE; }
    // Dawn of the Machine's Chthon (id's model, $walk1: his head and jaw 210-276 up, forward of his chest; id's has none).
    if(c == "monster_boss" && xTarget.mg3_chthon) { xHeadForward = 80; xHeadUp = 240; xHeadRadius = 45; return TRUE; }
    return FALSE;
};

float VR_HEAD_MARGIN = 3.5; // units: the head spheres are fitted to the top vertices' middle; a head's surface lies up to 3.5 beyond (an ogre's back of the head)

// Which zone a point that lies in more than one belongs to (1 head, 2 extremities, 3 legs, 0 body). Head priority
// (vr_hit_head_priority, Combat > Hit Detection > Head Priority): the head's zone wins the overlap - a head is a head
// whatever else reaches it. Off: the limb and leg zones claim the point first, and the head only where nothing else
// does. The head sphere is not grown for it: only which claim is taken changes.
float(float inHead, float outLateral, float belowOrigin) VR_RegionPick =
{
    if(cvar_hget(cvarh_vr_hit_head_priority))
    {
        if(inHead)
        {
            return 1; // headshot
        }
        if(outLateral)
        {
            return 2; // extremities
        }
        if(belowOrigin)
        {
            return 3; // legs
        }
        return 0; // standard
    }

    if(outLateral)
    {
        return 2; // extremities
    }
    if(belowOrigin)
    {
        return 3; // legs
    }
    if(inHead)
    {
        return 1; // headshot
    }
    return 0; // standard
}

// Point classification shared by precise shots, positional melee and the animated debug view's check.
// `rest` is the contacted point mapped to the standing model, in world space (hitmodel_rest).
float PositionalPointRegion(entity xTarget, vector rest) =
{
    if(!PositionalHead(xTarget)) { return -1; }
    float yaw = xTarget.angles_y * (3.14159265 / 180);
    vector facing = '0 0 0';
    facing_x = cos(yaw); facing_y = sin(yaw);
    vector head = xTarget.origin + facing * xHeadForward + VR_HeadSide(facing) + '0 0 1' * xHeadUp;
    vector side = '0 0 0';
    side_x = -facing_y; side_y = facing_x;
    float inHead = vlen(rest - head) < xHeadRadius + VR_HEAD_MARGIN;
    float lateral = fabs((rest - xTarget.origin) * side) > (xTarget.maxs_x - xTarget.mins_x) * 0.35;
    return VR_RegionPick(inHead, lateral, rest_z < xTarget.origin_z);
}

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
    vector head = xTarget.origin + facing * xHeadForward + VR_HeadSide(facing) + '0 0 1' * xHeadUp;

    // Precise hits (vr_hit_precise): the shot struck the model as drawn (at xEndpos, within its tolerance of it). Where
    // that place is on the model standing (hitmodel_rest: the same place of the same triangle in its stand frame, at its
    // origin, turned with its yaw) decides: in the head sphere (and VR_HEAD_MARGIN: the sphere is fitted to the head's
    // middle), far out from its middle, below its origin. A monster crouched, in pain or lying dead keeps its head.
    if(hitmodel_target(xTarget))
    {
        vector rest = hitmodel_rest(xTarget, xEndpos);
        return PositionalPointRegion(xTarget, rest);
    }

    // The ray's closest approach to the head, not behind where it entered.
    float t = max(0, (head - xEndpos) * d);
    float inHead = vlen(head - (xEndpos + d * t)) < xHeadRadius;

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

    return VR_RegionPick(inHead, vlen(off) > (xTarget.maxs_x - xTarget.mins_x) * 0.35, q_z < xTarget.origin_z);
}

// A melee blow's positional damage (vr_melee_positional, Combat > Damage and Knockback > Positional Damage): the same
// zones and the same multipliers the guns use, decided from the zero-radius point on the model the blow met
// (PositionalRegion, hitmodel_rest) - so a head is a head for a blade and for a shot alike. The looser melee
// tolerance (vr_hit_tolerance_melee, vr_hit_melee_scale) stays what *connects* only: it never makes a body hit count
// as a head. Head priority resolves overlapping zones at the contacted point. Its answer is recorded as a shot's (the
// dummy's report, the highlights). With the option off: 1, nothing recorded - today's flat melee damage.
float(entity xTarget, vector xTo, vector xDir) VR_Melee_Positional =
{
    if(cvar_hget(cvarh_vr_melee_positional) == 0 || cvar_hget(cvarh_vr_positional_damage) == 0)
    {
        return 1;
    }

    float region = PositionalRegion(xTarget, xTo, xDir);
    if(region < 0)
    {
        return 1;
    }

    vr_hitpart = region;
    vr_hitpart_target = xTarget;
    vr_hitpart_time = time;
    vr_hitpart_pos = xTo;
    return PositionalDamageFactor(region);
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

    VR_SoundVaried(shooter, CHAN_AUTO, "vr/headshot.wav", min(1, v), ATTN_NORM);
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
float vr_laser_bounces, vr_laser_bounce_ratio, vr_laser_stops; // the laser cannon's bolts' bounces and stops (tests)

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
        vector at = trace_ent == other ? trace_endpos : self.origin;
        posDmgType = PositionalDamage(other, at, v_forward);
        adjDmg = self.dmg * PositionalDamageFactor(posDmgType);

        // VR: every projectile's hit (VR_Proj_Begin's, its bounces its own): tagged a shot where it struck (a corpse's
        // head or limb worn), a kill at the head or a limb popped by chance (vr_decap_pop_laser).
        VR_SetHitKind(QVR_HITKIND_SHOT, -1, at, oldvel);
        float pop = VR_Decap_ProjArm(other, self.lastvictim, at, oldvel, adjDmg, QVR_PROJ_LASER);
        T_Damage(other, self, self.lastvictim, adjDmg);
        if(pop)
        {
            VR_Decap_After(other);
        }
        VR_ClearHitKind();
    }
    else if((self.cnt == 3) || (random()<0.15))
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_GUNSHOT);
        WriteVec3(MSG_BROADCAST, org);
    }
    else
    {
       vr_laser_bounces++; // (tests: vr_mg3_weapons_test.qc)
       vr_laser_bounce_ratio = 0.9;
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

    vr_laser_stops++;
    if(cvar_hget(cvarh_developer) > 1) { dprint(sprintf("laser bolt stopped: touch %g on %s (health %g)\n", self.cnt, other.classname, other.health)); }
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

// A player's projectile just launched from hand `xHand` (self the player; every weapon's, the expansions' and the
// enemy guns' too): credited with the hand's weapon (vr_dmgcredit.qc), and placed behind what the muzzle is inside.
void(entity xMissile, float xHand) VR_ShotLaunched =
{
    VR_DamageCredit_FromHand(xMissile, self, xHand);
    VR_ShotPlace(xMissile, xHand);
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
    traceline(xFrom, xTo, VR_HitModelFlags(self) | MOVE_HITGIBS, self);
    if(trace_startsolid && trace_ent != other && trace_ent != world)
    {
        traceline(self.origin, xTo, VR_HitModelFlags(self) | MOVE_HITGIBS, self);
    }
};

// A bolt's damage (self: who fires it): Weapon Damage's Laser Cannon (18; one in ten lit, `xLit`, 25/18 of it), and in Dawn
// of the Machine (campaign 5) its own laser cannon's 15 and 20 (official mg3_weapons.qc HIP_LaunchLaser: the same
// weapon, the same bouncing bolts, less damage; MG3_LaserScale, vr_mg3_weapons.qc).
float(float xLit) VR_LaserBoltDamage =
{
    if(xLit)
    {
        return 25 * VR_WpnDmgScale(self, cvarh_vr_dmg_laser, 18) * MG3_LaserScale(TRUE);
    }
    return VR_WpnDmg(self, cvarh_vr_dmg_laser, 18) * MG3_LaserScale(FALSE);
};

entity(vector org, vector vec, vector extraVel, float light) HIP_LaunchLaser =
{
    sound(self, CHAN_WEAPON, "hipweap/laserg.wav", 1, ATTN_NORM);

    vec = VR_AimCone_Bend(normalize(vec), VR_AIM_NARROW); // QVR: a monster's bent to its facing's aim cone (vr_aimcone.qc)

    entity newmis;
    newmis = spawn();
    newmis.owner = self;
    VR_AI_Tag(newmis);
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
    newmis.dmg = VR_LaserBoltDamage(FALSE);
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
        VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
        org = org - (2*aofs*v_right);
        newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
        VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    }
    else if(stat == 1)
    {
        delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 1);

        org = org + (ofs*(v_up * 0.4));

        if(random() < 0.1)
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 1);
            VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
            newmis.dmg = VR_LaserBoltDamage(TRUE);
        }
        else
        {
            newmis = HIP_LaunchLaser(org, dir, extraVel, 0);
            VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
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
   VR_DamageCredit_Inherit(light, own); // (the chain's credit: its base's)
   light.v_angle = self.angles;
   light.v_angle_x = 0;
   light.v_angle_z = 0;
   light.origin = own.origin;
   light.think = HIP_LightningThink;
   light.nextthink = time;
   };

// xHand: the hand whose weapon struck it (Mjolnir, the Super Axe's burst): the chain's credit.
void(float xHand) HIP_SpawnMjolnirBase =
   {
   entity light;
   // spawn lightning base
   light = spawn();
//   light.origin = self.origin - '0 0 24' + (32*v_forward);
   light.origin = trace_endpos;
   light.flags = 0;
   light.owner = self;
   VR_DamageCredit_FromHand(light, self, xHand);
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
        VR_DamageCredit_FromHand(self, self, xHand); // (the discharge's inflictor: the player)
        T_RadiusDamage(self, self, 35*cells, world);
        discharged = 0;
        W_SetCurrentAmmo();
        return;
    }

    delAmmoOrClipAndUpdateCounter(xHand, AID_CELLS, 15);

    HIP_SpawnMjolnirBase(xHand);
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
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        VR_HitBlood(trace_ent, org, v_forward, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
        if((vr_melee_blunt || VR_Hit_Metal(victim)) && VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VR_SoundVaried(self, VRGetGunChannel(xHand), VR_Melee_HitSoundOn(victim, "", FALSE), 1, ATTN_NORM); // the head's hit has none
        }
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    else
    {
        // hit wall
        if(trace_fraction != 1.0)
        {
            VR_Melee_WallMark(xHand, xdmg, TRUE); // a hammer's dent (vr_decal_melee)
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
                VR_SoundVaried(self, VRGetGunChannel(xHand), "knight/sword1.wav", 1, ATTN_NORM);
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

    // HIPNOTIC and ROGUE weapons: Quake VR ships their sounds (the pack's lava gun cooldown when it is there)
    precache_sound("enforcer/enfstop.wav");   // laser cannon
    precache_sound("hipweap/laserg.wav");   // laser cannon
    precache_sound("hipweap/laserric.wav");   // laser cannon ricochet
    precache_sound("hipweap/proxwarn.wav");   // proximity bomb
    precache_sound("hipweap/proxbomb.wav");   // proximity bomb
    precache_sound("hipweap/mjolhit.wav");   // mjolnir
    precache_sound("hipweap/mjolslap.wav");   // mjolnir
    precache_sound("hipweap/mjoltink.wav");   // mjolnir
    //ZOID-- grapple
    precache_sound("weapons/chain1.wav");
    precache_sound("pendulum/hit.wav");
    precache_sound("vr/grapple_reel.wav"); // the reel winding in (vr_grapple.qc)
    precache_sound("vr/grapple_unreel.wav"); // the unreel paying out (vr_grapple.qc)
    precache_sound("vr/grapple_taut.wav"); // the rope snapping taut
    if(VR_Has("sound/lavagun/snail.wav")) { precache_sound("lavagun/snail.wav"); } // lava nail gun cooldown

    // QUAKE VR
    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");
    precache_sound("weapons/holster0.wav");
    precache_sound("weapons/holster1.wav");
    precache_sound("vr/flashlight_attach.wav"); // (a magnetic holster bound, unbound: vr_magholster.qc)
    precache_sound("vr/flashlight_detach.wav");
    precache_sound("vr/magholster_zip.wav"); // (a bound weapon reeled home, drawn: vr_magholster.qc)
    precache_sound("vr/magholster_draw.wav");
    precache_sound("vr/blade_sizzle.wav"); // incandescent blades (vr_hotblade.qc)
    precache_sound("vr/blade_hot.wav");
    precache_sound("vr/blade_quench.wav");
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
    float  bleed;
    bleed = FALSE;

    xdmg = VRMeleeDmg(xHand, cvar_hget(cvarh_vr_dmg_axe) * AD_AxeScale(xHand)); // AD support: the Shadow Axe (vr_ad_bridge.qc)

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
        AD_AxeMark(xHand, trace_ent); // AD support: the Shadow Axe's mark, for AD's monsters (vr_ad_bridge.qc)
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)

        // HONEY
        VR_DamageCredit_SetWeapon(self, WID_AXE);

        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            VR_HitBloodSplash(victim, org, xdmg);
            bleed = TRUE;
        }

        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "fisthit.wav", TRUE), 1.0, ATTN_NORM);
            VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), victim);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
    }

    if(!bleed)
    {
        // hit wall
        VR_Melee_WallMark(xHand, xdmg, FALSE); // its head's chop, its handle's dent (vr_decal_melee)
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
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
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        VR_HitBloodSplash(trace_ent, org, xdmg);
        T_Damage_VRMelee (trace_ent, self, self, xdmg);

        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "fisthit.wav", FALSE), 1.0, ATTN_NORM);
            VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), victim);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
    }
    else
    {
        // hit wall
        VR_Melee_WallMark(xHand, xdmg, TRUE); // a gun's dent (vr_decal_melee)
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
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
// hell knight's (weaponflags) harder still. AD support: AD's melee monsters' weapons (their kind in the weapon flags:
// vr_ad_guns_defs.qc) their own (vr_admelee_sword_damage; the hammer and the mace vr_admelee_heavy_damage in both hands,
// vr_admelee_heavy_1h of it in one; blunt: a club's knock, client.qc's vr_melee_blunt).
void(float xHand) W_SwordMelee =
{
    float xdmg = VRMeleeDmg(xHand, 20) * cvar_hget(cvarh_vr_sword_damage_mult);
    float kind = VR_ADMelee_Kind(VRGetWeaponFlags(xHand));
    if(VR_ADMelee_Flail(kind))
    {
        xdmg = vr_flail_damage; // (its head's blow: VR_Flail_Hit)
    }
    else if(VR_ADMelee_Heavy(kind))
    {
        float both = VRGet2HAiming(self) && VRIsHandEmpty(self, VRGetOtherHand(xHand));
        xdmg = xdmg * cvar("vr_admelee_heavy_damage") * (both ? 1 : cvar_hget(cvarh_vr_admelee_heavy_1h));
    }
    else if(kind)
    {
        xdmg = xdmg * cvar("vr_admelee_sword_damage");
    }
    else if(VRHasBit(VRGetWeaponFlags(xHand), QVR_WPNFLAG_USE_SECONDARY_AMMO))
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
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        VR_DamageCredit_SetWeapon(self, WID_SWORD);
        if(T_Damage_VRMelee(trace_ent, self, self, xdmg))
        {
            VR_HitBloodSplash(victim, org, xdmg);
        }
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "knight/sword2.wav", TRUE), 1.0, ATTN_NORM);
            if(VR_ADMelee_Blunt(kind) || VR_ADMelee_Flail(kind))
            {
                VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), victim); // (a hammer's, a mace's weight: the chainsaw's)
            }
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    // A wall: a clang and sparks, its blade's groove (its pommel's dent: vr_decal_melee).
    VR_Melee_WallMark(xHand, xdmg, FALSE);
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
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
        entity victim = trace_ent; // (T_Damage may trace: an explosive box blowing up)
        VR_DamageCredit_SetWeapon(self, WID_CHAINSAW);
        // Swung running, its bar sets a grenade off as its chain does (VR_GrenShot_BlowSetsOff); not its rear handle.
        vr_grenshot_saw = ((VRGetWeaponFlags(xHand) & QVR_WPNFLAG_SAW_RUNNING) &&
                           VR_Blow(xHand).mh_kind != VR_MKIND_POMMEL);
        float hurt = T_Damage_VRMelee(trace_ent, self, self, xdmg);
        vr_grenshot_saw = FALSE;
        if(hurt)
        {
            VR_HitBloodSplash(victim, org, xdmg);
        }
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSoundOn(victim, "fisthit.wav", TRUE), 1.0, ATTN_NORM);
            VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), victim);
            VRSetMeleeHitSoundPlayed(xHand, TRUE);
        }
        return;
    }

    // A wall: a clang and sparks, its bar's groove (its engine's dent: vr_decal_melee).
    VR_Melee_WallMark(xHand, xdmg, FALSE);
    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        VRMeleeHapticByHand(xHand);
        VR_SoundVaried(self, VRGetHandChannel(xHand), "player/axhit2.wav", 1, ATTN_NORM);
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
    CreateThrownWeapon(self, WID_SWORD, xHellKnight ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0, WeaponInst_New(0),
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

    if(VRGetMeleeHitSoundPlayed(xHand) == FALSE && vr_melee_slap)
    {
        // A slap (vr_melee_slap; only on what takes damage: VR_Melee_SlapLands): skin on skin, no knock.
        VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Melee_HitSound(""), 1.0, ATTN_NORM);
        playedsound = TRUE;
    }
    else if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
    {
        // (A held rock or brick knocks as it does; a fist on an explosive box, a metal knock: VR_Melee_HitSoundOn.)
        VR_SoundVaried(self, VRGetHandChannel(xHand), VR_Debris_Is(held) ? VR_Debris_Sound(held) :
            (VR_Hit_Metal(trace_ent) ? VR_Melee_HitSoundOn(trace_ent, "fisthit.wav", FALSE) : "fisthit.wav"), 1.0, ATTN_NORM);
        if(!VR_Debris_Is(held))
        {
            VR_Blunt_HitLayer(self, VRGetHandChannel(xHand), trace_ent);
        }
        playedsound = TRUE;
    }

    if(trace_ent.takedamage)
    {
        VRMeleeHapticByHand(xHand);
        trace_ent.axhitme = 1;
        if(!vr_melee_slap)
        {
            VR_HitBloodSplash(trace_ent, org, xdmg); // (a slap draws no blood)
        }
        T_Damage_VRMelee (trace_ent, self, self, xdmg);
    }
    else
    {
        // hit wall
        VR_Melee_WallMark(xHand, xdmg, TRUE); // a held prop's dent; a bare fist none (vr_decal_melee)
        if(VRGetMeleeHitSoundPlayed(xHand) == FALSE)
        {
            VRMeleeHapticByHand(xHand);
            // A secret brush remains metal after activation has disabled its damage.
            if(VR_Hit_Metal(trace_ent))
            {
                VR_HitBloodSplash(trace_ent, org, xdmg);
            }
            else
            {
                PainSound();
                SpawnBloodSplash (org, xdmg);
            }
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
float vr_blood_sent; // blood particles spawned (the small gibs' tests count them)

void(vector org, vector vel, float damage) SpawnBlood =
{
    vr_blood_sent++;
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
    // VR: what it touched is metal (an explosive box): sparks and a tink (vr_crates.qc).
    if(VR_Hit_Metal(other))
    {
        VR_Metal_Hit(other, self.origin + vel * 0.01, vel, damage, VR_METAL_SHOT);
        return;
    }
    // VR: what it touched is wood (a crate): splinters, not blood (vr_crates.qc).
    if(VR_Crate_Wooden(other))
    {
        VR_Crate_Splinters(self.origin + vel * 0.01, vel, damage);
        return;
    }
    if(other.vr_shootnade) // VR: a grenade (vr_grenade.qc, "Shooting grenades"): sparks
    {
        VR_GrenShot_Sparks(self.origin);
        return;
    }
    if(VR_WallTorch_IsTorch(other)) // the identity survives being knocked off by this hit
    {
        return;
    }
    // VR: an armed humanoid's unbroken plate: sparks, blood only for what it lets through (vr_agarmor.qc).
    float bleed = VR_AgArmor_HitFx(other, self.origin, vel, damage);
    if(bleed > 0)
    {
        SpawnBlood (self.origin + vel * 0.01, vel, damage * bleed);
    }
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
    VR_AgArmor_PelletsClear(); // (armour: each pellet's piece, vr_agarmor.qc)
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
    entity t = multi_ent;
    float pop = VR_Decap_BlastArm(t); // VR: a headshot blast pops the head if it kills (vr_decap.qc, "Head pops")
    VR_Decap_Damage(t, self, self, multi_damage, pop);
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
    VR_AgArmor_PelletAdd(hit); // (armour: the pellet's piece, vr_agarmor.qc)

    // HONEY
    return VR_DamageCredit_Accepts(hit, self); // (the shooter's hitscan shot: FireBullets credited him)
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

// ----------------------------------------------------------------------------
// Projectile hits: one path for every projectile's touch (ROUND22.md, "Projectiles: one path"): id1's nails, rockets,
// grenades and lasers, the expansions', AD's, Dawn of the Machine's and the enemy guns' (the inventory: ROUND22.md).
// The projectile (self) meeting `other`, its touch calls VR_Proj_Begin before it deals its hit, VR_Proj_End after:
// - the prop, gib or ragdoll (a corpse's, a knocked-down body's) it met or flew through pushed (VR_ShotPush: a nail's
//   push, or its own kind's, .vr_shot_push_kind); an explosive's blast pushes instead (physicsblast);
// - where it struck, along its flight (VR_TouchTrace: vr_proj_at, vr_proj_dir), and the body's part there
//   (PositionalDamage: vr_proj_factor, the headshot's crack); the hit tagged a shot there (VR_SetHitKind), so a corpse's
//   head or limb it strikes is worn down (VR_Limb_CorpseWear);
// - a kill at the head or a limb armed to pop it (VR_Decap_ProjArm: a laser's or a plasma bolt's chance
//   vr_decap_pop_laser, any other bolt's, nail's or lump's vr_decap_pop_projectile; an explosive's are its blast's,
//   vr_limbs_blast_chance), ended by VR_Proj_End (VR_Decap_After).
// A wall torch the hit's damage reaches comes off its wall (T_Damage: VR_WallTorch_Shot; AD's T_Damage hands ours to
// ours, VR_AD_DamageOurs); a grenade bounces off it, knocking it off (GrenadeTouch). The projectile's move meets corpses,
// gibs, knocked-down bodies and wall torches' shot boxes: a flying missile's always (MOVE_MISSILE), a small
// MOVETYPE_FLY one's and a grenade's when marked (VR_Proj_Mark: the engine's VR_HitModelMoveFlags, MOVE_HITGIBS).
float QVR_PROJ_BOLT = 1;  // a nail, a spike, a bolt, a lump of flesh or rock: pops at vr_decap_pop_projectile
float QVR_PROJ_LASER = 2; // a laser or a plasma bolt: pops at vr_decap_pop_laser
float QVR_PROJ_BLAST = 3; // an explosive's direct hit (a rocket, a grenade, a fireball, a voreball): no pop of its own

entity vr_proj_target; // what the last VR_Proj_Begin's projectile met
vector vr_proj_at;     // where it struck it (on its model, else where the projectile is)
vector vr_proj_dir;    // its flight's way
float vr_proj_factor;  // the part's damage factor there (PositionalDamageFactor; 1 for what takes no damage)
float vr_proj_armed;   // a pop armed (VR_Proj_End ends it)
float vr_proj_hits;    // VR_Proj_Begin's calls (the tests)

float(entity targ, entity attacker, vector xAt, vector xDir, float xDamage, float xKind) VR_Decap_ProjArm; // (vr_limbs.qc)

// `e`, a projectile just launched, marked one (its move meets corpses, gibs, knocked-down bodies: the engine's
// VR_HitModelMoveFlags), for a small MOVETYPE_FLY one (AD's bolts, lasers and plasma) or a grenade.
void(entity e) VR_Proj_Mark =
{
    e.vr_projectile = 1;
};

// Body shields (ROUND22.md, "Body shields"): a corpse or a knocked-down monster a player holds in either hand (its
// ragdoll, one hand or two) takes a monster's shot or projectile on its way to him. Held up against him, the body lies
// inside his own hit box, whose face the shot met first, so the shot passed through it. `xP`, the player the path
// xA..xB met: the held body's drawn shape is looked for along the path up to where it passes his middle; met there, it
// is what the shot hit instead (trace_ent, trace_endpos, trace_fraction of xA..xB). TRUE if so.
float(entity xP, vector xA, vector xB) VR_BodyShield_Take =
{
    if(!(xP.flags & FL_CLIENT) || xP.health <= 0)
    {
        return FALSE;
    }
    entity held1 = VR_Carry_Held(xP, cVR_MainHand);
    entity held2 = VR_Carry_Held(xP, cVR_OffHand);
    if(!isragdoll(held1) && !isragdoll(held2))
    {
        return FALSE;
    }
    vector dir = normalize(xB - xA);
    float along = (xP.origin - xA) * dir; // (to his middle)
    float len = vlen(xB - xA);
    if(along <= 0 || len <= 0)
    {
        return FALSE;
    }
    vector mid = xA + dir * along;
    entity best = world;
    float bestT = 2;
    for(float i = 0; i < 2; i++)
    {
        entity h = i ? held2 : held1;
        if(h == world || !isragdoll(h) || !h.takedamage || (i && h == held1))
        {
            continue;
        }
        float t = hitmodel_segment(h, xA, mid, 0, HITCLASS_GUNS);
        if(cvar_hget(cvarh_vr_debug_shots) >= 2)
        {
            dprint(sprintf("shot: body shield: %s from %v to %v (his middle): %g\n", h.classname, xA, mid, t));
        }
        if(t >= 0 && t < bestT)
        {
            best = h;
            bestT = t;
        }
    }
    if(best == world)
    {
        return FALSE;
    }
    trace_ent = best;
    trace_endpos = xA + (mid - xA) * bestT;
    trace_fraction = vlen(trace_endpos - xA) / len;
    trace_plane_normal = '0 0 0' - dir;
    trace_allsolid = FALSE;
    trace_startsolid = FALSE;
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("shot: %s held by %s takes it (body shield) at %v\n", best.classname, xP.netname, trace_endpos));
    }
    return TRUE;
};

// Before the projectile (self) deals its hit on `other` (`xKind` QVR_PROJ_*, `xDamage` its damage, if known: a zombie's
// head pops only to a hit that would knock it down).
void(float xKind, float xDamage) VR_Proj_Begin =
{
    vr_proj_hits++;
    vector dir = normalize(self.velocity);
    if(dir == '0 0 0')
    {
        makeforward_mdl(self.angles);
        dir = v_forward;
    }
    // A monster's projectile meeting a player: the body he holds before him takes it (VR_BodyShield_Take).
    if((other.flags & FL_CLIENT) && !(self.owner.flags & FL_CLIENT) &&
       VR_BodyShield_Take(other, self.origin - dir * 64, self.origin + dir * 64))
    {
        other = trace_ent;
        setorigin(self, trace_endpos);
    }
    if(self.vr_shot_push_kind)
    {
        VR_ShotPush_Touch(); // (its own kind's push, or a prop it flew through)
    }
    else if(xKind != QVR_PROJ_BLAST)
    {
        VR_ShotPush(other, self.origin, self.origin, dir, QVR_SHOTPUSH_NAIL, 1);
    }
    VR_TouchTrace(self.origin - dir * 8, self.origin + dir * 8);
    vr_proj_target = other;
    vr_proj_at = trace_ent == other ? trace_endpos : self.origin;
    vr_proj_dir = dir;
    vr_proj_factor = 1;
    vr_proj_armed = FALSE;
    if(!other.takedamage || other == world)
    {
        return;
    }
    vr_proj_factor = PositionalDamageFactor(PositionalDamage(other, vr_proj_at, dir));
    if(xKind == QVR_PROJ_BLAST)
    {
        return;
    }
    VR_SetHitKind(QVR_HITKIND_SHOT, -1, vr_proj_at, dir);
    vr_proj_armed = VR_Decap_ProjArm(other, self.owner, vr_proj_at, dir, xDamage, xKind);
    if(cvar_hget(cvarh_vr_debug_shots))
    {
        dprint(sprintf("projectile hit: %s (%s) at %s at %v, part factor %.2f, pop armed %g\n", self.classname,
            ftos(xKind), other.classname, vr_proj_at, vr_proj_factor, vr_proj_armed));
    }
};

// After its hit: the armed pop ended, the hit's tag cleared.
void() VR_Proj_End =
{
    if(vr_proj_armed)
    {
        VR_Decap_After(vr_proj_target);
        vr_proj_armed = FALSE;
    }
    if(vr_hitkind == QVR_HITKIND_SHOT && vr_hithand == -1 && vr_hitpos == vr_proj_at)
    {
        VR_ClearHitKind();
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
        VR_Decap_Pellet(trace_ent, damage, trace_endpos, dir); // VR: the blast's pellets at the head (vr_decap.qc)
        if(AddMultiDamage(trace_ent, damage) && trace_ent.classname != "hook") // (a grappling hook shot off: no blood)
        {
            if(VR_Hit_Metal(trace_ent))
            {
                VR_Metal_Hit(trace_ent, org, vel * 0.3, damage, VR_METAL_SHOT); // VR: metal (an explosive box): sparks
            }
            else if(VR_Crate_Wooden(trace_ent))
            {
                VR_Crate_Splinters(org, vel * 0.2, damage); // VR: wood (a crate): splinters
            }
            else if(trace_ent.vr_shootnade)
            {
                VR_GrenShot_Sparks(org); // VR: a grenade (vr_grenade.qc, "Shooting grenades"): sparks
            }
            else if(VR_WallTorch_IsTorch(trace_ent))
            {
                // VR: a wall torch on its wall (vr_walltorch.qc): no blood. It takes damage only so that shots meet
                // it; its hit is VR_WallTorch_KnockOff's embers, and a taken torch takes no damage at all.
            }
            else
            {
                // VR: an armed humanoid's unbroken plate: sparks; blood and a wound only for what it lets through
                // (vr_agarmor.qc).
                float through = VR_AgArmor_HitFx(trace_ent, trace_endpos, vel, damage);
                if(through > 0)
                {
                    SpawnBlood (org, vel*0.2, damage * through);
                    VR_Wound_Pellet(trace_ent, trace_endpos, dir, damage); // VR: painted on its model (vr_wounds.qc)
                }
                VR_HitReact_Pellet(trace_ent, trace_endpos, dir); // VR: where its flinch is (vr_hitreact.qc)
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
    VR_SmallGib_BlastBegin(); // VR: the blast's pellets one hit for the small gibs (vr_smallgibs.qc)
    vr_decap_pellets = shotcount; // VR: head pops' chance (vr_decap.qc): the share of the blast at the head, the range
    vr_decap_src = src;
    while(shotcount > 0)
    {
        direction = dir + crandom()*spread_x*v_right + crandom()*spread_y*v_up;
        vr_fire_dev_sum = vr_fire_dev_sum + atan2(vlen(direction - dir * (direction * dir)), direction * dir) * 57.29578;
        vr_fire_dev_n = vr_fire_dev_n + 1;

        traceline(src, src + direction*xRange, MOVE_HITGIBS | MOVE_HITMODEL | MOVE_PORTALS, self); // VR: gibs and heads too (vr_gib_destroy), monsters' models (vr_hit_precise), on through teleporters (vr_portals.cpp),
        if(xStartHits && trace_startsolid && trace_ent != world && trace_ent.takedamage)
        {
            trace_fraction = 0; // started inside it: hit there
            trace_endpos = src;
            trace_plane_normal = -direction;
        }
        if(!(self.flags & FL_CLIENT) && (trace_ent.flags & FL_CLIENT))
        {
            VR_BodyShield_Take(trace_ent, src, src + direction*xRange); // a body the player holds before him takes it
        }
        // QVR: through a teleporter, the pellet goes on from its far side, turned with it (vr_portals_walk)
        vector shotFrom = src, shotDir = direction;
        if(portal_crossings() > 0)
        {
            tracer(self, vr_tracer_hand, vr_tracer_from, portal_entry(0)); // its tracer into the gate,
            shotFrom = portal_exit(portal_crossings() - 1);
            shotDir = portal_turn(direction);
            tracer(world, 2, shotFrom, trace_endpos);                   // and out of the far side
        }
        else
        {
            tracer(self, vr_tracer_hand, vr_tracer_from, trace_endpos); // a bullet tracer, from the muzzle (vr_weaponfx.cpp)
        }
        if(!(self.flags & FL_CLIENT))
        {
            VR_Crate_MonsterShot(src, src + direction*xRange); // a crate held up in the way
        }
        VR_ShotPush(trace_ent, shotFrom, trace_endpos, shotDir, QVR_SHOTPUSH_PELLET, 1); // the prop it hits or goes through
        VR_Gib_TraceHeld(src, src + direction*xRange);               // and one held in the other hand
        VR_WaterShotSplash(shotFrom, trace_endpos);                   // a splash where it goes into water
        if(debugShots)
        {
            if(trace_fraction == 1.0) { misses++; }
            else if(trace_ent == world) { hitsWorld++; }
            else { hitsTarget++; targetName = trace_ent.classname; }
        }
        // VR: a held weapon or its hand on the pellet's way (vr_armedgrunt_shot.qc); stopped on the weapon: no further
        if(VR_AGS_Shot(self, shotFrom, trace_endpos, shotDir, vr_pellet_damage ? vr_pellet_damage : 4, QVR_AGS_PELLET))
        {
            shotcount = shotcount - 1;
            continue;
        }
        if(trace_fraction != 1.0)
        //JIM
        {
            posDmgType = PositionalDamage(trace_ent, trace_endpos, shotDir);
            VR_SmallGib_Pellet(trace_ent, trace_endpos);
            if(debugShots && posDmgType == 1) { hitsHead++; }
            adjDmg = (vr_pellet_damage ? vr_pellet_damage : 4) * PositionalDamageFactor(posDmgType);
            if(vr_pellet_heavy_mult > 0 && (trace_ent.flags & FL_MONSTER) && trace_ent.max_health >= vr_pellet_heavy_health)
            {
                adjDmg = adjDmg * vr_pellet_heavy_mult; // VR: a revolver's slug into a big body (vr_revolver.qc)
            }
            vr_pellet_last = adjDmg;
            vr_pellet_last_part = posDmgType;
            vr_pellet_last_ent = trace_ent;

            TraceAttack (adjDmg, shotDir);

            // HIPNOTIC
            if(trace_ent == world)
            {
                placebullethole(trace_endpos);
            }
        }

        shotcount = shotcount - 1;
    }
    ApplyMultiDamage();
    VR_SmallGib_BlastEnd();
    if(debugShots)
    {
        dprint(sprintf("shot: from %v dir %v: %d on %s (%d head), %d world, %d nothing\n", src, dir, hitsTarget,
            targetName, hitsHead, hitsWorld, misses));
    }
};

void(float xHand, float shotcount, vector dir, vector spread) FireBullets =
{
    VR_DamageCredit_FromHand(self, self, xHand); // (a hitscan shot's inflictor: the shooter; vr_dmgcredit.qc)
    if(VRGet2HAiming(self))
    {
        spread *= cvar_hget(cvarh_vr_2h_spread_reduction);
    }
    spread = spread * (1 + vr_fire_spread_extra); // VR: an armed monster's dual-wielded gun (vr_armedgrunt.qc VR_AG_ShootProp)
    spread = AD_SharpSpread(spread); // AD support: AD's Sharp Shooter (vr_ad_items.qc)

    // From behind the muzzle, along the shot's line (VR_ShotBackPoint): what the muzzle is inside is hit too.
    vector muzzle = VRGetMuzzlePos(xHand);
    vector src = VR_ShotBackPoint(xHand, muzzle, dir);

    if(AD_Pellets(self)) // AD support: AD's projectile pellets (vr_ad_projectiles; vr_ad_bridge.qc)
    {
        AD_FirePellets(xHand, shotcount, dir, spread, muzzle);
        return;
    }
    VR_SetHitKind(QVR_HITKIND_SHOT, xHand, '0 0 0', dir);
    vr_tracer_hand = xHand;
    vr_tracer_from = muzzle;
    FireBulletsImpl(shotcount, dir, spread, src, 2048 + vlen(muzzle - src), TRUE);
    VR_ClearHitKind();
    // QVR: the shot's line passing near a monster alerts it (vr_stealth.qc)
    traceline(src, src + dir * 2048, FALSE, self);
    VR_Stealth_Graze(muzzle, trace_endpos, self, trace_ent);
}

void(float shotcount, vector dir, vector spread) EnemyFireBullets =
{
    vector    src;

    src = self.origin + v_forward*10;
    src_z = self.absmin_z + self.size_z * 0.7;
    if(vr_agr_native_from != '0 0 0')
    {
        src = vr_agr_native_from; // QVR: a dual wielder's own gun, from its muzzle (vr_armedrepl.qc VR_AGR_NativeShot)
    }
    if(!VR_AI_ShotReady(src)) { return; }
    if(self.vr_ai_gate > 0)
        dir = normalize(VR_AI_Point(self.enemy.origin + self.enemy.view_ofs) - VR_AI_Vector(self.enemy.velocity) * 0.2 - src);
    else
        dir = VR_Crate_ShotAim(src, dir, self.enemy);
    dir = VR_AimCone_Bend(dir, VR_AIM_NARROW); // QVR: bent to its facing's aim cone (vr_aimcone.qc)

    weaponfired(self, -1); // VR: its gun's muzzle flash (a grunt's: vr_weaponfx.cpp)
    spread = spread * (1 + vr_fire_spread_extra); // VR: an armed humanoid's own gun, its accuracy (vr_armedrepl.qc VR_AGR_NativeShot)
    if(AD_EnemyPellets(shotcount, dir, spread, src)) return; // AD support: AD's pellets (vr_ad_bridge.qc)
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
    vr_decap_weapon = (VRGetWeapon(xHand) == WID_SUPER_SHOTGUN) ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN; // (head pops)
    FireBullets(xHand, 6, dir, '0.01 0.01 0');
    vr_decap_weapon = 0;
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
    else if(VRGetWeapon(xHand) == WID_SUPER_SHOTGUN)
    {
        VR_Reload_SsgFired(xHand, 1); // (immersive reloading: spent in its barrel until it is broken open)
    }

    particle2(VRGetMuzzlePos(xHand), '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
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
    vr_decap_weapon = QVR_DECAP_SSG; // (head pops)
    if(MG3_BloodyBits() & MG3_BLOODY_SSG)
    {
        FireBullets(xHand, 28, dir, '0.075 0.025 0'); // Dawn of the Machine's bloody one: twice the pellets, wider (vr_mg3_weapons.qc)
    }
    else
    {
        FireBullets(xHand, 14, dir, '0.035 0.025 0');
    }
    vr_decap_weapon = 0;
    vr_pellet_damage = 0;

    weaponfired(self, xHand); // VR: its drawn kick and muzzle flash (vr_weaponfx.cpp)
    VRGunHaptic(xHand, 0.55, 75, 1.0);

    // Without reloading it reloads itself: the two spent shells come out as it would open.
    if(!VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, 2, 0.45, 0);
    }
    else
    {
        VR_Reload_SsgFired(xHand, 2); // (immersive reloading: spent in its barrels until it is broken open)
    }

    particle2(VRGetMuzzlePos(xHand) - '0 2 0', '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
    particle2(VRGetMuzzlePos(xHand) + '0 2 0', '0 0 0', QVR_PARTICLE_PRESET_GUNSMOKE, 1);
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

    if(VR_OwnerSpared(other))
        return;        // don't explode on owner (QVR: unless through a teleporter)

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    float rocketScale = VR_WpnDmgScale(self.owner, cvarh_vr_dmg_rocket, 100); // (Weapon Damage)
    damg = (100 + random()*20) * rocketScale;

    VR_Proj_Begin(QVR_PROJ_BLAST, damg); // VR: every projectile's hit (where it struck; its blast pops and pushes)
    if(other.health)
    {
        if(VR_IsShambler(other))
            damg = damg * 0.5;    // mostly immune
        T_Damage(other, self, self.owner, damg);
    }
    VR_Proj_End();

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
// A rocket fired by something that isn't a player (a rocket trap's, the Rogue ending's actor's): from `org` along `dir`,
// none of a player's hand, muzzle, ammo or kick.
entity(entity shooter, vector org, vector dir) VR_LaunchRocket =
{
    entity missile = spawn();
    missile.owner = shooter;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";
    missile.velocity = normalize(dir) * 1000;
    missile.angles = vectoangles(missile.velocity);
    missile.touch = T_MissileTouch;
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;
    setmodel(missile, "progs/missile.mdl");
    setsize(missile, '0 0 0', '0 0 0');
    setorigin(missile, org);
    return missile;
};

// The rocket launcher's rocket from self's hand `xHand` (its speed, model, touch, muzzle clearance and graze): the
// launcher's shot, and any gun's that fires our rocket (AD support: AD's rocket soldier's dropped launcher,
// vr_ad_enemyguns.qc). None of the gun's ammo, sound, kick or haptics.
entity(float xHand) VR_FireRocketMissile =
{
    entity missile = spawn();
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
    VR_ShotLaunched(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    VR_Stealth_GrazeMissile(missile); // QVR: its line passing near a monster alerts it (vr_stealth.qc)
    return missile;
};

entity(float xHand) W_FireRocket =
{
    delAmmoOrClipAndUpdateCounter(xHand, AID_ROCKETS, 1);

    sound(self, VRGetGunChannel(xHand), "weapons/sgun1.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    entity missile = VR_FireRocketMissile(xHand);

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

// VR: a bolt's damage to `t` (trace_ent, struck at trace_endpos going `xDir`): the player's at a head that it kills pops
// it (vr_decap.qc, "Head pops"). T_Damage's answer; the trace globals as they were (the death code may trace).
void(entity t, float xAlive, vector xAt, vector xDir) VR_Shock_Hit; // (vr_shock.qc)
float(entity t, entity from, float damage, vector xDir) VR_LightningHit =
{
    vector at = trace_endpos;
    float shockAlive = t.health > 0; // (killed by it, or struck dead: VR_Shock_Hit)
    if((t.flags & FL_MONSTER) || t.vr_corpse)
    {
        bodyshock(t, 0.75);
    }
    if((t.flags & FL_CLIENT) && shockAlive)
    {
        playershock(t, damage, at); // VR: arcs over the struck player's arms and body (vr_shock_self_time)
    }
    float pop = VR_Decap_BoltArm(t, at, xDir, damage);
    vr_limb_wear_at = at; // (a corpse's head or limb struck wears down: VR_Limb_CorpseWear)
    vr_limb_wear_ent = t;
    vr_limb_wear_time = time;
    vr_ad_ammo = AD_AMMO_CELLS; // AD support: lightning is cells to AD's ammo resistance (vr_ad_player.qc)
    float r = T_Damage(t, from, from, damage);
    vr_ad_ammo = 0;
    vr_limb_wear_ent = world;
    VR_Shock_Hit(t, shockAlive, at, xDir); // VR: lightning's lasting shock (vr_shock.qc)
    if(pop)
    {
        VR_Decap_After(t);
    }
    trace_ent = t;
    trace_endpos = at;
    return r;
};

/*
=================
LightningDamage
=================
*/
// QVR: MOVE_HITPASS while LightningDamage strikes a bolt's piece come out of a teleporter: the one who fired it is hit too
// (your bolt at your own image, seen through the gate).
float vr_lightning_hitpass;

void(vector p1, vector p2, entity from, float damage) LightningDamage =
{
    entity        e1, e2;
    vector        f;

    VR_Stealth_Graze(p1, p2, from, world); // QVR: the beam passing near a monster alerts it (vr_stealth.qc)

    f = p2 - p1;
    normalize(f);
    f_x = 0 - f_y;
    f_y = f_x;
    f_z = 0;
    f = f*16;

    e1 = e2 = world;

    // (The bolt's credit is `from`'s: a player's by his lightning gun, W_FireLightning; vr_dmgcredit.qc.)
    traceline(p1, p2, MOVE_HITGIBS | MOVE_HITMODEL | vr_lightning_hitpass, self); // VR: gibs and heads too (vr_gib_destroy), monsters' models (vr_hit_precise),
    // VR: the prop the beam hits or goes through, pushed (a player's bolt: 30; a shambler's by its damage).
    VR_ShotPush(trace_ent, p1, trace_endpos, p2 - p1, QVR_SHOTPUSH_LIGHTNING, damage / 30);
    VR_Gib_TraceHeld(p1, p2);              // and one held in the other hand
    VR_AGS_Shot(from, p1, trace_endpos, p2 - p1, damage, QVR_AGS_LIGHTNING); // VR: through a held weapon or its hand (vr_armedgrunt_shot.qc)
//JIM
    if(trace_ent.takedamage && !trace_ent.wetsuit_finished)
    {
        if(VR_LightningHit(trace_ent, from, damage, p2 - p1))
        {
            VR_HitBloodSplash(trace_ent, trace_endpos, damage * 5);
        }

        if(self.classname == "player")
        {
            if(other.classname == "player")
                trace_ent.velocity_z = trace_ent.velocity_z + 400;
        }
    }
    e1 = trace_ent;

    traceline(p1 + f, p2 + f, MOVE_HITGIBS | MOVE_HITMODEL | vr_lightning_hitpass, self);
//JIM
    if(trace_ent != e1 && trace_ent.takedamage &&
    !trace_ent.wetsuit_finished)
    {
        if(VR_LightningHit(trace_ent, from, damage, p2 - p1))
        {
            VR_HitBloodSplash(trace_ent, trace_endpos, damage * 5);
        }
    }
    e2 = trace_ent;

    traceline(p1 - f, p2 - f, MOVE_HITGIBS | MOVE_HITMODEL | vr_lightning_hitpass, self);
//JIM
    if(trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage &&
        !trace_ent.wetsuit_finished)
    {
        if(VR_LightningHit(trace_ent, from, damage, p2 - p1))
        {
            VR_HitBloodSplash(trace_ent, trace_endpos, damage * 5);
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
                if((head.flags & FL_CLIENT) && head.health > 0 && !head.wetsuit_finished)
                {
                    playershock(head, damage, mid); // (arcs over that player's arms and body)
                }
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
            DropWeaponInHand(self, hand, VRGetEntWeapon(self, hand), VRGetEntWeaponFlags(self, hand));
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
    VR_DamageCredit_FromHand(self, self, xHand); // (the shock's: the player's, its inflictor)

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

    VR_DamageCredit_FromHand(self, self, xHand); // (the bolt's and the discharge's inflictor: the player; vr_dmgcredit.qc)

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
    traceline(from, org + v_forward*600, TRUE | MOVE_PORTALS, self);
    // QVR: through a teleporter (vr_portals.cpp, vr_portals_walk), the bolt in two pieces: to the gate, and on from its far
    // side, turned with it (its beam: PlayerLightningGunEffect)
    float through = portal_crossings() > 0;
    float fromImage = portal_from_image(); // (the gun held through a gate: the bolt may strike you from the start)
    vector boltEnd = trace_endpos;
    vector gateIn = through ? portal_entry(0) : boltEnd;
    vector boltFrom = through ? portal_exit(portal_crossings() - 1) : org;
    vector boltDir = through ? portal_turn(v_forward) : v_forward;
    if(cvar_hget(cvarh_vr_lg_water))
    {
        VR_LGWater_Beam(boltFrom, boltEnd); // into water from the open: electrified
    }
    VR_LGWater_Splash(boltFrom, boltEnd); // and a splash where it goes in
    particle2(boltEnd, '0 0 0', QVR_PARTICLE_PRESET_LIGHTNING, 60);
    particle2(boltEnd, '0 0 0', QVR_PARTICLE_PRESET_SMOKE, 1);
    particle2(boltEnd, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 12);
    VR_SetHitKind(QVR_HITKIND_SHOT, xHand, '0 0 0', boltDir);
    vr_decap_weapon = QVR_DECAP_LIGHTNING; // (head pops)
    vr_lightning_hitpass = fromImage ? MOVE_HITPASS : 0;
    if(through)
    {
        LightningDamage(from, gateIn, self, cvar_hget(cvarh_vr_dmg_lightning)); // the piece on this side
        from = boltFrom;
        vr_lightning_hitpass = MOVE_HITPASS; // (the far piece strikes you too: your own image)
    }
    LightningDamage(from, boltEnd + boltDir*4, self, cvar_hget(cvarh_vr_dmg_lightning));
    vr_lightning_hitpass = 0;
    vr_decap_weapon = 0;
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
    if(VR_OwnerSpared(other))
        return;        // don't explode on owner (QVR: unless through a teleporter)

    if(VR_WallTorch_OnWall(other))
    {
        VR_WallTorch_KnockOff(other, self, normalize(self.velocity));
        return; // retain the grenade's bounce and its original fuse
    }
    VR_Proj_Begin(QVR_PROJ_BLAST, 0); // VR: every projectile's hit (where it struck: its blast's pops are its own)
    VR_Proj_End();

    if(other.takedamage == DAMAGE_AIM && other.mg3_chthon)
    {
        // Dawn of the Machine's Chthon (upstream GrenadeTouch): the whole blast into him (his middle is far above where it
        // struck), the rest around.
        float gd = VR_WpnDmg(self.owner, cvarh_vr_dmg_grenade, 120);
        T_Damage(other, self, self.owner, gd);
        T_RadiusDamage(self, self.owner, gd, other);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_EXPLOSION);
        WriteVec3(MSG_BROADCAST, self.origin);
        BecomeExplosion();
        return;
    }
    if(other.takedamage == DAMAGE_AIM)
    {
        if(MG3_Shub_Is(other))
        {
            MG3_Shub_GrenadeHit(); // (Dawn of the Machine's Shub: the hit hers, the blast without her: vr_mg3_shub.qc)
        }
        else if(VR_DamageCredit_Accepts(other, self)) // HONEY: else it bounces off what its weapon can't hurt
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
// The grenade launcher's grenade from self's hand `xHand` (its speed, model, size, bounce, fuse, explosion, muzzle
// clearance and catchability): the launcher's shot, and any gun's that fires our grenade (AD support: AD's grenade
// soldier's dropped launcher, vr_ad_enemyguns.qc). None of the gun's ammo, sound, kick or haptics.
entity(float xHand) VR_FireGrenadeMissile =
{
    vector tmpAngles;
    entity missile = spawn();
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
    VR_ShotLaunched(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    VR_Stealth_GrazeMissile(missile); // QVR: its line passing near a monster alerts it (vr_stealth.qc)
    VR_Grenade_Make(missile, GrenadeExplode, 2.5, TRUE); // QVR: catchable too with vr_grenade_catch 2
    return missile;
};

void(float xHand) W_FireGrenade =
{
    delAmmoOrClipAndUpdateCounter(xHand, AID_ROCKETS, 1);

    sound(self, VRGetGunChannel(xHand), "weapons/grenade.wav", 1, ATTN_NORM);

    self.punchangle_x = -2;

    VR_FireGrenadeMissile(xHand);

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
// (float NumProximityGrenades: vr_grenade.qc, before this: a pouch's proximity grenade let go of armed is one too)
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
      VR_Proj_Begin(QVR_PROJ_BLAST, 0); // VR: every projectile's hit (weapons.qc VR_Proj_Begin)
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
    VR_ShotLaunched(missile, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    VR_Stealth_GrazeMissile(missile); // QVR: its line passing near a monster alerts it (vr_stealth.qc)
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
// QVR: the nailguns' nails' speed (vr_nail_speed: 2065, 18% above Quake VR's 1750 of before round 23; id's 1000), the
// player's and every monster's firing them through launch_spike or launch_lava_spike (a caller may set its own after).
float() VR_NailSpeed =
{
    return max(100, cvar_hget(cvarh_vr_nail_speed));
};
#ifdef QVR_TESTS
void() VR_NailTest_Frame; // vr_nail_test: the nails' speed, size and hit box (vr_nail_test.qc)
#endif

entity(vector org, vector dir) launch_spike =
{
    entity newmis;

    newmis = spawn();
    newmis.owner = self;
    VR_AI_Tag(newmis);
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

    newmis.velocity = dir * VR_NailSpeed(); // QVR: 1750 until round 23 (id's 1000)
    VR_ShotPush_Launch(newmis, QVR_SHOTPUSH_NAIL); // VR: pushes the prop it hits or flies through
    VR_Stealth_GrazeMissile(newmis); // QVR: its line passing near a monster alerts it (vr_stealth.qc)
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
    VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)
    newmis.touch = superspike_touch;
    newmis.vr_shot_push_kind = QVR_SHOTPUSH_SUPERNAIL;
    setmodel(newmis, "progs/s_spike.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);
    VR_Burn_NailLaunched(newmis); // VR: through a torch's flame, a lava nail (vr_burning.qc)

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
    VR_ShotLaunched(newmis, xHand); // behind what the muzzle is inside (VR_ShotPlace)

    newmis.velocity += vrProjectileVelocity(xHand, 1.0);
    VR_Burn_NailLaunched(newmis); // VR: through a torch's flame, a lava nail (vr_burning.qc)

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
    float adjDmg;

    // HONEY
    float bleed;
    bleed = FALSE;

    if(VR_OwnerSpared(other)) // (QVR: through a teleporter it hits you)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    // VR: every projectile's hit (VR_Proj_Begin): the prop or ragdoll it hit (or flew through) pushed, where it
    // struck (a head more), a corpse's head or limb worn, a kill at the head or a limb popped by chance.
    VR_Proj_Begin(QVR_PROJ_BOLT, VR_WpnDmg(self.owner, cvarh_vr_dmg_nail, 9));
    makeforward_mdl(self.angles);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        adjDmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_nail, 9) * vr_proj_factor; // (its part: VR_Proj_Begin)

        if(T_Damage(other, self, self.owner, adjDmg))
        {
            spawn_touchblood (adjDmg);
            bleed = TRUE;
        }
        AD_NailPierce(other); // AD support: AD's Nail Piercer: the nail on through the body (vr_ad_items.qc)
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

    VR_Proj_End();
    remove(self);
};

void() superspike_touch =
{
    float adjDmg;

    // HONEY
    float bleed;
    bleed = FALSE;

    if(VR_OwnerSpared(other)) // (QVR: through a teleporter it hits you)
        return;

    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    // VR: every projectile's hit (VR_Proj_Begin): the prop or ragdoll it hit (or flew through) pushed, where it
    // struck (a head more), a corpse's head or limb worn, a kill at the head or a limb popped by chance.
    VR_Proj_Begin(QVR_PROJ_BOLT, VR_WpnDmg(self.owner, cvarh_vr_dmg_super_nail, 18));
    makeforward_mdl(self.angles);

    // hit something that maybe bleeds
    if(other.takedamage)
    {
        adjDmg = VR_WpnDmg(self.owner, cvarh_vr_dmg_super_nail, 18) * vr_proj_factor; // (its part: VR_Proj_Begin)

        if(T_Damage(other, self, self.owner, adjDmg))
        {
            spawn_touchblood (adjDmg);
        }
        AD_NailPierce(other); // AD support: AD's Nail Piercer (vr_ad_items.qc)
    }

    if(!bleed)
    {
        spike_bullethole();
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_SUPERSPIKE);
        WriteVec3(MSG_BROADCAST, self.origin);
    }

    VR_Proj_End();
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

// W_AttackImpl's firing, one function a weapon (W18; W_AttackImpl_Fire dispatches): the hand, the weapon's flags and
// where its next attack time goes (left as it was where the weapon sets none).

// The axe, the swords, the crowbar and the Super Axe: swung by the tracked hands themselves; untracked, a swing.
void(float xHand, float* xOutAttackFinished) W_Fire_Melee =
{
    if(!VRHandsTracked(self) ||
       !self.ishuman)
    {
        VR_SoundVaried(self, VRGetGunChannel(xHand), "weapons/ax1.wav", 1, ATTN_NORM);
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
};

void(float xHand, float* xOutAttackFinished) W_Fire_Shotgun =
{
    SuperDamageSound();
    player_shot1(xHand);
    W_FireShotgun(xHand);
    *xOutAttackFinished = time + RuneApplyHell(MG3_BloodyShotgunRefire(0.5), self); // (Dawn of the Machine's bloody one: 0.28)
};

void(float xHand, float* xOutAttackFinished) W_Fire_SuperShotgun =
{
    float widow = AD_WidowmakerIn(self, xHand);
    SuperDamageSound();
    player_shot1(xHand);
    if(widow)
    {
        // AD support: the Widowmaker, an auto shotgun: a shell a shot, again while the trigger is held (vr_ad_bridge.qc).
        AD_FireWidowmaker(xHand);
        *xOutAttackFinished = time + RuneApplyHell(max(0.02, cvar_hget(cvarh_vr_ad_widowmaker_refire)), self);
        return;
    }
    W_FireSuperShotgun(xHand);
    *xOutAttackFinished = time + RuneApplyHell(0.7, self);
};

// The nailgun and the super nailgun (their frames fire: player_nail1).
void(float xHand, float xWeaponFlags) W_Fire_Nailgun =
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
};

void(float xHand, float xWeaponFlags, float* xOutAttackFinished) W_Fire_GrenadeLauncher =
{
    SuperDamageSound();
    player_rocket1(xHand);
    if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiGrenade(xHand); }
    else                                              { W_FireGrenade(xHand); }
    *xOutAttackFinished = time + RuneApplyHell(0.6, self);
};

void(float xHand, float xWeaponFlags, float* xOutAttackFinished) W_Fire_RocketLauncher =
{
    SuperDamageSound();
    player_rocket1(xHand);
    if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO) { W_FireMultiRocket(xHand); }
    else                                              { W_FireRocket(xHand); }
    *xOutAttackFinished = time + RuneApplyHell(0.8, self);
};

void(float xHand, float xWeaponFlags, float* xOutAttackFinished) W_Fire_Lightning =
{
    if(AD_PlasmaGun(xHand, xWeaponFlags)) // AD support: the Plasma Gun (vr_ad_bridge.qc)
    {
        AD_FirePlasma(xHand);
        *xOutAttackFinished = time + 0.2;
        return;
    }

    player_light1(xHand);
    if(xWeaponFlags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
    {
        *xOutAttackFinished = time + RuneApplyHell(1.0, self);
    }
    else
    {
        *xOutAttackFinished = time + 0.1;
    }
    sound(self, VRGetHandChannel(xHand), "weapons/lstart.wav", 1, ATTN_NORM);
};

// HIPNOTIC (MED): Mjolnir, swung by the tracked hands; untracked, a swing (its lightning with 30 cells or more).
void(float xHand, float* xOutAttackFinished) W_Fire_Mjolnir =
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
};

void(float xHand, float* xOutAttackFinished) W_Fire_ProximityGun =
{
    SuperDamageSound();
    player_rocket1(xHand);
    W_FireProximityGrenade(xHand);
    *xOutAttackFinished = time + 0.6;
};

void(float xHand, float* xOutAttackFinished) W_Fire_Grapple =
{
    // A hook that came off by itself (what it bit died or went) fires again only on a new pull (vr_grapple.qc).
    if(!getHookOut(xHand) && VR_Grapple_MustRelease(xHand))
        return;
    if(!getHookOut(xHand))
        player_grapple1(xHand);
    else
        player_grapple3(xHand);

    *xOutAttackFinished = time + 0.1;
};

// The weapon `xWeapon` fires (W_AttackImpl, once its ammo and its other checks passed).
void(float xHand, float xWeapon, float xWeaponFlags, float* xOutAttackFinished) W_AttackImpl_Fire =
{
    if(xWeapon == WID_AXE || xWeapon == WID_SWORD || xWeapon == WID_CROWBAR || xWeapon == WID_SUPERAXE)
                                           { W_Fire_Melee(xHand, xOutAttackFinished); }
    else if(xWeapon == WID_SHOTGUN)          { W_Fire_Shotgun(xHand, xOutAttackFinished); }
    else if(xWeapon == WID_SUPER_SHOTGUN)    { W_Fire_SuperShotgun(xHand, xOutAttackFinished); }
    else if(xWeapon == WID_NAILGUN || xWeapon == WID_SUPER_NAILGUN) { W_Fire_Nailgun(xHand, xWeaponFlags); }
    else if(xWeapon == WID_GRENADE_LAUNCHER) { W_Fire_GrenadeLauncher(xHand, xWeaponFlags, xOutAttackFinished); }
    else if(xWeapon == WID_ROCKET_LAUNCHER)  { W_Fire_RocketLauncher(xHand, xWeaponFlags, xOutAttackFinished); }
    else if(xWeapon == WID_LIGHTNING)        { W_Fire_Lightning(xHand, xWeaponFlags, xOutAttackFinished); }
    else if(xWeapon == WID_LASER_CANNON)     { player_laser1(xHand); } // HIPNOTIC (MED)
    else if(xWeapon == WID_MJOLNIR)          { W_Fire_Mjolnir(xHand, xOutAttackFinished); }
    else if(xWeapon == WID_PROXIMITY_GUN)    { W_Fire_ProximityGun(xHand, xOutAttackFinished); }
    else if(xWeapon == WID_GRAPPLE)          { W_Fire_Grapple(xHand, xOutAttackFinished); }
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
    // The toolgun's trigger is the engine's (vr_toolgun.cpp: its tools act on what it points at): nothing here.
    if(xWeapon == WID_TOOLGUN)
    {
        return;
    }
    // The nail pistol: semi-automatic, a shot (or its dry click) a pull (vr_nailpistol.qc).
    if(xWeapon == WID_NAILPISTOL)
    {
        VR_NailPistol_Attack(xHand, xAttackFinished, xOutAttackFinished);
        return;
    }
    // The flare gun: a flare a pull, from its own two (its clip), then spent (vr_flaregun.qc).
    if(xWeapon == WID_FLAREGUN)
    {
        VR_FlareGun_Attack(xHand, xAttackFinished, xOutAttackFinished);
        return;
    }
    // The heavy revolver: double action, a pull cocks the hammer, turns the cylinder and fires (vr_revolver.qc).
    if(xWeapon == WID_REVOLVER)
    {
        VR_Revolver_Attack(xHand, xAttackFinished, xOutAttackFinished);
        return;
    }

    // A grunt's shotgun or an enforcer's rifle: from its own ammo, its clip (vr_enemyguns.qc).
    if(VR_IsEnemyGun(xWeapon))
    {
        VR_EnemyGun_Attack(xHand, xWeapon, xAttackFinished, xOutAttackFinished);
        return;
    }

    // The super shotgun broken open (immersive reloading, vr_reload.qc): its hammers fall on nothing.
    if(VR_Reload_SsgIsOpen(self, xHand))
    {
        if(time > xAttackFinished)
        {
            sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
            VR_Reload_Log(sprintf("a dry click: the super shotgun (hand %g) is open", xHand));
        }
        return;
    }

    // So is the Super Axe without its cells (they are only its burst's: vr_mg3_weapons.qc): held by the whole hand, the
    // trigger closed round its haft, silent (the author's note, vrfiringrange_2026-10-08_14-14-01); untracked, it swings.
    float emptyHammer = (xWeapon == WID_MJOLNIR || xWeapon == WID_SUPERAXE) && !W_CheckNoAmmo(xHand);
    if(emptyHammer && VRHandsTracked(self) && self.ishuman)
    {
        return;
    }

    if(!emptyHammer && !W_CheckNoAmmo(xHand))
    {
        if(time > xAttackFinished)
        {
            sound(self, VRGetHandChannel(xHand), "gunclick.wav", 1, ATTN_NORM);
            *xOutAttackFinished = time + 0.5;
            if(cvar_hget(cvarh_vr_debug_shots))
            {
                dprint(sprintf("empty click: hand %g, weapon %g\n", xHand, xWeapon));
            }
        }

        return;
    }

    // AD support: the Widowmaker's barrels first spin up to firing speed (vr_ad_bridge.qc AD_WidowmakerSpinFrame).
    if(xWeapon == WID_SUPER_SHOTGUN && AD_WidowmakerIn(self, xHand) && !AD_WidowmakerSpunUp(xHand))
    {
        return;
    }

    makevectors(VRGetWeaponFireRot(xHand)); // calculate forward angle for velocity
    // Wake monsters up: not for a hand merely closing (the trigger on a fist, or on a melee weapon the tracked hands
    // swing themselves: nothing happens here; the author's note e1m2_2026-10-07_22-44-33). A real swing or punch wakes
    // them where it whooshes or lands (VR_Melee_Whoosh, VR_Melee_Landed).
    float tracked = VRHandsTracked(self) && self.ishuman;
    float justClosing = xWeapon == WID_FIST || (tracked && (xWeapon == WID_AXE || xWeapon == WID_SWORD ||
        xWeapon == WID_CROWBAR || xWeapon == WID_SUPERAXE || xWeapon == WID_MJOLNIR));
    if(!justClosing)
    {
        self.show_hostile = time + 1;
        VR_Stealth_Shot(self, xWeapon); // QVR: heard by its loudness (vr_stealth.qc)
    }

    // ZOID--
    RuneApplyBlackNoise(self); // make rune noise
    //--ZOID

    W_AttackImpl_Fire(xHand, xWeapon, xWeaponFlags, xOutAttackFinished);
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
        // VR: the shells' slot holds the shotgun and the heavy revolver (vr_revolver.qc): 2 again toggles them, as 4
        // the nailgun and the nail pistol; the one you have when you have one only.
        wid = WID_SHOTGUN;
        if(VR_ItemUtil_EntHasItem(self, IID_REVOLVER) &&
           (self.weapon == WID_SHOTGUN || !VR_ItemUtil_EntHasItem(self, IID_SHOTGUN)))
        {
            wid = WID_REVOLVER;
        }
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) < 1) { am = 1; }
    }
    else if(self.impulse == 3)
    {
        wid = WID_SUPER_SHOTGUN;
        if(VR_AmmoUtil_GetAmmoCountByAmmoId(self, AID_SHELLS) < 2) { am = 1; }
    }
    else if(self.impulse == 4)
    {
        // VR: the nails' slot holds the nailgun and the nail pistol (vr_nailpistol.qc): 4 again toggles them, as
        // Hipnotic's 6 the grenade launcher and the proximity gun; the one you have when you have one only.
        wid = WID_NAILGUN;
        if(VR_ItemUtil_EntHasItem(self, IID_NAILPISTOL) &&
           (self.weapon == WID_NAILGUN || !VR_ItemUtil_EntHasItem(self, IID_NAILGUN)))
        {
            wid = WID_NAILPISTOL;
        }
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
    if(wid != self.weapon)
    {
        VRCycleEntWeaponInst(self, cVR_MainHand, wid);
    }
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

    // ROGUE (each where its projectile is there)
    {
        self.ammo_lava_nails = VR_Pack_AmmoAvailable(AID_LAVA_NAILS) ? 200 : 0;
        self.ammo_multi_rockets = 100;
        self.ammo_plasma = VR_Pack_AmmoAvailable(AID_PLASMA) ? 100 : 0;
    }

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
    VR_WeaponUtil_EntAddWeapon(self, WID_NAILPISTOL); // VR: the nail pistol (vr_nailpistol.qc)
    VR_WeaponUtil_EntAddWeapon(self, WID_REVOLVER); // VR: the heavy revolver (vr_revolver.qc)

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

    // Flatscreen play (-flatscreenmode): the empty hand (the fist) is one step of the cycle, both ways.
    if(VR_FlatPlay(xPlayer)) { minWeapon = WID_FIST; }

    // Just for safety.
    float maxIterations = maxWeapon + 3; // (the nail pistol and the heavy revolver too)

    float skipEmptyWeapons = FALSE;

    // Used to avoid infinite loops.
    float originalWeapon = xWeapon;

    // Find the next suitable weapon.
    float resultWeapon = originalWeapon;
    for(float iterations = 0; iterations < maxIterations; ++iterations)
    {

        // (+ xDir; the heavy revolver after the shotgun, the nail pistol before the nailgun)
        xWeapon = VR_Revolver_CycleStep(xWeapon, xDir);

        if(xWeapon > maxWeapon && xWeapon != WID_NAILPISTOL && xWeapon != WID_REVOLVER) { xWeapon = minWeapon; }
        else if(xWeapon < minWeapon)                                                    { xWeapon = maxWeapon; }

        // If we get to the original weapon after cycling, break.
        if(xWeapon == originalWeapon)
        {
            break;
        }

        // Do not select unavailable weapons.
        if(xWeapon != WID_FIST && !VR_WeaponUtil_EntHasWeapon(xPlayer, xWeapon))
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

    // Flatscreen play (-flatscreenmode): 1 and 2 cycle the weapons collected into the left and the right hand, so
    // cycling is on whatever vr_weapon_cycle_mode says.
    if(VR_FlatPlay(self))
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
        VRCycleEntWeaponInst(self, xHand, newWeapon);

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

    if(!WeaponSupportsSecondaryAmmoFlag(weapon) ||
       (weapon != WID_SWORD && !VR_Pack_WeaponFlags(weapon, QVR_WPNFLAG_USE_SECONDARY_AMMO))) // (its secondary's projectile not here)
    {
        return;
    }
    if(weapon == WID_LIGHTNING && AD_PlasmaGun(xHand, VRGetWeaponFlags(xHand)))
    {
        return; // AD support: AD's Plasma Gun fires cells only (AD's W_FirePlasma), no button (vr_weapons.inc slot 35)
    }

    // vr_ammo_switch_empty_hand: pressable only with the other hand empty (nothing held at all): not akimbo.
    if(cvar("vr_ammo_switch_empty_hand") && !VRIsHandEmpty(self, VRGetOtherHand(xHand)))
    {
        dprint("ammo switch: refused, the other hand is full\n");
        VRGunHaptic(VRGetOtherHand(xHand), 0.15, 40, 0.6);
        sound(self, VRGetGunChannel(xHand), "weapons/lock4.wav", 0.4, ATTN_NORM);
        return;
    }
    dprint(sprintf("ammo switch: toggled (other hand: weapon %d, %s)\n", VRGetWeapon(VRGetOtherHand(xHand)),
        VRIsHandEmpty(self, VRGetOtherHand(xHand)) ? "empty" : "full"));

    if(VR_Reload_MagIn(self, xHand))
    {
        // Immersive reloading: the magazine of the old ammo drops out (its rounds in it); one of the new goes in by hand.
        makevectors(VRGetEntHandRot(self, xHand));
        VR_Reload_Eject(self, xHand, VRGetEntHandVel(self, xHand) * VRMetersToUnits() - v_up * 40, -1, "the ammo changed");
    }
    else
    {
        VRTryUnloadWeapon(self, xHand, weapon, VRGetWeaponFlags(xHand));
    }

    VRGunHaptic(VRGetOtherHand(xHand), 0.3, 75, 1.0);
    sound(self, VRGetGunChannel(xHand), "buttons/switch02.wav", 0.5, ATTN_NORM);

    VRSetWeaponFlags(xHand, VRGetWeaponFlags(xHand) ^ QVR_WPNFLAG_USE_SECONDARY_AMMO);
    W_SetCurrentAmmoFor(xHand);
}

#ifdef QVR_TESTS
float vr_test_armor_kind;   // impulse 250: the next armour it drops (0 green, 1 yellow, 2 red)
float vr_test_held_kind;    // impulse 252: the next gib or head it puts in the off hand (round 21)
entity vr_test_armor_last;  // and the last it dropped (impulse 251)
#endif

// Projectiles through the same corner columns and at the middle (impulse 237 a nail, 236 a rocket, 234 a nail with the
// grappling hook's class, one at a time, each
// through the next of the 6 columns, then the middle; 235 prints what the last touched): real missiles, as the engine
// flies them (MOVE_MISSILE, with the precise hits of the option as it is set). The target takes no damage (no pain
// poses, no shove), and is put back where it stood.
#ifdef QVR_TESTS
entity vr_test_target;
vector vr_test_home;
float vr_test_shot;
float vr_test_on;
float vr_test_world;
float vr_test_other;
#endif

// QVR test: crate cover (Debug > Tests > Crate Cover; NOTES.md vrfiringrange_2026-10-02_01-39-38, "set up a scenario"):
// a large crate square to you just ahead to crouch behind, a small one at your right to hold up, and a grunt beyond the
// crate facing you, not yet awake (Quake's monsters wake when they see you: vr_crate_sight, VR_Crate_HidesFrom).
// Crouched behind the crate it should stay asleep; standing up, or holding the small crate up, it should see you.
#ifdef QVR_TESTS
entity vr_test_cover_grunt; // the last one it put there (impulse 224 reports on it)
#endif

#ifdef QVR_TESTS
void() VR_WeaponInst_Test; // (vr_weaponinst_test.qc)
void() VR_Reload_Test;     // (vr_reload_test.qc)
#endif

// ImpulseCommands' per-hand steps (W19).
// Whether `xHand` may fire (and so cycle) again.
float(float xHand) VR_Impulse_HandReady =
{
    return time >= (xHand == cVR_OffHand ? self.offhand_attack_finished : self.attack_finished);
};

// Cycles the weapon in `xHand` to the next one collected (`xReverse`: the one before) when cycling is allowed.
void(float xHand, float xReverse) VR_Impulse_CycleHand =
{
    if(getWeaponCycleMode() != QVR_WEAPONCYCLEMODE_ALLOWED)
    {
        return;
    }
    if(xHand == cVR_OffHand)
    {
        if(xReverse) { CycleWeaponReverseCommand(cVR_OffHand, self.weapon2, &self.weapon2, self.weaponflags2, &self.weaponflags2); }
        else         { CycleWeaponCommand(cVR_OffHand, self.weapon2, &self.weapon2, self.weaponflags2, &self.weaponflags2); }
    }
    else
    {
        if(xReverse) { CycleWeaponReverseCommand(cVR_MainHand, self.weapon, &self.weapon, self.weaponflags, &self.weaponflags); }
        else         { CycleWeaponCommand(cVR_MainHand, self.weapon, &self.weapon, self.weaponflags, &self.weaponflags); }
    }
};

// The button of the weapon in `xHand` pressed: a grappling hook's detach, else its secondary ammo.
void(float xHand) VR_Impulse_WeaponButton =
{
    if(VRGetWeapon(xHand) == WID_GRAPPLE) { VR_Grapple_Detach(xHand); }
    else                                  { ToggleSecondaryAmmo(xHand); }
};
void() ImpulseCommands =
{
    // QVR: the single-player test and debug impulses (vr_impulse_test.qc): first, as each returns or falls
    // through as it did here.
#ifdef QVR_TESTS
    if(!deathmatch && !coop && VR_TestImpulse())
    {
        return;
    }
#endif

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
        VRGiveEntWeapon(self, cheatHand, cheatWid, 0, WeaponIdToBaseClipSize(cheatWid)); // (a new one: the old is gone)
        W_SetCurrentAmmoFor(cheatHand);
        self.impulse = 0;
        return;
    }

    // Flatscreen play (-flatscreenmode): E force-grabs what the view is pointed at (vr_flatplay.cpp reads the key and
    // sends the impulse; the force grab itself is the game's own, vr_wpnforcegrab.qc).
    if(self.impulse == 46)
    {
        self.impulse = 0;
        if(VR_FlatPlay(self))
        {
            VR_Forcegrab_FlatGrab();
        }
        return;
    }

    if(self.impulse >= 1 && self.impulse <= 8)
    {
        // Flatscreen play (-flatscreenmode): 1 cycles the left hand's weapon and 2 the right hand's, through the weapons
        // collected (the same cycling the controller's Y and B do); Quake's own weapon select stands for the other keys.
        if(VR_FlatPlay(self) && (self.impulse == 1 || self.impulse == 2))
        {
            float flatHand = self.impulse == 1 ? cVR_OffHand : cVR_MainHand;
            self.impulse = 0;
            if(VR_Impulse_HandReady(flatHand) && !VR_Grapple_ButtonsBusy(flatHand))
            {
                VR_Impulse_CycleHand(flatHand, FALSE);
            }
            return;
        }

        if(getWeaponCycleMode() == QVR_WEAPONCYCLEMODE_ALLOWED)
        {
            W_ChangeWeapon();
        }
    }

    // A weapon's button pressed (the other hand's fingertip on it): its secondary ammo; the grappling hook's detach
    // (vr_grapple.qc). 44/45: the grappling gun's front button, the reel-in.
    if(self.impulse == 42) { VR_Impulse_WeaponButton(cVR_OffHand); }
    if(self.impulse == 43) { VR_Impulse_WeaponButton(cVR_MainHand); }
    if(self.impulse == 44 && VRGetWeapon(cVR_OffHand) == WID_GRAPPLE) { VR_Grapple_QuickRelease(cVR_OffHand); }
    if(self.impulse == 45 && VRGetWeapon(cVR_MainHand) == WID_GRAPPLE) { VR_Grapple_QuickRelease(cVR_MainHand); }

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

    // B/Y (10/12) cycle the hand's weapon, 15/16 back. The hand's B/Y also reels a grappling hook in (vr_grapple.qc: its own
    // gun's, or the other hand's when its own has none out): while it drives one, it cycles nothing; and they eject a
    // magazine that is in (vr_reload.qc).
    if(self.impulse == 10 || self.impulse == 12)
    {
        float cycleHand = self.impulse == 12 ? cVR_OffHand : cVR_MainHand;
        if(VR_Impulse_HandReady(cycleHand) && !VR_Grapple_ButtonsBusy(cycleHand) && !VR_Reload_MagIn(self, cycleHand))
        {
            VR_Impulse_CycleHand(cycleHand, FALSE);
        }
    }

    if(self.impulse == 15 || self.impulse == 16)
    {
        float backHand = self.impulse == 16 ? cVR_OffHand : cVR_MainHand;
        if(VR_Impulse_HandReady(backHand))
        {
            VR_Impulse_CycleHand(backHand, TRUE);
        }
    }

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
    // QVR: Debug > Cheats and Recording (vr_cheats.qc): the powerups on for an hour or off at once, full health and armour.
    if(self.impulse == 190)
        VR_Cheat_TogglePowerup(super_damage_finished, super_time, IID_QUAD, "Quad Damage");
    if(self.impulse == 191)
        VR_Cheat_TogglePowerup(invincible_finished, invincible_time, IID_INVULNERABILITY, "Pentagram of Protection");
    if(self.impulse == 192)
        VR_Cheat_TogglePowerup(invisible_finished, invisible_time, IID_INVISIBILITY, "Ring of Shadows");
    if(self.impulse == 193)
        VR_Cheat_TogglePowerup(radsuit_finished, rad_time, IID_SUIT, "Biosuit");
    if(self.impulse == 194)
        VR_Cheat_HealthArmor();
    if(self.impulse == 195)
        VR_Cheat_Die(0);
    if(self.impulse == 196)
        VR_Cheat_Die(1);
    if(self.impulse == 197)
        VR_Cheat_Die(2);
    if(self.impulse == 239) // QVR: every grappling hook's state, its gun's place and its rope (vr_grapple.qc)
    {
        VR_Grapple_Report();
    }
    if(self.impulse == 228) // QVR: the chainsaws in your hands: fuel, engine, cord (vr_chainsaw.qc)
    {
        VR_Chainsaw_Impulse(FALSE);
    }
    if(self.impulse == 218) // QVR: each crowbar lying about and in your hands (vr_crowbar.qc)
    {
        VR_Crowbar_Report();
    }
    if(self.impulse == 213) // QVR: the enemy guns in your hands and their ammo (vr_enemyguns.qc)
    {
        VR_EnemyGuns_Impulse(FALSE);
    }
    if(self.impulse == 208) // QVR test: each thrown axe, stuck or not (vr_axestick.qc)
    {
#ifdef QVR_TESTS
        VR_AxeStick_TestReport();
#endif
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
    if(dump_coord == 1)
        DumpCoordinates();
    self.impulse = 0;
};

void(vector xOrigin, float xHand) PlayerLightningGunEffect =
{
    makeforward(VRGetWeaponFireRot(xHand));
    traceline(VR_ShotBackPoint(xHand, xOrigin, v_forward), xOrigin + v_forward*600, TRUE | MOVE_PORTALS, self); // as W_FireLightning

    // QVR: through a teleporter (vr_portals.cpp), two beams: from the gun into the gate (the client draws it from the gun as
    // drawn, this long), and from the gate's far side on (the world's: drawn where it is sent)
    vector beamEnd = trace_endpos;
    if(portal_crossings() > 0)
    {
        vector farFrom = portal_exit(portal_crossings() - 1);
        beamEnd = portal_entry(0);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity(MSG_BROADCAST, world);
        WriteByte(MSG_BROADCAST, xHand); /* disambiguator */
        WriteVec3(MSG_BROADCAST, farFrom);
        WriteVec3(MSG_BROADCAST, trace_endpos);
    }

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, xHand); /* disambiguator */
    WriteVec3(MSG_BROADCAST, xOrigin);
    WriteVec3(MSG_BROADCAST, beamEnd);
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
    AD_WidowmakerSpinHook(xHand, VRGetFireButtonPressed(xHand) && !VR_Forcegrab_IsHandBusy(self, xHand));

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
            sound(self, VRGetHandChannel(xHand), "lavagun/snail.wav", 1, ATTN_NORM);
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
