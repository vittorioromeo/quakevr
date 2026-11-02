#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_disarmhull_view_test.qc: test 17 (vr_agrunt_test 17), an eyeshot aid: his two views of Show Disarm Volumes (notes
// 2026-10-31 19:36 and 19:38): a grunt (vr_agrunt_test_kind) with an axe, then one with the Knight's Sword, 48 ahead of
// you, frozen in its Aim (the sword held level), Show Disarm Volumes on (vr_debug_disarm 1). Once its weapon is drawn
// and still it takes a screenshot and an eyeshot (`disarmhullview: <weapon>`), then the next; ends on `agtest: test 17
// over: ...` (run.sh -QuitOn on it). Armour: as the run sets it (vr_agarmor 0 for his views). No checks but the waits'
// give-ups. Made to be cherry-picked onto an older tree for the before views (only VR_AGT_* and weapondrawnpose 30).
// ----------------------------------------------------------------------------

float vr_dhv_w;     // the weapon shown (0 the axe, 1 the Knight's Sword)
float vr_dhv_t0;    // the state's start (its give-up's clock)
float vr_dhv_still; // frames its drawn grip moved under 0.05
vector vr_dhv_last; // ... its last place
float vr_dhv_was;   // vr_debug_disarm as it was

void() VR_DHV_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_dhv_t0;
    pl.flags = pl.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agt_step == 0)
    {
        vr_dhv_was = cvar_hget(cvarh_vr_debug_disarm);
        cvar_set("vr_debug_disarm", "1");
        vr_dhv_w = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        cvar_set("vr_agrunt_freeze", "1");
        vr_agk_test_hold = VR_AGK_AIM + 1;
        vr_agt_grunt = g = VR_AGT_Spawn(vr_dhv_w ? WID_SWORD : WID_AXE, 48);
        vr_dhv_still = 0;
        vr_dhv_last = '0 0 0';
        vr_dhv_t0 = time;
        vr_agt_step = 2;
        return;
    }
    string wn = vr_dhv_w ? "Knight's Sword" : "axe";
    if(vr_agt_step == 2)
    {
        entity p = g.vr_ag_prop;
        vector o = p ? weapondrawnpose(p, 0, 30) : '0 0 0';
        vr_dhv_still = (o != '0 0 0' && vlen(o - vr_dhv_last) < 0.05) ? vr_dhv_still + 1 : 0;
        vr_dhv_last = o;
        if(vr_dhv_still < 10)
        {
            if(t > 10)
            {
                VR_AGT_Check(sprintf("disarmhullview %s: timed out (its weapon never drawn and still)", wn), FALSE);
                vr_agt_step = 4;
            }
            return;
        }
        cvar_set("vr_eyeshot", "3"); // (with the UI pass: the debug lines are drawn in it)
        localcmd("screenshot\n");
        VR_AGT_Print(sprintf("disarmhullview: %s", wn));
        vr_dhv_t0 = time;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        // (the screenshot and the eyeshot taken: two frames on)
        vr_dhv_still = vr_dhv_still + 1;
        if(vr_dhv_still < 13)
        {
            return;
        }
        vr_agt_step = 4;
    }
    if(vr_agt_step == 4)
    {
        cvar_set("vr_agrunt_freeze", "0");
        vr_agk_test_hold = 0;
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_dhv_w = vr_dhv_w + 1;
        vr_agt_step = vr_dhv_w < 2 ? 1 : 5;
        return;
    }
    cvar_set("vr_debug_disarm", ftos(vr_dhv_was));
    pl.flags = pl.flags - (pl.flags & (FL_GODMODE | FL_NOTARGET));
    vr_agt_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif
