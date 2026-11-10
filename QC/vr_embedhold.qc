// vr_embedhold.qc -- Embedded blades, stage 2: held stabs and swung axes (ROUND23.md, "Embedded blades, stage 2"; vr_embed).
//
// A held sword or pointed blade driven tip first along its line into a body, or a swung axe landing edge first, fast enough,
// stays in it on top of the blow. It is then what a thrown one is (vr_axestick.qc, vr_axestick.cpp): the hand's weapon becomes a
// stuck thrown_weapon, riding the triangle it went into, bleeding, falling when the limb is cut or the body gibbed. The hand
// (VR_HOLD_EMBED, the blade in .vr_hold_ent_*) holds it, drawn on it (vr_view_hands.cpp embedDrawnHand), striking and touching
// nothing (VR_Hand_HeldOff): its tracked place only says how the blade slides along its axis (.vr_stick_dir, .vr_stick_slide),
// with resistance, and comes out: pulled far enough, or off the axis, or the hand left behind by a limb moving away, it is the
// hand's again, by its handle (VR_Embed_Free). Opening the hand leaves it there; nothing to throw, the hand holds no weapon.
// A grip on it takes it again on the same axis (VR_Embed_Grip); a force grab tugs it out as any stuck blade.
//
// The hold goes by the hand's own motion (.vr_hold_err along the axis, .vr_hold_off off it), not by where the blade is: a body's
// flinch swings a hilt on a long lever. Both start at the gap between the real hand and the drawn one (VR_Embed_SetG0): a gap is
// fine. Going back to the drawn hand does nothing (.vr_hold_slack, .vr_hold_offslack: what is left of it); past it, the hand drives. For stage 3 (moving the body, stagger, the speed penalty): VR_Embed_HoldThink has them,
// `far` (how much further the hand is from the wound than it held it from) and the depth; everything about the hold is on the blade.
//
// Moving the body (ROUND23.md, "Embedded blades, his sixth test"). How far a blade throws a body about is its monster group's Skewer
// (groups.txt; VR_Embed_Skewer): a stab hard enough staggers it (VR_Embed_Stagger: how long, how far off balance, by it, how deep, one hand
// or two). Any ragdoll of it (staggering, knocked down, dead) is held by the limb the blade is in (VR_Embed_Spring: a spring from the hand,
// stronger the deeper, with two hands, the more Skewer; ragdolltug 12). The hand's motion along the blade outwards pulls the blade out, one
// to one, and leaves the body (the spring's target, VR_Embed_AxialOf); across the blade, or pushed in past how deep it goes, it moves the
// body. In a ragdoll, off the axis it only comes free by a yank (vr_embed_hold_yank). Standing, walking away drags it, from
// vr_embed_light_hold's Skewer up (VR_Embed_Drag). Past reach, it comes out. VR_Embed_TetherShare: your speed.
//
// It goes in only where the blade as drawn truly meets the drawn model (the engine's bladeMeets). A blow lands by the melee's sweep, which
// has width: landed short, the blade may still come to touch it for a moment (VR_Embed_WaitFrame).

void(entity p) VR_Skewer_AllOff; // (vr_skewer.qc)

.entity vr_hold_player;   // who holds it (world: nobody)
.float  vr_hold_hands;    // the hands on it (bits: VR_Embed_Bit)
.float  vr_hold_handle;   // the hand on it by its handle (a bit; 0: none)
.float  vr_hold_distoff;  // the off hand's tracked place from where it went in, as it took hold (as if not slid)
.float  vr_hold_distmain; // the main hand's
.vector vr_hold_lastoff;  // the off hand's tracked place last frame
.vector vr_hold_lastmain; // the main hand's
.float  vr_hold_err;      // where the hand(s) are along its axis from the drawn hand, deeper positive (units)
.vector vr_hold_off;      // and off its axis (units)
.float  vr_hold_slack;    // the gap along it that does nothing, signed as vr_hold_err (0: the real hand has met the drawn one)
.float  vr_hold_offslack; // and off it
.float  vr_hold_sndgo;    // units slid since its last flesh sound, signed by the way it goes
.float  vr_hold_stoff;    // the off hand's grip's states towards opening (VR_Embed_HandOpen)
.float  vr_hold_stmain;
.float  vr_hold_last;     // the last frame's time
.float  vr_hold_far;      // cm further from where it went in than the hand held it (for the debug view)
.float  vr_hold_farmost;  // and its limit
.float  vr_hold_stick;    // cm the hand moves before it does, now
.float  vr_hold_owed;     // sliding deeper's damage not dealt yet
.float  vr_hold_buzz;     // the next slide's buzz
.vector vr_hold_lastwound; // where it went in, last frame
.vector vr_hold_lastorg;  // where its holder stood, last frame
.vector vr_hold_lasthost; // where its body stood, last frame
.float  vr_hold_tether;   // in a dead body: it is being dragged (VR_Embed_TetherShare)
.float  vr_hold_axial;    // the hand's motion along the blade since its spring began that isn't the body's (units, deeper positive)
.vector vr_hold_rot0;     // the handle hand's angles as it took hold (the engine's leeway: vr_embed_lean_*)
.float  vr_hold_tugv;     // an axe's tug: the hand's pull out, averaged over vr_embed_hold_axe_tug_time (m/s)
.float  vr_hold_gapoff;   // the least cm the off hand has been past its stretched arm since it took hold (.vr_embed_armgap's x)
.float  vr_hold_gapmain;  // the main hand's
.float  vr_hold_carried;  // a dead body carried by a slow pull: the hand's way out along it, the last moments' (a leaky sum, units)
.float  vr_hold_carrying; // and whether it was carried last frame
.float  vr_hold_strain;   // its spring's pull now, a share of its most (VR_Embed_Spring; 0 none): a dead body carried that won't come is at 1
.vector vr_hold_gapat;    // when each took hold (x off, y main): its gap counts once its drawn hand has settled on the blade
.entity vr_embed_wait_off;   // on a player: the body a blow of this hand landed short of (VR_Embed_WaitFrame)
.entity vr_embed_wait_main;
.float  vr_embed_waitto_off; // and until when
.float  vr_embed_waitto_main;
.vector vr_embed_waitat_off; // where it landed
.vector vr_embed_waitat_main;
.vector vr_embed_waitvel_off; // at what speed
.vector vr_embed_waitvel_main;
.float  vr_embed_waitmdl_off; // its model then
.float  vr_embed_waitmdl_main;
.float  vr_embed_quiet_off;  // on a player: the hand strikes nothing until then (it just came out)
.float  vr_embed_quiet_main;

// Wooden crates and barrels (his notes 2026-11-08, items 13 and 14; vr_embed_props): a held stab or swing hard enough stays in one as in
// a body (the engine's stickHeld: its Box3D body met, kind AXESTICK_PROP, riding its origin and angles). The hold is a body's but for its
// moving: no ragdoll to hold, no monster to drag, but a pull on the crate's body at the wound towards where the hand has it
// (VR_Embed_PropSpring: the engine's embedproppull, a spring capped at vr_embed_prop_force, so a heavy crate lags the hand and a blade
// can't lift it one-handed), and it slides harder in wood (vr_embed_prop_stick). Breaking the crate frees it (VR_AxeStick_Think).
.float  vr_hold_propon;   // the pull's hold taken (its offset): from the hand's next think on
.vector vr_hold_propoff;  // the wound's place from the handle hand as it took hold
.vector vr_hold_proptgt;  // where the pull wanted the wound last think
.float  vr_hold_proppull; // its pull then, N (the test's)

// Whether a blow may leave a blade in `m`: a wooden crate or barrel lying loose (not in a hand), with vr_embed_props.
float(entity m) VR_Embed_PropHost =
{
    return cvar_hget(cvarh_vr_embed_props) && VR_Crate_Is(m) && m.carry_player == world && m.solid == SOLID_BBOX;
};

// Whether the blade `e` is in one (stuck there by a hand's blow or a throw).
float(entity e) VR_Embed_InProp =
{
    return e.vr_stick_kind == AXESTICK_PROP && cvar_hget(cvarh_vr_embed_props) && VR_Crate_Is(e.vr_stick_ent);
};

float VR_EMBED_QUIET = 0.25;     // s a hand that just pulled a blade out strikes nothing
float VR_EMBED_OFF_RELAX = 1.5;  // 1/s: a hand slowly drifting off the axis is forgiven; a hard pull isn't
float VR_EMBED_JUMP = 9;         // m/s: the hand's place moving faster is a jump (a stop against the body let go of it), no pull
float VR_EMBED_ST_RELEASE = 3;   // VR_Embed_HandOpen: the grip was held: open once it isn't

// The blow being dealt that may leave the blade in (VR_Embed_Blow, for VR_Embed_BlowsDone at the end of them).
entity vr_embed_host;
float  vr_embed_hostmodel;
vector vr_embed_at;
vector vr_embed_vel;
float  vr_embed_stabbing;  // inside embedstick: VR_AxeStick_Stuck's blow is dealt already, no thrown axe's cut or highlight
float  vr_embed_gap;       // VR_Embed_Try's -1: units the blade was off the model
float  vr_embed_last_hold;   // (for the test: the last stagger's time)
float  vr_embed_last_amount; // (and its amount)

float(float xHand) VR_Embed_Bit =
{
    return xHand == cVR_MainHand ? 2 : 1;
};

float(entity e) VR_Embed_Held =
{
    return e.vr_hold_hands != 0;
};

// Whether `xHand` of `xPlayer` still holds the blade `e` (VR_Hand_BusyHolds).
float(entity e, entity xPlayer, float xHand) VR_Embed_HeldBy =
{
    return e != world && e.vr_hold_player == xPlayer && (e.vr_hold_hands & VR_Embed_Bit(xHand)) && VR_AxeStick_IsStuck(e);
};

// Whether `xHand` of `p` strikes (VR_Melee_Strike): not a moment after pulling a blade out.
float(entity p, float xHand) VR_Embed_HandMayStrike =
{
    return time >= (xHand == cVR_MainHand ? p.vr_embed_quiet_main : p.vr_embed_quiet_off);
};

// Where `xHand` holds it from now on: nothing pending.
void(entity e, entity p, float xHand) VR_Embed_SetG0 =
{
    vector at = VRGetEntHandPos(p, xHand);
    // (From the drawn hand if the real one is nearer the wound than it: coming back to it is no going further. e.origin: the drawn hand
    // on its handle, vr_view_hands.cpp embedDrawnHand.)
    float from = max(vlen(at - e.vr_stick_at), vlen(e.origin - e.vr_stick_at)) + e.vr_stick_slide;
    if(xHand == cVR_MainHand)
    {
        e.vr_hold_distmain = from;
        e.vr_hold_lastmain = at;
    }
    else
    {
        e.vr_hold_distoff = from;
        e.vr_hold_lastoff = at;
    }
    e.vr_hold_lastwound = e.vr_stick_at;
    e.vr_hold_lastorg = p.origin;
    e.vr_hold_lasthost = e.vr_stick_ent.origin;
    // The gap to the drawn hand is the handle hand's; a second hand helping changes nothing.
    if(VR_Embed_Bit(xHand) == e.vr_hold_handle)
    {
        e.vr_hold_rot0 = VRGetEntHandRot(p, xHand); // (the leeway turns it from here: vr_axestick.cpp place)
        vector gap = at - e.origin;
        e.vr_hold_err = gap * e.vr_stick_dir;
        e.vr_hold_off = gap - e.vr_stick_dir * e.vr_hold_err;
        e.vr_hold_slack = e.vr_hold_err;
        e.vr_hold_offslack = vlen(e.vr_hold_off);
        if(VR_Embed_InProp(e))
        {
            e.vr_hold_propon = FALSE; // (the crate's pull takes its hold again: VR_Embed_PropSpring)
            e.vr_hold_axial = 0;
        }
    }
};

// A buzz in `xHand` of `p` (haptic is `self`'s, and the hold's think and a hand's touch run as the blade).
void(entity p, float xHand, float xFor, float xFreq, float xAmp) VR_Embed_Haptic =
{
    entity was = self;
    self = p;
    haptic(xHand, 0.0, xFor, xFreq, xAmp);
    self = was;
};

// `xHand` of `xPlayer` holds the stuck blade `e` now.
void(entity e, entity xPlayer, float xHand) VR_Embed_TakeHand =
{
    VR_Hand_Busy(xPlayer, xHand, VR_HOLD_EMBED, e, "embedded blade");
    if(VR_Hand_Kind(xPlayer, xHand) != VR_HOLD_EMBED)
    {
        return;
    }
    float bit = VR_Embed_Bit(xHand);
    e.vr_hold_player = xPlayer;
    e.vr_hold_hands = e.vr_hold_hands | bit;
    if(!e.vr_hold_handle)
    {
        e.vr_hold_handle = bit;
    }
    e.vr_hold_last = time;
    e.vr_hold_tugv = 0;
    e.vr_hold_carried = 0;
    e.vr_hold_carrying = FALSE;
    if(xHand == cVR_MainHand)
    {
        e.vr_hold_gapmain = 1000000;
        e.vr_hold_gapat_y = time;
    }
    else
    {
        e.vr_hold_gapoff = 1000000;
        e.vr_hold_gapat_x = time;
    }
    VR_Embed_SetG0(e, xPlayer, xHand);
    // (Grip mode 1, or a weapon given and never gripped: it opens by a press and a release, as the weapon's let-go.)
    float st = VR_EMBED_ST_RELEASE;
    if(!VR_HandGrabUtil_IsHandGrabbing(xPlayer, xHand) || cvar_hget(cvarh_vr_weapon_grip_mode) == 1)
    {
        st = 0;
    }
    if(xHand == cVR_MainHand)
    {
        e.vr_hold_stmain = st;
    }
    else
    {
        e.vr_hold_stoff = st;
    }
    VR_Embed_Haptic(xPlayer, xHand, 0.08, 50, 0.6);
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: %s held by hand %g, %.1f units in, axis %v\n", e.netname, xHand, e.vr_stick_depth + e.vr_stick_slide,
            e.vr_stick_dir));
    }
};

// `xHand` is off the blade `e` (it stays).
void(entity e, float xHand, string xWhy) VR_Embed_DropHand =
{
    float bit = VR_Embed_Bit(xHand);
    if(!(e.vr_hold_hands & bit))
    {
        return;
    }
    entity p = e.vr_hold_player;
    if(p != world && e.vr_stick_ent != world)
    {
        ragdolltug(e.vr_stick_ent, p, xHand, 4); // (its spring, if this hand's: VR_Embed_Spring)
    }
    e.vr_hold_hands = e.vr_hold_hands - bit;
    if(e.vr_hold_handle == bit)
    {
        e.vr_hold_handle = e.vr_hold_hands; // (the other's, if it is on it; it holds on where it is)
    }
    if(!e.vr_hold_hands)
    {
        e.vr_hold_player = world;
        e.vr_stick_goal = 0;
        e.vr_hold_tether = FALSE;
        e.vr_stick_shake = 0;
    }
    if(p != world)
    {
        VR_Hand_Unbusy(p, xHand, VR_HOLD_EMBED, xWhy);
    }
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: hand %g off %s: %s\n", xHand, e.netname, xWhy));
    }
};

// Both hands off it, it stays.
void(entity e, string xWhy) VR_Embed_DropHands =
{
    VR_Embed_DropHand(e, cVR_OffHand, xWhy);
    VR_Embed_DropHand(e, cVR_MainHand, xWhy);
};

// The blade `e` comes free into the hand on it (its handle's, else the other), with a small spurt (the hand's taking it:
// VR_AxeStick_PulledOut). Not a new strike: its blow goes on as the same motion, on the same body (VR_Melee_Rearm).
void(entity e, string xWhy) VR_Embed_Free =
{
    entity p = e.vr_hold_player;
    float hand = (e.vr_hold_handle ? e.vr_hold_handle : e.vr_hold_hands) == 2 ? cVR_MainHand : cVR_OffHand;
    entity host = e.vr_stick_ent;
    vector dir = e.vr_stick_dir;
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: %s comes free: %s, hand %g, %.1f units in\n", e.netname, xWhy, hand, e.vr_stick_depth + e.vr_stick_slide));
    }
    if(cvar_hget(cvarh_vr_debug_embed))
    {
        embedsay(sprintf("CAME OUT: %s (%.1f units in)", xWhy, e.vr_stick_depth + e.vr_stick_slide));
    }
    VR_Embed_DropHands(e, xWhy);
    if(p.classname != "player" || p.health <= 0 || !VR_Hand_Free(p, hand, VR_HANDFREE_LOCKED))
    {
        VR_AxeStick_PulledOut(e);
        VR_AxeStick_Free(e, '0 0 0');
        return;
    }
    e.vr_grab_by_handle = TRUE;
    entity was = self;
    wpnthrow_handtouch_impl(hand, e, p, VRGetEntWeaponPtr(p, hand), VRGetEntWeaponFlagsPtr(p, hand)); // (it leaves self the player)
    self = p;
    entity b = VR_Blow(hand);
    VRSetMeleeHitSoundPlayed(hand, TRUE);
    b.mh_hit_time = VR_PlayerTime();
    b.mh_hit_touch = b.mh_hit_time;
    b.mh_hit_ent = host;
    b.mh_hit_dir = dir * -1;
    if(host != world && !VR_Melee_HitHas(b, host))
    {
        VR_Melee_HitAdd(b, host, FALSE);
    }
    if(hand == cVR_MainHand)
    {
        p.vr_embed_quiet_main = time + VR_EMBED_QUIET;
    }
    else
    {
        p.vr_embed_quiet_off = time + VR_EMBED_QUIET;
    }
    self = was == e ? p : was; // (`e` is gone)
};

// A hand's grip starting on the stuck blade `e`: in a body, it takes hold of it on its axis (TRUE: VR_Embed_HoldThink from here
// on). In a wall, a door, a prop: as before, it comes.
float(entity e, entity xPlayer, float xHand) VR_Embed_Grip =
{
    if(!VR_AxeStick_IsStuck(e) || !cvar_hget(cvarh_vr_embed) || !(VR_AxeStick_InMonster(e) || VR_Embed_InProp(e)) || VR_AxeStick_Tugging(e) ||
       (e.vr_hold_player != world && e.vr_hold_player != xPlayer))
    {
        return FALSE;
    }
    VR_Embed_TakeHand(e, xPlayer, xHand);
    return VR_Hand_Kind(xPlayer, xHand) == VR_HOLD_EMBED;
};

// Whether `xHand` opened on the blade `e` this frame.
float(entity e, entity p, float xHand) VR_Embed_HandOpen =
{
    float grabbing = VR_HandGrabUtil_IsHandGrabbing(p, xHand);
    float st = xHand == cVR_MainHand ? e.vr_hold_stmain : e.vr_hold_stoff;
    float next = st;
    if(st == VR_EMBED_ST_RELEASE)
    {
        return !grabbing;
    }
    if(st == 0 && !grabbing)
    {
        next = 1;
    }
    else if(st == 1 && grabbing)
    {
        next = 2;
    }
    else if(st == 2 && !grabbing)
    {
        return TRUE;
    }
    if(xHand == cVR_MainHand)
    {
        e.vr_hold_stmain = next;
    }
    else
    {
        e.vr_hold_stoff = next;
    }
    return FALSE;
};

// A hand on it this frame: opened (it stays), another weapon put in it (a weapon's switch; it stays).
void(entity e, entity p, float xHand) VR_Embed_HandFrame =
{
    if(!(e.vr_hold_hands & VR_Embed_Bit(xHand)))
    {
        return;
    }
    if(VR_Hand_Kind(p, xHand) != VR_HOLD_EMBED || !VRIsHandWeaponless(p, xHand))
    {
        VR_Embed_DropHand(e, xHand, "the hand has something else");
    }
    else if(VR_Embed_HandOpen(e, p, xHand))
    {
        VR_Embed_DropHand(e, xHand, "let go");
    }
};

// A line on the debug panel; a reason with its value against its limit.
void(string xText) VR_Embed_Say =
{
    if(cvar_hget(cvarh_vr_debug_embed))
    {
        embedsay(xText);
    }
};

string(string xWhy, float xValue, float xLimit, string xUnit) VR_Embed_Why =
{
    return cvar_hget(cvarh_vr_debug_embed) ? sprintf("%s: %.1f of %.1f %s", xWhy, xValue, xLimit, xUnit) : xWhy;
};

// How it holds the hand back now (`xIn`: how far in, 0..1): how far the hand moves before it does, cm...
float(entity e, float xIn, float xCorpse) VR_Embed_Stick =
{
    float s = e.vr_stick_embed == AXESTICK_POINT ?
              cvar_hget(cvarh_vr_embed_hold_sword_stick) + (cvar_hget(cvarh_vr_embed_hold_sword_stick_deep) - cvar_hget(cvarh_vr_embed_hold_sword_stick)) * xIn :
              cvar_hget(cvarh_vr_embed_hold_axe_stick);
    return max(0, s) * (xCorpse ? max(0.01, cvar_hget(cvarh_vr_embed_hold_corpse)) : 1);
};

// ... and the most it slides at, cm/s.
float(entity e, float xIn, float xCorpse) VR_Embed_Rate =
{
    float r = e.vr_stick_embed == AXESTICK_POINT ?
              cvar_hget(cvarh_vr_embed_hold_sword_rate) + (cvar_hget(cvarh_vr_embed_hold_sword_rate_deep) - cvar_hget(cvarh_vr_embed_hold_sword_rate)) * xIn :
              cvar_hget(cvarh_vr_embed_hold_axe_rate);
    return max(1, r) / (xCorpse ? max(0.01, cvar_hget(cvarh_vr_embed_hold_corpse)) : 1);
};

// How far in the blade is, of as far as it goes (0..1).
float(entity e) VR_Embed_InShare =
{
    if(e.vr_stick_embed != AXESTICK_POINT)
    {
        return 0;
    }
    float most = max(1, e.vr_stick_bladelen) * max(0, cvar_hget(cvarh_vr_embed_hold_sword_most)) * 0.01;
    return max(0, min(1, (e.vr_stick_depth + e.vr_stick_slide) / max(1, most)));
};

float(entity m, float xHand, float xUnused) VR_FoeGrab_Strength;
float(entity e) VR_IsDummy;
float(entity e, vector xForward, float xPush, float xBalance) VR_Knockdown_Stagger;

// How far a blade stuck in `m` throws it about, 0..1: its monster group's Skewer (groups.txt). 0: never.
float(entity m) VR_Embed_Skewer =
{
    if(m == world || (m.flags & (FL_FLY | FL_SWIM)) || (VR_IsDummy(m) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return 0;
    }
    return max(0, min(1, monstergroupget(m, MGP_SKEWER)));
};

// Whether walking away from a standing `m` with a blade in it drags it.
float(entity m) VR_Embed_Light =
{
    float s = VR_Embed_Skewer(m);
    return s > 0 && s >= cvar_hget(cvarh_vr_embed_light_hold);
};

// How far a blow of `xSpeed` m/s took it in, of as far as one goes (0..1). A sword: a stab's depth (going on to it). An axe's depth goes
// by the blow's speed alone (the engine's stickHeld: half of it at most at Swing's Least Speed, all of it at twice that).
float(entity e, float xSpeed) VR_Embed_DepthShare =
{
    if(e.vr_stick_embed == AXESTICK_POINT)
    {
        float in = max(e.vr_stick_goal, e.vr_stick_depth + e.vr_stick_slide);
        return max(0, min(1, in / (max(1, e.vr_stick_bladelen) * max(1, cvar_hget(cvarh_vr_embed_sword_depth_max)) * 0.01)));
    }
    return max(0.5, min(1, xSpeed / max(0.1, 2 * cvar_hget(cvarh_vr_embed_swing_speed))));
};

// The blow that left `e` in `m` went at `xSpeed` m/s: strong enough, it staggers, up again by itself: for how long and how far off balance
// by the blade (a sword's longer than an axe's), how deep, with two hands, and its Skewer (groups.txt): a grunt thrown off balance by a deep
// stab, an ogre a little, a Shambler a moment, standing on, a boss not at all. What has no ragdoll to stagger stops a moment instead.
void(entity e, entity m, vector xDir, float xSpeed) VR_Embed_Stagger =
{
    float point = e.vr_stick_embed == AXESTICK_POINT;
    float least = point ? cvar_hget(cvarh_vr_embed_stagger_sword_speed) : cvar_hget(cvarh_vr_embed_stagger_axe_speed);
    float how = point ? cvar_hget(cvarh_vr_embed_stagger_sword) : cvar_hget(cvarh_vr_embed_stagger_axe);
    float skewer = VR_Embed_Skewer(m);
    float alive = m.health > 0 && !m.vr_knockdown && !m.vr_corpse && !m.vr_corpse_dying && (m.flags & FL_MONSTER) && m.solid != SOLID_BSP;
    if(!alive || skewer <= 0 || xSpeed < least || how <= 0)
    {
        VR_Embed_Say(sprintf("no stagger of %s: %.1f m/s of %.1f, skewer %.2f, alive %g", m.classname, xSpeed, least, skewer, alive));
        return;
    }
    float low = max(0, min(1, cvar_hget(cvarh_vr_embed_stagger_shallow)));
    float deep = low + (1 - low) * VR_Embed_DepthShare(e, xSpeed);
    float hands = e.vr_hold_hands == 3 ? max(1, cvar_hget(cvarh_vr_embed_stagger_twohand)) : 1;
    float hold = how * skewer * deep * hands;
    float amount = max(0, cvar_hget(cvarh_vr_embed_stagger_amount)) * skewer * deep * hands;
    float ragdoll = m.vr_kd_chance_h && cvar_hget(cvarh_vr_knockdown) && canragdoll(m) && ragdollcanstagger(m);
    float did = FALSE;
    vr_embed_last_hold = hold;
    vr_embed_last_amount = amount;
    if(ragdoll)
    {
        vector flat = xDir;
        flat_z = 0;
        float sure = amount < cvar_hget(cvarh_vr_embed_stagger_topple);
        vr_kd_stagger_recover = hold;
        vr_kd_stagger_sure = sure;
        did = VR_Knockdown_Stagger(m, normalize(flat), max(0, cvar_hget(cvarh_vr_embed_stagger_push)) * min(2, amount), sure ? 1 : max(0, 1 - amount));
        vr_kd_stagger_recover = 0;
        vr_kd_stagger_sure = FALSE;
    }
    else
    {
        m.nextthink = max(m.nextthink, time + hold);
    }
    string text = strcat(sprintf("%s %s %s, %.1f m/s", e.netname, ragdoll ? (did ? "staggers" : "can't stagger") : "stops", m.classname, xSpeed),
        sprintf(": %.2f s, amount %.2f (skewer %.2f, %g hands)", hold, amount, skewer, e.vr_hold_hands == 3 ? 2 : 1));
    VR_Embed_Say(text);
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: %s\n", text));
    }
};

// A ragdoll of the body `h` (staggering, knocked down, dead) held by the limb the blade went into, by a spring from the hand on its
// handle, to where the hand is less `xAxial`'s worth along the blade: that's the blade's own. TRUE: it is. FALSE: it stands, or won't.
float(entity e, entity p, entity h, float xIn, float xAxial) VR_Embed_Spring =
{
    float skewer = VR_Embed_Skewer(h);
    if(skewer <= 0)
    {
        return FALSE;
    }
    float hand = (e.vr_hold_handle ? e.vr_hold_handle : e.vr_hold_hands) == 2 ? cVR_MainHand : cVR_OffHand;
    float force = cvar_hget(cvarh_vr_embed_move_axe_force);
    if(e.vr_stick_embed == AXESTICK_POINT)
    {
        float a = cvar_hget(cvarh_vr_embed_move_sword_force);
        force = a + (cvar_hget(cvarh_vr_embed_move_sword_force_deep) - a) * xIn;
    }
    // (At least so many times its weight, by how deep: a body skewered on a sword held up over your head, his note 2026-11-05.)
    float lift = cvar_hget(cvarh_vr_embed_move_axe_lift);
    if(e.vr_stick_embed == AXESTICK_POINT)
    {
        lift = cvar_hget(cvarh_vr_embed_move_sword_lift) + (cvar_hget(cvarh_vr_embed_move_sword_lift_deep) - cvar_hget(cvarh_vr_embed_move_sword_lift)) * xIn;
    }
    // (Its weight under the physics' own gravity: sv_gravity in m/s^2, about 3 times Earth's, not 9.81.)
    float kg = ragdolltug(h, p, hand, 15);
    // (The force on top of the lift: as the more of the two, the lift's 2.5 weights, 6100 N for a grunt, hid any Force under it.
    // 2026-11-08.)
    force = max(0, force) + max(0, lift) * kg * cvar_hget(cvarh_sv_gravity) / VRMetersToUnits();
    force = force * skewer * (e.vr_hold_hands == 3 ? max(1, cvar_hget(cvarh_vr_embed_move_twohand)) : 1);
    force = force * max(0, cvar_hget(cvarh_vr_embed_hold_strength)); // (Blade Hold Strength: the player's own, all of it)
    if(cvar_hget(cvarh_vr_debug_axestick) >= 3)
    {
        dprint(sprintf("embedspring: %s pull %.0f N now, %.0f N at most (%.0f kg), axial %.1f + %.1f\n", e.netname, ragdolltug(h, p, hand, 1), force, kg, e.vr_hold_axial, xAxial));
    }
    float now = ragdolltug(h, p, hand, 1);
    e.vr_hold_strain = (now >= 0 && force > 0) ? now / force : 0;
    if(now >= 0)
    {
        e.vr_hold_axial = e.vr_hold_axial + xAxial;
        ragdolltug(h, p, hand, 14, e.vr_stick_dir * -e.vr_hold_axial);
        return ragdolltug(h, p, hand, 13, '0 0 0', 0, force) > 0;
    }
    e.vr_hold_axial = 0;
    return ragdolltug(h, p, hand, 12, e.vr_stick_at, 0, force) > 0;
};

// A crate or barrel (VR_Embed_InProp) held by the blade `e`: its body pulled at the wound towards where the hand on the handle has it (the
// wound's place from that hand as it took hold), less `xAxial`'s worth along the blade (the hand's way out, and in while the blade can go
// deeper: that is the blade sliding, as a body's VR_Embed_Spring), at vr_embed_prop_force N at most (Moving: Two Hands with both, Blade Hold
// Strength). TRUE: it is (FALSE: no body to pull, the hold goes as a standing body's).
float(entity e, entity p, entity h, float xAxial, float xDt) VR_Embed_PropSpring =
{
    float hand = (e.vr_hold_handle ? e.vr_hold_handle : e.vr_hold_hands) == 2 ? cVR_MainHand : cVR_OffHand;
    vector at = VRGetEntHandPos(p, hand);
    if(!e.vr_hold_propon)
    {
        e.vr_hold_propon = TRUE;
        e.vr_hold_propoff = e.vr_stick_at - at;
        e.vr_hold_axial = 0;
        e.vr_hold_proptgt = e.vr_stick_at;
    }
    e.vr_hold_axial = e.vr_hold_axial + xAxial;
    vector target = at + e.vr_hold_propoff - e.vr_stick_dir * e.vr_hold_axial;
    vector going = xDt > 0.0005 ? (target - e.vr_hold_proptgt) * (1 / xDt) : '0 0 0';
    if(vlen(going) > VR_EMBED_JUMP * VRMetersToUnits())
    {
        going = '0 0 0'; // (a jump of the hand: no throw)
    }
    e.vr_hold_proptgt = target;
    float by = (e.vr_hold_hands == 3 ? max(1, cvar_hget(cvarh_vr_embed_move_twohand)) : 1) * max(0, cvar_hget(cvarh_vr_embed_hold_strength));
    float force = max(0, cvar_hget(cvarh_vr_embed_prop_force)) * by;
    float pull = embedproppull(h, e.vr_stick_at, target, going, force, cvar_hget(cvarh_vr_embed_prop_hertz), max(0, cvar_hget(cvarh_vr_embed_prop_lift)) * by);
    e.vr_hold_proppull = max(0, pull);
    e.vr_hold_strain = (pull >= 0 && force > 0) ? pull / force : 0;
    if(cvar_hget(cvarh_vr_debug_axestick) >= 3)
    {
        dprint(sprintf("embedprop: %s pulls %s %.0f N of %.0f, the wound %.1f units off its place\n", e.netname, h.classname, pull, force, vlen(target - e.vr_stick_at)));
    }
    return pull >= 0;
};

// A body standing, light enough, its holder's walk `xWalk` this frame: going away from it, past the slack, it comes along, as a monster steps
// (no further off a ledge than it would walk; blocked, the hand gets `far` and the blade comes out).
void(entity e, entity h, vector xHand, float xFar, vector xWalk) VR_Embed_Drag =
{
    if(h.health <= 0 || h.vr_knockdown || !(h.flags & FL_MONSTER) || !(h.flags & FL_ONGROUND) ||
       xFar <= max(0, cvar_hget(cvarh_vr_embed_drag_slack)) * 0.01 * VRMetersToUnits() || !VR_Embed_Light(h))
    {
        return;
    }
    vector way = xHand - e.vr_stick_at;
    way_z = 0;
    way = normalize(way);
    xWalk_z = 0;
    float pull = xWalk * way;
    if(pull <= 0)
    {
        return;
    }
    entity was = self;
    self = h;
    float yaw = vectoyaw(way);
    if(!walkmove(yaw, pull))
    {
        walkmove(yaw, pull * 0.5);
    }
    self = was;
};

// What your walking speed is times (VR_HeavyMelee_Frame's .vr_heavy_move): a blade you hold in a living body ties you to it, however
// heavy, however it stands. A dead body only once you are dragging it (.vr_hold_tether): holding on with it at hand doesn't.
float(entity p) VR_Embed_TetherShare =
{
    entity o = p.vr_embed_off;
    entity n = p.vr_embed_main;
    if((o != world && (o.vr_stick_ent.health > 0 || o.vr_hold_tether)) || (n != world && (n.vr_stick_ent.health > 0 || n.vr_hold_tether)))
    {
        return max(0.05, min(1, cvar_hget(cvarh_vr_embed_tether_speed)));
    }
    return 1;
};

float vr_embed_slidesounds; // (for the test: how many so far)
float vr_embed_slidebloods; // (and how many spurts)

// The blade `e` slid `xMoved` units: every so far, in or out, the flesh clip of that way, quieter, and blood spurting out of
// the wound (as VR_AxeStick_PulledOut's), in a live body or a corpse.
void(entity e, float xMoved, float xCm) VR_Embed_SlideFlesh =
{
    float every = cvar_hget(cvarh_vr_embed_slide_sound_cm) * xCm;
    if(every <= 0 || xMoved == 0)
    {
        return;
    }
    e.vr_hold_sndgo = (e.vr_hold_sndgo * xMoved < 0 ? 0 : e.vr_hold_sndgo) + xMoved; // (a new way: its own distance)
    if(fabs(e.vr_hold_sndgo) >= every)
    {
        // (In a crate: wood's creak, not flesh.)
        string snd = xMoved > 0 ? "vr/blade_flesh_in.wav" : "vr/blade_flesh_out.wav";
        if(VR_Embed_InProp(e))
        {
            snd = "vr/phys/wood_m1.wav";
        }
        VR_SoundVaried(e, CHAN_AUTO, snd, max(0, min(1, cvar_hget(cvarh_vr_embed_slide_sound_vol))), ATTN_NORM);
        e.vr_hold_sndgo = 0;
        vr_embed_slidesounds = vr_embed_slidesounds + 1;
        float blood = cvar_hget(cvarh_vr_embed_slide_blood);
        if(blood > 0 && VR_AxeStick_InMonster(e))
        {
            SpawnBlood(e.vr_stick_at, e.vr_stick_dir * -120, blood);
            vr_embed_slidebloods = vr_embed_slidebloods + 1;
        }
    }
};

// Running into a stab you hold (his note 2026-11-08 17:17, "I very often overshoot my run"): the blade, which can't go further in,
// pushes back. Your speed along its way into the body (level) eases down to vr_embed_brake_speed at vr_embed_brake per second, so you
// stay before the body. Any body the blade is in, living or one the stab killed; your speed off that way is left alone.
float vr_embed_braked; // (for the test: m/s taken off your speed by the last brake)
void(entity e, entity p, entity h) VR_Embed_Brake =
{
    float rate = cvar_hget(cvarh_vr_embed_brake);
    vector way = e.vr_stick_dir;
    way_z = 0;
    if(rate <= 0 || !(h.flags & FL_MONSTER) || vlen(way) < 0.1)
    {
        return;
    }
    way = normalize(way);
    float along = p.velocity * way;
    float most = max(0, cvar_hget(cvarh_vr_embed_brake_speed)) * VRMetersToUnits();
    if(along <= most)
    {
        return;
    }
    float now = most + (along - most) * max(0, 1 - rate * frametime);
    p.velocity = p.velocity - way * (along - now);
    vr_embed_braked = (along - now) / VRMetersToUnits();
};

// The hold, each think of a blade held (self). TRUE: it is gone (in the hand).
float() VR_Embed_HoldThink =
{
    entity e = self;
    entity p = e.vr_hold_player;
    entity h = e.vr_stick_ent;
    if(!cvar_hget(cvarh_vr_embed))
    {
        VR_Embed_Free(e, "vr_embed off");
        return TRUE;
    }
    if(p.classname != "player" || p.health <= 0)
    {
        VR_Embed_DropHands(e, "its holder is dead or gone"); // (it stays in the body)
        return FALSE;
    }
    VR_Embed_HandFrame(e, p, cVR_OffHand);
    VR_Embed_HandFrame(e, p, cVR_MainHand);
    // A hand drawn on it that its arm, stretched as far as it may (vr_embed_arm_stretch), no longer reaches lets go (the engine's
    // avatar measures it: .vr_embed_armgap, x the off hand, y the main; his note 2026-11-05).
    // (Past the least it has been since the hand took hold: a hand already off its arm as it did, by the avatar's own reach, isn't the blade's.)
    // (Counted once the drawn hand has gone onto the blade and the stretch has eased in: as it takes hold, the avatar's last frame still
    // has the hand where its controller was, in reach, and a second hand drawn onto its grip hotspot may then be out of it.)
    float letgo = max(0, cvar_hget(cvarh_vr_embed_arm_letgo));
    float settle = max(cvar_hget(cvarh_vr_embed_arm_stretch_ease), cvar_hget(cvarh_vr_embed_free_ease)) + 0.1;
    if(time >= e.vr_hold_gapat_x + settle)
    {
        e.vr_hold_gapoff = min(e.vr_hold_gapoff, p.vr_embed_armgap_x);
    }
    if(time >= e.vr_hold_gapat_y + settle)
    {
        e.vr_hold_gapmain = min(e.vr_hold_gapmain, p.vr_embed_armgap_y);
    }
    // (Only an arm that reached the hand on it at some point: the mock hands of the tests are held further than any arm.)
    if((e.vr_hold_hands & 1) && e.vr_hold_gapoff <= letgo && p.vr_embed_armgap_x > e.vr_hold_gapoff + letgo)
    {
        VR_Embed_Say(VR_Embed_Why("the arm can't reach the hand on it: it lets go", p.vr_embed_armgap_x - e.vr_hold_gapoff, letgo, "cm"));
        VR_Embed_DropHand(e, cVR_OffHand, sprintf("out of the arm's reach (%.1f cm, from %.1f)", p.vr_embed_armgap_x, e.vr_hold_gapoff));
    }
    if((e.vr_hold_hands & 2) && e.vr_hold_gapmain <= letgo && p.vr_embed_armgap_y > e.vr_hold_gapmain + letgo)
    {
        VR_Embed_Say(VR_Embed_Why("the arm can't reach the hand on it: it lets go", p.vr_embed_armgap_y - e.vr_hold_gapmain, letgo, "cm"));
        VR_Embed_DropHand(e, cVR_MainHand, sprintf("out of the arm's reach (%.1f cm, from %.1f)", p.vr_embed_armgap_y, e.vr_hold_gapmain));
    }
    if(!e.vr_hold_hands)
    {
        e.vr_stick_shake = 0;
        return FALSE;
    }
    // (The hand's own time, what its pull and the resistance to it go by: bullet time's hands move at their real speed, 3 times as far
    // a game second at 0.3. By the game's seconds a blade would hold 3 times as hard, and a real pull of 3 m/s read as a jump.)
    float dt = max(0, time - e.vr_hold_last) * (vr_hand_time_scale > 0 ? 1 / vr_hand_time_scale : 1);
    e.vr_hold_last = time;
    float cm = 0.01 * VRMetersToUnits(); // units a cm
    vector dir = e.vr_stick_dir;
    VR_Embed_Brake(e, p, h);

    // Going on to its depth, after the stab struck: the blow's speed takes it, the hand drawn with it. Its place from there is
    // where the hand's is from the blade as it gets there.
    if(e.vr_stick_goal > 0)
    {
        float toGo = e.vr_stick_goal - (e.vr_stick_depth + e.vr_stick_slide);
        float step = e.vr_stick_goal / max(0.01, cvar_hget(cvarh_vr_embed_enter_time)) * dt;
        e.vr_stick_slide = e.vr_stick_slide + min(max(0, toGo), step);
        if(toGo <= step)
        {
            e.vr_stick_goal = 0;
            if(e.vr_hold_hands & 1)
            {
                VR_Embed_SetG0(e, p, cVR_OffHand);
            }
            if(e.vr_hold_hands & 2)
            {
                VR_Embed_SetG0(e, p, cVR_MainHand);
            }
        }
        return FALSE;
    }

    // The hand(s)' own motion this frame, along its axis and off it. (Not the blade's: a body flinching swings the hilt about by
    // more than a hand's pull, and that is no pull; too far from the hand, it comes free below.) What the body moved with the hand
    // (where it went in: it follows the hold, it is dragged) is no pull either; nor, off the axis, is your walking: that is `far`'s.
    vector moved3 = '0 0 0';
    float far = 0; // how much further the hand is from where it went in than it held it from
    float hands = 0;
    vector atHandNow = '0 0 0';
    if(e.vr_hold_hands & 1)
    {
        vector atOff = VRGetEntHandPos(p, cVR_OffHand);
        atHandNow = atOff;
        moved3 = moved3 + atOff - e.vr_hold_lastoff;
        far = far + vlen(atOff - e.vr_stick_at) - (e.vr_hold_distoff - e.vr_stick_slide);
        e.vr_hold_lastoff = atOff;
        hands = hands + 1;
    }
    if(e.vr_hold_hands & 2)
    {
        vector atMain = VRGetEntHandPos(p, cVR_MainHand);
        atHandNow = atMain;
        moved3 = moved3 + atMain - e.vr_hold_lastmain;
        far = far + vlen(atMain - e.vr_stick_at) - (e.vr_hold_distmain - e.vr_stick_slide);
        e.vr_hold_lastmain = atMain;
        hands = hands + 1;
    }
    moved3 = moved3 * (1 / hands);
    far = far / hands;
    float jump = VR_EMBED_JUMP * VRMetersToUnits() * max(dt, 0.001);
    vector walk = p.origin - e.vr_hold_lastorg;
    // (Standing, where it stands: the wound on its drawn model comes a tenth of a second after a monster's step.)
    // (A crate: where the wound itself went, as it turns and tips.)
    float inProp = VR_Embed_InProp(e);
    vector wound = (h.health > 0 && !h.vr_knockdown && !inProp) ? h.origin - e.vr_hold_lasthost : e.vr_stick_at - e.vr_hold_lastwound;
    e.vr_hold_lastorg = p.origin;
    e.vr_hold_lastwound = e.vr_stick_at;
    e.vr_hold_lasthost = h.origin;
    if(vlen(moved3) > jump)
    {
        moved3 = '0 0 0';
        walk = '0 0 0';
    }
    if(vlen(walk) > jump)
    {
        walk = '0 0 0';
    }
    if(vlen(wound) > jump)
    {
        wound = '0 0 0';
    }
    // The hand's pull out along it, averaged over the last moments (not the last frame alone): an axe's tug, a dead body's.
    float pullv = dt > 0 ? -(moved3 * dir) / dt / VRMetersToUnits() : 0;
    float tw = max(0.01, cvar_hget(cvarh_vr_embed_hold_axe_tug_time));
    e.vr_hold_tugv = e.vr_hold_tugv + (pullv - e.vr_hold_tugv) * min(1, dt / tw);
    // (A dead body gives it up to a firm tug of the arm, however it comes along: only your walking drags it. A slower pull carries the
    // body, along the blade too, and doesn't slide it out: a corpse skewered on a sword is lifted, one on an axe dragged; a body that won't
    // come, the hand slips off it, the blade staying in. His decision 2026-11-05, "like the axe".)
    float corpseTug = cvar_hget(e.vr_stick_embed == AXESTICK_POINT ? cvarh_vr_embed_hold_corpse_tug_sword : cvarh_vr_embed_hold_corpse_tug_axe);
    // (Blade Hold Strength over x1, the blade grips harder: a dead body's tug that much faster, and a living one's ragdoll, staggering
    // or down, is carried as a dead one by a pull slower than the tug times what is over x1 (at x1, none: a staggering body stays as
    // the blade comes out, test 11 case 23). Without, a lift's share along the blade never moved it, at any strength: his x10 "still
    // extremely heavy", 2026-11-10.)
    float gripBy = max(0, cvar_hget(cvarh_vr_embed_hold_strength));
    if(h.health > 0)
    {
        corpseTug = (h.vr_knockdown && VR_Embed_Skewer(h) > 0) ? corpseTug * max(0, gripBy - 1) : 0;
    }
    else
    {
        corpseTug = corpseTug * max(1, gripBy);
    }
    float carry = corpseTug > 0 && e.vr_hold_tugv < corpseTug;
    float tugged = (h.health <= 0 && !carry) ? (moved3 - walk) * dir : 0;
    float stepAlong = moved3 * dir - tugged;
    float woundAlong = wound * dir;
    stepAlong = tugged + stepAlong - (stepAlong > 0 ? max(0, min(stepAlong, woundAlong)) : min(0, max(stepAlong, woundAlong)));
    vector arm = moved3 - walk;
    arm = arm - dir * (arm * dir);
    float armLen = vlen(arm);
    if(armLen > 0.001)
    {
        arm = arm * ((armLen - max(0, min(armLen, wound * arm * (1 / armLen)))) / armLen);
    }
    e.vr_hold_err = e.vr_hold_err + stepAlong;
    // What is left of the gap: it closes as the hand comes back, and is gone once the hand has met the drawn one. Only past it, it drives.
    if(e.vr_hold_err * e.vr_hold_slack <= 0)
    {
        e.vr_hold_slack = 0;
    }
    else if(fabs(e.vr_hold_err) < fabs(e.vr_hold_slack))
    {
        e.vr_hold_slack = e.vr_hold_err;
    }
    float along = e.vr_hold_err - e.vr_hold_slack;
    vector offNow = e.vr_hold_off + arm;
    float offLen = vlen(offNow);
    e.vr_hold_offslack = min(e.vr_hold_offslack, offLen);
    float off = (offLen - e.vr_hold_offslack) * max(0, 1 - VR_EMBED_OFF_RELAX * dt); // (the slow drift forgiven: past the gap only)
    e.vr_hold_off = offLen > 0.001 ? offNow * ((e.vr_hold_offslack + off) / offLen) : '0 0 0';
    if(cvar_hget(cvarh_vr_debug_axestick) >= 3)
    {
        dprint(sprintf("embedhold: %s along %.1f off %.1f units, %.1f further, %.1f in; this frame the hand %.1f, it %.1f along it\n", e.netname, along, off, far, e.vr_stick_depth + e.vr_stick_slide, moved3 * dir, wound * dir));
    }
    // Moving the body, by the limb it is in, what is along the blade left out while it can slide in or out (how far it goes: a sword up to
    // its Most In, an axe no deeper than it went).
    float point = e.vr_stick_embed == AXESTICK_POINT;
    float inShare = VR_Embed_InShare(e);
    float deepest = point ? max(1, e.vr_stick_bladelen) * max(0, cvar_hget(cvarh_vr_embed_hold_sword_most)) * 0.01 - e.vr_stick_depth : 0;
    float armAlong = (moved3 - walk) * dir;
    // (A dead body carried by a slow pull out: the spring takes the hand's way along the blade as well. A firm tug's first moments, its
    // average still under the Tug Speed, carried it: once it is a tug, that way, the last moments' (a leaky sum over the Tug Time), is
    // the blade's after all, as a tug's from its start.)
    float carriedNow = 0;
    e.vr_hold_carried = e.vr_hold_carried * max(0, 1 - dt / tw);
    if(carry && armAlong < 0)
    {
        e.vr_hold_carried = e.vr_hold_carried + armAlong;
    }
    if(!carry && e.vr_hold_carrying)
    {
        carriedNow = e.vr_hold_carried;
        e.vr_hold_err = e.vr_hold_err + carriedNow;
        along = e.vr_hold_err - e.vr_hold_slack;
    }
    if(!carry)
    {
        e.vr_hold_carried = 0;
    }
    e.vr_hold_carrying = carry;
    float axialNow = (carry && armAlong < 0) ? 0 : (((armAlong < 0 || e.vr_stick_slide < deepest - 0.01) ? armAlong : 0) + carriedNow);
    float sprung = inProp ? VR_Embed_PropSpring(e, p, h, axialNow, dt) : VR_Embed_Spring(e, p, h, inShare, axialNow);
    // Off the axis: a ragdoll comes along; only a yank tears it out of one. What stands can't: the hand's gone too far.
    float across = armLen / max(0.001, dt) / VRMetersToUnits();
    if(off > max(0, cvar_hget(cvarh_vr_embed_hold_break)) * cm && (!sprung || across >= cvar_hget(cvarh_vr_embed_hold_yank)))
    {
        VR_Embed_Free(e, VR_Embed_Why(sprung ? "yanked off its axis" : "off its axis", off / cm, cvar_hget(cvarh_vr_embed_hold_break), "cm"));
        return TRUE;
    }
    // (A body dying or knocked down falls away from the hand: the hold goes on, further.)
    e.vr_hold_far = far / cm;
    float reachBy = h.health <= 0 ? max(1, cvar_hget(cvarh_vr_embed_hold_reach_corpse)) :
                    ((h.vr_knockdown && !h.vr_kd_stagger) ? max(1, cvar_hget(cvarh_vr_embed_hold_reach_down)) : 1);
    e.vr_hold_farmost = max(0, cvar_hget(cvarh_vr_embed_hold_reach)) * reachBy;
    if(far > max(0, cvar_hget(cvarh_vr_embed_hold_reach)) * cm * reachBy)
    {
        VR_Embed_Free(e, VR_Embed_Why("too far from the hand", far / cm, cvar_hget(cvarh_vr_embed_hold_reach) * reachBy, "cm"));
        return TRUE;
    }

    // The resistance: a stick, then a slide at most so fast, both by how deep (a dead body: less).
    float corpse = h.health <= 0;

    // Standing, dragged along as you walk.
    if(!sprung)
    {
        VR_Embed_Drag(e, h, atHandNow, far, walk);
    }
    // (A dead body counts while you walk away from it, past a little: it fell at your hand, and holding on with it there is no tether.)
    float tetherAt = max(0, cvar_hget(cvarh_vr_embed_tether_far)) * cm;
    vector awayFrom = atHandNow - e.vr_stick_at;
    awayFrom_z = 0;
    if(!corpse || far <= tetherAt * 0.5 || walk * normalize(awayFrom) <= 0)
    {
        e.vr_hold_tether = FALSE;
    }
    else if(far > tetherAt)
    {
        e.vr_hold_tether = TRUE;
    }
    float stick = VR_Embed_Stick(e, inShare, corpse) * cm;
    float rate = VR_Embed_Rate(e, inShare, corpse) * cm;
    if(inProp)
    {
        // (Wood: harder to slide than flesh, slower.)
        float woodBy = max(0.01, cvar_hget(cvarh_vr_embed_prop_stick));
        stick = stick * woodBy;
        rate = rate / woodBy;
    }
    e.vr_hold_stick = stick / cm;
    float over = fabs(along) - stick;
    // An axe's tug: barely in, it stays until the hand has pulled on past its depth by this.
    float tug = point ? 0 : max(0, cvar_hget(cvarh_vr_embed_hold_axe_tug)) * cm;
    float least = tug > 0 ? min(0, 1 - e.vr_stick_depth) : -1000000;
    // A dead body carried by a slow pull: it doesn't slide out; a hand that gets so far ahead of a body that won't come slips off it.
    if(carry && along < 0)
    {
        // (While its spring holds it short of its most the body is coming: the hand's lead on the wound, the spring's give and the
        // wound's place easing after its limb, is no slip. Only at its most, the body held back, does the lead build.)
        if(sprung && e.vr_hold_strain < 0.95)
        {
            e.vr_hold_err = max(e.vr_hold_err, e.vr_hold_slack - stick);
            along = e.vr_hold_err - e.vr_hold_slack;
            over = fabs(along) - stick;
        }
        float slip = max(0, cvar_hget(cvarh_vr_embed_hold_corpse_slip)) * cm;
        if(over > slip)
        {
            VR_Embed_Say(VR_Embed_Why("pulled too slowly, the body won't come: the hand slips off", e.vr_hold_tugv, corpseTug, "m/s"));
            VR_Embed_DropHands(e, sprintf("the body won't come (%.1f cm ahead, %.2f of %.2f m/s)", over / cm, e.vr_hold_tugv, corpseTug));
            return FALSE;
        }
        over = 0;
    }
    if(over > 0)
    {
        float move = min(over * max(0, cvar_hget(cvarh_vr_embed_hold_follow)), rate) * dt * (along > 0 ? 1 : -1);
        float was = e.vr_stick_slide;
        e.vr_stick_slide = max(least, min(deepest, e.vr_stick_slide + move));
        float moved = e.vr_stick_slide - was;
        e.vr_hold_err = e.vr_hold_err - moved;
        // (Stopped by how far it goes, the hand past it is a gap as any other: coming back to the drawn hand does nothing.)
        VR_Embed_SlideFlesh(e, moved, cm);
        if(moved > 0 && cvar_hget(cvarh_vr_embed_hold_slide_damage) > 0 && h.takedamage && h.health > 0)
        {
            e.vr_hold_owed = e.vr_hold_owed + moved / (10 * cm) * cvar_hget(cvarh_vr_embed_hold_slide_damage);
            if(e.vr_hold_owed >= 1)
            {
                float dmg = floor(e.vr_hold_owed);
                e.vr_hold_owed = e.vr_hold_owed - dmg;
                T_Damage(h, e, p, dmg);
            }
        }
        if(fabs(moved) > 0.001 && time >= e.vr_hold_buzz)
        {
            e.vr_hold_buzz = time + 0.06;
            float amp = 0.25 + 0.35 * inShare;
            if(e.vr_hold_hands & 1)
            {
                VR_Embed_Haptic(p, cVR_OffHand, 0.05, 45, amp);
            }
            if(e.vr_hold_hands & 2)
            {
                VR_Embed_Haptic(p, cVR_MainHand, 0.05, 45, amp);
            }
        }
    }

    // Out only once all of it has travelled out, any blade, any body: the hand's pull and the blade's way are one to one. An axe's: and tugged,
    // hard enough: the pull out's speed over the last moments (not the last frame alone), else the hand comes off it and it stays (his note
    // 2026-11-05). Nearing it, the axe and the hand on it shake (the engine's place: .vr_stick_shake) and the buzz rises.
    e.vr_stick_shake = 0;
    if(tug > 0)
    {
        float tugged2 = -(e.vr_hold_err - e.vr_hold_slack) - stick;
        if(e.vr_stick_slide <= least + 0.01 && tugged2 > 0)
        {
            float from = max(0, min(0.99, cvar_hget(cvarh_vr_embed_tug_shake_from)));
            e.vr_stick_shake = max(0, min(1, (tugged2 / tug - from) / (1 - from)));
            if(tugged2 >= tug)
            {
                float needs = cvar_hget(cvarh_vr_embed_hold_axe_tug_speed);
                if(needs > 0 && e.vr_hold_tugv < needs)
                {
                    e.vr_stick_shake = 0;
                    VR_Embed_Say(VR_Embed_Why("tug too gentle: the hand comes off", e.vr_hold_tugv, needs, "m/s"));
                    VR_Embed_DropHands(e, sprintf("tugged too gently (%.2f of %.2f m/s)", e.vr_hold_tugv, needs));
                    return FALSE;
                }
                e.vr_stick_shake = 0;
                VR_Embed_Free(e, VR_Embed_Why("tugged out", tugged2 / cm, tug / cm, "cm"));
                return TRUE;
            }
            if(time >= e.vr_hold_buzz)
            {
                e.vr_hold_buzz = time + 0.05;
                float near = tugged2 / tug;
                VR_Embed_Haptic(p, (e.vr_hold_hands & 2) ? cVR_MainHand : cVR_OffHand, 0.05, 60 + 120 * near, 0.25 + 0.65 * near * near);
            }
        }
    }
    else if(e.vr_stick_depth + e.vr_stick_slide <= 0)
    {
        VR_Embed_Free(e, VR_Embed_Why("pulled out: slid", -e.vr_stick_slide / cm, e.vr_stick_depth / cm, "cm"));
        return TRUE;
    }
    return FALSE;
};

// Each PlayerPreThink (VR_Hand_Derive): the hands' blades, what the engine sends the client.
void(entity p) VR_Embed_Derive =
{
    p.vr_embed_off = VR_Hand_Kind(p, cVR_OffHand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(p, cVR_OffHand) : world;
    p.vr_embed_main = VR_Hand_Kind(p, cVR_MainHand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(p, cVR_MainHand) : world;
    p.vr_embed_handle = (p.vr_embed_off.vr_hold_handle & 1) + (p.vr_embed_main.vr_hold_handle & 2);
};

// Whether a blow of a pointed blade is a stab that goes in, tip first (the blade's line in mh_grip, mh_far; xAt, xDir, xSpeed, xKind as
// VR_Embed_Blow's; `xLeast` m/s the least): string_null, or why not. A body's (VR_Embed_Blow), a gib's (VR_Skewer_Blow).
string(entity b, vector xAt, vector xDir, float xSpeed, float xLeast, float xKind) VR_Embed_StabWhy =
{
    vector line = b.mh_far - b.mh_grip;
    float len = vlen(line);
    vector axis = len > 0.01 ? line * (1 / len) : '0 0 0';
    float along = normalize(xDir) * axis;
    // (What strikes: the blow's point; a corpse's has none, where it met it.)
    float tip = xKind >= 0 ? 1 - b.mh_c_f : (len > 0.01 ? vlen(b.mh_far - xAt) / len : 1);
    float deg = atan2(sqrt(max(0, 1 - along * along)), along) * (180 / 3.14159265);
    if(xKind >= 0 && xKind != VR_MKIND_STAB)
    {
        return sprintf("the blow read as no stab (kind %g, not %g)", xKind, VR_MKIND_STAB);
    }
    if(xSpeed < xLeast)
    {
        return sprintf("too slow: %.1f, least %.1f m/s", xSpeed, xLeast);
    }
    if(along < cos(cvar_hget(cvarh_vr_embed_stab_angle) * (3.14159265 / 180)))
    {
        return sprintf("off its line: %.0f, most %.0f deg", deg, cvar_hget(cvarh_vr_embed_stab_angle));
    }
    if(tip > cvar_hget(cvarh_vr_embed_stab_tip) * 0.01)
    {
        return sprintf("not by its tip: %.0f, most %.0f%% of the blade", tip * 100, cvar_hget(cvarh_vr_embed_stab_tip));
    }
    return string_null;
};

// A blow of `xHand` being dealt on `m` at `xAt` going along `xDir` (self is the player; xSpeed m/s, the hand's, as the blow's own;
// xKind its VR_MKIND_*, -1 on a corpse whose blow has none: its line says): whether it may leave the blade in. For VR_Embed_BlowsDone.
void(float xHand, entity b, entity m, vector xAt, vector xDir, float xSpeed, float xKind) VR_Embed_Blow =
{
    if(!cvar_hget(cvarh_vr_embed) || vr_embed_host != world || m == world || m.vr_gib || !self.ishuman ||
       !((m.flags & FL_MONSTER) || VR_Melee_CorpseTarget(m) || VR_Embed_PropHost(m)))
    {
        return;
    }
    float wid = VRGetEntWeapon(self, xHand);
    float flags = VRGetEntWeaponFlags(self, xHand);
    if(VRIsHandWeaponless(self, xHand) || (flags & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        return;
    }
    float how = weaponvalue(WeaponIdToModel(wid, flags), "embed");
    if(how != AXESTICK_POINT && how != AXESTICK_EDGE)
    {
        return;
    }
    vector line = b.mh_far - b.mh_grip;
    float len = vlen(line);
    vector axis = len > 0.01 ? line * (1 / len) : '0 0 0';
    float along = normalize(xDir) * axis;
    string why = string_null;
    if(how == AXESTICK_POINT)
    {
        why = VR_Embed_StabWhy(b, xAt, xDir, xSpeed, cvar_hget(cvarh_vr_embed_stab_speed), xKind);
    }
    else
    {
        if(xKind >= 0 && xKind != VR_MKIND_SLASH)
        {
            why = sprintf("the blow read as no swing (kind %g, not %g)", xKind, VR_MKIND_SLASH);
        }
        else if(xSpeed < cvar_hget(cvarh_vr_embed_swing_speed))
        {
            why = sprintf("too slow: %.1f, least %.1f m/s", xSpeed, cvar_hget(cvarh_vr_embed_swing_speed));
        }
        // (Edge first, not its side nor along the handle: the engine, embedstick.)
    }
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: blow of hand %g at %s, kind %g, %.1f m/s, along %.2f, point %.2f: %s\n", xHand, m.classname, xKind, xSpeed, along, b.mh_c_f,
            why == string_null ? "may stay in" : why));
    }
    if(cvar_hget(cvarh_vr_debug_embed))
    {
        embedsay(why == string_null ? sprintf("blow at %s may stay in: %.1f m/s (your body %.1f), along %.2f, point %.2f", m.classname, xSpeed, b.mh_bodyspeed, along, b.mh_c_f) : sprintf("blow at %s NOT STUCK: %s (your body %.1f m/s)", m.classname, why, b.mh_bodyspeed));
    }
    if(why != string_null)
    {
        return;
    }
    vr_embed_host = m;
    vr_embed_hostmodel = m.modelindex;
    vr_embed_at = xAt;
    vr_embed_vel = normalize(xDir) * xSpeed * VRMetersToUnits();
};

// The hand's weapon the blade stuck in `m` now, if it is still there and the blade touches it, the hand on it (and the other hand, if it
// helps hold it): 1 it is; 0 it won't; -1 not yet: the blade as drawn is `vr_embed_gap` units off its drawn model.
float(float xHand, entity m, float xModel, vector xAt, vector xVel) VR_Embed_Try =
{
    if(m.vr_gib || m.modelindex != xModel || m.model == string_null || VRIsHandWeaponless(self, xHand))
    {
        VR_Embed_Say(sprintf("NOT STUCK: %s", VRIsHandWeaponless(self, xHand) ? "the hand holds nothing now" : "the blow gibbed it or took its model"));
        return 0;
    }
    if(VR_MaterialOf(m) == VR_MAT_WOOD && !VR_Embed_PropHost(m))
    {
        VR_Embed_Say("NOT STUCK: the blow broke the crate, or it was taken up"); // (a crate's blow is dealt before: VR_Crate_Break)
        return 0;
    }
    // A blow that cut off a head or a limb went through it: not in the body (his note 2026-11-06: left where the part was, in nothing,
    // the body moved by it). Nor where it would go in is a part cut off before. It stays in the hand.
    if(m.vr_stick_cutat > 0 && time - m.vr_stick_cutat <= max(0, cvar_hget(cvarh_vr_embed_touch_wait)) + 0.05)
    {
        VR_Embed_Say("NOT STUCK: the blow cut a part off, it went through");
        return 0;
    }
    if(VR_AxeStick_BoneCut(m, ragdolllimb(m, xAt)))
    {
        VR_Embed_Say(sprintf("NOT STUCK: the part it went into is cut off (bone %g)", ragdolllimb(m, xAt)));
        return 0;
    }
    float wid = VRGetEntWeapon(self, xHand);
    float flags = VRGetEntWeaponFlags(self, xHand);
    entity inst = VRGetEntWeaponInst(self, xHand);
    float other = VRGetOtherHand(xHand);
    float helping = VR_HandGrabUtil_IsHandGrabbing(self, other) && VR_Hand_Free(self, other, VR_HANDFREE_GUNGRIP | VR_HANDFREE_LOCKED) &&
                    VRIsHandWeaponless(self, other) && (VRGet2HAiming(self) || weaponanygrip(self, other) != 0);
    entity w = CreateThrownWeapon(self, wid, flags - (flags & QVR_WPNFLAG_SAW_STATE), inst, VRGetHandPos(xHand), VRGetHandRot(xHand),
        '0 0 0', '0 0 0', FALSE, FALSE, 0);
    if(w == world)
    {
        return 0;
    }
    w.vr_wpn_from = self; // (taken back: a regrip, not a pickup)
    w.throwhit = QVR_THROWHIT_HIT; // (its blow is dealt)
    w.throwdmgspeed = VR_Throw_DmgSpeed();
    vr_embed_stabbing = TRUE;
    float stuck = embedstick(w, m, xAt, xVel, xHand);
    vr_embed_stabbing = FALSE;
    if(stuck <= 0)
    {
        vr_embed_gap = w.vr_stick_depth;
        remove(w); // (the weapon's record is the hand's still)
        return stuck;
    }
    if(VR_AxeStick_BoneCut(m, w.vr_stick_bone))
    {
        // (Where the blade went in, not the blow's contact: as above.)
        VR_Embed_Say(sprintf("NOT STUCK: the part it went into is cut off (bone %g)", w.vr_stick_bone));
        remove(w);
        return 0;
    }
    VRSetHandEmpty(self, xHand);
    VRHandStopFiring(self, xHand);
    W_SetCurrentAmmoFor(xHand);
    VR_Embed_TakeHand(w, self, xHand);
    if(helping)
    {
        VR_Embed_TakeHand(w, self, other);
    }
    VR_Embed_Stagger(w, m, xVel, vlen(xVel) / VRMetersToUnits());
    return 1;
};

// The blows of `xHand` this frame dealt: one that may stay in stays in. Landed short (the sweep's width), it may yet, for a moment.
void(float xHand) VR_Embed_BlowsDone =
{
    entity m = vr_embed_host;
    vr_embed_host = world;
    if(m == world)
    {
        return;
    }
    if(VR_Embed_Try(xHand, m, vr_embed_hostmodel, vr_embed_at, vr_embed_vel) < 0)
    {
        float until = time + max(0, cvar_hget(cvarh_vr_embed_touch_wait));
        if(xHand == cVR_MainHand)
        {
            self.vr_embed_wait_main = m;
            self.vr_embed_waitto_main = until;
            self.vr_embed_waitat_main = vr_embed_at;
            self.vr_embed_waitvel_main = vr_embed_vel;
            self.vr_embed_waitmdl_main = vr_embed_hostmodel;
        }
        else
        {
            self.vr_embed_wait_off = m;
            self.vr_embed_waitto_off = until;
            self.vr_embed_waitat_off = vr_embed_at;
            self.vr_embed_waitvel_off = vr_embed_vel;
            self.vr_embed_waitmdl_off = vr_embed_hostmodel;
        }
    }
};

// Each frame of `xHand` (self the player): a blow that landed short of its body: the blade touches it now, or no longer may.
void(float xHand) VR_Embed_WaitFrame =
{
    float main = xHand == cVR_MainHand;
    entity m = main ? self.vr_embed_wait_main : self.vr_embed_wait_off;
    if(m == world)
    {
        return;
    }
    float r = cvar_hget(cvarh_vr_embed) ? VR_Embed_Try(xHand, m, main ? self.vr_embed_waitmdl_main : self.vr_embed_waitmdl_off, main ? self.vr_embed_waitat_main : self.vr_embed_waitat_off,
        main ? self.vr_embed_waitvel_main : self.vr_embed_waitvel_off) : 0;
    if(r < 0 && time < (main ? self.vr_embed_waitto_main : self.vr_embed_waitto_off))
    {
        return;
    }
    if(r < 0)
    {
        float cmOff = vr_embed_gap / (0.01 * VRMetersToUnits());
        VR_Embed_Say(sprintf("blow at %s NOT STUCK: the blade never touched its model: %.1f cm off, most %.1f", m.classname, cmOff, cvar_hget(cvarh_vr_embed_touch)));
        if(cvar_hget(cvarh_vr_debug_axestick))
        {
            dprint(sprintf("embedhold: blow at %s not stuck: the blade %.1f cm off its model\n", m.classname, cmOff));
        }
    }
    if(main)
    {
        self.vr_embed_wait_main = world;
    }
    else
    {
        self.vr_embed_wait_off = world;
    }
};

// The player leaves the level (SetChangeParms): his hands have their blades back, nothing stuck. No spurt to speak of: the level goes.
void(entity p) VR_Embed_LevelEnd =
{
    entity o = VR_Hand_Kind(p, cVR_OffHand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(p, cVR_OffHand) : world;
    entity n = VR_Hand_Kind(p, cVR_MainHand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(p, cVR_MainHand) : world;
    entity was = self;
    if(n != world)
    {
        VR_Embed_Free(n, "the level ends");
    }
    if(o != world && o != n)
    {
        VR_Embed_Free(o, "the level ends");
    }
    self = was;
    VR_Skewer_AllOff(p);
};
