// vr_melee.qc -- melee for tracked hands: one model for swords, axes, guns and bare hands (docs/vr-port/ROUND21.md,
// "Melee, redesigned").
//
// WHAT A HAND HOLDS is laid out as a line: its near end (a sword's pommel, the end of an axe's handle, a gun's butt),
// its grip (the hand) and its far end (the muzzle point of its model: a sword's tip, an axe's head, a gun's muzzle; a
// fist's knuckles). Points along it, each of one kind (VR_MPT_*), are swept from the last pose to this one against
// what bleeds and against walls.
//
// A HIT is a point sweeping into something while the ARM drives it: the grip's speed, relative to the head (walking and
// turning are not the hand's motion), at least vr_melee_speed (a weapon's swing VR_MELEE_ARM times that), having come
// VR_MELEE_RUN towards where it goes (a blow, not a wiggle; a slow swing from further, VR_MELEE_SLOW_*) and carrying the
// hand (the wrist's speed). Any direction but back towards the hand (VR_MELEE_PULLBACK: drawn back); what a hand struck
// it strikes again only entering it (a retraction still in it strikes nothing). What kind of blow it is follows
// from which part hit and how it moved (VR_Melee_Decide): a stab or jab (the far end moving along the line, outwards), a
// slash or chop or gun swing (the far end any other way), a pommel or butt strike (the near end), a punch (a closed
// fist). Of a blade's points in the thing, the outermost strikes. The pommel's touch waits a moment for the blade
// proper (vr_melee_pommel_wait): a cut from close by is the blade's. The hilt, a handle, a gun's grip (where the hand
// is) strike nothing.
//
// THE PARRY STANCE is the held thing presented across (the parry's pose: VR_Parry_Blocks, combat.qc), or an open palm
// facing ahead. A BASH is the stance, held VR_BASH_HELD, pushed forward (both its ends going ahead), a few centimetres:
// then what is just ahead is knocked back (VR_Bash). A palm's SHOVE is the palm pushed out from the shoulder. Two hands
// (a weapon held two-handed, an open palm pushing on a one-handed weapon, both palms) bash harder.
//
// The numbers are in one place, below: tune them against the labelled takes (vr_motion_eval), not case by case. Those the
// engine's melee builtins use too (the history's length and look-back times) are in Quake/vr/vr_melee_shared.h.

// ----------------------------------------------------------------------------
// Following a hand

// Takes `xHand`'s new pose (once a frame, PlayerVRMelee): the points swept from the last pose to this one, the line's
// velocities. FALSE when there is no new pose (a server frame between two client ones: the next pose's sweep spans
// both) or nothing to sweep from (the first pose, a jump, another weapon).
// Where the hand of `p` turns about when it wiggles (world): VR_MELEE_WRIST_BACK behind the controller along its own
// axis. The hand's angles carry the gun angle (vr_gunangle/vr_gunyaw, vr_offhandpitch/vr_offhandyaw: the engine's
// withHandOffsets turns the controller by the yaw, then by minus the pitch): taken out here. The rest of the hand
// calibration (its move and roll, hands::calibration) is the real hand's place on the controller: the wrist goes with
// the hand, as it is here. The off hand's pitch and yaw are the main hand's mirrored with vr_handcal_off_mirror. A
// weapon held two-handed turns with the hands' line, not with one controller: its hand itself.
vector(entity p, float xHand) VR_Melee_Wrist =
{
    vector h = VRGetEntHandPos(p, xHand);
    if(VRGet2HAiming(p))
    {
        return h;
    }
    float ownOff = xHand == cVR_OffHand && !cvar_hget(cvarh_vr_handcal_off_mirror);
    float pitch = cvar_hget(ownOff ? cvarh_vr_offhandpitch : cvarh_vr_gunangle) * 0.0174533;
    float yaw = cvar_hget(ownOff ? cvarh_vr_offhandyaw : cvarh_vr_gunyaw) * 0.0174533;
    if(xHand == cVR_OffHand && !ownOff)
    {
        yaw = -yaw;
    }
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetEntHandRot(p, xHand));
    float d = VR_MELEE_WRIST_BACK * VRMetersToUnits();
    float cy = cos(yaw);
    vector w = h - v_right * (d * sin(yaw)) - v_up * (d * cy * sin(pitch)) - v_forward * (d * cy * cos(pitch));
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return w;
};

// The player's own clock: `time`, ahead by the engine's vr_player_time_offset (slow motion's Sandevistan, where the
// player runs at full speed in a slowed world; 0 otherwise). Its blows, rearming, pommel wait, bash stance and cooldown
// are timed on it.
float() VR_PlayerTime =
{
    return time + vr_player_time_offset;
};

// A point of the line's velocity about the grip (`xR0` to `xR1`, from the grip, over `xSpan` s), as its chord over
// VR_MELEE_VEL_SPAN would read it. A chord's way is the arc's at the chord's middle: over a longer span (a tick's pose
// two headset frames on, a dropped frame) it points further back into the turn (a blade's tip swung round read as drawn
// back along the line: VR_MELEE_PULLBACK) and reads slower. Turning uniformly about the grip (the line is rigid), the
// chord is turned on to the window's middle and scaled to its length: every span reads as a 72 Hz tick's chord did.
// The strength's (and the damage's) speed: the measured speed times this. Measured over the poses' times (meleefps,
// 2026-10-19) a blow's speed is its own; before, at the author's 120 Hz, the ticks beating against the frames read the
// first hit's speed 1.15-1.2 times too fast on average, and his blows' strength was tuned on those. The gates (land,
// slow, wiggle) read the true speed; only how hard a landed blow strikes keeps the old feel.
float VR_MELEE_STRENGTH_SPEED = 1.13;
float VR_MELEE_VEL_SPAN = 0.013889; // s (a 72 Hz tick: at 72 Hz every chord is this, as before)
vector(vector xR0, vector xR1, float xSpan) VR_Melee_TurnVel =
{
    vector chord = (xR1 - xR0) * (1 / xSpan);
    vector n = VR_Cross(xR0, xR1);
    float sn = vlen(n);
    float th = atan2(sn, xR0 * xR1);
    if(sn < 0.000001 || th < 0.001)
    {
        return chord;
    }
    n = n * (1 / sn);
    float k = VR_MELEE_VEL_SPAN / xSpan;
    float a = th * 0.5 * (1 - k);
    float hw = min(th * k * 0.5, 1.5);
    float hs = th * 0.5;
    chord = chord * (sin(hw) / hw) * (hs / sin(hs));
    return chord * cos(a) + VR_Cross(n, chord) * sin(a) + n * (n * chord) * (1 - cos(a));
};

float(float xHand, entity b) VR_Melee_Track =
{
    // The pose is the client's as this server frame ends (time + frametime): QC's time is still the frame's start. On
    // the player's own clock (the engine's vr_player_time_offset: 0 but in slow motion's Sandevistan, where the player
    // runs at full speed in a slowed world, and frametime is the player's), so its blows' speeds are its own.
    float now = time + vr_player_time_offset + frametime;
    float dt = now - b.mh_time;
    // Over the poses' own times when the client sends them (.vr_posetime, the headset's frames' times): the server's
    // 72 Hz ticks beat against the headset's frames, so a tick's pose is one or two frames newer than the last tick's
    // (at 90 Hz 1/90 or 2/90 s for the tick's 1/72: a steady swing read at 0.8 and 1.6 times its speed by turns; a
    // dropped frame likewise). Measured over the poses' times every headset rate reads a swing's own speed.
    float ptime = self.vr_posetime;
    if(ptime >= 0 && b.mh_ptime >= 0)
    {
        float pdt = ptime - b.mh_ptime;
        if(pdt < -0.5 * VR_POSE_CLOCK_WRAP)
        {
            pdt = pdt + VR_POSE_CLOCK_WRAP;
        }
        if(pdt == 0 && dt > 0 && dt < 0.05)
        {
            return FALSE; // (the same frame's pose again: no news, not a stop)
        }
        if(pdt > 0 && pdt <= 0.25 && dt > 0 && dt <= 0.25)
        {
            dt = pdt;
        }
    }
    float thing = VR_Melee_Thing(self, xHand);
    float wid = VRGetEntWeapon(self, xHand);
    if(!VR_Melee_Line(self, xHand, thing) || !VRHandsTracked(self))
    {
        b.mh_n = 0;
        b.mh_thing = thing;
        b.mh_free = FALSE;
        b.mh_heavy2h = VR_Heavy_TwoHanded(self, xHand);
        b.mh_lenient = VR_Weight_Leniency(self, xHand) * VR_Heavy_SpeedFactor(b.mh_heavy2h);
        b.mh_curve = VR_Weight_MeleeCurve(self, xHand);
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        b.mh_fast = 0;
        b.mh_time = now;
        b.mh_ptime = ptime;
        return FALSE;
    }

    float m2u = VRMetersToUnits();
    vector head = VRGetHeadPos(self);
    vector rnear = VR_Blow_ToPlaySpace(vr_mline_near - head) * (1 / m2u);
    vector rgrip = VR_Blow_ToPlaySpace(vr_mline_grip - head) * (1 / m2u);
    vector rfar = VR_Blow_ToPlaySpace(vr_mline_far - head) * (1 / m2u);
    vector rwrist = VR_Blow_ToPlaySpace(VR_Melee_Wrist(self, xHand) - head) * (1 / m2u);
    vector vwrist = '0 0 0';

    // The same pose again: no news, not a stop.
    if(rgrip == b.mh_rgrip && rfar == b.mh_rfar && dt > 0 && dt < 0.05 && thing == b.mh_thing)
    {
        return FALSE;
    }

    float same = b.mh_n > 0 && thing == b.mh_thing && wid == b.mh_wid && dt > 0 && dt <= 0.1;
    // (Taken anywhere, let go of its handle, the far end the other end: its points are other points.)
    if(vr_mline_free != b.mh_free || (vr_mline_free && (vr_mline_tip != b.mh_free_tip || fabs(vr_mline_hs - b.mh_free_hs) > 0.05)))
    {
        same = FALSE;
    }
    // What the hand holds jumped, not the hand: put back in it at once by the weight (left too far behind it), or its far
    // end jumping (a weapon taken). No speed of its points from the jump and no blow, but the hand's own way is still its
    // way: the grip's history goes on (round 21 wigglefix: a gun wiggled at the dummy, lagging the wrist's roll, is put
    // back in the hand at about every turn of the wiggle; each put-back forgot the wiggle, and the next flick passed for a
    // fresh blow, a butt strike).
    float jumped = FALSE;
    vector vgripOwn = '0 0 0';
    b.mh_vnear_last = b.mh_vnear;
    b.mh_vgrip_last = b.mh_vgrip;
    b.mh_vfar_last = b.mh_vfar;
    if(same)
    {
        // A jump (a teleport, the play space re-based) is faster than a hand goes.
        if(vlen(rgrip - b.mh_rgrip) > max(0.3, VR_MELEE_JUMP * dt))
        {
            same = FALSE;
        }
        else if(vlen(rfar - b.mh_rfar) > max(0.6, VR_MELEE_TIPJUMP * dt) ||
                (self.handdrop & (xHand == cVR_MainHand ? VR_HANDJUMP_MAIN : VR_HANDJUMP_OFF)))
        {
            jumped = TRUE;
        }
    }
    if(same && !jumped)
    {
        b.mh_vgrip = (rgrip - b.mh_rgrip) * (1 / dt);
        b.mh_vnear = b.mh_vgrip + VR_Melee_TurnVel(b.mh_rnear - b.mh_rgrip, rnear - rgrip, dt);
        b.mh_vfar = b.mh_vgrip + VR_Melee_TurnVel(b.mh_rfar - b.mh_rgrip, rfar - rgrip, dt);
        vwrist = (rwrist - b.mh_rwrist) * (1 / dt);
    }
    else
    {
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        if(jumped)
        {
            vgripOwn = (rgrip - b.mh_rgrip) * (1 / dt);
        }
    }
    b.mh_rnear = rnear;
    b.mh_rgrip = rgrip;
    b.mh_rfar = rfar;
    // (A fist's blow line is its knuckles' surface as edited (vr_melee_fist_*: the author's runs down the hand, not out
    // ahead of it): drawn back or struck is judged along the hand's own forward, the punch's way, not along that line.)
    b.mh_rpunch = normalize(VR_Blow_ToPlaySpace(VRForwardOf(VRGetEntHandRot(self, xHand))));
    b.mh_rwrist = rwrist;
    b.mh_near = vr_mline_near;
    b.mh_pgrip = same ? b.mh_grip : vr_mline_grip;
    b.mh_grip = vr_mline_grip;
    b.mh_far = vr_mline_far;
    b.mh_dt = (same && !jumped) ? dt : 0;
    b.mh_time = now;
    b.mh_ptime = ptime;
    b.mh_thing = thing;
    b.mh_wid = wid;
    b.mh_heavy2h = VR_Heavy_TwoHanded(self, xHand);
    b.mh_lenient = VR_Weight_Leniency(self, xHand) * VR_Heavy_SpeedFactor(b.mh_heavy2h);
    b.mh_curve = VR_Weight_MeleeCurve(self, xHand);
    if(!jumped)
    {
        b.mh_fast = max(vlen(b.mh_vfar), vlen(b.mh_vgrip)); // (a jump: as it was, no news)
    }
    if(same)
    {
        b.mh_hi = b.mh_hi + 1;
        if(b.mh_hi >= VR_MELEE_HISTORY)
        {
            b.mh_hi = 0;
        }
        b.mh_hfar[b.mh_hi] = b.mh_vfar; // (a jump: none)
        b.mh_hgrip[b.mh_hi] = jumped ? vgripOwn : b.mh_vgrip;
        b.mh_hwrist[b.mh_hi] = vwrist;
        b.mh_hdt[b.mh_hi] = dt;
    }
    else
    {
        b.mh_hdt[b.mh_hi] = 0; // (nothing to look back through)
    }

    if(vr_mline_free && (!b.mh_free || vr_mline_tip != b.mh_free_tip) && cvar_hget(cvarh_developer))
    {
        dprint(sprintf("melee: hand %g holds its weapon off its handle, %.2f of the way from its near end to its tip (%.1f units long); "
            "it strikes with all of it, the far end its %s (the hand at %v, the near end %v, the far end %v)\n", xHand,
            vr_mline_hs, vlen(vr_mline_far - vr_mline_near), vr_mline_tip ? "tip" : "near end", vr_mline_grip, vr_mline_near,
            vr_mline_far));
    }
    b.mh_free = vr_mline_free;
    b.mh_free_hs = vr_mline_hs;
    b.mh_free_tip = vr_mline_tip;
    b.mh_free_gw = vr_mline_gw;
    b.mh_free_back = vr_mline_back;
    b.mh_bfrom = vr_mline_free ? -1 : vr_mline_from_x;

    // The points: kept from the last pose, laid out now.
    VR_Melee_LayoutOf(b);
    // (Unrolled, constant indices: plain fields and globals, not the array accessors a variable index calls.)
#define VR_MELEE_KEEP(k) \
    if(k < vr_mpt_n) \
    { \
        b.mh_pp[k] = same ? b.mh_cp[k] : VR_Melee_At(vr_mpt_f[k]); \
        b.mh_cp[k] = VR_Melee_At(vr_mpt_f[k]); \
    }
    VR_MELEE_KEEP(0)
    VR_MELEE_KEEP(1)
    VR_MELEE_KEEP(2)
    VR_MELEE_KEEP(3)
    VR_MELEE_KEEP(4)
    VR_MELEE_KEEP(5)
    VR_MELEE_KEEP(6)
    VR_MELEE_KEEP(7)
    VR_MELEE_KEEP(8)
#undef VR_MELEE_KEEP
    b.mh_n = vr_mpt_n;
    // A hammer's or mace's head: its box (made from AD's model) and the line's frame, kept from the last pose.
    // (Or the weapon's own head and width: Edit Blade Shape.)
    VR_Melee_HeadOf(self, xHand, thing);
    b.mh_hd_lo = vr_mhead_lo;
    b.mh_hd_hi = vr_mhead_hi;
    b.mh_bwidth = vr_mline_shape_y;
    vector hy = VR_Melee_ModelY(self, xHand, vr_mline_far - vr_mline_grip);
    b.mh_hd_pg = same ? b.mh_hd_cg : vr_mline_grip;
    b.mh_hd_pf = same ? b.mh_hd_cf : vr_mline_far;
    b.mh_hd_py = same ? b.mh_hd_cy : hy;
    b.mh_hd_cg = vr_mline_grip;
    b.mh_hd_cf = vr_mline_far;
    b.mh_hd_cy = hy;
    return same;
};

// The velocity of a point of the layout at `xF` (play space, m/s): the line moves as a rigid thing.
vector(entity b, float xF) VR_Melee_PointVel =
{
    if(xF < 0 && !b.mh_free)
    {
        return b.mh_vnear;
    }
    return b.mh_vgrip + (b.mh_vfar - b.mh_vgrip) * xF;
};

// Whether `e` can be struck by the hands of `self`: it bleeds (or breaks), it is solid, and it is not the player's
// own (held or thrown; thrown with both hands a moment ago, VR_Throw_SparedBlows).
float(entity e) VR_Blow_Strikable =
{
    return e != self && e != world && e.takedamage && e.solid != SOLID_NOT && e.solid != SOLID_TRIGGER &&
           e.owner != self && e.carry_player != self && !VR_Throw_SparedBlows(e, self);
};

// ----------------------------------------------------------------------------
// Deciding a blow

// The way a part moved, named for the report (the play space: the head's facing ahead).
string(entity b, vector xVel, float xThing) VR_Melee_Way =
{
    float speed = vlen(xVel);
    if(speed <= 0)
    {
        return "";
    }
    vector d = xVel * (1 / speed);
    vector fwd = VR_Blow_Facing();
    float ahead = d * fwd;
    float up = d_z;
    float side = sqrt(max(0, 1 - ahead * ahead - up * up));
    if(xThing == VR_MTHING_FIST)
    {
        if(up > 0.6)
        {
            return "uppercut";
        }
        if(up < -0.6)
        {
            return "overhead";
        }
        if(side > fabs(ahead))
        {
            return "hook";
        }
        return ahead >= 0 ? "straight" : "backhand";
    }
    if(up < -0.8)
    {
        return "overhead";
    }
    if(up > 0.8)
    {
        return "rising";
    }
    if(fabs(up) < 0.4)
    {
        return "horizontal";
    }
    return up < 0 ? "diagonal, down" : "diagonal, up";
};

// Whether `b`'s weapon is wiggled, not swung (meleewiggled). Never a fist's.
float(entity b) VR_Melee_Wiggled =
{
    if(b.mh_thing == VR_MTHING_FIST)
    {
        return FALSE;
    }
    return meleewiggled(b);
};

// Whether `b`'s gun is being taken down to a holster to reload, not struck with (NOTES.md vrfiringrange_2026-09-30_02-03-28:
// a fast reload next to a monster hit it): its grip gone steeply down from aim or torso level (VR_MELEE_RELOAD_*), coming
// back towards the body as it drops or not. A gun chopped down from overhead, or swung down and ahead at something, is
// still a blow.
float(entity b) VR_Melee_Reloading =
{
    if(b.mh_thing != VR_MTHING_GUN || b.mh_vgrip_z >= 0)
    {
        return FALSE;
    }
    vector from = meleegripago(b, VR_MELEE_RELOAD_TIME);
    if(from_z > VR_MELEE_RELOAD_FROM)
    {
        return FALSE;
    }
    // Its way, less any coming back (a holster is by the hip, under and behind a gun held out).
    vector moved = b.mh_rgrip - from;
    vector fwd = VR_Blow_Facing();
    float ahead = moved * fwd;
    if(ahead < 0)
    {
        moved = moved - fwd * ahead;
    }
    return -moved_z >= VR_MELEE_RELOAD_DOWN * vlen(moved);
};

// How a weapon's blow is struck, for VR_Melee_WristMoved and VR_Melee_CameFar: a swing of a sword, an axe or a club (not a
// gun's: its own numbers, VR_MELEE_GUN_*), a pommel strike (the near end), anything else.
float VR_MHOW_OTHER = 0;
float VR_MHOW_SWING = 1;
float VR_MHOW_POMMEL = 2;

// Whether the hand itself moved as a blow's does (vr_melee_wrist_speed; a punch VR_MELEE_WRIST_PUNCH times that, a swing
// VR_MELEE_WRIST_SWING, a pommel strike VR_MELEE_WRIST_POMMEL: `xHow`, VR_MHOW_*), not only turned.
float(entity b, float xHow) VR_Melee_WristMoved =
{
    float need = cvar_hget(cvarh_vr_melee_wrist_speed) * (b.mh_lenient > 0 ? b.mh_lenient : 1);
    if(b.mh_thing == VR_MTHING_FIST)
    {
        need = need * VR_MELEE_WRIST_PUNCH;
    }
    else if(xHow == VR_MHOW_SWING)
    {
        need = need * VR_MELEE_WRIST_SWING;
    }
    else if(xHow == VR_MHOW_POMMEL)
    {
        need = need * VR_MELEE_WRIST_POMMEL;
    }
    return meleewristspeed(b) >= need;
};

// Whether `b`'s motion came from far enough to be a blow (VR_MELEE_RUN): a swing (`xHow`, VR_MHOW_*) at `xOver` times the
// speed it lands at, the further the slower (VR_MELEE_SLOW_*); a pommel strike VR_MELEE_POMMEL_RUN.
float(entity b, float xHow, float xOver) VR_Melee_CameFar =
{
    float need = VR_MELEE_RUN;
    if(xHow == VR_MHOW_SWING)
    {
        need = need + VR_MELEE_SLOW_RUN * min(1, max(0, (VR_MELEE_SLOW_FROM - xOver) / (VR_MELEE_SLOW_FROM - 1)));
    }
    else if(xHow == VR_MHOW_POMMEL)
    {
        need = max(need, VR_MELEE_POMMEL_RUN);
    }
    return meleerun(b, TRUE) >= need;
};

// Whether `b`'s far end goes back along the line towards the hand (VR_MELEE_PULLBACK): drawn back, not struck with. A
// fist's: back against the hand's forward (mh_rpunch), whatever way its edited blow line runs (vr_melee_fist_*).
float(entity b) VR_Melee_PulledBack =
{
    float vfar = vlen(b.mh_vfar);
    vector axis = b.mh_thing == VR_MTHING_FIST ? b.mh_rpunch : normalize(b.mh_rfar - b.mh_rgrip);
    return vfar > 0 && (b.mh_vfar * axis) / vfar <= VR_MELEE_PULLBACK;
};

// The weight of a pommel (butt) strike with `xThing`.
float(float xThing) VR_Melee_PommelWeight =
{
    if(xThing == VR_MTHING_SWORD)
    {
        return VR_MELEE_POMMEL_SWORD;
    }
    if(xThing == VR_MTHING_AXE || xThing == VR_MTHING_CLUB || xThing == VR_MTHING_SAW)
    {
        return VR_MELEE_POMMEL_AXE;
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return VR_MELEE_POMMEL_CROWBAR;
    }
    return 1;
};

// Whether the blow of `xHand` (point `xI`, of kind `xKind`, on `xEnt`) lands, and as what: sets mh_kind, mh_speed,
// mh_strength, mh_part and mh_way. FALSE: nothing (too slow, or no blow).
float vr_msw_decide; // (the multi-target test, vr_melee_sweep_test 6: every blow lands, a cut at this strength)
float(float xHand, entity b, float xI, float xKind) VR_Melee_Decide =
{
    if(vr_msw_decide > 0)
    {
        b.mh_part = vr_mpt_name[xI];
        b.mh_kind = xKind == VR_MPT_EDGE ? VR_MKIND_SLASH : VR_MKIND_POMMEL;
        b.mh_speed = 6;
        b.mh_dmgspeed = 6;
        b.mh_core = vr_msw_decide;
        b.mh_strength = vr_msw_decide;
        b.mh_way = "down";
        return TRUE;
    }
    float minSpeed = VR_Melee_MinSpeedOf(b);
    float f = vr_mpt_f[xI];
    vector vp = VR_Melee_PointVel(b, f);
    vector axis = normalize(b.mh_rfar - b.mh_rgrip);
    float vfar = vlen(b.mh_vfar);
    float vgrip = vlen(b.mh_vgrip);
    float kind = VR_MKIND_SLASH;
    float speed = vgrip; // the arm's: the grip's speed
    float need = minSpeed;
    float land = 1; // the share over `need` it lands at (VR_MELEE_PUNCH_LAND, VR_MELEE_GUN_LAND)
    float weight = 1;
    vector way = b.mh_vfar;
    b.mh_part = vr_mpt_name[xI];
    float along = vfar > 0 ? (b.mh_vfar * axis) / vfar : 0;
    float lead = 0;

    if(xKind == VR_MPT_FIST)
    {
        // A punch is a closed fist (the grip held, or the flashlight in it): an open hand only shoves (VR_Bash). (The
        // author: every punch of his with the grip held; his open hands waved, flicked and patted at the dummy, some at
        // 7-9 m/s, all "no hit".)
        // Swung across, an open hand slaps (VR_Melee_Slaps): as a punch, weaker.
        kind = VR_MKIND_PUNCH;
        if(!VR_Melee_ClosedFist(xHand))
        {
            if(!VR_Melee_Slaps(xHand, b))
            {
                return FALSE;
            }
            kind = VR_MKIND_SLAP;
            b.mh_part = "the open hand";
        }
        way = b.mh_vgrip;
        // The blow's way: rising and steeply falling blows are thrown faster for the same effort.
        need = VR_Melee_SwingNeed(xHand, b, b.mh_vgrip);
        land = VR_MELEE_PUNCH_LAND;
        weight = cvar_hget(cvarh_vr_melee_punch_mult);
        if(kind == VR_MKIND_SLAP)
        {
            weight = weight * cvar_hget(cvarh_vr_melee_slap_mult);
        }
        else if(VR_Carry_IsHolding(self, xHand))
        {
            weight = weight * VR_Carry_PunchMult(self, xHand); // a box, a rock, a brick in hand: an improvised weapon
        }
    }
    else if(xKind == VR_MPT_EDGE || xKind == VR_MPT_BLUNT)
    {
        // The far end driven point first, along the line (its outer half striking): a stab (a jab). Else a swing: the
        // arm must drive it (VR_MELEE_ARM), however fast the head goes round. (Held anywhere on it, its pommel, hilt,
        // handle or butt: struck with as a swing is, a pommel (butt) strike.)
        if(xKind == VR_MPT_EDGE && along >= VR_MELEE_STAB_ALONG && f >= 0.5 && b.mh_thing != VR_MTHING_AXE &&
           b.mh_thing != VR_MTHING_CROWBAR)
        {
            kind = VR_MKIND_STAB;
            need = minSpeed * VR_MELEE_STAB_SPEED;
        }
        else
        {
            need = minSpeed * VR_MELEE_ARM;
            if(b.mh_thing == VR_MTHING_GUN)
            {
                land = VR_MELEE_GUN_LAND;
            }
            if(b.mh_thing == VR_MTHING_CROWBAR && vr_mpt_fh[xI] >= VR_CROWBAR_HOOK_F)
            {
                weight = VR_MELEE_CROWBAR_HOOK; // the hook: a hooked blow
            }
            if(xKind == VR_MPT_BLUNT)
            {
                kind = VR_MKIND_POMMEL;
                way = vp;
                weight = VR_Melee_PommelWeight(b.mh_thing);
            }
        }
    }
    else
    {
        // The pommel, the handle's end, a gun's butt: a pommel (butt) strike, unless the blade proper follows it into
        // the thing at once (VR_Melee_Sweep: a cut from close by).
        float sp = vlen(vp);
        lead = sp > 0 ? (vp * axis) / -sp : 0;
        kind = VR_MKIND_POMMEL;
        way = vp;
        speed = sp;
        if(vgrip < minSpeed * (b.mh_thing == VR_MTHING_GUN ? VR_MELEE_GUN_BUTT : 1))
        {
            return FALSE; // the arm drives it too (a sword waved round swings its guard fast)
        }
        // A gun's butt is struck with by carrying the gun at the thing (vr_melee_butt_run): a gun wiggled turns about the
        // hand, which hardly travels, and flings its butt round faster than the hand (round 21 wigglefix). Not a sword's
        // pommel: turned hilt first from close by, the author's pommel strikes come 0.36-0.54 m.
        if(b.mh_thing == VR_MTHING_GUN && meleerun(b, TRUE) < cvar_hget(cvarh_vr_melee_butt_run))
        {
            if(cvar_hget(cvarh_developer) >= 2)
            {
                dprint(sprintf("melee debug: the butt came %.2f m, under vr_melee_butt_run: no butt strike\n", meleerun(b, TRUE)));
            }
            return FALSE;
        }
        weight = VR_Melee_PommelWeight(b.mh_thing);
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: hand %g %s at %s (point %g, %.2f on the weapon): grip %.1f far %.1f", xHand,
            VR_Melee_KindName(kind, b.mh_thing), vr_mpt_name[xI], xI, vr_mpt_fh[xI], vgrip, vfar)); // (8 at most)
        dprint(sprintf(" along %.2f lead %.2f need %.1f run %.2f wrist %.2f\n", along, lead, need, meleerun(b, TRUE),
            meleewristspeed(b)));
    }
    if(speed < need * land)
    {
        return FALSE;
    }
    // Heavy handling: a heavy weapon in both hands is swung slower than a light one (its weight), so its blows need less
    // speed (mh_lenient: VR_Heavy_SpeedFactor); one slower than a light weapon's blow strikes only as a deliberate swing,
    // a smooth arc of its far end (meleearc: a steady way, at least vr_heavy_swing_arc m over vr_heavy_swing_time s), not
    // a slow reposition or a wave. Such a swing need not come from far (VR_Melee_CameFar) and isn't a wiggle (its arc is
    // steady); its strength is the arc's and the weight's (below).
    float heavyArc = 0;
    float deliberate = FALSE;
    float arcNeed = max(0.05, cvar("vr_heavy_swing_arc"));
    if(b.mh_heavy2h > 0 && xKind != VR_MPT_FIST)
    {
        heavyArc = meleearc(b, need * 0.5);
        if(speed < need * land / VR_Heavy_SpeedFactor(b.mh_heavy2h))
        {
            if(heavyArc < arcNeed)
            {
                if(cvar_hget(cvarh_developer) >= 2)
                {
                    dprint(sprintf("melee debug: heavy two-handed, slow (%.2f m/s) and no deliberate arc (%.2f m of %.2f): no blow\n",
                        speed, heavyArc, arcNeed));
                }
                return FALSE;
            }
            deliberate = TRUE;
        }
    }
    float how = VR_MHOW_OTHER;
    if(b.mh_thing != VR_MTHING_GUN && b.mh_thing != VR_MTHING_FIST)
    {
        if(xKind == VR_MPT_EDGE && kind != VR_MKIND_STAB)
        {
            how = VR_MHOW_SWING;
        }
        else if(xKind == VR_MPT_NEAR)
        {
            how = VR_MHOW_POMMEL;
        }
    }
    // Not drawn back: a blade, a head or a fist going back towards the hand (after a blow, still touching).
    if((xKind == VR_MPT_EDGE || xKind == VR_MPT_FIST) && VR_Melee_PulledBack(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: pulled back, no blow\n");
        }
        return FALSE;
    }
    // A blow, not a wiggle: the arm came from some way off (a slow swing from further).
    // (A heavy two-handed blow fast enough without its arc: from as far as a light weapon's at that speed, so a short jerk
    // of it doesn't strike either.)
    if(!deliberate && !VR_Melee_CameFar(b, how, speed * VR_Heavy_SpeedFactor(b.mh_heavy2h) / (need * land)))
    {
        return FALSE;
    }
    // And the hand itself moved, not only turned in place (a wiggle flicks the grip and the tip fast).
    if(!VR_Melee_WristMoved(b, how))
    {
        return FALSE;
    }
    // Nor a weapon waved back and forth.
    if(!deliberate && VR_Melee_Wiggled(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: wiggled, no blow\n");
        }
        return FALSE;
    }
    // Nor a gun taken down to reload.
    if(VR_Melee_Reloading(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint("melee debug: taken down to reload, no blow\n");
        }
        return FALSE;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(deliberate ? sprintf("melee debug: a blow (heavy two-handed, deliberate: arc %.2f m)\n", heavyArc) : "melee debug: a blow\n");
    }

    // The strength: the speed over the least. A sword's cut or stab measured at its tip (VR_MELEE_TIP_*), not the
    // grip that decided it landed (a sword carried off its handle, its near end out: the grip's, as before).
    float sspeed = speed;
    float sneed = need;
    if(b.mh_thing == VR_MTHING_SWORD && (kind == VR_MKIND_SLASH || kind == VR_MKIND_STAB) && (!b.mh_free || b.mh_free_tip))
    {
        sspeed = vfar;
        sneed = need * (kind == VR_MKIND_STAB ? VR_MELEE_TIP_STAB : VR_MELEE_TIP_SLASH);
    }
    sspeed = sspeed * VR_MELEE_STRENGTH_SPEED;
    float s = VR_MELEE_WEAKEST + (1 - VR_MELEE_WEAKEST) * (sspeed - sneed) / (sneed * (VR_MELEE_GOOD - 1)) *
              (b.mh_curve > 0 ? b.mh_curve : 1);
    b.mh_kind = kind;
    b.mh_speed = speed;
    b.mh_dmgspeed = sspeed;
    b.mh_core = min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s));
    // Heavy handling: a deliberate two-handed arc of a heavy weapon strikes hard however slow (its weight does the work:
    // weightdamage on top), vr_heavy_swing_strength at its weight class 1 for the arc needed, up to 1.3 times that for one
    // twice as long; a faster blow keeps its own if stronger.
    if(b.mh_heavy2h > 0 && heavyArc >= arcNeed)
    {
        float arcStrength = cvar("vr_heavy_swing_strength") * min(1.3, 0.7 + 0.3 * heavyArc / arcNeed);
        arcStrength = VR_MELEE_WEAKEST + (arcStrength - VR_MELEE_WEAKEST) * b.mh_heavy2h;
        b.mh_core = min(VR_MELEE_STRONGEST, max(b.mh_core, arcStrength));
    }
    if(b.mh_heavy2h > 0 && cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: heavy two-handed strength %.2f (by its speed %.2f, its arc %.2f m)\n", b.mh_core,
            min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s)), heavyArc));
    }
    b.mh_strength = b.mh_core * weight;
    b.mh_way = VR_Melee_Way(b, way, b.mh_thing);
    return TRUE;
};

// Whether the blow just decided (VR_Melee_Decide) lands on `xEnt`: a slap only on something that takes damage, not on a
// wall or a loose gib or head (it would burst it: VR_Gib_Blow); every other blow anywhere.
float(entity b, entity xEnt) VR_Melee_SlapLands =
{
    return b.mh_kind != VR_MKIND_SLAP || (xEnt.takedamage && !xEnt.vr_gib);
};

// One swing, many things (vr_melee_multi; ROUND22.md "One swing hits everything its edge passes through"): a swing of a
// weapon (not a bare fist: a punch strikes one thing) strikes each thing its striking part passes through once (at
// most vr_melee_multi_max of them), each its own blow by its own point and speed, and the world (a wall, the floor)
// once as well, in the order its parts meet them: a blow on the floor before a body lying on it no longer spends the
// swing (his note vrtesthall_2026-10-24_01-17-02), nor one on a body the wall beyond it.
float VR_MELEE_MULTI_MOST = 6; // things a swing strikes at most (vr_melee_multi_max's top)
float VR_MELEE_MULTI_LIST = 8; // and loose gibs with them (the list's fields: mh_hit_e0..e7; a full list: no more)
float(entity b) VR_Melee_Multi =
{
    return (cvar("vr_melee_multi") != 0) && (b.mh_thing != VR_MTHING_FIST) && (b.mh_thing != VR_MTHING_NONE);
};

float(entity b, entity xEnt) VR_Melee_HitHas =
{
    float n = b.mh_hit_n;
    return ((n > 0) && (b.mh_hit_e0 == xEnt)) || ((n > 1) && (b.mh_hit_e1 == xEnt)) ||
           ((n > 2) && (b.mh_hit_e2 == xEnt)) || ((n > 3) && (b.mh_hit_e3 == xEnt)) ||
           ((n > 4) && (b.mh_hit_e4 == xEnt)) || ((n > 5) && (b.mh_hit_e5 == xEnt)) ||
           ((n > 6) && (b.mh_hit_e6 == xEnt)) || ((n > 7) && (b.mh_hit_e7 == xEnt));
};

void(entity b, entity xEnt, float xGib) VR_Melee_HitAdd =
{
    if(VR_Melee_HitHas(b, xEnt) || (b.mh_hit_n >= VR_MELEE_MULTI_LIST))
    {
        return;
    }
    if(b.mh_hit_n == 0)      b.mh_hit_e0 = xEnt;
    else if(b.mh_hit_n == 1) b.mh_hit_e1 = xEnt;
    else if(b.mh_hit_n == 2) b.mh_hit_e2 = xEnt;
    else if(b.mh_hit_n == 3) b.mh_hit_e3 = xEnt;
    else if(b.mh_hit_n == 4) b.mh_hit_e4 = xEnt;
    else if(b.mh_hit_n == 5) b.mh_hit_e5 = xEnt;
    else if(b.mh_hit_n == 6) b.mh_hit_e6 = xEnt;
    else                     b.mh_hit_e7 = xEnt;
    b.mh_hit_n = b.mh_hit_n + 1;
    if(!xGib)
    {
        b.mh_hit_cap = b.mh_hit_cap + 1;
    }
};

// (A new motion: nothing struck yet.)
void(entity b) VR_Melee_HitClear =
{
    b.mh_hit_n = 0;
    b.mh_hit_cap = 0;
    b.mh_hit_world = FALSE;
};

// Whether a swing of `b` may still strike another thing: it struck something already (none: a bash or a held gib
// spent it, setting the hand's blow landed without one), fewer than vr_melee_multi_max things (loose gibs aside) and
// its list not full.
float(entity b) VR_Melee_MultiRoom =
{
    float most = min(VR_MELEE_MULTI_MOST, max(1, floor(cvar("vr_melee_multi_max"))));
    return ((b.mh_hit_n > 0) || b.mh_hit_world) && (b.mh_hit_cap < most) && (b.mh_hit_n < VR_MELEE_MULTI_LIST);
};

// Whether `xHand` may strike `xEnt` now: once per motion (see VR_MELEE_REARM_*); another thing at once (a swing
// through two monsters). A bash or a held gib struck spends the motion for everything (mh_hit_ent world). Never a
// ragdoll it holds or just let go of (VR_Ragdoll_HandSpares: a throw is not a blow). One swing, many things
// (VR_Melee_Multi): each thing once, after the world too.
float(float xHand, entity b, entity xEnt) VR_Melee_Armed =
{
    if(VR_Ragdoll_HandSpares(xEnt, self, xHand))
    {
        return FALSE;
    }
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    if(VR_Melee_Multi(b))
    {
        return VR_Melee_MultiRoom(b) && !VR_Melee_HitHas(b, xEnt);
    }
    return b.mh_hit_ent != world && xEnt != b.mh_hit_ent;
};

// Whether `xHand` may strike the world (a wall, the floor) now: a motion's first blow; one swing, many things
// (VR_Melee_Multi), once a motion after the things it struck too.
float(float xHand, entity b) VR_Melee_WallArmed =
{
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    return VR_Melee_Multi(b) && !b.mh_hit_world && b.mh_hit_n > 0;
};

// Re-arms `xHand` (each frame) once its motion slowed down since the last blow, or, its parts out of what it struck,
// turned back (a backswing) or went on long enough: a swing whose arc curves on through a big monster is one blow.
void(float xHand, entity b) VR_Melee_Rearm =
{
    if(!VRGetMeleeHitSoundPlayed(xHand) || VR_PlayerTime() < b.mh_hit_time + 0.1)
    {
        return;
    }
    float rearm = b.mh_fast < VR_Melee_MinSpeedOf(b) * VR_MELEE_REARM_SLOW;
    if(!rearm && VR_PlayerTime() - b.mh_hit_touch > VR_MELEE_REARM_OUT)
    {
        if(VR_PlayerTime() - b.mh_hit_time > VR_MELEE_REARM_TIME)
        {
            rearm = TRUE;
        }
        else if(b.mh_hit_dir != '0 0 0')
        {
            vector v = b.mh_vfar;
            if(vlen(b.mh_vgrip) > vlen(v))
            {
                v = b.mh_vgrip;
            }
            rearm = normalize(v) * b.mh_hit_dir < VR_MELEE_REARM_TURN;
        }
    }
    if(rearm)
    {
        VRSetMeleeHitSoundPlayed(xHand, FALSE);
        b.mh_hit_ent = world;
        VR_Melee_HitClear(b);
    }
};

// Whether a point swept from `xP0` to `xP1` strikes the wall (world, brush) it meets at `xAt`, whose normal is
// `xNormal`: not going down steeply onto the floor by the body (a hand reaching for a holster: a chop at the ground
// ahead does, VR_MELEE_FLOOR_AHEAD), nor down past a wall, grazing it; a blow chopped down into a wall does.
float vr_melee_wallwin; // vr_melee_wall_hold (s; 0: a wall struck only in the frame a part crosses it, as before)
float(vector xP0, vector xP1, vector xNormal, vector xAt) VR_Melee_WallTaken =
{
    vector d = normalize(xP1 - xP0);
    if(d_z >= VR_MELEE_WALL_DOWN)
    {
        return TRUE;
    }
    float taken;
    if(vr_melee_wallwin <= 0 && xNormal_z >= VR_MELEE_FLOOR)
    {
        taken = FALSE;
    }
    else if(xNormal_z >= VR_MELEE_FLOOR)
    {
        vector off = xAt - VRGetHeadPos(self);
        off_z = 0;
        taken = off * VRForwardOf('0 1 0' * self.v_viewangle_y) >= VR_MELEE_FLOOR_AHEAD * VRMetersToUnits();
    }
    else
    {
        taken = d * xNormal <= -VR_MELEE_WALL_INTO;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: going down at %.2f onto a surface facing %v (into it %.2f): %s\n", d_z, xNormal,
            -(d * xNormal), taken ? "strikes" : "passes"));
    }
    return taken;
};

// The speed (m/s) point `xI` of `b`, swept from `xP0` to `xP1`, goes into a surface facing `xNormal` (world): now (the
// line's velocity, or the sweep's in the world), or a pose before, the most. The hand's slide (vr_handpose.cpp) holds a
// weapon off the wall it meets, along its surface, so the pose it arrives in keeps little of the speed it came in at;
// a pose whose line jumped (VR_Melee_Track: a grip taken) has no velocity.
float(entity b, float xI, vector xP0, vector xP1, vector xNormal) VR_Melee_WallInto =
{
    vector nrm = VR_Blow_ToPlaySpace(xNormal);
    float f = vr_mpt_f[xI];
    vector before = b.mh_vgrip_last + (b.mh_vfar_last - b.mh_vgrip_last) * f;
    if(f < 0 && !b.mh_free)
    {
        before = b.mh_vnear_last;
    }
    float swept = 0;
    if(b.mh_dt > 0)
    {
        swept = (xP1 - xP0) * xNormal / (b.mh_dt * VRMetersToUnits());
    }
    return min(min(VR_Melee_PointVel(b, f) * nrm, before * nrm), swept) * -1;
};

// Whether point `xI` of `b`, swept from `xP0` to `xP1`, goes into the wall it met (VR_Melee_WallTaken) at `xAt`, facing
// `xNormal`, hard enough to strike it (VR_MELEE_WALL_ENTRY; a brush along it doesn't): then the hand may land a blow on
// that wall for vr_melee_wall_hold (VR_Melee_WallHeld).
float(entity b, float xI, vector xP0, vector xP1, vector xNormal, vector xAt) VR_Melee_WallEntered =
{
    if(!VR_Melee_WallTaken(xP0, xP1, xNormal, xAt))
    {
        return FALSE;
    }
    float into = VR_Melee_WallInto(b, xI, xP0, xP1, xNormal);
    if(vr_melee_wallwin <= 0)
    {
        return TRUE;
    }
    if(into < VR_MELEE_WALL_ENTRY * VR_Melee_MinSpeedOf(b))
    {
        if(cvar_hget(cvarh_developer) >= 2)
        {
            dprint(sprintf("melee debug: %s brushes a wall, into it at %.1f m/s\n", vr_mpt_name[xI], into));
        }
        return FALSE;
    }
    // The motion it came in at: this pose's, or the one before if faster (the slide slowed the hand meeting the wall);
    // going on into it (another of its parts, the next pose), the fastest since it met it.
    float again = VR_PlayerTime() - b.mh_wall_time <= vr_melee_wallwin;
    b.mh_wall_time = VR_PlayerTime();
    b.mh_wall_n = xNormal;
    if(again && vlen(b.mh_wall_vgrip) >= max(vlen(b.mh_vgrip), vlen(b.mh_vgrip_last)))
    {
        return TRUE;
    }
    b.mh_wall_vnear = b.mh_vnear;
    b.mh_wall_vgrip = b.mh_vgrip;
    b.mh_wall_vfar = b.mh_vfar;
    if(vlen(b.mh_vgrip_last) > vlen(b.mh_vgrip))
    {
        b.mh_wall_vnear = b.mh_vnear_last;
        b.mh_wall_vgrip = b.mh_vgrip_last;
        b.mh_wall_vfar = b.mh_vfar_last;
    }
    return TRUE;
};

// VR_Melee_Decide for a blow of `xHand` on a wall with point `xI`: at the speeds the hand came into it at
// (VR_Melee_WallEntered), not those the wall left it: held off by the hand's slide (vr_handpose.cpp), the weapon and the
// hand slow down on it, and a swing decided a frame or two after it met the wall was taken for a slow one.
float(float xHand, entity b, float xI) VR_Melee_WallDecide =
{
    vector vnear = b.mh_vnear;
    vector vgrip = b.mh_vgrip;
    vector vfar = b.mh_vfar;
    if(vr_melee_wallwin <= 0)
    {
        return VR_Melee_Decide(xHand, b, xI, vr_mpt_kind[xI]);
    }
    if(vlen(b.mh_wall_vgrip) > vlen(vgrip))
    {
        b.mh_vnear = b.mh_wall_vnear;
        b.mh_vgrip = b.mh_wall_vgrip;
        b.mh_vfar = b.mh_wall_vfar;
    }
    if(cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: a wall met %.2f s ago, come in at grip %.1f m/s (now %.1f)\n",
            VR_PlayerTime() - b.mh_wall_time, vlen(b.mh_wall_vgrip), vlen(vgrip)));
    }
    float landed = VR_Melee_Decide(xHand, b, xI, vr_mpt_kind[xI]);
    b.mh_vnear = vnear;
    b.mh_vgrip = vgrip;
    b.mh_vfar = vfar;
    return landed;
};

// Whether point `xI` of `b` (swept from `xP0` to `xP1`, `xR` thick) is still at the wall a part of the hand went into
// (VR_Melee_WallEntered) within vr_melee_wall_hold: in it (behind its surface, seen from the grip), or held off it by
// the hand's slide within `xR` and a unit, and not leaving it. The contact in the trace_ globals (fraction 1). A wall
// gave a blow one frame, the one the point crossed its surface: a blow decided a frame late (the wrist not yet up to
// speed: VR_Melee_WristMoved) never landed, while one in a monster's box is decided again every frame it is in it.
float(entity b, float xI, vector xP0, vector xP1, float xR) VR_Melee_WallHeld =
{
    if(vr_melee_wallwin <= 0 || vr_mpt_kind[xI] == VR_MPT_GRIP || VR_PlayerTime() - b.mh_wall_time > vr_melee_wallwin ||
       normalize(xP1 - xP0) * b.mh_wall_n > VR_MELEE_WALL_LEAVE)
    {
        return FALSE;
    }
    traceline(b.mh_grip, xP1, TRUE, self);
    if(trace_startsolid || trace_fraction >= 1)
    {
        if(trace_startsolid)
        {
            return FALSE; // (the hand itself in it)
        }
        traceline(xP1, xP1 - b.mh_wall_n * (xR + 1), TRUE, self);
        if(trace_startsolid || trace_fraction >= 1)
        {
            return FALSE;
        }
    }
    if(trace_plane_normal * b.mh_wall_n < VR_MELEE_WALL_SAME || VR_Blow_Strikable(trace_ent))
    {
        return FALSE;
    }
    trace_fraction = 1;
    return TRUE;
};

// Each frame (VR_Melee_Strike): whether the weapon of `b` is now off the wall it last struck (VR_MELEE_WALL_OFF, every
// part in front of that wall's plane by that much); then that wall may be struck again (VR_Melee_WallFresh).
void(entity b) VR_Melee_WallLeft =
{
    if(!b.mh_wall_on)
    {
        return;
    }
    float off = min(min(b.mh_near * b.mh_wall_bn, b.mh_grip * b.mh_wall_bn), b.mh_far * b.mh_wall_bn) - b.mh_wall_bd;
    if(off >= VR_MELEE_WALL_OFF * VRMetersToUnits())
    {
        b.mh_wall_on = FALSE;
    }
};

// Whether a contact at `xAt` on a wall facing `xNormal` is a new blow of `b` (its speed in mh_speed: after
// VR_Melee_WallDecide) and not the swing that struck that wall last again: one wall blow a swing. The same wall (its
// plane: the normals within VR_MELEE_WALL_SAME, the contact within 4 units of it) is struck again only once the weapon
// has left it (VR_Melee_WallLeft: a weapon lying on the wall, swept along it, struck it again with another part), and
// within VR_MELEE_WALL_FOLLOW of the last blow only as fast as VR_MELEE_WALL_WEAKER of it (the swing's recovery poking
// the wall). Two real blows in a row (the weapon drawn back and swung again) both land. Another wall: at once.
float(entity b, vector xNormal, vector xAt) VR_Melee_WallFresh =
{
    if(vr_melee_wallwin <= 0 || b.mh_wall_blow <= 0 || xNormal * b.mh_wall_bn < VR_MELEE_WALL_SAME ||
       fabs(xAt * b.mh_wall_bn - b.mh_wall_bd) > 4)
    {
        return TRUE;
    }
    float fresh = TRUE;
    string why = "";
    if(b.mh_wall_on)
    {
        fresh = FALSE;
        why = "the weapon still on it";
    }
    else if(VR_PlayerTime() - b.mh_wall_blow < VR_MELEE_WALL_FOLLOW && b.mh_speed < VR_MELEE_WALL_WEAKER * b.mh_wall_bspeed)
    {
        fresh = FALSE;
        why = sprintf("%.1f m/s after %.1f, %.2f s ago", b.mh_speed, b.mh_wall_bspeed, VR_PlayerTime() - b.mh_wall_blow);
    }
    if(!fresh && cvar_hget(cvarh_developer) >= 2)
    {
        dprint(sprintf("melee debug: the wall struck again by the same swing (%s): no blow\n", why));
    }
    return fresh;
};

// Whether the hands' blows find `e` by its box (VR_Melee_Sweep, after VR_Blow_Strikable): a solid one; a living monster
// knocked down (vr_knockdown.qc: not solid, its ragdoll struck as drawn, vr_hit_precise; a blow on it is a live one's:
// its head cut off, its limbs, by the same rules as standing; his note 2026-10-09); a loose gib or head, an exploding
// crate's piece (not solid: the traces pass them), not one held nor a small gib spared.
float(entity e) VR_Melee_BoxTarget =
{
    return e.solid == SOLID_SLIDEBOX || e.solid == SOLID_BBOX ||
           (e.solid == SOLID_NOT_BUT_TOUCHABLE && e.vr_knockdown == 1 && e.health > 0 && (e.flags & FL_MONSTER)) ||
           (e.solid == SOLID_NOT_BUT_TOUCHABLE && (e.vr_gib || e.vr_cratekind == 3) && e.carry_player == world &&
            (!VR_SmallGib_BlowSpared(e) || VR_SmallGib_BlowBursts(e)));
};

// One swing, many things (VR_Melee_Multi): a corpse or a body still dying (not solid to the traces: a ragdoll lying on the
// floor) is struck by the sweep as a box target is, as drawn (vr_hit_precise), and dealt as VR_Corpse_StrikeFrame's
// blow (VR_Corpse_Struck: client.qc VR_Melee_Deal). That one only traced the weapon's line as it is, from the hand: a
// blade whose tip met the floor first stopped there, short of the limb under it (his note vrtesthall_2026-10-24_01-17-02).
float(entity e) VR_Melee_CorpseTarget =
{
    return (e.solid == SOLID_NOT_BUT_TOUCHABLE) && e.takedamage && ((e.vr_corpse == 2) || e.vr_corpse_dying) &&
           !e.vr_gib && (e.health <= 0) && cvar_hget(cvarh_vr_corpse_gib);
};

// Whether a point swept from `xP0` to `xP1` (`xR` thick) starts in `e` (a box target: its model, vr_hit_precise, else its
// box, as VR_Melee_Sweep's boxes measure it): it doesn't enter it.
float(entity e, vector xP0, vector xP1, float xR) VR_Melee_StartsIn =
{
    if(!VR_Melee_BoxTarget(e))
    {
        return FALSE;
    }
    if(hitmodel_target(e))
    {
        return hitmodel_segment(e, xP0, xP1, xR, HITCLASS_MELEE) == 0;
    }
    vector pad = '1 1 1' * xR;
    return VR_SegmentBox(xP0, xP1, e.origin + e.mins - pad, e.origin + e.maxs + pad) == 0;
};

// The lines a hand's sweep strikes with (VR_Melee_Sweep, from VR_Melee_Ribs): each swept from the pose before to this one
// (vr_rib_p0 to vr_rib_p1), vr_rib_r thick, striking as the layout's point vr_rib_i (VR_Melee_Decide's); the box round
// them all, their thickness in (vr_rib_lo, _hi); a wall's crossing by each (VR_Melee_Sweep: how far past its surface,
// where, its normal).
float VR_MELEE_RIBS = 48; // (the arrays below as long)
float VR_MELEE_RIB_GAP = 1.5; // vr_melee_swept: neighbouring striking points' lines at most this many thicknesses apart
float VR_MELEE_RIB_MORE = 3;  // (at most this many lines more between two points)
float VR_MELEE_HEAD_MOST = 4; // a hammer's or mace's head: its lines along each of its two long sides, at most
vector vr_rib_p0[48];
vector vr_rib_p1[48];
float vr_rib_i[48];
float vr_rib_r[48];
float vr_rib_wd[48];
vector vr_rib_wat[48];
vector vr_rib_wn[48];
float vr_rib_n;
float vr_rib_rmax;
vector vr_rib_lo;
vector vr_rib_hi;

void(vector xP0, vector xP1, float xI, float xR) VR_Melee_RibAdd =
{
    if(vr_rib_n >= VR_MELEE_RIBS)
    {
        return;
    }
    vr_rib_p0[vr_rib_n] = xP0;
    vr_rib_p1[vr_rib_n] = xP1;
    vr_rib_i[vr_rib_n] = xI;
    vr_rib_r[vr_rib_n] = xR;
    vr_rib_n = vr_rib_n + 1;
    vr_rib_rmax = max(vr_rib_rmax, xR);
    vr_rib_lo_x = min(vr_rib_lo_x, min(xP0_x, xP1_x) - xR);
    vr_rib_lo_y = min(vr_rib_lo_y, min(xP0_y, xP1_y) - xR);
    vr_rib_lo_z = min(vr_rib_lo_z, min(xP0_z, xP1_z) - xR);
    vr_rib_hi_x = max(vr_rib_hi_x, max(xP0_x, xP1_x) + xR);
    vr_rib_hi_y = max(vr_rib_hi_y, max(xP0_y, xP1_y) + xR);
    vr_rib_hi_z = max(vr_rib_hi_z, max(xP0_z, xP1_z) + xR);
};

// A heavy head's lines along one of its sides, from `xLo` to `xHi` (shares of the line), each reaching `xIn` (its
// thickness, a share) to the faces: a thickness apart at most.
float(float xLo, float xHi, float xIn) VR_Melee_HeadLines =
{
    float span = xHi - xLo - 2 * xIn;
    if(span <= 0 || xIn <= 0)
    {
        return 1;
    }
    return min(VR_MELEE_HEAD_MOST, 1 + ceil(span / xIn));
};

// The place of the `xK`th of `xN` such lines.
float(float xLo, float xHi, float xIn, float xK, float xN) VR_Melee_HeadAt =
{
    if(xN <= 1)
    {
        return (xLo + xHi) * 0.5;
    }
    return xLo + xIn + (xHi - xLo - 2 * xIn) * xK / (xN - 1);
};

// A point's line from the pose before to this one along its arc round the grip, not the chord, when the chord cuts the
// arc's inside by more than VR_MELEE_ARC_SAG of the line's thickness: a fast turn, or a pose two headset frames on (a
// dropped frame; the 72 Hz ticks beating against the headset's frames) cuts further in. Then two lines, through the
// arc's middle (the grip's middle, the point's way from it turned half way, its distance the mean).
float VR_MELEE_ARC_SAG = 0.5;
void(entity b, vector xP0, vector xP1, float xI, float xR) VR_Melee_ArcRib =
{
    vector r0 = xP0 - b.mh_pgrip;
    vector r1 = xP1 - b.mh_grip;
    float l0 = vlen(r0);
    float l1 = vlen(r1);
    if(l0 >= 1 && l1 >= 1 && r0 * r1 > -0.9 * l0 * l1)
    {
        vector mid = (b.mh_pgrip + b.mh_grip) * 0.5 + normalize(r0 * (1 / l0) + r1 * (1 / l1)) * ((l0 + l1) * 0.5);
        if(vlen(mid - (xP0 + xP1) * 0.5) > VR_MELEE_ARC_SAG * xR)
        {
            VR_Melee_RibAdd(xP0, mid, xI, xR);
            VR_Melee_RibAdd(mid, xP1, xI, xR);
            return;
        }
    }
    VR_Melee_RibAdd(xP0, xP1, xI, xR);
};

// The lines of `b`'s sweep, `xR` thick: its layout's points (VR_Melee_Track); with vr_melee_swept (`xSwept`) the surface
// its whole blade sweeps: between two neighbouring striking points (a blade's, an axe's head's, the crowbar's bar), as
// many lines more as keep them at most VR_MELEE_RIB_GAP thicknesses apart all along the sweep (his note: a long swing
// passed by a thing its blade went through between two points), each striking as the nearer point; and a hammer's or
// mace's head as its volume (its box, admeleehead): a grid of lines over its middle, as thick as half its depth and
// reaching its faces, each striking as the head's point.
void(entity b, float xR, float xSwept) VR_Melee_Ribs =
{
    float n = b.mh_n;
    float i;
    vr_rib_n = 0;
    vr_rib_rmax = 0;
    vr_rib_lo = '1 1 1' * 1000000;
    vr_rib_hi = '-1 -1 -1' * 1000000;
    for(i = 0; i < n; ++i)
    {
        vector p0 = b.mh_pp[i];
        vector p1 = b.mh_cp[i];
        VR_Melee_ArcRib(b, p0, p1, i, xR);
        if(!xSwept || i + 1 >= n || !VR_Melee_Strikes(vr_mpt_kind[i]) || !VR_Melee_Strikes(vr_mpt_kind[i + 1]))
        {
            continue;
        }
        vector q0 = b.mh_pp[i + 1];
        vector q1 = b.mh_cp[i + 1];
        float more = min(VR_MELEE_RIB_MORE, ceil(max(vlen(q0 - p0), vlen(q1 - p1)) / (VR_MELEE_RIB_GAP * xR)) - 1);
        float j;
        for(j = 1; j <= more; ++j)
        {
            float u = j / (more + 1);
            VR_Melee_RibAdd(p0 + (q0 - p0) * u, p1 + (q1 - p1) * u, u < 0.5 ? i : i + 1, xR);
        }
    }
    if(!xSwept || n <= 0 || b.mh_hd_hi == '0 0 0')
    {
        return;
    }
    vector lo = b.mh_hd_lo;
    vector hi = b.mh_hd_hi;
    float len = vlen(b.mh_hd_cf - b.mh_hd_cg);
    float plen = vlen(b.mh_hd_pf - b.mh_hd_pg);
    if(len < 1 || plen < 1)
    {
        return;
    }
    float thick = max(xR, hi_z * len);
    float inset = thick / len;
    float na = VR_Melee_HeadLines(lo_x, hi_x, inset);
    float ny = VR_Melee_HeadLines(lo_y, hi_y, inset);
    vector cn = normalize(VR_Cross(b.mh_hd_cy, b.mh_hd_cf - b.mh_hd_cg)) * len;
    vector pn = normalize(VR_Cross(b.mh_hd_py, b.mh_hd_pf - b.mh_hd_pg)) * plen;
    float ia;
    float iy;
    for(ia = 0; ia < na; ++ia)
    {
        float sa = VR_Melee_HeadAt(lo_x, hi_x, inset, ia, na);
        for(iy = 0; iy < ny; ++iy)
        {
            float sy = VR_Melee_HeadAt(lo_y, hi_y, inset, iy, ny);
            VR_Melee_RibAdd(b.mh_hd_pg + (b.mh_hd_pf - b.mh_hd_pg) * sa + pn * sy,
                            b.mh_hd_cg + (b.mh_hd_cf - b.mh_hd_cg) * sa + cn * sy, n - 1, thick);
        }
    }
};

// vr_melee_swept: how squarely a line of `b` swept from `xP0` to `xP1` crosses `e` (lower is better): its nearest to the
// thing's middle, a share of its half size; the blade proper's lines (from VR_MELEE_BLADE_FROM out: a blade cuts with
// its outer part, a fist with its knuckles) before those nearer the hand, and a line already in it (`xIn`: it didn't
// enter it) after any that entered.
float(entity b, entity e, vector xP0, vector xP1, float xIn) VR_Melee_RibScore =
{
    vector c = (e.absmin + e.absmax) * 0.5;
    vector line = b.mh_far - b.mh_grip;
    float inner = (xP1 - b.mh_grip) * line < VR_MELEE_BLADE_FROM * (line * line);
    return (xIn ? 2 : 0) + (inner ? 1 : 0) + VR_PointSegmentDistance(c, xP0, xP1) / max(1, vlen(e.absmax - e.absmin) * 0.5);
};

// Show Melee Blades (vr_debug_melee_blades, Debug > Views; vr_meleeshow.hpp). Fast enough: VR_Melee_Decide would land a blow of its outermost striking point now (the speed its kind needs, the arm
// driving it, not drawn back, wiggled or turned in place); what it sets on `b` is put back (the blow's own comes after).
float(float xHand, entity b) VR_Melee_ShowFast =
{
    float i;
    float tip = -1;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[i]))
        {
            tip = i;
        }
    }
    if(tip < 0 || b.mh_fast < VR_Melee_MinSpeedOf(b) * 0.5)
    {
        return FALSE;
    }
    float kind = b.mh_kind, strength = b.mh_strength, speed = b.mh_speed, dmgspeed = b.mh_dmgspeed, core = b.mh_core;
    string part = b.mh_part, way = b.mh_way;
    float fast = VR_Melee_Decide(xHand, b, tip, vr_mpt_kind[tip]);
    b.mh_kind = kind;
    b.mh_strength = strength;
    b.mh_speed = speed;
    b.mh_dmgspeed = dmgspeed;
    b.mh_core = core;
    b.mh_part = part;
    b.mh_way = way;
    return fast;
};

// The sweep of `xHand` this frame to the view: its blow line, whether it is fast enough to strike (VR_Melee_ShowFast),
// the lines VR_Melee_Ribs made, a head's box. Only while the view is on.
void(float xHand, entity b) VR_Melee_Show =
{
    meleeshow(xHand, 0, b.mh_grip, b.mh_far, VR_Melee_ShowFast(xHand, b));
    float k;
    vector from = '0 0 0'; // (its first striking point: where its blade starts, Edit Blade Shape's Blade Start)
    for(k = 0; k < vr_mpt_n && from == '0 0 0'; ++k)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[k]) && vr_mpt_f[k] > 0)
        {
            from = b.mh_cp[k];
        }
    }
    meleeshow(xHand, 1, b.mh_near, from, 0);
    for(k = 0; k < vr_rib_n; ++k)
    {
        meleeshow(xHand, 2, vr_rib_p0[k], vr_rib_p1[k], vr_rib_r[k]);
    }
    if(b.mh_hd_hi != '0 0 0' && cvar_hget(cvarh_vr_melee_swept))
    {
        meleeshow(xHand, 3, b.mh_hd_lo, b.mh_hd_hi, 0);
        meleeshow(xHand, 4, b.mh_hd_cg, b.mh_hd_cf, 0);
        meleeshow(xHand, 5, b.mh_hd_cy, '0 0 0', 0);
    }
};

// VR_Melee_SweepTest's probe: VR_Melee_Sweep finds the contacts and stops (no speeds, nothing armed or decided), into
// vr_msw_*.
float vr_msw_probe;
float vr_msw_more; // (VR_Melee_StrikeMore's sweep: the frame's blades already shown)
entity vr_msw_edge;
vector vr_msw_edge_at;
float vr_msw_edge_i;
vector vr_msw_wall_at;
float vr_msw_wall_i;

// Sweeps the lines of `xHand` (VR_Melee_Ribs) from the last pose to this one (VR_Melee_Track) and decides the blow: the
// contact to land on into mh_c_* and TRUE, or FALSE. Things that bleed first: of the lines striking this frame, the
// blade's (or head's, fist's) over the pommel's; with vr_melee_swept the thing the blade crossed most squarely, struck
// where the line that crossed it nearest its middle met it, as its outermost striking point in it (its kind, a stab's
// or a cut's: VR_Melee_Decide); else (0) the outermost point's. The pommel's: the earliest along the sweep. A wall only
// when nothing that bleeds is struck, and not by a point going down (a hand reaching for a holster); with
// vr_melee_swept where the blade went deepest into it (the middle of the lines as deep), else the first touch.
float(float xHand, entity b) VR_Melee_Sweep =
{
    float n = b.mh_n;
    if(n <= 0)
    {
        return FALSE;
    }
    VR_Melee_LayoutOf(b); // (this hand's: the globals hold the last hand tracked)
    vr_melee_wallwin = cvar_hget(cvarh_vr_melee_wall_hold);
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier)) * (b.mh_bwidth > 0 ? b.mh_bwidth : 1);
    float swept = cvar_hget(cvarh_vr_melee_swept);
    vector hand = b.mh_grip;
    VR_Melee_Ribs(b, r, swept);
    if(!vr_msw_probe && !vr_msw_more && cvar_hget(cvarh_vr_debug_melee_blades))
    {
        VR_Melee_Show(xHand, b);
    }

    // The best contact of each class: the striking part's (edge, fist) and the near end's (pommel, butt).
    float edgeT = 2;
    float edgeI = -1;
    entity edgeEnt = world;
    vector edgeAt = '0 0 0';
    vector edgeDir = '0 0 0';
    float edgeScore = 1000000;
    float hiltT = 2;
    float hiltI = -1;
    entity hiltEnt = world;
    vector hiltAt = '0 0 0';
    vector hiltDir = '0 0 0';
    float wallT = 2;
    float wallI = -1;
    entity wallEnt = world;
    vector wallAt = '0 0 0';
    vector wallNormal = '0 0 0';
    vector wallDir = '0 0 0';
    float wallDepth = -1000000;
    entity traceEnt = world; // (vr_melee_swept: a box target the lines' traces met first, its best line, its outermost)
    float traceOuter = -1;
    float traceScore = 1000000;
    vector traceAt = '0 0 0';
    vector traceDir = '0 0 0';
    float k;
    float i;
    vector p0;
    vector p1;
    float rr;
    float t;
    float striker;

    // Walls and brush entities (a line each, exact against the BSP; it finds monsters too). A striking point strikes a
    // wall within `r` past its sweep, as it does a monster's box widened by `r`: the hand and what it holds are stopped
    // short of walls (vr_handpose.cpp: the far end, a blade's tip or an axe's head, a unit off), so an axe swung into a
    // wall never reached it, while a fist's knuckles, 4 units ahead of the stopped hand, went in. Held off it, the hand
    // slows down at once: the wall is struck over the frames it stays against it (vr_melee_wall_hold, VR_Melee_WallHeld),
    // at the speed it came in at (VR_Melee_WallDecide), as a monster's box is over the frames a blow is in it.
    for(k = 0; k < vr_rib_n; ++k)
    {
        vr_rib_wd[k] = -1000000;
        p0 = vr_rib_p0[k];
        p1 = vr_rib_p1[k];
        if(p0 == p1)
        {
            continue;
        }
        i = vr_rib_i[k];
        rr = vr_rib_r[k];
        striker = VR_Melee_Strikes(vr_mpt_kind[i]);
        traceline(p0, p1, MOVE_HITMODEL_MELEE, self); // (a monster: its model, vr_hit_precise)
        if(trace_fraction >= 1 && !trace_startsolid)
        {
            float past = FALSE;
            if(striker)
            {
                traceline(p1, p1 + normalize(p1 - p0) * rr, TRUE, self);
                if(trace_fraction < 1 && !trace_startsolid)
                {
                    past = TRUE;
                }
            }
            trace_fraction = past ? 1 : 2; // (a wall within reach past the sweep's end: there)
        }
        // Nothing met on the way (or the sweep starts in a wall): a point still at, or in, the wall a part of this hand
        // went into a moment ago (VR_Melee_WallHeld) lands there, a frame later, as one through a monster's box does.
        float held = FALSE;
        if(trace_startsolid || trace_fraction > 1)
        {
            if(!VR_Melee_WallHeld(b, i, p0, p1, rr))
            {
                continue;
            }
            held = TRUE;
        }
        if(!held && VR_Blow_Strikable(trace_ent))
        {
            if(trace_ent == b.mh_hit_ent)
            {
                b.mh_hit_touch = VR_PlayerTime();
            }
            if(vr_mpt_kind[i] == VR_MPT_GRIP || (trace_ent == b.mh_struck && VR_Melee_StartsIn(trace_ent, p0, p1, rr)))
            {
                continue; // (a blow enters what it strikes: below)
            }
            if(swept && VR_Melee_BoxTarget(trace_ent))
            {
                // (Measured with its box's below: kept for the one thing a line found first.)
                if(striker && (traceEnt == world || traceEnt == trace_ent) && (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
                {
                    float tsc = VR_Melee_RibScore(b, trace_ent, p0, p1, FALSE);
                    traceEnt = trace_ent;
                    if(traceOuter < 0 || fabs(vr_mpt_f[i]) > fabs(vr_mpt_f[traceOuter]))
                    {
                        traceOuter = i;
                    }
                    if(tsc < traceScore)
                    {
                        traceScore = tsc;
                        traceAt = trace_endpos;
                        traceDir = normalize(p1 - p0);
                    }
                }
                continue;
            }
            float sc = swept ? VR_Melee_RibScore(b, trace_ent, p0, p1, FALSE) : 0;
            float first = trace_fraction < edgeT;
            if(swept)
            {
                first = sc < edgeScore;
            }
            if(striker && first &&
               (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
            {
                edgeT = trace_fraction;
                edgeScore = sc;
                edgeI = i;
                edgeEnt = trace_ent;
                edgeAt = trace_endpos;
                edgeDir = normalize(p1 - p0);
            }
            else if(!striker && trace_fraction < hiltT && (vr_msw_probe || VR_Melee_Armed(xHand, b, trace_ent)))
            {
                hiltT = trace_fraction;
                hiltI = i;
                hiltEnt = trace_ent;
                hiltAt = trace_endpos;
                hiltDir = normalize(p1 - p0);
            }
        }
        else if(vr_mpt_kind[i] != VR_MPT_GRIP)
        {
            // How far past the surface the line went (held off it, within reach: under 0).
            float depth = (trace_endpos - p1) * trace_plane_normal;
            float better = trace_fraction < wallT;
            if(swept)
            {
                // (As deep: a striking part's over the pommel's, then the first.)
                better = depth > wallDepth + 0.01;
                if(!better && depth > wallDepth - 0.01)
                {
                    better = ((striker && wallI >= 0 && !VR_Melee_Strikes(vr_mpt_kind[wallI])) ||
                              (striker == VR_Melee_Strikes(vr_mpt_kind[max(0, wallI)]) && trace_fraction < wallT));
                }
            }
            float entered = FALSE;
            if(better || swept)
            {
                entered = (held || vr_msw_probe);
                if(!entered)
                {
                    entered = VR_Melee_WallEntered(b, i, p0, p1, trace_plane_normal, trace_endpos);
                }
            }
            if(entered)
            {
                vr_rib_wd[k] = depth;
                vr_rib_wat[k] = trace_endpos;
                vr_rib_wn[k] = trace_plane_normal;
                if(better)
                {
                    wallT = trace_fraction;
                    wallDepth = depth;
                    wallI = i;
                    wallEnt = trace_ent;
                    wallAt = trace_endpos;
                    wallNormal = trace_plane_normal;
                    wallDir = normalize(p1 - p0);
                }
            }
        }
    }
    // The wall where the blade crossed it: the middle of the lines that went as deep into the same surface (a flat
    // chop at the floor: the blade's middle, not its first point).
    if(swept && wallI >= 0)
    {
        vector sum = '0 0 0';
        float cnt = 0;
        for(k = 0; k < vr_rib_n; ++k)
        {
            if(vr_rib_wd[k] >= wallDepth - 1 && vr_rib_wn[k] * wallNormal > 0.9 &&
               VR_Melee_Strikes(vr_mpt_kind[vr_rib_i[k]]) == VR_Melee_Strikes(vr_mpt_kind[wallI]))
            {
                sum = sum + vr_rib_wat[k];
                cnt = cnt + 1;
            }
        }
        if(cnt > 1)
        {
            wallAt = sum * (1 / cnt);
        }
    }

    // Monsters' boxes, widened by the lines' thickness (and not through a wall); and loose gibs and heads (on the
    // floor, flying): not solid (SOLID_NOT_BUT_TOUCHABLE: the traces above pass them; only shots trace with
    // MOVE_HITGIBS), struck as a monster's box, and burst (VR_Gib_Blow). A gib held in a hand is VR_Gib_HeldFrame's.
    // Precise hits (vr_hit_precise): a monster's (or corpse's) box, grown by VR_HITMODEL_REACH, is only the broad phase:
    // a line strikes where its sweep meets the model as drawn, grown by its thickness and the melee tolerance
    // (hitmodel_segment); through the box beside the model it misses.
    vector mid = (b.mh_pp[0] + b.mh_cp[0] + b.mh_pp[n - 1] + b.mh_cp[n - 1]) * 0.25;
    float reach = max(max(vlen(b.mh_pp[0] - mid), vlen(b.mh_cp[0] - mid)),
                      max(vlen(b.mh_pp[n - 1] - mid), vlen(b.mh_cp[n - 1] - mid)));
    entity e = findradius(mid, reach + vr_rib_rmax + 96);
    float corpses = VR_Melee_Multi(b); // (corpses too: VR_Melee_CorpseTarget)
    while(e)
    {
        // A small gib just torn out: struck only to burst (vr_smallgibs_blow_burst), never batted (VR_SmallGib_BlowSpared).
        if((VR_Blow_Strikable(e) || VR_SmallGib_BlowBursts(e)) &&
           (VR_Melee_BoxTarget(e) || (corpses && VR_Melee_CorpseTarget(e))))
        {
            vector lo = e.origin + e.mins;
            vector hi = e.origin + e.maxs;
            float armed = vr_msw_probe || VR_Melee_Armed(xHand, b, e);
            float precise = hitmodel_target(e);
            float boxPad = 0;
            if(precise)
            {
                boxPad = VR_HITMODEL_REACH;
            }
            // (The box round all the lines first: most things near are nowhere near them.)
            float near = !swept || (lo_x - boxPad <= vr_rib_hi_x && hi_x + boxPad >= vr_rib_lo_x &&
                                    lo_y - boxPad <= vr_rib_hi_y && hi_y + boxPad >= vr_rib_lo_y &&
                                    lo_z - boxPad <= vr_rib_hi_z && hi_z + boxPad >= vr_rib_lo_z);
            vector ctr = (lo + hi) * 0.5;
            float eScore = 1000000;
            float eOuter = -1;
            vector eAt = '0 0 0';
            vector eDir = '0 0 0';
            for(k = 0; near && k < vr_rib_n; ++k)
            {
                p0 = vr_rib_p0[k];
                p1 = vr_rib_p1[k];
                i = vr_rib_i[k];
                rr = vr_rib_r[k];
                float pad = rr + boxPad;
                t = VR_SegmentBox(p0, p1, lo - '1 1 1' * pad, hi + '1 1 1' * pad);
                if(t < 0)
                {
                    continue;
                }
                if(precise)
                {
                    t = hitmodel_segment(e, p0, p1, rr, HITCLASS_MELEE);
                    if(t < 0)
                    {
                        continue;
                    }
                }
                if(!armed || vr_mpt_kind[i] == VR_MPT_GRIP)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = VR_PlayerTime(); // still in what it struck
                    }
                    if(!armed)
                    {
                        break;
                    }
                    continue;
                }
                // A blow strikes again what it struck only entering it: a point whose sweep starts in it (a fist or a
                // blade still in the thing after a blow, drawn back or turned about in it: his note 2026-10-12, a
                // retraction striking again) strikes nothing; its sweep from outside, even through it in one frame,
                // does. (Not a first blow's: his punches and thrusts start their last frame within a point's reach.)
                if(t <= 0 && e == b.mh_struck)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = VR_PlayerTime();
                    }
                    continue;
                }
                striker = VR_Melee_Strikes(vr_mpt_kind[i]);
                float rsc = 0;
                if(swept && striker)
                {
                    // The line that crossed it most squarely is where it lands; the outermost striking point in it
                    // what struck (its kind).
                    rsc = VR_Melee_RibScore(b, e, p0, p1, t <= 0);
                    if(rsc >= eScore && eOuter >= 0 && fabs(vr_mpt_f[i]) <= fabs(vr_mpt_f[eOuter]))
                    {
                        continue;
                    }
                }
                // Of the striking points in it, the outermost (a blade strikes with its outer part, its root may have been
                // in the thing for a while: the furthest from the grip, either way along a weapon held anywhere); of the
                // near end's, the first along the sweep.
                else if((striker && edgeI >= 0 && fabs(vr_mpt_f[i]) < fabs(vr_mpt_f[edgeI])) || (!striker && t >= hiltT))
                {
                    continue;
                }
                // On the box itself (precise: where the sweep met the model; a head's line: its face, not its middle).
                vector at = p0 + (p1 - p0) * t;
                if(!precise)
                {
                    at_x = min(hi_x, max(lo_x, at_x));
                    at_y = min(hi_y, max(lo_y, at_y));
                    at_z = min(hi_z, max(lo_z, at_z));
                }
                else if(rr > r)
                {
                    at = at + normalize(ctr - at) * min(rr - r, vlen(ctr - at));
                }
                traceline(hand, at, TRUE, self);
                if(trace_fraction < 1)
                {
                    continue;
                }
                if(striker && swept)
                {
                    if(eOuter < 0 || fabs(vr_mpt_f[i]) > fabs(vr_mpt_f[eOuter]))
                    {
                        eOuter = i;
                    }
                    if(rsc < eScore)
                    {
                        eScore = rsc;
                        eAt = at;
                        eDir = normalize(p1 - p0);
                    }
                }
                else if(striker)
                {
                    edgeT = t;
                    edgeI = i;
                    edgeEnt = e;
                    edgeAt = at;
                    edgeDir = normalize(p1 - p0);
                }
                else
                {
                    hiltT = t;
                    hiltI = i;
                    hiltEnt = e;
                    hiltAt = at;
                    hiltDir = normalize(p1 - p0);
                }
            }
            if(eOuter >= 0 && eScore < edgeScore)
            {
                edgeScore = eScore;
                edgeT = 0;
                edgeI = eOuter;
                edgeEnt = e;
                edgeAt = eAt;
                edgeDir = eDir;
            }
        }
        e = e.chain;
    }
    // (The thing the traces met: its outermost striking point and best line with its box's.)
    if(traceEnt != world && (edgeEnt == traceEnt || traceScore < edgeScore))
    {
        if(edgeEnt != traceEnt || fabs(vr_mpt_f[traceOuter]) > fabs(vr_mpt_f[edgeI]))
        {
            edgeI = traceOuter;
        }
        if(edgeEnt != traceEnt || traceScore < edgeScore)
        {
            edgeScore = traceScore;
            edgeAt = traceAt;
            edgeDir = traceDir;
        }
        edgeEnt = traceEnt;
        edgeT = 0;
    }

    if(vr_msw_probe)
    {
        vr_msw_edge = edgeEnt;
        vr_msw_edge_at = edgeAt;
        vr_msw_edge_i = edgeI;
        vr_msw_wall_at = wallAt;
        vr_msw_wall_i = wallI;
        return FALSE;
    }

    // The striking part first. The pommel's touch waits VR_Melee_PommelWait for the blade proper to reach the
    // same thing: then the blow is the blade's (a cut whose hands arrive first); else it lands as the pommel strike it
    // was.
    if(edgeI >= 0 && VR_Melee_Decide(xHand, b, edgeI, vr_mpt_kind[edgeI]) && VR_Melee_SlapLands(b, edgeEnt))
    {
        if(b.mh_pend_ent == world || b.mh_pend_ent != edgeEnt || vr_mpt_f[edgeI] >= VR_MELEE_BLADE_FROM)
        {
            b.mh_pend_ent = world;
            b.mh_c_ent = edgeEnt;
            b.mh_c_pos = edgeAt;
            b.mh_c_dir = edgeDir;
            b.mh_c_normal = b.mh_c_dir * -1;
            return TRUE;
        }
    }
    if(hiltI >= 0 && b.mh_pend_ent == world && VR_Melee_Decide(xHand, b, hiltI, vr_mpt_kind[hiltI]))
    {
        b.mh_c_ent = hiltEnt;
        b.mh_c_pos = hiltAt;
        b.mh_c_dir = hiltDir;
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_pend_ent = hiltEnt;
        b.mh_pend_time = VR_PlayerTime();
        b.mh_pend_pos = hiltAt;
        b.mh_pend_dir = b.mh_c_dir;
        b.mh_pend_kind = b.mh_kind;
        b.mh_pend_strength = b.mh_strength;
        b.mh_pend_speed = b.mh_speed;
        b.mh_pend_dmgspeed = b.mh_dmgspeed;
        b.mh_pend_core = b.mh_core;
        b.mh_pend_part = b.mh_part;
        b.mh_pend_way = b.mh_way;
    }
    if(b.mh_pend_ent != world && VR_PlayerTime() - b.mh_pend_time >= VR_Melee_PommelWait())
    {
        // The blade never came: the pommel strike.
        b.mh_c_ent = b.mh_pend_ent;
        b.mh_c_pos = b.mh_pend_pos;
        b.mh_c_dir = b.mh_pend_dir;
        b.mh_c_normal = b.mh_c_dir * -1;
        b.mh_kind = b.mh_pend_kind;
        b.mh_strength = b.mh_pend_strength;
        b.mh_speed = b.mh_pend_speed;
        b.mh_dmgspeed = b.mh_pend_dmgspeed;
        b.mh_core = b.mh_pend_core;
        b.mh_part = b.mh_pend_part;
        b.mh_way = b.mh_pend_way;
        b.mh_pend_ent = world;
        return b.mh_c_ent.takedamage && VR_Melee_Armed(xHand, b, b.mh_c_ent);
    }
    if(edgeI < 0 && hiltI < 0 && wallI >= 0 && VR_Melee_WallArmed(xHand, b) &&
       (vr_melee_wallwin <= 0 || VR_PlayerTime() - b.mh_wall_blow >= VR_MELEE_WALL_AGAIN) &&
       VR_Melee_WallDecide(xHand, b, wallI) && VR_Melee_SlapLands(b, wallEnt) && VR_Melee_WallFresh(b, wallNormal, wallAt))
    {
        b.mh_c_ent = wallEnt;
        b.mh_c_pos = wallAt;
        b.mh_c_dir = wallDir;
        b.mh_c_normal = wallNormal;
        b.mh_c_blunt = vr_mpt_kind[wallI] != VR_MPT_EDGE;
        return TRUE;
    }
    return FALSE;
};

// The contact the blow of `xHand` lands on (VR_Melee_Sweep), in the trace_ globals and v_forward (the striking
// point's way), as a traceline's hit (VRDoMeleeAttackTracelineFor, weapons.qc).
float(float xHand) VR_Blow_Contact =
{
    entity b = VR_Blow(xHand);
    if(b.mh_c_ent == world && b.mh_c_pos == '0 0 0')
    {
        return FALSE;
    }
    trace_ent = b.mh_c_ent;
    trace_endpos = b.mh_c_pos;
    trace_plane_normal = b.mh_c_normal;
    v_forward = b.mh_c_dir;
    trace_fraction = 0.5;
    trace_startsolid = FALSE;
    trace_allsolid = FALSE;
    return TRUE;
};

// The damage multiplier of the blow `xHand` is dealing (VRMeleeDmg).
float(float xHand) VR_Blow_Strength =
{
    return VR_Blow(xHand).mh_strength;
};

// And a counter's (VR_Counter_Use; VRMeleeDmg): 1 for a blow that isn't one.
float(float xHand) VR_Blow_Counter =
{
    float c = VR_Blow(xHand).mh_counter;
    return c > 0 ? c : 1;
};

// And for want of stamina (VR_Stamina_Strike; VRMeleeDmg, VR_Gib_Blow): 1 for a blow paid in full.
float(float xHand) VR_Blow_Exhaust =
{
    float c = VR_Blow(xHand).mh_exhaust;
    return c > 0 ? c : 1;
};

string(float xKind, float xThing) VR_Melee_KindName =
{
    if(xKind == VR_MKIND_STAB)
    {
        return xThing == VR_MTHING_SWORD ? "stab" : "jab";
    }
    if(xKind == VR_MKIND_POMMEL)
    {
        return xThing == VR_MTHING_GUN ? "butt strike" : (xThing == VR_MTHING_CROWBAR ? "chisel strike" : "pommel strike");
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xKind == VR_MKIND_SLAP)
    {
        return "slap";
    }
    if(xThing == VR_MTHING_SWORD)
    {
        return "slash";
    }
    if(xThing == VR_MTHING_AXE)
    {
        return "chop";
    }
    if(xThing == VR_MTHING_SAW)
    {
        return "cut";
    }
    if(xThing == VR_MTHING_CROWBAR)
    {
        return "strike";
    }
    return "swing";
};

// The sound of a blow landing on something that takes damage (weapons.qc: the weapons' melee): `xSound`, the weapon's
// own, or for a pommel, hilt or butt strike a blunt knock of its own (vr/pommel1..3.wav, a little apart in pitch; made by
// make_sounds.py; one of the two not played last), apart from the blades' cuts and the punches. vr_melee_blunt is set
// around the weapons' melee by PlayerVRMeleeImpl (client.qc) for the blow being struck. `developer` prints which.
float vr_melee_blunt;
float vr_melee_pommel_last; // 1..3 (0: none yet)
float vr_melee_slap_last;   // 1..3 (0: none yet)
string(string xSound) VR_Melee_HitSound =
{
    if(vr_melee_slap)
    {
        float j = min(3, 1 + floor(random() * 3));
        if(vr_melee_slap_last)
        {
            j = vr_melee_slap_last + (random() < 0.5 ? 1 : 2);
            if(j > 3)
            {
                j = j - 3;
            }
        }
        vr_melee_slap_last = j;
        xSound = j == 1 ? "vr/slap1.wav" : (j == 2 ? "vr/slap2.wav" : "vr/slap3.wav");
        dprint(sprintf("melee sound: %s\n", xSound));
        return xSound;
    }
    if(vr_melee_blunt)
    {
        float k = min(3, 1 + floor(random() * 3));
        if(vr_melee_pommel_last)
        {
            k = vr_melee_pommel_last + (random() < 0.5 ? 1 : 2);
            if(k > 3)
            {
                k = k - 3;
            }
        }
        vr_melee_pommel_last = k;
        xSound = k == 1 ? "vr/pommel1.wav" : (k == 2 ? "vr/pommel2.wav" : "vr/pommel3.wav");
    }
    dprint(sprintf("melee sound: %s\n", xSound));
    return xSound;
};

// The category of a blow for the motion evaluation: slash, stab, pommel, punch, gun.
string(float xKind, float xThing) VR_Melee_Category =
{
    if(xThing == VR_MTHING_GUN)
    {
        return "gun";
    }
    if(xKind == VR_MKIND_STAB)
    {
        return "stab";
    }
    if(xKind == VR_MKIND_POMMEL)
    {
        return "pommel";
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xKind == VR_MKIND_SLAP)
    {
        return "slap";
    }
    return "slash";
};

// The least speed of a blow by `xHand`'s far end moving along `xVel` (a punch's by its way; a weapon's swing).
float(float xHand, entity b, vector xVel) VR_Melee_SwingNeed =
{
    float minSpeed = VR_Melee_MinSpeedOf(b);
    if(b.mh_thing != VR_MTHING_FIST)
    {
        return minSpeed * VR_MELEE_ARM;
    }
    float s = vlen(xVel);
    float vz = s > 0 ? xVel_z / s : 0;
    return minSpeed * (1 + VR_MELEE_UP * max(0, vz) + VR_MELEE_DOWN * max(0, -vz - VR_MELEE_DOWN_FROM));
};

// The speed at which a blow by `xHand` along `xVel` lands (and whooshes): its least speed, more for a punch or a gun's swing
// (VR_MELEE_PUNCH_LAND, VR_MELEE_GUN_LAND: too weak to count below).
float(float xHand, entity b, vector xVel) VR_Melee_LandNeed =
{
    float need = VR_Melee_SwingNeed(xHand, b, xVel);
    if(b.mh_thing == VR_MTHING_FIST)
    {
        return need * VR_MELEE_PUNCH_LAND;
    }
    if(b.mh_thing == VR_MTHING_GUN)
    {
        return need * VR_MELEE_GUN_LAND;
    }
    return need;
};

float vr_melee_whoosh_alt; // the swish played last (VR_Melee_Whoosh): the knights' two swings taken in turn
