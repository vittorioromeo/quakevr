#ifdef QVR_TESTS
// Test 89 (`vr_agrunt_test 89`; ROUND23.md, "Armed humanoids shove up close, not backpedal"): a gun's holder and you
// up close (vr_agrunt_shove_close, VR_AG_ShoveCloseIn). Trials, each a fresh one hunting you (god), you standing still
// (its own state machine's end each):
// - in its reach (`reach - 10` apart) and just beyond it (`reach + close * 0.6`): it shoves (its Shove pose, VR_AGP_SHOVE)
//   within 4 s, never backing off before it (VR_AG_RangeMove's steps, vr_ag_range_backs); the step-ins counted;
// - in the band beyond that (halfway from `reach + close` to its least distance, vr_ag_rmin): it backs off (24 units
//   further off within 4 s) and never shoves;
// - far (420): in 1.5 s no step-ins and no shove (as before).
// A shotgun, a nail pistol, a nailgun, an enforcer's laser rifle in turn. Checks: shoves in most of the near trials
// (75%), backs off in most band trials (75%) and none shoves, the far ones all as before
// (`agtest: shove close: ...: PASS|FAIL`). The run ends on `agtest: test 89 over: <p> passed, <f> failed`.

float vr_agsc_trial;
float vr_agsc_step;
float vr_agsc_at;
float vr_agsc_d0;       // the trial's distance (flat, between the middles)
float vr_agsc_backs0;   // vr_ag_range_backs, vr_ag_close_steps at its start
float vr_agsc_steps0;
float vr_agsc_band;     // the trial's band: 0 in reach, 1 just beyond, 2 the backing-off band, 3 far
vector vr_agsc_home;
float vr_agsc_yaw;
float vr_agsc_near_n;   // near trials (bands 0, 1), shoved without backing off first
float vr_agsc_near_ok;
float vr_agsc_band_n;   // band 2 trials, backed off, shoved
float vr_agsc_band_ok;
float vr_agsc_band_shoved;
float vr_agsc_far_n;    // far trials, as before
float vr_agsc_far_ok;
float vr_agsc_stepins;  // step-ins over the near trials
float VR_AGSC_N = 16;   // (4 bands x 4 weapons)

float(float t) VR_AGSC_Wid =
{
    float w = t & 3;
    if(w == 0) { return WID_SHOTGUN; }
    if(w == 1) { return WID_NAILPISTOL; }
    if(w == 2) { return WID_NAILGUN; }
    return WID_ENFRIFLE;
};

// (flat, between the middles)
float(entity a, entity b) VR_AGSC_Dist =
{
    vector to = a.origin - b.origin;
    to_z = 0;
    return vlen(to);
};

void() VR_AGSC_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    pl.flags = pl.flags | FL_GODMODE;
    if(vr_agsc_trial == 0 && vr_agsc_step == 0 && !vr_agsc_yaw)
    {
        vr_agsc_home = pl.origin;
        vr_agsc_yaw = pl.angles_y + 0.001;
    }
    if(vr_agsc_trial >= VR_AGSC_N)
    {
        float nearOk = vr_agsc_near_ok >= 0.75 * vr_agsc_near_n;
        VR_AGT_Check(sprintf("shove close: near (in reach, just beyond): %g of %g shoved without backing off first (%g "
            "step-ins):", vr_agsc_near_ok, vr_agsc_near_n, vr_agsc_stepins), vr_agsc_near_n > 0 && nearOk);
        VR_AGT_Check(sprintf("shove close: band (beyond the step-in, inside its least distance): %g of %g backed off, %g "
            "shoved:", vr_agsc_band_ok, vr_agsc_band_n, vr_agsc_band_shoved),
            vr_agsc_band_n > 0 && vr_agsc_band_ok >= 0.75 * vr_agsc_band_n && vr_agsc_band_shoved == 0);
        VR_AGT_Check(sprintf("shove close: far: %g of %g as before (no step-ins, no shove):", vr_agsc_far_ok, vr_agsc_far_n),
            vr_agsc_far_n > 0 && vr_agsc_far_ok == vr_agsc_far_n);
        if(g && g.classname != "") { VR_AGT_Remove(g); }
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_test_kind", "0");
        pl.flags = pl.flags - (pl.flags & FL_GODMODE);
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    float wid = VR_AGSC_Wid(vr_agsc_trial);
    vr_agsc_band = floor(vr_agsc_trial / 4);
    if(vr_agsc_step == 0)
    {
        if(g && g.classname != "" && g.health > 0) { VR_AGT_Remove(g); }
        VR_AGT_ClearWeapons();
        pl.angles_y = vr_agsc_yaw;
        pl.v_angle_y = vr_agsc_yaw;
        pl.fixangle = TRUE;
        setorigin(pl, vr_agsc_home);
        pl.velocity = '0 0 0';
        pl.vr_eshove_speed = 0;
        cvar_set("vr_agrunt_test_kind", wid == WID_ENFRIFLE ? "1" : "0");
        // Its distance: from its reach (VR_EnemyShove_Reach) and its least (VR_AG_Ranges, you its enemy).
        vr_agt_grunt = VR_AGT_Spawn(wid, 300);
        g = vr_agt_grunt;
        g.enemy = pl;
        float reach = VR_EnemyShove_Reach(g);
        float close = cvar("vr_agrunt_shove_close");
        VR_AG_Ranges(g);
        float d = 420;
        if(vr_agsc_band == 0) { d = reach - 10; }
        else if(vr_agsc_band == 1) { d = reach + close * 0.6; }
        else if(vr_agsc_band == 2) { d = (reach + close + vr_ag_rmin) * 0.5; }
        makevectors([0, vr_agsc_yaw, 0]);
        vector at = vr_agsc_home + v_forward * d;
        at_z = g.origin_z;
        setorigin(g, at);
        g.angles_y = vectoyaw(pl.origin - g.origin);
        VR_AGT_Hunt(g);
        vr_agsc_d0 = VR_AGSC_Dist(g, pl);
        vr_agsc_backs0 = vr_ag_range_backs;
        vr_agsc_steps0 = vr_ag_close_steps;
        vr_agsc_at = time;
        vr_agsc_step = 1;
        VR_AGT_Print(sprintf("shove close: trial %g: %s, band %g, %.0f units apart (reach %.0f, step-in to %.0f, its least "
            "%.0f)", vr_agsc_trial, VR_AGT_Name(wid), vr_agsc_band, vr_agsc_d0, reach, reach + close, vr_ag_rmin));
        return;
    }
    float t = time - vr_agsc_at;
    float dNow = VR_AGSC_Dist(g, pl);
    float shoved = g.vr_ag_act == VR_AGP_SHOVE;
    float backs = vr_ag_range_backs - vr_agsc_backs0;
    float ins = vr_ag_close_steps - vr_agsc_steps0;
    float done = FALSE;
    string what = "";
    if(!g || g.health <= 0)
    {
        what = "its monster gone";
        done = TRUE;
    }
    else if(vr_agsc_band <= 1)
    {
        if(shoved || t > 4)
        {
            vr_agsc_near_n = vr_agsc_near_n + 1;
            float ok = shoved && backs == 0;
            vr_agsc_near_ok = vr_agsc_near_ok + ok;
            vr_agsc_stepins = vr_agsc_stepins + ins;
            what = sprintf("%s after %.2f s, %g backing steps, %g step-ins, %.0f apart", shoved ? "shoved" : "no shove",
                t, backs, ins, dNow);
            done = TRUE;
        }
    }
    else if(vr_agsc_band == 2)
    {
        if(shoved || dNow > vr_agsc_d0 + 24 || t > 4)
        {
            vr_agsc_band_n = vr_agsc_band_n + 1;
            vr_agsc_band_shoved = vr_agsc_band_shoved + shoved;
            vr_agsc_band_ok = vr_agsc_band_ok + (!shoved && dNow > vr_agsc_d0 + 24);
            what = sprintf("%s after %.2f s, %g backing steps, %g step-ins, %.0f apart", shoved ? "shoved" :
                (dNow > vr_agsc_d0 + 24 ? "backed off" : "stayed"), t, backs, ins, dNow);
            done = TRUE;
        }
    }
    else
    {
        if(shoved || ins > 0 || t > 1.5)
        {
            vr_agsc_far_n = vr_agsc_far_n + 1;
            vr_agsc_far_ok = vr_agsc_far_ok + (!shoved && ins == 0);
            what = sprintf("%s after %.2f s, %g step-ins, %.0f apart", shoved ? "shoved" : "as before", t, ins, dNow);
            done = TRUE;
        }
    }
    if(done)
    {
        VR_AGT_Print(sprintf("shove close: trial %g: %s", vr_agsc_trial, what));
        vr_agsc_trial = vr_agsc_trial + 1;
        vr_agsc_step = 0;
    }
};
#endif
