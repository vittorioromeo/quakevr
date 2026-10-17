// vr_hitstagger's test harness (moved out of vr_hitstagger.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// For tests (vr_knockdown_test 40..46; self the player), on the nearest live monster: 40 lists its chances (a weak tap, a
// strong blow, a strong punch to the head, a heavy throw, a thrown weapon), 41 a heavy throw (a 12 kg crate at 10 m/s),
// 42 a strong blow (36 damage), 43 a weak tap (5), 44 a strong punch to the head (20), 45 its state. 41..44 deal no
// damage (the stagger alone) and print "hit stagger test: <class> <what>: <0 none, 1 a ragdoll's stagger, 2 pushed and
// pained, -1 its cooldown>"; 46 the nearest loose prop thrown into it (the real hit: VR_Prop_Flung).
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
    if(xMode == 46)
    {
        // The real way: the loose prop nearest you thrown as yours (VR_Carry_Throw) into its middle from 40 units before
        // it at vr_test_throw_up_speed m/s (its hit: VR_Prop_Flung; the roll printed with vr_knockdown_debug).
        entity prop = world;
        float near = 1024;
        for(entity p = nextent(world); p != world; p = nextent(p))
        {
            if(p.vr_rigid && p.carry_player == world && (p.touch == forcegrabbable_touch || p.vr_impact) &&
               vlen(p.origin - self.origin) < near)
            {
                near = vlen(p.origin - self.origin);
                prop = p;
            }
        }
        if(prop == world)
        {
            dprint("hit stagger test: no prop\n");
            return;
        }
        vector to = best.origin - self.origin;
        to_z = 0;
        to = normalize(to);
        vector mid = (best.absmin + best.absmax) * 0.5;
        prop.throwhit = QVR_THROWHIT_HIT;
        prop.owner = world;
        prop.vr_fling_hit = 0;
        prop.fg_state = 0;
        prop.flags = prop.flags - (prop.flags & FL_ONGROUND);
        prop.carry_letgo = self;
        prop.carry_letgo_time = time;
        setorigin(prop, mid - to * (40 + best.maxs_x) - (prop.mins + prop.maxs) * 0.5);
        prop.vr_spin = '0 0 0';
        prop.avelocity = '0 0 0';
        VR_Carry_Throw(prop, to * cvar("vr_test_throw_up_speed") * VRMetersToUnits());
        dprint(sprintf("hit stagger test: %s (%.1f kg) thrown at %s at %.1f m/s\n", prop.classname, VR_Thrown_Mass(prop),
            best.classname, cvar("vr_test_throw_up_speed")));
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
