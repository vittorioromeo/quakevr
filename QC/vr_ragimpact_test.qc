// vr_ragimpact's test harness (moved out of vr_ragimpact.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// For tests (vr_knockdown_test 50..59; self the player), the nearest monster down as a ragdoll, alive: 50 its state (any
// nearest, a corpse too: health, its head's and limbs' worn damage, the strikes dealt so far; and your health), 51 flung
// at the nearest wall at vr_test_throw_up_speed m/s and 3 m/s up (as your throw), 52 you put vr_test_drop_height (256)
// units over its middle (a two-storey drop onto it), 53 the same 96 units (a short hop); the nearest standing one: 54
// made staggered now (.vr_kd_shaken), 55 its health set to a fifth, 58 moved to 48 units off the nearest wall, 59 you put
// vr_test_drop_height units over its top; 56 strikes printed from now (57: off again); the nearest ragdoll (down or a
// corpse): 60 you over its head, 61 over its left shin, your feet vr_test_drop_height over the floor; 62 the nearest
// living one killed whole.
void(float xMode) VR_RagImpact_Test =
{
    if(xMode == 56 || xMode == 57)
    {
        vr_rimp_test = xMode == 56;
        dprint(sprintf("ragimpact test: prints %s\n", vr_rimp_test ? "on" : "off"));
        return;
    }
    entity best = world;
    float bestDist = 100000;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if(!(m.flags & FL_MONSTER) || vlen(m.origin - self.origin) >= bestDist)
        {
            continue;
        }
        float down = m.health > 0 && m.vr_knockdown == 1;
        if(xMode == 60 || xMode == 61 || xMode == 62)
        {
            down = ((xMode == 62) ? (m.health > 0) : (isragdoll(m) && !m.vr_gib));
        }
        else if(xMode == 54 || xMode == 55 || xMode == 58 || xMode == 59)
        {
            down = ((m.health > 0) && !m.vr_knockdown);
        }
        else if(xMode == 50)
        {
            down = (isragdoll(m) || (m.health > 0));
        }
        if(down)
        {
            best = m;
            bestDist = vlen(m.origin - self.origin);
        }
    }
    if(best == world)
    {
        dprint("ragimpact test: no monster\n");
        return;
    }
    if(xMode == 50)
    {
        float limbs = 0;
        for(float i = 1; i < 16; i++)
        {
            limbs = max(limbs, best.vr_limb_corpse_dmg[i]);
        }
        dprint(sprintf("ragimpact state: %s health %g knockdown %g headless %g head worn %.1f, most on a limb %.1f",
            best.classname, best.health, best.vr_knockdown, best.vr_headless, best.vr_limb_corpse_dmg[0], limbs));
        dprint(sprintf("; strikes %g, %.1f damage; decaps %g, limb pops %g", vr_rimp_dealt, vr_rimp_total,
            vr_decap_count, vr_limb_pops));
        float head = ragdolllimbs(best, 3);
        vector at = (isragdoll(best) && head >= 0) ? ragdollpoint(best, head, '0 0 0', 1) : best.origin;
        dprint(sprintf("; its head at %v, its feet at %.0f; your main hand at %v\n", at, best.absmin_z,
            VRGetHandPos(cVR_MainHand)));
        dprint(sprintf("ragimpact you: health %g\n", self.health));
        dprint(sprintf("ragimpact doll: %s ragdoll %g knockdown %g health %g\n", best.classname, isragdoll(best),
            best.vr_knockdown, best.health));
        return;
    }
    if(xMode == 51)
    {
        // Towards the nearest wall level with its middle (8 ways looked at), as if you threw it (its letting go yours).
        vector from = (best.absmin + best.absmax) * 0.5;
        vector dir = '0 0 0';
        float near = 2;
        for(float k = 0; k < 8; k++)
        {
            makevectors('0 1 0' * k * 45);
            traceline(from, from + v_forward * 512, TRUE, best);
            if(trace_fraction < near)
            {
                near = trace_fraction;
                dir = v_forward;
            }
        }
        if(dir == '0 0 0')
        {
            dir = best.origin - self.origin;
            dir_z = 0;
            dir = normalize(dir);
        }
        float speed = cvar("vr_test_throw_up_speed") * VRMetersToUnits();
        best.velocity = dir * speed + '0 0 1' * 3 * VRMetersToUnits();
        best.vr_rag_letgo = self;
        best.vr_rag_letgo_hand = 0;
        best.vr_rag_letgo_time = time;
        dprint(sprintf("ragimpact test: %s flung at %.1f m/s at a wall %.0f units away\n", best.classname,
            cvar("vr_test_throw_up_speed"), near * 512));
        return;
    }
    if(xMode == 52 || xMode == 53)
    {
        float up = xMode == 52 ? cvar("vr_test_drop_height") : 96;
        vector mid = (best.absmin + best.absmax) * 0.5;
        setorigin(self, mid + '0 0 1' * (up - self.mins_z));
        self.velocity = '0 0 0';
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        dprint(sprintf("ragimpact test: you %g units over %s\n", up, best.classname));
        return;
    }
    if(xMode == 60 || xMode == 61)
    {
        // You over its head (60) or its left shin (61), your feet vr_test_drop_height units over the floor it lies on
        // (46: a jump from the same level, about 270 units/s; 112: two heights).
        float part = (xMode == 60) ? ragdolllimbs(best, 3) : ragdolllimbs(best, 6, 6);
        if(part < 0)
        {
            dprint(sprintf("ragimpact test: %s has no such part\n", best.classname));
            return;
        }
        vector on = limbplace(best, part, (xMode == 60) ? 0 : 3); // (the head's middle; a point on the shin)
        traceline(on, on - '0 0 256', TRUE, best);
        on_z = trace_endpos_z + cvar("vr_test_drop_height");
        setorigin(self, on - '0 0 1' * self.mins_z);
        self.velocity = '0 0 0';
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        dprint(sprintf("ragimpact test: you %g units over %s's %s (health %g)\n", cvar("vr_test_drop_height"),
            best.classname, (xMode == 60) ? "head" : "left shin", best.health));
        return;
    }
    if(xMode == 62)
    {
        // Killed, whole (a corpse, as a ragdoll if it may be one).
        T_Damage(best, world, self, best.health + 1);
        dprint(sprintf("ragimpact test: %s killed, ragdoll %g\n", best.classname, isragdoll(best)));
        return;
    }
    if(xMode == 59)
    {
        // You vr_test_drop_height units over the nearest standing one's top, over its middle (its head).
        float drop = cvar("vr_test_drop_height");
        setorigin(self, best.origin + '0 0 1' * (best.maxs_z + drop - self.mins_z));
        self.velocity = '0 0 0';
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        dprint(sprintf("ragimpact test: you %g units over standing %s (health %g)\n", drop, best.classname, best.health));
        return;
    }
    if(xMode == 58)
    {
        // Moved to 48 units off the nearest wall level with its middle (8 ways, within 512), its feet where they were.
        vector mid = (best.absmin + best.absmax) * 0.5;
        float wall = 2;
        vector way = '0 0 0';
        for(float w = 0; w < 8; w++)
        {
            makevectors('0 1 0' * w * 45);
            traceline(mid, mid + v_forward * 512, TRUE, best);
            if(trace_fraction < wall)
            {
                wall = trace_fraction;
                way = v_forward;
            }
        }
        if(way == '0 0 0')
        {
            dprint("ragimpact test: no wall near\n");
            return;
        }
        float move = wall * 512 - 48 - best.maxs_x;
        setorigin(best, best.origin + way * move);
        dprint(sprintf("ragimpact test: %s moved %.0f units towards a wall, 48 off it\n", best.classname, move));
        return;
    }
    if(xMode == 54)
    {
        VR_Manhandle_Shaken(best);
        dprint(sprintf("ragimpact test: %s staggered now\n", best.classname));
        return;
    }
    if(xMode == 55)
    {
        float full = best.max_health > 0 ? best.max_health : best.health;
        best.health = max(1, rint(full * 0.2));
        dprint(sprintf("ragimpact test: %s at %g of %g health\n", best.classname, best.health, full));
        return;
    }
};
