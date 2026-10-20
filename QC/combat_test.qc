// combat's test harness (moved out of combat.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

void(entity xTarget) VR_TestOnly =
{
    vr_test_only = xTarget;
};

// Whether a test looking for its nearest monster skips `e` (vr_test_only set, and not it).
float(entity e) VR_TestSkips =
{
    return (vr_test_only != world && e != vr_test_only);
};

// Test aid `vr_test_kill <n>` (console; the active ragdolls' tests, Misc/quakevr/ragdoll/active_test.sh): the live
// monster nearest the player killed by a shot of his (20 damage on its last point of health), struck (n - 1) on: 0 the body, 1 the head, 2 an arm (its right
// side as he sees it), 3 a leg, on its side facing him; 5: nowhere known. Prints `test kill:`.
void() VR_TestKill_Frame =
{
    float want = cvar("vr_test_kill");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_test_kill", "0");
    entity pl = find(world, classname, "player");
    entity best = world;
    float bestD = 100000;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && e.takedamage && !VR_IsDummy(e) && !VR_TestSkips(e) &&
           vlen(e.origin - pl.origin) < bestD)
        {
            bestD = vlen(e.origin - pl.origin);
            best = e;
        }
        e = nextent(e);
    }
    if(best == world)
    {
        dprint("test kill: no live monster\n");
        return;
    }
    float part = want - 1;
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
    }
    VR_ClearHitKind();
    vr_hitpart_target = world;
    if(part < 4)
    {
        VR_SetHitKind(QVR_HITKIND_SHOT, -1, at, dir);
        vr_hitpart = part;
        vr_hitpart_target = best;
        vr_hitpart_time = time;
        vr_hitpart_pos = at;
    }
    dprint(sprintf("test kill: %s at %v struck %g at %v\n", best.classname, best.origin, part, at));
    // (A shotgun blast's worth on its last point of health: its kill push a real shot's, vr_kill_push.)
    best.health = 1;
    T_Damage(best, pl, pl, 20);
    VR_ClearHitKind();
    vr_hitpart_target = world;
};

// Test aid `vr_test_hurt <damage>` (console; earned bullet time's test, Misc/quakevr/bullettime_earn_test.sh): the live
// monster nearest the player takes <damage> from a shot of his, its health as it is (a kill when it is enough). Prints
// `test hurt:` with its health before and after.
void() VR_TestHurt_Frame =
{
    float dmg = cvar("vr_test_hurt");
    if(dmg <= 0)
    {
        return;
    }
    cvar_set("vr_test_hurt", "0");
    entity pl = find(world, classname, "player");
    entity best = world;
    float bestD = 100000;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && e.takedamage && !VR_IsDummy(e) && !VR_TestSkips(e) &&
           vlen(e.origin - pl.origin) < bestD)
        {
            bestD = vlen(e.origin - pl.origin);
            best = e;
        }
        e = nextent(e);
    }
    if(best == world)
    {
        dprint("test hurt: no live monster\n");
        return;
    }
    float before = best.health;
    VR_ClearHitKind();
    T_Damage(best, pl, pl, dmg);
    dprint(sprintf("test hurt: %s %g damage, health %g -> %g\n", best.classname, dmg, before, best.health));
};
