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
        else
        {
            // Thrown by you: its impacts count (vr_ragimpact_time; vr_ragimpact.qc), no hand spared by it.
            m.vr_rag_letgo = self;
            m.vr_rag_letgo_hand = 0;
            m.vr_rag_letgo_time = time;
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
// snap"). The engine: m's head sphere, positional damage's, the headshots' (VR_Decap_HeadZone), as
// '<forward> <up> <radius>' from its origin standing (turned with its yaw); radius 0: none. The engine's head zone for the
// hands' holds is shaped from it (vr_foegrab.cpp headZoneOf: the head, then a column as narrow as its neck under it; never
// its shoulders).
vector(entity m) VR_FoeGrab_HeadZone =
{
    if(!VR_Decap_HeadZone(m))
    {
        return '0 0 0';
    }
    vector r;
    r_x = xHeadForward;
    r_y = xHeadUp;
    r_z = xHeadRadius;
    return r;
};

// Its head zone's middle to its left (xHeadLeft: 0 for most; AD's zombie knight's), for the engine's hands' head zone.
float(entity m) VR_FoeGrab_HeadLeft =
{
    return VR_Decap_HeadZone(m) ? xHeadLeft : 0;
};

// The neck snap's crack (xVolume 0-2): in both hands of the player self (his hands' channels: played from each hand,
// right by the head they hold, a hand's breadth from his ears: it fills his head), and from m (its voice channel, cutting
// its death cry off; world: none). Two hands and the head: three layers, as loud as the loudest
// blows (vr/necksnap.wav alone is a dry transient: Misc/quakevr/make_necksnap_sound.py).
void(entity m, float xVolume) VR_FoeGrab_Crack =
{
    if(xVolume <= 0)
    {
        return;
    }
    float hands = min(1, 0.7 * xVolume);
    sound(self, VRGetHandChannel(cVR_OffHand), "vr/necksnap.wav", hands, ATTN_NORM);
    sound(self, VRGetHandChannel(cVR_MainHand), "vr/necksnap.wav", hands, ATTN_NORM);
    if(m != world)
    {
        sound(m, CHAN_VOICE, "vr/necksnap.wav", min(1, xVolume), ATTN_NORM);
    }
};

// The engine: both hands of the player self twisted the head of m (a humanoid: its class list) round: its neck snaps. It
// dies on the spot, killed by him, whole (no gib, no head wound; a zombie or a mummy for good, lying as its ragdoll does,
// as a beheaded one's), its ragdoll limp (.vr_death_hit the head's: a collapse); the crack (VR_FoeGrab_Crack, xVolume 0-2)
// in his hands and over its death cry. Standing (xRagdoll 0), it is knocked down first (VR_Knockdown_Start, no push): its ragdoll
// made now, for the hands to hold (the engine's handOver). Returns 1 killed, a ragdoll; 3 killed standing (no ragdoll:
// its death animation, the hands let go); 2 the training dummy (shown: it stands); 0 it can't die now (dead already).
float(entity m, float xVolume, float xRagdoll) VR_FoeGrab_NeckSnap =
{
    if(VR_IsDummy(m))
    {
        VR_FoeGrab_Crack(m, xVolume);
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
        if(VR_Limb_IsZombie(m))
        {
            m.th_die = VR_Decap_ZombieDie; // (its death code gibs it)
        }
        Killed(m, self);
    }
    m.vr_death_hit = 1; // (its ragdoll, made later standing, collapses)
    VR_FoeGrab_Crack(m, xVolume);
    haptic(cVR_OffHand, 0, 0.25, 45, 1);
    haptic(cVR_MainHand, 0, 0.25, 45, 1);
    dprint(sprintf("necksnap: %s's neck snapped (%s, %s), %s; killed: %g monsters\n", m.classname, was,
        xRagdoll ? "a ragdoll" : "held standing", down ? "its ragdoll held" : "no ragdoll", killed_monsters));
    return down ? 1 : 3;
};
