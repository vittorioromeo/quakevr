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
// flinch swings a hilt on a long lever. For stage 3 (moving the body, stagger, the speed penalty): VR_Embed_HoldThink has them,
// `far` (how much further the hand is from the wound than it held it from) and the depth; everything about the hold is on the blade.
//
// Moving the body (ROUND23.md, "Embedded blades, stage 3"). A light body (VR_Embed_Light: Holding Enemies' one hand's hold on it, its
// monster group's Hold or by its size) struck hard staggers (VR_Embed_Stagger). Any ragdoll of it (staggering, knocked down, dead) is
// held by the limb the blade is in (VR_Embed_Spring: a spring from the hand, stronger the deeper; ragdolltug 12), so it follows the
// hand; what it follows is no pull on the blade (VR_Embed_HoldThink), what it doesn't slides it out, as ever. Standing, walking away
// drags it (VR_Embed_Drag). Past reach, it comes out. Heavy bodies and bosses have none of it. VR_Embed_TetherShare: your speed.

.entity vr_hold_player;   // who holds it (world: nobody)
.float  vr_hold_hands;    // the hands on it (bits: VR_Embed_Bit)
.float  vr_hold_handle;   // the hand on it by its handle (a bit; 0: none)
.float  vr_hold_distoff;  // the off hand's tracked place from where it went in, as it took hold (as if not slid)
.float  vr_hold_distmain; // the main hand's
.vector vr_hold_lastoff;  // the off hand's tracked place last frame
.vector vr_hold_lastmain; // the main hand's
.float  vr_hold_err;      // how far the hand(s) have moved along its axis that the blade hasn't yet (units)
.vector vr_hold_off;      // and off its axis (units)
.float  vr_hold_stoff;    // the off hand's grip's states towards opening (VR_Embed_HandOpen)
.float  vr_hold_stmain;
.float  vr_hold_last;     // the last frame's time
.float  vr_hold_pulled;   // units it has come out, in a dead body (vr_embed_tug_corpse)
.float  vr_hold_owed;     // sliding deeper's damage not dealt yet
.float  vr_hold_buzz;     // the next slide's buzz
.vector vr_hold_lastwound; // where it went in, last frame
.vector vr_hold_lastorg;  // where its holder stood, last frame
.vector vr_hold_lasthost; // where its body stood, last frame
.float  vr_hold_tether;   // in a dead body: it is being dragged (VR_Embed_TetherShare)
.float  vr_embed_quiet_off;  // on a player: the hand strikes nothing until then (it just came out)
.float  vr_embed_quiet_main;

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
    if(xHand == cVR_MainHand)
    {
        e.vr_hold_distmain = vlen(at - e.vr_stick_at) + e.vr_stick_slide;
        e.vr_hold_lastmain = at;
    }
    else
    {
        e.vr_hold_distoff = vlen(at - e.vr_stick_at) + e.vr_stick_slide;
        e.vr_hold_lastoff = at;
    }
    e.vr_hold_lastwound = e.vr_stick_at;
    e.vr_hold_lastorg = p.origin;
    e.vr_hold_lasthost = e.vr_stick_ent.origin;
    e.vr_hold_err = 0;
    e.vr_hold_off = '0 0 0';
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
    e.vr_hold_pulled = 0;
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
    if(!VR_AxeStick_IsStuck(e) || !cvar_hget(cvarh_vr_embed) || !VR_AxeStick_InMonster(e) || VR_AxeStick_Tugging(e) ||
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

// Whether a blade can move the body `m`: Holding Enemies' one hand's hold on it (its monster group's Hold in groups.txt, else by its
// size) is enough. Heavy ones and bosses (their group's Hold 0) never stagger, nor are moved or dragged by one.
float(entity m) VR_Embed_Light =
{
    if(m == world || (m.flags & (FL_FLY | FL_SWIM)) || (VR_IsDummy(m) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return FALSE;
    }
    return VR_FoeGrab_Strength(m, 0, 0) >= cvar_hget(cvarh_vr_embed_light_hold);
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

// The blow that left `e` in `m` went at `xSpeed` m/s: strong enough, a light living body staggers (a sword for longer than an axe,
// both longer the deeper), up again by itself. The usual stagger's: its AI stops, it stands on, sure footed.
void(entity e, entity m, vector xDir, float xSpeed) VR_Embed_Stagger =
{
    float point = e.vr_stick_embed == AXESTICK_POINT;
    float least = point ? cvar_hget(cvarh_vr_embed_stagger_sword_speed) : cvar_hget(cvarh_vr_embed_stagger_axe_speed);
    float how = point ? cvar_hget(cvarh_vr_embed_stagger_sword) : cvar_hget(cvarh_vr_embed_stagger_axe);
    float can = m.health > 0 && !m.vr_knockdown && !m.vr_corpse && !m.vr_corpse_dying && m.vr_kd_chance_h && (m.flags & FL_MONSTER) &&
                cvar_hget(cvarh_vr_knockdown) && m.solid != SOLID_BSP && VR_Embed_Light(m) && canragdoll(m) && ragdollcanstagger(m);
    if(can && xSpeed >= least && how > 0)
    {
        float low = max(0, min(1, cvar_hget(cvarh_vr_embed_stagger_shallow)));
        float hold = how * (low + (1 - low) * VR_Embed_DepthShare(e, xSpeed));
        xDir_z = 0;
        vr_kd_stagger_recover = hold;
        vr_kd_stagger_sure = TRUE;
        float did = VR_Knockdown_Stagger(m, normalize(xDir), max(0, cvar_hget(cvarh_vr_embed_stagger_push)), 1);
        vr_kd_stagger_recover = 0;
        vr_kd_stagger_sure = FALSE;
        if(cvar_hget(cvarh_vr_debug_axestick))
        {
            dprint(sprintf("embedhold: %s %s %s, %.1f m/s: %.2f s\n", e.netname, did ? "staggers" : "can't stagger", m.classname, xSpeed, hold));
        }
    }
    else if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: %s doesn't stagger %s: %.1f m/s of %.1f, can %g\n", e.netname, m.classname, xSpeed, least, can));
    }
};

// A light body with a ragdoll (staggering, knocked down, dead) held by the limb the blade went into, by a spring from the hand on
// its handle. TRUE: it is. FALSE: it stands, or is heavy.
float(entity e, entity p, entity h, float xIn) VR_Embed_Spring =
{
    if(!VR_Embed_Light(h))
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
    if(ragdolltug(h, p, hand, 1) >= 0)
    {
        return ragdolltug(h, p, hand, 13, '0 0 0', 0, force) > 0;
    }
    return ragdolltug(h, p, hand, 12, e.vr_stick_at, 0, force) > 0;
};

// A light body standing, its holder's walk `xWalk` this frame: going away from it, past the slack, it comes along, as a monster steps
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
    if(!e.vr_hold_hands)
    {
        return FALSE;
    }
    // (The hand's own time, what its pull and the resistance to it go by: bullet time's hands move at their real speed, 3 times as far
    // a game second at 0.3. By the game's seconds a blade would hold 3 times as hard, and a real pull of 3 m/s read as a jump.)
    float dt = max(0, time - e.vr_hold_last) * (vr_hand_time_scale > 0 ? 1 / vr_hand_time_scale : 1);
    e.vr_hold_last = time;
    float cm = 0.01 * VRMetersToUnits(); // units a cm
    vector dir = e.vr_stick_dir;

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
    vector wound = (h.health > 0 && !h.vr_knockdown) ? h.origin - e.vr_hold_lasthost : e.vr_stick_at - e.vr_hold_lastwound;
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
    // (A dead body gives it up to a tug of the arm, however it comes along: only your walking drags it.)
    float tugged = h.health <= 0 ? (moved3 - walk) * dir : 0;
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
    e.vr_hold_off = (e.vr_hold_off + arm) * max(0, 1 - VR_EMBED_OFF_RELAX * dt);
    float along = e.vr_hold_err;
    float off = vlen(e.vr_hold_off);
    if(cvar_hget(cvarh_vr_debug_axestick) >= 3)
    {
        dprint(sprintf("embedhold: %s along %.1f off %.1f units, %.1f further, %.1f in; this frame the hand %.1f, it %.1f along it\n", e.netname, along, off, far, e.vr_stick_depth + e.vr_stick_slide, moved3 * dir, wound * dir));
    }
    if(off > max(0, cvar_hget(cvarh_vr_embed_hold_break)) * cm)
    {
        VR_Embed_Free(e, "off its axis");
        return TRUE;
    }
    // (A body dying or knocked down falls away from the hand: the hold goes on, further.)
    float reachBy = h.health <= 0 ? max(1, cvar_hget(cvarh_vr_embed_hold_reach_corpse)) :
                    ((h.vr_knockdown && !h.vr_kd_stagger) ? max(1, cvar_hget(cvarh_vr_embed_hold_reach_down)) : 1);
    if(far > max(0, cvar_hget(cvarh_vr_embed_hold_reach)) * cm * reachBy)
    {
        VR_Embed_Free(e, "too far from the hand");
        return TRUE;
    }

    // The resistance: a stick, then a slide at most so fast, both by how deep (a dead body: less).
    float corpse = h.health <= 0;
    float point = e.vr_stick_embed == AXESTICK_POINT;
    float inShare = VR_Embed_InShare(e);

    // Moving the body: by the limb it is in, or, standing, dragged along as you walk.
    if(!VR_Embed_Spring(e, p, h, inShare))
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
    float over = fabs(along) - stick;
    if(over > 0)
    {
        float move = min(over * max(0, cvar_hget(cvarh_vr_embed_hold_follow)), rate) * dt * (along > 0 ? 1 : -1);
        // (How far it goes: a sword up to its Most In, an axe no deeper than it went.)
        float deepest = point ? max(1, e.vr_stick_bladelen) * max(0, cvar_hget(cvarh_vr_embed_hold_sword_most)) * 0.01 - e.vr_stick_depth : 0;
        float was = e.vr_stick_slide;
        e.vr_stick_slide = min(deepest, e.vr_stick_slide + move);
        float moved = e.vr_stick_slide - was;
        e.vr_hold_err = e.vr_hold_err - moved;
        // (Stopped by how far it goes: the hand's motion past it isn't kept for later.)
        if(fabs(e.vr_hold_err) > stick + 2 * cm && e.vr_hold_err > 0 && moved == 0)
        {
            e.vr_hold_err = stick;
        }
        if(moved < 0)
        {
            e.vr_hold_pulled = e.vr_hold_pulled - moved;
        }
        else if(moved > 0 && cvar_hget(cvarh_vr_embed_hold_slide_damage) > 0 && h.takedamage && h.health > 0)
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

    // Out far enough: free. A dead body gives it up after a tug (vr_embed_tug_corpse).
    float free = point ? e.vr_stick_depth + e.vr_stick_slide <= max(0, cvar_hget(cvarh_vr_embed_hold_sword_out)) * cm :
                         e.vr_stick_slide <= -max(0, cvar_hget(cvarh_vr_embed_hold_axe_travel)) * cm;
    if(!free && corpse && e.vr_hold_pulled >= max(0, cvar_hget(cvarh_vr_embed_tug_corpse)) * cm)
    {
        free = TRUE;
    }
    if(free)
    {
        VR_Embed_Free(e, "pulled out");
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

// A blow of `xHand` being dealt on `m` at `xAt` going along `xDir` (self is the player; xSpeed m/s, the hand's, as the blow's own;
// xKind its VR_MKIND_*, -1 on a corpse whose blow has none: its line says): whether it may leave the blade in. For VR_Embed_BlowsDone.
void(float xHand, entity b, entity m, vector xAt, vector xDir, float xSpeed, float xKind) VR_Embed_Blow =
{
    if(!cvar_hget(cvarh_vr_embed) || vr_embed_host != world || m == world || m.vr_gib || !self.ishuman ||
       !((m.flags & FL_MONSTER) || VR_Melee_CorpseTarget(m)))
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
        // (What strikes: the blow's point; a corpse's has none, where it met it.)
        float tip = xKind >= 0 ? 1 - b.mh_c_f : (len > 0.01 ? vlen(b.mh_far - xAt) / len : 1);
        if(xKind >= 0 && xKind != VR_MKIND_STAB)
        {
            why = "not a stab";
        }
        else if(xSpeed < cvar_hget(cvarh_vr_embed_stab_speed))
        {
            why = "too slow";
        }
        else if(along < cos(cvar_hget(cvarh_vr_embed_stab_angle) * (3.14159265 / 180)))
        {
            why = "off its line";
        }
        else if(tip > cvar_hget(cvarh_vr_embed_stab_tip) * 0.01)
        {
            why = "not by its tip";
        }
    }
    else
    {
        if(xKind >= 0 && xKind != VR_MKIND_SLASH)
        {
            why = "not a swing";
        }
        else if(xSpeed < cvar_hget(cvarh_vr_embed_swing_speed))
        {
            why = "too slow";
        }
        // (Edge first, not its side nor along the handle: the engine, embedstick.)
    }
    if(cvar_hget(cvarh_vr_debug_axestick))
    {
        dprint(sprintf("embedhold: blow of hand %g at %s, kind %g, %.1f m/s, along %.2f, point %.2f: %s\n", xHand, m.classname, xKind, xSpeed, along, b.mh_c_f,
            why == string_null ? "may stay in" : why));
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

// The blows of `xHand` this frame dealt: one that may stay in, on a body still there: the hand's weapon is the blade stuck in it
// now, the hand on it (and the other hand, if it helps hold it).
void(float xHand) VR_Embed_BlowsDone =
{
    entity m = vr_embed_host;
    vr_embed_host = world;
    if(m == world || m.modelindex != vr_embed_hostmodel || m.model == string_null || VRIsHandWeaponless(self, xHand))
    {
        return;
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
        return;
    }
    w.vr_wpn_from = self; // (taken back: a regrip, not a pickup)
    w.throwhit = QVR_THROWHIT_HIT; // (its blow is dealt)
    w.throwdmgspeed = VR_Throw_DmgSpeed();
    vr_embed_stabbing = TRUE;
    float stuck = embedstick(w, m, vr_embed_at, vr_embed_vel, xHand);
    vr_embed_stabbing = FALSE;
    if(!stuck)
    {
        remove(w); // (the weapon's record is the hand's still)
        return;
    }
    VRSetHandEmpty(self, xHand);
    VRHandStopFiring(self, xHand);
    W_SetCurrentAmmoFor(xHand);
    VR_Embed_TakeHand(w, self, xHand);
    if(helping)
    {
        VR_Embed_TakeHand(w, self, other);
    }
    VR_Embed_Stagger(w, m, vr_embed_vel, vlen(vr_embed_vel) / VRMetersToUnits());
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
};
