// vr_armedgrunt_twohand_test.qc -- vr_agrunt_test 92: two hands in the tug-of-war, and the eased tug of a staggering or
// downed armed grunt (vr_armedgrunt_play.qc: VR_AG_TugBegin, VR_AG_TugFrame, VR_AG_TugStrike).
// Phases, each with a fresh shotgun grunt 60 units ahead:
//   0 one hand pulls back at 0.4 m/s: the time it takes (or 5 s: kept);
//   1 both hands pull at 0.4 m/s: the second grab joins (hand2 set), and they win in at most 0.75 x the one hand's time
//     (or win where one did not; 0.85: the mock's off hand reads little stretch, the eased thresholds win it);
//   2 one hand holds still, the other's fist strikes the gun at 2 m/s (kept), then at 3.5 m/s: the weapon in the
//     holding hand;
//   3 both hold; the off hand lets go: the tug goes on on the main; it takes hold again and the main lets go: the tug
//     goes on on the off hand;
//   4 a staggering grunt grabbed: not yours at once (vr_agrunt_tug_downed_scale 0.5), held still 0.4 s it keeps it,
//     pulled at 0.6 m/s it is yours within 2 s (a standing one's: about 1.2 s at 0.4 m/s, no fight);
//   5 the same, knocked down.
// Ends on `agtest: test 92 over: P passed, F failed`.

#ifdef QVR_TESTS
float vr_ag2_phase;
float vr_ag2_one;      // phase 0's win time (s; 99: kept)
float vr_ag2_keep_drop;
float vr_ag2_keep_scale;
float vr_ag2_keep_fight;

void(string s) VR_AG2_Print = { bprint(sprintf("agtwo: %s\n", s)); };

// The off hand's mock pull (m drawn back: vr_mock_hand_pull_off) and the main's.
void(float m, float o) VR_AG2_Pull =
{
    cvar_set("vr_mock_hand_pull", ftos(m));
    cvar_set("vr_mock_hand_pull_off", ftos(o));
};

// A fresh grunt 60 ahead, your hands empty, nothing pulled.
void() VR_AG2_Fresh =
{
    entity p = vr_agt_player;
    VR_AGPT_Clear();
    VR_AGPT_EmptyHand(p);
    p.weapon2 = WID_FIST;
    VR_AG2_Pull(0, 0);
    cvar_set("vr_mock_hand_lift", "0");
    vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 60);
};

// The off hand's fist striking the held weapon `p` of `g` at `sp` m/s (as test 29's blow: the melee tracker's swing,
// the fist closed, swung the frame before too), this frame: whether it struck.
float(entity g, entity p, float sp) VR_AG2_OffStrike =
{
    entity pl = vr_agt_player;
    VR_AG_WeaponLine(p);
    pl.offhandpos = (vr_ag_wa + vr_ag_wb) * 0.5;
    makevectors(pl.angles);
    pl.offhandvel = v_forward * sp * VRMetersToUnits();
    pl.velocity = '0 0 0';
    entity was = self;
    self = pl;
    entity b = VR_Blow(cVR_OffHand);
    self = was;
    b.mh_thing = VR_MTHING_FIST;
    b.mh_time = time;
    b.mh_fast = sp;
    b.mh_vfar_last = v_forward * sp;
    pl.vrbits0 = pl.vrbits0 | QVR_VRBITS0_OFFHAND_GRABBING;
    pl.vr_ag_knock_off = 0;
    return VR_AG_WeaponStruck(g, p);
};

void() VR_AG2_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float ph = vr_ag2_phase;
    if(vr_agt_step == 0)
    {
        if(ph == 0)
        {
            vr_ag2_keep_drop = cvar("vr_agrunt_fall_drop");
            vr_ag2_keep_scale = cvar("vr_agrunt_tug_downed_scale");
            cvar_set("vr_agrunt_fall_drop", "0"); // (knocked down: it keeps it, for the tug)
            cvar_set("vr_agrunt_tug_downed_scale", "0.5");
            vr_ag2_keep_fight = cvar("vr_agrunt_tug_fight");
        }
        // (a staggering or downed one up again in its tug fights back: not while its pull is measured)
        cvar_set("vr_agrunt_tug_fight", ph >= 4 ? "0" : ftos(vr_ag2_keep_fight));
        if(ph > 5)
        {
            cvar_set("vr_agrunt_fall_drop", ftos(vr_ag2_keep_drop));
            cvar_set("vr_agrunt_tug_downed_scale", ftos(vr_ag2_keep_scale));
            cvar_set("vr_agrunt_tug_fight", ftos(vr_ag2_keep_fight));
            VR_AG2_Pull(0, 0);
            VR_AGPT_Clear();
            vr_ag2_phase = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        cvar_set("vr_agrunt_disarm_yank", "0"); // (no grab by the hands' own touches: the test's grabs)
        VR_AG2_Fresh();
        vr_agt_step = 1;
        vr_agt_until = time + 1;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 4 && vr_agt_step != 5) { return; }
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
        if(ph == 4 || ph == 5)
        {
            makevectors('0 1 0' * g.angles_y);
            VR_Knockdown_Stagger(g, v_forward * -1, 0.5, 0.95); // (as test 23)
            if(ph == 5) { VR_Knockdown_Start(g); }
        }
        if(ph < 4) { VR_AGPT_ToHand(g, p); }
        else { cvar_set("vr_mock_hand_lift", "0"); localcmd("vr_mock_hand_to main agweapon 0.5\n"); }
        if(ph == 1 || ph == 3) { localcmd("vr_mock_hand_to off agweapon 0.85\n"); }
        vr_agt_step = 3;
        vr_agt_until = time + 0.15;
        return;
    }
    if(vr_agt_step == 3)
    {
        cvar_set("vr_agrunt_disarm_yank", "1");
        VR_AG_GrabBy(g, p, cVR_MainHand);
        if((ph == 1 || ph == 3) && g.vr_ag_tug_pl == p)
        {
            VR_AG_GrabBy(g, p, cVR_OffHand);
        }
        if(ph <= 3)
        {
            VR_AGT_Check(sprintf("two %g: begun: tug %g, physical %g, second hand %g:", ph, g.vr_ag_tug_pl == p,
                g.vr_ag_tug_rag, g.vr_ag_tug_two), g.vr_ag_tug_pl == p && g.vr_ag_tug_rag && g.vr_ag_tug_two == (ph == 1 || ph == 3));
        }
        else
        {
            VR_AGT_Check(sprintf("two %g: %s grabbed: not at once: tug %g (eased %g), it holds %s:", ph,
                ph == 4 ? "staggering" : "knocked down", g.vr_ag_tug_pl == p, g.vr_ag_tug_down,
                g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), g.vr_ag_tug_pl == p && g.vr_ag_tug_down > 0 &&
                g.vr_ag_prop != world);
        }
        vr_agt_clip = time;
        vr_agt_step = 4;
        vr_agt_until = time + (ph >= 4 ? 0.4 : 0.5);
        return;
    }
    if(vr_agt_step == 4)
    {
        if(time < vr_agt_until)
        {
            if(ph >= 4 && !g.vr_ag_tug_pl)
            {
                VR_AGT_Check(sprintf("two %g: held still: kept: %s:", ph, vr_ag_tug_why), FALSE);
                vr_agt_step = 9;
                vr_agt_until = time;
            }
            return;
        }
        if(ph == 2)
        {
            float slow = VR_AG2_OffStrike(g, g.vr_ag_prop, 2);
            float hard = slow ? 0 : VR_AG2_OffStrike(g, g.vr_ag_prop, 3.5);
            VR_AGT_Check(sprintf("two 2: the other fist at 2 m/s %s, 3.5 m/s %s (%s): your main hand %s, the grunt holds %s:",
                slow ? "struck" : "kept", hard ? "struck" : "kept", vr_ag_tug_why, VR_AGT_Name(p.weapon),
                g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), !slow && hard && p.weapon == WID_SHOTGUN && !g.vr_ag_prop);
            vr_agt_step = 9;
            vr_agt_until = time + 0.5;
            return;
        }
        if(ph == 3)
        {
            p.offhand_lastreleasetime = time; // (the off hand lets go)
            vr_agt_step = 6;
            vr_agt_until = time + 0.1;
            return;
        }
        if(ph >= 4)
        {
            VR_AGT_Check(sprintf("two %g: held still 0.4 s: tug %g, it holds %s:", ph, g.vr_ag_tug_pl == p,
                g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), g.vr_ag_tug_pl == p && g.vr_ag_prop != world);
        }
        vr_agt_clip = time;
        vr_agt_step = 5;
        vr_agt_until = time + 5;
        return;
    }
    if(vr_agt_step == 5) // pulling
    {
        float rate = ph >= 4 ? 0.6 : 0.4;
        float pull = min(0.7, (time - vr_agt_clip) * rate);
        VR_AG2_Pull(pull, ph == 1 ? pull : 0);
        if(g.vr_ag_tug_pl && floor((time - vr_agt_clip) * 2) != floor((time - frametime - vr_agt_clip) * 2))
        {
            VR_AG2_Print(sprintf("two %g at %.1f s: %.0f N (joint %.0f, stretch %.0f), held %.2f s,", ph, time - vr_agt_clip,
                vr_ag_tug_force, vr_ag_tug_fjoint, vr_ag_tug_fstretch, g.vr_ag_tug_held));
            VR_AG2_Print(sprintf("    %.0f N s, progress %.2f (old: %.1f units); the off hand: joint %.0f, stretch %.2f m",
                g.vr_ag_tug_imp, g.vr_ag_tug_prog, vr_ag_tug_stretch, ragdolltug(g, p, cVR_OffHand, 1), ragdolltug(g, p, cVR_OffHand, 3)));
            VR_AG2_Print(sprintf("    rag %g, knockdown %g stagger %g, hand to grunt %.1f", g.vr_ag_tug_rag, g.vr_knockdown, g.vr_kd_stagger,
                vlen(p.handpos - g.origin)));
        }
        if(g.vr_ag_tug_pl && time < vr_agt_until) { return; }
        float won = g.vr_ag_tug_wins >= 1 && p.weapon == WID_SHOTGUN && !g.vr_ag_prop;
        float took = won ? time - vr_agt_clip : 99;
        string what = sprintf("two %g: pulled at %.1f m/s: %s after %.2f s (peak %.0f N), your hand %s:", ph, rate,
            won ? "won" : vr_ag_tug_why, time - vr_agt_clip, g.vr_ag_tug_peak, VR_AGT_Name(p.weapon));
        if(ph == 0)
        {
            vr_ag2_one = took;
            VR_AG2_Print(what);
        }
        else if(ph == 1)
        {
            VR_AGT_Check(sprintf("%s one hand's %.2f s:", what, vr_ag2_one), won && (vr_ag2_one >= 99 || took <= vr_ag2_one * 0.85));
        }
        else
        {
            VR_AGT_Check(sprintf("%s within 2 s:", what), won && took <= 2);
        }
        VR_AG2_Pull(0, 0);
        vr_agt_step = 9;
        vr_agt_until = time + 0.5;
        return;
    }
    if(vr_agt_step == 6) // phase 3: the off hand let go
    {
        VR_AGT_Check(sprintf("two 3: the off hand let go: tug %g on hand %g, second hand %g:", g.vr_ag_tug_pl == p,
            g.vr_ag_tug_hand, g.vr_ag_tug_two), g.vr_ag_tug_pl == p && g.vr_ag_tug_hand == cVR_MainHand && !g.vr_ag_tug_two);
        VR_AG_GrabBy(g, p, cVR_OffHand); // (again)
        p.mainhand_lastreleasetime = time + 0.01; // (then the main lets go)
        vr_agt_step = 7;
        vr_agt_until = time + 0.1;
        return;
    }
    if(vr_agt_step == 7)
    {
        VR_AGT_Check(sprintf("two 3: the main hand let go: tug %g on hand %g, second hand %g, it holds %s:", g.vr_ag_tug_pl == p,
            g.vr_ag_tug_hand, g.vr_ag_tug_two, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"),
            g.vr_ag_tug_pl == p && g.vr_ag_tug_hand == cVR_OffHand && !g.vr_ag_tug_two && g.vr_ag_prop != world);
        p.offhand_lastreleasetime = time + 0.01;
        vr_agt_step = 9;
        vr_agt_until = time + 0.5;
        return;
    }
    // (step 9) the next phase
    VR_AG2_Pull(0, 0);
    p.mainhand_lastreleasetime = time; // (both let go: any tug over)
    p.offhand_lastreleasetime = time;
    if(g.vr_ag_tug_pl && time < vr_agt_until + 1) { return; }
    vr_ag2_phase = vr_ag2_phase + 1;
    vr_agt_step = 0;
};
#endif
