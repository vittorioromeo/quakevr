// vr_knockdown's test harness (moved out of vr_knockdown.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

void(float xMode) VR_Knockdown_Test =
{
    if(xMode == 70)
    {
        VR_Buzzsaw_Test(); // a buzzsaw on the nearest ragdoll (its blade cuts ragdolls)
        return;
    }
    if(xMode >= 40 && xMode < 50)
    {
        VR_HitStagger_Test(xMode); // (vr_hitstagger.qc: hits that unbalance)
        return;
    }
    if(xMode >= 50 && xMode < 66)
    {
        VR_RagImpact_Test(xMode); // (vr_ragimpact.qc: impacts, landing crush, manhandling)
        return;
    }
    if(xMode == 20)
    {
        float removed = 0;
        entity next = world;
        for(entity mon = nextent(world); mon; mon = next)
        {
            next = nextent(mon);
            if(mon.flags & FL_MONSTER)
            {
                if(mon.vr_shove != world && mon.vr_shove.classname == "vr_shove" && mon.vr_shove.owner == mon)
                {
                    remove(mon.vr_shove);
                }
                VR_Knockdown_RemoveBody(mon);
                removed = removed + 1;
            }
        }
        dprint(sprintf("knockdown test: %g monsters removed\n", removed));
        return;
    }
    // Feedback regression probes: 6 lists bodies and their breathing point;
    // 7 moves a living knockdown to the player (underwater test); 8 fires a
    // real nail at the nearest living monster at one health.
    if(xMode >= 6 && xMode <= 8)
    {
        entity m = VR_Decap_TestTarget(TRUE);
        if(xMode == 7)
        {
            m = world;
            float nearest = 999999;
            for(entity down = nextent(world); down; down = nextent(down))
            {
                float distance = vlen(down.origin - self.origin);
                if(down.vr_knockdown == 1 && down.health > 0 && distance < nearest)
                {
                    m = down;
                    nearest = distance;
                }
            }
        }
        if(xMode == 7 && m != world)
        {
            setorigin(m, self.origin);
            m.velocity = '0 0 0';
            m.vr_kd_until = time + 60;
        }
        if(xMode == 8 && m != world)
        {
            m.health = 1;
            VR_Test_Fire(1, (m.absmin + m.absmax) * 0.5);
        }
        for(entity body = nextent(world); body; body = nextent(body))
        {
            if((body.flags & FL_MONSTER) && !body.vr_gib)
            {
                float level = VR_Liquid_Level(body);
                dprint(sprintf("bodycheck: %s health=%g ragdoll=%g headless=%g knocked=%g water=%g type=%g\n",
                    body.classname, body.health, isragdoll(body), body.vr_headless, body.vr_knockdown,
                    level, vr_liquid_type));
                dprint(sprintf("breathcheck: %s breath=%g at=%v\n", body.classname, body.vr_liquid_air - time, body.origin));
            }
        }
        return;
    }
    entity best = world;
    float bestDist = 999999;
    entity x = nextent(world);
    while(x)
    {
        if((x.flags & FL_MONSTER) && x.health > 0)
        {
            float d = vlen(x.origin - self.origin);
            if(xMode == 5)
            {
                dprint(sprintf("knockdown: %s: state %g, health %g, ragdoll %g, at %v\n", x.classname,
                    x.vr_knockdown, x.health, isragdoll(x), x.origin));
            }
            else if(xMode == 1)
            {
                if(x.vr_knockdown == 1 && !VR_TestSkips(x))
                {
                    x.vr_kd_until = time;
                }
            }
            else if(d < bestDist && !VR_TestSkips(x) &&
                    ((xMode == 0 || (xMode >= 21 && xMode <= 24)) ? (!x.vr_knockdown && x.vr_kd_chance_h) :
                                                                  x.vr_knockdown == 1))
            {
                best = x;
                bestDist = d;
            }
        }
        x = nextent(x);
    }
    if(xMode == 1 || xMode == 5)
    {
        return;
    }
    if(xMode == 10)
    {
        entity near = world;
        float nearest = 999999;
        for(entity y = nextent(world); y; y = nextent(y))
        {
            if((y.flags & FL_MONSTER) && y.health > 0 && !VR_TestSkips(y) && vlen(y.origin - self.origin) < nearest)
            {
                near = y;
                nearest = vlen(y.origin - self.origin);
            }
        }
        if(near == world)
        {
            dprint("whynot: no living monster\n");
            return;
        }
        VR_Knockdown_WhyNot(near);
        return;
    }
    if(best == world)
    {
        dprint("knockdown test: no monster\n");
        return;
    }
    if(xMode == 21 || xMode == 22)
    {
        // 21, 22: a one- or two-handed open-palm shove of the nearest (VR_Bash_Hit, as a real one: its push, its stagger,
        // its chance; vr_knockdown_chance 100 for a sure knockdown), fresh (not tired).
        vector away = best.origin - self.origin;
        away_z = 0;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        float two = xMode == 22;
        dprint(sprintf("knockdown test: %s shove of %s\n", two ? "two-handed" : "one-handed", best.classname));
        VR_Bash_Hit(best, best.origin, normalize(away), QVR_HITKIND_SHOVE, two ? -1 : cVR_MainHand,
            two ? 1 : VR_BASH_ONE_DAMAGE, VR_Shove_HandsPush(two));
        return;
    }
    if(xMode == 23 || xMode == 24)
    {
        // 23: a hit of 30 from you on the nearest standing one (a zombie: its own fall, zombie_pain's 25 or more);
        // 24: a real grenade of yours fired at the floor 150 units short of it (its blast, not a direct hit).
        if(xMode == 23)
        {
            T_Damage(best, self, self, 30);
        }
        else
        {
            vector short = best.origin - normalize(best.origin - self.origin) * 150;
            short_z = best.absmin_z + 2;
            VR_Test_Fire(2, short);
        }
        dprint(sprintf("knockdown test: %s %s: state %g, ragdoll %g, health %g, solid %g\n", best.classname,
            xMode == 23 ? "hit for 30" : "a grenade fired near it", best.vr_knockdown, isragdoll(best), best.health,
            best.solid));
        return;
    }
    if(xMode == 0)
    {
        if(!(canragdoll(best)) || !VR_Knockdown_MakeRoom(best))
        {
            dprint(sprintf("knockdown test: %s cannot be knocked down\n", best.classname));
            return;
        }
        vector dir = normalize(best.origin - self.origin);
        dir_z = 0;
        best.velocity = normalize(dir) * 220 + '0 0 120';
        dprint(sprintf("knockdown test: %s down: %g\n", best.classname, VR_Knockdown_Start(best)));
        return;
    }
    float dmg = xMode == 2 ? 5 : (xMode == 3 ? best.health : best.health + 200);
    dprint(sprintf("knockdown test: %s hit for %g\n", best.classname, dmg));
    T_Damage(best, self, self, dmg);
};
