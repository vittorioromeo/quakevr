#ifdef QVR_TESTS
// Test 90 (`vr_agrunt_test 90`; ROUND23.md, "The fists' guard in game as the Pose Studio shows it"): an unarmed enforcer's
// guard as drawn running at you against its Aim (Guard) key as the Pose Studio / Rig Studio's Pose Keys tool holds it.
// - The studio's view: the enforcer frozen (vr_agrunt_freeze, its stand frame) and held in Aim as the studio holds it
//   (vr_agk_test_hold: VR_AGK_Hold, the studio's own path), its drawn hands (their middles) less its head's middle
//   (weapondrawnpose(e, side, 29): the client's solve, model space) once still (20 frames moving under 0.02).
// - The game's: let go, put back 600 ahead and hunting you (god): each frame it runs at you (its run frames, its run's
//   share and its guard's whole, no punch, 100+ away) its drawn hands less its head, over 72 frames (a run's cycle and
//   more).
// Checks: each hand's mean within 2 units of the studio's (about 1.3 left: the run frames' own hands carried, the
// anchor), each frame's within 3 (the run's bob, the head's own bob), the right hand right of the left (no crossing). The run ends on `agtest: test 90 over: <p> passed, <f> failed`.

float vr_aggd_step;
float vr_aggd_at;
float vr_aggd_still;  // frames the studio's hands moved under 0.02
vector vr_aggd_lastR;
vector vr_aggd_lastL;
vector vr_aggd_edR;   // the studio's hands less the head
vector vr_aggd_edL;
float vr_aggd_n;      // the game's samples
vector vr_aggd_sumR;
vector vr_aggd_sumL;
float vr_aggd_worst;  // the farthest a sample's hand was from the studio's
float vr_aggd_cross;  // samples with the right hand left of the left
float VR_AGGD_N = 72;

void(float okEnd) VR_AGGD_End =
{
    entity g = vr_agt_grunt;
    if(g && g.classname != "") { VR_AGT_Remove(g); }
    cvar_set("vr_agrunt_freeze", "0");
    vr_agk_test_hold = 0;
    cvar_set("vr_agrunt_test_kind", "0");
    vr_agt_player.flags = vr_agt_player.flags - (vr_agt_player.flags & FL_GODMODE);
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGGD_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    pl.flags = pl.flags | FL_GODMODE;
    if(vr_aggd_step == 0)
    {
        cvar_set("vr_agrunt_test_kind", "1"); // (the enforcer)
        cvar_set("vr_agrunt_freeze", "1");
        vr_agk_test_hold = VR_AGK_AIM + 1;
        vr_agt_grunt = VR_AGT_Spawn(-1, 160);
        vr_aggd_at = time;
        vr_aggd_step = 1;
        return;
    }
    if(!g || g.health <= 0)
    {
        VR_AGT_Check("fists' guard: its enforcer gone", FALSE);
        VR_AGGD_End(FALSE);
        return;
    }
    vector r = weapondrawnpose(g, 0, 29);
    vector l = weapondrawnpose(g, 1, 29);
    float drawn = vlen(r) > 0.001 && vlen(l) > 0.001;
    if(vr_aggd_step == 1)
    {
        // The studio's view: held in Aim, still.
        if(drawn && vlen(r - vr_aggd_lastR) < 0.02 && vlen(l - vr_aggd_lastL) < 0.02)
        {
            vr_aggd_still = vr_aggd_still + 1;
        }
        else
        {
            vr_aggd_still = 0;
        }
        vr_aggd_lastR = r;
        vr_aggd_lastL = l;
        if(vr_aggd_still >= 20)
        {
            vr_aggd_edR = r;
            vr_aggd_edL = l;
            VR_AGT_Print(sprintf("fists' guard: the studio's (Aim, held): right %v left %v (less the head, x fw y left z up)",
                r, l));
            cvar_set("vr_agrunt_freeze", "0");
            vr_agk_test_hold = 0;
            makevectors(pl.angles);
            vector fw = v_forward;
            fw_z = 0;
            fw = normalize(fw);
            traceline(pl.origin, pl.origin + fw * 600, TRUE, pl);
            vector at = trace_endpos - fw * 24;
            at_z = g.origin_z;
            setorigin(g, at);
            g.angles_y = vectoyaw(pl.origin - at);
            VR_AGT_Hunt(g);
            vr_aggd_at = time;
            vr_aggd_step = 2;
        }
        else if(time - vr_aggd_at > 10)
        {
            VR_AGT_Check("fists' guard: timed out (the studio's view never still)", FALSE);
            VR_AGGD_End(FALSE);
        }
        return;
    }
    // The game's: running at you in its guard.
    VR_AM_Row(g.vr_ag_kind);
    vector to = pl.origin - g.origin;
    to_z = 0;
    float running = g.frame >= vr_am_f_run && g.frame < vr_am_f_run + 8;
    if(drawn && running && g.vr_agp_runw > 0.95 && g.vr_ag_readyw > 0.74 && !g.vr_ag_act && vlen(to) > 100)
    {
        vr_aggd_n = vr_aggd_n + 1;
        vr_aggd_sumR = vr_aggd_sumR + r;
        vr_aggd_sumL = vr_aggd_sumL + l;
        vr_aggd_worst = max(vr_aggd_worst, max(vlen(r - vr_aggd_edR), vlen(l - vr_aggd_edL)));
        vr_aggd_cross = vr_aggd_cross + (r_y >= l_y);
    }
    if(vr_aggd_n >= VR_AGGD_N || vlen(to) <= 100)
    {
        float n = max(1, vr_aggd_n);
        vector mr = vr_aggd_sumR * (1 / n);
        vector ml = vr_aggd_sumL * (1 / n);
        VR_AGT_Print(sprintf("fists' guard: the game's (running at you, %g frames): right %v left %v; off the studio's "
            "%.2f, %.2f (worst frame %.2f)", vr_aggd_n, mr, ml, vlen(mr - vr_aggd_edR), vlen(ml - vr_aggd_edL),
            vr_aggd_worst));
        VR_AGT_Check(sprintf("fists' guard: sampled running at you (%g of %g frames)", vr_aggd_n, VR_AGGD_N),
            vr_aggd_n >= VR_AGGD_N);
        VR_AGT_Check(sprintf("fists' guard: right hand's mean within 2 of the studio's (%.2f)", vlen(mr - vr_aggd_edR)),
            vr_aggd_n > 0 && vlen(mr - vr_aggd_edR) < 2);
        VR_AGT_Check(sprintf("fists' guard: left hand's mean within 2 of the studio's (%.2f)", vlen(ml - vr_aggd_edL)),
            vr_aggd_n > 0 && vlen(ml - vr_aggd_edL) < 2);
        VR_AGT_Check(sprintf("fists' guard: every frame within 3 of the studio's (worst %.2f)", vr_aggd_worst),
            vr_aggd_n > 0 && vr_aggd_worst < 3);
        VR_AGT_Check(sprintf("fists' guard: hands never crossed (%g frames)", vr_aggd_cross), vr_aggd_n > 0 && vr_aggd_cross == 0);
        VR_AGGD_End(TRUE);
        return;
    }
    if(time - vr_aggd_at > 15)
    {
        VR_AGT_Check(sprintf("fists' guard: timed out (%g frames running at you)", vr_aggd_n), FALSE);
        VR_AGGD_End(FALSE);
    }
};
#endif
