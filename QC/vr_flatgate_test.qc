#ifdef QVR_TESTS
// vr_flatgate_test.qc -- a flat-screen walk through a teleporter (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Teleporters"),
// on `map vrteleporters` (make_vrteleporters_map.py --tests), the player's own +forward into each gate.
// `vr_flatgate_test 1`: seamless (vr_portals_walk 1): the pitch he looked at stays, the yaw turns by the gate's turn alone, he
//   goes on walking the way he looks, the body and the hand with the shots follow. Ends `flatgate: N passed, M failed`.
// `vr_flatgate_test 2`: the same gate with vr_portals_walk 0: Quake's own teleport, not seamless: the view is set to the
//   destination's, pitch level (as it ever was).
// `vr_flatgate_test 3`: on a map just changed to: the view is the level's start, pitch level, whatever he looked at before.

float vr_fgt_on;     // the running test's number
float vr_fgt_s;      // its state
float vr_fgt_case;   // which gate
float vr_fgt_t0;     // when the state began (a stuck test: FAIL "timed out")
float vr_fgt_wait;   // frames left before the state goes on
vector vr_fgt_last;  // his place last frame (a jump over 64 units is the crossing)
vector vr_fgt_look;  // his view just before it

float VR_FGT_GIVEUP = 8; // seconds a state may last
float VR_FGT_CASES = 4;

void(float s, float frames) VR_FGT_To =
{
    vr_fgt_s = s;
    vr_fgt_wait = frames;
    vr_fgt_t0 = time;
};

// The gate's start, as the map's table has it (setpos x y z pitch yaw roll); `turn`: what the gate turns him by.
vector(float xCase) VR_FGT_From =
{
    if(xCase == 0) { return '-256 576 24'; }  // flush_player
    if(xCase == 1) { return '-1280 704 24'; } // turn90
    if(xCase == 2) { return '-1459 397 24'; } // turn45
    return '-256 576 24';                     // flush_player, looking up
};

// (pitch, yaw, the gate's turn in degrees)
vector(float xCase) VR_FGT_Look =
{
    if(xCase == 0) { return '25 90 0'; }
    if(xCase == 1) { return '-20 90 90'; }
    if(xCase == 2) { return '30 225 135'; }
    return '-40 90 0';
};

float(float a, float b) VR_FGT_AngleOff =
{
    return fabs(anglemod(a - b + 180) - 180);
};

void() VR_FGT_End =
{
    localcmd("-forward\n");
    VR_Test_Verdict();
    vr_fgt_on = 0;
    cvar_set("vr_flatgate_test", "0");
};

void(entity p) VR_FGT_Walk =
{
    if(vr_fgt_s == 0)
    {
        if(vr_fgt_case >= (vr_fgt_on == 2 ? 1 : VR_FGT_CASES))
        {
            VR_FGT_End();
            return;
        }
        vector at = VR_FGT_From(vr_fgt_case);
        vector look = VR_FGT_Look(vr_fgt_case);
        localcmd(sprintf("setpos %g %g %g %g %g 0\n", at_x, at_y, at_z, look_x, look_y));
        VR_FGT_To(1, 70); // (the view set, landed, the hands settled, the lag gone: counted)
        return;
    }
    if(vr_fgt_s == 1)
    {
        vr_fgt_last = p.origin;
        vr_fgt_look = p.v_angle;
        localcmd("+forward\n");
        VR_FGT_To(2, 0);
        return;
    }
    if(vr_fgt_s == 2) // on until the jump
    {
        if(vlen(p.origin - vr_fgt_last) > 64)
        {
            VR_FGT_To(3, 6); // (the word to the client and its move back)
            return;
        }
        vr_fgt_last = p.origin;
        vr_fgt_look = p.v_angle;
        return;
    }
    if(vr_fgt_s == 3)
    {
        localcmd("-forward\n");
        vector want = VR_FGT_Look(vr_fgt_case);
        if(vr_fgt_on == 2)
        {
            VR_Test_Check(fabs(p.v_angle_x) < 1, sprintf("plain teleporter (vr_portals_walk 0): the view is set, level (pitch %.2f, was %.2f)", p.v_angle_x, vr_fgt_look_x));
        }
        else
        {
            float yawOff = VR_FGT_AngleOff(p.v_angle_y, vr_fgt_look_y);
            VR_Test_Check(fabs(p.v_angle_x - vr_fgt_look_x) < 0.5, sprintf("case %g: the pitch stays through the gate (%.2f, was %.2f)", vr_fgt_case, p.v_angle_x, vr_fgt_look_x));
            VR_Test_Check(fabs(yawOff - want_z) < 0.5, sprintf("case %g: the yaw turns by the gate's %g alone (by %.2f)", vr_fgt_case, want_z, yawOff));
            vector flat = p.velocity;
            flat_z = 0;
            float headingOff = VR_FGT_AngleOff(vectoyaw(flat), p.v_angle_y);
            VR_Test_Check(vlen(flat) > 100 && headingOff < 5, sprintf("case %g: he walks on the way he looks (speed %.0f, off by %.1f)", vr_fgt_case, vlen(flat), headingOff));
            VR_Test_Check(VR_FGT_AngleOff(p.angles_y, p.v_angle_y) < 25, sprintf("case %g: the body follows (body %.1f, view %.1f)", vr_fgt_case, p.angles_y, p.v_angle_y));
            makevectors(VRGetEntHandRot(p, cVR_MainHand));
            float aimOff = VR_FGT_AngleOff(vectoyaw(v_forward), p.v_angle_y);
            VR_Test_Check(aimOff < 12, sprintf("case %g: the hand's aim follows (off by %.1f)", vr_fgt_case, aimOff));
        }
        vr_fgt_case = vr_fgt_case + 1;
        VR_FGT_To(0, 4);
        return;
    }
};

void() VR_FlatGateTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_flatgate_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_fgt_on != ask)
    {
        VR_Test_Begin("flatgate");
        vr_fgt_on = ask;
        vr_fgt_case = 0;
        VR_FGT_To(0, 0);
        if(!VR_FlatPlay(p))
        {
            bprint("flatgate: flat play off: run with -flatscreenmode\n");
        }
    }
    if(vr_fgt_wait > 0)
    {
        vr_fgt_wait = vr_fgt_wait - 1;
        return;
    }
    if(ask == 3)
    {
        VR_Test_Check(fabs(p.v_angle_x) < 1, sprintf("a level change: the view is the level's start (pitch %.2f)", p.v_angle_x));
        VR_FGT_End();
        return;
    }
    if(time - vr_fgt_t0 > VR_FGT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: test %g, case %g, state %g", ask, vr_fgt_case, vr_fgt_s));
        VR_FGT_End();
        return;
    }
    if(ask == 1) { VR_FGT_Walk(p); }
    if(ask == 2) { VR_FGT_Walk(p); }
};

#endif
