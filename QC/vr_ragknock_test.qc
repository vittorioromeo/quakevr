// vr_ragknock_test.qc -- vr_agrunt_test 236, on vrragtriggers: ragdolls striking living monsters knock them down only
// when it convinces (vr_ragknock; vr_box3d_step.cpp ragKnockStrike, vr_kdtrig.qc VR_KdTrig_Bump; ROUND26.md,
// "ragknock"; the author, 2026-11-28: "even being hit by ragdolls with smaller forces can cause an enemy to fall down").
// Each case RK_TRIALS times on a fresh grunt (its own spot on the flat south half, every trial its own corpse's):
//   0 a light drop: a grunt knocked down standing on its head (its ragdoll falls through it from a standstill): stands
//   1 a corpse falling beside it: a grunt 40 units off, facing away, killed (no kill push): stands
//   2 a hard shove: a grunt shoved by two hands (vr_knockdown_chance 100: it goes over) into one 48 units behind it: both
//     down
//   3 a body falling from height: 0's body flung 140 units up over its head, falling back onto it: down
//   4 an ogre struck by a grunt's corpse flung at it at 380 u/s: not down
//   5 a grunt struck likewise: down (the throw still floors one of its weight)
// Each trial ends when the body lies still (its middle moving under half a unit for 36 frames) or the target is down;
// 6 s the safety net (FAIL "timed out").
//   sv_random_seed 1;map vrragtriggers;wait30;god;vr_agrunt_test 236     -QuitOn "test 236 over"
// vr_kdtrig_debug 1 shows each strike (`kdtrig: thrown on ...`), 2 the engine's measure (`ragknock: ...`).

#ifdef QVR_TESTS

float RK_TRIALS = 3;
float RK_CASES = 6;

float rk_case, rk_trial, rk_step, rk_t0, rk_still, rk_down, rk_bumped, rk_keep_chance, rk_keep_class, rk_keep_push;
float rk_downs[6];     // each case's trials that floored the target
float rk_bodydowns[6]; // ... the shoved body's (case 2)
float rk_staggers[6];
float rk_timeouts;
entity rk_tgt, rk_body;
vector rk_last;

string(float c) VR_RK_Name =
{
    if(c == 0) { return "a light drop"; }
    if(c == 1) { return "a corpse falling beside it"; }
    if(c == 2) { return "a hard shove into it"; }
    if(c == 3) { return "a body falling from height"; }
    if(c == 4) { return "an ogre struck by a flung corpse"; }
    return "a grunt struck by a flung corpse";
};

// The trial's spot: its own on the flat south half (x -1100 .. 300, rows 270 apart from y -150).
vector() VR_RK_Spot =
{
    float s = rk_case * RK_TRIALS + rk_trial;
    float col = s - 8 * floor(s / 8);
    return '1 0 0' * (-1100 + 200 * col) + '0 -1 0' * (150 + 270 * floor(s / 8)) + '0 0 24';
};

void() VR_RK_Frame =
{
    entity p = vr_agt_player;
    if(mapname != "vrragtriggers")
    {
        VR_AGT_Print(sprintf("ragknock: NOT RUN on %s: map vrragtriggers", mapname));
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    if(rk_step == 0)
    {
        if(rk_case == 0 && rk_trial == 0)
        {
            rk_keep_chance = cvar_hget(cvarh_vr_knockdown_chance);
            rk_keep_class = cvar_hget(cvarh_vr_kdtrig_test_class);
            rk_keep_push = cvar_hget(cvarh_vr_kill_push);
            rk_timeouts = 0;
            float j = 0;
            while(j < RK_CASES)
            {
                rk_downs[j] = 0;
                rk_bodydowns[j] = 0;
                rk_staggers[j] = 0;
                j = j + 1;
            }
        }
        if(rk_case >= RK_CASES)
        {
            VR_KdTrig_TestClear();
            cvar_set("vr_knockdown_chance", ftos(rk_keep_chance));
            cvar_set("vr_kdtrig_test_class", ftos(rk_keep_class));
            cvar_set("vr_kill_push", ftos(rk_keep_push));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            float n = RK_TRIALS;
            VR_AGT_Check(sprintf("%s: stands (%g of %g down, %g staggered)", VR_RK_Name(0), rk_downs[0], n, rk_staggers[0]),
                rk_downs[0] == 0);
            VR_AGT_Check(sprintf("%s: stands (%g of %g down, %g staggered)", VR_RK_Name(1), rk_downs[1], n, rk_staggers[1]),
                rk_downs[1] == 0);
            VR_AGT_Check(sprintf("%s: both down (the shoved %g, the one behind %g of %g)", VR_RK_Name(2), rk_bodydowns[2],
                rk_downs[2], n), rk_bodydowns[2] == n && rk_downs[2] == n);
            VR_AGT_Check(sprintf("%s: down (%g of %g)", VR_RK_Name(3), rk_downs[3], n), rk_downs[3] == n);
            VR_AGT_Check(sprintf("%s: not down (%g of %g down, %g staggered)", VR_RK_Name(4), rk_downs[4], n, rk_staggers[4]),
                rk_downs[4] == 0);
            // (A corpse lying any way flung off the floor now and then meets it glancing, a leg or an arm first: a stagger.)
            VR_AGT_Check(sprintf("%s: down (%g of %g, at least %g)", VR_RK_Name(5), rk_downs[5], n, n - 1), rk_downs[5] >= n - 1);
            VR_AGT_Check(sprintf("no trial timed out (%g)", rk_timeouts), rk_timeouts == 0);
            rk_case = 0; // (another run starts over)
            rk_trial = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        // (100 only for the shove, so it goes over: in the rest a hit stagger's balance would read it too, the body's gun
        // falling on its head unbalancing it for sure.)
        cvar_set("vr_knockdown_chance", rk_case == 2 ? "100" : ftos(rk_keep_chance));
        // (Beside it the corpse only falls over: no killing blow's throw, vr_kill_push.)
        cvar_set("vr_kill_push", rk_case == 1 ? "0" : ftos(rk_keep_push));
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        VR_KdTrig_TestClear();
        vector at = VR_RK_Spot();
        cvar_set("vr_kdtrig_test_class", rk_case == 4 ? "4" : "0");
        rk_tgt = VR_KdTrig_TestSpawn(p, at);
        cvar_set("vr_kdtrig_test_class", "0");
        rk_tgt.angles_y = 90; // (facing north, towards the body)
        vector off = '0 128 0';
        if(rk_case == 0 || rk_case == 3)
        {
            off = '0 0 0';
        }
        else if(rk_case == 1)
        {
            off = '40 0 0';
        }
        else if(rk_case == 2)
        {
            off = '0 48 0';
        }
        vr_kdt_spawn_grunt = TRUE;
        rk_body = VR_KdTrig_TestSpawn(p, rk_tgt.origin + off);
        vr_kdt_spawn_grunt = FALSE;
        if(rk_case == 0 || rk_case == 3)
        {
            setorigin(rk_body, rk_tgt.origin + '0 0 72'); // (it comes down on its head and stands there)
            rk_body.flags = rk_body.flags - (rk_body.flags & FL_ONGROUND);
        }
        rk_body.angles_y = rk_case == 1 ? 0 : 270; // (facing south, the way it goes; beside it, facing away: it falls back onto it)
        // The player: out of the way, or behind the body for the kill's push and the shove.
        vector pat = rk_tgt.origin + '0 200 0';
        if(rk_case == 1)
        {
            pat = rk_body.origin + '64 0 0';
        }
        else if(rk_case == 2)
        {
            pat = rk_body.origin + '0 40 0';
        }
        else if(rk_case == 4 || rk_case == 5)
        {
            pat = rk_body.origin + '-64 0 0';
        }
        setorigin(p, pat);
        p.velocity = '0 0 0';
        rk_down = 0;
        rk_bumped = 0;
        rk_still = 0;
        rk_t0 = time;
        rk_step = 1;
        return;
    }
    entity t = rk_tgt;
    entity b = rk_body;
    if(rk_step == 1) // (their AI begun: the event)
    {
        if(time < rk_t0 + 0.6)
        {
            return;
        }
        t.vr_kdt_bump = 0;
        t.vr_kdt_fired = 0;
        t.health = 1000; // (nothing in the trial kills it)
        if(rk_case == 0 || rk_case == 3)
        {
            if(!VR_Knockdown_Start(b))
            {
                VR_AGT_Print(sprintf("ragknock: %s: the body above could not be knocked down", VR_RK_Name(rk_case)));
            }
            VR_AGT_Print(sprintf("ragknock: %s: the body at %.0f over the target's feet (its top %.0f)", VR_RK_Name(rk_case),
                b.origin_z - t.origin_z, t.absmax_z - t.origin_z));
        }
        else if(rk_case == 2)
        {
            entity oldself = self;
            self = p;
            vr_bash_exhaust_dmg = 0;
            vr_bash_exhaust_push = 0;
            VR_Bash_Hit(b, b.origin, '0 -1 0', QVR_HITKIND_SHOVE, -1, 1, VR_Shove_HandsPush(TRUE));
            self = oldself;
        }
        else
        {
            T_Damage(b, p, p, b.health + 1);
        }
        rk_t0 = time;
        rk_step = (rk_case == 3 || rk_case >= 4) ? 2 : 3;
        return;
    }
    if(rk_step == 2) // (3: the body up over its head; 4, 5: the corpse made, then flung at it)
    {
        if(!isragdoll(b) || (rk_case >= 4 && time < rk_t0 + 1.2))
        {
            if(time > rk_t0 + 5)
            {
                VR_AGT_Print(sprintf("ragknock: %s: no ragdoll", VR_RK_Name(rk_case)));
                rk_step = 3;
            }
            return;
        }
        if(rk_case == 3)
        {
            ragdollfling(b, '0 0 1' * sqrt(2 * cvar_hget(cvarh_sv_gravity) * 140));
        }
        else
        {
            // From its pelvis at the target's chest at 380 u/s (its throw's arc: up by the gravity over its flight; at its
            // middle the body lying on the floor dragged its legs and met it low, a leg first, now and then).
            t.vr_kdt_bump = 0;
            vector from = ragdollpoint(b, 0, '0 0 0', TRUE);
            vector to = (t.absmin + t.absmax) * 0.5 + '0 0 1' * (0.3 * (t.absmax_z - t.absmin_z)) - from;
            float flight = vlen(to) / 380;
            ragdollfling(b, normalize(to) * 380 + '0 0 1' * (0.5 * cvar_hget(cvarh_sv_gravity) * flight));
        }
        rk_t0 = time;
        rk_step = 3;
        return;
    }
    // Watching: the target down (its strike's verdict or lying as a knockdown), the body still.
    if(t.vr_kdt_bump == 2 || (t.vr_knockdown == 1 && !t.vr_kd_stagger))
    {
        rk_down = 1;
    }
    rk_bumped = max(rk_bumped, t.vr_kdt_bump);
    float stillNow = (isragdoll(b) || b.health <= 0) && vlen(b.origin - rk_last) < 0.5;
    rk_still = stillNow ? rk_still + 1 : 0;
    rk_last = b.origin;
    float bodyDown = b.vr_knockdown == 1 && !b.vr_kd_stagger;
    float settled = time > rk_t0 + 0.5 && rk_still >= 36 && (rk_case != 2 || bodyDown || b.vr_knockdown != 1);
    float done = rk_down && (rk_case != 2 || bodyDown);
    float timedout = time > rk_t0 + 6;
    if(!done && !settled && !timedout)
    {
        return;
    }
    if(timedout && !done && !settled)
    {
        rk_timeouts = rk_timeouts + 1;
    }
    rk_downs[rk_case] = rk_downs[rk_case] + rk_down;
    rk_bodydowns[rk_case] = rk_bodydowns[rk_case] + bodyDown;
    rk_staggers[rk_case] = rk_staggers[rk_case] + (!rk_down && rk_bumped == 1);
    VR_AGT_Print(sprintf("ragknock: %s, trial %g: the target %s%s, %.2f s%s", VR_RK_Name(rk_case), rk_trial + 1,
        rk_down ? "down" : rk_bumped == 1 ? "staggered" : "stood", rk_case == 2 ? (bodyDown ? ", the shoved down" :
        ", the shoved up") : "", time - rk_t0, (done || settled) ? "" : " (timed out)"));
    rk_trial = rk_trial + 1;
    if(rk_trial >= RK_TRIALS)
    {
        rk_trial = 0;
        rk_case = rk_case + 1;
    }
    rk_step = 0;
};

#endif
