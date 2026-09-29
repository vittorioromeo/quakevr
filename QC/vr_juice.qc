// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat).
// The melee itself (blows, the parry stance, bashes and shoves) is in vr_melee.qc. Hit reactions (vr_hit_push),
// explosion rumble (vr_explosion_rumble) and directional damage haptics are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat


// VR_SegmentDistance's closest points: how far along the first segment (0 its start, 1 its end), and the second.
float vr_seg_s;
float vr_seg_t;

// The least distance between the segments `xA0` .. `xA1` and `xB0` .. `xB1` (their closest points: vr_seg_s, vr_seg_t).
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
    vr_seg_s = 0;
    vr_seg_t = 0;
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
    vr_seg_s = s;
    vr_seg_t = t;
    return vlen((xA0 + d1 * s) - (xB0 + d2 * t));
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

// Batting projectiles back (vr_deflect; docs/vr-port/ROUND15.md, ROUND21.md "Deflection by blows and bashes; catching
// grenades; ogre aim"). Only an armed hand bats: a weapon or a club (VR_Melee_HoldsWeapon; an empty hand never does
// since ROUND21.md "Debug menu; quad sound; grenade catch default; no empty-hand deflection": a projectile flies past
// it or hits you, a grenade is caught). A batting swing is a motion whose striking part (the far end of what the hand holds:
// a sword's tip, a gun's muzzle, a club's head) goes vr_deflect_speed times vr_melee_speed or faster: less than a
// blow, and every blow. A hand in a bash's push doesn't swing: its bash bats (VR_Bash_Deflect). Each frame of it, the
// held thing's line (its near end to its far end: the pommel to the tip, the grip to the muzzle; vr_melee.qc) is kept
// with both ends' velocities, for vr_deflect_window seconds. An enemy projectile whose path this frame (from a little behind it, VR_BAT_LATE, to where it will be at the frame's end) comes within
// vr_deflect_radius (plus its size) of any of those lines is batted: so a swing a little early still bats what flies
// into where the weapon went, and the frame rate doesn't matter (the paths are swept).
//
// As a ball off a bat (VR_Deflect_Bat): the weapon's point nearest the projectile's path moves at its ends' velocities
// mixed along the line (the line is rigid); the projectile, seen from that point, comes off the face that meets it
// (across the line, against its coming, tilted by a glancing touch) with vr_deflect_bounce of the speed it met it at,
// keeping most of its speed along the face. So a swing towards the thrower sends it back at him, a swing across sends
// it off to the side, the harder the faster (VR_BAT_MIN_SPEED .. VR_BAT_MAX_SPEED). Then the aim assist
// (VR_Deflect_Assist). A bash sends it the way the hands push instead (VR_Bash_Deflect, vr_melee.qc).
float VR_BAT_KEEP = 8;            // weapon lines kept (a ring)
float VR_BAT_LATE = 0.05;         // s: a projectile's path starts this far behind it (just past the weapon)
float VR_BAT_GLANCE = 0.5;        // a touch off the line's middle tilts the face this much towards it (at the reach's edge)
float VR_BAT_GRIP = 0.2;          // share of its speed along the face the weapon takes (the rest is kept: a slice)
float VR_BAT_MIN_SPEED = 450;     // u/s a batted projectile flies at least (a tap still sends it off),
float VR_BAT_MAX_SPEED = 1500;    // and at most
float VR_BAT_PAID_KEEP = 0.4;     // s a batting swing's stamina covers the rest of the swing (a blow landing, more batting)
float VR_BAT_ASSIST_THROWER = 12; // degrees: the aim assist's cone round the projectile's thrower,
float VR_BAT_ASSIST_CONE = 6;     // and round any other monster ("only when very close")
float VR_BAT_FIST = 1;            // an empty hand's punch speed, times vr_melee_speed (a blow's least)

// The speed (m/s) the far end of what `b`'s hand holds must reach to bat: vr_deflect_speed times vr_melee_speed. An
// empty hand (a fist, an open hand, a box in the fist) never bats; for it this is a punch's speed, over which it
// doesn't catch a grenade either (VR_Grenade_HandCatches: a punch isn't a catch).
float(entity b) VR_Deflect_Need =
{
    float share = max(0, cvar_hget(cvarh_vr_deflect_speed));
    if(b.mh_thing == VR_MTHING_FIST)
    {
        share = max(share, VR_BAT_FIST);
    }
    return VR_Melee_MinSpeed() * share;
};

// The gravity (u/s^2) a projectile `e` falls by (a grenade, a zombie's flesh), else 0.
float(entity e) VR_Deflect_Gravity =
{
    if(e.movetype != MOVETYPE_BOUNCE && e.movetype != MOVETYPE_TOSS)
    {
        return 0;
    }
    float g = cvar_hget(cvarh_sv_gravity);
    if(e.gravity)
    {
        g = g * e.gravity;
    }
    return g;
};

// The aim assist (vr_deflect_aim_assist, 0..1): a batted or bashed projectile `e` about to fly along `xDir` at `xSpeed`
// that goes within VR_BAT_ASSIST_CONE degrees of a live monster in sight (VR_BAT_ASSIST_THROWER of its thrower
// `xThrower`) bends that share of the way towards it (a falling one towards where its arc meets it). The way it goes.
vector(entity e, vector xDir, float xSpeed, entity xThrower) VR_Deflect_Assist =
{
    float share = min(1, max(0, cvar_hget(cvarh_vr_deflect_aim_assist)));
    if(share <= 0)
    {
        return xDir;
    }
    float g = VR_Deflect_Gravity(e);
    entity best = world;
    vector bestDir = xDir;
    float bestScore = 2;
    // (Not findradius: the callers walk a findradius chain.)
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if(!(m.flags & FL_MONSTER) || m.health <= 0 || !m.takedamage)
        {
            continue;
        }
        vector mid = m.origin + (m.mins + m.maxs) * 0.5;
        float dist = vlen(mid - e.origin);
        if(dist < 24 || dist > 2500)
        {
            continue;
        }
        vector at = mid;
        if(g > 0)
        {
            float tof = dist / max(1, xSpeed);
            at_z = at_z + 0.5 * g * tof * tof; // (aimed above it: the arc falls onto it)
        }
        vector to = normalize(at - e.origin);
        float cone = VR_BAT_ASSIST_CONE;
        if(m == xThrower)
        {
            cone = VR_BAT_ASSIST_THROWER;
        }
        float off = VRAngleBetween(xDir, to);
        if(off > cone || off / cone >= bestScore)
        {
            continue;
        }
        traceline(e.origin, mid, TRUE, e);
        if(trace_fraction < 1)
        {
            continue; // (a wall between)
        }
        best = m;
        bestDir = to;
        bestScore = off / cone;
    }
    if(best == world)
    {
        return xDir;
    }
    vector d = normalize(xDir * (1 - share) + bestDir * share);
    dprint(sprintf("deflect: aim assist towards %s, %.1f deg off, %.1f after\n", best.classname,
        VRAngleBetween(xDir, bestDir), VRAngleBetween(d, bestDir)));
    return d;
};

// Sends the projectile `e` off at `xVel` (the aim assist may bend it); `xHow` ("swing" or "bash") for the motion
// recorder; `xHand` feels it (-1: both hands). It is the player's now: it hurts monsters and spares the player.
void(entity e, vector xVel, float xHand, string xHow) VR_Deflect_Send =
{
    entity oldOwner = e.owner;
    vector vin = e.velocity;
    float speed = vlen(xVel);
    vector dir = VR_Deflect_Assist(e, normalize(xVel), speed, oldOwner);

    e.velocity = dir * speed;
    e.angles = vectoangles(e.velocity);
    e.owner = self;
    e.enemy = oldOwner; // homing ones (a vore's ball) go for their thrower
    e.vr_deflect_time = time;
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    VR_Grenade_Return(e, sprintf("batted back by a %s", xHow)); // (an ogre's: it goes off as your launcher's)

    string what = e.classname;
    if(what == "")
    {
        what = e.model; // (a vore's ball has no name)
    }
    dprint(sprintf("deflect: %s by a %s, in %v (%.0f u/s), out %v (%.0f u/s)\n", what, xHow, vin, vlen(vin),
        e.velocity, speed));
    sound(self, CHAN_AUTO, "weapons/tink1.wav", 1, ATTN_NORM);
    sound(self, CHAN_AUTO, "player/axhit2.wav", 0.6, ATTN_NORM);
    particle2(e.origin, dir * 120, QVR_PARTICLE_PRESET_SPARKS, 20);
    VR_Motion_Event("deflect", what, xHand, 0, e.origin, oldOwner, xHow);
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

// A batting swing's stamina (vr_strike_stamina): paid as a blow of that hand is (VR_Stamina_Strike's costs), once a
// swing: not again for more batting or a blow landing in the next VR_BAT_PAID_KEEP seconds (VR_Melee_Strike reads
// bat_paid_time), nor at all if a blow of this motion has paid. The speed multiplier it leaves (1: paid in full).
float(float xHand, entity b) VR_Deflect_Pay =
{
    if(b.mh_stam_paid && VRGetMeleeHitSoundPlayed(xHand))
    {
        if(b.mh_exhaust > 0)
        {
            return b.mh_exhaust;
        }
        return 1;
    }
    if(time - b.bat_paid_time >= VR_BAT_PAID_KEEP)
    {
        b.mh_stam_paid = FALSE;
        VR_Stamina_Strike(xHand, b);
        b.bat_paid_time = time;
        b.bat_exhaust = b.mh_exhaust;
    }
    if(b.bat_exhaust > 0)
    {
        return b.bat_exhaust;
    }
    return 1;
};

// Bats `e` off the weapon line `xA` .. `xB` (its ends moving at `xVA`, `xVB`; world, u/s) where its path (`xFrom` ..
// `xTo`) passed nearest: `xS` along the path, `xT` along the line, within `xReach` (VR_Deflect).
void(entity e, vector xA, vector xB, vector xVA, vector xVB, vector xFrom, vector xTo, float xS, float xT,
     float xReach, float xHand, entity b) VR_Deflect_Bat =
{
    vector c = xA + (xB - xA) * xT;        // the weapon's point,
    vector vbat = xVA + (xVB - xVA) * xT;  // its velocity,
    vector p = xFrom + (xTo - xFrom) * xS; // and the projectile's
    // (A fist, an open hand: a ball. Its line, 5 units along the hand, is no blade: taken as one, a punch sent it on
    // past the fist.)
    vector axis = '0 0 0';
    if(vlen(xB - xA) > 0.5 && b.mh_thing != VR_MTHING_FIST)
    {
        axis = normalize(xB - xA);
    }

    // The face it meets: across the line, against its coming (seen from the weapon), tilted towards where it passed.
    vector w = e.velocity - vbat;
    vector across = w - axis * (w * axis);
    vector n = normalize(across) * -1;
    if(vlen(across) < 1)
    {
        n = normalize(vbat - axis * (vbat * axis)); // (it rides along the line: the swing's way)
    }
    vector off = p - c;
    off = off - axis * (off * axis);
    if(vlen(off) > 0.5 && off * n > 0)
    {
        n = normalize(n + normalize(off) * VR_BAT_GLANCE * min(1, vlen(off) / max(1, xReach)));
    }

    // Off it: the bounce of the speed it met the face at, most of the speed along it kept.
    float into = w * n;
    vector along = w - n * into;
    vector v = vbat + n * (fabs(into) * min(1, max(0, cvar_hget(cvarh_vr_deflect_bounce)))) + along * (1 - VR_BAT_GRIP);
    float speed = vlen(v);
    if(speed < 1)
    {
        v = n;
        speed = 1;
    }
    float want = min(VR_BAT_MAX_SPEED, max(VR_BAT_MIN_SPEED, speed));
    want = want * VR_Deflect_Pay(xHand, b); // (short of stamina: weaker, as a blow is)
    dprint(sprintf("deflect: the weapon at %v along %v, moving %v (%.0f u/s)\n", c, axis, vbat, vlen(vbat)));
    dprint(sprintf("deflect: its face %v; off it at %.0f u/s, sent at %.0f\n", n, speed, want));
    VR_Deflect_Send(e, v * (want / speed), xHand, "swing");
};

void(float xHand, entity b) VR_Deflect = // (b: VR_Blow(xHand))
{
    if(!cvar_hget(cvarh_vr_deflect))
    {
        return;
    }

    // A hand in a bash's push (or its batting) bats with the bash: the way the hands push, off the weapon's face
    // (VR_Bash_Deflect), not as a swing of its weapon line.
    if(b.mh_pushing || (time <= self.vr_bashdef_until && (self.vr_bashdef_hand == xHand || self.vr_bashdef_hand < 0)))
    {
        if(b.bat_newest > 0)
        {
            b.bat_newest = 0;
            for(float c = 0; c < VR_BAT_KEEP; ++c)
            {
                b.bat_t[c] = 0; // (nor its lines from before the push)
            }
        }
        return;
    }

    if(!VR_Melee_HoldsWeapon(b.mh_thing))
    {
        return; // an empty hand bats nothing (nor its lines from before it let go of its weapon)
    }

    if(b.mh_fresh) // (a new pose: the line moved)
    {
        if(vlen(b.mh_vfar) >= VR_Deflect_Need(b))
        {
            b.bat_i = b.bat_i + 1;
            if(b.bat_i >= VR_BAT_KEEP)
            {
                b.bat_i = 0;
            }
            // (The ends' velocities are the play space's, from the head, in m/s: turned to the world's, with the body's.)
            float m2u = VRMetersToUnits();
            b.bat_a[b.bat_i] = b.mh_near;
            b.bat_b[b.bat_i] = b.mh_far;
            b.bat_va[b.bat_i] = VR_Blow_ToWorld(b.mh_vnear) * m2u + self.velocity;
            b.bat_vb[b.bat_i] = VR_Blow_ToWorld(b.mh_vfar) * m2u + self.velocity;
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
            float nearK = 0;
            float nearS = 0;
            float nearT = 0;
            for(float k = 0; k < VR_BAT_KEEP; ++k)
            {
                if(b.bat_t[k] > 0 && time - b.bat_t[k] <= window)
                {
                    float d = VR_SegmentDistance(from, to, b.bat_a[k], b.bat_b[k]);
                    if(d < near)
                    {
                        near = d;
                        when = time - b.bat_t[k];
                        nearK = k;
                        nearS = vr_seg_s;
                        nearT = vr_seg_t;
                    }
                }
            }
            if(near < radius + e.maxs_x)
            {
                dprint(sprintf("deflect: %s of %s, %.0f units from the swing's line of %.2f s ago\n", e.classname,
                    e.owner.classname, near, when));
                VR_Deflect_Bat(e, b.bat_a[nearK], b.bat_b[nearK], b.bat_va[nearK], b.bat_vb[nearK], from, to, nearS,
                    nearT, radius + e.maxs_x, xHand, b);
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
