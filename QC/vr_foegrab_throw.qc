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
        m.velocity = v_forward * max(0, cvar_hget(cvarh_vr_foegrab_throw_push)) +
                     '0 0 1' * max(0, cvar_hget(cvarh_vr_foegrab_throw_lift));
        result = VR_Knockdown_Start(m) ? 1 : -2;
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
