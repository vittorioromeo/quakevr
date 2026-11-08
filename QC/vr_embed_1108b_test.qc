#ifdef QVR_TESTS
// vr_embed_1108b_test.qc -- `vr_embed_test 29` (embed1108b): a gib skewered on the sword in the hand stays on the drawn blade
// while you turn and move with the stick (his note 2026-11-08, item 5; the engine's vr_held.cpp skewerFrame). Each case puts
// a gib on the blade by a stab (as test 21), then holds the stick for a while; the engine's vr_embed_probe_skew is the most the
// drawn gib was off its place on the drawn blade (cm), over vr_embed_probe_skewn frames.
// 0  a smooth turn (the main stick)
// 1  snap turns (vr_snap_turn 45)
// 2  a move forward (the off stick)
// 3  a smooth turn and a move at once
// 4  the same with vr_carry_local 0 (the gib drawn where the server has it, as before): how far it lagged (no check but that
//    the probe sees it)
// Ends `embed1108b: N passed, M failed`.

float VR_E9_MOST = 0.5;  // cm: the most a drawn gib may be off its place on the drawn blade
float VR_E9_HOLD = 0.75; // s of the stick held (54 frames)
float vr_e9_snap;        // vr_snap_turn as the test began
entity vr_e9_g;
float vr_e9_yaw;         // the player's yaw as the stick went on
vector vr_e9_at;         // and its place

void() VR_E9_Begin =
{
    VR_Test_BeginFlags("embed1108b", VR_TEST_LOUD);
    vr_e9_snap = cvar_hget(cvarh_vr_snap_turn);
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    cvar_set("vr_embed_probe", "on");
    vr_et_on = 29;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_E9_Next =
{
    localcmd("vr_mock_stick main 0 0\nvr_mock_stick off 0 0\n");
    cvar_set("vr_carry_local", "1");
    cvar_set("vr_snap_turn", ftos(vr_e9_snap));
    VR_EH_NextCase();
};

void() VR_E9_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_case > 4)
    {
        cvar_set("vr_embed_probe", "");
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        localcmd("+grabmain\n");
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand) && t > 0.1)
    {
        VR_EH_Give(WID_SWORD, 0);
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.3)
    {
        vr_e9_g = VR_ETS_Gib(p, VR_ETS_OnLine(0));
        VR_ETS_PutOn(vr_e9_g, 0);
        VR_ETS_Blow(p, vr_e9_g, VR_MKIND_STAB, 6);
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.2)
    {
        VR_Test_Check(vr_e9_g.vr_skewer_p == p, sprintf("case %g: a gib on the blade in the hand", vr_et_case));
        if(vr_e9_g.vr_skewer_p != p)
        {
            VR_E9_Next();
            return;
        }
        if(vr_et_case == 1)
        {
            cvar_set("vr_snap_turn", "45");
        }
        if(vr_et_case == 4)
        {
            cvar_set("vr_carry_local", "0");
        }
        float turn = vr_et_case != 2;
        float walk = vr_et_case >= 2;
        localcmd(sprintf("vr_mock_stick main %g 0\nvr_mock_stick off 0 %g\n", turn, walk));
        cvar_set("vr_embed_probe_skew", "-1");
        cvar_set("vr_embed_probe_skewn", "0");
        vr_e9_yaw = p.angles_y;
        vr_e9_at = p.origin;
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && vr_e9_g.vr_skewer_p == p && t > VR_E9_HOLD)
    {
        float off = cvar_hget(cvarh_vr_embed_probe_skew);
        float n = cvar_hget(cvarh_vr_embed_probe_skewn);
        float turned = fabs(anglemod(p.angles_y - vr_e9_yaw + 180) - 180);
        float moved = vlen(p.origin - vr_e9_at);
        string what = sprintf("(turned %.0f deg, moved %.0f units; %g frames; at most %.2f cm off)", turned, moved, n, off);
        if(vr_et_case == 4)
        {
            VR_Test_Check(n > 10 && off > VR_E9_MOST, sprintf("vr_carry_local 0: the gib lags the drawn blade %s", what));
        }
        else
        {
            float did = vr_et_case == 2 ? moved > 32 : turned > 30;
            VR_Test_Check(did && n > 10 && off >= 0 && off <= VR_E9_MOST,
                sprintf("case %g (%s): the drawn gib stays on the drawn blade %s", vr_et_case,
                vr_et_case == 0 ? "smooth turn" : (vr_et_case == 1 ? "snap turns" : (vr_et_case == 2 ? "a move" : "a turn and a move")), what));
        }
        VR_E9_Next();
    }
    else if(vr_et_s == 4 && vr_e9_g.vr_skewer_p != p)
    {
        VR_Test_Check(FALSE, sprintf("case %g: the gib came off the blade while turning or moving", vr_et_case));
        VR_E9_Next();
    }
};
#endif
