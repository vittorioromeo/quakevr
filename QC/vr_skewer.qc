// vr_skewer.qc -- Skewering gibs (vr_embed; ROUND23.md, "Skewering gibs"; Combat > Embedded Blades).
//
// A stab (VR_Embed_StabWhy, tip first) by a pointed blade in the hand on a loose gib, severed head or limb: it takes no blow and
// goes on the blade, centred on its line, vr_embed_skewer_depth back from the tip and vr_embed_skewer_gap apart, up to
// vr_embed_skewer_most on a blade. Any other blow bursts it as ever (VR_Melee_Deal, VR_Gib_Blow). On the blade it is not solid, so
// blows, touches and force grabs pass it; a shot's or a blast's damage still reaches it (it isn't in the shots' traces).
//
// The gib drives itself (its think each frame), so nothing about it can dangle: burst, it is simply gone. Its own think waits in
// .vr_skewer_think and comes back as it comes off (a fade starts over: VR_Gib_FadeAnew).
//
// It stays with its blade, the weapon's record (.vr_skewer_inst), wherever that goes (VR_Skewer_Find): the hand; dropped, thrown or
// left in a body, the thrown_weapon of that record (.vr_skewer_w), whose rigid body has a sphere for it (the engine's
// addSkeweredShapes, remade when .vr_skewer_gen changes); picked up, whichever hand has it. No joint: it is one more shape of one body,
// so its mass and centre of mass come with it. It comes off (VR_Skewer_Off, falling) when the blade is nowhere (holstered, switched,
// gone), its holder is dead, vr_embed is off, the level ends; a free hand's grip takes it (VR_Skewer_Take).
//
// Bursts: on a blade flying fast enough, a gib that itself meets what is ahead of it (VR_Skewer_Impact, vr_embed_skewer_throw_burst); on a
// blade in the hand, a swing fast enough whose contact falls at that gib's place along it (VR_Skewer_SwingHit, vr_embed_skewer_swing_burst).
//
// In the hand it weighs: each frame's gibs add up in the player's .vr_skewer_load_* (kg, kg m, kg m^2 about the grip: VR_Skewer_Frame),
// for the hand's weight (vr_weight.cpp computeLoad). Its mass: its own .vr_prop_mass (a limb's), else vr_embed_skewer_mass_head or _gib.

.float  vr_skewer_hand;    // the hand holding the blade
.float  vr_skewer_slot;    // its place along the blade, 0 nearest the tip
.entity vr_skewer_inst;    // the blade's record (its WeaponInst)
.vector vr_skewer_fw;      // its forward in the frame it rides (VR_Skewer_Basis: the hand's, or the thrown blade's: the same numbers)
.vector vr_skewer_up;      // its up
.vector vr_skewer_axis;    // the blade's line in that frame
.vector vr_skewer_c;       // its middle now
.vector vr_skewer_lastv;   // its move last frame (units)
.float  vr_skewer_lastsp;  // and its speed (m/s)
.float  vr_skewer_lastt;   // when it was placed last
.void() vr_skewer_think;   // its own think, waiting
.float  vr_skewer_next;    // s left of it when it went on (0: it had none)

vector vr_skewer_o; // VR_Skewer_Basis: the frame's origin

// `d` in the basis of v_forward, v_right, v_up...
vector(vector d) VR_Skewer_ToBasis =
{
    vector r;
    r_x = d * v_forward;
    r_y = d * v_right;
    r_z = d * v_up;
    return r;
};

// ... and back.
vector(vector d) VR_Skewer_FromBasis =
{
    return v_forward * d_x + v_right * d_y + v_up * d_z;
};

// A place along a blade as a bit of a set.
float(float xSlot) VR_Skewer_Bit =
{
    float bit = 1;
    for(float i = 0; i < xSlot; ++i)
    {
        bit = bit * 2;
    }
    return bit;
};

// Whether `xHand` of `p` holds a pointed blade now.
float(entity p, float xHand) VR_Skewer_HasBlade =
{
    float flags = VRGetEntWeaponFlags(p, xHand);
    return !VRIsHandWeaponless(p, xHand) && !(flags & QVR_WPNFLAG_FOREGRIP_CARRIED) &&
           weaponvalue(WeaponIdToModel(VRGetEntWeapon(p, xHand), flags), "embed") == AXESTICK_POINT;
};

// Its weight, kg.
float(entity g) VR_Skewer_Mass =
{
    if(g.vr_prop_mass > 0)
    {
        return g.vr_prop_mass;
    }
    return max(0, cvar_hget(g.vr_gib_healthmult >= 1.5 ? cvarh_vr_embed_skewer_mass_head : cvarh_vr_embed_skewer_mass_gib));
};

// How wide it is, units (as the engine's sphere for it).
float(entity g) VR_Skewer_Radius =
{
    vector size = g.maxs - g.mins;
    return max(2, (size_x + size_y + size_z) / 6);
};

// How far from the tip its middle sits on a blade whose line is `xLen` long.
float(entity g, float xLen) VR_Skewer_Back =
{
    return min(cvar_hget(cvarh_vr_embed_skewer_depth) + g.vr_skewer_slot * cvar_hget(cvarh_vr_embed_skewer_gap), xLen * 0.85);
};

// The frame `g` rides: v_forward, v_right, v_up and vr_skewer_o.
void(entity g) VR_Skewer_Basis =
{
    if(g.vr_skewer_w != world)
    {
        vector a = g.vr_skewer_w.angles;
        a_x = -a_x;
        makevectors(a);
        vr_skewer_o = g.vr_skewer_w.origin;
    }
    else
    {
        makevectors(VRGetEntHandRot(g.vr_skewer_p, g.vr_skewer_hand));
        vr_skewer_o = VRGetEntHandPos(g.vr_skewer_p, g.vr_skewer_hand);
    }
};

// Placed: its middle on the blade's line in the hand, at its place in the frame of a thrown blade. FALSE: no line to ride.
float(entity g) VR_Skewer_Place =
{
    vector c = '0 0 0';
    vector line = '0 0 0';
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_hand;
    if(g.vr_skewer_w == world)
    {
        if(!VR_Melee_Line(p, h, VR_Melee_Thing(p, h)))
        {
            return FALSE;
        }
        line = vr_mline_far - vr_mline_grip;
        c = vr_mline_far - normalize(line) * VR_Skewer_Back(g, vlen(line));
    }
    VR_Skewer_Basis(g);
    if(g.vr_skewer_w == world)
    {
        g.vr_skewer_axis = VR_Skewer_ToBasis(normalize(line));
    }
    else
    {
        c = vr_skewer_o + VR_Skewer_FromBasis(g.vr_skewer_at);
    }
    vector f = VR_Skewer_FromBasis(g.vr_skewer_fw);
    vector u = VR_Skewer_FromBasis(g.vr_skewer_up);
    vector ang = VR_AxeStick_Angles(f, u);
    makevectors(ang);
    vector mid = (g.mins + g.maxs) * 0.5;
    g.angles = ang;
    g.angles_x = -ang_x;
    g.vr_skewer_c = c;
    setorigin(g, c - (v_forward * mid_x - v_right * mid_y + v_up * mid_z));
    return TRUE;
};

// No longer on a blade: its own think back, a force grab may have it again.
void(entity g) VR_Skewer_Clear =
{
    g.think = g.vr_skewer_think;
    g.nextthink = g.vr_skewer_next > 0 ? time + g.vr_skewer_next : 0;
    if(g.think == SUB_Remove)
    {
        VR_Gib_FadeAnew(g);
    }
    g.flags = g.flags | FL_FORCEGRABBABLE;
    VR_Skewer_Gone(g);
};

// It falls off.
void(entity g) VR_Skewer_Off =
{
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_hand;
    float inHand = g.vr_skewer_w == world && p.classname == "player";
    vector v = g.vr_skewer_w != world ? g.vr_skewer_w.velocity : (inHand ? VRGetEntHandVel(p, h) * VRMetersToUnits() : '0 0 0');
    if(inHand)
    {
        // (The blade's end may be past a wall: not left in it.)
        traceline(VRGetEntHandPos(p, h), g.origin, TRUE, p);
        if(trace_fraction < 1)
        {
            setorigin(g, trace_endpos + trace_plane_normal * 2);
        }
        g.vr_gib_releasetime = time; // (no blow of that hand's follow-through)
        g.vr_gib_releasehand = h;
        g.carry_letgo = p;
        g.carry_letgo_time = time;
    }
    VR_Skewer_Clear(g);
    VR_Carry_Loose(g);
    g.velocity = v;
    dprint(sprintf("skewer: %s falls off\n", g.classname));
};

// `xHand` of `p` takes it off the blade, held as a gib is.
void(entity g, entity p, float xHand) VR_Skewer_Take =
{
    VR_Skewer_Clear(g);
    VR_Carry_Start(p, xHand, g);
    dprint(sprintf("skewer: %s taken by hand %g\n", g.classname, xHand));
};

// Whether its blade is still somewhere: where it was, or (its record's) gone from the hand into a thrown_weapon or from one into a hand.
float(entity g) VR_Skewer_Find =
{
    if(!cvar_hget(cvarh_vr_embed))
    {
        return FALSE;
    }
    entity p = g.vr_skewer_p;
    entity inst = g.vr_skewer_inst;
    entity w = g.vr_skewer_w;
    entity e;
    if(w != world)
    {
        if(w.modelindex && w.classname == "thrown_weapon" && w.weaponinst == inst) // (a removed one keeps its classname)
        {
            return TRUE;
        }
    }
    else if(p.classname == "player" && p.health > 0 && VRGetEntWeaponInst(p, g.vr_skewer_hand) == inst && VR_Skewer_HasBlade(p, g.vr_skewer_hand))
    {
        return TRUE;
    }
    for(e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.weaponinst == inst)
        {
            // Let go of: on it as on the hand (it starts in the hand's pose), its place from the blade's own tip.
            g.vr_skewer_w = e;
            e.vr_skewer_gen = e.vr_skewer_gen + 1;
            VR_Skewer_Basis(g);
            vector axis = VR_Skewer_FromBasis(g.vr_skewer_axis);
            vector tip = weaponmuzzle(e);
            float len = vlen(tip - e.origin);
            g.vr_skewer_at = VR_Skewer_ToBasis(tip - axis * VR_Skewer_Back(g, len) - e.origin);
            g.vr_skewer_lastt = 0;
            return TRUE;
        }
    }
    for(e = VR_Player(); e; e = VR_NextPlayer(e))
    {
        float h = VRGetEntWeaponInst(e, cVR_MainHand) == inst ? cVR_MainHand : (VRGetEntWeaponInst(e, cVR_OffHand) == inst ? cVR_OffHand : -1);
        if(h >= 0 && e.health > 0 && VR_Skewer_HasBlade(e, h))
        {
            VR_Skewer_Gone(g); // (off the thrown one)
            g.vr_skewer_p = e;
            g.vr_skewer_hand = h;
            return TRUE;
        }
    }
    return FALSE;
};

// A grip closing on it, within reach: the hand. -1: none. Not the hand holding the blade.
float(entity g, entity p) VR_Skewer_Gripper =
{
    float hand = -1;
    entity was = self;
    self = p; // (carryreach: self's hand)
    if(g.vr_skewer_w != world || g.vr_skewer_hand != cVR_MainHand)
    {
        if(VR_Carry_GripPressed(p, cVR_MainHand) && VR_Hand_Free(p, cVR_MainHand, 0) && !VR_Carry_IsHolding(p, cVR_MainHand) && carryreach(g, VRGetHandPos(cVR_MainHand), cVR_MainHand))
        {
            hand = cVR_MainHand;
        }
    }
    if(hand < 0 && (g.vr_skewer_w != world || g.vr_skewer_hand != cVR_OffHand))
    {
        if(VR_Carry_GripPressed(p, cVR_OffHand) && VR_Hand_Free(p, cVR_OffHand, 0) && !VR_Carry_IsHolding(p, cVR_OffHand) && carryreach(g, VRGetHandPos(cVR_OffHand), cVR_OffHand))
        {
            hand = cVR_OffHand;
        }
    }
    self = was;
    return hand;
};

// On a thrown blade: going fast enough, or just stopped from it, with something right ahead of it: it meets it and bursts (TRUE).
// Measured on its own place, so a gib further along than what the blade hit has nothing ahead of it.
float(entity g) VR_Skewer_Impact =
{
    float dt = time - g.vr_skewer_lastt;
    vector v = g.vr_skewer_c - g.vr_skewer_lastc;
    vector lastv = g.vr_skewer_lastv;
    float lastsp = g.vr_skewer_lastsp;
    float first = ((!g.vr_skewer_lastt) || (dt <= 0));
    vector from = g.vr_skewer_lastc;
    g.vr_skewer_lastt = time;
    g.vr_skewer_lastc = g.vr_skewer_c;
    if(first)
    {
        g.vr_skewer_lastsp = 0;
        g.vr_skewer_lastv = '0 0 0';
        return FALSE;
    }
    float sp = vlen(v) * (1 / dt) * (1 / VRMetersToUnits());
    g.vr_skewer_lastsp = sp;
    g.vr_skewer_lastv = v;
    if(max(sp, lastsp) < cvar_hget(cvarh_vr_embed_skewer_throw_burst))
    {
        return FALSE;
    }
    float reach = VR_Skewer_Radius(g) + 2;
    entity w = g.vr_skewer_w;
    vector dir = v;
    if(sp < lastsp)
    {
        dir = lastv;
    }
    float hit = FALSE;
    float vLen = vlen(v);
    if(vLen > 0.01)
    {
        vector ahead = normalize(v) * reach;
        traceline(from, g.vr_skewer_c + ahead, FALSE, w);
        hit = ((trace_fraction < 1) && (trace_ent != g.vr_skewer_p));
    }
    if((!hit) && (vlen(lastv) > 0.01))
    {
        // (A bounce this frame turned its way: what it was going at.)
        vector ahead2 = normalize(lastv) * reach;
        traceline(g.vr_skewer_c, g.vr_skewer_c + ahead2, FALSE, w);
        hit = ((trace_fraction < 1) && (trace_ent != g.vr_skewer_p));
    }
    if(!hit)
    {
        return FALSE;
    }
    dprint(sprintf("skewer: %s bursts on its blade's impact at %.1f m/s\n", g.classname, max(sp, lastsp)));
    VR_Gib_Burst(g, normalize(dir));
    return TRUE;
};

// In the hand: its weight in the player's tally for the next frame (VR_Skewer_Frame).
void(entity g) VR_Skewer_Tally =
{
    entity p = g.vr_skewer_p;
    float m = VR_Skewer_Mass(g);
    float d = vlen(g.vr_skewer_c - VRGetEntHandPos(p, g.vr_skewer_hand)) * (1 / VRMetersToUnits());
    vector add = '1 0 0' * m + '0 1 0' * (m * d) + '0 0 1' * (m * d * d);
    if(g.vr_skewer_hand == cVR_MainHand)
    {
        p.vr_skewer_sum_main = p.vr_skewer_sum_main + add;
    }
    else
    {
        p.vr_skewer_sum_off = p.vr_skewer_sum_off + add;
    }
};

// Each frame, as the gib.
void() VR_Skewer_Think =
{
    entity g = self;
    g.nextthink = time;
    VR_Carry_KeepAlive(g); // (a fade on its other think)
    if(!VR_Skewer_Find(g))
    {
        VR_Skewer_Off(g);
        return;
    }
    float h = VR_Skewer_Gripper(g, g.vr_skewer_p);
    if(h >= 0)
    {
        VR_Skewer_Take(g, g.vr_skewer_p, h);
        return;
    }
    if(!VR_Skewer_Place(g))
    {
        VR_Skewer_Off(g);
        return;
    }
    if(g.vr_skewer_w != world)
    {
        VR_Skewer_Impact(g);
    }
    else
    {
        VR_Skewer_Tally(g);
    }
};

// A free place along the blade in `xHand` of `p`: -1 none.
float(entity p, float xHand) VR_Skewer_FreeSlot =
{
    float most = cvar_hget(cvarh_vr_embed_skewer_most);
    float taken = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == p && e.vr_skewer_w == world && e.vr_skewer_hand == xHand)
        {
            taken = taken | VR_Skewer_Bit(e.vr_skewer_slot);
        }
    }
    for(float s = 0; s < most; ++s)
    {
        if(!(taken & VR_Skewer_Bit(s)))
        {
            return s;
        }
    }
    return -1;
};

// The blow of `xHand` of self on the loose gib `g` (VR_Melee_Deal). TRUE: a stab that put it on the blade (no other blow).
float(float xHand, entity b, entity g, vector xAt, vector xDir, float xSpeed, float xKind) VR_Skewer_Blow =
{
    if(!cvar_hget(cvarh_vr_embed) || !self.ishuman || g.vr_sgib || g.gorging || g.carry_player != world || g.vr_skewer_p != world ||
       !VR_Skewer_HasBlade(self, xHand))
    {
        return FALSE;
    }
    string why = VR_Embed_StabWhy(b, xAt, xDir, xSpeed, 0, xKind);
    float slot = why == string_null ? VR_Skewer_FreeSlot(self, xHand) : -1;
    if(cvar_hget(cvarh_vr_debug_embed))
    {
        embedsay(why != string_null ? sprintf("%s NOT SKEWERED: %s", g.classname, why) :
                 (slot < 0 ? sprintf("%s NOT SKEWERED: the blade is full", g.classname) : sprintf("%s skewered: %.1f m/s", g.classname, xSpeed)));
    }
    if(slot < 0)
    {
        return FALSE;
    }
    VR_Head_StopFlies(g);
    g.vr_skewer_p = self;
    g.vr_skewer_hand = xHand;
    g.vr_skewer_slot = slot;
    g.vr_skewer_inst = VRGetEntWeaponInst(self, xHand);
    g.vr_skewer_w = world;
    g.vr_skewer_mass = VR_Skewer_Mass(g);
    g.vr_skewer_think = g.think;
    g.vr_skewer_next = g.nextthink > time ? g.nextthink - time : 0;
    g.think = VR_Skewer_Think;
    g.nextthink = time;
    g.movetype = MOVETYPE_NONE;
    g.solid = SOLID_NOT;
    g.velocity = '0 0 0';
    g.avelocity = '0 0 0';
    g.vr_spin = '0 0 0';
    g.vr_gib_flight = 0;
    g.flags = g.flags - (g.flags & (FL_FORCEGRABBABLE | FL_ONGROUND));
    // Its turn as it is now, in the hand's frame.
    vector a = g.angles;
    a_x = -a_x;
    makevectors(a);
    vector f = v_forward;
    vector u = v_up;
    VR_Skewer_Basis(g);
    g.vr_skewer_fw = VR_Skewer_ToBasis(f);
    g.vr_skewer_up = VR_Skewer_ToBasis(u);
    VR_Skewer_Place(g);
    VRSetMeleeHitSoundPlayed(xHand, TRUE);
    VRMeleeHapticByHand(xHand);
    VR_Gib_Squish(g, 0.8);
    return TRUE;
};

// A blow of `xHand` of self being dealt (VR_Melee_Deal), not a stab, not on a gib: fast enough, its contact (mh_c_pos) at a gib's place
// along the blade bursts that one.
void(float xHand, entity b) VR_Skewer_SwingHit =
{
    vector load = xHand == cVR_MainHand ? self.vr_skewer_load_main : self.vr_skewer_load_off;
    if(!load_x || b.mh_c_ent.vr_gib || b.mh_kind == VR_MKIND_STAB || b.mh_c_pos == '0 0 0' || b.mh_speed < cvar_hget(cvarh_vr_embed_skewer_swing_burst))
    {
        return;
    }
    vector line = b.mh_far - b.mh_grip;
    float at = (b.mh_far - b.mh_c_pos) * normalize(line); // (from the tip)
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == self && e.vr_skewer_w == world && e.vr_skewer_hand == xHand &&
           fabs(at - VR_Skewer_Back(e, vlen(line))) <= VR_Skewer_Radius(e))
        {
            dprint(sprintf("skewer: %s bursts: the swing's contact at it, %.1f m/s\n", e.classname, b.mh_speed));
            VR_Gib_Burst(e, b.mh_c_dir);
        }
    }
};

// Whether a touch of `xHand` of `p` is a blade driven along its line (VR_Gib_Struck): the blow's to judge (VR_Skewer_Blow), not the touch's.
float(entity p, float xHand) VR_Skewer_Thrust =
{
    if(!cvar_hget(cvarh_vr_embed) || !cvar_hget(cvarh_vr_embed_skewer_most) || !VR_Skewer_HasBlade(p, xHand) ||
       !VR_Melee_Line(p, xHand, VR_Melee_Thing(p, xHand)))
    {
        return FALSE;
    }
    return normalize(VRGetEntHandVel(p, xHand)) * normalize(vr_mline_far - vr_mline_grip) >= 0.7;
};

// The level ends (VR_Embed_LevelEnd): all off.
void(entity p) VR_Skewer_AllOff =
{
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == p)
        {
            VR_Skewer_Off(e);
        }
    }
};
