// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat);
// - bashing (vr_bash): a guard held in front (a weapon across, as for a parry, or both empty hands
//   together) and then driven forward knocks monsters back and staggers them; so does an open palm.
// The hands' blows are here too: the melee's model (what makes a motion a punch or a swing, and how
// hard).
// Hit reactions (vr_hit_push), explosion rumble (vr_explosion_rumble) and directional damage haptics
// are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat
.float vr_bash_time;      // on a player: when the next bash may be
.float vr_guard_hand;     // on a player: the guard held (VR_Bash_Guard),
.float vr_guard_time;     // and when it was last held still (0: not yet)

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

float VR_BLOW_KIND_PUNCH = 0;
float VR_BLOW_KIND_SLAP = 1;
float VR_BLOW_KIND_DOWN = 2;
float VR_BLOW_KIND_UP = 3;
float VR_BLOW_KIND_BACK = 4;
float VR_BLOW_KIND_SWING = 5; // a swung weapon's arc across
float VR_BLOW_KIND_THRUST = 6; // a swung weapon driven straight ahead
float VR_BLOW_KIND_RISING = 7; // a swung weapon swung upwards

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
vector(vector xVec) VR_Blow_ToPlaySpace =
{
    float a = -self.vryaw * (3.14159265 / 180);
    float c = cos(a);
    float s = sin(a);
    vector r;
    r_x = xVec_x * c - xVec_y * s;
    r_y = xVec_x * s + xVec_y * c;
    r_z = xVec_z;
    return r;
};

// The wrist of `xHand`, in metres from the head, in the play space: behind the controller's
// point along the hand's forward, and below it (the controller sits above the fist, ahead of it).
vector(float xHand) VR_Blow_Wrist =
{
    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    float m2u = VRMetersToUnits();
    vector w = VRGetHandPos(xHand) - VRGetHeadPos(self) - v_forward * (0.13 * m2u) - v_up * (0.085 * m2u);
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    return VR_Blow_ToPlaySpace(w) * (1 / m2u);
};

// The striking point of `xHand`, likewise: a weapon's head (its muzzle point), or the fist.
vector(float xHand) VR_Blow_Tip =
{
    vector p = VRGetHandPos(xHand);
    vector m = VRGetMuzzlePos(xHand);
    if(!VRIsHandEmpty(self, xHand) && m != '0 0 0' && vlen(m - p) < 80)
    {
        p = m;
    }
    return VR_Blow_ToPlaySpace(p - VRGetHeadPos(self)) * (1 / VRMetersToUnits());
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

// Follows the stroke of `xHand`, once a frame (PlayerVRMelee, before the bashes and the blows).
void(float xHand) VR_Blow_Update =
{
    entity b = VR_Blow(xHand);
    float dt = time - b.blow_time;
    vector w = VR_Blow_Wrist(xHand);
    vector d = w - b.blow_wrist;
    vector tip = VR_Blow_Tip(xHand);
    vector dtip = tip - b.blow_tip;

    // The same pose again (a server frame with no new tracking): no news, not a stop; the next
    // pose's motion spans both frames.
    if(d == '0 0 0' && dtip == '0 0 0' && dt > 0 && dt < 0.05)
    {
        return;
    }
    b.blow_time = time;
    b.blow_wrist = w;
    b.blow_tip = tip;

    // A gap (the hands lost), or a jump (a teleport, the play space re-based, a weapon taken): start
    // over.
    if(!VRHandsTracked(self) || dt <= 0 || dt > 0.1 || vlen(d) > 0.3 || vlen(dtip) > 0.6)
    {
        b.blow_t0 = 0;
        b.blow_speed = 0;
        b.blow_tipspeed = 0;
        b.blow_vel = '0 0 0';
        return;
    }

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
           time - b.blow_t0 > 0.5)
        {
            b.blow_t0 = 0;
        }
    }
    if(!b.blow_t0)
    {
        if(b.blow_speed < VR_BLOW_MOVING)
        {
            return;
        }
        // A new stroke, from the last still point (or the turn).
        b.blow_t0 = time - dt;
        b.blow_start = w - d;
        b.blow_peak = 0;
        b.blow_peaktime = time;
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
        b.blow_peaktime = time;
    }
    b.blow_tippeak = max(b.blow_tippeak, b.blow_tipspeed);
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
float(float xHand) VR_Blow_IsBlow =
{
    entity b = VR_Blow(xHand);
    float speed = VR_Blow_MinSpeed(VR_Blow_Class(xHand));
    if(!b.blow_t0 || b.blow_peak < speed || b.blow_speed < speed * 0.5 || VR_Blow_Accel(b) < VR_BLOW_SNAP)
    {
        return FALSE;
    }
    vector s = b.blow_wrist - b.blow_start;
    float dist = vlen(s);
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
    return "punch";
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

// During a melee swing of `xHand` (self is the player): enemy projectiles near the weapon (from the
// hand to its muzzle, or the fist) fly back where the hand points, faster, and now hurt monsters.
void(float xHand) VR_Deflect =
{
    if(!cvar_hget(cvarh_vr_deflect))
    {
        return;
    }

    vector a = VRGetHandPos(xHand);
    vector b = VRGetMuzzlePos(xHand);
    if(b == '0 0 0' || vlen(b - a) > 80)
    {
        b = a;
    }
    vector mid = (a + b) * 0.5;
    float len = vlen(b - a);
    vector along = len > 0 ? (b - a) * (1 / len) : '0 0 0';

    entity e = findradius(mid, len * 0.5 + 40);
    while(e)
    {
        if(VR_Deflect_IsProjectile(e) && time >= e.vr_deflect_time + 0.3)
        {
            // Its distance from the weapon (a segment).
            float t = min(len, max(0, (e.origin - a) * along));
            float d = vlen(e.origin - (a + along * t));
            if(d < 16 + e.maxs_x)
            {
                entity oldOwner = e.owner;
                vector dir = VRForwardOf(VRGetHandRot(xHand));

                float speed = max(500, vlen(e.velocity)) * 1.2;
                e.velocity = dir * speed;
                e.angles = vectoangles(e.velocity);
                e.owner = self;
                e.enemy = oldOwner; // homing ones (a vore's ball) go for their thrower
                e.vr_deflect_time = time;

                sound(self, CHAN_AUTO, "weapons/tink1.wav", 1, ATTN_NORM);
                sound(self, CHAN_AUTO, "player/axhit2.wav", 0.6, ATTN_NORM);
                particle2(e.origin, dir * 120, QVR_PARTICLE_PRESET_SPARKS, 20);
                haptic(xHand, 0.0, 0.2, 90, 1.0);
                handimpact(xHand, 3, dir * -1);
                dprint("deflect: "); dprint(e.classname); dprint(" of "); dprint(oldOwner.classname); dprint("\n");
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
// Bashes. A bash is a guard pushed forward: the guard held first, still, then driven straight ahead
// (not down: a chop from a raised weapon is a swing) faster than vr_bash_speed -- less than a blow
// needs. The guards are the parry's (combat.qc): a weapon held across in front, about level, at chest
// or face height (VR_Parry_Guard), or both empty hands together in front (VR_Parry_HandsTogether).
// An open palm driven straight ahead over a blow's distance is a one-hand shove, with no guard.

float VR_GUARD_HANDS = 2;   // vr_guard_hand: both empty hands together (else the hand with the weapon; -1 none)
float VR_BASH_STILL = 1.0;  // m/s: the guard's wrists slower than this hold it still
float VR_BASH_WINDOW = 0.4; // s: the push must follow the guard held still this soon

// The guard `self` holds now, facing `xForward`: the hand holding a weapon across, VR_GUARD_HANDS,
// or -1.
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
    if(VR_Parry_HandsTogether(self, xForward))
    {
        return VR_GUARD_HANDS;
    }
    return -1;
};

// Whether the hands of guard `xGuard` are held still (their wrists, from the head).
float(float xGuard) VR_Bash_GuardStill =
{
    if(xGuard == VR_GUARD_HANDS)
    {
        return VR_Blow(cVR_MainHand).blow_speed < VR_BASH_STILL && VR_Blow(cVR_OffHand).blow_speed < VR_BASH_STILL;
    }
    return VR_Blow(xGuard).blow_speed < VR_BASH_STILL;
};

// Whether `xHand` of `self` drives forward (along the body's facing `xForward`) faster than
// vr_bash_speed (m/s), and mostly forward (within 45 degrees: not a chop down, nor a swing across).
float(float xHand, vector xForward) VR_Bash_Driving =
{
    vector v = VRGetEntHandVel(self, xHand);
    float ahead = v * xForward;
    return ahead > cvar_hget(cvarh_vr_bash_speed) && ahead > vlen(v) * 0.7;
};

// Whether empty `xHand` of `self` shoves on its own, driven forward (along `xForward`): the open
// palm facing ahead (fingers up or out; a fist's palm faces in or down), over a blow's distance
// (vr_melee_distance) straight ahead (a wiggle of an open hand is no shove).
float(float xHand, vector xForward) VR_Bash_PalmShove =
{
    if(!VRIsHandEmpty(self, xHand) || VR_Carry_IsHolding(self, xHand))
    {
        return FALSE;
    }

    vector oldForward = v_forward;
    vector oldRight = v_right;
    vector oldUp = v_up;
    makevectors(VRGetHandRot(xHand));
    vector palm = xHand == cVR_OffHand ? v_right : v_right * -1; // palms face in, as held
    v_forward = oldForward;
    v_right = oldRight;
    v_up = oldUp;
    if(palm * xForward < 0.6)
    {
        return FALSE;
    }

    // Straight ahead, not curving: a slap's arc starts ahead too, palm first.
    entity b = VR_Blow(xHand);
    float dist = VR_Blow_Distance(xHand);
    vector fwd = VR_Blow_ToPlaySpace(xForward);
    return dist >= cvar_hget(cvarh_vr_melee_distance) && (b.blow_wrist - b.blow_start) * fwd > dist * 0.9 &&
           b.blow_vel * fwd > b.blow_speed * 0.85;
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

// A bash (vr_bash): a guard held still and then driven forward -- a weapon held across in front
// (the parry pose), or both empty hands together (crossed or side by side: a shove) -- or one empty
// hand with the open palm ahead (a shove half as strong). What is just ahead takes a little damage
// (vr_bash_damage), is thrown back (vr_bash_push) and staggers (its pain animation, forced). The
// motion is only a bash: the hands in it don't also punch or swing (runs before the hands' melee,
// PlayerVRMelee in client.qc; they may hit again once they stop or pull back).
void() VR_Bash =
{
    if(!cvar_hget(cvarh_vr_bash) || !self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        self.vr_guard_hand = -1;
        self.vr_guard_time = 0;
        return;
    }

    // The body's facing is the head's (.v_angle is the main hand's aim: a weapon held across in
    // front aims sideways).
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);

    // The guard, and when it was last held still: the push must follow it.
    float guard = VR_Bash_Guard(fwd);
    if(guard != self.vr_guard_hand)
    {
        self.vr_guard_hand = guard;
        self.vr_guard_time = 0;
    }
    if(guard >= 0 && VR_Bash_GuardStill(guard))
    {
        self.vr_guard_time = time;
    }

    float mainDriving = VR_Bash_Driving(cVR_MainHand, fwd);
    float offDriving = VR_Bash_Driving(cVR_OffHand, fwd);
    if(!mainDriving && !offDriving)
    {
        return;
    }

    float armed = FALSE;
    float hand = -1;
    if(guard >= 0 && self.vr_guard_time > 0 && time - self.vr_guard_time < VR_BASH_WINDOW)
    {
        if(guard == VR_GUARD_HANDS)
        {
            if(mainDriving && offDriving)
            {
                hand = cVR_MainHand;
            }
        }
        else if(guard == cVR_MainHand ? mainDriving : offDriving)
        {
            hand = guard;
            armed = TRUE;
        }
    }
    float oneHand = FALSE;
    if(hand < 0)
    {
        if(mainDriving && VR_Bash_PalmShove(cVR_MainHand, fwd))
        {
            hand = cVR_MainHand;
        }
        else if(offDriving && VR_Bash_PalmShove(cVR_OffHand, fwd))
        {
            hand = cVR_OffHand;
        }
        else
        {
            return;
        }
        oneHand = TRUE;
    }

    // No punches or swings in this motion, even during the bash's cooldown: the hands' melee counts
    // a swing as already landed until the hand stops or pulls back (PlayerVRMeleeImpl). A weapon
    // bash or a one-handed shove keeps the other hand free, unless it drives forward right by the
    // weapon (held in both hands).
    VRSetMeleeHitSoundPlayed(hand, TRUE);
    float other = hand == cVR_MainHand ? cVR_OffHand : cVR_MainHand;
    if((!armed && !oneHand) || (armed && (hand == cVR_MainHand ? offDriving : mainDriving) &&
                              vlen(VRGetHandPos(other) - VRGetHandPos(hand)) < 24))
    {
        VRSetMeleeHitSoundPlayed(other, TRUE);
    }
    if(time < self.vr_bash_time)
    {
        return;
    }

    // What is just ahead of the hands.
    vector at = (VRGetHandPos(cVR_MainHand) + VRGetHandPos(cVR_OffHand)) * 0.5;
    if(armed || oneHand)
    {
        at = VRGetHandPos(hand);
    }
    float hits = VR_Bash_Hit(at, fwd, armed ? QVR_HITKIND_BASH : QVR_HITKIND_SHOVE, (armed || oneHand) ? hand : -1,
                             oneHand ? 0.5 : 1);
    if(!hits)
    {
        return; // a push at the air: nothing, no cooldown
    }
    self.vr_bash_time = time + 0.8;
    sound(self, CHAN_AUTO, armed ? "player/axhit2.wav" : "fisthit.wav", 1, ATTN_NORM);
    sound(self, CHAN_AUTO, "demon/dland2.wav", oneHand ? 0.45 : 0.7, ATTN_NORM);
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
    dprint("bash: "); dprint(ftos(hits)); dprint(armed ? " (weapon)\n" : (oneHand ? " (one hand)\n" : " (hands)\n"));
};
