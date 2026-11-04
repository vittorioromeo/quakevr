// vr_skewer.qc -- Skewering gibs (vr_embed; ROUND23.md, "Skewering gibs"; Combat > Embedded Blades).
//
// A stab (VR_Embed_StabWhy, tip first) by a pointed blade in the hand on a loose gib, severed head or limb: it takes no blow and
// goes on the blade, centred on its line, vr_embed_skewer_depth back from the tip and vr_embed_skewer_gap apart, up to
// vr_embed_skewer_most on a blade. Any other blow bursts it as ever (VR_Melee_Deal, VR_Gib_Blow). On the blade it is not solid, so
// blows, touches and force grabs pass it; a shot's or a blast's damage still reaches it (it isn't in the shots' traces).
//
// The gib drives itself (its think each frame), so nothing about it can dangle: burst, it is simply gone. Its own think waits in
// .vr_skewer_think and comes back as it comes off (a fade starts over: VR_Gib_FadeAnew). It comes off (VR_Skewer_Off, falling) when the
// hand no longer holds that blade (thrown, dropped, holstered, switched), its holder is dead or gone, vr_embed is off, the level
// ends; the other hand's grip takes it (VR_Skewer_Take). A blade left in a body keeps it (VR_Skewer_BladeStuck) until it is out
// of the body; back in the hand (VR_Skewer_BladeBack) it rides there again.

.float  vr_skewer_hand;    // the hand holding the blade
.float  vr_skewer_slot;    // its place along the blade, 0 nearest the tip
.float  vr_skewer_wid;     // the blade's weapon
.entity vr_skewer_w;       // the blade, stuck in a body (world: in the hand)
.vector vr_skewer_fw;      // its forward in the frame it rides (VR_Skewer_Basis)
.vector vr_skewer_up;      // its up
.vector vr_skewer_at;      // and, on a stuck blade, its place
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

// Its turn and place as they are now, in the frame it rides from here.
void(entity g) VR_Skewer_Bind =
{
    vector a = g.angles;
    a_x = -a_x;
    makevectors(a);
    vector f = v_forward;
    vector u = v_up;
    VR_Skewer_Basis(g);
    vector d = g.origin - vr_skewer_o;
    g.vr_skewer_fw = VR_Skewer_ToBasis(f);
    g.vr_skewer_up = VR_Skewer_ToBasis(u);
    g.vr_skewer_at = VR_Skewer_ToBasis(d);
};

// Placed: on a stuck blade where it was on it; in the hand on the blade's line, its middle there. FALSE: no line to ride.
float(entity g) VR_Skewer_Place =
{
    vector at = '0 0 0';
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_hand;
    if(g.vr_skewer_w == world)
    {
        if(!VR_Melee_Line(p, h, VR_Melee_Thing(p, h)))
        {
            return FALSE;
        }
        vector line = vr_mline_far - vr_mline_grip;
        float back = min(cvar_hget(cvarh_vr_embed_skewer_depth) + g.vr_skewer_slot * cvar_hget(cvarh_vr_embed_skewer_gap), vlen(line) * 0.85);
        at = vr_mline_far - normalize(line) * back;
    }
    VR_Skewer_Basis(g);
    if(g.vr_skewer_w != world)
    {
        at = vr_skewer_o + VR_Skewer_FromBasis(g.vr_skewer_at);
    }
    vector f = VR_Skewer_FromBasis(g.vr_skewer_fw);
    vector u = VR_Skewer_FromBasis(g.vr_skewer_up);
    vector ang = VR_AxeStick_Angles(f, u);
    makevectors(ang);
    if(g.vr_skewer_w == world)
    {
        vector mid = (g.mins + g.maxs) * 0.5;
        at = at - (v_forward * mid_x - v_right * mid_y + v_up * mid_z);
    }
    g.angles = ang;
    g.angles_x = -ang_x;
    setorigin(g, at);
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
    g.vr_skewer_p = world;
    g.vr_skewer_w = world;
};

// It falls off.
void(entity g) VR_Skewer_Off =
{
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_hand;
    float inHand = g.vr_skewer_w == world && p.classname == "player";
    vector v = inHand ? VRGetEntHandVel(p, h) * VRMetersToUnits() : '0 0 0';
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

// Whether it stays on.
float(entity g) VR_Skewer_Stays =
{
    entity p = g.vr_skewer_p;
    if(p.classname != "player" || p.health <= 0 || !cvar_hget(cvarh_vr_embed))
    {
        return FALSE;
    }
    if(g.vr_skewer_w != world)
    {
        return g.vr_skewer_w.classname == "thrown_weapon" && VR_AxeStick_InMonster(g.vr_skewer_w);
    }
    return VRGetEntWeapon(p, g.vr_skewer_hand) == g.vr_skewer_wid && VR_Skewer_HasBlade(p, g.vr_skewer_hand);
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

// Each frame, as the gib.
void() VR_Skewer_Think =
{
    entity g = self;
    g.nextthink = time;
    VR_Carry_KeepAlive(g); // (a fade on its other think)
    if(!VR_Skewer_Stays(g))
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
    g.vr_skewer_wid = VRGetEntWeapon(self, xHand);
    g.vr_skewer_w = world;
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
    VR_Skewer_Bind(g);
    VR_Skewer_Place(g);
    VRSetMeleeHitSoundPlayed(xHand, TRUE);
    VRMeleeHapticByHand(xHand);
    VR_Gib_Squish(g, 0.8);
    return TRUE;
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

// The blade in `xHand` of `p` stays in a body as `w` (VR_Embed_BlowsDone): what is on it rides that.
void(entity p, float xHand, entity w) VR_Skewer_BladeStuck =
{
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == p && e.vr_skewer_w == world && e.vr_skewer_hand == xHand)
        {
            e.vr_skewer_w = w;
            VR_Skewer_Bind(e);
        }
    }
};

// The stuck blade `w` comes back into `xHand` of `p` (VR_Embed_Free): what is on it rides the hand again.
void(entity p, float xHand, entity w) VR_Skewer_BladeBack =
{
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p != world && e.vr_skewer_w == w)
        {
            e.vr_skewer_p = p;
            e.vr_skewer_w = world;
            e.vr_skewer_hand = xHand;
            VR_Skewer_Bind(e);
        }
    }
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
