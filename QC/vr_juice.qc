// vr_juice.qc -- feedback that makes fights physical (docs/vr-port/ROUND7.md, item 13):
// - batting enemy projectiles back with a melee swing (vr_deflect);
// - a heartbeat in the hands when your health is low (vr_heartbeat).
// Hit reactions (vr_hit_push), explosion rumble (vr_explosion_rumble) and directional damage haptics
// are in combat.qc.

.float vr_deflect_time;   // on a projectile: when it was last batted back
.float vr_heartbeat_time; // on a player: the next heartbeat

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
                vector oldForward = v_forward;
                vector oldRight = v_right;
                vector oldUp = v_up;
                makevectors(VRGetHandRot(xHand));
                vector dir = v_forward;
                v_forward = oldForward;
                v_right = oldRight;
                v_up = oldUp;

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
