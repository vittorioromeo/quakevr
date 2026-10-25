#ifdef QVR_TESTS
// vr_hitreact's test harness (moved out of vr_hitreact.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// (vr_test_hit_all 1: every live monster struck so at once: the cost tests.)
void() VR_TestHit_Frame =
{
    if(vr_hr_watch != world && time < vr_hr_watch_until)
    {
        dprint(sprintf("hitwatch: %.3f %s frame %g at %v health %g enemy %s state %g\n", time, vr_hr_watch.classname,
            vr_hr_watch.frame, vr_hr_watch.origin, vr_hr_watch.health, vr_hr_watch.enemy.classname,
            vr_hr_watch.attack_state));
    }
    float want = cvar("vr_test_hit");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_test_hit", "0");
    entity pl = find(world, classname, "player");
    float all = cvar("vr_test_hit_all");
    entity best = world;
    float bestD = 100000;
    float count = 0;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && e.takedamage && !VR_IsDummy(e) && !VR_TestSkips(e))
        {
            if(all)
            {
                VR_TestHit_One(pl, e, want - 1);
                count = count + 1;
            }
            else if(vlen(e.origin - pl.origin) < bestD)
            {
                bestD = vlen(e.origin - pl.origin);
                best = e;
            }
        }
        e = nextent(e);
    }
    if(all)
    {
        dprint(sprintf("test hit: %g monsters struck\n", count));
        return;
    }
    if(best == world)
    {
        dprint("test hit: no live monster\n");
        return;
    }
    VR_TestHit_One(pl, best, want - 1);
};

void(entity pl, entity best, float part) VR_TestHit_One =
{
    vector dir = best.origin - pl.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector right = '0 0 0';
    right_x = dir_y;
    right_y = 0 - dir_x;
    float lo = best.absmin_z;
    float hi = best.absmax_z;
    vector at = (best.absmin + best.absmax) * 0.5 - dir * ((best.maxs_x - best.mins_x) * 0.35);
    at_z = lo + (hi - lo) * 0.6;
    if(part == 1)
    {
        at_z = hi - (hi - lo) * 0.08;
    }
    else if(part == 2)
    {
        at_z = lo + (hi - lo) * 0.62;
        at = at + right * ((best.maxs_y - best.mins_y) * 0.4);
    }
    else if(part == 3)
    {
        at_z = lo + (hi - lo) * 0.22;
        at = at + right * ((best.maxs_y - best.mins_y) * 0.2);
    }
    VR_ClearHitKind();
    VR_SetHitKind(QVR_HITKIND_SHOT, -1, at, dir);
    vr_hitpart = part;
    vr_hitpart_target = best;
    vr_hitpart_time = time;
    vr_hitpart_pos = at;
    if(cvar("vr_test_hit_watch") > 0 && !cvar("vr_test_hit_all"))
    {
        vr_hr_watch = best;
        vr_hr_watch_until = time + cvar("vr_test_hit_watch");
    }
    float dmg = min(cvar("vr_test_hit_damage"), best.health - 1);
    dprint(sprintf("test hit: %s at %v (health %g) struck %g at %v for %g\n", best.classname, best.origin, best.health,
        part, at, dmg));
    if(dmg > 0)
    {
        if(cvar("vr_test_hit_calm"))
        {
            // (the flinch alone: no pain frames, and nobody's blow, so it doesn't turn on him)
            best.pain_finished = time + 5;
            T_Damage(best, pl, world, dmg);
        }
        else
        {
            T_Damage(best, pl, pl, dmg);
        }
    }
    VR_ClearHitKind();
    vr_hitpart_target = world;
};
#endif
