// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat);
// - bashing (vr_bash): a guard held in front (a weapon across, as for a parry) and then driven forward
//   knocks monsters back and staggers them; so does a shove, open palms pushed at them.
// The hands' blows are here too: the melee's model (what makes a motion a punch or a swing, and how
// hard).
// Hit reactions (vr_hit_push), explosion rumble (vr_explosion_rumble) and directional damage haptics
// are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat
.float vr_bash_time;      // on a player: when the next bash may be
.float vr_guard_hand;     // on a player: the guard held (VR_Bash_Guard),
.float vr_guard_time;     // and when it was last ready: held still for vr_bash_hold (0: not yet),
.float vr_guard_since;    // since when it has been held still (0: it isn't),
.float vr_guard_seen;     // and last held at all
.float vr_guard_note;     // the stroke (blow_t0) a "no bash" line was printed for (developer)
.float vr_push_since;     // since when the guard has been pushed (VR_BASH_CONFIRM; 0: it isn't)
.float vr_bashdef_until;  // on a player: a bash's (or an armed shove's) batting goes on until then (VR_Bash_Deflect),
.float vr_bashdef_hand;   // with that hand (-1: both)
.float vr_motion_pushseen; // on a player: the last frame of a push (the motion recorder's "push" event: once a push)

// ----------------------------------------------------------------------------
// Blows: one model for every hand's melee, fist or weapon (docs/vr-port/ROUND14.md, "Melee").
//
// WHAT IS FOLLOWED. Each hand's wrist (estimated behind the controller: the arm's own motion, which a
// flick of the wrist hardly moves) and its striking point (the fist; a weapon's head: its muzzle
// point), both from the head and in the play space, so walking, turning and the body moving under the
// head are not the hand's motion.
//
// A STROKE is one run of the wrist in a steady direction. It starts from a wind-up: the wrist starting
// to move (faster than VR_BLOW_MOVING) or turning back. It ends when the wrist slows down, turns away
// from its line (more than about 65 degrees: an arc of the arm up to about 130 degrees goes on, a
// wiggle is many short strokes) or goes on for over half a second (a drift).
//
// A BLOW is a stroke that, all at once (VR_Blow_IsBlow):
//  - has taken the wrist vr_melee_distance (flicks, wiggles and whips don't: too short);
//  - got the wrist to the hand's minimum speed: vr_melee_speed for a fist or a blunt item (a gun used
//    as a club), VR_BLOW_SWING_MIN times it for a swung weapon (axe, sword, Mjolnir: waving one about
//    is no blow);
//  - got there in a snap: the wrist's mean acceleration from the stroke's start to its fastest is
//    VR_BLOW_SNAP or more (a soft wide wave builds up its speed slowly, a blow suddenly);
//  - is still moving hard (half the minimum speed);
//  - goes ahead, up, down or sideways, not back towards or behind the body; and for a fist, the fist
//    doesn't turn more than VR_BLOW_WHIP for the way the wrist went (whipped by the wrist and forearm).
//
// HOW HARD (blow_strength: the weapon's base damage is multiplied by it -- fist 10, gun 12, axe and
// sword 20, Mjolnir 25 -- at most VR_BLOW_MAX), the product of:
//  - speed: the wrist's fastest, 1 at a good blow (VR_BLOW_GOOD, or VR_BLOW_SWING_GOOD, times
//    vr_melee_speed), linear from VR_BLOW_BASE times vr_melee_speed: a fist at the minimum 0.25, a
//    swung weapon 0.45, twice as fast as good about 2.5;
//  - reach: the wrist's travel, 1 at vr_melee_distance to 1.2 at twice it (a full punch, a wide swing);
//  - shape (VR_Blow_Weight): a fist or a club is best punching straight ahead (vr_melee_punch_mult),
//    an uppercut 1, straight down 0.85, a slap or hook (an arc: VR_BLOW_SLAP) and an overhead blow
//    (VR_BLOW_OVERHEAD) less, as an arc swings the whole arm and moves the hand faster for the same
//    effort; a swung weapon is best in an arc (overhead, side swing: 1), a straight thrust or push
//    VR_BLOW_THRUST, swung upwards less (VR_BLOW_SWING_UP);
//  - for a swung weapon, the head's snap: its fastest over the wrist's, from 0.85 (held stiffly: x1.2
//    or less) to 1.2 (the wrist's snap or a long arc: x1.8 and more).
// So a good punch does about 12-15, a good axe swing about 20, a hard overhead axe blow 30 to 40, a
// weak one a third of that or less. After a blow lands the hand must slow down or pull back (and
// VR_BLOW_REARM) before the next.
//
// (Bashes -- a guard held, then pushed forward -- are below, VR_Bash; they come first, and a motion
// that is a bash is no blow.)

float VR_BLOW_MOVING = 0.7;     // m/s: slower, the wrist is not in a stroke
float VR_BLOW_SNAP = 25;        // m/s2: the least mean acceleration of the wrist to its fastest
float VR_BLOW_BASE = 0.8;       // times vr_melee_speed: the speed factor would be 0 there
float VR_BLOW_GOOD = 1.6;       // times vr_melee_speed: a good punch (speed factor 1)
float VR_BLOW_SWING_MIN = 1.25; // times vr_melee_speed: the least for a swung weapon
float VR_BLOW_SWING_GOOD = 1.8; // times vr_melee_speed: a good swing
float VR_BLOW_MAX = 2;          // the most a blow's strength can be
float VR_BLOW_SLAP = 0.6;
float VR_BLOW_DOWN = 0.85;
float VR_BLOW_UP = 1.0;
float VR_BLOW_BACK = 0.5;
float VR_BLOW_OVERHEAD = 0.7;
float VR_BLOW_THRUST = 0.8;     // a swung weapon driven straight
float VR_BLOW_SWING_UP = 0.5;   // a swung weapon swung straight up
float VR_BLOW_ARC = 1.8;    // rad/m: an arc of about 55 cm around the shoulder (a straight punch: 0)
float VR_BLOW_REARM = 0.2;  // s after a blow lands before the next
float VR_BLOW_BACKMOST = -0.45; // cosine from the head's facing: further back (about 117 degrees) is no blow
float VR_BLOW_BACKBIAS = 1;     // going back at all, the distance needed grows by this much of the cosine
float VR_BLOW_WHIP = 2.6;       // rad/m: a fist turning more than this for the way the wrist went is whipped
float VR_BLOW_JUMP = 20;        // m/s: the wrist moving faster than this jumped (a teleport, a re-based play space)
float VR_BLOW_TIPJUMP = 60;     // m/s: likewise the striking point (a weapon taken)
float VR_BLOW_STAB_ALIGN = 0.8; // cosine: a sword's stroke this near the blade's line, the tip leading, is a stab
float VR_BLOW_STAB_TIP = 1.4;   // a stab's tip goes at most this much faster than its wrist (a cut sweeps the tip round)
float VR_BLOW_STAB_TURN = 150;  // degrees/s: a stab's blade turns slower than this (a cut's turns faster)
float VR_BLOW_STAB_SNAP = 12;   // m/s2: a stab's least snap (a thrust gets going less suddenly than a swing)
float VR_BLOW_STAB = 1;         // a stab's shape weight (piercing; another thrust of a swung weapon: VR_BLOW_THRUST)

// A hand's stroke, on an entity of its own (the player's vr_blow_main/vr_blow_off).
.entity vr_blow_main;
.entity vr_blow_off;
.vector blow_wrist;    // the wrist a frame ago (metres, from the head, in the play space)
.vector blow_vel;      // its velocity (m/s)
.float blow_speed;
.float blow_time;      // when it was measured
.vector blow_tip;      // the striking point a frame ago (the fist, or the weapon's head)
.float blow_tipspeed;
.vector blow_start;    // where the stroke began
.float blow_t0;        // when (0: no stroke)
.float blow_peak;      // the stroke's fastest: the wrist's,
.float blow_peaktime;  // when it was,
.float blow_tippeak;   // and the striking point's (m/s)
.float blow_turn;      // how far the wrist's path has turned (radians),
.float blow_path;      // over how long a path (metres)
.vector blow_fwd0;     // the way the hand pointed as the stroke began (play space)
.vector blow_dir;      // the direction of the last blow that landed (play space)
.float blow_rearm;     // no blow before this
.float blow_arc;       // the blow being dealt: how much of an arc it is (0 .. 1),
.float blow_strength;  // its damage multiplier,
.float blow_kind;      // what it is (VR_BLOW_KIND_*),
.float blow_dist;      // how far the wrist travelled (metres),
.float blow_accel;     // and how fast it got going (the wrist's mean acceleration to its fastest, m/s2)
.float blow_dt;        // s between the hand's last two poses
.vector blow_axis;     // the way what it holds points (the hand to the weapon's far end; a fist: the hand's forward), play space,
.float blow_axisrate;  // and how fast that turns (degrees/s): a swing turns the blade, a bash's push hardly does
.float blow_swung;     // the stroke (blow_t0) in which the weapon turned as fast as a swing (no bash in it)
.float blow_stab;      // the stroke is a stab: a sword driven along its blade, the tip leading (VR_Blow_IsBlow)

// The striking points (VR_Blow_Points, "Striking points" below).
vector vr_pt[6];       // the points of the hand last asked for (world),
float vr_pt_mult[6];   // each one's damage multiplier,
string vr_pt_name[6];  // and name,
float vr_pt_count;     // how many
vector vr_pt_axis;     // and the way the hand's weapon points (world, unit: the hand to its far end; a fist: the hand's forward)
.vector blow_pp[6];    // the hand's points at its pose before (world),
.vector blow_cp[6];    // at its pose now,
.float blow_pmult[6];  // their damage multipliers,
.float blow_ppn;       // how many (0: none yet),
.float blow_ppw;       // and of which weapon (another: nothing to sweep from)
.float blow_ok;        // the stroke (its blow_t0) that became a blow (VR_Blow_IsBlow)
.float blow_slowest;   // the wrist's slowest since the last blow landed (m/s),
.float blow_hitpeak;   // and that blow's fastest
.float blow_hit_stroke; // the stroke (blow_t0) of the first contact with something that bleeds:
.entity blow_hit_ent;  // what,
.vector blow_hit_pos;  // where,
.vector blow_hit_dir;  // the point's way,
.float blow_hit_mult;  // its damage multiplier,
.string blow_hit_name; // and its name ("the tip"),
.float blow_hit_time;  // when,
.float blow_hit_swing; // and whether the weapon was swinging past it (a weak point's contact then waits for the blade)
.float blow_wall_stroke; // likewise, the stroke's first contact with a wall,
.float blow_wall_time;
.vector blow_wall_pos;
.vector blow_wall_dir;
.vector blow_wall_normal;
.entity blow_wall_ent;
.string blow_wall_name;
.vector bat_a[8];      // a batting swing's weapon lines, as they were (VR_Deflect),
.vector bat_b[8];
.float bat_t[8];       // when,
.float bat_i;          // and the newest,
.float bat_newest;     // its time (bat_t[bat_i])

float VR_BLOW_KIND_PUNCH = 0;
float VR_BLOW_KIND_SLAP = 1;
float VR_BLOW_KIND_DOWN = 2;
float VR_BLOW_KIND_UP = 3;
float VR_BLOW_KIND_BACK = 4;
float VR_BLOW_KIND_SWING = 5; // a swung weapon's arc across
float VR_BLOW_KIND_THRUST = 6; // a swung weapon driven straight ahead
float VR_BLOW_KIND_RISING = 7; // a swung weapon swung upwards
float VR_BLOW_KIND_STAB = 8;   // a sword driven along its blade, the tip leading (piercing)

// What a hand strikes with.
float VR_BLOW_CLASS_FIST = 0;  // a fist (a box in hand too)
float VR_BLOW_CLASS_CLUB = 1;  // a blunt item: a gun used as a club
float VR_BLOW_CLASS_SWING = 2; // a swung weapon: the axe, a sword, Mjolnir

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

float(float xHand) VR_Blow_Class =
{
    float wid = VRGetEntWeapon(self, xHand);
    if(wid == WID_FIST)
    {
        return VR_BLOW_CLASS_FIST;
    }
    if(wid == WID_AXE || wid == WID_SWORD || wid == WID_MJOLNIR)
    {
        return VR_BLOW_CLASS_SWING;
    }
    return VR_BLOW_CLASS_CLUB;
};

// The wrist's least speed for a blow of a hand of `xClass` (m/s).
float(float xClass) VR_Blow_MinSpeed =
{
    float s = max(0.5, cvar_hget(cvarh_vr_melee_speed));
    return xClass == VR_BLOW_CLASS_SWING ? s * VR_BLOW_SWING_MIN : s;
};

// `xVec` of the world turned into the play space (the player's turning undone).
// The turn's cosine and sine are kept for the last yaw (it changes only as the player turns; the
// hands ask several times a frame).
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
    float c = vr_ps_cos;
    float s = vr_ps_sin;
    vector r;
    r_x = xVec_x * c - xVec_y * s;
    r_y = xVec_x * s + xVec_y * c;
    r_z = xVec_z;
    return r;
};

// The wrist of `xHand`, in metres from the head, in the play space: behind the controller's
// point along the hand's forward, and below it (the controller sits above the fist, ahead of it).
// (`m2u`: VRMetersToUnits(), the caller's.)
vector(float xHand, float m2u) VR_Blow_Wrist =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector w = VRGetHandPos(xHand) - VRGetHeadPos(self) - v_forward * (0.13 * m2u) - v_up * (0.085 * m2u);
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return VR_Blow_ToPlaySpace(w) * (1 / m2u);
};

// The striking point of `xHand`, likewise: a weapon's head (its muzzle point), or the fist.
vector(float xHand, float m2u) VR_Blow_Tip =
{
    vector p = VRGetHandPos(xHand);
    vector m = VRGetMuzzlePos(xHand);
    if(!VRIsHandEmpty(self, xHand) && m != '0 0 0' && vlen(m - p) < 80)
    {
        p = m;
    }
    return VR_Blow_ToPlaySpace(p - VRGetHeadPos(self)) * (1 / m2u);
};

// The way `xHand` points, in the play space.
vector(float xHand) VR_Blow_HandForward =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector f = v_forward;
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return VR_Blow_ToPlaySpace(f);
};

// Follows the stroke of `xHand` (VR_Blow_Update): 0 for the same pose again (no news), -1 when it
// started over (a gap or a jump), 1 otherwise.
float(float xHand, entity b) VR_Blow_Track =
{
    // The pose is the client's as this server frame ends (time + frametime): QC's time is still the
    // frame's start, one frame behind it, and frames differ (144 fps runs the server at 72 and 48 Hz
    // by turns): measured by time, the wrist's speed came out up to half again too fast or too slow.
    float now = time + frametime;
    float dt = now - b.blow_time;
    float m2u = VRMetersToUnits();
    vector w = VR_Blow_Wrist(xHand, m2u);
    vector d = w - b.blow_wrist;
    vector tip = VR_Blow_Tip(xHand, m2u);
    vector dtip = tip - b.blow_tip;

    // The same pose again (a server frame with no new tracking): no news, not a stop; the next
    // pose's motion spans both frames.
    if(d == '0 0 0' && dtip == '0 0 0' && dt > 0 && dt < 0.05)
    {
        return 0;
    }
    b.blow_time = now;
    b.blow_wrist = w;
    b.blow_tip = tip;

    // A gap (the hands lost), or a jump (a teleport, the play space re-based, a weapon taken): start
    // over. A jump is faster than a hand goes (VR_BLOW_JUMP, VR_BLOW_TIPJUMP), not a distance in a
    // frame: a sword's tip goes 0.6 m in a frame at 45 Hz in a hard swing, which used to end the stroke
    // (so the swing never hit at 45 or 90 fps: the server runs at 45 Hz at both).
    if(!VRHandsTracked(self) || dt <= 0 || dt > 0.1 || vlen(d) > max(0.3, VR_BLOW_JUMP * dt) ||
       vlen(dtip) > max(0.6, VR_BLOW_TIPJUMP * dt))
    {
        b.blow_t0 = 0;
        b.blow_speed = 0;
        b.blow_tipspeed = 0;
        b.blow_vel = '0 0 0';
        b.blow_dt = 0;
        return -1;
    }

    b.blow_dt = dt;
    vector oldVel = b.blow_vel;
    float oldSpeed = b.blow_speed;
    b.blow_vel = d * (1 / dt);
    b.blow_speed = vlen(b.blow_vel);
    b.blow_tipspeed = vlen(dtip) / dt;

    // The stroke ends when the wrist slows down, turns away from its line (more than about 65
    // degrees: arcs go on) or has gone on for long (a slow drift is no wind-up).
    if(b.blow_t0)
    {
        vector s = w - b.blow_start;
        float sl = vlen(s);
        if(b.blow_speed < VR_BLOW_MOVING || (sl > 0.03 && b.blow_vel * s < 0.4 * b.blow_speed * sl) ||
           now - b.blow_t0 > 0.5)
        {
            b.blow_t0 = 0;
        }
    }
    if(!b.blow_t0)
    {
        if(b.blow_speed < VR_BLOW_MOVING)
        {
            return 1;
        }
        // A new stroke, from the last still point (or the turn).
        b.blow_t0 = now - dt;
        b.blow_start = w - d;
        b.blow_peak = 0;
        b.blow_peaktime = now;
        b.blow_tippeak = 0;
        b.blow_turn = 0;
        b.blow_path = 0;
        b.blow_fwd0 = VR_Blow_HandForward(xHand);
    }
    else if(oldSpeed > 1 && b.blow_speed > 1)
    {
        // The path's turn since the last frame (fast enough for its direction to be sure).
        b.blow_turn = b.blow_turn + VRAngleBetween(oldVel * (1 / oldSpeed), b.blow_vel * (1 / b.blow_speed)) *
                                    (3.14159265 / 180);
        b.blow_path = b.blow_path + vlen(d);
    }
    if(b.blow_speed > b.blow_peak)
    {
        b.blow_peak = b.blow_speed;
        b.blow_peaktime = now;
    }
    b.blow_tippeak = max(b.blow_tippeak, b.blow_tipspeed);
    return 1;
};

// Follows the stroke of `xHand` and its striking points (below), once a frame (PlayerVRMelee,
// before the bashes and the blows): the points are swept from the last new pose to this one, and a
// stroke's first contact is kept (VR_Blow_Sweep).
void(float xHand) VR_Blow_Update =
{
    entity b = VR_Blow(xHand);
    float tracked = VR_Blow_Track(xHand, b);
    if(!tracked)
    {
        return; // no new pose: the next one's sweep spans both frames
    }

    VR_Blow_Points(xHand);
    float wid = VRIsHandEmpty(self, xHand) ? WID_FIST : VRGetEntWeapon(self, xHand);
    float same = tracked > 0 && b.blow_ppn == vr_pt_count && b.blow_ppw == wid;
    // Unrolled, constant indices: plain fields and globals, not the array accessor functions a
    // variable index calls (six of them a point, every frame).
#define VR_BLOW_KEEP(k) \
    if(k < vr_pt_count) \
    { \
        b.blow_pp[k] = same ? b.blow_cp[k] : vr_pt[k]; \
        b.blow_cp[k] = vr_pt[k]; \
        b.blow_pmult[k] = vr_pt_mult[k]; \
    }
    VR_BLOW_KEEP(0)
    VR_BLOW_KEEP(1)
    VR_BLOW_KEEP(2)
    VR_BLOW_KEEP(3)
    VR_BLOW_KEEP(4)
    VR_BLOW_KEEP(5)
#undef VR_BLOW_KEEP
    b.blow_ppn = vr_pt_count;
    b.blow_ppw = wid;

    // How fast what the hand holds turns (a swing, not a bash: VR_Bash).
    vector axis = VR_Blow_ToPlaySpace(vr_pt_axis);
    b.blow_axisrate = 0;
    if(same)
    {
        if(b.blow_dt > 0)
        {
            b.blow_axisrate = VRAngleBetween(b.blow_axis, axis) / b.blow_dt;
        }
    }
    b.blow_axis = axis;

    if(same && b.blow_t0)
    {
        VR_Blow_Sweep(xHand, b);
    }
};

// How far the stroke of `xHand` has taken the wrist (metres; 0 when not in one).
float(float xHand) VR_Blow_Distance =
{
    entity b = VR_Blow(xHand);
    return b.blow_t0 ? vlen(b.blow_wrist - b.blow_start) : 0;
};

// The wrist's mean acceleration from the start of the stroke `b` to its fastest (m/s2).
float(entity b) VR_Blow_Accel =
{
    return b.blow_peak / max(0.011, b.blow_peaktime - b.blow_t0);
};

// The head's facing, flat, in the play space.
vector() VR_Blow_Facing =
{
    return VRForwardOf('0 1 0' * (self.v_viewangle_y - self.vryaw));
};

// Whether `xHand` is in a blow (see above): far enough, fast enough, in a snap, still moving hard;
// not going back (more than VR_BLOW_BACKMOST from the head's facing: a hand pulled back out of a
// monster, or behind the body), and going back at all it must travel further (VR_BLOW_BACKBIAS). A
// fist that turned more than VR_BLOW_WHIP for the way the wrist went is whipped back and forth by the
// wrist and the forearm, no blow (a punch hardly turns the fist -- a corkscrew turns it about its own
// line, which does not count -- and a slap or a hook swings from the shoulder, the fist turning with
// the arm's wide arc).
float(float xHand, entity b) VR_Blow_IsBlow = // (b: VR_Blow(xHand))
{
    if(!b.blow_t0)
    {
        b.blow_stab = FALSE;
        return FALSE;
    }
    vector s = b.blow_wrist - b.blow_start;
    float dist = vlen(s);

    // A stab (a sword driven along its blade, the tip leading, the blade hardly turning: its tip no
    // faster than the wrist, VR_BLOW_STAB_TIP) pierces: it needs less speed and snap than a swing
    // (vr_melee_stab_speed times vr_melee_speed). A cut's blade may point along its path too (a
    // diagonal cut's, down and ahead) but sweeps round: its tip goes two to five times the wrist's speed.
    // (Nested ifs: fteqcc evaluated this as one chain of && to true with one of its terms false.)
    float stab = FALSE;
    if(dist > 0.05)
    {
        if(VRGetEntWeapon(self, xHand) == WID_SWORD)
        {
            if(s * b.blow_axis > dist * VR_BLOW_STAB_ALIGN)
            {
                if(b.blow_tippeak < b.blow_peak * VR_BLOW_STAB_TIP)
                {
                    if(b.blow_axisrate < VR_BLOW_STAB_TURN)
                    {
                        stab = TRUE;
                    }
                }
            }
        }
    }
    b.blow_stab = stab;
    float speed = VR_Blow_MinSpeed(VR_Blow_Class(xHand));
    float snap = VR_BLOW_SNAP;
    if(b.blow_stab)
    {
        speed = max(0.5, cvar_hget(cvarh_vr_melee_speed)) * max(0.3, cvar_hget(cvarh_vr_melee_stab_speed));
        snap = VR_BLOW_STAB_SNAP;
    }
    if(b.blow_peak < speed || b.blow_speed < speed * 0.5 || VR_Blow_Accel(b) < snap)
    {
        return FALSE;
    }
    float need = cvar_hget(cvarh_vr_melee_distance);
    if(dist < need)
    {
        return FALSE;
    }
    float f = (s * (1 / dist)) * VR_Blow_Facing();
    if(f < VR_BLOW_BACKMOST || (f < 0 && dist < need * (1 - f * VR_BLOW_BACKBIAS)))
    {
        return FALSE;
    }
    if(VRGetEntWeapon(self, xHand) == WID_FIST &&
       VRAngleBetween(b.blow_fwd0, VR_Blow_HandForward(xHand)) * (3.14159265 / 180) > dist * VR_BLOW_WHIP)
    {
        return FALSE;
    }
    return TRUE;
};

// The stroke's weight by its shape and direction, from the head's facing, for a hand of `xClass`
// (see above); sets its blow_kind and blow_arc.
float(entity b, float xClass) VR_Blow_Weight =
{
    vector dir = normalize(b.blow_wrist - b.blow_start);
    vector fwd = VR_Blow_Facing();
    float f = dir * fwd;
    float z = dir_z;
    float side = max(0, 1 - f * f - z * z);

    float ahead = f >= 0 ? f * f * cvar_hget(cvarh_vr_melee_punch_mult) : f * f * VR_BLOW_BACK;
    float vertical = z < 0 ? z * z * VR_BLOW_DOWN : z * z * VR_BLOW_UP;

    b.blow_kind = VR_BLOW_KIND_PUNCH;
    if(f < 0 && f * f > side && f * f > z * z)
    {
        b.blow_kind = VR_BLOW_KIND_BACK;
    }
    else if(side > f * f && side > z * z)
    {
        b.blow_kind = VR_BLOW_KIND_SLAP;
    }
    else if(z * z > f * f)
    {
        b.blow_kind = z < 0 ? VR_BLOW_KIND_DOWN : VR_BLOW_KIND_UP;
    }
    float straight = ahead + vertical + side * VR_BLOW_SLAP;

    // How much of an arc: from a third of VR_BLOW_ARC (a straight stroke, give or take the
    // tracking's jitter and the elbow's swing) to all of it.
    float curve = b.blow_path > 0.05 ? b.blow_turn / b.blow_path : 0;
    b.blow_arc = min(1, max(0, (curve - VR_BLOW_ARC / 3) / (VR_BLOW_ARC * 2 / 3)));

    // An overhead blow: an arc upright (its plane: across the stroke's line and the way the wrist
    // goes now), not going up (it lands going forward, or down).
    float down = 0;
    if(b.blow_arc > 0)
    {
        vector n = b.blow_wrist - b.blow_start;
        vector v = b.blow_vel;
        vector plane;
        plane_x = n_y * v_z - n_z * v_y;
        plane_y = n_z * v_x - n_x * v_z;
        plane_z = n_x * v_y - n_y * v_x;
        if(vlen(plane) > 0.0001 && b.blow_speed > 0)
        {
            plane = normalize(plane);
            down = (1 - fabs(plane_z)) * min(1, max(0, 1 - v_z / b.blow_speed * 4));
        }
        if(b.blow_arc >= 0.5)
        {
            b.blow_kind = down >= 0.5 ? VR_BLOW_KIND_DOWN : VR_BLOW_KIND_SLAP;
        }
    }

    if(xClass == VR_BLOW_CLASS_SWING)
    {
        // A sword's stab (VR_Blow_IsBlow) pierces: the point leads, whichever way.
        if(b.blow_stab)
        {
            b.blow_kind = VR_BLOW_KIND_STAB;
            return VR_BLOW_STAB;
        }
        // A weapon's natural blows are arcs, down or across; straight, it is a thrust (ahead) or a
        // push; swung upwards, it has little behind it (VR_BLOW_SWING_UP, going straight up).
        if(z > 0.7)
        {
            b.blow_kind = VR_BLOW_KIND_RISING;
        }
        else if(b.blow_kind == VR_BLOW_KIND_SLAP)
        {
            b.blow_kind = VR_BLOW_KIND_SWING;
        }
        else if(b.blow_kind == VR_BLOW_KIND_PUNCH)
        {
            b.blow_kind = b.blow_arc >= 0.5 ? VR_BLOW_KIND_SWING : VR_BLOW_KIND_THRUST;
        }
        float rising = z > 0 ? 1 - (1 - VR_BLOW_SWING_UP) * z * z : 1;
        return (VR_BLOW_THRUST + (1 - VR_BLOW_THRUST) * b.blow_arc) * rising;
    }

    if(b.blow_arc <= 0)
    {
        return straight;
    }
    float arc = VR_BLOW_SLAP + (VR_BLOW_OVERHEAD - VR_BLOW_SLAP) * down;
    return straight + (arc - straight) * b.blow_arc;
};

// Measures the blow `xHand` is dealing: sets blow_strength, blow_kind, blow_dist and blow_accel.
void(float xHand) VR_Blow_Measure =
{
    entity b = VR_Blow(xHand);
    float cls = VR_Blow_Class(xHand);
    float s = max(0.5, cvar_hget(cvarh_vr_melee_speed));
    float base = s * VR_BLOW_BASE;
    float good = s * (cls == VR_BLOW_CLASS_SWING ? VR_BLOW_SWING_GOOD : VR_BLOW_GOOD);
    if(b.blow_stab)
    {
        // A stab's least speed is less than a swing's: its speed factor starts from half of it.
        base = min(base, s * max(0.3, cvar_hget(cvarh_vr_melee_stab_speed)) * 0.5);
    }
    float need = max(0.05, cvar_hget(cvarh_vr_melee_distance));

    b.blow_dist = VR_Blow_Distance(xHand);
    b.blow_accel = VR_Blow_Accel(b);
    float speed = max(0, (b.blow_peak - base) / (good - base));
    float reach = 1 + 0.2 * min(1, max(0, b.blow_dist / need - 1));
    float strength = speed * reach * VR_Blow_Weight(b, cls);
    if(cls == VR_BLOW_CLASS_SWING)
    {
        // The head's snap: the wrist's flick and the weapon's reach through a wide arc.
        float ratio = b.blow_tippeak / max(0.1, b.blow_peak);
        strength = strength * (0.85 + 0.35 * min(1, max(0, (ratio - 1.2) / 0.6)));
    }
    b.blow_strength = min(VR_BLOW_MAX, strength);
    if(b.blow_hit_stroke == b.blow_t0)
    {
        // Struck with a sword's hilt or an axe's handle (VR_Blow_Points).
        b.blow_strength = b.blow_strength * b.blow_hit_mult;
    }
};

// The damage multiplier of the blow `xHand` is dealing (VRMeleeDmg).
float(float xHand) VR_Blow_Strength =
{
    return VR_Blow(xHand).blow_strength;
};

string(float xKind) VR_Blow_KindName =
{
    if(xKind == VR_BLOW_KIND_SLAP)   return "slap";
    if(xKind == VR_BLOW_KIND_DOWN)   return "overhead blow";
    if(xKind == VR_BLOW_KIND_UP)     return "uppercut";
    if(xKind == VR_BLOW_KIND_BACK)   return "backhand";
    if(xKind == VR_BLOW_KIND_SWING)  return "swing";
    if(xKind == VR_BLOW_KIND_THRUST) return "thrust";
    if(xKind == VR_BLOW_KIND_RISING) return "rising swing";
    if(xKind == VR_BLOW_KIND_STAB)   return "stab";
    return "punch";
};

// ----------------------------------------------------------------------------
// Striking points (docs/vr-port/ROUND15.md, "Melee points, deflect, frame rate"). A hand strikes with
// a few points along what it holds, from the hand (VRGetHandPos) to the weapon's far end (its muzzle
// point, VRGetMuzzlePos: a sword's tip, the axe's head, a gun's muzzle: anchors on the weapon's model,
// so a weapon held another way, vr_weapons.inc, takes its points with it):
//  - a fist (or a box in hand): the fist and the knuckles;
//  - a sword: the pommel and the hilt (the hand, just under the guard: VR_PT_HILT of the damage), the
//    guard, the blade's middle, its outer part and the tip;
//  - the axe and Mjolnir: the hand and the handle (VR_PT_HANDLE), the head's back and the head;
//  - a gun used as a club: the grip, the barrel and the muzzle.
// Each server frame with a new pose, every point is swept from where it was to where it is
// (VR_Blow_Sweep: a line, VR_PT_RADIUS thick, against the boxes of things that bleed; a line against
// walls), so what the weapon passes through between two frames is struck at any frame rate. The first
// thing a stroke's points pass through is its contact: the point, where, and which way it went. The
// stroke's blow lands on it once: as the stroke becomes a blow (VR_Blow_IsBlow) if the contact came
// earlier in the stroke, or at the contact while the stroke goes on (PlayerVRMeleeImpl); then the hand
// must stop or pull back before it strikes again. Things that bleed come first: a wall is struck only
// if nothing else is, when it was touched in the last VR_PT_WALL_AGE seconds, and not by a point going
// down (a hand reaching for a hip holster).

float VR_PT_RADIUS = 3;      // units around a point's sweep that it strikes (times vr_melee_range_multiplier)
float VR_PT_HILT = 0.6;      // a sword's hilt (a pommel strike): its damage multiplier
float VR_PT_HANDLE = 0.5;    // the axe's or Mjolnir's handle
float VR_PT_WALL_AGE = 0.1;  // s: how recent a wall's contact must be to be the blow's
float VR_PT_UPGRADE = 0.05;  // s: a weak point's contact in a hilt strike waits this long for a strong one's
float VR_PT_SWING_TURN = 150; // degrees/s: a weapon turning faster than this is swinging (a hilt strike hardly turns it)
float VR_PT_SWEEP = 1.5;     // a weapon whose tip sweeps this much further (faster) than the hand is swinging,
float VR_PT_SWEEP_NOW = 2.5; // or this much, and over 4 m/s, in one frame (the head lags the hand a little: a punch stopping has it faster)

// A striking point (a macro: the constant index is a plain global, not an array accessor's call;
// several statements, so only in a block).
#define VR_Blow_Point(xIndex, xAt, xMult, xName) \
    vr_pt[xIndex] = xAt; \
    vr_pt_mult[xIndex] = xMult; \
    vr_pt_name[xIndex] = xName; \
    vr_pt_count = xIndex + 1

// The striking points of `xHand` (see above), into vr_pt.
void(float xHand) VR_Blow_Points =
{
    vector h = VRGetHandPos(xHand);
    vector fwd = VRForwardOf(VRGetHandRot(xHand));
    float wid = VRGetEntWeapon(self, xHand);
    if(VRIsHandEmpty(self, xHand) || wid == WID_FIST)
    {
        VR_Blow_Point(0, h - fwd * 1, 1, "the fist");
        VR_Blow_Point(1, h + fwd * 4, 1, "the knuckles");
        vr_pt_axis = fwd;
        return;
    }

    vector m = VRGetMuzzlePos(xHand);
    if(m == '0 0 0' || vlen(m - h) > 80 || vlen(m - h) < 2)
    {
        m = h + fwd * 8;
    }
    vector a = m - h;
    vr_pt_axis = normalize(a);
    if(wid == WID_SWORD)
    {
        // The hand closes just under the guard; the grip and the pommel go on below it.
        VR_Blow_Point(0, h - a * 0.3, VR_PT_HILT, "the pommel");
        VR_Blow_Point(1, h, VR_PT_HILT, "the hilt");
        VR_Blow_Point(2, h + a * 0.12, 0.85, "the guard");
        VR_Blow_Point(3, h + a * 0.45, 1, "mid-blade");
        VR_Blow_Point(4, h + a * 0.72, 1, "the blade");
        VR_Blow_Point(5, m, 1, "the tip");
    }
    else if(wid == WID_AXE || wid == WID_MJOLNIR)
    {
        VR_Blow_Point(0, h, VR_PT_HANDLE, "the hand");
        VR_Blow_Point(1, h + a * 0.5, VR_PT_HANDLE, "the handle");
        VR_Blow_Point(2, h + a * 0.8, 1, "the head");
        VR_Blow_Point(3, m, 1, "the head");
    }
    else
    {
        VR_Blow_Point(0, h, 1, "the grip");
        VR_Blow_Point(1, h + a * 0.5, 1, "the barrel");
        VR_Blow_Point(2, m, 1, "the muzzle");
    }
};

// (VR_SegmentBox, a line into a box: vr_carry.qc.)

// The least distance between the segments `xA0` .. `xA1` and `xB0` .. `xB1`.
float(vector xA0, vector xA1, vector xB0, vector xB1) VR_SegmentDistance =
{
    vector d1 = xA1 - xA0;
    vector d2 = xB1 - xB0;
    vector r = xA0 - xB0;
    float a = d1 * d1;
    float e = d2 * d2;
    float f = d2 * r;
    float s = 0;
    float t = 0;
    if(a <= 0.0001 && e <= 0.0001)
    {
        return vlen(r);
    }
    if(a <= 0.0001)
    {
        t = min(1, max(0, f / e));
    }
    else
    {
        float c = d1 * r;
        if(e <= 0.0001)
        {
            s = min(1, max(0, -c / a));
        }
        else
        {
            float bb = d1 * d2;
            float denom = a * e - bb * bb;
            if(denom > 0.0001)
            {
                s = min(1, max(0, (bb * f - c * e) / denom));
            }
            t = (bb * s + f) / e;
            if(t < 0)
            {
                t = 0;
                s = min(1, max(0, -c / a));
            }
            else if(t > 1)
            {
                t = 1;
                s = min(1, max(0, (bb - c) / a));
            }
        }
    }
    return vlen((xA0 + d1 * s) - (xB0 + d2 * t));
};

// Whether `e` can be struck by the hands of `self`: it bleeds (or breaks), it is solid, and it is not
// the player's own (held or thrown).
float(entity e) VR_Blow_Strikable =
{
    return e != self && e != world && e.takedamage && e.solid != SOLID_NOT && e.solid != SOLID_TRIGGER &&
           e.owner != self && e.carry_player != self;
};

// Sweeps the striking points of `xHand` (b.blow_pp to b.blow_cp) for the stroke's first contacts: with
// something that bleeds, and with a wall. Of the points striking in the same frame, the strongest
// (the blade over the hilt), then the earliest along the sweep. A weak point's contact (a sword's
// hilt, pommel or guard, an axe's handle) gives way to a strong one's for as long as it hasn't landed
// (VR_Blow_Contact: a hilt strike's waits VR_PT_UPGRADE, a swing's vr_melee_hilt_window, and a swing's
// never lands: in a big swing the hands reach a monster before the blade, which sweeps in after).
void(float xHand, entity b) VR_Blow_Sweep =
{
    float upgrade = FALSE;
    if(b.blow_hit_stroke == b.blow_t0)
    {
        upgrade = b.blow_hit_mult < 1;
    }
    float wantHit = b.blow_hit_stroke != b.blow_t0 || upgrade;
    float n = b.blow_ppn;
    if(n <= 0)
    {
        return;
    }
    float r = VR_PT_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    vector hand = VRGetHandPos(xHand);

    float bestT = 2;
    float bestM = upgrade ? 0.99 : 0; // (upgrading: only the blade or head, not the guard over the pommel)
    float best = -1;
    entity bestEnt = world;
    vector bestAt = '0 0 0';
    float wallT = 2;
    float wall = -1;
    entity wallEnt = world;
    vector wallAt = '0 0 0';
    vector wallNormal = '0 0 0';
    float i;
    vector p0;
    vector p1;
    float t;

    // Walls and brush entities (a line each, exact against the BSP; it finds monsters too).
    for(i = n - 1; i >= 0; --i)
    {
        p0 = b.blow_pp[i];
        p1 = b.blow_cp[i];
        if(p0 == p1)
        {
            continue;
        }
        traceline(p0, p1, FALSE, self);
        if(trace_startsolid || trace_fraction >= 1)
        {
            continue;
        }
        if(VR_Blow_Strikable(trace_ent))
        {
            if(wantHit && (b.blow_pmult[i] > bestM || (b.blow_pmult[i] == bestM && best >= 0 && trace_fraction < bestT)))
            {
                bestM = b.blow_pmult[i];
                bestT = trace_fraction;
                best = i;
                bestEnt = trace_ent;
                bestAt = trace_endpos;
            }
        }
        else if(trace_fraction < wallT && p1_z - p0_z >= -0.35 * vlen(p1 - p0))
        {
            wallT = trace_fraction;
            wall = i;
            wallEnt = trace_ent;
            wallAt = trace_endpos;
            wallNormal = trace_plane_normal;
        }
    }

    // Monsters' boxes, widened by the points' thickness (and not through a wall).
    if(wantHit)
    {
        vector mid = '0 0 0';
        for(i = 0; i < n; ++i)
        {
            mid = mid + b.blow_pp[i] + b.blow_cp[i];
        }
        mid = mid * (0.5 / n);
        float reach = 0;
        for(i = 0; i < n; ++i)
        {
            reach = max(reach, max(vlen(b.blow_pp[i] - mid), vlen(b.blow_cp[i] - mid)));
        }

        entity e = findradius(mid, reach + r + 96);
        while(e)
        {
            if(VR_Blow_Strikable(e) && (e.solid == SOLID_SLIDEBOX || e.solid == SOLID_BBOX))
            {
                vector lo = e.origin + e.mins;
                vector hi = e.origin + e.maxs;
                for(i = n - 1; i >= 0; --i)
                {
                    p0 = b.blow_pp[i];
                    p1 = b.blow_cp[i];
                    t = VR_SegmentBox(p0, p1, lo - '1 1 1' * r, hi + '1 1 1' * r);
                    if(t >= 0 && (b.blow_pmult[i] > bestM || (b.blow_pmult[i] == bestM && best >= 0 && t < bestT)))
                    {
                        // On the box itself.
                        vector at = p0 + (p1 - p0) * t;
                        at_x = min(hi_x, max(lo_x, at_x));
                        at_y = min(hi_y, max(lo_y, at_y));
                        at_z = min(hi_z, max(lo_z, at_z));
                        traceline(hand, at, TRUE, self);
                        if(trace_fraction >= 1)
                        {
                            bestM = b.blow_pmult[i];
                            bestT = t;
                            best = i;
                            bestEnt = e;
                            bestAt = at;
                        }
                    }
                }
            }
            e = e.chain;
        }
    }

    if(best >= 0)
    {
        if(b.blow_hit_stroke != b.blow_t0)
        {
            b.blow_hit_time = time; // (an upgrade keeps the first contact's time)
            // A weak point's contact: is the weapon swinging past it (its tip sweeping round much
            // further than the hand) or striking with it (the pommel leading, or the weapon driven
            // along, as a pommel punch or a hilt brought down)?
            b.blow_hit_swing = FALSE;
            if(b.blow_pmult[best] < 1)
            {
                float hi = n == 6 ? 1 : 0; // the hand's point (a sword's hilt; the axe's hand)
                vector tipAt = b.blow_cp[n - 1];
                vector handAt = b.blow_cp[hi];
                float handSweep = vlen(handAt - b.blow_pp[hi]);
                vector way = normalize(b.blow_cp[best] - b.blow_pp[best]);
                float swinging = FALSE; // (a local: see VR_Blow_IsBlow)
                if(vlen(tipAt - b.blow_pp[n - 1]) > handSweep * VR_PT_SWEEP + 0.5)
                {
                    swinging = way * normalize(tipAt - handAt) > -0.5;
                }
                b.blow_hit_swing = swinging;
            }
        }
        b.blow_hit_stroke = b.blow_t0;
        b.blow_hit_ent = bestEnt;
        b.blow_hit_pos = bestAt;
        b.blow_hit_dir = normalize(b.blow_cp[best] - b.blow_pp[best]);
        b.blow_hit_mult = b.blow_pmult[best];
        b.blow_hit_name = vr_pt_name[best];
    }
    if(wall >= 0 && (b.blow_wall_stroke != b.blow_t0 || time - b.blow_wall_time > VR_PT_WALL_AGE))
    {
        b.blow_wall_stroke = b.blow_t0;
        b.blow_wall_time = time;
        b.blow_wall_ent = wallEnt;
        b.blow_wall_pos = wallAt;
        b.blow_wall_dir = normalize(b.blow_cp[wall] - b.blow_pp[wall]);
        b.blow_wall_normal = wallNormal;
        b.blow_wall_name = vr_pt_name[wall];
    }
};

// The contact the blow of `xHand` lands on (see above), in the trace_ globals (trace_ent, trace_endpos,
// trace_plane_normal, trace_fraction) and v_forward (the striking point's way), as a traceline's hit;
// FALSE while there is none (a later frame of the stroke may find one).
float(float xHand) VR_Blow_Contact =
{
    entity b = VR_Blow(xHand);
    if(!b.blow_t0)
    {
        return FALSE;
    }
    if(b.blow_hit_stroke == b.blow_t0 && b.blow_hit_mult < 1)
    {
        // A hilt or handle: the blade or head may follow it in (VR_Blow_Sweep). A hilt strike lands
        // after VR_PT_UPGRADE; a swing's hilt never does: once vr_melee_hilt_window is over it is
        // forgotten, and the swing may still land with the blade later in the stroke.
        // A swing: the weapon swinging past the contact as it came, or turning fast, or its tip going
        // (or having gone, in the stroke) much faster than the wrist: a swing's start is slow, and it
        // may touch first then. A pommel punch or a hilt brought down moves the weapon along whole.
        if(b.blow_axisrate > VR_PT_SWING_TURN || b.blow_tippeak > b.blow_peak * VR_PT_SWEEP ||
           b.blow_tipspeed > max(4, b.blow_speed * VR_PT_SWEEP_NOW))
        {
            b.blow_hit_swing = TRUE;
        }
        if(time - b.blow_hit_time < (b.blow_hit_swing ? max(VR_PT_UPGRADE, cvar_hget(cvarh_vr_melee_hilt_window))
                                                       : VR_PT_UPGRADE))
        {
            return FALSE;
        }
        if(b.blow_hit_swing)
        {
            b.blow_hit_stroke = 0;
            return FALSE;
        }
    }
    if(b.blow_hit_stroke == b.blow_t0 && b.blow_hit_ent.takedamage && b.blow_hit_ent.solid != SOLID_NOT)
    {
        trace_ent = b.blow_hit_ent;
        trace_endpos = b.blow_hit_pos;
        trace_plane_normal = b.blow_hit_dir * -1;
        v_forward = b.blow_hit_dir;
    }
    else if(b.blow_wall_stroke == b.blow_t0 && time - b.blow_wall_time <= VR_PT_WALL_AGE)
    {
        trace_ent = b.blow_wall_ent;
        trace_endpos = b.blow_wall_pos;
        trace_plane_normal = b.blow_wall_normal;
        v_forward = b.blow_wall_dir;
    }
    else
    {
        return FALSE;
    }
    trace_fraction = 0.5;
    trace_startsolid = FALSE;
    trace_allsolid = FALSE;
    return TRUE;
};

// Whether `e` is a monster's projectile in flight (a knight's spike, an enforcer's laser, a
// scrag's spit, a vore's ball, an ogre's grenade, ...).
float(entity e) VR_Deflect_IsProjectile =
{
    if(!(e.owner.flags & FL_MONSTER) || e.solid == SOLID_NOT || !e.touch || e.takedamage)
    {
        return FALSE;
    }
    return e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_BOUNCE || e.movetype == MOVETYPE_FLY ||
           e.movetype == MOVETYPE_BOUNCEMISSILE;
};

// Batting projectiles back (vr_deflect; docs/vr-port/ROUND15.md). A batting swing is a stroke of the
// wrist that got to vr_deflect_speed times vr_melee_speed and still goes at half that: much less than
// a blow (no reach, snap or shape needed). Each frame of it, the weapon's line (from its first striking
// point to its last: the hilt to the tip, the fist to the knuckles) is kept, for vr_deflect_window
// seconds. An enemy projectile whose path this frame (from a little behind it, VR_BAT_LATE, to where it
// will be at the frame's end) comes within vr_deflect_radius (plus its size) of any of those lines is
// batted back: so a swing a little early still bats what flies into where the weapon went, and the
// frame rate doesn't matter (the paths are swept). It flies back where the hand points (at its
// thrower, when that is within VR_BAT_AIM degrees of it), faster, and now hurts monsters.
float VR_BAT_KEEP = 8;   // weapon lines kept (a ring)
float VR_BAT_LATE = 0.05; // s: a projectile's path starts this far behind it (just past the weapon)
float VR_BAT_AIM = 25;    // degrees: a thrower this near where the hand points gets its projectile back

// Sends the projectile `e` back along `xDir` (at its thrower, when that is within `xAim` degrees of
// it), faster; `xHand` feels it (-1: both hands).
void(entity e, vector xDir, float xHand, float xAim) VR_Deflect_Send =
{
    entity oldOwner = e.owner;
    vector dir = xDir;
    vector toOwner = normalize(oldOwner.origin + (oldOwner.mins + oldOwner.maxs) * 0.5 - e.origin);
    if(oldOwner.health > 0 && VRAngleBetween(dir, toOwner) < xAim)
    {
        dir = toOwner;
    }

    float speed = max(500, vlen(e.velocity)) * 1.2;
    e.velocity = dir * speed;
    e.angles = vectoangles(e.velocity);
    e.owner = self;
    e.enemy = oldOwner; // homing ones (a vore's ball) go for their thrower
    e.vr_deflect_time = time;

    sound(self, CHAN_AUTO, "weapons/tink1.wav", 1, ATTN_NORM);
    sound(self, CHAN_AUTO, "player/axhit2.wav", 0.6, ATTN_NORM);
    particle2(e.origin, dir * 120, QVR_PARTICLE_PRESET_SPARKS, 20);
    VR_Motion_Event("deflect", e.classname, xHand, 0, e.origin, oldOwner, xAim == VR_BASH_AIM ? "bash" : "swing");
    if(xHand != cVR_OffHand)
    {
        haptic(cVR_MainHand, 0.0, 0.2, 90, 1.0);
        handimpact(cVR_MainHand, 3, dir * -1);
    }
    if(xHand != cVR_MainHand)
    {
        haptic(cVR_OffHand, 0.0, 0.2, 90, 1.0);
        handimpact(cVR_OffHand, 3, dir * -1);
    }
};

void(float xHand, entity b) VR_Deflect = // (b: VR_Blow(xHand))
{
    if(!cvar_hget(cvarh_vr_deflect))
    {
        return;
    }

    float n = b.blow_ppn;
    if(n > 0 && b.blow_t0) // (in a stroke: most frames are not)
    {
        float need = max(0.5, cvar_hget(cvarh_vr_melee_speed)) * max(0, cvar_hget(cvarh_vr_deflect_speed));
        if(b.blow_peak >= need && b.blow_speed >= need * 0.5)
        {
            b.bat_i = b.bat_i + 1;
            if(b.bat_i >= VR_BAT_KEEP)
            {
                b.bat_i = 0;
            }
            b.bat_a[b.bat_i] = b.blow_cp[0];
            b.bat_b[b.bat_i] = b.blow_cp[n - 1];
            b.bat_t[b.bat_i] = time;
            b.bat_newest = time;
        }
    }

    float window = max(0, cvar_hget(cvarh_vr_deflect_window));
    if(time - b.bat_newest > window) // (bat_t[bat_i], without the array's accessor call)
    {
        return; // no batting swing lately
    }
    float radius = max(0, cvar_hget(cvarh_vr_deflect_radius));
    float dt = max(frametime, 0.011);

    entity e = findradius(VRGetHandPos(xHand), 160);
    while(e)
    {
        if(VR_Deflect_IsProjectile(e) && time >= e.vr_deflect_time + 0.3)
        {
            vector from = e.origin - e.velocity * max(dt, VR_BAT_LATE);
            vector to = e.origin + e.velocity * dt;
            float near = 9999;
            float when = 0;
            for(float k = 0; k < VR_BAT_KEEP; ++k)
            {
                if(b.bat_t[k] > 0 && time - b.bat_t[k] <= window)
                {
                    float d = VR_SegmentDistance(from, to, b.bat_a[k], b.bat_b[k]);
                    if(d < near)
                    {
                        near = d;
                        when = time - b.bat_t[k];
                    }
                }
            }
            if(near < radius + e.maxs_x)
            {
                dprint(sprintf("deflect: %s of %s, %.0f units from the swing's line of %.2f s ago\n", e.classname,
                    e.owner.classname, near, when));
                VR_Deflect_Send(e, VRForwardOf(VRGetHandRot(xHand)), xHand, VR_BAT_AIM);
            }
        }
        e = e.chain;
    }
};

// A heartbeat felt in both hands when health is low (vr_heartbeat, below 30), faster the lower it
// is: a strong beat and a softer one after it.
void() VR_Heartbeat =
{
    if(!cvar_hget(cvarh_vr_heartbeat) || !self.ishuman || self.health <= 0 || self.health >= 30 ||
       time < self.vr_heartbeat_time)
    {
        return;
    }

    self.vr_heartbeat_time = time + 0.55 + 0.5 * (self.health / 30);
    haptic(cVR_OffHand, 0.0, 0.07, 25, 0.8);
    haptic(cVR_MainHand, 0.0, 0.07, 25, 0.8);
    haptic(cVR_OffHand, 0.18, 0.06, 25, 0.45);
    haptic(cVR_MainHand, 0.18, 0.06, 25, 0.45);
};

// ----------------------------------------------------------------------------
// Bashes and shoves (docs/vr-port/ROUND20.md, "Melee: blade, hilt, bash, shove").
//
// A BASH is the parry's guard pushed forward: a weapon held about level across in front, in one hand
// or two (VR_Parry_Guard: a sword by its grip or its blade, the axe, a gun held across), held there
// still -- the wrist slower than VR_BASH_STILL, the weapon turning less than half vr_bash_swing_rate --
// for vr_bash_hold seconds, then driven ahead (within 53 degrees of the facing) faster than
// vr_bash_speed within VR_BASH_WINDOW, the weapon still turning slower than vr_bash_swing_rate. A swing
// whose blade passes through level across on its way (a diagonal cut, a side swing from a guard)
// turns the blade far faster than that, however slow the hands: it is never a bash, it stays a swing
// and lands with the blade.
//
// A SHOVE is an open hand pushed at what is ahead: empty, not gripping (the grip held makes a fist),
// not steadying the other hand's weapon, not carrying or climbing; the palm facing ahead and the hand
// moving the way the palm faces, faster than vr_shove_speed, in a snap (VR_SHOVE_SNAP) and a few
// centimetres straight ahead (VR_SHOVE_TRAVEL). Both hands: a shove; one: half as strong. A slow reach,
// a punch (the palm faces in or down), a slap (the hand goes sideways) or a hand on a sword's grip is
// no shove.
//
// A weapon bash soon after a parry (VR_PARRYBASH_WINDOW) is a parry-bash: a counter, harder. Each has
// its own sound (vr_bash_sound; Misc/quakevr/make_sounds.py), apart from the blows' and the parry's.
// With batting on (vr_deflect), a bash, or a shove while a hand holds a weapon, bats back what meets
// the guard for a while (VR_Bash_Deflect).
// The motion is only a bash or a shove: the hands in it don't also punch or swing.

float VR_BASH_STILL = 1.0;       // m/s: the guard's wrist slower than this holds it (about) still
float VR_BASH_WINDOW = 0.75;     // s: the push must follow the guard held still this soon
float VR_GUARD_LOST = 0.25;      // s: a guard lost this briefly is still held (a push tilts it)
float VR_BASH_CONFIRM = 0.05;    // s: the push must go on this long, not swinging (a swing from a guard starts as a push)
float VR_PARRYBASH_WINDOW = 1.5; // s: a weapon bash this soon after a parry is a parry-bash
float VR_PARRYBASH_SCALE = 1.3;  // how much harder a parry-bash is (damage, push, stagger)
float VR_SHOVE_PALM = 0.6;       // cosine: the palm faces ahead (within 53 degrees)
float VR_SHOVE_ALONG = 0.7;      // cosine: the hand goes the way its palm faces (within 45 degrees)
float VR_SHOVE_SNAP = 8;         // m/s2: the wrist's least mean acceleration to its fastest
float VR_SHOVE_TRAVEL = 0.1;     // m: the wrist's least travel, straight ahead
float VR_SHOVE_PAIR = 0.5;       // the other hand shoves with it pushing at this much of vr_shove_speed
float VR_BASH_AIM = 45;          // degrees: a thrower this near the facing gets its projectile back
float vr_bash_counter;           // while a bash's hits are dealt: a parry-bash (vr_dummy.qc)

// The guard `self` holds now, facing `xForward`: the hand holding a weapon across, or -1.
float(vector xForward) VR_Bash_Guard =
{
    if(VR_Parry_Guard(self, cVR_MainHand, xForward))
    {
        return cVR_MainHand;
    }
    if(VR_Parry_Guard(self, cVR_OffHand, xForward))
    {
        return cVR_OffHand;
    }
    return -1;
};

// Whether `xHand` of `self` drives forward (along the body's facing `xForward`) faster than `xSpeed`
// (m/s), and mostly forward (within 53 degrees: not a chop down, nor a swing across).
float(float xHand, vector xForward, float xSpeed) VR_Bash_Driving =
{
    vector v = VRGetEntHandVel(self, xHand);
    float ahead = v * xForward;
    return ahead > xSpeed && ahead > vlen(v) * 0.6;
};

// Whether `xHand` of `self` shoves, pushed ahead (`xForward`) faster than `xSpeed` (see above).
float(float xHand, vector xForward, float xSpeed) VR_Bash_Shoving =
{
    vector v = VRGetEntHandVel(self, xHand);
    float speed = vlen(v);
    if(v * xForward < max(xSpeed, speed * 0.8)) // (most frames: the hand is not pushing)
    {
        return FALSE;
    }
    if(!VRIsHandEmpty(self, xHand) || VR_HandGrabUtil_IsHandGrabbing(self, xHand) || VRGet2HAiming(self) ||
       VR_Carry_IsHolding(self, xHand) ||
       VRHasBit(self.vrbits0, xHand == cVR_MainHand ? QVR_VRBITS0_MAINHAND_CLIMBING : QVR_VRBITS0_OFFHAND_CLIMBING))
    {
        return FALSE; // not an open hand
    }

    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector palm = xHand == cVR_OffHand ? v_right : v_right * -1; // palms face in, as held
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    if(palm * xForward < VR_SHOVE_PALM || v * palm < speed * VR_SHOVE_ALONG)
    {
        return FALSE; // a punch, a slap, a reach
    }

    // A push, not a drift: in a snap, and a few centimetres straight ahead.
    entity b = VR_Blow(xHand);
    if(!b.blow_t0 || VR_Blow_Accel(b) < VR_SHOVE_SNAP)
    {
        return FALSE;
    }
    vector s = b.blow_wrist - b.blow_start;
    float ahead = s * VR_Blow_ToPlaySpace(xForward);
    return ahead > VR_SHOVE_TRAVEL && ahead > vlen(s) * 0.8;
};

// What is just ahead of a bash at `xAt` (along `xForward`) takes `xScale` of vr_bash_damage, is
// thrown back (vr_bash_push, scaled) and staggers (its pain animation, forced). How many.
float(vector xAt, vector xForward, float xKind, float xHand, float xScale) VR_Bash_Hit =
{
    float hits = 0;
    float push = cvar_hget(cvarh_vr_bash_push) * max(0.75, xScale); // a one-hand shove still throws (off ledges too)
    float dmg = cvar_hget(cvarh_vr_bash_damage) * xScale;
    entity e = findradius(xAt + xForward * 24, 48);
    while(e)
    {
        if(e != self && e.takedamage && e.health > 0 && (e.flags & FL_MONSTER) &&
           normalize(e.origin - self.origin) * xForward > 0.3)
        {
            hits = hits + 1;
            VR_SetHitKind(xKind, xHand, xAt + xForward * 12, xForward);
            T_Damage(e, self, self, dmg);
            VR_ClearHitKind();
            VR_Push(e, xForward, 520 * push, 140 * push);
            if(e.health > 0 && e.th_pain)
            {
                // Staggered: its pain, whatever its own chance of flinching.
                entity oldSelf = self;
                self = e;
                e.pain_finished = 0;
                e.th_pain(oldSelf, dmg);
                e.attack_finished = time + xScale;
                self = oldSelf;
            }
            SpawnBloodSplash(e.origin + '0 0 16', 4);
        }
        e = e.chain;
    }
    return hits;
};

// Batting with a bash (vr_deflect; round 20): for vr_bash_deflect_window seconds from the push, a
// monster's projectile whose path this frame (from a little behind it to where it will be at the
// frame's end) comes within vr_bash_deflect_radius of the guard -- each hand's weapon, from its butt to
// its far end (VR_Parry_WeaponLine), each empty hand, and the line between the hands while they are
// together -- is sent back ahead (at its thrower, within VR_BASH_AIM of the facing `xForward`).
// More lenient than a swing's batting (vr_deflect_radius, _window): a push covers less ground.
void(vector xForward) VR_Bash_Deflect =
{
    float radius = max(0, cvar_hget(cvarh_vr_bash_deflect_radius));
    float dt = max(frametime, 0.011);
    vector mh = VRGetHandPos(cVR_MainHand);
    vector oh = VRGetHandPos(cVR_OffHand);
    vector ma = mh;
    vector mb = mh;
    vector oa = oh;
    vector ob = oh;
    if(VR_Parry_WeaponLine(self, cVR_MainHand))
    {
        ma = vr_parry_butt;
        mb = vr_parry_end;
    }
    if(VR_Parry_WeaponLine(self, cVR_OffHand))
    {
        oa = vr_parry_butt;
        ob = vr_parry_end;
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

// A bash (vr_bash), or a shove (see above). What is just ahead takes a little damage (vr_bash_damage),
// is thrown back (vr_bash_push) and staggers (its pain animation, forced). Runs before the hands'
// melee (PlayerVRMelee in client.qc): the hands in it don't also punch or swing (they may hit again
// once they stop or pull back).
void() VR_Bash =
{
    if(!cvar_hget(cvarh_vr_bash) || !self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        self.vr_guard_hand = -1;
        self.vr_guard_time = 0;
        self.vr_guard_since = 0;
        return;
    }

    // The body's facing is the head's (.v_angle is the main hand's aim: a weapon held across in
    // front aims sideways).
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);
    if(time <= self.vr_bashdef_until)
    {
        VR_Bash_Deflect(fwd); // a bash's batting goes on
    }

    // The guard, and when it was last ready (held still for vr_bash_hold): the push must follow it. A
    // guard lost for a moment (VR_GUARD_LOST: a push tilts the blade) is still the one held; any longer
    // (the weapon turned to thrust, say) and the push is no bash.
    float swingRate = max(10, cvar_hget(cvarh_vr_bash_swing_rate));
    float guard = VR_Bash_Guard(fwd);
    if(guard >= 0)
    {
        if(guard != self.vr_guard_hand)
        {
            self.vr_guard_hand = guard;
            self.vr_guard_time = 0;
            self.vr_guard_since = 0;
        }
        self.vr_guard_seen = time;
        entity gb = VR_Blow(guard);
        if(gb.blow_speed < VR_BASH_STILL && gb.blow_axisrate < swingRate * 0.5)
        {
            if(!self.vr_guard_since)
            {
                self.vr_guard_since = time;
            }
            if(time - self.vr_guard_since >= cvar_hget(cvarh_vr_bash_hold))
            {
                self.vr_guard_time = time;
            }
        }
        else
        {
            self.vr_guard_since = 0;
        }
    }
    else
    {
        self.vr_guard_since = 0;
        if(time - self.vr_guard_seen < VR_GUARD_LOST)
        {
            guard = self.vr_guard_hand;
            if(guard >= 0 && VRIsHandEmpty(self, guard))
            {
                guard = -1; // the weapon is gone from the hand
            }
        }
    }

    float armed = FALSE;
    float both = FALSE;
    float hand = -1;
    // The push: the guard ready, then driven ahead, the weapon not turning as a swing does -- for
    // VR_BASH_CONFIRM (a swing started from a guard begins as a push: its blade turns fast a frame
    // later), and not in a stroke in which it already swung.
    float pushing = FALSE;
    if(guard >= 0)
    {
        entity pb = VR_Blow(guard);
        if(pb.blow_axisrate >= swingRate && pb.blow_t0)
        {
            pb.blow_swung = pb.blow_t0;
        }
        if(self.vr_guard_time > 0 && time - self.vr_guard_time < VR_BASH_WINDOW &&
           VR_Bash_Driving(guard, fwd, cvar_hget(cvarh_vr_bash_speed)))
        {
            if(pb.blow_axisrate < swingRate && !(pb.blow_t0 && pb.blow_swung == pb.blow_t0))
            {
                pushing = TRUE;
                if(!self.vr_push_since)
                {
                    self.vr_push_since = time;
                }
                VR_Bash_DeflectStart(guard, fwd); // (batting from the push's start: every moment counts)
                if(time - self.vr_push_since < VR_BASH_CONFIRM)
                {
                    return; // (not a shove either: wait and see)
                }
                hand = guard;
                armed = TRUE;
            }
            else if(pb.blow_t0 && self.vr_guard_note != pb.blow_t0)
            {
                // For tuning (vr_bash_swing_rate): a guard pushed, but swinging.
                self.vr_guard_note = pb.blow_t0;
                dprint(sprintf("bash: none, the weapon turns %.0f deg/s (a swing: Bash Swing Limit %.0f)\n",
                    pb.blow_axisrate, swingRate));
            }
        }
    }
    if(!pushing)
    {
        self.vr_push_since = 0;
    }
    if(hand < 0)
    {
        float shoveSpeed = cvar_hget(cvarh_vr_shove_speed);
        float mainShove = VR_Bash_Shoving(cVR_MainHand, fwd, shoveSpeed);
        float offShove = VR_Bash_Shoving(cVR_OffHand, fwd, shoveSpeed);
        if(!mainShove && !offShove)
        {
            return;
        }
        hand = mainShove ? cVR_MainHand : cVR_OffHand;
        // The other hand pushing with it (a little behind) makes it a two-hand shove.
        if(mainShove && offShove)
        {
            both = TRUE;
        }
        else
        {
            both = VR_Bash_Shoving(mainShove ? cVR_OffHand : cVR_MainHand, fwd, shoveSpeed * VR_SHOVE_PAIR);
        }
    }
    float oneHand = TRUE;
    if(armed || both)
    {
        oneHand = FALSE;
    }

    // The motion recorder (vr_motion.qc): a push, whatever it meets; once a push (its frames in a row).
    if(time - self.vr_motion_pushseen > 0.15)
    {
        VR_Motion_Event("push", armed ? "bash" : "shove", both ? -1 : hand, 0, VRGetHandPos(hand), world,
                        oneHand ? "one hand" : "two hands");
    }
    self.vr_motion_pushseen = time;

    // No punches or swings in this motion, even during the bash's cooldown: the hands' melee counts
    // a swing as already landed until the hand stops or pulls back (PlayerVRMeleeImpl). A weapon
    // bash or a one-handed shove keeps the other hand free, unless it drives forward right by the
    // weapon (held in both hands).
    VRSetMeleeHitSoundPlayed(hand, TRUE);
    float other = hand == cVR_MainHand ? cVR_OffHand : cVR_MainHand;
    if(both || (armed && VR_Bash_Driving(other, fwd, cvar_hget(cvarh_vr_bash_speed)) &&
                vlen(VRGetHandPos(other) - VRGetHandPos(hand)) < 24))
    {
        VRSetMeleeHitSoundPlayed(other, TRUE);
    }

    // Batting (vr_deflect): a bash, or a shove while a hand holds a weapon, sends back what meets the
    // guard from now on, for vr_bash_deflect_window seconds.
    if(armed || !VRIsHandEmpty(self, cVR_MainHand) || !VRIsHandEmpty(self, cVR_OffHand))
    {
        VR_Bash_DeflectStart(both ? -1 : hand, fwd);
    }

    if(time < self.vr_bash_time)
    {
        return;
    }

    // What is just ahead of the hands (a weapon held in both, or both hands: between them).
    vector at = VRGetHandPos(hand);
    if(both || (armed && VRGet2HAiming(self)))
    {
        at = (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5;
    }
    // A weapon bash soon after a parry: a parry-bash, the counter.
    float counter = armed && self.vr_parry_time > 0 && time - self.vr_parry_time < VR_PARRYBASH_WINDOW;
    vr_bash_counter = counter;
    float hits = VR_Bash_Hit(at, fwd, armed ? QVR_HITKIND_BASH : QVR_HITKIND_SHOVE, both ? -1 : hand,
                             oneHand ? 0.5 : (counter ? VR_PARRYBASH_SCALE : 1));
    vr_bash_counter = FALSE;
    if(!hits)
    {
        return; // a push at the air: nothing, no cooldown
    }
    self.vr_bash_time = time + 0.8;
    if(counter)
    {
        self.vr_parry_time = 0; // one counter a parry
    }
    // Each its own sound (vr_bash_sound; round 18), apart from the blows'; 0: the old ones.
    float bashSound = min(1, cvar_hget(cvarh_vr_bash_sound));
    if(bashSound > 0)
    {
        sound(self, CHAN_AUTO, counter ? "vr/bash_parry.wav" : (armed ? "vr/bash.wav" : "vr/shove.wav"),
              bashSound * (oneHand ? 0.7 : 1), ATTN_NORM);
    }
    else
    {
        sound(self, CHAN_AUTO, armed ? "player/axhit2.wav" : "fisthit.wav", 1, ATTN_NORM);
        sound(self, CHAN_AUTO, "demon/dland2.wav", oneHand ? 0.45 : 0.7, ATTN_NORM);
    }
    if(oneHand)
    {
        haptic(hand, 0.0, 0.2, 70, 0.8);
        handimpact(hand, 2, fwd * -1);
    }
    else
    {
        haptic(cVR_MainHand, 0.0, 0.25, 70, 1.0);
        haptic(cVR_OffHand, 0.0, 0.25, 70, 1.0);
        handimpact(cVR_MainHand, 3, fwd * -1);
        handimpact(cVR_OffHand, 3, fwd * -1);
    }
    particle2(at + fwd * 12, fwd * 80, QVR_PARTICLE_PRESET_BULLETPUFF, oneHand ? 10 : 16);
    dprint("bash: "); dprint(ftos(hits));
    dprint(counter ? " (parry-bash)\n" : (armed ? " (weapon)\n" : (oneHand ? " (one hand)\n" : " (hands)\n")));
};
