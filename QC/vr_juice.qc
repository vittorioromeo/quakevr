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

// A bash (vr_bash): a guard driven forward hard -- a weapon held across in front (the parry pose),
// or both empty hands together (crossed or side by side: a shove). What is just ahead takes a little
// damage (vr_bash_damage), is thrown back (vr_bash_push) and staggers (its pain animation, forced).
// The motion is only a bash: the hands in it don't also punch or swing (runs before the hands'
// melee, PlayerVRMelee in client.qc; they may hit again once they slow down).
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
    if(hand < 0)
    {
        return;
    }

    // No punches or swings in this motion, even during the bash's cooldown: the hands' melee counts
    // a swing as already landed until the hand slows down (PlayerVRMeleeImpl). A weapon bash keeps
    // the other hand free, unless it drives forward right by the weapon (held in both hands).
    VRSetMeleeHitSoundPlayed(hand, TRUE);
    float other = hand == cVR_MainHand ? cVR_OffHand : cVR_MainHand;
    if(!armed || ((hand == cVR_MainHand ? offDriving : mainDriving) &&
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
    if(armed)
    {
        at = VRGetHandPos(hand);
    }
    float hits = 0;
    float push = cvar_hget(cvarh_vr_bash_push);
    entity e = findradius(at + fwd * 24, 48);
    while(e)
    {
        if(e != self && e.takedamage && e.health > 0 && (e.flags & FL_MONSTER) &&
           normalize(e.origin - self.origin) * fwd > 0.3)
        {
            hits = hits + 1;
            VR_SetHitKind(armed ? QVR_HITKIND_BASH : QVR_HITKIND_SHOVE, armed ? hand : -1, at + fwd * 12, fwd);
            T_Damage(e, self, self, cvar_hget(cvarh_vr_bash_damage));
            VR_ClearHitKind();
            VR_Push(e, fwd, 520 * push, 140 * push);
            if(e.health > 0 && e.th_pain)
            {
                // Staggered: its pain, whatever its own chance of flinching.
                entity oldSelf = self;
                self = e;
                e.pain_finished = 0;
                e.th_pain(oldSelf, cvar_hget(cvarh_vr_bash_damage));
                e.attack_finished = time + 1;
                self = oldSelf;
            }
            SpawnBloodSplash(e.origin + '0 0 16', 4);
        }
        e = e.chain;
    }

    if(!hits)
    {
        return; // a push at the air: nothing, no cooldown
    }
    self.vr_bash_time = time + 0.8;
    sound(self, CHAN_AUTO, armed ? "player/axhit2.wav" : "fisthit.wav", 1, ATTN_NORM);
    sound(self, CHAN_AUTO, "demon/dland2.wav", 0.7, ATTN_NORM);
    haptic(cVR_MainHand, 0.0, 0.25, 70, 1.0);
    haptic(cVR_OffHand, 0.0, 0.25, 70, 1.0);
    handimpact(cVR_MainHand, 3, fwd * -1);
    handimpact(cVR_OffHand, 3, fwd * -1);
    particle2(at + fwd * 12, fwd * 80, QVR_PARTICLE_PRESET_BULLETPUFF, 16);
    dprint("bash: "); dprint(ftos(hits)); dprint(armed ? " (weapon)\n" : " (hands)\n");
};
