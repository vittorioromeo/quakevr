// vr_melee.qc -- melee for tracked hands: one model for swords, axes, guns and bare hands (docs/vr-port/ROUND21.md,
// "Melee, redesigned").
//
// WHAT A HAND HOLDS is laid out as a line: its near end (a sword's pommel, the end of an axe's handle, a gun's butt),
// its grip (the hand) and its far end (the muzzle point of its model: a sword's tip, an axe's head, a gun's muzzle; a
// fist's knuckles). Points along it, each of one kind (VR_MPT_*), are swept from the last pose to this one against
// what bleeds and against walls.
//
// A HIT is a point sweeping into something while the part it belongs to moves fast enough (vr_melee_speed), relative
// to the head (walking and turning are not the hand's motion). The direction doesn't matter. A blade (or an axe's
// head, a gun's barrel) is as fast as the far end: a swing is as fast as its tip. The pommel, hilt, butt and knuckles
// are as fast as they move themselves. What kind of blow it is follows from which part hit and how it moved
// (VR_Melee_Decide): a slash or chop or gun swing (the far end sweeping across the line), a stab or jab (driven
// along the line, the far end first), a pommel or butt strike (the near end or the grip leading, the weapon moving
// whole), a punch. A hilt or pommel reaching a monster while the blade sweeps past much faster lands nothing: the
// blade does, a moment later.
//
// THE PARRY STANCE is the held thing presented across (the parry's pose: VR_Parry_Blocks, combat.qc), or an open palm
// facing ahead. A BASH is the stance pushed forward, as a whole (its two ends moving alike), a few centimetres: then
// what is just ahead is knocked back (VR_Bash). Two hands (a weapon held two-handed, an open palm pushing on a
// one-handed weapon, both palms) bash harder.
//
// The numbers are in one place, below: tune them against the labelled takes (vr_motion_eval), not case by case.

// ----------------------------------------------------------------------------
// The model's numbers

// Strikes.
float VR_MELEE_SWING = 1.25;       // a weapon's swing: its far end's least speed, times vr_melee_speed (a head swung round is fast)
float VR_MELEE_STAB_SPEED = 0.75;  // a stab's (jab's) least speed, times vr_melee_speed (a thrust needs less than a swing)
float VR_MELEE_UP = 0.8;           // a punch's least speed grows by this share for a blow going straight up (the author's
                                   // uppercuts go 6-9 m/s, a fist wiggled upwards 4.5-5.5),
float VR_MELEE_DOWN = 1.5;         // and by this share for each unit (sine) of steepness downwards beyond VR_MELEE_DOWN_FROM
float VR_MELEE_DOWN_FROM = 0.4;    // (a hammer blow falls easily: his overhead punches go 12 m/s, a hand waved down 6)
float VR_MELEE_RUN = 0.15;         // m the striking part travelled towards the contact (its way within VR_MELEE_RUN_TURN,
float VR_MELEE_RUN_TURN = 0.34;    // cosine, faster than VR_MELEE_RUN_SLOW): a blow, not a wiggle (his wiggles: 5-12 cm)
float VR_MELEE_RUN_SLOW = 0.5;     // m/s
float VR_MELEE_RUN_TIME = 0.4;     // s looked back at most
float VR_MELEE_GOOD = 2;           // times the least speed: a blow of strength 1
float VR_MELEE_WEAKEST = 0.5;      // the strength at the least speed
float VR_MELEE_STRONGEST = 2;      // and the most
float VR_MELEE_ALONG = 0.75;       // cosine: the far end moving within ~41 degrees of the line, outwards, is driven along it
float VR_MELEE_WHOLE = 0.5;        // a thrust or pommel strike: the far end's motion differs from the grip's by less than this share
float VR_MELEE_SWEEP = 1.6;        // the far end this much faster than a hilt's or pommel's contact (plus 1 m/s): a swing passing
float VR_MELEE_REARM_SLOW = 0.5;   // a hand strikes the same thing again once its part slowed below this share of the least speed,
float VR_MELEE_REARM_TURN = -0.17; // or, out of it, turned back more than 100 degrees from the blow (cosine: a backswing),
float VR_MELEE_REARM_TIME = 0.6;   // or, out of it, after this long (s)
float VR_MELEE_REARM_OUT = 0.1;    // s since the hand's parts last touched what it struck: out of it
float VR_MELEE_RADIUS = 3;         // units around a point's sweep that it strikes (times vr_melee_range_multiplier)
float VR_MELEE_JUMP = 20;          // m/s: the grip moving faster than this jumped (a teleport, the play space re-based)
float VR_MELEE_TIPJUMP = 60;       // m/s: likewise the far end (a weapon taken)
float VR_MELEE_WALL_DOWN = -0.35;  // a point going down more steeply than this (sine) doesn't strike walls (reaching for a holster)
float VR_MELEE_POMMEL_SWORD = 0.6; // weights: a sword's pommel or hilt strike,
float VR_MELEE_POMMEL_AXE = 0.5;   // an axe's or Mjolnir's handle,
float VR_MELEE_GUARD = 0.85;       // a sword's guard in a pommel strike

// The stance and the bash.
float VR_BASH_AHEAD = 0.75;       // cosine: a push within ~41 degrees of the facing
float VR_BASH_WHOLE = 0.6;        // the stance's two ends move alike: their difference under this share of the push,
float VR_BASH_WHOLE_SLACK = 0.3;  // plus this (m/s)
float VR_BASH_TRAVEL = 0.06;      // m pushed that way before it is a bash
float VR_BASH_GAP = 0.08;         // s a push may falter and go on
float VR_BASH_REACH = 0.75;       // m ahead of the stance a bash reaches
float VR_BASH_PAIR = 0.6;         // the second hand pushes at this share of vr_bash_speed
float VR_BASH_PAIR_NEAR = 0.2;    // m: an open palm this near a one-handed weapon's line pushes it with the weapon's hand
float VR_BASH_ONE_DAMAGE = 0.6;   // one hand: this share of the damage,
float VR_BASH_ONE_PUSH = 0.7;     // and of the knockback
float VR_BASH_PALM = 0.6;         // cosine: an open palm facing ahead (within 53 degrees)
float VR_BASH_PALM_ALONG = 0.7;   // cosine: moving the way it faces (within 45 degrees)
float VR_BASH_PALM_RADIAL = 0.82; // cosine: a shove extends the arm: the palm moving out from its shoulder (his shoves:
                                  // 0.84-0.98; his hands waved down or round at the dummy, palm first: 0.55-0.8)
float VR_SHOULDER_DOWN = 0.28;    // m: a shoulder below the eyes,
float VR_SHOULDER_SIDE = 0.2;     // and to the side
float VR_BASH_COOLDOWN = 0.8;     // s after a bash lands before the next
float VR_PARRYBASH_WINDOW = 1.5;  // s: a weapon bash this soon after a parry is a parry-bash
float VR_PARRYBASH_SCALE = 1.3;   // how much harder
float VR_BASH_AIM = 45;           // degrees: a batted projectile's thrower this near the facing gets it back
float VR_STANCE_REPORT = 0.2;     // s a stance is held before it is reported (developer: "melee event: stance")

// ----------------------------------------------------------------------------
// What a hand holds

float VR_MTHING_FIST = 0;  // a fist (a box in hand too) or an open hand
float VR_MTHING_SWORD = 1; // a knight's sword
float VR_MTHING_AXE = 2;   // the axe, Mjolnir: a short handle and a head
float VR_MTHING_GUN = 3;   // a gun: blunt all along (held, or carried by its pump)
float VR_MTHING_NONE = 4;  // the empty hand steadying the other's weapon, a climbing hand: strikes nothing

// The kinds of points (what they strike as).
float VR_MPT_EDGE = 0;  // the blade, an axe's head, a gun's barrel and muzzle: moves with the far end
float VR_MPT_GRIP = 1;  // the hilt, the guard, the hand on a handle or a gun's grip
float VR_MPT_NEAR = 2;  // the pommel, the handle's end, a gun's butt
float VR_MPT_FIST = 3;  // the fist and the knuckles

// The kinds of blows.
float VR_MKIND_SLASH = 0;  // a swing's blade, an axe's head, a gun's barrel sweeping across
float VR_MKIND_STAB = 1;   // driven along the line, the far end first
float VR_MKIND_POMMEL = 2; // the near end or the grip leading, the weapon moving whole
float VR_MKIND_PUNCH = 3;  // a fist (any way: jab, hook, uppercut, overhead)

// The hand's state, on an entity of its own (the player's vr_blow_main / vr_blow_off).
.entity vr_blow_main;
.entity vr_blow_off;
.float mh_time;        // when the last new pose was taken
.float mh_dt;          // s since the one before
.float mh_thing;       // VR_MTHING_*
.float mh_wid;         // the weapon (another: nothing to sweep from)
.float mh_n;           // points (0: none yet)
.vector mh_pp[9];      // the points at the pose before (world),
.vector mh_cp[9];      // and now
.vector mh_near;       // the near end, the grip and the far end (world, now)
.vector mh_grip;
.vector mh_far;
.vector mh_rnear;      // the same from the head, in the play space (metres)
.vector mh_rgrip;
.vector mh_rfar;
.vector mh_vnear;      // and their velocities (m/s, play space)
.vector mh_vgrip;
.vector mh_vfar;
.float mh_fast;        // the part's fastest (m/s): the far end, or the grip if faster
.float mh_whooshed;    // a whoosh played for this motion (until it slows)
.entity mh_hit_ent;    // the last thing struck (world: none, or a bash or a held gib spent the motion),
.vector mh_hit_dir;    // the struck part's way then (play space),
.float mh_hit_time;    // when
.float mh_hit_touch;   // the last time a part touched it (VR_MELEE_REARM_OUT)
// The blow being dealt (VR_Melee_Contact, VRMeleeDmg, the dummy's report).
.float mh_kind;        // VR_MKIND_*
.float mh_strength;    // its damage multiplier
.float mh_speed;       // the striking part's speed (m/s)
.string mh_part;       // its name ("the tip")
.string mh_way;        // how it moved, for the report ("overhead", "hook")
.entity mh_c_ent;      // where it lands, as a traceline's hit
.vector mh_c_pos;
.vector mh_c_dir;
.vector mh_c_normal;
// The stance and the push (VR_Bash).
.float mh_stance;      // 0: none, 1: a weapon across, 2: an open palm facing ahead
.float mh_stance_since;
.float mh_stance_said; // the stance was reported (developer)
.float mh_fresh;       // a new pose was swept this frame
.float mh_pushing;     // pushing as a bash does this frame
.float mh_push_dist;   // m pushed so far
.float mh_push_last;   // the last frame it pushed
// The last poses' velocities (a ring: VR_Melee_Run): the far end's, the grip's, and each pose's time step.
.vector mh_hfar[24];
.vector mh_hgrip[24];
.float mh_hdt[24];
.float mh_hi;
// Batting swings (VR_Deflect, vr_juice.qc): the lines the weapon swept through.
.vector bat_a[8];
.vector bat_b[8];
.float bat_t[8];
.float bat_i;
.float bat_newest;

.float vr_bash_time;      // on a player: when the next bash may be
.float vr_arms;           // on a player: arms crossed (the unarmed parry),
.float vr_arms_since;     // since when (0: reported)
.float vr_bash_mark;      // on a monster: found by this frame's bash (VR_Bash_Find),
.entity vr_bash_next;     // and the next one found
.float vr_bashdef_until;  // on a player: a bash's (or an armed shove's) batting goes on until then (VR_Bash_Deflect),
.float vr_bashdef_hand;   // with that hand (-1: both)
float vr_bash_counter;    // while a bash's hits are dealt: a parry-bash (vr_dummy.qc)
float vr_bash_both;       // and with two hands

// The layout of the points (VR_Melee_Layout): their place along the line (the grip 0, the far end 1; the near
// end negative), kind and name.
float vr_mpt_f[9];
float vr_mpt_kind[9];
string vr_mpt_name[9];
float vr_mpt_n;

entity(float xHand) VR_Blow =
{
    entity b = xHand == cVR_OffHand ? self.vr_blow_off : self.vr_blow_main;
    if(b == world || b.classname != "vr_blow" || b.owner != self)
    {
        b = spawn();
        b.classname = "vr_blow";
        b.owner = self;
        if(xHand == cVR_OffHand)
        {
            self.vr_blow_off = b;
        }
        else
        {
            self.vr_blow_main = b;
        }
    }
    return b;
};

// The least speed of a strike (m/s).
float() VR_Melee_MinSpeed =
{
    return max(0.5, cvar_hget(cvarh_vr_melee_speed));
};

// What `xHand` of `p` holds, as a striking thing.
float(entity p, float xHand) VR_Melee_Thing =
{
    if(VRHasBit(p.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING))
    {
        return VR_MTHING_NONE; // a hand holding a ledge pulls the body
    }
    float wid = VRGetEntWeapon(p, xHand);
    if(wid == WID_FIST)
    {
        // The empty hand steadying the other's weapon swings with it: the weapon's hand strikes.
        if(VRGet2HAiming(p) && !VRIsHandEmpty(p, VRGetOtherHand(xHand)))
        {
            return VR_MTHING_NONE;
        }
        return VR_MTHING_FIST;
    }
    if(wid == WID_SWORD)
    {
        return VR_MTHING_SWORD;
    }
    if(wid == WID_AXE || wid == WID_MJOLNIR)
    {
        return VR_MTHING_AXE;
    }
    return VR_MTHING_GUN;
};

// Where the helping hand `xHelper` of `p` holds the weapon in its other hand (two-handed): today the hand itself;
// the hand-IK's grip hotspots (agent/hands) go here.
vector(entity p, float xHelper) VR_Melee_HelpGrip =
{
    return VRGetEntHandPos(p, xHelper);
};

// The line of what `xHand` of `p` holds, into vr_mline_near, _grip, _far (world): FALSE for nothing to strike with.
vector vr_mline_near;
vector vr_mline_grip;
vector vr_mline_far;
float(entity p, float xHand, float xThing) VR_Melee_Line =
{
    vector h = VRGetEntHandPos(p, xHand);
    vr_mline_grip = h;
    if(xThing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    if(xThing == VR_MTHING_FIST)
    {
        vector fwd = VRForwardOf(VRGetEntHandRot(p, xHand));
        vr_mline_near = h - fwd * 1;
        vr_mline_far = h + fwd * 4;
        return TRUE;
    }
    vector m = xHand == cVR_OffHand ? p.offmuzzlepos : p.muzzlepos;
    if(m == '0 0 0' || vlen(m - h) > 80 || vlen(m - h) < 2)
    {
        m = h + VRForwardOf(VRGetEntHandRot(p, xHand)) * 8;
    }
    vector a = m - h;
    vr_mline_far = m;
    if(xThing == VR_MTHING_SWORD)
    {
        vr_mline_near = h - a * 0.3; // the hand closes just under the guard; the grip and the pommel below it
    }
    else if(xThing == VR_MTHING_AXE)
    {
        vr_mline_near = h - a * 0.25; // the handle's end, below the hand
    }
    else if(VRIsHandCarryingByForegrip(p, xHand))
    {
        vr_mline_near = h - a * 0.8; // held by its pump: the stock is well behind the hand
    }
    else
    {
        vr_mline_near = h - a * 0.3; // the butt behind the grip
    }
    return TRUE;
};

// A point of the layout (a macro: the constant index is a plain global).
#define VR_MPT(xIndex, xF, xKind, xName) \
    vr_mpt_f[xIndex] = xF; \
    vr_mpt_kind[xIndex] = xKind; \
    vr_mpt_name[xIndex] = xName; \
    vr_mpt_n = xIndex + 1

// The points of a thing (see the top): the whole blade strikes, from the guard to the tip.
void(float xThing) VR_Melee_Layout =
{
    vr_mpt_n = 0;
    if(xThing == VR_MTHING_FIST)
    {
        VR_MPT(0, 0, VR_MPT_FIST, "the fist");
        VR_MPT(1, 1, VR_MPT_FIST, "the knuckles");
    }
    else if(xThing == VR_MTHING_SWORD)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the pommel");
        VR_MPT(1, 0, VR_MPT_GRIP, "the hilt");
        VR_MPT(2, 0.12, VR_MPT_GRIP, "the guard");
        VR_MPT(3, 0.27, VR_MPT_EDGE, "the blade's root");
        VR_MPT(4, 0.42, VR_MPT_EDGE, "mid-blade");
        VR_MPT(5, 0.57, VR_MPT_EDGE, "mid-blade");
        VR_MPT(6, 0.72, VR_MPT_EDGE, "the blade");
        VR_MPT(7, 0.86, VR_MPT_EDGE, "the blade");
        VR_MPT(8, 1, VR_MPT_EDGE, "the tip");
    }
    else if(xThing == VR_MTHING_AXE)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the handle's end");
        VR_MPT(1, 0, VR_MPT_GRIP, "the handle");
        VR_MPT(2, 0.45, VR_MPT_GRIP, "the handle");
        VR_MPT(3, 0.75, VR_MPT_EDGE, "the head");
        VR_MPT(4, 1, VR_MPT_EDGE, "the head");
    }
    else if(xThing == VR_MTHING_GUN)
    {
        VR_MPT(0, -1, VR_MPT_NEAR, "the butt");
        VR_MPT(1, 0, VR_MPT_GRIP, "the grip");
        VR_MPT(2, 0.5, VR_MPT_EDGE, "the barrel");
        VR_MPT(3, 1, VR_MPT_EDGE, "the muzzle");
    }
};

// A point's place on the line (the layout's -1: the near end).
vector(float xF) VR_Melee_At =
{
    if(xF < 0)
    {
        return vr_mline_near;
    }
    return vr_mline_grip + (vr_mline_far - vr_mline_grip) * xF;
};

// `xVec` of the world turned into the play space (the player's turning undone). The turn's cosine and sine are kept
// for the last yaw (it changes only as the player turns).
float vr_ps_set;
float vr_ps_yaw;
float vr_ps_cos;
float vr_ps_sin;
vector(vector xVec) VR_Blow_ToPlaySpace =
{
    if(self.vryaw != vr_ps_yaw || !vr_ps_set)
    {
        float a = -self.vryaw * (3.14159265 / 180);
        vr_ps_cos = cos(a);
        vr_ps_sin = sin(a);
        vr_ps_yaw = self.vryaw;
        vr_ps_set = TRUE;
    }
    vector r;
    r_x = xVec_x * vr_ps_cos - xVec_y * vr_ps_sin;
    r_y = xVec_x * vr_ps_sin + xVec_y * vr_ps_cos;
    r_z = xVec_z;
    return r;
};

// The play space's way back to the world.
vector(vector xVec) VR_Blow_ToWorld =
{
    VR_Blow_ToPlaySpace('0 0 0'); // (the turn's cosine and sine for this yaw)
    vector r;
    r_x = xVec_x * vr_ps_cos + xVec_y * vr_ps_sin;
    r_y = -xVec_x * vr_ps_sin + xVec_y * vr_ps_cos;
    r_z = xVec_z;
    return r;
};

// The head's facing, flat, in the play space.
vector() VR_Blow_Facing =
{
    return VRForwardOf('0 1 0' * (self.v_viewangle_y - self.vryaw));
};

// Where `xHand`'s palm faces (the main hand's palm is its left, the off hand's its right, as held).
vector(float xHand) VR_Melee_Palm =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector palm = xHand == cVR_OffHand ? v_right : v_right * -1;
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return palm;
};

// Whether `xHand` of `self` is an open hand: empty, the grip not held, not steadying the other's weapon, not carrying
// or climbing.
float(float xHand) VR_Melee_OpenHand =
{
    if(!VRIsHandEmpty(self, xHand) || VR_HandGrabUtil_IsHandGrabbing(self, xHand) || VR_Carry_IsHolding(self, xHand))
    {
        return FALSE;
    }
    if(VRGet2HAiming(self) && !VRIsHandEmpty(self, VRGetOtherHand(xHand)))
    {
        return FALSE; // steadying the other's weapon
    }
    return !VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING);
};

// Prints a melee event for the logs and the motion evaluation (developer): "melee event: <what> ...".
void(string xWhat) VR_Melee_Event =
{
    dprint("melee event: ");
    dprint(xWhat);
    dprint("\n");
};
.float vr_motion_pushseen; // on a player: the last frame of a push (the motion recorder's "push" event: once a push)

// ----------------------------------------------------------------------------
// Following a hand

// Takes `xHand`'s new pose (once a frame, PlayerVRMelee): the points swept from the last pose to this one, the line's
// velocities. FALSE when there is no new pose (a server frame between two client ones: the next pose's sweep spans
// both) or nothing to sweep from (the first pose, a jump, another weapon).
float(float xHand, entity b) VR_Melee_Track =
{
    // The pose is the client's as this server frame ends (time + frametime): QC's time is still the frame's start.
    float now = time + frametime;
    float dt = now - b.mh_time;
    float thing = VR_Melee_Thing(self, xHand);
    float wid = VRGetEntWeapon(self, xHand);
    if(!VR_Melee_Line(self, xHand, thing) || !VRHandsTracked(self))
    {
        b.mh_n = 0;
        b.mh_thing = thing;
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
        b.mh_fast = 0;
        b.mh_time = now;
        return FALSE;
    }

    float m2u = VRMetersToUnits();
    vector head = VRGetHeadPos(self);
    vector rnear = VR_Blow_ToPlaySpace(vr_mline_near - head) * (1 / m2u);
    vector rgrip = VR_Blow_ToPlaySpace(vr_mline_grip - head) * (1 / m2u);
    vector rfar = VR_Blow_ToPlaySpace(vr_mline_far - head) * (1 / m2u);

    // The same pose again: no news, not a stop.
    if(rgrip == b.mh_rgrip && rfar == b.mh_rfar && dt > 0 && dt < 0.05 && thing == b.mh_thing)
    {
        return FALSE;
    }

    float same = b.mh_n > 0 && thing == b.mh_thing && wid == b.mh_wid && dt > 0 && dt <= 0.1;
    if(same)
    {
        // A jump (a teleport, the play space re-based, a weapon taken) is faster than a hand goes.
        if(vlen(rgrip - b.mh_rgrip) > max(0.3, VR_MELEE_JUMP * dt) || vlen(rfar - b.mh_rfar) > max(0.6, VR_MELEE_TIPJUMP * dt))
        {
            same = FALSE;
        }
    }
    if(same)
    {
        b.mh_vnear = (rnear - b.mh_rnear) * (1 / dt);
        b.mh_vgrip = (rgrip - b.mh_rgrip) * (1 / dt);
        b.mh_vfar = (rfar - b.mh_rfar) * (1 / dt);
    }
    else
    {
        b.mh_vnear = b.mh_vgrip = b.mh_vfar = '0 0 0';
    }
    b.mh_rnear = rnear;
    b.mh_rgrip = rgrip;
    b.mh_rfar = rfar;
    b.mh_near = vr_mline_near;
    b.mh_grip = vr_mline_grip;
    b.mh_far = vr_mline_far;
    b.mh_dt = same ? dt : 0;
    b.mh_time = now;
    b.mh_thing = thing;
    b.mh_wid = wid;
    b.mh_fast = max(vlen(b.mh_vfar), vlen(b.mh_vgrip));
    if(same)
    {
        b.mh_hi = b.mh_hi + 1;
        if(b.mh_hi >= 24)
        {
            b.mh_hi = 0;
        }
        b.mh_hfar[b.mh_hi] = b.mh_vfar;
        b.mh_hgrip[b.mh_hi] = b.mh_vgrip;
        b.mh_hdt[b.mh_hi] = dt;
    }
    else
    {
        b.mh_hdt[b.mh_hi] = 0; // (nothing to look back through)
    }

    // The points: kept from the last pose, laid out now.
    VR_Melee_Layout(thing);
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
    return same;
};

// The velocity of a point of the layout at `xF` (play space, m/s): the line moves as a rigid thing.
vector(entity b, float xF) VR_Melee_PointVel =
{
    if(xF < 0)
    {
        return b.mh_vnear;
    }
    return b.mh_vgrip + (b.mh_vfar - b.mh_vgrip) * xF;
};

// Whether `e` can be struck by the hands of `self`: it bleeds (or breaks), it is solid, and it is not the player's
// own (held or thrown).
float(entity e) VR_Blow_Strikable =
{
    return e != self && e != world && e.takedamage && e.solid != SOLID_NOT && e.solid != SOLID_TRIGGER &&
           e.owner != self && e.carry_player != self;
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

// How far the far end (or, `xGrip`, the grip) of what `b`'s hand holds has travelled towards where it goes now (m): back
// through the last poses while it kept its way (VR_MELEE_RUN_TURN) and speed (VR_MELEE_RUN_SLOW), VR_MELEE_RUN_TIME at most.
float(entity b, float xGrip) VR_Melee_Run =
{
    vector now = xGrip ? b.mh_vgrip : b.mh_vfar;
    float sn = vlen(now);
    float run = 0;
    float back = 0;
    float k = b.mh_hi;
    float n;
    for(n = 0; n < 24; ++n)
    {
        float dt = b.mh_hdt[k];
        if(dt <= 0)
        {
            break;
        }
        vector v = xGrip ? b.mh_hgrip[k] : b.mh_hfar[k];
        float s = vlen(v);
        if(s < VR_MELEE_RUN_SLOW || v * now < VR_MELEE_RUN_TURN * s * sn)
        {
            break;
        }
        run = run + s * dt;
        back = back + dt;
        if(back > VR_MELEE_RUN_TIME)
        {
            break;
        }
        k = k - 1;
        if(k < 0)
        {
            k = 23;
        }
    }
    return run;
};

// Whether the blow of `xHand` (point `xI`, of kind `xKind`, on `xEnt`) lands, and as what: sets mh_kind, mh_speed,
// mh_strength, mh_part and mh_way. FALSE: nothing (too slow, or a hilt the blade passes).
float(float xHand, entity b, float xI, float xKind) VR_Melee_Decide =
{
    float minSpeed = VR_Melee_MinSpeed();
    float f = vr_mpt_f[xI];
    vector vp = VR_Melee_PointVel(b, f);
    vector axis = normalize(b.mh_rfar - b.mh_rgrip);
    float vfar = vlen(b.mh_vfar);
    float kind = VR_MKIND_SLASH;
    float speed = vfar;
    float need = minSpeed;
    float weight = 1;
    vector way = b.mh_vfar;
    b.mh_part = vr_mpt_name[xI];

    float grip = FALSE; // (the run: the grip's, else the far end's)
    if(xKind == VR_MPT_FIST)
    {
        // A punch is a closed fist (the grip held): an open hand only shoves (VR_Bash). (The author: every punch of his
        // with the grip held; his open hands waved, flicked and patted at the dummy, some at 7-9 m/s, all "no hit".)
        if(!VRIsEntHandGrabbingBit(self, xHand))
        {
            return FALSE;
        }
        kind = VR_MKIND_PUNCH;
        speed = vfar;
        // The blow's way: rising and steeply falling blows are thrown faster for the same effort.
        float vz = vfar > 0 ? b.mh_vfar_z / vfar : 0;
        need = minSpeed * (1 + VR_MELEE_UP * max(0, vz) + VR_MELEE_DOWN * max(0, -vz - VR_MELEE_DOWN_FROM));
        weight = cvar_hget(cvarh_vr_melee_punch_mult);
        if(VR_Carry_IsHolding(self, xHand))
        {
            weight = weight * cvar_hget(cvarh_vr_carry_melee_mult); // a box in hand is an improvised weapon
        }
    }
    else if(xKind == VR_MPT_EDGE)
    {
        // Driven along the line, the far end first, the thing moving whole: a stab (a jab).
        float along = b.mh_vfar * axis;
        if(along > vfar * VR_MELEE_ALONG)
        {
            if(vlen(b.mh_vfar - b.mh_vgrip) < vfar * VR_MELEE_WHOLE)
            {
                kind = VR_MKIND_STAB;
                need = minSpeed * VR_MELEE_STAB_SPEED;
            }
        }
        if(kind == VR_MKIND_SLASH)
        {
            need = minSpeed * VR_MELEE_SWING;
        }
    }
    else
    {
        // The pommel, the hilt, the handle, a gun's grip or butt: a swing's far end sweeping past much faster is the
        // swing's (the blade lands, a moment later); else the weapon is driven whole, or pommel first.
        speed = vlen(vp);
        if(vfar > speed * VR_MELEE_SWEEP + 1)
        {
            return FALSE;
        }
        kind = VR_MKIND_POMMEL;
        way = vp;
        grip = TRUE;
        if(b.mh_thing == VR_MTHING_SWORD)
        {
            weight = xI == 2 ? VR_MELEE_GUARD * VR_MELEE_POMMEL_SWORD : VR_MELEE_POMMEL_SWORD;
        }
        else if(b.mh_thing == VR_MTHING_AXE)
        {
            weight = VR_MELEE_POMMEL_AXE;
        }
    }
    if(speed < need)
    {
        return FALSE;
    }
    // A blow, not a wiggle: the striking part came from some way off.
    float run = VR_Melee_Run(b, grip);
    if(run < VR_MELEE_RUN)
    {
        if(cvar("developer") >= 2)
        {
            dprint(sprintf("melee debug: hand %g, %.1f m/s but a run of %.2f m: a wiggle\n", xHand, speed, run));
        }
        return FALSE;
    }

    float s = VR_MELEE_WEAKEST + (1 - VR_MELEE_WEAKEST) * (speed - need) / (need * (VR_MELEE_GOOD - 1));
    b.mh_kind = kind;
    b.mh_speed = speed;
    b.mh_strength = min(VR_MELEE_STRONGEST, max(VR_MELEE_WEAKEST, s)) * weight;
    b.mh_way = VR_Melee_Way(b, way, b.mh_thing);
    return TRUE;
};

// Whether `xHand` may strike `xEnt` now: once per motion (see VR_MELEE_REARM_*); another thing at once (a swing
// through two monsters). A bash or a held gib struck spends the motion for everything (mh_hit_ent world).
float(float xHand, entity b, entity xEnt) VR_Melee_Armed =
{
    if(!VRGetMeleeHitSoundPlayed(xHand))
    {
        return TRUE;
    }
    return b.mh_hit_ent != world && xEnt != b.mh_hit_ent;
};

// Re-arms `xHand` (each frame) once its motion slowed down since the last blow, or, its parts out of what it struck,
// turned back (a backswing) or went on long enough: a swing whose arc curves on through a big monster is one blow.
void(float xHand, entity b) VR_Melee_Rearm =
{
    if(!VRGetMeleeHitSoundPlayed(xHand) || time < b.mh_hit_time + 0.1)
    {
        return;
    }
    float rearm = b.mh_fast < VR_Melee_MinSpeed() * VR_MELEE_REARM_SLOW;
    if(!rearm && time - b.mh_hit_touch > VR_MELEE_REARM_OUT)
    {
        if(time - b.mh_hit_time > VR_MELEE_REARM_TIME)
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
    }
};

// Sweeps the points of `xHand` from the last pose to this one (VR_Melee_Track) and decides the blow: the contact to
// land on into mh_c_* and TRUE, or FALSE. Things that bleed first: of the points striking this frame, the blade's
// (or head's, fist's) over the hilt's and pommel's, then the earliest along the sweep. A wall only when nothing that
// bleeds is struck, and not by a point going down (a hand reaching for a holster).
float(float xHand, entity b) VR_Melee_Sweep =
{
    float n = b.mh_n;
    if(n <= 0)
    {
        return FALSE;
    }
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    vector hand = b.mh_grip;

    // The best contact of each class: the striking part's (edge, fist) and the hilt's (grip, near end).
    float edgeT = 2;
    float edgeI = -1;
    entity edgeEnt = world;
    vector edgeAt = '0 0 0';
    float hiltT = 2;
    float hiltI = -1;
    entity hiltEnt = world;
    vector hiltAt = '0 0 0';
    float wallT = 2;
    float wallI = -1;
    entity wallEnt = world;
    vector wallAt = '0 0 0';
    vector wallNormal = '0 0 0';
    float i;
    vector p0;
    vector p1;
    float t;
    float striker;

    // Walls and brush entities (a line each, exact against the BSP; it finds monsters too).
    for(i = 0; i < n; ++i)
    {
        p0 = b.mh_pp[i];
        p1 = b.mh_cp[i];
        if(p0 == p1)
        {
            continue;
        }
        traceline(p0, p1, FALSE, self);
        if(trace_startsolid || trace_fraction >= 1)
        {
            continue;
        }
        striker = vr_mpt_kind[i] == VR_MPT_EDGE || vr_mpt_kind[i] == VR_MPT_FIST;
        if(VR_Blow_Strikable(trace_ent))
        {
            if(trace_ent == b.mh_hit_ent)
            {
                b.mh_hit_touch = time;
            }
            if(striker && trace_fraction < edgeT && VR_Melee_Armed(xHand, b, trace_ent))
            {
                edgeT = trace_fraction;
                edgeI = i;
                edgeEnt = trace_ent;
                edgeAt = trace_endpos;
            }
            else if(!striker && trace_fraction < hiltT && VR_Melee_Armed(xHand, b, trace_ent))
            {
                hiltT = trace_fraction;
                hiltI = i;
                hiltEnt = trace_ent;
                hiltAt = trace_endpos;
            }
        }
        else if(trace_fraction < wallT && p1_z - p0_z >= VR_MELEE_WALL_DOWN * vlen(p1 - p0))
        {
            wallT = trace_fraction;
            wallI = i;
            wallEnt = trace_ent;
            wallAt = trace_endpos;
            wallNormal = trace_plane_normal;
        }
    }

    // Monsters' boxes, widened by the points' thickness (and not through a wall).
    vector mid = (b.mh_pp[0] + b.mh_cp[0] + b.mh_pp[n - 1] + b.mh_cp[n - 1]) * 0.25;
    float reach = max(max(vlen(b.mh_pp[0] - mid), vlen(b.mh_cp[0] - mid)),
                      max(vlen(b.mh_pp[n - 1] - mid), vlen(b.mh_cp[n - 1] - mid)));
    entity e = findradius(mid, reach + r + 96);
    while(e)
    {
        if(VR_Blow_Strikable(e) && (e.solid == SOLID_SLIDEBOX || e.solid == SOLID_BBOX))
        {
            vector lo = e.origin + e.mins;
            vector hi = e.origin + e.maxs;
            float armed = VR_Melee_Armed(xHand, b, e);
            for(i = 0; i < n; ++i)
            {
                p0 = b.mh_pp[i];
                p1 = b.mh_cp[i];
                t = VR_SegmentBox(p0, p1, lo - '1 1 1' * r, hi + '1 1 1' * r);
                if(t < 0)
                {
                    continue;
                }
                if(!armed)
                {
                    if(e == b.mh_hit_ent)
                    {
                        b.mh_hit_touch = time; // still in what it struck
                    }
                    break;
                }
                striker = vr_mpt_kind[i] == VR_MPT_EDGE || vr_mpt_kind[i] == VR_MPT_FIST;
                if((striker && t >= edgeT) || (!striker && t >= hiltT))
                {
                    continue;
                }
                // On the box itself.
                vector at = p0 + (p1 - p0) * t;
                at_x = min(hi_x, max(lo_x, at_x));
                at_y = min(hi_y, max(lo_y, at_y));
                at_z = min(hi_z, max(lo_z, at_z));
                traceline(hand, at, TRUE, self);
                if(trace_fraction < 1)
                {
                    continue;
                }
                if(striker)
                {
                    edgeT = t;
                    edgeI = i;
                    edgeEnt = e;
                    edgeAt = at;
                }
                else
                {
                    hiltT = t;
                    hiltI = i;
                    hiltEnt = e;
                    hiltAt = at;
                }
            }
        }
        e = e.chain;
    }

    // The striking part first; the hilt's or pommel's only if the striking part lands nothing.
    if(edgeI >= 0 && VR_Melee_Decide(xHand, b, edgeI, vr_mpt_kind[edgeI]))
    {
        b.mh_c_ent = edgeEnt;
        b.mh_c_pos = edgeAt;
        b.mh_c_dir = normalize(b.mh_cp[edgeI] - b.mh_pp[edgeI]);
        b.mh_c_normal = b.mh_c_dir * -1;
        return TRUE;
    }
    if(hiltI >= 0 && VR_Melee_Decide(xHand, b, hiltI, vr_mpt_kind[hiltI]))
    {
        b.mh_c_ent = hiltEnt;
        b.mh_c_pos = hiltAt;
        b.mh_c_dir = normalize(b.mh_cp[hiltI] - b.mh_pp[hiltI]);
        b.mh_c_normal = b.mh_c_dir * -1;
        return TRUE;
    }
    if(edgeI < 0 && hiltI < 0 && wallI >= 0 && !VRGetMeleeHitSoundPlayed(xHand) &&
       VR_Melee_Decide(xHand, b, wallI, vr_mpt_kind[wallI]))
    {
        b.mh_c_ent = wallEnt;
        b.mh_c_pos = wallAt;
        b.mh_c_dir = normalize(b.mh_cp[wallI] - b.mh_pp[wallI]);
        b.mh_c_normal = wallNormal;
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

string(float xKind, float xThing) VR_Melee_KindName =
{
    if(xKind == VR_MKIND_STAB)
    {
        return xThing == VR_MTHING_SWORD ? "stab" : "jab";
    }
    if(xKind == VR_MKIND_POMMEL)
    {
        return xThing == VR_MTHING_GUN ? "butt strike" : "pommel strike";
    }
    if(xKind == VR_MKIND_PUNCH)
    {
        return "punch";
    }
    if(xThing == VR_MTHING_SWORD)
    {
        return "slash";
    }
    if(xThing == VR_MTHING_AXE)
    {
        return "chop";
    }
    return "swing";
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
    return "slash";
};

// The weapon's whoosh as `xHand`'s striking part gets going (once a motion).
void(float xHand, entity b) VR_Melee_Whoosh =
{
    float minSpeed = VR_Melee_MinSpeed();
    if(b.mh_fast < minSpeed * VR_MELEE_REARM_SLOW)
    {
        b.mh_whooshed = FALSE;
        return;
    }
    if(b.mh_whooshed || vlen(b.mh_vfar) < minSpeed || b.mh_thing == VR_MTHING_NONE)
    {
        return;
    }
    b.mh_whooshed = TRUE;
    VR_Motion_Event("stroke", VR_Melee_Category(b.mh_thing == VR_MTHING_FIST ? VR_MKIND_PUNCH : VR_MKIND_SLASH, b.mh_thing), xHand, vlen(b.mh_vfar), b.mh_far, world, "");
    string s = "knight/sword1.wav";
    if(b.mh_thing == VR_MTHING_AXE)
    {
        s = "weapons/ax1.wav";
    }
    else if(b.mh_thing == VR_MTHING_GUN && b.mh_vfar_z < -0.6 * vlen(b.mh_vfar))
    {
        return; // a gun swung down: reaching for a holster to reload is fast too
    }
    else if(b.mh_thing == VR_MTHING_FIST && VR_Melee_OpenHand(xHand))
    {
        return;
    }
    sound(self, xHand == cVR_OffHand ? CHAN_WEAPON2 : CHAN_WEAPON, s, 1, ATTN_NORM);
};

// ----------------------------------------------------------------------------
// The stance and the bash

// The stance of `xHand` of `self` facing `xForward` (world, flat): 1, a weapon held across (the parry's pose: the
// weapon's line, or the hands' line held two-handed, level, across and in front); 2, an open palm facing ahead;
// 0, none.
float(float xHand, entity b, vector xForward) VR_Bash_Stance =
{
    if(b.mh_thing == VR_MTHING_NONE)
    {
        return 0;
    }
    if(b.mh_thing == VR_MTHING_FIST)
    {
        if(VR_Melee_OpenHand(xHand) && VR_Melee_Palm(xHand) * xForward > VR_BASH_PALM)
        {
            return 2;
        }
        return 0;
    }
    return VR_Parry_Blocks(self, xHand, xForward) ? 1 : 0;
};

// The stance's velocity (play space, m/s): a weapon's middle, or the palm.
vector(entity b) VR_Bash_PushVel =
{
    if(b.mh_stance == 2)
    {
        return b.mh_vgrip;
    }
    return (b.mh_vnear + b.mh_vfar) * 0.5;
};

// Whether `xHand`'s stance is pushed as a bash is this frame, at `xSpeed` (m/s) or more: forward (play space
// `xFwd`), as a whole (a weapon's two ends moving alike; a palm moving the way it faces).
float(float xHand, entity b, vector xFwd, float xSpeed) VR_Bash_Pushing =
{
    vector v;
    if(b.mh_stance == 2)
    {
        v = b.mh_vgrip;
        vector palm = VR_Blow_ToPlaySpace(VR_Melee_Palm(xHand));
        if(v * palm < vlen(v) * VR_BASH_PALM_ALONG)
        {
            return FALSE;
        }
        // The arm extending: out from its shoulder (a hand waved down or round, palm first, is no shove).
        vector right = '0 0 0';
        right_x = xFwd_y;
        right_y = -xFwd_x;
        float side = VR_SHOULDER_SIDE;
        if((xHand == cVR_OffHand) != !!cvar("vr_lefthanded"))
        {
            side = -side;
        }
        vector out = b.mh_rgrip - (right * side - '0 0 1' * VR_SHOULDER_DOWN);
        if(v * normalize(out) < vlen(v) * VR_BASH_PALM_RADIAL)
        {
            return FALSE;
        }
        xSpeed = xSpeed * max(0.1, cvar_hget(cvarh_vr_shove_speed)) / max(0.1, cvar_hget(cvarh_vr_bash_speed));
    }
    else
    {
        v = (b.mh_vnear + b.mh_vfar) * 0.5;
        if(vlen(b.mh_vfar - b.mh_vnear) > vlen(v) * VR_BASH_WHOLE + VR_BASH_WHOLE_SLACK)
        {
            return FALSE; // turning: a swing through the stance
        }
    }
    float ahead = v * xFwd;
    return ahead >= xSpeed && ahead >= vlen(v) * VR_BASH_AHEAD;
};

// What is just ahead of the stance of `xHand` (VR_BASH_REACH, along `xForward`): the weapon's line (or the palm)
// swept forward, against monsters' boxes. Into vr_bash_victims (a chain), how many.
entity vr_bash_victims;
float(float xHand, entity b, vector xForward) VR_Bash_Find =
{
    vector a = b.mh_near;
    vector c = b.mh_far;
    if(b.mh_stance == 2)
    {
        a = b.mh_grip - xForward * 2;
        c = b.mh_grip + xForward * 2;
    }
    float reach = VR_BASH_REACH * VRMetersToUnits();
    float pad = 6;
    float hits = 0;
    vr_bash_victims = world;
    entity e = findradius((a + c) * 0.5 + xForward * reach * 0.5, reach + vlen(c - a) * 0.5 + 64);
    while(e)
    {
        if(e != self && e.takedamage && e.health > 0 && (e.flags & FL_MONSTER) && e.vr_bash_mark != time)
        {
            vector lo = e.origin + e.mins - '1 1 1' * pad;
            vector hi = e.origin + e.maxs + '1 1 1' * pad;
            float k;
            for(k = 0; k <= 3; ++k)
            {
                vector off = xForward * (reach * k / 3);
                if(VR_SegmentBox(a + off, c + off, lo, hi) >= 0)
                {
                    e.vr_bash_mark = time;
                    e.vr_bash_next = vr_bash_victims;
                    vr_bash_victims = e;
                    hits = hits + 1;
                    break;
                }
            }
        }
        e = e.chain;
    }
    return hits;
};

// Knocks `e` back along `xForward`: `xScale` of vr_bash_damage and `xPush` of vr_bash_push, and a stagger (its pain,
// forced).
void(entity e, vector xAt, vector xForward, float xKind, float xHand, float xScale, float xPush) VR_Bash_Hit =
{
    float push = cvar_hget(cvarh_vr_bash_push) * xPush;
    float dmg = cvar_hget(cvarh_vr_bash_damage) * xScale;
    VR_SetHitKind(xKind, xHand, xAt, xForward);
    T_Damage(e, self, self, dmg);
    VR_ClearHitKind();
    VR_Push(e, xForward, 520 * push, 140 * push);
    if(e.health > 0 && e.th_pain)
    {
        entity oldSelf = self;
        self = e;
        e.pain_finished = 0;
        e.th_pain(oldSelf, dmg);
        e.attack_finished = time + xScale;
        self = oldSelf;
    }
    SpawnBloodSplash(e.origin + '0 0 16', 4);
};

// Batting with a bash (vr_deflect; round 20): for vr_bash_deflect_window seconds from the push, a monster's
// projectile whose path this frame (from a little behind it to where it will be at the frame's end) comes within
// vr_bash_deflect_radius of the guard -- each hand's weapon line, each empty hand, and the line between the hands
// while they are together -- is sent back ahead (at its thrower, within VR_BASH_AIM of the facing `xForward`).
void(vector xForward) VR_Bash_Deflect =
{
    float radius = max(0, cvar_hget(cvarh_vr_bash_deflect_radius));
    float dt = max(frametime, 0.011);
    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    vector mh = VRGetHandPos(cVR_MainHand);
    vector oh = VRGetHandPos(cVR_OffHand);
    vector ma = mh;
    vector mb = mh;
    vector oa = oh;
    vector ob = oh;
    if(bm.mh_thing != VR_MTHING_FIST && bm.mh_thing != VR_MTHING_NONE)
    {
        ma = bm.mh_near;
        mb = bm.mh_far;
    }
    if(bo.mh_thing != VR_MTHING_FIST && bo.mh_thing != VR_MTHING_NONE)
    {
        oa = bo.mh_near;
        ob = bo.mh_far;
    }
    float together = vlen(mh - oh) < 0.7 * VRMetersToUnits();
    vector head = VRGetHeadPos(self);

    entity e = findradius((mh + oh) * 0.5, 200);
    while(e)
    {
        if(VR_Deflect_IsProjectile(e) && time >= e.vr_deflect_time + 0.3 && (e.origin - head) * xForward > -8)
        {
            vector from = e.origin - e.velocity * max(dt, VR_BAT_LATE * 2);
            vector to = e.origin + e.velocity * dt;
            float near = min(VR_SegmentDistance(from, to, ma, mb), VR_SegmentDistance(from, to, oa, ob));
            if(together)
            {
                near = min(near, VR_SegmentDistance(from, to, mh, oh));
            }
            if(near < radius + e.maxs_x)
            {
                dprint(sprintf("deflect: %s of %s, %.0f units from the bash's guard, %.2f s into the push\n",
                    e.classname, e.owner.classname, near,
                    time - (self.vr_bashdef_until - max(0, cvar_hget(cvarh_vr_bash_deflect_window)))));
                VR_Deflect_Send(e, xForward, self.vr_bashdef_hand, VR_BASH_AIM);
            }
        }
        e = e.chain;
    }
};

// Starts a bash's batting (vr_deflect), with `xHand` (-1: both), unless it is going on already.
void(float xHand, vector xForward) VR_Bash_DeflectStart =
{
    if(time > self.vr_bashdef_until && cvar_hget(cvarh_vr_deflect))
    {
        self.vr_bashdef_hand = xHand;
        self.vr_bashdef_until = time + max(0, cvar_hget(cvarh_vr_bash_deflect_window));
        VR_Bash_Deflect(xForward);
    }
};

// Follows each hand's stance and push; a push of a stance far enough is a bash (or a shove, open palms): what is just
// ahead is knocked back, once. Runs after the hands are tracked and before their blows: the hands in a push don't
// also strike.
void() VR_Bash =
{
    entity bm = VR_Blow(cVR_MainHand);
    entity bo = VR_Blow(cVR_OffHand);
    bm.mh_pushing = FALSE;
    bo.mh_pushing = FALSE;
    if(!cvar_hget(cvarh_vr_bash) || !self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        bm.mh_stance = bo.mh_stance = 0;
        bm.mh_push_dist = bo.mh_push_dist = 0;
        return;
    }

    // The body's facing is the head's (.v_angle is the main hand's aim: a weapon held across in front aims sideways).
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);
    vector pfwd = VR_Blow_Facing();
    if(time <= self.vr_bashdef_until)
    {
        VR_Bash_Deflect(fwd); // a bash's batting goes on
    }

    float speed = max(0.1, cvar_hget(cvarh_vr_bash_speed));
    float h;
    entity b;
    float readyHand = -1;
    for(h = 0; h < 2; ++h)
    {
        b = h == cVR_OffHand ? bo : bm;
        float stance = VR_Bash_Stance(h, b, fwd);
        if(stance != b.mh_stance)
        {
            b.mh_stance = stance;
            b.mh_stance_since = time;
            b.mh_stance_said = FALSE;
        }
        if(stance == 1 && !b.mh_stance_said && time - b.mh_stance_since >= VR_STANCE_REPORT)
        {
            // (The parry poses are reported: a weapon across, crossed arms; not a palm facing ahead.)
            b.mh_stance_said = TRUE;
            VR_Melee_Event(sprintf("stance weapon, hand %g%s", h, VRGet2HAiming(self) ? ", two hands" : ""));
            VR_Motion_Event("stance", "weapon", h, 0, VRGetHandPos(h), world, VRGet2HAiming(self) ? "two hands" : "");
        }
        if(stance && cvar("developer") >= 2 && VR_Bash_PushVel(b) * pfwd > speed * 0.5)
        {
            dprint(sprintf("melee debug: hand %g stance %g push %v ends %v %v dist %.3f\n", h, stance,
                VR_Bash_PushVel(b), b.mh_vnear, b.mh_vfar, b.mh_push_dist));
        }
        if(stance && VR_Bash_Pushing(h, b, pfwd, speed))
        {
            b.mh_pushing = TRUE;
            b.mh_push_dist = b.mh_push_dist + (VR_Bash_PushVel(b) * pfwd) * b.mh_dt;
            b.mh_push_last = time;
        }
        else if(time - b.mh_push_last > VR_BASH_GAP)
        {
            b.mh_push_dist = 0;
        }
        if(b.mh_pushing && b.mh_push_dist >= VR_BASH_TRAVEL && readyHand < 0)
        {
            readyHand = h;
        }
    }
    // Crossed arms (the unarmed parry, VR_Parry_ArmsCrossed): reported as a stance too.
    float arms = VR_Parry_ArmsCrossed(self, fwd);
    if(arms != self.vr_arms)
    {
        self.vr_arms = arms;
        self.vr_arms_since = time;
    }
    else if(arms && self.vr_arms_since > 0 && time - self.vr_arms_since >= VR_STANCE_REPORT)
    {
        self.vr_arms_since = 0;
        VR_Melee_Event("stance arms, hand -1");
        VR_Motion_Event("stance", "arms", -1, 0, VRGetHandPos(cVR_MainHand), world, "");
    }
    if(readyHand < 0)
    {
        return;
    }
    // An open palm's push beside a weapon's (the palm on the blade) is the weapon's.
    entity rb = readyHand == cVR_OffHand ? bo : bm;
    entity rob = readyHand == cVR_OffHand ? bm : bo;
    if(rb.mh_stance == 2 && rob.mh_stance == 1 && rob.mh_pushing)
    {
        readyHand = VRGetOtherHand(readyHand);
    }

    b = readyHand == cVR_OffHand ? bo : bm;
    float other = VRGetOtherHand(readyHand);
    entity ob = other == cVR_OffHand ? bo : bm;
    float armed = b.mh_stance == 1;

    // Two hands: the weapon held two-handed; an open palm pushing on a one-handed weapon; both palms (or both weapons)
    // pushing.
    float both = FALSE;
    if(armed && VRGet2HAiming(self) && VRIsHandEmpty(self, other))
    {
        both = TRUE;
    }
    else if(ob.mh_stance && (ob.mh_pushing || VR_Bash_Pushing(other, ob, pfwd, speed * VR_BASH_PAIR)))
    {
        if(ob.mh_stance == b.mh_stance)
        {
            both = TRUE;
        }
        else if(armed && VR_SegmentDistance(ob.mh_grip, ob.mh_grip, b.mh_near, b.mh_far) <
                         VR_BASH_PAIR_NEAR * VRMetersToUnits())
        {
            both = TRUE; // the palm on the blade
        }
    }

    // The motion recorder (vr_motion.qc): a push, whatever it meets; once a push (its frames in a row).
    if(time - self.vr_motion_pushseen > 0.15)
    {
        VR_Motion_Event("push", armed ? "bash" : "shove", both ? -1 : readyHand, 0, VRGetHandPos(readyHand), world,
                        both ? "two hands" : "one hand");
    }
    self.vr_motion_pushseen = time;

    // The hands in the push strike nothing else in this motion.
    VRSetMeleeHitSoundPlayed(readyHand, TRUE);
    b.mh_hit_ent = world;
    b.mh_hit_time = time;
    b.mh_hit_touch = time;
    if(both)
    {
        VRSetMeleeHitSoundPlayed(other, TRUE);
        ob.mh_hit_ent = world;
        ob.mh_hit_time = time;
        ob.mh_hit_touch = time;
    }

    // Batting: a bash, or a shove while a hand holds a weapon.
    if(armed || !VRIsHandEmpty(self, cVR_MainHand) || !VRIsHandEmpty(self, cVR_OffHand))
    {
        VR_Bash_DeflectStart(both ? -1 : readyHand, fwd);
    }

    if(time < self.vr_bash_time)
    {
        return;
    }
    float hits = VR_Bash_Find(readyHand, b, fwd);
    if(both && !hits && ob.mh_stance)
    {
        hits = VR_Bash_Find(other, ob, fwd);
    }
    if(!hits)
    {
        return; // a push at the air: nothing yet (the push may still reach something)
    }

    // A weapon bash soon after a parry: a parry-bash, the counter.
    float counter = armed && self.vr_parry_time > 0 && time - self.vr_parry_time < VR_PARRYBASH_WINDOW;
    float scale = both ? 1 : VR_BASH_ONE_DAMAGE;
    float push = both ? 1 : VR_BASH_ONE_PUSH;
    if(counter)
    {
        scale = scale * VR_PARRYBASH_SCALE;
        push = push * VR_PARRYBASH_SCALE;
    }
    vector at = both ? (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5 : VRGetHandPos(readyHand);
    if(armed)
    {
        at = (b.mh_near + b.mh_far) * 0.5;
    }
    vr_bash_counter = counter;
    vr_bash_both = both;
    entity e = vr_bash_victims;
    while(e)
    {
        entity next = e.vr_bash_next;
        vector hitAt = at;
        hitAt_x = min(e.absmax_x, max(e.absmin_x, hitAt_x));
        hitAt_y = min(e.absmax_y, max(e.absmin_y, hitAt_y));
        hitAt_z = min(e.absmax_z, max(e.absmin_z, hitAt_z));
        VR_Bash_Hit(e, hitAt, fwd, armed ? QVR_HITKIND_BASH : QVR_HITKIND_SHOVE, both ? -1 : readyHand, scale, push);
        e = next;
    }
    vr_bash_counter = FALSE;
    vr_bash_both = FALSE;
    self.vr_bash_time = time + VR_BASH_COOLDOWN;
    if(counter)
    {
        self.vr_parry_time = 0; // one counter a parry
    }

    // Each its own sound (vr_bash_sound; round 18), apart from the blows'; 0: the old ones.
    float bashSound = min(1, cvar_hget(cvarh_vr_bash_sound));
    if(bashSound > 0)
    {
        sound(self, CHAN_AUTO, counter ? "vr/bash_parry.wav" : (armed ? "vr/bash.wav" : "vr/shove.wav"),
              bashSound * (both ? 1 : 0.7), ATTN_NORM);
    }
    else
    {
        sound(self, CHAN_AUTO, armed ? "player/axhit2.wav" : "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", both ? 0.7 : 0.45, ATTN_NORM);
    }
    if(both)
    {
        haptic(cVR_MainHand, 0.0, 0.25, 70, 1.0);
        haptic(cVR_OffHand, 0.0, 0.25, 70, 1.0);
        handimpact(cVR_MainHand, 3, fwd * -1);
        handimpact(cVR_OffHand, 3, fwd * -1);
    }
    else
    {
        haptic(readyHand, 0.0, 0.2, 70, 0.8);
        handimpact(readyHand, 2, fwd * -1);
    }
    particle2(at + fwd * 12, fwd * 80, QVR_PARTICLE_PRESET_BULLETPUFF, both ? 16 : 10);
    VR_Melee_Event(sprintf("%s, hand %g, %s, %g hit%s, push %.1f m/s over %.0f cm", counter ? "parrybash" :
        (armed ? "bash" : "shove"), readyHand, both ? "two hands" : "one hand", hits, hits == 1 ? "" : "s",
        VR_Bash_PushVel(b) * pfwd, b.mh_push_dist * 100));
};

// ----------------------------------------------------------------------------
// Each frame

// Follows both hands (PlayerVRMelee, before the bash and the blows).
void() VR_Melee_Frame =
{
    float h;
    for(h = 0; h < 2; ++h)
    {
        entity b = VR_Blow(h);
        b.mh_c_ent = world;
        b.mh_c_pos = '0 0 0';
        float fresh = VR_Melee_Track(h, b);
        if(!fresh)
        {
            b.mh_dt = 0; // (no new pose, or nothing to sweep from: no motion counted this frame)
        }
        b.mh_fresh = fresh;
        if(fresh && cvar("developer") >= 3 && b.mh_thing != VR_MTHING_NONE)
        {
            dprint(sprintf("melee trace: %.3f hand %g grip %v far %v vfar %v vgrip %v\n", time, h, b.mh_rgrip, b.mh_rfar,
                b.mh_vfar, b.mh_vgrip));
        }
    }
};

// The blow of `xHand` this frame, if any (PlayerVRMeleeImpl): FALSE, or TRUE with the contact in mh_c_*, the kind and
// strength set.
float(float xHand) VR_Melee_Strike =
{
    entity b = VR_Blow(xHand);
    VR_Melee_Rearm(xHand, b);
    if(!b.mh_fresh || b.mh_thing == VR_MTHING_NONE)
    {
        return FALSE;
    }
    VR_Melee_Whoosh(xHand, b);
    if(b.mh_pushing)
    {
        return FALSE; // a bash's push
    }
    return VR_Melee_Sweep(xHand, b);
};

// After a blow of `xHand` landed on mh_c_ent: once per motion (VR_Melee_Rearm).
void(float xHand) VR_Melee_Landed =
{
    entity b = VR_Blow(xHand);
    b.mh_hit_ent = b.mh_c_ent;
    b.mh_hit_time = time;
    b.mh_hit_touch = time;
    vector v = b.mh_vfar;
    if(b.mh_kind == VR_MKIND_POMMEL)
    {
        v = b.mh_vgrip;
    }
    b.mh_hit_dir = normalize(v);
    string what = sprintf("%s, hand %g, %s (%s) with %s", VR_Melee_Category(b.mh_kind, b.mh_thing), xHand,
        VR_Melee_KindName(b.mh_kind, b.mh_thing), b.mh_way, b.mh_part);
    VR_Melee_Event(sprintf("%s, %.1f m/s, x%.2f, %s", what, b.mh_speed, b.mh_strength,
        b.mh_c_ent.takedamage ? b.mh_c_ent.classname : "a wall"));
};
