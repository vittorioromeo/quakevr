// vr_wpnholster.qc -- holsters (get/set, models, hover), force grab touches, throwing and dropping weapons, taking and giving them, the weapon frame, SuperDamageSound.
// Split from weapons.qc (a move): compiled right after it (progs.src), the same code in the same order.

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

// (A holster's weapon's magazine: its record's, VRGetEntHolsterWeaponInst, vr_weaponinst.qc.)

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

float(float xHotspot, float xWeapon, entity xInst) isReloadHolsterHotspot =
{
    float mode = VR_Reload_ModeOfInst(xWeapon, xInst); // (the weapon's own override, else the global)
    if(mode == QVR_RELOADMODE_ALLHOLSTERS)
    {
        return TRUE;
    }

    if(mode == QVR_RELOADMODE_NONE)
    {
        return FALSE;
    }

    return xHotspot == QVR_HS_LEFT_HIP_HOLSTER ||
           xHotspot == QVR_HS_RIGHT_HIP_HOLSTER;
}

// A force-grabbable thing's think (thrown and dropped weapons, pickups as objects): water, dust, settling after a throw.
// In the engine (propthink, vr_propthink.cpp: 5-18% of all the QuakeC's statements were here; docs/vr-port/QC_VM.md).
float(entity e) VR_EnemyGun_DepletedSkipped; // (vr_wpnforcegrab.qc)
void() forcegrabbable_think_impl =
{
    // A spent enemy gun (the force grab skips it) has no grab sparkles either: nothing says it could be force grabbed.
    propthink(VR_EnemyGun_DepletedSkipped(self) ? 0 : vr_weapondrop_particles_on);
}

void() forcegrabbable_think =
{
    forcegrabbable_think_impl();
    self.nextthink = time + 0.02;
}

// A running chainsaw dropped or thrown runs on a moment as it lies (vr_chainsaw.qc).
void(entity xProp) VR_Saw_PropRuns;
void(entity xProp) VR_Saw_PropStop;
void(entity xProp) VR_Saw_PropTaken;
void() VR_Saw_PropThink;
void(entity xNew) VR_EnemyWeapons_MakeRoom;

void() wpnthrow_think =
{
    if(deathmatch && time > GetThrownWeaponTimeout(self))
    {
        VR_Saw_PropStop(self);
        SUB_Remove();
        return;
    }

    VR_Saw_PropThink();

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

void() forcegrabbable_touch =
{
    if(self.owner != world && other == self.owner)
        return;
    if(other.solid == SOLID_TRIGGER)
        return;    // trigger field, do nothing

    // Not thrown (or its throw over), but as fast into a monster or player as a throw: it hurts as one (VR_Prop_Flung:
    // a flail on the grapple's rope, a batted or knocked prop; their speed towards each other, never a resting contact,
    // nor one leaving it: a prop batted away still overlaps your box, and touchNearby's hit box meets you as it starts).
    VR_Prop_Flung(0, 0, TRUE);

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
    // (A heavy thing's lower: it can't be thrown as fast. VR_Thrown_HitSpeed.)
    if(self.throwhit == QVR_THROWHIT_NEVER_HIT && other.takedamage && vlen(self.velocity) < VR_Thrown_HitSpeed(self))
    {
        dprint(sprintf("throw hit: %s into %s at %.1f m/s, under %.1f: no damage, the throw is over\n",
            self.netname != "" ? self.netname : self.classname, other.classname, vlen(self.velocity) / VRMetersToUnits(),
            VR_Thrown_HitSpeed(self) / VRMetersToUnits()));
        self.solid = SOLID_NOT_BUT_TOUCHABLE;
        self.throwhit = QVR_THROWHIT_HIT;
    }

    if(self.throwhit == QVR_THROWHIT_NEVER_HIT)
    {
        float finalDmg = VR_Thrown_Damage(self, vlen(self.velocity));

        // hit something that bleeds
        if(GetThrownWeaponDealDamage(self) && other.takedamage)
        {
            VR_SoundVaried(self, CHAN_AUTO, VR_Hit_Metal(other) ? VR_Metal_Sound(VR_METAL_BLADE) : "fisthit.wav", 1, ATTN_NORM); // (an explosive box: a clang)
            VR_Blunt_HitLayer(self, CHAN_AUTO, other);

            // Where it struck, and going which way (not the trace globals, stale in a touch).
            float posDmgType = PositionalDamage(other, self.origin, normalize(self.velocity));
            float adjDmg = finalDmg * PositionalDamageFactor(posDmgType);

            spawn_touchblood(adjDmg);
            dprint(sprintf("throw hit: %s into %s at %.0f u/s (%.1f m/s; hurts from %.1f): %.1f damage (%.1f before where;",
                self.netname != "" ? self.netname : self.classname, other.classname, vlen(self.velocity),
                vlen(self.velocity) / VRMetersToUnits(), VR_Thrown_HitSpeed(self) / VRMetersToUnits(), adjDmg, finalDmg));
            dprint(sprintf(" hands %g: x%.2f)\n", self.vr_throw_2h ? 2 : 1, VR_Thrown_2hMult(self)));
            entity thrower = VR_Throw_Thrower(self); // (its owner; past vr_throw_self_grace the player who threw it)
            if(other == thrower)
            {
                dprint(sprintf("throw: %s fell on its thrower at %.0f u/s, %.2f s after the throw: %.1f damage\n",
                    self.classname, vlen(self.velocity), time - self.carry_letgo_time, adjDmg));
            }
            float pop = VR_Dismember_ThrownArm(other, self, thrower, adjDmg); // VR: a heavy blunt thing at the head pops it if it kills (vr_decap.qc)
            float wasAlive = other.health > 0 && !other.vr_corpse && !other.vr_corpse_dying;
            VR_Decap_Damage(other, self, thrower, adjDmg, pop);
            // VR: an incandescent blade thrown sets what it struck on fire (vr_hotblade.qc).
            VR_HotBlade_ThrownStruck(self, other, thrower, wasAlive, self.origin, normalize(self.velocity));
            self.vr_fling_hit = time; // (not hit again as a flung prop as it bounces off: VR_FLING_REHIT)
        }
        // hit wall
        else
        {
            if(GetThrownWeaponMetalNoiseOnTouch(self))
            {
                VR_SoundVaried(self, CHAN_AUTO, "player/axhit2.wav", 0.65, ATTN_NORM);
            }

            particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_BULLETPUFF, 24);
        }

        if(GetThrownWeaponDisappearOnHit(self) == TRUE)
        {
            VR_Saw_PropStop(self);
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
     float* xOutEntWeapon, float* xOutEntWeaponFlags)
     wpnthrow_handtouch_impl =
{
    if(ent.vr_wpn_from == entPlayer)
    {
        // Your own weapon taken back (thrown, dropped or let go of, caught, pulled by a force grab): the hand closing on
        // its handle (vr/phys/grab_wood1..3 an axe's, grab_metal1..3 the rest's, at 0.9: a third of the chime's loudness,
        // a third its length), from the hand, a shorter pulse; Quake's pickup chime only for a weapon new to you.
        float k = floor(random() * 3);
        string grab = k < 1 ? "vr/phys/grab_metal1.wav" : (k < 2 ? "vr/phys/grab_metal2.wav" : "vr/phys/grab_metal3.wav");
        if(ent.weapon == WID_AXE)
        {
            grab = k < 1 ? "vr/phys/grab_wood1.wav" : (k < 2 ? "vr/phys/grab_wood2.wav" : "vr/phys/grab_wood3.wav");
        }
        VR_SoundVaried(entPlayer, VRGetHandChannel(xHand), grab, 0.9, ATTN_NORM);
        VRGunHaptic(xHand, 0.12, 75, 0.7);
        dprint(sprintf("weapon: %s taken back (%s)\n", ent.classname, grab));
    }
    else
    {
        VR_SoundVaried(ent, CHAN_AUTO, "weapons/pkup.wav", 1, ATTN_NORM);
        VRGunHaptic(xHand, 0.3, 75, 1.0);
    }
    if(VR_AxeStick_IsStuck(ent))
    {
        VR_AxeStick_PulledOut(ent); // out of what it was stuck in (vr_axestick.qc)
    }

    // Gripped nearer one of its grip or blade hotspots than its handle (vr_weapon_grab_hotspots, the engine's view finds
    // it): carried by that hotspot, as the hand-off leaves a weapon (QVR_WPNFLAG_FOREGRIP_CARRIED: drawn hanging from
    // the hand, unusable until a hand takes its handle). A force grab's catch takes the handle, to use it.
    // (BY_HANDLE, func_weapon_grabbable's spawnflag 2: always its handle, ready to use: the trailer scene's Super Axe, so
    // that the cut that beheads is never a carried weapon's, VR_Decap_Slash)
    float spot = (forcegrabCatching || ent.vr_grab_by_handle) ? 0 : weapongrabspot(entPlayer, xHand, ent);

    if(cvar_hget(cvarh_developer))
    {
        dprint(sprintf("weapon: %s taken into the %s hand, %g units from its handle%s\n", ent.netname,
            xHand == cVR_MainHand ? "main" : "off", vlen(VRGetEntHandPos(entPlayer, xHand) - ent.origin),
            spot == 5 ? ", anywhere on it" : spot ? sprintf(", by its hotspot %g", spot - 1) : ""));
    }

    VR_Saw_PropTaken(ent); // a chainsaw still running: its fuel as burnt, its sound the hand's from now on
    float flags = ent.weaponflags - (ent.weaponflags & QVR_WPNFLAG_FOREGRIP_CARRIED);
    if(spot)
    {
        flags = flags | QVR_WPNFLAG_FOREGRIP_CARRIED;
    }
    *xOutEntWeapon = ent.weapon;
    *xOutEntWeaponFlags = flags;
    VRSetEntWeaponInst(entPlayer, xHand, ent.weaponinst ? ent.weaponinst : WeaponInst_New(0)); // (its magazine, its id)
    if(cvar_hget(cvarh_developer))
    {
        dprint(sprintf("weapon: %s hand takes %s\n", xHand == cVR_MainHand ? "main" : "off", VRWeaponInstDesc(entPlayer, xHand)));
    }
    VR_FlailHead_Taken(ent, entPlayer, xHand); // AD support: a flail's head lying about becomes the hand's (vr_ad_melee.qc)
    remove(ent);

    self = entPlayer;
    W_SetCurrentAmmo();
}

// A mid-air catch (vr_weapon_catch_window; ROUND22.md, "Mid-air catches"): a weapon in the air (not lying about, not
// one a force grab pulls) is caught by a grip closed up to that long before it reaches the fist, not only 0.125 s; and
// the hand that just let it go (a toss to re-grip it, a flip) may close again at once (VR_HandGrabUtil_StartedHandGrabbing
// waits 0.125 s after a release): it catches it once it comes back down, from 0.05 s after the release (not the throw's
// own fist closing again as it leaves).
float(entity ent, entity entPlayer, float xHand) wpnthrow_midair_catch =
{
    float window = cvar("vr_weapon_catch_window");
    if(window <= 0 || (ent.flags & FL_ONGROUND) || ent.fg_state == 1 || vlen(ent.velocity) < 30)
    {
        return FALSE;
    }
    if(!VR_HandGrabUtil_IsHandGrabbing(entPlayer, xHand))
    {
        return FALSE;
    }
    float grabbed = VR_HandGrabUtil_Impl_GetLastGrabTime(entPlayer, xHand);
    float released = VR_HandGrabUtil_Impl_GetLastReleaseTime(entPlayer, xHand);
    if(grabbed <= released || time >= grabbed + window)
    {
        return FALSE;
    }
    if(released + 0.125 > grabbed)
    {
        return ent.velocity_z < 0 && time >= released + 0.05;
    }
    return TRUE;
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

    float performWeaponSwitch = FALSE;

    // Must catch it: the grab started as it touches, or a force grab caught in time.
    if(VRGetHandtouchHand(ent) == xHand && (pStartedGrabbing || forcegrabCatching || wpnthrow_midair_catch(ent, entPlayer, xHand)))
    {
        performWeaponSwitch = TRUE;
    }

    if(performWeaponSwitch)
    {
        wpnthrow_handtouch_impl(xHand, ent, entPlayer,
                                pWeaponPtr, pWeaponFlagsPtr);
        return TRUE;
    }

    return FALSE;
}

void(entity entWpn) wpnthrow_handtouch_ent =
{
    if(wpnthrow_handtouch_ent_hand(entWpn, cVR_OffHand)) { return; }
    if(wpnthrow_handtouch_ent_hand(entWpn, cVR_MainHand)) { return; }
}

void() wpnthrow_handtouch =
{
    if(cvar_hget(cvarh_vr_debug_axestick) >= 2 && VR_AxeStick_IsStuck(self))
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
    entity thrower, float xWeapon, float xWeaponFlags, entity xInst,
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
    newmis.vr_throw_self = 0;
    VR_DamageCredit_SetWeapon(newmis, xWeapon); // (its blow the weapon's: vr_dmgcredit.qc)

    newmis.throwstabilize = 8;
    newmis.throwstabilizedim = WeaponIdToThrowStabilizeDim(xWeapon);

    newmis.angles = xRotation;
    newmis.angles_x = newmis.angles_x * -1;

    newmis.touch = wpnthrow_touch;
    newmis.handtouch = wpnthrow_handtouch;

    newmis.classname = "thrown_weapon";
    newmis.netname = WeaponNameOf(xWeapon, xWeaponFlags);

    newmis.avelocity = xAVel;

    newmis.waterlevel = 0;
    newmis.ammocounter = 0; // water frame counter
    newmis.think = wpnthrow_think;
    newmis.nextthink = time + 0.02;

    SetThrownWeaponTimeout(newmis, time + 10); // removal time (timeout) in deathmatch

    newmis.weapon = xWeapon;
    newmis.weaponflags = xWeaponFlags;
    newmis.weaponinst = xInst ? xInst : WeaponInst_New(0); // its record: its magazine, its id

    float tmp = WeaponIdToThrowBounds(xWeapon);

    vector tmp_bounds;
    tmp_bounds_x = tmp;
    tmp_bounds_y = tmp;
    tmp_bounds_z = tmp;

    setmodel(newmis, AD_WeaponModel(xWeapon, xWeaponFlags, newmis.weaponinst)); // (AD support: an upgraded one's own)
    setsize(newmis, -tmp_bounds, tmp_bounds);
    setorigin(newmis, xOrigin);

    newmis.velocity = xThrowVel + thrower.velocity * VR_Throw_Inherit(thrower); // (a player's: vr_throw_inherit of it)

    // Thrown hard (as fast as a carried thing must be let go of to be a missile: VR_THROW_MISSILE_SPEED, a heavy one's
    // less), it spares its thrower only for vr_throw_self_grace (VR_Throw_SelfStart): thrown up, it can fall back on
    // him. Let go of gently (dropped, set down) it is his for good, as it always was: a long heavy weapon let go of at
    // the body fell through his box at about the speed its throw hurts at (the chainsaw's, the laser cannon's: 115 u/s,
    // reached 0.35 s into the fall; NOTES.md vrfiringrange_2026-10-01_22-33-36).
    if(vlen(xThrowVel) >= VR_THROW_MISSILE_SPEED * VR_Thrown_Lenient(newmis))
    {
        VR_Throw_SelfStart(newmis, thrower); // (a player's throw spares him only for vr_throw_self_grace)
    }
    else if(thrower.flags & FL_CLIENT)
    {
        dprint(sprintf("thrown: %s let go of gently (%.0f u/s, under %.0f): never hurts %s\n", newmis.netname,
            vlen(xThrowVel), VR_THROW_MISSILE_SPEED * VR_Thrown_Lenient(newmis), thrower.netname));
    }

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

// Shared by the two throws (DropWeaponInHandScaled; vr_carry.qc VR_Carry_Throw). The units/s^2 a thrown weapon or
// prop falls at: vr_throw_gravity (m/s^2) when set, else the world's.
float() VR_Throw_Gravity =
{
    if(cvar_hget(cvarh_vr_throw_gravity) > 0)
    {
        return cvar_hget(cvarh_vr_throw_gravity) * VRMetersToUnits();
    }
    return cvar_hget(cvarh_sv_gravity);
};

// The .gravity that makes a thrown thing fall at VR_Throw_Gravity (0: no world gravity to scale, leave it).
float() VR_Throw_GravityScale =
{
    if(cvar_hget(cvarh_sv_gravity) > 0)
    {
        return VR_Throw_Gravity() / cvar_hget(cvarh_sv_gravity);
    }
    return 0;
};

// A thrown thing's .throwdmgspeed: the same damage for the same hand speed as the old x120 scaling.
float() VR_Throw_DmgSpeed =
{
    return 700 * VRMetersToUnits() / 120;
};

// `xInst`: the weapon's record (vr_weaponinst.qc): a hand's, moved to the prop, or a new weapon's (WeaponInst_New).
entity(entity thrower, float xWeapon, float xWeaponFlags, entity xInst,
    vector xOrigin, vector xRotation,
    vector xThrowVel, vector xAVel,
    float xDisappearOnHit,
    float xDealDamage,
    float xBaseDamage) CreateThrownWeapon =
{
    if(!VR_Pack_WeaponAvailable(xWeapon)) { return world; }
    xWeaponFlags = VR_Pack_WeaponFlags(xWeapon, xWeaponFlags);

    entity newmis = spawn();

    MakeThrown(newmis,
        thrower, xWeapon, xWeaponFlags, xInst,
        xOrigin, xRotation,
        xThrowVel, xAVel,
        xDisappearOnHit,
        xDealDamage,
        xBaseDamage);

    if((thrower.flags & FL_MONSTER) || thrower.vr_was_monster) // (a gibbed one's head: TryEnemyDrop after ThrowHead)
    {
        newmis.vr_monster_drop = time; // dropped by a monster: it can't hurt players just yet (vr_prop_drop_grace)
        newmis.weaponinst.vr_enemy_drop = 1; // (its record: still a monster's drop in a hand, a holster, dropped again)
    }

    newmis.vr_drop_born = time;
    // Spent, a single-use enemy gun or a chainsaw is never fired again: no loot beam, no force grab
    // (vr_enemy_weapon_depleted_loot).
    newmis.vr_depleted = VR_Weapon_Spent(xWeapon, newmis.weaponinst);
    if(xWeapon == WID_FLAREGUN)
    {
        newmis.skin = VR_FlareGun_Skin(newmis.weaponinst.wi_clip); // (its muzzles sooted by the flares fired: vr_flaregun.qc)
    }
    VR_EnemyWeapons_MakeRoom(newmis); // (a single-use enemy weapon: at most vr_enemy_weapon_drop_max lie about)
    VR_FuelTank_Attach(newmis, thrower); // AD support: a flamethrower's or poison sprayer's tank with it (vr_ad_enemyguns.qc)
    VR_FlailHead_Attach(newmis, thrower); // AD support: the death sergeant's flail's head on its chain (vr_ad_melee.qc)

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
        WeaponInst_New(WeaponIdToBaseClipSize(xWeapon)) /* a new one, loaded */,
        xOrigin, randomVec(180),
        LegacyThrowVelocity(xWeapon, throwVel), LegacyThrowSpin(throwVel, randomVec(20)),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(xWeapon)
    );
}

void(entity thrower, float xWeapon, vector xOrigin) CreateAmmoBoxWeaponDrop =
{
    vector throwVel;

    throwVel = randomVec(0.1);
    throwVel_z = 0.2 + random() * 0.1;

    CreateThrownWeapon(thrower, xWeapon, 0 /* weaponflags */,
        WeaponInst_New(WeaponIdToBaseClipSize(xWeapon)) /* a new one, loaded */,
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

    if(cvar_hget(cvarh_developer))
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

// `xVelScale`: times the throw's velocity and spin (1: thrown; vr_weight_drop_velocity: wrenched out, the grip lost
// rather than a throw, which the throw assist does not aim either).
// The hand's weapon (`xWeapon`, `xWeaponFlags`: the hand's) thrown or dropped: a prop, with the hand's record (its
// magazine, its id).
void(entity ent, float xHand, float xWeapon, float xWeaponFlags, float xVelScale) DropWeaponInHandScaled =
{
    if(VRIsHandWeaponless(ent, xHand))
    {
        return;
    }
    entity inst = VRGetEntWeaponInst(ent, xHand);

    float twoHThrowVelMult;
    float throwHands = 1;
    vector throwVel;
    vector throwFlick;
    if(self.last_2h_time > 0 && time - self.last_2h_time < 1)
    {
        dprint(sprintf("throw: a weapon held in both hands until %.3f s ago\n", time - self.last_2h_time));
    }
    if(VR_Throw_2hRecent(self.last_2h_time)) // (its foregrip let go of within vr_throw_2h_window: vr_carry.qc)
    {
        twoHThrowVelMult = cvar_hget(cvarh_vr_2h_throw_velocity_mult);
        throwVel = VRGetHandThrowVel(xHand) + VRGetHandThrowVel(VRGetOtherHand(xHand));
        throwVel /= 2.0;
        throwVel *= twoHThrowVelMult;
        throwFlick = (VRGetHandThrowFlick(xHand) + VRGetHandThrowFlick(VRGetOtherHand(xHand))) * 0.5 * twoHThrowVelMult;
        throwHands = 2;
    }
    else
    {
        twoHThrowVelMult = 1.0;
        throwVel = VRGetHandThrowVel(xHand);
        throwFlick = VRGetHandThrowFlick(xHand);
    }

    // The hand's velocity (m/s) at true scale: the weapon leaves the hand as fast as the hand
    // moved, a little more for real throws (the gain), as fast as its mass lets one hand or two throw it (throws by
    // weight: throwvelocity), a little less for heavy weapons, and falls at vr_throw_gravity rather than Quake's 2.5g.
    float m2u = VRMetersToUnits();
    float weight = 1 + (WeaponIdToThrowMult(xWeapon) - 1) * cvar_hget(cvarh_vr_throw_weight_influence);
    float throwMass = weaponvalue(WeaponIdToModel(xWeapon, xWeaponFlags), "w_mass");
    vector asThrown = throwvelocity(throwVel, throwFlick, throwMass, throwHands) * m2u * weight *
                      cvar_hget(cvarh_vr_weapon_throw_velocity_mult);
    vector throwVelUnits = asThrown * xVelScale;

    float throwGravity = VR_Throw_Gravity();

    // It left the hand a moment ago: it starts where it would be by now (vr_carry.qc VR_Throw_BackProject, a prop's too).
    vector handNow = VRGetHandPos(xHand);
    vector origin = VR_Throw_BackProject(ent, xHand, handNow, handNow, throwVelUnits, '0 0 0', '0 0 0', throwGravity);
    throwVelUnits = vr_throw_vel;
    float age = vr_throw_age;

    // Carried off its handle (the hand-off, a hotspot, anywhere: QVR_WPNFLAG_FOREGRIP_CARRIED): it leaves from where it
    // is drawn, not as if held by its handle (the local player's view knows; else from the hand).
    vector throwRot = VRGetHandRot(xHand);
    if((xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED) && weapondrawnpose(ent, xHand, 2) != '0 0 0')
    {
        vector carriedAt = origin + (weapondrawnpose(ent, xHand, 0) - handNow);
        traceline(origin, carriedAt, TRUE, ent); // never inside a wall
        origin = trace_endpos;
        if(trace_fraction < 1)
        {
            origin = origin - normalize(carriedAt - handNow) * 2;
        }
        throwRot = weapondrawnpose(ent, xHand, 1);
        dprint(sprintf("thrown: carried off its handle, from %v (the hand at %v), angles %v\n", origin, handNow, throwRot));
    }

    if(xVelScale == 1)
    {
        throwVelUnits = VRThrowAssist(ent, origin, throwVelUnits, throwGravity);
    }

    // A running chainsaw's engine runs on a moment as it lies (VR_Saw_PropRuns); its chain stops.
    float sawRuns = xWeapon == WID_CHAINSAW && (xWeaponFlags & QVR_WPNFLAG_SAW_RUNNING) && inst.wi_clip > 0 &&
                    cvar_hget(cvarh_vr_chainsaw_drop_run) > 0;
    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED); // a hand's state, not the weapon's
    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_SAW_STATE); // (the hand's chainsaw's engine stops)

    // The lightning gun's hum stops with it (it played on for up to 0.6 s from the empty hand; its beam ends at once on
    // the client: VR_BeamGone; NOTES.md vrfiringrange_2026-10-01_11-41-09).
    if(xWeapon == WID_LIGHTNING)
    {
        sound(ent, VRGetGunChannel(xHand), "misc/null.wav", 1, ATTN_NORM);
    }

    entity thrown = CreateThrownWeapon(ent, xWeapon, xWeaponFlags, inst,
        origin, throwRot,
        throwVelUnits, '0 0 0',
        FALSE /* disappear on hit */,
        TRUE /* deal damage */,
        WeaponIdToThrowDamage(xWeapon));

    if(thrown != world)
    {
        if(ent.flags & FL_CLIENT)
        {
            thrown.vr_wpn_from = ent; // (taken back: a regrip, not a pickup: wpnthrow_handtouch_impl)
        }
        if(sawRuns)
        {
            VR_Saw_PropRuns(thrown);
        }

        // (Its blood goes with it: its record, its id, vr_weaponinst.qc.)

        thrown.throwdmgspeed = VR_Throw_DmgSpeed();

        float gravityScale = VR_Throw_GravityScale();
        if(gravityScale)
        {
            thrown.gravity = gravityScale;
        }

        if(throwHands == 2 && xVelScale == 1)
        {
            thrown.vr_throw_2h = 1; // (its hits hurt vr_throw_2h_damage times more: VR_Thrown_Damage)
            VR_Throw_TwoHanded(ent, thrown); // (neither hand strikes in its follow-through)
        }

        // Flies, spins, bounces and settles as a rigid body (engine: vr_rigid.cpp).
        thrown.vr_rigid = 1;
        thrown.vr_spin = VR_Throw_Spin(VRThrowSpinCapped(VRGetHandAVel(xHand)), throwMass, throwHands) * xVelScale;

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

    VR_SoundVaried(ent, VRGetHandChannel(xHand), "knight/sword2.wav", 0.7, ATTN_NORM);
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmoFor(xHand);
}

void(entity ent, float xHand, float xWeapon, float xWeaponFlags) DropWeaponInHand =
{
    DropWeaponInHandScaled(ent, xHand, xWeapon, xWeaponFlags, 1);
}

// `xInst`: the weapon's record (the hand's, moved; a quick slot's copy: WeaponInst_Clone).
void(entity entPlayer, float xHand, float xHotspot, float xWeapon, float xWeaponFlags, entity xInst)
VRPutWeaponInHolster =
{
    VR_SoundVaried(entPlayer, VRGetHandChannel(xHand), "weapons/holster1.wav", 1, ATTN_NORM);

    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED); // a hand's state, not the weapon's
    xWeaponFlags = xWeaponFlags - (xWeaponFlags & QVR_WPNFLAG_SAW_STATE); // a chainsaw's engine stops
    setHolsterWeapon(xHotspot, xWeapon);
    setHolsterWeaponModel(xHotspot, AD_WeaponModel(xWeapon, xWeaponFlags, xInst)); // (AD support: an upgraded one's own)
    setHolsterWeaponFlags(xHotspot, xWeaponFlags);
    VRSetEntHolsterWeaponInst(self, xHotspot, xInst);
}

// The hand's magazine (its weapon's record) filled from the player's ammo.
void(entity entPlayer, float xHand, float xWeapon, float xWeaponFlags)
VRReloadWeapon =
{
    float clip = VRGetEntWeaponClip(entPlayer, xHand);
    VRGunHaptic(xHand, 0.25, 70, 1.0);
    VR_SoundVaried(entPlayer, VRGetHandChannel(xHand), "reload1.wav", 1, ATTN_NORM);

    float ammoId = WeaponIdToAmmoId(xWeapon, xWeaponFlags);
    float missingAmmo = VR_ClipSizeIn(entPlayer, xHand) - clip;
    float remainingAmmo = VR_AmmoUtil_GetAmmoCountByAmmoId(entPlayer, ammoId);
    float clampedMissingAmmo = clamp(missingAmmo, 0, remainingAmmo);

    delAmmoAndUpdateCounter(xHand, ammoId, clampedMissingAmmo);
    VRSetEntWeaponClip(entPlayer, xHand, clip + clampedMissingAmmo);

    // The super shotgun, broken open, throws out its spent shells: flung by a flick reload
    // (vr_shells.cpp).
    if(xWeapon == WID_SUPER_SHOTGUN && missingAmmo > 0 && !AD_WidowmakerIn(entPlayer, xHand)) // (the Widowmaker's go as it fires)
    {
        entity oself = self;
        self = entPlayer;
        ejectcasings(xHand, QVR_CASING_SHOTGUNSHELL, clamp(missingAmmo, 1, 2), 0,
            VR_HandGrabUtil_IsHandReloadFlicking(entPlayer, xHand) ? QVR_EJECT_FLICK : 0);
        self = oself;
    }
}

void(entity entPlayer, float xHand, float xWeapon, float xWeaponFlags)
VRUnloadWeapon =
{
    float clip = VRGetEntWeaponClip(entPlayer, xHand);
    VRGunHaptic(xHand, 0.25, 70, 1.0);
    VR_SoundVaried(entPlayer, VRGetHandChannel(xHand), "reload1.wav", 1, ATTN_NORM);

    float ammoId = WeaponIdToAmmoId(xWeapon, xWeaponFlags);

    addAmmoAndUpdateCounter(xHand, ammoId, clip);
    VRSetEntWeaponClip(entPlayer, xHand, 0);
}

void(entity entPlayer, float xHand, float xWeapon, float xWeaponFlags)
VRTryReloadWeapon =
{
    if(VRIsWeaponReloadingEnabledFor(entPlayer, xHand) && !VR_Reload_ManualForInst(xWeapon, VRGetEntWeaponInst(entPlayer, xHand)) &&
       (VRGetEntWeaponClip(entPlayer, xHand) < VR_ClipSizeIn(entPlayer, xHand)) &&
       PlayerHasAmmoForWeapon(entPlayer, xWeapon, xWeaponFlags))
    {
        VRReloadWeapon(entPlayer, xHand, xWeapon, xWeaponFlags);
    }
}

void(entity entPlayer, float xHand, float xWeapon, float xWeaponFlags)
VRTryUnloadWeapon =
{
    if(VRIsWeaponReloadingEnabledFor(entPlayer, xHand) && VRGetEntWeaponClip(entPlayer, xHand) > 0)
    {
        VRUnloadWeapon(entPlayer, xHand, xWeapon, xWeaponFlags);
    }
}

// The save's original weapon instance stays in this hand briefly after loading (both release checks agree).
float(entity ent, float xHand) VRWeaponLoadProtected =
{
    entity restored = VR_HAND_FIELD(ent, xHand, vr_weapon_load_main, vr_weapon_load_off);
    return time < ent.vr_weapon_load_until && restored != world && restored == VRGetEntWeaponInst(ent, xHand);
}

// A test scene's weapon given into this hand (vr_scenario's give: VR_Scene_HoldGiven) is held as if just grabbed: with
// vr_weapon_grip_mode 0 it stays, its grip unpressed, until that hand's grip is pressed; from then on the grip holds it
// as usual (let go on release). Sticky mode needs nothing: it waits for a squeeze and a release already.
float(entity ent, float xHand) VRWeaponGivenHeld =
{
    entity given = VR_HAND_FIELD(ent, xHand, vr_weapon_given_main, vr_weapon_given_off);
    if(given == world)
    {
        return FALSE;
    }
    if(given != VRGetEntWeaponInst(ent, xHand) || VR_HandGrabUtil_IsHandGrabbing(ent, xHand))
    {
        // Gone from the hand, or the grip took it.
        if(xHand == cVR_MainHand)
        {
            ent.vr_weapon_given_main = world;
        }
        else
        {
            ent.vr_weapon_given_off = world;
        }
        return FALSE;
    }
    return cvar_hget(cvarh_vr_weapon_grip_mode) != 1;
}

// (Engine, vr_scenario: a give placed the weapon in `xHand` of `p`.)
void(entity p, float xHand) VR_Scene_HoldGiven =
{
    if(xHand == cVR_MainHand)
    {
        p.vr_weapon_given_main = VRGetEntWeaponInst(p, cVR_MainHand);
    }
    else
    {
        p.vr_weapon_given_off = VRGetEntWeaponInst(p, cVR_OffHand);
    }
}

// Whether the weapon in `xHand` is let go of (to be thrown, holstered or switched to the other
// hand): with vr_weapon_grip_mode 0 whenever its grip is not held; with 1 ("sticky") only once
// the grip is let go a second time since the weapon came to the hand.
float(entity ent, float xHand, float xWeapon) VRWeaponLetGo =
{
    if(VRWeaponLoadProtected(ent, xHand) || VRWeaponGivenHeld(ent, xHand))
    {
        return FALSE;
    }
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
// (2H hotspots: unusable, as you can only dual wield with no weapon out; the grenade pouch holds grenades only, the ammo
// pouch rounds).
float(float xHotspot) VRLetGoDropsAt =
{
    return xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB ||
           xHotspot == QVR_HS_CARRIED_GRIP ||
           xHotspot == QVR_HS_GRENADE_POUCH ||
           xHotspot == QVR_HS_AMMO_POUCH;
}

// Whether the weapon in `xHand` leaves it this frame (DoHandImpl's let-go: thrown, dropped or holstered), as
// VRWeaponLetGo will say, without its side effects. W_WeaponFrameImpl runs earlier in the frame: a trigger held through
// a throw fires no last shot as the weapon leaves.
float(entity ent, float xHand) VRWeaponLeavingHand =
{
    if(VRIsHandWeaponless(ent, xHand) || VR_HandGrabUtil_IsHandGrabbing(ent, xHand) || VRWeaponLoadProtected(ent, xHand) ||
       VRWeaponGivenHeld(ent, xHand))
    {
        return FALSE;
    }

    if(cvar_hget(cvarh_vr_weapon_grip_mode) == 1)
    {
        float bit = xHand == cVR_MainHand ? 2 : 1;
        float lastWeapon = VR_HAND_FIELD(ent, xHand, stickyweaponmain, stickyweaponoff);
        float armedNow = VR_HandGrabUtil_IsHandPrevGrabbing(ent, xHand) && (ent.stickyarmed & bit) &&
                         VRGetEntWeapon(ent, xHand) == lastWeapon;
        if(!(ent.stickypending & bit) && !armedNow)
        {
            return FALSE;
        }
    }

    float hotspot = VR_HAND_FIELD(ent, xHand, mainhand_hotspot, offhand_hotspot);
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

// Hand-off (vr_2h_handoff, ROUND16.md (removed 2026-10-06; git history)): the hand holding a weapon two-handed lets go while
// the other, empty hand still grips it (steadying it: VRGet2HAiming, or a moment ago). The weapon
// stays in that hand instead of falling: it hangs from where that hand holds it (a gun's foregrip, a
// sword's grip or blade: QVR_WPNFLAG_FOREGRIP_CARRIED: the client draws it there; it doesn't fire or
// reload) until a hand takes its handle again. A hand that
// holds a weapon of its own is never helping, so dual wielding keeps dropping as before.
// Weapons held anywhere (vr_weapon_grab_anywhere, the engine's weaponanygrip 2): the other hand holding it anywhere on it
// (whatever its mode, and on a weapon carried off its handle too) carries it there, a sword or a crowbar too.
// `xHand`'s weapon to the other hand, carried where that hand holds it, a sword or a crowbar too (by its grip below the
// hand or its blade: it used to change hands, flipped onto its handle; NOTES.md vrfiringrange_2026-10-01_17-01-55). Its
// record goes with it (its magazine, its id).
void(entity ent, float xHand) VRHandOffWeapon =
{
    float other = VRGetOtherHand(xHand);
    float wflags = VRGetEntWeaponFlags(ent, xHand);
    VRSetEntWeapon(ent, other, VRGetEntWeapon(ent, xHand));
    VRSetEntWeaponFlags(ent, other, (wflags - (wflags & QVR_WPNFLAG_FOREGRIP_CARRIED)) | QVR_WPNFLAG_FOREGRIP_CARRIED);
    VRMoveEntWeaponInst(ent, xHand, other);
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmo();

    VR_SoundVaried(ent, VRGetHandChannel(other), "weapons/holster0.wav", 0.5, ATTN_NORM);
    VRGunHaptic(other, 0.08, 75, 0.6);
};

float(entity ent, float xHand, float xWeapon, float xWeaponFlags) VRTryHandOff =
{
    float other = VRGetOtherHand(xHand);
    float anywhere = weaponanygrip(ent, other) == 2;
    if(!cvar_hget(cvarh_vr_2h_handoff) ||
       ((xWeaponFlags & QVR_WPNFLAG_FOREGRIP_CARRIED) && !anywhere) ||
       !(VRGet2HAiming(ent) || time - ent.last_2h_time < 0.1 || anywhere) ||
       !VR_HandGrabUtil_IsHandGrabbing(ent, other) ||
       !VRIsHandEmpty(ent, other))
    {
        return FALSE;
    }

    dprint(sprintf("weapon: %g handed off to the %s hand%s\n", xWeapon, other == cVR_MainHand ? "main" : "off",
        anywhere ? ", held anywhere" : ""));
    VRHandOffWeapon(ent, xHand);
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
    VRMoveEntWeaponInst(ent, other, xHand); // (its magazine, its id)
    VRSetHandEmpty(ent, other);
    W_SetCurrentAmmo();

    VR_SoundVaried(ent, VRGetHandChannel(xHand), "weapons/holster0.wav", 0.5, ATTN_NORM);
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
    // Held two-handed, the other hand keeps it (the hand-off); else it falls: the grip lost, slower than a throw
    // (vr_weight_drop_velocity; NOTES.md vrfiringrange_2026-10-01_00-10).
    if(!VRTryHandOff(self, xHand, wid, flags))
    {
        DropWeaponInHandScaled(self, xHand, wid, flags, max(0, cvar_hget(cvarh_vr_weight_drop_velocity)));
    }
    VRHandStopFiring(self, xHand);
    VRGunHaptic(xHand, 0.15, 60, 1);
    sprint(self, "Too heavy for that: it is wrenched out of your hand!\n");
    dprint(sprintf("weight drop: weapon %g wrenched out of hand %g\n", wid, xHand));
};

// A weapon too big for a holster (WeaponIdToHolsterable) let go at holster `xHotspot`: refused (a buzz, a word; counted
// for the tests). The caller lets it fall.
float vr_holster_refused; // (the tests)
void(entity ent, float xHand, float xHotspot, float xWeapon, float xWeaponFlags) VRHolsterRefused =
{
    vr_holster_refused++;
    sprint(ent, sprintf("The %s is too big for a holster.\n", WeaponNameOf(xWeapon, xWeaponFlags)));
    VRHolsterHapticBuzz(ent, xHand, xHotspot);
    dprint(sprintf("holster: weapon %g (flags %g) refused by holster %g: not holsterable\n", xWeapon, xWeaponFlags, xHotspot));
};

// `xHand`'s weapon into the empty holster `xHotspot` (immersive): its record with it (its magazine, its id); the hand
// empty. (`self` is `ent`: the holster's getters are self's.)
void(entity ent, float xHand, float xHotspot) VRHolsterHandWeapon =
{
    VRPutWeaponInHolster(ent, xHand, xHotspot, VRGetEntWeapon(ent, xHand), VRGetEntWeaponFlags(ent, xHand),
        VRGetEntWeaponInst(ent, xHand));
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmoFor(xHand);
};

// `xHand`'s weapon into the other, empty hand (the hand switch hotspot): its record with it.
void(entity ent, float xHand) VRSwitchWeaponHands =
{
    float other = VRGetOtherHand(xHand);
    float wflags = VRGetEntWeaponFlags(ent, xHand);
    VR_SoundVaried(ent, VRGetHandChannel(other), "knight/sword2.wav", 0.7, ATTN_NORM);
    VRSetEntWeapon(ent, other, VRGetEntWeapon(ent, xHand));
    VRSetEntWeaponFlags(ent, other, wflags - (wflags & QVR_WPNFLAG_FOREGRIP_CARRIED));
    VRMoveEntWeaponInst(ent, xHand, other); // (its magazine, its id)
    VRSetHandEmpty(ent, xHand);
    W_SetCurrentAmmo();
};

void(entity ent, float xHand, float hs, entity inst) VR_MagHolster_DrawSound; // (vr_magholster.qc)

// The weapon in holster `xHotspot` drawn into the empty `xHand`: immersive, the holster's (its record with it: its
// magazine, its id), the holster empty; a quick slot, a copy (WeaponInst_Clone), the holster keeping its own. (`self`
// is `ent`: the holster's getters are self's.)
void(entity ent, float xHand, float xHotspot) VRDrawHolsterWeapon =
{
    float wid = getHolsterWeapon(xHotspot);
    entity holstered = VRGetEntHolsterWeaponInst(ent, xHotspot);
    VR_SoundVaried(ent, VRGetHandChannel(xHand), "weapons/holster0.wav", 1, ATTN_NORM);
    VR_MagHolster_DrawSound(ent, xHand, xHotspot, holstered); // (bound: the chain heard too)
    VRSetEntWeapon(ent, xHand, wid);
    VRSetEntWeaponFlags(ent, xHand, getHolsterWeaponFlags(xHotspot));
    if(getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE)
    {
        VRSetEntWeaponInst(ent, xHand, WeaponInst_Ensure(holstered, wid, 0));
        setHolsterWeapon(xHotspot, WID_FIST);
        setHolsterWeaponModel(xHotspot, WeaponIdToModel(WID_FIST, 0 /* weaponflags */));
        setHolsterWeaponFlags(xHotspot, 0);
        VRSetEntHolsterWeaponInst(ent, xHotspot, world);
    }
    else
    {
        VRSetEntWeaponInst(ent, xHand, WeaponInst_Clone(holstered));
    }
    W_SetCurrentAmmoFor(xHand);
};

// `xHand` lets its weapon go at holster `xHotspot`: a weapon too big for a holster (WeaponIdToHolsterable: AD's hammer
// and mace) is refused and falls, as anywhere (a quick slot keeps no copy); else, immersive, into the empty holster (the
// hand empty), or a quick slot's copy. `xPrevGrabbing`: the hand held on until now (a full holster's buzz).
void(entity ent, float xHand, float xHotspot, float xWeapon, float xWeaponFlags, float xPrevGrabbing) VRLetGoAtHolster =
{
    if(!WeaponIdToHolsterable(xWeapon, xWeaponFlags))
    {
        VRHolsterRefused(ent, xHand, xHotspot, xWeapon, xWeaponFlags);
        if(getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE)
        {
            DropWeaponInHand(ent, xHand, xWeapon, xWeaponFlags);
        }
        return;
    }
    // In "immersive" mode, weapons can only be placed in an empty holster.
    // Holstering a weapon frees your hand up.
    if(getWeaponMode() == QVR_WEAPONMODE_IMMERSIVE)
    {
        if(getHolsterWeapon(xHotspot) == WID_FIST)
        {
            VRHolsterHandWeapon(ent, xHand, xHotspot);
        }
        else if(xPrevGrabbing)
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
            // (its sound: VRPutWeaponInHolster's, from the hand)

            // (The holster keeps a copy: the hand keeps its weapon.)
            VRPutWeaponInHolster(ent, xHand, xHotspot, xWeapon, xWeaponFlags,
                WeaponInst_Clone(VRGetEntWeaponInst(ent, xHand)));
        }
    }
};

void(entity ent, float xHand) VRWeaponLetGoDone =
{
    if(xHand == cVR_MainHand) ent.stickypending &= ~2;
    else                      ent.stickypending &= ~1;
}

// (What moves a weapon moves its record too: VRHolsterHandWeapon, VRSwitchWeaponHands, VRDrawHolsterWeapon,
// VRTryHandOff, DropWeaponInHand; vr_weaponinst.qc.)
void(entity ent, float xHand,
     float xWeapon, float xWeaponFlags,
     float xAttackFinished, float xHotspot,
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
        VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags);
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

        // Held two-handed: the other hand keeps it (the hand-off), wherever this hand lets go: at a holster or the hand
        // switch too (the other hand still holds it: NOTES.md vrfiringrange_2026-10-01_17-09-51, _17-11-37, _17-01-55).
        if(VRTryHandOff(ent, xHand, xWeapon, xWeaponFlags))
        {
        }
        else if(VRLetGoDropsAt(xHotspot))
        {
            // Otherwise it falls, or flies.
            DropWeaponInHand(ent, xHand, xWeapon, xWeaponFlags);
        }
        // Let go at a holster: into it, or refused (VRLetGoAtHolster).
        else if(isHolsterHotspot(xHotspot))
        {
            VRLetGoAtHolster(ent, xHand, xHotspot, xWeapon, xWeaponFlags, handPrevGrabbing);
        }
        // Swap weapon between hands.
        else if(xHotspot == QVR_HS_HAND_SWITCH)
        {
            if(otherHandGrabbing && otherHandEmpty && time >= xOtherAttackFinished)
            {
                VRSwitchWeaponHands(ent, xHand);
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

            if(VRIsWeaponReloadingEnabledFor(ent, xHand) && isReloadHolsterHotspot(xHotspot, xWeapon, VRGetEntWeaponInst(ent, xHand)) && !carried)
            {
                VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags);
            }
        }
        else if(handReloadFlicking && !carried)
        {
            if(VRIsWeaponReloadingEnabledFor(ent, xHand) && VRIsWeaponReloadFlickingEnabledFor(ent, xHand))
            {
                VRTryReloadWeapon(ent, xHand, xWeapon, xWeaponFlags);
            }
        }
    }
    // The empty hand closing on the handle of the weapon the other hand carries off its handle (the hand-off, a hotspot,
    // anywhere): it takes it back, the trigger pulled or not. A hand that held the blade in a fist (the trigger pulled with
    // the grip: a sword's does nothing) closed on the handle the same way, and the force grab's branch below took the grip,
    // so nothing did (NOTES.md vrfiringrange_2026-10-02_00-34-04).
    else if(handEmpty && startedHandGrabbing && xHotspot == QVR_HS_CARRIED_GRIP &&
            VRIsHandCarryingByForegrip(ent, VRGetOtherHand(xHand)))
    {
        VRTakeCarriedWeapon(ent, xHand);
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
        // (The handle of the weapon the other hand carries: taken above.)
        // Not a holster. Do nothing.
        if(xHotspot == QVR_HS_NONE ||
           xHotspot == QVR_HS_CARRIED_GRIP ||
           xHotspot == QVR_HS_OFFHAND_2H_GRAB ||
           xHotspot == QVR_HS_MAINHAND_2H_GRAB ||
           xHotspot == QVR_HS_HAND_SWITCH ||
           xHotspot == QVR_HS_GRENADE_POUCH || // (a grenade from the pouch: VR_HandGrenade_HandFrame)
           xHotspot == QVR_HS_AMMO_POUCH)      // (a round from the ammo pouch: VR_Reload_HandFrame)
        {
            // Intentionally left empty.
        }
        // Attempt unholstering weapon.
        else if(isHolsterHotspot(xHotspot))
        {
            if(getHolsterWeapon(xHotspot) != WID_FIST)
            {
                // In "immersive" mode, unholstering removes a weapon from a holster; in "quick slot" mode it does not.
                VRDrawHolsterWeapon(ent, xHand, xHotspot);
            }
        }
    }
}

void() VR_MagHolster_Frame; // (vr_magholster.qc)

void() W_Frame =
{
    if(self.impulse)
    {
        ImpulseCommands();
    }

    VR_Carry_TwoHandFrame();
    VR_Carry_HandFrame(cVR_MainHand);
    VR_Carry_HandFrame(cVR_OffHand);
    VR_Carry_ItemsFrame(); // (the hands carrying a thing to put away: their zones larger, STAT_QVR_CARRYITEM)
    VR_WallTorch_HandFrame(cVR_MainHand); // a torch gripped on its wall pulled out (vr_walltorch.qc)
    VR_WallTorch_HandFrame(cVR_OffHand);
#ifdef QVR_TESTS
    VR_AI_TestFrame();
    VR_WallTorch_TestShot(); // vr_test_walltorch_shot: a wall torch shot off its wall (vr_walltorch.qc)
#endif
    VR_HandGrenade_HandFrame(cVR_MainHand); // a grenade taken from the pouch at the small of the back (vr_grenade.qc)
    VR_HandGrenade_HandFrame(cVR_OffHand);
    VR_Reload_PlayerFrame(); // what the ammo pouch gives and shows (vr_reload.qc)
    VR_Reload_HandFrame(cVR_MainHand); // a round taken from the ammo pouch on the belt (vr_reload.qc)
    VR_Reload_HandFrame(cVR_OffHand);

    UpdateHolsterHover(self);

    if(VRGet2HAiming(self))
    {
        // Used for 2H throws.
        self.last_2h_time = time;
    }

    VRWeightWrenchFrame(cVR_MainHand);
    VRWeightWrenchFrame(cVR_OffHand);

    DoHandImpl(self, cVR_OffHand, self.weapon2, self.weaponflags2,
               self.offhand_attack_finished, self.offhand_hotspot, self.attack_finished);

    DoHandImpl(self, cVR_MainHand, self.weapon, self.weaponflags,
               self.attack_finished, self.mainhand_hotspot, self.offhand_attack_finished);

    W_Frame_Forcegrab();

    VR_MagHolster_Frame(); // magnetic holsters: binds, and the bound weapons' way home (vr_magholster.qc)
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
