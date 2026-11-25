#ifdef QVR_TESTS
// vr_stealth_test7.qc -- `vr_stealth_test 132` on e1m1: lift shafts kept clear (his report of 2026-11-25: monsters under
// a raised lift send it back up each time it comes down; vr_shaft, docs/vr-port/NAVMESH.md 24). The first lift (*22, the
// thin plat at 792 512 that goes 400 down to the room below), three cases in turn, the lift raised and held at its top
// while each plays out, then let go:
//   investigate: a grunt sent to a noise under the raised lift (the player rides the lift from the top, notarget): it
//     stops at the shaft's edge; the lift comes down to its bottom with no bounce;
//   fight: a knight after the player standing under the raised lift goes into the shaft (its enemy is there); the
//     player put far away out of its sight, it walks out; the lift comes down with no bounce;
//   ride: a knight below after the player at the top by the shaft waits beside the shaft (never in it while the lift is
//     up), boards the lift once it is down, rides it up.
// Prints `shafttest: <case>_<what> PASS|FAIL ...`, then `shafttest: N passed, M failed`. Each case's give-up is a
// FAIL "timed out" line (a safety net, never the end of a passing run).

float stt7_case;    // 0 investigate, 1 fight, 2 ride
float stt7_stage;
float stt7_t0;
float stt7_held;    // the lift held at its top
float stt7_prev;    // the lift's state last frame (a bounce: down, then up)
float stt7_bounces;
float stt7_entered; // the monster in the shaft while the lift was up (frames)
float stt7_wait;    // frames counted toward a stage's condition
entity stt7_plat;
entity stt7_m;
vector stt7_mid;    // the shaft's middle on the floor below (a monster's origin there)

string() VR_STT7_Name =
{
    if(stt7_case == 0) { return "investigate"; }
    if(stt7_case == 1) { return "fight"; }
    return "ride";
};

void(float xStage) VR_STT7_Stage =
{
    stt7_stage = xStage;
    stt7_t0 = time;
    stt7_wait = 0;
};

// m's box in the shaft's live part: the lift's footprint widened by vr_shaft_margin, under the lift where it is now.
float(entity m) VR_STT7_InLive =
{
    float g = cvar_hget(cvarh_vr_shaft_margin);
    vector lo = stt7_plat.absmin;
    vector hi = stt7_plat.absmax;
    float zlo = stt7_plat.pos2_z + stt7_plat.mins_z;
    return stt7_plat.absmin_z > zlo + 2 && m.absmin_x < hi_x + g && m.absmax_x > lo_x - g && m.absmin_y < hi_y + g &&
        m.absmax_y > lo_y - g && m.absmin_z < stt7_plat.absmin_z - 2 && m.absmax_z > zlo;
};

// A spot for a monster on the floor below, along the longest clear way from the shaft's middle, 260 units off at most.
vector() VR_STT7_Spot =
{
    float len = VR_STT2_ClearWay(stt7_mid, '0 0 0', 400);
    return stt7_mid + stt2_dir * min(len - 32, 260);
};

// A spot for the player at the top beside the shaft (the floor round its hole), or '0 0 0'.
vector() VR_STT7_TopSpot =
{
    vector c = (stt7_plat.absmin + stt7_plat.absmax) * 0.5;
    for(float d = 40; d <= 160; d = d + 40)
    {
        for(float a = 0; a < 360; a = a + 45)
        {
            makevectors('0 1 0' * a);
            vector at = c + v_forward * (fabs(v_forward_x) * stt7_plat.size_x * 0.5 + fabs(v_forward_y) * stt7_plat.size_y * 0.5 + d);
            at_z = stt7_plat.pos1_z + stt7_plat.maxs_z + 40;
            traceline(at, at - '0 0 60', TRUE, world);
            at_z = trace_endpos_z + 25;
            if(trace_fraction < 1 && !trace_startsolid && VR_STT2_Room(at))
            {
                return at;
            }
        }
    }
    return '0 0 0';
};

void() VR_STT7_Release =
{
    stt7_held = FALSE;
    if(stt7_plat.state == STATE_TOP)
    {
        stt7_plat.think = plat_go_down;
        stt7_plat.nextthink = stt7_plat.ltime + 0.05;
    }
};

void() VR_STT7_Next =
{
    VR_STT2_Remove(stt7_m);
    stt7_m = world;
    VR_STT7_Release();
    stt2_p.flags = stt2_p.flags | FL_NOTARGET;
    setorigin(stt2_p, stt2_phome);
    stt2_p.velocity = '0 0 0';
    stt7_case = stt7_case + 1;
    if(stt7_case > 2)
    {
        VR_Test_Verdict();
        VR_STT2_End();
        return;
    }
    VR_STT7_Stage(0);
};

void(string xWhy) VR_STT7_Fail =
{
    VR_Test_Check(FALSE, sprintf("%s_%s", VR_STT7_Name(), xWhy));
    VR_STT7_Next();
};

void() VR_STT7_Frame =
{
    entity p = stt2_p;
    float t = time - stt7_t0;
    entity os = self;
    if(stt7_plat != world)
    {
        if(stt7_held && stt7_plat.state == STATE_TOP)
        {
            stt7_plat.nextthink = stt7_plat.ltime + 999;
        }
        if(stt7_prev == STATE_DOWN && stt7_plat.state == STATE_UP)
        {
            stt7_bounces = stt7_bounces + 1;
        }
        stt7_prev = stt7_plat.state;
        if(stt7_m != world && stt7_stage >= 2 && VR_STT7_InLive(stt7_m))
        {
            stt7_entered = stt7_entered + 1;
        }
    }
    // 0: the lift found, resting at its bottom; raised.
    if(stt7_stage == 0)
    {
        stt7_plat = world;
        for(entity e = find(world, classname, "plat"); e; e = find(e, classname, "plat"))
        {
            vector c = (e.absmin + e.absmax) * 0.5;
            if(fabs(c_x - 792) < 64 && fabs(c_y - 512) < 64)
            {
                stt7_plat = e;
            }
        }
        if(stt7_plat == world)
        {
            VR_STT7_Fail("no lift");
            return;
        }
        if(stt7_plat.state != STATE_BOTTOM || stt7_plat.velocity != '0 0 0')
        {
            if(t > 20) { VR_STT7_Fail("timed out: the lift never at its bottom"); }
            return;
        }
        stt7_mid = (stt7_plat.absmin + stt7_plat.absmax) * 0.5;
        stt7_mid_z = stt7_plat.pos2_z + stt7_plat.mins_z + 26; // (the floor below: a grunt's origin there)
        self = stt7_plat;
        plat_go_up();
        self = os;
        stt7_held = TRUE;
        stt7_bounces = 0;
        stt7_entered = 0;
        stt7_prev = stt7_plat.state;
        VR_STT7_Stage(1);
        return;
    }
    // 1: at its top (held there): the case's monster and the player put down.
    if(stt7_stage == 1)
    {
        if(stt7_plat.state != STATE_TOP)
        {
            if(t > 15) { VR_STT7_Fail("timed out: the lift never at its top"); }
            return;
        }
        vector spot = VR_STT7_Spot();
        if(stt7_case == 0)
        {
            stt7_m = VR_STT2_Spawn(spot, vectoyaw(stt7_mid - spot), 0);
            // (he rides the lift from the top, unseen)
            setorigin(p, [(stt7_plat.absmin_x + stt7_plat.absmax_x) * 0.5, (stt7_plat.absmin_y + stt7_plat.absmax_y) * 0.5,
                stt7_plat.absmax_z + 25]);
        }
        else
        {
            vector at = stt7_mid;
            if(stt7_case == 2)
            {
                at = VR_STT7_TopSpot();
                if(at == '0 0 0')
                {
                    VR_STT7_Fail("no place at the top");
                    return;
                }
            }
            setorigin(p, at);
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            stt7_m = VR_STT2_Spawn(spot, vectoyaw(stt7_mid - spot), 5);
        }
        p.velocity = '0 0 0';
        if(stt7_m == world)
        {
            VR_STT7_Fail("no monster");
            return;
        }
        VR_STT7_Stage(5);
        return;
    }
    // 5: the monster started (0.6 s): sent to the noise, or after him.
    if(stt7_stage == 5)
    {
        if(t < 0.6)
        {
            return;
        }
        if(stt7_case == 0)
        {
            VR_STT2_Calm(stt7_m, stt7_m.angles_y);
            // (past the middle: its walk ends 64 short of the noise, under the lift)
            vector far = stt7_mid + normalize(stt7_mid - stt7_m.origin) * 40;
            far_z = stt7_mid_z;
            VR_Stealth_Alert(stt7_m, far, STL_N_PROP, 1, FALSE);
            bprint(sprintf("shafttest:   investigate: the noise at %v, its walk to %v\n", far, navshaftclear(stt7_m, far)));
        }
        else
        {
            stt7_m.health = 500;
            VR_Stealth_Spot(stt7_m, p);
            bprint(sprintf("shafttest:   %s: the monster after %s (his flags %g)\n", VR_STT7_Name(), stt7_m.enemy.classname, p.flags));
        }
        stt7_entered = 0;
        VR_STT7_Stage(2);
        return;
    }
    // 2: the case plays out under the held lift.
    if(stt7_stage == 2)
    {
        if(floor(t * 72) % 144 == 0)
        {
            bprint(sprintf("shafttest:   %s %.0f s: the monster at %v (state %g, phase %g), the shaft at %v\n", VR_STT7_Name(),
                t, stt7_m.origin, stt7_m.stl_state, stt7_m.stl_phase, stt7_mid));
        }
        if(stt7_case == 0)
        {
            // (its walk to the noise over and 2 s of its search: it would stand there)
            if(stt7_m.stl_state == STL_ALERT && stt7_m.stl_phase == STL_SEARCH) { stt7_wait = stt7_wait + 1; }
            if(stt7_wait < 144 && t < 25) { return; }
            VR_Test_Check(t < 25, sprintf("%s_%s", VR_STT7_Name(), sprintf("searched (%.1f s)", t)));
            VR_Test_Check(!VR_STT7_InLive(stt7_m), sprintf("%s_%s", VR_STT7_Name(), sprintf("outside the shaft (in it %g frames)", stt7_entered)));
        }
        else if(stt7_case == 1)
        {
            if(!VR_STT7_InLive(stt7_m))
            {
                if(t > 20) { VR_STT7_Fail("timed out: never went in after him"); }
                return;
            }
            VR_Test_Check(TRUE, sprintf("%s_%s", VR_STT7_Name(), sprintf("went in after him (%.1f s)", t)));
            p.flags = p.flags | FL_NOTARGET;
            setorigin(p, stt2_phome);
            p.velocity = '0 0 0';
            VR_STT7_Stage(3);
            return;
        }
        else
        {
            // (waiting beside the shaft for 1 s, by its edge)
            vector d = stt7_m.origin - stt7_mid;
            d_z = 0;
            if(vlen(d) < 160 && stt7_m.flags & FL_ONGROUND) { stt7_wait = stt7_wait + 1; }
            if(stt7_wait < 72 && t < 20) { return; }
            VR_Test_Check(t < 20, sprintf("%s_%s", VR_STT7_Name(), sprintf("waited by the shaft (%.1f s)", t)));
            VR_Test_Check(stt7_entered == 0, sprintf("%s_%s", VR_STT7_Name(), sprintf("never in the raised lift's shaft (%g frames)", stt7_entered)));
        }
        VR_STT7_Release();
        VR_STT7_Stage(4);
        return;
    }
    // 3: (fight) him gone: out of the shaft.
    if(stt7_stage == 3)
    {
        if(VR_STT7_InLive(stt7_m))
        {
            if(t > 15) { VR_STT7_Fail("timed out: still in the shaft"); }
            return;
        }
        VR_Test_Check(TRUE, sprintf("%s_%s", VR_STT7_Name(), sprintf("walked out (%.1f s)", t)));
        VR_STT7_Release();
        VR_STT7_Stage(4);
        return;
    }
    // 4: let go: down to its bottom with no bounce; (ride) the knight boards it and rides it up.
    if(stt7_stage == 4)
    {
        if(stt7_wait == 0)
        {
            if(stt7_plat.state != STATE_BOTTOM)
            {
                if(t > 20) { VR_STT7_Fail(sprintf("timed out: never at its bottom (%g bounces)", stt7_bounces)); }
                return;
            }
            stt7_wait = 1;
            VR_Test_Check(stt7_bounces == 0, sprintf("%s_%s", VR_STT7_Name(), sprintf("down with no bounce (%g, %.1f s)", stt7_bounces, t)));
            if(stt7_case != 2)
            {
                VR_STT7_Next();
            }
            return;
        }
        if(stt7_m.absmin_z < stt7_plat.pos1_z + stt7_plat.maxs_z - 4)
        {
            if(t > 40) { VR_STT7_Fail(sprintf("timed out: never rode up (%g bounces)", stt7_bounces)); }
            return;
        }
        VR_Test_Check(stt7_bounces == 0, sprintf("%s_%s", VR_STT7_Name(), sprintf("rode up (%.1f s, %g bounces, in the raised shaft %g frames)", t, stt7_bounces,
            stt7_entered)));
        VR_STT7_Next();
        return;
    }
};

void() VR_STT7_Start =
{
    stt7_case = 0;
    stt7_plat = world;
    stt7_m = world;
    VR_Test_BeginFlags("shafttest", VR_TEST_LOUD);
    VR_STT7_Stage(0);
};
#endif
