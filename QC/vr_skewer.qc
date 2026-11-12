// vr_skewer.qc -- Skewering gibs (vr_embed; ROUND23.md, "Skewering gibs"; Combat > Embedded Blades).
//
// A stab (VR_Embed_StabWhy, tip first) by a pointed blade in the hand on a loose gib, severed head or limb whose model its line truly
// goes through (VR_Skewer_Through: the sweep's wider reach only finds it): it takes no blow and goes on the blade, centred on its line,
// vr_embed_skewer_depth back from the tip and vr_embed_skewer_gap apart, up to vr_embed_skewer_most on a blade. A stab beside it leaves it
// be; any other blow bursts it as ever (VR_Melee_Deal, VR_Gib_Blow). On the blade it is not solid, so blows, touches, force grabs and the
// drawn hands' collision (vr_modelcollide.cpp) pass it; a shot's or a blast's damage still reaches it (it isn't in the shots' traces).
//
// The gib drives itself (its think each frame), so nothing about it can dangle: burst, it is simply gone. Its own think waits in
// .vr_skewer_think and comes back as it comes off (a fade starts over: VR_Gib_FadeAnew).
//
// It stays with its blade, the weapon's record (.vr_skewer_inst), wherever that goes (VR_Skewer_Find): the hand; dropped, thrown or
// left in a body, the thrown_weapon of that record (.vr_skewer_w), whose rigid body has a sphere for it (the engine's
// addSkeweredShapes, remade when .vr_skewer_gen changes); picked up, whichever hand has it. No joint: it is one more shape of one body,
// so its mass and centre of mass come with it. It comes off (VR_Skewer_Off, falling) when the blade is nowhere (holstered, switched,
// gone), its holder is dead, vr_embed is off, the level ends. A free hand's grip holds it (VR_Skewer_Grip) and it goes only along the blade
// with that hand: stopped at the hilt end and by the next gib along, off the tip end when clear of it, or torn off by a tug across the
// line (vr_embed_skewer_tug_speed, _tug_stretch); then it is a prop in that hand (VR_Skewer_Take).
//
// Bursts: a gib that itself, going fast enough, meets what is ahead of it (VR_Skewer_Impact: on a flying blade vr_embed_skewer_throw_burst;
// in the hand vr_embed_skewer_swing_burst, not along the blade's line: the tip gets there first); in the hand also a swing that fast whose
// blow lands by its place along the blade (VR_Skewer_SwingHit, vr_embed_skewer_swing_reach).
//
// In the hand it weighs: each frame's gibs add up in the player's .vr_skewer_load_* (kg, kg m, kg m^2 about the grip: VR_Skewer_Frame),
// for the hand's weight (vr_weight.cpp computeLoad). Its mass: VR_Skewer_Mass, by kind and size.

.float  vr_skewer_hand;    // the hand holding the blade
.float  vr_skewer_slot;    // its place along the blade, 0 nearest the tip
.entity vr_skewer_inst;    // the blade's record (its WeaponInst)
.vector vr_skewer_fw;      // its forward in the frame it rides (VR_Skewer_Basis: the hand's, or the thrown blade's: the same numbers)
.vector vr_skewer_up;      // its up
.vector vr_skewer_axis;    // the blade's line in that frame
.vector vr_skewer_c;       // its middle now
.vector vr_skewer_lastv;   // its move last frame (units)
.float  vr_skewer_lastsp;  // and its speed (m/s; -1 a snap, VR_Skewer_Impact)
.float  vr_skewer_lastt;   // when it was placed last
.void() vr_skewer_think;   // its own think, waiting
.float  vr_skewer_next;    // s left of it when it went on (0: it had none)
.float  vr_skewer_shift;   // units it has been slid towards the hilt from its place
.float  vr_skewer_grip;    // the other hand holding it, + 1 (0: none)
.float  vr_skewer_gripat;  // that hand's hold: how far further from the tip the gib is than the hand
.float  vr_skewer_gripoff; // and how far the hand was from the line as it took hold
.float  vr_skewer_gentle;  // its blade let go of and not yet gone across at half vr_embed_skewer_throw_burst: dropped, its landing (7 to 11 m/s, a fall) doesn't burst it
// Cut onto the edge (VR_Skewer_CutOn: a head or limb the blade in the hand just cut off). The same ride as a skewered gib (its think, weight,
// the other hand's grip, coming off, bursts), but held rigid where it went on in the hand's frame (vr_skewer_at), not along the blade's line,
// and only in the hand: let go of, thrown, holstered or stuck in a body, it falls. Off by a sharp stop from a fast move (a hard swing, a shake:
// VR_Skewer_Fling) or the other hand's tug (vr_embed_skewer_tug_*: from where it took hold, any way).
.float  vr_skewer_cut;     // TRUE: on the edge at its cut
.float  vr_skewer_cutt;    // when it went on
.vector vr_skewer_gripv;   // the other hand's hold, in the blade hand's frame from it
.vector vr_skewer_gripd;   // and from its middle, on its surface (world): where the hand has it once taken off (VR_Skewer_Take)
.float  vr_skewer_peak;    // its fastest own speed lately (m/s) ...
.vector vr_skewer_peakv;   // ... that move (units/s) ...
.float  vr_skewer_peakt;   // ... and when
float VR_SKEWER_CUT_SETTLE = 0.4; // s after it went on: the swing that cut it goes on and stops, no fling and no burst yet
float VR_SKEWER_FLING_WINDOW = 0.25; // s: a stop to half the speed within this of the fast move flings it off
float VR_SKEWER_CUT_REACH = 16; // units: the blade's edge (an axe's head, a sword's blade proper) from the cut, at most, to take the part

vector vr_skewer_o;   // VR_Skewer_Basis: the frame's origin
vector vr_skewer_tip; // VR_Skewer_Line: the blade's tip
vector vr_skewer_way; // and the way along it to the tip
float VR_SKEWER_YANK_FROM = 5; // cm from the line, or half the stretch that tears it off, before a fast move away is a yank (not a hand closing on it as it arrives, nor pushed off the blade)
float VR_SKEWER_SNAP = 30; // m/s in the hand: no swing's speed, a hand a wall held off and let go of: the speed before it counts (VR_Skewer_Impact)
float VR_SKEWER_THRUST = 0.7; // cosine of a move along the blade's line (VR_Skewer_Thrust, VR_Skewer_Impact)

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

// Whether `xHand` of `p` holds a blade a part it cuts off goes onto (vr_embed_cuton: 1 an edge's, the axes; 2 a point's too, the swords).
float(entity p, float xHand) VR_Skewer_CutBlade =
{
    float on = cvar_hget(cvarh_vr_embed_cuton);
    float flags = VRGetEntWeaponFlags(p, xHand);
    if(on <= 0 || VRIsHandWeaponless(p, xHand) || (flags & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return FALSE;
    }
    float kind = weaponvalue(WeaponIdToModel(VRGetEntWeapon(p, xHand), flags), "embed");
    return kind == AXESTICK_EDGE || (kind == AXESTICK_POINT && on >= 2);
};

// How wide it is, units (as the engine's sphere for it).
float(entity g) VR_Skewer_Radius =
{
    vector size = g.maxs - g.mins;
    return max(2, (size_x + size_y + size_z) / 6);
};

float VR_SKEWER_PALM = 2; // units: a hand on a gib's surface, its palm's middle that far out (the engine's skewerPalm)
float VR_SKEWER_MIDDLING = 5; // units: the radius of a gib or limb of the middling size, the weights' (VR_Skewer_Mass)

// Its weight, kg: a head's; a gib's or a limb's by its size.
float(entity g) VR_Skewer_Mass =
{
    if(g.vr_gib_healthmult >= 1.5)
    {
        return max(0, cvar_hget(cvarh_vr_embed_skewer_mass_head));
    }
    // (A limb's .vr_prop_mass, its share of the body it came from, is for holding it: a soldier's leg 12 kg on a blade.)
    return max(0, cvar_hget(g.vr_prop_mass > 0 ? cvarh_vr_embed_skewer_mass_limb : cvarh_vr_embed_skewer_mass_gib)) *
           min(1.5, max(0.5, VR_Skewer_Radius(g) / VR_SKEWER_MIDDLING));
};

float VR_SKEWER_HILT = 0.85; // the share of the blade's line from the tip that a gib can go along it, at most

// How far from the tip its place along the blade is, not slid.
float(entity g) VR_Skewer_Place0 =
{
    return cvar_hget(cvarh_vr_embed_skewer_depth) + g.vr_skewer_slot * cvar_hget(cvarh_vr_embed_skewer_gap);
};

// How far from the tip its middle sits on a blade whose line is `xLen` long (under 0: past it, coming off).
float(entity g, float xLen) VR_Skewer_Back =
{
    return min(VR_Skewer_Place0(g) + g.vr_skewer_shift, xLen * VR_SKEWER_HILT);
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

// The blade it rides: vr_skewer_tip, vr_skewer_way and how long its line is (0: it has none now). Leaves the v_ vectors anyhow.
float(entity g) VR_Skewer_Line =
{
    entity w = g.vr_skewer_w;
    if(w != world)
    {
        VR_Skewer_Basis(g);
        vr_skewer_way = VR_Skewer_FromBasis(g.vr_skewer_axis);
        vr_skewer_tip = weaponmuzzle(w);
        return vlen(vr_skewer_tip - w.origin);
    }
    if(!VR_Melee_Line(g.vr_skewer_p, g.vr_skewer_hand, VR_Melee_Thing(g.vr_skewer_p, g.vr_skewer_hand)))
    {
        return 0;
    }
    vr_skewer_tip = vr_mline_far;
    vr_skewer_way = normalize(vr_mline_far - vr_mline_grip);
    return vlen(vr_mline_far - vr_mline_grip);
};

// On a thrown blade, its place in the blade's frame from its place along the line (VR_Skewer_Line's).
void(entity g, float xLen) VR_Skewer_SetAt =
{
    VR_Skewer_Basis(g);
    g.vr_skewer_at = VR_Skewer_ToBasis(vr_skewer_tip - vr_skewer_way * VR_Skewer_Back(g, xLen) - g.vr_skewer_w.origin);
};

// Placed: its middle on the blade's line in the hand, at its place in the frame of a thrown blade. FALSE: no line to ride.
float(entity g) VR_Skewer_Place =
{
    vector c = '0 0 0';
    vector line = '0 0 0';
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_hand;
    if(g.vr_skewer_cut)
    {
        // (On the edge: where it went on, in the hand's frame.)
        VR_Skewer_Basis(g);
        c = vr_skewer_o + VR_Skewer_FromBasis(g.vr_skewer_at);
    }
    else
    {
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
    g.vr_skewer_cut = FALSE;
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
    // Into the hand where it held it (his note 2026-11-08 17:08: the hand had torn it off yards from it and held it there, a gap):
    // the gib comes to the hand, the hand on its surface where it took hold.
    if(g.vr_skewer_grip == xHand + 1 && g.vr_skewer_gripd != '0 0 0')
    {
        setorigin(g, g.origin + (VRGetEntHandPos(p, xHand) - g.vr_skewer_gripd - g.vr_skewer_c));
    }
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
    else if(p.classname == "player" && p.health > 0 && VRGetEntWeaponInst(p, g.vr_skewer_hand) == inst &&
            (g.vr_skewer_cut ? VR_Skewer_CutBlade(p, g.vr_skewer_hand) : VR_Skewer_HasBlade(p, g.vr_skewer_hand)))
    {
        return TRUE;
    }
    if(g.vr_skewer_cut)
    {
        return FALSE; // (on the edge only in the hand that cut it)
    }
    for(e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.weaponinst == inst)
        {
            // Let go of: on it as on the hand (it starts in the hand's pose), its place from the blade's own tip.
            g.vr_skewer_w = e;
            e.vr_skewer_gen = e.vr_skewer_gen + 1;
            VR_Skewer_SetAt(g, VR_Skewer_Line(g));
            g.vr_skewer_lastt = 0;
            g.vr_skewer_gentle = TRUE; // (until its way across says it was thrown: VR_Skewer_Impact)
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
            g.vr_skewer_lastt = 0;
            g.vr_skewer_grip = 0;
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

// The gib on `p`'s blades nearest `xHand`, or the one that hand holds already: a grip takes one.
entity(entity p, float xHand) VR_Skewer_ForGrip =
{
    entity best = world;
    float bestD = 0;
    vector at = VRGetEntHandPos(p, xHand);
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == p)
        {
            if(e.vr_skewer_grip == xHand + 1)
            {
                return e;
            }
            float d = vlen(e.vr_skewer_c - at);
            if(best == world || d < bestD)
            {
                best = e;
                bestD = d;
            }
        }
    }
    return best;
};

// A grip that let go of it, on a thrown blade: its sphere where it was slid to (the engine's addSkeweredShapes).
void(entity g) VR_Skewer_GripEnd =
{
    g.vr_skewer_grip = 0;
    if(g.vr_skewer_w != world)
    {
        g.vr_skewer_w.vr_skewer_gen = g.vr_skewer_w.vr_skewer_gen + 1;
    }
};

// The other hand on it. A grip within reach takes hold; from there it goes only along the blade with the hand, stopped at the hilt end and
// by the next gib along, until it is clear of the tip; or a tug across the line tears it off. TRUE: off, in that hand.
float(entity g) VR_Skewer_Grip =
{
    entity p = g.vr_skewer_p;
    float h = g.vr_skewer_grip - 1;
    float len;
    vector d;
    float along;
    if(h < 0)
    {
        h = VR_Skewer_Gripper(g, p);
        len = ((h >= 0) && (VR_Skewer_ForGrip(p, h) == g)) ? VR_Skewer_Line(g) : 0;
        if(len > 0)
        {
            d = VRGetEntHandPos(p, h) - vr_skewer_tip;
            along = -(d * vr_skewer_way);
            g.vr_skewer_grip = h + 1;
            g.vr_skewer_gripat = VR_Skewer_Back(g, len) - along;
            g.vr_skewer_gripoff = vlen(d + vr_skewer_way * along);
            // On its drawn surface, as a hand takes a prop lying about (VR_Carry_FitToHand: the engine's carryfit, the fist against the
            // model as drawn), the hand going to the gib here, the gib staying on its blade; the engine draws the hand there
            // (vr_view_hands.cpp skewerDrawnHand). It was put on a ball of the gib's box's size round its middle, 9 cm and more off
            // the gib itself: "the grabbing position can be far away from the actual visual model", his note 2026-11-12 19:52.
            makevectors(VRGetEntHandRot(p, h));
            vector palmWay = h == cVR_MainHand ? v_right * -1 : v_right;
            g.vr_skewer_gripd = VRGetEntHandPos(p, h) - carryfit(g, VRGetEntHandPos(p, h), palmWay) - g.vr_skewer_c;
            if(g.vr_skewer_cut)
            {
                VR_Skewer_Basis(g);
                g.vr_skewer_gripv = VR_Skewer_ToBasis(VRGetEntHandPos(p, h) - vr_skewer_o);
            }
            VRMeleeHapticByHand(h);
            dprint(sprintf("skewer: %s held by hand %g\n", g.classname, h));
        }
        return FALSE;
    }
    len = VR_Skewer_Line(g);
    if(!len || !VR_HandGrabUtil_IsHandGrabbing(p, h) || !VR_Hand_Free(p, h, 0) || VR_Carry_IsHolding(p, h))
    {
        VR_Skewer_GripEnd(g);
        return FALSE;
    }
    // A tug across the line: how far the hand has gone from it, how fast it goes from it (the hand's own, not the blade's move).
    d = VRGetEntHandPos(p, h) - vr_skewer_tip;
    along = -(d * vr_skewer_way);
    vector across = d + vr_skewer_way * along;
    vector rel = VRGetEntHandVel(p, h) - (g.vr_skewer_w != world ? g.vr_skewer_w.velocity * (1 / VRMetersToUnits()) : VRGetEntHandVel(p, g.vr_skewer_hand));
    float yank;
    float stretch;
    if(g.vr_skewer_cut)
    {
        // On the edge it doesn't slide: a tug any way from where the hand took hold of it, held to the blade's hand.
        VR_Skewer_Basis(g);
        across = VRGetEntHandPos(p, h) - (vr_skewer_o + VR_Skewer_FromBasis(g.vr_skewer_gripv));
        yank = vlen(across) > 1 ? rel * normalize(across) : vlen(rel);
        stretch = vlen(across) * (100 / VRMetersToUnits());
    }
    else
    {
        rel = rel - vr_skewer_way * (rel * vr_skewer_way);
        yank = vlen(across) > 1 ? rel * normalize(across) : vlen(rel);
        stretch = (vlen(across) - g.vr_skewer_gripoff) * (100 / VRMetersToUnits());
    }
    float tugSpeed = cvar_hget(cvarh_vr_embed_skewer_tug_speed);
    float tugStretch = cvar_hget(cvarh_vr_embed_skewer_tug_stretch);
    if((tugSpeed > 0 && yank >= tugSpeed && stretch >= max(VR_SKEWER_YANK_FROM, tugStretch * 0.5)) || (tugStretch > 0 && stretch >= tugStretch))
    {
        dprint(sprintf("skewer: %s tugged off: %.1f m/s, %.0f cm\n", g.classname, yank, stretch));
        VR_Skewer_Take(g, p, h);
        return TRUE;
    }
    if(g.vr_skewer_cut)
    {
        return FALSE;
    }
    // Along it, as far as the hilt end and the next gib along let it.
    float want = min(along + g.vr_skewer_gripat, len * VR_SKEWER_HILT);
    float now = VR_Skewer_Back(g, len);
    float gap = cvar_hget(cvarh_vr_embed_skewer_gap);
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e != g && e.vr_skewer_p == p && e.vr_skewer_inst == g.vr_skewer_inst && !e.vr_skewer_cut)
        {
            float place = VR_Skewer_Back(e, len);
            if(place < now && want < place + gap)
            {
                want = min(now, place + gap);
            }
            else if(place >= now && want > place - gap)
            {
                want = max(now, place - gap);
            }
        }
    }
    g.vr_skewer_shift = want - VR_Skewer_Place0(g);
    // Slid along the blade: flesh on steel, in both hands (his note 2026-11-08 17:10), as a blade sliding in a body
    // (VR_Embed_SlideFlesh: every Slide Sound cm a squelch).
    float slid = want - now;
    if(fabs(slid) > 0.01)
    {
        VR_Embed_SlideFlesh(g, slid, 0.01 * VRMetersToUnits());
        if(time >= g.vr_hold_buzz)
        {
            g.vr_hold_buzz = time + 0.06;
            float buzz = min(0.6, 0.2 + fabs(slid) / max(0.001, frametime) / VRMetersToUnits() * 0.3); // (by its speed along)
            VR_Embed_Haptic(p, h, 0.05, 45, buzz);
            VR_Embed_Haptic(p, g.vr_skewer_hand, 0.05, 45, buzz * 0.6);
        }
    }
    if(want <= -VR_Skewer_Radius(g))
    {
        dprint(sprintf("skewer: %s slid off the tip\n", g.classname));
        VR_Skewer_Take(g, p, h);
        return TRUE;
    }
    if(g.vr_skewer_w != world)
    {
        VR_Skewer_SetAt(g, len);
    }
    return FALSE;
};

vector(entity e) weaponmuzzle; // (vr_armedgrunt.qc)

// The thrown blade `w` that `g` is on bounced off something this frame (TRUE): its way along the last frame's move `xLastV` lost more than
// half of it (`xV`), it didn't stick, and its handle, middle or tip has something solid ahead of it, the way it was going, within `xReach`
// and that move. A blade meets a wall by its edge or flat while the gib on it is still short of it (test 21: 20 units off a wall a sword
// thrown flat met): what the blade hit at that speed, the gib on it hits too (its place alone would never reach it).
float(entity g, entity w, vector xLastV, vector xV, float xReach) VR_Skewer_ThrownBounce =
{
    float lastLen = vlen(xLastV);
    if(lastLen < 0.01 || VR_AxeStick_IsStuck(w) || xV * (xLastV * (1 / lastLen)) > 0.5 * lastLen)
    {
        return FALSE;
    }
    vector way = xLastV * (1 / lastLen);
    vector tip = weaponmuzzle(w);
    float far = xReach + lastLen;
    for(float i = 0; i < 3; i = i + 1)
    {
        vector at = w.origin + (tip - w.origin) * (i * 0.5);
        traceline(at, at + way * far, MOVE_HITMODEL_MELEE, w);
        if(trace_fraction < 1 && trace_ent != g.vr_skewer_p && trace_ent != g && (trace_ent.carry_player != g.vr_skewer_p || trace_ent == world))
        {
            dprint(sprintf("skewer: the thrown %s a gib is on bounced off %s\n", w.netname, trace_ent == world ? "the level" : trace_ent.classname));
            return TRUE;
        }
    }
    return FALSE;
};

// Going fast enough, or just stopped from it, with something right ahead of it: it meets it and bursts (TRUE). On a thrown blade fast
// by vr_embed_skewer_throw_burst; in the hand by vr_embed_skewer_swing_burst, the hand's own move (not the player's walk) and not along the
// blade's line (a thrust's tip meets things first). Measured on its own place, so a gib further along than what the blade hit has
// nothing ahead of it.
float(entity g) VR_Skewer_Impact =
{
    entity w = g.vr_skewer_w;
    entity p = g.vr_skewer_p;
    float dt = time - g.vr_skewer_lastt;
    vector v = g.vr_skewer_c - g.vr_skewer_lastc;
    vector lastv = g.vr_skewer_lastv;
    float lastsp = g.vr_skewer_lastsp;
    float first = ((!g.vr_skewer_lastt) || (dt <= 0) || VR_Blade_Jumped(g.vr_skewer_lastc, g.vr_skewer_c));
    vector from = g.vr_skewer_lastc;
    g.vr_skewer_lastt = time;
    g.vr_skewer_lastc = g.vr_skewer_c;
    if(first)
    {
        g.vr_skewer_lastsp = 0;
        g.vr_skewer_lastv = '0 0 0';
        return FALSE;
    }
    vector own = w != world ? v : v - p.velocity * dt;
    float sp = vlen(own) * (1 / dt) * (1 / VRMetersToUnits());
    vector across = own;
    across_z = 0;
    if(w != world && vlen(across) * (1 / dt) * (1 / VRMetersToUnits()) >= 0.5 * cvar_hget(cvarh_vr_embed_skewer_throw_burst))
    {
        g.vr_skewer_gentle = FALSE;
    }
    float snap = w == world && sp > VR_SKEWER_SNAP;
    if(snap)
    {
        sp = 0;
    }
    g.vr_skewer_lastsp = snap ? -1 : sp; // (a snap: no speed read, for VR_Skewer_Fling neither a fast move nor a stop)
    g.vr_skewer_lastv = v;
    if(g.vr_skewer_cut && time < g.vr_skewer_cutt + VR_SKEWER_CUT_SETTLE)
    {
        return FALSE; // (the swing that cut it, still going on through the body it came from)
    }
    // A hand the game moves (VR_HandMotionTracked no: the flat screen's hands turned with the mouse's look, a lean, the wall
    // tilt) swings nothing: its move bursts nothing (his note 2026-11-11, flat screen: a fast mouse turn shed what was on the axe).
    if(w == world && !VR_HandMotionTracked(p, g.vr_skewer_hand))
    {
        return FALSE;
    }
    if(max(sp, lastsp) < cvar_hget(w != world ? cvarh_vr_embed_skewer_throw_burst : cvarh_vr_embed_skewer_swing_burst) || (w != world && g.vr_skewer_gentle))
    {
        return FALSE;
    }
    entity ignore = w != world ? w : p;
    if(w == world)
    {
        VR_Skewer_Line(g);
        if(normalize(own) * vr_skewer_way >= VR_SKEWER_THRUST)
        {
            return FALSE;
        }
    }
    float reach = VR_Skewer_Radius(g) + 2;
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
        traceline(from, g.vr_skewer_c + ahead, MOVE_HITMODEL_MELEE, ignore);
        hit = ((trace_fraction < 1) && (trace_ent != p) && (trace_ent.carry_player != p || trace_ent == world));
    }
    if((!hit) && (vlen(lastv) > 0.01))
    {
        // (A bounce this frame turned its way: what it was going at.)
        vector ahead2 = normalize(lastv) * reach;
        traceline(g.vr_skewer_c, g.vr_skewer_c + ahead2, MOVE_HITMODEL_MELEE, ignore);
        hit = ((trace_fraction < 1) && (trace_ent != p) && (trace_ent.carry_player != p || trace_ent == world));
    }
    if((!hit) && w != world)
    {
        hit = VR_Skewer_ThrownBounce(g, w, lastv, v, reach);
    }
    if(!hit)
    {
        return FALSE;
    }
    dprint(sprintf("skewer: %s bursts on what it met at %.1f m/s\n", g.classname, max(sp, lastsp)));
    if(w == world)
    {
        VRMeleeHapticByHand(g.vr_skewer_hand);
    }
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

// On the edge (vr_skewer_cut), in the hand: a sharp stop from a fast move (a hard swing that stops, a shake's turn back) flings it off
// the way it was going (TRUE): its own speed (VR_Skewer_Impact's, not the walk) at least vr_embed_cuton_fling, then within
// VR_SKEWER_FLING_WINDOW down to half that. Not while the swing that cut it settles. The fast move is the hand's own
// (VR_HandMotionTracked: a controller, or flat-screen play's swing or punch, a button's action): his note 2026-11-11, flat
// screen, "If I turn my camera with the mouse quickly enough, gibs attached to the axe still fall out very easily, even for
// extreme values": the flat screen's hands turn with the look (a 860-degree-a-second mouse turn moved the head at 9.7 m/s,
// past Fling Speed 6, and its stop flung it). The game's moves make no fast move; the stop after the hand's own may come in
// them (a swing's end eases back into the look's pose). A snap (VR_Skewer_Impact: -1) is neither.
float(entity g) VR_Skewer_Fling =
{
    float fling = cvar_hget(cvarh_vr_embed_cuton_fling);
    float sp = g.vr_skewer_lastsp;
    if(fling <= 0 || time < g.vr_skewer_cutt + VR_SKEWER_CUT_SETTLE)
    {
        g.vr_skewer_peak = 0;
        return FALSE;
    }
    if(sp < 0)
    {
        return FALSE;
    }
    if(!VR_HandMotionTracked(g.vr_skewer_p, g.vr_skewer_hand))
    {
        if(time - g.vr_skewer_peakt > VR_SKEWER_FLING_WINDOW)
        {
            g.vr_skewer_peak = 0; // (the hand's own move long past: none)
            return FALSE;
        }
        if(sp >= g.vr_skewer_peak)
        {
            return FALSE; // (the look turning on: no stop, and no fast move of the hand's)
        }
    }
    else if(sp >= g.vr_skewer_peak || time - g.vr_skewer_peakt > VR_SKEWER_FLING_WINDOW)
    {
        g.vr_skewer_peak = sp;
        g.vr_skewer_peakv = vlen(g.vr_skewer_lastv) > 0.001 ? normalize(g.vr_skewer_lastv) * sp * VRMetersToUnits() : '0 0 0';
        g.vr_skewer_peakt = time;
        return FALSE;
    }
    if(g.vr_skewer_peak < fling || sp > g.vr_skewer_peak * 0.5)
    {
        return FALSE;
    }
    entity p = g.vr_skewer_p;
    vector v = g.vr_skewer_peakv + p.velocity;
    dprint(sprintf("skewer: %s flung off the edge: %.1f m/s, stopped to %.1f\n", g.classname, g.vr_skewer_peak, sp));
    VR_Skewer_Off(g);
    g.velocity = v;
    return TRUE;
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
    if(VR_Skewer_Grip(g))
    {
        return;
    }
    if(!VR_Skewer_Place(g))
    {
        VR_Skewer_Off(g);
        return;
    }
    if(!VR_Skewer_Impact(g) && g.vr_skewer_w == world && !(g.vr_skewer_cut && VR_Skewer_Fling(g)))
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
        if(e.vr_skewer_p == p && e.vr_skewer_w == world && e.vr_skewer_hand == xHand && !e.vr_skewer_cut)
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

// Whether a touch of `xHand` of `p` is a blade driven along its line (VR_Gib_Struck): the blow's to judge (VR_Skewer_Blow), not the touch's.
float(entity p, float xHand) VR_Skewer_Thrust =
{
    if(!cvar_hget(cvarh_vr_embed) || !cvar_hget(cvarh_vr_embed_skewer_most) || !VR_Skewer_HasBlade(p, xHand) ||
       !VR_Melee_Line(p, xHand, VR_Melee_Thing(p, xHand)))
    {
        return FALSE;
    }
    return normalize(VRGetEntHandVel(p, xHand)) * normalize(vr_mline_far - vr_mline_grip) >= VR_SKEWER_THRUST;
};

// Whether the blade of the blow `b` truly goes through `g`: its line, tip a touch on, vr_embed_skewer_thick thick, against its model as
// drawn (no shape of its own: its box).
float(entity b, entity g) VR_Skewer_Through =
{
    vector far = b.mh_far + normalize(b.mh_far - b.mh_grip) * 2;
    float thick = max(0, cvar_hget(cvarh_vr_embed_skewer_thick));
    float t = hitmodel_any(g, b.mh_grip, far, thick);
    if(t == -2)
    {
        vector pad = '1 1 1' * thick;
        t = VR_SegmentBox(b.mh_grip, far, g.origin + g.mins - pad, g.origin + g.maxs + pad);
    }
    return t >= 0;
};

// Whether `xHand` of `p` may skewer `g`: a pointed blade, a loose gib not held.
float(entity p, float xHand, entity g) VR_Skewer_Can =
{
    // (A head's .gorging is set for life: the gremlins' mark of what they've done with, not a gremlin at it.)
    return cvar_hget(cvarh_vr_embed) && cvar_hget(cvarh_vr_embed_skewer_most) > 0 && p.ishuman && !g.vr_sgib && g.carry_player == world &&
           g.vr_skewer_p == world && VR_Skewer_HasBlade(p, xHand);
};

// The blow of `xHand` of self being dealt (VR_Melee_Deal) is a stab that goes by the loose gib the sweep's wider reach found: no blow at all.
float(float xHand, entity b) VR_Skewer_GoesBy =
{
    entity g = b.mh_c_ent;
    if(!g.vr_gib || !VR_Skewer_Can(self, xHand, g) || VR_Embed_StabWhy(b, b.mh_c_pos, b.mh_c_dir, b.mh_speed, 0, b.mh_kind) != string_null ||
       VR_Skewer_Through(b, g))
    {
        return FALSE;
    }
    if(cvar_hget(cvarh_vr_debug_embed))
    {
        embedsay(sprintf("%s NOT SKEWERED: the blade's line goes by it", g.classname));
    }
    return TRUE;
};

// A sweep's thing: a blade thrust along its line finds no loose gib its line doesn't go through (VR_Melee_Sweep: it isn't what it is aimed
// at, and mustn't take the blow from one that is).
float(float xHand, entity b, entity e) VR_Skewer_SweepPasses =
{
    return e.vr_gib && VR_Skewer_Can(self, xHand, e) && VR_Skewer_Thrust(self, xHand) && !VR_Skewer_Through(b, e);
};

// The blow of `xHand` of self on the loose gib `g` (VR_Melee_Deal). TRUE: a stab that put it on the blade (no other blow).
float(float xHand, entity b, entity g, vector xAt, vector xDir, float xSpeed, float xKind) VR_Skewer_Blow =
{
    if(!VR_Skewer_Can(self, xHand, g))
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
    g.vr_skewer_cut = FALSE;
    g.vr_skewer_p = self;
    g.vr_skewer_hand = xHand;
    g.vr_skewer_slot = slot;
    g.vr_skewer_shift = 0;
    g.vr_skewer_grip = 0;
    g.vr_skewer_lastt = 0;
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

// A blow of `xHand` of self being dealt (VR_Melee_Deal), not a stab, not on a gib: fast enough, its contact (mh_c_pos) by a gib's place
// along the blade (a tip's blow reaches the first gib: vr_embed_skewer_swing_reach past its width) bursts that one.
void(float xHand, entity b) VR_Skewer_SwingHit =
{
    if(b.mh_c_ent.vr_gib || b.mh_kind == VR_MKIND_STAB || b.mh_c_pos == '0 0 0' || b.mh_speed < cvar_hget(cvarh_vr_embed_skewer_swing_burst))
    {
        return;
    }
    vector line = b.mh_far - b.mh_grip;
    float at = (b.mh_far - b.mh_c_pos) * normalize(line); // (from the tip)
    float reach = cvar_hget(cvarh_vr_embed_skewer_swing_reach);
    for(entity e = nextent(world); e; e = nextent(e))
    {
        // (On the edge: its place along the line where it sits; not by the swing that cut it, still going.)
        if(e.vr_skewer_p == self && e.vr_skewer_w == world && e.vr_skewer_hand == xHand &&
           !(e.vr_skewer_cut && time < e.vr_skewer_cutt + VR_SKEWER_CUT_SETTLE) &&
           fabs(at - (e.vr_skewer_cut ? (b.mh_far - e.vr_skewer_c) * normalize(line) : VR_Skewer_Back(e, vlen(line)))) <= VR_Skewer_Radius(e) + reach)
        {
            dprint(sprintf("skewer: %s bursts: the swing's contact at it, %.1f m/s\n", e.classname, b.mh_speed));
            VR_Gib_Burst(e, b.mh_c_dir);
        }
    }
};

// The head or limb `g` just cut off `c` at `xCut` (the neck, the joint) by `by`'s blow of `xHow` (VR_Decap_Cut, VR_Limb_Cut): his note
// 2026-11-06, "the cut off limb should get stuck on the blade that cut it off". A swing's cut by a hand whose blade goes onto it
// (VR_Skewer_CutBlade) with its edge at the cut (the blade proper: an axe's head, a sword past VR_DECAP_BLADE_SWORD; within
// VR_SKEWER_CUT_REACH): it rides that hand, its cut end moved onto the edge, turned as it was. TRUE: on it.
float(entity g, entity c, entity by, float xHow, vector xCut) VR_Skewer_CutOn =
{
    if(g == world || !g.vr_gib || xHow != QVR_DECAP_SWING || !(by.flags & FL_CLIENT) || !by.ishuman || by.health <= 0 ||
       !cvar_hget(cvarh_vr_embed) || g.carry_player != world || g.vr_skewer_p != world)
    {
        return FALSE;
    }
    float hand = -1;
    float bestD = VR_SKEWER_CUT_REACH;
    vector on = '0 0 0';
    for(float h = 0; h < 2; ++h)
    {
        float th = VR_Melee_Thing(by, h);
        if(!VR_Skewer_CutBlade(by, h) || !VR_Melee_Line(by, h, th))
        {
            continue;
        }
        vector line = vr_mline_far - vr_mline_grip;
        float from = th == VR_MTHING_AXE ? VR_DECAP_BLADE_AXE : VR_DECAP_BLADE_SWORD;
        float s = vlen(line) > 0.01 ? ((xCut - vr_mline_grip) * line) / (line * line) : 0;
        vector at = vr_mline_grip + line * min(1, max(from, s));
        float d = vlen(xCut - at);
        if(d < bestD)
        {
            hand = h;
            bestD = d;
            on = at;
        }
    }
    if(hand < 0)
    {
        dprint(sprintf("skewer: %s cut off, not onto a blade: no edge at the cut\n", g.classname));
        return FALSE;
    }
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_skewer_p == by && e.vr_skewer_cut && e.vr_skewer_hand == hand)
        {
            n = n + 1;
        }
    }
    if(n >= cvar_hget(cvarh_vr_embed_skewer_most))
    {
        dprint(sprintf("skewer: %s cut off, not onto the blade: it has %g already\n", g.classname, n));
        return FALSE;
    }
    VR_Head_StopFlies(g);
    setorigin(g, g.origin + (on - xCut)); // (its cut end on the edge)
    // In the blade, not beside it (his note 2026-11-08 17:03): its middle ahead of the edge the way the swing went (else the side
    // it is on), the edge in it by vr_embed_cuton_depth of its radius.
    float depthShare = max(0, min(1, cvar_hget(cvarh_vr_embed_cuton_depth)));
    if(depthShare > 0 && VR_Melee_Line(by, hand, VR_Melee_Thing(by, hand)))
    {
        vector lw = normalize(vr_mline_far - vr_mline_grip);
        vector a0 = g.angles;
        a0_x = -a0_x;
        makevectors(a0);
        vector mid0 = (g.mins + g.maxs) * 0.5;
        vector cNow = g.origin + v_forward * mid0_x - v_right * mid0_y + v_up * mid0_z;
        vector sw = VRGetEntHandVel(by, hand);
        sw = sw - lw * (sw * lw);
        if(vlen(sw) < 1) // (m/s: no swing to go by)
        {
            sw = cNow - on;
            sw = sw - lw * (sw * lw);
        }
        if(vlen(sw) > 0.001)
        {
            vector want = on + normalize(sw) * (VR_Skewer_Radius(g) * (1 - depthShare));
            setorigin(g, g.origin + (want - cNow));
        }
    }
    g.vr_skewer_cut = TRUE;
    g.vr_skewer_cutt = time;
    g.vr_skewer_peak = 0;
    g.vr_skewer_p = by;
    g.vr_skewer_hand = hand;
    g.vr_skewer_slot = 0;
    g.vr_skewer_shift = 0;
    g.vr_skewer_grip = 0;
    g.vr_skewer_lastt = 0;
    g.vr_skewer_inst = VRGetEntWeaponInst(by, hand);
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
    g.vr_gib_flingtill = 0; // (no wall to stick to: its fling watch ends)
    g.flags = g.flags - (g.flags & (FL_FORCEGRABBABLE | FL_ONGROUND));
    // Its turn and its middle as they are now, in the hand's frame.
    vector a = g.angles;
    a_x = -a_x;
    makevectors(a);
    vector f = v_forward;
    vector u = v_up;
    vector mid = (g.mins + g.maxs) * 0.5;
    vector c0 = g.origin + v_forward * mid_x - v_right * mid_y + v_up * mid_z;
    VR_Skewer_Basis(g);
    g.vr_skewer_fw = VR_Skewer_ToBasis(f);
    g.vr_skewer_up = VR_Skewer_ToBasis(u);
    g.vr_skewer_at = VR_Skewer_ToBasis(c0 - vr_skewer_o);
    VR_Skewer_Place(g);
    VRMeleeHapticByHand(hand);
    dprint(sprintf("skewer: %s cut off %s onto hand %g's blade (its edge %.1f units from the cut)\n", g.classname, c.classname, hand, bestD));
    return TRUE;
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
