#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_disarmviz_test.qc: test 97 (vr_agrunt_test 97), Show Disarm Volumes (vr_debug_disarm; vr_disarmviz.qc) in each
// state, its eye images taken (vr_eyeshot): a shotgun grunt 60 ahead of you (unseen) standing ("alive"), your main hand
// put on its weapon ("hover": the engine prints `disarmviz: main hand takes weapon`), the grip pressed ("tug": a tug
// begun), let go, then shoved into a stagger: an active ragdoll still holding it, a pellet's line put through its
// weapon by the shot-disarm code (VR_AGS_Shot: the hit marked) ("ragdoll"). Checks: its weapon held, the tug, the
// ragdoll with its weapon, the shot counted; then `agtest: test 97 over: ...` (run.sh -QuitOn on it). Each wait is the
// state reached (or its give-up, a FAIL).
// ----------------------------------------------------------------------------

float vr_dvt_t0;
float vr_dvt_was;   // vr_debug_disarm as it was
vector vr_dvt_home;

void(string what) VR_DVT_Shot =
{
    cvar_set("vr_eyeshot", "3"); // (with the UI pass: the debug lines are drawn in it)
    if(!cvar("vr_enabled"))
    {
        localcmd("screenshot\n"); // (the flat view: its overlay's lines)
    }
    VR_AGT_Print(sprintf("disarmvizshot: %s", what));
};

void() VR_DVT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_dvt_t0;
    if(vr_agt_step == 0)
    {
        vr_dvt_was = cvar("vr_debug_disarm");
        cvar_set("vr_debug_disarm", "2");
        p.flags = p.flags | FL_NOTARGET;
        vr_dvt_home = p.origin;
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 60);
        g.health = 400;
        vr_dvt_t0 = time;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(t < 0.4)
        {
            return; // (its arms posed and sent first)
        }
        VR_AGT_Check("disarmviz: it holds its weapon", g.vr_ag_prop != world);
        VR_DVT_Shot("alive");
        vr_agt_step = 2;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 2)
    {
        localcmd("vr_mock_hand_to main agweaponpalm 0.5 0 either\n");
        vr_agt_step = 3;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(t < 0.25)
        {
            return; // (the hand there: a few frames)
        }
        VR_DVT_Shot("hover");
        localcmd("+grabmain\n");
        p.mainhand_lastgrabtime = time; // (the grip pressed now: VR_HandGrabUtil_StartedHandGrabbing)
        p.mainhand_lastreleasetime = time - 1;
        vr_agt_step = 4;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 4)
    {
        if(g.vr_ag_tug_pl != p && t < 1)
        {
            return;
        }
        VR_AGT_Check("disarmviz: a tug begun", g.vr_ag_tug_pl == p);
        vr_agt_step = 5;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 5)
    {
        if(t < 0.3 && g.vr_ag_tug_pl == p)
        {
            return;
        }
        VR_DVT_Shot("tug");
        vr_agt_step = 6;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 6)
    {
        localcmd("-grabmain\n");
        if(g.vr_ag_tug_pl == p && t < 1)
        {
            return;
        }
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Stagger(g, v_forward * -1, 1, 1);
        vr_agt_step = 7;
        vr_dvt_t0 = time;
        return;
    }
    if(vr_agt_step == 7)
    {
        if(!(isragdoll(g) && g.vr_knockdown) && t < 1)
        {
            return;
        }
        if(t < 0.3)
        {
            return; // (its ragdoll a few frames on)
        }
        VR_AGT_Check("disarmviz: an active ragdoll holding its weapon", isragdoll(g) && g.vr_ag_prop != world);
        if(g.vr_ag_prop)
        {
            VR_AG_WeaponLine(g.vr_ag_prop);
            vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
            vector from = p.origin + p.view_ofs;
            float n = vr_ags_hits_w + vr_ags_hits_h;
            float nw = vr_ags_hits_w;
            VR_AGS_Shot(p, from, mid + normalize(mid - from) * 16, normalize(mid - from), 1, QVR_AGS_PELLET);
            VR_AGT_Check("disarmviz: a pellet through its weapon counted", vr_ags_hits_w + vr_ags_hits_h > n);
            // (On its weapon, not its hand: the ragdoll's forearm laid along its limb, VR_AGS_HandLine.)
            VR_AGS_HandLine(g, g.vr_ag_prop);
            VR_AGT_Check(sprintf("disarmviz: ... on its weapon (%g), the forearm %.1f units from its weapon's middle",
                vr_ags_hits_w - nw, VR_AG_SegDist(mid, mid, vr_ags_ha, vr_ags_hb)), vr_ags_hits_w > nw);
        }
        VR_DVT_Shot("ragdoll");
        vr_agt_step = 8;
        vr_dvt_t0 = time;
        return;
    }
    if(t < 0.1)
    {
        return; // (the eye images taken)
    }
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    cvar_set("vr_debug_disarm", ftos(vr_dvt_was));
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    vr_agt_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif
