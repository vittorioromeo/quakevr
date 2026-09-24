// vr_wpnforcegrab.qc -- force grab, as Half-Life: Alyx's gravity gloves.
//
// Point an empty, open hand at an object: a dropped or thrown weapon, a backpack, an ammo or
// health box, or (in single player) a weapon lying in the level. The one nearest to where the
// hand points, within vr_forcegrab_cone degrees and vr_forcegrab_distance units and in sight,
// sparkles. Pull the trigger to lock on to it and flick the hand back or up
// (vr_forcegrab_flick_speed, vr_forcegrab_flick_turn): the object flies to the hand in an arc,
// homing on it, and gets there in a set time. Close the hand (the grip) as it arrives to catch it:
// the grip pressed at most vr_forcegrab_catch_early seconds before it arrives, or up to
// vr_forcegrab_catch_late after, while it is still within reach. Missed, it drops at the
// player's feet.
//
// In flight the object does not collide (MOVETYPE_NOCLIP, SOLID_NOT), so it cannot snag on a
// corner or a ledge. Missed, it is put back where it fits (its box traced from the player's chest
// to the hand) and falls as before, not touchable for a moment: else the hand it missed would
// pick it up anyway.

float forcegrabCatching; // TRUE while a caught object's handtouch runs (wpnthrow_handtouch_ent_hand)

// ----------------------------------------------------------------------------
// Per-hand state, on the player
// ----------------------------------------------------------------------------

entity(float xHand) VR_Forcegrab_GetTarget =
{
    if(xHand == cVR_OffHand) return self.offhand_fgtarget;
    return self.mainhand_fgtarget;
}

float(float xHand) VR_Forcegrab_GetLocked =
{
    if(xHand == cVR_OffHand) return self.offhand_fglocked;
    return self.mainhand_fglocked;
}

void(float xHand, entity xTarget, float xLocked) VR_Forcegrab_Set =
{
    if(xHand == cVR_OffHand)
    {
        self.offhand_fgtarget = xTarget;
        self.offhand_fglocked = xLocked;
    }
    else
    {
        self.mainhand_fgtarget = xTarget;
        self.mainhand_fglocked = xLocked;
    }
}

// Whether `xHand` of `xPlayer` is locked on to an object (the trigger then does not attack).
float(entity xPlayer, float xHand) VR_Forcegrab_IsHandBusy =
{
    if(xHand == cVR_OffHand) return xPlayer.offhand_fglocked;
    return xPlayer.mainhand_fglocked;
}

void(entity xPlayer) VR_Forcegrab_Reset =
{
    xPlayer.offhand_fgtarget = world;
    xPlayer.offhand_fglocked = FALSE;
    xPlayer.mainhand_fgtarget = world;
    xPlayer.mainhand_fglocked = FALSE;
}

// ----------------------------------------------------------------------------
// Targets
// ----------------------------------------------------------------------------

vector(entity e) VR_Forcegrab_Centre =
{
    return e.origin + (e.mins + e.maxs) * 0.5;
}

float(entity e) VR_Forcegrab_IsEligible =
{
    if(e == world || e.fg_state != 0 || e.model == string_null || e.solid == SOLID_NOT)
    {
        return FALSE;
    }

    if(e.classname == "thrown_weapon" || (e.flags & FL_FORCEGRABBABLE))
    {
        // Not while it still flies from a throw.
        return (e.flags & FL_ONGROUND) || vlen(e.velocity) < 300;
    }

    // Weapons lying in the level, in single player (in coop and deathmatch they stay for all).
    if(!deathmatch && !coop && e.handtouch == weapon_touch)
    {
        return TRUE;
    }

    return FALSE;
}

// The eligible object nearest to where the hand points, or world.
entity(float xHand) VR_Forcegrab_FindTarget =
{
    vector handPos = VRGetHandPos(xHand);
    makeforward(VRGetHandRot(xHand));
    vector aim = v_forward;

    float range = cvar_hget(cvarh_vr_forcegrab_distance);
    float minCos = cos(cvar_hget(cvarh_vr_forcegrab_cone) * 0.0174533);

    entity best = world;
    float bestScore = 0;

    for(entity e = findradius(handPos, range); e; e = e.chain)
    {
        if(!VR_Forcegrab_IsEligible(e))
        {
            continue;
        }

        vector c = VR_Forcegrab_Centre(e);
        float dist = vlen(c - handPos);
        if(dist < 1)
        {
            continue;
        }

        float cosAngle = ((c - handPos) * aim) / dist;
        if(cosAngle < minCos)
        {
            continue;
        }

        // In sight (level geometry only).
        traceline(handPos, c, TRUE, self);
        if(vlen(trace_endpos - c) > 8)
        {
            continue;
        }

        // Nearest to the pointing direction first; nearer ones win close calls.
        float score = cosAngle - dist * 0.0002;
        if(best == world || score > bestScore)
        {
            best = e;
            bestScore = score;
        }
    }

    return best;
}

// A flick of the hand back towards the player or up: fast enough, or the fingers swinging that
// way fast enough.
float(float xHand, entity xTarget) VR_Forcegrab_Flicked =
{
    vector back = VRGetHandPos(xHand) - VR_Forcegrab_Centre(xTarget);
    back_z = 0;
    vector pull = normalize(normalize(back) + '0 0 1');

    // Metres per second, and radians per second (world axes), in the play space.
    if(VRGetHandVel(xHand) * pull >= cvar_hget(cvarh_vr_forcegrab_flick_speed))
    {
        return TRUE;
    }

    makeforward(VRGetHandRot(xHand));
    vector w = VRGetHandAVel(xHand);
    vector f = v_forward;
    vector tip; // w x f: the fingertips' swing
    tip_x = w_y * f_z - w_z * f_y;
    tip_y = w_z * f_x - w_x * f_z;
    tip_z = w_x * f_y - w_y * f_x;

    return (tip * pull) * 57.29578 >= cvar_hget(cvarh_vr_forcegrab_flick_turn);
}

// ----------------------------------------------------------------------------
// Flight
// ----------------------------------------------------------------------------

// Back to how it was before it flew.
void(entity e) VR_Forcegrab_Restore =
{
    e.movetype = e.fg_movetype;
    e.solid = e.fg_solid;
    e.think = e.fg_think;
    e.nextthink = e.fg_think ? time + 0.02 : 0;
    e.fg_state = 0;
    setorigin(e, e.origin);
}

// Whether the hand closes in time: empty, gripping, and the grip pressed at most
// vr_forcegrab_catch_early seconds before `xArrive`.
float(entity xPlayer, float xHand, float xArrive) VR_Forcegrab_CanCatch =
{
    if(!VRIsHandEmpty(xPlayer, xHand) || !VR_HandGrabUtil_IsHandGrabbing(xPlayer, xHand))
    {
        return FALSE;
    }

    return VR_HandGrabUtil_Impl_GetLastGrabTime(xPlayer, xHand) >=
           xArrive - cvar_hget(cvarh_vr_forcegrab_catch_early);
}

void(entity e, entity xPlayer, float xHand) VR_Forcegrab_Catch =
{
    dprint("force grab: caught ");
    dprint(e.classname);
    dprint(" at ");
    dprint(ftos(time - e.fg_arrive));
    dprint(" s\n");
    VR_Forcegrab_Restore(e);
    e.velocity = '0 0 0';
    e.avelocity = '0 0 0';
    e.vr_spin = '0 0 0';
    setorigin(e, VRGetEntHandPos(xPlayer, xHand) - (e.mins + e.maxs) * 0.5);

    if(xPlayer.ishuman)
    {
        haptic(xHand, 0.0, 0.1, 60, 0.8);
    }

    // Handed over as the hand touching it would, the grab taken as started.
    entity oldSelf = self;
    entity oldOther = other;

    xPlayer.touchinghand = xHand;
    e.handtouch_hand = xHand;
    e.handtouch_ent = xPlayer;

    self = e;
    other = xPlayer;
    forcegrabCatching = TRUE;
    self.handtouch();
    forcegrabCatching = FALSE;

    self = oldSelf;
    other = oldOther;
}

// Missed: it falls from where it fits, a little of its speed kept, not touchable for a moment.
void(entity e) VR_Forcegrab_Miss =
{
    dprint("force grab: missed ");
    dprint(e.classname);
    dprint("\n");
    entity p = e.fg_player;
    if(p.classname == "player")
    {
        vector from = p.origin + '0 0 10' - (e.mins + e.maxs) * 0.5;
        tracebox(from, e.mins, e.maxs, e.origin, TRUE, p);
        if(!trace_startsolid)
        {
            setorigin(e, trace_endpos);
        }
    }

    vector v = e.velocity * cvar_hget(cvarh_vr_forcegrab_miss_speed);
    v_z = 0;
    e.velocity = v;
    e.avelocity = e.avelocity * 0.5;

    e.movetype = e.fg_movetype;
    if(e.movetype == MOVETYPE_NONE || e.movetype == MOVETYPE_NOCLIP)
    {
        e.movetype = MOVETYPE_TOSS;
    }
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    e.vr_rest = 0;
    e.fg_state = 2;
    setorigin(e, e.origin);
}

void() VR_Forcegrab_FlightThink =
{
    self.nextthink = time; // every frame

    entity p = self.fg_player;
    float hand = self.fg_hand;

    if(p.classname != "player" || p.health <= 0)
    {
        if(self.fg_state == 1)
        {
            VR_Forcegrab_Miss(self);
        }
        VR_Forcegrab_Restore(self);
        return;
    }

    vector centreOffset = (self.mins + self.maxs) * 0.5;
    vector handPos = VRGetEntHandPos(p, hand);
    float reach = vlen(handPos - (self.origin + centreOffset));
    float catchRadius = cvar_hget(cvarh_vr_forcegrab_catch_radius);

    if(self.fg_state == 1)
    {
        if(reach < catchRadius && VR_Forcegrab_CanCatch(p, hand, self.fg_arrive))
        {
            VR_Forcegrab_Catch(self, p, hand);
            return;
        }

        float remaining = self.fg_arrive - time;
        if(remaining <= 0.001)
        {
            VR_Forcegrab_Miss(self);
            return;
        }

        // The arc that reaches the hand (where it is now) when due: recomputed every frame, it
        // homes on a moving hand.
        vector target = handPos - centreOffset;
        self.velocity = (target - self.origin) * (1 / remaining);
        self.velocity_z = self.velocity_z + 0.5 * self.fg_gravity * remaining;
        return;
    }

    // Missed and falling: a late grab still catches it within reach.
    if(time < self.fg_arrive + cvar_hget(cvarh_vr_forcegrab_catch_late) && reach < catchRadius * 1.5 &&
       VR_Forcegrab_CanCatch(p, hand, self.fg_arrive))
    {
        VR_Forcegrab_Catch(self, p, hand);
        return;
    }

    if(time >= self.fg_arrive + 0.4)
    {
        VR_Forcegrab_Restore(self);
    }
}

void(entity e, float xHand) VR_Forcegrab_Launch =
{
    vector centreOffset = (e.mins + e.maxs) * 0.5;
    vector start = e.origin + centreOffset;
    vector handPos = VRGetHandPos(xHand);
    float dist = vlen(handPos - start);
    float flight = cvar_hget(cvarh_vr_forcegrab_time) + dist / max(1, cvar_hget(cvarh_vr_forcegrab_speed));

    // The arc peaks vr_forcegrab_arc of the distance above the straight line; lower under a
    // lower ceiling.
    float arc = cvar_hget(cvarh_vr_forcegrab_arc) * dist;
    vector mid = (start + handPos) * 0.5;
    traceline(mid, mid + '0 0 1' * (arc + 8), TRUE, e);
    arc = max(0, (arc + 8) * trace_fraction - 8);

    e.fg_movetype = e.movetype;
    e.fg_solid = e.solid;
    e.fg_think = e.think;
    e.fg_player = self;
    e.fg_hand = xHand;
    e.fg_state = 1;
    e.fg_arrive = time + flight;
    e.fg_gravity = 8 * arc / (flight * flight);

    // Dropped, not thrown: no damage when it lands.
    if(e.throwhit == QVR_THROWHIT_NEVER_HIT)
    {
        e.throwhit = QVR_THROWHIT_HIT;
    }
    e.owner = self; // does not land on the player's head

    e.movetype = MOVETYPE_NOCLIP;
    e.solid = SOLID_NOT;
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    e.vr_rest = 0;
    e.vr_spin = '0 0 0';
    e.avelocity_x = crandom() * 60;
    e.avelocity_y = crandom() * 180;
    e.avelocity_z = crandom() * 60;

    e.think = VR_Forcegrab_FlightThink;
    e.nextthink = time;
    setorigin(e, e.origin);

    sound(e, CHAN_AUTO, "forcegrab.wav", 0.65, ATTN_NORM);
    haptic(xHand, 0.0, 0.12, 50, 0.6);
    dprint("force grab: ");
    dprint(e.classname);
    dprint(" flies ");
    dprint(ftos(dist));
    dprint(" units in ");
    dprint(ftos(flight));
    dprint(" s\n");
}

// ----------------------------------------------------------------------------
// Player frame
// ----------------------------------------------------------------------------

void(float xHand) VR_Forcegrab_HandFrame =
{
    if(!cvar_hget(cvarh_vr_forcegrab_mode) || !self.ishuman || self.health <= 0 ||
       !VRIsHandEmpty(self, xHand))
    {
        VR_Forcegrab_Set(xHand, world, FALSE);
        return;
    }

    entity target = VR_Forcegrab_GetTarget(xHand);
    float trigger = VRGetFireButtonPressed(xHand);

    // Locked on: flick to pull it; let go of the trigger to cancel.
    if(VR_Forcegrab_GetLocked(xHand))
    {
        if(!trigger || !VR_Forcegrab_IsEligible(target) ||
           vlen(VR_Forcegrab_Centre(target) - VRGetHandPos(xHand)) > cvar_hget(cvarh_vr_forcegrab_distance) * 1.25)
        {
            VR_Forcegrab_Set(xHand, world, FALSE);
            return;
        }

        if(VR_Forcegrab_Flicked(xHand, target))
        {
            VR_Forcegrab_Launch(target, xHand);
            VR_Forcegrab_Set(xHand, world, FALSE);
            return;
        }

        particle2(VR_Forcegrab_Centre(target), '0 0 0', QVR_PARTICLE_PRESET_GUNFORCEGRAB, 1);
        return;
    }

    // Pointing with an open hand.
    if(VR_HandGrabUtil_IsHandGrabbing(self, xHand))
    {
        VR_Forcegrab_Set(xHand, world, FALSE);
        return;
    }

    entity best = VR_Forcegrab_FindTarget(xHand);
    if(best != target && best != world && cvar_hget(cvarh_vr_forcegrab_eligible_haptics))
    {
        haptic(xHand, 0.0, 0.08, 75, 0.2);
    }

    if(best == world)
    {
        VR_Forcegrab_Set(xHand, world, FALSE);
        return;
    }

    if(trigger)
    {
        VR_Forcegrab_Set(xHand, best, TRUE);
        haptic(xHand, 0.0, 0.1, 100, 0.5);
        return;
    }

    VR_Forcegrab_Set(xHand, best, FALSE);
    if(cvar_hget(cvarh_vr_forcegrab_eligible_particles) && random() < 0.35)
    {
        particle2(VR_Forcegrab_Centre(best), '0 0 0', QVR_PARTICLE_PRESET_GUNFORCEGRAB, 1);
    }
}

void() W_Frame_Forcegrab =
{
    VR_Forcegrab_HandFrame(cVR_MainHand);
    VR_Forcegrab_HandFrame(cVR_OffHand);
}
