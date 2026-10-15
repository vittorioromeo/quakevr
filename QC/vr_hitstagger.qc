// vr_hitstagger.qc -- hits that unbalance (vr_hit_stagger; Combat > Knockdowns, "Hits Unbalance"; ROUND22.md, "Hits that
// unbalance"): a strong throw or melee blow may make a monster lose its balance as a shove does (the author, 2026-10-15:
// "strong prop/weapon throws and melee hits make enemies lose balance like a shove").
//
// - Impact: a thrown prop's or weapon's momentum into it (its mass x its speed towards it, kg m/s) over
//   vr_hit_stagger_throw_ref; a melee blow's damage (its speed, its weapon and its zone in) over vr_hit_stagger_melee_ref,
//   times vr_hit_stagger_head at the head (a fist's too). Plus vr_hit_stagger_damage x the hit's damage as a share of its
//   full health.
// - Chance: none under vr_hit_stagger_min (a tap), rising to sure at impact 1; times (30 / its full health) ^
//   vr_hit_stagger_resist above 30 health (big monsters resist: the shambler about a tenth); times vr_hit_stagger_chance.
//   Never a dummy that stays put (vr_dummy_anchored), one already down or staggering, nor a boss (1000 health or more).
// - Then, for vr_hit_stagger_cooldown seconds, no hit unbalances it again (no stunlock); shoves and bashes as ever.
// - The reaction is the shove's: pushed as a shove of that strength is (vr_hit_stagger_push; impact 1 a two-hand shove's)
//   and, with Knockdowns and an active ragdoll (vr_ragdoll_active_stagger), staggering as a ragdoll, its balance the
//   shove's roll against its knockdown chance (so it recovers, or by that chance goes over: VR_Knockdown_Stagger). Other
//   monsters (no ragdoll, flying, Arcane Dimensions'): pushed and a forced pain, kept from attacking a moment (as a bash).
// - vr_knockdown_debug (developer 1) prints each roll; vr_knockdown_test 40..45 test it (VR_HitStagger_Test).

float VR_HST_HEALTH_REF = 30;  // the full health big monsters' resistance is reckoned from (a grunt's: none)
float VR_HST_BOSS = 1000;      // full health from which a monster is never unbalanced by a hit (Chthon, Shub and their kin)
float VR_HST_PAIN_TIME = 0.75; // s a monster without an active stagger is kept from attacking, times the push
float vr_hst_test;             // in a test (VR_HitStagger_Test): each roll printed whatever vr_knockdown_debug says

// Whether the blow at `xAt` struck `e`'s head: its positional zone (vr_hitpart, set this frame for it), else the top
// fifth of its box.
float(entity e, vector xAt) VR_HitStagger_Head =
{
    if(vr_hitpart_time == time && vr_hitpart_target == e)
    {
        return vr_hitpart == 1;
    }
    if(xAt == '0 0 0' || e.size_z <= 0)
    {
        return FALSE;
    }
    return xAt_z >= e.absmax_z - e.size_z * 0.2;
};

// Its chance to be unbalanced by a hit of impact `xImpact` dealing `xDamage` (0 .. 1, times vr_hit_stagger_chance).
float(entity e, float xImpact, float xDamage) VR_HitStagger_Chance =
{
    float full = e.max_health > 0 ? e.max_health : e.health;
    float share = full > 0 ? min(1, max(0, xDamage) / full) : 0;
    float score = xImpact + max(0, cvar_hget(cvarh_vr_hit_stagger_damage)) * share;
    float lo = min(0.99, max(0, cvar_hget(cvarh_vr_hit_stagger_min)));
    float p = min(1, max(0, (score - lo) / (1 - lo)));
    float resist = max(0, cvar_hget(cvarh_vr_hit_stagger_resist));
    if(full > VR_HST_HEALTH_REF && resist > 0)
    {
        p = p * pow(VR_HST_HEALTH_REF / full, resist);
    }
    return p * max(0, cvar_hget(cvarh_vr_hit_stagger_chance));
};

// Whether a hit may unbalance `e` at all now.
float(entity e, entity xBy) VR_HitStagger_Can =
{
    if(!cvar_hget(cvarh_vr_hit_stagger) || !(xBy.flags & FL_CLIENT) || !(e.flags & FL_MONSTER) || e.health <= 0 ||
       e.vr_knockdown || e.vr_corpse || e.vr_corpse_dying || time < e.vr_hst_next || e.solid == SOLID_BSP ||
       e.vr_parry_stagger_until > time || (VR_IsDummy(e) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return FALSE;
    }
    float full = e.max_health > 0 ? e.max_health : e.health;
    return full < VR_HST_BOSS;
};

// A hit of impact `xImpact` dealing `xDamage` by `xBy` (a player) going along `xDir`, on `e` (alive, its damage dealt):
// by chance it loses its balance as a shove makes it. 0 not, 1 a ragdoll's stagger (or its topple), 2 pushed and a
// forced pain (no active stagger for it).
float(entity e, entity xBy, vector xDir, float xImpact, float xDamage, string xWhat) VR_HitStagger_Try =
{
    if(!VR_HitStagger_Can(e, xBy))
    {
        return 0;
    }
    float chance = VR_HitStagger_Chance(e, xImpact, xDamage);
    float roll = random();
    float loud = vr_hst_test || cvar_hget(cvarh_vr_knockdown_debug);
    if(roll >= chance)
    {
        if(loud && chance > 0)
        {
            dprint(sprintf("hit stagger: %s by %s (impact %.2f, %.1f damage): chance %.2f, roll %.2f: keeps its balance\n",
                e.classname, xWhat, xImpact, xDamage, chance, roll));
        }
        return 0;
    }
    xDir_z = 0;
    xDir = normalize(xDir);
    if(xDir == '0 0 0')
    {
        xDir = normalize(e.origin - xBy.origin);
        xDir_z = 0;
        xDir = normalize(xDir);
    }
    e.vr_hst_next = time + max(0, cvar_hget(cvarh_vr_hit_stagger_cooldown));
    // The shove's push at this strength (VR_Bash_Hit's: 520 u/s and a 140 hop, times vr_bash_push).
    float push = min(1.25, max(0.5, xImpact)) * max(0, cvar_hget(cvarh_vr_hit_stagger_push));
    float bashPush = cvar_hget(cvarh_vr_bash_push) * push;
    VR_Push(e, xDir, 520 * bashPush, 140 * bashPush);
    // An active stagger (VR_Knockdown_Try's): its balance the shove's roll against its knockdown chance at this strength.
    if(cvar_hget(cvarh_vr_knockdown) && e.vr_kd_chance_h && !(e.flags & (FL_FLY | FL_SWIM)) && canragdoll(e) &&
       ragdollcanstagger(e))
    {
        float kd = cvar_hget(e.vr_kd_chance_h) * max(0, cvar_hget(cvarh_vr_knockdown_chance)) * min(1.5, xImpact);
        float kdRoll = random();
        float balance = min(1, max(0, 0.5 + (kdRoll - kd) * VR_KD_STAGGER_SLOPE));
        if(VR_Knockdown_Stagger(e, xDir, bashPush, balance))
        {
            if(loud)
            {
                dprint(sprintf("hit stagger: %s by %s (impact %.2f, %.1f damage): chance %.2f, roll %.2f", e.classname,
                    xWhat, xImpact, xDamage, chance, roll));
                dprint(sprintf(": staggers, push %.2f, balance %.2f\n", bashPush, balance));
            }
            return 1;
        }
    }
    // Else as a bash staggers: a pain forced, kept from attacking a moment (pushed above).
    entity oldSelf = self;
    if(e.th_pain && !e.vr_knockdown)
    {
        self = e;
        e.pain_finished = 0;
        e.th_pain(xBy, xDamage);
        e.attack_finished = max(e.attack_finished, time + VR_HST_PAIN_TIME * push);
        self = oldSelf;
    }
    else if(e.vr_ad_mon && !e.vr_knockdown)
    {
        VR_AD_Pain(e, xBy, xDamage, VR_HST_PAIN_TIME * push); // AD support: AD's pain function (vr_ad_monsters.qc)
    }
    if(loud)
    {
        dprint(sprintf("hit stagger: %s by %s (impact %.2f, %.1f damage): chance %.2f, roll %.2f: pushed and pained, push %.2f\n",
            e.classname, xWhat, xImpact, xDamage, chance, roll, bashPush));
    }
    return 2;
};

// A melee blow by `xBy` (T_Damage_VRMelee) dealing `xDamage` (its zone's multiplier in), at the head or not.
float(entity e, entity xBy, vector xDir, float xDamage, float xHead) VR_HitStagger_Melee =
{
    float impact = xDamage / max(1, cvar_hget(cvarh_vr_hit_stagger_melee_ref));
    if(xHead)
    {
        impact = impact * max(0, cvar_hget(cvarh_vr_hit_stagger_head));
    }
    return VR_HitStagger_Try(e, xBy, xDir, impact, xDamage, xHead ? "a blow to the head" : "a blow");
};

// A thrown prop or weapon (vr_carry.qc VR_Prop_Flung) of `xMass` kg meeting `e` at `xSpeed` units/s towards it, thrown by
// `xBy`, dealing `xDamage`.
float(entity e, entity xBy, vector xDir, float xMass, float xSpeed, float xDamage) VR_HitStagger_Throw =
{
    float momentum = max(0, xMass) * max(0, xSpeed) / max(1, VRMetersToUnits());
    float impact = momentum / max(1, cvar_hget(cvarh_vr_hit_stagger_throw_ref));
    return VR_HitStagger_Try(e, xBy, xDir, impact, xDamage, sprintf("a throw (%.1f kg, %.0f kg m/s)", xMass, momentum));
};

// For tests (vr_knockdown_test 40..45; self the player), on the nearest live monster: 40 lists its chances (a weak tap, a
// strong blow, a strong punch to the head, a heavy throw, a thrown weapon), 41 a heavy throw (a 12 kg crate at 10 m/s),
// 42 a strong blow (36 damage), 43 a weak tap (5), 44 a strong punch to the head (20), 45 its state. 41..44 deal no
// damage (the stagger alone) and print "hit stagger test: <class> <what>: <0 none, 1 a ragdoll's stagger, 2 pushed and
// pained, -1 its cooldown>".
void(float xMode) VR_HitStagger_Test =
{
    entity best = world;
    float bestDist = 100000;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if((m.flags & FL_MONSTER) && m.health > 0 && vlen(m.origin - self.origin) < bestDist)
        {
            best = m;
            bestDist = vlen(m.origin - self.origin);
        }
    }
    if(best == world)
    {
        dprint("hit stagger test: no live monster\n");
        return;
    }
    float m2u = VRMetersToUnits();
    float meleeRef = max(1, cvar_hget(cvarh_vr_hit_stagger_melee_ref));
    float throwRef = max(1, cvar_hget(cvarh_vr_hit_stagger_throw_ref));
    if(xMode == 40)
    {
        dprint(sprintf("hit stagger chances: %s (health %g of %g)", best.classname, best.health, best.max_health));
        dprint(sprintf(": weak tap %.3f, strong blow %.3f, head punch %.3f, heavy throw %.3f, weapon throw %.3f\n",
            VR_HitStagger_Chance(best, 5 / meleeRef, 5), VR_HitStagger_Chance(best, 36 / meleeRef, 36),
            VR_HitStagger_Chance(best, 20 / meleeRef * max(0, cvar_hget(cvarh_vr_hit_stagger_head)), 20),
            VR_HitStagger_Chance(best, 120 / throwRef, 16), VR_HitStagger_Chance(best, 36 / throwRef, 24)));
        return;
    }
    if(xMode == 45)
    {
        dprint(sprintf("hit stagger state: %s knockdown %g stagger %g, cooldown %.2f s\n", best.classname, best.vr_knockdown,
            best.vr_kd_stagger, max(0, best.vr_hst_next - time)));
        return;
    }
    float cooling = time < best.vr_hst_next;
    vector dir = best.origin - self.origin;
    float r = 0;
    string what = "";
    vr_hst_test = TRUE;
    if(xMode == 41)
    {
        what = "heavy throw";
        r = VR_HitStagger_Throw(best, self, dir, 12, 10 * m2u, 16);
    }
    else if(xMode == 42)
    {
        what = "strong blow";
        r = VR_HitStagger_Melee(best, self, dir, 36, FALSE);
    }
    else if(xMode == 43)
    {
        what = "weak tap";
        r = VR_HitStagger_Melee(best, self, dir, 5, FALSE);
    }
    else if(xMode == 44)
    {
        what = "head punch";
        r = VR_HitStagger_Melee(best, self, dir, 20, TRUE);
    }
    vr_hst_test = FALSE;
    dprint(sprintf("hit stagger test: %s %s: %g\n", best.classname, what, (cooling && !r) ? -1 : r));
};
