// vr_armedgrunt_tugpunch_test.qc -- vr_agrunt_test 19: the other fist's real punch in a tug (his checklist answer
// 2026-11-01 on "Hold its gun with one hand and punch the gun with the other fist: the gun ends in the holding hand"
// {test:agrunt_tug}: "the punch seemed to go through the gun, it didn't punch it out at all"). Test 92's phase 2 strikes
// with a made-up blow record; here the mock's off hand itself, its fist closed 28 units beside the gun, glides through
// the gun's drawn middle in 0.2 s (about 5 m/s: 38 units), as his punch did. A shotgun grunt 60 ahead, your main hand holding its
// gun (a physical tug): within half a second of the punch the gun is in your main hand and the grunt holds nothing.
// Ends on `agtest: test 19 over: P passed, F failed`.

#ifdef QVR_TESTS
float vr_agtp_yank;   // vr_agrunt_disarm_yank as it was
float vr_agtp_frames; // frames since the punch began
vector vr_agtp_mid;   // the gun's drawn middle as the punch began
float vr_agtp_s0;     // the tug's IK stretch factor before the grab (1)

// `v` as the mock's "x y z".
string(vector v) VR_AGTP_Xyz = { return sprintf("%g %g %g", v_x, v_y, v_z); };

void() VR_AGTP_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    p.flags = p.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agt_step == 0)
    {
        vr_agtp_yank = cvar("vr_agrunt_disarm_yank");
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab by the hands' own touches: the test's grab)
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        p.weapon2 = WID_FIST;
        cvar_set("vr_mock_hand_pull", "0");
        cvar_set("vr_mock_hand_pull_off", "0");
        cvar_set("vr_mock_hand_lift", "0");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 60);
        vr_agt_step = 1;
        vr_agt_until = time + 1;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 5) { return; }
    if(vr_agt_step == 1)
    {
        VR_AGT_Hunt(g);
        VR_AGT_HandToHeight(p, g.vr_ag_prop.origin_z);
        vr_agt_step = 2;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 2)
    {
        vr_agtp_s0 = VR_AGK_TugStretch(g); // (no tug yet: 1)
        VR_AGPT_ToHand(g, p);
        vr_agt_step = 3;
        vr_agt_until = time + 0.15;
        return;
    }
    if(vr_agt_step == 3)
    {
        cvar_set("vr_agrunt_disarm_yank", "1");
        VR_AG_GrabBy(g, p, cVR_MainHand);
        VR_AGT_Check(sprintf("tugpunch: your main hand holds its gun: tug %g, physical %g", g.vr_ag_tug_pl == p,
            g.vr_ag_tug_rag), g.vr_ag_tug_pl == p);
        // The off fist closed 28 units beside the gun's drawn middle (your left of it, level).
        VR_AG_WeaponLine(g.vr_ag_prop);
        vr_agtp_mid = (vr_ag_wa + vr_ag_wb) * 0.5;
        makevectors('0 1 0' * p.angles_y);
        localcmd(sprintf("vr_mock_hand_glide 0\nvr_mock_hand_to off %s\nvr_mock_button off grip 1\n",
            VR_AGTP_Xyz(vr_agtp_mid - v_right * 28)));
        vr_agt_step = 4;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 4)
    {
        VR_AGT_Check(sprintf("tugpunch: held 0.3 s, the fist beside it: tug %g, second hand %g, it holds %s", g.vr_ag_tug_pl == p,
            g.vr_ag_tug_two, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), g.vr_ag_tug_pl == p && g.vr_ag_prop != world &&
            !g.vr_ag_tug_two);
        // The tug's IK stretch (vr_agrunt_tug_ik_stretch; none before the grab: step 2). The physical tug's arms are its
        // ragdoll's limbs, not the posed arms' IK: its share stays 0 here (ROUND23.md, "Tug readout, hand hulls").
        VR_AGT_Check(sprintf("tugpunch: no tug IK stretch before the grab (x%.3f)", vr_agtp_s0), vr_agtp_s0 == 1);
        VR_AGT_Print(sprintf("tugpunch: the tug's IK stretch share in the physical tug %.2f (its arms: ragdoll limbs)",
            g.vr_agk_tugw));
        string ro = VR_AG_TugReadout(g);
        VR_AGT_Print(sprintf("tugpunch: the tug readout: %s", ro));
        // The punch: through its drawn middle (as it is now) to 10 units past it, in 0.2 s.
        VR_AG_WeaponLine(g.vr_ag_prop);
        vr_agtp_mid = (vr_ag_wa + vr_ag_wb) * 0.5;
        makevectors('0 1 0' * p.angles_y);
        localcmd(sprintf("vr_mock_hand_glide 0.2\nvr_mock_hand_to off %s\n", VR_AGTP_Xyz(vr_agtp_mid + v_right * 10)));
        vr_agtp_frames = 0;
        vr_agt_step = 5;
        vr_agt_until = time + 0.5;
        return;
    }
    if(vr_agt_step == 5)
    {
        vr_agtp_frames = vr_agtp_frames + 1;
        // (the off hand's world velocity the glide's: the mock's glide reads about 0.1 m/s for the off hand at the server,
        // its positions and the melee tracker's swing right: see ROUND23.md "Punching the gun in a tug")
        makevectors('0 1 0' * p.angles_y);
        p.offhandvel = v_right * (38 / 0.2);
        if(g.vr_ag_prop && time < vr_agt_until)
        {
            return;
        }
        VR_AGT_Check(sprintf("tugpunch: the other fist's punch (%g frames): the grunt holds %s, your main hand %s, second "
            "hand %g (%s; strike: %s)", vr_agtp_frames, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing",
            VR_AGT_Name(p.weapon), g.vr_ag_tug_two, vr_ag_tug_why, vr_ag_strike_why), !g.vr_ag_prop && p.weapon == WID_SHOTGUN);
        localcmd("vr_mock_button off grip 0\nvr_mock_hand_glide 0\n");
        cvar_set("vr_agrunt_disarm_yank", ftos(vr_agtp_yank));
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        VR_AGPT_Clear();
        p.flags = p.flags - (p.flags & (FL_GODMODE | FL_NOTARGET));
        vr_agt_step = 0;
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
