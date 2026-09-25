// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat);
// - bashing (vr_bash): driving a guard forward (a weapon held across in front, as for a parry) or
//   shoving with both empty hands knocks monsters back and staggers them.
// Hit reactions (vr_hit_push), explosion rumble (vr_explosion_rumble) and directional damage haptics
// are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat
.float vr_bash_time;      // on a player: when the next bash may be

// ----------------------------------------------------------------------------
// Blows: what makes a hand's motion a melee blow (PlayerVRMeleeImpl in client.qc), and how hard.
//
// Measured on the wrist, not the controller: a flick of the wrist swings the controller fast (it
// sits well ahead of the wrist) while the wrist hardly moves. The wrist is taken relative to the
// head and in the play space (the turning undone), so walking, turning and the body sliding under
// the head are not the hand's motion. A stroke is the wrist moving on (faster than
// VR_BLOW_MOVING) along a steady line: it ends when the hand slows down or turns away (a wiggle
// is many short strokes). A blow is a stroke that has travelled vr_melee_distance with the wrist
// reaching vr_melee_speed, still moving hard.
//
// How hard: the stroke's fastest over vr_melee_speed (up to three times). A hand's blows are also
// weighted by their shape, as the same effort moves the hand about twice as fast in an arc (a slap,
// an overhead blow: the shoulder swings the whole arm) as straight out (a punch). A straight stroke
// is weighted by its direction from the head's facing: ahead vr_melee_punch_mult, sideways
// VR_BLOW_SLAP, downwards VR_BLOW_DOWN, upwards (an uppercut) VR_BLOW_UP, back VR_BLOW_BACK,
// blended by how much of each it is. An arc (the wrist's path turning VR_BLOW_ARC radians per metre
// and more) is a slap (VR_BLOW_SLAP) or, going down, an overhead blow (VR_BLOW_OVERHEAD), whatever
// way it goes when it lands (an overhead blow lands going forward). Weapons swing by the
// controller's fastest instead (their reach adds to it), unweighted.

float VR_BLOW_MOVING = 0.7; // m/s: slower, the wrist is not in a stroke
float VR_BLOW_SLAP = 0.6;
float VR_BLOW_DOWN = 0.85;
float VR_BLOW_UP = 1.0;
float VR_BLOW_BACK = 0.5;
float VR_BLOW_OVERHEAD = 0.7;
float VR_BLOW_ARC = 1.8;    // rad/m: an arc of about 55 cm around the shoulder (a straight punch: 0)
float VR_BLOW_REARM = 0.2;  // s after a blow lands before the next

// A hand's stroke, on an entity of its own (the player's vr_blow_main/vr_blow_off).
.entity vr_blow_main;
.entity vr_blow_off;
.vector blow_wrist;    // the wrist a frame ago (metres, from the head, in the play space)
.vector blow_vel;      // its velocity (m/s)
.float blow_speed;
.float blow_time;      // when it was measured
.vector blow_start;    // where the stroke began
.float blow_t0;        // when (0: no stroke)
.float blow_peak;      // the stroke's fastest: the wrist's,
.float blow_tippeak;   // and the controller's (m/s)
.float blow_turn;      // how far the wrist's path has turned (radians),
.float blow_path;      // over how long a path (metres)
.vector blow_dir;      // the direction of the last blow that landed (play space)
.float blow_rearm;     // no blow before this
.float blow_arc;       // the blow being dealt: how much of an arc it is (0 .. 1),
.float blow_strength;  // its damage multiplier,
.float blow_kind;      // what it is (VR_BLOW_KIND_*),
.float blow_dist;      // and how far the wrist travelled (metres)

float VR_BLOW_KIND_PUNCH = 0;
float VR_BLOW_KIND_SLAP = 1;
float VR_BLOW_KIND_DOWN = 2;
float VR_BLOW_KIND_UP = 3;
float VR_BLOW_KIND_BACK = 4;

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

// Follows the stroke of `xHand`, once a frame (PlayerVRMelee, before the bashes and the blows).
void(float xHand) VR_Blow_Update =
{
    entity b = VR_Blow(xHand);
    float dt = time - b.blow_time;
    vector w = VR_Blow_Wrist(xHand);
    vector d = w - b.blow_wrist;
    b.blow_time = time;
    b.blow_wrist = w;

    // A gap (the hands lost), or a jump (a teleport, the play space re-based): start over.
    if(!VRHandsTracked(self) || dt <= 0 || dt > 0.1 || vlen(d) > 0.3)
    {
        b.blow_t0 = 0;
        b.blow_speed = 0;
        b.blow_vel = '0 0 0';
        return;
    }

    vector oldVel = b.blow_vel;
    float oldSpeed = b.blow_speed;
    b.blow_vel = d * (1 / dt);
    b.blow_speed = vlen(b.blow_vel);

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
        b.blow_t0 = time - dt;
        b.blow_start = w - d;
        b.blow_peak = 0;
        b.blow_tippeak = 0;
        b.blow_turn = 0;
        b.blow_path = 0;
    }
    else if(oldSpeed > 1 && b.blow_speed > 1)
    {
        // The path's turn since the last frame (fast enough for its direction to be sure).
        b.blow_turn = b.blow_turn + VRAngleBetween(oldVel * (1 / oldSpeed), b.blow_vel * (1 / b.blow_speed)) *
                                    (3.14159265 / 180);
        b.blow_path = b.blow_path + vlen(d);
    }
    b.blow_peak = max(b.blow_peak, b.blow_speed);
    b.blow_tippeak = max(b.blow_tippeak, VRGetHandVelMag(xHand));
};

// How far the stroke of `xHand` has taken the wrist (metres; 0 when not in one).
float(float xHand) VR_Blow_Distance =
{
    entity b = VR_Blow(xHand);
    return b.blow_t0 ? vlen(b.blow_wrist - b.blow_start) : 0;
};

// Whether `xHand` is in a blow: far enough, fast enough, and still moving hard.
float(float xHand) VR_Blow_IsBlow =
{
    entity b = VR_Blow(xHand);
    float speed = max(0.5, cvar_hget(cvarh_vr_melee_speed));
    return b.blow_t0 && b.blow_peak >= speed && b.blow_speed >= speed * 0.5 &&
           VR_Blow_Distance(xHand) >= cvar_hget(cvarh_vr_melee_distance);
};

// The stroke's weight by its shape and direction, from the head's facing (see above); sets its
// blow_kind and blow_arc.
float(entity b) VR_Blow_Weight =
{
    vector dir = normalize(b.blow_wrist - b.blow_start);
    vector fwd = VRForwardOf('0 1 0' * (self.v_viewangle_y - self.vryaw)); // flat
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
    if(b.blow_arc <= 0)
    {
        return straight;
    }
    // An overhead blow: an arc upright (its plane: across the stroke's line and the way the wrist
    // goes now), not going up (it lands going forward, or down).
    vector n = b.blow_wrist - b.blow_start;
    vector v = b.blow_vel;
    vector plane;
    plane_x = n_y * v_z - n_z * v_y;
    plane_y = n_z * v_x - n_x * v_z;
    plane_z = n_x * v_y - n_y * v_x;
    float down = 0;
    if(vlen(plane) > 0.0001 && b.blow_speed > 0)
    {
        plane = normalize(plane);
        down = (1 - fabs(plane_z)) * min(1, max(0, 1 - v_z / b.blow_speed * 4));
    }
    if(b.blow_arc >= 0.5)
    {
        b.blow_kind = down >= 0.5 ? VR_BLOW_KIND_DOWN : VR_BLOW_KIND_SLAP;
    }
    float arc = VR_BLOW_SLAP + (VR_BLOW_OVERHEAD - VR_BLOW_SLAP) * down;
    return straight + (arc - straight) * b.blow_arc;
};

// Measures the blow `xHand` is dealing (a fist when `xFist`): sets blow_strength, blow_kind and
// blow_dist.
void(float xHand, float xFist) VR_Blow_Measure =
{
    entity b = VR_Blow(xHand);
    float threshold = max(0.5, cvar_hget(cvarh_vr_melee_speed));
    b.blow_dist = VR_Blow_Distance(xHand);
    if(xFist)
    {
        b.blow_strength = min(3, b.blow_peak / threshold) * VR_Blow_Weight(b);
    }
    else
    {
        VR_Blow_Weight(b); // the kind, for the training dummy
        b.blow_strength = min(3, max(b.blow_peak, b.blow_tippeak) / threshold);
    }
};

// The damage multiplier of the blow `xHand` is dealing (VRMeleeDmg).
float(float xHand) VR_Blow_Strength =
{
    return VR_Blow(xHand).blow_strength;
};

string(float xKind) VR_Blow_KindName =
{
    if(xKind == VR_BLOW_KIND_SLAP) return "slap";
    if(xKind == VR_BLOW_KIND_DOWN) return "overhead blow";
    if(xKind == VR_BLOW_KIND_UP)   return "uppercut";
    if(xKind == VR_BLOW_KIND_BACK) return "backhand";
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

// Whether `xHand` of `self` drives forward (along the body's facing `xForward`) faster than
// vr_bash_speed (m/s).
float(float xHand, vector xForward) VR_Bash_Driving =
{
    return VRGetEntHandVel(self, xHand) * xForward > cvar_hget(cvarh_vr_bash_speed);
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
    float push = cvar_hget(cvarh_vr_bash_push) * xScale;
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

// A bash (vr_bash): a guard driven forward hard -- a weapon held across in front (the parry pose),
// or both empty hands together (crossed or side by side: a shove), or one empty hand with the open
// palm ahead (a shove half as strong). What is just ahead takes a little damage (vr_bash_damage),
// is thrown back (vr_bash_push) and staggers (its pain animation, forced). The motion is only a
// bash: the hands in it don't also punch or swing (runs before the hands' melee, PlayerVRMelee in
// client.qc; they may hit again once they stop or pull back).
void() VR_Bash =
{
    if(!cvar_hget(cvarh_vr_bash) || !self.ishuman || self.health <= 0 || !VRHandsTracked(self))
    {
        return;
    }

    // The body's facing is the head's (.v_angle is the main hand's aim: a weapon held across in
    // front aims sideways).
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);

    // (Most frames no hand drives forward: that cheap test comes first.)
    float mainDriving = VR_Bash_Driving(cVR_MainHand, fwd);
    float offDriving = VR_Bash_Driving(cVR_OffHand, fwd);
    if(!mainDriving && !offDriving)
    {
        return;
    }

    float armed = FALSE;
    float hand = -1;
    if(mainDriving && VR_Parry_Blocks(self, cVR_MainHand, fwd))
    {
        hand = cVR_MainHand;
        armed = TRUE;
    }
    else if(offDriving && VR_Parry_Blocks(self, cVR_OffHand, fwd))
    {
        hand = cVR_OffHand;
        armed = TRUE;
    }
    else if(mainDriving && offDriving && VRIsHandEmpty(self, cVR_MainHand) && VRIsHandEmpty(self, cVR_OffHand))
    {
        // Both hands together, in front at chest height.
        vector m = VRGetHandPos(cVR_MainHand) - self.origin;
        vector o = VRGetHandPos(cVR_OffHand) - self.origin;
        if(vlen(m - o) < 30 && m * fwd > 2 && o * fwd > 2 && m_z > -4 && o_z > -4 && m_z < 50 && o_z < 50)
        {
            hand = cVR_MainHand;
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
