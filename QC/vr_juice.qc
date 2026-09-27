// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat).
// The melee itself (blows, the parry stance, bashes and shoves) is in vr_melee.qc. Hit reactions (vr_hit_push),
// explosion rumble (vr_explosion_rumble) and directional damage haptics are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat


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

// Batting projectiles back (vr_deflect; docs/vr-port/ROUND15.md). A batting swing is a motion whose striking part
// (the far end of what the hand holds: a sword's tip, a gun's muzzle, the knuckles) goes vr_deflect_speed times
// vr_melee_speed or faster: less than a blow. Each frame of it, the held thing's line (its near end to its far end:
// the pommel to the tip, the fist to the knuckles; vr_melee.qc) is kept, for vr_deflect_window seconds. An enemy
// projectile whose path this frame (from a little behind it, VR_BAT_LATE, to where it will be at the frame's end)
// comes within vr_deflect_radius (plus its size) of any of those lines is batted back: so a swing a little early
// still bats what flies into where the weapon went, and the frame rate doesn't matter (the paths are swept). It
// flies back where the hand points (at its thrower, when that is within VR_BAT_AIM degrees of it), faster, and now
// hurts monsters.
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

    if(b.mh_fresh && b.mh_thing != VR_MTHING_NONE) // (a new pose: the line moved)
    {
        float need = VR_Melee_MinSpeed() * max(0, cvar_hget(cvarh_vr_deflect_speed));
        if(vlen(b.mh_vfar) >= need)
        {
            b.bat_i = b.bat_i + 1;
            if(b.bat_i >= VR_BAT_KEEP)
            {
                b.bat_i = 0;
            }
            b.bat_a[b.bat_i] = b.mh_near;
            b.bat_b[b.bat_i] = b.mh_far;
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
