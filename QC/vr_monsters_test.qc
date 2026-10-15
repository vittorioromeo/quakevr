// A whole map's monsters at once (vr_test_monsters; Debug > Tests > Whole-Map Monsters; the benchmarks' full-count
// scenarios, BENCHMARKS.md "Dawn of the Machine"): 1 report, 2 wake every live monster (each hunts the player, as if it
// had seen you), 3 kill every live monster with just enough damage (deaths, corpses and ragdolls as a fight's: up to
// vr_ragdoll_max at once, the rest corpses), 4 bring in every monster still waiting for its trigger (the official
// campaigns' deferred monsters, MG_MonsterActivate: the map's full count at once). Prints one "monsterstest:" line
// (bprint: any developer level; waiting: deferred monsters not yet brought in).
void(float xRequest) VR_MonstersTest_Run =
{
    entity p = find(world, classname, "player");
    float live = 0, dead = 0, awake = 0, waiting = 0, acted = 0;
    entity oself = self, oother = other, oact = activator;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.MG_deferred || AD_TestWaiting(e)) // (AD support: AD's own delayed monsters too)
        {
            if(xRequest == 4)
            {
                self = e;
                activator = p;
                self.use(); // MG_MonsterActivate (AD support: AD_MonsterActivate, AD's monster_spawn)
                acted++;
            }
            else
            {
                waiting++;
            }
            continue;
        }
        if(!(e.flags & FL_MONSTER))
        {
            continue;
        }
        if(e.health <= 0 || e.deadflag != DEAD_NO)
        {
            dead++;
            continue;
        }
        if(p && xRequest == 2 && e.enemy != p && AD_Run(e))
        {
            self = e;
            if(AD_TestWake(p)) acted++; // AD support: AD's monsters by AD's AI
        }
        else if(p && xRequest == 2 && e.enemy != p && e.th_run)
        {
            self = e;
            self.enemy = p;
            FoundTarget();
            acted++;
        }
        else if(p && xRequest == 3 && e.takedamage)
        {
            self = oself;
            T_Damage(e, p, p, e.health + 1);
            acted++;
        }
        live++;
        if(xRequest == 1)
        {
            // (developer: where each live one is, and whether it sees the player: a stuck one found)
            float sees = 0;
            if(p)
            {
                traceline(e.origin + e.view_ofs, p.origin + p.view_ofs, TRUE, e);
                sees = trace_fraction == 1;
            }
            dprint(sprintf("monsterstest: %s at %v enemy %s sees the player %g\n", e.classname, e.origin,
                           e.enemy.classname, sees));
        }
        if(e.enemy)
        {
            awake++;
        }
    }
    self = oself;
    other = oother;
    activator = oact;
    // (a builtin takes at most 8 parameters: the skill is the console's)
    bprint(sprintf("monsterstest: request %g map %s live %g awake %g dead %g waiting %g acted %g\n", xRequest, mapname, live,
                   awake, dead, waiting, acted));
};

// 5: the hit probe: level shots (a gun's trace, precise hits: MOVE_HITMODEL) from the player's side at the nearest live
// monster (from 72 units out of its box), every 8 units from below its box to 160 above it, through its middle: the heights (from its origin) that hit
// it, as ranges (a boss drawn as two entities, the Armagon's legs and torso: both its own).
void() VR_MonstersTest_HitProbe =
{
    entity p = find(world, classname, "player");
    entity m = world;
    float best = 3000;
    for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
    {
        if(e.health > 0 && vlen(e.origin - p.origin) < best)
        {
            best = vlen(e.origin - p.origin);
            m = e;
        }
    }
    if(m == world)
    {
        bprint("hitprobe: no monster near\n");
        return;
    }
    vector dir = m.origin - p.origin;
    dir_z = 0;
    dir = normalize(dir);
    float from = 0, last = 0, open = FALSE, hits = 0;
    for(float z = m.mins_z - 16; z <= m.maxs_z + 160; z = z + 8)
    {
        vector a = m.origin - dir * (m.maxs_x + 72) + '0 0 1' * z;
        vector b = a + dir * (2 * m.maxs_x + 144);
        traceline(a, b, MOVE_HITMODEL, p);
        // (past anything else in the way: a map's invisible walls, another monster)
        for(float k = 0; k < 4 && trace_ent != m && trace_ent != world && trace_fraction < 1; k++)
        {
            traceline(trace_endpos + dir, b, MOVE_HITMODEL, trace_ent);
        }
        if(trace_ent == m)
        {
            hits = hits + 1;
            if(!open)
            {
                from = z;
                open = TRUE;
            }
            last = z;
        }
        else if(open)
        {
            bprint(sprintf("hitprobe:   hit from %g to %g\n", from, last));
            open = FALSE;
        }
    }
    if(open)
    {
        bprint(sprintf("hitprobe:   hit from %g to %g\n", from, last));
    }
    bprint(sprintf("hitprobe: %s at %v (solid %g, %.0f away) box z %g..%g: %g hits (the heights above)\n", m.classname,
        m.origin, m.solid, best, m.mins_z, m.maxs_z, hits));
};

void() VR_MonstersTest_Frame =
{
    float request = cvar("vr_test_monsters");
    if(!request)
    {
        return;
    }
    cvar_set("vr_test_monsters", "0");
    if(request == 5)
    {
        VR_MonstersTest_HitProbe();
        return;
    }
    VR_MonstersTest_Run(request);
};
