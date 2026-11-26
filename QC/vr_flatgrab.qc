// vr_flatgrab.qc -- flatscreen play's Grab / Pull key (H, vr_flat_key_pull; Toggle by default, vr_flat_grab_hold 1 for
// Hold; client input & reach-out/hold animation in Quake/vr/vr_flatgrenade.cpp).
//
// Uses every available empty hand to physically grab and manipulate props, crates, active or dead ragdolls, gibs and
// loose items/weapons in front of the player, reusing VR's existing VR_Carry_Start / carry2h / ragdollgrabreach paths
// (NOT force-grab). Grabbed objects can be pushed/pulled/carried cleanly, released with H (or tap B), or thrown with
// hold B (vr_flatgrenade.qc).

.float vr_flatgrab_hands;        // bitmask of hands holding an H-grabbed prop or ragdoll (1 off, 2 main)
.float vr_flatgrab_holdmode;     // TRUE while H is held in Hold mode (vr_flat_grab_hold 1)
.float vr_flatgrab_hold_done;    // set when B throws/drops while H is held in Hold mode (don't snatch it back until key up)
.entity vr_flatgrab_ignore;      // recently thrown/released entity (briefly ignored by Hold-mode retry)
.float vr_flatgrab_ignore_until; // time until vr_flatgrab_ignore expires

vector vr_flatgrab_hitpt;        // world point on/near target along the crosshair ray (VR_FlatGrab_FindTarget)
float vr_flatgrab_count;         // (tests) successful H grabs
float vr_flatgrab_releases;      // (tests) H releases

void() VR_FlatGrab_SyncStat;

float(float xHand) VR_FlatGrab_HandBit =
{
    return xHand == cVR_OffHand ? 1 : 2;
};

// Whether `self` currently holds a 2H prop, a bulky 1H prop (crate/barrel) or a ragdoll via Grab / Pull (tells client
// to raise and hold the hand(s) out comfortably in front, clear of the torso and floor).
float() VR_FlatGrab_UsesHoldPose =
{
    if(!self.vr_flatgrab_hands)
    {
        return FALSE;
    }
    entity eOff = (self.vr_flatgrab_hands & 1) ? VR_Carry_Held(self, cVR_OffHand) : world;
    entity eMain = (self.vr_flatgrab_hands & 2) ? VR_Carry_Held(self, cVR_MainHand) : world;
    if(eOff != world && (isragdoll(eOff) || eOff.carry_2h || vlen(eOff.maxs - eOff.mins) > 18))
    {
        return TRUE;
    }
    if(eMain != world && (isragdoll(eMain) || eMain.carry_2h || vlen(eMain.maxs - eMain.mins) > 18))
    {
        return TRUE;
    }
    return FALSE;
};

// Packed into STAT_QVR_FLATGREN (.vr_flatgren): bit 256 off hand H-grabbed, 512 main hand H-grabbed, 1024 hold-out pose.
float() VR_FlatGrab_Bits =
{
    float bits = (self.vr_flatgrab_hands & 3) * 256;
    if(VR_FlatGrab_UsesHoldPose())
    {
        bits = bits + 1024;
    }
    return bits;
};

// Syncs STAT_QVR_FLATGREN with the current grenade, throw and grab bits.
void() VR_FlatGrab_SyncStat =
{
    self.vr_flatgren = (self.vr_flatgren_phase ? self.vr_flatgren_phase + (self.vr_flatgren_hand == cVR_MainHand ? 8 : 0) : 0) +
                       VR_FlatThrow_Bits() + VR_FlatGrab_Bits();
};

// Notify Grab / Pull that `e` was just thrown or dropped by B so Hold mode does not immediately re-grab it.
void(entity e) VR_FlatGrab_NoteThrown =
{
    self.vr_flatgrab_ignore = e;
    self.vr_flatgrab_ignore_until = time + 0.45;
    if(self.vr_flatgrab_holdmode)
    {
        self.vr_flatgrab_hold_done = TRUE;
    }
};

// Whether hand `xHand` of `self` is empty and available to grab with H (a hand resting on a weapon's 2H grip is free).
float(float xHand) VR_FlatGrab_CanHandGrab =
{
    if(VR_FlatReload_Busy(self, xHand))
    {
        return FALSE;
    }
    if(self.vr_flatgren_phase && self.vr_flatgren_hand == xHand)
    {
        return FALSE;
    }
    if(self.vr_flatthrow_phase == VR_FLATTHROW_WIND && self.vr_flatthrow_hand == xHand)
    {
        return FALSE;
    }
    return VR_Hand_Free(self, xHand, VR_HANDFREE_GUNGRIP);
};

// Whether entity `e` can be physically grabbed with H (ragdolls, crates, props, gibs, heads, pickups, torches, weapons).
float(entity e) VR_FlatGrab_IsEligible =
{
    if(e == world || e == self || e.model == string_null || e.fg_state != 0)
    {
        return FALSE;
    }
    if(e == self.vr_flatgrab_ignore && time < self.vr_flatgrab_ignore_until)
    {
        return FALSE;
    }
    if(VR_AxeStick_Tugging(e) || VR_Embed_Held(e) || e.classname == "vr_ag_weapon" || VR_Run_RefusesEnt(e))
    {
        return FALSE;
    }
    if(e.carry_player != world)
    {
        return FALSE;
    }
    // Active (e.g. shoved/staggered/knocked-down) or dead ragdolls.
    if(isragdoll(e))
    {
        if(cvar_hget(cvarh_vr_ragdoll_grab) < 1)
        {
            return FALSE;
        }
        // Skip if already held by both hands of this player.
        if(VR_Carry_Held(self, cVR_MainHand) == e && VR_Carry_Held(self, cVR_OffHand) == e)
        {
            return FALSE;
        }
        return TRUE;
    }
    // Wall torches (on the wall or lying on the ground).
    if(VR_WallTorch_IsTorch(e))
    {
        return TRUE;
    }
    // Carryable props, crates, explosive boxes, barrels, gibs, heads, debris, rocks, grenades, pickups.
    if((e.handtouch == VR_Carry_Handtouch || e.handtouch == VR_PickupObj_Handtouch) && e.solid != SOLID_NOT && VR_Carry_On(e))
    {
        return TRUE;
    }
    // Loose / dropped weapons (or level weapons in single player).
    if((e.classname == "thrown_weapon" || (!deathmatch && !coop && e.handtouch == weapon_touch)) && e.solid != SOLID_NOT)
    {
        return TRUE;
    }
    return FALSE;
};

// Closest point on entity `e`'s world AABB to the crosshair ray (`eye` + `aim * t`), falling back to model centre.
vector(entity e, vector eye, vector aim, float reach) VR_FlatGrab_TargetPoint =
{
    vector centre = VR_Forcegrab_Centre(e);
    float along = clamp((centre - eye) * aim, 0, reach);
    vector rayPt = eye + aim * along;
    if(e.absmax_x > e.absmin_x || e.absmax_y > e.absmin_y || e.absmax_z > e.absmin_z)
    {
        vector pt;
        pt_x = clamp(rayPt_x, e.absmin_x, e.absmax_x);
        pt_y = clamp(rayPt_y, e.absmin_y, e.absmax_y);
        pt_z = clamp(rayPt_z, e.absmin_z, e.absmax_z);
        return pt;
    }
    return centre;
};

// Best grabbable entity in front of `self` within `vr_flat_grab_reach` and `vr_flat_grab_cone`.
entity(vector eye, vector aim) VR_FlatGrab_FindTarget =
{
    float reach = clamp(cvar_hget(cvarh_vr_flat_grab_reach), 16, 300);
    float cone = clamp(cvar_hget(cvarh_vr_flat_grab_cone), 1, 80);
    float minCos = cos(cone * 0.0174533);
    float closeCos = cos(min(80, cone * 1.6) * 0.0174533);

    entity best = world;
    float bestScore = -10;
    vector bestPt = '0 0 0';

    for(entity e = findradius(eye, reach + 48); e; e = e.chain)
    {
        if(!VR_FlatGrab_IsEligible(e))
        {
            continue;
        }
        vector pt = VR_FlatGrab_TargetPoint(e, eye, aim, reach);
        vector d = pt - eye;
        float dist = vlen(d);
        if(dist > reach || dist < 0.01)
        {
            continue;
        }
        float dot = (d * aim) / dist;
        float needCos = dist < 40 ? closeCos : minCos;
        if(dot < needCos)
        {
            continue;
        }
        // Line of sight (don't grab through solid walls).
        traceline(eye, pt, TRUE, self);
        if(trace_fraction < 0.95 && trace_ent != e)
        {
            vector mid = VR_Forcegrab_Centre(e);
            traceline(eye, mid, TRUE, self);
            if(trace_fraction < 0.95 && trace_ent != e)
            {
                continue;
            }
            pt = mid;
        }
        float score = dot - (dist / (reach * 4));
        if(score > bestScore)
        {
            bestScore = score;
            best = e;
            bestPt = pt;
        }
    }
    vr_flatgrab_hitpt = bestPt;
    return best;
};

// Clear world origin for placing carryable `e` ahead of `from` along `aim` at `dist` without pushing into a wall.
vector(entity e, vector eye, vector from, vector aim, float dist) VR_FlatGrab_PlaceClear =
{
    vector target = from + aim * dist;
    if(e.maxs != e.mins)
    {
        tracebox(eye, e.mins, e.maxs, target, TRUE, self);
        if(!trace_startsolid && !trace_allsolid)
        {
            return trace_endpos;
        }
    }
    traceline(eye, target, TRUE, self);
    return trace_endpos;
};

// Take a ragdoll (active or dead) with every available empty hand (1 or 2 hands).
float(entity e, float canMain, float canOff, float reach) VR_FlatGrab_TakeRagdoll =
{
    float h1 = canMain ? cVR_MainHand : cVR_OffHand;
    // If one hand already holds this ragdoll, grab with the other empty hand.
    if(VR_Carry_Held(self, h1) == e)
    {
        h1 = VRGetOtherHand(h1);
        if(!VR_FlatGrab_CanHandGrab(h1))
        {
            return FALSE;
        }
        canMain = (h1 == cVR_MainHand);
        canOff = (h1 == cVR_OffHand);
    }
    float part1 = ragdollgrabreach(e, self, h1, vr_flatgrab_hitpt, reach + 24, -1);
    if(part1 <= 0)
    {
        return FALSE;
    }
    VR_Hand_Take(self, h1, e);
    VR_Haptic_RagdollTouch(h1);
    self.vr_flatgrab_hands = self.vr_flatgrab_hands | VR_FlatGrab_HandBit(h1);

    float takenBoth = FALSE;
    if(canMain && canOff)
    {
        float h2 = VRGetOtherHand(h1);
        float part2 = ragdollgrabreach(e, self, h2, vr_flatgrab_hitpt, reach + 24, part1 - 1);
        if(part2 > 0)
        {
            VR_Hand_Take(self, h2, e);
            VR_Haptic_RagdollTouch(h2);
            self.vr_flatgrab_hands = self.vr_flatgrab_hands | VR_FlatGrab_HandBit(h2);
            takenBoth = TRUE;
        }
    }
    vr_flatgrab_count++;
    dprint(sprintf("flat grab: took ragdoll %s (%s)%s\n", e.classname, e.health > 0 ? "active" : "dead",
        takenBoth ? " with both hands" : sprintf(" with %s hand", VR_FlatGrenade_HandName(h1))));
    return TRUE;
};

// Take a carryable prop / crate / barrel / gib / head / pickup with available empty hand(s).
float(entity e, vector eye, vector aim, float canMain, float canOff) VR_FlatGrab_TakeCarry =
{
    float twoH = (canMain && canOff && propvalue(e, "two_hands") != 0);
    float halfExt = clamp(vlen(e.maxs - e.mins) * 0.35, 4, 28);
    if(twoH)
    {
        vector handMid = (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5;
        float wantDist = max(clamp(cvar_hget(cvarh_vr_flat_grab_carry_dist), 8, 96), halfExt + 14);
        float curDist = vlen(e.origin - handMid);
        float useDist = clamp(curDist, halfExt + 14, wantDist);
        vector placeAt = VR_FlatGrab_PlaceClear(e, eye, handMid, aim, useDist);
        placeAt = carrybody(e, self, placeAt);
        setorigin(e, placeAt);
        VR_Carry_Start(self, cVR_MainHand, e);
        if(VR_Carry_Held(self, cVR_MainHand) != e)
        {
            return FALSE;
        }
        VR_Hand_Take(self, cVR_OffHand, e);
        e.carry_2h = TRUE;
        carry2h(e, self, TRUE);
        VR_Haptic_GrabProp(cVR_OffHand);
        self.vr_flatgrab_hands = self.vr_flatgrab_hands | 3;
        vr_flatgrab_count++;
        dprint(sprintf("flat grab: took %s with both hands\n", e.classname));
        return TRUE;
    }

    float h = canMain ? cVR_MainHand : cVR_OffHand;
    vector handPos = VRGetHandPos(h);
    vector placeAt = handPos;
    if(halfExt > 8)
    {
        // Bulky 1H prop (crate/barrel while other hand holds a weapon): keep clear of torso and floor, and seat cleanly
        // ahead of the raised hold-pose hand so VR_Carry_Follow doesn't snag on the hip or floor while lifting.
        float dist1h = max(24, halfExt + 12);
        placeAt = VR_FlatGrab_PlaceClear(e, eye, eye - '0 0 10', aim, dist1h);
        placeAt_z = max(placeAt_z, self.absmin_z - e.mins_z + 6);
        placeAt = carrybody(e, self, placeAt);
    }
    setorigin(e, placeAt);
    VR_Carry_Start(self, h, e);
    if(VR_Carry_Held(self, h) == e)
    {
        if(halfExt > 8)
        {
            e.carry_offset_x = max(e.carry_offset_x, halfExt + 2);
            e.carry_offset_y = clamp(e.carry_offset_y, -8, 8);
            e.carry_offset_z = clamp(e.carry_offset_z, -6, 6);
        }
        self.vr_flatgrab_hands = self.vr_flatgrab_hands | VR_FlatGrab_HandBit(h);
        vr_flatgrab_count++;
        dprint(sprintf("flat grab: took %s with %s hand\n", e.classname, VR_FlatGrenade_HandName(h)));
        return TRUE;
    }
    return FALSE;
};

// Take a wall torch or loose weapon via its existing VR handtouch handover.
float(entity e, float canMain, float canOff) VR_FlatGrab_TakeHandTouch =
{
    float h = canMain ? cVR_MainHand : cVR_OffHand;
    vector oldOrg = e.origin;
    vector oldAng = e.angles;
    vector handPos = VRGetHandPos(h);
    if(!VR_WallTorch_IsTorch(e))
    {
        vector grabPoint = VR_Forcegrab_GrabPoint(e);
        setorigin(e, handPos - (grabPoint - e.origin));
    }
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = h;
    e.handtouch_hand = h;
    e.handtouch_ent = self;
    self = e;
    other = oldSelf;
    forcegrabCatching = TRUE;
    self.handtouch();
    forcegrabCatching = FALSE;
    self = oldSelf;
    other = oldOther;

    if(VR_Carry_Held(self, h) != world)
    {
        self.vr_flatgrab_hands = self.vr_flatgrab_hands | VR_FlatGrab_HandBit(h);
        vr_flatgrab_count++;
        dprint(sprintf("flat grab: took %s with %s hand\n", e.classname, VR_FlatGrenade_HandName(h)));
        return TRUE;
    }
    if(!VRIsHandWeaponless(self, h))
    {
        catchblend(h, e, oldOrg, oldAng);
        vr_flatgrab_count++;
        dprint(sprintf("flat grab: equipped weapon %s with %s hand\n", e.classname, VR_FlatGrenade_HandName(h)));
        return TRUE;
    }
    return FALSE;
};

// Attempt to grab what is in front of `self` using every available empty hand.
float() VR_FlatGrab_TryGrab =
{
    if(!VR_FlatPlay(self) || self.health <= 0 || !self.ishuman)
    {
        return FALSE;
    }
    float canMain = VR_FlatGrab_CanHandGrab(cVR_MainHand);
    float canOff = VR_FlatGrab_CanHandGrab(cVR_OffHand);
    if(!canMain && !canOff)
    {
        dprint("flat grab: no empty hand available\n");
        return FALSE;
    }
    vector eye = VRGetHeadPos(self);
    makevectors(self.v_angle);
    vector aim = v_forward;
    float reach = clamp(cvar_hget(cvarh_vr_flat_grab_reach), 16, 300);

    entity e = VR_FlatGrab_FindTarget(eye, aim);
    if(e == world)
    {
        dprint("flat grab: nothing grabbable in reach\n");
        return FALSE;
    }
    float ok = FALSE;
    if(isragdoll(e))
    {
        ok = VR_FlatGrab_TakeRagdoll(e, canMain, canOff, reach);
    }
    else if(VR_WallTorch_IsTorch(e) || e.classname == "thrown_weapon" || e.handtouch == weapon_touch)
    {
        ok = VR_FlatGrab_TakeHandTouch(e, canMain, canOff);
    }
    else
    {
        ok = VR_FlatGrab_TakeCarry(e, eye, aim, canMain, canOff);
    }
    if(ok)
    {
        VR_FlatGrab_SyncStat();
    }
    return ok;
};

// Release a ragdoll held by `self` (1 or both hands) cleanly with `vel` (u/s; '0 0 0' for a clean drop).
void(entity xRag, vector xVel) VR_FlatGrab_ReleaseRagdoll =
{
    if(xRag == world)
    {
        return;
    }
    float hadOff = (VR_Carry_Held(self, cVR_OffHand) == xRag);
    float hadMain = (VR_Carry_Held(self, cVR_MainHand) == xRag);
    ragdollthrow(xRag, self, xVel);
    if(hadOff)
    {
        VR_Hand_Release(self, cVR_OffHand, "letgo");
        VR_Ragdoll_LetGo(cVR_OffHand, xRag);
        self.vr_flatgrab_hands = self.vr_flatgrab_hands - (self.vr_flatgrab_hands & 1);
    }
    if(hadMain)
    {
        VR_Hand_Release(self, cVR_MainHand, "letgo");
        VR_Ragdoll_LetGo(cVR_MainHand, xRag);
        self.vr_flatgrab_hands = self.vr_flatgrab_hands - (self.vr_flatgrab_hands & 2);
    }
};

// Release all hands currently holding an H-grabbed prop or ragdoll cleanly.
void() VR_FlatGrab_ReleaseAll =
{
    if(!self.vr_flatgrab_hands)
    {
        return;
    }
    float released = FALSE;
    float h = cVR_OffHand;
    while(h <= cVR_MainHand)
    {
        float bit = VR_FlatGrab_HandBit(h);
        if(self.vr_flatgrab_hands & bit)
        {
            entity e = VR_Carry_Held(self, h);
            if(e != world)
            {
                if(isragdoll(e))
                {
                    VR_FlatGrab_ReleaseRagdoll(e, '0 0 0');
                    dprint(sprintf("flat grab: released ragdoll %s\n", e.classname));
                    released = TRUE;
                }
                else if(VR_Forcegrab_FlatDropHand(h))
                {
                    dprint(sprintf("flat grab: released %s\n", e.classname));
                    released = TRUE;
                }
            }
            self.vr_flatgrab_hands = self.vr_flatgrab_hands - (self.vr_flatgrab_hands & bit);
        }
        h = h + 1;
    }
    self.vr_flatgrab_hands = 0;
    if(released)
    {
        vr_flatgrab_releases++;
    }
    VR_FlatGrab_SyncStat();
};

// Impulse 52: Toggle mode press (default vr_flat_grab_hold 0).
void() VR_FlatGrab_Toggle =
{
    if(!VR_FlatPlay(self) || self.health <= 0 || !self.ishuman)
    {
        return;
    }
    self.vr_flatgrab_holdmode = FALSE;
    self.vr_flatgrab_hold_done = FALSE;
    if(self.vr_flatgrab_hands)
    {
        VR_FlatGrab_ReleaseAll();
        return;
    }
    VR_FlatGrab_TryGrab();
};

// Impulse 37: Hold mode down / retry while held (vr_flat_grab_hold 1).
void() VR_FlatGrab_HoldStart =
{
    if(!VR_FlatPlay(self) || self.health <= 0 || !self.ishuman)
    {
        return;
    }
    self.vr_flatgrab_holdmode = TRUE;
    if(self.vr_flatgrab_hands || self.vr_flatgrab_hold_done)
    {
        return;
    }
    VR_FlatGrab_TryGrab();
};

// Impulse 38: Hold mode release (vr_flat_grab_hold 1).
void() VR_FlatGrab_HoldEnd =
{
    self.vr_flatgrab_holdmode = FALSE;
    self.vr_flatgrab_hold_done = FALSE;
    if(self.vr_flatgrab_hands)
    {
        VR_FlatGrab_ReleaseAll();
    }
};

// Each server player frame: clear stale grab bits when a hand empties (thrown, auto-collected, lost, died) and sync stat.
void() VR_FlatGrab_Frame =
{
    if(!self.vr_flatgrab_hands)
    {
        return;
    }
    if(!VR_FlatPlay(self) || self.health <= 0)
    {
        self.vr_flatgrab_hands = 0;
        self.vr_flatgrab_holdmode = FALSE;
        self.vr_flatgrab_hold_done = FALSE;
        VR_FlatGrab_SyncStat();
        return;
    }
    float before = self.vr_flatgrab_hands;
    if((self.vr_flatgrab_hands & 1) && VR_Carry_Held(self, cVR_OffHand) == world)
    {
        self.vr_flatgrab_hands = self.vr_flatgrab_hands - 1;
    }
    if((self.vr_flatgrab_hands & 2) && VR_Carry_Held(self, cVR_MainHand) == world)
    {
        self.vr_flatgrab_hands = self.vr_flatgrab_hands - 2;
    }
    if(self.vr_flatgrab_hands != before)
    {
        VR_FlatGrab_SyncStat();
    }
};
