#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_ragfire_test.qc (docs/vr-port/RAGFIRE.md; vr_agrunt_ragfire, a prototype):
//
// Test 236: armed humanoids firing as ragdolls. Each case spawns one 80 units ahead of you (you unseen while it is
// set up), knocks it over backwards (a topple: down vr_knockdown_time_min..max, set to 6 s here; its landings never
// cost it its weapon) and then lets it see you. Cases: a grunt's revolver, an enforcer's and a grunt's nail pistol
// (each must fire at least twice as it lies: `ragfire <case>: N shots ...: PASS`), a grunt's rocket launcher (never: a
// big, explosive gun) and a grunt's revolver with vr_agrunt_ragfire 0 (never). Each case's line gives its shots, its
// due shots that pointed within the cone, and those held back as blocked. The test's last line is agtest's
// `agtest: test 236 over: N passed, M failed` (run.sh -QuitOn on it). A case that never gets down FAILs as such.
// ----------------------------------------------------------------------------

float vr_agrf_case;
float vr_agrf_t0;
float vr_agrf_down;
float vr_agrf_on0;      // vr_agrunt_ragfire as it was
float vr_agrf_keep0;    // vr_agrunt_fall_keep_impact as it was
float vr_agrf_tmin0;    // vr_knockdown_time_min, _max as they were
float vr_agrf_tmax0;
float vr_agrf_asked0;   // the global counts at the case's start
float vr_agrf_blocked0;
vector vr_agrf_home;
float VR_AGRF_N = 5;

float(float c) VR_AGRF_Wid =
{
    if(c == 1 || c == 2) { return WID_NAILPISTOL; } // (a long gun lying muzzle down is held back: RAGFIRE.md, "Results")
    if(c == 3) { return WID_ROCKET_LAUNCHER; }
    return WID_REVOLVER;
};
float(float c) VR_AGRF_Kind = { return c == 1 ? 1 : 0; };
float(float c) VR_AGRF_Fires = { return c <= 2; }; // (3: an excluded gun; 4: the setting off)
string(float c) VR_AGRF_Name =
{
    if(c == 0) { return "grunt revolver"; }
    if(c == 1) { return "enforcer nail pistol"; }
    if(c == 2) { return "grunt nail pistol"; }
    if(c == 3) { return "grunt rocket launcher (never)"; }
    return "grunt revolver, setting off (never)";
};

void() VR_AGRF_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    p.flags = p.flags | FL_GODMODE;
    if(vr_agt_step == 0)
    {
        if(vr_agrf_case == 0)
        {
            vr_agrf_home = p.origin;
            vr_agrf_on0 = cvar_hget(cvarh_vr_agrunt_ragfire);
            vr_agrf_keep0 = cvar_hget(cvarh_vr_agrunt_fall_keep_impact);
            vr_agrf_tmin0 = cvar_hget(cvarh_vr_knockdown_time_min);
            vr_agrf_tmax0 = cvar_hget(cvarh_vr_knockdown_time_max);
            cvar_set("vr_agrunt_fall_keep_impact", "100");
            cvar_set("vr_knockdown_time_min", "6");
            cvar_set("vr_knockdown_time_max", "6");
        }
        if(vr_agrf_case >= VR_AGRF_N)
        {
            cvar_set("vr_agrunt_ragfire", ftos(vr_agrf_on0));
            cvar_set("vr_agrunt_fall_keep_impact", ftos(vr_agrf_keep0));
            cvar_set("vr_knockdown_time_min", ftos(vr_agrf_tmin0));
            cvar_set("vr_knockdown_time_max", ftos(vr_agrf_tmax0));
            cvar_set("vr_agrunt_test_kind", "0");
            p.flags = p.flags - (p.flags & (FL_NOTARGET | FL_GODMODE));
            vr_agrf_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        cvar_set("vr_agrunt_ragfire", vr_agrf_case == 4 ? "0" : "1");
        cvar_set("vr_agrunt_test_kind", ftos(VR_AGRF_Kind(vr_agrf_case)));
        p.flags = p.flags | FL_NOTARGET;
        setorigin(p, vr_agrf_home);
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(VR_AGRF_Wid(vr_agrf_case), 80);
        g.health = 400; // (a landing's impact damage never kills it)
        vr_agrf_t0 = time;
        vr_agrf_down = 0;
        vr_agt_step = 1;
        return;
    }
    string name = VR_AGRF_Name(vr_agrf_case);
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agrf_t0 + 0.3)
        {
            return; // (its arms posed and sent first)
        }
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Start(g);
        ragdollshovetopple(g, v_forward * -1, 1, FALSE, 40);
        g.enemy = p; // (it knows where you are: its AI asleep while it lies, its gun not)
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        g.vr_ag_rf_shots = 0;
        vr_agrf_asked0 = vr_ag_rf_asked;
        vr_agrf_blocked0 = vr_ag_rf_blocked;
        vr_agrf_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agrf_t0;
    if(g.vr_knockdown == 1 && isragdoll(g))
    {
        vr_agrf_down = 1;
    }
    float shots = g.vr_ag_rf_shots;
    float fires = VR_AGRF_Fires(vr_agrf_case);
    string why = "";
    if(!g.vr_ag_prop)
    {
        why = "it dropped its weapon";
    }
    else if(!vr_agrf_down && t > 1)
    {
        why = "never became a ragdoll";
    }
    if(why == "")
    {
        if(fires && shots >= 2)
        {
            why = "ok"; // (done early: two shots as it lies)
        }
        else if(vr_agrf_down && (g.vr_knockdown != 1 || g.vr_kd_rise))
        {
            why = "ok"; // (getting up: its time down over)
        }
        else if(t > 8)
        {
            why = "timed out (still down 8 s on)";
        }
        else
        {
            return;
        }
    }
    float ok = why == "ok" && (fires ? shots >= 2 : shots == 0);
    VR_AGT_Check(sprintf("ragfire %s: %g shots as a ragdoll in %.2f s (%g due within the cone, %g blocked)%s", name, shots,
        t, vr_ag_rf_asked - vr_agrf_asked0, vr_ag_rf_blocked - vr_agrf_blocked0, why == "ok" ? "" : strcat(": ", why)), ok);
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agrf_case = vr_agrf_case + 1;
    vr_agt_step = 0;
};
#endif
