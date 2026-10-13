// vr_foegrab_throw.qc -- the two-hand throw (Combat > Holding Enemies, "Two-Hand Throw"; ROUND21.md, "Holding enemies").
// Both hands holding one enemy (vr_foegrab.cpp) and turning it over hard enough (the engine's throwCheck: the hands' turn
// of it about a level axis through its middle, vr_foegrab_throw_twist, and their speed, vr_foegrab_throw_speed) throw it
// down that way: a knockdown as a shove's (vr_knockdown.qc: it falls as a ragdoll, lies, gets up), for sure, by its kind
// and its health:
// - always: the kinds in vr_foegrab_throw_always (a grunt, an enforcer, a zombie, a knight, a rottweiler, a mummy, the
//   infected);
// - only when hurt, below vr_foegrab_throw_hurt of its full health (and never above): vr_foegrab_throw_when_hurt (a death
//   knight, the ogres, a fiend, a ranged knight);
// - never: any other (a shambler, a vore, the bosses; a spawn, Dawn of the Machine's slime and Rogue's centroid, the
//   scorpion: they have no knockdown get-up), and anything flying or swimming;
// or by its mass (vr_foegrab_throw_by_mass: light up to _mass_always, hurt up to _mass_hurt). The engine reads the class
// lists (they are strings) and passes the kind; the rest is here. One hand never throws.

// By its mass: 0 always, 1 only when hurt, 2 never (a vore and an overlord: rooted, as VR_FoeGrab_Size has them; a
// spawn, a slime and a centroid: no knockdown get-up).
float(entity m) VR_FoeGrab_ThrowMassTier =
{
    string c = m.classname;
    if(c == "monster_shalrath" || c == "monster_super_wrath" || c == "monster_tarbaby" || c == "monster_slime" ||
       c == "monster_scourge")
    {
        return 2;
    }
    float mass = VR_Grapple_MonsterMass(m);
    if(mass <= cvar_hget(cvarh_vr_foegrab_throw_mass_always))
    {
        return 0;
    }
    if(mass <= cvar_hget(cvarh_vr_foegrab_throw_mass_hurt))
    {
        return 1;
    }
    return 2;
};

// Self the player, m held in both hands and turned over towards yaw xYaw. xTier the engine's (its class lists): 0 always,
// 1 only when hurt, 2 never; -1 by its mass. Returns 1 thrown (knocked down), 2 the training dummy (shown, not knocked
// down: it stands on its spot), 0 never thrown (its kind), -1 not hurt enough, -2 it can't be knocked down now (no
// ragdoll or get-up, no room for one, down already).
float(entity m, float xYaw, float xTier) VR_FoeGrab_Throw =
{
    float tier = xTier < 0 ? VR_FoeGrab_ThrowMassTier(m) : xTier;
    float dummy = VR_IsDummy(m);
    float full = dummy ? m.vr_dummy_hpmax : (m.max_health > 0 ? m.max_health : m.health);
    float left = dummy ? m.vr_dummy_hp : m.health;
    float share = full > 0 ? left / full : 1;
    float result;
    if(tier >= 2 || (m.flags & (FL_FLY | FL_SWIM)))
    {
        result = 0;
    }
    else if(tier == 1 && share >= cvar_hget(cvarh_vr_foegrab_throw_hurt))
    {
        result = -1;
    }
    else if(dummy)
    {
        result = 2;
    }
    else if(m.vr_knockdown || !m.vr_kd_chance_h || !canragdoll(m) || !VR_Knockdown_MakeRoom(m))
    {
        result = -2;
    }
    else
    {
        makevectors('0 1 0' * xYaw);
        vector was = m.velocity;
        m.velocity = v_forward * max(0, cvar_hget(cvarh_vr_foegrab_throw_push)) +
                     '0 0 1' * max(0, cvar_hget(cvarh_vr_foegrab_throw_lift));
        result = VR_Knockdown_Start(m) ? 1 : -2;
        if(result < 1)
        {
            m.velocity = was; // (not knocked down: it stands, no shove)
        }
    }
    if(result < 1)
    {
        // Too strong for you: the hands feel it hold its ground.
        haptic(cVR_OffHand, 0, 0.1, 40, 0.5);
        haptic(cVR_MainHand, 0, 0.1, 40, 0.5);
    }
    if(dummy)
    {
        // The training dummy shows how it went, over its head.
        vector at = m.origin + '0 0 1' * (m.maxs_z + 8);
        if(result == 2)
        {
            floattext(at, "thrown", '0.4 1 0.4', 0.6);
        }
        else if(result == -1)
        {
            floattext(at, sprintf("hurt it below %g%%", rint(cvar_hget(cvarh_vr_foegrab_throw_hurt) * 100)), '1 0.8 0.2', 0.6);
        }
        else
        {
            floattext(at, "never thrown", '1 0.3 0.2', 0.6);
        }
    }
    return result;
};

// The engine (vr_foegrab.cpp handOver): m, thrown or lifted, its ragdoll held on by hand xHand of the player self (the
// engine's soft hold on the limb nearest the hand: box3d ragdollHoldSoft). The hand holds it as a ragdoll's limb (as
// VR_Ragdoll_Handtouch takes one): VR_Ragdoll_HandFrame lets go of it as any (the grip let go: it keeps the hand's throw).
// Returns 1 held, 0 the hand is busy (a weapon, something held).
float(entity m, float xHand, float xUnused) VR_FoeGrab_HandOver =
{
    if(!VRIsHandEmpty(self, xHand) || VR_Forcegrab_IsHandBusy(self, xHand) || !isragdoll(m))
    {
        return 0;
    }
    VR_Carry_SetHeld(self, xHand, m);
    return 1;
};

// The engine (vr_foegrab.cpp liftCheck): m pulled straight up by both hands of the player self: knocked down (its
// ragdoll launched up at xUp units/s), its get-up held off xHold s (it can't rise in the hands; it breaks free before).
// Returns 1 lifted, 2 the training dummy (shown: it stands), -2 it can't be knocked down now (no ragdoll or get-up, no
// room for one, down already, flying or swimming).
float(entity m, float xUp, float xHold) VR_FoeGrab_Lift =
{
    float dummy = VR_IsDummy(m);
    float result;
    if(dummy)
    {
        result = 2;
    }
    else if((m.flags & (FL_FLY | FL_SWIM)) || m.vr_knockdown || !m.vr_kd_chance_h || !canragdoll(m) || !VR_Knockdown_MakeRoom(m))
    {
        result = -2;
    }
    else
    {
        vector was = m.velocity;
        m.velocity = '0 0 1' * max(0, xUp);
        result = VR_Knockdown_Start(m) ? 1 : -2;
        if(result == 1)
        {
            m.vr_kd_until = max(m.vr_kd_until, time + xHold);
        }
        else
        {
            m.velocity = was; // (not knocked down: it stands, no hop)
        }
    }
    if(result < 1)
    {
        haptic(cVR_OffHand, 0, 0.1, 40, 0.5); // (it holds its ground)
        haptic(cVR_MainHand, 0, 0.1, 40, 0.5);
    }
    if(dummy)
    {
        floattext(m.origin + '0 0 1' * (m.maxs_z + 8), "lifted", '0.4 1 0.4', 0.6);
    }
    return result;
};

// The engine (vr_foegrab.cpp breakFree): m, lifted by the player self, broke free (vr_foegrab_lift_time): the hands
// holding it lose it, it shoves him back xDist units (VR_EnemyShove_Push's slide) and gets up as soon as it lies.
float(entity m, float xDist, float xUnused) VR_FoeGrab_BreakFree =
{
    if(VR_Carry_Held(self, cVR_OffHand) == m)
    {
        VR_Carry_SetHeld(self, cVR_OffHand, world);
    }
    if(VR_Carry_Held(self, cVR_MainHand) == m)
    {
        VR_Carry_SetHeld(self, cVR_MainHand, world);
    }
    VR_EnemyShove_Push(self, self.origin - m.origin, xDist);
    m.vr_kd_until = time;
    haptic(cVR_OffHand, 0, 0.2, 60, 0.9);
    haptic(cVR_MainHand, 0, 0.2, 60, 0.9);
    return 1;
};

// The neck snap (Combat > Holding Enemies > Neck Snap; vr_foegrab.cpp neckCheck, ragdollNeckCheck; ROUND22.md, "The neck
// snap"). The engine: whether xAt (a hand's hold, on m's model) is at its head or neck: positional damage's head zone, the
// headshots' (VR_Decap_HeadZone; the point as on its model standing: hitmodel_rest), grown by xMargin units, down to
// vr_foegrab_necksnap_neck units below the head sphere's bottom and no lower (the sphere grown reaches its shoulders:
// a hold there is the judo throw's, the lift's).
float(entity m, vector xAt, float xMargin) VR_FoeGrab_OnHead =
{
    if(!VR_Decap_HeadZone(m))
    {
        return FALSE;
    }
    float yaw = m.angles_y * (3.14159265 / 180);
    vector facing;
    facing_x = cos(yaw);
    facing_y = sin(yaw);
    facing_z = 0;
    vector head = m.origin + facing * xHeadForward + '0 0 1' * xHeadUp;
    vector p = hitmodel_target(m) ? hitmodel_rest(m, xAt) : xAt;
    if(p_z < head_z - xHeadRadius - max(0, cvar_hget(cvarh_vr_foegrab_necksnap_neck)))
    {
        return FALSE;
    }
    return vlen(p - head) < xHeadRadius + VR_HEAD_MARGIN + xMargin;
};

// The engine: both hands of the player self twisted the head of m (a humanoid: its class list) round: its neck snaps. It
// dies on the spot, killed by him, whole (no gib, no head wound; a zombie or a mummy for good, lying as its ragdoll does,
// as a beheaded one's), its ragdoll limp (.vr_death_hit the head's: a collapse); the crack at xVolume on its voice
// channel, over its death cry. Standing (xRagdoll 0), it is knocked down first (VR_Knockdown_Start, no push): its ragdoll
// made now, for the hands to hold (the engine's handOver). Returns 1 killed, a ragdoll; 3 killed standing (no ragdoll:
// its death animation, the hands let go); 2 the training dummy (shown: it stands); 0 it can't die now (dead already).
float(entity m, float xVolume, float xRagdoll) VR_FoeGrab_NeckSnap =
{
    if(VR_IsDummy(m))
    {
        sound(m, CHAN_AUTO, "vr/necksnap.wav", xVolume, ATTN_NORM);
        floattext(m.origin + '0 0 1' * (m.maxs_z + 8), "neck snapped", '0.4 1 0.4', 0.6);
        haptic(cVR_OffHand, 0, 0.15, 50, 0.8);
        haptic(cVR_MainHand, 0, 0.15, 50, 0.8);
        return 2;
    }
    if(m.health <= 0 || !(m.flags & FL_MONSTER) || m.takedamage == DAMAGE_NO)
    {
        return 0;
    }
    string was = m.vr_knockdown == 1 ? (m.vr_kd_stagger ? "staggering" : "knocked down") : "standing";
    float down = m.vr_knockdown == 1 && isragdoll(m);
    if(!down && !m.vr_knockdown && m.vr_kd_chance_h && !(m.flags & (FL_FLY | FL_SWIM)) && canragdoll(m) &&
       VR_Knockdown_MakeRoom(m))
    {
        m.velocity = '0 0 0';
        down = VR_Knockdown_Start(m);
    }
    m.vr_death_hit = 1;
    m.vr_death_hitpos = '0 0 0';
    m.health = -1;
    if(!VR_AD_NeckSnapKilled(m, self)) // AD support: an AD monster dies by AD's Killed (vr_ad_monsters.qc)
    {
        if(VR_Decap_IsZombie(m) || m.classname == "monster_mummy")
        {
            m.th_die = VR_Decap_ZombieDie; // (its death code gibs it)
        }
        Killed(m, self);
    }
    m.vr_death_hit = 1; // (its ragdoll, made later standing, collapses)
    sound(m, CHAN_VOICE, "vr/necksnap.wav", xVolume, ATTN_NORM);
    haptic(cVR_OffHand, 0, 0.25, 45, 1);
    haptic(cVR_MainHand, 0, 0.25, 45, 1);
    dprint(sprintf("necksnap: %s's neck snapped (%s, %s), %s; killed: %g monsters\n", m.classname, was,
        xRagdoll ? "a ragdoll" : "held standing", down ? "its ragdoll held" : "no ragdoll", killed_monsters));
    return down ? 1 : 3;
};
